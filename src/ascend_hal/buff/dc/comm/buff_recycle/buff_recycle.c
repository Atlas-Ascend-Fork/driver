/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define _GNU_SOURCE
#include <sched.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <semaphore.h>

#include "ascend_hal_define.h"

#include "drv_user_common.h"
#include "drv_buff_common.h"
#include "drv_buff_adp.h"
#include "drv_usr_buff_mempool.h"
#include "drv_buff_common_mempool.h"
#include "drv_buff_memzone.h"
#include "drv_buff_mbuf.h"
#include "bitmap.h"
#include "buff_range.h"
#include "buff_manage_base.h"
#include "buff_recycle_ctx.h"
#include "buff_recycle.h"
#include "buff_platform.h"

#define ATOMIC_SET(x, y) __sync_lock_test_and_set((x), (y))

typedef void (*block_handle)(struct proc_mng_ctx *p_mng, struct block_mem_ctx *block);
typedef void (*block_handle_with_rec_type)(struct block_mem_ctx *block, enum buff_recycle_type recycle_type);

#ifdef EMU_ST
void buff_restore_proc_para(int pid, int task_id);
#endif

static void block_handle_mov_to_alloc_list(struct proc_mng_ctx *p_mng, struct block_mem_ctx *block)
{
    drv_user_list_del(&block->node);
    drv_user_list_add_head(&block->node, &p_mng->alloc_list[block->type]);
}

static void block_handle_idle_free(struct proc_mng_ctx *p_mng, struct block_mem_ctx *block)
{
    if (block->valid == 0) {
        buff_debug("block %p free\n", block->mng);
        drv_user_list_del(&block->node);
        free(block);
        buff_api_atomic_dec(&p_mng->free_num);
    }
}

static void mbuf_recycle(struct proc_mng_ctx *p_mng, struct mempool_t *mp, struct share_mbuf *s_mbuf,
                         struct uni_buff_head_t *head)
{
    struct uni_buff_ext_info *ext = (struct uni_buff_ext_info *)((char *)head - sizeof(struct uni_buff_ext_info));
    unsigned long long proc_uid;

    proc_uid = ext->process_uni_id;

    /* check user proc is exit */
    if (p_mng->exit_task_id != proc_uid) {
        return;
    }

    if (ext->use_pid == getpid()) {
        return;
    }

    buff_event("Mbuf recycle. (pid=%d, mbuf=%p, alloc_pid=%d, use_pid=%d, proc_uid=%llu)\n", buff_get_current_pid(),
               s_mbuf, ext->alloc_pid, ext->use_pid, proc_uid);

    (void)s_mbuf_free((void *)mp, s_mbuf);
}

static void block_handle_mbuf_mp_recycle(struct proc_mng_ctx *p_mng, struct block_mem_ctx *block)
{
    struct mempool_t *mp = (struct mempool_t *)block->mng;
    struct uni_buff_head_t *head = NULL;
    void *buff = NULL;
    unsigned long i;

    if (block->status == 0) {
        return;
    }

    for (i = 0; i < mp->bit_num; i++) {
        i = bitmap_find_next_bit(mp->bitmap, mp->bit_num, i);
        if (i >= mp->blk_num) {
            break;
        }

        buff = mp_get_buff_uva_by_index(mp, i);
        head = buff_mempool_get_head(buff);
        if (head->status == UNI_STATUS_ALLOC) {
            if ((head->buff_type == MBUF_BY_POOL) || (head->buff_type == MBUF_NORMAL) ||
                (head->buff_type == MBUF_BARE_BUFF) || (head->buff_type == MBUF_BY_BUILD)) {
                mbuf_recycle(p_mng, mp, (struct share_mbuf *)buff, head);
            }
        }
    }
}

