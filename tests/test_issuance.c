/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_issuance.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"issuance line %d: %s\n",__LINE__,#e);} } while(0)

static void fill(stn_issuance_record *r,uint8_t reason,uint64_t units)
{
    size_t i;
    memset(r,0,sizeof(*r));
    r->reason=reason;r->units=units;
    r->destination.mining_identity.type=STN_ADDRESS_IDENTITY;
    r->destination.wallet.type=STN_ADDRESS_WALLET;
    for(i=0;i<32u;++i){
        r->evidence_id[i]=(uint8_t)(0x20u+i);
        r->destination.mining_identity.identifier[i]=(uint8_t)i;
        r->destination.wallet.identifier[i]=(uint8_t)(0xffu-i);
    }
}

static void vectors(void)
{
    stn_issuance_record in,out={0},saved;
    uint8_t canonical[STN_ISSUANCE_CANONICAL_SIZE],before[STN_ISSUANCE_CANONICAL_SIZE];

    fill(&in,STN_ISSUANCE_REASON_SHARE,STN_ISSUANCE_SHARE_UNITS);
    CHECK(stn_issuance_encode(&in,canonical)==STN_DATA_OK);
    CHECK(canonical[0]==STN_ISSUANCE_VERSION && canonical[1]==STN_ISSUANCE_REASON_SHARE);
    CHECK(canonical[8]==0u && canonical[9]==100u);
    CHECK(memcmp(canonical+10u,in.evidence_id,32u)==0);
    CHECK(memcmp(canonical+42u,in.destination.mining_identity.identifier,32u)==0);
    CHECK(memcmp(canonical+74u,in.destination.wallet.identifier,32u)==0);
    CHECK(stn_issuance_decode(canonical,sizeof(canonical),&out)==STN_DATA_OK);
    CHECK(out.reason==STN_ISSUANCE_REASON_SHARE && out.units==STN_ISSUANCE_SHARE_UNITS);
    CHECK(memcmp(out.evidence_id,in.evidence_id,32u)==0);
    CHECK(memcmp(out.destination.mining_identity.identifier,in.destination.mining_identity.identifier,32u)==0);
    CHECK(memcmp(out.destination.wallet.identifier,in.destination.wallet.identifier,32u)==0);

    fill(&in,STN_ISSUANCE_REASON_BLOCK,STN_ISSUANCE_BLOCK_UNITS);
    CHECK(stn_issuance_encode(&in,canonical)==STN_DATA_OK);
    CHECK(canonical[1]==STN_ISSUANCE_REASON_BLOCK && canonical[8]==0x27u && canonical[9]==0x10u);
    CHECK(stn_issuance_decode(canonical,sizeof(canonical),&out)==STN_DATA_OK);
    CHECK(out.reason==STN_ISSUANCE_REASON_BLOCK && out.units==STN_ISSUANCE_BLOCK_UNITS);

    memset(before,0xa5,sizeof(before));memcpy(canonical,before,sizeof(canonical));
    fill(&in,STN_ISSUANCE_REASON_SHARE,STN_ISSUANCE_SHARE_UNITS-1u);
    CHECK(stn_issuance_encode(&in,canonical)==STN_DATA_CONTENT);
    CHECK(memcmp(canonical,before,sizeof(canonical))==0);
    fill(&in,STN_ISSUANCE_REASON_BLOCK,STN_ISSUANCE_BLOCK_UNITS-1u);
    CHECK(stn_issuance_encode(&in,canonical)==STN_DATA_CONTENT);
    CHECK(memcmp(canonical,before,sizeof(canonical))==0);
    fill(&in,3u,STN_ISSUANCE_SHARE_UNITS);
    CHECK(stn_issuance_encode(&in,canonical)==STN_DATA_CONTENT);
    fill(&in,STN_ISSUANCE_REASON_SHARE,STN_ISSUANCE_SHARE_UNITS);
    in.destination.wallet.type=STN_ADDRESS_IDENTITY;
    CHECK(stn_issuance_encode(&in,canonical)==STN_DATA_TYPE);
    in.destination.wallet.type=STN_ADDRESS_WALLET;
    in.destination.mining_identity.type=STN_ADDRESS_WALLET;
    CHECK(stn_issuance_encode(&in,canonical)==STN_DATA_TYPE);
    CHECK(stn_issuance_encode(NULL,canonical)==STN_DATA_ARGUMENT);
    CHECK(stn_issuance_encode(&in,NULL)==STN_DATA_ARGUMENT);

    fill(&in,STN_ISSUANCE_REASON_SHARE,STN_ISSUANCE_SHARE_UNITS);
    CHECK(stn_issuance_encode(&in,canonical)==STN_DATA_OK);
    saved=out;
    CHECK(stn_issuance_decode(canonical,sizeof(canonical)-1u,&out)==STN_DATA_LENGTH);
    CHECK(memcmp(&out,&saved,sizeof(out))==0);
    canonical[0]=2u;
    CHECK(stn_issuance_decode(canonical,sizeof(canonical),&out)==STN_DATA_VERSION);
    CHECK(memcmp(&out,&saved,sizeof(out))==0);
    canonical[0]=STN_ISSUANCE_VERSION;canonical[1]=9u;
    CHECK(stn_issuance_decode(canonical,sizeof(canonical),&out)==STN_DATA_CONTENT);
    CHECK(memcmp(&out,&saved,sizeof(out))==0);
    CHECK(stn_issuance_decode(NULL,sizeof(canonical),&out)==STN_DATA_ARGUMENT);
    CHECK(stn_issuance_decode(canonical,sizeof(canonical),NULL)==STN_DATA_ARGUMENT);
}

int test_issuance(void);
int test_issuance(void)
{
    vectors();
    printf("Economic issuance record: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}

#ifdef STN_ISSUANCE_TEST_MAIN
int main(void)
{
    return test_issuance();
}
#endif
