/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "installer_core.h"
#include "runtimes.h"
#include "zip.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <urlmon.h>
#include <shlobj.h>
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#define mkdir_p_(p) _mkdir(p)
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#define mkdir_p_(p) mkdir(p, 0755)
#endif

/* ─── Helpers ─── */

static void log_msg(const installer_callbacks_t *cb, void *user_data, const char *fmt, ...) {
    if (!cb || !cb->on_log) return;
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    cb->on_log(buf, user_data);
}

static void update_progress(const installer_callbacks_t *cb, void *user_data, int step, int total, int pct, const char *name) {
    if (cb && cb->on_progress) {
        cb->on_progress(step, total, pct, name, user_data);
    }
}

static void mkdir_p(const char *path) {
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == AR_PATH_SEP) {
            *p = '\0';
            mkdir_p_(tmp);
            *p = AR_PATH_SEP;
        }
    }
    mkdir_p_(tmp);
}

static int file_exists(const char *path) {
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path, F_OK) == 0;
#endif
}

/* Safe process execution helper without invoking an OS shell (prevents CWE-78) */
static int safe_run_process(const char *prog, char *const argv[]) {
#ifdef _WIN32
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    char cmdline[4096] = { 0 };
    for (int i = 0; argv[i]; i++) {
        if (i > 0) strncat(cmdline, " ", sizeof(cmdline) - strlen(cmdline) - 1);
        strncat(cmdline, "\"", sizeof(cmdline) - strlen(cmdline) - 1);
        strncat(cmdline, argv[i], sizeof(cmdline) - strlen(cmdline) - 1);
        strncat(cmdline, "\"", sizeof(cmdline) - strlen(cmdline) - 1);
    }
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (code == 0) ? 0 : -1;
#else
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp(prog, argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
#endif
}

static int is_safe_url(const char *url) {
    if (!url) return 0;
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) return 0;
    for (const char *p = url; *p; p++) {
        if (*p == ';' || *p == '&' || *p == '|' || *p == '`' || *p == '$' ||
            *p == '\n' || *p == '\r' || *p == '"' || *p == '\'')
            return 0;
    }
    return 1;
}

static int download_file(const char *url, const char *dest) {
    if (!is_safe_url(url)) return -1;
#ifdef _WIN32
    HRESULT hr = URLDownloadToFileA(NULL, url, dest, 0, NULL);
    return SUCCEEDED(hr) ? 0 : -1;
#else
    char *const curl_argv[] = {"curl", "-fsSL", (char *)url, "-o", (char *)dest, NULL};
    if (safe_run_process("curl", curl_argv) == 0) return 0;

    char *const wget_argv[] = {"wget", "-q", (char *)url, "-O", (char *)dest, NULL};
    return safe_run_process("wget", wget_argv);
#endif
}

static int extract_zip_to_dir(const char *zip_path, const char *dest_dir) {
    zip_reader_t *z = zip_reader_open(zip_path);
    if (!z) return -1;

    mkdir_p(dest_dir);
    int count = zip_reader_count(z);
    for (int i = 0; i < count; i++) {
        zip_reader_extract(z, i, dest_dir);
    }
    zip_reader_close(z);
    return 0;
}

static void copy_dir_recursive(const char *src, const char *dst) {
    mkdir_p(dst);
#ifdef _WIN32
    char *const xcopy_argv[] = {"xcopy", "/E", "/I", "/Y", "/Q", (char *)src, (char *)dst, NULL};
    safe_run_process("xcopy", xcopy_argv);
#else
    char src_dot[2048];
    snprintf(src_dot, sizeof(src_dot), "%s/.", src);
    char *const cp_argv[] = {"cp", "-rP", src_dot, (char *)dst, NULL};
    safe_run_process("cp", cp_argv);
#endif
}

/* ─── Privileges & Default Paths ─── */

int installer_is_privileged(void) {
#ifdef _WIN32
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            fRet = Elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return fRet ? 1 : 0;
#else
    return (geteuid() == 0) ? 1 : 0;
#endif
}

void installer_get_default_paths(char *dest_out, size_t dest_size, int *is_privileged) {
    int priv = installer_is_privileged();
    if (is_privileged) *is_privileged = priv;

#ifdef _WIN32
    if (priv) {
        char progFiles[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES, NULL, 0, progFiles))) {
            snprintf(dest_out, dest_size, "%s\\ALRIOS", progFiles);
        } else {
            snprintf(dest_out, dest_size, "C:\\Program Files\\ALRIOS");
        }
    } else {
        const char *localAppData = getenv("LOCALAPPDATA");
        if (localAppData && localAppData[0]) {
            snprintf(dest_out, dest_size, "%s\\Programs\\ALRIOS", localAppData);
        } else {
            const char *userProfile = getenv("USERPROFILE");
            snprintf(dest_out, dest_size, "%s\\AppData\\Local\\Programs\\ALRIOS", userProfile ? userProfile : "C:");
        }
    }
