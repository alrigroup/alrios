/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "aros_hal.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

/* ---------- forward declarations ---------- */

static int  os_file_create(const char *dir, const char *file, const char *content);
static int  os_file_delete(const char *path);
static int  os_file_exists(const char *path);

static int  os_process_create(const char *path, char *const argv[]);
static int  os_process_wait(int pid);
static int  os_process_wait_nohang(int pid);
static int  os_process_wait_status(int pid, int *exit_code);
static int  os_process_kill(int pid);
static int  os_process_self(void);
static void *os_process_group_create(void);
static int   os_process_group_add(void *group, int pid);
static int   os_process_group_destroy(void *group);

static void *os_module_load(const char *path);
static void *os_module_sym(void *handle, const char *name);
static int   os_module_unload(void *handle);

static void *os_thread_create(void *(*fn)(void *), void *arg);
static int   os_thread_detach(void *thread);
static int   os_thread_join(void *thread);

static void *os_mutex_create(void);
static int   os_mutex_lock(void *mutex);
static int   os_mutex_unlock(void *mutex);
static void  os_mutex_destroy(void *mutex);

static void *os_cond_create(void);
static int   os_cond_signal(void *cond);
static int   os_cond_wait(void *cond, void *mutex);
static void  os_cond_destroy(void *cond);

static uint64_t os_time_ms(void);
static void     os_sleep_ms(uint32_t ms);

static int  os_socket_create(int type);
static int  os_socket_reuseaddr(int fd, int enable);
static int  os_socket_bind(int fd, const char *addr, uint16_t port);
static int  os_socket_listen(int fd, int backlog);
static int  os_socket_accept(int fd);
static int  os_socket_connect(int fd, const char *addr, uint16_t port);
static int  os_socket_send(int fd, const void *data, size_t len);
static int  os_socket_recv(int fd, void *buf, size_t len);
static void os_socket_close(int fd);
static int  os_socket_set_nonblock(int fd);
static int  os_socket_set_recv_timeout(int fd, int timeout_ms);

static int   os_fs_mkdir(const char *path);
static int   os_fs_rmdir(const char *path);
static int   os_fs_exists(const char *path);

static void *os_mem_alloc(size_t size);
static void  os_mem_free(void *ptr);

static void *os_ssl_ctx_create(int is_server);
static int   os_ssl_ctx_use_certificate(void *ctx, const char *cert_path, const char *key_path);
static void *os_ssl_new(void *ctx, int fd);
static int   os_ssl_handshake(void *ssl);
static int   os_ssl_read(void *ssl, void *buf, int num);
static int   os_ssl_write(void *ssl, const void *buf, int num);
static void  os_ssl_free(void *ssl);
static void  os_ssl_ctx_free(void *ctx);

static void *os_poll_create(void);
static int   os_poll_add(void *p, int fd, int events, void *user_data);
static int   os_poll_mod(void *p, int fd, int events);
static void  os_poll_remove(void *p, int fd);
static int   os_poll_wait(void *p, ArPollEvent *events, int max_events, int timeout_ms);
static void  os_poll_destroy(void *p);

/* ---------- public API ---------- */

int ar_file_create(const char *directory, const char *filename, const char *content) {
    if (!directory || !filename || !content) return -EINVAL;
    return os_file_create(directory, filename, content);
}

int ar_file_delete(const char *path) {
    if (!path) return -EINVAL;
    return os_file_delete(path);
}

int ar_file_exists(const char *path) {
    if (!path) return 0;
    return os_file_exists(path);
}

int ar_process_create(const char *path, char *const argv[]) {
    if (!path) return -EINVAL;
    return os_process_create(path, argv);
}

int ar_process_wait(int pid) {
    if (pid <= 0) return -EINVAL;
    return os_process_wait(pid);
}

int ar_process_wait_nohang(int pid) {
    if (pid <= 0) return -EINVAL;
    return os_process_wait_nohang(pid);
}

int ar_process_wait_status(int pid, int *exit_code) {
    if (pid <= 0) return -EINVAL;
    return os_process_wait_status(pid, exit_code);
}

int ar_process_kill(int pid) {
    if (pid <= 0) return -EINVAL;
    return os_process_kill(pid);
}

int ar_process_self(void) {
    return os_process_self();
}

void *ar_process_group_create(void) {
    return os_process_group_create();
}

int ar_process_group_add(void *group, int pid) {
    if (!group || pid <= 0) return -EINVAL;
    return os_process_group_add(group, pid);
}

int ar_process_group_destroy(void *group) {
    if (!group) return -EINVAL;
    return os_process_group_destroy(group);
}

void *ar_module_load(const char *path) {
    if (!path) return NULL;
    return os_module_load(path);
}

void *ar_module_sym(void *handle, const char *name) {
    if (!handle || !name) return NULL;
    return os_module_sym(handle, name);
}

int ar_module_unload(void *handle) {
    if (!handle) return -EINVAL;
    return os_module_unload(handle);
}

void *ar_thread_create(void *(*fn)(void *), void *arg) {
    if (!fn) return NULL;
    return os_thread_create(fn, arg);
}

int ar_thread_detach(void *thread) {
    if (!thread) return -EINVAL;
    return os_thread_detach(thread);
}

int ar_thread_join(void *thread) {
    if (!thread) return -EINVAL;
    return os_thread_join(thread);
}

void *ar_mutex_create(void) {
    return os_mutex_create();
}

int ar_mutex_lock(void *mutex) {
    if (!mutex) return -EINVAL;
    return os_mutex_lock(mutex);
}

int ar_mutex_unlock(void *mutex) {
    if (!mutex) return -EINVAL;
    return os_mutex_unlock(mutex);
}

void ar_mutex_destroy(void *mutex) {
    if (mutex) os_mutex_destroy(mutex);
}

