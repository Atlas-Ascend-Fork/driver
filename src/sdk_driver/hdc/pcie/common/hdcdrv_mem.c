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

#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif
#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif

#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"
#include "ka_compiler_pub.h"
#include "hdcdrv_core.h"
#include "hdcdrv_mem.h"

ka_delayed_work_t *hdcdrv_get_recycle_mem(void)
{
    return &(hdc_ctrl->recycle_mem);
}

ka_device_t *hdcdrv_get_pdev_dev(int dev_id)
{
    if (hdc_ctrl->devices[dev_id].valid != HDCDRV_VALID) {
        hdcdrv_err("Device is not ready. (dev_id=%d)\n", dev_id);
        return NULL;
    }
    return hdc_ctrl->devices[dev_id].dev;
}

/* 1 fast alloc/free/map        -> uni -> ctrl.lo
 *  2 add/del                    -> sep -> ctrl.dev[id].lo/re
 *  3 send/rx src & pm(fid == 0) -> uni -> ctrl.lo
 *  4 send/rx/dma-dst            -> sep -> ctrl.dev[id].lo/re
 */
struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_uni(void)
{
    return &(hdc_ctrl->fmem);
}

struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_sep(int devid)
{
    return &(hdc_ctrl->devices[devid].fmem);
}

void hdcdrv_iova_fmem_unmap(u32 dev_id, u32 fid, struct hdcdrv_fast_mem *f_mem, u32 num)
{
#ifdef CFG_FEATURE_VFIO
    u32 i;

    for (i = 0; i < num; i++) {
        if (f_mem->mem[i].dma_sgt != NULL) {
            vmngh_dma_unmap_guest_page(dev_id, fid, f_mem->mem[i].dma_sgt);
        }
    }
#endif
}

#ifdef CFG_FEATURE_VFIO
static void hdcdrv_iova_new_mem(u32 dev_id, u32 fid, u32 old_num, struct hdcdrv_mem_f old_mem[],
                                struct hdcdrv_mem_f new_mem[])
{
    u32 i, j;
    u32 num_new = 0;
    ka_scatterlist_t *sgl = NULL;
    u32 len_per_sgt;

    for (i = 0; i < old_num; i++) {
        sgl = old_mem[i].dma_sgt->sgl;
        len_per_sgt = 0;

        for (j = 0; j < old_mem[i].dma_sgt->nents; j++) {
            if (sgl == NULL) {
                break;
            }
            /*lint -e679 */
            /* Adding offset in PAGE, in case of PM PAGE_SIZE is larger than VM */
            new_mem[num_new + j].addr = ka_mm_sg_dma_address(sgl) + (old_mem[i].addr & (KA_MM_PAGE_SIZE - 1));
            new_mem[num_new + j].len = ka_mm_sg_dma_len(sgl);
            new_mem[num_new + j].dma_sgt = (j == 0) ? (old_mem[i].dma_sgt) : (NULL);
            len_per_sgt = len_per_sgt + new_mem[num_new + j].len;
            /*lint +e679 */
            sgl = ka_base_sg_next(sgl);
        }

        num_new = num_new + old_mem[i].dma_sgt->nents;

        if (len_per_sgt != old_mem[i].len) {
            hdcdrv_warn("sgt_len not match. (dev_id=%u; fid=%u; mem=%u; len=%u; sgt=%u)\n", dev_id, fid, i,
                        old_mem[i].len, len_per_sgt);
        }
    }
}
#endif

