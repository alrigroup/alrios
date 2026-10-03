/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */

#include "alrios/ipc_channel.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/* Validates HTTP method against known safe HTTP tokens */
static int is_valid_http_method(const char *method) {
    if (!method || method[0] == '\0') {
        return 0;
    }
    const char *valid_methods[] = {
        "GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH", "CONNECT", "TRACE"
    };
    for (size_t i = 0; i < sizeof(valid_methods) / sizeof(valid_methods[0]); ++i) {
        if (strcmp(method, valid_methods[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Validates HTTP path: absolute path, no control chars or CRLF */
static int is_valid_http_path(const char *path) {
    if (!path || path[0] != '/') {
        return 0;
    }
    for (const char *p = path; *p != '\0'; ++p) {
        unsigned char c = (unsigned char)*p;
        /* Reject CRLF and non-printable control characters */
        if (c < 0x20 || c == 0x7F) {
            return 0;
        }
    }
    return 1;
}

int alrios_ipc_serialize_http_req(char *buf, size_t max, const char *method, const char *path) {
    if (!buf || max == 0 || !method || !path) {
        return -1;
    }

    if (!is_valid_http_method(method) || !is_valid_http_path(path)) {
        return -1;
    }

    int written = snprintf(buf, max, "%s %s HTTP/1.1\r\n\r\n", method, path);
    if (written < 0 || (size_t)written >= max) {
        return -2;
    }

    return written;
}

int alrios_ipc_serialize_http_resp(char *buf, size_t max, int status_code, const char *reason_phrase, const char *body, size_t body_len) {
    if (!buf || max == 0 || status_code < 100 || status_code > 599) {
        return -1;
    }

    const char *reason = (reason_phrase && reason_phrase[0] != '\0') ? reason_phrase : "OK";
    for (const char *p = reason; *p != '\0'; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c == 0x7F) {
            return -1;
        }
    }

    int header_len = snprintf(buf, max, "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n\r\n", status_code, reason, body_len);
    if (header_len < 0 || (size_t)header_len >= max) {
        return -2;
    }

    if (body_len > 0 && body != NULL) {
        if ((size_t)header_len + body_len >= max) {
            return -2;
        }
        memcpy(buf + header_len, body, body_len);
        buf[header_len + body_len] = '\0';
        return header_len + (int)body_len;
    }

    return header_len;
}

int alrios_ipc_parse_http_req(const char *raw, size_t raw_len, char *method, size_t max_method, char *path, size_t max_path) {
    if (!raw || raw_len == 0 || !method || max_method == 0 || !path || max_path == 0) {
        return -1;
    }

    /* Find first line */
    const char *crlf = memmem(raw, raw_len, "\r\n", 2);
    if (!crlf) {
        return -1;
    }

    size_t line_len = (size_t)(crlf - raw);
    /* Find first space */
    const char *sp1 = memchr(raw, ' ', line_len);
    if (!sp1) {
        return -1;
    }

    size_t m_len = (size_t)(sp1 - raw);
    if (m_len == 0 || m_len >= max_method) {
        return -1;
    }

    /* Find second space */
    const char *p_start = sp1 + 1;
    size_t rem = line_len - (size_t)(p_start - raw);
    const char *sp2 = memchr(p_start, ' ', rem);
    if (!sp2) {
        return -1;
    }

    size_t p_len = (size_t)(sp2 - p_start);
    if (p_len == 0 || p_len >= max_path) {
        return -1;
    }

    /* Check HTTP version string */
    const char *ver_start = sp2 + 1;
    size_t ver_len = line_len - (size_t)(ver_start - raw);
    if (ver_len != 8 || memcmp(ver_start, "HTTP/1.1", 8) != 0) {
        return -1;
    }

    memcpy(method, raw, m_len);
    method[m_len] = '\0';

    memcpy(path, p_start, p_len);
    path[p_len] = '\0';

    if (!is_valid_http_method(method) || !is_valid_http_path(path)) {
        return -1;
    }

    return 0;
}

int alrios_ipc_parse_http_resp(const char *raw, size_t raw_len, int *status_code, char *reason, size_t max_reason) {
    if (!raw || raw_len < 12 || !status_code) {
        return -1;
    }

    if (memcmp(raw, "HTTP/1.1 ", 9) != 0) {
        return -1;
    }

    /* Parse 3-digit status code */
    if (raw[9] < '1' || raw[9] > '5' ||
        raw[10] < '0' || raw[10] > '9' ||
        raw[11] < '0' || raw[11] > '9') {
        return -1;
    }

    int code = (raw[9] - '0') * 100 + (raw[10] - '0') * 10 + (raw[11] - '0');
    *status_code = code;

    if (reason && max_reason > 0) {
        reason[0] = '\0';
        if (raw_len > 12 && raw[12] == ' ') {
            const char *r_start = raw + 13;
            const char *crlf = memmem(r_start, raw_len - 13, "\r\n", 2);
            if (crlf) {
                size_t r_len = (size_t)(crlf - r_start);
                if (r_len >= max_reason) {
                    r_len = max_reason - 1;
                }
                memcpy(reason, r_start, r_len);
                reason[r_len] = '\0';
            }
        }
    }

    return 0;
}

int alrios_ipc_pack_http_req(const alrios_http_request_t *req, uint8_t *out_buf, size_t max_len, size_t *out_len) {
    if (!req || !out_buf || !out_len || max_len < sizeof(alrios_ipc_bin_req_hdr_t)) {
        return -1;
    }

    if (!is_valid_http_method(req->method) || !is_valid_http_path(req->path)) {
        return -1;
    }

    size_t m_len = strlen(req->method);
    size_t p_len = strlen(req->path);
    if (m_len > UINT16_MAX || p_len > UINT16_MAX || req->header_count > ALRIOS_HTTP_MAX_HEADERS) {
        return -1;
    }

    size_t total_size = sizeof(alrios_ipc_bin_req_hdr_t) + m_len + p_len;
    for (size_t i = 0; i < req->header_count; ++i) {
        size_t k_len = strlen(req->headers[i].key);
        size_t v_len = strlen(req->headers[i].value);
        if (k_len > UINT16_MAX || v_len > UINT16_MAX) {
            return -1;
        }
        total_size += sizeof(alrios_ipc_bin_hdr_pair_t) + k_len + v_len;
    }
    total_size += req->body_len;

    if (total_size > max_len || total_size > ALRIOS_IPC_MAX_PAYLOAD) {
        return -2;
    }

    alrios_ipc_bin_req_hdr_t hdr;
    hdr.magic = htonl(ALRIOS_HTTP_REQ_MAGIC);
    hdr.method_len = htons((uint16_t)m_len);
    hdr.path_len = htons((uint16_t)p_len);
    hdr.header_count = htons((uint16_t)req->header_count);
    hdr.reserved = 0;
    hdr.body_len = htonl((uint32_t)req->body_len);

    uint8_t *cursor = out_buf;
    memcpy(cursor, &hdr, sizeof(hdr));
    cursor += sizeof(hdr);

    memcpy(cursor, req->method, m_len);
    cursor += m_len;

    memcpy(cursor, req->path, p_len);
    cursor += p_len;

    for (size_t i = 0; i < req->header_count; ++i) {
        size_t k_len = strlen(req->headers[i].key);
        size_t v_len = strlen(req->headers[i].value);

        alrios_ipc_bin_hdr_pair_t hp;
        hp.key_len = htons((uint16_t)k_len);
        hp.val_len = htons((uint16_t)v_len);

        memcpy(cursor, &hp, sizeof(hp));
        cursor += sizeof(hp);

        memcpy(cursor, req->headers[i].key, k_len);
        cursor += k_len;

        memcpy(cursor, req->headers[i].value, v_len);
        cursor += v_len;
    }

    if (req->body_len > 0 && req->body != NULL) {
        memcpy(cursor, req->body, req->body_len);
        cursor += req->body_len;
    }

    *out_len = (size_t)(cursor - out_buf);
    return 0;
}

int alrios_ipc_unpack_http_req(const uint8_t *in_buf, size_t in_len, alrios_http_request_t *req, uint8_t *body_buf, size_t max_body_len) {
    if (!in_buf || in_len < sizeof(alrios_ipc_bin_req_hdr_t) || !req) {
        return -1;
    }

    memset(req, 0, sizeof(*req));

    const alrios_ipc_bin_req_hdr_t *hdr = (const alrios_ipc_bin_req_hdr_t *)in_buf;
    if (ntohl(hdr->magic) != ALRIOS_HTTP_REQ_MAGIC) {
        return -1;
    }

    size_t m_len = ntohs(hdr->method_len);
    size_t p_len = ntohs(hdr->path_len);
    size_t h_count = ntohs(hdr->header_count);
    size_t b_len = ntohl(hdr->body_len);

    if (m_len == 0 || m_len >= sizeof(req->method) ||
        p_len == 0 || p_len >= sizeof(req->path) ||
        h_count > ALRIOS_HTTP_MAX_HEADERS) {
        return -1;
    }

    const uint8_t *cursor = in_buf + sizeof(alrios_ipc_bin_req_hdr_t);
    size_t remaining = in_len - sizeof(alrios_ipc_bin_req_hdr_t);

    if (remaining < m_len + p_len) {
        return -1;
    }

    memcpy(req->method, cursor, m_len);
    req->method[m_len] = '\0';
    cursor += m_len;
    remaining -= m_len;

    memcpy(req->path, cursor, p_len);
    req->path[p_len] = '\0';
    cursor += p_len;
    remaining -= p_len;

    if (!is_valid_http_method(req->method) || !is_valid_http_path(req->path)) {
        return -1;
    }

    req->header_count = h_count;
    for (size_t i = 0; i < h_count; ++i) {
        if (remaining < sizeof(alrios_ipc_bin_hdr_pair_t)) {
            return -1;
        }

        const alrios_ipc_bin_hdr_pair_t *hp = (const alrios_ipc_bin_hdr_pair_t *)cursor;
        size_t k_len = ntohs(hp->key_len);
        size_t v_len = ntohs(hp->val_len);

        cursor += sizeof(alrios_ipc_bin_hdr_pair_t);
        remaining -= sizeof(alrios_ipc_bin_hdr_pair_t);

        if (k_len >= sizeof(req->headers[i].key) || v_len >= sizeof(req->headers[i].value)) {
            return -1;
        }

        if (remaining < k_len + v_len) {
            return -1;
        }

        memcpy(req->headers[i].key, cursor, k_len);
        req->headers[i].key[k_len] = '\0';
        cursor += k_len;
        remaining -= k_len;

        memcpy(req->headers[i].value, cursor, v_len);
        req->headers[i].value[v_len] = '\0';
        cursor += v_len;
        remaining -= v_len;
    }

    if (b_len > 0) {
        if (remaining < b_len) {
            return -1;
        }
        if (body_buf != NULL && max_body_len >= b_len) {
            memcpy(body_buf, cursor, b_len);
            req->body = body_buf;
        } else {
            req->body = cursor;
        }
        req->body_len = b_len;
    } else {
        req->body = NULL;
        req->body_len = 0;
    }

    return 0;
}

int alrios_ipc_pack_http_resp(const alrios_http_response_t *resp, uint8_t *out_buf, size_t max_len, size_t *out_len) {
    if (!resp || !out_buf || !out_len || max_len < sizeof(alrios_ipc_bin_resp_hdr_t)) {
        return -1;
    }

    if (resp->status_code < 100 || resp->status_code > 599) {
        return -1;
    }

    size_t st_len = strlen(resp->status_text);
    if (st_len > UINT16_MAX || resp->header_count > ALRIOS_HTTP_MAX_HEADERS) {
        return -1;
    }

    size_t total_size = sizeof(alrios_ipc_bin_resp_hdr_t) + st_len;
    for (size_t i = 0; i < resp->header_count; ++i) {
        size_t k_len = strlen(resp->headers[i].key);
        size_t v_len = strlen(resp->headers[i].value);
        if (k_len > UINT16_MAX || v_len > UINT16_MAX) {
            return -1;
        }
        total_size += sizeof(alrios_ipc_bin_hdr_pair_t) + k_len + v_len;
    }
    total_size += resp->body_len;

    if (total_size > max_len || total_size > ALRIOS_IPC_MAX_PAYLOAD) {
        return -2;
    }

    alrios_ipc_bin_resp_hdr_t hdr;
    hdr.magic = htonl(ALRIOS_HTTP_RESP_MAGIC);
    hdr.status_code = htons((uint16_t)resp->status_code);
    hdr.status_text_len = htons((uint16_t)st_len);
    hdr.header_count = htons((uint16_t)resp->header_count);
    hdr.reserved = 0;
    hdr.body_len = htonl((uint32_t)resp->body_len);

    uint8_t *cursor = out_buf;
    memcpy(cursor, &hdr, sizeof(hdr));
    cursor += sizeof(hdr);

    memcpy(cursor, resp->status_text, st_len);
    cursor += st_len;

    for (size_t i = 0; i < resp->header_count; ++i) {
        size_t k_len = strlen(resp->headers[i].key);
        size_t v_len = strlen(resp->headers[i].value);

        alrios_ipc_bin_hdr_pair_t hp;
        hp.key_len = htons((uint16_t)k_len);
        hp.val_len = htons((uint16_t)v_len);

        memcpy(cursor, &hp, sizeof(hp));
        cursor += sizeof(hp);

        memcpy(cursor, resp->headers[i].key, k_len);
        cursor += k_len;

        memcpy(cursor, resp->headers[i].value, v_len);
        cursor += v_len;
    }

    if (resp->body_len > 0 && resp->body != NULL) {
        memcpy(cursor, resp->body, resp->body_len);
        cursor += resp->body_len;
    }

    *out_len = (size_t)(cursor - out_buf);
    return 0;
}

int alrios_ipc_unpack_http_resp(const uint8_t *in_buf, size_t in_len, alrios_http_response_t *resp, uint8_t *body_buf, size_t max_body_len) {
    if (!in_buf || in_len < sizeof(alrios_ipc_bin_resp_hdr_t) || !resp) {
        return -1;
    }

    memset(resp, 0, sizeof(*resp));

    const alrios_ipc_bin_resp_hdr_t *hdr = (const alrios_ipc_bin_resp_hdr_t *)in_buf;
    if (ntohl(hdr->magic) != ALRIOS_HTTP_RESP_MAGIC) {
        return -1;
    }

    int code = (int)ntohs(hdr->status_code);
    if (code < 100 || code > 599) {
        return -1;
    }

    size_t st_len = ntohs(hdr->status_text_len);
    size_t h_count = ntohs(hdr->header_count);
    size_t b_len = ntohl(hdr->body_len);

    if (st_len >= sizeof(resp->status_text) || h_count > ALRIOS_HTTP_MAX_HEADERS) {
        return -1;
    }

    const uint8_t *cursor = in_buf + sizeof(alrios_ipc_bin_resp_hdr_t);
    size_t remaining = in_len - sizeof(alrios_ipc_bin_resp_hdr_t);

    if (remaining < st_len) {
        return -1;
    }

    resp->status_code = code;
    memcpy(resp->status_text, cursor, st_len);
    resp->status_text[st_len] = '\0';
    cursor += st_len;
    remaining -= st_len;

    resp->header_count = h_count;
    for (size_t i = 0; i < h_count; ++i) {
        if (remaining < sizeof(alrios_ipc_bin_hdr_pair_t)) {
            return -1;
        }

        const alrios_ipc_bin_hdr_pair_t *hp = (const alrios_ipc_bin_hdr_pair_t *)cursor;
        size_t k_len = ntohs(hp->key_len);
        size_t v_len = ntohs(hp->val_len);

        cursor += sizeof(alrios_ipc_bin_hdr_pair_t);
        remaining -= sizeof(alrios_ipc_bin_hdr_pair_t);

        if (k_len >= sizeof(resp->headers[i].key) || v_len >= sizeof(resp->headers[i].value)) {
            return -1;
        }

        if (remaining < k_len + v_len) {
            return -1;
        }

        memcpy(resp->headers[i].key, cursor, k_len);
        resp->headers[i].key[k_len] = '\0';
        cursor += k_len;
        remaining -= k_len;

        memcpy(resp->headers[i].value, cursor, v_len);
        resp->headers[i].value[v_len] = '\0';
        cursor += v_len;
        remaining -= v_len;
    }

    if (b_len > 0) {
        if (remaining < b_len) {
            return -1;
        }
        if (body_buf != NULL && max_body_len >= b_len) {
            memcpy(body_buf, cursor, b_len);
            resp->body = body_buf;
        } else {
            resp->body = cursor;
        }
        resp->body_len = b_len;
    } else {
        resp->body = NULL;
        resp->body_len = 0;
    }

    return 0;
}
