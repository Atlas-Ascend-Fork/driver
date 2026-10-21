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
#include "ka_barrier_pub.h"
#include "ka_compiler_pub.h"
#include "securec.h"

#include "trs_chan.h"
#include "trs_id.h"
#include "trs_ts_inst.h"
#include "trs_proc_fs.h"
#include "trs_timestamp.h"
#include "trs_core_trace.h"
#include "trs_logic_cq.h"
#include "trs_sqcq_timestamp.h"

/* non stars cqe */
struct trs_logic_cqe_v1 {
    u16 phase : 1;
    u16 SOP : 1; /* start of packet, indicates this is the first 32bit return payload */
    u16 MOP : 1; /* middle of packet, indicates the payload is a continuation of previous task
                               return payload */
    u16 EOP : 1; /* end of packet, indicates this is the last 32bit return payload. SOP & EOP
                               can appear in the same packet, MOP & EOP can also appear on the same packet. */
    u16 logic_cq_id : 12;
    u16 stream_id;
    u16 task_id;
    u8 error_type;
    u8 match_flag; /* ts set notice drv thread recv must match stream id and task id (sync task set) */
    u32 error_code;
    u32 reserved1;
};

#define LOGIC_CQE_V1_SIZE sizeof(struct trs_logic_cqe_v1)
#define LOGIC_CQ_V1_DEPTH 1024
#define TRS_INVALID_PHY_CQID 0xFFFFU

#define LOGIC_CQE_SIZE sizeof(struct trs_logic_cqe)
#define LOGIC_CQ_MAX_DEPTH (64 * 1024)

#define TRS_MAX_THREAD_BIND_IRQ_NUM 32
#define TRS_PROC_MAX_THREAD_BIND_IRQ_NUM 2

static ka_irqreturn_t trs_thread_bind_irq_proc(int irq, void *para)
{
    struct trs_thread_bind_intr_ctx *irq_ctx = (struct trs_thread_bind_intr_ctx *)para;

    if (irq_ctx->valid != 0) {
        ka_wait_queue_head_t *wait_queue = irq_ctx->wait_queue;
        ka_atomic_t *wait_flag = irq_ctx->wait_flag;

        if (wait_flag != NULL) {
            ka_base_atomic_set(wait_flag, 1);
            ka_smp_wmb();
        }

        if (wait_queue != NULL) {
            ka_task_wake_up(wait_queue);
        }
    }

    return KA_IRQ_HANDLED;
}

void trs_thread_bind_irq_hw_res_uninit(struct trs_core_ts_inst *ts_inst, u32 irq_num)
{
    struct trs_thread_bind_intr_mng *intr_mng = &ts_inst->logic_cq_ctx.intr_mng;
    struct trs_thread_bind_intr_ctx *irq_ctx = intr_mng->irq_ctx;
    u32 i;

    if (intr_mng->irq_num == 0) {
        return;
    }

    for (i = 0; i < irq_num; i++) {
        ts_inst->ops.free_irq(&ts_inst->inst, irq_ctx[i].irq_type, irq_ctx[i].irq, (void *)irq_ctx);
    }
}

static void trs_thread_bind_irq_sw_res_uninit(struct trs_thread_bind_intr_mng *intr_mng)
{
    if (intr_mng->irq_ctx != NULL) {
        trs_kfree(intr_mng->irq_ctx);
        intr_mng->irq_ctx = NULL;
    }

    ka_task_mutex_destroy(&intr_mng->mutex);
}

static void trs_thread_bind_irq_uninit(struct trs_core_ts_inst *ts_inst, u32 irq_num)
{
    trs_thread_bind_irq_hw_res_uninit(ts_inst, irq_num);
    trs_thread_bind_irq_sw_res_uninit(&ts_inst->logic_cq_ctx.intr_mng);
}

