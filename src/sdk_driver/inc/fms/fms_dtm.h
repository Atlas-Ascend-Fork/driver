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

#ifndef FMS_DTM_H
#define FMS_DTM_H

#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_base_pub.h"

#include "dms/dms_cmd_def.h"
#include "ascend_dev_num.h"
#include "dms_device_node_type.h"

#define DEVICE_NODE_NUM_MAX 4
/* Maximum character length of device name */
#define DMS_MAX_DEV_NAME_LEN 16
/* Sensor description length */
#define DMS_SENSOR_DESCRIPT_LENGTH 20
/* The maximum number of entries recorded by the sensor timing detection execution time */
#define DMS_MAX_TIME_RECORD_COUNT 20
/* Maximum number of events supported by the sensor */
#define DMS_MAX_SENSOR_EVENT_COUNT 16

#ifndef DMS_MAX_EVENT_DATA_LENGTH
#define DMS_MAX_EVENT_DATA_LENGTH 32
#endif

/* dms state flag */
#define DMS_NOT_USED 0x1
#define DMS_IN_USED 0x2

#ifndef ASCEND_DEV_MAX_NUM
#define ASCEND_DEV_MAX_NUM 64
#endif

typedef enum {
    DMS_DEV_NODE0 = 0x0,
    DMS_DEV_NODE1 = 0x1,
    DMS_DEV_ID_MAX
} DMS_DEVICE_NODE_ID;

typedef enum {
    HAL_DMS_DEV_TYPE_BASE_SERVCIE = 0x600,
    HAL_DMS_DEV_TYPE_PROC_MGR = 0x601,
    HAL_DMS_DEV_TYPE_IAMMGR = 0x602,
    HAL_DMS_DEV_TYPE_PROC_LAUNCHER = 0x603,
    HAL_DMS_DEV_TYPE_ADDA = 0x604,
    HAL_DMS_DEV_TYPE_DMP_DAEMON = 0x605,
    HAL_DMS_DEV_TYPE_SKLOGD = 0x606,
    HAL_DMS_DEV_TYPE_SLOGD = 0x607,
    HAL_DMS_DEV_TYPE_LOG_DAEMON = 0x608,
    HAL_DMS_DEV_TYPE_HDCD = 0x609,
    HAL_DMS_DEV_TYPE_AICPU_SCH = 0x60B,
    HAL_DMS_DEV_TYPE_QUEUE_SCH = 0x60C,
    HAL_DMS_DEV_TYPE_AICPU_CUST_SCH = 0x60E,
    HAL_DMS_DEV_TYPE_HCCP = 0x60F,
    HAL_DMS_DEV_TYPE_TSD_DAEMON = 0x610,
    HAL_DMS_DEV_TYPE_TIMER_SERVER = 0x616,
    HAL_DMS_DEV_TYPE_OS_LINUX = 0x617,
    HAL_DMS_DEV_TYPE_DATA_MASTER = 0x619,
    HAL_DMS_DEV_TYPE_CFG_MGR = 0x61A,
    HAL_DMS_DEV_TYPE_DATA_GW = 0x61D,
    HAL_DMS_DEV_TYPE_RESMGR = 0x623,
    HAL_DMS_DEV_TYPE_MAMI_DEV_CTRL = 0x624,
    HAL_DMS_DEV_TYPE_DRV_KERNEL = 0x625,
    HAL_DMS_DEV_TYPE_UBCFGLITE = 0x626,
    HAL_DMS_DEV_TYPE_MAX
} HAL_DMS_DEVICE_NODE_TYPE;

/* bit 15~8: 000-hardware/kernel 110-soft 111-product */
#define DMS_EVENT_OBJ_KERNEL (0x0)
#define DMS_EVENT_OBJ_USER (0x6)
#define DMS_EVENT_OBJ_PRODUCT (0x7)
#define DMS_EVENT_OBJ_TYPE(node_type) (((node_type) >> 8) & 0x7)

