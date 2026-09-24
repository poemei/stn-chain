/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_LINUX_PEER_H
#define STN_LINUX_PEER_H

#include "stn_peer.h"

typedef struct stn_linux_peer {
    int socket;
    unsigned io_timeout_ms;
    int opened;
    uint64_t operation_deadline_ms;
    int last_errno;
    int last_operation;
} stn_linux_peer;

/*
 * Explicit IPv4 endpoint, no DNS/discovery. timeout_ms is an idle I/O bound,
 * not a total session lifetime. Objects must not be copied or concurrently used.
 * Close every successful open.
 *
 * Listener binds loopback only; public listening is deliberately not enabled.
 */
stn_peer_status stn_linux_peer_connect(
    const char *ipv4,
    uint16_t port,
    unsigned timeout_ms,
    stn_linux_peer *out,
    stn_peer_transport *transport);

stn_peer_status stn_linux_peer_listen(
    uint16_t port,
    stn_linux_peer *out,
    uint16_t *bound_port);

stn_peer_status stn_linux_peer_accept(
    stn_linux_peer *listener,
    unsigned timeout_ms,
    stn_linux_peer *out,
    stn_peer_transport *transport);

/*
 * May interrupt a live operation from another thread. Join that thread before
 * close/reuse; the object/socket lifetime must remain stable during interrupt.
 */
void stn_linux_peer_interrupt(stn_linux_peer *peer);
void stn_linux_peer_close(stn_linux_peer *peer);

/*
 * Optional operation deadline for bounded RPC/outbound transfers; zero
 * preserves existing idle semantics.
 */
stn_peer_status stn_linux_peer_open_candidate(
    void *user,
    const stn_peer_endpoint *endpoint,
    stn_peer_transport *transport);

void stn_linux_peer_close_candidate(void *user);

/* Observational diagnostics only; never consensus-visible. */
enum {
    STN_LINUX_PEER_OP_NONE = 0,
    STN_LINUX_PEER_OP_CONNECT,
    STN_LINUX_PEER_OP_SEND,
    STN_LINUX_PEER_OP_RECEIVE,
    STN_LINUX_PEER_OP_POLL
};
const char *stn_linux_peer_operation_name(int operation);

#endif