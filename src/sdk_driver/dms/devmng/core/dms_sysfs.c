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

#include "ka_common_pub.h"
#include "ka_driver_pub.h"
#include "ka_fs_pub.h"
#include "ka_base_pub.h"
#include "ka_compiler_pub.h"

#include "pbl/pbl_davinci_api.h"
#include "pbl_mem_alloc_interface.h"
#include "fms_kernel_interface.h"
#include "urd_acc_ctrl.h"
#include "urd_feature.h"
#include "dms_sensor.h"
#include "dms_event_dfx.h"
#if defined(CFG_BUILD_DEBUG) && defined(CFG_FEATURE_IMU_CORE)
#include "dms_imu_heartbeat.h"
#endif
#include "dms_sysfs.h"

#define DMS_ATTR_RD (00400 | 00040)
#define DMS_ATTR_WR (00200 | 00020)
#define DMS_ATTR_RW (DMS_ATTR_RD | DMS_ATTR_WR)
#define MAX_CAT_BUFSIZE 2010
#define PRINTT_LINE 30
#define DSMI_PROC_NAME "dsmi_log"
#define DSMI_PROC_ATTR 0640

STATIC struct dsmi_log_ctrl g_log = {0};
STATIC ka_proc_dir_entry_t *g_dsmi_proc = NULL;

STATIC ssize_t dms_sysfs_feature_list_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    if (buf == NULL) {
        dms_err("buf is NULL.\n");
        return 0;
    }

    return dms_feature_print_feature_list(buf);
}

STATIC ssize_t dms_sysfs_node_list_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    if (buf == NULL) {
        dms_err("buf is NULL.\n");
        return 0;
    }

    return dms_devnode_print_node_list(buf);
}

STATIC ssize_t dms_sysfs_sensor_list_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    return dms_sensor_print_sensor_list(buf);
}

STATIC ssize_t dms_sysfs_sensor_mask_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    return 0;
}

STATIC ssize_t dms_sysfs_sensor_mask_store(ka_device_t *dev, ka_device_attribute_t *attr, const char *buf, size_t count)
{
    return 0;
}

STATIC ssize_t dms_sysfs_channel_flux_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    return dms_event_dfx_channel_flux_show(buf);
}

STATIC ssize_t dms_sysfs_channel_flux_store(ka_device_t *dev, ka_device_attribute_t *attr, const char *buf,
                                            size_t count)
{
    return dms_event_dfx_channel_flux_store(buf, count);
}

STATIC ssize_t dms_sysfs_convergent_diagrams_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    return dms_event_dfx_convergent_diagrams_show(buf);
}

STATIC ssize_t dms_sysfs_convergent_diagrams_store(ka_device_t *dev, ka_device_attribute_t *attr, const char *buf,
                                                   size_t count)
{
    return dms_event_dfx_convergent_diagrams_store(buf, count);
}

STATIC ssize_t dms_sysfs_event_list_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    return dms_event_dfx_event_list_show(buf);
}

STATIC ssize_t dms_sysfs_event_list_store(ka_device_t *dev, ka_device_attribute_t *attr, const char *buf, size_t count)
{
    return dms_event_dfx_event_list_store(buf, count);
}

STATIC ssize_t dms_sysfs_mask_list_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    return dms_event_dfx_mask_list_show(buf);
}

STATIC ssize_t dms_sysfs_mask_list_store(ka_device_t *dev, ka_device_attribute_t *attr, const char *buf, size_t count)
{
    return dms_event_dfx_mask_list_store(buf, count);
}

STATIC ssize_t dms_sysfs_subscribe_handle_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    return dms_event_dfx_subscribe_handle_show(buf);
}

STATIC ssize_t dms_sysfs_subscribe_process_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    return dms_event_dfx_subscribe_process_show(buf);
}

#if defined(CFG_BUILD_DEBUG) && !defined(CFG_HOST_ENV) && defined(CFG_FEATURE_IMU_CORE)
ssize_t dms_sysfs_imu_hb_show(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    return dms_imu_hb_show(buf);
}

