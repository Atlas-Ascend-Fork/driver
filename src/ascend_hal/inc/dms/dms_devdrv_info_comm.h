/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DEVDRV_INFO_COMM_H
#define __DEVDRV_INFO_COMM_H
#include <sys/time.h>
#include <stdint.h>
#include "ascend_hal.h"
#include "dsmi_common_interface.h"
#include "dms_devdrv_info.h"

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef u8
typedef unsigned char u8;
#endif

#define DEVDRV_RC_ID_1 1000
#define DEVDRV_RC_ID_2 2000
#define DEVDRV_RC_ID_3 1004
#define DEVDRV_RC_ID_4 2004

#define DEVDRV_HOST_PHY_MACH_FLAG 0x5a6b7c8d /* host physical mathine flag */
#define DM_DEVPOWER_MSG 0x345543
#define MINI_BOARD_POWER_REG_BASE 0x1100c3000
#define MINI_BOARD_POWER_REG_OFFSET 0x434
#ifndef DEVDRV_IMU_CMD_LEN
#define DEVDRV_IMU_CMD_LEN (32U)
#endif
#define INFO_MAX_DAVINCI_NUM 64

#if defined(DRV_HOST) && defined(CFG_FEATURE_SRIOV)
#ifndef DEVDRV_MAX_DAVINCI_NUM
#define DEVDRV_MAX_DAVINCI_NUM 1124
#endif
#else
#ifndef DEVDRV_MAX_DAVINCI_NUM
#define DEVDRV_MAX_DAVINCI_NUM 64
#endif
#endif

#define MAX_CMD_DES_CHAR_COUNT 1000

#define DEVDRV_HOST_VM_MACH_FLAG 0x1a2b3c4d        /* vm mathine flag */
#define DEVDRV_HOST_CONTAINER_MACH_FLAG 0xa4b3c2d1 /* container mathine flag */

struct inline_reduce {
    unsigned int ADD : 1;
    unsigned int SUB : 1;
    unsigned int MUL : 1;
    unsigned int DIV : 1;
    unsigned int FP4 : 1;
    unsigned int FP8 : 1;
    unsigned int FP16 : 1;
    unsigned int FP32 : 1;
    unsigned int reserve1 : 8;
    unsigned int reserve2 : 8;
    unsigned int reserve3 : 8;
};

struct ioctl_arg {
    unsigned int dev_id;
    unsigned int type;  // core id or device type
    unsigned int data1; // u32 data
    int data2;          // int data  for temperature
    unsigned int data3;
    unsigned int vfid;
};

struct dms_ddr_temp_arg {
    unsigned int dev_id;
    unsigned int vfid;
    unsigned int core_id;
    void *commoninfo;
    unsigned int commoninfo_len;
};

struct all_temp {
    char aicore0_temp;
    char aicore1_temp;
    char aicore2_temp;
    char aicore3_temp;
    char aicore4_temp;
    char aicore5_temp;
    char aicore6_temp;
    char aicore7_temp;
    char aicore8_temp;
    char aicore9_temp;
    char cpu0_temp;
    char cpu1_temp;
    char cpu2_temp;
    char cpu3_temp;
    char dvpp_temp;
    char io_temp;
    char ao_temp;
    char isp_temp;
};

#define DMANAGE_ALL_TEMP_LEN 18
struct ioctl_all_temp_arg {
    unsigned int cnt_check;
    union {
        struct all_temp tmp;
        char tmpp[DMANAGE_ALL_TEMP_LEN];
    } data;
};
#define CNT_CHECK_FAIL 2
#define CNT_CHECK_SUCC 1

#define COMPUTING_POWER_PMU_NUM 4
#define BBOX_ERRSTR_LEN (48)

struct tag_computing_power_msg {
    unsigned long long state;
    unsigned long long timestamp1;
    unsigned long long timestamp2;
    unsigned long long event_count[COMPUTING_POWER_PMU_NUM];
    unsigned int system_frequency;
};

