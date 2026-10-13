#!/usr/bin/env python3
"""
scripts/gates/banned_apis.py - ALRIOS Strict Toolchain Banned-API Scanner Gate

Acceptance Criterion 1:
Rejects unsafe C functions (strcpy, strcat, sprintf, vsprintf, gets, system, popen)
at build/scan time with clear diagnostic error.

Copyright (c) 2026 ALRIGROUP and its affiliates.
Engineered and maintained by ALRI Development.

This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
found in the LICENSE file in the root directory of this source tree
and at: https://github.com/alrigroup/licenses
"""

import os
import sys
import argparse
import json
from typing import List, Dict, Any, Optional

BANNED_APIS = {
    "strcpy": "Unbounded string copy vulnerability (CWE-120). Use snprintf or explicit bounds.",
    "strcat": "Unbounded string concatenation vulnerability (CWE-120). Use snprintf or explicit bounds.",
    "sprintf": "Buffer overflow hazard without length check (CWE-134/120). Use snprintf.",
    "vsprintf": "Buffer overflow hazard without length check (CWE-134/120). Use vsnprintf.",
    "gets": "Fatal buffer overflow hazard removed in C11 (CWE-242). Use fgets.",
    "system": "Command injection vulnerability (CWE-78). Use safe aros_hal / ar_process_create abstractions.",
    "popen": "Shell invocation hazard (CWE-78). Use direct pipe and safe process execution."
}

VALID_C_EXTENSIONS = {".c", ".h", ".cpp", ".cc", ".cxx", ".C", ".H"}


def scan_c_source(filepath: str, content: str) -> List[Dict[str, Any]]:
    violations: List[Dict[str, Any]] = []
    lines = content.splitlines()

    state = 0  # 0: NORMAL, 1: LINE_COMMENT, 2: BLOCK_COMMENT, 3: STRING, 4: CHAR
    i = 0
    n = len(content)
    line_num = 1
    col_num = 1

    while i < n:
        ch = content[i]

        if ch == '\n':
            next_line = line_num + 1
            next_col = 1
        else:
            next_line = line_num
            next_col = col_num + 1

        if state == 0:  # NORMAL
            if ch == '/' and i + 1 < n and content[i + 1] == '/':
                state = 1
                i += 2
                col_num += 2
                continue
            elif ch == '/' and i + 1 < n and content[i + 1] == '*':
                state = 2
                i += 2
                col_num += 2
                continue
            elif ch == '"':
                state = 3
                i += 1
                line_num, col_num = next_line, next_col
                continue
            elif ch == "'":
                state = 4
                i += 1
                line_num, col_num = next_line, next_col
                continue
            elif ch.isalpha() or ch == '_':
                tok_line = line_num
                tok_col = col_num
                tok_chars = [ch]
                j = i + 1
                while j < n and (content[j].isalnum() or content[j] == '_'):
                    tok_chars.append(content[j])
                    j += 1
                tok = "".join(tok_chars)
                if tok in BANNED_APIS:
                    context_line = lines[tok_line - 1].strip() if tok_line - 1 < len(lines) else ""
                    violations.append({
                        "file": filepath,
                        "line": tok_line,
                        "column": tok_col,
                        "api": tok,
                        "reason": BANNED_APIS[tok],
                        "context": context_line
                    })
                adv = j - i
                col_num += adv
                i = j
                continue
        elif state == 1:  # LINE_COMMENT
            if ch == '\n':
                state = 0
        elif state == 2:  # BLOCK_COMMENT
            if ch == '*' and i + 1 < n and content[i + 1] == '/':
                state = 0
                i += 2
                col_num += 2
                line_num = next_line
                continue
        elif state == 3:  # STRING
            if ch == '\\' and i + 1 < n:
                i += 2
                col_num += 2
                continue
            elif ch == '"':
                state = 0
        elif state == 4:  # CHAR
            if ch == '\\' and i + 1 < n:
                i += 2
                col_num += 2
                continue
            elif ch == "'":
                state = 0

        i += 1
        line_num, col_num = next_line, next_col

    return violations


