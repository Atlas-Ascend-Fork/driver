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
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_common_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"
#include "ka_fs_pub.h"
#include "comm_kernel_interface.h"
#include "kernel_version_adapt.h"
#include "pbl_kref_safe.h"
#include "trs_pub_def.h"
#include "trs_chan_update.h"
#include "trs_chan.h"
#include "trs_chan_near_ops_mem.h"
#include "trs_sqe_update.h"
#include "pbl/pbl_uda.h"

/* stub for david ub scene start */
#ifndef EMU_ST
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
struct devdrv_dma_prepare *devdrv_dma_link_prepare(u32 devid, enum devdrv_dma_data_type type,
                                                   struct devdrv_dma_node *dma_node, u32 node_cnt, u32 fill_status)
{
    return NULL;
}

int devdrv_dma_link_free(struct devdrv_dma_prepare *dma_prepare)
{
    return -1;
}

ka_dma_addr_t hal_kernel_devdrv_dma_map_page(ka_device_t *dev, ka_page_t *page, size_t offset, size_t size,
                                             ka_dma_data_direction_t dir)
{
    return (ka_dma_addr_t)NULL;
}

void hal_kernel_devdrv_dma_unmap_page(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir)
{}
#endif
#endif
/* stub for david ub scene end */

#define TRS_DMA_NODE_RB_ROOT_NUM 256
struct trs_rb_info {
    ka_rb_root_t root[TRS_DMA_NODE_RB_ROOT_NUM];
    u32 rb_cnt;
    ka_spinlock_t spinlock;
    struct kref_safe ref;
    u32 udevid;
};

struct trs_rb_info *trs_dma_desc_rb[TRS_DEV_MAX_NUM] = {
    NULL,
};
static KA_TASK_DEFINE_RWLOCK(trs_rb_desc_lock);

struct trs_security_src_info {
    ka_page_t *src_pages;
    ka_dma_addr_t dma_addr;
    u32 size;
    bool is_src_secure;
};

struct trs_dma_desc_node {
    ka_rb_node_t desc_node;
    u64 rb_handle;

    /* rb key */
    u32 sqid;
    u32 sqeid;
    struct trs_security_src_info src_info;

    struct devdrv_dma_prepare *dma_prepare;
    struct trs_dma_desc dma_desc;
};
static struct trs_dma_desc_node *trs_dma_desc_node_del_one_from_root(struct trs_rb_info *rb_info, u32 root_index);
static void trs_sqe_update_desc_destroy_all(struct trs_rb_info *rb_info);

static inline void trs_rb_get_root_index(u32 sqid, u32 *root_index)
{
    *root_index = sqid % TRS_DMA_NODE_RB_ROOT_NUM;
}

static struct trs_rb_info *trs_rb_info_get(u32 devid)
{
    struct trs_rb_info *rb_info = NULL;

    ka_task_read_lock_bh(&trs_rb_desc_lock);
    rb_info = trs_dma_desc_rb[devid];
    if (rb_info != NULL) {
        kref_safe_get(&rb_info->ref);
    }
    ka_task_read_unlock_bh(&trs_rb_desc_lock);

    return rb_info;
}

static void trs_rb_info_release(struct kref_safe *kref)
{
    struct trs_rb_info *rb_info = ka_container_of(kref, struct trs_rb_info, ref);

    if (rb_info->rb_cnt != 0) {
        trs_sqe_update_desc_destroy_all(rb_info);
    }

    trs_vfree(rb_info);
}

static void trs_rb_info_put(struct trs_rb_info *rb_info)
{
    kref_safe_put(&rb_info->ref, trs_rb_info_release);
}

static bool trs_rb_root_is_empty(ka_rb_root_t *root)
{
    return KA_BASE_RB_EMPTY_ROOT(root);
}

typedef u64 (*rb_handle_func)(ka_rb_node_t *node);
static int trs_rb_insert(ka_rb_root_t *root, ka_rb_node_t *node, rb_handle_func get_handle)
{
    ka_rb_node_t **cur_node = &root->rb_node;
    ka_rb_node_t *parent = NULL;
    u64 handle = get_handle(node);

    /* Figure out where to put new node */
    while (*cur_node) {
        u64 tmp_handle = get_handle(*cur_node);

        parent = *cur_node;
        if (handle < tmp_handle) {
            cur_node = &((*cur_node)->rb_left);
        } else if (handle > tmp_handle) {
            cur_node = &((*cur_node)->rb_right);
        } else {
            trs_err("Insert same priv.\n");
            return -EINVAL;
        }
    }

    /* Add new node and rebalance tree. */
    ka_base_rb_link_node(node, parent, cur_node);
    ka_base_rb_insert_color(node, root);
    return 0;
}

