# ALRIOS Master Developer & Application Architecture Guide

**The Complete Manual for Engineering, Packaging, and Deploying High-Performance Applications in ALRIOS.**

Developed and maintained by **[ALRIGROUP](https://alrigroup.com/)**.

---

## 📑 Table of Contents
1. [Core Philosophy & Operating Concept](#1-core-philosophy--operating-concept)
2. [Global Architecture Overview](#2-global-architecture-overview)
3. [Native Application Specifications (`.arapp`)](#3-native-application-specifications-arapp)
4. [ALRI Web Native (ARWN & `.arweb` Containers)](#4-alri-web-native-arwn--arweb-containers)
5. [Routing, Gateway & Proxy (`ARWS`)](#5-routing-gateway--proxy-arws)
6. [MANDATORY Rules & Standards for Developers](#6-mandatory-rules--standards-for-developers)
7. [Step-by-Step Tutorial: Building a Complete App from Scratch](#7-step-by-step-tutorial-building-a-complete-app-from-scratch)
8. [Ecosystem Applications Breakdown](#8-ecosystem-applications-breakdown)
9. [CLI & Lifecycle Reference](#9-cli--lifecycle-reference)

---

## 1. Core Philosophy & Operating Concept

**ALRIOS** is an ultra-fast, zero-overhead application runtime, reverse proxy gateway, and execution kernel written in high-performance C. 

### Why ALRIOS?
- **Zero Runtime Disk Lookups**: Web applications and WASM micro-units are compiled into structured binary containers (`.arweb`) and served directly from RAM.
- **Hardware-Level Performance**: Direct memory buffers, SIMD vectorization, and raw pointers without garbage collection stalls.
- **Strict IP Protection**: Integrated multi-pass Base64 VM obfuscation and mandatory licensing injection on all web units.
- **Unified IPC & Service Supervision**: All system services (`arws`, `cdn`) and web apps (`home-web`, `detroit-web`, `test_ecosystem.web`) are managed by a centralized supervision daemon (`arcore` / `alrios`).

---

## 2. Global Architecture Overview

```
                      ┌───────────────────────────────┐
                      │    Client Browser / Agent     │
                      └──────────────┬────────────────┘
                                     │ HTTP (8080) / HTTPS (443)
                      ┌──────────────▼────────────────┐
                      │    ARWS Routing Gateway       │
                      │  (65k Conns, 16 Cache Shards) │
                      └──────────────┬────────────────┘
                                     │ IPC / Upstream Loopback (9500)
    ┌────────────────────────────────┼────────────────────────────────┐
    │                                │                                │
┌───▼────────────────────────┐ ┌─────▼──────────────────────┐ ┌───────▼──────────────────────┐
│ Legacy / SPA Native Apps   │ │ ARWN Native Web Framework  │ │ Static Delivery System       │
│ - home-web (3001)          │ │ - test_ecosystem.web(3055) │ │ - cdn (3005)                 │
│ - detroit-web (3004)       │ │   ├── c_engine.arweb (WASM)│ │   ├── Assets, Media, Videos  │
│ - projetoliteratura (3003) │ │   ├── cpp_engine.arweb     │ │   └── Zero-Copy Sendfile     │
│   (Vite + React + C Serve) │ │   ├── rust_engine.arweb    │ │                              │
│                            │ │   └── main.arweb (Base64VM)│ │                              │
└────────────────────────────┘ └────────────────────────────┘ └──────────────────────────────┘
```

---

## 3. Native Application Specifications (`.arapp`)

Every service or app in ALRIOS is distributed as a sealed `.arapp` package (an uncompressed or ZIP-structured tar archive created via `armake`).

### Directory Layout
```
src/apps/my_app.web/
├── my_app.web.arappmake   # Application manifest (ALRIGROUP@APPMAKE)
├── CMakeLists.txt         # Build rules for CMake
├── my_app_server.c        # Native server logic (C / POSIX / HAL)
├── config.arwn            # (Optional) ARWN Multi-Unit configuration
└── web/                   # Frontend assets & components
    ├── main.arhtml        # Mandatory standard entrypoint
    ├── main.js            # JavaScript frontend logic
    └── main.css           # Styling & design system
```

### Manifest Format (`*.arappmake`)
The manifest MUST begin with the exact magic string `ALRIGROUP@APPMAKE` followed by valid JSON:

```json
ALRIGROUP@APPMAKE
{
  "name": "my-service",
  "version": "1.0.0",
  "type": "app",
  "runtime": "native",
  "executable": "my_service_bin",
  "routes": [
    {
      "host": "myservice.localhost",
      "path": "/*",
      "target": "http://127.0.0.1:3090"
    }
  ]
}
```

- **`type`**: `"app"` (standard application) or `"svc"` (essential background service).
- **`runtime`**: `"native"` (compiled C/C++/Rust binary) or `"node"` / `"python3"`.
- **`routes`**: Direct proxy rules consumed by the ARWS gateway upon startup.

---

## 4. ALRI Web Native (ARWN & `.arweb` Containers)

ARWN is the modern standard for web apps in ALRIOS. It transforms frontend and WASM backend code into binary `.arweb` containers.

### Binary Layout (`.arweb`)
```
┌──────────────────────────────────────────────────────────┐
│ Magic Header: 0x4E575241 ('ARWN') | Version | Sections  │
├──────────────────────────────────────────────────────────┤
│ Section Table (48 Bytes per Section):                    │
│   - Name (32 Bytes NULL-terminated)                      │
│   - Offset (uint32)                                      │
│   - Size (uint32)                                        │
│   - CRC32 Checksum (uint32)                              │
├──────────────────────────────────────────────────────────┤
│ Data Payloads:                                           │
│   - app.html       -> Raw Entrypoint HTML                │
│   - main.js        -> Base64 VM Encapsulated JS          │
│   - main.css       -> Compiled Minified CSS              │
│   - mod/*.wasm     -> Linear Memory WASM Micro-Engines   │
└──────────────────────────────────────────────────────────┘
```

### Configuration Format (`config.arwn`)
```ini
[app]
name=my-app
port=3055
bind=127.0.0.1
copyright=Copyright (c) 2026 Developer Name. All rights reserved.

[arws]
gateway=127.0.0.1:9500
route.host=myapp.localhost
route.path=/*
route.mode=production

[unit:main]
source=web/
entry=main.arhtml
compile=main.js
compile.lang=js
obfuscate=yes
copyright=Proprietary Frontend Core - Unauthorized copying prohibited.

[unit:native_calc]
source=units/c
compile=calc.wasm
compile.lang=c
obfuscate=yes
```

---

## 5. Routing, Gateway & Proxy (`ARWS`)

The **ARWS (ALRI Web Services)** gateway sits in front of all services.

- **Port 8080**: HTTP Entrypoint / Development Gateway.
- **Port 443**: Production HTTPS with SSL/TLS termination.
- **Port 9500**: IPC Control Channel for dynamic route registration and heartbeat queries.

### IPC Protocol Specification (5-Byte Framing)
```
┌─────────────────┬───────────────────────────────┬─────────────────────────┐
│ 1 Byte (Type)   │ 4 Bytes (Payload Length - BE) │ Payload (JSON / String) │
└─────────────────┴───────────────────────────────┴─────────────────────────┘
```

| Type ID | Command Constant | Description |
| :--- | :--- | :--- |
| `1` | `IPC_TYPE_PING` | Heartbeat keep-alive check |
| `2` | `IPC_TYPE_LIST` | Query active applications and process states |
| `3` | `IPC_TYPE_START` | Spin up an application process |
| `4` | `IPC_TYPE_STOP` | Graceful shutdown of a running service |
| `6` | `IPC_TYPE_REGISTER_ROUTE` | Dynamically attach a virtual host / path to an upstream |
| `12` | `IPC_TYPE_CACHE_CLEAR` | Invalidate all 16 memory cache shards |

---

## 6. MANDATORY Rules & Standards for Developers

When contributing to or developing for ALRIOS, you **MUST** strictly adhere to the following rules:

### 1. File Naming Standard ("MAIN" Convention)
All frontend assets in an ARWN unit must be strictly isolated into separate files:
- ❌ **NEVER** write inline `<style>` or `<script>` tags inside HTML.
- ✅ Entrypoint HTML must always be named **`main.arhtml`**.
- ✅ Main JavaScript must always be named **`main.js`**.
- ✅ Main Stylesheet must always be named **`main.css`**.

### 2. IP Protection & Obfuscation
- When `obfuscate=yes` is set in `config.arwn`, the compilation toolchain wraps JavaScript into a self-executing Base64 Virtual Machine payload.
- All files automatically receive the **Official ALRI GROUP Free Use Header** alongside the developer's **Custom Copyright**.

### 3. Concurrency & Non-Blocking I/O
- HTTP services must use the **Multi-Worker Thread Pool** pattern with `ar_mutex` and `ar_cond` synchronization.
- **NEVER** block on synchronous file I/O during request handling; serve all web payloads directly from linear memory buffers.

### 4. Git & Workspace Cleanliness
- **NEVER** commit build folders (`build/`, `dist/`, `.staging/`), compiled `.arapp` packages, or runtime logs to Git.
- Always verify repository cleanliness using `git status -s`.

---

## 7. Step-by-Step Tutorial: Building a Complete App from Scratch

Let's build a new service called **`analytics.web`** in C with a React/WASM frontend.

### Step 1: Create Directory Tree
```bash
mkdir -p src/apps/analytics.web/web
mkdir -p src/apps/analytics.web/units/c
```

### Step 2: Write Native WASM Logic (`units/c/calc.c`)
```c
#include <stdint.h>

static uint32_t memory_buffer[65536];

uint32_t compute(uint32_t iters) {
    uint32_t acc = 0x12345678;
    for (uint32_t i = 0; i < iters; i++) {
        uint32_t idx = i & 0xFFFF;
        acc = ((acc ^ i) * 1664525) + 1013904223;
        memory_buffer[idx] = acc;
    }
    return acc;
}
```

### Step 3: Write Frontend Files

**`web/main.arhtml`**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>ALRIOS Analytics</title>
  <link rel="stylesheet" href="main.css">
  <script src="https://unpkg.com/react@18/umd/react.production.min.js"></script>
  <script src="https://unpkg.com/react-dom@18/umd/react-dom.production.min.js"></script>
</head>
<body>
  <div id="root"></div>
  <script src="main.js"></script>
</body>
</html>
```

**`web/main.css`**:
```css
body {
  margin: 0;
  background: #020617;
  color: #f8fafc;
  font-family: 'Inter', sans-serif;
}
```

**`web/main.js`**:
```javascript
function App() {
  return React.createElement('h1', null, 'ALRIOS Analytics Running Native!');
}

function init() {
  const root = ReactDOM.createRoot(document.getElementById('root'));
  root.render(React.createElement(App));
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
```

### Step 4: Write `config.arwn`
```ini
[app]
name=analytics
port=3090
bind=127.0.0.1
copyright=Copyright (c) 2026 ALRIGROUP.

[arws]
gateway=127.0.0.1:9500
route.host=analytics.localhost
route.path=/*

[unit:main]
source=web/
entry=main.arhtml
compile=main.js
compile.lang=js
obfuscate=yes

[unit:calc]
source=units/c
compile=calc.wasm
compile.lang=c
obfuscate=yes
```

### Step 5: Build & Deploy
```bash
# 1. Compile binaries and package .arapp
cmake -B build -S . && cmake --build build

# 2. Package application archive
./arcore/armake pack src/apps/analytics.web arcore/apps/analytics.web.arapp

# 3. Reload ALRIOS Daemon
./arcore/alrios power reload
./arcore/alrios status
```

---

## 8. Ecosystem Applications Breakdown

| Application | Port | Virtual Host | Architecture | Description |
| :--- | :--- | :--- | :--- | :--- |
| **`arws`** | 8080 / 443 | `*` | Native C / Multi-Shard Cache | Central Reverse Proxy & Load Balancer |
| **`cdn`** | 3005 | `cdn.localhost` | Native C / Sendfile | Static Asset & Media Streaming Server |
| **`home.web`** | 3001 | `alrigroup.com` | React 18 / C Backend | Official ALRIGROUP Corporate Portal |
| **`detroit.web`** | 3004 | `detroitgg.com` | React 18 / Tailwind | Gaming Community Portal & Leaderboards |
| **`projetoliteratura.web`** | 3003 | `localhost/literature` | React 18 / UI Suite | Machado de Assis Digital Literature Archive |
| **`test_ecosystem.web`** | 3055 | `ecosystem.localhost` | ARWN / WASM Multi-Engine | Multi-Language Linear Memory Benchmark (C, C++, Rust, Go, JS) |

---

## 9. CLI & Lifecycle Reference

The `alrios` CLI is the control tool for managing the entire operating system:

```bash
# Power Lifecycle
alrios power on        # Boot kernel, ARWS gateway, and all services
alrios power off       # Terminate all processes gracefully
alrios power reload    # Zero-downtime hot reload of packages & routes

# Monitoring & Status
alrios status          # Display PID table and daemon health
alrios list            # List all installed .arapp applications
alrios apps            # View active routing table

# Package Management (armake)
armake pack <dir> <out.arapp>     # Build and seal an .arapp container
armake extract <in.arapp> <dir>   # Unpack an .arapp archive
armake info <in.arapp>            # Inspect headers and manifest
```

---

## 🏢 Credits & Governance
- **Architecture & Design**: **[ALRIGROUP Core Team](https://alrigroup.com/)**
- **Licensing**: Governed by the **ARGLR (ALRI GROUP LICENSE RESERVED)** and **ARGLFU (ALRI GROUP LICENSE FREE USE)**.
