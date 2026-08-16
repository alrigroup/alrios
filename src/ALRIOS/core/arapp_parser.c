/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapp_parser.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/utsname.h>
#endif

void ar_platform_detect(char *out, int max) {
#ifdef _WIN32
    (void)snprintf(out, max, "windows");
#else
    struct utsname u;
    if (uname(&u) == 0) {
        const char *mach = u.machine;
        if (strcmp(mach, "x86_64") == 0 || strcmp(mach, "amd64") == 0)
            snprintf(out, max, "linux-x64");
        else if (strcmp(mach, "aarch64") == 0 || strcmp(mach, "arm64") == 0)
            snprintf(out, max, "linux-arm64");
        else
            snprintf(out, max, "linux-%s", mach);
    } else {
        snprintf(out, max, "linux-unknown");
    }
#endif
}

static const char *skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    return p;
}

static const char *parse_string(const char *p, char *out, int max) {
    p = skip_ws(p);
    if (*p != '"') return NULL;
    p++;
    const char *start = p;
    while (*p && *p != '"') {
        if (*p == '\\') p++;
        p++;
    }
    if (*p != '"') return NULL;
    int len = (int)(p - start);
    if (len >= max) len = max - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return p + 1;
}

static const char *skip_value(const char *p) {
    p = skip_ws(p);
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (*p == '\\') p++;
            p++;
        }
        if (*p == '"') p++;
        return p;
    }
    if (*p == '{' || *p == '[') {
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '{' || *p == '[') depth++;
            else if (*p == '}' || *p == ']') depth--;
            if (depth > 0) p++;
        }
        if (*p) p++;
        return p;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']' && *p != '\n')
        p++;
    return p;
}

static const char *parse_bool(const char *p, int *out) {
    p = skip_ws(p);
    if (*p == 't' || *p == 'T') {
        *out = 1;
        /* skip "true" */
        while (*p && *p != ',' && *p != '}' && *p != ']' && *p != '\n')
            p++;
    } else if (*p == 'f' || *p == 'F') {
        *out = 0;
        while (*p && *p != ',' && *p != '}' && *p != ']' && *p != '\n')
            p++;
    } else if (*p == '1') {
        *out = 1;
        p++;
    } else if (*p == '0') {
        *out = 0;
        p++;
    }
    return p;
}

