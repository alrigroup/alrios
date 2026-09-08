/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ar_ipc.h"
#include "aros_hal.h"
#include "arpm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

static void get_base_dir(char *buf, int size) {
#ifdef _WIN32
  GetModuleFileNameA(NULL, buf, size);
  char *p = strrchr(buf, '\\');
  if (p)
    *p = '\0';
#else
  char link[32] = "/proc/self/exe";
  ssize_t len = readlink(link, buf, size - 1);
  if (len < 0) {
    strncpy(buf, ".", size);
    return;
  }
  buf[len] = '\0';
  char *p = strrchr(buf, '/');
  if (p)
    *p = '\0';
#endif
}

static int file_exists(const char *path) {
#ifdef _WIN32
  return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
  return access(path, F_OK) == 0;
#endif
}

static int ctl_connect(void) {
  return ar_ipc_client_connect("127.0.0.1", AR_CTL_PORT);
}

static int admin_connect(void) {
  return ar_ipc_client_connect("127.0.0.1", AR_IPC_DEFAULT_PORT);
}

static void get_apps_dir(char *buf, int size) {
  get_base_dir(buf, size);
#ifdef _WIN32
  strncat(buf, "\\apps", size - strlen(buf) - 1);
#else
  strncat(buf, "/apps", size - strlen(buf) - 1);
#endif
}

typedef struct {
  const char *name;
  const char *desc;
  const char *cmds;
} KnownAppInfo;

static const KnownAppInfo g_known_apps[] = {
  {"arws", "Gateway HTTP/HTTPS, Reverse Proxy e Load Balancer", "routes, status, cfg reload, upstream list, maintenance, ping"},
  {"arauth", "Motor Soberano de Identidade e Autenticacao Bancaria", "status, login, user add, user passwd, session verify, audit verify"},
  {"ardb", "Guardiao de Banco de Dados Soberano e Firewall SQL", "status, auth login, user add, app add, group create, audit verify"},
  {"arcdn", "Motor de Distribuicao Estatica e Streaming de Midia", "status, routes, list, add <path> <file>, del <path>, ping"},
  {"arenterprise", "ALRI Enterprise Suite Daemon e Integracao Corporativa", "status, ping"},
  {"arwn", "Runtime Nativo Web e Servidor de Containers .arweb", "status, routes, ping"},
  {"ardcbot", "Bot de Integracao Discord e Automacao da Comunidade", "status, plugins, reload, ping"},
  {"detroit.web", "Portal e Aplicacao Web Detroit City Roleplay", "status, routes, ping"},
  {"fourtech.web", "Plataforma Institucional FourTech", "status, routes, ping"},
  {"alrigroup.web", "Portal Oficial ALRI GROUP", "status, routes, ping"},
  {"omniroute", "Roteador de Modelos de Inteligencia Artificial", "status, models, routes, ping"},
  {NULL, NULL, NULL}
};

static int is_app_installed(const char *app) {
  char apps_dir[1024];
  get_apps_dir(apps_dir, sizeof(apps_dir));
  char path[1024];
#ifdef _WIN32
  snprintf(path, sizeof(path), "%s\\%s.arapp", apps_dir, app);
  if (file_exists(path)) return 1;
  snprintf(path, sizeof(path), "%s\\%s", apps_dir, app);
  if (file_exists(path)) return 1;
  snprintf(path, sizeof(path), "src\\apps\\%s", app);
  if (file_exists(path)) return 1;
#else
  snprintf(path, sizeof(path), "%s/%s.arapp", apps_dir, app);
  if (file_exists(path)) return 1;
  snprintf(path, sizeof(path), "%s/%s", apps_dir, app);
  if (file_exists(path)) return 1;
  snprintf(path, sizeof(path), "src/apps/%s", app);
  if (file_exists(path)) return 1;
#endif
  return 0;
}

