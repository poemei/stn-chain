/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "../stn_backend.h"
#include "stn_linux_storage.h"
#include "stn_linux_peer.h"
#include "stn_mining.h"
#include "stn_sha256.h"
#include "../../src/stn_wire_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

#ifdef STN_PHASE9_TEST_RUNTIME
#include "../../tests/stn_phase9_runtime.h"
#endif

#define APP_STORAGE_INITIAL \
    (STN_STORAGE_OVERHEAD + 4u + STN_BLOCK_HEADER_SIZE + STN_BLOCK_MIN_BODY)

#define APP_RPC_ACCEPT_POLL_MS 250u
#define APP_RPC_IO_TIMEOUT_MS 60000u

static volatile sig_atomic_t stopping;

static uint64_t monotonic_ms(void)
{
    struct timespec value;

    if(clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        return 0;
    }

    return ((uint64_t)value.tv_sec * 1000u) +
        ((uint64_t)value.tv_nsec / 1000000u);
}

static void sleep_ms(unsigned milliseconds)
{
    struct timespec requested;

    requested.tv_sec = (time_t)(milliseconds / 1000u);
    requested.tv_nsec = (long)((milliseconds % 1000u) * 1000000u);

    while(nanosleep(&requested, &requested) != 0 && errno == EINTR) {
    }
}

static void stop_signal(int signal_number)
{
    (void)signal_number;
    stopping = 1;
}

static int install_signal_handlers(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = stop_signal;

    if(sigemptyset(&action.sa_mask) != 0) {
        return 0;
    }

    if(sigaction(SIGINT, &action, NULL) != 0) {
        return 0;
    }

    if(sigaction(SIGTERM, &action, NULL) != 0) {
        return 0;
    }

    return 1;
}

static int read_file(
    const char *path,
    uint8_t *bytes,
    size_t capacity,
    size_t *length)
{
    FILE *file;
    int extra;
    int bad;

    file = fopen(path, "rb");

    if(file == NULL) {
        return 0;
    }

    *length = fread(bytes, 1, capacity, file);
    extra = fgetc(file);
    bad = ferror(file);

    (void)fclose(file);

    return !bad && extra == EOF && *length != 0;
}

static size_t initial_storage_capacity(const char *path)
{
    struct stat info;
    size_t required = APP_STORAGE_INITIAL;

    if(stat(path, &info) == 0 &&
       S_ISREG(info.st_mode) &&
       info.st_size >= 0 &&
       (uintmax_t)info.st_size <= (uintmax_t)SIZE_MAX) {
        size_t existing = (size_t)info.st_size;

        if(existing > required) {
            required = existing;
        }
    }

    return required;
}

static void development_genesis(uint8_t *bytes)
{
    static const uint8_t commitment[32] = {
        0xec,0xf9,0x1b,0xfa,0x6a,0x4e,0x06,0xd8,
        0x6a,0x24,0xd6,0x36,0xee,0xd2,0x5f,0x01,
        0xcf,0x30,0x29,0x49,0xe8,0x53,0x51,0xd4,
        0x37,0xe0,0x4c,0x4e,0x49,0x21,0x9a,0x76
    };

    memset(bytes, 0, 364);

    memcpy(bytes, "STNB", 4);
    bytes[5] = 3;
    bytes[8] = 1;
    bytes[163] = 1;
    bytes[167] = 196;
    bytes[171] = 192;

    memcpy(bytes + 172, "STNT", 4);
    bytes[177] = 1;
    bytes[179] = 1;
    bytes[183] = 180;

    memcpy(bytes + 184, "STNR", 4);
    bytes[189] = 1;
    bytes[191] = 1;
    bytes[192] = 1;
    bytes[256] = 3;

    memcpy(bytes + 88, commitment, 32);

    /*
     * Bootstrap PoW must not use the protocol maximum target.  The former
     * 0x7fff... target made roughly half of all hashes valid blocks, causing
     * qualifying shares and replacement jobs to churn faster than miners
     * could meaningfully work them.  This deterministic bootstrap target is
     * approximately 2^-25 of the SHA-256 space.  Nonce 51449120 is the
     * qualified genesis proof for this exact header.
     */
    memset(bytes + 120, 255, 32);
    bytes[120] = 0;
    bytes[121] = 0;
    bytes[122] = 0;
    bytes[123] = 127;
    bytes[152] = 0;
    bytes[153] = 0;
    bytes[154] = 0;
    bytes[155] = 0;
    bytes[156] = 3;
    bytes[157] = 17;
    bytes[158] = 13;
    bytes[159] = 32;
}

