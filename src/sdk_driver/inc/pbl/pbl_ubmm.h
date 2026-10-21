/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#ifndef PBL_UBMM_H
#define PBL_UBMM_H

#include "ascend_kernel_hal.h"

#define UBMM_INVALID_SERVER_ID 0x3FFU

#ifdef __cplusplus
extern "C" {
#endif

#define UBMEM_DEV_TASK_ALL (-1)

enum ubmem_dev_task_status {
    UBMEM_DEV_TASK_UNUSE = 0,
    UBMEM_DEV_TASK_INUSE = 1,
};

enum ubmem_dev_status {
    UBMEM_DEV_STATUS_NORMAL = 0,
    UBMEM_DEV_STATUS_FAULT = 1,
};

struct ubmm_pa_seg {
    u64 pa;
    u64 size;
};

int ubmm_init_dev_res(u32 udevid);
void ubmm_uninit_dev_res(u32 udevid);

int ubmm_alloc_uba(u64 size, u64 align, u64 *uba);
int ubmm_free_uba(u64 uba, u64 size);
int ubmm_get_uba_pool(u64 *uba_base, u64 *total_size, u64 *avail_size);
int ubmm_query_mem_id(u32 udevid, u32 *mem_id);

int ubmm_map(u32 udevid, u64 uba, u64 size, struct ubmm_pa_seg *pa_seg, u64 seg_num);
int ubmm_unmap(u32 udevid, u64 uba, u64 size);

int ubmm_get_hpa(u32 udevid, u32 peer_udevid, u32 mem_id, u64 offset, u32 map_route, u64 *hpa);
bool ubmm_map_route_is_exist(u32 dst_udevid, u32 src_server_id, u32 src_mem_id, u32 map_route);

/* Note: the following interfaces will go to sleep and cannot be called
 * in an atomic context
 */
int ubmem_dev_init(void);
void ubmem_dev_exit(void);
int ubmem_dev_add_task(u32 udevid, int tgid);
int ubmem_dev_delete_task(u32 udevid, int tgid, bool *is_idle);
bool ubmem_dev_task_is_idle(u32 udevid);
bool ubmem_dev_is_fault(u32 udevid);
int ubmem_dev_set_task_status(u32 udevid, int tgid, enum ubmem_dev_task_status status, bool *is_idle);
int ubmem_dev_set_dev_status(u32 udevid, enum ubmem_dev_status status);

#ifdef __cplusplus
}
#endif

#endif
