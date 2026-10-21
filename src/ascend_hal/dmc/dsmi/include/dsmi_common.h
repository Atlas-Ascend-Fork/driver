/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DSMI_COMMON_H__
#define __DSMI_COMMON_H__
#include <limits.h>
#include <semaphore.h>
#include <time.h>
#include <linux/limits.h>

#include "dm_common.h"
#include "device_monitor_type.h"
#include "dsmi_common_interface.h"
#include "dmc/dev_mon_cmd_def.h"
#include "dev_mon_dmp_client.h"
#include "dev_mon_log.h"
#include "dms_user_interface.h"

#define __stringify_1(x...) #x
#define __stringify(x...) __stringify_1(x)

#define CMD_LENGTH_INVALID 0XFFFF
#define INVALID_OPCODE 0xFF

#define FLASH_CONTENT_SIZE_MAX 32768
#define DSMI_SET_ALL_DEVICE 0xFFFFFFFFU

#define DSMI_CMD_DEF_COMMON_INSTANCE(op_cmd, in_len, rsp_len, optype) \
    static DMP_CMD_DEF g_dmp_cmd_def_##op_cmd = {                     \
        .name = (const unsigned char *)#op_cmd,                       \
        .opcode = (DEV_MON_SMB_FUN_CODE_COMMON << 8 | (op_cmd)),      \
        .length = (in_len),                                           \
        .resp_len = (rsp_len),                                        \
        .op_type = (optype),                                          \
    };

#define DSMI_CMD_DEF_DAVINCI_INSTANCE(op_cmd, in_len, rsp_len, optype) \
    static const DMP_CMD_DEF g_dmp_cmd_def_##op_cmd = {                \
        .name = (const unsigned char *)#op_cmd,                        \
        .opcode = (DEV_MON_SMB_FUN_CODE_DAVINCI << 8 | (op_cmd)),      \
        .length = (in_len),                                            \
        .resp_len = (rsp_len),                                         \
        .op_type = (optype),                                           \
    };

#define MAX_PATH_LEN (PATH_MAX - NAME_MAX)
#define COPY_LEN 10

#define MAX_INPUT_LEN 0XC000U

#ifdef DEV_MON_UT
#define STATIC
#define dsb(opt)

#else
#define STATIC static

