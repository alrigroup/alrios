# 🏛️ ALRIOS — Enciclopédia & Catálogo Completo de Features

**Documento Oficial de Especificação Técnica e Recursos do ALRIOS (ALRI Operating System)**  
*Holding Company ALRIGROUP — Engenharia de Sistemas de Alta Performance e Segurança Soberana.*

---

## 📑 Sumário Executivo dos Módulos

1. [ARKernel — Hardware Abstraction Layer (HAL)](#1-arkernel--hardware-abstraction-layer-hal)
2. [arcore — Master Service Daemon & Supervisor](#2-arcore--master-service-daemon--supervisor)
3. [ARWS — High-Performance Reverse Proxy, Gateway & WAF](#3-arws--high-performance-reverse-proxy-gateway--waf)
4. [ARAUTH — Sovereign Identity & Authentication Vault](#4-arauth--sovereign-identity--authentication-vault)
5. [ARDB — Sovereign Data Guardian & SQL Engine Proxy](#5-ardb--sovereign-data-guardian--sql-engine-proxy)
6. [ARWN — ALRI Web Native Framework & `.arweb` Containers](#6-arwn--alri-web-native-framework--arweb-containers)
7. [ARCDN — High-Throughput Edge & Media Server](#7-arcdn--high-throughput-edge--media-server)
8. [Developer Toolchain & CLI (`alrios`, `armake`, `arinstall`)](#8-developer-toolchain--cli-alrios-armake-arinstall)
9. [Segurança em Camadas & Matriz Criptográfica](#9-seguranca-em-camadas--matriz-criptografica)

---

## 1. ARKernel — Hardware Abstraction Layer (HAL)

O **ARKernel** é a base de nível mais baixo do sistema (`src/ALRIOS/arkernel/`), fornecendo uma camada unificada de chamadas de sistema (Win32 e POSIX) com abstrações de zero-overhead (`aros_hal.h`).

### 1.1 Gerenciamento de Processos (`ar_process.h`)
- **Spawn Assíncrono e Isolado**: Criação de processos filhos desacoplados de console pai (`CreateProcessW` no Windows / `fork` + `execvp` no Linux).
- **Controle de Ciclo de Vida**: Envio de sinais de encerramento suave (`SIGTERM`) e terminação forçada (`SIGKILL` / `TerminateProcess`).
- **Detecção de Liveness**: Consulta atômica de integridade e verificação de PID em tempo real sem travamento de threads.
- **Herança de File Descriptors Controlada**: Fechamento automático de descritores não autorizados em processos filhos (`FD_CLOEXEC`).

### 1.2 Camada de Rede & Sockets (`ar_socket.h`)
- **Modo Non-Blocking Nativo**: Configuração atômica de sockets `O_NONBLOCK` para escalabilidade massiva de I/O.
- **Backlog de Conexões em Escala**: Backlog padrão configurável em até `4096` conexões pendentes simultâneas por porta.
- **Otimização de Latência (`TCP_NODELAY`)**: Desativação obrigatória do algoritmo de Nagle para comunicação sub-milissegundo entre nós.
- **Reaproveitamento de Endereço (`SO_REUSEADDR` & `SO_REUSEPORT`)**: Reinicialização instantânea de serviços sem bloqueio por estado `TIME_WAIT`.
- **Timeouts de I/O Granulares**: Timeouts configuráveis de leitura (`ARWS_READ_TIMEOUT_MS`) e escrita para mitigar ataques Slowloris.

### 1.3 Multithreading & Sincronização (`ar_sync.h`)
- **Thread Spawning Padrão**: Criação transparente de threads nativas (`pthread_create` / `CreateThread`).
- **Fast Mutexes**: Travas de exclusão mútua (`ar_mutex`) com suporte a recursão e contenção ultra-rápida.
- **Condition Variables (`ar_cond`)**: Notificação orientada a eventos (`ar_cond_wait`, `ar_cond_signal`, `ar_cond_broadcast`), eliminando loops de busy-wait e consumo inútil de CPU.
- **Atomic Operations**: Suporte a contadores e flags atômicas para contagem de conexões ativas sem lock global.

### 1.4 Gerenciamento de Memória & HAL Utilitário
- **Linear Memory Buffers**: Alocadores de alta velocidade (`ar_mem_alloc`, `ar_mem_free`) com alinhamento de memória para instruções SIMD.
- **Timer de Alta Precisão (`ar_time_ms`)**: Leitura monotônica de milissegundos independente de alterações no relógio do sistema operacional.

---

## 2. arcore — Master Service Daemon & Supervisor

O **`arcore`** (`src/ALRIOS/core/`) é o orquestrador do sistema operacional.

### 2.1 Supervisão e Recuperação Automática (Self-Healing)
- **State Machine de Serviços**: Controle estrito dos estados `UNREGISTERED` ➔ `STOPPED` ➔ `STARTING` ➔ `RUNNING` ➔ `CRASHED` ➔ `RECOVERING`.
- **Auto-Restart Inteligente**: Detecção instantânea de queda de processos e reinicialização automática com backoff exponencial.
- **Isolamento em Duas Fases**:
  - **Fase 1**: Inicialização de serviços de infraestrutura compartilhada (`arws`, `arauth`, `ardb`, `arcdn`).
  - **Fase 2**: Inicialização de aplicações finais (`home-web`, `detroit-web`, etc.).

### 2.2 IPC Control Plane (Porta 9500)
- **Protocolo de Enquadramento 5-Bytes**: Estrutura `[1B Type][4B Payload Length (Big-Endian)][Payload]`.
- **Comandos Nativos Suportados**:
  - `IPC_PING` (`1`): Heartbeat de integridade do daemon.
  - `IPC_LIST` (`2`): Tabela detalhada de processos, PIDs, portas e consumos.
  - `IPC_START` / `IPC_STOP` (`3`/`4`): Controle individual de serviços.
  - `IPC_RELOAD` (`5`): Hot-reload de configurações em tempo de execução sem downtime.
  - `IPC_REGISTER_ROUTE` (`6`): Injeção dinâmica de novas rotas de proxy reverso no gateway.
  - `IPC_CACHE_CLEAR` (`12`): Esvaziamento instantâneo de todos os shards de cache em memória.

### 2.3 Gestão de Pacotes `.arapp` & Staging
- **Extração On-Demand**: Descompactação transparente de arquivos `.arapp` para o diretório `.staging/`.
- **Validação de Assinatura e Integridade**: Verificação de manifesto `ALRIGROUP@APPMAKE` e checagem de integridade de binários antes da execução.

---

## 3. ARWS — High-Performance Reverse Proxy, Gateway & WAF

O **ARWS (ALRI Web Services)** (`src/apps/arws/`) é a porta de entrada para todo o tráfego HTTP/HTTPS do sistema.

### 3.1 Arquitetura de Eventos & Concorrência
- **Thread Pool de Conexões**: Pool com `POOL_SIZE` threads persistentes alimentadas por filas não-bloqueantes.
- **Controle de Conexões Simultâneas (`MAX_CONNECTIONS`)**: Teto seguro de conexões ativas com resposta imediata `503 Service Unavailable` em picos de saturação.
- **Multiplexação por Polling**: Suporte a `epoll` / `WSAPoll` para monitoramento de milhares de conexões em um único ciclo.

### 3.2 Roteamento Avançado & Host Matching
- **Virtual Hosting Dinâmico**: Roteamento por múltiplos domínios (`alrigroup.com`, `detroitgg.alrigroup.com`, `localhost`, `127.0.0.1`).
- **Equivalência Localhost/127.0.0.1**: Roteamento case-insensitive que trata nomes de loopback de forma unificada.
- **Prioridade de Rotas Exatas vs Wildcards**: Rotas profundas (ex: `/arapi/auth/*`) têm precedência sobre wildcards globais (`/*`).
- **Balanceamento de Carga Upstream**: Pools de servidores de backend com suporte a peso (`weight`), nós de backup (`backup=1`) e modo de drenagem graciosa (`drain`).

### 3.3 Cache em Memória com 16 Shards
- **Zero Disk Latency**: Armazenamento de respostas HTTP completas em RAM com indexação por chave de hash.
- **TTL Granular**: Configuração de tempo de vida global (`cache_ttl`) ou supressão específica por endpoint (`no-cache`).

### 3.4 Rate Limiting & Proteção contra Abusos
- **Proteção Multi-Camadas**: Rate limits independentes por IP de cliente e por rota/host.
- **Detecção de Proxy Reverso Confiável (`X-Forwarded-For` / `CF-Connecting-IP`)**: Validação de IPs reais apenas a partir de proxies explicitamente autorizados (`ARWS_TRUSTED_PROXY`).
- **Auto-Blacklisting Temporário**: Bloqueio total de IPs ofensores por períodos configuráveis.

### 3.5 WAF (Web Application Firewall) & Headers Defensivos
- **Anti-Path Traversal**: Bloqueio de sequências `..`, caracteres codificados (`%2e%2e`, `%2f`, `%5c`) e injeção de *Null Bytes* (`\0`).
- **Content Security Policy (CSP)**: Injeção automática de políticas de restrição de scripts, estilos e mídias.
- **Mitigação de Clickjacking & Sniffing**: Injeção padrão de `X-Frame-Options: DENY` e `X-Content-Type-Options: nosniff`.
- **HSTS Forçado**: `Strict-Transport-Security: max-age=31536000; includeSubDomains`.

---

## 4. ARAUTH — Sovereign Identity & Authentication Vault

O **ARAUTH** (`src/apps/arauth/` e `src/ALRIOS/arauth/`) é o subsistema de identidade soberana, controle de acesso e auditoria, disponível tanto como **Framework Nativo C (`libarauth.a`)** quanto como **Gateway REST (`arapiauth`)**.

### 4.1 Framework Nativo `libarauth` & Plantas Baixas (App Blueprints)
- **Biblioteca Estática C Embutível (`libarauth.a`)**: Permite que qualquer aplicativo do ecossistema defina suas próprias regras de autenticação sem reinventar infraestrutura.
- **Planta Baixa Declarativa (`arauth_blueprint_t`)**: Configuração por aplicativo de meios de login aceitos (Username, Email, Celular, ID Personalizado), requisitos mínimos de senha, TTL de sessão e obrigatoriedade de 2FA.

### 4.2 Login Multi-Identificador
- **Múltiplos Meios de Contato por Conta**: O usuário pode autenticar usando Username, Email, Telefone ou ID/Passaporte com a mesma chave criptográfica.
- **Detecção Automática de Identificador**: Normalização e resolução rápida no cofre sem duplicação de dados.

### 4.3 Cargos Dinâmicos (Estilo FiveM / Discord) & Resolução em Cascata
- **Registro Dinâmico de Grupos (`arauth_group_create`)**: Definição flexível de grupos com listas de nós de permissão (`police.armory,police.radio`).
- **Múltiplos Grupos por Usuário**: Um usuário pode participar de múltiplos grupos simultaneamente (ex: `"police,vip,staff"`).
- **Resolução de Permissões em Cascata (`arauth_has_permission`)**:
  1. *Permissões Diretas*: Inspeciona permissões atribuídas especificamente à conta do usuário.
  2. *Herança de Grupos*: Inspeciona e herda todas as permissões dos grupos dos quais o usuário é membro.
  3. *Suporte a Wildcards*: Concessão global via `*` ou por namespace (`police.*`, `panel.*`).

### 4.4 Poda Imediata de Sessões na RAM (Zero-Waste)
- **Limpeza Imediata de Memória (`memset 0`)**: Tokens revogados no logout ou expirados têm seus blocos de RAM zerados instantaneamente.
- **Shift Atômico de Array**: Eliminação de retenção de sessões mortas em memória, maximizando o throughput.

### 4.5 Criptografia de Senhas (Padrão NIST SP 800-63B / OWASP)
- **PBKDF2-HMAC-SHA512 com 210.000 Iterações**: Proteção de nível governamental contra quebra de hashes por clusters de GPUs/FPGAs.
- **Salts CSPRNG Únicos (128 bits)**: Geração individual de salt por usuário via `/dev/urandom` / `RAND_bytes`.
- **Pepper / Contexto Soberano**: Mistura de salt com contexto de aplicativo (`:ALRIOS_SOVEREIGN_AUTH`).
- **Comparações em Tempo Constante**: Eliminação total de vulnerabilidades de *Timing Attack* durante a validação de senhas.

### 4.6 Multi-Factor Authentication (MFA / 2FA TOTP)
- **RFC 6238 TOTP Engine**: Algoritmo nativo em C de cálculo HMAC-SHA1 com janelas de tempo de 30 segundos.
- **Validação com Tolerância de Drift**: Aceitação de códigos na janela atual e janelas imediatamente adjacentes (±1 step).
- **Segredos Isolados**: Armazenamento de segredos Base32 no cofre sob criptografia.

### 4.7 Gestão Segura de Sessões & Anti-Replay
- **Tokens de Alta Entropia (256-bit CSPRNG)**: Geração de identificadores de sessão criptograficamente seguros em formato hexadecimal.
- **Mecanismo de Refresh Token com Rotação**: Cada renovação invalida o token anterior imediatamente (*Single-Use Refresh Tokens*), detectando roubo de sessão.
- **Armazenamento de Metadados de Sessão**: Rastreamento de IP de origem, User-Agent, data de criação e expiração absoluta.
- **Revogação Instantânea (Kill Switch)**: Invalidação granular de sessão individual ou revogação global de todas as sessões ativas de um usuário em caso de alerta.

### 4.8 Rate Limiting & Proteção contra Força Bruta
- **Rastreamento por Chave Composta (`username:client_ip`)**: Bloqueio de tentativas repetidas de adivinhação de senhas.
- **Janela de Lockout Automática**: Bloqueio com retorno `429 Too Many Requests` e contagem de tempo de cooldown.

### 4.9 Trilha de Auditoria Forense
- **Log de Eventos de Autenticação**: Registro estruturado de tentativas bem-sucedidas, falhas, bloqueios e operações administrativas.

---

## 5. ARDB — Sovereign Data Guardian & SQL Engine Proxy

O **ARDB** (`src/apps/ardb/`) é o proxy de dados soberano que inspeciona, protege e isola queries de banco de dados.

### 5.1 Isolamento de Tabelas por App & App Groups (Espaços Compartilhados)
- **Credenciais e Tokens por App**: Cada aplicativo se conecta com suas credenciais e token individuais.
- **Isolamento de Tabelas Privadas**: O aplicativo só pode acessar suas próprias tabelas (ex: `app1` acessa `app1_*`).
- **Espaços Compartilhados (App Groups)**: Criação de grupos de apps (ex: grupo `loja`) permitindo acesso conjunto a tabelas compartilhadas (`loja_produtos`, `loja_pedidos`) entre apps autorizados (`app1`, `app4`, `app5`).
- **SQL Firewall Table ACL**: Interceptação em tempo real de `SELECT`, `INSERT`, `UPDATE`, `DELETE` e `JOIN`. Tentativas de acesso não autorizado são bloqueadas com código de erro `42501 (Permission Denied)`.

### 5.2 Protocolo PG-Wire Nativo (PostgreSQL Wire Protocol v3.0)
- **Implementação Completa de Handshake**: Processamento de `StartupMessage`, autenticação (Cleartext, MD5, SASL) e negociação de parâmetros de backend.
- **Execução de Queries em Streaming**: Repasse de mensagens `Query ('Q')`, `RowDescription ('T')`, `DataRow ('D')` e `CommandComplete ('C')`.

### 5.2 Firewall SQL & Prevenção de SQL Injection
- **Validação Estrita de Identificadores de Tenant**: Whitelist alfanumérica (`[a-zA-Z0-9_-]`), impedindo escape de aspas em injeções multi-tenant.
- **Injeção de RLS Automática**: Aplicação forçada de `SET LOCAL alri.tenant_id = '...'` no início de cada transação.
- **Bloqueio de DDL/DML Destrutivo**: Impedimento de comandos perigosos (`DROP TABLE`, `DROP DATABASE`, `TRUNCATE`, `ALTER SYSTEM`, `GRANT ALL`, `COPY TO PROGRAM`) para perfis não-administradores.
- **Detecção de Evasão de RLS**: Alertas contra tentativas de manipulação de cláusulas `where tenant_id` e comentários maliciosos.

### 5.3 Auditoria Imutável com Hash-Chain (Blockchain-Style)
- **Assinatura SHA-256 em Cadeia**: Cada query auditada gera um hash criptográfico que inclui o hash do evento anterior (`prev_hash`).
- **Verificação Forense de Integridade (`alrios ardb audit verify`)**: Validação de ponta a ponta da cadeia para comprovar que nenhum registro foi alterado ou apagado retroativamente.

---

## 6. ARWN — ALRI Web Native Framework & `.arweb` Containers

O **ARWN (ALRI Web Native)** (`src/apps/arwn/`) é a arquitetura de empacotamento e entrega de aplicações web sem acesso a disco em runtime.

### 6.1 Contêiner Binário `.arweb`
- **Zero Disk Lookups**: Todos os recursos (HTML, CSS, JS, WASM) são montados em uma única estrutura binária em memória RAM.
- **Tabela de Seções com CRC32**: Validação de integridade de 48 bytes por seção contra corrupção de dados.
- **Entrega por Faixas de Memória**: O servidor ARWN serve payloads diretamente a partir de ponteiros de memória compartilhada.

### 6.2 Motores WebAssembly (WASM) Nativos
- **Micro-Engines em C / C++ / Rust / Go**: Suporte a execução de código compilado para WASM com memória linear isolada.
- **Chamadas Bidirecionais JS ↔ WASM**: Módulos carregados automaticamente através da bridge `arwn-bridge.js`.

### 6.3 Ofuscação de Propriedade Intelectual
- **Base64 VM Container**: Encapsulamento de código JavaScript proprietário dentro de interpretadores protegidos quando configurado `obfuscate=yes`.
- **Injeção de Licenciamento Soberano**: Aplicação compulsória dos cabeçalhos legais de licenciamento do ALRI Group em todas as saídas de compilação.

---

## 7. ARCDN — High-Throughput Edge & Media Server

O **ARCDN** (`src/apps/arcdn/`) é o subsistema de entrega de arquivos estáticos, mídias e streaming.

### 7.1 Zero-Copy I/O (`sendfile`)
- **Transferência Direta Kernel-to-Socket**: Envio de arquivos diretamente do page cache do kernel para a placa de rede sem cópia para o espaço do usuário.
- **Streaming de Mídias com Suporte a Range Requests (`HTTP 206 Partial Content`)**: Reprodução fluida de vídeos MP4/WebM com seeking instantâneo.

### 7.2 Cache de Descritores & Mapeamento de Tipos MIME
- **Tabela de MIME Types Otimizada**: Resolução instantânea de tipos para mais de 100 extensões comuns (`.html`, `.css`, `.js`, `.png`, `.svg`, `.wasm`, `.arweb`, etc.).
- **Controle de Cache HTTP**: Emissão automática de cabeçalhos `ETag` e `Cache-Control: public, max-age=...` para eficiência de CDN.

---

## 8. Developer Toolchain & CLI (`alrios`, `armake`, `arinstall`)

Ferramentas nativas (`src/tools/`) que gerenciam todo o ciclo de desenvolvimento, compilação, empacotamento e operação do sistema.

### 8.1 `armake` — Application Packager
- **Construção de Pacotes `.arapp`**: Empacotamento estruturado de binários, manifests e assets em arquivos autocontidos.
- **Extração e Inspeção**: Comandos `armake build`, `armake extract` e `armake list`.

### 8.2 `arinstall` — Runtime Manager
- **Instalação Isolada de Ambientes**: Download e preparação de runtimes de linguagens (Node.js, Python, OpenJDK, Go, Lua) dentro de `arcore/programfiles/` sem poluir o sistema operacional hospedeiro.

### 8.3 `alrios` — CLI Unificado de Governança
- **Controle de Energia**: `alrios power on | off | reload`.
- **Monitoramento em Tempo Real**: `alrios status` (exibe serviços, PIDs, estado da memória e saúde do gateway).
- **Gestão de Rotas & Upstreams**: `alrios arws upstream list | add | drain | remove`.
- **Governança do Banco de Dados**: `alrios ardb status | auth login | auth revoke | audit tail | audit verify`.

---

## 9. Segurança em Camadas & Matriz Criptográfica

| Camada | Mecanismo | Algoritmo / Padrão | Objetivo |
| :--- | :--- | :--- | :--- |
| **Senhas** | Derivação de Chave | **PBKDF2-HMAC-SHA512 (210.000 iterações)** | Proteção contra ataques com GPUs/FPGAs |
| **Sessões** | Identificadores de Sessão | **CSPRNG 256-bit Hex + Single-Use Refresh** | Anti-Hijacking e mitigação de Replay |
| **MFA** | Autenticação em Duas Etapas | **RFC 6238 TOTP (HMAC-SHA1)** | Segundo fator de autenticação |
| **E2EE / Payload** | Criptografia Simétrica | **AES-256-GCM com Tag de Autenticação 128-bit** | Sigilo e integridade de ponta a ponta |
| **Banco de Dados** | Isolamento Multi-Tenant | **SET LOCAL alri.tenant_id + Regex Whitelist** | Prevenção absoluta de vazamento cross-tenant |
| **Auditoria** | Log Forense Imutável | **Blockchain-Style SHA-256 Hash Chain** | Garantia de não-repúdio e detecção de adulteração |
| **Rede & Gateway** | Proteção de Tráfego | **TLS 1.3 / CSP / HSTS / Anti-Path Traversal** | Proteção contra interceptação, XSS e CSRF |
