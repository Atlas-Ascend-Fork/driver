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

#include "ka_system_pub.h"
#include "ka_compiler_pub.h"
#include "ka_driver_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "pbl_ka_memory.h"
#include "hdcdrv_core_com.h"
#include "comm_kernel_interface.h"
#include "kernel_version_adapt.h"
#include "hdcdrv_mem_com.h"

STATIC struct hdcdrv_init_stamp g_hdcdrv_init_stamp = {0};
STATIC struct hdcdrv_alloc_pool_stamp g_alloc_pool_stamp = {0};
#define HDCDRV_HUGETLB_FOLIO_SIZE (2 * 1024 * 1024UL)
#define HDCDRV_HUGEPAGE_2M_ORDER 9

#define HDC_RESERVE_MEM_NUMA 31
void hdcdrv_init_stamp_init(void)
{
    g_hdcdrv_init_stamp.wait_mutex_start = 0;
    g_hdcdrv_init_stamp.wait_mutex_end = 0;
    g_hdcdrv_init_stamp.chan_alloc_start = 0;
    g_hdcdrv_init_stamp.chan_alloc_end = 0;
    g_hdcdrv_init_stamp.chan_memset_end = 0;
    g_hdcdrv_init_stamp.init_pool_start = 0;
    g_hdcdrv_init_stamp.alloc_mem_pool0 = 0;
    g_hdcdrv_init_stamp.alloc_mem_pool1 = 0;
    g_hdcdrv_init_stamp.alloc_mem_pool2 = 0;
    g_hdcdrv_init_stamp.alloc_mem_pool3 = 0;
    g_hdcdrv_init_stamp.init_pool_end = 0;
    g_hdcdrv_init_stamp.tasklet_start = 0;
    g_hdcdrv_init_stamp.tasklet_end = 0;
    g_hdcdrv_init_stamp.end = 0;
}
void hdcdrv_alloc_pool_stamp_init(void)
{
    g_alloc_pool_stamp.ring_alloc_start = 0;
    g_alloc_pool_stamp.ring_alloc_end = 0;
    g_alloc_pool_stamp.list_init_end = 0;
    g_alloc_pool_stamp.dma_alloc_start = 0;
    g_alloc_pool_stamp.dma_alloc_end = 0;
    g_alloc_pool_stamp.head_alloc_end = 0;
    g_alloc_pool_stamp.dma_alloc_max = 0;
    g_alloc_pool_stamp.head_alloc_max = 0;
    g_alloc_pool_stamp.dma_alloc_total = 0;
    g_alloc_pool_stamp.head_alloc_total = 0;
    g_alloc_pool_stamp.end = 0;
}
void hdcdrv_set_time_stamp(u64 *stamp)
{
    *stamp = ka_jiffies;
}
struct hdcdrv_init_stamp *hdcdrv_get_init_stamp_info(void)
{
    return &g_hdcdrv_init_stamp;
}
STATIC void hdcdrv_calc_alloc_pool_stamp(void)
{
    u64 dma_alloc_diff = g_alloc_pool_stamp.dma_alloc_end - g_alloc_pool_stamp.dma_alloc_start;
    u64 head_alloc_diff = g_alloc_pool_stamp.head_alloc_end - g_alloc_pool_stamp.dma_alloc_end;
    g_alloc_pool_stamp.dma_alloc_max = (g_alloc_pool_stamp.dma_alloc_max > dma_alloc_diff ?
                                            g_alloc_pool_stamp.dma_alloc_max :
                                            dma_alloc_diff);
    g_alloc_pool_stamp.head_alloc_max = (g_alloc_pool_stamp.head_alloc_max > head_alloc_diff ?
                                             g_alloc_pool_stamp.head_alloc_max :
                                             head_alloc_diff);
    g_alloc_pool_stamp.dma_alloc_total += dma_alloc_diff;
    g_alloc_pool_stamp.head_alloc_total += head_alloc_diff;
}
STATIC u32 hdcdrv_calc_time_cost(u64 end, u64 start)
{
    if (end <= start) {
        return 0;
    }
    return ka_system_jiffies_to_msecs(end - start);
}
STATIC void hdcdrv_alloc_pool_stamp_record(void)
{
    if (hdcdrv_calc_time_cost(g_alloc_pool_stamp.end, g_alloc_pool_stamp.ring_alloc_start) >
        HDCDRV_ALLOC_POOL_MAX_TIME) {
        hdcdrv_warn("alloc pool time. (total=%ums, alloc_ring=%ums, list_init=%ums, dma_total=%ums, "
                    "head_total=%ums, dma_max=%ums, head_max=%ums)\n",
                    hdcdrv_calc_time_cost(g_alloc_pool_stamp.end, g_alloc_pool_stamp.ring_alloc_start),
                    hdcdrv_calc_time_cost(g_alloc_pool_stamp.ring_alloc_end, g_alloc_pool_stamp.ring_alloc_start),
                    hdcdrv_calc_time_cost(g_alloc_pool_stamp.list_init_end, g_alloc_pool_stamp.ring_alloc_end),
                    ka_system_jiffies_to_msecs(g_alloc_pool_stamp.dma_alloc_total),
                    ka_system_jiffies_to_msecs(g_alloc_pool_stamp.head_alloc_total),
                    ka_system_jiffies_to_msecs(g_alloc_pool_stamp.dma_alloc_max),
                    ka_system_jiffies_to_msecs(g_alloc_pool_stamp.head_alloc_max));
    }
}
void hdcdrv_init_stamp_record()
{
    if (hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.end, g_hdcdrv_init_stamp.wait_mutex_start) >
        HDCDRV_INIT_TRANS_MAX_TIME) {
        hdcdrv_warn("init time. (total=%ums, wait_mutex=%ums, alloc_chan=%ums, memset=%ums, start_init_pool=%ums, "
                    "alloc_pool0=%ums ,alloc_pool1=%ums, alloc_pool2=%ums, alloc_pool3=%ums, total_alloc_pool=%ums, "
                    "tasklet_start=%ums, ka_system_tasklet_init=%ums, end=%ums)\n",
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.end, g_hdcdrv_init_stamp.wait_mutex_start),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.wait_mutex_end, g_hdcdrv_init_stamp.wait_mutex_start),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.chan_alloc_end, g_hdcdrv_init_stamp.chan_alloc_start),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.chan_memset_end, g_hdcdrv_init_stamp.chan_alloc_end),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.init_pool_start, g_hdcdrv_init_stamp.chan_memset_end),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.alloc_mem_pool0, g_hdcdrv_init_stamp.init_pool_start),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.alloc_mem_pool1, g_hdcdrv_init_stamp.alloc_mem_pool0),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.alloc_mem_pool2, g_hdcdrv_init_stamp.alloc_mem_pool1),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.alloc_mem_pool3, g_hdcdrv_init_stamp.alloc_mem_pool2),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.init_pool_end, g_hdcdrv_init_stamp.init_pool_start),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.tasklet_start, g_hdcdrv_init_stamp.init_pool_end),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.tasklet_end, g_hdcdrv_init_stamp.tasklet_start),
                    hdcdrv_calc_time_cost(g_hdcdrv_init_stamp.end, g_hdcdrv_init_stamp.tasklet_end));
    }
}

