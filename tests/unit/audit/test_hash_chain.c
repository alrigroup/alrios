/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/audit/block.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define close_file _close
#define open_file _open
#define seek_file _lseeki64
#define write_file _write
#else
#include <pthread.h>
#include <unistd.h>
#define close_file close
#define open_file open
#define seek_file lseek
#define write_file write
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define HEADER_PAYLOAD_SIZE_OFFSET 12U
#define HEADER_TIMESTAMP_OFFSET 24U
#define HEADER_PREVIOUS_HASH_OFFSET 32U
#define HEADER_HASH_OFFSET 64U
#define TRAILER_MAGIC_SIZE 8U
#define CONCURRENT_THREAD_COUNT 8U
#define APPENDS_PER_THREAD 32U

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static char test_directory[512];

static void initialize_test_directory(void) {
#ifdef _WIN32
    char temporary_path[MAX_PATH];
    char temporary_name[MAX_PATH];
    DWORD path_length = GetTempPathA(MAX_PATH, temporary_path);
    UINT result;
    int written;

    CHECK(path_length > 0U && path_length < MAX_PATH);
    result = GetTempFileNameA(temporary_path, "ara", 0U, temporary_name);
    CHECK(result != 0U);
    CHECK(DeleteFileA(temporary_name) != 0);
    CHECK(CreateDirectoryA(temporary_name, NULL) != 0);
    written = snprintf(test_directory, sizeof(test_directory), "%s", temporary_name);
    CHECK(written >= 0 && (size_t)written < sizeof(test_directory));
#else
    int written = snprintf(test_directory,
                           sizeof(test_directory),
                           "/tmp/alrios-audit-test-XXXXXX");

    CHECK(written >= 0 && (size_t)written < sizeof(test_directory));
    CHECK(mkdtemp(test_directory) != NULL);
#endif
}

static void make_path(char *path, size_t capacity, const char *name) {
#ifdef _WIN32
    int result = snprintf(path, capacity, "%s\\%s", test_directory, name);
#else
    int result = snprintf(path, capacity, "%s/%s", test_directory, name);
#endif
    CHECK(result >= 0);
    CHECK((size_t)result < capacity);
}

static uint64_t file_size(const char *path) {
#ifdef _WIN32
    struct _stat64 status;

    CHECK(_stat64(path, &status) == 0);
    CHECK(status.st_size >= 0);
#else
    struct stat status;

    CHECK(stat(path, &status) == 0);
    CHECK(status.st_size >= 0);
#endif
    return (uint64_t)status.st_size;
}

static void overwrite_byte(const char *path, uint64_t offset, uint8_t value) {
#ifdef _WIN32
    __int64 converted = (__int64)offset;
#else
    off_t converted = (off_t)offset;
#endif
    int fd = open_file(path, O_WRONLY | O_BINARY);

    CHECK(fd >= 0);
    CHECK(converted >= 0 && (uint64_t)converted == offset);
    CHECK(seek_file(fd, converted, SEEK_SET) == converted);
    CHECK(write_file(fd, &value, 1U) == 1);
#ifdef _WIN32
    CHECK(_commit(fd) == 0);
#else
    CHECK(fsync(fd) == 0);
#endif
    CHECK(close_file(fd) == 0);
}

static void append_bytes(const char *path, const uint8_t *bytes, size_t length) {
    int fd = open_file(path,
                       O_WRONLY | O_APPEND | O_CREAT | O_BINARY,
#ifdef _WIN32
                       _S_IREAD | _S_IWRITE
#else
                       0600
#endif
    );
    size_t written = 0U;

    CHECK(fd >= 0);
    while (written < length) {
#ifdef _WIN32
        int count = write_file(fd,
                               bytes + written,
                               (unsigned int)(length - written));
#else
        ssize_t count = write_file(fd, bytes + written, length - written);
#endif
        CHECK(count > 0);
        written += (size_t)count;
    }
#ifdef _WIN32
    CHECK(_commit(fd) == 0);
#else
    CHECK(fsync(fd) == 0);
#endif
    CHECK(close_file(fd) == 0);
}

