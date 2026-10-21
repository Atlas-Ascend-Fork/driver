/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pthread.h>

#include "securec.h"
#include "ascend_hal.h"
#include "dms_user_interface.h"
#include "davinci_interface.h"
#include "dp_proc_mng_ioctl.h"
#include "dp_proc_mng.h"
#include "dp_adapt.h"
#include "dpa/dpa_dp_proc_mng.h"

static THREAD int g_dp_proc_mng_fd = -1;
THREAD volatile uint64_t g_svm_module_used_size[MEM_STATS_DEVICE_CNT][MEM_STATS_MAX_MODULE_ID];
void dp_proc_mng_module_used_size_update(uint32_t devid, uint32_t module_id, uint64_t size)
{
    if ((devid >= MEM_STATS_DEVICE_CNT) || (module_id >= MEM_STATS_MAX_MODULE_ID)) {
        return;
    }

    g_svm_module_used_size[devid][module_id] = size;
}

int dp_proc_mng_get_fd(uint32_t dev_id)
{
    static bool get_flag = false;
#ifndef CFG_FEATURE_SUPPORT_SRIOV
    uint32_t host_mode = DEVDRV_HOST_PHY_MACH_FLAG;
    int ret;
#endif

    if (get_flag == false) {
#ifndef CFG_FEATURE_SUPPORT_SRIOV
#ifndef EMU_ST
        ret = devdrv_get_host_phy_mach_flag(dev_id, &host_mode);
        if (ret != 0) {
            DP_PROC_MNG_INFO("Devdrv_get_host_phy_mach_flag completed. (ret=%d)\n", ret);
            return ret;
        }
#endif
        if (host_mode != DEVDRV_HOST_VM_MACH_FLAG) {
            g_dp_proc_mng_fd = dp_proc_mng_get_fd_inner();
        }
#else
        g_dp_proc_mng_fd = dp_proc_mng_get_fd_inner();
#endif
    }
    get_flag = true;
    (void)dev_id;

    return 0;
}

void dp_proc_mng_prof_stop(struct prof_sample_stop_para *para)
{
    (void)para;
    return;
}

bool dp_proc_mng_get_prof_start_sample_flag(void)
{
    return true;
}