typedef enum {
    POWER_STATE_SUSPEND,
    POWER_STATE_POWEROFF,
    POWER_STATE_RESET,
    POWER_STATE_BIST,
    POWER_STATE_MAX
} DSMI_POWER_STATE;

typedef enum {
    DMS_EVENT_OK = 0,
    DMS_EVENT_MINOR = 1,
    DMS_EVENT_MAJOR = 2,
    DMS_EVENT_CRITICAL = 3,
    DMS_EVENT_MAX
} DMS_SEVERITY_T;

typedef enum {
    DMS_RC_MODE,
    DMS_EP_MODE
} DMS_MODE_T;

#define DMS_FAULT_CONVERGE_CONFIG(sub_id, mod_id, sec_type, code, s_type, err_type, str) \
    {                                                                                    \
        .subsys_id = (sub_id),                                                           \
        .module_id = (mod_id),                                                           \
        .section_type = (sec_type),                                                      \
        .ras_code.err_status = (code),                                                   \
        .describe = (str),                                                               \
        .sensor_type = (s_type),                                                         \
        .error_type = (err_type),                                                        \
    },

/* ras first converge table item */
struct ras_fault_converge_item {
    unsigned char subsys_id;   /*  subsys:hbm-0x48  */
    unsigned char module_id;   /*  hbmc/ddrc  */
    unsigned int section_type; /*  MEM/OEM/PCIE/ARM ...  */
    union {
        unsigned int err_type;         /*  section_type:MEM  */
        unsigned int int_status;       /*  section_type:ARM  */
        unsigned long long err_status; /*  section_type:OEM  */
    } ras_code;
    unsigned char describe[DMS_MAX_EVENT_DATA_LENGTH]; /*  detail  */
    unsigned char sensor_type;                         /*  ras sensor 0xC0  */
    unsigned char error_type;                          /*  unified type definition  */
};

typedef struct dms_fault_inject_info {
    unsigned int device_id;
    unsigned int node_type;
    unsigned int node_id;
    unsigned int sub_node_type;
    unsigned int sub_node_id;
    unsigned int fault_index;
    unsigned int event_id;
    unsigned int reserve1;
    unsigned int reserve2;
} dms_fault_inject_t;

/* Data attribute definition */
struct dms_dev_data_attr {
    /* Data type */
    int info_type;
    /* Data life cycle 0: always valid 1: valid once */
    int info_life;
    /* Data reading permission, which permission or higher can read this data */
    int read_permission;
    /* Data write permission, which permission can read this data */
    int write_permission;
};
struct dms_node_operations;
/* Device management node structure */
struct dms_node {
    ka_list_head_t list;
    /* Device node (global edition) */
    int node_type;
    int node_id; /* global node index in chip */
    int pid;
    /* Device name */
    char node_name[DMS_MAX_DEV_NAME_LEN];
    /* Each bit represents a capability */
    unsigned long long capacity;
    /* Device permissions */
    int permission;
    /* The ID of the parent device to which the child device belongs, corresponding to the device ID of the chip */
    int owner_devid;
    /* The parent node of the device */
    struct dms_node *owner_device;
    /* node index in owner device, valid when @owner_device is not null */
    int inner_node_id;

    struct dms_node *sub_node[DEVICE_NODE_NUM_MAX];
    int state;
    /* Control and operation interface for equipment */
    struct dms_node_operations *ops;
};

struct dms_node_operations {
    int (*init)(struct dms_node *device);
    void (*uninit)(struct dms_node *device);
    int (*get_info_list)(struct dms_node *device, struct dms_dev_data_attr *InfoList);
    /* Query setting status */
    int (*get_state)(struct dms_node *device, unsigned int *state);
    int (*get_init_state)(struct dms_node *device, unsigned int *init_state);
    /* Query capability, each bit represents a capability */
    int (*get_capacity)(struct dms_node *device, unsigned long long *Capacity);
    /* Set the power state of the device to support the power-off, power-on, sleep, and reset of the device */
    int (*set_power_state)(struct dms_node *device, DSMI_POWER_STATE power_state);
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
    int (*fault_inject)(dms_fault_inject_t info);
    int (*get_fault_inject_info)(struct dms_node *node, dms_fault_inject_t *buf, unsigned int buf_cnt,
                                 unsigned int *real_cnt);