ssize_t dms_sysfs_imu_hb_store(ka_device_t *dev, ka_device_attribute_t *attr, const char *buf, size_t count)
{
    return dms_imu_hb_store(buf, count);
}
#endif

#define DRV_DMS_FS_ATTR KA_DRIVER_DEVICE_ATTR
#define DRV_FS_ATTR_NAME_POINTER(_name) &dev_attr_##_name.attr

#define DECLARE_DMS_FS_ATTR                                                                                       \
    static DRV_DMS_FS_ATTR(feature_list, DMS_ATTR_RD, dms_sysfs_feature_list_show, NULL);                         \
    static DRV_DMS_FS_ATTR(node_list, DMS_ATTR_RD, dms_sysfs_node_list_show, NULL);                               \
    static DRV_DMS_FS_ATTR(sensor_list, DMS_ATTR_RD, dms_sysfs_sensor_list_show, NULL);                           \
    static DRV_DMS_FS_ATTR(sensor_trace, DMS_ATTR_RW, dms_sysfs_sensor_mask_show, dms_sysfs_sensor_mask_store);   \
    static DRV_DMS_FS_ATTR(channel_flux, DMS_ATTR_RW, dms_sysfs_channel_flux_show, dms_sysfs_channel_flux_store); \
    static DRV_DMS_FS_ATTR(convergent_diagrams, DMS_ATTR_RW, dms_sysfs_convergent_diagrams_show,                  \
                           dms_sysfs_convergent_diagrams_store);                                                  \
    static DRV_DMS_FS_ATTR(event_list, DMS_ATTR_RW, dms_sysfs_event_list_show, dms_sysfs_event_list_store);       \
    static DRV_DMS_FS_ATTR(mask_list, DMS_ATTR_RW, dms_sysfs_mask_list_show, dms_sysfs_mask_list_store);          \
    static DRV_DMS_FS_ATTR(subscribe_handle, DMS_ATTR_RD, dms_sysfs_subscribe_handle_show, NULL);                 \
    static DRV_DMS_FS_ATTR(subscribe_process, DMS_ATTR_RD, dms_sysfs_subscribe_process_show, NULL)

#define DECLARE_DMS_FS_ATTR_LIST(_struct_type, _profix)    \
    static _struct_type *g_dms_##_profix##_attr_list[] = { \
        DRV_FS_ATTR_NAME_POINTER(feature_list),            \
        DRV_FS_ATTR_NAME_POINTER(node_list),               \
        DRV_FS_ATTR_NAME_POINTER(sensor_list),             \
        DRV_FS_ATTR_NAME_POINTER(sensor_trace),            \
        DRV_FS_ATTR_NAME_POINTER(channel_flux),            \
        DRV_FS_ATTR_NAME_POINTER(convergent_diagrams),     \
        DRV_FS_ATTR_NAME_POINTER(event_list),              \
        DRV_FS_ATTR_NAME_POINTER(mask_list),               \
        DRV_FS_ATTR_NAME_POINTER(subscribe_handle),        \
        DRV_FS_ATTR_NAME_POINTER(subscribe_process),       \
        NULL,                                              \
    }

#if (defined CFG_BUILD_DEBUG) && (!defined(CFG_HOST_ENV)) && defined(CFG_FEATURE_IMU_CORE)
#define DECLARE_DEBUG_DMS_FS_ATTR \
    static DRV_DMS_FS_ATTR(imu_hb, DMS_ATTR_RW, dms_sysfs_imu_hb_show, dms_sysfs_imu_hb_store)
#endif

DECLARE_DMS_FS_ATTR;
DECLARE_DMS_FS_ATTR_LIST(ka_attribute_t, sysfs);

#if (defined CFG_BUILD_DEBUG) && (!defined(CFG_HOST_ENV)) && defined(CFG_FEATURE_IMU_CORE)
DECLARE_DEBUG_DMS_FS_ATTR;
#endif

