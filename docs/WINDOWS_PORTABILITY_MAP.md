# Mapeamento Completo de Portabilidade Linux -> Windows (ALRIOS)

**Escopo:** Auditoria arquitetural e de código-fonte estática em `/home/alexsanderalri/Documentos/ALRIGROUP/ALRIOS Ecosystem/alrios`.  
**Objetivo:** Mapear todos os componentes, chamadas de sistema, pressupostos de compilação, workflows e pipelines que atualmente tornam o ALRIOS restrito a Linux (*Linux-only*), definindo as adaptações necessárias para execução nativa e segura em Windows.

---

## 1. Conclusão Executiva

O ecossistema ALRIOS possui uma fundação parcial para Win32 no HAL (`src/platform/hal/windows/`), mas **não oferece suporte Windows funcional e seguro equivalente ao Linux no momento**.

1. **Substituição por Stubs Nulos**: Módulos críticos do sistema (IPC v2, Guest Channel, Sandboxing, Sovereign Loader, Atomic Swap e Socket Handover) são deliberadamente substituídos no Windows por stubs que retornam erro imediato (`-1`, `_UNSUPPORTED` ou `_PLATFORM`).
2. **Bloqueio de Compilação MSVC**: Diversas bibliotecas (`arsdk_app`, `arsensitive_mem`, `arvault_store`, `arindex`, `ardeploy`, daemons) incluem e utilizam incondicionalmente headers e chamadas POSIX (`unistd.h`, `fcntl.h`, `poll.h`, `pthread.h`, `madvise`, `openat`, `O_NOFOLLOW`, `O_CLOEXEC`, `FICLONE`).
3. **Ausência de Equivalentes de Segurança**: Isolamento com Seccomp e Landlock, execução de executáveis assinados em memória via `memfd_create` selado (`F_ADD_SEALS`) e monitoramento PSI (`/proc/pressure/memory`) não possuem correspondente implementado no Windows.
4. **Instalador e Distribuição**: O instalador Windows tenta baixar `alrios-windows-x64-installer.zip`, mas o workflow oficial do GitHub Actions compila e publica estritamente `alrios-installer-linux-x64`.

---

## 2. Matriz de Bloqueios e Incompatibilidades Diretas

### 2.1 Código POSIX em Targets Globais (Bloqueadores de Build MSVC)

| Componente | Arquivos e Linhas | Causa POSIX / Linux-only | Adaptação Necessária para Windows |
|---|---|---|---|
| **App SDK** (`arsdk_app`) | `src/libraries/sdk/app.c:21-30, 105-116, 526-647, 742` | Inclui `unistd.h`, `fcntl.h`, `poll.h`, sockets POSIX; invoca `fcntl(F_GET_SEALS)`, `poll()`, `clock_gettime()`. | Separar lógica de aplicação em núcleo portável e transporte de segredos via `HANDLE` / Named Pipe autenticado em vez de descritor `int`. |
| **Memória Sensível** (`arsensitive_mem`) | `src/libraries/memory/sensitive.c:19-24, 33-44, 46-66, 73-96` | Usa `pthread_mutex_t`, `PTHREAD_MUTEX_INITIALIZER`, `unistd.h` e `madvise(MADV_DONTDUMP)`. | Migrar sincronização para HAL ou `SRWLOCK`; usar `VirtualLock` para prevenir swap e configurar Crash Dumps via WER. |
| **Vault Store** (`arvault_store`) | `src/libraries/vault/store.c:19-32, 1081-1173, 1229-1241` | Depende de `memfd` para segredos; caminho de persistência Windows usa `fopen` sem ACLs equivalentes a `0600`. | Usar `CreateFileMappingW` com DACL restritiva ou Named Pipe para IPC; aplicar DACLs explícitas para usuário/serviço no armazenamento. |
| **Index & Layering** (`arindex`, `ardeploy`) | `src/libraries/package/index.c:13-19, 436-446`<br>`src/libraries/package/index_writer.c:12-22, 302-322`<br>`src/libraries/deploy/layering.c:16-31, 138-174, 314-530` | Uso de descritores relativos: `openat()`, `O_NOFOLLOW`, `O_CLOEXEC`, `fchmodat()`, `mkstemp()`, `dirent.h`, `ioctl(FICLONE)`. | Implementar operações de filesystem baseadas em `HANDLE` via `CreateFileW(FILE_FLAG_OPEN_REPARSE_POINT)`, validação de reparse points e ACLs. |
| **Auditoria e Daemons** | `src/libraries/audit/hash_chain.c:15-23, 323-355`<br>`src/daemons/ar_auditd/main.c:18-23`<br>`src/daemons/ar_chronod/main.c:16-23`<br>`src/daemons/ar_cored/main.c:20-25`<br>`src/daemons/ar_eventd/main.c:17-28` | Uso de locks POSIX `fcntl()`, descritores Unix, `AF_UNIX`, sinais POSIX e `poll()`. | Portar locking para `LockFileEx`, IPC para Named Pipes locais autenticados e lifecycle para Service Control Manager (SCM). |
| **Daemon de Recursos** (`ar_resourced`) | `src/daemons/ar_resourced/main.c:111-116`<br>`src/libraries/resources/psi.c:307-313` | Requer leitura estrita de `/proc/pressure/memory` (Linux PSI). Falha imediatamente fora do Linux. | Abstrair métrica de pressão de memória: no Windows, utilizar `GlobalMemoryStatusEx`, `GetPerformanceInfo` e contadores de Job Object. |
| **Merge do SDK** | `src/CMakeLists.txt:346-405` | Script de empacotamento chama `ar -x`, `ar qc` e `ranlib`, incompatíveis com o `lib.exe` do MSVC. | Utilizar branch específica para MSVC chamando `lib.exe` ou exportar targets estáticos sem unificação destrutiva de `.a`. |