    int (*enable_device)(struct dms_node *device);
    int (*disable_device)(struct dms_node *device);
    int (*set_power_info)(struct dms_node *device, void *buf, unsigned int size);
    int (*get_power_info)(struct dms_node *device, void *buf, unsigned int size);
    /* Bus path detection hook, called in softirq context, sleeping is prohibited */
    int (*set_hw_bus_test)(struct dms_node *device, unsigned int cpu_id);
};

struct dms_converge_event_list {
    ka_list_head_t head;
    unsigned int event_num;
    unsigned int health_code;
    ka_mutex_t lock;
};

struct dms_sensor_reported_list {
    ka_list_head_t head;
    unsigned int reported_num;
    ka_mutex_t lock;
};

/* General threshold type sensor */
struct dms_general_sensor {
    /* Sensor properties */
    unsigned int attribute;
    /* Supported threshold types, each BIT represents a threshold type, currently supports
    Six types, including:DMS_SENSOR_THRES_LOW_MINOR_SERIES,DMS_SENSOR_THRES_LOW_MAJOR_SERIES
    DMS_SENSOR_THRES_LOW_CRITICAL_SERIES,DMS_SENSOR_THRES_UP_MINOR_SERIES,
    DMS_SENSOR_THRES_UP_MAJOR_SERIES, DMS_SENSOR_THRES_UP_CRITICAL_SERIES */
    unsigned int thres_series;
    /* Lower critical threshold */
    int low_critical;
    /* Lower critical threshold */
    int low_major;
    /* Lower minor threshold */
    int low_minor;
    /* Upper critical threshold */
    int up_critical;
    /* Upper severe threshold */
    int up_major;
    /* Upper minor threshold */
    int up_minor;
    /* is a positive value, a positive hysteresis value, which means that when a certain type of failure (critical,
     * serious, minor) is restored, the difference between the sensor value and the lower threshold after restoration */
    int pos_thd_hysteresis;
    /* is a positive value and a negative hysteresis value, which means that when a certain type of upper (critical,
     * serious, minor) fault is restored, the upper threshold minus the difference of the sensor value after
     * restoration */
    int neg_thd_hysteresis;
    /* The maximum legal value of the threshold */
    int max_thres;
    /* The minimum legal value of the threshold */
    int min_thres;
};

/* Discrete sensor */
struct dms_discrete_sensor {
    /* Sensor properties */
    unsigned int attribute;
    /* Anti-shake frequency attribute */
    unsigned int debounce_time;
};

/* Statistical threshold sensor */
struct dms_statistic_sensor {
    /* Generate threshold type, 0-periodic statistical type, 1-continuous detection type */
    unsigned short occur_thres_type;
    /* Recovery threshold type, 0-periodic statistics type, 1-continuous detection type */
    unsigned short resume_thres_type;
    /* Maximum statistical period */
    unsigned int max_stat_time;
    /* Minimum statistical period */
    unsigned int min_stat_time;
    /* Generate threshold statistical period */
    unsigned int occur_stat_time;
    /* Recovery threshold statistical period */
    unsigned int resume_stat_time;
    /* Sensor properties */
    unsigned int attribute;
    /* Maximum generation threshold */
    unsigned int max_occur_thres;
    /* Minimum generation threshold */
    unsigned int min_occur_thres;
    /* Maximum recovery threshold */
    unsigned int max_resume_thres;
    /* Minimum recovery threshold */
    unsigned int min_resume_thres;
    /* Generation threshold */
    unsigned int occur_thres;
    /* Recovery threshold */
    unsigned int resume_thres;
};

