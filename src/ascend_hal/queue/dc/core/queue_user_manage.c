/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "drv_user_common.h"
#include "drv_buff_common.h"
#include "buff_manage_base.h"
#include "queue.h"
#include "que_compiler.h"
#include "buff_mng.h"
#include "buff_user_interface.h"

#include "queue_interface.h"
#include "queue_user_manage.h"

#include "queue_kernel_api.h"

#define QUE_MEM_BASE_ADDR_PROP_NAME "que_mem_base_addr"
#define QUE_MEM_BASE_ADDR_MASTER "que_mem_base_addr_master"
#define SINGLE_QUE_MEM_SIZE que_align_up(sizeof(struct queue_manages), 128)
#define QUE_MEM_SIZE (SINGLE_QUE_MEM_SIZE * MAX_SURPORT_QUEUE_NUM)
#define SINGLE_MERGE_MEM_SIZE que_align_up(sizeof(struct group_merge), 128) /* cache line */
#define MERGE_MEM_SIZE (SINGLE_MERGE_MEM_SIZE * MAX_SURPORT_QUEUE_NUM)
#define QUE_ENTITY_NODE_SIZE sizeof(queue_entity_node)

#define SUBSCRIBE_MAX_GRP_NUM 32U
#define MAX_WAITE_TIME 0xffffffffffffffff

#define QUEUE_BASE_ADDR_MAX_MASTER_TIME 3000 /* wait for 3s */

#define QUE_THREAD_SLEEP_INTERVAL (100000) // 100ms

enum queue_thread_status {
    QUE_THREAD_IDLE = 0,
    QUE_THREAD_RUN,
    QUE_THREAD_STOP,
};

static THREAD int g_que_thread_status = (int)QUE_THREAD_IDLE;
static THREAD pthread_t g_queue_mng_thread = 0;

pthread_mutex_t merge_mutex = PTHREAD_MUTEX_INITIALIZER;

THREAD struct group_merge *grp_merge[SUBSCRIBE_MAX_GRP_NUM];
THREAD struct group_merge_list grp_merge_list[SUBSCRIBE_MAX_GRP_NUM];
THREAD struct queue_merge_node merge_que[MAX_SURPORT_QUEUE_NUM];
THREAD struct queue_local_info g_que_local_info[MAX_SURPORT_QUEUE_NUM];

THREAD int que_mng_grp_id = -1;
THREAD void *que_mem_base = NULL;
THREAD pid_t que_cur_pid = -1;

struct que_chan_ctx_agent_list que_chan_ctx_agent = {NULL};

struct que_chan_ctx_agent_list *que_get_chan_ctx_agent(void)
{
    return &que_chan_ctx_agent;
}

void que_chan_cnt_info_print(unsigned int devid, unsigned int qid)
{
    if (que_chan_ctx_agent.que_chan_cnt_info_print == NULL) {
        return;
    }
    que_chan_ctx_agent.que_chan_cnt_info_print(devid, qid);
}

void que_ctx_cnt_info_print(unsigned int devid)
{
    if (que_chan_ctx_agent.que_ctx_cnt_info_print == NULL) {
        return;
    }
    que_chan_ctx_agent.que_ctx_cnt_info_print(devid);
}

static void que_mng_thread_init(void);

bool queue_is_init(void)
{
    return (que_mem_base != NULL);
}

static drvError_t queue_init_grp(void)
{
    que_mng_grp_id = buff_get_default_pool_id();
    return DRV_ERROR_NONE;
}

static void *queue_alloc_buf(unsigned long size, unsigned long flag, uint32_t *blk_id)
{
    return buff_blk_alloc(que_mng_grp_id, size, (flag | XSMEM_BLK_ALLOC_FROM_OS), blk_id);
}

static void queue_free_buf(void *addr)
{
    buff_blk_free(que_mng_grp_id, addr);
}

static drvError_t queue_buf_get(void *mem_base, uint64_t mem_size)
{
    drvError_t ret;
    int32_t pool_id = 0;
    void *alloc_ptr = NULL;
    uint64_t alloc_size = 0;
    uint32_t blk_id;

    ret = buff_blk_get(mem_base, &pool_id, &alloc_ptr, &alloc_size, &blk_id);
    if ((ret != 0) || (alloc_ptr != mem_base) || (alloc_size < mem_size)) {
        QUEUE_LOG_ERR("queue grp get error. (ret=%d; addr=0x%llx; baseaddr=0x%llx; size=%lu; base size=%lu)\n", ret,
                      (unsigned long long)(uintptr_t)alloc_ptr, (unsigned long long)(uintptr_t)mem_base,
                      (unsigned long)alloc_size, (unsigned long)mem_size);
        return DRV_ERROR_INNER_ERR;
    }
    return DRV_ERROR_NONE;
}

static drvError_t queue_set_prop(const char *prop_name, unsigned long value)
{
    return buff_pool_set_prop(que_mng_grp_id, prop_name, value);
}

static drvError_t queue_prop_get(const char *prop_name, unsigned long *value)
{
    return buff_pool_get_prop(que_mng_grp_id, prop_name, value);
}

