/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_BLOCK_COMPENSATION_H
#define STN_BLOCK_COMPENSATION_H

#include "stn_address.h"

#define STN_BLOCK_COMPENSATION_VERSION 1u
#define STN_BLOCK_COMPENSATION_BLOCK_ID_SIZE 32u
#define STN_BLOCK_COMPENSATION_ID_SIZE 32u
#define STN_BLOCK_COMPENSATION_CANONICAL_SIZE 65u

typedef struct stn_block_compensation_evidence {
    uint8_t block_id[STN_BLOCK_COMPENSATION_BLOCK_ID_SIZE];
    stn_address miner;
} stn_block_compensation_evidence;

/* Canonical solved-block compensation evidence:
 * version[1] || accepted block id[32] || stn0_ miner identifier[32].
 *
 * This record preserves the miner identity supplied with solved work after the
 * solved block itself is accepted. It does not mint units or infer a wallet.
 * Evidence ID = SHA256("STN-CHAIN:BLOCK-COMPENSATION:ID:1" including its NUL
 * || canonical evidence). */
stn_data_status stn_block_compensation_encode(
    const stn_block_compensation_evidence *evidence,
    uint8_t canonical[STN_BLOCK_COMPENSATION_CANONICAL_SIZE]);

stn_data_status stn_block_compensation_decode(
    const uint8_t *canonical,size_t length,
    stn_block_compensation_evidence *out);

stn_data_status stn_block_compensation_id(
    const stn_block_compensation_evidence *evidence,
    uint8_t id[STN_BLOCK_COMPENSATION_ID_SIZE]);

#endif
