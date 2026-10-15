# ALRIOS Implementation Status Matrix

This document records the exact readiness of every major system module. It establishes an absolute boundary between deployed implementations, hardware-dependent components, and experimental or prototype contracts.

**Governance Invariant:** No component may be presented as production-ready or certified without an independent, compilable, non-mocked verification suite.

---

## 1. System Readiness State Machine

- **PLANNED**: Architecturally defined, schemas and headers drafted, no runtime backend.
- **PROTOTYPE**: Initial user-space implementation, relies on stubs, simulation, or temporary mocks.
- **EXPERIMENTAL**: Real algorithms and system calls active, lacks full formal proofs, fuzz coverage, or external certification.
- **IMPLEMENTED**: Fully functional in standard runtime environments, adheres to strict C11, covered by integration tests.
- **VERIFIED**: Passes comprehensive ASan/UBSan, Valgrind zero-leak audits, and mutation fuzzing suites.
- **CERTIFIED**: Formally evaluated under recognized external regulatory regimes (e.g., FIPS 140-3, Common Criteria).

---

## 2. Core Platform & Kernel Modules

| Subsystem | Implemented File(s) | Status | Notes / Limitations |
|---|---|:---:|---|
| **Memory Zeroize & Constant-Time Memcmp** | `src/libraries/crypto/utils.c` | **VERIFIED** | Branchless comparison, memory barriers on GCC, Clang, and MSVC. |
| **SHA-512 Streaming Digest** | `src/libraries/crypto/sha512.c` | **VERIFIED** | FIPS 180-4 streaming and block engine backed by OpenSSL EVP. |
| **Ed25519 Signature Verification** | `src/libraries/crypto/ed25519.c` | **VERIFIED** | RFC 8032 pure verification via OpenSSL 3.x EVP raw public key API. |
| **AES-256-GCM Streaming Decrypt** | `src/libraries/crypto/aes_gcm.c` | **VERIFIED** | NIST SP 800-38D with full tag verification and zeroization on auth failure. |
| **ML-DSA-65 (FIPS 204 Lattice)** | `src/libraries/crypto/ml_dsa_65.c` | **VERIFIED** | Active runtime provider support (`ALRIOS_HAVE_ML_DSA_65`). Fails closed if unavailable. |
| **ML-KEM-768 (FIPS 203 Lattice)** | `src/libraries/crypto/ml_kem_768.c` | **VERIFIED** | Active runtime provider support (`ALRIOS_HAVE_ML_KEM_768`). Quantum-safe key encapsulation. |
| **Multi-Profile Governance** | `src/libraries/policy/profiles.c` | **VERIFIED** | Evaluates capability bitmasks. Sovereign-Max fails closed when required crypto is missing. |
| **In-Memory Sealed Memfd Engine** | `src/libraries/loader/memloader.c` | **IMPLEMENTED** | Native `memfd_create` and `fcntl(F_ADD_SEALS)` on Linux. |
| **In-Memory Vault Loader** | `src/platform/core/vault_loader.c` | **VERIFIED** | Zeroizes all temporary slots before free, strict POSIX/Win32 `envp[]` formatting. |
| **Seccomp-BPF Assembly Filter** | `src/libraries/sandbox/seccomp_filter.c` | **IMPLEMENTED** | Strict BPF assembly filter blocking high-risk syscalls (`ptrace`, `reboot`, `mount`). |
| **Cgroups v2 Resource Governance** | `src/libraries/sandbox/cgroups_init.c` | **IMPLEMENTED** | Applies `memory.max` and `pids.max` constraints on Linux. |
| **Windows Job Object Sandbox** | `src/libraries/sandbox/devmode_jail_windows.c` | **IMPLEMENTED** | Confinamento estrito Windows via Job Objects (`KILL_ON_JOB_CLOSE`, memória e limite de processos). |
| **Namespace & Path Virtualization** | `src/libraries/sandbox/ns_unshare.c` / `mounts_windows.c` | **IMPLEMENTED** | Linux namespaces e Windows canonical path containment anti-traversal. |
| **Live FD Passing (SCM_RIGHTS)** | `src/libraries/deploy/fd_pass.c` | **IMPLEMENTED** | Transfers active socket descriptors across UNIX domain streams for zero-downtime reloads. |
| **Atomic Symlink Deployment** | `src/libraries/deploy/atomic_swap.c` | **IMPLEMENTED** | Atomically swaps release slots using `rename()` at the inode level. |
| **Delta Packaging Index** | `src/libraries/deploy/arapp_index.c` | **IMPLEMENTED** | Implements hardlink layering and content-addressed comparisons. |
| **AR-SDK Arena Allocator** | `src/libraries/sdk/ar_sdk.c` | **VERIFIED** | Request-scoped memory arena with alignment guarantees and clean destruction. |
| **Reactive Hooks Execution** | `src/libraries/hooks/hooks_executor.c` | **IMPLEMENTED** | Spawns lifecycle hook scripts with timeouts and environment injection. |
| **Dual-Ring Host/Guest IPC** | `src/libraries/ipc/ipc_channel.c` | **IMPLEMENTED** | 12-byte binary framed communications over `AF_UNIX` `SOCK_SEQPACKET`. |
