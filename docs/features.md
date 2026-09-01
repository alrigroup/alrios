# ALRIOS - Complete Features Catalog & Technical Specification

**Official Technical Specification and Feature Catalog for ALRIOS (ALRI Operating System).**  
*Developed by ALRIGROUP - High-Performance Systems Engineering.*

---

## Executive Module Summary

1. [ARKernel - Hardware Abstraction Layer (HAL)](#1-arkernel--hardware-abstraction-layer-hal)
2. [arcore - Master Service Daemon & Supervisor](#2-arcore--master-service-daemon--supervisor)
3. [ARWS - High-Performance Reverse Proxy, Gateway & WAF](#3-arws--high-performance-reverse-proxy-gateway--waf)
4. [ARDB - Sovereign Data Guardian & SQL Engine Proxy](#4-ardb--sovereign-data-guardian--sql-engine-proxy)
5. [ARWN - ALRI Web Native Framework & `.arweb` Containers](#5-arwn--alri-web-native-framework--arweb-containers)
6. [ARCDN - High-Throughput Edge & Media Server](#6-arcdn--high-throughput-edge--media-server)
7. [Developer Toolchain & CLI](#7-developer-toolchain--cli)
8. [Security Matrix & Cryptographic Layer](#8-security-matrix--cryptographic-layer)

---

## 1. ARKernel - Hardware Abstraction Layer (HAL)

The **ARKernel** (`src/ALRIOS/arkernel/`) provides a unified system call layer (Win32 and POSIX) with zero-overhead abstractions (`aros_hal.h`).

### 1.1 Process Management (`ar_process.h`)
- **Async Isolated Spawn**: Child process creation decoupled from parent console (`CreateProcessW` on Windows / `fork` + `execvp` on Linux).
- **Lifecycle Control**: Graceful shutdown signals (`SIGTERM`) and forced termination (`SIGKILL` / `TerminateProcess`).
- **Liveness Detection**: Atomic integrity checks and real-time PID verification without thread locking.

### 1.2 Networking & Sockets (`ar_socket.h`)
- **Native Non-Blocking Mode**: Atomic `O_NONBLOCK` socket configuration for massive I/O scalability.
- **Connection Backlog at Scale**: Configurable default backlog up to `4096` simultaneous pending connections per port.
- **Latency Optimization (`TCP_NODELAY`)**: Mandatory Nagle algorithm disable for sub-millisecond inter-node communication.
- **Address Reuse (`SO_REUSEADDR` & `SO_REUSEPORT`)**: Instant service restart without `TIME_WAIT` state blocking.
- **Granular I/O Timeouts**: Configurable read/write timeouts to mitigate Slowloris attacks.

### 1.3 Multithreading & Synchronization (`ar_sync.h`)
- **Standard Thread Spawning**: Transparent native thread creation (`pthread_create` / `CreateThread`).
- **Fast Mutexes**: Mutual exclusion locks (`ar_mutex`) with recursion support and ultra-low contention.
- **Condition Variables (`ar_cond`)**: Event-driven notification (`ar_cond_wait`, `ar_cond_signal`, `ar_cond_broadcast`), eliminating busy-wait loops.
- **Atomic Operations**: Support for atomic counters and flags for active connection counting without global locks.

### 1.4 Memory Management & HAL Utilities
- **Linear Memory Buffers**: High-speed allocators (`ar_mem_alloc`, `ar_mem_free`) with memory alignment for SIMD instructions.
- **High-Precision Timer (`ar_time_ms`)**: Monotonic millisecond reading independent of OS clock changes.

---

## 2. arcore - Master Service Daemon & Supervisor

The **`arcore`** (`src/ALRIOS/core/`) is the system orchestrator.

### 2.1 Self-Healing Supervision
- **Service State Machine**: Strict control of states `UNREGISTERED` -> `STOPPED` -> `STARTING` -> `RUNNING` -> `CRASHED` -> `RECOVERING`.
- **Smart Auto-Restart**: Instant process crash detection with exponential backoff restart.
- **Two-Phase Isolation**:
  - **Phase 1**: Infrastructure service initialization (`arws`, `ardb`, `arcdn`).
  - **Phase 2**: Application initialization.

### 2.2 IPC Control Plane (Port 9500)
- **5-Byte Framing Protocol**: `[1B Type][4B Payload Length (Big-Endian)][Payload]`.
- **Native Commands Supported**:
  - `IPC_PING` (1): Daemon health heartbeat.
  - `IPC_LIST` (2): Detailed process table, PIDs, ports, and resource usage.
  - `IPC_START` / `IPC_STOP` (3/4): Individual service control.
  - `IPC_RELOAD` (5): Hot-reload configurations at runtime without downtime.
  - `IPC_REGISTER_ROUTE` (6): Dynamic injection of reverse proxy routes into the gateway.
  - `IPC_CACHE_CLEAR` (12): Instant flush of all in-memory cache shards.

### 2.3 `.arapp` Package Management & Staging
- **On-Demand Extraction**: Transparent `.arapp` archive extraction to `.staging/` directory.
- **Signature & Integrity Validation**: `ALRIGROUP@APPMAKE` manifest verification and binary integrity checks before execution.

---

## 3. ARWS - High-Performance Reverse Proxy, Gateway & WAF

**ARWS** (`src/apps/arws/`) is the entry point for all HTTP/HTTPS traffic.

### 3.1 Event Architecture & Concurrency
- **Connection Thread Pool**: Pool with `POOL_SIZE` persistent threads fed by non-blocking queues.
- **Simultaneous Connection Control (`MAX_CONNECTIONS`)**: Active connection ceiling with immediate `503 Service Unavailable` response at saturation peaks.
- **Polling Multiplexation**: `epoll` / `WSAPoll` support for monitoring thousands of connections in a single cycle.

### 3.2 Advanced Routing & Host Matching
- **Dynamic Virtual Hosting**: Multi-domain routing (`alrigroup.com`, `localhost`, `127.0.0.1`).
- **Exact Route vs Wildcard Priority**: Deep routes take precedence over global wildcards.
- **Upstream Load Balancing**: Backend server pools with weight support, backup nodes, and graceful drain mode.

### 3.3 In-Memory Cache with 16 Shards
- **Zero Disk Latency**: Complete HTTP response storage in RAM with hash key indexing.
- **Granular TTL**: Global or per-endpoint cache lifetime configuration.

### 3.4 Rate Limiting & Abuse Protection
- **Multi-Layer Protection**: Independent rate limits per client IP and per route/host.
- **Trusted Reverse Proxy Detection**: Real IP validation from explicitly authorized proxies (`ARWS_TRUSTED_PROXY`).
- **Temporary Auto-Blacklisting**: Full IP blocking for configurable periods.

### 3.5 WAF (Web Application Firewall) & Defensive Headers
- **Anti-Path Traversal**: Blocking of `..` sequences, encoded characters, and null byte injection.
- **Content Security Policy (CSP)**: Automatic injection of script, style, and media restriction policies.
- **Clickjacking & Sniffing Mitigation**: Default `X-Frame-Options: DENY` and `X-Content-Type-Options: nosniff` injection.
- **Forced HSTS**: `Strict-Transport-Security: max-age=31536000; includeSubDomains`.

---

## 4. ARDB - Sovereign Data Guardian & SQL Engine Proxy

**ARDB** (`src/apps/ardb/`) is the sovereign data proxy that inspects, protects, and isolates database queries.

### 4.1 App Table Isolation & Shared Groups
- **Per-App Credentials & Tokens**: Each application connects with its own credentials and token.
- **Private Table Isolation**: Applications can only access their own tables.
- **Shared Spaces (App Groups)**: Creation of app groups enabling joint access to shared data spaces between authorized apps.
- **SQL Firewall Table ACL**: Real-time interception of `SELECT`, `INSERT`, `UPDATE`, `DELETE`, and `JOIN`. Unauthorized access attempts are blocked with error code `42501`.

### 4.2 Native PG-Wire Protocol (PostgreSQL Wire Protocol v3.0)
- **Complete Handshake Implementation**: `StartupMessage` processing, authentication, and backend parameter negotiation.
- **Streaming Query Execution**: Message passthrough for `Query`, `RowDescription`, `DataRow`, and `CommandComplete`.

### 4.3 SQL Firewall & SQL Injection Prevention
- **Strict Tenant Identifier Validation**: Alphanumeric whitelist preventing quote escape in multi-tenant injections.
- **Automatic RLS Injection**: Forced `SET LOCAL alri.tenant_id = '...'` at the start of each transaction.
- **DDL/DML Destruction Blocking**: Prevention of dangerous commands (`DROP TABLE`, `TRUNCATE`, `ALTER SYSTEM`, etc.) for non-admin profiles.

### 4.4 Immutable Audit with Hash-Chain (Blockchain-Style)
- **SHA-256 Chained Signing**: Each audited query generates a cryptographic hash including the previous event hash.
- **Forensic Integrity Verification**: End-to-end chain validation proving no records were altered or retroactively deleted.

---

## 5. ARWN - ALRI Web Native Framework & `.arweb` Containers

**ARWN** (`src/apps/arwn/`) is the packaging and delivery architecture for web applications with zero runtime disk access.

### 5.1 Binary `.arweb` Container
- **Zero Disk Lookups**: All resources (HTML, CSS, JS, WASM) are mounted into a single binary in-memory RAM structure.
- **Section Table with CRC32**: 48-byte per-section integrity validation against data corruption.
- **Memory-Range Delivery**: The ARWN server serves payloads directly from shared memory pointers.

### 5.2 Native WebAssembly (WASM) Engines
- **Micro-Engines in C / C++ / Rust / Go**: Support for WASM-compiled code execution with isolated linear memory.
- **Bidirectional JS <-> WASM Calls**: Modules automatically loaded through the `arwn-bridge.js` bridge.

### 5.3 Intellectual Property Obfuscation
- **Base64 VM Container**: JavaScript encapsulation within protected self-executing interpreters when `obfuscate=yes` is configured.
- **Sovereign Licensing Injection**: Mandatory ALRI Group legal headers on all compilation outputs.

---

## 6. ARCDN - High-Throughput Edge & Media Server

**ARCDN** (`src/apps/arcdn/`) is the static file, media, and streaming delivery subsystem.

### 6.1 Zero-Copy I/O (`sendfile`)
- **Direct Kernel-to-Socket Transfer**: Files sent directly from kernel page cache to network card without user-space copy.
- **Media Streaming with Range Request Support (`HTTP 206 Partial Content`)**: Smooth MP4/WebM playback with instant seeking.

### 6.2 File Descriptor Cache & MIME Type Mapping
- **Optimized MIME Type Table**: Instant type resolution for 100+ common extensions.
- **HTTP Cache Control**: Automatic `ETag` and `Cache-Control` header emission for CDN efficiency.

---

## 7. Developer Toolchain & CLI

Native tools (`src/tools/`) managing the entire development lifecycle, compilation, packaging, and system operation.

### 7.1 `armake` - Application Packager
- **`.arapp` Package Building**: Structured packaging of binaries, manifests, and assets into self-contained archives.
- **Extraction & Inspection**: `armake build`, `armake extract`, and `armake info` commands.

### 7.2 `arinstall` - Runtime Manager
- **Isolated Environment Installation**: Download and preparation of language runtimes (Node.js, Python, OpenJDK, Go, Lua) within `arcore/programfiles/` without polluting the host OS.

### 7.3 `alrios` - Unified Governance CLI
- **Power Control**: `alrios power on | off | reload`.
- **Real-Time Monitoring**: `alrios status` (displays services, PIDs, memory state, and gateway health).
- **Route & Upstream Management**: `alrios arws upstream list | add | drain | remove`.

---

## 8. Security Matrix & Cryptographic Layer

| Layer | Mechanism | Algorithm / Standard | Purpose |
| :--- | :--- | :--- | :--- |
| **Passwords** | Key Derivation | **PBKDF2-HMAC-SHA512 (210,000 iterations)** | GPU/FPGA attack protection |
| **Sessions** | Session Identifiers | **CSPRNG 256-bit Hex + Single-Use Refresh** | Anti-Hijacking and Replay mitigation |
| **MFA** | Two-Factor Auth | **RFC 6238 TOTP (HMAC-SHA1)** | Second authentication factor |
| **E2EE / Payload** | Symmetric Encryption | **AES-256-GCM with 128-bit Auth Tag** | End-to-end confidentiality and integrity |
| **Database** | Multi-Tenant Isolation | **SET LOCAL alri.tenant_id + Regex Whitelist** | Absolute cross-tenant leak prevention |
| **Audit** | Immutable Forensic Log | **Blockchain-Style SHA-256 Hash Chain** | Non-repudiation and tamper detection |
| **Network & Gateway** | Traffic Protection | **TLS 1.3 / CSP / HSTS / Anti-Path Traversal** | Interception, XSS, and CSRF protection |
