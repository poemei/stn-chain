/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONFIG_H
#define STN_CONFIG_H
#include "stn_address.h"

#define STN_CONFIG_MAX_BYTES 4096u

typedef struct stn_config {
    int internal_miner_enabled;
    stn_address miner_wallet;
    int has_miner_wallet;
} stn_config;

/* Strict, bounded JSON configuration. Unknown keys, duplicate keys, malformed
 * JSON and wrong address namespaces are rejected. No allocation is performed. */
stn_data_status stn_config_decode(const uint8_t *bytes,size_t length,stn_config *out);

#endif
