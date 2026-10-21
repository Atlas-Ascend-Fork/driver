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

#ifdef CFG_FEATURE_TRACE_EVENT_FUNC
#include "pbl_uda.h"
#endif

#include "ka_base_pub.h"
#include "ka_fs_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_common_pub.h"
#include "ka_compiler_pub.h"

#include "securec.h"
#include "trs_chan.h"
#include "trs_id.h"
#include "trs_ts_inst.h"
#include "trs_proc.h"
#include "trs_timestamp.h"
#include "trs_proc_fs.h"
#include "trs_core_adapt.h"

#define PROC_FS_NAME_LEN 32
#define PROC_FS_MODE 0444
#define BUFF_LEN 1000

static ka_proc_dir_entry_t *top_entry = NULL;
static ka_proc_dir_entry_t *ts_inst_entry = NULL;
static ka_proc_dir_entry_t *proc_entry = NULL;

static char *res_name[TRS_CORE_MAX_ID_TYPE] = {
    "stream",
    "event",
    "notify",
    "model",
    "cmo",
    "cnt_notify",
    "hw_sq",
    "hw_cq",
    "sw_sq",
    "sw_cq",
    "cb_sq",
    "cb_cq",
    "logic_cq",
    "maint_sq",
    "maint_cq",
    "cdq",
    "stars_h2d_jetty",
    "stars_d2d_in_board_jetty",
    "stars_d2d_cross_board_jetty",
};

#ifdef CFG_FEATURE_TRACE_EVENT_FUNC
static int proc_trace_show(ka_seq_file_t *seq, void *offset)
{
    struct trs_core_ts_inst *ts_inst = (struct trs_core_ts_inst *)(uintptr_t)ka_fs_get_seq_file_private(seq);

    ka_fs_seq_printf(seq, "%d\n", ts_inst->trace_enable);
    return 0;
}

STATIC int proc_trace_ops_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, proc_trace_show, ka_base_pde_data(inode));
}

STATIC ssize_t proc_trace_ops_write(ka_file_t *filp, const char __ka_user *ubuf, size_t count, loff_t *ppos)
{
    ka_inode_t *inode = ka_fs_file_inode(filp);
    struct trs_core_ts_inst *ts_inst = (struct trs_core_ts_inst *)(uintptr_t)ka_base_pde_data(inode);
    char ch[2] = {0}; /* 2 bytes long */
    long val;

    if ((ppos == NULL) || (*ppos != 0) || (count != sizeof(ch)) || (ubuf == NULL)) {
        return -EINVAL;
    }

    if (!uda_can_access_udevid(ts_inst->inst.devid)) {
        return -EACCES;
    }

    if (ka_base_copy_from_user(ch, ubuf, count)) {
        return -ENOMEM;
    }

    ch[count - 1] = '\0';
    if (ka_base_kstrtol(ch, 10, &val)) {
        return -EFAULT;
    }
    ts_inst->trace_enable = (val == 0) ? false : true;
    return (ssize_t)count;
}

// clang-format off
static const ka_procfs_ops_t proc_trace_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) \
    ka_fs_init_pf_open(proc_trace_ops_open) \
    ka_fs_init_pf_read(ka_fs_seq_read) \
    ka_fs_init_pf_write(proc_trace_ops_write) \
    ka_fs_init_pf_lseek(ka_fs_seq_lseek) \
    ka_fs_init_pf_release(ka_fs_single_release) \
};
// clang-format on
#endif

static void trs_proc_show(struct trs_proc_ctx *proc_ctx, ka_seq_file_t *seq)
{
    ka_fs_seq_printf(seq, "pid %d(%s) task_id %lld devid %d status %d(0:normal, 1:exit) ssid %u\n", proc_ctx->pid,
                     proc_ctx->name, proc_ctx->task_id, proc_ctx->devid, proc_ctx->status, proc_ctx->cp_ssid);
}

static void ts_inst_proc_show(struct trs_core_ts_inst *ts_inst, ka_seq_file_t *seq)
{
    struct trs_proc_ctx *proc_ctx = NULL;

    ka_task_down_read(&ts_inst->sem);
    ka_fs_seq_printf(seq, "\nproc list:\n");
    ka_list_for_each_entry(proc_ctx, &ts_inst->proc_list_head, node)
    {
        trs_proc_show(proc_ctx, seq);
    }

    ka_fs_seq_printf(seq, "\nexit proc list:\n");
    ka_list_for_each_entry(proc_ctx, &ts_inst->exit_proc_list_head, node)
    {
        trs_proc_show(proc_ctx, seq);
    }
    ka_task_up_read(&ts_inst->sem);
}

