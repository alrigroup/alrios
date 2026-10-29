/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

#include "alrios/package/arpkg.h"
#include "alrios/crypto_verify.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_compat(p) _mkdir(p)
#else
#include <dirent.h>
#include <unistd.h>
#define mkdir_compat(p) mkdir(p, 0755)
#endif

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#define ARPKG_MAX_FILES 1024

typedef struct {
    char rel_path[256];
    char full_path[1024];
    uint32_t mode;
    uint64_t size;
} arpkg_scan_item_t;

typedef struct {
    arpkg_scan_item_t items[ARPKG_MAX_FILES];
    size_t count;
} arpkg_file_list_t;

static int is_safe_relative_path(const char *path) {
    if (!path || path[0] == '\0') return 0;

    /* Rejeita caminhos absolutos Unix ou Windows */
    if (path[0] == '/' || path[0] == '\\') return 0;
    if (path[1] == ':' && ((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z'))) {
        return 0;
    }

    /* Analisa cada segmento do caminho contra '..' */
    const char *p = path;
    while (*p) {
        while (*p == '/' || *p == '\\') p++;
        if (!*p) break;

        const char *seg_start = p;
        while (*p && *p != '/' && *p != '\\') p++;
        size_t seg_len = (size_t)(p - seg_start);

        if (seg_len == 2 && seg_start[0] == '.' && seg_start[1] == '.') {
            return 0;
        }
    }
    return 1;
}

static void ensure_parent_dirs(const char *path) {
    char temp[1024];
    size_t len = strlen(path);
    if (len >= sizeof(temp)) return;

    memcpy(temp, path, len + 1U);
    for (size_t i = 1; i < len; i++) {
        if (temp[i] == '/' || temp[i] == '\\') {
            char sep = temp[i];
            temp[i] = '\0';
            (void)mkdir_compat(temp);
            temp[i] = sep;
        }
    }
}

#ifdef _WIN32
static void scan_directory(const char *base_dir, const char *rel_sub, arpkg_file_list_t *list) {
    char search_pattern[1024];
    WIN32_FIND_DATAA fd;
    HANDLE hFind;

    if (list->count >= ARPKG_MAX_FILES) return;
    if (rel_sub && rel_sub[0]) {
        (void)snprintf(search_pattern, sizeof(search_pattern), "%s\\%s\\*", base_dir, rel_sub);
    } else {
        (void)snprintf(search_pattern, sizeof(search_pattern), "%s\\*", base_dir);
    }

    hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0 ||
            strcmp(fd.cFileName, ".git") == 0) {
            continue;
        }

        char sub_rel[256];
        if (rel_sub && rel_sub[0]) {
            (void)snprintf(sub_rel, sizeof(sub_rel), "%s/%s", rel_sub, fd.cFileName);
        } else {
            (void)snprintf(sub_rel, sizeof(sub_rel), "%s", fd.cFileName);
        }

        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            scan_directory(base_dir, sub_rel, list);
        } else {
            if (list->count < ARPKG_MAX_FILES) {
                arpkg_scan_item_t *item = &list->items[list->count++];
                (void)snprintf(item->rel_path, sizeof(item->rel_path), "%s", sub_rel);
                (void)snprintf(item->full_path, sizeof(item->full_path), "%s\\%s", base_dir, sub_rel);
                item->size = ((uint64_t)fd.nFileSizeHigh << 32U) | (uint64_t)fd.nFileSizeLow;
                item->mode = 0644;
            }
        }
    } while (FindNextFileA(hFind, &fd) != 0 && list->count < ARPKG_MAX_FILES);

    FindClose(hFind);
}
#else
static void scan_directory(const char *base_dir, const char *rel_sub, arpkg_file_list_t *list) {
    char current_full[1024];
    DIR *dir;
    struct dirent *entry;

    if (list->count >= ARPKG_MAX_FILES) return;
    if (rel_sub && rel_sub[0]) {
        (void)snprintf(current_full, sizeof(current_full), "%s/%s", base_dir, rel_sub);
    } else {
        (void)snprintf(current_full, sizeof(current_full), "%s", base_dir);
    }

    dir = opendir(current_full);
    if (!dir) return;

    while ((entry = readdir(dir)) != NULL && list->count < ARPKG_MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, ".git") == 0) {
            continue;
        }

        char sub_rel[256];
        if (rel_sub && rel_sub[0]) {
            (void)snprintf(sub_rel, sizeof(sub_rel), "%s/%s", rel_sub, entry->d_name);
        } else {
            (void)snprintf(sub_rel, sizeof(sub_rel), "%s", entry->d_name);
        }

        char item_full[1024];
        (void)snprintf(item_full, sizeof(item_full), "%s/%s", base_dir, sub_rel);

        struct stat st;
        if (stat(item_full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_directory(base_dir, sub_rel, list);
        } else if (S_ISREG(st.st_mode)) {
            arpkg_scan_item_t *item = &list->items[list->count++];
            (void)snprintf(item->rel_path, sizeof(item->rel_path), "%s", sub_rel);
            (void)snprintf(item->full_path, sizeof(item->full_path), "%s", item_full);
            item->size = (uint64_t)st.st_size;
            item->mode = (uint32_t)(st.st_mode & 0777);
        }
    }
    closedir(dir);
}
#endif

