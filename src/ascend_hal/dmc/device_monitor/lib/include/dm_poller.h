/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef POLLER_H
#define POLLER_H

#include <poll.h>
#include <pthread.h>
#include "dm_list.h"
#include "mmpa_api.h"

/* conversion of units */
#define MS_PER_SECOND 1000L
#define US_PER_SECOND 1000000L
#define NS_PER_SECOND 1000000000L
#define US_PER_MSECOND 1000L
#define NS_PER_MSECOND 1000000L

/* default poll loop interval */
#define DEFAULT_POLL_TIMEOUT 50

/* poller notice */
#define NOTICE_POLL_EXIT 1U
#define NOTICE_POLL_DEAD 2U
#define NOTICE_POLL_WAKE 3U

/* poller state */
#define POLLER_STATE_READY 0
#define POLLER_STATE_LOOP 1
#define POLLER_STATE_EXIT 2
#define POLLER_STATE_DEAD 3

#define POLLER_T_FD_LEN 2
typedef struct poller_t {
    int state;                            /* poller module stats: all 4 status */
    int self_notify_fds[POLLER_T_FD_LEN]; /* poller internal communication fds
                                  use pipe channel: fds[0]-read fds[1]-write */
    pthread_t owner;                      /* thread id which call poller */
    pthread_t looper;                     /* poler id itself ID ( __do_poll ) */

    LIST_T *fds_list;   /* poller fd_list which it detect */
    LIST_T *timer_list; /* poller timer_list which it detect */
} POLLER_T;

struct POLLER_TIMER_ID_S {
    unsigned long id;
};
typedef struct POLLER_TIMER_ID_S POLLER_TIMER_ID_T;

/* poller mechanism handles message processing for file descriptors when events occur */
typedef void (*POLLER_FDS_HNDL_T)(int fd, short revents, void *user_data, int data_len);
/* poller mechanism handles message processing when there are events to be processed by the timer */
typedef void (*POLLER_TIMEDOUT_T)(int interval, int elapsed, void *user_data, int data_len);

/* fds struct definition in Poller mechanism */
typedef struct POLLER_FDS_S {
    struct pollfd user_fd;  /* fd of fds_list */
    struct pollfd *real_fd; /* fd which poll will handle (__poller_load_fds, copy from user_fd) */
    POLLER_FDS_HNDL_T hndl; /* FDs call back function */
} POLLER_FDS_T;

/* Timer struct definition */
typedef struct POLLER_TIMER_S {
    mmTimespec expired; /* system relative time */
    int interval;       /* timeout which user set */
    POLLER_TIMEDOUT_T hndl;
} POLLER_TIMER_T;

/*
 * poller fds & timer list(entry)
 */
typedef struct POLLER_ENTRY_S {
    union {
        POLLER_FDS_T fds;
        POLLER_TIMER_T timer;
    } cb_data;
    void *user_data;
    int data_len;
    int removed; /* label indicate entry has been deleted */
} POLLER_ENTRY_T;

struct POLLER_S {
    int state;                            /* status of poller, all 4 status */
    int self_notify_fds[POLLER_T_FD_LEN]; /* fds which poller communicate with itself
                               use pipe channel: fds[0]-read fds[1]-write */
    pthread_t owner;                      /* thread id of owner of poller(create it) */
    pthread_t looper;                     /* thread id of poller itself */

    LIST_T *fds_list;   /* fd list which poller detect */
    LIST_T *timer_list; /* timer list which poller detect */
};

extern int poller_create(POLLER_T **poller);
extern int poller_destory(POLLER_T *poller);
extern int poller_fd_add(POLLER_T *poller, int fd, short events, POLLER_FDS_HNDL_T handler, void *user_data,
                         int data_len);
extern int poller_fd_remove(POLLER_T *poller, int fd, short events);
extern int poller_timer_add(POLLER_T *poller, int timeout, POLLER_TIMEDOUT_T handler, void *user_data, int data_len,
                            POLLER_TIMER_ID_T *timer_id);
extern int poller_timer_remove(POLLER_T *poller, POLLER_TIMER_ID_T *time_id);
extern int poller_run(POLLER_T *poller);
extern int poller_exit(POLLER_T *poller);

#ifdef STATIC_SKIP
int __timespec_cmp(const mmTimespec *ts1, const mmTimespec *ts2);
#endif

#ifdef STATIC_SKIP
int __timespec_diff(mmTimespec *diff, const mmTimespec *left, const mmTimespec *right);
#endif

#ifdef STATIC_SKIP
void __timespec_add(mmTimespec *dest, const mmTimespec *base, int ms);
#endif

#ifdef STATIC_SKIP
void __timespec_sub(mmTimespec *dest, const mmTimespec *base, int ms);
#endif

#ifdef STATIC_SKIP
void __timespec_now_add(mmTimespec *ts, int ms);
#endif

#ifdef STATIC_SKIP
int __timespec_from_now(const mmTimespec *ts);
#endif

#ifdef STATIC_SKIP
int __poller_fd_cmp(const void *data1, const void *data2);
#endif

#ifdef STATIC_SKIP
int __poller_timespec_cmp(const void *data1, const void *data2);
#endif

#ifdef STATIC_SKIP
int __poller_timer_cmp(const void *data1, const void *data2);
#endif

#ifdef STATIC_SKIP
void __poller_entry_free(void *item);
#endif

#ifdef STATIC_SKIP
void __poller_send_notice(int fd, unsigned char notice);
#endif

#ifdef STATIC_SKIP
void __poller_recv_notice(int fd, unsigned char *notice);
#endif

#ifdef STATIC_SKIP
void __poller_notice_handle(int fd, short revents, void *user_data, int data_len);
#endif

#ifdef STATIC_SKIP
int __poller_self_notifier_init(POLLER_T *poller);
#endif

#ifdef STATIC_SKIP
void __poller_free(POLLER_T *poller);
#endif

#ifdef STATIC_SKIP
void __poller_deal_fds(LIST_T *fds_list);
#endif

#ifdef STATIC_SKIP
void __poller_deal_timers(LIST_T *timer_list);
#endif

#ifdef STATIC_SKIP
void __poller_deal_removed_entries(LIST_T *list);
#endif

#ifdef STATIC_SKIP
void *__do_poll(void *arg);
#endif

#endif
