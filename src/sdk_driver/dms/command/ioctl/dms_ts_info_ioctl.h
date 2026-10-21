/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef DMS_TS_IOCTL_STATUS_H
#define DMS_TS_IOCTL_STATUS_H

#if defined(CFG_SOC_PLATFORM_CLOUD)
#define DEVDRV_MANGER_MAX_DEVICE_NUM 64
#elif defined(CFG_SOC_PLATFORM_MINIV2)
#if defined(CFG_FEATURE_UC_CHIP_MAX_ON)
#define DEVDRV_MANGER_MAX_DEVICE_NUM 1
#else
#define DEVDRV_MANGER_MAX_DEVICE_NUM 2
#endif
#elif defined(CFG_SOC_PLATFORM_MINI)
#define DEVDRV_MANGER_MAX_DEVICE_NUM 64
#endif

typedef enum {
    DSMI_TS_SUB_AICORE_UTILIZATION_RATE = 0,
    DSMI_TS_SUB_VECTORCORE_UTILIZATION_RATE = 1,
    DSMI_TS_SUB_FFTS_TYPE, // Obtains the type of FFTS or FFTS+
    DSMI_TS_SUB_SET_FAULT_MASK,
    DSMI_TS_SUB_GET_FAULT_MASK,
    DSMI_TS_SUB_LAUNCH_AICORE_STL,
    DSMI_TS_SUB_AICORE_STL_STATUS,
    DSMI_TS_SUB_AIC_UTILIZATION_RATE_ASYN,
    DSMI_TS_SUB_AIV_UTILIZATION_RATE_ASYN,
    DSMI_TS_SUB_ACC_UTILIZATION_RATE,
    DSMI_TS_SUB_NPU_UTILIZATION_RATE,
    DSMI_TS_SUB_COMMON_MSG, // consistent to DSMI_TS_SUB_CMD_COMMON_MSG
    DSMI_TS_SUB_NPU_MULTI_UTILIZATION_RATE,
    DSMI_TS_SUB_STREAM_AVAILABLE,
    DSMI_TS_SUB_CMD_MAX_VALUE
} DSMI_SUB_TS_INFO;

typedef struct _core_utilization_rate {
    unsigned int dev_id;
    unsigned int cmd_type;
    unsigned char *core_utilization_rate;
    unsigned int core_num;
    unsigned int vfid;
    unsigned int resv[2];
} core_utilization_rate_t;

struct dms_ts_info_in {
    unsigned int dev_id;
    unsigned int vfid;
    unsigned int core_id;
};

struct dms_ts_hb_status_in {
    unsigned int dev_id;
    unsigned int vf_id;
    unsigned int ts_id;
};

#endif