int arpkg_is_arpkg(const char *path) {
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char magic[16];
    size_t n = fread(magic, 1, sizeof(magic), f);
    fclose(f);
    if (n != sizeof(magic)) return 0;
    return memcmp(magic, ARPKG_MAGIC, ARPKG_MAGIC_LEN) == 0 ? 1 : 0;
}

int arpkg_get_header(const char *path, arpkg_header_t *out_header) {
    if (!path || !out_header) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(out_header, 1, sizeof(arpkg_header_t), f);
    fclose(f);
    if (n != sizeof(arpkg_header_t)) return -1;
    if (memcmp(out_header->magic, ARPKG_MAGIC, ARPKG_MAGIC_LEN) != 0) return -1;
    return 0;
}

static int derive_key_pbkdf2(const char *passphrase, const uint8_t *salt, uint8_t *out_key) {
    if (!passphrase || !salt || !out_key) return -1;
    size_t pass_len = strlen(passphrase);
    if (PKCS5_PBKDF2_HMAC(passphrase, (int)pass_len, salt, ARPKG_SALT_LEN, 100000,
                          EVP_sha256(), 32, out_key) != 1) {
        return -1;
    }
    return 0;
}

static int build_payload_buffer(const arpkg_file_list_t *list,
                                uint8_t **out_buf, size_t *out_size) {
    size_t total_size = sizeof(uint32_t); /* count */
    for (size_t i = 0; i < list->count; i++) {
        total_size += sizeof(arpkg_toc_entry_t);
        total_size += (size_t)list->items[i].size;
    }

    uint8_t *buf = (uint8_t *)malloc(total_size);
    if (!buf) return -1;

    size_t offset = 0;
    uint32_t count = (uint32_t)list->count;
    memcpy(buf + offset, &count, sizeof(count));
    offset += sizeof(count);

    for (size_t i = 0; i < list->count; i++) {
        const arpkg_scan_item_t *item = &list->items[i];
        arpkg_toc_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        size_t rlen = strlen(item->rel_path);
        if (rlen >= sizeof(entry.path)) rlen = sizeof(entry.path) - 1;
        entry.path_len = (uint16_t)rlen;
        memcpy(entry.path, item->rel_path, rlen);
        entry.mode = item->mode;
        entry.file_size = item->size;

        /* Le o arquivo para calcular SHA256 e copiar */
        FILE *in = fopen(item->full_path, "rb");
        if (!in) {
            free(buf);
            return -1;
        }

        uint8_t *file_data = (uint8_t *)malloc((size_t)item->size + 1U);
        if (!file_data) {
            fclose(in);
            free(buf);
            return -1;
        }
        if (item->size > 0 && fread(file_data, 1, (size_t)item->size, in) != (size_t)item->size) {
            free(file_data);
            fclose(in);
            free(buf);
            return -1;
        }
        fclose(in);

        /* Hash SHA-256 */
        SHA256(file_data, (size_t)item->size, entry.sha256);

        memcpy(buf + offset, &entry, sizeof(entry));
        offset += sizeof(entry);

        if (item->size > 0) {
            memcpy(buf + offset, file_data, (size_t)item->size);
            offset += (size_t)item->size;
        }
        free(file_data);
    }

    *out_buf = buf;
    *out_size = total_size;
    return 0;
}

int arpkg_pack_normal(const char *source_dir, const char *output_file) {
    if (!source_dir || !output_file) return -1;

    arpkg_file_list_t list;
    memset(&list, 0, sizeof(list));
    scan_directory(source_dir, "", &list);

    uint8_t *payload = NULL;
    size_t payload_size = 0;
    if (build_payload_buffer(&list, &payload, &payload_size) != 0) {
        return -1;
    }

    arpkg_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, ARPKG_MAGIC, ARPKG_MAGIC_LEN);
    hdr.version = ARPKG_VERSION;
    hdr.mode = ARPKG_MODE_NORMAL;
    hdr.flags = 0;
    hdr.created_at = (uint64_t)time(NULL);
    hdr.file_count = (uint32_t)list.count;
    hdr.payload_size = (uint64_t)payload_size;

    FILE *out = fopen(output_file, "wb");
    if (!out) {
        free(payload);
        return -1;
    }

    if (fwrite(&hdr, 1, sizeof(hdr), out) != sizeof(hdr) ||
        fwrite(payload, 1, payload_size, out) != payload_size) {
        fclose(out);
        free(payload);
        return -1;
    }

    fclose(out);
    free(payload);
    return 0;
}

