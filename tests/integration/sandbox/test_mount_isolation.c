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

#include "alrios/sandbox/mounts.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define TEST_PATH_BUF_SIZE 1024U

static int join_path2(char *out, size_t out_sz, const char *p1, const char *p2) {
    if (!out || out_sz == 0U || !p1 || !p2) {
        return -1;
    }
    int n = snprintf(out, out_sz, "%s/%s", p1, p2);
    if (n < 0 || (size_t)n >= out_sz) {
        return -1;
    }
    return 0;
}

static int join_path3(char *out, size_t out_sz, const char *p1, const char *p2, const char *p3) {
    if (!out || out_sz == 0U || !p1 || !p2 || !p3) {
        return -1;
    }
    int n = snprintf(out, out_sz, "%s/%s/%s", p1, p2, p3);
    if (n < 0 || (size_t)n >= out_sz) {
        return -1;
    }
    return 0;
}

static void create_file_with_content(const char *path, const char *content) {
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    CHECK(fd >= 0);
    if (content && content[0] != '\0') {
        ssize_t written = write(fd, content, strlen(content));
        CHECK(written == (ssize_t)strlen(content));
    }
    CHECK(close(fd) == 0);
}

static void test_app_id_validation(void) {
    CHECK(alrios_mount_validate_app_id("com.alrigroup.auth") == 1);
    CHECK(alrios_mount_validate_app_id("app-123_test") == 1);
    CHECK(alrios_mount_validate_app_id("app") == 1);

    /* Invalid */
    CHECK(alrios_mount_validate_app_id(NULL) == 0);
    CHECK(alrios_mount_validate_app_id("") == 0);
    CHECK(alrios_mount_validate_app_id("app/traversal") == 0);
    CHECK(alrios_mount_validate_app_id("../app") == 0);
    CHECK(alrios_mount_validate_app_id("app@domain") == 0);
    CHECK(alrios_mount_validate_app_id("app;ls") == 0);
    CHECK(alrios_mount_validate_app_id("app$var") == 0);
}

static void test_path_containment(void) {
    char base_dir[] = "/tmp/alrios_mount_contain_XXXXXX";
    CHECK(mkdtemp(base_dir) != NULL);

    char valid_child[TEST_PATH_BUF_SIZE];
    CHECK(join_path2(valid_child, sizeof(valid_child), base_dir, "app1") == 0);
    CHECK(mkdir(valid_child, 0755) == 0);

    CHECK(alrios_mount_validate_path_containment(base_dir, valid_child) == ALRIOS_MOUNT_OK);

    /* Direct traversal attempt */
    char traversal_child[TEST_PATH_BUF_SIZE];
    CHECK(join_path2(traversal_child, sizeof(traversal_child), base_dir, "../etc") == 0);
    CHECK(alrios_mount_validate_path_containment(base_dir, traversal_child) == ALRIOS_MOUNT_ERR_PATH_TRAVERSAL);

    /* Unrelated path outside base */
    CHECK(alrios_mount_validate_path_containment(base_dir, "/var/tmp") == ALRIOS_MOUNT_ERR_PATH_TRAVERSAL);

    /* Invalid arguments */
    CHECK(alrios_mount_validate_path_containment(NULL, valid_child) == ALRIOS_MOUNT_ERR_INVALID_ARG);
    CHECK(alrios_mount_validate_path_containment(base_dir, NULL) == ALRIOS_MOUNT_ERR_INVALID_ARG);

    rmdir(valid_child);
    rmdir(base_dir);
}

static void test_data_path_generation(void) {
    char out_path[TEST_PATH_BUF_SIZE];
    int status = alrios_mount_get_data_path("/var/data", "com.alrigroup.web", out_path, sizeof(out_path));
    CHECK(status == ALRIOS_MOUNT_OK);
    CHECK(strcmp(out_path, "/var/data/com.alrigroup.web") == 0);

    /* Rejects malicious app_id */
    status = alrios_mount_get_data_path("/var/data", "../escape", out_path, sizeof(out_path));
    CHECK(status == ALRIOS_MOUNT_ERR_APP_ID);

    status = alrios_mount_get_data_path(NULL, "app", out_path, sizeof(out_path));
    CHECK(status == ALRIOS_MOUNT_ERR_INVALID_ARG);
}

