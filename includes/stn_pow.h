/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_POW_H
#define STN_POW_H
#include "stn_transaction.h"
#define STN_POW_BLOCK_VERSION 3u
/* Fixed 32-byte unsigned big-endian target. Range 1 .. 2^255-1.
 * Version 2 remains reserved/rejected. Version 1 stays legacy non-PoW.
 * No compact encoding, alternate encodings, floating point or adjustment. */
typedef struct stn_pow_policy { uint8_t fixed_target[32]; } stn_pow_policy;
typedef struct stn_work { uint8_t bytes[32]; } stn_work;
stn_data_status stn_target_validate(const uint8_t *target,size_t length);
/* Exact unsigned hash <= target. No byte reversal. */
stn_data_status stn_pow_compare(const uint8_t hash[32],const uint8_t target[32]);
/* floor(2^256/(target+1)); unchanged output on failure. */
stn_data_status stn_target_work(const uint8_t target[32],stn_work *out);
/* Overflow rejects without wrapping or modifying output. Aliasing allowed. */
stn_data_status stn_work_add(const stn_work *a,const stn_work *b,stn_work *out);
/* Fixed-target expected cumulative work for height+1 blocks, including genesis. */
stn_data_status stn_work_at_height(const uint8_t target[32],uint64_t height,stn_work *out);
/* PoW hash IS block ID: single SHA-256(domain || canonical header).
 * Standalone verify needs v3 structure and valid target; policy equality is
 * checked separately by chain context. No mining or search loop. */
stn_data_status stn_pow_verify(const uint8_t *bytes,size_t length,
    const stn_hash_provider *provider,uint8_t digest[32]);
#endif
