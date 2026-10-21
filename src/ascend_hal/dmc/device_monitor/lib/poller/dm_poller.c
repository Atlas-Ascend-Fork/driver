/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "securec.h"

#include "dm_list.h"
#include "dm_common.h"
#include "dev_mon_log.h"
#include "mmpa_api.h"
#include "dm_poller.h"

#define SIZE_PTR_VALUE_CHECK() \
    if (*size <= 0) {          \
        return;                \
    }

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC int __timespec_cmp(const mmTimespec *ts1, const mmTimespec *ts2)
{
    if (ts1->tv_sec > ts2->tv_sec) {
        return 1;
    }

    if (ts1->tv_sec < ts2->tv_sec) {
        return -1;
    }

    /* if sec equal, compare nsec */
    if (ts1->tv_nsec > ts2->tv_nsec) {
        return 1;
    }

    if (ts1->tv_nsec < ts2->tv_nsec) {
        return -1;
    }

    return 0;
}

STATIC int __timespec_diff(mmTimespec *diff, const mmTimespec *left, const mmTimespec *right)

{
    mmTimespec tmp = {0};
    int ms;
    int ret;

    if ((left == NULL) || (right == NULL)) {
        return -1;
    }

    ret = __timespec_cmp(left, right);
    /* left less than right */
    if (ret <= 0) {
        tmp.tv_sec = 0;
        tmp.tv_nsec = 0;
        return -1;
    }

    /* left greater than right */
    tmp.tv_sec = left->tv_sec - right->tv_sec;
    tmp.tv_nsec = left->tv_nsec - right->tv_nsec;

    /* if nsec less than zero, borrow from sec */
    while (tmp.tv_nsec < 0) {
        tmp.tv_nsec += NS_PER_SECOND;
        tmp.tv_sec--;
    }

    if (diff != NULL) {
        *diff = tmp;
    }

    /* convert time diff to msec */
    ms = (int)((tmp.tv_sec * MS_PER_SECOND) + (tmp.tv_nsec / NS_PER_MSECOND));
    return ms;
}

STATIC void __timespec_add(mmTimespec *dest, const mmTimespec *base, int ms)
{
    int ms_temp;
    /* convert ms to sec and nsec and save it in timespec */
    dest->tv_sec = base->tv_sec + (ms / MS_PER_SECOND);
    ms_temp = ms % MS_PER_SECOND;
    dest->tv_nsec = base->tv_nsec + (ms_temp * NS_PER_MSECOND);

    /* if nsec exceeds sec, transport to sec */
    while (dest->tv_nsec >= NS_PER_SECOND) {
        dest->tv_nsec -= NS_PER_SECOND;
        dest->tv_sec++;
    }

    return;
}

STATIC void __timespec_sub(mmTimespec *dest, const mmTimespec *base, int ms)
{
    int ms_temp;
    dest->tv_sec = base->tv_sec - (ms / MS_PER_SECOND);
    ms_temp = ms % MS_PER_SECOND;
    dest->tv_nsec = base->tv_nsec - (ms_temp * NS_PER_MSECOND);

    while (dest->tv_nsec < 0) {
        dest->tv_nsec += NS_PER_SECOND;
        dest->tv_sec--;
    }

    return;
}

STATIC void __timespec_now_add(mmTimespec *ts, int ms)
{
    mmTimespec now = {0};
    now = mmGetTickCount();
    __timespec_add(ts, &now, ms);
    return;
}
/*
 * equivalent to the current time, the time that the timer has already passed
 */
STATIC int __timespec_from_now(const mmTimespec *ts)
{
    mmTimespec now = {0};
    now = mmGetTickCount();
    return __timespec_diff(NULL, ts, &now);
}