static ka_rb_node_t *trs_rb_search(ka_rb_root_t *root, u64 handle, rb_handle_func get_handle)
{
    ka_rb_node_t *node = NULL;

    node = ka_base_get_rb_root_node(root);
    while (node != NULL) {
        u64 tmp_handle = get_handle(node);
        if (handle < tmp_handle) {
            node = ka_base_get_rb_node_left(node);
        } else if (handle > tmp_handle) {
            node = ka_base_get_rb_node_right(node);
        } else {
            return node;
        }
    }

    return NULL;
}

static int trs_rb_erase(ka_rb_root_t *root, ka_rb_node_t *node)
{
    int ret = -ENODEV;

    if (KA_BASE_RB_EMPTY_NODE(node) == false) {
        ka_base_rb_erase(node, root);
        KA_BASE_RB_CLEAR_NODE(node);
        ret = 0;
    }
    return ret;
}

static u64 rb_handle_of_desc_node(ka_rb_node_t *node)
{
    struct trs_dma_desc_node *tmp = ka_base_rb_entry(node, struct trs_dma_desc_node, desc_node);
    return tmp->rb_handle;
}

static int trs_dma_desc_node_find(u32 devid, u32 sqid, u64 rb_handle, struct trs_dma_desc *dma_desc)
{
    struct trs_rb_info *rb_info = NULL;
    struct trs_dma_desc_node *dma_desc_node = NULL;
    ka_rb_node_t *node = NULL;
    int ret = -ENODEV;
    u32 root_index = 0;

    rb_info = trs_rb_info_get(devid);
    if (rb_info == NULL) {
        trs_err("Invalid rb info. (devid=%u; sqid=%u)\n", devid, sqid);
        return ret;
    }

    trs_rb_get_root_index(sqid, &root_index);
    ka_task_spin_lock_bh(&rb_info->spinlock);
    node = trs_rb_search(&rb_info->root[root_index], rb_handle, rb_handle_of_desc_node);
    if (node != NULL) {
        dma_desc_node = ka_base_rb_entry(node, struct trs_dma_desc_node, desc_node);
        *dma_desc = dma_desc_node->dma_desc;
        ret = 0;
    }
    ka_task_spin_unlock_bh(&rb_info->spinlock);

    trs_rb_info_put(rb_info);

    return ret;
}

static int trs_dma_desc_node_create(u32 devid, struct trs_dma_desc_addr_info *addr_info,
                                    struct devdrv_dma_prepare *dma_prepare, struct trs_dma_desc *dma_desc,
                                    struct trs_security_src_info *src_info)
{
    struct trs_rb_info *rb_info = NULL;
    struct trs_dma_desc_node *node = NULL;
    u32 root_index = 0;
    u32 chip_type;
    u64 rb_handle;
    int ret;

    rb_info = trs_rb_info_get(devid);
    if (rb_info == NULL) {
        trs_err("Invalid rb info. (devid=%u; sqid=%u)\n", devid, addr_info->sqid);
        return -ENXIO;
    }