#else
    if (priv) {
        snprintf(dest_out, dest_size, "/opt/alrios");
    } else {
        const char *home = getenv("HOME");
        snprintf(dest_out, dest_size, "%s/.local/share/alrios", home ? home : "/tmp");
    }
#endif
}

int installer_check_tool_exists(const char *tool_name) {
#ifdef _WIN32
    char exe_name[256];
    if (strstr(tool_name, ".exe") == NULL) {
        snprintf(exe_name, sizeof(exe_name), "%s.exe", tool_name);
    } else {
        snprintf(exe_name, sizeof(exe_name), "%s", tool_name);
    }
    char found_path[MAX_PATH];
    return SearchPathA(NULL, exe_name, NULL, MAX_PATH, found_path, NULL) > 0 ? 1 : 0;
#else
    char *const which_argv[] = {"which", (char *)tool_name, NULL};
    return (safe_run_process("which", which_argv) == 0) ? 1 : 0;
#endif
}

int installer_has_embedded_payload(const char *self_exe) {
    if (!self_exe || !file_exists(self_exe)) return 0;
    zip_reader_t *z = zip_reader_open(self_exe);
    if (!z) return 0;
    int count = zip_reader_count(z);
    zip_reader_close(z);
    return count > 0 ? 1 : 0;
}

/* ─── Extraction & Setup ─── */

int installer_extract_payload(const installer_config_t *cfg, const installer_callbacks_t *cb, void *user_data) {
    log_msg(cb, user_data, "Criando diretorio de destino: %s\n", cfg->dest_dir);
    mkdir_p(cfg->dest_dir);

    /* 1. Checar se este proprio executavel tem o ZIP embutido */
    if (installer_has_embedded_payload(cfg->self_exe)) {
        log_msg(cb, user_data, "Extraindo pacote de instalacao embutido...\n");
        if (extract_zip_to_dir(cfg->self_exe, cfg->dest_dir) != 0) {
            log_msg(cb, user_data, "[ERRO] Falha ao descompactar pacote embutido.\n");
            return -1;
        }
    } else {
        /* 2. Procurar pasta arcore local (modo de desenvolvimento / repositorio) */
        char local_arcore[1024];
        snprintf(local_arcore, sizeof(local_arcore), "arcore");
        if (file_exists(local_arcore)) {
            log_msg(cb, user_data, "Instalando a partir dos fontes locais (arcore/)...\n");
            char dest_arcore[1024];
            snprintf(dest_arcore, sizeof(dest_arcore), "%s%carcore", cfg->dest_dir, AR_PATH_SEP);
            copy_dir_recursive(local_arcore, dest_arcore);
        } else {
            /* 3. Baixar versao de release do GitHub */
            log_msg(cb, user_data, "Baixando ultima versao do ALRIOS do GitHub...\n");
            char staging[1024], zip_file[1024];
            snprintf(staging, sizeof(staging), "%s%c.staging", cfg->dest_dir, AR_PATH_SEP);
            mkdir_p(staging);
#ifdef _WIN32
            snprintf(zip_file, sizeof(zip_file), "%s\\alrios-release.zip", staging);
            const char *url = "https://github.com/alrigroup/alrios/releases/latest/download/alrios-windows-x64-installer.zip";
#else
            snprintf(zip_file, sizeof(zip_file), "%s/alrios-release.tar.gz", staging);
            const char *url = "https://github.com/alrigroup/alrios/releases/latest/download/alrios-linux-x64-installer.tar.gz";
#endif
            if (download_file(url, zip_file) != 0) {
                log_msg(cb, user_data, "[ERRO] Nao foi possivel baixar o pacote oficial de %s\n", url);
                return -1;
            }
#ifdef _WIN32
            extract_zip_to_dir(zip_file, cfg->dest_dir);
#else
            char *const tar_argv[] = {"tar", "-xzf", (char *)zip_file, "-C", (char *)cfg->dest_dir, NULL};
            safe_run_process("tar", tar_argv);
#endif
        }
    }

    /* Ajustar permissoes de execucao no Linux via syscall nativa chmod() */
#ifndef _WIN32
    const char *bins[] = {"alrios", "arcore", "armake", "arinstall", NULL};
    for (int i = 0; bins[i]; i++) {
        char bin_path[2048];
        snprintf(bin_path, sizeof(bin_path), "%s/arcore/%s", cfg->dest_dir, bins[i]);
        chmod(bin_path, 0755);
    }
#endif

    log_msg(cb, user_data, "✓ Arquivos base do ALRIOS instalados com sucesso.\n");
    return 0;
}

