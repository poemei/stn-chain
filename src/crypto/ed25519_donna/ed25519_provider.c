/* Public-domain Ed25519-donna provider, isolated behind STN's API. */
#define ED25519_SUFFIX _stn
#define ED25519_REFHASH
#define ED25519_TEST
#define ED25519_FORCE_32BIT
#include "ed25519.c"
int stn_ed25519_verify(const unsigned char *m,size_t n,const unsigned char pk[32],const unsigned char sig[64])
{
    return ed25519_sign_open_stn(m,n,pk,sig);
}
