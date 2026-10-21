/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DEVDRV_COMMON_H
#define DEVDRV_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include "securec.h"
#include "ascend_inpackage_hal.h"
#include "dmc_user_interface.h"

#define DEVDRV_DEVICE_WORK_FLAG 0xAABBCDEF

#define DEV_DRV_MAX_PATH_LEN 128
#define DEV_DDR_HBM_MEM_INFO_NUM 4
#define DEV_DDR_HBM_CAPACITY 1
#define DEV_DDR_HBM_UTILIZATION 2

#ifndef REQ_D_INFO_DEV_TYPE_MEM
#define REQ_D_INFO_DEV_TYPE_MEM 1 // lint !e652
#endif

#ifndef REQ_D_INFO_DEV_TYPE_HBM
#define REQ_D_INFO_DEV_TYPE_HBM 6 // lint !e652
#endif

#ifndef REQ_D_INFO_DEV_TYPE_AI_CPU
#define REQ_D_INFO_DEV_TYPE_AI_CPU 3
#endif

#ifndef REQ_D_INFO_DEV_TYPE_CTRL_CPU
#define REQ_D_INFO_DEV_TYPE_CTRL_CPU 4
#endif

#define DDR_NODE_INDEX 0
#define HBM_NODE_INDEX 1
#define HBM_P2P_NODE_INDEX 2

#define DEVDRV_DEVICE_NAME_BUF_SIZE 32
#define DEVDRV_INVALID_FD_OR_INDEX (-1)
#define DEVDRV_NEVER_ENTER 0
#define DEVDRV_ENTERED 1
#define DEVDRV_DEVICE_PATH "/dev/"
#define DEVDRV_DEVICE_NAME "davinci"
#define DEVDRV_VDEVICE_NAME "vdavinci"

#define AICORE_SHOW_INFO_COUNT 100

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#include "ascend_hal.h"
#include "devdrv_user_common.h"

#define DEVDRV_SHA256_DIGEST 32

struct devdrv_statistic_info {
    /* Key Information Count */
    u64 cmdcount;
    u64 reportcount;

    /* Receive Normal Count */
    /* Error Count */
    u64 irqwaittimeout;
};

#if defined(DEVDRV_USER_UT_TEST) || defined(DEVDRV_MANAGER_HOST_UT_TEST)
#define DEVDRV_DRV_ERR(fmt...) printf(fmt)
#define DEVDRV_DRV_WARN(fmt...) printf(fmt)
#define DEVDRV_DRV_EVENT(fmt...) printf(fmt)
#define DEVDRV_DRV_INFO(fmt...) printf(fmt)
#define DEVDRV_DRV_DEBUG(fmt...)
#define DEVDRV_DRV_ERR_EXTEND(fmt...)
#define DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, fmt, ...)
#else
#define MODULE_DEVDRV "devdrv"
#define DEVDRV_DRV_ERR(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DEVDRV_DRV_WARN(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DEVDRV_DRV_EVENT(fmt, ...) DRV_RUN_INFO(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DEVDRV_DRV_INFO(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)
#define DEVDRV_DRV_DEBUG(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_DEV_MANAGER, fmt, ##__VA_ARGS__)

