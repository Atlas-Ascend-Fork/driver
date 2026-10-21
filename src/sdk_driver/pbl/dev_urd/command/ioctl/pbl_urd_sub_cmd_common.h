/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef URD_SUB_CMD_DEF_H
#define URD_SUB_CMD_DEF_H
#include "dev_mon_cmd_def.h"

#ifdef CFG_HOST_ENV
#define URD_MAX_UDEV_NUM 1140
#else
#define URD_MAX_UDEV_NUM 64
#endif

/* subcmd: The main cmd is dmp common, dmp davinci cmd */
#define ZERO_CMD 0

/* subcmd: The main cmd is DMS_MAIN_CMD_BASIC */
#define DMS_SUBCMD_GET_FAULT_EVENT 0
#define DMS_SUBCMD_GET_EVENT_CODE 1
#define DMS_SUBCMD_GET_HEALTH_CODE 2
#define DMS_SUBCMD_QUERY_STR 3
#define DMS_SUBCMD_CLEAR_EVENT 4
#define DMS_SUBCMD_DISABLE_EVENT 5
#define DMS_SUBCMD_ENABLE_EVENT 6
#define DMS_SUBCMD_GET_PCIE_INFO 7
#define DMS_GET_VDEVICE_INFO 11
#define DMS_GET_AI_DDR_INFO 12
#define DMS_SUBCMD_GET_AI_INFO_FROM_TS 13
#define DMS_SUBCMD_GET_DEV_TOPOLOGY 14
#define DMS_SUBCMD_GET_DEV_SPLIT_MODE 15
#define DMS_SUBCMD_SRIOV_SWITCH 16
#define DMS_SUBCMD_GET_HCCL_DEVICE_INFO 17
#define DMS_SUBCMD_GET_CORE_SPEC_INFO 18
#define DMS_SUBCMD_GET_CORE_INUSE_INFO 19
#define DMS_SUBCMD_GET_HISTORY_FAULT_EVENT 20
#define DMS_SET_TIME_ZONE_SYNC 21
#define DMS_SUBCMD_GET_DEVICES_TOPOLOGY 22
#define DMS_SUBCMD_GET_HOST_OSC_FREQ 23
#define DMS_SUBCMD_GET_DEV_OSC_FREQ 24
#define DMS_SUBCMD_GET_DEV_INIT_STATUS 25
#define DMS_SUBCMD_INJECT_FAULT 26
#define DMS_SUBCMD_GET_FAULT_INJECT_INFO 27
#define DMS_SUBCMD_GET_FAULT_EVENT_OBJ 28
#define DMS_SUBCMD_GET_CHIP_INFO 29
#define DMS_SUBCMD_GET_DEVICE_STATE 30
#define DMS_SUBCMD_GET_BOARD_ID_HOST 31
#define DMS_SUBCMD_GET_SLOT_ID_HOST 32
#define DMS_SUBCMD_GET_BOM_ID_HOST 33
#define DMS_SUBCMD_GET_PCB_ID_HOST 34
#define DMS_SUBCMD_GET_DEV_BOOT_STATUS 35
#define DMS_SUBCMD_GET_CHIP_TYPE 36
#define DMS_SUBCMD_GET_MASTER_DEV 37
#define DMS_SUBCMD_GET_DEV_PROBE_NUM 38
#define DMS_SUBCMD_GET_DEV_PROBE_LIST 39
#define DMS_SUBCMD_DEV_P2P_ATTR 40
#define DMS_SUBCMD_GET_H2D_DEV_INFO 41
#define DMS_SUBCMD_GET_CHIP_COUNT 42
#define DMS_SUBCMD_GET_CHIP_LIST 43
#define DMS_SUBCMD_GET_DEVICE_FROM_CHIP 44
#define DMS_SUBCMD_GET_CHIP_FROM_DEVICE 45
#define DMS_SUBCMD_GET_AICORE_DIE_NUM 46
#define DMS_SUBCMD_SET_CC_INFO 47
#define DMS_SUBCMD_GET_CC_INFO 48
#define DMS_SUBCMD_GET_CONNECT_TYPE 49
#define DMS_SUBCMD_GET_CPU_WORK_MODE 50
#define DMS_SUBCMD_GET_ALL_DEV_LIST 51
#define DMS_SUBCMD_GET_PROCESS_LIST 52
#define DMS_SUBCMD_GET_PROCESS_MEMORY 53
#define DMS_SUBCMD_GET_TOKEN_VAL 54
#define DMS_SUBCMD_GET_HCOM_CPU_NUM 55
#define DMS_SUBCMD_GET_UDIS_DEVICE_INFO 56
#define DMS_SUBCMD_SET_UDIS_DEVICE_INFO 57
#define DMS_SUBCMD_GET_PHY_DEVICE_INFO 58
#define DMS_SUBCMD_CREATE_CAPABILITY_GROUP 59
#define DMS_SUBCMD_DELETE_CAPABILITY_GROUP 60
#define DMS_SUBCMD_GET_CAPABILITY_GROUP 61
#define DMS_SUBCMD_GET_CAPABILITY_GROUP_NUM 62
#define DMS_SUBCMD_GET_NOTIFY_EVENT_OBJ 63
#define DMS_SUBCMD_GET_MODULE_FEATURE 64

