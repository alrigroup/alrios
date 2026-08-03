# arcore — Master Service & Lifecycle Daemon

The **`arcore`** daemon (`src/ALRIOS/core/`) is the central process manager of the ALRIOS platform.

---

## ⚙️ Core Responsibilities

1. **Lifecycle Management**: Spawns, monitors, and restarts services (`arws`) and applications (`home-web`, `detroit-web`, `cdn`, `projetoliteratura-web`).
2. **IPC Server**: Listens on port `9500` to process control commands from CLI tools (`alrios`) and application workers.
3. **App Package Extraction**: Automatically extracts `.arapp` archives into temporary runtime spaces under `arcore/.staging/`.
4. **Environment Bootstrapping**: Prepares configuration folders (`storage/arws/`) and self-signed TLS certificates.

---

## 🔄 Service State Machine

```
   [ UNREGISTERED ] ──( Load Manifest )──▶ [ STOPPED ]
                                              │
                                       ( power on / start )
                                              │
                                              ▼
   [ RECOVERING ] ◀──( Process Crash )─── [ RUNNING ]
```
