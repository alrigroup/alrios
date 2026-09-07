# CLI Command Reference - alrios

Command-line reference for the `alrios` management tool.

---

## Service Control Commands

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

## Application Management Commands

```bash
# List all registered applications
./alrios list

# Start a specific application
./alrios start <app-name>

# Stop a specific application
./alrios stop <app-name>

# Restart a specific application
./alrios restart <app-name>
```

---

## ARWS Gateway Management Commands

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
./alrios arws override <domain> <path> <mode>

# Manage Upstream Load Balancing Pools
./alrios arws upstream list
./alrios arws upstream add <pool> <host> <port> [weight] [backup]
./alrios arws upstream drain <pool> <host> <port> [1|0]
```

---

## Package Management (armake)

```bash
# Build and seal an .arapp container
armake build <app-dir> <output.arapp>

# Unpack an .arapp archive
armake extract <input.arapp> <output-dir>

# Inspect headers and manifest
armake info <input.arapp>
```

---

## ALRIOS Package Manager (arpm)

Manage remote and local applications with native lifecycle hooks:

```bash
# List all installed packages and status
alrios arpm list
./arcore/arpm list

# Install pre-built package from registry or URL
alrios arpm install <app> [--autostart] [--start]

# Install & compile directly from remote git source
alrios arpm install-src <git-url> [--autostart] [--start]

# Update packages preserving user data
alrios arpm update <app>
alrios arpm update --all

# Uninstall package
alrios arpm uninstall <app> [--purge]

# Search registry catalogue
alrios arpm search <query>
alrios arpm info <app>
```
