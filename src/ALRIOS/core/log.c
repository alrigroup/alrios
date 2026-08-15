/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static int ansi_strip = -1;

static int should_strip_ansi(void) {
    if (ansi_strip == -1) {
#ifdef _WIN32
        ansi_strip = !_isatty(_fileno(stdout)) ? 1 : 0;
#else
        ansi_strip = !isatty(fileno(stdout)) ? 1 : 0;
#endif
    }
    return ansi_strip;
}

static void strip_ansi(const char *in, char *out, size_t out_size) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o < out_size - 1; i++) {
        if (in[i] == '\033' && in[i + 1] == '[') {
            i += 2;
            while (in[i] && !((in[i] >= 'A' && in[i] <= 'Z') ||
                              (in[i] >= 'a' && in[i] <= 'z')))
                i++;
            if (!in[i]) break;
            continue;
        }
        out[o++] = in[i];
    }
    out[o] = '\0';
}

static void do_print(const char *format, va_list args) {
    char buf[8192];
    int len = vsnprintf(buf, sizeof(buf), format, args);
    if (len < 0) return;

    if (should_strip_ansi()) {
        char clean[8192];
        strip_ansi(buf, clean, sizeof(clean));
        printf("%s", clean);
    } else {
        printf("%s", buf);
    }
}

void alri_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    do_print(format, args);
    va_end(args);
}

void alri_print(const char *format, ...) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", t);

    char full[8192];
    if (should_strip_ansi()) {
        snprintf(full, sizeof(full), "[%s] %s", ts, format);
    } else {
        snprintf(full, sizeof(full), DIM "[%s]" RST " %s", ts, format);
    }

    va_list args;
    va_start(args, format);
    do_print(full, args);
    va_end(args);
}

void alri_print_force(const char *format, ...) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", t);

    char full[8192];
    if (should_strip_ansi()) {
        snprintf(full, sizeof(full), "[%s] %s", ts, format);
    } else {
        snprintf(full, sizeof(full), DIM "[%s]" RST " %s", ts, format);
    }

    va_list args;
    va_start(args, format);
    do_print(full, args);
    va_end(args);
}