static void print_installed_apps(void) {
  char apps_dir[1024];
  get_apps_dir(apps_dir, sizeof(apps_dir));

  char installed_names[64][64];
  int installed_count = 0;

#ifdef _WIN32
  char pattern[1024];
  snprintf(pattern, sizeof(pattern), "%s\\*", apps_dir);
  WIN32_FIND_DATAA ffd;
  HANDLE hFind = FindFirstFileA(pattern, &ffd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
      char name[64];
      strncpy(name, ffd.cFileName, sizeof(name) - 1);
      name[sizeof(name) - 1] = '\0';
      char *dot = strstr(name, ".arapp");
      if (dot && strcmp(dot, ".arapp") == 0) *dot = '\0';
      else if (strstr(name, ".hash") != NULL) continue;

      int exists = 0;
      for (int i = 0; i < installed_count; i++) {
        if (strcmp(installed_names[i], name) == 0) { exists = 1; break; }
      }
      if (!exists && installed_count < 64) {
        strncpy(installed_names[installed_count++], name, 63);
      }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
  }
#else
  DIR *d = opendir(apps_dir);
  if (d) {
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
      if (entry->d_name[0] == '.') continue;
      char name[64];
      strncpy(name, entry->d_name, sizeof(name) - 1);
      name[sizeof(name) - 1] = '\0';
      char *dot = strstr(name, ".arapp");
      if (dot && strcmp(dot, ".arapp") == 0) *dot = '\0';
      else if (strstr(name, ".hash") != NULL) continue;

      int exists = 0;
      for (int i = 0; i < installed_count; i++) {
        if (strcmp(installed_names[i], name) == 0) { exists = 1; break; }
      }
      if (!exists && installed_count < 64) {
        strncpy(installed_names[installed_count++], name, 63);
      }
    }
    closedir(d);
  }
#endif

  printf("Aplicativos Instalados & Comandos Dinamicos:\n");
  printf("  alrios <app> [comando]           (executa comando no aplicativo via gateway)\n\n");

  if (installed_count == 0) {
    printf("  (Nenhum aplicativo instalado em %s. Use 'alrios arpm install <app>' para instalar)\n", apps_dir);
    return;
  }

  for (int i = 0; i < installed_count; i++) {
    const char *aname = installed_names[i];
    const char *desc = "Aplicativo Soberano ALRIOS";
    const char *cmds = "status, ping, help";

    for (int k = 0; g_known_apps[k].name != NULL; k++) {
      if (strcmp(g_known_apps[k].name, aname) == 0) {
        desc = g_known_apps[k].desc;
        cmds = g_known_apps[k].cmds;
        break;
      }
    }

    char status_buf[64] = "OFFLINE";
    int fd = ctl_connect();
    if (fd >= 0) {
      ar_socket_set_recv_timeout(fd, 300);
      if (ar_ipc_send_frame(fd, IPC_CTL_STATUS, aname, (uint32_t)strlen(aname) + 1) == 0) {
        unsigned char buf[64];
        int rtype;
        uint32_t rlen = sizeof(buf);
        if (ar_ipc_recv_frame(fd, &rtype, buf, &rlen) == 0 && rtype == IPC_RESPONSE) {
          buf[rlen < sizeof(buf) ? rlen : sizeof(buf) - 1] = '\0';
          strncpy(status_buf, (char *)buf, sizeof(status_buf) - 1);
        }
      }
      ar_socket_close(fd);
    }

    const char *status_color = strcmp(status_buf, "RUNNING") == 0 ? "\033[1;32m" : "\033[1;33m";
    printf("  \033[1;36m• %s\033[0m [%s%s\033[0m]\n", aname, status_color, status_buf);
    printf("    %s\n", desc);
    printf("    \033[0;37mComandos:\033[0m %s\n\n", cmds);
  }
  printf("  (Dica: use 'alrios <app> help' para ver o manual completo de qualquer app)\n");
}

