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

#include "ka_kernel_def_pub.h"
#include "msg_chan_main.h"

STATIC int devdrv_set_common_msg_client(const struct devdrv_common_msg_client *msg_client, bool register_flag)
{
    u32 type;
    struct devdrv_msg_client *client_info = devdrv_get_msg_client();

    if (msg_client == NULL) {
        devdrv_err("Input parameter is null.\n");
        return -EINVAL;
    }
    type = (u32)msg_client->type;
    if (type >= (u32)DEVDRV_COMMON_MSG_TYPE_MAX) {
        devdrv_err("Input parameter is invalid. (msg_client_type=%u;flag=%d)\n", type, (int)register_flag);
        return -EINVAL;
    }

    ka_task_mutex_lock(&client_info->lock);
    if (register_flag == true) {
        client_info->comm[type] = msg_client;
    } else {
        client_info->comm[type] = NULL;
    }
    ka_task_mutex_unlock(&client_info->lock);
    return 0;
}

int devdrv_register_common_msg_client(const struct devdrv_common_msg_client *msg_client)
{
    int ret;
    struct devdrv_comm_dev_ops *dev_ops;

    ret = devdrv_set_common_msg_client(msg_client, true); // first save all client info, enable will register
    if (ret != 0) {
        return ret;
    }

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops != NULL) {
        ret = dev_ops->ops.register_common_msg_client(msg_client);
        devdrv_sub_ops_ref(dev_ops);
    }
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_register_common_msg_client);

int devdrv_unregister_common_msg_client(u32 devid, const struct devdrv_common_msg_client *msg_client)
{
    int ret = 0;
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    u32 index_id;

    (void)uda_udevid_to_add_id(devid, &index_id);
    dev_ops = devdrv_add_ops_ref();
    if ((dev_ops != NULL) && (dev_ops->ops.unregister_common_msg_client != NULL)) {
        ret = dev_ops->ops.unregister_common_msg_client(index_id, msg_client);
        devdrv_sub_ops_ref(dev_ops);
    }
    (void)devdrv_set_common_msg_client(msg_client, false);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_unregister_common_msg_client);

int devdrv_common_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len,
                           enum devdrv_common_msg_type msg_type)
{
    int ret;
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    u32 index_id;
    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Msg chan send common msg fail.\n");
        return -ENODEV;
    }
    index_id = devdrv_get_index_id_by_devid(devid);
    ret = dev_ops->ops.common_msg_send(index_id, data, in_data_len, out_data_len, real_out_len, msg_type);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_common_msg_send);