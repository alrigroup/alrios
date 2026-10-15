/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include <alrios/platform/aros_hal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

#define ARCC_VERSION_STRING "1.0.0"

static const char *const g_banned_apis[] = {
    "strcpy",
    "strcat",
    "sprintf",
    "vsprintf",
    "gets",
    "system",
    "popen"
};
#define NUM_BANNED_APIS (sizeof(g_banned_apis) / sizeof(g_banned_apis[0]))

static const char *const g_mandatory_flags[] = {
    "-Wall",
    "-Wextra",
    "-Werror",
    "-Wpedantic",
    "-std=c11",
    "-D_FORTIFY_SOURCE=2",
    "-fstack-protector-strong"
};
#define NUM_MANDATORY_FLAGS (sizeof(g_mandatory_flags) / sizeof(g_mandatory_flags[0]))

typedef enum {
    ARCC_POLICY_DEFAULT = 0,
    ARCC_POLICY_STATIC  = 1,
    ARCC_POLICY_SANDBOX = 2,
    ARCC_POLICY_STRICT  = 3
} arcc_policy_t;

typedef enum {
    LEX_STATE_NORMAL,
    LEX_STATE_LINE_COMMENT,
    LEX_STATE_BLOCK_COMMENT,
    LEX_STATE_STRING,
    LEX_STATE_CHAR
} lex_state_t;

static void print_version(void) {
    (void)printf("arcc %s (ALRIOS Strict Compiler Toolchain)\n", ARCC_VERSION_STRING);
    (void)printf("Mandatory Invariant Gates: C11 Strict, Zero Banned APIs, Static/Sandbox Policies Enforced\n");
}

static void print_help(void) {
    (void)printf("ALRIOS Strict Compiler Wrapper (arcc) - MP-024 Toolchain\n\n");
    (void)printf("Usage:\n");
    (void)printf("  arcc [wrapper-options] [compiler-options] <source-files>\n\n");
    (void)printf("Wrapper Options:\n");
    (void)printf("  --help, -h             Show this help message and exit\n");
    (void)printf("  --version, -v          Show arcc version and toolchain info\n");
    (void)printf("  --dump-flags           Print mandatory strict compiler flags\n");
    (void)printf("  --dump-policy          Print active toolchain security policies\n");
    (void)printf("  --check-policy         Alias for --dump-policy\n");
    (void)printf("  --policy=<policy>      Select enforcement policy: static, sandbox, strict, default\n");
    (void)printf("  --static-runtime       Enforce static runtime linking (-static)\n");
    (void)printf("  --sandbox-compat       Enforce sandbox compatibility (PIE/RELRO hardening)\n");
    (void)printf("  --scan                 Scan source files for banned APIs without compiling\n");
    (void)printf("  --dry-run, -###        Perform scans and print underlying command line without executing\n\n");
    (void)printf("Banned C APIs (Rejected at build/scan time with clear diagnostic error):\n");
    (void)printf("  strcpy, strcat, sprintf, vsprintf, gets, system, popen\n\n");
    (void)printf("Mandatory Strict Flags Injected:\n");
    (void)printf("  -Wall -Wextra -Werror -Wpedantic -std=c11 -D_FORTIFY_SOURCE=2 -fstack-protector-strong\n");
}

static void dump_flags(void) {
    for (size_t i = 0; i < NUM_MANDATORY_FLAGS; i++) {
        (void)printf("%s%s", g_mandatory_flags[i], (i + 1 < NUM_MANDATORY_FLAGS) ? " " : "");
    }
    (void)printf("\n");
}

static void dump_policy(arcc_policy_t policy) {
    (void)printf("ALRIOS Compiler Toolchain Policy Specification:\n");
    (void)printf("  Active Policy: %s\n",
        (policy == ARCC_POLICY_STATIC) ? "STATIC_RUNTIME" :
        (policy == ARCC_POLICY_SANDBOX) ? "SANDBOX_COMPAT" :
        (policy == ARCC_POLICY_STRICT) ? "STRICT_SOVEREIGN" : "DEFAULT_STRICT");
    (void)printf("  Static Runtime Linking: %s\n",
        (policy == ARCC_POLICY_STATIC || policy == ARCC_POLICY_STRICT) ? "ENFORCED" : "PERMITTED");
    (void)printf("  Sandbox Compatibility: %s\n",
        (policy == ARCC_POLICY_SANDBOX || policy == ARCC_POLICY_STRICT) ? "ENFORCED" : "PERMITTED");
    (void)printf("  Banned libc APIs: REJECTED (strcpy, strcat, sprintf, vsprintf, gets, system, popen)\n");
    (void)printf("  Mandatory Strict Flags: -Wall -Wextra -Werror -Wpedantic -std=c11 -D_FORTIFY_SOURCE=2 -fstack-protector-strong\n");
}