#define I2C_DATA_LEN 16

struct i2c_data {
    unsigned int len;
    unsigned int data[I2C_DATA_LEN];
};

struct hns_i2cdev_info_cmd {
    unsigned int op_type;
    unsigned int dev_id;
    int i2cdev_info;
};

#if defined(CFG_SOC_PLATFORM_CLOUD)
#define QSFP_TEMP_INFO 0
#define I2C_DEVINFO_CMD 2

#define I2C_IOCTRL_TYPE 'Z'
#define DEVDRV_MANAGER_GET_OPTICAL_TEMP _IOWR(I2C_IOCTRL_TYPE, I2C_DEVINFO_CMD, struct i2c_data)
#endif

struct computing_power_arg {
    unsigned int dev_id;
    struct tag_computing_power_msg compute_power_msg;
};

struct bb_err_string {
    unsigned int dev_id;
    unsigned int errcode;
    int buf_len;
    unsigned char errstr[BBOX_ERRSTR_LEN];
};

struct dmanage_flash_info {
    unsigned long flash_id;         /* combined device & manufacturer code */
    unsigned short device_id;       /* device id */
    unsigned short vendor;          /* the primary vendor id */
    unsigned int state;             /* flash health */
    unsigned long size;             /* total size in bytes */
    unsigned int sector_count;      /* number of erase units */
    unsigned short manufacturer_id; /* manufacturer id */
};

#define DEVDRV_TS_CONFLICT_PROFILING 0xEF
enum dmanager_core_id {
    CLUSTER_ID = 0,
    PERI_ID = 1,
    TS_ID = 2,
    DDR_ID = 3,
    AICORE0_ID = 4,
    AICORE1_ID = 5,
    HBM_ID = 6,
    VECTOR_ID = 7,
    INVALID_ID,
};

enum dmanage_freq_id {
    CCPU_FREQ = 0,
    DDR_FREQ = 1,
    AICORE0_FREQ = 2,
    AICORE1_FREQ = 3,
    HBM_FREQ = 4,
    VECTOR_FREQ = 5,
    INVALID_FREQ,
};

enum dmanager_tsensor_id {
    CLUSTER_TEMP_ID = 0,
    PERI_TEMP_ID = 1,
    AICORE0_TEMP_ID,
    AICORE1_TEMP_ID,
    AICORE_LIMIT_ID,      // AICORE core limit status: 0 (no limit), 1 (core limit)
    AICORE_TOTAL_PER_ID,  // AICORE pulse total cycle
    AICORE_ELIM_PER_ID,   // aicore erasable cycle
    AICORE_BASE_FREQ_ID,  // aicore Base Frequency (MHz) returns u16
    NPU_DDR_FREQ_ID,      // DDR frequency unit in MHz returns u16
    THERMAL_THRESHOLD_ID, // thermal comfort frequency limit temp, system reset temp, return u8 * temp temp[0] temp[1]
    NTC_TEMP_ID,          // four thermistor temperatures, return is an array: int * ret ret[0] ret[1] ret[2] ret[3]
    SOC_TEMP_ID,          // SOC's max temp
    FP_TEMP_ID,           // optical module's temp
    N_DIE_TEMP_ID,
    HBM_TEMP_ID,       // hbm's max temp
    HBM_ARRAY_TEMP_ID, // hbm dram temp
    INVALID_TSENSOR_ID,
};
#define DEV_TIME_TO_MS 1000
#define THERMAL_THRESHOLD_NUM 2
#define NTC_NUM 4

#define MACR_CPU_TYPE_CPU0 0x0
#define MACR_CPU_TYPE_CPU1 0x1
#define MACR_CPU_TYPE_CPU2 0x2
#define MACR_CPU_TYPE_CPU3 0x3
#define MACR_CPU_TYPE_CPU4 0x4
#define MACR_CPU_TYPE_CPU5 0x5
#define MACR_CPU_TYPE_CPU6 0x6
#define MACR_CPU_TYPE_CPU7 0x7
#define MACR_CPU_TYPE_CPU_ALL 0xFF
#define MACR_CPU_STAT_BUF_LEN 1024
#define MACR_CPU_NAME_LEN 0x5

