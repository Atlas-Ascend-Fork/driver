/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DCMI_INTERFACE_API_H__
#define __DCMI_INTERFACE_API_H__

#include <stdbool.h>
#include "dsmi_common_interface_custom.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#ifdef __linux
#define DCMIDLLEXPORT
#else
#define DCMIDLLEXPORT _declspec(dllexport)
#endif

#define MAX_VER_LEN 255 // Maximum length of version string

#define MAX_CARD_NUM 64 // The system supports up to 64 cards

#define MAX_CHIP_NAME_LEN 32 // Maximum length of chip name

#define MAX_LENTH 256 // Maximum length of string

#define DIE_ID_COUNT 5 // Number of die ID characters

#define MAX_CORE_NUM 16 // Maximum AI CPU

#define DCMI_SENSOR_DATA_MAX_LEN 16

#define DCMI_SENSOR_TEMP_LEN 2

#define DCMI_SENSOR_NTC_TEMP_LEN 4

#define SYSLOG_RETRY_TIME 5000000 // 5s

#define DCMI_COMPUTE_GROUP_INFO_RES_NUM 8
#define DCMI_AICORE_MASK_NUM 2

#define DCMI_MAX_SPEC_RESERVE 8
#define DCMI_VF_FLAG_BIT 9 // VF: computing virtualization scenario

#define IV_MAX_LEN 64

#define KEY_MAX_LEN 512

#define RESERVED_MAX_LEN 512

#define MAX_RECORD_ECC_ADDR_COUNT 64

#define TOPO_INFO_MAX_LENTH 32 // topo info max length

#define PRIORITY_NUM 8

#define RESERVE_NUMBER_EXT 6

#define TS_RSEV_MAX_LENTH 36

/*----------------------------------------------*
 * Structure description                        *
 *----------------------------------------------*/
struct dcmi_chip_info {
    unsigned char chip_type[MAX_CHIP_NAME_LEN];
    unsigned char chip_name[MAX_CHIP_NAME_LEN];
    unsigned char chip_ver[MAX_CHIP_NAME_LEN];
    unsigned int aicore_cnt;
};

struct dcmi_chip_info_v2 {
    unsigned char chip_type[MAX_CHIP_NAME_LEN];
    unsigned char chip_name[MAX_CHIP_NAME_LEN];
    unsigned char chip_ver[MAX_CHIP_NAME_LEN];
    unsigned int aicore_cnt;
    unsigned char npu_name[MAX_CHIP_NAME_LEN];
};

struct dcmi_pcie_info {
    unsigned int deviceid;
    unsigned int venderid;
    unsigned int subvenderid;
    unsigned int subdeviceid;
    unsigned int bdf_deviceid;
    unsigned int bdf_busid;
    unsigned int bdf_funcid;
};

struct dcmi_pcie_info_all {
    unsigned int venderid;    /* 厂商id */
    unsigned int subvenderid; /* 厂商子id */
    unsigned int deviceid;    /* 设备id */
    unsigned int subdeviceid; /* 设备子id */
    int domain;               /* pcie domain */
    unsigned int bdf_busid;
    unsigned int bdf_deviceid;
    unsigned int bdf_funcid;
    unsigned char reserve[32]; /* the size of dcmi_pcie_info_all is 64 */
};

struct dcmi_ub_id_info {
    unsigned short device_id;        /* 设备id */
    unsigned short vendor_id;        /* 厂商id */
    unsigned short module_vendor_id; /* 厂商子id */
    unsigned short module_id;        /* 设备子id */
    unsigned char reserved[32];      /* consistent with the definition of hisi. */
};

struct dcmi_board_info {
    unsigned int board_id;
    unsigned int pcb_id;
    unsigned int bom_id;
    unsigned int slot_id;
};

struct dcmi_elabel_info {
    char product_name[MAX_LENTH];
    char model[MAX_LENTH];
    char manufacturer[MAX_LENTH];
    char manufacturer_date[MAX_LENTH];
    char serial_number[MAX_LENTH];
};

struct dcmi_die_id {
    unsigned int soc_die[DIE_ID_COUNT];
};

struct dcmi_aicore_info {
    unsigned int freq;
    unsigned int cur_freq;
};

struct dcmi_aicpu_info {
    unsigned int max_freq;
    unsigned int cur_freq;
    unsigned int aicpu_num;
    unsigned int util_rate[MAX_CORE_NUM];
};

struct dcmi_flash_info {
    unsigned long long flash_id;
    unsigned short device_id;
    unsigned short vendor;
    unsigned int state;
    unsigned long long size;
    unsigned int sector_count;
    unsigned short manufacturer_id;
};

struct dcmi_chip_pcie_err_rate {
    unsigned int reg_deskew_fifo_overflow_intr_status;
    unsigned int reg_symbol_unlock_intr_status;
    unsigned int reg_deskew_unlock_intr_status;
    unsigned int reg_phystatus_timeout_intr_status;
    unsigned int symbol_unlock_counter;
    unsigned int pcs_rx_err_cnt;
    unsigned int phy_lane_err_counter;
    unsigned int pcs_rcv_err_status;
    unsigned int symbol_unlock_err_status;
    unsigned int phy_lane_err_status;
    unsigned int dl_lcrc_err_num;
    unsigned int dl_dcrc_err_num;
};

struct dcmi_ecc_info {
    int enable_flag;
    unsigned int single_bit_error_cnt;
    unsigned int double_bit_error_cnt;
    unsigned int total_single_bit_error_cnt;
    unsigned int total_double_bit_error_cnt;
    unsigned int single_bit_isolated_pages_cnt;
    unsigned int double_bit_isolated_pages_cnt;
};

struct dcmi_hbm_info {
    unsigned long long memory_size;
    unsigned int freq;
    unsigned long long memory_usage;
    int temp;
    unsigned int bandwith_util_rate;
};

struct dcmi_memory_info {
    unsigned long long memory_size; /* unit:MB */
    unsigned int freq;
    unsigned int utiliza;
};

struct dcmi_get_memory_info_stru {
    unsigned long long memory_size;      /* unit:MB */
    unsigned long long memory_available; /* free + hugepages_free * hugepagesize */
    unsigned int freq;
    unsigned long hugepagesize; /* unit:KB */
    unsigned long hugepages_total;
    unsigned long hugepages_free;
    unsigned int utiliza;      /* ddr memory info usages */
    unsigned char reserve[60]; /* the size of dcmi_memory_info is 96 */
};

struct dcmi_hbm_product_info {
    unsigned short manufacturer_id;
    unsigned char reserve[62];
};

struct dcmi_dvpp_ratio {
    int vdec_ratio;
    int vpc_ratio;
    int venc_ratio;
    int jpege_ratio;
    int jpegd_ratio;
};

enum SM_KEY_TYPE {
    SM3_NORMAL_SUMMARY = 0,
    SM4_CBC_ENCRYPT = 1,
    SM4_CBC_DECRYPT = 2
};

struct dcmi_sm_parm {
    unsigned int key_type;
    unsigned int key_len;
    unsigned int iv_len;
    unsigned int reserves;
    unsigned char iv[IV_MAX_LEN];
    unsigned char key[KEY_MAX_LEN];
    unsigned char reserved[RESERVED_MAX_LEN];
};

struct dcmi_sm_data {
    const unsigned char *in_buf;
    unsigned int in_len;
    unsigned char *out_buf;
    unsigned int *out_len;
};

typedef enum {
    DCMI_TS_AICORE = 0,
    DCMI_TS_AIVECTOR,
} DCMI_TS_ID;

struct dcmi_capability_group_info {
    unsigned int group_id;
    unsigned int state;
    unsigned int extend_attribute;
    unsigned int aicore_number;
    unsigned int aivector_number;
    unsigned int sdma_number;
    unsigned int aicpu_number;
    unsigned int active_sq_number;
    unsigned int aicore_mask[DCMI_AICORE_MASK_NUM]; /* as output in dcmi_get_capability_group_info */
    unsigned int res[DCMI_COMPUTE_GROUP_INFO_RES_NUM - DCMI_AICORE_MASK_NUM];
};

struct dcmi_cgroup_info {
    unsigned long limit_in_bytes;
    unsigned long max_usage_in_bytes;
    unsigned long usage_in_bytes;
};

struct dcmi_llc_perf {
    unsigned int wr_hit_rate;
    unsigned int rd_hit_rate;
    unsigned int throughput;
};

enum dcmi_ip_addr_type {
    DCMI_IPADDR_TYPE_V4 = 0, /** IPv4 */
    DCMI_IPADDR_TYPE_V6 = 1, /** IPv6 */
    DCMI_IPADDR_TYPE_ANY = 2 /** IPv4+IPv6 ("dual-stack") */
};

struct dcmi_ip_addr {
    union {
        unsigned char ip6[16];
        unsigned char ip4[4];
    } u_addr;
    enum dcmi_ip_addr_type ip_type;
};

struct dcmi_proc_mem_info {
    int proc_id;
    unsigned long proc_mem_usage;
};

enum ts_dcmi_msg_type_t {
    DCMI_TYPE_STARS_TASK_TIMEOUT_SET = 0,
    DCMI_TYPE_STARS_TASK_TIMEOUT_GET,
};

typedef struct {
    unsigned int timeout_limit_exp; // 范围：[20,32]
    unsigned char rsev[TS_RSEV_MAX_LENTH];
} ts_dcmi_task_timeout_t;

struct ts_dcmi_ctrl_msg_body_t {
    unsigned int msg_type;
    union {
        ts_dcmi_task_timeout_t set_task_timeout_info;
        ts_dcmi_task_timeout_t get_task_timeout_info;
    } u;
};

enum dcmi_reset_channel {
    OUTBAND_CHANNEL = 0,
    INBAND_CHANNEL
};

enum dcmi_unit_type {
    NPU_TYPE = 0,
    MCU_TYPE = 1,
    CPU_TYPE = 2,
    INVALID_TYPE = 0xFF
};

enum dcmi_die_type {
    NDIE,
    VDIE,
    DDIE,
    INVALID_DIE
};

enum dcmi_rdfx_detect_result {
    DCMI_RDFX_DETECT_OK = 0,
    DCMI_RDFX_DETECT_SOCK_FAIL = 1,
    DCMI_RDFX_DETECT_RECV_TIMEOUT = 2,
    DCMI_RDFX_DETECT_UNREACH = 3,
    DCMI_RDFX_DETECT_TIME_EXCEEDED = 4,
    DCMI_RDFX_DETECT_FAULT = 5,
    DCMI_RDFX_DETECT_INIT = 6,
    DCMI_RDFX_DETECT_THREAD_ERR = 7,
    DCMI_RDFX_DETECT_IP_SET = 8,
    DCMI_RDFX_DETECT_MAX = 0xFF
};

enum dcmi_device_compat {
    DCMI_COMPAT_OK = 1,
    DCMI_COMPAT_NOK = 2,
    DCMI_COMPAT_UNKNOWN = 3
};

enum dcmi_port_type {
    DCMI_VNIC_PORT = 0,
    DCMI_ROCE_PORT = 1,
    DCMI_INVALID_PORT
};

enum dcmi_revo_type {
    DCMI_REVOCATION_TYPE_SOC = 0,         /* for SOC revocation */
    DCMI_REVOCATION_TYPE_CMS_CRL = 1,     /* for CMS CRL file upgrade */
    DCMI_REVOCATION_TYPE_CMS_CRL_EXT = 2, /* for extended CRL upgrade */
    DCMI_REVOCATION_TYPE_MAX
};

enum dcmi_main_cmd {
    DCMI_MAIN_CMD_DVPP = 0,
    DCMI_MAIN_CMD_ISP,
    DCMI_MAIN_CMD_TS_GROUP_NUM,
    DCMI_MAIN_CMD_CAN,
    DCMI_MAIN_CMD_UART,
    DCMI_MAIN_CMD_UPGRADE = 5,
    DCMI_MAIN_CMD_UFS,
    DCMI_MAIN_CMD_OS_POWER,
    DCMI_MAIN_CMD_LP,
    DCMI_MAIN_CMD_MEMORY,
    DCMI_MAIN_CMD_RECOVERY,
    DCMI_MAIN_CMD_TS,
    DCMI_MAIN_CMD_CHIP_INF,
    DCMI_MAIN_CMD_QOS,
    DCMI_MAIN_CMD_SOC_INFO,
    DCMI_MAIN_CMD_SILS,
    DCMI_MAIN_CMD_HCCS,
    DCMI_MAIN_CMD_HOST_AICPU,
    DCMI_MAIN_CMD_TEMP = 50,
    DCMI_MAIN_CMD_SVM,
    DCMI_MAIN_CMD_VDEV_MNG,
    DCMI_MAIN_CMD_SEC,
    DCMI_MAIN_CMD_PCIE = 55,
    DCMI_MAIN_CMD_SIO = 56,
    DCMI_MAIN_CMD_EX_COMPUTING = 0x8000,
    DCMI_MAIN_CMD_DEVICE_SHARE = 0x8001,
    DCMI_MAIN_CMD_EX_CERT = 0x8003,
    DCMI_MAIN_CMD_MAX
};

#define DCMI_PCIE_SUB_CMD_PCIE_ERROR_INFO 1

typedef enum {
    DCMI_DEVICE_SHARE_SUB_CMD_COMMON = 0,
    DCMI_DEVICE_SHARE_SUB_CMD_MAX,
} DCMI_DEVICE_SHARE_SUB_CMD;

struct dcmi_pcie_link_error_info {
    unsigned int tx_err_cnt;   // Pcie发送错误计数
    unsigned int rx_err_cnt;   // Pcie接收错误计数
    unsigned int lcrc_err_cnt; // Pcie DLLP LCRC校验错误计数
    unsigned int ecrc_err_cnt; // Pcie TLP ECRC校验错误计数
    unsigned int retry_cnt;    // Pcie链路重传计数
    unsigned int rsv[32];      // 保留
};

typedef enum {
    DCMI_SUB_CMD_HOST_AICPU_INFO = 0,
} DCMI_HOST_AICPU_SUB_CMD;

#define DCMI_HOST_AICPU_BITMAP_LEN 8 /* (Maximum core num:512) / sizeof(unsigned long long) */
#define DCMI_HOST_AICPU_THREAD_MODE 0
#define DCMI_HOST_AICPU_PROCESS_MODE 1
#define DCMI_HOST_AICPU_RESERVED_LEN 16

struct dcmi_host_aicpu_info {
    unsigned int num;
    unsigned long long bitmap[DCMI_HOST_AICPU_BITMAP_LEN];
    unsigned int work_mode;                               /* thread or process */
    unsigned char reserved[DCMI_HOST_AICPU_RESERVED_LEN]; /* reserved data must be set to 0 */
};

/* DCMI sub commond for HCCS module */
typedef enum {
    DCMI_HCCS_CMD_GET_STATUS = 0,
    DCMI_HCCS_CMD_GET_LANE_INFO = 1,
    DCMI_HCCS_CMD_GET_STATISTIC_INFO = 3,
    DCMI_HCCS_CMD_GET_STATISTIC_INFO_U64 = 5,
    DCMI_HCCS_CMD_MAX,
} DCMI_HCCS_SUB_CMD;

#define DCMI_HCCS_MAX_PCS_NUM 16
#define DCMI_HCCS_RES_FIELD_NUM 64 // 预留字段未使用
#define DCMI_HCCS_910B_PCS_NUM 7   // macro 1-7为hccs
#define DCMI_HCCS_310P_PCS_NUM 0   // 310P HPCS数量为1
#define LANE_MODE_OFFSET 9
#define LINK_LANE_OFFSET 1
#define FIRST_ERROR_LANE 8
#define LANE_MODE_BIT_NUM 4
#define NET_MACRO_ID_MAX 0
#define MACRO_LANE_MAX_NUM 4 // macro上有4个lane

#define DCMI_HCCS_910_93_MACRO_NUM1 7    // macro (天成 A+K A+X)M1-M7用作hccs
#define DCMI_HCCS_910_93_MACRO_NUM2 6    // macro (天工)M2-M7用作hccs
#define DCMI_HCCS_910_93_MACRO_OFFSET1 1 // 910A3 M1-M7用作hccs
#define DCMI_HCCS_910_93_MACRO_OFFSET2 2 // 910A3 M2-M7用作hccs

struct dcmi_hccs_lane_info {
    unsigned int hccs_port_pcs_bitmap;
    unsigned int pcs_lane_bitmap[DCMI_HCCS_MAX_PCS_NUM];
    unsigned int reserve[16]; // 预留字段
};

struct dcmi_hccs_statistic_info {
    unsigned int tx_cnt[DCMI_HCCS_MAX_PCS_NUM];
    unsigned int rx_cnt[DCMI_HCCS_MAX_PCS_NUM];
    unsigned int crc_err_cnt[DCMI_HCCS_MAX_PCS_NUM];
    unsigned int retry_cnt[DCMI_HCCS_MAX_PCS_NUM];
    unsigned int reserved_field_cnt[DCMI_HCCS_RES_FIELD_NUM]; // 预留64个字段供后续扩充使用
};

struct dcmi_hccs_statistic_info_u64 {
    unsigned long long tx_cnt[DCMI_HCCS_MAX_PCS_NUM];
    unsigned long long rx_cnt[DCMI_HCCS_MAX_PCS_NUM];
    unsigned long long crc_err_cnt[DCMI_HCCS_MAX_PCS_NUM];
    unsigned long long retry_cnt[DCMI_HCCS_MAX_PCS_NUM];
    unsigned long long reserved[DCMI_HCCS_RES_FIELD_NUM]; // 预留64个字段供后续扩充使用
};

#define DCMI_HCCS_STATUS_RESERVED_LEN 4
#define DCMI_HDLC_STATUS_OK 0x3
struct dcmi_hccs_statues {
    unsigned int pcs_status;
    unsigned int hdlc_status;
    unsigned char reserve[DCMI_HCCS_STATUS_RESERVED_LEN];
};

#define DCMI_UB_PORT_NUM 36 // NPU芯片端口数量
typedef enum {
    DCMI_UB_ALL_PORT_NO_LINK = 0, // 所有端口未建链
    DCMI_UB_ALL_PORT_LINK,        // 所有端口建链
    DCMI_UB_PARTIAL_PORT_LINK,    // 部分端口建链
    DCMI_UB_NO_NEED_LINK,         // 断链标记 无需建链
} dcmi_entire_ub_status;          /* 芯片维度： 0-2: actual link status, 3: link requirement */

typedef enum {
    DCMI_UB_PORT_STATUS_NONE_LANE = 0, // 芯片端口不存在
    DCMI_UB_PORT_STATUS_FULL_LANE,     // 端口建链 全带宽
    DCMI_UB_PORT_STATUS_PARTIAL_LANE,  // 端口建链 部分带宽
    DCMI_UB_PORT_STATUS_INITIAL,       // 初始化状态 未建链
} dcmi_ub_port_status;                 /* 芯片端口维度 */

struct dcmi_ub_port_link_status {
    dcmi_entire_ub_status ub_link_status;
    dcmi_ub_port_status ub_port_status[DCMI_UB_PORT_NUM];
};

/* DCMI sub commond for SIO */
typedef enum {
    DCMI_SIO_SUB_CMD_CRC_ERR_STATISTICS = 0,
} DCMI_SIO_SUB_CMD;

struct dcmi_sio_crc_err_statistics_info {
    unsigned short tx_error_count;
    unsigned short rx_error_count;
    unsigned char reserved[8]; // 预留
};

/* DCMI sub command for safetyisland module */
typedef enum {
    DCMI_SILS_SUB_CMD_PMUWDG_DISABLE = 0,
    DCMI_SILS_SUB_CMD_PMUWDG_ENABLE = 1,
    DCMI_SILS_SUB_CMD_PMUWDG_STATUS = 2,
    DCMI_SILS_SUB_CMD_INVALID = 0xFF,
} DCMI_SILS_SUB_CMD;