/* Recover the local anchor only; normal storage load still validates the full
 * checksum, genesis, PoW and history before the node starts serving. */
static int history_genesis(const char *path, uint8_t *bytes, size_t *length)
{
    uint8_t header[16];
    struct stat info;
    FILE *file;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    size_t size;
    int ok;
    if(fd < 0) { return errno == ENOENT ? 0 : -1; }
    if(fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(fd); return -1;
    }
    file = fdopen(fd, "rb");
    if(file == NULL) { close(fd); return -1; }
    if(fread(header, 1, sizeof(header), file) != sizeof(header) ||
       memcmp(header, "STNS\0\1\0\0", 8) != 0 ||
       stn_wire_read(header + 8, 4) == 0) {
        fclose(file); return -1;
    }
    size = (size_t)stn_wire_read(header + 12, 4);
    ok = size >= STN_BLOCK_HEADER_SIZE + STN_BLOCK_MIN_BODY &&
        size <= STN_BLOCK_MAX_SIZE;
    if(ok) { ok = fread(bytes, 1, size, file) == size && !ferror(file); }
    if(fclose(file) != 0) { ok = 0; }
    if(!ok) { return -1; }
    *length = size;
    return 1;
}

static stn_peer_status transfer(
    const stn_peer_transport *transport,
    uint8_t *bytes,
    size_t length,
    int sending,
    int idle_allowed)
{
    size_t remaining = length;

    while(length != 0) {
        size_t chunk = length > 65536u ? 65536u : length;
        stn_peer_status status;

        status = sending
            ? transport->send(transport->user, bytes, chunk)
            : transport->receive(transport->user, bytes, chunk);

        if(status == STN_PEER_TIMEOUT &&
           (!idle_allowed || length != remaining)) {
            continue;
        }

        if(status != STN_PEER_OK) {
            return status;
        }

        bytes += chunk;
        length -= chunk;
    }

    return STN_PEER_OK;
}

static stn_peer_status rpc_transfer(
    stn_linux_peer *peer,
    const stn_peer_transport *transport,
    uint8_t *bytes,
    size_t length,
    int sending,
    int idle_allowed)
{
    stn_peer_status status;
    uint64_t now = monotonic_ms();

    if(now == 0) {
        return STN_PEER_IO;
    }

    peer->operation_deadline_ms = now + APP_RPC_IO_TIMEOUT_MS;

    status = transfer(
        transport,
        bytes,
        length,
        sending,
        idle_allowed);

    peer->operation_deadline_ms = 0;

    return status;
}

typedef struct rpc_client {
    pthread_t thread;
    stn_linux_peer peer;
    stn_peer_transport transport;
    const stn_rpc_service *service;
    pthread_mutex_t *dispatch_lock;
    int done;
    pthread_mutex_t done_lock;
    struct rpc_client *next;
} rpc_client;

static int client_done(rpc_client *client)
{
    int done;

    pthread_mutex_lock(&client->done_lock);
    done = client->done;
    pthread_mutex_unlock(&client->done_lock);

    return done;
}

static void client_set_done(rpc_client *client)
{
    pthread_mutex_lock(&client->done_lock);
    client->done = 1;
    pthread_mutex_unlock(&client->done_lock);
}

static void *rpc_client_thread(void *user)
{
    rpc_client *client = (rpc_client *)user;
    uint8_t *request = (uint8_t *)malloc(STN_RPC_MAX_FRAME);
    uint8_t *response = (uint8_t *)malloc(STN_RPC_MAX_FRAME);

    if(request != NULL && response != NULL) {
        while(!stopping) {
            size_t payload_length;
            size_t response_length;
            stn_peer_status io;
            stn_rpc_code dispatch;

            io = rpc_transfer(
                &client->peer,
                &client->transport,
                request,
                24,
                0,
                1);

            if(io == STN_PEER_TIMEOUT) {
                continue;
            }

            if(io != STN_PEER_OK) {
                break;
            }

            if(memcmp(request, "STNC", 4) != 0 ||
               stn_rpc_payload_length(
                   request,
                   24,
                   &payload_length) != STN_RPC_OK) {
                break;
            }

            io = rpc_transfer(
                &client->peer,
                &client->transport,
                request + 24,
                payload_length,
                0,
                0);

            if(io != STN_PEER_OK) {
                break;
            }

            pthread_mutex_lock(client->dispatch_lock);

            dispatch = stn_rpc_dispatch(
                request,
                24 + payload_length,
                STN_RPC_READ | STN_RPC_SUBMISSION,
                client->service,
                response,
                STN_RPC_MAX_FRAME,
                &response_length);

            pthread_mutex_unlock(client->dispatch_lock);

            if(dispatch != STN_RPC_OK ||
               rpc_transfer(
                   &client->peer,
                   &client->transport,
                   response,
                   response_length,
                   1,
                   0) != STN_PEER_OK) {
                break;
            }
        }
    }

    free(request);
    free(response);

    client_set_done(client);

    return NULL;
}

