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
#include "ka_memory_pub.h"
#include "msg_chan_main.h"

#ifndef CFG_FEATURE_VPBL
void *hal_kernel_devdrv_dma_alloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    void *addr = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return NULL;
    }

    if (dev_ops->ops.devdrv_dma_alloc_coherent != NULL) {
        addr = dev_ops->ops.devdrv_dma_alloc_coherent(dev, size, dma_addr, gfp);
    }
    devdrv_sub_ops_ref(dev_ops);

    return addr;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_alloc_coherent);

void *hal_kernel_devdrv_dma_zalloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    void *addr = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return NULL;
    }

    if (dev_ops->ops.devdrv_dma_zalloc_coherent != NULL) {
        addr = dev_ops->ops.devdrv_dma_zalloc_coherent(dev, size, dma_addr, gfp);
    }
    devdrv_sub_ops_ref(dev_ops);

    return addr;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_zalloc_coherent);

void hal_kernel_devdrv_dma_free_coherent(ka_device_t *dev, size_t size, void *addr, ka_dma_addr_t dma_addr)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return;
    }

    if (dev_ops->ops.devdrv_dma_free_coherent != NULL) {
        dev_ops->ops.devdrv_dma_free_coherent(dev, size, addr, dma_addr);
    }
    devdrv_sub_ops_ref(dev_ops);

    return;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_free_coherent);

ka_dma_addr_t hal_kernel_devdrv_dma_map_single(ka_device_t *dev, void *ptr, size_t size, ka_dma_data_direction_t dir)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    ka_dma_addr_t dma_addr = (~(ka_dma_addr_t)0);

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return dma_addr;
    }

    if (dev_ops->ops.devdrv_dma_map_single != NULL) {
        dma_addr = dev_ops->ops.devdrv_dma_map_single(dev, ptr, size, dir);
    }
    devdrv_sub_ops_ref(dev_ops);

    return dma_addr;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_map_single);

void hal_kernel_devdrv_dma_unmap_single(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return;
    }

    if (dev_ops->ops.devdrv_dma_unmap_single != NULL) {
        dev_ops->ops.devdrv_dma_unmap_single(dev, addr, size, dir);
    }
    devdrv_sub_ops_ref(dev_ops);

    return;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_unmap_single);

ka_dma_addr_t hal_kernel_devdrv_dma_map_page_attrs(ka_device_t *dev, ka_page_t *page, size_t offset, size_t size,
                                                   ka_dma_data_direction_t dir, u64 attrs)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    ka_dma_addr_t dma_addr = (~(ka_dma_addr_t)0);

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return dma_addr;
    }

    if (dev_ops->ops.devdrv_dma_map_page_attrs != NULL) {
        dma_addr = dev_ops->ops.devdrv_dma_map_page_attrs(dev, page, offset, size, dir, attrs);
    }
    devdrv_sub_ops_ref(dev_ops);

    return dma_addr;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_map_page_attrs);

void hal_kernel_devdrv_dma_unmap_page_attrs(ka_device_t *dev, ka_dma_addr_t addr, size_t size,
                                            ka_dma_data_direction_t dir, u64 attrs)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return;
    }

    if (dev_ops->ops.devdrv_dma_unmap_page_attrs != NULL) {
        dev_ops->ops.devdrv_dma_unmap_page_attrs(dev, addr, size, dir, attrs);
    }
    devdrv_sub_ops_ref(dev_ops);

    return;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_unmap_page_attrs);

ka_dma_addr_t hal_kernel_devdrv_dma_map_page(ka_device_t *dev, ka_page_t *page, size_t offset, size_t size,
                                             ka_dma_data_direction_t dir)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    ka_dma_addr_t dma_addr = (~(ka_dma_addr_t)0);

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return dma_addr;
    }

    if (dev_ops->ops.devdrv_dma_map_page != NULL) {
        dma_addr = dev_ops->ops.devdrv_dma_map_page(dev, page, offset, size, dir);
    }
    devdrv_sub_ops_ref(dev_ops);

    return dma_addr;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_map_page);

void hal_kernel_devdrv_dma_unmap_page(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return;
    }

    if (dev_ops->ops.devdrv_dma_unmap_page != NULL) {
        dev_ops->ops.devdrv_dma_unmap_page(dev, addr, size, dir);
    }
    devdrv_sub_ops_ref(dev_ops);

    return;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_unmap_page);

ka_dma_addr_t devdrv_dma_map_resource(ka_device_t *dev, phys_addr_t phys_addr, size_t size, ka_dma_data_direction_t dir,
                                      unsigned long attrs)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    ka_dma_addr_t dma_addr = (~(ka_dma_addr_t)0);

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return dma_addr;
    }

    if (dev_ops->ops.devdrv_dma_map_resource != NULL) {
        dma_addr = dev_ops->ops.devdrv_dma_map_resource(dev, phys_addr, size, dir, attrs);
    }
    devdrv_sub_ops_ref(dev_ops);

    return dma_addr;
}
KA_EXPORT_SYMBOL(devdrv_dma_map_resource);

