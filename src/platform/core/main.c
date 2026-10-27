/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
#include <alrios/platform/ar_kernel.h>
#include <alrios/platform/aros_hal.h>
#include "ctl.h"
#include "loader.h"
#include "log.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#else
#include <windows.h>
#include <direct.h>
#include <process.h>
#include <shellapi.h>
#endif

static void drop_privileges(void) {
#ifdef __linux__
  if (geteuid() != 0)
    return;

  const char *stay = getenv("ARWS_STAY_ROOT");
  if (stay && stay[0] == '1') {
    alri_printf("  \033[2m◇\033[0m Staying root (ARWS_STAY_ROOT=1)\n");
    return;
  }

  const char *s_uid = getenv("SUDO_UID");
  const char *s_gid = getenv("SUDO_GID");
  if (!s_uid || !s_gid) {
    fprintf(stderr, "  \033[31m✗\033[0m Escalated but SUDO_UID/GID not set, "
                    "refusing to stay root\n");
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
  alri_printf("  \033[2m◇\033[0m Dropped privileges to uid=%d gid=%d\n", uid,
              gid);
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
  alri_printf("  " CYN BLD
              "╔═══════════════════════════════════════════════╗\n" RST);
  alri_printf("  " CYN BLD "║" RST "         " CYN "ALRI OS " RST
              "v1.0.0                  " CYN BLD "║\n" RST);
  alri_printf("  " CYN BLD "║" RST "     " GRN BLD
              "⚡ Virtual Machine Booting..." RST "       " CYN BLD "║\n" RST);
  alri_printf("  " CYN BLD
              "╚═══════════════════════════════════════════════╝\n" RST);
  alri_printf("\n");
}

static void print_status(const char *label, const char *status,
                         const char *color) {
  alri_printf("  " DIM "◇" RST " %-30s [%s%s" RST "]\n", label, color, status);
}

static const char *spinner_chars = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏";
static int spinner_idx = 0;

static void spinner_start(void) { spinner_idx = 0; }

static void spinner_tick(const char *msg) {
  alri_printf("  " DIM "%c" RST " %s\r", spinner_chars[spinner_idx], msg);
  spinner_idx = (spinner_idx + 1) % 10;
  ar_sleep_ms(60);
}

static void spinner_done(const char *msg, const char *result_color,
                         const char *result) {
  alri_printf("  " GRN "✓" RST " %-40s [%s%s" RST "]\n", msg, result_color,
              result);
}

static volatile int running = 1;
static int policy_initialized = 0;

static void handle_signal(int sig) {
  (void)sig;
  running = 0;
}

static void cleanup_and_exit(void) {
  loader_stop_all();
  ar_process_group_destroy(loader_get_proc_group());
  loader_cleanup_temp();
  if (policy_initialized) {
    loader_policy_shutdown();
    policy_initialized = 0;
  }
}

static void load_build_conf(void) {
  char base_dir[1024];
  char conf_path[2048];
  loader_get_base_dir(base_dir, (int)sizeof(base_dir));
#if defined(_WIN32)
  (void)snprintf(conf_path, sizeof(conf_path), "%s\\..\\alrios.build.conf", base_dir);
#else
  (void)snprintf(conf_path, sizeof(conf_path), "%s/../alrios.build.conf", base_dir);
#endif
  FILE *f = fopen(conf_path, "r");
  if (!f) {
#if defined(_WIN32)
    (void)snprintf(conf_path, sizeof(conf_path), "%s\\alrios.build.conf", base_dir);
#else
    (void)snprintf(conf_path, sizeof(conf_path), "%s/alrios.build.conf", base_dir);
#endif
    f = fopen(conf_path, "r");
  }
  if (!f) return;

  char line[256];
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#' || *p == '\r' || *p == '\n' || *p == '\0') continue;
    char *eq = strchr(p, '=');
    if (!eq) continue;
    *eq = '\0';
    char *key = p;
    char *val = eq + 1;
    /* trim key */
    char *end = key + strlen(key) - 1;
    while (end > key && (*end == ' ' || *end == '\t')) *end-- = '\0';
    /* trim val */
    while (*val == ' ' || *val == '\t') val++;
    end = val + strlen(val) - 1;
    while (end >= val && (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t')) *end-- = '\0';
    if (*key && *val) {
#if defined(_WIN32)
      _putenv_s(key, val);
#else
      setenv(key, val, 0); /* only if not already explicitly set in environment */
#endif
    }
  }
  fclose(f);
}

static int initialize_execution_policy(void) {
  load_build_conf();
  const char *configured_profile = getenv("ALRIOS_PROFILE");
  const char *trust_directory = getenv("ALRIOS_TRUST_DIR");
  alrios_profile_t profile;
  char default_trust_directory[1024];
  int result;

  if (!configured_profile || configured_profile[0] == '\0') {
    configured_profile = "enterprise";
  }
  if (alrios_profile_parse(configured_profile, &profile) != 0) {
    fprintf(stderr, "Invalid ALRIOS_PROFILE: %s\n", configured_profile);
    return -1;
  }
  if (!trust_directory || trust_directory[0] == '\0') {
    char base_dir[1024];
    loader_get_base_dir(base_dir, (int)sizeof(base_dir));
    int written = snprintf(default_trust_directory, sizeof(default_trust_directory), "%s/trust", base_dir);
    if (written < 0 || (size_t)written >= sizeof(default_trust_directory)) {
      return -1;
    }
    trust_directory = default_trust_directory;
  } else if (trust_directory[0] != '/' && !(trust_directory[1] == ':' && (trust_directory[2] == '\\' || trust_directory[2] == '/'))) {
    /* If relative path specified in conf, resolve relative to base_dir or cwd */
    char base_dir[1024];
    loader_get_base_dir(base_dir, (int)sizeof(base_dir));
    char resolved_path[1024];
    int written = snprintf(resolved_path, sizeof(resolved_path), "%s/../%s", base_dir, trust_directory);
    if (written > 0 && (size_t)written < sizeof(resolved_path) && access(resolved_path, F_OK) == 0) {
      (void)snprintf(default_trust_directory, sizeof(default_trust_directory), "%s", resolved_path);
      trust_directory = default_trust_directory;
    } else {
      written = snprintf(resolved_path, sizeof(resolved_path), "%s/%s", base_dir, trust_directory);
      if (written > 0 && (size_t)written < sizeof(resolved_path) && access(resolved_path, F_OK) == 0) {
        (void)snprintf(default_trust_directory, sizeof(default_trust_directory), "%s", resolved_path);
        trust_directory = default_trust_directory;
      }
    }
  }

  result = loader_policy_initialize(profile, trust_directory);
  if (result != ALRIOS_POLICY_OK) {
    fprintf(stderr,
            "Execution policy startup failed closed (profile=%s, trust_dir=%s, rc=%d)\n",
            alrios_profile_name(profile), trust_directory, result);
    return -1;
  }
  policy_initialized = 1;
  alri_printf("  " GRN "✓" RST " Execution profile " CYN "%s" RST
              " sealed\n", alrios_profile_name(profile));
  return 0;
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

  if (initialize_execution_policy() != 0) {
    return 1;
  }

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

  alri_printf("\n  " DIM "◇" RST " Scanning " CYN "/run" RST " (" DIM "%s" RST
              ")\n",
              run_dir);

  loader_scan_phase(run_dir, 0);

  alri_printf("\n  " DIM "◇" RST " Scanning " CYN "/apps" RST " (" DIM "%s" RST
              ")\n",
              apps_dir);

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

  /* Scan and register standalone app processes AFTER services are up */
  loader_scan_phase(apps_dir, 2);

  /* Spawn standalone apps in dependency priority order (ardb -> arauth ->
   * arapiauth -> arcdn -> arenterprise -> home-web) */
  alri_printf("\n  " DIM "◇" RST
              " Spawning autostart applications by priority...\n");
  loader_spawn_autostart_priority();

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
  loader_stop_all();
  ar_shutdown();

  return 0;
}