#define MACR_CPU_INFO_PATH "/proc/cpuinfo"
#define MACR_CPU_INFO_BUF_LEN 1024
#define MACR_CPU_INFO_WORD "processor"

#define DEVDRV_VNIC (0)
#define DEVDRV_ROCE (1)
#define DEVDRV_BOND (2)
#define DEVDRV_MAX_SYS_DEV_NUM (4)
#define DEVDRV_IPV6_ARRAY_LEN (16)
#define DEVDRV_MAX_MAC_LEN (6)
#define DEVDRV_MAX_ETH_NAME_LEN (16) /* same as IFNAMSIZ in <net/if.h> */

struct dmanager_card_info {
    unsigned int dev_id;
    unsigned char card_type;
    unsigned char card_id;
};

typedef union _dmanager_ipaddr {
    unsigned int addr_v4;
    unsigned char addr_v6[DEVDRV_IPV6_ARRAY_LEN];
} ipaddr_t;

struct dmanager_ip_info {
    unsigned char ip_type;
    ipaddr_t ip_addr;
    ipaddr_t mask_addr;
};

struct dmanager_gtw_info {
    unsigned char ip_type;
    ipaddr_t gtw_addr;
};

struct dmanager_mac_info {
    unsigned char mac_addr[DEVDRV_MAX_MAC_LEN];
};
struct dmanager_net_dev_info {
    unsigned int card_num;
    char card_name[DEVDRV_MAX_ETH_NAME_LEN];
};
struct dmanager_pmu_voltage_stru {
    unsigned int pmu_type;
    unsigned int device_id;
    unsigned int channel;
    unsigned int get_value;
    int return_value;
};

struct dmanager_llc_perf_info {
    unsigned int dev_id;
    unsigned int wr_hit_rate;
    unsigned int rd_hit_rate;
    unsigned int throughput;
};

#define TAISHAN_CORE_NUM 16
struct dmanage_aicpu_info_stru {
    unsigned int maxFreq;
    unsigned int curFreq;
    unsigned int aicpuNum;
    unsigned int utilRate[TAISHAN_CORE_NUM];
};

struct dmanage_pri_auth {
    int dev_id;    /**< Device ID */
    int host_root; /**< status 0:not root 1:root */
    int run_env;   /**< status 0:physical environment 1:not physical environment */
};

#define DEVDRV_SUCCESS 0
#define DEVDRV_ERR_OTHERS (-1)
#define DEVDRV_ERR_NULL (-2)
#define DEVDRV_ERR_XLOADER_IDX (-3)
#define DEV_GET_CURR_BOOT_AREA 0
#define DEV_CLEAR_BOOT_COUNT 1

int dmanage_check_module_init(const char *module_name);
int dmanage_get_pcie_id_info(unsigned int dev_id, struct dmanage_pcie_id_info *pcie_id_info);
int dmanage_get_device_flash_count(unsigned int *pflash_count);
int dmanage_get_device_flash_info(unsigned int flash_index, struct dmanage_flash_info *pflash_info);
int dmanage_get_device_ddr_capacity(unsigned int dev_id, unsigned int *pcapacity);

int dmanage_get_device_health(unsigned int dev_id, unsigned int *phealth);
int dmanage_get_device_errorcode(unsigned int dev_id, int *p_error_code_count, unsigned int *p_error_code,
                                 int p_error_code_len);
int devdrv_lpm3_smoke_ipc(unsigned char *send, unsigned char *ack, unsigned int len);
int dmanage_get_mini_device_id(unsigned int *id);
int devdrv_get_slot_id(unsigned int dev_id, unsigned int *slot_id);
int dmanage_get_emmc_voltage(int *emmc_vcc, int *emmc_vccq);
int dmanage_enable_efuse_ldo2(void);
int dmanage_disable_efuse_ldo2(void);
int devdrv_imu_smoke_ipc(unsigned int dev_id, const unsigned char *send, unsigned int send_len, unsigned char *ack,
                         unsigned int *ack_len);
