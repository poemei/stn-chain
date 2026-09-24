/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_SHARE_REPLAY_H
#define STN_SHARE_REPLAY_H

#include <stddef.h>
#include <stdint.h>
#include "stn_share.h"

#define STN_SHARE_REPLAY_KEY_SIZE 72u

typedef enum stn_share_replay_result {
    STN_SHARE_REPLAY_FRESH=0,
    STN_SHARE_REPLAY_DUPLICATE,
    STN_SHARE_REPLAY_CAPACITY,
    STN_SHARE_REPLAY_ARGUMENT
} stn_share_replay_result;

typedef struct stn_share_replay_state {
    uint8_t *consumed;
    size_t consumed_count;
    size_t consumed_capacity;
} stn_share_replay_state;

/* Canonical economic replay key:
 * work_id[32] || stn0_ identifier[32] || nonce[8] big-endian.
 * No clock, arrival order, session identifier or hardware class participates. */
stn_share_replay_result stn_share_replay_key(
    const stn_share_evidence *share,
    uint8_t key[STN_SHARE_REPLAY_KEY_SIZE]);

void stn_share_replay_initialize(
    stn_share_replay_state *state,
    uint8_t *consumed,
    size_t consumed_capacity);

stn_share_replay_result stn_share_replay_check(
    const stn_share_replay_state *state,
    const stn_share_evidence *share);

/* Consume only when the share evidence becomes part of accepted Chain state. */
stn_share_replay_result stn_share_replay_consume(
    stn_share_replay_state *state,
    const stn_share_evidence *share);

#endif