static drvError_t queue_del_prop(const char *prop_name)
{
    return buff_pool_del_prop(que_mng_grp_id, prop_name);
}

static drvError_t queue_set_grp_prop(const char *prop_name, unsigned long value)
{
    return buff_pool_set_grp_prop(que_mng_grp_id, prop_name, value);
}

static drvError_t queue_prop_put(const char *prop_name)
{
    return buff_pool_del_prop(que_mng_grp_id, prop_name);
}

void *queue_get_que_addr(unsigned int qid)
{
    /*lint -e502 -e647*/
    return (void *)(uintptr_t)((uint64_t)(uintptr_t)que_mem_base + SINGLE_QUE_MEM_SIZE * qid);
    /*lint +e502 +e647*/
}

void *queue_get_merge_addr(int qid)
{
    /*lint -e502 -e647*/
    return (void *)(uintptr_t)((uint64_t)(uintptr_t)que_mem_base + QUE_MEM_SIZE +
                               SINGLE_MERGE_MEM_SIZE * (long unsigned int)qid);
    /*lint +e502 +e647*/
}

void queue_set_local_info(unsigned int qid, struct queue_local_info *local_info)
{
    (void)ATOMIC_SET(&g_que_local_info[qid].depth, local_info->depth);
    (void)ATOMIC_SET((uint64 *)(&g_que_local_info[qid].que_mng), (uint64)(uintptr_t)local_info->que_mng);
    (void)ATOMIC_SET((uint64 *)(&g_que_local_info[qid].entity), (uint64)(uintptr_t)local_info->entity);
    if (!atomic_ref_try_init(&g_que_local_info[qid].ref_status)) {
        queue_delete(qid, g_que_local_info[qid].entity);
        QUEUE_RUN_LOG_INFO("queue ref already init. (qid=%u)", qid);
    } else {
        g_que_local_info[qid].detached = false;
    }
}

void queue_clear_local_info(unsigned int qid)
{
    (void)ATOMIC_SET((uint64 *)(&g_que_local_info[qid].entity), (uint64)NULL);
    (void)ATOMIC_SET(&g_que_local_info[qid].depth, 0);
    (void)ATOMIC_SET((uint64 *)(&g_que_local_info[qid].que_mng), (uint64)NULL);
}

static void queue_detach(unsigned int qid)
{
    queue_entity_node *que_entity = g_que_local_info[qid].entity;
    queue_clear_local_info(qid);
    queue_delete(qid, (void *)que_entity); // must in last step
}

bool queue_is_valid(unsigned int qid)
{
    return atomic_ref_is_valid(&g_que_local_info[qid].ref_status);
}

static bool queue_local_ref_inc(unsigned int qid)
{
    return atomic_ref_inc(&g_que_local_info[qid].ref_status);
}

static void queue_local_ref_dec(unsigned int qid)
{
    if (!atomic_ref_dec(&g_que_local_info[qid].ref_status)) {
        return;
    }
    if (!atomic_ref_is_ready_clear(&g_que_local_info[qid].ref_status)) {
        return;
    }
    if (!atomic_ref_try_uninit(&g_que_local_info[qid].ref_status)) {
        return;
    }
    queue_detach(qid);
}

bool queue_get(unsigned int qid)
{
    return queue_local_ref_inc(qid);
}

void queue_put(unsigned int qid)
{
    (void)queue_local_ref_dec(qid);
}

struct queue_manages *queue_get_local_mng(unsigned int qid)
{
    return g_que_local_info[qid].que_mng;
}

unsigned int queue_get_local_depth(unsigned int qid)
{
    return g_que_local_info[qid].depth;
}

void queue_get_entity_and_depth(unsigned int qid, queue_entity_node **entity, unsigned int *depth)
{
    *entity = g_que_local_info[qid].entity;
    *depth = g_que_local_info[qid].depth;
}

bool queue_local_is_attached_and_set_detached(unsigned int qid)
{
    return CAS(&g_que_local_info[qid].detached, false, true);
}

bool queue_local_is_detached_and_se_attached(unsigned int qid)
{
    return CAS(&g_que_local_info[qid].detached, true, false);
}

void queue_set_local_depth(unsigned int qid, unsigned int depth)
{
    (void)ATOMIC_SET(&g_que_local_info[qid].depth, depth);
}

static void queue_format_addr_name(char *name, int len, unsigned int qid)
{
    if (sprintf_s(name, (unsigned int)len, "que_%u_addr", qid) <= 0) {
        QUEUE_LOG_WARN("sprintf_s not successful. (qid=%u)\n", qid);
    }
}