static void print_usage(void) {
  printf("ALRIOS CLI v0.2.01\n\n");
  printf("Uso:\n");
  printf("  alrios power on|off|reload       (gerencia ciclo de vida do kernel arcore)\n");
  printf("  alrios status                    (alias: list - status dos daemons)\n");
  printf("  alrios list                      (lista processos e daemons do kernel)\n");
  printf("  alrios start <app>               (inicia um aplicativo)\n");
  printf("  alrios stop <app>                (interrompe um aplicativo)\n");
  printf("  alrios restart <app>             (reinicia um aplicativo)\n");
  printf("  alrios start add|del <app>       (configura inicializacao automatica)\n");
  printf("  alrios arpm <comando>            (gerenciador de pacotes soberanos .arapp)\n");
  printf("  alrios fullupdate [--force]      (rebuild incremental de apps e reload)\n");
  printf("  alrios update <alvo>             (alvo: all|alrios|armake|arinstall)\n");
  printf("  alrios build -p <SRC> [-o <OUT>] (compila diretorio de app -> .arapp)\n");
  printf("  alrios refresh                   (atualiza lista de apps sem reiniciar)\n\n");
  print_installed_apps();
}

static int send_and_print(int fd, int type, const char *payload) {
  uint32_t plen = payload ? (uint32_t)strlen(payload) : 0;
  if (ar_ipc_send_frame(fd, type, payload, plen) < 0)
    return -1;

  ar_socket_set_recv_timeout(fd, 10000);

  unsigned char buf[AR_IPC_BUF_SIZE];
  int rtype;
  uint32_t rlen = sizeof(buf);
  if (ar_ipc_recv_frame(fd, &rtype, buf, &rlen) < 0)
    return -1;
  buf[rlen] = '\0';
  printf("%s\n", buf);
  return (rtype == IPC_RESPONSE || rtype == IPC_QUERY_RESP || rtype == IPC_ACK)
             ? 0
             : 1;
}

static int run_ctl(int type, const char *payload) {
  int fd = ctl_connect();
  if (fd < 0) {
    printf("ALRIOS is not running (channel 9600 unavailable)\n");
    return 1;
  }
  int rc = send_and_print(fd, type, payload);
  ar_socket_close(fd);
  return (rc < 0) ? 1 : rc;
}

static int spawn_arcore_detached(const char *exe) {
#ifdef _WIN32
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "\"%s\"", exe);
  STARTUPINFOA si = {0};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {0};
  BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                           DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, NULL,
                           NULL, &si, &pi);
  if (!ok)
    return -1;
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return (int)pi.dwProcessId;
#else
  pid_t pid = fork();
  if (pid < 0)
    return -1;
  if (pid > 0)
    return pid;
  setsid();
  freopen("/dev/null", "r", stdin);
  freopen("/dev/null", "w", stdout);
  freopen("/dev/null", "w", stderr);
  execl(exe, exe, NULL);
  _exit(127);
#endif
}

static int cmd_power_on(void) {
  int fd = ctl_connect();
  if (fd >= 0) {
    ar_socket_set_recv_timeout(fd, 800);
    if (ar_ipc_send_frame(fd, IPC_CTL_PING, NULL, 0) == 0) {
      unsigned char buf[8];
      uint32_t rlen = sizeof(buf);
      int rtype;
      if (ar_ipc_recv_frame(fd, &rtype, buf, &rlen) == 0) {
        send_and_print(fd, IPC_CTL_POWER_RELOAD, NULL);
        ar_socket_close(fd);
        printf("ALRIOS daemons reloaded\n");
        return 0;
      }
    }
    ar_socket_close(fd);
  }

  char exe[1024];
  get_base_dir(exe, sizeof(exe));
#ifdef _WIN32
  strncat(exe, "\\arcore.exe", sizeof(exe) - strlen(exe) - 1);
#else
  strncat(exe, "/arcore", sizeof(exe) - strlen(exe) - 1);
#endif

  int pid = spawn_arcore_detached(exe);
  if (pid <= 0) {
    printf("Failed to start arcore (%s)\n", exe);
    return 1;
  }
  printf("arcore started (pid %d)\n", pid);
  return 0;
}

static void autostart_path(char *buf, int size) {
  get_base_dir(buf, size);
#ifdef _WIN32
  strncat(buf, "\\autostart.cfg", size - strlen(buf) - 1);
#else
  strncat(buf, "/autostart.cfg", size - strlen(buf) - 1);
#endif
}

