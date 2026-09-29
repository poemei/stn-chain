/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_pending.h"
#include "stn_lifecycle.h"
#include "stn_sha256.h"
#include "stn_wire_internal.h"
#include "stn_share.h"
#include "stn_compensation.h"
#include "stn_issuance.h"
#include "stn_issuance_binding.h"
#include "stn_transfer_envelope.h"
#include "stn_transfer_envelope_authorization.h"
#include "stn_contract_transaction.h"
#include <stdlib.h>
#include <string.h>

void stn_pending_init(stn_pending *p)
{
    if (p != NULL)
    {
        memset(p, 0, sizeof(*p));
    }
}
size_t stn_pending_count(const stn_pending *p)
{
    return p != NULL ? p->count : 0;
}
static size_t find_id(const stn_pending *p, const uint8_t id[32])
{
    size_t at = 0;
    while (at < p->count && memcmp(p->entries[at].id, id, 32) < 0)
    {
        ++at;
    }
    return at;
}

static stn_data_status pending_share_for_issuance(
    const stn_pending *p, const stn_issuance_record *issuance,
    const stn_compensation_state *compensation, int *found)
{
    size_t i;
    if (p == NULL || issuance == NULL || compensation == NULL || found == NULL)
        return STN_DATA_ARGUMENT;
    *found = 0;
    for (i = 0u; i < p->count; ++i)
    {
        stn_transaction tx;
        if (stn_transaction_decode(p->entries[i].transaction,
                                   p->entries[i].length, &tx) != STN_DATA_OK)
            return STN_DATA_CONTENT;
        if (tx.type == STN_TX_SHARE_EVIDENCE)
        {
            stn_share_evidence share;
            if (stn_share_decode(tx.record_bytes, tx.record_length, &share) !=
                STN_DATA_OK)
                return STN_DATA_CONTENT;
            if (stn_issuance_bind_share(issuance, &share, compensation) ==
                STN_DATA_OK)
            {
                *found = 1;
                return STN_DATA_OK;
            }
        }
    }
    return STN_DATA_OK;
}

static stn_data_status
history_issuance_state(const stn_storage_view *v,
                       const stn_issuance_record *issuance,
                       const stn_compensation_state *compensation,
                       int *share_found, int *issuance_found)
{
    size_t i, j;
    if (v == NULL || issuance == NULL || compensation == NULL ||
        share_found == NULL || issuance_found == NULL ||
        (v->count != 0u && v->blocks == NULL))
        return STN_DATA_ARGUMENT;
    *share_found = 0;
    *issuance_found = 0;
    for (i = 0u; i < v->count; ++i)
    {
        stn_block block;
        size_t offset = 0u;
        if (stn_block_decode(v->blocks[i].bytes, v->blocks[i].length, &block) !=
            STN_DATA_OK)
            return STN_DATA_CONTENT;
        for (j = 0u; j < block.header.transaction_count; ++j)
        {
            uint32_t n;
            stn_transaction tx;
            if (offset + 4u > block.header.body_length)
                return STN_DATA_CONTENT;
            n = (uint32_t)stn_wire_read(block.body + offset, 4u);
            offset += 4u;
            if (n > block.header.body_length - offset ||
                stn_transaction_decode(block.body + offset, n, &tx) !=
                    STN_DATA_OK)
                return STN_DATA_CONTENT;
            if (tx.type == STN_TX_SHARE_EVIDENCE)
            {
                stn_share_evidence share;
                if (stn_share_decode(tx.record_bytes, tx.record_length,
                                     &share) != STN_DATA_OK)
                    return STN_DATA_CONTENT;
                if (stn_issuance_bind_share(issuance, &share, compensation) ==
                    STN_DATA_OK)
                    *share_found = 1;
            }
            else if (tx.type == STN_TX_ISSUANCE)
            {
                stn_issuance_record prior;
                if (stn_issuance_decode(tx.record_bytes, tx.record_length,
                                        &prior) != STN_DATA_OK)
                    return STN_DATA_CONTENT;
                if (prior.reason == issuance->reason &&
                    memcmp(prior.evidence_id, issuance->evidence_id,
                           STN_ISSUANCE_EVIDENCE_ID_SIZE) == 0)
                    *issuance_found = 1;
            }
            offset += n;
        }
    }
    return STN_DATA_OK;
}

