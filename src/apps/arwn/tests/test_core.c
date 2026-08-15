/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

/* Gates da Fase 0: config parser estrito + contêiner .arweb (CRC/offsets). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arwn.h"
#include "arwn_config.h"
#include "arwn_pack.h"
#include "arwn_http.h"
#include "arwn_server.h"
#include "arwn_obfuscator.h"
#include "aros_hal.h"

static int g_fail = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("  [FAIL] %s (line %d)\n", msg, __LINE__);                  \
            g_fail++;                                                          \
        } else {                                                               \
            printf("  [ok]   %s\n", msg);                                      \
        }                                                                      \
    } while (0)

static void test_config(void) {
    printf("[config]\n");

    const char *ok_cfg =
        "# comentario\n"
        "[app]\n"
        "name=meuapp\n"
        "port = 3001\n"
        "\n"
        "[unit:main]\n"
        "source=web/\n"
        "entry=index.arhtml\n"
        "compile=main.c,main.js\n"
        "compile.lang=c,js\n"
        "obfuscate=yes\n";

    arwn_cfg_t cfg;
    int rc = arwn_cfg_parse(&cfg, ok_cfg, strlen(ok_cfg));
    CHECK(rc == 0, "parse valido");

    const char *name = arwn_cfg_find(&cfg, "app", "name");
    CHECK(name && strcmp(name, "meuapp") == 0, "key name");

    int port = arwn_cfg_find_int(&cfg, "app", "port", -1);
    CHECK(port == 3001, "porta 3001 (int estrito)");

    int obf = arwn_cfg_find_bool(&cfg, "unit:main", "obfuscate", 0);
    CHECK(obf == 1, "obfuscate=yes -> 1");

    /* inteiro inválido não derruba e cai no default */
    int bad = arwn_cfg_find_int(&cfg, "app", "port", 9999);
    CHECK(bad == 3001, "porta valida continua");

    /* --- casos malformados --- */
    const char *overflow =
        "[app]\nport=99999999999999999999999\n";
    arwn_cfg_t c2;
    rc = arwn_cfg_parse(&c2, overflow, strlen(overflow));
    CHECK(rc == 0, "parse overflow nao quebra (aceito como string)");
    int ov = arwn_cfg_find_int(&c2, "app", "port", -1);
    CHECK(ov == -1, "overflow -> default -1 (guarda de overflow)");

    const char *hex = "[app]\nport=0x10\n";
    arwn_cfg_t c3;
    rc = arwn_cfg_parse(&c3, hex, strlen(hex));
    CHECK(rc == 0, "parse hex aceito (string)");
    int hx = arwn_cfg_find_int(&c3, "app", "port", -1);
    CHECK(hx == -1, "hex -> default -1 (rejeita 0x)");

    const char *bad_sect = "[app\nport=1\n";
    arwn_cfg_t c4;
    rc = arwn_cfg_parse(&c4, bad_sect, strlen(bad_sect));
    CHECK(rc == -1, "secao sem ] rejeitada");

    const char *no_eq = "[app]\nport 3001\n";
    arwn_cfg_t c5;
    rc = arwn_cfg_parse(&c5, no_eq, strlen(no_eq));
    CHECK(rc == -1, "linha sem = rejeitada");

    const char *empty_key = "[app]\n=3001\n";
    arwn_cfg_t c6;
    rc = arwn_cfg_parse(&c6, empty_key, strlen(empty_key));
    CHECK(rc == -1, "chave vazia rejeitada");

    /* buffer gigante */
    char big[ARWN_CFG_MAX_FILE + 8];
    memset(big, 'a', sizeof(big));
    arwn_cfg_t c7;
    rc = arwn_cfg_parse(&c7, big, sizeof(big));
    CHECK(rc == -1, "config acima do max rejeitado");
}

