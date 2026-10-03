<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://cdn.alrigroup.com/ALRIDEV-SF-W.png">
    <source media="(prefers-color-scheme: light)" srcset="https://cdn.alrigroup.com/ALRIDEV-SF-B.png">
    <img alt="ALRI Development Logo" src="https://cdn.alrigroup.com/ALRIDEV-SF-W.png" width="300">
  </picture>
</p>

<h1 align="center">ALRIOS</h1>
<p align="center"><strong>Sovereign Operating System, Native Microkernel & High-Performance Microservice Platform</strong></p>

<p align="center">
  <a href="https://github.com/alrigroup/alrios/actions/workflows/build-installers.yml"><img src="https://img.shields.io/github/actions/workflow/status/alrigroup/alrios/build-installers.yml?branch=main&label=CI%2FCD%20Build%20%26%20Installers&style=flat-square" alt="CI/CD Status"></a>
  <img alt="Language" src="https://img.shields.io/badge/language-C%20%2F%20C%2B%2B-00599C?style=flat-square" />
  <img alt="Platform" src="https://img.shields.io/badge/platform-Linux%20x64%20%7C%20Windows%20x64-blueviolet?style=flat-square" />
  <img alt="License" src="https://img.shields.io/badge/license-ARGLP-green?style=flat-square" />
</p>

---

## ⚡ Overview

**ALRIOS** is an ultra-high-performance sovereign operating system kernel and native microservice platform engineered to handle **65,536 concurrent connections**, featuring **Upstream TCP Connection Pooling**, **Sharded In-Memory Cache**, and native modular containerization (`.arapp`).

This repository contains:
- **`arcore`**: The Sovereign OS Runtime Kernel & Microservices Supervisor.
- **`alrios`**: The Unified Management CLI and daemon orchestrator.
- **`arpm`**: The ALRIOS Sovereign Package Manager (`install`, `search`, `update`).
- **`armake`**: The Sovereign Application Bundler and compiler (`.arapp`).
- **Unified Master Installer**: Single-executable standalone installers for Linux & Windows with dual CLI and GUI support.

```
┌─────────────────┐       ┌──────────────────────────────────────────────────────┐
│     Browser     │──────▶│ ARWS Gateway (Reverse Proxy & Load Balancer)         │
│  (HTTP / HTTPS) │       │ ─ 65,536 Concurrent Connections, Upstream TCP Pool   │
│                 │       │ ─ 16 Shards Cache, Dynamic Rate Limiting & SSL       │
└─────────────────┘       └───────────┬──────────────┬──────────────┬────────────┘
                                      │              │              │
                   ┌──────────────────▼──┐    ┌──────▼───────┐ ┌────▼────────────┐
                   │ ARDB Database       │    │ ARCDN        │ │ ARWE (ALRI Web Engine) │
                   │ PGWire + HTTP Engine│    │ Static CDN   │ │ High-Perf Engine│
                   └─────────────────────┘    └──────────────┘ └─────────────────┘
```

---

## 🚀 Instant Installation (1-Click Standalone)

ALRIOS provides **single-executable standalone installers** for both Linux and Windows.  
**Zero dependencies required**: If your machine lacks compilers or runtimes (like GCC, Node.js, Python), the installer detects and auto-downloads everything on-the-fly!

### 🪟 Windows (x64)