STATIC int __poller_fd_cmp(const void *data1, const void *data2)
{
    const POLLER_ENTRY_T *entry1 = (const POLLER_ENTRY_T *)data1;
    const POLLER_ENTRY_T *entry2 = (const POLLER_ENTRY_T *)data2;
    const struct pollfd *poll_fd1 = &(entry1->cb_data.fds.user_fd);
    const struct pollfd *poll_fd2 = &(entry2->cb_data.fds.user_fd);

    /* compare two fd */
    if ((poll_fd1->fd == poll_fd2->fd) &&
        ((((unsigned short)(poll_fd1->events)) & ((unsigned short)(poll_fd2->events))) != 0)) {
        return 0;
    } else {
        return 1;
    }
}

STATIC int __poller_timespec_cmp(const void *data1, const void *data2)
{
    const POLLER_ENTRY_T *entry1 = (const POLLER_ENTRY_T *)data1;
    const POLLER_ENTRY_T *entry2 = (const POLLER_ENTRY_T *)data2;
    const mmTimespec *ts1 = &(entry1->cb_data.timer.expired);
    const mmTimespec *ts2 = &(entry2->cb_data.timer.expired);
    return __timespec_cmp(ts1, ts2);
}

STATIC int __poller_timer_cmp(const void *data1, const void *data2)
{
    const POLLER_ENTRY_T *entry1 = (const POLLER_ENTRY_T *)data1;
    const POLLER_ENTRY_T *entry2 = (const POLLER_ENTRY_T *)data2;
    mmTimespec ts1;
    mmTimespec ts2;
    int ret;

    if (data1 != data2) {
        return -1;
    }

    /* if both of timers are of equal id and equal adding time,
       then they are same */
    /* get timer's adding time */
    __timespec_sub(&ts1, &(entry1->cb_data.timer.expired), entry1->cb_data.timer.interval);
    __timespec_sub(&ts2, &(entry2->cb_data.timer.expired), entry2->cb_data.timer.interval);
    ret = __timespec_cmp(&ts1, &ts2);

    if (ret != 0) {
        return -1;
    }

    return 0;
}

/*
 * poller node item releases the handle (used by list_create())
 */
STATIC void __poller_entry_free(void *item)
{
    POLLER_ENTRY_T *entry = (POLLER_ENTRY_T *)item;

    if (entry == NULL) {
        return;
    }

    if (entry->user_data != NULL) {
        free(entry->user_data);
        entry->user_data = NULL;
    }

    free(item);
    item = NULL;
    return;
}

/*
 * poller pipe write
 */
STATIC void __poller_send_notice(int fd, unsigned char notice)
{
    (void)mm_write_file(fd, &notice, 1);
    return;
}

/*
 * poller pipe read
 */
STATIC void __poller_recv_notice(int fd, unsigned char *notice)
{
    int ret;
    ret = (int)mm_read_file(fd, notice, 1);
    if (ret < 0) {
        DEV_MON_ERR("read %u error:%d\n", *notice, errno);
        return;
    }

    return;
}

STATIC void __poller_notice_handle(int fd, short revents, void *user_data, int data_len)
{
    unsigned char notice = 0;
    unsigned short tmprevents = (unsigned short)revents;
    POLLER_T *poller = *((POLLER_T **)user_data);
    (void)data_len;

    if ((tmprevents & POLLIN) != 0) { /* check if msg comes */
        /* read pipe message */
        __poller_recv_notice(fd, &notice);
    }

    switch (notice) {
        case NOTICE_POLL_EXIT:
            poller->state = POLLER_STATE_EXIT;
            break;

        case NOTICE_POLL_DEAD:
            poller->state = POLLER_STATE_DEAD;
            break;

        default:
            /* do nothing */
            break;
    }

    return;
}

