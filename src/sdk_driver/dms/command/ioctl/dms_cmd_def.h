/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef __DMS_CMD_DEF_H__
#define __DMS_CMD_DEF_H__

#include "pbl/pbl_urd_main_cmd_def.h"
#include "pbl/pbl_urd_sub_cmd_def.h"

#define MAKE_UP_COMMAND(fun, cmd) (((fun) << 16) | (cmd))
#define MAKE_UP_INDEPENDENCE_COMMAND(main, sub) (((main) << 12) | (sub))

#define DMS_MAKE_UP_FILTER_HAL_DEV_INFO_EX(f, module, info)                                                  \
    do {                                                                                                     \
        (f)->filter_len = (unsigned int)sprintf_s((f)->filter, sizeof((f)->filter), "module=0x%x,info=0x%x", \
                                                  (unsigned int)(module), (unsigned int)(info));             \
    } while (0)

#define DAVINCI_INTF_MODULE_DMS "DMS"

#define DMS_MAGIC 'V'
#define DMS_IOCTL_CMD _IO(DMS_MAGIC, 1)

#define DMS_GET_BOARD_ID _IO(DMS_MAIN_CMD_SOC, DMS_SUBCMD_GET_BOARD_ID)
#define DMS_GET_MASTER_DEV _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_MASTER_DEV)
#define DMS_GET_H2D_DEV_INFO _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_H2D_DEV_INFO)
#define DMS_GET_CPU_WORK_MODE _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CPU_WORK_MODE)
#define DMS_GET_HCOM_CPU_NUM _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_HCOM_CPU_NUM)
#define DMS_GET_CHIP_TYPE _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_TYPE)
#define DMS_GET_DEV_PROBE_NUM _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_PROBE_NUM)
#define DMS_GET_DEV_PROBE_LIST _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_PROBE_LIST)
#define DMS_GET_P2P_ATTR _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_DEV_P2P_ATTR)
#define DMS_GET_LPM_FREQUENCY _IO(DMS_MAIN_CMD_LPM, DMS_SUBCMD_GET_FREQUENCY)
#define DMS_GET_QOS_CONFIG_INFO _IO(DMS_MAIN_CMD_QOS, DMS_SUBCMD_GET_CONFIG_INFO)
#define DMS_GET_BBOX_LOG_DUMP_INFO _IO(DMS_MAIN_CMD_BBOX, DMS_SUBCMD_GET_LOG_DUMP_INFO)

#define DMS_GET_SLOT_ID _IO(DMS_MAIN_CMD_SOC, DMS_SUBCMD_GET_SLOT_ID)
#define DMS_GET_PHY_DEVICE_INFO _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_PHY_DEVICE_INFO)

#define DMS_GET_FAULT_EVENT _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_FAULT_EVENT)
#define DMS_GET_EVENT_CODE _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_EVENT_CODE)
#define DMS_GET_TRS_CAPABILITY_GROUP_INFO _IO(DMS_MAIN_CMD_TRS, DMS_SUBCMD_GET_CAPABILITY_GROUP)
#define DMS_GET_H2D_DEVINFO _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_H2D_DEV_INFO)

#define DMS_ENABLE_EVENT _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_ENABLE_EVENT)
#define DMS_DISABLE_EVENT _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_DISABLE_EVENT)
#define DMS_CLEAR_EVENT _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_CLEAR_EVENT)

#define DMS_GET_AI_INFO_FROM_TS _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_AI_INFO_FROM_TS)
#define DMS_GET_HISTORY_FAULT_EVENT _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_HISTORY_FAULT_EVENT)

#define DMS_GET_OSC_FREQ_INFO_HOST _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_HOST_OSC_FREQ)
#define DMS_GET_OSC_FREQ_INFO_DEVICE _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_OSC_FREQ)

#define DMS_GET_FAULT_INJECT_INFO _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_FAULT_INJECT_INFO)

#define DMS_GET_CHIP_INFO _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_INFO)

