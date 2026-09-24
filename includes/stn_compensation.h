/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_COMPENSATION_H
#define STN_COMPENSATION_H

#include "stn_address.h"

#define STN_COMPENSATION_DESTINATION_VERSION 1u
#define STN_COMPENSATION_DESTINATION_SIZE 65u

typedef struct stn_compensation_destination {
    stn_address mining_identity;
    stn_address wallet;
} stn_compensation_destination;

/* Canonical identity-to-wallet compensation destination:
 * version[1] || stn0_ identifier[32] || stnw0_ identifier[32].
 *
 * This primitive records an explicit typed relationship only. It does not
 * prove wallet control, authorize issuance, create a balance, or infer a
 * relationship from either address. Those are later Economy consensus rules.
 * Encode/decode are exact and deterministic; outputs remain unchanged on
 * failure. */
stn_data_status stn_compensation_destination_encode(
    const stn_compensation_destination *destination,
    uint8_t canonical[STN_COMPENSATION_DESTINATION_SIZE]);

stn_data_status stn_compensation_destination_decode(
    const uint8_t *canonical,
    size_t length,
    stn_compensation_destination *out);

#endif