STATIC int __poller_self_notifier_init(POLLER_T *poller)
{
    int ret;
    int ret_t;
    poller->self_notify_fds[0] = -1;
    poller->self_notify_fds[1] = -1;

    /* create pipe to control do_poll thread */
    if ((pipe(poller->self_notify_fds) < 0) || (fcntl(poller->self_notify_fds[0], F_SETFL, O_NONBLOCK) < 0) ||
        (fcntl(poller->self_notify_fds[1], F_SETFL, O_NONBLOCK) < 0) ||
        (fcntl(poller->self_notify_fds[0], F_SETFD, FD_CLOEXEC) < 0) ||
        (fcntl(poller->self_notify_fds[1], F_SETFD, FD_CLOEXEC) < 0)) {
        ret = errno;
        goto out;
    }

    /* add fd to fds_list of poller */
    ret = poller_fd_add(poller, poller->self_notify_fds[0], POLLIN, __poller_notice_handle, &poller,
                        sizeof(POLLER_T *));
out:

    if ((ret != 0) && (poller->self_notify_fds[0] >= 0)) {
        ret_t = mm_close_file(poller->self_notify_fds[0]);
        DRV_CHECK_CHK(ret_t == 0);
        ret_t = mm_close_file(poller->self_notify_fds[1]);
        DRV_CHECK_CHK(ret_t == 0);
        poller->self_notify_fds[0] = -1;
        poller->self_notify_fds[1] = -1;
    }

    return ret;
}

int poller_create(POLLER_T **poller)
{
    POLLER_T *pl = NULL;
    int ret;

    if (poller == NULL) {
        return EINVAL;
    }

    pl = malloc(sizeof(POLLER_T));
    if (pl == NULL) {
        *poller = NULL;
        return ENOMEM;
    }

    ret = memset_s((void *)pl, sizeof(POLLER_T), 0, sizeof(POLLER_T));
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail!\n");
        free(pl);
        pl = NULL;
        *poller = NULL;
        return ret;
    }

    *poller = pl;
    /* create fds_list to save channel which will be detected */
    ret = list_create(&pl->fds_list, __poller_fd_cmp, __poller_entry_free);
    if (ret != 0) {
        free(pl);
        pl = NULL;
        *poller = NULL;
        return ret;
    }

    /* create timer_list to handle timeout */
    ret = list_create(&pl->timer_list, __poller_timer_cmp, __poller_entry_free);
    if (ret != 0) {
        list_destroy(pl->fds_list);
        pl->fds_list = NULL;
        free(pl);
        pl = NULL;
        *poller = NULL;
        return ret;
    }

    pl->owner = pthread_self();
    pl->state = POLLER_STATE_READY;
    /* create pipe and add its fd into poller->fds_list */
    ret = __poller_self_notifier_init(pl);
    if (ret != 0) {
        list_destroy(pl->timer_list);
        pl->timer_list = NULL;
        list_destroy(pl->fds_list);
        pl->fds_list = NULL;
        free(pl);
        pl = NULL;
        *poller = NULL;
        return ret;
    }

    return 0;
}

STATIC void __poller_free(POLLER_T *poller)
{
    int ret;
    /* destroy two list first, and then poller itself */
    list_destroy(poller->fds_list);
    poller->fds_list = NULL;
    list_destroy(poller->timer_list);
    poller->timer_list = NULL;
    /* close pipe of read and write */
    ret = mm_close_file(poller->self_notify_fds[0]);
    DRV_CHECK_CHK(ret == 0);
    ret = mm_close_file(poller->self_notify_fds[1]);
    DRV_CHECK_CHK(ret == 0);
    /* free poller */
    free(poller);
    poller = NULL;
    return;
}

STATIC int __try_poller_free(POLLER_T *poller)
{
    int retry_count = 30; /* 30: max retry time 30*100ms = 3s */

    while (retry_count != 0 && poller->state != POLLER_STATE_READY) {
        retry_count--;
        (void)mmSleep(100); /* 100 ms one period */
    }

    if (retry_count != 0) {
        __poller_free(poller);
        return 0;
    }

    return EPERM;
}

int poller_destory(POLLER_T *poller)
{
    int ret;
    if (poller == NULL) {
        return EINVAL;
    }
#ifndef DEV_MON_UT
    poller->state = POLLER_STATE_EXIT;
#else
    __poller_send_notice(poller->self_notify_fds[1], NOTICE_POLL_EXIT);
#endif
    ret = __try_poller_free(poller);
    return ret;
}

