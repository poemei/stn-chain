/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_query.h"

#include <stdio.h>
#include <string.h>

static void be16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void entry(stn_contract_state_entry *e,uint8_t idbyte,uint8_t who,
    uint16_t state,uint64_t created,uint8_t participant[STN_CONTRACT_PARTICIPANT_SIZE])
{
    memset(e,0,sizeof(*e));memset(e->contract_id,idbyte,STN_ADDRESS_ID_SIZE);
    memset(participant,who,STN_ADDRESS_ID_SIZE);be16(participant+STN_ADDRESS_ID_SIZE,STN_CONTRACT_ROLE_PARTICIPANT);
    e->current.version=STN_CONTRACT_VERSION;e->current.type=STN_CONTRACT_GENERIC;
    e->current.state=state;e->current.created_at=created;e->current.participant_count=1;
    e->current.participant_bytes=participant;
}

#ifdef STN_CONTRACT_QUERY_TEST_MAIN
int main(void)
{
    stn_contract_state_store store;stn_contract_state_entry entries[4];
    stn_contract_query_result out[4];uint8_t packed[4][STN_CONTRACT_PARTICIPANT_SIZE];
    uint8_t identity[STN_ADDRESS_ID_SIZE];size_t count=99;
    memset(identity,0x11,sizeof(identity));
    entry(&entries[0],0x40,0x11,STN_CONTRACT_STATE_CLOSED,400,packed[0]);
    entry(&entries[1],0x30,0x11,STN_CONTRACT_STATE_REVIEW,300,packed[1]);
    entry(&entries[2],0x20,0x22,STN_CONTRACT_STATE_APPROVALS,500,packed[2]);
    entry(&entries[3],0x10,0x11,STN_CONTRACT_STATE_APPROVALS,500,packed[3]);
    stn_contract_state_initialize(&store,entries,4,NULL,0);store.entry_count=4;
    if(stn_contract_query_identity(&store,identity,out,4,&count)!=STN_CONTRACT_OK)return 1;
    if(count!=3)return 2;
    if(out[0].contract_id[0]!=0x10 || out[1].contract_id[0]!=0x30 || out[2].contract_id[0]!=0x40)return 3;
    if(stn_contract_query_identity(&store,identity,out,1,&count)!=STN_CONTRACT_OK || count!=1 || out[0].contract_id[0]!=0x10)return 4;
    if(stn_contract_query_identity(NULL,identity,out,4,&count)!=STN_CONTRACT_ARGUMENT)return 5;
    puts("STN Contract query tests passed.");return 0;
}
#endif
