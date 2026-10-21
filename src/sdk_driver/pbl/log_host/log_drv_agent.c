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

#ifndef LOG_UT
#include "securec.h"
#include "ka_system_pub.h"
#include "ka_compiler_pub.h"
#include "ka_base_pub.h"
#include "ka_fs_pub.h"
#include "log_drv_agent.h"
#include "ka_kernel_def_pub.h"

#define LOG_S_TO_US 1000000
STATIC log_ring_buf_t g_log_ring_buf = {0};
static KA_BASE_DEFINE_RATELIMIT_STATE(drv_log_err_ratelimit, 1 * KA_HZ, 5);

STATIC log_init_buf_t g_init_log_buf = {0};
STATIC log_phase_t g_log_phase = LOG_PHASE_INIT;
STATIC ka_proc_dir_entry_t *g_hostlog_proc_dir = NULL;
STATIC ka_proc_dir_entry_t *g_hostlog_proc_init_log = NULL;
STATIC ka_proc_dir_entry_t *g_hostlog_proc_log = NULL;

STATIC bool log_switch_to_normal(void)
{
    if (KA_READ_ONCE(g_log_phase) != LOG_PHASE_INIT) {
        return false;
    }
    KA_WRITE_ONCE(g_log_phase, LOG_PHASE_NORMAL);
    return true;
}

STATIC int log_get_date(char *date, u32 len)
{
    ka_timespec64_t sys_time = {0};
    ka_rtc_time_t tm = {0};
    int ret;

    ka_system_ktime_get_real_ts64(&sys_time);
    ka_system_rtc_time_convert(&tm, sys_time);
    ret = snprintf_s(date, len, len - 1, "%04ld-%02d-%02d-%02d:%02d:%02d.%03llu.%03llu", tm.tm_year + TWENTY_CENTURY,
                     tm.tm_mon + JANUARY, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                     (unsigned long long)(sys_time.tv_nsec / KA_NSEC_PER_MSEC),
                     (unsigned long long)((sys_time.tv_nsec / KA_NSEC_PER_USEC) % KA_USEC_PER_MSEC));
    if (ret < 0) {
        slog_drv_err("Log snprintf_s failed. (ret=%d)\n", ret);
        return ret;
    }
    return 0;
}

STATIC int log_write_ringbuffer(void)
{
    u32 buf_left, str_len;
    int ret;

    buf_left = LOG_RINGBUF_SIZE - g_log_ring_buf.point;
    str_len = ka_base_strnlen(g_log_ring_buf.printk_buf, LOG_PRINT_LEN);
    if ((str_len == 0) || (str_len >= LOG_PRINT_LEN)) {
        return -EINVAL;
    }

    if (str_len > buf_left) {
        ret = memcpy_s(g_log_ring_buf.log_buf + g_log_ring_buf.point, buf_left, g_log_ring_buf.printk_buf, buf_left);
        if (ret != 0) {
            return ret;
        }
        ret = memcpy_s(g_log_ring_buf.log_buf, LOG_RINGBUF_SIZE, (char *)g_log_ring_buf.printk_buf + buf_left,
                       str_len - buf_left);
        if (ret != 0) {
            return ret;
        }
    } else {
        ret = memcpy_s(g_log_ring_buf.log_buf + g_log_ring_buf.point, buf_left, g_log_ring_buf.printk_buf, str_len);
        if (ret != 0) {
            return ret;
        }
    }
    g_log_ring_buf.point = (g_log_ring_buf.point + str_len) % LOG_RINGBUF_SIZE;
    g_log_ring_buf.size = ((g_log_ring_buf.size + str_len) >= LOG_RINGBUF_SIZE) ? LOG_RINGBUF_SIZE :
                                                                                  (g_log_ring_buf.size + str_len);

    return 0;
}

STATIC int log_ringbuffer_snapshot(char *dst, u32 dst_len, u32 *out_len)
{
    unsigned long flags;
    u32 pos;
    int ret;

    ka_task_spin_lock_irqsave(&g_log_ring_buf.logbuf_lock, flags);
    pos = g_log_ring_buf.point;

    if ((g_log_ring_buf.size > LOG_RINGBUF_SIZE) || (pos >= LOG_RINGBUF_SIZE)) {
        ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
        slog_drv_err("Invalid ring buf state. (size=%u, pos=%u)\n", g_log_ring_buf.size, pos);
        return -EINVAL;
    }

    if (g_log_ring_buf.size < LOG_RINGBUF_SIZE) {
        ret = memcpy_s(dst, dst_len, g_log_ring_buf.log_buf, pos);
    } else {
        ret = memcpy_s(dst, dst_len, g_log_ring_buf.log_buf + pos, LOG_RINGBUF_SIZE - pos);
        if (ret == 0) {
            ret = memcpy_s(dst + (LOG_RINGBUF_SIZE - pos), pos, g_log_ring_buf.log_buf, pos);
        }
    }

    if (ret != 0) {
        ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
        slog_drv_err("Copy ring buf failed. (ret=%d)\n", ret);
        return ret;
    }

    *out_len = g_log_ring_buf.size;
    ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);

    return 0;
}

