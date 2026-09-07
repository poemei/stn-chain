/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. Windows adapter only. */
#ifndef STN_WINDOWS_STORAGE_H
#define STN_WINDOWS_STORAGE_H
#include "stn_storage.h"
#include <wchar.h>
typedef struct stn_windows_storage {
    wchar_t path[260], staging[260], lock_path[260];
    void *lock_handle;
} stn_windows_storage;
/* Local fixed NTFS volume only, existing trusted parent directory, MAX_PATH
 * bounded ordinary file name (no streams/UNC/device names). Context must stay
 * alive and not be copied/reinitialized during a call. All writers must use
 * this adapter/exclusion contract; directory mutation by outsiders unsupported.
 * Leftover staging file fails writes closed; never promoted by startup.
 * No power-loss guarantee for volume metadata/hardware beyond Windows APIs. */
stn_storage_status stn_windows_storage_init(stn_windows_storage *storage,
    const wchar_t *path,stn_storage_provider *provider);
#endif