static void mp_scan_recycle_one_buff(struct proc_mng_ctx *p_mng, void *buff, uint32_t blk_id)
{
    struct uni_buff_head_t *head = NULL;
    struct uni_buff_trace_t *trace = NULL;
    int i;

    head = buff_mempool_get_head(buff);
    trace = buff_mempool_get_trace(buff);

    for (i = 0; i < BUFF_REF_MAX_MBUF_NUM; i++) {
        if (trace->mbuf[i] == NULL) {
            continue;
        }
        if ((uint32)p_mng->exit_task_id != trace->alloc_uid[i]) {
            continue;
        }
        if ((uint32)p_mng->exit_task_id != trace->proc_uid[i]) {
            continue;
        }
        (void)ATOMIC_SET(&(trace->mbuf[i]), NULL);
        (void)buff_free(blk_id, buff, head);
    }
}

static void mp_scan_recycle_buff(struct proc_mng_ctx *p_mng, struct mempool_t *mp)
{
    void *buff = NULL;
    unsigned long i;

    for (i = 0; i < mp->bit_num; i++) {
        i = bitmap_find_next_bit(mp->bitmap, mp->bit_num, i);
        if (i >= mp->blk_num) {
            break;
        }

        buff = mp_get_buff_uva_by_index(mp, i);
        mp_scan_recycle_one_buff(p_mng, buff, mp->blk_id);
    }
}

static void block_handle_mp_recycle(struct proc_mng_ctx *p_mng, struct block_mem_ctx *block)
{
    struct mempool_t *mp = (struct mempool_t *)block->mng;
    (void)pthread_mutex_lock(&block->mutex);
    if (block->status == 1) {
        mp_scan_recycle_buff(p_mng, mp);
    }
    (void)pthread_mutex_unlock(&block->mutex);
}

static void block_handle_mp_scan_free(struct block_mem_ctx *block, enum buff_recycle_type recycle_type)
{
    struct mempool_t *mp = (struct mempool_t *)block->mng;
    bool recycle_flag = false;

    (void)pthread_mutex_lock(&block->mutex);
    if (block->status == 1) {
        mp_scan_free_all_buff(mp);
        if (block->type == MEMPOOL_MBUF_LIST) {
            recycle_flag = true;
        }
    }
    (void)pthread_mutex_unlock(&block->mutex);
    if ((recycle_type == ACTIVE_RECYCLE) && (recycle_flag == true)) {
        mp_destroy_mbuf_mp(mp);
    }
}

static void block_handle_mz_scan_free(struct block_mem_ctx *block, enum buff_recycle_type recycle_type)
{
    struct buff_memzone_list_node *mz_list_node = NULL;
    struct memzone_user_mng_t *mz = NULL;

    (void)pthread_mutex_lock(&block->mutex);
    if (block->status == 1) {
        mz = (struct memzone_user_mng_t *)block->mng;
        mz_list_node = mz->mz_list_node;
        (void)memzone_scan_free(mz);
    }
    (void)pthread_mutex_unlock(&block->mutex);
    if (mz != NULL) {
        if (recycle_type == ACTIVE_RECYCLE) {
            memzone_scan_free_idle_pool(mz_list_node, mz);
        }
    }
}

static void block_handle_huge_buf_scan_free(struct block_mem_ctx *block, enum buff_recycle_type recycle_type)
{
    (void)recycle_type;
    struct memzone_huge_user_mng_t *huge_mz = (struct memzone_huge_user_mng_t *)block->mng;
    struct uni_buff_head_t *head = NULL;
    int head_status = UNI_STATUS_IDLE;

    (void)pthread_mutex_lock(&block->mutex);
    if (block->status == 1) {
        head = (struct uni_buff_head_t *)huge_mz->uni_head;
        head_status = head->status;
    }
    (void)pthread_mutex_unlock(&block->mutex);

    if (head_status == UNI_STATUS_RELEASE) {
        huge_buff_scan_free(huge_mz, head + 1);
    }
}

static void block_list_for_each_handle(struct proc_mng_ctx *p_mng, struct list_head *head, block_handle handle)
{
    struct block_mem_ctx *block = NULL;
    struct list_head *pos = NULL, *n = NULL;

    list_for_each_safe(pos, n, head)
    {
        block = list_entry(pos, struct block_mem_ctx, node);
        handle(p_mng, block);
    }
}

