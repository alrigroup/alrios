# AGENTS.md — Autonomous AI Agent Operating Protocol & Technical Invariants

> **Target Audience**: Autonomous AI Agents (Antigravity, Claude Code, Cursor, Copilot) & Systems Kernel Engineers.  
> **Repository**: `alrios` (Sovereign Operating System Kernel, HAL & Native Microservice Platform)  
> **Visibility**: Public Open-Core  
> **Asset Owner**: ALRI Group | **Engineering**: ALRI Development  
> **License**: ARGLP (ALRI Group License Permissive — Version 2)  
> **Primary Technical Reference**: Consult [`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md), [`docs/features.md`](docs/features.md), and [`docs/KERNEL.md`](docs/KERNEL.md).

---

## 1. Project Mission & Identity

**ALRIOS** is an ultra-high-performance sovereign operating system kernel, hardware abstraction layer (HAL), and native microservice execution platform written in C11. It provides centralized process supervision (`arcore`), native containerization (`.arapp`), a package manager (`arpm`), and a cross-platform compilation toolchain (`armake`).

### Core Architectural Specifications
- **Supervisor Daemon (`arcore`)**: Spawns, monitors, and restarts infrastructure services (`arws`, `ardb`, `arcdn`) and user applications with self-healing state machines (`UNREGISTERED` -> `STOPPED` -> `STARTING` -> `RUNNING` -> `CRASHED` -> `RECOVERING`).
- **Operating Ports**: Port `9500` (IPC Server for daemon control, route registration, and CLI queries) and Port `9600`.
- **System HAL (`src/ALRIOS/arkernel/`)**: Isolates low-level Win32 vs. Linux POSIX code behind unified APIs in `aros_hal.h`.
- **Application Staging (`arcore/.staging/`)**: Automatically extracts `.arapp` packages into isolated runtime sandboxes verifying `ALRIGROUP@APPMAKE` manifests before execution.

---

## 2. Directory Structure & Key Subsystems

```
alrios/
├── CMakeLists.txt            # Master CMake build file
├── build_linux.sh            # Full Linux Release compilation script
├── build.bat                 # MSVC compilation script for Windows
├── alrios.service            # Systemd service unit descriptor
├── serve.py                  # Developer helper / diagnostic server
├── docs/                     # Comprehensive system documentation
│   ├── KERNEL.md             # ARKernel HAL specification
│   ├── CORE.md               # arcore supervisor lifecycle daemon manual
│   ├── DEVELOPER_GUIDE.md    # Master developer guide and IPC specification
│   ├── features.md           # Exhaustive technical features catalog
│   └── PRODUCTION.md         # Deployment and production operations guide
├── src/
│   ├── ALRIOS/
│   │   ├── arkernel/         # OS HAL (os/linux, os/windows, process, socket, sync)
│   │   └── core/             # arcore supervisor daemon, loader, ctl, arapp_parser
│   ├── tools/                # alrios CLI, armake packager, arpm package manager
│   └── installer/            # Native runtime installer routines
└── arcore/                   # Target staging directory for compiled binaries
    ├── arcore                # Master supervisor binary
    ├── alrios                # Unified management CLI
    ├── armake                # Package compiler and extractor
    ├── arpm -> alrios        # Package manager symlink
    └── apps/                 # Generated .arapp application bundles
```

---

## 3. Essential Commands & Toolchain Invariants

### 3.1 Full System Compilation (Linux x64)
```bash
# Compiles kernel, HAL, tools, and builds all modular .arapp applications
bash build_linux.sh
```

### 3.2 Manual CMake Invocation
```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --target arcore alrios armake -- -j$(nproc)
```

### 3.3 Lifecycle & Governance via `alrios` CLI
```bash
./alrios power on             # Boots arcore daemon, mounts ARWS, and starts auto-start apps
./alrios status               # Displays PID table, ports, and operational health
./alrios power reload         # Zero-downtime hot reload of packages, routes, and configs
./alrios list                 # Lists all installed .arapp applications
./alrios start <app>          # Starts a specific application process
./alrios stop <app>           # Gracefully stops an application process
./alrios power off            # Terminates all processes cleanly
```

### 3.4 Packaging Applications via `armake`
```bash
# Pack directory into sealed .arapp container
./arcore/armake pack src/apps/<app_dir> arcore/apps/<app_name>.arapp

# Extract .arapp package to destination directory
./arcore/armake extract arcore/apps/<app_name>.arapp /tmp/extracted_app

# Inspect package manifest and file table
./arcore/armake list arcore/apps/<app_name>.arapp
```

---

## 4. Architectural Rules & The "NEVER" List

Autonomous AI Agents operating within this codebase must strictly observe these inviolable rules:

### 4.1 Strict Prohibitions
- ❌ **NEVER add Git submodules**: Applications are decoupled independent repositories located under `github.com/alrigroup/<app>`. Submodules in `alrios` are strictly forbidden.
- ❌ **NEVER commit build outputs or generated packages**: Block `build/`, `build-linux/`, `.staging/`, `arcore/apps/*.arapp`, and `lib/*.a`.
- ❌ **NEVER bypass the AROS HAL in application code**: Direct OS calls like `fork()`, `execvp()`, `CreateProcessW()`, `pthread_create()`, or `CreateThread()` are prohibited outside `src/ALRIOS/arkernel/os/`. Always use `aros_hal.h`.
- ❌ **NEVER introduce blocking socket operations without timeouts**: Network sockets must enforce `O_NONBLOCK` and evaluate granular timeouts to prevent thread exhaustion.
- ❌ **NEVER ignore return codes from HAL memory or process routines**: Always check return values of `ar_process_spawn()` and `ar_socket_create()`.

---

## 5. Code Style & Engineering Standards

### 5.1 Correct vs. Incorrect Implementations

#### Process Management via AROS HAL
```c
/* FORBIDDEN: Direct OS call breaking cross-platform compatibility */
#ifdef __linux__
pid_t pid = fork();
if (pid == 0) execvp(cmd, args);
#endif

/* CORRECT (ALRIOS Standard): Unified HAL abstraction */
ar_process_t *proc = NULL;
int status = ar_process_spawn(executable_path, argv, &proc);
if (status != 0 || !proc) {
    ar_log_error("Failed to spawn process: %s", executable_path);
    return -1;
}
```

#### IPC 5-Byte Binary Framing Protocol
```c
/* CORRECT (ALRIOS Standard): 5-Byte framing [4B Length BE][1B Type][Payload] */
uint32_t payload_len = (uint32_t)strlen(payload);
uint32_t len_be = htonl(payload_len);
uint8_t msg_type = IPC_TYPE_REGISTER_ROUTE;

write(fd, &len_be, 4);
write(fd, &msg_type, 1);
write(fd, payload, payload_len);
```

### 5.2 Mandatory Copyright Header
Every new C source or header file created must begin with:
```c
/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
```

---

## 6. Pre-Commit & Pull Request Verification Checklist

Before submitting changes, the agent must verify:
1. Full build succeeds via `bash build_linux.sh` with zero compilation warnings.
2. The repository working tree is 100% clean (`git status -s` shows no untracked build artifacts).
3. No `.env`, secret keys, or `.arapp` binary bundles are staged.
4. Any change to HAL interfaces (`aros_hal.h`) maintains full binary and syntactic parity across both `os/linux/` and `os/windows/`.
5. Git commits adhere to Conventional Commits with the mandatory trailer:
   `Signed-off-by: ALRI Development <dev@alrigroup.com>`.
