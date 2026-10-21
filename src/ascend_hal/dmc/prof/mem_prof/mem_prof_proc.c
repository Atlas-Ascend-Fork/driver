/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <time.h>
#include <securec.h>
#include "ascend_hal.h"
#include "dpa/dpa_apm.h"

#if defined CFG_FEATURE_SYSLOG
#include <syslog.h>
#define DRV_EVENT_LOG_ERR(fmt, ...) syslog(LOG_ERR, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define DRV_EVENT_LOG_WARN(fmt, ...) syslog(LOG_WARNING, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define DRV_EVENT_LOG_DBG(fmt, ...) syslog(LOG_DEBUG, "[%s %d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#ifndef EMU_ST
#include "dmc/dmc_log_user.h"
#include "ascend_inpackage_hal.h"
#else
#include "ascend_inpackage_hal.h"
#include "ut_log.h"
#endif

#define DRV_EVENT_LOG_ERR(format, ...)                               \
    do {                                                             \
        DRV_ERR(HAL_MODULE_TYPE_COMMON, format "\n", ##__VA_ARGS__); \
    } while (0)

#define DRV_EVENT_LOG_WARN(format, ...)                               \
    do {                                                              \
        DRV_WARN(HAL_MODULE_TYPE_COMMON, format "\n", ##__VA_ARGS__); \
    } while (0)

#define DRV_EVENT_LOG_DBG(format, ...)                                 \
    do {                                                               \
        DRV_DEBUG(HAL_MODULE_TYPE_COMMON, format "\n", ##__VA_ARGS__); \
    } while (0)
#endif

#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000U
#endif
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC 1000U
#endif

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000ULL
#endif

#define MEM_PROF_MAX_DEV_NUM 65
#define MEM_PROF_SAMPLE_PROC_MODE 0

/* This struct comes from the profile tool;
 * directory: collector/dvvp/driver/inc/ai_drv_prof_api.h;
 * definition: struct TagMemProfileConfig;
 */
struct msprof_config {
    uint32_t period;
    uint32_t timestamp_mode; /* 0 monotonic : others cpu_cycle */
    uint32_t event;
    uint32_t res;
};

struct module_mem_info {
    uint32_t module_id;
    uint32_t memory_type;
    uint64_t timestamp;
    uint64_t total_size;
};

static uint64_t g_start_timestamp[MEM_PROF_MAX_DEV_NUM] = {0};
int (*g_get_module_stats_func)(uint32_t devid, uint32_t module_id, uint64_t *alloced_size);
static uint32_t g_timestamp_mode = 1; /* Default is cpu_cycle_count */

#if defined(__x86_64__)
static uint64_t mem_prof_get_rdtsc(void)
{
    uint64_t cycles;
    uint32_t hi = 0;
    uint32_t lo = 0;
    const int uint32_bits = 32; // 32 is uint bit count

    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    cycles = ((uint64_t)lo) | ((uint64_t)hi << uint32_bits);

    return cycles;
}
#endif

static uint64_t mem_prof_get_cpu_cycle_count(void)
{
    uint64_t cycles;

#if defined(__aarch64__)
    asm volatile("mrs %0, cntvct_el0" : "=r"(cycles));
#elif defined(__x86_64__)
    cycles = mem_prof_get_rdtsc();
#else
    cycles = 0;
#endif

    return cycles;
}

static uint64_t mem_prof_get_timestamp_ns(void)
{
    struct timespec ts = {0};

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

static uint64_t mem_prof_get_timestamp_us(void)
{
    struct timespec ts = {0};

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * USEC_PER_SEC + ts.tv_nsec / NSEC_PER_USEC;
}

static int mem_prof_npu_app_mem_start_func(struct prof_sample_start_para *para)
{
    if (para == NULL) {
        DRV_EVENT_LOG_ERR("Para invalid. (para=%u)\n", (para == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->dev_id >= MEM_PROF_MAX_DEV_NUM) {
        DRV_EVENT_LOG_ERR("Devid is out of range. (devid=%u, max=%u)\n", para->dev_id, MEM_PROF_MAX_DEV_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }

    g_start_timestamp[para->dev_id] = mem_prof_get_timestamp_us();
    return 0;
}

static int mem_prof_npu_app_mem_sample_para_check(struct prof_sample_para *para)
{
    if ((para == NULL) || (para->buff == NULL)) {
        DRV_EVENT_LOG_ERR("Para invalid. (para=%u)\n", (para == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->buff_len < sizeof(struct mem_prof_sample_data)) {
        DRV_EVENT_LOG_ERR("Buff len invalid. (len=%u, min_len=%u)\n", para->buff_len,
                          (unsigned int)sizeof(struct mem_prof_sample_data));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->dev_id >= MEM_PROF_MAX_DEV_NUM) {
        DRV_EVENT_LOG_ERR("Devid is out of range. (devid=%u, max=%u)\n", para->dev_id, MEM_PROF_MAX_DEV_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }
    return 0;
}

static int mem_prof_npu_app_mem_sample_func(struct prof_sample_para *para)
{
    struct mem_prof_sample_data *data = NULL;
    unsigned long long used_size;
    int ret;

    ret = mem_prof_npu_app_mem_sample_para_check(para);
    if (ret != 0) {
        return ret;
    }

    ret = halQuerySlaveProcMeminfo(para->target_pid, para->dev_id, PROCESS_CP1, PROC_MEM_TYPE_ALL, &used_size);
    if (ret != 0) {
        return (ret == DRV_ERROR_NO_PROCESS) ? DRV_ERROR_CALL_NO_RETRY : ret;
    }

    data = (struct mem_prof_sample_data *)para->buff;
    data->timestamp = (uint32_t)(mem_prof_get_timestamp_us() - g_start_timestamp[para->dev_id]);
    data->event = MEM_PROF_SAMPLE_PROC_MODE;
    data->ddr_used_size = 0;
    data->hbm_used_size = used_size;
    data->rsv = 0;
    para->report_len = sizeof(struct mem_prof_sample_data);
    return 0;
}

static drvError_t mem_prof_npu_module_mem_para_check(struct prof_sample_para *para)
{
    if (para == NULL) {
        DRV_EVENT_LOG_ERR("Prof sample para is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->dev_id >= MEM_PROF_MAX_DEV_NUM) {
        DRV_EVENT_LOG_ERR("Invalid prof sample para devid. (devid=%u)\n", para->dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->buff == NULL) {
        DRV_EVENT_LOG_ERR("Prof sample para buff is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para->buff_len < sizeof(struct module_mem_info) * MAX_MODULE_ID) {
        DRV_EVENT_LOG_ERR("Invalid prof sample para buff_len. (buff_len=%u, report_len=%u)\n", para->buff_len,
                          sizeof(struct module_mem_info) * MAX_MODULE_ID);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_get_module_stats_func == NULL) {
        DRV_EVENT_LOG_ERR("Get_module_stats_func is NULL. (devid=%u)\n", para->dev_id);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

int mem_prof_npu_module_mem_sample_fun(struct prof_sample_para *para)
{
    uint64_t timestamp = (g_timestamp_mode == 0) ? mem_prof_get_timestamp_ns() : mem_prof_get_cpu_cycle_count();
    struct module_mem_info *mem_info = NULL;
    uint32_t mem_module_id;
    int ret;

    ret = mem_prof_npu_module_mem_para_check(para);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    mem_info = (struct module_mem_info *)(para->buff);
    memset_s(mem_info, sizeof(struct module_mem_info) * MAX_MODULE_ID, 0,
             sizeof(struct module_mem_info) * MAX_MODULE_ID);
    for (mem_module_id = 0; mem_module_id < MAX_MODULE_ID; ++mem_module_id) {
        mem_info[mem_module_id].module_id = mem_module_id;
        mem_info[mem_module_id].timestamp = timestamp;
        (void)g_get_module_stats_func((uint32_t)para->dev_id, (uint32_t)mem_module_id,
                                      (uint64_t *)&mem_info[mem_module_id].total_size);
    }

    para->report_len = sizeof(struct module_mem_info) * MAX_MODULE_ID;

    return DRV_ERROR_NONE;
}

int mem_prof_npu_module_mem_start_fun(struct prof_sample_start_para *para)
{
    struct msprof_config *config = NULL;

    if (para == NULL) {
        DRV_EVENT_LOG_WARN("Prof sample start para is NULL. \n");
        return 0;
    }

    if ((para->user_data == NULL) || (para->user_data_len < sizeof(struct msprof_config))) {
        DRV_EVENT_LOG_WARN("Prof start para invalid value. (para->user_data_len=%u)\n", para->user_data_len);
        return 0;
    }
    config = (struct msprof_config *)para->user_data;
    g_timestamp_mode = config->timestamp_mode;
    return 0;
}

static void mem_prof_register_channel(uint32_t ids[], uint32_t dev_num, uint32_t chan_id,
                                      int (*start_func)(struct prof_sample_start_para *para),
                                      int (*sample_func)(struct prof_sample_para *para))
{
    struct prof_sample_register_para prof_ops;
    drvError_t ret;
    uint32_t i;

    prof_ops.sub_chan_num = 1;
    prof_ops.ops.start_func = start_func;
    prof_ops.ops.sample_func = sample_func;
    prof_ops.ops.flush_func = NULL;
    prof_ops.ops.stop_func = NULL;

    for (i = 0; (i < dev_num) && (i < MEM_PROF_MAX_DEV_NUM); i++) {
        ret = halProfSampleRegister(ids[i], chan_id, &prof_ops);
        if (ret != DRV_ERROR_NONE) {
            DRV_EVENT_LOG_ERR("halProfSampleRegister error. (ret=%d; devid=%u)\n", ret, ids[i]);
            return;
        }
    }
}

static drvError_t mem_prof_get_dev_info(uint32_t *dev_num, uint32_t *ids)
{
    int ret;

    ret = drvGetDevNum(dev_num);
    if (ret != DRV_ERROR_NONE) {
        DRV_EVENT_LOG_ERR("Drv get dev num error. (ret=%d)\n", ret);
        return ret;
    }

    ret = drvGetDevIDs(ids, MEM_PROF_MAX_DEV_NUM);
    if (ret != DRV_ERROR_NONE) {
        DRV_EVENT_LOG_ERR("Drv get dev ids error. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static void __attribute__((constructor)) mem_prof_sample_register(void)
{
    uint32_t dev_num, ids[MEM_PROF_MAX_DEV_NUM];
    drvError_t ret;

    ret = mem_prof_get_dev_info(&dev_num, ids);
    if (ret != DRV_ERROR_NONE) {
        return;
    }

    mem_prof_register_channel(ids, dev_num, CHANNEL_NPU_APP_MEM, mem_prof_npu_app_mem_start_func,
                              mem_prof_npu_app_mem_sample_func);
    mem_prof_register_channel(ids, dev_num, CHANNEL_NPU_MODULE_MEM, mem_prof_npu_module_mem_start_fun,
                              mem_prof_npu_module_mem_sample_fun);
}

void mem_prof_register_get_module_stats_func(int (*func)(uint32_t devid, uint32_t module_id, uint64_t *alloced_size))
{
    g_get_module_stats_func = func;
}
