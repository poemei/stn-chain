/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_snapshot.h"
#include "stn_chain.h"

#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

int test_contract_snapshot(void)
{
    stn_contract_snapshot *a,*b,*shared;
    stn_contract_state_store *sa,*sb;
    stn_contract_participant p[1];
    stn_contract c={0};
    uint8_t draft[STN_CONTRACT_MAX_SIZE];
    size_t draft_length=0,index=99;

    memset(p,0,sizeof(p));memset(p[0].identity,0x44,32);
    p[0].role=STN_CONTRACT_ROLE_APPROVER;
    c.version=STN_CONTRACT_VERSION;c.type=STN_CONTRACT_GENERIC;
    c.state=STN_CONTRACT_STATE_DRAFT;c.participants=p;c.participant_count=1u;
    CHECK(stn_contract_encode(&c,draft,sizeof(draft),&draft_length)==STN_CONTRACT_OK);

    a=stn_contract_snapshot_create();CHECK(a!=NULL);
    sa=stn_contract_snapshot_state(a);CHECK(sa!=NULL);
    CHECK(stn_contract_snapshot_register(a,draft,draft_length,&index)==STN_CONTRACT_OK);
    CHECK(sa->entry_count==1u);
    CHECK(sa->entries[0].canonical_draft!=draft);
    {
        uint8_t saved=draft[0];
        draft[0]^=0xffu;
        CHECK(sa->entries[0].canonical_draft[0]!=draft[0]);
        draft[0]=saved;
    }

    b=stn_contract_snapshot_clone(a);CHECK(b!=NULL);
    sb=stn_contract_snapshot_state(b);CHECK(sb!=NULL);
    CHECK(sb!=sa && sb->entries!=sa->entries && sb->votes!=sa->votes);
    CHECK(sb->entry_count==1u);
    CHECK(memcmp(sb->entries[0].contract_id,sa->entries[0].contract_id,
        STN_ADDRESS_ID_SIZE)==0);
    CHECK(sb->entries[0].canonical_draft!=sa->entries[0].canonical_draft);
    CHECK(memcmp(sb->entries[0].canonical_draft,sa->entries[0].canonical_draft,
        sa->entries[0].canonical_draft_length)==0);

    sb->entries[0].current.sequence=7u;
    CHECK(sa->entries[0].current.sequence==0u);

    shared=stn_contract_snapshot_share(a);CHECK(shared==a);
    stn_contract_snapshot_release(shared);
    CHECK(stn_contract_snapshot_const_state(a)->entry_count==1u);

    stn_contract_snapshot_release(b);
    stn_contract_snapshot_release(a);

    {
        stn_chain_state owner={0},copy={0},moved={0};
        owner.contracts=stn_contract_snapshot_create();
        CHECK(owner.contracts!=NULL);
        CHECK(stn_chain_state_share(&owner,&copy)==STN_DATA_OK);
        CHECK(copy.contracts==owner.contracts);
        stn_chain_state_move(&moved,&copy);
        CHECK(copy.contracts==NULL && moved.contracts==owner.contracts);
        stn_chain_state_release(&owner);
        CHECK(moved.contracts!=NULL);
        stn_chain_state_release(&moved);
        CHECK(moved.contracts==NULL);
    }
    printf("Contract snapshot: %u checks, %u failures.\n",checks,failures);
    return failures!=0u;
}
#ifdef STN_CONTRACT_SNAPSHOT_TEST_MAIN
int main(void){return test_contract_snapshot();}
#endif