int dms_imu_dmp_msg_send(int fd, unsigned int dev_id, unsigned char *buf, unsigned char len);
int dms_imu_dmp_msg_recv(int fd, unsigned int dev_id, unsigned char *buf, unsigned char buf_len,
                         unsigned char *recv_len);
int DmsGetImuInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size);
drvError_t dms_get_ub_info(uint32_t devId, int32_t infoType, void *buf, int32_t *size);
drvError_t DmsGetUbInfo(unsigned int dev_id, int module_type, int info_type, void *buf, unsigned int *size);
#ifdef CFG_FEATURE_GET_DEV_INDEX_IN_GROUP
drvError_t DmsGetIndexInGroup(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                              unsigned int *size);
#endif
int dms_get_ub_info_by_dmp(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size);
#ifndef CFG_SOC_PLATFORM_MINI
int dmanage_restart_ssh(const char *ip_addr);
#endif
int devdrv_get_host_phy_mach_flag(unsigned int dev_id, unsigned int *host_flag);
int dmanage_get_ip_address(unsigned int dev_id, struct dmanager_card_info card_info, struct dmanager_ip_info *ack_info);
int dmanage_set_ip_address(unsigned int dev_id, struct dmanager_card_info card_info,
                           struct dmanager_ip_info config_info);
int dmanage_get_gateway_address(unsigned int dev_id, struct dmanager_card_info card_info,
                                struct dmanager_gtw_info *ack_info);
int dmanage_set_gateway_address(unsigned int dev_id, struct dmanager_card_info card_info,
                                struct dmanager_gtw_info config_info);
int dmanage_set_gateway_address6(unsigned int dev_id, struct dmanager_card_info card_info,
                                 struct dmanager_gtw_info config_info);
int dmanage_get_mac_address(unsigned int dev_id, struct dmanager_card_info card_info,
                            struct dmanager_mac_info *ack_info);
int dmanage_set_mac_address(unsigned int dev_id, struct dmanager_card_info card_info,
                            struct dmanager_mac_info config_info);
int dmanage_get_net_device_info(unsigned int dev_id, struct dmanager_card_info card_info,
                                struct dmanager_net_dev_info *ack_info);
int dmanage_get_pmu_voltage(struct dmanager_pmu_voltage_stru *pmu_voltage);
int dmanage_get_llc_perf_para(unsigned int dev_id, struct dmanager_llc_perf_info *perf_info);
int devdrv_get_root_config(unsigned int devid, const char *name, unsigned char *buf, unsigned int *buf_size);
int devdrv_set_root_config(unsigned int devid, const char *name, unsigned char *buf, unsigned int buf_size);
int devdrv_clear_root_config(unsigned int devid, const char *name);

int dmanage_get_inline_reduce(unsigned int dev_id, struct inline_reduce *data);
int dmanage_inquire_imu_info(unsigned int dev_id, unsigned int *status);
int dmanage_get_computing_power(unsigned int dev_id, struct tag_computing_power_msg *data);
int dmanage_get_bb_errstr(unsigned int dev_id, unsigned int errcode, unsigned char *errstr, int buf_len);
int dmanage_reset_i2c_controller(void);
int dmanage_i2c_gpio_read(unsigned int gpio_num, unsigned int *gpio_value);
int dmanage_get_flash_count(unsigned int device_id, unsigned int *flash_count);
int dmanage_get_flash_info(unsigned int device_id, struct dm_flash_info_stru *flash_info);
int dmanage_get_hiss_status(int device_id, struct dsmi_hiss_status_stru *hiss_status_data);
int dmanage_get_lp_status(int device_id, struct dsmi_lp_status_stru *lp_status_data);
int dmanage_dmp_init(void);
int dmanage_create_capability_group(int dev_id, int ts_id, struct capability_group_info *group_info);
int dmanage_delete_capability_group(int dev_id, int ts_id, int group_id);
int dmanage_get_capability_group_info(int dev_id, int ts_id, int group_id, struct capability_group_info *group_info,
                                      int group_count);
