/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/package/arapp_v2.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ALRIOS_JSON_MAX_INPUT   (1024U * 1024U)
#define ALRIOS_JSON_MAX_DEPTH   64U

#define JSON_OK                 0
#define JSON_ERR_ARGUMENT      -1
#define JSON_ERR_SYNTAX        -2
#define JSON_ERR_OUTPUT        -3
#define JSON_ERR_MEMORY        -4
#define JSON_ERR_UNSUPPORTED   -5

typedef enum json_kind {
    JSON_NULL,
    JSON_BOOL,
    JSON_INTEGER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_kind_t;

typedef struct json_value json_value_t;

typedef struct json_member {
    unsigned char *key;
    size_t key_len;
    json_value_t *value;
} json_member_t;

struct json_value {
    json_kind_t kind;
    union {
        int boolean;
        struct {
            char *bytes;
            size_t len;
        } integer;
        struct {
            unsigned char *bytes;
            size_t len;
        } string;
        struct {
            json_value_t **items;
            size_t count;
            size_t capacity;
        } array;
        struct {
            json_member_t *members;
            size_t count;
            size_t capacity;
        } object;
    } data;
};

typedef struct json_parser {
    const unsigned char *input;
    size_t length;
    size_t offset;
} json_parser_t;

typedef struct json_writer {
    char *output;
    size_t capacity;
    size_t length;
} json_writer_t;

static void json_value_free(json_value_t *value);
static int parse_value(json_parser_t *parser, unsigned int depth, json_value_t **out_value);

static int checked_grow(void **buffer, size_t element_size, size_t *capacity, size_t required) {
    size_t next;
    void *resized;
    if (required <= *capacity) {
        return JSON_OK;
    }
    next = (*capacity == 0U) ? 4U : *capacity;
    while (next < required) {
        if (next > SIZE_MAX / 2U) {
            return JSON_ERR_MEMORY;
        }
        next *= 2U;
    }
    if (element_size != 0U && next > SIZE_MAX / element_size) {
        return JSON_ERR_MEMORY;
    }
    resized = realloc(*buffer, next * element_size);
    if (resized == NULL) {
        return JSON_ERR_MEMORY;
    }
    *buffer = resized;
    *capacity = next;
    return JSON_OK;
}

static json_value_t *json_value_new(json_kind_t kind) {
    json_value_t *value = calloc(1U, sizeof(*value));
    if (value != NULL) {
        value->kind = kind;
    }
    return value;
}

static void json_value_free(json_value_t *value) {
    size_t i;
    if (value == NULL) {
        return;
    }
    switch (value->kind) {
    case JSON_INTEGER:
        free(value->data.integer.bytes);
        break;
    case JSON_STRING:
        free(value->data.string.bytes);
        break;
    case JSON_ARRAY:
        for (i = 0U; i < value->data.array.count; ++i) {
            json_value_free(value->data.array.items[i]);
        }
        free(value->data.array.items);
        break;
    case JSON_OBJECT:
        for (i = 0U; i < value->data.object.count; ++i) {
            free(value->data.object.members[i].key);
            json_value_free(value->data.object.members[i].value);
        }
        free(value->data.object.members);
        break;
    case JSON_NULL:
    case JSON_BOOL:
        break;
    }
    free(value);
}

static void skip_whitespace(json_parser_t *parser) {
    while (parser->offset < parser->length) {
        unsigned char byte = parser->input[parser->offset];
        if (byte != ' ' && byte != '\t' && byte != '\n' && byte != '\r') {
            break;
        }
        ++parser->offset;
    }
}

static int hex_value(unsigned char byte) {
    if (byte >= '0' && byte <= '9') {
        return (int)(byte - '0');
    }
    if (byte >= 'a' && byte <= 'f') {
        return (int)(byte - 'a') + 10;
    }
    if (byte >= 'A' && byte <= 'F') {
        return (int)(byte - 'A') + 10;
    }
    return -1;
}

static int parse_hex_quad(json_parser_t *parser, uint32_t *out_codepoint) {
    uint32_t value = 0U;
    size_t i;
    if (parser->length - parser->offset < 4U) {
        return JSON_ERR_SYNTAX;
    }
    for (i = 0U; i < 4U; ++i) {
        int digit = hex_value(parser->input[parser->offset + i]);
        if (digit < 0) {
            return JSON_ERR_SYNTAX;
        }
        value = (value << 4U) | (uint32_t)digit;
    }
    parser->offset += 4U;
    *out_codepoint = value;
    return JSON_OK;
}

static int append_byte(unsigned char **bytes, size_t *length, size_t *capacity, unsigned char byte) {
    int status = checked_grow((void **)bytes, sizeof(**bytes), capacity, *length + 1U);
    if (status != JSON_OK) {
        return status;
    }
    (*bytes)[*length] = byte;
    ++(*length);
    return JSON_OK;
}

static int append_utf8(unsigned char **bytes, size_t *length, size_t *capacity, uint32_t codepoint) {
    int status;
    if (codepoint <= 0x7FU) {
        return append_byte(bytes, length, capacity, (unsigned char)codepoint);
    }
    if (codepoint <= 0x7FFU) {
        status = append_byte(bytes, length, capacity, (unsigned char)(0xC0U | (codepoint >> 6U)));
        if (status == JSON_OK) {
            status = append_byte(bytes, length, capacity, (unsigned char)(0x80U | (codepoint & 0x3FU)));
        }
        return status;
    }
    if (codepoint <= 0xFFFFU) {
        status = append_byte(bytes, length, capacity, (unsigned char)(0xE0U | (codepoint >> 12U)));
        if (status == JSON_OK) {
            status = append_byte(bytes, length, capacity, (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3FU)));
        }
        if (status == JSON_OK) {
            status = append_byte(bytes, length, capacity, (unsigned char)(0x80U | (codepoint & 0x3FU)));
        }
        return status;
    }
    if (codepoint <= 0x10FFFFU) {
        status = append_byte(bytes, length, capacity, (unsigned char)(0xF0U | (codepoint >> 18U)));
        if (status == JSON_OK) {
            status = append_byte(bytes, length, capacity, (unsigned char)(0x80U | ((codepoint >> 12U) & 0x3FU)));
        }
        if (status == JSON_OK) {
            status = append_byte(bytes, length, capacity, (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3FU)));
        }
        if (status == JSON_OK) {
            status = append_byte(bytes, length, capacity, (unsigned char)(0x80U | (codepoint & 0x3FU)));
        }
        return status;
    }
    return JSON_ERR_SYNTAX;
}

