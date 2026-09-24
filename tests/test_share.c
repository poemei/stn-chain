/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_share.h"
#include "stn_sha256.h"
#include "stn_chain.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"share line %d: %s\n",__LINE__,#e);} } while(0)

static void make_header(uint8_t bytes[STN_SHARE_TEMPLATE_HEADER_SIZE])
{
    stn_hash_provider provider={stn_sha256,NULL};
    stn_block_header h={0};
    size_t written=0;
    h.version=STN_POW_BLOCK_VERSION;
    h.height=1;
    h.timestamp=1;
    h.previous_hash[31]=1;
    h.reserved_target[0]=0x0f;
    h.reserved_target[31]=0xff;
    h.transaction_count=0;
    h.body_length=0;
    CHECK(stn_block_body_commitment(NULL,0,0,&provider,h.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_header_encode(&h,bytes,STN_SHARE_TEMPLATE_HEADER_SIZE,&written)==STN_DATA_OK);
    CHECK(written==STN_SHARE_TEMPLATE_HEADER_SIZE);
}

static void set_work_id(stn_share_evidence *s)
{
    static const uint8_t domain[]="STN-CHAIN:WORK:ID:1";
    CHECK(stn_sha256(NULL,domain,sizeof(domain),s->template_header,
        STN_SHARE_TEMPLATE_HEADER_SIZE,s->work_id)==STN_DATA_OK);
}

static void vectors(void)
{
    stn_share_evidence s={0},changed;
    uint8_t canonical[STN_SHARE_CANONICAL_SIZE],saved[STN_SHARE_CANONICAL_SIZE];
    uint8_t id[32],id2[32],saved_id[32];
    size_t i;

    s.miner.type=STN_ADDRESS_IDENTITY;
    for(i=0;i<32u;++i){s.miner.identifier[i]=(uint8_t)(0xa0u+i);}
    s.nonce=UINT64_C(0x0102030405060708);
    make_header(s.template_header);
    memcpy(s.body_commitment,s.template_header+88,STN_SHARE_BODY_COMMITMENT_SIZE);
    set_work_id(&s);

    CHECK(stn_share_encode(&s,canonical)==STN_DATA_OK);
    CHECK(canonical[0]==STN_SHARE_VERSION);
    CHECK(memcmp(canonical+1,s.work_id,32)==0);
    CHECK(memcmp(canonical+33,s.miner.identifier,32)==0);
    CHECK(STN_SHARE_CANONICAL_SIZE==273u);
    CHECK(canonical[65]==1 && canonical[66]==2 && canonical[67]==3 &&
          canonical[68]==4 && canonical[69]==5 && canonical[70]==6 &&
          canonical[71]==7 && canonical[72]==8);
    CHECK(memcmp(canonical+73,s.template_header,STN_SHARE_TEMPLATE_HEADER_SIZE)==0);
    CHECK(memcmp(canonical+241,s.body_commitment,STN_SHARE_BODY_COMMITMENT_SIZE)==0);

    {
        stn_share_evidence decoded={0};
        stn_share_evidence saved_decoded;
        memset(&saved_decoded,0x5a,sizeof(saved_decoded));
        CHECK(stn_share_decode(canonical,sizeof(canonical),&decoded)==STN_DATA_OK);
        CHECK(memcmp(decoded.work_id,s.work_id,32)==0);
        CHECK(decoded.miner.type==STN_ADDRESS_IDENTITY);
        CHECK(memcmp(decoded.miner.identifier,s.miner.identifier,32)==0);
        CHECK(decoded.nonce==s.nonce);
        CHECK(memcmp(decoded.template_header,s.template_header,STN_SHARE_TEMPLATE_HEADER_SIZE)==0);
        CHECK(memcmp(decoded.body_commitment,s.body_commitment,STN_SHARE_BODY_COMMITMENT_SIZE)==0);

        decoded=saved_decoded;
        CHECK(stn_share_decode(canonical,sizeof(canonical)-1u,&decoded)==STN_DATA_LENGTH);
        CHECK(memcmp(&decoded,&saved_decoded,sizeof(decoded))==0);
        canonical[0]=(uint8_t)(STN_SHARE_VERSION+1u);
        CHECK(stn_share_decode(canonical,sizeof(canonical),&decoded)==STN_DATA_VERSION);
        CHECK(memcmp(&decoded,&saved_decoded,sizeof(decoded))==0);
        canonical[0]=(uint8_t)STN_SHARE_VERSION;
        CHECK(stn_share_decode(NULL,sizeof(canonical),&decoded)==STN_DATA_ARGUMENT);
        CHECK(stn_share_decode(canonical,sizeof(canonical),NULL)==STN_DATA_ARGUMENT);
    }

    CHECK(stn_share_id(&s,id)==STN_DATA_OK);
    CHECK(stn_share_id(&s,id2)==STN_DATA_OK && memcmp(id,id2,32)==0);
    changed=s;changed.nonce++;
    CHECK(stn_share_id(&changed,id2)==STN_DATA_OK && memcmp(id,id2,32)!=0);
    changed=s;changed.work_id[0]^=1u;
    CHECK(stn_share_id(&changed,id2)==STN_DATA_OK && memcmp(id,id2,32)!=0);
    changed=s;changed.miner.identifier[0]^=1u;
    CHECK(stn_share_id(&changed,id2)==STN_DATA_OK && memcmp(id,id2,32)!=0);
    changed=s;changed.template_header[119]^=1u;
    CHECK(stn_share_id(&changed,id2)==STN_DATA_OK && memcmp(id,id2,32)!=0);
    changed=s;changed.body_commitment[0]^=1u;
    CHECK(stn_share_id(&changed,id2)==STN_DATA_CONTENT);
    CHECK(memcmp(id2,id,32)!=0);

    memset(saved,0xa5,sizeof(saved));memcpy(canonical,saved,sizeof(saved));
    CHECK(stn_share_encode(NULL,canonical)==STN_DATA_ARGUMENT);
    CHECK(memcmp(canonical,saved,sizeof(saved))==0);
    CHECK(stn_share_encode(&s,NULL)==STN_DATA_ARGUMENT);
    changed=s;changed.miner.type=STN_ADDRESS_WALLET;
    CHECK(stn_share_encode(&changed,canonical)==STN_DATA_TYPE);
    CHECK(memcmp(canonical,saved,sizeof(saved))==0);

    memset(saved_id,0x5a,sizeof(saved_id));memcpy(id2,saved_id,sizeof(id2));
    CHECK(stn_share_id(NULL,id2)==STN_DATA_ARGUMENT);
    CHECK(memcmp(id2,saved_id,sizeof(id2))==0);
    CHECK(stn_share_id(&s,NULL)==STN_DATA_ARGUMENT);
    CHECK(stn_share_id(&changed,id2)==STN_DATA_TYPE);
    CHECK(memcmp(id2,saved_id,sizeof(id2))==0);
}

static void proof_vectors(void)
{
    stn_hash_provider provider={stn_sha256,NULL};
    stn_share_evidence s={0};
    stn_block_header h;
    uint8_t hash[32],saved[32],share_target[32];
    uint64_t nonce;

    s.miner.type=STN_ADDRESS_IDENTITY;
    memset(s.miner.identifier,0x44,32);
    make_header(s.template_header);
    memcpy(s.body_commitment,s.template_header+88,STN_SHARE_BODY_COMMITMENT_SIZE);
    set_work_id(&s);
    CHECK(stn_block_header_decode(s.template_header,STN_SHARE_TEMPLATE_HEADER_SIZE,&h)==STN_DATA_OK);
    CHECK(stn_economy_share_target(h.reserved_target,share_target)==STN_DATA_OK);

    for(nonce=0;;++nonce){
        uint8_t candidate[STN_SHARE_TEMPLATE_HEADER_SIZE];
        stn_block_header trial=h;
        size_t n=0;
        static const uint8_t block_domain[]="STN-CHAIN:BLOCK:ID:1";
        trial.reserved_work_nonce=nonce;
        CHECK(stn_block_header_encode(&trial,candidate,sizeof(candidate),&n)==STN_DATA_OK);
        CHECK(stn_sha256(NULL,block_domain,sizeof(block_domain),candidate,sizeof(candidate),hash)==STN_DATA_OK);
        if(stn_pow_compare(hash,share_target)==STN_DATA_OK){break;}
        CHECK(nonce!=UINT64_MAX);
    }
    s.nonce=nonce;
    CHECK(stn_share_verify_evidence(&s,&provider,hash)==STN_DATA_OK);
    CHECK(stn_share_verify(&s,s.template_header,sizeof(s.template_header),&provider,hash)==STN_DATA_OK);

    memset(saved,0xa5,32);
    {
        stn_share_evidence bad=s;uint8_t out[32];memcpy(out,saved,32);
        bad.work_id[0]^=1u;
        CHECK(stn_share_verify_evidence(&bad,&provider,out)==STN_DATA_CONTENT);
        CHECK(memcmp(out,saved,32)==0);
    }
    {
        stn_share_evidence bad=s;uint8_t out[32];memcpy(out,saved,32);
        bad.miner.type=STN_ADDRESS_WALLET;
        CHECK(stn_share_verify_evidence(&bad,&provider,out)==STN_DATA_TYPE);
        CHECK(memcmp(out,saved,32)==0);
    }
    {
        uint8_t wrong[STN_SHARE_TEMPLATE_HEADER_SIZE];
        memcpy(wrong,s.template_header,sizeof(wrong));wrong[119]^=1u;
        CHECK(stn_share_verify(&s,wrong,sizeof(wrong),&provider,hash)==STN_DATA_CONTENT);
    }
    CHECK(stn_share_verify_evidence(NULL,&provider,hash)==STN_DATA_ARGUMENT);
    CHECK(stn_share_verify(NULL,s.template_header,sizeof(s.template_header),&provider,hash)==STN_DATA_ARGUMENT);
    CHECK(stn_share_verify(&s,NULL,sizeof(s.template_header),&provider,hash)==STN_DATA_ARGUMENT);
    CHECK(stn_share_verify(&s,s.template_header,sizeof(s.template_header),NULL,hash)==STN_DATA_ARGUMENT);
    CHECK(stn_share_verify(&s,s.template_header,sizeof(s.template_header),&provider,NULL)==STN_DATA_ARGUMENT);
    CHECK(stn_share_verify(&s,s.template_header,STN_BLOCK_HEADER_SIZE-1,&provider,hash)==STN_DATA_LENGTH);
}

int test_share(void);
int test_share(void)
{
    vectors();
    proof_vectors();
    printf("Economy share evidence/proof: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}

#ifdef STN_SHARE_TEST_MAIN
int main(void){return test_share();}
#endif
