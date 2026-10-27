# ALRIOS Ecosystem — Evolução Arquitetural e Roadmap Oficial (v1.1+)
**Documento Estratégico de Engenharia e Especificações Futuras**  
*ALRIGROUP & ALRI Development*

---

## 1. ARAUTH v2 — Motor Universal de Identidade, Grupos e Permissões

### 1.1 O Diagnóstico e a Nova Proposta
O `arauth` original era engessado e acoplado. Na v2, ele se torna o **Motor Universal de Segurança e Permissões** da ALRIGROUP:
- Não tem interface web própria e não se preocupa com domínios ou páginas.
- Funciona como um **Policy & Authority Engine** (inspirado no modelo de Grupos e Permissões do vRP/FiveM e AWS IAM).
- O backend das aplicações **não consulta o banco de dados diretamente** para checar acessos; ele faz perguntas declarativas para o `arauth`:
  ```javascript
  if (!await arauth.hasPermission(userId, "provas.delete", { tenant: "escola-xyz" })) {
      return res.status(403).json({ error: "Acesso negado: requer 'provas.delete'" });
  }
  ```

---

## 2. A Hierarquia Universal de Permissões (Tenant ➔ Group ➔ Role ➔ Access)

O `arauth` implementa controle de acesso granular multi-empresa com suporte a herança de grupos e exceções individuais:

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

### 2.1 API Declarativa para Gerenciamento de Permissões:
- **Gerenciamento de Grupos**:
  ```javascript
  arauth.setGroup(tenant, groupName, { roles: [...], permissions: [...] })
  ```
- **Vinculação de Usuários**:
  ```javascript
  arauth.addUserGroup(userId, groupName, { tenant })
  arauth.removeUserGroup(userId, groupName, { tenant })
  ```
- **Exceções por Usuário (Granular)**:
  ```javascript
  arauth.giveUserPermission(userId, "permissao.extra")
  arauth.denyUserPermission(userId, "!permissao.bloqueada")
  ```
- **Checagem Rápida em Memória (O(1))**:
  ```javascript
  arauth.hasPermission(userId, "flag", { tenant }) // Responde em microssegundos
  arauth.hasGroup(userId, "grupo", { tenant })
  ```

---

## 3. Os 3 Modos de Operação do ARAUTH

1. **Modo Standalone / Embedded (Offline / Apps de Clientes)**:
   - Qualquer app construído pela ALRIGROUP para clientes externos (ou por devs de fora) importa o `arauth` como motor próprio.
   - **Vínculo com a ALRIGROUP: ZERO.** Totalmente offline e on-premises, sem telemetria. Os dados dos usuários e permissões ficam gravados no banco local do cliente (`storage/<app>/auth.db`).
2. **Modo Self-Hosted IdP (SSO Corporativo do Cliente)**:
   - Empresas rodando ALRIOS podem subir seu próprio portal SSO privado (ex: `auth.cliente.com`).
3. **Modo Federado Oficial (`alriauth.web` em `auth.alrigroup.com`)**:
   - Serviço oficial de contas da ALRIGROUP para a nuvem.

---

## 4. ALRIAUTH.WEB — O Portal Oficial de Identidade (auth.alrigroup.com)
*(Evolução do antigo arapiauth remodelado)*

- **Papel**: É a casca visual e portal público da ALRIGROUP (equivalente ao `account.google.com`).
- **NÃO POSSUI LÓGICA DE NEGÓCIO PRÓPRIA (Pass-Through / Gateway / BFF)**:
  - Não faz hashing manual nem inventa regras de banco; ele apenas recebe as requisições HTTP do frontend e invoca o motor `arauth` por baixo.
- **Frontend Liquid Glass (ARWE)**:
  - Telas modernas de login, cadastro rápido, recuperação de senha, MFA/Passkeys e consentimento SSO (*"Entrar com ALRIGROUP"*).
- **APIs Enxutas (6 rotas fundamentais)**:
  - `POST /api/v1/auth/login`
  - `POST /api/v1/auth/register`
  - `GET  /api/v1/auth/me`
  - `POST /api/v1/auth/logout`
  - `GET  /oauth/authorize` & `POST /oauth/token` (Fluxo SSO)
  - `GET  /.well-known/jwks.json` (Chave pública Ed25519 para validação offline de tokens)

---

## 5. ARAPIAUTH v2 — Middleware Estático de Alta Performance

- Atua como interceptador de rotas de API nas aplicações.
- **Validação Local na RAM (Zero Chamadas de Rede)**:
  - Quando a requisição chega no ArgonGE com `Authorization: Bearer <token>`, o `arapiauth` valida a assinatura Ed25519 em `< 0.05ms` na memória usando a chave pública.
- **Suporte Duplo**:
  - Tokens de usuários emitidos pelo portal web.
  - Chaves estáticas de microsserviços Machine-to-Machine (`alri_live_xxxxxxxx`).

---

## 6. ARDB v2 — Motor de Armazenamento Universal Multi-Modelo

### 6.1 "One Engine, Multiple Protocols"
Um único binário em C compilado no nó, consumindo menos de 30MB de RAM e expondo os protocolos padrão do mercado:
1. **PostgreSQL Wire Protocol (Porta 5432)**: Compatível com DBeaver, TypeORM, Prisma, etc.
2. **MySQL Wire Protocol (Porta 3306)**: Compatível com drivers e ecossistemas MySQL.
3. **Redis RESP Protocol (Porta 6379)**: Cache ultrarrápido sem locks (Lock-free Atomic CAS), TTL, filas e Pub/Sub.
4. **Motor de Grafos (IPC / GraphQL / Cypher)**: Armazenamento nativo de vértices e arestas em disco, dispensando JOINs pesados para relações complexas.

### 6.2 Isolamento de Dados por Aplicação (Multi-Tenancy Automático)
- Cada aplicativo instalado via `arpm` recebe seu storage exclusivo:
  - `ardb://localhost/argonge` ➔ `/opt/alrios/arcore/storage/argonge/data.db`
  - `ardb://localhost/sc-zerohum` ➔ `/opt/alrios/arcore/storage/sc-zerohum/data.db`
- Modo WAL2 sem travamentos concorrentes e backups atômicos a quente com AES-256-GCM.

---

## 7. Estudo de Caso Prático: Aplicação de Cliente (Ex: ArgonGE)

- **Login e Permissões Desacopladas**:
  - O ArgonGE não cria tabelas de usuários nem colunas `role`/`is_admin`.
  - O `arauth` armazena e gerencia os grupos (`professores`, `alunos`, `coordenacao`) e suas flags (`provas.create`, `turmas.edit`).
  - O app apenas pergunta: `arauth.hasPermission(userId, "provas.create", { tenant: "escola-01" })`.
- **Autonomia de Cliente**:
  - Uma escola cliente roda o ArgonGE 100% offline com o `arauth` local.
  - Se a ALRIGROUP quiser oferecer o ArgonGE em nuvem, ativa o botão SSO para o `alriauth.web` (`auth.alrigroup.com`).
- **Persistência Completa no ARDB**:
  - SQL na porta 5432 para provas/turmas + Cache rápido na porta 6379 para sessões, sem precisar instalar Postgres ou Redis externos.
