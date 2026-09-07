/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_STORAGE_H
#define STN_STORAGE_H
#include "stn_fork.h"

#define STN_STORAGE_HEADER 12u
#define STN_STORAGE_OVERHEAD 44u
#define STN_STORAGE_MAX_SIZE (STN_STORAGE_OVERHEAD + STN_CHAIN_MAX_BATCH * (4u + STN_BLOCK_MAX_SIZE))

typedef enum stn_storage_status {
    STN_STORAGE_OK=0, STN_STORAGE_ARGUMENT, STN_STORAGE_FORMAT,
    STN_STORAGE_VALIDATION, STN_STORAGE_IO, STN_STORAGE_NOT_FOUND,
    STN_STORAGE_EXISTS, STN_STORAGE_STALE, STN_STORAGE_NOT_PREFERRED,
    STN_STORAGE_UNRESOLVED, STN_STORAGE_BUSY, STN_STORAGE_CAPACITY
} stn_storage_status;

/* Provider owns exclusion across read/validate/replace; acquire fails without
 * ownership, release cannot fail. read is bounded, never truncates to capacity.
 * replace must publish ALL bytes atomically or preserve the previous snapshot.
 * No callback may return failure after publication. Unknown status fails closed.
 * No concurrent calls on a provider/workspace; context and inputs immutable. */
typedef struct stn_storage_provider {
    void *user;
    stn_storage_status (*acquire)(void *user);
    void (*release)(void *user);
    stn_storage_status (*read)(void *user,uint8_t *bytes,size_t capacity,size_t *length);
    stn_storage_status (*replace)(void *user,const uint8_t *bytes,size_t length);
} stn_storage_provider;

typedef struct stn_storage_view {
    stn_block_span blocks[STN_CHAIN_MAX_BATCH];
    size_t count;
    stn_chain_state state;
} stn_storage_view;

typedef struct stn_storage_workspace {
    uint8_t *current_bytes; size_t current_capacity;
    uint8_t *next_bytes; size_t next_capacity;
} stn_storage_workspace;

/* Storage v1: STNS, u16 version=1, u16 flags=0, u32 block_count,
 * repeated u32 length + canonical block, then SHA256(storage-domain || prefix).
 * All integers big-endian. 1..64 blocks. No raw structs or cached metadata.
 * Encode/decode revalidate complete PoW history from EMPTY. Decode output is
 * unchanged on failure; encode scratch may change but *written stays zero.
 * Decoded spans borrow input bytes. Buffers/objects must not overlap. */
stn_storage_status stn_storage_encode(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,uint8_t *bytes,size_t capacity,size_t *written);
stn_storage_status stn_storage_decode(const stn_chain_context *context,
    const uint8_t *bytes,size_t length,stn_storage_view *out);

/* Scratch may change on failure, but out/active remain byte-for-byte unchanged.
 * Scratch must not overlap inputs, output, context or provider. Capacity is
 * caller-owned, never allocated from disk-controlled lengths. On load success
 * out spans borrow scratch until its next use. Missing store is NOT_FOUND;
 * corruption is never repaired or treated as an empty valid chain. */
stn_storage_status stn_storage_load(const stn_chain_context *context,
    const stn_storage_provider *provider,uint8_t *scratch,size_t capacity,stn_storage_view *out);
/* Explicit first installation only; existing files (even corrupt) are refused. */
stn_storage_status stn_storage_create(const stn_chain_context *context,
    const stn_storage_provider *provider,const stn_block_span *blocks,size_t count,
    uint8_t *scratch,size_t capacity,stn_chain_state *active);
/* Extension and reorganization share this path. Reload accepted disk history,
 * compare all active-state fields, recalculate plan and require exact plan
 * agreement + strictly greater work. Persist before changing active. Both
 * workspace buffers must be disjoint from one another and all inputs.
 * No detached transaction/mempool side effects. */
stn_storage_status stn_storage_apply(const stn_chain_context *context,
    const stn_storage_provider *provider,const stn_block_span *candidate,size_t count,
    const stn_reorg_plan *plan,stn_storage_workspace *workspace,stn_chain_state *active);
/* Explicit recovery mode, separate from strict startup load. A valid prefix
 * is evidence only, never an activated repaired chain. Unknown framing yields
 * EMPTY; recognizable framing is scanned only until first invalid block.
 * Provider failures are not corruption and never authorize recovery. */
stn_storage_status stn_storage_recovery_read(const stn_chain_context *context,
    const stn_storage_provider *provider,uint8_t *scratch,size_t capacity,
    stn_storage_view *prefix,int *needs_recovery);
/* Re-read under exclusion, revalidate peer evidence, use existing fork choice.
 * Healthy snapshots require greater work. Corrupt snapshots allow equal work
 * only for the exact validated prefix tip. No lower-work rollback. Output
 * state commits only after atomic replacement; no raw patch or prefix activation. */
stn_storage_status stn_storage_adopt(const stn_chain_context *context,
    const stn_storage_provider *provider,const stn_block_span *candidate,size_t count,
    stn_storage_workspace *workspace,stn_chain_state *active);
#endif