int hdcdrv_iova_fmem_map(u32 dev_id, u32 fid, struct hdcdrv_fast_mem *f_mem)
{
#ifdef CFG_FEATURE_VFIO
    ka_dma_addr_t dma_addr;
    u32 i, unmap_num;
    int num_new;
    struct hdcdrv_mem_f *new_mem = NULL;
    u32 new_mem_size;
    ka_sg_table_t *dma_sgt = NULL;

    num_new = 0;
    for (i = 0; i < (u32)f_mem->phy_addr_num; i++) {
        dma_addr = vmngh_dma_map_guest_page(dev_id, fid, f_mem->mem[i].addr, f_mem->mem[i].len, &dma_sgt);
        if ((dma_sgt == NULL) || (dma_addr == DMA_MAP_ERROR)) {
            hdcdrv_err("Calling vmngh_dma_map_guest_page failed. (dev=%u; fid=%u)\n", dev_id, fid);
            unmap_num = i;
            goto map_fail;
        }

        if ((num_new + dma_sgt->nents) > HDCDRV_MEM_MAX_PHY_NUM) {
            hdcdrv_err("New physical page number too big. (dev=%u; fid=%u)\n", dev_id, fid);

            unmap_num = i;
            goto map_fail;
        }

        num_new = num_new + (int)dma_sgt->nents;
        f_mem->mem[i].dma_sgt = dma_sgt;
    }

    new_mem_size = (u32)num_new * sizeof(struct hdcdrv_mem_f);
    new_mem = hdcdrv_kvmalloc(new_mem_size, KA_SUB_MODULE_TYPE_2);
    if (new_mem == NULL) {
        unmap_num = (u32)f_mem->phy_addr_num;
        hdcdrv_err("Calling alloc failed. (dev=%u; fid=%u)\n", dev_id, fid);
        goto map_fail;
    }

    hdcdrv_iova_new_mem(dev_id, fid, (u32)f_mem->phy_addr_num, f_mem->mem, new_mem);

    hdcdrv_kvfree((void **)&f_mem->mem, KA_SUB_MODULE_TYPE_2);
    f_mem->mem = new_mem;
    f_mem->phy_addr_num = num_new;
    f_mem->dma_map = 1;

    return HDCDRV_OK;

map_fail:
    hdcdrv_iova_fmem_unmap(dev_id, fid, f_mem, unmap_num);
    return HDCDRV_ERR;
#else
    return HDCDRV_OK;
#endif
}

STATIC struct hdcdrv_mem_pool *get_pool(struct hdcdrv_dev *dev, int pool_type, u32 data_len)
{
    struct hdcdrv_mem_pool *pool = NULL;
    if (data_len <= (u32)(HDCDRV_SMALL_PACKET_SEGMENT - HDCDRV_MEM_BLOCK_HEAD_SIZE)) {
        pool = &dev->small_mem_pool[pool_type];
    } else {
        pool = &dev->huge_mem_pool[pool_type];
    }
    return pool;
}

STATIC int alloc_mem_later(struct hdcdrv_mem_pool *pool, ka_list_head_t *wait_head)
{
    int ret = HDCDRV_OK;

    ka_task_spin_lock_bh(&pool->mem_lock);
    if (pool->head == pool->tail) {
        if (wait_head != NULL) {
            if (wait_head->next == NULL) {
                ka_list_add_tail(wait_head, &pool->wait_list);
            }
        }
#ifdef CFG_FEATURE_HDC_REG_MEM
        hdcdrv_limit_exclusive(warn, HDCDRV_LIMIT_LOG_0x10B, "mem pool used up.\n");
#endif
        ret = HDCDRV_DMA_MEM_ALLOC_FAIL;
    }
    ka_task_spin_unlock_bh(&pool->mem_lock);

    return ret;
}
struct hdcdrv_mem_pool *find_mem_pool(int pool_type, int dev_id, int len)
{
    struct hdcdrv_dev *dev = NULL;

    if (ka_unlikely((len > hdcdrv_mem_block_capacity()) || (len <= 0))) {
        hdcdrv_err_limit("data len invalid. (len=%d, capacity=%d)\n", len, hdcdrv_mem_block_capacity());
        return NULL;
    }
    dev = &hdc_ctrl->devices[dev_id];
    if (dev->valid != HDCDRV_VALID) {
        hdcdrv_err("Input parameter is error. (dev_id=%d)\n", dev_id);
        return NULL;
    }
    return get_pool(dev, pool_type, (u32)len);
}

