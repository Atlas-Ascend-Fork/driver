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
#include "ka_kernel_def_pub.h"
#include "ka_barrier_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"

#include <securec.h>
#include "pbl/pbl_soc_res.h"
#include "pbl/pbl_uda.h"
#include "trs_timestamp.h"
#include "trs_mailbox_def.h"
#include "chan_init.h"
#include "chan_rxtx.h"
#include "chan_ts_inst.h"
#include "chan_proc_fs.h"
#include "chan_trace.h"
#include "comm_kernel_interface.h"
#include "trs_sqcq_timestamp.h"

/* sqe/cqe address Domain, user address or kernel address */
#define CHAN_ADDR_DOMAIN_KERNEL 0
#define CHAN_ADDR_DOMAIN_USER 1

static inline void trs_chan_try_to_update_sq_head(struct trs_chan *chan, u8 *cq_addr);

static void trs_chan_sqe_flush(struct trs_chan *chan, u8 *sqe, u64 offset)
{
    if (chan->ts_inst->ops.flush_cache != NULL) {
        u64 pa;

        if (chan->sq.mem_attr.pa_wraper_list != NULL && chan->sq.mem_attr.pa_num > 0) {
            u64 cur_offset = 0;
            u32 i;
            for (i = 0; i < chan->sq.mem_attr.pa_num; i++) {
                if (offset < cur_offset + chan->sq.mem_attr.pa_wraper_list[i].size) {
                    pa = chan->sq.mem_attr.pa_wraper_list[i].pa + (offset - cur_offset);
                    break;
                }
                cur_offset += chan->sq.mem_attr.pa_wraper_list[i].size;
            }
            if (i == chan->sq.mem_attr.pa_num) {
                trs_warn("Can not find pa. (offset=%llu)\n", offset);
                return;
            }
        } else {
            pa = chan->sq.mem_attr.phy_addr + offset;
        }

        chan->ts_inst->ops.flush_cache(&chan->inst, chan->sq.mem_attr.mem_type, sqe, pa, chan->sq.para.sqe_size);
    }
}

static void trs_chan_cqe_invalid_cache(struct trs_chan *chan, u8 *cqe)
{
    if (chan->ts_inst->ops.invalid_cache != NULL) {
        u64 pa = chan->cq.mem_attr.phy_addr + (u64)(cqe - (u8 *)chan->cq.cq_addr);
        chan->ts_inst->ops.invalid_cache(&chan->inst, chan->cq.mem_attr.mem_type, cqe, pa, chan->cq.para.cqe_size);
    }
}

static int trs_chan_sqe_update(struct trs_chan *chan, u8 *sqe)
{
    int ret = 0;
    if (((chan->types.type == CHAN_TYPE_HW) && (chan->types.sub_type == CHAN_SUB_TYPE_HW_RTS)) ||
        ((chan->ts_inst->location == UDA_REMOTE) && (chan->types.type == CHAN_TYPE_HW) &&
         (chan->types.sub_type == CHAN_SUB_TYPE_HW_TOPIC_SCHED))) {
        if (chan->ts_inst->ops.sqe_update != NULL) {
            struct trs_sqe_update_info update_info = {0};
            update_info.pid = chan->pid;
            update_info.sqid = chan->sq.sqid;
            update_info.sqeid = chan->sq.sq_tail;
            update_info.sqe = (void *)sqe;
            update_info.long_sqe_cnt = &chan->sq.long_sqe_cnt;

            ret = chan->ts_inst->ops.sqe_update(&chan->inst, &update_info);
            if ((ret == 0) && (chan->sq.long_sqe_cnt != 0)) {
                chan->sq.long_sqe_cnt--;
            }
        }
    }
    return ret;
}

int trs_chan_stream_task_update(struct trs_id_inst *inst, int pid, void *vaddr, u32 *long_task_cnt)
{
    struct trs_chan_ts_inst *ts_inst = NULL;
    int ret = 0;

    ts_inst = trs_chan_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u)\n", inst->devid);
        return -EINVAL;
    }

    if (ts_inst->ops.sqe_update != NULL) {
        struct trs_sqe_update_info update_info = {0};
        update_info.pid = pid;
        update_info.sqid = KA_U32_MAX;
        update_info.sqe = vaddr;
        update_info.long_sqe_cnt = long_task_cnt;
        ret = ts_inst->ops.sqe_update(inst, &update_info);
        if ((ret == 0) && (*long_task_cnt != 0)) {
            (*long_task_cnt)--;
        }
    }
    trs_chan_ts_inst_put(ts_inst);
    trs_debug("Chan update sqe success. (devid=%u; pid=%d)\n", inst->devid, pid);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_stream_task_update);

static void trs_chan_cqe_update(struct trs_chan *chan, u8 *cqe)
{
    if ((chan->types.type == CHAN_TYPE_HW) && (chan->types.sub_type == CHAN_SUB_TYPE_HW_RTS)) {
        if (chan->ts_inst->ops.cqe_update != NULL) {
            (void)chan->ts_inst->ops.cqe_update(&chan->inst, chan->pid, chan->cq.cqid, cqe);
        }
    }
}

static bool trs_chan_sq_is_full(struct trs_chan_sq_ctx *sq)
{
    return (((sq->sq_tail + 1) % sq->para.sq_depth) == sq->sq_head);
}

static int trs_chan_submit_wait(struct trs_chan *chan, u32 timeout)
{
    struct trs_id_inst *inst = &chan->ts_inst->inst;
    struct trs_chan_sq_ctx *sq = &chan->sq;
    long ret;

    ret = ka_task_wait_event_interruptible_timeout(sq->wait_queue, !trs_chan_sq_is_full(sq),
                                                   ka_system_msecs_to_jiffies((u32)timeout));
    if (ret == 0) {
        return -ETIMEDOUT;
    } else if (ret < 0) {
#ifndef EMU_ST
        trs_warn("Wait warn. (devid=%u; tsid=%u; sqid=%u; ret=%ld)\n", inst->devid, inst->tsid, sq->sqid, ret);
#endif
        return (int)ret;
    } else {
        /* do nothing */
    }

    return 0;
}

void trs_chan_submit_wakeup(struct trs_chan *chan)
{
    struct trs_chan_sq_ctx *sq = &chan->sq;
    ka_smp_mb();
    if (ka_task_waitqueue_active(&sq->wait_queue) != 0) {
        ka_task_wake_up_interruptible(&sq->wait_queue);
    }
}

static int trs_chan_update_sq_head_from_hw(struct trs_chan *chan)
{
    if (chan->types.type == CHAN_TYPE_HW) {
        struct trs_id_inst *inst = &chan->ts_inst->inst;
        u64 sq_head;
        int ret = chan->ts_inst->ops.sqcq_query(inst, &chan->types, chan->sq.sqid, QUERY_CMD_SQ_HEAD, &sq_head);
        if (ret == 0) {
            chan->sq.sq_head = (u32)sq_head;
            return 0;
        }
    }

    return -1;
}