static int autostart_has(const char *path, const char *name) {
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  char line[256];
  int found = 0;
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == '#' || !*p)
      continue;
    char *end = p + strlen(p);
    while (end > p && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' ||
                       end[-1] == '\t'))
      end--;
    *end = '\0';
    if (strcmp(p, name) == 0) {
      found = 1;
      break;
    }
  }
  fclose(f);
  return found;
}

static int cmd_autostart_add(const char *app) {
  char path[1024];
  autostart_path(path, sizeof(path));
  if (autostart_has(path, app)) {
    printf("autostart: %s is already listed\n", app);
  } else {
    FILE *f = fopen(path, "a");
    if (!f) {
      printf("Failed to open %s\n", path);
      return 1;
    }
    fprintf(f, "%s\n", app);
    fclose(f);
    printf("autostart: %s added\n", app);
  }
  /* Apply immediately if arcore is running */
  run_ctl(IPC_CTL_START, app);
  return 0;
}

static int cmd_autostart_del(const char *app) {
  char path[1024];
  autostart_path(path, sizeof(path));
  char tmp[4096];
  FILE *f = fopen(path, "r");
  if (!f) {
    printf("Failed to open %s\n", path);
    return 1;
  }
  size_t n = fread(tmp, 1, sizeof(tmp) - 1, f);
  tmp[n] = '\0';
  fclose(f);

  FILE *out = fopen(path, "w");
  if (!out) {
    printf("Failed to write %s\n", path);
    return 1;
  }
  char *line = tmp;
  while (line && *line) {
    char *nl = strchr(line, '\n');
    if (nl)
      *nl = '\0';
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    char *end = p + strlen(p);
    while (end > p && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' ||
                       end[-1] == '\t'))
      end--;
    *end = '\0';
    int skip = (*p && *p != '#' && strcmp(p, app) == 0);
    if (!skip)
      fprintf(out, "%s%s", line, nl ? "\n" : "");
    line = nl ? nl + 1 : NULL;
  }
  fclose(out);
  printf("autostart: %s removed\n", app);
  /* Apply immediately if arcore is running */
  run_ctl(IPC_CTL_STOP, app);
  return 0;
}

static int cmd_app_query(const char *app, const char *cmd) {
  int fd = admin_connect();
  if (fd < 0) {
    printf("\033[1;33m[INFO]\033[0m ARWS Gateway indisponivel (canal IPC 9500 inativo).\n");
    printf("Para iniciar o ecossistema ALRIOS: alrios power on\n");
    printf("Para iniciar o aplicativo: alrios start %s\n", app);
    return 1;
  }
  char payload[AR_IPC_BUF_SIZE];
  snprintf(payload, sizeof(payload), "%s\n%s", app, cmd);

  uint32_t plen = (uint32_t)strlen(payload);
  if (ar_ipc_send_frame(fd, IPC_QUERY, payload, plen) < 0) {
    ar_socket_close(fd);
    return 1;
  }

  ar_socket_set_recv_timeout(fd, 10000);
  unsigned char buf[AR_IPC_BUF_SIZE];
  int rtype;
  uint32_t rlen = sizeof(buf);
  if (ar_ipc_recv_frame(fd, &rtype, buf, &rlen) < 0) {
    ar_socket_close(fd);
    return 1;
  }
  buf[rlen < sizeof(buf) ? rlen : sizeof(buf) - 1] = '\0';
  ar_socket_close(fd);

  if (strstr((char *)buf, "target not found") != NULL) {
    printf("\033[1;33m[INFO]\033[0m O aplicativo '%s' esta instalado mas nao esta em execucao.\n", app);
    printf("Inicie com: alrios start %s\n", app);
    return 0;
  }

  printf("%s\n", buf);
  return (rtype == IPC_RESPONSE || rtype == IPC_QUERY_RESP || rtype == IPC_ACK) ? 0 : 1;
}

