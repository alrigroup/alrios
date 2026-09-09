<p align="center">
  <img src="https://raw.githubusercontent.com/alrigroup/.github/main/alrigroup.svg" width="140" alt="ALRIGROUP" />
</p>

<h1 align="center">ALRIOS</h1>
<p align="center"><strong>Sovereign Operating System, Native Microkernel & High-Performance Microservice Platform</strong></p>

<p align="center">
  <a href="https://github.com/alrigroup/alrios/actions/workflows/build-installers.yml"><img src="https://img.shields.io/github/actions/workflow/status/alrigroup/alrios/build-installers.yml?branch=main&label=CI%2FCD%20Build%20%26%20Installers&style=flat-square" alt="CI/CD Status"></a>
  <img alt="Language" src="https://img.shields.io/badge/language-C%20%2F%20C%2B%2B-00599C?style=flat-square" />
  <img alt="Platform" src="https://img.shields.io/badge/platform-Linux%20x64%20%7C%20Windows%20x64-blueviolet?style=flat-square" />
  <img alt="License" src="https://img.shields.io/badge/license-ARGLP-green?style=flat-square" />
</p>

---

## ⚡ Overview

**ALRIOS** is an ultra-high-performance sovereign operating system kernel and native microservice platform engineered to handle **65,536 concurrent connections**, featuring **Upstream TCP Connection Pooling**, **Sharded In-Memory Cache**, and native modular containerization (`.arapp`).

This repository contains:
- **`arcore`**: The Sovereign OS Runtime Kernel & Microservices Supervisor.
- **`alrios`**: The Unified Management CLI and daemon orchestrator.
- **`arpm`**: The ALRIOS Sovereign Package Manager (`install`, `search`, `update`).
- **`armake`**: The Sovereign Application Bundler and compiler (`.arapp`).
- **Unified Master Installer**: Single-executable standalone installers for Linux & Windows with dual CLI and GUI support.

```
┌─────────────────┐       ┌──────────────────────────────────────────────────────┐
│     Browser     │──────▶│ ARWS Gateway (Reverse Proxy & Load Balancer)         │
│  (HTTP / HTTPS) │       │ ─ 65,536 Concurrent Connections, Upstream TCP Pool   │
│                 │       │ ─ 16 Shards Cache, Dynamic Rate Limiting & SSL       │
└─────────────────┘       └───────────┬──────────────┬──────────────┬────────────┘
                                      │              │              │
                   ┌──────────────────▼──┐    ┌──────▼───────┐ ┌────▼────────────┐
                   │ ARDB Database       │    │ ARCDN        │ │ ARWN Web Native │
                   │ PGWire + HTTP Engine│    │ Static CDN   │ │ High-Perf Engine│
                   └─────────────────────┘    └──────────────┘ └─────────────────┘
```

---

## 🚀 Instant Installation (1-Click Standalone)

ALRIOS provides **single-executable standalone installers** for both Linux and Windows.  
**Zero dependencies required**: If your machine lacks compilers or runtimes (like GCC, Node.js, Python), the installer detects and auto-downloads everything on-the-fly!

### 🪟 Windows (x64)