Download the official executable from the [Releases](https://github.com/alrigroup/alrios/releases) page:
1. Run **`alrios-setup.exe`** to open the native graphical installer (Win32), or run in terminal:
   ```cmd
   alrios-setup.exe --cli -y
   ```
2. **Installation Directory**:
   - Administrator: `C:\Program Files\ALRIOS\`
   - Standard user: `%LOCALAPPDATA%\Programs\ALRIOS\`
3. **Ready!** The installer automatically registers ALRIOS in the Windows `PATH`. Open any CMD or PowerShell and run `alrios` or `arpm`.

### 🐧 Linux (x64)

Download the official executable from the [Releases](https://github.com/alrigroup/alrios/releases) page:
```bash
# 1. Grant execution permission
chmod +x alrios-installer-linux-x64

# 2. Run installer (Opens native GTK3 graphical wizard or interactive CLI)
./alrios-installer-linux-x64

# Or run non-interactively in quiet mode:
./alrios-installer-linux-x64 --cli -y
```
- **Installation Path**: `/opt/alrios/` (with sudo) or `~/.local/share/alrios/` (unprivileged user).
- **Global PATH Commands**: Creates symbolic links in `/usr/local/bin` (`alrios`, `arpm`, `arcore`, `armake`).

---

## 💻 Essential Commands

Once installed, the following commands are available globally in your terminal:

```bash
# Initialize the daemon supervisor and autostart microservices
alrios power on

# Inspect daemon health and active process table
alrios status

# Manage modular packages via ARPM
arpm list                    # List installed applications
arpm search <query>          # Search official registry (arcore/registry.json)
arpm install <app>           # Download and install a pre-built .arapp bundle
arpm update --all            # Update all applications preserving user storage

# Build application package (.arapp)
armake build <app_dir> arcore/apps/<app_name>.arapp

# Graceful shutdown of all services
alrios power off
```

---

## 📦 Modular Application Ecosystem

All official applications are decoupled into independent repositories and cataloged in [`arcore/registry.json`](arcore/registry.json):

| Repository / Package | Description |
|---|---|
| **[arws](https://github.com/alrigroup/arws)** | High-performance Layer 7 reverse proxy, gateway & load balancer |
| **[ardb](https://github.com/alrigroup/ardb)** | Sovereign linear database engine with PGWire v3.0 protocol and AST SQL firewall |
| **[arcdn](https://github.com/alrigroup/arcdn)** | Zero-copy static file delivery, HTTP 206 media streaming & edge asset server |
| **[arwe](https://github.com/alrigroup/arwe)** | In-memory `.arweb` container runtime, bundler & WebAssembly engine |
| **[arauth](https://github.com/alrigroup/arauth)** | Sovereign cryptographic identity vault, PBKDF2/Argon2id & session authority |
| **[ardcbot](https://github.com/alrigroup/ardcbot)** | Official Discord automation bot with sandboxed microkernel plugin runtime |

---

## 🛠️ Building from Source

To clone and compile the core platform locally:

### Linux
```bash
# System dependencies
sudo apt update && sudo apt install -y cmake gcc make libssl-dev libgtk-3-dev pkg-config nodejs npm

# Compilation
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --target arcore alrios armake arinstall -j$(nproc)

# Execute local installation
./install.sh
```

### Windows
```cmd
cmake -S . -B build-win -DCMAKE_BUILD_TYPE=Release
cmake --build build-win --config Release --target arcore alrios armake arinstall
install.bat
```

---

## 📚 Technical Documentation

In-depth technical architecture and operational specifications in **[`/docs`](docs/README.md)**:

- 🏆 **[Complete Features Catalog](docs/features.md)**
- 📘 **[Architecture Overview](docs/README.md)**
- 📙 **[Developer Guide & IPC](docs/DEVELOPER_GUIDE.md)**
- 📕 **[Production & Deployment Guide](docs/PRODUCTION.md)**
- 🟢 **[System Requirements](docs/REQUIREMENTS.md)**
- 📓 **[CLI Command Reference](docs/comands.md)**

---

## 🏢 Credits & Governance

- **Engineering & Architecture**: **[ALRI Development](https://alrigroup.com/)** *(Systems & Core Runtime Division)*
- **Holding & Asset Management**: **[ALRI Group](https://alrigroup.com/)** *(Parent Holding & License Proprietor)*
- **Licensing**: Governed by the **ARGLP (ALRI GROUP LICENSE PERMISSIVE - Version 2)**. See [LICENSE](LICENSE) for full terms.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://cdn.alrigroup.com/ARD-SF-W.png">
    <source media="(prefers-color-scheme: light)" srcset="https://cdn.alrigroup.com/ARD-SF-B.png">
    <img alt="ARD Seal" src="https://cdn.alrigroup.com/ARD-SF-W.png" width="80">
  </picture><br>
  <sub>© 2026 ALRI Group and its affiliates. Engineered and maintained by ALRI Development.</sub>
</p>
