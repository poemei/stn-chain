/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_BLOCK_COMPENSATION_REPLAY_H
#define STN_BLOCK_COMPENSATION_REPLAY_H

#include "stn_block_compensation.h"

#define STN_BLOCK_COMPENSATION_REPLAY_CAPACITY 4096u

typedef struct stn_block_compensation_replay {
    uint8_t ids[STN_BLOCK_COMPENSATION_REPLAY_CAPACITY][STN_BLOCK_COMPENSATION_ID_SIZE];
    size_t count;
} stn_block_compensation_replay;

void stn_block_compensation_replay_initialize(stn_block_compensation_replay *state);
stn_data_status stn_block_compensation_replay_consume(
    stn_block_compensation_replay *state,
    const uint8_t evidence_id[STN_BLOCK_COMPENSATION_ID_SIZE]);
/* Derive the canonical evidence ID before consuming it. No caller-supplied
 * identifier can substitute for the accepted evidence representation. */
stn_data_status stn_block_compensation_replay_consume_evidence(
    stn_block_compensation_replay *state,
    const stn_block_compensation_evidence *evidence);

#endif
