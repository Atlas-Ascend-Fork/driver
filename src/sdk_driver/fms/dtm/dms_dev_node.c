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
#include "fms/fms_dtm.h"
#include "fms_define.h"
#include "kernel_version_adapt.h"
#include "ascend_hal_error.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"
#include "ka_dfx_pub.h"

#define print_sysfs (void)ka_dfx_printk

int dms_check_node_type(int node_type)
{
    if (DMS_EVENT_OBJ_TYPE(node_type) == DMS_EVENT_OBJ_KERNEL) {
        if ((((unsigned int)node_type & 0xFFU) >= (unsigned int)DMS_DEV_TYPE_SOC) &&
            (((unsigned int)node_type & 0xFFU) < (unsigned int)DMS_DEV_TYPE_MAX)) {
            return DRV_ERROR_NONE;
        }
    }

    if (DMS_EVENT_OBJ_TYPE(node_type) == DMS_EVENT_OBJ_USER) {
        if ((node_type >= (int)HAL_DMS_DEV_TYPE_BASE_SERVCIE) && (node_type < (int)HAL_DMS_DEV_TYPE_MAX)) {
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_PARA_ERROR;
}
KA_EXPORT_SYMBOL(dms_check_node_type);

void dev_node_release(int owner_pid)
{
    int i;
    struct dms_node *node = NULL;
    struct dms_node *next = NULL;
    ka_list_head_t tmp_node;
    struct dms_dev_ctrl_block *dev_cb = NULL;

    KA_INIT_LIST_HEAD(&tmp_node);
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        dev_cb = dms_get_dev_cb(i);
        if (dev_cb == NULL) {
            continue;
        }
        ka_task_mutex_lock(&dev_cb->node_lock);
        if (ka_list_empty_careful(&dev_cb->dev_node_list) != 0) {
            ka_task_mutex_unlock(&dev_cb->node_lock);
            continue;
        }
        ka_list_for_each_entry_safe(node, next, &dev_cb->dev_node_list, list)
        {
            if ((node->pid == owner_pid) && (DMS_EVENT_OBJ_TYPE(node->node_type) == DMS_EVENT_OBJ_USER)) {
                dms_info("release dev_node. (devid=%d; node_type=0x%x; owner_pid=%d)\n", i, node->node_type, owner_pid);
                ka_list_del(&node->list);
                ka_list_add(&node->list, &tmp_node);
            }
        }
        ka_task_mutex_unlock(&dev_cb->node_lock);
    }

    ka_list_for_each_entry_safe(node, next, &tmp_node, list)
    {
        ka_list_del(&node->list);
        if ((node->ops != NULL) && (node->ops->uninit != NULL)) {
            /* Release all nodes of all devices under the PID. Only need to do it once. */
            node->ops->uninit(node);
            break;
        }
    }
    return;
}
KA_EXPORT_SYMBOL(dev_node_release);

#ifdef CFG_FEATURE_DEVICE_STATE_TABLE
static void dms_state_table_item_set(struct dms_node *node, struct dms_node *val)
{
    uint32_t i;
    struct dms_state_table *table = dms_get_state_table();

    if (node->owner_devid != 0) {
        return;
    }
    for (i = 0; i < table->num; i++) {
        if ((table->item[i].node_type == node->node_type) && (table->item[i].node_id == node->node_id)) {
            table->item[i].node = val;
            break;
        }
    }
}

static void dms_state_table_enable(struct dms_node *node)
{
    dms_state_table_item_set(node, node);
}

static void dms_state_table_disable(struct dms_node *node)
{
    dms_state_table_item_set(node, NULL);
}
#endif

static int _add_dev_node_to_list(struct dms_node *node)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;
    dev_cb = dms_get_dev_cb(node->owner_devid);
    if (dev_cb == NULL) {
        return -ENODEV;
    }
    ka_task_mutex_lock(&dev_cb->node_lock);
    ka_list_add(&node->list, &dev_cb->dev_node_list);
#ifdef CFG_FEATURE_DEVICE_STATE_TABLE
    dms_state_table_enable(node);
#endif
    ka_task_mutex_unlock(&dev_cb->node_lock);
    return 0;
}
static int _remove_dev_node_from_list(struct dms_node *node)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;
    dev_cb = dms_get_dev_cb(node->owner_devid);
    if (dev_cb == NULL) {
        return -ENODEV;
    }
    ka_task_mutex_lock(&dev_cb->node_lock);
    ka_list_del(&node->list);
