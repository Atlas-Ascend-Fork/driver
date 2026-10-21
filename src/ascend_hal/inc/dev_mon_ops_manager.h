/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DEV_MON_OPS_MANAGER_H__
#define __DEV_MON_OPS_MANAGER_H__

#define DEV_MON_DOCKER_SCENES 1
#define DEV_MON_VIRTUAL_SCENES 2
#define DEV_MON_ALL_SCENES 0xFFFFFFFF

#define DEV_MON_REQUEST_COMMAND 1
#define DEV_MON_SETTING_COMMAND 2

typedef int (*dev_mon_device_info_handle)(unsigned int device_id, unsigned int main_cmd, unsigned int sub_cmd,
                                          void *buf, unsigned int *size);

struct dev_mon_device_info_command {
    unsigned short main_cmd;
    unsigned short sub_cmd;
    unsigned int uid;
    unsigned int scenes;
    unsigned int command_type;
    unsigned int reserved[4]; // 3 reserved 12 byte
    dev_mon_device_info_handle device_info_ops;
};

int dev_mon_register_device_info(struct dev_mon_device_info_command *device_info_ops);
struct dev_mon_device_info_command *dev_mon_get_device_info_command(unsigned int main_cmd, unsigned int sub_cmd,
                                                                    unsigned int command_type);

#endif