int alloc_mem(struct hdcdrv_mem_pool *pool, void **buf, ka_dma_addr_t *addr, u32 *offset, ka_list_head_t *wait_head)
{
    int ret;

    if (pool == NULL) {
        hdcdrv_err("pool is invalid.\n");
        return HDCDRV_ERR;
    }
    ret = alloc_mem_later(pool, wait_head);
    if (ret != HDCDRV_OK) {
        return ret;
    }
    ret = hdccom_alloc_mem(pool, buf, addr, offset);
    if (ret != HDCDRV_OK) {
        if (ret != HDCDRV_DMA_MEM_ALLOC_FAIL) {
            hdcdrv_err("Alloc memory failed. (ret=%d)\n", ret);
        }
        return ret;
    }

    return HDCDRV_OK;
}

STATIC void hdcdrv_mem_avail(int pool_type, ka_list_head_t *target)
{
    struct hdcdrv_msg_chan *msg_chan = NULL;

    if (pool_type == HDCDRV_MEM_POOL_TYPE_RX) {
        msg_chan = ka_list_entry(target, struct hdcdrv_msg_chan, wait_mem_list);
        msg_chan->dbg_stat.hdcdrv_mem_avail1++;
        hdcdrv_rx_msg_schedule_task(msg_chan);
    }
}

STATIC void free_mem_notify(struct hdcdrv_mem_pool *pool, u32 type)
{
    ka_list_head_t *list = NULL;

    ka_task_spin_lock_bh(&pool->mem_lock);
    if (!ka_list_empty_careful(&pool->wait_list)) {
        list = pool->wait_list.next;
        ka_list_del(list);
        list->next = NULL;
        list->prev = NULL;
        hdcdrv_mem_avail((int)type, list);
    }
    ka_task_spin_unlock_bh(&pool->mem_lock);
}

void free_mem(void *buf)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    struct hdcdrv_mem_pool *pool = NULL;
    int ret;

    if (hdcdrv_mem_block_head_check(buf) != HDCDRV_OK) {
        hdcdrv_err_spinlock("Block head check failed.\n");
        return;
    }

    block_head = (struct hdcdrv_mem_block_head *)buf;

    pool = get_pool(&hdc_ctrl->devices[block_head->devid], (int)block_head->type, block_head->size);
    if (pool->ring == NULL) {
        hdcdrv_warn_spinlock("Pool ring has freed.\n");
        return;
    }

    ret = hdccom_free_mem(pool, buf);
    if (ret != HDCDRV_OK) {
        hdcdrv_err_spinlock("Calling free_mem failed. (pool_type=%d; device=%d; ret=%d)\n", block_head->type,
                            block_head->devid, ret);
        return;
    }

    free_mem_notify(pool, block_head->type);
    return;
}

int hdcdrv_map_reserve_mem(struct hdccom_mem_init *init_mem, int pool_type, u32 segment, u32 num)
{
    int ret = 0;

    ret = devdrv_get_reserve_mem_info(init_mem->dev_id, devdrv_msg_client_hdc, &init_mem->reserve_mem_pa,
                                      &init_mem->reserve_mem_size);
    if (ret != 0) {
        hdcdrv_err("devdrv_get_reserve_mem_info failed. (pool_type=%d, devid=%d)\n", pool_type, init_mem->dev_id);
        return HDCDRV_ERR;
    }

    if (pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) {
        init_mem->reserve_mem_size /= HDCDRV_RESERVE_MEM_POOL_TYPE_NUM;
    } else if (pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX) {
        init_mem->reserve_mem_pa += init_mem->reserve_mem_size / HDCDRV_RESERVE_MEM_POOL_TYPE_NUM;
        init_mem->reserve_mem_size /= HDCDRV_RESERVE_MEM_POOL_TYPE_NUM;
    }
    if (init_mem->reserve_mem_size < (segment * num)) {
        hdcdrv_err("Init reserve mem failed. (pool_type=%d; segment=%u; num=%u; size=0x%lx)\n", pool_type, segment, num,
                   init_mem->reserve_mem_size);
        return HDCDRV_ERR;
    }

    init_mem->reserve_mem_va = ka_mm_ioremap_cache(init_mem->reserve_mem_pa, init_mem->reserve_mem_size);
    if (init_mem->reserve_mem_va == NULL) {
        hdcdrv_err("Calling ka_mm_ioremap failed. (pool_type=%d)\n", pool_type);
        return HDCDRV_ERR;
    }

    init_mem->reserve_mem_dma_addr = devdrv_dma_map_resource(init_mem->dev, init_mem->reserve_mem_pa, segment * num,
                                                             KA_DMA_BIDIRECTIONAL, 0);
    if (ka_mm_dma_mapping_error(init_mem->dev, init_mem->reserve_mem_dma_addr) != 0) {
        hdcdrv_err("devdrv_dma_map_resource failed. (dev_id=%d; segment=%u)\n", init_mem->dev_id, segment);
        return HDCDRV_ERR;
    }
    return HDCDRV_OK;
}