ka_page_t *hdcdrv_alloc_pages_node(u32 dev_id, ka_gfp_t gfp_mask, u32 order)
{
    return hdcdrv_alloc_pages_node_inner(dev_id, gfp_mask, order);
}

void *hdcdrv_kzalloc_mem_node(u32 dev_id, ka_gfp_t gfp_mask, u32 size, u32 level)
{
    return hdcdrv_kzalloc_mem_node_inner(dev_id, gfp_mask, size, level);
}

void *hdcdrv_kvmalloc(size_t size, int level)
{
    void *addr = NULL;

    if (size == 0) {
        return NULL;
    }

    addr = hdcdrv_kzalloc(size, KA_GFP_NOWAIT | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT | KA_GFP_KERNEL, level);
    if (addr == NULL) {
        addr = hdcdrv_vmalloc(size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT | __KA_GFP_ZERO, KA_PAGE_KERNEL, level);
        if (addr == NULL) {
            return NULL;
        }
    }

    return addr;
}

void hdcdrv_kvfree(void **addr, int level)
{
    if ((addr == NULL) || (*addr == NULL)) {
        return;
    }

    if (ka_mm_is_vmalloc_addr(*addr)) {
        hdcdrv_vfree(*addr, level);
    } else {
        hdcdrv_kfree(*addr, level);
    }
    *addr = NULL;
}