/* subcmd: The main cmd is DMS_MAIN_CMD_SOC */
#define DMS_SUBCMD_GET_PCB_ID 0
#define DMS_SUBCMD_GET_BOM_ID 1
#define DMS_SUBCMD_GET_SLOT_ID 2
#define DMS_SUBCMD_GET_CPU_INFO 3
#define DMS_SUBCMD_GET_CPU_UTILIZATION 4
#define DMS_SUBCMD_GET_BOARD_ID 5
#define DMS_SUBCMD_GET_SOC_INFO 7

/* subcmd: The main cmd is DMS_MAIN_CMD_MEMORY */
#define DMS_SUBCMD_DDR_BW_UTIL_RATE 0
#define DMS_SUBCMD_HBM_BW_UTIL_RATE 1
#define DMS_SUBCMD_DDR_FREQUENCY 4
#define DMS_SUBCMD_CGROUP_MEM_INFO 5
#define DMS_SUBCMD_HBM_TEMPERATURE 6
#define DMS_SUBCMD_HBM_FREQUENCY 7
#define DMS_SUBCMD_HBM_CAPACITY 8
#define DMS_SUBCMD_HBM_SIGLE_ECC_HW_INFO 9
#define DMS_SUBCMD_HBM_MULTI_ECC_HW_INFO 10
#define DMS_SUBCMD_SERVICE_MEM_INFO 32
#define DMS_SUBCMD_SYSTEM_MEM_INFO 33
#define DMS_SUBCMD_DDR_MEM_INFO 34
#define DMS_SUBCMD_HBM_MEM_INFO 35
#define DMS_SUBCMD_HBM_GET_VA 36
#define DMS_SUBCMD_GET_FAULT_SYSCNT 37
#define DMS_SUBCMD_DDR_FREQUENCY_V2 38
#define DMS_SUBCMD_DEV_MEM_INFO 39
#define DMS_SUBCMD_HBM_ARRAY_TEMP 40
/* subcmd: The main cmd is DMS_MAIN_CMD_SVM */
#define DMS_SUBCMD_QUERY_SVM_FEATURE 0

/* subcmd: The main cmd is DMS_MAIN_CMD_LPM */
#define DMS_SUBCMD_GET_TEMPERATURE 0
#define DMS_SUBCMD_GET_POWER 1
#define DMS_SUBCMD_GET_VOLTAGE 2
#define DMS_SUBCMD_GET_FREQUENCY 3
#define DMS_SUBCMD_GET_TSENSOR 4
#define DMS_SUBCMD_GET_MAX_FREQUENCY 5
#define DMS_SUBCMD_PASS_THROUGTH_MCU 6
#define DMS_SUBCMD_GET_LP_STATUS 7
#define DMS_SUBCMD_GET_AIC_VOL_CUR 8
#define DMS_SUBCMD_GET_HYBIRD_VOL_CUR 9
#define DMS_SUBCMD_GET_TAISHAN_VOL_CUR 10
#define DMS_SUBCMD_GET_DDR_VOL_CUR 11
#define DMS_SUBCMD_GET_ACG 12
#define DMS_SUBCMD_GET_TEMP_DDR 13
#define DMS_SUBCMD_GET_DDR_THOLD 14
#define DMS_SUBCMD_GET_SOC_THOLD 15
#define DMS_SUBCMD_GET_SOC_MIN_THOLD 16
#define DMS_SUBCMD_SET_TEMP_THOLD 17
#define DMS_SUBCMD_GET_FREQUENCY_V2 18
#define DMS_SUBCMD_POWER_CONS_COEF 19

