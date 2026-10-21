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
#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "prof_common.h"
#include "prof_sample_ts_channel.h"
#include "svm_user_interface.h"
#include "prof_adapt_user.h"
#include "dms/dms_devdrv_info_comm.h"

/* Global buffer info array*/
host_user_profile_buffer_info g_host_user_prof_buff_info[DEV_NUM][HOST_MOVE_PROFILE_CHANNEL_COUNT];

/* ========== SVM batch register/unregister helpers ========== */

static void prof_sample_fill_reg_entries(host_user_profile_buffer_info *host_info,
                                         struct prof_svm_reg_entry entries[PROF_SVM_REG_ENTRY_COUNT])
{
    entries[PROF_SVM_REG_WPTR] = (struct prof_svm_reg_entry){.user_va = host_info->wptr_aligned_user_va,
                                                             .size = (uint64_t)MEM_PAGE_SIZE,
                                                             .name = "wptr",
                                                             .access_va_ptr = &host_info->wptr_access_va};
    entries[PROF_SVM_REG_RPTR] = (struct prof_svm_reg_entry){.user_va = host_info->rptr_aligned_user_va,
                                                             .size = (uint64_t)MEM_PAGE_SIZE,
                                                             .name = "rptr",
                                                             .access_va_ptr = &host_info->rptr_access_va};
    entries[PROF_SVM_REG_BUFFER_BASE] = (struct prof_svm_reg_entry){
        .user_va = host_info->user_profile_buffer_info.buffer_base_user_va,
        .size = (uint64_t)host_info->user_profile_buffer_info.buffer_size,
        .name = "buffer_base",
        .access_va_ptr = &host_info->buffer_base_access_va};
}

static void prof_sample_build_reg_entries(const ts_user_profile_buffer_info_t *info,
                                          host_user_profile_buffer_info *host_info,
                                          struct prof_svm_reg_entry entries[PROF_SVM_REG_ENTRY_COUNT])
{
    uint64_t page_size = (uint64_t)MEM_PAGE_SIZE;
    host_info->wptr_aligned_user_va = ALIGN_DOWN(info->buffer_write_ptr_user_va, page_size);
    host_info->rptr_aligned_user_va = ALIGN_DOWN(info->buffer_read_ptr_user_va, page_size);
    prof_sample_fill_reg_entries(host_info, entries);
}

static drvError_t prof_sample_register_svm_batch(uint32_t dev_id,
                                                 struct prof_svm_reg_entry entries[PROF_SVM_REG_ENTRY_COUNT])
{
    drvError_t ret = DRV_ERROR_NONE;
    int registered_count = 0;

    for (uint32_t i = 0; i < PROF_SVM_REG_ENTRY_COUNT; i++) {
        ret = halSvmAccessRegister(dev_id, entries[i].user_va, entries[i].size, SVM_REGISTER_FLAG_ACCESS_BY_DMA,
                                   entries[i].access_va_ptr);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("%s halSvmAccessRegister failed. (dev_id=%u, ret=%d)", entries[i].name, dev_id, ret);
            for (int j = registered_count - 1; j >= 0; j--) {
                drvError_t unret = halSvmAccessUnRegister(dev_id, entries[j].user_va, SVM_REGISTER_FLAG_ACCESS_BY_DMA);
                if (unret != DRV_ERROR_NONE) {
                    PROF_ERR("%s halSvmAccessUnRegister rollback failed. (dev_id=%u, ret=%d)", entries[i].name, dev_id,
                             unret);
                }
                *(entries[j].access_va_ptr) = 0;
            }
            return ret;
        }
        registered_count++;
    }
    return DRV_ERROR_NONE;
}

static drvError_t prof_sample_unregister_svm_batch(uint32_t dev_id,
                                                   struct prof_svm_reg_entry entries[PROF_SVM_REG_ENTRY_COUNT])
{
    drvError_t first_error = DRV_ERROR_NONE;
    for (uint32_t i = 0; i < PROF_SVM_REG_ENTRY_COUNT; i++) {
        drvError_t ret = halSvmAccessUnRegister(dev_id, entries[i].user_va, SVM_REGISTER_FLAG_ACCESS_BY_DMA);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("%s halSvmAccessUnRegister failed. (dev_id=%u, ret=%d)", entries[i].name, dev_id, ret);
            if (first_error == DRV_ERROR_NONE) {
                first_error = ret;
            }
        }
    }
    return first_error;
}

/* ========== Common helper fuctions =========== */

/**
 * Check sample parameters validity
 */
