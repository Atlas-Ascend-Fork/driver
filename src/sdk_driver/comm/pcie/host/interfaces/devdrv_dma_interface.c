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
#include "comm_kernel_interface.h"
#include "devdrv_util.h"
#include "devdrv_atu.h"
#include "devdrv_dma.h"
#include "devdrv_pci.h"
#include "devdrv_ctrl.h"
#include "pbl/pbl_uda.h"

int devdrv_dma_sync_copy_plus_inner(u32 index_id, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst,
                                    u32 size, enum devdrv_dma_direction direction)
{
    int ret;
    struct devdrv_dma_node dma_node = {0};
    struct devdrv_dma_dev *dma_dev = NULL;
    struct devdrv_dma_copy_para para = {0};

    dma_node.src_addr = src;
    dma_node.dst_addr = dst;
    dma_node.size = size;
    dma_node.direction = direction;

    dma_dev = devdrv_get_dma_dev(index_id);
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (index_id=%u)\n", index_id);
        return -EINVAL;
    }

    ret = devdrv_dma_para_check(index_id, type, DEVDRV_DMA_SYNC, NULL);
    if (ret != 0) {
        devdrv_err("Dma parameter check failed. (index_id=%u; ret=%d)\n", index_id, ret);
        return ret;
    }

    ret = devdrv_dma_node_check(index_id, &dma_node, 1, dma_dev);
    if (ret != 0) {
        devdrv_err("Dma node check failed. (index_id=%u; ret=%d)\n", index_id, ret);
        return ret;
    }

    devdrv_dma_copy_type_info_init(&para, type, DEVDRV_DMA_WAIT_INTR, DEVDRV_DMA_SYNC);
    devdrv_dma_copy_para_info_init(&para, DEVDRV_DMA_VA_COPY, instance, NULL);
    ret = devdrv_dma_copy(dma_dev, &dma_node, 1, &para);

    return ret;
}

int devdrv_dma_sync_copy_inner(u32 index_id, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                               enum devdrv_dma_direction direction)
{
    return devdrv_dma_sync_copy_plus_inner(index_id, type, DEVDRV_INVALID_INSTANCE, src, dst, size, direction);
}

int devdrv_dma_sync_link_copy_plus_extend_inner(u32 index_id, enum devdrv_dma_data_type type, int wait_type,
                                                int instance, struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    int ret;
    struct devdrv_dma_dev *dma_dev = NULL;
    struct devdrv_dma_copy_para para = {0};

    dma_dev = devdrv_get_dma_dev(index_id);
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (index_id=%u)\n", index_id);
        return -EINVAL;
    }

    ret = devdrv_dma_para_check(index_id, type, DEVDRV_DMA_SYNC, NULL);
    if (ret != 0) {
        devdrv_err("Dma parameter check failed. (index_id=%u; ret=%d)\n", index_id, ret);
        return ret;
    }

    ret = devdrv_dma_node_check(index_id, dma_node, node_cnt, dma_dev);
    if (ret != 0) {
        devdrv_err("Dma node check failed. (index_id=%u; ret=%d)\n", index_id, ret);
        return ret;
    }

    devdrv_dma_copy_type_info_init(&para, type, wait_type, DEVDRV_DMA_SYNC);
    devdrv_dma_copy_para_info_init(&para, DEVDRV_DMA_PA_COPY, instance, NULL);
    ret = devdrv_dma_copy(dma_dev, dma_node, node_cnt, &para);

    return ret;
}

int devdrv_dma_sync_link_copy_extend_inner(u32 index_id, enum devdrv_dma_data_type type, int wait_type,
                                           struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    return devdrv_dma_sync_link_copy_plus_extend_inner(index_id, type, wait_type, DEVDRV_INVALID_INSTANCE, dma_node,
                                                       node_cnt);
}

int devdrv_dma_async_copy_plus_inner(u32 index_id, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst,
                                     u32 size, enum devdrv_dma_direction direction,
                                     struct devdrv_asyn_dma_para_info *para_info)
{
    int ret;
    struct devdrv_dma_node dma_node = {0};
    struct devdrv_dma_dev *dma_dev = NULL;
    struct devdrv_dma_copy_para para = {0};

    dma_node.src_addr = src;
    dma_node.dst_addr = dst;
    dma_node.size = size;
    dma_node.direction = direction;

    dma_dev = devdrv_get_dma_dev(index_id);
    if (dma_dev == NULL) {
        devdrv_err_spinlock("Get dma_dev failed. (index_id=%u)\n", index_id);
        return -EINVAL;
    }