static int trs_thread_bind_irq_init(struct trs_core_ts_inst *ts_inst)
{
    struct trs_thread_bind_intr_mng *intr_mng = &ts_inst->logic_cq_ctx.intr_mng;
    struct trs_thread_bind_intr_ctx *irq_ctx = NULL;
    struct trs_id_inst *inst = &ts_inst->inst;
    u32 i, irq_type, irq[TRS_MAX_THREAD_BIND_IRQ_NUM];
    int ret;

    ka_task_mutex_init(&intr_mng->mutex);

    if (ts_inst->ops.get_thread_bind_irq == NULL) {
        return 0;
    }

    ret = ts_inst->ops.get_thread_bind_irq(inst, irq, TRS_MAX_THREAD_BIND_IRQ_NUM, &intr_mng->irq_num, &irq_type);
    if (ret != 0) {
        ka_task_mutex_destroy(&intr_mng->mutex);
        trs_err("Get thread bind irq failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    trs_info("thread bind irq. (devid=%u; tsid=%u; irq_num=%d)\n", inst->devid, inst->tsid, intr_mng->irq_num);

    if ((intr_mng->irq_num == 0)) {
        trs_info("Not support thread bind irq. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return 0;
    }

    irq_ctx = trs_kzalloc(sizeof(struct trs_thread_bind_intr_ctx) * intr_mng->irq_num, KA_GFP_KERNEL);
    if (irq_ctx == NULL) {
        ka_task_mutex_destroy(&intr_mng->mutex);
        trs_err("Mem alloc failed. (size=%lx)\n", sizeof(struct trs_thread_bind_intr_ctx) * intr_mng->irq_num);
        return -ENOMEM;
    }

    intr_mng->irq_ctx = irq_ctx;

    for (i = 0; i < intr_mng->irq_num; i++) {
        irq_ctx[i].valid = 0;
        irq_ctx[i].wait_flag = NULL;
        irq_ctx[i].wait_queue = NULL;
        irq_ctx[i].irq = irq[i];
        irq_ctx[i].irq_type = irq_type;

        ret = ts_inst->ops.request_irq(inst, irq_ctx[i].irq_type, irq_ctx[i].irq, (void *)&irq_ctx[i],
                                       trs_thread_bind_irq_proc);
        if (ret != 0) {
            trs_thread_bind_irq_uninit(ts_inst, i);
            trs_err("Request irq failed. (devid=%u; tsid=%u; id=%u; ret=%d)\n", inst->devid, inst->tsid, i, ret);
            return ret;
        }
    }

    return 0;
}

static int trs_thread_bind_irq_alloc(struct trs_core_ts_inst *ts_inst, ka_atomic_t *wait_flag,
                                     ka_wait_queue_head_t *wait_queue)
{
    struct trs_thread_bind_intr_mng *intr_mng = &ts_inst->logic_cq_ctx.intr_mng;
    struct trs_thread_bind_intr_ctx *irq_ctx = intr_mng->irq_ctx;
    u32 i;
    int irq = -1;

    ka_task_mutex_lock(&intr_mng->mutex);
    for (i = 0; i < intr_mng->irq_num; i++) {
        if (irq_ctx[i].valid == 0) {
            irq_ctx[i].valid = 1;
            irq_ctx[i].wait_flag = wait_flag;
            irq_ctx[i].wait_queue = wait_queue;
            irq = irq_ctx[i].irq;
            break;
        }
    }
    ka_task_mutex_unlock(&intr_mng->mutex);

    return irq;
}

static void trs_thread_bind_irq_free(struct trs_core_ts_inst *ts_inst, u32 irq)
{
    struct trs_thread_bind_intr_mng *intr_mng = &ts_inst->logic_cq_ctx.intr_mng;
    struct trs_thread_bind_intr_ctx *irq_ctx = intr_mng->irq_ctx;
    u32 i;

    ka_task_mutex_lock(&intr_mng->mutex);
    for (i = 0; i < intr_mng->irq_num; i++) {
        if ((irq_ctx[i].valid == 1) && (irq_ctx[i].irq == irq)) {
            irq_ctx[i].wait_flag = NULL;
            irq_ctx[i].wait_queue = NULL;
            irq_ctx[i].valid = 0;
            break;
        }
    }
    ka_task_mutex_unlock(&intr_mng->mutex);
}

static int trs_thread_bind_irq_wait(struct trs_logic_cq *logic_cq, int timeout)
{
    long tm, ret;

    tm = (timeout == -1) ? KA_TASK_MAX_SCHEDULE_TIMEOUT : ka_system_msecs_to_jiffies((u32)timeout);

    ret = ka_task_wait_event_interruptible_timeout(logic_cq->wait_queue,
                                                   (ka_base_atomic_read(&logic_cq->wakeup_num) > 0), tm);
    if (ret == 0) {
        return -ETIMEDOUT;
    } else if (ret < 0) {
        return (int)ret;
    } else {
        /* do nothing */
    }

    return 0;
}

static bool trs_logic_cq_is_empty(struct trs_logic_cq *logic_cq)
{
    return (logic_cq->tail == logic_cq->head);
}

static bool trs_logic_cq_is_full(struct trs_logic_cq *logic_cq)
{
    return (((logic_cq->tail + 1) % logic_cq->cq_depth) == logic_cq->head);
}

void trs_logic_set_cqe_version(struct trs_core_ts_inst *ts_inst, u32 logic_cqid, u32 cqe_verion)
{
    struct trs_logic_cq *logic_cq = &ts_inst->logic_cq_ctx.cq[logic_cqid];
    logic_cq->cqe_verion = cqe_verion;
}

static bool trs_logic_is_cqe_need_match(struct trs_logic_cq *logic_cq, void *cqe)
{
    if (logic_cq->cqe_verion == LOGIC_CQE_VERSION_V1) {
        struct trs_logic_cqe_v1 *logic_cqe = (struct trs_logic_cqe_v1 *)cqe;
        return (logic_cqe->match_flag == 1);
    } else {
        struct trs_logic_cqe *logic_cqe = (struct trs_logic_cqe *)cqe;
        return (logic_cqe->match_flag == 1);
    }
}

static bool trs_logic_is_cqe_match(struct trs_logic_cq *logic_cq, void *cqe, u32 stream_id, u32 task_id)
{
    if (logic_cq->cqe_verion == LOGIC_CQE_VERSION_V1) {
        struct trs_logic_cqe_v1 *logic_cqe = (struct trs_logic_cqe_v1 *)cqe;
        return ((logic_cqe->stream_id == stream_id) && (logic_cqe->task_id == task_id));
    } else {
        struct trs_logic_cqe *logic_cqe = (struct trs_logic_cqe *)cqe;
        return ((logic_cqe->stream_id == stream_id) && (logic_cqe->task_id == task_id));
    }
}

static int trs_logic_cq_recv_para_check(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                        struct halReportRecvInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_LOGIC_CQ, para->cqId)) {
        trs_err("Not proc owner cq. (devid=%u; tsid=%u; logic_cqid=%u)\n", inst->devid, inst->tsid, para->cqId);
        return -EINVAL;
    }

    if (((para->timeout < 0) && (para->timeout != -1)) || (para->cqe_num == 0) || (para->cqe_addr == NULL)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; logic_cqid=%u; timeout=%d; cqe_num=%u)\n", inst->devid, inst->tsid,
                para->cqId, para->timeout, para->cqe_num);
        return -EINVAL;
    }

    return 0;
}

static void trs_logic_cq_check_report_cqe(u32 cqid, struct trs_logic_cqe *cqe, u32 pos)
{
    u64 timestamp = trs_get_s_timestamp();
    if (timestamp - cqe->enque_timestamp > 1800) { /* 1800 s */
        trs_warn(
            "Pay attention to the stranded match cqe. (cqid=%u; pos=%u; match_flag=%u; timestamp=%llu(s);"
            "enque_timestamp=%llu(s); enque_time=%llu(s); stream_id=%u; task_id=%u; sqeType=%u; sqId=%u; sqHead=%u)\n",
            cqid, pos, cqe->match_flag, timestamp, cqe->enque_timestamp, timestamp - cqe->enque_timestamp,
            cqe->stream_id, cqe->task_id, cqe->sqe_type, cqe->sq_id, cqe->sq_head);
    }
}

static u32 trs_logic_cq_get_match_copy_range(struct trs_logic_cq *logic_cq, struct halReportRecvInfo *para,
                                             u32 cqe_start, u32 cqe_end, u32 *num)
{
    u32 cqe_cnt, report_cnt, i, pos, search_start, search_num;
    void *cqe = NULL;

    cqe_cnt = cqe_end - cqe_start;
    report_cnt = ka_base_min(cqe_cnt, para->cqe_num);

    search_start = 0;
    search_num = 0;

    /* Principle: Only one hole can be formed after the copy is complete.
       several situations:
       1. taskid invalid, cqe: normal, normal, ..., task1. normal cqe is more than recv cqe_num, copy all normal cqes;
       2. taskid invalid, cqe: normal, task1, normal, normal. continuous normal cqe is less than recv cqe_num, copy all
          continuous cqes before the match task;
       3. taskid invalid, cqe: task1, task2, normal, task3. normal cqe is after than match cqes, ignore the match cqes;
       4. taskid=1, cqe: normal, normal, ..., task1. normal cqe is more than recv cqe_num, copy all
          normal cqes, copy the match cqe when user recv next time;
       5. taskid=1, cqe: normal, task1, normal. normal cqes in front of match cqe is less than(include
          match cqe) recv cqe_num, copy all the cqes in front of match cqe and the match cqe;
       6. taskid=1, cqe: normal, task2, normal, task1. If there are normal cqes and other cqes need to
          be matched before the match cqe, only the first consecutive segment of common cqes is copied.
       7. taskid=1, cqe: task2, normal, task1. copy normal and task1.
       8. taskid=1, cqe: normal, normal, normal. copy all normal.
       9. taskid=3, cqe: task2, normal, normal, task1. copy all normal.
    */
    for (i = 0; i < cqe_cnt; i++) {
        pos = cqe_start + i;
        cqe = (void *)(logic_cq->addr + ((unsigned long)pos * logic_cq->cqe_size));

        if (trs_logic_is_cqe_need_match(logic_cq, cqe)) {
            /* matched, not copy the following cqes. */
            if (trs_logic_is_cqe_match(logic_cq, cqe, para->stream_id, para->task_id)) {
                if (search_num == 0) {
                    search_start = pos;
                }
                search_num++;
                trs_logic_cq_check_report_cqe(logic_cq->cqid, cqe, pos);
                break;
            }

            /* there are normal cqes and other cqes need to be matched before the match cqe, copy the normal cqes */
            if (search_num != 0) {
                break;
            }
        } else {
            /* set consecutive normal cqes first pos and update num */
            if (search_num == 0) {
                search_start = pos;
            }
            search_num++;
        }

        if (search_num >= report_cnt) {
            break;
        }
    }

    trs_debug("Cmp. (cqid=%u; tid=%d; report_cnt=%u; cqe_num=%u; cqe_cnt=%u, search_start=%u; search_num=%u)\n",
              logic_cq->cqid, ka_task_get_current_pid(), report_cnt, para->cqe_num, cqe_cnt, search_start, search_num);

    *num = search_num;
    return search_start;
}