static void test_pack(void) {
    printf("[pack]\n");

    const char *s1 = "hello config";
    const char *s2 = "<html></html>";
    uint8_t wasm[64];
    memset(wasm, 0x1, sizeof(wasm));

    arwn_pack_section_t secs[3];
    snprintf(secs[0].name, sizeof(secs[0].name), "config.arwn");
    secs[0].data = s1;
    secs[0].size = (uint32_t)strlen(s1);
    snprintf(secs[1].name, sizeof(secs[1].name), "app.html");
    secs[1].data = s2;
    secs[1].size = (uint32_t)strlen(s2);
    snprintf(secs[2].name, sizeof(secs[2].name), "mod/main.wasm");
    secs[2].data = wasm;
    secs[2].size = sizeof(wasm);

    uint8_t buf[4096];
    size_t plen = 0;
    int rc = arwn_pack_build(secs, 3, buf, sizeof(buf), &plen);
    CHECK(rc == 0, "pack build ok");

    rc = arwn_pack_validate(buf, plen);
    CHECK(rc == 0, "pack validate ok (CRC+offsets)");

    /* corrompe um byte do payload */
    buf[plen - 10] ^= 0xFF;
    rc = arwn_pack_validate(buf, plen);
    CHECK(rc == -1, "payload corrompido detectado (CRC)");

    /* trunca o buffer */
    rc = arwn_pack_validate(buf, plen - 3);
    CHECK(rc == -1, "buffer truncado rejeitado");

    /* magic errado */
    buf[0] = 'X';
    rc = arwn_pack_validate(buf, plen);
    CHECK(rc == -1, "magic errado rejeitado");
}

static void test_app_lifecycle(void) {
    printf("[app]\n");

    arwn_app_t *app = arwn_app_new("teste");
    CHECK(app != NULL, "arwn_app_new");
    CHECK(strcmp(arwn_app_name(app), "teste") == 0, "nome ok");

    const char *cfg =
        "[app]\nname=meuapp\nport=3001\n"
        "[unit:main]\nsource=web/\nentry=index.arhtml\ncompile=main.js\ncompile.lang=js\n"
        "[unit:functions]\nsource=web/functions.go\nentry=functions.go\ncompile.lang=go\n";

    int rc = arwn_config_parse_buffer(app, cfg, strlen(cfg));
    CHECK(rc == 0, "config buffer ok");

    CHECK(arwn_config_unit_count(app) == 2, "2 units derivadas");

    const arwn_unit_t *u0 = arwn_config_unit(app, 0);
    CHECK(u0 && strcmp(u0->name, "main") == 0, "unit 0 = main");
    CHECK(u0 && strcmp(u0->entry, "index.arhtml") == 0, "unit 0 entry");

    arwn_app_free(app);
    printf("[app] free ok\n");
}