static inline u32 hdcdrv_mem_block_head_crc32(const struct hdcdrv_mem_block_head *block_head)
{
    return ka_base_crc32_le(~0u, (unsigned char *)block_head, HDCDRV_BLOCK_CRC_LEN);
}

STATIC void hdcdrv_mem_block_head_init(void *buf, ka_dma_addr_t addr, u32 offset, struct hdccom_mem_init *init_mem,
                                       u32 segment)
{
    struct hdcdrv_mem_block_head *block_head = (struct hdcdrv_mem_block_head *)buf;

    block_head->magic = HDCDRV_MEM_BLOCK_MAGIC;
    block_head->devid = (u32)init_mem->dev_id;
    block_head->type = (u32)init_mem->pool_type;
    block_head->size = segment;
    block_head->offset = offset;
    block_head->dma_addr = addr;
    block_head->head_crc = hdcdrv_mem_block_head_crc32(block_head);
    block_head->ref_count = 0;
}

STATIC void hdcdrv_mem_block_head_dump(struct hdcdrv_mem_block_head *block_head)
{
    hdcdrv_err_spinlock("Critical error, memory block head is corrupted.\n"
                        "(magic=%x; "
                        "devid=%x; "
                        "type=%x; "
                        "size=%x; "
                        "dma_addr=(no print); "
                        "head_crc=%x; "
                        "ref_count=%x; "
                        "current crc=%x)\n",
                        block_head->magic, block_head->devid, block_head->type, block_head->size, block_head->head_crc,
                        block_head->ref_count, hdcdrv_mem_block_head_crc32(block_head));
#ifdef CFG_BUILD_DEBUG
    ka_base_dump_stack();
#endif
}

int hdcdrv_mem_block_head_check(void *buf)
{
    struct hdcdrv_mem_block_head *block_head = NULL;

    if (buf == NULL) {
        hdcdrv_err_spinlock("Input parameter is error.\n");
        return HDCDRV_ERR;
    }

    block_head = HDCDRV_BLOCK_HEAD(buf);
    if (block_head == NULL) {
        hdcdrv_err_spinlock("Calling HDCDRV_BLOCK_HEAD failed.\n");
        return HDCDRV_ERR;
    }

    if ((block_head->magic != HDCDRV_MEM_BLOCK_MAGIC) ||
        (block_head->head_crc != hdcdrv_mem_block_head_crc32(block_head))) {
        hdcdrv_mem_block_head_dump(block_head);
        hdcdrv_err_spinlock("Memory block head check failed.\n");
        return HDCDRV_ERR;
    }

    return HDCDRV_OK;
}

#ifdef CFG_FEATURE_SMALL_HUGE_POOL
STATIC bool is_alloc_mirror_mem(u32 dev_id, u32 segment)
{
    u32 chip_type;

    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        return false;
    }

    chip_type = uda_get_chip_type(dev_id);
    /* from v4/v5, huge mem pool not use mirror mem */
    if ((chip_type >= HISI_CLOUD_V4) && (chip_type < HISI_CHIP_UNKNOWN) && (segment > HDCDRV_SMALL_PACKET_SEGMENT)) {
        return false;
    }

    return true;
}
#endif