static int utf8_sequence_length(unsigned char lead) {
    if (lead >= 0xC2U && lead <= 0xDFU) {
        return 2;
    }
    if (lead >= 0xE0U && lead <= 0xEFU) {
        return 3;
    }
    if (lead >= 0xF0U && lead <= 0xF4U) {
        return 4;
    }
    return 0;
}

static int append_valid_utf8(json_parser_t *parser, unsigned char **bytes, size_t *length, size_t *capacity) {
    unsigned char lead = parser->input[parser->offset];
    int sequence_length = utf8_sequence_length(lead);
    uint32_t codepoint;
    int i;
    int status;
    if (sequence_length == 0 || parser->length - parser->offset < (size_t)sequence_length) {
        return JSON_ERR_SYNTAX;
    }
    if (sequence_length == 2) {
        codepoint = (uint32_t)(lead & 0x1FU);
    } else if (sequence_length == 3) {
        codepoint = (uint32_t)(lead & 0x0FU);
    } else {
        codepoint = (uint32_t)(lead & 0x07U);
    }
    for (i = 1; i < sequence_length; ++i) {
        unsigned char continuation = parser->input[parser->offset + (size_t)i];
        if ((continuation & 0xC0U) != 0x80U) {
            return JSON_ERR_SYNTAX;
        }
        codepoint = (codepoint << 6U) | (uint32_t)(continuation & 0x3FU);
    }
    if ((sequence_length == 2 && codepoint < 0x80U) ||
        (sequence_length == 3 && codepoint < 0x800U) ||
        (sequence_length == 4 && codepoint < 0x10000U) ||
        codepoint > 0x10FFFFU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU)) {
        return JSON_ERR_SYNTAX;
    }
    for (i = 0; i < sequence_length; ++i) {
        status = append_byte(bytes, length, capacity, parser->input[parser->offset + (size_t)i]);
        if (status != JSON_OK) {
            return status;
        }
    }
    parser->offset += (size_t)sequence_length;
    return JSON_OK;
}

