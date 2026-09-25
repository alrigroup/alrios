# ALRIOS SOVEREIGN OPERATING SYSTEM ARCHITECTURE
## SPECIFICATION-DRIVEN DEVELOPMENT (SDD) MASTER ARCHITECTURAL SPECIFICATION
**DOCUMENT IDENTIFIER:** ALRIOS-SDD-SPEC-V2-ULTIMATE  
**CLASSIFICATION:** Sovereign Technical Blueprint / Non-Commercial Pure Open-Source Platform  
**GOVERNANCE POLICY:** ALRI Group & ALRI Development Open Standards (github.com/alrigroup)  
**CRYPTOGRAPHIC BASELINE:** NIST FIPS 203 (ML-KEM), NIST FIPS 204 (ML-DSA), RFC 8032 (Ed25519), NIST SP 800-38D (AES-256-GCM)  
**SYSTEM TARGET:** ISO/IEC 9899:2011 (C11 Strict), POSIX.1-2008, Linux Kernel 6.x+ In-Memory Interfaces  
**AUDIENCE:** 100 Systems Engineers across 7 Specialized Engineering Squads  
**REVISION DATE:** 2026-09-24  

---

# SECTION 1: MISSION DIRECTIVE & PHILOSOPHICAL FOUNDATION

## 1.1 The Pure Open-Source Sovereign Commons Manifesto

ALRIOS is an autonomous, post-quantum resilient, microservice and operating platform conceived, engineered, and maintained as **100% Free and Open-Source Software (FOSS)**. 

### Core Tenets of the Sovereign Commons:
1. **Zero Commercial Dependency:** The system is not licensed, sold, or gated by corporate paywalls. It belongs to the public domain of sovereign infrastructure, engineered for governments, financial backbones, decentralized autonomous institutions, and resilient private systems.
2. **Decoupled Governance:** The core codebase contains zero proprietary cryptographic keys, zero hardcoded telemetry endpoints, and zero backdoors. Any entity (from a public utility to a sovereign state or a private enterprise) can clone, inspect, audit, compile, and bind their own immutable Root of Trust into the kernel ROM.
3. **Radical Auditability:** Every subsystem—from the low-level in-memory loader to the IPC multiplexer—is implemented in pure, readable, standard C11 without obfuscated macros or hidden state machines.
4. **Reproducible Sovereignty:** Independent third parties must be able to compile the repository in isolated, network-less environments and produce bit-for-bit identical cryptographic binaries.

## 1.2 Threat Model & Zero-Trust Operational Posture

The ALRIOS runtime operates under the assumption of **Total System Compromise (Zero-Trust Model)**:
* **Host Untrustedness:** The physical hardware host, cloud hypervisor, or base operating system is assumed to be potentially compromised or hostile.
* **Storage Adversity:** Any persistent disk, SSD, or non-volatile block device is treated as public plaintext. Plaintext executable binaries, cleartext secrets, or unencrypted configuration matrices are strictly prohibited from ever touching physical storage media.
* **Network Hostility:** Every network perimeter, switch backplane, and virtual bridge is treated as tapped. The adversary is assumed capable of intercepting and indefinitely storing encrypted traffic for future post-quantum cryptanalysis (**Harvest Now, Decrypt Later - HNDL**).
* **Process Skepticism:** Processes running within the system do not trust their peers. An authenticated gateway (`arws`) treats guest applications as potentially malicious or vulnerable to Remote Code Execution (RCE).
* **Administrator Malfeasance:** Administrative (`root`) access is treated as an attack vector. The kernel audit bus and cryptographic ledger must resist retroactive alteration even by superusers.

## 1.3 The Triad of Non-Negotiable Engineering Invariants

Every module, driver, daemon, and API within the ALRIOS platform must preserve three absolute architectural invariants:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        THE ALRIOS TRIAD OF NON-NEGOTIABLE INVARIANTS                   │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ INVARIANT 1: ZERO-DISK TRANSIENCE                                                      │
│ Binaries decrypt directly into anonymous RAM descriptors (memfd_create), seal immutable│
│ via fcntl(F_ADD_SEALS), and execute via fexecve. SSDs hold only encrypted envelopes.   │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ INVARIANT 2: TRIPARTITE STATE SEPARATION                                               │
│ Code is 100% immutable (.arapp). Config is in-memory only (alrios-vault). Data is      │
│ isolated externally (/var/data). Updating an app never touches database or config.     │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ INVARIANT 3: DUAL-RING PRIVILEGE QUARANTINE                                            │
│ Signed code executes in the Sovereign Trust Ring with direct IPC and hardware ports.   │
│ Unsigned code executes jailed in the DevMode Sandbox with strict Seccomp-BPF filters.  │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

# SECTION 2: MULTI-PROFILE EXECUTION MATRIX

ALRIOS incorporates a dynamic kernel execution profile architecture, allowing a single unified C11 codebase to serve divergent operational demands: from quantum-hardened national central bank settlement daemons to high-frequency trading (HFT) and ultra-low-latency game engines (e.g., FiveM Roleplay servers).

## 2.1 Comparative Architecture Matrix across the 3 Operational Profiles

```
┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ ALRIOS MULTI-PROFILE SUBSYSTEM COMPARISON MATRIX                                                                 │
├──────────────────────────┬───────────────────────────┬───────────────────────────┬───────────────────────────────┤
│ SUBSYSTEM / CAPABILITY   │ SOVEREIGN-MAX             │ ENTERPRISE-BALANCED       │ EDGE-PERFORMANCE              │
├──────────────────────────┼───────────────────────────┼───────────────────────────┼───────────────────────────────┤
│ Target Sector            │ Central Banks, Gov, Def   │ Enterprise SaaS, APIs     │ Gaming (FiveM), HFT, Stream   │
│ Target Latency (p99)     │ < 20 ms                   │ < 5 ms                    │ < 200 µs (Microseconds)       │
│ Primary Engineering Goal │ Provable Mathematical Inv │ Balanced Defense / Speed  │ Raw CPU Throughput & Low Ping │
│ Container Decryption     │ AES-256-GCM + Split-Key   │ AES-256-GCM Local Vault   │ Direct MMAP from NVMe (Raw)   │
│ Signature Verification   │ Hybrid: Ed25519 + ML-DSA  │ Ed25519 Classic (64 bytes)│ Optional SHA-256 CRC / None   │
│ Post-Quantum Protection  │ Active (FIPS 204 Level 3) │ Dormant                   │ Disabled                      │
│ Memory Sandbox Mode      │ memfd_create + Seals      │ memfd_create + Seals      │ Direct Process Fork / Exec    │
│ Linux Namespaces         │ Full (PID, NET, NS, IPC)  │ Full (PID, NET, NS)       │ None (Shared Host Network)    │
│ Seccomp-BPF Jail         │ Strict Syscall Whitelist  │ Destructive Blacklist     │ Disabled (Raw Kernel Access)  │
│ RAM Guard Pages          │ PROT_NONE Sentinels       │ Critical Vault Areas Only │ Disabled (Zero TLB Overhead)  │
│ Kernel Audit Bus         │ SHA-256 Hash Chain Sync   │ Asynchronous Ring Buffer  │ Circular In-Memory Syslog     │
│ Inter-Daemon IPC         │ Encrypted SOCK_SEQPACKET  │ Clear SOCK_SEQPACKET      │ Lock-Free Shared Memory Ring  │
│ Socket Timeouts          │ Mandatory (3000ms Hard)   │ Resilient (10000ms Hard)  │ Non-blocking / High-Watermark │
│ Memory Allocation Model  │ Static Pre-allocation     │ Guaranteed + Burst Limit  │ Dynamic OS Heap / Jemalloc    │
│ CPU Scheduling Policy    │ SCHED_DEADLINE / Hard RT  │ SCHED_OTHER (Nice -10)    │ SCHED_FIFO (Realtime Priority)│
│ Core Overhead Footprint  │ ~2.5% to 4.0% CPU         │ ~0.5% to 1.2% CPU         │ < 0.01% CPU (Bare Metal)      │
└──────────────────────────┴───────────────────────────┴───────────────────────────┴───────────────────────────────┘
```

## 2.2 Profile 1: `SOVEREIGN-MAX` (Military, Banking & Sovereign Cloud)

### Boot Configuration Flag:
```text
alrios.profile=sovereign alrios.pqc=enforce alrios.audit=hashchain alrios.memlock=all
```

### Architectural Behavior:
1. **Cryptographic Gate:** Every application `.arapp` MUST present valid hybrid signatures: both RFC 8032 Ed25519 and FIPS 204 ML-DSA-65. The kernel aborts boot if the ML-DSA signature digest verification fails.
2. **Zero-Disk Execution:** The binary payload is streamed over memory, decrypted via AES-256-GCM using keys injected solely via volatile pipe channels, written to an anonymous RAM descriptor via `memfd_create()`, sealed with `F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE`, and invoked via `fexecve()`.
3. **Syscall Whitelist:** The Seccomp-BPF engine terminates the process immediately with `SECCOMP_RET_KILL_PROCESS` if any system call outside the manifest whitelist is issued.
4. **Audit Immutability:** Every lifecycle transition, network bind, and administrative invocation generates a cryptographically linked SHA-256 block in kernel memory, synced off-host over ML-KEM-768 encrypted syslog tunnels.
5. **Memory Defense:** Sensitive heap areas are locked with `mlockall(MCL_CURRENT | MCL_FUTURE)`. Pages adjacent to cryptographic structures are bounded by `PROT_NONE` guard pages to capture out-of-bounds pointer tampering.

## 2.3 Profile 2: `ENTERPRISE-BALANCED` (Default Corporate SaaS)

### Boot Configuration Flag:
```text
alrios.profile=enterprise alrios.pqc=optional alrios.audit=async alrios.memlock=vault
```

### Architectural Behavior:
1. **Cryptographic Gate:** The kernel verifies the 64-byte Ed25519 signature against the intermediate CA. ML-DSA verification is bypassed to shave ~10ms off cold-start startup latency.
2. **Zero-Disk Execution:** Retained fully. Applications execute from sealed `memfd` descriptors to prevent binary tampering on disk.
3. **Decoupled Vault:** Configuration matrices are retrieved from `/opt/alrios/vault/` and injected via volatile `envp[]` memory buffers without `.env` disk files.
4. **Relaxed Seccomp:** Blocks high-risk primitives (`ptrace`, `kexec_load`, `mount`, `reboot`, raw packet sockets), but permits dynamic socket creation, thread pooling, and standard memory management.
5. **Asynchronous Audit:** Audit records are written to a lock-free memory ring buffer and drained asynchronously to disk by `ar_auditd`.

## 2.4 Profile 3: `EDGE-PERFORMANCE` (FiveM Game Engines, Real-Time Streaming & HFT)

### Boot Configuration Flag:
```text
alrios.profile=performance alrios.pqc=disabled alrios.sandbox=bypass alrios.sched=fifo
```

### Architectural Behavior:
1. **Zero Cryptographic Latency at Rest:** Applications are packed with uncompressed or LZ4-compressed cleartext binary layouts. The kernel maps the executable directly into the address space via `mmap(NULL, len, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0)` directly from NVMe block storage, achieving launch times under 800 microseconds.
2. **Network Bypass:** The container shares the host network namespace (`CLONE_NEWNET` omitted). UDP game packets flow directly through kernel network queues to the worker threads without intermediate veth/bridge latency.
3. **Lock-Free Shared Memory IPC:** Inter-daemon communication operates via fixed circular ring buffers allocated in POSIX shared memory (`shm_open`) utilizing atomic compare-and-swap primitives (`stdatomic.h`), bypassing kernel socket layer transitions completely.
4. **Realtime Thread Priority:** Workers bind directly to isolated CPU cores (`pthread_setaffinity_np`) under `SCHED_FIFO` priority 99.

---

# SECTION 3: REPOSITORY SPLIT & SQUAD MATRIX (100 DEVS / 7 SQUADS)

## 3.1 Repository Layout and Include-Path Hierarchy

To enforce absolute isolation across the 100 core systems engineers, the platform code is partitioned into 7 discrete repositories. Code duplication is prohibited; shared interfaces exist exclusively as versioned public C11 headers in `/home/alexsanderalri/ALRIGROUP/code-space/alrios/include/alrios/`.

```
/home/alexsanderalri/ALRIGROUP/code-space/
├── alrios/                         # REPO 1: SQUAD 1 & 5 (Kernel Core, Memloader, Sandbox)
│   ├── include/alrios/             # CANONICAL SYSTEM HEADERS
│   │   ├── arapp_format.h          # Binary wire specification
│   │   ├── crypto.h                # Low-level cryptographic interface
│   │   ├── memloader.h             # Memory execution engine
│   │   ├── sandbox.h               # Seccomp-BPF & Namespaces
│   │   ├── ipc_channel.h           # Host-Guest wire framing
│   │   ├── ar.h                    # AR-SDK Public Component API
│   │   └── versioning.h            # ABI Symbol Versioning Directives
│   ├── src/
│   │   ├── kernel/memloader.c      # Loader implementation
│   │   ├── sandbox/jail.c          # Namespaces and Cgroups v2
│   │   ├── sandbox/devmode.c       # Seccomp-BPF filters
│   │   └── vfs/vfs_core.c          # In-memory filesystem
│   └── Makefile
│
├── src-apps/
│   ├── alrios-ca/                  # REPO 2: SQUAD 2 (PKI Master Tool, Root Manager)
│   │   ├── src/ca_main.c           # Key ceremony CLI & certificate generator
│   │   └── Makefile
│   │
│   ├── armake/                     # REPO 2: SQUAD 2 (Hermetic Application Builder)
│   │   ├── src/armake_main.c       # Packaging engine (No key access)
│   │   └── Makefile
│   │
│   ├── arsign/                     # REPO 2: SQUAD 2 (Cryptographic Notary CLI)
│   │   ├── src/arsign_main.c       # Dual signature stamping engine
│   │   └── Makefile
│   │
│   ├── libarcrypto/                # REPO 3: SQUAD 3 (PQC Cryptographic Engine)
│   │   ├── src/ed25519.c           # RFC 8032 implementation
│   │   ├── src/ml_dsa_65.c         # FIPS 204 Dilithium implementation
│   │   ├── src/ml_kem_768.c        # FIPS 203 Kyber implementation
│   │   ├── src/aes_gcm.c           # NIST SP 800-38D AES-GCM
│   │   ├── src/sha512.c            # FIPS 180-4 Secure Hash
│   │   └── src/utils.c             # Zeroize & constant-time primitives
│   │
│   ├── arws/                       # REPO 4: SQUAD 4 (Web Server & Gateway Daemon)
│   │   ├── src/server.c            # Reverse proxy & stream multiplexer
│   │   ├── src/router.c            # Route table & policy engine
│   │   └── Makefile
│   │
│   ├── arwe/                       # REPO 4: SQUAD 4 (WASM Engine & Container Runtime)
│   │   ├── src/arwe_core.c         # In-memory WASM container runtime
│   │   ├── src/arwe_obfuscator.c   # AST symbol mangler
│   │   └── Makefile
│   │
│   ├── alrios-deployer/            # REPO 6: SQUAD 6 (Sub-20ms Atomic Deployment)
│   │   ├── src/deployer.c          # Symlink swapper & key ingestion
│   │   ├── src/fd_pass.c           # Socket descriptor migration (SCM_RIGHTS)
│   │   └── Makefile
│   │
│   └── alrios-qa-redteam/          # REPO 7: SQUAD 7 (Offensive Fuzzers & Verification)
│       ├── fuzz/fuzz_arapp.c       # LibFuzzer target for container parsing
│       ├── fuzz/fuzz_ipc.c         # LibFuzzer target for IPC frames
│       ├── suites/penetration.c    # Sandbox escape exploit harness
│       └── Makefile
```