#if defined(__AARCH64_CMODEL_SMALL__) && __AARCH64_CMODEL_SMALL__
#define dsb(opt)                                  \
    {                                             \
        asm volatile("dsb " #opt : : : "memory"); \
    }
#else
#define dsb(opt)
#endif

#endif

#define rmb() dsb(ld) /* read fence */
#define wmb() dsb(st) /* write fence */
#define mb() dsb(sy)  /* rw fence */

#define P2P_ENABLE_RANGE 1
#define ECC_ENABLE_RANGE 1
#define LOG_LEVEL_RANGE 3

#ifdef CFG_SOC_PLATFORM_MINI
#define BUF_MAX_LEN 1024
#else
#define BUF_MAX_LEN 0x8000
#endif

#define CONFIG_NAME_LEN 64
#define CHECK_LEN 1
#define COPY_LEN 10
#define CONFIG_FIND 1

#define DSMI_UPGRADE_LOCK_TAG 0X1ABC2
#define DSMI_FLASH_LOCK_TAG 0X72D4F
#define DSMI_DFT_UDP_PORT 0
#define DSMI_DFT_DEST_PORT 8087

#define MEMORY_4G 4
#define MEMORY_8G 8
#define MEMORY_16G 16
#define GB_TO_MB 1024
#define GET_FAN_COUNT 0X00
#define GET_FAN_SPEED 0X01

#define SET_AVERAGE_FAN_NUM 0X0
#define DEVDRV_MIN_DAVINCI_NUM 0X00

#define ERROR_CODE_MAX_NUM 128

#define ROOT_USER 0

#define DSMI_USER_CONFIG_NAME_MAX 32

#define REVOCATION_FILE_LEN_MAX 0x200000
#define UPGRADE_DIR_NAME "/upgrade/"
#define USHORT_MAX 65535
#define INPUT_SIZE_MAX 128

#define DM_USER "HwDmUser"
#define USER_ID 1000
#define GROUP_ID 1000

#define LOCAL_COPY_FILE_SIZE_MAX 0x10000000 /* local copy file size max, 256 MB */

// used for dsmi_set_power_state
#define LIB_SYS_CTR_SO_PATH "/lib64/libsys_ctr.so.1"
#define SYS_REBOOT "SysReboot"
#define SYS_SHUTDOWN "SysShutdown"

#ifndef ERESTARTSYS
#define ERESTARTSYS 512
#endif

typedef signed int (*SYS_CMD_INTERFACE)(void);

#pragma pack(1)
typedef struct dsmi_cmd_code {
    unsigned char lun;
    unsigned char optype;
    unsigned short opcode;
    unsigned int offset;
    unsigned int length;
    char send_data[];
} DSMI_CMD_CODE;

typedef struct dsmi_dft_res_cmd {
    unsigned short error_code;
    unsigned short opcode;
    unsigned int total_length;
    unsigned int length;
    char response_data[];
} DSMI_DFT_RES_CMD;
typedef union dsmi_davinchi_info {
    struct {
        unsigned char device_type : 4;
        unsigned char info_type : 4;
    } info;
    unsigned char data;
} DSMI_DAVINCHI_INFO;

typedef struct dsmi_flash_info {
    unsigned char flash_id;
} DSMI_FLASH_INFO;

typedef union dsmi_ecc_statics {
    struct {
        unsigned char device_type : 6;
        unsigned char error_type : 2;
    } info;
    unsigned char udata;
} DSMI_ECC_STATICS;

typedef struct dsmi_health_state {
    unsigned char health_state;
} DSMI_HEALTH_STATE;

typedef struct dsmi_error_code {
    int error_count;
    unsigned int errorcode[ERROR_CODE_MAX_NUM];
} DSMI_ERROR_CODE;

typedef struct dsmi_chip_temp {
    signed short chip_temp;
} DSMI_CHIP_TEMP;

typedef struct dsmi_power {
    unsigned short power;
} DSMI_POWER;

typedef struct dsmi_venderid {
    unsigned short vender_id;
} DSMI_VENDER_ID;

typedef struct dsmi_deviceid {
    unsigned short device_id;
} DSMI_DEVICE_ID;

typedef struct dsmi_sub_venderid {
    unsigned short sub_vender_id;
} DSMI_SUB_VENDER_ID;

typedef struct dsmi_sub_deviceid {
    unsigned short sub_device_id;
} DSMI_SUB_DEVICE_ID;

typedef struct dsmi_board_id {
    unsigned short board_id;
} DSMI_BOARD_ID;

typedef struct dsmi_pcb_id {
    unsigned char pcb_id;
} DSMI_PCB_ID;

typedef struct dsmi_board_info {
    unsigned int id_value;
} DSMI_BOARD_INFO;

typedef struct dsmi_board_info_req {
    unsigned char info_type;
} DSMI_BOARD_INFO_REQ;

typedef struct dsmi_chip_voltage {
    unsigned short chip_voltage;
} DSMI_CHIP_VOLTAGE;

typedef struct dsmi_davinchi_info_data {
    unsigned int davinchi_info;
} DSMI_DAVINCHI_INFO_DATA;

typedef struct dsmi_flash_basic_info {
    unsigned int flash_cnt;
} DSMI_FLASH_BASIC_INFO;

typedef struct dsmi_die_id {
    unsigned char dir_id[32]; // 32 dir_id size
} DSMI_DIE_ID;

typedef struct dsmi_ecc_statics_result {
    unsigned int single_bit_error_count;
    unsigned int double_bit_error_count;
} DSMI_ECC_STATICS_RESULT;

typedef struct dsmi_device_flash_info {
    unsigned long flash_id;         /* combined device & manufacturer code */
    unsigned short device_id;       /* device id    */
    unsigned short vendor;          /* the primary vendor id  */
    unsigned int state;             /* flash health */
    unsigned long size;             /* total size in bytes  */
    unsigned int sector_count;      /* number of erase units  */
    unsigned short manufacturer_id; /* manufacturer id   */
} DSMI_DEVICE_FLASH_INFO;

typedef struct dsmi_llc_rx_result {
    unsigned int dev_id;
    unsigned int wr_hit_rate;
    unsigned int rd_hit_rate;
    unsigned int throughput;
} DSMI_LLC_RX_RESULT;

#define ELABLE_DATA_MAX_LENGTH 64
#define FW_VERSION_MAX_LENGTH 64

typedef struct dsmi_device_support_component {
    unsigned long long component_type;
} DSMI_DEVICE_SUPPORT_COMPONENT;

typedef enum dsmi_update_control {
    UPGRADE_PREPARE = 0x1,
    GET_SUPPORT_COMPONENT,
    TRANSMIT_FILE,
    START_UPDATE,
    STOP_UPDATE,
    UPDATE_INVALID,
    CHANGE_IMAGE,
    REVERT_UPDATE,
    START_UPDATE_AND_RESET,
    START_SYNC,
    START_SYNC_RECOVERY,
    START_SYNC_FIRMWARE,
    START_UPDATE_STATE,
    UPGRADE_PATCH,
    UNLOAD_PATCH,
    UPGRADE_MAMI_PACH,
} DSMI_UPDATE_CONTROL;

typedef enum {
    DSMI_UPGRADE_STATE_IDLE,
    DSMI_UPGRADE_STATE_UPDATING,
    DSMI_UPGRADE_STATE_NONSUPPORT,
    DSMI_UPGRADE_STATE_FAIL,
    DSMI_UPGRADE_STATE_NONE
} DSMI_DEV_UPGRADE_STATE;

typedef struct dsmi_upgrade_state {
    unsigned char component_type;
} DSMI_UPGRADE_STATE;

typedef struct dsmi_davinci_version {
    unsigned char component_type;
} DSMI_DAVINCI_VERSION;

typedef struct dsmi_user_data {
    DSMI_DFT_RES_CMD *dsmi_dft_res;
    unsigned int data_len;
    unsigned long sem_id;
} DSMI_USER_DATA;

typedef struct dsmi_upgrade_state_res {
    unsigned char status;
    unsigned char schedule;
} DSMI_UPGRADE_STATE_RES;

typedef enum {
    BASIC_MAC_TYPE = 0X0,
    ROCE_MAC_TYPE = 0X1
} MAC_TYPE;

#pragma pack()

typedef enum dsmi_start_end_test {
    DSMI_STOP_TEST = 0,
    DSMI_START_TEST
} DSMI_START_END_TEST;

typedef struct dsmi_dmp_command_st {
    unsigned int device_index;
    unsigned short op_code;
    DM_MSG_ST send_msg;
    DM_MSG_ST recv_msg;
    unsigned int recv_success;
} DSMI_DMP_COMMAND_ST;

typedef struct dsmi_command_ctl_st {
    DSMI_DMP_COMMAND_ST *dmp_cmd;
    unsigned long sem_id;
} DSMI_COMMAND_CTL_ST;

typedef struct dmp_cmd_def {
    const unsigned char *name;
    unsigned short opcode;
    unsigned int length;
    unsigned int resp_len;
    unsigned int op_type;
} DMP_CMD_DEF;

typedef enum {
    CORE_NUM_A_INDEX = 1,
    CORE_NUM_B_INDEX,
    CORE_NUM_C_INDEX,
    CORE_NUM_LEVEL_MAX
} AICORE_NUM_LEVEL;

typedef enum {
    FREQ_LITE_INDEX = 1,
    FREQ_PRO_INDEX,
    FREQ_PRE_INDEX,
    CORE_FREQ_LEVEL_MAX,
} AICORE_FREQ_LEVEL;

typedef enum {
    MAIN_CMD_TYPE_NONE = 0,
    MAIN_CMD_TYPE_PRODUCT,
    MAIN_CMD_TYPE_HOST_DMP,
    MAIN_CMD_TYPE_HOST_DEVMNG,
} DEV_INFO_MAIN_CMD_TYPE;

#define TIMEOUT_MS 60000
#ifdef DRV_HOST
#define WARN_COST_TIME 4000 /* warn time between DM_COMMAND_BIGIN to DM_COMMAND_END 4000 ms */
#else
#define WARN_COST_TIME 500 /* warn time between DM_COMMAND_BIGIN to DM_COMMAND_END 500 ms */
#endif

struct dsmi_udis_ecc_count_info {
    DSMI_ECC_STATICS_RESULT ecc_result;
    unsigned int is_vaild;
};
struct ecc_double_data_info {
    unsigned long int physical_addr;
    unsigned char resv[4]; /* 4 is resv length */
    unsigned short rank;
    unsigned short module_id;
    unsigned char type;
    unsigned char module;
    unsigned short bank;
    unsigned short row;
    unsigned short column;
    int timer_stamp;
};
struct ecc_single_data_info {
    unsigned int hbmc_id;
    unsigned int single_bit_count;
    unsigned int single_bit_low_addr;
    unsigned int single_bit_high_addr;
    unsigned int last_appear_time_stamp;
};
#pragma pack(1)
struct ecc_common_data {
    unsigned long long physical_addr;
    unsigned int stack_pc_id;
    unsigned int reg_addr_h;
    unsigned int reg_addr_l;
    unsigned int ecc_count;
    int timestamp;
};
#pragma pack()
#define SINGLE_ECC_INFO_BLOCK_NUMS 8
#define SINGLE_ECC_INFO_BLOCK_SIZE 8
#define DOUBLE_ECC_INFO_BLOCK_NUMS 16
#define DOUBLE_ECC_INFO_BLOCK_SIZE 4
#define DOUBLE_ECC_TIME_INFO_BLOCK_NUMS 2
#define DOUBLE_ECC_TIME_INFO_BLOCK_SIZE 32
#define HBM_ECC_RECORD_MAX_NUMS 64
struct udis_multi_ecc_time_data {
    unsigned int multi_record_count;
    unsigned int multi_ecc_times[HBM_ECC_RECORD_MAX_NUMS];
};
#if defined CFG_FEATURE_HBM_FLASH || defined CFG_FEATURE_DDR_FLASH
#define HIGH_ADDR_SID_BITS_COUNT 4
#define LOW_ADDR_ROW_BITS_OFFSET 8
#else
#define HIGH_ADDR_COLUMN_BITS_COUNT 14
#define LOW_ADDR_RANK_BITS_OFFSET 4
#endif

#define DM_COMMAND_BIGIN(cmd_name, device_index, input_len, out_len)                                        \
    DSMI_DMP_COMMAND_ST *dm_dmp = NULL;                                                                     \
    DSMI_CMD_CODE *dm_request = NULL;                                                                       \
    DSMI_DFT_RES_CMD *dm_resp = NULL;                                                                       \
    unsigned char *dm_fill_in = NULL;                                                                       \
    unsigned char *dm_fill_out = NULL;                                                                      \
    unsigned int cpy_len = 0;                                                                               \
    unsigned int out_current = 0;                                                                           \
    unsigned int in_current = 0;                                                                            \
    int local_devid = (int)(device_index);                                                                  \
    int ret = -1;                                                                                           \
    struct devdrv_device_info dev_info = {0};                                                               \
    struct timespec tv_start = {0};                                                                         \
    struct timespec tv_end = {0};                                                                           \
    long during;                                                                                            \
    (void)clock_gettime(CLOCK_MONOTONIC, &tv_start);                                                        \
    DRV_CHECK_RETV((((unsigned int)(input_len)) <= ((unsigned int)USHORT_MAX)), DRV_ERROR_PARA_ERROR);      \
    DRV_CHECK_RETV((((unsigned int)(out_len)) <= (MAX_OUTPUT_LEN)), DRV_ERROR_PARA_ERROR);                  \
    ret = dsmi_check_device_id((int)(device_index));                                                        \
    if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {                                                          \
        return DRV_ERROR_RESOURCE_OCCUPIED;                                                                 \
    }                                                                                                       \
    DRV_CHECK_RETV((ret == 0), DRV_ERROR_INVALID_DEVICE);                                                   \
    /* The dsmi interface cannot be called when device is offline or in the virtual machine */              \
    ret = (int)drvGetDevInfo((unsigned int)(device_index), &dev_info);                                      \
    if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {                                                          \
        return DRV_ERROR_RESOURCE_OCCUPIED;                                                                 \
    }                                                                                                       \
    dm_dmp = dmp_command_init((unsigned int)(device_index), g_dmp_cmd_def_##cmd_name.opcode,                \
                              (unsigned char)g_dmp_cmd_def_##cmd_name.op_type, (unsigned short)(input_len), \
                              (unsigned short)(out_len));                                                   \
    if (dm_dmp == NULL) {                                                                                   \
        DEV_MON_ERR("dev(%d) dmp_command_init failed\n", local_devid);                                      \
        return DRV_ERROR_INNER_ERR;                                                                         \
    }                                                                                                       \
    dm_request = (DSMI_CMD_CODE *)dm_dmp->send_msg.data;                                                    \
    dm_fill_in = (unsigned char *)dm_request->send_data;                                                    \
    dm_resp = (DSMI_DFT_RES_CMD *)dm_dmp->recv_msg.data;                                                    \
    dm_fill_out = (unsigned char *)dm_resp->response_data;                                                  \
    (void)dm_fill_out;                                                                                      \
    (void)dm_fill_in;                                                                                       \
    (void)out_current;                                                                                      \
    (void)in_current;                                                                                       \
    (void)cpy_len;                                                                                          \
    (void)ret;

#define DM_COMMAND_ADD_REQ(in_data, in_len)                                                                          \
    if (dsmi_is_null_check((const char *)(in_data)) != 0) {                                                          \
        dsmi_cmd_req_free(dm_dmp);                                                                                   \
        DEV_MON_ERR("dev(%d) input para %s is null\n", local_devid, #in_data);                                       \
        return DRV_ERROR_PARA_ERROR;                                                                                 \
    }                                                                                                                \
    if ((in_current + (in_len)) > (dm_request->length)) {                                                            \
        DEV_MON_ERR("dev(%d) add para %s, in_current:%d,len=%d error\n", local_devid, #in_data, in_current, in_len); \
        DEV_MON_ERR("dev(%d) dm_request length=%d error\n", local_devid, dm_request->length);                        \
        dsmi_cmd_req_free(dm_dmp);                                                                                   \
        return DRV_ERROR_PARA_ERROR;                                                                                 \
    }                                                                                                                \
    if (memmove_s(dm_fill_in, (in_len), (const unsigned char *)(in_data), (in_len)) != 0) {                          \
        dsmi_cmd_req_free(dm_dmp);                                                                                   \
        DEV_MON_ERR("dev(%d) memmove_s error\n", local_devid);                                                       \
        return DRV_ERROR_PARA_ERROR;                                                                                 \
    }                                                                                                                \
    dm_fill_in += (in_len);                                                                                          \
    in_current += (unsigned int)(in_len);

#define DM_COMMAND_SEND()                                                                                          \
    ret = dsmi_send_msg_rec_res(dm_dmp);                                                                           \
    if (ret != 0) {                                                                                                \
        dsmi_cmd_req_free(dm_dmp);                                                                                 \
        if (ret != (int)DRV_ERROR_NOT_EXIST) {                                                                     \
            DEV_MON_EX_NOTSUPPORT_ERR(ret, "dev(%d) dsmi_send_msg_rec_res failed, ret = %d.\n", local_devid, ret); \
        }                                                                                                          \
        return ret;                                                                                                \
    }

#define DM_COMMAND_PUSH_OUT(out_buff, out_len)                                                          \
    if ((char *)(out_buff) == NULL) {                                                                   \
        dsmi_cmd_req_free(dm_dmp);                                                                      \
        DEV_MON_ERR("dev(%d) output para %s is null\n", local_devid, #out_buff);                        \
        return DRV_ERROR_PARA_ERROR;                                                                    \
    }                                                                                                   \
    cpy_len = (unsigned int)(out_len);                                                                  \
    if ((out_current + (unsigned int)(out_len)) > (dm_resp->length)) {                                  \
        cpy_len = (dm_resp->length) - (out_current);                                                    \
    }                                                                                                   \
    if (cpy_len == 0) {                                                                                 \
        dsmi_cmd_req_free(dm_dmp);                                                                      \
        DEV_MON_INFO("dev(%d) cpy_len len null\n", local_devid);                                        \
        return OK;                                                                                      \
    }                                                                                                   \
    if (memmove_s((unsigned char *)(out_buff), (size_t)(out_len), dm_fill_out, (size_t)cpy_len) != 0) { \
        dsmi_cmd_req_free(dm_dmp);                                                                      \
        DEV_MON_ERR("dev(%d) memmove_s error\n", local_devid);                                          \
        return DRV_ERROR_PARA_ERROR;                                                                    \
    }                                                                                                   \
    dm_fill_out += cpy_len;                                                                             \
    out_current += cpy_len;

#define DM_COMMAND_GET_RSP_LEN(a)                               \
    if ((char *)(a) == NULL) {                                  \
        dsmi_cmd_req_free(dm_dmp);                              \
        DEV_MON_ERR("dev(%d) out msg len null\n", local_devid); \
        return DRV_ERROR_PARA_ERROR;                            \
    }                                                           \
    *(unsigned int *)(a) = (dm_resp->length);

#define DM_COMMAND_PROC_RSP_DATA(a) a;

#ifdef DRV_HOST
#define DM_COMMAND_END()                                                                               \
    dsmi_cmd_req_free(dm_dmp);                                                                         \
    (void)clock_gettime(CLOCK_MONOTONIC, &tv_end);                                                     \
    during = (tv_end.tv_sec - tv_start.tv_sec) * 1000 + (tv_end.tv_nsec - tv_start.tv_nsec) / 1000000; \
    if (during >= WARN_COST_TIME) {                                                                    \
        DEV_MON_DEBUG("dsmi send and receive dmp cmd time ms. (time=%ldms)\n", during);                \
    }                                                                                                  \
    return OK;
#else
#define DM_COMMAND_END()                                                                               \
    dsmi_cmd_req_free(dm_dmp);                                                                         \
    (void)clock_gettime(CLOCK_MONOTONIC, &tv_end);                                                     \
    during = (tv_end.tv_sec - tv_start.tv_sec) * 1000 + (tv_end.tv_nsec - tv_start.tv_nsec) / 1000000; \
    if (during >= WARN_COST_TIME) {                                                                    \
        DEV_MON_EVENT("dsmi send and receive dmp cmd time ms. (time=%ldms)\n", during);                \
    }                                                                                                  \
    return OK;
#endif

#define DSMI_FREE(p) \
    do {             \
        free(p);     \
        p = NULL;    \
    } while (0)

#define DSMI_SAFE_FUN_FAIL 0X20

#ifdef CFG_FEATURE_POWER_COMMAND
int dsmi_raise_script(const char *path, char **argv);
int dsmi_invoke_os_cmd(DSMI_POWER_STATE cmd_type);
#endif
#define DSMI_MUTEX_NO_WAIT 0
#define DSMI_MUTEX_WAIT_FOR_EVER (0XFFFFFFFFU)
DSMI_DMP_COMMAND_ST *dmp_command_init(unsigned int device_index, unsigned short opcode, unsigned char optype,
                                      unsigned short input_len, unsigned short output_len);

int dsmi_send_msg_rec_res(struct dsmi_dmp_command_st *dmp);

void clear_all_blank(char *str);

int split_by_char(const char *src, char *path, unsigned int path_len_max, char *value, unsigned int value_len_max,
                  char split_char);

int local_copy_file(int device_id, const char *src_file, const char *dst_file);
int local_copy_file_to_mem(int device_id, const char *src_file, char *dest_addr, long length);
/* DLLEXPORT is for custom compilation */
DLLEXPORT int find_file_name(int device_id, const char *path_name, char *dst_name);

int get_pcie_mode(void);
int dsmi_mutex_p(key_t sem_tag, int *sem_id, unsigned int timeout);
int dsmi_mutex_v(int sem_id);
int dsmi_init(void);

int user_prop_check(void);

DLLEXPORT int drv_get_phy_mach_flag(int device_id);

/* DLLEXPORT is for testcase compilation */
DLLEXPORT int dsmi_config_enable(int device_id, CONFIG_ITEM config_item, DSMI_DEVICE_TYPE device_type, int enable_flag);
DLLEXPORT int dsmi_get_enable(int device_id, CONFIG_ITEM config_item, DSMI_DEVICE_TYPE device_type, int *enable_flag);
int check_upgrade_component_type_and_state(int device_id, unsigned char *upgrade_schedule,
                                           unsigned char *upgrade_status, DSMI_COMPONENT_TYPE component_type);
void dsmi_cmd_req_free(const void *item);
int dsmi_check_device_id(int device_id);
int dsmi_is_null_check(const char *ptr);
void dsmi_stop_fault_event_thread(void);

DEV_INFO_MAIN_CMD_TYPE dsmi_get_dev_info_main_cmd_type(DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd);
int parse_cfg_file(int devices_id, CFG_FILE_DES *component_des, const char *file_name,
                   DSMI_COMPONENT_TYPE component_type);
int upgrade_ufs_component(int device_id, const char *file_name, DSMI_COMPONENT_TYPE component_type);
int check_component_type(DSMI_COMPONENT_TYPE component_type, int device_id);
int check_component_type_validity(DSMI_COMPONENT_TYPE *component_list, DSMI_COMPONENT_TYPE component_type,
                                  unsigned int component_num);
int check_dst_file_path(int device_id, const char *src_path);
int dsmi_get_davinchi_info(int device_id, int device_type, int info_type, unsigned int *result_data);
int dsmi_update_no_response_data(int device_id, unsigned char control_cmd);
int dsmi_cmd_set_device_info_method(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                                    unsigned int buf_size);
int dsmi_get_muti_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                              unsigned int *size);
int dsmi_udis_get_hbm_isolated_pages_info(int dev_id, unsigned char module_type,
                                          struct dsmi_ecc_pages_stru *pdevice_ecc_pages_statistics);
int dsmi_cmd_get_sec_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                          unsigned int *size);
int dsmi_cmd_get_custom_sign_flag(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                                  unsigned int *size);
drvError_t dsmi_cmd_set_custom_sign_flag(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd,
                                         const void *buf, unsigned int size);
drvError_t dsmi_cmd_set_custom_sign_cert(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd,
                                         const void *buf, unsigned int size);
int dsmi_cmd_get_sign_cert(unsigned int device_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                           unsigned int *size);
int dsmi_udis_get_multi_ecc_info(int dev_id, struct ecc_common_data *dsmi_ecc_common_data_s);
int dsmi_udis_get_single_ecc_info(int dev_id, struct ecc_common_data *dsmi_ecc_common_data_s);
int dsmi_udis_multi_ecc_time_info(int dev_id, struct udis_multi_ecc_time_data *multi_ecc_time_data);
#endif