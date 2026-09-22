/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_H
#define STN_CONTRACT_H

#include "stn_address.h"
#include "stn_authority.h"
#include "stn_platform.h"

#include <stddef.h>

#define STN_CONTRACT_VERSION 1u

#define STN_CONTRACT_AUTHORITY_ACTION_DOMAIN 0x43u
#define STN_CONTRACT_AUTHORITY_CONTEXT_DOMAIN 0x43u

#define STN_CONTRACT_HEADER_SIZE 32u
#define STN_CONTRACT_PARTICIPANT_SIZE 34u

#define STN_CONTRACT_MAX_PARTICIPANTS 32u
#define STN_CONTRACT_MAX_TERMS 65536u

#define STN_CONTRACT_MAX_SIZE \
    (STN_CONTRACT_HEADER_SIZE + \
     (STN_CONTRACT_MAX_PARTICIPANTS * STN_CONTRACT_PARTICIPANT_SIZE) + \
     STN_CONTRACT_MAX_TERMS)

/*
 * Contract types are protocol-defined.
 *
 * Version 1 establishes the canonical contract representation without
 * assigning application-specific meaning to arbitrary executable content.
 */
typedef enum stn_contract_type {
    STN_CONTRACT_GENERIC = 1,
    STN_CONTRACT_WORK_OFFER = 2,
    STN_CONTRACT_CONTRIBUTOR_AGREEMENT = 3,
    STN_CONTRACT_POLICY = 4,
    STN_CONTRACT_ORGANIZATIONAL_DECISION = 5,
    STN_CONTRACT_SERVICE_AGREEMENT = 6
} stn_contract_type;

/*
 * Contract states are protocol state, not executable instructions.
 *
 * A contract type may permit only a subset of these states. State-transition
 * policy belongs to the Contract Engine and is not inferred by the structural
 * codec.
 */
typedef enum stn_contract_state {
    STN_CONTRACT_STATE_DRAFT = 1,
    STN_CONTRACT_STATE_ISSUED = 2,
    STN_CONTRACT_STATE_REVIEW = 3,
    STN_CONTRACT_STATE_APPROVALS = 4,
    STN_CONTRACT_STATE_ATTESTATION = 5,
    STN_CONTRACT_STATE_EXECUTED = 6,
    STN_CONTRACT_STATE_REJECTED = 7,
    STN_CONTRACT_STATE_REVOKED = 8,
    STN_CONTRACT_STATE_CLOSED = 9
} stn_contract_state;

/*
 * Participant roles are protocol identifiers.
 *
 * Roles identify a participant's relationship to the agreement. They do not
 * themselves establish ownership, signature validity, authority or consensus
 * acceptance.
 */
typedef enum stn_contract_role {
    STN_CONTRACT_ROLE_PARTICIPANT = 1,
    STN_CONTRACT_ROLE_ISSUER = 2,
    STN_CONTRACT_ROLE_RECIPIENT = 3,
    STN_CONTRACT_ROLE_APPROVER = 4,
    STN_CONTRACT_ROLE_ATTESTOR = 5
} stn_contract_role;

/*
 * Contract actions are protocol-defined requests to advance or terminate a
 * contract lifecycle. They are not executable instructions.
 *
 * Action validity is evaluated against the current contract state and sequence.
 * Identity, signature and scoped-authority validation remain separate protocol
 * concerns and are not inferred from an action value.
 */
typedef enum stn_contract_action {
    STN_CONTRACT_ACTION_CREATE = 1,
    STN_CONTRACT_ACTION_AMEND = 2,
    STN_CONTRACT_ACTION_APPROVE = 3,
    STN_CONTRACT_ACTION_REJECT = 4,
    STN_CONTRACT_ACTION_EXECUTE = 5,
    STN_CONTRACT_ACTION_REVOKE = 6,
    STN_CONTRACT_ACTION_CLOSE = 7
} stn_contract_action;