Baixe o executável oficial direto da aba [Releases](https://github.com/alrigroup/alrios/releases):
1. Execute **`alrios-setup.exe`** com 2 cliques para abrir o assistente gráfico nativo (Win32), ou execute pelo terminal:
   ```cmd
   alrios-setup.exe --cli -y
   ```
2. **Local de Instalação**:
   - Administrador: `C:\Program Files\ALRIOS\`
   - Usuário padrão: `%LOCALAPPDATA%\Programs\ALRIOS\`
3. **Pronto!** O instalador registra o ALRIOS no `PATH` do Windows automaticamente. Abra qualquer CMD ou PowerShell e digite `alrios` ou `arpm`.

### 🐧 Linux (x64)

Baixe o executável oficial direto da aba [Releases](https://github.com/alrigroup/alrios/releases):
```bash
# 1. Dar permissão de execução
chmod +x alrios-installer-linux-x64

# 2. Executar instalador (Abre interface gráfica GTK3 ou CLI interativa)
./alrios-installer-linux-x64

# Ou executar de forma rápida e silenciosa:
./alrios-installer-linux-x64 --cli -y
```
- **Local de Instalação**: `/opt/alrios/` (com sudo) ou `~/.local/share/alrios/` (usuário comum).
- **Comandos Globais no PATH**: Cria os links simbólicos automaticamente em `/usr/local/bin` (`alrios`, `arpm`, `arcore`, `armake`).

---

## 💻 Comandos Principais

Após a instalação, os seguintes comandos globais estão prontos para uso no seu terminal:

```bash
# Inicializar o ecossistema e microservicos
alrios power on

# Verificar status de todos os serviços em execução
alrios status

# Gerenciar pacotes e aplicativos do ecossistema
arpm list                    # Lista aplicativos instalados
arpm search <termo>          # Pesquisa no catálogo oficial (arcore/registry.json)
arpm install <app>           # Baixa e instala um pacote .arapp
arpm update --all            # Atualiza todos os aplicativos

# Empacotar novo aplicativo em .arapp soberano
armake build src/apps/meuapp arcore/apps/meuapp.arapp

# Desligar todos os serviços
alrios power off
```

---

## 📦 Ecossistema Modular de Aplicativos

Todos os pacotes oficiais são catalogados em [`arcore/registry.json`](arcore/registry.json):

| Repositório / Pacote | Descrição |
|---|---|
| **[arws](https://github.com/alrigroup/arws)** | Gateway reverso de alta performance, proxy e balanceador de carga HTTP/HTTPS |
| **[ardb](https://github.com/alrigroup/ardb)** | Banco de dados nativo linear de alta velocidade com suporte a PGWire |
| **[arcdn](https://github.com/alrigroup/arcdn)** | Servidor de assets estáticos e engine de streaming de baixa latência |
| **[arwn](https://github.com/alrigroup/arwn)** | Engine nativa de containers web `.arweb` e runtime Web Native |
| **[arauth](https://github.com/alrigroup/arauth)** | Serviço unificado de autenticação soberana e cofres de tokens |
| **[ardcbot](https://github.com/alrigroup/ardcbot)** | Integração oficial com Discord Bot Service |

---

## 🛠️ Compilação a partir do Código-Fonte (Desenvolvedores)

Se você deseja clonar o repositório e compilar localmente:

### Linux
```bash
# Dependências do sistema
sudo apt update && sudo apt install -y cmake gcc make libssl-dev libgtk-3-dev pkg-config nodejs npm

# Compilação
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --target arcore alrios armake arinstall -j$(nproc)

# Executar instalador local
./install.sh
```

### Windows
```cmd
cmake -S . -B build-win -DCMAKE_BUILD_TYPE=Release
cmake --build build-win --config Release --target arcore alrios armake arinstall
install.bat
```

---

## 📚 Documentação Técnica

Documentação aprofundada na pasta **[`/docs`](docs/README.md)**:

- 🏆 **[Catálogo Completo de Recursos](docs/features.md)**
- 📘 **[Visão Geral do Sistema](docs/README.md)**
- 📙 **[Guia do Desenvolvedor & IPC](docs/DEVELOPER_GUIDE.md)**
- 📕 **[Guia de Produção](docs/PRODUCTION.md)**
- 🟢 **[Requisitos de Sistema](docs/REQUIREMENTS.md)**
- 📓 **[Referência de Comandos CLI](docs/comands.md)**

---

## 🏢 Governança & Licença

- **Engenharia & Arquitetura**: **[ALRI Development](https://alrigroup.com/)** *(Divisão de Sistemas e Engenharia)*
- **Holding Controladora**: **[ALRI Group](https://alrigroup.com/)** *(Proprietária dos Ativos e Licenças)*
- **Licenciamento**: Distribuído sob os termos da licença **ARGLP (ALRI GROUP LICENSE PERMISSIVE - Versão 2)**. Consulte o arquivo [LICENSE](LICENSE) para termos completos.

<p align="center">© 2026 ALRI Group e suas afiliadas. Desenvolvido e mantido pela ALRI Development.</p>
