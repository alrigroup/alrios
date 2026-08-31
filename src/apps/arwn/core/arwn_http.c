/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arwn_http.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Tabela lowcase de 256 bytes (estilo nginx/picohttpparser).          */
/* ------------------------------------------------------------------ */

static const unsigned char lowcase[256] = {
    [0]='\0', [1]=1, [2]=2, [3]=3, [4]=4, [5]=5, [6]=6, [7]=7,
    [8]=8, [9]=9, [10]=10, [11]=11, [12]=12, [13]=13, [14]=14, [15]=15,
    [16]=16, [17]=17, [18]=18, [19]=19, [20]=20, [21]=21, [22]=22, [23]=23,
    [24]=24, [25]=25, [26]=26, [27]=27, [28]=28, [29]=29, [30]=30, [31]=31,
    [32]=' ', [33]='!', [34]='"', [35]='#', [36]='$', [37]='%', [38]='&', [39]='\'',
    [40]='(', [41]=')', [42]='*', [43]='+', [44]=',', [45]='-', [46]='.', [47]='/',
    [48]='0', [49]='1', [50]='2', [51]='3', [52]='4', [53]='5', [54]='6', [55]='7',
    [56]='8', [57]='9', [58]=':', [59]=';', [60]='<', [61]='=', [62]='>', [63]='?',
    [64]='@', [65]='a', [66]='b', [67]='c', [68]='d', [69]='e', [70]='f', [71]='g',
    [72]='h', [73]='i', [74]='j', [75]='k', [76]='l', [77]='m', [78]='n', [79]='o',
    [80]='p', [81]='q', [82]='r', [83]='s', [84]='t', [85]='u', [86]='v', [87]='w',
    [88]='x', [89]='y', [90]='z', [91]='[', [92]='\\', [93]=']', [94]='^', [95]='_',
    [96]='`', [97]='a', [98]='b', [99]='c', [100]='d', [101]='e', [102]='f', [103]='g',
    [104]='h', [105]='i', [106]='j', [107]='k', [108]='l', [109]='m', [110]='n', [111]='o',
    [112]='p', [113]='q', [114]='r', [115]='s', [116]='t', [117]='u', [118]='v', [119]='w',
    [120]='x', [121]='y', [122]='z', [123]='{', [124]='|', [125]='}', [126]='~', [127]=127,
    [128]=128, [129]=129, [130]=130, [131]=131, [132]=132, [133]=133, [134]=134, [135]=135,
    [136]=136, [137]=137, [138]=138, [139]=139, [140]=140, [141]=141, [142]=142, [143]=143,
    [144]=144, [145]=145, [146]=146, [147]=147, [148]=148, [149]=149, [150]=150, [151]=151,
    [152]=152, [153]=153, [154]=154, [155]=155, [156]=156, [157]=157, [158]=158, [159]=159,
    [160]=160, [161]=161, [162]=162, [163]=163, [164]=164, [165]=165, [166]=166, [167]=167,
    [168]=168, [169]=169, [170]=170, [171]=171, [172]=172, [173]=173, [174]=174, [175]=175,
    [176]=176, [177]=177, [178]=178, [179]=179, [180]=180, [181]=181, [182]=182, [183]=183,
    [184]=184, [185]=185, [186]=186, [187]=187, [188]=188, [189]=189, [190]=190, [191]=191,
    [192]=192, [193]=193, [194]=194, [195]=195, [196]=196, [197]=197, [198]=198, [199]=199,
    [200]=200, [201]=201, [202]=202, [203]=203, [204]=204, [205]=205, [206]=206, [207]=207,
    [208]=208, [209]=209, [210]=210, [211]=211, [212]=212, [213]=213, [214]=214, [215]=215,
    [216]=216, [217]=217, [218]=218, [219]=219, [220]=220, [221]=221, [222]=222, [223]=223,
    [224]=224, [225]=225, [226]=226, [227]=227, [228]=228, [229]=229, [230]=230, [231]=231,
    [232]=232, [233]=233, [234]=234, [235]=235, [236]=236, [237]=237, [238]=238, [239]=239,
    [240]=240, [241]=241, [242]=242, [243]=243, [244]=244, [245]=245, [246]=246, [247]=247,
    [248]=248, [249]=249, [250]=250, [251]=251, [252]=252, [253]=253, [254]=254, [255]=255
};

/* ------------------------------------------------------------------ */
/* Hash incremental dos cabeçalhos conhecidos (1 byte por vez).         */
/* ------------------------------------------------------------------ */

static uint32_t hdr_hash_init(void) {
    return 2166136261u;
}

static uint32_t hdr_hash_add(uint32_t h, unsigned char c) {
    return (h ^ c) * 16777619u;
}

