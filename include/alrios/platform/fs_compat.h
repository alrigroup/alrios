/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_PLATFORM_FS_COMPAT_H
#define ALRIOS_PLATFORM_FS_COMPAT_H

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif
#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR 0x200
#endif

#ifndef S_ISLNK
#define S_ISLNK(m) (0)
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0x02000000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0x04000000
#endif

#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifndef ELOOP
#define ELOOP 40
#endif

#ifndef FICLONE
#define FICLONE 0x40049409
#endif

#define mkdir(d, m) _mkdir(d)
#define unlink(p) _unlink(p)
#define fsync(fd) _commit(fd)
#ifndef F_RDLCK
#define F_RDLCK 0
#endif
#ifndef F_WRLCK
#define F_WRLCK 1
#endif
#ifndef F_UNLCK
#define F_UNLCK 2
#endif

#define fchmod(fd, m) (0)
#define ftruncate(fd, size) _chsize_s(fd, size)

#ifndef LOCK_SH
#define LOCK_SH 1
#endif
#ifndef LOCK_EX
#define LOCK_EX 2
#endif
#ifndef LOCK_NB
#define LOCK_NB 4
#endif
#ifndef LOCK_UN
#define LOCK_UN 8
#endif

static inline int alrios_win32_flock(int fd, int op) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    if (op & LOCK_UN) {
        return UnlockFileEx(h, 0, MAXDWORD, MAXDWORD, &ov) ? 0 : -1;
    }
    DWORD flags = 0;
    if (op & LOCK_EX) flags |= LOCKFILE_EXCLUSIVE_LOCK;
    if (op & LOCK_NB) flags |= LOCKFILE_FAIL_IMMEDIATELY;
    return LockFileEx(h, flags, 0, MAXDWORD, MAXDWORD, &ov) ? 0 : -1;
}
#define flock(fd, op) alrios_win32_flock(fd, op)

