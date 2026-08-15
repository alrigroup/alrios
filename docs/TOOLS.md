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