int dms_create_capability_multi_group(int dev_id, struct capability_multi_group_info *multi_group_info);
int dms_delete_capability_multi_group(int dev_id, struct capability_multi_group_info *multi_group_info);
int dms_get_capability_multi_group(int dev_id, struct capability_multi_group_info *multi_group_info);
int dmanage_get_ts_group_check_para(int group_id, int group_count);
int dmanage_get_ts_group_num(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                             unsigned int *ret_size);
int dmanage_get_temperature(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                            unsigned int *size);
int dmanage_set_temperature(unsigned int dev_id, unsigned int sub_cmd, void *out_buf, unsigned int ret_size);
#ifdef CFG_FEATURE_GPIO_STATUS
int dmanage_gpio_read(int device_id, unsigned int phy_gpio_num, unsigned int *gpio_value);
#endif

#ifdef CFG_FEATURE_UFS_INFO_AND_STATUS
int dmanage_get_ufs_status(int device_id, struct dsmi_ufs_status_stru *ufs_status_data);
int dmanage_get_ufs_info(unsigned int device_id, unsigned int vfid, unsigned int sub_cmd, void *buf,
                         unsigned int *size);
int dmanage_set_ufs_info(unsigned int device_id, unsigned int sub_cmd, void *buf, unsigned int size);
#endif

#ifdef CFG_FEATURE_POWER_STATE
int dmanage_get_power_state(unsigned int device_id, unsigned int vfid, unsigned int sub_cmd, void *buf,
                            unsigned int *size);
#endif

#ifdef CFG_FEATURE_SOC_INFO
int dmanage_get_soc_info(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                         unsigned int *ret_size);
#endif

drvError_t dms_get_pcie_info(unsigned int dev_id, unsigned int info_type, void *buf, int *size);
/*
 * diff soc adapt
 */
int dmanage_get_optical_module_temp_adapt(unsigned int dev_id, int *value);
int dmanage_get_network_health_adapt(unsigned int dev_id, unsigned int *health);
int dmanage_get_network_dev_info_adapt(struct dmanage_pri_auth *auth, const char *inbuf, unsigned int size_in,
                                       char *outbuf, unsigned int *size_out);
void devdrv_p2p_mem_sync_to_flash(void);

int dmanage_get_imu_info(unsigned int dev_id, unsigned char *send, unsigned int send_len, unsigned char *ack,
                         unsigned int *ack_len);
int devdrv_get_user_config(u32 devid, const char *name, u8 *buf, u32 *buf_size);
int devdrv_set_user_config(u32 devid, const char *name, u8 *buf, u32 buf_size);
int devdrv_clear_user_config(u32 devid, const char *name);
drvError_t drvDeviceGetPcieIdInfo(uint32_t devId, struct tag_pcie_idinfo *pcie_idinfo);
int dmanage_get_optical_module_temp(unsigned int dev_id, int *value);

#ifdef CFG_FEATURE_PSS_SIGN
int dmanage_set_device_sign(unsigned int dev_id, unsigned int subcmd, void *buf, unsigned int buf_size);
int dmanage_get_device_sign(unsigned int dev_id, unsigned int vfid, unsigned int subcmd, void *buf,
                            unsigned int *buf_size);
#endif

int dmanage_set_device_sec_info(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int buf_size);
int dmanage_get_device_sec_info(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf,
                                unsigned int *buf_size);