void free_mem_pool_single(ka_device_t *dev, u32 segment, struct hdcdrv_mem_block_head *buf, ka_dma_addr_t addr)
{
    if ((buf == NULL) || (buf->dma_buf == NULL) || (addr == KA_DMA_MAPPING_ERROR) || (dev == NULL)) {
        return;
    }

    if ((buf->type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) || (buf->type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX)) {
        buf->dma_buf = NULL;
        addr = 0;
    } else {
#ifdef CFG_FEATURE_SMALL_HUGE_POOL
        if ((is_alloc_mirror_mem(buf->devid, segment) == false) && (buf->pool_page != NULL)) {
            hal_kernel_devdrv_dma_unmap_page(dev, addr, segment, KA_DMA_BIDIRECTIONAL);
            hdcdrv_free_pages_ex(buf->pool_page, ka_mm_get_order(segment), KA_SUB_MODULE_TYPE_2);
            buf->pool_page = NULL;
        } else {
            hal_kernel_devdrv_dma_free_coherent(dev, segment, buf->dma_buf, addr);
        }
#else
        hal_kernel_devdrv_dma_free_coherent(dev, segment, buf->dma_buf, addr);
#endif
    }
    buf->dma_buf = NULL;
    hdcdrv_kvfree((void **)&buf, KA_SUB_MODULE_TYPE_0);
}

static inline u32 hdccom_calc_ring_id(u64 ring_cnt, u32 mask, u32 size)
{
    if (size == 0) {
        hdcdrv_err_spinlock("Input parameter is error.\n");
        return 0;
    }

    return (mask != 0) ? (ring_cnt & mask) : (u32)(ring_cnt % size);
}

int hdccom_alloc_mem(struct hdcdrv_mem_pool *pool, void **buf, ka_dma_addr_t *addr, u32 *offset)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    u32 ring_id;

    ka_task_spin_lock_bh(&pool->mem_lock);

    if (pool->valid != HDCDRV_VALID) {
        ka_task_spin_unlock_bh(&pool->mem_lock);
        hdcdrv_err("pool is invalid. (devid=%u)\n", pool->dev_id);
        return HDCDRV_ERR;
    }

    if (pool->head == pool->tail) {
        ka_task_spin_unlock_bh(&pool->mem_lock);
        return HDCDRV_DMA_MEM_ALLOC_FAIL;
    }

    ring_id = hdccom_calc_ring_id(pool->head, pool->mask, pool->size);
    if (hdcdrv_mem_block_head_check(pool->ring[ring_id].buf) != HDCDRV_OK) {
        pool->head++;
        block_head = pool->ring[ring_id].buf;
        ka_task_spin_unlock_bh(&pool->mem_lock);
        if (block_head != NULL) {
            hdcdrv_err("Block head is corrupted. (magic=%u; devid=%u; type=%u; size=%u; head_crc=%u; current crc=%u)\n",
                       block_head->magic, block_head->devid, block_head->type, block_head->size, block_head->head_crc,
                       hdcdrv_mem_block_head_crc32(block_head));
        }
        hdcdrv_err("Block head check failed. (devid=%u; ring_id=%d)\n", pool->dev_id, ring_id);
        return HDCDRV_MEM_NOT_MATCH;
    }

    block_head = pool->ring[ring_id].buf;
    if (block_head->ref_count != 0) {
        ka_task_spin_unlock_bh(&pool->mem_lock);
        hdcdrv_mem_block_head_dump(block_head);
        hdcdrv_err("Memory block ref_count error. (devid=%u; ref_count=%x)\n", pool->dev_id, block_head->ref_count);
        return HDCDRV_MEM_NOT_MATCH;
    }
    block_head->ref_count++;
    ka_base_atomic_set(&block_head->status, HDCDRV_BLOCK_STATE_NORMAL);

    *buf = pool->ring[ring_id].buf;
    *addr = block_head->dma_addr;
    *offset = block_head->offset;

    pool->ring[ring_id].buf = NULL;

    pool->head++;
    ka_task_spin_unlock_bh(&pool->mem_lock);

    return HDCDRV_OK;
}

int hdccom_free_mem(struct hdcdrv_mem_pool *pool, void *buf)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    u32 ring_id;

    ka_task_spin_lock_bh(&pool->mem_lock);

    block_head = (struct hdcdrv_mem_block_head *)buf;
    if (block_head->ref_count != 1) {
        ka_task_spin_unlock_bh(&pool->mem_lock);
        hdcdrv_mem_block_head_dump(block_head);
        hdcdrv_err_spinlock("Memory block ref_count error. (current=%x)\n", block_head->ref_count);
        return HDCDRV_MEM_NOT_MATCH;
    }
    block_head->ref_count--;

    ring_id = hdccom_calc_ring_id(pool->tail, pool->mask, pool->size);
    pool->ring[ring_id].buf = buf;
    pool->tail++;

    ka_task_spin_unlock_bh(&pool->mem_lock);
    return HDCDRV_OK;
}