void hdcdrv_unmap_reserve_mem(struct hdcdrv_mem_pool *pool, ka_device_t *dev)
{
    if (pool->reserve_mem_dma_addr != (~(ka_dma_addr_t)0)) {
        devdrv_dma_unmap_resource(dev, pool->reserve_mem_dma_addr, pool->reserve_mem_size, KA_DMA_BIDIRECTIONAL, 0);
        pool->reserve_mem_dma_addr = ~(ka_dma_addr_t)0;
    }

    if (pool->reserve_mem_va != NULL) {
        ka_mm_iounmap(pool->reserve_mem_va);
        pool->reserve_mem_va = NULL;
    }
}

int alloc_mem_pool(int pool_type, int dev_id, u32 segment, u32 num)
{
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[dev_id];
    struct hdcdrv_mem_pool *pool;
    struct hdccom_mem_init init_mem = {0};
    int ret;

    if (num == 0) {
        hdcdrv_warn("hdccom_init_mem_pool num is 0. (dev_id=%d; pool_type=%d)\n", dev_id, pool_type);
        return HDCDRV_OK;
    }
    pool = get_pool(hdc_dev, pool_type, segment);

    init_mem.dev = hdc_dev->dev;
    init_mem.pool_type = pool_type;
    init_mem.dev_id = dev_id;
    init_mem.segment = segment;
    init_mem.num = num;
    init_mem.reserve_mem_dma_addr = ~(ka_dma_addr_t)0;
    if ((init_mem.pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) ||
        (init_mem.pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX)) {
        ret = hdcdrv_map_reserve_mem(&init_mem, pool_type, segment, num);
        /* get va and dma_addr for release resource when free_mem_pool */
        pool->reserve_mem_va = init_mem.reserve_mem_va;
        pool->reserve_mem_dma_addr = init_mem.reserve_mem_dma_addr;
        pool->reserve_mem_size = init_mem.reserve_mem_size;
        if (ret != HDCDRV_OK) {
            hdcdrv_err("Calling hdcdrv_reserve_mem_init failed. (dev_id=%d; pool_type=%d)\n", dev_id, pool_type);
            return ret;
        }
    }

    ret = hdccom_init_mem_pool(pool, &init_mem);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdccom_init_mem_pool failed. (dev_id=%d; pool_type=%d)\n", dev_id, pool_type);
        return ret;
    }
    return HDCDRV_OK;
}

void free_mem_pool(int pool_type, int dev_id, u32 segment)
{
    struct hdcdrv_dev *hdc_dev = &hdc_ctrl->devices[dev_id];
    struct hdcdrv_mem_pool *pool = NULL;
    int ret;

    pool = get_pool(hdc_dev, pool_type, segment);

    ret = hdccom_free_mem_pool(pool, hdc_dev->dev, segment);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdccom_free_mem_pool failed. (dev_id=%d; pool_type=%d; ret=%d)\n", dev_id, pool_type, ret);
    }

    if ((pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) || (pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX)) {
        hdcdrv_unmap_reserve_mem(pool, hdc_dev->dev);
    }

    return;
}

