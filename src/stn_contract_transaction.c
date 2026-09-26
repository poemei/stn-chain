/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_transaction.h"
#include "stn_wire_internal.h"

#include <string.h>

static int action_valid(uint16_t action)
{
    return action >= STN_CONTRACT_ACTION_CREATE &&
           action <= STN_CONTRACT_ACTION_CLOSE;
}

static int authority_length_valid(uint16_t action,uint32_t length)
{
    if(action==STN_CONTRACT_ACTION_CREATE)return length==0u;
    return length==STN_AUTHORITY_EVIDENCE_SIZE;
}

stn_contract_status stn_contract_transaction_decode(
    const uint8_t *input,
    size_t input_length,
    stn_contract_transaction *transaction)
{
    stn_contract_transaction decoded;
    uint32_t contract_length;
    uint32_t authority_length;
    size_t expected_length;

    if (input == NULL || transaction == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (input_length < STN_CONTRACT_TX_HEADER_SIZE ||
        input_length > STN_CONTRACT_TX_MAX_SIZE) {
        return STN_CONTRACT_LENGTH;
    }

    memset(&decoded, 0, sizeof(decoded));

    decoded.version = (uint16_t)stn_wire_read(input, 2);
    decoded.action = (uint16_t)stn_wire_read(input + 2, 2);
    decoded.sequence = stn_wire_read(input + 4, 8);
    contract_length = (uint32_t)stn_wire_read(input + 12, 4);
    authority_length = (uint32_t)stn_wire_read(input + 16, 4);

    if (decoded.version != STN_CONTRACT_TX_VERSION) {
        return STN_CONTRACT_VERSION_ERROR;
    }
    if (!action_valid(decoded.action)) {
        return STN_CONTRACT_ACTION_ERROR;
    }
    if (contract_length < STN_CONTRACT_HEADER_SIZE ||
        contract_length > STN_CONTRACT_MAX_SIZE) {
        return STN_CONTRACT_LENGTH;
    }
    if (!authority_length_valid(decoded.action,authority_length)) {
        return STN_CONTRACT_AUTHORITY_ERROR;
    }

    expected_length = STN_CONTRACT_TX_HEADER_SIZE +
        (size_t)contract_length + (size_t)authority_length;
    if (expected_length != input_length) {
        return STN_CONTRACT_LENGTH;
    }

    memcpy(decoded.actor, input + 20, STN_IDENTITY_PUBLIC_KEY_SIZE);
    memcpy(decoded.signature, input + 52, STN_IDENTITY_SIGNATURE_SIZE);

    decoded.canonical_contract = input + STN_CONTRACT_TX_HEADER_SIZE;
    decoded.canonical_contract_length = contract_length;
    decoded.authority_evidence = authority_length == 0u ? NULL :
        decoded.canonical_contract + contract_length;
    decoded.authority_evidence_length = authority_length;

    if (stn_contract_validate_structure(
            decoded.canonical_contract,
            decoded.canonical_contract_length) != STN_CONTRACT_OK) {
        return STN_CONTRACT_LENGTH;
    }

    if (decoded.action != STN_CONTRACT_ACTION_CREATE &&
        stn_authority_evidence_validate(
            decoded.authority_evidence,
            decoded.authority_evidence_length) != STN_AUTHORITY_AUTHORIZED) {
        return STN_CONTRACT_AUTHORITY_ERROR;
    }

    *transaction = decoded;
    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_transaction_encode(
    const stn_contract_transaction *transaction,
    uint8_t *output,
    size_t capacity,
    size_t *written)
{
    uint8_t temporary[STN_CONTRACT_TX_MAX_SIZE];
    size_t total;

    if (written != NULL) {
        *written = 0u;
    }

    if (transaction == NULL || output == NULL || written == NULL ||
        transaction->canonical_contract == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (transaction->version != STN_CONTRACT_TX_VERSION) {
        return STN_CONTRACT_VERSION_ERROR;
    }
    if (!action_valid(transaction->action)) {
        return STN_CONTRACT_ACTION_ERROR;
    }
    if (transaction->canonical_contract_length < STN_CONTRACT_HEADER_SIZE ||
        transaction->canonical_contract_length > STN_CONTRACT_MAX_SIZE) {
        return STN_CONTRACT_LENGTH;
    }
    if (!authority_length_valid(transaction->action,
                                transaction->authority_evidence_length)) {
        return STN_CONTRACT_AUTHORITY_ERROR;
    }
    if (transaction->authority_evidence_length != 0u &&
        transaction->authority_evidence == NULL) {
        return STN_CONTRACT_ARGUMENT;
    }

    if (stn_contract_validate_structure(
            transaction->canonical_contract,
            transaction->canonical_contract_length) != STN_CONTRACT_OK) {
        return STN_CONTRACT_LENGTH;
    }

    if (transaction->action != STN_CONTRACT_ACTION_CREATE &&
        stn_authority_evidence_validate(
            transaction->authority_evidence,
            transaction->authority_evidence_length) !=
        STN_AUTHORITY_AUTHORIZED) {
        return STN_CONTRACT_AUTHORITY_ERROR;
    }

    total = STN_CONTRACT_TX_HEADER_SIZE +
        (size_t)transaction->canonical_contract_length +
        (size_t)transaction->authority_evidence_length;

    if (capacity < total) {
        return STN_CONTRACT_CAPACITY;
    }

    stn_wire_write(temporary, 2, transaction->version);
    stn_wire_write(temporary + 2, 2, transaction->action);
    stn_wire_write(temporary + 4, 8, transaction->sequence);
    stn_wire_write(
        temporary + 12, 4, transaction->canonical_contract_length);
    stn_wire_write(
        temporary + 16, 4, transaction->authority_evidence_length);
    memcpy(
        temporary + 20,
        transaction->actor,
        STN_IDENTITY_PUBLIC_KEY_SIZE);
    memcpy(
        temporary + 52,
        transaction->signature,
        STN_IDENTITY_SIGNATURE_SIZE);
    memcpy(
        temporary + STN_CONTRACT_TX_HEADER_SIZE,
        transaction->canonical_contract,
        transaction->canonical_contract_length);
    if(transaction->authority_evidence_length!=0u){
        memcpy(
            temporary + STN_CONTRACT_TX_HEADER_SIZE +
                transaction->canonical_contract_length,
            transaction->authority_evidence,
            transaction->authority_evidence_length);
    }

    memcpy(output, temporary, total);
    *written = total;
    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_transaction_validate_structure(
    const uint8_t *input,
    size_t input_length)
{
    stn_contract_transaction transaction;

    return stn_contract_transaction_decode(
        input,
        input_length,
        &transaction);
}
