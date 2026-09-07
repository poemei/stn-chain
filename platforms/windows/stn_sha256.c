/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. Windows-only adapter. */
#include "stn_sha256.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <string.h>

stn_data_status stn_sha256(void *user,const uint8_t *domain,size_t domain_length,
    const uint8_t *bytes,size_t length,uint8_t digest[32])
{
    BCRYPT_ALG_HANDLE algorithm=NULL;
    BCRYPT_HASH_HANDLE hash=NULL;
    uint8_t temporary[32];
    NTSTATUS status;
    stn_data_status result=STN_DATA_PROVIDER_ERROR;
    (void)user;
    if(digest==NULL || (domain_length!=0 && domain==NULL) || (length!=0 && bytes==NULL)) { return STN_DATA_ARGUMENT; }
    /* CNG lengths are ULONG; reject rather than truncate. No input-sized allocation. */
    if(domain_length>UINT32_MAX || length>UINT32_MAX) { return STN_DATA_LENGTH; }
    status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,MS_PRIMITIVE_PROVIDER,0);
    if(status<0) { goto done; }
    status=BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,0);
    if(status<0) { goto done; }
    if(domain_length!=0 && BCryptHashData(hash,(PUCHAR)domain,(ULONG)domain_length,0)<0) { goto done; }
    if(length!=0 && BCryptHashData(hash,(PUCHAR)bytes,(ULONG)length,0)<0) { goto done; }
    if(BCryptFinishHash(hash,temporary,32,0)<0) { goto done; }
    result=STN_DATA_OK;
done:
    if(hash!=NULL && BCryptDestroyHash(hash)<0) { result=STN_DATA_PROVIDER_ERROR; }
    if(algorithm!=NULL && BCryptCloseAlgorithmProvider(algorithm,0)<0) { result=STN_DATA_PROVIDER_ERROR; }
    if(result==STN_DATA_OK) { memcpy(digest,temporary,32); }
    return result;
}