static int parse_string_bytes(json_parser_t *parser, unsigned char **out_bytes, size_t *out_length) {
    unsigned char *bytes = NULL;
    size_t length = 0U;
    size_t capacity = 0U;
    int status = JSON_ERR_SYNTAX;
    if (parser->offset >= parser->length || parser->input[parser->offset] != '"') {
        return JSON_ERR_SYNTAX;
    }
    ++parser->offset;
    while (parser->offset < parser->length) {
        unsigned char byte = parser->input[parser->offset++];
        if (byte == '"') {
            *out_bytes = bytes;
            *out_length = length;
            return JSON_OK;
        }
        if (byte < 0x20U) {
            break;
        }
        if (byte >= 0x80U) {
            --parser->offset;
            status = append_valid_utf8(parser, &bytes, &length, &capacity);
            if (status != JSON_OK) {
                break;
            }
            continue;
        }
        if (byte != '\\') {
            status = append_byte(&bytes, &length, &capacity, byte);
            if (status != JSON_OK) {
                break;
            }
            continue;
        }
        if (parser->offset >= parser->length) {
            break;
        }
        byte = parser->input[parser->offset++];
        switch (byte) {
        case '"': case '\\': case '/':
            status = append_byte(&bytes, &length, &capacity, byte);
            break;
        case 'b': status = append_byte(&bytes, &length, &capacity, '\b'); break;
        case 'f': status = append_byte(&bytes, &length, &capacity, '\f'); break;
        case 'n': status = append_byte(&bytes, &length, &capacity, '\n'); break;
        case 'r': status = append_byte(&bytes, &length, &capacity, '\r'); break;
        case 't': status = append_byte(&bytes, &length, &capacity, '\t'); break;
        case 'u': {
            uint32_t codepoint;
            status = parse_hex_quad(parser, &codepoint);
            if (status != JSON_OK) {
                break;
            }
            if (codepoint >= 0xD800U && codepoint <= 0xDBFFU) {
                uint32_t low;
                if (parser->length - parser->offset < 6U ||
                    parser->input[parser->offset] != '\\' ||
                    parser->input[parser->offset + 1U] != 'u') {
                    status = JSON_ERR_SYNTAX;
                    break;
                }
                parser->offset += 2U;
                status = parse_hex_quad(parser, &low);
                if (status != JSON_OK || low < 0xDC00U || low > 0xDFFFU) {
                    status = JSON_ERR_SYNTAX;
                    break;
                }
                codepoint = 0x10000U + ((codepoint - 0xD800U) << 10U) + (low - 0xDC00U);
            } else if (codepoint >= 0xDC00U && codepoint <= 0xDFFFU) {
                status = JSON_ERR_SYNTAX;
                break;
            }
            status = append_utf8(&bytes, &length, &capacity, codepoint);
            break;
        }
        default:
            status = JSON_ERR_SYNTAX;
            break;
        }
        if (status != JSON_OK) {
            break;
        }
    }
    free(bytes);
    return status;
}

static int parse_literal(json_parser_t *parser, const char *literal, json_kind_t kind, int boolean, json_value_t **out_value) {
    size_t length = strlen(literal);
    json_value_t *value;
    if (parser->length - parser->offset < length ||
        memcmp(parser->input + parser->offset, literal, length) != 0) {
        return JSON_ERR_SYNTAX;
    }
    value = json_value_new(kind);
    if (value == NULL) {
        return JSON_ERR_MEMORY;
    }
    parser->offset += length;
    value->data.boolean = boolean;
    *out_value = value;
    return JSON_OK;
}