/* ─── PATH Registration ─── */

int installer_register_path(const char *dest_dir, const installer_callbacks_t *cb, void *user_data) {
    log_msg(cb, user_data, "Configurando variaveis de ambiente e PATH do sistema...\n");

#ifdef _WIN32
    char arcore_bin[1024];
    snprintf(arcore_bin, sizeof(arcore_bin), "%s\\arcore", dest_dir);

    HKEY hKey;
    const char *subKey = installer_is_privileged() ?
        "System\\CurrentControlSet\\Control\\Session Manager\\Environment" :
        "Environment";
    HKEY rootKey = installer_is_privileged() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    if (RegOpenKeyExA(rootKey, subKey, 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        char currentPath[32768] = {0};
        DWORD dataSize = sizeof(currentPath) - 1;
        DWORD type = REG_EXPAND_SZ;

        RegQueryValueExA(hKey, "Path", NULL, &type, (LPBYTE)currentPath, &dataSize);

        if (strstr(currentPath, arcore_bin) == NULL) {
            char newPath[32768];
            if (strlen(currentPath) > 0 && currentPath[strlen(currentPath) - 1] != ';') {
                snprintf(newPath, sizeof(newPath), "%s;%s", currentPath, arcore_bin);
            } else {
                snprintf(newPath, sizeof(newPath), "%s%s", currentPath, arcore_bin);
            }
            RegSetValueExA(hKey, "Path", 0, REG_EXPAND_SZ, (const BYTE *)newPath, (DWORD)strlen(newPath) + 1);
            log_msg(cb, user_data, "✓ ALRIOS adicionado ao PATH do Windows: %s\n", arcore_bin);
        } else {
            log_msg(cb, user_data, "✓ ALRIOS ja presente no PATH do Windows.\n");
        }
        RegCloseKey(hKey);

        /* Notificar o sistema sobre a mudanca de variaveis */
        DWORD_PTR dwResult;
        SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 3000, &dwResult);
    }
#else
    char target_bin[1024];
    int is_root = (geteuid() == 0);
    const char *bin_dir = is_root ? "/usr/local/bin" : NULL;

    if (!bin_dir) {
        const char *home = getenv("HOME");
        static char user_bin[1024];
        snprintf(user_bin, sizeof(user_bin), "%s/.local/bin", home ? home : "/tmp");
        mkdir_p(user_bin);
        bin_dir = user_bin;

        /* Exportar PATH nos perfis de shell se nao estiver presente */
        char check_path[1024];
        snprintf(check_path, sizeof(check_path), "%s/.local/bin", home);
        const char *env_path = getenv("PATH");
        if (!env_path || strstr(env_path, check_path) == NULL) {
            char pfile[1024];
            snprintf(pfile, sizeof(pfile), "%s/.bashrc", home);
            FILE *f = fopen(pfile, "a");
            if (f) {
                fprintf(f, "\n# ALRIOS Sovereign System Path\nexport PATH=\"$HOME/.local/bin:$PATH\"\n");
                fclose(f);
            }
            snprintf(pfile, sizeof(pfile), "%s/.zshrc", home);
            if (file_exists(pfile)) {
                f = fopen(pfile, "a");
                if (f) {
                    fprintf(f, "\n# ALRIOS Sovereign System Path\nexport PATH=\"$HOME/.local/bin:$PATH\"\n");
                    fclose(f);
                }
            }
        }
    }

    /* Criar links simbolicos usando chamadas de sistema nativas C (unlink/symlink) */
    const char *bins[] = {"alrios", "arpm", "arcore", "armake", NULL};
    const char *targets[] = {"alrios", "alrios", "arcore", "armake", NULL};
    for (int i = 0; bins[i]; i++) {
        char link_path[2048], target_path[2048];
        snprintf(link_path, sizeof(link_path), "%s/%s", bin_dir, bins[i]);
        snprintf(target_path, sizeof(target_path), "%s/arcore/%s", dest_dir, targets[i]);
        unlink(link_path);
        symlink(target_path, link_path);
    }
    log_msg(cb, user_data, "✓ Comandos vinculados com sucesso em %s (alrios, arpm, arcore, armake)\n", bin_dir);
#endif

    return 0;
}

/* ─── GCC Toolchain Auto-Provisioning ─── */

