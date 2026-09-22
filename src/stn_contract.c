/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract.h"

#include <string.h>

static const uint8_t contract_magic[4] = {
    0x53, 0x54, 0x43, 0x54
};

static uint64_t read_be(const uint8_t *bytes, size_t count)
{
    uint64_t value = 0;
    size_t i;

    for (i = 0; i < count; ++i) {
        value = (value << 8) | bytes[i];
    }

    return value;
}

static void write_be(uint8_t *bytes, size_t count, uint64_t value)
{
    while (count != 0) {
        bytes[--count] = (uint8_t)(value & UINT64_C(255));
        value >>= 8;
    }
}

static int contract_type_valid(uint16_t type)
{
    switch (type) {
    case STN_CONTRACT_GENERIC:
    case STN_CONTRACT_WORK_OFFER:
    case STN_CONTRACT_CONTRIBUTOR_AGREEMENT:
    case STN_CONTRACT_POLICY:
    case STN_CONTRACT_ORGANIZATIONAL_DECISION:
    case STN_CONTRACT_SERVICE_AGREEMENT:
        return 1;
    default:
        return 0;
    }
}

static int contract_state_valid(uint16_t state)
{
    switch (state) {
    case STN_CONTRACT_STATE_DRAFT:
    case STN_CONTRACT_STATE_ISSUED:
    case STN_CONTRACT_STATE_REVIEW:
    case STN_CONTRACT_STATE_APPROVALS:
    case STN_CONTRACT_STATE_ATTESTATION:
    case STN_CONTRACT_STATE_EXECUTED:
    case STN_CONTRACT_STATE_REJECTED:
    case STN_CONTRACT_STATE_REVOKED:
    case STN_CONTRACT_STATE_CLOSED:
        return 1;
    default:
        return 0;
    }
}

static int contract_role_valid(uint16_t role)
{
    switch (role) {
    case STN_CONTRACT_ROLE_PARTICIPANT:
    case STN_CONTRACT_ROLE_ISSUER:
    case STN_CONTRACT_ROLE_RECIPIENT:
    case STN_CONTRACT_ROLE_APPROVER:
    case STN_CONTRACT_ROLE_ATTESTOR:
        return 1;
    default:
        return 0;
    }
}