static void ts_inst_res_show(struct trs_core_ts_inst *ts_inst, ka_seq_file_t *seq)
{
    int i;

    ka_fs_seq_printf(seq, "\nres list:\n");

    for (i = 0; i < TRS_CORE_MAX_ID_TYPE; i++) {
        struct trs_res_mng *res_mng = &ts_inst->res_mng[i];
        ka_fs_seq_printf(seq, "idx %d res %s id_num %u max_id %u use_num %u\n", i, res_name[i], res_mng->id_num,
                         res_mng->max_id, res_mng->use_num);
    }

    ka_fs_seq_printf(seq, "\nid_allocator res list:\n");
    for (i = 0; i < TRS_ID_TYPE_MAX; i++) {
        struct trs_id_stat stat;
        int ret;

        ret = trs_id_get_stat(&ts_inst->inst, i, &stat);
        if (ret == 0) {
            ka_fs_seq_printf(seq, "idx %d res %s use_num %u allocatable %u rsv_num %u\n", i, trs_id_type_to_name(i),
                             stat.alloc, stat.allocatable, stat.rsv_num);
        }
    }

    ka_fs_seq_printf(seq, "\nid_pool res list:\n");
    for (i = 0; i < TRS_ID_TYPE_MAX; i++) {
        u32 avail_num;
        int ret;

        ret = trs_id_get_avail_num_in_pool(&ts_inst->inst, i, &avail_num);
        if (ret == 0) {
            trs_core_printf_avail_num(ts_inst, seq, i, avail_num);
        }
    }
}

static int ts_inst_info_show(ka_seq_file_t *seq, void *offset)
{
    struct trs_core_ts_inst *ts_inst = (struct trs_core_ts_inst *)ka_fs_get_seq_file_private(seq);

    ka_fs_seq_printf(seq, "devid %d tsid %d hw_type %d(0:tscpu, 1:stars) support_proc_num %u(0:no_limit) ref %u\n",
                     ts_inst->inst.devid, ts_inst->inst.tsid, ts_inst->hw_type, ts_inst->support_proc_num,
                     kref_safe_read(&ts_inst->ref));
    if (ts_inst->inst.tsid == 0) {
        ts_inst_proc_show(ts_inst, seq);
    }
    ts_inst_res_show(ts_inst, seq);

    return 0;
}

STATIC int ts_inst_sum_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ts_inst_info_show, ka_base_pde_data(inode));
}

static void trs_chan_show(struct trs_core_ts_inst *ts_inst, ka_seq_file_t *seq, int chan_id)
{
    char buff[BUFF_LEN];
    if (trs_chan_to_string(&ts_inst->inst, chan_id, buff, BUFF_LEN) > 0) {
        ka_fs_seq_printf(seq, "%s\n", buff);
    }
}

static void trs_id_show(struct trs_core_ts_inst *ts_inst, ka_seq_file_t *seq, int type, u32 id)
{
    char buff[BUFF_LEN];
    if (trs_id_to_string(&ts_inst->inst, type, id, buff, BUFF_LEN) > 0) {
        ka_fs_seq_printf(seq, "    %s\n", buff);
    }
}

static void ts_inst_cb_phy_sqcq_show(struct trs_core_ts_inst *ts_inst, ka_seq_file_t *seq)
{
    struct trs_cb_ctx *cb_ctx = &ts_inst->cb_ctx;
    if (cb_ctx->phy_sqcq.chan_id >= 0) {
        ka_fs_seq_printf(seq, "cb cq_num %u phy sqid %u cqid %u chan %d\n", cb_ctx->cq_num, cb_ctx->phy_sqcq.sqid,
                         cb_ctx->phy_sqcq.cqid, cb_ctx->phy_sqcq.chan_id);
        trs_id_show(ts_inst, seq, TRS_SW_SQ_ID, cb_ctx->phy_sqcq.sqid);
        trs_id_show(ts_inst, seq, TRS_SW_CQ_ID, cb_ctx->phy_sqcq.cqid);
        trs_chan_show(ts_inst, seq, cb_ctx->phy_sqcq.chan_id);
    }
}