#ifdef __cplusplus
extern "C" {
#endif

static inline int alrios_win32_resolve_path(int dirfd, const char *rel_path, char *out_buf, size_t out_size) {
    if (!rel_path || !out_buf || out_size == 0) {
        errno = EINVAL;
        return -1;
    }
    if (dirfd == AT_FDCWD || (rel_path[0] == '/' || rel_path[0] == '\\' || (rel_path[0] && rel_path[1] == ':'))) {
        if (strlen(rel_path) >= out_size) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(out_buf, rel_path, out_size);
        out_buf[out_size - 1] = '\0';
        return 0;
    }
    HANDLE hDir = (HANDLE)_get_osfhandle(dirfd);
    if (hDir == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    char dir_path[MAX_PATH * 4] = {0};
    DWORD len = GetFinalPathNameByHandleA(hDir, dir_path, sizeof(dir_path), FILE_NAME_NORMALIZED);
    if (len == 0 || len >= sizeof(dir_path)) {
        errno = EBADF;
        return -1;
    }
    const char *actual_dir = dir_path;
    if (strncmp(actual_dir, "\\\\?\\", 4) == 0) {
        actual_dir += 4;
    }
    int n = snprintf(out_buf, out_size, "%s/%s", actual_dir, rel_path);
    if (n < 0 || (size_t)n >= out_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

static inline int openat(int dirfd, const char *path, int flags, ...) {
    char full[MAX_PATH * 4];
    if (alrios_win32_resolve_path(dirfd, path, full, sizeof(full)) != 0) {
        return -1;
    }

    DWORD access = 0;
    if ((flags & O_ACCMODE) == O_RDONLY) access = GENERIC_READ;
    else if ((flags & O_ACCMODE) == O_WRONLY) access = GENERIC_WRITE;
    else if ((flags & O_ACCMODE) == O_RDWR) access = GENERIC_READ | GENERIC_WRITE;

    DWORD creation = OPEN_EXISTING;
    if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) creation = CREATE_NEW;
    else if ((flags & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC)) creation = CREATE_ALWAYS;
    else if (flags & O_CREAT) creation = OPEN_ALWAYS;
    else if (flags & O_TRUNC) creation = TRUNCATE_EXISTING;

    DWORD file_flags = FILE_ATTRIBUTE_NORMAL;
    if (flags & O_NOFOLLOW) {
        file_flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }
    if (flags & O_DIRECTORY) {
        file_flags |= FILE_FLAG_BACKUP_SEMANTICS;
    }

    HANDLE h = CreateFileA(full, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, creation, file_flags, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) errno = ENOENT;
        else if (err == ERROR_ACCESS_DENIED) errno = EACCES;
        else if (err == ERROR_FILE_EXISTS) errno = EEXIST;
        else errno = EIO;
        return -1;
    }

    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileInformationByHandle(h, &info)) {
        if ((flags & O_NOFOLLOW) && (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            CloseHandle(h);
            errno = ELOOP;
            return -1;
        }
        if ((flags & O_DIRECTORY) && !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            CloseHandle(h);
            errno = ENOTDIR;
            return -1;
        }
    }

    int crt_flags = flags & (O_RDONLY | O_WRONLY | O_RDWR | O_APPEND | O_BINARY | O_TEXT);
    int fd = _open_osfhandle((intptr_t)h, crt_flags);
    if (fd < 0) {
        CloseHandle(h);
        errno = EIO;
        return -1;
    }
    return fd;
}

static inline int alrios_win32_link(const char *oldpath, const char *newpath) {
    if (CreateHardLinkA(newpath, oldpath, NULL)) return 0;
    DWORD err = GetLastError();
    if (err == ERROR_NOT_SAME_DEVICE) errno = EXDEV;
    else if (err == ERROR_ACCESS_DENIED) errno = EACCES;
    else if (err == ERROR_FILE_NOT_FOUND) errno = ENOENT;
    else errno = EOPNOTSUPP;
    return -1;
}
#define link(old, nw) alrios_win32_link(old, nw)

static inline int linkat(int olddirfd, const char *oldpath,
                          int newdirfd, const char *newpath, int flags) {
    (void)flags;
    char full_old[MAX_PATH * 4];
    char full_new[MAX_PATH * 4];
    if (alrios_win32_resolve_path(olddirfd, oldpath, full_old, sizeof(full_old)) != 0 ||
        alrios_win32_resolve_path(newdirfd, newpath, full_new, sizeof(full_new)) != 0) {
        return -1;
    }
    return alrios_win32_link(full_old, full_new);
}

static inline int fstatat(int dirfd, const char *path, struct stat *st, int flags) {
    (void)flags;
    int fd = openat(dirfd, path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return -1;
    int res = fstat(fd, st);
    (void)_close(fd);
    return res;
}

static inline int fchmodat(int dirfd, const char *path, mode_t mode, int flag) {
    (void)flag;
    (void)mode;
    char full[MAX_PATH * 4];
    if (alrios_win32_resolve_path(dirfd, path, full, sizeof(full)) != 0) {
        return -1;
    }
    return 0;
}

static inline int mkdirat(int dirfd, const char *path, mode_t mode) {
    (void)mode;
    char full[MAX_PATH * 4];
    if (alrios_win32_resolve_path(dirfd, path, full, sizeof(full)) != 0) {
        return -1;
    }
    if (CreateDirectoryA(full, NULL)) return 0;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        errno = EEXIST;
        return -1;
    }
    errno = EIO;
    return -1;
}

static inline int unlinkat(int dirfd, const char *path, int flag) {
    char full[MAX_PATH * 4];
    if (alrios_win32_resolve_path(dirfd, path, full, sizeof(full)) != 0) {
        return -1;
    }
    if (flag & 0x200) {
        return RemoveDirectoryA(full) ? 0 : -1;
    }
    return DeleteFileA(full) ? 0 : -1;
}

static inline int alrios_win32_mkstemp(char *template_name) {
    if (!template_name) {
        errno = EINVAL;
        return -1;
    }
    size_t len = strlen(template_name);
    if (len < 6 || strcmp(template_name + len - 6, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }
    for (int attempts = 0; attempts < 100; attempts++) {
        unsigned int r = (unsigned int)rand() ^ (unsigned int)GetTickCount();
        snprintf(template_name + len - 6, 7, "%06x", r & 0xFFFFFF);
        HANDLE h = CreateFileA(template_name, GENERIC_READ | GENERIC_WRITE,
                               0, NULL, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            int fd = _open_osfhandle((intptr_t)h, _O_RDWR);
            if (fd >= 0) return fd;
            CloseHandle(h);
            return -1;
        }
        if (GetLastError() != ERROR_FILE_EXISTS) {
            break;
        }
    }
    errno = EEXIST;
    return -1;
}
#define mkstemp(t) alrios_win32_mkstemp(t)

static inline int alrios_win32_pread(int fd, void *buf, size_t count, off_t offset) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)((uint64_t)offset & 0xFFFFFFFF);
    ov.OffsetHigh = (DWORD)(((uint64_t)offset >> 32) & 0xFFFFFFFF);
    DWORD bytes_read = 0;
    if (!ReadFile(h, buf, (DWORD)count, &bytes_read, &ov)) {
        DWORD err = GetLastError();
        if (err == ERROR_HANDLE_EOF) return 0;
        errno = EIO;
        return -1;
    }
    return (int)bytes_read;
}
#define pread(f, b, c, o) alrios_win32_pread(f, b, c, o)

static inline int alrios_win32_pwrite(int fd, const void *buf, size_t count, off_t offset) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)((uint64_t)offset & 0xFFFFFFFF);
    ov.OffsetHigh = (DWORD)(((uint64_t)offset >> 32) & 0xFFFFFFFF);
    DWORD bytes_written = 0;
    if (!WriteFile(h, buf, (DWORD)count, &bytes_written, &ov)) {
        errno = EIO;
        return -1;
    }
    return (int)bytes_written;
}
#define pwrite(f, b, c, o) alrios_win32_pwrite(f, b, c, o)

#if defined(__MINGW32__) || defined(__MINGW64__)
#include <dirent.h>
static inline DIR *alrios_win32_fdopendir(int fd) {
    char dir_path[MAX_PATH * 4];
    HANDLE hDir = (HANDLE)_get_osfhandle(fd);
    if (hDir == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return NULL;
    }
    DWORD len = GetFinalPathNameByHandleA(hDir, dir_path, sizeof(dir_path), FILE_NAME_NORMALIZED);
    if (len == 0 || len >= sizeof(dir_path)) {
        errno = EBADF;
        return NULL;
    }
    const char *actual_dir = dir_path;
    if (strncmp(actual_dir, "\\\\?\\", 4) == 0) {
        actual_dir += 4;
    }
    return opendir(actual_dir);
}
#define fdopendir(fd) alrios_win32_fdopendir(fd)
#endif

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* ALRIOS_PLATFORM_FS_COMPAT_H */