static void test_http_parser(void) {
    printf("[http parser]\n");

    /* GET simples completo */
    const char *get =
        "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";
    arwn_http_req_t req;
    int rc = arwn_http_parse(&req, get, strlen(get));
    CHECK(rc == ARWN_HTTP_COMPLETE, "GET / completo");
    CHECK(arwn_http_method_is(&req, "GET"), "metodo GET");
    CHECK(req.path_len == 1 && req.path[0] == '/', "path '/'");
    CHECK(req.header_count == 2, "2 headers");
    CHECK(req.content_length == -1, "sem Content-Length");
    CHECK(req.complete == 1, "complete=1");

    /* incompleto -> NEED_MORE */
    const char *partial = "GET / HTTP/1.1\r\nHost: l";
    arwn_http_req_t r2;
    rc = arwn_http_parse(&r2, partial, strlen(partial));
    CHECK(rc == ARWN_HTTP_NEED_MORE, "request incompleto -> NEED_MORE");

    /* POST com body e Content-Length */
    const char *post =
        "POST /api HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
    arwn_http_req_t r3;
    rc = arwn_http_parse(&r3, post, strlen(post));
    CHECK(rc == ARWN_HTTP_COMPLETE, "POST com body completo");
    CHECK(r3.content_length == 5, "Content-Length=5");
    CHECK(r3.body_len == 5, "body_len=5");

    /* POST sem corpo ainda -> NEED_MORE */
    const char *post_partial =
        "POST /api HTTP/1.1\r\nContent-Length: 10\r\n\r\nhello";
    arwn_http_req_t r4;
    rc = arwn_http_parse(&r4, post_partial, strlen(post_partial));
    CHECK(rc == ARWN_HTTP_NEED_MORE, "body incompleto -> NEED_MORE");

    /* --- anti-smuggling: Content-Length hex/não-dígito -> 400 --- */
    const char *cl_hex =
        "POST / HTTP/1.1\r\nContent-Length: 0x10\r\n\r\n";
    arwn_http_req_t r5;
    rc = arwn_http_parse(&r5, cl_hex, strlen(cl_hex));
    CHECK(rc == ARWN_HTTP_BAD, "Content-Length hex -> 400");

    const char *cl_neg =
        "POST / HTTP/1.1\r\nContent-Length: -5\r\n\r\n";
    arwn_http_req_t r6;
    rc = arwn_http_parse(&r6, cl_neg, strlen(cl_neg));
    CHECK(rc == ARWN_HTTP_BAD, "Content-Length negativo -> 400");

    const char *cl_alpha =
        "POST / HTTP/1.1\r\nContent-Length: 12ab\r\n\r\n";
    arwn_http_req_t r7;
    rc = arwn_http_parse(&r7, cl_alpha, strlen(cl_alpha));
    CHECK(rc == ARWN_HTTP_BAD, "Content-Length nao numerico -> 400");

    /* Content-Length gigante (overflow) -> 400 */
    const char *cl_huge =
        "POST / HTTP/1.1\r\nContent-Length: 99999999999999999999999999\r\n\r\n";
    arwn_http_req_t r8;
    rc = arwn_http_parse(&r8, cl_huge, strlen(cl_huge));
    CHECK(rc == ARWN_HTTP_BAD, "Content-Length overflow -> 400");

    /* Content-Length acima do cap de request -> 400 */
    const char *cl_cap =
        "POST / HTTP/1.1\r\nContent-Length: 1000000\r\n\r\n";
    arwn_http_req_t r9;
    rc = arwn_http_parse(&r9, cl_cap, strlen(cl_cap));
    CHECK(rc == ARWN_HTTP_BAD, "Content-Length acima do cap -> 400");

    /* Content-Length duplicado com valores diferentes -> 400 */
    const char *cl_dup =
        "POST / HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\n";
    arwn_http_req_t r10;
    rc = arwn_http_parse(&r10, cl_dup, strlen(cl_dup));
    CHECK(rc == ARWN_HTTP_BAD, "Content-Length duplicado diferente -> 400");

    /* request line malformada -> 400 */
    const char *bad_line = "GET /\r\n\r\n";
    arwn_http_req_t r11;
    rc = arwn_http_parse(&r11, bad_line, strlen(bad_line));
    CHECK(rc == ARWN_HTTP_BAD, "request-line malformada -> 400");

    /* header sem colon -> 400 */
    const char *bad_hdr = "GET / HTTP/1.1\r\nHost localhost\r\n\r\n";
    arwn_http_req_t r12;
    rc = arwn_http_parse(&r12, bad_hdr, strlen(bad_hdr));
    CHECK(rc == ARWN_HTTP_BAD, "header sem ':' -> 400");

    /* header name com caractere inválido -> 400 */
    const char *bad_tchar = "GET / HTTP/1.1\r\nHo(st: x\r\n\r\n";
    arwn_http_req_t r13;
    rc = arwn_http_parse(&r13, bad_tchar, strlen(bad_tchar));
    CHECK(rc == ARWN_HTTP_BAD, "header name com tchar invalido -> 400");

    /* buffer grande demais (acima do max) -> parse BAD */
    char huge[ARWN_HTTP_MAX_REQUEST + 16];
    memset(huge, 'a', sizeof(huge));
    memcpy(huge, "GET / HTTP/1.1\r\n", 16);
    arwn_http_req_t r14;
    rc = arwn_http_parse(&r14, huge, sizeof(huge));
    CHECK(rc == ARWN_HTTP_BAD, "request acima do max -> 400");
}

/* roda o server num thread, faz um GET via socket, verifica headers */
static int g_server_ok = 0;

static void *test_server_thread(void *arg) {
    (void)arg;
    arwn_server_t *s = (arwn_server_t *)arg;
    int rc = arwn_server_run(s, "127.0.0.1", 3017);
    g_server_ok = (rc == 0) ? 1 : 0;
    return NULL;
}