STATIC drvError_t queue_init_base_addr(void)
{
    unsigned long size = (unsigned long)QUE_MEM_SIZE + (unsigned long)MERGE_MEM_SIZE; /*lint !e502 !e647*/
    unsigned long value = 0;
    uint32_t blk_id;
    void *addr = NULL;
    int cnt = 0;
    drvError_t ret;

    ret = queue_prop_get(QUE_MEM_BASE_ADDR_PROP_NAME, &value);
    if ((ret == DRV_ERROR_NONE) && (queue_buf_get((void *)(uintptr_t)value, size) == DRV_ERROR_NONE)) {
        que_mem_base = (void *)(uintptr_t)value;
        return DRV_ERROR_NONE;
    }

    do {
        value = (unsigned long)(unsigned int)GETPID();
        ret = queue_set_prop(QUE_MEM_BASE_ADDR_MASTER, value);
        if (ret == DRV_ERROR_NONE) {
            addr = queue_alloc_buf(size, QUEUE_ALLOC_BUF_FLAG, &blk_id);
            if (addr == NULL) {
                (void)queue_del_prop(QUE_MEM_BASE_ADDR_MASTER);
                QUEUE_LOG_ERR("malloc buff size failed. (size=%lu)\n", size);
                return DRV_ERROR_OUT_OF_MEMORY;
            }

            (void)memset_s(addr, size, 0, size);

            value = (unsigned long)(uintptr_t)addr; // lint !e507
            ret = queue_set_grp_prop(QUE_MEM_BASE_ADDR_PROP_NAME, value);
            if (ret != DRV_ERROR_NONE) {
                queue_free_buf(addr);
                (void)queue_del_prop(QUE_MEM_BASE_ADDR_MASTER);
            }
        } else {
            ret = queue_prop_get(QUE_MEM_BASE_ADDR_PROP_NAME, &value);
            if ((ret != DRV_ERROR_NONE) || (queue_buf_get((void *)(uintptr_t)value, size) != DRV_ERROR_NONE)) {
                (void)usleep(1000); /* 1000 us */
                cnt++;
            }
        }
    } while ((ret != DRV_ERROR_NONE) && (cnt < QUEUE_BASE_ADDR_MAX_MASTER_TIME));

    if (cnt == QUEUE_BASE_ADDR_MAX_MASTER_TIME) {
        QUEUE_LOG_ERR("buff_get_prop cnt. (prop_name=%s; cnt=%d; ret=%d)\n", QUE_MEM_BASE_ADDR_PROP_NAME, cnt, ret);
        return DRV_ERROR_INNER_ERR;
    }

    que_mem_base = (void *)(uintptr_t)value;
    return DRV_ERROR_NONE;
}

STATIC void queue_show_stat(struct queue_manages *que_manage, unsigned int qid)
{
    int merge_idx = que_manage->merge_idx;
    char que_name[MAX_STR_LEN] = {0};
    unsigned long name_len;
    int ret;

    name_len = strnlen(que_manage->name, MAX_STR_LEN);
    if (name_len >= (unsigned long)MAX_STR_LEN) {
        QUEUE_LOG_ERR("queue name len is overlimit. (len=%ld, max=%d)\n", name_len, MAX_STR_LEN);
        return;
    }
    ret = strncpy_s(que_name, (unsigned long)(unsigned int)MAX_STR_LEN, que_manage->name, name_len);
    if (ret != 0) {
        QUEUE_LOG_ERR("queue name copy failed. (qid=%u, ret=%d)\n", que_manage->id, ret);
        return;
    }

    QUEUE_RUN_LOG_INFO("(qid=%u; creator_pid=%d;"
                       " producer_pid=%d; spec_thread=%d; tid=%u; inner_sub_flag=%u;"
                       " consumer_pid=%d; spec_thread=%d; tid=%u; inner_sub_flag=%u;"
                       " bind_type=%d; work_mode=%d; empty_flag=%d;"
                       " full_flag=%d; event_flag=%d;"
                       " head=%u; tail=%u; depth=%u;"
                       " over_write_flag=%d; fctl_flag=%d).\n",
                       que_manage->id, que_manage->creator_pid, que_manage->producer.pid,
                       que_manage->producer.spec_thread, que_manage->producer.tid, que_manage->producer.inner_sub_flag,
                       que_manage->consumer.pid, que_manage->consumer.spec_thread, que_manage->consumer.tid,
                       que_manage->consumer.inner_sub_flag, que_manage->bind_type, que_manage->work_mode,
                       que_manage->empty_flag, que_manage->full_flag, que_manage->event_flag,
                       que_manage->queue_head.head_info.head, queue_get_orig_tail(que_manage),
                       queue_get_local_depth(qid), que_manage->over_write, que_manage->fctl_flag);

    QUEUE_RUN_LOG_INFO("(enque_ok=%llu; enque_fail=%u; enque_drop=%lu;"
                       " deque_num=%llu; deque_ok=%llu; deque_fail=%u; deque_drop=%llu;"
                       " enque_full=%llu; deque_null=%u; deque_empty=%llu;"
                       " enque_event_ok=%llu; enque_event_fail=%llu; call_back=%llu;"
                       " enque_event_result=%d; enque_none_subscrib=%lu;"
                       " f2nf_event_ok=%llu; f2nf_event_fail=%llu)\n",
                       que_manage->stat.enque_ok, que_manage->stat.enque_fail, que_manage->stat.enque_drop,
                       que_manage->stat.deque_num, que_manage->stat.deque_ok, que_manage->stat.deque_fail,
                       que_manage->stat.deque_drop, que_manage->stat.enque_full, que_manage->stat.deque_null,
                       que_manage->stat.deque_empty, que_manage->stat.enque_event_ok, que_manage->stat.enque_event_fail,
                       que_manage->stat.call_back, que_manage->stat.enque_event_ret,
                       que_manage->stat.enque_none_subscrib, que_manage->stat.f2nf_event_ok,
                       que_manage->stat.f2nf_event_fail);

    if (queue_is_merge_idx_valid(merge_idx)) {
        struct group_merge *merge = queue_get_merge_addr(merge_idx);
        QUEUE_RUN_LOG_INFO("merge info. (pid=%d, grp_id=%u, ref=%u, atomic=%d, pause=%d evt_succ=%llu, evt_fail=%llu, "
                           "call_back=%llu, enque_event_result=%d)\n",
                           merge->pid, merge->groupid, merge->ref_cnt, merge->atomic_flag, merge->pause_flag,
                           merge->event_stat.user_event_succ, merge->event_stat.user_event_fail,
                           merge->event_stat.call_back, merge->enque_event_ret);
    }
}