static void trs_logic_cq_move_cqes(struct trs_logic_cq *logic_cq, u32 src_pos, u32 dst_pos, u32 num)
{
    u32 i, src_last, dst_last;

    trs_debug("Move cqe. (cqid=%u; num=%u; src_pos=%u; dst_pos=%u)\n", logic_cq->cqid, num, src_pos, dst_pos);

    src_last = src_pos + num - 1;
    dst_last = dst_pos + num - 1;

    if (logic_cq->cqe_verion == LOGIC_CQE_VERSION_V1) {
        struct trs_logic_cqe_v1 *report_s = NULL;
        struct trs_logic_cqe_v1 *report_d = NULL;
        report_s = (struct trs_logic_cqe_v1 *)(logic_cq->addr + ((unsigned long)src_last * logic_cq->cqe_size));
        report_d = (struct trs_logic_cqe_v1 *)(logic_cq->addr + ((unsigned long)dst_last * logic_cq->cqe_size));

        /* Copy cqes from the back to the front to avoid overwriting. */
        for (i = 0; i < num; i++) {
            *report_d = *report_s;
            report_s--;
            report_d--;
        }
    } else {
        struct trs_logic_cqe *report_s = NULL;
        struct trs_logic_cqe *report_d = NULL;
        report_s = (struct trs_logic_cqe *)(logic_cq->addr + ((unsigned long)src_last * logic_cq->cqe_size));
        report_d = (struct trs_logic_cqe *)(logic_cq->addr + ((unsigned long)dst_last * logic_cq->cqe_size));

        /* Copy cqes from the back to the front to avoid overwriting. */
        for (i = 0; i < num; i++) {
            *report_d = *report_s;
            report_s--;
            report_d--;
        }
    }
}

static void trs_logic_cq_eliminate_holes(struct trs_logic_cq *logic_cq, u32 start, u32 report_cnt, u32 rollback)
{
    u32 num, src_pos, dst_pos;

    trs_debug("Eliminate holes. (cqid=%u; start=%u; report_cnt=%u; rollback=%u; head=%u; tail=%u)\n", logic_cq->cqid,
              start, report_cnt, rollback, logic_cq->head, logic_cq->tail);

    if (rollback == 0) {
        num = start - logic_cq->head;
        src_pos = logic_cq->head;
    } else {
        num = start;
        src_pos = 0;
    }
    dst_pos = src_pos + report_cnt;

    /* rollback:1 */
    /* 0----start--start+cnt---tail-------head-----depth-1 */
    if (num != 0) {
        trs_logic_cq_move_cqes(logic_cq, src_pos, dst_pos, num);
    }

    if (rollback == 0) {
        return;
    }

    /* move the cqes in que tail to head */
    num = ka_base_min(report_cnt, logic_cq->cq_depth - logic_cq->head);
    src_pos = logic_cq->cq_depth - num;
    dst_pos = report_cnt - num;
    trs_logic_cq_move_cqes(logic_cq, src_pos, dst_pos, num);

    /* move the cqes to tail */
    /* 0-----tail-------head=====head+num------depth-1 */
    num = logic_cq->cq_depth - logic_cq->head - num;
    if (num == 0) {
        return;
    }
    src_pos = logic_cq->head;
    dst_pos = logic_cq->cq_depth - num;
    trs_logic_cq_move_cqes(logic_cq, src_pos, dst_pos, num);
}

static int trs_logic_cq_match_copy(struct trs_core_ts_inst *ts_inst, struct trs_logic_cq *logic_cq,
                                   struct halReportRecvInfo *para)
{
    u32 start, report_cnt, tail;
    u32 rollback = 0;
    int ret;

    tail = logic_cq->tail; /* tail is update by tasklet, should save local */
    if (tail > logic_cq->head) {
        start = trs_logic_cq_get_match_copy_range(logic_cq, para, logic_cq->head, tail, &report_cnt);
    } else {
        start = trs_logic_cq_get_match_copy_range(logic_cq, para, logic_cq->head, logic_cq->cq_depth, &report_cnt);
        if (report_cnt == 0) {
            rollback = 1;
            start = trs_logic_cq_get_match_copy_range(logic_cq, para, 0, tail, &report_cnt);
        }
    }

    if (report_cnt == 0) {
        return -EAGAIN;
    }

    trs_logic_cq_copy_trace("Logic Cq Recv Match", ts_inst, logic_cq, start, report_cnt);
    ret = ka_base_copy_to_user((void __ka_user *)para->cqe_addr,
                               logic_cq->addr + ((unsigned long)start * logic_cq->cqe_size),
                               (unsigned long)report_cnt * logic_cq->cqe_size);
    if (ret != 0) {
        trs_err("copy to user fail, cqid=%u report_cnt=%u\n", logic_cq->cqid, report_cnt);
        return -EFAULT;
    }

    para->report_cqe_num = report_cnt;

    if (logic_cq->head != start) {
        trs_logic_cq_eliminate_holes(logic_cq, start, report_cnt, rollback);
    }

    return 0;
}

static int trs_logic_cq_non_match_copy(struct trs_core_ts_inst *ts_inst, struct trs_logic_cq *logic_cq,
                                       struct halReportRecvInfo *para)
{
    u32 start, report_cnt, tail;
    int ret;

    tail = logic_cq->tail;
    start = logic_cq->head;
    report_cnt = (tail > start) ? tail - start : logic_cq->cq_depth - start;

    trs_logic_cq_copy_trace("Logic Cq Recv NoMatch", ts_inst, logic_cq, start, report_cnt);
    ret = ka_base_copy_to_user((void __ka_user *)para->cqe_addr,
                               logic_cq->addr + ((unsigned long)start * logic_cq->cqe_size),
                               (unsigned long)report_cnt * logic_cq->cqe_size);
    if (ret != 0) {
        trs_err("copy to user fail, cqid=%u report_cnt=%u\n", logic_cq->cqid, report_cnt);
        return -EFAULT;
    }

