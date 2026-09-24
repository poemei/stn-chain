/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSFER_ENVELOPE_AUTHORIZATION_H
#define STN_TRANSFER_ENVELOPE_AUTHORIZATION_H

#include "stn_transfer_envelope.h"

#define STN_TRANSFER_ENVELOPE_AUTHORIZATION_VERSION 1u
#define STN_TRANSFER_ENVELOPE_AUTHORIZATION_DOMAIN "STN-CHAIN:TRANSFER:ENVELOPE:AUTHORIZE:1"
#define STN_TRANSFER_ENVELOPE_AUTHORIZATION_DOMAIN_SIZE 40u
#define STN_TRANSFER_ENVELOPE_AUTHORIZATION_STATEMENT_SIZE \
 (STN_TRANSFER_ENVELOPE_AUTHORIZATION_DOMAIN_SIZE + 1u + STN_IDENTITY_PUBLIC_KEY_SIZE + \
  STN_TRANSFER_NONCE_SIZE + STN_TRANSFER_CANONICAL_SIZE)

/* Signed statement binds controller, nonce and exact canonical transfer.
 * The signature field itself is excluded. */
stn_data_status stn_transfer_envelope_authorization_statement(
 const stn_transfer_envelope *envelope,
 uint8_t statement[STN_TRANSFER_ENVELOPE_AUTHORIZATION_STATEMENT_SIZE]);
stn_data_status stn_transfer_envelope_authorization_verify(
 const stn_transfer_envelope *envelope);

#endif
