# 📘 ALRIOS Core Architecture & Technical Reference Manual
**Documento Canônico:** `DOCS.md`  
**Entidade:** ALRI Development (departamento de desenvolvimento da holding alrigroup.com)  
**Classificação:** Livro Técnico de Engenharia (Padrão Corporativo)  
**Versão Alvo:** `v0.2.04-security-hardened`  
**Norma de Conformidade:** C11 (`-std=c11 -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -D_POSIX_C_SOURCE=200809L`)  

---

## 1. Visão Geral da Arquitetura

O **ALRIOS** é uma plataforma operacional nativa em espaço de usuário (*user-space sovereign kernel*), Hardware Abstraction Layer (HAL) e orquestrador de microsserviços escrito em padrão estrito ISO C11. Ele provê isolamento, ciclo de vida e supervisão resiliente para a infraestrutura soberana da holding ALRI (`arws`, `ardb`, `arcdn`, `arwe`, `arauth`).

```
                              ┌─────────────────────────────────────────┐
                              │      Supervisor & Daemon Manager        │
                              │           (arcore / ctl)                │
                              └────────────────────┬────────────────────┘
                                                   │ IPC Frame Protocol (Port 9500 / 9600)
                              ┌────────────────────▼────────────────────┐
                              │               HAL Unificado             │
                              │                (aros_hal.h)             │
                              └──────────┬───────────────────┬──────────┘
                                         │                   │
                     ┌───────────────────▼───────┐   ┌───────▼───────────────────┐
                     │     POSIX Linux Engine    │   │     Win32 Windows Engine  │
                     │  (process, socket, sync)  │   │  (process, socket, sync)  │
                     └───────────────────────────┘   └───────────────────────────┘
```

### Princípios Arquiteturais Invioláveis:
1. **Zero Runtime Disk Lookups**: Desacoplamento entre empacotamento (`.arapp`) e execução via staging isolado e cache em memória RAM.
2. **Hardware Abstraction Layer (HAL)**: Nenhuma chamada direta ao sistema operacional (`fork`, `execvp`, `CreateProcessW`, `pthread_create`, `socket`) fora do escopo de `src/ALRIOS/arkernel/os/`. Toda invocação transita por `aros_hal.h`.
3. **Comunicação Segura Zero-Trust**: Todos os descritores de sockets IPC e de controle impõem timeouts simétricos mandatórios (`SO_RCVTIMEO` e `SO_SNDTIMEO`), fechamento atômico `FD_CLOEXEC` e validação estrita de frames binários.
4. **Alocação de Memória Segura (Zero-Leak Discipline)**: Proibição estrita de realocação direta (`ptr = realloc(ptr, ...)`). Toda alocação dinâmica exige ponteiro intermediário temporário e liberação em profundidade em caso de falha de memória do kernel.
5. **Eliminação de Primitivas Inseguras**: Proibição terminante de `sprintf`, `strcpy`, `strcat`, `gets` ou `scanf`. Apenas primitivas delimitadas (`snprintf`) ou chamadas diretas com vetor de argumentos (`execvp`) são aceitas.

---

## 2. Hardware Abstraction Layer (ARKernel HAL)

O ARKernel abstrai integralmente as diferenças de ABI e chamadas de sistema entre plataformas POSIX (Linux) e Windows NT (Win32), expondo uma API C11 consistente.

```
src/ALRIOS/arkernel/
├── include/
│   ├── ar_ipc.h              # Protocolo de quadros e transporte IPC
│   ├── ar_kernel.h           # Estruturas de controle de processos e subsistemas
│   └── ar_svc.h              # Definições de supervisor de serviços e daemons
├── os/
│   ├── include/
│   │   └── aros_hal.h        # Declarações públicas unificadas da HAL
│   ├── linux/                # Implementação nativa Linux (POSIX, epoll/poll, pthreads)
│   │   ├── file.c, fs.c, memory.c, module.c, poll.c, process.c, socket.c, ssl.c, thread.c, time.c
│   └── windows/              # Implementação nativa Windows (Win32, IOCP/select, Threads Win32)
│       ├── file.c, fs.c, memory.c, module.c, poll.c, process.c, socket.c, ssl.c, thread.c, time.c
└── svc/
    ├── svc_manager.c         # Gerenciamento de tabela de serviços
    ├── svc_supervisor.c      # Loop de supervisão com reinicialização exponencial
    ├── svc_health.c          # Verificação de liveness/healthcheck de processos
    └── arkernel_main.c       # Ponto de entrada do kernel embarcado
```

### 2.1 Subsistemas da HAL
* **Processos (`ar_process_*`)**: Inicialização em processo isolado, monitoramento sem bloqueio (`waitpid` com `WNOHANG`), encerramento gracioso via sinais (`SIGTERM` -> `SIGKILL`) e controle de grupos de processos (*Job Objects* no Windows e *Process Groups* no Linux).
* **Sockets & Rede (`ar_socket_*`)**: Sockets não-bloqueantes (`O_NONBLOCK`), desativação de buffer de Nagle (`TCP_NODELAY`), reutilização imediata de portas (`SO_REUSEADDR`) e imunidade contra DoS/Deadlock via configuração estrita de `SO_RCVTIMEO` e `SO_SNDTIMEO`.
* **Sincronização & Threads (`ar_thread_*`, `ar_mutex_*`, `ar_cond_*`)**: Primitivas de exclusão mútua e variáveis de condição multiplataforma substituindo espera ativa (*busy-wait*).
* **Módulos Dinâmicos & SSL (`ar_module_*`, `ar_ssl_*`)**: Abstração para carga de bibliotecas dinâmicas (`dlopen`/`LoadLibraryA`) e canal TLS via OpenSSL.

