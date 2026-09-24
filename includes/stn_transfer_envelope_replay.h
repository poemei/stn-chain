/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSFER_ENVELOPE_REPLAY_H
#define STN_TRANSFER_ENVELOPE_REPLAY_H

#include "stn_transfer_envelope.h"

#define STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE \
    (STN_IDENTITY_PUBLIC_KEY_SIZE + STN_TRANSFER_NONCE_SIZE)

typedef enum stn_transfer_envelope_replay_result {
    STN_TRANSFER_ENVELOPE_REPLAY_FRESH=0,
    STN_TRANSFER_ENVELOPE_REPLAY_DUPLICATE,
    STN_TRANSFER_ENVELOPE_REPLAY_CAPACITY,
    STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT
} stn_transfer_envelope_replay_result;

typedef struct stn_transfer_envelope_replay_state {
    uint8_t *consumed;
    size_t consumed_count;
    size_t consumed_capacity;
} stn_transfer_envelope_replay_state;

/* Replay identity follows existing Chain record law: controller identity
 * plus a nonzero 32-byte nonce. Transfer amount/destination are deliberately
 * not the replay discriminator, so identical legitimate payments with distinct
 * nonces remain distinct. */
stn_transfer_envelope_replay_result stn_transfer_envelope_replay_key(
    const stn_transfer_envelope *envelope,
    uint8_t key[STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE]);
void stn_transfer_envelope_replay_initialize(
    stn_transfer_envelope_replay_state *state,uint8_t *consumed,size_t capacity);
stn_transfer_envelope_replay_result stn_transfer_envelope_replay_check(
    const stn_transfer_envelope_replay_state *state,const stn_transfer_envelope *envelope);
stn_transfer_envelope_replay_result stn_transfer_envelope_replay_consume(
    stn_transfer_envelope_replay_state *state,const stn_transfer_envelope *envelope);

#endif
