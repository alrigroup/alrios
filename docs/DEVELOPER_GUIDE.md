# ALRIOS / arcore — Developer Guide

Guide for creating applications, writing manifests (`.arappmake`), packaging `.arapp` files, using IPC APIs, and integrating with `arws`.

---

## 📋 1. Application Directory Structure

Each application resides under `src/apps/<app_name>/`:

```
src/apps/home.web/
├── manifest.arappmake      # Package manifest (JSON)
├── CMakeLists.txt         # Native C compilation rules
├── home_server.c          # Native HTTP backend entry point
├── main.c                 # Service bootstrap file
└── web/                   # Frontend SPA source code (React / Vite)
    ├── package.json
    ├── vite.config.js
    └── src/
```

---

## ⚙️ 2. Manifest (`manifest.arappmake`)

Manifest format:

```json
ALRIGROUP@APPMAKE
{
  "name": "home-web",
  "version": "1.0.0",
  "type": "app",
  "runtime": "native",
  "executable": "home_web.exe",
  "web": {
    "dir": "web/dist",
    "index": "index.html"
  },
  "routes": [
    {
      "host": "alrigroup.com",
      "path": "/*",
      "target": "http://127.0.0.1:3001"
    }
  ]
}
```

- **`type`**: `"app"` (standard application) or `"svc"` (system service such as `arws`).
- **`runtime`**: `"native"` (compiled C/Go) or `"node"` / `"python"`.
- **`executable`**: Name of the entry point binary.
- **`routes`**: Routing declarations registered automatically with `arws`.

---

## 🔌 3. IPC Architecture & Commands (`ARIPC`)

Applications communicate with `arcore` via TCP/Unix Socket IPC on port `9500` using 5-byte headers:

```
[1 Byte: Request Type] [4 Bytes Big-Endian: Payload Length] [Payload Data...]
```

### Supported IPC Command Types:
- `1` (`IPC_TYPE_PING`): Heartbeat ping.
- `2` (`IPC_TYPE_LIST`): List all registered services and applications.
- `3` (`IPC_TYPE_START`): Start application process.
- `4` (`IPC_TYPE_STOP`): Stop application process.
- `5` (`IPC_TYPE_RESTART`): Restart application process.
- `6` (`IPC_TYPE_REGISTER_ROUTE`): Register route dynamically with `arws`.
- `7` (`IPC_TYPE_UNREGISTER_ROUTE`): Unregister route from `arws`.
- `12` (`IPC_TYPE_CACHE_CLEAR`): Flush in-memory cache shard entries.

---

## 📦 4. Packaging Applications (`armake`)

Applications are packaged into single compressed archives (`.arapp`):

```bash
# Packaging an application manually
armake build src/apps/home.web arcore/apps/home.web.arapp
```

The output `.arapp` file is stored in `arcore/apps/` and loaded dynamically by `arcore` at runtime.