/*lint -e629 */
static void queue_info_show(void)
{
    unsigned int qid;

    for (qid = 0; qid < MAX_SURPORT_QUEUE_NUM; qid++) {
        struct queue_manages *que_manage = NULL;

        que_manage = queue_get_local_mng(qid);
        if ((que_manage == NULL) || (que_manage->valid != QUEUE_CREATED)) {
            continue;
        }

        queue_show_stat(que_manage, qid);
        que_chan_cnt_info_print(que_manage->dev_id, qid);
        que_ctx_cnt_info_print(que_manage->dev_id);
    }
}

STATIC void queue_signal_proc(signed int signum)
{
    QUEUE_RUN_LOG_INFO("******************* queue_signal_proc start. (signum=%d) ***********************\n", signum);
    queue_info_show();
    QUEUE_RUN_LOG_INFO("******************* queue_signal_proc end. (signum=%d) ***********************\n", signum);
}

static void queue_signal_init(void)
{
    struct sigaction act;

    act.sa_handler = queue_signal_proc;
    act.sa_flags = 0;
    (void)sigemptyset(&act.sa_mask);
    (void)sigaction(SIGUSR2, &act, NULL);
}

drvError_t queue_dc_init(void)
{
    drvError_t ret;
    unsigned int i;

    ret = queue_init_grp();
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_ERR("queue init mem grp failed. (ret=%d)\n", ret);
        return ret;
    }

    que_mng_thread_init();

    ret = memset_s(g_que_local_info, sizeof(g_que_local_info), 0, sizeof(g_que_local_info));
    if (ret != 0) {
        QUEUE_LOG_ERR("queue local info memset failed. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    for (i = 0; i < SUBSCRIBE_MAX_GRP_NUM; i++) {
        grp_merge[i] = NULL;
    }

    for (i = 0; i < MAX_SURPORT_QUEUE_NUM; i++) {
        merge_que[i].groupid = -1;
        merge_que[i].list.prev = NULL;
        merge_que[i].list.next = NULL;
        g_que_local_info[i].detached = true;
    }

    que_cur_pid = GETPID();

    ret = queue_init_base_addr();
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    queue_signal_init();
    return DRV_ERROR_NONE;
}

static queue_entity_node *queue_entity_alloc(unsigned int depth, uint32_t *blk_id)
{
    unsigned long size = (unsigned long)depth * QUE_ENTITY_NODE_SIZE;
    queue_entity_node *addr = NULL;

    addr = (queue_entity_node *)queue_alloc_buf(size, 0, blk_id);
    if (addr == NULL) {
        QUEUE_LOG_ERR("queue alloc queue entity fail. (depth=%u)\n", depth);
        return NULL;
    }

    return addr;
}

static void queue_entity_free(void *que_entity)
{
    queue_free_buf(que_entity);
}

static drvError_t queue_id_alloc(void *que_entity, unsigned int *qid)
{
    static unsigned int last_id = 0;
    unsigned long value;
    unsigned int i = 0, id = 0;

    value = (unsigned long)(uintptr_t)que_entity;
    for (i = 0; i < MAX_SURPORT_QUEUE_NUM; i++) {
        char name[MAX_STR_LEN];
        drvError_t ret;

        id = (i + last_id) % MAX_SURPORT_QUEUE_NUM;
        queue_format_addr_name(name, MAX_STR_LEN, id);
        ret = queue_set_grp_prop((const char *)name, value);
        if (ret == DRV_ERROR_NONE) {
            last_id = id + 1;
            break;
        }
    }
    if (i >= MAX_SURPORT_QUEUE_NUM) {
        QUEUE_LOG_ERR("no valid qid. (qid=%u, max=%u)\n", id, (MAX_SURPORT_QUEUE_NUM - 1));
        return DRV_ERROR_OVER_LIMIT;
    }

    *qid = id;

    return DRV_ERROR_NONE;
}

