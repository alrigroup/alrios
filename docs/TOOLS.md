# Developer Tools — armake & arinstall

Ecosystem utilities for packaging and runtime installation.

---

## 📦 1. `armake` — Application Packager

`armake` compresses application source code, manifests (`.arappmake`), and compiled binaries into single `.arapp` archives.

```bash
# Build an app package
armake build src/apps/home.web arcore/apps/home.web.arapp

# Extract an app package
armake extract arcore/apps/home.web.arapp /tmp/extracted_app

# List contents of an app package
armake list arcore/apps/home.web.arapp
```

---

## 📥 2. `arinstall` — Runtime Installer

`arinstall` downloads, verifies, and installs isolated language runtimes (Node.js, Python, Java, Go, Lua, Ruby) into `arcore/run/`.

```bash
# Install all required runtimes
./arcore/arinstall all

# Install specific runtime
./arcore/arinstall node
```

---

## ⚡ 3. `alrios` — Unified CLI & Orchestrator

The main management binary for controlling the `arcore` daemon, apps, and ARWS load balancer:

```bash
# Daemon Lifecycle
alrios power on|off|reload
alrios status                 # List running apps and processes

# Application Management
alrios start <app>
alrios stop <app>
alrios restart <app>

# ARWS Gateway & Load Balancer Governance
alrios arws status
alrios arws upstream list
alrios arws upstream add <pool> <host> <port> [weight=1] [backup=0|1]
alrios arws upstream drain <pool> <host> <port> [1|0]
alrios arws cfg reload

# ALRI DB Sovereign Data Guardian Governance
alrios ardb status              # Check engine, Postgres status and firewall state
alrios ardb auth login <user>   # Generate 4-hour ephemeral session token with 2FA
alrios ardb auth revoke <token> # Immediately invalidate an active session token
alrios ardb user add <user> <pass> <tenant> [role]
alrios ardb audit tail          # Real-time forensic query log stream
alrios ardb audit verify        # Validate SHA-256 blockchain hash chain integrity

# System Build & Updates
alrios update all
alrios build -p <SRC> [-o <OUT>]
```
