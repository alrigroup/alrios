# ALRIOS Ecosystem — Evolução Arquitetural e Roadmap Oficial (v1.1+)
**Documento Estratégico de Engenharia e Especificações Futuras**  
*ALRIGROUP & ALRI Development*

---

## 1. ARAUTH v2 — Matriz de Perfis de Segurança & Modelos de Execução
*(Do Login Básico ao Nível Governamental / Militar com Criptografia Pós-Quântica)*

O `arauth` não impõe um modelo único rígido. Ele implementa **4 Tiers de Execução de Segurança**, permitindo desde logins ultra-leves e sem atrito até o modo de paranoia máxima:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       ARAUTH SECURITY PROFILES MATRIX                       │
├───────────────────┬───────────────────┬───────────────────┬─────────────────┤
│    TIER 0: LITE   │ TIER 1: STANDARD  │ TIER 2: ENTERPRISE│ TIER 3: PARANOID│
│   (Dev / Local)   │  (SaaS / Web App) │(Bancos / Fintechs)│(Gov / Defesa)   │
├───────────────────┼───────────────────┼───────────────────┼─────────────────┤
│ • User + Senha    │ • Senha + TOTP    │ • 2FA/3FA Obrigat.│ • 3FA + PQC     │
│ • SFA Simples     │ • Magic Links     │ • WebAuthn/Passkey│ • FIPS 204      │
│ • PBKDF2 / SHA-2  │ • Argon2id        │ • Device Binding  │   (ML-DSA-65)   │
│ • JWT Clássico    │ • Ed25519 Token   │ • RTR + mTLS      │ • FIPS 203      │
│ • Sessão longa    │ • 15 min TTL + RTR│ • Análise de Risco│   (ML-KEM-768)  │
│                   │                   │   (Adaptive Auth) │ • Hardware Key  │
│                   │                   │                   │ • Zero Trust IPC│
└───────────────────┴───────────────────┴───────────────────┴─────────────────┘
```

### 1.1 Fatores de Autenticação Suportados:
- **Algo que você sabe (Knowledge)**: Senhas de alta entropia, PIN, Passphrases.
- **Algo que você tem (Possession)**: Hardware Keys (YubiKey / FIDO2), TOTP (Aegis/Google Auth), Smartcard PIV, SMS/Email descartável.
- **Algo que você é (Inherence)**: Biometria nativa WebAuthn (TouchID, FaceID, leitor biométrico Windows Hello).
- **Fatores Contextuais (Adaptive/Risk-Based)**: Validação contínua de IP, ASN, geolocalização e impressão digital de dispositivo.

### 1.2 Os 4 Tiers Detalhados:

1. **TIER 0: LITE (Single-Factor - SFA)**:
   - Uso: Apps internos locais, ferramentas de dev, protótipos e softwares sem dados sensíveis.
   - Mecânica: E-mail/User + Senha com PBKDF2/SHA-512 rápido. Sem 2FA obrigatório.
2. **TIER 1: STANDARD (2FA Opcional / SaaS — Ex: ArgonGE)**:
   - Uso: Escolas, plataformas web comerciais, dashboards de gestão.
   - Mecânica: Argon2id para senhas + suporte a TOTP e Magic Links. Tokens assinados com **Ed25519**. Access Token curto (15m) + Refresh Token Rotation (RTR) em cookie `HttpOnly`.
3. **TIER 2: ENTERPRISE (Fintech / Bancário / Compliance)**:
   - Uso: Bancos, gateways de pagamento, prontuários médicos (LGPD/HIPAA).
   - Mecânica: **Passkeys / WebAuthn obrigatório** (imune a phishing). Comunicação mTLS entre microsserviços. Autenticação Adaptativa baseada em risco com **Step-Up Authentication** (exige biometria/chave física em rotas de alto valor, como transferências ou exclusões em lote).
4. **TIER 3: PARANOID / SOVEREIGN (Governo, Defesa e Infraestrutura Crítica)**:
   - Uso: Sistemas militares, bancos centrais, governamentais ou no perfil `SOVEREIGN-MAX` do kernel.
   - Mecânica: **3FA Obrigatório** (Senha + Hardware Token + Biometria).
   - **Criptografia Pós-Quântica Nativa (PQC)**:
     - Assinaturas de tokens e sessões com **FIPS 204 ML-DSA-65**.
     - Enkapsulamento de chaves e trânsito com **FIPS 203 ML-KEM-768**.
     - Trilha imutável em hash-chain no daemon `ar_auditd`.

---

## 2. A Hierarquia Universal de Permissões (Tenant ➔ Group ➔ Role ➔ Access)
*(Inspirado no modelo de Grupos e Permissões do vRP/FiveM e AWS IAM)*

O backend das aplicações **não consulta o banco de dados diretamente** para checar acessos; ele faz perguntas declarativas para o `arauth`:

```
[ TENANT / EMPRESA ] (ex: "alrigroup", "escola-objetivo")
        │
        ├──▶ [ GRUPOS ] (ex: "master", "staff", "professores", "financeiro")
        │         │
        │         └── Cargos (Roles): "ceo", "developer", "coordenador"
        │                   │
        │                   └── Permissões/Flags: ["provas.create", "provas.read", "*"]
        │
        └──▶ [ USUÁRIO INDIVIDUAL ]
                  ├── Pertence aos Grupos: ["professores"]
                  ├── Herda automaticamente todos os acessos dos grupos
                  │
                  └── Exceções Individuais (Overrides):
                        ├── + Permissão Extra: "sistema.debug" (apenas para esse usuário)
                        └── - Negação Explícita: "!provas.delete" (bloqueio cirúrgico)