#define DEVDRV_DRV_ERR_EXTEND(ret, no_error_code, fmt, ...) \
    do {                                                    \
        if ((ret) != (no_error_code)) {                     \
            DEVDRV_DRV_ERR(fmt, ##__VA_ARGS__);             \
        } else {                                            \
            DEVDRV_DRV_WARN(fmt, ##__VA_ARGS__);            \
        }                                                   \
    } while (0)

#define DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, fmt, ...)                      \
    do {                                                                 \
        if (((ret) != DRV_ERROR_NOT_SUPPORT) && ((ret) != EOPNOTSUPP)) { \
            DEVDRV_DRV_ERR(fmt, ##__VA_ARGS__);                          \
        }                                                                \
    } while (0)
#endif /* end of DEVDRV_USER_UT_TEST */

#ifdef DEVDRV_USER_UT_TEST
#define STATIC
#else
#define STATIC static
#endif

#define DEVDRV_MANAGER_DEVICE_ENV 0
#define DEVDRV_MANAGER_HOST_ENV 1

#define DEVDRV_HCCL_CHANNEL_BUS 0
#define DEVDRV_HCCL_CHANNEL_PCIE 1
#define DEVDRV_HCCL_CHANNEL_HCCS 2

#define DEVDRV_GPIOIRQ_THREAD_STOP 0x3A4A

#define DEVDRV_MANAGER_MATRIX_INVALID 0
#define DEVDRV_MANAGER_MATRIX_VALID 1

#ifndef ERESTARTSYS
#define ERESTARTSYS 512
#endif

#ifdef __linux
int devdrv_open_device_manager(void);
int devdrv_get_dev_manager_fd(void);
#else
HANDLE devdrv_open_device_manager(void);
HANDLE devdrv_get_dev_manager_fd(void);
#endif

#ifdef SYSTRACE_ON
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <cutils/trace.h>
#define DRV_ATRACE_BEGIN(VAR) ATRACE_BEGIN(#VAR)
#define DRV_ATRACE_END() ATRACE_END()
#else
#define DRV_ATRACE_BEGIN(VAR)
#define DRV_ATRACE_END()
#endif

#define OK 0

#define DEVDRV_CHECK_INFO_PARA(info)                                                                           \
    do {                                                                                                       \
        if ((info) == NULL) {                                                                                  \
            return DRV_ERROR_INVALID_VALUE;                                                                    \
        }                                                                                                      \
        if (((info)->sqeSize) != DEVDRV_CB_SQCQ_MAX_SIZE || ((info)->cqeSize) > DEVDRV_CB_SQCQ_MAX_SIZE ||     \
            ((info)->cqeSize) < DEVDRV_CB_SQCQ_MIN_SIZE) {                                                     \
            TSDRV_PRINT_ERR(                                                                                   \
                "invalid size, sq_size(%u) cq_size(%u), expected sq_size=%u, valid cq_size range=[%u, %u].\n", \
                ((info)->sqeSize), (info)->cqeSize, DEVDRV_CB_SQCQ_MAX_SIZE, DEVDRV_CB_SQCQ_MIN_SIZE,          \
                DEVDRV_CB_SQCQ_MAX_SIZE);                                                                      \
            return DRV_ERROR_INVALID_VALUE;                                                                    \
        }                                                                                                      \
    } while (0)

#define DEVDRV_CHECK_ID_PARA(info, devId)                                                         \
    do {                                                                                          \
        if ((info) == NULL) {                                                                     \
            TSDRV_PRINT_ERR("param is null pointer\n");                                           \
            return DRV_ERROR_INVALID_VALUE;                                                       \
        }                                                                                         \
        if (((info)->tsId) >= DEVDRV_MAX_TS_NUM || (devId) >= DEVDRV_MAX_DAVINCI_NUM) {           \
            TSDRV_PRINT_ERR("invalid tsid(%u) or invalid devid(%u).\n", ((info)->tsId), (devId)); \
            return DRV_ERROR_INVALID_VALUE;                                                       \
        }                                                                                         \
    } while (0)

#define TSDRV_CHECK_RESOURCEID_PARA(in, out, devId)                                             \
    do {                                                                                        \
        if ((in) == NULL) {                                                                     \
            TSDRV_PRINT_ERR("param input is null pointer\n");                                   \
            return DRV_ERROR_INVALID_VALUE;                                                     \
        }                                                                                       \
        if ((out) == NULL) {                                                                    \
            TSDRV_PRINT_ERR("param output is null pointer\n");                                  \
            return DRV_ERROR_INVALID_VALUE;                                                     \
        }                                                                                       \
        if (((in)->tsId) >= DEVDRV_MAX_TS_NUM || (devId) >= DEVDRV_MAX_DAVINCI_NUM) {           \
            TSDRV_PRINT_ERR("invalid tsid(%u) or invalid devid(%u).\n", ((in)->tsId), (devId)); \
            return DRV_ERROR_INVALID_VALUE;                                                     \
        }                                                                                       \
    } while (0)

#define DEVDRV_CHECK_CB_SQ_ID_PARA(sq_id)                   \
    do {                                                    \
        if ((sq_id) >= DEVDRV_CB_SQ_MAX_NUM) {              \
            TSDRV_PRINT_ERR("invalid sqid(%u)\n", (sq_id)); \
            return DRV_ERROR_INVALID_VALUE;                 \
        }                                                   \
    } while (0)

#define DEVDRV_CHECK_CB_CQ_ID_PARA(cq_id)                   \
    do {                                                    \
        if ((cq_id) >= DEVDRV_CB_CQ_MAX_NUM) {              \
            TSDRV_PRINT_ERR("invalid cqid(%u)\n", (cq_id)); \
            return DRV_ERROR_INVALID_VALUE;                 \
        }                                                   \
    } while (0)

int dmanage_run_proc(char **arg);
drvError_t drv_get_info_from_dev_info(uint32_t devId, int32_t info_type, int64_t *value);
drvError_t devdrv_close_restore_device_manager(void);
#endif