static void block_list_for_each_handle_with_devid(struct list_head *head, block_handle_with_rec_type handle,
                                                  uint32 devid, enum buff_recycle_type recycle_type)
{
    struct block_mem_ctx *block = NULL;
    struct list_head *pos = NULL, *n = NULL;

    list_for_each_safe(pos, n, head)
    {
        block = list_entry(pos, struct block_mem_ctx, node);
        if ((devid != BUFF_INVALID_DEV) && (devid != block->devid)) {
            continue;
        }
        handle(block, recycle_type);
    }
}

/*lint -e629 */
static void proc_block_check_free(struct proc_mng_ctx *p_mng)
{
    int i;

    if (buff_api_atomic_read(&p_mng->free_num) == 0) {
        return;
    }

    for (i = 0; i < MEM_LIST_NUM; i++) {
        block_list_for_each_handle(p_mng, &p_mng->alloc_list[i], block_handle_idle_free);
    }
}

static void exit_task_recycle(struct proc_mng_ctx *p_mng)
{
    do {
        int ret = buff_pool_poll_exit_task(p_mng->pool_id, &p_mng->exit_task_id);
        if (ret != DRV_ERROR_NONE) {
            break;
        }

        block_list_for_each_handle(p_mng, &p_mng->alloc_list[MEMPOOL_MBUF_LIST], block_handle_mbuf_mp_recycle);
        block_list_for_each_handle(p_mng, &p_mng->alloc_list[MEMPOOL_LIST], block_handle_mp_recycle);
    } while (true);
}

static void recycle_proc_default_pool_res(struct proc_mng_ctx *p_mng, uint32_t devid,
                                          enum buff_recycle_type recycle_type)
{
#ifdef EMU_ST
    buff_restore_proc_para(p_mng->pid, p_mng->task_id);
#endif
    /* 1. Move the block from the temporary list to the alloc list. For the performance of proc_block_add */
    (void)pthread_mutex_lock(&p_mng->alloc_list_mutex);
    (void)pthread_mutex_lock(&p_mng->alloc_tmp_list_mutex);
    block_list_for_each_handle(p_mng, &p_mng->alloc_list_tmp, block_handle_mov_to_alloc_list);
    (void)pthread_mutex_unlock(&p_mng->alloc_tmp_list_mutex);

    /* 2 check and free the block that has been delete by call proc_block_del */
    proc_block_check_free(p_mng);

    /* 3. The user's PID is recorded in Mbuf head, Recycle mbuf and it's data block if user exits . */
    exit_task_recycle(p_mng);

    /* 4. if the buf is set to unused, scan and free it here. */
    block_list_for_each_handle_with_devid(&p_mng->alloc_list[MEMZONE_LIST], block_handle_mz_scan_free, devid,
                                          recycle_type);
    block_list_for_each_handle_with_devid(&p_mng->alloc_list[HUGE_BUF_LIST], block_handle_huge_buf_scan_free, devid,
                                          recycle_type);
    block_list_for_each_handle_with_devid(&p_mng->alloc_list[MEMPOOL_LIST], block_handle_mp_scan_free, devid,
                                          recycle_type);
    block_list_for_each_handle_with_devid(&p_mng->alloc_list[MEMPOOL_MBUF_LIST], block_handle_mp_scan_free, devid,
                                          recycle_type);
    (void)pthread_mutex_unlock(&p_mng->alloc_list_mutex);
}

#define USING_BUFF_MAX_SHOW_CNT 10
drvError_t buff_proc_cache_free(uint32_t devid)
{
    struct buff_recycle_ctx_mng *ctx_mng = buff_get_recycle_ctx_mng();
    struct buff_recycle_ctx *ctx = NULL;
    struct list_head *pos = NULL, *n = NULL;

    (void)pthread_rwlock_rdlock(&ctx_mng->rwlock);
    list_for_each_safe(pos, n, &ctx_mng->head)
    {
        ctx = container_of(pos, struct buff_recycle_ctx, node);
        /* Proc without alloc permission has no memory to free. */
        if (ctx->p_mng.pool_id >= 0) {
            recycle_proc_default_pool_res(&ctx->p_mng, devid, ACTIVE_RECYCLE);
        }
    }
    (void)pthread_rwlock_unlock(&ctx_mng->rwlock);

    return idle_buff_range_free(devid, USING_BUFF_MAX_SHOW_CNT, true);
}

