# System Requirements and Dependencies — ALRIOS

System environment requirements and developer build tools.

---

## 📦 1. Runtime Packages (Linux)

To run ALRIOS on a Linux server without compiling:

```bash
sudo apt update && sudo apt install -y libssl3 libzstd1 zlib1g curl
```

- **`libssl3`:** SSL/TLS encryption support for ARWS Gateway.
- **`libzstd1` / `zlib1g`:** Asset and package decompression.
- **`curl`:** Health checks and diagnostic HTTP calls.

---

## 🛠️ 2. Build Packages (For Developers)

To compile the ecosystem from source:

```bash
sudo apt install -y cmake gcc make libssl-dev pkg-config nodejs npm
```

- **`cmake` / `make` / `gcc`:** C language compilation toolchain (C11).
- **`libssl-dev`:** OpenSSL header files for compilation.
- **`nodejs` / `npm`:** Required to build React SPA bundles (`home.web`, `detroit.web`).

---

## 💻 3. Windows Requirements

- **Compiler:** Visual Studio 2022 / MSVC (`cl.exe`) or MinGW.
- **Tools:** CMake (`3.20+`), Node.js (`18+`), Python (`3.10+`).
