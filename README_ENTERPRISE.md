# ALRIOS SOVEREIGN OPERATING SYSTEM (AST-V1) — ENTERPRISE GUIDE
**Classification:** Corporate Architecture & Production Deployment Manual  
**Platform Target:** Linux x64 / C11 Strict & POSIX.1-2008  

---

## 1. Executive Summary & Design Tenets
The **ALRIOS** platform breaks away from conventional fragile cloud architecture. It provides an autonomous, post-quantum resilient, zero-disk volatile execution platform designed for mission-critical, high-availability environments.

### Core Architectural Guarantees
1. **Zero-Disk Execution:** Encrypted `.arapp` binaries never touch persistent storage in plaintext. They decrypt directly into anonymous RAM descriptors via `memfd_create` and launch via `fexecve`.
2. **Tripartite State Separation:** Complete isolation between Immutable Code (`.arapp`), Volatile Vault Configs in RAM, and External Persistent Storage (`/var/data`).
3. **Native Post-Quantum Cryptography (PQC):** FIPS 204 (ML-DSA-65) hybrid signatures and FIPS 203 (ML-KEM-768) key encapsulation shielding against *Harvest Now, Decrypt Later* attacks.
4. **Dual-Ring Convivênce:** Signed core services execute with direct Sovereign Trust, while untrusted guest apps execute hermetically jailed in DevMode Sandboxes (Namespaces, Cgroups v2, Seccomp-BPF).
5. **Zero-Downtime Hot Reload:** SCM_RIGHTS socket descriptor handover coupled with connection draining ensures uninterrupted service during updates.

---

## 2. Directory Layout & Production SysRoot (`alrios-release`)
The compiled production release is structured under a clean Unix-like hierarchy:

```text
/opt/alrios/
├── alrios               # Master entrypoint CLI & Supervisor Controller
├── bin/                 # User-facing sovereign utilities
│   ├── armake           # Hermetic .arapp container packager
│   ├── arpm             # Delta package manager & atomic swapper
│   ├── arcc             # Strict C11 compiler wrapper & banned-API scanner
│   ├── arsign           # Isolated cryptographic notary CLI
│   ├── alrios-ca        # Air-gapped PKI Root/Intermediate issuer
│   └── arinstall        # Standalone cross-runtime installer
├── libexec/             # Core OS background daemons
│   ├── ar_auditd        # Append-only cryptographic hash chain validator
│   ├── ar_chronod       # Secure monotonic clock & anti-rollback monitor
│   ├── ar_cored         # Sensitive-memory scrubber & encrypted crash dumper
│   ├── ar_eventd        # Authorized in-memory IPC pub/sub broker
│   └── ar_resourced     # PSI memory pressure monitor & cooperative shedder
├── lib/                 # Static core and SDK libraries
└── include/alrios/      # Public C11 AR-SDK headers
```

---

## 3. Developer Quickstart & Integration Tutorial

### Step 3.1: Enforcing Strict C11 Compilation (`arcc`)
All source code must pass the static Banned-API scanner (`banned_apis.py`), which blocks insecure libc primitives (`strcpy`, `strcat`, `sprintf`, `gets`, `system`).
```bash
# Compile application source code using the strict ALRIOS wrapper
./alrios-release/bin/arcc my_service.c -o my_service
```

### Step 3.2: Writing an Sovereign AR-SDK Application
Applications integrate with the platform using `alrios/sdk/app.h`:
```c
#include "alrios/sdk/app.h"

static int my_app_init(ar_app_context_t *ctx) {
    char storage_path[256];
    // Storage retrieved securely via context binding to /var/data/<app_id>
    ar_app_get_storage_path(ctx, "db.sqlite", storage_path, sizeof(storage_path));
    return AR_APP_OK;
}

static int my_app_run(ar_app_context_t *ctx) {
    // Config retrieved from secure volatile memory vault, no plaintext .env files on disk
    const char *secret = ar_app_get_config_value(ctx, "API_SECRET");
    (void)secret;
    return AR_APP_OK;
}

static void my_app_cleanup(ar_app_context_t *ctx) {
    (void)ctx;
}

AR_DECLARE_APP("my_service", "1.0.0", my_app_init, my_app_run, my_app_cleanup);
```

### Step 3.3: Packaging and Signing an `.arapp` Container
```bash
# 1. Pack application directory into an unsigned v2 archive
./alrios-release/bin/armake pack src/apps/my_service arcore/apps/my_service.arapp

# 2. Stamp with Ed25519 & ML-DSA-65 signatures via isolated notary
./alrios-release/bin/arsign sign-arapp --in arcore/apps/my_service.arapp --out arcore/apps/my_service.signed.arapp --key leaf_key.pem
```

---

## 4. Production Operations & Deployment

### Booting the Platform
```bash
# Power on the master supervisor daemon and mount runtime bindings
./alrios-release/alrios power on

# Inspect cluster health, PIDs, and active sockets
./alrios-release/alrios status
```

### Zero-Downtime Hot Reload
```bash
# Promote new release slot atomically with live SCM_RIGHTS descriptor handoff
./alrios-release/alrios power reload
```

---
*Maintained by ALRI Development | Sovereign Systems Architecture Division (alrigroup.com)*
