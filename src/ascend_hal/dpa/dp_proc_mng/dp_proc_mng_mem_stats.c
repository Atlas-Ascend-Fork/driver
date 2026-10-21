/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/ioctl.h>
#include <time.h>

#include "securec.h"
#include "ascend_hal.h"
#include "ascend_hal_define.h"
#include "dms_user_interface.h"
#include "dp_proc_mng.h"
#include "dp_proc_mng_ioctl.h"
#include "dpa/dpa_dp_proc_mng.h"
#include "dp_adapt.h"

#define NSEC_PER_SEC 1000000000

static THREAD uint32_t g_timestamp_mode = 1; /* Default is cpu_cycle_count */

#if defined(__x86_64__)
static uint64_t get_rdtsc(void)
{
    uint64_t cycles;
    uint32_t hi = 0;
    uint32_t lo = 0;
    const int uint32Bits = 32; // 32 is uint bit count

    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    cycles = ((uint64_t)lo) | ((uint64_t)hi << uint32Bits);

    return cycles;
}
#endif

static uint64_t get_cpu_cycle_count(void)
{
    uint64_t cycles;

#if defined(__aarch64__)
    asm volatile("mrs %0, cntvct_el0" : "=r"(cycles));
#elif defined(__x86_64__)
    cycles = get_rdtsc();
#else
    cycles = 0;
#endif

    return cycles;
}

static uint64_t get_timestamp_ns(void)
{
    struct timespec ts = {0};

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)((long long)ts.tv_sec) * NSEC_PER_SEC + (uint64_t)ts.tv_nsec;
}

static drvError_t dp_proc_mng_prof_para_check(struct prof_sample_para *para)
{
    if (para == NULL) {
        DP_PROC_MNG_ERR("Prof sample para is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->dev_id >= MEM_STATS_DEVICE_CNT) {
        DP_PROC_MNG_ERR("Invalid prof sample para devid. (devid=%u)\n", para->dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->buff == NULL) {
        DP_PROC_MNG_ERR("Prof sample para buff is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->buff_len < sizeof(struct module_mem_info) * MEM_STATS_MAX_MODULE_ID) {
        DP_PROC_MNG_ERR("Invalid prof sample para buff_len. (buff_len=%u, report_len=%u)\n", para->buff_len,
                        (unsigned int)(sizeof(struct module_mem_info) * MEM_STATS_MAX_MODULE_ID));
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

int dp_proc_mng_mem_stats_sample(struct module_mem_info *mem_info, uint32_t num, uint32_t devid)
{
    uint32_t module_id;
    int ret;

    if ((devid >= MEM_STATS_DEVICE_CNT) || (num < MEM_STATS_MAX_MODULE_ID)) {
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = dp_proc_mng_get_fd(devid);
    if (ret != 0) {
        return ret;
    }

    for (module_id = 0; module_id < MEM_STATS_MAX_MODULE_ID; ++module_id) {
        mem_info[module_id].module_id = module_id;
        mem_info[module_id].total_size = dp_proc_mng_get_module_used_size(devid, module_id);
    }

    ret = dp_proc_mng_update_mbuff_and_process_mem_stats(mem_info, devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return 0;
}

int dp_proc_mng_prof_sample_fun(struct prof_sample_para *para)
{
    uint64_t timestamp = (g_timestamp_mode == 0) ? get_timestamp_ns() : get_cpu_cycle_count();
    struct module_mem_info *mem_info = NULL;
    uint32_t mem_module_id, dev_id;
    int ret;

    ret = dp_proc_mng_prof_para_check(para);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    dev_id = para->dev_id;
    mem_info = (struct module_mem_info *)(para->buff);
    memset_s(mem_info, sizeof(struct module_mem_info) * MEM_STATS_MAX_MODULE_ID, 0,
             sizeof(struct module_mem_info) * MEM_STATS_MAX_MODULE_ID);
    for (mem_module_id = 0; mem_module_id < MEM_STATS_MAX_MODULE_ID; ++mem_module_id) {
        mem_info[mem_module_id].module_id = mem_module_id;
        mem_info[mem_module_id].timestamp = timestamp;
        mem_info[mem_module_id].total_size = dp_proc_mng_get_module_used_size(dev_id, mem_module_id);
    }

#ifndef DRV_HOST
    ret = dp_proc_mng_update_mbuff_and_process_mem_stats(mem_info, dev_id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
#endif

    para->report_len = sizeof(struct module_mem_info) * MEM_STATS_MAX_MODULE_ID;

    return DRV_ERROR_NONE;
}

int dp_proc_mng_prof_start_fun(struct prof_sample_start_para *para)
{
#ifndef EMU_ST
    struct msprof_config *config = NULL;

    if (para == NULL) {
        DP_PROC_MNG_INFO("Para check completed. (para_is_null=%d)\n", (para == NULL));
        return DRV_ERROR_NONE;
    }

    (void)dp_proc_mng_get_fd(para->dev_id);

    if ((para->user_data == NULL) || (para->user_data_len < sizeof(struct msprof_config))) {
        DP_PROC_MNG_INFO("Para check completed, but value unexpected. (user_data_len=%u; expect_len=%u)\n",
                         para->user_data_len, (u32)sizeof(struct msprof_config));
        return 0;
    }
    config = (struct msprof_config *)para->user_data;
    g_timestamp_mode = config->timestamp_mode;
#endif
    return 0;
}

int dp_proc_mng_prof_stop_fun(struct prof_sample_stop_para *para)
{
    dp_proc_mng_prof_stop(para);
    return 0;
}

__attribute__((constructor)) static void dp_proc_mng_prof_init(void)
{
    struct prof_sample_register_para prof_ops;
    uint32_t channel_id = CHANNEL_NPU_MODULE_MEM;
    uint32_t dev_num, ids[MEM_STATS_DEVICE_CNT], i;
    drvError_t ret;

    ret = dp_proc_mng_get_dev_info(&dev_num, ids);
    if (ret != DRV_ERROR_NONE) {
        return;
    }

    prof_ops.sub_chan_num = 1;
    prof_ops.ops.start_func = dp_proc_mng_prof_start_fun;
    prof_ops.ops.stop_func = dp_proc_mng_prof_stop_fun;
    prof_ops.ops.sample_func = dp_proc_mng_prof_sample_fun;
    prof_ops.ops.flush_func = NULL;
#ifndef EMU_ST
    for (i = 0; (i < dev_num) && (i < MEM_STATS_DEVICE_CNT); ++i) {
        ret = halProfSampleRegister(ids[i], channel_id, &prof_ops);
        if (ret != DRV_ERROR_NONE) {
            DP_PROC_MNG_ERR("halProfSampleRegister error. (ret=%d)\n", ret);
            return;
        }
    }
#endif
}