---

## 3. Barramento de Comunicação Inter-Processos (IPC)

O barramento IPC do ALRIOS orquestra toda a comunicação entre o daemon supervisor (`arcore`), as ferramentas de linha de comando (`alrios`, `arpm`) e os serviços nativos.

### 3.1 Protocolo de Enquadramento Binário (5-Byte Framing)
Cada quadro IPC transmitido pelo barramento obedece estritamente ao cabeçalho canônico binário:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Payload Length (BE)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Frame Type   |             Payload Data (...)                |
+-+-+-+-+-+-+-+-+                                               |
|                             ...                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

* **Bytes 0–3:** Tamanho total do payload em *Big-Endian* (`uint32_t`).
* **Byte 4:** Tipo de comando / Opcode (`uint8_t`):
  * `0x01` (`IPC_PING` / `IPC_CTL_PING`): Verificação de pulso de vida.
  * `0x02` (`IPC_PONG`): Resposta de pulso de vida.
  * `0x03` (`IPC_RESPONSE`): Resposta operacional de comando.
  * `0x04` (`IPC_ERROR`): Resposta de erro estruturado.
  * `0x10` (`IPC_CTL_START`): Solicitação de boot de serviço.
  * `0x11` (`IPC_CTL_STOP`): Solicitação de parada de serviço.
  * `0x12` (`IPC_CTL_RELOAD`): Recarga a quente sem perda de estado.
  * `0x13` (`IPC_CTL_STATUS`): Telemetria e tabela de processos.
  * `0x14` (`IPC_CTL_LIST`): Inventário de pacotes instalados.
  * `0x15` (`IPC_CTL_POWER_OFF`): Desligamento gracioso do supervisor.
* **Bytes 5 em diante:** Carga útil estruturada (dados seriais C ou JSON seguro).

---

## 4. Supervisor de Serviços & Tolerância a Falhas

O daemon supervisor (`arcore`) monitora a integridade de todos os daemons em execução.

### 4.1 Máquina de Estados Finitos (FSM)
Cada serviço supervisionado transita através dos seguintes estados determinísticos:
```
  [ UNREGISTERED ] ──── Carregar Manifesto ────▶ [ STOPPED ]
                                                    │
                                             power on / start
                                                    │
                                                    ▼
  [ RECOVERING ] ◀──── Falha do Processo ──── [ RUNNING ]
        │
   Backoff Exponencial
        │
        ▼
   (Reinicialização Automática)
```

### 4.2 Verificação de Saúde e Política de Descarte
1. **Ciclo de Sondagem:** A cada 500ms, o subsistema `svc_health` verifica a vivacidade dos processos ativos via `waitpid(WNOHANG)`.
2. **Backoff de Recuperação:** Daemons que sofrem falha abrupta são reinicializados de forma automática com escalonamento de atraso (500ms, 1000ms, 2000ms, até o limite de tolerância).
3. **Prevenção de Deadlocks de Mutex:** Todo acesso concorrente segue estritamente a hierarquia global de travamento:
   * Nível 1: `g_supervisor_lock` (Escopo global do ciclo de vida)
   * Nível 2: `g_service_table_lock` (Tabela de estado de serviços)
   * Nível 3: `g_ipc_client_lock` (Transmissão sobre sockets IPC)

---

## 5. Subsistema de Empacotamento `.arapp` & Ferramentas

O ecossistema ALRIOS utiliza pacotes selados `.arapp` gerados pela ferramenta `armake`.

### 5.1 Especificação de Formato `.arapp`
* **Magic Header:** Os primeiros 16 bytes do arquivo contêm a assinatura `ALRIGROUP@APP` seguida da versão binária `0x0001` (little-endian).
* **Corpo do Arquivo:** Arquivo estruturado compatível com ZIP (armazenamento puro `STORED` ou deflated) contendo o manifesto de aplicação e todos os binários e recursos estáticos.
* **Manifesto (`*.arappmake`):** Deve conter o cabeçalho de magic bytes `ALRIGROUP@APPMAKE` seguido da declaração JSON:
  ```json
  {
    "app": "arws",
    "version": "1.0.0",
    "name": "ALRI Web Server Gateway",
    "executable": "bin/arws",
    "autostart": true,
    "restart_on_crash": true,
    "env": {
      "PORT": "8080"
    }
  }
  ```

### 5.2 Ferramentas Oficiais
* **`alrios`**: Interface de comando unificada para governança do kernel (`power on`, `power off`, `status`, `start`, `stop`, `list`).
* **`arpm`**: Gerenciador de pacotes soberano para download, validação de hash SHA-256 e extração segura em staging sem invocação de shell.
* **`armake`**: Utilitário de empacotamento, extração de contêineres e geração de snapshots diferenciais de arquivos.

---

## 6. Portões de Qualidade e Governança de Código

Todas as compilações e contribuições para o ALRIOS devem ser auditadas sob critérios corporativos de tolerância zero:
1. **Compilação Estrita C11:** Zero warnings sob as flags `-std=c11 -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -D_POSIX_C_SOURCE=200809L`.
2. **Propriedade Intelectual Regulamentar:** Cabeçalho mandatório de copyright em todos os arquivos:
   ```c
   /* ====================================================================
    * Copyright (c) 2026 ALRI Development. All rights reserved.
    * Proprietary and confidential. Unauthorized copying is prohibited.
    * ==================================================================== */
   ```
3. **Memory Safety Absoluto:** Validação rigorosa sob Valgrind Memcheck / ASan com zero bytes vazados.
