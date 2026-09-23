/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_consensus.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

int test_contract_consensus(void)
{
    stn_contract c={0};
    stn_contract_participant p[3];
    uint8_t bytes[STN_CONTRACT_MAX_SIZE],votes[3*STN_CONTRACT_VOTE_KEY_SIZE];
    uint8_t outsider[32];
    size_t written=0;
    stn_contract_vote_state s,before;
    stn_contract current,next,unchanged;
    int reached=-1;
    memset(p,0,sizeof(p));memset(outsider,0x44,sizeof(outsider));
    memset(p[0].identity,0x11,32);p[0].role=STN_CONTRACT_ROLE_APPROVER;
    memset(p[1].identity,0x22,32);p[1].role=STN_CONTRACT_ROLE_APPROVER;
    memset(p[2].identity,0x33,32);p[2].role=STN_CONTRACT_ROLE_APPROVER;
    c.version=STN_CONTRACT_VERSION;c.type=STN_CONTRACT_GENERIC;
    c.state=STN_CONTRACT_STATE_DRAFT;c.participants=p;c.participant_count=3;
    CHECK(stn_contract_encode(&c,bytes,sizeof(bytes),&written)==STN_CONTRACT_OK);
    memset(votes,0,sizeof(votes));
    CHECK(stn_contract_vote_state_initialize(&s,bytes,written,votes,3)==STN_CONTRACT_OK);
    CHECK(s.eligible_count==3u);
    CHECK(s.required_count==2u);
    CHECK(s.accepted_count==0u);
    CHECK(stn_contract_vote_eligible(bytes,written,p[0].identity)==STN_CONTRACT_OK);
    CHECK(stn_contract_vote_eligible(bytes,written,outsider)==STN_CONTRACT_AUTHORITY_ERROR);
    CHECK(stn_contract_vote_accept(&s,bytes,written,p[0].identity,&reached)==STN_CONTRACT_OK);
    CHECK(reached==0);
    CHECK(s.accepted_count==1u);
    before=s;
    CHECK(stn_contract_vote_accept(&s,bytes,written,p[0].identity,&reached)==STN_CONTRACT_DUPLICATE_APPROVAL);
    CHECK(s.accepted_count==before.accepted_count);
    CHECK(stn_contract_vote_accept(&s,bytes,written,p[1].identity,&reached)==STN_CONTRACT_OK);
    CHECK(reached==1);
    CHECK(s.accepted_count==2u);
    CHECK(stn_contract_vote_accept(&s,bytes,written,p[2].identity,&reached)==STN_CONTRACT_OK);
    CHECK(reached==1);
    CHECK(s.accepted_count==3u);

    c.sequence=1u;
    CHECK(stn_contract_encode(&c,bytes,sizeof(bytes),&written)==STN_CONTRACT_OK);
    before=s;
    CHECK(stn_contract_vote_accept(&s,bytes,written,p[0].identity,&reached)==STN_CONTRACT_STATE_ERROR);
    CHECK(memcmp(&s,&before,sizeof(s))==0);

    c.sequence=0u;c.state=STN_CONTRACT_STATE_ISSUED;
    CHECK(stn_contract_encode(&c,bytes,sizeof(bytes),&written)==STN_CONTRACT_OK);
    CHECK(stn_contract_vote_state_initialize(&before,bytes,written,votes,3)==STN_CONTRACT_STATE_ERROR);

    c.state=STN_CONTRACT_STATE_DRAFT;c.participant_count=1;c.participants=&p[0];
    CHECK(stn_contract_encode(&c,bytes,sizeof(bytes),&written)==STN_CONTRACT_OK);
    CHECK(stn_contract_vote_state_initialize(&s,bytes,written,votes,1)==STN_CONTRACT_OK);
    CHECK(s.required_count==1u);

    p[0].role=STN_CONTRACT_ROLE_PARTICIPANT;
    CHECK(stn_contract_encode(&c,bytes,sizeof(bytes),&written)==STN_CONTRACT_OK);
    CHECK(stn_contract_vote_state_initialize(&s,bytes,written,votes,1)==STN_CONTRACT_AUTHORITY_ERROR);


    /* Consensus-aware lifecycle: first accepted vote enters APPROVALS but does
     * not approve; the strict-majority vote advances to ATTESTATION. */
    c.state=STN_CONTRACT_STATE_DRAFT;c.sequence=0u;c.participant_count=3;c.participants=p;
    p[0].role=STN_CONTRACT_ROLE_APPROVER;
    CHECK(stn_contract_encode(&c,bytes,sizeof(bytes),&written)==STN_CONTRACT_OK);
    memset(votes,0,sizeof(votes));
    CHECK(stn_contract_vote_state_initialize(&s,bytes,written,votes,3)==STN_CONTRACT_OK);
    current=c;current.state=STN_CONTRACT_STATE_REVIEW;current.sequence=7u;
    memset(&next,0,sizeof(next));
    CHECK(stn_contract_vote_apply(&current,8u,bytes,written,p[0].identity,&s,&next)==STN_CONTRACT_OK);
    CHECK(next.sequence==8u);
    CHECK(next.state==STN_CONTRACT_STATE_APPROVALS);
    CHECK(s.accepted_count==1u);
    current=next;
    CHECK(stn_contract_vote_apply(&current,9u,bytes,written,p[1].identity,&s,&next)==STN_CONTRACT_OK);
    CHECK(next.sequence==9u);
    CHECK(next.state==STN_CONTRACT_STATE_ATTESTATION);
    CHECK(s.accepted_count==2u);

    unchanged=next;
    before=s;
    CHECK(stn_contract_vote_apply(&current,9u,bytes,written,p[0].identity,&s,&next)==STN_CONTRACT_DUPLICATE_APPROVAL);
    CHECK(memcmp(&next,&unchanged,sizeof(next))==0);
    CHECK(s.accepted_count==before.accepted_count);

    printf("Contract consensus: %u checks, %u failures.\n",checks,failures);
    return failures!=0u;
}
#ifdef STN_CONTRACT_CONSENSUS_TEST_MAIN
int main(void){return test_contract_consensus();}
#endif