static void copy_prefix(const char *source, const char *destination, uint64_t length) {
    uint8_t buffer[4096];
    uint64_t remaining = length;
    int source_fd = open_file(source, O_RDONLY | O_BINARY);
    int destination_fd = open_file(destination,
                                   O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
#ifdef _WIN32
                                   _S_IREAD | _S_IWRITE
#else
                                   0600
#endif
    );

    CHECK(source_fd >= 0);
    CHECK(destination_fd >= 0);
    while (remaining > 0U) {
        size_t chunk_size = remaining < sizeof(buffer)
                                ? (size_t)remaining
                                : sizeof(buffer);
#ifdef _WIN32
        int read_count = _read(source_fd, buffer, (unsigned int)chunk_size);
        int write_count;
#else
        ssize_t read_count = read(source_fd, buffer, chunk_size);
        ssize_t write_count;
#endif
        CHECK(read_count == (int)chunk_size);
        write_count = write_file(destination_fd,
                                 buffer,
#ifdef _WIN32
                                 (unsigned int)chunk_size
#else
                                 chunk_size
#endif
        );
        CHECK(write_count == (int)chunk_size);
        remaining -= (uint64_t)chunk_size;
    }
#ifdef _WIN32
    CHECK(_commit(destination_fd) == 0);
#else
    CHECK(fsync(destination_fd) == 0);
#endif
    CHECK(close_file(destination_fd) == 0);
    CHECK(close_file(source_fd) == 0);
}

static void test_hash_known_answer(void) {
    static const uint8_t payload[] = "deterministic-audit-event";
    static const uint8_t expected[ALRIOS_AUDIT_HASH_SIZE] = {
        0x76U, 0xdaU, 0x30U, 0x0aU, 0x8aU, 0x0eU, 0xccU, 0x3bU,
        0x9fU, 0xa0U, 0x5eU, 0x01U, 0x4fU, 0x90U, 0xa6U, 0x66U,
        0xa3U, 0xebU, 0xadU, 0x6aU, 0x15U, 0xd3U, 0x65U, 0x55U,
        0x4fU, 0x5fU, 0x05U, 0x3dU, 0xa3U, 0x9aU, 0x3aU, 0xb1U
    };
    uint8_t previous_hash[ALRIOS_AUDIT_HASH_SIZE] = {0};
    uint8_t actual[ALRIOS_AUDIT_HASH_SIZE];

    CHECK(alrios_audit_compute_hash(UINT64_C(7), UINT64_C(0x0102030405060708),
                                    previous_hash, payload,
                                    sizeof(payload) - 1U, actual) ==
          ALRIOS_AUDIT_OK);
    CHECK(memcmp(actual, expected, sizeof(expected)) == 0);
    CHECK(alrios_audit_compute_hash(0U, 0U, previous_hash, NULL, 1U, actual) ==
          ALRIOS_AUDIT_ERR_INVALID_ARGUMENT);
}

static void test_append_and_verify_links(void) {
    static const uint8_t first_payload[] = "login accepted";
    static const uint8_t second_payload[] = "policy applied";
    char path[512];
    alrios_audit_block_t first;
    alrios_audit_block_t second;
    alrios_audit_verify_result_t result;
    uint8_t zero_hash[ALRIOS_AUDIT_HASH_SIZE] = {0};

    make_path(path, sizeof(path), "links.audit");
    CHECK(alrios_audit_append(path, 1000U, first_payload,
                              sizeof(first_payload) - 1U, &first) ==
          ALRIOS_AUDIT_OK);
    CHECK(alrios_audit_append(path, 1001U, second_payload,
                              sizeof(second_payload) - 1U, &second) ==
          ALRIOS_AUDIT_OK);
    CHECK(first.sequence == 0U);
    CHECK(second.sequence == 1U);
    CHECK(memcmp(first.previous_hash, zero_hash, sizeof(zero_hash)) == 0);
    CHECK(memcmp(second.previous_hash, first.hash, sizeof(first.hash)) == 0);
    CHECK(alrios_audit_verify(path, &result) == ALRIOS_AUDIT_OK);
    CHECK(result.block_count == 2U);
    CHECK(result.has_incomplete_tail == 0);
    CHECK(memcmp(result.last_hash, second.hash, sizeof(second.hash)) == 0);
    CHECK(result.valid_bytes == file_size(path));
}