static int trs_chan_update_sq_tail_from_hw(struct trs_chan *chan)
{
    if (chan->types.type == CHAN_TYPE_HW) {
        struct trs_id_inst *inst = &chan->ts_inst->inst;
        u64 sq_tail;
        int ret = chan->ts_inst->ops.sqcq_query(inst, &chan->types, chan->sq.sqid, QUERY_CMD_SQ_TAIL, &sq_tail);
        if (ret == 0) {
            chan->sq.sq_tail = (u32)sq_tail;
            return 0;
        }
    }

    return -1;
}

static bool trs_chan_is_full(struct trs_chan *chan)
{
    if (!trs_chan_sq_is_full(&chan->sq)) {
        return false;
    }
    (void)trs_chan_update_sq_head_from_hw(chan);
    return trs_chan_sq_is_full(&chan->sq);
}

static bool trs_chan_mem_kva_non_contiguous(struct trs_chan *chan)
{
    return (chan->sq.mem_attr.vmap_pfn_handle != NULL) &&
           (ka_mm_vmap_pfn_kva(chan->sq.mem_attr.vmap_pfn_handle) == NULL);
}

static int trs_chan_sqe_wait_space(struct trs_chan *chan, int timeout)
{
    struct trs_id_inst *inst = &chan->ts_inst->inst;
    int ret;

    if (!trs_chan_is_full(chan)) {
        return 0;
    }

    chan->stat.tx_full++;
    if (timeout == 0) {
        trs_debug("Sq is full. (devid=%u; tsid=%u; sqid=%u)\n", inst->devid, inst->tsid, chan->sq.sqid);
        return -ENOSPC;
    }

    ret = trs_chan_submit_wait(chan, timeout);
    if (ret < 0) {
        return ret;
    }

    if (trs_chan_is_full(chan)) {
        return -ETIMEDOUT;
    }

    return 0;
}

static int trs_chan_fill_sqe(struct trs_chan *chan, u8 *sqe, int timeout, int addr_domain)
{
    struct trs_id_inst *inst = &chan->ts_inst->inst;
    struct trs_chan_sq_ctx *sq = &chan->sq;
    u8 sqe_tmp[SQE_CACHE_SIZE];
    u8 *sqe_addr = NULL, *dst_addr = NULL;
    u64 offset = (u64)sq->para.sqe_size * sq->sq_tail;
    int ret;

    /* For SVM memory with vmap_pfn handle, dst_addr from kva (NULL if non-contiguous < 5.10) */
    dst_addr = (trs_chan_mem_kva_non_contiguous(chan)) ? NULL : (u8 *)sq->sq_addr + offset;

    ret = trs_chan_sqe_wait_space(chan, timeout);
    if (ret < 0) {
        return ret;
    }

    /* if using bar to r/w sqe, it should use stack value to store sqe to avoid waster time */
    sqe_addr = trs_chan_mem_is_local_mem(sq->mem_attr.mem_type) ? dst_addr : sqe_tmp;
    if ((chan->sq.mem_attr.phy_addr == TRS_INVALID_PHYS_ADDR) || (trs_chan_mem_kva_non_contiguous(chan))) {
        sqe_addr = sqe_tmp;
    }

    if (addr_domain == CHAN_ADDR_DOMAIN_KERNEL) {
        memcpy_s(sqe_addr, sq->para.sqe_size, sqe, sq->para.sqe_size);
    } else {
        unsigned long ret_cpy = ka_base_copy_from_user(sqe_addr, sqe, sq->para.sqe_size);
        if (ret_cpy != 0) {
            trs_err("Copy fail. (devid=%u; tsid=%u; sqid=%u; ret=%lu)\n", inst->devid, inst->tsid, sq->sqid, ret_cpy);
            return -EINVAL;
        }
    }

    ret = trs_chan_sqe_update(chan, sqe_addr);
    if (ret != 0) {
        trs_warn("Update warn. (devid=%u; tsid=%u; sqid=%u; ret=%d)\n", inst->devid, inst->tsid, sq->sqid, ret);
        return ret;
    }

    if (chan->sq.mem_attr.phy_addr != TRS_INVALID_PHYS_ADDR) {
        if (!trs_chan_mem_is_local_mem(sq->mem_attr.mem_type)) {
            if (sq->mem_attr.vmap_pfn_handle != NULL) {
                ka_mm_vmap_pfn_memcpy_toio(sq->mem_attr.vmap_pfn_handle, offset, sqe_addr, sq->para.sqe_size);
            } else {
                ka_mm_memcpy_toio(dst_addr, sqe_addr, sq->para.sqe_size);
            }
        }
        /* flush_cache: use sqe_addr for cache flush (valid kva), dst_addr may be NULL for non-contiguous SVM */
        trs_chan_sqe_flush(chan, dst_addr != NULL ? dst_addr : sqe_addr, offset);
    }

    sq->sq_tail = (sq->sq_tail + 1) % sq->para.sq_depth;
    if (addr_domain == CHAN_ADDR_DOMAIN_KERNEL) {
        /* update software queue after sqe_update */
        (void)memcpy_s(sqe, sq->para.sqe_size, sqe_addr, sq->para.sqe_size);
    }
    trs_chan_trace_sqe("Task Send", chan, sq, sqe_addr);
    return 0;
}

static int trs_chan_submit(struct trs_chan *chan, struct trs_chan_send_para *para, int addr_domain)
{
    struct trs_id_inst *inst = &chan->ts_inst->inst;
    struct trs_chan_sq_ctx *sq = &chan->sq;
    u32 sq_tail = sq->sq_tail;
    u32 send_sqe_num = 0;
    int ret;

    while ((send_sqe_num < para->sqe_num) && (ka_base_atomic_read(&chan->chan_status))) {
        ret = trs_chan_fill_sqe(chan, para->sqe + sq->para.sqe_size * send_sqe_num, para->timeout, addr_domain);
        if (ret == 0) {
            send_sqe_num++;
            continue;
        } else if (ret != -ETIMEDOUT) {
            return ret;
        } else {
            trs_debug("Wait timeout. (devid=%u; tsid=%u; sqid=%u; ret=%d)\n", inst->devid, inst->tsid, sq->sqid, ret);
            return -ENOSPC;
        }
    }

    ka_wmb();
    chan->sq.pos = sq_tail;
    ret = chan->ts_inst->ops.sqcq_ctrl(inst, &chan->types, sq->sqid, CTRL_CMD_SQ_TAIL_UPDATE, sq->sq_tail);
    if (ret != 0) {
        sq->sq_tail = sq_tail;
        trs_err("Sq tail update fail. (devid=%u; tsid=%u; sqid=%u; ret=%d)\n", inst->devid, inst->tsid, sq->sqid, ret);
        return ret;
    }

    chan->stat.tx += para->sqe_num;

    return 0;
}

