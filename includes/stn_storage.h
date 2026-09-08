/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_STORAGE_H
#define STN_STORAGE_H
#include "stn_fork.h"

#define STN_STORAGE_HEADER 12u
#define STN_STORAGE_OVERHEAD 44u

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

/* blocks is allocated by storage decode/load and borrows block bytes from the
 * caller-owned input/scratch buffer. Release the span array with
 * stn_storage_view_release(); block bytes remain caller-owned. */
typedef struct stn_storage_view {
    stn_block_span *blocks;
    size_t count;
    stn_chain_state state;
} stn_storage_view;

typedef struct stn_storage_workspace {
    uint8_t *current_bytes; size_t current_capacity;
    uint8_t *next_bytes; size_t next_capacity;
} stn_storage_workspace;

/* Storage v1: STNS, u16 version=1, u16 flags=0, u32 block_count,
 * repeated u32 length + canonical block, then SHA256(storage-domain || prefix).
 * All integers big-endian. History length is NOT constrained by
 * STN_CHAIN_MAX_BATCH; that limit applies only to bounded validation/fork work.
 * Encode/decode validate history incrementally from EMPTY. No raw structs or
 * cached consensus metadata. */
stn_storage_status stn_storage_encode(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,uint8_t *bytes,size_t capacity,size_t *written);
stn_storage_status stn_storage_decode(const stn_chain_context *context,
    const uint8_t *bytes,size_t length,stn_storage_view *out);
void stn_storage_view_release(stn_storage_view *view);

stn_storage_status stn_storage_load(const stn_chain_context *context,
    const stn_storage_provider *provider,uint8_t *scratch,size_t capacity,stn_storage_view *out);
stn_storage_status stn_storage_create(const stn_chain_context *context,
    const stn_storage_provider *provider,const stn_block_span *blocks,size_t count,
    uint8_t *scratch,size_t capacity,stn_chain_state *active);
stn_storage_status stn_storage_apply(const stn_chain_context *context,
    const stn_storage_provider *provider,const stn_block_span *candidate,size_t count,
    const stn_reorg_plan *plan,stn_storage_workspace *workspace,stn_chain_state *active);
/* Fast path for the ordinary server case: append exactly one candidate block to
 * the current validated tip. It does not run fork choice because there is no
 * competing branch. Disk is re-read under exclusion and the active state must
 * still match before publication. */
stn_storage_status stn_storage_extend(const stn_chain_context *context,
    const stn_storage_provider *provider,const uint8_t *block,size_t length,
    stn_storage_workspace *workspace,stn_chain_state *active);
stn_storage_status stn_storage_recovery_read(const stn_chain_context *context,
    const stn_storage_provider *provider,uint8_t *scratch,size_t capacity,
    stn_storage_view *prefix,int *needs_recovery);
stn_storage_status stn_storage_adopt(const stn_chain_context *context,
    const stn_storage_provider *provider,const stn_block_span *candidate,size_t count,
    stn_storage_workspace *workspace,stn_chain_state *active);
#endif
