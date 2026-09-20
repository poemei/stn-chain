/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. Linux adapter only. */
#ifndef STN_LINUX_STORAGE_H
#define STN_LINUX_STORAGE_H

#include "stn_storage.h"

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct stn_linux_storage {
    char path[PATH_MAX];
    char staging[PATH_MAX];
    char lock_path[PATH_MAX];
    int lock_fd;
} stn_linux_storage;

/*
 * Local Linux filesystem adapter.
 *
 * The supplied path must resolve beneath an existing trusted local directory.
 * The authoritative snapshot, staging file, and lock file remain in the same
 * directory.
 *
 * All cooperating writers must use this adapter and its exclusion contract.
 * A replacement is written completely to a new staging file, fsync()'d, then
 * atomically renamed over the authoritative snapshot.
 *
 * Leftover staging files fail writes closed and are never promoted during
 * startup.
 *
 * The context must remain alive and must not be copied or reinitialized while
 * provider operations are active.
 */
stn_storage_status stn_linux_storage_init(
    stn_linux_storage *storage,
    const char *path,
    stn_storage_provider *provider);

#endif