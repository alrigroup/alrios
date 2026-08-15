/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdlib.h>

static int ssl_init_done = 0;

static void ssl_ensure_init(void) {
    if (!ssl_init_done) {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        ssl_init_done = 1;
    }
}

static void *os_ssl_ctx_create(int is_server) {
    ssl_ensure_init();
    const SSL_METHOD *method = is_server ? TLS_server_method() : TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) return NULL;
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION | SSL_OP_CIPHER_SERVER_PREFERENCE);
    return ctx;
}

static int os_ssl_ctx_use_certificate(void *ctx, const char *cert_path, const char *key_path) {
    SSL_CTX *c = (SSL_CTX *)ctx;
    if (SSL_CTX_use_certificate_file(c, cert_path, SSL_FILETYPE_PEM) != 1)
        return -1;
    if (SSL_CTX_use_PrivateKey_file(c, key_path, SSL_FILETYPE_PEM) != 1)
        return -1;
    if (SSL_CTX_check_private_key(c) != 1)
        return -1;
    return 0;
}

static void *os_ssl_new(void *ctx, int fd) {
    SSL *ssl = SSL_new((SSL_CTX *)ctx);
    if (!ssl) return NULL;
    SSL_set_fd(ssl, fd);
    return ssl;
}

static int os_ssl_handshake(void *ssl) {
    int ret = SSL_accept((SSL *)ssl);
    if (ret != 1) return -1;
    return 0;
}

static int os_ssl_read(void *ssl, void *buf, int num) {
    int ret = SSL_read((SSL *)ssl, buf, num);
    if (ret <= 0) return -1;
    return ret;
}

static int os_ssl_write(void *ssl, const void *buf, int num) {
    int ret = SSL_write((SSL *)ssl, buf, num);
    if (ret <= 0) return -1;
    return ret;
}

static void os_ssl_free(void *ssl) {
    if (ssl) SSL_free((SSL *)ssl);
}

static void os_ssl_ctx_free(void *ctx) {
    if (ctx) SSL_CTX_free((SSL_CTX *)ctx);
}
