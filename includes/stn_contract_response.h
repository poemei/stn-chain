/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_RESPONSE_H
#define STN_CONTRACT_RESPONSE_H

#include "stn_contract.h"
#include "stn_identity.h"
#include <stddef.h>
#include <stdint.h>

#define STN_CONTRACT_RESPONSE_VERSION 1u
#define STN_CONTRACT_RESPONSE_MAX_TEXT 65536u
#define STN_CONTRACT_RESPONSE_HEADER_SIZE 136u
#define STN_CONTRACT_RESPONSE_MAX_SIZE (STN_CONTRACT_RESPONSE_HEADER_SIZE + STN_CONTRACT_RESPONSE_MAX_TEXT)
#define STN_CONTRACT_RESPONSE_UNSIGNED_HEADER_SIZE 72u

typedef struct stn_contract_response {
    uint16_t version;
    uint8_t contract_id[STN_ADDRESS_ID_SIZE];
    uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE];
    const uint8_t *text;
    uint32_t text_length;
    uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE];
} stn_contract_response;

/* Canonical response wire bytes are STRP, version, reserved=0, contract id,
 * actor public key, text length, signature, then exact response text. */
stn_contract_status stn_contract_response_decode(const uint8_t *input,size_t input_length,stn_contract_response *response);
stn_contract_status stn_contract_response_encode(const stn_contract_response *response,uint8_t *output,size_t capacity,size_t *written);
stn_contract_status stn_contract_response_validate_structure(const uint8_t *input,size_t input_length);

/* Build the exact unsigned bytes signed by the actor. The signature field is
 * deliberately excluded; all contract binding and response text are included. */
stn_contract_status stn_contract_response_statement(const stn_contract_response *response,uint8_t *output,size_t capacity,size_t *written);

/* Validate that the actor is a participant in the immutable accepted DRAFT. */
stn_contract_status stn_contract_response_participant_validate(const stn_contract_response *response,const uint8_t *canonical_draft,size_t canonical_draft_length);

/* Verify the Ed25519 signature over the deterministic response statement. */
stn_contract_status stn_contract_response_signature_verify(const stn_contract_response *response);

#endif