/* subcmd: The main cmd is DMS_MAIN_CMD_PRODUCT */
#define DMS_SUBCMD_GET_VRD_INFO 2

/* subcmd: The main cmd is DMS_MAIN_CMD_SOFT_FAULT */
#define DMS_SUBCMD_SENSOR_NODE_REGISTER 0
#define DMS_SUBCMD_SENSOR_NODE_UNREGISTER 1
#define DMS_SUBCMD_SENSOR_NODE_UPDATE_VAL 2

/* subcmd: The main cmd is DMS_MAIN_CMD_TRS */
#define DMS_SUBCMD_CREATE_GROUP 0
#define DMS_SUBCMD_DEL_GROUP 1
#define DMS_SUBCMD_GET_GROUP 2
#define DMS_SUBCMD_GET_AI_INFO 3
#define DMS_SUBCMD_GET_TS_HB_STATUS 4
#define DMS_SUBCMD_GET_AI_INFO_ASYN 5
#define DMS_SUBCMD_SET_AI_INFO_ASYN 6
#define DMS_SUBCMD_SET_STL_RUNNING_STATUS 7
#define DMS_SUBCMD_UPDATE_TS_PATCH 8
#define DMS_SUBCMD_LOAD_TS_PATCH 9
#define DMS_SUBCMD_QUERY_TRS_FEATURE 10
#define DMS_SUBCMD_CREATE_CAPABILITY_MULTI_GROUP 11
#define DMS_SUBCMD_DELETE_CAPABILITY_MULTI_GROUP 12
#define DMS_SUBCMD_GET_CAPABILITY_MULTI_GROUP 13

/* subcmd: The main cmd is DMS_MAIN_CMD_SENSORHUB */
#define DMS_SUBCMD_GET_SENSORHUB_STATUS 0
#define DMS_SUBCMD_GET_SENSORHUB_CONFIG 1

/* subcmd: The main cmd is DMS_MAIN_CMD_ISP */
#define DMS_SUBCMD_GET_ISP_STATUS 0
#define DMS_SUBCMD_GET_ISP_CONFIG 1

/* subcmd: The main cmd is DMS_MAIN_CMD_CAN */
#define DMS_SUBCMD_GET_CAN_STATUS 0

/* subcmd: The main cmd is DMS_MAIN_CMD_BBOX  */
#define DMS_SUBCMD_GET_LAST_BOOT_STATE 0
#define DMS_SUBCMD_GET_LOG_DUMP_INFO 1
#define DMS_SUBCMD_GET_PCIE_LOG_DUMP_INFO 2
#define DMS_SUBCMD_GET_BBOX_DATA 3
#define DMS_SUBCMD_SET_BBOX_DATA 4
#define DMS_SUBCMD_GET_BBOX_FEATURE 5

/* subcmd: The main cmd is DMS_MAIN_CMD_EMMC  */
#define DMS_SUBCMD_GET_EMMC_INFO 0

/* subcmd: The main cmd is DMS_MAIN_CMD_PCIE */
#define DMS_SUBCMD_GET_PCIE_LINK_INFO 0

/* subcmd: The main cmd is DMS_MAIN_CMD_HOTRESET */
#define DMS_SUBCMD_HOTRESET_ASSEMBLE 0
#define DMS_SUBCMD_HOTRESET_SETFLAG 1
#define DMS_SUBCMD_HOTRESET_CLEARFLAG 2
#define DMS_SUBCMD_HOTRESET_UNBIND 3
#define DMS_SUBCMD_HOTRESET_RESET 4
#define DMS_SUBCMD_HOTRESET_REMOVE 5
#define DMS_SUBCMD_HOTRESET_RESCAN 6
#define DMS_SUBCMD_PRERESET_ASSEMBLE 7
#define DMS_SUBCMD_PRERESET_ASSEMBLE1 8

/* subcmd: The main cmd is DMS_MAIN_CMD_QOS */
#define DMS_SUBCMD_GET_CONFIG_INFO 0

