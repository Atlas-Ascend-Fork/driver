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

#ifndef TRS_PROC_H
#define TRS_PROC_H

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "pbl_kref_safe.h"

#include "pbl/pbl_task_ctx.h"

struct trs_core_ts_inst;

struct trs_proc_shm_ctx {
    ka_mutex_t mutex;
    int chan_id;
    u32 cqid;
    u64 cq_pa;
    u32 cq_irq;
    unsigned long shm_sq_va;
};

struct trs_proc_ts_ctx {
    ka_mutex_t mutex;
    ka_atomic_t thread_bind_irq_num;
    ka_atomic_t current_id_num[TRS_CORE_MAX_ID_TYPE];
    ka_atomic_t agent_id_num[TRS_CORE_MAX_ID_TYPE];
    ka_atomic_t id_type_dfx_times[TRS_CORE_MAX_ID_TYPE];
    struct trs_proc_shm_ctx shm_ctx;
    struct trs_core_ts_inst *ts_inst;
    void *smmu_inst;
};

#define TASK_COMM_LEN 16

#define TRS_PROC_STATUS_NORMAL 0
#define TRS_PROC_STATUS_EXIT 1

typedef enum {
    TRS_PROC_RELEASE_FLAG_NONE = 0,
    TRS_PROC_RELEASE_BY_TSDRV_FD,
    TRS_PROC_RELEASE_BY_APM_MASTER,
    TRS_PROC_RELEASE_BY_APM_SLAVE
} TRS_PROC_RELEASE_FLAG;

typedef enum {
    TRS_PROC_RELEASE_LOCAL_REMOTE = 0,
    TRS_PROC_RELEASE_LOCAL,
    TRS_PROC_RELEASE_TYPE_MAX
} TRS_PROC_RELEASE_TYPE; /* Keep consistent with the ascend_hal.h DEV_CLOSE_TYPE */

#define TRS_PROC_DFX_TIMES_MAX 3

struct trs_proc_ctx {
    ka_list_head_t node;
    ka_mm_struct_t *mm;
    struct task_start_time start_time;
    char name[TASK_COMM_LEN];
    s64 task_id;
    int pid;
    int status;
    ka_atomic_t release_flag;
    int release_stage;
    TRS_PROC_RELEASE_TYPE release_type;
    int cp_ssid;
    u32 devid;
    bool force_recycle;
    u32 cmd_support_dfx_times;
    ka_workqueue_struct_t *work_queue;
    ka_proc_dir_entry_t *entry;
    struct trs_proc_ts_ctx ts_ctx[TRS_TS_MAX_NUM];
    struct kref_safe ref;
    ka_rwlock_t ctx_rwlock;
    u32 cp2_pid;
    s64 cp2_task_id;
    u32 agent_app_pid;
    ka_atomic_t agent_release_flag;
};

struct trs_task_info_struct {
    struct trs_proc_ctx *proc_ctx;
    s64 unique_id;
};

static inline int trs_get_proc_res_num(struct trs_proc_ctx *proc_ctx, u32 tsid, int res_type)
{
    return ka_base_atomic_read(&proc_ctx->ts_ctx[tsid].current_id_num[res_type]);
}

static inline int trs_proc_has_agent_res(struct trs_proc_ctx *proc_ctx, u32 tsid)
{
    int i;

    for (i = 0; i < TRS_CORE_MAX_ID_TYPE; i++) {
        if (ka_base_atomic_read(&proc_ctx->ts_ctx[tsid].agent_id_num[i]) != 0) {
            return true;
        }
    }

    return false;
}

static inline int trs_get_proc_id_dfx_times(struct trs_proc_ctx *proc_ctx, u32 tsid, int res_type)
{
    return ka_base_atomic_inc_return(&proc_ctx->ts_ctx[tsid].id_type_dfx_times[res_type]);
}

