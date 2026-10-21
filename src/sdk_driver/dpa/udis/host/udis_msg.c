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

#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_base_pub.h"
#include "ka_list_pub.h"
#include "ka_errno_pub.h"
#include "securec.h"
#include "pbl_mem_alloc_interface.h"
#include "udis_log.h"
#include "udis_management.h"
#include "udis_msg.h"
#include "comm_kernel_interface.h"

#define UDIS_NODE_NUM_INIT 128
#define UDIS_LINK_NODES_SCALE_UP_FACTOR 2

STATIC struct devdrv_common_msg_client g_udis_common_chan;
STATIC struct udis_link_nodes g_link_nodes[UDIS_DEVICE_UDEVID_MAX] = {0};

struct devdrv_common_msg_client *udis_get_common_msg_client(void)
{
    return &g_udis_common_chan;
}

struct udis_link_nodes *udis_get_link_nodes(unsigned int udevid)
{
    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalid param. (udevid=%u)\n", udevid);
        return NULL;
    }
    return &g_link_nodes[udevid];
}

int udis_link_nodes_init(unsigned int udevid)
{
    struct udis_link_nodes *link_nodes = NULL;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalid param. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    link_nodes = udis_get_link_nodes(udevid);
    if (link_nodes == NULL) {
        udis_err("link_nodes is NULL. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    if ((devdrv_get_connect_protocol(udevid) == CONNECT_PROTOCOL_UB)) {
        if (link_nodes->node.ub_nodes != NULL) {
            udis_info("ub_nodes is not NULL, no need to init. (udevid=%u)\n", udevid);
            return 0;
        }

        link_nodes->node.ub_nodes = (struct udis_link_ub_node *)dbl_kzalloc(
            (sizeof(struct udis_link_ub_node) * UDIS_NODE_NUM_INIT), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (link_nodes->node.ub_nodes == NULL) {
            udis_err("Failed to alloc ub node array. (udevid=%u)\n", udevid);
            return -ENOMEM;
        }
    } else {
        if (link_nodes->node.dma_nodes != NULL) {
            udis_info("dma_nodes is not NULL, no need to init. (udevid=%u)\n", udevid);
            return 0;
        }

        link_nodes->node.dma_nodes = (struct devdrv_dma_node *)dbl_kzalloc(
            (sizeof(struct devdrv_dma_node) * UDIS_NODE_NUM_INIT), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (link_nodes->node.dma_nodes == NULL) {
            udis_err("Failed to alloc dma node array. (udevid=%u)\n", udevid);
            return -ENOMEM;
        }
    }

    link_nodes->capacity = UDIS_NODE_NUM_INIT;
    link_nodes->node_num = 0;
    return 0;
}

void udis_link_nodes_uninit(unsigned int udevid)
{
    struct udis_link_nodes *link_nodes = NULL;

    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalid param. (udevid=%u)\n", udevid);
        return;
    }

    link_nodes = udis_get_link_nodes(udevid);
    if (link_nodes == NULL) {
        udis_err("ub_nodes_arr is NULL. (udevid=%u)\n", udevid);
        return;
    }

    if ((devdrv_get_connect_protocol(udevid) == CONNECT_PROTOCOL_UB)) {
        if (link_nodes->node.ub_nodes == NULL) {
            udis_info("ub_nodes is NULL, no need to uninit. (udevid=%u)\n", udevid);
            return;
        }
        dbl_kfree(link_nodes->node.ub_nodes);
        link_nodes->node.ub_nodes = NULL;
    } else {
        if (link_nodes->node.dma_nodes == NULL) {
            udis_info("dma_nodes is NULL, no need to uninit. (udevid=%u)\n", udevid);
            return;
        }
        dbl_kfree(link_nodes->node.dma_nodes);
        link_nodes->node.dma_nodes = NULL;
    }

    link_nodes->capacity = 0;
    link_nodes->node_num = 0;
    return;
}

STATIC int udis_ub_nodes_scale_up(unsigned int udevid, struct udis_link_nodes *link_nodes)
{
    int ret;
    struct udis_link_ub_node *old_nodes = NULL;
    struct udis_link_ub_node *new_nodes = NULL;
    unsigned int new_capacity = 0;
    unsigned int old_capacity = 0;

    if (link_nodes->node.ub_nodes == NULL) {
        udis_err("ub_nodes is NULL. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    old_capacity = link_nodes->capacity;
    new_capacity = old_capacity * UDIS_LINK_NODES_SCALE_UP_FACTOR;
    old_nodes = link_nodes->node.ub_nodes;

    new_nodes = (struct udis_link_ub_node *)dbl_kzalloc(sizeof(struct udis_link_ub_node) * new_capacity,
                                                        KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (new_nodes == NULL) {
        udis_err("Failed to alloc new ub node array. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }

    ret = memcpy_s(new_nodes, sizeof(struct udis_link_ub_node) * old_capacity, old_nodes,
                   sizeof(struct udis_link_ub_node) * old_capacity);
    if (ret != 0) {
        dbl_kfree(new_nodes);
        new_nodes = NULL;
        udis_err("memcpy_s failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return -ENOMEM;
    }
    dbl_kfree(old_nodes);
    old_nodes = NULL;
    link_nodes->node.ub_nodes = new_nodes;
    link_nodes->capacity = new_capacity;
    return 0;
}

STATIC int udis_dma_nodes_scale_up(unsigned int udevid, struct udis_link_nodes *link_nodes)
{
    int ret;
    struct devdrv_dma_node *old_nodes = NULL;
    struct devdrv_dma_node *new_nodes = NULL;
    unsigned int new_capacity = 0;
    unsigned int old_capacity = 0;

    if (link_nodes->node.dma_nodes == NULL) {
        udis_err("dma_nodes is NULL. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    old_capacity = link_nodes->capacity;
    new_capacity = old_capacity * UDIS_LINK_NODES_SCALE_UP_FACTOR;
    old_nodes = link_nodes->node.dma_nodes;

    new_nodes = (struct devdrv_dma_node *)dbl_kzalloc(sizeof(struct devdrv_dma_node) * new_capacity,
                                                      KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (new_nodes == NULL) {
        udis_err("Failed to alloc new dma node array. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }

    ret = memcpy_s(new_nodes, sizeof(struct devdrv_dma_node) * old_capacity, old_nodes,
                   sizeof(struct devdrv_dma_node) * old_capacity);
    if (ret != 0) {
        dbl_kfree(new_nodes);
        new_nodes = NULL;
        udis_err("memcpy_s failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return -ENOMEM;
    }
    dbl_kfree(old_nodes);
    old_nodes = NULL;
    link_nodes->node.dma_nodes = new_nodes;
    link_nodes->capacity = new_capacity;
    return 0;
}

int udis_link_nodes_scale_up(unsigned int udevid)
{
    int ret;
    struct udis_link_nodes *link_nodes = NULL;
    if (udevid >= UDIS_DEVICE_UDEVID_MAX) {
        udis_err("Invalid param. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    link_nodes = udis_get_link_nodes(udevid);
    if (link_nodes == NULL) {
        udis_err("link_ub_nodes is NULL. (udevid=%u)\n", udevid);
        return -EFAULT;
    }

    if ((devdrv_get_connect_protocol(udevid) == CONNECT_PROTOCOL_UB)) {
        ret = udis_ub_nodes_scale_up(udevid, link_nodes);
        if (ret != 0) {
            udis_err("udis_ub_nodes_scale_up failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
    } else {
        ret = udis_dma_nodes_scale_up(udevid, link_nodes);
        if (ret != 0) {
            udis_err("udis_dma_nodes_scale_up failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
    }

    return 0;
}

STATIC int udis_update_ub_nodes(unsigned int udevid, const struct udis_ctrl_block *udis_cb,
                                UDIS_UPDATE_TYPE update_type, struct udis_link_nodes *link_nodes)
{
    int ret = 0;
    struct udis_node *cur;
    unsigned int node_num = 0;
    ka_list_for_each_entry(cur, &udis_cb->addr_list[update_type], list)
    {
        link_nodes->node.ub_nodes[node_num].host_segment = cur->host_segment;
        link_nodes->node.ub_nodes[node_num].device_segment_import = cur->device_segment_import;
        link_nodes->node.ub_nodes[node_num].host_segment_len = cur->host_segment_len;
        link_nodes->node.ub_nodes[node_num].device_segment_import_len = cur->device_segment_import_len;
        link_nodes->node.ub_nodes[node_num].host_va_addr = cur->host_va_addr;
        link_nodes->node.ub_nodes[node_num].data_len = cur->data_len;
        link_nodes->node.ub_nodes[node_num].device_addr_offset = cur->device_addr_offset;
        ++node_num;
        if (node_num < link_nodes->capacity) {
            continue;
        }
        ret = udis_link_nodes_scale_up(udevid);
        if (ret != 0) {
            udis_err("Link ub nodes scale up failed. (udevid=%u; ret=%d)\n", udevid, ret);
            goto out;
        }
    }

out:
    link_nodes->node_num = node_num;
    return ret;
}

STATIC int udis_update_dma_nodes(unsigned int udevid, const struct udis_ctrl_block *udis_cb,
                                 UDIS_UPDATE_TYPE update_type, struct udis_link_nodes *link_nodes)
{
    int ret = 0;
    struct udis_node *cur;
    unsigned int node_num = 0;
    ka_list_for_each_entry(cur, &udis_cb->addr_list[update_type], list)
    {
        link_nodes->node.dma_nodes[node_num].src_addr = cur->dev_dma_addr;
        link_nodes->node.dma_nodes[node_num].dst_addr = cur->host_dma_addr;
        link_nodes->node.dma_nodes[node_num].size = cur->data_len;
        link_nodes->node.dma_nodes[node_num].direction = DEVDRV_DMA_DEVICE_TO_HOST;
        link_nodes->node.dma_nodes[node_num].loc_passid = DEVDRV_DMA_PASSID_DEFAULT;
        ++node_num;
        if (node_num < link_nodes->capacity) {
            continue;
        }
        ret = udis_link_nodes_scale_up(udevid);
        if (ret != 0) {
            udis_err("Link dma nodes scale up failed. (udevid=%u; ret=%d)\n", udevid, ret);
            goto out;
        }
    }

out:
    link_nodes->node_num = node_num;
    return ret;
}

STATIC int udis_update_link_nodes(unsigned int udevid, const struct udis_ctrl_block *udis_cb,
                                  UDIS_UPDATE_TYPE update_type)
{
    int ret = 0;
    unsigned int node_num = 0;
    struct udis_link_nodes *link_nodes = NULL;

    if (update_type < UPDATE_PERIOD_LEVEL_1) {
        udis_info("Not period update type, no need to flush link ub nodes. (udevid=%u; update_type=%u; node_num=%u)\n",
                  udevid, update_type, node_num);
        return 0;
    }

    link_nodes = udis_get_link_nodes(udevid);
    if (link_nodes == NULL) {
        udis_err("nodes_arr is NULL. (udevid=%u; update_type=%u)\n", udevid, update_type);
        return -ENOMEM;
    }

    if ((devdrv_get_connect_protocol(udevid) == CONNECT_PROTOCOL_UB)) {
        ret = udis_update_ub_nodes(udevid, udis_cb, update_type, link_nodes);
        if (ret != 0) {
            udis_err("udis_update_ub_nodes failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
    } else {
        ret = udis_update_dma_nodes(udevid, udis_cb, update_type, link_nodes);
        if (ret != 0) {
            udis_err("udis_update_dma_nodes failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
    }

    return ret;
}

STATIC int udis_check_addr_node(const struct udis_node *addr_node)
{
    int is_discrete_info;
    unsigned int name_len = 0;

    if (addr_node == NULL) {
        udis_err("Addr node is NULL\n");
        return -EINVAL;
    }

    if (addr_node->module_type >= UDIS_MODULE_MAX) {
        udis_err("Invalid module_type. (module_type=%u; max_module_type=%u)\n", addr_node->module_type,
                 UDIS_MODULE_MAX - 1);
        return -EINVAL;
    }

    name_len = ka_base_strnlen(addr_node->name, UDIS_MAX_NAME_LEN);
    if ((name_len == 0) || (name_len >= UDIS_MAX_NAME_LEN)) {
        udis_err("Invalid name_len. (module_type=%u; name_len=%u; max_name_len=%u)\n", addr_node->module_type, name_len,
                 UDIS_MAX_NAME_LEN - 1);
        return -EINVAL;
    }

    if (addr_node->update_type >= UPDATE_TYPE_MAX) {
        udis_err("Invalid update type. (mode_type=%u; name=%s; update_type=%u; max_update_type=%u)\n",
                 addr_node->module_type, addr_node->name, addr_node->update_type, UPDATE_TYPE_MAX - 1);
        return -EINVAL;
    }

    is_discrete_info = ka_base_strcmp(addr_node->name, UDIS_UNIFIED_MODULE_INFO);
    if (((addr_node->dev_dma_addr == UDIS_BAD_DMA_ADDR) && (addr_node->dev_va_addr == UDIS_BAD_VA_ADDR)) ||
        (addr_node->data_len == 0) || ((is_discrete_info != 0) && (addr_node->data_len > UDIS_MAX_DATA_LEN)) ||
        ((is_discrete_info == 0) && (addr_node->data_len != UDIS_UNIFIED_MODULE_OFFSET))) {
        udis_err("Invalid param. (module_type=%u; name=%s; dma_addr_is_bad=%d; data_len=%u)\n", addr_node->module_type,
                 addr_node->name, addr_node->dev_dma_addr == UDIS_BAD_DMA_ADDR, addr_node->data_len);
        return -EINVAL;
    }

    return 0;
}

STATIC struct devdrv_seg_info *udis_alloc_seg_info(unsigned int udevid, va_addr_t addr, unsigned int data_len,
                                                   u32 token_value)
{
    struct devdrv_seg_info *seg_info = NULL;
    seg_info = (struct devdrv_seg_info *)dbl_kzalloc(sizeof(struct devdrv_seg_info), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (seg_info == NULL) {
        udis_err("kzalloc reg_seg failed. (dev_id=%u)\n", udevid);
        return NULL;
    }
    seg_info->va = addr;
    seg_info->token_value = token_value;
    seg_info->access = DEVDRV_ACCESS_READ | DEVDRV_ACCESS_WRITE;
    seg_info->mem_len = data_len;
    return seg_info;
}

STATIC void udis_free_seg_info(struct devdrv_seg_info *seg_info)
{
    if (seg_info != NULL) {
        dbl_kfree(seg_info);
        seg_info = NULL;
    }
    return;
}

STATIC int udis_get_segment(unsigned int udevid, struct udis_ctrl_block *udis_cb, struct udis_node *addr_node)
{
    int ret;
    void *host_segment = NULL;
    void *device_segment_import = NULL;
    size_t device_segment_import_len = 0;
    size_t host_segment_len = 0;
    u32 token_value = 0;
    struct devdrv_seg_info *seg_info = NULL;

    device_segment_import = devdrv_import_seg(udevid, addr_node->token_value, (void *)addr_node->device_segment,
                                              addr_node->device_segment_len, &device_segment_import_len);
    if (KA_IS_ERR_OR_NULL(device_segment_import)) {
        udis_err("devdrv_import_seg failed. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = devdrv_get_token_val(udevid, &token_value);
    if (ret != 0) {
        udis_err("devdrv_get_token_val failed. (udevid=%u)\n", udevid);
        return ret;
    }
    seg_info = udis_alloc_seg_info(udevid, (va_addr_t)(uintptr_t)(udis_cb->udis_info_buf),
                                   UDIS_MODULE_OFFSET * UDIS_MODULE_MAX, token_value);
    if (seg_info == NULL) {
        udis_err("udis_alloc_seg_info failed. (udevid=%u)\n", udevid);
        return ret;
    }

    ret = devdrv_register_seg(udevid, seg_info, &host_segment, &host_segment_len);
    udis_free_seg_info(seg_info);
    if (ret != 0) {
        udis_err("devdrv_register_seg failed. (udevid=%u)\n", udevid);
        return ret;
    }

    addr_node->host_segment = host_segment;
    addr_node->device_segment_import = device_segment_import;
    addr_node->host_segment_len = host_segment_len;
    addr_node->device_segment_import_len = device_segment_import_len;
    return 0;
}

STATIC int udis_addr_node_register(unsigned int udevid, struct udis_ctrl_block *udis_cb, struct udis_node *addr_node)
{
    int ret;
    ka_dma_addr_t host_addr = 0;

    ret = udis_alloc_info_block(udevid, addr_node, &host_addr);
    if (ret != 0) {
        udis_err("Init info block failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
                 addr_node->module_type, addr_node->name, ret);
        return ret;
    }
    if ((devdrv_get_connect_protocol(udevid) == CONNECT_PROTOCOL_UB)) {
        addr_node->host_va_addr = host_addr;
    } else {
        addr_node->host_dma_addr = host_addr;
    }

    if ((devdrv_get_connect_protocol(udevid)) == CONNECT_PROTOCOL_UB) {
        ret = udis_get_segment(udevid, udis_cb, addr_node);
        if (ret != 0) {
            udis_err("udis_get_segment failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
                     addr_node->module_type, addr_node->name, ret);
            goto free_info_block;
        }
    }

    ret = udis_addr_list_add_node(udevid, udis_cb, addr_node);
    if (ret != 0) {
        udis_err("Add node to addr list failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
                 addr_node->module_type, addr_node->name, ret);
        goto free_info_block;
    }
    ret = udis_update_link_nodes(udevid, udis_cb, addr_node->update_type);
    if (ret != 0) {
        udis_err("Update link nodes failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
                 addr_node->module_type, addr_node->name, ret);
        goto remove_node;
    }
    return 0;

remove_node:
    udis_addr_list_remove_node(udevid, udis_cb, addr_node);
free_info_block:
    udis_free_info_block(udevid, addr_node);
    return ret;
}

STATIC int udis_register_addr(void *msg, u32 *ack_len)
{
    int ret;
    unsigned int udevid;
    struct udis_msg_info *msg_info = NULL;
    struct udis_node *addr_node = NULL;
    struct udis_ctrl_block *udis_cb = NULL;
    struct udis_node *repeat_node = NULL;

    msg_info = (struct udis_msg_info *)msg;
    addr_node = (struct udis_node *)msg_info->payload;
    udevid = msg_info->head.dev_id;

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX) || (udis_check_addr_node(addr_node) != 0)) {
        udis_err("Invalid udevid or udis addr node. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    udis_cb_read_lock(udevid);
    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        ret = -ENODEV;
        goto unlock_cb_read_lock;
    }

    ka_task_down_write(&udis_cb->addr_list_lock);
    repeat_node = udis_addr_list_find_node(udis_cb, addr_node->module_type, addr_node->name);
    if (repeat_node != NULL) {
        udis_info("Already have the same addr node. (udevid=%u; module_type=%u; name=%s)\n", udevid,
                  addr_node->module_type, addr_node->name);
        *ack_len = 0;
        ret = 0;
        goto unlock_addr_list_lock;
    }

    ret = udis_addr_node_register(udevid, udis_cb, addr_node);
    if (ret != 0) {
        udis_err("Add new addr node failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
                 addr_node->module_type, addr_node->name, ret);
        goto unlock_addr_list_lock;
    }

    (void)udis_sync_copy(udevid, udis_cb, addr_node);

    *ack_len = 0;
    udis_info("Register addr node success.\
 (udevid=%u; module_type=%u; name=%s; update_type=%u; acc_ctrl=%u; data_len=%u)\n",
              udevid, addr_node->module_type, addr_node->name, addr_node->update_type, addr_node->acc_ctrl,
              addr_node->data_len);

unlock_addr_list_lock:
    ka_task_up_write(&udis_cb->addr_list_lock);
unlock_cb_read_lock:
    udis_cb_read_unlock(udevid);
    return ret;
}

STATIC int udis_unregister_addr(void *msg, u32 *ack_len)
{
    int ret;
    unsigned int udevid;
    struct udis_msg_info *msg_info = NULL;
    struct udis_node *addr_node = NULL;
    struct udis_node *target_node = NULL;
    struct udis_ctrl_block *udis_cb = NULL;

    msg_info = (struct udis_msg_info *)msg;
    udevid = msg_info->head.dev_id;
    addr_node = (struct udis_node *)msg_info->payload;

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX) || (udis_check_addr_node(addr_node) != 0)) {
        udis_err("Invalid udevid or udis addr node. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    udis_cb_read_lock(udevid);
    udis_cb = udis_get_ctrl_block(udevid);
    if (udis_cb == NULL) {
        udis_cb_read_unlock(udevid);
        udis_err("Get udis ctrl block failed. (udevid=%u)\n", udevid);
        return -ENODEV;
    }

    ka_task_down_write(&udis_cb->addr_list_lock);

    target_node = udis_addr_list_find_node(udis_cb, addr_node->module_type, addr_node->name);
    if (target_node == NULL) {
        udis_info("No matched node found. (udevid=%u; module_type=%u; name=%s)\n", udevid, addr_node->module_type,
                  addr_node->name);
        *ack_len = 0;
        ret = 0;
        goto out;
    }

    ka_list_del(&target_node->list);

    ret = udis_update_link_nodes(udevid, udis_cb, addr_node->update_type);
    if (ret != 0) {
        ka_list_add(&target_node->list, &udis_cb->addr_list[target_node->update_type]);
        udis_err("Update link nodes failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
                 addr_node->module_type, addr_node->name, ret);
        goto out;
    }
    udis_free_info_block(udevid, target_node);
    dbl_kfree(target_node);
    target_node = NULL;

    *ack_len = 0;
    udis_info("Unregister addr node success.\
 (udevid=%u; module_type=%u; name=%s; update_type=%u; acc_ctrl=%u; data_len=%u)\n",
              udevid, addr_node->module_type, addr_node->name, addr_node->update_type, addr_node->acc_ctrl,
              addr_node->data_len);
out:
    ka_task_up_write(&udis_cb->addr_list_lock);
    udis_cb_read_unlock(udevid);
    return ret;
}

STATIC int (*udis_chan_msg_processes[UDIS_CHAN_D2H_MAX_ID])(void *msg, u32 *ack_len) = {
    [UDIS_CHAN_D2H_REGIST] = udis_register_addr,
    [UDIS_CHAN_D2H_UNREGIST] = udis_unregister_addr,
};

STATIC int udis_rx_common_msg_process(unsigned int dev_id, void *data, unsigned int in_data_len,
                                      unsigned int out_data_len, unsigned int *real_out_len)
{
    unsigned int msg_id;
    struct udis_msg_info *msg_info = NULL;

    if ((dev_id >= UDIS_DEVICE_UDEVID_MAX) || (data == NULL) || (real_out_len == NULL) ||
        (in_data_len < sizeof(struct udis_msg_info))) {
        udis_err("data(%pK) or real_out_len(%pK) is NULL. (devid=%u; in_data_len=%u)\n", data, real_out_len, dev_id,
                 in_data_len);
        return -EINVAL;
    }
    msg_info = (struct udis_msg_info *)data;
    msg_id = msg_info->head.msg_id;
    if (msg_id >= UDIS_CHAN_D2H_MAX_ID) {
        udis_err("Invalid msg_id. (dev_id=%u; msg_id=%u)\n", dev_id, msg_id);
        return -EINVAL;
    }

    if (udis_chan_msg_processes[msg_id] == NULL) {
        udis_err("udis_chan_msg_processes[%u] is NULL. (dev_id=%u)\n", msg_id, dev_id);
        return -EINVAL;
    }
    msg_info->head.dev_id = dev_id;
    return udis_chan_msg_processes[msg_id](data, real_out_len);
}

int udis_send_host_vf_uninit_notify(unsigned int udevid)
{
    int ret;
    unsigned int out_len = 0;
    struct udis_msg_info udis_msg_info = {{0}, {0}};

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX)) {
        udis_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    udis_msg_info.head.dev_id = udevid;
    udis_msg_info.head.msg_id = UDIS_CHAN_H2D_UNINIT;
    udis_msg_info.head.valid = (u16)UDIS_MSG_VALID;
    udis_msg_info.head.result = (u16)UDIS_MSG_INVALID_RESULT;
    udis_msg_info.payload[0] = 1;

    ret = devdrv_common_msg_send(udevid, &udis_msg_info, sizeof(udis_msg_info), sizeof(udis_msg_info), &out_len,
                                 DEVDRV_COMMON_MSG_UDIS);
    if (ret != 0) {
        udis_err("H2D Send host uninit msg to device failed. (devid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    return 0;
}

int udis_send_host_ready_msg_to_device(unsigned int udevid)
{
    int ret;
    unsigned int out_len = 0;
    struct udis_msg_info udis_msg_info = {{0}, {0}};

    if ((udevid >= UDIS_DEVICE_UDEVID_MAX)) {
        udis_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    udis_msg_info.head.dev_id = udevid;
    udis_msg_info.head.msg_id = UDIS_CHAN_H2D_READY;
    udis_msg_info.head.valid = (u16)UDIS_MSG_VALID;
    udis_msg_info.head.result = (u16)UDIS_MSG_INVALID_RESULT;
    udis_msg_info.payload[0] = 1;

    ret = devdrv_common_msg_send(udevid, &udis_msg_info, sizeof(udis_msg_info), sizeof(udis_msg_info), &out_len,
                                 DEVDRV_COMMON_MSG_UDIS);
    if (ret != 0) {
        udis_err("H2D Send host ready msg to device failed. (devid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    return 0;
}

int udis_common_chan_init(void)
{
    g_udis_common_chan.type = DEVDRV_COMMON_MSG_UDIS;
    g_udis_common_chan.common_msg_recv = udis_rx_common_msg_process;
    g_udis_common_chan.init_notify = NULL;
    return 0;
}

void udis_common_chan_uninit(void)
{
    g_udis_common_chan.type = DEVDRV_COMMON_MSG_TYPE_MAX;
    g_udis_common_chan.common_msg_recv = NULL;
    g_udis_common_chan.init_notify = NULL;
}