STATIC void thread_prop_set(void)
{
    buff_recycle_set_thread_affinity();
    (void)prctl(PR_SET_NAME, "buff_recycle");
}

#ifdef EMU_ST
static void free_one_buff(struct proc_mng_ctx *p_mng, struct block_mem_ctx *block)
{
    if (block->status == 1) {
        if (block->type == MEMZONE_LIST) {
            memzone_delete(block->mng);
        } else if (block->type == HUGE_BUF_LIST) {
            struct memzone_huge_user_mng_t *huge_mz = (struct memzone_huge_user_mng_t *)block->mng;
            void *buff = (struct uni_buff_head_t *)huge_mz->uni_head + 1;
            (void)memzone_free_huge(huge_mz, buff);
        } else {
            struct buff_req_mp_destroy arg;
            arg.mp = block->mng;
            (void)buff_usr_mp_delete(&arg);
        }
    }
}

/* free malloc memory, avoid detected memory leaks */
static void free_all_buff(struct proc_mng_ctx *p_mng)
{
    int i;

    block_list_for_each_handle(p_mng, &p_mng->alloc_list_tmp, block_handle_mov_to_alloc_list);

    for (i = 0; i < MEM_LIST_NUM; i++) {
        block_list_for_each_handle(p_mng, &p_mng->alloc_list[i], free_one_buff);
    }

    proc_block_check_free(p_mng);
}

void list_handle_of_recycle_ctx_destroy(struct list_head *node)
{
    struct buff_recycle_ctx *ctx = container_of(node, struct buff_recycle_ctx, node);

    buff_info("pid %d recycle thread stop thread status %d\n", buff_get_current_pid(), ctx->status);

    if (ctx->p_mng.pool_id >= 0) {
        recycle_proc_default_pool_res(&ctx->p_mng, BUFF_INVALID_DEV, PASSIVE_RECYCLE);
        free_all_buff(&ctx->p_mng);
    }

    drv_user_list_del(&ctx->node);
    free(ctx);
}

/* this function is for ut-test only */
void recycle_thread_stop(void)
{
    struct buff_recycle_ctx_mng *ctx_mng = buff_get_recycle_ctx_mng();

    if (ctx_mng->ctx_num != 0) {
        (void)pthread_rwlock_wrlock(&ctx_mng->rwlock);
        drv_user_list_for_each(&ctx_mng->head, list_handle_of_recycle_ctx_destroy);
        (void)pthread_rwlock_unlock(&ctx_mng->rwlock);
    }

    del_others_range();
}
#endif

void wake_up_recyle_thread(int pool_id)
{
    struct buff_recycle_ctx *ctx = NULL;

    ctx = buff_get_recycle_ctx(pool_id);
    if (ctx == NULL) {
        return;
    }

    if ((ctx->wait_flag == 1) && (ctx->wake_up_cnt == 0)) {
        ctx->wake_up_cnt++;
    }
}