void *ar_cond_create(void) {
    return os_cond_create();
}

int ar_cond_signal(void *cond) {
    if (!cond) return -EINVAL;
    return os_cond_signal(cond);
}

int ar_cond_wait(void *cond, void *mutex) {
    if (!cond || !mutex) return -EINVAL;
    return os_cond_wait(cond, mutex);
}

void ar_cond_destroy(void *cond) {
    if (cond) os_cond_destroy(cond);
}

uint64_t ar_time_ms(void) {
    return os_time_ms();
}

void ar_sleep_ms(uint32_t ms) {
    os_sleep_ms(ms);
}

int ar_socket_create(int type) {
    return os_socket_create(type);
}

int ar_socket_reuseaddr(int fd, int enable) {
    if (fd < 0) return -EINVAL;
    return os_socket_reuseaddr(fd, enable);
}

int ar_socket_bind(int fd, const char *addr, uint16_t port) {
    if (!addr) return -EINVAL;
    return os_socket_bind(fd, addr, port);
}

int ar_socket_listen(int fd, int backlog) {
    if (fd < 0) return -EINVAL;
    return os_socket_listen(fd, backlog);
}

int ar_socket_accept(int fd) {
    if (fd < 0) return -EINVAL;
    return os_socket_accept(fd);
}

int ar_socket_connect(int fd, const char *addr, uint16_t port) {
    if (!addr) return -EINVAL;
    return os_socket_connect(fd, addr, port);
}

int ar_socket_send(int fd, const void *data, size_t len) {
    if (fd < 0 || !data) return -EINVAL;
    return os_socket_send(fd, data, len);
}

int ar_socket_recv(int fd, void *buf, size_t len) {
    if (fd < 0 || !buf) return -EINVAL;
    return os_socket_recv(fd, buf, len);
}

void ar_socket_close(int fd) {
    if (fd >= 0) os_socket_close(fd);
}

int ar_socket_set_nonblock(int fd) {
    if (fd < 0) return -1;
    return os_socket_set_nonblock(fd);
}

int ar_socket_set_recv_timeout(int fd, int timeout_ms) {
    if (fd < 0) return -1;
    return os_socket_set_recv_timeout(fd, timeout_ms);
}

int ar_fs_mkdir(const char *path) {
    if (!path) return -EINVAL;
    return os_fs_mkdir(path);
}

int ar_fs_rmdir(const char *path) {
    if (!path) return -EINVAL;
    return os_fs_rmdir(path);
}

int ar_fs_exists(const char *path) {
    if (!path) return 0;
    return os_fs_exists(path);
}

void *ar_mem_alloc(size_t size) {
    if (size == 0) return NULL;
    return os_mem_alloc(size);
}

void ar_mem_free(void *ptr) {
    if (ptr) os_mem_free(ptr);
}

void *ar_ssl_ctx_create(int is_server) {
    return os_ssl_ctx_create(is_server);
}

int ar_ssl_ctx_use_certificate(void *ctx, const char *cert_path, const char *key_path) {
    if (!ctx || !cert_path || !key_path) return -1;
    return os_ssl_ctx_use_certificate(ctx, cert_path, key_path);
}

void *ar_ssl_new(void *ctx, int fd) {
    if (!ctx || fd < 0) return NULL;
    return os_ssl_new(ctx, fd);
}

int ar_ssl_handshake(void *ssl) {
    if (!ssl) return -1;
    return os_ssl_handshake(ssl);
}

int ar_ssl_read(void *ssl, void *buf, int num) {
    if (!ssl || !buf) return -1;
    return os_ssl_read(ssl, buf, num);
}

int ar_ssl_write(void *ssl, const void *buf, int num) {
    if (!ssl || !buf) return -1;
    return os_ssl_write(ssl, buf, num);
}

void ar_ssl_free(void *ssl) {
    os_ssl_free(ssl);
}

void ar_ssl_ctx_free(void *ctx) {
    os_ssl_ctx_free(ctx);
}

ArPoll *ar_poll_create(void) {
    return (ArPoll *)os_poll_create();
}

int ar_poll_add(ArPoll *p, int fd, int events, void *user_data) {
    if (!p || fd < 0) return -1;
    return os_poll_add((void *)p, fd, events, user_data);
}

int ar_poll_mod(ArPoll *p, int fd, int events) {
    if (!p || fd < 0) return -1;
    return os_poll_mod((void *)p, fd, events);
}

void ar_poll_remove(ArPoll *p, int fd) {
    if (!p || fd < 0) return;
    os_poll_remove((void *)p, fd);
}

int ar_poll_wait(ArPoll *p, ArPollEvent *events, int max_events, int timeout_ms) {
    if (!p || !events || max_events <= 0) return -1;
    return os_poll_wait((void *)p, events, max_events, timeout_ms);
}

void ar_poll_destroy(ArPoll *p) {
    if (p) os_poll_destroy((void *)p);
}

/* ---------- OS dispatch ---------- */

#ifdef __linux__
    #include "linux/file.c"
    #include "linux/process.c"
    #include "linux/module.c"
    #include "linux/thread.c"
    #include "linux/time.c"
    #include "linux/socket.c"
    #include "linux/fs.c"
    #include "linux/memory.c"
    #include "linux/ssl.c"
    #include "linux/poll.c"
#elif defined(_WIN32)
    #include "windows/file.c"
    #include "windows/process.c"
    #include "windows/module.c"
    #include "windows/thread.c"
    #include "windows/time.c"
    #include "windows/socket.c"
    #include "windows/fs.c"
    #include "windows/memory.c"
    #include "windows/ssl.c"
    #include "windows/poll.c"
#else
    #error "alrios: no HAL implementation for this OS"
#endif
