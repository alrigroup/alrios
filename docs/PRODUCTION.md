# Operations and Production Guide — ALRIOS / arcore

Production deployment manual for Linux Debian 13 / Ubuntu servers, SSL/HTTPS configuration, and systemd service management.

---

## 📋 1. System Requirements

- **Distribution:** Debian 13 (Trixie) / Ubuntu 22.04 LTS +
- **Architecture:** `x86_64`
- **Glibc:** `>= 2.34`
- **Essential Packages:** `libssl3`, `libzstd1`, `zlib1g`, `openssl`, `curl`

---

## 🛠️ 2. Server Compilation

```bash
# Complete build (Kernel + Tools + Apps)
bash build_linux.sh
```

Compiled executables and `.arapp` packages will be placed in `arcore/`:
- `arcore/arcore` (Main daemon)
- `arcore/alrios` (CLI control tool)
- `arcore/armake` (Package manager)
- `arcore/apps/*.arapp` (Application bundles: `arws`, `arcdn`, `ardb`, `arwn`)

---

## ⚙️ 3. Production Configuration (`arws.cfg`)

Configuration file: `arcore/storage/arws/arws.cfg`

```ini
mode=production
global_mode=production
port=443
bind=0.0.0.0
```

---

## 🔒 4. SSL Certificate Setup (HTTPS)

### Let's Encrypt Certificates (Production)
```bash
sudo apt install certbot -y
sudo certbot certonly --standalone -d yourdomain.com

mkdir -p arcore/storage/arws/certs
sudo cp /etc/letsencrypt/live/yourdomain.com/fullchain.pem arcore/storage/arws/certs/cert.pem
sudo cp /etc/letsencrypt/live/yourdomain.com/privkey.pem arcore/storage/arws/certs/key.pem
sudo chown $USER:$USER arcore/storage/arws/certs/*.pem
```

---

## 🚀 5. Execution & Systemd Service

### Start Manually (in background):
```bash
./arcore/alrios power on
```

### Install System Service (systemd):
```bash
sudo cp alrios.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now alrios
```

### Check Service Status:
```bash
./arcore/alrios status
```