int ar_manifest_parse(const char *json, ar_app_manifest_t *out) {
    if (!json || !out) return -1;

    memset(out, 0, sizeof(*out));
    out->runtime = AR_RUNTIME_UNKNOWN;

    const char *p = skip_ws(json);
    if (*p != '{') return -1;
    p++;

    while (*p) {
        p = skip_ws(p);
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }

        char key[64];
        p = parse_string(p, key, sizeof(key));
        if (!p) return -1;

        p = skip_ws(p);
        if (*p != ':') return -1;
        p++;

        if (strcmp(key, "name") == 0)
            p = parse_string(p, out->name, AR_APP_NAME_MAX);
        else if (strcmp(key, "version") == 0)
            p = parse_string(p, out->version, AR_APP_VERSION_MAX);
        else if (strcmp(key, "runtime") == 0) {
            char buf[AR_RUNTIME_MAX];
            p = parse_string(p, buf, AR_RUNTIME_MAX);
            if (strcmp(buf, "native") == 0)
                out->runtime = AR_RUNTIME_NATIVE;
            else if (strcmp(buf, "python3") == 0)
                out->runtime = AR_RUNTIME_PYTHON3;
            else if (strcmp(buf, "node") == 0)
                out->runtime = AR_RUNTIME_NODE;
            else if (strcmp(buf, "java") == 0)
                out->runtime = AR_RUNTIME_JAVA;
            else if (strcmp(buf, "lua") == 0)
                out->runtime = AR_RUNTIME_LUA;
            else if (strcmp(buf, "ruby") == 0)
                out->runtime = AR_RUNTIME_RUBY;
            else if (strcmp(buf, "go") == 0)
                out->runtime = AR_RUNTIME_GO;
        }
        else if (strcmp(key, "entry") == 0)
            p = parse_string(p, out->entry, AR_ENTRY_MAX);
        else if (strcmp(key, "bin") == 0)
            p = parse_string(p, out->bin, AR_FILE_PATH_MAX);
        else if (strcmp(key, "is_runtime") == 0)
            p = parse_bool(p, &out->is_runtime);
        else if (strcmp(key, "requires") == 0) {
            p = skip_ws(p);
            if (*p != '[') return -1;
            p++;
            while (*p) {
                p = skip_ws(p);
                if (*p == ']') { p++; break; }
                if (*p == ',') { p++; continue; }
                char *req = out->requires[out->requires_count];
                p = parse_string(p, req, AR_REQUIRE_MAX);
                if (!p) return -1;
                if (req[0])
                    out->requires_count++;
                if (out->requires_count >= AR_MAX_REQUIRES) break;
            }
        }
        else if (strcmp(key, "platforms") == 0) {
            p = skip_ws(p);
            if (*p == '{') {
                int depth = 1;
                const char *start = p;
                p++;
                while (*p && depth > 0) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                    if (depth > 0) p++;
                }
                if (*p) p++;
                int len = (int)(p - start);
                if (len >= AR_PLATFORM_RAW_MAX)
                    len = AR_PLATFORM_RAW_MAX - 1;
                memcpy(out->platforms_raw, start, len);
                out->platforms_raw[len] = '\0';
            } else {
                p = skip_value(p);
            }
        }
        else if (strcmp(key, "files") == 0) {
            p = skip_ws(p);
            if (*p != '[') return -1;
            p++;
            while (*p) {
                p = skip_ws(p);
                if (*p == ']') { p++; break; }
                if (*p == ',') { p++; continue; }
                char *file = out->files[out->file_count];
                p = parse_string(p, file, AR_FILE_PATH_MAX);
                if (!p) return -1;
                if (file[0])
                    out->file_count++;
                if (out->file_count >= AR_MAX_FILES) break;
            }
        }
        else if (strcmp(key, "files_windows") == 0) {
            p = skip_ws(p);
            if (*p != '[') return -1;
            p++;
            while (*p) {
                p = skip_ws(p);
                if (*p == ']') { p++; break; }
                if (*p == ',') { p++; continue; }
                char *file = out->files_windows[out->files_windows_count];
                p = parse_string(p, file, AR_FILE_PATH_MAX);
                if (!p) return -1;
                if (file[0])
                    out->files_windows_count++;
                if (out->files_windows_count >= AR_MAX_FILES) break;
            }
        }
        else if (strcmp(key, "files_linux") == 0) {
            p = skip_ws(p);
            if (*p != '[') return -1;
            p++;
            while (*p) {
                p = skip_ws(p);
                if (*p == ']') { p++; break; }
                if (*p == ',') { p++; continue; }
                char *file = out->files_linux[out->files_linux_count];
                p = parse_string(p, file, AR_FILE_PATH_MAX);
                if (!p) return -1;
                if (file[0])
                    out->files_linux_count++;
                if (out->files_linux_count >= AR_MAX_FILES) break;
            }
        }
        else if (strcmp(key, "services") == 0) {
            p = skip_ws(p);
            if (*p != '[') return -1;
            p++;
            while (*p) {
                p = skip_ws(p);
                if (*p == ']') { p++; break; }
                if (*p == ',') { p++; continue; }

                if (*p != '{') return -1;
                p++;
                ar_service_def_t *svc = &out->services[out->service_count];
                memset(svc, 0, sizeof(*svc));

                while (*p) {
                    p = skip_ws(p);
                    if (*p == '}') { p++; break; }
                    if (*p == ',') { p++; continue; }

                    char sk[64];
                    p = parse_string(p, sk, sizeof(sk));
                    if (!p) return -1;
                    p = skip_ws(p);
                    if (*p != ':') return -1;
                    p++;

                    if (strcmp(sk, "name") == 0)
                        p = parse_string(p, svc->name, AR_SERVICE_NAME_MAX);
                    else if (strcmp(sk, "entry") == 0)
                        p = parse_string(p, svc->entry, AR_ENTRY_MAX);
                    else
                        p = skip_value(p);
                }

                if (svc->name[0])
                    out->service_count++;
                if (out->service_count >= AR_APP_MAX_SERVICES) break;
            }
        }
        else if (strcmp(key, "build") == 0) {
            p = skip_ws(p);
            if (*p == '{') {
                p++;
                while (*p) {
                    p = skip_ws(p);
                    if (*p == '}') { p++; break; }
                    if (*p == ',') { p++; continue; }
                    char bk[64];
                    p = parse_string(p, bk, sizeof(bk));
                    if (!p) return -1;
                    p = skip_ws(p);
                    if (*p != ':') return -1;
                    p++;
                    p = skip_ws(p);

                    if (strcmp(bk, "command") == 0) {
                        /* Legado: build.command */
                        p = parse_string(p, out->build.command, sizeof(out->build.command));

                    } else if (strcmp(bk, "staging") == 0) {
                        p = parse_string(p, out->build.staging, sizeof(out->build.staging));

                    } else if (strcmp(bk, "cleanup") == 0) {
                        /* build.cleanup: array de strings */
                        if (*p == '[') {
                            p++;
                            while (*p && out->build.cleanup_count < AR_BUILD_CLEANUP_MAX) {
                                p = skip_ws(p);
                                if (*p == ']') { p++; break; }
                                if (*p == ',') { p++; continue; }
                                p = parse_string(p, out->build.cleanup[out->build.cleanup_count],
                                                 AR_BUILD_CLEANUP_PATH);
                                if (!p) return -1;
                                out->build.cleanup_count++;
                            }
                        } else {
                            p = skip_value(p);
                        }

                    } else if (strcmp(bk, "steps") == 0) {
                        /* build.steps: array de { name, cmd, cwd } */
                        if (*p == '[') {
                            p++;
                            while (*p && out->build.step_count < AR_BUILD_MAX_STEPS) {
                                p = skip_ws(p);
                                if (*p == ']') { p++; break; }
                                if (*p == ',') { p++; continue; }
                                if (*p != '{') { p = skip_value(p); continue; }
                                p++;
                                ar_build_step_t *step = &out->build.steps[out->build.step_count];
                                while (*p) {
                                    p = skip_ws(p);
                                    if (*p == '}') { p++; break; }
                                    if (*p == ',') { p++; continue; }
                                    char sk[64];
                                    p = parse_string(p, sk, sizeof(sk));
                                    if (!p) return -1;
                                    p = skip_ws(p);
                                    if (*p != ':') return -1;
                                    p++;
                                    if (strcmp(sk, "name") == 0)
                                        p = parse_string(p, step->name, sizeof(step->name));
                                    else if (strcmp(sk, "cmd") == 0)
                                        p = parse_string(p, step->cmd, sizeof(step->cmd));
                                    else if (strcmp(sk, "cwd") == 0)
                                        p = parse_string(p, step->cwd, sizeof(step->cwd));
                                    else
                                        p = skip_value(p);
                                    if (!p) return -1;
                                }
                                if (step->cmd[0])
                                    out->build.step_count++;
                            }
                        } else {
                            p = skip_value(p);
                        }

                    } else {
                        p = skip_value(p);
                    }

                    if (!p) return -1;
                }
            } else {
                p = skip_value(p);
            }
        }
        else {
            p = skip_value(p);
        }
    }

    return 0;
}

