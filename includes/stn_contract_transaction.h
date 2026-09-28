/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_TRANSACTION_H
#define STN_CONTRACT_TRANSACTION_H

#include "stn_contract.h"

#include <stddef.h>
#include <stdint.h>

#define STN_CONTRACT_TX_VERSION 1u
#define STN_CONTRACT_TX_HEADER_SIZE 116u
#define STN_CONTRACT_TX_MAX_SIZE \
    (STN_CONTRACT_TX_HEADER_SIZE + STN_CONTRACT_MAX_SIZE + STN_AUTHORITY_EVIDENCE_SIZE)

/*
 * Canonical Contract Action v1 payload.
 *
 * Wire representation:
 *
 *   2 bytes   version
 *   2 bytes   action
 *   8 bytes   sequence
 *   4 bytes   contract_length
 *   4 bytes   authority_length
 *   32 bytes  actor
 *   64 bytes  signature
 *   variable  canonical Contract v1 bytes
 *   variable  Phase 14 authority evidence
 *
 * All integer fields are big-endian.
 *
 * CREATE is the bootstrap action. It carries zero authority bytes because no
 * accepted Contract exists yet from which a scoped Contract grant can derive.
 * CREATE still carries actor and signature and remains subject to identity,
 * signature, participant, initial-state and consensus validation.
 *
 * [AI-MODIFIED] 2026-09-27
 * A CREATE payload must also describe a DRAFT whose immutable participant set
 * can initialize Phase 18 approval consensus. At least one unique APPROVER is
 * therefore required. This prevents a consensus-impossible CREATE from being
 * represented as admissible pending work.
 * [HUMAN-REVIEW-REQUIRED]
 *
 * Every action after CREATE carries exactly STN_AUTHORITY_EVIDENCE_SIZE bytes.
 *
 * This transport layer does not establish signature validity, scoped authority,
 * accepted history, duplicate approval state or consensus acceptance. CREATE's
 * deterministic approval-set viability is checked because it is immutable and
 * already knowable before pending admission.
 */
typedef struct stn_contract_transaction {
    uint16_t version;
    uint16_t action;
    uint64_t sequence;
    const uint8_t *canonical_contract;
    uint32_t canonical_contract_length;
    uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE];
    uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE];
    const uint8_t *authority_evidence;
    uint32_t authority_evidence_length;
} stn_contract_transaction;

stn_contract_status stn_contract_transaction_decode(
    const uint8_t *input,
    size_t input_length,
    stn_contract_transaction *transaction);

stn_contract_status stn_contract_transaction_encode(
    const stn_contract_transaction *transaction,
    uint8_t *output,
    size_t capacity,
    size_t *written);

stn_contract_status stn_contract_transaction_validate_structure(
    const uint8_t *input,
    size_t input_length);

#endif
