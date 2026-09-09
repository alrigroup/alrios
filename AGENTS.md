# AGENTS.md — AI Agent Guidance & Repository Rules

> **Target Audience**: Autonomous AI Agents (Antigravity, Claude Code, Cursor, Copilot) & Systems Engineers.  
> **Repository**: `alrios` (Sovereign Operating System & Core Microservice Platform)  
> **Visibility**: Public Open-Core  
> **Asset Owner**: ALRI Group | **Engineering**: ALRI Development  
> **License**: ARGLP (ALRI Group License Permissive — Version 2)  

---

## 1. Project Mission & Identity

**ALRIOS** is the sovereign operating system kernel, hardware abstraction layer (HAL), and microservice platform engineered in C11. It provides process supervision (`arcore`), native packaging (`armake`), dynamic service management (`alrios`), and package registry operations (`arpm`).

- **Core Daemon (`arcore`)**: Central supervisor managing lifecycle state machines, `.arapp` staging extraction, and IPC routing on TCP ports `9500` (IPC control) and `9600`.
- **System HAL (`src/ALRIOS/arkernel`)**: Cross-platform abstractions for processes, sockets, threads, and mutexes.
- **Reference**: Consult [`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md) and [`docs/features.md`](docs/features.md) for full technical schemas.

---

## 2. Essential Commands

### Build from Source (Linux x64)
```bash
# Full ecosystem build (kernel, tools, and apps)
bash build_linux.sh
```

### Build from Source (Windows x64 MSVC)
```cmd
build.bat
```

### Verification & Status
```bash
./alrios power on       # Boot supervisor and configured services
./alrios status         # Inspect daemon health and active process table
./alrios list           # List installed .arapp applications
./alrios power off      # Graceful shutdown of all services
```

---

## 3. Strict Prohibitions for AI Agents (The "NEVER" List)

- ❌ **NEVER commit Git submodules**: Applications are independent repositories; submodules are strictly banned.
- ❌ **NEVER commit build residues**: Block `build/`, `build-linux/`, `.staging/`, `arcore/apps/*.arapp`, or `lib/*.a`.
- ❌ **NEVER bypass AROS HAL**: Direct calls to `CreateProcessW`, `fork()`, `pthread_create()` are prohibited in application code. Use `aros_hal.h` (`ar_process_*`, `ar_socket_*`, `ar_sync_*`).
- ❌ **NEVER use blocking socket calls without timeouts**: Sockets must enforce non-blocking modes (`O_NONBLOCK`) with explicit timeouts.
- ❌ **NEVER introduce unvetted external C libraries**: Rely exclusively on OpenSSL, POSIX/Win32 HAL, and internal C libraries.

---

## 4. Code Style & Architectural Invariants

- **Language Standard**: Strict C11 (`-std=c11 -O2 -Wall -Wextra -Werror`).
- **IPC Protocol**: All inter-process communication with `arcore` follows the 5-byte framing:
  `[4B Length Big-Endian][1B Type Opcode][Payload]`.
- **Compulsory File Header**: Every source file must begin with:
  ```c
  /*
   * Copyright (c) 2026 ALRIGROUP and its affiliates.
   * Engineered and maintained by ALRI Development.
   *
   * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
   * found in the LICENSE file in the root directory of this source tree.
   */
  ```

---

## 5. Git Commit Protocol

- Conventional Commits enforced (`feat`, `fix`, `docs`, `refactor`, `perf`, `security`, `chore`).
- Mandatory trailer: `Signed-off-by: ALRI Development <dev@alrigroup.com>`.