/* DCMI sub commond for DCMI_MAIN_CMD_SOC_INFO */
typedef enum {
    DCMI_SOC_INFO_SUB_CMD_DOMAIN_INFO = 0,
    DCMI_SOC_INFO_SUB_CMD_CUSTOM_OP = 1,
    DCMI_SOC_INFO_SUB_CMD_MAX,
} DCMI_SOC_INFO_SUB_CMD;

#define DCMI_SOC_INFO_RESERVED_LEN 8

struct dcmi_domain_info {
    int ai_cpu_num;
    int ctrl_cpu_num;
    int data_cpu_num;
    int ai_core_num;
    int vector_core_num;
    int reserve[DCMI_SOC_INFO_RESERVED_LEN];
};

/* DCMI sub QOS info def */
typedef enum {
    DCMI_QOS_SUB_MATA_CONFIG,
    DCMI_QOS_SUB_MASTER_CONFIG,
    DCMI_QOS_SUB_BW_DATA,
    DCMI_QOS_SUB_GLOBAL_CONFIG,
    DCMI_QOS_SUB_CONFIG_DONE,
} DCMI_QOS_SUB_INFO;

#define DCMI_QOS_TARGET_NUM_MAX 16U
#define DCMI_QOS_CFG_RESERVED_LEN 8
#define DCMI_QOS_INDEX_OFFSET 8
#define DCMI_QOS_GROUP_RESERVED_LEN 7 /* keep both sides of union 32 bytes, 对齐 dsmi qos_master_config */
#define DCMI_QOS_SUB_CMD_MAKE(qos_index, qos_sub_cmd) (((qos_index) << DCMI_QOS_INDEX_OFFSET) | (qos_sub_cmd))

struct dcmi_qos_bw_config {
    unsigned char mode;
    unsigned char state;
    unsigned char cnt;
    unsigned char method;
    unsigned int interval;
    unsigned int target_set[DCMI_QOS_TARGET_NUM_MAX];
    int reserved_1[DCMI_QOS_CFG_RESERVED_LEN];
};

struct dcmi_qos_bw_result {
    int mpamid;
    unsigned int curr;
    unsigned int bw_max;
    unsigned int bw_min;
    unsigned int bw_mean;
    int reserved[DCMI_QOS_CFG_RESERVED_LEN];
};

struct dcmi_qos_mata_config {
    int mpamid;
    unsigned int bw_high;
    unsigned int bw_low;
    int hardlimit;
    int reserved[DCMI_QOS_CFG_RESERVED_LEN];
};

struct dcmi_qos_master_config {
    int master;
    int mpamid;
    int qos;
    int pmg;
    union {
        unsigned long long bitmap[4]; /* mode 0/2: max support 64 * 4 */
        struct {
            unsigned int master_group_id; /* mode 3: compute group ID, 对齐 dsmi qos_master_config */
            unsigned int group_reserved[DCMI_QOS_GROUP_RESERVED_LEN];
        } group;
    };
    unsigned int mode; /* 0--regs valid, 2--sqe valid, 3--group id valid */
    int reserved[DCMI_QOS_CFG_RESERVED_LEN - 1];
};

struct dcmi_qos_gbl_config {
    unsigned int enable;
    unsigned int autoqos_fuse_en;   /* 0--enable, 1--disable */
    unsigned int mpamqos_fuse_mode; /* 0--average, 1--max, 2--replace */
    unsigned int mpam_subtype;      /* 0--all, 1--wr, 2--rd, 3--none */
    int reserved[DCMI_QOS_CFG_RESERVED_LEN];
};

/* DCMI sub CHIP info CMD def */
typedef enum {
    DCMI_CHIP_INF_SUB_CMD_CHIP_ID,
    DCMI_CHIP_INF_SUB_CMD_SPOD_INFO,
    DCMI_CHIP_INF_SUB_CMD_SPOD_NODE_STATUS,
    DCMI_CHIP_INF_SUB_CMD_MAX = 0xFF,
} DCMI_CHIP_INFO_SUB_CMD;

struct dcmi_spod_info {
    unsigned int sdid;
    unsigned int super_pod_size;
    unsigned int super_pod_id;
    unsigned int server_index;
    unsigned int chassis_id;
    unsigned int super_pod_type;
    unsigned int reserve[6]; /* the size of dcmi_spod_info is 48 */
};

#define DCMI_UTIL_RESERVED_LEN 8
struct dcmi_multi_utilization_info {
    unsigned int aic_util;
    unsigned int aiv_util;
    unsigned int aicore_util;
    unsigned int npu_util;
    unsigned int reserved[DCMI_UTIL_RESERVED_LEN];
};

/* DCMI sub commond for TS  */
typedef enum {
    DCMI_TS_SUB_CMD_AICORE_UTILIZATION_RATE = 0, // Obtains the single-core usage of AI Core.
    DCMI_TS_SUB_CMD_VECTORCORE_UTILIZATION_RATE, // Obtains the single-core usage of Vector Core.
    DCMI_TS_SUB_CMD_FFTS_TYPE,                   // Obtains the type of FFTS or FFTS+
    DCMI_TS_SUB_CMD_SET_FAULT_MASK,
    DCMI_TS_SUB_CMD_GET_FAULT_MASK,
    DCMI_TS_SUB_CMD_LAUNCH_AICORE_STL,
    DCMI_TS_SUB_CMD_COMMON_MSG = 11,
    DCMI_TS_SUB_CMD_MAX,
} DCMI_TS_SUB_CMD;

/* DCMI sub command for RECOVERY module */
typedef enum {
    DCMI_RCVR_SUB_CMD_SET_FLAG = 0,
    DCMI_RCVR_SUB_CMD_GET_FLAG,
    DCMI_RCVR_SUB_CMD_CLEAN_FLAG,
    DCMI_RCVR_SUB_CMD_RESET_BOOT_CNT,
    DCMI_RCVR_SUB_CMD_SET_STATUS,
} DCMI_RECOVERY_SUB_CMD;

/* DCMI sub commond for memory */
#define DCMI_SUB_CMD_MEMORY_TYPE 0
#define DCMI_SUB_CMD_MEMORY_CAPACITY 1
#define DCMI_SUB_CMD_MEMORY_CHANNEL 2
#define DCMI_SUB_CMD_MEMORY_RANK_NUM 3
#define DCMI_SUB_CMD_MEMORY_ECC_ENABLE 4
#define DCMI_SUB_CMD_MEMORY_MANUFACTURES 5
#define DCMI_SUB_CMD_MEMORY_SET_MATA_READ_ONCE 30
#define DCMI_SUB_CMD_MEMORY_GET_MATA_READ_ONCE 31

/* DCMI sub commond for os power module */
#define DCMI_OS_TYPE_OFFSET 24
#define DCMI_OS_TYPE_CFG_BIT 0xff000000
#define DCMI_POWER_TYPE_CFG_BIT 0x00ffffff
#define DCMI_OS_SUB_CMD_MAKE(os_type, power_type) (((os_type) << DCMI_OS_TYPE_OFFSET) | (power_type))

typedef enum {
    DCMI_SUB_OS_SD = 0,     // SD域
    DCMI_SUB_OS_ALL = 0xFE, // 所有域
    DCMI_SUB_OS_INVALID = 0xFF,
} DCMI_SUB_OS_TYPE;

/* DCMI sub command for UFS module */
typedef enum {
    DCMI_UFS_SUB_CMD_CONFIG = 0,
    DCMI_UFS_SUB_CMD_STATUS = 1,
    DCMI_UFS_SUB_CMD_INLINE_CRYPTO = 2,
    DCMI_UFS_SUB_CMD_CREATE_KEY = 3,
    DCMI_UFS_SUB_CMD_DELETE_KEY = 4,
    DCMI_UFS_SUB_CMD_QUERY_DESCRIPTOR = 5,
    DCMI_UFS_SUB_CMD_INVALID = 0xFF,
} DCMI_UFS_SUB_CMD;

#define DCMI_INLINE_CRYPTO_RESERVED_LEN 4
#define DCMI_UFS_DEFAULT_KEY_NUM 8
#define DCMI_UFS_MAX_MN_LEN 18  /**< ufs max manufacturer name length */
#define DCMI_UFS_MAX_SN_LEN 254 /**< ufs max serial number length */
#define DCMI_UFS_MAX_PI_LEN 34  /**< ufs max product identification */
#define DCMI_UFS_DESC_LEN 255

typedef enum {
    DCMI_UFS_FAST_MODE = 1,     // 高速模式
    DCMI_UFS_SLOW_MODE = 2,     // 低速模式
    DCMI_UFS_FASTAUTO_MODE = 4, // 自动高速模式
    DCMI_UFS_SLOWAUTO_MODE = 5, // 自动低速模式
    DCMI_UFS_UNCHANGED = 7,     // 不做改变
} DCMI_UFS_PWR_MODE;            // UFS速率模式

typedef enum {
    DCMI_UFS_DONT_CHANGE, // 不做改变
    DCMI_UFS_GEAR_1,      // 速率等级1
    DCMI_UFS_GEAR_2,      // 速率等级2
    DCMI_UFS_GEAR_3,      // 速率等级3
} DCMI_UFS_GEAR;          // 高速模式下的速率等级

typedef enum {
    DCMI_UFS_PA_HS_MODE_A = 1, // 频率模式A
    DCMI_UFS_PA_HS_MODE_B = 2, // 频率模式B
} DCMI_UFS_HS_MODE;            // 高速模式下的频率模式

typedef enum {
    DCMI_UFS_PM_LEVEL_0, /**< UFS_ACTIVE_PWR_MODE, UIC_LINK_ACTIVE_STATE */
    DCMI_UFS_PM_LEVEL_1, /**< UFS_ACTIVE_PWR_MODE, UIC_LINK_HIBERN8_STATE */
    DCMI_UFS_PM_LEVEL_2, /**< UFS_SLEEP_PWR_MODE, UIC_LINK_ACTIVE_STATE */
    DCMI_UFS_PM_LEVEL_3, /**< UFS_SLEEP_PWR_MODE, UIC_LINK_HIBERN8_STATE */
    DCMI_UFS_PM_LEVEL_4, /**< UFS_POWERDOWN_PWR_MODE, UIC_LINK_HIBERN8_STATE */
    DCMI_UFS_PM_LEVEL_5, /**< UFS_POWERDOWN_PWR_MODE, UIC_LINK_OFF_STATE */
    DCMI_UFS_PM_LEVEL_MAX,
} DCMI_UFS_PM_LEVEL;

typedef enum {
    DCMI_UFS_DEV_CLK_19M2 = 0,
    DCMI_UFS_DEV_CLK_26M0,
    DCMI_UFS_DEV_CLK_38M4,
    DCMI_UFS_DEV_CLK_52M0,
    DCMI_UFS_DEV_CLK_INVAL,
} DCMI_UFS_DEV_CLOCK; // 查询设备时钟频率

typedef struct dcmi_ufs_config_stru {
    DCMI_UFS_PWR_MODE pwr_mode;          /**< Link Rate Mode */
    DCMI_UFS_GEAR pwr_gear;              /**< Link Rate */
    DCMI_UFS_HS_MODE hs_series;          /**< HS Series, Only query, not configuration */
    DCMI_UFS_PM_LEVEL suspend_pwr_level; /**< HS Series, Only query, not configuration */
    unsigned int auto_h8;                /**< enable autoH8: 0-disable, 1-enable */
    unsigned int lane_count;             /**< active lanes count */
    DCMI_UFS_DEV_CLOCK device_refclk;    /**< Reference Clock Frequency value, Only query, not configuration */
} DCMI_UFS_CONFIG_STRU;

typedef struct dcmi_ufs_partition_stru {
    unsigned int lun;
    unsigned int lba_start;
    unsigned int lba_end;
    unsigned int crypto_key_index;
    unsigned int reserved[DCMI_INLINE_CRYPTO_RESERVED_LEN];
} DCMI_UFS_PARTITION_STRU;

typedef struct dcmi_ufs_inline_crypto_stru {
    unsigned int enable;
    DCMI_UFS_PARTITION_STRU partition[DCMI_UFS_DEFAULT_KEY_NUM];
    unsigned int partition_cnt;
} DCMI_UFS_INLINE_CRYPTO_STRU;

typedef struct dcmi_ufs_key_index_stru {
    unsigned int key_index;
    unsigned int reserved[DCMI_INLINE_CRYPTO_RESERVED_LEN];
} DCMI_UFS_KEY_INDEX_STRU;

typedef enum {
    DCMI_UFS_STATE_LINKOFF, // 断开连接状态
    DCMI_UFS_STATE_ACTIVE,  // 正常状态
    DCMI_UFS_STATE_HIBERN8, // 低功耗状态
} DCMI_UFS_STATE;           // UFS状态

typedef enum {
    DCMI_UFS_UIC_LINK_OFF_STATE = 0,     /**< Link powered down or disabled */
    DCMI_UFS_UIC_LINK_ACTIVE_STATE = 1,  /**< Link is in Fast/Slow/Sleep state */
    DCMI_UFS_UIC_LINK_HIBERN8_STATE = 2, /**< Link is in Hibernate state */
} DCMI_UFS_LINK_STATE;

typedef enum {
    DCMI_UFS_DEV_PWR_ACTIVE = 1,    // 正常工作状态
    DCMI_UFS_DEV_PWR_SLEEP = 2,     // 休眠状态
    DCMI_UFS_DEV_PWR_POWERDOWN = 3, // 关机状态
} DCMI_UFS_DEV_PWR_STATE;           // UFS电源状态

typedef struct dcmi_ufs_descriptor_stru {
    unsigned char automotive_health[DCMI_UFS_DESC_LEN]; // 自定义描述符，部分厂家支持
    unsigned char device_health[DCMI_UFS_DESC_LEN];
    unsigned char device[DCMI_UFS_DESC_LEN];
    unsigned char osv_report[DCMI_UFS_DESC_LEN];       // 自定义描述符，部分厂家支持
    unsigned char nand_info_report[DCMI_UFS_DESC_LEN]; // 自定义描述符，部分厂家支持
    unsigned char reserved[DCMI_UFS_DESC_LEN];
} DCMI_UFS_DESCRIPTOR_STRU;

typedef struct dcmi_ufs_status_stru {
    DCMI_UFS_STATE status;                    /**< ufs status */
    DCMI_UFS_PWR_MODE rx_pwr_mode;            /**< rx rate mode */
    DCMI_UFS_PWR_MODE tx_pwr_mode;            /**< tx rate mode */
    DCMI_UFS_GEAR rx_pwr_gear;                /**< rx rate */
    DCMI_UFS_GEAR tx_pwr_gear;                /**< tx rate */
    unsigned int rx_lanes;                    /**< rx lanes */
    unsigned int tx_lanes;                    /**< tx lanes */
    DCMI_UFS_LINK_STATE link_pwr_status;      /**< link power status */
    DCMI_UFS_DEV_PWR_STATE device_pwr_status; /**< device power status */
    int temperature;                          /**< ufs device temperature */
    unsigned int fault_status;                /**< ufs device exception status */
    unsigned int total_capacity;              /**< total raw device capacity */
    unsigned int model_number;                /**< ufs device sub class */
    unsigned int device_life_time;            /**< ufs device life time used */
    unsigned int fw_ver;                      /**< product revision level */
    unsigned int fw_update_enable;            /**< whether to support firmware update: 0-not support, 1-support */
    unsigned char product_name[DCMI_UFS_MAX_PI_LEN];      /**< ufs device product identification */
    unsigned char manufacturer_name[DCMI_UFS_MAX_MN_LEN]; /** <ufs device manufacturer name */
    unsigned char serial_number[DCMI_UFS_MAX_SN_LEN];     /** <ufs device serial number */
    unsigned int spec_version;                            /**< ufs device specification version */
    unsigned int device_version;                          /**< ufs device device version */
} DCMI_UFS_STATUS_STRU;

/* DCMI sub commond for DCMI_MAIN_CMD_EX_COMPUTING */
#define DCMI_EX_COMPUTING_SUB_CMD_TOKEN 0

/* DCMI sub commond for DCMI_MAIN_CMD_SEC */
#define DCMI_SEC_SUB_CMD_PSS 0

/* DCMI sub command for DCMI_MAIN_CMD_CHIP_INF */
#define DCMI_CHIP_INF_SUB_CMD_CUST_BOARD_INF 3

#define DCMI_SRIOV_DISABLE 0
#define DCMI_SRIOV_ENABLE 1

/* DCMI sub vdev mng CMD def */
typedef enum {
    DCMI_VMNG_SUB_CMD_GET_VDEV_RESOURCE,
    DCMI_VMNG_SUB_CMD_GET_TOTAL_RESOURCE,
    DCMI_VMNG_SUB_CMD_GET_FREE_RESOURCE,
    DCMI_VMNG_SUB_CMD_SET_SRIOV_SWITCH,
    DCMI_VMNG_SUB_CMD_GET_VDEV_ACTIVITY = 5,
    DCMI_VMNG_SUB_CMD_MAX,
} DCMI_VDEV_MNG_SUB_CMD;

/* DCMI sub commond for Low power */
typedef enum {
    DCMI_LP_SUB_CMD_AICORE_VOLTAGE_CURRENT = 0, // 获取AICORE电压电流的寄存器值
    DCMI_LP_SUB_CMD_HYBIRD_VOLTAGE_CURRENT,     // 获取HYBIRD电压电流的寄存器值
    DCMI_LP_SUB_CMD_TAISHAN_VOLTAGE_CURRENT,    // 获取CPU电压电流的寄存器值
    DCMI_LP_SUB_CMD_DDR_VOLTAGE_CURRENT,        // 获取DDR电压电流的寄存器值
    DCMI_LP_SUB_CMD_ACG,                        // 获取ACG调频计数值
    DCMI_LP_SUB_CMD_STATUS,                     // 获取低功耗总状态
    DCMI_LP_SUB_CMD_TOPS_DETAILS,               // 获取所有工作档位
    DCMI_LP_SUB_CMD_SET_WORK_TOPS,              // 设置工作档位
    DCMI_LP_SUB_CMD_GET_WORK_TOPS,              // 获取当前工作档位
    DCMI_LP_SUB_CMD_AICORE_FREQREDUC_CAUSE,     // 获取当前降频原因
    DCMI_LP_SUB_CMD_GET_POWER_INFO,             // 获取功耗信息
    DCMI_LP_SUB_CMD_SET_IDLE_SWITCH,            // 设置IDLE模式开关
    DCMI_LP_SUB_CMD_MAX,
} DCMI_LP_SUB_CMD;

/* DCMI sub tls cert CMD def */
typedef enum {
    DCMI_CERT_SUB_CMD_INIT_TLS_PUB_KEY = 0,
    DCMI_CERT_SUB_CMD_INIT_RESERVE,
    DCMI_CERT_SUB_CMD_TLS_CERT_INFO,
    DCMI_CERT_SUB_CMD_MAX,
} DCMI_EX_CERT_SUB_CMD;

#define DCMI_LP_WORK_TOPS_MAX 10
#define DCMI_LP_WORK_TOPS_RESERVE 32
#define DCMI_LP_POWER_RESERVED_LEN 32

typedef struct dcmi_lp_cur_tops_stru {
    unsigned int work_tops;
    unsigned int tops_nums;
} DCMI_LP_CUR_TOPS_STRU;

typedef struct dcmi_lp_work_tops_stru {
    unsigned int work_tops;
    unsigned int is_in_flash;
    unsigned char reserve[DCMI_LP_WORK_TOPS_RESERVE];
} DCMI_LP_WORK_TOPS_STRU;

struct dcmi_lp_each_tops_details {
    unsigned int work_tops; /* it is a index for aic_tops */
    unsigned int aic_tops;  /* just as 4T/8T/8Tx/16T */
    unsigned int aic_freq;  /* AI core frequence */
    unsigned int aic_vol;   /* AI core voltage */
    unsigned int cpu_freq;  /* CPU frequence */
    unsigned int cpu_vol;   /* CPU voltage */
    unsigned char reserve[DCMI_LP_WORK_TOPS_RESERVE];
};