static void ts_inst_logic_phy_sqcq_show(struct trs_core_ts_inst *ts_inst, ka_seq_file_t *seq)
{
    struct trs_logic_cq_ctx *logic_cq_ctx = &ts_inst->logic_cq_ctx;
    if (logic_cq_ctx->phy_cq.chan_id >= 0) {
        ka_fs_seq_printf(seq, "logic cq_num %u phy cqid %u chan %d\n", logic_cq_ctx->cq_num, logic_cq_ctx->phy_cq.cqid,
                         logic_cq_ctx->phy_cq.chan_id);
        trs_id_show(ts_inst, seq, TRS_SW_CQ_ID, logic_cq_ctx->phy_cq.cqid);
        trs_chan_show(ts_inst, seq, logic_cq_ctx->phy_cq.chan_id);
    }
}

static void res_stream_detail_show(struct trs_core_ts_inst *ts_inst, u32 id, ka_seq_file_t *seq)
{
    struct trs_stream_ctx *stream_ctx = &ts_inst->stream_ctx[id];
    ka_fs_seq_printf(seq, "    host_pid %d sq %d cq %d logic_cq %d\n", stream_ctx->host_pid, stream_ctx->sq,
                     stream_ctx->cq, stream_ctx->logic_cq);
}

static int trs_hw_sq_to_string(struct trs_core_ts_inst *ts_inst, u32 id, char *buff, u32 buff_len)
{
    struct trs_sq_ctx *sq_ctx = &ts_inst->sq_ctx[id];
    int ret, len = 0;
    ret = sprintf_s(buff, buff_len, "    mode %s type %u flag %u cqid %u stream_id %u chan %d cp_proc %s\n",
                    (sq_ctx->mode == 0) ? "kio" : ((sq_ctx->mode == 1) ? "uio_t" : "uio_d"), sq_ctx->type, sq_ctx->flag,
                    sq_ctx->cqid, sq_ctx->stream_id, sq_ctx->chan_id,
                    (sq_ctx->reg_mem.cp_proc_state == 0) ? "exist" : "not exist");

    ka_task_spin_lock_bh(&sq_ctx->shr_info_lock);
    if ((ret > 0) && ((sq_ctx->mode == (u32)SEND_MODE_UIO_T) || (sq_ctx->mode == (u32)SEND_MODE_UIO_D)) &&
        (sq_ctx->shr_info.kva != NULL)) {
        struct trs_sq_shr_info *shr_info = (struct trs_sq_shr_info *)sq_ctx->shr_info.kva;
        len += ret;
        ret = sprintf_s(buff + len, buff_len - len, "    uio send ok %llu send full %llu send invalid in kernel %llu\n",
                        shr_info->send_ok, shr_info->send_full, sq_ctx->send_fail);
        if ((ts_inst->ops.get_connect_protocol != NULL) &&
            (ts_inst->ops.get_connect_protocol(&ts_inst->inst) == TRS_CONNECT_PROTOCOL_UB)) {
            len += ret;
            ret = sprintf_s(buff + len, buff_len - len, "    urma info: jfs_id %u jfc_id %u\n", shr_info->jfs_id,
                            shr_info->jfc_id);
        }
    }
    ka_task_spin_unlock_bh(&sq_ctx->shr_info_lock);

    if (ret > 0) {
        len += ret;
        ret = trs_chan_to_string(&ts_inst->inst, sq_ctx->chan_id, buff + len, buff_len - len);
    }

    return ret;
}

void trs_hw_sq_show(struct trs_core_ts_inst *ts_inst, u32 sqid)
{
    char buff[BUFF_LEN];
    if (trs_hw_sq_to_string(ts_inst, sqid, buff, BUFF_LEN) > 0) {
        trs_debug("%s", buff);
    }
}

static void res_hw_sq_detail_show(struct trs_core_ts_inst *ts_inst, u32 id, ka_seq_file_t *seq)
{
    char buff[BUFF_LEN];
    if (trs_hw_sq_to_string(ts_inst, id, buff, BUFF_LEN) > 0) {
        ka_fs_seq_printf(seq, "%s\n", buff);
    }
}