typedef struct tagDrvModuleStatus {
    unsigned int ai_core_error_bitmap;
    unsigned char lpm3_start_fail;
    unsigned char lpm3_lost_heart_beat;
    unsigned char ts_start_fail;
    unsigned char ts_lost_heart_beat;
    unsigned char ts_sram_broken;
    unsigned char ts_sdma_broken;
    unsigned char ts_bs_broken;
    unsigned char ts_l2_buf0_broken;
    unsigned char ts_l2_buf1_broken;
    unsigned char ts_spcie_broken;
    unsigned char ts_ai_core_broken;
    unsigned char ts_hwts_broken;
    unsigned char ts_doorbell_broken;
} drvModuleStatus_t;

#ifdef CFG_SOC_PLATFORM_CLOUD
drvError_t drvGetDeviceModuleStatus(uint32_t devId, drvModuleStatus_t *moduleStatus);
#else
drvError_t drvGetDeviceModuleStatus(drvModuleStatus_t *moduleStatus);
#endif
drvError_t drvCheckDevid(unsigned int devId);
drvError_t drvGetDeviceSplitMode(unsigned int dev_id, unsigned int *mode);
drvError_t drvCreateVdevice(u32 devid, u32 vdev_id, struct dsmi_create_vdev_res_stru *vdev_res,
                            struct dsmi_create_vdev_result *vdev_result);
drvError_t drvGetVdevTotalInfo(u32 devid, u32 main_cmd, u32 sub_cmd, void *vdev_res, u32 *size);
drvError_t drvGetVdevFreeInfo(u32 devid, u32 main_cmd, u32 sub_cmd, void *vdev_res, u32 *size);
drvError_t drvGetVdevActivityInfo(u32 devid, u32 main_cmd, u32 sub_cmd, void *vdev_res, u32 *size);
drvError_t drvGetSingleVdevInfo(u32 devid, u32 main_cmd, u32 sub_cmd, void *vdev_res, u32 *size);
drvError_t drvGetVdevTopsPercentage(u32 devid, u32 main_cmd, u32 sub_cmd, void *tops_ratio, u32 *size);

drvError_t drvGetVdavinciIdByIndex(uint32_t devIndex, uint32_t *vdevId);
drvError_t drvGetCommonDriverInitStatus(int *status);
drvError_t drvGetDeviceResourceInfo(u32 devid, struct dsmi_resource_para *para, struct dsmi_resource_info *info);
drvError_t drvSetSvmVdevInfo(unsigned int devid, unsigned int sub_cmd, const void *buf, unsigned int buf_size);
drvError_t drvGetSvmVdevInfo(unsigned int devid, unsigned int sub_cmd, void *buf, unsigned int *buf_size);

/* internal */
#define IPCDRV_MSG_LENGTH 28
struct msgHeader {
    unsigned int msgType : 1;
    unsigned int cmdType : 7;
    unsigned int syncType : 1;
    unsigned int res : 1;
    unsigned int msg_length : 14;
    unsigned int msg_index : 8;
};

struct ipcMessage {
    struct msgHeader header;
    char payload[IPCDRV_MSG_LENGTH];
};

struct tsMessage {
    struct ipcMessage msg;
    uint8_t channelType;
    void (*eventCallbackfunc)(int32_t *result, void *args);
};

drvError_t drvIPCMsgRxASync(uint32_t devId, struct tsMessage *msg);
drvError_t drvIPCMsgTxASync(uint32_t devId, struct tsMessage *msg);

#ifdef CFG_SOC_PLATFORM_CLOUD
typedef enum {
    HISI_RPROC_RX_IMU_MBX0,
    HISI_RPROC_RX_IMU_MBX1,
    HISI_RPROC_RX_TS_MBX2,
    HISI_RPROC_TX_TS_MBX17,
    HISI_RPROC_TX_IMU_MBX24,
    HISI_RPROC_TX_IMU_MBX25,
    HISI_RPROC_MAX
} rproc_id_t;
#else
typedef enum {
    HISI_RPROC_TX_TS,
    HISI_RPROC_TX_LPM3,
    HISI_RPROC_RX_TS_MBX4,   /* ACPU0 */
    HISI_RPROC_RX_LPM3_MBX5, /* ACPU0 */

    HISI_RPROC_RX_TS_MBX6,   /* ACPU1 */
    HISI_RPROC_RX_LPM3_MBX7, /* ACPU1 */

    HISI_RPROC_RX_TS_MBX8,   /* ACPU2 */
    HISI_RPROC_RX_LPM3_MBX9, /* ACPU2 */

    HISI_RPROC_RX_TS_MBX10,   /* ACPU3 */
    HISI_RPROC_RX_LPM3_MBX11, /* ACPU3 */
    HISI_RPROC_MAX = 0xFF
} rproc_id_t;
#endif

