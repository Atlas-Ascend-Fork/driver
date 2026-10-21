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

#ifndef FMS_SMF_H
#define FMS_SMF_H

#include "ka_list_pub.h"
#include "ka_task_pub.h"

#include "dms/dms_shm_info.h"
#include "dms/dms_cmd_def.h"
#include "fms/fms_dtm.h"
#include "fms/fms_fpdc.h"
#include "dms_device_node_type.h"

typedef struct {
    struct dms_event_para event;
    ka_list_head_t node;
} DMS_EVENT_NODE_STRU;

struct dms_device_event {
    ka_list_head_t head;
    int event_num; /* max 128 */
    ka_mutex_t lock;
    unsigned int highest_severity;
};

typedef enum {
    DMS_DISTRIBUTE_PRIORITY0 = 0,
    DMS_DISTRIBUTE_PRIORITY1,
    DMS_DISTRIBUTE_PRIORITY2,
    DMS_DISTRIBUTE_PRIORITY3,
    DMS_DISTRIBUTE_PRIORITY_MAX
} DMS_DISTRIBUTE_PRIORITY;

typedef enum {
    DMS_ET_SENSOR,
    DMS_ET_OEM,
} DMS_EVENT_TYPE_T;

typedef struct dms_sensor_event {
    /* The index of the sensor, the current coding rule is SensorType + SensorInstance */
    unsigned short sensor_num;
    char sensor_name[DMS_SENSOR_DESCRIPT_LENGTH];
    /* Sensor type */
    unsigned char sensor_type;
    /* 0-resume, 1-occur, 2-one time */
    unsigned char assertion;
    /* Current state */
    unsigned char event_state;
    unsigned char param_len;
    char param_buffer[DMS_MAX_EVENT_DATA_LENGTH];
    unsigned char event_info[DMS_MAX_EVENT_INFO_LENGTH];
} DMS_SENSOR_EVENT_T;

typedef union {
    DMS_SENSOR_EVENT_T sensor_event;
} DMS_EVENT_UNION_T;

struct dms_event_obj {
    unsigned short deviceid; /* device id of host */
    unsigned int node_type;
    unsigned int node_id;
    unsigned int sub_node_type;
    unsigned int sub_node_id;
    DMS_EVENT_TYPE_T event_type;
    unsigned long long time_stamp; /*  event time  */
    DMS_SEVERITY_T severity;
    DMS_EVENT_UNION_T event;
    /* Alarm serial number */
    unsigned int alarm_serial_num;
    int pid;
};

enum dms_event_poll_flag {
    EVENT_POLL_EXIT = 0,
    EVENT_POLL_WORK,
    EVENT_POLL_STOP,
    EVENT_POLL_MAX
};