static int trs_hw_cq_stat_to_string(struct trs_cq_ctx *cq_ctx, char *buff, u32 buff_len)
{
    int len = 0;

    len = sprintf_s(buff, buff_len, "    chan %d, logic_cqid %d\n", cq_ctx->chan_id, cq_ctx->logic_cqid);
    if (len < 0) {
        return len;
    }
    len += sprintf_s(buff + len, buff_len - len, "    stat: rx %llu rx_enque %llu drop %llu logic_enque_invalid %llu\n",
                     cq_ctx->stat.rx, cq_ctx->stat.rx_enque, cq_ctx->stat.rx_drop, cq_ctx->stat.rx_enque_fail);
    return len;
}

void trs_hw_cq_show(struct trs_core_ts_inst *ts_inst, u32 cqid)
{
    struct trs_cq_ctx *cq_ctx = &ts_inst->cq_ctx[cqid];
    struct trs_id_inst *inst = &ts_inst->inst;
    char buff[BUFF_LEN];

    trs_info("Hw cq stat show. (devid=%u; tsid=%u; id=%u)\n", inst->devid, inst->tsid, cqid);
    if (trs_hw_cq_stat_to_string(cq_ctx, buff, BUFF_LEN) > 0) {
        trs_info("%s", buff);
    }
}

static void res_hw_cq_detail_show(struct trs_core_ts_inst *ts_inst, u32 id, ka_seq_file_t *seq)
{
    struct trs_cq_ctx *cq_ctx = &ts_inst->cq_ctx[id];
    char buff[BUFF_LEN];

    if (trs_hw_cq_stat_to_string(cq_ctx, buff, BUFF_LEN) > 0) {
        ka_fs_seq_printf(seq, "%s", buff);
    }

    if (!trs_is_stars_inst(ts_inst)) {
        trs_chan_show(ts_inst, seq, cq_ctx->chan_id);
    }
}

static void res_sw_sq_detail_show(struct trs_core_ts_inst *ts_inst, u32 id, ka_seq_file_t *seq)
{
    struct trs_sq_ctx *sq_ctx = &ts_inst->sw_sq_ctx[id];
    ka_fs_seq_printf(seq, "    cqid %u chan %d\n", sq_ctx->cqid, sq_ctx->chan_id);
    trs_chan_show(ts_inst, seq, sq_ctx->chan_id);
}

static void res_sw_cq_detail_show(struct trs_core_ts_inst *ts_inst, u32 id, ka_seq_file_t *seq)
{
    struct trs_cq_ctx *cq_ctx = &ts_inst->sw_cq_ctx[id];
    ka_fs_seq_printf(seq, "    chan %d logic_cqid %u\n", cq_ctx->chan_id, cq_ctx->logic_cqid);
    if (!trs_is_stars_inst(ts_inst)) {
        trs_chan_show(ts_inst, seq, cq_ctx->chan_id);
    }
}

static void res_cb_cq_detail_show(struct trs_core_ts_inst *ts_inst, u32 id, ka_seq_file_t *seq)
{
    struct trs_cb_cq *cb_cq = &ts_inst->cb_ctx.cq[id];
    ka_fs_seq_printf(seq, "    valid %u pid %d cq_depth %u cqe_size %u grpid %u\n", cb_cq->valid, cb_cq->pid,
                     cb_cq->cq_depth, cb_cq->cqe_size, cb_cq->grpid);
}

static int trs_logic_cq_to_string(struct trs_logic_cq *cq, char *buff, u32 buff_len)
{
    u64 timestamp = trs_get_s_timestamp();
    struct trs_logic_cqe *cqe;
    int len;
    int ret;

    ka_task_mutex_lock(&cq->mutex); // free logiccq and reading proc/x/summary from userspace is parallel
    len = sprintf_s(
        buff, buff_len,
        "    valid %u head %u tail %u cq_depth %u cqe_size %u thread_bind_irq %d wakeup_num %d wait_thread_num %d\n"
        "    enque %llu full_drop %llu wakeup %llu recv_in %llu recv %llu timeout %llu \n",
        cq->valid, cq->head, cq->tail, cq->cq_depth, cq->cqe_size, cq->thread_bind_irq,
        ka_base_atomic_read(&cq->wakeup_num), ka_base_atomic_read(&cq->wait_thread_num), cq->stat.enque,
        cq->stat.full_drop, cq->stat.wakeup, cq->stat.recv_in, cq->stat.recv, cq->stat.timeout);
    if ((len < 0) || (cq->head == cq->tail) || (cq->addr == NULL)) {
        ka_task_mutex_unlock(&cq->mutex);
        return len;
    }

    cqe = (struct trs_logic_cqe *)(cq->addr + (unsigned long)cq->head * cq->cqe_size);
    ret = sprintf_s(
        buff + len, buff_len - len,
        "    stranded cqe: pos=%u match_flag=%u timestamp=%llu(s) enque_timestamp=%llu(s) enque_time=%llu(s)"
        " stream_id=%u task_id=%u sqeType=%u sqId=%u sqHead=%u\n",
        cq->head, cqe->match_flag, timestamp, cqe->enque_timestamp, timestamp - cqe->enque_timestamp, cqe->stream_id,
        cqe->task_id, cqe->sqe_type, cqe->sq_id, cqe->sq_head);
    ka_task_mutex_unlock(&cq->mutex);
    return ret;
}