    para->report_cqe_num = report_cnt;

    return 0;
}

static int trs_logic_cq_copy_report(struct trs_core_ts_inst *ts_inst, struct trs_logic_cq *logic_cq,
                                    struct halReportRecvInfo *para)
{
    u32 version = para->res[0];
    int full_flag = 0;
    int ret;

    if (logic_cq->valid == 0) {
        return -EFAULT;
    }

    if (trs_logic_cq_is_empty(logic_cq)) {
        return -EAGAIN;
    }

    if (trs_logic_cq_is_full(logic_cq)) {
        full_flag = 1;
    }

    if (version == 1) {
        ret = trs_logic_cq_match_copy(ts_inst, logic_cq, para); // runtime new version
    } else {
        ret = trs_logic_cq_non_match_copy(ts_inst, logic_cq, para);
    }
    if (ret != 0) {
        return ret;
    }

    /*
     * Ensure that loads to register are visible to this CPU before head write.
     */
    ka_smp_mb();

    logic_cq->head = (logic_cq->head + para->report_cqe_num) % logic_cq->cq_depth;

    trs_debug("Recv success. (cqid=%u; tail=%u; head=%u; report_cqe_num=%u; logic_cq_depth=%u)\n", logic_cq->cqid,
              logic_cq->tail, logic_cq->head, para->report_cqe_num, logic_cq->cq_depth);

    if ((full_flag != 0) && (ts_inst->logic_cq_ctx.phy_cq.chan_id >= 0)) {
        (void)trs_chan_ctrl(&ts_inst->inst, ts_inst->logic_cq_ctx.phy_cq.chan_id, CHAN_CTRL_CMD_CQ_SCHED, 0);
    }

    return 0;
}

static int trs_logic_cq_wait_event(struct trs_logic_cq *logic_cq, int timeout)
{
    KA_TASK_DEFINE_WAIT(wq_entry);
    long ret, tm;

    ka_base_atomic_inc(&logic_cq->wait_thread_num);
    trs_debug("Wake wait start. (logic_cqid=%u; timeout=%d; wait_thread_num=%d)\n", logic_cq->cqid, timeout,
              ka_base_atomic_read(&logic_cq->wait_thread_num));

    tm = (timeout == -1) ? KA_TASK_MAX_SCHEDULE_TIMEOUT : ka_system_msecs_to_jiffies((u32)timeout);
    (void)ka_task_prepare_to_wait_exclusive(&logic_cq->wait_queue, &wq_entry, KA_TASK_INTERRUPTIBLE);
    if (ka_base_atomic_read(&logic_cq->wakeup_num) > 0) {
        ka_base_atomic_dec(&logic_cq->wakeup_num);
        ret = tm;
    } else {
        ret = ka_task_schedule_timeout(tm);
    }
    if (ka_task_signal_pending_state(KA_TASK_INTERRUPTIBLE, ka_task_get_current()) != 0) {
        ret = -ERESTARTSYS;
    }
    ka_task_finish_wait(&logic_cq->wait_queue, &wq_entry);

    ka_base_atomic_dec(&logic_cq->wait_thread_num);
    trs_debug("Wake wait finish. (logic_cqid=%u; timeout=%d; wait_thread_num=%d; ret=%ld)\n", logic_cq->cqid, timeout,
              ka_base_atomic_read(&logic_cq->wait_thread_num), ret);

    if (ret == 0) {
        /* wait event timeout */
        logic_cq->stat.timeout++;
        return -ETIMEDOUT;
    } else if (ret > 0) {
        /* wait event is awakened */
        return 0;
    } else {
        /* do nothing */
    }
    if (ret == -ERESTARTSYS) {
#ifndef EMU_ST
        trs_debug("Wait event interrupted. (cqid=%u)\n", logic_cq->cqid);
#endif
    }

    return ret;
}

static int trs_logic_cq_reset(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 cqid)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_logic_cq *logic_cq = NULL;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_LOGIC_CQ, cqid)) {
        trs_err("Not proc owner cq. (devid=%u; logic_cqid=%u)\n", inst->devid, cqid);
        return -EINVAL;
    }

    logic_cq = &ts_inst->logic_cq_ctx.cq[cqid];
    ka_task_mutex_lock(&logic_cq->mutex);
    logic_cq->head = 0;
    logic_cq->tail = 0;
    ka_base_atomic_set(&logic_cq->wakeup_num, 0);
    ka_base_atomic_set(&logic_cq->wait_thread_num, 0);
    ka_task_mutex_unlock(&logic_cq->mutex);
    trs_debug("Reset logic cq. (devid=%u; tsid=%u; logic_cqid=%u)\n", inst->devid, inst->tsid, cqid);
    return 0;
}

int trs_logic_cq_config(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqConfigInfo *para)
{
    if (para->prop == DRV_SQCQ_PROP_SQCQ_RESET) {
        return trs_logic_cq_reset(proc_ctx, ts_inst, para->cqId);
    }

    return -EOPNOTSUPP;
}

#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX
static void trs_dfx_print_cq_basic_info(struct trs_logic_cq *logic_cq, u32 head, u32 tail, u32 cqe_num)
{
    trs_info("DFX logic_cq_info: cqid=%u valid=%u head=%u tail=%u depth=%u cqe_num=%u "
             "cqe_version=%u cqe_size=%u thread_bind_irq=%d "
             "enque=%llu full_drop=%llu wakeup=%llu recv=%llu recv_in=%llu timeout=%llu "
             "wait_thread_num=%d wakeup_num=%d\n",
             logic_cq->cqid, logic_cq->valid, head, tail, logic_cq->cq_depth, cqe_num, logic_cq->cqe_verion,
             logic_cq->cqe_size, logic_cq->thread_bind_irq, logic_cq->stat.enque, logic_cq->stat.full_drop,
             logic_cq->stat.wakeup, logic_cq->stat.recv, logic_cq->stat.recv_in, logic_cq->stat.timeout,
             ka_base_atomic_read(&logic_cq->wait_thread_num), ka_base_atomic_read(&logic_cq->wakeup_num));
}

static void trs_dfx_print_cqe_trace(u32 tsid, u32 logic_cqid, struct trs_logic_cq *logic_cq, u32 idx)
{
    struct trs_logic_cqe *cqe = (struct trs_logic_cqe *)(logic_cq->addr + idx * logic_cq->cqe_size);
    struct trs_logic_cqe_trace_info *trace = trs_dfx_get_logic_cq_enque_trace(tsid, logic_cqid, idx);
    trs_info("    [%u] sq_id=%u task_id=%u stream_id=%u enque_ts=%llums\n", idx, cqe->sq_id, cqe->task_id,
             cqe->stream_id, (trace != NULL) ? trs_tick_to_millisecond(trace->timestamp) : 0);
}