#ifdef DRV_HOST
drvError_t dp_proc_mng_update_mbuff_and_process_mem_stats(struct module_mem_info *mem_info, uint32_t devid)
{
    struct dp_proc_mng_ioctl_arg arg = {0};
    int ret;

    arg.head.logical_devid = devid;

    if (g_dp_proc_mng_fd < 0) {
        return DRV_ERROR_NONE;
    }
    ret = dp_proc_mng_ioctl(DP_PROC_MNG_GET_MEM_STATS, &arg);
    if (ret != DRV_ERROR_NONE) {
        DP_PROC_MNG_ERR("Ioctl get mem stats failed. (ret=%d)\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    mem_info[MBUFF_MODULE_ID].total_size = arg.data.get_mem_stats_para.mbuff_used_size;
    mem_info[AICPU_SCHE_MODULE_ID].total_size = arg.data.get_mem_stats_para.aicpu_used_size;
    mem_info[CUSTOM_SCHE_MODULE_ID].total_size = arg.data.get_mem_stats_para.custom_used_size;
    mem_info[HCCP_SCHE_MODULE_ID].total_size = arg.data.get_mem_stats_para.hccp_used_size;

    return DRV_ERROR_NONE;
}
#else
#ifndef EMU_ST /* sscanf_s in ut is sscanf */
static int dp_proc_mng_get_size_from_string(const char *p_str, const char *form_str, int *pid, uint64_t *size,
                                            uint64_t *aicpu_size)
{
    if (pid != NULL) {
        return sscanf_s(p_str, form_str, pid, size, aicpu_size);
    } else {
        return sscanf_s(p_str, form_str, size);
    }
}
#endif

#define DP_PROC_MNG_SHAREPOOL_INFO_MEMBER_NUM 3
#define BYTES_PER_KB 1024

static drvError_t dp_proc_mng_get_sp_and_aicpu_memsize_from_string(const char *file_string, const char *mem_string,
                                                                   uint64_t *sp_size, uint64_t *aicpu_size)
{
    char *p_str = NULL, *ptr = NULL, *temp_ptr = NULL;
    uint64_t tmp_sp_size, tmp_aicpu_size;
    int pid;

    p_str = strstr(file_string, mem_string);
    if (p_str == NULL) {
        DP_PROC_MNG_ERR("Can not find string. (mem_string=%s)\n", mem_string);
        return DRV_ERROR_INVALID_VALUE;
    }

    ptr = strtok_s(p_str, "\n", &temp_ptr);
    while (ptr != NULL) {
        ptr = strtok_s(NULL, "\n", &temp_ptr);
        if ((ptr != NULL) &&
            (dp_proc_mng_get_size_from_string(ptr, "%d %*s %llu %*llu %*llu %llu", &pid, &tmp_sp_size,
                                              &tmp_aicpu_size) == DP_PROC_MNG_SHAREPOOL_INFO_MEMBER_NUM)) {
            if (pid == getpid()) {
                *sp_size = tmp_sp_size * BYTES_PER_KB;
                *aicpu_size = tmp_aicpu_size * BYTES_PER_KB;
                return DRV_ERROR_NONE;
            }
        }
    }

    return DRV_ERROR_NONE;
}

#define DP_PROC_MNG_PROC_INFO_MEMBER_NUM 1

static drvError_t dp_proc_mng_get_proc_memsize_from_string(const char *file_string, const char *mem_string,
                                                           uint64_t *size)
{
    char *p_str = NULL;
    uint64_t tmp_size;
    drvError_t ret;

    p_str = strstr(file_string, mem_string);
    if (p_str == NULL) {
        DP_PROC_MNG_ERR("Can not find string. (mem_string=%s)\n", mem_string);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = dp_proc_mng_get_size_from_string(p_str, "%*s %llu", NULL, &tmp_size, NULL);
    if (ret != DP_PROC_MNG_PROC_INFO_MEMBER_NUM) {
        DP_PROC_MNG_ERR("Sscanf not right. (mem_string=%s)\n", mem_string);
        return DRV_ERROR_INVALID_VALUE;
    }

    *size = tmp_size * BYTES_PER_KB;

    return DRV_ERROR_NONE;
}

static drvError_t dp_proc_mng_get_memsize_by_flag(char *file_string, size_t count, uint64_t *size, uint64_t *flag)
{
    drvError_t ret;

    (void)count;

    if (flag != NULL) {
        ret = dp_proc_mng_get_sp_and_aicpu_memsize_from_string(file_string, "PID", size, flag);
        if (ret != DRV_ERROR_NONE) {
            DP_PROC_MNG_ERR("Can not find \"PID\". \n");
            return ret;
        }
    } else {
        ret = dp_proc_mng_get_proc_memsize_from_string(file_string, "VmRSS", size);
        if (ret != DRV_ERROR_NONE) {
            DP_PROC_MNG_ERR("Can not find \"VmRSS\". \n");
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

#ifndef EMU_ST
static int dp_proc_mng_open_file(char *file_name, FILE **fp)
{
    *fp = fopen(file_name, "r");
    if (*fp == NULL) {
        return DRV_ERROR_FILE_OPS;
    }

    return DRV_ERROR_NONE;
}

static void dp_proc_mng_close_file(FILE *fp)
{
    (void)fclose(fp);
}

static size_t dp_proc_mng_read_file(char *file_string, size_t fsize, size_t count, FILE *fp)
{
    return fread(file_string, fsize, count, fp);
}
#endif

#define DP_PROC_MNG_PROC_INFO_MAX_LEN 4096

static drvError_t dp_proc_mng_get_mbuff_and_proc_mem_used_size(char *file_name, size_t count, uint64_t *size,
                                                               uint64_t *aicpu_size_flag)
{
    FILE *fp = NULL;
    char *file_string = NULL;
    drvError_t ret;

    (void)count;

    ret = dp_proc_mng_open_file(file_name, &fp);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        /* too much log, not print, if open failed, mbuff or proc mem_used_size will equal to 0 */
        return DRV_ERROR_NONE;
#endif
    }

    file_string = (char *)malloc(DP_PROC_MNG_PROC_INFO_MAX_LEN * sizeof(char));
    if (file_string == NULL) {
        DP_PROC_MNG_ERR("malloc error.\n");
        ret = DRV_ERROR_MALLOC_FAIL;
        goto err_get_malloc;
    }

    if (dp_proc_mng_read_file(file_string, sizeof(char), DP_PROC_MNG_PROC_INFO_MAX_LEN - 1, fp) <= 0) {
#ifndef EMU_ST
        ret = DRV_ERROR_NONE;
        goto err_get_memsize;
#endif
    }
    file_string[DP_PROC_MNG_PROC_INFO_MAX_LEN - 1] = '\0';

    ret = dp_proc_mng_get_memsize_by_flag(file_string, DP_PROC_MNG_PROC_INFO_MAX_LEN, size, aicpu_size_flag);
    if (ret != DRV_ERROR_NONE) {
        DP_PROC_MNG_ERR("Dp_proc_mng_get_memsize_by_flag error. (file_name=%s; ret=%d)\n", file_name, ret);
        goto err_get_memsize;
    }

err_get_memsize:
    free(file_string);
err_get_malloc:
    dp_proc_mng_close_file(fp);
    return ret;
}

#define DP_PROC_MNG_PROC_INFO_PATH_MAX_LEN 128

drvError_t dp_proc_mng_update_mbuff_and_process_mem_stats(struct module_mem_info *mem_info, uint32_t devid)
{
    struct halQueryDevpidInfo info;
    pid_t aicpu_pid, custom_pid;
    char file_name[DP_PROC_MNG_PROC_INFO_PATH_MAX_LEN] = {0};
    drvError_t ret;

    info.hostpid = getpid();
    info.devid = devid;
    info.vfid = 0;
    info.proc_type = DEVDRV_PROCESS_CP1;
    ret = halQueryDevpid(info, &aicpu_pid);
    if (ret != DRV_ERROR_NONE) {
        return DRV_ERROR_NONE;
    }

    if (sprintf_s(file_name, DP_PROC_MNG_PROC_INFO_PATH_MAX_LEN, "%s", "/proc/sharepool/proc_overview") < 0) {
        ret = DRV_ERROR_INVALID_VALUE;
        DP_PROC_MNG_ERR("sprintf_s error. (ret=%d)\n", ret);
        return ret;
    }

    ret = dp_proc_mng_get_mbuff_and_proc_mem_used_size(file_name, DP_PROC_MNG_PROC_INFO_PATH_MAX_LEN,
                                                       &mem_info[MBUFF_MODULE_ID].total_size,
                                                       &mem_info[AICPU_SCHE_MODULE_ID].total_size);
    if (ret != DRV_ERROR_NONE) {
        DP_PROC_MNG_ERR("Get sharepool/aicpu mem stats failed. (ret=%d)\n", ret);
        return ret;
    }

    if (mem_info[AICPU_SCHE_MODULE_ID].total_size == 0) {
        if (sprintf_s(file_name, DP_PROC_MNG_PROC_INFO_PATH_MAX_LEN, "%s%d%s", "/proc/", aicpu_pid, "/status") < 0) {
            ret = DRV_ERROR_INVALID_VALUE;
            DP_PROC_MNG_ERR("sprintf_s error. (ret=%d)\n", ret);
            return ret;
        }
        ret = dp_proc_mng_get_mbuff_and_proc_mem_used_size(file_name, DP_PROC_MNG_PROC_INFO_PATH_MAX_LEN,
                                                           &mem_info[AICPU_SCHE_MODULE_ID].total_size, NULL);
        if (ret != DRV_ERROR_NONE) {
            DP_PROC_MNG_ERR("Get aicpu mem stats failed. (ret=%d)\n", ret);
            return ret;
        }
    }

    info.hostpid = aicpu_pid;
    info.devid = devid;
    info.vfid = 0;
    info.proc_type = DEVDRV_PROCESS_CP2;
    ret = halQueryDevpid(info, &custom_pid);
    if ((ret == DRV_ERROR_NONE) && (custom_pid != aicpu_pid)) {
        if (sprintf_s(file_name, DP_PROC_MNG_PROC_INFO_PATH_MAX_LEN, "%s%d%s", "/proc/", custom_pid, "/status") < 0) {
            ret = DRV_ERROR_INVALID_VALUE;
            DP_PROC_MNG_ERR("sprintf_s error. (ret=%d)\n", ret);
            return ret;
        }
        ret = dp_proc_mng_get_mbuff_and_proc_mem_used_size(file_name, DP_PROC_MNG_PROC_INFO_PATH_MAX_LEN,
                                                           &mem_info[CUSTOM_SCHE_MODULE_ID].total_size, NULL);
        if (ret != DRV_ERROR_NONE) {
            DP_PROC_MNG_ERR("Get custom mem stats failed. (ret=%d)\n", ret);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}
#endif

uint64_t dp_proc_mng_get_module_used_size(uint32_t devid, uint32_t module_id)
{
    if ((devid >= MEM_STATS_DEVICE_CNT) || (module_id >= MEM_STATS_MAX_MODULE_ID)) {
        return 0;
    }

    return g_svm_module_used_size[devid][module_id];
}

drvError_t dp_proc_mng_get_dev_info(uint32_t *dev_num, uint32_t *ids)
{
    int ret;

    ret = drvGetDevNum(dev_num);
    if (ret != DRV_ERROR_NONE) {
        DP_PROC_MNG_ERR("Drv get dev num error. (ret=%d)\n", ret);
        return ret;
    }

    ret = drvGetDevIDs(ids, MEM_STATS_DEVICE_CNT);
    if (ret != DRV_ERROR_NONE) {
        DP_PROC_MNG_ERR("Drv get dev ids error. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

bool dp_proc_support_bind_cgroup(void)
{
    return true;
}