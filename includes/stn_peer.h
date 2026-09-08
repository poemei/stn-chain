/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_PEER_H
#define STN_PEER_H
#include "stn_storage.h"
#define STN_PEER_HEADER 12u
#define STN_PEER_MAX_PAYLOAD (STN_BLOCK_MAX_SIZE+4u)
#define STN_PEER_MAX_FRAME (STN_PEER_HEADER+STN_PEER_MAX_PAYLOAD)
typedef enum stn_peer_status {
    STN_PEER_OK=0,STN_PEER_ARGUMENT,STN_PEER_PROTOCOL,STN_PEER_NETWORK,
    STN_PEER_VALIDATION,STN_PEER_IO,STN_PEER_TIMEOUT,STN_PEER_DISCONNECTED,
    STN_PEER_CAPACITY,STN_PEER_STORAGE,STN_PEER_RETAINED
} stn_peer_status;
typedef enum stn_peer_type {
    STN_PEER_HELLO=1,STN_PEER_STATE=2,STN_PEER_GET_HEADERS=3,
    STN_PEER_HEADERS=4,STN_PEER_GET_BLOCK=5,STN_PEER_BLOCK=6
} stn_peer_type;
typedef struct stn_peer_message { uint16_t type;const uint8_t *payload;size_t length; } stn_peer_message;
/* Exact bounded transfers. Implementation owns idle-I/O polling,
 * handles short I/O and distinguishes timeout/disconnect. Close connection on
 * any protocol failure; never reuse a failed byte stream. Caller owns close. */
typedef struct stn_peer_transport {
    void *user;
    stn_peer_status (*send)(void *,const uint8_t *,size_t);
    stn_peer_status (*receive)(void *,uint8_t *,size_t);
} stn_peer_transport;
typedef struct stn_peer_session {
    int handshake;size_t requests; /* Saturating request counter; no lifetime request ceiling. */
} stn_peer_session;
typedef struct stn_peer_report {
    stn_peer_status status;
    size_t reused_blocks,received_blocks;
    int recovery;
    stn_chain_state verified; /* Valid only on OK/RETAINED; never advertisement. */
} stn_peer_report;
typedef struct stn_peer_workspace {
    stn_storage_workspace storage;
    uint8_t *candidate;size_t candidate_capacity;
    uint8_t *frame;size_t frame_capacity;
} stn_peer_workspace;
/* STNP,u16 version=1,u16 type,u32 payload length; big-endian, exact framing.
 * Header-only parse gives bounded payload length before reading/allocating.
 * Decode outputs unchanged on failure; encode permits payload at bytes+12. */
stn_peer_status stn_peer_header(const uint8_t *bytes,size_t length,uint16_t *type,size_t *payload_length);
stn_peer_status stn_peer_decode(const uint8_t *bytes,size_t length,stn_peer_message *out);
stn_peer_status stn_peer_encode(uint16_t type,const uint8_t *payload,size_t length,uint8_t *bytes,size_t capacity,size_t *written);
/* One bounded request/response. A server snapshot must remain immutable for
 * the session. It is revalidated, not trusted state metadata. No filesystem
 * or socket implementation lives here. Unknown/unexpected requests disconnect. */
stn_peer_status stn_peer_serve(const stn_chain_context *context,const stn_block_span *blocks,size_t count,
    stn_peer_session *session,const uint8_t *request,size_t length,uint8_t *response,size_t capacity,size_t *written);
stn_peer_status stn_peer_receive(const stn_peer_transport *transport,uint8_t *frame,size_t capacity,size_t *length);
/* Explicitly connected peer, one outstanding request, pages of at most 64 headers.
 * Headers locate reusable validated prefixes only; full blocks determine work.
 * Claimed summary never determines acceptance. Buffers/context/candidate/active
 * must be disjoint and immutable where borrowed. Scratch can change on failure.
 * No active mutation on failure/RETAINED. Storage adoption rechecks local state
 * under exclusion; successive peers can only improve validated work. */
stn_peer_report stn_peer_sync(const stn_chain_context *context,const stn_storage_provider *storage,
    const stn_peer_transport *transport,stn_peer_workspace *workspace,stn_chain_state *active);
#endif