static void reap_clients(rpc_client **head)
{
    rpc_client **link = head;

    while(*link != NULL) {
        rpc_client *client = *link;

        if(client_done(client)) {
            (void)pthread_join(client->thread, NULL);
            stn_linux_peer_close(&client->peer);
            pthread_mutex_destroy(&client->done_lock);

            *link = client->next;
            free(client);
        } else {
            link = &client->next;
        }
    }
}

static void stop_clients(rpc_client **head)
{
    rpc_client *client;

    for(client = *head; client != NULL; client = client->next) {
        stn_linux_peer_interrupt(&client->peer);
    }

    while(*head != NULL) {
        client = *head;

        (void)pthread_join(client->thread, NULL);
        stn_linux_peer_close(&client->peer);
        pthread_mutex_destroy(&client->done_lock);

        *head = client->next;
        free(client);
    }
}

static int serve_once(
    stn_linux_peer *listener,
    const stn_rpc_service *service)
{
    stn_linux_peer peer;
    stn_peer_transport transport;
    uint8_t *request = NULL;
    uint8_t *response = NULL;
    int ok = 0;

    memset(&peer, 0, sizeof(peer));
    peer.socket = -1;

    if(stn_linux_peer_accept(
        listener,
        APP_RPC_IO_TIMEOUT_MS,
        &peer,
        &transport) != STN_PEER_OK) {
        return 0;
    }

    request = (uint8_t *)malloc(STN_RPC_MAX_FRAME);
    response = (uint8_t *)malloc(STN_RPC_MAX_FRAME);

    if(request != NULL && response != NULL) {
        for(;;) {
            size_t payload_length;
            size_t response_length;
            stn_peer_status io;

            io = rpc_transfer(
                &peer,
                &transport,
                request,
                24,
                0,
                1);

            if(io == STN_PEER_TIMEOUT) {
                continue;
            }

            if(io != STN_PEER_OK) {
                ok = 1;
                break;
            }

            if(memcmp(request, "STNC", 4) != 0 ||
               stn_rpc_payload_length(
                   request,
                   24,
                   &payload_length) != STN_RPC_OK) {
                break;
            }

            if(rpc_transfer(
                &peer,
                &transport,
                request + 24,
                payload_length,
                0,
                0) != STN_PEER_OK) {
                break;
            }

            if(stn_rpc_dispatch(
                   request,
                   24 + payload_length,
                   STN_RPC_READ | STN_RPC_SUBMISSION,
                   service,
                   response,
                   STN_RPC_MAX_FRAME,
                   &response_length) != STN_RPC_OK ||
               rpc_transfer(
                   &peer,
                   &transport,
                   response,
                   response_length,
                   1,
                   0) != STN_PEER_OK) {
                break;
            }
        }
    }

    free(request);
    free(response);

    stn_linux_peer_close(&peer);

    return ok;
}

typedef struct outbound_runtime {
    stn_peer_outbound manager;
    stn_linux_peer socket;
    stn_peer_workspace workspace;
    stn_mining_service *mining;
    pthread_mutex_t *lock;
    pthread_t thread;
    int thread_started;
} outbound_runtime;

static void *outbound_thread(void *user)
{
    outbound_runtime *runtime = (outbound_runtime *)user;

    while(!stopping) {
        uint64_t now = monotonic_ms();

        if(now == 0) {
            stopping = 1;
            break;
        }

        pthread_mutex_lock(runtime->lock);

        runtime->socket.operation_deadline_ms = now + 5000u;

        (void)stn_peer_outbound_step(
            &runtime->manager,
            now,
            runtime->mining->chain,
            runtime->mining->storage,
            &runtime->workspace,
            &runtime->mining->active);

        pthread_mutex_unlock(runtime->lock);

        sleep_ms(100);
    }

    stn_peer_outbound_close(&runtime->manager);

    return NULL;
}