struct smf_event_adapt {
    int (*subscribe_from_device)(u32 phyid);
    int (*clean_to_device)(u32 phyid);
    int (*mask_event_code)(u32 phyid, u32 event_code, u8 mask);
    int (*get_event_code_from_bar)(u32 devid, u32 *health_code, u32 health_len, struct shm_event_code *event_code,
                                   u32 event_len);
    int (*get_health_code_from_bar)(u32 devid, u32 *health_code, u32 health_len);
    int (*distribute_all_devices_event_to_bar)(void);
    int (*distribute_to_bar)(u32 phyid);
    int (*logical_id_to_physical_id)(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
    int (*get_event_para)(int phyid, struct dms_event_para *dms_event, u32 in_cnt, u32 *event_num);
    int (*get_notify_event_para)(int phyid, struct dms_event_para *dms_event, u32 in_cnt, u32 *event_num);
    int (*get_event_code_from_local)(u32 devid, u32 *health_code, struct shm_event_code *event_code, u32 event_len);
    int (*get_health_code_from_local)(u32 devid, u32 *health_code);
    int (*get_container_ns_id)(u32 *ns_id);
};

typedef int (*add_exception_handle)(u32 devid, u32 code, struct timespec stamp);
typedef int (*DMS_EVENT_DISTRIBUTE_HANDLE_T)(DMS_EVENT_NODE_STRU *exception_node);

int dms_smf_suspend(unsigned int devid);
int dms_smf_resume(unsigned int devid);

int dms_event_get_notify_serial_num(void);
void dms_event_cb_release(int owner_pid);
int dms_get_event_code_from_event_cb(u32 devid, u32 *health_code, u32 health_len, struct shm_event_code *event_code,
                                     u32 event_len);
int dms_event_is_converge(void);

ssize_t dms_event_dfx_channel_flux_store(const char *buf, size_t count);
ssize_t dms_event_dfx_channel_flux_show(char *str);
ssize_t dms_event_dfx_convergent_diagrams_store(const char *buf, size_t count);
ssize_t dms_event_dfx_convergent_diagrams_show(char *str);
ssize_t dms_event_dfx_event_list_store(const char *buf, size_t count);
ssize_t dms_event_dfx_event_list_show(char *str);
ssize_t dms_event_dfx_mask_list_store(const char *buf, size_t count);
ssize_t dms_event_dfx_mask_list_show(char *str);
ssize_t dms_event_dfx_subscribe_handle_show(char *str);
ssize_t dms_event_dfx_subscribe_process_show(char *str);

int dms_event_set_add_exception_handle(add_exception_handle func);

struct dms_device_event *dms_get_device_event(u32 dev_id);
struct dms_device_event *dms_get_device_notify_event(u32 dev_id);
void dms_release_one_device_remote_event(unsigned int dev_id);
int dms_event_get_exception(int dev_id, struct dms_event_filter *filter, struct dms_event_para *fault_event,
                            int timeout, enum cmd_source cmd_src);
void dms_event_release_proc(int tgid, int pid);
int dms_event_subscribe_register(DMS_EVENT_DISTRIBUTE_HANDLE_T handle_func, DMS_DISTRIBUTE_PRIORITY priority);
void dms_event_subscribe_unregister(DMS_EVENT_DISTRIBUTE_HANDLE_T handle_func);
int dms_event_distribute_handle(DMS_EVENT_NODE_STRU *exception_node, DMS_DISTRIBUTE_PRIORITY priority);

int dms_event_report(struct dms_event_obj *event_obj);
int dms_event_clear_by_phyid(u32 phyid);
int dms_event_mask_by_phyid(u32 phyid, u32 event_id, u8 mask);
int dms_get_event_code_from_sensor(u32 devid, u32 *health_code, u32 health_len, struct shm_event_code *event_code,
                                   u32 event_len);
void dms_event_set_poll_task_flag(enum dms_event_poll_flag flag);

int smf_event_adapt_init(struct smf_event_adapt *apt);
void smf_event_adapt_uninit(void);

int dms_sensor_event_notify(unsigned int dev_id, struct dms_sensor_object_cfg *psensor_obj_cfg);

ssize_t dms_sensor_print_sensor_list(char *buf);
int dms_get_event_severity(unsigned int node_type, unsigned char sensor_type, unsigned char event_offset,
                           unsigned int *severity);
int dms_sensor_get_health_events(struct dms_dev_sensor_cb *dev_sensor_cb, struct dms_event_obj *event_buff,
                                 unsigned int input_count, unsigned int *output_count);
unsigned int dms_sensor_register(struct dms_node *owner_node, struct dms_sensor_object_cfg *psensor_obj_cfg);
unsigned int dms_sensor_register_for_userspace(struct dms_node *owner_node,
                                               struct dms_sensor_object_cfg *psensor_obj_cfg);
unsigned int dms_sensor_object_unregister(struct dms_node *owner_node, struct dms_sensor_object_cfg *psensor_obj_cfg);
unsigned int dms_sensor_node_unregister(struct dms_node *owner_node);
void dms_sensor_release(int owner_pid);

#endif