union dms_sensor_union {
    /* Discrete sensor */
    struct dms_discrete_sensor discrete_sensor;
    /* General threshold type sensor */
    struct dms_general_sensor general_sensor;
    /* Statistical threshold sensor */
    struct dms_statistic_sensor statistic_sensor;
};

#define DMS_MAX_EVENT_INFO_LENGTH 4
struct dms_sensor_event_data_item {
    /* Event offset of the sensor */
    int current_value;
    /* Event additional data length */
    unsigned short data_size;
    /* Event description */
    unsigned char event_data[DMS_MAX_EVENT_DATA_LENGTH];
    /* Event additional data
       Now just support on discrete sensor,
       statistical sensor and general threshold type sensor, temporarily */
    unsigned char event_info[DMS_MAX_EVENT_INFO_LENGTH];
};
struct dms_sensor_event_data {
    unsigned char event_count;
    struct dms_sensor_event_data_item sensor_data[DMS_MAX_SENSOR_EVENT_COUNT];
};

enum {
    DMS_SERSOR_SCAN_PERIOD = 0,
    DMS_SERSOR_SCAN_NOTIFY = 1,
    DMS_SERSOR_SCAN_MODULE_MAX
};

/* Register the configuration of sensor input */
struct dms_sensor_object_cfg {
    /* sensor type */
    unsigned char sensor_type;
    /* Sensor name description */
    char sensor_name[DMS_SENSOR_DESCRIPT_LENGTH];
    /* The sensor information table types include 3 types: threshold type, discrete type, and statistical type */
    unsigned short sensor_class;
    /* Sensor type combination */
    union dms_sensor_union sensor_class_cfg;
    /* Scan module, 0 is period, 1 is notify */
    unsigned int scan_module;
    /* Detection period, in milliseconds */
    unsigned int scan_interval;
    /* Detection processing flag, 0 processing, 1 not processing */
    unsigned int proc_flag;
    /* Enable flag, 0 is forbidden, 1 is for enable */
    unsigned int enable_flag;
    /* Detect function pointer */
    int (*pf_scan_func)(unsigned long long private_data, struct dms_sensor_event_data *pevent_data);
    /* Reserved function parameters */
    unsigned long long private_data;
    /* A flag indicating whether it is possible to report a fault event: 1-enable, 0-forbid */
    unsigned int assert_event_mask;
    unsigned int deassert_event_mask;
    int pid;
    /* Clear sensor events callback */
    int (*pf_clear_event_func)(unsigned long long private_data);
};

/* Add SYS_TIME structure definition */
typedef struct dms_sys_time {
    unsigned short year;  /* if set to OS time the scope is 1970 ~ 2038, or
                           the scope is 1970 ~ 2100 */
    unsigned char month;  /* scope is 1 - 12 */
    unsigned char date;   /* scope is 1 - 31 */
    unsigned char hour;   /* scope is 0 - 23 */
    unsigned char minute; /* scope is 0 - 59 */
    unsigned char second; /* scope is 0 - 59 */
    unsigned char week;   /* scope is 0 - 6  */
} SYS_TIME;

typedef struct tag_dms_event_list_item {
    /* Whether to use 0 = not used */
    unsigned char in_use : 1;
    /* Whether it is a newly generated event or inherits
    the old event 1 = Old event 0 = New event, used when comparing event lists */
    unsigned char is_report : 7;
    unsigned char event_data;
    unsigned int sensor_num;
    char sensor_name[DMS_SENSOR_DESCRIPT_LENGTH];
    unsigned long long timestamp;
    /* The serial number generated by the alarm */
    unsigned int alarm_serial_num;
    struct tag_dms_event_list_item *p_next;
    /* Additional data for the event */
    unsigned short para_len;
    /* The length of the additional data of the event */
    unsigned char *event_paras;
    /* Other information about the incident */
    unsigned char event_info[DMS_MAX_EVENT_INFO_LENGTH];
    /* Continuous generation times */
    unsigned char continued_count;
} DMS_EVENT_LIST_ITEM;