static void trs_dfx_print_logic_cq_info(u32 tsid, struct trs_logic_cq *logic_cq)
{
    u32 head;
    u32 tail;
    u32 depth = logic_cq->cq_depth;
    u32 cqe_num;
    u32 i;

    ka_task_spin_lock_bh(&logic_cq->lock);
    head = logic_cq->head;
    tail = logic_cq->tail;
    ka_task_spin_unlock_bh(&logic_cq->lock);

    cqe_num = (head == tail) ? 0 : (head < tail) ? (tail - head) : (depth - head + tail);
    trs_dfx_print_cq_basic_info(logic_cq, head, tail, cqe_num);

    if (head == tail) {
        trs_info("  CQE ring empty: head=%u tail=%u\n", head, tail);
        return;
    }

    if (head < tail) {
        trs_info("  CQE ring [head=%u, tail=%u) (no wrap):\n", head, tail);
        for (i = head; i < tail; i++) {
            trs_dfx_print_cqe_trace(tsid, logic_cq->cqid, logic_cq, i);
        }
    } else {
        trs_info("  CQE ring [head=%u..depth=%u) + [0..tail=%u) (wrap):\n", head, depth, tail);
        for (i = head; i < depth; i++) {
            trs_dfx_print_cqe_trace(tsid, logic_cq->cqid, logic_cq, i);
        }
        for (i = 0; i < tail; i++) {
            trs_dfx_print_cqe_trace(tsid, logic_cq->cqid, logic_cq, i);
        }
    }
}

static void trs_dfx_recv_timeout_print(struct trs_core_ts_inst *ts_inst, struct trs_logic_cq *logic_cq,
                                       u64 recv_start_tick, struct halReportRecvInfo *para, int err)
{
    u32 tsid = ts_inst->inst.tsid;
    u32 logic_cqid = logic_cq->cqid;
    u64 wait_start_ms = trs_tick_to_millisecond(recv_start_tick);
    u64 wait_end_ms = trs_tick_to_millisecond(trs_get_cpu_tick());
    u32 head;
    u32 tail;

    if (trs_sqcq_dfx_ratelimited()) {
        return;
    }

    if (err == -ETIMEDOUT) {
        ka_task_spin_lock_bh(&logic_cq->lock);
        head = logic_cq->head;
        tail = logic_cq->tail;
        ka_task_spin_unlock_bh(&logic_cq->lock);
        if (head == tail) {
            return;
        }

        trs_info("wait logic cq result show, (tsid=%u, logic_cq_id=%u, version=%u, cqe_num=%u, timeout=%dms, "
                 "streamid=%u, taskid=%u, err=%d, wait_start_timestamp=%llums, wait_end_timestamp=%llums).\n",
                 tsid, logic_cqid, para->res[0], para->cqe_num, para->timeout, para->stream_id, para->task_id, err,
                 wait_start_ms, wait_end_ms);

        trs_dfx_print_timestamp_nodes(tsid, logic_cqid, recv_start_tick);
        trs_dfx_print_logic_cq_info(tsid, logic_cq);
    } else if (err == -ERESTARTSYS) {
        trs_warn("wait event interrupted, (tsid=%u, logic_cq_id=%u, version=%u, cqe_num=%u, timeout=%dms, "
                 "err=%d, wait_start_timestamp=%llums, wait_end_timestamp=%llums).\n",
                 tsid, logic_cqid, para->res[0], para->cqe_num, para->timeout, err, wait_start_ms, wait_end_ms);
    }
}
#endif /* CFG_FEATURE_SUPPORT_CQ_RECV_DFX */

int trs_logic_cq_recv(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halReportRecvInfo *para)
{
    struct trs_logic_cq *logic_cq = NULL;
    int ret;
#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX
    u64 recv_start_tick = trs_get_cpu_tick();
#endif

    ret = trs_logic_cq_recv_para_check(proc_ctx, ts_inst, para);
    if (ret != 0) {
        return ret;
    }

    logic_cq = &ts_inst->logic_cq_ctx.cq[para->cqId];
    logic_cq->stat.recv_in++;

    if (logic_cq->thread_bind_irq >= 0) {
        ret = trs_thread_bind_irq_wait(logic_cq, para->timeout);
#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX
        if (ret < 0) {
            trs_dfx_recv_timeout_print(ts_inst, logic_cq, recv_start_tick, para, ret);
        }
#endif
        return ret;
    }
    trs_logic_cq_recv_trace("Recv start", ts_inst, para);
    do {
        ka_task_mutex_lock(&logic_cq->mutex);
        ret = trs_logic_cq_copy_report(ts_inst, logic_cq, para);
        ka_task_mutex_unlock(&logic_cq->mutex);
        if (ret == 0) {
            logic_cq->stat.recv++;
            trs_logic_cq_recv_trace("Recv finish", ts_inst, para);
            return ret;
        }

        if (ret == -EAGAIN) {
            if (para->timeout == 0) {
                para->report_cqe_num = 0;
                return 0;
            }
            ret = trs_logic_cq_wait_event(logic_cq, para->timeout);
        }
    } while (ret >= 0);

#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX
    if (ret < 0) {
        trs_dfx_recv_timeout_print(ts_inst, logic_cq, recv_start_tick, para, ret);
    }
#endif

    return ret;
}

static void trs_logic_cq_wakeup(struct trs_core_ts_inst *ts_inst, struct trs_logic_cq *logic_cq)
{
    int wait_thread_num = ka_base_atomic_read(&logic_cq->wait_thread_num);
    int wakeup_num = (wait_thread_num > 0) ? wait_thread_num : 1;

    ka_base_atomic_set(&logic_cq->wakeup_num, wakeup_num);
    trs_debug("Wakeup all. (logic_cqid=%u; wait_thread_num=%d)\n", logic_cq->cqid, wait_thread_num);
    ka_smp_mb();
#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX
    trs_dfx_set_logic_cq_wakeup_tick(ts_inst->inst.tsid, logic_cq->cqid);
#endif
    if (ka_task_waitqueue_active(&logic_cq->wait_queue) != 0) {
        ka_task_wake_up_nr(&logic_cq->wait_queue, wakeup_num);
        logic_cq->stat.wakeup++;
    }
}

