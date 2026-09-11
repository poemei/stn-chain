/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_AUTHORITY_H
#define STN_AUTHORITY_H
#include <stddef.h>
#include <stdint.h>
#include "stn_identity.h"
#include "stn_transaction.h"

#define STN_AUTHORITY_ACTION_SIZE 32u
#define STN_AUTHORITY_CONTEXT_SIZE 32u
#define STN_AUTHORITY_EVIDENCE_SIZE 97u
#define STN_AUTHORITY_VERSION 1u
#define STN_AUTHORITY_MAX_ROOTS 16u
#define STN_AUTHORITY_GRANT_SIZE 194u
#define STN_AUTHORITY_GRANT_DOMAIN "STN-CHAIN:AUTHORITY:GRANT:1"
#define STN_AUTHORITY_GRANT_DOMAIN_SIZE 28u
#define STN_AUTHORITY_REVOCATION_SIZE 129u
#define STN_AUTHORITY_REVOKE_DOMAIN "STN-CHAIN:AUTHORITY:REVOKE:1"
#define STN_AUTHORITY_REVOKE_DOMAIN_SIZE 29u
#define STN_AUTHORITY_MAX_REVOKED 256u
#define STN_AUTHORITY_ROTATION_SIZE 129u
#define STN_AUTHORITY_ROTATE_DOMAIN "STN-CHAIN:IDENTITY:ROTATE:1"
#define STN_AUTHORITY_ROTATE_DOMAIN_SIZE 28u
#define STN_AUTHORITY_MAX_ROTATIONS 256u

typedef enum stn_authority_result {
    STN_AUTHORITY_AUTHORIZED=0,
    STN_AUTHORITY_UNAUTHORIZED,
    STN_AUTHORITY_MALFORMED
} stn_authority_result;

typedef enum stn_authority_grant_result {
    STN_AUTHORITY_VALID_GRANT=0,
    STN_AUTHORITY_INVALID_GRANT,
    STN_AUTHORITY_MALFORMED_GRANT
} stn_authority_grant_result;

typedef enum stn_authority_revocation_result {
    STN_AUTHORITY_VALID_REVOCATION=0,
    STN_AUTHORITY_INVALID_REVOCATION,
    STN_AUTHORITY_MALFORMED_REVOCATION
} stn_authority_revocation_result;

typedef struct stn_authority_state {
    uint8_t revoked_ids[STN_AUTHORITY_MAX_REVOKED][32];
    size_t revoked_count;
} stn_authority_state;

typedef struct stn_identity_rotation_state {
    uint8_t initial_identity[32];
    uint8_t current_identity[32];
    uint8_t old_identities[STN_AUTHORITY_MAX_ROTATIONS][32];
    uint8_t new_identities[STN_AUTHORITY_MAX_ROTATIONS][32];
    size_t rotation_count;
} stn_identity_rotation_state;

typedef enum stn_identity_rotation_result {
    STN_AUTHORITY_VALID_ROTATION=0,
    STN_AUTHORITY_INVALID_ROTATION,
    STN_AUTHORITY_MALFORMED_ROTATION
} stn_identity_rotation_result;

/* Action and context are opaque, fixed-width, versioned tokens. Their
 * meanings are assigned by later protocol phases; this primitive compares
 * them exactly and does not infer policy. */
stn_authority_result stn_authority_action_validate(
    const uint8_t action[STN_AUTHORITY_ACTION_SIZE]);
stn_authority_result stn_authority_context_validate(
    const uint8_t context[STN_AUTHORITY_CONTEXT_SIZE]);

stn_authority_result stn_authority_evidence_validate(
    const uint8_t *evidence, size_t evidence_length);

/* Roots are the canonical genesis-declared public-key set, sorted ascending
 * with no duplicates. A zero-count set is valid and grants no issuer power. */
stn_authority_result stn_authority_root_set_validate(
    const uint8_t *roots, size_t root_count);

stn_authority_grant_result stn_authority_grant_statement(
    const uint8_t issuer[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t evidence[STN_AUTHORITY_EVIDENCE_SIZE],
    uint8_t *statement, size_t capacity, size_t *written);

stn_authority_grant_result stn_authority_grant_encode(
    const uint8_t issuer[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t evidence[STN_AUTHORITY_EVIDENCE_SIZE],
    const uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE],
    uint8_t *grant, size_t capacity, size_t *written);

/* Only a root listed in the applicable genesis root set may issue a grant. */
stn_authority_grant_result stn_authority_grant_validate(
    const uint8_t *grant, size_t grant_length,
    const uint8_t *genesis_roots, size_t root_count,
    uint8_t evidence[STN_AUTHORITY_EVIDENCE_SIZE]);

stn_data_status stn_authority_grant_id(const uint8_t *grant, size_t grant_length,
    const stn_hash_provider *provider, uint8_t grant_id[32]);

stn_authority_revocation_result stn_authority_revocation_statement(
    const uint8_t issuer[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t grant_id[32], uint8_t *statement, size_t capacity, size_t *written);

stn_authority_revocation_result stn_authority_revocation_encode(
    const uint8_t issuer[STN_IDENTITY_PUBLIC_KEY_SIZE], const uint8_t grant_id[32],
    const uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE], uint8_t *revocation,
    size_t capacity, size_t *written);

stn_authority_revocation_result stn_authority_revocation_validate(
    const uint8_t *revocation, size_t revocation_length,
    const uint8_t *grant, size_t grant_length,
    const uint8_t *genesis_roots, size_t root_count,
    const stn_hash_provider *provider, uint8_t grant_id[32]);

void stn_authority_state_initialize(stn_authority_state *state);
int stn_authority_state_is_revoked(const stn_authority_state *state,
    const uint8_t grant_id[32]);
stn_authority_revocation_result stn_authority_state_apply(
    stn_authority_state *state, const uint8_t *revocation, size_t revocation_length,
    const uint8_t *grant, size_t grant_length, const uint8_t *genesis_roots,
    size_t root_count, const stn_hash_provider *provider);
stn_authority_grant_result stn_authority_grant_active(
    const uint8_t *grant, size_t grant_length, const uint8_t *genesis_roots,
    size_t root_count, const stn_hash_provider *provider,
    const stn_authority_state *state, uint8_t evidence[STN_AUTHORITY_EVIDENCE_SIZE]);

stn_identity_rotation_result stn_identity_rotation_statement(
    const uint8_t old_identity[32], const uint8_t new_identity[32],
    uint8_t *statement, size_t capacity, size_t *written);
stn_identity_rotation_result stn_identity_rotation_encode(
    const uint8_t old_identity[32], const uint8_t new_identity[32],
    const uint8_t signature[64], uint8_t *rotation, size_t capacity, size_t *written);
stn_identity_rotation_result stn_identity_rotation_validate(
    const uint8_t *rotation, size_t rotation_length,
    const stn_identity_rotation_state *state, const uint8_t *genesis_roots,
    size_t root_count);
void stn_identity_rotation_initialize(stn_identity_rotation_state *state,
    const uint8_t initial_identity[32]);
stn_identity_rotation_result stn_identity_rotation_apply(
    stn_identity_rotation_state *state, const uint8_t *rotation,
    size_t rotation_length, const uint8_t *genesis_roots, size_t root_count);
stn_identity_rotation_result stn_identity_rotation_current(
    const stn_identity_rotation_state *state, uint8_t current_identity[32]);

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