stn_pending_result stn_pending_insert(stn_pending *p, const uint8_t *bytes,
                                      size_t length,
                                      const stn_hash_provider *hash,
                                      uint8_t id[32])
{
    stn_pending_entry entry = {0};
    stn_transaction tx;
    stn_record record;
    size_t at;
    uint8_t statement[256];
    size_t written = 0;
    if (p == NULL || id == NULL)
        return STN_PENDING_INVALID;
    if (stn_transaction_decode(bytes, length, &tx) != STN_DATA_OK)
        return STN_PENDING_INVALID;
    if (tx.type == STN_TX_PUBLICATION)
    {
        if (stn_record_decode(tx.record_bytes, tx.record_length, &record) !=
            STN_RECORD_OK)
            return STN_PENDING_INVALID;
        memcpy(entry.signer, record.signer_public_key, 32);
        memcpy(entry.nonce, record.nonce, 32);
    }
    else if (tx.type == STN_TX_CONTRACT_ACTION)
    {
        stn_contract_transaction contract_tx;
        if (stn_contract_transaction_decode(tx.record_bytes, tx.record_length,
                                            &contract_tx) != STN_CONTRACT_OK)
            return STN_PENDING_INVALID;
        memcpy(entry.signer, contract_tx.actor, 32);
    }
    else if (tx.type == STN_TX_SHARE_EVIDENCE)
    {
        stn_share_evidence share;
        size_t i;
        if (stn_share_decode(tx.record_bytes, tx.record_length, &share) !=
            STN_DATA_OK)
            return STN_PENDING_INVALID;
        memcpy(entry.signer, share.miner.identifier, 32);
        memcpy(entry.nonce, share.work_id, 24);
        for (i = 0; i < 8u; ++i)
            entry.nonce[24u + i] = (uint8_t)(share.nonce >> (56u - 8u * i));
    }
    else if (tx.type == STN_TX_COMPENSATION_DESTINATION)
    {
        stn_compensation_destination destination;
        if (stn_compensation_destination_decode(
                tx.record_bytes, tx.record_length, &destination) != STN_DATA_OK)
            return STN_PENDING_INVALID;
        memcpy(entry.signer, destination.mining_identity.identifier, 32);
        memcpy(entry.nonce, destination.wallet.identifier, 32);
    }
    else if (tx.type == STN_TX_ISSUANCE)
    {
        stn_issuance_record issuance;
        if (stn_issuance_decode(tx.record_bytes, tx.record_length, &issuance) !=
            STN_DATA_OK)
            return STN_PENDING_INVALID;
        memcpy(entry.signer, issuance.destination.mining_identity.identifier,
               32);
        memcpy(entry.nonce, issuance.evidence_id, 32);
    }
    else if (tx.type == STN_TX_TRANSFER)
    {
        stn_transfer_envelope envelope;
        if (stn_transfer_envelope_decode(tx.record_bytes, tx.record_length,
                                         &envelope) != STN_DATA_OK)
            return STN_PENDING_INVALID;
        memcpy(entry.signer, envelope.controller, 32);
        memcpy(entry.nonce, envelope.nonce, 32);
    }
    else
    {
        const uint8_t *signer = tx.record_bytes + 1;
        if (tx.type == STN_TX_AUTHORITY_GRANT)
        {
            if (stn_authority_grant_statement(
                    tx.record_bytes + 1, tx.record_bytes + 33, statement,
                    sizeof(statement), &written) != STN_AUTHORITY_VALID_GRANT)
                return STN_PENDING_INVALID;
        }
        else if (tx.type == STN_TX_AUTHORITY_REVOKE)
        {
            if (stn_authority_revocation_statement(
                    tx.record_bytes + 1, tx.record_bytes + 33, statement,
                    sizeof(statement),
                    &written) != STN_AUTHORITY_VALID_REVOCATION)
                return STN_PENDING_INVALID;
        }
        else if (tx.type == STN_TX_IDENTITY_ROTATE)
        {
            if (stn_identity_rotation_statement(tx.record_bytes + 1,
                                                tx.record_bytes + 33, statement,
                                                sizeof(statement), &written) !=
                STN_AUTHORITY_VALID_ROTATION)
                return STN_PENDING_INVALID;
        }
        else
            return STN_PENDING_INVALID;
        if (stn_lifecycle_replay_nonce(tx.type, statement, written, hash,
                                       entry.nonce) != STN_LIFECYCLE_OK)
            return STN_PENDING_PROVIDER;
        memcpy(entry.signer, signer, 32);
    }
    if (stn_transaction_id(bytes, length, hash, entry.id) != STN_DATA_OK)
        return STN_PENDING_PROVIDER;
    at = find_id(p, entry.id);
    if (at < p->count && memcmp(p->entries[at].id, entry.id, 32) == 0)
        return STN_PENDING_DUPLICATE;
    if (p->count >= STN_PENDING_MAX_ENTRIES ||
        length > STN_PENDING_MAX_BYTES - p->bytes)
        return STN_PENDING_CAPACITY;
    entry.transaction = malloc(length);
    if (entry.transaction == NULL)
        return STN_PENDING_CAPACITY;
    memcpy(entry.transaction, bytes, length);
    entry.length = length;
    memmove(p->entries + at + 1, p->entries + at,
            (p->count - at) * sizeof(entry));
    p->entries[at] = entry;
    ++p->count;
    p->bytes += length;
    memcpy(id, entry.id, 32);
    return STN_PENDING_ACCEPTED;
}

