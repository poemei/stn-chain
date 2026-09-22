/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_transaction.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;

#define CHECK(e) do { \
    ++checks; \
    if (!(e)) { \
        ++failures; \
        fprintf(stderr, "contract transaction line %d: %s\\n", __LINE__, #e); \
    } \
} while (0)

static void fill(uint8_t *bytes, size_t length, uint8_t start)
{
    size_t i;
    for (i = 0; i < length; ++i) {
        bytes[i] = (uint8_t)(start + (uint8_t)i);
    }
}

int test_contract_transaction(void)
{
    static const uint8_t terms[] = {'t','e','s','t'};
    stn_contract_participant participant;
    stn_contract contract;
    stn_contract_transaction transaction;
    stn_contract_transaction decoded;
    stn_contract_transaction before;
    uint8_t canonical[STN_CONTRACT_MAX_SIZE];
    uint8_t authority_action[STN_AUTHORITY_ACTION_SIZE];
    uint8_t authority_context[STN_AUTHORITY_CONTEXT_SIZE];
    uint8_t authority[STN_AUTHORITY_EVIDENCE_SIZE];
    uint8_t encoded[STN_CONTRACT_TX_MAX_SIZE];
    uint8_t encoded_again[STN_CONTRACT_TX_MAX_SIZE];
    uint8_t mutated[STN_CONTRACT_TX_MAX_SIZE];
    uint8_t guard[STN_CONTRACT_TX_MAX_SIZE];
    uint8_t guard_before[STN_CONTRACT_TX_MAX_SIZE];
    size_t canonical_length = 0u;
    size_t authority_length = 0u;
    size_t written = 0u;
    size_t written_again = 0u;
    size_t expected_length;

    checks = 0u;
    failures = 0u;

    memset(&participant, 0, sizeof(participant));
    fill(participant.identity, sizeof(participant.identity), 0x10u);
    participant.role = STN_CONTRACT_ROLE_ISSUER;

    memset(&contract, 0, sizeof(contract));
    contract.version = STN_CONTRACT_VERSION;
    contract.type = STN_CONTRACT_GENERIC;
    contract.sequence = UINT64_C(7);
    contract.created_at = UINT64_C(9);
    contract.state = STN_CONTRACT_STATE_REVIEW;
    contract.participants = &participant;
    contract.participant_count = 1u;
    contract.terms = terms;
    contract.terms_length = (uint32_t)sizeof(terms);

    CHECK(stn_contract_encode(
        &contract,
        canonical,
        sizeof(canonical),
        &canonical_length) == STN_CONTRACT_OK);

    CHECK(stn_contract_authority_action(
        STN_CONTRACT_ACTION_APPROVE,
        authority_action) == STN_CONTRACT_OK);
    CHECK(stn_contract_authority_context(
        canonical,
        canonical_length,
        authority_context) == STN_CONTRACT_OK);

    CHECK(stn_authority_evidence_encode(
        participant.identity,
        authority_action,
        authority_context,
        authority,
        sizeof(authority),
        &authority_length) == STN_AUTHORITY_AUTHORIZED);
    CHECK(authority_length == STN_AUTHORITY_EVIDENCE_SIZE);

    memset(&transaction, 0, sizeof(transaction));
    transaction.version = STN_CONTRACT_TX_VERSION;
    transaction.action = STN_CONTRACT_ACTION_APPROVE;
    transaction.sequence = UINT64_C(8);
    transaction.canonical_contract = canonical;
    transaction.canonical_contract_length = (uint32_t)canonical_length;
    memcpy(
        transaction.actor,
        participant.identity,
        STN_IDENTITY_PUBLIC_KEY_SIZE);
    fill(
        transaction.signature,
        sizeof(transaction.signature),
        0x80u);
    transaction.authority_evidence = authority;
    transaction.authority_evidence_length =
        (uint32_t)authority_length;

    expected_length =
        STN_CONTRACT_TX_HEADER_SIZE +
        canonical_length +
        authority_length;

    CHECK(stn_contract_transaction_encode(
        &transaction,
        encoded,
        sizeof(encoded),
        &written) == STN_CONTRACT_OK);
    CHECK(written == expected_length);

    CHECK(encoded[0] == 0x00u && encoded[1] == 0x01u);
    CHECK(encoded[2] == 0x00u && encoded[3] == 0x03u);
    CHECK(memcmp(
        encoded + 4,
        "\x00\x00\x00\x00\x00\x00\x00\x08",
        8) == 0);
    CHECK(memcmp(
        encoded + 20,
        participant.identity,
        STN_IDENTITY_PUBLIC_KEY_SIZE) == 0);
    CHECK(memcmp(
        encoded + STN_CONTRACT_TX_HEADER_SIZE,
        canonical,
        canonical_length) == 0);
    CHECK(memcmp(
        encoded + STN_CONTRACT_TX_HEADER_SIZE + canonical_length,
        authority,
        authority_length) == 0);

    memset(&decoded, 0, sizeof(decoded));
    CHECK(stn_contract_transaction_decode(
        encoded,
        written,
        &decoded) == STN_CONTRACT_OK);
    CHECK(decoded.version == STN_CONTRACT_TX_VERSION);
    CHECK(decoded.action == STN_CONTRACT_ACTION_APPROVE);
    CHECK(decoded.sequence == UINT64_C(8));
    CHECK(decoded.canonical_contract ==
          encoded + STN_CONTRACT_TX_HEADER_SIZE);
    CHECK(decoded.canonical_contract_length == canonical_length);
    CHECK(memcmp(
        decoded.actor,
        participant.identity,
        STN_IDENTITY_PUBLIC_KEY_SIZE) == 0);
    CHECK(memcmp(
        decoded.signature,
        transaction.signature,
        STN_IDENTITY_SIGNATURE_SIZE) == 0);
    CHECK(decoded.authority_evidence ==
          encoded + STN_CONTRACT_TX_HEADER_SIZE + canonical_length);
    CHECK(decoded.authority_evidence_length == authority_length);

    CHECK(stn_contract_transaction_validate_structure(
        encoded,
        written) == STN_CONTRACT_OK);

    written_again = 0u;
    CHECK(stn_contract_transaction_encode(
        &decoded,
        encoded_again,
        sizeof(encoded_again),
        &written_again) == STN_CONTRACT_OK);
    CHECK(written_again == written);
    CHECK(memcmp(encoded, encoded_again, written) == 0);

    memset(&before, 0x5a, sizeof(before));

    memcpy(mutated, encoded, written);
    mutated[1] = 0x02u;
    decoded = before;
    CHECK(stn_contract_transaction_decode(
        mutated,
        written,
        &decoded) == STN_CONTRACT_VERSION_ERROR);
    CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);

    memcpy(mutated, encoded, written);
    mutated[2] = 0x00u;
    mutated[3] = 0xffu;
    decoded = before;
    CHECK(stn_contract_transaction_decode(
        mutated,
        written,
        &decoded) == STN_CONTRACT_ACTION_ERROR);
    CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);

    memcpy(mutated, encoded, written);
    mutated[15] ^= 0x01u;
    decoded = before;
    CHECK(stn_contract_transaction_decode(
        mutated,
        written,
        &decoded) == STN_CONTRACT_LENGTH);
    CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);

    memcpy(mutated, encoded, written);
    mutated[19] = 0x60u;
    decoded = before;
    CHECK(stn_contract_transaction_decode(
        mutated,
        written,
        &decoded) == STN_CONTRACT_AUTHORITY_ERROR);
    CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);

    memcpy(mutated, encoded, written);
    mutated[STN_CONTRACT_TX_HEADER_SIZE] = 'X';
    decoded = before;
    CHECK(stn_contract_transaction_decode(
        mutated,
        written,
        &decoded) == STN_CONTRACT_LENGTH);
    CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);

    memcpy(mutated, encoded, written);
    mutated[STN_CONTRACT_TX_HEADER_SIZE + canonical_length] = 0xffu;
    decoded = before;
    CHECK(stn_contract_transaction_decode(
        mutated,
        written,
        &decoded) == STN_CONTRACT_AUTHORITY_ERROR);
    CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);

    CHECK(stn_contract_transaction_decode(
        NULL,
        written,
        &decoded) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_transaction_decode(
        encoded,
        written,
        NULL) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_transaction_decode(
        encoded,
        STN_CONTRACT_TX_HEADER_SIZE - 1u,
        &decoded) == STN_CONTRACT_LENGTH);

    memset(guard, 0xa5, sizeof(guard));
    memcpy(guard_before, guard, sizeof(guard));
    written_again = 999u;
    CHECK(stn_contract_transaction_encode(
        &transaction,
        guard,
        expected_length - 1u,
        &written_again) == STN_CONTRACT_CAPACITY);
    CHECK(written_again == 0u);
    CHECK(memcmp(guard, guard_before, sizeof(guard)) == 0);

    transaction.version = 2u;
    written_again = 999u;
    CHECK(stn_contract_transaction_encode(
        &transaction,
        guard,
        sizeof(guard),
        &written_again) == STN_CONTRACT_VERSION_ERROR);
    CHECK(written_again == 0u);
    transaction.version = STN_CONTRACT_TX_VERSION;

    transaction.action = 0xffffu;
    CHECK(stn_contract_transaction_encode(
        &transaction,
        guard,
        sizeof(guard),
        &written_again) == STN_CONTRACT_ACTION_ERROR);
    transaction.action = STN_CONTRACT_ACTION_APPROVE;

    transaction.authority_evidence_length =
        STN_AUTHORITY_EVIDENCE_SIZE - 1u;
    CHECK(stn_contract_transaction_encode(
        &transaction,
        guard,
        sizeof(guard),
        &written_again) == STN_CONTRACT_AUTHORITY_ERROR);
    transaction.authority_evidence_length =
        STN_AUTHORITY_EVIDENCE_SIZE;

    printf(
        "Contract transactions: %u checks, %u failures.\\n",
        checks,
        failures);

    return failures != 0u;
}

#ifdef STN_CONTRACT_TRANSACTION_TEST_MAIN
int main(void)
{
    return test_contract_transaction();
}
#endif
