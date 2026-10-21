/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <fcntl.h>

#include "drv_buff_common.h"
#include "ascend_hal.h"
#include "drv_buff_unibuff.h"
#include "drv_buff_mbuf.h"
#include "drv_buff_maintain.h"
#include "buff_manage_base.h"
#include "buff_manage_kernel_api.h"

#define ATOMIC_SET(x, y) __sync_lock_test_and_set((x), (y))

STATIC uint64 THREAD g_process_uni_id = 0; // process node's unique id
pthread_mutex_t g_buff_mutex = PTHREAD_MUTEX_INITIALIZER;

STATIC struct buff_config_para_adp g_buff_config[BUFF_CONF_MAX] = {
    [BUFF_CONF_MBUF_TIMEOUT_CHECK] = {(uint32)sizeof(struct MbufTimeoutCheckPara), buff_timeout_mbuf_check},
    [BUFF_CONF_MBUF_TIMESTAMP_SET] = {0, buff_set_mbuf_timestamp},
};

STATIC struct buff_get_info_adp g_buff_get_info[BUFF_GET_MAX] = {
    [BUFF_GET_MBUF_TIMEOUT_INFO] = {0, buff_get_timeout_mbuf_info},
    [BUFF_GET_MBUF_USE_INFO] = {0, buff_get_mbuf_use_info},
    [BUFF_GET_MBUF_TYPE_INFO] = {0, buff_get_mbuf_type_info},
    [BUFF_GET_BUFF_TYPE_INFO] = {0, buff_get_buff_type_info},
    [BUFF_GET_MEMPOOL_INFO] = {0, buff_get_mempool_info},
    [BUFF_GET_MEMPOOL_BLK_AVAILABLE] = {0, buff_get_mempool_blk_available}};

void buff_set_pid(pid_t pid)
{
    buff_set_pid_base(pid);
}

int buff_api_getpid(void)
{
    return buff_api_getpid_base();
}

void buff_set_process_uni_id(uint64 node_id)
{
    ATOMIC_SET(&g_process_uni_id, node_id);
}

uint64 buff_get_process_uni_id(void)
{
    return g_process_uni_id;
}

unsigned long buff_make_devid_to_flags(int devid, unsigned long flags)
{
    return ((((unsigned long)devid) << BUFF_FLAGS_DEVID_OFFSET) | flags); // lint !e571
}

int buff_get_devid_from_flags(unsigned long flags)
{
    return (int)((unsigned int)(flags >> BUFF_FLAGS_DEVID_OFFSET) & 0xff); // lint !e571
}

unsigned int buff_get_all_devid_flag(unsigned long flags)
{
    return ((unsigned int)(flags >> BUFF_FLAGS_ALL_DEVID_OFFSET) & 0x1);
}

int halBuffGetPhyAddr(void *buf, unsigned long long *phyAddr)
{
    (void)buf;
    (void)phyAddr;
    return 0;
}

int halBuffRecycleByPid(int pid)
{
    (void)pid;
    return DRV_ERROR_NONE;
}

drvError_t buff_timeout_mbuf_check(struct buff_config_handle_arg *para_in)
{
    (void)para_in;
    return DRV_ERROR_NONE;
}

int halBuffCfg(enum BuffConfCmdType cmd, void *data, unsigned int len)
{
    struct buff_config_handle_arg para;

    if (data == NULL) {
        buff_err("invalid para, cmd:%u, data:0x%lx\n", cmd, (uintptr_t)data);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    if ((cmd >= BUFF_CONF_MAX) || (g_buff_config[cmd].config_handle == NULL)) {
        return (int)DRV_ERROR_NOT_SUPPORT;
    }

    if (len != g_buff_config[cmd].cmd_len) {
        buff_err("invalid len:%u, cmd size:%u, cmd:%u\n", len, g_buff_config[cmd].cmd_len, cmd);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    para.data = data;
    return (g_buff_config[cmd].config_handle)(&para);
}

drvError_t buff_get_timeout_mbuf_info(struct buff_get_info_handle_arg *para_in)
{
    para_in->out_size = 0;
    return DRV_ERROR_NONE;
}

int halBuffGetInfo(enum BuffGetCmdType cmd, void *inBuff, unsigned int inLen, void *outBuff, unsigned int *outLen)
{
    struct buff_get_info_handle_arg para;
    int ret = DRV_ERROR_INVALID_VALUE;

    if ((outBuff == NULL) || (outLen == NULL)) {
        buff_err("invalid para, cmd:%u, out_buff:0x%lx, out_len:0x%lx\n", cmd, (uintptr_t)outBuff, (uintptr_t)outLen);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((*outLen == 0) || ((inLen != 0) && (inBuff == NULL)) || ((inLen == 0) && (inBuff != NULL))) {
        buff_err("invalid para, out_len:%u, in_len:%u, in_buff:0x%lx\n", *outLen, inLen, (uintptr_t)inBuff);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((cmd < 0) || (cmd >= BUFF_GET_MAX) || (g_buff_get_info[cmd].get_info_handle == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    para.in_size = inLen;
    para.out_size = *outLen;
    para.in = inBuff;
    para.out = outBuff;

    ret = (int)(g_buff_get_info[cmd].get_info_handle)(&para);
    if (ret != 0) {
        buff_err("buff get info fail:%d, cmd:%u\n", ret, cmd);
        return ret;
    }
    *outLen = para.out_size;

    return ret;
}