/* subcmd: The main cmd is DMS_MAIN_CMD_LOG */
#define DMS_SUBCMD_GET_LOG_INFO 0

/* subcmd: The main cmd is DMS_MAIN_CMD_CAPA_GROUP */
#define DMS_SUBCMD_CREATE_CAPA_GROUP 0
#define DMS_SUBCMD_DEL_CAPA_GROUP 1
#define DMS_SUBCMD_GET_CAPA_GROUP 2

/* subcmd: The main cmd is ASCEND_UB_CMD_BASIC */
#define ASCEND_UB_SUBCMD_GET_URMA_NAME 0
#define ASCEND_UB_SUBCMD_HOST_DEVICE_RELINK 1
#define ASCEND_UB_SUBCMD_GET_EID_INDEX 2
#define ASCEND_UB_SUBCMD_GET_D2D_EID 3
#define ASCEND_UB_SUBCMD_GET_BUS_INST_EID 4
#define ASCEND_UB_SUBCMD_GET_DEV_ID_INFO 5

/* subcmd: The main cmd is DMS_MAIN_CMD_DVPP */
#define DMS_SUBCMD_GET_DVPP_STATUS 0
#define DMS_SUBCMD_GET_DVPP_RATE 1

/* subcmd: The main cmd is DMS_MAIN_CMD_SILS */
#define DMS_SUBCMD_GET_SILS_EMU_INFO 0
#define DMS_SUBCMD_GET_SILS_HEALTH_STATUS 1
#define DMS_SUBCMD_SET_SILS_PMUWDG_INFO 2
#define DMS_SUBCMD_GET_SILS_PMUWDG_INFO 3
#define DMS_SUBCMD_SET_SILS_HARDWARE_TEST 4
#define DMS_SUBCMD_GET_SILS_HARDWARE_TEST 5
#define DMS_SUBCMD_SET_SILS_DEVICE_INFO 6
#define DMS_SUBCMD_GET_SILS_DEVICE_INFO 7

/* subcmd: The main cmd is DMS_MAIN_CMD_P2P_COM */
#define DMS_SUBCMD_P2P_COM_STATUS 0
#define DMS_SUBCMD_P2P_COM_FORCE_LINKDOWN 1
#define DMS_SUBCMD_P2P_PERMIT_SUSPEND 2
#define DMS_SUBCMD_P2P_PERMIT_LOWPOWER 3
#define DMS_SUBCMD_P2P_PERMIT_SAVEPOWER 4

/* subcmd: The main cmd is DMS_MAIN_CMD_FLASH */
#define DMS_SUBCMD_GET_FLASH_ERASE_COUNT 0
#define DMS_SUBCMD_GET_FLASH_ERASE_COUNT_RESERVED1 1
#define DMS_SUBCMD_GET_FLASH_ERASE_COUNT_RESERVED2 2
#define DMS_SUBCMD_GET_FLASH_ERASE_COUNT_RESERVED3 3
#define DMS_SUBCMD_FW_WRITE_PROTECTION 0x10

/* filter:  dsmi_get_device_info main cmd & subcmd, combine urd filter */
/* use urd main cmd: DMS_GET_SET_DEVICE_INFO_CMD, sub cmd: ZERO_CMD */

#define DMS_MASK_USAGE "data[0]=0x01,data[1]=0x02"
#define DMS_MASK_FREQ "data[0]=0x01,data[1]=0x03"

#define DMS_FILTER_SET_CAN_CONFIG "main_cmd=0x3"

#define DMS_MAIN_CMD_LP 0x00008 /* DSMI_MAIN_CMD_LP */
#define DMS_SUBCMD_LP_SUSPEND 0
#define DMS_MAIN_CMD_TEMP 0x00032 /* DSMI_MAIN_CMD_TEMP */