static int trs_chan_sem_down(struct trs_chan *chan, int timeout)
{
    int ret;

    if (timeout > 0) {
        ret = ka_task_down_timeout(&chan->sq.sem, ka_system_msecs_to_jiffies((u32)timeout));
    } else {
        ret = ka_task_down_interruptible(&chan->sq.sem);
    }

    return ret;
}

static void trs_chan_sem_up(struct trs_chan *chan)
{
    ka_task_up(&chan->sq.sem);
}

static int trs_chan_send_ex(struct trs_id_inst *inst, int chan_id, struct trs_chan_send_para *para, int addr_domain)
{
    struct trs_chan *chan = NULL;
    int ret;

    if ((para == NULL) || (para->sqe == NULL)) {
        trs_err("Null ptr. (chan_id=%d; domain=%d, inst=%pK; para=%pK)\n", chan_id, addr_domain, inst, para);
        return -EINVAL;
    }

    chan = trs_chan_get(inst, chan_id);
    if (chan == NULL) {
        trs_err("Invalid para. (chan_id=%d)\n", chan_id);
        return -EINVAL;
    }

    if (!trs_chan_has_sq_mem(chan)) {
        trs_chan_put(chan);
        trs_err("Unauthorized operation. (devid=%u; tsid=%u; chan_id=%d)\n", inst->devid, inst->tsid, chan_id);
        return -EPERM;
    }

    if (chan->sq.status == 0) {
        trs_chan_put(chan);
        return -EPERM;
    }

    if (ka_base_in_softirq()) {
        ka_task_spin_lock_bh(&chan->sq.lock); /* submit task in tasklet */
        para->timeout = 0;                    /* to avoid into wait event, if sq is full. */
    } else {
        ret = trs_chan_sem_down(chan, para->timeout);
        if (ret != 0) {
            trs_warn("Down warn. (devid=%u; tsid=%u; sqid=%u; ret=%d)\n", chan->ts_inst->inst.devid,
                     chan->ts_inst->inst.tsid, chan->sq.sqid, ret);
            chan->stat.tx_timeout++;
            trs_chan_put(chan);
            return ret;
        }
    }

    para->first_pos = chan->sq.sq_tail;
    ret = trs_chan_submit(chan, para, addr_domain);

    if (ka_base_in_softirq()) {
        ka_task_spin_unlock_bh(&chan->sq.lock);
    } else {
        trs_chan_sem_up(chan);
    }
    trs_chan_put(chan);

    return ret;
}

int trs_chan_send(struct trs_id_inst *inst, int chan_id, struct trs_chan_send_para *para)
{
    return trs_chan_send_ex(inst, chan_id, para, CHAN_ADDR_DOMAIN_USER);
}
KA_EXPORT_SYMBOL_GPL(trs_chan_send);

int hal_kernel_trs_chan_send(struct trs_id_inst *inst, int chan_id, struct trs_chan_send_para *para)
{
    return trs_chan_send_ex(inst, chan_id, para, CHAN_ADDR_DOMAIN_KERNEL);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_trs_chan_send);

static int trs_chan_update_sq_head(struct trs_chan *chan, u32 sq_head)
{
    if (sq_head >= chan->sq.para.sq_depth) {
        trs_warn("Invalid head. (devid=%u; tsid=%u; sqid=%u; sq_head=%u; sq_depth=%u)\n", chan->ts_inst->inst.devid,
                 chan->ts_inst->inst.tsid, chan->sq.sqid, sq_head, chan->sq.para.sq_depth);
        return -EINVAL;
    }

    chan->sq.sq_head = sq_head;
    ka_smp_wmb();
    trs_chan_submit_wakeup(chan);

    return 0;
}

static int trs_chan_set_sq_head(struct trs_chan *chan, u32 sq_head)
{
    int ret;

    if (sq_head < chan->sq.para.sq_depth) {
        ret = chan->ts_inst->ops.sqcq_ctrl(&chan->ts_inst->inst, &chan->types, chan->sq.sqid, CTRL_CMD_SQ_HEAD_UPDATE,
                                           sq_head);
        if (ret != 0) {
            return ret;
        }
    }

    return trs_chan_update_sq_head(chan, sq_head);
}

static int trs_chan_update_sq_tail(struct trs_chan *chan, u32 sq_tail)
{
    if (sq_tail >= chan->sq.para.sq_depth) {
        trs_warn("Invalid tail. (devid=%u; tsid=%u; sqid=%u; sq_tail=%u; sq_depth=%u)\n", chan->ts_inst->inst.devid,
                 chan->ts_inst->inst.tsid, chan->sq.sqid, sq_tail, chan->sq.para.sq_depth);
        return -EINVAL;
    }

    chan->sq.sq_tail = sq_tail;
    return 0;
}

static int trs_chan_set_sq_tail(struct trs_chan *chan, u32 sq_tail)
{
    int ret;

    if (sq_tail < chan->sq.para.sq_depth) {
        ret = chan->ts_inst->ops.sqcq_ctrl(&chan->ts_inst->inst, &chan->types, chan->sq.sqid, CTRL_CMD_SQ_TAIL_UPDATE,
                                           sq_tail);
        if (ret != 0) {
            return ret;
        }
    }

    return trs_chan_update_sq_tail(chan, sq_tail);
}

static int trs_chan_set_sq_status(struct trs_chan *chan, u32 status)
{
    int ret = chan->ts_inst->ops.sqcq_ctrl(&chan->ts_inst->inst, &chan->types, chan->sq.sqid, CTRL_CMD_SQ_STATUS_SET,
                                           status);
    if (ret == 0) {
        chan->sq.status = status;
    }

    return ret;
}

static int trs_chan_sq_disable_to_enable(struct trs_chan *chan, u32 timeout)
{
    int ret = chan->ts_inst->ops.sqcq_ctrl(&chan->ts_inst->inst, &chan->types, chan->sq.sqid,
                                           CTRL_CMD_SQ_DISABLE_TO_ENABLE, timeout);
    if (ret == 0) {
        chan->sq.status = 1;
    }

    return ret;
}

static bool trs_chan_is_valid_cqe(struct trs_chan *chan, void *cqe, u32 loop)
{
    bool is_valid = (chan->ops.cqe_is_valid != NULL) ? chan->ops.cqe_is_valid(cqe, loop) :
                                                       chan->ts_inst->ops.cqe_is_valid(&chan->ts_inst->inst, cqe, loop);

    ka_rmb();

    return is_valid;
}

static void trs_chan_update_cq_head(struct trs_chan_cq_ctx *cq)
{
    cq->cq_head = (cq->cq_head + 1) % cq->para.cq_depth;
    if (cq->cq_head == 0) {
        cq->loop++;
    }
}

static void trs_chan_get_sq_head_in_cqe(struct trs_chan *chan, void *cqe, u32 *sq_head)
{
    if (chan->ops.get_sq_head_in_cqe != NULL) {
        chan->ops.get_sq_head_in_cqe(cqe, sq_head);
    } else {
        chan->ts_inst->ops.get_sq_head_in_cqe(&chan->ts_inst->inst, cqe, sq_head);
    }
}