static stn_contract_status contract_size(
    uint16_t participant_count,
    uint32_t terms_length,
    size_t *total)
{
    size_t participant_bytes;
    size_t calculated;

    if (total == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (participant_count > STN_CONTRACT_MAX_PARTICIPANTS) {
        return STN_CONTRACT_PARTICIPANT_LIMIT;
    }

    if (terms_length > STN_CONTRACT_MAX_TERMS) {
        return STN_CONTRACT_TERMS_LIMIT;
    }

    participant_bytes =
        (size_t)participant_count * STN_CONTRACT_PARTICIPANT_SIZE;

    calculated =
        STN_CONTRACT_HEADER_SIZE +
        participant_bytes +
        (size_t)terms_length;

    if (calculated > STN_CONTRACT_MAX_SIZE) {
        return STN_CONTRACT_LENGTH;
    }

    *total = calculated;
    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_decode(
    const uint8_t *input,
    size_t input_length,
    stn_contract *contract)
{
    stn_contract decoded = {0};
    stn_contract_status status;
    uint16_t participant_count;
    uint32_t terms_length;
    size_t expected_length;
    size_t participant_bytes_length;
    size_t offset;
    size_t i;

    if (input == NULL || contract == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (input_length < STN_CONTRACT_HEADER_SIZE) {
        return STN_CONTRACT_TRUNCATED;
    }

    if (input_length > STN_CONTRACT_MAX_SIZE) {
        return STN_CONTRACT_LENGTH;
    }

    if (memcmp(input, contract_magic, sizeof(contract_magic)) != 0) {
        return STN_CONTRACT_MAGIC;
    }

    decoded.version = (uint16_t)read_be(input + 4, 2);
    decoded.type = (uint16_t)read_be(input + 6, 2);
    decoded.sequence = read_be(input + 8, 8);
    decoded.created_at = read_be(input + 16, 8);
    decoded.state = (uint16_t)read_be(input + 24, 2);

    participant_count = (uint16_t)read_be(input + 26, 2);
    terms_length = (uint32_t)read_be(input + 28, 4);

    if (decoded.version != STN_CONTRACT_VERSION) {
        return STN_CONTRACT_VERSION_ERROR;
    }

    if (!contract_type_valid(decoded.type)) {
        return STN_CONTRACT_TYPE_ERROR;
    }

    if (!contract_state_valid(decoded.state)) {
        return STN_CONTRACT_STATE_ERROR;
    }

    status = contract_size(
        participant_count,
        terms_length,
        &expected_length);

    if (status != STN_CONTRACT_OK) {
        return status;
    }

    if (input_length != expected_length) {
        return STN_CONTRACT_LENGTH;
    }

    participant_bytes_length =
        (size_t)participant_count * STN_CONTRACT_PARTICIPANT_SIZE;

    offset = STN_CONTRACT_HEADER_SIZE;

    /*
     * Validate every packed participant without overlaying native structures
     * onto canonical bytes.
     */
    for (i = 0; i < participant_count; ++i) {
        uint16_t role;

        role = (uint16_t)read_be(
            input + offset + STN_ADDRESS_ID_SIZE,
            2);

        if (!contract_role_valid(role)) {
            return STN_CONTRACT_ROLE_ERROR;
        }

        offset += STN_CONTRACT_PARTICIPANT_SIZE;
    }

    decoded.participants = NULL;
    decoded.participant_count = participant_count;

    decoded.participant_bytes =
        participant_count == 0
        ? NULL
        : input + STN_CONTRACT_HEADER_SIZE;

    decoded.terms_length = terms_length;
    decoded.terms =
        terms_length == 0
        ? NULL
        : input + STN_CONTRACT_HEADER_SIZE + participant_bytes_length;

    *contract = decoded;

    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_encode(
    const stn_contract *contract,
    uint8_t *output,
    size_t capacity,
    size_t *written)
{
    stn_contract_status status;
    size_t total;
    size_t offset;
    size_t i;

    if (written != NULL) {
        *written = 0;
    }

    if (contract == NULL ||
        output == NULL ||
        written == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (contract->version != STN_CONTRACT_VERSION) {
        return STN_CONTRACT_VERSION_ERROR;
    }

    if (!contract_type_valid(contract->type)) {
        return STN_CONTRACT_TYPE_ERROR;
    }

    if (!contract_state_valid(contract->state)) {
        return STN_CONTRACT_STATE_ERROR;
    }

    if (contract->participant_count != 0 &&
        contract->participants == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (contract->terms_length != 0 &&
        contract->terms == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    status = contract_size(
        contract->participant_count,
        contract->terms_length,
        &total);

    if (status != STN_CONTRACT_OK) {
        return status;
    }

    for (i = 0; i < contract->participant_count; ++i) {
        if (!contract_role_valid(contract->participants[i].role)) {
            return STN_CONTRACT_ROLE_ERROR;
        }
    }

    if (capacity < total) {
        return STN_CONTRACT_CAPACITY;
    }

    memcpy(output, contract_magic, sizeof(contract_magic));

    write_be(output + 4, 2, contract->version);
    write_be(output + 6, 2, contract->type);
    write_be(output + 8, 8, contract->sequence);
    write_be(output + 16, 8, contract->created_at);
    write_be(output + 24, 2, contract->state);
    write_be(output + 26, 2, contract->participant_count);
    write_be(output + 28, 4, contract->terms_length);

    offset = STN_CONTRACT_HEADER_SIZE;

    for (i = 0; i < contract->participant_count; ++i) {
        memcpy(
            output + offset,
            contract->participants[i].identity,
            STN_ADDRESS_ID_SIZE);

        write_be(
            output + offset + STN_ADDRESS_ID_SIZE,
            2,
            contract->participants[i].role);

        offset += STN_CONTRACT_PARTICIPANT_SIZE;
    }

    if (contract->terms_length != 0) {
        memcpy(
            output + offset,
            contract->terms,
            contract->terms_length);
    }

    *written = total;

    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_participant_at(
    const stn_contract *contract,
    uint16_t index,
    stn_contract_participant *participant)
{
    stn_contract_participant extracted;
    const uint8_t *entry;
    uint16_t role;

    if (contract == NULL || participant == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (index >= contract->participant_count) {
        return STN_CONTRACT_PARTICIPANT_INDEX;
    }

    if (contract->participant_bytes == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    entry =
        contract->participant_bytes +
        ((size_t)index * STN_CONTRACT_PARTICIPANT_SIZE);

    memcpy(
        extracted.identity,
        entry,
        STN_ADDRESS_ID_SIZE);

    role = (uint16_t)read_be(
        entry + STN_ADDRESS_ID_SIZE,
        2);

    if (!contract_role_valid(role)) {
        return STN_CONTRACT_ROLE_ERROR;
    }

    extracted.role = role;

    *participant = extracted;

    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_validate_structure(
    const uint8_t *input,
    size_t input_length)
{
    stn_contract contract;

    return stn_contract_decode(
        input,
        input_length,
        &contract);
}

stn_contract_status stn_contract_address(
    const uint8_t *input,
    size_t input_length,
    stn_address *address)
{
    stn_contract_status status;
    stn_address derived;
    stn_data_status address_status;

    if (input == NULL || address == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    status = stn_contract_validate_structure(
        input,
        input_length);

    if (status != STN_CONTRACT_OK) {
        return status;
    }

    address_status = stn_address_derive(
        STN_ADDRESS_CONTRACT,
        input,
        input_length,
        &derived);

    if (address_status != STN_DATA_OK) {
        return STN_CONTRACT_ADDRESS_ERROR;
    }

    *address = derived;

    return STN_CONTRACT_OK;
}

static int contract_action_valid(uint16_t action)
{
    switch (action) {
    case STN_CONTRACT_ACTION_CREATE:
    case STN_CONTRACT_ACTION_AMEND:
    case STN_CONTRACT_ACTION_APPROVE:
    case STN_CONTRACT_ACTION_REJECT:
    case STN_CONTRACT_ACTION_EXECUTE:
    case STN_CONTRACT_ACTION_REVOKE:
    case STN_CONTRACT_ACTION_CLOSE:
        return 1;
    default:
        return 0;
    }
}

stn_contract_status stn_contract_transition(
    uint16_t current_state,
    uint16_t action,
    uint16_t *next_state)
{
    uint16_t result;

    if (next_state == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (!contract_state_valid(current_state)) {
        return STN_CONTRACT_STATE_ERROR;
    }

    if (!contract_action_valid(action)) {
        return STN_CONTRACT_ACTION_ERROR;
    }

    switch (action) {
    case STN_CONTRACT_ACTION_CREATE:
        if (current_state != STN_CONTRACT_STATE_DRAFT) {
            return STN_CONTRACT_TRANSITION_ERROR;
        }
        result = STN_CONTRACT_STATE_ISSUED;
        break;

    case STN_CONTRACT_ACTION_AMEND:
        if (current_state != STN_CONTRACT_STATE_ISSUED &&
            current_state != STN_CONTRACT_STATE_REVIEW) {
            return STN_CONTRACT_TRANSITION_ERROR;
        }
        result = STN_CONTRACT_STATE_REVIEW;
        break;

    case STN_CONTRACT_ACTION_APPROVE:
        if (current_state == STN_CONTRACT_STATE_REVIEW) {
            result = STN_CONTRACT_STATE_APPROVALS;
        } else if (current_state == STN_CONTRACT_STATE_APPROVALS) {
            result = STN_CONTRACT_STATE_ATTESTATION;
        } else {
            return STN_CONTRACT_TRANSITION_ERROR;
        }
        break;

    case STN_CONTRACT_ACTION_REJECT:
        if (current_state != STN_CONTRACT_STATE_ISSUED &&
            current_state != STN_CONTRACT_STATE_REVIEW &&
            current_state != STN_CONTRACT_STATE_APPROVALS &&
            current_state != STN_CONTRACT_STATE_ATTESTATION) {
            return STN_CONTRACT_TRANSITION_ERROR;
        }
        result = STN_CONTRACT_STATE_REJECTED;
        break;

    case STN_CONTRACT_ACTION_EXECUTE:
        if (current_state != STN_CONTRACT_STATE_ATTESTATION) {
            return STN_CONTRACT_TRANSITION_ERROR;
        }
        result = STN_CONTRACT_STATE_EXECUTED;
        break;

    case STN_CONTRACT_ACTION_REVOKE:
        if (current_state != STN_CONTRACT_STATE_ISSUED &&
            current_state != STN_CONTRACT_STATE_REVIEW &&
            current_state != STN_CONTRACT_STATE_APPROVALS &&
            current_state != STN_CONTRACT_STATE_ATTESTATION &&
            current_state != STN_CONTRACT_STATE_EXECUTED) {
            return STN_CONTRACT_TRANSITION_ERROR;
        }
        result = STN_CONTRACT_STATE_REVOKED;
        break;

    case STN_CONTRACT_ACTION_CLOSE:
        if (current_state != STN_CONTRACT_STATE_EXECUTED &&
            current_state != STN_CONTRACT_STATE_REJECTED &&
            current_state != STN_CONTRACT_STATE_REVOKED) {
            return STN_CONTRACT_TRANSITION_ERROR;
        }
        result = STN_CONTRACT_STATE_CLOSED;
        break;

    default:
        return STN_CONTRACT_ACTION_ERROR;
    }

    *next_state = result;
    return STN_CONTRACT_OK;
}


stn_contract_status stn_contract_authority_action(
    uint16_t action,
    uint8_t authority_action[STN_AUTHORITY_ACTION_SIZE])
{
    uint8_t token[STN_AUTHORITY_ACTION_SIZE] = {0};

    if (authority_action == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (!contract_action_valid(action)) {
        return STN_CONTRACT_ACTION_ERROR;
    }

    token[0] = STN_AUTHORITY_VERSION;
    token[1] = STN_CONTRACT_AUTHORITY_ACTION_DOMAIN;
    token[2] = (uint8_t)((action >> 8) & 0xffu);
    token[3] = (uint8_t)(action & 0xffu);

    if (stn_authority_action_validate(token) != STN_AUTHORITY_AUTHORIZED) {
        return STN_CONTRACT_AUTHORITY_ERROR;
    }

    memcpy(
        authority_action,
        token,
        STN_AUTHORITY_ACTION_SIZE);

    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_authority_context(
    const uint8_t *canonical_contract,
    size_t canonical_contract_length,
    uint8_t authority_context[STN_AUTHORITY_CONTEXT_SIZE])
{
    uint8_t token[STN_AUTHORITY_CONTEXT_SIZE] = {0};
    stn_address address;
    stn_contract_status status;

    if (canonical_contract == NULL || authority_context == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    status = stn_contract_address(
        canonical_contract,
        canonical_contract_length,
        &address);

    if (status != STN_CONTRACT_OK) {
        return status;
    }

    /*
     * Phase 14 tokens require byte 0 to carry the authority version. The
     * remaining 31 bytes bind the scope to the canonical Contract v1
     * identifier. No textual stnc0_ representation is involved.
     */
    token[0] = STN_AUTHORITY_VERSION;
    token[1] = STN_CONTRACT_AUTHORITY_CONTEXT_DOMAIN;
    memcpy(
        token + 2,
        address.identifier,
        STN_AUTHORITY_CONTEXT_SIZE - 2u);

    if (stn_authority_context_validate(token) != STN_AUTHORITY_AUTHORIZED) {
        return STN_CONTRACT_AUTHORITY_ERROR;
    }

    memcpy(
        authority_context,
        token,
        STN_AUTHORITY_CONTEXT_SIZE);

    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_authority_evaluate(
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],
    uint16_t action,
    const uint8_t *canonical_contract,
    size_t canonical_contract_length,
    const uint8_t *evidence,
    size_t evidence_length)
{
    uint8_t authority_action[STN_AUTHORITY_ACTION_SIZE];
    uint8_t authority_context[STN_AUTHORITY_CONTEXT_SIZE];
    stn_contract_status status;
    stn_authority_result authority_status;

    if (actor == NULL || canonical_contract == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    status = stn_contract_authority_action(
        action,
        authority_action);

    if (status != STN_CONTRACT_OK) {
        return status;
    }

    status = stn_contract_authority_context(
        canonical_contract,
        canonical_contract_length,
        authority_context);

    if (status != STN_CONTRACT_OK) {
        return status;
    }

    authority_status = stn_authority_evaluate(
        actor,
        authority_action,
        authority_context,
        evidence,
        evidence_length);

    if (authority_status != STN_AUTHORITY_AUTHORIZED) {
        return STN_CONTRACT_AUTHORITY_ERROR;
    }

    return STN_CONTRACT_OK;
}



stn_contract_status stn_contract_signature_verify(
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],
    uint16_t action,
    const uint8_t *canonical_contract,
    size_t canonical_contract_length,
    uint64_t sequence,
    const uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE])
{
    uint8_t canonical_actor[STN_IDENTITY_PUBLIC_KEY_SIZE];
    uint8_t unsigned_bytes[
        STN_CONTRACT_MAX_SIZE +
        STN_IDENTITY_PUBLIC_KEY_SIZE +
        2u +
        8u];
    uint8_t statement[
        STN_IDENTITY_DOMAIN_SIZE +
        STN_CONTRACT_MAX_SIZE +
        STN_IDENTITY_PUBLIC_KEY_SIZE +
        2u +
        8u];
    size_t unsigned_length;
    size_t statement_length;
    stn_contract_status status;
    stn_identity_result identity_status;

    if (actor == NULL ||
        canonical_contract == NULL ||
        signature == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (!contract_action_valid(action)) {
        return STN_CONTRACT_ACTION_ERROR;
    }

    status = stn_contract_validate_structure(
        canonical_contract,
        canonical_contract_length);

    if (status != STN_CONTRACT_OK) {
        return status;
    }

    identity_status = stn_identity_derive(
        actor,
        canonical_actor);

    if (identity_status != STN_IDENTITY_VALID) {
        return STN_CONTRACT_SIGNATURE_ERROR;
    }

    /*
     * Canonical Contract action signing bytes:
     *
     *   exact canonical Contract v1 bytes
     *   canonical actor identity (32 bytes)
     *   action (2-byte big-endian)
     *   action sequence (8-byte big-endian)
     *
     * stn_identity_statement() applies the existing STN identity signing
     * domain. Signature verification authenticates this exact tuple only.
     * Authority, transition validity, accepted history and consensus remain
     * separate checks.
     */
    memcpy(
        unsigned_bytes,
        canonical_contract,
        canonical_contract_length);

    memcpy(
        unsigned_bytes + canonical_contract_length,
        canonical_actor,
        STN_IDENTITY_PUBLIC_KEY_SIZE);

    write_be(
        unsigned_bytes +
            canonical_contract_length +
            STN_IDENTITY_PUBLIC_KEY_SIZE,
        2u,
        action);

    write_be(
        unsigned_bytes +
            canonical_contract_length +
            STN_IDENTITY_PUBLIC_KEY_SIZE +
            2u,
        8u,
        sequence);

    unsigned_length =
        canonical_contract_length +
        STN_IDENTITY_PUBLIC_KEY_SIZE +
        2u +
        8u;

    statement_length = 0u;

    identity_status = stn_identity_statement(
        unsigned_bytes,
        unsigned_length,
        statement,
        sizeof(statement),
        &statement_length);

    if (identity_status != STN_IDENTITY_VALID) {
        return STN_CONTRACT_SIGNATURE_ERROR;
    }

    identity_status = stn_identity_verify(
        canonical_actor,
        statement,
        statement_length,
        signature,
        STN_IDENTITY_SIGNATURE_SIZE);

    if (identity_status != STN_IDENTITY_VALID) {
        return STN_CONTRACT_SIGNATURE_ERROR;
    }

    return STN_CONTRACT_OK;
}


stn_contract_status stn_contract_approval_key(
    const uint8_t *canonical_contract,
    size_t canonical_contract_length,
    uint64_t sequence,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],
    uint8_t key[STN_CONTRACT_APPROVAL_KEY_SIZE])
{
    stn_address address;
    uint8_t canonical_actor[STN_IDENTITY_PUBLIC_KEY_SIZE];
    uint8_t built[STN_CONTRACT_APPROVAL_KEY_SIZE];
    stn_contract_status status;

    if (canonical_contract == NULL || actor == NULL || key == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    status = stn_contract_address(
        canonical_contract,
        canonical_contract_length,
        &address);

    if (status != STN_CONTRACT_OK) {
        return status;
    }

    if (stn_identity_derive(actor, canonical_actor) != STN_IDENTITY_VALID) {
        return STN_CONTRACT_AUTHORITY_ERROR;
    }

    memcpy(built, address.identifier, STN_ADDRESS_ID_SIZE);
    write_be(built + STN_ADDRESS_ID_SIZE, 8u, sequence);
    memcpy(
        built + STN_ADDRESS_ID_SIZE + 8u,
        canonical_actor,
        STN_IDENTITY_PUBLIC_KEY_SIZE);

    memcpy(key, built, sizeof(built));
    return STN_CONTRACT_OK;
}

void stn_contract_approval_state_initialize(
    stn_contract_approval_state *state,
    uint8_t *accepted,
    size_t accepted_capacity)
{
    if (state == NULL) {
        return;
    }

    state->accepted = accepted;
    state->accepted_count = 0u;
    state->accepted_capacity = accepted_capacity;
}

stn_contract_status stn_contract_approval_state_check(
    const stn_contract_approval_state *state,
    const uint8_t key[STN_CONTRACT_APPROVAL_KEY_SIZE])
{
    size_t i;

    if (state == NULL || key == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (state->accepted_count > state->accepted_capacity ||
        (state->accepted_capacity != 0u && state->accepted == NULL)) {
        return STN_CONTRACT_ARGUMENT;
    }

    for (i = 0u; i < state->accepted_count; ++i) {
        if (memcmp(
                state->accepted + (i * STN_CONTRACT_APPROVAL_KEY_SIZE),
                key,
                STN_CONTRACT_APPROVAL_KEY_SIZE) == 0) {
            return STN_CONTRACT_DUPLICATE_APPROVAL;
        }
    }

    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_approval_state_consume(
    stn_contract_approval_state *state,
    const uint8_t key[STN_CONTRACT_APPROVAL_KEY_SIZE])
{
    stn_contract_status status;

    if (state == NULL || key == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    status = stn_contract_approval_state_check(state, key);
    if (status != STN_CONTRACT_OK) {
        return status;
    }

    if (state->accepted_count == state->accepted_capacity) {
        return STN_CONTRACT_CAPACITY;
    }

    memcpy(
        state->accepted +
            (state->accepted_count * STN_CONTRACT_APPROVAL_KEY_SIZE),
        key,
        STN_CONTRACT_APPROVAL_KEY_SIZE);

    ++state->accepted_count;
    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_approval_state_rebuild(
    stn_contract_approval_state *state,
    const uint8_t *keys,
    size_t key_count)
{
    size_t i;
    size_t original_count;
    stn_contract_status status;

    if (state == NULL ||
        (key_count != 0u && keys == NULL)) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (state->accepted_count > state->accepted_capacity ||
        (state->accepted_capacity != 0u && state->accepted == NULL)) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (key_count > state->accepted_capacity) {
        return STN_CONTRACT_CAPACITY;
    }

    original_count = state->accepted_count;
    state->accepted_count = 0u;

    for (i = 0u; i < key_count; ++i) {
        status = stn_contract_approval_state_consume(
            state,
            keys + (i * STN_CONTRACT_APPROVAL_KEY_SIZE));

        if (status != STN_CONTRACT_OK) {
            state->accepted_count = original_count;
            return status;
        }
    }

    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_apply_action(
    const stn_contract *current,
    uint16_t action,
    uint64_t expected_sequence,
    stn_contract *next)
{
    stn_contract updated;
    stn_contract_status status;
    uint16_t next_state;

    if (current == NULL || next == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (current->version != STN_CONTRACT_VERSION) {
        return STN_CONTRACT_VERSION_ERROR;
    }

    if (!contract_type_valid(current->type)) {
        return STN_CONTRACT_TYPE_ERROR;
    }

    if (!contract_state_valid(current->state)) {
        return STN_CONTRACT_STATE_ERROR;
    }

    if (current->sequence == UINT64_MAX ||
        expected_sequence != current->sequence + UINT64_C(1)) {
        return STN_CONTRACT_SEQUENCE_ERROR;
    }

    status = stn_contract_transition(
        current->state,
        action,
        &next_state);

    if (status != STN_CONTRACT_OK) {
        return status;
    }

    updated = *current;
    updated.sequence = expected_sequence;
    updated.state = next_state;

    *next = updated;
    return STN_CONTRACT_OK;
}