STATIC int log_format_buf(const char *fmt, ka_va_list args)
{
    char date[DATATIME_MAXLEN] = {0};
    static char new_fmt[LOG_PRINT_LEN];
    ka_ktime_t kt = ka_system_ktime_get();
    unsigned long long usec = ka_system_ktime_to_us(kt);
    int ret;

    ret = log_get_date(date, DATATIME_MAXLEN);
    if (ret != 0) {
        return ret;
    }

    ret = vsnprintf_truncated_s(new_fmt, sizeof(new_fmt), fmt + LOG_LEVEL_OFFSET, args);
    if (ret <= 0) {
        slog_drv_err("Log format is incorrect. (ret=%d)\n", ret);
        return -EINVAL;
    }

    ret = snprintf_truncated_s(g_log_ring_buf.printk_buf, sizeof(g_log_ring_buf.printk_buf), "[%s] [%llu.%06u] %s",
                               date, (usec / LOG_S_TO_US), (usec % LOG_S_TO_US), new_fmt);
    if (ret < 0) {
        slog_drv_err("Log snprintf_truncated_s failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    if (ret >= LOG_PRINT_LEN - 1) {
        g_log_ring_buf.printk_buf[LOG_PRINT_LEN - 2] = '\n';
        g_log_ring_buf.printk_buf[LOG_PRINT_LEN - 1] = '\0';
    }

    return 0;
}

STATIC bool log_try_save_to_init_buf(void)
{
    u32 str_len = ka_base_strnlen(g_log_ring_buf.printk_buf, LOG_PRINT_LEN);
    int ret;

    if ((str_len == 0) || (str_len >= LOG_PRINT_LEN) || (g_init_log_buf.point + str_len > LOG_INIT_BUF_SIZE)) {
        return false;
    }

    ret = memcpy_s(g_init_log_buf.buf + g_init_log_buf.point, LOG_INIT_BUF_SIZE - g_init_log_buf.point,
                   g_log_ring_buf.printk_buf, str_len);
    if (ret != 0) {
        return false;
    }

    g_init_log_buf.point += str_len;
    g_init_log_buf.size = g_init_log_buf.point;
    return true;
}

static int log_save_to_ringbuf(const char *fmt, ka_va_list args)
{
    unsigned long flags;
    int ret;

    if ((g_log_ring_buf.log_buf == NULL) && (g_init_log_buf.buf == NULL)) {
        return -EINVAL;
    }

    ka_task_spin_lock_irqsave(&g_log_ring_buf.logbuf_lock, flags);

    ret = log_format_buf(fmt, args);
    if (ret != 0) {
        ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
        return ret;
    }

    if ((g_log_phase == LOG_PHASE_INIT) && (g_init_log_buf.buf != NULL)) {
        if (log_try_save_to_init_buf()) {
            ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
            return 0;
        }
        (void)log_switch_to_normal();
    }

    ret = log_write_ringbuffer();
    ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
    if (ret != 0) {
        slog_drv_err("Log write ringbuffer failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

void log_save_to_ring_buf(const char *fmt, ...)
{
    ka_va_list args;
    int ret;

    ka_va_start(args, fmt);
    ret = log_save_to_ringbuf(fmt, args);
    ka_va_end(args);
    if (ret != 0) {
        slog_drv_err("Log save to ring_buf failed. (ret=%d)\n", ret);
        return;
    }

    return;
}
KA_EXPORT_SYMBOL_GPL(log_save_to_ring_buf);

void log_to_printk_and_ringbuf(const char *fmt, ...)
{
    ka_va_list args;
    int ret;

    if (__ka_base_ratelimit(&drv_log_err_ratelimit)) {
        ka_va_start(args, fmt);
        (void)ka_dfx_vprintk(fmt, args);
        ka_va_end(args);
    }

    ka_va_start(args, fmt);
    ret = log_save_to_ringbuf(fmt, args);
    ka_va_end(args);
    if (ret != 0) {
        slog_drv_err("Log save to ring_buf failed. (ret=%d)\n", ret);
        return;
    }

    return;
}
KA_EXPORT_SYMBOL_GPL(log_to_printk_and_ringbuf);

int log_get_ringbuffer(char *buff, u32 buf_len, u32 *out_len)
{
    char *tmp_buf;
    u32 actual_len;
    int ret;

    if ((buff == NULL) || (buf_len != LOG_RINGBUF_SIZE) || (out_len == NULL)) {
        slog_drv_err("Invalid paras. (buff=%d, buf_len=%u, out_len=%d)\n", (int)(buff == NULL), buf_len,
                     (int)(out_len == NULL));
        return -EINVAL;
    }

    tmp_buf = log_drv_vzalloc(LOG_RINGBUF_SIZE);
    if (tmp_buf == NULL) {
        slog_drv_err("Vzalloc tmp_buf failed.\n");
        return -ENOMEM;
    }

    ret = log_ringbuffer_snapshot(tmp_buf, LOG_RINGBUF_SIZE, &actual_len);
    if (ret != 0) {
        log_drv_vfree(tmp_buf);
        return ret;
    }

    ret = ka_base_copy_to_user((void *)(uintptr_t)buff, tmp_buf, actual_len);
    if (ret != 0) {
        log_drv_vfree(tmp_buf);
        slog_drv_err("Log copy to user failed. (ret=%d)\n", ret);
        return ret;
    }

    *out_len = actual_len;
    log_drv_vfree(tmp_buf);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(log_get_ringbuffer);

STATIC int log_ringbuffer_init(void)
{
    g_log_ring_buf.log_buf = log_drv_vzalloc(LOG_RINGBUF_SIZE);
    if (g_log_ring_buf.log_buf == NULL) {
        slog_drv_err("Vzalloc g_log_ring_buf failed.\n");
        return -ENOMEM;
    }

    g_log_ring_buf.point = 0;
    g_log_ring_buf.size = 0;
    ka_task_spin_lock_init(&g_log_ring_buf.logbuf_lock);

    return 0;
}

STATIC void log_ringbuffer_uninit(void)
{
    if (g_log_ring_buf.log_buf != NULL) {
        log_drv_vfree(g_log_ring_buf.log_buf);
        g_log_ring_buf.log_buf = NULL;
    }
}

STATIC int log_init_buf_init(void)
{
    g_init_log_buf.buf = (char *)log_drv_vzalloc(LOG_INIT_BUF_SIZE);
    if (g_init_log_buf.buf == NULL) {
        slog_drv_err("Vzalloc init log buf failed.\n");
        return -ENOMEM;
    }

    g_init_log_buf.point = 0;
    g_init_log_buf.size = 0;
    return 0;
}

STATIC void log_init_buf_uninit(void)
{
    if (g_init_log_buf.buf != NULL) {
        log_drv_vfree(g_init_log_buf.buf);
        g_init_log_buf.buf = NULL;
        g_init_log_buf.point = 0;
        g_init_log_buf.size = 0;
    }
}

STATIC int log_proc_init_log_open(ka_inode_t *inode, ka_file_t *file)
{
    log_proc_data_t *proc_data;
    unsigned long flags;

    proc_data = (log_proc_data_t *)log_drv_vzalloc(sizeof(log_proc_data_t));
    if (proc_data == NULL) {
        slog_drv_err("Vzalloc proc_data failed.\n");
        return -ENOMEM;
    }

    proc_data->buf = (char *)log_drv_vzalloc(LOG_INIT_BUF_SIZE);
    if (proc_data->buf == NULL) {
        slog_drv_err("Vzalloc init log read buf failed.\n");
        log_drv_vfree(proc_data);
        return -ENOMEM;
    }

    ka_task_spin_lock_irqsave(&g_log_ring_buf.logbuf_lock, flags);
    if (g_init_log_buf.size > 0) {
        if (memcpy_s(proc_data->buf, LOG_INIT_BUF_SIZE, g_init_log_buf.buf, g_init_log_buf.size) != 0) {
            ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);
            slog_drv_err("Copy init log failed.\n");
            log_drv_vfree(proc_data->buf);
            log_drv_vfree(proc_data);
            return -EFAULT;
        }
    }
    proc_data->len = g_init_log_buf.size;
    ka_task_spin_unlock_irqrestore(&g_log_ring_buf.logbuf_lock, flags);

    ka_fs_set_file_private_data(file, proc_data);
    return 0;
}

STATIC int log_proc_log_open(ka_inode_t *inode, ka_file_t *file)
{
    log_proc_data_t *proc_data;
    u32 actual_len;
    int ret;

    (void)inode;

    proc_data = (log_proc_data_t *)log_drv_vzalloc(sizeof(log_proc_data_t));
    if (proc_data == NULL) {
        slog_drv_err("Vzalloc proc_data failed.\n");
        return -ENOMEM;
    }

    proc_data->buf = (char *)log_drv_vzalloc(LOG_RINGBUF_SIZE);
    if (proc_data->buf == NULL) {
        slog_drv_err("Vzalloc log read buf failed.\n");
        log_drv_vfree(proc_data);
        return -ENOMEM;
    }

    ret = log_ringbuffer_snapshot(proc_data->buf, LOG_RINGBUF_SIZE, &actual_len);
    if (ret != 0) {
        log_drv_vfree(proc_data->buf);
        log_drv_vfree(proc_data);
        return ret;
    }

    proc_data->len = actual_len;
    ka_fs_set_file_private_data(file, proc_data);
    return 0;
}

STATIC ssize_t log_proc_data_read(ka_file_t *file, char __ka_user *buf, size_t count, loff_t *ppos)
{
    log_proc_data_t *proc_data = (log_proc_data_t *)ka_fs_get_file_private_data(file);

    if (proc_data == NULL) {
        return -EINVAL;
    }

    return ka_fs_simple_read_from_buffer(buf, count, ppos, proc_data->buf, proc_data->len);
}

STATIC int log_proc_data_release(ka_inode_t *inode, ka_file_t *file)
{
    log_proc_data_t *proc_data = (log_proc_data_t *)ka_fs_get_file_private_data(file);

    if (proc_data != NULL) {
        if (proc_data->buf != NULL) {
            log_drv_vfree(proc_data->buf);
        }
        log_drv_vfree(proc_data);
        ka_fs_set_file_private_data(file, NULL);
    }

    return 0;
}

STATIC const ka_procfs_ops_t g_log_proc_init_log_ops = {ka_fs_init_pf_owner(KA_THIS_MODULE) ka_fs_init_pf_open(
    log_proc_init_log_open) ka_fs_init_pf_read(log_proc_data_read) ka_fs_init_pf_lseek(generic_file_llseek)
                                                            ka_fs_init_pf_release(log_proc_data_release)};

STATIC const ka_procfs_ops_t g_log_proc_log_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) ka_fs_init_pf_open(log_proc_log_open) ka_fs_init_pf_read(log_proc_data_read)
        ka_fs_init_pf_lseek(generic_file_llseek) ka_fs_init_pf_release(log_proc_data_release)};

STATIC int log_proc_fs_init(void)
{
    g_hostlog_proc_dir = ka_fs_proc_mkdir(HOSTLOG_PROC_DIR, NULL);
    if (g_hostlog_proc_dir == NULL) {
        slog_drv_err("Create proc dir failed.\n");
        return -EFAULT;
    }

    g_hostlog_proc_init_log = ka_fs_proc_create(HOSTLOG_PROC_INIT_LOG, KA_S_IRUSR, g_hostlog_proc_dir,
                                                &g_log_proc_init_log_ops);
    if (g_hostlog_proc_init_log == NULL) {
        slog_drv_err("Create proc init_log failed.\n");
        goto proc_fail;
    }

    g_hostlog_proc_log = ka_fs_proc_create(HOSTLOG_PROC_LOG, KA_S_IRUSR, g_hostlog_proc_dir, &g_log_proc_log_ops);
    if (g_hostlog_proc_log == NULL) {
        slog_drv_err("Create proc log failed.\n");
        goto proc_fail;
    }

    return 0;

proc_fail:
    ka_fs_remove_proc_subtree(HOSTLOG_PROC_DIR, NULL);
    g_hostlog_proc_dir = NULL;
    g_hostlog_proc_init_log = NULL;
    g_hostlog_proc_log = NULL;
    return -EFAULT;
}

STATIC void log_proc_fs_uninit(void)
{
    if (g_hostlog_proc_dir != NULL) {
        ka_fs_remove_proc_subtree(HOSTLOG_PROC_DIR, NULL);
        g_hostlog_proc_dir = NULL;
        g_hostlog_proc_init_log = NULL;
        g_hostlog_proc_log = NULL;
    }
}

int log_drv_module_init(void)
{
    int ret;

    ret = log_ringbuffer_init();
    if (ret != 0) {
        slog_drv_err("Log ringbuffer init failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = log_init_buf_init();
    if (ret != 0) {
        slog_drv_err("Log init buf init failed. (ret=%d)\n", ret);
        log_ringbuffer_uninit();
        return ret;
    }

    ret = log_proc_fs_init();
    if (ret != 0) {
        slog_drv_err("Log proc fs init failed. (ret=%d)\n", ret);
        log_init_buf_uninit();
        log_ringbuffer_uninit();
        return ret;
    }

    slog_drv_info("Host logdrv module init successfully.\n");
    return 0;
}

void log_drv_module_exit(void)
{
    log_proc_fs_uninit();
    log_init_buf_uninit();
    log_ringbuffer_uninit();

    return;
}
#else
int drv_log_ut(void)
{
    return 0;
}
#endif