static int trs_chan_fetch_wait(struct trs_chan *chan, u32 timeout)
{
    struct trs_id_inst *inst = &chan->ts_inst->inst;
    struct trs_chan_cq_ctx *cq = &chan->cq;
    long tm, ret;

    if (timeout == 0) {
        return -ETIMEDOUT;
    }

    tm = (timeout == -1) ? KA_TASK_MAX_SCHEDULE_TIMEOUT : ka_system_msecs_to_jiffies((u32)timeout);

    ret = ka_task_wait_event_interruptible_timeout(cq->wait_queue, (cq->cqe_valid == 1), tm);
    if (ret == 0) {
        trs_warn("Wait timeout. (devid=%u; tsid=%u; type=%u; cqid=%u; timeout=%u ms)\n", inst->devid, inst->tsid,
                 chan->types.type, cq->cqid, timeout);
        return -ETIMEDOUT;
    } else if (ret < 0) {
#ifndef EMU_ST
        trs_warn("Wait warn. (devid=%u; tsid=%u; cqid=%u; ret=%ld)\n", inst->devid, inst->tsid, cq->cqid, ret);
#endif
        return (int)ret;
    } else {
        /* do nothing */
    }

    cq->cqe_valid = 0;

    return 0;
}

void trs_chan_fetch_wakeup(struct trs_chan *chan)
{
    struct trs_chan_cq_ctx *cq = &chan->cq;

    cq->cqe_valid = 1;

    ka_smp_mb();
    if (ka_task_waitqueue_active(&cq->wait_queue) != 0) {
        chan->stat.rx_wakeup++;
        ka_task_wake_up_interruptible(&cq->wait_queue);
    }
}

static int trs_chan_fetch(struct trs_chan *chan, u8 *cqe, int addr_domain, u32 timeout)
{
    struct trs_chan_cq_ctx *cq = &chan->cq;
    u8 *cq_addr = NULL;
    int ret;

    ka_task_mutex_lock(&cq->mutex);
    cq_addr = (u8 *)cq->cq_addr + cq->para.cqe_size * cq->cq_head;
    trs_chan_cqe_invalid_cache(chan, cq_addr);
    while (!trs_chan_is_valid_cqe(chan, cq_addr, cq->loop)) {
        chan->stat.rx_empty++;
        ka_task_mutex_unlock(&cq->mutex);
        ret = trs_chan_fetch_wait(chan, timeout);
        if (ret != 0) {
            return ret;
        }
        ka_task_mutex_lock(&cq->mutex);
        cq_addr = (u8 *)cq->cq_addr + cq->para.cqe_size * cq->cq_head;
        trs_chan_cqe_invalid_cache(chan, cq_addr);
    }

    trs_chan_cqe_update(chan, cq_addr);
    trs_chan_try_to_update_sq_head(chan, cq_addr);

    if (addr_domain == CHAN_ADDR_DOMAIN_KERNEL) {
        ka_mm_memcpy_fromio(cqe, cq_addr, cq->para.cqe_size);
    } else {
        if (((u64)cq_addr % sizeof(u8 *)) != 0) {
            /* copy_to_user need addr 8 byte align */
            ret = ka_base_put_user(*(u32 *)cq_addr, (u32 *)cqe);
            ret |= ka_base_copy_to_user(cqe + CQE_ALIGN_SIZE, (void *)(cq_addr + CQE_ALIGN_SIZE),
                                        cq->para.cqe_size - CQE_ALIGN_SIZE);
        } else {
            ret = ka_base_copy_to_user(cqe, (void *)cq_addr, cq->para.cqe_size);
        }
        if (ret != 0) {
            ka_task_mutex_unlock(&cq->mutex);
            trs_err("Copy fail. (devid=%u; tsid=%u; cqid=%u; ret=%d)\n", chan->ts_inst->inst.devid,
                    chan->ts_inst->inst.tsid, cq->cqid, ret);
            return ret;
        }
    }

    trs_chan_update_cq_head(cq);
    trs_chan_trace_cqe("Task Recv", chan, cq, cq_addr);
    chan->stat.rx++;
    ka_task_mutex_unlock(&cq->mutex);

    return 0;
}

static int trs_chan_recv_ex(struct trs_id_inst *inst, int chan_id, struct trs_chan_recv_para *para, int addr_domain)
{
    struct trs_chan *chan = NULL;
    u32 timeout;
    int ret = 0;

    if ((para == NULL) || (para->cqe == NULL)) {
        trs_err("Null ptr. (chan_id=%d; domain=%d, inst=%pK; para=%pK)\n", chan_id, addr_domain, inst, para);
        return -EINVAL;
    }
    chan = trs_chan_get(inst, chan_id);
    if (chan == NULL) {
        trs_err("Invalid para. (chan_id=%d)\n", chan_id);
        return -EINVAL;
    }

    if (!trs_chan_has_cq_mem(chan)) {
        trs_chan_put(chan);
        trs_err("Unauthorized operation. (devid=%u; tsid=%u; chan_id=%d)\n", inst->devid, inst->tsid, chan_id);
        return -EPERM;
    }

    para->recv_cqe_num = 0;
    timeout = para->timeout;
    trace_chan_trace_recv("Recv start", chan, para);
    while (para->recv_cqe_num < para->cqe_num) {
        ret = trs_chan_fetch(chan, para->cqe + chan->cq.para.cqe_size * para->recv_cqe_num, addr_domain, timeout);
        if (ret != 0) {
            break;
        }
        para->recv_cqe_num++;
        timeout = 0;
    }

    ka_mb();

    if (para->recv_cqe_num > 0) {
        chan->ts_inst->ops.sqcq_ctrl(inst, &chan->types, chan->cq.cqid, CTRL_CMD_CQ_HEAD_UPDATE, chan->cq.cq_head);
        ret = 0;
    }
    trace_chan_trace_recv("Recv finish", chan, para);
    trs_chan_put(chan);

    return ret;
}

int trs_chan_recv(struct trs_id_inst *inst, int chan_id, struct trs_chan_recv_para *para)
{
    return trs_chan_recv_ex(inst, chan_id, para, CHAN_ADDR_DOMAIN_USER);
}
KA_EXPORT_SYMBOL_GPL(trs_chan_recv);

int hal_kernel_trs_chan_recv(struct trs_id_inst *inst, int chan_id, struct trs_chan_recv_para *para)
{
    return trs_chan_recv_ex(inst, chan_id, para, CHAN_ADDR_DOMAIN_KERNEL);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_trs_chan_recv);

static inline void trs_chan_try_to_update_sq_head(struct trs_chan *chan, u8 *cq_addr)
{
    u32 sq_head;

    if (trs_chan_has_sq(chan) && trs_chan_is_auto_update_sq_head(chan)) {
        if (trs_chan_update_sq_head_from_hw(chan) < 0) {
            trs_chan_get_sq_head_in_cqe(chan, cq_addr, &sq_head);
            (void)trs_chan_update_sq_head(chan, sq_head);
        }
    }
}