union dms_sensor_class_cb {
    /* Discrete sensor */
    struct dms_discrete_sen_cb {
        /* For discrete sensors, this field indicates the last state of the sensor */
        unsigned int pre_status;
    } discrete_cb;
    /* General threshold type sensor */
    struct dms_general_sen_cb {
        unsigned int pre_status;
    } general_cb;
    /* Statistical threshold sensor */
    struct dms_statistic_sen_cb {
        /* It is dedicated to statistical sensors. The parameters used for statistical sensors to report events
        updated by the alarm processing module have a fixed length of 20 bytes, and the effective data length is
        determined by the length of the event parameters above. The first part of the parameter stores the key field
        parameters.
         */
        unsigned char stat_event_paras[DMS_MAX_EVENT_DATA_LENGTH];
        /* The statistics sensor is dedicated to indicate the current cycle count,
        which is less than the statistics cycle. After the sensor state changes
         or the statistical period arrives, the DMS common module sets the initial value.
         */
        unsigned int stat_time_counter;
        /* Special for statistical sensors, current state bit count variable, record previous current state,
        each bit represents the state once,
         the previous 32 states are cached at most, and the lowest bit is the most recent state
         */
        unsigned int current_bit_count;
        /* Statistic sensor dedicated, alarm recovery times */
        unsigned int alarm_clear_times;
        /* State counter. In the fault state, it indicates the count of the normal state;
        in the normal state, it indicates the count of the fault state */
        unsigned int status_counter;
        /* Current sensor status change time */
        SYS_TIME object_op_state_ch_time;
        /* Reason for current sensor status change */
        unsigned int object_op_state_chg_cause;
    } statistic_cb;
};

struct dms_sensor_object_cb {
    /* Instance index */
    unsigned short object_index;

    unsigned int sensor_num;

    /* Node ID to which the sensor belongs */
    unsigned int owner_node_id;
    unsigned int owner_node_type;
    struct dms_node_sensor_cb *p_owner_cb;

    /* Sensor instance configuration, from the input configuration of the registered sensor */
    struct dms_sensor_object_cfg sensor_object_cfg;
    struct dms_sensor_object_cfg orig_obj_cfg;

    /* The remaining time, in milliseconds */
    unsigned int remain_time;

    /* Indicates the current sensor value, the latest value read by the sensor
    Discrete sensor: is the event offset value of the sensor
    Threshold sensor: the actual reading of the sensor, such as temperature value
    Statistics sensor: is the value of statistics */
    int current_value;

    /* Record the status of the most recently reported event */
    unsigned int event_status;
    /* The health status of the sensor, collecting all events of the sensor,
    and the most serious level as the status of the sensor */
    unsigned int fault_status;

    ka_list_head_t list;

    DMS_EVENT_LIST_ITEM *p_event_list;
    /* The parameter of the currently detected event updated by the detection function has a
    fixed length of 20 bytes, and the effective data length is determined by the length of the above event parameter.
    The first part of the parameter stores the key field parameters.
     */
    unsigned int paras_len;
    unsigned char event_paras[DMS_MAX_EVENT_DATA_LENGTH];

    union dms_sensor_class_cb class_cb;
};

/* Public sensor detection information record structure definition, external interface */
struct dms_sensor_scan_fail_record {
    unsigned int node_type;
    unsigned int node_id;
    /* The current polled query table handle */
    unsigned int current_handle;
    /* Detect function pointer in current query table */
    void *current_scan_func;
    /* Number of failures to obtain synchronization semaphore */
    unsigned int syn_sema_fail;
    /* Number of failures to obtain mutex semaphore */
    unsigned int mux_sema_fail;
    /* Number of failures to obtain data from the linked list node */
    unsigned int get_data_from_node_fail;
    /* The number of times the obtained pTable structure is invalid */
    unsigned int table_error_fail;
    /* The obtained detection function is empty or the sensor table pointer is empty times */
    unsigned int null_scan_func_fail;
    /* The number of failed calls to the detection function */
    unsigned int call_scan_func_fail;
    unsigned int scan_func_date_error;
    /* The number of errors when the instance table is empty */
    unsigned int null_object_fail;
    /* Number of failures to call alarm processing */
    unsigned int sensor_process_fail;
    /* Number of failed reporting events */
    unsigned int report_event_fail;
};

