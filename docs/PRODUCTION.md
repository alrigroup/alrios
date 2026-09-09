# Operations and Production Guide — ALRIOS / arcore

Production deployment manual for Linux Debian 13 / Ubuntu servers, systemd daemon management, and high-concurrency runtime operations.

---

## 📋 1. System Requirements

- **Distribution:** Debian 13 (Trixie) / Ubuntu 22.04 LTS +
- **Architecture:** `x86_64`
- **Glibc:** `>= 2.34`
- **Essential Packages:** `libssl3`, `libzstd1`, `zlib1g`, `openssl`, `curl`

---

## 🛠️ 2. Kernel & Tools Compilation

```bash
# Complete build of ALRIOS kernel supervisor and developer tools
bash build_linux.sh
```

Compiled binaries are placed in `arcore/`:
- `arcore/arcore` (Master supervisor daemon)
- `arcore/alrios` (Unified CLI control tool)
- `arcore/armake` (Modular application packager)
- `arcore/arpm` (Package manager)

---

## ⚙️ 3. Production Supervisor Deployment

The `arcore` daemon orchestrates the lifecycle of all modular application packages (`.arapp`).

```bash
# Start supervisor in background
./alrios power on

# Verify active status and process table
./alrios status

# Graceful shutdown
./alrios power off
```

---

## 🚀 4. Systemd Service Integration

To run the ALRIOS supervisor as a persistent system-level daemon:

```bash
# 1. Install service unit to systemd
sudo cp alrios.service /etc/systemd/system/

# 2. Reload daemon registry and enable autostart on boot
sudo systemctl daemon-reload
sudo systemctl enable --now alrios

# 3. Check service health
sudo systemctl status alrios
```

---

## 🔒 5. Resource Limits & System Hardening

For high-concurrency operations (supporting up to 65,536 concurrent connections):

```bash
# Increase system-wide open file descriptor limit
sudo sysctl -w fs.file-max=2097152

# Configure security limits (/etc/security/limits.conf)
# * soft nofile 65536
# * hard nofile 65536
```

---

*Engineered by ALRI Development. Governed by ALRI GROUP © 2026 — All rights reserved.*