static int parse_integer(json_parser_t *parser, json_value_t **out_value) {
    size_t start = parser->offset;
    size_t digits_start;
    size_t length;
    json_value_t *value;
    char *bytes;
    if (parser->input[parser->offset] == '-') {
        ++parser->offset;
        if (parser->offset >= parser->length) {
            return JSON_ERR_SYNTAX;
        }
    }
    digits_start = parser->offset;
    if (parser->input[parser->offset] == '0') {
        ++parser->offset;
        if (parser->offset < parser->length &&
            parser->input[parser->offset] >= '0' &&
            parser->input[parser->offset] <= '9') {
            return JSON_ERR_SYNTAX;
        }
    } else {
        if (parser->input[parser->offset] < '1' || parser->input[parser->offset] > '9') {
            return JSON_ERR_SYNTAX;
        }
        do {
            ++parser->offset;
        } while (parser->offset < parser->length &&
                 parser->input[parser->offset] >= '0' &&
                 parser->input[parser->offset] <= '9');
    }
    if (parser->offset < parser->length &&
        (parser->input[parser->offset] == '.' ||
         parser->input[parser->offset] == 'e' ||
         parser->input[parser->offset] == 'E')) {
        return JSON_ERR_UNSUPPORTED;
    }
    length = parser->offset - start;
    value = json_value_new(JSON_INTEGER);
    if (value == NULL) {
        return JSON_ERR_MEMORY;
    }
    if (parser->input[start] == '-' && parser->offset - digits_start == 1U && parser->input[digits_start] == '0') {
        start = digits_start;
        length = 1U;
    }
    bytes = malloc(length + 1U);
    if (bytes == NULL) {
        json_value_free(value);
        return JSON_ERR_MEMORY;
    }
    memcpy(bytes, parser->input + start, length);
    bytes[length] = '\0';
    value->data.integer.bytes = bytes;
    value->data.integer.len = length;
    *out_value = value;
    return JSON_OK;
}

static int utf8_next(const unsigned char *bytes, size_t length, size_t *offset, uint32_t *codepoint) {
    unsigned char lead = bytes[*offset];
    int count = utf8_sequence_length(lead);
    int i;
    uint32_t value;
    if (lead < 0x80U) {
        *codepoint = lead;
        ++(*offset);
        return JSON_OK;
    }
    if (count == 2) {
        value = (uint32_t)(lead & 0x1FU);
    } else if (count == 3) {
        value = (uint32_t)(lead & 0x0FU);
    } else {
        value = (uint32_t)(lead & 0x07U);
    }
    if (count == 0 || length - *offset < (size_t)count) {
        return JSON_ERR_SYNTAX;
    }
    for (i = 1; i < count; ++i) {
        value = (value << 6U) | (uint32_t)(bytes[*offset + (size_t)i] & 0x3FU);
    }
    *offset += (size_t)count;
    *codepoint = value;
    return JSON_OK;
}

static uint16_t next_utf16_unit(const unsigned char *bytes, size_t length, size_t *offset, uint16_t *pending) {
    uint32_t codepoint = 0U;
    if (*pending != 0U) {
        uint16_t unit = *pending;
        *pending = 0U;
        return unit;
    }
    (void)utf8_next(bytes, length, offset, &codepoint);
    if (codepoint <= 0xFFFFU) {
        return (uint16_t)codepoint;
    }
    codepoint -= 0x10000U;
    *pending = (uint16_t)(0xDC00U + (codepoint & 0x3FFU));
    return (uint16_t)(0xD800U + (codepoint >> 10U));
}

static int compare_keys_raw(const unsigned char *left, size_t left_length, const unsigned char *right, size_t right_length) {
    size_t left_offset = 0U;
    size_t right_offset = 0U;
    uint16_t left_pending = 0U;
    uint16_t right_pending = 0U;
    while (left_offset < left_length || left_pending != 0U) {
        uint16_t left_unit;
        uint16_t right_unit;
        if (right_offset >= right_length && right_pending == 0U) {
            return 1;
        }
        left_unit = next_utf16_unit(left, left_length, &left_offset, &left_pending);
        right_unit = next_utf16_unit(right, right_length, &right_offset, &right_pending);
        if (left_unit < right_unit) {
            return -1;
        }
        if (left_unit > right_unit) {
            return 1;
        }
    }
    return (right_offset < right_length || right_pending != 0U) ? -1 : 0;
}

static int compare_members(const void *left, const void *right) {
    const json_member_t *left_member = left;
    const json_member_t *right_member = right;
    return compare_keys_raw(left_member->key, left_member->key_len, right_member->key, right_member->key_len);
}

