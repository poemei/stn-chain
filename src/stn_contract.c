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