static void test_mutation_detected(void) {
    static const uint8_t payload[] = "immutable event";
    static const uint64_t mutation_offsets[] = {
        HEADER_TIMESTAMP_OFFSET,
        HEADER_HASH_OFFSET,
        ALRIOS_AUDIT_RECORD_HEADER_SIZE,
        ALRIOS_AUDIT_RECORD_HEADER_SIZE + sizeof(payload) - 1U,
        ALRIOS_AUDIT_RECORD_HEADER_SIZE + sizeof(payload) - 1U +
            TRAILER_MAGIC_SIZE
    };
    size_t index;

    for (index = 0U;
         index < sizeof(mutation_offsets) / sizeof(mutation_offsets[0]);
         ++index) {
        char path[512];
        char name[64];
        alrios_audit_verify_result_t result;
        int written = snprintf(name, sizeof(name), "mutation-%u.audit",
                               (unsigned int)index);

        CHECK(written >= 0 && (size_t)written < sizeof(name));
        make_path(path, sizeof(path), name);
        CHECK(alrios_audit_append(path, 2000U, payload,
                                  sizeof(payload) - 1U, NULL) ==
              ALRIOS_AUDIT_OK);
        overwrite_byte(path, mutation_offsets[index], 0x5aU);
        CHECK(alrios_audit_verify(path, &result) != ALRIOS_AUDIT_OK);
        CHECK(alrios_audit_append(path, 2001U, payload,
                                  sizeof(payload) - 1U, NULL) !=
              ALRIOS_AUDIT_OK);
    }

    {
        char path[512];
        alrios_audit_verify_result_t result;

        make_path(path, sizeof(path), "mutated-link.audit");
        CHECK(alrios_audit_append(path, 3000U, payload,
                                  sizeof(payload) - 1U, NULL) ==
              ALRIOS_AUDIT_OK);
        CHECK(alrios_audit_append(path, 3001U, payload,
                                  sizeof(payload) - 1U, NULL) ==
              ALRIOS_AUDIT_OK);
        overwrite_byte(path,
                       ALRIOS_AUDIT_RECORD_OVERHEAD + sizeof(payload) - 1U +
                           HEADER_PREVIOUS_HASH_OFFSET,
                       0x5aU);
        CHECK(alrios_audit_verify(path, &result) ==
              ALRIOS_AUDIT_ERR_SEQUENCE);
    }
}

static void test_header_length_mutation_is_not_crash_tail(void) {
    static const uint8_t payload[] = "payload length integrity";
    char larger_path[512];
    char smaller_path[512];
    alrios_audit_verify_result_t result;
    uint64_t original_size;

    make_path(larger_path, sizeof(larger_path), "payload-size-larger.audit");
    CHECK(alrios_audit_append(larger_path, 3100U, payload,
                              sizeof(payload) - 1U, NULL) == ALRIOS_AUDIT_OK);
    original_size = file_size(larger_path);
    overwrite_byte(larger_path, HEADER_PAYLOAD_SIZE_OFFSET + 3U,
                   (uint8_t)sizeof(payload));
    CHECK(alrios_audit_verify(larger_path, &result) == ALRIOS_AUDIT_ERR_FORMAT);
    CHECK(alrios_audit_append(larger_path, 3101U, payload,
                              sizeof(payload) - 1U, NULL) ==
          ALRIOS_AUDIT_ERR_FORMAT);
    CHECK(file_size(larger_path) == original_size);

    make_path(smaller_path, sizeof(smaller_path), "payload-size-smaller.audit");
    CHECK(alrios_audit_append(smaller_path, 3200U, payload,
                              sizeof(payload) - 1U, NULL) == ALRIOS_AUDIT_OK);
    overwrite_byte(smaller_path, HEADER_PAYLOAD_SIZE_OFFSET + 3U,
                   (uint8_t)(sizeof(payload) - 2U));
    CHECK(alrios_audit_verify(smaller_path, &result) == ALRIOS_AUDIT_ERR_FORMAT);
}

static void test_tail_with_committed_marker_rejected(void) {
    static const uint8_t payload[] = "tail validation test";
    char path[512];
    alrios_audit_verify_result_t result;

    make_path(path, sizeof(path), "tail-with-commit.audit");
    CHECK(alrios_audit_append(path, 3300U, payload,
                              sizeof(payload) - 1U, NULL) == ALRIOS_AUDIT_OK);
    overwrite_byte(path, HEADER_PAYLOAD_SIZE_OFFSET + 3U,
                   (uint8_t)(sizeof(payload) + 10U));
    CHECK(alrios_audit_verify(path, &result) == ALRIOS_AUDIT_ERR_FORMAT);
    CHECK(alrios_audit_append(path, 3301U, payload,
                              sizeof(payload) - 1U, NULL) ==
          ALRIOS_AUDIT_ERR_FORMAT);
}

