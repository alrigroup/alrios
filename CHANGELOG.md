# Changelog

## [1.0.0] — 2026-10-15

### Sovereign General Availability Release (v1.0.0)

Primeira versão estável oficial (GA) do ecossistema ALRIOS, alcançando paridade funcional, arquitetural e de segurança estrita entre **Linux** e **Windows**.

### Destaques da Versão

- **Paridade Funcional e de Confinamento (Linux & Windows)**:
  - **Linux**: cgroups v2, namespaces PID/Mount, Landlock e seccomp-BPF estritos.
  - **Windows**: Windows Job Objects com `KILL_ON_JOB_CLOSE`, controle estrito de memória (`ProcessMemoryLimit` / `JobMemoryLimit`), limites de processos ativos (`ActiveProcessLimit`), contenção de caminhos canônicos anti-traversal e políticas de mitigação de processos contra criação desautorizada de processos filhos.
- **Camada Canônica de I/O e Filesystem (`fs_compat.h`)**:
  - Implementação e emulação nativa em Win32 de chamadas POSIX `*at` (`openat`, `fstatat`, `unlinkat`, `mkdirat`, `linkat` atômico via hardlink com fallback seguro), `pread`, `pwrite` e `flock`.
- **Compilação e Linkagem Estática de Runtime**:
  - Binários Windows compilados com `-static -static-libgcc`, executando de forma soberana sem necessidade de DLLs externas ou runtimes pré-instalados.
  - Binários Linux compilados com rigor máximo (`-std=c11 -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes`).
- **Arquitetura C11 Modular e Governança**:
  - Kernel soberano, supervisor de daemons, event broker daemon (`ar_eventd`), auditoria criptográfica (`ar_auditd`), time sync (`ar_chronod`), crash reporter (`ar_cored`) e monitor de recursos (`ar_resourced`).
  - Suporte criptográfico pós-quântico (FIPS 204 ML-DSA-65 e FIPS 203 ML-KEM-768) integrado a assinaturas Ed25519 (FIPS 186-5 / RFC 8032).
  - Gate de segurança automatizado de APIs proibidas aprovado com zero violações em 141 arquivos.
- **Licenciamento Open Source Unificado**:
  - Código-fonte, documentações e especificações unificados sob a licença permissiva **ARGLP - ALRI GROUP LICENSE PERMISSIVE**.

---

## [0.9.3] — 2026-10-15

### Linux-only release

- Publicação principal temporariamente limitada ao instalador Linux x64.
- Pipeline de release simplificada para não bloquear a release enquanto a portabilidade Windows é corrigida e validada.
- Testes, servidor de desenvolvimento não utilizado, arquivos gerados, caches locais e instruções exclusivas para agentes removidos do checkout.

## [0.9.2] — 2026-10-15

### Release candidate público para a v1.0

Esta versão inicia a validação de uso real antes da estabilização v1.0. O histórico Git anterior é preservado para rastreabilidade, bisect, auditoria e comparação de compatibilidade.

### Destaques

- Arquitetura C11 modular para kernel, HAL, supervisor, CLI, empacotamento e SDK.
- Cadeia PKI/notário, assinaturas híbridas e suporte criptográfico pós-quântico integrado.
- Execução zero-disk, políticas de sandbox, vault, auditoria, IPC autenticado e event bus.
- Hot reload, transferência de sockets, drenagem de conexões e supervisão contextual.
- Builds reprodutíveis, SBOM/proveniência, compilador estrito `arcc` e gate de APIs proibidas.

### Qualificação da revisão candidata

Na revisão imediatamente anterior ao bump de versão:

- build Linux Release aprovado sem warnings observados;
- 60/60 testes CTest aprovados no ambiente de rede esperado;
- 60/60 testes aprovados com ASan/UBSan, sem diagnóstico observado;
- gate de APIs proibidas aprovado em 221 arquivos;
- seis fuzzers executados com 100.000 casos limitados por alvo, totalizando 600.000 execuções sem crash observado.

Uma execução com rede totalmente isolada obteve 59/60 porque `test_app_lifecycle` exige timeout para um endereço TEST-NET; sem rota, a pilha pode retornar erro imediatamente. O caso passou repetidamente usando a rede normal do host.

### Limitações conhecidas

- Esta publicação é temporariamente exclusiva para Linux x64.
- Windows permanece fora desta versão, pendente de correções de portabilidade e validação nativa.
- A suíte automatizada não equivale à execução individual de todos os 88 planos adversariais documentados.
- Esta versão não declara ausência absoluta de vulnerabilidades.

[0.9.3]: https://github.com/alrigroup/alrios/compare/v0.9.2...v0.9.3
[0.9.2]: https://github.com/alrigroup/alrios/compare/v0.9.1...v0.9.2
