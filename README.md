# ALRIOS / arcore

**High-Performance Microservices Architecture and Web Server Ecosystem developed by [ALRIGROUP](https://alrigroup.com/).**

---

## 📋 Overview

**ALRIOS** is an ultra-high-speed web server platform and native C microservice ecosystem capable of handling **65,536 concurrent connections**, featuring **Upstream TCP Connection Pooling**, **Sharded In-Memory Cache**, and native support for React/Vue SPA applications.

```
┌─────────────────┐       ┌────────────────────────────────────────────────────────┐
│     Browser     │──────▶│ ARWS Gateway (arws.arapp)                              │
│  (HTTP / HTTPS) │       │ ─ Ports: 8080 (HTTP) / 443 (HTTPS) / 9500 (IPC)        │
│                 │       │ ─ 65,536 Concurrent Connections, Upstream Pool        │
│                 │       │ ─ Sharded Cache (16 shards), Active Rate Limiting      │
└─────────────────┘       └───────────┬──────────────┬──────────────┬──────────────┘
                                      │              │              │
                   ┌──────────────────▼──┐    ┌──────▼───────┐ ┌────▼─────────────────┐
                   │ home-web.arapp      │    │ cdn.arapp    │ │ detroit-web.arapp    │
                   │ Port: 3001          │    │ Port: 3005   │ │ Port: 3004           │
                   └─────────────────────┘    └──────────────┘ └──────────────────────┘
```

---

## 🐧 Linux / WSL Installation & Setup Guide

### 1. Install System Dependencies
On Debian, Ubuntu, or WSL2 (Ubuntu):
```bash
sudo apt update && sudo apt install -y cmake gcc make libssl-dev pkg-config nodejs npm curl
```

### 2. Clone Repository & Navigate to Directory
```bash
git clone https://github.com/ALRIGROUP/alrios.git
cd alrios
```

### 3. Build Entire Ecosystem (Kernel + Tools + Apps)
```bash
bash build_linux.sh
```

### 4. Start Server
```bash
cd arcore
./alrios power on
```

### 5. Check Service Status
```bash
./alrios status
```

---

## 🪟 Windows Installation & Setup Guide

### 1. Requirements
- **Visual Studio 2022** (with *Desktop Development with C++* workload) or **MSVC**.
- **CMake** (`3.20` or higher).
- **Node.js** (`18` or higher) and **Python 3**.

### 2. Build via Visual Studio Command Prompt (x64 Native Tools Command Prompt)
```cmd
build.bat
```

### 3. Launch Development Server
```cmd
python serve.py
```

### 4. Run End-to-End Integration Test Suite
```cmd
python test_suite.py
```

---

## 📂 Complete Documentation Suite

All technical documentation is centralized in **[`/docs`](docs/README.md)**:

- 📘 **[System Overview](docs/README.md)**
- 📙 **[Developer Guide & IPC Specification](docs/DEVELOPER_GUIDE.md)**
- 📕 **[Production Deployment & Operations Guide](docs/PRODUCTION.md)**
- 🟢 **[System Requirements & Dependencies](docs/REQUIREMENTS.md)**
- 📒 **[ARKernel Technical Specification](docs/KERNEL.md)**
- 📕 **[arcore Daemon Technical Specification](docs/CORE.md)**
- 📓 **[alrios CLI Command Reference](docs/comands.md)**
- 📑 **[High-Speed Optimizations Changelog](docs/CHANGELOG.md)**
- 📱 **[Native Applications (ARWS, CDN, Home Web, Detroit Web, Literature)](docs/apps/arws.md)**

---

## 🏢 Credits

Developed and maintained by **[ALRIGROUP](https://alrigroup.com/)**.

- **Official Website:** [https://alrigroup.com/](https://alrigroup.com/)
- **Support & Infrastructure:** ALRI GROUP Engineering Team

---

## 📄 License

This project is governed by the **ARGLR (ALRI GROUP LICENSE RESERVED - Version 1)** license. Usage, distribution, and modifications are strictly restricted to authorized users. Please refer to the **[LICENSE](LICENSE)** file for complete terms and conditions.
