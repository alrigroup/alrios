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

# Manage Upstream Load Balancing Pools
./alrios arws upstream list
./alrios arws upstream add <pool> <host> <port> [weight] [backup]
./alrios arws upstream drain <pool> <host> <port> [1|0]
```

---

## 🛡️ ALRI DB Sovereign Data Guardian Commands

```bash
# Display help menu for ardb
./alrios ardb help

# Check status of ALRI DB engine and isolated PostgreSQL
./alrios ardb status

# Authenticate user with 2FA and generate 4-hour ephemeral session token for DBeaver
./alrios ardb auth login <username>

# Immediately revoke an active session token
./alrios ardb auth revoke <token>

# Create a new tenant user with RBAC role
./alrios ardb user add <username> <password> <tenant_id> [role]

# Validate cryptographic integrity of the SHA-256 blockchain audit log
./alrios ardb audit verify

# Stream audit logs in real-time
./alrios ardb audit tail
```
