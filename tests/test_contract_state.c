/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_state.h"

#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

int test_contract_state(void)
{
    stn_contract_state_store store;
    stn_contract_state_entry entries[2];
    uint8_t votes[6*STN_CONTRACT_VOTE_KEY_SIZE];
    stn_contract_participant p[3];
    stn_contract c={0},current;
    uint8_t draft[STN_CONTRACT_MAX_SIZE],now[STN_CONTRACT_MAX_SIZE];
    size_t draft_length=0,now_length=0,index=99,found=99;
    uint8_t id[STN_ADDRESS_ID_SIZE];

    memset(p,0,sizeof(p));
    memset(p[0].identity,0x11,32);p[0].role=STN_CONTRACT_ROLE_APPROVER;
    memset(p[1].identity,0x22,32);p[1].role=STN_CONTRACT_ROLE_APPROVER;
    memset(p[2].identity,0x33,32);p[2].role=STN_CONTRACT_ROLE_APPROVER;
    c.version=STN_CONTRACT_VERSION;c.type=STN_CONTRACT_GENERIC;
    c.state=STN_CONTRACT_STATE_DRAFT;c.participants=p;c.participant_count=3;
    CHECK(stn_contract_encode(&c,draft,sizeof(draft),&draft_length)==STN_CONTRACT_OK);

    stn_contract_state_initialize(&store,entries,2,votes,6);
    CHECK(stn_contract_state_register(&store,draft,draft_length,&index)==STN_CONTRACT_OK);
    CHECK(index==0u && store.entry_count==1u);
    memcpy(id,entries[0].contract_id,sizeof(id));
    CHECK(stn_contract_state_find(&store,id,&found)==STN_CONTRACT_OK && found==0u);
    CHECK(entries[0].eligible_count==3u && entries[0].required_count==2u);
    CHECK(stn_contract_state_register(&store,draft,draft_length,&found)==STN_CONTRACT_ADDRESS_ERROR);

    current=c;current.state=STN_CONTRACT_STATE_REVIEW;current.sequence=7u;
    entries[0].current=current;
    CHECK(stn_contract_encode(&current,now,sizeof(now),&now_length)==STN_CONTRACT_OK);
    CHECK(stn_contract_state_apply_vote(&store,0,now,now_length,8u,p[0].identity)==STN_CONTRACT_OK);
    CHECK(entries[0].current.state==STN_CONTRACT_STATE_APPROVALS);
    CHECK(entries[0].current.sequence==8u && entries[0].vote_count==1u);

    current=entries[0].current;
    /*
     * Store state is a decoded-style borrowed view. Canonical wire bytes for
     * the next action are reconstructed from the immutable DRAFT payload with
     * only sequence/state changed; participant/terms bytes remain identical.
     */
    current.participants=p;
    CHECK(stn_contract_encode(&current,now,sizeof(now),&now_length)==STN_CONTRACT_OK);
    CHECK(stn_contract_state_apply_vote(&store,0,now,now_length,9u,p[1].identity)==STN_CONTRACT_OK);
    CHECK(entries[0].current.state==STN_CONTRACT_STATE_ATTESTATION);
    CHECK(entries[0].current.sequence==9u && entries[0].vote_count==2u);

    current=entries[0].current;current.state=STN_CONTRACT_STATE_APPROVALS;
    current.participants=p;
    CHECK(stn_contract_encode(&current,now,sizeof(now),&now_length)==STN_CONTRACT_OK);
    CHECK(stn_contract_state_apply_vote(&store,0,now,now_length,10u,p[0].identity)==STN_CONTRACT_STATE_ERROR);
    CHECK(entries[0].current.state==STN_CONTRACT_STATE_ATTESTATION &&
          entries[0].vote_count==2u);

    printf("Contract state: %u checks, %u failures.\n",checks,failures);
    return failures!=0u;
}
#ifdef STN_CONTRACT_STATE_TEST_MAIN
int main(void){return test_contract_state();}
#endif