static void trs_chan_sched(struct trs_chan *chan)
{
    if (trs_chan_is_recv_block(chan)) {
        ka_task_schedule_work(&chan->work);
    } else {
        ka_system_tasklet_schedule(&chan->irq_ctx->task);
    }
}

static int trs_chan_cq_bar_addr_pre_copy(struct trs_chan *chan, u8 *report, u32 report_size)
{
    struct trs_chan_cq_ctx *cq = &chan->cq;
    void *cq_addr = cq->cq_addr + cq->para.cqe_size * cq->cq_head;
    u64 cq_tail;

    /* Before memcpy to stack val, it must guarantee cqe written compeletly.
        Q: Why using cq tail to judge can meet the cqe data is ready?
        A: Due to hardware writes cqe firstly, then update cq tail, the order is controlled by hardware,
            in addition, host reads cqe and cq tail by pcie bar, the order is decided by host instruction.
    */
    chan->ts_inst->ops.sqcq_query(&chan->inst, &chan->types, cq->cqid, QUERY_CMD_CQ_TAIL, &cq_tail);
    if ((cq_tail != cq->cq_head) && (cq->para.cqe_size <= report_size)) {
        ka_mm_memcpy_fromio((void *)report, cq_addr, cq->para.cqe_size);
    } else {
        if (trs_chan_is_valid_cqe(chan, cq_addr, cq->loop)) {
            trs_chan_sched(chan);
        }
        return -EAGAIN;
    }

    return 0;
}

static int trs_chan_cq_dispatch(struct trs_chan *chan, u32 dispatch_num)
{
    struct trs_chan_cq_ctx *cq = &chan->cq;
    const int max_resched_times = 100; /* max is 100 */
    int recv_num = 0;
    u32 proc_num = 0;
    u8 *cq_addr = NULL;
    u8 report[CQE_CACHE_SIZE] = {0};
    int state = 0;
    int ret;

    chan->stat.rx_in++;

    if (!trs_chan_has_cq_mem(chan)) {
        trs_debug("Unauthorized operation. (devid=%u; tsid=%u; cq_id=%d)\n", chan->ts_inst->inst.devid,
                  chan->ts_inst->inst.tsid, chan->cq.cqid);
        return 0;
    }

    chan->interrupt_time_us = trs_get_us_timestamp();
    while (ka_base_atomic_read(&chan->chan_status)) {
        cq_addr = (u8 *)cq->cq_addr + cq->para.cqe_size * cq->cq_head;
        trs_chan_cqe_invalid_cache(chan, cq_addr);

        if (trs_chan_mem_is_local_mem(cq->mem_attr.mem_type) == false) {
            /* Using bar to r/w cqe waster time, so add a stack value. */
            ret = trs_chan_cq_bar_addr_pre_copy(chan, report, CQE_CACHE_SIZE);
            if (ret != 0) {
                break;
            }
            cq_addr = report;
        }

        if (!trs_chan_is_valid_cqe(chan, cq_addr, cq->loop)) {
#ifndef EMU_ST /* if delete, the emu st will into and sched tasklet, which causes dispatch grab the spin_lock */
            if ((chan->ts_inst->ops.cq_need_resched != NULL) &&
                chan->ts_inst->ops.cq_need_resched(&chan->inst, &chan->types)) {
                struct trs_chan_adapt_ops *ops = &chan->ts_inst->ops;
                u64 cq_tail;
                /* 'cq_tail != chan->cq.cq_head' not represent the cqe data is later than cq tail,
                    maybe, this moment, cqe data has arrived, and cq tail also updated too. */
                ops->sqcq_query(&chan->inst, &chan->types, chan->cq.cqid, QUERY_CMD_CQ_TAIL, &cq_tail);
                if ((cq_tail != chan->cq.cq_head) && (chan->retry_times < max_resched_times)) {
                    trs_chan_sched(chan);
                    chan->retry_times++;
                    if (chan->retry_times == max_resched_times) {
                        chan->stat.hw_err++;
                    }
                }
            }
#endif
            break;
        }
        chan->retry_times = 0;

        proc_num++;
        if (proc_num == dispatch_num) {
            ka_task_schedule_work(&chan->work);
            state = -EBUSY;
            break;
        }

        trs_chan_cqe_update(chan, cq_addr);

        chan->stat.rx_dispatch++;

        trs_chan_trace_cqe("Cqe Dispatch", chan, cq, cq_addr);
        ret = CQ_RECV_FINISH;
        if (chan->ops.cq_recv != NULL) {
            ret = chan->ops.cq_recv(&chan->ts_inst->inst, cq->cqid, cq_addr);
        }
        if (ret == CQ_RECV_FINISH) {
            trs_chan_try_to_update_sq_head(chan, cq_addr);
            trs_chan_update_cq_head(cq);
#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX
            trs_dfx_set_phy_cq_tick(chan->ts_inst->inst.tsid, cq->cqid);
#endif
            recv_num++;
        } else {
            trs_chan_fetch_wakeup(chan);
            break; /* stop traversal, wait for thread to fetch */
        }
    }

    ka_mb();

    if (recv_num > 0) {
        chan->ts_inst->ops.sqcq_ctrl(&chan->ts_inst->inst, &chan->types, chan->cq.cqid, CTRL_CMD_CQ_HEAD_UPDATE,
                                     cq->cq_head);
    }
    return state;
}

static int trs_chan_cq_dispatch_non_block(struct trs_chan *chan)
{
    int ret;

    if (ka_task_spin_trylock_bh(&chan->lock) == 0) {
        return 0;
    }
    ret = trs_chan_cq_dispatch(chan, 64); /* 64 for tasklet reduce time consuming */
    ka_task_spin_unlock_bh(&chan->lock);
    return ret;
}

static void trs_chan_cq_dispatch_block(struct trs_chan *chan)
{
    (void)trs_chan_cq_dispatch(chan, chan->cq.para.cq_depth + 1);
}

static int trs_chan_irq_proc_cqs(struct trs_chan_ts_inst *ts_inst, u32 irq_type, u32 cqid[], u32 cq_num)
{
    u32 i;
    unsigned long timeout = ka_jiffies + (3 * KA_HZ);
    int ret = 0;

    for (i = 0; i < cq_num; i++) {
        struct trs_chan *chan = NULL;
        int chan_id;

        if (cqid[i] == KA_U32_MAX) {
            continue; /* in mia. if cqid equals KA_U32_MAX, it means the cqid is belong to vf. */
        }
        chan_id = (irq_type == TS_FUNC_CQ_IRQ) ? trs_chan_maint_cq_to_chan_id(ts_inst, cqid[i]) :
                                                 trs_chan_cq_to_chan_id(ts_inst, cqid[i]);
        chan = trs_chan_get(&ts_inst->inst, (u32)chan_id);
        if (chan != NULL) {
            chan->tasklet_running = 1;
            if (trs_chan_is_recv_block(chan) || ka_system_time_after(ka_jiffies, timeout) ||
                (chan->work_running == 1)) {
                ka_task_schedule_work(&chan->work);
                ret = -EBUSY;
            } else {
                ret = trs_chan_cq_dispatch_non_block(chan);
            }
            chan->tasklet_running = 0;
            trs_chan_put(chan);
        } else {
            trs_debug("Chan failed. (devid=%u; tsid=%u; cqid=%u; chan_id=%d)\n", ts_inst->inst.devid,
                      ts_inst->inst.tsid, cqid[i], chan_id);
        }
    }
    return ret;
}