static void test_server(void) {
    printf("[server]\n");

    /* monta um .arweb em memória e expõe via load_arweb */
    const char *s1 = "name=meuapp\n";
    const char *s2 = "<html>ARWN</html>";
    uint8_t wasm[16];
    memset(wasm, 0xAB, sizeof(wasm));

    arwn_pack_section_t secs[3];
    snprintf(secs[0].name, sizeof(secs[0].name), "config.arwn");
    secs[0].data = s1;
    secs[0].size = (uint32_t)strlen(s1);
    snprintf(secs[1].name, sizeof(secs[1].name), "app.html");
    secs[1].data = s2;
    secs[1].size = (uint32_t)strlen(s2);
    snprintf(secs[2].name, sizeof(secs[2].name), "mod/main.wasm");
    secs[2].data = wasm;
    secs[2].size = sizeof(wasm);

    static uint8_t buf[4096];
    size_t plen = 0;
    int rc = arwn_pack_build(secs, 3, buf, sizeof(buf), &plen);
    CHECK(rc == 0, "pack build ok");

    arwn_unit_t unit;
    memset(&unit, 0, sizeof(unit));
    strncpy(unit.name, "main", sizeof(unit.name) - 1);
    strncpy(unit.entry, "index.arhtml", sizeof(unit.entry) - 1);

    arwn_server_t *s = arwn_server_create();
    CHECK(s != NULL, "server create");

    rc = arwn_server_load_arweb(s, &unit, buf, plen);
    CHECK(rc == 0, "load_arweb ok");
    CHECK(arwn_server_route_count(s) >= 3, ">=3 rotas (/, entry, wasm)");

    void *th = ar_thread_create(test_server_thread, s);
    CHECK(th != NULL, "server thread started");
    if (th) ar_thread_detach(th);

    /* espera o server subir */
    ar_sleep_ms(300);

    /* GET / */
    int fd = ar_socket_create(1);
    CHECK(fd >= 0, "client socket");
    if (fd >= 0) {
        rc = ar_socket_connect(fd, "127.0.0.1", 3017);
        CHECK(rc == 0, "connect ao server");
        if (rc == 0) {
            const char *req =
                "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
            ar_socket_send(fd, req, (int)strlen(req));

            char resp[2048];
            int total = 0;
            while (total < (int)sizeof(resp) - 1) {
                int n = ar_socket_recv(fd, resp + total, (int)sizeof(resp) - 1 - total);
                if (n <= 0) break; /* Connection: close -> EOF */
                total += n;
            }
            resp[total] = '\0';

            CHECK(strncmp(resp, "HTTP/1.1 200", 12) == 0, "resposta 200");
            CHECK(strstr(resp, "Content-Type: text/html") != NULL,
                  "Content-Type html");
            CHECK(strstr(resp, "X-Content-Type-Options: nosniff") != NULL,
                  "nosniff header");
            CHECK(strstr(resp, "Content-Security-Policy") != NULL,
                  "CSP header");
            CHECK(strstr(resp, "<html>ARWN</html>") != NULL, "body do app.html");
        }
        ar_socket_close(fd);
    }

    /* GET /mod/main.wasm -> application/wasm */
    fd = ar_socket_create(1);
    if (fd >= 0) {
        rc = ar_socket_connect(fd, "127.0.0.1", 3017);
        if (rc == 0) {
            const char *req =
                "GET /mod/main.wasm HTTP/1.1\r\nHost: localhost\r\n"
                "Connection: close\r\n\r\n";
            ar_socket_send(fd, req, (int)strlen(req));
            char resp[2048];
            int total = 0;
            while (total < (int)sizeof(resp) - 1) {
                int n = ar_socket_recv(fd, resp + total, (int)sizeof(resp) - 1 - total);
                if (n <= 0) break;
                total += n;
                if (total > 50) break;
            }
            resp[total] = '\0';
            CHECK(strstr(resp, "Content-Type: application/wasm") != NULL,
                  "wasm servido com application/wasm");
            CHECK(strstr(resp, "Cache-Control: public, max-age=31536000, immutable") != NULL,
                  "wasm immutable");
        }
        ar_socket_close(fd);
    }

    /* GET /nao-existe -> 404 */
    fd = ar_socket_create(1);
    if (fd >= 0) {
        rc = ar_socket_connect(fd, "127.0.0.1", 3017);
        if (rc == 0) {
            const char *req =
                "GET /nao-existe HTTP/1.1\r\nHost: localhost\r\n"
                "Connection: close\r\n\r\n";
            ar_socket_send(fd, req, (int)strlen(req));
            char resp[2048];
            int total = 0;
            while (total < (int)sizeof(resp) - 1) {
                int n = ar_socket_recv(fd, resp + total, (int)sizeof(resp) - 1 - total);
                if (n <= 0) break;
                total += n;
            }
            resp[total] = '\0';
            CHECK(strncmp(resp, "HTTP/1.1 404", 12) == 0, "404 p/ rota inexistente");
        }
        ar_socket_close(fd);
    }

    /* smuggling via request real: Content-Length hex -> 400 */
    fd = ar_socket_create(1);
    if (fd >= 0) {
        rc = ar_socket_connect(fd, "127.0.0.1", 3017);
        if (rc == 0) {
            const char *bad =
                "POST / HTTP/1.1\r\nContent-Length: 0x10\r\n\r\n";
            ar_socket_send(fd, bad, (int)strlen(bad));
            char resp[2048];
            int total = 0;
            while (total < (int)sizeof(resp) - 1) {
                int n = ar_socket_recv(fd, resp + total, (int)sizeof(resp) - 1 - total);
                if (n <= 0) break;
                total += n;
            }
            resp[total] = '\0';
            CHECK(strncmp(resp, "HTTP/1.1 400", 12) == 0, "smuggling -> 400");
        }
        ar_socket_close(fd);
    }

    /* GET / com If-None-Match igual ao ETag -> 304 (gate de cache) */
    fd = ar_socket_create(1);
    if (fd >= 0) {
        rc = ar_socket_connect(fd, "127.0.0.1", 3017);
        if (rc == 0) {
            const char *req =
                "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
            ar_socket_send(fd, req, (int)strlen(req));
            char resp[2048];
            int total = 0;
            while (total < (int)sizeof(resp) - 1) {
                int n = ar_socket_recv(fd, resp + total, (int)sizeof(resp) - 1 - total);
                if (n <= 0) break;
                total += n;
            }
            resp[total] = '\0';
            char *etag = strstr(resp, "ETag: ");
            CHECK(etag != NULL, "GET / tem ETag");
            if (etag) {
                char etag_line[64];
                snprintf(etag_line, sizeof(etag_line), "%.*s",
                         (int)strcspn(etag + 6, "\r\n"), etag + 6);
                ar_socket_close(fd); /* 1a conexao fechou (Connection: close) */

                fd = ar_socket_create(1);
                rc = ar_socket_connect(fd, "127.0.0.1", 3017);
                if (rc == 0) {
                    char req2[1024];
                    snprintf(req2, sizeof(req2),
                             "GET / HTTP/1.1\r\nHost: localhost\r\n"
                             "If-None-Match: %s\r\nConnection: close\r\n\r\n",
                             etag_line);
                    ar_socket_send(fd, req2, (int)strlen(req2));
                    int total2 = 0;
                    while (total2 < (int)sizeof(resp) - 1) {
                        int n = ar_socket_recv(fd, resp + total2, (int)sizeof(resp) - 1 - total2);
                        if (n <= 0) break;
                        total2 += n;
                    }
                    resp[total2] = '\0';
                    CHECK(strncmp(resp, "HTTP/1.1 304", 12) == 0,
                          "If-None-Match igual -> 304");
                    CHECK(strstr(resp, "\r\n\r\n") != NULL &&
                          strstr(resp, "<html>") == NULL,
                          "304 sem body");
                }
            }
        }
        ar_socket_close(fd);
    }

    /* Gate Fase 6: Path-traversal attempt -> 404 (safe, strictly in-memory routing) */
    fd = ar_socket_create(1);
    if (fd >= 0) {
        rc = ar_socket_connect(fd, "127.0.0.1", 3017);
        if (rc == 0) {
            const char *req =
                "GET /../../../../etc/passwd HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
            ar_socket_send(fd, req, (int)strlen(req));
            char resp[2048];
            int total = 0;
            while (total < (int)sizeof(resp) - 1) {
                int n = ar_socket_recv(fd, resp + total, (int)sizeof(resp) - 1 - total);
                if (n <= 0) break;
                total += n;
            }
            resp[total] = '\0';
            CHECK(strncmp(resp, "HTTP/1.1 404", 12) == 0, "path traversal attempt -> 404");
        }
        ar_socket_close(fd);
    }

    /* Gate Fase 6: Oversized request (>64KB cap) -> 400 Bad Request */
    fd = ar_socket_create(1);
    if (fd >= 0) {
        rc = ar_socket_connect(fd, "127.0.0.1", 3017);
        if (rc == 0) {
            char huge_req[65536 + 1024];
            memset(huge_req, 'A', sizeof(huge_req));
            memcpy(huge_req, "POST / HTTP/1.1\r\nContent-Length: 70000\r\n\r\n", 42);
            ar_socket_send(fd, huge_req, (int)sizeof(huge_req));
            char resp[2048];
            int total = 0;
            while (total < (int)sizeof(resp) - 1) {
                int n = ar_socket_recv(fd, resp + total, (int)sizeof(resp) - 1 - total);
                if (n <= 0) break;
                total += n;
            }
            resp[total] = '\0';
            CHECK(strncmp(resp, "HTTP/1.1 400", 12) == 0, "oversized request -> 400");
        }
        ar_socket_close(fd);
    }

    arwn_server_stop(s);
    ar_sleep_ms(100);
    arwn_server_free(s);
    printf("[server] stopped\n");
}

