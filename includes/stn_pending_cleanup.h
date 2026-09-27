/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_PENDING_CLEANUP_H
#define STN_PENDING_CLEANUP_H

#include "stn_pending.h"

/* Discover pending transactions contained in an already committed candidate.
 * This cleanup path compares canonical transaction IDs only. It deliberately
 * does not reinterpret transaction payload classes: consensus validation owns
 * that responsibility. Failure to discover cleanup entries must never become
 * authority to reject an otherwise valid solved block. */
stn_data_status stn_pending_committed_inclusions(
    const stn_pending *pool,
    const stn_storage_view *candidate,
    const stn_hash_provider *hash,
    uint8_t remove[STN_PENDING_MAX_ENTRIES]);

#endif
