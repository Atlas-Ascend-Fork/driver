/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <unistd.h>
#include <stdint.h>

#include "securec.h"

#include "ascend_hal.h"
#include "esched_user_interface.h"

#include "trs_user_pub_def.h"
#include "trs_cb_event.h"

#define CALLBACK_EVENT_THREAD_MAX 1024 /* halTsdrvCtl() determine max thread num */

static void trs_cb_event_wait_init(uint32_t dev_id, uint32_t tid)
{
    struct event_info event;

    /*
     * Tell the current grp that some tid is waiting for cb_event.
     * Prevents events that need to be dispatched from being discarded before calling trs_cb_event_wait
     */
    (void)halEschedWaitEvent(dev_id, TRS_CB_EVENT_GRP_ID, tid, 0, &event); /* timeout 0, means no wait */
}

int trs_cb_event_init(uint32_t dev_id)
{
    struct esched_grp_para grp_para;
    unsigned long event_bitmap = (0x1 << EVENT_TS_CALLBACK_MSG);
    unsigned int tid;
    int ret;

    /* process attach device only need once */
    ret = halEschedAttachDevice(dev_id);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_PROCESS_REPEAT_ADD) {
            trs_err("Call halEschedAttachDevice failed. (dev_id=%u, ret=%d)\n", dev_id, ret);
            return ret;
        }
    }

    /* process attach device only need once */
    grp_para.type = GRP_TYPE_BIND_CP_CPU;
    grp_para.threadNum = CALLBACK_EVENT_THREAD_MAX;
    grp_para.rsv[0] = 0; // rsv word 0
    grp_para.rsv[1] = 0; // rsv word 1
    grp_para.rsv[2] = 0; // rsv word 2

    ret = esched_create_extend_grp(dev_id, TRS_CB_EVENT_GRP_ID, &grp_para);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_GROUP_EXIST) {
            (void)halEschedDettachDevice(dev_id);
            trs_err("Call halEschedCreateGrp failed. (dev_id=%u, ret=%d)\n", dev_id, ret);
            return ret;
        }
    }

    /* all the threads subscribe the callback event */
    for (tid = 0; tid < CALLBACK_EVENT_THREAD_MAX; tid++) {
        ret = halEschedSubscribeEvent(dev_id, TRS_CB_EVENT_GRP_ID, tid, event_bitmap);
        if (ret != DRV_ERROR_NONE) {
            (void)halEschedDettachDevice(dev_id);
            trs_err("Call halEschedSubscribeEvent failed. (dev_id=%u, tid=%u; ret=%d)\n", dev_id, tid, ret);
            return ret;
        }
        trs_cb_event_wait_init(dev_id, tid);
    }
    return DRV_ERROR_NONE;
}

void trs_cb_event_uninit(uint32_t dev_id)
{
    (void)halEschedDettachDevice(dev_id);
}

static void trs_cb_event_to_cqe(struct trs_cb_stars_event *event, struct trs_cb_cqe *cqe)
{
    cqe->cq_id = (unsigned short)(event->cqid & 0xfff);
    cqe->stream_id = event->stream_id;
    cqe->event_id = event->event_id;
    cqe->task_id = event->task_id;
    cqe->is_block = (unsigned char)event->is_block;
    cqe->host_func_cb_ptr = (((uint64_t)event->host_func_low) | ((uint64_t)event->host_func_high << 32)); /* bit 32 */
    cqe->fn_data_ptr = (((uint64_t)event->fn_data_low) | ((uint64_t)event->fn_data_high << 32));          /* bit 32 */
}

int trs_cb_event_wait(uint32_t dev_id, uint32_t tid, int32_t timeout, uint8_t *buff)
{
    struct trs_cb_cqe *cqe = (struct trs_cb_cqe *)buff;
    struct event_info event;
    int ret;

    if (cqe == NULL) {
        trs_err("Null ptr. (dev_id=%u, tid =%d)\n", dev_id, tid);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = halEschedWaitEvent(dev_id, TRS_CB_EVENT_GRP_ID, tid, timeout, &event);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        trs_warn("Call halEschedWaitEvent warn. (dev_id=%u, tid =%d; ret=%d)\n", dev_id, tid, ret);
#endif
        return ret;
    }

    if (event.comm.subevent_id == TRS_CB_HW_TIMEOUT_SUBEVENTID) {
        return DRV_ERROR_WAIT_TIMEOUT; /* cb Id is freed. Use this subevent to wakeup thread. */
    } else if (event.comm.subevent_id == TRS_CB_SW_SUBEVENTID) {
        if (event.priv.msg_len == sizeof(*cqe)) {
            (void)memcpy_s((void *)cqe, sizeof(*cqe), event.priv.msg, sizeof(*cqe));
            return DRV_ERROR_NONE;
        }
    } else {
        if (event.priv.msg_len == sizeof(struct trs_cb_stars_event)) {
            trs_cb_event_to_cqe((struct trs_cb_stars_event *)(event.priv.msg), cqe);
            return DRV_ERROR_NONE;
        }
    }

    trs_err("Event Msg len is error. (dev_id=%u, tid=%d; msg_len=%u)\n", dev_id, tid, event.priv.msg_len);
    return DRV_ERROR_PARA_ERROR;
}

/* process submit event, the device id is the destination devid */
int trs_cb_event_submit(uint32_t dev_id, char *sqe, uint32_t e_size)
{
    struct event_summary event;
    drvError_t ret;

    event.pid = getpid();
    event.grp_id = TRS_CB_EVENT_GRP_ID;
    event.event_id = EVENT_TS_CALLBACK_MSG;
    event.subevent_id = TRS_CB_HW_SUBEVENTID;
    event.dst_engine = TS_CPU;
    event.policy = ONLY;
    event.msg_len = e_size;
    event.msg = sqe;

    ret = halEschedSubmitEvent(dev_id, &event);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Call halEschedSubmitEvent failed. (dev_id=%u, ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
