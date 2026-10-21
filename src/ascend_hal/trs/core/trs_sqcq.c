/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <pthread.h>

#include "securec.h"
#include "ascend_hal.h"

#include "trs_ioctl.h"
#include "trs_res.h"
#include "trs_cb_event.h"
#include "trs_user_pub_def.h"
#include "trs_dev_drv.h"
#include "trs_sqcq.h"
#include "trs_user_interface.h"

#if defined(__arm__) || defined(__aarch64__)
#define dsb(opt)                                  \
    {                                             \
        asm volatile("dsb " #opt : : : "memory"); \
    }
#define dmb(opt)                                  \
    {                                             \
        asm volatile("dmb " #opt : : : "memory"); \
    }
#else
#define dsb(opt)
#define dmb(opt)
#endif
#define mb() dsb(sy)
#define wmb() dsb(st) /* write fence */
#define smp_wmb() dmb(ishst)

#define ALIGN_UP(len, pagesize) ((uint32_t)((((len) + (pagesize) - 1) & (~((pagesize) - 1)))))

struct trs_sqcq_ctx {
    uint32_t sq_num;
    uint32_t cq_num;
    struct sqcq_usr_info *sq_info;
    struct sqcq_usr_info *cq_info;
};

struct trs_sq_bp_ctx {
    uint32_t sq_num;
    struct sqcq_usr_info *sq_info;
};

static struct trs_sqcq_ctx cqcq_ctxs[TRS_DEV_NUM][TRS_TS_NUM][DRV_INVALID_TYPE];
static struct trs_sq_bp_ctx g_bp_sq_info[TRS_DEV_NUM];

static int trs_sq_type_trans[DRV_INVALID_TYPE] = {
    [DRV_NORMAL_TYPE] = TRS_HW_SQ,    [DRV_CALLBACK_TYPE] = TRS_CB_SQ, [DRV_LOGIC_TYPE] = TRS_MAX_ID_TYPE,
    [DRV_SHM_TYPE] = TRS_MAX_ID_TYPE, [DRV_CTRL_TYPE] = TRS_SW_SQ,     [DRV_GDB_TYPE] = TRS_MAINT_SQ};

static int trs_cq_type_trans[DRV_INVALID_TYPE] = {
    [DRV_NORMAL_TYPE] = TRS_HW_CQ,    [DRV_CALLBACK_TYPE] = TRS_CB_CQ, [DRV_LOGIC_TYPE] = TRS_LOGIC_CQ,
    [DRV_SHM_TYPE] = TRS_MAX_ID_TYPE, [DRV_CTRL_TYPE] = TRS_SW_CQ,     [DRV_GDB_TYPE] = TRS_MAINT_CQ};

struct trs_sqcq_remote_ops g_sqcq_remote_ops;
void trs_register_sqcq_remote_ops(struct trs_sqcq_remote_ops *ops)
{
    g_sqcq_remote_ops = *ops;
}

static struct trs_sqcq_remote_ops *trs_get_sqcq_remote_ops(void)
{
    return &g_sqcq_remote_ops;
}

static struct trs_sqcq_mem_ops g_sqcq_mem_ops = {NULL};
void trs_register_sqcq_mem_ops(struct trs_sqcq_mem_ops *ops)
{
    g_sqcq_mem_ops = *ops;
}

static struct trs_sqcq_mem_ops *trs_get_sqcq_mem_ops(void)
{
    return &g_sqcq_mem_ops;
}

void devdrv_flush_cache(uint64_t base, uint32_t len);

static bool trs_is_mmap_sq(struct halSqCqInputInfo *in)
{
    return (((in->type == DRV_NORMAL_TYPE) && ((in->flag & TSDRV_FLAG_REUSE_SQ) == 0) &&
             ((in->flag & TSDRV_FLAG_ONLY_SQCQ_ID) == 0)) ||
            (in->type == DRV_SHM_TYPE) || (in->type == DRV_CTRL_TYPE));
}

static uint32_t trs_get_sq_que_len(uint32_t sqe_depth, uint32_t sqe_size)
{
    long page_size = sysconf(_SC_PAGESIZE);
    return ALIGN_UP((uint64_t)sqe_depth * sqe_size, (unsigned long)page_size);
}

void trs_sq_munmap(struct trs_sq_map *sq_map)
{
    int i;

    sq_map->que.addr = NULL;
    for (i = 0; i < TRS_UIO_ADDR_MAX; i++) {
        sq_map->ctrl[i].addr = NULL;
    }

    if ((sq_map->non_reg_addr.addr != NULL) && (sq_map->non_reg_addr.addr != MAP_FAILED)) {
        (void)trs_dev_munmap(sq_map->non_reg_addr.addr, sq_map->non_reg_addr.len);
        sq_map->non_reg_addr.addr = NULL;
    }

    if ((sq_map->reg_addr.addr != NULL) && (sq_map->reg_addr.addr != MAP_FAILED)) {
        (void)trs_dev_munmap(sq_map->reg_addr.addr, sq_map->reg_addr.len);
        sq_map->reg_addr.addr = NULL;
    }
}

int trs_sq_mmap(uint32_t dev_id, struct halSqCqInputInfo *in, struct trs_sq_map *sq_map)
{
    long page_size = sysconf(_SC_PAGESIZE);
    int prot = (in->type == DRV_SHM_TYPE) ? PROT_READ : PROT_READ | PROT_WRITE;
    uint32_t que_mmap_len = 0;
    bool is_sq_mem_specified = ((in->flag & TSDRV_FLAG_SPECIFIED_SQ_MEM) != 0);
    int i;

    if (!trs_is_mmap_sq(in)) {
        return 0;
    }

    // mmap non reg addr, and set que
    que_mmap_len = (is_sq_mem_specified ? 0 : trs_get_sq_que_len(in->sqeDepth, in->sqeSize));
    sq_map->non_reg_addr.len = (size_t)(que_mmap_len + TRS_UIO_HEAD_REG * page_size);
    sq_map->non_reg_addr.addr = trs_dev_mmap(dev_id, NULL, sq_map->non_reg_addr.len, prot, MAP_SHARED);
    if (sq_map->non_reg_addr.addr == MAP_FAILED) {
        trs_err("Mmap non reg addr failed. (len=%lx)\n", sq_map->non_reg_addr.len);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if (!is_sq_mem_specified) {
        sq_map->que.addr = (void *)sq_map->non_reg_addr.addr;
        sq_map->que.len = trs_get_sq_que_len(in->sqeDepth, in->sqeSize);
    }

    if (in->type == DRV_SHM_TYPE) { /* shm not need send */
        return 0;
    }

    // mmap reg addr
    sq_map->reg_addr.len = (size_t)((TRS_UIO_ADDR_MAX - TRS_UIO_HEAD_REG) * page_size);
    sq_map->reg_addr.addr = trs_dev_mmap(dev_id, NULL, sq_map->reg_addr.len, PROT_READ, MAP_SHARED);
    if (sq_map->reg_addr.addr == MAP_FAILED) {
        trs_dev_munmap(sq_map->non_reg_addr.addr, sq_map->non_reg_addr.len);
        trs_err("Mmap addr failed. (len=%lx)\n", sq_map->reg_addr.len);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    // set ctrl
    for (i = 0; i < TRS_UIO_ADDR_MAX; i++) {
        if (i < TRS_UIO_HEAD_REG) {
            sq_map->ctrl[i].addr = (void *)((uintptr_t)sq_map->non_reg_addr.addr + que_mmap_len +
                                            (size_t)(i * page_size));
        } else {
            sq_map->ctrl[i].addr = (void *)((uintptr_t)sq_map->reg_addr.addr +
                                            (size_t)((i - TRS_UIO_HEAD_REG) * page_size));
        }
    }

    return 0;
}

static void trs_fill_sq_alloc_info(struct halSqCqInputInfo *in, struct trs_uio_info *uio_info,
                                   struct trs_sq_map *sq_map, void *sq_que_va)
{
    struct trs_alloc_para *para = get_alloc_para_addr(in);
    int i;

    para->uio_info = uio_info;
    uio_info->sq_que_addr = ((sq_que_va != NULL) ? (unsigned long)(uintptr_t)sq_que_va :
                                                   (unsigned long)(uintptr_t)sq_map->que.addr);
    for (i = 0; i < TRS_UIO_ADDR_MAX; i++) {
        uio_info->sq_ctrl_addr[i] = (unsigned long)(uintptr_t)sq_map->ctrl[i].addr;
    }
}

static int trs_get_sq_id_type(drvSqCqType_t type)
{
    return ((type >= DRV_NORMAL_TYPE) && (type < DRV_INVALID_TYPE)) ? trs_sq_type_trans[type] : TRS_MAX_ID_TYPE;
}

static int trs_get_cq_id_type(drvSqCqType_t type)
{
    return ((type >= DRV_NORMAL_TYPE) && (type < DRV_INVALID_TYPE)) ? trs_cq_type_trans[type] : TRS_MAX_ID_TYPE;
}

static struct trs_sqcq_ctx *trs_get_sqcq_ctx(uint32_t dev_id, uint32_t ts_id, drvSqCqType_t type)
{
    return &cqcq_ctxs[dev_id][ts_id][type];
}

uint32_t trs_get_sq_num(uint32_t dev_id, uint32_t ts_id, drvSqCqType_t type)
{
    struct trs_sqcq_ctx *sqcq_ctx = trs_get_sqcq_ctx(dev_id, ts_id, type);
    return sqcq_ctx->sq_num;
}

struct sqcq_usr_info *_trs_get_sq_info(uint32_t dev_id, uint32_t ts_id, drvSqCqType_t type, uint32_t id)
{
    struct trs_sqcq_ctx *sqcq_ctx = trs_get_sqcq_ctx(dev_id, ts_id, type);

    if (id >= sqcq_ctx->sq_num) {
        trs_err("Invalid id. (dev_id=%u; ts_id=%u; type=%d; id=%u; sq_num=%u)\n", dev_id, ts_id, type, id,
                sqcq_ctx->sq_num);
        return NULL;
    }

    if (sqcq_ctx->sq_info == NULL) {
        trs_err("Not init. (dev_id=%u; ts_id=%u; type=%d; id=%u)\n", dev_id, ts_id, type, id);
        return NULL;
    }

    return &sqcq_ctx->sq_info[id];
}

static struct sqcq_usr_info *_trs_get_cq_info(uint32_t dev_id, uint32_t ts_id, drvSqCqType_t type, uint32_t id)
{
    struct trs_sqcq_ctx *sqcq_ctx = trs_get_sqcq_ctx(dev_id, ts_id, type);

    if (id >= sqcq_ctx->cq_num) {
        trs_err("Invalid id. (dev_id=%u; ts_id=%u; type=%d; id=%u)\n", dev_id, ts_id, type, id);
        return NULL;
    }

    if (sqcq_ctx->cq_info == NULL) {
        trs_err("Not init. (dev_id=%u; ts_id=%u; type=%d; id=%u)\n", dev_id, ts_id, type, id);
        return NULL;
    }

    return &sqcq_ctx->cq_info[id];
}

struct sqcq_usr_info *trs_get_sq_info(uint32_t dev_id, uint32_t ts_id, drvSqCqType_t type, uint32_t sq_id)
{
    struct sqcq_usr_info *sq_info = NULL;

    if (trs_get_sq_id_type(type) >= TRS_MAX_ID_TYPE) {
        trs_err("Invalid value. (dev_id=%u; ts_id=%u; type=%d)\n", dev_id, ts_id, type);
        return NULL;
    }

    sq_info = _trs_get_sq_info(dev_id, ts_id, type, sq_id);
    if (sq_info != NULL) {
        /* In the scenario where only sqcq id is applied for, the sq is invalid in free. */
        if (sq_info->valid == 0) {
            return NULL;
        }
    }

    return sq_info;
}

struct sqcq_usr_info *trs_get_cq_info(uint32_t dev_id, uint32_t ts_id, int type, uint32_t cq_id)
{
    struct sqcq_usr_info *cq_info = NULL;

    if (trs_get_cq_id_type(type) >= TRS_MAX_ID_TYPE) {
        trs_err("Invalid value. (dev_id=%u; ts_id=%u; type=%d)\n", dev_id, ts_id, type);
        return NULL;
    }

    cq_info = _trs_get_cq_info(dev_id, ts_id, type, cq_id);
    if (cq_info != NULL) {
        if (cq_info->valid == 0) {
            trs_warn("Not init id. (dev_id=%u; ts_id=%u; type=%d; cq_id=%u)\n", dev_id, ts_id, type, cq_id);
            return NULL;
        }
    }

    return cq_info;
}

static int trs_sqcq_info_locks_init(struct sqcq_usr_info *info, uint32_t num)
{
    uint32_t i;
    int ret;

    for (i = 0; i < num; i++) {
        ret = pthread_mutex_init(&info[i].sq_send_mutex, NULL);
        if (ret != 0) {
            goto destroy_inited_locks;
        }

        ret = pthread_rwlock_init(&info[i].mutex, NULL);
        if (ret != 0) {
            (void)pthread_mutex_destroy(&info[i].sq_send_mutex);
            goto destroy_inited_locks;
        }
    }

    return DRV_ERROR_NONE;

destroy_inited_locks:
    while (i > 0) {
        i--;
        (void)pthread_rwlock_destroy(&info[i].mutex);
        (void)pthread_mutex_destroy(&info[i].sq_send_mutex);
    }

    return ret;
}

static void trs_sqcq_info_locks_destroy(struct sqcq_usr_info *info, uint32_t num)
{
    uint32_t i;

    for (i = 0; i < num; i++) {
        (void)pthread_rwlock_destroy(&info[i].mutex);
        (void)pthread_mutex_destroy(&info[i].sq_send_mutex);
    }
}

static int trs_id_type_init(uint32_t dev_id, uint32_t ts_id, int id_type, struct sqcq_usr_info **sqcq_info,
                            uint32_t *sqcq_num)
{
    struct trs_res_id_para para = {.tsid = ts_id, .res_type = id_type};
    struct sqcq_usr_info *info = NULL;
    uint32_t num;
    int ret;

    if (id_type >= TRS_MAX_ID_TYPE) {
        *sqcq_info = NULL;
        *sqcq_num = 0;
        return 0;
    }

    ret = trs_id_query(dev_id, TRS_RES_ID_MAX_QUERY, &para, &num);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Query failed. (dev_id=%u; ts_id=%u; id_type=%d; ret=%d)\n", dev_id, ts_id, id_type, ret);
        return ret;
    }

    if (num == 0) {
        *sqcq_info = NULL;
        *sqcq_num = 0;
        trs_info("No resource. (dev_id=%u; ts_id=%u; id_type=%d)\n", dev_id, ts_id, id_type);
        return DRV_ERROR_NONE;
    }

    info = malloc(sizeof(*info) * num);
    if (info == NULL) {
        trs_err("Malloc failed. (dev_id=%u; ts_id=%u; id_type=%d; num=%u)\n", dev_id, ts_id, id_type, num);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = memset_s(info, sizeof(*info) * num, 0, sizeof(*info) * num);
    if (ret != 0) {
        free(info);
        trs_err("Memset failed. (dev_id=%u; ts_id=%u; id_type=%d; num=%u)\n", dev_id, ts_id, id_type, num);
        return ret;
    }

    ret = trs_sqcq_info_locks_init(info, num);
    if (ret != DRV_ERROR_NONE) {
        free(info);
        trs_err("Mutex init failed. (dev_id=%u; ts_id=%u; id_type=%d; num=%u; ret=%d)\n", dev_id, ts_id, id_type, num,
                ret);
        return ret;
    }

    *sqcq_info = info;
    *sqcq_num = num;

    trs_info("Id type init success. (dev_id=%u; ts_id=%u; id_type=%d; num=%u)\n", dev_id, ts_id, id_type, num);
    return DRV_ERROR_NONE;
}

static void trs_id_type_un_init(struct sqcq_usr_info **sqcq_info, uint32_t *sqcq_num)
{
    struct sqcq_usr_info *info = *sqcq_info;

    if (info == NULL) {
        *sqcq_num = 0;
        return;
    }

    trs_sqcq_info_locks_destroy(info, *sqcq_num);

    free(info);
    *sqcq_info = NULL;
    *sqcq_num = 0;
}

static int trs_ts_sq_cq_init(uint32_t dev_id, uint32_t ts_id)
{
    struct trs_sqcq_ctx *sqcq_ctx = NULL;
    drvSqCqType_t i, j;
    int ret;

    for (i = 0; i < DRV_INVALID_TYPE; i++) {
        sqcq_ctx = trs_get_sqcq_ctx(dev_id, ts_id, i);
        ret = trs_id_type_init(dev_id, ts_id, trs_get_sq_id_type(i), &sqcq_ctx->sq_info, &sqcq_ctx->sq_num);
        if (ret != DRV_ERROR_NONE) {
            goto error;
        }

        ret = trs_id_type_init(dev_id, ts_id, trs_get_cq_id_type(i), &sqcq_ctx->cq_info, &sqcq_ctx->cq_num);
        if (ret != DRV_ERROR_NONE) {
            trs_id_type_un_init(&sqcq_ctx->sq_info, &sqcq_ctx->sq_num);
            goto error;
        }
    }

    return 0;

error:
    for (j = 0; j < i; j++) {
        sqcq_ctx = trs_get_sqcq_ctx(dev_id, ts_id, j);
        trs_id_type_un_init(&sqcq_ctx->sq_info, &sqcq_ctx->sq_num);
        trs_id_type_un_init(&sqcq_ctx->cq_info, &sqcq_ctx->cq_num);
    }

    return ret;
}

int __attribute__((weak)) trs_recycle_sq_cq_with_urma(uint32_t dev_id, uint32_t ts_id, uint32_t sq_id, uint32_t cq_id,
                                                      bool remote_free_flag)
{
    (void)dev_id;
    (void)ts_id;
    (void)sq_id;
    (void)cq_id;
    (void)remote_free_flag;
    return 0;
}

static void trs_ts_sq_cq_un_init(uint32_t dev_id, uint32_t ts_id, uint32_t close_type)
{
    int connection_type = trs_get_connection_type(dev_id);
    drvSqCqType_t i;

    for (i = 0; i < DRV_INVALID_TYPE; i++) {
        struct trs_sqcq_ctx *sqcq_ctx = trs_get_sqcq_ctx(dev_id, ts_id, i);
        uint32_t j;
        for (j = 0; j < sqcq_ctx->sq_num; j++) {
            if ((connection_type == TRS_CONNECT_PROTOCOL_UB) && (i == DRV_NORMAL_TYPE)) {
                trs_recycle_sq_cq_with_urma(dev_id, ts_id, j, j, close_type == DEV_CLOSE_HOST_DEVICE);
            } else {
                trs_sq_munmap(&sqcq_ctx->sq_info[j].sq_map);
            }
        }
        trs_id_type_un_init(&sqcq_ctx->sq_info, &sqcq_ctx->sq_num);
        trs_id_type_un_init(&sqcq_ctx->cq_info, &sqcq_ctx->cq_num);
    }
}

int trs_dev_sq_cq_init(uint32_t dev_id)
{
    struct trs_dev_ctx *dev_ctx = trs_get_dev_ctx(dev_id);
    uint32_t i, j;
    int ret;

    trs_info("Sqcq init. (devid=%u)\n", dev_id);

    for (i = 0; i < dev_ctx->ts_num; i++) {
        ret = trs_ts_sq_cq_init(dev_id, i);
        if (ret != DRV_ERROR_NONE) {
            for (j = 0; j < i; j++) {
                trs_ts_sq_cq_un_init(dev_id, j, DEV_CLOSE_HOST_DEVICE);
            }
            return ret;
        }

        (void)pthread_mutex_init(&dev_ctx->ts_ctx[i].trs_sqcq_mutex, NULL);
    }

    return 0;
}

void trs_dev_sq_cq_uninit(uint32_t dev_id, uint32_t close_type)
{
    struct trs_dev_ctx *dev_ctx = trs_get_dev_ctx(dev_id);
    uint32_t i, ts_num = dev_ctx->ts_num;

    for (i = 0; i < ts_num; i++) {
        trs_sq_munmap(&dev_ctx->ts_ctx[i].shm_sq);
        trs_ts_sq_cq_un_init(dev_id, i, close_type);
    }
}

static bool trs_is_sq_surport_uio(struct sqcq_usr_info *info)
{
    return (info->sq_ctrl.que_addr != NULL);
}

static UINT64 trs_get_sq_que_addr(struct sqcq_usr_info *info)
{
    return (uintptr_t)info->sq_ctrl.que_addr;
}

static UINT64 trs_get_sq_bind_que_addr(uint32_t dev_id, uint32_t ts_id, struct sqcq_usr_info *sq_info)
{
    if (sq_info->sq_ctrl.que_addr != NULL) {
        return (uintptr_t)sq_info->sq_ctrl.que_addr;
    }

    if (sq_info->switch_stream_flag) {
        struct res_id_usr_info *stream_usr_info = trs_get_res_id_info(dev_id, ts_id, DRV_STREAM_ID, sq_info->stream_id);
        if ((stream_usr_info != NULL) && (stream_usr_info->valid != 0)) {
            return stream_usr_info->res_addr;
        }
    }

    return (uintptr_t)NULL;
}

static uint32_t trs_get_sq_mem_attr(struct sqcq_usr_info *info)
{
    return info->sq_ctrl.mem_local_flag;
}

static bool trs_is_cq_support_recv(struct sqcq_usr_info *info)
{
    return ((info->flag & TSDRV_FLAG_ONLY_SQCQ_ID) == 0);
}

static inline uint32_t trs_get_sq_head(struct sqcq_usr_info *sq_info)
{
    return *((UINT16 *)sq_info->sq_ctrl.head);
}

static inline void trs_set_sq_head(struct sqcq_usr_info *sq_info, uint32_t head)
{
    mb();
    *((UINT16 *)sq_info->sq_ctrl.head) = (UINT16)head;
}

static inline uint32_t trs_get_sq_head_reg(struct sqcq_usr_info *sq_info)
{
    return *((uint32_t *)sq_info->sq_ctrl.head_reg);
}

static inline void trs_set_sq_tail(struct sqcq_usr_info *sq_info, uint32_t tail)
{
    smp_wmb();
    *((UINT16 *)sq_info->sq_ctrl.tail) = (UINT16)tail;
}

static inline uint32_t trs_get_sqtail(struct sqcq_usr_info *sq_info)
{
    return *((UINT16 *)sq_info->sq_ctrl.tail);
}

static inline uint32_t trs_get_sq_tail_reg(struct sqcq_usr_info *sq_info)
{
    return *((uint32_t *)sq_info->sq_ctrl.tail_reg);
}

static inline void trs_set_sq_db(struct sqcq_usr_info *sq_info, uint32_t value)
{
    wmb();
    *((uint32_t *)sq_info->sq_ctrl.db) = value;
}

static inline uint32_t trs_get_sq_db(struct sqcq_usr_info *sq_info)
{
    return *((UINT16 *)sq_info->sq_ctrl.db);
}

static inline bool trs_is_sq_use_soft_que(struct sqcq_usr_info *sq_info)
{
    return (sq_info->sq_ctrl.soft_que_flag == 1);
}

int trs_set_sq_info_head(uint32_t dev_id, uint32_t ts_id, int type, uint32_t sq_id, uint32_t head)
{
    struct sqcq_usr_info *sq_info = trs_get_sq_info(dev_id, ts_id, type, sq_id);
    if (sq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, ts_id, type, sq_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    sq_info->head = head;
    return DRV_ERROR_NONE;
}

int trs_set_sq_info_tail(uint32_t dev_id, uint32_t ts_id, int type, uint32_t sq_id, uint32_t tail)
{
    struct sqcq_usr_info *sq_info = trs_get_sq_info(dev_id, ts_id, type, sq_id);
    if (sq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, ts_id, type, sq_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    sq_info->tail = tail;
    return DRV_ERROR_NONE;
}

static inline bool trs_is_support_sq_mem_ops(uint32_t dev_id, struct halSqCqInputInfo *in)
{
    if (in->type == DRV_NORMAL_TYPE) {
        if ((trs_get_connection_type(dev_id) == TRS_CONNECT_PROTOCOL_PCIE) && ((in->flag & TSDRV_FLAG_REUSE_SQ) == 0) &&
            ((in->flag & TSDRV_FLAG_ONLY_SQCQ_ID) == 0) && ((in->flag & TSDRV_FLAG_NO_SQ_MEM) == 0)) {
            return true;
        }
    }

    return false;
}

static void trs_sq_usr_info_init(uint32_t dev_id, struct sqcq_usr_info *info, struct halSqCqInputInfo *in,
                                 struct trs_sq_map *sq_map)
{
    (void)dev_id;
    struct trs_alloc_para *para = get_alloc_para_addr(in);
    struct trs_uio_info *uio_info = para->uio_info;

    if ((in->type == DRV_NORMAL_TYPE) && ((in->flag & TSDRV_FLAG_ONLY_SQCQ_ID) != 0)) {
        info->flag = in->flag;
        info->valid = 1;
        info->status = 1;
        return;
    }

    info->sq_ctrl.mem_local_flag = uio_info->sq_mem_local_flag;
    info->sq_que_spec_addr = (sq_map->que.addr != (void *)(uintptr_t)uio_info->sq_que_addr) ?
                                 (void *)(uintptr_t)uio_info->sq_que_addr :
                                 NULL; /* set specified alloced mem */
    if (uio_info->uio_flag == 0) {     /* sq not support user op */
        trs_sq_munmap(sq_map);
        info->sq_ctrl.que_addr = NULL;
    } else {
        info->sq_ctrl.que_addr = (void *)(uintptr_t)uio_info->sq_que_addr;
        info->sq_ctrl.db = (void *)(uintptr_t)uio_info->sq_ctrl_addr[TRS_UIO_DB];
        info->sq_ctrl.head = (void *)(uintptr_t)uio_info->sq_ctrl_addr[TRS_UIO_HEAD];
        info->sq_ctrl.tail = (void *)(uintptr_t)uio_info->sq_ctrl_addr[TRS_UIO_TAIL];
        info->sq_ctrl.shr_info = (struct trs_sq_shr_info *)(uintptr_t)uio_info->sq_ctrl_addr[TRS_UIO_SHR_INFO];
        info->sq_ctrl.head_reg = (void *)(uintptr_t)uio_info->sq_ctrl_addr[TRS_UIO_HEAD_REG];
        info->sq_ctrl.tail_reg = (void *)(uintptr_t)uio_info->sq_ctrl_addr[TRS_UIO_TAIL_REG];
        info->sq_ctrl.soft_que_flag = uio_info->soft_que_flag;

        info->sq_map = *sq_map;
    }
    if (in->sqeSize == 0U) {
        trs_err("sqe_size is 0. (ts_id=%u; type=%d; id=%u)\n", in->tsId, in->type, in->sqId);
        return;
    }
    info->head = 0;
    info->tail = 0;
    info->depth = in->sqeDepth;
    info->e_size = in->sqeSize;
    info->max_num = TRS_SQCQ_BUFF_LEN / in->sqeSize;
    info->cur_num = 0;
    info->flag = in->flag;
    info->valid = 1;
    info->status = 1;

    if (in->type == DRV_CTRL_TYPE) {
        info->depth -= TRS_CTRL_USE_SQE_NUM; /* last 2 sqe used for head and tail ptr */
    }
}

static void trs_sq_usr_info_un_init(uint32_t dev_id, struct sqcq_usr_info *info)
{
    if ((info->sq_que_spec_addr != NULL) && (trs_get_sqcq_mem_ops()->mem_free != NULL)) {
        trs_get_sqcq_mem_ops()->mem_free(dev_id, (uint64_t)info->sq_que_spec_addr,
                                         trs_get_sq_que_len(info->depth, info->e_size));
        info->sq_que_spec_addr = NULL;
    }

    trs_sq_munmap(&info->sq_map);
    (void)memset_s(&info->sq_ctrl, sizeof(info->sq_ctrl), 0, sizeof(info->sq_ctrl));
    info->valid = 0;
    info->flag = 0;
    info->status = 0;
    info->urma_ctx = NULL;
    info->switch_stream_flag = 0;
    info->stream_id = UINT32_MAX;
}

static void trs_cq_usr_info_init(struct sqcq_usr_info *info, struct halSqCqInputInfo *in)
{
    info->depth = in->cqeDepth;
    info->e_size = in->cqeSize;
    if (in->cqeSize == 0U) {
        info->max_num = 0U;
    } else {
        info->max_num = TRS_SQCQ_BUFF_LEN / in->cqeSize;
    }
    info->cur_num = 0;
    info->flag = in->flag;
    info->valid = 1;

    trs_debug("cq info. (type=%u; cqid=%u; depth=%u; size=%u)\n", in->type, in->cqId, in->cqeDepth, in->cqeSize);
}

static void trs_cq_usr_info_un_init(struct sqcq_usr_info *info)
{
    info->valid = 0;
    info->flag = 0;
}

static void trs_shm_sq_init(uint32_t dev_id, uint32_t ts_id, struct trs_sq_map *sq_map)
{
    struct trs_dev_ctx *dev_ctx = trs_get_dev_ctx(dev_id);
    dev_ctx->ts_ctx[ts_id].shm_sq = *sq_map;
}

static void trs_shm_sq_un_init(uint32_t dev_id, uint32_t ts_id)
{
    struct trs_dev_ctx *dev_ctx = trs_get_dev_ctx(dev_id);
    trs_sq_munmap(&dev_ctx->ts_ctx[ts_id].shm_sq);
}

void trs_sq_info_init(uint32_t dev_id, struct halSqCqInputInfo *in, struct trs_sq_map *sq_map)
{
    struct sqcq_usr_info *info = NULL;

    if (trs_get_sq_id_type(in->type) >= TRS_MAX_ID_TYPE) {
        if (in->type == DRV_SHM_TYPE) {
            trs_shm_sq_init(dev_id, in->tsId, sq_map);
        }
        return;
    }

    if ((in->type == DRV_NORMAL_TYPE) && ((in->flag & TSDRV_FLAG_REUSE_SQ) != 0)) {
        return;
    }

    info = _trs_get_sq_info(dev_id, in->tsId, in->type, in->sqId);
    if (info != NULL) {
        trs_sq_usr_info_init(dev_id, info, in, sq_map);
    }
}

static void trs_sq_info_lock(uint32_t dev_id, struct halSqCqFreeInfo *in, struct sqcq_usr_info **info)
{
    if (trs_get_sq_id_type(in->type) >= TRS_MAX_ID_TYPE) {
        return;
    }

    if ((in->type == DRV_NORMAL_TYPE) && ((in->flag & TSDRV_FLAG_REUSE_SQ) != 0)) {
        return;
    }

    *info = _trs_get_sq_info(dev_id, in->tsId, in->type, in->sqId);
    if (*info == NULL) {
        return;
    }
    (void)pthread_rwlock_wrlock(&(*info)->mutex);
}

static void trs_sq_info_unlock(struct sqcq_usr_info *info)
{
    if (info != NULL) {
        (void)pthread_rwlock_unlock(&info->mutex);
    }
}

static void trs_sq_info_un_init(uint32_t dev_id, struct halSqCqFreeInfo *in)
{
    struct sqcq_usr_info *info = NULL;

    if (trs_get_sq_id_type(in->type) >= TRS_MAX_ID_TYPE) {
        if (in->type == DRV_SHM_TYPE) {
            trs_shm_sq_un_init(dev_id, in->tsId);
        }
        return;
    }

    if ((in->type == DRV_NORMAL_TYPE) && ((in->flag & TSDRV_FLAG_REUSE_SQ) != 0)) {
        return;
    }

    info = trs_get_sq_info(dev_id, in->tsId, in->type, in->sqId);
    if (info != NULL) {
        trs_sq_usr_info_un_init(dev_id, info);
    }
}

void trs_cq_info_init(uint32_t dev_id, struct halSqCqInputInfo *in)
{
    struct trs_dev_ctx *dev_ctx = trs_get_dev_ctx(dev_id);
    struct sqcq_usr_info *info = NULL;

    if (trs_get_cq_id_type(in->type) >= TRS_MAX_ID_TYPE) {
        return;
    }

    if (in->type == DRV_NORMAL_TYPE) {
        if ((in->flag & TSDRV_FLAG_REUSE_CQ) != 0) {
            return;
        }
        dev_ctx->ts_ctx[in->tsId].normal_cq = in->cqId;
    }

    info = _trs_get_cq_info(dev_id, in->tsId, in->type, in->cqId);
    if (info != NULL) {
        trs_cq_usr_info_init(info, in);
    }
}

static void trs_cq_info_uninit(uint32_t dev_id, struct halSqCqFreeInfo *in)
{
    struct sqcq_usr_info *info = NULL;

    if (trs_get_cq_id_type(in->type) >= TRS_MAX_ID_TYPE) {
        return;
    }

    if ((in->type == DRV_NORMAL_TYPE) && ((in->flag & TSDRV_FLAG_REUSE_CQ) != 0)) {
        return;
    }

    info = trs_get_cq_info(dev_id, in->tsId, in->type, in->cqId);
    if (info != NULL) {
        trs_cq_usr_info_un_init(info);
    }
}

static struct sqcq_usr_info *trs_get_sq_bp_ctx(uint32_t dev_id, uint32_t sq_id)
{
    if ((sq_id >= g_bp_sq_info[dev_id].sq_num) || (g_bp_sq_info[dev_id].sq_info == NULL)) {
        return NULL;
    }
    return &g_bp_sq_info[dev_id].sq_info[sq_id];
}

static bool trs_is_alloc_sq(drvSqCqType_t type)
{
    return ((type == DRV_NORMAL_TYPE) || (type == DRV_CALLBACK_TYPE) || (type == DRV_SHM_TYPE) ||
            (type == DRV_CTRL_TYPE));
}

bool trs_is_remote_sqcq_ops(drvSqCqType_t type, uint32_t flag)
{
    return (((type == DRV_NORMAL_TYPE) || (type == DRV_LOGIC_TYPE)) && ((flag & TSDRV_FLAG_REMOTE_ID) != 0));
}

drvError_t trs_sqcq_alloc_para_check(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    if ((in == NULL) || (out == NULL)) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((dev_id >= TRS_DEV_NUM) || (in->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", dev_id, in->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((trs_is_alloc_sq(in->type) && (in->sqeSize > TRS_SQCQ_BUFF_LEN)) || (in->cqeSize > TRS_SQCQ_BUFF_LEN) ||
        (in->type >= DRV_INVALID_TYPE) || ((in->type == DRV_CTRL_TYPE) && (in->sqeDepth <= TRS_CTRL_USE_SQE_NUM))) {
        trs_err("Invalid para. (dev_id=%u; type=%u; sqe_depth=%u; sqe_size=%u; cqe_depth=%u; cqe_size=%u)\n", dev_id,
                in->type, in->sqeDepth, in->sqeSize, in->cqeDepth, in->cqeSize);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

static drvError_t trs_remote_sqcq_alloc(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    if (trs_get_sqcq_remote_ops()->sqcq_alloc != NULL) {
        return trs_get_sqcq_remote_ops()->sqcq_alloc(dev_id, in, out);
    } else {
        trs_warn("Not support. (dev_id=%u; res_type=%d; flag=%u)\n", dev_id, in->type, in->flag);
        return DRV_ERROR_NOT_SUPPORT;
    }
}

drvError_t trs_local_sqcq_alloc(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    struct trs_dev_ctx *dev_ctx = trs_get_dev_ctx(dev_id);
    struct trs_uio_info uio_info;
    struct trs_sq_map sq_map = {0};
    void *sq_que_va = NULL;
    int ret;

    in->flag &= (~TSDRV_FLAG_SPECIFIED_SQ_MEM);
    if ((trs_is_support_sq_mem_ops(dev_id, in)) && (trs_get_sqcq_mem_ops()->mem_alloc != NULL)) {
        in->flag |= TSDRV_FLAG_SPECIFIED_SQ_MEM;
        if ((in->flag & TSDRV_FLAG_SPECIFIED_SQ_ID) != 0) {
            struct sqcq_usr_info *sq_info = trs_get_sq_bp_ctx(dev_id, in->sqId);
            if ((sq_info != NULL) && (sq_info->valid == 1)) {
                sq_que_va = sq_info->sq_ctrl.que_addr;
                trs_debug("Use svm restored memory. (dev_id=%u; sq_id=%u; sq_que_va=0x%llx)\n", dev_id, in->sqId,
                          (unsigned long long)(uintptr_t)sq_que_va);
            }
        }
        if (sq_que_va == NULL) {
            ret = trs_get_sqcq_mem_ops()->mem_alloc(dev_id, (uint64_t *)&sq_que_va,
                                                    trs_get_sq_que_len(in->sqeDepth, in->sqeSize));
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
        }
    }

    ret = trs_sq_mmap(dev_id, in, &sq_map);
    if (ret != DRV_ERROR_NONE) {
        goto sq_mem_free;
    }

    trs_fill_sq_alloc_info(in, &uio_info, &sq_map, sq_que_va);

    (void)pthread_mutex_lock(&dev_ctx->ts_ctx[in->tsId].trs_sqcq_mutex);
    ret = trs_dev_io_ctrl(dev_id, TRS_SQCQ_ALLOC, in);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&dev_ctx->ts_ctx[in->tsId].trs_sqcq_mutex);
        trs_err_if((ret != DRV_ERROR_NO_RESOURCES), "Ioctl ret. (ret=%d; dev_id=%u)\n", ret, dev_id);
        goto sq_munmap;
    }

    trs_debug("Alloc success. (dev_id=%u; ts_id=%u; sq_id=%u; cq_id=%u)\n", dev_id, in->tsId, in->sqId, in->cqId);

    trs_sq_info_init(dev_id, in, &sq_map);
    trs_cq_info_init(dev_id, in);

    out->sqId = in->sqId;
    out->cqId = in->cqId;
    out->flag = in->flag;
    if (in->type == DRV_SHM_TYPE) {
        out->queueVAddr = (unsigned long long)(uintptr_t)sq_map.que.addr;
    }
    (void)pthread_mutex_unlock(&dev_ctx->ts_ctx[in->tsId].trs_sqcq_mutex);

    return DRV_ERROR_NONE;

sq_munmap:
    trs_sq_munmap(&sq_map);
sq_mem_free:
    if ((trs_is_support_sq_mem_ops(dev_id, in)) && (trs_get_sqcq_mem_ops()->mem_free != NULL)) {
        trs_get_sqcq_mem_ops()->mem_free(dev_id, (uint64_t)sq_que_va, trs_get_sq_que_len(in->sqeDepth, in->sqeSize));
    }
    return ret;
}

drvError_t trs_local_sqcq_free(uint32_t dev_id, struct halSqCqFreeInfo *info)
{
    struct trs_dev_ctx *dev_ctx = trs_get_dev_ctx(dev_id);
    int ret;
    struct sqcq_usr_info *sq_info = NULL;

    (void)pthread_mutex_lock(&dev_ctx->ts_ctx[info->tsId].trs_sqcq_mutex);
    trs_sq_info_lock(dev_id, info, &sq_info);
    ret = trs_dev_io_ctrl(dev_id, TRS_SQCQ_FREE, info);
    if (ret != DRV_ERROR_NONE) {
        trs_sq_info_unlock(sq_info);
        (void)pthread_mutex_unlock(&dev_ctx->ts_ctx[info->tsId].trs_sqcq_mutex);
        trs_err("Ioctl failed. (dev_id=%u)\n", dev_id);
        return ret;
    }

    trs_sq_info_un_init(dev_id, info);
    trs_sq_info_unlock(sq_info);
    trs_cq_info_uninit(dev_id, info);
    (void)pthread_mutex_unlock(&dev_ctx->ts_ctx[info->tsId].trs_sqcq_mutex);

    return DRV_ERROR_NONE;
}

drvError_t trs_sqcq_alloc(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    drvError_t ret;

    if (trs_is_remote_sqcq_ops(in->type, in->flag)) {
        return trs_remote_sqcq_alloc(dev_id, in, out);
    }

    ret = trs_local_sqcq_alloc(dev_id, in, out);
    if (ret != 0) {
        trs_warn("Alloc sqcq warn. (dev_id=%u)\n", dev_id);
        return ret;
    }
    trs_debug("Alloc sqcq success. (dev_id=%u; sq_id=%u; cq_id=%u)\n", dev_id, out->sqId, out->cqId);
    return ret;
}

/* internal interface, running in dev cp proc */
drvError_t _halSqCqAllocate(uint32_t dev_id, struct halSqCqInputInfo *in, struct halSqCqOutputInfo *out)
{
    drvError_t ret;

    ret = trs_sqcq_alloc_para_check(dev_id, in, out);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return trs_local_sqcq_alloc(dev_id, in, out);
}

drvError_t trs_sqcq_free_para_check(uint32_t dev_id, struct halSqCqFreeInfo *info)
{
    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((dev_id >= TRS_DEV_NUM) || (info->tsId >= TRS_TS_NUM) || (info->type >= DRV_INVALID_TYPE)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%u)\n", dev_id, info->tsId, info->type);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

static drvError_t trs_remote_sqcq_free(uint32_t dev_id, struct halSqCqFreeInfo *info)
{
    if (trs_get_sqcq_remote_ops()->sqcq_free != NULL) {
        return trs_get_sqcq_remote_ops()->sqcq_free(dev_id, info);
    } else {
        trs_warn("Not support. (dev_id=%u; res_type=%d; flag=%u)\n", dev_id, info->type, info->flag);
        return DRV_ERROR_NOT_SUPPORT;
    }
}

drvError_t trs_sqcq_free(uint32_t dev_id, struct halSqCqFreeInfo *info)
{
    if (trs_is_remote_sqcq_ops(info->type, info->flag)) {
        return trs_remote_sqcq_free(dev_id, info);
    }

    return trs_local_sqcq_free(dev_id, info);
}

/* internal interface, running in dev cp proc */
drvError_t _halSqCqFree(uint32_t dev_id, struct halSqCqFreeInfo *info)
{
    drvError_t ret;

    ret = trs_sqcq_free_para_check(dev_id, info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return trs_local_sqcq_free(dev_id, info);
}

static drvError_t trs_hw_sq_cq_config_uio(struct halSqCqConfigInfo *info, struct sqcq_usr_info *sq_info)
{
    if ((info->prop == DRV_SQCQ_PROP_SQ_HEAD) && (sq_info->sq_ctrl.head != NULL)) {
        trs_set_sq_head(sq_info, info->value[0]);
        return DRV_ERROR_NONE;
    } else if ((info->prop == DRV_SQCQ_PROP_SQ_TAIL) && (sq_info->sq_ctrl.tail != NULL)) {
        trs_set_sq_tail(sq_info, info->value[0]);
        return DRV_ERROR_NONE;
    } else if ((info->prop == DRV_SQCQ_PROP_SQ_TAIL) && (sq_info->sq_ctrl.db != NULL)) {
        trs_set_sq_db(sq_info, info->value[0]);
        return DRV_ERROR_NONE;
    } else {
        /* do nothing */
    }

    return DRV_ERROR_NOT_SUPPORT;
}

static drvError_t trs_sq_cq_config_uio(uint32_t dev_id, struct halSqCqConfigInfo *info)
{
    struct sqcq_usr_info *sq_info = NULL;
    int ret = DRV_ERROR_NOT_SUPPORT;

    sq_info = trs_get_sq_info(dev_id, info->tsId, info->type, info->sqId);
    if (sq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, info->tsId, info->type, info->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_rwlock_rdlock(&sq_info->mutex);
    if ((!sq_info->valid) || (!sq_info->status)) {
        (void)pthread_rwlock_unlock(&sq_info->mutex);
        trs_err("Invalid status. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u; status=%u; prop=%d)\n", dev_id, info->tsId,
                info->type, info->sqId, sq_info->status, info->prop);
        return DRV_ERROR_STATUS_FAIL;
    }

    if (!trs_is_sq_use_soft_que(sq_info)) {
        ret = trs_hw_sq_cq_config_uio(info, sq_info);
    }

    if (info->prop == DRV_SQCQ_PROP_SQ_HEAD) {
        sq_info->head = info->value[0];
    }
    if (info->prop == DRV_SQCQ_PROP_SQ_TAIL) {
        sq_info->tail = info->value[0];
    }

    (void)pthread_rwlock_unlock(&sq_info->mutex);

    return ret;
}

static drvError_t trs_sq_info_reset(uint32_t dev_id, struct halSqCqConfigInfo *info)
{
    struct sqcq_usr_info *sq_info = NULL;

    sq_info = trs_get_sq_info(dev_id, info->tsId, info->type, info->sqId);
    if (sq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, info->tsId, info->type, info->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_rwlock_rdlock(&sq_info->mutex);
    if (sq_info->status == 1) {
        trs_warn("Invalid status. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u; status=%u)\n", dev_id, info->tsId,
                 info->type, info->sqId, sq_info->status);
        /* unlock and return DRV_ERROR_STATUS_FAIL after runtime done */
    }
    sq_info->head = 0;
    sq_info->tail = 0;
    sq_info->pos = 0;
    (void)pthread_rwlock_unlock(&sq_info->mutex);
    trs_debug("Reset success. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u; cq_id=%u)\n", dev_id, info->tsId, info->type,
              info->sqId, info->cqId);

    return DRV_ERROR_NONE;
}

static drvError_t trs_single_sq_ctrl(uint32_t dev_id, uint32_t ts_id, uint32_t sqId, drvSqCqPropType_t prop)
{
    struct sqcq_usr_info *sq_info = NULL;
    drvError_t ret = DRV_ERROR_PARA_ERROR;

    sq_info = trs_get_sq_info(dev_id, ts_id, DRV_NORMAL_TYPE, sqId);
    if (sq_info == NULL) {
        return DRV_ERROR_NOT_EXIST;
    }

    (void)pthread_rwlock_rdlock(&sq_info->mutex);
    if (prop == DRV_SQCQ_PROP_SQ_PAUSE) {
        sq_info->status = 0;
        ret = DRV_ERROR_NONE;
    }
    if (prop == DRV_SQCQ_PROP_SQ_RESUME) {
        sq_info->status = 1;
        ret = DRV_ERROR_NONE;
    }
    (void)pthread_rwlock_unlock(&sq_info->mutex);
    return ret;
}

static drvError_t trs_sq_ctrl(uint32_t dev_id, struct halSqCqConfigInfo *info)
{
    drvError_t ret;

    if (info->type != DRV_NORMAL_TYPE) {
        return DRV_ERROR_PARA_ERROR;
    }
    if (info->sqId == UINT_MAX) {
        struct trs_sqcq_ctx *sqcq_ctx = trs_get_sqcq_ctx(dev_id, info->tsId, DRV_NORMAL_TYPE);
        uint32_t sq_id;
        for (sq_id = 0; sq_id < sqcq_ctx->sq_num; sq_id++) {
            (void)trs_single_sq_ctrl(dev_id, info->tsId, sq_id, info->prop);
        }
        return DRV_ERROR_NONE;
    }
    ret = trs_single_sq_ctrl(dev_id, info->tsId, info->sqId, info->prop);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Pause failed. (dev_id=%u; ts_id=%u; sq_id=%u; prop=%d; ret=%d)\n", dev_id, info->tsId, info->sqId,
                info->prop, ret);
    }
    return ret;
}

static drvError_t trs_sq_cq_config_para_check(uint32_t dev_id, struct halSqCqConfigInfo *info)
{
    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((dev_id >= TRS_DEV_NUM) || (info->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", dev_id, info->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((info->type == DRV_NORMAL_TYPE) && (info->sqId != UINT_MAX)) {
        struct sqcq_usr_info *sq_info = trs_get_sq_info(dev_id, info->tsId, info->type, info->sqId);
        if (sq_info == NULL) {
            return DRV_ERROR_NOT_EXIST;
        }
        if (((sq_info->flag & TSDRV_FLAG_REMOTE_ID) == 0) &&
            ((info->value[SQCQ_CONFIG_INFO_FLAG] & TSDRV_FLAG_REMOTE_ID) != 0)) {
            trs_err("Local sqcq but config remote flag. (dev_id=%u; sq_id=%u; sq_flag=0x%x; config_flag=0x%x)\n",
                    dev_id, info->sqId, sq_info->flag, info->value[SQCQ_CONFIG_INFO_FLAG]);
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t trs_sq_cq_config(uint32_t dev_id, struct halSqCqConfigInfo *info)
{
    drvError_t ret;

    ret = trs_sq_cq_config_para_check(dev_id, info);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Failed to check para. (ret=%d; dev_id=%u)\n", ret, dev_id);
        return ret;
    }

    if ((info->type == DRV_NORMAL_TYPE) &&
        ((info->prop == DRV_SQCQ_PROP_SQ_HEAD) || (info->prop == DRV_SQCQ_PROP_SQ_TAIL))) {
        ret = trs_sq_cq_config_uio(dev_id, info);
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            return ret;
        }
    }

    if ((info->type == DRV_NORMAL_TYPE) && (info->prop == DRV_SQCQ_PROP_SQCQ_RESET)) {
        ret = trs_sq_info_reset(dev_id, info);
        if (ret != DRV_ERROR_NONE) {
            trs_err("Sq_info reset failed. (ret=%d; dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", ret, dev_id, info->tsId,
                    info->type, info->sqId);
            return ret;
        }
    }

    if ((info->prop == DRV_SQCQ_PROP_SQ_PAUSE) || (info->prop == DRV_SQCQ_PROP_SQ_RESUME)) {
        ret = trs_sq_ctrl(dev_id, info);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    ret = trs_dev_io_ctrl(dev_id, TRS_SQCQ_CONFIG, info);
    if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_NOT_SUPPORT)) {
        trs_err("Ioctl failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static drvError_t trs_soft_que_sq_cq_query_uio(struct halSqCqQueryInfo *info, struct sqcq_usr_info *sq_info)
{
    if ((info->prop == DRV_SQCQ_PROP_SQ_HEAD) && (sq_info->sq_ctrl.head_reg != NULL)) {
        info->value[0] = trs_get_sq_head_reg(sq_info);
        return DRV_ERROR_NONE;
    } else if ((info->prop == DRV_SQCQ_PROP_SQ_TAIL) && (sq_info->sq_ctrl.tail_reg != NULL)) {
        info->value[0] = trs_get_sq_tail_reg(sq_info);
        return DRV_ERROR_NONE;
    } else if ((info->prop == DRV_SQCQ_PROP_SQ_CQE_STATUS) && (sq_info->sq_ctrl.shr_info != NULL)) {
        info->value[0] = sq_info->sq_ctrl.shr_info->cqe_status;
        if (info->value[0] == 1) {
            sq_info->sq_ctrl.shr_info->cqe_status = 0; /* clear status */
        }
        return DRV_ERROR_NONE;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }
}

static drvError_t trs_hw_sq_cq_query_uio(struct halSqCqQueryInfo *info, struct sqcq_usr_info *sq_info)
{
    if (info->prop == DRV_SQCQ_PROP_SQ_DEPTH) {
        info->value[0] = sq_info->depth;
        return DRV_ERROR_NONE;
    }

    if (info->prop == DRV_SQCQ_PROP_SQE_SIZE) {
        info->value[0] = sq_info->e_size;
        return DRV_ERROR_NONE;
    }

    if ((info->prop == DRV_SQCQ_PROP_SQ_HEAD) && (sq_info->sq_ctrl.head != NULL)) {
        info->value[0] = trs_get_sq_head(sq_info);
        return DRV_ERROR_NONE;
    } else if (info->prop == DRV_SQCQ_PROP_SQ_TAIL) {
        if (sq_info->sq_ctrl.tail != NULL) {
            info->value[0] = trs_get_sqtail(sq_info);
        } else if (sq_info->sq_ctrl.db != NULL) { /* when uio_d mode, tail is NULL, use db */
            info->value[0] = trs_get_sq_db(sq_info);
        } else {
            return DRV_ERROR_NOT_SUPPORT;
        }
        return DRV_ERROR_NONE;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }
}

drvError_t trs_sq_cq_query_uio(uint32_t dev_id, struct halSqCqQueryInfo *info)
{
    struct sqcq_usr_info *sq_info = NULL;
    int ret;

    sq_info = trs_get_sq_info(dev_id, info->tsId, info->type, info->sqId);
    if (sq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, info->tsId, info->type, info->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_rwlock_rdlock(&sq_info->mutex);
    if (!sq_info->valid) {
        (void)pthread_rwlock_unlock(&sq_info->mutex);
        trs_err("Invalid sq_info. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, info->tsId, info->type,
                info->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((info->prop == DRV_SQCQ_PROP_SQ_BASE) && trs_is_sq_surport_uio(sq_info)) {
        info->value[0] = (uint32_t)(trs_get_sq_que_addr(sq_info) & 0xffffffff); /* low 32 bit */
        info->value[1] = (uint32_t)(trs_get_sq_que_addr(sq_info) >> 32);        /* 0xffffffff for high 32bit */
        (void)pthread_rwlock_unlock(&sq_info->mutex);
        return DRV_ERROR_NONE;
    }

    if (info->prop == DRV_SQCQ_PROP_SQ_MEM_ATTR) {
        info->value[0] = trs_get_sq_mem_attr(sq_info);
        (void)pthread_rwlock_unlock(&sq_info->mutex);
        return DRV_ERROR_NONE;
    }

    if (trs_is_sq_use_soft_que(sq_info)) {
        ret = trs_soft_que_sq_cq_query_uio(info, sq_info);
    } else {
        ret = trs_hw_sq_cq_query_uio(info, sq_info);
    }
    (void)pthread_rwlock_unlock(&sq_info->mutex);
    return ret;
}

static drvError_t trs_cq_info_query(uint32_t dev_id, struct halSqCqQueryInfo *info)
{
    struct sqcq_usr_info *cq_info = NULL;

    cq_info = trs_get_cq_info(dev_id, info->tsId, info->type, info->cqId);
    if (cq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; cq_id=%u)\n", dev_id, info->tsId, info->type, info->cqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (cq_info->valid == 0) {
        trs_err("Invalid cq_info. (dev_id=%u; ts_id=%u; type=%d; cq_id=%u)\n", dev_id, info->tsId, info->type,
                info->cqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (info->prop) {
        case DRV_SQCQ_PROP_CQ_DEPTH:
            info->value[0] = cq_info->depth;
            break;
        case DRV_SQCQ_PROP_CQE_SIZE:
            info->value[0] = cq_info->e_size;
            break;
        default:
            trs_err("Invalid type. (type=%u)\n", info->prop);
            return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t trs_sq_cq_query(uint32_t dev_id, struct halSqCqQueryInfo *info)
{
    int ret;

    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((dev_id >= TRS_DEV_NUM) || (info->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", dev_id, info->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((info->type == DRV_NORMAL_TYPE) &&
        ((info->prop == DRV_SQCQ_PROP_SQ_HEAD) || (info->prop == DRV_SQCQ_PROP_SQ_TAIL) ||
         (info->prop == DRV_SQCQ_PROP_SQ_CQE_STATUS) || (info->prop == DRV_SQCQ_PROP_SQ_BASE) ||
         (info->prop == DRV_SQCQ_PROP_SQ_DEPTH) || (info->prop == DRV_SQCQ_PROP_SQE_SIZE) ||
         (info->prop == DRV_SQCQ_PROP_SQ_MEM_ATTR))) {
        ret = trs_sq_cq_query_uio(dev_id, info);
        if ((ret != DRV_ERROR_NOT_SUPPORT) || (info->prop == DRV_SQCQ_PROP_SQ_BASE)) {
            return ret;
        }
    }

    if (((info->type == DRV_NORMAL_TYPE) || (info->type == DRV_LOGIC_TYPE)) &&
        ((info->prop == DRV_SQCQ_PROP_CQ_DEPTH) || (info->prop == DRV_SQCQ_PROP_CQE_SIZE))) {
        ret = trs_cq_info_query(dev_id, info);
        if (ret == DRV_ERROR_NONE) {
            return ret;
        }
    }

    ret = trs_dev_io_ctrl(dev_id, TRS_SQCQ_QUERY, info);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Ioctl failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static void trs_query_sq_head(uint32_t dev_id, uint32_t ts_id, uint32_t sq_id, uint32_t *sq_head)
{
    struct halSqCqQueryInfo info;

    info.type = DRV_NORMAL_TYPE;
    info.tsId = ts_id;
    info.sqId = sq_id;
    info.prop = DRV_SQCQ_PROP_SQ_HEAD;
    if (halSqCqQuery(dev_id, &info) == 0) {
        *sq_head = info.value[0];
    }
}

static void trs_update_sq_head(uint32_t dev_id, uint32_t ts_id, uint32_t sq_id, struct sqcq_usr_info *sq_info)
{
    if (sq_info->sq_ctrl.head != NULL) {
        sq_info->head = trs_get_sq_head(sq_info);
    } else {
        trs_query_sq_head(dev_id, ts_id, sq_id, &sq_info->head);
    }
}

static inline uint32_t trs_sq_get_credit(struct sqcq_usr_info *sq_info)
{
    if (sq_info->tail >= sq_info->head) {
        return sq_info->depth - (sq_info->tail - sq_info->head + 1);
    } else {
        return sq_info->head - sq_info->tail - 1;
    }
}

static inline void trs_sq_send_full_inc(struct sqcq_usr_info *sq_info)
{
    if (sq_info->sq_ctrl.shr_info != NULL) {
        sq_info->sq_ctrl.shr_info->send_full++;
    }
}

static inline bool trs_sq_has_specified_num_task(struct sqcq_usr_info *sq_info, uint32_t num)
{
    return (((trs_get_sq_head(sq_info) + num) % sq_info->depth) == sq_info->tail);
}

static inline void trs_soft_que_prefetch(struct sqcq_usr_info *sq_info, uint32_t sqe_num)
{
    uint32_t i;

    for (i = 0; i < sqe_num; i++) {
        uint8_t *dst = (uint8_t *)sq_info->sq_ctrl.que_addr + ((sq_info->tail + i) % sq_info->depth) * sq_info->e_size;
        __builtin_prefetch(dst, 0, 1);
    }

    __builtin_prefetch(sq_info->sq_ctrl.tail, 0, 3); /* 3 cache temporal locality */
}

static drvError_t trs_sq_credit_check(uint32_t dev_id, struct halTaskSendInfo *info, struct sqcq_usr_info *sq_info)
{
    uint32_t credit;

    credit = trs_sq_get_credit(sq_info);
    if (credit < info->sqe_num) {
        trs_update_sq_head(dev_id, info->tsId, info->sqId, sq_info);
        credit = trs_sq_get_credit(sq_info);
    }

    if (credit < info->sqe_num) {
        trs_run_info_limit("No idle credit. (credit=%u; sqe_num=%u; devid=%u; sq_id=%u; head=%u; tail=%u; depth=%u)\n",
                           credit, info->sqe_num, dev_id, info->sqId, sq_info->head, sq_info->tail, sq_info->depth);
        trs_sq_send_full_inc(sq_info);
        return DRV_ERROR_NO_RESOURCES;
    }

    return DRV_ERROR_NONE;
}

drvError_t trs_sq_task_send_check(uint32_t dev_id, struct halTaskSendInfo *info, struct sqcq_usr_info *sq_info)
{
    if (trs_get_sq_ctrl_flag(sq_info, info) == TRS_SQ_CTRL_BY_USER_FLAG) {
        uint32_t user_ctrl_sq_tail;

        if (((uintptr_t)info->sqe_addr - (uintptr_t)sq_info->sq_ctrl.que_addr) % sq_info->e_size != 0) {
            trs_err("Sqe addr not aligned. (sqe_addr=0x%llx; align=%u; dev_id=%u; ts_id=%u; sq_id=%u)\n",
                    (u64)(uintptr_t)info->sqe_addr, sq_info->e_size, dev_id, info->tsId, info->sqId);
            return DRV_ERROR_INVALID_VALUE;
        }

        user_ctrl_sq_tail = (uint32_t)(((uintptr_t)info->sqe_addr - (uintptr_t)sq_info->sq_ctrl.que_addr) /
                                       sq_info->e_size);
        if (user_ctrl_sq_tail != sq_info->tail) {
            trs_err("Sqe addr not at sq tail. (dev_id=%u; ts_id=%u; sq_id=%u; user_ctrl_tail=%u; tail=%u)\n", dev_id,
                    info->tsId, info->sqId, user_ctrl_sq_tail, sq_info->tail);
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    return trs_sq_credit_check(dev_id, info, sq_info);
}

void trs_sq_task_fill(struct halTaskSendInfo *info, struct sqcq_usr_info *sq_info)
{
    uint32_t i;

    for (i = 0; i < info->sqe_num; i++) {
        uint8_t *dst = (uint8_t *)sq_info->sq_ctrl.que_addr + ((sq_info->tail + i) % sq_info->depth) * sq_info->e_size;
        uint8_t *src = info->sqe_addr + i * sq_info->e_size;
#if defined(CFG_SOC_PLATFORM_CLOUD_V4) && defined(CFG_SOC_PLATFORM_ESL_FPGA)
        uint32_t j;
        for (j = 0; j < sq_info->e_size; j++) {
            *(dst + j) = *(src + j);
        }
#else
        (void)memcpy_s(dst, sq_info->e_size, src, sq_info->e_size);
#endif
    }
}

static void trs_sq_task_send_set_db(struct sqcq_usr_info *sq_info, uint32_t sq_id, uint32_t sqe_num)
{
    if (sq_info->sq_ctrl.tail != NULL) {
        trs_set_sq_tail(sq_info, sq_info->tail);
    }

    if (trs_is_sq_use_soft_que(sq_info)) {
        if (trs_sq_has_specified_num_task(sq_info, sqe_num)) {
            /* set sqid to db for fast send in kernel(no which sq need send), it may be rewrite by other thread */
            trs_set_sq_db(sq_info, sq_id);
        }
    } else {
        trs_set_sq_db(sq_info, sq_info->tail);
    }

    trs_sq_send_ok_stat(sq_info, sqe_num);
}

static drvError_t trs_sq_task_send_uio(uint32_t dev_id, struct halTaskSendInfo *info, struct sqcq_usr_info *sq_info)
{
    drvError_t ret;

    if (sq_info->status == 0) {
        trs_err("Invalid status. (dev_id=%u; sq_id=%u; status=%u)\n", dev_id, info->sqId, sq_info->status);
        return DRV_ERROR_STATUS_FAIL;
    }

    if (sq_info->depth == 0U) {
        trs_err("sq info depth is 0. (dev_id=%u; ts_id=%u; sq_id=%u)\n", dev_id, info->tsId, info->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = trs_sq_task_send_check(dev_id, info, sq_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (trs_is_sq_use_soft_que(sq_info)) {
        trs_soft_que_prefetch(sq_info, info->sqe_num);
    }

    if (trs_get_sq_ctrl_flag(sq_info, info) == TRS_SQ_CTRL_BY_TRS_FLAG) {
        trs_sq_task_fill(info, sq_info);
    }

    info->pos = sq_info->tail;
    sq_info->tail = (sq_info->tail + info->sqe_num) % sq_info->depth;

    trs_sq_task_send_set_db(sq_info, info->sqId, info->sqe_num);

    return 0;
}

drvError_t trs_sq_task_send(uint32_t dev_id, struct halTaskSendInfo *info, struct sqcq_usr_info *sq_info)
{
    drvError_t ret;
    if (trs_is_sq_surport_uio(sq_info)) {
#ifdef CFG_FEATURE_SQ_SEND_LOCK
        (void)pthread_mutex_lock(&sq_info->sq_send_mutex);
#endif
        ret = trs_sq_task_send_uio(dev_id, info, sq_info);
#ifdef CFG_FEATURE_SQ_SEND_LOCK
        (void)pthread_mutex_unlock(&sq_info->sq_send_mutex);
#endif
        return ret;
    }

    if ((info->type == DRV_CALLBACK_TYPE) && (trs_is_stars_inst(dev_id, info->tsId))) {
        return trs_cb_event_submit(dev_id, (char *)info->sqe_addr, 40); /* 40 is event msg len */
    } else {
        return trs_dev_io_ctrl(dev_id, TRS_SQCQ_SEND, info);
    }
}

drvError_t trs_sq_switch_stream_batch(uint32_t dev_id, struct sq_switch_stream_info *info, uint32_t num)
{
    struct trs_sq_switch_stream_para para;
    drvError_t ret;
    uint32_t i;

    para.info = info;
    para.num = num;
    ret = trs_dev_io_ctrl(dev_id, TRS_SQ_SWITCH_STREAM, &para);
    if (ret != 0) {
        trs_err("Ioctl failed. (dev_id=%u; num=%u; ret=%d)\n", dev_id, num, ret);
        return ret;
    }

    if (trs_get_sq_send_mode(dev_id) != TRS_MODE_TYPE_SQ_SEND_HIGH_PERFORMANCE) {
        return DRV_ERROR_NONE;
    }

    for (i = 0; i < num; i++) {
        struct sqcq_usr_info *sq_info = NULL;
        uint32_t stream_id = info[i].stream_id;

        sq_info = trs_get_sq_info(dev_id, 0, DRV_NORMAL_TYPE, info[i].sq_id);
        if (sq_info == NULL) {
            trs_err("Get sq_info failed. (dev_id=%u; sq_id=%u)\n", dev_id, info[i].sq_id);
            return DRV_ERROR_INVALID_VALUE;
        }

        sq_info->depth = info[i].sq_depth;
        sq_info->switch_stream_flag = (stream_id != UINT32_MAX) ? 1 : 0;
        sq_info->stream_id = stream_id;

        /* init stream usr info when first bind sq */
        if ((stream_id != UINT32_MAX) && (trs_get_res_id_info(dev_id, 0, DRV_STREAM_ID, stream_id) == NULL)) {
            struct res_id_info_val id_info_val = {0};

            id_info_val.res_addr = (uintptr_t)info[i].stream_mem;
            id_info_val.res_len = (info[i].sq_depth * TRS_HW_SQE_SIZE);
            ret = trs_res_id_info_init(dev_id, 0, stream_id, DRV_STREAM_ID, &id_info_val);
            if (ret != 0) {
                trs_err("Failed to init stream id info. (dev_id=%u; id=%u)\n", dev_id, stream_id);
                return ret;
            }
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t trs_stream_task_fill(uint32_t dev_id, uint32_t stream_id, void *stream_mem, void *task_info,
                                uint32_t task_cnt)
{
    drvError_t ret = 0;

    if (trs_get_sq_send_mode(dev_id) == TRS_MODE_TYPE_SQ_SEND_HIGH_PERFORMANCE) {
        struct res_id_usr_info *stream_usr_info = NULL;

        stream_usr_info = trs_get_res_id_info(dev_id, 0, DRV_STREAM_ID, stream_id);
        if ((stream_usr_info == NULL) || (stream_usr_info->valid == 0)) {
            trs_err("Not init or null addr. (devid=%u; id=%u; addr_is_null=%d)\n", dev_id, stream_id,
                    (stream_usr_info == NULL) ? 1 : 0);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (((uintptr_t)stream_mem < stream_usr_info->res_addr) ||
            ((uintptr_t)stream_mem + task_cnt * TRS_HW_SQE_SIZE) >
                (stream_usr_info->res_addr + stream_usr_info->res_len)) {
            trs_err("Fill task exceed stream queue. (devid=%u; streamid=%u; res_addr=0x%llx; queue_len=0x%x; "
                    "stream_mem=0x%llx; task_cnt=%u)\n",
                    dev_id, stream_id, stream_usr_info->res_addr, stream_usr_info->res_len,
                    (uint64_t)(uintptr_t)stream_mem, task_cnt);
            return DRV_ERROR_NO_RESOURCES;
        }

        ret = drvMemcpy((uintptr_t)stream_mem, task_cnt * TRS_HW_SQE_SIZE, (uintptr_t)task_info,
                        task_cnt * TRS_HW_SQE_SIZE);
        if (ret != DRV_ERROR_NONE) {
            trs_err("Memcpy failed. (ret=%d).\n", ret);
            return DRV_ERROR_INNER_ERR;
        }
    } else {
        struct trs_stream_task_para para = {0};

        para.stream_id = stream_id;
        para.stream_mem = stream_mem;
        para.task_info = task_info;
        para.task_cnt = task_cnt;
        ret = trs_dev_io_ctrl(dev_id, TRS_STREAM_TASK_FILL, &para);
        if (ret != DRV_ERROR_NONE) {
            trs_err("Ioctl failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }
    }

    trs_debug("Fill stream task success. (dev_id=%u; stream_id=%u; task_cnt=%u)\n", dev_id, stream_id, task_cnt);
    return DRV_ERROR_NONE;
}

drvError_t trs_cq_report_recv(uint32_t dev_id, struct halReportRecvInfo *info)
{
    struct sqcq_usr_info *cq_info = NULL;

    if (info->type == DRV_CALLBACK_TYPE) {
        return trs_cb_event_wait(dev_id, info->cqId, info->timeout, info->cqe_addr);
    } else {
        cq_info = trs_get_cq_info(dev_id, info->tsId, info->type, info->cqId);
        if (cq_info == NULL) {
            trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; cq_id=%u)\n", dev_id, info->tsId, info->type,
                    info->cqId);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (!trs_is_cq_support_recv(cq_info)) {
            trs_warn("Cq is only id type, not support recv. (dev_id=%u; ts_id=%u; cq_id=%u; flag=%u)\n", dev_id,
                     info->tsId, info->cqId, cq_info->flag);
            return DRV_ERROR_NOT_SUPPORT;
        }

        info->report_cqe_num = 0;
        return trs_dev_io_ctrl(dev_id, TRS_SQCQ_RECV, info);
    }
}

drvError_t halCqReportRecv(uint32_t devId, struct halReportRecvInfo *info)
{
    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (info->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u)\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    info->res[0] = 1; /* version 1 */

    return trs_cq_report_recv(devId, info);
}

drvError_t halSqMemGet(uint32_t devId, struct halSqMemGetInput *in, struct halSqMemGetOutput *out)
{
    struct sqcq_usr_info *sq_info = NULL;
    uint32_t credit;

    if ((in == NULL) || (out == NULL)) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (in->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", devId, in->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    sq_info = trs_get_sq_info(devId, in->tsId, in->type, in->sqId);
    if (sq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", devId, in->tsId, in->type, in->sqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!trs_is_sq_support_send(sq_info)) {
        trs_warn("Sq is only id type, not support mem get. (dev_id=%u; ts_id=%u; sq_id=%u; flag=%u)\n", devId, in->tsId,
                 in->sqId, sq_info->flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

    credit = sq_info->max_num;
    if (trs_is_sq_surport_uio(sq_info)) {
        credit = trs_sq_get_credit(sq_info);
        if (credit == 0) {
            trs_update_sq_head(devId, in->tsId, in->sqId, sq_info);
            credit = trs_sq_get_credit(sq_info);
        }

        if (credit == 0) {
            return DRV_ERROR_OUT_OF_CMD_SLOT;
        }
    }

    out->cmdPtr = (volatile void *)sq_info->buf;
    out->cmdCount = (in->cmdCount < sq_info->max_num) ? in->cmdCount : sq_info->max_num;
    out->pos = sq_info->pos;

    sq_info->cur_num = out->cmdCount;

    return DRV_ERROR_NONE;
}

drvError_t halSqMsgSend(uint32_t devId, struct halSqMsgInfo *info)
{
    struct sqcq_usr_info *sq_info = NULL;
    struct halTaskSendInfo send_info;
    int ret;

    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (info->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", devId, info->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    sq_info = trs_get_sq_info(devId, info->tsId, info->type, info->sqId);
    if (sq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; sq_id=%u; type=%d)\n", devId, info->tsId, info->sqId, info->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((!trs_is_sq_support_send(sq_info)) || (trs_is_sq_init_without_sq_mem(sq_info->flag))) {
        trs_warn("Sq is only id type, not support send. (dev_id=%u; ts_id=%u; sq_id=%u; flag=%u)\n", devId, info->tsId,
                 info->sqId, sq_info->flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (info->cmdCount != sq_info->cur_num) {
        trs_err("Invalid para. (dev_id=%u; sq_id=%u; cmd_count=%u; cur_num=%u)\n", devId, info->sqId, info->cmdCount,
                sq_info->cur_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    send_info.type = info->type;
    send_info.tsId = info->tsId;
    send_info.sqId = info->sqId;
    send_info.timeout = 3000; /* default: 3000 ms */
    send_info.sqe_addr = sq_info->buf;
    send_info.sqe_num = sq_info->cur_num;

    ret = trs_sq_task_send(devId, &send_info, sq_info);
    if ((ret == 0) && (sq_info->depth != 0U)) {
        sq_info->pos = (send_info.pos + send_info.sqe_num) % sq_info->depth; /* next get pos */
    }

    return ret;
}

static int trs_cq_recv(uint32_t dev_id, struct halReportInfoInput *in, struct halReportInfoOutput *out)
{
    struct trs_dev_ctx *dev_ctx = trs_get_dev_ctx(dev_id);
    (void)out;
    struct halReportRecvInfo recv_info;
    struct sqcq_usr_info *cq_info = NULL;
    int ret;
    uint32_t cq_id;

    cq_id = (in->type == DRV_NORMAL_TYPE) ? dev_ctx->ts_ctx[in->tsId].normal_cq : in->grpId;
    cq_info = trs_get_cq_info(dev_id, in->tsId, in->type, cq_id);
    if (cq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; cq_id=%u)\n", dev_id, in->tsId, in->type, in->grpId);
        return DRV_ERROR_INVALID_VALUE;
    }

    recv_info.type = in->type;
    recv_info.tsId = in->tsId;
    recv_info.cqId = (in->type == DRV_NORMAL_TYPE) ? dev_ctx->ts_ctx[in->tsId].normal_cq : in->grpId;
    recv_info.timeout = in->timeout;
    recv_info.cqe_addr = cq_info->buf;
    recv_info.cqe_num = cq_info->max_num;
    recv_info.stream_id = (uint32_t)-1;
    recv_info.task_id = (uint32_t)-1;
    recv_info.res[0] = 0; /* version 0 */

    ret = trs_cq_report_recv(dev_id, &recv_info);
    if (ret != 0) {
        trs_warn("Recv warn. (dev_id=%u; type=%d; ret=%d)\n", dev_id, in->type, ret);
        return ret;
    }

    cq_info->cur_num = recv_info.report_cqe_num;

    return DRV_ERROR_NONE;
}

static int trs_cb_cq_recv(uint32_t dev_id, struct halReportInfoInput *in, struct halReportInfoOutput *out)
{
    struct halReportRecvInfo recv_info;
    struct trs_cb_cqe cqe;
    struct sqcq_usr_info *cq_info = NULL;
    uint32_t i;
    int ret;

    if (out->cqIdBitmap == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    recv_info.type = in->type;
    recv_info.tsId = in->tsId;
    recv_info.cqId = in->grpId;
    recv_info.timeout = in->timeout;
    recv_info.cqe_addr = (void *)&cqe;
    recv_info.cqe_num = 1;

    ret = halCqReportRecv(dev_id, &recv_info);
    if (ret != 0) {
        trs_warn("Recv warn. (dev_id=%u; type=%d; ret=%d)\n", dev_id, in->type, ret);
        return ret;
    }

    if ((cqe.cq_id / 64) >= out->cqIdBitmapSize) { /* u64 has 64 bit, echo bit one cq */
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; cq_id=%u; cq_id_bitmap_size=%u)\n", dev_id, in->tsId,
                in->type, cqe.cq_id, out->cqIdBitmapSize);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < out->cqIdBitmapSize; i++) {
        out->cqIdBitmap[i] = 0;
    }

    cq_info = trs_get_cq_info(dev_id, in->tsId, in->type, cqe.cq_id);
    if (cq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; cq_id=%u)\n", dev_id, in->tsId, in->type, cqe.cq_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    *(struct trs_cb_cqe *)cq_info->buf = cqe;
    cq_info->cur_num = 1;
    out->cqIdBitmap[cqe.cq_id / 64] |= (0x01ULL << (cqe.cq_id % 64)); /* 64 bit */

    return DRV_ERROR_NONE;
}

drvError_t halCqReportIrqWait(uint32_t devId, struct halReportInfoInput *in, struct halReportInfoOutput *out)
{
    if ((in == NULL) || (out == NULL)) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (in->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", devId, in->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (in->type == DRV_CALLBACK_TYPE) {
        return trs_cb_cq_recv(devId, in, out);
    } else {
        return trs_cq_recv(devId, in, out);
    }
}

drvError_t halCqReportGet(uint32_t devId, struct halReportGetInput *in, struct halReportGetOutput *out)
{
    struct sqcq_usr_info *cq_info = NULL;

    if ((in == NULL) || (out == NULL)) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (in->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", devId, in->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    cq_info = trs_get_cq_info(devId, in->tsId, in->type, in->cqId);
    if (cq_info == NULL) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; cq_id=%u)\n", devId, in->tsId, in->type, in->cqId);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!trs_is_cq_support_recv(cq_info)) {
        trs_warn("Cq is only id type, not support report get. (dev_id=%u; ts_id=%u; cq_id=%u; flag=%u)\n", devId,
                 in->tsId, in->cqId, cq_info->flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

    out->reportPtr = cq_info->buf;
    out->count = cq_info->cur_num;

    return DRV_ERROR_NONE;
}

drvError_t halReportRelease(uint32_t devId, struct halReportReleaseInfo *info)
{
    struct sqcq_usr_info *cq_info = NULL;

    if (info == NULL) {
        trs_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devId >= TRS_DEV_NUM) || (info->tsId >= TRS_TS_NUM)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u)\n", devId, info->tsId);
        return DRV_ERROR_INVALID_VALUE;
    }

    cq_info = trs_get_cq_info(devId, info->tsId, info->type, info->cqId);
    if (cq_info == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!trs_is_cq_support_recv(cq_info)) {
        trs_warn("Cq is only id type, not support release. (dev_id=%u; ts_id=%u; cq_id=%u; flag=%u)\n", devId,
                 info->tsId, info->cqId, cq_info->flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((info->count == 0) || (info->count != cq_info->cur_num)) {
        trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; cq_id=%u; count=%u; cur_num=%u)\n", devId, info->tsId,
                info->type, info->cqId, info->count, cq_info->cur_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    cq_info->cur_num = 0;

    return DRV_ERROR_NONE;
}

drvError_t trs_async_dma_desc_create(uint32_t dev_id, struct halAsyncDmaInputPara *in,
                                     struct halAsyncDmaOutputPara *out)
{
    int ret;

    if (in->type != DRV_NORMAL_TYPE) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (trs_get_sq_send_mode(dev_id) == TRS_MODE_TYPE_SQ_SEND_HIGH_PERFORMANCE) {
        unsigned long long sq_base_addr, dst_addr;
        struct sqcq_usr_info *sq_info = NULL;

        sq_info = trs_get_sq_info(dev_id, in->tsId, in->type, in->info.sq_id);
        if (sq_info == NULL) {
            trs_err("Invalid para. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, in->tsId, in->type,
                    in->info.sq_id);
            return DRV_ERROR_INVALID_VALUE;
        }

        sq_base_addr = trs_get_sq_bind_que_addr(dev_id, in->tsId, sq_info);
        if (sq_base_addr == 0) {
            trs_err("Get sq addr failed. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, in->tsId, in->type,
                    in->info.sq_id);
            return DRV_ERROR_INVALID_VALUE;
        }

        dst_addr = sq_base_addr + (uint64_t)in->info.sqe_pos * sq_info->e_size;
        out->dma_addr.offsetAddr.devid = dev_id; /* sqe update h2d only, use dev addr devid */
        ret = drvMemConvertAddr((uintptr_t)in->src, dst_addr, in->len, &out->dma_addr);
        if (ret != 0) {
            trs_err("Convert dma failed. (dev_id=%u; ts_id=%u; type=%d; sq_id=%u)\n", dev_id, in->tsId, in->type,
                    in->info.sq_id);
            return ret;
        }
    } else { /* check and convert dma desc in kernel when high security mode */
        struct trs_cmd_dma_desc para = {0};
        para.tsid = in->tsId;
        para.type = in->type;
        para.src = (void *)in->src;
        para.sq_id = in->info.sq_id;
        para.sqe_pos = in->info.sqe_pos;
        para.len = in->len;
        para.dir = in->dir;
        ret = trs_dev_io_ctrl(dev_id, TRS_DMA_DESC_CREATE, &para);
        if (ret != DRV_ERROR_NONE) {
            if (ret != DRV_ERROR_NOT_SUPPORT) {
                trs_err("Ioctl failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            }
            return ret;
        }

        out->dma_addr.phyAddr.src = (void *)(uintptr_t)para.dma_base;
        out->dma_addr.phyAddr.len = para.dma_node_num;
    }

    return DRV_ERROR_NONE;
}

drvError_t trs_async_dma_destory(uint32_t dev_id, struct halAsyncDmaDestoryPara *para)
{
    int ret;

    if ((para->dma_addr == NULL) || (para->type != DRV_NORMAL_TYPE)) {
        trs_err("Invalid para. (dev_id=%u; type=%d)\n", dev_id, para->type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (trs_get_sq_send_mode(dev_id) == TRS_MODE_TYPE_SQ_SEND_HIGH_PERFORMANCE) {
        ret = drvMemDestroyAddr(para->dma_addr);
        if (ret != 0) {
            trs_err("Destroy dma desc failed. (dev_id=%u; type=%d; sq_id=%u)\n", dev_id, para->type, para->sqId);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

void drvDfxShowReport(uint32_t devId)
{
    (void)devId;
}

static void *g_backup_sq_mem[TRS_DEV_NUM] = {NULL};
static int trs_sq_info_back_up(u32 dev_id)
{
    size_t sq_info_size;

    sq_info_size = sizeof(struct sqcq_usr_info) * cqcq_ctxs[dev_id][0][0].sq_num;
    if (g_bp_sq_info[dev_id].sq_info == NULL) {
        g_bp_sq_info[dev_id].sq_info = malloc(sq_info_size);
        if (g_bp_sq_info[dev_id].sq_info == NULL) {
            trs_err("Fail to malloc sq_info. (dev_id=%u)\n", dev_id);
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    (void)memcpy_s(g_bp_sq_info[dev_id].sq_info, sq_info_size, cqcq_ctxs[dev_id][0][0].sq_info, sq_info_size);
    g_bp_sq_info[dev_id].sq_num = cqcq_ctxs[dev_id][0][0].sq_num;
    return DRV_ERROR_NONE;
}

drvError_t trsMemProcResBackup(halProcResBackupInfo *info)
{
    drvError_t ret;
    uint32_t dev_num, dev_id;

    (void)info;
    ret = drvGetDevNum(&dev_num);
    if (ret != 0) {
        trs_err("Failed to get device num. (ret=%d)\n", ret);
        return ret;
    }

    for (dev_id = 0; dev_id < dev_num; dev_id++) {
        ret = trs_sq_info_back_up(dev_id);
        if (ret != 0) {
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

static int trs_flush_user_sq(uint32_t dev_id, uint32_t sq_id, struct sqcq_usr_info *info)
{
    struct sqcq_usr_info *bp_info = trs_get_sq_bp_ctx(dev_id, sq_id);

    if (bp_info == NULL) {
        trs_err("Flash sq fail. (dev_id=%u; sq_id=%u)\n", dev_id, sq_id);
        return DRV_ERROR_NO_RESOURCES;
    }
    info->tail = bp_info->tail;
    info->cur_num = bp_info->cur_num;
    return DRV_ERROR_NONE;
}

static int trs_sq_backup_uio(uint32_t dev_id, struct stream_backup_info *in)
{
    struct sqcq_usr_info *sq_info = NULL;
    void *que_va;
    u32 sq_id;
    int ret, i;

    for (i = 0; i < (int)in->id_num; i++) {
        sq_id = in->id_list[i];
        sq_info = trs_get_sq_info(dev_id, 0, DRV_NORMAL_TYPE, sq_id);
        if (sq_info == NULL) {
            trs_err("Invalid sq id. (dev_id=%u; sq_id=%u)\n", dev_id, sq_id);
            return DRV_ERROR_NO_RESOURCES;
        }
        if (sq_info->tail == 0) {
            continue;
        }
        que_va = (char *)g_backup_sq_mem[dev_id] + (uint64_t)sq_id * sq_info->e_size * sq_info->depth;
        if (sq_info->sq_que_spec_addr == sq_info->sq_ctrl.que_addr) {
            ret = drvMemcpy((DVdeviceptr)(uintptr_t)que_va, (uint64_t)sq_info->e_size * sq_info->depth,
                            (DVdeviceptr)(uintptr_t)sq_info->sq_ctrl.que_addr,
                            (uint64_t)sq_info->e_size * sq_info->tail);
        } else {
            ret = memcpy_s(que_va, (uint64_t)sq_info->e_size * sq_info->depth, sq_info->sq_ctrl.que_addr,
                           (uint64_t)sq_info->e_size * sq_info->tail);
        }
        if (ret != 0) {
            trs_err("Memcpy fail. (ret=%d; dev_id=%u; sq_id=%u)\n", ret, dev_id, sq_id);
            return DRV_ERROR_INNER_ERR;
        }
    }
    ret = trs_sq_info_back_up(dev_id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    trs_debug("Sq backup success with uio. (dev_id=%u; id_num=%u)\n", dev_id, in->id_num);
    return DRV_ERROR_NONE;
}

static int trs_get_common_sq_info(uint32_t dev_id, struct stream_backup_info *in, struct sqcq_usr_info **sq_info)
{
    uint32_t sq_id, i, sqe_size = 0, sq_depth = 0;

    for (i = 0; i < in->id_num; i++) {
        sq_id = in->id_list[i];
        *sq_info = trs_get_sq_info(dev_id, 0, DRV_NORMAL_TYPE, sq_id);
        if ((*sq_info) == NULL) {
            trs_err("Invalid sq type or id. (dev_id=%u; sq_id=%u)\n", dev_id, sq_id);
            return DRV_ERROR_INVALID_VALUE;
        }
        if (!trs_is_sq_surport_uio((*sq_info))) {
            return DRV_ERROR_NOT_SUPPORT;
        }
        if (i == 0) {
            sqe_size = (*sq_info)->e_size;
            sq_depth = (*sq_info)->depth;
        } else {
            if (((*sq_info)->e_size != sqe_size) || ((*sq_info)->depth != sq_depth)) {
                trs_err("Invalid sq info. (sqid=%u; sqe_size=%u; sq_depth=%u)\n", sq_id, (*sq_info)->e_size,
                        (*sq_info)->depth);
                return DRV_ERROR_INVALID_VALUE;
            }
        }
    }
    return DRV_ERROR_NONE;
}

static int trs_sq_backup(uint32_t dev_id, struct stream_backup_info *in)
{
    struct sqcq_usr_info *sq_info = NULL;
    uint64_t sq_mem_size;
    uint32_t sq_num;
    int ret;

    sq_num = trs_get_sq_num(dev_id, 0, DRV_NORMAL_TYPE);
    if (in->id_num > sq_num) {
        trs_err("Invalid id_num. (id_num=%u; sq_num=%u)\n", in->id_num, sq_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = trs_get_common_sq_info(dev_id, in, &sq_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (g_backup_sq_mem[dev_id] == NULL) {
        sq_mem_size = (uint64_t)sq_num * sq_info->e_size * sq_info->depth;
        g_backup_sq_mem[dev_id] = malloc(sq_mem_size);
        if (g_backup_sq_mem[dev_id] == NULL) {
            trs_err("Fail to malloc va. (dev_id=%u)\n", dev_id);
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    ret = trs_sq_backup_uio(dev_id, in);
    if (ret != 0) {
        trs_err("Failed to backup sq. (ret=%d; dev_id=%u)\n", ret, dev_id);
        free(g_backup_sq_mem[dev_id]);
        g_backup_sq_mem[dev_id] = NULL;
    }
    return ret;
}

drvError_t halStreamBackup(uint32_t dev_id, struct stream_backup_info *in)
{
    int ret;

    if ((dev_id >= TRS_DEV_NUM) || (in == NULL) || (in->id_num == 0) || (in->id_list == NULL)) {
        trs_err("Invalid para. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (in->type) {
        case DRV_RESOURCE_SQ_ID:
            ret = trs_sq_backup(dev_id, in);
            break;
        default:
            trs_err("Invalid type. (dev_id=%u; type=%d)\n", dev_id, in->type);
            return DRV_ERROR_INVALID_VALUE;
    }
    return ret;
}

drvError_t __attribute__((weak)) trs_sq_task_send_with_urma(uint32_t dev_id, uint32_t sqe_num,
                                                            struct sqcq_usr_info *sq_info)
{
    (void)dev_id;
    (void)sqe_num;
    (void)sq_info;
    return DRV_ERROR_NOT_SUPPORT;
}

static int trs_sq_restore_uio(uint32_t dev_id, struct stream_backup_info *in)
{
    struct sqcq_usr_info *sq_info = NULL;
    int ret, i, connection_type;
    uint32_t sq_id;
    void *que_va = NULL;

    connection_type = trs_get_connection_type(dev_id);
    for (i = 0; i < (int)in->id_num; i++) {
        sq_id = in->id_list[i];
        sq_info = trs_get_sq_info(dev_id, 0, DRV_NORMAL_TYPE, sq_id);
        if (sq_info == NULL) {
            trs_err("Fail to get sq info. (dev_id=%u; sq_id=%u)\n", dev_id, sq_id);
            return DRV_ERROR_NO_RESOURCES;
        }
        ret = trs_flush_user_sq(dev_id, sq_id, sq_info);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
        if (sq_info->tail == 0) {
            continue;
        }
        que_va = (char *)g_backup_sq_mem[dev_id] + (uint64_t)sq_id * sq_info->e_size * sq_info->depth;
        if (sq_info->sq_que_spec_addr == sq_info->sq_ctrl.que_addr) {
            ret = drvMemcpy((DVdeviceptr)(uintptr_t)sq_info->sq_ctrl.que_addr,
                            (uint64_t)sq_info->e_size * sq_info->depth, (DVdeviceptr)(uintptr_t)que_va,
                            (uint64_t)sq_info->tail * sq_info->e_size);
        } else {
            ret = memcpy_s(sq_info->sq_ctrl.que_addr, (uint64_t)sq_info->e_size * sq_info->depth, que_va,
                           (uint64_t)sq_info->tail * sq_info->e_size);
        }
        if (ret != 0) {
            trs_err("Memcpy fail. (dev_id=%u; sq_id=%u)\n", dev_id, sq_id);
            return DRV_ERROR_INNER_ERR;
        }
        if (connection_type == TRS_CONNECT_PROTOCOL_UB) {
            uint32_t sq_tail = sq_info->tail;
            sq_info->tail = 0;
            ret = trs_sq_task_send_with_urma(dev_id, sq_tail, sq_info);
            if (ret != 0) {
                sq_info->tail = sq_tail;
                trs_err("Failed to send task with urma. (ret=%d; dev_id=%u; sq_id=%u)\n", ret, dev_id, sq_id);
                return ret;
            }
            sq_info->tail = sq_tail;
            trs_sq_send_ok_stat(sq_info, sq_info->tail);
        } else {
            trs_sq_task_send_set_db(sq_info, sq_id, sq_info->tail);
        }
    }
    trs_debug("Sq restore success with uio. (dev_id=%u; id_num=%u; connection_type=%d)\n", dev_id, in->id_num,
              connection_type);
    return DRV_ERROR_NONE;
}

static int trs_sq_restore(uint32_t dev_id, struct stream_backup_info *in)
{
    struct sqcq_usr_info *sq_info = NULL;
    uint32_t sq_num;
    int ret;

    if (g_backup_sq_mem[dev_id] == NULL) {
        trs_err("Backup fail. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    sq_num = trs_get_sq_num(dev_id, 0, DRV_NORMAL_TYPE);
    if (in->id_num > sq_num) {
        trs_err("Invalid id_num. (id_num=%u; sq_num=%u)\n", in->id_num, sq_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = trs_get_common_sq_info(dev_id, in, &sq_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = trs_sq_restore_uio(dev_id, in);
    if (ret != DRV_ERROR_NONE) {
        trs_err("Failed to restore sq. (ret=%d; dev_id=%u)\n", ret, dev_id);
    }
    return ret;
}

drvError_t halStreamRestore(uint32_t dev_id, struct stream_backup_info *in)
{
    int ret;

    if ((dev_id >= TRS_DEV_NUM) || (in == NULL) || (in->id_num == 0) || (in->id_list == NULL)) {
        trs_err("Invalid para. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    switch (in->type) {
        case DRV_RESOURCE_SQ_ID:
            ret = trs_sq_restore(dev_id, in);
            break;
        default:
            trs_err("Invalid type. (dev_id=%u; type=%d)\n", dev_id, in->type);
            return DRV_ERROR_INVALID_VALUE;
    }
    return ret;
}
