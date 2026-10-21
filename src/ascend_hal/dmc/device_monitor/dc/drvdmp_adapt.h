/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DRVDMP_ADAPT_H
#define DRVDMP_ADAPT_H

#include "hash_table.h"
#include "device_monitor_type.h"

#define DMP_THREAD_LIMIT_MAX 1024
#define SCAN_SLEEP_TIME 10000

#ifdef CFG_FEATURE_READ_ONLY_CONTAINER /* 310Brc read-only container. */
#define DMP_SERVER_PATH_MANAGEMENT "/dev/shm/dmp/dmpdaemon_management.socket"
#define DMP_SERVER_PATH_SERVICE "/dev/shm/dmp/dmpdaemon_service.socket"
#else
#define DMP_SERVER_PATH_MANAGEMENT "/var/dmp/dmpdaemon_management.socket"
#define DMP_SERVER_PATH_SERVICE "/var/dmp/dmpdaemon_service.socket"
#endif

signed int IT_cmd_insert_to_table_ex(hash_table *hashtable);
signed int davinci_cmd_insert_to_table_ex(hash_table *hashtable);

typedef int (*get_device_info_handle)(unsigned int device_id, unsigned int vfid, unsigned int sub_cmd, void *buf,
                                      unsigned int *size);
typedef int (*set_device_info_handle)(unsigned int device_id, unsigned int sub_cmd, void *buf, unsigned int size);

typedef enum {
    DMANAGE_GET_DEVICE_INFO,
    DMANAGE_SET_DEVICE_INFO,
    DMANAGE_SET_DEVICE_INFO_EX /* This parameter can be set in the non-physical machine scenario. */
} DMANAGE_OPERATE_DEVICE_INFO_FLAG;

struct device_info_ops {
    unsigned int main_cmd;
    unsigned int sub_cmd;
    DMANAGE_OPERATE_DEVICE_INFO_FLAG operate_flag;
    get_device_info_handle get_device_info_fun;
    set_device_info_handle set_device_info_fun;
};

void dev_mon_api_get_cgroup_info(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);

#endif