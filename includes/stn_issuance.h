/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_ISSUANCE_H
#define STN_ISSUANCE_H

#include "stn_compensation.h"

#define STN_ISSUANCE_VERSION 1u
#define STN_ISSUANCE_REASON_SHARE 1u
#define STN_ISSUANCE_REASON_BLOCK 2u
#define STN_ISSUANCE_EVIDENCE_ID_SIZE 32u
#define STN_ISSUANCE_CANONICAL_SIZE 106u
#define STN_ISSUANCE_SHARE_UNITS 1u
#define STN_ISSUANCE_BLOCK_UNITS 100u

typedef struct stn_issuance_record {
    uint8_t reason;
    uint64_t units;
    uint8_t evidence_id[STN_ISSUANCE_EVIDENCE_ID_SIZE];
    stn_compensation_destination destination;
} stn_issuance_record;

/* Canonical mining issuance record:
 * version[1] || reason[1] || units[8] big-endian || evidence_id[32] ||
 * compensation_destination[64 identifiers only].
 *
 * reason determines the only valid amount:
 * SHARE -> 1 unit, BLOCK -> 100 units.
 * The destination is explicitly stn0_ identity -> stnw0_ wallet.
 * This is a canonical economic record primitive only. It does not establish
 * wallet control, accept evidence, mutate balances, or activate issuance. */
stn_data_status stn_issuance_encode(
    const stn_issuance_record *record,
    uint8_t canonical[STN_ISSUANCE_CANONICAL_SIZE]);

stn_data_status stn_issuance_decode(
    const uint8_t *canonical,
    size_t length,
    stn_issuance_record *out);

#endif
