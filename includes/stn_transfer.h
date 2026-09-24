/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSFER_H
#define STN_TRANSFER_H

#include "stn_address.h"

#define STN_TRANSFER_VERSION 1u
#define STN_TRANSFER_CANONICAL_SIZE 73u

typedef struct stn_transfer {
    stn_address source;
    stn_address destination;
    uint64_t units;
} stn_transfer;

/* Canonical wallet transfer:
 * version[1] || source stnw0_ identifier[32] ||
 * destination stnw0_ identifier[32] || units[8] big-endian.
 *
 * This is the deterministic value-movement primitive only. It does not prove
 * wallet control, authorize spending, provide replay protection or decide
 * acceptance. Source and destination must differ and units must be nonzero. */
stn_data_status stn_transfer_encode(
    const stn_transfer *transfer,
    uint8_t canonical[STN_TRANSFER_CANONICAL_SIZE]);

stn_data_status stn_transfer_decode(
    const uint8_t *canonical,
    size_t length,
    stn_transfer *out);

#endif