static bool is_source_file(const char *arg) {
    if (!arg) {
        return false;
    }
    size_t len = strlen(arg);
    if (len >= 2 && strcmp(arg + len - 2, ".c") == 0) return true;
    if (len >= 2 && strcmp(arg + len - 2, ".h") == 0) return true;
    if (len >= 4 && strcmp(arg + len - 4, ".cpp") == 0) return true;
    if (len >= 3 && strcmp(arg + len - 3, ".cc") == 0) return true;
    if (len >= 4 && strcmp(arg + len - 4, ".cxx") == 0) return true;
    if (len >= 2 && strcmp(arg + len - 2, ".C") == 0) return true;
    if (len >= 2 && strcmp(arg + len - 2, ".H") == 0) return true;
    return false;
}

static int scan_source_buffer(const char *filename, const char *src, size_t len, size_t *out_violations) {
    lex_state_t state = LEX_STATE_NORMAL;
    size_t line = 1;
    size_t col = 1;
    size_t i = 0;
    size_t violations = 0;

    while (i < len) {
        char ch = src[i];
        size_t cur_line = line;
        size_t cur_col = col;

        if (ch == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }

        switch (state) {
        case LEX_STATE_NORMAL:
            if (ch == '/' && i + 1 < len && src[i + 1] == '/') {
                state = LEX_STATE_LINE_COMMENT;
                i += 2;
                col++;
                continue;
            } else if (ch == '/' && i + 1 < len && src[i + 1] == '*') {
                state = LEX_STATE_BLOCK_COMMENT;
                i += 2;
                col++;
                continue;
            } else if (ch == '"') {
                state = LEX_STATE_STRING;
            } else if (ch == '\'') {
                state = LEX_STATE_CHAR;
            } else if (isalpha((unsigned char)ch) || ch == '_') {
                char ident[128];
                size_t id_len = 0;
                size_t id_line = cur_line;
                size_t id_col = cur_col;

                ident[id_len++] = ch;
                size_t j = i + 1;
                while (j < len && (isalnum((unsigned char)src[j]) || src[j] == '_')) {
                    if (id_len < sizeof(ident) - 1) {
                        ident[id_len++] = src[j];
                    }
                    j++;
                }
                ident[id_len] = '\0';

                for (size_t b = 0; b < NUM_BANNED_APIS; b++) {
                    if (strcmp(ident, g_banned_apis[b]) == 0) {
                        (void)fprintf(stderr,
                            "[ARCC ERROR] %s:%zu:%zu: Banned libc function '%s()' detected. "
                            "Unsafe C functions are prohibited in ALRIOS.\n",
                            filename, id_line, id_col, ident);
                        violations++;
                        break;
                    }
                }

                size_t adv = j - i;
                col += (adv - 1);
                i = j;
                continue;
            }
            break;

        case LEX_STATE_LINE_COMMENT:
            if (ch == '\n') {
                state = LEX_STATE_NORMAL;
            }
            break;

        case LEX_STATE_BLOCK_COMMENT:
            if (ch == '*' && i + 1 < len && src[i + 1] == '/') {
                state = LEX_STATE_NORMAL;
                i += 2;
                col++;
                continue;
            }
            break;

        case LEX_STATE_STRING:
            if (ch == '\\' && i + 1 < len) {
                i += 2;
                col++;
                continue;
            } else if (ch == '"') {
                state = LEX_STATE_NORMAL;
            }
            break;

        case LEX_STATE_CHAR:
            if (ch == '\\' && i + 1 < len) {
                i += 2;
                col++;
                continue;
            } else if (ch == '\'') {
                state = LEX_STATE_NORMAL;
            }
            break;

        default:
            state = LEX_STATE_NORMAL;
            break;
        }

        i++;
    }

    if (out_violations) {
        *out_violations += violations;
    }
    return (violations > 0) ? -1 : 0;
}