STATIC void *hdccom_alloc_mem_pool(struct hdccom_mem_init *init_mem, u32 segment, ka_dma_addr_t *addr, u32 *offset,
                                   ka_gfp_t gfp)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    ka_page_t *page = NULL;
    void *dma_buf = NULL;

    hdcdrv_set_time_stamp(&g_alloc_pool_stamp.dma_alloc_start);
    /* alloc data dma addr */
    if ((init_mem->pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) ||
        (init_mem->pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX)) {
        *addr = init_mem->reserve_mem_dma_addr;
        dma_buf = init_mem->reserve_mem_va;
        *offset = init_mem->reserve_mem_offset;
        init_mem->reserve_mem_dma_addr += segment;
        init_mem->reserve_mem_va += segment;
        init_mem->reserve_mem_offset += segment;
    } else {
        /* alloc data dma addr */
#ifdef CFG_FEATURE_SMALL_HUGE_POOL
        if (is_alloc_mirror_mem((u32)init_mem->dev_id, segment) == false) {
            page = hdcdrv_alloc_pages_node((u32)init_mem->dev_id, gfp, ka_mm_get_order(segment));
            if (ka_unlikely(page == NULL)) {
                return NULL;
            }
            dma_buf = ka_mm_page_address(page);
            *addr = hal_kernel_devdrv_dma_map_page(init_mem->dev, page, 0, segment, KA_DMA_BIDIRECTIONAL);
            if (ka_mm_dma_mapping_error(init_mem->dev, *addr) != 0) {
                hdcdrv_free_pages_ex(page, ka_mm_get_order(segment), KA_SUB_MODULE_TYPE_2);
                return NULL;
            }
        } else {
            dma_buf = hal_kernel_devdrv_dma_alloc_coherent(init_mem->dev, segment, addr, gfp);
        }
#else
        dma_buf = hal_kernel_devdrv_dma_alloc_coherent(init_mem->dev, segment, addr, gfp);
#endif
    }
    if (ka_unlikely(dma_buf == NULL)) {
        return NULL;
    }
    hdcdrv_set_time_stamp(&g_alloc_pool_stamp.dma_alloc_end);
    /* alloc management structure addr */
    block_head = (struct hdcdrv_mem_block_head *)hdcdrv_kvmalloc(sizeof(struct hdcdrv_mem_block_head),
                                                                 KA_SUB_MODULE_TYPE_0);
    if (ka_unlikely(block_head == NULL)) {
#ifndef DRV_UT
        if ((init_mem->pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_TX) ||
            (init_mem->pool_type == HDCDRV_RESERVE_MEM_POOL_TYPE_RX)) {
            dma_buf = NULL;
            *addr = 0;
        } else {
#ifdef CFG_FEATURE_SMALL_HUGE_POOL
            if (is_alloc_mirror_mem(init_mem->dev_id, segment) == false) {
                hal_kernel_devdrv_dma_unmap_page(init_mem->dev, *addr, segment, KA_DMA_BIDIRECTIONAL);
                hdcdrv_free_pages_ex(page, ka_mm_get_order(segment), KA_SUB_MODULE_TYPE_2);
            } else {
                hal_kernel_devdrv_dma_free_coherent(init_mem->dev, segment, dma_buf, *addr);
            }
#else
            hal_kernel_devdrv_dma_free_coherent(init_mem->dev, segment, dma_buf, *addr);
#endif
        }
#endif
        return NULL;
    }
    hdcdrv_set_time_stamp(&g_alloc_pool_stamp.head_alloc_end);
    hdcdrv_calc_alloc_pool_stamp();
    block_head->dma_buf = dma_buf;
    block_head->pool_page = page;

    return (void *)block_head;
}