static void queue_id_free(unsigned int qid)
{
    char name[MAX_STR_LEN];
    drvError_t ret;

    queue_format_addr_name(name, MAX_STR_LEN, qid);
    ret = queue_prop_put((const char *)name);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_WARN("delete prop name. (qid=%u; ret=%d)\n", qid, ret);
    }
}

drvError_t queue_create(unsigned int depth, unsigned int *qid, struct queue_local_info *local_info)
{
    struct queue_manages *que_mng = NULL;
    queue_entity_node *que_entity = NULL;
    unsigned int id;
    drvError_t ret;
    uint32_t blk_id;

    if (!queue_is_init()) {
        QUEUE_LOG_ERR("queue not init.\n");
        return DRV_ERROR_UNINIT;
    }

    que_entity = queue_entity_alloc(depth, &blk_id);
    if (que_entity == NULL) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = queue_id_alloc(que_entity, &id);
    if (ret != DRV_ERROR_NONE) {
        goto ERR_FREE_QUEUE_ENTITY;
    }

    que_mng = (struct queue_manages *)queue_get_que_addr(id);
    ret = memset_s((void *)que_mng, sizeof(struct queue_manages), 0, sizeof(struct queue_manages));
    if (ret != 0) {
        QUEUE_LOG_ERR("queue manage memset fail. (ret=%d)\n", ret);
        ret = DRV_ERROR_INNER_ERR;
        goto ERR_FREE_QUEUE_ID;
    }

    local_info->entity = que_entity;
    local_info->depth = depth;
    local_info->que_mng = que_mng;

    *qid = id;

    return DRV_ERROR_NONE;

ERR_FREE_QUEUE_ID:
    queue_id_free(id);
ERR_FREE_QUEUE_ENTITY:
    queue_entity_free(que_entity);

    return ret;
}

void queue_delete(unsigned int qid, void *que_entity)
{
    queue_entity_free(que_entity);
    queue_id_free(qid);
    QUEUE_RUN_LOG_INFO("queue detach success. (qid=%u)\n", qid);
}

static drvError_t queue_entity_get(void *entity, unsigned int *depth)
{
    void *alloc_ptr = NULL;
    uint64_t alloc_size = 0;
    int32_t pool_id = 0;
    drvError_t ret;
    uint32_t blk_id;

    ret = buff_blk_get(entity, &pool_id, &alloc_ptr, &alloc_size, &blk_id);
    if ((ret != DRV_ERROR_NONE) || (alloc_ptr != entity)) {
        QUEUE_LOG_ERR("queue grp get error. (ret=%d; addr=0x%llx; baseaddr=0x%llx; size=%lu; base size=%lu)\n", ret,
                      (unsigned long long)(uintptr_t)alloc_ptr, (unsigned long long)(uintptr_t)entity,
                      (unsigned long)alloc_size, sizeof(void *));
        return DRV_ERROR_INNER_ERR;
    }

    if ((alloc_size < (MIN_VALID_QUEUE_DEPTH * QUE_ENTITY_NODE_SIZE)) ||
        (alloc_size > (MAX_QUEUE_DEPTH * QUE_ENTITY_NODE_SIZE))) {
        QUEUE_LOG_ERR("entity size out of range. (alloc_size=%lu)\n", (unsigned long)alloc_size);
        return DRV_ERROR_INNER_ERR;
    }

    *depth = (unsigned int)(alloc_size / QUE_ENTITY_NODE_SIZE);
    return DRV_ERROR_NONE;
}

drvError_t queue_attach(unsigned int qid, int pid, int timeout, struct queue_local_info *local_info)
{
    (void)pid;
    (void)timeout;
    char name[MAX_STR_LEN];
    unsigned long entity;
    drvError_t ret, sub_ret;
    unsigned int depth;

    if (!queue_is_init()) {
        QUEUE_LOG_ERR("queue not init. (qid=%u)\n", qid);
        return DRV_ERROR_UNINIT;
    }

    queue_format_addr_name(name, MAX_STR_LEN, qid);
    ret = queue_prop_get((const char *)name, &entity);
    if (ret != 0) {
        return DRV_ERROR_INNER_ERR;
    }

    ret = queue_entity_get((void *)(uintptr_t)entity, &depth);
    if (ret != 0) {
        goto ERR_PROP_PUT;
    }

    local_info->que_mng = (struct queue_manages *)queue_get_que_addr(qid);
    local_info->entity = (void *)(uintptr_t)entity;
    local_info->depth = depth;

    return DRV_ERROR_NONE;

ERR_PROP_PUT:
    sub_ret = queue_prop_put((const char *)name);
    if (sub_ret != 0) {
        QUEUE_LOG_ERR("delete prop name. (qid=%u; ret=%d)\n", qid, sub_ret);
    }
    return ret;
}

static struct queue_manages *get_merge_grp_not_empty_que(struct group_merge *merge)
{
    struct queue_manages *que_manage = NULL;
    struct queue_merge_node *node = NULL;
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    unsigned int groupid = merge->groupid % SUBSCRIBE_MAX_GRP_NUM;