int trs_logic_cq_enque(struct trs_core_ts_inst *ts_inst, u32 logic_cq_id, u32 stream_id, u32 task_id, void *cqe)
{
    struct trs_logic_cq *logic_cq = &ts_inst->logic_cq_ctx.cq[logic_cq_id];
    struct trs_id_inst *inst = &ts_inst->inst;

    ka_task_spin_lock_bh(&logic_cq->lock);
    if (logic_cq->valid == 0) {
        ka_task_spin_unlock_bh(&logic_cq->lock);
        trs_debug("Invalid logic cq drop. (devid=%u; tsid=%u; logic_cqid=%u; stream_id=%u; task_id=%u)\n", inst->devid,
                  inst->tsid, logic_cq_id, stream_id, task_id);
        return -EINVAL;
    }

    if (trs_logic_cq_is_full(logic_cq)) {
        ka_task_spin_unlock_bh(&logic_cq->lock);
        logic_cq->stat.full_drop++;
        trs_debug("Full drop. (devid=%u; tsid=%u; logic_cqid=%u; stream_id=%u; task_id=%u)\n", inst->devid, inst->tsid,
                  logic_cq_id, stream_id, task_id);
        return -EAGAIN; /* block recv */
    }

    (void)memcpy_s(logic_cq->addr + logic_cq->tail * logic_cq->cqe_size, logic_cq->cqe_size, cqe, logic_cq->cqe_size);
    ka_smp_wmb();
#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX
    {
        struct trs_logic_cqe *logic_cqe_ptr = (struct trs_logic_cqe *)cqe;
        trs_dfx_set_logic_cq_enque_trace(inst->tsid, logic_cq_id, (u16)task_id, (u16)stream_id, logic_cqe_ptr->sq_id,
                                         logic_cq->tail);
    }
#endif
    logic_cq->tail = (logic_cq->tail + 1) % logic_cq->cq_depth;
    ka_task_spin_unlock_bh(&logic_cq->lock);

    trs_debug("Enque. (devid=%u; tsid=%u; logic_cqid=%u; stream_id=%u; task_id=%u; tail=%u)\n", inst->devid, inst->tsid,
              logic_cq_id, stream_id, task_id, logic_cq->tail);

    ka_smp_wmb();
    trs_logic_cq_enque_trace(ts_inst, logic_cq, stream_id, task_id, cqe);
    trs_logic_cq_wakeup(ts_inst, logic_cq);
    logic_cq->stat.enque++;
    return 0;
}

static int trs_logic_phy_cq_recv_proc(struct trs_core_ts_inst *ts_inst, u32 cqid, void *cqe)
{
    struct trs_logic_cq_ctx *logic_cq_ctx = &ts_inst->logic_cq_ctx;
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_logic_cqe_v1 *logic_cqe = (struct trs_logic_cqe_v1 *)cqe;
    int ret;

    if (logic_cq_ctx->phy_cq.cqid != cqid) {
        trs_err("Not logic phy cqid. (devid=%u; tsid=%u; cqid=%u; logic_cqid=%u)\n", inst->devid, inst->tsid, cqid,
                logic_cq_ctx->phy_cq.cqid);
        return CQ_RECV_FINISH;
    }

    if (logic_cqe->logic_cq_id >= logic_cq_ctx->cq_num) {
        trs_err("Invalid logic cqid. (devid=%u; tsid=%u; cq_id=%u)\n", inst->devid, inst->tsid, logic_cqe->logic_cq_id);
        return CQ_RECV_FINISH;
    }

    ret = trs_logic_cq_enque(ts_inst, logic_cqe->logic_cq_id, logic_cqe->stream_id, logic_cqe->task_id, cqe);
    if (ret == -EAGAIN) {
        return CQ_RECV_CONTINUE;
    }

    return CQ_RECV_FINISH;
}

static int trs_logic_phy_cq_recv(struct trs_id_inst *inst, u32 cqid, void *cqe)
{
    struct trs_core_ts_inst *ts_inst = NULL;
    int ret;

    ts_inst = trs_core_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u; cqid=%u)\n", inst->devid, inst->tsid, cqid);
        return CQ_RECV_FINISH;
    }

    ret = trs_logic_phy_cq_recv_proc(ts_inst, cqid, cqe);
    trs_core_ts_inst_put(ts_inst);

    return ret;
}

static bool trs_logic_cqe_is_valid(void *cqe, u32 loop)
{
    struct trs_logic_cqe_v1 *logic_cqe = (struct trs_logic_cqe_v1 *)cqe;
    return (logic_cqe->phase == ((loop + 1) & 0x1));
}

