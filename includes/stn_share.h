/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_SHARE_H
#define STN_SHARE_H

#include "stn_economy.h"
#include "stn_address.h"

#define STN_SHARE_VERSION 2u
#define STN_SHARE_WORK_ID_SIZE 32u
#define STN_SHARE_ID_SIZE 32u
#define STN_SHARE_NONCE_SIZE 8u
#define STN_SHARE_TEMPLATE_HEADER_SIZE 168u
#define STN_SHARE_BODY_COMMITMENT_SIZE 32u
#define STN_SHARE_CANONICAL_SIZE 273u

typedef struct stn_share_evidence {
    uint8_t work_id[STN_SHARE_WORK_ID_SIZE];
    stn_address miner;
    uint64_t nonce;
    uint8_t template_header[STN_SHARE_TEMPLATE_HEADER_SIZE];
    uint8_t body_commitment[STN_SHARE_BODY_COMMITMENT_SIZE];
} stn_share_evidence;

/* Canonical share evidence v2 is:
 * version[1] || work_id[32] || miner identifier[32] || nonce[8] big-endian ||
 * exact canonical mining header[168] || body commitment[32].
 *
 * Work ID is SHA256("STN-CHAIN:WORK:ID:1" including its terminating NUL ||
 * canonical mining header[168] || body commitment[32]). The retained evidence makes accepted share proof
 * independently reproducible during restart, synchronization and reorg
 * without retaining transient Stratum or pending-pool state.
 *
 * miner must be an stn0_ identity. The textual address prefix is namespace
 * syntax and is not duplicated in canonical evidence.
 * Share ID = SHA256("STN-CHAIN:SHARE:ID:1" including its terminating NUL ||
 * canonical evidence). Outputs are unchanged on failure. */
stn_data_status stn_share_encode(
    const stn_share_evidence *share,
    uint8_t canonical[STN_SHARE_CANONICAL_SIZE]);

/* Decode exact canonical share evidence. The miner namespace is implicit in
 * this record class and is restored as STN_ADDRESS_IDENTITY. Output remains
 * unchanged on failure. */
stn_data_status stn_share_decode(
    const uint8_t canonical[STN_SHARE_CANONICAL_SIZE],
    size_t length,
    stn_share_evidence *out);

stn_data_status stn_share_id(
    const stn_share_evidence *share,
    uint8_t id[STN_SHARE_ID_SIZE]);

/* Independently verify canonical share evidence using its retained mining
 * header. The Work ID is reproduced from that exact header, the share target
 * is derived from its Chain target, only the nonce field is replaced, and the
 * resulting block-header hash must satisfy the Share Target. */
stn_data_status stn_share_verify_evidence(
    const stn_share_evidence *share,
    const stn_hash_provider *provider,
    uint8_t digest[32]);

/* Live-path verification additionally requires the supplied canonical mining
 * template to carry exactly the same 168-byte header as the evidence. */
stn_data_status stn_share_verify(
    const stn_share_evidence *share,
    const uint8_t *canonical_template,
    size_t template_length,
    const stn_hash_provider *provider,
    uint8_t digest[32]);

#endif