void trs_chan_work(ka_work_struct_t *p_work)
{
    struct trs_chan *chan = ka_container_of(p_work, struct trs_chan, work);

    if (ka_base_atomic_read(&chan->chan_status) == 0) {
        return;
    }

    chan->work_running = 1;
    if (!trs_chan_is_recv_block(chan)) {
        (void)trs_chan_cq_dispatch_non_block(chan);
    } else {
        trs_chan_cq_dispatch_block(chan);
    }
    chan->work_running = 0;
}

static int trs_chan_irq_proc_list(struct trs_chan_irq_ctx *irq_ctx)
{
    struct trs_chan *chan = NULL;
    unsigned long timeout = ka_jiffies + (3 * KA_HZ);
    int ret = 0;

    ka_task_spin_lock_bh(&irq_ctx->lock);
    ka_list_for_each_entry(chan, &irq_ctx->chan_list, node)
    {
        if (!trs_chan_is_recv_block(chan) && ka_system_time_before(ka_jiffies, timeout) && (chan->work_running == 0)) {
            ret = trs_chan_cq_dispatch_non_block(chan);
        } else {
            ka_task_schedule_work(&chan->work);
            ret = -EBUSY;
        }
    }
    ka_task_spin_unlock_bh(&irq_ctx->lock);

    return ret;
}

int trs_chan_irq_proc(int irq_type, int irq_index, void *para, u32 cqid[], u32 cq_num)
{
    struct trs_chan_irq_ctx *irq_ctx = (struct trs_chan_irq_ctx *)para;

    if ((cqid != NULL) && (cq_num > 0)) {
        return trs_chan_irq_proc_cqs(irq_ctx->ts_inst, irq_ctx->irq_type, cqid, cq_num);
    } else {
        return trs_chan_irq_proc_list(irq_ctx);
    }
}

void trs_chan_tasklet(unsigned long data)
{
    struct trs_chan_irq_ctx *irq_ctx = (struct trs_chan_irq_ctx *)data;
    trs_chan_irq_proc_list(irq_ctx);
}

void trs_chan_cq_guard_work_proc(struct trs_chan_ts_inst *ts_inst)
{
    unsigned long cur_jiffies = ka_jiffies;
    struct trs_chan *chan = NULL;
    u64 cq_head, cq_tail, time_cost;
    u32 cqid;
    int ret;

    for (cqid = 0; cqid < ts_inst->cq_max_id; cqid++) {
        if (ts_inst->hw_cq_ctx[cqid].valid == false) {
            continue;
        }

        chan = trs_chan_get(&ts_inst->inst, ts_inst->hw_cq_ctx[cqid].chan_id);
        if (chan == NULL) {
            trs_debug("Failed to get chan. (devid=%u; cqid=%u; chan_id=%u)\n", ts_inst->inst.devid, cqid,
                      ts_inst->hw_cq_ctx[cqid].chan_id);
            continue;
        }

        if (chan->types.type != CHAN_TYPE_HW) {
            trs_chan_put(chan);
            continue;
        }

        if (chan->types.sub_type != CHAN_SUB_TYPE_HW_RTS) {
            trs_chan_put(chan);
            continue;
        }
        trs_try_resched(&cur_jiffies, 8); /* timeout is 8ms */

        ret = ts_inst->ops.sqcq_query(&chan->inst, &chan->types, cqid, QUERY_CMD_CQ_HEAD, &cq_head);
        ret |= ts_inst->ops.sqcq_query(&chan->inst, &chan->types, cqid, QUERY_CMD_CQ_TAIL, &cq_tail);
        if (ret != 0) {
            trs_debug("Failed to query cq head tail. (ret=%d; devid=%u; cqid=%u; chan_id=%u)\n", ret,
                      ts_inst->inst.devid, cqid, ts_inst->hw_cq_ctx[cqid].chan_id);
            trs_chan_put(chan);
            continue;
        }
        time_cost = trs_get_us_timestamp() - chan->interrupt_time_us;
        if ((cq_head != cq_tail) && (time_cost > 1000000) && (chan->tasklet_running == 0)) { /* 1s = 1000000us */
            trs_debug("Guard work process cq. (devid=%u; chan_id=%u; cqid=%u; cq_head=%llu; cq_tail=%llu; "
                      "time_cost=%llu us)\n",
                      ts_inst->inst.devid, ts_inst->hw_cq_ctx[cqid].chan_id, cqid, cq_head, cq_tail, time_cost);
            ka_task_schedule_work(&chan->work);
        }
        trs_chan_put(chan);
    }
}

static int trs_chan_reset_sq(struct trs_chan *chan)
{
    struct trs_chan_ts_inst *ts_inst = chan->ts_inst;
    int ret;

    ret = ts_inst->ops.sqcq_ctrl(&ts_inst->inst, &chan->types, chan->sq.sqid, CTRL_CMD_SQ_RESET, 0);
    if (ret != 0) {
        trs_err("Failed to reset sq. (devid=%u; sqid=%u; ret=%d)\n", ts_inst->inst.devid, chan->sq.sqid, ret);
        return ret;
    }
    chan->sq.sq_head = 0;
    chan->sq.sq_tail = 0;
    return 0;
}

static int trs_chan_pause_cq(struct trs_chan *chan, u32 cqid)
{
    struct trs_chan_ts_inst *ts_inst = chan->ts_inst;
    int ret;

    ret = ts_inst->ops.sqcq_ctrl(&ts_inst->inst, &chan->types, chan->cq.cqid, CTRL_CMD_CQ_PAUSE, 0);
    if (ret != 0) {
        trs_err("Failed to pause cq. (devid=%u; cqid=%u; ret=%d)\n", ts_inst->inst.devid, chan->cq.cqid, ret);
        return ret;
    }
    trs_debug("Pause cq success. (devid=%u; cqid=%u)\n", chan->ts_inst->inst.devid, chan->cq.cqid);
    return 0;
}

static int trs_chan_resume_cq(struct trs_chan *chan, u32 cqid)
{
    struct trs_chan_ts_inst *ts_inst = chan->ts_inst;
    int ret;

    ret = ts_inst->ops.sqcq_ctrl(&ts_inst->inst, &chan->types, chan->cq.cqid, CTRL_CMD_CQ_RESUME, 0);
    if (ret != 0) {
        trs_err("Failed to resume cq. (devid=%u; cqid=%u; ret=%d)\n", ts_inst->inst.devid, chan->cq.cqid, ret);
        return ret;
    }
    trs_debug("Resume cq success. (devid=%u; cqid=%u)\n", chan->ts_inst->inst.devid, chan->cq.cqid);
    return 0;
}

