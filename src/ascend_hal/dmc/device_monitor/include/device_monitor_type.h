/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEVICE_MONITOR_TYPE_H
#define DEVICE_MONITOR_TYPE_H
#include <sys/time.h>
#include "hash_table.h"
#include "dm_common.h"

#define PTHREAD_STOP_FLAG 0
#define PTHREAD_RUNNING_FLAG 1
#define PTHREAD_STOPED_FLAG 2

#define DEVICE_MONITOR_TAG 0X55AFAA55

#define DM_MODULE_PORT 0x1020
#define DM_HDC_TARGET 2

#ifndef DM_UDP_PORT
#define DM_UDP_PORT 8087
#endif

#define DEVICE_MONITOR_IDENTIFY_CODE 0XEEE

#define DEVICE_MONITOR_DMP_VERSION_OFFSET 12
#define DEVICE_MONITOR_DMP_VERSION 0X1

#define DEVICE_MONITOR_DEVICE_TYPE 0X5

#define DEV_MON_MAIN_QUE "dem_mon_man_que"
#define DEV_MON_MAIN_TASK "dem_mon_man_task"
#define LIB_DFT_SO_PATH "/lib64/libascend_drvdft.so"
#define LIB_DFT_INIT_FUN_NAME "soc_testlib_init"

#ifndef OK
#define OK 0
#endif

#ifndef FAILED
#define FAILED (-1)
#endif

#ifndef DEV_MON_PARA_ERR
#define DEV_MON_PARA_ERR 0X1
#endif

#ifndef DEV_MON_CMD_NOT_SUPPORT
#define DEV_MON_CMD_NOT_SUPPORT 0X2
#endif

#ifndef DEV_MON_CMD_INTERNAL_ERROR
#define DEV_MON_CMD_INTERNAL_ERROR 0X3
#endif

#ifndef DEV_MON_MSG_QUEUE_BREAK
#define DEV_MON_MSG_QUEUE_BREAK 0x04
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAX_OUTPUT_LEN
#define MAX_OUTPUT_LEN 0XC000U
#endif

#pragma pack(1)

#define DEV_MON_REQUEST_HEAD_LEN 12U

typedef struct tg_dev_mp_msg {
    unsigned char lun;
    unsigned char op_type;
    unsigned char op_cmd;
    unsigned char op_fun;
    unsigned int offset;
    unsigned int length;
    char msg[0];
} DEV_MP_MSG_ST;

typedef struct tg_msg_ctl {
    unsigned int inter_type;
    void *addr_in;
    void *addr_to;
    unsigned int data_len;
    DEV_MP_MSG_ST mp;
} MSG_CTL_ST;

typedef struct tg_dev_bd_msg {
    void *msg_in_intf;
    void *msg_out_intf;
    void *msg;
} DEV_VD_MSG;

typedef struct tg_bd_msg {
    unsigned char msgType;
    DM_INTF_S *intf;
    DM_RECV_ST *lpPara;
} BD_MSG_ST;

typedef struct tag_system_globals_t {
    hash_table *cmdhashtable;
    unsigned long event_que;
    void *msg_cb;
    hash_table *rsp_hashtable;
    hash_table *req_hashtable;
    unsigned int i2c_heartbeat_cnt;
} SYSTEM_CB_T;

typedef struct dev_mon_resp_tag_value {
    unsigned char op_fun;
    unsigned char op_cmd;
    unsigned int total_len;
    char *send_data;
    struct timespec time;
} DEV_MON_RESP_TAG_VALUE_ST;

typedef struct dev_mon_client_resp_tag_value {
    unsigned char op_fun;
    unsigned char op_cmd;
    unsigned int data_len;
    char *data;
} DEV_MON_CLIENT_RESP_TAG_VALUE_ST;

typedef struct send_ctl_cb {
    DM_INTF_S *intf;
    DM_ADDR_ST addr;
    unsigned int addr_len;
    DM_MSG_ST *msg;
    DM_MSG_CMD_HNDL_T rsp_hndl;
    void *user_data;
    int data_len;
    struct send_ctl_cb *next;
    LIST_NODE_T *split_msg_node;
} SEND_CTL_CB;

typedef struct dev_mon_request_tag_value {
    unsigned char op_fun;
    unsigned char op_cmd;
    unsigned int data_len;
    char *data;
} DEV_MON_REQUEST_TAG_VALUE_ST;

#define REQ_D_INFO_INFO_TYPE_RATE 0
#define REQ_D_INFO_INFO_TYPE_FREQ 1
#define REQ_D_INFO_INFO_TYPE_MEM_SIZE 2
#define REQ_D_INFO_INFO_TYPE_HBM_USAGE 3