int arpkg_pack_encrypted(const char *source_dir, const char *output_file,
                        const char *passphrase, const char *cloud_app_id,
                        const char *cloud_tenant) {
    if (!source_dir || !output_file || !passphrase || passphrase[0] == '\0') {
        return -1;
    }

    arpkg_file_list_t list;
    memset(&list, 0, sizeof(list));
    scan_directory(source_dir, "", &list);

    uint8_t *plaintext = NULL;
    size_t plaintext_len = 0;
    if (build_payload_buffer(&list, &plaintext, &plaintext_len) != 0) {
        return -1;
    }

    arpkg_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, ARPKG_MAGIC, ARPKG_MAGIC_LEN);
    hdr.version = ARPKG_VERSION;
    hdr.mode = ARPKG_MODE_ENCRYPTED;
    hdr.created_at = (uint64_t)time(NULL);
    hdr.file_count = (uint32_t)list.count;
    hdr.payload_size = (uint64_t)plaintext_len;

    if (cloud_app_id && cloud_app_id[0]) {
        size_t id_len = strlen(cloud_app_id);
        if (id_len >= sizeof(hdr.cloud_app_id)) id_len = sizeof(hdr.cloud_app_id) - 1;
        memcpy(hdr.cloud_app_id, cloud_app_id, id_len);
        hdr.flags |= ARPKG_FLAG_CLOUD_BOUND;
    }
    if (cloud_tenant && cloud_tenant[0]) {
        size_t ten_len = strlen(cloud_tenant);
        if (ten_len >= sizeof(hdr.cloud_tenant)) ten_len = sizeof(hdr.cloud_tenant) - 1;
        memcpy(hdr.cloud_tenant, cloud_tenant, ten_len);
    }

    /* Gera salt e IV criptograficos */
    if (RAND_bytes(hdr.salt, ARPKG_SALT_LEN) != 1 ||
        RAND_bytes(hdr.iv, ARPKG_IV_LEN) != 1) {
        free(plaintext);
        return -1;
    }

    /* Deriva chave AES-256 */
    uint8_t aes_key[32];
    if (derive_key_pbkdf2(passphrase, hdr.salt, aes_key) != 0) {
        free(plaintext);
        return -1;
    }

    uint8_t *ciphertext = (uint8_t *)malloc(plaintext_len + 16U);
    if (!ciphertext) {
        alrios_explicit_zeroize(aes_key, sizeof(aes_key));
        free(plaintext);
        return -1;
    }

    /* AAD vincula o header ate antes da tag */
    size_t aad_len = offsetof(arpkg_header_t, tag);
    int crypt_rc = alrios_aes256_gcm_encrypt(plaintext, plaintext_len,
                                            (const uint8_t *)&hdr, aad_len,
                                            aes_key, hdr.iv, ciphertext, hdr.tag);
    alrios_explicit_zeroize(aes_key, sizeof(aes_key));
    alrios_explicit_zeroize(plaintext, plaintext_len);
    free(plaintext);

    if (crypt_rc != ALRIOS_CRYPTO_OK) {
        free(ciphertext);
        return -1;
    }

    FILE *out = fopen(output_file, "wb");
    if (!out) {
        free(ciphertext);
        return -1;
    }

    if (fwrite(&hdr, 1, sizeof(hdr), out) != sizeof(hdr) ||
        fwrite(ciphertext, 1, plaintext_len, out) != plaintext_len) {
        fclose(out);
        free(ciphertext);
        return -1;
    }

    fclose(out);
    free(ciphertext);
    return 0;
}

static int extract_payload_buffer(const uint8_t *payload, size_t payload_len, const char *dest_dir) {
    if (!payload || payload_len < sizeof(uint32_t) || !dest_dir) return -1;

    size_t offset = 0;
    uint32_t file_count = 0;
    memcpy(&file_count, payload + offset, sizeof(file_count));
    offset += sizeof(file_count);

    for (uint32_t i = 0; i < file_count; i++) {
        if (offset + sizeof(arpkg_toc_entry_t) > payload_len) return -1;

        arpkg_toc_entry_t entry;
        memcpy(&entry, payload + offset, sizeof(entry));
        offset += sizeof(entry);

        /* Blindagem contra Path Traversal (ARPKG-Slip / CWE-22) */
        entry.path[sizeof(entry.path) - 1] = '\0';
        if (!is_safe_relative_path(entry.path)) {
            return ARPKG_ERR_PATH_TRAVERSAL;
        }

        if (offset + (size_t)entry.file_size > payload_len) return -1;

        char target_path[1024];
        (void)snprintf(target_path, sizeof(target_path), "%s/%s", dest_dir, entry.path);
        ensure_parent_dirs(target_path);

        FILE *f = fopen(target_path, "wb");
        if (!f) return -1;

        if (entry.file_size > 0) {
            if (fwrite(payload + offset, 1, (size_t)entry.file_size, f) != (size_t)entry.file_size) {
                fclose(f);
                return -1;
            }
            offset += (size_t)entry.file_size;
        }
        fclose(f);

#ifndef _WIN32
        if (entry.mode != 0) {
            chmod(target_path, (mode_t)entry.mode);
        }
#endif
    }
    return 0;
}