static int parse_array(json_parser_t *parser, unsigned int depth, json_value_t **out_value) {
    json_value_t *array = json_value_new(JSON_ARRAY);
    int status = JSON_OK;
    if (array == NULL) {
        return JSON_ERR_MEMORY;
    }
    ++parser->offset;
    skip_whitespace(parser);
    if (parser->offset < parser->length && parser->input[parser->offset] == ']') {
        ++parser->offset;
        *out_value = array;
        return JSON_OK;
    }
    for (;;) {
        json_value_t *item = NULL;
        status = parse_value(parser, depth + 1U, &item);
        if (status != JSON_OK) {
            break;
        }
        status = checked_grow((void **)&array->data.array.items, sizeof(*array->data.array.items), &array->data.array.capacity, array->data.array.count + 1U);
        if (status != JSON_OK) {
            json_value_free(item);
            break;
        }
        array->data.array.items[array->data.array.count++] = item;
        skip_whitespace(parser);
        if (parser->offset >= parser->length) {
            status = JSON_ERR_SYNTAX;
            break;
        }
        if (parser->input[parser->offset] == ']') {
            ++parser->offset;
            *out_value = array;
            return JSON_OK;
        }
        if (parser->input[parser->offset++] != ',') {
            status = JSON_ERR_SYNTAX;
            break;
        }
        skip_whitespace(parser);
    }
    json_value_free(array);
    return status;
}

static int parse_object(json_parser_t *parser, unsigned int depth, json_value_t **out_value) {
    json_value_t *object = json_value_new(JSON_OBJECT);
    int status = JSON_OK;
    if (object == NULL) {
        return JSON_ERR_MEMORY;
    }
    ++parser->offset;
    skip_whitespace(parser);
    if (parser->offset < parser->length && parser->input[parser->offset] == '}') {
        ++parser->offset;
        *out_value = object;
        return JSON_OK;
    }
    for (;;) {
        json_member_t member;
        size_t i;
        memset(&member, 0, sizeof(member));
        status = parse_string_bytes(parser, &member.key, &member.key_len);
        if (status != JSON_OK) {
            break;
        }
        for (i = 0U; i < object->data.object.count; ++i) {
            if (compare_keys_raw(member.key, member.key_len, object->data.object.members[i].key, object->data.object.members[i].key_len) == 0) {
                status = JSON_ERR_SYNTAX;
                break;
            }
        }
        if (status != JSON_OK) {
            free(member.key);
            break;
        }
        skip_whitespace(parser);
        if (parser->offset >= parser->length || parser->input[parser->offset++] != ':') {
            free(member.key);
            status = JSON_ERR_SYNTAX;
            break;
        }
        status = parse_value(parser, depth + 1U, &member.value);
        if (status != JSON_OK) {
            free(member.key);
            break;
        }
        status = checked_grow((void **)&object->data.object.members, sizeof(*object->data.object.members), &object->data.object.capacity, object->data.object.count + 1U);
        if (status != JSON_OK) {
            free(member.key);
            json_value_free(member.value);
            break;
        }
        object->data.object.members[object->data.object.count++] = member;
        skip_whitespace(parser);
        if (parser->offset >= parser->length) {
            status = JSON_ERR_SYNTAX;
            break;
        }
        if (parser->input[parser->offset] == '}') {
            ++parser->offset;
            qsort(object->data.object.members, object->data.object.count, sizeof(*object->data.object.members), compare_members);
            *out_value = object;
            return JSON_OK;
        }
        if (parser->input[parser->offset++] != ',') {
            status = JSON_ERR_SYNTAX;
            break;
        }
        skip_whitespace(parser);
    }
    json_value_free(object);
    return status;
}

static int parse_value(json_parser_t *parser, unsigned int depth, json_value_t **out_value) {
    unsigned char byte;
    if (depth > ALRIOS_JSON_MAX_DEPTH) {
        return JSON_ERR_SYNTAX;
    }
    skip_whitespace(parser);
    if (parser->offset >= parser->length) {
        return JSON_ERR_SYNTAX;
    }
    byte = parser->input[parser->offset];
    if (byte == '{') {
        return parse_object(parser, depth, out_value);
    }
    if (byte == '[') {
        return parse_array(parser, depth, out_value);
    }
    if (byte == '"') {
        json_value_t *value = json_value_new(JSON_STRING);
        int status;
        if (value == NULL) {
            return JSON_ERR_MEMORY;
        }
        status = parse_string_bytes(parser, &value->data.string.bytes, &value->data.string.len);
        if (status != JSON_OK) {
            json_value_free(value);
            return status;
        }
        *out_value = value;
        return JSON_OK;
    }
    if (byte == 'n') {
        return parse_literal(parser, "null", JSON_NULL, 0, out_value);
    }
    if (byte == 't') {
        return parse_literal(parser, "true", JSON_BOOL, 1, out_value);
    }
    if (byte == 'f') {
        return parse_literal(parser, "false", JSON_BOOL, 0, out_value);
    }
    if (byte == '-' || (byte >= '0' && byte <= '9')) {
        return parse_integer(parser, out_value);
    }
    return JSON_ERR_SYNTAX;
}

