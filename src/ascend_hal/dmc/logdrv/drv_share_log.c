/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "dmc/dmc_share_log.h"
#include "dmc/dmc_log_user.h"
#include "securec.h"

#include <sys/mman.h>
#include <stdbool.h>

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define SHARE_LOG_NO_INIT (0U)
#define SHARE_LOG_INIT (1U)
#define SHARE_LOG_MAGIC_LENGTH (24U)
#define SHARE_LOG_RECORD_OFFSET (100U)
#define SHARE_LOG_MAGIC ("drvshartlogab90cd78ef56")

#ifdef LOG_UT
#define LOG_PRINT_WARN(fmt, ...) printf("[WARN][%s %d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_PRINT_WARN(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_LOG, fmt, ##__VA_ARGS__)
#endif
struct share_log_info {
    char magic[SHARE_LOG_MAGIC_LENGTH];
    int32_t total_size;
    void *record_base;
    int32_t record_size;
    int32_t read;
    int32_t write;
};

struct share_log_module_mng {
    void *start;
    int32_t init_flag;
};

STATIC struct share_log_module_mng g_module_mng[HAL_MODULE_TYPE_MAX][SHARE_LOG_TYPE_MAX] = {
    [HAL_MODULE_TYPE_DEVMM] = {{(void *)(uintptr_t)DEVMM_SHARE_LOG_START, SHARE_LOG_NO_INIT},
                               {(void *)(uintptr_t)DEVMM_SHARE_LOG_RUNINFO_START, SHARE_LOG_NO_INIT}},
    [HAL_MODULE_TYPE_TS_DRIVER] = {{(void *)(uintptr_t)TSDRV_SHARE_LOG_START, SHARE_LOG_NO_INIT},
                                   {(void *)(uintptr_t)TSDRV_SHARE_LOG_RUNINFO_START, SHARE_LOG_NO_INIT}},
    [HAL_MODULE_TYPE_DEV_MANAGER] = {{(void *)(uintptr_t)DEVMNG_SHARE_LOG_START, SHARE_LOG_NO_INIT},
                                     {(void *)(uintptr_t)DEVMNG_SHARE_LOG_RUNINFO_START, SHARE_LOG_NO_INIT}},
    [HAL_MODULE_TYPE_HDC] = {{(void *)(uintptr_t)HDC_SHARE_LOG_START, SHARE_LOG_NO_INIT},
                             {(void *)(uintptr_t)HDC_SHARE_LOG_RUNINFO_START, SHARE_LOG_NO_INIT}},
    [HAL_MODULE_TYPE_EVENT_SCHEDULE] = {{(void *)(uintptr_t)ESCHED_SHARE_LOG_START, SHARE_LOG_NO_INIT},
                                        {(void *)(uintptr_t)ESCHED_SHARE_LOG_RUNINFO_START, SHARE_LOG_NO_INIT}},
    [HAL_MODULE_TYPE_BUF_MANAGER] = {{(void *)(uintptr_t)XSMEM_SHARE_LOG_START, SHARE_LOG_NO_INIT},
                                     {(void *)(uintptr_t)XSMEM_SHARE_LOG_RUNINFO_START, SHARE_LOG_NO_INIT}},
    [HAL_MODULE_TYPE_QUEUE_MANAGER] = {{(void *)(uintptr_t)QUEUE_SHARE_LOG_START, SHARE_LOG_NO_INIT},
                                       {(void *)(uintptr_t)QUEUE_SHARE_LOG_RUNINFO_START, SHARE_LOG_NO_INIT}},
    [HAL_MODULE_TYPE_COMMON] = {{(void *)(uintptr_t)COMMON_SHARE_LOG_START, SHARE_LOG_NO_INIT},
                                {(void *)(uintptr_t)COMMON_SHARE_LOG_RUNINFO_START, SHARE_LOG_NO_INIT}},
    [HAL_MODULE_TYPE_APM] = {{(void *)(uintptr_t)APM_SHARE_LOG_START, SHARE_LOG_NO_INIT},
                             {(void *)(uintptr_t)APM_SHARE_LOG_RUNINFO_START, SHARE_LOG_NO_INIT}},
};

STATIC pthread_mutex_t share_log_mutex[SHARE_LOG_TYPE_MAX] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};