static int cmd_app_query_args(const char *app, int argc, char *argv[]) {
  char cmd[AR_IPC_BUF_SIZE];
  int off = 0;
  if (argc < 3) {
    snprintf(cmd, sizeof(cmd), "help");
    return cmd_app_query(app, cmd);
  }
  for (int i = 2; i < argc; i++) {
    if (i > 2 && off < (int)sizeof(cmd) - 1)
      cmd[off++] = ' ';
    int n = snprintf(cmd + off, sizeof(cmd) - off, "%s", argv[i]);
    if (n < 0)
      break;
    off += n;
    if (off >= (int)sizeof(cmd) - 1)
      break;
  }
  cmd[off] = '\0';
  return cmd_app_query(app, cmd);
}

#include <sys/stat.h>
#ifndef _WIN32
#include <dirent.h>
#endif

static int cmd_fullupdate(int argc, char **argv) {
  char base[1024], root[1024];
  get_base_dir(base, sizeof(base));
  strncpy(root, base, sizeof(root) - 1);
  root[sizeof(root) - 1] = '\0';
  char *sep = strrchr(root, '\\');
#ifndef _WIN32
  sep = strrchr(root, '/');
#endif
  if (sep)
    *sep = '\0';

  if (chdir(root) != 0) {
    printf("[ERRO] Falha ao navegar para raiz: %s\n", root);
    return 1;
  }

  int no_pull = 0;
  int force = 0;
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--no-pull") == 0)
      no_pull = 1;
    else if (strcmp(argv[i], "--force") == 0 || strcmp(argv[i], "-f") == 0)
      force = 1;
  }

  printf(
      "\n\033[1;"
      "36m════════════════════════════════════════════════════════\033[0m\n");
  printf("\033[1;36m  ALRIOS Sovereign Full Update & Hot Reload Pipeline    "
         "\033[0m\n");
  printf(
      "\033[1;36m════════════════════════════════════════════════════════\033["
      "0m\n\n");

/* 1. Git Pull */
#ifdef _WIN32
  if (!no_pull && GetFileAttributesA(".git") != INVALID_FILE_ATTRIBUTES) {
#else
  if (!no_pull && access(".git", F_OK) == 0) {
#endif
    printf("\033[1;34m[1/4]\033[0m Sincronizando com repositorio remoto (git "
           "pull origin main)...\n");
    int git_rc = system("git pull");
    if (git_rc != 0) {
      printf("\033[1;33m[AVISO]\033[0m git pull retornou codigo %d "
             "(continuando com fontes locais)...\n\n",
             git_rc);
    } else {
      printf("\033[1;32m✓\033[0m Repositorio sincronizado com sucesso.\n\n");
    }
  } else {
    printf("\033[1;34m[1/4]\033[0m Sincronizacao git ignorada (--no-pull ou "
           "sem .git).\n\n");
  }

  /* 2. Build kernel and developer tools */
  printf("\033[1;34m[2/4]\033[0m Verificando kernel (arcore) e ferramentas de "
         "desenvolvedor (armake/alrios)...\n");
#ifdef _WIN32
  system("cmake --build build --target arcore armake alrios -j4");
#else
  system("cmake --build build-linux --target arcore armake alrios -- -j4 "
         "2>/dev/null || cmake --build build --target arcore armake alrios -j4 "
         "2>/dev/null || true");
#endif
  printf("\033[1;32m✓\033[0m Kernel e ferramentas atualizados.\n\n");

  /* 3. Incremental App Packaging */
  printf("\033[1;34m[3/4]\033[0m Executando empacotamento incremental de "
         "aplicacoes (Content-Hash)...\n");
#ifndef _WIN32
  DIR *d = opendir("src/apps");
  if (d) {
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
      if (entry->d_name[0] == '.')
        continue;
      char appdir[1024];
      snprintf(appdir, sizeof(appdir), "src/apps/%s", entry->d_name);
      struct stat st;
      if (stat(appdir, &st) == 0 && S_ISDIR(st.st_mode)) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
                 "./arcore/armake build %s arcore/apps/%s.arapp %s", appdir,
                 entry->d_name, force ? "--force" : "");
        system(cmd);
      }
    }
    closedir(d);
  }
