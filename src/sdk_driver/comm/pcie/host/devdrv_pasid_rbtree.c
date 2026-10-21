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

#include "pbl/pbl_uda.h"
#include "devdrv_ctrl.h"
#include "devdrv_pasid_rbtree.h"
#include "devdrv_mem_alloc.h"
#include "devdrv_util.h"

#define DEVDRV_NON_TRANS_MSG_SVA_DESC_SIZE 0x400
struct devdrv_dma_pasid_rbtree_ctrl g_pasid_rbtree[MAX_DEV_CNT] = {0};

STATIC int devdrv_dma_pasid_node_insert(ka_rwlock_t *lock, ka_rb_root_t *root,
                                        struct devdrv_dma_pasid_rbtree_node *dma_pasid_node)
{
    ka_rb_node_t **new_node = NULL;
    ka_rb_node_t *parent = NULL;
    u64 new_node_hash, tree_hash;

    ka_task_write_lock(lock);
    new_node = ka_base_get_rb_root_node_addr(root);
    new_node_hash = dma_pasid_node->hash_va;

    /* Figure out where to put new node */
    while (*new_node) {
        struct devdrv_dma_pasid_rbtree_node *this = ka_base_rb_entry(*new_node, struct devdrv_dma_pasid_rbtree_node,
                                                                     node);

        parent = *new_node;
        tree_hash = this->hash_va;
        if (new_node_hash < tree_hash) {
            new_node = &((*new_node)->rb_left);
        } else if (new_node_hash > tree_hash) {
            new_node = &((*new_node)->rb_right);
        } else {
            ka_task_write_unlock(lock);
            return -EEXIST; // Node already exists
        }
    }

    /* Add new node and rebalance tree. */
    ka_base_rb_link_node(&dma_pasid_node->node, parent, new_node);
    ka_base_rb_insert_color(&dma_pasid_node->node, root);

    ka_task_write_unlock(lock);

    return 0;
}

STATIC struct devdrv_dma_pasid_rbtree_node *devdrv_dma_pasid_node_search(ka_rwlock_t *lock, ka_rb_root_t *root,
                                                                         u64 hash_va)
{
    u64 tree_hash;
    ka_rb_node_t *node = NULL;
    struct devdrv_dma_pasid_rbtree_node *dma_pasid_node = NULL;

    if (lock != NULL) {
        ka_task_read_lock(lock);
    }
    node = ka_base_get_rb_root_node(root);
    while (node != NULL) {
        dma_pasid_node = ka_base_rb_entry(node, struct devdrv_dma_pasid_rbtree_node, node);
        tree_hash = dma_pasid_node->hash_va;

        if (hash_va < tree_hash) {
            node = node->rb_left;
        } else if (hash_va > tree_hash) {
            node = node->rb_right;
        } else {
            if (lock != NULL) {
                ka_task_read_unlock(lock);
            }
            return dma_pasid_node;
        }
    }

    if (lock != NULL) {
        ka_task_read_unlock(lock);
    }
    return NULL;
}

STATIC void devdrv_dma_pasid_node_erase(ka_rwlock_t *lock, ka_rb_root_t *root,
                                        struct devdrv_dma_pasid_rbtree_node *dma_pasid_node)
{
    // Erase may run with other processes under a lock or alone; thus, the lock can be null for easier combination.
    if (lock != NULL) {
        ka_task_write_lock(lock);
    }
    ka_base_rb_erase(&dma_pasid_node->node, root);
    if (lock != NULL) {
        ka_task_write_unlock(lock);
    }
}