static int writer_append(json_writer_t *writer, const char *bytes, size_t length) {
    if (length > writer->capacity - writer->length - 1U) {
        return JSON_ERR_OUTPUT;
    }
    if (length != 0U) {
        memcpy(writer->output + writer->length, bytes, length);
        writer->length += length;
    }
    writer->output[writer->length] = '\0';
    return JSON_OK;
}

static int write_string(json_writer_t *writer, const unsigned char *bytes, size_t length) {
    static const char hex[] = "0123456789abcdef";
    size_t i;
    int status = writer_append(writer, "\"", 1U);
    for (i = 0U; status == JSON_OK && i < length; ++i) {
        unsigned char byte = bytes[i];
        const char *escape = NULL;
        if (byte == '"') escape = "\\\"";
        else if (byte == '\\') escape = "\\\\";
        else if (byte == '\b') escape = "\\b";
        else if (byte == '\f') escape = "\\f";
        else if (byte == '\n') escape = "\\n";
        else if (byte == '\r') escape = "\\r";
        else if (byte == '\t') escape = "\\t";
        if (escape != NULL) {
            status = writer_append(writer, escape, 2U);
        } else if (byte < 0x20U) {
            char encoded[6] = {'\\', 'u', '0', '0', hex[(byte >> 4U) & 0x0FU], hex[byte & 0x0FU]};
            status = writer_append(writer, encoded, sizeof(encoded));
        } else {
            status = writer_append(writer, (const char *)&bytes[i], 1U);
        }
    }
    if (status == JSON_OK) {
        status = writer_append(writer, "\"", 1U);
    }
    return status;
}

static int write_value(json_writer_t *writer, const json_value_t *value) {
    size_t i;
    int status;
    switch (value->kind) {
    case JSON_NULL:
        return writer_append(writer, "null", 4U);
    case JSON_BOOL:
        return value->data.boolean ? writer_append(writer, "true", 4U) : writer_append(writer, "false", 5U);
    case JSON_INTEGER:
        return writer_append(writer, value->data.integer.bytes, value->data.integer.len);
    case JSON_STRING:
        return write_string(writer, value->data.string.bytes, value->data.string.len);
    case JSON_ARRAY:
        status = writer_append(writer, "[", 1U);
        for (i = 0U; status == JSON_OK && i < value->data.array.count; ++i) {
            if (i != 0U) {
                status = writer_append(writer, ",", 1U);
            }
            if (status == JSON_OK) {
                status = write_value(writer, value->data.array.items[i]);
            }
        }
        return (status == JSON_OK) ? writer_append(writer, "]", 1U) : status;
    case JSON_OBJECT:
        status = writer_append(writer, "{", 1U);
        for (i = 0U; status == JSON_OK && i < value->data.object.count; ++i) {
            if (i != 0U) {
                status = writer_append(writer, ",", 1U);
            }
            if (status == JSON_OK) {
                status = write_string(writer, value->data.object.members[i].key, value->data.object.members[i].key_len);
            }
            if (status == JSON_OK) {
                status = writer_append(writer, ":", 1U);
            }
            if (status == JSON_OK) {
                status = write_value(writer, value->data.object.members[i].value);
            }
        }
        return (status == JSON_OK) ? writer_append(writer, "}", 1U) : status;
    }
    return JSON_ERR_SYNTAX;
}

int alrios_manifest_canonicalize(const char *in_json, char *out_canon, size_t max_out) {
    json_parser_t parser;
    json_writer_t writer;
    json_value_t *root = NULL;
    size_t input_length = 0U;
    int status;
    if (in_json == NULL || out_canon == NULL || max_out == 0U) {
        return JSON_ERR_ARGUMENT;
    }
    while (input_length <= ALRIOS_JSON_MAX_INPUT && in_json[input_length] != '\0') {
        ++input_length;
    }
    if (input_length == 0U || input_length > ALRIOS_JSON_MAX_INPUT) {
        return JSON_ERR_ARGUMENT;
    }
    parser.input = (const unsigned char *)in_json;
    parser.length = input_length;
    parser.offset = 0U;
    status = parse_value(&parser, 0U, &root);
    if (status == JSON_OK) {
        skip_whitespace(&parser);
        if (parser.offset != parser.length) {
            status = JSON_ERR_SYNTAX;
        }
    }
    if (status == JSON_OK) {
        char *temporary = malloc(max_out);
        if (temporary == NULL) {
            status = JSON_ERR_MEMORY;
        } else {
            writer.output = temporary;
            writer.capacity = max_out;
            writer.length = 0U;
            temporary[0] = '\0';
            status = write_value(&writer, root);
            if (status == JSON_OK) {
                memcpy(out_canon, temporary, writer.length + 1U);
            }
            free(temporary);
        }
    }
    json_value_free(root);
    return status;
}

