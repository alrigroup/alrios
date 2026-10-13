/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "aros_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifdef TEST_ASSERT
#undef TEST_ASSERT
#endif
#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "[ASSERTION FAILED] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void get_arcc_path(char *out, size_t out_len) {
    const char *env_path = getenv("ALRIOS_ARCC_BIN");
    if (env_path && env_path[0] != '\0') {
        (void)snprintf(out, out_len, "%s", env_path);
    } else {
        (void)snprintf(out, out_len, "./arcore/arcc");
    }
}

static void make_temp_file(char *out, size_t out_len, const char *prefix, const char *content) {
    static unsigned long counter = 0;
    (void)snprintf(out, out_len, "/tmp/arcc_test_%s_%ld_%lu.c", prefix, (long)getpid(), ++counter);
    FILE *f = fopen(out, "w");
    TEST_ASSERT(f != NULL);
    if (content) {
        size_t len = strlen(content);
        TEST_ASSERT(fwrite(content, 1, len, f) == len);
    }
    (void)fclose(f);
}

static int run_process(char *const argv[]) {
    int pid = ar_process_create(argv[0], argv);
    if (pid < 0) {
        return -1;
    }
    return ar_process_wait(pid);
}

static void test_arcc_version_and_flags(void) {
    char arcc[1024];
    get_arcc_path(arcc, sizeof(arcc));

    (void)printf("[TEST] Testing arcc --version and --dump-flags...\n");

    char *argv_ver[] = { arcc, "--version", NULL };
    int rc = run_process(argv_ver);
    TEST_ASSERT(rc == 0);

    char *argv_flags[] = { arcc, "--dump-flags", NULL };
    rc = run_process(argv_flags);
    TEST_ASSERT(rc == 0);

    char *argv_policy[] = { arcc, "--dump-policy", NULL };
    rc = run_process(argv_policy);
    TEST_ASSERT(rc == 0);
}

static void test_banned_apis_rejection(void) {
    char arcc[1024];
    get_arcc_path(arcc, sizeof(arcc));

    const char *banned_functions[] = {
        "strcpy",
        "strcat",
        "sprintf",
        "vsprintf",
        "gets",
        "system",
        "popen"
    };
    size_t num_banned = sizeof(banned_functions) / sizeof(banned_functions[0]);

    (void)printf("[TEST] Testing rejection of %zu banned libc functions...\n", num_banned);

    for (size_t i = 0; i < num_banned; i++) {
        char src_path[1024];
        char content[1024];
        (void)snprintf(content, sizeof(content),
            "/* Test for banned %s */\n"
            "int main(void) {\n"
            "    void *p = (void *)%s;\n"
            "    (void)p;\n"
            "    return 0;\n"
            "}\n",
            banned_functions[i], banned_functions[i]);

        make_temp_file(src_path, sizeof(src_path), banned_functions[i], content);

        /* Test scan mode */
        char *argv_scan[] = { arcc, "--scan", src_path, NULL };
        int rc = run_process(argv_scan);
        TEST_ASSERT(rc != 0);

        /* Test compile mode */
        char out_path[1050];
        (void)snprintf(out_path, sizeof(out_path), "%s.o", src_path);
        char *argv_compile[] = { arcc, "-c", src_path, "-o", out_path, NULL };
        rc = run_process(argv_compile);
        TEST_ASSERT(rc != 0);

        (void)unlink(src_path);
        (void)unlink(out_path);
    }
}

static void test_valid_source_compilation(void) {
    char arcc[1024];
    get_arcc_path(arcc, sizeof(arcc));

    (void)printf("[TEST] Testing safe C11 code compilation with mandatory strict flags...\n");

    char src_path[1024];
    const char *safe_code =
        "#include <stdio.h>\n"
        "int main(void) {\n"
        "    return 0;\n"
        "}\n";

    make_temp_file(src_path, sizeof(src_path), "clean", safe_code);

    /* 1. Scan mode must succeed */
    char *argv_scan[] = { arcc, "--scan", src_path, NULL };
    int rc = run_process(argv_scan);
    TEST_ASSERT(rc == 0);

    /* 2. Dry-run mode must succeed */
    char out_path[1050];
    (void)snprintf(out_path, sizeof(out_path), "%s.o", src_path);
    char *argv_dry[] = { arcc, "--dry-run", "-c", src_path, "-o", out_path, NULL };
    rc = run_process(argv_dry);
    TEST_ASSERT(rc == 0);

    /* 3. Real compilation must succeed */
    char *argv_compile[] = { arcc, "-c", src_path, "-o", out_path, NULL };
    rc = run_process(argv_compile);
    TEST_ASSERT(rc == 0);

    struct stat st;
    TEST_ASSERT(stat(out_path, &st) == 0 && st.st_size > 0);

    (void)unlink(src_path);
    (void)unlink(out_path);
}

