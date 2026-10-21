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

#ifndef __DEVDRV_MANAGER_COMM_H
#define __DEVDRV_MANAGER_COMM_H
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_fs_pub.h"
#include "ascend_hal_define.h"

#ifndef ka_pid_t
typedef int ka_pid_t;
#endif

#include "fms/fms_dtm.h"
#include "dms_devdrv_info.h"

#if ((defined CFG_BUILD_DEBUG) && (!defined EXPORT_SYMBOL_UNRELEASE))
#define EXPORT_SYMBOL_UNRELEASE(symbol) KA_EXPORT_SYMBOL(symbol)
#elif (!defined EXPORT_SYMBOL_UNRELEASE)
#define EXPORT_SYMBOL_UNRELEASE(symbol)
#endif

#define MAX_DOCKER_NUM 128U /* equal to max device num */
#define MAX_AICPU_CORE_NUM 32U

struct host_pid_info {
    ka_pid_t host_pid;
    unsigned int dev_id;
    unsigned int vfid;
    enum devdrv_process_type cp_type;
};

struct dev_pid_info {
    unsigned int dev_id;
    unsigned int vfid;
    enum devdrv_process_type cp_type;
};

enum devdrv_ts_access_mem_type {
    DEVDRV_TS_NODE_DDR_MEM = 0,
    DEVDRV_DDR_MEM,
    DEVDRV_HBM_MEM,
    DEVDRV_P2P_HBM_MEM,
    DEVDRV_MEM_TYPE_MAX
};

typedef enum {
    POWER_RESUME_MODE_BUTTON,        /* resume by button */
    POWER_RESUME_MODE_TIME,          /* resume by time */
    POWER_RESUME_MODE_TIME_POWEROFF, /* poweroff by time */
    POWER_RESUME_MODE_TEST_TIME,     /* resume for selftest by time*/
    POWER_RESUME_MODE_MAX,
} DSMI_LP_RESUME_MODE;

typedef enum {
    POWER_RESET_MODE_NORMAL,      /* normal reset mode */
    POWER_RESET_MODE_TO_LOWPOWER, /* reset to lowpower */
    POWER_RESET_MODE_TO_SUSPEND,  /* reset to suspend */
    POWER_RESET_MODE_MAX,
} DSMI_LP_RESET_MODE;

struct drvdev_power_state_info_stru {
    DSMI_POWER_STATE type;
    union {
        DSMI_LP_RESUME_MODE mode;      /* resume mode */
        DSMI_LP_RESET_MODE reset_mode; /* reset mode */
    };
    unsigned int value;
    unsigned int reserve[POWER_INFO_RESERVE_LEN];
};

struct drvdev_power_state_info {
    unsigned int dev_id;
    struct drvdev_power_state_info_stru power_info;
};

typedef enum {
    POWER_STATE_MODULE_SILS,
    POWER_STATE_MODULE_LPM,
    POWER_STATE_MODULE_MAX
} POWER_STATE_MODULE_TYPE;
struct devdrv_power_handle {
    POWER_STATE_MODULE_TYPE module;
    int (*func)(struct drvdev_power_state_info *info);
};

typedef enum {
    FREQSCALING = 0,
    POWERGATINGEN,
    CLOCKGATINGEN,
    RESETEN,
    DEVICE_STATUS,
    MAXDEVOPSIDNUMS = 0xff,
} DEV_POWER_OP_TYPE;

typedef union {
    unsigned int level;
    int flag;
} DEV_POWER_OP_VAL;

typedef struct {
    DEV_POWER_OP_TYPE op_type;
    DEV_POWER_OP_VAL op_val;
} DEVPOWER_OP;

typedef enum {
    DEVMM_SET_VDEV_CONVERT_LEN = 0,
    DEVMM_GET_VDEV_CONVERT_LEN,
    DMS_HUGEPAGES_SURP_STATISTIC,
    DMS_DEV_INFO_TYPE_MAX
} DMS_DEV_INFO_TYPE;