static drvError_t prof_sample_para_check(const struct prof_sample_para *para,
                                         const host_user_profile_buffer_info *host_info, uint32_t buf_unit)
{
    if ((para->buff == NULL) || buf_unit == 0) {
        PROF_ERR("Invalid para. (para=%p, buf_unit=%u)", para, buf_unit);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (host_info == NULL) {
        PROF_ERR("host_info is NULL.");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (host_info->user_profile_buffer_info.buffer_base_user_va == 0) {
        PROF_ERR("TS profile buffer not initialized.");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (host_info->host_pid != getpid()) {
        PROF_ERR("Pid mismatch. (prof sample host_pid=%d, current pid=%d)", host_info->host_pid, getpid());
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

/**
 * Copy data from device buffer to host buffer
 */
static drvError_t prof_sample_copy_data(struct prof_sample_para *para, const host_user_profile_buffer_info *host_info,
                                        uint32_t read_index, uint32_t write_index, uint32_t *copy_size)
{
    /* No new data */
    if (read_index == write_index) {
        PROF_DEBUG("No new data to report.");
        return DRV_ERROR_NONE;
    }

    drvError_t ret;
    *copy_size = 0;
    uint64_t access_va = host_info->buffer_base_access_va;
    if (read_index > write_index) {
        /* Ring buffer wrap-around: copy in two segments */
        uint32_t first_copy = host_info->user_profile_buffer_info.buffer_size - read_index;
        *copy_size = first_copy + write_index;
        if (*copy_size > para->buff_len) {
            PROF_WARN("Ring buffer wrap-around: Device profiling sample data length is grater host buffer length."
                      "(read_index=%u, write_index=%u, used_len=%u, remain_len=%u)",
                      read_index, write_index, *copy_size, SIZE_2M - *copy_size);
            *copy_size = para->buff_len;
            first_copy = *copy_size > first_copy ? first_copy : *copy_size;
        }

        /* First segment: read_index to buffer end*/
        ret = halSvmAccess(para->dev_id, access_va + (uint64_t)read_index, (uint64_t)(uintptr_t)para->buff,
                           (uint64_t)first_copy, SVM_MEM_ACCESS_READ);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("D2H copy first segment failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
            return ret;
        }

        if (*copy_size > first_copy) {
            uint32_t second_copy = *copy_size - first_copy;
            /* Second segment: buffer start to write_index */
            ret = halSvmAccess(para->dev_id, access_va, (uint64_t)(uintptr_t)para->buff + (uint64_t)first_copy,
                               (uint64_t)(second_copy), SVM_MEM_ACCESS_READ);
            if (ret != DRV_ERROR_NONE) {
                PROF_ERR("D2H copy second segment failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
                return ret;
            }
        }
    } else {
        /* Normal case: single segment copy */
        *copy_size = write_index - read_index;
        if (*copy_size > para->buff_len) {
            PROF_WARN("Normal case: Device profiling sample data length is grater host buffer length."
                      "(read_index=%u, write_index=%u, used_len=%u, remain_len=%u)",
                      read_index, write_index, *copy_size, SIZE_2M - *copy_size);
            *copy_size = para->buff_len;
        }

        ret = halSvmAccess(para->dev_id, access_va + (uint64_t)read_index, (uint64_t)(uintptr_t)para->buff,
                           (uint64_t)(*copy_size), SVM_MEM_ACCESS_READ);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("D2H copy failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

static drvError_t prof_sample_copy_data_and_update_read_pointer(struct prof_sample_para *para,
                                                                host_user_profile_buffer_info *host_info, uint32_t wptr,
                                                                uint32_t buf_unit)
{
    uint32_t copy_size = 0;

    /* Calculate write index */
    uint32_t write_index = wptr * buf_unit;
    uint32_t read_index = host_info->read_index;

    if (write_index >= host_info->user_profile_buffer_info.buffer_size) {
        PROF_ERR("Write index invalid. (write_index=%u, buffer_size=%u)", write_index,
                 host_info->user_profile_buffer_info.buffer_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* Copy data from device buffer */
    drvError_t ret = prof_sample_copy_data(para, host_info, read_index, write_index, &copy_size);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Copy data failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
        return ret;
    }

    /* Update host side read index */
    host_info->read_index = (copy_size == para->buff_len) ?
                                ((read_index + copy_size) % host_info->user_profile_buffer_info.buffer_size) :
                                write_index;
    uint32_t rptr = (copy_size == para->buff_len) ? (host_info->read_index / buf_unit) : wptr;
    /* H2D: Update device side read pointer */
    uint64_t rptr_access_va = host_info->rptr_access_va + (host_info->user_profile_buffer_info.buffer_read_ptr_user_va -
                                                           host_info->rptr_aligned_user_va);
    ret = halSvmAccess(para->dev_id, rptr_access_va, (uint64_t)(uintptr_t)(&rptr), sizeof(uint32_t),
                       SVM_MEM_ACCESS_WRITE);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Update read pointer failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
        return ret;
    }

    /* Set report length */
    para->report_len = copy_size;
    return DRV_ERROR_NONE;
}

/* ========== Common callback implementations ========== */

static int prof_sample_common_start_func(struct prof_sample_start_para *para, uint32_t channel_idx)
{
    ts_user_profile_buffer_info_t *info = NULL;

    if (para == NULL) {
        PROF_ERR("Para is NULL.");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (para->dev_id >= DEV_NUM) {
        PROF_ERR("dev_id invalid. (dev_id=%u, max dev_id=%u)", para->dev_id, DEV_NUM - 1);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (para->phease != PROF_HOST_SAMPLE_PHEASE2) {
        PROF_DEBUG("The ts channel only needs to invoke the post-processing of host start. (channel_idx=%u, phease=%d)",
                   channel_idx, para->phease);
        return DRV_ERROR_NONE;
    }
    if ((para->out_data == NULL) || (para->out_data_len == 0)) {
        PROF_ERR("Out data invalid. (out_data=%p, out_data_len=%u)", para->out_data, para->out_data_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->out_data_len != sizeof(ts_user_profile_buffer_info_t)) {
        PROF_ERR("Out data len mismatch. (out_data_len=%u, expected=%zu.)", para->out_data_len,
                 sizeof(ts_user_profile_buffer_info_t));
        return DRV_ERROR_INVALID_VALUE;
    }

    info = (ts_user_profile_buffer_info_t *)para->out_data;
    (void)memcpy_s(&g_host_user_prof_buff_info[para->dev_id][channel_idx].user_profile_buffer_info,
                   sizeof(ts_user_profile_buffer_info_t), info, sizeof(ts_user_profile_buffer_info_t));
    g_host_user_prof_buff_info[para->dev_id][channel_idx].read_index = 0;
    g_host_user_prof_buff_info[para->dev_id][channel_idx].host_pid = getpid();

    /* Batch Register user_va, and get access_va. */
    struct prof_svm_reg_entry entries[PROF_SVM_REG_ENTRY_COUNT];
    prof_sample_build_reg_entries(info, &g_host_user_prof_buff_info[para->dev_id][channel_idx], entries);
    drvError_t ret = prof_sample_register_svm_batch(para->dev_id, entries);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    return DRV_ERROR_NONE;
}

static int prof_sample_common_stop_func(struct prof_sample_stop_para *para, uint32_t channel_idx)
{
    if (para == NULL) {
        PROF_ERR("Para is NULL.");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (para->dev_id >= DEV_NUM) {
        PROF_ERR("dev_id invalid. (dev_id=%u, max dev_id=%u)", para->dev_id, DEV_NUM - 1);
        return DRV_ERROR_INVALID_VALUE;
    }

    struct prof_svm_reg_entry entries[PROF_SVM_REG_ENTRY_COUNT];
    prof_sample_fill_reg_entries(&g_host_user_prof_buff_info[para->dev_id][channel_idx], entries);

    drvError_t ret = prof_sample_unregister_svm_batch(para->dev_id, entries);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    (void)memset_s(&g_host_user_prof_buff_info[para->dev_id][channel_idx], sizeof(host_user_profile_buffer_info), 0,
                   sizeof(host_user_profile_buffer_info));
    return DRV_ERROR_NONE;
}

static int prof_sample_common_sample_func(struct prof_sample_para *para, uint32_t channel_idx, uint32_t buf_unit)
{
    host_user_profile_buffer_info *host_info = NULL;
    ts_user_profile_buffer_info_t *user_info = NULL;
    uint32_t wptr = 0;
    drvError_t ret;

    if (para == NULL) {
        PROF_ERR("Para is NULL.");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (para->dev_id >= DEV_NUM) {
        PROF_ERR("dev_id invalid. (dev_id=%u, max dev_id=%u)", para->dev_id, DEV_NUM - 1);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* Get buffer info from global array */
    host_info = &g_host_user_prof_buff_info[para->dev_id][channel_idx];
    user_info = &host_info->user_profile_buffer_info;

    /* Check parameters */
    ret = prof_sample_para_check(para, host_info, buf_unit);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    /* D2H: Get write pointer from device*/
    uint64_t wptr_access_va = host_info->wptr_access_va +
                              (user_info->buffer_write_ptr_user_va - host_info->wptr_aligned_user_va);
    ret = halSvmAccess(para->dev_id, wptr_access_va, (uint64_t)(uintptr_t)(&wptr), sizeof(uint32_t),
                       SVM_MEM_ACCESS_READ);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Get write pointer failed. (dev_id=%u, ret=%d)", para->dev_id, ret);
        return ret;
    }

    /* Copy data from device buffer and update device read poniter */
    ret = prof_sample_copy_data_and_update_read_pointer(para, host_info, wptr, buf_unit);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return DRV_ERROR_NONE;
}

/* ========== SOC_LOG channel callbacks ========== */
static int soc_log_start_func(struct prof_sample_start_para *para)
{
    return prof_sample_common_start_func(para, STARS_PROFILE_SOC_LOG_CHANNEL_IDX);
}

static int soc_log_stop_func(struct prof_sample_stop_para *para)
{
    return prof_sample_common_stop_func(para, STARS_PROFILE_SOC_LOG_CHANNEL_IDX);
}

static int soc_log_sample_func(struct prof_sample_para *para)
{
    return prof_sample_common_sample_func(para, STARS_PROFILE_SOC_LOG_CHANNEL_IDX, STARS_LOG_BUF_UNIT);
}

/* ========== ACC_PROFILE channel callbacks ========== */
static int acc_profile_start_func(struct prof_sample_start_para *para)
{
    return prof_sample_common_start_func(para, STARS_ACC_PROFILE_BUFFER_CHANNEL_IDX);
}

static int acc_profile_stop_func(struct prof_sample_stop_para *para)
{
    return prof_sample_common_stop_func(para, STARS_ACC_PROFILE_BUFFER_CHANNEL_IDX);
}

static int acc_profile_sample_func(struct prof_sample_para *para)
{
    return prof_sample_common_sample_func(para, STARS_ACC_PROFILE_BUFFER_CHANNEL_IDX, STARS_ACC_PROFILE_BUF_UNIT);
}

/* ========== Register interfaces ========== */
int register_soc_log_channel(unsigned int devid)
{
    struct prof_sample_register_para prof_ops;
    drvError_t ret;

    prof_ops.sub_chan_num = PROFILE_LOG_BUFFER_NUM;
    prof_ops.ops.start_func = soc_log_start_func;
    prof_ops.ops.sample_func = soc_log_sample_func;
    prof_ops.ops.flush_func = NULL;
    prof_ops.ops.stop_func = soc_log_stop_func;

    ret = halProfSampleRegisterEx(devid, STARS_PROFILE_SOC_LOG_CHANNEL, &prof_ops);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("50 channel soc_log register error. (devid=%u, channel_id=%u, ret=%d.)\n", devid,
                 STARS_PROFILE_SOC_LOG_CHANNEL, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

int register_acc_profile_channel(unsigned int devid)
{
    struct prof_sample_register_para prof_ops;
    drvError_t ret;

    prof_ops.sub_chan_num = PROFILE_LOG_BUFFER_NUM;
    prof_ops.ops.start_func = acc_profile_start_func;
    prof_ops.ops.sample_func = acc_profile_sample_func;
    prof_ops.ops.flush_func = NULL;
    prof_ops.ops.stop_func = acc_profile_stop_func;

    ret = halProfSampleRegisterEx(devid, STARS_ACC_PROFILE_BUFFER_CHANNEL, &prof_ops);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("53 channel acc_profile register error. (devid=%u, channel_id=%u, ret=%d.)\n", devid,
                 STARS_ACC_PROFILE_BUFFER_CHANNEL, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

typedef int (*prof_sample_register_func)(unsigned int);

static prof_sample_register_func g_ts_prof_sample_info[] = {
    register_soc_log_channel,
    register_acc_profile_channel,
};

static void __attribute__((constructor)) prof_sample_ts_channel_init(void)
{
    unsigned int i;
    int ret;
    unsigned int split_mode = 0;
    unsigned int devid;

    ret = drvGetDeviceSplitMode(0, &split_mode);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("get split mode failed. (ret=%d)\n", ret);
        return;
    }

    if (split_mode != VMNG_NORMAL_NONE_SPLIT_MODE) {
        PROF_INFO("It is not a physical machine. (split_mode=%u)\n", split_mode);
        return;
    }

    for (i = 0; i < sizeof(g_ts_prof_sample_info) / sizeof(g_ts_prof_sample_info[0]); i++) {
        for (devid = 0; devid < DEV_NUM; devid++) {
            ret = g_ts_prof_sample_info[i](devid);
            if (ret != 0) {
                PROF_ERR("ts channel register failed. (devid=%u, i=%u, ret=%d)\n", devid, i, ret);
            }
        }
    }
    return;
}

#else

int prof_sample_ts_channel_ut_test(void)
{
    return 0;
}

#endif