    get_atomic_lock(&grp_merge_list[groupid].lock, MAX_WAITE_TIME);
    list_for_each_safe(pos, n, &grp_merge_list[groupid].head)
    {
        node = list_entry(pos, struct queue_merge_node, list);
        que_manage = (struct queue_manages *)queue_get_local_mng(node->qid); // lint !e454 !e613
        if ((que_manage != NULL) && ((queue_get_orig_tail(que_manage) != que_manage->queue_head.head_info.head))) {
            release_atomic_lock(&grp_merge_list[groupid].lock);
            return que_manage;
        }
    }
    release_atomic_lock(&grp_merge_list[groupid].lock);

    return NULL; // lint !e454
}

STATIC void queue_resume_grp_event(uint32 devid, struct group_merge *merge)
{
    struct queue_manages *que_manage = NULL;
    ATOMIC_INC(&merge->event_stat.call_back);

    if (CAS(&merge->atomic_flag, 1, 0) == false) {
        QUEUE_LOG_WARN("cas not successful. (grp=%u; flag=%d)\n", merge->groupid, merge->atomic_flag);
    }

    que_manage = get_merge_grp_not_empty_que(merge);
    if (que_manage != NULL) {
        send_queue_event(devid, que_manage);
    }
}

static void queue_resume_single_queue_event(uint32 devid, struct queue_manages *que_manage)
{
    ATOMIC_INC(&que_manage->stat.call_back);

    if (CAS(&que_manage->event_flag, 1, 0) != true) {
        QUEUE_LOG_WARN("queue cas not successful. (qid=%u)\n", que_manage->id);
    }

    if (que_manage->queue_head.head_info.head != queue_get_orig_tail(que_manage)) {
        send_queue_event(devid, que_manage);
    }
}

void queue_finish_callback_local(unsigned int dev_id, unsigned int qid, unsigned int grp_id, unsigned int event_id)
{
    struct queue_manages *que_manage = NULL;
    struct group_merge *merge = NULL;

    if ((event_id != EVENT_QUEUE_ENQUEUE) || (grp_id >= SUBSCRIBE_MAX_GRP_NUM)) {
        QUEUE_LOG_ERR("event callback para invalid. (grp_id=%u, event_id=%u, qid=%u)\n", grp_id, event_id, qid);
        return;
    }

    if (qid >= MAX_SURPORT_QUEUE_NUM) {
        QUEUE_LOG_ERR("event callback qid invalid. (qid=%u, max=%u)\n", qid, MAX_SURPORT_QUEUE_NUM);
        return;
    }

    if (queue_is_init() == false) {
        QUEUE_LOG_ERR("not init. (grp_id=%u, event_id=%u, qid=%u)\n", grp_id, event_id, qid);
        return;
    }

    que_manage = (struct queue_manages *)queue_get_que_addr(qid);
    if (que_manage->valid != QUEUE_CREATED) {
        QUEUE_LOG_ERR("queue is not created. (qid=%u)\n", qid);
        return;
    }

    if (!queue_get(qid)) {
        QUEUE_LOG_ERR("queue get failed. (qid=%u)\n", qid);
        return;
    }

    merge = grp_merge[grp_id];

    /* For the client queue, the bind_type cannot be determined through merge,
     * because the gid of the client queue is on the host side and may be the same as the device side gid,
     * causing the merge to not be NULL. So check QUEUE_TYPE_SINGLE first.
     */
    if (que_manage->bind_type == QUEUE_TYPE_SINGLE) {
        queue_resume_single_queue_event(dev_id, que_manage);
    } else if (merge != NULL) {
        queue_resume_grp_event(dev_id, merge);
    } else {
        QUEUE_LOG_WARN("parameter invalid. (group_id=%u; qid=%u; bind_type=%d; valid=%d).\n", grp_id, qid,
                       que_manage->bind_type, que_manage->valid);
    }
    queue_put(qid);
}

STATIC void queue_format_merge_name(char *name, int len, unsigned int merge_id)
{
    if (sprintf_s(name, (unsigned long)(unsigned int)len, "que_%u_merge", merge_id) <= 0) {
        QUEUE_LOG_WARN("sprintf_s merge name. (merge_id=%u)\n", merge_id);
    }
}

STATIC void queue_merge_unlock(int merge_id)
{
    char name[MAX_STR_LEN];
    drvError_t ret;

    queue_format_merge_name(name, MAX_STR_LEN, (unsigned int)merge_id);
    ret = queue_del_prop((const char *)name);
    if (ret != DRV_ERROR_NONE) {
        QUEUE_LOG_WARN("delete merge prop. (merge_id=%d, ret=%d)\n", merge_id, ret);
    }
}

STATIC drvError_t queue_merge_lock(int merge_id)
{
    char name[MAX_STR_LEN];

    queue_format_merge_name(name, MAX_STR_LEN, (unsigned int)merge_id);
    return queue_set_prop((const char *)name, (unsigned long)(unsigned int)merge_id);
}