typedef struct dcmi_lp_tops_details_stru {
    unsigned int tops_nums;
    struct dcmi_lp_each_tops_details each_work_tops[DCMI_LP_WORK_TOPS_MAX];
} DCMI_LP_TOPS_DETAILS_STRU;

typedef struct dcmi_lp_power_info {
    unsigned int soc_rated_power;
    unsigned char reserved[DCMI_LP_POWER_RESERVED_LEN];
} DCMI_LP_POWER_INFO_STRU;

typedef struct dcmi_aicfreq_tops_stru {
    unsigned int aic_freq;
    const char *tops;
} DCMI_AICFRQ_TOPS_STRU;

enum dcmi_component_type {
    DCMI_COMPONENT_TYPE_NVE,
    DCMI_COMPONENT_TYPE_XLOADER,
    DCMI_COMPONENT_TYPE_M3FW,
    DCMI_COMPONENT_TYPE_UEFI,
    DCMI_COMPONENT_TYPE_TEE,
    DCMI_COMPONENT_TYPE_KERNEL,
    DCMI_COMPONENT_TYPE_DTB,
    DCMI_COMPONENT_TYPE_ROOTFS,
    DCMI_COMPONENT_TYPE_IMU,
    DCMI_COMPONENT_TYPE_IMP,
    DCMI_COMPONENT_TYPE_AICPU,
    DCMI_COMPONENT_TYPE_HBOOT1_A,
    DCMI_COMPONENT_TYPE_HBOOT1_B,
    DCMI_COMPONENT_TYPE_HBOOT2,
    DCMI_COMPONENT_TYPE_DDR,
    DCMI_COMPONENT_TYPE_LP,
    DCMI_COMPONENT_TYPE_HSM,
    DCMI_COMPONENT_TYPE_SAFETY_ISLAND,
    DCMI_COMPONENT_TYPE_HILINK,
    DCMI_COMPONENT_TYPE_RAWDATA,
    DCMI_COMPONENT_TYPE_SYSDRV,
    DCMI_COMPONENT_TYPE_ADSAPP,
    DCMI_COMPONENT_TYPE_COMISOLATOR,
    DCMI_COMPONENT_TYPE_CLUSTER,
    DCMI_COMPONENT_TYPE_CUSTOMIZED,
    DCMI_COMPONENT_TYPE_SYS_BASE_CONFIG,
    DCMI_COMPONENT_TYPE_RECOVERY,
    DCMI_COMPONENT_TYPE_HILINK2,
    DCMI_COMPONENT_TYPE_LOGIC_BIST,
    DCMI_COMPONENT_TYPE_MEMORY_BIST,
    DCMI_COMPONENT_TYPE_ATF,
    DCMI_COMPONENT_TYPE_USER_BASE_CONFIG,
    DCMI_COMPONENT_TYPE_BOOTROM,
    DCMI_COMPONENT_TYPE_MAX,
    DCMI_UPGRADE_AND_RESET_ALL_COMPONENT = 0xFFFFFFF7,
    DCMI_UPGRADE_ALL_IMAGE_COMPONENT = 0xFFFFFFFD,
    DCMI_UPGRADE_ALL_FIRMWARE_COMPONENT = 0xFFFFFFFE,
    DCMI_UPGRADE_ALL_COMPONENT = 0xFFFFFFFF
};

enum dcmi_upgrade_state {
    DCMI_UPGRADE_IDLE = 0,
    DCMI_UPGRADE_UPGRADING = 1,
    DCMI_UPGRADE_NOT_SUPPORT = 2,
    DCMI_UPGRADE_UPGRADE_FAIL = 3,
    DCMI_UPGRADE_NOT_NEED = 4,
    DCMI_UPGRADE_NEED_VALIDATE = 5,
    DCMI_UPGRADE_STATE_NONE
};

enum dcmi_upgrade_type {
    MCU_UPGRADE_START = 1,
    MCU_UPGRADE_VALIDETE = 3,
    VRD_UPGRADE_START = 11,
    MCU_UPGRADE_NONE
};

enum dcmi_boot_status {
    DCMI_BOOT_STATUS_UNINIT = 0,
    DCMI_BOOT_STATUS_BIOS,
    DCMI_BOOT_STATUS_OS,
    DCMI_BOOT_STATUS_FINISH,
    DCMI_SYSTEM_START_FINISH = 16
};

enum dcmi_device_type {
    DCMI_DEVICE_TYPE_DDR,
    DCMI_DEVICE_TYPE_SRAM,
    DCMI_DEVICE_TYPE_HBM,
    DCMI_DEVICE_TYPE_NPU,
    DCMI_HBM_RECORDED_SINGLE_ADDR,
    DCMI_HBM_RECORDED_MULTI_ADDR,
    DCMI_DEVICE_TYPE_NONE = 0xff
};

enum dcmi_freq_type {
    DCMI_FREQ_DDR = 1,
    DCMI_FREQ_CTRLCPU = 2,
    DCMI_FREQ_HBM = 6,
    DCMI_FREQ_AICORE_CURRENT_ = 7,
    DCMI_FREQ_AICORE_MAX = 9,
    DCMI_FREQ_VECTORCORE_CURRENT = 12
};

#define DCMI_UTILIZATION_RATE_DDR 1
#define DCMI_UTILIZATION_RATE_AICORE 2
#define DCMI_UTILIZATION_RATE_AICPU 3
#define DCMI_UTILIZATION_RATE_CTRLCPU 4
#define DCMI_UTILIZATION_RATE_DDR_BANDWIDTH 5
#define DCMI_UTILIZATION_RATE_HBM 6
#define DCMI_UTILIZATION_RATE_HBM_BANDWIDTH 10
#define DCMI_UTILIZATION_RATE_VECTORCORE 12
#define DCMI_UTILIZATION_RATE_NPU 13
#define DCMI_UTILIZATION_RATE_AICUBE 14

enum dcmi_manager_sensor_id {
    DCMI_CLUSTER_TEMP_ID = 0,
    DCMI_PERI_TEMP_ID = 1,
    DCMI_AICORE0_TEMP_ID,
    DCMI_AICORE1_TEMP_ID,
    DCMI_AICORE_LIMIT_ID,
    DCMI_AICORE_TOTAL_PER_ID,
    DCMI_AICORE_ELIM_PER_ID,
    DCMI_AICORE_BASE_FREQ_ID,
    DCMI_NPU_DDR_FREQ_ID,
    DCMI_THERMAL_THRESHOLD_ID,
    DCMI_NTC_TEMP_ID,
    DCMI_SOC_TEMP_ID,
    DCMI_FP_TEMP_ID,
    DCMI_N_DIE_TEMP_ID,
    DCMI_HBM_TEMP_ID,
    DCMI_SENSOR_INVALID_ID = 255
};

union dcmi_sensor_info {
    unsigned char uchar;
    unsigned short ushort;
    unsigned int uint;
    signed int iint;
    signed char temp[DCMI_SENSOR_TEMP_LEN];
    signed int ntc_tmp[DCMI_SENSOR_NTC_TEMP_LEN];
    unsigned int data[DCMI_SENSOR_DATA_MAX_LEN];
};

/*----------------------------------------------*
 *         dcmi_get_fault_event                 *
 *----------------------------------------------*/
#define DCMI_EVENT_FILTER_FLAG_EVENT_ID (1UL << 0)
#define DCMI_EVENT_FILTER_FLAG_SERVERITY (1UL << 1)
#define DCMI_EVENT_FILTER_FLAG_NODE_TYPE (1UL << 2)

#define DCMI_MAX_EVENT_NAME_LENGTH 256
#define DCMI_MAX_EVENT_DATA_LENGTH 32
#define DCMI_MAX_EVENT_RESV_LENGTH 32

struct dcmi_event_filter {
    unsigned long long filter_flag;
    unsigned int event_id;
    unsigned char severity;
    unsigned char node_type;
    unsigned char resv[DCMI_MAX_EVENT_RESV_LENGTH]; /**< reserve 32byte */
};

struct dcmi_dms_fault_event {
    unsigned int event_id;
    unsigned short deviceid;
    unsigned char node_type;
    unsigned char node_id;
    unsigned char sub_node_type;
    unsigned char sub_node_id;
    unsigned char severity;
    unsigned char assertion;
    int event_serial_num;
    int notify_serial_num;
    unsigned long long alarm_raised_time;
    char event_name[DCMI_MAX_EVENT_NAME_LENGTH];
    char additional_info[DCMI_MAX_EVENT_DATA_LENGTH];
    unsigned char os_id;
    unsigned char resv_1[1]; /* reserve 1 byte for byte alignment */
    unsigned short node_type_ex;
    unsigned short sub_node_type_ex;
    unsigned char resv_2[2]; /* reserve 2 byte for byte alignment */

    /* os_id occupy 1byte, node_type_ex,sub_node_type_ex occupy 4byte, reserve 24byte */
    unsigned char resv[DCMI_MAX_EVENT_RESV_LENGTH - 8];
};

enum dcmi_event_type {
    DCMI_DMS_FAULT_EVENT = 0,
    DCMI_EVENT_TYPE_MAX
};

struct dcmi_event {
    enum dcmi_event_type type;
    union {
        struct dcmi_dms_fault_event dms_event;
    } event_t;
};

typedef void (*dcmi_fault_event_callback)(struct dcmi_event *event);

#define DCMI_VDEV_RES_NAME_LEN 16
#define DCMI_VDEV_FOR_RESERVE 32
#define DCMI_SOC_SPLIT_MAX 32
#define DCMI_ULONG_LONG_SIZE sizeof(unsigned long long)
#define DCMI_UINT_SIZE sizeof(unsigned int)
#define DCMI_VDEV_SIZE ((2 * DCMI_ULONG_LONG_SIZE) + DCMI_UINT_SIZE)

/* Tls Cert Mananger */
#define CERT_NAME_MAX_LEN 16
#define CERT_CN_NAME_LEN 2
#define TIME_LEN 32
#define CERT_COMMON_NAME_LEN 64
#define MAX_CERT_COUNT (15U)
#define NPU_CERT_MAX_SIZE 2048
#define DCMI_CERT_SUB_CMD_INIT_TLS_PUB_KEY 0
#define DCMI_CERT_SUB_CMD_TLS_CERT_INFO 2
#define CERT_PERIOD_MIN 7
#define CERT_PERIOD_MAX 180

#define CERT_CONFIG_EXPIRED "cert_expired_threshold"
#define DEFAULT_EXPIERD_THRESHOLD 90

struct dcmi_csr_info {
    char country_name[CERT_NAME_MAX_LEN];
    char province_name[CERT_NAME_MAX_LEN];
    char city_name[CERT_NAME_MAX_LEN];
    char organization_name[CERT_NAME_MAX_LEN];
    char department_name[CERT_NAME_MAX_LEN];
    char reserve_name[CERT_COMMON_NAME_LEN];
    int reserve;
    int csr_len;
    char csr_data[NPU_CERT_MAX_SIZE];
};

struct dcmi_cert_info {
    unsigned int alarm_stat;
    unsigned int reserve;
    char start_time[TIME_LEN];
    char end_time[TIME_LEN];
    char country_name[CERT_NAME_MAX_LEN];
    char province_name[CERT_NAME_MAX_LEN];
    char city_name[CERT_NAME_MAX_LEN];
    char organization_name[CERT_NAME_MAX_LEN];
    char department_name[CERT_NAME_MAX_LEN];
    char reserve_name[CERT_COMMON_NAME_LEN];
    char common_name[CERT_COMMON_NAME_LEN];
};

struct dcmi_certs_chain_data {
    unsigned int count;
    unsigned int data_len[MAX_CERT_COUNT];
    unsigned char data[0];
};

struct dcmi_base_resource {
    unsigned long long token;
    unsigned long long token_max;
    unsigned long long task_timeout;
    unsigned int vfg_id;
    unsigned char vip_mode;
    unsigned char reserved[DCMI_VDEV_FOR_RESERVE - 1]; /* bytes aligned */
};

/* total types of computing resource */
struct dcmi_computing_resource {
    /* accelerator resource */
    float aic;
    float aiv;
    unsigned short dsa;
    unsigned short rtsq;
    unsigned short acsq;
    unsigned short cdqm;
    unsigned short c_core;
    unsigned short ffts;
    unsigned short sdma;
    unsigned short pcie_dma;

    /* memory resource, MB as unit */
    unsigned long long memory_size;

    /* id resource */
    unsigned int event_id;
    unsigned int notify_id;
    unsigned int stream_id;
    unsigned int model_id;

    /* cpu resource */
    unsigned short topic_schedule_aicpu;
    unsigned short host_ctrl_cpu;
    unsigned short host_aicpu;
    unsigned short device_aicpu;
    unsigned short topic_ctrl_cpu_slot;

    /* vnpu resource */
    unsigned int vdev_aicore_utilization;
    unsigned long long vdev_memory_total;
    unsigned long long vdev_memory_free;

    /* vdev_memory_total and vdev_memory_free occupy 16 byte, vdev_aicore_utilization occupy 4 byte, reserve 12byte */
    unsigned char reserved[DCMI_VDEV_FOR_RESERVE - DCMI_VDEV_SIZE];
};

/* configurable computing resource */
struct dcmi_computing_configurable {
    /* memory resource, MB as unit */
    unsigned long long memory_size;

    /* accelator resource */
    float aic;
    float aiv;
    unsigned short dsa;
    unsigned short rtsq;
    unsigned short cdqm;

    /* cpu resource */
    unsigned short topic_schedule_aicpu;
    unsigned short host_ctrl_cpu;
    unsigned short host_aicpu;
    unsigned short device_aicpu;

    unsigned char reserved[DCMI_VDEV_FOR_RESERVE];
};
struct dcmi_media_resource {
    /* dvpp resource */
    float jpegd;
    float jpege;
    float vpc;
    float vdec;
    float pngd;
    float venc;
    unsigned char reserved[DCMI_VDEV_FOR_RESERVE];
};

struct dcmi_create_vdev_in {
    char name[DCMI_VDEV_RES_NAME_LEN];
    struct dcmi_base_resource base;
    struct dcmi_computing_configurable computing;
    struct dcmi_media_resource media;
};

struct dcmi_create_vdev_out {
    unsigned int vdev_id;
    unsigned int pcie_bus;
    unsigned int pcie_device;
    unsigned int pcie_func;
    unsigned int vfg_id;
    unsigned char reserved[DCMI_VDEV_FOR_RESERVE];
};

struct dcmi_create_vdev_res_stru {
    unsigned int vdev_id;
    unsigned int vfg_id;
    char template_name[32];
    unsigned char reserved[64];
};

struct dcmi_vdev_query_info {
    char name[DCMI_VDEV_RES_NAME_LEN];
    unsigned int status;
    unsigned int is_container_used;
    unsigned int vfid;
    unsigned int vfg_id;
    unsigned long long container_id;
    struct dcmi_base_resource base;
    struct dcmi_computing_resource computing;
    struct dcmi_media_resource media;
};

/* for single search */
struct dcmi_vdev_query_stru {
    unsigned int vdev_id;
    struct dcmi_vdev_query_info query_info;
};

struct dcmi_soc_free_resource {
    unsigned int vfg_num;
    unsigned int vfg_bitmap;
    struct dcmi_base_resource base;
    struct dcmi_computing_resource computing;
    struct dcmi_media_resource media;
};

struct dcmi_soc_total_resource {
    unsigned int vdev_num;
    unsigned int vdev_id[DCMI_SOC_SPLIT_MAX];
    unsigned int vfg_num;
    unsigned int vfg_bitmap;
    struct dcmi_base_resource base;
    struct dcmi_computing_resource computing;
    struct dcmi_media_resource media;
};

typedef enum {
    DCMI_POWER_STATE_SUSPEND,
    DCMI_POWER_STATE_POWEROFF,
    DCMI_POWER_STATE_RESET,
    DCMI_POWER_STATE_BIST,
    DCMI_POWER_STATE_MAX,
} DCMI_POWER_STATE;

typedef enum {
    DCMI_POWER_RESUME_MODE_BUTTON, /* resume by button */
    DCMI_POWER_RESUME_MODE_TIME,   /* resume by time */
    DCMI_POWER_RESUME_MODE_MAX,
} DCMI_LP_RESUME_MODE;

#define DCMI_POWER_INFO_RESERVE_LEN 8
struct dcmi_power_state_info_stru {
    DCMI_POWER_STATE type;
    DCMI_LP_RESUME_MODE mode;
    unsigned int value;
    unsigned int reserve[DCMI_POWER_INFO_RESERVE_LEN];
};

enum ECC_INFO_READ {
    MULTI_ECC_TIMES_READ = 0,
    SINGLE_ECC_INFO_READ,
    MULTI_ECC_INFO_READ,
    ECC_ADDRESS_COUNT_READ,
    ECC_MAX_READ_CMD
};

struct dcmi_ecc_record_type {
    enum ECC_INFO_READ read_type;
    enum dcmi_device_type module_type;
};

#pragma pack(1)
struct dcmi_multi_ecc_time_data {
    unsigned int multi_record_count;
    unsigned int multi_ecc_times[MAX_RECORD_ECC_ADDR_COUNT];
};

struct dcmi_ecc_common_data {
    unsigned long long physical_addr;
    unsigned int stack_pc_id; /* hbmc_id */
    unsigned int reg_addr_h;  /* row col */
    unsigned int reg_addr_l;  /* sid bank */
    unsigned int ecc_count;
    int timestamp;
};
#pragma pack()

#define AGENTDRV_PROF_DATA_NUM 3 /* pcie bandwidth MIN/MAX/AVG */
struct dcmi_pcie_link_bandwidth_info {
    int profiling_time;
    unsigned int tx_p_bw[AGENTDRV_PROF_DATA_NUM];
    unsigned int tx_np_bw[AGENTDRV_PROF_DATA_NUM];
    unsigned int tx_cpl_bw[AGENTDRV_PROF_DATA_NUM];
    unsigned int tx_np_lantency[AGENTDRV_PROF_DATA_NUM];

    unsigned int rx_p_bw[AGENTDRV_PROF_DATA_NUM];
    unsigned int rx_np_bw[AGENTDRV_PROF_DATA_NUM];
    unsigned int rx_cpl_bw[AGENTDRV_PROF_DATA_NUM];
};

struct dcmi_hccs_bandwidth_info {
    int profiling_time;
    double total_txbw;
    double total_rxbw;
    double tx_bandwidth[DCMI_HCCS_MAX_PCS_NUM];
    double rx_bandwidth[DCMI_HCCS_MAX_PCS_NUM];
};

#define UB_BANDWIDTH_RESERVED_LEN 2
struct dcmi_ub_bandwidth_info {
    unsigned int tx_bandwidth;
    unsigned int rx_bandwidth;
    unsigned int reserved[UB_BANDWIDTH_RESERVED_LEN];
};

struct dcmi_ub_port_info {
    int udie_id;
    int port_id;
};

#define UBOE_PKT_STAT_NUM 48