STATIC int poller_init_entry(POLLER_FDS_T *fds, short events, POLLER_ENTRY_T **entry, int fd, POLLER_FDS_HNDL_T handler)
{
    int ret;
    *entry = (POLLER_ENTRY_T *)malloc(sizeof(POLLER_ENTRY_T));
    if ((*entry) == NULL) {
        DEV_MON_ERR("malloc fail\n");
        return ENOMEM;
    }

    ret = memset_s((void *)(*entry), sizeof(POLLER_ENTRY_T), 0, sizeof(POLLER_ENTRY_T));
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail!\n");
        free(*entry);
        (*entry) = NULL;
        return ret;
    }

    fds = &((*entry)->cb_data.fds);
    fds->user_fd.fd = fd;
    fds->user_fd.events = events;
    fds->user_fd.revents = 0;
    fds->real_fd = NULL;
    fds->hndl = handler;
    return 0;
}

static inline void poller_uninit_entry(POLLER_ENTRY_T *entry)
{
    if (entry->user_data != NULL) {
        free(entry->user_data);
        entry->user_data = NULL;
    }

    free(entry);
    entry = NULL;
}

int poller_fd_add(POLLER_T *poller, int fd, short events, POLLER_FDS_HNDL_T handler, void *user_data, int data_len)
{
    POLLER_ENTRY_T *entry = NULL;
    POLLER_FDS_T *fds = NULL;
    int ret;

    if ((poller == NULL) || (data_len == 0)) {
        return EINVAL;
    }

    ret = poller_init_entry(fds, events, &entry, fd, handler);
    if (ret != 0) {
        DEV_MON_ERR("init entry fail ret = %d\n", ret);
        return ret;
    }

    /* save user's data */
    if ((user_data != NULL) && (data_len > 0)) {
        entry->user_data = malloc((size_t)data_len);
        if (entry->user_data == NULL) {
            DEV_MON_ERR("malloc fail, errno=%d.\n", errno);
            free(entry);
            entry = NULL;
            return ENOMEM;
        }

        ret = memset_s((void *)entry->user_data, (size_t)data_len, 0, (size_t)data_len);
        if (ret != 0) {
            DEV_MON_ERR("memset_s fail!\n");
            poller_uninit_entry(entry);
            return ret;
        }

        ret = memmove_s(entry->user_data, (size_t)data_len, user_data, (size_t)data_len);
        DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(entry->user_data); entry->user_data = NULL; free(entry);
                                    entry = NULL; DEV_MON_ERR("memmove_s error\n"));
        entry->data_len = data_len;
    }

    entry->removed = 0;
    /* add entry node the tail of fds_list */
    ret = list_append(poller->fds_list, entry);
    if (ret != 0) {
        poller_uninit_entry(entry);
        return ret;
    }
    __poller_send_notice(poller->self_notify_fds[1], NOTICE_POLL_WAKE);

    return 0; // lint !e429
}

int poller_fd_remove(POLLER_T *poller, int fd, short events)
{
    int ret;
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    POLLER_ENTRY_T *entry = NULL;
    POLLER_ENTRY_T ientry;
    POLLER_FDS_T *fds = NULL;

    if (poller == NULL) {
        return EINVAL;
    }

    ret = memset_s(&ientry, sizeof(POLLER_ENTRY_T), 0, sizeof(POLLER_ENTRY_T));
    if (ret != EOK) {
        return EINVAL;
    }

    fds = &(ientry.cb_data.fds);
    fds->user_fd.fd = fd;
    fds->user_fd.events = events;
    list_iter_init(poller->fds_list, &iter);

    while ((node = list_iter_next(&iter)) != NULL) {
        entry = (POLLER_ENTRY_T *)list_to_item(node);
        if (__poller_fd_cmp(entry, &ientry) == 0) {
            if (poller->state == POLLER_STATE_LOOP) {
                /* mark it, then poller looping will remove it */
                entry->removed = 1;
            } else {
                /* if poller not running, delete it here */
                (void)list_remove(poller->fds_list, node);
            }
        }
    }

    list_iter_destroy(&iter);
    return 0;
}
STATIC int poller_timer_init_entry(POLLER_ENTRY_T **entry, int timeout, POLLER_TIMEDOUT_T handler)
{
    int ret;
    POLLER_TIMER_T *timer = NULL;

    *entry = (POLLER_ENTRY_T *)malloc(sizeof(POLLER_ENTRY_T));
    if (*entry == NULL) {
        return ENOMEM;
    }

    ret = memset_s((void *)(*entry), sizeof(POLLER_ENTRY_T), 0, sizeof(POLLER_ENTRY_T));
    if (ret != 0) {
        DEV_MON_ERR("entry memset fail!\n");
        free(*entry);
        *entry = NULL;
        return ret;
    }

    timer = &((*entry)->cb_data.timer);
    __timespec_now_add(&timer->expired, timeout);
    timer->interval = timeout;
    timer->hndl = handler;
    return 0;
}