def should_exclude(rel_path: str, exclude_list: List[str]) -> bool:
    norm = rel_path.replace("\\", "/")
    for ex in exclude_list:
        ex_norm = ex.replace("\\", "/").rstrip("/")
        if norm == ex_norm or norm.startswith(ex_norm + "/") or f"/{ex_norm}/" in norm:
            return True
    return False


def collect_files(targets: List[str], exclude_list: List[str]) -> List[str]:
    collected: List[str] = []
    for target in targets:
        if os.path.isfile(target):
            if not should_exclude(target, exclude_list):
                collected.append(target)
        elif os.path.isdir(target):
            for root, dirs, files in os.walk(target):
                rel_root = os.path.relpath(root, ".")
                if should_exclude(rel_root, exclude_list):
                    dirs.clear()
                    continue
                dirs[:] = [d for d in dirs if not should_exclude(os.path.join(rel_root, d), exclude_list)]
                for f in files:
                    ext = os.path.splitext(f)[1]
                    if ext in VALID_C_EXTENSIONS:
                        f_path = os.path.join(root, f)
                        if not should_exclude(f_path, exclude_list):
                            collected.append(f_path)
    return collected


def main() -> int:
    parser = argparse.ArgumentParser(
        description="ALRIOS Banned-API Security Gate Scanner (MP-024)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Banned libc functions rejected:
  - strcpy, strcat, sprintf, vsprintf, gets, system, popen
"""
    )
    parser.add_argument(
        "paths",
        nargs="*",
        help="Source files or directories to scan (default: scan target paths)"
    )
    parser.add_argument(
        "--exclude", "-e",
        action="append",
        default=[],
        help="Directories or file paths to exclude from scanning"
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit violations report in JSON format"
    )
    parser.add_argument(
        "--quiet", "-q",
        action="store_true",
        help="Suppress informational output, emit only errors"
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        default=True,
        help="Fail closed: exit with code 1 if any banned API is found (default: enabled)"
    )

    args = parser.parse_args()

    # Default targets if none provided
    if not args.paths:
        default_candidates = ["src/commands/arcc", "src/ALRIOS", "src/installer", "src/daemons"]
        targets = [c for c in default_candidates if os.path.exists(c)]
        if not targets:
            targets = ["src"]
    else:
        targets = args.paths

    files_to_scan = collect_files(targets, args.exclude)

    all_violations: List[Dict[str, Any]] = []

    for fpath in files_to_scan:
        try:
            with open(fpath, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()
        except OSError as e:
            if not args.quiet:
                sys.stderr.write(f"[BANNED-API-GATE] Warning: Could not read {fpath}: {e}\n")
            continue

        violations = scan_c_source(fpath, content)
        all_violations.extend(violations)

    if args.json:
        result = {
            "scanned_files_count": len(files_to_scan),
            "violations_count": len(all_violations),
            "violations": all_violations,
            "status": "FAILED" if all_violations else "PASSED"
        }
        print(json.dumps(result, indent=2))
    else:
        for v in all_violations:
            sys.stderr.write(
                f"[BANNED-API-GATE] ERROR: {v['file']}:{v['line']}:{v['column']}: "
                f"Banned libc function '{v['api']}()' detected.\n"
                f"  Diagnostic: {v['reason']}\n"
                f"  Context: {v['context']}\n"
            )

        if all_violations:
            if not args.quiet:
                sys.stderr.write(
                    f"\n[BANNED-API-GATE] GATE FAILED: Found {len(all_violations)} prohibited "
                    f"banned-API violation(s) across {len(files_to_scan)} file(s).\n"
                )
        else:
            if not args.quiet:
                sys.stdout.write(
                    f"[BANNED-API-GATE] GATE PASSED: Scanned {len(files_to_scan)} file(s) with ZERO banned APIs.\n"
                )

    if all_violations and args.strict:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
