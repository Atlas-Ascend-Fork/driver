/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "securec.h"

#include "ascend_hal.h"
#include "esched_user_interface.h"
#include "drv_buff_common.h"
#include "queue.h"

#include "queue_client_comm.h"
#include "que_platform.h"

void queue_updata_timeout(struct timeval start, struct timeval end, int *timeout)
{
    if (*timeout > 0) {
#ifndef EMU_ST
        long tmp = *timeout;
        tmp -= (end.tv_sec - start.tv_sec) * MS_PER_SECOND + (end.tv_usec - start.tv_usec) / US_PER_MSECOND;
        *timeout = tmp > 0 ? (int)tmp : 0;
#endif
    }
}

STATIC void queue_unsubscribe_with_try(unsigned int devid, unsigned int qid, unsigned int unsub_id)
{
    drvError_t ret_val;
    int try_cnt;

    for (try_cnt = 0; try_cnt < UNSUBSCRIBE_TRY_CNT; try_cnt++) {
        ret_val = queue_unsubscribe(devid, qid, unsub_id);
        if (ret_val == DRV_ERROR_NONE) {
            return;
        }
    }
    QUEUE_LOG_ERR("queue_unsubscribe fail. (ret=%d; devid=%u; qid=%u; try_cnt=%d)\n", ret_val, devid, qid, try_cnt);
    return;
}

#ifndef EMU_ST
STATIC drvError_t queue_update_error_code(struct event_info *back_event, drvError_t ret)
{
    struct event_proc_result *result = NULL;

    if ((back_event->priv.msg_len != sizeof(struct event_proc_result))) {
        return ret;
    }

    result = (struct event_proc_result *)back_event->priv.msg;
    if (result->ret == QUEUE_IS_DESTROY_MAGIC) {
        return DRV_ERROR_NOT_EXIST;
    } else if (result->ret == QUEUE_IS_CLEAR_MAGIC) {
        return DRV_ERROR_BUSY;
    } else {
        return ret;
    }
}
#endif

drvError_t queue_wait_event(unsigned int devid, unsigned int qid, int result, int timeout)
{
    unsigned long timestamp = esched_get_cur_cpu_timestamp();
    struct event_info back_event_info = {{0}, {0}};
    unsigned int sub_id, unsub_id;
    drvError_t ret;
    struct event_res res;
    int event_type;

    if (result == DRV_ERROR_QUEUE_EMPTY) {
        event_type = E2NE_EVENT;
        sub_id = DRV_SUBEVENT_SUBE2NE_MSG;
        unsub_id = DRV_SUBEVENT_UNSUBE2NE_MSG;
    } else {
        event_type = F2NF_EVENT;
        sub_id = DRV_SUBEVENT_SUBF2NF_MSG;
        unsub_id = DRV_SUBEVENT_UNSUBF2NF_MSG;
    }

    ret = (drvError_t)esched_alloc_event_res(devid, event_type, &res);
    if (ret != 0) {
#ifndef EMU_ST
        QUEUE_LOG_WARN("event resource can not alloc. (ret=%d; devid=%u; event_type=%d)\n", ret, devid, event_type);
        return ret;
#endif
    }

    ret = queue_subscribe(devid, qid, sub_id, &res);
    if (ret != 0) {
        QUEUE_LOG_ERR("queue_subscribe failed. (ret=%d)\n", ret);
        goto OUT;
    }

    do {
        ret = halEschedWaitEvent(devid, res.gid, res.tid, timeout, &back_event_info);
        if (ret != 0) {
            QUEUE_LOG_WARN("halEschedWaitEvent invalid. (ret=%d; event_id=%u; gid=%u; tid=%u; timeout=%dms)\n", ret,
                           res.event_id, res.gid, res.tid, timeout);
            queue_unsubscribe_with_try(devid, qid, unsub_id);
            goto OUT;
        } else {
#ifndef EMU_ST
            ret = queue_update_error_code(&back_event_info, ret);
            if (ret == DRV_ERROR_NOT_EXIST) {
                goto OUT;
            }
#endif
        }
    } while (back_event_info.comm.submit_timestamp < timestamp);

    queue_unsubscribe_with_try(devid, qid, unsub_id);

OUT:
    esched_free_event_res(devid, event_type, &res);
    return ret;
}

drvError_t queue_param_check(unsigned int dev_id, unsigned int qid, const struct buff_iovec *vector,
                             const char *func_name)
{
    unsigned int i;

    if (dev_id >= MAX_DEVICE) {
        QUEUE_LOG_ERR("devId is invalid.(devid=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (qid >= MAX_SURPORT_QUEUE_NUM) {
        QUEUE_LOG_ERR("qid is invalid.(qid=%u)\n", qid);
        report_arg_out_of_range(func_name, "queue ID", qid, 0, MAX_SURPORT_QUEUE_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (vector == NULL) {
        QUEUE_LOG_ERR("vector is null.\n");
        report_arg_null_pointer(func_name, "vector");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (vector->count == 0 || vector->count > QUEUE_MAX_IOVEC_NUM) {
        QUEUE_LOG_ERR("vector->count can not be zero.\n");
        report_arg_equal_to_zero(func_name, "vector->count");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((vector->context_base == NULL && vector->context_len != 0) ||
        (vector->context_base != NULL && (vector->context_len == 0 || vector->context_len > USER_DATA_LEN))) {
        QUEUE_LOG_ERR("input param is invalid. (context_base_is_NULL=%d; context_len=%llu)\n",
                      (vector->context_base == NULL), vector->context_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < vector->count; i++) {
        if (vector->ptr[i].iovec_base == NULL || vector->ptr[i].len == 0) {
            QUEUE_LOG_ERR("input param is invalid. (i=%d; iovec_base_is_NULL=%d; len=0x%llx)\n", i,
                          (vector->ptr[i].iovec_base == NULL), vector->ptr[i].len);
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    return DRV_ERROR_NONE;
}

void queue_report_insufficient_device_memory(struct buff_iovec *vector, int ret)
{
    unsigned int i;
    unsigned long long buf_len = 0;
    if (ret != (int)DRV_ERROR_OUT_OF_MEMORY) {
        return;
    }
    for (i = 0; i < vector->count; i++) {
        buf_len += vector->ptr[i].len;
    }
    report_insufficient_device_memory(buf_len, halDrvLogGetModuleStr(HAL_MODULE_TYPE_QUEUE_MANAGER));
}
