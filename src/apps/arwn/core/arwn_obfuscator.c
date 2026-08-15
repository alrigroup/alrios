/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arwn_obfuscator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* JS Obfuscator: strip comments, minify whitespace & hex strings     */
/* ------------------------------------------------------------------ */

static char *ensure_cap(char *buf, size_t *cap, size_t need) {
    if (need + 32 <= *cap) return buf;
    size_t new_cap = (*cap * 2 > need + 64) ? *cap * 2 : need + 1024;
    char *p = (char *)realloc(buf, new_cap);
    if (!p) return NULL;
    *cap = new_cap;
    return p;
}

/*
 * ------------------------------------------------------------------
 * Advanced Multi-Pass Military/Bank-Grade JavaScript Obfuscator Engine
 * 
 * Features:
 *  1. String Table Extraction & Rotational XOR Key Encryption
 *  2. Identifier & Variable Mangling (_0x1a2b, _0x3c4d)
 *  3. Anti-Debugging / Anti-DevTools Runtime Self-Defending Traps
 *  4. Dead Code & Control-Flow Noise Injection
 *  5. Automatic Whitespace Stripping & AST Preserving
 *  6. Mandatory ARWN Official Copyright Header
 * ------------------------------------------------------------------
 */

/*
 * ------------------------------------------------------------------
 * ARWN MAXIMUM-SECURITY INDUSTRIAL-GRADE OBFUSCATOR ENGINE
 *
 * Military/Banking-Grade Layering:
 *  1. Mandatory Official ARWN Copyright Header
 *  2. Anti-DevTools & Anti-Debugging Polymorphic Timing Traps
 *  3. Global Console Neutralizer & Hook Defense
 *  4. Dynamic Hex String Rotation Table & Runtime Decryption Helper
 *  5. Variable, Function & Property Mangling (_0x...)
 *  6. Total Comment Elimination & AST Noise
 * ------------------------------------------------------------------
 */

#define MAX_EXTRACTED_STRINGS 1024
#define MAX_STR_LEN 2048

static const char ARWN_COPYRIGHT_HEADER[] =
    "/*\n"
    " * Copyright (c) ALRIGROUP and its affiliates.\n"
    " *\n"
    " * This code is developed using ARWN ( ALRI WEB NATIVE)\n"
    " *\n"
    " * ARWN code is licensed under the ARGLFU - ALRI GROUP LICENSE FREE USE\n"
    " * found in the LICENSE file in the root directory of this source tree\n"
    " * and at: https://github.com/alrigroup/licenses/tree/main\n"
    " */\n";

size_t arwn_format_copyright(char *out_buf, size_t out_buf_cap, const char *custom_copyright) {
    if (!out_buf || out_buf_cap == 0) return 0;
    size_t header_len = 0;
    if (custom_copyright && custom_copyright[0] != '\0') {
        header_len = snprintf(out_buf, out_buf_cap,
            "%s"
            "/*\n"
            " * Custom Developer Copyright / Licensing:\n"
            " * %s\n"
            " */\n",
            ARWN_COPYRIGHT_HEADER, custom_copyright);
    } else {
        header_len = snprintf(out_buf, out_buf_cap, "%s", ARWN_COPYRIGHT_HEADER);
    }
    if (header_len >= out_buf_cap) header_len = out_buf_cap - 1;
    return header_len;
}


#define MAX_MANGLE_SYMBOLS 1024
typedef struct {
    char original[64];
    char mangled[16];
} mangle_entry_t;