    node = trs_vmalloc(sizeof(struct trs_dma_desc_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (node == NULL) {
        trs_rb_info_put(rb_info);
        trs_err("Vzalloc failed.\n");
        return -ENOMEM;
    }

    chip_type = uda_get_chip_type(devid);
    if ((chip_type != HISI_CLOUD_V4) && (chip_type != HISI_CLOUD_V5) && (chip_type != HISI_MINI_V4)) {
        rb_handle = (((u64)(addr_info->sqid) << 32) | (u64)(addr_info->sqeid)); /* low 32 bits is sqeid */
    } else {
        rb_handle = (u64)(uintptr_t)node;
    }

    node->sqid = addr_info->sqid;
    node->sqeid = addr_info->sqeid;
    node->dma_prepare = dma_prepare;
    node->dma_desc = *dma_desc;
    node->rb_handle = rb_handle;
    node->src_info.is_src_secure = true;
    if (src_info != NULL) {
        node->src_info = *src_info;
    }

    KA_BASE_RB_CLEAR_NODE(&node->desc_node);

    trs_rb_get_root_index(addr_info->sqid, &root_index);
    ka_task_spin_lock_bh(&rb_info->spinlock);
    ret = trs_rb_insert(&rb_info->root[root_index], &node->desc_node, rb_handle_of_desc_node);
    if (ret == 0) {
        rb_info->rb_cnt++;
    }
    ka_task_spin_unlock_bh(&rb_info->spinlock);
    if (ret != 0) {
        trs_vfree(node);
    }

    trs_rb_info_put(rb_info);

    return ret;
}

static void trs_dma_node_pack(struct trs_dma_desc_addr_info *addr_info, struct trs_security_src_info *src_info,
                              struct trs_chan_sq_info *sq_info, u32 sq_mem_side, struct devdrv_dma_node *dma_node)
{
    if (sq_mem_side == TRS_CHAN_DEV_SVM_MEM) {
        dma_node->src_addr = (u64)src_info->dma_addr;
        dma_node->dst_addr = (uintptr_t)sq_info->sq_para.sq_que_uva + addr_info->sqeid * sq_info->sq_para.sqe_size +
                             addr_info->offset;
        dma_node->direction = DEVDRV_DMA_HOST_TO_DEVICE;
    } else {
        dma_node->src_addr = addr_info->src_va;
        dma_node->dst_addr = sq_info->sq_phy_addr + addr_info->sqeid * sq_info->sq_para.sqe_size + addr_info->offset;
        dma_node->direction = DEVDRV_DMA_DEVICE_TO_HOST;
    }

    dma_node->loc_passid = addr_info->passid;
    dma_node->size = addr_info->size;
}

#define TRS_SQE_UPDATE_LIMITED (2048 * 512)
static bool trs_is_rb_node_limited(u32 devid)
{
    struct trs_rb_info *rb_info = NULL;

    rb_info = trs_rb_info_get(devid);
    if (rb_info == NULL) {
        trs_err("Invalid rb info. (devid=%u)\n", devid);
        return true;
    }

    ka_task_spin_lock_bh(&rb_info->spinlock);
    if (rb_info->rb_cnt >= TRS_SQE_UPDATE_LIMITED) {
        ka_task_spin_unlock_bh(&rb_info->spinlock);
        trs_rb_info_put(rb_info);
        trs_warn("Dma_desc is limited. (devid=%u; cnt=%u)\n", devid, rb_info->rb_cnt);
        return true;
    }
    ka_task_spin_unlock_bh(&rb_info->spinlock);
    trs_rb_info_put(rb_info);

    return false;
}

static void trs_fill_sqe_update_info(struct trs_sqe_update_info *update_info, struct trs_chan_sq_info *sq_info,
                                     struct trs_dma_desc_addr_info *addr_info, u8 *sqe_dst_buf, ka_page_t *tmp_pages)
{
    update_info->sqid = addr_info->sqid;
    update_info->sqeid = addr_info->sqeid;
    update_info->sqe = ka_mm_page_to_virt(tmp_pages);
    update_info->size = addr_info->size;
    if (sq_info->vmap_pfn_handle != NULL) {
        /* Physical address non-contiguous: read dst sqe via vmap_pfn handle */
        unsigned long dst_offset = (unsigned long)addr_info->sqeid * sq_info->sq_para.sqe_size + sq_info->addr_offset;
        ka_mm_vmap_pfn_memcpy_fromio(sq_info->vmap_pfn_handle, dst_offset, sqe_dst_buf, addr_info->size);
        update_info->dst_sqe = sqe_dst_buf;
        update_info->addr_non_continuous = true;
    } else {
        update_info->sq_base = sq_info->sq_vaddr;
    }
}

static int trs_check_alloc_src_security_dma_addr(u32 devid, struct trs_dma_desc_addr_info *addr_info,
                                                 struct trs_chan_sq_info *sq_info, bool is_src_secure,
                                                 struct trs_security_src_info *src_info)
{
    struct trs_id_inst inst = {0};
    ka_device_t *dev = NULL;
    ka_page_t *tmp_pages = NULL;
    struct trs_sqe_update_info update_info = {0};
    u8 sqe_dst_buf[SQE_CACHE_SIZE];
    int ret = 0;

    /* The src is uva in milan, no need dma map;
     * The src always not secure in current scene in david, secure src dma map not implemented yet.
     */
    src_info->is_src_secure = is_src_secure;
    if (is_src_secure == true) {
        return 0;
    }

    dev = hal_kernel_devdrv_get_pci_dev_by_devid(devid);
    if (dev == NULL) {
        return -ENODEV;
    }

    tmp_pages = ka_mm_alloc_pages(KA_GFP_KERNEL, ka_mm_get_order(addr_info->size));
    if (tmp_pages == NULL) {
        trs_err("Alloc pages failed.\n");
        return -ENOMEM;
    }

    ret = ka_base_copy_from_user(ka_mm_page_to_virt(tmp_pages), (void *)(uintptr_t)addr_info->src_va, addr_info->size);
    if (ret != 0) {
        __ka_mm_free_pages(tmp_pages, ka_mm_get_order(addr_info->size));
        trs_err("Copy from user failed. (ret=%d; size=%d)\n", ret, addr_info->size);
        return -EINVAL;
    }

    trs_id_inst_pack(&inst, devid, 0);
    trs_fill_sqe_update_info(&update_info, sq_info, addr_info, sqe_dst_buf, tmp_pages);

    ret = trs_chan_ops_sqe_update_src_check(&inst, &update_info);
    if (ret != 0) {
        __ka_mm_free_pages(tmp_pages, ka_mm_get_order(addr_info->size));
        trs_err("Sqe update src sqe check failed. (ret=%d)\n", ret);
        return -EFAULT;
    }

    src_info->dma_addr = hal_kernel_devdrv_dma_map_page(dev, tmp_pages, 0, addr_info->size, KA_DMA_BIDIRECTIONAL);
    if (ka_mm_dma_mapping_error(dev, src_info->dma_addr)) {
        __ka_mm_free_pages(tmp_pages, ka_mm_get_order(addr_info->size));
        trs_err("Dma map va failed. (size=%u)\n", addr_info->size);
        return -ENOMEM;
    }

    src_info->size = addr_info->size;
    src_info->src_pages = tmp_pages;
    return 0;
}

static void trs_check_free_src_security_dma_addr(u32 devid, struct trs_security_src_info *src_info)
{
    ka_device_t *dev = NULL;

    /* The src always not secure in current scene in david, secure src dma map not implemented yet */
    if (src_info->is_src_secure == true) {
        return;
    }

    dev = hal_kernel_devdrv_get_pci_dev_by_devid(devid);
    if (dev != NULL) {
        hal_kernel_devdrv_dma_unmap_page(dev, src_info->dma_addr, src_info->size, KA_DMA_BIDIRECTIONAL);
    }

    if (src_info->src_pages != NULL) {
        __ka_mm_free_pages(src_info->src_pages, ka_mm_get_order(src_info->size));
    }
}

/* sqid sqeid --key */
int trs_sqe_update_desc_create(u32 devid, u32 tsid, struct trs_dma_desc_addr_info *addr_info,
                               struct trs_dma_desc *dma_desc, bool is_src_secure)
{
    struct devdrv_dma_prepare *dma_prepare = NULL;
    struct devdrv_dma_node dma_node;
    struct trs_chan_sq_info sq_info;
    u32 dma_node_num = 1;
    struct trs_id_inst inst;
    u32 sq_mem_side;
    u64 sdma_dst_addr;
    int ret;

    if (trs_is_rb_node_limited(devid)) {
        return -EMFILE;
    }

    trs_id_inst_pack(&inst, devid, tsid);
    ret = trs_get_res_info(&inst, TRS_HW_SQ, addr_info->sqid, (void *)&sq_info);
    if (ret != 0) {
        trs_err("Get res addr failed. (devid=%u; tsid=%u; sqid=%u; ret=%d)\n", devid, tsid, addr_info->sqid, ret);
        return ret;
    }

    if (addr_info->sqeid >= sq_info.sq_para.sq_depth) {
        trs_err("Addr info invalid. (devid=%u; tsid=%u; sqeid=%u; depth=%u)\n", devid, tsid, addr_info->sqeid,
                sq_info.sq_para.sq_depth);
        return -EINVAL;
    }

    sq_mem_side = trs_chan_get_sqcq_mem_side(sq_info.mem_type);
    if (sq_mem_side == TRS_CHAN_DEV_RSV_MEM) {
        if (sq_info.sq_dev_vaddr == NULL) {
            trs_err("sq mapped device vaddr is NULL. (devid=%u; tsid=%u; sqid=%u)\n", devid, tsid, addr_info->sqid);
            return -EFAULT;
        }
        sdma_dst_addr = (u64)sq_info.sq_dev_vaddr + addr_info->sqeid * sq_info.sq_para.sqe_size + addr_info->offset;
        dma_desc->dma_type = TRS_DMA_TYPE_SDMA;
        dma_desc->sdma_desc.dst_addr = (void *)(uintptr_t)sdma_dst_addr;
        ret = trs_dma_desc_node_create(devid, addr_info, NULL, dma_desc, NULL);
        if (ret != 0) {
            return ret;
        }
    } else {
        struct trs_security_src_info src_info = {0};
        ret = trs_check_alloc_src_security_dma_addr(devid, addr_info, &sq_info, is_src_secure, &src_info);
        if (ret != 0) {
            return ret;
        }

        trs_dma_node_pack(addr_info, &src_info, &sq_info, sq_mem_side, &dma_node);
        dma_prepare = devdrv_dma_link_prepare(devid, DEVDRV_DMA_DATA_TRAFFIC, &dma_node, dma_node_num,
                                              DEVDRV_DMA_DESC_FILL_FINISH);
        if (dma_prepare == NULL) {
            trs_check_free_src_security_dma_addr(devid, &src_info);
            trs_err("Dma_link_prepare alloc failed. (devid=%u)\n", devid);
            return -ENOMEM;
        }
        dma_desc->dma_type = TRS_DMA_TYPE_PCIEDMA;
        dma_desc->pciedma_desc.sq_addr = (void *)(uintptr_t)dma_prepare->sq_dma_addr;
        dma_desc->pciedma_desc.sq_tail = dma_node_num;

        ret = trs_dma_desc_node_create(devid, addr_info, dma_prepare, dma_desc, &src_info);
        if (ret != 0) {
            (void)devdrv_dma_link_free(dma_prepare);
            trs_check_free_src_security_dma_addr(devid, &src_info);
            return ret;
        }
    }
    trs_debug("Dma desc create. (devid=%u; sqid=%u; sqeid=%u; ssid=%u; offset=%u; size=%u; sq_mem_side=%u)\n", devid,
              addr_info->sqid, addr_info->sqeid, addr_info->passid, addr_info->offset, addr_info->size, sq_mem_side);
    return ret;
}

/* for chips before cloudv4 */
int hal_kernel_sqe_update_desc_create(u32 devid, u32 tsid, struct trs_dma_desc_addr_info *addr_info,
                                      struct trs_dma_desc *dma_desc)
{
    u64 rb_handle = 0;
    int ret = 0;

    rb_handle = (((u64)addr_info->sqid << 32) | (u64)addr_info->sqeid);
    ret = trs_dma_desc_node_find(devid, addr_info->sqid, rb_handle, dma_desc);
    if (ret == 0) {
        return 0;
    }

    return trs_sqe_update_desc_create(devid, tsid, addr_info, dma_desc, true);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_sqe_update_desc_create);

static struct trs_dma_desc_node *trs_dma_desc_node_del_one_by_sqid(u32 devid, u32 sqid)
{
    struct trs_rb_info *rb_info = NULL;
    struct trs_dma_desc_node *pos = NULL;
    struct trs_dma_desc_node *tmp = NULL;
    u32 root_index = 0;

    rb_info = trs_rb_info_get(devid);
    if (rb_info == NULL) {
        trs_err("Invalid rb info. (devid=%u; sqid=%u)\n", devid, sqid);
        return NULL;
    }

    trs_rb_get_root_index(sqid, &root_index);
    ka_task_spin_lock_bh(&rb_info->spinlock);
    ka_base_rbtree_postorder_for_each_entry_safe(pos, tmp, &rb_info->root[root_index], desc_node)
    {
        if (pos->sqid == sqid) {
            (void)trs_rb_erase(&rb_info->root[root_index], &pos->desc_node);
            rb_info->rb_cnt--;
            ka_task_spin_unlock_bh(&rb_info->spinlock);
            trs_rb_info_put(rb_info);
            return pos;
        }
    }
    ka_task_spin_unlock_bh(&rb_info->spinlock);
    trs_rb_info_put(rb_info);

    return NULL;
}

void hal_kernel_sqe_update_desc_destroy(u32 devid, u32 tsid, u32 sqid)
{
    struct trs_dma_desc_node *node = NULL;
    u32 num = 0;

    while (1) {
        node = trs_dma_desc_node_del_one_by_sqid(devid, sqid);
        if (node == NULL) {
            break;
        }

        trs_debug("Dma desc destroy. (devid=%u; sqid=%u; sqeid=%u)\n", devid, node->sqid, node->sqeid);
        if (node->dma_desc.dma_type == TRS_DMA_TYPE_PCIEDMA) {
            (void)devdrv_dma_link_free(node->dma_prepare);
        }

        trs_check_free_src_security_dma_addr(devid, &node->src_info);
        trs_vfree(node);

        ka_task_cond_resched();
        num++;
    }

    trs_debug("Destroy dma_desc info. (num=%u)\n", num);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_sqe_update_desc_destroy);

static struct trs_dma_desc_node *trs_dma_desc_node_del_one_from_root(struct trs_rb_info *rb_info, u32 root_index)
{
    struct trs_dma_desc_node *pos = NULL;
    struct trs_dma_desc_node *tmp = NULL;

    ka_task_spin_lock_bh(&rb_info->spinlock);
    ka_base_rbtree_postorder_for_each_entry_safe(pos, tmp, &rb_info->root[root_index], desc_node)
    {
        if (pos != NULL) {
            (void)trs_rb_erase(&rb_info->root[root_index], &pos->desc_node);
            rb_info->rb_cnt--;
            ka_task_spin_unlock_bh(&rb_info->spinlock);
            return pos;
        }
    }
    ka_task_spin_unlock_bh(&rb_info->spinlock);

    return NULL;
}

static void trs_sqe_update_desc_destroy_all(struct trs_rb_info *rb_info)
{
    struct trs_dma_desc_node *node = NULL;
    u32 cnt = rb_info->rb_cnt;
    u32 root_index;
    u32 num = 0;

    for (root_index = 0; root_index < TRS_DMA_NODE_RB_ROOT_NUM; root_index++) {
        if (trs_rb_root_is_empty(&rb_info->root[root_index]) == true) {
            continue;
        }

        while (1) {
            node = trs_dma_desc_node_del_one_from_root(rb_info, root_index);
            if (node == NULL) {
                break;
            }

            if (node->dma_desc.dma_type == TRS_DMA_TYPE_PCIEDMA) {
                (void)devdrv_dma_link_free(node->dma_prepare);
            }

            trs_check_free_src_security_dma_addr(rb_info->udevid, &node->src_info);
            trs_vfree(node);
            ka_task_cond_resched();
            num++;
        }

        if (rb_info->rb_cnt == 0) {
            break;
        }
    }

    trs_debug("Destroy dma_desc info. (need_free_cnt=%u; recycle_free_num=%u)\n", cnt, num);
}

int trs_sqe_update_init(u32 devid)
{
    struct trs_rb_info *rb_info = NULL;
    u32 root_index;

    rb_info = trs_vzalloc(sizeof(struct trs_rb_info));
    if (rb_info == NULL) {
        trs_err("Rb info mem alloc failed. (devid=%u)\n", devid);
        return -ENOMEM;
    }

    for (root_index = 0; root_index < TRS_DMA_NODE_RB_ROOT_NUM; root_index++) {
        rb_info->root[root_index] = KA_RB_ROOT;
    }

    rb_info->udevid = devid;
    rb_info->rb_cnt = 0;
    ka_task_spin_lock_init(&rb_info->spinlock);
    kref_safe_init(&rb_info->ref);
    trs_dma_desc_rb[devid] = rb_info;

    trs_info("Sqe update info init. (devid=%u)\n", devid);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_sqe_update_init);

void trs_sqe_update_uninit(u32 devid)
{
    struct trs_rb_info *rb_info = trs_dma_desc_rb[devid];

    if (rb_info == NULL) {
        trs_warn("Rb info is null. (devid=%u)\n", devid);
        return;
    }

    ka_task_write_lock_bh(&trs_rb_desc_lock);
    trs_dma_desc_rb[devid] = NULL;
    ka_task_write_unlock_bh(&trs_rb_desc_lock);

    trs_info("Sqe update info uninit. (devid=%u)\n", devid);
    trs_rb_info_put(rb_info);

    return;
}
KA_EXPORT_SYMBOL_GPL(trs_sqe_update_uninit);