/* Worker function executed inside isolated namespace */
static void run_mount_isolation_worker(const char *test_root) {
    int status = alrios_mount_setup_private_namespace();
    CHECK(status == ALRIOS_MOUNT_OK);

    char code_dir[TEST_PATH_BUF_SIZE];
    char data_dir_a[TEST_PATH_BUF_SIZE];
    char data_dir_b[TEST_PATH_BUF_SIZE];
    char sandbox_a[TEST_PATH_BUF_SIZE];
    char sandbox_b[TEST_PATH_BUF_SIZE];

    CHECK(join_path2(code_dir, sizeof(code_dir), test_root, "code_src") == 0);
    CHECK(join_path3(data_dir_a, sizeof(data_dir_a), test_root, "data_host", "app_a") == 0);
    CHECK(join_path3(data_dir_b, sizeof(data_dir_b), test_root, "data_host", "app_b") == 0);
    CHECK(join_path2(sandbox_a, sizeof(sandbox_a), test_root, "sandbox_a") == 0);
    CHECK(join_path2(sandbox_b, sizeof(sandbox_b), test_root, "sandbox_b") == 0);

    /* Create initial file in code directory */
    char binary_file[TEST_PATH_BUF_SIZE];
    CHECK(join_path2(binary_file, sizeof(binary_file), code_dir, "bin.elf") == 0);
    create_file_with_content(binary_file, "IMMUTABLE_CODE_PAYLOAD");

    /* Create existing file in App B's data */
    char secret_b[TEST_PATH_BUF_SIZE];
    CHECK(join_path2(secret_b, sizeof(secret_b), data_dir_b, "secret.key") == 0);
    create_file_with_content(secret_b, "APP_B_PRIVATE_KEY");

    alrios_app_mount_context_t ctx_a;
    alrios_app_mount_context_t ctx_b;

    /* Set up mounts for App A */
    status = alrios_mount_setup_app_mounts("app_a", code_dir, data_dir_a, sandbox_a, &ctx_a);
    CHECK(status == ALRIOS_MOUNT_OK);
    CHECK(ctx_a.entry_count == 2U);
    CHECK(ctx_a.active == 1U);

    /* Set up mounts for App B */
    status = alrios_mount_setup_app_mounts("app_b", code_dir, data_dir_b, sandbox_b, &ctx_b);
    CHECK(status == ALRIOS_MOUNT_OK);
    CHECK(ctx_b.entry_count == 2U);
    CHECK(ctx_b.active == 1U);

    /* 1. Verify Code directory is strictly MS_RDONLY (attempting to write returns EROFS or EACCES) */
    char code_mounted_file[TEST_PATH_BUF_SIZE];
    CHECK(join_path3(code_mounted_file, sizeof(code_mounted_file), sandbox_a, "app", "bin.elf") == 0);

    /* Read should succeed */
    int fd = open(code_mounted_file, O_RDONLY);
    CHECK(fd >= 0);
    char buf[64];
    ssize_t r = read(fd, buf, sizeof(buf) - 1);
    CHECK(r > 0);
    buf[r] = '\0';
    CHECK(strcmp(buf, "IMMUTABLE_CODE_PAYLOAD") == 0);
    close(fd);

    /* Overwrite must fail with EROFS */
    fd = open(code_mounted_file, O_WRONLY);
    CHECK(fd < 0);
    CHECK(errno == EROFS || errno == EACCES);

    /* Creating new file in code mount must fail with EROFS */
    char code_new_file[TEST_PATH_BUF_SIZE];
    CHECK(join_path3(code_new_file, sizeof(code_new_file), sandbox_a, "app", "malicious.sh") == 0);
    fd = open(code_new_file, O_CREAT | O_WRONLY, 0755);
    CHECK(fd < 0);
    CHECK(errno == EROFS || errno == EACCES);

    /* Verify Code directory flags via statvfs */
    struct statvfs st;
    CHECK(statvfs(ctx_a.code_target, &st) == 0);
    CHECK((st.f_flag & ST_RDONLY) != 0);
    CHECK((st.f_flag & ST_NOSUID) != 0);
    CHECK((st.f_flag & ST_NODEV) != 0);

    /* 2. Verify Data directory allows read-write and enforces MS_NOSUID | MS_NODEV | MS_NOEXEC */
    char data_a_file[TEST_PATH_BUF_SIZE];
    CHECK(join_path3(data_a_file, sizeof(data_a_file), sandbox_a, "data/app_a", "persistent.db") == 0);
    fd = open(data_a_file, O_CREAT | O_RDWR | O_TRUNC, 0600);
    CHECK(fd >= 0);
    const char *db_data = "SQLITE_RECORD_001";
    ssize_t written = write(fd, db_data, strlen(db_data));
    CHECK(written == (ssize_t)strlen(db_data));
    close(fd);

    /* Verify mount flags via statvfs */
    CHECK(statvfs(ctx_a.data_target, &st) == 0);
    CHECK((st.f_flag & ST_NOSUID) != 0);
    CHECK((st.f_flag & ST_NODEV) != 0);
    CHECK((st.f_flag & ST_NOEXEC) != 0);

    /* 3. Cross-app data isolation: App A cannot access App B's data mount */
    char cross_access_path[TEST_PATH_BUF_SIZE];
    CHECK(join_path3(cross_access_path, sizeof(cross_access_path), sandbox_a, "data/app_b", "secret.key") == 0);
    fd = open(cross_access_path, O_RDONLY);
    /* In App A's sandbox, /data/app_b does not exist */
    CHECK(fd < 0);
    CHECK(errno == ENOENT || errno == EACCES);

    /* App B can access its own data */
    char b_data_file[TEST_PATH_BUF_SIZE];
    CHECK(join_path3(b_data_file, sizeof(b_data_file), sandbox_b, "data/app_b", "secret.key") == 0);
    fd = open(b_data_file, O_RDONLY);
    CHECK(fd >= 0);
    r = read(fd, buf, sizeof(buf) - 1);
    CHECK(r > 0);
    buf[r] = '\0';
    CHECK(strcmp(buf, "APP_B_PRIVATE_KEY") == 0);
    close(fd);

    /* 4. Clean teardown unmounts all mounts cleanly */
    status = alrios_mount_teardown_app_mounts(&ctx_a);
    CHECK(status == ALRIOS_MOUNT_OK);
    CHECK(ctx_a.entry_count == 0U);
    CHECK(ctx_a.active == 0U);

    status = alrios_mount_teardown_app_mounts(&ctx_b);
    CHECK(status == ALRIOS_MOUNT_OK);
    CHECK(ctx_b.entry_count == 0U);
    CHECK(ctx_b.active == 0U);

    /* After teardown, writing to code_src directly succeeds (unmounted) */
    fd = open(binary_file, O_WRONLY | O_APPEND);
    CHECK(fd >= 0);
    close(fd);

    _exit(EXIT_SUCCESS);
}