int poller_timer_add(POLLER_T *poller, int timeout, POLLER_TIMEDOUT_T handler, void *user_data, int data_len,
                     POLLER_TIMER_ID_T *timer_id)
{
    POLLER_ENTRY_T *entry = NULL;
    int ret;
    int errno_tmp;
    uintptr_t tmpptr = 0;

    if ((poller == NULL) || (data_len <= 0)) {
        return EINVAL;
    }

    ret = poller_timer_init_entry(&entry, timeout, handler);
    if (ret != 0) {
        DEV_MON_ERR("init poller timer entry failed. (ret=%d)\n", ret);
        return ret;
    }
    /* save user's data */
    if ((user_data != NULL) && data_len) {
        entry->user_data = malloc((size_t)data_len);
        errno_tmp = errno;
        DRV_CHECK_RETV_DO_SOMETHING((entry->user_data != NULL), ENOMEM, free(entry); entry = NULL;
                                    DEV_MON_ERR("malloc fail, errno=%d.\n", errno_tmp));

        ret = memset_s((void *)entry->user_data, (size_t)data_len, 0, (size_t)data_len);
        if (ret != 0) {
            DEV_MON_ERR("entry->user_data fail!\n");
            poller_uninit_entry(entry);
            return ret;
        }

        ret = memmove_s(entry->user_data, (size_t)data_len, user_data, (size_t)data_len);
        DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(entry->user_data); entry->user_data = NULL; free(entry);
                                    entry = NULL; DEV_MON_ERR("memmove_s error\n"));
        entry->data_len = data_len;
    }

    entry->removed = 0;
    /* ascend the entry based func __poller_timespec_cmp */
    ret = list_insert_ascend(poller->timer_list, entry, __poller_timespec_cmp);
    if (ret != 0) {
        DEV_MON_ERR("failed to call list_insert_ascend\n");
        poller_uninit_entry(entry);
        return ret;
    }

    if (timer_id != NULL) {
        tmpptr = (uintptr_t)entry;
        timer_id->id = (unsigned long)tmpptr;
    }

    return 0; // lint !e593
}

int poller_timer_remove(POLLER_T *poller, POLLER_TIMER_ID_T *time_id)
{
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    POLLER_ENTRY_T *entry = NULL;
    uintptr_t tmpid = 0;

    if ((poller == NULL) || (time_id == NULL)) {
        return EINVAL;
    }

    list_iter_init(poller->timer_list, &iter);

    while ((node = list_iter_next(&iter)) != NULL) {
        entry = (POLLER_ENTRY_T *)list_to_item(node);
        tmpid = (uintptr_t)time_id->id;

        if (__poller_timer_cmp((void *)entry, (void *)tmpid) == 0) {
            if (poller->state == POLLER_STATE_LOOP) {
                /* mark it, then poller looping will remove it */
                entry->removed = 1;
            } else {
                /* poller not running, delete timer node */
                (void)list_remove(poller->timer_list, node);
            }
        }
    }

    list_iter_destroy(&iter);
    return 0;
}

