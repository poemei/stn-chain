/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_PEER_H
#define STN_PEER_H
#include "stn_storage.h"
#include "stn_pending.h"
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
    STN_PEER_HEADERS=4,STN_PEER_GET_BLOCK=5,STN_PEER_BLOCK=6,
    STN_PEER_GET_PEERS=7,STN_PEER_PEERS=8
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
struct stn_peer_candidates;struct stn_peer_endpoint;
typedef struct stn_peer_session {
    const struct stn_peer_candidates *known; /* Optional explicitly shareable candidates, immutable during session. */
    const struct stn_peer_endpoint *self; /* Optional configured listening endpoint to omit. */
    int discovery_sent;
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
    /* Optional local store. Serialize the entire sync with pending/RPC work.
     * Only successful adoption applies prepared canonical-ID removals. */
    stn_pending *pending;
} stn_peer_workspace;
/* STNP,u16 version=2,u16 type,u32 payload length; big-endian, exact framing.
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
/* Local candidate policy only; explicit discovery codecs serialize endpoint fields.
 * IPv4 matches the existing transport; octets are in network order, port is
 * a numeric value. No text aliases, DNS, IPv6, source priority or identity.
 * Zero-init before use. Caller owns/serializes access; do not edit entries.
 * Enumerate entries[0..count) in ascending address then numeric-port order.
 * 64 is a local resource bound, independent of consensus batch limits.
 * Add: OK inserts, RETAINED means duplicate (even when full), ARGUMENT rejects
 * unusable endpoint/state, CAPACITY means full. Non-OK leaves store unchanged.
 * No connection, handshake, reachability or authority is implied. */
#define STN_PEER_CANDIDATE_MAX 64u
typedef struct stn_peer_endpoint { uint8_t address[4]; uint16_t port; } stn_peer_endpoint;
typedef struct stn_peer_candidates {
    stn_peer_endpoint entries[STN_PEER_CANDIDATE_MAX];size_t count;
} stn_peer_candidates;
stn_peer_status stn_peer_candidate_add(stn_peer_candidates *set,const stn_peer_endpoint *endpoint);
/* One caller-owned outbound lane, not an application-wide connection limit.
 * Init once on an inactive object; only validated discovery may extend candidates.
 * open/close own only this lane's resources; close must clean partial opens.
 * Caller serializes step with pending/RPC mutation and supplies monotonic ms.
 * One attempt/refresh per 5 seconds; failure advances canonical round-robin.
 * Connected steps reuse the existing handshake and revalidate all evidence.
 * Close before destroy/re-init. No consensus preference or eviction. */
#define STN_PEER_OUTBOUND_INTERVAL_MS 5000u
typedef struct stn_peer_connector {
    void *user;
    stn_peer_status (*open)(void *,const stn_peer_endpoint *,stn_peer_transport *);
    void (*close)(void *);
} stn_peer_connector;
typedef struct stn_peer_outbound {
    stn_peer_candidates candidates;stn_peer_connector connector;stn_peer_transport transport;
    const stn_peer_endpoint *self; /* Optional caller-owned immutable local endpoint. */
    uint32_t remote_capabilities;stn_peer_status last_discovery;
    size_t next;uint64_t last_step;int started,connected;stn_peer_status last_status;
} stn_peer_outbound;
stn_peer_status stn_peer_outbound_init(stn_peer_outbound *,const stn_peer_candidates *,const stn_peer_connector *);
stn_peer_status stn_peer_outbound_step(stn_peer_outbound *,uint64_t,const stn_chain_context *,
    const stn_storage_provider *,stn_peer_workspace *,stn_chain_state *);
void stn_peer_outbound_close(stn_peer_outbound *);
/* Discovery payload: u16 count (0..64), then count * (IPv4[4], u16 port), BE.
 * No native structure serialization. Ingest stages/validates the whole batch,
 * canonicalizes order, coalesces duplicates, omits self and atomically commits.
 * Invalid or capacity-exhausted batches leave the destination unchanged.
 * Encode writes canonical order from an eligible store; self may be NULL. */
#define STN_PEER_DISCOVERY_MAX (2u+6u*STN_PEER_CANDIDATE_MAX)
stn_peer_status stn_peer_discovery_encode(const stn_peer_candidates *,const stn_peer_endpoint *,uint8_t *,size_t,size_t *);
stn_peer_status stn_peer_discovery_admit(stn_peer_candidates *,const stn_peer_endpoint *,const uint8_t *,size_t);
#endif