static void test_policy_enforcement(void) {
    char arcc[1024];
    get_arcc_path(arcc, sizeof(arcc));

    (void)printf("[TEST] Testing static runtime and sandbox compatibility policies...\n");

    char src_path[1024];
    const char *safe_code =
        "int main(void) {\n"
        "    return 0;\n"
        "}\n";
    make_temp_file(src_path, sizeof(src_path), "policy", safe_code);

    /* Test 1: Static policy rejects -shared / -dynamic */
    char *argv_static_bad[] = { arcc, "--policy=static", "-shared", src_path, "-o", "/dev/null", NULL };
    int rc = run_process(argv_static_bad);
    TEST_ASSERT(rc != 0);

    char *argv_static_bad2[] = { arcc, "--static-runtime", "-rdynamic", src_path, "-o", "/dev/null", NULL };
    rc = run_process(argv_static_bad2);
    TEST_ASSERT(rc != 0);

    /* Test 2: Sandbox policy rejects -no-pie */
    char *argv_sandbox_bad[] = { arcc, "--policy=sandbox", "-no-pie", src_path, "-o", "/dev/null", NULL };
    rc = run_process(argv_sandbox_bad);
    TEST_ASSERT(rc != 0);

    /* Test 3: Unknown policy rejects gracefully */
    char *argv_unknown_policy[] = { arcc, "--policy=nonexistent", src_path, "-o", "/dev/null", NULL };
    rc = run_process(argv_unknown_policy);
    TEST_ASSERT(rc != 0);

    /* Test 4: Static policy accepts valid dry-run invocation */
    char *argv_static_ok[] = { arcc, "--policy=static", "--dry-run", src_path, "-o", "/dev/null", NULL };
    rc = run_process(argv_static_ok);
    TEST_ASSERT(rc == 0);

    /* Test 5: Sandbox policy accepts valid dry-run invocation */
    char *argv_sandbox_ok[] = { arcc, "--policy=sandbox", "--dry-run", src_path, "-o", "/dev/null", NULL };
    rc = run_process(argv_sandbox_ok);
    TEST_ASSERT(rc == 0);

    /* Test 6: Policy dump for all variants */
    char *argv_p1[] = { arcc, "--policy=static", "--dump-policy", NULL };
    TEST_ASSERT(run_process(argv_p1) == 0);

    char *argv_p2[] = { arcc, "--policy=sandbox", "--dump-policy", NULL };
    TEST_ASSERT(run_process(argv_p2) == 0);

    char *argv_p3[] = { arcc, "--policy=strict", "--dump-policy", NULL };
    TEST_ASSERT(run_process(argv_p3) == 0);

    (void)unlink(src_path);
}

static void test_banned_apis_py_scanner(void) {
    const char *scanner_path = getenv("ALRIOS_BANNED_APIS_PY");
    if (!scanner_path || scanner_path[0] == '\0') {
        scanner_path = "scripts/gates/banned_apis.py";
    }

    (void)printf("[TEST] Testing scripts/gates/banned_apis.py python scanner gate...\n");

    /* 1. Test clean file passes */
    char clean_file[1024];
    make_temp_file(clean_file, sizeof(clean_file), "py_clean", "int main(void) { return 0; }\n");
    char *argv_clean[] = { "/usr/bin/python3", (char *)scanner_path, clean_file, NULL };
    int rc = run_process(argv_clean);
    TEST_ASSERT(rc == 0);
    (void)unlink(clean_file);

    /* 2. Test banned API files fail */
    const char *banned_fns[] = {
        "strcpy", "strcat", "sprintf", "vsprintf", "gets", "system", "popen"
    };
    for (size_t i = 0; i < sizeof(banned_fns) / sizeof(banned_fns[0]); i++) {
        char bad_file[1024];
        char content[256];
        (void)snprintf(content, sizeof(content), "void f(void) { %s(); }\n", banned_fns[i]);
        make_temp_file(bad_file, sizeof(bad_file), "py_bad", content);

        char *argv_bad[] = { "/usr/bin/python3", (char *)scanner_path, bad_file, NULL };
        rc = run_process(argv_bad);
        TEST_ASSERT(rc != 0);
        (void)unlink(bad_file);
    }
}

int main(void) {
    (void)printf("====================================================\n");
    (void)printf("ALRIOS MP-024 Toolchain Integration Test Suite: arcc\n");
    (void)printf("====================================================\n");

    test_arcc_version_and_flags();
    test_banned_apis_rejection();
    test_banned_apis_py_scanner();
    test_valid_source_compilation();
    test_policy_enforcement();

    (void)printf("[SUCCESS] All MP-024 arcc toolchain integration tests passed.\n");
    return 0;
}