void trs_logic_cq_show(struct trs_core_ts_inst *ts_inst, u32 cqid)
{
    struct trs_logic_cq *cq = &ts_inst->logic_cq_ctx.cq[cqid];
    struct trs_id_inst *inst = &ts_inst->inst;
    char buff[BUFF_LEN];

    if (trs_logic_cq_to_string(cq, buff, BUFF_LEN) > 0) {
        trs_info_ratelimited("Logic cq stat show. (devid=%u; tsid=%u; id=%u; flag=%u)\n%s", inst->devid, inst->tsid,
                             cqid, cq->flag, buff);
    }
}

static void res_logic_cq_detail_show(struct trs_core_ts_inst *ts_inst, u32 id, ka_seq_file_t *seq)
{
    struct trs_logic_cq *cq = &ts_inst->logic_cq_ctx.cq[id];
    char buff[BUFF_LEN];
    if (trs_logic_cq_to_string(cq, buff, BUFF_LEN) > 0) {
        ka_fs_seq_printf(seq, "%s\n", buff);
    }
}

static void (*const res_detail_handles[TRS_CORE_MAX_ID_TYPE])(struct trs_core_ts_inst *ts_inst, u32 id,
                                                              ka_seq_file_t *seq) = {
    [TRS_STREAM] = res_stream_detail_show,    [TRS_HW_SQ] = res_hw_sq_detail_show, [TRS_HW_CQ] = res_hw_cq_detail_show,
    [TRS_SW_SQ] = res_sw_sq_detail_show,      [TRS_SW_CQ] = res_sw_cq_detail_show, [TRS_CB_CQ] = res_cb_cq_detail_show,
    [TRS_LOGIC_CQ] = res_logic_cq_detail_show};

static int res_detail_show(struct trs_res_mng *res_mng, int pid, ka_seq_file_t *seq)
{
    u32 i;

    for (i = 0; i < res_mng->max_id; i++) {
        struct trs_res_ids *id = &res_mng->ids[i];
        if ((id->ref != 0) && ((pid == id->pid) || (pid == 0))) {
            int res_type = trs_res_replace_res_type(res_mng->ts_inst, res_mng->res_type);
            ka_fs_seq_printf(seq, "id %u ref %d status %d pid %d\n", i, id->ref, id->status, id->pid);
            trs_id_show(res_mng->ts_inst, seq, trs_res_get_id_type(&res_mng->ts_inst->inst, res_type), i);
            if (res_detail_handles[res_mng->res_type] != NULL) {
                res_detail_handles[res_mng->res_type](res_mng->ts_inst, i, seq);
            }
        }
    }

    return 0;
}

static int ts_inst_res_detail_show(ka_seq_file_t *seq, void *offset)
{
    struct trs_res_mng *res_mng = (struct trs_res_mng *)ka_fs_get_seq_file_private(seq);

    if (res_mng->res_type == TRS_CB_CQ) {
        ts_inst_cb_phy_sqcq_show(res_mng->ts_inst, seq);
    } else if (res_mng->res_type == TRS_LOGIC_CQ) {
        ts_inst_logic_phy_sqcq_show(res_mng->ts_inst, seq);
    } else {
        /* do nothing */
    }

    res_detail_show(res_mng, 0, seq);

    return 0;
}

STATIC int ts_inst_res_detail_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, ts_inst_res_detail_show, ka_base_pde_data(inode));
}