int hdcdrv_mem_block_capacity(void)
{
#ifdef CFG_FEATURE_HDC_REG_MEM
    return HDCDRV_SMALL_PACKET_SEGMENT - HDCDRV_MEM_BLOCK_HEAD_SIZE;
#else
    return hdc_ctrl->segment - HDCDRV_MEM_BLOCK_HEAD_SIZE;
#endif
}

int hdcdrv_init_alloc_mem_pool(u32 dev_id, u32 small_packet_num, u32 huge_packet_num)
{
    int ret;

    ret = alloc_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT, small_packet_num);
    if (ret != HDCDRV_OK) {
        goto free_small_tx;
    }
    hdcdrv_set_time_stamp(&(hdcdrv_get_init_stamp_info()->alloc_mem_pool0));
    ret = alloc_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT, small_packet_num);
    if (ret != HDCDRV_OK) {
        goto free_small_rx;
    }
    hdcdrv_set_time_stamp(&(hdcdrv_get_init_stamp_info()->alloc_mem_pool1));
    ret = alloc_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, (u32)hdc_ctrl->segment, huge_packet_num);
    if (ret != HDCDRV_OK) {
        goto free_huge_tx;
    }
    hdcdrv_set_time_stamp(&(hdcdrv_get_init_stamp_info()->alloc_mem_pool2));
    ret = alloc_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, (u32)hdc_ctrl->segment, huge_packet_num);
    if (ret != HDCDRV_OK) {
        goto free_huge_rx;
    }
    hdcdrv_set_time_stamp(&(hdcdrv_get_init_stamp_info()->alloc_mem_pool3));
    return HDCDRV_OK;

free_huge_rx:
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, (u32)hdc_ctrl->segment);
free_huge_tx:
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, (u32)hdc_ctrl->segment);
free_small_rx:
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);
free_small_tx:
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);

    return HDCDRV_ERR;
}

void hdcdrv_uninit_alloc_mem_pool(u32 dev_id)
{
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, (u32)hdc_ctrl->segment);
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, (u32)hdc_ctrl->segment);
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_RX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);
    free_mem_pool(HDCDRV_MEM_POOL_TYPE_TX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);
}

int hdcdrv_init_reserve_mem_pool(u32 dev_id, u32 small_packet_num, u32 huge_packet_num)
{
    int ret;

    ret = alloc_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_TX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT, small_packet_num);
    if (ret != HDCDRV_OK) {
        goto free_reserve_tx;
    }
    hdcdrv_set_time_stamp(&(hdcdrv_get_init_stamp_info()->alloc_mem_pool0));
    ret = alloc_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_RX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT, small_packet_num);
    if (ret != HDCDRV_OK) {
        goto free_reserve_rx;
    }
    hdcdrv_set_time_stamp(&(hdcdrv_get_init_stamp_info()->alloc_mem_pool1));
    return HDCDRV_OK;

free_reserve_rx:
    free_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_RX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);
free_reserve_tx:
    free_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_TX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);
    return HDCDRV_ERR;
}

void hdcdrv_uninit_reserve_mem_pool(u32 dev_id)
{
    free_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_RX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);
    free_mem_pool(HDCDRV_RESERVE_MEM_POOL_TYPE_TX, (int)dev_id, HDCDRV_SMALL_PACKET_SEGMENT);
}