typedef enum stn_contract_status {
    STN_CONTRACT_OK = 0,
    STN_CONTRACT_ARGUMENT,
    STN_CONTRACT_TRUNCATED,
    STN_CONTRACT_MAGIC,
    STN_CONTRACT_VERSION_ERROR,
    STN_CONTRACT_TYPE_ERROR,
    STN_CONTRACT_STATE_ERROR,
    STN_CONTRACT_ROLE_ERROR,
    STN_CONTRACT_PARTICIPANT_LIMIT,
    STN_CONTRACT_PARTICIPANT_INDEX,
    STN_CONTRACT_TERMS_LIMIT,
    STN_CONTRACT_LENGTH,
    STN_CONTRACT_CAPACITY,
    STN_CONTRACT_ADDRESS_ERROR,
    STN_CONTRACT_ACTION_ERROR,
    STN_CONTRACT_TRANSITION_ERROR,
    STN_CONTRACT_SEQUENCE_ERROR,
    STN_CONTRACT_AUTHORITY_ERROR,
    STN_CONTRACT_SIGNATURE_ERROR,
    STN_CONTRACT_DUPLICATE_APPROVAL
} stn_contract_status;

/*
 * Native participant representation.
 *
 * Identity is the 32-byte identifier underlying a canonical stn0_ address.
 * Textual address representation is not stored in canonical contract bytes.
 *
 * This structure is used for construction and participant extraction. Native
 * structure layout is never used as canonical wire representation.
 */
typedef struct stn_contract_participant {
    uint8_t identity[STN_ADDRESS_ID_SIZE];
    uint16_t role;
} stn_contract_participant;

/*
 * Canonical Contract v1
 *
 * Wire representation:
 *
 *   4 bytes   magic "STCT"
 *   2 bytes   version
 *   2 bytes   type
 *   8 bytes   sequence
 *   8 bytes   created_at
 *   2 bytes   state
 *   2 bytes   participant_count
 *   4 bytes   terms_length
 *   N * 34    participants
 *   variable  terms
 *
 * Participant wire representation:
 *
 *   32 bytes  identity identifier
 *   2 bytes   role
 *
 * All integer fields use big-endian representation.
 *
 * The contract address is not encoded into the contract. It is derived from
 * the exact canonical contract bytes using STN_ADDRESS_CONTRACT.
 *
 * Signatures and authority evidence are intentionally not part of this
 * structural object. Address identifies the contract; signatures authenticate;
 * authority permits actions; consensus accepts state.
 */
typedef struct stn_contract {
    uint16_t version;
    uint16_t type;
    uint64_t sequence;
    uint64_t created_at;
    uint16_t state;

    /*
     * Encode input only.
     *
     * When constructing a contract for encoding, participants points to
     * participant_count native participant objects.
     *
     * Decode does not point this member into packed canonical bytes because
     * native structure layout, padding and alignment are platform-dependent.
     * It is therefore NULL in a successfully decoded view. Participants from
     * decoded canonical bytes are obtained with stn_contract_participant_at().
     */
    const stn_contract_participant *participants;
    uint16_t participant_count;

    /*
     * Canonical participant wire span.
     *
     * Decode borrows this span directly from the canonical input. Its length
     * is participant_count * STN_CONTRACT_PARTICIPANT_SIZE.
     *
     * Encode callers do not set this member.
     */
    const uint8_t *participant_bytes;

    /*
     * Decode borrows terms directly from canonical input. Encode callers
     * provide the exact terms bytes here.
     */
    const uint8_t *terms;
    uint32_t terms_length;
} stn_contract;

/*
 * Structural codec only.
 *
 * Success establishes only that the canonical contract representation is
 * structurally valid. It does not establish participant ownership, signature
 * validity, signer authority, permitted state transition, legal validity,
 * economic entitlement or consensus acceptance.
 *
 * No allocation, I/O or global mutable state.
 *
 * Decode borrows participant_bytes and terms from the supplied canonical
 * input. The input must remain alive and unchanged for the lifetime of the
 * decoded view. Native participant objects are not overlaid onto canonical
 * bytes. Output is unchanged on failure.
 */