#endif
  printf("\n\033[1;32m✓\033[0m Todas as aplicacoes foram verificadas e "
         "empacotadas.\n\n");

  /* 4. Trigger auto-discovery & process reload */
  printf("\033[1;34m[4/4]\033[0m Notificando arcore para atualizar registro e "
         "daemons...\n");
  int fd = ctl_connect();
  if (fd >= 0) {
    send_and_print(fd, IPC_CTL_REFRESH, NULL);
    ar_socket_close(fd);
    printf("\033[1;32m✓\033[0m arcore atualizado com sucesso.\n\n");
  } else {
    printf("\033[1;33m[INFO]\033[0m arcore nao esta rodando no momento. Inicie "
           "com: ./alrios power on\n\n");
  }

  /* 5. Print status table */
  printf("\033[1;36m=== Status Atual do Ecossistema ===\033[0m\n");
  run_ctl(IPC_CTL_LIST, NULL);
  return 0;
}

static int cmd_update(const char *which) {
  if (strcmp(which, "all") == 0) {
    char *fake_argv[] = {"alrios", "fullupdate"};
    return cmd_fullupdate(2, fake_argv);
  }

  char base[1024], root[1024];
  get_base_dir(base, sizeof(base));
  strncpy(root, base, sizeof(root) - 1);
  root[sizeof(root) - 1] = '\0';
  char *sep = strrchr(root, '\\');
#ifndef _WIN32
  sep = strrchr(root, '/');
#endif
  if (sep)
    *sep = '\0';

  char cmd[2048];
  snprintf(cmd, sizeof(cmd),
           "cmake --build build-linux --config Release --target %s", which);

  printf("update %s: %s\n  (dir: %s)\n", which, cmd, root);
  if (chdir(root) != 0) {
    printf("chdir failed: %s\n", root);
    return 1;
  }
  int res = system(cmd);
  if (res == 0) {
    printf("\n[ALRIOS] Update successful! Triggering hot power reload...\n");
    run_ctl(IPC_CTL_POWER_RELOAD, NULL);
  }
  return res;
}

static void abs_path(const char *in, char *out, int size) {
#ifdef _WIN32
  _fullpath(out, in, size);
#else
  if (!realpath(in, out)) {
    strncpy(out, in, size - 1);
    out[size - 1] = '\0';
  }
#endif
}

static int path_eq(const char *a, const char *b) {
#ifdef _WIN32
  return _stricmp(a, b) == 0;
#else
  return strcmp(a, b) == 0;
#endif
}

static int fexists(const char *path) {
  FILE *f = fopen(path, "rb");
  if (f) {
    fclose(f);
    return 1;
  }
  return 0;
}

static int cmd_build(int argc, char *argv[]) {
  const char *src = NULL;
  const char *out = NULL;

  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
      src = argv[++i];
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      out = argv[++i];
    } else {
      printf("build: unknown option: %s (use -p <SRC> [-o <OUT>])\n", argv[i]);
      return 1;
    }
  }
  if (!src) {
    printf("build: requires -p <SRC>\n");
    return 1;
  }

  int apps_out = (!out || strcmp(out, "apps") == 0 ||
                  strcmp(out, "/apps") == 0 || strcmp(out, "\\apps") == 0);

  /* Delegate app building directly to armake buildapp */
  char exe[1024];
  get_base_dir(exe, sizeof(exe));
#ifdef _WIN32
  strncat(exe, "\\armake.exe", sizeof(exe) - strlen(exe) - 1);
#else
  strncat(exe, "/armake", sizeof(exe) - strlen(exe) - 1);
#endif
  const char *o = apps_out ? "apps" : out;
  char cmd[2048];
#ifdef _WIN32
  snprintf(cmd, sizeof(cmd), "\"\"%s\" buildapp -s \"%s\" -o \"%s\"\"", exe,
           src, o);
#else
  snprintf(cmd, sizeof(cmd), "\"%s\" buildapp -s \"%s\" -o \"%s\"", exe, src,
           o);
#endif
  printf("build: %s\n", cmd);
  if (system(cmd) != 0)
    return 1;
  printf("tip: run 'alrios refresh' to update application list\n");
  return 0;
}

