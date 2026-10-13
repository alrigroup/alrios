#!/usr/bin/env python3
"""Generate and strictly verify deterministic SPDX and SLSA release metadata."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import secrets
import stat
import subprocess
import sys
from pathlib import Path
from typing import Any, Iterable
from urllib.parse import urlsplit

SPDX_VERSION = "SPDX-2.3"
SPDX_DATA_LICENSE = "CC0-1.0"
PROVENANCE_TYPE = "https://in-toto.io/Statement/v1"
PROVENANCE_PREDICATE = "https://slsa.dev/provenance/v1"
DEFAULT_BUILDER_ID = "https://alrigroup.com/alrios/reproducible-build/v1"
DEFAULT_BUILD_TYPE_URI = "https://alrigroup.com/alrios/build/reproducible/v1"
DEFAULT_SOURCE_URI = "git+https://github.com/alrigroup/alrios.git"
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
SHA1_RE = re.compile(r"^[0-9a-f]{40}$")
REVISION_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._+:-]{0,127}$")
SPDX_ID_RE = re.compile(r"^SPDXRef-[A-Za-z0-9.-]+$")
WINDOWS_PATH_RE = re.compile(r"(?i)(?:^|[\s'\"=(])(?:[a-z]:[\\/]|\\\\[^\\\s]+\\[^\\\s]+)")
POSIX_PATH_RE = re.compile(r"(?:^|[\s'\"=(])/(?!/)[^\s,;]+")


class MetadataError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise MetadataError(message)


def exact_keys(value: Any, keys: set[str], label: str) -> dict[str, Any]:
    require(isinstance(value, dict), f"{label} must be an object")
    require(set(value) == keys, f"{label} has missing or unsupported fields")
    return value


def hash_bytes(data: bytes, algorithm: str = "sha256") -> str:
    digest = hashlib.new(algorithm)
    digest.update(data)
    return digest.hexdigest()


def hash_file(path: Path, algorithm: str = "sha256") -> str:
    digest = hashlib.new(algorithm)
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonical_json(data: Any) -> bytes:
    return (json.dumps(data, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def safe_output_path(value: str, label: str) -> Path:
    path = Path(value)
    require(path.name not in ("", ".", ".."), f"{label} has no filename")
    parent = path.parent.resolve(strict=True)
    parent_stat = os.stat(parent, follow_symlinks=False)
    require(stat.S_ISDIR(parent_stat.st_mode), f"{label} parent is not a directory")
    require(parent_stat.st_uid == os.geteuid(), f"{label} parent must be owned by the current user")
    require(not (parent_stat.st_mode & 0o022), f"{label} parent must not be group/world writable")
    require(not path.exists() and not path.is_symlink(), f"{label} destination already exists")
    return parent / path.name


def write_exclusive(path: Path, data: bytes, epoch: int) -> None:
    parent = path.parent
    before = os.stat(parent, follow_symlinks=False)
    temporary_path = parent / f".{path.name}.{secrets.token_hex(16)}"
    descriptor = os.open(temporary_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0), 0o600)
    published = False
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temporary_path, 0o644, follow_symlinks=False)
        os.utime(temporary_path, (epoch, epoch), follow_symlinks=False)
        current = os.stat(parent, follow_symlinks=False)
        require((before.st_dev, before.st_ino) == (current.st_dev, current.st_ino), "output directory was replaced")
        os.link(temporary_path, path, follow_symlinks=False)
        published = True
        after = os.stat(parent, follow_symlinks=False)
        require((before.st_dev, before.st_ino) == (after.st_dev, after.st_ino), "output directory was replaced during publication")
        os.unlink(temporary_path)
    except BaseException:
        try:
            temporary_path.unlink()
        except FileNotFoundError:
            pass
        if published:
            try:
                path.unlink()
            except FileNotFoundError:
                pass
        raise


def load_json(path: Path) -> Any:
    def no_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            require(key not in result, f"duplicate JSON key: {key}")
            result[key] = value
        return result

    try:
        with path.open("r", encoding="utf-8") as stream:
            return json.load(stream, object_pairs_hook=no_duplicate_keys)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise MetadataError(f"cannot read valid JSON from {path.name}: {error}") from error


def resolve_existing_file(value: str, label: str) -> Path:
    path = Path(value)
    try:
        resolved = path.resolve(strict=True)
    except OSError as error:
        raise MetadataError(f"{label} is not accessible: {value}") from error
    require(resolved.is_file() and not path.is_symlink(), f"{label} must be a regular non-symlink file")
    return resolved


def resolve_source_root(value: str) -> Path:
    try:
        root = Path(value).resolve(strict=True)
    except OSError as error:
        raise MetadataError(f"source root is not accessible: {value}") from error
    require(root.is_dir() and (root / ".git").exists(), "source root must be a Git working tree")
    return root


def validate_revision(value: str) -> str:
    require(bool(REVISION_RE.fullmatch(value)), "revision must be a full lowercase hexadecimal Git object ID")
    return value


def validate_epoch(value: str) -> int:
    require(value.isascii() and value.isdecimal(), "SOURCE_DATE_EPOCH must be a non-negative integer")
    epoch = int(value)
    require(epoch <= 253402300799, "SOURCE_DATE_EPOCH is outside the supported UTC range")
    return epoch


def contains_local_path(value: str) -> bool:
    return bool(POSIX_PATH_RE.search(value) or WINDOWS_PATH_RE.search(value))


def safe_metadata(value: str, label: str) -> str:
    require(bool(value) and len(value) <= 512, f"{label} has invalid length")
    require(not any(ord(character) < 0x20 for character in value), f"{label} contains control characters")
    require(not contains_local_path(value), f"{label} contains a local filesystem path")
    return value


def validate_uri(value: str, label: str, schemes: set[str]) -> str:
    safe_metadata(value, label)
    parsed = urlsplit(value)
    require(parsed.scheme in schemes and bool(parsed.netloc), f"{label} is not an allowed absolute URI")
    require(parsed.username is None and parsed.password is None, f"{label} must not contain credentials")
    require(not parsed.query and not parsed.fragment, f"{label} must not contain query or fragment data")
    return value


def unique_name_values(values: Iterable[str], label: str, preserve_order: bool = False) -> list[tuple[str, str]]:
    result: list[tuple[str, str]] = []
    names: set[str] = set()
    for value in values:
        name, separator, item_value = value.partition("=")
        require(bool(separator) and bool(NAME_RE.fullmatch(name)), f"{label} must use NAME=VALUE")
        require(name not in names, f"duplicate {label} name: {name}")
        names.add(name)
        result.append((name, safe_metadata(item_value, f"{label} value")))
    return result if preserve_order else sorted(result)


def unique_names(values: Iterable[str], label: str) -> list[str]:
    result: list[str] = []
    seen: set[str] = set()
    for value in values:
        require(bool(NAME_RE.fullmatch(value)), f"invalid {label}: {value}")
        require(value not in seen, f"duplicate {label}: {value}")
        seen.add(value)
        result.append(value)
    require(bool(result), f"at least one {label} is required")
    return result


def git_output(root: Path, arguments: list[str], text: bool = True) -> str | bytes:
    try:
        process = subprocess.run(
            ["git", "-C", os.fspath(root), *arguments], check=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=text,
            env={"PATH": os.environ.get("PATH", ""), "LC_ALL": "C"},
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise MetadataError(f"Git failed while reading immutable source data: {' '.join(arguments)}") from error
    return process.stdout


def git_archive_sha256(root: Path, revision: str) -> str:
    digest = hashlib.sha256()
    try:
        process = subprocess.Popen(
            ["git", "-C", os.fspath(root), "archive", "--format=tar", revision],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env={"PATH": os.environ.get("PATH", ""), "LC_ALL": "C"},
        )
        assert process.stdout is not None
        for chunk in iter(lambda: process.stdout.read(1024 * 1024), b""):
            digest.update(chunk)
        stderr = process.stderr.read() if process.stderr else b""
        require(process.wait() == 0, f"Git archive failed: {stderr.decode('utf-8', 'replace').strip()}")
    except OSError as error:
        raise MetadataError("cannot execute Git archive") from error
    return digest.hexdigest()


def immutable_source(root: Path, requested_revision: str) -> tuple[str, str, str, list[tuple[str, bytes]]]:
    revision = validate_revision(requested_revision)
    commit = str(git_output(root, ["rev-parse", "--verify", f"{revision}^{{commit}}"])).strip()
    require(commit == revision, "requested revision is not the exact commit object ID")
    tree = str(git_output(root, ["rev-parse", "--verify", f"{revision}^{{tree}}"])).strip()
    require(bool(REVISION_RE.fullmatch(tree)), "Git returned an invalid tree object ID")
    raw = git_output(root, ["ls-tree", "-rz", "--full-tree", revision], text=False)
    assert isinstance(raw, bytes)
    entries: list[tuple[str, bytes]] = []
    seen: set[str] = set()
    for record in raw.split(b"\0"):
        if not record:
            continue
        metadata, separator, encoded_path = record.partition(b"\t")
        fields = metadata.split(b" ")
        require(bool(separator) and len(fields) == 3, "malformed Git tree entry")
        mode, object_type, object_id = (field.decode("ascii") for field in fields)
        require(mode in {"100644", "100755"} and object_type == "blob", "unsupported non-regular Git tree entry")
        try:
            relative = encoded_path.decode("utf-8")
        except UnicodeDecodeError as error:
            raise MetadataError("source path is not valid UTF-8") from error
        require(not relative.startswith("/") and "\\" not in relative and ".." not in Path(relative).parts, "unsafe source path")
        require(relative not in seen, "duplicate source path in Git tree")
        seen.add(relative)
        blob = git_output(root, ["cat-file", "blob", object_id], text=False)
        assert isinstance(blob, bytes)
        entries.append((relative, blob))
    require(bool(entries), "source commit has no regular tracked files")
    entries.sort(key=lambda item: item[0].encode("utf-8"))
    return revision, tree, git_archive_sha256(root, revision), entries


def spdx_id(prefix: str, value: str) -> str:
    normalized = re.sub(r"[^A-Za-z0-9.-]", "-", value).strip("-.") or "item"
    return f"SPDXRef-{prefix}-{normalized[:64]}-{hashlib.sha256(value.encode()).hexdigest()[:12]}"


def iso_timestamp(epoch: int) -> str:
    return dt.datetime.fromtimestamp(epoch, tz=dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def artifact_records(paths: Iterable[str]) -> list[tuple[str, Path, str]]:
    result: list[tuple[str, Path, str]] = []
    seen: set[str] = set()
    for value in paths:
        path = resolve_existing_file(value, "artifact")
        require(bool(NAME_RE.fullmatch(path.name)) and path.name not in seen, "artifact names must be unique and portable")
        seen.add(path.name)
        result.append((path.name, path, hash_file(path)))
    require(bool(result), "at least one artifact is required")
    return result


def config_from_args(args: argparse.Namespace) -> dict[str, Any]:
    root = resolve_source_root(args.source_root)
    revision, tree, archive, sources = immutable_source(root, args.revision)
    artifacts = artifact_records(args.artifact)
    targets = unique_names(args.target, "target")
    require(targets == [item[0] for item in artifacts], "ordered targets must match ordered artifact basenames")
    project = safe_metadata(args.name, "project name")
    require(bool(NAME_RE.fullmatch(project)), "project name must be portable")
    flags = [safe_metadata(value, "normalized build flag") for value in args.normalized_flag]
    require(len(flags) == len(set(flags)), "duplicate normalized build flag")
    return {
        "root": root, "revision": revision, "tree": tree, "archive": archive, "sources": sources,
        "artifacts": artifacts, "epoch": validate_epoch(args.epoch),
        "project": project,
        "dependencies": unique_name_values(args.dependency, "dependency"),
        "toolchains": unique_name_values(args.toolchain, "toolchain", preserve_order=True),
        "targets": targets, "buildType": safe_metadata(args.build_type, "build type"),
        "builderId": validate_uri(args.builder_id, "builder ID", {"https"}),
        "buildTypeUri": validate_uri(args.build_type_uri, "build type URI", {"https"}),
        "sourceUri": validate_uri(args.source_uri, "source URI", {"git+https"}),
        "networkPolicy": safe_metadata(args.network_policy, "network policy"),
        "flags": flags,
    }


def invocation_input(config: dict[str, Any]) -> dict[str, Any]:
    return {
        "artifacts": [{"name": name, "sha256": digest} for name, _, digest in config["artifacts"]],
        "builderId": config["builderId"], "buildType": config["buildType"],
        "buildTypeUri": config["buildTypeUri"], "networkAccess": False,
        "networkPolicy": config["networkPolicy"], "normalizedBuildFlags": config["flags"],
        "source": {"archiveSha256": config["archive"], "gitCommit": config["revision"],
                   "gitTree": config["tree"], "uri": config["sourceUri"]},
        "sourceDateEpoch": config["epoch"], "targets": config["targets"],
        "toolchains": [{"name": name, "version": version} for name, version in config["toolchains"]],
    }


def create_documents(config: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any]]:
    invocation = invocation_input(config)
    invocation_digest = hashlib.sha256(canonical_json(invocation)).hexdigest()
    source_package_id = spdx_id("Package", config["project"])
    files: list[dict[str, Any]] = []
    relationships: list[dict[str, str]] = []
    source_sha1: list[str] = []
    for relative, content in config["sources"]:
        digest = hash_bytes(content)
        source_sha1.append(hash_bytes(content, "sha1"))
        file_id = spdx_id("Source", relative)
        files.append({"SPDXID": file_id, "checksums": [{"algorithm": "SHA256", "checksumValue": digest}],
                      "copyrightText": "NOASSERTION", "fileName": f"./{relative}"})
        relationships.append({"spdxElementId": source_package_id, "relationshipType": "CONTAINS", "relatedSpdxElement": file_id})
    for name, _, digest in config["artifacts"]:
        file_id = spdx_id("Artifact", name)
        files.append({"SPDXID": file_id, "checksums": [{"algorithm": "SHA256", "checksumValue": digest}],
                      "copyrightText": "NOASSERTION", "fileName": f"./artifacts/{name}", "fileTypes": ["BINARY"]})
        relationships.append({"spdxElementId": source_package_id, "relationshipType": "GENERATES", "relatedSpdxElement": file_id})
    packages: list[dict[str, Any]] = [{
        "SPDXID": source_package_id,
        "checksums": [{"algorithm": "SHA256", "checksumValue": config["archive"]}],
        "copyrightText": "NOASSERTION", "downloadLocation": "NOASSERTION", "filesAnalyzed": True,
        "externalRefs": [
            {"referenceCategory": "OTHER", "referenceLocator": config["revision"], "referenceType": "gitCommit"},
            {"referenceCategory": "OTHER", "referenceLocator": config["tree"], "referenceType": "gitTree"},
            {"referenceCategory": "OTHER", "referenceLocator": config["sourceUri"], "referenceType": "vcs"},
        ],
        "name": config["project"],
        "packageVerificationCode": {"packageVerificationCodeValue": hashlib.sha1("".join(sorted(source_sha1)).encode()).hexdigest()},
        "supplier": "Organization: ALRI GROUP", "versionInfo": config["revision"],
    }]
    for name, version in config["dependencies"]:
        package_id = spdx_id("Dependency", f"{name}-{version}")
        packages.append({"SPDXID": package_id, "copyrightText": "NOASSERTION", "downloadLocation": "NOASSERTION",
                         "filesAnalyzed": False, "name": name, "versionInfo": version})
        relationships.append({"spdxElementId": source_package_id, "relationshipType": "DEPENDS_ON", "relatedSpdxElement": package_id})
    relationships.append({"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES", "relatedSpdxElement": source_package_id})
    sbom = {
        "SPDXID": "SPDXRef-DOCUMENT",
        "creationInfo": {"created": iso_timestamp(config["epoch"]), "creators": ["Organization: ALRI GROUP", "Tool: alrios-sbom.py-2"]},
        "dataLicense": SPDX_DATA_LICENSE,
        "documentNamespace": f"https://alrigroup.com/spdx/{config['project']}/{invocation_digest}",
        "files": sorted(files, key=lambda item: item["fileName"]),
        "name": f"{config['project']}-{config['revision'][:12]}",
        "packages": sorted(packages, key=lambda item: item["SPDXID"]),
        "relationships": sorted(relationships, key=lambda item: (item["spdxElementId"], item["relationshipType"], item["relatedSpdxElement"])),
        "spdxVersion": SPDX_VERSION,
    }
    provenance = {
        "_type": PROVENANCE_TYPE, "predicateType": PROVENANCE_PREDICATE,
        "subject": [{"name": name, "digest": {"sha256": digest}} for name, _, digest in config["artifacts"]],
        "predicate": {
            "buildDefinition": {
                "buildType": config["buildTypeUri"],
                "externalParameters": {"buildType": config["buildType"], "networkAccess": False,
                    "networkPolicy": config["networkPolicy"], "normalizedBuildFlags": config["flags"],
                    "sourceDateEpoch": config["epoch"], "targets": config["targets"]},
                "internalParameters": {"toolchains": invocation["toolchains"]},
                "resolvedDependencies": [{"uri": config["sourceUri"], "digest": {
                    "archiveSha256": config["archive"], "gitCommit": config["revision"], "gitTree": config["tree"]}}],
            },
            "runDetails": {"builder": {"id": config["builderId"]},
                           "metadata": {"invocationId": f"sha256:{invocation_digest}"}},
        },
    }
    return sbom, provenance


def validate_no_paths(value: Any, label: str = "metadata", uri_keys: set[str] | None = None) -> None:
    uri_keys = uri_keys or {"documentNamespace", "uri", "buildType", "id"}
    if isinstance(value, dict):
        for key, item in value.items():
            if isinstance(item, str) and key in uri_keys:
                continue
            if key == "fileName" and isinstance(item, str):
                require(item.startswith("./") and not item.startswith("../") and "\\" not in item, "unsafe SPDX fileName")
                continue
            validate_no_paths(item, f"{label}.{key}", uri_keys)
    elif isinstance(value, list):
        for index, item in enumerate(value):
            validate_no_paths(item, f"{label}[{index}]", uri_keys)
    elif isinstance(value, str):
        require(not contains_local_path(value), f"{label} contains a local filesystem path")


def checksum(value: Any, label: str) -> str:
    obj = exact_keys(value, {"algorithm", "checksumValue"}, label)
    require(obj["algorithm"] == "SHA256" and isinstance(obj["checksumValue"], str) and SHA256_RE.fullmatch(obj["checksumValue"]), f"{label} is invalid")
    return obj["checksumValue"]


def validate_sbom(sbom: Any) -> None:
    document = exact_keys(sbom, {"SPDXID", "creationInfo", "dataLicense", "documentNamespace", "files", "name", "packages", "relationships", "spdxVersion"}, "SBOM")
    require(document["SPDXID"] == "SPDXRef-DOCUMENT" and document["spdxVersion"] == SPDX_VERSION and document["dataLicense"] == SPDX_DATA_LICENSE, "unsupported SPDX document identity")
    require(isinstance(document["name"], str) and bool(document["name"]), "SPDX name is invalid")
    validate_uri(document["documentNamespace"], "SPDX namespace", {"https"})
    creation = exact_keys(document["creationInfo"], {"created", "creators"}, "SPDX creationInfo")
    require(isinstance(creation["created"], str), "SPDX timestamp is invalid")
    try:
        parsed = dt.datetime.strptime(creation["created"], "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=dt.timezone.utc)
    except ValueError as error:
        raise MetadataError("SPDX timestamp is invalid") from error
    require(parsed.strftime("%Y-%m-%dT%H:%M:%SZ") == creation["created"], "SPDX timestamp is not canonical")
    require(creation["creators"] == ["Organization: ALRI GROUP", "Tool: alrios-sbom.py-2"], "SPDX creators are invalid")
    require(isinstance(document["files"], list) and isinstance(document["packages"], list) and isinstance(document["relationships"], list), "SPDX collections are invalid")
    identifiers = {"SPDXRef-DOCUMENT"}
    file_names: set[str] = set()
    for index, item in enumerate(document["files"]):
        allowed = {"SPDXID", "checksums", "copyrightText", "fileName"}
        if isinstance(item, dict) and "fileTypes" in item:
            allowed.add("fileTypes")
        entry = exact_keys(item, allowed, f"SPDX file {index}")
        require(isinstance(entry["SPDXID"], str) and SPDX_ID_RE.fullmatch(entry["SPDXID"]) and entry["SPDXID"] not in identifiers, "duplicate or invalid SPDX file ID")
        identifiers.add(entry["SPDXID"])
        require(isinstance(entry["fileName"], str) and entry["fileName"] not in file_names and entry["fileName"].startswith("./"), "duplicate or invalid SPDX filename")
        file_names.add(entry["fileName"])
        require(entry["copyrightText"] == "NOASSERTION" and isinstance(entry["checksums"], list) and len(entry["checksums"]) == 1, "invalid SPDX file fields")
        checksum(entry["checksums"][0], "SPDX file checksum")
        if "fileTypes" in entry:
            require(entry["fileTypes"] == ["BINARY"] and entry["fileName"].startswith("./artifacts/"), "invalid SPDX binary file type")
    package_names: set[str] = set()
    for index, item in enumerate(document["packages"]):
        require(isinstance(item, dict), "SPDX package is invalid")
        source = item.get("filesAnalyzed") is True
        keys = {"SPDXID", "copyrightText", "downloadLocation", "filesAnalyzed", "name", "versionInfo"}
        if source:
            keys |= {"checksums", "externalRefs", "packageVerificationCode", "supplier"}
        entry = exact_keys(item, keys, f"SPDX package {index}")
        require(isinstance(entry["SPDXID"], str) and SPDX_ID_RE.fullmatch(entry["SPDXID"]) and entry["SPDXID"] not in identifiers, "duplicate or invalid SPDX package ID")
        identifiers.add(entry["SPDXID"])
        require(isinstance(entry["name"], str) and entry["name"] not in package_names, "duplicate or invalid SPDX package name")
        package_names.add(entry["name"])
        require(type(entry["filesAnalyzed"]) is bool and entry["copyrightText"] == "NOASSERTION" and entry["downloadLocation"] == "NOASSERTION" and isinstance(entry["versionInfo"], str), "invalid SPDX package fields")
        if source:
            require(isinstance(entry["checksums"], list) and len(entry["checksums"]) == 1, "invalid source package checksum")
            checksum(entry["checksums"][0], "SPDX package checksum")
            pvc = exact_keys(entry["packageVerificationCode"], {"packageVerificationCodeValue"}, "SPDX verification code")
            require(isinstance(pvc["packageVerificationCodeValue"], str) and SHA1_RE.fullmatch(pvc["packageVerificationCodeValue"]), "invalid SPDX verification code")
            require(entry["supplier"] == "Organization: ALRI GROUP" and isinstance(entry["externalRefs"], list) and len(entry["externalRefs"]) == 3, "invalid source package metadata")
            ref_types: set[str] = set()
            for ref in entry["externalRefs"]:
                ref = exact_keys(ref, {"referenceCategory", "referenceLocator", "referenceType"}, "SPDX external reference")
                require(ref["referenceCategory"] == "OTHER" and ref["referenceType"] in {"gitCommit", "gitTree", "vcs"} and ref["referenceType"] not in ref_types and isinstance(ref["referenceLocator"], str), "invalid or duplicate SPDX external reference")
                ref_types.add(ref["referenceType"])
    relationship_keys: set[tuple[str, str, str]] = set()
    for item in document["relationships"]:
        relation = exact_keys(item, {"spdxElementId", "relationshipType", "relatedSpdxElement"}, "SPDX relationship")
        key = (relation["spdxElementId"], relation["relationshipType"], relation["relatedSpdxElement"])
        require(key not in relationship_keys, "duplicate SPDX relationship")
        relationship_keys.add(key)
        require(key[0] in identifiers and key[2] in identifiers, "dangling SPDX relationship")
        require(key[1] in {"CONTAINS", "GENERATES", "DEPENDS_ON", "DESCRIBES"}, "unsupported SPDX relationship type")
    validate_no_paths(document)


def validate_provenance(provenance: Any) -> None:
    statement = exact_keys(provenance, {"_type", "predicateType", "subject", "predicate"}, "provenance")
    require(statement["_type"] == PROVENANCE_TYPE and statement["predicateType"] == PROVENANCE_PREDICATE, "unsupported provenance type")
    require(isinstance(statement["subject"], list) and bool(statement["subject"]), "provenance subjects are invalid")
    names: set[str] = set()
    for item in statement["subject"]:
        subject = exact_keys(item, {"name", "digest"}, "provenance subject")
        digest = exact_keys(subject["digest"], {"sha256"}, "provenance subject digest")
        require(isinstance(subject["name"], str) and NAME_RE.fullmatch(subject["name"]) and subject["name"] not in names, "duplicate or invalid provenance subject")
        names.add(subject["name"])
        require(isinstance(digest["sha256"], str) and SHA256_RE.fullmatch(digest["sha256"]), "invalid provenance subject digest")
    predicate = exact_keys(statement["predicate"], {"buildDefinition", "runDetails"}, "provenance predicate")
    definition = exact_keys(predicate["buildDefinition"], {"buildType", "externalParameters", "internalParameters", "resolvedDependencies"}, "build definition")
    validate_uri(definition["buildType"], "provenance build type", {"https"})
    external = exact_keys(definition["externalParameters"], {"buildType", "networkAccess", "networkPolicy", "normalizedBuildFlags", "sourceDateEpoch", "targets"}, "external parameters")
    require(isinstance(external["buildType"], str) and external["networkAccess"] is False and isinstance(external["networkPolicy"], str), "invalid external parameters")
    require(type(external["sourceDateEpoch"]) is int and 0 <= external["sourceDateEpoch"] <= 253402300799, "invalid provenance epoch")
    require(isinstance(external["targets"], list) and all(isinstance(item, str) for item in external["targets"]), "invalid provenance targets")
    require(len(external["targets"]) == len(set(external["targets"])), "duplicate provenance target")
    require(isinstance(external["normalizedBuildFlags"], list) and all(isinstance(item, str) for item in external["normalizedBuildFlags"]), "invalid normalized flags")
    internal = exact_keys(definition["internalParameters"], {"toolchains"}, "internal parameters")
    require(isinstance(internal["toolchains"], list), "toolchains must be a list")
    toolchain_names: set[str] = set()
    for item in internal["toolchains"]:
        toolchain = exact_keys(item, {"name", "version"}, "toolchain")
        require(isinstance(toolchain["name"], str) and NAME_RE.fullmatch(toolchain["name"]) and toolchain["name"] not in toolchain_names and isinstance(toolchain["version"], str), "duplicate or invalid toolchain")
        toolchain_names.add(toolchain["name"])
    materials = definition["resolvedDependencies"]
    require(isinstance(materials, list) and len(materials) == 1, "exactly one source material is required")
    material = exact_keys(materials[0], {"uri", "digest"}, "source material")
    validate_uri(material["uri"], "source material URI", {"git+https"})
    material_digest = exact_keys(material["digest"], {"archiveSha256", "gitCommit", "gitTree"}, "source material digest")
    require(all(isinstance(value, str) for value in material_digest.values()), "source material digests are invalid")
    require(SHA256_RE.fullmatch(material_digest["archiveSha256"]) is not None and REVISION_RE.fullmatch(material_digest["gitCommit"]) is not None and REVISION_RE.fullmatch(material_digest["gitTree"]) is not None, "source material digests are invalid")
    run = exact_keys(predicate["runDetails"], {"builder", "metadata"}, "run details")
    builder = exact_keys(run["builder"], {"id"}, "builder")
    validate_uri(builder["id"], "builder ID", {"https"})
    metadata = exact_keys(run["metadata"], {"invocationId"}, "run metadata")
    require(isinstance(metadata["invocationId"], str) and re.fullmatch(r"sha256:[0-9a-f]{64}", metadata["invocationId"]), "invalid invocation ID")
    validate_no_paths(statement)


def verify_documents(args: argparse.Namespace) -> None:
    config = config_from_args(args)
    expected_sbom, expected_provenance = create_documents(config)
    actual_sbom = load_json(resolve_existing_file(args.sbom, "SBOM"))
    actual_provenance = load_json(resolve_existing_file(args.provenance, "provenance"))
    validate_sbom(actual_sbom)
    validate_provenance(actual_provenance)
    require(actual_sbom == expected_sbom, "SBOM does not exactly match immutable source, artifacts, and trusted expectations")
    require(actual_provenance == expected_provenance, "provenance does not exactly match invocation-bound trusted expectations")


def add_contract_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--artifact", action="append", required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--epoch", required=True)
    parser.add_argument("--target", action="append", required=True)
    parser.add_argument("--toolchain", action="append", required=True)
    parser.add_argument("--dependency", action="append", default=[])
    parser.add_argument("--normalized-flag", action="append", default=[])
    parser.add_argument("--name", default="alrios")
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--builder-id", default=DEFAULT_BUILDER_ID)
    parser.add_argument("--build-type-uri", default=DEFAULT_BUILD_TYPE_URI)
    parser.add_argument("--source-uri", default=DEFAULT_SOURCE_URI)
    parser.add_argument("--network-policy", default="LinuxNetworkNamespace")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate or strictly verify deterministic ALRIOS release metadata")
    commands = parser.add_subparsers(dest="command", required=True)
    generate = commands.add_parser("generate")
    add_contract_arguments(generate)
    generate.add_argument("--sbom", required=True)
    generate.add_argument("--provenance", required=True)
    verify = commands.add_parser("verify")
    add_contract_arguments(verify)
    verify.add_argument("--sbom", required=True)
    verify.add_argument("--provenance", required=True)
    return parser


def main() -> int:
    try:
        args = build_parser().parse_args()
        if args.command == "generate":
            config = config_from_args(args)
            sbom, provenance = create_documents(config)
            sbom_path = safe_output_path(args.sbom, "SBOM")
            provenance_path = safe_output_path(args.provenance, "provenance")
            require(sbom_path != provenance_path, "SBOM and provenance destinations must differ")
            write_exclusive(sbom_path, canonical_json(sbom), config["epoch"])
            try:
                write_exclusive(provenance_path, canonical_json(provenance), config["epoch"])
            except BaseException:
                sbom_path.unlink(missing_ok=True)
                raise
        else:
            verify_documents(args)
    except MetadataError as error:
        print(f"[supply-chain] ERROR: {error}", file=sys.stderr)
        return 1
    except OSError as error:
        print(f"[supply-chain] ERROR: file operation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
