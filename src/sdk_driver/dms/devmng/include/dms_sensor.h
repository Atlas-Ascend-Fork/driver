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

#ifndef __DMS_SENSOR_H__
#define __DMS_SENSOR_H__

#include "ka_system_pub.h"
#include "dms/dms_interface.h"
#include "kernel_version_adapt.h"
#include "ascend_hal_error.h"
#include "drv_type.h"

#include "fms/fms_dtm.h"
#include "fms/fms_smf.h"
/* ******************************************Interface definition
 * start******************************************************* */
#define DMS_SENSOR_CMD_NAME "DMS_SENSOR"

/* Event parameter length */
#define DMS_EVENT_LENGTH 3

/* Sensor enable flag */
#define DMS_SENSOR_ENABLE_FALG 1
#define DMS_SENSOR_DISABLE_FALG 0

/* Sensor event report enable status */
#define DMS_SENSOR_EVENT_REPORT_ENABLE 1
#define DMS_SENSOR_EVENT_REPORT_DISABLE 0

/* Sensor detection processing flag */
#define DMS_SENSOR_PROC_ENABLE_FLAG 0
#define DMS_SENSOR_PROC_DISABLE_FLAG 1

/* Detection enable flag of sensor instance */
#define DMS_SENSOR_OBJECT_ENABLE_FLAG 1
#define DMS_SENSOR_OBJECT_DISABLE_FLAG 0

/* Sensor properties */
#define DMS_SENSOR_ATTRIB_THRES_SET_ENABLE 0x01 /* Sensor threshold can be set */
#define DMS_SENSOR_ATTRIB_THRES_NONE 0          /* No property settings */

/* Repeat mark of sensor information table */
#define DMS_SENSOR_TABLE_REPEAT 1
#define DMS_SENSOR_TABLE_NOT_REPEAT 0

#define DMS_MASK_16_BIT 16

/* ******************************************Interface definition
 * end******************************************************* */

/* Overload time of one description */
#define DMS_SCAN_OUT_TIME_MAX 70 // ms

/* The sensor periodically detects whether the processing time is recorded or not */
#define DMS_SENSOR_CHECK_RECORD 1
#define DMS_SENSOR_CHECK_NOT_RECORD 0

/* The sensor information table has the correct format and illegal flag */
#define DMS_SENSOR_TABLE_VALID 1
#define DMS_SENSOR_TABLE_INVALID 0

/* Registration environment type */
#define DMS_SENSOR_ENV_KERNEL_SPACE 0
#define DMS_SENSOR_ENV_USER_SPACE 1

/* The state of the sensor state, has changed, has not changed */
#define DMS_SENSOR_STATUS_CHANGED 1
#define DMS_SENSOR_STATUS_NO_CHANGE 0

/* The maximum number of entries recorded by the sensor timing detection execution time */
#define DMS_MAX_TIME_RECORD_COUNT 20

/* The maximum number of  sensor num every node */
#define DMS_MAX_NODE_SENSOR_COUNT 64

/* The mask used to get the lowest bit value */
#define DMS_BITS_MASK 0x00000001

/* sensor overload time of one scan */
#define DMS_SENSOR_SCAN_OUT_TIME 1000 // 1ms
/* device overload time of one scan */
#define DMS_DEV_SENSOR_SCAN_OUT_TIME 50000 // 50ms

#ifndef EVENT_INFO_CONFIG_PATH
#ifdef CFG_HOST_ENV
#define EVENT_INFO_CONFIG_PATH "/etc/dms_events_conf.lst"
#else
#define EVENT_INFO_CONFIG_PATH "/var/dms_events_conf.lst"
#endif
#endif
#define EVENT_INFO_ARRAY_MAX 2000

struct dms_event_config {
    unsigned int event_code;
    unsigned int severity;
};

struct dms_eventinfo_from_config {
    struct dms_event_config event_configs[EVENT_INFO_ARRAY_MAX];
    int config_cnt;
};

extern struct dms_eventinfo_from_config g_event_configs;

unsigned int dms_init_dev_sensor_cb(int deviceid, struct dms_dev_sensor_cb *sensor_cb);

void dms_clean_dev_sensor_cb(struct dms_dev_sensor_cb *sensor_cb);

void dms_sensor_scan_proc(struct dms_dev_sensor_cb *dev_sensor_cb);

void dms_printf_sensor_time_recorder(struct dms_dev_sensor_cb *pdev_sen_cb);

unsigned int dms_sensor_check_result_process(unsigned short sensor_class, struct dms_sensor_object_cb *psensor_obj_cb,
                                             unsigned int *status, unsigned char *event_type);

int dms_add_one_sensor_event(struct dms_sensor_object_cb *psensor_obj_cb,
                             struct dms_sensor_event_data_item *pevent_data);
int dms_get_event_string(unsigned char sensor_type, unsigned char event_offset, char *event_str, int inbuf_size);
int dms_resume_all_sensor_event(struct dms_sensor_object_cb *psensor_obj_cb);
int dms_check_sensor_data(struct dms_sensor_object_cb *sensor_obj_cb, unsigned char event_data);
DMS_EVENT_LIST_ITEM *sensor_add_event_to_list(DMS_EVENT_LIST_ITEM **pp_event_list,
                                              struct dms_sensor_event_data_item *pevent_data,
                                              struct dms_sensor_object_cb *psensor_obj_cb);
int dms_update_sensor_list(struct dms_sensor_object_cb *p_sensor_obj_cb, DMS_EVENT_LIST_ITEM *p_new_list);
int dms_sensor_get_dev_health(struct dms_dev_sensor_cb *dev_sensor_cb);
int dms_get_sensor_type_name(unsigned char sensor_type, char *type_name, int inbuf_size);
int dms_sensor_clean_health_events(struct dms_dev_sensor_cb *dev_sensor_cb);
int dms_sensor_mask_events(struct dms_dev_sensor_cb *dev_sensor_cb, u8 mask, u16 node_type, u8 sensor_type,
                           u8 event_state);
bool dms_sensor_check_mask_enable(unsigned int mask, unsigned int offset);
unsigned int dms_sen_init_sensor(void);
unsigned int dms_sen_exit_sensor_event(void);
unsigned int dms_init_dev_sensor_cb(int deviceid, struct dms_dev_sensor_cb *sensor_cb);
void dms_exit_dev_sensor_cb(struct dms_dev_sensor_cb *dev_sensor_cb);
int dms_sensor_scan_one_node_object(struct dms_dev_sensor_cb *dev_sensor_cb, struct dms_node_sensor_cb *node_sensor_cb,
                                    struct dms_sensor_object_cb *psensor_obj_cb,
                                    struct dms_sensor_scan_time_recorder *ptime_recorder);
unsigned long dms_get_time_change(ka_ktime_t start, ka_ktime_t end);
unsigned long dms_get_time_change_ms(ka_ktime_t start, ka_ktime_t end);
int dms_mgnt_clockid_init(void);
int dms_sensor_inject_fault(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int dms_sensor_get_fault_inject_info(void *feature, char *in, u32 in_len, char *out, u32 out_len);
#endif
