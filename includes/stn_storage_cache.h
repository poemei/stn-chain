/* Copyright (c) 2026 STN-Labz. */
#ifndef STN_STORAGE_CACHE_H
#define STN_STORAGE_CACHE_H
#include "stn_storage.h"
/* Zero initialize; serialize use. Context and its pointed-to policy/providers
 * must remain immutable until release. Disk is read under provider exclusion on
 * every call. Only byte-for-byte identical, previously validated history reuses
 * state. Changes always undergo the ordinary complete storage validation. */
typedef struct stn_storage_cache {
    uint8_t *bytes;
    size_t length;
    stn_storage_view view;
    stn_chain_context context;
} stn_storage_cache;
void stn_storage_cache_release(stn_storage_cache *cache);
stn_storage_status stn_storage_cache_load(stn_storage_cache *cache,
    const stn_chain_context *context,const stn_storage_provider *provider,
    uint8_t *scratch,size_t capacity,stn_storage_view *out);
/* Same read/compare operation with provider exclusion already held. */
stn_storage_status stn_storage_cache_load_locked(stn_storage_cache *cache,
    const stn_chain_context *context,const stn_storage_provider *provider,
    uint8_t *scratch,size_t capacity,stn_storage_view *out);
/* Internal publication hook: bytes and state MUST be the exact history already
 * fully validated and atomically committed under this immutable context.
 * Allocation failure only discards the optimization. Never use untrusted state. */
void stn_storage_cache_remember(stn_storage_cache *cache,
    const stn_chain_context *context,const uint8_t *bytes,size_t length,
    const stn_chain_state *validated_state);
#endif