// clang-format off
static const ka_attribute_group_t g_dms_sysfs_group = {
    ka_fs_init_ag_attrs(g_dms_sysfs_attr_list)
    ka_fs_init_ag_name("dms")
};
// clang-format on

static void *dsmi_log_proc_start(ka_seq_file_t *m, loff_t *pos)
{
    if (pos != NULL && *pos <= 1) {
        return pos;
    }

    return NULL;
}

static void *dsmi_log_proc_next(ka_seq_file_t *m, void *v, loff_t *pos)
{
    if (pos != NULL) {
        (*pos)++;
    }

    return dsmi_log_proc_start(m, pos);
}

static void dsmi_log_proc_stop(ka_seq_file_t *m, void *v)
{}

static int dsmi_log_proc_show(ka_seq_file_t *m, void *v)
{
    u32 w_pos;

    // only support admin docker and physical read
    if (dms_feature_access_identify(DMS_ACC_ALL | DMS_ADMIN_DOCKER_ONLY | DMS_PHYSICAL_ONLY, 0) != 0) {
        return 0;
    }

    if (m == NULL || v == NULL) {
        return 0;
    }

    ka_task_mutex_lock(&g_log.lock);
    if (*(loff_t *)v == 1) {
        // First output log，then output dfx
        ka_fs_seq_printf(m, "[DFX] size=%u cnt=%llu bytes=%llu ov=%llu used=%u free=%u pos=%u ret=%d.\n",
                         g_log.dfx.buffer_size, g_log.dfx.write_cnt, g_log.dfx.write_bytes, g_log.dfx.overwrite_cnt,
                         g_log.dfx.buf_used, g_log.dfx.buf_free, g_log.dfx.w_pos, g_log.dfx.last_write_ret);
        ka_task_mutex_unlock(&g_log.lock);
        return 0;
    }

    // output log(latest at the end)
    w_pos = g_log.dfx.w_pos;
    if (g_log.dfx.buf_used < DSMI_LOG_BUF_SIZE && g_log.dfx.buf_used != 0) {
        ka_fs_seq_write(m, g_log.buf, w_pos);
    } else if (g_log.dfx.buf_used >= DSMI_LOG_BUF_SIZE) {
        ka_fs_seq_write(m, g_log.buf + w_pos, DSMI_LOG_BUF_SIZE - w_pos);
        ka_fs_seq_write(m, g_log.buf, w_pos);
    }

    ka_task_mutex_unlock(&g_log.lock);
    return 0;
}

static const ka_seq_operations_t dsmi_log_seq_ops = {ka_seq_init_start(dsmi_log_proc_start) ka_seq_init_next(
    dsmi_log_proc_next) ka_seq_init_stop(dsmi_log_proc_stop) ka_seq_init_show(dsmi_log_proc_show)};

static int dsmi_log_proc_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_seq_open(file, &dsmi_log_seq_ops);
}

static void dsmi_log_update_dfx(size_t len, int ret)
{
    g_log.dfx.write_cnt += (len == 0) ? 0 : 1;
    g_log.dfx.write_bytes += len;
    g_log.dfx.last_write_ret = ret;

    if (g_log.dfx.buf_used + len <= DSMI_LOG_BUF_SIZE) {
        g_log.dfx.buf_used += len;
    } else {
        g_log.dfx.buf_used = DSMI_LOG_BUF_SIZE;
    }

    g_log.dfx.buf_free = DSMI_LOG_BUF_SIZE - g_log.dfx.buf_used;
}