```

### 2.1 API Declarativa do App:
```javascript
// O app não sabe como a permissão foi calculada; ele só pergunta:
if (!await arauth.hasPermission(userId, "provas.delete", { tenant: "escola-xyz" })) {
    return res.status(403).json({ error: "Acesso negado: requer 'provas.delete'" });
}
```
- **Checagem Rápida em Memória (O(1))**: As permissões são compiladas no login e validadas em microssegundos na RAM.
- **Suporte a Wildcard (`*`)**: Poder absoluto para grupos master.

---

## 3. Os 3 Modos de Operação do ARAUTH

1. **Modo Standalone / Embedded (Offline / Apps de Clientes)**:
   - Qualquer app construído pela ALRIGROUP para clientes externos importa o `arauth` como motor próprio.
   - **Vínculo com a ALRIGROUP: ZERO.** Totalmente offline e on-premises, sem telemetria. Os dados dos usuários e permissões ficam no banco local do cliente (`storage/<app>/auth.db`).
2. **Modo Self-Hosted IdP (SSO Corporativo do Cliente)**:
   - Empresas rodando ALRIOS podem subir seu próprio portal SSO privado (ex: `auth.cliente.com`).
3. **Modo Federado Oficial (`alriauth.web` em `auth.alrigroup.com`)**:
   - Serviço oficial de contas da ALRIGROUP para a nuvem.

---

## 4. ALRIAUTH.WEB — O Portal Oficial de Identidade (auth.alrigroup.com)
*(Evolução do antigo arapiauth remodelado)*

- **Papel**: É a casca visual e portal público da ALRIGROUP (equivalente ao `account.google.com`).
- **NÃO POSSUI LÓGICA DE NEGÓCIO PRÓPRIA (Pass-Through / Gateway / BFF)**:
  - Não faz hashing manual nem inventa regras de banco; apenas recebe requisições HTTP do frontend e invoca o motor `arauth`.
- **Frontend Liquid Glass (ARWE)**:
  - Telas modernas de login, cadastro rápido, recuperação de senha, MFA/Passkeys e consentimento SSO (*"Entrar com ALRIGROUP"*).
- **APIs Enxutas (6 rotas fundamentais)**:
  - `POST /api/v1/auth/login`
  - `POST /api/v1/auth/register`
  - `GET  /api/v1/auth/me`
  - `POST /api/v1/auth/logout`
  - `GET  /oauth/authorize` & `POST /oauth/token` (Fluxo SSO)
  - `GET  /.well-known/jwks.json` (Chave pública para validação offline de tokens)

---

## 5. ARAPIAUTH v2 — Middleware Estático de Alta Performance

- Interceptador de rotas de API nas aplicações.
- **Validação Local na RAM (Zero Chamadas de Rede)**:
  - Valida a assinatura assimétrica Ed25519 do Bearer Token em `< 0.05ms` na memória usando a chave pública.
- **Suporte Duplo**: Tokens de usuários e chaves de microsserviços M2M (`alri_live_xxxxxxxx`).

---

## 6. ARDB v2 — Motor de Armazenamento Universal Multi-Modelo

### 6.1 "One Engine, Multiple Protocols"
Um único binário em C compilado no nó, consumindo menos de 30MB de RAM e expondo os protocolos padrão do mercado:
1. **PostgreSQL Wire Protocol (Porta 5432)**: Compatível com DBeaver, TypeORM, Prisma, etc.
2. **MySQL Wire Protocol (Porta 3306)**: Compatível com drivers e ecossistemas MySQL.
3. **Redis RESP Protocol (Porta 6379)**: Cache ultrarrápido sem locks (Lock-free Atomic CAS), TTL, filas e Pub/Sub.
4. **Motor de Grafos (IPC / GraphQL / Cypher)**: Vértices e arestas nativos em disco, sem JOINs caros.

### 6.2 Multi-Tenancy e Concorrência:
- Isolamento automático de armazenamento: `storage/<app>/data.db`.
- Modo WAL2 sem travamentos concorrentes e backups atômicos a quente com AES-256-GCM.

---

## 7. Estudo de Caso Prático: Aplicação de Cliente (Ex: ArgonGE)

- **Login e Permissões Desacopladas**: O ArgonGE não cria tabelas de usuários nem colunas `role`. O `arauth` gerencia grupos (`professores`, `alunos`) e flags (`provas.create`).
- **Autonomia Total**: O cliente roda 100% offline em Tier 1. Em nuvem corporativa, pode ativar Tier 2 com Passkeys e SSO da ALRIGROUP.
- **Persistência Completa no ARDB**: SQL para provas e Redis/Cache para sessões no mesmo daemon `ardb`.
