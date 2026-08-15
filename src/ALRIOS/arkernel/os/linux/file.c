/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static int os_file_create(const char *directory, const char *filename, const char *content) {
    struct stat st = {0};
    if (stat(directory, &st) == -1) {
        if (mkdir(directory, 0755) == -1 && errno != EEXIST)
            return -errno;
    }

    size_t dirlen = strlen(directory);
    size_t filelen = strlen(filename);
    char fullpath[4096];
    if (dirlen + 1 + filelen >= sizeof(fullpath))
        return -ENAMETOOLONG;
    memcpy(fullpath, directory, dirlen);
    fullpath[dirlen] = '/';
    memcpy(fullpath + dirlen + 1, filename, filelen + 1);

    int fd = open(fullpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return -errno;

    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    int save_errno = errno;
    close(fd);

    if ((size_t)written != len)
        return written >= 0 ? -1 : -save_errno;

    return 0;
}

static int os_file_delete(const char *path) {
    if (unlink(path) == -1) return -errno;
    return 0;
}

static int os_file_exists(const char *path) {
    return access(path, F_OK) == 0 ? 1 : 0;
}
