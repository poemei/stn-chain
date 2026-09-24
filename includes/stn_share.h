/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_SHARE_H
#define STN_SHARE_H

#include "stn_economy.h"
#include "stn_address.h"

#define STN_SHARE_VERSION 1u
#define STN_SHARE_WORK_ID_SIZE 32u
#define STN_SHARE_ID_SIZE 32u
#define STN_SHARE_NONCE_SIZE 8u
#define STN_SHARE_CANONICAL_SIZE 73u

typedef struct stn_share_evidence {
    uint8_t work_id[STN_SHARE_WORK_ID_SIZE];
    stn_address miner;
    uint64_t nonce;
} stn_share_evidence;

/* Canonical share evidence is:
 * version[1] || work_id[32] || miner identifier[32] || nonce[8] big-endian.
 * miner must be an stn0_ identity. The textual address prefix is namespace
 * syntax and is not duplicated in canonical evidence.
 * Share ID = SHA256("STN-CHAIN:SHARE:ID:1" including its terminating NUL ||
 * canonical evidence). Outputs are unchanged on failure. */
stn_data_status stn_share_encode(
    const stn_share_evidence *share,
    uint8_t canonical[STN_SHARE_CANONICAL_SIZE]);

stn_data_status stn_share_id(
    const stn_share_evidence *share,
    uint8_t id[STN_SHARE_ID_SIZE]);

#endif