## 3.2 100-Developer Squad Allocation & Responsibility Matrix

```
┌──────────────────────────────────────────────────────────────────────────────────────────────────┐
│ SQUAD ASSIGNMENT AND SYSTEM DOMAIN BREAKDOWN                                                     │
├─────────┬──────────────┬───────────────────────────────┬─────────────────────────────────────────┤
│ SQUAD   │ HEADCOUNT    │ MANDATED SYSTEM SCOPE         │ FORMAL CONTRACT BOUNDARY                │
├─────────┼──────────────┼───────────────────────────────┼─────────────────────────────────────────┤
│ Squad 1 │ 15 Engineers │ Kernel Core & Loader          │ memloader.h, vfs_core.h                 │
│ Squad 2 │ 15 Engineers │ PKI, armake & arsign CLI      │ arapp_format.h, manifest.json schema    │
│ Squad 3 │ 15 Engineers │ Cryptography & Post-Quantum   │ crypto.h (libarcrypto.a)                │
│ Squad 4 │ 15 Engineers │ Gateway Host & IPC Multiplexer│ ipc_channel.h, arws, arwe               │
│ Squad 5 │ 15 Engineers │ DevMode Sandbox & Syscalls    │ sandbox.h (Seccomp-BPF & Cgroups v2)    │
│ Squad 6 │ 15 Engineers │ Fast Deployer & Socket Handover│ deploy_ipc.h, fd_pass.h                 │
│ Squad 7 │ 10 Engineers │ Offensive Security & Red Team │ Sanitizers, LibFuzzer, Valgrind Suites  │
└─────────┴──────────────┴───────────────────────────────┴─────────────────────────────────────────┘
```

---

# SECTION 4: CANONICAL WIRE PROTOCOL & CONTAINER SPECIFICATION (`.arapp`)

## 4.1 ASCII Bitfield Specification (32-Bit Word Alignment)

The `.arapp` format is an immutable binary envelope structured with rigid 32-bit word alignment. All multi-byte integer quantities are serialized in **Network Byte Order (Big-Endian)**.

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       MAGIC[0..3] ('A','L','R','I')           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       MAGIC[4..7] ('G','R','O','U')           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       MAGIC[8..11] ('P','@','A','R')          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       MAGIC[12..15] ('A','P','P','\0')        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         FORMAT_VERSION        |          TARGET_ARCH          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         CONTAINER_FLAGS                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      TIMESTAMP_ISSUED (MSB)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      TIMESTAMP_ISSUED (LSB)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      TIMESTAMP_EXPIRY (MSB)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      TIMESTAMP_EXPIRY (LSB)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        HEADER_SIZE_BYTES                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     ENTITLEMENTS_JSON_LENGTH                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      CIPHERTEXT_SIZE_BYTES                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                    AES_GCM_IV_NONCE [12 Bytes]                +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                    AES_GCM_AUTH_TAG [16 Bytes]                +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                  CLEARTEXT_SHA512 [64 Bytes]                  +
|               (Digest of Decrypted ELF Payload)               |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                   SIG_ED25519 [64 Bytes]                      +
|             (Classic Signature over Canonical State)          |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                 SIG_ML_DSA_65 [3309 Bytes]                    +
|          (FIPS 204 Lattice Signature over Canonical State)    |
|                                                               |
|                 PADDING TO 32-BIT ALIGNMENT (19 Bytes)        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    ENTITLEMENTS_JSON_PAYLOAD                  |
|                 (Variable Length UTF-8 Buffer)                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    ENCRYPTED_ELF_CIPHERTEXT                   |
|                 (Raw AES-256-GCM Output Buffer)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## 4.2 Complete C11 Protocol Header: `include/alrios/arapp_format.h`

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef ALRIOS_ARAPP_FORMAT_H
#define ALRIOS_ARAPP_FORMAT_H

#include <stdint.h>
#include <stddef.h>

#define ARAPP_MAGIC_STRING              "ALRIGROUP@ARAPP"
#define ARAPP_MAGIC_LEN                 16
#define ARAPP_FORMAT_VERSION_2          0x0002

/* Target Machine Architecture */
#define ARAPP_ARCH_X86_64               0x0001
#define ARAPP_ARCH_AARCH64              0x0002
#define ARAPP_ARCH_RISCV64              0x0003

/* Container Execution Flags */
#define ARAPP_FLAG_PROFILE_SOVEREIGN    (1U << 0)
#define ARAPP_FLAG_PROFILE_ENTERPRISE   (1U << 1)
#define ARAPP_FLAG_PROFILE_PERFORMANCE  (1U << 2)
#define ARAPP_FLAG_RING_SOVEREIGN       (1U << 3)
#define ARAPP_FLAG_RING_DEVMODE         (1U << 4)
#define ARAPP_FLAG_ENCRYPTED_GCM        (1U << 5)
#define ARAPP_FLAG_PQC_HYBRID_SIG       (1U << 6)
#define ARAPP_FLAG_STRIPPED_SYMBOLS     (1U << 7)

/* Cryptographic Field Lengths */
#define ARAPP_GCM_IV_LEN                12
#define ARAPP_GCM_TAG_LEN               16
#define ARAPP_SHA512_LEN                64
#define ARAPP_ED25519_SIG_LEN           64
#define ARAPP_ML_DSA_65_SIG_LEN         3309
#define ARAPP_ML_DSA_65_ALIGNED_LEN     3328  /* 3309 + 19 pad bytes */

#pragma pack(push, 1)

typedef struct arapp_header_prefix {
    uint8_t  magic[ARAPP_MAGIC_LEN];
    uint16_t format_version;
    uint16_t target_arch;
    uint32_t container_flags;
    uint64_t timestamp_issued;
    uint64_t timestamp_expiry;
    uint32_t header_size_bytes;
    uint32_t entitlements_json_length;
    uint32_t ciphertext_size_bytes;
    uint8_t  aes_gcm_iv[ARAPP_GCM_IV_LEN];
    uint8_t  aes_gcm_tag[ARAPP_GCM_TAG_LEN];
    uint8_t  cleartext_sha512[ARAPP_SHA512_LEN];
} arapp_header_prefix_t;

typedef struct arapp_signatures {
    uint8_t  sig_ed25519[ARAPP_ED25519_SIG_LEN];
    uint8_t  sig_ml_dsa_65[ARAPP_ML_DSA_65_SIG_LEN];
    uint8_t  alignment_padding[ARAPP_ML_DSA_65_ALIGNED_LEN - ARAPP_ML_DSA_65_SIG_LEN];
} arapp_signatures_t;

typedef struct arapp_file_header {
    arapp_header_prefix_t prefix;
    arapp_signatures_t    signatures;
} arapp_file_header_t;

#pragma pack(pop)

_Static_assert(sizeof(arapp_header_prefix_t) == 136, "Header prefix packing violated: must be 136 bytes");
_Static_assert(sizeof(arapp_signatures_t) == 3392, "Signatures structure packing violated: must be 3392 bytes");
_Static_assert(sizeof(arapp_file_header_t) == 3528, "Composite header packing violated: must be 3528 bytes");

#endif /* ALRIOS_ARAPP_FORMAT_H */
```

## 4.3 Canonical Pre-Image Signature Calculation Pipeline

To ensure absolute determinism across all build systems, the signature verification pre-image stream $\mathcal{M}$ is assembled via strict byte-level concatenation:

$$\mathcal{M} = 	ext{arapp\_header\_prefix\_t (136 bytes)} \mathbin{\Vert} 	ext{Normalized UTF-8 Entitlements} \mathbin{\Vert} 	ext{Raw Ciphertext}$$

The dual signature digest $\mathcal{H}$ is derived via FIPS 180-4 SHA-512:

$$\mathcal{H} = 	ext{SHA-512}(\mathcal{M})$$

The verification gate executes in sequence:
1. `alrios_ed25519_verify(Root_Ed25519_Pubkey, H, sig_ed25519)`
2. `alrios_ml_dsa_65_verify(Root_ML_DSA_Pubkey, H, sig_ml_dsa_65)`

If either cryptographic verification routine returns non-zero, execution terminates immediately.

## 4.4 Formal Manifest Schema: `manifest.entitlements`

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "ALRIOSEntitlementsManifestV2",
  "type": "object",
  "required": [
    "app_id",
    "version",
    "execution_profile",
    "execution_ring",
    "network",
    "resources",
    "ipc",
    "storage",
    "vault"
  ],
  "additionalProperties": false,
  "properties": {
    "app_id": {
      "type": "string",
      "pattern": "^[a-z0-9]+(\.[a-z0-9]+)*$"
    },
    "version": {
      "type": "string",
      "pattern": "^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$"
    },
    "execution_profile": {
      "type": "string",
      "enum": ["sovereign", "enterprise", "performance"]
    },
    "execution_ring": {
      "type": "string",
      "enum": ["sovereign_trust", "devmode_sandbox"]
    },
    "network": {
      "type": "object",
      "required": ["allow_inbound", "allow_outbound", "ports"],
      "additionalProperties": false,
      "properties": {
        "allow_inbound": { "type": "boolean" },
        "allow_outbound": { "type": "boolean" },
        "ports": {
          "type": "array",
          "items": {
            "type": "object",
            "required": ["proto", "port"],
            "additionalProperties": false,
            "properties": {
              "proto": { "type": "string", "enum": ["tcp", "udp"] },
              "port": { "type": "integer", "minimum": 1, "maximum": 65535 }
            }
          }
        }
      }
    },
    "resources": {
      "type": "object",
      "required": ["memory_guaranteed_mb", "memory_burst_mb", "max_threads", "cpu_weight"],
      "additionalProperties": false,
      "properties": {
        "memory_guaranteed_mb": { "type": "integer", "minimum": 16, "maximum": 65536 },
        "memory_burst_mb": { "type": "integer", "minimum": 16, "maximum": 262144 },
        "max_threads": { "type": "integer", "minimum": 1, "maximum": 1024 },
        "cpu_weight": { "type": "integer", "minimum": 1, "maximum": 10000 }
      }
    },
    "ipc": {
      "type": "object",
      "required": ["allowed_peers"],
      "additionalProperties": false,
      "properties": {
        "allowed_peers": {
          "type": "array",
          "items": { "type": "string", "pattern": "^[a-z0-9_.-]+$" }
        }
      }
    },
    "storage": {
      "type": "object",
      "required": ["persistent_mount", "quota_mb"],
      "additionalProperties": false,
      "properties": {
        "persistent_mount": { "type": "string", "pattern": "^/var/data/[a-z0-9_.-]+$" },
        "quota_mb": { "type": "integer", "minimum": 0, "maximum": 1048576 }
      }
    },
    "vault": {
      "type": "object",
      "required": ["required_keys"],
      "additionalProperties": false,
      "properties": {
        "required_keys": {
          "type": "array",
          "items": { "type": "string", "pattern": "^[A-Z0-9_]+$" }
        }
      }
    }
  }
}
```

---

# SECTION 5: CRYPTOGRAPHIC SUBSYSTEM & KEY HIERARCHY (PQC)

## 5.1 Public Key Infrastructure (PKI) Architecture

The ALRIOS cryptographic hierarchy guarantees non-repudiation, quantum-resistance, and complete decoupling of root authority from operational release infrastructure.

```
┌────────────────────────────────────────────────────────┐
│ TIER 1: ROOT OF TRUST (Cold / Air-Gapped Master CA)    │
│  - Ed25519 Root Keypair (Master Identity)              │
│  - ML-DSA-65 Root Keypair (Quantum Resistance Root)    │
│  - Storage: Hardware Offline Vault / Split-Shamir      │
│  - Function: Signs Intermediate CAs every 12-24 Months │
└──────────────────────────┬─────────────────────────────┘
                           │
                           ▼ Signs with FIPS 204 & RFC 8032
┌────────────────────────────────────────────────────────┐
│ TIER 2: INTERMEDIATE CA (Automated Production Notary)  │
│  - Ephemeral Lifetime: 90 Days Maximum                 │
│  - Storage: In-Memory Secure Enclave / Vault Host      │
│  - Function: Invoked by `arsign` to Certify .arapp     │
└──────────────────────────┬─────────────────────────────┘
                           │
                           ▼ Certifies Application
┌────────────────────────────────────────────────────────┐
│ TIER 3: LEAF APPLICATION CERTIFICATE (Runtime Host)    │
│  - Bound directly inside the `.arapp` binary container │
│  - Validated by the in-memory loader in RAM            │
└────────────────────────────────────────────────────────┘
```

## 5.2 Public Interface Header: `include/alrios/crypto.h`

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef ALRIOS_CRYPTO_H
#define ALRIOS_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

#define ALRIOS_CRYPTO_OK                    0
#define ALRIOS_CRYPTO_ERR_NULL_PTR         -1001
#define ALRIOS_CRYPTO_ERR_BAD_SIG_CLASSIC  -1002
#define ALRIOS_CRYPTO_ERR_BAD_SIG_PQC      -1003
#define ALRIOS_CRYPTO_ERR_GCM_AUTH_FAIL    -1004
#define ALRIOS_CRYPTO_ERR_ALLOC_FAIL       -1005
#define ALRIOS_CRYPTO_ERR_INVALID_KEY      -1006

#define ED25519_PUBLIC_KEY_LEN             32
#define ED25519_PRIVATE_KEY_LEN            64
#define ED25519_SIGNATURE_LEN              64

#define ML_DSA_65_PUBLIC_KEY_LEN           1952
#define ML_DSA_65_SECRET_KEY_LEN           4032
#define ML_DSA_65_SIGNATURE_LEN            3309

#define ML_KEM_768_PUBLIC_KEY_LEN          1184
#define ML_KEM_768_SECRET_KEY_LEN          2400
#define ML_KEM_768_CIPHERTEXT_LEN          1088
#define ML_KEM_768_SHARED_SECRET_LEN       32

#define AES256_KEY_LEN                     32
#define AES256_GCM_IV_LEN                  12
#define AES256_GCM_TAG_LEN                 16
#define SHA512_DIGEST_LEN                  64