STATIC void __poller_load_fds(LIST_T *fds_list, struct pollfd **fds, int *nfds, int *size)
{
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    POLLER_ENTRY_T *entry = NULL;
    char *new_space = NULL;
    int ret = 0;
    struct pollfd *tmpfds = NULL;
    int quiry_node_num = 0;

    DRV_CHECK_RET((fds != NULL));
    DRV_CHECK_RET((nfds != NULL));
    DRV_CHECK_RET((size != NULL));
    *nfds = list_size(fds_list);

    if ((*size < *nfds) || (*size == 0)) {
        *size = (int)(((unsigned int)(*nfds + FDS_INTEGER_NUMBAR)) & 0xfffffff8U);
        SIZE_PTR_VALUE_CHECK();
        new_space = malloc((unsigned long)((unsigned int)(*size) * sizeof(struct pollfd)));
        if (new_space != NULL) {
            if ((fds != NULL) && (*fds != NULL)) {
                free(*fds);
            }

            *fds = (struct pollfd *)new_space;
        } else {
            DEV_MON_ERR("malloc fail. errno=%d.", errno);
            *nfds = 0;
            *size = 0;
            return;
        }

        ret = memset_s((void *)new_space, (unsigned long)(*size) * sizeof(struct pollfd), 0,
                       (unsigned long)(*size) * sizeof(struct pollfd));
        if (ret != 0) {
            DEV_MON_ERR("memset_s fail. errno=%d.", errno);
            free(new_space);
            new_space = NULL;
            *fds = NULL;
            *nfds = 0;
            *size = 0;
            return;
        }
    }

    tmpfds = *fds;
    list_iter_init(fds_list, &iter);

    while ((quiry_node_num < *nfds) && ((node = list_iter_next(&iter)) != NULL)) {
        entry = (POLLER_ENTRY_T *)list_to_item(node);
        if (!entry->removed) {
            *tmpfds = entry->cb_data.fds.user_fd;
            entry->cb_data.fds.real_fd = tmpfds;
            tmpfds++;
            quiry_node_num++;
        }
    }

    list_iter_destroy(&iter);
    return;
}

STATIC void __poller_deal_fds(LIST_T *fds_list)
{
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    POLLER_ENTRY_T *entry = NULL;
    POLLER_FDS_T *fds = NULL;
    list_iter_init(fds_list, &iter);

    while ((node = list_iter_next(&iter)) != NULL) {
        entry = (POLLER_ENTRY_T *)list_to_item(node);
        if (!entry->removed) {
            /* get fds */
            fds = &(entry->cb_data.fds);

            /* find fds which has revents */
            if (fds->real_fd && fds->real_fd->revents && fds->hndl) {
                /* call deal fd when msg comes */
                fds->hndl(fds->real_fd->fd, fds->real_fd->revents, entry->user_data, entry->data_len);
            }
        }
    }

    list_iter_destroy(&iter);
    return;
}

STATIC void __poller_deal_timers(LIST_T *timer_list)
{
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    POLLER_ENTRY_T *entry = NULL;
    POLLER_TIMER_T *timer = NULL;
    int diff = 0;
    int elapsed = 0;
    list_iter_init(timer_list, &iter);

    while ((node = list_iter_next(&iter)) != NULL) {
        entry = (POLLER_ENTRY_T *)list_to_item(node);
        /* check if entry has been removed */
        if (!entry->removed) {
            timer = &(entry->cb_data.timer);
            /* calculate the time diff of timeout and current */
            diff = __timespec_from_now(&timer->expired);
            if (diff > 0) {
                break;
            }

            if (timer->hndl != NULL) {
                elapsed = timer->interval - diff;
                /* call handle func when timeout occurs */
                timer->hndl(timer->interval, elapsed, entry->user_data, entry->data_len);
            }
        }

        /* delete the node which has been timeout */
        (void)list_remove(timer_list, node);
    }

    list_iter_destroy(&iter);
    return;
}

STATIC void __poller_deal_removed_entries(LIST_T *list)
{
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    POLLER_ENTRY_T *entry = NULL;
    list_iter_init(list, &iter);

    while ((node = list_iter_next(&iter)) != NULL) {
        entry = (POLLER_ENTRY_T *)list_to_item(node);
        if (entry->removed != 0) {
            (void)list_remove(list, node);
        }
    }

    list_iter_destroy(&iter);

    return;
}

