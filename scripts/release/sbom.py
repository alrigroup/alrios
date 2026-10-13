#!/usr/bin/env python3
"""Generate and verify deterministic SPDX SBOM and in-toto provenance metadata."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Iterable

SPDX_VERSION = "SPDX-2.3"
SPDX_DATA_LICENSE = "CC0-1.0"
PROVENANCE_TYPE = "https://in-toto.io/Statement/v1"
PROVENANCE_PREDICATE = "https://slsa.dev/provenance/v1"
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
REVISION_RE = re.compile(r"^[0-9a-f]{40,64}$")
NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._+:-]{0,127}$")


class MetadataError(ValueError):
    pass


def hash_file(path: Path, algorithm: str) -> str:
    digest = hashlib.new(algorithm)
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_file(path: Path) -> str:
    return hash_file(path, "sha256")


def canonical_json(data: Any) -> bytes:
    return (json.dumps(data, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def write_atomic(path: Path, data: bytes, epoch: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temporary_name, 0o644)
        os.utime(temporary_name, (epoch, epoch))
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def load_json(path: Path) -> Any:
    def reject_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise MetadataError(f"duplicate JSON key: {key}")
            result[key] = value
        return result

    try:
        with path.open("r", encoding="utf-8") as stream:
            return json.load(stream, object_pairs_hook=reject_duplicates)
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise MetadataError(f"cannot read valid JSON from {path.name}: {error}") from error


def resolve_existing_file(value: str, label: str) -> Path:
    path = Path(value)
    try:
        resolved = path.resolve(strict=True)
    except OSError as error:
        raise MetadataError(f"{label} is not an accessible file: {value}") from error
    if not resolved.is_file() or path.is_symlink():
        raise MetadataError(f"{label} must be a regular, non-symlink file: {value}")
    return resolved


def resolve_source_root(value: str) -> Path:
    try:
        root = Path(value).resolve(strict=True)
    except OSError as error:
        raise MetadataError(f"source root is not accessible: {value}") from error
    if not root.is_dir():
        raise MetadataError(f"source root is not a directory: {value}")
    if not (root / ".git").exists():
        raise MetadataError("source root must be a Git working tree")
    return root


def validate_revision(value: str) -> str:
    revision = value.lower()
    if not REVISION_RE.fullmatch(revision):
        raise MetadataError("revision must be a 40-64 character lowercase hexadecimal digest")
    return revision


def validate_epoch(value: str) -> int:
    if not value.isascii() or not value.isdecimal():
        raise MetadataError("SOURCE_DATE_EPOCH must be a non-negative base-10 integer")
    epoch = int(value)
    if epoch < 0 or epoch > 253402300799:
        raise MetadataError("SOURCE_DATE_EPOCH is outside the supported UTC timestamp range")
    return epoch


def safe_metadata(value: str, label: str) -> str:
    if not value or len(value) > 512 or any(ord(character) < 0x20 for character in value):
        raise MetadataError(f"{label} contains invalid characters or has invalid length")
    if value.startswith(("/", "\\")) or re.search(r"[A-Za-z]:[\\/]", value):
        raise MetadataError(f"{label} must not contain an absolute local path")
    return value


def parse_name_version(value: str, label: str) -> tuple[str, str]:
    name, separator, version = value.partition("=")
    if not separator or not NAME_RE.fullmatch(name):
        raise MetadataError(f"{label} must use NAME=VERSION with a portable name")
    return name, safe_metadata(version, f"{label} version")


def git_output(root: Path, arguments: list[str], text: bool = True) -> str | bytes:
    try:
        process = subprocess.run(
            ["git", "-C", os.fspath(root), *arguments],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=text,
            env={"PATH": os.environ.get("PATH", ""), "LC_ALL": "C"},
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise MetadataError(f"Git command failed while reading source metadata: {' '.join(arguments)}") from error
    return process.stdout


def tracked_source_files(root: Path) -> list[tuple[str, Path]]:
    raw = git_output(root, ["ls-files", "-z"], text=False)
    assert isinstance(raw, bytes)
    entries: list[tuple[str, Path]] = []
    for encoded in raw.split(b"\0"):
        if not encoded:
            continue
        try:
            relative = encoded.decode("utf-8")
        except UnicodeDecodeError as error:
            raise MetadataError("tracked source path is not valid UTF-8") from error
        if relative.startswith("/") or ".." in Path(relative).parts or "\\" in relative:
            raise MetadataError(f"unsafe tracked source path: {relative!r}")
        path = root / relative
        try:
            resolved = path.resolve(strict=True)
        except OSError as error:
            raise MetadataError(f"tracked source file is unavailable: {relative}") from error
        if not resolved.is_relative_to(root) or not resolved.is_file() or path.is_symlink():
            raise MetadataError(f"tracked source entry is not a regular in-tree file: {relative}")
        entries.append((relative, resolved))
    if not entries:
        raise MetadataError("source tree has no tracked files")
    return sorted(entries, key=lambda item: item[0].encode("utf-8"))


def spdx_id(prefix: str, value: str) -> str:
    normalized = re.sub(r"[^A-Za-z0-9.-]", "-", value).strip("-.") or "item"
    suffix = hashlib.sha256(value.encode("utf-8")).hexdigest()[:12]
    return f"SPDXRef-{prefix}-{normalized[:64]}-{suffix}"


def iso_timestamp(epoch: int) -> str:
    return dt.datetime.fromtimestamp(epoch, tz=dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def artifact_records(paths: Iterable[str]) -> list[tuple[str, Path, str]]:
    records: list[tuple[str, Path, str]] = []
    names: set[str] = set()
    for value in paths:
        path = resolve_existing_file(value, "artifact")
        name = path.name
        if not NAME_RE.fullmatch(name) or name in names:
            raise MetadataError("artifact basenames must be unique portable names")
        names.add(name)
        records.append((name, path, sha256_file(path)))
    if not records:
        raise MetadataError("at least one artifact is required")
    return sorted(records, key=lambda item: item[0])


def create_documents(args: argparse.Namespace) -> tuple[dict[str, Any], dict[str, Any], int]:
    root = resolve_source_root(args.source_root)
    revision = validate_revision(args.revision)
    epoch = validate_epoch(args.epoch)
    project_name = safe_metadata(args.name, "project name")
    if not NAME_RE.fullmatch(project_name):
        raise MetadataError("project name must be portable")

    actual_revision = str(git_output(root, ["rev-parse", "HEAD"])).strip().lower()
    if revision != actual_revision:
        raise MetadataError("requested revision does not match source working tree HEAD")
    sources = tracked_source_files(root)
    artifacts = artifact_records(args.artifact)
    dependencies = sorted({parse_name_version(item, "dependency") for item in args.dependency})
    toolchains = sorted({parse_name_version(item, "toolchain") for item in args.toolchain})
    parameters = dict(sorted(parse_name_version(item, "parameter") for item in args.parameter))

    namespace_seed = canonical_json({
        "revision": revision,
        "epoch": epoch,
        "artifacts": [(name, digest) for name, _, digest in artifacts],
        "dependencies": dependencies,
        "toolchains": toolchains,
        "parameters": parameters,
    })
    namespace_digest = hashlib.sha256(namespace_seed).hexdigest()
    document_id = "SPDXRef-DOCUMENT"
    source_package_id = spdx_id("Package", project_name)

    files: list[dict[str, Any]] = []
    relationships: list[dict[str, str]] = []
    source_sha1_hashes: list[str] = []
    source_sha256_hashes: list[str] = []
    for relative, path in sources:
        digest = sha256_file(path)
        source_sha1_hashes.append(hash_file(path, "sha1"))
        source_sha256_hashes.append(digest)
        file_id = spdx_id("Source", relative)
        files.append({
            "SPDXID": file_id,
            "checksums": [{"algorithm": "SHA256", "checksumValue": digest}],
            "copyrightText": "NOASSERTION",
            "fileName": f"./{relative}",
        })
        relationships.append({
            "spdxElementId": source_package_id,
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": file_id,
        })

    artifact_ids: dict[str, str] = {}
    for name, _, digest in artifacts:
        artifact_id = spdx_id("Artifact", name)
        artifact_ids[name] = artifact_id
        files.append({
            "SPDXID": artifact_id,
            "checksums": [{"algorithm": "SHA256", "checksumValue": digest}],
            "copyrightText": "NOASSERTION",
            "fileName": f"./artifacts/{name}",
            "fileTypes": ["BINARY"],
        })
        relationships.append({
            "spdxElementId": source_package_id,
            "relationshipType": "GENERATES",
            "relatedSpdxElement": artifact_id,
        })

    packages: list[dict[str, Any]] = [{
        "SPDXID": source_package_id,
        "checksums": [{"algorithm": "SHA256", "checksumValue": hashlib.sha256("".join(source_sha256_hashes).encode("ascii")).hexdigest()}],
        "copyrightText": "NOASSERTION",
        "downloadLocation": "NOASSERTION",
        "filesAnalyzed": True,
        "name": project_name,
        "packageVerificationCode": {"packageVerificationCodeValue": hashlib.sha1("".join(sorted(source_sha1_hashes)).encode("ascii")).hexdigest()},
        "supplier": "Organization: ALRI GROUP",
        "versionInfo": revision,
    }]
    for name, version in dependencies:
        package_id = spdx_id("Dependency", f"{name}-{version}")
        packages.append({
            "SPDXID": package_id,
            "copyrightText": "NOASSERTION",
            "downloadLocation": "NOASSERTION",
            "filesAnalyzed": False,
            "name": name,
            "versionInfo": version,
        })
        relationships.append({
            "spdxElementId": source_package_id,
            "relationshipType": "DEPENDS_ON",
            "relatedSpdxElement": package_id,
        })

    relationships.append({
        "spdxElementId": document_id,
        "relationshipType": "DESCRIBES",
        "relatedSpdxElement": source_package_id,
    })
    sbom: dict[str, Any] = {
        "SPDXID": document_id,
        "creationInfo": {
            "created": iso_timestamp(epoch),
            "creators": ["Organization: ALRI GROUP", "Tool: alrios-sbom.py-1"],
        },
        "dataLicense": SPDX_DATA_LICENSE,
        "documentNamespace": f"https://alrigroup.com/spdx/{project_name}/{namespace_digest}",
        "files": sorted(files, key=lambda item: item["fileName"]),
        "name": f"{project_name}-{revision[:12]}",
        "packages": sorted(packages, key=lambda item: item["SPDXID"]),
        "relationships": sorted(relationships, key=lambda item: (item["spdxElementId"], item["relationshipType"], item["relatedSpdxElement"])),
        "spdxVersion": SPDX_VERSION,
    }

    provenance: dict[str, Any] = {
        "_type": PROVENANCE_TYPE,
        "predicateType": PROVENANCE_PREDICATE,
        "subject": [
            {"name": name, "digest": {"sha256": digest}}
            for name, _, digest in artifacts
        ],
        "predicate": {
            "buildDefinition": {
                "buildType": "https://alrigroup.com/alrios/build/reproducible/v1",
                "externalParameters": {
                    "networkAccess": False,
                    "parameters": parameters,
                    "sourceDateEpoch": epoch,
                },
                "internalParameters": {
                    "toolchains": [{"name": name, "version": version} for name, version in toolchains],
                },
                "resolvedDependencies": [{
                    "uri": "git+https://github.com/alrigroup/alrios.git",
                    "digest": {"gitCommit": revision},
                }],
            },
            "runDetails": {
                "builder": {"id": "https://alrigroup.com/alrios/reproducible-build/v1"},
                "metadata": {
                    "invocationId": f"sha256:{namespace_digest}",
                },
            },
        },
    }
    return sbom, provenance, epoch


def checksum_from_spdx_file(item: Any) -> str | None:
    if not isinstance(item, dict) or not isinstance(item.get("checksums"), list):
        return None
    values = [entry.get("checksumValue") for entry in item["checksums"] if isinstance(entry, dict) and entry.get("algorithm") == "SHA256"]
    if len(values) != 1 or not isinstance(values[0], str) or not SHA256_RE.fullmatch(values[0]):
        return None
    return values[0]


def verify_documents(args: argparse.Namespace) -> None:
    sbom_path = resolve_existing_file(args.sbom, "SBOM")
    provenance_path = resolve_existing_file(args.provenance, "provenance")
    sbom = load_json(sbom_path)
    provenance = load_json(provenance_path)
    if not isinstance(sbom, dict) or sbom.get("spdxVersion") != SPDX_VERSION or sbom.get("dataLicense") != SPDX_DATA_LICENSE:
        raise MetadataError("SBOM is not a supported SPDX 2.3 document")
    if not isinstance(provenance, dict) or provenance.get("_type") != PROVENANCE_TYPE or provenance.get("predicateType") != PROVENANCE_PREDICATE:
        raise MetadataError("provenance is not a supported in-toto/SLSA statement")

    artifact_map = {name: digest for name, _, digest in artifact_records(args.artifact)}
    sbom_artifacts: dict[str, str] = {}
    for item in sbom.get("files", []):
        if not isinstance(item, dict):
            raise MetadataError("SBOM files entry is malformed")
        name = item.get("fileName")
        if isinstance(name, str) and name.startswith("./artifacts/"):
            basename = name.removeprefix("./artifacts/")
            digest = checksum_from_spdx_file(item)
            if not NAME_RE.fullmatch(basename) or digest is None or basename in sbom_artifacts:
                raise MetadataError("SBOM artifact entry is malformed or duplicated")
            sbom_artifacts[basename] = digest
    if sbom_artifacts != artifact_map:
        raise MetadataError("SBOM artifact digests do not match supplied artifacts")

    subjects = provenance.get("subject")
    if not isinstance(subjects, list):
        raise MetadataError("provenance subject list is missing")
    provenance_artifacts: dict[str, str] = {}
    for subject in subjects:
        if not isinstance(subject, dict) or not isinstance(subject.get("digest"), dict):
            raise MetadataError("provenance subject is malformed")
        name = subject.get("name")
        digest = subject["digest"].get("sha256")
        if not isinstance(name, str) or not isinstance(digest, str) or not SHA256_RE.fullmatch(digest) or name in provenance_artifacts:
            raise MetadataError("provenance subject is malformed or duplicated")
        provenance_artifacts[name] = digest
    if provenance_artifacts != artifact_map:
        raise MetadataError("provenance artifact digests do not match supplied artifacts")

    build_definition = provenance.get("predicate", {}).get("buildDefinition", {}) if isinstance(provenance.get("predicate"), dict) else {}
    external = build_definition.get("externalParameters", {}) if isinstance(build_definition, dict) else {}
    if not isinstance(external, dict) or external.get("networkAccess") is not False or not isinstance(external.get("sourceDateEpoch"), int):
        raise MetadataError("provenance lacks fail-closed offline and SOURCE_DATE_EPOCH parameters")
    dependencies = build_definition.get("resolvedDependencies")
    if not isinstance(dependencies, list) or len(dependencies) != 1:
        raise MetadataError("provenance source material is missing")
    digest = dependencies[0].get("digest", {}) if isinstance(dependencies[0], dict) else {}
    revision = digest.get("gitCommit") if isinstance(digest, dict) else None
    if not isinstance(revision, str) or not REVISION_RE.fullmatch(revision):
        raise MetadataError("provenance source revision is malformed")

    if args.source_root:
        root = resolve_source_root(args.source_root)
        actual_revision = str(git_output(root, ["rev-parse", "HEAD"])).strip().lower()
        if revision != actual_revision:
            raise MetadataError("provenance source revision does not match source working tree")
        expected_sources = {f"./{relative}": sha256_file(path) for relative, path in tracked_source_files(root)}
        actual_sources: dict[str, str] = {}
        for item in sbom.get("files", []):
            name = item.get("fileName") if isinstance(item, dict) else None
            if isinstance(name, str) and not name.startswith("./artifacts/"):
                digest_value = checksum_from_spdx_file(item)
                if digest_value is None or name in actual_sources:
                    raise MetadataError("SBOM source entry is malformed or duplicated")
                actual_sources[name] = digest_value
        if actual_sources != expected_sources:
            raise MetadataError("SBOM source inventory does not match source working tree")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate or verify deterministic ALRIOS release metadata")
    subparsers = parser.add_subparsers(dest="command", required=True)

    generate = subparsers.add_parser("generate", help="generate SPDX SBOM and SLSA provenance")
    generate.add_argument("--source-root", required=True)
    generate.add_argument("--artifact", action="append", required=True)
    generate.add_argument("--sbom", required=True)
    generate.add_argument("--provenance", required=True)
    generate.add_argument("--revision", required=True)
    generate.add_argument("--epoch", required=True)
    generate.add_argument("--name", default="alrios")
    generate.add_argument("--dependency", action="append", default=[])
    generate.add_argument("--toolchain", action="append", default=[])
    generate.add_argument("--parameter", action="append", default=[])

    verify = subparsers.add_parser("verify", help="verify metadata and artifact/source bindings")
    verify.add_argument("--artifact", action="append", required=True)
    verify.add_argument("--sbom", required=True)
    verify.add_argument("--provenance", required=True)
    verify.add_argument("--source-root")
    return parser


def main() -> int:
    try:
        args = build_parser().parse_args()
        if args.command == "generate":
            sbom, provenance, epoch = create_documents(args)
            sbom_path = Path(args.sbom).resolve()
            provenance_path = Path(args.provenance).resolve()
            if sbom_path == provenance_path:
                raise MetadataError("SBOM and provenance outputs must be different files")
            write_atomic(sbom_path, canonical_json(sbom), epoch)
            write_atomic(provenance_path, canonical_json(provenance), epoch)
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