stn_pending_result stn_pending_lookup(const stn_pending *p,
                                      const uint8_t id[32], uint8_t *output,
                                      size_t capacity, size_t *written)
{
    size_t at;
    if (written != NULL)
        *written = 0;
    if (p == NULL || id == NULL || output == NULL || written == NULL)
        return STN_PENDING_INVALID;
    at = find_id(p, id);
    if (at == p->count || memcmp(p->entries[at].id, id, 32) != 0)
        return STN_PENDING_NOT_FOUND;
    if (capacity < p->entries[at].length)
        return STN_PENDING_CAPACITY;
    memcpy(output, p->entries[at].transaction, p->entries[at].length);
    *written = p->entries[at].length;
    return STN_PENDING_ACCEPTED;
}
stn_pending_result stn_pending_remove(stn_pending *p, const uint8_t id[32])
{
    size_t at;
    if (p == NULL || id == NULL)
        return STN_PENDING_INVALID;
    at = find_id(p, id);
    if (at == p->count || memcmp(p->entries[at].id, id, 32) != 0)
        return STN_PENDING_NOT_FOUND;
    p->bytes -= p->entries[at].length;
    free(p->entries[at].transaction);
    memmove(p->entries + at, p->entries + at + 1,
            (p->count - at - 1) * sizeof(p->entries[0]));
    --p->count;
    memset(p->entries + p->count, 0, sizeof(p->entries[0]));
    return STN_PENDING_ACCEPTED;
}
stn_pending_result stn_pending_enumerate(const stn_pending *p, size_t start,
                                         uint8_t (*ids)[32], size_t capacity,
                                         size_t *written)
{
    size_t i, n;
    if (written != NULL)
        *written = 0;
    if (p == NULL || written == NULL || (capacity != 0 && ids == NULL))
        return STN_PENDING_INVALID;
    n = start < p->count ? p->count - start : 0;
    if (n > capacity)
        n = capacity;
    for (i = 0; i < n; ++i)
        memcpy(ids[i], p->entries[start + i].id, 32);
    *written = n;
    return STN_PENDING_ACCEPTED;
}
void stn_pending_clear(stn_pending *p)
{
    size_t i;
    if (p == NULL)
        return;
    for (i = 0; i < p->count; ++i)
        free(p->entries[i].transaction);
    memset(p, 0, sizeof(*p));
}
static int same_nonce(const stn_pending_entry *e, const uint8_t signer[32],
                      const uint8_t nonce[32])
{
    return memcmp(e->signer, signer, 32) == 0 &&
           memcmp(e->nonce, nonce, 32) == 0;
}

static stn_data_status scan(const stn_pending *p, const stn_storage_view *v,
                            uint8_t *mask, int replay,
                            const stn_hash_provider *hash)
{
    size_t i, j, k, offset;
    stn_block b;
    stn_transaction tx;
    stn_record r;
    uint8_t id[32], signer[32], nonce[32], statement[256];
    size_t written;
    int lifecycle;
    if (p == NULL || v == NULL || mask == NULL ||
        (v->count != 0 && v->blocks == NULL))
        return STN_DATA_ARGUMENT;
    memset(mask, 0, STN_PENDING_MAX_ENTRIES);
    for (i = 0; i < v->count; ++i)
    {
        if (stn_block_decode(v->blocks[i].bytes, v->blocks[i].length, &b) !=
            STN_DATA_OK)
            return STN_DATA_CONTENT;
        offset = 0;
        for (j = 0; j < b.header.transaction_count; ++j)
        {
            size_t n = (size_t)stn_wire_read(b.body + offset, 4);
            if (stn_transaction_decode(b.body + offset + 4, n, &tx) !=
                STN_DATA_OK)
                return STN_DATA_CONTENT;
            lifecycle = 1;
            if (tx.type == STN_TX_CONTRACT_ACTION)
            {
                stn_contract_transaction contract_tx;
                if (stn_contract_transaction_decode(
                        tx.record_bytes, tx.record_length, &contract_tx) !=
                    STN_CONTRACT_OK)
                    return STN_DATA_CONTENT;
                memcpy(signer, contract_tx.actor, 32);
            }
            else if (tx.type == STN_TX_SHARE_EVIDENCE)
            {
                stn_share_evidence share;
                size_t z;
                if (stn_share_decode(tx.record_bytes, tx.record_length,
                                     &share) != STN_DATA_OK)
                    return STN_DATA_CONTENT;
                memcpy(signer, share.miner.identifier, 32);
                memcpy(nonce, share.work_id, 24);
                for (z = 0; z < 8u; ++z)
                    nonce[24u + z] = (uint8_t)(share.nonce >> (56u - 8u * z));
                lifecycle = 0;
            }
            else if (tx.type == STN_TX_COMPENSATION_DESTINATION)
            {
                stn_compensation_destination destination;
                if (stn_compensation_destination_decode(
                        tx.record_bytes, tx.record_length, &destination) !=
                    STN_DATA_OK)
                    return STN_DATA_CONTENT;
                memcpy(signer, destination.mining_identity.identifier, 32);
                memcpy(nonce, destination.wallet.identifier, 32);
                lifecycle = 0;
            }
            else if (tx.type == STN_TX_ISSUANCE)
            {
                stn_issuance_record issuance;
                if (stn_issuance_decode(tx.record_bytes, tx.record_length,
                                        &issuance) != STN_DATA_OK)
                    return STN_DATA_CONTENT;
                memcpy(signer, issuance.destination.mining_identity.identifier,
                       32);
                memcpy(nonce, issuance.evidence_id, 32);
                lifecycle = 0;
            }
            else if (tx.type == STN_TX_TRANSFER)
            {
                stn_transfer_envelope envelope;
                if (stn_transfer_envelope_decode(tx.record_bytes,
                                                 tx.record_length,
                                                 &envelope) != STN_DATA_OK)
                    return STN_DATA_CONTENT;
                memcpy(signer, envelope.controller, 32);
                memcpy(nonce, envelope.nonce, 32);
                lifecycle = 0;
            }
            else if (tx.type == STN_TX_PUBLICATION)
            {
                if (stn_record_decode(tx.record_bytes, tx.record_length, &r) !=
                    STN_RECORD_OK)
                    return STN_DATA_CONTENT;
                memcpy(signer, r.signer_public_key, 32);
                memcpy(nonce, r.nonce, 32);
                lifecycle = 0;
            }
            else
            {
                memcpy(signer, tx.record_bytes + 1, 32);
                if (hash != NULL)
                {
                    if (tx.type == STN_TX_AUTHORITY_GRANT)
                    {
                        if (stn_authority_grant_statement(
                                tx.record_bytes + 1, tx.record_bytes + 33,
                                statement, sizeof(statement),
                                &written) != STN_AUTHORITY_VALID_GRANT)
                            return STN_DATA_CONTENT;
                    }
                    else if (tx.type == STN_TX_AUTHORITY_REVOKE)
                    {
                        if (stn_authority_revocation_statement(
                                tx.record_bytes + 1, tx.record_bytes + 33,
                                statement, sizeof(statement),
                                &written) != STN_AUTHORITY_VALID_REVOCATION)
                            return STN_DATA_CONTENT;
                    }
                    else if (tx.type == STN_TX_IDENTITY_ROTATE)
                    {
                        if (stn_identity_rotation_statement(
                                tx.record_bytes + 1, tx.record_bytes + 33,
                                statement, sizeof(statement),
                                &written) != STN_AUTHORITY_VALID_ROTATION)
                            return STN_DATA_CONTENT;
                    }
                    else
                        return STN_DATA_CONTENT;
                    if (stn_lifecycle_replay_nonce(tx.type, statement, written,
                                                   hash,
                                                   nonce) != STN_LIFECYCLE_OK)
                        return STN_DATA_CONTENT;
                }
            }
            if (!replay && stn_transaction_id(b.body + offset + 4, n, hash,
                                              id) != STN_DATA_OK)
                return STN_DATA_PROVIDER_ERROR;
            for (k = 0; k < p->count; ++k)
            {
                if (replay ? (lifecycle
                                  ? (p->entries[k].length == n &&
                                     memcmp(p->entries[k].transaction,
                                            b.body + offset, n) == 0)
                                  : same_nonce(&p->entries[k], signer, nonce))
                           : memcmp(p->entries[k].id, id, 32) == 0)
                    mask[k] = 1;
            }
            offset += 4 + n;
        }
    }
    return STN_DATA_OK;
}

