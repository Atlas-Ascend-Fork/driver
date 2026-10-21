/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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

#include "msg_chan_main.h"
#include "ka_kernel_def_pub.h"

void *devdrv_pcimsg_alloc_trans_queue(u32 udevid, struct devdrv_trans_msg_chan_info *chan_info)
{
    void *ret;
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return NULL;
    }

    if (dev_ops->ops.alloc_trans_queue == NULL) {
        devdrv_err("ops.alloc_trans_queue is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return NULL;
    }

    ret = dev_ops->ops.alloc_trans_queue(udevid, chan_info);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_pcimsg_alloc_trans_queue);

int devdrv_pcimsg_realease_trans_queue(void *msg_chan)
{
    int ret;
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.realease_trans_queue == NULL) {
        devdrv_err("ops.release_trans_queue is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return -EOPNOTSUPP;
    }

    ret = dev_ops->ops.realease_trans_queue(msg_chan);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_pcimsg_realease_trans_queue);

void devdrv_msg_ring_doorbell(void *msg_chan)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return;
    }
    if (dev_ops->ops.ring_doorbell == NULL) {
        devdrv_err("ops.ring_doorbell is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return;
    }

    dev_ops->ops.ring_doorbell(msg_chan);
    devdrv_sub_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(devdrv_msg_ring_doorbell);

void devdrv_msg_ring_cq_doorbell(void *msg_chan)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return;
    }
    if (dev_ops->ops.ring_cq_doorbell == NULL) {
        devdrv_err("ops.ring_cq_doorbell is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return;
    }
    dev_ops->ops.ring_cq_doorbell(msg_chan);
    devdrv_sub_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(devdrv_msg_ring_cq_doorbell);

void *devdrv_get_msg_chan_host_sq_head(void *msg_chan, u32 *head)
{
    void *ret;
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return NULL;
    }
    if (dev_ops->ops.get_sq_head == NULL) {
        devdrv_err("ops.get_sq_head is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return NULL;
    }
    ret = dev_ops->ops.get_sq_head(msg_chan, head);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_msg_chan_host_sq_head);

void devdrv_move_msg_chan_host_sq_head(void *msg_chan)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return;
    }
    if (dev_ops->ops.move_sq_head == NULL) {
        devdrv_err("ops.move_sq_head is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return;
    }
    dev_ops->ops.move_sq_head(msg_chan);
    devdrv_sub_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(devdrv_move_msg_chan_host_sq_head);

void *devdrv_get_msg_chan_host_cq_head(void *msg_chan)
{
    void *ret;
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return NULL;
    }
    if (dev_ops->ops.get_cq_head == NULL) {
        devdrv_err("ops.get_cq_head is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return NULL;
    }
    ret = dev_ops->ops.get_cq_head(msg_chan);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_msg_chan_host_cq_head);

void devdrv_move_msg_chan_host_cq_head(void *msg_chan)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return;
    }
    if (dev_ops->ops.move_cq_head == NULL) {
        devdrv_err("ops.move_cq_head is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return;
    }
    dev_ops->ops.move_cq_head(msg_chan);
    devdrv_sub_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(devdrv_move_msg_chan_host_cq_head);

void devdrv_set_msg_chan_slave_sq_head(void *msg_chan, u32 head)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return;
    }

    if (dev_ops->ops.set_sq_head == NULL) {
        devdrv_err("ops.set_sq_head is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return;
    }
    dev_ops->ops.set_sq_head(msg_chan, head);
    devdrv_sub_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(devdrv_set_msg_chan_slave_sq_head);

void *devdrv_get_msg_chan_slave_sq_tail(void *msg_chan, u32 *tail)
{
    void *ret;
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return NULL;
    }

    if (dev_ops->ops.get_sq_tail == NULL) {
        devdrv_err("ops.get_sq_tail is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return NULL;
    }
    ret = dev_ops->ops.get_sq_tail(msg_chan, tail);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_msg_chan_slave_sq_tail);

void devdrv_move_msg_chan_slave_sq_tail(void *msg_chan)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return;
    }

    if (dev_ops->ops.move_sq_tail == NULL) {
        devdrv_err("ops.move_sq_tail is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return;
    }
    dev_ops->ops.move_sq_tail(msg_chan);
    devdrv_sub_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(devdrv_move_msg_chan_slave_sq_tail);

bool devdrv_msg_chan_slave_sq_full_check(void *msg_chan)
{
    bool ret;
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return true;
    }
    if (dev_ops->ops.sq_full_check == NULL) {
        devdrv_err("ops.sq_full_check is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return true;
    }
    ret = dev_ops->ops.sq_full_check(msg_chan);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_msg_chan_slave_sq_full_check);

void *devdrv_get_msg_chan_slave_cq_tail(void *msg_chan)
{
    void *ret;
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return NULL;
    }

    if (dev_ops->ops.get_cq_tail == NULL) {
        devdrv_err("ops.get_cq_tail is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return NULL;
    }
    ret = dev_ops->ops.get_cq_tail(msg_chan);
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_msg_chan_slave_cq_tail);

void devdrv_move_msg_chan_slave_cq_tail(void *msg_chan)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("dev_ops is null.\n");
        return;
    }

    if (dev_ops->ops.move_cq_tail == NULL) {
        devdrv_err("ops.move_cq_tail is null.\n");
        devdrv_sub_ops_ref(dev_ops);
        return;
    }
    dev_ops->ops.move_cq_tail(msg_chan);
    devdrv_sub_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(devdrv_move_msg_chan_slave_cq_tail);