static int candidate_argument(
    const char *text,
    stn_peer_candidates *set)
{
    stn_peer_endpoint endpoint;
    unsigned values[5] = {0};
    size_t i;
    const char *cursor = text;

    for(i = 0; i < 5; ++i) {
        unsigned digits = 0;

        while(*cursor >= '0' && *cursor <= '9') {
            if(++digits > (i == 4 ? 5u : 3u)) {
                return 0;
            }

            values[i] =
                values[i] * 10u +
                (unsigned)(*cursor++ - '0');
        }

        if(digits == 0 ||
           values[i] > (i == 4 ? 65535u : 255u)) {
            return 0;
        }

        if(i < 4) {
            if(*cursor++ != (i == 3 ? ':' : '.')) {
                return 0;
            }
        }
    }

    if(*cursor != '\0') {
        return 0;
    }

    for(i = 0; i < 4; ++i) {
        endpoint.address[i] = (uint8_t)values[i];
    }

    endpoint.port = (uint16_t)values[4];

    {
        stn_peer_status status =
            stn_peer_candidate_add(set, &endpoint);

        return status == STN_PEER_OK ||
            status == STN_PEER_RETAINED;
    }
}

int stn_linux_app(int argc, char **argv)
{
    const char *data = "stn-chain-dev.stns";
    const char *genesis_path = NULL;
    const char *transaction_path = NULL;

    int dev = 0;
    int once = 0;
    int i;
    int result = EXIT_FAILURE;
    int lock_ready = 0;
    int instance_lock = -1;
    char *instance_path = NULL;

    unsigned long port = 18473;
    char *end;

    uint8_t *genesis = NULL;
    uint8_t *body = NULL;

    size_t genesis_length = 0;
    size_t transaction_length = 0;

    stn_block decoded;
    stn_chain_context chain = {0};
    stn_pow_policy policy;
    stn_mining_service mining = {0};
    stn_pending pending = {0};

    stn_linux_storage disk;
    stn_storage_provider storage;
    stn_storage_view view;

    stn_linux_peer listener;
    uint16_t bound;

    stn_storage_status status;
    stn_rpc_service service = {
        &mining,
        stn_mining_handle
    };

    rpc_client *clients = NULL;
    pthread_mutex_t dispatch_lock;

    outbound_runtime outbound;
    stn_peer_candidates candidates = {0};

    memset(&disk, 0, sizeof(disk));
    disk.lock_fd = -1;

    memset(&listener, 0, sizeof(listener));
    listener.socket = -1;

    memset(&outbound, 0, sizeof(outbound));
    outbound.socket.socket = -1;

    stopping = 0;

    for(i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "--help") == 0) {
            result = EXIT_SUCCESS;
            goto usage;
        } else if(strcmp(argv[i], "--dev") == 0) {
            dev = 1;
        } else if(strcmp(argv[i], "--peer") == 0 &&
                  i + 1 < argc) {
            if(!candidate_argument(
                argv[++i],
                &candidates)) {
                goto usage;
            }
        } else if(strcmp(argv[i], "--once") == 0) {
            once = 1;
        } else if(strcmp(argv[i], "--data") == 0 &&
                  i + 1 < argc) {
            data = argv[++i];
        } else if(strcmp(argv[i], "--genesis") == 0 &&
                  i + 1 < argc) {
            genesis_path = argv[++i];
        } else if(strcmp(argv[i], "--transaction") == 0 &&
                  i + 1 < argc) {
            transaction_path = argv[++i];
        } else if(strcmp(argv[i], "--rpc-port") == 0 &&
                  i + 1 < argc) {
            errno = 0;
            port = strtoul(argv[++i], &end, 10);

            if(errno != 0 ||
               *end != '\0' ||
               end == argv[i] ||
               port > 65535) {
                goto usage;
            }
        } else {
            goto usage;
        }
    }

    if((dev &&
        ((genesis_path == NULL) !=
         (transaction_path == NULL))) ||
       (!dev && transaction_path != NULL)) {
        goto usage;
    }

    if(once && candidates.count != 0) {
        goto usage;
    }

    genesis = (uint8_t *)malloc(STN_BLOCK_MAX_SIZE);
    body = (uint8_t *)malloc(STN_BLOCK_MAX_BODY);
    mining.template_bytes =
        (uint8_t *)malloc(STN_BLOCK_MAX_SIZE);

    if(genesis == NULL ||
       body == NULL ||
       mining.template_bytes == NULL) {
        goto cleanup;
    }

    /* One node owns this history for its entire lifetime, including bootstrap. */
    if(strlen(data) > SIZE_MAX - sizeof(".node.lock")) { goto cleanup; }
    instance_path = malloc(strlen(data) + sizeof(".node.lock"));
    if(instance_path == NULL) { goto cleanup; }
    strcpy(instance_path, data);
    strcat(instance_path, ".node.lock");
    instance_lock = open(instance_path, O_CREAT | O_RDWR | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600);
    if(instance_lock < 0 || flock(instance_lock, LOCK_EX | LOCK_NB) != 0) {
        fprintf(stderr, "Cannot own data path: directory unavailable or node already running.\n");
        goto cleanup;
    }

    {
        struct stat info;
        if(fstat(instance_lock, &info) != 0 || !S_ISREG(info.st_mode)) {
            fprintf(stderr, "Node ownership lock is not a regular file.\n");
            goto cleanup;
        }
    }
    if(genesis_path == NULL) {
        int existing = dev ? 0 : history_genesis(data, genesis, &genesis_length);
        if(existing < 0) {
            fprintf(stderr, "Cannot read existing history genesis; history was not replaced.\n");
            goto cleanup;
        }
        if(existing == 0) {
            development_genesis(genesis);
            genesis_length = 364;
        }
        if(dev) {
            memcpy(body, genesis + 168, 196);
            transaction_length = 192;
        }
    } else {
        if(!read_file(
               genesis_path,
               genesis,
               STN_BLOCK_MAX_SIZE,
               &genesis_length) ||
           (dev &&
            !read_file(
                transaction_path,
                body + 4,
                STN_TX_MAX_SIZE,
                &transaction_length))) {
            fprintf(
                stderr,
                "Cannot read genesis/transaction.\n");
            goto cleanup;
        }

        stn_wire_write(
            body,
            4,
            transaction_length);
    }

    if(stn_block_decode(
           genesis,
           genesis_length,
           &decoded) != STN_DATA_OK ||
       decoded.header.version != 3) {
        fprintf(stderr, "Invalid v3 genesis.\n");
        goto cleanup;
    }

    memcpy(
        chain.network_id,
        decoded.header.network_id,
        32);

    memcpy(
        policy.fixed_target,
        decoded.header.reserved_target,
        32);

    chain.genesis_bytes = genesis;
    chain.genesis_length = genesis_length;
    chain.pow_policy = &policy;
    chain.hash_provider.hash = stn_sha256;

    if(dev &&
       (stn_block_body_validate_structure(
            body,
            4 + transaction_length,
            1) != STN_DATA_OK ||
        memcmp(
            body + 24,
            chain.network_id,
            32) != 0)) {
        fprintf(
            stderr,
            "Invalid selected transaction or network.\n");
        goto cleanup;
    }

    if(stn_linux_storage_init(
           &disk,
           data,
           &storage) != STN_STORAGE_OK) {
        fprintf(
            stderr,
            "Data path requires a trusted existing local directory.\n");
        goto cleanup;
    }

    {
        size_t capacity =
            initial_storage_capacity(data);

        if(capacity <
           STN_STORAGE_OVERHEAD +
           4u +
           genesis_length) {
            capacity =
                STN_STORAGE_OVERHEAD +
                4u +
                genesis_length;
        }

        mining.snapshot =
            (uint8_t *)malloc(capacity);

        mining.workspace.current_bytes =
            (uint8_t *)malloc(capacity);

        mining.workspace.next_bytes =
            (uint8_t *)malloc(capacity);

        if(mining.snapshot == NULL ||
           mining.workspace.current_bytes == NULL ||
           mining.workspace.next_bytes == NULL) {
            goto cleanup;
        }

        mining.snapshot_capacity = capacity;
        mining.workspace.current_capacity = capacity;
        mining.workspace.next_capacity = capacity;
    }

    mining.chain = &chain;
    mining.storage = &storage;
    mining.body = body;
    mining.body_length = 4 + transaction_length;
    mining.transaction_count = 1;

    if(!dev) {
        mining.pending = &pending;
        mining.pending_body = body;
        mining.pending_body_capacity =
            STN_BLOCK_MAX_BODY;
        mining.body = NULL;
        mining.body_length = 0;
        mining.transaction_count = 0;
    }

    mining.template_capacity = STN_BLOCK_MAX_SIZE;
    mining.owns_buffers = 1;