static void test_crash_tail_recovery_at_every_boundary(void) {
    static const uint8_t first_payload[] = "committed";
    static const uint8_t second_payload[] = "record interrupted by crash";
    char complete_path[512];
    alrios_audit_block_t first;
    alrios_audit_block_t expected_second;
    uint64_t committed_size;
    uint64_t complete_size;
    uint64_t cut;

    make_path(complete_path, sizeof(complete_path), "complete-source.audit");
    CHECK(alrios_audit_append(complete_path, 4000U, first_payload,
                              sizeof(first_payload) - 1U, &first) ==
          ALRIOS_AUDIT_OK);
    committed_size = file_size(complete_path);
    CHECK(alrios_audit_append(complete_path, 4001U, second_payload,
                              sizeof(second_payload) - 1U,
                              &expected_second) == ALRIOS_AUDIT_OK);
    complete_size = file_size(complete_path);

    for (cut = committed_size + 1U; cut < complete_size; ++cut) {
        char path[512];
        char name[64];
        alrios_audit_block_t recovered_second;
        alrios_audit_verify_result_t result;
        int written = snprintf(name, sizeof(name), "crash-%llu.audit",
                               (unsigned long long)(cut - committed_size));

        CHECK(written >= 0 && (size_t)written < sizeof(name));
        make_path(path, sizeof(path), name);
        copy_prefix(complete_path, path, cut);
        CHECK(alrios_audit_verify(path, &result) == ALRIOS_AUDIT_OK);
        CHECK(result.block_count == 1U);
        CHECK(result.valid_bytes == committed_size);
        CHECK(result.has_incomplete_tail == 1);
        CHECK(alrios_audit_append(path, 4001U, second_payload,
                                  sizeof(second_payload) - 1U,
                                  &recovered_second) == ALRIOS_AUDIT_OK);
        CHECK(recovered_second.sequence == expected_second.sequence);
        CHECK(memcmp(recovered_second.previous_hash,
                     first.hash,
                     sizeof(first.hash)) == 0);
        CHECK(memcmp(recovered_second.hash,
                     expected_second.hash,
                     sizeof(expected_second.hash)) == 0);
        CHECK(alrios_audit_verify(path, &result) == ALRIOS_AUDIT_OK);
        CHECK(result.block_count == 2U);
        CHECK(result.has_incomplete_tail == 0);
        CHECK(result.valid_bytes == file_size(path));
        CHECK(remove(path) == 0);
    }
}

static void test_malformed_committed_data_rejected(void) {
    static const uint8_t payload[] = "event";
    static const uint8_t fragment[] = {'A', 'L', 'R', 'I'};
    static const uint8_t garbage[] = {'N', 'O', 'P', 'E'};
    char path[512];
    char fragment_path[512];
    char garbage_path[512];
    alrios_audit_verify_result_t result;

    make_path(path, sizeof(path), "bad-magic.audit");
    CHECK(alrios_audit_append(path, 5000U, payload,
                              sizeof(payload) - 1U, NULL) == ALRIOS_AUDIT_OK);
    overwrite_byte(path, 0U, (uint8_t)'X');
    CHECK(alrios_audit_verify(path, &result) == ALRIOS_AUDIT_ERR_FORMAT);
    CHECK(alrios_audit_append(path, 5001U, payload,
                              sizeof(payload) - 1U, NULL) ==
          ALRIOS_AUDIT_ERR_FORMAT);

    make_path(fragment_path, sizeof(fragment_path), "fragment-only.audit");
    append_bytes(fragment_path, fragment, sizeof(fragment));
    CHECK(alrios_audit_verify(fragment_path, &result) == ALRIOS_AUDIT_OK);
    CHECK(result.block_count == 0U);
    CHECK(result.valid_bytes == 0U);
    CHECK(result.has_incomplete_tail == 1);
    CHECK(alrios_audit_append(fragment_path, 5002U, payload,
                              sizeof(payload) - 1U, NULL) == ALRIOS_AUDIT_OK);
    CHECK(alrios_audit_verify(fragment_path, &result) == ALRIOS_AUDIT_OK);
    CHECK(result.block_count == 1U);
    CHECK(result.has_incomplete_tail == 0);

    make_path(garbage_path, sizeof(garbage_path), "garbage-tail.audit");
    CHECK(alrios_audit_append(garbage_path, 5003U, payload,
                              sizeof(payload) - 1U, NULL) == ALRIOS_AUDIT_OK);
    append_bytes(garbage_path, garbage, sizeof(garbage));
    CHECK(alrios_audit_verify(garbage_path, &result) ==
          ALRIOS_AUDIT_ERR_FORMAT);
    CHECK(alrios_audit_append(garbage_path, 5004U, payload,
                              sizeof(payload) - 1U, NULL) ==
          ALRIOS_AUDIT_ERR_FORMAT);
}

typedef struct append_worker {
    const char *path;
    unsigned int worker_id;
    int status;
} append_worker_t;

