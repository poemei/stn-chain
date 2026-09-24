/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSFER_ACCEPTANCE_H
#define STN_TRANSFER_ACCEPTANCE_H

#include "stn_transfer_authorization.h"
#include "stn_transfer_binding.h"

/* Validate source-wallet control, then publish one already consensus-accepted
 * transfer through the atomic replay/economic-state binding.
 *
 * This function does not decide consensus acceptance. Authorization failure
 * cannot consume replay state or mutate economic state. */
stn_data_status stn_transfer_accept(
    stn_economic_state *economic,
    stn_transfer_replay_state *replay,
    const stn_transfer *transfer,
    const uint8_t controller[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE]);

#endif
