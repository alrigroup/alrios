<p align="center">
  <img src="https://raw.githubusercontent.com/alrigroup/.github/main/alrigroup.svg" width="140" />
</p>

<h1 align="center">ALRIOS</h1>
<p align="center"><strong>High-Performance Native Operating System & Microservice Platform</strong></p>
<p align="center">
  <img alt="Language" src="https://img.shields.io/badge/language-C-00599C?style=flat-square" />
  <img alt="Platform" src="https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey?style=flat-square" />
  <img alt="License" src="https://img.shields.io/badge/license-ARGLR--v1-red?style=flat-square" />
</p>

---

## Overview

**ALRIOS** is an ultra-high-performance web server platform and native C microservice ecosystem capable of handling **65,536 concurrent connections**, featuring **Upstream TCP Connection Pooling**, **Sharded In-Memory Cache**, and native support for React/Vue SPA applications.

This repository contains the **Core OS Kernel**, **SDK**, and **CLI tools** (`alrios`, `armake`, `arinstall`, `arcreate`). Each application and service runs in its own standalone repository.

```
┌─────────────────┐       ┌──────────────────────────────────────────────────────┐
│     Browser     │──────▶│ ARWS Gateway                                        │
│  (HTTP / HTTPS) │       │ ─ 65,536 Concurrent Connections, Upstream Pool       │
│                 │       │ ─ Sharded Cache (16 shards), Active Rate Limiting    │
└─────────────────┘       └───────────┬──────────────┬──────────────┬────────────┘
                                      │              │              │
                   ┌──────────────────▼──┐    ┌──────▼───────┐ ┌────▼────────────┐
                   │ ARDB Database       │    │ ARCDN        │ │ ARWN Web Native │
                   │ PGWire + HTTP API   │    │ Static CDN   │ │ Compiler        │
                   └─────────────────────┘    └──────────────┘ └─────────────────┘
```

---

## Ecosystem

ALRIOS is a modular platform. Each component lives in its own repository:

### 🌐 Public Repositories (Open Source)

| Repository | Description |
|---|---|
| **[alrios](https://github.com/alrigroup/alrios)** | Core OS Kernel, SDK, CLI tools (`alrios`, `armake`, `arinstall`) |
| **[arws](https://github.com/alrigroup/arws)** | High-performance reverse proxy, load balancer & stream proxy |
| **[ardb](https://github.com/alrigroup/ardb)** | Native linear database engine with PGWire protocol |
| **[arcdn](https://github.com/alrigroup/arcdn)** | Native static file server & CDN |
| **[arwn](https://github.com/alrigroup/arwn)** | Web Native compiler, bundler & runtime |
| **[arapiauth](https://github.com/alrigroup/arapiauth)** | OAuth2/OIDC authentication gateway |

### 🔒 Private Repositories

Additional enterprise applications and client portals are maintained in private repositories within the `alrigroup` organization.

---

## Quick Start

### Linux / WSL

```bash
# 1. Install dependencies
sudo apt update && sudo apt install -y cmake gcc make libssl-dev pkg-config nodejs npm curl

# 2. Clone with public submodules
git clone --recurse-submodules https://github.com/alrigroup/alrios.git
cd alrios

# 3. Build the entire ecosystem
bash build_linux.sh

# 4. Start
cd arcore
./alrios power on

# 5. Check status
./alrios status
```

### Windows

Requires **Visual Studio 2022** (C++ workload), **CMake 3.20+**, **Node.js 18+**, **Python 3**.

```cmd
build.bat
python serve.py
```

---

## Documentation

All technical documentation is in **[`/docs`](docs/README.md)**:

- 🏆 **[Complete Features Catalog](docs/features.md)**
- 📘 **[System Overview](docs/README.md)**
- 📙 **[Developer Guide & IPC Specification](docs/DEVELOPER_GUIDE.md)**
- 📕 **[Production Deployment Guide](docs/PRODUCTION.md)**
- 🟢 **[System Requirements](docs/REQUIREMENTS.md)**
- 📒 **[ARKernel Specification](docs/KERNEL.md)**
- 📕 **[arcore Daemon Specification](docs/CORE.md)**
- 📓 **[CLI Command Reference](docs/comands.md)**
- 📑 **[Changelog](docs/CHANGELOG.md)**

---

## Credits

Developed and maintained by **[ALRI Group](https://alrigroup.com/)**.

---

## License

This project is governed by the **ARGLR (ALRI GROUP LICENSE RESERVED - Version 1)** license. See **[LICENSE](LICENSE)** for full terms.
