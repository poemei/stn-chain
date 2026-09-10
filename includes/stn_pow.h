/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_POW_H
#define STN_POW_H
#include "stn_block.h"
#define STN_POW_BLOCK_VERSION 3u
/* Fixed 32-byte unsigned big-endian target. Range 1 .. 2^255-1.
 * Version 2 remains reserved/rejected. Version 1 stays legacy non-PoW.
 * No compact encoding, alternate encodings or floating point. */
/* fixed_target retains its API name; it is the genesis/bootstrap target. */
typedef struct stn_pow_policy { uint8_t fixed_target[32]; } stn_pow_policy;
#define STN_WORK_SIZE 40u
/* Exact unsigned 320-bit canonical big-endian work, not a target/hash width.
 * At most 2^64 blocks * 2^255 work per block = 2^319; all valid heights fit. */
typedef struct stn_work { uint8_t bytes[STN_WORK_SIZE]; } stn_work;
/* Pure calculation; the Chain required-target boundary applies it to validated
 * branch history for ordinary validation and mining templates.
 * history is already accepted, decoded history in ascending height order:
 * no entries at H=0; the predecessor otherwise; exactly 60 entries at H%60=0.
 * Caller establishes acceptance/ancestry. This checks count, heights and targets.
 * Missing history is UNRESOLVED; malformed history is CONTENT/TARGET.
 * Bootstrap target applies to H<60. Output unchanged on failure; aliasing allowed.
 * Timestamps are canonical Unix seconds, never local clock samples. */
stn_data_status stn_target_next(uint64_t height,const uint8_t bootstrap[32],
    const stn_block_header *history,size_t count,uint8_t out[32]);
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