#define DMS_GET_AICORE_DIE_NUM _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_AICORE_DIE_NUM)

#define DMS_GET_DEV_SPLIT_MODE _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_SPLIT_MODE)

#define DMS_GET_CPU_INFO _IO(DMS_MAIN_CMD_SOC, DMS_SUBCMD_GET_CPU_INFO)
#define DMS_GET_CPU_UTILIZATION _IO(DMS_MAIN_CMD_SOC, DMS_SUBCMD_GET_CPU_UTILIZATION)

#define DMS_SENSOR_NODE_REGISTER _IO(DMS_MAIN_CMD_SOFT_FAULT, DMS_SUBCMD_SENSOR_NODE_REGISTER)
#define DMS_SENSOR_NODE_UNREGISTER _IO(DMS_MAIN_CMD_SOFT_FAULT, DMS_SUBCMD_SENSOR_NODE_UNREGISTER)
#define DMS_SENSOR_NODE_UPDATE_VAL _IO(DMS_MAIN_CMD_SOFT_FAULT, DMS_SUBCMD_SENSOR_NODE_UPDATE_VAL)

#define DMS_GET_ALL_DEVICE_LIST _IO(DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_ALL_DEV_LIST)

#define FILTER_MAX_LEN 128
struct dms_ioctl_arg {
    unsigned int msg_source;
    unsigned int main_cmd;
    unsigned int sub_cmd;
    const char *filter;
    unsigned int filter_len;
    void *input;
    unsigned int input_len;
    void *output;
    unsigned int output_len;
};

#define DMS_MAX_EVENT_NAME_LENGTH 256
#define DMS_MAX_EVENT_DATA_LENGTH 32
#define DMS_MAX_EVENT_ARRAY_LENGTH 128
#define DMS_MAX_EVENT_INFO_NUM 4

struct dms_event_para {
    unsigned int event_code;
    int pid;

    unsigned int event_id;
    unsigned short deviceid;
    unsigned short node_type;
    unsigned char node_id;
    unsigned short sub_node_type;
    unsigned char sub_node_id;
    unsigned char severity;
    unsigned char assertion;
    unsigned short sensor_num;
    int event_serial_num;
    int notify_serial_num;
    unsigned long long alarm_raised_time;
    char event_name[DMS_MAX_EVENT_NAME_LENGTH];
    char additional_info[DMS_MAX_EVENT_DATA_LENGTH];
    unsigned char event_info[DMS_MAX_EVENT_INFO_NUM];
};

struct devdrv_event_obj_para {
    unsigned int event_count;
    struct dms_event_para dms_event[DMS_MAX_EVENT_ARRAY_LENGTH];
};

#define MAX_EVENT_RESV_LENGTH 32
struct dms_event_filter {
    unsigned long long filter_flag; /* bit0: event_id; bit1: severity; bit2: node_type; bit3: current tgid */
    unsigned int event_id;
    unsigned char severity;
    unsigned short node_type;
    unsigned char resv[MAX_EVENT_RESV_LENGTH]; /* reserve 32byte */
};

enum cmd_source {
    FROM_DSMI = 0,
    FROM_HAL = 1,
    FROM_KERNEL = 2,
    INVALID_SOURCE = 0xFF
};
struct dms_read_event_ioctl {
    enum cmd_source cmd_src;
    int timeout;
    int dev_id;
    struct dms_event_filter filter;
};

struct dms_get_event_ioctl {
    unsigned int dev_id;
    struct dms_event_filter filter;
};

struct dms_event_ioctrl {
    unsigned int devid;
    unsigned int event_code;
};

struct dms_power_state_st {
    unsigned int dev_id;
    int state;
};

struct dms_get_gpio {
    unsigned int dev_id;
    unsigned int gpio_num;
};

struct dms_set_device_info_in {
    unsigned int dev_id;
    unsigned int sub_cmd;
    const void *buff;
    unsigned int buff_size;
};

