/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md.
 * TEST ONLY: scripted provider assertions, NOT signature verification. */
#ifndef STN_PHASE9_TEST_RUNTIME
#error This fixture must never be compiled into the production executable.
#endif
static HANDLE phase9_stop_event;
static stn_validation_context phase9_validation;
static stn_stage_status phase9_signature(void *user,const uint8_t *domain,size_t dn,
    const uint8_t *bytes,size_t n,const uint8_t key[32],const uint8_t signature[64])
{
    (void)user;(void)domain;(void)dn;(void)bytes;(void)n;(void)key;
    return signature[0]==4 ? STN_STAGE_PASS : STN_STAGE_REJECT;
}
static stn_stage_status phase9_authority(void *user,const stn_record *record,const stn_intelligence *intel)
{
    (void)user;(void)intel;
    return record->signer_public_key[0]==2 ? STN_STAGE_PASS : STN_STAGE_UNRESOLVED;
}
static stn_stage_status phase9_replay(void *user,const stn_record *record)
{
    (void)user;(void)record;return STN_STAGE_PASS;
}
static int phase9_setup(stn_mining_service *service)
{
    char event_name[128];DWORD n=GetEnvironmentVariableA("STN_PHASE9_STOP_EVENT",event_name,sizeof(event_name));
    if(n==0 || n>=sizeof(event_name)){return 0;}
    phase9_stop_event=OpenEventA(SYNCHRONIZE,FALSE,event_name);
    if(phase9_stop_event==NULL){return 0;}
    memcpy(phase9_validation.expected_network,service->chain->network_id,32);
    phase9_validation.time_configured=1;phase9_validation.validation_time=10;
    phase9_validation.verify_signature=phase9_signature;
    phase9_validation.lookup_authority=phase9_authority;
    phase9_validation.check_replay=phase9_replay;service->intelligence=&phase9_validation;
    fprintf(stderr,"TEST ONLY: Phase 9 scripted identity hooks; NOT production cryptography.\n");
    return 1;
}
