/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_AUTHORITY_H
#define STN_AUTHORITY_H
#include <stddef.h>
#include <stdint.h>
#include "stn_identity.h"

#define STN_AUTHORITY_ACTION_SIZE 32u
#define STN_AUTHORITY_CONTEXT_SIZE 32u
#define STN_AUTHORITY_EVIDENCE_SIZE 97u
#define STN_AUTHORITY_VERSION 1u

typedef enum stn_authority_result {
    STN_AUTHORITY_AUTHORIZED=0,
    STN_AUTHORITY_UNAUTHORIZED,
    STN_AUTHORITY_MALFORMED
} stn_authority_result;

/* Action and context are opaque, fixed-width, versioned tokens. Their
 * meanings are assigned by later protocol phases; this primitive compares
 * them exactly and does not infer policy. */
stn_authority_result stn_authority_action_validate(
    const uint8_t action[STN_AUTHORITY_ACTION_SIZE]);
stn_authority_result stn_authority_context_validate(
    const uint8_t context[STN_AUTHORITY_CONTEXT_SIZE]);

/* Canonical evidence: version || subject identity || action || context. */
stn_authority_result stn_authority_evidence_encode(
    const uint8_t subject[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t action[STN_AUTHORITY_ACTION_SIZE],
    const uint8_t context[STN_AUTHORITY_CONTEXT_SIZE],
    uint8_t *evidence, size_t capacity, size_t *written);

/* No evidence is a structurally absent grant and is unauthorized. Non-empty
 * malformed evidence is malformed. A valid grant is scoped to all three
 * supplied values and never derives authority from signature validity. */
stn_authority_result stn_authority_evaluate(
    const uint8_t identity[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t action[STN_AUTHORITY_ACTION_SIZE],
    const uint8_t context[STN_AUTHORITY_CONTEXT_SIZE],
    const uint8_t *evidence, size_t evidence_length);
#endif
