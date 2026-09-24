/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSFER_ENVELOPE_ACCEPTANCE_H
#define STN_TRANSFER_ENVELOPE_ACCEPTANCE_H
#include "stn_transfer_envelope_authorization.h"
#include "stn_transfer_envelope_replay.h"
#include "stn_economic_state.h"

/* Apply one already consensus-accepted, authorized envelope atomically.
 * Authorization and replay/capacity are checked before economic mutation.
 * Consensus acceptance remains caller-owned. */
stn_data_status stn_transfer_envelope_accept(
 stn_economic_state *economic,
 stn_transfer_envelope_replay_state *replay,
 const stn_transfer_envelope *envelope);
#endif
