/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef LOG_DRV_AGENT_H
#define LOG_DRV_AGENT_H

#include "log_drv_agent_alloc_interface.h"
#include "dmc_kernel_interface.h"

#define LOG_LEVEL_OFFSET 2U
#define DATATIME_MAXLEN 50U
#define TWENTY_CENTURY 1900
#define JANUARY 1U
#define KILO 1000U
#define LOG_RINGBUF_SIZE 0x1400000 // 20M
#define LOG_INIT_BUF_SIZE 0x18000U // 96K
#define LOG_PRINT_LEN 0x400        // 1024 Bytes
#define MODULE_HOST_LOG "ascend_logdrv"

#define HOSTLOG_PROC_DIR "asdrv_hostlog"
#define HOSTLOG_PROC_INIT_LOG "init_log"
#define HOSTLOG_PROC_LOG "log"

#define slog_err_printk(level, module, fmt, ...) \
    (void)printk(level "[ascend] [ERROR] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)
#define slog_printk(level, module, fmt, ...) \
    (void)ka_dfx_printk(level "[ascend] [%s] [%s %d] " fmt, module, __func__, __LINE__, ##__VA_ARGS__)

#define slog_err(module, fmt...) slog_err_printk(KA_KERN_ERR, module, fmt)
#define slog_info(module, fmt...) slog_printk(KA_KERN_INFO, module, fmt)

#ifndef LOG_UNIT_TEST
#define slog_drv_err(fmt, ...)                                                                             \
    slog_err(MODULE_HOST_LOG, "<%s:%d,%d> " fmt, ka_task_get_current()->comm, ka_task_get_current()->tgid, \
             ka_task_get_current()->pid, ##__VA_ARGS__)
#define slog_drv_info(fmt, ...)                                                                             \
    slog_info(MODULE_HOST_LOG, "<%s:%d,%d> " fmt, ka_task_get_current()->comm, ka_task_get_current()->tgid, \
              ka_task_get_current()->pid, ##__VA_ARGS__)
#else
#define slog_drv_err(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define slog_drv_info(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#ifdef LOG_UNIT_TEST
#define STATIC
#else
#define STATIC static
#endif

typedef struct {
    char *log_buf;
    u32 point;
    u32 size;
    ka_task_spinlock_t logbuf_lock;
    char printk_buf[LOG_PRINT_LEN]; /* temporary log_buf */
} log_ring_buf_t;

typedef enum {
    LOG_PHASE_INIT = 0,
    LOG_PHASE_NORMAL = 1,
} log_phase_t;

typedef struct {
    char *buf;
    u32 point;
    u32 size;
} log_init_buf_t;

typedef struct {
    char *buf;
    u32 len;
} log_proc_data_t;

int log_get_ringbuffer(char *buff, u32 buf_len, u32 *out_len);
int log_drv_module_init(void);
void log_drv_module_exit(void);
#endif