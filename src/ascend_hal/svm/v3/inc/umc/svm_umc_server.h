/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SVM_UMC_SERVER_H
#define SVM_UMC_SERVER_H

#include <stdint.h>

#include "ascend_hal_define.h"
#include "ascend_hal_error.h"
#include "esched_user_interface.h"

#include "svm_pub.h"
#include "svm_log.h"

struct svm_event_proc {
    int (*proc_func)(u32 devid, const void *msg_in, void *msg_out);
    u64 msg_in_len;
    u64 msg_out_len;
    bool need_rsp;
};

#define SVM_EVENT_PROC_REGISTER(subevent_id, func, in_len, out_len)                                                    \
    static struct svm_event_proc _##func = {                                                                           \
        .proc_func = func,                                                                                             \
        .msg_in_len = in_len,                                                                                          \
        .msg_out_len = out_len,                                                                                        \
        .need_rsp = true,                                                                                              \
    };                                                                                                                 \
    static drvError_t _##func##_proc(unsigned int devId, const void *msg, int msg_len, struct drv_event_proc_rsp *rsp) \
    {                                                                                                                  \
        drvError_t ret;                                                                                                \
                                                                                                                       \
        rsp->need_rsp = _##func.need_rsp;                                                                              \
        rsp->real_rsp_data_len = 0;                                                                                    \
                                                                                                                       \
        if ((u64)msg_len < _##func.msg_in_len) {                                                                       \
            svm_err("Msg in len err. (msg_in_len=%d; expected_len=0x%llx)\n", msg_len, _##func.msg_in_len);            \
            return DRV_ERROR_INNER_ERR;                                                                                \
        }                                                                                                              \
                                                                                                                       \
        if ((u64)rsp->rsp_data_buf_len < _##func.msg_out_len) {                                                        \
            svm_err("Msg out len err. (rsp_out_len=%d; expected_len=0x%llx)\n", rsp->rsp_data_buf_len,                 \
                    _##func.msg_out_len);                                                                              \
            return DRV_ERROR_INNER_ERR;                                                                                \
        }                                                                                                              \
                                                                                                                       \
        ret = _##func.proc_func(devId, msg, rsp->rsp_data_buf);                                                        \
        rsp->real_rsp_data_len = (int)_##func.msg_out_len;                                                             \
                                                                                                                       \
        return ret;                                                                                                    \
    }                                                                                                                  \
    static struct drv_event_proc __##func = {                                                                          \
        .proc_func = _##func##_proc, .proc_size = in_len, .proc_name = "svm_" #func};                                  \
    static void __attribute__((constructor)) svm_event_proc_##func##_register(void)                                    \
    {                                                                                                                  \
        drv_registert_event_proc(subevent_id, &__##func);                                                              \
        svm_debug("svm event proc register. (subevent_id=%d; name=%s)\n", subevent_id, __##func.proc_name);            \
    }

#endif
