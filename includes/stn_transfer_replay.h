/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSFER_REPLAY_H
#define STN_TRANSFER_REPLAY_H

#include "stn_transfer.h"

#define STN_TRANSFER_REPLAY_KEY_SIZE STN_TRANSFER_CANONICAL_SIZE

typedef enum stn_transfer_replay_result {
    STN_TRANSFER_REPLAY_FRESH=0,
    STN_TRANSFER_REPLAY_DUPLICATE,
    STN_TRANSFER_REPLAY_CAPACITY,
    STN_TRANSFER_REPLAY_ARGUMENT
} stn_transfer_replay_result;

typedef struct stn_transfer_replay_state {
    uint8_t *consumed;
    size_t consumed_count;
    size_t consumed_capacity;
} stn_transfer_replay_state;

/* Replay identity is the exact canonical transfer bytes. No clock, arrival
 * order, platform detail or native struct representation participates. */
stn_transfer_replay_result stn_transfer_replay_key(
    const stn_transfer *transfer,
    uint8_t key[STN_TRANSFER_REPLAY_KEY_SIZE]);

void stn_transfer_replay_initialize(
    stn_transfer_replay_state *state,
    uint8_t *consumed,
    size_t consumed_capacity);

stn_transfer_replay_result stn_transfer_replay_check(
    const stn_transfer_replay_state *state,
    const stn_transfer *transfer);

/* Consume only after the transfer has become accepted Chain state. */
stn_transfer_replay_result stn_transfer_replay_consume(
    stn_transfer_replay_state *state,
    const stn_transfer *transfer);

#endif