static void store_u16_be(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8U);
    p[1] = (uint8_t)v;
}

static void store_u32_be(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24U);
    p[1] = (uint8_t)(v >> 16U);
    p[2] = (uint8_t)(v >> 8U);
    p[3] = (uint8_t)v;
}

static void store_u64_be(uint8_t *p, uint64_t v) {
    size_t i;
    for (i = 8U; i > 0U; --i) {
        p[i - 1U] = (uint8_t)v;
        v >>= 8U;
    }
}

static int check_path_traversal_str(const char *val) {
    if (!val || val[0] == '\0') {
        return 0;
    }
    if (val[0] == '/' || val[0] == '\\') {
        return 1;
    }
    if (((val[0] >= 'a' && val[0] <= 'z') || (val[0] >= 'A' && val[0] <= 'Z')) && val[1] == ':') {
        return 1;
    }
    if (strstr(val, "../") || strstr(val, "..\\")) {
        return 1;
    }
    size_t len = strlen(val);
    if (len >= 2U) {
        if (val[len - 2U] == '.' && val[len - 1U] == '.') {
            if (len == 2U || val[len - 3U] == '/' || val[len - 3U] == '\\') {
                return 1;
            }
        }
    }
    return 0;
}

static int check_safe_app_id_str(const char *app_id) {
    if (!app_id || app_id[0] == '\0') {
        return 0;
    }
    if (check_path_traversal_str(app_id)) {
        return 0;
    }
    for (size_t i = 0; app_id[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)app_id[i];
        if (c == '/' || c == '\\' || c == ':' || c <= 32 || c >= 127) {
            return 0;
        }
    }
    return 1;
}

static int extract_top_field(const char *json, size_t json_len, const char *key, char *out_val, size_t out_max) {
    if (!json || json_len == 0U || !key || !out_val || out_max == 0U) {
        return -1;
    }
    const char *p = json;
    const char *end = json + json_len;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
    }
    if (p >= end || *p != '{') {
        return -1;
    }
    p++;
    size_t key_len = strlen(key);
    for (;;) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        if (p >= end || *p == '}') {
            break;
        }
        if (*p != '\"') {
            return -1;
        }
        p++;
        const char *k_start = p;
        while (p < end && *p != '\"') {
            if (*p == '\\') {
                p++;
                if (p >= end) return -1;
            }
            p++;
        }
        if (p >= end || *p != '\"') {
            return -1;
        }
        size_t k_len = (size_t)(p - k_start);
        p++;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        if (p >= end || *p != ':') {
            return -1;
        }
        p++;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        if (p >= end) {
            return -1;
        }
        int is_match = (k_len == key_len && memcmp(k_start, key, key_len) == 0);
        if (*p == '\"') {
            p++;
            size_t val_len = 0U;
            while (p < end && *p != '\"') {
                if (*p == '\\') {
                    p++;
                    if (p >= end) return -1;
                }
                if (is_match) {
                    if (val_len + 1U >= out_max) {
                        return -1;
                    }
                    out_val[val_len++] = *p;
                }
                p++;
            }
            if (p >= end || *p != '\"') {
                return -1;
            }
            p++;
            if (is_match) {
                out_val[val_len] = '\0';
                return 0;
            }
        } else if (*p == '{' || *p == '[') {
            char open_ch = *p;
            char close_ch = (open_ch == '{') ? '}' : ']';
            int nest = 1;
            p++;
            while (p < end && nest > 0) {
                if (*p == '\"') {
                    p++;
                    while (p < end && *p != '\"') {
                        if (*p == '\\') {
                            p++;
                            if (p >= end) return -1;
                        }
                        p++;
                    }
                    if (p < end && *p == '\"') p++;
                } else {
                    if (*p == open_ch) nest++;
                    else if (*p == close_ch) nest--;
                    p++;
                }
            }
            if (nest != 0) return -1;
        } else {
            while (p < end && *p != ',' && *p != '}' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                p++;
            }
        }
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        if (p < end && *p == ',') {
            p++;
        } else if (p < end && *p == '}') {
            break;
        } else {
            return -1;
        }
    }
    return -1;
}