STATIC int devdrv_pasid_rbtree_add(u32 dev_id, u64 pasid)
{
    struct devdrv_dma_pasid_rbtree_node *node = NULL;
    int ret;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Invalid dev_id.(dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    node = devdrv_kzalloc(sizeof(struct devdrv_dma_pasid_rbtree_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (node == NULL) {
        devdrv_err("Alloc mem for rb_tree failed.(dev_id=%u).\n", dev_id);
        return -ENOMEM;
    }

    node->dev_id = dev_id;
    node->hash_va = pasid;

    ret = devdrv_dma_pasid_node_insert(&g_pasid_rbtree[dev_id].rb_lock, &g_pasid_rbtree[dev_id].rbtree, node);
    if (ret != 0) {
        devdrv_err("Insert rbtree failed.(dev_id=%u; pasid=%llu)\n", dev_id, pasid);
        devdrv_kfree(node);
        return ret;
    }

    return 0;
}

STATIC int devdrv_pasid_rbtree_del(u32 dev_id, u64 pasid)
{
    struct devdrv_dma_pasid_rbtree_node *dma_pasid_node = NULL;
    struct devdrv_dma_pasid_rbtree_ctrl *tree = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err("Invalid dev_id.(dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    // Erase node from local tree
    tree = &g_pasid_rbtree[dev_id];
    ka_task_write_lock(&tree->rb_lock);
    dma_pasid_node = devdrv_dma_pasid_node_search(NULL, &tree->rbtree, pasid);
    if (dma_pasid_node == NULL) {
        ka_task_write_unlock(&tree->rb_lock);
        devdrv_err("Pasid is not in rbtree.(dev_id=%u; pasid=%llu)\n", dev_id, pasid);
        return -ENOENT; // Can not find node
    }

    devdrv_dma_pasid_node_erase(NULL, &tree->rbtree, dma_pasid_node);
    ka_task_write_unlock(&tree->rb_lock);
    devdrv_kfree(dma_pasid_node);
    return 0;
}

STATIC int devdrv_pasid_msg_edge_check(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *p_real_out_len)
{
    u32 max_data_len = DEVDRV_NON_TRANS_MSG_SVA_DESC_SIZE - DEVDRV_NON_TRANS_MSG_HEAD_LEN;
    struct devdrv_pasid_msg *recv_msg = NULL;

    if ((devid >= MAX_DEV_CNT) || (data == NULL) || (p_real_out_len == NULL)) {
        devdrv_err("Input parameter is error.(devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((in_data_len < sizeof(struct devdrv_pasid_msg)) || (in_data_len > max_data_len)) {
        devdrv_err("Input parameter in_data_len is error.(devid=%u; in_data_len=0x%x)\n", devid, in_data_len);
        return -EINVAL;
    }

    recv_msg = (struct devdrv_pasid_msg *)data;
    if ((recv_msg->op_code >= (u32)DEVDRV_PASID_MAX) || (recv_msg->dev_id >= MAX_DEV_CNT)) {
        devdrv_err("Pasid message channel recv message op_code or devid error.(dev_id=%u; msg_type=%u; msg_devid=%u)\n",
                   devid, recv_msg->op_code, recv_msg->dev_id);
        return -EINVAL;
    }

    return 0;
}

STATIC int devdrv_pasid_non_trans_msg_recv(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len,
                                           u32 *real_out_len)
{
    u32 devid = (u32)devdrv_get_msg_chan_devid_inner(msg_chan);
    struct devdrv_pasid_msg *recv_msg;
    u32 op_code, index_id;
    int ret;

    if (devdrv_pasid_msg_edge_check(devid, data, in_data_len, out_data_len, real_out_len) != 0) {
        devdrv_err("Input parameters check failed.(devid=%u)\n", devid);
        return -EINVAL;
    }
    *real_out_len = sizeof(struct devdrv_pasid_msg);
    recv_msg = (struct devdrv_pasid_msg *)data;
    op_code = recv_msg->op_code;

    ret = uda_udevid_to_add_id(recv_msg->dev_id, &index_id);
    if (ret != 0) {
        devdrv_err("Devid to index_id failed.(dev_id=%u; index_id=%u)\n", recv_msg->dev_id, index_id);
        return -EINVAL;
    }
    if (op_code == DEVDRV_PASID_ADD) {
        ret = devdrv_pasid_rbtree_add(index_id, recv_msg->hash_va);
        if (ret != 0) {
            devdrv_err("Add node to rbtree failed.(index_id=%u; pasid=%llu)\n", index_id, recv_msg->hash_va);
        }
    } else {
        ret = devdrv_pasid_rbtree_del(index_id, recv_msg->hash_va);
        if (ret != 0) {
            devdrv_err("Del node from rbtree failed.(index_id=%u; pasid=%llu)\n", index_id, recv_msg->hash_va);
        }
    }

    recv_msg->error_code = ret;
    return 0;
}

STATIC void devdrv_pasid_rbtree_clear(u32 dev_id)
{
    struct devdrv_dma_pasid_rbtree_node *dma_pasid_node = NULL;
    struct devdrv_dma_pasid_rbtree_ctrl *tree = NULL;
    ka_rb_node_t *node = NULL;

    tree = &g_pasid_rbtree[dev_id];
    ka_task_write_lock(&tree->rb_lock);

    while ((node = ka_base_rb_first(&tree->rbtree)) != NULL) {
        dma_pasid_node = ka_base_rb_entry(node, struct devdrv_dma_pasid_rbtree_node, node);
        devdrv_dma_pasid_node_erase(NULL, &tree->rbtree, dma_pasid_node);
        ka_task_write_unlock(&tree->rb_lock);
        devdrv_kfree(dma_pasid_node);
        ka_task_write_lock(&tree->rb_lock);
    }

    ka_task_write_unlock(&tree->rb_lock);

    return;
}

STATIC struct devdrv_non_trans_msg_chan_info devdrv_pasid_non_trans_msg_chan_info = {
    .msg_type = devdrv_msg_client_pasid,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_HIGH,
    .s_desc_size = DEVDRV_NON_TRANS_MSG_SVA_DESC_SIZE,
    .c_desc_size = DEVDRV_NON_TRANS_MSG_SVA_DESC_SIZE,
    .rx_msg_process = devdrv_pasid_non_trans_msg_recv,
};

bool devdrv_dma_pasid_valid_check(u32 dev_id, u64 pasid)
{
    struct devdrv_dma_pasid_rbtree_node *dma_pasid_node = NULL;
    struct devdrv_dma_pasid_rbtree_ctrl *tree = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;

    if (dev_id >= MAX_DEV_CNT) {
        devdrv_err_spinlock("Invalid dev_id.(dev_id=%u)\n", dev_id);
        return false;
    }

    pci_ctrl = devdrv_get_top_half_pci_ctrl_by_id(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err_spinlock("Get pci_ctrl failed. (devid=%u)\n", dev_id);
        return false;
    }

    if (!devdrv_feature_is_support(pci_ctrl->features, DEVDRV_FEATURE_DMA_COPY_SVA)) {
        return true;
    }

    tree = &g_pasid_rbtree[dev_id];
    dma_pasid_node = devdrv_dma_pasid_node_search(&tree->rb_lock, &tree->rbtree, pasid);
    if (dma_pasid_node == NULL) {
        devdrv_err_spinlock("Pasid is not in rbtree.(dev_id=%u; pasid=%llu)\n", dev_id, pasid);
        return false;
    }

    return true;
}

void devdrv_pasid_rbtree_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (!devdrv_feature_is_support(pci_ctrl->features, DEVDRV_FEATURE_DMA_COPY_SVA)) {
        return;
    }
    g_pasid_rbtree[pci_ctrl->dev_id].rbtree = KA_RB_ROOT;
    ka_task_rwlock_init(&g_pasid_rbtree[pci_ctrl->dev_id].rb_lock);
}

void devdrv_pasid_rbtree_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (!devdrv_feature_is_support(pci_ctrl->features, DEVDRV_FEATURE_DMA_COPY_SVA)) {
        return;
    }
    devdrv_pasid_rbtree_clear(pci_ctrl->dev_id);
}

int devdrv_pasid_non_trans_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    void *msg_chan = NULL;

    if (!devdrv_feature_is_support(pci_ctrl->features, DEVDRV_FEATURE_DMA_COPY_SVA)) {
        return 0;
    }

    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        return 0;
    }

    msg_chan = devdrv_pcimsg_alloc_non_trans_queue_inner(pci_ctrl->dev_id, &devdrv_pasid_non_trans_msg_chan_info);
    if (msg_chan == NULL) {
        devdrv_err("Alloc common msg_queue failed, msg_chan is null. (dev_id=%u)\n", pci_ctrl->dev_id);
        return -EINVAL;
    }

    /* save common msg_chan to msg_dev */
    g_pasid_rbtree[pci_ctrl->dev_id].msg_chan = (struct devdrv_msg_chan *)msg_chan;
    return 0;
}

void devdrv_pasid_non_trans_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret;

    if (!devdrv_feature_is_support(pci_ctrl->features, DEVDRV_FEATURE_DMA_COPY_SVA)) {
        return;
    }

    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        return;
    }

    ret = devdrv_pcimsg_free_non_trans_queue_inner((void *)g_pasid_rbtree[pci_ctrl->dev_id].msg_chan);
    if (ret != 0) {
        devdrv_info("No need to free common msg_queue. (dev_id=%u;ret=%d)\n", pci_ctrl->dev_id, ret);
    }

    g_pasid_rbtree[pci_ctrl->dev_id].msg_chan = NULL;
}