#ifdef CFG_FEATURE_DEVICE_STATE_TABLE
    dms_state_table_disable(node);
#endif
    ka_task_mutex_unlock(&dev_cb->node_lock);
    return 0;
}
static int _devnode_check_para(struct dms_node *node)
{
    if (node == NULL) {
        dms_err("node is null\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (node->ops == NULL) {
        dms_err("ops is null. (node_type=%d; node_id=%d; devid=%d)\n", node->node_type, node->node_id,
                node->owner_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (node->ops->init == NULL) {
        dms_err("ops->init is null. (node_type=%d; node_id=%d; devid=%d)\n", node->node_type, node->node_id,
                node->owner_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (node->ops->uninit == NULL) {
        dms_err("ops->uninit is null. (node_type=%d; node_id=%d; devid=%d)\n", node->node_type, node->node_id,
                node->owner_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dms_check_node_type(node->node_type) != (int)DRV_ERROR_NONE) {
        dms_err("node_type is invalid. (node_type=0x%x; node_id=%d; devid=%d)\n", node->node_type, node->node_id,
                node->owner_devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (node->owner_devid >= ASCEND_DEV_MAX_NUM) {
        dms_err("owner_devid is error. (node_type=%d; node_id=%d; devid=%d)\n", node->node_type, node->node_id,
                node->owner_devid);
        return DRV_ERROR_INVALID_VALUE;
    }
    return 0;
}
int dms_register_dev_node(struct dms_node *node)
{
    int ret;
    ret = _devnode_check_para(node);
    if (ret != 0) {
        return ret;
    }
    /* call device node init function */
    if ((node->ops != NULL) && (node->ops->init != NULL)) {
        ret = node->ops->init(node);
        if (ret != 0) {
            return ret;
        }
    }
    ret = _add_dev_node_to_list(node);
    if (ret != 0) {
        if ((node->ops != NULL) && (node->ops->uninit != NULL)) {
            node->ops->uninit(node);
        }
    }
    return ret;
}
EXPORT_SYMBOL_ADAPT(dms_register_dev_node);

int dms_unregister_dev_node(struct dms_node *node)
{
    int ret;

    ret = _devnode_check_para(node);
    if (ret != 0) {
        return ret;
    }

    ret = _remove_dev_node_from_list(node);
    if (ret != 0) {
        return ret;
    }

    if ((node->ops != NULL) && (node->ops->uninit != NULL)) {
        node->ops->uninit(node);
    }
    return 0;
}
EXPORT_SYMBOL_ADAPT(dms_unregister_dev_node);

struct dms_node *dms_get_devnode_cb_nolock(u32 dev_id, int node_type, int node_id)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct dms_node *node = NULL;
    struct dms_node *next = NULL;
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        return NULL;
    }

    if (ka_list_empty_careful(&dev_cb->dev_node_list) != 0) {
        return NULL;
    }
    ka_list_for_each_entry_safe(node, next, &dev_cb->dev_node_list, list)
    {
        if ((node->node_id == node_id) && (node->node_type == node_type)) {
            return node;
        }
    }

    return NULL;
}

struct dms_node *dms_get_devnode_cb(u32 dev_id, int node_type, int node_id)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct dms_node *node = NULL;
    struct dms_node *next = NULL;
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        return NULL;
    }
    ka_task_mutex_lock(&dev_cb->node_lock);
    if (ka_list_empty_careful(&dev_cb->dev_node_list) != 0) {
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return NULL;
    }
    ka_list_for_each_entry_safe(node, next, &dev_cb->dev_node_list, list)
    {
        if ((node->node_id == node_id) && (node->node_type == node_type)) {
            ka_task_mutex_unlock(&dev_cb->node_lock);
            return node;
        }
    }

    ka_task_mutex_unlock(&dev_cb->node_lock);
    return NULL;
}
KA_EXPORT_SYMBOL(dms_get_devnode_cb);

int dms_update_devnode_cb(u32 dev_id, int node_type, int node_id, int (*update_dms_node)(struct dms_node *node))
{
    int ret;
    struct dms_node *node;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        return -EINVAL;
    }
    ka_task_mutex_lock(&dev_cb->node_lock);
    node = dms_get_devnode_cb_nolock(dev_id, node_type, node_id);
    if (node == NULL) {
        dms_err("node is unregistered. (node_type=%d; node_id=%d)\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -ENODEV;
    }

    if (update_dms_node == NULL) {
        dms_err("update_dms_node is NULL, node_type=%d, node_id=%d\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -EINVAL;
    }

    ret = update_dms_node(node);
    if (ret != 0) {
        ka_task_mutex_unlock(&dev_cb->node_lock);
        dms_err("update_dms_node failed, node_type=%d, node_id=%d\n", node_type, node_id);
        return ret;
    }
    ka_task_mutex_unlock(&dev_cb->node_lock);
    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL(dms_update_devnode_cb);

int dms_traverse_devnode_get_capacity(u32 dev_id, struct dsmi_dtm_node_s node_info[], unsigned int size,
                                      unsigned int *out_size)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct dms_node *node = NULL;
    struct dms_node *next = NULL;
    int ret;
    const unsigned long long support_power_mask = (0x1 << 1);
    unsigned long long capacity;

    if (node_info == NULL || out_size == NULL || size == 0) {
        dms_err("node_info or out_size is NULL or size is 0\n");
        return -EINVAL;
    }
    *out_size = 0;

    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        return -EINVAL;
    }
    ka_task_mutex_lock(&dev_cb->node_lock);
    if (ka_list_empty_careful(&dev_cb->dev_node_list) != 0) {
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -EINVAL;
    }
    ka_list_for_each_entry_safe(node, next, &dev_cb->dev_node_list, list)
    {
        if ((node->ops != NULL) && (node->ops->get_capacity != NULL)) {
            ret = node->ops->get_capacity(node, &capacity);
            if (ret != (int)DRV_ERROR_NONE) {
                dms_err("get_capacity failed, node_type=%d, node_id=%d\n", node->node_type, node->node_id);
                ka_task_mutex_unlock(&dev_cb->node_lock);
                return ret;
            }

            capacity &= support_power_mask;
            if (capacity == support_power_mask) {
                node_info[*out_size].node_type = node->node_type;
                node_info[*out_size].node_id = node->node_id;
                (*out_size)++;
                capacity = 0;
            }

            if (*out_size >= size) {
                dms_warn("dms_traverse_devnode_get_capacity out_size=%u, size=%d\n", *out_size, size);
                ka_task_mutex_unlock(&dev_cb->node_lock);
                return DRV_ERROR_NONE;
            }
        }
    }

    ka_task_mutex_unlock(&dev_cb->node_lock);
    return DRV_ERROR_NONE;
}
KA_EXPORT_SYMBOL(dms_traverse_devnode_get_capacity);

int dms_devnode_get_state(u32 dev_id, int node_type, int node_id, u32 *state)
{
    struct dms_node *node = NULL;
    node = dms_get_devnode_cb(dev_id, node_type, node_id);
    if (node == NULL) {
        return -ENODEV;
    }
    if ((node->ops == NULL) || (node->ops->get_state == NULL)) {
        return -EOPNOTSUPP;
    }
    return node->ops->get_state(node, state);
}

int dms_devnode_get_capacity(u32 dev_id, int node_type, int node_id, unsigned long long *cap)
{
    struct dms_node *node = NULL;
    node = dms_get_devnode_cb(dev_id, node_type, node_id);
    if (node == NULL) {
        return -ENODEV;
    }
    if ((node->ops == NULL) || (node->ops->get_capacity == NULL)) {
        return -EOPNOTSUPP;
    }
    return node->ops->get_capacity(node, cap);
}

int dms_devnode_enable_device(u32 dev_id, int node_type, int node_id)
{
    struct dms_node *node = NULL;
    int ret;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    dms_event("dms_devnode_enable_device, node_type=%d, node_id=%d\n", node_type, node_id);
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        return -EINVAL;
    }
    ka_task_mutex_lock(&dev_cb->node_lock);
    node = dms_get_devnode_cb_nolock(dev_id, node_type, node_id);
    if (node == NULL) {
        dms_err("node is unregistered. (node_type=%d; node_id=%d)\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -ENODEV;
    }
    if (node->ops == NULL) {
        dms_err("node->ops is NULL, node_type=%d, node_id=%d\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -EINVAL;
    }

    if (node->ops->enable_device == NULL) {
        dms_err("enable_device is NULL, node_type=%d, node_id=%d\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -EINVAL;
    }
    ret = node->ops->enable_device(node);
    if (ret != 0) {
        dms_err("enable_device failed, node_type=%d, node_id=%d\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return ret;
    }

    ka_task_mutex_unlock(&dev_cb->node_lock);
    return ret;
}
KA_EXPORT_SYMBOL(dms_devnode_enable_device);

int dms_devnode_disable_device(u32 dev_id, int node_type, int node_id)
{
    struct dms_node *node = NULL;
    int ret;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    dms_event("dms_devnode_disable_device, node_type=%d, node_id=%d\n", node_type, node_id);
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        return -EINVAL;
    }
    ka_task_mutex_lock(&dev_cb->node_lock);
    node = dms_get_devnode_cb_nolock(dev_id, node_type, node_id);
    if (node == NULL) {
        dms_err("node is unregistered. (node_type=%d; node_id=%d)\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -ENODEV;
    }

    if (node->ops == NULL) {
        dms_err("node->ops is NULL, node_type=%d, node_id=%d\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -EINVAL;
    }

    if (node->ops->disable_device == NULL) {
        dms_err("disable_device is NULL, node_type=%d, node_id=%d\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -EINVAL;
    }
    ret = node->ops->disable_device(node);
    if (ret != 0) {
        ka_task_mutex_unlock(&dev_cb->node_lock);
        dms_err("disable_device failed, node_type=%d, node_id=%d\n", node_type, node_id);
        return ret;
    }

    ka_task_mutex_unlock(&dev_cb->node_lock);
    return ret;
}
KA_EXPORT_SYMBOL(dms_devnode_disable_device);

int dms_devnode_get_power_info(u32 dev_id, int node_type, int node_id, void *buf, unsigned int size)
{
    struct dms_node *node = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    int ret;

    if (buf == NULL) {
        dms_err("buf is NULL\n");
        return -EINVAL;
    }
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        return -EINVAL;
    }
    ka_task_mutex_lock(&dev_cb->node_lock);
    node = dms_get_devnode_cb_nolock(dev_id, node_type, node_id);
    if (node == NULL) {
        dms_err("node is unregistered. (node_type=%d; node_id=%d)\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -ENODEV;
    }

    if (node->ops == NULL) {
        dms_err("node->ops is NULL, node_type=%d, node_id=%d\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -EINVAL;
    }

    if (node->ops->get_power_info == NULL) {
        dms_err("get_power_info is NULL, node_type=%d, node_id=%d\n", node_type, node_id);
        ka_task_mutex_unlock(&dev_cb->node_lock);
        return -EINVAL;
    }
    ret = node->ops->get_power_info(node, buf, size);
    if (ret != 0) {
        ka_task_mutex_unlock(&dev_cb->node_lock);
        dms_err("get_power_info failed, node_type=%d, node_id=%d\n", node_type, node_id);
        return ret;
    }

    ka_task_mutex_unlock(&dev_cb->node_lock);
    return ret;
}
KA_EXPORT_SYMBOL(dms_devnode_get_power_info);

int dms_devnode_set_power_info(u32 dev_id, int node_type, int node_id, void *buf, unsigned int size)
{
    struct dms_node *node = NULL;
    int ret;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    dms_event("dms_devnode_set_power_info, node_type=%d, node_id=%d\n", node_type, node_id);
    if (buf == NULL) {
        dms_err("buf is NULL\n");
        return -EINVAL;
    }
    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        return -EINVAL;
    }
    ka_task_mutex_lock(&dev_cb->node_lock);
    node = dms_get_devnode_cb_nolock(dev_id, node_type, node_id);
    if (node == NULL) {
        ka_task_mutex_unlock(&dev_cb->node_lock);
        dms_err("node is unregistered. (node_type=%d; node_id=%d)\n", node_type, node_id);
        return -ENODEV;
    }

    if (node->ops == NULL) {
        ka_task_mutex_unlock(&dev_cb->node_lock);
        dms_err("node->ops is NULL, node_type=%d, node_id=%d\n", node_type, node_id);
        return -EINVAL;
    }

    if (node->ops->set_power_info == NULL) {
        ka_task_mutex_unlock(&dev_cb->node_lock);
        dms_err("set_power_info is NULL, node_type=%d, node_id=%d\n", node_type, node_id);
        return -EINVAL;
    }
    ret = node->ops->set_power_info(node, buf, size);
    if (ret != 0) {
        ka_task_mutex_unlock(&dev_cb->node_lock);
        dms_err("set_power_info failed, node_type=%d, node_id=%d\n", node_type, node_id);
        return ret;
    }

    ka_task_mutex_unlock(&dev_cb->node_lock);
    return ret;
}
KA_EXPORT_SYMBOL(dms_devnode_set_power_info);

int dms_devnode_set_power_state(u32 dev_id, int node_type, int node_id, DSMI_POWER_STATE state)
{
    struct dms_node *node = NULL;
    node = dms_get_devnode_cb(dev_id, node_type, node_id);
    if (node == NULL) {
        return -ENODEV;
    }
    if ((node->ops == NULL) || (node->ops->set_power_state == NULL)) {
        return -EOPNOTSUPP;
    }
    return node->ops->set_power_state(node, state);
}

int dms_devnode_fault_diag(u32 dev_id, int node_type, int node_id, int *state)
{
    struct dms_node *node = NULL;
    node = dms_get_devnode_cb(dev_id, node_type, node_id);
    if (node == NULL) {
        return -ENODEV;
    }
    if ((node->ops == NULL) || (node->ops->fault_diag == NULL)) {
        return -EOPNOTSUPP;
    }
    return node->ops->fault_diag(node, state);
}

ssize_t dms_devnode_print_node_list(char *buf)
{
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct dms_node *node = NULL;
    struct dms_node *next = NULL;
    int i;
    ssize_t buf_ret = 0;
    ssize_t offset = 0;
    offset = snprintf_s(buf, KA_MM_PAGE_SIZE, KA_MM_PAGE_SIZE - 1UL,
                        "dev_id\ttype\tnode_id\tname\tcap\tperm\tstate\towner\n");
    print_sysfs("==================================\n");
    print_sysfs("dev_id\ttype\tnode_id\tname\tcap\tperm\tstate\towner\n");
    if (offset < 0) {
        return 0;
    }
    buf_ret += offset;
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        dev_cb = dms_get_dev_cb(i);
        if (dev_cb == NULL) {
            continue;
        }
        ka_task_mutex_lock(&dev_cb->node_lock);
        if (ka_list_empty_careful(&dev_cb->dev_node_list) != 0) {
            ka_task_mutex_unlock(&dev_cb->node_lock);
            continue;
        }
        ka_list_for_each_entry_safe(node, next, &dev_cb->dev_node_list, list)
        {
            print_sysfs("%d\t%d\t%d\t%s\t%llx\t%x\t%d\t%s\n", i, node->node_type, node->node_id, node->node_name,
                        node->capacity, node->permission, node->state,
                        (node->owner_device == NULL) ? "null" : node->owner_device->node_name);
            offset = snprintf_s(buf + buf_ret, KA_MM_PAGE_SIZE - buf_ret, KA_MM_PAGE_SIZE - 1 - buf_ret,
                                "%d\t%d\t%d\t%s\t%llx\t%x\t%d\t%s\n", i, node->node_type, node->node_id,
                                node->node_name, node->capacity, node->permission, node->state,
                                (node->owner_device == NULL) ? "null" : node->owner_device->node_name);
            if (offset >= 0) {
                buf_ret += offset;
            }
        }
        ka_task_mutex_unlock(&dev_cb->node_lock);
    }

    return buf_ret;
}
KA_EXPORT_SYMBOL(dms_devnode_print_node_list);
