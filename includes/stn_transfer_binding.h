/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSFER_BINDING_H
#define STN_TRANSFER_BINDING_H

#include "stn_economic_state.h"
#include "stn_transfer_replay.h"

/* Atomic accepted-transfer boundary.
 * A transfer is applied to economic state exactly once. Replay capacity is
 * checked before mutation. Economic state is mutated before replay consumption;
 * because replay capacity was reserved and the canonical transfer was already
 * validated, replay consumption must then succeed deterministically.
 *
 * This boundary does not decide authorization or consensus acceptance. */
stn_data_status stn_transfer_binding_apply(
    stn_economic_state *economic,
    stn_transfer_replay_state *replay,
    const stn_transfer *transfer);

#endif