STATIC void share_log_create_single_type(enum devdrv_module_type module_type, enum share_log_type_enum log_type,
                                         uint32_t size)
{
    struct share_log_info *info = NULL;
    int32_t len;

    (void)pthread_mutex_lock(&share_log_mutex[log_type]);
    if ((module_type >= HAL_MODULE_TYPE_MAX) || (size > (uint32_t)SHARE_LOG_MAX_SIZE) ||
        (g_module_mng[module_type][log_type].init_flag == SHARE_LOG_INIT)) {
        (void)pthread_mutex_unlock(&share_log_mutex[log_type]);
        LOG_PRINT_WARN("Invalid input or share_log has been inited. (module_type=%d; log_type=%d; size=%u)\n",
                       module_type, log_type, size);
        return;
    }

    info = (struct share_log_info *)mmap(g_module_mng[module_type][log_type].start, size, PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (info != (struct share_log_info *)g_module_mng[module_type][log_type].start) {
        (void)pthread_mutex_unlock(&share_log_mutex[log_type]);
#ifndef LOG_UT
        LOG_PRINT_WARN("Share_log mmap addr not match. (errno=%d; reason=\"%s\")\n", errno, strerror(errno));
        if (info != MAP_FAILED) {
            (void)munmap(info, size);
        }
#endif
        return;
    }
    (void)memset_s(info, size, 0, size);
    len = snprintf_s(info->magic, SHARE_LOG_MAGIC_LENGTH, SHARE_LOG_MAGIC_LENGTH - 1, "%s", SHARE_LOG_MAGIC);
    if (len < 0) {
        (void)munmap(g_module_mng[module_type][log_type].start, size);
        (void)pthread_mutex_unlock(&share_log_mutex[log_type]);
        LOG_PRINT_WARN("Snprintf_s unsuccessfully. (len=%d)\n", len);
        return;
    }

    info->magic[SHARE_LOG_MAGIC_LENGTH - 1] = '\0';
    info->record_base = (char *)g_module_mng[module_type][log_type].start + SHARE_LOG_RECORD_OFFSET;
    info->total_size = (int32_t)size;
    info->record_size = (int32_t)(size - SHARE_LOG_RECORD_OFFSET);
    info->read = 0;
    info->write = 0;
    g_module_mng[module_type][log_type].init_flag = SHARE_LOG_INIT;
    (void)pthread_mutex_unlock(&share_log_mutex[log_type]);

    return;
}

void share_log_create(enum devdrv_module_type module_type, uint32_t size)
{
    enum share_log_type_enum i = SHARE_LOG_ERR;
    for (i = SHARE_LOG_ERR; i < SHARE_LOG_TYPE_MAX; i++) {
        share_log_create_single_type(module_type, i, size);
    }
}

STATIC bool share_log_magic_check(const char *magic)
{
    if (strcmp(magic, SHARE_LOG_MAGIC) == 0) {
        return true;
    } else {
        return false;
    }
}

STATIC void share_log_destroy_single_type(enum devdrv_module_type module_type, enum share_log_type_enum log_type)
{
    struct share_log_info *info = NULL;

    if (module_type >= HAL_MODULE_TYPE_MAX) {
        return;
    }

    share_log_read(HAL_MODULE_TYPE_COMMON);
    (void)pthread_mutex_lock(&share_log_mutex[log_type]);
    info = (struct share_log_info *)g_module_mng[module_type][log_type].start;
    if ((g_module_mng[module_type][log_type].init_flag == SHARE_LOG_INIT) &&
        (share_log_magic_check(info->magic) == true)) {
        size_t size = (size_t)info->total_size;
        (void)memset_s(info, size, 0, size);
        (void)munmap(g_module_mng[module_type][log_type].start, size);
        g_module_mng[module_type][log_type].init_flag = SHARE_LOG_NO_INIT;
    }
    (void)pthread_mutex_unlock(&share_log_mutex[log_type]);

    return;
}

void share_log_destroy(enum devdrv_module_type type)
{
    enum share_log_type_enum i = SHARE_LOG_ERR;
    for (i = SHARE_LOG_ERR; i < SHARE_LOG_TYPE_MAX; i++) {
        share_log_destroy_single_type(type, i);
    }
}

static void share_log_read_in_single_module(enum share_log_type_enum log_type, enum devdrv_module_type module_type)
{
    struct share_log_info *info = NULL;
    int32_t tmp_read, tmp_write;

    if (module_type >= HAL_MODULE_TYPE_MAX) {
        return;
    }

    (void)pthread_mutex_lock(&share_log_mutex[log_type]);
    info = (struct share_log_info *)g_module_mng[module_type][log_type].start;
    if ((g_module_mng[module_type][log_type].init_flag == SHARE_LOG_NO_INIT) ||
        (share_log_magic_check(info->magic) == false) || (info->write == info->read) ||
        (info->write > info->record_size) || (info->record_size <= 0) ||
        ((unsigned int)(info->record_size) > (SHARE_LOG_MAX_SIZE - SHARE_LOG_RECORD_OFFSET))) {
        (void)pthread_mutex_unlock(&share_log_mutex[log_type]);
        return;
    }

    tmp_read = info->read;
    tmp_write = info->write;

    if (tmp_write > tmp_read) {
#ifndef LOG_UT
        uint32_t i;
        char *read_buff = malloc((unsigned int)info->record_size);
        if (read_buff == NULL) {
            (void)pthread_mutex_unlock(&share_log_mutex[log_type]);
            return;
        }
        size_t out_size = (size_t)(tmp_write - tmp_read);
        int32_t ret = memcpy_s(read_buff, (unsigned int)info->record_size, (char *)info->record_base + tmp_read,
                               out_size);
        if (ret != 0) {
            free(read_buff);
            (void)pthread_mutex_unlock(&share_log_mutex[log_type]);
            return;
        }

        for (i = 0; i < out_size - 1; i++) {
            if (read_buff[i] == '\n') {
                read_buff[i] = ' ';
            }
        }
        read_buff[out_size] = '\0';
        if (log_type == SHARE_LOG_ERR) {
            DRV_ERR(module_type, "%s", read_buff);
        } else {
            DRV_RUN_INFO(module_type, "%s", read_buff);
        }

        free(read_buff);
        info->read = tmp_write;
#endif
    }
    (void)pthread_mutex_unlock(&share_log_mutex[log_type]);

    return;
}

void share_log_read(enum devdrv_module_type module_type)
{
    share_log_read_in_single_module(SHARE_LOG_ERR, module_type);
    share_log_read_in_single_module(SHARE_LOG_ERR, HAL_MODULE_TYPE_COMMON);
}

void share_log_read_err(enum devdrv_module_type module_type)
{
    share_log_read_in_single_module(SHARE_LOG_ERR, module_type);
    share_log_read_in_single_module(SHARE_LOG_ERR, HAL_MODULE_TYPE_COMMON);
}

void share_log_read_run_info(enum devdrv_module_type module_type)
{
    share_log_read_in_single_module(SHARE_LOG_RUN_INFO, module_type);
    share_log_read_in_single_module(SHARE_LOG_RUN_INFO, HAL_MODULE_TYPE_COMMON);
}
