/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/armake/package_v2.h"
#include "alrios/package/arapp_v2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#define close_fd _close
#define make_temp_dir(path) _mkdir(path)
#define remove_dir(path) _rmdir(path)
#define remove_file(path) _unlink(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define close_fd close
#define remove_dir(path) rmdir(path)
#define remove_file(path) unlink(path)
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void join_path(char *out, size_t capacity, const char *directory,
                      const char *name) {
    int length = snprintf(out, capacity, "%s/%s", directory, name);
    CHECK(length > 0 && (size_t)length < capacity);
}

static void write_file(const char *path, const uint8_t *data, size_t length) {
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(length == 0U || fwrite(data, 1U, length, file) == length);
    CHECK(fclose(file) == 0);
}

static uint8_t *read_file(const char *path, size_t *out_length) {
    FILE *file = fopen(path, "rb");
    long length;
    uint8_t *data;

    CHECK(file != NULL);
    CHECK(fseek(file, 0L, SEEK_END) == 0);
    length = ftell(file);
    CHECK(length >= 0L);
    CHECK(fseek(file, 0L, SEEK_SET) == 0);
    data = malloc((size_t)length == 0U ? 1U : (size_t)length);
    CHECK(data != NULL);
    CHECK(length == 0L || fread(data, 1U, (size_t)length, file) == (size_t)length);
    CHECK(fclose(file) == 0);
    *out_length = (size_t)length;
    return data;
}

static void make_test_directory(char *directory, size_t capacity) {
#if defined(_WIN32)
    int length = snprintf(directory, capacity, "armake-v2-test-%lu",
                          (unsigned long)_getpid());
    CHECK(length > 0 && (size_t)length < capacity);
    CHECK(make_temp_dir(directory) == 0);
#else
    int length = snprintf(directory, capacity, "/tmp/alrios-armake-v2-XXXXXX");
    int fd;

    CHECK(length > 0 && (size_t)length < capacity);
    fd = mkstemp(directory);
    CHECK(fd >= 0);
    CHECK(close_fd(fd) == 0);
    CHECK(remove_file(directory) == 0);
    CHECK(mkdir(directory, 0700) == 0);
#endif
}

int main(void) {
    static const uint8_t manifest_source[] =
        "{ \"version\": \"1.0.0\", \"app_id\": \"com.example.app\" }\n";
    static const uint8_t payload[] = {0x7fU, 'E', 'L', 'F', 1U, 2U, 3U};
    char directory[512];
    char manifest_path[768];
    char payload_path[768];
    char mutable_path[768];
    char first_output[768];
    char second_output[768];
    char bad_output[768];
    alrios_armake_v2_options_t options;
    alrios_arapp_v2_view_t view;
    uint8_t *first;
    uint8_t *second;
    size_t first_length;
    size_t second_length;
    size_t index;

    make_test_directory(directory, sizeof(directory));
    join_path(manifest_path, sizeof(manifest_path), directory, "manifest.json");
    join_path(payload_path, sizeof(payload_path), directory, "app.bin");
    join_path(mutable_path, sizeof(mutable_path), directory, "state.sqlite");
    join_path(first_output, sizeof(first_output), directory, "first.arapp");
    join_path(second_output, sizeof(second_output), directory, "second.arapp");
    join_path(bad_output, sizeof(bad_output), directory, "bad.arapp");
    write_file(manifest_path, manifest_source, sizeof(manifest_source) - 1U);
    write_file(payload_path, payload, sizeof(payload));
    write_file(mutable_path, payload, sizeof(payload));

    memset(&options, 0, sizeof(options));
    options.target_arch = ALRIOS_ARAPP_V2_ARCH_X86_64;
    options.container_flags = ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                              ALRIOS_ARAPP_V2_FLAG_RING_DEVMODE;
    options.issued_at = 1000U;
    options.expires_at = 2000U;

    CHECK(alrios_armake_package_v2(manifest_path, payload_path, first_output,
                                   &options) == ALRIOS_ARMAKE_V2_OK);
    CHECK(alrios_armake_package_v2(manifest_path, payload_path, second_output,
                                   &options) == ALRIOS_ARMAKE_V2_OK);
    first = read_file(first_output, &first_length);
    second = read_file(second_output, &second_length);
    CHECK(first_length == second_length);
    CHECK(memcmp(first, second, first_length) == 0);
    CHECK(alrios_arapp_v2_validate(first, first_length, 1500U, 0U, &view) ==
          ALRIOS_ARAPP_V2_OK);
    CHECK(view.flags == options.container_flags);
    CHECK(view.manifest_len ==
          strlen("{\"app_id\":\"com.example.app\",\"version\":\"1.0.0\"}"));
    CHECK(memcmp(view.manifest,
                 "{\"app_id\":\"com.example.app\",\"version\":\"1.0.0\"}",
                 view.manifest_len) == 0);
    CHECK(view.ciphertext_len == sizeof(payload));
    CHECK(memcmp(view.ciphertext, payload, sizeof(payload)) == 0);
    for (index = 0U; index < ALRIOS_ARAPP_V2_SIGNATURES_SIZE; ++index) {
        CHECK(first[ALRIOS_ARAPP_V2_PREFIX_SIZE + index] == 0U);
    }
    CHECK(alrios_armake_package_v2(manifest_path, payload_path, first_output,
                                   &options) ==
          ALRIOS_ARMAKE_V2_ERR_OUTPUT_EXISTS);
    CHECK(alrios_armake_package_v2(manifest_path, mutable_path, bad_output,
                                   &options) ==
          ALRIOS_ARMAKE_V2_ERR_MUTABLE_FILE);
    CHECK(fopen(bad_output, "rb") == NULL);

    options.container_flags = ALRIOS_ARAPP_V2_FLAG_PROFILE_ENTERPRISE |
                              ALRIOS_ARAPP_V2_FLAG_RING_SOVEREIGN;
    CHECK(alrios_armake_package_v2(manifest_path, payload_path, bad_output,
                                   &options) == ALRIOS_ARMAKE_V2_ERR_POLICY);

    free(second);
    free(first);
    CHECK(remove_file(second_output) == 0);
    CHECK(remove_file(first_output) == 0);
    CHECK(remove_file(mutable_path) == 0);
    CHECK(remove_file(payload_path) == 0);
    CHECK(remove_file(manifest_path) == 0);
    CHECK(remove_dir(directory) == 0);
    (void)puts("test_armake_v2: PASS");
    return EXIT_SUCCESS;
}