---

## 3. Stubs Nulos Existentes no Windows (Críticos para Runtime)

Quando o CMake é configurado fora do Linux, os seguintes arquivos são selecionados:

| Módulo | Arquivo de Stub Windows | Impacto no Funcionamento |
|---|---|---|
| **Canal de IPC v2** | `src/libraries/ipc/ipc_channel_unsupported.c` | Todas as operações de leitura/escrita retornam `-1`. Comunicação entre componentes inoperante. |
| **Multiplexador IPC** | `src/libraries/ipc/epoll_multiplexer_unsupported.c` | Espera de eventos assíncronos não suportada (depende de `epoll`). |
| **Memloader** | `src/libraries/loader/memloader_unsupported.c` | Não executa código a partir de memória selada (`memfd`). |
| **Sovereign Loader** | `src/libraries/loader/sovereign_loader_unsupported.c` | Staging e verificação de integridade de `.arapp` falham com erro de plataforma. |
| **Montagens de Sandbox** | `src/libraries/sandbox/mounts_unsupported.c` | Isolamento de diretórios, namespaces privados e chroot inoperantes. |
| **Isolamento de Processo** | *Target ausente no Windows* | Seccomp, cgroups v2 e namespaces Linux não existem no Windows. |
| **Devmode Jail** | `src/libraries/sandbox/devmode_jail_unsupported.c` | Ambiente controlado para desenvolvimento falha ao iniciar. |
| **Passagem de Descritor** | `src/libraries/deploy/fd_pass_unsupported.c` | Não transfere sockets abertos entre processos (sem `SCM_RIGHTS`). |
| **Socket Handover** | `src/libraries/supervisor/socket_handover_unsupported.c` | Atualização zero-downtime falha ao transferir o listener do socket. |
| **Drain de Conexões** | `src/libraries/deploy/drain_unsupported.c` | Não realiza esvaziamento gracioso de conexões ativas. |
| **Atomic Swap de Versão** | `src/libraries/deploy/atomic_swap_unsupported.c` | Não existe ativação atômica de nova release via symlink atômico. |
| **Guest Channel & Router** | `src/libraries/ipc/guest_channel_unsupported.c`<br>`src/libraries/supervisor/guest_router_unsupported.c` | Roteamento de mensagens host <-> guest e controle do supervisor inoperantes. |
| **Hooks de Ciclo de Vida** | `src/libraries/hooks/hooks_unsupported.c` | Hooks pós-instalação e pré-ativação desativados. |

---

## 4. Auditoria de Falhas no HAL Windows Atual (`src/platform/hal/windows/`)

Embora exista código Win32 implementado, identificamos vulnerabilidades e limitações técnicas:

1. **Uso Indiscriminado de APIs ANSI e Buffers `MAX_PATH`**:
   - `file.c`, `fs.c`, `module.c`, `process.c`: utilizam `CreateDirectoryA`, `CreateFileA`, `LoadLibraryA`, `CreateProcessA` e buffers estáticos de 260 bytes.
   - *Risco:* Corrupção de caminhos Unicode (acentos, diretórios internacionais), estouro de buffer e falhas em caminhos longos (> 260 caracteres).
   - *Correção:* Migrar integralmente para APIs Unicode (`*W`), conversão estrita de UTF-8 via `MultiByteToWideChar`, e alocação dinâmica.

2. **Truncamento de Handles de Socket (`SOCKET` -> `int`)**:
   - `src/platform/hal/windows/socket.c`: Faz cast de `SOCKET` (que é `UINT_PTR` de 64 bits no Windows x64) para `int` de 32 bits (`(int)s`, `(SOCKET)fd`).
   - *Risco:* Truncamento do handle, corrupção de descritores, I/O em conexões trocadas e comportamento indefinido.
   - *Correção:* Criar tipo abstrato `ar_socket_handle_t` mantendo integridade de `uintptr_t`.

3. **Multiplexação por `select()` com `FD_SETSIZE`**:
   - `src/platform/hal/windows/poll.c`: Emula `poll` utilizando `select()`.
   - *Risco:* Limite máximo rígido de 64 sockets por conjunto (`FD_SETSIZE`), complexidade $O(N)$ e ausência de escalabilidade.
   - *Correção:* Reescrever o backend de I/O assíncrono para I/O Completion Ports (IOCP).