static void proc_res_show(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, ka_seq_file_t *seq)
{
    int i;

    for (i = 0; i < TRS_CORE_MAX_ID_TYPE; i++) {
        int res_num = trs_get_proc_res_num(proc_ctx, ts_inst->inst.tsid, i);
        if (res_num != 0) {
            ka_fs_seq_printf(seq, "tsid %u res %s res_num %u\n", ts_inst->inst.tsid, res_name[i], res_num);
            res_detail_show(&ts_inst->res_mng[i], proc_ctx->pid, seq);
        }
    }
}

static void proc_ts_shm_show(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, ka_seq_file_t *seq)
{
    struct trs_shm_ctx *shm_ctx = &ts_inst->shm_ctx;
    u32 tsid = ts_inst->inst.tsid;

    if (shm_ctx->chan_id >= 0) {
        ka_fs_seq_printf(seq, "tsid %u shm sqid %u chan %d\n", tsid, shm_ctx->sqid, shm_ctx->chan_id);
        trs_chan_show(ts_inst, seq, shm_ctx->chan_id);
    }
}

static void proc_logic_cq_show(struct trs_proc_ctx *proc_ctx, u32 tsid, ka_seq_file_t *seq)
{
    int thread_bind_irq_num = ka_base_atomic_read(&proc_ctx->ts_ctx[tsid].thread_bind_irq_num);
    if (thread_bind_irq_num != 0) {
        ka_fs_seq_printf(seq, "tsid %u thread_bind_irq_num %d\n", tsid, thread_bind_irq_num);
    }
}

static int proc_sum_show(ka_seq_file_t *seq, void *offset)
{
    struct trs_proc_ctx *proc_ctx = (struct trs_proc_ctx *)ka_fs_get_seq_file_private(seq);
    u32 i;

    trs_proc_show(proc_ctx, seq);
    for (i = 0; i < TRS_TS_MAX_NUM; i++) {
        struct trs_core_ts_inst *ts_inst = trs_core_inst_get(proc_ctx->devid, i);
        if (ts_inst != NULL) {
            proc_res_show(proc_ctx, ts_inst, seq);
            proc_ts_shm_show(proc_ctx, ts_inst, seq);
            proc_logic_cq_show(proc_ctx, i, seq);
            trs_core_inst_put(ts_inst);
        }
    }

    return 0;
}

STATIC int proc_sum_open(ka_inode_t *inode, ka_file_t *file)
{
    return ka_fs_single_open(file, proc_sum_show, ka_base_pde_data(inode));
}

// clang-format off
static const ka_procfs_ops_t ts_inst_sum_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) \
    ka_fs_init_pf_open(ts_inst_sum_open) \
    ka_fs_init_pf_read(ka_fs_seq_read) \
    ka_fs_init_pf_lseek(ka_fs_seq_lseek) \
    ka_fs_init_pf_release(ka_fs_single_release) \
};

static const ka_procfs_ops_t ts_inst_res_detail_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) \
    ka_fs_init_pf_open(ts_inst_res_detail_open) \
    ka_fs_init_pf_read(ka_fs_seq_read) \
    ka_fs_init_pf_lseek(ka_fs_seq_lseek) \
    ka_fs_init_pf_release(ka_fs_single_release) \
};

static const ka_procfs_ops_t proc_sum_ops = {
    ka_fs_init_pf_owner(KA_THIS_MODULE) \
    ka_fs_init_pf_open(proc_sum_open) \
    ka_fs_init_pf_read(ka_fs_seq_read) \
    ka_fs_init_pf_lseek(ka_fs_seq_lseek) \
    ka_fs_init_pf_release(ka_fs_single_release) \
};
// clang-format on