STATIC struct group_merge *queue_get_local_merge(int pid, unsigned int groupid, int *merge_id)
{
    struct group_merge *merge = NULL;
    drvError_t ret;
    int id;

    merge = grp_merge[groupid];
    if ((merge != NULL) && (merge->pid == pid) && (merge->ref_cnt > 0)) {
        id = merge->idx;
        if (merge != queue_get_merge_addr(id)) {
            return NULL;
        }

        ret = queue_merge_lock(id);
        if (ret == DRV_ERROR_NONE) {
            *merge_id = id;
            return merge;
        }
    }

    return NULL;
}

STATIC bool queue_merge_is_in_use(int merge_id)
{
    unsigned int qid;

    for (qid = 0; qid < MAX_SURPORT_QUEUE_NUM; qid++) {
        struct queue_manages *que_mng = (struct queue_manages *)queue_get_que_addr(qid);
        if (que_mng->valid == QUEUE_UNCREATED) {
            continue;
        }

        if (que_mng->merge_idx == merge_id) {
            return true;
        }
    }

    return false;
}

STATIC drvError_t queue_merge_id_alloc(int qid, int *merge_id)
{
    drvError_t ret;
    int i, id;

    for (i = 0; i < MAX_SURPORT_QUEUE_NUM; i++) {
        id = (i + qid) % MAX_SURPORT_QUEUE_NUM;

        if (queue_merge_is_in_use(id)) {
            continue;
        }

        ret = queue_merge_lock(id);
        if (ret != DRV_ERROR_NONE) {
            continue;
        }

        if (queue_merge_is_in_use(id)) {
            queue_merge_unlock(id);
            continue;
        }

        *merge_id = id;
        return DRV_ERROR_NONE;
    }

    QUEUE_LOG_ERR("no valid merge id. (qid=%d)\n", qid);

    return DRV_ERROR_OVER_LIMIT;
}

STATIC void queue_init_merge_info(struct group_merge *merge, int idx, int pid, unsigned int groupid)
{
    merge->idx = idx;
    merge->pid = pid;
    merge->groupid = groupid;
    merge->pause_flag = 0;
    merge->ref_cnt = 0;
    merge->atomic_flag = 0;
    merge->enque_event_ret = 0;
    merge->last_dequeue_time = 0;
    merge->event_stat.call_back = 0;
    merge->event_stat.user_event_fail = 0;
    merge->event_stat.user_event_succ = 0;

    INIT_LIST_HEAD(&grp_merge_list[groupid].head);
    init_atomic_lock(&grp_merge_list[groupid].lock);
}

STATIC void queue_merge_add_node(struct group_merge *merge, unsigned int groupid, unsigned int qid)
{
    get_atomic_lock(&grp_merge_list[groupid].lock, MAX_WAITE_TIME);
    if ((merge_que[qid].groupid >= 0) && (merge_que[qid].list.next != NULL) && (merge_que[qid].list.prev != NULL)) {
        drv_user_list_del(&merge_que[qid].list);
    }
    merge_que[qid].qid = qid;
    merge_que[qid].groupid = (int)groupid;
    drv_user_list_add_head(&merge_que[qid].list, &grp_merge_list[groupid].head);
    release_atomic_lock(&grp_merge_list[groupid].lock);

    ATOMIC_INC(&merge->ref_cnt);
}

