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

#ifndef _HDCDRV_MEM_H_
#define _HDCDRV_MEM_H_

#include "ka_memory_pub.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "hdcdrv_mem_com.h"

struct hdcdrv_mem_pool *find_mem_pool(int pool_type, int dev_id, int len);
int alloc_mem(struct hdcdrv_mem_pool *pool, void **buf, ka_dma_addr_t *addr, u32 *offset, ka_list_head_t *wait_head);
extern void free_mem(void *buf);
extern int alloc_mem_pool(int pool_type, int dev_id, u32 segment, u32 num);
extern void free_mem_pool(int pool_type, int dev_id, u32 segment);

extern void hdcdrv_mem_block_head_init(void *buf, ka_dma_addr_t addr, int pool_type, int dev_id, u32 segment);
extern int hdcdrv_mem_block_head_check(void *buf);
extern int hdcdrv_init_mem_pool(u32 dev_id);
extern void hdcdrv_uninit_mem_pool(u32 dev_id);
extern int hdcdrv_mem_block_capacity(void);
extern void hdcdrv_recycle_mem_work(ka_work_struct_t *p_work);

ka_device_t *hdcdrv_get_pdev_dev(int dev_id);
void *hdcdrv_kvmalloc(size_t size, int level);
int hdcdrv_map_reserve_mem(struct hdccom_mem_init *init_mem, int pool_type, u32 segment, u32 num);
void hdcdrv_unmap_reserve_mem(struct hdcdrv_mem_pool *pool, ka_device_t *dev);
int hdcdrv_init_alloc_mem_pool(u32 dev_id, u32 small_packet_num, u32 huge_packet_num);
void hdcdrv_uninit_alloc_mem_pool(u32 dev_id);
int hdcdrv_init_reserve_mem_pool(u32 dev_id, u32 small_packet_num, u32 huge_packet_num);
void hdcdrv_uninit_reserve_mem_pool(u32 dev_id);
int hdcdrv_list_node_mem_alloc(struct hdcdrv_ctx *ctx, struct hdcdrv_mem_fd_list **new_node);
void hdcdrv_list_node_mem_free(void *ctx, struct hdcdrv_mem_fd_list *new_node);
long hdcdrv_fast_alloc_mem(struct hdcdrv_ctx *ctx, struct hdcdrv_cmd_alloc_mem *cmd);
struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_sep(int devid);
int hdcdrv_iova_fmem_map(u32 dev_id, u32 fid, struct hdcdrv_fast_mem *f_mem);
void hdcdrv_iova_fmem_unmap(u32 dev_id, u32 fid, struct hdcdrv_fast_mem *f_mem, u32 num);

#define HDCDRV_MEMPOOL_FREE_IN_PM 0
#define HDCDRV_MEMPOOL_FREE_IN_VM 1

/* hdc mempool level */
enum {
    HDC_MEM_POOL_LEVEL_0 = 0,
    HDC_MEM_POOL_LEVEL_1,
    HDC_MEM_POOL_LEVEL_2,
    HDC_MEM_POOL_LEVEL_3,
    HDC_MEM_POOL_LEVEL_4,
    HDC_MEM_POOL_LEVEL_INVALID
};
#endif
