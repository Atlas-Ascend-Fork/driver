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
#include <stdlib.h>
#include <pthread.h>
#include <malloc.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>

#include "securec.h"
#include "mmpa_api.h"
#include "dev_mon_log.h"
#include "dm_queue.h"
#include "dm_poller.h"
#include "device_monitor_type.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DM_MSG_QUEUE_TIMEOUT_CNT 5
#define DM_MSG_QUEUE_TIMEOUT_MAX 8
#define DM_MSG_QUEUE_TIMEOUT 45000 /* 45s */

static int dm_msg_queue_init(int *ul_queue_id, mmKey_t msgkey, struct msqid_ds *data)
{
    int ret;
    *ul_queue_id = msgget(msgkey, 0); // if the queue corresponding to the key already exists, delete it
    if (*ul_queue_id != -1) {
        ret = mmMsgCtl(*ul_queue_id, IPC_RMID, data);
        if (ret != 0) {
            DEV_MON_ERR("mmMsgCtl IPC_RMID failed ret = %d.\r\n", ret);
            return ret;
        }
    }

    *ul_queue_id = mmMsgCreate(msgkey, IPC_CREAT | DM_MSG_MODE_PRIVATE);
    if (*ul_queue_id == -1) {
        DEV_MON_ERR("mmMsgCreate failed\r\n");
        return DM_ERR;
    }

    ret = mmMsgCtl(*ul_queue_id, IPC_STAT, data);
    if (ret != 0) {
        DEV_MON_ERR("mmMsgCtl failed IPC_STAT, ret = %d\r\n", ret);
        return ret;
    }

    return 0;
}

/*****************************************************************************
 function   : dm_private_queue_create
 description: create queue
 input      : pch_name       : name(4 chars) of the queue
              ul_length      : the length of the queue
              pul_queue_id    : the address of the queue ID
              ul_max_msg_size  : the max message size that the queue can contain
 output     : pul_queue_id    : the address of the queue ID
 return     : 0 on success or errno on failure
 *****************************************************************************/
unsigned long dm_private_queue_create(const char *pch_name, mmKey_t msgkey, unsigned long ul_length,
                                      unsigned long *pul_queue_id, unsigned long ul_max_msg_size)
{
    int ul_queue_id;
    signed char q_name[DM_QUEUE_NAME_LEN_MAX] = {0};
    signed long ret;
    struct msqid_ds data = {0};
    unsigned int i = 0;
    unsigned int pch_name_len;

    if ((ul_length == 0) || (ul_max_msg_size == 0) || (pch_name == NULL) || (pul_queue_id == NULL)) {
        return DM_ERR;
    }

    ret = dm_msg_queue_init(&ul_queue_id, msgkey, &data);
    if (ret != 0) {
        DEV_MON_ERR("dm_msg_queue_init failed.\r\n");
        return DM_ERR;
    }

    data.msg_qbytes = ul_length * sizeof(void *);
    ret = memset_s(q_name, sizeof(q_name), 0, sizeof(q_name));
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), DM_ERR, DEV_MON_ERR("memset_s error\n"));

    pch_name_len = (unsigned int)strlen(pch_name);
    for (i = 0; i < DM_QUEUE_NAME_LEN_MAX - 1 && i < pch_name_len; i++) {
        q_name[i] = (signed char)pch_name[i];
    }

    ret = memcpy_s(&data.msg_perm.uid, sizeof(data.msg_perm.uid), q_name, 4); /* 4 bytes for uid */
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), DM_ERR, DEV_MON_ERR("memcpy_s error\n"));
    ret = memcpy_s(&data.msg_perm.gid, sizeof(data.msg_perm.gid), q_name + 4, 4); /* 4 bytes for gid */
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), DM_ERR, DEV_MON_ERR("memcpy_s error\n"));
    ret = mmMsgCtl(ul_queue_id, IPC_SET, &data);
    if (ret != 0) {
        DEV_MON_ERR("set queue stat error!! errno is 0x%x.\n", errno);
        return DM_ERR;
    }

    *pul_queue_id = (unsigned long)(unsigned int)ul_queue_id;
    return 0;
}

/*****************************************************************************
 function   : dm_queue_read
 description: reading message from a queue synchronously
 input      : ul_queue_id   : queue ID
              ul_time_out   : time-out interval,  in milliseconds. 0 means infinite
              p_buffer_addr : buffer to retrieve msg
              ul_buffer_size: size of buffer
 output     : p_buffer_addr : buffer to retrieve msg
 return     : the length of the message read from specific queue;
              DM_NULL_LONG indicates reading failure.
 *****************************************************************************/