#ifdef __cplusplus
extern "C" {
#endif

void alrios_explicit_zeroize(void *ptr, size_t len);

int alrios_constant_time_memcmp(const void *a, const void *b, size_t len);

int alrios_sha512(const uint8_t *data, size_t len, uint8_t out_hash[SHA512_DIGEST_LEN]);

int alrios_ed25519_verify(
    const uint8_t public_key[ED25519_PUBLIC_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    const uint8_t signature[ED25519_SIGNATURE_LEN]
);

int alrios_ed25519_sign(
    const uint8_t secret_key[ED25519_PRIVATE_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    uint8_t out_signature[ED25519_SIGNATURE_LEN]
);

int alrios_ml_dsa_65_verify(
    const uint8_t public_key[ML_DSA_65_PUBLIC_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    const uint8_t signature[ML_DSA_65_SIGNATURE_LEN]
);

int alrios_ml_dsa_65_sign(
    const uint8_t secret_key[ML_DSA_65_SECRET_KEY_LEN],
    const uint8_t digest[SHA512_DIGEST_LEN],
    uint8_t out_signature[ML_DSA_65_SIGNATURE_LEN]
);

int alrios_ml_kem_768_keypair(
    uint8_t out_public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_secret_key[ML_KEM_768_SECRET_KEY_LEN]
);

int alrios_ml_kem_768_encaps(
    const uint8_t public_key[ML_KEM_768_PUBLIC_KEY_LEN],
    uint8_t out_ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
);

int alrios_ml_kem_768_decaps(
    const uint8_t secret_key[ML_KEM_768_SECRET_KEY_LEN],
    const uint8_t ciphertext[ML_KEM_768_CIPHERTEXT_LEN],
    uint8_t out_shared_secret[ML_KEM_768_SHARED_SECRET_LEN]
);

int alrios_aes256_gcm_decrypt(
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t tag[AES256_GCM_TAG_LEN],
    const uint8_t key[AES256_KEY_LEN],
    const uint8_t iv[AES256_GCM_IV_LEN],
    uint8_t *out_plaintext
);

int alrios_aes256_gcm_encrypt(
    const uint8_t *plaintext,
    size_t plaintext_len,
    const uint8_t *aad,
    size_t aad_len,
    const uint8_t key[AES256_KEY_LEN],
    const uint8_t iv[AES256_GCM_IV_LEN],
    uint8_t *out_ciphertext,
    uint8_t out_tag[AES256_GCM_TAG_LEN]
);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_CRYPTO_H */
```

## 5.3 Deterministic Utilities: `src/crypto/utils.c`

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#include "alrios/crypto.h"
#include <string.h>

void alrios_explicit_zeroize(void *ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) {
        *p++ = 0x00;
    }
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
}

int alrios_constant_time_memcmp(const void *a, const void *b, size_t len) {
    if (!a || !b) return -1;
    const volatile unsigned char *ua = (const volatile unsigned char *)a;
    const volatile unsigned char *ub = (const volatile unsigned char *)b;
    unsigned char result = 0;

    for (size_t i = 0; i < len; i++) {
        result |= (ua[i] ^ ub[i]);
    }

    return (int)result == 0 ? 0 : -1;
}
```

---

# SECTION 6: IN-MEMORY KERNEL LOADER ENGINE (`memloader.c`)

The loader orchestrates memory-only decryption, descriptor sealing, execution ring verification, and clean context handover via `fexecve`.

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <time.h>

#include "alrios/arapp_format.h"
#include "alrios/crypto.h"
#include "alrios/memloader.h"

/* Root of Trust: Master Public Keys Burned into Binary ROM */
static const uint8_t ALRIOS_ROOT_ED25519_PUBKEY[ED25519_PUBLIC_KEY_LEN] = {
    0x2c, 0xa1, 0x93, 0xef, 0x11, 0x48, 0x72, 0x90,
    0xab, 0x33, 0xdf, 0x88, 0x76, 0x51, 0x09, 0x12,
    0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12,
    0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x12
};

static const uint8_t ALRIOS_ROOT_ML_DSA_PUBKEY[ML_DSA_65_PUBLIC_KEY_LEN] = {
    0x4d, 0x4c, 0x44, 0x53, 0x41, 0x36, 0x35, 0x01,
    [8 and all subsequent validated operations 1951] = 0xAA
};

int alrios_memloader_run(
    const uint8_t *arapp_stream,
    size_t stream_len,
    const uint8_t decryption_key[AES256_KEY_LEN],
    char *const argv[],
    char *const envp[]
) {
    if (!arapp_stream || stream_len < sizeof(arapp_file_header_t) || !decryption_key) {
        return ALRIOS_MEMLOADER_ERR_INVALID_PARAM;
    }

    const arapp_file_header_t *hdr = (const arapp_file_header_t *)arapp_stream;

    /* 1. Magic Sequence Verification */
    if (alrios_constant_time_memcmp(hdr->prefix.magic, ARAPP_MAGIC_STRING, ARAPP_MAGIC_LEN) != 0) {
        return ALRIOS_MEMLOADER_ERR_BAD_MAGIC;
    }

    /* 2. Endianness Conversion & Boundary Checks */
    uint32_t header_size = ntohl(hdr->prefix.header_size_bytes);
    uint32_t entitlements_len = ntohl(hdr->prefix.entitlements_json_length);
    uint32_t ciphertext_len = ntohl(hdr->prefix.ciphertext_size_bytes);
    uint32_t flags = ntohl(hdr->prefix.container_flags);

    if (header_size != sizeof(arapp_file_header_t)) {
        return ALRIOS_MEMLOADER_ERR_CORRUPT_HEADER;
    }

    if (stream_len != (size_t)header_size + entitlements_len + ciphertext_len) {
        return ALRIOS_MEMLOADER_ERR_LENGTH_MISMATCH;
    }

    /* 3. Anti-Replay Temporal Check */
    uint64_t now_sec = (uint64_t)time(NULL);
    uint64_t issued = ((uint64_t)ntohl(hdr->prefix.timestamp_issued >> 32) << 32) |
                      ntohl(hdr->prefix.timestamp_issued & 0xFFFFFFFF);
    uint64_t expiry = ((uint64_t)ntohl(hdr->prefix.timestamp_expiry >> 32) << 32) |
                      ntohl(hdr->prefix.timestamp_expiry & 0xFFFFFFFF);

    if (now_sec < issued || now_sec > expiry) {
        return ALRIOS_MEMLOADER_ERR_EXPIRED;
    }

    /* 4. Pre-Image Construction */
    size_t pre_image_len = sizeof(arapp_header_prefix_t) + entitlements_len + ciphertext_len;
    uint8_t *pre_image = (uint8_t *)malloc(pre_image_len);
    if (!pre_image) {
        return ALRIOS_MEMLOADER_ERR_OOM;
    }

    memcpy(pre_image, &hdr->prefix, sizeof(arapp_header_prefix_t));
    memcpy(pre_image + sizeof(arapp_header_prefix_t),
           arapp_stream + header_size,
           entitlements_len);
    memcpy(pre_image + sizeof(arapp_header_prefix_t) + entitlements_len,
           arapp_stream + header_size + entitlements_len,
           ciphertext_len);

    uint8_t digest[SHA512_DIGEST_LEN];
    if (alrios_sha512(pre_image, pre_image_len, digest) != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(pre_image, pre_image_len);
        free(pre_image);
        return ALRIOS_MEMLOADER_ERR_CRYPTO_FAIL;
    }

    alrios_explicit_zeroize(pre_image, pre_image_len);
    free(pre_image);

    /* 5. Cryptographic Signature Gates */
    if (alrios_ed25519_verify(ALRIOS_ROOT_ED25519_PUBKEY, digest, hdr->signatures.sig_ed25519) != ALRIOS_CRYPTO_OK) {
        return ALRIOS_MEMLOADER_ERR_SIG_FAIL_CLASSIC;
    }

    /* Enforce ML-DSA unless running under explicit performance profile */
    if (!(flags & ARAPP_FLAG_PROFILE_PERFORMANCE)) {
        if (alrios_ml_dsa_65_verify(ALRIOS_ROOT_ML_DSA_PUBKEY, digest, hdr->signatures.sig_ml_dsa_65) != ALRIOS_CRYPTO_OK) {
            return ALRIOS_MEMLOADER_ERR_SIG_FAIL_PQC;
        }
    }

    /* 6. In-Memory Decryption Allocation */
    uint8_t *decrypted_elf = (uint8_t *)malloc(ciphertext_len);
    if (!decrypted_elf) {
        return ALRIOS_MEMLOADER_ERR_OOM;
    }

    const uint8_t *ciphertext_ptr = arapp_stream + header_size + entitlements_len;
    const uint8_t *entitlements_ptr = arapp_stream + header_size;

    int dec_rc = alrios_aes256_gcm_decrypt(
        ciphertext_ptr,
        ciphertext_len,
        entitlements_ptr,
        entitlements_len,
        hdr->prefix.aes_gcm_tag,
        decryption_key,
        hdr->prefix.aes_gcm_iv,
        decrypted_elf
    );

    if (dec_rc != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(decrypted_elf, ciphertext_len);
        free(decrypted_elf);
        return ALRIOS_MEMLOADER_ERR_DECRYPT_FAIL;
    }

    /* 7. Verify Integrity of Plaintext ELF Image */
    uint8_t elf_digest[SHA512_DIGEST_LEN];
    if (alrios_sha512(decrypted_elf, ciphertext_len, elf_digest) != ALRIOS_CRYPTO_OK) {
        alrios_explicit_zeroize(decrypted_elf, ciphertext_len);
        free(decrypted_elf);
        return ALRIOS_MEMLOADER_ERR_CRYPTO_FAIL;
    }

    if (alrios_constant_time_memcmp(elf_digest, hdr->prefix.cleartext_sha512, SHA512_DIGEST_LEN) != 0) {
        alrios_explicit_zeroize(decrypted_elf, ciphertext_len);
        free(decrypted_elf);
        return ALRIOS_MEMLOADER_ERR_ELF_HASH_MISMATCH;
    }

    /* 8. Allocate Anonymous Sealed Memory File */
    int memfd = (int)syscall(SYS_memfd_create, "alrios_payload", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (memfd < 0) {
        alrios_explicit_zeroize(decrypted_elf, ciphertext_len);
        free(decrypted_elf);
        return ALRIOS_MEMLOADER_ERR_MEMFD_FAIL;
    }

    size_t total_written = 0;
    while (total_written < ciphertext_len) {
        ssize_t written = write(memfd, decrypted_elf + total_written, ciphertext_len - total_written);
        if (written <= 0) {
            if (written < 0 && errno == EINTR) continue;
            close(memfd);
            alrios_explicit_zeroize(decrypted_elf, ciphertext_len);
            free(decrypted_elf);
            return ALRIOS_MEMLOADER_ERR_IO_FAIL;
        }
        total_written += (size_t)written;
    }

    /* 9. Zeroize Plaintext Heap Copy Immediately */
    alrios_explicit_zeroize(decrypted_elf, ciphertext_len);
    free(decrypted_elf);

    /* 10. Lock Descritor with Irreversible Kernel Seals */
    if (fcntl(memfd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) < 0) {
        close(memfd);
        return ALRIOS_MEMLOADER_ERR_SEALING_FAIL;
    }

    /* 11. Atomic Execution Transfer via fexecve */
    fexecve(memfd, argv, envp);

    /* Unreachable unless exec failed */
    int err = errno;
    close(memfd);
    return (err == EACCES) ? ALRIOS_MEMLOADER_ERR_EXEC_DENIED : ALRIOS_MEMLOADER_ERR_EXEC_FAIL;
}
```

---

# SECTION 7: MEMORY GOVERNANCE, ENCRYPTION & PRESSURE STALL

## 7.1 Dynamic Memory Tiering Architecture

```
┌────────────────────────────────────────────────────────────────────────┐
│ NÍVEL 1: CORE DAEMONS (Zero Overcommit / Fixed Locked Memory)          │
│  - Services: arkernel, arauth (Vault), ardb (Buffer Pool)              │
│  - Allocation: Pre-allocated at boot, locked via mlockall()            │
│  - Guarantee: Swap disabled, OOM killer immunity, permanent priority   │
├────────────────────────────────────────────────────────────────────────┤
│ NÍVEL 2: WORKER DAEMONS (Guaranteed Quota + Elastic Burst Limit)       │
│  - Services: arws, arwe, guest applications (.arapp)                   │
│  - Allocation: Guaranteed base + dynamic burst up to memory.max        │
│  - Pressure Signaling: PSI notification via epoll at 80% watermark     │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                       RAM Pressure > 80% Watermark
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ COOPERATIVE RECOVERY & CIRCUIT BREAKING (Graceful Degradation)         │
│  1. Kernel broadcasts ALRI_IPC_MSG_MEM_PRESSURE_WARN to all workers    │
│  2. Applications purge query caches, connection buffers, session pools │
│  3. Gateway (arws) triggers 503 Service Unavailable / Queue Shedding   │
│  4. If pressure crosses 95%: Sacrificial drop of DevMode guests first  │
└────────────────────────────────────────────────────────────────────────┘
```

## 7.2 Memory Governance Implementation: `src/kernel/mem_governor.c`

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

#define ALRIOS_PAGE_SIZE 4096

void *alrios_alloc_guarded_pages(size_t payload_size) {
    if (payload_size == 0) return NULL;

    size_t num_pages = (payload_size + ALRIOS_PAGE_SIZE - 1) / ALRIOS_PAGE_SIZE;
    size_t total_alloc = (num_pages + 2) * ALRIOS_PAGE_SIZE;

    uint8_t *base = (uint8_t *)mmap(
        NULL,
        total_alloc,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (base == MAP_FAILED) return NULL;

    /* Lock allocation to physical RAM; prevent swap extraction */
    if (mlock(base, total_alloc) != 0) {
        munmap(base, total_alloc);
        return NULL;
    }

    /* Guard Page 1 (Leading Sentinel) */
    mprotect(base, ALRIOS_PAGE_SIZE, PROT_NONE);

    /* Guard Page 2 (Trailing Sentinel) */
    mprotect(base + (num_pages + 1) * ALRIOS_PAGE_SIZE, ALRIOS_PAGE_SIZE, PROT_NONE);

    return (void *)(base + ALRIOS_PAGE_SIZE);
}

void alrios_free_guarded_pages(void *ptr, size_t payload_size) {
    if (!ptr || payload_size == 0) return;

    size_t num_pages = (payload_size + ALRIOS_PAGE_SIZE - 1) / ALRIOS_PAGE_SIZE;
    size_t total_alloc = (num_pages + 2) * ALRIOS_PAGE_SIZE;

    uint8_t *base = ((uint8_t *)ptr) - ALRIOS_PAGE_SIZE;

    /* Zeroize data pages prior to release */
    alrios_explicit_zeroize(ptr, num_pages * ALRIOS_PAGE_SIZE);

    munlock(base, total_alloc);
    munmap(base, total_alloc);
}
```

---

# SECTION 8: TRIPARTITE STATE SEPARATION & IN-MEMORY CONFIG VAULT

To allow sub-20ms atomic hot-reloads without losing active SQLite sessions or recompiling `.arapp` binaries, the system strictly isolates three layers:

```
┌────────────────────────────────────────────────────────┐
│ LAYER 1: IMMUTABLE CODE CONTAINER (.arapp)             │
│  - Storage: /opt/alrios/releases/<TIMESTAMP>/app.arapp │
│  - VFS: Read-only memory-mapped anonymous RAM          │
│  - Upgrades: Swap symlink pointer /opt/alrios/current  │
├────────────────────────────────────────────────────────┤
│ LAYER 2: IN-MEMORY VOLATILE VAULT (Dynamic Config)     │
│  - Storage: Ingested via IPC/SSH straight to RAM       │
│  - Decrypted into secure heap, passed via envp[]       │
│  - Persistence on SSD: ZERO (No .env files on disk)    │
├────────────────────────────────────────────────────────┤
│ LAYER 3: EXTERNAL PERSISTENT DATA (/var/data/<app_id>/)│
│  - Storage: /opt/alrios/storage/<app_id>/              │
│  - Mounted transparently inside container VFS          │
│  - Upgrades: Remains untouched across binary reloads   │
└────────────────────────────────────────────────────────┘
```

## 8.1 In-Memory Vault Controller: `src/vault/vault_loader.c`

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

#define MAX_VAULT_ENTRIES 128
#define MAX_ENV_KEY_LEN   64
#define MAX_ENV_VAL_LEN   1024

typedef struct vault_entry {
    char key[MAX_ENV_KEY_LEN];
    char value[MAX_ENV_VAL_LEN];
} vault_entry_t;

typedef struct vault_matrix {
    size_t count;
    vault_entry_t entries[MAX_VAULT_ENTRIES];
} vault_matrix_t;

int alrios_vault_inject_envp(const vault_matrix_t *vault, char ***out_envp) {
    if (!vault || !out_envp) return -1;

    char **envp = (char **)calloc(vault->count + 1, sizeof(char *));
    if (!envp) return -2;

    for (size_t i = 0; i < vault->count; i++) {
        size_t line_len = strlen(vault->entries[i].key) + strlen(vault->entries[i].value) + 2;
        envp[i] = (char *)malloc(line_len);
        if (!envp[i]) {
            for (size_t j = 0; j < i; j++) {
                alrios_explicit_zeroize(envp[j], strlen(envp[j]));
                free(envp[j]);
            }
            free(envp);
            return -3;
        }
        snprintf(envp[i], line_len, "%s=%s", vault->entries[i].key, vault->entries[i].value);
    }
    envp[vault->count] = NULL;
    *out_envp = envp;
    return 0;
}
```

---

# SECTION 9: DEVMODE JAIL, SECCOMP-BPF & NAMESPACES

Unsigned, un-homologated developer code runs inside a DevMode Sandbox Ring. It is isolated from production daemons via unshare namespaces, Cgroups v2 quotas, and strict Seccomp-BPF filters.

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#define _GNU_SOURCE
#include <stddef.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <errno.h>
#include <sched.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>

#include "alrios/sandbox.h"

int alrios_sandbox_apply_seccomp(void) {
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        return ALRIOS_SANDBOX_ERR_PRCTL_FAIL;
    }

    #define X32_SYSCALL_BIT 0x40000000
    #if defined(__x86_64__)
        #define ALRIOS_AUDIT_ARCH AUDIT_ARCH_X86_64
    #elif defined(__aarch64__)
        #define ALRIOS_AUDIT_ARCH AUDIT_ARCH_AARCH64
    #else
        #error "Unsupported architecture for ALRIOS Seccomp Sandbox"
    #endif

    struct sock_filter filter[] = {
        /* [0] Architecture Verification */
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, arch))),
        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, ALRIOS_AUDIT_ARCH, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        /* [3] Load Syscall Number */
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),

        #if defined(__x86_64__)
        /* Block x32 ABI calls */
        BPF_JUMP(BPF_JMP | BPF_K | BPF_GE, X32_SYSCALL_BIT, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        #endif

        /* BLACKLISTED HIGH HAZARD SYSCALLS (Instant Kill) */
        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_ptrace, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_reboot, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_mount, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_umount2, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_kexec_load, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_chroot, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        /* RAW & PACKET NETWORK DENIAL */
        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_socket, 1, 0),
        BPF_JUMP(BPF_JMP | BPF_K | BPF_ALWAYS, 0, 0, 6),

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, args[0]))),
        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, 16, 0, 1), /* Block AF_NETLINK */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA)),
        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, 17, 0, 1), /* Block AF_PACKET */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA)),

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),

        /* SAFE WHITELIST SYSCALLS */
        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_read, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_write, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_close, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_mmap, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_mprotect, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_munmap, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_brk, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_exit_group, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        BPF_JUMP(BPF_JMP | BPF_K | BPF_EQ, SYS_clock_gettime, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        /* Default: Block and return EPERM */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA))
    };

    struct sock_fprog prog = {
        .len = (unsigned short)(sizeof(filter) / sizeof(filter[0])),
        .filter = filter,
    };

    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        return ALRIOS_SANDBOX_ERR_SECCOMP_FAIL;
    }

    return ALRIOS_SANDBOX_OK;
}
```

---

# SECTION 10: HOST-GUEST IPC PROTOCOL (`arws` / `arwe` <-> GUEST)

## 10.1 Protocol Header: `include/alrios/ipc_channel.h`

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef ALRIOS_IPC_CHANNEL_H
#define ALRIOS_IPC_CHANNEL_H

#include <stdint.h>
#include <stddef.h>

#define ALRIOS_IPC_MAGIC             0x41524950U /* 'A','R','I','P' */
#define ALRIOS_IPC_VERSION           0x01
#define ALRIOS_IPC_MAX_FRAME_SIZE    65536

typedef enum alrios_ipc_type {
    ALRIOS_IPC_MSG_PING             = 0x0001,
    ALRIOS_IPC_MSG_PONG             = 0x0002,
    ALRIOS_IPC_MSG_HTTP_REQ         = 0x0010,
    ALRIOS_IPC_MSG_HTTP_RESP        = 0x0011,
    ALRIOS_IPC_MSG_STREAM_DATA      = 0x0020,
    ALRIOS_IPC_MSG_MEM_PRESSURE_WARN= 0x0080,
    ALRIOS_IPC_MSG_DRAIN_DRAINING   = 0x00E0,
    ALRIOS_IPC_MSG_TERMINATE        = 0x00FF,
    ALRIOS_IPC_MSG_ERR_REJECTED     = 0xEEEE
} alrios_ipc_type_t;

#pragma pack(push, 1)

typedef struct alrios_ipc_header {
    uint32_t magic;
    uint8_t  version;
    uint8_t  reserved[3];
    uint16_t msg_type;
    uint16_t status_flags;
    uint64_t transaction_id;
    uint32_t payload_len;
    uint32_t crc32_checksum;
} alrios_ipc_header_t;

#pragma pack(pop)

_Static_assert(sizeof(alrios_ipc_header_t) == 28, "IPC Header packing violated: must be 28 bytes");

int alrios_ipc_send_frame(
    int socket_fd,
    alrios_ipc_type_t type,
    uint64_t txn_id,
    const void *payload,
    uint32_t payload_len
);

int alrios_ipc_recv_frame(
    int socket_fd,
    alrios_ipc_header_t *out_hdr,
    void *out_payload_buf,
    uint32_t max_buf_len
);

#endif /* ALRIOS_IPC_CHANNEL_H */
```

## 10.2 IPC Frame Processor: `src/ipc/ipc_channel.c`

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#include "alrios/ipc_channel.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

static uint32_t calculate_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

int alrios_ipc_send_frame(
    int socket_fd,
    alrios_ipc_type_t type,
    uint64_t txn_id,
    const void *payload,
    uint32_t payload_len
) {
    if (socket_fd < 0 || (payload_len > 0 && !payload)) return -1;
    if (payload_len > (ALRIOS_IPC_MAX_FRAME_SIZE - sizeof(alrios_ipc_header_t))) return -2;

    uint8_t frame_buf[ALRIOS_IPC_MAX_FRAME_SIZE];
    alrios_ipc_header_t *hdr = (alrios_ipc_header_t *)frame_buf;

    hdr->magic = ALRIOS_IPC_MAGIC;
    hdr->version = ALRIOS_IPC_VERSION;
    hdr->reserved[0] = 0;
    hdr->reserved[1] = 0;
    hdr->reserved[2] = 0;
    hdr->msg_type = (uint16_t)type;
    hdr->status_flags = 0;
    hdr->transaction_id = txn_id;
    hdr->payload_len = payload_len;
    hdr->crc32_checksum = payload_len ? calculate_crc32((const uint8_t *)payload, payload_len) : 0;

    if (payload_len > 0) {
        memcpy(frame_buf + sizeof(alrios_ipc_header_t), payload, payload_len);
    }

    size_t total = sizeof(alrios_ipc_header_t) + payload_len;
    ssize_t sent = send(socket_fd, frame_buf, total, MSG_NOSIGNAL);
    return (sent == (ssize_t)total) ? 0 : -3;
}

int alrios_ipc_recv_frame(
    int socket_fd,
    alrios_ipc_header_t *out_hdr,
    void *out_payload_buf,
    uint32_t max_buf_len
) {
    if (socket_fd < 0 || !out_hdr) return -1;

    uint8_t frame_buf[ALRIOS_IPC_MAX_FRAME_SIZE];
    ssize_t recvd = recv(socket_fd, frame_buf, sizeof(frame_buf), 0);
    if (recvd < (ssize_t)sizeof(alrios_ipc_header_t)) return -2;

    const alrios_ipc_header_t *hdr = (const alrios_ipc_header_t *)frame_buf;
    if (hdr->magic != ALRIOS_IPC_MAGIC || hdr->version != ALRIOS_IPC_VERSION) return -3;
    if ((size_t)recvd != (sizeof(alrios_ipc_header_t) + hdr->payload_len)) return -4;
    if (hdr->payload_len > max_buf_len) return -5;

    if (hdr->payload_len > 0) {
        const uint8_t *payload_ptr = frame_buf + sizeof(alrios_ipc_header_t);
        if (calculate_crc32(payload_ptr, hdr->payload_len) != hdr->crc32_checksum) return -6;
        memcpy(out_payload_buf, payload_ptr, hdr->payload_len);
    }

    memcpy(out_hdr, hdr, sizeof(alrios_ipc_header_t));
    return 0;
}
```

---

# SECTION 11: SUB-20MS ATOMIC DEPLOYMENT ENGINE

## 11.1 Atomic Swap and Live Descriptor Migration

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>

int alrios_deploy_atomic_swap(const char *release_timestamp, pid_t master_daemon_pid) {
    char target_dir[256];
    char symlink_tmp[256];
    const char *symlink_current = "/opt/alrios/current";

    snprintf(target_dir, sizeof(target_dir), "/opt/alrios/releases/%s", release_timestamp);
    snprintf(symlink_tmp, sizeof(symlink_tmp), "/opt/alrios/current_tmp_%s", release_timestamp);

    unlink(symlink_tmp);
    if (symlink(target_dir, symlink_tmp) != 0) return -1;
    if (rename(symlink_tmp, symlink_current) != 0) {
        unlink(symlink_tmp);
        return -2;
    }

    if (master_daemon_pid > 1) {
        kill(master_daemon_pid, SIGHUP);
    }
    return 0;
}

int alrios_send_live_fd(int unix_sock, int fd_to_send) {
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));

    char iov_base = 'F';
    struct iovec iov = { .iov_base = &iov_base, .iov_len = 1 };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    struct cmsghdr *cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    *((int *)CMSG_DATA(cmptr)) = fd_to_send;

    return (sendmsg(unix_sock, &msg, 0) < 0) ? -1 : 0;
}
```

---

# SECTION 12: ALRIOS NATIVE SDK (AR-SDK) & COMPILER WRAPPER (`arcc`)

## 12.1 Public SDK Interface: `include/alrios/ar.h`

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef ALRIOS_AR_SDK_H
#define ALRIOS_AR_SDK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define AR_OK                      0
#define AR_ERR_TIMEOUT            -1
#define AR_ERR_OOM                -2
#define AR_ERR_PERMISSION         -3

typedef struct ar_context ar_context_t;
typedef struct ar_socket ar_socket_t;
typedef struct ar_db ar_db_t;
typedef struct ar_req ar_req_t;
typedef struct ar_res ar_res_t;

typedef int (*ar_init_fn)(ar_context_t *ctx);
typedef void (*ar_req_fn)(ar_context_t *ctx, ar_req_t *req, ar_res_t *res);
typedef void (*ar_drain_fn)(ar_context_t *ctx);

typedef struct ar_app_descriptor {
    const char *app_id;
    const char *version;
    ar_init_fn  on_init;
    ar_req_fn   on_req;
    ar_drain_fn on_drain;
} ar_app_descriptor_t;

#define AR_DECLARE_APP(and all subsequent validated operations)     static const ar_app_descriptor_t g_alrios_app_manifest = { __VA_ARGS__ };     const ar_app_descriptor_t *alrios_get_app_descriptor(void) { return &g_alrios_app_manifest; }

/* Memory Governance: Request-Scoped Arena Allocator (Zero Leaks by Design) */
void *ar_req_alloc(ar_req_t *req, size_t size);

/* Vault Access: Volatile In-RAM Environment Retrieval */
const char *ar_vault_get(ar_context_t *ctx, const char *key);

/* Network: Enforced Timeout Signature (Non-Negotiable) */
ar_socket_t *ar_tcp_connect(
    ar_context_t *ctx,
    const char *host,
    uint16_t port,
    uint32_t timeout_ms
);

/* Storage: Persistent Storage Bind Path */
ar_db_t *ar_db_open_sqlite(ar_context_t *ctx, const char *db_filename);

#endif /* ALRIOS_AR_SDK_H */
```

---

# SECTION 13: SQUAD IMPLEMENTATION WORK ORDERS & TIMELINE

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ 12-WEEK ROLLOUT SCHEDULE ACROSS 100 CORE SYSTEMS DEVELOPERS                           │
├───────┬──────────────────────┬─────────────────────────────────────────────────────────┤
│ WEEK  │ SQUADS INVOLVED      │ OBJECTIVES & MILESTONES                                 │
├───────┼──────────────────────┼─────────────────────────────────────────────────────────┤
│ W1-W2 │ Squad 1, 2, 3        │ Freeze arapp_format.h, crypto.h, setup root CA keys     │
│ W3-W4 │ Squad 3              │ Complete ML-DSA-65 & Ed25519 dual-signing engine        │
│ W5-W6 │ Squad 1, 5           │ Implement memloader.c, memfd seals, and Seccomp jail    │
│ W7-W8 │ Squad 2, 4           │ Finalize arsign CLI, arws/arwe Host-Guest IPC channels │
│ W9-10 │ Squad 6              │ Implement alrios-deployer, atomic symlinks & fd_pass    │
│ W11   │ Squad 7              │ Execute full fuzzing barrage (10^8 cycles without crash)│
│ W12   │ All Squads           │ Final homologation sign-off; release v1.0.0-RELEASE     │
└───────┴──────────────────────┴─────────────────────────────────────────────────────────┘
```

---

# SECTION 14: EXHAUSTIVE TECHNICAL TEST SUITES & EDGE-CASE HARNESSES

To ensure absolute industrial verification across all 7 squads, every subsystem is bound to explicit, reproducible unit and integration test harnesses written in pure C11. Each test case asserts state invariants, validates boundary constraints, and ensures memory cleanup without leaks.


# SECTION 14: EXHAUSTIVE TECHNICAL TEST SUITES & EDGE-CASE HARNESSES

Every test in this section is a mandatory pre-merge gate. A failed assertion, sanitizer diagnostic, memory leak, timing variance outside the declared bound, or malformed audit event blocks promotion.

## 14.1 Test Specification `TC-CORE-001` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `001`. The case uses a 136-byte allocation and corrupts digest byte offset 1. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 136 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_001_execute(void) {
    const size_t allocation_size = 136U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 1U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[1U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_001_execute();
    puts("TC-CORE-001: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_001.c build/libarcrypto.a -o build/tc_core_001
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_001
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_001
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 1 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.2 Test Specification `TC-CORE-002` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `002`. The case uses a 144-byte allocation and corrupts digest byte offset 2. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 144 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_002_execute(void) {
    const size_t allocation_size = 144U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 2U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[2U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_002_execute();
    puts("TC-CORE-002: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_002.c build/libarcrypto.a -o build/tc_core_002
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_002
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_002
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 2 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.3 Test Specification `TC-CORE-003` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `003`. The case uses a 152-byte allocation and corrupts digest byte offset 3. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 152 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_003_execute(void) {
    const size_t allocation_size = 152U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 3U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[3U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_003_execute();
    puts("TC-CORE-003: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_003.c build/libarcrypto.a -o build/tc_core_003
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_003
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_003
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 3 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.4 Test Specification `TC-CORE-004` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `004`. The case uses a 160-byte allocation and corrupts digest byte offset 4. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 160 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_004_execute(void) {
    const size_t allocation_size = 160U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 4U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[4U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_004_execute();
    puts("TC-CORE-004: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_004.c build/libarcrypto.a -o build/tc_core_004
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_004
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_004
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 4 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.5 Test Specification `TC-CORE-005` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `005`. The case uses a 168-byte allocation and corrupts digest byte offset 5. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 168 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_005_execute(void) {
    const size_t allocation_size = 168U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 5U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[5U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_005_execute();
    puts("TC-CORE-005: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_005.c build/libarcrypto.a -o build/tc_core_005
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_005
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_005
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 5 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.6 Test Specification `TC-CORE-006` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `006`. The case uses a 176-byte allocation and corrupts digest byte offset 6. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 176 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_006_execute(void) {
    const size_t allocation_size = 176U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 6U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[6U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_006_execute();
    puts("TC-CORE-006: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_006.c build/libarcrypto.a -o build/tc_core_006
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_006
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_006
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 6 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.7 Test Specification `TC-CORE-007` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `007`. The case uses a 184-byte allocation and corrupts digest byte offset 7. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 184 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_007_execute(void) {
    const size_t allocation_size = 184U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 7U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[7U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_007_execute();
    puts("TC-CORE-007: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_007.c build/libarcrypto.a -o build/tc_core_007
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_007
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_007
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 7 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.8 Test Specification `TC-CORE-008` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `008`. The case uses a 192-byte allocation and corrupts digest byte offset 8. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 192 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_008_execute(void) {
    const size_t allocation_size = 192U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 8U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[8U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_008_execute();
    puts("TC-CORE-008: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_008.c build/libarcrypto.a -o build/tc_core_008
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_008
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_008
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 8 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.9 Test Specification `TC-CORE-009` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `009`. The case uses a 200-byte allocation and corrupts digest byte offset 9. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 200 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_009_execute(void) {
    const size_t allocation_size = 200U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 9U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[9U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_009_execute();
    puts("TC-CORE-009: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_009.c build/libarcrypto.a -o build/tc_core_009
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_009
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_009
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 9 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.10 Test Specification `TC-CORE-010` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `010`. The case uses a 208-byte allocation and corrupts digest byte offset 10. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 208 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_010_execute(void) {
    const size_t allocation_size = 208U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 10U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[10U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_010_execute();
    puts("TC-CORE-010: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_010.c build/libarcrypto.a -o build/tc_core_010
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_010
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_010
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 10 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.11 Test Specification `TC-CORE-011` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `011`. The case uses a 216-byte allocation and corrupts digest byte offset 11. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 216 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_011_execute(void) {
    const size_t allocation_size = 216U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 11U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[11U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_011_execute();
    puts("TC-CORE-011: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_011.c build/libarcrypto.a -o build/tc_core_011
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_011
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_011
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 11 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.12 Test Specification `TC-CORE-012` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `012`. The case uses a 224-byte allocation and corrupts digest byte offset 12. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 224 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_012_execute(void) {
    const size_t allocation_size = 224U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 12U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[12U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_012_execute();
    puts("TC-CORE-012: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_012.c build/libarcrypto.a -o build/tc_core_012
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_012
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_012
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 12 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.13 Test Specification `TC-CORE-013` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `013`. The case uses a 232-byte allocation and corrupts digest byte offset 13. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 232 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_013_execute(void) {
    const size_t allocation_size = 232U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 13U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[13U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_013_execute();
    puts("TC-CORE-013: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_013.c build/libarcrypto.a -o build/tc_core_013
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_013
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_013
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 13 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.14 Test Specification `TC-CORE-014` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `014`. The case uses a 240-byte allocation and corrupts digest byte offset 14. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 240 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_014_execute(void) {
    const size_t allocation_size = 240U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 14U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[14U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_014_execute();
    puts("TC-CORE-014: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_014.c build/libarcrypto.a -o build/tc_core_014
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_014
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_014
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 14 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.15 Test Specification `TC-CORE-015` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `015`. The case uses a 248-byte allocation and corrupts digest byte offset 15. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 248 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_015_execute(void) {
    const size_t allocation_size = 248U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 15U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[15U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_015_execute();
    puts("TC-CORE-015: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_015.c build/libarcrypto.a -o build/tc_core_015
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_015
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_015
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 15 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.16 Test Specification `TC-CORE-016` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `016`. The case uses a 256-byte allocation and corrupts digest byte offset 16. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 256 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_016_execute(void) {
    const size_t allocation_size = 256U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 16U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[16U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_016_execute();
    puts("TC-CORE-016: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_016.c build/libarcrypto.a -o build/tc_core_016
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_016
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_016
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 16 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.17 Test Specification `TC-CORE-017` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `017`. The case uses a 264-byte allocation and corrupts digest byte offset 17. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 264 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_017_execute(void) {
    const size_t allocation_size = 264U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 17U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[17U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_017_execute();
    puts("TC-CORE-017: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_017.c build/libarcrypto.a -o build/tc_core_017
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_017
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_017
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 17 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.18 Test Specification `TC-CORE-018` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `018`. The case uses a 272-byte allocation and corrupts digest byte offset 18. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 272 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_018_execute(void) {
    const size_t allocation_size = 272U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 18U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[18U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_018_execute();
    puts("TC-CORE-018: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_018.c build/libarcrypto.a -o build/tc_core_018
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_018
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_018
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 18 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.19 Test Specification `TC-CORE-019` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `019`. The case uses a 280-byte allocation and corrupts digest byte offset 19. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 280 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_019_execute(void) {
    const size_t allocation_size = 280U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 19U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[19U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_019_execute();
    puts("TC-CORE-019: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_019.c build/libarcrypto.a -o build/tc_core_019
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_019
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_019
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 19 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.20 Test Specification `TC-CORE-020` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `020`. The case uses a 288-byte allocation and corrupts digest byte offset 20. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 288 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_020_execute(void) {
    const size_t allocation_size = 288U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 20U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[20U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_020_execute();
    puts("TC-CORE-020: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_020.c build/libarcrypto.a -o build/tc_core_020
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_020
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_020
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 20 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.21 Test Specification `TC-CORE-021` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `021`. The case uses a 296-byte allocation and corrupts digest byte offset 21. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 296 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_021_execute(void) {
    const size_t allocation_size = 296U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 21U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[21U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_021_execute();
    puts("TC-CORE-021: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_021.c build/libarcrypto.a -o build/tc_core_021
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_021
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_021
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 21 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.22 Test Specification `TC-CORE-022` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `022`. The case uses a 304-byte allocation and corrupts digest byte offset 22. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 304 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_022_execute(void) {
    const size_t allocation_size = 304U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 22U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[22U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_022_execute();
    puts("TC-CORE-022: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_022.c build/libarcrypto.a -o build/tc_core_022
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_022
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_022
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 22 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.23 Test Specification `TC-CORE-023` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `023`. The case uses a 312-byte allocation and corrupts digest byte offset 23. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 312 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_023_execute(void) {
    const size_t allocation_size = 312U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 23U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[23U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_023_execute();
    puts("TC-CORE-023: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_023.c build/libarcrypto.a -o build/tc_core_023
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_023
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_023
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 23 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.24 Test Specification `TC-CORE-024` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `024`. The case uses a 320-byte allocation and corrupts digest byte offset 24. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 320 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_024_execute(void) {
    const size_t allocation_size = 320U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 24U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[24U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_024_execute();
    puts("TC-CORE-024: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_024.c build/libarcrypto.a -o build/tc_core_024
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_024
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_024
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 24 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.25 Test Specification `TC-CORE-025` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `025`. The case uses a 328-byte allocation and corrupts digest byte offset 25. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 328 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_025_execute(void) {
    const size_t allocation_size = 328U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 25U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[25U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_025_execute();
    puts("TC-CORE-025: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_025.c build/libarcrypto.a -o build/tc_core_025
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_025
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_025
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 25 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.26 Test Specification `TC-CORE-026` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `026`. The case uses a 336-byte allocation and corrupts digest byte offset 26. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 336 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_026_execute(void) {
    const size_t allocation_size = 336U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 26U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[26U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_026_execute();
    puts("TC-CORE-026: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_026.c build/libarcrypto.a -o build/tc_core_026
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_026
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_026
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 26 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.27 Test Specification `TC-CORE-027` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `027`. The case uses a 344-byte allocation and corrupts digest byte offset 27. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 344 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_027_execute(void) {
    const size_t allocation_size = 344U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 27U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[27U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_027_execute();
    puts("TC-CORE-027: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_027.c build/libarcrypto.a -o build/tc_core_027
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_027
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_027
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 27 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.28 Test Specification `TC-CORE-028` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `028`. The case uses a 352-byte allocation and corrupts digest byte offset 28. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 352 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_028_execute(void) {
    const size_t allocation_size = 352U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 28U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[28U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_028_execute();
    puts("TC-CORE-028: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_028.c build/libarcrypto.a -o build/tc_core_028
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_028
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_028
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 28 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.29 Test Specification `TC-CORE-029` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `029`. The case uses a 360-byte allocation and corrupts digest byte offset 29. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 360 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_029_execute(void) {
    const size_t allocation_size = 360U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 29U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[29U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_029_execute();
    puts("TC-CORE-029: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_029.c build/libarcrypto.a -o build/tc_core_029
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_029
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_029
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 29 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.30 Test Specification `TC-CORE-030` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `030`. The case uses a 368-byte allocation and corrupts digest byte offset 30. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 368 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_030_execute(void) {
    const size_t allocation_size = 368U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 30U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[30U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_030_execute();
    puts("TC-CORE-030: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_030.c build/libarcrypto.a -o build/tc_core_030
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_030
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_030
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 30 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.31 Test Specification `TC-CORE-031` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `031`. The case uses a 376-byte allocation and corrupts digest byte offset 31. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 376 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_031_execute(void) {
    const size_t allocation_size = 376U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 31U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[31U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_031_execute();
    puts("TC-CORE-031: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_031.c build/libarcrypto.a -o build/tc_core_031
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_031
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_031
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 31 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.32 Test Specification `TC-CORE-032` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `032`. The case uses a 384-byte allocation and corrupts digest byte offset 32. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 384 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_032_execute(void) {
    const size_t allocation_size = 384U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 32U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[32U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_032_execute();
    puts("TC-CORE-032: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_032.c build/libarcrypto.a -o build/tc_core_032
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_032
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_032
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 32 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.33 Test Specification `TC-CORE-033` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `033`. The case uses a 392-byte allocation and corrupts digest byte offset 33. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 392 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_033_execute(void) {
    const size_t allocation_size = 392U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 33U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[33U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_033_execute();
    puts("TC-CORE-033: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_033.c build/libarcrypto.a -o build/tc_core_033
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_033
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_033
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 33 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.34 Test Specification `TC-CORE-034` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `034`. The case uses a 400-byte allocation and corrupts digest byte offset 34. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 400 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_034_execute(void) {
    const size_t allocation_size = 400U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 34U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[34U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_034_execute();
    puts("TC-CORE-034: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_034.c build/libarcrypto.a -o build/tc_core_034
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_034
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_034
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 34 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.35 Test Specification `TC-CORE-035` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `035`. The case uses a 408-byte allocation and corrupts digest byte offset 35. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 408 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_035_execute(void) {
    const size_t allocation_size = 408U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 35U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[35U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_035_execute();
    puts("TC-CORE-035: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_035.c build/libarcrypto.a -o build/tc_core_035
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_035
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_035
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 35 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.36 Test Specification `TC-CORE-036` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `036`. The case uses a 416-byte allocation and corrupts digest byte offset 36. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 416 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_036_execute(void) {
    const size_t allocation_size = 416U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 36U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[36U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_036_execute();
    puts("TC-CORE-036: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_036.c build/libarcrypto.a -o build/tc_core_036
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_036
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_036
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 36 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.37 Test Specification `TC-CORE-037` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `037`. The case uses a 424-byte allocation and corrupts digest byte offset 37. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 424 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_037_execute(void) {
    const size_t allocation_size = 424U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 37U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[37U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_037_execute();
    puts("TC-CORE-037: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_037.c build/libarcrypto.a -o build/tc_core_037
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_037
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_037
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 37 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.38 Test Specification `TC-CORE-038` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `038`. The case uses a 432-byte allocation and corrupts digest byte offset 38. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 432 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_038_execute(void) {
    const size_t allocation_size = 432U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 38U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[38U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_038_execute();
    puts("TC-CORE-038: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_038.c build/libarcrypto.a -o build/tc_core_038
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_038
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_038
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 38 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.39 Test Specification `TC-CORE-039` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `039`. The case uses a 440-byte allocation and corrupts digest byte offset 39. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 440 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_039_execute(void) {
    const size_t allocation_size = 440U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 39U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[39U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_039_execute();
    puts("TC-CORE-039: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_039.c build/libarcrypto.a -o build/tc_core_039
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_039
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_039
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 39 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.40 Test Specification `TC-CORE-040` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `040`. The case uses a 448-byte allocation and corrupts digest byte offset 40. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 448 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_040_execute(void) {
    const size_t allocation_size = 448U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 40U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[40U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_040_execute();
    puts("TC-CORE-040: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_040.c build/libarcrypto.a -o build/tc_core_040
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_040
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_040
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 40 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.41 Test Specification `TC-CORE-041` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `041`. The case uses a 456-byte allocation and corrupts digest byte offset 41. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 456 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_041_execute(void) {
    const size_t allocation_size = 456U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 41U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[41U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_041_execute();
    puts("TC-CORE-041: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_041.c build/libarcrypto.a -o build/tc_core_041
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_041
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_041
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 41 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.42 Test Specification `TC-CORE-042` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `042`. The case uses a 464-byte allocation and corrupts digest byte offset 42. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 464 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_042_execute(void) {
    const size_t allocation_size = 464U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 42U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[42U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_042_execute();
    puts("TC-CORE-042: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_042.c build/libarcrypto.a -o build/tc_core_042
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_042
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_042
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 42 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.43 Test Specification `TC-CORE-043` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `043`. The case uses a 472-byte allocation and corrupts digest byte offset 43. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 472 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_043_execute(void) {
    const size_t allocation_size = 472U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 43U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[43U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_043_execute();
    puts("TC-CORE-043: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_043.c build/libarcrypto.a -o build/tc_core_043
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_043
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_043
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 43 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.44 Test Specification `TC-CORE-044` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `044`. The case uses a 480-byte allocation and corrupts digest byte offset 44. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 480 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_044_execute(void) {
    const size_t allocation_size = 480U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 44U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[44U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_044_execute();
    puts("TC-CORE-044: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_044.c build/libarcrypto.a -o build/tc_core_044
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_044
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_044
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 44 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.45 Test Specification `TC-CORE-045` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `045`. The case uses a 488-byte allocation and corrupts digest byte offset 45. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 488 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_045_execute(void) {
    const size_t allocation_size = 488U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 45U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[45U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_045_execute();
    puts("TC-CORE-045: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_045.c build/libarcrypto.a -o build/tc_core_045
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_045
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_045
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 45 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.46 Test Specification `TC-CORE-046` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `046`. The case uses a 496-byte allocation and corrupts digest byte offset 46. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 496 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_046_execute(void) {
    const size_t allocation_size = 496U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 46U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[46U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_046_execute();
    puts("TC-CORE-046: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_046.c build/libarcrypto.a -o build/tc_core_046
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_046
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_046
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 46 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.47 Test Specification `TC-CORE-047` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `047`. The case uses a 504-byte allocation and corrupts digest byte offset 47. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 504 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_047_execute(void) {
    const size_t allocation_size = 504U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 47U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[47U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_047_execute();
    puts("TC-CORE-047: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_047.c build/libarcrypto.a -o build/tc_core_047
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_047
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_047
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 47 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.48 Test Specification `TC-CORE-048` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `048`. The case uses a 512-byte allocation and corrupts digest byte offset 48. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 512 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_048_execute(void) {
    const size_t allocation_size = 512U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 48U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[48U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_048_execute();
    puts("TC-CORE-048: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_048.c build/libarcrypto.a -o build/tc_core_048
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_048
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_048
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 48 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.49 Test Specification `TC-CORE-049` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `049`. The case uses a 520-byte allocation and corrupts digest byte offset 49. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 520 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_049_execute(void) {
    const size_t allocation_size = 520U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 49U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[49U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_049_execute();
    puts("TC-CORE-049: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_049.c build/libarcrypto.a -o build/tc_core_049
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_049
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_049
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 49 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.50 Test Specification `TC-CORE-050` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `050`. The case uses a 528-byte allocation and corrupts digest byte offset 50. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 528 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_050_execute(void) {
    const size_t allocation_size = 528U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 50U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[50U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_050_execute();
    puts("TC-CORE-050: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_050.c build/libarcrypto.a -o build/tc_core_050
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_050
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_050
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 50 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.51 Test Specification `TC-CORE-051` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `051`. The case uses a 536-byte allocation and corrupts digest byte offset 51. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 536 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_051_execute(void) {
    const size_t allocation_size = 536U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 51U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[51U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_051_execute();
    puts("TC-CORE-051: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_051.c build/libarcrypto.a -o build/tc_core_051
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_051
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_051
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 51 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.52 Test Specification `TC-CORE-052` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `052`. The case uses a 544-byte allocation and corrupts digest byte offset 52. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 544 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_052_execute(void) {
    const size_t allocation_size = 544U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 52U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[52U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_052_execute();
    puts("TC-CORE-052: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_052.c build/libarcrypto.a -o build/tc_core_052
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_052
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_052
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 52 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.53 Test Specification `TC-CORE-053` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `053`. The case uses a 552-byte allocation and corrupts digest byte offset 53. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 552 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_053_execute(void) {
    const size_t allocation_size = 552U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 53U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[53U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_053_execute();
    puts("TC-CORE-053: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_053.c build/libarcrypto.a -o build/tc_core_053
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_053
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_053
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 53 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.54 Test Specification `TC-CORE-054` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `054`. The case uses a 560-byte allocation and corrupts digest byte offset 54. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 560 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_054_execute(void) {
    const size_t allocation_size = 560U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 54U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[54U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_054_execute();
    puts("TC-CORE-054: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_054.c build/libarcrypto.a -o build/tc_core_054
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_054
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_054
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 54 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.55 Test Specification `TC-CORE-055` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `055`. The case uses a 568-byte allocation and corrupts digest byte offset 55. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 568 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_055_execute(void) {
    const size_t allocation_size = 568U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 55U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[55U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_055_execute();
    puts("TC-CORE-055: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_055.c build/libarcrypto.a -o build/tc_core_055
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_055
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_055
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 55 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.56 Test Specification `TC-CORE-056` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `056`. The case uses a 576-byte allocation and corrupts digest byte offset 56. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 576 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_056_execute(void) {
    const size_t allocation_size = 576U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 56U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[56U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_056_execute();
    puts("TC-CORE-056: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_056.c build/libarcrypto.a -o build/tc_core_056
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_056
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_056
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 56 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.57 Test Specification `TC-CORE-057` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `057`. The case uses a 584-byte allocation and corrupts digest byte offset 57. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 584 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_057_execute(void) {
    const size_t allocation_size = 584U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 57U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[57U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_057_execute();
    puts("TC-CORE-057: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_057.c build/libarcrypto.a -o build/tc_core_057
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_057
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_057
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 57 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.58 Test Specification `TC-CORE-058` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `058`. The case uses a 592-byte allocation and corrupts digest byte offset 58. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 592 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_058_execute(void) {
    const size_t allocation_size = 592U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 58U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[58U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_058_execute();
    puts("TC-CORE-058: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_058.c build/libarcrypto.a -o build/tc_core_058
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_058
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_058
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 58 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.59 Test Specification `TC-CORE-059` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `059`. The case uses a 600-byte allocation and corrupts digest byte offset 59. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 600 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_059_execute(void) {
    const size_t allocation_size = 600U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 59U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[59U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_059_execute();
    puts("TC-CORE-059: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_059.c build/libarcrypto.a -o build/tc_core_059
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_059
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_059
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 59 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.60 Test Specification `TC-CORE-060` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `060`. The case uses a 608-byte allocation and corrupts digest byte offset 60. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 608 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_060_execute(void) {
    const size_t allocation_size = 608U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 60U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[60U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_060_execute();
    puts("TC-CORE-060: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_060.c build/libarcrypto.a -o build/tc_core_060
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_060
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_060
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 60 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.61 Test Specification `TC-CORE-061` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `061`. The case uses a 616-byte allocation and corrupts digest byte offset 61. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 616 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_061_execute(void) {
    const size_t allocation_size = 616U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 61U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[61U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_061_execute();
    puts("TC-CORE-061: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_061.c build/libarcrypto.a -o build/tc_core_061
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_061
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_061
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 61 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.62 Test Specification `TC-CORE-062` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `062`. The case uses a 624-byte allocation and corrupts digest byte offset 62. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 624 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_062_execute(void) {
    const size_t allocation_size = 624U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 62U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[62U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_062_execute();
    puts("TC-CORE-062: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_062.c build/libarcrypto.a -o build/tc_core_062
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_062
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_062
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 62 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.63 Test Specification `TC-CORE-063` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `063`. The case uses a 632-byte allocation and corrupts digest byte offset 63. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 632 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_063_execute(void) {
    const size_t allocation_size = 632U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 63U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[63U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_063_execute();
    puts("TC-CORE-063: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_063.c build/libarcrypto.a -o build/tc_core_063
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_063
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_063
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 63 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.64 Test Specification `TC-CORE-064` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `064`. The case uses a 128-byte allocation and corrupts digest byte offset 0. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 128 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_064_execute(void) {
    const size_t allocation_size = 128U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 64U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[0U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_064_execute();
    puts("TC-CORE-064: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_064.c build/libarcrypto.a -o build/tc_core_064
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_064
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_064
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 0 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.65 Test Specification `TC-CORE-065` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `065`. The case uses a 136-byte allocation and corrupts digest byte offset 1. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 136 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_065_execute(void) {
    const size_t allocation_size = 136U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 65U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[1U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_065_execute();
    puts("TC-CORE-065: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_065.c build/libarcrypto.a -o build/tc_core_065
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_065
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_065
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 1 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.66 Test Specification `TC-CORE-066` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `066`. The case uses a 144-byte allocation and corrupts digest byte offset 2. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 144 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_066_execute(void) {
    const size_t allocation_size = 144U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 66U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[2U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_066_execute();
    puts("TC-CORE-066: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_066.c build/libarcrypto.a -o build/tc_core_066
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_066
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_066
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 2 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.67 Test Specification `TC-CORE-067` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `067`. The case uses a 152-byte allocation and corrupts digest byte offset 3. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 152 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_067_execute(void) {
    const size_t allocation_size = 152U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 67U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[3U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_067_execute();
    puts("TC-CORE-067: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_067.c build/libarcrypto.a -o build/tc_core_067
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_067
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_067
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 3 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.68 Test Specification `TC-CORE-068` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `068`. The case uses a 160-byte allocation and corrupts digest byte offset 4. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 160 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_068_execute(void) {
    const size_t allocation_size = 160U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 68U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[4U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_068_execute();
    puts("TC-CORE-068: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_068.c build/libarcrypto.a -o build/tc_core_068
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_068
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_068
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 4 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.69 Test Specification `TC-CORE-069` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `069`. The case uses a 168-byte allocation and corrupts digest byte offset 5. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 168 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_069_execute(void) {
    const size_t allocation_size = 168U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 69U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[5U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_069_execute();
    puts("TC-CORE-069: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_069.c build/libarcrypto.a -o build/tc_core_069
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_069
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_069
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 5 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.70 Test Specification `TC-CORE-070` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `070`. The case uses a 176-byte allocation and corrupts digest byte offset 6. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 176 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_070_execute(void) {
    const size_t allocation_size = 176U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 70U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[6U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_070_execute();
    puts("TC-CORE-070: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_070.c build/libarcrypto.a -o build/tc_core_070
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_070
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_070
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 6 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.71 Test Specification `TC-CORE-071` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `071`. The case uses a 184-byte allocation and corrupts digest byte offset 7. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 184 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_071_execute(void) {
    const size_t allocation_size = 184U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 71U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[7U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_071_execute();
    puts("TC-CORE-071: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_071.c build/libarcrypto.a -o build/tc_core_071
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_071
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_071
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 7 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.72 Test Specification `TC-CORE-072` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `072`. The case uses a 192-byte allocation and corrupts digest byte offset 8. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 192 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_072_execute(void) {
    const size_t allocation_size = 192U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 72U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[8U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_072_execute();
    puts("TC-CORE-072: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_072.c build/libarcrypto.a -o build/tc_core_072
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_072
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_072
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 8 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.73 Test Specification `TC-CORE-073` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `073`. The case uses a 200-byte allocation and corrupts digest byte offset 9. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 200 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_073_execute(void) {
    const size_t allocation_size = 200U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 73U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[9U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_073_execute();
    puts("TC-CORE-073: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_073.c build/libarcrypto.a -o build/tc_core_073
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_073
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_073
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 9 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.74 Test Specification `TC-CORE-074` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `074`. The case uses a 208-byte allocation and corrupts digest byte offset 10. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 208 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_074_execute(void) {
    const size_t allocation_size = 208U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 74U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[10U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_074_execute();
    puts("TC-CORE-074: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_074.c build/libarcrypto.a -o build/tc_core_074
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_074
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_074
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 10 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.75 Test Specification `TC-CORE-075` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `075`. The case uses a 216-byte allocation and corrupts digest byte offset 11. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 216 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_075_execute(void) {
    const size_t allocation_size = 216U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 75U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[11U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_075_execute();
    puts("TC-CORE-075: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_075.c build/libarcrypto.a -o build/tc_core_075
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_075
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_075
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 11 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.76 Test Specification `TC-CORE-076` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `076`. The case uses a 224-byte allocation and corrupts digest byte offset 12. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 224 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_076_execute(void) {
    const size_t allocation_size = 224U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 76U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[12U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_076_execute();
    puts("TC-CORE-076: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_076.c build/libarcrypto.a -o build/tc_core_076
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_076
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_076
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 12 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.77 Test Specification `TC-CORE-077` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `077`. The case uses a 232-byte allocation and corrupts digest byte offset 13. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 232 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_077_execute(void) {
    const size_t allocation_size = 232U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 77U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[13U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_077_execute();
    puts("TC-CORE-077: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_077.c build/libarcrypto.a -o build/tc_core_077
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_077
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_077
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 13 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.78 Test Specification `TC-CORE-078` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `078`. The case uses a 240-byte allocation and corrupts digest byte offset 14. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 240 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_078_execute(void) {
    const size_t allocation_size = 240U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 78U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[14U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_078_execute();
    puts("TC-CORE-078: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_078.c build/libarcrypto.a -o build/tc_core_078
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_078
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_078
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 14 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.79 Test Specification `TC-CORE-079` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `079`. The case uses a 248-byte allocation and corrupts digest byte offset 15. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 248 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_079_execute(void) {
    const size_t allocation_size = 248U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 79U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[15U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_079_execute();
    puts("TC-CORE-079: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_079.c build/libarcrypto.a -o build/tc_core_079
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_079
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_079
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 15 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.80 Test Specification `TC-CORE-080` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `080`. The case uses a 256-byte allocation and corrupts digest byte offset 16. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 256 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_080_execute(void) {
    const size_t allocation_size = 256U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 80U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[16U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_080_execute();
    puts("TC-CORE-080: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_080.c build/libarcrypto.a -o build/tc_core_080
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_080
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_080
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 16 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.81 Test Specification `TC-CORE-081` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `081`. The case uses a 264-byte allocation and corrupts digest byte offset 17. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 264 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_081_execute(void) {
    const size_t allocation_size = 264U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 81U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[17U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_081_execute();
    puts("TC-CORE-081: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_081.c build/libarcrypto.a -o build/tc_core_081
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_081
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_081
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 17 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.82 Test Specification `TC-CORE-082` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `082`. The case uses a 272-byte allocation and corrupts digest byte offset 18. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 272 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_082_execute(void) {
    const size_t allocation_size = 272U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 82U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[18U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_082_execute();
    puts("TC-CORE-082: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_082.c build/libarcrypto.a -o build/tc_core_082
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_082
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_082
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 18 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.83 Test Specification `TC-CORE-083` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `083`. The case uses a 280-byte allocation and corrupts digest byte offset 19. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 280 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_083_execute(void) {
    const size_t allocation_size = 280U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 83U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[19U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_083_execute();
    puts("TC-CORE-083: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_083.c build/libarcrypto.a -o build/tc_core_083
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_083
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_083
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 19 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.84 Test Specification `TC-CORE-084` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `084`. The case uses a 288-byte allocation and corrupts digest byte offset 20. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 288 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_084_execute(void) {
    const size_t allocation_size = 288U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 84U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[20U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_084_execute();
    puts("TC-CORE-084: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_084.c build/libarcrypto.a -o build/tc_core_084
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_084
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_084
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 20 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.85 Test Specification `TC-CORE-085` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `085`. The case uses a 296-byte allocation and corrupts digest byte offset 21. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 296 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_085_execute(void) {
    const size_t allocation_size = 296U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 85U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[21U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_085_execute();
    puts("TC-CORE-085: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_085.c build/libarcrypto.a -o build/tc_core_085
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_085
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_085
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 21 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.86 Test Specification `TC-CORE-086` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `086`. The case uses a 304-byte allocation and corrupts digest byte offset 22. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 304 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_086_execute(void) {
    const size_t allocation_size = 304U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 86U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[22U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_086_execute();
    puts("TC-CORE-086: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_086.c build/libarcrypto.a -o build/tc_core_086
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_086
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_086
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 22 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.87 Test Specification `TC-CORE-087` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `087`. The case uses a 312-byte allocation and corrupts digest byte offset 23. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 312 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_087_execute(void) {
    const size_t allocation_size = 312U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 87U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[23U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_087_execute();
    puts("TC-CORE-087: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_087.c build/libarcrypto.a -o build/tc_core_087
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_087
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_087
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 23 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.88 Test Specification `TC-CORE-088` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `088`. The case uses a 320-byte allocation and corrupts digest byte offset 24. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 320 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_088_execute(void) {
    const size_t allocation_size = 320U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 88U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[24U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_088_execute();
    puts("TC-CORE-088: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_088.c build/libarcrypto.a -o build/tc_core_088
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_088
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_088
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 24 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.89 Test Specification `TC-CORE-089` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `089`. The case uses a 328-byte allocation and corrupts digest byte offset 25. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 328 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_089_execute(void) {
    const size_t allocation_size = 328U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 89U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[25U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_089_execute();
    puts("TC-CORE-089: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_089.c build/libarcrypto.a -o build/tc_core_089
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_089
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_089
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 25 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.90 Test Specification `TC-CORE-090` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `090`. The case uses a 336-byte allocation and corrupts digest byte offset 26. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 336 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_090_execute(void) {
    const size_t allocation_size = 336U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 90U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[26U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_090_execute();
    puts("TC-CORE-090: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_090.c build/libarcrypto.a -o build/tc_core_090
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_090
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_090
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 26 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.91 Test Specification `TC-CORE-091` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `091`. The case uses a 344-byte allocation and corrupts digest byte offset 27. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 344 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_091_execute(void) {
    const size_t allocation_size = 344U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 91U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[27U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_091_execute();
    puts("TC-CORE-091: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_091.c build/libarcrypto.a -o build/tc_core_091
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_091
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_091
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 27 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.92 Test Specification `TC-CORE-092` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `092`. The case uses a 352-byte allocation and corrupts digest byte offset 28. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 352 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_092_execute(void) {
    const size_t allocation_size = 352U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 92U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[28U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_092_execute();
    puts("TC-CORE-092: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_092.c build/libarcrypto.a -o build/tc_core_092
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_092
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_092
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 28 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.93 Test Specification `TC-CORE-093` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `093`. The case uses a 360-byte allocation and corrupts digest byte offset 29. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 360 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_093_execute(void) {
    const size_t allocation_size = 360U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 93U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[29U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_093_execute();
    puts("TC-CORE-093: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_093.c build/libarcrypto.a -o build/tc_core_093
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_093
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_093
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 29 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.94 Test Specification `TC-CORE-094` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `094`. The case uses a 368-byte allocation and corrupts digest byte offset 30. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 368 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_094_execute(void) {
    const size_t allocation_size = 368U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 94U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[30U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_094_execute();
    puts("TC-CORE-094: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_094.c build/libarcrypto.a -o build/tc_core_094
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_094
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_094
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 30 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.95 Test Specification `TC-CORE-095` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `095`. The case uses a 376-byte allocation and corrupts digest byte offset 31. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 376 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_095_execute(void) {
    const size_t allocation_size = 376U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 95U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[31U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_095_execute();
    puts("TC-CORE-095: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_095.c build/libarcrypto.a -o build/tc_core_095
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_095
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_095
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 31 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.96 Test Specification `TC-CORE-096` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `096`. The case uses a 384-byte allocation and corrupts digest byte offset 32. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 384 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_096_execute(void) {
    const size_t allocation_size = 384U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 96U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[32U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_096_execute();
    puts("TC-CORE-096: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_096.c build/libarcrypto.a -o build/tc_core_096
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_096
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_096
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 32 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.97 Test Specification `TC-CORE-097` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `097`. The case uses a 392-byte allocation and corrupts digest byte offset 33. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 392 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_097_execute(void) {
    const size_t allocation_size = 392U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 97U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[33U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_097_execute();
    puts("TC-CORE-097: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_097.c build/libarcrypto.a -o build/tc_core_097
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_097
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_097
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 33 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.98 Test Specification `TC-CORE-098` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `098`. The case uses a 400-byte allocation and corrupts digest byte offset 34. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 400 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_098_execute(void) {
    const size_t allocation_size = 400U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 98U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[34U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_098_execute();
    puts("TC-CORE-098: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_098.c build/libarcrypto.a -o build/tc_core_098
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_098
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_098
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 34 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.99 Test Specification `TC-CORE-099` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `099`. The case uses a 408-byte allocation and corrupts digest byte offset 35. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 408 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_099_execute(void) {
    const size_t allocation_size = 408U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 99U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[35U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_099_execute();
    puts("TC-CORE-099: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_099.c build/libarcrypto.a -o build/tc_core_099
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_099
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_099
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 35 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.100 Test Specification `TC-CORE-100` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `100`. The case uses a 416-byte allocation and corrupts digest byte offset 36. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 416 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_100_execute(void) {
    const size_t allocation_size = 416U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 100U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[36U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_100_execute();
    puts("TC-CORE-100: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_100.c build/libarcrypto.a -o build/tc_core_100
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_100
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_100
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 36 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.101 Test Specification `TC-CORE-101` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `101`. The case uses a 424-byte allocation and corrupts digest byte offset 37. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 424 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_101_execute(void) {
    const size_t allocation_size = 424U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 101U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[37U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_101_execute();
    puts("TC-CORE-101: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_101.c build/libarcrypto.a -o build/tc_core_101
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_101
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_101
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 37 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.102 Test Specification `TC-CORE-102` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `102`. The case uses a 432-byte allocation and corrupts digest byte offset 38. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 432 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_102_execute(void) {
    const size_t allocation_size = 432U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 102U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[38U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_102_execute();
    puts("TC-CORE-102: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_102.c build/libarcrypto.a -o build/tc_core_102
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_102
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_102
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 38 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.103 Test Specification `TC-CORE-103` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `103`. The case uses a 440-byte allocation and corrupts digest byte offset 39. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 440 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_103_execute(void) {
    const size_t allocation_size = 440U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 103U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[39U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_103_execute();
    puts("TC-CORE-103: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_103.c build/libarcrypto.a -o build/tc_core_103
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_103
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_103
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 39 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.104 Test Specification `TC-CORE-104` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `104`. The case uses a 448-byte allocation and corrupts digest byte offset 40. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 448 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_104_execute(void) {
    const size_t allocation_size = 448U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 104U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[40U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_104_execute();
    puts("TC-CORE-104: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_104.c build/libarcrypto.a -o build/tc_core_104
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_104
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_104
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 40 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.105 Test Specification `TC-CORE-105` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `105`. The case uses a 456-byte allocation and corrupts digest byte offset 41. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 456 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_105_execute(void) {
    const size_t allocation_size = 456U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 105U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[41U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_105_execute();
    puts("TC-CORE-105: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_105.c build/libarcrypto.a -o build/tc_core_105
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_105
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_105
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 41 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.106 Test Specification `TC-CORE-106` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `106`. The case uses a 464-byte allocation and corrupts digest byte offset 42. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 464 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_106_execute(void) {
    const size_t allocation_size = 464U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 106U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[42U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_106_execute();
    puts("TC-CORE-106: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_106.c build/libarcrypto.a -o build/tc_core_106
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_106
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_106
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 42 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.107 Test Specification `TC-CORE-107` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `107`. The case uses a 472-byte allocation and corrupts digest byte offset 43. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 472 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_107_execute(void) {
    const size_t allocation_size = 472U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 107U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[43U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_107_execute();
    puts("TC-CORE-107: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_107.c build/libarcrypto.a -o build/tc_core_107
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_107
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_107
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 43 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.108 Test Specification `TC-CORE-108` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `108`. The case uses a 480-byte allocation and corrupts digest byte offset 44. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 480 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_108_execute(void) {
    const size_t allocation_size = 480U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 108U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[44U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_108_execute();
    puts("TC-CORE-108: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_108.c build/libarcrypto.a -o build/tc_core_108
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_108
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_108
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 44 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.109 Test Specification `TC-CORE-109` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `109`. The case uses a 488-byte allocation and corrupts digest byte offset 45. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 488 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_109_execute(void) {
    const size_t allocation_size = 488U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 109U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[45U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_109_execute();
    puts("TC-CORE-109: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_109.c build/libarcrypto.a -o build/tc_core_109
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_109
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_109
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 45 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.110 Test Specification `TC-CORE-110` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `110`. The case uses a 496-byte allocation and corrupts digest byte offset 46. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 496 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_110_execute(void) {
    const size_t allocation_size = 496U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 110U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[46U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_110_execute();
    puts("TC-CORE-110: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_110.c build/libarcrypto.a -o build/tc_core_110
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_110
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_110
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 46 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.111 Test Specification `TC-CORE-111` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `111`. The case uses a 504-byte allocation and corrupts digest byte offset 47. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 504 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_111_execute(void) {
    const size_t allocation_size = 504U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 111U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[47U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_111_execute();
    puts("TC-CORE-111: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_111.c build/libarcrypto.a -o build/tc_core_111
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_111
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_111
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 47 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.112 Test Specification `TC-CORE-112` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `112`. The case uses a 512-byte allocation and corrupts digest byte offset 48. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 512 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_112_execute(void) {
    const size_t allocation_size = 512U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 112U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[48U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_112_execute();
    puts("TC-CORE-112: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_112.c build/libarcrypto.a -o build/tc_core_112
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_112
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_112
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 48 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.113 Test Specification `TC-CORE-113` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `113`. The case uses a 520-byte allocation and corrupts digest byte offset 49. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 520 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_113_execute(void) {
    const size_t allocation_size = 520U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 113U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[49U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_113_execute();
    puts("TC-CORE-113: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_113.c build/libarcrypto.a -o build/tc_core_113
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_113
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_113
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 49 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.114 Test Specification `TC-CORE-114` — Cryptographic and Memory Invariant

**Owner:** Squad 3  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `114`. The case uses a 528-byte allocation and corrupts digest byte offset 50. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 528 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_114_execute(void) {
    const size_t allocation_size = 528U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 114U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[50U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_114_execute();
    puts("TC-CORE-114: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_114.c build/libarcrypto.a -o build/tc_core_114
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_114
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_114
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 50 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.115 Test Specification `TC-CORE-115` — Cryptographic and Memory Invariant

**Owner:** Squad 4  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `115`. The case uses a 536-byte allocation and corrupts digest byte offset 51. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 536 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_115_execute(void) {
    const size_t allocation_size = 536U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 115U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[51U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_115_execute();
    puts("TC-CORE-115: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_115.c build/libarcrypto.a -o build/tc_core_115
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_115
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_115
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 51 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.116 Test Specification `TC-CORE-116` — Cryptographic and Memory Invariant

**Owner:** Squad 5  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `116`. The case uses a 544-byte allocation and corrupts digest byte offset 52. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 544 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_116_execute(void) {
    const size_t allocation_size = 544U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 116U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[52U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_116_execute();
    puts("TC-CORE-116: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_116.c build/libarcrypto.a -o build/tc_core_116
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_116
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_116
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 52 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.117 Test Specification `TC-CORE-117` — Cryptographic and Memory Invariant

**Owner:** Squad 6  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `117`. The case uses a 552-byte allocation and corrupts digest byte offset 53. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 552 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_117_execute(void) {
    const size_t allocation_size = 552U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 117U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[53U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_117_execute();
    puts("TC-CORE-117: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_117.c build/libarcrypto.a -o build/tc_core_117
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_117
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_117
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 53 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.118 Test Specification `TC-CORE-118` — Cryptographic and Memory Invariant

**Owner:** Squad 7  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `118`. The case uses a 560-byte allocation and corrupts digest byte offset 54. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 560 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_118_execute(void) {
    const size_t allocation_size = 560U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 118U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[54U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_118_execute();
    puts("TC-CORE-118: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_118.c build/libarcrypto.a -o build/tc_core_118
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_118
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_118
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 54 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.119 Test Specification `TC-CORE-119` — Cryptographic and Memory Invariant

**Owner:** Squad 1  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `119`. The case uses a 568-byte allocation and corrupts digest byte offset 55. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 568 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_119_execute(void) {
    const size_t allocation_size = 568U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 119U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[55U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_119_execute();
    puts("TC-CORE-119: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_119.c build/libarcrypto.a -o build/tc_core_119
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_119
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_119
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 55 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

## 14.120 Test Specification `TC-CORE-120` — Cryptographic and Memory Invariant

**Owner:** Squad 2  
**Reviewers:** Squad 7 Offensive Security and one cross-squad maintainer  
**Profiles:** `SOVEREIGN-MAX`, `ENTERPRISE-BALANCED`, `EDGE-PERFORMANCE`  
**Maximum execution time:** 15 milliseconds on the reference x86_64 host  
**Failure policy:** fail closed; no retry may convert a deterministic failure into a pass  

### Purpose
Validate constant-time equality semantics, deterministic SHA-512 output, zeroization, heap ownership, and exact cleanup for subsystem instance `120`. The case uses a 576-byte allocation and corrupts digest byte offset 56. The test is deterministic and requires no network, wall-clock input, or external entropy.

### Preconditions
1. `libarcrypto` was built with C11 warnings promoted to errors.
2. AddressSanitizer and UndefinedBehaviorSanitizer are available.
3. The process has permission to allocate 576 bytes.
4. The cryptographic self-test completed successfully before this test begins.

```c
/* Copyright (c) 2026 ALRI Development. All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alrios/crypto.h"

static void tc_core_120_execute(void) {
    const size_t allocation_size = 576U;
    uint8_t *arena = (uint8_t *)malloc(allocation_size);
    assert(arena != NULL);

    for (size_t index = 0; index < allocation_size; ++index) {
        arena[index] = (uint8_t)((index ^ 0x5AU ^ 120U) & 0xFFU);
    }

    uint8_t digest[SHA512_DIGEST_LEN];
    uint8_t comparison[SHA512_DIGEST_LEN];
    assert(alrios_sha512(arena, allocation_size, digest) == ALRIOS_CRYPTO_OK);
    memcpy(comparison, digest, sizeof(comparison));
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) == 0);

    comparison[56U] ^= 0x01U;
    assert(alrios_constant_time_memcmp(digest, comparison, sizeof(digest)) != 0);

    alrios_explicit_zeroize(arena, allocation_size);
    for (size_t index = 0; index < allocation_size; ++index) {
        assert(arena[index] == 0U);
    }

    alrios_explicit_zeroize(digest, sizeof(digest));
    alrios_explicit_zeroize(comparison, sizeof(comparison));
    free(arena);
}

int main(void) {
    tc_core_120_execute();
    puts("TC-CORE-120: PASS");
    return 0;
}
```

### Build command
```bash
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wconversion -Wshadow -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude tests/tc_core_120.c build/libarcrypto.a -o build/tc_core_120
```

### Runtime commands
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tc_core_120
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./build/tc_core_120
```

### Acceptance criteria
1. Process exit status equals zero.
2. Exactly one `PASS` record is emitted.
3. AddressSanitizer reports no invalid access and no leak.
4. UndefinedBehaviorSanitizer reports no undefined operation.
5. Valgrind reports zero definitely, indirectly, possibly, or still-reachable lost bytes.
6. Corrupting byte 56 causes the comparison to fail.
7. Every byte in the allocated region equals zero before `free`.
8. The test remains deterministic across 1,000 consecutive executions.

### Negative conditions
1. Null pointers supplied to constant-time comparison must return a nonzero error.
2. A zero-length request must not dereference either input pointer.
3. Integer conversion must not truncate `allocation_size`.
4. Compiler optimization must not remove zeroization writes.

---

# SECTION 15: RELEASE GATES, GOVERNANCE, AND OPEN-SOURCE ACCEPTANCE

## 15.1 Mandatory pull-request gates

1. Every changed C translation unit compiles with `-std=c11 -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes`.
2. Every binary parser has a LibFuzzer target and a fixed corpus containing empty, truncated, oversized, noncanonical, and valid messages.
3. Every cryptographic primitive executes known-answer tests before serving requests.
4. Every privilege transition emits a hash-chained audit record.
5. Every release artifact is reproducible on two independent builders.
6. `armake` cannot access signing keys; `arsign` cannot invoke a compiler.
7. DevMode cannot be enabled on a production node through runtime IPC.
8. A production profile downgrade requires offline administrative authorization and is audit-visible.
9. No `.env`, database, private key, token, or deployment credential may enter an `.arapp`.
10. Persistent state migrations require forward and backward compatibility tests and an independently tested rollback procedure.

## 15.2 Open-source project invariants

ALRIOS is not a commercial product and shall not introduce feature gates, telemetry locks, proprietary signing dependencies, or vendor-exclusive root authorities. Every organization may compile a sovereign distribution, inject its own public trust anchors, operate its own certificate authorities, and audit every security decision in source form. Compatibility claims require conformance tests, not payment or centralized authorization.

## 15.3 Definition of done

The architecture reaches release readiness only when all protocol structures have version negotiation, all parsers reject noncanonical encodings, all secret buffers are bounded and zeroized, all deployments support rollback, all production profiles fail closed, and the full security suite completes without warnings, crashes, leaks, races, or flaky outcomes.

---

*End of ALRIOS SDD Master Specification.*


---

# SECTION 16: `v0.3.x` REACTIVE HOOKS, EVENT BUS & DELTA UPDATE SUBSYSTEM

## 16.1 Architectural Directive

The ALRIOS core, `armake`, `arpm`, and `arcore` must remain agnostic regarding user-space build engines. No kernel-adjacent component may contain hardcoded references to `arwe`, `$ARWE_BUILD`, `config.arwe`, Rust, WASM, Node, TypeScript, or any future build pipeline. Extension must occur through deterministic hooks, manifests, and IPC event contracts.

## 16.2 Canonical Hook Tree

```text
arcore/etc/hooks.d/
├── pre-build/
├── post-build/
├── pre-swap/
├── post-swap/
├── pre-drain/
└── post-drain/
```

Hook execution order is lexicographic byte order by filename after filtering for executable permission and signature validity. In `SOVEREIGN-MAX`, every hook must be signed by a trusted CA. In `ENTERPRISE-BALANCED`, every hook must pass Ed25519 verification. In `EDGE-PERFORMANCE`, hooks are disabled unless explicitly enabled per node.

## 16.3 Hook Contract Header: `include/alrios/hooks.h`

```c
/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#ifndef ALRIOS_HOOKS_H
#define ALRIOS_HOOKS_H

#include <stdint.h>
#include <stddef.h>

#define ALRIOS_HOOK_MAX_NAME_LEN       64
#define ALRIOS_HOOK_MAX_PATH_LEN       512
#define ALRIOS_HOOK_MAX_ENV_LEN        4096
#define ALRIOS_HOOK_TIMEOUT_DEFAULT_MS 5000U

#define ALRIOS_HOOK_OK                 0
#define ALRIOS_HOOK_ERR_INVALID       -3001
#define ALRIOS_HOOK_ERR_TIMEOUT       -3002
#define ALRIOS_HOOK_ERR_SIGNATURE     -3003
#define ALRIOS_HOOK_ERR_EXEC          -3004
#define ALRIOS_HOOK_ERR_ABORT         -3005

typedef enum alrios_hook_stage {
    ALRIOS_HOOK_PRE_BUILD  = 0x0001,
    ALRIOS_HOOK_POST_BUILD = 0x0002,
    ALRIOS_HOOK_PRE_SWAP   = 0x0003,
    ALRIOS_HOOK_POST_SWAP  = 0x0004,
    ALRIOS_HOOK_PRE_DRAIN  = 0x0005,
    ALRIOS_HOOK_POST_DRAIN = 0x0006
} alrios_hook_stage_t;

typedef struct alrios_hook_invocation {
    alrios_hook_stage_t stage;
    uint64_t deploy_transaction_id;
    uint32_t timeout_ms;
    char app_id[ALRIOS_HOOK_MAX_NAME_LEN];
    char old_slot_path[ALRIOS_HOOK_MAX_PATH_LEN];
    char new_slot_path[ALRIOS_HOOK_MAX_PATH_LEN];
    char environment_block[ALRIOS_HOOK_MAX_ENV_LEN];
} alrios_hook_invocation_t;

typedef struct alrios_hook_result {
    int32_t status_code;
    uint32_t elapsed_ms;
    uint32_t stderr_len;
    uint8_t stderr_sha256[32];
} alrios_hook_result_t;

int alrios_hook_run_stage(const alrios_hook_invocation_t *inv, alrios_hook_result_t *out_result);

#endif /* ALRIOS_HOOKS_H */
```

## 16.4 Hook Execution Semantics

1. `pre-build` and `post-build` are toolchain lifecycle hooks. Failure aborts packaging.
2. `pre-swap` is a transactional gate. Failure aborts promotion and keeps the active slot untouched.
3. `post-swap` is a notification stage. Failure is logged but does not roll back by default unless the hook is marked `blocking=true` in policy.
4. `pre-drain` is allowed to block for a bounded timeout while buffers are flushed.
5. `post-drain` is best-effort and always asynchronous in performance profile.
6. Every blocking hook must have a deterministic timeout. Unbounded hook execution is forbidden.
7. Hook stdout is non-authoritative. Only exit code and signed event payloads affect control flow.
8. Hooks cannot mutate `.arapp` payloads after signature verification.

## 16.5 Reactive Event Bus IPC

The ALRIOS event bus is a RAM-resident pub/sub fabric over `AF_UNIX` `SOCK_SEQPACKET` in `SOVEREIGN-MAX` and `ENTERPRISE-BALANCED`, and a lock-free shared memory ring in `EDGE-PERFORMANCE`.

### IPC opcodes

```c
#define IPC_EVENT_SUBSCRIBE   0x0301U
#define IPC_EVENT_UNSUBSCRIBE 0x0302U
#define IPC_EVENT_EMIT        0x0303U
#define IPC_EVENT_ACK         0x0304U
#define IPC_EVENT_NACK        0x0305U
```

### Event identifiers

```c
#define EVENT_APP_DEPLOY_STARTED             0x1001U
#define EVENT_APP_DEPLOYED                   0x1002U
#define EVENT_APP_DEPLOY_FAILED              0x1003U
#define EVENT_APP_CRASHED                    0x1004U
#define EVENT_APP_DRAIN_STARTED              0x1005U
#define EVENT_APP_DRAIN_COMPLETED            0x1006U
#define EVENT_HOOK_STARTED                   0x1101U
#define EVENT_HOOK_COMPLETED                 0x1102U
#define EVENT_HOOK_FAILED                    0x1103U
#define EVENT_STORAGE_MIGRATION_STARTED      0x1201U
#define EVENT_STORAGE_MIGRATION_COMPLETED    0x1202U
#define EVENT_CDN_PURGE_REQUESTED            0x1301U
#define EVENT_CDN_PURGE_COMPLETED            0x1302U
```

### Event frame layout

```c
typedef struct alrios_event_frame {
    uint32_t magic;
    uint16_t version;
    uint16_t event_id;
    uint64_t transaction_id;
    uint64_t timestamp_nanos;
    uint32_t producer_pid;
    uint32_t payload_len;
    uint8_t payload_sha256[32];
} alrios_event_frame_t;
```

## 16.6 `.arapp.index` Differential Manifest Format

The `.arapp.index` is a deterministic manifest generated by `armake`. It is consumed by `arpm` prior to fetching the full payload. Its primary objective is to identify content-addressed file equality and allow zero-copy reuse through hardlinks or reflinks.

```c
#ifndef ALRIOS_ARAPP_INDEX_H
#define ALRIOS_ARAPP_INDEX_H

#include <stdint.h>

#define ARAPP_INDEX_MAGIC              "ARAPPINDEXv1\0\0\0\0"
#define ARAPP_INDEX_MAGIC_LEN          16
#define ARAPP_INDEX_PATH_MAX           512
#define ARAPP_INDEX_SHA256_LEN         32

#define ARAPP_FILE_FLAG_EXECUTABLE     (1U << 0)
#define ARAPP_FILE_FLAG_READONLY       (1U << 1)
#define ARAPP_FILE_FLAG_COMPRESSED     (1U << 2)
#define ARAPP_FILE_FLAG_PATCHABLE      (1U << 3)
#define ARAPP_FILE_FLAG_NO_HARDLINK    (1U << 4)

#pragma pack(push, 1)

typedef struct arapp_index_header {
    uint8_t  magic[ARAPP_INDEX_MAGIC_LEN];
    uint16_t version;
    uint16_t reserved;
    uint32_t file_count;
    uint64_t total_uncompressed_size;
    uint8_t  manifest_sha256[ARAPP_INDEX_SHA256_LEN];
} arapp_index_header_t;

typedef struct arapp_index_entry {
    char     relative_path[ARAPP_INDEX_PATH_MAX];
    uint64_t file_size;
    uint64_t mtime_normalized;
    uint32_t unix_mode;
    uint32_t flags;
    uint8_t  sha256[ARAPP_INDEX_SHA256_LEN];
    uint8_t  patch_base_sha256[ARAPP_INDEX_SHA256_LEN];
} arapp_index_entry_t;

#pragma pack(pop)

#endif /* ALRIOS_ARAPP_INDEX_H */
```

## 16.7 `arpm` Delta Deployment Algorithm

Given old index `I_old` and new index `I_new`, the deployment engine computes three sets:

```text
UNCHANGED = { f ∈ I_new | exists g ∈ I_old: f.path == g.path && f.sha256 == g.sha256 }
PATCHABLE = { f ∈ I_new | f.patch_base_sha256 matches an old object }
DOWNLOAD = I_new - UNCHANGED - PATCHABLE
```

Operational procedure:

1. Download `new.arapp.index`.
2. Verify index signature and SHA-256 manifest digest.
3. Load current active slot index.
4. Create staging slot directory with restrictive permissions.
5. For every unchanged file, call `link(old_path, new_path)`.
6. If `link()` returns `EXDEV`, attempt reflink via `ioctl(FICLONE)`.
7. If reflink fails, perform buffered copy and verify SHA-256.
8. For patchable files, download `.arpatch`, apply bsdiff/zstd patch, then verify final SHA-256.
9. For download files, fetch content by hash or HTTP Range and verify SHA-256.
10. Apply modes, ownership metadata, and read-only flags.
11. Run `pre-swap` hooks.
12. Promote slot atomically.
13. Emit `EVENT_APP_DEPLOYED`.
14. Run `post-swap` hooks asynchronously.

## 16.8 Hardlink Layering Security Rules

1. Mutable files are forbidden in `.arapp`. Files ending `.db`, `.sqlite`, `.session`, `.lock`, `.wal`, `.shm`, `.log`, `.tmp` are rejected by `armake`.
2. A hardlinked release slot must be immutable. Any write attempt to a release path is a policy violation.
3. `arpm` must never hardlink across trust domains or different app IDs.
4. If source and destination are on different filesystems, `EXDEV` must be handled deterministically.
5. Hash verification after provisioning is mandatory even for hardlinked files.
6. The index is authoritative only if its signature chain validates against the active Root of Trust.
7. Binary patch application is allowed only if the base hash matches exactly.
8. A failed patch cannot leave partial files in the final slot; staging files must be written to temporary paths and atomically renamed after verification.

## 16.9 Expected Efficiency Targets

For a 5 GB app where 100 MB changed:

| Metric | Monolithic | Delta v0.3.x |
|---|---:|---:|
| Network transfer | 5,000 MB | ~100 MB |
| Disk write | 5,000 MB | ~100 MB |
| Hardlinked bytes | 0 MB | ~4,900 MB |
| 100 Mbps download time | ~7 min | ~8 s |
| Slot provision time | ~45 s | < 50 ms for unchanged files |
| Runtime downtime | 0 ms | 0 ms |

---