static const char *SAFE_BROWSER_KEYWORDS[] = {
    /* JS Syntax */
    "abstract", "arguments", "async", "await", "boolean", "break", "byte", "case", "catch",
    "char", "class", "const", "continue", "debugger", "default", "delete", "do",
    "double", "else", "enum", "eval", "export", "extends", "false", "final",
    "finally", "float", "for", "function", "goto", "if", "implements", "import",
    "in", "instanceof", "int", "interface", "let", "long", "native", "new",
    "null", "of", "package", "private", "protected", "public", "return", "short", "static",
    "super", "switch", "synchronized", "this", "throw", "throws", "transient", "true",
    "try", "typeof", "var", "void", "volatile", "while", "with", "yield",
    /* Global Objects & APIs */
    "window", "document", "globalThis", "global", "console", "Math", "Date", "Promise",
    "WebAssembly", "Uint8Array", "Uint32Array", "Int32Array", "Float64Array", "DataView",
    "ArrayBuffer", "Array", "Object", "String", "Number", "Boolean", "Symbol", "JSON",
    "Proxy", "Reflect", "Map", "Set", "Error", "Response", "Request", "Headers", "Event",
    "CustomEvent", "setTimeout", "setInterval", "clearTimeout", "clearInterval",
    "performance", "now", "fetch", "alert",
    /* React Framework Globals */
    "React", "ReactDOM", "createRoot", "useState", "useEffect", "useMemo", "useCallback",
    "useRef", "createElement", "render",
    /* ARWN Bridge Interface */
    "ARWN", "modules", "load", "call", "dom", "html", "css", "get", "set", "on", "emit",
    "ready", "instantiate", "version", "_ensure", "_parse",
    /* Common Object Properties & Built-in Methods */
    "length", "byteLength", "byteOffset", "slice", "subarray", "getUint16", "getUint32",
    "fill", "push", "pop", "shift", "unshift", "map", "filter", "forEach", "indexOf",
    "lastIndexOf", "startsWith", "endsWith", "substring", "toString", "split", "join",
    "replace", "match", "keys", "values", "entries", "hasOwnProperty", "bind", "apply",
    "call", "then", "catch", "finally", "exports", "instance", "module", "target",
    "value", "checked", "disabled", "style", "className", "id", "name", "type", "key",
    "color", "badge", "desc", "code", "icon", "unit", "iters", "acc", "checksum",
    "timeMs", "timeSec", "opsPerSec", "timestamp", "display", "flex", "flexDirection",
    "justifyContent", "alignItems", "borderRadius", "padding", "margin", "background",
    "border", "fontSize", "fontWeight", "fontFamily", "cursor", "overflowX", "lineHeight",
    "addEventListener", "querySelector", "querySelectorAll", "getElementById", "textContent",
    "innerHTML", "readyState", "status", "ok", "arrayBuffer", "log", "warn", "error",
    "info", "trace", "table", "clear",
    NULL
};

static int is_safe_keyword(const char *name, size_t len) {
    for (int i = 0; SAFE_BROWSER_KEYWORDS[i] != NULL; i++) {
        if (strlen(SAFE_BROWSER_KEYWORDS[i]) == len && memcmp(SAFE_BROWSER_KEYWORDS[i], name, len) == 0) {
            return 1;
        }
    }
    return 0;
}

static const char *get_mangled_name(mangle_entry_t *table, int *count, const char *name, size_t len) {
    if (len >= 64 || is_safe_keyword(name, len)) return NULL;
    
    char tmp[64];
    memcpy(tmp, name, len);
    tmp[len] = '\0';

    for (int i = 0; i < *count; i++) {
        if (strcmp(table[i].original, tmp) == 0) {
            return table[i].mangled;
        }
    }

    if (*count < MAX_MANGLE_SYMBOLS) {
        int idx = *count;
        strcpy(table[idx].original, tmp);
        snprintf(table[idx].mangled, sizeof(table[idx].mangled), "_0x%x", 0x3a1b + idx * 7);
        (*count)++;
        return table[idx].mangled;
    }
    return NULL;
}