drvError_t queue_merge_init(struct queue_manages *que_manage, unsigned int qid, int pid, unsigned int groupid)
{
    struct group_merge *merge = NULL;
    int merge_id, ret;

    if (groupid >= SUBSCRIBE_MAX_GRP_NUM) {
        QUEUE_LOG_ERR("groupid is invalid. (groupid=%u)\n", groupid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = (int)get_atomic_lock(&que_manage->merge_atomic_lock, ATOMIC_LOCK_TIME_DEFAULT);
    if (ret != ATOMIC_LOCK_ERROR_NONE) {
        QUEUE_LOG_ERR("apply the merge atomic lock error. (qid=%u, ret=%d)\n", qid, ret);
        return DRV_ERROR_INNER_ERR;
    }

    if (queue_is_merge_idx_valid(que_manage->merge_idx)) {
        release_atomic_lock(&que_manage->merge_atomic_lock);
        QUEUE_LOG_ERR("queue merge reinit. (qid=%u, pid=%d, groupid=%u)\n", qid, pid, groupid);
        return DRV_ERROR_REPEATED_SUBSCRIBED;
    }

    (void)pthread_mutex_lock(&merge_mutex);

    if (merge_que[qid].groupid >= 0) {
        QUEUE_RUN_LOG_INFO("queue is already subscribed. (qid=%u; sub_grp=%d; cur_grp=%u)\n", qid,
                           merge_que[qid].groupid, groupid);
    }

    merge = queue_get_local_merge(pid, groupid, &merge_id);
    if (merge == NULL) {
        ret = (int)queue_merge_id_alloc((int)qid, &merge_id);
        if (ret != DRV_ERROR_NONE) {
            (void)pthread_mutex_unlock(&merge_mutex);
            release_atomic_lock(&que_manage->merge_atomic_lock);
            QUEUE_LOG_ERR("no merge res. (groupid=%u, qid=%u)\n", groupid, qid);
            return (drvError_t)ret;
        }
        merge = queue_get_merge_addr(merge_id);
        queue_init_merge_info(merge, merge_id, pid, groupid);

        grp_merge[groupid] = merge;
    }

    queue_merge_add_node(merge, groupid, qid);
    que_manage->merge_idx = merge_id;

    queue_merge_unlock(merge_id);
    (void)pthread_mutex_unlock(&merge_mutex);
    release_atomic_lock(&que_manage->merge_atomic_lock);

    return DRV_ERROR_NONE;
}

drvError_t queue_merge_uninit(struct queue_manages *que_manage, unsigned int qid)
{
    struct group_merge *merge = NULL;
    int groupid, merge_id, ret;
    unsigned int ref = 0;

    ret = (int)get_atomic_lock(&que_manage->merge_atomic_lock, ATOMIC_LOCK_TIME_DEFAULT);
    if (ret != ATOMIC_LOCK_ERROR_NONE) {
        QUEUE_LOG_ERR("apply merge atomic lock error. (qid=%u, ret=%d)\n", qid, ret);
        return DRV_ERROR_INNER_ERR;
    }

    merge_id = que_manage->merge_idx;
    if (!queue_is_merge_idx_valid(merge_id)) {
        release_atomic_lock(&que_manage->merge_atomic_lock);
        QUEUE_LOG_ERR("queue merge not init. (qid=%u)\n", qid);
        return DRV_ERROR_UNINIT;
    }

    merge = queue_get_merge_addr(merge_id);

    /* unsubscribe by other proc, not need set proc para */
    if (que_manage->consumer.pid == que_cur_pid) {
        (void)pthread_mutex_lock(&merge_mutex);
        ref = ATOMIC_DEC(&merge->ref_cnt);
        groupid = merge_que[qid].groupid;
        merge_que[qid].groupid = -1;
        get_atomic_lock(&grp_merge_list[groupid].lock, MAX_WAITE_TIME);
        drv_user_list_del(&merge_que[qid].list);
        release_atomic_lock(&grp_merge_list[groupid].lock);
        if (ref == 0) {
            grp_merge[groupid] = NULL;
        }
        (void)pthread_mutex_unlock(&merge_mutex);
    } else {
        ATOMIC_DEC(&merge->ref_cnt);
    }

    que_manage->merge_idx = INVALID_QUEUE_MERGE_IDX;

    release_atomic_lock(&que_manage->merge_atomic_lock);

    return DRV_ERROR_NONE;
}

/*lint +e629 */
drvError_t queue_merge_ctrl(unsigned int dev_id, int pid, unsigned int groupid, QUE_EVENT_CMD cmd_type)
{
    struct group_merge *merge = NULL;
    struct queue_manages *que_mng = NULL;

    if (groupid >= SUBSCRIBE_MAX_GRP_NUM) {
        QUEUE_LOG_ERR("groupid is invalid. (groupid=%u)\n", groupid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pid == que_cur_pid) {
        merge = grp_merge[groupid];
    } else {
        QUEUE_LOG_ERR("invalid value. (pid=%d, group=%u)\n", pid, groupid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (merge == NULL) {
        QUEUE_LOG_ERR("not subscribe. (pid=%d; groupid=%u)\n", pid, groupid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (cmd_type == QUE_PAUSE_EVENT) {
        merge->pause_flag = 1;
    } else {
        merge->pause_flag = 0;

        que_mng = get_merge_grp_not_empty_que(merge);
        if (que_mng != NULL) {
            send_queue_event(dev_id, que_mng);
        }
    }

    return DRV_ERROR_NONE;
}

static void *que_mng_thread(void *arg)
{
    (void)arg;
    (void)prctl(PR_SET_NAME, "queue_mng");
    g_que_thread_status = QUE_THREAD_RUN;

    while (g_que_thread_status == QUE_THREAD_RUN) {
        queue_detach_invalid_queue();
        queue_run_log_flow_ctrl_cnt_clear();
        (void)usleep(QUE_THREAD_SLEEP_INTERVAL);
    }
    g_que_thread_status = QUE_THREAD_STOP;
    return NULL; /*lint !e527*/
}

static void que_mng_thread_init(void)
{
    pthread_attr_t attr;
    pthread_t thread;
    int ret;

    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    ret = pthread_create(&thread, &attr, que_mng_thread, (void *)&que_mng_grp_id);
    if (ret != 0) {
        QUEUE_LOG_ERR("queue mng thread create fail. (ret=%d)\n", ret);
    } else {
        g_queue_mng_thread = thread;
    }
    (void)pthread_attr_destroy(&attr);
}

static void __attribute__((destructor)) queue_exit(void)
{
    if (g_queue_mng_thread == 0) {
        return;
    }
    g_que_thread_status = QUE_THREAD_STOP;
    (void)pthread_cancel(g_queue_mng_thread);
    (void)pthread_join(g_queue_mng_thread, NULL);
}