void devdrv_dma_unmap_resource(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir,
                               unsigned long attrs)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return;
    }

    if (dev_ops->ops.devdrv_dma_unmap_resource != NULL) {
        dev_ops->ops.devdrv_dma_unmap_resource(dev, addr, size, dir, attrs);
    }
    devdrv_sub_ops_ref(dev_ops);

    return;
}
KA_EXPORT_SYMBOL(devdrv_dma_unmap_resource);
#endif

int hal_kernel_devdrv_dma_sync_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst,
                                         u32 size, enum devdrv_dma_direction direction)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_sync_copy_plus != NULL) {
        ret = dev_ops->ops.dma_sync_copy_plus(udevid, type, instance, src, dst, size, direction);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_sync_copy_plus);

int hal_kernel_devdrv_dma_sync_copy(u32 udevid, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                                    enum devdrv_dma_direction direction)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_sync_copy != NULL) {
        ret = dev_ops->ops.dma_sync_copy(udevid, type, src, dst, size, direction);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_sync_copy);

int hal_kernel_devdrv_dma_async_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst,
                                          u32 size, enum devdrv_dma_direction direction,
                                          struct devdrv_asyn_dma_para_info *para_info)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_async_copy_plus != NULL) {
        ret = dev_ops->ops.dma_async_copy_plus(udevid, type, instance, src, dst, size, direction, para_info);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_async_copy_plus);

int hal_kernel_devdrv_dma_async_copy(u32 udevid, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                                     enum devdrv_dma_direction direction, struct devdrv_asyn_dma_para_info *para_info)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_async_copy != NULL) {
        ret = dev_ops->ops.dma_async_copy(udevid, type, src, dst, size, direction, para_info);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_async_copy);

int hal_kernel_devdrv_dma_sync_link_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int wait_type, int instance,
                                              struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_sync_link_copy_plus != NULL) {
        ret = dev_ops->ops.dma_sync_link_copy_plus(udevid, type, wait_type, instance, dma_node, node_cnt);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_sync_link_copy_plus);

int hal_kernel_devdrv_dma_sync_link_copy(u32 udevid, enum devdrv_dma_data_type type, int wait_type,
                                         struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_sync_link_copy != NULL) {
        ret = dev_ops->ops.dma_sync_link_copy(udevid, type, wait_type, dma_node, node_cnt);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_sync_link_copy);

int hal_kernel_devdrv_dma_async_link_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int instance,
                                               struct devdrv_dma_node *dma_node, u32 node_cnt,
                                               struct devdrv_asyn_dma_para_info *para_info)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_async_link_copy_plus != NULL) {
        ret = dev_ops->ops.dma_async_link_copy_plus(udevid, type, instance, dma_node, node_cnt, para_info);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_async_link_copy_plus);

int hal_kernel_devdrv_dma_async_link_copy(u32 udevid, enum devdrv_dma_data_type type, struct devdrv_dma_node *dma_node,
                                          u32 node_cnt, struct devdrv_asyn_dma_para_info *para_info)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_async_link_copy != NULL) {
        ret = dev_ops->ops.dma_async_link_copy(udevid, type, dma_node, node_cnt, para_info);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_async_link_copy);

int hal_kernel_devdrv_dma_sync_link_copy_plus_extend(u32 udevid, enum devdrv_dma_data_type type, int wait_type,
                                                     int instance, struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_sync_link_copy_plus_extend != NULL) {
        ret = dev_ops->ops.dma_sync_link_copy_plus_extend(udevid, type, wait_type, instance, dma_node, node_cnt);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_sync_link_copy_plus_extend);

int hal_kernel_devdrv_dma_sync_link_copy_extend(u32 udevid, enum devdrv_dma_data_type type, int wait_type,
                                                struct devdrv_dma_node *dma_node, u32 node_cnt)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_sync_link_copy_extend != NULL) {
        ret = dev_ops->ops.dma_sync_link_copy_extend(udevid, type, wait_type, dma_node, node_cnt);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_sync_link_copy_extend);