struct dms_set_device_info_in_multi_packet {
    unsigned int dev_id;
    unsigned int sub_cmd;
    unsigned char buff[300];
    unsigned int buff_size;
    unsigned int current_packet;
    unsigned int total_size;
};

struct dms_get_device_info_in {
    unsigned int dev_id;
    unsigned int sub_cmd;
    void *buff;
    unsigned int buff_size;
};
struct dms_hal_device_info_stru {
    unsigned int dev_id;        /* in */
    int module_type;            /* in */
    int info_type;              /* in */
    unsigned int buff_size;     /* in&out, must be <= sizeof(payload) */
    unsigned char payload[300]; /* in&out, valid length is min(buff_size, sizeof(payload)) */
};
#define DMS_HAL_DEV_INFO_HEAD_LEN (uint32_t)((size_t) & ((struct dms_hal_device_info_stru *)0)->payload)

struct dms_get_device_info_out {
    unsigned int out_size;
};
#define DMS_FILTER_LENGTH 256

struct dms_filter_st {
    char filter[DMS_FILTER_LENGTH];
    unsigned int filter_len;
};

struct dms_get_vdevice_info_in {
    unsigned int dev_id;
    unsigned int vfid;
};

struct dms_get_vdevice_info_out {
    unsigned int total_core;
    unsigned int core_num;
    unsigned long mem_size;
    unsigned char resv[64];
};

struct dms_get_ddr_info_in {
    unsigned int dev_id;
    unsigned int vfid;
    unsigned int resv[2];
};

struct dms_get_ddr_info_out {
    unsigned long total;
    unsigned long free;
    unsigned long resv[8];
};

struct dms_get_dev_topology_in {
    unsigned int dev_id1;
    unsigned int dev_id2;
};

struct dms_set_bist_info_in {
    unsigned int dev_id;
    unsigned int buff_size;
    unsigned char *buff;
};

struct dms_get_bist_info_in {
    unsigned int dev_id;
    unsigned int sub_cmd;
    unsigned int size;
    unsigned char *buff;
};

struct dms_set_bist_info_multi_cmd_in {
    unsigned int dev_id;
    unsigned int sub_cmd;
    unsigned int buff_size;
    unsigned char *buff;
};

struct dms_get_bist_info_out {
    unsigned int size;
};

struct dms_flash_content_in {
    int dev_id;
    unsigned int type;
    unsigned char *buf;
    unsigned int size;
    unsigned int offset;
};

struct dms_ctrl_device_node_in {
    unsigned int dev_id;
    int node_type;
    int node_id;
    unsigned int sub_cmd;
    void *in_buf;
    unsigned int in_size;
};

struct dms_get_all_device_node_in {
    unsigned int dev_id;
    unsigned int capability;
    struct dsmi_dtm_node_s *node_info;
    unsigned int size;
};

struct dms_get_all_device_node_out {
    unsigned int out_size;
};

struct dms_fault_inject_in {
    unsigned int dev_id;
    unsigned int vfid;
    unsigned int buff_size;
    const void *buff;
};

struct dms_sriov_switch_in {
    unsigned int dev_id;
    int sriov_switch;
};

struct dms_get_cpu_utilization_in {
    unsigned int index;
    unsigned int num;
};

struct dms_mem_info_in {
    unsigned int dev_id;
    unsigned int vfid;
    unsigned int resv[2];
};

struct dms_mem_info_out {
    unsigned long long total_size;
    unsigned long long use_size;
    unsigned long long free_size;
    unsigned long long hpage_2M_num;
    unsigned long long hpage_1G_num;
    unsigned int mem_util;
    unsigned long long resv[8];
};

struct dms_cgroup_mem_info_out {
    unsigned long long limit_in_bytes;
    unsigned long long max_usage_in_bytes;
    unsigned long long usage_in_bytes;
    unsigned long long resv[8];
};