int armake_validate_and_canonicalize_manifest(const char *raw_json, char *out_canon, size_t max_out) {
    if (!raw_json || !out_canon || max_out == 0U) {
        return -1;
    }
    size_t raw_len = strlen(raw_json);
    char *copy = (char *)malloc(raw_len + 1U);
    if (!copy) {
        return -1;
    }
    memcpy(copy, raw_json, raw_len);
    copy[raw_len] = '\0';

    int rc = alrios_manifest_canonicalize(copy, out_canon, max_out);
    free(copy);
    if (rc != 0) {
        return -1;
    }

    char app_id[256];
    size_t canon_len = strlen(out_canon);
    if (extract_top_field(out_canon, canon_len, "app_id", app_id, sizeof(app_id)) != 0 ||
        !check_safe_app_id_str(app_id)) {
        return -1;
    }

    if (!strstr(out_canon, "\"execution_profile\":\"enterprise\"") &&
        !strstr(out_canon, "\"execution_profile\":\"performance\"") &&
        !strstr(out_canon, "\"execution_profile\":\"sovereign\"")) {
        return -1;
    }

    return 0;
}

int armake_check_no_private_keys(const char *manifest_json) {
    if (!manifest_json) return 0;
    if (strstr(manifest_json, ".pem") || strstr(manifest_json, ".key") ||
        strstr(manifest_json, "id_rsa") || strstr(manifest_json, "key.pem") ||
        strstr(manifest_json, "private") || strstr(manifest_json, "secret")) {
        return -1;
    }
    return 0;
}

int armake_build_v2_envelope_buffer(const char *canon_manifest, const uint8_t *payload, size_t payload_len, uint8_t *out_buf, size_t out_cap, size_t *out_written) {
    if (!canon_manifest || !payload || !out_buf || !out_written) {
        return -1;
    }
    size_t canon_len = strlen(canon_manifest);
    size_t total_size = ARAPP_V2_HEADER_PREFIX_SIZE + canon_len + payload_len;
    if (total_size > out_cap) {
        return -1;
    }

    uint32_t container_flags = 0;
    if (strstr(canon_manifest, "\"execution_profile\":\"sovereign\"")) {
        container_flags |= ARAPP_V2_FLAG_PROFILE_SOVEREIGN;
    } else if (strstr(canon_manifest, "\"execution_profile\":\"performance\"")) {
        container_flags |= ARAPP_V2_FLAG_PROFILE_PERFORMANCE;
    } else {
        container_flags |= ARAPP_V2_FLAG_PROFILE_ENTERPRISE;
    }

    if (strstr(canon_manifest, "\"execution_ring\":\"sovereign_trust\"")) {
        container_flags |= ARAPP_V2_FLAG_RING_SOVEREIGN;
    } else {
        container_flags |= ARAPP_V2_FLAG_RING_DEVMODE;
    }

    memset(out_buf, 0, ARAPP_V2_HEADER_PREFIX_SIZE);
    memcpy(out_buf, "ALRIGROUP@ARAPP\0", ARAPP_V2_MAGIC_LEN);
    store_u16_be(out_buf + 16U, ARAPP_V2_FORMAT_VERSION);
    store_u16_be(out_buf + 18U, ARAPP_V2_ARCH_X86_64);
    store_u32_be(out_buf + 20U, container_flags);
    store_u64_be(out_buf + 24U, 1700000000ULL);
    store_u64_be(out_buf + 32U, 1800000000ULL);
    store_u32_be(out_buf + 40U, ARAPP_V2_HEADER_PREFIX_SIZE);
    store_u32_be(out_buf + 44U, (uint32_t)canon_len);
    store_u32_be(out_buf + 48U, (uint32_t)payload_len);

    memcpy(out_buf + ARAPP_V2_HEADER_PREFIX_SIZE, canon_manifest, canon_len);
    memcpy(out_buf + ARAPP_V2_HEADER_PREFIX_SIZE + canon_len, payload, payload_len);

    *out_written = total_size;
    return 0;
}