int devdrv_dma_done_schedule(u32 udevid, enum devdrv_dma_data_type type, int instance)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_done_schedule != NULL) {
        ret = dev_ops->ops.dma_done_schedule(udevid, type, instance);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(devdrv_dma_done_schedule);

int devdrv_dma_get_sq_cq_desc_size(u32 devid, u32 *sq_desc_size, u32 *cq_desc_size)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_get_sq_cq_desc_size != NULL) {
        ret = dev_ops->ops.dma_get_sq_cq_desc_size(devid, sq_desc_size, cq_desc_size);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(devdrv_dma_get_sq_cq_desc_size);

int devdrv_dma_fill_desc_of_sq(u32 udevid, struct devdrv_dma_prepare *dma_prepare, struct devdrv_dma_node *dma_node,
                               u32 node_cnt, u32 fill_status)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_fill_desc_of_sq != NULL) {
        ret = dev_ops->ops.dma_fill_desc_of_sq(udevid, dma_prepare, dma_node, node_cnt, fill_status);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(devdrv_dma_fill_desc_of_sq);

int devdrv_dma_fill_desc_of_sq_ext(u32 udevid, void *sq_base, struct devdrv_dma_node *dma_node, u32 node_cnt,
                                   u32 fill_status)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_fill_desc_of_sq_ext != NULL) {
        ret = dev_ops->ops.dma_fill_desc_of_sq_ext(udevid, sq_base, dma_node, node_cnt, fill_status);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(devdrv_dma_fill_desc_of_sq_ext);

struct devdrv_dma_prepare *devdrv_dma_link_prepare(u32 udevid, enum devdrv_dma_data_type type,
                                                   struct devdrv_dma_node *dma_node, u32 node_cnt, u32 fill_status)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    struct devdrv_dma_prepare *dma_prepare = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return NULL;
    }

    if (dev_ops->ops.dma_link_prepare != NULL) {
        dma_prepare = dev_ops->ops.dma_link_prepare(udevid, type, dma_node, node_cnt, fill_status);
    }
    devdrv_sub_ops_ref(dev_ops);

    return dma_prepare;
}
KA_EXPORT_SYMBOL(devdrv_dma_link_prepare);

int devdrv_dma_link_free(struct devdrv_dma_prepare *dma_prepare)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_link_free != NULL) {
        ret = dev_ops->ops.dma_link_free(dma_prepare);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(devdrv_dma_link_free);

int devdrv_dma_sqcq_desc_check(u32 devid, struct devdrv_dma_desc_info *dma_desc_info)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_sqcq_desc_check != NULL) {
        ret = dev_ops->ops.dma_sqcq_desc_check(devid, dma_desc_info);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(devdrv_dma_sqcq_desc_check);

int devdrv_dma_prepare_alloc_sq_addr(u32 udevid, u32 node_cnt, struct devdrv_dma_prepare *dma_prepare)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.dma_prepare_alloc_sq_addr != NULL) {
        ret = dev_ops->ops.dma_prepare_alloc_sq_addr(udevid, node_cnt, dma_prepare);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(devdrv_dma_prepare_alloc_sq_addr);

void devdrv_dma_prepare_free_sq_addr(u32 udevid, struct devdrv_dma_prepare *dma_prepare)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return;
    }

    if (dev_ops->ops.dma_prepare_free_sq_addr != NULL) {
        dev_ops->ops.dma_prepare_free_sq_addr(udevid, dma_prepare);
    }
    devdrv_sub_ops_ref(dev_ops);

    return;
}
KA_EXPORT_SYMBOL(devdrv_dma_prepare_free_sq_addr);

int hal_kernel_devdrv_dma_map_sg_cache(ka_scatterlist_t *sg, int nents, dma_addr_t *dma_handle,
                                       enum dma_data_direction dir)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.devdrv_dma_map_sg_cache != NULL) {
        ret = dev_ops->ops.devdrv_dma_map_sg_cache(sg, nents, dma_handle, dir);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_map_sg_cache);

void hal_kernel_devdrv_dma_unmap_sg_cache(ka_scatterlist_t *sg, int nents, dma_addr_t dma_handle,
                                          enum dma_data_direction dir)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return;
    }

    if (dev_ops->ops.devdrv_dma_unmap_sg_cache != NULL) {
        dev_ops->ops.devdrv_dma_unmap_sg_cache(sg, nents, dma_handle, dir);
    }
    devdrv_sub_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_unmap_sg_cache);

int hal_kernel_devdrv_dma_map_sg_no_cache(ka_scatterlist_t *sg, int nents, dma_addr_t *dma_handle,
                                          enum dma_data_direction dir)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.devdrv_dma_map_sg_no_cache != NULL) {
        ret = dev_ops->ops.devdrv_dma_map_sg_no_cache(sg, nents, dma_handle, dir);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_map_sg_no_cache);

void hal_kernel_devdrv_dma_unmap_sg_no_cache(ka_scatterlist_t *sg, int nents, dma_addr_t dma_handle,
                                             enum dma_data_direction dir)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return;
    }

    if (dev_ops->ops.devdrv_dma_unmap_sg_no_cache != NULL) {
        dev_ops->ops.devdrv_dma_unmap_sg_no_cache(sg, nents, dma_handle, dir);
    }
    devdrv_sub_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(hal_kernel_devdrv_dma_unmap_sg_no_cache);

int devdrv_get_dma_copy_support_mode(u32 udevid, u32 *support_mode)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;
    int ret = -EOPNOTSUPP;

    dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.get_dma_copy_support_mode != NULL) {
        ret = dev_ops->ops.get_dma_copy_support_mode(udevid, support_mode);
    }
    devdrv_sub_ops_ref(dev_ops);

    return ret;
}
KA_EXPORT_SYMBOL(devdrv_get_dma_copy_support_mode);
