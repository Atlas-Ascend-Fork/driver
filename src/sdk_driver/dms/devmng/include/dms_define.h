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

#ifndef __DMS_DEFINE_H__
#define __DMS_DEFINE_H__

#include "ka_memory_pub.h"
#include "ka_dfx_pub.h"
#include "ka_task_pub.h"
#include "ascend_hal_error.h"
#include "dms_sensor.h"
#include "dms_event.h"
#include "fms_kernel_interface.h"
#include "dmc_kernel_interface.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "ascend_dev_num.h"
#include "ka_system_pub.h"

#include "fms_kernel_interface.h"

#ifndef ASCEND_DEV_MAX_NUM
#define ASCEND_DEV_MAX_NUM 64
#endif

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#ifndef __KA_GFP_ACCOUNT
#ifdef __GFP_KMEMCG
#define __KA_GFP_ACCOUNT __GFP_KMEMCG /* for linux version 3.10 */
#endif

#ifdef __GFP_NOACCOUNT
#define __KA_GFP_ACCOUNT 0 /* for linux version 4.1 */
#endif
#endif

#if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_SRIOV)
#define DEVICE_NUM_MAX 1124
#else
#define DEVICE_NUM_MAX 64
#endif
/* The maximum number of information that the sub-device can query is: infoType 0 ~ dms_MAX_INFO_TYPE_NUM */
#define DMS_MAX_INFO_TYPE_NUM 20

/* Interconnected device node item */
struct dms_dev_link_item {
    struct dms_node dev1;
    struct dms_node dev2;
    /* The connection status of the two devices */
    int link_state;
    /* Type of connection between devices 0: HCCS, 1: PCIE, 2, USB, 3, I2C */
    int link_type;
};

struct dms_link_node {
    /* Device code majorDevID (high 2 bytes) + minorDevID (low 2 bytes) */
    int node_id;
    char major_dev_name[DMS_MAX_DEV_NAME_LEN];
    /* Device topology table, unique to interconnect nodes */
    struct dms_dev_link_item dev_topo_table;
};

/* Interconnect device node registration interface */
struct dms_link_node_operations {
    /* Scan the status of each device and sensor */
    int (*scan)(struct dms_node *device, int *state);
    /* Run test to diagnose whether the object is monitored */
    int (*fault_diag)(struct dms_node *device, int *state);
    /* Alarm notification sub-devices such as RAS */
    int (*event_notify)(struct dms_node *device, int event);
    /* Query the status of two device nodes */
    int (*get_link_state)(struct dms_node *device1, struct dms_node *device2, unsigned int *state);
    /* Query the status of two device nodes */
    int (*set_link_state)(struct dms_node *device1, struct dms_node *device2, unsigned int state);
};

/* The data view of the device, data snapshot, used to store the data of the sub-device of the device on host side */
struct dms_dev_data_view {
    /* Device handle */
    struct dms_node *device;
    /* The number of data in the view in the sub-device */
    int dev_data_num;
    struct {
        struct dms_dev_data_attr dataItem;
        void *data_value;
        unsigned int data_size;
    } *dev_data_items;
};

#define MODULE_DMS "dms_module"
#ifdef UT_VCAST
#define dms_err(fmt, ...) drv_err(MODULE_DMS, fmt, ##__VA_ARGS__)
#define dms_warn(fmt, ...) drv_warn(MODULE_DMS, fmt, ##__VA_ARGS__)
#define dms_info(fmt, ...) drv_info(MODULE_DMS, fmt, ##__VA_ARGS__)
#define dms_event(fmt, ...) drv_event(MODULE_DMS, fmt, ##__VA_ARGS__)
#define dms_debug(fmt, ...) drv_pr_debug(MODULE_DMS, fmt, ##__VA_ARGS__)
#define dms_err_ratelimited(fmt, ...) drv_err_ratelimited(MODULE_DMS, fmt, ##__VA_ARGS__)
#else
#define dms_err(fmt, ...)                                                                              \
    do {                                                                                               \
        drv_err(MODULE_DMS, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                ka_task_get_current_pid(), ##__VA_ARGS__);                                             \
        share_log_err(DEVMNG_SHARE_LOG_START, fmt, ##__VA_ARGS__);                                     \
    } while (0)
#define dms_warn(fmt, ...)                                                                          \
    drv_warn(MODULE_DMS, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
             ka_task_get_current_pid(), ##__VA_ARGS__)
#define dms_info(fmt, ...)                                                                          \
    drv_info(MODULE_DMS, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
             ka_task_get_current_pid(), ##__VA_ARGS__)
#define dms_event(fmt, ...)                                                                          \
    drv_event(MODULE_DMS, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
              ka_task_get_current_pid(), ##__VA_ARGS__)
#define dms_debug(fmt, ...)                                                                             \
    drv_pr_debug(MODULE_DMS, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                 ka_task_get_current_pid(), ##__VA_ARGS__)
#define dms_err_ratelimited(fmt, ...)                                                                              \
    do {                                                                                                           \
        drv_err_ratelimited(MODULE_DMS, "<%s:%d:%d> " fmt, ka_task_get_current_comm(), ka_task_get_current_tgid(), \
                            ka_task_get_current_pid(), ##__VA_ARGS__);                                             \
    } while (0)
#endif
#ifdef CFG_FEATURE_HOST_LOG
#define ONE_TIME_EVENT 2
#define dms_fault_mng_event(assertion, fmt, ...)            \
    do {                                                    \
        if (((assertion) == ONE_TIME_EVENT)) {              \
            drv_event("fault_manager", fmt, ##__VA_ARGS__); \
        } else {                                            \
            drv_err("fault_manager", fmt, ##__VA_ARGS__);   \
        }                                                   \
    } while (0)
#else
#define dms_fault_mng_event(assertion, fmt, ...)                                                  \
    do {                                                                                          \
        u64 ts = ka_system_local_clock();                                                         \
        u64 rem_nsec = do_div(ts, 1000000000);                                                    \
        drv_slog_event("fault_manager", "[%5lu.%06lu] " fmt, ts, rem_nsec / 1000, ##__VA_ARGS__); \
    } while (0)
#endif
#define dms_fmng_event(args...) (void)ka_dfx_printk(KA_KERN_NOTICE "[fault_manager] " args)
#define dms_fmng_err(args...) (void)ka_dfx_printk(KA_KERN_ERR "[fault_manager] " args)

#define dms_ex_notsupport_err(ret, fmt, ...)                                   \
    do {                                                                       \
        if (((ret) != (int)DRV_ERROR_NOT_SUPPORT) && ((ret) != -EOPNOTSUPP)) { \
            dms_err(fmt, ##__VA_ARGS__);                                       \
        }                                                                      \
    } while (0)

typedef enum dsmi_dev_init_state {
    DMS_DEV_INIT_NO_ERROR = 0x0,
    DMS_DEV_INIT_ABNORMAL = 0x1,
    DMS_DEV_INIT_NOT_REGISTER = 0xf,
} DMS_INIT_STATE_T;

unsigned int dms_fill_event_data(const struct dms_sensor_object_cb *psensor_obj_cb, DMS_EVENT_LIST_ITEM *event_item,
                                 unsigned char assertion, struct dms_event_obj *p_event_obj);

#endif