struct dms_pass_through_mcu_in {
    unsigned int dev_id;
    unsigned int rw_flag;
    unsigned int buf_len;
    unsigned char *buf;
};

#define MCU_RESP_LEN 28
struct dms_pass_through_mcu_out {
    unsigned int dev_id;
    unsigned int response_len;
    unsigned char response_data[MCU_RESP_LEN];
};

#define DMS_FILTER_TS_INFO "main_cmd=0xb"
struct dms_ts_fault_mask {
    unsigned int type;
    unsigned int mask_switch;
};

#define DSMI_SIO_SLLC_INDEX_BIT (0xFF000000U)
#define DSMI_SIO_SUB_CMD_BIT (0x00FFFFFFU)

struct dms_sio_crc_err_info {
    unsigned char sllc_index; /* input parameter */
    unsigned short tx_error_count;
    unsigned short rx_error_count;
    unsigned long long rx_ecc_count;
    unsigned long long resv[8];
};

struct dms_device_state_out {
    unsigned long buf_size;
    unsigned char buf[];
};

struct dms_update_ts_patch {
    unsigned int devid;
    unsigned int update_type; /* 0:install; 1: uninstall */
};

typedef struct dms_sils_info_out {
    unsigned int size;
} dms_sils_info_out_t;

#define SILS_RESERVE_LEN 8
typedef struct dms_sils_info_in {
    unsigned int dev_id;
    unsigned int size;
    unsigned int resv[SILS_RESERVE_LEN];
    void *buff;
} dms_sils_info_in_t;

#define DMS_EID_INFO_LEN 16
#define DMS_MAX_EID_PAIR_NUM 8
#define DMS_MAX_DEVICE_REPLACE_TIMEOUT_SEC 120

union dms_urma_addr_info {
    unsigned char eid[DMS_EID_INFO_LEN];
    struct {
        unsigned long long reserved;
        unsigned int prefix;
        unsigned int addr;
    } in4;
    struct {
        unsigned long long subnet_prefix;
        unsigned long long interface_id;
    } in6;
};

enum dms_urma_eid_type {
    DMS_URMA_EID_TYPE = 0U, /* 128bit */
    DMS_URMA_TYPE_MAX = 1U,
};

struct dms_eid_pair_info {
    union dms_urma_addr_info eid_local;
    union dms_urma_addr_info eid_remote;
    unsigned int default_eid_flag : 1;
    unsigned int resv : 31;
};

struct dms_device_attr {
    int phy_dev_id;
    unsigned int eid_num;
    struct dms_eid_pair_info eid_pair[DMS_MAX_EID_PAIR_NUM];
    enum dms_urma_eid_type type;
    unsigned int reserve;
};

struct dms_device_replace_stru {
    struct dms_device_attr src_dev_attr;
    struct dms_device_attr dst_dev_attr;
    unsigned int timeout;
    unsigned long long flag;
};

struct dms_eid_info {
    unsigned int dev_id;
    struct dms_eid_pair_info eid;
};

struct dms_eid_query_info {
    union dms_urma_addr_info local_eid[DMS_MAX_EID_PAIR_NUM];
    unsigned int num;
    unsigned int min_idx; // Currently, only choose the smallest eid, record the idx
};

enum devdrv_d2d_eid_query_type {
    D2D_EID_QUERY_BY_DEVID = 0,
    D2D_EID_QUERY_BY_TYPE,
    D2D_EID_QUERY_TYPE_MAX
};

struct dms_d2d_eid_input {
    unsigned int local_devid;
    unsigned int query_type; // query d2d_eid by dev_id or eid_type
    unsigned int remote_udevid;
    unsigned int d2d_eid_type;
    unsigned int reserved[4];
};

struct dms_d2d_eid_output {
    union dms_urma_addr_info d2d_eid;
    unsigned int eid_index;
    unsigned int reserved[4];
};

struct dms_get_phy_dev_info_out {
    unsigned int chip_id;
    unsigned int die_id;
    unsigned int reserved[32];
};

#endif