unsigned long dm_queue_read(unsigned long ul_queue_id, unsigned long ul_time_out, void *p_buffer_addr,
                            unsigned long ul_buffer_size)
{
    int i;
    long ul_return_code;
    ST_DM_QUEUE_MSG_TYPE read_buf = {0};
    int ret;

    if ((p_buffer_addr == NULL) || (ul_buffer_size == 0)) {
        return DM_NULL_LONG;
    }

    if (ul_time_out == 0) {
        do {
            ul_return_code = mmMsgRcv((mmMsgid)ul_queue_id, &read_buf, sizeof(void *), 0);
        } while ((ul_return_code == -1) && errno == EINTR);

        if (ul_return_code == sizeof(void *)) {
            ret = memcpy_s(p_buffer_addr, ul_buffer_size, read_buf.mtext, ul_buffer_size);
            DRV_CHECK_RETV_DO_SOMETHING((ret == 0), DM_NULL_LONG, free(read_buf.mtext); read_buf.mtext = 0;
                                        DEV_MON_ERR("memcpy_s error\n"));
            free(read_buf.mtext);
            read_buf.mtext = 0;
            return (unsigned long)ul_buffer_size;
        }
    } else {
        for (i = 0; i < (int)ul_time_out / DM_TIMEOUT_EACH_TIME_MS; i++) {
            ul_return_code = mmMsgRcv((mmMsgid)ul_queue_id, &read_buf, sizeof(void *), IPC_NOWAIT);
            if (ul_return_code == sizeof(void *)) {
                ret = memcpy_s(p_buffer_addr, ul_buffer_size, read_buf.mtext, ul_buffer_size);
                DRV_CHECK_RETV_DO_SOMETHING((ret == 0), DM_NULL_LONG, free(read_buf.mtext); read_buf.mtext = 0;
                                            DEV_MON_ERR("memcpy_s error\n"));
                free(read_buf.mtext);
                read_buf.mtext = 0;
                return (unsigned long)ul_buffer_size;
            }

            (void)usleep(DM_TIMEOUT_EACH_TIME_MS * 1000); /* 1000 us means 1 ms */
        }

        if (i >= (int)ul_time_out / DM_TIMEOUT_EACH_TIME_MS) {
            return DM_ERROR_WAIT_TIMEOUT;
        }
    }

    return DM_NULL_LONG;
}

STATIC bool dm_queue_write_check(struct timespec start, unsigned int *wr_cnt, int *ret)
{
    struct timespec end;
    long recv_time;

    (*wr_cnt)++;
    if (((*wr_cnt) >= DM_MSG_QUEUE_TIMEOUT_CNT) && ((*wr_cnt) <= DM_MSG_QUEUE_TIMEOUT_MAX)) {
        DEV_MON_ERR("Unable to write to message queue. (wr_cnt=%u)\n", (*wr_cnt));
    } else if ((*wr_cnt) > DM_MSG_QUEUE_TIMEOUT_MAX) {
        (void)clock_gettime(CLOCK_MONOTONIC, &end);
        recv_time = (end.tv_sec * MS_PER_SECOND + end.tv_nsec / NS_PER_MSECOND) -
                    (start.tv_sec * MS_PER_SECOND + start.tv_nsec / NS_PER_MSECOND);

        if (recv_time > DM_MSG_QUEUE_TIMEOUT) {
            DEV_MON_ERR("Message queue abnormal. (wr_cnt=%u; recv_time=%llums)\n", (*wr_cnt), recv_time);
            *ret = DEV_MON_MSG_QUEUE_BREAK;
            return false;
        }
    }
    return true;
}

/*****************************************************************************
 function   : dm_queue_asy_write
 description: write a message to the specific queue asynchronously
 input      : ul_queue_id     : queue ID
              p_buffer_addr   : message buffer
              ul_buffer_size  : the size of message buffer
 return     : 0 on success or errno on failure
 *****************************************************************************/
int dm_queue_asy_write(unsigned long ul_queue_id, const void *p_buffer_addr, unsigned long ul_buffer_size)
{
    int ul_return_code;
    ST_DM_QUEUE_MSG_TYPE send_buf = {0};
    unsigned int wr_cnt = 0;
    struct timespec time_start = {0, 0};
    int ret;

    if ((ul_queue_id == DM_NULL_LONG) || (p_buffer_addr == NULL) || (ul_buffer_size == 0)) {
        return DM_ERR;
    }

    send_buf.mtext = malloc(ul_buffer_size);
    if (send_buf.mtext == NULL) {
        DEV_MON_ERR("can't alloc memory, errno=%d.\n", errno);
        return DM_ERR;
    }

    ret = memset_s((void *)send_buf.mtext, ul_buffer_size, 0, ul_buffer_size);
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail: %d\n", ret);
        free(send_buf.mtext);
        send_buf.mtext = NULL;
        return DM_ERR;
    }

    send_buf.mtype = MMPA_DEFAULT_MSG_TYPE;
    ret = memcpy_s(send_buf.mtext, ul_buffer_size, p_buffer_addr, ul_buffer_size);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), DM_ERR, free(send_buf.mtext); send_buf.mtext = NULL;
                                DEV_MON_ERR("memcpy_s error\n"));

    (void)clock_gettime(CLOCK_MONOTONIC, &time_start);
    do {
        ul_return_code = mmMsgSnd((mmMsgid)ul_queue_id, &send_buf, sizeof(void *), IPC_NOWAIT);
    } while ((ul_return_code != 0) && (errno == EINTR) && (dm_queue_write_check(time_start, &wr_cnt, &ul_return_code)));

    if (ul_return_code != 0) {
        if (errno == EAGAIN) {
            do {
                ul_return_code = mmMsgSnd((mmMsgid)ul_queue_id, &send_buf, sizeof(void *), IPC_NOWAIT);
            } while ((ul_return_code != 0) && (errno == EINTR) &&
                     (dm_queue_write_check(time_start, &wr_cnt, &ul_return_code)));
        }

        if (ul_return_code != 0) {
            DEV_MON_ERR("Failed to write to message queue. (ul_return_code=%lu, errno=%d)\n", ul_return_code, errno);
            free(send_buf.mtext);
            send_buf.mtext = NULL;
        }
    }

    return ul_return_code;
}

/*****************************************************************************
 function   : dm_queue_delete
 description: delete the specific queue
 input      : ul_queue_id: queue ID
 return     : 0 on success or errno on failure
 *****************************************************************************/
int dm_queue_delete(unsigned long ul_queue_id)
{
    struct msqid_ds data = {0};
    return mmMsgCtl((mmMsgid)(0xffffffffUL & ul_queue_id), IPC_RMID, &data);
}
