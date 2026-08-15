/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWN_OBFUSCATOR_H
#define ARWN_OBFUSCATOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Format full combined copyright header (ARWN + Developer Custom) into out_buf. */
size_t arwn_format_copyright(char *out_buf, size_t out_buf_cap, const char *custom_copyright);

/* Strip comments, whitespaces and hex-encode string literals in JavaScript code.
   Returns newly allocated string (must be freed with free()) and fills *out_len.
   Returns NULL on memory error or invalid input. */
char *arwn_obfuscate_js(const char *js_src, size_t src_len,
                        const char *custom_copyright, size_t *out_len);

/* Strip custom debug sections and non-essential names from WebAssembly bytecode (WASM v1).
   Mutates wasm buffer in-place or returns new size *out_len.
   Returns 0 on success, -1 on error. */
int arwn_obfuscate_wasm(uint8_t *wasm_data, size_t wasm_len, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* ARWN_OBFUSCATOR_H */