static arwn_http_hdr_id_t hdr_id_for(uint32_t h, size_t len,
                                     const unsigned char *name) {
    (void)h;
    /* compara case-insensitive (nome já tem os bytes originais) */
    const char *ref;
    switch (len) {
        case 4:  ref = "host";             break;
        case 5:  ref = "range";            break;
        case 6:  ref = "cookie";           break;
        case 10: ref = "connection";       break;
        case 14: ref = "content-length";   break;
        case 15: ref = "accept-encoding";  break;
        default: return ARWN_HTTP_H_UNKNOWN;
    }
    for (size_t k = 0; k < len; k++) {
        if (lowcase[name[k]] != (unsigned char)ref[k])
            return ARWN_HTTP_H_UNKNOWN;
    }
    switch (len) {
        case 4:  return ARWN_HTTP_H_HOST;
        case 5:  return ARWN_HTTP_H_RANGE;
        case 6:  return ARWN_HTTP_H_COOKIE;
        case 10: return ARWN_HTTP_H_CONNECTION;
        case 14: return ARWN_HTTP_H_CONTENT_LENGTH;
        case 15: return ARWN_HTTP_H_ACCEPT_ENCODING;
    }
    return ARWN_HTTP_H_UNKNOWN;
}

/* ------------------------------------------------------------------ */
/* Parse de inteiro unsigned inline (guarda de overflow estilo HAProxy) */
/* ------------------------------------------------------------------ */

/* Retorna 1 se parseou ok, 0 se falhou (não-dígito ou overflow). */
static int parse_uint64(const char *s, size_t len, uint64_t *out) {
    if (len == 0) return 0;
    uint64_t v = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < '0' || c > '9') return 0;
        uint64_t digit = (uint64_t)(c - '0');
        if (v > (UINT64_MAX - digit) / 10) return 0; /* overflow */
        v = v * 10 + digit;
    }
    *out = v;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Parse principal                                                     */
/* ------------------------------------------------------------------ */

static int find_header_end(const char *buf, size_t len, size_t *end) {
    if (len < 4) return 0;
    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            *end = i + 4;
            return 1;
        }
    }
    return 0;
}

