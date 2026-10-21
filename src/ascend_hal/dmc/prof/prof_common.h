/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef PROF_COMMON_H
#define PROF_COMMON_H
#include "ascend_inpackage_hal.h"
#include "securec.h"
#include "prof_ioctl.h"

#ifdef PROF_UNIT_TEST
#include "ut_log.h"
#define STATIC
/* run log */
#define PROF_RUN_INFO(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_PROF, fmt, ##__VA_ARGS__)
#else
#include "dmc/dmc_log_user.h"
#define STATIC static
/* run log */
#define PROF_RUN_INFO(fmt, ...) DRV_RUN_INFO(HAL_MODULE_TYPE_PROF, fmt, ##__VA_ARGS__)
#endif
#define STATIC_INLINE static inline
/* debug log */
#define PROF_ERR(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_PROF, fmt, ##__VA_ARGS__)
#define PROF_WARN(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_PROF, fmt, ##__VA_ARGS__)
#define PROF_INFO(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_PROF, fmt, ##__VA_ARGS__)
#define PROF_DEBUG(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_PROF, fmt, ##__VA_ARGS__)

/* infrequent log level */
#define PROF_EMERG(fmt, ...) DRV_EMERG(HAL_MODULE_TYPE_PROF, fmt, ##__VA_ARGS__)
#define PROF_ALERT(fmt, ...) DRV_ALERT(HAL_MODULE_TYPE_PROF, fmt, ##__VA_ARGS__)

/* alarm event log, non-alarm events use debug or run log */
#define PROF_CRIT(fmt, ...) DRV_CRIT(HAL_MODULE_TYPE_PROF, fmt, ##__VA_ARGS__)
#define PROF_EVENT(fmt, ...) DRV_EVENT(HAL_MODULE_TYPE_PROF, fmt, ##__VA_ARGS__)

#define PROF_ERR_HEX_DUMP(buf, len, fmt, ...) DRV_ERR_HEX_DUMP(HAL_MODULE_TYPE_PROF, buf, len, fmt, ##__VA_ARGS__)

#define ATOMIC_SET(x, y) (void)__sync_lock_test_and_set((x), (y))
#define ATOMIC_INC(x) __sync_add_and_fetch((x), 1)
#define ATOMIC_DEC(x) __sync_sub_and_fetch((x), 1)
#define ATOMIC_ADD(x, y) __sync_add_and_fetch((x), (y))
#define ATOMIC_SUB(x, y) __sync_sub_and_fetch((x), (y))
#define CAS(ptr, oldval, newval) __sync_bool_compare_and_swap((ptr), (oldval), (newval))

#ifndef UNUSED
#define UNUSED(x)  \
    do {           \
        (void)(x); \
    } while (0)
#endif

typedef struct prof_user_start_para {
    uint32_t remote_pid;
    uint32_t sample_period;
    void *user_data;
    uint32_t user_data_size;
    uint32_t support_host_sample;
    char *start_out_data;
    char *addrdata;
    uint32_t addr_data_len;
} prof_user_start_para_t;

typedef struct prof_user_stop_para {
    uint32_t remote_pid;
    void *report;
    uint32_t report_len;
    bool support_host_sample;
    uint32_t host_sample_release_flag;
    drvError_t h2d_ret;
} prof_user_stop_para_t;

typedef struct prof_user_read_para {
    bool write_rptr_flag;
    char *out_buf;
    uint32_t buf_size;
} prof_user_read_para_t;

STATIC_INLINE bool prof_comm_errcode_no_need_convert(int errcode)
{
    int errcode_no_need_convert[] = {PROF_NOT_ENOUGH_SUB_CHANNEL_RESOURCE, PROF_VF_SUB_RESOURCE_FULL,
                                     PROF_STOPPED_ALREADY, PROF_ERROR};
    uint64_t i;

    for (i = 0; i < sizeof(errcode_no_need_convert) / sizeof(int); i++) {
        if (errcode_no_need_convert[i] == errcode) {
            return true;
        }
    }

    return false;
}

STATIC_INLINE drvError_t prof_comm_errcode_convert(int errcode)
{
    //  no need to convert, ts return code (115) need to be return directly.
    if (errcode >= PROF_OK) {
        return (drvError_t)errcode;
    }

    if ((errcode == PROF_NOT_SUPPORT) || (errcode == -EOPNOTSUPP)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (prof_comm_errcode_no_need_convert(errcode) == true) {
        return (drvError_t)errcode;
    }

    return DRV_ERROR_INNER_ERR;
}
#endif
