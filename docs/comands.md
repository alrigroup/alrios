# CLI Command Reference — alrios

Command-line reference for `alrios` management tool.

---

## 📋 Service Control Commands

```bash
# Check status of arcore daemon and all applications
./alrios status

# Start all configured applications
./alrios power on

# Stop all running services
./alrios power off

# Reload system configurations and autostart apps
./alrios power reload
```

---

## ⚙️ Application Management Commands

```bash
# List all registered applications
./alrios list

# Start a specific application
./alrios start home-web

# Stop a specific application
./alrios stop home-web

# Restart a specific application
./alrios restart home-web
```

---

## 🌐 ARWS Gateway Management Commands

```bash
# Query gateway routing table and current mode
./alrios arws status

# Reload arws.cfg live without downtime
./alrios arws cfg reload

# Toggle global mode (production | test | maintenance)
./alrios arws global production
./alrios arws global test
./alrios arws global maintenance

# Set domain override
./alrios arws override alrigroup.com * maintenance
./alrios arws override alrigroup.com * production
```