struct dms_dev_sensor_cb;

/* Sensor timing detection execution time recorder */
struct dms_sensor_scan_time_recorder {
    /* The sensor periodically detects whether the processing time is recorded or not, 0 means no recording, 1 means
     * recording */
    unsigned int record_scan_time_flag;
    unsigned int sensor_out_time_count;
    unsigned int sensor_record_index;
    /* Statistics of each sensor execution time record table */
    struct dms_sensor_scan_time_item {
        /* sensor name */
        char sensor_name[DMS_SENSOR_DESCRIPT_LENGTH];
        /* The timing detection execution time of the sensor information table corresponding to the query table, in
         * milliseconds */
        unsigned long exec_time;
    } sensor_scan_time_record[DMS_MAX_TIME_RECORD_COUNT], max_sensor_scan_record;

    unsigned int dev_out_time_count;
    unsigned long max_dev_scan_record;
    unsigned int dev_record_index;
    unsigned long dev_scan_time_record[DMS_MAX_TIME_RECORD_COUNT];
    /* Start recording the execution time of the sensor scan function */
    void (*start_sensor_scan_record)(struct dms_dev_sensor_cb *dev_sensor_cb);
    /* Stop recording the execution time of the device scan function */
    void (*stop_sensor_scan_record)(struct dms_dev_sensor_cb *dev_sensor_cb,
                                    struct dms_sensor_object_cb *psensor_obj_cb);
    /* Start recording the execution time of the device scan function */
    void (*start_dev_scan_record)(struct dms_dev_sensor_cb *dev_sensor_cb);
    /* Start recording the execution time of the device scan function */
    void (*stop_dev_scan_record)(struct dms_dev_sensor_cb *dev_sensor_cb);
    ka_ktime_t sensor_start_time;
    ka_ktime_t dev_start_time;
};

/* Each child node has an instance of the structure, and the sensor registration of the child node will be attached to
 * this type of table pointer */
struct dms_node_sensor_cb {
    /* Device node ID node type + id */
    unsigned int node_id;
    unsigned short node_type;
    /* User mode sensor registration process pid */
    int pid;
    /* User mode or kernel mode registered function */
    unsigned short env_type;
    /* Sensor information table version */
    unsigned short version;
    /* ID of the device to which the node belongs */
    struct dms_node *owner_node;
    ka_list_head_t list;
    /* Number of sensor types */
    unsigned int sensor_object_num;
    unsigned short health;
    /* Sensor instance table */
    ka_list_head_t sensor_object_table;
};

struct dms_dev_sensor_cb {
    /* device id, 0 in AMP mode, 0~3 in 910 SMP mode */
    int deviceid;
    /* Resource lock */
    ka_mutex_t dms_sensor_mutex;
    int node_cb_num;
    unsigned short health;
    /* Device data group, initial allocation, call for 1 device (device id = 0x1000) on the host side, if device, AMP is
     * 1 device, 910 SMP is 4 devices */
    ka_list_head_t dms_node_sensor_cb_list;
    /* Task scanning process information record */
    struct dms_sensor_scan_fail_record sensor_scan_fail_record;
    /* Sensor timing detection execution time recorder */
    struct dms_sensor_scan_time_recorder scan_time_recorder;
};

/* Event Type: disappearance, generation */
enum {
    DMS_EVENT_TYPE_RESUME = 0,   /* disappearance */
    DMS_EVENT_TYPE_OCCUR = 1,    /* generation */
    DMS_EVENT_TYPE_ONE_TIME = 2, /* one-time */
    DMS_EVENT_TYPE_MAX
};

