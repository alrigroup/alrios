# ALRIOS / arcore Documentation

**High-Performance Native Operating System & Microservice Platform Powered by ARWS.**

Developed and maintained by **[ALRIGROUP](https://alrigroup.com/)**.

---

## Architecture Overview

```
┌─────────────────┐       ┌────────────────────────────────────────────────────────┐
│     Browser     │──────▶│ ARWS Gateway (arws.arapp)                              │
│  (HTTP / HTTPS) │       │ - Ports: 8080 (HTTP) / 443 (HTTPS) / 9500 (IPC)       │
│                 │       │ - 65,536 Concurrent Connections                        │
│                 │       │ - Sharded In-Memory Cache (16 Shards)                  │
└─────────────────┘       └───────────┬──────────────┬──────────────┬──────────────┘
                                      │              │              │
                   ┌──────────────────▼──┐    ┌──────▼───────┐ ┌────▼─────────────┐
                   │ arwe web apps       │    │ arcdn        │ │ ardb             │
                   │ (React SPA + ARWE)  │    │ Static CDN   │ │ Database Engine  │
                   └─────────────────────┘    └──────────────┘ └──────────────────┘
```

---

## Linux / WSL Installation & Setup Guide

### 1. Install System Dependencies
```bash
sudo apt update && sudo apt install -y cmake gcc make libssl-dev pkg-config nodejs npm curl
```

### 2. Build Ecosystem
```bash
bash build_linux.sh
```

### 3. Start & Verify Server
```bash
./alrios power on
./alrios status
```

---

## Windows Installation & Setup Guide

### 1. Requirements
- **Visual Studio 2022** (with C++ support) or **MSVC**.
- **CMake** (`3.20+`), **Node.js** (`18+`), and **Python 3**.

### 2. Build via MSVC Command Prompt
```cmd
build.bat
```

---

## Documentation

- **[Developer Guide & IPC Specification](DEVELOPER_GUIDE.md):** `.arapp` package creation and IPC protocol.
- **[Complete Features Catalog](features.md):** Full technical catalog of all core subsystems.
- **[Production & Deployment Guide](PRODUCTION.md):** Deployment manual for Debian/Linux servers.
- **[System Requirements](REQUIREMENTS.md):** Complete software dependencies list.
- **[ARKernel Specification](KERNEL.md):** OS Hardware Abstraction Layer.
- **[Core Daemon (arcore)](CORE.md):** Lifecycle manager and service runner.
- **[Developer Tools (armake & arinstall)](TOOLS.md):** Package manager and runtime installer.
- **[CLI Command Reference](comands.md):** Command-line interface reference guide.
- **[Recommended Applications](RECOMMENDED_APPS.md):** List of public apps with descriptions and install instructions.

---

## 🏢 Credits & Governance

- **Engineering & Architecture:** **[ALRI Development](https://alrigroup.com/)** *(Systems & Core Runtime Division)*
- **Holding & Asset Management:** **[ALRI Group](https://alrigroup.com/)** *(Parent Holding Company)*
- **Official Website:** [https://alrigroup.com/](https://alrigroup.com/)
- **License:** Governed by **ARGLP (ALRI GROUP LICENSE PERMISSIVE - Version 2)**. See [LICENSE](../LICENSE) for full terms.

<p align="center">© 2026 ALRI Group and its affiliates. Engineered by ALRI Development.</p>