stn_data_status stn_pending_inclusions(const stn_pending *p,
                                       const stn_storage_view *v,
                                       const stn_hash_provider *hash,
                                       uint8_t remove[STN_PENDING_MAX_ENTRIES])
{
    uint8_t mask[STN_PENDING_MAX_ENTRIES];
    stn_data_status s;
    if (remove == NULL)
        return STN_DATA_ARGUMENT;
    s = scan(p, v, mask, 0, hash);
    if (s == STN_DATA_OK)
        memcpy(remove, mask, sizeof(mask));
    return s;
}
void stn_pending_prune(stn_pending *p,
                       const uint8_t remove[STN_PENDING_MAX_ENTRIES])
{
    size_t i, n = 0;
    for (i = 0; i < p->count; ++i)
    {
        if (remove[i])
        {
            p->bytes -= p->entries[i].length;
            free(p->entries[i].transaction);
        }
        else
            p->entries[n++] = p->entries[i];
    }
    memset(p->entries + n, 0, (p->count - n) * sizeof(p->entries[0]));
    p->count = n;
}
static stn_pending_result disposition(const stn_validation_report *r)
{
    if (r->acceptance == STN_ACCEPTANCE_UNRESOLVED)
        return STN_PENDING_UNAVAILABLE;
    if (r->acceptance == STN_ACCEPTANCE_ERROR)
        return STN_PENDING_PROVIDER;
    if (r->acceptance == STN_ACCEPTANCE_UNDER_CONTEXT)
        return STN_PENDING_ACCEPTED;
    if (r->envelope_error == STN_RECORD_UNSUPPORTED)
        return STN_PENDING_UNSUPPORTED;
    if (r->payload_error == STN_SENTINEL_INTELLIGENCE_VERSION_ERROR)
        return STN_PENDING_UNSUPPORTED;
    if (r->network == STN_STAGE_REJECT)
        return STN_PENDING_NETWORK;
    if (r->time == STN_STAGE_REJECT)
        return STN_PENDING_TIME;
    if (r->signature == STN_STAGE_REJECT)
        return STN_PENDING_SIGNATURE;
    if (r->authority == STN_STAGE_REJECT)
        return STN_PENDING_AUTHORITY;
    if (r->replay == STN_STAGE_REJECT)
        return STN_PENDING_REPLAY;
    return STN_PENDING_INVALID;
}
static int production_next(const stn_storage_view *active)
{
    return active != NULL && active->state.publication_activation_height != 0 &&
           (active->state.height == UINT64_MAX ||
            active->state.height + 1 >=
                active->state.publication_activation_height);
}