struct dcmi_port_pkt_stats_info {
    unsigned int port_id;
    unsigned int is_uboe_port;
    union {
        struct {
            unsigned long long ub_ipv4_pkt_cnt_rx;
            unsigned long long ub_ipv6_pkt_cnt_rx;
            unsigned long long unic_ipv4_pkt_cnt_rx;
            unsigned long long unic_ipv6_pkt_cnt_rx;
            unsigned long long ub_compact_pkt_cnt_rx;
            unsigned long long ub_umoc_ctph_cnt_rx;
            unsigned long long ub_umoc_ntph_cnt_rx;
            unsigned long long ub_mem_pkt_cnt_rx;
            unsigned long long unknown_pkt_cnt_rx;
            unsigned long long drop_ind_cnt_rx;
            unsigned long long err_ind_cnt_rx;
            unsigned long long to_host_pkt_cnt_rx;
            unsigned long long to_imp_pkt_cnt_rx;
            unsigned long long to_mar_pkt_cnt_rx;
            unsigned long long to_link_pkt_cnt_rx;
            unsigned long long to_noc_pkt_cnt_rx;
            unsigned long long route_err_cnt_rx;
            unsigned long long out_err_cnt_rx;
            unsigned long long length_err_cnt_rx;
            unsigned long long rx_busi_flit_num;
            unsigned long long rx_send_ack_flit;
            unsigned long long ub_ipv4_pkt_cnt_tx;
            unsigned long long ub_ipv6_pkt_cnt_tx;
            unsigned long long unic_ipv4_pkt_cnt_tx;
            unsigned long long unic_ipv6_pkt_cnt_tx;
            unsigned long long ub_compact_pkt_cnt_tx;
            unsigned long long ub_umoc_ctph_cnt_tx;
            unsigned long long ub_umoc_ntph_cnt_tx;
            unsigned long long ub_mem_pkt_cnt_tx;
            unsigned long long unknown_pkt_cnt_tx;
            unsigned long long drop_ind_cnt_tx;
            unsigned long long err_ind_cnt_tx;
            unsigned long long lpbk_ind_cnt_tx;
            unsigned long long out_err_cnt_tx;
            unsigned long long length_err_cnt_tx;
            unsigned long long tx_busi_flit_num;
            unsigned long long tx_recv_ack_flit;
            unsigned long long retry_req_sum;
            unsigned long long retry_ack_sum;
            unsigned long long crc_error_sum;
            union {
                struct {
                    unsigned long long core_mib_rxpausepkts;
                    unsigned long long core_mib_txpausepkts;
                    unsigned long long core_mib_rxpfcpkts;
                    unsigned long long core_mib_txpfcpkts;
                    unsigned long long core_mib_rxbadpkts;
                    unsigned long long core_mib_txbadpkts;
                    unsigned long long core_mib_rxbadoctets;
                    unsigned long long core_mib_txbadoctets;
                } uboe;
            };
        };
        unsigned long long pkt_num[UBOE_PKT_STAT_NUM];
    };
    unsigned char reserved[128]; /* 预留后续可能修改空间 */
};

#define VL_NUM_PER_PORT 16

struct dcmi_credit_info {
    unsigned int link_alloc_port_share_credit;
    unsigned int link_cur_used_port_share_credit;
    unsigned int link_alloc_vl_pri_credit[VL_NUM_PER_PORT];
    unsigned int link_cur_used_pri_credit[VL_NUM_PER_PORT];
};

#define NETDEV_MAX_NUM 8
#define NETDEV_NAME_MAX_LEN 16

struct dcmi_netdev_list_info {
    int netdev_nums;
    char netdev_name[NETDEV_MAX_NUM][NETDEV_NAME_MAX_LEN];
};

#define MAX_DIE_NUMS 2
#define MAX_PORT_NUMS 32
#define UB_MODE 0
#define UBOE_MODE 1

struct port_info {
    int port_index; // port标识
    int mode;       // 属性 UB_MODE/UBOE_MODE
};
struct die_port_list_info {
    int die_index;                                  // udie标识
    int port_nums;                                  // 该udie下可用port数量
    struct port_info port_list_data[MAX_PORT_NUMS]; // port信息
};
struct dcmi_port_list_info {
    int die_nums;                                          // 可用udie数量
    struct die_port_list_info die_list_data[MAX_DIE_NUMS]; // udie信息
};

#define SERDES_RESERVED_LEN 64
#define SERDES_INFO_NUM 8
struct dcmi_serdes_quality_base {
    unsigned int snr;
    unsigned int heh;
    signed int bottom;
    signed int top;
    signed int left;
    signed int right;
};

typedef struct dcmi_serdes_quality_info {
    unsigned int macro_id;
    unsigned int reserved1; /* reserve for byte alignment */
    struct dcmi_serdes_quality_base serdes_quality_info[SERDES_INFO_NUM];
    unsigned int reserved2[SERDES_RESERVED_LEN];
} DCMI_SERDES_QUALITY_INFO;

#define SERDES_FULL_EYE_INFO_NUM 256
#define SERDES_FULL_EYE_RESERVED_LEN 8
struct dcmi_serdes_full_eye_base {
    int offset;
    int eye_diagram_0;
    int eye_diagram_1;
};

typedef struct dcmi_serdes_full_eye {
    unsigned int macro_id;
    unsigned int lane_id;
    unsigned int reserved[SERDES_FULL_EYE_RESERVED_LEN];
    unsigned int info_size;
    struct dcmi_serdes_full_eye_base serdes_full_eye[SERDES_FULL_EYE_INFO_NUM];
} DCMI_SERDES_FULL_EYE;

enum {
    DCMI_TOPO_TYPE_SELF = 0, /* 芯片本身 */
    DCMI_TOPO_TYPE_SYS,      /* 通过PCIe连接且穿过NUMA nodes， nodes之间通过SMP连接，如：QPI、 UPI */
    DCMI_TOPO_TYPE_PHB,      /* 通过PCIe连接且穿过同一个CPU的 PCIe host bridge */
    DCMI_TOPO_TYPE_HCCS,     /* 通过HCCS链接 */
    DCMI_TOPO_TYPE_PXB,      /* 通过PCIe连接穿过多个PCIe switch */
    DCMI_TOPO_TYPE_PIX,      /* 通过PCIe连接穿过一个PCIe switch */
    DCMI_TOPO_TYPE_SIO,      /* 通过SIO总线连接 */
    DCMI_TOPO_TYPE_HCCS_SW,  /* 通过交换的HCCS链接 */
    DCMI_TOPO_TYPE_UB,       /* 通过UB总线连接 */
    DCMI_TOPO_TYPE_BUTT,     /* 未知关系 */
    DCMI_TOPO_TYOE_MAX,
};

struct dcmi_network_rdma_bandwidth_info {
    unsigned int tx_bandwidth;
    unsigned int rx_bandwidth;
};

#define TC_MAX_NUM 8

struct dcmi_tc_stat_data {
    unsigned long long tc_tx[TC_MAX_NUM];
    unsigned long long tc_rx[TC_MAX_NUM];
    unsigned long long reserved[TC_MAX_NUM];
};

struct dcmi_network_pkt_stats_info {
    unsigned long long mac_tx_mac_pause_num;
    unsigned long long mac_rx_mac_pause_num;
    unsigned long long mac_tx_pfc_pkt_num;
    unsigned long long mac_tx_pfc_pri0_pkt_num;
    unsigned long long mac_tx_pfc_pri1_pkt_num;
    unsigned long long mac_tx_pfc_pri2_pkt_num;
    unsigned long long mac_tx_pfc_pri3_pkt_num;
    unsigned long long mac_tx_pfc_pri4_pkt_num;
    unsigned long long mac_tx_pfc_pri5_pkt_num;
    unsigned long long mac_tx_pfc_pri6_pkt_num;
    unsigned long long mac_tx_pfc_pri7_pkt_num;
    unsigned long long mac_rx_pfc_pkt_num;
    unsigned long long mac_rx_pfc_pri0_pkt_num;
    unsigned long long mac_rx_pfc_pri1_pkt_num;
    unsigned long long mac_rx_pfc_pri2_pkt_num;
    unsigned long long mac_rx_pfc_pri3_pkt_num;
    unsigned long long mac_rx_pfc_pri4_pkt_num;
    unsigned long long mac_rx_pfc_pri5_pkt_num;
    unsigned long long mac_rx_pfc_pri6_pkt_num;
    unsigned long long mac_rx_pfc_pri7_pkt_num;
    unsigned long long mac_tx_total_pkt_num;
    unsigned long long mac_tx_total_oct_num;
    unsigned long long mac_tx_bad_pkt_num;
    unsigned long long mac_tx_bad_oct_num;
    unsigned long long mac_rx_total_pkt_num;
    unsigned long long mac_rx_total_oct_num;
    unsigned long long mac_rx_bad_pkt_num;
    unsigned long long mac_rx_bad_oct_num;
    unsigned long long mac_rx_fcs_err_pkt_num;
    unsigned long long roce_rx_rc_pkt_num;
    unsigned long long roce_rx_all_pkt_num;
    unsigned long long roce_rx_err_pkt_num;
    unsigned long long roce_tx_rc_pkt_num;
    unsigned long long roce_tx_all_pkt_num;
    unsigned long long roce_tx_err_pkt_num;
    unsigned long long roce_cqe_num;
    unsigned long long roce_rx_cnp_pkt_num;
    unsigned long long roce_tx_cnp_pkt_num;
    unsigned long long roce_err_ack_num;
    unsigned long long roce_err_psn_num;
    unsigned long long roce_verification_err_num;
    unsigned long long roce_err_qp_status_num;
    unsigned long long roce_new_pkt_rty_num;
    unsigned long long roce_ecn_db_num;
    unsigned long long nic_tx_all_pkg_num;
    unsigned long long nic_tx_all_oct_num;
    unsigned long long nic_rx_all_pkg_num;
    unsigned long long nic_rx_all_oct_num;
    long tv_sec;
    long tv_usec;
    unsigned char reserved[64];
};

struct dcmi_extra_statistics_info {
    unsigned long long cw_total_cnt;
    unsigned long long cw_before_correct_cnt;
    unsigned long long cw_correct_cnt;
    unsigned long long cw_uncorrect_cnt;
    unsigned long long cw_bad_cnt;
    unsigned long long trans_total_bit;
    unsigned long long cw_total_correct_bit;
    unsigned long long rx_full_drop_cnt;
    unsigned long long pcs_err_cnt;
    unsigned long long rx_send_app_good_pkts;
    unsigned long long rx_send_app_bad_pkts;
    double correcting_bit_rate;
};

#define MAX_LANE_NUM 8
typedef struct dcmi_serdes_prbs_param_base {
    unsigned int serdes_prbs_macro_id;
    unsigned int serdes_prbs_start_lane_id;
    unsigned int serdes_prbs_lane_count;
} DCMI_SERDES_PRBS_PARAM_BASE;

typedef DCMI_SERDES_PRBS_PARAM_BASE
    DCMI_SERDES_PRBS_GET_PARAM; // get时仅需传入基础信息param base，具体获取类型通过sub_cmd区分

typedef struct dcmi_serdes_prbs_set_param {
    DCMI_SERDES_PRBS_PARAM_BASE param_base;
    unsigned int serdes_prbs_type;
    unsigned int serdes_prbs_direction;
} DCMI_SERDES_PRBS_SET_PARAM;

typedef struct dcmi_prbs_operate_param {
    unsigned int main_cmd;
    unsigned int sub_cmd; // 标识是设置打流命令还是查询打流结果命令
    union {
        DCMI_SERDES_PRBS_SET_PARAM set_param;
        DCMI_SERDES_PRBS_GET_PARAM get_param;
    } operate_para;
} DCMI_PRBS_OPERATE_PARAM;

typedef struct dcmiv2_serdes_prbs_param_base {
    unsigned int udie_id;
    unsigned int port_id;
} DCMIV2_SERDES_PRBS_PARAM_BASE;

typedef DCMIV2_SERDES_PRBS_PARAM_BASE
    DCMIV2_SERDES_PRBS_GET_PARAM; // get时仅需传入基础信息param base，具体获取类型通过sub_cmd区分

typedef struct dcmiv2_serdes_prbs_set_param {
    DCMIV2_SERDES_PRBS_PARAM_BASE param_base;
    unsigned int serdes_prbs_type;
    unsigned int serdes_prbs_direction;
} DCMIV2_SERDES_PRBS_SET_PARAM;

typedef struct dcmiv2_prbs_operate_param {
    unsigned int main_cmd;
    unsigned int sub_cmd; // 标识是设置打流命令还是查询打流结果命令
    union {
        DCMIV2_SERDES_PRBS_SET_PARAM set_param;
        DCMIV2_SERDES_PRBS_GET_PARAM get_param;
    } operate_para;
} DCMIV2_PRBS_OPERATE_PARAM;

typedef struct dcmi_serdes_prbs_lane_status {
    unsigned int lane_prbs_tx_status;
    unsigned int lane_prbs_rx_status;
} DCMI_SERDES_PRBS_LANE_STATUS;

typedef struct {
    unsigned int check_en;
    unsigned int check_type;
    unsigned int error_status;
    unsigned int error_cnt;
    unsigned long error_rate; /* used by PAM4 only */
    unsigned int alos_status; /* 0:not los, 1:los */
    unsigned long time_val;
} SERDES_PRBS_STATUS_S;

typedef struct dcmi_prbs_operate_result {
    union {
        SERDES_PRBS_STATUS_S result[MAX_LANE_NUM];
        DCMI_SERDES_PRBS_LANE_STATUS lane_status[MAX_LANE_NUM];
    } prbs_result;
} DCMI_PRBS_OPERATE_RESULT;

enum dcmi_prbs_sub_cmd_list {
    SERDES_PRBS_SET_CMD = 0,
    SERDES_PRBS_GET_RESULT_CMD,
    SERDES_PRBS_GET_STATUS_CMD,
    SERDES_PRBS_SUB_CMD_MAX
};

enum dcmi_prbs_main_cmd_list {
    DSMI_SERDES_CMD_PRBS = 0,
    DSMI_SERDES_CMD_MAX
};

enum dcmi_serdes_prbs_type_list {
    SERDES_PRBS_TYPE_END = 0,
    SERDES_PRBS_TYPE_7,
    SERDES_PRBS_TYPE_9,
    SERDES_PRBS_TYPE_10,
    SERDES_PRBS_TYPE_11,
    SERDES_PRBS_TYPE_15,
    SERDES_PRBS_TYPE_20,
    SERDES_PRBS_TYPE_23,
    SERDES_PRBS_TYPE_31,
    SERDES_PRBS_TYPE_58,
    SERDES_PRBS_TYPE_63,
    SERDES_PRBS_TYPE_MAX
};

enum dcmi_serdes_prbs_direction {
    SERDES_PRBS_DIRECTION_TX = 0,
    SERDES_PRBS_DIRECTION_RX,
    SERDES_PRBS_DIRECTION_TXRX,
    SERDES_PRBS_DIRECTION_MAX
};

enum dcmi_serdes_npu_adaptive_status {
    DCMI_SERDES_NPU_ADAPTIVE_OPEN = 0,
    DCMI_SERDES_NPU_ADAPTIVE_CLOSE,
    DCMI_SERDES_NPU_ADAPTIVE_UPDATE,
}; // 需与 driver仓 MAX_PRBS_FLAG_SIZE 同步修改

#define IP_ADDR_LEN 16                // length of ipv4 string
#define DCMI_PING_PACKET_NUM_MAX 1000 // supernodes ping packet number
#define BRO_DEVICE_OFFSET 8
#define MULTI_DEVICE_NUM 2

// supernodes ping result
enum dcmi_ping_result {
    DCMI_PING_NOT_START,
    DCMI_PING_SUCCESS,
    DCMI_SEND_FAILED,
    DCMI_RECV_TIMEOUT,
    DCMI_RES_MAX
};

// supernodes ping operate
struct dcmi_ping_operate_info {
    char dst_addr[IP_ADDR_LEN]; // 输入ipv4地址。与sdid互斥，同时输入时，以dst_addr为准
    unsigned int sdid;          // 输入sdid数值
    unsigned int packet_size;
    unsigned int packet_send_num;
    unsigned int packet_interval;
    unsigned int timeout;
    unsigned char reserved[32];
};

// supernodes ping reply
struct dcmi_ping_reply_info {
    char dst_addr[IP_ADDR_LEN]; // 存储目的地址字符串
    enum dcmi_ping_result ret[DCMI_PING_PACKET_NUM_MAX];
    unsigned int total_packet_send_num;
    unsigned int total_packet_recv_num;
    long start_tv_sec[DCMI_PING_PACKET_NUM_MAX];
    long start_tv_usec[DCMI_PING_PACKET_NUM_MAX];
    long end_tv_sec[DCMI_PING_PACKET_NUM_MAX];
    long end_tv_usec[DCMI_PING_PACKET_NUM_MAX];
    unsigned char reserved[32];
};

struct dcmi_ping_reply_info_v2 {
    struct dcmi_ping_reply_info info;
    unsigned char L1_plane_check_res[DCMI_PING_PACKET_NUM_MAX];
    unsigned char reserved[DCMI_PING_PACKET_NUM_MAX];
};

#define IP_ADDR_LIST_LEN 1024
#define HCCS_PING_MESH_MAX_NUM 48
#define ADDR_MAX_LEN 16

struct dcmi_hccsping_mesh_operate {
    char dst_addr_list[IP_ADDR_LIST_LEN]; // IPV4地址/sdid列表，二者选其一，以英文逗号分割.
    int pkt_size;
    int pkt_send_num;
    int pkt_interval;
    int timeout;
    int task_interval;
    int task_id;
};

struct ping_mesh_parse_ext {
    int (*parse_proc)(struct dcmi_hccsping_mesh_operate *operate, int min, int max);
    int min;
    int max;
};

struct dcmi_hccsping_mesh_info {
    char dst_addr[HCCS_PING_MESH_MAX_NUM][ADDR_MAX_LEN];
    unsigned int suc_pkt_num[HCCS_PING_MESH_MAX_NUM];
    unsigned int fail_pkt_num[HCCS_PING_MESH_MAX_NUM];
    long max_time[HCCS_PING_MESH_MAX_NUM];
    long min_time[HCCS_PING_MESH_MAX_NUM];
    long avg_time[HCCS_PING_MESH_MAX_NUM];
    long tp95_time[HCCS_PING_MESH_MAX_NUM];
    int reply_stat_num[HCCS_PING_MESH_MAX_NUM];
    unsigned long long ping_total_num[HCCS_PING_MESH_MAX_NUM];
    int dest_num;
};

struct dcmi_hccsping_mesh_info_v2 {
    struct dcmi_hccsping_mesh_info info;
    unsigned char L1_plane_check_res[HCCS_PING_MESH_MAX_NUM]; // 到每个目的地址的hccs链路存在故障的L1平面汇总
    unsigned char reserved[HCCS_PING_MESH_MAX_NUM];
};

// traceroute struct info
#define IP_ADRESS_MAX_LEN 48
#define TRACEROUTE_RESERVE_MAX_LEN 64

struct dcmi_traceroute_info {
    int max_ttl;
    int tos;
    int waittime;
    int source_port;
    int dest_port;
    char dest_ip[IP_ADRESS_MAX_LEN];
    bool ipv6_flag;
    bool reset_flag;
};

struct dcmi_network_node_info {
    int mask;
    char ip[IP_ADRESS_MAX_LEN];
    int snt;
    double loss;
    double last;
    double avg;
    double best;
    double wrst;
    double stdev;
    char reserve[TRACEROUTE_RESERVE_MAX_LEN];
};

#define IP_MAX_LEN 48
#define TRACERT_PARAM_RES_LEN 7

struct dcmi_traceroute_params {
    int max_ttl;
    int tos;
    int waittime;
    int src_port;
    int dst_port;
    char src_ip[IP_MAX_LEN];
    char dst_ip[IP_MAX_LEN];
    bool reset_flag;
    char reserved[TRACERT_PARAM_RES_LEN];
};

struct traceroute_result {
    int func_mask;
    char src_ip[IP_MAX_LEN];
    char dst_ip[IP_MAX_LEN];
    int snt;
    double loss;
    double last;
    double avg;
    double best;
    double wrst;
    double stdev;
    char reserved[TRACEROUTE_RESERVE_MAX_LEN];
};

struct dcmi_traceroute_result {
    unsigned int result_nums;
    struct traceroute_result *result_data;
};

struct dcmi_pfc_duration_info {
    unsigned long long tx[PRIORITY_NUM];
    unsigned long long rx[PRIORITY_NUM];
};