#define CHIP_TYPE_ASCEND_V1 1
#define CHIP_TYPE_ASCEND_V2 2
#define CHIP_TYPE_ASCEND_V3 3
#define CHIP_TYPE_ASCEND_V51_LITE (0x19513U)
#define CENTRE_NOFITY_CHIP_INDEX 8

#define DEVDRV_HOST_MASTER 0
#define DEVDRV_DEVICE_MASTER 1

typedef int (*dms_set_dev_info_ops)(u32 devid, const void *buf, u32 buf_size);
typedef int (*dms_get_dev_info_ops)(u32 devid, void *buf, u32 *buf_size);
typedef int (*devmm_get_device_accounting_pids_ops)(u32, u32, u32, int *, u32);
typedef int (*devmm_get_device_process_memory_ops)(u32, u32, int, u64 *);

int devdrv_creat_ipc_name(char *ipc_name, unsigned int len);
extern struct devdrv_info *devdrv_manager_get_devdrv_info(u32 dev_id);
int devdrv_try_get_dev_info_occupy(struct devdrv_info *dev_info);
void devdrv_put_dev_info_occupy(struct devdrv_info *dev_info);
int devdrv_inquire_aicore_task(unsigned int dev_id, unsigned int fid, unsigned int tgid, unsigned int *result);
int tsdrv_mirror_ctx_status_set(ka_pid_t pid, u32 dev_id, u32 status);
int devdrv_manager_container_table_devlist_add_ns(u32 *physical_devlist, u32 physical_dev_num,
                                                  ka_mnt_namespace_t *mnt_ns);
int devdrv_manager_container_check_devid_in_container_ns(u32 devid, ka_task_struct_t *tsk);
int devdrv_manager_container_check_devid_in_container(u32 devid, ka_pid_t hostpid);
int dev_mnt_vdevice_logical_id_to_phy_id(u32 logical_id, u32 *phy_id, u32 *vfid);
int dev_mnt_vdevice_phy_id_to_logical_id(u32 phy_id, u32 vfid, u32 *logical_id);
int dev_mnt_vdev_register_client(u32 phy_id, u32 vfid, const ka_file_operations_t *ops);
int dev_mnt_vdev_unregister_client(u32 phy_id, u32 vfid);
int devdrv_get_devnum(u32 *num_dev);
int devdrv_get_vdevnum(u32 *num_dev);
int devdrv_manager_devid_to_nid(u32 devid, u32 mem_type);
int devdrv_manager_get_docker_id(u32 *docker_id);
int devdrv_manager_get_process_pids_register(devmm_get_device_accounting_pids_ops func);
void devdrv_manager_get_process_pids_unregister(void);
int devdrv_manager_get_process_memory_register(devmm_get_device_process_memory_ops func);
void devdrv_manager_get_process_memory_unregister(void);
int dms_register_set_dev_info_handler(DMS_DEV_INFO_TYPE type, dms_set_dev_info_ops func);
int dms_unregister_set_dev_info_handler(DMS_DEV_INFO_TYPE type);
int dms_register_get_svm_dev_info_handler(DMS_DEV_INFO_TYPE type, dms_get_dev_info_ops func);
int dms_unregister_get_svm_dev_info_handler(DMS_DEV_INFO_TYPE type);
int devdrv_manager_get_chip_type(int *chip_type);
bool devdrv_manager_ts_is_enable(void);
bool devdrv_manager_is_pf_device(unsigned int dev_id);
int devdrv_get_chip_die_id(u32 dev_id, u32 *chip_id, u32 *die_id);
int devdrv_manager_get_bootstrap(unsigned int *bootstrap);
int devdrv_get_devids(u32 *devices, u32 device_num);
int devdrv_manager_register_power_handle(unsigned int module, int (*func)(struct drvdev_power_state_info *power_info));
int devdrv_manager_unregister_power_handle(unsigned int module);

#endif /* __DEVDRV_MANAGER_COMM_H */
