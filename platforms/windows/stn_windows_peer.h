/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_WINDOWS_PEER_H
#define STN_WINDOWS_PEER_H
#include "stn_peer.h"
typedef struct stn_windows_peer { uintptr_t socket;unsigned io_timeout_ms;int opened;uint64_t operation_deadline_ms; } stn_windows_peer;
/* Explicit IPv4 endpoint, no DNS/discovery. timeout_ms is an idle I/O bound,
 * not a total session lifetime. Objects must not be copied or concurrently used.
 * Close every successful open.
 * Listener binds loopback only; public listening is deliberately not enabled. */
stn_peer_status stn_windows_peer_connect(const char *ipv4,uint16_t port,unsigned timeout_ms,
    stn_windows_peer *out,stn_peer_transport *transport);
stn_peer_status stn_windows_peer_listen(uint16_t port,stn_windows_peer *out,uint16_t *bound_port);
stn_peer_status stn_windows_peer_accept(stn_windows_peer *listener,unsigned timeout_ms,
    stn_windows_peer *out,stn_peer_transport *transport);
/* May interrupt a live operation from another thread. Join that thread before
 * close/reuse; the object/socket lifetime must remain stable during interrupt. */
void stn_windows_peer_interrupt(stn_windows_peer *peer);
void stn_windows_peer_close(stn_windows_peer *peer);
/* Optional outbound operation deadline; zero preserves existing idle semantics. */
stn_peer_status stn_windows_peer_open_candidate(void *,const stn_peer_endpoint *,stn_peer_transport *);
void stn_windows_peer_close_candidate(void *);
#endif
