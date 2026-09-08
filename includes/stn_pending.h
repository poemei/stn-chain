/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_PENDING_H
#define STN_PENDING_H
#include "stn_storage.h"
/* Local admission bounds, independent of history and validation batch sizes. */
#define STN_PENDING_MAX_ENTRIES 128u
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
/* Store foundation: init only fresh storage; clear resets/releases a live store
 * and is also its destructor. Do not copy a live store or modify its fields.
 * Calls require external serialization. All input/output spans must be disjoint
 * from the store and each other. No internal pointers are returned.
 * Insert checks canonical structure and derives the ID using the protocol hash;
 * it does NOT authenticate/authorize a submission or establish chain eligibility.
 * The caller owns admission policy. Accepted bytes are copied and owned until
 * removal/clear. Duplicate ID takes precedence over capacity; no eviction.
 * Lookup copies into caller storage, setting written=0 on failure.
 * Enumeration copies at most capacity IDs from start in unsigned-byte ascending
 * ID order. A start at/past the end returns an empty page. */
void stn_pending_init(stn_pending *pool);
size_t stn_pending_count(const stn_pending *pool);
stn_pending_result stn_pending_insert(stn_pending *pool,const uint8_t *transaction,
    size_t length,const stn_hash_provider *hash,uint8_t id[32]);
stn_pending_result stn_pending_lookup(const stn_pending *pool,const uint8_t id[32],
    uint8_t *output,size_t capacity,size_t *written);
stn_pending_result stn_pending_remove(stn_pending *pool,const uint8_t id[32]);
stn_pending_result stn_pending_enumerate(const stn_pending *pool,size_t start,
    uint8_t (*ids)[32],size_t capacity,size_t *written);
/* Zero initialize; serialize access with chain activation. No pool persistence.
 * Views must be fully validated active history, not untrusted peer metadata.
 * Context/hook snapshots remain immutable for each operation. Missing providers
 * fail closed. Accepted means every existing validation stage passed under that
 * context; the pool supplies no signature/authority implementation. */
void stn_pending_clear(stn_pending *pool);
stn_pending_result stn_pending_admit(stn_pending *pool,const uint8_t *record,size_t length,
    const stn_validation_context *context,const stn_storage_view *active,
    const stn_hash_provider *hash,stn_validation_report *report,uint8_t id[32]);
/* Canonical STNT entry point; reuses the record admission path above. Both
 * require a validated active view and matching immutable validation snapshot.
 * Missing hooks/context fail closed. INVALID includes malformed envelope or
 * payload; UNSUPPORTED covers reserved transaction/record versions and types.
 * The report describes record validation, not subsequent store/replay results.
 * No ownership transfers on rejection. Use these entry points for submissions;
 * insert alone is only the structural store primitive. */
stn_pending_result stn_pending_admit_transaction(stn_pending *pool,
    const uint8_t *transaction,size_t length,const stn_validation_context *context,
    const stn_storage_view *active,const stn_hash_provider *hash,
    stn_validation_report *report,uint8_t id[32]);
/* Prepare inclusion/replay removals without mutation; apply only after successful
 * activation. All outputs/input spans must be disjoint. Mask is not serialized. */
stn_data_status stn_pending_inclusions(const stn_pending *pool,const stn_storage_view *active,
    uint8_t remove[STN_PENDING_MAX_ENTRIES]);
void stn_pending_prune(stn_pending *pool,const uint8_t remove[STN_PENDING_MAX_ENTRIES]);
/* Revalidate eligibility; select ascending unsigned transaction-ID bytes.
 * Skip ineligible entries without eviction. Stop at count/body/caller capacity.
 * Provider error fails the assembly; no empty-block consensus exception. */
stn_data_status stn_pending_assemble(const stn_pending *pool,const stn_validation_context *context,const stn_storage_view *active,
    uint8_t *body,size_t capacity,size_t *written,uint32_t *count);
#endif