#define DMS_SUBCMD_LP_VOLTAGE_CURRENT_AICORE 0
#define DMS_SUBCMD_LP_VOLTAGE_CURRENT_HYBRID 1
#define DMS_SUBCMD_LP_VOLTAGE_CURRENT_TAISHAN 2
#define DMS_SUBCMD_LP_VOLTAGE_CURRENT_DDR 3
#define DMS_SUBCMD_LP_ACG 4
#define DMS_SUBCMD_LP_STATUS 5
#define DMS_SUBCMD_LP_GET_ALL_TOPS 6
#define DMS_SUBCMD_LP_SET_TOPS 7
#define DMS_SUBCMD_LP_GET_CUR_TOPS 8
#define DMS_SUBCMD_LP_AICORE_FREQREDUC_CAUSE 9
#define DMS_SUBCMD_LP_GET_POWER_INFO 10
#define DMS_SUBCMD_LP_SET_IDLE_SWITCH 11
#define DMS_SUBCMD_LP_SET_STRESS_TEST 12 /* 2024-12-30 reserve */
#define DMS_SUBCMD_LP_GET_AIC_CPM 13     /* 2024-12-30 reserve */
#define DMS_SUBCMD_LP_GET_BUS_CPM 14     /* 2024-12-30 reserve */
#define DMS_SUBCMD_LP_FEATURE_SWITCH 15
#define DMS_SUBCMD_LP_INNER_START 0x100 /* start for inner used sub_cmd, not for product */
#define DMS_SUBCMD_LP_INNER_SET_STRESS_TEST 0x101
#define DMS_SUBCMD_LP_INNER_GET_AIC_CPM 0x102
#define DMS_SUBCMD_LP_INNER_GET_BUS_CPM 0x103
#define DMS_SUBCMD_LP_INNER_GET_STRESS_FREQ_VOLT 0x104
#define DMS_SUBCMD_LP_INNER_GET_TEMP_INFO 0x105
#define DMS_SUBCMD_LP_INNER_GET_AIC_INFO 0x106
#define DMS_SUBCMD_LP_INNER_GET_BUS_INFO 0x107
#define DMS_SUBCMD_LP_INNER_GET_HBM_INFO 0x108

#define DMS_SUBCMD_LP_SET_LPTEST 1000 /* for lptest debug */
#define DMS_SUBCMD_LP_GET_LPTEST 1001

#define DMS_SUBCMD_TEMP_DDR 0
#define DMS_SUBCMD_TEMP_DDR_THOLD 1
#define DMS_SUBCMD_TEMP_SOC_THOLD 2
#define DMS_SUBCMD_TEMP_SOC_MIN_THOLD 3

#define FILTER_DEVICE_INFO "main_cmd=0x8" // MAIN_CMD_LP
#define FILTER_TEMP_INFO "main_cmd=0x32"  // MAIN_CMD_TEMP

/* HCCS */
#define DMS_FILTER_HCCS_CREDIT_INFO "main_cmd=0x10,sub_cmd=0x4"
#define DMS_FILTER_HCCS "main_cmd=0x10"
#define DMS_HCCS_SUB_CMD_STATUS 0
#define DMS_HCCS_SUB_CMD_LANE_INFO 1
#define DMS_HCCS_SUB_CMD_PING_INFO 2
#define DMS_HCCS_SUB_CMD_STATISTIC_INFO 3
#define DMS_HCCS_SUB_CMD_CREDIT_INFO 4
#define DMS_HCCS_SUB_CMD_STATISTIC_INFO_EXT 5

#define DMS_BIST_CMD_GET_RSLT 0
#define DMS_BIST_CMD_SET_MODE 1
#define DMS_BIST_CMD_GET_VEC_CNT 2
#define DMS_BIST_CMD_GET_VEC_TIME 3
#define DMS_BIST_CMD_SET_MBIST_MODE 4
#define DMS_BIST_CMD_GET_MBIST_RESULT 5
#define DMS_BIST_CMD_GET_CRACK_RESULT 6
#define DMS_BIST_CMD_START_RBIST 7
#define DMS_BIST_CMD_GET_RBIST_RESULT 8
#define DMS_BIST_CMD_SET_FREQVOLT 9

#define DMS_DTM_OPCODE_GET_INFO_LIST 0
#define DMS_DTM_OPCODE_GET_STATE 1
#define DMS_DTM_OPCODE_GET_CAPACITY 2
#define DMS_DTM_OPCODE_SET_POWER_STATE 3
#define DMS_DTM_OPCODE_SCAN 4
#define DMS_DTM_OPCODE_FAULT_DIAG 5
#define DMS_DTM_OPCODE_EVENT_NOTIFY 6
#define DMS_DTM_OPCODE_GET_LINK_STATE 7
#define DMS_DTM_OPCODE_SET_LINK_STATE 8
#define DMS_DTM_OPCODE_FAULT_INJECT 9
#define DMS_DTM_OPCODE_ENABLE_DEVICE 10
#define DMS_DTM_OPCODE_DISABLE_DEVICE 11
#define DMS_DTM_OPCODE_SET_POWER_INFO 12
#define DMS_DTM_OPCODE_GET_POWER_INFO 13

