/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. Linux-only adapter. */
#include "../stn_backend.h"
#include "stn_linux_peer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

const char *stn_linux_peer_operation_name(int operation)
{
    switch(operation) {
        case STN_LINUX_PEER_OP_CONNECT: return "connect";
        case STN_LINUX_PEER_OP_SEND: return "send";
        case STN_LINUX_PEER_OP_RECEIVE: return "receive";
        case STN_LINUX_PEER_OP_POLL: return "poll";
        default: return "none";
    }
}

static uint64_t monotonic_ms(void)
{
    struct timespec value;

    if(clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        return 0;
    }

    return ((uint64_t)value.tv_sec * 1000u) +
        ((uint64_t)value.tv_nsec / 1000000u);
}

static stn_peer_status ready(stn_linux_peer *peer, int writing)
{
    struct pollfd descriptor;
    int timeout;
    int result;

    if(peer == NULL || !peer->opened || peer->io_timeout_ms == 0) {
        return STN_PEER_IO;
    }

    if(peer->operation_deadline_ms != 0) {
        uint64_t now = monotonic_ms();
        uint64_t remaining;

        if(now == 0 || now >= peer->operation_deadline_ms) {
            return STN_PEER_IO;
        }

        remaining = peer->operation_deadline_ms - now;

        timeout = remaining < (uint64_t)peer->io_timeout_ms
            ? (int)remaining
            : (int)peer->io_timeout_ms;
    } else {
        timeout = (int)peer->io_timeout_ms;
    }

    descriptor.fd = peer->socket;
    descriptor.events = writing ? POLLOUT : POLLIN;
    descriptor.revents = 0;

    do {
        result = poll(&descriptor, 1, timeout);
    } while(result < 0 && errno == EINTR);

    if(result == 0) {
        return STN_PEER_TIMEOUT;
    }

    if(result < 0) {
        peer->last_operation = STN_LINUX_PEER_OP_POLL;
        peer->last_errno = errno;
        return STN_PEER_IO;
    }

    if((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        int socket_error = 0;
        socklen_t socket_error_length = (socklen_t)sizeof(socket_error);
        peer->last_operation = STN_LINUX_PEER_OP_POLL;
        if(getsockopt(peer->socket, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_length) == 0) {
            peer->last_errno = socket_error;
        } else {
            peer->last_errno = errno;
        }
        return STN_PEER_IO;
    }

    if((descriptor.revents & descriptor.events) == 0) {
        return STN_PEER_IO;
    }

    return STN_PEER_OK;
}

static stn_peer_status send_all(
    void *user,
    const uint8_t *bytes,
    size_t length)
{
    stn_linux_peer *peer = (stn_linux_peer *)user;
    size_t offset = 0;

    if(length > STN_PEER_MAX_FRAME) {
        return STN_PEER_CAPACITY;
    }

    while(offset < length) {
        ssize_t sent;
        stn_peer_status status = ready(peer, 1);

        if(status == STN_PEER_TIMEOUT && offset != 0) {
            continue;
        }

        if(status != STN_PEER_OK) {
            return status;
        }

        sent = send(
            peer->socket,
            bytes + offset,
            length - offset,
            MSG_NOSIGNAL);

        if(sent < 0) {
            if(errno == EAGAIN ||
               errno == EWOULDBLOCK ||
               errno == EINTR) {
                continue;
            }

            peer->last_operation = STN_LINUX_PEER_OP_SEND;
            peer->last_errno = errno;
            return STN_PEER_IO;
        }

        if(sent == 0) {
            return STN_PEER_DISCONNECTED;
        }

        offset += (size_t)sent;
    }

    return STN_PEER_OK;
}

static stn_peer_status receive_all(
    void *user,
    uint8_t *bytes,
    size_t length)
{
    stn_linux_peer *peer = (stn_linux_peer *)user;
    size_t offset = 0;

    if(length > STN_PEER_MAX_FRAME) {
        return STN_PEER_CAPACITY;
    }

    while(offset < length) {
        ssize_t received;
        stn_peer_status status = ready(peer, 0);

        if(status == STN_PEER_TIMEOUT && offset != 0) {
            continue;
        }

        if(status != STN_PEER_OK) {
            return status;
        }

        received = recv(
            peer->socket,
            bytes + offset,
            length - offset,
            0);

        if(received < 0) {
            if(errno == EAGAIN ||
               errno == EWOULDBLOCK ||
               errno == EINTR) {
                continue;
            }

            peer->last_operation = STN_LINUX_PEER_OP_RECEIVE;
            peer->last_errno = errno;
            return STN_PEER_IO;
        }

        if(received == 0) {
            return STN_PEER_DISCONNECTED;
        }

        offset += (size_t)received;
    }

    return STN_PEER_OK;
}

void stn_linux_peer_interrupt(stn_linux_peer *peer)
{
    if(peer != NULL && peer->opened) {
        (void)shutdown(peer->socket, SHUT_RDWR);
    }
}

void stn_linux_peer_close(stn_linux_peer *peer)
{
    if(peer != NULL && peer->opened) {
        (void)close(peer->socket);
        peer->socket = -1;
        peer->opened = 0;
    }
}

static stn_peer_status setup(
    int socket_value,
    unsigned timeout,
    stn_linux_peer *out,
    stn_peer_transport *transport)
{
    int flags;

    flags = fcntl(socket_value, F_GETFL, 0);

    if(flags < 0 ||
       fcntl(socket_value, F_SETFL, flags | O_NONBLOCK) != 0) {
        (void)close(socket_value);
        return STN_PEER_IO;
    }

    out->socket = socket_value;
    out->io_timeout_ms = timeout;
    out->operation_deadline_ms = 0;
    out->last_errno = 0;
    out->last_operation = STN_LINUX_PEER_OP_NONE;
    out->opened = 1;

    if(transport != NULL) {
        transport->user = out;
        transport->send = send_all;
        transport->receive = receive_all;
    }

    return STN_PEER_OK;
}

stn_peer_status stn_linux_peer_connect(
    const char *ip,
    uint16_t port,
    unsigned timeout,
    stn_linux_peer *out,
    stn_peer_transport *transport)
{
    struct sockaddr_in address;
    stn_peer_status status;
    socklen_t error_length;
    int error = 0;
    int socket_value;
    int result;

    if(ip == NULL ||
       out == NULL ||
       transport == NULL ||
       timeout == 0 ||
       timeout > 60000 ||
       port == 0) {
        return STN_PEER_ARGUMENT;
    }

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if(inet_pton(AF_INET, ip, &address.sin_addr) != 1) {
        return STN_PEER_ARGUMENT;
    }

    socket_value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(socket_value < 0) {
        out->last_operation = STN_LINUX_PEER_OP_CONNECT;
        out->last_errno = errno;
        return STN_PEER_IO;
    }

    status = setup(socket_value, timeout, out, transport);

    if(status != STN_PEER_OK) {
        return status;
    }

    result = connect(
        socket_value,
        (struct sockaddr *)&address,
        sizeof(address));

    if(result != 0 && errno != EINPROGRESS) {
        out->last_operation = STN_LINUX_PEER_OP_CONNECT;
        out->last_errno = errno;
        stn_linux_peer_close(out);
        return STN_PEER_IO;
    }

    if(result != 0) {
        status = ready(out, 1);

        if(status != STN_PEER_OK) {
            stn_linux_peer_close(out);
            return status;
        }

        error_length = (socklen_t)sizeof(error);

        if(getsockopt(
            socket_value,
            SOL_SOCKET,
            SO_ERROR,
            &error,
            &error_length) != 0 ||
           error != 0) {
            out->last_operation = STN_LINUX_PEER_OP_CONNECT;
            out->last_errno = error != 0 ? error : errno;
            stn_linux_peer_close(out);
            return STN_PEER_IO;
        }
    }

    return STN_PEER_OK;
}

stn_peer_status stn_linux_peer_listen(
    uint16_t port,
    stn_linux_peer *out,
    uint16_t *bound)
{
    struct sockaddr_in address;
    socklen_t address_length;
    stn_peer_status status;
    int socket_value;

    if(out == NULL || bound == NULL) {
        return STN_PEER_ARGUMENT;
    }

    memset(&address, 0, sizeof(address));

    socket_value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(socket_value < 0) {
        return STN_PEER_IO;
    }

    status = setup(socket_value, 60000, out, NULL);

    if(status != STN_PEER_OK) {
        return status;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if(bind(
        socket_value,
        (struct sockaddr *)&address,
        sizeof(address)) != 0 ||
       listen(socket_value, SOMAXCONN) != 0) {
        stn_linux_peer_close(out);
        return STN_PEER_IO;
    }

    address_length = (socklen_t)sizeof(address);

    if(getsockname(
        socket_value,
        (struct sockaddr *)&address,
        &address_length) != 0) {
        stn_linux_peer_close(out);
        return STN_PEER_IO;
    }

    *bound = ntohs(address.sin_port);

    return STN_PEER_OK;
}

stn_peer_status stn_linux_peer_accept(
    stn_linux_peer *listener,
    unsigned timeout,
    stn_linux_peer *out,
    stn_peer_transport *transport)
{
    stn_peer_status status;
    int accepted;

    if(listener == NULL ||
       out == NULL ||
       transport == NULL ||
       !listener->opened ||
       timeout == 0 ||
       timeout > 60000) {
        return STN_PEER_ARGUMENT;
    }

    listener->io_timeout_ms = timeout;

    status = ready(listener, 0);

    if(status != STN_PEER_OK) {
        return status;
    }

    for(;;) {
        accepted = accept(listener->socket, NULL, NULL);

        if(accepted >= 0) {
            break;
        }

        if(errno == EINTR) {
            continue;
        }

        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return STN_PEER_TIMEOUT;
        }

        return STN_PEER_IO;
    }

    return setup(accepted, timeout, out, transport);
}

stn_peer_status stn_linux_peer_open_candidate(
    void *user,
    const stn_peer_endpoint *endpoint,
    stn_peer_transport *transport)
{
    stn_linux_peer *peer = (stn_linux_peer *)user;
    char ip[16];
    int written;

    if(peer == NULL ||
       endpoint == NULL ||
       peer->opened) {
        return STN_PEER_ARGUMENT;
    }

    written = snprintf(
        ip,
        sizeof(ip),
        "%u.%u.%u.%u",
        (unsigned)endpoint->address[0],
        (unsigned)endpoint->address[1],
        (unsigned)endpoint->address[2],
        (unsigned)endpoint->address[3]);

    if(written < 0 || (size_t)written >= sizeof(ip)) {
        return STN_PEER_ARGUMENT;
    }

    return stn_linux_peer_connect(
        ip,
        endpoint->port,
        1000,
        peer,
        transport);
}

void stn_linux_peer_close_candidate(void *user)
{
    stn_linux_peer_close((stn_linux_peer *)user);
}