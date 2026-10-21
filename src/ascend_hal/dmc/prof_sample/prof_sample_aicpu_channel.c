/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef UT_TEST
#include "prof_sample_aicpu_channel.h"
#include <string.h>
#include "ascend_inpackage_hal.h"
#include "prof_common.h"

#define AICPU_HAL_MODULE_ID 36
#define AICPU_HOST_MOVE_BUFFER_SIZE (16 * 1024 * 1024ULL)

struct host_user_aicpu_buffer_info g_aicpu_host_user_prof_buff_info[DEV_NUM][HOST_MOVE_AICPU_CHANNEL_COUNT];

static drvError_t aicpu_prof_hal_mem_alloc(uint32_t dev_id, void **ptr, uint64_t size)
{
    uint64_t drv_flag = (uint64_t)MEM_DEV | (uint64_t)MEM_PAGE_HUGE | (uint64_t)NODE_TO_DEVICE(dev_id) |
                        ((uint64_t)AICPU_HAL_MODULE_ID << (uint64_t)MEM_MODULE_ID_BIT);
    drvError_t ret = halMemAlloc(ptr, (unsigned long long)size, (unsigned long long)drv_flag);
    if (ret != DRV_ERROR_NONE || *ptr == NULL) {
        PROF_ERR("halMemAlloc failed. (dev_id=%u, size=%llu, ret=%d)", dev_id, size, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

static void aicpu_prof_hal_mem_free(void *ptr)
{
    if (ptr != NULL) {
        (void)halMemFree(ptr);
    }
}

/* ========== Common helper functions ========== */

static drvError_t aicpu_prof_sample_para_check(const struct prof_sample_para *para,
                                               const host_user_aicpu_buffer_info *host_info, uint32_t buf_unit)
{
    if ((para == NULL) || (para->buff == NULL) || buf_unit == 0) {
        PROF_ERR("Invalid para. (para=%p, buf_unit=%u)", para, buf_unit);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (host_info == NULL) {
        PROF_ERR("host_info is NULL.");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (host_info->user_profile_buffer_info.buffer_base_user_va == 0) {
        PROF_ERR("AICPU profile buffer not initialized.");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (host_info->host_pid != getpid()) {
        PROF_ERR("Pid mismatch. (prof sample host_pid=%d, current pid=%d)", host_info->host_pid, getpid());
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t aicpu_prof_sample_copy_data(struct prof_sample_para *para,
                                              const host_user_aicpu_buffer_info *host_info, uint32_t read_index,
                                              uint32_t write_index, uint32_t *copy_size)
{
    if (read_index == write_index) {
        PROF_DEBUG("No new data to report.");
        return DRV_ERROR_NONE;
    }

    drvError_t ret;
    *copy_size = 0;
    uint64_t dev_buf_va = host_info->user_profile_buffer_info.buffer_base_user_va;

    if (read_index > write_index) {
        uint32_t first_copy = host_info->user_profile_buffer_info.buffer_size - read_index;
        *copy_size = first_copy + write_index;
        if (*copy_size > para->buff_len) {
            PROF_WARN("Ring buffer wrap-around: data too large. (read=%u, write=%u, used=%u)", read_index, write_index,
                      *copy_size);
            *copy_size = para->buff_len;
            first_copy = *copy_size > first_copy ? first_copy : *copy_size;
        }

        ret = drvMemcpy((DVdeviceptr)(uintptr_t)para->buff, (size_t)para->buff_len,
                        (DVdeviceptr)(dev_buf_va + (uint64_t)read_index), (size_t)first_copy);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("D2H copy first segment failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
            return ret;
        }

        if (*copy_size > first_copy) {
            uint32_t second_copy = *copy_size - first_copy;
            ret = drvMemcpy((DVdeviceptr)((uintptr_t)para->buff + (size_t)first_copy),
                            (size_t)(para->buff_len - first_copy), (DVdeviceptr)dev_buf_va, (size_t)second_copy);
            if (ret != DRV_ERROR_NONE) {
                PROF_ERR("D2H copy second segment failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
                return ret;
            }
        }
    } else {
        *copy_size = write_index - read_index;
        if (*copy_size > para->buff_len) {
            PROF_WARN("Normal case: data too large. (read=%u, write=%u, used=%u)", read_index, write_index, *copy_size);
            *copy_size = para->buff_len;
        }

        ret = drvMemcpy((DVdeviceptr)(uintptr_t)para->buff, (size_t)para->buff_len,
                        (DVdeviceptr)(dev_buf_va + (uint64_t)read_index), (size_t)(*copy_size));
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("D2H copy failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

/* ========== Phase-specific start helpers ========== */

static int aicpu_start_phase1(struct prof_sample_start_para *para, uint32_t channel_idx)
{
    PROF_INFO("Phase 1 enter: phase=%u, is_support_host_move=%u, dev_id=%u", para->phease, para->is_support_host_move,
              para->dev_id);

    void *buffer_ptr = NULL;
    void *wptr_ptr = NULL;
    void *rptr_ptr = NULL;
    drvError_t ret;

    ret = aicpu_prof_hal_mem_alloc(para->dev_id, &buffer_ptr, AICPU_HOST_MOVE_BUFFER_SIZE);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = aicpu_prof_hal_mem_alloc(para->dev_id, &wptr_ptr, MEM_PAGE_SIZE);
    if (ret != DRV_ERROR_NONE) {
        aicpu_prof_hal_mem_free(buffer_ptr);
        return ret;
    }

    ret = aicpu_prof_hal_mem_alloc(para->dev_id, &rptr_ptr, MEM_PAGE_SIZE);
    if (ret != DRV_ERROR_NONE) {
        aicpu_prof_hal_mem_free(wptr_ptr);
        aicpu_prof_hal_mem_free(buffer_ptr);
        return ret;
    }

    aicpu_user_profile_buffer_info_t *info = (aicpu_user_profile_buffer_info_t *)para->out_data;
    info->buffer_size = (unsigned int)AICPU_HOST_MOVE_BUFFER_SIZE;
    info->reserved = 0;
    info->buffer_base_user_va = (uint64_t)(uintptr_t)buffer_ptr;
    info->buffer_read_ptr_user_va = (uint64_t)(uintptr_t)rptr_ptr;
    info->buffer_write_ptr_user_va = (uint64_t)(uintptr_t)wptr_ptr;
    para->out_data_len = sizeof(aicpu_user_profile_buffer_info_t);

    g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx].phase1_buffer_ptr = buffer_ptr;
    g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx].phase1_wptr_ptr = wptr_ptr;
    g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx].phase1_rptr_ptr = rptr_ptr;
    g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx].host_pid = getpid();

    (void)memcpy_s(&g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx].user_profile_buffer_info,
                   sizeof(aicpu_user_profile_buffer_info_t), info, sizeof(aicpu_user_profile_buffer_info_t));
    g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx].read_index = 0;

    PROF_INFO("Phase 1: halMemAlloc done. base=0x%llx, size=%u, wptr=0x%llx, rptr=0x%llx, out_data_len=%u",
              info->buffer_base_user_va, info->buffer_size, info->buffer_write_ptr_user_va,
              info->buffer_read_ptr_user_va, para->out_data_len);
    return DRV_ERROR_NONE;
}

static int aicpu_start_phase2(struct prof_sample_start_para *para, uint32_t channel_idx)
{
    if (para->out_data_len != sizeof(aicpu_user_profile_buffer_info_t)) {
        PROF_ERR("Out data len mismatch. (out_data_len=%u, expected=%zu.)", para->out_data_len,
                 sizeof(aicpu_user_profile_buffer_info_t));
        return DRV_ERROR_INVALID_VALUE;
    }

    aicpu_user_profile_buffer_info_t *info = (aicpu_user_profile_buffer_info_t *)para->out_data;
    PROF_INFO("Phase 2: AICPU host move start: base=0x%llx, size=%u, wptr=0x%llx, rptr=0x%llx",
              info->buffer_base_user_va, info->buffer_size, info->buffer_write_ptr_user_va,
              info->buffer_read_ptr_user_va);

    (void)memcpy_s(&g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx].user_profile_buffer_info,
                   sizeof(aicpu_user_profile_buffer_info_t), info, sizeof(aicpu_user_profile_buffer_info_t));
    g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx].read_index = 0;
    g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx].host_pid = getpid();

    PROF_INFO("host move mode: start success, dev_id=%u, host_pid=%d", para->dev_id, getpid());
    return DRV_ERROR_NONE;
}

/* ========== Common callback implementations ========== */

static int prof_sample_common_start_func(struct prof_sample_start_para *para, uint32_t channel_idx)
{
    if (para == NULL) {
        PROF_ERR("Para is NULL.");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (para->out_data == NULL) {
        PROF_ERR("Out data is NULL.");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (para->dev_id >= DEV_NUM) {
        PROF_ERR("Device id is invalid.(dev_id=%u).", para->dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->out_data_max_len < sizeof(aicpu_user_profile_buffer_info_t)) {
        PROF_ERR("Out data max len too small. (max_len=%u, need=%zu)", para->out_data_max_len,
                 sizeof(aicpu_user_profile_buffer_info_t));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->phease == 1U) {
        return aicpu_start_phase1(para, channel_idx);
    }

    PROF_INFO("Phase enter: phase=%u, is_support_host_move=%u, dev_id=%u, out_data_len=%u", para->phease,
              para->is_support_host_move, para->dev_id, para->out_data_len);

    if (para->out_data_len == 0) {
        PROF_ERR("Out data len is 0 for non-phase1 start.");
        return DRV_ERROR_INVALID_VALUE;
    }
    return aicpu_start_phase2(para, channel_idx);
}

/* ========== Host-side stop helpers ========== */
static int prof_stop_release(struct prof_sample_stop_para *para, uint32_t channel_idx)
{
    if (para == NULL) {
        PROF_ERR("Para is NULL.");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (para->dev_id >= DEV_NUM) {
        PROF_ERR("Device id is invalid.(dev_id=%u).", para->dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    host_user_aicpu_buffer_info *host_info = &g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx];

    aicpu_prof_hal_mem_free(host_info->phase1_rptr_ptr);
    aicpu_prof_hal_mem_free(host_info->phase1_wptr_ptr);
    aicpu_prof_hal_mem_free(host_info->phase1_buffer_ptr);

    (void)memset_s(host_info, sizeof(host_user_aicpu_buffer_info), 0, sizeof(host_user_aicpu_buffer_info));
    PROF_INFO("host move mode: release.");
    return DRV_ERROR_NONE;
}

static int prof_sample_common_stop_func(struct prof_sample_stop_para *para, uint32_t channel_idx)
{
    return prof_stop_release(para, channel_idx);
}

static int prof_sample_common_sample_func(struct prof_sample_para *para, uint32_t channel_idx, uint32_t buf_unit)
{
    if (para->dev_id >= DEV_NUM) {
        PROF_ERR("Device id is invalid.(dev_id=%u).", para->dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    host_user_aicpu_buffer_info *host_info = NULL;
    aicpu_user_profile_buffer_info_t *user_info = NULL;
    uint32_t wptr = 0;
    uint32_t write_index = 0;
    uint32_t read_index = 0;
    uint32_t copy_size = 0;
    drvError_t ret;

    host_info = &g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx];
    user_info = &host_info->user_profile_buffer_info;

    ret = aicpu_prof_sample_para_check(para, host_info, buf_unit);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    uint64_t wptr_dev_va = user_info->buffer_write_ptr_user_va;
    ret = drvMemcpy((DVdeviceptr)(uintptr_t)(&wptr), (size_t)sizeof(uint32_t), (DVdeviceptr)wptr_dev_va,
                    (size_t)sizeof(uint32_t));
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Get write pointer failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
        return ret;
    }

    write_index = wptr * buf_unit;
    read_index = host_info->read_index;

    if (write_index >= user_info->buffer_size) {
        PROF_ERR("Write index invalid. (write_index=%u, buffer_size=%u)", write_index, user_info->buffer_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = aicpu_prof_sample_copy_data(para, host_info, read_index, write_index, &copy_size);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Copy data failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
        return ret;
    }

    host_info->read_index = (copy_size == para->buff_len) ?
                                ((read_index + copy_size) % g_aicpu_host_user_prof_buff_info[para->dev_id][channel_idx]
                                                                .user_profile_buffer_info.buffer_size) :
                                write_index;
    uint32_t rptr = (copy_size == para->buff_len) ? (host_info->read_index / buf_unit) : wptr;
    uint64_t rptr_dev_va = user_info->buffer_read_ptr_user_va;
    ret = drvMemcpy((DVdeviceptr)rptr_dev_va, (size_t)sizeof(uint32_t), (DVdeviceptr)(uintptr_t)(&rptr),
                    (size_t)sizeof(uint32_t));
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Update read pointer failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
    }

    para->report_len = copy_size;
    return DRV_ERROR_NONE;
}

/* ========== AICPU channel callbacks ========== */

static int aicpu_start_func(struct prof_sample_start_para *para)
{
    PROF_INFO("start aicpu_start_func");
    return prof_sample_common_start_func(para, AICPU_CHANNEL_IDX);
}

static int aicpu_stop_func(struct prof_sample_stop_para *para)
{
    PROF_INFO("start aicpu_stop_func");
    return prof_sample_common_stop_func(para, AICPU_CHANNEL_IDX);
}

static int aicpu_sample_func(struct prof_sample_para *para)
{
    if (para == NULL) {
        PROF_ERR("Para is NULL.");
        return DRV_ERROR_INVALID_VALUE;
    }
    return prof_sample_common_sample_func(para, AICPU_CHANNEL_IDX, AICPU_BUF_UNIT);
}

/* ========== CUSTOM AICPU channel callbacks ========== */

static int cus_aicpu_start_func(struct prof_sample_start_para *para)
{
    PROF_INFO("start cus_aicpu_start_func");
    return prof_sample_common_start_func(para, CUS_AICPU_CHANNEL_IDX);
}

static int cus_aicpu_stop_func(struct prof_sample_stop_para *para)
{
    PROF_INFO("start cus_aicpu_stop_func");
    return prof_sample_common_stop_func(para, CUS_AICPU_CHANNEL_IDX);
}

static int cus_aicpu_sample_func(struct prof_sample_para *para)
{
    if (para == NULL) {
        PROF_ERR("Para is NULL.");
        return DRV_ERROR_INVALID_VALUE;
    }
    return prof_sample_common_sample_func(para, CUS_AICPU_CHANNEL_IDX, AICPU_BUF_UNIT);
}

/* ========== Register interfaces ========== */
int register_aicpu_channel(unsigned int devid)
{
    struct prof_sample_register_para prof_ops;
    drvError_t ret;

    prof_ops.sub_chan_num = PROFILE_BUFFER_NUM;
    prof_ops.ops.start_func = aicpu_start_func;
    prof_ops.ops.sample_func = aicpu_sample_func;
    prof_ops.ops.flush_func = NULL;
    prof_ops.ops.stop_func = aicpu_stop_func;

    ret = halProfSampleRegisterEx(devid, PROF_CHANNEL_AICPU, &prof_ops);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("aicpu channel register error. (devid=%u, channel_id=%u, ret=%d.)\n", devid, PROF_CHANNEL_AICPU, ret);
        return ret;
    }

    PROF_INFO("register aicpu channel success. devid=%d\n", devid);
    return DRV_ERROR_NONE;
}

int register_cus_aicpu_channel(unsigned int devid)
{
    struct prof_sample_register_para prof_ops;
    drvError_t ret;

    prof_ops.sub_chan_num = PROFILE_BUFFER_NUM;
    prof_ops.ops.start_func = cus_aicpu_start_func;
    prof_ops.ops.sample_func = cus_aicpu_sample_func;
    prof_ops.ops.flush_func = NULL;
    prof_ops.ops.stop_func = cus_aicpu_stop_func;

    ret = halProfSampleRegisterEx(devid, PROF_CHANNEL_CUS_AICPU, &prof_ops);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("cus aicpu channel register error. (devid=%u, channel_id=%u, ret=%d.)\n", devid,
                 PROF_CHANNEL_CUS_AICPU, ret);
        return ret;
    }

    PROF_INFO("register custom aicpu channel success. devid=%d\n", devid);
    return DRV_ERROR_NONE;
}
#else

int prof_sample_aicpu_channel_ut_test(void)
{
    return 0;
}

#endif