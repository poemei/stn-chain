/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;

#define CHECK(e) do { \
    ++checks; \
    if (!(e)) { \
        ++failures; \
        fprintf(stderr, "contract line %d: %s\n", __LINE__, #e); \
    } \
} while (0)

static void fill_identity(uint8_t identity[STN_ADDRESS_ID_SIZE], uint8_t start)
{
    size_t i;

    for (i = 0; i < STN_ADDRESS_ID_SIZE; ++i) {
        identity[i] = (uint8_t)(start + (uint8_t)i);
    }
}

int test_contract(void)
{
    static const uint8_t terms[] = {
        'S','T','N',' ','C','h','a','i','n',' ','C','o','n','t','r','a','c','t'
    };
    static const uint8_t signing_actor[STN_IDENTITY_PUBLIC_KEY_SIZE] = {
        0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,
        0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,
        0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,
        0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a
    };
    static const uint8_t contract_signature[STN_IDENTITY_SIGNATURE_SIZE] = {
        0xa2,0xf0,0x63,0xb0,0x36,0xd9,0xb3,0xdb,
        0x38,0xf2,0xe7,0xfd,0x40,0xd5,0xa1,0x0e,
        0x8d,0xb0,0xfe,0xe6,0x5b,0xde,0xb4,0x03,
        0x58,0x99,0x2a,0x2f,0xf9,0xf6,0xd7,0x16,
        0xc8,0x04,0x5e,0x81,0xa0,0x19,0x92,0x44,
        0x5d,0x8a,0xef,0x46,0x49,0x6d,0xe6,0x27,
        0x72,0x68,0xb5,0x5b,0x74,0x47,0x6e,0xe8,
        0x35,0x3e,0x91,0x51,0x86,0x20,0x3c,0x01
    };
    static const uint8_t action_signature[STN_IDENTITY_SIGNATURE_SIZE] = {
        0xdc,0x30,0x7a,0x6a,0xc5,0x82,0x72,0xa0,
        0xbf,0x25,0x47,0x97,0x55,0xf7,0xbc,0xb9,
        0xde,0x70,0x8b,0x77,0xe7,0x52,0x72,0xc0,
        0x9b,0xe5,0xf3,0xca,0x5d,0xc2,0x5a,0x5c,
        0xca,0x8e,0x66,0x0c,0xe2,0x50,0x70,0x43,
        0x8a,0xe8,0x24,0x73,0x19,0x50,0xb3,0x4b,
        0xe5,0xc1,0x46,0xa3,0xc5,0x41,0x8b,0x23,
        0x01,0x74,0x4f,0xd0,0x97,0xa6,0x09,0x01
    };
    stn_contract_participant participants[2];
    stn_contract_participant extracted;
    stn_contract_participant before_participant;
    stn_contract contract;
    stn_contract decoded;
    stn_contract before_contract;
    stn_contract transition_contract;
    stn_contract transitioned;
    stn_contract before_transitioned;
    stn_address address_a;
    stn_address address_b;
    uint8_t canonical[STN_CONTRACT_MAX_SIZE];
    uint8_t canonical_again[STN_CONTRACT_MAX_SIZE];
    uint8_t mutated[STN_CONTRACT_MAX_SIZE];
    uint8_t output_guard[STN_CONTRACT_MAX_SIZE];
    uint8_t output_before[STN_CONTRACT_MAX_SIZE];
    uint8_t authority_action[STN_AUTHORITY_ACTION_SIZE];
    uint8_t authority_action_again[STN_AUTHORITY_ACTION_SIZE];
    uint8_t authority_context[STN_AUTHORITY_CONTEXT_SIZE];
    uint8_t authority_context_again[STN_AUTHORITY_CONTEXT_SIZE];
    uint8_t authority_evidence[STN_AUTHORITY_EVIDENCE_SIZE];
    uint8_t authority_evidence_mutated[STN_AUTHORITY_EVIDENCE_SIZE];
    uint8_t authority_guard[STN_AUTHORITY_CONTEXT_SIZE];
    uint8_t authority_before[STN_AUTHORITY_CONTEXT_SIZE];
    uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE];
    uint8_t other_actor[STN_IDENTITY_PUBLIC_KEY_SIZE];
    uint8_t signature_mutated[STN_IDENTITY_SIGNATURE_SIZE];
    uint8_t signature_contract_mutated[STN_CONTRACT_MAX_SIZE];
    uint8_t action_canonical[STN_CONTRACT_MAX_SIZE];
    uint8_t action_canonical_mutated[STN_CONTRACT_MAX_SIZE];
    uint8_t approval_key[STN_CONTRACT_APPROVAL_KEY_SIZE];
    uint8_t approval_key_again[STN_CONTRACT_APPROVAL_KEY_SIZE];
    uint8_t approval_key_other[STN_CONTRACT_APPROVAL_KEY_SIZE];
    uint8_t approval_key_next_sequence[STN_CONTRACT_APPROVAL_KEY_SIZE];
    uint8_t approval_store[4u * STN_CONTRACT_APPROVAL_KEY_SIZE];
    uint8_t approval_rebuild[3u * STN_CONTRACT_APPROVAL_KEY_SIZE];
    stn_contract_approval_state approval_state;
    stn_contract_approval_state rebuild_state;
    stn_contract_accepted_action accepted_history[2];
    stn_contract history_contract;
    stn_contract rebuilt_contract;
    stn_contract before_rebuilt_contract;
    uint8_t rebuild_canonical_one[STN_CONTRACT_MAX_SIZE];
    uint8_t rebuild_canonical_two[STN_CONTRACT_MAX_SIZE];
    uint8_t rebuild_approval_store[4u * STN_CONTRACT_APPROVAL_KEY_SIZE];
    size_t rebuild_written_one;
    size_t rebuild_written_two;
    char address_text[STN_ADDRESS_TEXT_CAPACITY];
    char address_text_again[STN_ADDRESS_TEXT_CAPACITY];
    size_t written;
    size_t written_again;
    size_t address_written;
    size_t address_written_again;
    size_t expected_size;
    size_t authority_written;
    size_t i;
    uint16_t next_state;
    uint16_t before_state;

    checks = 0;
    failures = 0;

    memset(&participants, 0, sizeof(participants));
    fill_identity(participants[0].identity, 0x00u);
    participants[0].role = STN_CONTRACT_ROLE_ISSUER;
    fill_identity(participants[1].identity, 0x80u);
    participants[1].role = STN_CONTRACT_ROLE_RECIPIENT;

    memset(&contract, 0, sizeof(contract));
    contract.version = STN_CONTRACT_VERSION;
    contract.type = STN_CONTRACT_CONTRIBUTOR_AGREEMENT;
    contract.sequence = UINT64_C(0x0102030405060708);
    contract.created_at = UINT64_C(0x1112131415161718);
    contract.state = STN_CONTRACT_STATE_ISSUED;
    contract.participants = participants;
    contract.participant_count = 2;
    contract.terms = terms;
    contract.terms_length = (uint32_t)sizeof(terms);

    expected_size =
        STN_CONTRACT_HEADER_SIZE +
        (2u * STN_CONTRACT_PARTICIPANT_SIZE) +
        sizeof(terms);

    written = 999u;
    CHECK(stn_contract_encode(
        &contract,
        canonical,
        sizeof(canonical),
        &written) == STN_CONTRACT_OK);
    CHECK(written == expected_size);

    /* Exact canonical STCT header and big-endian fields. */
    CHECK(memcmp(canonical, "STCT", 4) == 0);
    CHECK(canonical[4] == 0x00u && canonical[5] == 0x01u);
    CHECK(canonical[6] == 0x00u && canonical[7] == 0x03u);
    CHECK(memcmp(canonical + 8,
        "\x01\x02\x03\x04\x05\x06\x07\x08", 8) == 0);
    CHECK(memcmp(canonical + 16,
        "\x11\x12\x13\x14\x15\x16\x17\x18", 8) == 0);
    CHECK(canonical[24] == 0x00u && canonical[25] == 0x02u);
    CHECK(canonical[26] == 0x00u && canonical[27] == 0x02u);
    CHECK(canonical[28] == 0x00u &&
          canonical[29] == 0x00u &&
          canonical[30] == 0x00u &&
          canonical[31] == (uint8_t)sizeof(terms));

    for (i = 0; i < STN_ADDRESS_ID_SIZE; ++i) {
        CHECK(canonical[STN_CONTRACT_HEADER_SIZE + i] ==
              participants[0].identity[i]);
    }
    CHECK(canonical[STN_CONTRACT_HEADER_SIZE + 32] == 0x00u);
    CHECK(canonical[STN_CONTRACT_HEADER_SIZE + 33] == 0x02u);

    for (i = 0; i < STN_ADDRESS_ID_SIZE; ++i) {
        CHECK(canonical[
            STN_CONTRACT_HEADER_SIZE +
            STN_CONTRACT_PARTICIPANT_SIZE + i] ==
            participants[1].identity[i]);
    }
    CHECK(canonical[
        STN_CONTRACT_HEADER_SIZE +
        STN_CONTRACT_PARTICIPANT_SIZE + 32] == 0x00u);
    CHECK(canonical[
        STN_CONTRACT_HEADER_SIZE +
        STN_CONTRACT_PARTICIPANT_SIZE + 33] == 0x03u);

    CHECK(memcmp(
        canonical + STN_CONTRACT_HEADER_SIZE +
            (2u * STN_CONTRACT_PARTICIPANT_SIZE),
        terms,
        sizeof(terms)) == 0);

    /* Repeat encoding must produce byte-identical canonical output. */
    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        canonical_again,
        sizeof(canonical_again),
        &written_again) == STN_CONTRACT_OK);
    CHECK(written_again == written);
    CHECK(memcmp(canonical, canonical_again, written) == 0);

    /* Decode borrows canonical spans and never overlays native participants. */
    memset(&decoded, 0xa5, sizeof(decoded));
    CHECK(stn_contract_decode(
        canonical,
        written,
        &decoded) == STN_CONTRACT_OK);
    CHECK(decoded.version == STN_CONTRACT_VERSION);
    CHECK(decoded.type == STN_CONTRACT_CONTRIBUTOR_AGREEMENT);
    CHECK(decoded.sequence == UINT64_C(0x0102030405060708));
    CHECK(decoded.created_at == UINT64_C(0x1112131415161718));
    CHECK(decoded.state == STN_CONTRACT_STATE_ISSUED);
    CHECK(decoded.participants == NULL);
    CHECK(decoded.participant_count == 2u);
    CHECK(decoded.participant_bytes ==
          canonical + STN_CONTRACT_HEADER_SIZE);
    CHECK(decoded.terms_length == sizeof(terms));
    CHECK(decoded.terms ==
          canonical + STN_CONTRACT_HEADER_SIZE +
          (2u * STN_CONTRACT_PARTICIPANT_SIZE));
    CHECK(memcmp(decoded.terms, terms, sizeof(terms)) == 0);

    memset(&extracted, 0, sizeof(extracted));
    CHECK(stn_contract_participant_at(
        &decoded, 0, &extracted) == STN_CONTRACT_OK);
    CHECK(memcmp(
        extracted.identity,
        participants[0].identity,
        STN_ADDRESS_ID_SIZE) == 0);
    CHECK(extracted.role == STN_CONTRACT_ROLE_ISSUER);

    memset(&extracted, 0, sizeof(extracted));
    CHECK(stn_contract_participant_at(
        &decoded, 1, &extracted) == STN_CONTRACT_OK);
    CHECK(memcmp(
        extracted.identity,
        participants[1].identity,
        STN_ADDRESS_ID_SIZE) == 0);
    CHECK(extracted.role == STN_CONTRACT_ROLE_RECIPIENT);

    before_participant = extracted;
    CHECK(stn_contract_participant_at(
        &decoded, 2, &extracted) == STN_CONTRACT_PARTICIPANT_INDEX);
    CHECK(memcmp(
        &extracted,
        &before_participant,
        sizeof(extracted)) == 0);

    CHECK(stn_contract_validate_structure(
        canonical, written) == STN_CONTRACT_OK);

    /* Contract address must be deterministic and use the stnc0_ namespace. */
    CHECK(stn_contract_address(
        canonical, written, &address_a) == STN_CONTRACT_OK);
    CHECK(address_a.type == STN_ADDRESS_CONTRACT);
    CHECK(stn_contract_address(
        canonical, written, &address_b) == STN_CONTRACT_OK);
    CHECK(address_b.type == STN_ADDRESS_CONTRACT);
    CHECK(memcmp(
        address_a.identifier,
        address_b.identifier,
        STN_ADDRESS_ID_SIZE) == 0);

    address_written = 0;
    address_written_again = 0;
    CHECK(stn_address_encode(
        &address_a,
        address_text,
        sizeof(address_text),
        &address_written) == STN_DATA_OK);
    CHECK(stn_address_encode(
        &address_b,
        address_text_again,
        sizeof(address_text_again),
        &address_written_again) == STN_DATA_OK);
    CHECK(address_written == 70u);
    CHECK(address_written_again == address_written);
    CHECK(strncmp(address_text, "stnc0_", 6) == 0);
    CHECK(strcmp(address_text, address_text_again) == 0);

    printf("Contract vector bytes: ");
    for (i = 0; i < written; ++i) {
        printf("%02x", canonical[i]);
    }
    printf("\nContract address: %s\n", address_text);

    /*
     * Decode failures must leave the decoded output unchanged.
     * Exercise malformed magic, version, type, state, role and length.
     */
    memset(&before_contract, 0x5a, sizeof(before_contract));

    memcpy(mutated, canonical, written);
    mutated[0] = 'X';
    decoded = before_contract;
    CHECK(stn_contract_decode(
        mutated, written, &decoded) == STN_CONTRACT_MAGIC);
    CHECK(memcmp(&decoded, &before_contract, sizeof(decoded)) == 0);

    memcpy(mutated, canonical, written);
    mutated[5] = 0x02u;
    decoded = before_contract;
    CHECK(stn_contract_decode(
        mutated, written, &decoded) == STN_CONTRACT_VERSION_ERROR);
    CHECK(memcmp(&decoded, &before_contract, sizeof(decoded)) == 0);

    memcpy(mutated, canonical, written);
    mutated[6] = 0x00u;
    mutated[7] = 0xffu;
    decoded = before_contract;
    CHECK(stn_contract_decode(
        mutated, written, &decoded) == STN_CONTRACT_TYPE_ERROR);
    CHECK(memcmp(&decoded, &before_contract, sizeof(decoded)) == 0);

    memcpy(mutated, canonical, written);
    mutated[24] = 0x00u;
    mutated[25] = 0xffu;
    decoded = before_contract;
    CHECK(stn_contract_decode(
        mutated, written, &decoded) == STN_CONTRACT_STATE_ERROR);
    CHECK(memcmp(&decoded, &before_contract, sizeof(decoded)) == 0);

    memcpy(mutated, canonical, written);
    mutated[STN_CONTRACT_HEADER_SIZE + 32] = 0x00u;
    mutated[STN_CONTRACT_HEADER_SIZE + 33] = 0xffu;
    decoded = before_contract;
    CHECK(stn_contract_decode(
        mutated, written, &decoded) == STN_CONTRACT_ROLE_ERROR);
    CHECK(memcmp(&decoded, &before_contract, sizeof(decoded)) == 0);

    decoded = before_contract;
    CHECK(stn_contract_decode(
        canonical,
        STN_CONTRACT_HEADER_SIZE - 1u,
        &decoded) == STN_CONTRACT_TRUNCATED);
    CHECK(memcmp(&decoded, &before_contract, sizeof(decoded)) == 0);

    decoded = before_contract;
    CHECK(stn_contract_decode(
        canonical,
        written - 1u,
        &decoded) == STN_CONTRACT_LENGTH);
    CHECK(memcmp(&decoded, &before_contract, sizeof(decoded)) == 0);

    /* Declared participant count beyond the v1 bound. */
    memcpy(mutated, canonical, written);
    mutated[26] = 0x00u;
    mutated[27] = (uint8_t)(STN_CONTRACT_MAX_PARTICIPANTS + 1u);
    decoded = before_contract;
    CHECK(stn_contract_decode(
        mutated, written, &decoded) == STN_CONTRACT_PARTICIPANT_LIMIT);
    CHECK(memcmp(&decoded, &before_contract, sizeof(decoded)) == 0);

    /* Declared terms length beyond the v1 bound. */
    memcpy(mutated, canonical, written);
    mutated[28] = 0x00u;
    mutated[29] = 0x01u;
    mutated[30] = 0x00u;
    mutated[31] = 0x01u;
    decoded = before_contract;
    CHECK(stn_contract_decode(
        mutated, written, &decoded) == STN_CONTRACT_TERMS_LIMIT);
    CHECK(memcmp(&decoded, &before_contract, sizeof(decoded)) == 0);

    /*
     * Encode failures promised by the API must leave output untouched and
     * report written == 0.
     */
    memset(output_guard, 0xa5, sizeof(output_guard));
    memcpy(output_before, output_guard, sizeof(output_guard));

    contract.version = 2u;
    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        output_guard,
        sizeof(output_guard),
        &written_again) == STN_CONTRACT_VERSION_ERROR);
    CHECK(written_again == 0u);
    CHECK(memcmp(
        output_guard,
        output_before,
        sizeof(output_guard)) == 0);
    contract.version = STN_CONTRACT_VERSION;

    contract.type = 0xffffu;
    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        output_guard,
        sizeof(output_guard),
        &written_again) == STN_CONTRACT_TYPE_ERROR);
    CHECK(written_again == 0u);
    CHECK(memcmp(
        output_guard,
        output_before,
        sizeof(output_guard)) == 0);
    contract.type = STN_CONTRACT_CONTRIBUTOR_AGREEMENT;

    contract.state = 0xffffu;
    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        output_guard,
        sizeof(output_guard),
        &written_again) == STN_CONTRACT_STATE_ERROR);
    CHECK(written_again == 0u);
    CHECK(memcmp(
        output_guard,
        output_before,
        sizeof(output_guard)) == 0);
    contract.state = STN_CONTRACT_STATE_ISSUED;

    participants[0].role = 0xffffu;
    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        output_guard,
        sizeof(output_guard),
        &written_again) == STN_CONTRACT_ROLE_ERROR);
    CHECK(written_again == 0u);
    CHECK(memcmp(
        output_guard,
        output_before,
        sizeof(output_guard)) == 0);
    participants[0].role = STN_CONTRACT_ROLE_ISSUER;

    contract.participant_count =
        (uint16_t)(STN_CONTRACT_MAX_PARTICIPANTS + 1u);
    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        output_guard,
        sizeof(output_guard),
        &written_again) == STN_CONTRACT_PARTICIPANT_LIMIT);
    CHECK(written_again == 0u);
    CHECK(memcmp(
        output_guard,
        output_before,
        sizeof(output_guard)) == 0);
    contract.participant_count = 2u;

    contract.terms_length = STN_CONTRACT_MAX_TERMS + 1u;
    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        output_guard,
        sizeof(output_guard),
        &written_again) == STN_CONTRACT_TERMS_LIMIT);
    CHECK(written_again == 0u);
    CHECK(memcmp(
        output_guard,
        output_before,
        sizeof(output_guard)) == 0);
    contract.terms_length = (uint32_t)sizeof(terms);

    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        output_guard,
        expected_size - 1u,
        &written_again) == STN_CONTRACT_CAPACITY);
    CHECK(written_again == 0u);
    CHECK(memcmp(
        output_guard,
        output_before,
        sizeof(output_guard)) == 0);

    contract.participants = NULL;
    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        output_guard,
        sizeof(output_guard),
        &written_again) == STN_CONTRACT_ARGUMENT);
    CHECK(written_again == 0u);
    CHECK(memcmp(
        output_guard,
        output_before,
        sizeof(output_guard)) == 0);
    contract.participants = participants;

    contract.terms = NULL;
    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        output_guard,
        sizeof(output_guard),
        &written_again) == STN_CONTRACT_ARGUMENT);
    CHECK(written_again == 0u);
    CHECK(memcmp(
        output_guard,
        output_before,
        sizeof(output_guard)) == 0);
    contract.terms = terms;

    /*
     * Contract Engine lifecycle policy.
     *
     * These checks qualify deterministic state transitions only. Identity,
     * signatures, scoped authority, accepted history and consensus acceptance
     * remain separate protocol concerns.
     */
    next_state = 0u;
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_DRAFT,
        STN_CONTRACT_ACTION_CREATE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_ISSUED);

    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_ISSUED,
        STN_CONTRACT_ACTION_AMEND,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REVIEW);

    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_REVIEW,
        STN_CONTRACT_ACTION_AMEND,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REVIEW);

    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_REVIEW,
        STN_CONTRACT_ACTION_APPROVE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_APPROVALS);

    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_APPROVALS,
        STN_CONTRACT_ACTION_APPROVE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_ATTESTATION);

    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_ATTESTATION,
        STN_CONTRACT_ACTION_EXECUTE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_EXECUTED);

    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_EXECUTED,
        STN_CONTRACT_ACTION_CLOSE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_CLOSED);

    /* Rejection is permitted while an agreement remains unresolved. */
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_ISSUED,
        STN_CONTRACT_ACTION_REJECT,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REJECTED);
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_REVIEW,
        STN_CONTRACT_ACTION_REJECT,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REJECTED);
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_APPROVALS,
        STN_CONTRACT_ACTION_REJECT,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REJECTED);
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_ATTESTATION,
        STN_CONTRACT_ACTION_REJECT,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REJECTED);

    /* Revocation is permitted from issued through executed state. */
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_ISSUED,
        STN_CONTRACT_ACTION_REVOKE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REVOKED);
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_REVIEW,
        STN_CONTRACT_ACTION_REVOKE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REVOKED);
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_APPROVALS,
        STN_CONTRACT_ACTION_REVOKE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REVOKED);
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_ATTESTATION,
        STN_CONTRACT_ACTION_REVOKE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REVOKED);
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_EXECUTED,
        STN_CONTRACT_ACTION_REVOKE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_REVOKED);

    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_REJECTED,
        STN_CONTRACT_ACTION_CLOSE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_CLOSED);
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_REVOKED,
        STN_CONTRACT_ACTION_CLOSE,
        &next_state) == STN_CONTRACT_OK);
    CHECK(next_state == STN_CONTRACT_STATE_CLOSED);

    /* Invalid action/state combinations leave output unchanged. */
    before_state = UINT16_C(0xa5a5);
    next_state = before_state;
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_ISSUED,
        STN_CONTRACT_ACTION_CREATE,
        &next_state) == STN_CONTRACT_TRANSITION_ERROR);
    CHECK(next_state == before_state);

    next_state = before_state;
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_DRAFT,
        STN_CONTRACT_ACTION_APPROVE,
        &next_state) == STN_CONTRACT_TRANSITION_ERROR);
    CHECK(next_state == before_state);

    next_state = before_state;
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_EXECUTED,
        STN_CONTRACT_ACTION_EXECUTE,
        &next_state) == STN_CONTRACT_TRANSITION_ERROR);
    CHECK(next_state == before_state);

    next_state = before_state;
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_CLOSED,
        STN_CONTRACT_ACTION_AMEND,
        &next_state) == STN_CONTRACT_TRANSITION_ERROR);
    CHECK(next_state == before_state);

    next_state = before_state;
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_CLOSED,
        STN_CONTRACT_ACTION_REJECT,
        &next_state) == STN_CONTRACT_TRANSITION_ERROR);
    CHECK(next_state == before_state);

    next_state = before_state;
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_CLOSED,
        STN_CONTRACT_ACTION_REVOKE,
        &next_state) == STN_CONTRACT_TRANSITION_ERROR);
    CHECK(next_state == before_state);

    next_state = before_state;
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_DRAFT,
        STN_CONTRACT_ACTION_CLOSE,
        &next_state) == STN_CONTRACT_TRANSITION_ERROR);
    CHECK(next_state == before_state);

    next_state = before_state;
    CHECK(stn_contract_transition(
        0xffffu,
        STN_CONTRACT_ACTION_CREATE,
        &next_state) == STN_CONTRACT_STATE_ERROR);
    CHECK(next_state == before_state);

    next_state = before_state;
    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_DRAFT,
        0xffffu,
        &next_state) == STN_CONTRACT_ACTION_ERROR);
    CHECK(next_state == before_state);

    CHECK(stn_contract_transition(
        STN_CONTRACT_STATE_DRAFT,
        STN_CONTRACT_ACTION_CREATE,
        NULL) == STN_CONTRACT_ARGUMENT);

    /*
     * Applying an action advances sequence exactly once and changes only the
     * protocol state/sequence fields.
     */
    transition_contract = contract;
    transition_contract.sequence = UINT64_C(40);
    transition_contract.state = STN_CONTRACT_STATE_DRAFT;

    memset(&transitioned, 0, sizeof(transitioned));
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_CREATE,
        UINT64_C(41),
        &transitioned) == STN_CONTRACT_OK);
    CHECK(transitioned.version == transition_contract.version);
    CHECK(transitioned.type == transition_contract.type);
    CHECK(transitioned.sequence == UINT64_C(41));
    CHECK(transitioned.created_at == transition_contract.created_at);
    CHECK(transitioned.state == STN_CONTRACT_STATE_ISSUED);
    CHECK(transitioned.participants == transition_contract.participants);
    CHECK(transitioned.participant_count == transition_contract.participant_count);
    CHECK(transitioned.participant_bytes == transition_contract.participant_bytes);
    CHECK(transitioned.terms == transition_contract.terms);
    CHECK(transitioned.terms_length == transition_contract.terms_length);

    transition_contract = transitioned;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_AMEND,
        UINT64_C(42),
        &transitioned) == STN_CONTRACT_OK);
    CHECK(transitioned.sequence == UINT64_C(42));
    CHECK(transitioned.state == STN_CONTRACT_STATE_REVIEW);

    transition_contract = transitioned;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        &transitioned) == STN_CONTRACT_OK);
    CHECK(transitioned.sequence == UINT64_C(43));
    CHECK(transitioned.state == STN_CONTRACT_STATE_APPROVALS);

    transition_contract = transitioned;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(44),
        &transitioned) == STN_CONTRACT_OK);
    CHECK(transitioned.sequence == UINT64_C(44));
    CHECK(transitioned.state == STN_CONTRACT_STATE_ATTESTATION);

    transition_contract = transitioned;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_EXECUTE,
        UINT64_C(45),
        &transitioned) == STN_CONTRACT_OK);
    CHECK(transitioned.sequence == UINT64_C(45));
    CHECK(transitioned.state == STN_CONTRACT_STATE_EXECUTED);

    transition_contract = transitioned;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_CLOSE,
        UINT64_C(46),
        &transitioned) == STN_CONTRACT_OK);
    CHECK(transitioned.sequence == UINT64_C(46));
    CHECK(transitioned.state == STN_CONTRACT_STATE_CLOSED);

    /* Apply failures preserve the destination object. */
    memset(&before_transitioned, 0x5a, sizeof(before_transitioned));
    transitioned = before_transitioned;
    transition_contract = contract;
    transition_contract.sequence = UINT64_C(100);
    transition_contract.state = STN_CONTRACT_STATE_DRAFT;

    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_CREATE,
        UINT64_C(100),
        &transitioned) == STN_CONTRACT_SEQUENCE_ERROR);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_CREATE,
        UINT64_C(102),
        &transitioned) == STN_CONTRACT_SEQUENCE_ERROR);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    transition_contract.sequence = UINT64_MAX;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_CREATE,
        0u,
        &transitioned) == STN_CONTRACT_SEQUENCE_ERROR);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    transition_contract.sequence = UINT64_C(100);
    transition_contract.state = STN_CONTRACT_STATE_CLOSED;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_CREATE,
        UINT64_C(101),
        &transitioned) == STN_CONTRACT_TRANSITION_ERROR);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    transition_contract.state = STN_CONTRACT_STATE_DRAFT;
    transition_contract.version = 2u;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_CREATE,
        UINT64_C(101),
        &transitioned) == STN_CONTRACT_VERSION_ERROR);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    transition_contract.version = STN_CONTRACT_VERSION;
    transition_contract.type = 0xffffu;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_CREATE,
        UINT64_C(101),
        &transitioned) == STN_CONTRACT_TYPE_ERROR);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    transition_contract.type = STN_CONTRACT_CONTRIBUTOR_AGREEMENT;
    transition_contract.state = 0xffffu;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_CREATE,
        UINT64_C(101),
        &transitioned) == STN_CONTRACT_STATE_ERROR);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    transition_contract.state = STN_CONTRACT_STATE_DRAFT;
    CHECK(stn_contract_apply_action(
        &transition_contract,
        0xffffu,
        UINT64_C(101),
        &transitioned) == STN_CONTRACT_ACTION_ERROR);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    CHECK(stn_contract_apply_action(
        NULL,
        STN_CONTRACT_ACTION_CREATE,
        UINT64_C(101),
        &transitioned) == STN_CONTRACT_ARGUMENT);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_CREATE,
        UINT64_C(101),
        NULL) == STN_CONTRACT_ARGUMENT);


    /*
     * Phase 14 scoped-authority bridge.
     *
     * Contract v1 assigns deterministic meanings to the existing opaque
     * Phase 14 action/context tokens. The authority primitive still owns
     * evidence structure and exact subject/action/context matching.
     */
    memset(authority_action, 0, sizeof(authority_action));
    memset(authority_action_again, 0, sizeof(authority_action_again));

    CHECK(stn_contract_authority_action(
        STN_CONTRACT_ACTION_APPROVE,
        authority_action) == STN_CONTRACT_OK);
    CHECK(authority_action[0] == STN_AUTHORITY_VERSION);
    CHECK(authority_action[1] == STN_CONTRACT_AUTHORITY_ACTION_DOMAIN);
    CHECK(authority_action[2] == 0x00u);
    CHECK(authority_action[3] == (uint8_t)STN_CONTRACT_ACTION_APPROVE);
    for (i = 4u; i < STN_AUTHORITY_ACTION_SIZE; ++i) {
        CHECK(authority_action[i] == 0u);
    }

    CHECK(stn_contract_authority_action(
        STN_CONTRACT_ACTION_APPROVE,
        authority_action_again) == STN_CONTRACT_OK);
    CHECK(memcmp(
        authority_action,
        authority_action_again,
        STN_AUTHORITY_ACTION_SIZE) == 0);

    memset(authority_guard, 0x5au, sizeof(authority_guard));
    memcpy(authority_before, authority_guard, sizeof(authority_guard));
    CHECK(stn_contract_authority_action(
        0xffffu,
        authority_guard) == STN_CONTRACT_ACTION_ERROR);
    CHECK(memcmp(
        authority_guard,
        authority_before,
        STN_AUTHORITY_ACTION_SIZE) == 0);
    CHECK(stn_contract_authority_action(
        STN_CONTRACT_ACTION_APPROVE,
        NULL) == STN_CONTRACT_ARGUMENT);

    memset(authority_context, 0, sizeof(authority_context));
    memset(authority_context_again, 0, sizeof(authority_context_again));

    CHECK(stn_contract_authority_context(
        canonical,
        written,
        authority_context) == STN_CONTRACT_OK);
    CHECK(authority_context[0] == STN_AUTHORITY_VERSION);
    CHECK(authority_context[1] == STN_CONTRACT_AUTHORITY_CONTEXT_DOMAIN);
    CHECK(memcmp(
        authority_context + 2,
        address_a.identifier,
        STN_AUTHORITY_CONTEXT_SIZE - 2u) == 0);

    CHECK(stn_contract_authority_context(
        canonical,
        written,
        authority_context_again) == STN_CONTRACT_OK);
    CHECK(memcmp(
        authority_context,
        authority_context_again,
        STN_AUTHORITY_CONTEXT_SIZE) == 0);

    memset(authority_guard, 0x5au, sizeof(authority_guard));
    memcpy(authority_before, authority_guard, sizeof(authority_guard));
    CHECK(stn_contract_authority_context(
        mutated,
        STN_CONTRACT_HEADER_SIZE - 1u,
        authority_guard) == STN_CONTRACT_TRUNCATED);
    CHECK(memcmp(
        authority_guard,
        authority_before,
        STN_AUTHORITY_CONTEXT_SIZE) == 0);
    CHECK(stn_contract_authority_context(
        NULL,
        written,
        authority_guard) == STN_CONTRACT_ARGUMENT);
    CHECK(memcmp(
        authority_guard,
        authority_before,
        STN_AUTHORITY_CONTEXT_SIZE) == 0);
    CHECK(stn_contract_authority_context(
        canonical,
        written,
        NULL) == STN_CONTRACT_ARGUMENT);

    /*
     * Build canonical Phase 14 evidence and prove that Contract evaluation
     * accepts only the exact actor/action/context tuple.
     */
    fill_identity(actor, 0x20u);
    fill_identity(other_actor, 0x40u);

    authority_written = 0u;
    CHECK(stn_authority_evidence_encode(
        actor,
        authority_action,
        authority_context,
        authority_evidence,
        sizeof(authority_evidence),
        &authority_written) == STN_AUTHORITY_AUTHORIZED);
    CHECK(authority_written == STN_AUTHORITY_EVIDENCE_SIZE);

    CHECK(stn_contract_authority_evaluate(
        actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        authority_evidence,
        authority_written) == STN_CONTRACT_OK);

    CHECK(stn_contract_authority_evaluate(
        other_actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        authority_evidence,
        authority_written) == STN_CONTRACT_AUTHORITY_ERROR);

    CHECK(stn_contract_authority_evaluate(
        actor,
        STN_CONTRACT_ACTION_REJECT,
        canonical,
        written,
        authority_evidence,
        authority_written) == STN_CONTRACT_AUTHORITY_ERROR);

    memcpy(
        authority_evidence_mutated,
        authority_evidence,
        authority_written);
    authority_evidence_mutated[
        STN_AUTHORITY_EVIDENCE_SIZE - 1u] ^= 0x01u;
    CHECK(stn_contract_authority_evaluate(
        actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        authority_evidence_mutated,
        authority_written) == STN_CONTRACT_AUTHORITY_ERROR);

    CHECK(stn_contract_authority_evaluate(
        actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        NULL,
        0u) == STN_CONTRACT_AUTHORITY_ERROR);

    CHECK(stn_contract_authority_evaluate(
        actor,
        0xffffu,
        canonical,
        written,
        authority_evidence,
        authority_written) == STN_CONTRACT_ACTION_ERROR);

    CHECK(stn_contract_authority_evaluate(
        actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        STN_CONTRACT_HEADER_SIZE - 1u,
        authority_evidence,
        authority_written) == STN_CONTRACT_TRUNCATED);

    CHECK(stn_contract_authority_evaluate(
        NULL,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        authority_evidence,
        authority_written) == STN_CONTRACT_ARGUMENT);

    CHECK(stn_contract_authority_evaluate(
        actor,
        STN_CONTRACT_ACTION_APPROVE,
        NULL,
        written,
        authority_evidence,
        authority_written) == STN_CONTRACT_ARGUMENT);

    /*
     * Contract action signature verification.
     *
     * This fixed vector signs the existing canonical Contract v1 vector using
     * the existing STN identity statement domain. Verification must bind the
     * exact canonical contract, canonical actor, action and action sequence.
     * Signature validity authenticates only; authority and consensus remain
     * separate checks.
     */
    CHECK(stn_contract_signature_verify(
        signing_actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        UINT64_C(43),
        contract_signature) == STN_CONTRACT_OK);

    CHECK(stn_contract_signature_verify(
        signing_actor,
        STN_CONTRACT_ACTION_REJECT,
        canonical,
        written,
        UINT64_C(43),
        contract_signature) == STN_CONTRACT_SIGNATURE_ERROR);

    CHECK(stn_contract_signature_verify(
        signing_actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        UINT64_C(44),
        contract_signature) == STN_CONTRACT_SIGNATURE_ERROR);

    CHECK(stn_contract_signature_verify(
        other_actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        UINT64_C(43),
        contract_signature) == STN_CONTRACT_SIGNATURE_ERROR);

    memcpy(signature_contract_mutated, canonical, written);
    signature_contract_mutated[written - 1u] ^= 0x01u;
    CHECK(stn_contract_signature_verify(
        signing_actor,
        STN_CONTRACT_ACTION_APPROVE,
        signature_contract_mutated,
        written,
        UINT64_C(43),
        contract_signature) == STN_CONTRACT_SIGNATURE_ERROR);

    memcpy(signature_mutated, contract_signature, sizeof(signature_mutated));
    signature_mutated[STN_IDENTITY_SIGNATURE_SIZE - 1u] ^= 0x01u;
    CHECK(stn_contract_signature_verify(
        signing_actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        UINT64_C(43),
        signature_mutated) == STN_CONTRACT_SIGNATURE_ERROR);

    CHECK(stn_contract_signature_verify(
        signing_actor,
        0xffffu,
        canonical,
        written,
        UINT64_C(43),
        contract_signature) == STN_CONTRACT_ACTION_ERROR);

    CHECK(stn_contract_signature_verify(
        signing_actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        STN_CONTRACT_HEADER_SIZE - 1u,
        UINT64_C(43),
        contract_signature) == STN_CONTRACT_TRUNCATED);

    CHECK(stn_contract_signature_verify(
        NULL,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        UINT64_C(43),
        contract_signature) == STN_CONTRACT_ARGUMENT);

    CHECK(stn_contract_signature_verify(
        signing_actor,
        STN_CONTRACT_ACTION_APPROVE,
        NULL,
        written,
        UINT64_C(43),
        contract_signature) == STN_CONTRACT_ARGUMENT);

    CHECK(stn_contract_signature_verify(
        signing_actor,
        STN_CONTRACT_ACTION_APPROVE,
        canonical,
        written,
        UINT64_C(43),
        NULL) == STN_CONTRACT_ARGUMENT);


    /*
     * Contract duplicate-approval prevention.
     *
     * Approval identity is scoped to the exact canonical contract identifier,
     * action sequence and canonical approving identity. Only accepted approval
     * state is consumed; pending arrival order is not authoritative.
     */
    memset(approval_key, 0, sizeof(approval_key));
    memset(approval_key_again, 0, sizeof(approval_key_again));
    memset(approval_key_other, 0, sizeof(approval_key_other));
    memset(approval_key_next_sequence, 0, sizeof(approval_key_next_sequence));

    CHECK(stn_contract_approval_key(
        canonical,
        written,
        UINT64_C(43),
        actor,
        approval_key) == STN_CONTRACT_OK);

    CHECK(memcmp(
        approval_key,
        address_a.identifier,
        STN_ADDRESS_ID_SIZE) == 0);
    CHECK(memcmp(
        approval_key + STN_ADDRESS_ID_SIZE,
        "\x00\x00\x00\x00\x00\x00\x00\x2b",
        8u) == 0);

    CHECK(stn_contract_approval_key(
        canonical,
        written,
        UINT64_C(43),
        actor,
        approval_key_again) == STN_CONTRACT_OK);
    CHECK(memcmp(
        approval_key,
        approval_key_again,
        STN_CONTRACT_APPROVAL_KEY_SIZE) == 0);

    CHECK(stn_contract_approval_key(
        canonical,
        written,
        UINT64_C(43),
        other_actor,
        approval_key_other) == STN_CONTRACT_OK);
    CHECK(memcmp(
        approval_key,
        approval_key_other,
        STN_CONTRACT_APPROVAL_KEY_SIZE) != 0);

    CHECK(stn_contract_approval_key(
        canonical,
        written,
        UINT64_C(44),
        actor,
        approval_key_next_sequence) == STN_CONTRACT_OK);
    CHECK(memcmp(
        approval_key,
        approval_key_next_sequence,
        STN_CONTRACT_APPROVAL_KEY_SIZE) != 0);

    memset(approval_store, 0, sizeof(approval_store));
    stn_contract_approval_state_initialize(
        &approval_state,
        approval_store,
        4u);
    CHECK(approval_state.accepted_count == 0u);

    CHECK(stn_contract_approval_state_check(
        &approval_state,
        approval_key) == STN_CONTRACT_OK);
    CHECK(stn_contract_approval_state_consume(
        &approval_state,
        approval_key) == STN_CONTRACT_OK);
    CHECK(approval_state.accepted_count == 1u);
    CHECK(stn_contract_approval_state_check(
        &approval_state,
        approval_key) == STN_CONTRACT_DUPLICATE_APPROVAL);
    CHECK(stn_contract_approval_state_consume(
        &approval_state,
        approval_key) == STN_CONTRACT_DUPLICATE_APPROVAL);
    CHECK(approval_state.accepted_count == 1u);

    /* Different identity and later sequence remain independently fresh. */
    CHECK(stn_contract_approval_state_check(
        &approval_state,
        approval_key_other) == STN_CONTRACT_OK);
    CHECK(stn_contract_approval_state_consume(
        &approval_state,
        approval_key_other) == STN_CONTRACT_OK);
    CHECK(stn_contract_approval_state_check(
        &approval_state,
        approval_key_next_sequence) == STN_CONTRACT_OK);
    CHECK(stn_contract_approval_state_consume(
        &approval_state,
        approval_key_next_sequence) == STN_CONTRACT_OK);
    CHECK(approval_state.accepted_count == 3u);

    /* Accepted-history rebuild reproduces the same deterministic state. */
    memcpy(
        approval_rebuild,
        approval_key,
        STN_CONTRACT_APPROVAL_KEY_SIZE);
    memcpy(
        approval_rebuild + STN_CONTRACT_APPROVAL_KEY_SIZE,
        approval_key_other,
        STN_CONTRACT_APPROVAL_KEY_SIZE);
    memcpy(
        approval_rebuild + (2u * STN_CONTRACT_APPROVAL_KEY_SIZE),
        approval_key_next_sequence,
        STN_CONTRACT_APPROVAL_KEY_SIZE);

    approval_state.accepted_count = 0u;
    CHECK(stn_contract_approval_state_rebuild(
        &approval_state,
        approval_rebuild,
        3u) == STN_CONTRACT_OK);
    CHECK(approval_state.accepted_count == 3u);
    CHECK(stn_contract_approval_state_check(
        &approval_state,
        approval_key) == STN_CONTRACT_DUPLICATE_APPROVAL);
    CHECK(stn_contract_approval_state_check(
        &approval_state,
        approval_key_other) == STN_CONTRACT_DUPLICATE_APPROVAL);
    CHECK(stn_contract_approval_state_check(
        &approval_state,
        approval_key_next_sequence) == STN_CONTRACT_DUPLICATE_APPROVAL);

    /* Duplicate accepted history is invalid and does not publish partial state. */
    memcpy(
        approval_rebuild + STN_CONTRACT_APPROVAL_KEY_SIZE,
        approval_key,
        STN_CONTRACT_APPROVAL_KEY_SIZE);
    approval_state.accepted_count = 0u;
    CHECK(stn_contract_approval_state_rebuild(
        &approval_state,
        approval_rebuild,
        2u) == STN_CONTRACT_DUPLICATE_APPROVAL);
    CHECK(approval_state.accepted_count == 0u);

    /* Capacity and malformed arguments fail deterministically. */
    stn_contract_approval_state_initialize(
        &approval_state,
        approval_store,
        1u);
    CHECK(stn_contract_approval_state_consume(
        &approval_state,
        approval_key) == STN_CONTRACT_OK);
    CHECK(stn_contract_approval_state_consume(
        &approval_state,
        approval_key_other) == STN_CONTRACT_CAPACITY);
    CHECK(approval_state.accepted_count == 1u);

    CHECK(stn_contract_approval_key(
        canonical,
        STN_CONTRACT_HEADER_SIZE - 1u,
        UINT64_C(43),
        actor,
        approval_key_again) == STN_CONTRACT_TRUNCATED);
    CHECK(stn_contract_approval_key(
        NULL,
        written,
        UINT64_C(43),
        actor,
        approval_key_again) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_approval_key(
        canonical,
        written,
        UINT64_C(43),
        NULL,
        approval_key_again) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_approval_key(
        canonical,
        written,
        UINT64_C(43),
        actor,
        NULL) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_approval_state_check(
        NULL,
        approval_key) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_approval_state_consume(
        NULL,
        approval_key) == STN_CONTRACT_ARGUMENT);

    /*
     * Contract action validation composition.
     *
     * Build one exact REVIEW contract at sequence 42. The fixed signature
     * authenticates signing_actor approving that canonical contract at
     * sequence 43. Authority evidence is scoped to the same actor/action/
     * contract tuple. Validation must not consume accepted approval state.
     */
    transition_contract = contract;
    transition_contract.sequence = UINT64_C(42);
    transition_contract.state = STN_CONTRACT_STATE_REVIEW;

    written_again = 0u;
    CHECK(stn_contract_encode(
        &transition_contract,
        action_canonical,
        sizeof(action_canonical),
        &written_again) == STN_CONTRACT_OK);

    CHECK(stn_contract_authority_action(
        STN_CONTRACT_ACTION_APPROVE,
        authority_action) == STN_CONTRACT_OK);
    CHECK(stn_contract_authority_context(
        action_canonical,
        written_again,
        authority_context) == STN_CONTRACT_OK);

    authority_written = 0u;
    CHECK(stn_authority_evidence_encode(
        signing_actor,
        authority_action,
        authority_context,
        authority_evidence,
        sizeof(authority_evidence),
        &authority_written) == STN_AUTHORITY_AUTHORIZED);
    CHECK(authority_written == STN_AUTHORITY_EVIDENCE_SIZE);

    memset(approval_store, 0, sizeof(approval_store));
    stn_contract_approval_state_initialize(
        &approval_state,
        approval_store,
        4u);

    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        action_signature,
        authority_evidence,
        authority_written,
        &approval_state) == STN_CONTRACT_OK);
    CHECK(approval_state.accepted_count == 0u);

    /* Exact +1 sequence is part of action validity. */
    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(44),
        action_canonical,
        written_again,
        signing_actor,
        action_signature,
        authority_evidence,
        authority_written,
        &approval_state) == STN_CONTRACT_SEQUENCE_ERROR);

    /* The canonical bytes must describe the supplied current contract exactly. */
    memcpy(action_canonical_mutated, action_canonical, written_again);
    action_canonical_mutated[written_again - 1u] ^= 0x01u;
    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical_mutated,
        written_again,
        signing_actor,
        action_signature,
        authority_evidence,
        authority_written,
        &approval_state) == STN_CONTRACT_LENGTH);

    /* A different current object cannot borrow valid canonical action evidence. */
    before_contract = transition_contract;
    before_contract.created_at ^= UINT64_C(1);
    CHECK(stn_contract_validate_action(
        &before_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        action_signature,
        authority_evidence,
        authority_written,
        &approval_state) == STN_CONTRACT_LENGTH);

    /* Signature authentication remains an independent required layer. */
    memcpy(signature_mutated, action_signature, sizeof(signature_mutated));
    signature_mutated[STN_IDENTITY_SIGNATURE_SIZE - 1u] ^= 0x01u;
    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        signature_mutated,
        authority_evidence,
        authority_written,
        &approval_state) == STN_CONTRACT_SIGNATURE_ERROR);

    /* Valid signature without matching scoped authority is not sufficient. */
    memcpy(
        authority_evidence_mutated,
        authority_evidence,
        authority_written);
    authority_evidence_mutated[
        STN_AUTHORITY_EVIDENCE_SIZE - 1u] ^= 0x01u;
    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        action_signature,
        authority_evidence_mutated,
        authority_written,
        &approval_state) == STN_CONTRACT_AUTHORITY_ERROR);

    /* APPROVE checks accepted duplicate state but does not consume it. */
    CHECK(stn_contract_approval_key(
        action_canonical,
        written_again,
        UINT64_C(43),
        signing_actor,
        approval_key) == STN_CONTRACT_OK);
    CHECK(stn_contract_approval_state_consume(
        &approval_state,
        approval_key) == STN_CONTRACT_OK);
    CHECK(approval_state.accepted_count == 1u);
    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        action_signature,
        authority_evidence,
        authority_written,
        &approval_state) == STN_CONTRACT_DUPLICATE_APPROVAL);
    CHECK(approval_state.accepted_count == 1u);

    /* APPROVE requires an accepted-history view for duplicate detection. */
    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        action_signature,
        authority_evidence,
        authority_written,
        NULL) == STN_CONTRACT_ARGUMENT);

    /* Malformed required inputs fail before any state can be changed. */
    CHECK(stn_contract_validate_action(
        NULL,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        action_signature,
        authority_evidence,
        authority_written,
        &approval_state) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        NULL,
        written_again,
        signing_actor,
        action_signature,
        authority_evidence,
        authority_written,
        &approval_state) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        NULL,
        action_signature,
        authority_evidence,
        authority_written,
        &approval_state) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        NULL,
        authority_evidence,
        authority_written,
        &approval_state) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_validate_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        action_signature,
        NULL,
        authority_written,
        &approval_state) == STN_CONTRACT_ARGUMENT);


    /*
     * Accepted Contract action application.
     *
     * Consensus acceptance is established by the caller before this primitive
     * is invoked. Accepted application advances the deterministic Contract
     * state and, for APPROVE, consumes exactly one accepted approval key.
     */
    transition_contract = contract;
    transition_contract.sequence = UINT64_C(42);
    transition_contract.state = STN_CONTRACT_STATE_REVIEW;

    written_again = 0u;
    CHECK(stn_contract_encode(
        &transition_contract,
        action_canonical,
        sizeof(action_canonical),
        &written_again) == STN_CONTRACT_OK);

    memset(approval_store, 0, sizeof(approval_store));
    stn_contract_approval_state_initialize(
        &approval_state,
        approval_store,
        4u);

    memset(&transitioned, 0, sizeof(transitioned));
    CHECK(stn_contract_accept_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        &approval_state,
        &transitioned) == STN_CONTRACT_OK);
    CHECK(transitioned.version == transition_contract.version);
    CHECK(transitioned.type == transition_contract.type);
    CHECK(transitioned.sequence == UINT64_C(43));
    CHECK(transitioned.created_at == transition_contract.created_at);
    CHECK(transitioned.state == STN_CONTRACT_STATE_APPROVALS);
    CHECK(transitioned.participants == transition_contract.participants);
    CHECK(transitioned.participant_count ==
          transition_contract.participant_count);
    CHECK(transitioned.participant_bytes ==
          transition_contract.participant_bytes);
    CHECK(transitioned.terms == transition_contract.terms);
    CHECK(transitioned.terms_length == transition_contract.terms_length);
    CHECK(approval_state.accepted_count == 1u);

    CHECK(stn_contract_approval_key(
        action_canonical,
        written_again,
        UINT64_C(43),
        signing_actor,
        approval_key) == STN_CONTRACT_OK);
    CHECK(stn_contract_approval_state_check(
        &approval_state,
        approval_key) == STN_CONTRACT_DUPLICATE_APPROVAL);

    /*
     * Re-applying the same accepted APPROVE is a duplicate. Neither the
     * destination Contract nor accepted approval state may change.
     */
    memset(&before_transitioned, 0x5au, sizeof(before_transitioned));
    transitioned = before_transitioned;
    CHECK(stn_contract_accept_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        &approval_state,
        &transitioned) == STN_CONTRACT_DUPLICATE_APPROVAL);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);
    CHECK(approval_state.accepted_count == 1u);

    /*
     * Capacity failure is atomic. A fresh accepted APPROVE cannot publish the
     * next Contract if its approval key cannot also enter accepted state.
     */
    stn_contract_approval_state_initialize(
        &approval_state,
        approval_store,
        1u);
    CHECK(stn_contract_approval_state_consume(
        &approval_state,
        approval_key) == STN_CONTRACT_OK);
    CHECK(approval_state.accepted_count == 1u);

    transitioned = before_transitioned;
    CHECK(stn_contract_accept_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        other_actor,
        &approval_state,
        &transitioned) == STN_CONTRACT_CAPACITY);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);
    CHECK(approval_state.accepted_count == 1u);

    /*
     * Canonical mismatch and sequence failure also preserve both caller-visible
     * outputs.
     */
    memset(approval_store, 0, sizeof(approval_store));
    stn_contract_approval_state_initialize(
        &approval_state,
        approval_store,
        4u);

    memcpy(action_canonical_mutated, action_canonical, written_again);
    action_canonical_mutated[written_again - 1u] ^= 0x01u;

    transitioned = before_transitioned;
    CHECK(stn_contract_accept_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical_mutated,
        written_again,
        signing_actor,
        &approval_state,
        &transitioned) == STN_CONTRACT_LENGTH);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);
    CHECK(approval_state.accepted_count == 0u);

    transitioned = before_transitioned;
    CHECK(stn_contract_accept_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(44),
        action_canonical,
        written_again,
        signing_actor,
        &approval_state,
        &transitioned) == STN_CONTRACT_SEQUENCE_ERROR);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);
    CHECK(approval_state.accepted_count == 0u);

    /*
     * Non-APPROVE accepted actions do not require or consume approval state.
     * REVIEW -> REJECTED is deterministic and advances sequence exactly once.
     */
    transitioned = before_transitioned;
    CHECK(stn_contract_accept_action(
        &transition_contract,
        STN_CONTRACT_ACTION_REJECT,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        NULL,
        &transitioned) == STN_CONTRACT_OK);
    CHECK(transitioned.sequence == UINT64_C(43));
    CHECK(transitioned.state == STN_CONTRACT_STATE_REJECTED);
    CHECK(approval_state.accepted_count == 0u);

    /* APPROVE requires accepted approval state. */
    transitioned = before_transitioned;
    CHECK(stn_contract_accept_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        NULL,
        &transitioned) == STN_CONTRACT_ARGUMENT);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    /* Required malformed arguments leave the destination untouched. */
    transitioned = before_transitioned;
    CHECK(stn_contract_accept_action(
        NULL,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        &approval_state,
        &transitioned) == STN_CONTRACT_ARGUMENT);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    transitioned = before_transitioned;
    CHECK(stn_contract_accept_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        NULL,
        written_again,
        signing_actor,
        &approval_state,
        &transitioned) == STN_CONTRACT_ARGUMENT);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    transitioned = before_transitioned;
    CHECK(stn_contract_accept_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        NULL,
        &approval_state,
        &transitioned) == STN_CONTRACT_ARGUMENT);
    CHECK(memcmp(
        &transitioned,
        &before_transitioned,
        sizeof(transitioned)) == 0);

    CHECK(stn_contract_accept_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        action_canonical,
        written_again,
        signing_actor,
        &approval_state,
        NULL) == STN_CONTRACT_ARGUMENT);
    CHECK(approval_state.accepted_count == 0u);


    /*
     * Accepted-history Contract reconstruction.
     *
     * Rebuild starts from one accepted REVIEW Contract at sequence 42 and
     * replays accepted Chain history in exact order. The first APPROVE advances
     * to APPROVALS at sequence 43; the second APPROVE advances to ATTESTATION
     * at sequence 44. Both accepted approval keys must be reconstructed.
     */
    transition_contract = contract;
    transition_contract.sequence = UINT64_C(42);
    transition_contract.state = STN_CONTRACT_STATE_REVIEW;

    rebuild_written_one = 0u;
    CHECK(stn_contract_encode(
        &transition_contract,
        rebuild_canonical_one,
        sizeof(rebuild_canonical_one),
        &rebuild_written_one) == STN_CONTRACT_OK);

    CHECK(stn_contract_apply_action(
        &transition_contract,
        STN_CONTRACT_ACTION_APPROVE,
        UINT64_C(43),
        &history_contract) == STN_CONTRACT_OK);

    rebuild_written_two = 0u;
    CHECK(stn_contract_encode(
        &history_contract,
        rebuild_canonical_two,
        sizeof(rebuild_canonical_two),
        &rebuild_written_two) == STN_CONTRACT_OK);

    memset(accepted_history, 0, sizeof(accepted_history));

    accepted_history[0].action = STN_CONTRACT_ACTION_APPROVE;
    accepted_history[0].sequence = UINT64_C(43);
    accepted_history[0].canonical_contract = rebuild_canonical_one;
    accepted_history[0].canonical_contract_length = rebuild_written_one;
    memcpy(
        accepted_history[0].actor,
        signing_actor,
        STN_IDENTITY_PUBLIC_KEY_SIZE);

    accepted_history[1].action = STN_CONTRACT_ACTION_APPROVE;
    accepted_history[1].sequence = UINT64_C(44);
    accepted_history[1].canonical_contract = rebuild_canonical_two;
    accepted_history[1].canonical_contract_length = rebuild_written_two;
    memcpy(
        accepted_history[1].actor,
        other_actor,
        STN_IDENTITY_PUBLIC_KEY_SIZE);

    memset(rebuild_approval_store, 0, sizeof(rebuild_approval_store));
    stn_contract_approval_state_initialize(
        &rebuild_state,
        rebuild_approval_store,
        4u);

    memset(&rebuilt_contract, 0, sizeof(rebuilt_contract));
    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        accepted_history,
        2u,
        &rebuild_state,
        &rebuilt_contract) == STN_CONTRACT_OK);
    CHECK(rebuilt_contract.version == transition_contract.version);
    CHECK(rebuilt_contract.type == transition_contract.type);
    CHECK(rebuilt_contract.sequence == UINT64_C(44));
    CHECK(rebuilt_contract.created_at == transition_contract.created_at);
    CHECK(rebuilt_contract.state == STN_CONTRACT_STATE_ATTESTATION);
    CHECK(rebuilt_contract.participants == transition_contract.participants);
    CHECK(rebuilt_contract.participant_count ==
          transition_contract.participant_count);
    CHECK(rebuilt_contract.participant_bytes ==
          transition_contract.participant_bytes);
    CHECK(rebuilt_contract.terms == transition_contract.terms);
    CHECK(rebuilt_contract.terms_length == transition_contract.terms_length);
    CHECK(rebuild_state.accepted_count == 2u);

    CHECK(stn_contract_approval_key(
        rebuild_canonical_one,
        rebuild_written_one,
        UINT64_C(43),
        signing_actor,
        approval_key) == STN_CONTRACT_OK);
    CHECK(stn_contract_approval_state_check(
        &rebuild_state,
        approval_key) == STN_CONTRACT_DUPLICATE_APPROVAL);

    CHECK(stn_contract_approval_key(
        rebuild_canonical_two,
        rebuild_written_two,
        UINT64_C(44),
        other_actor,
        approval_key_other) == STN_CONTRACT_OK);
    CHECK(stn_contract_approval_state_check(
        &rebuild_state,
        approval_key_other) == STN_CONTRACT_DUPLICATE_APPROVAL);

    /*
     * Repeating reconstruction from the same initial state and accepted history
     * must produce the same Contract and accepted approval state.
     */
    memset(rebuild_approval_store, 0, sizeof(rebuild_approval_store));
    stn_contract_approval_state_initialize(
        &rebuild_state,
        rebuild_approval_store,
        4u);
    memset(&history_contract, 0, sizeof(history_contract));

    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        accepted_history,
        2u,
        &rebuild_state,
        &history_contract) == STN_CONTRACT_OK);
    CHECK(history_contract.version == rebuilt_contract.version);
    CHECK(history_contract.type == rebuilt_contract.type);
    CHECK(history_contract.sequence == rebuilt_contract.sequence);
    CHECK(history_contract.created_at == rebuilt_contract.created_at);
    CHECK(history_contract.state == rebuilt_contract.state);
    CHECK(history_contract.participants == rebuilt_contract.participants);
    CHECK(history_contract.participant_count ==
          rebuilt_contract.participant_count);
    CHECK(history_contract.participant_bytes ==
          rebuilt_contract.participant_bytes);
    CHECK(history_contract.terms == rebuilt_contract.terms);
    CHECK(history_contract.terms_length == rebuilt_contract.terms_length);
    CHECK(rebuild_state.accepted_count == 2u);
    CHECK(stn_contract_approval_state_check(
        &rebuild_state,
        approval_key) == STN_CONTRACT_DUPLICATE_APPROVAL);
    CHECK(stn_contract_approval_state_check(
        &rebuild_state,
        approval_key_other) == STN_CONTRACT_DUPLICATE_APPROVAL);

    /*
     * Empty accepted history reconstructs exactly the supplied initial Contract
     * and an empty accepted approval view.
     */
    memset(rebuild_approval_store, 0xa5, sizeof(rebuild_approval_store));
    stn_contract_approval_state_initialize(
        &rebuild_state,
        rebuild_approval_store,
        4u);
    rebuild_state.accepted_count = 2u;
    memset(&history_contract, 0, sizeof(history_contract));

    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        NULL,
        0u,
        &rebuild_state,
        &history_contract) == STN_CONTRACT_OK);
    CHECK(memcmp(
        &history_contract,
        &transition_contract,
        sizeof(history_contract)) == 0);
    CHECK(rebuild_state.accepted_count == 0u);

    /*
     * Wrong accepted order/sequence fails without publishing reconstructed
     * Contract state or accepted approval count.
     */
    accepted_history[1].sequence = UINT64_C(45);
    memset(rebuild_approval_store, 0, sizeof(rebuild_approval_store));
    stn_contract_approval_state_initialize(
        &rebuild_state,
        rebuild_approval_store,
        4u);
    rebuild_state.accepted_count = 1u;
    memset(rebuild_approval_store, 0x3c, STN_CONTRACT_APPROVAL_KEY_SIZE);
    before_rebuilt_contract = transition_contract;
    before_rebuilt_contract.sequence = UINT64_C(999);
    before_rebuilt_contract.state = STN_CONTRACT_STATE_CLOSED;
    rebuilt_contract = before_rebuilt_contract;

    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        accepted_history,
        2u,
        &rebuild_state,
        &rebuilt_contract) == STN_CONTRACT_SEQUENCE_ERROR);
    CHECK(memcmp(
        &rebuilt_contract,
        &before_rebuilt_contract,
        sizeof(rebuilt_contract)) == 0);
    CHECK(rebuild_state.accepted_count == 1u);

    accepted_history[1].sequence = UINT64_C(44);

    /*
     * Canonical history must describe the exact reconstructed current Contract.
     * A mismatched second entry is rejected and no caller-visible state is
     * published.
     */
    accepted_history[1].canonical_contract = rebuild_canonical_one;
    accepted_history[1].canonical_contract_length = rebuild_written_one;
    rebuild_state.accepted_count = 0u;
    rebuilt_contract = before_rebuilt_contract;

    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        accepted_history,
        2u,
        &rebuild_state,
        &rebuilt_contract) == STN_CONTRACT_LENGTH);
    CHECK(memcmp(
        &rebuilt_contract,
        &before_rebuilt_contract,
        sizeof(rebuilt_contract)) == 0);
    CHECK(rebuild_state.accepted_count == 0u);

    accepted_history[1].canonical_contract = rebuild_canonical_two;
    accepted_history[1].canonical_contract_length = rebuild_written_two;

    /*
     * Duplicate accepted APPROVE history is rejected deterministically.
     * Use the same actor for both entries and the same sequence scope by
     * presenting the first entry twice; the second entry cannot match the
     * reconstructed current Contract and therefore cannot be accepted.
     */
    accepted_history[1] = accepted_history[0];
    rebuild_state.accepted_count = 0u;
    rebuilt_contract = before_rebuilt_contract;

    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        accepted_history,
        2u,
        &rebuild_state,
        &rebuilt_contract) != STN_CONTRACT_OK);
    CHECK(memcmp(
        &rebuilt_contract,
        &before_rebuilt_contract,
        sizeof(rebuilt_contract)) == 0);
    CHECK(rebuild_state.accepted_count == 0u);

    accepted_history[1].action = STN_CONTRACT_ACTION_APPROVE;
    accepted_history[1].sequence = UINT64_C(44);
    accepted_history[1].canonical_contract = rebuild_canonical_two;
    accepted_history[1].canonical_contract_length = rebuild_written_two;
    memcpy(
        accepted_history[1].actor,
        other_actor,
        STN_IDENTITY_PUBLIC_KEY_SIZE);

    /*
     * Insufficient accepted-approval capacity prevents reconstruction from
     * publishing partial Contract or approval state.
     */
    memset(rebuild_approval_store, 0, sizeof(rebuild_approval_store));
    stn_contract_approval_state_initialize(
        &rebuild_state,
        rebuild_approval_store,
        1u);
    rebuilt_contract = before_rebuilt_contract;

    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        accepted_history,
        2u,
        &rebuild_state,
        &rebuilt_contract) == STN_CONTRACT_CAPACITY);
    CHECK(memcmp(
        &rebuilt_contract,
        &before_rebuilt_contract,
        sizeof(rebuilt_contract)) == 0);
    CHECK(rebuild_state.accepted_count == 0u);

    /*
     * Malformed history entries and required arguments fail deterministically.
     */
    accepted_history[0].canonical_contract = NULL;
    rebuilt_contract = before_rebuilt_contract;
    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        accepted_history,
        2u,
        &rebuild_state,
        &rebuilt_contract) == STN_CONTRACT_ARGUMENT);
    CHECK(memcmp(
        &rebuilt_contract,
        &before_rebuilt_contract,
        sizeof(rebuilt_contract)) == 0);
    accepted_history[0].canonical_contract = rebuild_canonical_one;

    CHECK(stn_contract_rebuild_accepted_state(
        NULL,
        accepted_history,
        2u,
        &rebuild_state,
        &rebuilt_contract) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        NULL,
        2u,
        &rebuild_state,
        &rebuilt_contract) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        accepted_history,
        2u,
        NULL,
        &rebuilt_contract) == STN_CONTRACT_ARGUMENT);
    CHECK(stn_contract_rebuild_accepted_state(
        &transition_contract,
        accepted_history,
        2u,
        &rebuild_state,
        NULL) == STN_CONTRACT_ARGUMENT);

    /* Zero-participant / zero-terms contracts are valid structural objects. */
    contract.type = STN_CONTRACT_GENERIC;
    contract.sequence = 0u;
    contract.created_at = 0u;
    contract.state = STN_CONTRACT_STATE_DRAFT;
    contract.participants = NULL;
    contract.participant_count = 0u;
    contract.terms = NULL;
    contract.terms_length = 0u;

    written_again = 999u;
    CHECK(stn_contract_encode(
        &contract,
        canonical_again,
        sizeof(canonical_again),
        &written_again) == STN_CONTRACT_OK);
    CHECK(written_again == STN_CONTRACT_HEADER_SIZE);
    CHECK(stn_contract_decode(
        canonical_again,
        written_again,
        &decoded) == STN_CONTRACT_OK);
    CHECK(decoded.participant_count == 0u);
    CHECK(decoded.participant_bytes == NULL);
    CHECK(decoded.terms_length == 0u);
    CHECK(decoded.terms == NULL);

    printf(
        "Contracts: %u checks, %u failures.\n",
        checks,
        failures);

    return failures != 0u;
}

#ifdef STN_CONTRACT_TEST_MAIN
int main(void)
{
    return test_contract();
}
#endif
