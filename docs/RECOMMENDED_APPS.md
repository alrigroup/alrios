# Recommended Applications for ALRIOS

This document lists the recommended applications to use with ALRIOS, their purpose, and installation instructions.

---

## ARAPIAUTH

**Purpose:** OAuth2 / OIDC Authentication Gateway  
**Language:** C  
**Role:** Public-facing authentication API handling token exchange and SSO flows. Delegates secure credential storage to the internal ARAUTH vault.

**Installation:**
```bash
# Clone the alrios repository
git clone https://github.com/alrigroup/alrios.git
cd alrios

# Initialize submodules (includes arapiauth)
git submodule update --init --recursive

# Build
armake build arapiauth
```

**Repository:** https://github.com/alrigroup/arapiauth

---

## ARCDN

**Purpose:** Native Static File Server & CDN  
**Language:** C  
**Role:** High-performance static file server with TLS support, hot reloading, and minimal footprint.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios

# Initialize submodules
git submodule update --init --recursive

# Build
armake build arcdn
```

**Repository:** https://github.com/alrigroup/arcdn

---

## ARDB

**Purpose:** Native Linear Database Engine  
**Language:** C  
**Role:** High-performance embedded database with PostgreSQL-compatible wire protocol (PGZero), built-in authentication, firewall, and audit logging.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios

# Initialize submodules
git submodule update --init --recursive

# Build
armake build ardb
```

**Repository:** https://github.com/alrigroup/ardb

---

## ARWN

**Purpose:** Web Native Compiler & Runtime  
**Language:** C  
**Role:** Compiler, bundler, and runtime for building web applications as native ALRIOS apps. Compiles `.arhtml` templates and JavaScript into optimized web packages.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios

# Initialize submodules
git submodule update --init --recursive

# Build
armake build arwn
```

**Repository:** https://github.com/alrigroup/arwn

---

## ARWS

**Purpose:** High-Performance Reverse Proxy, Load Balancer & Stream Proxy  
**Language:** C  
**Role:** Native C reverse proxy and load balancer with round-robin distribution, stream proxying, rate limiting, response caching, and TLS termination.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios

# Initialize submodules
git submodule update --init --recursive

# Build
armake build arws
```

**Repository:** https://github.com/alrigroup/arws

---

## ALRIGROUP.WEB

**Purpose:** ALRIOS Web Applications  
**Language:** HTML/CSS/JS  
**Role:** Collection of web applications providing browser-based management and monitoring interfaces for ALRIOS.

**Installation:**
```bash
# Standalone clone (not a submodule)
git clone https://github.com/alrigroup/alrigroup.web.git

# Or via alrios
git clone https://github.com/alrigroup/alrios.git
cd alrios
git submodule update --init --recursive
```

**Repository:** https://github.com/alrigroup/alrigroup.web

---

## ARAUTH

**Purpose:** Authentication & Authorization Service  
**Language:** C  
**Role:** Secure user authentication, token management, and access control for ALRIOS applications. Supports OAuth2/OIDC, TOTP, vault integration, and rate limiting.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios

# Initialize submodules
git submodule update --init --recursive

# Build
armake build arauth
```

**Repository:** https://github.com/alrigroup/arauth

---

## AR ENTERPRISE

**Purpose:** Enterprise Platform for ALRIOS  
**Language:** C  
**Role:** Comprehensive enterprise platform providing database management, authentication, and HTTP services for enterprise-grade applications.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios

# Initialize submodules
git submodule update --init --recursive

# Build
armake build arenterprise
```

**Repository:** https://github.com/alrigroup/arenterprise

---

## AR ENTERPRISE.WEB

**Purpose:** Web Interface for AR ENTERPRISE  
**Language:** HTML/CSS/JS  
**Role:** Browser-based management console for the AR ENTERPRISE platform, providing database and service configuration via web UI.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios

# Initialize submodules
git submodule update --init --recursive
```

**Repository:** https://github.com/alrigroup/arenterprise.web

---

## DETROIT.WEB

**Purpose:** Detroit Web Application  
**Language:** HTML/CSS/JS  
**Role:** Web application for industrial and enterprise use cases, providing web-based industrial control interface and system configuration.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios

# Initialize submodules
git submodule update --init --recursive
```

**Repository:** https://github.com/alrigroup/detroit.web

---

## FOURTECH.WEB

**Purpose:** FourTech Web Application  
**Language:** HTML/CSS/JS  
**Role:** Web application providing technical tools and interfaces for fourtech-related use cases, with data dashboard and system configuration.

**Installation:**
```bash
git clone https://github.com/alrigroup/alrios.git
cd alrios

# Initialize submodules
git submodule update --init --recursive
```

**Repository:** https://github.com/alrigroup/fourtech.web

---

## License Notice

- **Public apps** (ARAPIAUTH, ARCDN, ARDB, ARWN, ARWS): Licensed under **ARGLP** (ALRI Group License Permissive)
- **Private apps** (ALRIGROUP.WEB, ARAUTH, AR ENTERPRISE, AR ENTERPRISE.WEB, DETROIT.WEB, FOURTECH.WEB): Licensed under **ARGLR** (ALRI Group License Reserved)

*Refer to each app's README for full license terms.*