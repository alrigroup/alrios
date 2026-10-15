# Changelog

## [0.9.3] — 2026-10-15

### Linux-only preview

- Publicação temporariamente limitada ao instalador Linux x64.
- Pipeline de release simplificada para não bloquear a pré-release enquanto a portabilidade Windows é corrigida e validada.
- Arquivos gerados, caches locais e instruções exclusivas para agentes removidos do checkout.

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
- Esta versão é pré-release e não declara ausência absoluta de vulnerabilidades.

[0.9.3]: https://github.com/alrigroup/alrios/compare/v0.9.2...v0.9.3
[0.9.2]: https://github.com/alrigroup/alrios/compare/v0.9.1...v0.9.2