int installer_provision_gcc(const char *dest_dir, const installer_callbacks_t *cb, void *user_data) {
    if (installer_check_tool_exists("gcc")) {
        log_msg(cb, user_data, "✓ Compilador GCC C/C++ ja disponivel no sistema.\n");
        return 0;
    }

    log_msg(cb, user_data, "-> Compilador GCC nao encontrado. Instalando automaticamente...\n");

#ifdef _WIN32
    char staging[1024], zip_file[1024], toolchain_dir[1024];
    snprintf(staging, sizeof(staging), "%s\\.staging", dest_dir);
    mkdir_p(staging);
    snprintf(zip_file, sizeof(zip_file), "%s\\w64devkit.zip", staging);
    snprintf(toolchain_dir, sizeof(toolchain_dir), "%s\\toolchain", dest_dir);
    mkdir_p(toolchain_dir);

    const char *gcc_url = "https://github.com/skeeto/w64devkit/releases/download/v2.0.0/w64devkit-1.21.0.zip";
    log_msg(cb, user_data, "   Baixando kit completo de desenvolvimento C/C++ (w64devkit MinGW)...\n");
    if (download_file(gcc_url, zip_file) != 0) {
        log_msg(cb, user_data, "   [AVISO] Falha ao baixar compilador GCC. Prosseguindo sem ele...\n");
        return -1;
    }

    log_msg(cb, user_data, "   Extraindo compilador GCC para %s...\n", toolchain_dir);
    extract_zip_to_dir(zip_file, toolchain_dir);

    /* Adicionar w64devkit/bin ao PATH do Windows */
    char gcc_bin[1024];
    snprintf(gcc_bin, sizeof(gcc_bin), "%s\\w64devkit\\bin", toolchain_dir);
    if (file_exists(gcc_bin)) {
        HKEY hKey;
        const char *subKey = installer_is_privileged() ?
            "System\\CurrentControlSet\\Control\\Session Manager\\Environment" :
            "Environment";
        HKEY rootKey = installer_is_privileged() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

        if (RegOpenKeyExA(rootKey, subKey, 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            char currentPath[32768] = {0};
            DWORD dataSize = sizeof(currentPath) - 1;
            DWORD type = REG_EXPAND_SZ;
            RegQueryValueExA(hKey, "Path", NULL, &type, (LPBYTE)currentPath, &dataSize);

            if (strstr(currentPath, gcc_bin) == NULL) {
                char newPath[32768];
                snprintf(newPath, sizeof(newPath), "%s;%s", currentPath, gcc_bin);
                RegSetValueExA(hKey, "Path", 0, REG_EXPAND_SZ, (const BYTE *)newPath, (DWORD)strlen(newPath) + 1);
            }
            RegCloseKey(hKey);
        }
    }
    log_msg(cb, user_data, "✓ Compilador GCC C/C++ instalado com sucesso em %s\n", gcc_bin);
#else
    if (geteuid() == 0) {
        log_msg(cb, user_data, "   Instalando compilador GCC via gerenciador de pacotes do sistema...\n");
        char *const which_apt[] = {"which", "apt-get", NULL};
        char *const which_pac[] = {"which", "pacman", NULL};
        char *const which_dnf[] = {"which", "dnf", NULL};
        if (safe_run_process("which", which_apt) == 0) {
            char *const apt_argv[] = {"apt-get", "install", "-y", "-qq", "build-essential", "gcc", "make", NULL};
            safe_run_process("apt-get", apt_argv);
        } else if (safe_run_process("which", which_pac) == 0) {
            char *const pac_argv[] = {"pacman", "-Sy", "--noconfirm", "base-devel", "gcc", "make", NULL};
            safe_run_process("pacman", pac_argv);
        } else if (safe_run_process("which", which_dnf) == 0) {
            char *const dnf_argv[] = {"dnf", "install", "-y", "gcc", "make", NULL};
            safe_run_process("dnf", dnf_argv);
        }
        if (installer_check_tool_exists("gcc")) {
            log_msg(cb, user_data, "✓ GCC instalado com sucesso via gerenciador nativo.\n");
            return 0;
        }
    }
    log_msg(cb, user_data, "   [AVISO] Nao foi possivel instalar o GCC automaticamente sem root. Instale com: sudo apt install build-essential\n");
#endif

    return 0;
}

/* ─── Runtime Provisioning ─── */

int installer_provision_runtime(const char *dest_dir, const char *rt_name, const installer_callbacks_t *cb, void *user_data) {
    for (size_t i = 0; i < RUNTIME_COUNT; i++) {
        if (strcmp(available_runtimes[i].name, rt_name) == 0) {
            log_msg(cb, user_data, "-> Provisionando runtime %s (v%s)...\n", rt_name, available_runtimes[i].version);
            char run_dir[1024];
            snprintf(run_dir, sizeof(run_dir), "%s%carcore%crun", dest_dir, AR_PATH_SEP, AR_PATH_SEP);
            mkdir_p(run_dir);

            char staging[1024];
            snprintf(staging, sizeof(staging), "%s%c.staging%c%s", dest_dir, AR_PATH_SEP, AR_PATH_SEP, rt_name);
            mkdir_p(staging);

#ifdef _WIN32
            const char *url = available_runtimes[i].url_windows;
            char dest_file[1024];
            snprintf(dest_file, sizeof(dest_file), "%s\\rt.zip", staging);
#else
            const char *url = available_runtimes[i].url_linux;
            char dest_file[1024];
            snprintf(dest_file, sizeof(dest_file), "%s/rt.tar.gz", staging);
#endif
            log_msg(cb, user_data, "   Baixando %s de %s...\n", rt_name, url);
            if (download_file(url, dest_file) == 0) {
                log_msg(cb, user_data, "✓ Runtime %s baixado com sucesso.\n", rt_name);
            }
            return 0;
        }
    }
    return -1;
}

/* ─── Cleanup ─── */

void installer_cleanup(const char *dest_dir) {
    char staging[1024];
    snprintf(staging, sizeof(staging), "%s%c.staging", dest_dir, AR_PATH_SEP);
#ifdef _WIN32
    char *const rmdir_argv[] = {"cmd.exe", "/c", "rmdir", "/S", "/Q", staging, NULL};
    safe_run_process("cmd.exe", rmdir_argv);
#else
    char *const rm_argv[] = {"rm", "-rf", staging, NULL};
    safe_run_process("rm", rm_argv);
#endif
}

/* ─── Master Pipeline ─── */

int installer_run(const installer_config_t *cfg, const installer_callbacks_t *cb, void *user_data) {
    int total_steps = 5;

    /* Passo 1: Preparacao e Destino */
    update_progress(cb, user_data, 1, total_steps, 20, "Preparando diretorios");
    log_msg(cb, user_data, "========================================================\n");
    log_msg(cb, user_data, "  ALRIOS — Instalador Oficial Soberano                  \n");
    log_msg(cb, user_data, "========================================================\n");
    log_msg(cb, user_data, "Destino da Instalacao: %s\n\n", cfg->dest_dir);

    if (cfg->dry_run) {
        log_msg(cb, user_data, "[DRY-RUN] Nenhuma alteracao sera feita no disco.\n");
        if (cb && cb->on_complete) cb->on_complete(1, "Dry run executado com sucesso.", user_data);
        return 0;
    }

    /* Passo 2: Extrair Payload */
    update_progress(cb, user_data, 2, total_steps, 40, "Extraindo ecossistema ALRIOS");
    if (installer_extract_payload(cfg, cb, user_data) != 0) {
        if (cb && cb->on_complete) cb->on_complete(0, "Falha na extracao do pacote ALRIOS.", user_data);
        return -1;
    }

    /* Passo 3: Registrar no PATH */
    if (cfg->add_to_path) {
        update_progress(cb, user_data, 3, total_steps, 60, "Configurando PATH do sistema");
        installer_register_path(cfg->dest_dir, cb, user_data);
    }

    /* Passo 4: Dependencias & Toolchains */
    update_progress(cb, user_data, 4, total_steps, 80, "Verificando dependencias essenciais");
    if (cfg->install_gcc) {
        installer_provision_gcc(cfg->dest_dir, cb, user_data);
    }
    if (cfg->install_node) {
        installer_provision_runtime(cfg->dest_dir, "node", cb, user_data);
    }
    if (cfg->install_python) {
        installer_provision_runtime(cfg->dest_dir, "python3", cb, user_data);
    }

    /* Passo 5: Limpeza e Finalizacao */
    update_progress(cb, user_data, 5, total_steps, 100, "Concluindo instalacao");
    installer_cleanup(cfg->dest_dir);

    log_msg(cb, user_data, "\n========================================================\n");
    log_msg(cb, user_data, " ✓ Instalacao do ALRIOS concluida com sucesso!\n");
    log_msg(cb, user_data, "   Execute 'alrios power on' para inicializar.\n");
    log_msg(cb, user_data, "========================================================\n");

    if (cb && cb->on_complete) {
        cb->on_complete(1, "ALRIOS instalado com sucesso no sistema!", user_data);
    }
    return 0;
}