static void *recycle_thread(void *arg)
{
#ifndef EMU_ST
    struct buff_recycle_ctx *ctx = (struct buff_recycle_ctx *)arg;
    thread_prop_set();
    ctx->status = RECYCLE_THREAD_STATUS_RUN;
    (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    while (ctx->status == RECYCLE_THREAD_STATUS_RUN) {
        if (ctx->p_mng.pool_id >= 0) {
            recycle_proc_default_pool_res(&ctx->p_mng, BUFF_INVALID_DEV, PASSIVE_RECYCLE);
        }
        (void)idle_buff_range_free(BUFF_INVALID_DEV, 0, false);
        (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        (void)usleep(ctx->period);
        (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    }
    (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    ctx->status = RECYCLE_THREAD_STATUS_STOP;
#endif
    return NULL;
}

static void recycle_thread_init(struct buff_recycle_ctx *ctx)
{
    pthread_attr_t attr;
    pthread_t thread;
    int ret;

    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    ret = pthread_create(&thread, &attr, recycle_thread, (void *)ctx);
    if (ret != 0) {
        buff_err("Buff recycle thread create fail. (ret=%d)\n", ret);
    } else {
        ctx->thread = thread;
    }
    (void)pthread_attr_destroy(&attr);
}

static void mem_config_show(unsigned int cfg_id)
{
    memZoneCfg *cfg_info = memzone_get_cfg_info_by_id(cfg_id);

    buff_show("Mz cfg. (cfg_id=%d; total_size=0x%llx; blk_size=0x%x; max_buf_size=0x%llx; page_type=%d)\n",
              cfg_info->cfg_id, cfg_info->total_size, cfg_info->blk_size, cfg_info->max_buf_size, cfg_info->page_type);
}

static void memzone_buff_show(struct memzone_user_mng_t *mz)
{
    void *start = NULL, *end = NULL;
    struct uni_buff_ext_info *ext_info = NULL;
    struct uni_buff_head_t *head = NULL;
    unsigned long i, size;
    unsigned long j = 0;

    (void)pthread_mutex_lock(&mz->mutex);
    for (i = 0; i < mz->bitnum;) {
        if ((mz->alloc_flag == 1) || (mz->free_flag == 1)) {
            break;
        }

        i = bitmap_find_next_bit(mz->bitmap, mz->bitnum, i);
        if (i >= mz->blk_num_total) {
            break;
        }

        head = (struct uni_buff_head_t *)(uintptr_t)*memzone_get_buff_head_by_index(mz, (unsigned int)i);
        if (head->status == UNI_STATUS_ALLOC) {
            ext_info = buff_get_ext_info(head, mz->area.blk_id);
            if (ext_info != NULL) {
                j++;
                buff_show("Buff. (index=%ld; ext=%p; alloc_pid=%d; use_pid=%d)\n", j, ext_info, ext_info->alloc_pid,
                          ext_info->use_pid);
                buff_put_ext_info(ext_info, mz->area.blk_id);
            }
        }

        start = (void *)((char *)head - (head->resv_head * UNI_RSV_HEAD_LEN));
        end = (void *)((char *)head + head->size);
        size = (unsigned long)end - (unsigned long)start;
        if ((size < mz->blk_size) || (size > mz->area.mz_mem_total_size)) {
            break;
        }
        i += size / mz->blk_size;
    }
    (void)pthread_mutex_unlock(&mz->mutex);
}

static void memzone_show(void *buff)
{
    struct memzone_user_mng_t *mz = (struct memzone_user_mng_t *)buff;

    buff_show("memzone %p: blk addr %p pid %d blk(size %d total num %d free %d), mem(total 0x%llx free 0x%llx)\n", mz,
              mz->area.mz_mem_uva, mz->pid, mz->blk_size, mz->blk_num_total, mz->blk_num_available,
              mz->area.mz_mem_total_size, mz->mz_mem_free_size);
    mem_config_show(mz->cfg_id);
    memzone_buff_show(mz);
}

static void mem_pool_buff_show(struct mempool_t *mp)
{
    struct uni_buff_ext_info *ext_info = NULL;
    struct uni_buff_head_t *head = NULL;
    void *buff = NULL;
    unsigned long i;
    unsigned long j = 0;

    for (i = 0; i < mp->bit_num; i++) {
        i = bitmap_find_next_bit(mp->bitmap, mp->bit_num, i);
        if (i >= mp->blk_num) {
            break;
        }

        buff = mp_get_buff_uva_by_index(mp, i);
        head = buff_mempool_get_head(buff);
        if (head->status == UNI_STATUS_ALLOC) {
            ext_info = buff_get_ext_info(head, mp->blk_id);
            if (ext_info != NULL) {
                j++;
                buff_show("Buff. (index=%ld; ext=%p; alloc_pid=%d; use_pid=%d)\n", j, ext_info, ext_info->alloc_pid,
                          ext_info->use_pid);
                buff_put_ext_info(ext_info, mp->blk_id);
            }
        }
    }
}

static void mem_pool_show(void *buff)
{
    struct mempool_t *mp = (struct mempool_t *)buff;

    buff_show("mempool %p: blk addr %p name(%s) blk(size %d total num %d free %d), bit_num %d\n", mp, mp->blk_start,
              mp->pool_name, mp->blk_size, mp->blk_num, mp->blk_available, mp->bit_num);
    mem_pool_buff_show(mp);
}

static void large_buf_show(void *buff)
{
    struct memzone_huge_user_mng_t *huge_mz = (struct memzone_huge_user_mng_t *)buff;
    struct uni_buff_head_t *head = (struct uni_buff_head_t *)huge_mz->uni_head;

    buff_show("Large. (buf=%p; size=0x%lx)\n", buff, (unsigned long)head->size);
}

static void (*const mem_show_handler[MEM_LIST_NUM])(void *buff) = {[MEMZONE_LIST] = memzone_show,
                                                                   [HUGE_BUF_LIST] = large_buf_show,
                                                                   [MEMPOOL_LIST] = mem_pool_show,
                                                                   [MEMPOOL_MBUF_LIST] = mem_pool_show};

static void block_handle_show(struct proc_mng_ctx *p_mng, struct block_mem_ctx *block)
{
    (void)p_mng;
    buff_show("block type %d status %d mng %p\n", block->type, block->status, block->mng);
    (void)pthread_mutex_lock(&block->mutex);
    if (block->status == 1) {
        mem_show_handler[block->type](block->mng);
    }
    (void)pthread_mutex_unlock(&block->mutex);
}
#ifndef EMU_ST
static void _pool_blk_show(struct proc_mng_ctx *p_mng)
{
    int i;

    buff_show("\n");
    buff_show("pid %d, task uid %llu, pool_id %d block_num %u free_num %u. block info:\n", p_mng->pid, p_mng->task_id,
              p_mng->pool_id, p_mng->block_num, p_mng->free_num);

    buff_show("alloc tmp list:\n");
    (void)pthread_mutex_lock(&p_mng->alloc_tmp_list_mutex);
    block_list_for_each_handle(p_mng, &p_mng->alloc_list_tmp, block_handle_show);
    (void)pthread_mutex_unlock(&p_mng->alloc_tmp_list_mutex);

    buff_show("alloc list:\n");
    (void)pthread_mutex_lock(&p_mng->alloc_list_mutex);
    for (i = 0; i < MEM_LIST_NUM; i++) {
        block_list_for_each_handle(p_mng, &p_mng->alloc_list[i], block_handle_show);
    }
    (void)pthread_mutex_unlock(&p_mng->alloc_list_mutex);
}

static void list_handle_of_pool_blk_show(struct list_head *node)
{
    struct buff_recycle_ctx *ctx = container_of(node, struct buff_recycle_ctx, node);

    if (ctx->p_mng.pool_id >= 0) {
        _pool_blk_show(&ctx->p_mng);
    }
}
#endif
static void pool_blk_show(void)
{
    struct buff_recycle_ctx_mng *ctx_mng = buff_get_recycle_ctx_mng();
#ifndef EMU_ST
    (void)pthread_rwlock_rdlock(&ctx_mng->rwlock);
    drv_user_list_for_each(&ctx_mng->head, list_handle_of_pool_blk_show);
    (void)pthread_rwlock_unlock(&ctx_mng->rwlock);
#endif
}

STATIC void sigal_proc(signed int signum)
{
#ifndef EMU_ST
    buff_show("****************** signum %d proc %d start ***********************\n", signum, buff_get_current_pid());
    buff_range_show();
    pool_blk_show();
    buff_show("****************** signum %d proc %d end ***********************\n", signum, buff_get_current_pid());
#endif
}

static void sigal_init(void)
{
    struct sigaction act;
    static THREAD bool is_inited = false;

    if (is_inited == true) {
        return;
    }

    act.sa_handler = sigal_proc;
    act.sa_flags = 0;
    (void)sigemptyset(&act.sa_mask);
    (void)sigaction(SIGUSR1, &act, NULL);
}

drvError_t proc_block_ctx_init(int pool_id, int type, void *mng, struct block_mem_ctx *block)
{
    struct common_handle_t *head = (struct common_handle_t *)mng;
    struct buff_recycle_ctx *ctx = NULL;
    int ret;

    ctx = buff_get_recycle_ctx(pool_id);
    if (ctx == NULL) {
        buff_err("buff_get_recycle_ctx failed\n");
        return DRV_ERROR_NO_RESOURCES;
    }

    block->valid = 1;
    block->type = type;
    block->status = 1;
    block->mng = mng;
    block->devid = head->devid;
    block->recycle_ctx = ctx;
    ret = pthread_mutex_init(&block->mutex, NULL);
    if (ret != 0) {
        buff_err("pthread_mutex_init failed\n");
        return DRV_ERROR_NO_RESOURCES;
    }
    return DRV_ERROR_NONE;
}

/* In the scenario where the cache pool is automatically released,
 * ensure that the head->parent is assigned before the linked list is added.
 * Otherwise, the head->parent may access invalid memory in concurrent scenarios.
 */
void proc_block_add(struct block_mem_ctx *block)
{
    struct buff_recycle_ctx *ctx = block->recycle_ctx;

    buff_api_atomic_inc(&ctx->p_mng.block_num);
    (void)pthread_mutex_lock(&ctx->p_mng.alloc_tmp_list_mutex);
    drv_user_list_add_head(&block->node, &ctx->p_mng.alloc_list_tmp);
    (void)pthread_mutex_unlock(&ctx->p_mng.alloc_tmp_list_mutex);
}

void proc_block_del(void *blk)
{
    struct block_mem_ctx *block = (struct block_mem_ctx *)blk;
    struct buff_recycle_ctx *ctx = block->recycle_ctx;

    /* avoid conflicts with the recycle thread, here just set invalid, the recycle thread deletes the block. */
    block->valid = 0;
    buff_api_atomic_dec(&ctx->p_mng.block_num);
    buff_api_atomic_inc(&ctx->p_mng.free_num);
}

void proc_set_buff_invalid(void *blk)
{
    struct block_mem_ctx *block = (struct block_mem_ctx *)blk;

    (void)pthread_mutex_lock(&block->mutex);
    block->status = 0;
    (void)pthread_mutex_unlock(&block->mutex);
}

/*lint +e629 */
void buf_recycle_init(int pool_id)
{
    struct buff_recycle_ctx *ctx = NULL;
    drvError_t ret;

    ret = buff_recycle_ctx_create(pool_id, &ctx);
    if (ret != DRV_ERROR_NONE) {
        return;
    }

    recycle_thread_init(ctx);
    sigal_init();
}

static void list_handle_of_recycle_exit(struct list_head *node)
{
    struct buff_recycle_ctx *ctx = container_of(node, struct buff_recycle_ctx, node);

    if (ctx->thread != 0) {
        ctx->status = RECYCLE_THREAD_STATUS_STOP;
        (void)pthread_cancel(ctx->thread);
        (void)pthread_join(ctx->thread, NULL);
        ctx->thread = 0;
    }
}

static void __attribute__((destructor)) buff_exit(void)
{
    struct buff_recycle_ctx_mng *ctx_mng = buff_get_recycle_ctx_mng();

    (void)pthread_rwlock_rdlock(&ctx_mng->rwlock);
    drv_user_list_for_each(&ctx_mng->head, list_handle_of_recycle_exit);
    (void)pthread_rwlock_unlock(&ctx_mng->rwlock);
}

drvError_t halBufferModeNotify(PSM_STATUS status, void *rsv)
{
    (void)status;
    (void)rsv;
    return DRV_ERROR_NOT_SUPPORT;
}