/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_REPORT_H
#define STN_REPORT_H

#include <stddef.h>

typedef enum stn_report_event {
    STN_REPORT_START = 0,
    STN_REPORT_PEER,
    STN_REPORT_SYNC,
    STN_REPORT_BLOCK,
    STN_REPORT_REORG,
    STN_REPORT_STOP,
    STN_REPORT_ERROR
} stn_report_event;

const char *stn_report_event_name(stn_report_event event);
int stn_report_format(
    stn_report_event event,
    const char *message,
    char *output,
    size_t capacity);

#endif
