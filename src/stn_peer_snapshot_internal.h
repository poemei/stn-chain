#ifndef STN_PEER_SNAPSHOT_INTERNAL_H
#define STN_PEER_SNAPSHOT_INTERNAL_H
#include "stn_peer_snapshot.h"
/* Only stn_peer_snapshot_load constructs this, from validated storage. */
struct stn_peer_snapshot {
    stn_chain_context context;
    stn_chain_state summary; /* Only height, tip and work; no owned references. */
    uint8_t *bytes;
    stn_block_span *blocks;
    size_t count;
};
#endif