int main(int argc, char *argv[]) {
  const char *prog = strrchr(argv[0], '/');
  if (!prog)
    prog = strrchr(argv[0], '\\');
  prog = prog ? prog + 1 : argv[0];
  if (strcmp(prog, "arpm") == 0 || strcmp(prog, "arpm.exe") == 0) {
    return cmd_arpm(argc, argv);
  }

  if (argc < 2) {
    print_usage();
    return 0;
  }

  const char *a1 = argv[1];

  if (strcmp(a1, "help") == 0 || strcmp(a1, "--help") == 0 ||
      strcmp(a1, "-h") == 0) {
    print_usage();
    return 0;
  }

  if (strcmp(a1, "version") == 0 || strcmp(a1, "--version") == 0 ||
      strcmp(a1, "-v") == 0) {
    printf("ALRIOS CLI v0.2.01\n");
    return 0;
  }

  /* Comandos do Ciclo de Vida do SO (arcore 9600) */
  if (strcmp(a1, "arpm") == 0 || strcmp(a1, "pkg") == 0)
    return cmd_arpm(argc - 1, argv + 1);

  if (strcmp(a1, "status") == 0 || strcmp(a1, "list") == 0)
    return run_ctl(IPC_CTL_LIST, NULL);

  if (strcmp(a1, "power") == 0) {
    if (argc < 3) {
      print_usage();
      return 1;
    }
    if (strcmp(argv[2], "on") == 0)
      return cmd_power_on();
    if (strcmp(argv[2], "off") == 0)
      return run_ctl(IPC_CTL_POWER_OFF, NULL);
    if (strcmp(argv[2], "reload") == 0)
      return run_ctl(IPC_CTL_POWER_RELOAD, NULL);
    print_usage();
    return 1;
  }

  if (strcmp(a1, "start") == 0) {
    if (argc < 3) {
      print_usage();
      return 1;
    }
    if (strcmp(argv[2], "add") == 0 && argc >= 4)
      return cmd_autostart_add(argv[3]);
    if (strcmp(argv[2], "del") == 0 && argc >= 4)
      return cmd_autostart_del(argv[3]);
    return run_ctl(IPC_CTL_START, argv[2]);
  }

  if (strcmp(a1, "stop") == 0) {
    if (argc < 3) {
      print_usage();
      return 1;
    }
    return run_ctl(IPC_CTL_STOP, argv[2]);
  }

  if (strcmp(a1, "restart") == 0) {
    if (argc < 3) {
      print_usage();
      return 1;
    }
    return run_ctl(IPC_CTL_RESTART, argv[2]);
  }

  if (strcmp(a1, "update") == 0) {
    if (argc < 3) {
      print_usage();
      return 1;
    }
    const char *w = argv[2];
    if (strcmp(w, "all") != 0 && strcmp(w, "alrios") != 0 &&
        strcmp(w, "armake") != 0 && strcmp(w, "arinstall") != 0) {
      printf("update: invalid target '%s' (use all|alrios|armake|arinstall)\n",
             w);
      return 1;
    }
    return cmd_update(w);
  }

  if (strcmp(a1, "fullupdate") == 0 || strcmp(a1, "updateall") == 0)
    return cmd_fullupdate(argc, argv);

  if (strcmp(a1, "build") == 0)
    return cmd_build(argc, argv);

  if (strcmp(a1, "refresh") == 0)
    return run_ctl(IPC_CTL_REFRESH, NULL);

  /* Universal dynamic routing for apps (IPC 9500).
     The target application owns and serves its own command catalogue and help!
   */
  const char *target_app = a1;
  if (strcmp(a1, "auth") == 0) target_app = "arauth";
  else if (strcmp(a1, "cdn") == 0) target_app = "arcdn";
  else if (strcmp(a1, "db") == 0) target_app = "ardb";
  else if (strcmp(a1, "ws") == 0) target_app = "arws";

  if (!is_app_installed(target_app)) {
    printf("\033[1;31m[ERRO]\033[0m O aplicativo '%s' nao esta instalado no sistema.\n", target_app);
    printf("Dica: use 'alrios arpm search %s' ou 'alrios arpm install %s' para instalar.\n", target_app, target_app);
    return 1;
  }

  return cmd_app_query_args(target_app, argc, argv);
}