static int scan_file_for_banned_apis(const char *filename, size_t *out_violations) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        (void)fprintf(stderr, "[ARCC ERROR] Cannot open source file '%s': %s\n",
                      filename, strerror(errno));
        if (out_violations) {
            (*out_violations)++;
        }
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        (void)fclose(f);
        if (out_violations) (*out_violations)++;
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0) {
        (void)fclose(f);
        if (out_violations) (*out_violations)++;
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        (void)fclose(f);
        if (out_violations) (*out_violations)++;
        return -1;
    }

    if (sz == 0) {
        (void)fclose(f);
        return 0;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        (void)fclose(f);
        if (out_violations) (*out_violations)++;
        return -1;
    }

    size_t rd = fread(buf, 1, (size_t)sz, f);
    (void)fclose(f);
    buf[rd] = '\0';

    int rc = scan_source_buffer(filename, buf, rd, out_violations);
    free(buf);
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    /* Fast-path help and version queries */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[i], "--dump-flags") == 0 || strcmp(argv[i], "--print-flags") == 0) {
            dump_flags();
            return 0;
        }
    }

    arcc_policy_t policy = ARCC_POLICY_DEFAULT;
    bool scan_only = false;
    bool dry_run = false;
    bool has_opt_flag = false;
    bool is_compile_only = false;
    bool has_static_flag = false;

    /* Parse wrapper options */
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strncmp(arg, "--policy=", 9) == 0) {
            const char *p = arg + 9;
            if (strcmp(p, "static") == 0) {
                policy = ARCC_POLICY_STATIC;
            } else if (strcmp(p, "sandbox") == 0) {
                policy = ARCC_POLICY_SANDBOX;
            } else if (strcmp(p, "strict") == 0) {
                policy = ARCC_POLICY_STRICT;
            } else if (strcmp(p, "default") == 0) {
                policy = ARCC_POLICY_DEFAULT;
            } else {
                (void)fprintf(stderr,
                    "[ARCC ERROR] Unknown policy '%s'. Valid options: static, sandbox, strict, default.\n", p);
                return 1;
            }
        } else if (strcmp(arg, "--static-runtime") == 0 || strcmp(arg, "-static") == 0 || strcmp(arg, "--static") == 0) {
            policy = (policy == ARCC_POLICY_SANDBOX) ? ARCC_POLICY_STRICT : ARCC_POLICY_STATIC;
            has_static_flag = true;
        } else if (strcmp(arg, "--sandbox-compat") == 0) {
            policy = (policy == ARCC_POLICY_STATIC) ? ARCC_POLICY_STRICT : ARCC_POLICY_SANDBOX;
        } else if (strcmp(arg, "--scan") == 0) {
            scan_only = true;
        } else if (strcmp(arg, "--dry-run") == 0 || strcmp(arg, "-###") == 0) {
            dry_run = true;
        } else if (strcmp(arg, "--dump-policy") == 0 || strcmp(arg, "--check-policy") == 0) {
            dump_policy(policy);
            return 0;
        } else if (strncmp(arg, "-O", 2) == 0) {
            has_opt_flag = true;
        } else if (strcmp(arg, "-c") == 0 || strcmp(arg, "-S") == 0 || strcmp(arg, "-E") == 0) {
            is_compile_only = true;
        }
    }

    /* Policy enforcement: check for conflicting or invalid flags */
    if (policy == ARCC_POLICY_STATIC || policy == ARCC_POLICY_STRICT) {
        for (int i = 1; i < argc; i++) {
            const char *arg = argv[i];
            if (strcmp(arg, "-shared") == 0 || strcmp(arg, "-dynamic") == 0 || strcmp(arg, "-rdynamic") == 0) {
                (void)fprintf(stderr,
                    "[ARCC ERROR] Policy violation: Incompatible flag '%s' rejected under static runtime policy.\n",
                    arg);
                return 1;
            }
        }
    }

    if (policy == ARCC_POLICY_SANDBOX || policy == ARCC_POLICY_STRICT) {
        for (int i = 1; i < argc; i++) {
            const char *arg = argv[i];
            if (strcmp(arg, "-no-pie") == 0 && !has_static_flag) {
                (void)fprintf(stderr,
                    "[ARCC ERROR] Policy violation: Flag '%s' violates sandbox compatibility policy.\n",
                    arg);
                return 1;
            }
        }
    }

    /* Step 1: Scan all source files in argument list for banned APIs */
    size_t total_violations = 0;
    size_t scanned_files = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (arg[0] == '-') {
            continue;
        }
        if (scan_only || is_source_file(arg)) {
            scanned_files++;
            (void)scan_file_for_banned_apis(arg, &total_violations);
        }
    }

    if (total_violations > 0) {
        (void)fprintf(stderr,
            "[ARCC ERROR] Build rejected: %zu banned-API violation(s) detected across input source files.\n",
            total_violations);
        return 1;
    }

    if (scan_only) {
        (void)printf("[ARCC OK] Scanned %zu source file(s): zero banned APIs detected.\n", scanned_files);
        return 0;
    }

    /* Step 2: Determine backend compiler */
    const char *backend_cc = getenv("ALRIOS_CC");
    if (!backend_cc || backend_cc[0] == '\0') {
        backend_cc = getenv("CC");
    }
    if (!backend_cc || backend_cc[0] == '\0') {
        backend_cc = "cc";
    }

    /* Step 3: Construct injected compiler arguments */
    size_t max_args = (size_t)argc + NUM_MANDATORY_FLAGS + 16;
    char **exec_argv = (char **)calloc(max_args, sizeof(char *));
    if (!exec_argv) {
        (void)fprintf(stderr, "[ARCC ERROR] Out of memory constructing compiler command line\n");
        return 1;
    }

    size_t out_idx = 0;
    exec_argv[out_idx++] = (char *)backend_cc;

    /* Inject mandatory strict flags */
    for (size_t m = 0; m < NUM_MANDATORY_FLAGS; m++) {
        exec_argv[out_idx++] = (char *)g_mandatory_flags[m];
    }

    /* Inject optimization flag if missing, ensuring -D_FORTIFY_SOURCE=2 is active without warning */
    if (!has_opt_flag) {
        exec_argv[out_idx++] = (char *)"-O2";
    }

    /* Policy-driven injection */
    if ((policy == ARCC_POLICY_STATIC || policy == ARCC_POLICY_STRICT) && !is_compile_only) {
        if (!has_static_flag) {
            exec_argv[out_idx++] = (char *)"-static";
        }
    }

    if ((policy == ARCC_POLICY_SANDBOX || policy == ARCC_POLICY_STRICT) && !has_static_flag && !is_compile_only) {
        exec_argv[out_idx++] = (char *)"-fPIE";
        exec_argv[out_idx++] = (char *)"-pie";
        exec_argv[out_idx++] = (char *)"-Wl,-z,relro,-z,now";
    }

    /* Forward user arguments, filtering wrapper-only options */
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--dry-run") == 0 || strcmp(arg, "-###") == 0 ||
            strcmp(arg, "--scan") == 0 ||
            strncmp(arg, "--policy=", 9) == 0 ||
            strcmp(arg, "--static-runtime") == 0 ||
            strcmp(arg, "--sandbox-compat") == 0 ||
            strcmp(arg, "--dump-policy") == 0 ||
            strcmp(arg, "--check-policy") == 0) {
            continue;
        }
        exec_argv[out_idx++] = (char *)arg;
    }
    exec_argv[out_idx] = NULL;

    /* Step 4: Handle --dry-run */
    if (dry_run) {
        (void)printf("[ARCC EXEC]");
        for (size_t i = 0; i < out_idx; i++) {
            (void)printf(" %s", exec_argv[i]);
        }
        (void)printf("\n");
        free(exec_argv);
        return 0;
    }

    /* Step 5: Execute compiler using safe HAL abstraction */
    int pid = ar_process_create(backend_cc, exec_argv);
    if (pid < 0) {
        (void)fprintf(stderr, "[ARCC ERROR] Failed to spawn backend compiler '%s' (rc=%d)\n",
                      backend_cc, pid);
        free(exec_argv);
        return 1;
    }

    int status = ar_process_wait(pid);
    free(exec_argv);
    return status;
}
