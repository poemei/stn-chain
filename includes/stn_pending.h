/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_PENDING_H
#define STN_PENDING_H
#include "stn_storage.h"
/* Local admission bounds, independent of history and validation batch sizes. */
/* Allow share bursts to use the existing byte budget instead of exhausting
 * 128 small-record slots long before the memory limit. This is local policy. */
#define STN_PENDING_MAX_ENTRIES 1024u
#define STN_PENDING_MAX_BYTES (256u*1024u)
typedef enum stn_pending_result {
    STN_PENDING_ACCEPTED=0, STN_PENDING_DUPLICATE, STN_PENDING_REPLAY,
    STN_PENDING_INVALID, STN_PENDING_UNAVAILABLE, STN_PENDING_CAPACITY,
    STN_PENDING_PROVIDER, STN_PENDING_SIGNATURE, STN_PENDING_AUTHORITY,
    STN_PENDING_TIME, STN_PENDING_NETWORK, STN_PENDING_UNSUPPORTED,
    STN_PENDING_NOT_FOUND
} stn_pending_result;
typedef struct stn_pending_entry {
    uint8_t id[32],signer[32],nonce[32];
    uint8_t *transaction;size_t length;
} stn_pending_entry;
typedef struct stn_pending {
    stn_pending_entry entries[STN_PENDING_MAX_ENTRIES];
    size_t count,bytes;
} stn_pending;
void stn_pending_init(stn_pending *pool);
size_t stn_pending_count(const stn_pending *pool);
stn_pending_result stn_pending_insert(stn_pending *pool,const uint8_t *transaction,
    size_t length,const stn_hash_provider *hash,uint8_t id[32]);
stn_pending_result stn_pending_lookup(const stn_pending *pool,const uint8_t id[32],
    uint8_t *output,size_t capacity,size_t *written);
stn_pending_result stn_pending_remove(stn_pending *pool,const uint8_t id[32]);
stn_pending_result stn_pending_enumerate(const stn_pending *pool,size_t start,
    uint8_t (*ids)[32],size_t capacity,size_t *written);
void stn_pending_clear(stn_pending *pool);
stn_pending_result stn_pending_admit(stn_pending *pool,const uint8_t *record,size_t length,
    const stn_validation_context *context,const stn_storage_view *active,
    const stn_hash_provider *hash,stn_validation_report *report,uint8_t id[32]);
stn_pending_result stn_pending_admit_transaction(stn_pending *pool,
    const uint8_t *transaction,size_t length,const stn_validation_context *context,
    const stn_storage_view *active,const stn_hash_provider *hash,
    stn_validation_report *report,uint8_t id[32]);
/* Pending cleanup is local bookkeeping, never consensus authority. Inclusion
 * discovery may fail if local pending/hash bookkeeping is unavailable; callers
 * accepting an already committed and consensus-valid block must not reject that
 * block for cleanup failure. Apply a mask only when discovery returns OK. */
stn_data_status stn_pending_inclusions(const stn_pending *pool,const stn_storage_view *active,
    const stn_hash_provider *hash,uint8_t remove[STN_PENDING_MAX_ENTRIES]);
void stn_pending_prune(stn_pending *pool,const uint8_t remove[STN_PENDING_MAX_ENTRIES]);
/* Revalidate eligibility; select ascending unsigned transaction-ID bytes.
 * Skip ineligible entries without eviction. Share issuance remains pending until
 * matching share evidence is accepted, preserving evidence-before-issuance
 * consensus ordering. Stop at count/body/caller capacity.
 * Provider error fails the assembly; no empty-block consensus exception. */
stn_data_status stn_pending_assemble(const stn_pending *pool,const stn_validation_context *context,const stn_storage_view *active,
    uint8_t *body,size_t capacity,size_t *written,uint32_t *count);
#endif
