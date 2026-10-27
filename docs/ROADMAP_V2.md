# ALRIOS Ecosystem Evolution Roadmap (v1.1+)
**Documento de Especificação Arquitetural e Planejamento de Futuro**  
*Mantido por ALRI Development & ALRIGROUP*

---

## 1. ARAUTH v2 — Identidade Soberana & Desacoplamento Global

### 1.1 O Diagnóstico Atual
O `arauth` atual é excessivamente acoplado ao nó local e burocrático (cadastros longos e regras rígidas de formulário). A nova abordagem separa a **experiência do usuário** da **lógica do sistema**.

### 1.2 Os Três Modos de Operação
1. **Modo Standalone / Embedded (Offline / On-Premises)**:
   - Qualquer desenvolvedor no mundo pode usar a biblioteca `libarauth` em aplicativos próprios sem nenhum vínculo, telemetria ou dependência da ALRIGROUP.
   - Gerencia hash de senhas (Argon2id/PBKDF2), rate limit, sanitização e sessões diretamente no SQLite local do app.
2. **Modo Self-Hosted IdP (Privado Empresarial)**:
   - Empresas que rodam ALRIOS podem hospedar seu próprio portal de autenticação (ex: `auth.minhaempresa.com`).
   - Gerencia usuários internos e emite tokens corporativos próprios.
3. **Modo Federado Oficial (`auth.alrigroup.com`)**:
   - Serviço web oficial da ALRIGROUP (`account.google.com` do ecossistema).
   - Frontend em SPA moderna (Liquid Glass) rodando no ARWE com suporte a MFA, Passkeys/WebAuthn e gestão de perfil.
   - Emite tokens assinados assimetricamente com chaves Ed25519 corporativas.

---

## 2. ARAPIAUTH v2 — Middleware de Alta Velocidade Baseado em Chave Pública

### 2.1 Validação Local em Memória (Zero Chamadas de Rede)
- O `arapiauth` passa a atuar como um middleware de validação estática.
- Em vez de fazer chamadas HTTP de volta ao `arauth` a cada request, ele apenas valida a assinatura criptográfica Ed25519 do Bearer Token utilizando a chave pública em memória RAM (latência < 0.05ms).

### 2.2 Suporte Duplo (M2M + Usuários)
- **User Tokens**: Sessões de usuários emitidas pelo `auth.alrigroup.com` ou instâncias locais.
- **Service API Keys**: Chaves estáticas de comunicação entre microsserviços com hash de integridade (`alri_live_...`).

---

## 3. ARDB v2 — Universal Multi-Model Storage Engine

### 3.1 O Conceito: Motor Único, Múltiplas Interfaces
Elimina a necessidade de instalar e manter múltiplos bancos de dados no mesmo servidor. Um único binário compilado em C de altíssimo desempenho expondo os protocolos nativos padrão da indústria.

### 3.2 As Quatro Camadas do ARDB Universal:
1. **Protocolos Wire Multiplexados**:
   - **PostgreSQL Wire Protocol (Porta 5432)**: Compatibilidade com DBeaver, TypeORM, Prisma, etc.
   - **MySQL Wire Protocol (Porta 3306)**: Suporte a clientes e drivers MySQL legados.
   - **Redis RESP Protocol (Porta 6379)**: Compatibilidade com `redis-cli`, `ioredis`, cache em RAM, TTL e Pub/Sub sem locks.
   - **Graph Protocol (IPC / GraphQL / Cypher)**: Consultas diretas de grafos e relacionamentos complexos sem JOINs caros.
2. **Isolamento de Armazenamento por Aplicação (Multi-Tenancy Automático)**:
   - Cada aplicativo instalado via `arpm` ganha seu namespace automático no storage:
     - `ardb://localhost/argonge` ➔ `/opt/alrios/arcore/storage/argonge/data.db`
     - `ardb://localhost/sc-zerohum` ➔ `/opt/alrios/arcore/storage/sc-zerohum/data.db`
   - Garante que a invasão de um app não comprometa os dados de outros apps.
3. **Alta Concorrência & WAL2**:
   - Arquitetura Single-Writer / Multi-Reader sem contenção de locks para escrita intensiva.
4. **Snapshots Criptografados a Quente**:
   - Backups atômicos instantâneos cifrados com a chave AES-256-GCM do nó sem paralisação do banco.

---

## 4. Integração Prática em Apps do Ecossistema (Exemplo: ArgonGE)

- **Login Simples**: O usuário pode optar por login local simples (aluno/professor) gerenciado pela `libarauth` ou clicar no botão universal **"Entrar com ALRIGROUP"**.
- **Performance de API**: O backend do ArgonGE valida tokens via `arapiauth` na memória sem gargalos de rede.
- **Persistência**: O ArgonGE armazena dados relacionais, sessões em cache rápido e grafos de turmas diretamente no mesmo daemon `ardb`, com pegada mínima de memória.
