#define _GNU_SOURCE

/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. Linux-only adapter. */
#include "../stn_backend.h"
#include "stn_linux_storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int ordinary_fd(int fd)
{
    struct stat info;

    if(fstat(fd, &info) != 0) {
        return 0;
    }

    return S_ISREG(info.st_mode);
}

static stn_storage_status acquire(void *user)
{
    stn_linux_storage *storage = (stn_linux_storage *)user;
    int fd;

    if(storage == NULL) {
        return STN_STORAGE_ARGUMENT;
    }

    if(storage->lock_fd >= 0) {
        return STN_STORAGE_BUSY;
    }

    fd = open(storage->lock_path,
        O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW,
        0600);

    if(fd < 0) {
        return STN_STORAGE_IO;
    }

    if(!ordinary_fd(fd)) {
        close(fd);
        return STN_STORAGE_IO;
    }

    if(flock(fd, LOCK_EX | LOCK_NB) != 0) {
        int saved = errno;

        close(fd);

        if(saved == EWOULDBLOCK || saved == EAGAIN) {
            return STN_STORAGE_BUSY;
        }

        return STN_STORAGE_IO;
    }

    storage->lock_fd = fd;
    return STN_STORAGE_OK;
}

static void release(void *user)
{
    stn_linux_storage *storage = (stn_linux_storage *)user;

    if(storage == NULL || storage->lock_fd < 0) {
        return;
    }

    (void)flock(storage->lock_fd, LOCK_UN);
    (void)close(storage->lock_fd);
    storage->lock_fd = -1;
}

static stn_storage_status read_snapshot(
    void *user,
    uint8_t *bytes,
    size_t capacity,
    size_t *length)
{
    stn_linux_storage *storage = (stn_linux_storage *)user;
    struct stat info;
    size_t offset = 0;
    int fd;
    stn_storage_status result = STN_STORAGE_IO;

    if(storage == NULL ||
       storage->lock_fd < 0 ||
       bytes == NULL ||
       length == NULL) {
        return STN_STORAGE_IO;
    }

    fd = open(storage->path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);

    if(fd < 0) {
        if(errno == ENOENT) {
            return STN_STORAGE_NOT_FOUND;
        }

        return STN_STORAGE_IO;
    }

    if(!ordinary_fd(fd) || fstat(fd, &info) != 0 || info.st_size < 0) {
        goto done;
    }

    if((uintmax_t)info.st_size > (uintmax_t)SIZE_MAX) {
        result = STN_STORAGE_CAPACITY;
        goto done;
    }

    *length = (size_t)info.st_size;

    if(*length > capacity) {
        result = STN_STORAGE_CAPACITY;
        goto done;
    }

    while(offset < *length) {
        size_t remaining = *length - offset;
        size_t request = remaining > 65536u ? 65536u : remaining;
        ssize_t got = read(fd, bytes + offset, request);

        if(got < 0) {
            if(errno == EINTR) {
                continue;
            }

            goto done;
        }

        if(got == 0) {
            goto done;
        }

        offset += (size_t)got;
    }

    *length = offset;
    result = STN_STORAGE_OK;

done:
    if(close(fd) != 0) {
        result = STN_STORAGE_IO;
    }

    return result;
}

static stn_storage_status replace_snapshot(
    void *user,
    const uint8_t *bytes,
    size_t length)
{
    stn_linux_storage *storage = (stn_linux_storage *)user;
    size_t offset = 0;
    int fd;
    int ok = 1;

    if(storage == NULL ||
       storage->lock_fd < 0 ||
       bytes == NULL ||
       length < STN_STORAGE_OVERHEAD) {
        return STN_STORAGE_IO;
    }

    /*
     * CREATE_NEW equivalent. Never truncate or reuse a previous staging file.
     * The staging path is derived from the authoritative path, keeping rename
     * on the same filesystem.
     */
    fd = open(storage->staging,
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        0600);

    if(fd < 0) {
        return STN_STORAGE_IO;
    }

    while(offset < length) {
        size_t remaining = length - offset;
        size_t request = remaining > 65536u ? 65536u : remaining;
        ssize_t wrote = write(fd, bytes + offset, request);

        if(wrote < 0) {
            if(errno == EINTR) {
                continue;
            }

            ok = 0;
            break;
        }

        if(wrote == 0) {
            ok = 0;
            break;
        }

        offset += (size_t)wrote;
    }

    if(ok && fsync(fd) != 0) {
        ok = 0;
    }

    if(close(fd) != 0) {
        ok = 0;
    }

    /*
     * rename() is the publication point. Because staging and authoritative
     * paths are in the same directory, successful rename replaces the old
     * snapshot atomically within the filesystem namespace.
     */
    if(ok && rename(storage->staging, storage->path) == 0) {
        return STN_STORAGE_OK;
    }

    /*
     * Remove only the staging file created by this operation.
     * The authoritative snapshot is never removed here.
     */
    (void)unlink(storage->staging);

    return STN_STORAGE_IO;
}

stn_storage_status stn_linux_storage_init(
    stn_linux_storage *storage,
    const char *path,
    stn_storage_provider *provider)
{
    stn_linux_storage temporary;
    stn_storage_provider result;
    char parent[PATH_MAX];
    char *slash;
    struct stat info;
    size_t length;

    if(storage == NULL || path == NULL || provider == NULL) {
        return STN_STORAGE_ARGUMENT;
    }

    memset(&temporary, 0, sizeof(temporary));
    memset(&result, 0, sizeof(result));

    temporary.lock_fd = -1;

    length = strlen(path);

    if(length == 0 ||
       length >= sizeof(temporary.path) ||
       length + sizeof(".stage") > sizeof(temporary.staging) ||
       length + sizeof(".lock") > sizeof(temporary.lock_path)) {
        return STN_STORAGE_ARGUMENT;
    }

    memcpy(temporary.path, path, length + 1);

    if(snprintf(temporary.staging,
        sizeof(temporary.staging),
        "%s.stage",
        temporary.path) < 0 ||
       snprintf(temporary.lock_path,
        sizeof(temporary.lock_path),
        "%s.lock",
        temporary.path) < 0) {
        return STN_STORAGE_ARGUMENT;
    }

    /*
     * Require an existing trusted parent directory. The snapshot itself may
     * legitimately not exist yet because storage creation follows init.
     */
    memcpy(parent, temporary.path, length + 1);

    slash = strrchr(parent, '/');

    if(slash == NULL) {
        memcpy(parent, ".", 2);
    } else if(slash == parent) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }

    if(stat(parent, &info) != 0 || !S_ISDIR(info.st_mode)) {
        return STN_STORAGE_IO;
    }

    result.user = storage;
    result.acquire = acquire;
    result.release = release;
    result.read = read_snapshot;
    result.replace = replace_snapshot;

    *storage = temporary;
    *provider = result;

    return STN_STORAGE_OK;
}