int hdcdrv_init_mem_pool(u32 dev_id)
{
    int ret;
    u32 small_packet_num = HDCDRV_SMALL_PACKET_NUM;
    u32 huge_packet_num = HDCDRV_HUGE_PACKET_NUM;

    /* only uninit free, suspend status not free */
    if (hdcdrv_get_running_status() != HDCDRV_RUNNING_NORMAL) {
        return HDCDRV_OK;
    }
    hdcdrv_get_mempool_size(&small_packet_num, &huge_packet_num);

#ifdef CFG_FEATURE_RESERVE_MEM_POOL
    ret = hdcdrv_init_reserve_mem_pool(dev_id, small_packet_num, huge_packet_num);
#else
    ret = hdcdrv_init_alloc_mem_pool(dev_id, small_packet_num, huge_packet_num);
#endif
    if (ret != HDCDRV_OK) {
        hdcdrv_err("hdcdrv_init_mem_pool failed.\n");
        return HDCDRV_ERR;
    }

    hdcdrv_info("hdcdrv_init_mem_pool success.\n");
    return HDCDRV_OK;
}

void hdcdrv_uninit_mem_pool(u32 dev_id)
{
    /* only uninit free, suspend status not free */
    if (hdcdrv_get_running_status() != HDCDRV_RUNNING_NORMAL) {
        return;
    }

#ifdef CFG_FEATURE_RESERVE_MEM_POOL
    hdcdrv_uninit_reserve_mem_pool(dev_id);
#else
    hdcdrv_uninit_alloc_mem_pool(dev_id);
#endif
    hdcdrv_info("hdcdrv_uninit_mem_pool success.\n");
}

int hdcdrv_list_node_mem_alloc(struct hdcdrv_ctx *ctx, struct hdcdrv_mem_fd_list **new_node)
{
    if ((ctx != NULL) && (ctx != HDCDRV_KERNEL_WITHOUT_CTX)) {
        *new_node = (struct hdcdrv_mem_fd_list *)hdcdrv_kzalloc(sizeof(struct hdcdrv_mem_fd_list),
                                                                KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_SUB_MODULE_TYPE_1);
        if (*new_node == NULL) {
            hdcdrv_err("Calling ka_mm_kzalloc failed.\n");
            return HDCDRV_MEM_ALLOC_FAIL;
        }
    }

    return HDCDRV_OK;
}

void hdcdrv_list_node_mem_free(void *ctx, struct hdcdrv_mem_fd_list *new_node)
{
    if ((ctx != NULL) && ((struct hdcdrv_ctx *)ctx != HDCDRV_KERNEL_WITHOUT_CTX)) {
        hdcdrv_kfree(new_node, KA_SUB_MODULE_TYPE_1);
    }
}

long hdcdrv_fast_alloc_mem(struct hdcdrv_ctx *ctx, struct hdcdrv_cmd_alloc_mem *cmd)
{
    long ret;
    struct hdcdrv_fast_node *f_node = NULL;
    struct hdcdrv_mem_fd_list *new_node = NULL;

    ret = hdcdrv_list_node_mem_alloc(ctx, &new_node);
    if (ret != HDCDRV_OK) {
        return ret;
    }

    ret = hdccom_fast_alloc_mem((void *)ctx, cmd, &f_node);
    if ((ret != HDCDRV_OK) || (f_node == NULL)) {
        if (ret == HDCDRV_VA_UNMAP_FAILED) {
            hdcdrv_bind_mem_ctx(&ctx->abnormal_ctx_fmem, f_node, new_node);
        } else {
            hdcdrv_list_node_mem_free((void *)ctx, new_node);
            new_node = NULL;
        }
        hdcdrv_err("Calling alloc memory error. (dev=%d)\n", cmd->dev_id);
        return ret;
    }

    if ((ctx != NULL) && (ctx != HDCDRV_KERNEL_WITHOUT_CTX)) {
        hdcdrv_bind_mem_ctx(&ctx->ctx_fmem, f_node, new_node);
    }

    f_node->ctx = (void *)ctx;

    return HDCDRV_OK;
}

bool hdcdrv_mem_is_notify(const struct hdcdrv_fast_mem *f_mem)
{
    if ((f_mem->mem_type == HDCDRV_FAST_MEM_TYPE_RX_DATA) || (f_mem->mem_type == HDCDRV_FAST_MEM_TYPE_RX_CTRL)) {
        return false;
    }

    return true;
}