4. **Sinais e Encerramento Forçado de Processos**:
   - `process.c`: Tanto `AR_PROCESS_SIGNAL_TERM` quanto `KILL` executam `TerminateProcess()`. Não há protocolo de encerramento gracioso.
   - `HUP` não é suportado, quebrando o mecanismo de recarregamento sem reinicialização.
   - Janela de corrida na criação de processos associados a Job Objects (devem ser criados suspensos com `CREATE_SUSPENDED`, vinculados ao Job Object e somente então resumidos).

5. **Criação de Threads via `CreateThread`**:
   - `thread.c`: Usa `CreateThread` diretamente em vez de `_beginthreadex`, o que pode causar vazamento de recursos na CRT C/C++.

---

## 5. Scripts, Infraestrutura e Instalação

1. **Scripts `.bat` Inseguros**:
   - `stop.bat`: Executa `taskkill /f /im node.exe`, `python.exe` e encerra PIDs vinculados a portas locais indiscriminadamente, podendo derrubar serviços de terceiros do desenvolvedor.
   - `build.bat`: Força inclusão de `C:\Program Files\Go\bin`, apaga `db.sqlite` e dados locais antes da compilação e não verifica pré-requisitos como OpenSSL.
2. **Download Inseguro no Instalador Win32**:
   - `src/commands/arinstall/installer_core.c`: Utiliza `URLDownloadToFileA` sem conferência prévia de hash SHA-512 ou assinatura digital do manifesto.
   - Extração de ZIP (`extract_zip_to_dir`) não valida retornos individuais e não possui defesa explícita contra *Zip Slip* em caminhos relativos.
3. **Ausência de Serviço Windows**:
   - O projeto possui `alrios.service` (Systemd para Linux), mas não implementa binário ou registro de Windows Service no Service Control Manager (SCM).

---

## 6. Roadmap Sequencial para Implementação Windows

### Fase 0: Arquitetura e Contratos de Plataforma
- [ ] Definir perfis formais de suporte: Developer CLI vs. Runtime Completo vs. Windows Service.
- [ ] Criar tipos opacos para handles no HAL (`ar_file_t`, `ar_socket_t`, `ar_proc_t`), eliminando o uso de `int` como identificador universal.
- [ ] Especificar o modelo de entrega de segredos e execução no Windows (substituindo a semântica de `memfd` por staging autenticado em disco com DACL restrita).

### Fase 1: Portabilidade de Build (MSVC)
- [ ] Dividir bibliotecas com código POSIX (`arsdk_app`, `arsensitive_mem`, `arvault_store`, `arindex`, `ardeploy`) em núcleo comum e backends `_linux.c` / `_windows.c`.
- [ ] Substituir o script de concatenação de arquivos estáticos (`ar`) no CMake por compatibilidade nativa com `lib.exe`.
- [ ] Criar presets do CMake (`windows-msvc-release`) configurando include paths e bibliotecas do OpenSSL via `vcpkg` ou diretórios determinísticos.

### Fase 2: HAL Windows Maduro e Seguro
- [ ] Migrar todas as funções de arquivo, diretório, módulo e processo no HAL para variantes Unicode (`W`) com paths UTF-8 normalizados.
- [ ] Eliminar casts de `SOCKET` para `int`.
- [ ] Criar backend IOCP para substituir a emulação via `select()`.
- [ ] Implementar criação atômica e segura de processos associados a Job Objects com token restrito.

### Fase 3: IPC, Identidade Local e Isolamento
- [ ] Implementar transporte IPC sobre Named Pipes locais restritos (`PIPE_REJECT_REMOTE_CLIENTS`) com verificação do SID/PID do chamador.
- [ ] Substituir `SCM_RIGHTS` por transferência controlada de sockets via `WSADuplicateSocket` e handles via `DuplicateHandle`.
- [ ] Implementar `arsandbox_windows` utilizando AppContainer, tokens restritos, Job Objects e políticas de mitigação de processos.

### Fase 4: Deploy, Serviço e Instalador
- [ ] Implementar ativação atômica de versões no Windows usando arquivos ponteiro com `ReplaceFileW`.
- [ ] Criar host de serviço Windows integrado ao SCM.
- [ ] Atualizar o instalador `arinstall` para validar assinaturas criptográficas dos pacotes baixados e extrair arquivos com proteção de ACLs.
- [ ] Corrigir scripts `.bat` para operar exclusivamente sobre os processos da instância do ALRIOS via Job Objects.

### Fase 5: CI/CD e Validação
- [ ] Configurar runner `windows-latest` obrigatório no GitHub Actions.
- [ ] Habilitar compilação MSVC com `/W4 /WX`.
- [ ] Executar testes automatizados em ambiente Windows limpo antes de gerar qualquer release pública.
