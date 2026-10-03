#ifndef STN_PEER_SNAPSHOT_H
#define STN_PEER_SNAPSHOT_H
#include "stn_peer.h"
#include "stn_storage_cache.h"
typedef struct stn_peer_snapshot stn_peer_snapshot;
/* Caller serializes provider/cache and all shared accepted-state ownership.
 * Returns owned immutable bytes and scalar summary; no accepted-state references
 * escape. Context policy/provider pointers must remain immutable for its lifetime.
 * Unknown/changed storage is fully validated. Output unchanged on failure. */
stn_storage_status stn_peer_snapshot_load(stn_storage_cache *,
    const stn_chain_context *,const stn_storage_provider *,stn_peer_snapshot **);
void stn_peer_snapshot_release(stn_peer_snapshot *);
size_t stn_peer_snapshot_count(const stn_peer_snapshot *);
stn_peer_status stn_peer_snapshot_serve(const stn_peer_snapshot *,
    stn_peer_session *,const uint8_t *,size_t,uint8_t *,size_t,size_t *);
#endif