int arwn_http_parse(arwn_http_req_t *req, const char *buf, size_t len) {
    if (!req || !buf) return ARWN_HTTP_BAD;
    memset(req, 0, sizeof(*req));
    req->content_length = -1;
    req->header_end = 0;

    /* requisição acima do cap -> rejeita já de cara */
    if (len > ARWN_HTTP_MAX_REQUEST) return ARWN_HTTP_BAD;

    /* request line sem fim em até 64KB de a's -> precisa concluir */
    size_t scan = 0;
    while (scan < len && buf[scan] != '\n') scan++;
    if (scan >= len) {
        /* sem \n: se já passou do max, BAD; senão NEED_MORE */
        return len >= ARWN_HTTP_MAX_REQUEST ? ARWN_HTTP_BAD : ARWN_HTTP_NEED_MORE;
    }

    /* --- 1. linhas do request --- */
    /* request-line: METHOD SP PATH SP VERSION CRLF */
    size_t i = 0;
    /* acha o fim da request line (CRLF ou LF) */
    while (i < len && buf[i] != '\n') i++;
    if (i >= len) return ARWN_HTTP_NEED_MORE;
    if (i < len && buf[i] == '\n') {
        size_t line_len = i; /* sem o \n */
        if (line_len > 0 && buf[line_len - 1] == '\r') line_len--;
        i++; /* pula o \n */

        /* METHOD SP PATH SP VERSION */
        const char *p = buf;
        const char *end = buf + line_len;

        const char *m = p;
        while (p < end && *p != ' ') p++;
        if (p >= end) return ARWN_HTTP_BAD;
        req->method = m;
        req->method_len = (size_t)(p - m);
        p++; /* pula espaço */

        const char *pa = p;
        while (p < end && *p != ' ') p++;
        if (p >= end) return ARWN_HTTP_BAD;
        const char *qmark = memchr(pa, '?', (size_t)(p - pa));
        req->path = pa;
        req->path_len = qmark ? (size_t)(qmark - pa) : (size_t)(p - pa);
        p++; /* pula espaço */

        req->version = p;
        req->version_len = (size_t)(end - p);

        if (req->method_len == 0 || req->method_len > ARWN_HTTP_MAX_METHOD)
            return ARWN_HTTP_BAD;
        if (req->path_len == 0 || req->path_len > ARWN_HTTP_MAX_PATH)
            return ARWN_HTTP_BAD;
        if (req->version_len == 0 || req->version_len > ARWN_HTTP_MAX_VERSION)
            return ARWN_HTTP_BAD;
    } else {
        return ARWN_HTTP_BAD;
    }

    /* --- 2. headers --- */
    size_t hl = 0; /* offset atual dentro de buf (depois da request line) */
    hl = i;
    int count = 0;
    while (1) {
        if (hl >= len) return ARWN_HTTP_NEED_MORE;

        /* fim dos headers? */
        if (buf[hl] == '\n') {
            /* linha em branco -> fim */
            hl++;
            break;
        }
        if (buf[hl] == '\r' && hl + 1 < len && buf[hl + 1] == '\n') {
            hl += 2;
            break;
        }

        /* acha fim desta linha de header */
        size_t le = hl;
        while (le < len && buf[le] != '\n') le++;
        if (le >= len) return ARWN_HTTP_NEED_MORE;
        size_t hlen = le - hl;
        if (hlen > 0 && buf[le - 1] == '\r') hlen--;
        le++; /* pula o \n */

        if (count >= ARWN_HTTP_MAX_HEADERS) return ARWN_HTTP_BAD;

        /* separa name : value */
        const char *name = buf + hl;
        size_t colon = 0;
        while (colon < hlen && buf[hl + colon] != ':') colon++;
        if (colon == 0 || colon >= hlen) return ARWN_HTTP_BAD;
        size_t name_len = colon;

        const char *value = buf + hl + colon + 1;
        size_t value_len = hlen - colon - 1;
        /* trim de espaços/HT no valor */
        while (value_len > 0 &&
               (value[0] == ' ' || value[0] == '\t')) { value++; value_len--; }
        while (value_len > 0 &&
               (value[value_len - 1] == ' ' || value[value_len - 1] == '\t'))
            value_len--;

        /* valida nome: tchars */
        uint32_t h = hdr_hash_init();
        for (size_t k = 0; k < name_len; k++) {
            unsigned char c = (unsigned char)name[k];
            /* tchar: alfanumérico ou !#$%&'*+-.^_`|~ */
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '!' || c == '#' || c == '$' ||
                  c == '%' || c == '&' || c == '\'' || c == '*' || c == '+' ||
                  c == '-' || c == '.' || c == '^' || c == '_' || c == '`' ||
                  c == '|' || c == '~'))
                return ARWN_HTTP_BAD;
            h = hdr_hash_add(h, lowcase[c]);
        }

        arwn_http_header_t *hd = &req->headers[count];
        hd->name = name;
        hd->name_len = name_len;
        hd->value = value;
        hd->value_len = value_len;
        hd->id = hdr_id_for(h, name_len, (const unsigned char *)name);
        count++;

        hl = le;
    }

    req->header_count = count;

    /* acha o fim dos headers no buffer */
    if (!find_header_end(buf, len, &req->header_end))
        return ARWN_HTTP_BAD;

    /* --- 3. Content-Length (estrito, anti-smuggling) --- */
    uint64_t cl_total = 0;
    int cl_seen = 0;
    for (int k = 0; k < count; k++) {
        if (req->headers[k].id == ARWN_HTTP_H_CONTENT_LENGTH) {
            uint64_t v;
            if (!parse_uint64(req->headers[k].value,
                              req->headers[k].value_len, &v))
                return ARWN_HTTP_BAD; /* hex, negativo, não-dígito → 400 */
            /* Content-Length múltiplo com valores diferentes → 400 */
            if (cl_seen && cl_total != v) return ARWN_HTTP_BAD;
            cl_total = v;
            cl_seen = 1;
        }
    }
    if (cl_seen) {
        if (cl_total > (uint64_t)(ARWN_HTTP_MAX_REQUEST - req->header_end))
            return ARWN_HTTP_BAD; /* corpo acima do cap */
        req->content_length = (int64_t)cl_total;
        req->body_len = (size_t)cl_total;
    }

    /* --- 4. completeza --- */
    size_t total_needed = req->header_end + req->body_len;
    if (len < total_needed) return ARWN_HTTP_NEED_MORE;
    req->complete = 1;
    return ARWN_HTTP_COMPLETE;
}

const char *arwn_http_header(const arwn_http_req_t *req, arwn_http_hdr_id_t id,
                             size_t *value_len) {
    for (int i = 0; i < req->header_count; i++) {
        if (req->headers[i].id == id) {
            if (value_len) *value_len = req->headers[i].value_len;
            return req->headers[i].value;
        }
    }
    if (value_len) *value_len = 0;
    return NULL;
}

int arwn_http_method_is(const arwn_http_req_t *req, const char *m) {
    size_t ml = strlen(m);
    return req->method_len == ml && memcmp(req->method, m, ml) == 0;
}

int arwn_http_header_copy(const arwn_http_req_t *req, arwn_http_hdr_id_t id,
                          char *out, size_t cap) {
    size_t vlen;
    const char *v = arwn_http_header(req, id, &vlen);
    if (!v || cap == 0) return -1;
    if (vlen > cap - 1) vlen = cap - 1;
    memcpy(out, v, vlen);
    out[vlen] = '\0';
    return (int)vlen;
}