bool trs_proc_has_res(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id);
bool trs_proc_has_res_with_pid(struct trs_core_ts_inst *ts_inst, int pid, int res_type, u32 res_id);
int trs_proc_add_res(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id);
int trs_proc_add_res_ex(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id,
                        bool is_agent_res);
int trs_proc_del_res(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id);
int trs_res_get(struct trs_core_ts_inst *ts_inst, int pid, int res_type, u32 res_id, u32 hpid, u32 seq);
int trs_res_put(struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id, u32 hpid, u32 seq);
bool trs_is_proc_res_limited(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type);

struct trs_proc_ctx *trs_proc_ctx_find(struct trs_core_ts_inst *ts_inst, int pid);
int trs_proc_wait_for_exit(struct trs_core_ts_inst *ts_inst, int pid);
struct trs_proc_ctx *trs_proc_ctx_create(struct trs_core_ts_inst *all_ts_inst[], u32 ts_num);
bool trs_proc_is_res_leak(struct trs_proc_ctx *proc_ctx, u32 tsid);
void trs_proc_release_notice_ts(struct trs_core_ts_inst *ts_inst, struct trs_proc_ctx *proc_ctx);
int trs_proc_release_check_ts(struct trs_proc_ctx *proc_ctx, u32 tsid);
void trs_proc_leak_res_show(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst);
void trs_proc_release_ras_report(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst);

void trs_proc_release(struct trs_core_ts_inst *ts_inst, struct trs_proc_ctx *proc_ctx);
int trs_proc_recycle(struct trs_core_ts_inst *ts_inst, struct trs_proc_ctx *proc_ctx);
int trs_proc_recycle_post(struct trs_core_ts_inst *ts_inst, struct trs_proc_ctx *proc_ctx);
void trs_clear_exit_proc_list(struct trs_core_ts_inst *ts_inst);
void trs_proc_ctx_put(struct trs_proc_ctx *proc_ctx);
struct trs_proc_ctx *trs_get_share_ctx(struct trs_core_ts_inst *ts_inst, u32 share_pid);

#define TRS_MAP_TYPE_MEM 0        /* normal memory, kernel and user share */
#define TRS_MAP_TYPE_DEV_MEM 1    /* device prop memory, hardware and user share */
#define TRS_MAP_TYPE_RO_DEV_MEM 2 /* device prop memory, hardware and user share, user is readonly */
#define TRS_MAP_TYPE_REG 3        /* device register, hardware and user share */
#define TRS_MAP_TYPE_RO_REG 4     /* device register, hardware and user share, user is readonly */

struct trs_mem_map_para {
    int type;
    bool cc_shared; /* Use decrypted page protection for this mapping in a confidential VM. */
    unsigned long va;
    unsigned long pa;
    unsigned long len;
    struct ka_pa_wraper *pa_wraper_list;
    u32 pa_num;
};

struct trs_mem_unmap_para {
    int type;
    unsigned long va;
    unsigned long len;
};

static inline void trs_remap_fill_para(struct trs_mem_map_para *para, int type, bool cc_shared, unsigned long va,
                                       unsigned long pa, unsigned long len)
{
    para->type = type;
    para->cc_shared = cc_shared;
    para->va = va;
    para->pa = pa;
    para->len = len;
    para->pa_num = 1; /* default pa_num */
}

static inline void trs_unmap_fill_para(struct trs_mem_unmap_para *para, int type, unsigned long va, unsigned long len)
{
    para->type = type;
    para->va = va;
    para->len = len;
}

int trs_remap_sq_mem(struct trs_proc_ctx *proc_ctx, ka_vm_area_struct_t *vma, struct trs_mem_map_para *para);
int trs_unmap_sq_mem(struct trs_proc_ctx *proc_ctx, ka_vm_area_struct_t *vma, struct trs_mem_unmap_para *para);

int trs_remap_sq(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_mem_map_para *para);
int trs_unmap_sq(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_mem_unmap_para *para);

#endif