static void proc_fs_format_ts_inst_dir_name(struct trs_core_ts_inst *ts_inst, char *name, int len)
{
    if (sprintf_s(name, len, "dev%u-ts%u", ts_inst->inst.devid, ts_inst->inst.tsid) <= 0) {
#ifndef EMU_ST
        trs_warn("Sprintf_s warn. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);
#endif
    }
}

static ka_proc_dir_entry_t *proc_fs_mk_ts_inst_dir(struct trs_core_ts_inst *ts_inst, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    proc_fs_format_ts_inst_dir_name(ts_inst, name, PROC_FS_NAME_LEN);
    return ka_fs_proc_mkdir((const char *)name, parent);
}

static void proc_fs_rm_ts_inst_dir(struct trs_core_ts_inst *ts_inst, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    proc_fs_format_ts_inst_dir_name(ts_inst, name, PROC_FS_NAME_LEN);
    (void)ka_fs_remove_proc_subtree((const char *)name, parent);
}

void proc_fs_add_ts_inst(struct trs_core_ts_inst *ts_inst)
{
    int i;

    ts_inst->entry = proc_fs_mk_ts_inst_dir(ts_inst, ts_inst_entry);
    if (ts_inst->entry == NULL) {
#ifndef EMU_ST
        trs_warn("Create entry warn. (devid=%u; tsid=%u)\n", ts_inst->inst.devid, ts_inst->inst.tsid);
#endif
        return;
    }

#ifdef CFG_FEATURE_TRACE_EVENT_FUNC
    (void)ka_fs_proc_create_data("trace_enable", PROC_FS_MODE, ts_inst->entry, &proc_trace_ops, ts_inst);
#endif
    (void)ka_fs_proc_create_data("summary", PROC_FS_MODE, ts_inst->entry, &ts_inst_sum_ops, ts_inst);

    for (i = 0; i < TRS_CORE_MAX_ID_TYPE; i++) {
        struct trs_res_mng *res_mng = &ts_inst->res_mng[i];
        (void)ka_fs_proc_create_data(res_name[i], PROC_FS_MODE, ts_inst->entry, &ts_inst_res_detail_ops, res_mng);
    }
}

void proc_fs_del_ts_inst(struct trs_core_ts_inst *ts_inst)
{
    proc_fs_rm_ts_inst_dir(ts_inst, ts_inst_entry);
}

static void proc_fs_format_pid_dir_name(struct trs_proc_ctx *proc_ctx, char *name, int len)
{
    if (sprintf_s(name, len, "%d-%lld-dev-%d", proc_ctx->pid, proc_ctx->task_id, proc_ctx->devid) <= 0) {
#ifndef EMU_ST
        trs_warn("Sprintf_s warn. (pid=%d; task_id=%lld)\n", proc_ctx->pid, proc_ctx->task_id);
#endif
    }
}

static ka_proc_dir_entry_t *proc_fs_mk_pid_dir(struct trs_proc_ctx *proc_ctx, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    proc_fs_format_pid_dir_name(proc_ctx, name, PROC_FS_NAME_LEN);
    return ka_fs_proc_mkdir((const char *)name, parent);
}

static void proc_fs_rm_pid_dir(struct trs_proc_ctx *proc_ctx, ka_proc_dir_entry_t *parent)
{
    char name[PROC_FS_NAME_LEN];

    proc_fs_format_pid_dir_name(proc_ctx, name, PROC_FS_NAME_LEN);
    (void)ka_fs_remove_proc_subtree((const char *)name, parent);
}

void proc_fs_add_pid(struct trs_proc_ctx *proc_ctx)
{
    proc_ctx->entry = proc_fs_mk_pid_dir(proc_ctx, proc_entry);
    if (proc_ctx->entry == NULL) {
#ifndef EMU_ST
        trs_warn("Create entry warn. (pid=%d)\n", proc_ctx->pid);
#endif
        return;
    }

    (void)ka_fs_proc_create_data("summary", PROC_FS_MODE, proc_ctx->entry, &proc_sum_ops, proc_ctx);
}

void proc_fs_del_pid(struct trs_proc_ctx *proc_ctx)
{
    proc_fs_rm_pid_dir(proc_ctx, proc_entry);
}

void trs_proc_fs_init(void)
{
    top_entry = ka_fs_proc_mkdir("trs_core", NULL);
    if (top_entry == NULL) {
        trs_err("create top entry dir failed\n");
        return;
    }

    ts_inst_entry = ka_fs_proc_mkdir("ts_inst", top_entry);
    if (ts_inst_entry == NULL) {
        trs_err("create ts_inst entry dir failed\n");
        return;
    }

    proc_entry = ka_fs_proc_mkdir("proc", top_entry);
    if (proc_entry == NULL) {
        trs_err("create proc entry dir failed\n");
        return;
    }

    return;
}

void trs_proc_fs_uninit(void)
{
    (void)ka_fs_remove_proc_subtree("trs_core", NULL);
}
