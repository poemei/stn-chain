/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_H
#define STN_CONTRACT_H

#include "stn_address.h"
#include "stn_platform.h"

#include <stddef.h>

#define STN_CONTRACT_VERSION 1u

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
    STN_CONTRACT_SEQUENCE_ERROR
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
stn_contract_status stn_contract_apply_action(
    const stn_contract *current,
    uint16_t action,
    uint64_t expected_sequence,
    stn_contract *next);

#endif