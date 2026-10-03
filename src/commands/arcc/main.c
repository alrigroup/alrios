/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int has_source_suffix(const char *path) {
    const char *extension = strrchr(path, '.');
    return extension && (strcmp(extension, ".c") == 0 || strcmp(extension, ".cc") == 0 ||
                         strcmp(extension, ".cpp") == 0);
}

static int run_process(char *const argv[]) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        return 1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    do {
        if (waitpid(pid, &status, 0) >= 0) {
            break;
        }
    } while (errno == EINTR);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

static int scanner_path(const char *argv0, char output[PATH_MAX]) {
    char executable[PATH_MAX];
    char *directory;
    int written;

    if (!argv0 || !output) {
        return -1;
    }
    if (!realpath(argv0, executable)) {
        return -1;
    }
    directory = dirname(executable);
    written = snprintf(output, PATH_MAX, "%s/../scripts/gates/banned_apis.py", directory);
    return written > 0 && written < PATH_MAX ? 0 : -1;
}

int main(int argc, char *argv[]) {
    static const char *const strict_flags[] = {
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wpedantic",
        "-Wstrict-prototypes", "-D_FORTIFY_SOURCE=3"
    };
    const char *compiler;
    char scanner[PATH_MAX];
    char **compiler_argv;
    int source_count = 0;
    int index;
    int offset;

    if (argc < 2) {
        (void)fprintf(stderr, "usage: arcc [compiler options] <source.c>\n");
        return 64;
    }
    if (scanner_path(argv[0], scanner) != 0) {
        (void)fprintf(stderr, "arcc: unable to resolve banned API scanner\n");
        return 1;
    }
    for (index = 1; index < argc; ++index) {
        if (has_source_suffix(argv[index])) {
            char *scanner_argv[] = {"python3", scanner, argv[index], NULL};
            source_count++;
            if (run_process(scanner_argv) != 0) {
                (void)fprintf(stderr, "arcc: banned API policy rejected %s\n", argv[index]);
                return 1;
            }
        }
    }
    if (source_count == 0) {
        (void)fprintf(stderr, "arcc: at least one C/C++ source input is required\n");
        return 64;
    }

    compiler = getenv("CC");
    if (!compiler || compiler[0] == '\0') {
        compiler = "cc";
    }
    compiler_argv = calloc((size_t)argc + (sizeof(strict_flags) / sizeof(strict_flags[0])) + 1U,
                           sizeof(*compiler_argv));
    if (!compiler_argv) {
        return 1;
    }
    compiler_argv[0] = (char *)compiler;
    offset = 1;
    for (index = 0; index < (int)(sizeof(strict_flags) / sizeof(strict_flags[0])); ++index) {
        compiler_argv[offset++] = (char *)strict_flags[index];
    }
    for (index = 1; index < argc; ++index) {
        compiler_argv[offset++] = argv[index];
    }
    compiler_argv[offset] = NULL;

    index = run_process(compiler_argv);
    free(compiler_argv);
    return index;
}