#ifdef STN_PHASE9_TEST_RUNTIME
    if(!phase9_setup(&mining)) {
        fprintf(
            stderr,
            "Test runtime requires its private stop event.\n");
        goto cleanup;
    }
#endif

    status = stn_storage_load(
        &chain,
        &storage,
        mining.snapshot,
        mining.snapshot_capacity,
        &view);

    if(status == STN_STORAGE_NOT_FOUND) {
        stn_block_span anchor = {
            genesis,
            genesis_length
        };

        status = stn_storage_create(
            &chain,
            &storage,
            &anchor,
            1,
            mining.snapshot,
            mining.snapshot_capacity,
            &mining.active);
    } else if(status == STN_STORAGE_OK) {
        stn_chain_state_move(
            &mining.active,
            &view.state);

        stn_storage_view_release(&view);
    }

    if(status != STN_STORAGE_OK) {
        fprintf(
            stderr,
            "Storage startup failed: %d (no automatic repair).\n",
            (int)status);
        if(status == STN_STORAGE_VALIDATION) {
            stn_storage_status read_status;
            size_t diagnostic_length = 0u;
            stn_storage_view diagnostic_view = {0};

            read_status = storage.acquire(storage.user);
            if(read_status == STN_STORAGE_OK) {
                read_status = storage.read(
                    storage.user,
                    mining.snapshot,
                    mining.snapshot_capacity,
                    &diagnostic_length);
                storage.release(storage.user);
            }

            if(read_status == STN_STORAGE_OK) {
                read_status = stn_storage_decode(
                    &chain,
                    mining.snapshot,
                    diagnostic_length,
                    &diagnostic_view);
                stn_storage_view_release(&diagnostic_view);
            }

            /*
             * Storage decode deliberately exposes only storage status. For a
             * validation failure, decode the immutable STNS block table here
             * and replay it once so startup can report the exact consensus
             * rejection without modifying or repairing accepted history.
             */
            if(read_status == STN_STORAGE_VALIDATION &&
               diagnostic_length >= STN_STORAGE_OVERHEAD &&
               memcmp(mining.snapshot,"STNS",4) == 0) {
                uint32_t block_count = (uint32_t)stn_wire_read(mining.snapshot + 8,4);
                stn_block_span *diagnostic_blocks = NULL;
                size_t diagnostic_offset = STN_STORAGE_HEADER;
                size_t diagnostic_count = 0u;
                int diagnostic_shape_ok = block_count != 0u;

                if(diagnostic_shape_ok)
                    diagnostic_blocks = (stn_block_span *)calloc(
                        block_count,sizeof(*diagnostic_blocks));
                if(diagnostic_blocks == NULL)
                    diagnostic_shape_ok = 0;

                while(diagnostic_shape_ok && diagnostic_count < block_count) {
                    uint32_t block_length;
                    if(diagnostic_offset > diagnostic_length - 32u ||
                       diagnostic_length - 32u - diagnostic_offset < 4u) {
                        diagnostic_shape_ok = 0;
                        break;
                    }
                    block_length = (uint32_t)stn_wire_read(mining.snapshot + diagnostic_offset,4);
                    diagnostic_offset += 4u;
                    if(block_length < STN_BLOCK_HEADER_SIZE ||
                       diagnostic_offset > diagnostic_length - 32u ||
                       block_length > diagnostic_length - 32u - diagnostic_offset) {
                        diagnostic_shape_ok = 0;
                        break;
                    }
                    diagnostic_blocks[diagnostic_count].bytes =
                        mining.snapshot + diagnostic_offset;
                    diagnostic_blocks[diagnostic_count].length = block_length;
                    diagnostic_offset += block_length;
                    ++diagnostic_count;
                }

                if(diagnostic_shape_ok &&
                   diagnostic_offset == diagnostic_length - 32u) {
                    stn_chain_state diagnostic_state = {0};
                    stn_chain_report report = stn_chain_reconstruct_history(
                        &chain,
                        diagnostic_blocks,
                        diagnostic_count,
                        &diagnostic_state);
                    if(report.acceptance != STN_ACCEPTANCE_UNDER_CONTEXT) {
                        fprintf(
                            stderr,
                            "History validation failed: index=%zu height=%llu reason=%d detail=%d"
                            " structure=%d link=%d body=%d identifier=%d target=%d pow=%d work=%d.\n",
                            report.failing_index,
                            (unsigned long long)report.failing_height,
                            (int)report.reason,
                            (int)report.detail,
                            (int)report.structure,
                            (int)report.link,
                            (int)report.body,
                            (int)report.identifier,
                            (int)report.target,
                            (int)report.pow,
                            (int)report.work);
                        if(report.failing_index < diagnostic_count) {
                            const stn_block_span *failed =
                                &diagnostic_blocks[report.failing_index];
                            if(failed->length >= STN_BLOCK_HEADER_SIZE) {
                                const uint8_t *fb = failed->bytes;
                                uint32_t tx_count =
                                    (uint32_t)stn_wire_read(fb + 160,4);
                                uint32_t body_length =
                                    (uint32_t)stn_wire_read(fb + 164,4);
                                fprintf(
                                    stderr,
                                    "Failed block raw header: stored_length=%zu magic=%02x%02x%02x%02x"
                                    " version=%u flags=%u height=%llu tx_count=%u body_length=%u.\n",
                                    failed->length,
                                    fb[0],fb[1],fb[2],fb[3],
                                    (unsigned)stn_wire_read(fb + 4,2),
                                    (unsigned)stn_wire_read(fb + 6,2),
                                    (unsigned long long)stn_wire_read(fb + 72,8),
                                    (unsigned)tx_count,
                                    (unsigned)body_length);
                                if(failed->length > STN_BLOCK_HEADER_SIZE &&
                                   tx_count != 0u &&
                                   failed->length - STN_BLOCK_HEADER_SIZE >= 4u) {
                                    uint32_t tx_length =
                                        (uint32_t)stn_wire_read(
                                            fb + STN_BLOCK_HEADER_SIZE,4);
                                    fprintf(
                                        stderr,
                                        "Failed block first transaction: length=%u"
                                        " magic=%02x%02x%02x%02x version=%u type=%u record_length=%u.\n",
                                        (unsigned)tx_length,
                                        failed->length >= STN_BLOCK_HEADER_SIZE + 16u ?
                                            fb[STN_BLOCK_HEADER_SIZE + 4u] : 0u,
                                        failed->length >= STN_BLOCK_HEADER_SIZE + 16u ?
                                            fb[STN_BLOCK_HEADER_SIZE + 5u] : 0u,
                                        failed->length >= STN_BLOCK_HEADER_SIZE + 16u ?
                                            fb[STN_BLOCK_HEADER_SIZE + 6u] : 0u,
                                        failed->length >= STN_BLOCK_HEADER_SIZE + 16u ?
                                            fb[STN_BLOCK_HEADER_SIZE + 7u] : 0u,
                                        failed->length >= STN_BLOCK_HEADER_SIZE + 16u ?
                                            (unsigned)stn_wire_read(
                                                fb + STN_BLOCK_HEADER_SIZE + 8u,2) : 0u,
                                        failed->length >= STN_BLOCK_HEADER_SIZE + 16u ?
                                            (unsigned)stn_wire_read(
                                                fb + STN_BLOCK_HEADER_SIZE + 10u,2) : 0u,
                                        failed->length >= STN_BLOCK_HEADER_SIZE + 16u ?
                                            (unsigned)stn_wire_read(
                                                fb + STN_BLOCK_HEADER_SIZE + 12u,4) : 0u);
                                }
                            }
                        }
                    }
                    stn_chain_state_release(&diagnostic_state);
                }
                free(diagnostic_blocks);
            }
        }
        goto cleanup;
    }

    if(stn_linux_peer_listen(
           (uint16_t)port,
           &listener,
           &bound) != STN_PEER_OK) {
        fprintf(
            stderr,
            "Cannot bind RPC port.\n");
        goto cleanup;
    }

    if(!install_signal_handlers()) {
        goto cleanup;
    }

    if(pthread_mutex_init(
           &dispatch_lock,
           NULL) != 0) {
        goto cleanup;
    }

    lock_ready = 1;

    printf(
        "STN Chain node; RPC 0.0.0.0:%u; height %llu\n",
        (unsigned)bound,
        (unsigned long long)mining.active.height);

    if(dev) {
        puts(
            "DEVELOPMENT FIXTURE: repeated structural test transaction; "
            "no signed intelligence admission or coin.");
    }

    puts(
        "RPC v2 binary STNC; concurrent clients limited only by "
        "host resources; read + solved-work submission. Ctrl+C stops.");

    fflush(stdout);

    result = EXIT_SUCCESS;

    if(candidates.count != 0) {
        size_t capacity = 8u * 1024u * 1024u;
        stn_peer_connector connector = {
            &outbound.socket,
            stn_linux_peer_open_candidate,
            stn_linux_peer_close_candidate
        };

        if(mining.snapshot_capacity > capacity) {
            capacity = mining.snapshot_capacity;
        }

        outbound.workspace.storage.current_bytes =
            (uint8_t *)malloc(capacity);

        outbound.workspace.storage.current_capacity =
            capacity;

        outbound.workspace.storage.next_bytes =
            (uint8_t *)malloc(capacity);

        outbound.workspace.storage.next_capacity =
            capacity;

        outbound.workspace.candidate =
            (uint8_t *)malloc(capacity);

        outbound.workspace.candidate_capacity =
            capacity;

        outbound.workspace.frame =
            (uint8_t *)malloc(STN_PEER_MAX_FRAME);

        outbound.workspace.frame_capacity =
            STN_PEER_MAX_FRAME;

        outbound.workspace.pending = mining.pending;
        outbound.mining = &mining;
        outbound.lock = &dispatch_lock;

        if(outbound.workspace.storage.current_bytes == NULL ||
           outbound.workspace.storage.next_bytes == NULL ||
           outbound.workspace.candidate == NULL ||
           outbound.workspace.frame == NULL ||
           stn_peer_outbound_init(
               &outbound.manager,
               &candidates,
               &connector) != STN_PEER_OK) {
            result = EXIT_FAILURE;
            goto shutdown;
        }

        if(pthread_create(
               &outbound.thread,
               NULL,
               outbound_thread,
               &outbound) != 0) {
            result = EXIT_FAILURE;
            goto shutdown;
        }

        outbound.thread_started = 1;
    }

    if(once) {
        if(!serve_once(
            &listener,
            &service)) {
            result = EXIT_FAILURE;
        }

        goto shutdown;
    }

    while(!stopping) {
        stn_linux_peer peer;
        stn_peer_transport transport;
        rpc_client *client;
        stn_peer_status accepted;

        memset(&peer, 0, sizeof(peer));
        peer.socket = -1;

        accepted = stn_linux_peer_accept(
            &listener,
            APP_RPC_ACCEPT_POLL_MS,
            &peer,
            &transport);

        if(accepted == STN_PEER_TIMEOUT) {
            reap_clients(&clients);
            continue;
        }

        if(accepted != STN_PEER_OK) {
            reap_clients(&clients);
            sleep_ms(APP_RPC_ACCEPT_POLL_MS);
            continue;
        }

        peer.io_timeout_ms = APP_RPC_IO_TIMEOUT_MS;

        client =
            (rpc_client *)calloc(
                1,
                sizeof(*client));

        if(client == NULL) {
            stn_linux_peer_close(&peer);
            continue;
        }

        client->peer = peer;
        client->transport = transport;
        client->transport.user = &client->peer;
        client->service = &service;
        client->dispatch_lock = &dispatch_lock;

        if(pthread_mutex_init(
               &client->done_lock,
               NULL) != 0) {
            stn_linux_peer_close(
                &client->peer);
            free(client);
            continue;
        }

        if(pthread_create(
               &client->thread,
               NULL,
               rpc_client_thread,
               client) != 0) {
            pthread_mutex_destroy(
                &client->done_lock);

            stn_linux_peer_close(
                &client->peer);

            free(client);
            continue;
        }

        client->next = clients;
        clients = client;

        reap_clients(&clients);
    }

