/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "aros_hal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "FAIL: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static int contains_token(const char *value, const char *token) {
    return value != NULL && token != NULL && strstr(value, token) != NULL;
}

#ifndef _WIN32
static void write_or_die(const char *path, const char *data) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    CHECK(fd >= 0);
    CHECK(write(fd, data, strlen(data)) == (ssize_t)strlen(data));
    CHECK(close(fd) == 0);
}

static int try_read_file(const char *path) {
    char buf[8];
    ssize_t n;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    n = read(fd, buf, sizeof(buf));
    if (n < 0) {
        (void)close(fd);
        return -1;
    }
    CHECK(close(fd) == 0);
    return 0;
}

static void join_path_or_die(char *out, size_t out_cap, const char *dir, const char *name) {
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    CHECK(dir_len + 1U + name_len + 1U <= out_cap);
    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1U, name, name_len);
    out[dir_len + 1U + name_len] = '\0';
}

static int run_command(const char *cmd) {
    /* Desabilitar leak detection herdada ao invocar subprocessos de teste sob ptrace/sanitizer */
    char full_cmd[16384 + 64];
    snprintf(full_cmd, sizeof(full_cmd), "ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0 %s", cmd);
    int rc = system(full_cmd);
    CHECK(rc != -1);
    return (WIFEXITED(rc)) ? WEXITSTATUS(rc) : 255;
}

static void verify_meaningful_key_boundary(void) {
    char tmpl[] = "/tmp/alrios-armake-iso-XXXXXX";
    char *base = mkdtemp(tmpl);
    char app_dir[4096];
    char keys_dir[4096];
    char app_file[4096];
    char key_file[4096];
    pid_t child;
    int status = 0;

    CHECK(base != NULL);
    join_path_or_die(app_dir, sizeof(app_dir), base, "app");
    join_path_or_die(keys_dir, sizeof(keys_dir), base, "keys");
    CHECK(mkdir(app_dir, 0700) == 0);
    CHECK(mkdir(keys_dir, 0700) == 0);
    join_path_or_die(app_file, sizeof(app_file), app_dir, "index.bin");
    join_path_or_die(key_file, sizeof(key_file), keys_dir, "signing.material");
    write_or_die(app_file, "payload");
    write_or_die(key_file, "not-a-real-secret");

    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        const char *read_roots[] = { app_dir };
        int rc = ar_fs_restrict_to_paths(read_roots, 1U, NULL, 0U);
        if (rc != 0) {
            _exit(100);
        }
        if (try_read_file(app_file) != 0) {
            _exit(101);
        }
        if (try_read_file(key_file) == 0) {
            _exit(102);
        }
        _exit(0);
    }

    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == 100);
    if (WEXITSTATUS(status) == 100) {
        (void)printf("[INFO] Landlock unavailable; relying on armake canonical path validation fallback\n");
    }
}

static void verify_armake_real_packaging_boundary(void) {
    const char *armake = getenv("ALRIOS_ARMAKE_BIN");
    char tmpl[] = "/tmp/alrios-armake-real-XXXXXX";
    char *base = mkdtemp(tmpl);
    char manifest[4096];
    char payload[4096];
    char key_file[4096];
    char output_ok[4096];
    char output_bad[4096];
    char cmd[16384];

    CHECK(armake != NULL && armake[0] != '\0');
    CHECK(base != NULL);
    join_path_or_die(manifest, sizeof(manifest), base, "app.arappmake");
    join_path_or_die(payload, sizeof(payload), base, "payload.bin");
    join_path_or_die(key_file, sizeof(key_file), base, "key.pem");
    join_path_or_die(output_ok, sizeof(output_ok), base, "ok.arapp");
    join_path_or_die(output_bad, sizeof(output_bad), base, "bad.arapp");

    write_or_die(payload, "payload");
    write_or_die(key_file, "secret-key-material");
    write_or_die(manifest, "{\"name\":\"probe\",\"version\":\"1.0\",\"entry\":\"payload.bin\",\"files\":[\"payload.bin\"]}");
    CHECK(snprintf(cmd, sizeof(cmd), "'%s' build '%s' '%s'", armake, base, output_ok) > 0);
    CHECK(run_command(cmd) == 0);
    CHECK(try_read_file(output_ok) == 0);

    write_or_die(manifest, "{\"name\":\"probe\",\"version\":\"1.0\",\"entry\":\"payload.bin\",\"files\":[\"key.pem\"]}");
    CHECK(snprintf(cmd, sizeof(cmd), "'%s' build '%s' '%s'", armake, base, output_bad) > 0);
    CHECK(run_command(cmd) != 0);
    CHECK(try_read_file(output_bad) != 0);
}
#endif

int main(void) {
    const char *libs = getenv("ALRIOS_ARMAKE_LINK_LIBRARIES");
    const char *sources = getenv("ALRIOS_ARMAKE_SOURCES");
    const char *isolation_sources = getenv("ALRIOS_ARHAL_ISOLATION_SOURCES");

    CHECK(libs != NULL);
    CHECK(sources != NULL);
    CHECK(isolation_sources != NULL);

    CHECK(!contains_token(libs, "arpki"));
    CHECK(!contains_token(libs, "arcrypto"));
    CHECK(!contains_token(libs, "OpenSSL::Crypto"));
    CHECK(!contains_token(libs, "OpenSSL::SSL"));
    CHECK(!contains_token(libs, "EVP"));
    CHECK(!contains_token(libs, "PEM"));

    CHECK(!contains_token(sources, "notary"));
    CHECK(!contains_token(sources, "pki"));
    CHECK(contains_token(libs, "arhal_isolation"));
    CHECK(contains_token(isolation_sources, "isolation.c"));

#ifndef _WIN32
    verify_meaningful_key_boundary();
    verify_armake_real_packaging_boundary();
#endif

    (void)printf("[PASS] armake build graph and filesystem boundary exclude private-key access\n");
    return 0;
}
