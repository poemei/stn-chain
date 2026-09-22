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
    stn_contract_participant participants[2];
    stn_contract_participant extracted;
    stn_contract_participant before_participant;
    stn_contract contract;
    stn_contract decoded;
    stn_contract before_contract;
    stn_address address_a;
    stn_address address_b;
    uint8_t canonical[STN_CONTRACT_MAX_SIZE];
    uint8_t canonical_again[STN_CONTRACT_MAX_SIZE];
    uint8_t mutated[STN_CONTRACT_MAX_SIZE];
    uint8_t output_guard[STN_CONTRACT_MAX_SIZE];
    uint8_t output_before[STN_CONTRACT_MAX_SIZE];
    char address_text[STN_ADDRESS_TEXT_CAPACITY];
    char address_text_again[STN_ADDRESS_TEXT_CAPACITY];
    size_t written;
    size_t written_again;
    size_t address_written;
    size_t address_written_again;
    size_t expected_size;
    size_t i;

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