static int trs_chan_reset_cq(struct trs_chan *chan)
{
    struct trs_chan_ts_inst *ts_inst = chan->ts_inst;
    u32 cq_size = chan->cq.para.cqe_size * chan->cq.para.cq_depth;
    int ret;

    ret = ts_inst->ops.sqcq_ctrl(&ts_inst->inst, &chan->types, chan->cq.cqid, CTRL_CMD_CQ_RESET, 0);
    if (ret != 0) {
        trs_err("Failed to reset cq. (devid=%u; cqid=%u; ret=%d)\n", ts_inst->inst.devid, chan->cq.cqid, ret);
        return ret;
    }

    chan->cq.cq_head = 0;
    chan->cq.cq_tail = 0;
    chan->cq.loop = 0;
    if (chan->cq.cq_addr != NULL) {
        if (trs_chan_mem_is_local_mem(chan->cq.mem_attr.mem_type)) {
            (void)memset_s(chan->cq.cq_addr, cq_size, 0, cq_size);
        } else {
            ka_mm_memset_io(chan->cq.cq_addr, 0, cq_size);
        }
    }
    trs_debug("Reset cq success. (devid=%u; cqid=%u)\n", chan->ts_inst->inst.devid, chan->cq.cqid);

    return 0;
}

int trs_chan_ctrl(struct trs_id_inst *inst, int chan_id, u32 cmd, u32 para)
{
    struct trs_chan *chan = trs_chan_get(inst, chan_id);
    int ret;

    if (chan == NULL) {
        trs_err("Invalid para. (chan_id=%d; cmd=%u)\n", chan_id, cmd);
        return -EINVAL;
    }

    if (((cmd == CHAN_CTRL_CMD_CQ_SCHED) && !trs_chan_has_cq(chan)) ||
        ((cmd != CHAN_CTRL_CMD_CQ_SCHED) && !trs_chan_has_sq(chan))) {
        trs_chan_put(chan);
        trs_err("Chan no sq or cq. (devid=%u; tsid=%u, chan_id=%d; cmd=%u)\n", inst->devid, inst->tsid, chan_id, cmd);
        return -EINVAL;
    }

    switch (cmd) {
        case CHAN_CTRL_CMD_SQ_HEAD_UPDATE:
            ret = trs_chan_update_sq_head(chan, para);
            break;
        case CHAN_CTRL_CMD_SQ_HEAD_SET:
            ret = trs_chan_set_sq_head(chan, para);
            break;
        case CHAN_CTRL_CMD_SQ_TAIL_SET:
            ret = trs_chan_set_sq_tail(chan, para);
            break;
        case CHAN_CTRL_CMD_SQ_STATUS_SET:
            ret = trs_chan_set_sq_status(chan, para);
            break;
        case CHAN_CTRL_CMD_SQ_DISABLE_TO_ENABLE:
            ret = trs_chan_sq_disable_to_enable(chan, para);
            break;
        case CHAN_CTRL_CMD_CQ_SCHED:
            trs_chan_sched(chan);
            ret = 0;
            break;
        case CHAN_CTRL_CMD_NOT_NOTICE_TS:
            trs_chan_set_not_notice_ts(chan);
            ret = 0;
            break;
        case CHAN_CTRL_CMD_SQCQ_RESET:
            ret = trs_chan_reset_sq(chan);
            if (ret != 0) {
#ifndef EMU_ST
                break;
#endif
            }
            ret = trs_chan_reset_cq(chan);
            break;
        case CHAN_CTRL_CMD_CQ_PAUSE:
            ret = trs_chan_pause_cq(chan, para);
            break;
        case CHAN_CTRL_CMD_CQ_RESUME:
            ret = trs_chan_resume_cq(chan, para);
            break;
        default:
            trs_err("Invalid cmd. (devid=%u; tsid=%u, chan_id=%d; cmd=%u)\n", inst->devid, inst->tsid, chan_id, cmd);
            ret = -EINVAL;
            break;
    }

    trs_chan_put(chan);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_ctrl);

int trs_chan_query(struct trs_id_inst *inst, int chan_id, u32 cmd, u32 *value)
{
    struct trs_chan *chan = trs_chan_get(inst, chan_id);
    u64 tmp;
    int ret = 0;

    if (chan == NULL) {
        trs_err("Invalid para. (chan_id=%d; cmd=%u)\n", chan_id, cmd);
        return -EINVAL;
    }

    if (!trs_chan_has_sq(chan)) {
        trs_chan_put(chan);
        trs_err("Chan no sq. (devid=%u; tsid=%u, chan_id=%d; cmd=%u)\n", inst->devid, inst->tsid, chan_id, cmd);
        return -EINVAL;
    }

    switch (cmd) {
        case CHAN_QUERY_CMD_SQ_STATUS:
            ret = chan->ts_inst->ops.sqcq_query(inst, &chan->types, chan->sq.sqid, QUERY_CMD_SQ_STATUS, &tmp);
            *value = (u32)tmp;
            break;
        case CHAN_QUERY_CMD_SQ_HEAD:
            (void)trs_chan_update_sq_head_from_hw(chan);
            *value = chan->sq.sq_head;
            break;
        case CHAN_QUERY_CMD_SQ_TAIL:
            if (trs_chan_sqcq_is_agent(chan)) { /* agent sq task send by remote, local sq tail not set */
                (void)trs_chan_update_sq_tail_from_hw(chan);
            }
            *value = chan->sq.sq_tail;
            break;
        case CHAN_QUERY_CMD_SQ_POS:
            *value = chan->sq.pos;
            break;
        case CHAN_QUERY_CMD_CQ_HEAD:
            *value = chan->cq.cq_head;
            break;
        default:
            trs_err("Invalid cmd. (devid=%u; tsid=%u, chan_id=%d; cmd=%u)\n", inst->devid, inst->tsid, chan_id, cmd);
            ret = -EINVAL;
            break;
    }

    trs_chan_put(chan);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_query);

int trs_chan_dma_desc_destroy(struct trs_id_inst *inst, int chan_id)
{
    struct trs_chan *chan = NULL;
    chan = trs_chan_get(inst, chan_id);
    if (chan == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u; chan_id=%d)\n", inst->devid, inst->tsid, chan_id);
        return -EINVAL;
    }

    if (!trs_chan_has_sq(chan)) {
        trs_chan_put(chan);
        trs_err("Chan no sq. (devid=%u; tsid=%u, chan_id=%d)\n", inst->devid, inst->tsid, chan_id);
        return -EINVAL;
    }

    if (chan->ts_inst->ops.sq_dma_desc_destroy != NULL) {
        chan->ts_inst->ops.sq_dma_desc_destroy(inst, chan->sq.sqid);
    }

    trs_chan_put(chan);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_dma_desc_destroy);