#define DMS_DTM_POWER_CAPABILITY 0

/* Fault inject */
#define DMS_FAULT_INJECT_SUB_CMD_MEMORY 0

/* IMU sub cmd */
#define DMS_IMU_SUBCMD_DMP_MSG_RECV 0
#define DMS_IMU_SUBCMD_DMP_MSG_SEND 1

#define DMS_CONTENT_RTC_CONFIG 0
enum urd_devdrv_p2p_attr_op {
    DEVDRV_P2P_ADD = 0,
    DEVDRV_P2P_DEL,
    DEVDRV_P2P_QUERY,
    DEVDRV_P2P_ACCESS_STATUS_QUERY,
    DEVDRV_P2P_CAPABILITY_QUERY,
    DEVDRV_P2P_MAX
};

struct ubmem_dev_repair_msg_in {
    unsigned int devid;
    unsigned int udevid;
    int cp1_tgid;
};

#define UBMEM_DEV_REPAIR_SUCCESS 0U
#define UBMEM_DEV_REPAIR_NO_FAULT 1U
#define UBMEM_DEV_REPAIR_TASK_NO_USED 2U
struct ubmem_dev_repair_msg_out {
    unsigned int result;
};

struct ubmem_map_route_check_msg_in {
    unsigned int dst_devid;
    unsigned int src_server_id;
    unsigned int src_mem_id;
    unsigned int map_route;
};

struct ubmem_map_route_check_msg_out {
    int is_exist;
};

struct urd_probe_dev_info {
    unsigned int num_dev;
    unsigned int devids[URD_MAX_UDEV_NUM];
};

struct urd_p2p_attr {
    unsigned int op;
    unsigned int dev_id;
    unsigned int peer_dev_id;
    unsigned int type;
    int status;
    unsigned long long capability;
};

struct urd_imu_dmp_info {
    unsigned int dev_id;
    unsigned char len;
    unsigned char data[220]; /* 220:dmp data length of the IMU */
};

#define DMS_CMDLINE_SIZE 4096

enum dms_cc_mode_type {
    DMS_CC_MODE_OFF = 0,
    DMS_CC_MODE_NORMAL,
    DMS_CC_MODE_ADDITIONAL,
    DMS_CC_MODE_MAX,
};
enum dms_crypto_mode_type {
    DMS_CRYPTO_MODE_OFF = 0,
    DMS_CRYPTO_MODE_ON,
    DMS_CRYPTO_MODE_MAX,
};

#define DMS_CC_RESV_LEN 8
struct dms_cc_mode {
    enum dms_cc_mode_type cc_mode;         /* 0:cc off  1:cc normal  2:cc additional */
    enum dms_crypto_mode_type crypto_mode; /* 0:crypto off  1:crypto on */
    unsigned int resv[DMS_CC_RESV_LEN];
};

struct dms_cc_info {
    struct dms_cc_mode cc_running_info;
    struct dms_cc_mode cc_cfg_info;
};

typedef struct bbox_data_info {
    unsigned int dev_id;
    unsigned int type;
    unsigned int offset;
    unsigned int len;
    void *bbox_data_buf;
} bbox_data_info_t;

#define UDIS_MAX_NAME_LEN 16
struct udis_get_ioctl_in {
    unsigned int module_type;
    char name[UDIS_MAX_NAME_LEN];
    unsigned int in_len;
    void *data;
};

struct udis_get_ioctl_out {
    unsigned long long last_update_time;
    unsigned int acc_ctrl;
    unsigned int out_len;
};

struct udis_set_ioctl_in {
    unsigned int module_type;
    char name[UDIS_MAX_NAME_LEN];
    unsigned int acc_ctrl;
    unsigned long long last_update_time;
    unsigned int data_len;
    const void *data;
};

struct urd_dev_mem_type {
    unsigned int dev_id;
    unsigned int type;
};

#endif