stn_pending_result
stn_pending_admit(stn_pending *p, const uint8_t *record, size_t length,
                  const stn_validation_context *c,
                  const stn_storage_view *active, const stn_hash_provider *hash,
                  stn_validation_report *report, uint8_t id[32])
{
    stn_pending_result result;
    stn_pending_entry e = {0};
    stn_record r;
    stn_transaction tx;
    uint8_t encoded[STN_TX_MAX_SIZE], mask[STN_PENDING_MAX_ENTRIES];
    size_t n, i;
    stn_pending probe = {0};
    if (report == NULL || id == NULL)
        return STN_PENDING_PROVIDER;
    memset(report, 0, sizeof(*report));
    memset(id, 0, 32);
    if (p == NULL || active == NULL || hash == NULL)
    {
        report->acceptance = STN_ACCEPTANCE_ERROR;
        return STN_PENDING_PROVIDER;
    }
    if (production_next(active))
    {
        stn_lifecycle_result checked = stn_lifecycle_check_publication(
            active->state.lifecycle, record, length, hash);
        if (checked == STN_LIFECYCLE_PROVIDER ||
            checked == STN_LIFECYCLE_CAPACITY)
        {
            report->acceptance = STN_ACCEPTANCE_ERROR;
            return STN_PENDING_PROVIDER;
        }
        if (checked != STN_LIFECYCLE_OK)
        {
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return checked == STN_LIFECYCLE_REPLAY    ? STN_PENDING_REPLAY
                   : checked == STN_LIFECYCLE_INVALID ? STN_PENDING_AUTHORITY
                                                      : STN_PENDING_INVALID;
        }
        report->acceptance = STN_ACCEPTANCE_UNDER_CONTEXT;
    }
    else
    {
        if (c == NULL)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        if (memcmp(c->expected_network, active->state.network_id, 32) != 0)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        *report = stn_validate_intelligence_record(record, length, c);
        result = disposition(report);
        if (result != STN_PENDING_ACCEPTED)
            return result;
    }
    if (stn_record_decode(record, length, &r) != STN_RECORD_OK)
        return STN_PENDING_INVALID;
    if (memcmp(r.network_id, active->state.network_id, 32) != 0)
    {
        report->acceptance = STN_ACCEPTANCE_REJECTED;
        report->network = STN_STAGE_REJECT;
        return STN_PENDING_NETWORK;
    }
    tx.version = 1;
    tx.type = STN_TX_PUBLICATION;
    tx.record_bytes = record;
    tx.record_length = (uint32_t)length;
    if (stn_transaction_encode(&tx, encoded, sizeof(encoded), &n) !=
            STN_DATA_OK ||
        stn_transaction_id(encoded, n, hash, e.id) != STN_DATA_OK)
        return STN_PENDING_PROVIDER;
    memcpy(id, e.id, 32);
    memcpy(e.signer, r.signer_public_key, 32);
    memcpy(e.nonce, r.nonce, 32);
    e.length = n;
    i = find_id(p, e.id);
    if (i < p->count && memcmp(p->entries[i].id, e.id, 32) == 0)
        return STN_PENDING_DUPLICATE;
    for (i = 0; i < p->count; ++i)
        if (memcmp(p->entries[i].signer, e.signer, 32) == 0 &&
            memcmp(p->entries[i].nonce, e.nonce, 32) == 0)
            return STN_PENDING_REPLAY;
    probe.entries[0] = e;
    probe.count = 1;
    if (scan(&probe, active, mask, 1, hash) != STN_DATA_OK)
        return STN_PENDING_PROVIDER;
    if (mask[0])
        return STN_PENDING_REPLAY;
    return stn_pending_insert(p, encoded, n, hash, id);
}