shutdown:
    stopping = 1;

    /*
     * Close listener before joining workers so no new RPC sessions may enter
     * during shutdown.
     */
    stn_linux_peer_close(&listener);

    if(outbound.thread_started) {
        (void)pthread_join(
            outbound.thread,
            NULL);
    }

    stop_clients(&clients);

cleanup:
    stn_linux_peer_close(&listener);

    if(lock_ready) {
        pthread_mutex_destroy(
            &dispatch_lock);
    }

    free(
        outbound.workspace.storage.current_bytes);

    free(
        outbound.workspace.storage.next_bytes);

    free(outbound.workspace.candidate);
    free(outbound.workspace.frame);

    stn_chain_state_release(
        &mining.active);

    stn_pending_clear(&pending);

    free(genesis);
    free(body);
    free(mining.snapshot);
    free(mining.workspace.current_bytes);
    free(mining.workspace.next_bytes);
    free(mining.template_bytes);
    if(instance_lock >= 0) { close(instance_lock); }
    free(instance_path);

    return result;

usage:
    puts(
        "Usage: stn-chain [--data PATH] [--rpc-port 18473]\n"
        "Resume saved history or initialize the built-in genesis if absent.\n"
        "Optional --dev enables the repeated development transaction.\n"
        "   or: stn-chain --genesis BLOCK --data PATH [--rpc-port PORT]\n"
        "Explicit selected content: --dev --genesis BLOCK "
        "--transaction STNT.\n"
        "Repeat --peer IPv4:PORT for automatic outbound P2P "
        "(not with --once).\n"
        "RPC listens on all IPv4 interfaces. --once serves one connection. "
        "Port 0 chooses a free port.");

    return result;
}