static ssize_t dsmi_log_proc_write(ka_file_t *file, const char __ka_user *buf, size_t count, loff_t *ppos)
{
    size_t remain = count;
    size_t copy_len;
    const char __ka_user *ubuf = buf;
    int ret = 0;

    // only support admin docker write
    if (dms_feature_access_identify(DMS_ACC_MANAGE_USER | DMS_ADMIN_DOCKER_ONLY, 0) != 0) {
        ka_task_mutex_lock(&g_log.lock);
        dsmi_log_update_dfx(0, -DRV_ERROR_OPER_NOT_PERMITTED);
        ka_task_mutex_unlock(&g_log.lock);
        return -EPERM;
    }

    if (count == 0 || buf == NULL) {
        return 0;
    }

    ka_task_mutex_lock(&g_log.lock);
    while (remain > 0) {
        copy_len = ka_base_min(remain, (size_t)(DSMI_LOG_BUF_SIZE - g_log.dfx.w_pos));
        if (ka_base_copy_from_user(g_log.buf + g_log.dfx.w_pos, ubuf, copy_len)) {
            dms_err("Log copy_from_user fail.\n");
            ret = -EFAULT;
            break;
        }

        ubuf += copy_len;
        remain -= copy_len;
        g_log.dfx.w_pos += copy_len;

        if (g_log.dfx.w_pos >= DSMI_LOG_BUF_SIZE) {
            g_log.dfx.w_pos = 0;
            g_log.dfx.overwrite_cnt++;
        }
    }

    dsmi_log_update_dfx(ret == 0 ? count : 0, ret);
    ka_task_mutex_unlock(&g_log.lock);

    return ret;
}

// clang-format off
static const ka_procfs_ops_t dsmi_log_proc_fops = {
    ka_fs_init_pf_open(dsmi_log_proc_open)
    ka_fs_init_pf_read(ka_fs_seq_read)
    ka_fs_init_pf_write(dsmi_log_proc_write)
    ka_fs_init_pf_lseek(ka_fs_seq_lseek)
    ka_fs_init_pf_release(ka_fs_seq_release)
};
// clang-format on

static void dsmi_log_init(void)
{
    ka_task_mutex_init(&g_log.lock);
    g_log.dfx.buffer_size = DSMI_LOG_BUF_SIZE;
    g_log.dfx.buf_free = DSMI_LOG_BUF_SIZE;
    g_log.dfx.last_write_ret = 0;
    g_log.dfx.buf_used = 0;
    g_dsmi_proc = ka_fs_proc_create(DSMI_PROC_NAME, DSMI_PROC_ATTR, NULL, &dsmi_log_proc_fops);
    if (g_dsmi_proc == NULL) {
        dms_err("Proc_create fail, g_dsmi_proc is NULL.\n");
        return;
    }
}

static void dsmi_log_uninit(void)
{
    ka_fs_proc_remove(g_dsmi_proc);
    ka_task_mutex_destroy(&g_log.lock);
}

void dms_sysfs_init(void)
{
    int ret;
    ka_device_t *dev = NULL;

    dsmi_log_init();
    dev = davinci_intf_get_owner_device();
    if (dev == NULL) {
        dms_err("dms dev is NULL, sysfs init failed.\n");
        return;
    }

    ret = ka_sysfs_create_group(&dev->kobj, &g_dms_sysfs_group);
    if (ret != 0) {
        dms_err("dms sysfs create group failed.\n");
        return;
    }
#if (defined CFG_BUILD_DEBUG) && (!defined(CFG_HOST_ENV)) && defined(CFG_FEATURE_IMU_CORE)
    ret = sysfs_add_file_to_group(&dev->kobj, &dev_attr_imu_hb.attr, g_dms_sysfs_group.name);
    if (ret != 0) {
        dms_warn("dms sysfs add imu hb node warn, (ret=%d)\n", ret);
    }
#endif
    dms_info("dms sysfs init.\n");
    return;
}

void dms_sysfs_uninit(void)
{
    ka_device_t *dev = NULL;

    dsmi_log_uninit();
    dev = davinci_intf_get_owner_device();
    if (dev == NULL) {
        return;
    }
    ka_sysfs_remove_group(&dev->kobj, &g_dms_sysfs_group);
}
