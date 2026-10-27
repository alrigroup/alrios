# ALRIOS Ecosystem — Evolução Arquitetural e Roadmap Oficial (v1.1+)
**Documento Estratégico de Engenharia e Especificações Futuras**  
*ALRIGROUP & ALRI Development*

---

## 1. A Nova Arquitetura de Autenticação Soberana

A separação é cirúrgica e divide-se em dois componentes com responsabilidades estritas:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              arauth (PÚBLICO / OPEN SOURCE / MOTOR UNIVERSAL)               │
│                                                                             │
│  - Contém TODA a lógica de segurança, regras de negócio e criptografia      │
│  - Não tem interface web própria e não se preocupa com domínios específicos │
│  - Hashing Argon2id / PBKDF2, gestão de tokens Ed25519, rate limit, anti-brute│
│  - Totalmente livre: qualquer desenvolvedor no mundo usa em app próprio     │
│    sem nenhum vínculo ou telemetria com a ALRIGROUP (100% offline).         │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ (Invocado como Engine Core)
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│           arauth.web (PRIVADO / ALRIGROUP OFFICIAL IDENTITY PORTAL)         │
│                        (Antigo arapiauth remodelado)                        │
│                                                                             │
│  - NÃO POSSUI LÓGICA DE NEGÓCIO PRÓPRIA (Pass-Through / Gateway / BFF)      │
│  - Responsável EXCLUSIVO por orquestrar o portal auth.alrigroup.com         │
│  - Ponte entre a Interface Web Frontend (Liquid Glass no ARWE) e o arauth   │
│  - Gerencia o fluxo visual: telas de login, cadastro rápido, recuperação de │
│    senha, telas de consentimento SSO ("Entrar com ALRIGROUP"), MFA/Passkeys │
│  - Apenas recebe as requisições HTTP do front, repassa para o arauth motor, │
│    e devolve os tokens assinados e cookies de sessão para o navegador.      │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Modos de Uso Prático

### Modo A: Desenvolvedor Externo (Sem Vínculo com a ALRIGROUP)
- O desenvolvedor importa o **`arauth` (Público)** no projeto dele (C, Node, Go, Python).
- Ele tem o motor completo de autenticação funcionando no banco dele (SQLite, Postgres ou `ardb`).
- Ele não precisa do `arauth.web` da ALRI; ele constrói o front dele como quiser ou usa o `arauth` como biblioteca pura.

### Modo B: Ecossistema Oficial ALRIGROUP (`auth.alrigroup.com`)
- O usuário acessa os serviços da ALRIGROUP (ArgonGE, SC-ZeroHum, ARWS, portal corporativo).
- Clica em **"Entrar com ALRIGROUP"**.
- É direcionado para o **`arauth.web`** no domínio `auth.alrigroup.com`.
- O `arauth.web` renderiza a interface visual, colhe as credenciais e chama o motor `arauth` por baixo dos panos.
- O token é assinado pela autoridade da ALRIGROUP e o usuário volta logado para o app.

---

## 3. ARAPIAUTH v2 — Middleware Estático de Alta Performance

Com o antigo `arapiauth` promovido para `arauth.web`, a função de interceptador de API vira uma biblioteca/middleware leve:
- **Zero Chamadas de Rede**: Valida tokens em memória RAM usando apenas a chave pública Ed25519 (tempo de resposta < 0.05ms).
- **M2M e Usuários**: Valida tanto Bearer Tokens de usuários emitidos pelo `auth.alrigroup.com` quanto API Keys estáticas de comunicação entre microsserviços (`alri_live_...`).

---

## 4. ARDB v2 — Motor de Armazenamento Universal Multi-Modelo

### 4.1 Conceito: "One Engine, Multiple Protocols"
Elimina a necessidade de instalar múltiplos gerenciadores de banco de dados no servidor (como Postgres, Redis e MySQL separados). Um único binário compilado em C de altíssimo desempenho expondo os protocolos padrão do mercado:

1. **Protocolo PostgreSQL Wire (Porta 5432)**:
   - Totalmente compatível com clientes Postgres, DBeaver, TypeORM, Prisma, etc.
2. **Protocolo MySQL Wire (Porta 3306)**:
   - Compatibilidade com ferramentas e drivers do ecossistema MySQL/MariaDB.
3. **Protocolo Redis RESP (Porta 6379)**:
   - Cache em memória ultrarrápido sem locks (Lock-free Atomic CAS), filas de mensagens, TTL e Pub/Sub.
4. **Motor de Grafos (IPC / GraphQL / Cypher)**:
   - Armazenamento de vértices e arestas por ponteiros diretos em disco para relacionamentos complexos, dispensando JOINs pesados.

### 4.2 Isolamento de Dados por Aplicação (Multi-Tenancy Automático)
- Cada aplicativo instalado via `arpm` recebe seu namespace isolado no storage do kernel:
  - `ardb://localhost/argonge` ➔ `/opt/alrios/arcore/storage/argonge/data.db`
  - `ardb://localhost/sc-zerohum` ➔ `/opt/alrios/arcore/storage/sc-zerohum/data.db`
- Garante isolamento físico entre aplicações no mesmo servidor.

### 4.3 Concorrência Avançada & Backups a Quente
- Modo WAL2 (Write-Ahead Logging) com arquitetura Single-Writer / Multi-Reader sem travamentos de concorrência (`SQLITE_BUSY`).
- Snapshots atômicos a quente criptografados com AES-256-GCM sem paralisação do serviço.

---

## 5. Estudo de Caso Prático: Aplicação Externa (Ex: ArgonGE)

- **Login Flexível**: O usuário do ArgonGE pode logar localmente (professor/aluno) via banco SQLite local gerenciado pela `libarauth`, ou usar o botão universal **"Entrar com ALRIGROUP"** via `auth.alrigroup.com` (`arauth.web`).
- **Independência**: O ArgonGE pode ser vendido para rodar em servidores fechados de escolas sem qualquer dependência da ALRIGROUP.
- **Alta Performance de API**: O backend valida tokens em microssegundos via `arapiauth` na memória RAM.
- **Persistência Unificada**: O ArgonGE conecta no `ardb` usando SQL para turmas/provas e cache rápido na porta 6379 para sessões, consumindo menos de 30MB de memória total.