#define MCU_RESP_LEN 28
struct devdrv_mcu_msg {
    unsigned char dev_id;
    unsigned char rw_flag;
    unsigned char send_len;
    unsigned char *send_data;
    unsigned char resp_len;
    unsigned char resp_data[MCU_RESP_LEN];
};

drvError_t drvStartTimeSyncServeDevice(void);

/* drv_devmng ipc struct info */
struct ipc_msg {
    void *send_buf;
    void *recv_buf;
    unsigned int send_len;
    unsigned int recv_len;
};

struct ioctl_ipc {
    unsigned int dev_id;
    unsigned char target_id;
    unsigned char main_cmd;
    unsigned char sub_cmd;
    struct ipc_msg msg;
};

/**
 * @ingroup driver
 * @brief  set or get imu,ts info from ipc channel
 * @attention this interface use only in device side, not support in host side
 * if the type you input is not compatitable with the table below, then will return failed
 * ---------------------------------------------------------------------------------------------------------
 * targetId               | mainCmd             | subCmd                 | support board type              |
 * ---------------------------------------------------------------------------------------------------------
 * TARGET_ID_LP           | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_SOC_TEMP   | ascend310,ascend310p   |
 * TARGET_ID_LP           | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_AIC0_FREQ  | ascend310p             |
 * TARGET_ID_LP           | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_AIC1_FREQ  | ascend310p             |
 * TARGET_ID_LP           | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_CCPU_FREQ  | ascend310p             |
 * TARGET_ID_LP           | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_DDR_FREQ   | ascend310,ascend310p   |
 * ---------------------------------------------------------------------------------------------------------
 * TARGET_ID_IMU          | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_SOC_TEMP   | ascend910                       |
 * TARGET_ID_IMU          | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_HBM_TEMP   | ascend910                       |
 * TARGET_ID_IMU          | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_NDIE_TEMP  | ascend910                       |
 * TARGET_ID_IMU          | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_AIC0_FREQ  | ascend910                       |
 * TARGET_ID_IMU          | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_DDR_FREQ   | ascend910                       |
 * TARGET_ID_IMU          | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_HBM_FREQ   | ascend910                       |
 * TARGET_ID_IMU          | IPC_MAIN_CMD_QUERY  | IPC_SUB_CMD_SOC_POWER  | ascend910                       |
 * --------------------------------------------------------------------------------------------------------
 * TARGET_ID_TSC          | mainCmd             | subCmd                 | not support                     |
 * --------------------------------------------------------------------------------------------------------
 * TARGET_ID_TSV          | mainCmd             | subCmd                 | not support                     |
 * --------------------------------------------------------------------------------------------------------
 * TARGET_ID_SAFETYISLAND | mainCmd             | subCmd                 | not support                     |
 * --------------------------------------------------------------------------------------------------------
 * @param [in] devId: chip id
 * @param [in] targetId: ipc machine id that that you want get info from
 * @param [in] mainCmd: ipcMainCmdType
 * @param [in] subCmd: ipcSubCmdType
 * @param [out] msg: msg return from driver
 * @return   0 for success, others for fail
 */
int ipc_msg_request(unsigned int devId, unsigned char targetId, unsigned char mainCmd, unsigned char subCmd,
                    struct ipc_msg *msg);
/* internal end */
#endif
