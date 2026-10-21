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

#include "securec.h"
#include "ka_base_pub.h"
#include "ka_list_pub.h"
#include "pbl_mem_alloc_interface.h"
#include "udis_log.h"
#include "udis_management.h"

int udis_addr_list_add_node(unsigned int udevid, struct udis_ctrl_block *udis_cb, const struct udis_node *addr_node)
{
    int ret;
    struct udis_node *new_node = NULL;

    new_node = dbl_kzalloc(sizeof(struct udis_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (new_node == NULL) {
        udis_err("Failed to malloc for udis_node. (udevid=%u; module_type=%u; name=%s)\n", udevid,
                 addr_node->module_type, addr_node->name);
        return -ENOMEM;
    }

    ret = strcpy_s(new_node->name, UDIS_MAX_NAME_LEN, addr_node->name);
    if (ret != 0) {
        udis_err("strncpy_s failed. (module_type=%u; name=%s; ret=%d)\n", addr_node->module_type, addr_node->name, ret);
        dbl_kfree(new_node);
        return -ENOMEM;
    }

    ret = strcpy_s(new_node->device_segment, UDIS_SEGMENT_MAX_LEN, addr_node->device_segment);
    if (ret != 0) {
        udis_err("strncpy_s failed. (module_type=%u; name=%s; ret=%d)\n", addr_node->module_type, addr_node->name, ret);
        dbl_kfree(new_node);
        return -ENOMEM;
    }

    new_node->module_type = addr_node->module_type;
    new_node->update_type = addr_node->update_type;
    new_node->dev_dma_addr = addr_node->dev_dma_addr;
    new_node->host_dma_addr = addr_node->host_dma_addr;
    new_node->data_len = addr_node->data_len;
    new_node->host_va_addr = addr_node->host_va_addr;
    new_node->dev_va_addr = addr_node->dev_va_addr;
    new_node->dev_va_addr_attr = addr_node->dev_va_addr_attr;
    new_node->host_segment = addr_node->host_segment;
    new_node->device_segment_import = addr_node->device_segment_import;
    new_node->device_segment_ptr = addr_node->device_segment_ptr;
    new_node->host_segment_len = addr_node->host_segment_len;
    new_node->device_segment_import_len = addr_node->device_segment_import_len;
    new_node->token_value = addr_node->token_value;
    new_node->device_segment_len = addr_node->device_segment_len;
    new_node->device_addr_offset = addr_node->device_addr_offset;
    if (addr_node->update_type >= UPDATE_TYPE_MAX) {
        udis_err("Invalid update_type: %d\n", addr_node->update_type);
        dbl_kfree(new_node);
        return -EINVAL;
    }
    ka_list_add(&new_node->list, &udis_cb->addr_list[addr_node->update_type]);

    return 0;
}

void udis_addr_list_remove_node(unsigned int udevid, struct udis_ctrl_block *udis_cb, const struct udis_node *addr_node)
{
    struct udis_node *node = NULL;

    node = udis_addr_list_find_node(udis_cb, addr_node->module_type, addr_node->name);
    if (node == NULL) {
        udis_err("No matched node found. (udevid=%u; module_type=%u; name=%s)\n", udevid, addr_node->module_type,
                 addr_node->name);
        return;
    }

    ka_list_del(&node->list);
    dbl_kfree(node);
    node = NULL;

    return;
}