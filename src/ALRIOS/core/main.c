/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "loader.h"
#include "ctl.h"
#include "aros_hal.h"
#include "ar_kernel.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#else
#include <windows.h>
#include <shellapi.h>
#include <process.h>
#include <direct.h>
#endif

static void escalate_privileges(void) {
#ifdef __linux__
    if (geteuid() != 0) {
        alri_printf("  \033[33m⚡\033[0m Root privileges required. Re-running with sudo...\n\n");
        char exe[1024];
        ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (len > 0) {
            exe[len] = '\0';
            char *args[] = {"sudo", exe, NULL};
            execvp("sudo", args);
            alri_printf("  \033[31m✗\033[0m Failed to escalate: sudo not available?\n");
        }
    }
#elif defined(_WIN32)
    static int elevated = 0;
    if (!elevated) {
        HANDLE hToken = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            TOKEN_ELEVATION elev;
            DWORD size = sizeof(elev);
            if (GetTokenInformation(hToken, TokenElevation, &elev, size, &size)) {
                if (!elev.TokenIsElevated) {
                    alri_printf("  \033[33m⚡\033[0m Administrator privileges required. Re-launching...\n\n");
                    char exe[MAX_PATH];
                    GetModuleFileNameA(NULL, exe, sizeof(exe));
                    SHELLEXECUTEINFOA sei = {0};
                    sei.cbSize = sizeof(sei);
                    sei.lpVerb = "runas";
                    sei.lpFile = exe;
                    sei.nShow = SW_NORMAL;
                    if (ShellExecuteExA(&sei)) {
                        exit(0);
                    }
                    alri_printf("  \033[31m✗\033[0m Failed to escalate privileges.\n");
                }
            }
            CloseHandle(hToken);
        }
        elevated = 1;
    }
#endif
}

static void drop_privileges(void) {
#ifdef __linux__
    if (geteuid() != 0) return;

    const char *stay = getenv("ARWS_STAY_ROOT");
    if (stay && stay[0] == '1') {
        alri_printf("  \033[2m◇\033[0m Staying root (ARWS_STAY_ROOT=1)\n");
        return;
    }

    const char *s_uid = getenv("SUDO_UID");
    const char *s_gid = getenv("SUDO_GID");
    if (!s_uid || !s_gid) {
        fprintf(stderr, "  \033[31m✗\033[0m Escalated but SUDO_UID/GID not set, refusing to stay root\n");
        return;
    }

    uid_t uid = (uid_t)atol(s_uid);
    gid_t gid = (gid_t)atol(s_gid);

    struct passwd *pw = getpwuid(uid);
    if (pw) {
        int ngroups = 0;
        getgrouplist(pw->pw_name, gid, NULL, &ngroups);
        if (ngroups > 0) {
            gid_t *groups = (gid_t *)malloc(sizeof(gid_t) * ngroups);
            if (groups) {
                getgrouplist(pw->pw_name, gid, groups, &ngroups);
                setgroups(ngroups, groups);
                free(groups);
            }
        }
    }

    if (setgid(gid) != 0) {
        fprintf(stderr, "  \033[31m✗\033[0m Failed to drop group privileges\n");
        return;
    }
    if (setuid(uid) != 0) {
        fprintf(stderr, "  \033[31m✗\033[0m Failed to drop user privileges\n");
        return;
    }

    unsetenv("SUDO_UID");
    unsetenv("SUDO_GID");
    alri_printf("  \033[2m◇\033[0m Dropped privileges to uid=%d gid=%d\n", uid, gid);
#elif defined(_WIN32)
    (void)0;
#endif
}

static void enable_ansi(void) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
    }
#endif
}

static void print_banner(void) {
    alri_printf("\n");
    alri_printf("  " CYN BLD "╔═══════════════════════════════════════════════╗\n" RST);
    alri_printf("  " CYN BLD "║" RST "         " CYN "ALRI OS " RST "v1.0.0                  " CYN BLD "║\n" RST);
    alri_printf("  " CYN BLD "║" RST "     " GRN BLD "⚡ Virtual Machine Booting..." RST "       " CYN BLD "║\n" RST);
    alri_printf("  " CYN BLD "╚═══════════════════════════════════════════════╝\n" RST);
    alri_printf("\n");
}

static void print_status(const char *label, const char *status, const char *color) {
    alri_printf("  " DIM "◇" RST " %-30s [%s%s" RST "]\n", label, color, status);
}

static const char *spinner_chars = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏";
static int spinner_idx = 0;

static void spinner_start(void) {
    spinner_idx = 0;
}

static void spinner_tick(const char *msg) {
    alri_printf("  " DIM "%c" RST " %s\r", spinner_chars[spinner_idx], msg);
    spinner_idx = (spinner_idx + 1) % 10;
    ar_sleep_ms(60);
}

static void spinner_done(const char *msg, const char *result_color, const char *result) {
    alri_printf("  " GRN "✓" RST " %-40s [%s%s" RST "]\n", msg, result_color, result);
}

static volatile int running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static void cleanup_and_exit(void) {
    ar_process_group_destroy(loader_get_proc_group());
    loader_cleanup_temp();
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    enable_ansi();
    print_banner();

    {
        char cwd[1024];
        loader_get_base_dir(cwd, sizeof(cwd));
        if (chdir(cwd) != 0)
            alri_printf("  " YLW "!" RST " Failed to chdir to %s\n", cwd);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(cleanup_and_exit);

    char run_dir[1024];
    loader_get_run_dir(run_dir, sizeof(run_dir));

    char apps_dir[1024];
    loader_get_apps_dir(apps_dir, sizeof(apps_dir));

    loader_set_temp_dir(run_dir);

    loader_cleanup_temp();

    /*
     * Drop root privileges immediately when started via sudo.
     * Only the initial bind of privileged ports (<1024) needs root.
     * Use `setcap cap_net_bind_service=+ep` for that, or run
     * the whole thing as root (not recommended).
     */
    drop_privileges();

    spinner_start();
    for (int i = 0; i < 6; i++)
        spinner_tick("Initializing kernel...");
    if (ar_init() != 0) {
        spinner_done("Initializing kernel", RED, "FAIL");
        return 1;
    }
    spinner_done("Initializing kernel", GRN, "OK");

    spinner_start();
    for (int i = 0; i < 6; i++)
        spinner_tick("Starting health monitor...");
    ar_health_init(5000);
    spinner_done("Starting health monitor", GRN, "OK");

    alri_printf("\n  " DIM "◇" RST " Scanning " CYN "/run" RST " (" DIM "%s" RST ")\n", run_dir);

    loader_scan_phase(run_dir, 0);

    alri_printf("\n  " DIM "◇" RST " Scanning " CYN "/apps" RST " (" DIM "%s" RST ")\n", apps_dir);

    loader_scan_phase(apps_dir, 1);

    alri_printf("\n");
    int svc_started = ar_svc_start_all();
    if (svc_started > 0) {
        char status[32];
        snprintf(status, sizeof(status), "%d started", svc_started);
        print_status("Starting services", status, GRN);
    } else {
        print_status("Starting services", "OK", GRN);
    }

    /* Spawn standalone app processes AFTER services (arws on port 9500) are up */
    loader_scan_phase(apps_dir, 2);

    ctl_start();
    alri_printf("\n");
    alri_printf("  " GRN BLD "✓ System ready." RST " Press Ctrl+C to stop.\n");
    alri_printf("\n");

    while (running) {
        loader_reap_apps();
        ar_sleep_ms(500);
    }

    alri_printf("\n  " DIM "◇" RST " Shutting down...\n");
    ctl_stop();
    ar_shutdown();

    return 0;
}
