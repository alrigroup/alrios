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
| **Memory Zeroize & Constant-Time Memcmp** | `src/ALRIOS/crypto/utils.c` | **VERIFIED** | Branchless comparison, memory barriers on GCC, Clang, and MSVC. |
| **SHA-512 Streaming Digest** | `src/ALRIOS/crypto/sha512.c` | **IMPLEMENTED** | FIPS 180-4 streaming and block engine backed by OpenSSL EVP. |
| **Ed25519 Signature Verification** | `src/ALRIOS/crypto/ed25519.c` | **IMPLEMENTED** | RFC 8032 pure verification via OpenSSL 3.x EVP raw public key API. |
| **AES-256-GCM Streaming Decrypt** | `src/ALRIOS/crypto/aes_gcm.c` | **IMPLEMENTED** | NIST SP 800-38D with full tag verification and zeroization on auth failure. |
| **ML-DSA-65 (FIPS 204 Lattice)** | `src/ALRIOS/crypto/ml_dsa_65.c` | **EXPERIMENTAL** | Requires runtime provider support (`ALRIOS_HAVE_ML_DSA_65`). Fails closed if unavailable. |
| **Multi-Profile Governance** | `src/ALRIOS/profiles/profiles.c` | **IMPLEMENTED** | Evaluates capability bitmasks. Sovereign-Max fails closed when required crypto is missing. |
| **In-Memory Sealed Memfd Engine** | `src/ALRIOS/kernel/memloader.c` | **IMPLEMENTED** | Native `memfd_create` and `fcntl(F_ADD_SEALS)` on Linux. Unused by legacy path. |
| **In-Memory Vault Loader** | `src/ALRIOS/core/vault_loader.c` | **VERIFIED** | Zeroizes all temporary slots before free, strict POSIX `envp[]` formatting. |
| **Seccomp-BPF Assembly Filter** | `src/ALRIOS/sandbox/seccomp_filter.c` | **EXPERIMENTAL** | Strict BPF assembly filter blocking high-risk syscalls (`ptrace`, `reboot`, `mount`). |
| **Cgroups v2 Resource Governance** | `src/ALRIOS/sandbox/cgroups_init.c` | **EXPERIMENTAL** | Applies `memory.max` and `pids.max` constraints. |
| **Namespace Virtualization** | `src/ALRIOS/sandbox/ns_unshare.c` | **EXPERIMENTAL** | Encapsulates guest execution inside `CLONE_NEWPID/NET/NS`. |
| **Live FD Passing (SCM_RIGHTS)** | `src/ALRIOS/deploy/fd_pass.c` | **IMPLEMENTED** | Transfers active socket descriptors across UNIX domain streams for zero-downtime reloads. |
| **Atomic Symlink Deployment** | `src/ALRIOS/deploy/atomic_swap.c` | **IMPLEMENTED** | Atomically swaps release slots using `rename()` at the inode level. |
| **Delta Packaging Index** | `src/ALRIOS/deploy/arapp_index.c` | **EXPERIMENTAL** | Implements hardlink layering and content-addressed comparisons. |
| **AR-SDK Arena Allocator** | `src/ALRIOS/sdk/ar_sdk.c` | **VERIFIED** | Request-scoped memory arena with alignment guarantees and clean destruction. |
| **Reactive Hooks Execution** | `src/ALRIOS/hooks/hooks_executor.c` | **EXPERIMENTAL** | Spawns lifecycle hook scripts with timeouts and environment injection. |
| **Dual-Ring Host/Guest IPC** | `src/ALRIOS/ipc/ipc_channel.c` | **IMPLEMENTED** | 12-byte binary framed communications over `AF_UNIX` `SOCK_SEQPACKET`. |