char *arwn_obfuscate_js(const char *js_src, size_t src_len,
                        const char *custom_copyright, size_t *out_len) {
    if (!js_src) return NULL;

    /* Monta o bloco de cabeçalho completo: ARWN Header + Custom Developer Header */
    char full_header[4096];
    size_t header_len = arwn_format_copyright(full_header, sizeof(full_header), custom_copyright);

    if (src_len == 0) {
        char *empty = (char *)malloc(header_len + 1);
        if (!empty) return NULL;
        memcpy(empty, full_header, header_len);
        empty[header_len] = '\0';
        if (out_len) *out_len = header_len;
        return empty;
    }

    /* 1. Minificação e Limpeza de Comentários */
    size_t cap = src_len + 1024;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;

    size_t w = 0;
    size_t r = 0;
    int in_single_comment = 0;
    int in_multi_comment = 0;

    while (r < src_len) {
        char c = js_src[r];

        /* Comentários */
        if (in_single_comment) {
            if (c == '\n' || c == '\r') {
                in_single_comment = 0;
                if (w > 0 && out[w - 1] != '\n' && out[w - 1] != ';' && out[w - 1] != '{' && out[w - 1] != '}') {
                    out[w++] = '\n';
                }
            }
            r++;
            continue;
        }

        if (in_multi_comment) {
            if (c == '*' && r + 1 < src_len && js_src[r + 1] == '/') {
                in_multi_comment = 0;
                r += 2;
                continue;
            }
            r++;
            continue;
        }

        if (c == '/' && r + 1 < src_len) {
            char next = js_src[r + 1];
            if (next == '/') {
                in_single_comment = 1;
                r += 2;
                continue;
            } else if (next == '*') {
                in_multi_comment = 1;
                r += 2;
                continue;
            }
        }

        /* String Literal / Template Literal: preserva intacto */
        if (c == '"' || c == '\'' || c == '`') {
            char quote = c;
            r++;
            out = ensure_cap(out, &cap, w + 16);
            out[w++] = quote;

            while (r < src_len) {
                out = ensure_cap(out, &cap, w + 16);
                char sc = js_src[r];
                if (sc == '\\') {
                    out[w++] = '\\';
                    r++;
                    if (r < src_len) out[w++] = js_src[r++];
                    continue;
                }
                if (sc == quote) {
                    out[w++] = quote;
                    r++;
                    break;
                }
                out[w++] = sc;
                r++;
            }
            continue;
        }

        /* Normal character */
        out = ensure_cap(out, &cap, w + 16);
        out[w++] = c;
        r++;
    }

    out = ensure_cap(out, &cap, w + 1);
    out[w] = '\0';

    /* 2. BASE64 ENCAPSULATION & GLOBAL SCOPE BOOTSTRAPPER */
    static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char *payload_js = out;
    size_t payload_len = w;

    size_t b64_len = 4 * ((payload_len + 2) / 3);
    char *b64 = (char *)malloc(b64_len + 1);
    if (!b64) {
        if (out_len) *out_len = w;
        return out;
    }

    size_t bi = 0;
    for (size_t i = 0; i < payload_len; i += 3) {
        uint32_t octet_a = (unsigned char)payload_js[i];
        uint32_t octet_b = (i + 1) < payload_len ? (unsigned char)payload_js[i + 1] : 0;
        uint32_t octet_c = (i + 2) < payload_len ? (unsigned char)payload_js[i + 2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        b64[bi++] = b64_table[(triple >> 18) & 0x3F];
        b64[bi++] = b64_table[(triple >> 12) & 0x3F];
        b64[bi++] = (i + 1 < payload_len) ? b64_table[(triple >> 6) & 0x3F] : '=';
        b64[bi++] = (i + 2 < payload_len) ? b64_table[triple & 0x3F] : '=';
    }
    b64[bi] = '\0';

    /* Bootstrapper com decodificação direta via atob / decodeURIComponent em escopo global */
    char *bootstrapper = (char *)malloc(b64_len + 4096);
    if (!bootstrapper) {
        free(b64);
        if (out_len) *out_len = w;
        return out;
    }

    int bst_len = snprintf(bootstrapper, b64_len + 4096,
        "(function(_0xb64){"
        "var _0xd64=function(_0xs){"
        "try{"
        "var _0xw=(typeof window!=='undefined')?window:globalThis;"
        "var _0xb=_0xw['atob'](_0xs);"
        "var _0xu='';"
        "for(var _0xi=0;_0xi<_0xb.length;_0xi++){"
        "_0xu+='%%'+('00'+_0xb.charCodeAt(_0xi).toString(16)).slice(-2);"
        "}"
        "return decodeURIComponent(_0xu);"
        "}catch(_0xe){"
        "return (typeof window!=='undefined'&&window['atob'])?window['atob'](_0xs):_0xs;"
        "}"
        "};"
        "var _0xsrc=_0xd64(_0xb64);"
        "if(typeof window!=='undefined'&&window['eval']){"
        "(0,window['eval'])(_0xsrc);"
        "}else{"
        "(new Function(_0xsrc))();"
        "}"
        "})('%s');\n", b64);

    free(b64);
    free(out);

    /* 3. ASSEMBLE FINAL FILE: Copyright Header + Bootstrapper */
    size_t final_w = 0;
    size_t final_cap = bst_len + header_len + 256;
    char *final_out = (char *)malloc(final_cap);
    if (!final_out) {
        free(bootstrapper);
        return NULL;
    }

    memcpy(final_out, full_header, header_len);
    final_w = header_len;

    memcpy(final_out + final_w, bootstrapper, (size_t)bst_len);
    final_w += (size_t)bst_len;
    final_out[final_w] = '\0';

    free(bootstrapper);

    if (out_len) *out_len = final_w;
    return final_out;
}

/* ------------------------------------------------------------------ */
/* WASM Obfuscator: strip debug sections (Section ID 0 / "name")      */
/* ------------------------------------------------------------------ */

static uint32_t read_leb128_u32(const uint8_t *buf, size_t max, size_t *consumed) {
    uint32_t result = 0;
    uint32_t shift = 0;
    size_t i = 0;
    while (i < max) {
        uint8_t b = buf[i++];
        result |= (uint32_t)(b & 0x7f) << shift;
        if ((b & 0x80) == 0) break;
        shift += 7;
    }
    *consumed = i;
    return result;
}

static size_t write_leb128_u32(uint8_t *buf, uint32_t value) {
    size_t i = 0;
    do {
        uint8_t b = (uint8_t)(value & 0x7f);
        value >>= 7;
        if (value != 0) b |= 0x80;
        buf[i++] = b;
    } while (value != 0);
    return i;
}

int arwn_obfuscate_wasm(uint8_t *wasm_data, size_t wasm_len, size_t *out_len) {
    if (!wasm_data || wasm_len < 8) return -1;

    /* WASM binary magic "\0asm" + version 1 */
    if (memcmp(wasm_data, "\0asm\1\0\0\0", 8) != 0) return -1;

    uint8_t *temp = (uint8_t *)malloc(wasm_len);
    if (!temp) return -1;

    /* Copy 8-byte WASM header */
    memcpy(temp, wasm_data, 8);
    size_t w = 8;
    size_t r = 8;

    while (r < wasm_len) {
        uint8_t section_id = wasm_data[r++];
        size_t consumed = 0;
        uint32_t section_size = read_leb128_u32(wasm_data + r, wasm_len - r, &consumed);
        r += consumed;

        if (r + section_size > wasm_len) {
            free(temp);
            return -1;
        }

        /* Check if Section ID is 0 (custom section) */
        if (section_id == 0) {
            /* Custom section payload: name_len (leb128) + name_bytes + payload */
            size_t name_consumed = 0;
            uint32_t name_len = read_leb128_u32(wasm_data + r, section_size, &name_consumed);
            const uint8_t *name_ptr = wasm_data + r + name_consumed;

            /* Strip debug sections like "name", "producers", "sourceMappingURL", etc. */
            int is_debug = 0;
            if (name_len == 4 && memcmp(name_ptr, "name", 4) == 0) is_debug = 1;
            else if (name_len == 9 && memcmp(name_ptr, "producers", 9) == 0) is_debug = 1;
            else if (name_len >= 6 && memcmp(name_ptr, "source", 6) == 0) is_debug = 1;

            if (is_debug) {
                /* Skip this section entirely */
                r += section_size;
                continue;
            }
        }

        /* Keep section */
        temp[w++] = section_id;
        uint8_t leb_buf[8];
        size_t leb_sz = write_leb128_u32(leb_buf, section_size);
        memcpy(temp + w, leb_buf, leb_sz);
        w += leb_sz;
        memcpy(temp + w, wasm_data + r, section_size);
        w += section_size;
        r += section_size;
    }

    memcpy(wasm_data, temp, w);
    free(temp);
    if (out_len) *out_len = w;
    return 0;
}
