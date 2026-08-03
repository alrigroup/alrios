/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef AR_ARAPP_H
#define AR_ARAPP_H

#define AR_APP_NAME_MAX        64
#define AR_APP_VERSION_MAX     16
#define AR_RUNTIME_MAX         32
#define AR_ENTRY_MAX           256
#define AR_SERVICE_NAME_MAX    64
#define AR_APP_MAX_SERVICES    16
#define AR_MAX_FILES           64
#define AR_FILE_PATH_MAX       256
#define AR_MAX_REQUIRES        8
#define AR_REQUIRE_MAX         64
#define AR_PLATFORM_RAW_MAX    1024

/* Platform detection result (max 32 chars) */
void ar_platform_detect(char *out, int max);

typedef enum {
    AR_RUNTIME_NATIVE,
    AR_RUNTIME_PYTHON3,
    AR_RUNTIME_NODE,
    AR_RUNTIME_JAVA,
    AR_RUNTIME_LUA,
    AR_RUNTIME_RUBY,
    AR_RUNTIME_GO,
    AR_RUNTIME_UNKNOWN
} ar_runtime_t;

typedef struct {
    char name[AR_SERVICE_NAME_MAX];
    char entry[AR_ENTRY_MAX];
} ar_service_def_t;

typedef struct {
    char name[AR_APP_NAME_MAX];
    char version[AR_APP_VERSION_MAX];
    char bin[AR_FILE_PATH_MAX];        /* output filename for packager */
    ar_runtime_t runtime;
    char entry[AR_ENTRY_MAX];
    int  service_count;
    ar_service_def_t services[AR_APP_MAX_SERVICES];
    char files[AR_MAX_FILES][AR_FILE_PATH_MAX];
    int  file_count;
    char files_windows[AR_MAX_FILES][AR_FILE_PATH_MAX];
    int  files_windows_count;
    char files_linux[AR_MAX_FILES][AR_FILE_PATH_MAX];
    int  files_linux_count;

    int  is_runtime;                       /* 1 if this is a runtime package */
    int  requires_count;
    char requires[AR_MAX_REQUIRES][AR_REQUIRE_MAX];
    char platforms_raw[AR_PLATFORM_RAW_MAX]; /* raw JSON of platforms object */
    char build_command[512];                 /* build.command from manifest */
} ar_app_manifest_t;

int ar_manifest_parse(const char *json, ar_app_manifest_t *out);

/* Get the platform-specific entry path from platforms_raw.
   Returns 0 on success, -1 if not found. */
int ar_manifest_get_platform_entry(const ar_app_manifest_t *m,
                                        const char *platform,
                                        char *out, int max);

#endif
