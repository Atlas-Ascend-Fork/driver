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
#ifndef PBL_DAVINCI_API_H
#define PBL_DAVINCI_API_H

#include "ka_common_pub.h"
#include "ka_fs_pub.h"
#include "ka_task_pub.h"
#include "ka_type.h"

union status_para_un {
    u32 device_id;
    u32 process_id;
};

struct ascend_intf_get_status_para {
    u32 type;
    union status_para_un para;
};

#define NOTIFY_MODE_RELEASE_PREPARE 0x0001
#define NOTIFY_MODE_RELEASE_FINISH 0x0002
#define NOTIFY_MODE_REMOVE_PREPARE 0x0003

struct notifier_operations {
    struct module *owner;
    int (*notifier_call)(ka_file_t *file_op, unsigned long mode);
};
int drv_ascend_register_notify(const char *module_name, const struct notifier_operations *notifier);
int drv_ascend_unregister_notify(const char *module_name);

void drv_davinci_bind_ctrl_cpu(void *release_task);
int drv_davinci_register_sub_parallel_module(const char *module_name, const ka_file_operations_t *ops);
int drv_davinci_register_sub_module(const char *module_name, const ka_file_operations_t *ops);
int drv_davinci_register_sub_module_cnt(const char *module_name, const ka_file_operations_t *ops,
                                        unsigned int open_module_max);

int drv_ascend_unregister_sub_module(const char *module_name);
int drv_davinci_set_private_data(const ka_file_t *filep, const void *priv_data);
void *drv_davinci_get_private_data(const ka_file_t *filep);
u32 drv_davinci_get_device_id(const ka_file_t *filep);
int ascend_intf_report_process_status(ka_pid_t pid, unsigned int status);
bool davinci_intf_confirm_user(void);
u32 davinci_intf_get_manage_group(void);
int ascend_intf_report_device_status(unsigned int device_id, unsigned int status);
int ascend_intf_get_status(struct ascend_intf_get_status_para para, unsigned int *status);
struct device *davinci_intf_get_owner_device(void);
int drv_ascend_replace_sub_module_fops(const char *module_name, const ka_file_operations_t *ops);
int ascend_intf_is_pid_init(ka_pid_t process_id, const char *module_name);
bool ascend_intf_is_restrict_access(ka_file_t *filep);
#endif
