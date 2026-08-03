# ALRIOS / arcore Documentation

**High-Speed Web and Microservices Architecture Powered by ALRI Web Services (ARWS).**

Developed and maintained by **[ALRIGROUP](https://alrigroup.com/)**.

---

## 📋 Architecture Overview

```
┌─────────────────┐       ┌────────────────────────────────────────────────────────┐
│     Browser     │──────▶│ ARWS Gateway (arws.arapp)                              │
│  (HTTP / HTTPS) │       │ ─ Ports: 8080 (HTTP) / 443 (HTTPS) / 9500 (IPC)        │
│                 │       │ ─ High-Speed: 65,536 Concurrent Connections            │
│                 │       │ ─ Sharded In-Memory Cache (16 Shards), Condvar Locks   │
│                 │       │ ─ Active Rate Limiting, HTTP Keep-Alive, TCP_NODELAY   │
└─────────────────┘       └───────────┬──────────────┬──────────────┬──────────────┘
                                      │              │              │
                   ┌──────────────────▼──┐    ┌──────▼───────┐ ┌────▼─────────────────┐
                   │ home-web.arapp      │    │ cdn.arapp    │ │ detroit-web.arapp    │
                   │ (React SPA + C)     │    │ (Static      │ │ (React SPA + C)      │
                   │ Port: 3001          │    │ Assets)      │ │ Port: 3004           │
                   │ Route: alrigroup.com│    │ Port: 3005   │ │ Route: detroitgg.com │
                   └─────────────────────┘    └──────────────┘ └──────────────────────┘
                                                             ┌────────────────────────┐
                                                             │ projetoliteratura-web  │
                                                             │ (React SPA + C)        │
                                                             │ Port: 3003             │
                                                             │ Route: /literature     │
                                                             └────────────────────────┘
```

---

## 🐧 Linux / WSL Installation & Setup Guide

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
cd arcore
./alrios power on
./alrios status
```

---

## 🪟 Windows Installation & Setup Guide

### 1. Requirements
- **Visual Studio 2022** (with C++ support) or **MSVC**.
- **CMake** (`3.20+`), **Node.js** (`18+`), and **Python 3**.

### 2. Build via MSVC Command Prompt
```cmd
build.bat
```

### 3. Test Application
```cmd
python test_suite.py
```

---

## 📂 Infrastructure Documentation

- **[Developer Guide & IPC Specification](DEVELOPER_GUIDE.md):** `.arapp` package creation and IPC protocol.
- **[Production & Deployment Guide](PRODUCTION.md):** Deployment manual for Debian/Linux servers and SSL configuration.
- **[Requirements & Dependencies](REQUIREMENTS.md):** Complete software dependencies list.
- **[ARKernel](KERNEL.md):** OS Hardware Abstraction Layer (`aros_hal.h`).
- **[Core Daemon (arcore.exe)](CORE.md):** Lifecycle manager and service runner.
- **[Tools armake & arinstall](TOOLS.md):** Package manager and runtime installer.
- **[alrios CLI Reference](comands.md):** Command-line interface reference guide.
- **[Changelog](CHANGELOG.md):** Performance optimization history.

### Native Applications (`/docs/apps/`)
- **[ARWS Gateway (arws.arapp)](apps/arws.md):** HTTP/HTTPS server and Reverse Proxy.
- **[CDN (cdn.arapp)](apps/cdn.md):** Media delivery and static asset server.
- **[Home Web (home-web.arapp)](apps/home.web.md):** React SPA application (Port 3001).
- **[Detroit Web (detroit-web.arapp)](apps/detroit.web.md):** React SPA application (Port 3004).
- **[Literature Project (projetoliteratura-web.arapp)](apps/projetoliteratura.web.md):** React SPA application (Port 3003).

---

## 🏢 Credits & License

- **Developed by:** **[ALRIGROUP](https://alrigroup.com/)**
- **Official Website:** [https://alrigroup.com/](https://alrigroup.com/)
- **License:** Governed by **ARGLR (ALRI GROUP LICENSE RESERVED - Version 1)**. Refer to [LICENSE](../LICENSE) for complete terms.