static int trs_logic_alloc_phy_cq(struct trs_core_ts_inst *ts_inst)
{
    struct trs_logic_phy_cq *phy_cq = &ts_inst->logic_cq_ctx.phy_cq;
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_cq_info cq_info;
    struct trs_chan_para para = {0};
    int ret;

    para.types.type = CHAN_TYPE_SW;
    para.types.sub_type = CHAN_SUB_TYPE_SW_LOGIC;

    para.cq_para.cqe_size = LOGIC_CQE_V1_SIZE;
    para.cq_para.cq_depth = LOGIC_CQ_V1_DEPTH;
    para.ops.cqe_is_valid = trs_logic_cqe_is_valid;
    para.ops.get_sq_head_in_cqe = NULL;
    para.ops.cq_recv = trs_logic_phy_cq_recv;
    para.ops.abnormal_proc = NULL;

    para.flag = (0x1 << CHAN_FLAG_ALLOC_CQ_BIT);

    ret = hal_kernel_trs_chan_create(inst, &para, &phy_cq->chan_id);
    if (ret != 0) {
        trs_err("Alloc phy sqcq chan failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    (void)trs_chan_get_cq_info(inst, phy_cq->chan_id, &cq_info);

    phy_cq->cqid = cq_info.cqid;
    phy_cq->cq_irq = cq_info.irq;
    phy_cq->cq_phy_addr = cq_info.cq_phy_addr;

    trs_info("Alloc phy cq success. (devid=%u; tsid=%u; chan_id=%d; cqid=%u)\n", inst->devid, inst->tsid,
             phy_cq->chan_id, phy_cq->cqid);

    return 0;
}

static void trs_logic_free_phy_cq(struct trs_core_ts_inst *ts_inst)
{
    struct trs_logic_phy_cq *phy_cq = &ts_inst->logic_cq_ctx.phy_cq;
    struct trs_id_inst *inst = &ts_inst->inst;

    trs_info("Free phy cq success. (devid=%u; tsid=%u; chan_id=%d; cqid=%u)\n", inst->devid, inst->tsid,
             phy_cq->chan_id, phy_cq->cqid);
    hal_kernel_trs_chan_destroy(&ts_inst->inst, phy_cq->chan_id);
    phy_cq->chan_id = -1;
}

static int trs_logic_cq_id_init(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                struct halSqCqInputInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_logic_cq *logic_cq = NULL;
    int ret;

    ret = trs_proc_add_res(proc_ctx, ts_inst, TRS_LOGIC_CQ, para->cqId);
    if (ret != 0) {
        return -EINVAL;
    }

    logic_cq = &ts_inst->logic_cq_ctx.cq[para->cqId];

    ka_task_mutex_lock(&logic_cq->mutex);
    logic_cq->addr = ts_inst->ops.cq_mem_alloc(inst, para->cqeDepth * para->cqeSize);
    if (logic_cq->addr == NULL) {
        (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_LOGIC_CQ, para->cqId);
        ka_task_mutex_unlock(&logic_cq->mutex);
        trs_err("Mem alloc failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -ENOMEM;
    }

    logic_cq->thread_bind_irq = -1;
    if ((para->flag & TSDRV_FLAG_THREAD_BIND_IRQ) != 0) {
        /* thread bind irq num in proc needn't be precise, so not use lock  */
        if (ka_base_atomic_read(&proc_ctx->ts_ctx[inst->tsid].thread_bind_irq_num) < TRS_PROC_MAX_THREAD_BIND_IRQ_NUM) {
            logic_cq->thread_bind_irq = trs_thread_bind_irq_alloc(ts_inst, &logic_cq->wakeup_num,
                                                                  &logic_cq->wait_queue);
        }

        if (logic_cq->thread_bind_irq < 0) {
            para->flag &= ~TSDRV_FLAG_THREAD_BIND_IRQ;
        } else {
            ka_base_atomic_inc(&proc_ctx->ts_ctx[inst->tsid].thread_bind_irq_num);
            trs_info("Alloc thread bind irq. (devid=%u; tsid=%u; cqId=%u; thread_bind_irq=%u)\n", inst->devid,
                     inst->tsid, para->cqId, logic_cq->thread_bind_irq);
        }
    }

    logic_cq->cqid = para->cqId;
    logic_cq->cqe_verion = trs_is_stars_inst(ts_inst) ? LOGIC_CQE_VERSION_V2 : LOGIC_CQE_VERSION_V1;
    logic_cq->cq_depth = para->cqeDepth;
    logic_cq->cqe_size = para->cqeSize;
    logic_cq->head = 0;
    logic_cq->tail = 0;
    logic_cq->flag = para->flag;
    ka_base_atomic_set(&logic_cq->wakeup_num, 0);
    ka_base_atomic_set(&logic_cq->wait_thread_num, 0);
    (void)memset_s(&logic_cq->stat, sizeof(struct trs_logic_cq_stat), 0, sizeof(struct trs_logic_cq_stat));
    ka_wmb();
    ka_task_spin_lock_bh(&logic_cq->lock);
    logic_cq->valid = 1;
    ka_task_spin_unlock_bh(&logic_cq->lock);
    ka_task_mutex_unlock(&logic_cq->mutex);

    return 0;
}

static void trs_logic_cq_id_uninit(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 cqid)
{
    struct trs_logic_cq *logic_cq = &ts_inst->logic_cq_ctx.cq[cqid];
    struct trs_id_inst *inst = &ts_inst->inst;

    ka_task_mutex_lock(&logic_cq->mutex);
    ka_task_spin_lock_bh(&logic_cq->lock);
    logic_cq->valid = 0;
    ka_task_spin_unlock_bh(&logic_cq->lock);
    ka_wmb();
    trs_logic_cq_wakeup(ts_inst, logic_cq);
    if (logic_cq->thread_bind_irq >= 0) {
        ka_base_atomic_dec(&proc_ctx->ts_ctx[ts_inst->inst.tsid].thread_bind_irq_num);
        trs_thread_bind_irq_free(ts_inst, logic_cq->thread_bind_irq);
        trs_info("Free thread bind irq. (devid=%u; tsid=%u; cqId=%u; thread_bind_irq=%u)\n", inst->devid, inst->tsid,
                 cqid, logic_cq->thread_bind_irq);
    }
    ts_inst->ops.cq_mem_free(inst, logic_cq->addr, logic_cq->cq_depth * logic_cq->cqe_size);
    logic_cq->addr = NULL;
    (void)trs_proc_del_res(proc_ctx, ts_inst, TRS_LOGIC_CQ, cqid);
    ka_task_mutex_unlock(&logic_cq->mutex);
}

static int trs_logic_cq_alloc_notice_ts(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst,
                                        struct halSqCqInputInfo *para)
{
    struct trs_logic_phy_cq *phy_cq = &ts_inst->logic_cq_ctx.phy_cq;
    struct trs_logic_cq *logic_cq = &ts_inst->logic_cq_ctx.cq[para->cqId];
    struct trs_logic_cq_mbox msg;
    int ret;

    trs_mbox_init_header(&msg.header, TRS_MBOX_LOGIC_CQ_ALLOC);

    if (logic_cq->thread_bind_irq >= 0) {
        msg.mb_alloc.thread_bind_irq_flag = 1;
        msg.mb_alloc.cq_irq = (u16)logic_cq->thread_bind_irq;
        msg.mb_alloc.phy_cq_addr = 0;
    } else {
        msg.mb_alloc.thread_bind_irq_flag = 0;
        msg.mb_alloc.cq_irq = (u16)phy_cq->cq_irq;
        if (phy_cq->notice_flag == 0) {
            msg.mb_alloc.phy_cq_addr = phy_cq->cq_phy_addr;
            phy_cq->notice_flag = 1;
        } else {
            msg.mb_alloc.phy_cq_addr = 0;
        }
    }

    msg.mb_alloc.cqe_size = (u16)para->cqeSize;
    msg.mb_alloc.cq_depth = (u16)para->cqeDepth;
    msg.mb_alloc.vpid = (u32)proc_ctx->pid;
    msg.mb_alloc.logic_cqid = (u16)para->cqId;
    msg.mb_alloc.phy_cqid = (u16)phy_cq->cqid;

    ret = memcpy_s(msg.mb_alloc.info, sizeof(msg.mb_alloc.info), para->info, sizeof(para->info));
    if (ret != 0) {
        trs_err("Memcopy failed. (dest_len=%lx; src_len=%lx)\n", sizeof(msg.mb_alloc.info), sizeof(para->info));
        return ret;
    }

    /* adapt fill: app_flag vfid */

    return trs_core_notice_ts(ts_inst, (u8 *)&msg, sizeof(msg));
}

static int trs_logic_cq_alloc_para_check(struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para)
{
    if ((para->cqeDepth > LOGIC_CQ_MAX_DEPTH) || (para->cqeDepth == 0) || (para->cqeSize > LOGIC_CQE_SIZE) ||
        (para->cqeSize == 0)) {
        return -EINVAL;
    }

    return 0;
}

int trs_logic_cq_alloc(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    u32 id_flag = 0;
    int ret;

    ret = trs_logic_cq_alloc_para_check(ts_inst, para);
    if (ret != 0) {
        trs_err("Invalid para. (devid=%u; tsid=%u; cqeDepth=%u; cqeSize=%u)\n", inst->devid, inst->tsid, para->cqeDepth,
                para->cqeSize);
        return ret;
    }

    if ((para->flag & TSDRV_FLAG_SPECIFIED_CQ_ID) != 0) {
        trs_id_set_specified_flag(&id_flag);
    }

    ret = trs_id_alloc_ex(inst, TRS_LOGIC_CQ_ID, id_flag, &para->cqId, 1);
    if (ret != 0) {
        trs_err("Alloc logic cq failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }

    ret = trs_logic_cq_id_init(proc_ctx, ts_inst, para);
    if (ret != 0) {
        (void)trs_id_free_ex(inst, TRS_LOGIC_CQ_ID, 0, para->cqId);
        return ret;
    }

    if (!trs_is_stars_inst(ts_inst)) {
        if (ts_inst->logic_cq_ctx.phy_cq.chan_id == -1) {
            ret = trs_logic_alloc_phy_cq(ts_inst);
            if (ret != 0) {
                trs_logic_cq_id_uninit(proc_ctx, ts_inst, para->cqId);
                (void)trs_id_free_ex(inst, TRS_LOGIC_CQ_ID, 0, para->cqId);
                return ret;
            }
        }

        ret = trs_logic_cq_alloc_notice_ts(proc_ctx, ts_inst, para);
        if (ret != 0) {
            trs_logic_cq_id_uninit(proc_ctx, ts_inst, para->cqId);
            (void)trs_id_free_ex(inst, TRS_LOGIC_CQ_ID, 0, para->cqId);
            trs_err("Notice ts failed. (devid=%u; tsid=%u; logic_cqid=%u)\n", inst->devid, inst->tsid, para->cqId);
            return ret;
        }
    }

    trs_debug("Logic cq alloc. (devid=%u; tsid=%u; flag=0x%x; logic_cqid=%u)\n", inst->devid, inst->tsid, para->flag,
              para->cqId);

    return 0;
}

static int trs_logic_cq_free_notice_ts(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 logic_cqid)
{
    struct trs_logic_cq_mbox msg;

    trs_mbox_init_header(&msg.header, TRS_MBOX_LOGIC_CQ_FREE);

    msg.mb_free.vpid = (u32)proc_ctx->pid;
    msg.mb_free.logic_cqid = (u16)logic_cqid;
    msg.mb_free.phy_cqid = TRS_INVALID_PHY_CQID; /* phy_cq->cqid */

    /* adapt fill: vfid */
    return trs_core_notice_ts(ts_inst, (u8 *)&msg, sizeof(msg));
}

static bool trs_logic_cq_is_need_show(struct trs_logic_cq *cq)
{
    return ((cq->stat.full_drop != 0) || (cq->head != cq->tail));
}

static int _trs_logic_cq_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, u32 flag, u32 cqId)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    u32 id_flag = 0;

    if (!trs_proc_has_res(proc_ctx, ts_inst, TRS_LOGIC_CQ, cqId)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; logic_cqid=%u)\n", inst->devid, inst->tsid, cqId);
        return -EINVAL;
    }

    if (!trs_is_stars_inst(ts_inst)) {
        int ret = trs_logic_cq_free_notice_ts(proc_ctx, ts_inst, cqId);
        if (ret != 0) {
            trs_warn("Notice ts warn. (devid=%u; tsid=%u; logic_cqid=%u)\n", inst->devid, inst->tsid, cqId);
        }
    }

    if (trs_logic_cq_is_need_show(&ts_inst->logic_cq_ctx.cq[cqId])) {
        trs_logic_cq_show(ts_inst, cqId);
    }

    trs_logic_cq_id_uninit(proc_ctx, ts_inst, cqId);

    if ((flag & TSDRV_FLAG_RSV_CQ_ID) != 0) {
        trs_id_set_reserved_flag(&id_flag);
    }
    (void)trs_id_free_ex(inst, TRS_LOGIC_CQ_ID, id_flag, cqId);

    trs_debug("Logic cq free. (devid=%u; tsid=%u; flag=0x%x; logic_cqid=%u)\n", inst->devid, inst->tsid, flag, cqId);

    return 0;
}

int trs_logic_cq_free(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqFreeInfo *para)
{
    return _trs_logic_cq_free(proc_ctx, ts_inst, para->flag, para->cqId);
}

void trs_logic_cq_recycle(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, int res_type, u32 res_id)
{
    (void)_trs_logic_cq_free(proc_ctx, ts_inst, 0, res_id);
}

int trs_logic_cq_get(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_stream_ctx stm_ctx = {0};
    int logic_cqid, stream_id;
    int ret;

    stream_id = para->info[0];
    ret = trs_get_stream_ctx(proc_ctx, ts_inst, stream_id, &stm_ctx);
    if (ret != 0) {
        return ret;
    }

    logic_cqid = stm_ctx.logic_cq;
    if ((logic_cqid < 0) || ((u32)logic_cqid > trs_res_get_max_id(ts_inst, TRS_LOGIC_CQ))) {
        trs_err("Stream no logic cq. (devid=%u; tsid=%u; stream_id=%u; logic_cqid=%d)\n", inst->devid, inst->tsid,
                stream_id, logic_cqid);
        return -EINVAL;
    }

    para->cqId = logic_cqid;
    para->cqeDepth = ts_inst->logic_cq_ctx.cq[stm_ctx.logic_cq].cq_depth;
    para->cqeSize = ts_inst->logic_cq_ctx.cq[stm_ctx.logic_cq].cqe_size;

    trs_debug("Share logic cq info. (devid=%u; streamid=%u; logic_cqid=%u; cq_depth=%u; cqe_size=%u)\n",
              proc_ctx->devid, stream_id, para->cqId, para->cqeDepth, para->cqeSize);

    return 0;
}

int trs_logic_cq_init(struct trs_core_ts_inst *ts_inst)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_logic_cq_ctx *cq_ctx = &ts_inst->logic_cq_ctx;
    u32 id_num = trs_res_get_id_num(ts_inst, TRS_LOGIC_CQ);
    u32 max_id = trs_res_get_max_id(ts_inst, TRS_LOGIC_CQ);
    int ret;
    u32 i;

    trs_debug("Init logic cq. (devid=%u; tsid=%u; id_num=%u; max_id=%u)\n", inst->devid, inst->tsid, id_num, max_id);

    cq_ctx->cq = (struct trs_logic_cq *)trs_vzalloc(sizeof(struct trs_logic_cq) * max_id);
    if (cq_ctx->cq == NULL) {
        trs_err("Mem alloc failed. (devid=%u; tsid=%u; size=%lx)\n", inst->devid, inst->tsid,
                sizeof(struct trs_logic_cq) * max_id);
        return -ENOMEM;
    }

    cq_ctx->cq_num = max_id;
    for (i = 0; i < max_id; i++) {
        ka_task_init_waitqueue_head(&cq_ctx->cq[i].wait_queue);
        ka_task_spin_lock_init(&cq_ctx->cq[i].lock);
        ka_task_mutex_init(&cq_ctx->cq[i].mutex);
    }

    cq_ctx->phy_cq.chan_id = -1;

    ret = trs_thread_bind_irq_init(ts_inst);
    if (ret != 0) {
        for (i = 0; i < max_id; i++) {
            ka_task_mutex_destroy(&cq_ctx->cq[i].mutex);
        }
        trs_vfree(cq_ctx->cq);
        cq_ctx->cq = NULL;
        return ret;
    }

    return 0;
}

void trs_logic_cq_uninit(struct trs_core_ts_inst *ts_inst)
{
    struct trs_logic_cq_ctx *cq_ctx = &ts_inst->logic_cq_ctx;
    int i;

    if (cq_ctx->phy_cq.chan_id != -1) {
        trs_logic_free_phy_cq(ts_inst);
    }

    trs_thread_bind_irq_sw_res_uninit(&ts_inst->logic_cq_ctx.intr_mng);

    for (i = 0; i < cq_ctx->cq_num; i++) {
        ka_task_mutex_destroy(&cq_ctx->cq[i].mutex);
    }

    trs_vfree(cq_ctx->cq);
    cq_ctx->cq = NULL;
}