int ar_manifest_get_platform_entry(const ar_app_manifest_t *m,
                                        const char *platform,
                                        char *out, int max) {
    if (!m || !m->platforms_raw[0] || !platform || !out) return -1;

    const char *p = m->platforms_raw;
    if (*p != '{') return -1;
    p++;

    while (*p) {
        p = skip_ws(p);
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }

        char key[64];
        p = parse_string(p, key, sizeof(key));
        if (!p) return -1;
        p = skip_ws(p);
        if (*p != ':') return -1;
        p++;

        p = skip_ws(p);
        if (*p == '{') {
            p++;
            /* parse entry field inside platform object */
            char entry_val[AR_ENTRY_MAX] = {0};
            while (*p) {
                p = skip_ws(p);
                if (*p == '}') { p++; break; }
                if (*p == ',') { p++; continue; }

                char sk[64];
                p = parse_string(p, sk, sizeof(sk));
                if (!p) return -1;
                p = skip_ws(p);
                if (*p != ':') return -1;
                p++;

                if (strcmp(sk, "entry") == 0) {
                    p = parse_string(p, entry_val, AR_ENTRY_MAX);
                } else {
                    p = skip_value(p);
                }
            }

            if (strcmp(key, platform) == 0 && entry_val[0]) {
                snprintf(out, max, "%s", entry_val);
                return 0;
            }
        } else {
            p = skip_value(p);
        }
    }

    return -1;
}