int arpkg_unpack(const char *package_file, const char *dest_dir, const char *passphrase) {
    if (!package_file || !dest_dir) return ARPKG_ERR_ARGUMENT;

    FILE *f = fopen(package_file, "rb");
    if (!f) return ARPKG_ERR_IO;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return ARPKG_ERR_IO;
    }
    long total_len = ftell(f);
    if (total_len < (long)sizeof(arpkg_header_t) || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return ARPKG_ERR_IO;
    }
    size_t actual_file_size = (size_t)total_len;

    arpkg_header_t hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return ARPKG_ERR_IO;
    }

    if (memcmp(hdr.magic, ARPKG_MAGIC, ARPKG_MAGIC_LEN) != 0 || hdr.version != ARPKG_VERSION) {
        fclose(f);
        return ARPKG_ERR_ARGUMENT;
    }

    /* Blindagem 1: Limite maximo defensivo contra Decompression Bomb (CWE-400) */
    if (hdr.payload_size > ARPKG_MAX_PAYLOAD_SIZE) {
        fclose(f);
        return ARPKG_ERR_BOMB_LIMIT;
    }

    /* Blindagem 2: Payload declarado nao pode exceder o tamanho fisico no disco */
    size_t available_payload = actual_file_size - sizeof(hdr);
    if ((size_t)hdr.payload_size > available_payload) {
        fclose(f);
        return ARPKG_ERR_BOMB_LIMIT;
    }

    /* Blindagem 3: Limite maximo de arquivos catalogados no pacote */
    if (hdr.file_count > ARPKG_MAX_FILES) {
        fclose(f);
        return ARPKG_ERR_BOMB_LIMIT;
    }

    size_t payload_len = (size_t)hdr.payload_size;
    uint8_t *file_payload = (uint8_t *)malloc(payload_len + 1U);
    if (!file_payload) {
        fclose(f);
        return ARPKG_ERR_BOMB_LIMIT;
    }

    if (fread(file_payload, 1, payload_len, f) != payload_len) {
        free(file_payload);
        fclose(f);
        return ARPKG_ERR_IO;
    }
    fclose(f);

    (void)mkdir_compat(dest_dir);

    if (hdr.mode == ARPKG_MODE_NORMAL) {
        int rc = extract_payload_buffer(file_payload, payload_len, dest_dir);
        free(file_payload);
        return rc;
    } else if (hdr.mode == ARPKG_MODE_ENCRYPTED) {
        if (!passphrase || passphrase[0] == '\0') {
            free(file_payload);
            return ARPKG_ERR_REQUIRES_KEY;
        }

        uint8_t aes_key[32];
        if (derive_key_pbkdf2(passphrase, hdr.salt, aes_key) != 0) {
            free(file_payload);
            return ARPKG_ERR_ARGUMENT;
        }

        uint8_t *plaintext = (uint8_t *)malloc(payload_len + 1U);
        if (!plaintext) {
            alrios_explicit_zeroize(aes_key, sizeof(aes_key));
            free(file_payload);
            return ARPKG_ERR_BOMB_LIMIT;
        }

        size_t aad_len = offsetof(arpkg_header_t, tag);
        int crypt_rc = alrios_aes256_gcm_decrypt(file_payload, payload_len,
                                                (const uint8_t *)&hdr, aad_len,
                                                hdr.tag, aes_key, hdr.iv, plaintext);
        alrios_explicit_zeroize(aes_key, sizeof(aes_key));
        free(file_payload);

        if (crypt_rc != ALRIOS_CRYPTO_OK) {
            alrios_explicit_zeroize(plaintext, payload_len);
            free(plaintext);
            return ARPKG_ERR_AUTH_FAIL;
        }

        int rc = extract_payload_buffer(plaintext, payload_len, dest_dir);
        alrios_explicit_zeroize(plaintext, payload_len);
        free(plaintext);
        return rc;
    }

    free(file_payload);
    return ARPKG_ERR_ARGUMENT;
}