stn_pending_result
stn_pending_admit_transaction(stn_pending *p, const uint8_t *bytes,
                              size_t length, const stn_validation_context *c,
                              const stn_storage_view *active,
                              const stn_hash_provider *hash,
                              stn_validation_report *report, uint8_t id[32])
{
    stn_transaction tx;
    stn_data_status status;
    stn_pending_result lifecycle_result;
    if (report == NULL || id == NULL)
        return STN_PENDING_PROVIDER;
    memset(report, 0, sizeof(*report));
    memset(id, 0, 32);
    status = stn_transaction_decode(bytes, length, &tx);
    if (status != STN_DATA_OK)
    {
        report->structure = STN_STAGE_REJECT;
        report->acceptance = STN_ACCEPTANCE_REJECTED;
        if (status == STN_DATA_VERSION || status == STN_DATA_TYPE)
            return STN_PENDING_UNSUPPORTED;
        if (status == STN_DATA_CONTENT)
        {
            stn_record record;
            report->envelope_error =
                stn_record_decode(bytes + STN_TX_HEADER_SIZE,
                                  length - STN_TX_HEADER_SIZE, &record);
            if (report->envelope_error == STN_RECORD_UNSUPPORTED)
                return STN_PENDING_UNSUPPORTED;
        }
        return STN_PENDING_INVALID;
    }
    if (tx.type == STN_TX_CONTRACT_ACTION)
    {
        if (p == NULL || active == NULL || hash == NULL)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        {
            stn_contract_snapshot *projection;
            if (active->state.contracts == NULL || active->state.lifecycle == NULL)
                return STN_PENDING_UNAVAILABLE;
            projection = stn_contract_snapshot_clone(active->state.contracts);
            if (projection == NULL)
                return STN_PENDING_CAPACITY;
            status = stn_chain_contract_apply_transaction(projection,
                active->state.lifecycle, &tx, hash);
            stn_contract_snapshot_release(projection);
            if (status != STN_DATA_OK) {
                report->acceptance = STN_ACCEPTANCE_REJECTED;
                return status == STN_DATA_CAPACITY ? STN_PENDING_CAPACITY :
                    status == STN_DATA_PROVIDER_ERROR ? STN_PENDING_PROVIDER :
                    STN_PENDING_INVALID;
            }
        }
        lifecycle_result = stn_pending_insert(p, bytes, length, hash, id);
        if (lifecycle_result == STN_PENDING_ACCEPTED)
        {
            report->structure = STN_STAGE_PASS;
            report->acceptance = STN_ACCEPTANCE_UNDER_CONTEXT;
            return lifecycle_result;
        }
        report->acceptance = STN_ACCEPTANCE_REJECTED;
        return lifecycle_result;
    }
    if (tx.type == STN_TX_SHARE_EVIDENCE)
    {
        stn_share_evidence share;
        stn_share_replay_result replay;
        if (p == NULL || active == NULL || hash == NULL ||
            active->state.shares == NULL)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        if (stn_share_decode(tx.record_bytes, tx.record_length, &share) !=
            STN_DATA_OK)
        {
            report->structure = STN_STAGE_REJECT;
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_INVALID;
        }
        replay = stn_share_replay_check(active->state.shares, &share);
        if (replay == STN_SHARE_REPLAY_DUPLICATE)
        {
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_REPLAY;
        }
        if (replay != STN_SHARE_REPLAY_FRESH)
        {
            report->acceptance = STN_ACCEPTANCE_ERROR;
            return replay == STN_SHARE_REPLAY_CAPACITY ? STN_PENDING_CAPACITY
                                                       : STN_PENDING_PROVIDER;
        }
        lifecycle_result = stn_pending_insert(p, bytes, length, hash, id);
        if (lifecycle_result == STN_PENDING_ACCEPTED)
        {
            report->acceptance = STN_ACCEPTANCE_UNDER_CONTEXT;
            return lifecycle_result;
        }
        report->acceptance = STN_ACCEPTANCE_REJECTED;
        return lifecycle_result;
    }
    if (tx.type == STN_TX_ISSUANCE)
    {
        stn_issuance_record issuance;
        stn_address wallet;
        int accepted_share = 0, accepted_issuance = 0, pending_share = 0;
        if (p == NULL || active == NULL || hash == NULL ||
            active->state.compensation == NULL)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        if (stn_issuance_decode(tx.record_bytes, tx.record_length, &issuance) !=
            STN_DATA_OK)
        {
            report->structure = STN_STAGE_REJECT;
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_INVALID;
        }
        if (issuance.reason != STN_ISSUANCE_REASON_SHARE)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        status = stn_compensation_state_lookup(
            active->state.compensation, &issuance.destination.mining_identity,
            &wallet);
        if (status == STN_DATA_UNRESOLVED)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        if (status != STN_DATA_OK)
        {
            report->acceptance = STN_ACCEPTANCE_ERROR;
            return status == STN_DATA_CAPACITY ? STN_PENDING_CAPACITY
                                               : STN_PENDING_PROVIDER;
        }
        if (wallet.type != STN_ADDRESS_WALLET ||
            memcmp(wallet.identifier, issuance.destination.wallet.identifier,
                   STN_ADDRESS_ID_SIZE) != 0)
        {
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_REPLAY;
        }
        status = history_issuance_state(active, &issuance,
                                        active->state.compensation,
                                        &accepted_share, &accepted_issuance);
        if (status != STN_DATA_OK)
        {
            report->acceptance = STN_ACCEPTANCE_ERROR;
            return STN_PENDING_PROVIDER;
        }
        if (accepted_issuance)
        {
            report->replay = STN_STAGE_REJECT;
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_REPLAY;
        }
        if (!accepted_share)
        {
            status = pending_share_for_issuance(
                p, &issuance, active->state.compensation, &pending_share);
            if (status != STN_DATA_OK)
            {
                report->acceptance = STN_ACCEPTANCE_ERROR;
                return STN_PENDING_PROVIDER;
            }
            if (!pending_share)
            {
                report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
                return STN_PENDING_UNAVAILABLE;
            }
        }
        lifecycle_result = stn_pending_insert(p, bytes, length, hash, id);
        if (lifecycle_result == STN_PENDING_ACCEPTED)
        {
            report->replay = STN_STAGE_PASS;
            report->acceptance = STN_ACCEPTANCE_UNDER_CONTEXT;
            return lifecycle_result;
        }
        report->acceptance = STN_ACCEPTANCE_REJECTED;
        return lifecycle_result;
    }
    if (tx.type == STN_TX_COMPENSATION_DESTINATION)
    {
        stn_compensation_destination destination;
        stn_address wallet;
        stn_data_status mapping;
        if (p == NULL || active == NULL || hash == NULL ||
            active->state.compensation == NULL)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        if (stn_compensation_destination_decode(
                tx.record_bytes, tx.record_length, &destination) != STN_DATA_OK)
        {
            report->structure = STN_STAGE_REJECT;
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_INVALID;
        }
        mapping = stn_compensation_state_lookup(
            active->state.compensation, &destination.mining_identity, &wallet);
        if (mapping == STN_DATA_OK &&
            memcmp(wallet.identifier, destination.wallet.identifier,
                   STN_ADDRESS_ID_SIZE) != 0)
        {
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_REPLAY;
        }
        if (mapping != STN_DATA_OK && mapping != STN_DATA_UNRESOLVED)
        {
            report->acceptance = STN_ACCEPTANCE_ERROR;
            return mapping == STN_DATA_CAPACITY ? STN_PENDING_CAPACITY
                                                : STN_PENDING_PROVIDER;
        }
        lifecycle_result = stn_pending_insert(p, bytes, length, hash, id);
        if (lifecycle_result == STN_PENDING_ACCEPTED)
        {
            report->acceptance = STN_ACCEPTANCE_UNDER_CONTEXT;
            return lifecycle_result;
        }
        report->acceptance = STN_ACCEPTANCE_REJECTED;
        return lifecycle_result;
    }
    /* [AI:GPT-6 | 2026-09-28 01:09:15 UTC] */
    /* Read accepted transfer replay state; admission never consumes a nonce. */
    if (tx.type == STN_TX_TRANSFER)
    {
        stn_transfer_envelope envelope;
        uint64_t balance = 0;
        size_t i;
        if (p == NULL || active == NULL || hash == NULL ||
            active->state.economy == NULL)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        if (stn_transfer_envelope_decode(tx.record_bytes, tx.record_length,
                                         &envelope) != STN_DATA_OK)
        {
            report->structure = STN_STAGE_REJECT;
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_INVALID;
        }
        if (stn_transfer_envelope_authorization_verify(&envelope) !=
            STN_DATA_OK)
        {
            report->signature = STN_STAGE_REJECT;
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_SIGNATURE;
        }
        report->signature = STN_STAGE_PASS;
        if (stn_economic_state_balance(active->state.economy,
                                       &envelope.transfer.source,
                                       &balance) != STN_DATA_OK)
        {
            report->acceptance = STN_ACCEPTANCE_ERROR;
            return STN_PENDING_PROVIDER;
        }
        if (balance < envelope.transfer.units)
        {
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_INVALID;
        }
        if (stn_transaction_id(bytes, length, hash, id) != STN_DATA_OK)
        {
            report->acceptance = STN_ACCEPTANCE_ERROR;
            return STN_PENDING_PROVIDER;
        }
        i = find_id(p, id);
        if (i < p->count && memcmp(p->entries[i].id, id, 32) == 0)
        {
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_DUPLICATE;
        }
        for (i = 0; i < p->count; ++i)
            if (same_nonce(&p->entries[i], envelope.controller, envelope.nonce))
            {
                report->replay = STN_STAGE_REJECT;
                report->acceptance = STN_ACCEPTANCE_REJECTED;
                return STN_PENDING_REPLAY;
            }
        status = stn_chain_transfer_replay_check(&active->state, &envelope);
        if (status == STN_DATA_UNRESOLVED)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        if (status != STN_DATA_OK && status != STN_DATA_DUPLICATE)
        {
            report->acceptance = STN_ACCEPTANCE_ERROR;
            return STN_PENDING_PROVIDER;
        }
        if (status == STN_DATA_DUPLICATE)
        {
            report->replay = STN_STAGE_REJECT;
            report->acceptance = STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_REPLAY;
        }
        report->replay = STN_STAGE_PASS;
        lifecycle_result = stn_pending_insert(p, bytes, length, hash, id);
        if (lifecycle_result == STN_PENDING_ACCEPTED)
        {
            report->acceptance = STN_ACCEPTANCE_UNDER_CONTEXT;
            return lifecycle_result;
        }
        report->acceptance = STN_ACCEPTANCE_REJECTED;
        return lifecycle_result;
    }
    /* [End AI:GPT-6] */
    if (tx.type != STN_TX_PUBLICATION)
    {
        if (c == NULL || active == NULL ||
            memcmp(c->expected_network, active->state.network_id, 32) != 0)
        {
            report->acceptance = STN_ACCEPTANCE_UNRESOLVED;
            return STN_PENDING_UNAVAILABLE;
        }
        lifecycle_result = stn_pending_insert(p, bytes, length, hash, id);
        if (lifecycle_result == STN_PENDING_ACCEPTED)
        {
            report->acceptance = STN_ACCEPTANCE_UNDER_CONTEXT;
            return lifecycle_result;
        }
        report->acceptance = STN_ACCEPTANCE_REJECTED;
        return lifecycle_result;
    }
    return stn_pending_admit(p, tx.record_bytes, tx.record_length, c, active,
                             hash, report, id);
}

