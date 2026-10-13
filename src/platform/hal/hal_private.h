#ifndef ALRIOS_PLATFORM_HAL_PRIVATE_H
#define ALRIOS_PLATFORM_HAL_PRIVATE_H

#include "aros_hal.h"

#include <stddef.h>
#include <stdint.h>

int os_file_create(const char *directory, const char *filename, const char *content);
int os_file_delete(const char *path);
int os_file_exists(const char *path);

int os_process_create(const char *path, char *const argv[]);
int os_process_wait(int pid);
int os_process_wait_nohang(int pid);
int os_process_wait_status(int pid, int *exit_code);
int os_process_kill(int pid);
int os_process_self(void);
void *os_process_group_create(void);
int os_process_group_add(void *group, int pid);
int os_process_group_destroy(void *group);

void *os_module_load(const char *path);
void *os_module_sym(void *handle, const char *name);
int os_module_unload(void *handle);

void *os_thread_create(void *(*fn)(void *), void *arg);
int os_thread_detach(void *thread);
int os_thread_join(void *thread);
void *os_mutex_create(void);
int os_mutex_lock(void *mutex);
int os_mutex_unlock(void *mutex);
void os_mutex_destroy(void *mutex);
void *os_cond_create(void);
int os_cond_signal(void *cond);
int os_cond_wait(void *cond, void *mutex);
void os_cond_destroy(void *cond);

uint64_t os_time_ms(void);
void os_sleep_ms(uint32_t ms);

int os_socket_create(int type);
int os_socket_reuseaddr(int fd, int enable);
int os_socket_bind(int fd, const char *addr, uint16_t port);
int os_socket_listen(int fd, int backlog);
int os_socket_accept(int fd);
int os_socket_connect(int fd, const char *addr, uint16_t port);
int os_socket_send(int fd, const void *data, size_t len);
int os_socket_recv(int fd, void *buf, size_t len);
void os_socket_close(int fd);
int os_socket_set_nonblock(int fd);
int os_socket_set_recv_timeout(int fd, int timeout_ms);
int os_socket_set_timeouts(int fd, int timeout_sec);

int os_fs_mkdir(const char *path);
int os_fs_rmdir(const char *path);
int os_fs_exists(const char *path);

void *os_mem_alloc(size_t size);
void os_mem_free(void *ptr);

void *os_ssl_ctx_create(int is_server);
int os_ssl_ctx_use_certificate(void *ctx, const char *cert_path, const char *key_path);
void *os_ssl_new(void *ctx, int fd);
int os_ssl_handshake(void *ssl);
int os_ssl_read(void *ssl, void *buf, int num);
int os_ssl_write(void *ssl, const void *buf, int num);
void os_ssl_free(void *ssl);
void os_ssl_ctx_free(void *ctx);

void *os_poll_create(void);
int os_poll_add(void *poll, int fd, int events, void *user_data);
int os_poll_mod(void *poll, int fd, int events);
void os_poll_remove(void *poll, int fd);
int os_poll_wait(void *poll, ArPollEvent *events, int max_events, int timeout_ms);
void os_poll_destroy(void *poll);

#endif