static void test_obfuscator(void) {
    printf("[obfuscator]\n");

    /* JS Obfuscation */
    const char *js_raw =
        "// Comment line 1\n"
        "/* Multi line\n"
        "   comment */\n"
        "function hello(name) {\n"
        "    console.log(\"Hello world!\");\n"
        "    return 'secret123';\n"
        "}\n";

    size_t obf_len = 0;
    char *obf_js = arwn_obfuscate_js(js_raw, strlen(js_raw), "Custom Unit License", &obf_len);
    CHECK(obf_js != NULL, "js obfuscate ok");
    if (obf_js) {
        CHECK(strstr(obf_js, "Custom Unit License") != NULL, "contains custom copyright");
        CHECK(strstr(obf_js, "Comment line 1") == NULL, "strip single-line comment");
        CHECK(strstr(obf_js, "Multi line") == NULL, "strip multi-line comment");
        CHECK(strstr(obf_js, "Hello world!") == NULL, "hex-escaped double-quoted string");
        CHECK(strstr(obf_js, "secret123") == NULL, "hex-escaped single-quoted string");
        CHECK(strstr(obf_js, "\\x48\\x65\\x6c\\x6c\\x6f") != NULL ||
              strstr(obf_js, "\\x") != NULL, "contains \\x hex sequences");
        free(obf_js);
    }

    /* WASM Obfuscation: dummy WASM with custom 'name' section */
    /* Header: \0asm \1\0\0\0 */
    /* Section 0 (Custom): size 6 -> name_len 4 "name" + payload "ab" */
    uint8_t wasm_sample[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, /* header (8B) */
        0x00, /* section 0 */
        0x07, /* section length 7 */
        0x04, /* name length 4 */
        'n', 'a', 'm', 'e',
        0x01, 0x02 /* payload */
    };
    size_t wasm_sz = sizeof(wasm_sample);
    size_t out_wasm_sz = 0;
    int rc = arwn_obfuscate_wasm(wasm_sample, wasm_sz, &out_wasm_sz);
    CHECK(rc == 0, "wasm obfuscate ok");
    CHECK(out_wasm_sz == 8, "debug custom section stripped to 8-byte header");
}

int main(void) {
    printf("ARWN Fase 0 gates\n");
    test_config();
    test_pack();
    test_app_lifecycle();
    test_http_parser();
    test_server();
    test_obfuscator();

    if (g_fail == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_fail);
    return 1;
}