struct dms_event_dfx_table {
    ka_atomic_t recv_from_sensor[DMS_EVENT_TYPE_MAX];
    ka_atomic_t report_to_consumer[DMS_EVENT_TYPE_MAX];
    ka_list_head_t mask_list;
    ka_mutex_t lock;
};

struct dms_event_ctrl {
    struct dms_converge_event_list event_list;
    struct dms_sensor_reported_list reported_list;
    struct dms_event_dfx_table dfx_table;
};

struct dms_dev_ctrl_block {
    u32 state;
    void *dev_info;
    ka_atomic_t work_count;
    int sub_node_num;
    struct dms_event_ctrl dev_event_cb;
    ka_mutex_t node_lock;
    /* List of registered devices, manage all sub-devices on this device on the system */
    ka_list_head_t dev_node_list;
    /* Sensor control block of each chip */
    struct dms_dev_sensor_cb dev_sensor_cb;
};

struct dms_system_ctrl_block {
    int dev_id;
    /* Sensor scanning task */
    struct task_struct *sensor_scan_task;
    /* Scan task synchronization signal */
    wait_queue_head_t sensor_scan_wait;
    ka_atomic_t sensor_scan_task_state;
    /* This flag records whether system suspend, if true, the sensor scan task will not run */
    int sensor_scan_suspend_flag;

    ka_timer_list_t dms_sensor_check_timer;
    /*
     * Device data group, initial allocation, call for 1 device (device id = 0x1000) on the host side,
     * if device, AMP is 1 device, 910 SMP is 4 devices
     */
    struct dms_dev_ctrl_block base_cb;
    struct dms_dev_ctrl_block dev_cb_table[ASCEND_DEV_MAX_NUM];
};

struct state_item {
    uint32_t node_type;
    uint32_t node_id;
    struct dms_node *node;
};

struct dms_state_table {
    uint32_t num;
    struct state_item *item;
};

struct dms_state_table *dms_get_state_table(void);
struct dms_system_ctrl_block *dms_get_sys_ctrl_cb(void);
int dms_check_device_id(int dev_id);
DMS_MODE_T dms_get_rc_ep_mode(void);
struct dms_dev_ctrl_block *dms_get_dev_cb(int dev_id);
bool dms_is_devid_valid(int dev_id);

#define POWER_INFO_RESERVE_LEN 8
struct dsmi_dtm_node_s {
    int node_type;
    int node_id;
    unsigned int reserve[POWER_INFO_RESERVE_LEN];
};

int dms_check_node_type(int node_type);
void dev_node_release(int owner_pid);
int dms_register_dev_node(struct dms_node *node);
int dms_unregister_dev_node(struct dms_node *node);
struct dms_node *dms_get_devnode_cb(u32 dev_id, int node_type, int node_id);
struct dms_node *dms_get_devnode_cb_nolock(u32 dev_id, int node_type, int node_id);
int dms_update_devnode_cb(u32 dev_id, int node_type, int node_id, int (*update_dms_node)(struct dms_node *node));
int dms_traverse_devnode_get_capacity(u32 dev_id, struct dsmi_dtm_node_s node_info[], unsigned int size,
                                      unsigned int *out_size);
int dms_devnode_enable_device(u32 dev_id, int node_type, int node_id);
int dms_devnode_disable_device(u32 dev_id, int node_type, int node_id);
int dms_devnode_get_power_info(u32 dev_id, int node_type, int node_id, void *buf, unsigned int size);
int dms_devnode_set_power_info(u32 dev_id, int node_type, int node_id, void *buf, unsigned int size);
ssize_t dms_devnode_print_node_list(char *buf);

int dms_get_node_type_str(unsigned short node_type, char *node_str, unsigned int str_len);

extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern int memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
extern int strcpy_s(char *strDest, size_t destMax, const char *strSrc);
extern int strcat_s(char *strDest, size_t destMax, const char *strSrc);
extern int strncat_s(char *strDest, size_t destMax, const char *strSrc, size_t count);
extern int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);

#endif