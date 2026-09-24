/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_wallet.h"

#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"wallet line %d: %s\n",__LINE__,#e);} } while(0)

static void vectors(void)
{
    stn_wallet_binding binding={0},decoded={0},snapshot;
    stn_address wallet={0},again={0},wrong={0};
    uint8_t canonical[STN_WALLET_BINDING_CANONICAL_SIZE];
    uint8_t bad[STN_WALLET_BINDING_CANONICAL_SIZE];

    binding.identity.type=STN_ADDRESS_IDENTITY;
    memset(binding.identity.identifier,0x11,STN_ADDRESS_ID_SIZE);

    CHECK(stn_wallet_derive(&binding.identity,&wallet)==STN_DATA_OK);
    CHECK(wallet.type==STN_ADDRESS_WALLET);
    CHECK(stn_wallet_derive(&binding.identity,&again)==STN_DATA_OK);
    CHECK(memcmp(wallet.identifier,again.identifier,STN_ADDRESS_ID_SIZE)==0);

    binding.wallet=wallet;
    CHECK(stn_wallet_binding_encode(&binding,canonical)==STN_DATA_OK);
    CHECK(canonical[0]==STN_WALLET_VERSION);
    CHECK(memcmp(canonical+1u,binding.identity.identifier,STN_ADDRESS_ID_SIZE)==0);
    CHECK(memcmp(canonical+33u,wallet.identifier,STN_ADDRESS_ID_SIZE)==0);
    CHECK(stn_wallet_binding_decode(canonical,sizeof(canonical),&decoded)==STN_DATA_OK);
    CHECK(decoded.identity.type==STN_ADDRESS_IDENTITY);
    CHECK(decoded.wallet.type==STN_ADDRESS_WALLET);
    CHECK(memcmp(&decoded,&binding,sizeof(binding))==0);

    wrong=binding.identity;
    wrong.type=STN_ADDRESS_WALLET;
    snapshot=decoded;
    CHECK(stn_wallet_derive(&wrong,&again)==STN_DATA_TYPE);
    CHECK(stn_wallet_derive(NULL,&again)==STN_DATA_ARGUMENT);
    CHECK(stn_wallet_derive(&binding.identity,NULL)==STN_DATA_ARGUMENT);

    binding.identity.type=STN_ADDRESS_WALLET;
    CHECK(stn_wallet_binding_encode(&binding,canonical)==STN_DATA_TYPE);
    binding.identity.type=STN_ADDRESS_IDENTITY;
    binding.wallet.type=STN_ADDRESS_IDENTITY;
    CHECK(stn_wallet_binding_encode(&binding,canonical)==STN_DATA_TYPE);
    binding.wallet=wallet;
    binding.wallet.identifier[0]^=1u;
    CHECK(stn_wallet_binding_encode(&binding,canonical)==STN_DATA_CONTENT);
    binding.wallet=wallet;

    CHECK(stn_wallet_binding_encode(NULL,canonical)==STN_DATA_ARGUMENT);
    CHECK(stn_wallet_binding_encode(&binding,NULL)==STN_DATA_ARGUMENT);

    CHECK(stn_wallet_binding_encode(&binding,canonical)==STN_DATA_OK);
    memcpy(bad,canonical,sizeof(bad));
    bad[0]=2u;
    CHECK(stn_wallet_binding_decode(bad,sizeof(bad),&decoded)==STN_DATA_CONTENT);
    CHECK(memcmp(&decoded,&snapshot,sizeof(decoded))==0);
    memcpy(bad,canonical,sizeof(bad));
    bad[33]^=1u;
    CHECK(stn_wallet_binding_decode(bad,sizeof(bad),&decoded)==STN_DATA_CONTENT);
    CHECK(memcmp(&decoded,&snapshot,sizeof(decoded))==0);
    CHECK(stn_wallet_binding_decode(canonical,sizeof(canonical)-1u,&decoded)==STN_DATA_LENGTH);
    CHECK(stn_wallet_binding_decode(NULL,sizeof(canonical),&decoded)==STN_DATA_ARGUMENT);
    CHECK(stn_wallet_binding_decode(canonical,sizeof(canonical),NULL)==STN_DATA_ARGUMENT);
}

int test_wallet(void);
int test_wallet(void)
{
    vectors();
    printf("Wallet primitive: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}

#ifdef STN_WALLET_TEST_MAIN
int main(void){return test_wallet();}
#endif
