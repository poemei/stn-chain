/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_REPLAY_H
#define STN_REPLAY_H
#include <stddef.h>
#include <stdint.h>
#include "stn_record.h"

#define STN_REPLAY_ID_SIZE 64u

typedef enum stn_replay_result {
    STN_REPLAY_FRESH=0,
    STN_REPLAY_REPLAY,
    STN_REPLAY_MALFORMED
} stn_replay_result;

typedef struct stn_replay_state {
    uint8_t *consumed;
    size_t consumed_count;
    size_t consumed_capacity;
} stn_replay_state;

/* Existing record law defines the replay discriminator as signer identity
 * plus the nonzero 32-byte record nonce. No clock, arrival order, or counter
 * participates. */
stn_replay_result stn_replay_id_from_signer_nonce(
    const uint8_t signer[32], const uint8_t nonce[32], uint8_t replay_id[64]);
stn_replay_result stn_replay_id_from_record(
    const uint8_t *record, size_t record_length, uint8_t replay_id[64]);

void stn_replay_state_initialize(stn_replay_state *state,
    uint8_t *consumed, size_t consumed_capacity);
stn_replay_result stn_replay_state_check(
    const stn_replay_state *state, const uint8_t replay_id[64]);
/* Consume only after the corresponding action is part of accepted state. */
stn_replay_result stn_replay_state_consume(
    stn_replay_state *state, const uint8_t replay_id[64]);
/* Rebuild accepted replay state in canonical history order. */
stn_replay_result stn_replay_state_rebuild(
    stn_replay_state *state, const uint8_t *const *records,
    const size_t *record_lengths, size_t count);
#endif
