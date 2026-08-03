# ARKernel — Operating System Hardware Abstraction Layer (HAL)

The **ARKernel** module (`src/ALRIOS/arkernel/`) isolates low-level platform code (Windows Win32 vs Linux POSIX) behind unified, cross-platform APIs defined in `aros_hal.h`.

---

## 🏛️ Architecture

```
                       aros_hal.h (Unified Interface)
                                   │
              ┌────────────────────┴────────────────────┐
              ▼                                         ▼
      os/windows/ (Win32 API)                  os/linux/ (POSIX API)
   ├── process.c (CreateProcessW)           ├── process.c (fork/execvp)
   ├── socket.c (Winsock2)                  ├── socket.c (sys/socket.h)
   ├── threads.c (CreateThread)             ├── threads.c (pthread)
   └── sync.c (CRITICAL_SECTION/CV)        └── sync.c (pthread_mutex/cond)
```

---

## 🛠️ Module Specifications

### 1. Process Management (`ar_process.h`)
- `ar_process_spawn`: Spawns background process detached from parent console.
- `ar_process_kill`: Terminate process by PID.
- `ar_process_is_running`: Query process liveness.

### 2. Networking & Socket API (`ar_socket.h`)
- `ar_socket_create`: Initialize non-blocking TCP socket.
- `ar_socket_bind`: Bind socket to local IP and port.
- `ar_socket_listen`: Set listen backlog (`4096`).
- `ar_socket_accept`: Accept incoming connection.
- `ar_socket_set_nodelay`: Enable `TCP_NODELAY` (disables Nagle's algorithm for sub-millisecond response latency).

### 3. Threading & Synchronization (`ar_sync.h`)
- `ar_thread_create` / `ar_thread_join`: Thread creation and joining.
- `ar_mutex_create` / `ar_mutex_lock` / `ar_mutex_unlock`: High-performance mutex locks.
- `ar_condvar_create` / `ar_condvar_wait` / `ar_condvar_signal`: Condition variables replacing busy-wait sleep loops.