struct dcmi_bond_pfc_duration_info {
    unsigned char slave0_name[NETDEV_NAME_MAX_LEN];
    unsigned long long slave0_tx[PRIORITY_NUM]; // 0~7  8个优先级队列
    unsigned long long slave0_rx[PRIORITY_NUM];
    unsigned char slave1_name[NETDEV_NAME_MAX_LEN];
    unsigned long long slave1_tx[PRIORITY_NUM]; // 0~7  8个优先级队列
    unsigned long long slave1_rx[PRIORITY_NUM];
};

struct dcmi_netdev_pfc_duration_info {
    union {
        struct dcmi_bond_pfc_duration_info duration_info;
        unsigned long long reserved[36]; // 36
    };
};

#define MAX_QPN_NUM 8192

struct dcmi_qpn_list {
    unsigned int qp_num;
    unsigned int qpn_list[MAX_QPN_NUM];
};

#define IP_ADDRESS_LEN 40
struct dcmi_qp_info {
    unsigned char status;    // qp状态
    unsigned char type;      // qp类型
    char ip[IP_ADDRESS_LEN]; // 目标ip地址
    unsigned short src_port; // 源端口
    unsigned int src_qpn;    // 源qp号
    unsigned int dst_qpn;    // 目标qp号
    unsigned int send_psn;   // 发送的psn
    unsigned int recv_psn;   // 接收的psn
    char reserved[32];       // 保留字段
};

#define DCMI_HCCS_MAX_PCS_NUM 16
struct dcmi_hccs_credit_info {
    unsigned int credit_num[DCMI_HCCS_MAX_PCS_NUM];
    unsigned int reserve[DCMI_HCCS_MAX_PCS_NUM * 4];
};

typedef enum {
    DCMI_SPOD_NODE_STATUS_NORMAL = 0,
    DCMI_SPOD_NODE_STATUS_ABNORMAL,
    DCMI_SPOD_NODE_STATUS_MAX
} DCMI_SPOD_NODE_STATUS;

struct dcmi_spod_node_status {
    unsigned int sdid;
    DCMI_SPOD_NODE_STATUS status;
};

#define DCMI_URMA_EID_SIZE (16)
#define DCMI_URMA_EID_MAX_COUNT (32)
typedef union dcmi_urma_eid {
    unsigned char raw[DCMI_URMA_EID_SIZE]; /* Network Order */
    struct {
        unsigned long reserved; /* If IPv4 mapped to IPv6, == 0 */
        unsigned int prefix;    /* If IPv4 mapped to IPv6, == 0x0000ffff */
        unsigned int addr;      /* If IPv4 mapped to IPv6, == IPv4 addr */
    } in4;
    struct {
        unsigned long subnet_prefix;
        unsigned long interface_id;
    } in6;
} dcmi_urma_eid_t;

typedef struct dcmi_urma_eid_info {
    dcmi_urma_eid_t eid;
    unsigned int eid_index;
} dcmi_urma_eid_info_t;

enum dcmi_multi_die_policy {
    DCMI_MULTI_DIE_UNION_POLICY = 0, // A3 多die必须同时通入容器
    DCMI_MULTI_DIE_INDEP_POLICY = 1, // A3 支持单die单独通入容器
    DCMI_MULTI_DIE_POLICY_MAX = 2
};

#define EID_MAX_LEN 48
#define UBPING_MESH_MAX_NUM 48
#define UB_PING_RESERVE_NUM 4

struct ubping_pair_info {
    char src_eid[EID_MAX_LEN]; // 源eid地址 0000:0000:0000:0000:0000:0000:df00:001c
    char dst_eid[EID_MAX_LEN]; // 目的eid地址 0000:0000:0000:0000:0000:0000:df00:001c
};

struct dcmi_ub_ping_operate {
    struct ubping_pair_info ub_pair_info; // 源/目的eid对
    int pkt_size; // ping报文大小，取值范围：0~4046，默认值为4046，注这里需要看ip over urma的mtu值是多少
    int pkt_send_num;                          // ping报文发送轮数, 默认为3轮；
    int pkt_interval;                          // ping报文间隔，多久发送一次，默认值为1S；
    int timeout;                               // ping超时时间，发送失败了后直接停止；
    unsigned int reserve[UB_PING_RESERVE_NUM]; // 预留字段
};

struct ub_ping_result {
    unsigned int suc_pkt_num;                  // ping成功次数
    unsigned int fail_pkt_num;                 // ping失败次数
    long max_time;                             // ping最大时延
    long min_time;                             // ping最小时延
    long avg_time;                             // ping平均时延
    long tp95_time;                            // ping时延TP95分位数
    unsigned int reserve[UB_PING_RESERVE_NUM]; // 预留字段
};

typedef enum {
    EVIDENCE_TYPE_NONE = 0,                          // 返回失败。
    EVIDENCE_TYPE_QUOTE = 1,                         // 仅包含Quote+PCR
    EVIDENCE_TYPE_BOOT_MEASUREMENTS = 2,             // 仅包含BIOS启动日志
    EVIDENCE_TYPE_BOOT_MEASUREMENTS_AND_PCR = 3,     // 仅包含Quote+PCR+BIOS启动日志
    EVIDENCE_TYPE_RUNTIME_MEASUREMENTS = 4,          // 仅包含IMA日志
    EVIDENCE_TYPE_RUN_TIME_MEASUREMENTS_AND_PCR = 5, // 仅包含Quote+PCR+BIOS启动日志
    EVIDENCE_TYPE_ALL = 7,                           // 包含Quote+PCR+BIOS启动日志+IMA日志
} EVIDENCE_TYPE;

#define MAX_CHALLENGE_LEN 32
struct attest_ctx {
    unsigned int challenge_len;
    EVIDENCE_TYPE evidence_type;
    char challenge[MAX_CHALLENGE_LEN];
};

struct dsmi_computing_power_info; // 前向声明
union tag_sensor_info;

/*----------------------------------------------*
 * Error code description                       *
 *----------------------------------------------*/

#define DCMI_OK 0
#define DCMI_ERROR_CODE_BASE (-8000)
#define DCMI_ERR_CODE_INVALID_PARAMETER (DCMI_ERROR_CODE_BASE - 1)
#define DCMI_ERR_CODE_OPER_NOT_PERMITTED (DCMI_ERROR_CODE_BASE - 2)
#define DCMI_ERR_CODE_MEM_OPERATE_FAIL (DCMI_ERROR_CODE_BASE - 3)
#define DCMI_ERR_CODE_SECURE_FUN_FAIL (DCMI_ERROR_CODE_BASE - 4)
#define DCMI_ERR_CODE_INNER_ERR (DCMI_ERROR_CODE_BASE - 5)
#define DCMI_ERR_CODE_TIME_OUT (DCMI_ERROR_CODE_BASE - 6)
#define DCMI_ERR_CODE_INVALID_DEVICE_ID (DCMI_ERROR_CODE_BASE - 7)
#define DCMI_ERR_CODE_DEVICE_NOT_EXIST (DCMI_ERROR_CODE_BASE - 8)
#define DCMI_ERR_CODE_IOCTL_FAIL (DCMI_ERROR_CODE_BASE - 9)
#define DCMI_ERR_CODE_SEND_MSG_FAIL (DCMI_ERROR_CODE_BASE - 10)
#define DCMI_ERR_CODE_RECV_MSG_FAIL (DCMI_ERROR_CODE_BASE - 11)
#define DCMI_ERR_CODE_NOT_REDAY (DCMI_ERROR_CODE_BASE - 12)
#define DCMI_ERR_CODE_NOT_SUPPORT_IN_CONTAINER (DCMI_ERROR_CODE_BASE - 13)
#define DCMI_ERR_CODE_FILE_OPERATE_FAIL (DCMI_ERROR_CODE_BASE - 14)
#define DCMI_ERR_CODE_RESET_FAIL (DCMI_ERROR_CODE_BASE - 15)
#define DCMI_ERR_CODE_ABORT_OPERATE (DCMI_ERROR_CODE_BASE - 16)
#define DCMI_ERR_CODE_IS_UPGRADING (DCMI_ERROR_CODE_BASE - 17)
#define DCMI_ERR_CODE_RESOURCE_OCCUPIED (DCMI_ERROR_CODE_BASE - 20)
#define DCMI_ERR_CODE_PARTITION_NOT_RIGHT (DCMI_ERROR_CODE_BASE - 22)
#define DCMI_ERR_CODE_CONFIG_INFO_NOT_EXIST (DCMI_ERROR_CODE_BASE - 23)
#define DCMI_ERR_CODE_NOT_SUPPORT (DCMI_ERROR_CODE_BASE - 255)

#define DCMI_VERSION_1
#define DCMI_VERSION_2

#if defined DCMI_VERSION_2

DCMIDLLEXPORT int dcmi_init(void);

DCMIDLLEXPORT int dcmi_get_dcmi_version(char *dcmi_ver, unsigned int len);

DCMIDLLEXPORT int dcmi_get_driver_version(char *driver_ver, unsigned int len);

DCMIDLLEXPORT int dcmi_get_card_list(int *card_num, int *card_list, int list_len);

DCMIDLLEXPORT int dcmi_get_device_num_in_card(int card_id, int *device_num);

DCMIDLLEXPORT int dcmi_get_device_id_in_card(int card_id, int *device_id_max, int *mcu_id, int *cpu_id);

DCMIDLLEXPORT int dcmi_get_device_type(int card_id, int device_id, enum dcmi_unit_type *device_type);

DCMIDLLEXPORT int dcmi_get_device_chip_info(int card_id, int device_id, struct dcmi_chip_info *chip_info);

DCMIDLLEXPORT int dcmi_get_device_chip_info_v2(int card_id, int device_id, struct dcmi_chip_info_v2 *chip_info);

DCMIDLLEXPORT int dcmi_get_device_pcie_info(int card_id, int device_id, struct dcmi_pcie_info *pcie_info);

DCMIDLLEXPORT int dcmi_get_device_pcie_info_v2(int card_id, int device_id, struct dcmi_pcie_info_all *pcie_info);

DCMIDLLEXPORT int dcmi_get_device_board_info(int card_id, int device_id, struct dcmi_board_info *board_info);

DCMIDLLEXPORT int dcmi_get_device_elabel_info(int card_id, int device_id, struct dcmi_elabel_info *elabel_info);

DCMIDLLEXPORT int dcmi_get_device_power_info(int card_id, int device_id, int *power);

DCMIDLLEXPORT int dcmi_set_card_customized_info(int card_id, char *info, int len);

DCMIDLLEXPORT int dcmi_get_card_customized_info(int card_id, char *info, int len);