stn_contract_status stn_contract_decode(
    const uint8_t *input,
    size_t input_length,
    stn_contract *contract);

/*
 * Encode writes the canonical Contract v1 representation.
 *
 * participants must designate participant_count readable native participant
 * objects when participant_count is nonzero. NULL participants are permitted
 * only when participant_count is zero.
 *
 * participant_bytes is ignored during encoding.
 *
 * NULL terms are permitted only when terms_length is zero.
 *
 * On failure output is unchanged and *written is zero when written is
 * non-NULL.
 */
stn_contract_status stn_contract_encode(
    const stn_contract *contract,
    uint8_t *output,
    size_t capacity,
    size_t *written);

/*
 * Extract one participant from a successfully decoded canonical contract.
 *
 * contract must contain a canonical participant_bytes span produced by
 * stn_contract_decode(). index is zero-based and must be less than
 * participant_count.
 *
 * The participant is copied into native representation. Output is unchanged
 * on failure.
 */
stn_contract_status stn_contract_participant_at(
    const stn_contract *contract,
    uint16_t index,
    stn_contract_participant *participant);

/*
 * Validate canonical structural representation without retaining a decoded
 * view.
 */
stn_contract_status stn_contract_validate_structure(
    const uint8_t *input,
    size_t input_length);

/*
 * Derive the canonical stnc0_ contract address from exact canonical contract
 * bytes.
 *
 * The supplied bytes must first pass Contract v1 structural validation.
 * stn_address_derive() supplies the SHA-256 identifier using
 * STN_ADDRESS_CONTRACT. No textual prefix or implicit domain is included in
 * the hash input.
 *
 * The resulting address identifies this exact canonical agreement. It does
 * not establish ownership, authority, signature validity or acceptance.
 */
stn_contract_status stn_contract_address(
    const uint8_t *input,
    size_t input_length,
    stn_address *address);

/*
 * Determine the deterministic next state for a Contract v1 action.
 *
 * current_state and action must be supported protocol values. The function
 * evaluates lifecycle policy only; it does not validate identity, signatures,
 * authority, accepted history or consensus acceptance.
 *
 * On success next_state receives the protocol-defined resulting state.
 * Output is unchanged on failure.
 */
stn_contract_status stn_contract_transition(
    uint16_t current_state,
    uint16_t action,
    uint16_t *next_state);

/*
 * Validate and apply a Contract v1 lifecycle action to a native contract
 * object.
 *
 * expected_sequence is the sequence value carried by the action. It must be
 * exactly current->sequence + 1. Sequence overflow is rejected.
 *
 * The resulting contract preserves version, type, created_at, participants and
 * terms, advances sequence exactly once, and applies only the deterministic
 * state change defined by stn_contract_transition().
 *
 * This function does not authenticate the actor or evaluate signatures or
 * scoped authority. Those checks belong to the existing identity/signature/
 * authority layers before an action can become accepted Chain state.
 *
 * Output is unchanged on failure.
 */

/*
 * Build the Phase 14 scoped-authority action token for a Contract v1 action.
 *
 * The token is deterministic and contains no signer, signature, grant or
 * transport state. Contract actions remain the protocol values defined by
 * stn_contract_action.
 *
 * Output is unchanged on failure.
 */
stn_contract_status stn_contract_authority_action(
    uint16_t action,
    uint8_t authority_action[STN_AUTHORITY_ACTION_SIZE]);

/*
 * Build the Phase 14 scoped-authority context token for an exact canonical
 * Contract v1 object.
 *
 * The canonical contract bytes are structurally validated and their existing
 * stnc0_ identifier is used as the authority context. This scopes authority to
 * the exact agreement identified by the Chain rather than to textual display
 * data or transport metadata.
 *
 * Output is unchanged on failure.
 */
