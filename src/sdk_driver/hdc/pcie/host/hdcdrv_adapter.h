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

#ifndef _VHDC_ADAPTER_H_
#define _VHDC_ADAPTER_H_

#include "ka_base_pub.h"

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef u64
typedef unsigned long long u64;
#endif

#define HDCDRV_SUPPORT_MAX_SESSION_PER_VDEV 136 /* 32 * 4(log + tsd + dvpp + reserved) + 8 */
#define HDCDRV_SUPPORT_MAX_SHORT_SESSION_PER_VDEV 8
#define HDCDRV_SUPPORT_MAX_LONG_SESSION_PER_VDEV \
    (HDCDRV_SUPPORT_MAX_SESSION_PER_VDEV - HDCDRV_SUPPORT_MAX_SHORT_SESSION_PER_VDEV)

#define HDCDRV_SUPPORT_MAX_MSG_CHAN_PER_VDEV 48
#define HDCDRV_VDEV_FAST_MSG_CHAN_START 2
#define HDCDRV_VDEV_NORMAL_CHAN_CNT 2
#define HDCDRV_MAX_CORE_NUM_PER_DEV 32
#define HDCDRV_VDAVINCI_TYPE_MAX 5
#define HDCDRV_VDEV_MAX_CTX_NUM 200
#define HDCDRV_VDEV_MAX_FAST_NODE_NUM 0x80000
#define HDCDRV_VDEV_MAX_FNODE_PHY_NUM (HDCDRV_MEM_MAX_PHY_NUM * 128) /* 32GB / 256MB */

struct vhdch_fast_node {
    ka_rb_node_t mem_node;
    u64 hash_va;
};

int vhdch_init(void);
int vhdch_uninit(void);

typedef int (*get_aicore_num)(u32, u32, u32 *);

#define HDCDRV_VDEV_MEM_POOL_TYPE_RX_SMALL 0
#define HDCDRV_VDEV_MEM_POOL_TYPE_RX_HUGE 1
#define HDCDRV_VDEV_MEM_POOL_TYPE_TX_SMALL 2
#define HDCDRV_VDEV_MEM_POOL_TYPE_TX_HUGE 3
#define HDCDRV_VDEV_MEM_POOL_TYPE_MAX 4
#define HDCDRV_VDEV_RX_MEM_POOL_TYPE_MAX 2

struct hdccom_alloc_mem_para;
struct hdcdrv_buf_desc;
struct hdcdrv_session_work;
struct hdcdrv_service;

#ifdef CFG_FEATURE_VFIO
int vhdch_alloc_mem_vm(struct hdccom_alloc_mem_para *para, struct hdcdrv_buf_desc *desc);
void vhdch_free_mem_vm(u32 dev_id, u32 fid, void *buf);
int vhdch_alloc_mem_container(struct hdccom_alloc_mem_para *para, struct hdcdrv_buf_desc *desc);
void vhdch_free_mem_container(u32 dev_id, u32 fid, u32 chan_id, void *buf);
int vhdch_session_pre_alloc(u32 dev_id, u32 fid, int service_type);
void vhdch_session_free(u32 dev_id, u32 fid, int service_type);
u32 vdhch_alloc_normal_msg_chan(u32 dev_id, u32 fid, int service_type);
u32 vdhch_alloc_fast_msg_chan(u32 dev_id, u32 fid, int service_type);
void hdcdrv_session_work_free(struct hdcdrv_session_work *s_work);
int hdcdrv_dma_map_guest_page(u32 dev_id, u32 fid, unsigned long in_addr, unsigned long size,
                              struct hdcdrv_buf_desc *desc);
void hdcdrv_dma_unmap_guest_page(u32 dev_id, u32 fid, ka_sg_table_t *dma_sgt);
struct hdcdrv_service *vhdch_alloc_service(u32 devid, u32 fid, int service_type, u64 host_pid);
struct hdcdrv_service *vhdch_search_service(u32 devid, u32 fid, int service_type, u64 host_pid);
#endif
#endif /* _VHDC_ADAPTER_H_ */