STATIC void *__do_poll(void *arg)
{
    POLLER_T *poller = (POLLER_T *)arg;
    struct pollfd *fds = NULL;
    struct pollfd *poll_fds = NULL;
    int nfds = 0;
    int size = 0;
    int ret, poll_nfds;
    /* change status to unjoinable, make sure resource release when thread quit. */
    (void)pthread_detach(pthread_self());
    (void)mmSetCurrentThreadName("do_poll");

    while ((poller != NULL) && (poller->state == POLLER_STATE_LOOP)) {
        if (list_is_empty(poller->fds_list)) {
            poll_fds = NULL;
            poll_nfds = 0;
        } else { /* get all polled fds of poller->fds_list */
            __poller_load_fds(poller->fds_list, &fds, &nfds, &size);
            poll_fds = fds;
            poll_nfds = nfds;
        }

        ret = poll(poll_fds, (unsigned long)poll_nfds, DEFAULT_POLL_TIMEOUT);
        if (ret < 0) {
            if (errno != EINTR) {
                DEV_MON_ERR("Do poll fail. (ret=%d; errno=%d)\n", ret, errno);
#ifndef DEV_MON_UT
                continue;
#else
                break;
#endif
            }
        } else if (ret > 0) { /* if any fds has data to receive */
            /* deal with fds, call its message handling func */
            __poller_deal_fds(poller->fds_list);
        }

        /* regardless timeout, deal timer. */
        __poller_deal_timers(poller->timer_list);
        /* delete marked fds, should improve efficiency */
        __poller_deal_removed_entries(poller->fds_list);
        __poller_deal_removed_entries(poller->timer_list);
    }
    if (fds != NULL) {
        free(fds);
        fds = NULL;
    }

    if (poller != NULL) {
        poller->state = POLLER_STATE_READY;
    }

    return NULL;
}

int poller_run(POLLER_T *poller)
{
    int ret;

    mmUserBlock_t func_block = {0};
    mmThread *thread_handle = NULL;
    mmThreadAttr thread_attr = {0};

    if (poller == NULL) {
        return EINVAL;
    }
    func_block.procFunc = __do_poll;
    func_block.pulArg = (void *)poller;
    thread_handle = &poller->looper;
    thread_attr.priorityFlag = TRUE;
    thread_attr.policyFlag = TRUE;
#ifdef CFG_FEATURE_SCHED_OTHER
    thread_attr.policy = SCHED_OTHER;
    thread_attr.priority = 0;
#else
    thread_attr.policy = SCHED_RR;
    thread_attr.priority = 60; // 60 thread priority
#endif
    thread_attr.stackFlag = TRUE;
    thread_attr.stackSize = DEV_MON_ROOT_STACK_SIZE;
    thread_attr.detachFlag = TRUE;

    /* for safety, only owner can run the poller */
    if (pthread_self() != poller->owner) {
        return EPERM;
    }

    /* do not allow the same poller runs more than one time */
    if (poller->state != POLLER_STATE_READY) {
        return EPERM;
    }

    /* Prevent thread scheduling from accessing null pointers after resources are released */
    poller->state = POLLER_STATE_LOOP;
    /* set inherit policy, must set inherit as PTHREAD_EXPLICIT_SCHED, or thread sched policy set willbe failed */
    ret = mmCreateTaskWithThreadAttr(thread_handle, &func_block, &thread_attr);
    if (ret != 0) {
        DEV_MON_WARNING("The system does not support adjusting sched priority, try using default thread priority\n");
        ret = mmCreateTask(thread_handle, &func_block);
    }

    if (ret != 0) {
        DEV_MON_WARNING("Create task warn. (ret=%d)\n", ret);
        poller->state = POLLER_STATE_READY;
    }

    return ret;
}

int poller_exit(POLLER_T *poller)
{
    /* for safety, only owner can exit the poll loop */
    if (pthread_self() != poller->owner) {
        return EPERM;
    }

    __poller_send_notice(poller->self_notify_fds[1], NOTICE_POLL_EXIT);

    return 0;
}