int trs_chan_dma_desc_create(struct trs_id_inst *inst, int chan_id, struct trs_chan_dma_desc *para)
{
    struct trs_chan *chan = NULL;
    int ret = 0;

    chan = trs_chan_get(inst, chan_id);
    if (chan == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u; chan_id=%d)\n", inst->devid, inst->tsid, chan_id);
        return -EINVAL;
    }

    if (!trs_chan_has_sq(chan)) {
        trs_chan_put(chan);
        trs_err("Chan no sq. (devid=%u; tsid=%u, chan_id=%d)\n", inst->devid, inst->tsid, chan_id);
        return -EINVAL;
    }

    if ((para->sqe_pos >= chan->sq.para.sq_depth) ||
        (((chan->sq.para.sq_depth - para->sqe_pos) * chan->sq.para.sqe_size) < para->len)) {
        trs_chan_put(chan);
        trs_err("Sqe pos or len exceed. (devid=%u; tsid=%u, sqid=%d; sqe_pos=%u; depth=%u; sqe_size=%u)\n", inst->devid,
                inst->tsid, chan->sq.sqid, para->sqe_pos, chan->sq.para.sq_depth, chan->sq.para.sqe_size);
        return -EINVAL;
    }

    if (chan->ts_inst->ops.sq_dma_desc_create != NULL) {
        ret = chan->ts_inst->ops.sq_dma_desc_create(inst, para);
    } else {
        ret = -EOPNOTSUPP;
    }

    trs_chan_put(chan);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_dma_desc_create);

static int trs_chan_hw_info_to_string(struct trs_chan *chan, struct trs_chan_adapt_ops *ops, char *buff, u32 buff_len)
{
    u64 sq_head, sq_tail, cq_head, cq_tail, sq_status;
    int ret, len = 0;

    ret = sprintf_s(buff, buff_len, "    hw info: ");
    if ((ret > 0) &&
        (ops->sqcq_query(&chan->inst, &chan->types, chan->sq.sqid, QUERY_CMD_SQ_STATUS, &sq_status) == 0)) {
        len += ret;
        ret = sprintf_s(buff + len, buff_len - len, "sq status %llu ", sq_status);
    }

    if ((ret > 0) && (ops->sqcq_query(&chan->inst, &chan->types, chan->sq.sqid, QUERY_CMD_SQ_HEAD, &sq_head) == 0)) {
        len += ret;
        ret = sprintf_s(buff + len, buff_len - len, "sq head %llu ", sq_head);
    }

    if ((ret > 0) &&
        (ops->sqcq_query(&chan->inst, &chan->types, chan->sq.sqid, QUERY_CMD_STARS_SQ_HEAD, &sq_head) == 0)) {
        len += ret;
        ret = sprintf_s(buff + len, buff_len - len, "stars head %llu ", sq_head);
    }

    if ((ret > 0) && (ops->sqcq_query(&chan->inst, &chan->types, chan->sq.sqid, QUERY_CMD_SQ_TAIL, &sq_tail) == 0)) {
        len += ret;
        ret = sprintf_s(buff + len, buff_len - len, "sq tail %llu ", sq_tail);
    }

    if ((ret > 0) &&
        (ops->sqcq_query(&chan->inst, &chan->types, chan->sq.sqid, QUERY_CMD_STARS_SQ_TAIL, &sq_tail) == 0)) {
        len += ret;
        ret = sprintf_s(buff + len, buff_len - len, "stars tail %llu ", sq_tail);
    }

    if ((ret > 0) && (ops->sqcq_query(&chan->inst, &chan->types, chan->cq.cqid, QUERY_CMD_CQ_HEAD, &cq_head) == 0)) {
        len += ret;
        ret = sprintf_s(buff + len, buff_len - len, "cq head %llu ", cq_head);
    }

    if ((ret > 0) && (ops->sqcq_query(&chan->inst, &chan->types, chan->cq.cqid, QUERY_CMD_CQ_TAIL, &cq_tail) == 0)) {
        len += ret;
        ret = sprintf_s(buff + len, buff_len - len, "cq tail %llu ", cq_tail);
    }

    if (ret > 0) {
        len += ret;
        ret = sprintf_s(buff + len, buff_len - len, "\n");
    }

    return (ret > 0) ? len + ret : ret;
}

int _trs_chan_to_string(struct trs_chan *chan, char *buff, u32 buff_len)
{
    struct trs_chan_adapt_ops *ops = &chan->ts_inst->ops;
    int ret, len = 0;

    ret = sprintf_s(buff, buff_len, "    chan details:\n");
    if ((ret > 0) && trs_chan_has_sq(chan)) {
        len += ret;
        ret = sprintf_s(
            buff + len, buff_len - len,
            "    sq: id(%u),status(%u),head(%u),tail(%u),sqe_size(%u),sq_depth(%u),mem_type(0x%lx),use_virt(%d)\n"
            "      stat: tx(%llu),tx_full(%llu),tx_timeout(%llu)\n",
            chan->sq.sqid, chan->sq.status, chan->sq.sq_head, chan->sq.sq_tail, chan->sq.para.sqe_size,
            chan->sq.para.sq_depth, chan->sq.mem_attr.mem_type, (chan->sq.mem_attr.pa_num == 1) ? 0 : 1, chan->stat.tx,
            chan->stat.tx_full, chan->stat.tx_timeout);
    }

    if ((ret > 0) && trs_chan_has_cq(chan)) {
        len += ret;
        ret = sprintf_s(
            buff + len, buff_len - len,
            "    cq: id(%u),head(%u),loop(%u),cqe_size(%u),cq_depth(%u),mem_type(0x%lx)\n"
            "      stat: rx(%llu),rx_empty(%llu),rx_dispatch(%llu),rx_wakeup(%llu),hw_err(%llu), rx_in(%llu)\n",
            chan->cq.cqid, chan->cq.cq_head, chan->cq.loop, chan->cq.para.cqe_size, chan->cq.para.cq_depth,
            chan->cq.mem_attr.mem_type, chan->stat.rx, chan->stat.rx_empty, chan->stat.rx_dispatch,
            chan->stat.rx_wakeup, chan->stat.hw_err, chan->stat.rx_in);
    }

    if ((ret > 0) && (chan->types.type == CHAN_TYPE_HW)) {
        len += ret;
        ret = trs_chan_hw_info_to_string(chan, ops, buff + len, buff_len - len);
    }

    return ret;
}

int trs_chan_to_string(struct trs_id_inst *inst, int chan_id, char *buff, u32 buff_len)
{
    struct trs_chan *chan = trs_chan_get(inst, chan_id);
    int ret;

    if (chan == NULL) {
        trs_warn("Invalid para. (chan_id=%d)\n", chan_id);
        return -EINVAL;
    }

    ret = _trs_chan_to_string(chan, buff, buff_len);

    trs_chan_put(chan);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_to_string);