stn_contract_status stn_contract_authority_context(
    const uint8_t *canonical_contract,
    size_t canonical_contract_length,
    uint8_t authority_context[STN_AUTHORITY_CONTEXT_SIZE]);

/*
 * Evaluate Phase 14 scoped-authority evidence for a Contract v1 action.
 *
 * This function performs only exact Phase 14 subject/action/context evidence
 * evaluation. It does not validate an authority grant, revocation state,
 * signature, accepted history or consensus acceptance. Those remain owned by
 * the existing Phase 14 authority/lifecycle machinery.
 */
stn_contract_status stn_contract_authority_evaluate(
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],
    uint16_t action,
    const uint8_t *canonical_contract,
    size_t canonical_contract_length,
    const uint8_t *evidence,
    size_t evidence_length);



/*
 * Contract action signature verification.
 *
 * A signature authenticates the actor for one exact Contract v1 action. It
 * does not establish scoped authority, lifecycle validity, accepted history or
 * consensus acceptance. Those remain separate protocol checks.
 *
 * The signed statement is deterministic and binds the actor to the supplied
 * action, exact canonical contract and action sequence. Verification reuses the
 * existing STN identity/signature layer; Contract does not define a second
 * cryptographic provider.
 */
stn_contract_status stn_contract_signature_verify(
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],
    uint16_t action,
    const uint8_t *canonical_contract,
    size_t canonical_contract_length,
    uint64_t sequence,
    const uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE]);


/*
 * Accepted Contract approval state.
 *
 * An approval key is the exact tuple:
 *
 *   canonical contract identifier + contract sequence + approving identity
 *
 * This state records only approvals that have become accepted Chain state.
 * Pending arrival order, wall-clock time and transport metadata do not
 * participate. Buffers are caller-owned; no protocol-level approval ceiling is
 * imposed by this primitive.
 */
#define STN_CONTRACT_APPROVAL_KEY_SIZE \
    (STN_ADDRESS_ID_SIZE + 8u + STN_IDENTITY_PUBLIC_KEY_SIZE)

typedef struct stn_contract_approval_state {
    uint8_t *accepted;
    size_t accepted_count;
    size_t accepted_capacity;
} stn_contract_approval_state;

/*
 * Build the canonical duplicate-approval key for an APPROVE action.
 *
 * canonical_contract must be an exact structurally valid Contract v1 object.
 * sequence is the sequence carried by the approval action. actor is
 * canonicalized through the existing identity layer.
 *
 * Output is unchanged on failure.
 */
stn_contract_status stn_contract_approval_key(
    const uint8_t *canonical_contract,
    size_t canonical_contract_length,
    uint64_t sequence,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],
    uint8_t key[STN_CONTRACT_APPROVAL_KEY_SIZE]);

/*
 * Initialize caller-owned accepted approval state.
 *
 * accepted_capacity is measured in approval keys, not bytes.
 */
void stn_contract_approval_state_initialize(
    stn_contract_approval_state *state,
    uint8_t *accepted,
    size_t accepted_capacity);

/*
 * Check whether an exact canonical approval key is already accepted.
 *
 * STN_CONTRACT_OK means fresh. STN_CONTRACT_DUPLICATE_APPROVAL means the exact
 * contract/sequence/identity tuple is already present.
 */
stn_contract_status stn_contract_approval_state_check(
    const stn_contract_approval_state *state,
    const uint8_t key[STN_CONTRACT_APPROVAL_KEY_SIZE]);

/*
 * Consume an approval only after the corresponding APPROVE action becomes
 * accepted Chain state.
 *
 * Duplicate keys are rejected. Capacity exhaustion fails explicitly.
 */
stn_contract_status stn_contract_approval_state_consume(
    stn_contract_approval_state *state,
    const uint8_t key[STN_CONTRACT_APPROVAL_KEY_SIZE]);

/*
 * Deterministically rebuild accepted approval state from canonical approval
 * keys enumerated in accepted history order.
 *
 * Output state becomes usable only on success.
 */
