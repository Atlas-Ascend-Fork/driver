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

int devdrv_register_s2s_msg_proc_func(enum devdrv_s2s_msg_type msg_type, s2s_msg_recv func)
{
    int ret;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_warn("Can not get ops, dev may not have been probed.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.register_s2s_msg == NULL) {
        devdrv_err("ops.register_s2s_msg is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return -EOPNOTSUPP;
    }
    ret = dev_ops->ops.register_s2s_msg(msg_type, func);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_register_s2s_msg_proc_func);

int devdrv_unregister_s2s_msg_proc_func(enum devdrv_s2s_msg_type msg_type)
{
    int ret;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_warn("Can not get ops, dev may have been removed.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.unregister_s2s_msg == NULL) {
        devdrv_err("ops.unregister_s2s_msg is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return -EOPNOTSUPP;
    }
    ret = dev_ops->ops.unregister_s2s_msg(msg_type);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_unregister_s2s_msg_proc_func);

int devdrv_s2s_msg_send(u32 local_devid, u32 sdid, enum devdrv_s2s_msg_type msg_type, u32 direction,
                        struct data_input_info *data_info)
{
    int ret;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Send s2s msg fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.send_s2s_msg == NULL) {
        devdrv_err("ops.send_s2s_msg is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return -EOPNOTSUPP;
    }
    ret = dev_ops->ops.send_s2s_msg(local_devid, sdid, msg_type, direction, data_info);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_s2s_msg_send);

int devdrv_s2s_async_msg_recv(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type, struct data_recv_info *data_info)
{
    int ret;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Receive s2s async msg fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.recv_s2s_async_msg == NULL) {
        devdrv_err("Ops.recv_s2s_async_msg is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return -EOPNOTSUPP;
    }
    ret = dev_ops->ops.recv_s2s_async_msg(devid, sdid, msg_type, data_info);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_s2s_async_msg_recv);