static stn_data_status assemble_projected(stn_contract_snapshot **contracts,
                                     const stn_pending *p,
                                     const stn_validation_context *c,
                                     const stn_storage_view *active,
                                     uint8_t *body, size_t capacity,
                                     size_t *written, uint32_t *count)
{
    uint8_t replay[STN_PENDING_MAX_ENTRIES], ids[STN_PENDING_MAX_ENTRIES][32];
    stn_transaction_span selected[STN_BLOCK_MAX_TRANSACTIONS];
    uint32_t n = 0;
    size_t i, total = 0, available;
    if (written != NULL)
        *written = 0;
    if (count != NULL)
        *count = 0;
    if (p == NULL || body == NULL || written == NULL || count == NULL)
        return STN_DATA_ARGUMENT;
    if (p->count == 0)
        return STN_DATA_OK;
    if (scan(p, active, replay, 1, NULL) != STN_DATA_OK)
        return STN_DATA_CONTENT;
    if (stn_pending_enumerate(p, 0, ids, STN_PENDING_MAX_ENTRIES, &available) !=
        STN_PENDING_ACCEPTED)
        return STN_DATA_CONTENT;
    for (i = 0; i < available && n < STN_BLOCK_MAX_TRANSACTIONS; ++i)
    {
        size_t at = find_id(p, ids[i]);
        const stn_pending_entry *e = &p->entries[at];
        stn_transaction tx;
        stn_validation_report r;
        if (replay[at])
            continue;
        if (stn_transaction_decode(e->transaction, e->length, &tx) !=
            STN_DATA_OK)
            return STN_DATA_CONTENT;
        if (tx.type == STN_TX_SHARE_EVIDENCE)
        {
            stn_share_evidence share;
            stn_block_header work_header;
            if (active == NULL ||
                stn_share_decode(tx.record_bytes, tx.record_length, &share) !=
                    STN_DATA_OK ||
                stn_block_header_decode(share.template_header,
                                        STN_SHARE_TEMPLATE_HEADER_SIZE,
                                        &work_header) != STN_DATA_OK)
                return STN_DATA_CONTENT;
            if (!active->state.has_tip || active->state.height == UINT64_MAX)
                continue;
            if (work_header.height >= active->state.height + 1u)
                continue;
            {
                size_t parent_index = (size_t)(work_header.height - 1u);
                uint8_t parent_id[32];
                stn_hash_provider hp = {stn_sha256, NULL};
                if (work_header.height == 0u || parent_index >= active->count ||
                    stn_chain_block_id(active->blocks[parent_index].bytes,
                                       active->blocks[parent_index].length, &hp,
                                       parent_id) != STN_DATA_OK ||
                    memcmp(parent_id, work_header.previous_hash, 32u) != 0)
                    continue;
            }
        }
        if (tx.type == STN_TX_ISSUANCE)
        {
            stn_issuance_record issuance;
            int accepted_share = 0, accepted_issuance = 0;
            stn_data_status state;
            if (active == NULL || active->state.compensation == NULL ||
                stn_issuance_decode(tx.record_bytes, tx.record_length,
                                    &issuance) != STN_DATA_OK)
                return STN_DATA_CONTENT;
            if (issuance.reason != STN_ISSUANCE_REASON_SHARE)
                continue;
            state = history_issuance_state(active, &issuance,
                                           active->state.compensation,
                                           &accepted_share, &accepted_issuance);
            if (state != STN_DATA_OK)
                return state == STN_DATA_PROVIDER_ERROR
                           ? STN_DATA_PROVIDER_ERROR
                           : STN_DATA_CONTENT;
            if (accepted_issuance || !accepted_share)
                continue;
        }
        if (tx.type == STN_TX_PUBLICATION && production_next(active))
        {
            stn_hash_provider hp = {stn_sha256, NULL};
            stn_record record;
            stn_lifecycle_result checked = stn_lifecycle_check_publication(
                active->state.lifecycle, tx.record_bytes, tx.record_length,
                &hp);
            if (checked == STN_LIFECYCLE_PROVIDER ||
                checked == STN_LIFECYCLE_CAPACITY)
                return STN_DATA_PROVIDER_ERROR;
            if (checked != STN_LIFECYCLE_OK)
                continue;
            if (stn_record_decode(tx.record_bytes, tx.record_length, &record) !=
                    STN_RECORD_OK ||
                memcmp(record.network_id, active->state.network_id, 32) != 0)
                continue;
        }
        if (tx.type == STN_TX_PUBLICATION && !production_next(active))
        {
            if (c == NULL || active == NULL ||
                memcmp(c->expected_network, active->state.network_id, 32) != 0)
                return STN_DATA_UNRESOLVED;
            r = stn_validate_intelligence_record(tx.record_bytes,
                                                 tx.record_length, c);
            if (r.acceptance == STN_ACCEPTANCE_ERROR)
                return STN_DATA_PROVIDER_ERROR;
            if (r.acceptance != STN_ACCEPTANCE_UNDER_CONTEXT)
                continue;
        }
        if (e->length + 4 > STN_BLOCK_MAX_BODY - total)
            break;
        if (e->length + 4 > capacity - total)
            return STN_DATA_CAPACITY;
        if (tx.type == STN_TX_CONTRACT_ACTION) {
            stn_hash_provider hp = {stn_sha256, NULL};
            stn_contract_snapshot *trial;
            stn_data_status checked;
            if (active == NULL || active->state.contracts == NULL ||
                active->state.lifecycle == NULL)
                return STN_DATA_UNRESOLVED;
            /* A private trial also rolls back a failed action. Later actions
             * see earlier selected actions, so conflicting creates/transitions
             * cannot poison a candidate even if both passed admission. */
            trial = stn_contract_snapshot_clone(*contracts != NULL ?
                *contracts : active->state.contracts);
            if (trial == NULL)
                return STN_DATA_CAPACITY;
            checked = stn_chain_contract_apply_transaction(trial,
                active->state.lifecycle, &tx, &hp);
            if (checked != STN_DATA_OK) {
                stn_contract_snapshot_release(trial);
                if (checked == STN_DATA_PROVIDER_ERROR || checked == STN_DATA_CAPACITY)
                    return checked;
                continue;
            }
            stn_contract_snapshot_release(*contracts);
            *contracts = trial;
        }
        selected[n].bytes = e->transaction;
        selected[n].length = (uint32_t)e->length;
        ++n;
        total += 4 + e->length;
    }
    if (n == 0)
        return STN_DATA_OK;
    {
        stn_data_status s =
            stn_block_body_encode(selected, n, body, capacity, written);
        if (s == STN_DATA_OK)
            *count = n;
        return s;
    }
}

stn_data_status stn_pending_assemble(const stn_pending *p,
    const stn_validation_context *c,const stn_storage_view *active,
    uint8_t *body,size_t capacity,size_t *written,uint32_t *count)
{
    stn_contract_snapshot *contracts = NULL;
    stn_data_status status = assemble_projected(&contracts,p,c,active,
        body,capacity,written,count);
    stn_contract_snapshot_release(contracts);
    return status;
}