DCMIDLLEXPORT int dcmi_set_device_clear_pcie_error(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_get_device_pcie_error_cnt(int card_id, int device_id,
                                                 struct dcmi_chip_pcie_err_rate *pcie_err_code_info);

DCMIDLLEXPORT int dcmi_get_device_die_v2(int card_id, int device_id, enum dcmi_die_type input_type,
                                         struct dcmi_die_id *die_id);

DCMIDLLEXPORT int dcmi_get_device_health(int card_id, int device_id, unsigned int *health);

DCMIDLLEXPORT int dcmi_get_device_errorcode_v2(int card_id, int device_id, int *error_count,
                                               unsigned int *error_code_list, unsigned int list_len);

DCMIDLLEXPORT int dcmi_get_device_errorcode_string(int card_id, int device_id, unsigned int error_code,
                                                   unsigned char *error_info, int buf_size);

DCMIDLLEXPORT int dcmi_get_device_flash_count(int card_id, int device_id, unsigned int *flash_count);

DCMIDLLEXPORT int dcmi_get_device_flash_info_v2(int card_id, int device_id, unsigned int flash_index,
                                                struct dcmi_flash_info *flash_info);

DCMIDLLEXPORT int dcmi_get_device_aicore_info(int card_id, int device_id, struct dcmi_aicore_info *aicore_info);

DCMIDLLEXPORT int dcmi_get_device_aicpu_info(int card_id, int device_id, struct dcmi_aicpu_info *aicpu_info);

DCMIDLLEXPORT int dcmi_get_device_boot_status(int card_id, int device_id, enum dcmi_boot_status *boot_status);

DCMIDLLEXPORT int dcmi_set_device_pre_reset(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_set_device_rescan(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_set_device_reset(int card_id, int device_id, enum dcmi_reset_channel channel_type);

DCMIDLLEXPORT int dcmi_get_mcu_upgrade_status(int card_id, int *status, int *progress);

DCMIDLLEXPORT int dcmi_get_mcu_version(int card_id, char *version, int len);

DCMIDLLEXPORT int dcmi_set_mcu_upgrade_stage(int card_id, enum dcmi_upgrade_type input_type);

DCMIDLLEXPORT int dcmi_set_mcu_upgrade_file(int card_id, const char *file);

DCMIDLLEXPORT int dcmi_get_device_system_time(int card_id, int device_id, unsigned int *time);

DCMIDLLEXPORT int dcmi_get_device_temperature(int card_id, int device_id, int *temperature);

DCMIDLLEXPORT int dcmi_get_device_voltage(int card_id, int device_id, unsigned int *voltage);

DCMIDLLEXPORT int dcmi_get_device_p2p_enable(int card_id, int device_id, int *enable_flag);

DCMIDLLEXPORT int dcmi_get_device_ecc_info(int card_id, int device_id, enum dcmi_device_type input_type,
                                           struct dcmi_ecc_info *device_ecc_info);

DCMIDLLEXPORT int dcmi_set_device_clear_ecc_statistics_info(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_get_device_frequency(int card_id, int device_id, enum dcmi_freq_type input_type,
                                            unsigned int *frequency);

DCMIDLLEXPORT int dcmi_get_device_hbm_info(int card_id, int device_id, struct dcmi_hbm_info *hbm_info);

DCMIDLLEXPORT int dcmi_get_device_memory_info_v2(int card_id, int device_id, struct dcmi_memory_info *memory_info);

DCMIDLLEXPORT int dcmi_get_device_memory_info_v3(int card_id, int device_id,
                                                 struct dcmi_get_memory_info_stru *memory_info);

DCMIDLLEXPORT int dcmi_get_device_utilization_rate(int card_id, int device_id, int input_type,
                                                   unsigned int *utilization_rate);

DCMIDLLEXPORT int dcmi_get_device_multi_utilization_rate(int card_id, int device_id,
                                                         struct dcmi_multi_utilization_info *util_info);

DCMIDLLEXPORT int dcmi_get_device_sensor_info(int card_id, int device_id, enum dcmi_manager_sensor_id sensor_id,
                                              union dcmi_sensor_info *sensor_info);

DCMIDLLEXPORT int dcmi_set_container_service_enable(void);

DCMIDLLEXPORT int dcmi_get_device_board_id(int card_id, int device_id, unsigned int *board_id);

DCMIDLLEXPORT int dcmi_get_device_component_count(int card_id, int device_id, unsigned int *component_count);

DCMIDLLEXPORT int dcmi_get_device_component_list(int card_id, int device_id, enum dcmi_component_type *component_table,
                                                 unsigned int component_count);

DCMIDLLEXPORT int dcmi_get_device_component_static_version(int card_id, int device_id,
                                                           enum dcmi_component_type component_type,
                                                           unsigned char *version_str, unsigned int len);

DCMIDLLEXPORT int dcmi_get_device_cgroup_info(int card_id, int device_id, struct dcmi_cgroup_info *cg_info);

DCMIDLLEXPORT int dcmi_get_device_llc_perf_para(int card_id, int device_id, struct dcmi_llc_perf *perf_para);

DCMIDLLEXPORT int dcmi_set_device_info(int card_id, int device_id, enum dcmi_main_cmd main_cmd, unsigned int sub_cmd,
                                       const void *buf, unsigned int buf_size);

DCMIDLLEXPORT int dcmi_get_device_info(int card_id, int device_id, enum dcmi_main_cmd main_cmd, unsigned int sub_cmd,
                                       void *buf, unsigned int *size);

DCMIDLLEXPORT int dcmi_set_device_sec_revocation(int card_id, int device_id, enum dcmi_revo_type input_type,
                                                 const unsigned char *file_data, unsigned int file_size);

DCMIDLLEXPORT int dcmi_get_device_mac_count(int card_id, int device_id, int *count);

DCMIDLLEXPORT int dcmi_set_device_mac(int card_id, int device_id, int mac_id, const char *mac_addr, unsigned int len);

DCMIDLLEXPORT int dcmi_get_device_mac(int card_id, int device_id, int mac_id, char *mac_addr, unsigned int len);

DCMIDLLEXPORT int dcmi_get_device_gateway(int card_id, int device_id, enum dcmi_port_type input_type, int port_id,
                                          struct dcmi_ip_addr *gateway);

DCMIDLLEXPORT int dcmi_set_device_gateway(int card_id, int device_id, enum dcmi_port_type input_type, int port_id,
                                          struct dcmi_ip_addr *gateway);

DCMIDLLEXPORT int dcmi_set_device_ip(int card_id, int device_id, enum dcmi_port_type input_type, int port_id,
                                     struct dcmi_ip_addr *ip, struct dcmi_ip_addr *mask);

DCMIDLLEXPORT int dcmi_get_device_ip(int card_id, int device_id, enum dcmi_port_type input_type, int port_id,
                                     struct dcmi_ip_addr *ip, struct dcmi_ip_addr *mask);

DCMIDLLEXPORT int dcmi_get_device_network_health(int card_id, int device_id, enum dcmi_rdfx_detect_result *result);

DCMIDLLEXPORT int dcmi_get_device_fan_count(int card_id, int device_id, int *count);

DCMIDLLEXPORT int dcmi_get_device_fan_speed(int card_id, int device_id, int fan_id, int *speed);

DCMIDLLEXPORT int dcmi_get_device_logic_id(int *device_logic_id, int card_id, int device_id);

DCMIDLLEXPORT int dcmi_get_card_elabel_v2(int card_id, struct dcmi_elabel_info *elabel_info);

DCMIDLLEXPORT int dcmi_mcu_get_chip_temperature(int card_id, char *data_info, int buf_size, int *data_len);

DCMIDLLEXPORT int dcmi_get_device_ssh_enable(int card_id, int device_id, int *enable_flag);

DCMIDLLEXPORT int dcmi_set_device_share_enable(int card_id, int device_id, int enable_flag);

DCMIDLLEXPORT int dcmi_get_device_share_enable(int card_id, int device_id, int *enable_flag);

DCMIDLLEXPORT int dcmi_set_device_share_config_recover_mode(unsigned int enable_flag);

DCMIDLLEXPORT int dcmi_get_device_share_config_recover_mode(unsigned int *enable_flag);

DCMIDLLEXPORT int dcmi_get_multi_die_policy_config_recover_mode(unsigned int *enable_flag);

DCMIDLLEXPORT int dcmi_set_multi_die_policy_config_recover_mode(unsigned int enable_flag);

DCMIDLLEXPORT int dcmi_get_card_board_info(int card_id, struct dcmi_board_info *board_info);

DCMIDLLEXPORT int dcmi_get_card_pcie_info(int card_id, char *pcie_info, int pcie_info_len);

DCMIDLLEXPORT int dcmi_get_card_pcie_slot(int card_id, int *pcie_slot);

DCMIDLLEXPORT int dcmi_get_fault_device_num_in_card(int card_id, int *device_num);

DCMIDLLEXPORT int dcmi_mcu_check_i2c(int card_id, int *health_status, int buf_size);

DCMIDLLEXPORT int dcmi_set_device_user_config(int card_id, int device_id, const char *config_name,
                                              unsigned int buf_size, char *buf);

DCMIDLLEXPORT int dcmi_mcu_collect_log(int card_id, int log_type);

DCMIDLLEXPORT int dcmi_get_device_chip_slot(int card_id, int device_id, int *chip_pos_id);

DCMIDLLEXPORT int dcmi_get_product_type(int card_id, int device_id, char *product_type_str, int buf_size);

DCMIDLLEXPORT int dcmi_get_device_outband_channel_state(int card_id, int device_id, int *channel_state);

DCMIDLLEXPORT int dcmi_get_device_aicpu_count_info(int card_id, int device_id, unsigned char *count_info);

DCMIDLLEXPORT int dcmi_create_vdevice(int card_id, int device_id, struct dcmi_create_vdev_res_stru *vdev,
                                      struct dcmi_create_vdev_out *out);

DCMIDLLEXPORT int dcmi_set_destroy_vdevice(int card_id, int device_id, unsigned int vdevid);

DCMIDLLEXPORT int dcmi_get_board_id(int card_id, int device_id, int *board_id);

DCMIDLLEXPORT int dcmi_get_first_power_on_date(int card_id, unsigned int *first_power_on_date);

DCMIDLLEXPORT int dcmi_get_fault_event(int card_id, int device_id, int timeout, struct dcmi_event_filter filter,
                                       struct dcmi_event *event);

DCMIDLLEXPORT int dcmi_get_device_current_fault_event(int card_id, int device_id, struct dcmi_event *event_buf,
                                                      int input_event_buf_length, int *output_event_cnt);

DCMIDLLEXPORT int dcmi_subscribe_fault_event(int card_id, int device_id, struct dcmi_event_filter filter,
                                             dcmi_fault_event_callback handler);

DCMIDLLEXPORT int dcmi_get_device_resource_info(int card_id, int device_id, struct dcmi_proc_mem_info *proc_info,
                                                int *proc_num);

DCMIDLLEXPORT int dcmi_get_device_dvpp_ratio_info(int card_id, int device_id, struct dcmi_dvpp_ratio *usage);

DCMIDLLEXPORT int dcmi_get_device_cpu_num_config(int card_id, int device_id, unsigned char *buf, unsigned int buf_size);

DCMIDLLEXPORT int dcmi_set_device_cpu_num_config(int card_id, int device_id, unsigned char *buf, unsigned int buf_size);

DCMIDLLEXPORT int dcmi_get_device_phyid_from_logicid(unsigned int logicid, unsigned int *phyid);

DCMIDLLEXPORT int dcmi_get_device_logicid_from_phyid(unsigned int phyid, unsigned int *logicid);

DCMIDLLEXPORT int dcmi_get_card_id_device_id_from_logicid(int *card_id, int *device_id, unsigned int device_logic_id);

DCMIDLLEXPORT int dcmi_get_card_id_device_id_from_phyid(int *card_id, int *device_id, unsigned int device_phy_id);

DCMIDLLEXPORT int dcmi_set_vdevice_mode(int mode);

DCMIDLLEXPORT int dcmi_get_vdevice_mode(int *mode);

DCMIDLLEXPORT int dcmi_set_vnpu_config_recover_mode(unsigned int mode);

DCMIDLLEXPORT int dcmi_get_vnpu_config_recover_mode(unsigned int *mode);

DCMIDLLEXPORT int dcmi_get_driver_health(unsigned int *health);

DCMIDLLEXPORT int dcmi_get_driver_errorcode(int *error_count, unsigned int *error_code_list, unsigned int list_len);

DCMIDLLEXPORT int dcmi_sm_encrypt(int card_id, int device_id, struct dcmi_sm_parm *parm, struct dcmi_sm_data *data);

DCMIDLLEXPORT int dcmi_sm_decrypt(int card_id, int device_id, struct dcmi_sm_parm *parm, struct dcmi_sm_data *data);

DCMIDLLEXPORT int dcmi_get_npu_work_mode(int card_id, unsigned char *work_mode);

DCMIDLLEXPORT int dcmi_get_multi_die_policy(enum dcmi_multi_die_policy *policy);

DCMIDLLEXPORT int dcmi_set_multi_die_policy(enum dcmi_multi_die_policy policy);

DCMIDLLEXPORT int dcmi_get_mainboard_id(int card_id, int device_id, unsigned int *mainboard_id);

DCMIDLLEXPORT int dcmi_set_power_state(int card_id, int device_id, struct dcmi_power_state_info_stru power_info);

DCMIDLLEXPORT int dcmi_get_device_compatibility(int card_id, int device_id, enum dcmi_device_compat *compatibility);

DCMIDLLEXPORT int dcmi_create_capability_group(int card_id, int device_id, int ts_id,
                                               struct dcmi_capability_group_info *group_info);

DCMIDLLEXPORT int dcmi_delete_capability_group(int card_id, int device_id, int ts_id, int group_id);

DCMIDLLEXPORT int dcmi_get_capability_group_info(int card_id, int device_id, int ts_id, int group_id,
                                                 struct dcmi_capability_group_info *group_info, int group_count);

DCMIDLLEXPORT int dcmi_get_capability_group_aicore_usage(int card_id, int device_id, int group_id, int *rate);

DCMIDLLEXPORT int dcmi_get_multi_ecc_time_info(int card_id, struct dcmi_multi_ecc_time_data *multi_ecc_time_data);

DCMIDLLEXPORT int dcmi_get_multi_ecc_record_info(int card_id, unsigned int *ecc_count, unsigned char read_type,
                                                 unsigned char module_type,
                                                 struct dcmi_ecc_common_data *ecc_common_data_s);

DCMIDLLEXPORT int dcmi_get_pcie_link_bandwidth_info(int card_id, int device_id,
                                                    struct dcmi_pcie_link_bandwidth_info *pcie_link_bandwidth_info);

DCMIDLLEXPORT int dcmi_get_hccs_avail_credit_info(int card_id, int device_id,
                                                  struct dcmi_hccs_credit_info *hccs_avail_credit_info);

DCMIDLLEXPORT int dcmi_get_topo_info_by_device_id(int card_id1, int device_id1, int card_id2, int device_id2,
                                                  int *topo_type);

DCMIDLLEXPORT int dcmi_get_affinity_cpu_info_by_device_id(int card_id, int device_id, char *affinity_cpu, int *length);

DCMIDLLEXPORT int dcmi_get_multi_ecc_time_info_v2(int card_id, int device_id,
                                                  struct dcmi_multi_ecc_time_data *multi_ecc_time_data);

DCMIDLLEXPORT int dcmi_get_multi_ecc_record_info_v2(int card_id, int device_id, struct dcmi_ecc_record_type type,
                                                    unsigned int *ecc_count,
                                                    struct dcmi_ecc_common_data *ecc_common_data_s);

DCMIDLLEXPORT int dcmi_get_serdes_quality_info(int card_id, int device_id, unsigned int macro_id,
                                               struct dcmi_serdes_quality_info *serdes_quality_info);

DCMIDLLEXPORT int dcmi_get_serdes_full_eye(int card_id, int device_id,
                                           struct dcmi_serdes_full_eye *serdes_full_eye_info);

DCMIDLLEXPORT int dcmi_get_rdma_bandwidth_info(int card_id, int device_id, int port_id, unsigned int prof_time,
                                               struct dcmi_network_rdma_bandwidth_info *network_rdma_bandwidth_info);

DCMIDLLEXPORT int dcmi_get_netdev_pkt_stats_info(int card_id, int device_id, int port_id,
                                                 struct dcmi_network_pkt_stats_info *network_pkt_stats_info);

DCMIDLLEXPORT int dcmi_get_netdev_tc_stat_info(int card_id, int device_id,
                                               struct dcmi_tc_stat_data *network_tc_stat_info);

DCMIDLLEXPORT int dcmi_set_device_clear_tc_pkt_stats(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_prbs_operate(int card_id, int device_id, DCMI_PRBS_OPERATE_PARAM operate_para,
                                    DCMI_PRBS_OPERATE_RESULT *operate_result);

DCMIDLLEXPORT int dcmi_get_ping_info(int card_id, int device_id, int port_id, struct dcmi_ping_operate_info *dcmi_ping,
                                     struct dcmi_ping_reply_info *dcmi_reply);

DCMIDLLEXPORT int dcmi_get_ping_info_v2(int card_id, int device_id, int port_id,
                                        struct dcmi_ping_operate_info *dcmi_ping,
                                        struct dcmi_ping_reply_info_v2 *dcmi_reply);

DCMIDLLEXPORT int dcmi_start_hccsping_mesh(int card_id, int device_id, int port_id,
                                           struct dcmi_hccsping_mesh_operate *hccsping_mesh);

DCMIDLLEXPORT int dcmi_stop_hccsping_mesh(int card_id, int device_id, int port_id, unsigned int task_id);

DCMIDLLEXPORT int dcmi_get_hccsping_mesh_info(int card_id, int device_id, int port_id, unsigned int task_id,
                                              struct dcmi_hccsping_mesh_info *hccsping_mesh_reply);

DCMIDLLEXPORT int dcmi_get_hccsping_mesh_info_v2(int card_id, int device_id, int port_id, unsigned int task_id,
                                                 struct dcmi_hccsping_mesh_info_v2 *hccsping_mesh_reply);

DCMIDLLEXPORT int dcmi_get_hccsping_mesh_state(int card_id, int device_id, int port_id, unsigned int task_id,
                                               unsigned int *state);

DCMIDLLEXPORT int dcmi_set_traceroute(int card_id, int device_id, struct dcmi_traceroute_info param_in,
                                      struct dcmi_network_node_info *ret_info, unsigned int ret_info_size);

DCMIDLLEXPORT int dcmi_get_pfc_duration_info(int card_id, int device_id,
                                             struct dcmi_pfc_duration_info *pfc_duration_info);

DCMIDLLEXPORT int dcmi_clear_pfc_duration(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_get_qpn_list(int card_id, int device_id, int port_id, struct dcmi_qpn_list *list);

DCMIDLLEXPORT int dcmi_get_qp_info(int card_id, int device_id, int port_id, unsigned int qpn,
                                   struct dcmi_qp_info *qp_info);

DCMIDLLEXPORT int dcmi_get_device_cpu_freq_info(int card_id, int device_id, int *enable_flag);

DCMIDLLEXPORT int dcmi_get_custom_op_status(int card_id, int device_id, int *enable_flag);

DCMIDLLEXPORT int dcmi_get_custom_op_config_recover_mode(unsigned int *mode);

DCMIDLLEXPORT int dcmi_set_custom_op_config_recover_mode(unsigned int mode);

DCMIDLLEXPORT int dcmi_set_spod_node_status(int card_id, int device_id, unsigned int sdid, unsigned int status);

DCMIDLLEXPORT int dcmi_get_spod_node_status(int card_id, int device_id, unsigned int sdid, unsigned int *status);
/**
 * @brief: get specific security evidence.
 * @param [in] card_id card id
 * @param [in] device_id device id
 * @param [in] ctx include challenge and evidence type
 * @param [out] evidence the output of the evidence
 * @param [out] evidence_len the length of the evidence.The value cannot exceed 512*1024.
 * @return  0 for success, others for fail
 * @note Ascend910_93
 */
DCMIDLLEXPORT int dcmi_get_attest_evidence(int card_id, int device_id, struct attest_ctx *ctx, unsigned char *evidence,
                                           unsigned int *evidence_len);
/**
 * @brief: get akcert.
 * @param [in] card_id card id
 * @param [in] device_id device id
 * @param [out] ak_cert the output of the ak_cert
 * @param [out] ak_cert_len the length of the ak_cert.The value cannot exceed 2*1024.
 * @return  0 for success, others for fail
 * @note Ascend910_93
 */
DCMIDLLEXPORT int dcmi_get_attest_akcert(int card_id, int device_id, unsigned char *ak_cert, unsigned int *ak_cert_len);

/**
 * @brief 获取对应芯片上urma device数量
 * @param card_id    [in]  指定NPU管理单元ID
 * @param device_id  [in]  指定设备编号
 * @param dev_cnt    [out] urma device数量
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmi_get_urma_device_cnt(int card_id, int device_id, unsigned int *dev_cnt);

/**
 * @brief 获取对应芯片指定urma device的eid列表
 * @param card_id    [in]  指定NPU管理单元ID
 * @param device_id  [in]  指定设备编号
 * @param dev_index  [in]  urma device编号
 * @param eid_list   [out] 对应urma device的eid列表，约定最大长度为32个eid
 * @param eid_cnt    [in/out] 输入时代表传入的eid_list长度，输出时对应urma device的eid数量
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmi_get_eid_list_by_urma_dev_index(int card_id, int device_id, unsigned int dev_index,
                                                      dcmi_urma_eid_info_t *eid_list, unsigned int *eid_cnt);

/**
 * @brief 查询驱动版本
 * @param driver_ver  [out] 用户申请的空间，用于存放返回的版本号.
 * @param len    [in] driver_ver空间的长度。
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_driver_version(char *driver_ver, unsigned int len);

/**
 * @brief 查询DCMI版本
 * @param dcmi_ver  [out] 用户申请的空间，用于存放返回的版本号。
 * @param len    [int] dcmi_ver空间的长度
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_dcmi_version(char *dcmi_ver, unsigned int len);

/**
 * @brief 获取指定设备的URMA（Unified Remote Memory Access） device数量
 * @param device_id  [in]  芯片逻辑ID
 * @param dev_cnt    [out] URMA device数量
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_urma_device_cnt(int dev_id, unsigned int *dev_cnt);

/**
 * @brief 获取指定芯片上对应URMA device的EID（entity ID）列表。
 * @param device_id  [in]  芯片逻辑ID
 * @param dev_index  [in]  URMA device编号
 * @param eid_list   [out] URMA device的EID列表
 * @param eid_cnt    [in/out] 输入表示EID列表长度，最大长度为32个EID;输出表示URMA device的EID数量。
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_eid_list_by_urma_dev_index(int dev_id, unsigned int dev_index,
                                                        dcmi_urma_eid_info_t *eid_list, unsigned int *eid_cnt);

/**
 * @brief 获取对应芯片的芯片信息
 * @param device_id  [in]  芯片逻辑ID
 * @param chip_info   [out] chip_info信息，包含chip_name, chip_ver, aicore_cnt, npu_name
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_chip_info(int dev_id, struct dcmi_chip_info_v2 *chip_info);

/**
 * @brief 获取指定设备的PCIe信息
 * @param device_id  [in]  芯片逻辑ID
 * @param pcie_info   [out] PCIe信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_pcie_info(int dev_id, struct dcmi_pcie_info_all *pcie_info);

/**
 * @brief 获取指定设备的UB四元组信息
 * @param device_id   [in]  芯片逻辑ID
 * @param ub_id_info  [out] UB四元组信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_ub_id_info(int dev_id, struct dcmi_ub_id_info *ub_id_info);

/**
 * @brief 获取指定设备的board信息
 * @param device_id  [in] 芯片逻辑ID
 * @param board_info   [out] board信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_board_info(int dev_id, struct dcmi_board_info *board_info);

/**
 * @brief 获取设备的电子标签信息
 * @param device_id  [in]  芯片逻辑ID
 * @param elabel_info   [out] 电子标签信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_elabel_info(int dev_id, struct dcmi_elabel_info *elabel_info);

/**
 * @brief 获取所有已建链卡的数量
 * @param all_device_count    [out] device数量
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_all_device_count(int *all_device_count);

/**
 * @brief 获取指定NPU间的拓扑关系
 * @param device_id1  [in]  指定设备编号
 * @param device_id2  [in]  指定设备编号
 * @param topo_type   [out] NPU间的拓扑关系
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_topo_info_by_device_id(int dev_id1, int dev_id2, int *topo_type);

/**
 * @brief 查询指定NPU的CPU亲和性
 * @param dev_id    [in]  指定设备编号
 * @param affinity_cpu [out] 指定NPU设备的CPU亲和性
 * @param len       [out] 亲和性字符串长度
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_affinity_cpu_info_by_dev_id(int dev_id, char *affinity_cpu, int *len);

/**
 * @brief 查询指定NPU的mainboard_id
 * @param dev_id     [in]  指定设备编号
 * @param mainboard_id  [out] NPU的mainboard_id
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_mainboard_id(int dev_id, unsigned int *mainboard_id);

/**
 * @brief 查询指定NPU的某个macro端口的眼图质量
 * @param dev_id     [in]  指定设备编号
 * @param macro_id      [in]  macro端口编号
 * @param serdes_quality_info  [out] NPU的某个macro端口的眼图质量信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_serdes_quality_info(int dev_id, unsigned int macro_id,
                                                 struct dcmi_serdes_quality_info *serdes_quality_info);

/**
 * @brief 查询指定NPU的片上内存厂商信息
 * @param dev_id     [in]  指定设备编号
 * @param hbm_product_info  [out] NPU的片上内存厂商信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_hbm_product_info(int dev_id, struct dcmi_hbm_product_info *hbm_product_info);

/**
 * @brief 查询指定NPU的容器共享信息
 * @param dev_id       [in]  指定设备编号
 * @param enable_flag  [out] NPU的容器共享使能标记
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_share_enable(int dev_id, unsigned int *enable_flag);

/**
 * @brief 设置指定NPU的容器共享信息
 * @param dev_id       [in]  指定设备编号
 * @param enable_flag  [in]  NPU的容器共享使能标记
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_set_device_share_enable(int dev_id, unsigned int enable_flag);

/**
 * @brief 查询容器共享持久化信息
 * @param enable_flag  [out] 容器共享持久化使能标记
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_share_config_recover_mode(unsigned int *enable_flag);

/**
 * @brief 设置容器共享持久化
 * @param enable_flag  [in] 容器共享持久化使能标记
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_set_device_share_config_recover_mode(unsigned int enable_flag);

/**
 * @brief 查询NPU设备网口当前收发包数统计
 * @param dev_id         [in]    指定设备编号
 * @param port_id           [in]    指定设备的网口端口号
 * @param network_pkt_stats_info  [out] 网口当前收发包统计信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_netdev_pkt_stats_info(int dev_id, int port_id,
                                                   struct dcmi_network_pkt_stats_info *network_pkt_stats_info);

/**
 * @brief 查询指定采样时间内，NPU设备网口的rdma带宽信息
 * @param dev_id         [in]    指定设备编号
 * @param port_id           [in]    指定设备的网口端口号
 * @param prof_time         [in]    采样时间
 * @param network_rdma_bandwidth_info  [out] 网口rdma带宽信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_rdma_bandwidth_info(int dev_id, int port_id, unsigned int prof_time,
                                                 struct dcmi_network_rdma_bandwidth_info *network_rdma_bandwidth_info);

/**
 * @brief 获取驱动健康状态
 * @param health   [out] 驱动健康状态的指针
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_driver_health(unsigned int *health);

/**
 * @brief 查询设备故障码
 * @param dev_id  [in]  芯片逻辑ID
 * @param error_count   [out] 错误码数量
 * @param error_code_list   [out] 错误码列表
 * @param list_len  [in]  error_code_list空间大小
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_error_code_list(int dev_id, int *error_count, unsigned int *error_code_list,
                                                    unsigned int list_len);

/**
 * @brief 查询芯片的总体健康状态
 * @param dev_id  [in] 芯片逻辑ID
 * @param health   [out] 设备总体健康状态
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_health(int dev_id, unsigned int *health);

/**
 * @brief 获取指定设备的DIE ID
 * @param dev_id  [in]  芯片逻辑ID
 * @param input_type [in]  die类型
 * @param die_id   [out] die_id信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_die_id(int dev_id, enum dcmi_die_type input_type, struct dcmi_die_id *die_id);

/**
 * @brief 查询设备功耗
 * @param dev_id  [in]  芯片逻辑ID
 * @param power_info   [out] 设备功耗：单位为0.1W
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_power_info(int dev_id, int *power_info);

/**
 * @brief 查询设备类型
 * @param dev_id  [in]  芯片逻辑ID
 * @param device_type   [out] 设备类型
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_type(int dev_id, enum dcmi_unit_type *device_type);
/**
 * @brief 查询设备电压信息
 * @param dev_id  [in]  芯片逻辑ID
 * @param voltage [out] voltage信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_voltage(int dev_id, unsigned int *voltage);
/**
 * @brief 查询设备温度信息
 * @param dev_id      [in]  芯片逻辑ID
 * @param temperature [out] temperature信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_temperature(int dev_id, int *temperature);

/**
 * @brief 查询驱动故障码
 * @param error_count   [out] 错误码数量
 * @param error_code_list   [out] 错误码列表
 * @param list_len  [in]  error_code_list空间大小。
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_driver_error_code_list(int *error_count, unsigned int *error_code_list,
                                                    unsigned int list_len);

/**
 * @brief 查询设备故障描述
 * @param dev_id  [in]  芯片逻辑ID
 * @param error_code   [out] 要查询的错误码
 * @param error_info   [out] 对应的错误描述
 * @param buf_size  [in] 传入error_info的大小
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_error_info(int dev_id, unsigned int error_code, unsigned char *error_info,
                                               int buf_size);

/**
 * @brief 获取设备中Flash个数
 * @param dev_id      [in]  芯片逻辑ID
 * @param flash_cnt   [out] 返回Flash个数
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_flash_cnt(int dev_id, unsigned int *flash_cnt);

/**
 * @brief 获取设备中Flash个数
 * @param dev_id  [in]  芯片逻辑ID
 * @param flash_index  [in] Flash索引号
 * @param flash_info   [out] 返回Flash信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_flash_info(int dev_id, unsigned int flash_index,
                                               struct dcmi_flash_info *flash_info);

/**
 * @brief 查询AICore信息
 * @param dev_id  [in]  芯片逻辑ID
 * @param aicore_info   [out] 返回aicore信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_aicore_info(int dev_id, struct dcmi_aicore_info *aicore_info);

/**
 * @brief 查询NPU芯片驱动与固件兼容性
 * @param dev_id      [in]  指定设备编号
 * @param compatibility  [out] 兼容性信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_compatibility(int dev_id, enum dcmi_device_compat *compatibility);

/**
 * @brief 查询指定采样时间内，NPU设备与host OS间PCIe带宽信息
 * @param dev_id                 [in]    指定设备编号
 * @param pcie_link_bandwidth_info  [out] NPU设备与host OS间PCIe带宽信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_pcie_link_bandwidth_info(int dev_id,
                                                      struct dcmi_pcie_link_bandwidth_info *pcie_link_bandwidth_info);

/**
 * @brief 查询片上内存ECC多比特时间戳信息
 * @param dev_id            [in]  指定设备编号
 * @param multi_ecc_time_data  [out] 多比特时间戳信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_multi_ecc_time_info(int dev_id, struct dcmi_multi_ecc_time_data *multi_ecc_time_data);

/**
 * @brief 查询片上内存ECC详细地址信息
 * @param dev_id    [in]     指定设备编号
 * @param type         [in]     类型
 * @param ecc_count    [out]    片上内存ECC错误个数
 * @param ecc_common_data_s  [out] 内存ECC详细地址信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_multi_ecc_record_info(int dev_id, struct dcmi_ecc_record_type type,
                                                   unsigned int *ecc_count,
                                                   struct dcmi_ecc_common_data *ecc_common_data_s);

/**
 * @brief 获取设备的启动状态
 * @param dev_id          [in]  指定设备编号
 * @param boot_status        [out] 设备启动状态信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_boot_status(int dev_id, enum dcmi_boot_status *boot_status);

/**
 * @brief 获取某个芯片的告警信息
 * @param dev_id                [in]  指定设备编号
 * @param event_buf                [out] 存储故障事件
 * @param input_event_buf_length   [in]  期望获取的故障事件最大个数
 * @param output_event_cnt         [out] 实际获取到的故障事件个数
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_current_fault_event(int dev_id, struct dcmi_event *event_buf,
                                                        int input_event_buf_length, int *output_event_cnt);

/**
 * @brief 获取指定设备上的SVM模块相关业务进程及其占用的内存
 * @param dev_id           [in]   指定设备编号
 * @param proc_info           [out]  结构体包含进程ID和进程占用的内存
 * @param proc_num            [out]  进程个数
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_proc_mem_info(int dev_id, struct dcmi_proc_mem_info *proc_info, int *proc_num);

/**
 * @brief 订阅设备故障或恢复事件
 * @param dev_id       [in]  芯片逻辑ID
 * @param timeout      [in]  阻塞等待时间
 * @param filter       [in]  过滤条件
 * @param event        [out] 输出事件
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_fault_event(int dev_id, int timeout, struct dcmi_event_filter filter,
                                         struct dcmi_event *event);

/**
 * @brief 查询算力信息
 * @param dev_id       [in]  芯片逻辑ID
 * @param type         [in]  获取算力信息的类型
 * @param computing_power        [out] 输出的算力信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_computing_power_info(int dev_id, int type,
                                                  struct dsmi_computing_power_info *computing_power);

/**
 * @brief 查询带外通道状态
 * @param dev_id       [in]  芯片逻辑ID
 * @param channel_state   [out] 带外通道状态
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_outband_channel_state(int dev_id, int *channel_state);

/**
 * @brief 查询指定设备的PCIe slot ID
 * @param dev_id        [in]  芯片逻辑ID
 * @param pcie_slot     [out] PCIe slot ID
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_pcie_slot_id(int dev_id, int *pcie_slot);

/**
 * @brief 查询指定设备的UB slot ID
 * @param dev_id        [in]  芯片逻辑ID
 * @param ub_slot       [out] UB slot ID
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_ub_slot_id(int dev_id, int *ub_slot);

/**
 * @brief 根据芯片逻辑ID获取芯片物理ID
 * @param dev_id         [in]  芯片逻辑ID
 * @param phyid          [out] 芯片物理ID
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_chip_phy_id_by_dev_id(int dev_id, unsigned int *phyid);

/**
 * @brief 根据芯片物理ID获取芯片逻辑ID
 * @param phyid          [in] 芯片物理ID
 * @param dev_id         [out]  芯片逻辑ID
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_dev_id_by_chip_phy_id(int phyid, unsigned int *dev_id);

/**
 * @brief dcmi接口相关配置初始化
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_init(void);

/**
 * @brief 获取指定设备上的ecc信息
 * @param dev_id           [in]   指定设备编号
 * @param input_type       [in]   die类型
 * @param device_ecc_info  [out]  返回设备ecc信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_ecc_info(int dev_id, enum dcmi_device_type input_type,
                                             struct dcmi_ecc_info *device_ecc_info);

/**
 * @brief 获取指定设备上的频率
 * @param dev_id           [in]   指定设备编号
 * @param input_type       [in]   die类型
 * @param frequency        [out]  返回设备频率信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_frequency(int dev_id, enum dcmi_freq_type input_type, unsigned int *frequency);

/**
 * @brief 获取指定设备芯片的片上内存信息
 * @param dev_id           [in]   指定设备编号
 * @param hbm_info         [out]  返回设备片上内存信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_hbm_info(int dev_id, struct dcmi_hbm_info *hbm_info);

/**
 * @brief 获取指定设备芯片的占用率
 * @param dev_id           [in]   指定设备编号
 * @param input_type       [in]   die类型
 * @param utilization_rate [out]  返回设备占用率信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_utilization_rate(int dev_id, int input_type, unsigned int *utilization_rate);

/**
 * @brief 获取指定设备的传感器信息
 * @param dev_id           [in]   指定设备编号
 * @param sensor_id        [in]   指定传感器索引
 * @param sensor_info      [out]  返回传感器结构体信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_sensor_info(int dev_id, enum dcmi_manager_sensor_id sensor_id,
                                                union dcmi_sensor_info *sensor_info);

/**
 * @brief 获取指定设备的Board ID
 * @param dev_id           [in]   指定设备编号
 * @param board_id         [out]  返回单板的ID
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_board_id(int dev_id, unsigned int *board_id);

/**
 * @brief 获取可升级组件的个数
 * @param dev_id           [in]   指定设备编号
 * @param component_count  [out]  返回组件的个数
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_component_cnt(int dev_id, unsigned int *component_count);

/**
 * @brief 获取可升级组件列表
 * @param dev_id           [in]   指定设备编号
 * @param component_table  [out]  返回可升级组件列表
 * @param component_count  [in]   可升级组件列表数组的长度
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_component_list(int dev_id, enum dcmi_component_type *component_table,
                                                   unsigned int component_count);

/**
 * @brief 获取设备ID列表以及数量
 * @param device_list    [out]  设备ID列表
 * @param device_cnt     [out]  设备数量
 * @param list_len       [in]   数组列表长度
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_list(int *device_list, int *device_cnt, int list_len);

/**
 * @brief 查询芯片的PCIe链路误码信息。
 * @param dev_id                 [in]   指定设备编号
 * @param pcie_err_code_info     [out]  链路状态以及误码数量
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_pcie_error_cnt(int dev_id, struct dcmi_chip_pcie_err_rate *pcie_err_code_info);

/**
 * @brief 对指定设备执行预复位操作
 * @param dev_id                   [in]  指定设备编号
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_pre_reset_device(int dev_id);

/**
 * @brief 对指定设备重新扫描。
 * @param dev_id                   [in]  指定设备编号
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_rescan_device(int dev_id);

/**
 * @brief 对指定设备执行复位操作
 * @param dev_id                   [in]  指定设备编号
 * @param channel_type             [in]  复位通道
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_reset_device(int dev_id, enum dcmi_reset_channel channel_type);

/**
 * @brief 查询指定设备的静态组件版本
 * @param dev_id                   [in]  指定设备编号
 * @param component_type           [in]  固件类型
 * @param version_str              [out]  用户申请的空间，存放返回的固件版本号
 * @param len                      [in]  version_str的内存大小，大小不能小于64Byte
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_component_static_version(int dev_id, enum dcmi_component_type component_type,
                                                             unsigned char *version_str, unsigned int len);

/**
 * @brief 查询指定设备的cgroup内存信息，包括cgroup最大内存数、历史使用最大内存数、当前使用内存数
 * @param dev_id                   [in]  指定设备编号
 * @param cg_info                  [out]  cgroup信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_cgroup_info(int dev_id, struct dcmi_cgroup_info *cg_info);

/**
 * @brief 查询指定设备的LLC性能参数，包括LLC读命中率、写命中率和吞吐量
 * @param dev_id                   [in]  指定设备编号
 * @param perf_para                [out]  LLC性能参数信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_llc_perf_para(int dev_id, struct dcmi_llc_perf *perf_para);

/**
 * @brief 设置指定设备信息的通用接口，对各模块信息进行配置
 * @param dev_id                   [in]  指定设备编号
 * @param main_cmd                 [in]  模块cmd信息，执行用于获取对应模块的信息
 * @param sub_cmd                  [in]  子cmd命令字
 * @param buf                      [in]  用于配置相应设备的配置信息
 * @param buf_size                 [in]  buf数组的长度
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_set_device_info(int dev_id, enum dcmi_main_cmd main_cmd, unsigned int sub_cmd, const void *buf,
                                         unsigned int buf_size);

/**
 * @brief 查询指定设备信息的通用接口，对各模块信息进行读取
 * @param dev_id                   [in]  指定设备编号
 * @param main_cmd                 [in]  主命令字，模块cmd信息，执行用于获取对应模块的信息
 * @param sub_cmd                  [in]  子命令字
 * @param buf                      [in/out]  用于配置相应设备的配置信息
 * @param size                     [in/out]  buf数组的输入/输出长度
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_info(int dev_id, enum dcmi_main_cmd main_cmd, unsigned int sub_cmd, void *buf,
                                         unsigned int *size);

/**
 * @brief 对指定设备执行密钥吊销
 * @param dev_id                   [in]  指定设备编号
 * @param input_type               [in]  吊销类型
 * @param file_data                [in]  吊销文件的数据地址
 * @param file_size                [in]  吊销文件的数据长度，Soc二级密钥吊销操作的文件长度固定为544字节
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_revoke_device_sec_key(int dev_id, enum dcmi_revo_type input_type,
                                               const unsigned char *file_data, unsigned int file_size);

/**
 * @brief 查询指定设备的MAC地址数量
 * @param dev_id                   [in]  指定设备编号
 * @param count                    [out]  查询出MAC数，取值范围：0~1
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_mac_cnt(int dev_id, int *count);

/**
 * @brief 查询指定设备RoCE网卡的IP地址的连通状态
 * @param dev_id                   [in]  指定设备编号
 * @param result                   [out]  查询RoCE网卡的IP地址的连通状态
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_network_health(int dev_id, enum dcmi_rdfx_detect_result *result);

/**
 * @brief 查询指定设备UBOE端口的IP地址的连通状态
 * @param dev_id                   [in]  指定设备编号
 * @param netdev_name              [in]  UBOE端口的Bond名
 * @param netdev_name_len          [in]  UBOE端口的Bond名长度
 * @param result                   [out]  查询UBOE Bond口的IP地址的连通状态
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_netdev_health(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                                  enum dcmi_rdfx_detect_result *result);

/**
 * @brief 对指定设备设置用户配置。
 * @param dev_id                   [in]  指定设备编号
 * @param config_name              [in]  配置项名称
 * @param buf_size                 [in]  buf长度
 * @param buf                      [in]  buf指针，指向配置项内容
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_set_device_user_config(int dev_id, const char *config_name, unsigned int buf_size, char *buf);

/**
 * @brief 对指定设备查询用户配置
 * @param dev_id                   [in]  指定设备编号
 * @param config_name              [in]  配置项名称
 * @param buf_size                 [in]  buf长度
 * @param buf                      [out]  buf指针，指向配置项内容
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_user_config(int dev_id, const char *config_name, unsigned int buf_size,
                                                unsigned char *buf);

/**
 * @brief 对指定设备重置用户配置
 * @param dev_id                   [in]  指定设备编号
 * @param config_name              [in]  配置项名称
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_clear_device_user_config(int dev_id, const char *config_name);

/**
 * @brief 为指定设备升级vrd固件
 * @param dev_id                   [in]  指定设备编号
 * @param file                     [in]  VRD升级包
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_upgrade_vrd(int dev_id, const char *file);

/**
 * @brief 获取IP地址和mask地址
 * @param dev_id          [in]  芯片逻辑ID
 * @param filter          [in]  筛选事件等级
 * @param handler         [in]  有故障发生后的回调函数
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_subscribe_fault_event(int dev_id, struct dcmi_event_filter filter,
                                               dcmi_fault_event_callback handler);

/**
 * @brief 获取IP地址和mask地址
 * @param dev_id          [in]  芯片逻辑ID
 * @param netdev_name     [in]  bond口名称
 * @param netdev_name_len [in]  bond口名称长度
 * @param ip              [out] IP地址
 * @param mask            [out] mask地址
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_netdev_ip(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                       struct dcmi_ip_addr *ip, struct dcmi_ip_addr *mask);

/**
 * @brief 设置IP地址和mask地址
 * @param dev_id          [in]  芯片逻辑ID
 * @param netdev_name     [in]  bond口名称
 * @param netdev_name_len [in]  bond口名称长度
 * @param ip              [in]  IP地址
 * @param mask            [in]  mask地址
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_set_netdev_ip(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                       struct dcmi_ip_addr *ip, struct dcmi_ip_addr *mask);

/**
 * @brief 设置指定设备的 MAC 地址
 * @param dev_id          [in]  芯片逻辑 ID
 * @param netdev_name     [in]  UBOE端口的Bond名
 * @param netdev_name_len [in]  UBOE端口的Bond名长度
 * @param mac_addr        [in]  MAC 地址
 * @param mac_addr_len    [in]  MAC 地址长度，固定长度 6，单位 byte
 * @return 成功返回 0，错误返回参考 dcmi 故障码定义
 */
DCMIDLLEXPORT int dcmiv2_set_netdev_mac(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                        const char *mac_addr, unsigned int mac_addr_len);

/**
 * @brief 获取指定设备的 MAC 地址
 * @param dev_id          [in]  芯片逻辑 ID
 * @param netdev_name     [in]  UBOE端口的Bond名
 * @param netdev_name_len [in]  UBOE端口的Bond名长度
 * @param mac_addr        [out] MAC 地址
 * @param mac_addr_len    [in]  MAC 地址长度，固定长度 6，单位 byte
 * @return 成功返回 0，错误返回参考 dcmi 故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_netdev_mac(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                        char *mac_addr, unsigned int mac_addr_len);

/**
 * @brief 获取UBOE bond端口网关地址
 * @param dev_id          [in]  芯片逻辑ID
 * @param netdev_name     [in]  UBOE端口名
 * @param netdev_name_len [in]  UBOE端口名长度
 * @param gateway         [out] 网关地址
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_netdev_gateway(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                            struct dcmi_ip_addr *gateway);

/**
 * @brief 设置UBOE bond端口网关地址
 * @param dev_id          [in]  芯片逻辑ID
 * @param netdev_name     [in]  UBOE端口名
 * @param netdev_name_len [in]  UBOE端口名长度
 * @param gateway         [in]  网关地址
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_set_netdev_gateway(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                            struct dcmi_ip_addr *gateway);

/**
 * @brief 查询AICPU信息
 * @param dev_id          [in]  指定设备编号
 * @param aicpu_info      [out]  AICPU信息，信息结构体
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_aicpu_info(int dev_id, struct dcmi_aicpu_info *aicpu_info);

/**
 * @brief 查询芯片的系统时间
 * @param dev_id          [in]  指定设备编号
 * @param system_time     [out] 表示从1970年1月1日00:00:00至今的秒数值
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_system_time(int dev_id, unsigned int *system_time);

/**
 * @brief 根据逻辑ID查询slot id、chip id
 * @param dev_id         [in] NPU逻辑ID
 * @param slot_id        [out] 标识芯片带外位置的slot id
 * @param chip_id        [out] 标识芯片带外位置的chip id
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_slot_id_and_chip_id_by_dev_id(int dev_id, unsigned int *slot_id, unsigned int *chip_id);

/**
 * @brief 根据slot id、chip id查询逻辑ID
 * @param slot_id        [in] 标识芯片带外位置的slot id
 * @param chip_id        [in] 标识芯片带外位置的chip id
 * @param dev_id         [out] NPU逻辑ID
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_dev_id_by_slot_id_and_chip_id(unsigned int slot_id, unsigned int chip_id, int *dev_id);

/**
 * @brief A5标卡多P互联场景下，根据逻辑ID查询当前卡在GROUP组内的序号（4P卡：0-3）、（2P卡：0-1）
 * @param dev_id             [in] NPU逻辑ID
 * @param group_intra_id     [out] 当前卡在GROUP组内的序号
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_group_intra_id_by_dev_id(int dev_id, unsigned int *group_intra_id);

/**
 * @brief 对NPU芯片打流和获取打流结果
 * @param dev_id          [in]  芯片逻辑ID
 * @param operate_para    [in]  prbs码流操作
 * @param operate_result  [out]  查询打流结果或查询prbs链路状态
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_exec_prbs(int dev_id, DCMIV2_PRBS_OPERATE_PARAM v2_operate_para,
                                   DCMI_PRBS_OPERATE_RESULT *operate_result);

/**
 * @brief 获取指定设备到目的地址的链路连通信息
 * @param dev_id          [in]  芯片逻辑ID
 * @param port_id         [in]  NPU设备的网口端口号，当前仅支持配置0
 * @param dcmi_ping       [out]  ping操作信息
 * @param dcmi_reply      [out]  ping结果信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_exec_ping(int dev_id, int port_id, struct dcmi_ping_operate_info *dcmi_ping,
                                   struct dcmi_ping_reply_info *dcmi_reply);

/**
 * @brief 配置traceroute参数探测报文途径的网络节点信息
 * @param dev_id                 [in]  芯片逻辑ID
 * @param netdev_name            [in]  UBOE端口名
 * @param netdev_name_len        [in]  UBOE端口名长度
 * @param traceroute_info        [in]  traceroute接口的入参
 * @param result        [out]  traceroute接口的返回信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_exec_netdev_traceroute(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                                struct dcmi_traceroute_params traceroute_info,
                                                struct dcmi_traceroute_result *result);

/**
 * @brief 获取指定设备的PFC反压帧持续时间统计值
 * @param dev_id               [in]  芯片逻辑ID
 * @param netdev_name          [in]  UBOE端口的Bond名
 * @param netdev_name_len      [in]  UBOE端口的Bond名长度
 * @param pfc_duration_info    [out]  PFC pause duration值
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_pfc_duration_info(int dev_id, const char *netdev_name, unsigned int netdev_name_len,
                                               struct dcmi_netdev_pfc_duration_info *pfc_duration_info);

/**
 * @brief 清除指定设备的PFC反压帧持续时间统计值
 * @param dev_id               [in]  芯片逻辑ID
 * @param netdev_name          [in]  UBOE端口的Bond名
 * @param netdev_name_len      [in]  UBOE端口的Bond名长度
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_clear_pfc_duration(int dev_id, const char *netdev_name, unsigned int netdev_name_len);

/**
 * @brief 查询指定芯片的UB建链状态信息
 * @param dev_id          [in]  芯片逻辑ID
 * @param ub_status       [out] UB建链状态信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_ub_port_link_status(int dev_id, struct dcmi_ub_port_link_status *ub_status);

/**
 * @brief 单次同时获取NPU 、AICore 、AIC、AIV利用率
 * @param dev_id           [in]   指定设备编号
 * @param util_info        [out]  返回设备NPU 、AICore 、AIC、AIV利用率信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_multi_utilization_rate(int dev_id, struct dcmi_multi_utilization_info *util_info);

/**
 * @brief 查询VRD固件版本
 * @param dev_id          [in]  芯片逻辑ID
 * @param version         [out] 用户申请的空间，存放返回的VRD固件版本号
 * @param len             [in]  version空间的最大长度
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_vrd_version(int dev_id, char *version, int len);

/**
 * @brief 查询指定采样时间内，NPU设备的UB端口带宽信息
 * @param dev_id            [in]  芯片逻辑ID
 * @param profiling_time    [in]  采样时间，取值范围1ms~226ms
 * @param ub_port_info      [in]  UB端口信息
 * @param ub_bandwidth_info [out] UB端口带宽信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_ub_realtime_bandwidth_info(int dev_id, unsigned int profiling_time,
                                                        struct dcmi_ub_port_info *ub_port_info,
                                                        struct dcmi_ub_bandwidth_info *ub_bandwidth_info);

/**
 * @brief 清除ECC统计信息
 * @param dev_id            [in]  芯片逻辑ID
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_clear_device_ecc_stats_info(int dev_id);

/**
 * @brief 获取ubping信息
 * @param dev_id            [in]  芯片逻辑ID
 * @param ping_args         [in]  ubping入参信息
 * @param ping_result       [out] ubping结果信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_exec_ub_ping(int dev_id, struct dcmi_ub_ping_operate *ping_args,
                                      struct ub_ping_result *ping_result);

/* 单个EID对综合统计结果 */
struct dcmi_ubping_result {
    struct ubping_pair_info ub_pair_info;
    long max_time;             // ping最大时延
    long min_time;             // ping最小时延
    long avg_time;             // ping平均时延
    long tp95_time;            // ping时延TP95分位数
    unsigned int suc_pkt_num;  // ping成功次数
    unsigned int fail_pkt_num; // ping失败次数
    int reply_stat_num;        // 统计结果所用样本轮数
};

/* ubping mesh 查询返回结果 */
struct dcmi_ubping_mesh_info {
    int result_num; /* 有效EID对测试个数 */
    struct dcmi_ubping_result ub_ping_result[UBPING_MESH_MAX_NUM];
};

struct dcmi_ubping_mesh_params {
    struct ubping_pair_info ub_pair_info[UBPING_MESH_MAX_NUM];
    int pair_num;
    int pkt_size;
    int pkt_send_num;
    int pkt_interval;
    int task_interval;
    unsigned int reserved[UB_PING_RESERVE_NUM];
};

/**
 * @brief 开启ubping mesh常驻任务
 * @param dev_id              [in]设备ID
 * @param dcmi_ubping_mesh_params[in]入参信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_start_ubping_mesh(int dev_id, struct dcmi_ubping_mesh_params *ubping_mesh);

/**
 * @brief 获取ubping mesh统计结果
 * @param dev_id         [in]设备ID
 * @param ubping_mesh_reply [out] 统计结果
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_ubping_mesh_info(int dev_id, struct dcmi_ubping_mesh_info *ubping_mesh_reply);

/**
 * @brief 获取ubping mesh任务状态
 * @param dev_id [in] 设备ID
 * @param state     [out] 任务状态,0停止，1运行
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_ubping_mesh_state(int dev_id, unsigned int *state);

/**
 * @brief 停止ubping mesh常驻任务
 * @param dev_id    [in]  设备ID
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_stop_ubping_mesh(int dev_id);

/**
 * @brief 查询device的port数量和基本信息
 * @param dev_id            [in]  芯片逻辑ID
 * @param port_list_info    [out] device可用的port数量和基本信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_port_list_info(int dev_id, struct dcmi_port_list_info *port_list_info);

/**
 * @brief 获取ub端口信息
 * @param dev_id              [in]  芯片逻辑ID
 * @param ub_port_info        [in]  UB端口信息
 * @param port_pkt_stats_info [out] UB端口统计信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_port_pkt_stats_info(int dev_id, struct dcmi_ub_port_info *ub_port_info,
                                                 struct dcmi_port_pkt_stats_info *port_pkt_stats_info);

/**
 * @brief 获取ub端口信息
 * @param dev_id              [in]  芯片逻辑ID
 * @param ub_port_info        [in]  UB端口信息
 * @param dcmi_credit_info    [out] UB端口可用信息证信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_port_avail_credit_info(int dev_id, struct dcmi_ub_port_info *ub_port_info,
                                                    struct dcmi_credit_info *port_avail_credit_info);

/**
 * @brief 获取netdev数量和基本信息
 * @param dev_id              [in]  芯片逻辑ID
 * @param netdev_list         [out] 可用netdev信息
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_device_netdev_list_info(int dev_id, struct dcmi_netdev_list_info *netdev_list);

/**
 * @brief 获取IAK证书
 * @param dev_id            [in]  芯片逻辑ID
 * @param ak_cert           [out] IAK证书获取返回值
 * @param ak_cert_len       [in/out]  返回IAK证书长度
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_attest_akcert(int dev_id, unsigned char *ak_cert, unsigned int *ak_cert_len);

/**
 * @brief 获取指定的度量日志类型
 * @param dev_id            [in]  芯片逻辑ID
 * @param ctx               [in]  存储上下文，包含日志类型和挑战值
 * @param evidence          [out] 返回指定的度量日志
 * @param evidence_len      [in/out] 返回证据长度
 * @return 成功返回0，错误返回参考dcmi故障码定义
 */
DCMIDLLEXPORT int dcmiv2_get_attest_evidence(int dev_id, struct attest_ctx *ctx, unsigned char *evidence,
                                             unsigned int *evidence_len);
#endif

DCMIDLLEXPORT int dcmi_get_hccs_link_bandwidth_info(int card_id, int device_id,
                                                    struct dcmi_hccs_bandwidth_info *hccs_bandwidth_info);

DCMIDLLEXPORT int dcmi_get_ub_realtime_bandwidth_info(int card_id, int device_id, unsigned int profiling_time,
                                                      struct dcmi_ub_port_info *ub_port_info,
                                                      struct dcmi_ub_bandwidth_info *ub_bandwidth_info);

DCMIDLLEXPORT int dcmi_get_netdev_brother_device(int card_id, int device_id, int *brother_card_id);

DCMIDLLEXPORT int dcmi_get_ub_port_link_status_info(int card_id, int device_id,
                                                    struct dcmi_ub_port_link_status *ub_status);

#if defined DCMI_VERSION_1
/* The following interfaces are V1 version interfaces. In order to ensure the compatibility is temporarily reserved,
 * the later version will be deleted. Please switch to the V2 version interface as soon as possible */

struct dcmi_tag_pcie_idinfo {
    unsigned int deviceid;
    unsigned int venderid;
    unsigned int subvenderid;
    unsigned int subdeviceid;
    unsigned int bdf_deviceid;
    unsigned int bdf_busid;
    unsigned int bdf_funcid;
};

struct dcmi_board_info_stru {
    unsigned int board_id;
    unsigned int pcb_id;
    unsigned int bom_id;
    unsigned int slot_id;
};

typedef struct dcmi_elabel_info_stru {
    char product_name[MAX_LENTH];
    char model[MAX_LENTH];
    char manufacturer[MAX_LENTH];
    char serial_number[MAX_LENTH];
} DCMI_ELABEL_INFO_STRU, *PDCMI_ELABEL_INFO_STRU;

struct dcmi_chip_pcie_err_rate_stru {
    unsigned int reg_deskew_fifo_overflow_intr_status;
    unsigned int reg_symbol_unlock_intr_status;
    unsigned int reg_deskew_unlock_intr_status;
    unsigned int reg_phystatus_timeout_intr_status;
    unsigned int symbol_unlock_counter;
    unsigned int pcs_rx_err_cnt;
    unsigned int phy_lane_err_counter;
    unsigned int pcs_rcv_err_status;
    unsigned int symbol_unlock_err_status;
    unsigned int phy_lane_err_status;
    unsigned int dl_lcrc_err_num;
    unsigned int dl_dcrc_err_num;
};

struct dcmi_soc_die_stru {
    unsigned int soc_die[5];
};

struct dcmi_flash_info_stru {
    unsigned long long flash_id;    /* combined device & manufacturer code */
    unsigned short device_id;       /* device id */
    unsigned short vendor;          /* the primary vendor id */
    unsigned int state;             /* flash health */
    unsigned long long size;        /* total size in bytes */
    unsigned int sector_count;      /* number of erase units */
    unsigned short manufacturer_id; /* manufacturer id */
};

struct dcmi_ecc_info_stru {
    int enable_flag;
    unsigned int single_bit_error_count;
    unsigned int double_bit_error_count;
};

struct dcmi_memory_info_stru {
    unsigned long long memory_size; // 单位 MB
    unsigned int freq;
    unsigned int utiliza;
};

#ifndef __DSMI_COMMON_INTERFACE_H__

#define DMS_MAX_EVENT_NAME_LENGTH 256
#define DMS_MAX_EVENT_DATA_LENGTH 32
#define DMS_MAX_EVENT_RESV_LENGTH 32
#define TAISHAN_CORE_NUM 16

struct dsmi_soc_die_stru {
    unsigned int soc_die[5];
};

struct dsmi_board_info_stru {
    unsigned int board_id;
    unsigned int pcb_id;
    unsigned int bom_id;
    unsigned int slot_id;
};

struct tag_pcie_idinfo {
    unsigned int deviceid;
    unsigned int venderid;
    unsigned int subvenderid;
    unsigned int subdeviceid;
    unsigned int bdf_deviceid;
    unsigned int bdf_busid;
    unsigned int bdf_funcid;
};

struct dm_flash_info_stru {
    unsigned long flash_id;         /* combined device & manufacturer code */
    unsigned short device_id;       /* device id */
    unsigned short vendor;          /* the primary vendor id */
    unsigned int state;             /* flash health, 0x8:normal,0x10:abnormal */
    unsigned long size;             /* total size in bytes */
    unsigned int sector_count;      /* number of erase units */
    unsigned short manufacturer_id; /* manufacturer id */
};

struct dsmi_aicore_info_stru {
    unsigned int freq;    /* normal freq */
    unsigned int curfreq; /* current freq */
};

typedef struct dsmi_aicpu_info_stru {
    unsigned int maxFreq;
    unsigned int curFreq;
    unsigned int aicpuNum;
    unsigned int utilRate[TAISHAN_CORE_NUM];
} DSMI_AICPU_INFO;

struct dsmi_ecc_info_stru {
    int enable_flag;
    unsigned int single_bit_error_count;
    unsigned int double_bit_error_count;
};

struct dsmi_ecc_pages_stru {
    unsigned int corrected_ecc_errors_aggregate_total;   // 生命周期内所有可纠正ecc错误统计
    unsigned int uncorrected_ecc_errors_aggregate_total; // 生命周期内所有不可纠正ecc错误统计
    unsigned int isolated_pages_single_bit_error;        // 单bit错误隔离内存页数量
    unsigned int isolated_pages_double_bit_error;        // 多bit错误隔离内存页数量
};

struct dsmi_hbm_info_stru {
    unsigned long long memory_size;  /**< HBM total size, KB */
    unsigned int freq;               /**< HBM freq, MHZ */
    unsigned long long memory_usage; /**< HBM memory_usage, KB */
    int temp;                        /**< HBM temperature */
    unsigned int bandwith_util_rate;
};

struct dsmi_memory_info_stru {
    unsigned long long memory_size;
    unsigned int freq;
    unsigned int utiliza;
};

struct dsmi_computing_power_info {
    unsigned int data1;
    unsigned int reserve[3];
};

enum dsmi_event_type {
    DMS_FAULT_EVENT = 0,
    DSMI_EVENT_TYPE_MAX
};

struct dms_fault_event {
    unsigned int event_id;
    unsigned short deviceid;
    unsigned char node_type;
    unsigned char node_id;
    unsigned char sub_node_type;
    unsigned char sub_node_id;
    unsigned char severity;
    unsigned char assertion;
    int event_serial_num;
    int notify_serial_num;
    unsigned long long alarm_raised_time;
    char event_name[DMS_MAX_EVENT_NAME_LENGTH];
    char additional_info[DMS_MAX_EVENT_DATA_LENGTH];
    unsigned char os_id;
    unsigned char resv_1[1]; /* reserve 1 byte for byte alignment */
    unsigned short node_type_ex;
    unsigned short sub_node_type_ex;
    unsigned char resv_2[2]; /* reserve 2 byte for byte alignment */

    /* os_id occupy 1byte, node_type_ex,sub_node_type_ex occupy 4byte, reserve 24byte */
    unsigned char resv[DMS_MAX_EVENT_RESV_LENGTH - 8];
};

struct dsmi_event {
    enum dsmi_event_type type;
    union {
        struct dms_fault_event dms_event;
    } event_t;
};

#endif /* __DSMI_COMMON_INTERFACE_H__ */

typedef void (*fault_event_callback)(struct dsmi_event *event);

DCMIDLLEXPORT int dcmi_get_card_num_list(int *card_num, int *card_list, int list_len);

DCMIDLLEXPORT int dcmi_get_pcie_info(int card_id, int device_id, struct dcmi_tag_pcie_idinfo *pcie_idinfo);

DCMIDLLEXPORT int dcmi_get_board_info(int card_id, int device_id, struct dcmi_board_info_stru *board_info);

DCMIDLLEXPORT int dcmi_get_card_elabel(int card_id, struct dcmi_elabel_info_stru *elabel_info);

DCMIDLLEXPORT int dcmi_mcu_set_license_info(int card_id, char *license, int len);

DCMIDLLEXPORT int dcmi_mcu_get_license_info(int card_id, char *data_info, int *len);

DCMIDLLEXPORT int dcmi_get_customized_info_api(int card_id, char *data_info, int *len);

DCMIDLLEXPORT int dcmi_set_customized_info_api(int card_id, const char *data_info, int len);

DCMIDLLEXPORT int dcmi_clear_pcie_error_cnt(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_get_pcie_error_cnt(int card_id, int device_id,
                                          struct dcmi_chip_pcie_err_rate_stru *pcie_err_code_info);

DCMIDLLEXPORT int dcmi_get_device_die(int card_id, int device_id, struct dcmi_soc_die_stru *device_die);

DCMIDLLEXPORT int dcmi_get_device_ndie(int card_id, int device_id, struct dsmi_soc_die_stru *device_ndie);

DCMIDLLEXPORT int dcmi_get_device_errorcode(int card_id, int device_id, int *error_count, unsigned int *error_code,
                                            int *error_width);

DCMIDLLEXPORT int dcmi_get_device_errorinfo(int card_id, int device_id, int errorcode, unsigned char *errorinfo,
                                            int buf_size);

DCMIDLLEXPORT int dcmi_get_device_flash_info(int card_id, int device_id, unsigned int flash_index,
                                             struct dcmi_flash_info_stru *flash_info);

DCMIDLLEXPORT int dcmi_get_aicore_info(int card_id, int device_id, struct dsmi_aicore_info_stru *aicore_info);

DCMIDLLEXPORT int dcmi_get_aicpu_info(int card_id, int device_id, struct dsmi_aicpu_info_stru *aicpu_info);

DCMIDLLEXPORT int dcmi_pre_reset_soc(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_rescan_soc(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_reset_device(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_mcu_get_upgrade_statues(int card_id, int *status, int *progress);

DCMIDLLEXPORT int dcmi_mcu_get_upgrade_status(int card_id, int *status, int *progress);

DCMIDLLEXPORT int dcmi_mcu_get_version(int card_id, char *version_str, int max_version_len, int *len);

DCMIDLLEXPORT int dcmi_mcu_upgrade_control(int card_id, int upgrade_type);

DCMIDLLEXPORT int dcmi_mcu_upgrade_transfile(int card_id, const char *file);

DCMIDLLEXPORT int dcmi_get_p2p_enable(int card_id, int device_id, int *enable_flag);

DCMIDLLEXPORT int dcmi_get_ecc_info(int card_id, int device_id, int device_type,
                                    struct dsmi_ecc_info_stru *device_ecc_info);

DCMIDLLEXPORT int dcmi_get_hbm_info(int card_id, int device_id, struct dsmi_hbm_info_stru *device_hbm_info);

DCMIDLLEXPORT int dcmi_get_memory_info(int card_id, int device_id, struct dcmi_memory_info_stru *device_memory_info);

DCMIDLLEXPORT int dcmi_get_device_hbm_product_info(int card_id, int device_id,
                                                   struct dcmi_hbm_product_info *hbm_product_info);

DCMIDLLEXPORT int dcmi_get_soc_sensor_info(int card_id, int device_id, int sensor_id,
                                           union tag_sensor_info *sensor_info);

DCMIDLLEXPORT int dcmi_config_ecc_enable(int card_id, int device_id, int enable_flag);

DCMIDLLEXPORT int dcmi_get_version(int card_id, int device_id, char *verison_str, unsigned int version_len, int *len);

DCMIDLLEXPORT int dcmi_mcu_get_board_info(int card_id, struct dcmi_board_info *board_info);

DCMIDLLEXPORT int dcmi_mcu_get_power_info(int card_id, int *power);

DCMIDLLEXPORT int dcmi_get_computing_power_info(int card_id, int device_id, int type,
                                                struct dsmi_computing_power_info *computing_power);

DCMIDLLEXPORT int dcmi_set_device_ecc_enable(int card_id, int device_id, enum dcmi_device_type device_type,
                                             int enable_flag);

DCMIDLLEXPORT int dcmi_set_user_config(int card_id, int device_id, const char *config_name, unsigned int buf_size,
                                       unsigned char *buf);

DCMIDLLEXPORT int dcmi_get_user_config(int card_id, int device_id, const char *config_name, unsigned int buf_size,
                                       unsigned char *buf);

DCMIDLLEXPORT int dcmi_clear_device_user_config(int card_id, int device_id, const char *config_name);

DCMIDLLEXPORT int dcmi_reset_device_inband(int card_id, int device_id);

DCMIDLLEXPORT int dcmi_get_nve_level(int card_id, int device_id, int *nve_level);

DCMIDLLEXPORT int dcmi_set_nve_level(int card_id, int device_id, int level);

DCMIDLLEXPORT int dcmi_get_system_time(int card_id, int device_id, unsigned int *time);

DCMIDLLEXPORT int dcmi_get_all_device_count(int *all_device_count);

DCMIDLLEXPORT int dcmi_get_extra_statistics_info(int card_id, int device_id, int port_id,
                                                 struct dcmi_extra_statistics_info *info);

DCMIDLLEXPORT int dcmi_get_vrd_version(int card_id, char *version, int len);

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* __DCMI_INTERFACE_API_H__ */