static void safe_recursive_rm(const char *path) {
    if (!path || path[0] == '\0') return;
    DIR *d = opendir(path);
    if (!d) {
        unlink(path);
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char full[TEST_PATH_BUF_SIZE];
        if (join_path2(full, sizeof(full), path, entry->d_name) == 0) {
            struct stat st;
            if (lstat(full, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    safe_recursive_rm(full);
                } else {
                    unlink(full);
                }
            }
        }
    }
    closedir(d);
    rmdir(path);
}

static void test_mount_isolation_integration(void) {
    char test_root[] = "/tmp/alrios_mount_iso_XXXXXX";
    CHECK(mkdtemp(test_root) != NULL);

    char code_dir[TEST_PATH_BUF_SIZE];
    char data_dir_a[TEST_PATH_BUF_SIZE];
    char data_dir_b[TEST_PATH_BUF_SIZE];
    char data_host[TEST_PATH_BUF_SIZE];

    CHECK(join_path2(code_dir, sizeof(code_dir), test_root, "code_src") == 0);
    CHECK(join_path2(data_host, sizeof(data_host), test_root, "data_host") == 0);
    CHECK(join_path3(data_dir_a, sizeof(data_dir_a), test_root, "data_host", "app_a") == 0);
    CHECK(join_path3(data_dir_b, sizeof(data_dir_b), test_root, "data_host", "app_b") == 0);

    CHECK(mkdir(code_dir, 0755) == 0);
    CHECK(mkdir(data_host, 0700) == 0);
    CHECK(mkdir(data_dir_a, 0700) == 0);
    CHECK(mkdir(data_dir_b, 0700) == 0);

    pid_t child = fork();
    CHECK(child >= 0);

    if (child == 0) {
        run_mount_isolation_worker(test_root);
    }

    int wstatus = 0;
    pid_t waited;
    do {
        waited = waitpid(child, &wstatus, 0);
    } while (waited < 0 && errno == EINTR);

    CHECK(waited == child);
    CHECK(WIFEXITED(wstatus));
    CHECK(WEXITSTATUS(wstatus) == EXIT_SUCCESS);

    /* Cleanup host test files */
    safe_recursive_rm(test_root);
}

int main(void) {
    (void)printf("[TEST] Running MP-012 mount isolation tests...\n");

    test_app_id_validation();
    test_path_containment();
    test_data_path_generation();
    test_mount_isolation_integration();

    (void)printf("[TEST] MP-012 mount isolation tests: PASS\n");
    return 0;
}