int hdccom_init_mem_pool(struct hdcdrv_mem_pool *pool, struct hdccom_mem_init *init_mem)
{
    ka_dma_addr_t addr = 0;
    void *buf = NULL;
    u32 offset = 0;
    ka_gfp_t gfp;
    u32 i;

    hdcdrv_alloc_pool_stamp_init();
    pool->segment = init_mem->segment;
    pool->mask = init_mem->num - 1;
    pool->head = 0;
    pool->size = 0;
    hdcdrv_set_time_stamp(&g_alloc_pool_stamp.ring_alloc_start);
    pool->ring = (struct hdcdrv_mem *)hdcdrv_kzalloc(sizeof(struct hdcdrv_mem) * init_mem->num, KA_GFP_KERNEL,
                                                     KA_SUB_MODULE_TYPE_0);
    if (pool->ring == NULL) {
        pool->tail = 0;
        return HDCDRV_DMA_MEM_ALLOC_FAIL;
    }
    hdcdrv_set_time_stamp(&g_alloc_pool_stamp.ring_alloc_end);

    gfp = hdcdrv_init_mem_pool_get_gfp();

    KA_INIT_LIST_HEAD(&pool->wait_list);
    ka_task_spin_lock_init(&pool->mem_lock);
    hdcdrv_set_time_stamp(&g_alloc_pool_stamp.list_init_end);
    for (i = 0; i < init_mem->num; i++) {
        buf = hdccom_alloc_mem_pool(init_mem, pool->segment, &addr, &offset, gfp);
        if (ka_unlikely(buf == NULL)) {
            goto mem_alloc_fail;
        }

        hdcdrv_mem_block_head_init(buf, addr, offset, init_mem, pool->segment);
        pool->ring[i].buf = buf;
        pool->size++;
    }
    hdcdrv_set_time_stamp(&g_alloc_pool_stamp.end);
    hdcdrv_alloc_pool_stamp_record();
    pool->tail = pool->size;
    pool->dev_id = (u32)init_mem->dev_id;
    pool->valid = HDCDRV_VALID;

    return HDCDRV_OK;

mem_alloc_fail:
    hdcdrv_err("Calling alloc failed. (dev_id=%d; mem_pool=%x; num=%u; i=%u)\n", init_mem->dev_id, pool->segment,
               init_mem->num, i);
    pool->tail = pool->size;
    return HDCDRV_DMA_MEM_ALLOC_FAIL;
}

int hdccom_free_mem_pool(struct hdcdrv_mem_pool *pool, ka_device_t *dev, u32 segment)
{
    struct hdcdrv_mem_block_head *block_head = NULL;
    int ring_id;
    u32 i;

    if (pool->ring == NULL) {
        hdcdrv_err("Input parameter is error.\n");
        return HDCDRV_ERR;
    }

    if ((pool->tail - pool->head) != pool->size) {
        hdcdrv_info("Get pool value. (pool_segment=%d;alloc_size=%d;head=%lld;tail=%lld)\n", pool->segment, pool->size,
                    pool->head, pool->tail);
    }

    for (i = 0; i < (pool->tail - pool->head); i++) {
        ring_id = (int)((pool->head + i) % pool->size);
        if (hdcdrv_mem_block_head_check(pool->ring[ring_id].buf) == HDCDRV_OK) {
            block_head = pool->ring[ring_id].buf;
            free_mem_pool_single(dev, segment, block_head, block_head->dma_addr);
        }
        pool->ring[ring_id].buf = NULL;
    }

    hdcdrv_kfree(pool->ring, KA_SUB_MODULE_TYPE_0);
    pool->ring = NULL;
    pool->dev_id = 0;
    pool->valid = HDCDRV_INVALID;

    return HDCDRV_OK;
}

long hdcdrv_get_page_size(struct hdcdrv_cmd_get_page_size *cmd)
{
    cmd->page_size = KA_MM_PAGE_SIZE;
    cmd->hpage_size = KA_HPAGE_SIZE;
    cmd->page_bit = KA_MM_PAGE_SHIFT;

    return HDCDRV_OK;
}