    ret = devdrv_dma_para_check(index_id, type, DEVDRV_DMA_ASYNC, para_info);
    if (ret != 0) {
        devdrv_err_spinlock("Dma parameter check failed. (index_id=%u; ret=%d)\n", index_id, ret);
        return ret;
    }

    ret = devdrv_dma_node_check(index_id, &dma_node, 1, dma_dev);
    if (ret != 0) {
        devdrv_err_spinlock("Dma node check failed. (index_id=%u; ret=%d)\n", index_id, ret);
        return ret;
    }

    devdrv_dma_copy_type_info_init(&para, type, DEVDRV_DMA_WAIT_INTR, DEVDRV_DMA_ASYNC);
    devdrv_dma_copy_para_info_init(&para, DEVDRV_DMA_VA_COPY, instance, para_info);
    ret = devdrv_dma_copy(dma_dev, &dma_node, 1, &para);

    return ret;
}

int devdrv_dma_async_copy_inner(u32 index_id, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                                enum devdrv_dma_direction direction, struct devdrv_asyn_dma_para_info *para_info)
{
    return devdrv_dma_async_copy_plus_inner(index_id, type, DEVDRV_INVALID_INSTANCE, src, dst, size, direction,
                                            para_info);
}

int devdrv_dma_sync_link_copy_plus_inner(u32 index_id, enum devdrv_dma_data_type type, int wait_type, int instance,
                                         struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    int ret;
    struct devdrv_dma_dev *dma_dev = NULL;
    struct devdrv_dma_copy_para para = {0};

    dma_dev = devdrv_get_dma_dev(index_id);
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (index_id=%u)\n", index_id);
        return -EINVAL;
    }

    ret = devdrv_dma_para_check(index_id, type, DEVDRV_DMA_SYNC, NULL);
    if (ret != 0) {
        devdrv_err("Dma parameter check failed. (index_id=%u; ret=%d)\n", index_id, ret);
        return ret;
    }

    ret = devdrv_dma_node_check(index_id, dma_node, node_cnt, dma_dev);
    if (ret != 0) {
        devdrv_err("Dma node check failed. (index_id=%u; ret=%d)\n", index_id, ret);
        return ret;
    }

    devdrv_dma_copy_type_info_init(&para, type, wait_type, DEVDRV_DMA_SYNC);
    devdrv_dma_copy_para_info_init(&para, DEVDRV_DMA_VA_COPY, instance, NULL);
    ret = devdrv_dma_copy(dma_dev, dma_node, node_cnt, &para);

    return ret;
}

int devdrv_dma_sync_link_copy_inner(u32 index_id, enum devdrv_dma_data_type type, int wait_type,
                                    struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    return devdrv_dma_sync_link_copy_plus_inner(index_id, type, wait_type, DEVDRV_INVALID_INSTANCE, dma_node, node_cnt);
}

int devdrv_dma_async_link_copy_plus_inner(u32 index_id, enum devdrv_dma_data_type type, int instance,
                                          struct devdrv_dma_node *dma_node, u32 node_cnt,
                                          struct devdrv_asyn_dma_para_info *para_info)
{
    int ret;
    struct devdrv_dma_dev *dma_dev = NULL;
    struct devdrv_dma_copy_para para = {0};

    dma_dev = devdrv_get_dma_dev(index_id);
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (index_id=%u)\n", index_id);
        return -EINVAL;
    }

    ret = devdrv_dma_para_check(index_id, type, DEVDRV_DMA_ASYNC, para_info);
    if (ret != 0) {
        devdrv_err("Dma parameter check failed. (index_id=%u; ret=%d)\n", index_id, ret);
        return ret;
    }

    ret = devdrv_dma_node_check(index_id, dma_node, node_cnt, dma_dev);
    if (ret != 0) {
        devdrv_err("Dma node check failed. (index_id=%u; ret=%d)\n", index_id, ret);
        return ret;
    }

    devdrv_dma_copy_type_info_init(&para, type, DEVDRV_DMA_WAIT_INTR, DEVDRV_DMA_ASYNC);
    devdrv_dma_copy_para_info_init(&para, DEVDRV_DMA_VA_COPY, instance, para_info);
    ret = devdrv_dma_copy(dma_dev, dma_node, node_cnt, &para);

    return ret;
}

int devdrv_dma_async_link_copy_inner(u32 index_id, enum devdrv_dma_data_type type, struct devdrv_dma_node *dma_node,
                                     u32 node_cnt, struct devdrv_asyn_dma_para_info *para_info)
{
    return devdrv_dma_async_link_copy_plus_inner(index_id, type, DEVDRV_INVALID_INSTANCE, dma_node, node_cnt,
                                                 para_info);
}