stn_contract_status stn_contract_approval_state_rebuild(
    stn_contract_approval_state *state,
    const uint8_t *keys,
    size_t key_count);


/*
 * Validate one Contract v1 action against the established protocol layers.
 *
 * Validation is deliberately compositional:
 *   - canonical_contract establishes the exact addressed agreement;
 *   - actor/signature authenticate the action tuple;
 *   - authority evidence permits that actor/action/contract scope;
 *   - expected_sequence and current state are checked by the Contract engine;
 *   - APPROVE additionally checks accepted duplicate-approval state.
 *
 * This function validates only. It does not consume approval state, mutate the
 * current contract, publish accepted history, or decide consensus acceptance.
 */
stn_contract_status stn_contract_validate_action(
    const stn_contract *current,
    uint16_t action,
    uint64_t expected_sequence,
    const uint8_t *canonical_contract,
    size_t canonical_contract_length,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE],
    const uint8_t *authority_evidence,
    size_t authority_evidence_length,
    const stn_contract_approval_state *approval_state);

/*
 * Apply one Contract v1 action after consensus has accepted that exact action.
 *
 * The caller establishes consensus acceptance before calling this function.
 * This primitive does not decide consensus and does not re-run signature or
 * authority validation. It deterministically publishes the already-accepted
 * Contract state transition and, for APPROVE, consumes the corresponding
 * accepted duplicate-approval key.
 *
 * canonical_contract must be the exact canonical representation of current.
 * expected_sequence must be exactly current->sequence + 1.
 *
 * APPROVE requires approval_state. Non-APPROVE actions do not consume approval
 * state and permit approval_state to be NULL.
 *
 * Application is atomic with respect to caller-visible outputs: next is
 * unchanged on failure, and approval_state is not modified unless the complete
 * accepted action can be applied successfully.
 */
stn_contract_status stn_contract_accept_action(
    const stn_contract *current,
    uint16_t action,
    uint64_t expected_sequence,
    const uint8_t *canonical_contract,
    size_t canonical_contract_length,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],
    stn_contract_approval_state *approval_state,
    stn_contract *next);

/*
 * One already-accepted Contract action used for deterministic state rebuild.
 *
 * The history entry contains only consensus-visible Contract inputs required to
 * reproduce accepted Contract state. It does not carry arrival time, transport
 * metadata, pending state, signatures or authority evidence: those were
 * validated before the action entered accepted Chain history.
 *
 * canonical_contract is the exact canonical representation of the Contract
 * state immediately before this accepted action.
 */
typedef struct stn_contract_accepted_action {
    uint16_t action;
    uint64_t sequence;
    const uint8_t *canonical_contract;
    size_t canonical_contract_length;
    uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE];
} stn_contract_accepted_action;

/*
 * Deterministically reconstruct Contract state from accepted history.
 *
 * initial is the accepted Contract state immediately before history[0].
 * history must be enumerated in accepted Chain order. Each entry is applied
 * through stn_contract_accept_action(), so lifecycle, exact sequence,
 * canonical-current binding and duplicate APPROVE consumption remain identical
 * to normal accepted-state application.
 *
 * approval_state is rebuilt from the supplied history. Its caller-owned buffer
 * and capacity are preserved, but accepted_count becomes the reconstructed
 * count only on complete success.
 *
 * Output is atomic: current and approval_state are unchanged on failure.
 *
 * No signature, authority, pending, transport, persistence or consensus
 * decision is performed here. Accepted history is evidence supplied by the
 * existing Chain reconstruction/reorganization machinery.
 */
stn_contract_status stn_contract_rebuild_accepted_state(
    const stn_contract *initial,
    const stn_contract_accepted_action *history,
    size_t history_count,
    stn_contract_approval_state *approval_state,
    stn_contract *current);

stn_contract_status stn_contract_apply_action(
    const stn_contract *current,
    uint16_t action,
    uint64_t expected_sequence,
    stn_contract *next);

#endif