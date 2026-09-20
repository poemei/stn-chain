/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. Linux-only adapter. */
#include "stn_sha256.h"

#include <openssl/evp.h>
#include <string.h>

stn_data_status stn_sha256(void *user,
    const uint8_t *domain,
    size_t domain_length,
    const uint8_t *bytes,
    size_t length,
    uint8_t digest[32])
{
    EVP_MD_CTX *context = NULL;
    uint8_t temporary[32];
    unsigned int digest_length = 0;
    stn_data_status result = STN_DATA_PROVIDER_ERROR;

    (void)user;

    if(digest == NULL ||
       (domain_length != 0 && domain == NULL) ||
       (length != 0 && bytes == NULL)) {
        return STN_DATA_ARGUMENT;
    }

    context = EVP_MD_CTX_new();
    if(context == NULL) {
        goto done;
    }

    if(EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1) {
        goto done;
    }

    if(domain_length != 0 &&
       EVP_DigestUpdate(context, domain, domain_length) != 1) {
        goto done;
    }

    if(length != 0 &&
       EVP_DigestUpdate(context, bytes, length) != 1) {
        goto done;
    }

    if(EVP_DigestFinal_ex(context, temporary, &digest_length) != 1 ||
       digest_length != 32) {
        goto done;
    }

    result = STN_DATA_OK;

done:
    EVP_MD_CTX_free(context);

    if(result == STN_DATA_OK) {
        memcpy(digest, temporary, 32);
    }

    return result;
}