int devdrv_dma_done_schedule_inner(u32 index_id, enum devdrv_dma_data_type type, int instance)
{
    struct devdrv_dma_dev *dma_dev = NULL;
    struct devdrv_dma_channel *dma_chan = NULL;
    struct data_type_chan *data_chan = NULL;
    u32 chan_id;
    u32 device_status;

    dma_dev = devdrv_get_dma_dev(index_id);
    if (dma_dev == NULL) {
        devdrv_err("Get dma_dev failed. (index_id=%u)\n", index_id);
        return -EINVAL;
    }

    if ((type >= DEVDRV_DMA_DATA_TYPE_MAX) || (type < DEVDRV_DMA_DATA_COMMON) ||
        (instance == DEVDRV_INVALID_INSTANCE)) {
        devdrv_err("Input parameter is invalid. (index_id=%u; type_tmp=%u; instance=%d)\n", index_id, (u32)type,
                   instance);
        return -EINVAL;
    }

    device_status = dma_dev->pci_ctrl->device_status;
    if ((device_status != DEVDRV_DEVICE_ALIVE) && (device_status != DEVDRV_DEVICE_SUSPEND)) {
        devdrv_warn("Device is abnormal, can't handle dma done schedule. (index_id=%u)\n", index_id);
        return -EINVAL;
    }

    if (dma_dev->dev_status != DEVDRV_DMA_ALIVE) {
        devdrv_warn("DMA dev is not alive. (index_id=%u; type_tmp=%u)\n", index_id, (u32)type);
        return -EINVAL;
    }

    data_chan = &dma_dev->data_chan[type];
    chan_id = data_chan->chan_start_id + ((u32)instance % (u32)data_chan->chan_num);
    dma_chan = &dma_dev->dma_chan[chan_id];
    if (dma_chan->chan_status != DEVDRV_DMA_CHAN_ENABLED) {
        devdrv_warn("DMA chan disabled. (index_id=%u; type_tmp=%u)\n", index_id, (u32)type);
        return -EINVAL;
    }
    devdrv_dma_done_task((unsigned long)(uintptr_t)dma_chan);
    return 0;
}

int devdrv_pci_dma_sync_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst, u32 size,
                                  enum devdrv_dma_direction direction)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_sync_copy_plus_inner(index_id, type, instance, src, dst, size, direction);
}

int devdrv_pci_dma_sync_copy(u32 udevid, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                             enum devdrv_dma_direction direction)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_sync_copy_inner(index_id, type, src, dst, size, direction);
}

int devdrv_pci_dma_sync_link_copy_plus_extend(u32 udevid, enum devdrv_dma_data_type type, int wait_type, int instance,
                                              struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_sync_link_copy_plus_extend_inner(index_id, type, wait_type, instance, dma_node, node_cnt);
}

int devdrv_pci_dma_sync_link_copy_extend(u32 udevid, enum devdrv_dma_data_type type, int wait_type,
                                         struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_sync_link_copy_extend_inner(index_id, type, wait_type, dma_node, node_cnt);
}

int devdrv_pci_dma_async_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst, u32 size,
                                   enum devdrv_dma_direction direction, struct devdrv_asyn_dma_para_info *para_info)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_async_copy_plus_inner(index_id, type, instance, src, dst, size, direction, para_info);
}

int devdrv_pci_dma_async_copy(u32 udevid, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                              enum devdrv_dma_direction direction, struct devdrv_asyn_dma_para_info *para_info)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_async_copy_inner(index_id, type, src, dst, size, direction, para_info);
}

int devdrv_pci_dma_sync_link_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int wait_type, int instance,
                                       struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_sync_link_copy_plus_inner(index_id, type, wait_type, instance, dma_node, node_cnt);
}

int devdrv_pci_dma_sync_link_copy(u32 udevid, enum devdrv_dma_data_type type, int wait_type,
                                  struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_sync_link_copy_inner(index_id, type, wait_type, dma_node, node_cnt);
}

int devdrv_pci_dma_async_link_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int instance,
                                        struct devdrv_dma_node *dma_node, u32 node_cnt,
                                        struct devdrv_asyn_dma_para_info *para_info)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_async_link_copy_plus_inner(index_id, type, instance, dma_node, node_cnt, para_info);
}

int devdrv_pci_dma_async_link_copy(u32 udevid, enum devdrv_dma_data_type type, struct devdrv_dma_node *dma_node,
                                   u32 node_cnt, struct devdrv_asyn_dma_para_info *para_info)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_async_link_copy_inner(index_id, type, dma_node, node_cnt, para_info);
}

int devdrv_pci_dma_done_schedule(u32 udevid, enum devdrv_dma_data_type type, int instance)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_dma_done_schedule_inner(index_id, type, instance);
}