enum davinchi_info_enum {
    REQ_D_INFO_DEV_TYPE_MEM = 1, // DDR
    REQ_D_INFO_DEV_TYPE_CPU = 2, // AI core and AI Vector accelerator old util
    REQ_D_INFO_DEV_TYPE_AI_CPU = 3,
    REQ_D_INFO_DEV_TYPE_CTRL_CPU = 4,
    REQ_D_INFO_DEV_TYPE_MEM_BANDWIDTH = 5,
    REQ_D_INFO_DEV_TYPE_HBM = 6,
    REQ_D_INFO_DEV_TYPE_AICORE0 = 7, // cloud only use this
    REQ_D_INFO_DEV_TYPE_DDR = 8,
    REQ_D_INFO_DEV_TYPE_AICORE1 = 9,
    REQ_D_INFO_DEV_TYPE_HBM_BW = 10,
    REQ_D_INFO_DEV_TYPE_AICORE = 11,
    REQ_D_INFO_DEV_TYPE_VECTOR = 12,
    REQ_D_INFO_DEV_TYPE_ACC = 13,     // AI core and AI vector accelerator new util
    REQ_D_INFO_DEV_TYPE_AI_CUBE = 14, // AI cube accelerator
    REQ_D_INFO_DEV_TYPE_MAX_INVALID_VALUE
};

typedef union {
    struct {
        unsigned char dev_type : 4;
        unsigned char info_type : 4;
    } bits;
    unsigned char uchar;
} REQ_CMD_D_INFO_ARG;

typedef union {
    struct {
        unsigned char chan_num : 4;
        unsigned char recv : 3;
        unsigned char rw : 1;
    } bits;
    unsigned char uchar;
} REQ_PASSTHRU_ARG;

#define REQ_FLASH_DEVICE_INFO_COUNT_FLAG 0XFF

typedef signed int (*SOC_TESTLIB_INIT)(void);

#define REQ_ECC_STATUS_ERR_TYPE_ALL 0
#define REQ_ECC_STATUS_ERR_TYPE_SINGLE 1
#define REQ_ECC_STATUS_ERR_TYPE_DOUBLE 2

#define REQ_ECC_STATUS_DEV_TYPE_DDR 0
#define REQ_ECC_STATUS_DEV_TYPE_L2BUF 1
#define REQ_ECC_STATUS_DEV_TYPE_SRAM 2
#define REQ_ECC_STATUS_DEV_TYPE_CACHE 3
#define REQ_ECC_STATUS_DEV_TYPE_HBM 4

#define ELABEL_SET_ARG_INDEX 0
#define ELABEL_ITME_ID_INDEX 1
#define ELABEL_DATA_INDEX 2
#define ELABEL_LEN_INDEX 6

#define WRITE_ELABEL_ARG 0x0
#define CLEAR_ELABEL_ARG 0x1
#define UPDATE_ELABEL_ARG 0x2

#define VERSION_RETURN 2
#define HELP_RETURN 1

#define HEAD_LUN 0X80
#define LUN_TAIL_MASK 0X80
#define MINI2MCU_READ 0
#define DELAY_TIME_1S 1000
#define DELAY_TIME_800MS 800
#define MAX_COUNT 9999
#define PCIE_TYPE_A 1
#define PCIE_TYPE_B 3
#define PCIE_TYPE_C 5
#define PCIE_TYPE_MIN 200
#define PCIE_TYPE_MAX 299

#define MINI1 1
#define MINI3 3
#define SEND_LENGHT 4
#define SEND_MCU_NPU_STATUS_LENGTH 8
#define NOTIFY2MCU_WRITE 0
#define NPU_NOTIFY_INDEX 1

#define NPU_DOWN 0x00
#define NPU_UP 0x01

#define HBM_UTILIZATION 2

typedef union {
    struct {
        unsigned char dev_type : 6;
        unsigned char err_type : 2;
    } bits;
    unsigned char uchar;
} REQ_CMD_ECC_STATUS_ARG;

typedef struct notify_mcu_device_status_data {
    unsigned char lun;
    unsigned char op_type;
    unsigned char op_cmd;
    unsigned char op_fun;
    unsigned int offset;
    unsigned int length;
    char msg[SEND_MCU_NPU_STATUS_LENGTH];
} NOTIFY_MCU_ST;

#pragma pack()

#define DMP_ONE_STATICSTIC_PERIOD 64U /* The duration of each period is 64 seconds, it must be 2^n */
#define DMP_STATICSTIC_PERIOD_NUM 4U  /* DMP_STATICSTIC_PERIOD_NUM must be 2^n */
#define DMP_TATOL_STATICSTIC_PERIOD (DMP_ONE_STATICSTIC_PERIOD * DMP_STATICSTIC_PERIOD_NUM)

typedef void (*dev_mon_cmd_handle)(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *data);
typedef struct dev_mon_cmd_tag_value {
    unsigned char op_fun;
    unsigned char op_cmd;
    unsigned char op_prop;
    unsigned char op_uid;
    unsigned char op_type;
    dev_mon_cmd_handle handle;
    unsigned long long op_called_cnt[DMP_STATICSTIC_PERIOD_NUM];
    unsigned long long op_start_time[DMP_STATICSTIC_PERIOD_NUM];
    unsigned long long timeout_count;
    unsigned long long max_timeout;
    pthread_mutex_t op_mutex;
} DEV_MON_CMD_TAG_VALUE_ST;

#endif
