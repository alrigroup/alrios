# Recommended Applications for ALRIOS

This document lists the public, open-source applications recommended for use with ALRIOS.

---

## ARCDN

**Purpose:** Native Static File Server & CDN  
**Language:** C  
**License:** ARGLP  
**Description:** High-performance static file server with TLS support, hot reloading, and minimal footprint. Ideal for serving assets, media, and web content.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios
git submodule update --init --recursive
armake build arcdn
```

**Repository:** https://github.com/alrigroup/arcdn

---

## ARDB

**Purpose:** Native Linear Database Engine  
**Language:** C  
**License:** ARGLP  
**Description:** High-performance embedded database with PostgreSQL-compatible wire protocol (PGWire), built-in authentication, SQL firewall, and audit logging.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios
git submodule update --init --recursive
armake build ardb
```

**Repository:** https://github.com/alrigroup/ardb

---

## ARWN

**Purpose:** Web Native Compiler & Runtime  
**Language:** C  
**License:** ARGLP  
**Description:** Compiler, bundler, and runtime for building web applications as native ALRIOS apps. Compiles `.arhtml` templates and JavaScript into optimized `.arweb` binary packages.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios
git submodule update --init --recursive
armake build arwn
```

**Repository:** https://github.com/alrigroup/arwn

---

## ARWS

**Purpose:** High-Performance Reverse Proxy, Load Balancer & Stream Proxy  
**Language:** C  
**License:** ARGLP  
**Description:** Native C reverse proxy and load balancer with round-robin distribution, stream proxying, rate limiting, response caching, and TLS termination.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios
git submodule update --init --recursive
armake build arws
```

**Repository:** https://github.com/alrigroup/arws

---

## License Notice

All public applications are licensed under **ARGLP** (ALRI Group License Permissive).  
See each app's README for full license terms.