#ifdef _WIN32
static DWORD WINAPI append_worker_main(LPVOID argument)
#else
static void *append_worker_main(void *argument)
#endif
{
    append_worker_t *worker = (append_worker_t *)argument;
    unsigned int index;

    worker->status = ALRIOS_AUDIT_OK;
    for (index = 0U; index < APPENDS_PER_THREAD; ++index) {
        uint8_t payload[8];

        payload[0] = (uint8_t)(worker->worker_id >> 24U);
        payload[1] = (uint8_t)(worker->worker_id >> 16U);
        payload[2] = (uint8_t)(worker->worker_id >> 8U);
        payload[3] = (uint8_t)worker->worker_id;
        payload[4] = (uint8_t)(index >> 24U);
        payload[5] = (uint8_t)(index >> 16U);
        payload[6] = (uint8_t)(index >> 8U);
        payload[7] = (uint8_t)index;
        worker->status = alrios_audit_append(worker->path,
                                             (uint64_t)worker->worker_id *
                                                 APPENDS_PER_THREAD + index,
                                             payload,
                                             sizeof(payload),
                                             NULL);
        if (worker->status != ALRIOS_AUDIT_OK) {
            break;
        }
    }
#ifdef _WIN32
    return 0U;
#else
    return NULL;
#endif
}

static void test_concurrent_append_serialization(void) {
    char path[512];
    append_worker_t workers[CONCURRENT_THREAD_COUNT];
    alrios_audit_verify_result_t result;
    unsigned int index;
#ifdef _WIN32
    HANDLE threads[CONCURRENT_THREAD_COUNT];
#else
    pthread_t threads[CONCURRENT_THREAD_COUNT];
#endif

    make_path(path, sizeof(path), "concurrent.audit");
    for (index = 0U; index < CONCURRENT_THREAD_COUNT; ++index) {
        workers[index].path = path;
        workers[index].worker_id = index;
        workers[index].status = ALRIOS_AUDIT_ERR_IO;
#ifdef _WIN32
        threads[index] = CreateThread(NULL,
                                      0U,
                                      append_worker_main,
                                      &workers[index],
                                      0U,
                                      NULL);
        CHECK(threads[index] != NULL);
#else
        CHECK(pthread_create(&threads[index],
                             NULL,
                             append_worker_main,
                             &workers[index]) == 0);
#endif
    }
    for (index = 0U; index < CONCURRENT_THREAD_COUNT; ++index) {
#ifdef _WIN32
        CHECK(WaitForSingleObject(threads[index], INFINITE) == WAIT_OBJECT_0);
        CHECK(CloseHandle(threads[index]) != 0);
#else
        CHECK(pthread_join(threads[index], NULL) == 0);
#endif
        CHECK(workers[index].status == ALRIOS_AUDIT_OK);
    }
    CHECK(alrios_audit_verify(path, &result) == ALRIOS_AUDIT_OK);
    CHECK(result.block_count ==
          (uint64_t)CONCURRENT_THREAD_COUNT * APPENDS_PER_THREAD);
    CHECK(result.has_incomplete_tail == 0);
    CHECK(result.valid_bytes == file_size(path));
}

static void test_invalid_arguments(void) {
    uint8_t payload = 0U;
    uint8_t hash[ALRIOS_AUDIT_HASH_SIZE] = {0};
    alrios_audit_verify_result_t result;

    CHECK(alrios_audit_append(NULL, 0U, &payload, 1U, NULL) ==
          ALRIOS_AUDIT_ERR_INVALID_ARGUMENT);
    CHECK(alrios_audit_append("", 0U, &payload, 1U, NULL) ==
          ALRIOS_AUDIT_ERR_INVALID_ARGUMENT);
    CHECK(alrios_audit_verify(NULL, &result) ==
          ALRIOS_AUDIT_ERR_INVALID_ARGUMENT);
    CHECK(alrios_audit_verify("", &result) ==
          ALRIOS_AUDIT_ERR_INVALID_ARGUMENT);
    CHECK(alrios_audit_verify("missing.audit", NULL) ==
          ALRIOS_AUDIT_ERR_INVALID_ARGUMENT);
    CHECK(alrios_audit_compute_hash(0U, 0U, NULL, &payload, 1U, hash) ==
          ALRIOS_AUDIT_ERR_INVALID_ARGUMENT);
}

int main(void) {
    initialize_test_directory();
    test_hash_known_answer();
    test_append_and_verify_links();
    test_mutation_detected();
    test_header_length_mutation_is_not_crash_tail();
    test_tail_with_committed_marker_rejected();
    test_crash_tail_recovery_at_every_boundary();
    test_malformed_committed_data_rejected();
    test_concurrent_append_serialization();
    test_invalid_arguments();
    (void)printf("hash chain tests passed\n");
    return EXIT_SUCCESS;
}
