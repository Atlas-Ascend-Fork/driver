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
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_list_pub.h"
#include "ka_system_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "securec.h"
#include "trs_id.h"
#include "pbl/pbl_soc_res.h"
#include "pbl/pbl_uda.h"
#include "trs_res_id_def.h"
#include "trs_mailbox_def.h"

#include "chan_proc_fs.h"
#include "chan_rxtx.h"
#include "trs_chan.h"
#include "chan_init.h"

#define MAX_CHAN_NUM (64 * 2048)
#define TRS_SQCQ_SLOT_SIZE_MAX SQE_CACHE_SIZE
#define TRS_SQCQ_DEPTH_MAX 65535

u32 trs_chan_get_max_sq_depth(struct trs_chan_type *type);
u32 trs_chan_get_max_cq_depth(struct trs_chan_type *type);

#ifndef CFG_MEMORY_OPTIMIZE
u32 trs_chan_get_max_sq_depth(struct trs_chan_type *type)
{
    return TRS_SQCQ_DEPTH_MAX;
}

u32 trs_chan_get_max_cq_depth(struct trs_chan_type *type)
{
    return TRS_SQCQ_DEPTH_MAX;
}
#else
#define TRS_HW_SQ_DEPTH_MAX 4096
#define TRS_HW_CQ_DEPTH_MAX 1024
u32 trs_chan_get_max_sq_depth(struct trs_chan_type *type)
{
    return (type->type == CHAN_TYPE_HW) ? TRS_HW_SQ_DEPTH_MAX : TRS_SQCQ_DEPTH_MAX;
}

u32 trs_chan_get_max_cq_depth(struct trs_chan_type *type)
{
    return (type->type == CHAN_TYPE_HW) ? TRS_HW_CQ_DEPTH_MAX : TRS_SQCQ_DEPTH_MAX;
}
#endif

static KA_BASE_DEFINE_IDR(chan_idr);
static KA_TASK_DEFINE_RWLOCK(chan_idr_lock);

static void _trs_chan_get_sq_info(struct trs_chan *chan, struct trs_chan_sq_info *info)
{
    info->sqid = chan->sq.sqid;
    info->sq_dev_vaddr = chan->sq.sq_dev_vaddr;
    info->head_addr = chan->sq.head_addr;
    info->tail_addr = chan->sq.tail_addr;
    info->db_addr = chan->sq.db_addr;
    info->sq_para = chan->sq.para;
    info->mem_type = chan->sq.mem_attr.mem_type;
    info->addr_num = chan->sq.mem_attr.pa_num;
    info->sq_vaddr = chan->sq.sq_addr;
    if (chan->sq.mem_attr.vmap_pfn_handle != NULL) {
        info->vmap_pfn_handle = chan->sq.mem_attr.vmap_pfn_handle;
    }
    if (info->addr_num > 1) {
        info->sq_pa_wraper_list = chan->sq.mem_attr.pa_wraper_list;
    } else if (info->addr_num == 1) {
        info->sq_phy_addr = chan->sq.mem_attr.phy_addr;
    } else {
        info->sq_phy_addr = TRS_INVALID_PHYS_ADDR;
    }
}

static void _trs_chan_get_cq_info(struct trs_chan *chan, struct trs_chan_cq_info *info)
{
    info->cqid = chan->cq.cqid;
    info->irq = (int)chan->irq;
    info->cq_para = chan->cq.para;
    info->addr_num = chan->cq.mem_attr.pa_num;
    info->cq_vaddr = chan->cq.cq_addr;
    info->cq_phy_addr = chan->cq.mem_attr.phy_addr;
}

static struct trs_chan_irq_ctx *trs_chan_find_irq_ctx_with_least_chan(struct trs_chan_irq_ctx *irq_ctx, u32 irq_num)
{
    struct trs_chan_irq_ctx *irq_ctx_tmp = irq_ctx;
    struct trs_chan_irq_ctx *find = irq_ctx;
    u32 chan_num = 0xffff;
    u32 i;

    for (i = 0; i < irq_num; i++) {
        if (irq_ctx_tmp->chan_num < chan_num) {
            chan_num = irq_ctx_tmp->chan_num;
            find = irq_ctx_tmp;
        }
        irq_ctx_tmp++;
    }

    return find;
}

static void trs_chan_get_irq_ctx_base(struct trs_chan *chan, struct trs_chan_irq_ctx **irq_ctx, u32 *irq_num)
{
    if (chan->types.type == CHAN_TYPE_MAINT) {
        *irq_ctx = chan->ts_inst->maint_irq;
        *irq_num = chan->ts_inst->maint_irq_num;
    } else {
        *irq_ctx = chan->ts_inst->normal_irq;
        *irq_num = chan->ts_inst->normal_irq_num;
    }
}

static void trs_chan_irq_init(struct trs_chan *chan)
{
    struct trs_chan_irq_ctx *irq_ctx = NULL;
    u32 irq_num;

    trs_chan_get_irq_ctx_base(chan, &irq_ctx, &irq_num);
    if ((irq_ctx == NULL) || (irq_num == 0)) {
#ifndef EMU_ST
        trs_debug("No irq. (chan_type=%u; irq_num=%u)\n", chan->types.type, irq_num);
        return;
#endif
    }
    if ((chan->types.type == CHAN_TYPE_HW) && (chan->ts_inst->ops.get_cq_affinity_irq != NULL)) {
        if (chan->ts_inst->hw_cq_ctx[chan->cq.cqid].irq_index == KA_U32_MAX) {
            (void)chan->ts_inst->ops.get_cq_affinity_irq(&chan->inst, chan->cq.cqid,
                                                         &chan->ts_inst->hw_cq_ctx[chan->cq.cqid].irq_index);
        }
        irq_ctx += chan->ts_inst->hw_cq_ctx[chan->cq.cqid].irq_index % irq_num;
    } else {
        irq_ctx = trs_chan_find_irq_ctx_with_least_chan(irq_ctx, irq_num);
    }

    chan->irq = irq_ctx->irq;
    chan->irq_ctx = irq_ctx;
}

static void trs_chan_add_to_irq_list(struct trs_chan *chan)
{
    struct trs_chan_irq_ctx *irq_ctx = chan->irq_ctx;

    if (irq_ctx == NULL) {
        return; /* maybe not alloc cq, so cannot get chan irq ctx. */
    }

    ka_task_spin_lock_bh(&irq_ctx->lock);
    irq_ctx->chan_num++;
    ka_list_add_tail(&chan->node, &irq_ctx->chan_list);
    ka_task_spin_unlock_bh(&irq_ctx->lock);
}

static void trs_chan_del_from_irq_list(struct trs_chan *chan)
{
    struct trs_chan_irq_ctx *irq_ctx = chan->irq_ctx;

    if (irq_ctx == NULL) {
        return; /* maybe not alloc cq, so cannot get chan irq ctx. */
    }

    ka_task_spin_lock_bh(&irq_ctx->lock);
    irq_ctx->chan_num--;
    ka_list_del(&chan->node);
    ka_task_spin_unlock_bh(&irq_ctx->lock);
}

static int trs_chan_get_sq_type(int hw_type, u32 flag, struct trs_chan_type *types)
{
    if (types->type == CHAN_TYPE_HW) {
        if ((flag & (0x1 << CHAN_FLAG_RSV_SQ_ID_PRIOR_BIT)) != 0) {
            return TRS_RSV_HW_SQ_ID;
        } else {
            return TRS_HW_SQ_ID;
        }
    } else if (types->type == CHAN_TYPE_MAINT) {
        return TRS_MAINT_SQ_ID;
    } else {
        return (hw_type == TRS_HW_TYPE_STARS) ? TRS_SW_SQ_ID : TRS_HW_SQ_ID;
    }
}

static int trs_chan_get_cq_type(int hw_type, u32 flag, struct trs_chan_type *types)
{
    if (types->type == CHAN_TYPE_HW) {
        if ((flag & (0x1 << CHAN_FLAG_RSV_CQ_ID_PRIOR_BIT)) != 0) {
            return TRS_RSV_HW_CQ_ID;
        } else {
            return TRS_HW_CQ_ID;
        }
    } else if (types->type == CHAN_TYPE_MAINT) {
        return TRS_MAINT_CQ_ID;
    } else if (types->type == CHAN_TYPE_TASK_SCHED) {
        return TRS_TASK_SCHED_CQ_ID;
    } else {
        return (hw_type == TRS_HW_TYPE_STARS) ? TRS_SW_CQ_ID : TRS_HW_CQ_ID;
    }
}

static void sq_map_param_pack(struct trs_sq_mem_map_para *sq_map_param, struct trs_chan *chan,
                              struct trs_chan_sq_ctx *sq)
{
    sq_map_param->host_pid = chan->pid;
    sq_map_param->sq_para = sq->para;
    sq_map_param->sq_phy_addr = (sq->mem_attr.pa_num == 1) ? sq->mem_attr.phy_addr : 0;
    sq_map_param->chan_types = chan->types;
    sq_map_param->mem_type = sq->mem_attr.mem_type;
}

static int trs_chan_sq_mem_init(struct trs_chan_ts_inst *ts_inst, struct trs_chan *chan)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_sq_ctx *sq = &chan->sq;
    int ret;

    if ((sq->para.sq_depth == 0) || (sq->para.sq_depth > trs_chan_get_max_sq_depth(&chan->types)) ||
        (sq->para.sqe_size == 0) || (sq->para.sqe_size > TRS_SQCQ_SLOT_SIZE_MAX) ||
        ((sq->para.sqe_size % SQE_ALIGN_SIZE) != 0)) {
        trs_err("Invalid alloc para. (devid=%u; tsid=%u; sq_depth=%u; sqe_size=%u)\n", inst->devid, inst->tsid,
                sq->para.sq_depth, sq->para.sqe_size);
        return -EINVAL;
    }

    ret = ts_inst->ops.sqcq_query(&chan->inst, &chan->types, sq->sqid, QUERY_CMD_SQ_HEAD_PADDR, &sq->head_addr);
    ret |= ts_inst->ops.sqcq_query(&chan->inst, &chan->types, sq->sqid, QUERY_CMD_SQ_TAIL_PADDR, &sq->tail_addr);
    ret |= ts_inst->ops.sqcq_query(&chan->inst, &chan->types, sq->sqid, QUERY_CMD_SQ_DB_PADDR, &sq->db_addr);
    if (ret != 0) {
        trs_err("Get sq paddr failed. (devid=%u; tsid=%u; sqid=%u; ret=%d)\n", inst->devid, inst->tsid, sq->sqid, ret);
        return ret;
    }

    if ((ts_inst->location == UDA_REMOTE) &&
        (((chan->types.type == CHAN_TYPE_HW) && (chan->types.sub_type == CHAN_SUB_TYPE_HW_RTS)) ||
         ((chan->types.type == CHAN_TYPE_HW) && (chan->types.sub_type == CHAN_SUB_TYPE_HW_TOPIC_SCHED)))) {
        /* VM VF scene, rtsq memory is in pm */
        sq->mem_attr.phy_addr = TRS_INVALID_PHYS_ADDR;
        return 0;
    }

    sq->sq_addr = ts_inst->ops.sq_mem_alloc(inst, &chan->types, &sq->para, &sq->mem_attr);
    if ((sq->sq_addr == NULL) && (sq->mem_attr.vmap_pfn_handle == NULL)) {
        trs_err("Alloc sq mem failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -ENOMEM;
    }

    if (ts_inst->ops.sq_mem_map != NULL) {
        struct trs_sq_mem_map_para sq_map_param;
        sq_map_param_pack(&sq_map_param, chan, sq);
        ret = ts_inst->ops.sq_mem_map(inst, &sq_map_param, &sq->sq_dev_vaddr);
        if (ret != 0) {
            sq->sq_dev_vaddr = NULL;
            ts_inst->ops.sq_mem_free(inst, &chan->types, sq->sq_addr, &sq->mem_attr);
            trs_err("map sq mem failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
            return -EFAULT;
        }
    }
    return 0;
}

static int trs_chan_sq_init(struct trs_chan_ts_inst *ts_inst, struct trs_chan *chan)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int type = trs_chan_get_sq_type(ts_inst->hw_type, chan->flag, &chan->types);
    u32 id_flag = 0;
    int ret;

    if (trs_chan_specified_sq_id(chan)) {
        if ((ts_inst->ops.sqcq_speified_id_alloc == NULL)) {
            return -EOPNOTSUPP;
        }

        trs_id_set_specified_flag(&id_flag);
        if (trs_chan_ranged_sq_id(chan)) {
            trs_id_set_range_flag(&id_flag);
        }

        ret = ts_inst->ops.sqcq_speified_id_alloc(inst, type, id_flag, &chan->sq.sqid, chan->ext_msg);
        if (ret != 0) {
#ifndef EMU_ST
            trs_debug("Alloc sq id failed. (devid=%u; tsid=%u; id_flag=0x%x; ret=%d)\n", inst->devid, inst->tsid,
                      id_flag, ret);
#endif
            return ret;
        }
    } else if (trs_chan_rts_rsv_sq_id(chan)) {
        if ((ts_inst->ops.sqcq_rts_rsv_id_alloc == NULL)) {
            return -EOPNOTSUPP;
        }
        ret = ts_inst->ops.sqcq_rts_rsv_id_alloc(inst, type, &chan->sq.sqid);
        if (ret != 0) {
            trs_debug("Alloc sq id failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
            return ret;
        }
    } else {
        ret = trs_id_alloc(inst, type, &chan->sq.sqid);
        if (ret != 0) {
            trs_debug("Alloc sq id failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
            return ret;
        }
    }

    if (trs_chan_has_sq_mem(chan)) {
        ret = trs_chan_sq_mem_init(ts_inst, chan);
        if (ret != 0) {
            trs_err("Failed to sq mem init. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
            goto err;
        }
    }

    ka_task_init_waitqueue_head(&chan->sq.wait_queue);
    ka_task_sema_init(&chan->sq.sem, 1); /* allow one send thread op sq */
    ka_task_spin_lock_init(&chan->sq.lock);
    chan->sq.type = type;
    chan->sq.status = 1;

    if (chan->types.type == CHAN_TYPE_HW) {
        ts_inst->hw_sq_ctx[chan->sq.sqid].chan_id = chan->id;
    }

    if (chan->types.type == CHAN_TYPE_MAINT) {
        ts_inst->maint_sq_ctx[chan->sq.sqid].chan_id = chan->id;
    }

    trs_debug("Alloc sq. (devid=%u; tsid=%u; flag=0x%x; id=%d; sqid=%u; mem_type=0x%x)\n", inst->devid, inst->tsid,
              chan->flag, chan->id, chan->sq.sqid, chan->sq.mem_attr.mem_type);

    return 0;

err:
    if (trs_chan_specified_sq_id(chan) && (ts_inst->ops.sqcq_speified_id_free != NULL)) {
#ifndef EMU_ST
        (void)ts_inst->ops.sqcq_speified_id_free(inst, type, id_flag, chan->sq.sqid);
#endif
    } else {
        (void)trs_id_free(inst, type, chan->sq.sqid);
    }
    return ret;
}

static void trs_chan_sq_uninit(struct trs_chan_ts_inst *ts_inst, struct trs_chan *chan)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int type = chan->sq.type;
    u32 id_flag = 0;

    if (chan->types.type == CHAN_TYPE_HW) {
        ts_inst->hw_sq_ctx[chan->sq.sqid].chan_id = -1;
    }

    if (chan->types.type == CHAN_TYPE_MAINT) {
        ts_inst->maint_sq_ctx[chan->sq.sqid].chan_id = -1;
    }

    if (ts_inst->ops.sq_mem_unmap != NULL) {
        struct trs_sq_mem_map_para sq_map_param;
        sq_map_param_pack(&sq_map_param, chan, &chan->sq);
        (void)ts_inst->ops.sq_mem_unmap(inst, &sq_map_param, chan->sq.sq_dev_vaddr);
        chan->sq.sq_dev_vaddr = NULL;
    }

    if (chan->ts_inst->ops.sq_dma_desc_destroy != NULL) {
        chan->ts_inst->ops.sq_dma_desc_destroy(inst, chan->sq.sqid);
    }

    if (chan->sq.sq_addr != NULL) {
        ts_inst->ops.sq_mem_free(inst, &chan->types, chan->sq.sq_addr, &chan->sq.mem_attr);
        chan->sq.sq_addr = NULL;
    }

    if (trs_chan_reserved_sq_id(chan)) {
        trs_id_set_reserved_flag(&id_flag);
    }

    if (trs_chan_specified_sq_id(chan) && (ts_inst->ops.sqcq_speified_id_free != NULL)) {
        if (trs_chan_ranged_sq_id(chan)) {
            trs_id_set_range_flag(&id_flag);
        }

        (void)ts_inst->ops.sqcq_speified_id_free(inst, type, id_flag, chan->sq.sqid);
    } else {
        (void)trs_id_free_ex(inst, type, id_flag, chan->sq.sqid);
    }
}

static int trs_chan_cq_mem_init(struct trs_chan_ts_inst *ts_inst, struct trs_chan *chan)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan_cq_ctx *cq = &chan->cq;

    if ((chan->cq.para.cq_depth == 0) || (chan->cq.para.cq_depth > trs_chan_get_max_cq_depth(&chan->types)) ||
        (chan->cq.para.cqe_size == 0) || (chan->cq.para.cqe_size > TRS_SQCQ_SLOT_SIZE_MAX) ||
        ((chan->cq.para.cqe_size % CQE_ALIGN_SIZE) != 0)) {
        trs_err("Invalid alloc para. (devid=%u; tsid=%u; cq_depth=%u; cqe_size=%u)\n", inst->devid, inst->tsid,
                chan->cq.para.cq_depth, chan->cq.para.cqe_size);
        return -EINVAL;
    }

    chan->cq.cq_addr = ts_inst->ops.cq_mem_alloc(inst, &chan->types, &cq->para, &cq->mem_attr);
    if (chan->cq.cq_addr == NULL) {
        trs_err("Alloc cq mem failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -ENOMEM;
    }

    return 0;
}

static int trs_chan_cq_init(struct trs_chan_ts_inst *ts_inst, struct trs_chan *chan)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int type = trs_chan_get_cq_type(ts_inst->hw_type, chan->flag, &chan->types);
    u32 id_flag = 0;
    int ret;

    if (trs_chan_specified_cq_id(chan)) {
        if ((ts_inst->ops.sqcq_speified_id_alloc == NULL)) {
            return -EOPNOTSUPP;
        }

        trs_id_set_specified_flag(&id_flag);
        if (trs_chan_ranged_cq_id(chan)) {
            trs_id_set_range_flag(&id_flag);
        }

        ret = ts_inst->ops.sqcq_speified_id_alloc(inst, type, id_flag, &chan->cq.cqid, chan->ext_msg);
        if (ret != 0) {
#ifndef EMU_ST
            trs_debug("Alloc cq id failed. (devid=%u; tsid=%u; id_flag=0x%x; ret=%d)\n", inst->devid, inst->tsid,
                      id_flag, ret);
#endif
            return ret;
        }
    } else if (trs_chan_rts_rsv_cq_id(chan)) {
        if ((ts_inst->ops.sqcq_rts_rsv_id_alloc == NULL)) {
            return -EOPNOTSUPP;
        }
        ret = ts_inst->ops.sqcq_rts_rsv_id_alloc(inst, type, &chan->cq.cqid);
        if (ret != 0) {
            trs_debug("Alloc cq id failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
            return ret;
        }
    } else {
        ret = trs_id_alloc(inst, type, &chan->cq.cqid);
        if ((ret != 0) || ((chan->types.type == CHAN_TYPE_HW) && (chan->cq.cqid >= ts_inst->cq_max_id))) {
            trs_debug("Alloc cq id failed. (devid=%u; tsid=%u; type=%u; cqid=%u; ret=%d)\n", inst->devid, inst->tsid,
                      chan->types.type, chan->cq.cqid, ret);
            return (ret != 0) ? ret : -EFAULT;
        }
    }

    if (trs_chan_has_cq_mem(chan)) {
        ret = trs_chan_cq_mem_init(ts_inst, chan);
        if (ret != 0) {
            trs_err("Failed to cq mem init. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
            goto err;
        }
    }

    ka_task_init_waitqueue_head(&chan->cq.wait_queue);
    ka_task_mutex_init(&chan->cq.mutex);
    ka_base_atomic_set(&chan->chan_status, 1);
    KA_TASK_INIT_WORK(&chan->work, trs_chan_work);
    chan->cq.type = type;

    trs_chan_irq_init(chan);

    if (chan->types.type == CHAN_TYPE_HW) {
        ts_inst->hw_cq_ctx[chan->cq.cqid].chan_id = chan->id;
        ts_inst->hw_cq_ctx[chan->cq.cqid].valid = true;
    }

    if (chan->types.type == CHAN_TYPE_MAINT) {
        ts_inst->maint_cq_ctx[chan->cq.cqid].chan_id = chan->id;
    }

    trs_debug("Alloc cq. (devid=%u; tsid=%u; flag=0x%x; id=%d, cqid=%u)\n", inst->devid, inst->tsid, chan->flag,
              chan->id, chan->cq.cqid);

    return 0;

err:
    if (trs_chan_specified_cq_id(chan) && (ts_inst->ops.sqcq_speified_id_free != NULL)) {
#ifndef EMU_ST
        (void)ts_inst->ops.sqcq_speified_id_free(inst, type, id_flag, chan->cq.cqid);
#endif
    } else {
        (void)trs_id_free(inst, type, chan->cq.cqid);
    }
    return ret;
}

static void trs_chan_cq_uninit(struct trs_chan_ts_inst *ts_inst, struct trs_chan *chan)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    int type = chan->cq.type;
    u32 id_flag = 0;

    if (chan->types.type == CHAN_TYPE_HW) {
        ts_inst->hw_cq_ctx[chan->cq.cqid].chan_id = -1;
        ts_inst->hw_cq_ctx[chan->cq.cqid].valid = false;
    }

    if (chan->types.type == CHAN_TYPE_MAINT) {
        ts_inst->maint_cq_ctx[chan->cq.cqid].chan_id = -1;
    }

    if (ka_base_atomic_read(&chan->chan_status)) {
        ka_base_atomic_set(&chan->chan_status, 0);
        (void)ka_task_cancel_work_sync(&chan->work);
        trs_debug("Cancel work success. (devid=%u; tsid=%u; chan_id=%d)\n", chan->inst.devid, chan->inst.tsid,
                  chan->id);
    }

    if (chan->cq.cq_addr != NULL) {
        ts_inst->ops.cq_mem_free(inst, &chan->types, chan->cq.cq_addr, &chan->cq.mem_attr);
        chan->cq.cq_addr = NULL;
    }
    ka_task_mutex_destroy(&chan->cq.mutex);

    if (trs_chan_reserved_cq_id(chan)) {
        trs_id_set_reserved_flag(&id_flag);
    }

    if (trs_chan_specified_cq_id(chan) && (ts_inst->ops.sqcq_speified_id_free != NULL)) {
        if (trs_chan_ranged_cq_id(chan)) {
            trs_id_set_range_flag(&id_flag);
        }

        (void)ts_inst->ops.sqcq_speified_id_free(inst, type, id_flag, chan->cq.cqid);
    } else {
        (void)trs_id_free_ex(inst, type, id_flag, chan->cq.cqid);
    }
}

static int trs_chan_notice_ts(struct trs_chan *chan, u32 op)
{
    struct trs_id_inst *inst = &chan->ts_inst->inst;
    struct trs_chan_info info = {0};
    int ret;

    info.op = op;
    info.ssid = chan->ssid;
    info.types = chan->types;
    info.pid = chan->pid;
    info.irq_type = (chan->types.type == CHAN_TYPE_MAINT) ? TS_FUNC_CQ_IRQ : TS_CQ_UPDATE_IRQ;

    _trs_chan_get_sq_info(chan, &info.sq_info);
    _trs_chan_get_cq_info(chan, &info.cq_info);

    ret = memcpy_s(info.msg, sizeof(info.msg), chan->msg, sizeof(chan->msg));
    if (ret != 0) {
        trs_err("Memcopy failed. (dest_len=%lx; src_len=%lx)\n", sizeof(info.msg), sizeof(chan->msg));
        return ret;
    }

    info.ext_msg = chan->ext_msg;
    info.ext_msg_len = chan->ext_msg_len;

    if ((chan->flag & (0x1 << CHAN_FLAG_USE_MASTER_PID_BIT)) != 0) {
        info.master_pid_flag = 1;
    }

    if ((chan->flag & (0x1 << CHAN_FLAG_REMOTE_ID_BIT)) != 0) {
        info.remote_id_flag = 1;
    }

    if ((chan->flag & (0x1 << CHAN_FLAG_AGENT_ID_BIT)) != 0) {
        info.agent_id_flag = 1;
    }

    if ((chan->flag & (0x1 << CHAN_FLAG_NO_CQ_MEM_BIT)) != 0) {
        info.no_cq_mem_flag = 1;
    }

    if ((chan->flag & (0x1 << CHAN_FLAG_NO_SQ_MEM_BIT)) != 0) {
        info.sq_info.sq_phy_addr = 0;
    }

    if (chan->sq.mem_attr.pa_num > 1) {
        info.sq_addr_is_virtual = 1;
    }

    trs_debug("Info. (pid=%d; master_pid=%d; flag=0x%x)\n", ka_task_get_current_tgid(), info.msg[SQCQ_INFO_LENGTH - 1],
              chan->flag);

    return chan->ts_inst->ops.notice_ts(inst, &info);
}

static void trs_chan_wait_hw_stop(struct trs_chan *chan)
{
    struct trs_chan_adapt_ops *ops = &chan->ts_inst->ops;
    struct trs_id_inst *id_inst = &chan->ts_inst->inst;
    u64 sq_head, sq_tail, last_sq_head;
    int ret, timeout_cnt = 0;

    ret = ops->sqcq_query(id_inst, &chan->types, chan->sq.sqid, QUERY_CMD_SQ_HEAD, &sq_head);
    ret |= ops->sqcq_query(id_inst, &chan->types, chan->sq.sqid, QUERY_CMD_SQ_TAIL, &sq_tail);
    if (ret != 0) {
        return;
    }
    if (sq_head == sq_tail) {
        return;
    }

    trs_warn("Abnormal free. (devid=%u; tsid=%u; chan_id=%u; sqId=%u; cqId=%u; sqHead=%u; sqTail=%u)\n", id_inst->devid,
             id_inst->tsid, chan->id, chan->sq.sqid, chan->cq.cqid, (u32)sq_head, (u32)sq_tail);
    (void)ops->sqcq_ctrl(id_inst, &chan->types, chan->sq.sqid, CTRL_CMD_SQ_STATUS_SET, 0);
    (void)ops->sqcq_query(id_inst, &chan->types, chan->sq.sqid, QUERY_CMD_SQ_HEAD, &last_sq_head);
    while ((timeout_cnt < 50) && (TRS_IS_REBOOT_ACTIVE == false)) { /* 50 timeout */
        (void)ops->sqcq_query(id_inst, &chan->types, chan->sq.sqid, QUERY_CMD_SQ_HEAD, &sq_head);
        if ((sq_head != last_sq_head) || (sq_head == sq_tail)) { /* means sq head has moved. */
            break;
        } else {
            timeout_cnt++;
#ifndef EMU_ST
            ka_system_msleep(100); /* sleep 100 ms */
            trs_warn("Wait task done. (devid=%u; tsid=%u; last_sq_head=%u; sq_head=%u; sq_tail=%u; timeout_cnt=%d)\n",
                     id_inst->devid, id_inst->tsid, (u32)last_sq_head, (u32)sq_head, (u32)sq_tail, timeout_cnt);
#endif
        }
    }
}

static void trs_chan_inst_uninit(struct trs_chan_ts_inst *ts_inst, struct trs_chan *chan)
{
    bool wait_hw_stop = true;
    int ret;

    if (trs_chan_is_notice_ts(chan)) {
        ret = trs_chan_notice_ts(chan, 0);
        wait_hw_stop = (ret != 0) ? true : false;
    }

    if (wait_hw_stop) {
        trs_chan_wait_hw_stop(chan);
    }

    if (trs_chan_has_sq(chan)) {
        trs_chan_sq_uninit(ts_inst, chan);
    }

    if (trs_chan_has_cq(chan)) {
        trs_chan_cq_uninit(ts_inst, chan);
    }
}

static void _trs_chan_inst_release(struct trs_chan *chan)
{
    struct trs_id_inst *inst = &chan->ts_inst->inst;

    trs_debug("Release success. (devid=%u; tsid=%u; id=%d; flag=%x)\n", inst->devid, inst->tsid, chan->id, chan->flag);

    chan_proc_fs_del_chan(chan);
    trs_chan_del_from_irq_list(chan);
    trs_chan_inst_uninit(chan->ts_inst, chan);
    trs_chan_ts_inst_put(chan->ts_inst);
    trs_kfree(chan);
}

static int trs_chan_inst_init(struct trs_chan_ts_inst *ts_inst, struct trs_chan_para *para, struct trs_chan *chan)
{
    int ret;

    chan->ts_inst = ts_inst;
    chan->inst = ts_inst->inst;
    chan->types = para->types;
    chan->ops = para->ops;
    chan->sq.para = para->sq_para;
    chan->cq.para = para->cq_para;
    chan->ssid = para->ssid;
    chan->pid = ka_task_get_current_tgid();
    kref_safe_init(&chan->ref);
    ka_task_spin_lock_init(&chan->lock);

    ret = memcpy_s(chan->msg, sizeof(chan->msg), para->msg, sizeof(para->msg));
    if (ret != 0) {
        trs_err("Memcopy failed. (dest_len=%lx; src_len=%lx)\n", sizeof(chan->msg), sizeof(para->msg));
        return -EFAULT;
    }

    chan->ext_msg = para->ext_msg;
    chan->ext_msg_len = para->ext_msg_len;
    chan->flag = para->flag & ((0x1 << CHAN_FLAG_SPECIFIED_SQ_ID_BIT) | (0x1 << CHAN_FLAG_SPECIFIED_CQ_ID_BIT) |
                               (0x1 << CHAN_FLAG_NO_CQ_MEM_BIT) | (0x1 << CHAN_FLAG_USE_MASTER_PID_BIT) |
                               (0x1 << CHAN_FLAG_NO_SQ_MEM_BIT) | (0x1 << CHAN_FLAG_RSV_SQ_ID_PRIOR_BIT) |
                               (0x1 << CHAN_FLAG_RSV_CQ_ID_PRIOR_BIT) | (0x1 << CHAN_FLAG_RANGE_SQ_ID_BIT) |
                               (0x1 << CHAN_FLAG_RANGE_CQ_ID_BIT) | (0x1 << CHAN_FLAG_REMOTE_ID_BIT) |
                               (0x1 << CHAN_FLAG_AGENT_ID_BIT) | (0x1 << CHAN_FLAG_RTS_RSV_SQ_ID_BIT) |
                               (0x1 << CHAN_FLAG_RTS_RSV_CQ_ID_BIT) | (0x1U << CHAN_FLAG_SPECIFIED_SQ_MEM_BIT));

    if ((para->flag & (0x1 << CHAN_FLAG_ALLOC_SQ_BIT)) != 0) {
        chan->sq.sqid = para->sqid;
        ret = trs_chan_sq_init(ts_inst, chan);
        if (ret != 0) {
            return ret;
        }
        chan->flag |= (0x1 << CHAN_FLAG_ALLOC_SQ_BIT);
    }

    if ((para->flag & (0x1 << CHAN_FLAG_ALLOC_CQ_BIT)) != 0) {
        chan->cq.cqid = para->cqid;
        ret = trs_chan_cq_init(ts_inst, chan);
        if (ret != 0) {
            trs_chan_inst_uninit(ts_inst, chan);
            return ret;
        }
        chan->flag |= (0x1 << CHAN_FLAG_ALLOC_CQ_BIT);
        if ((para->flag & (0x1 << CHAN_FLAG_RECV_BLOCK_BIT)) != 0) {
            chan->flag |= (0x1 << CHAN_FLAG_RECV_BLOCK_BIT);
        }
    }

    if ((para->flag & (0x1 << CHAN_FLAG_AGENT_ID_BIT)) != 0) {
        chan->pid = para->msg[SQCQ_INFO_LENGTH - 1];
    }

    if ((para->flag & (0x1 << CHAN_FLAG_NOTICE_TS_BIT)) != 0) {
        ret = trs_chan_notice_ts(chan, 1);
        if (ret != 0) {
            trs_chan_inst_uninit(ts_inst, chan);
            return ret;
        }
        chan->flag |= (0x1 << CHAN_FLAG_NOTICE_TS_BIT);
    }

    if ((para->flag & (0x1 << CHAN_FLAG_AUTO_UPDATE_SQ_HEAD_BIT)) != 0) {
        chan->flag |= (0x1 << CHAN_FLAG_AUTO_UPDATE_SQ_HEAD_BIT);
    }

    /* It should be added to irq ctx chan list after tsfw init sqcq done.
       Otherwise, tasklet may handle cqe when chan not be inited completely. */
    trs_chan_add_to_irq_list(chan);

    return 0;
}

static void trs_chan_id_alloc(struct trs_chan *chan)
{
    ka_task_write_lock_bh(&chan_idr_lock);
    chan->id = ka_base_idr_alloc_cyclic(&chan_idr, chan, 0, MAX_CHAN_NUM, KA_GFP_ATOMIC);
    ka_task_write_unlock_bh(&chan_idr_lock);
}

static void trs_chan_id_free(struct trs_chan *chan)
{
    ka_task_write_lock_bh(&chan_idr_lock);
    (void)ka_base_idr_remove(&chan_idr, chan->id);
    ka_task_write_unlock_bh(&chan_idr_lock);
}

static int trs_chan_inst_create(struct trs_chan_ts_inst *ts_inst, struct trs_chan_para *para, int *chan_id)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_chan *chan = NULL;
    int ret;

    chan = trs_kzalloc(sizeof(struct trs_chan), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (chan == NULL) {
        trs_err("Mem alloc failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -ENOMEM;
    }

    trs_chan_id_alloc(chan);
    if (chan->id < 0) {
        trs_kfree(chan);
        trs_err("Alloc chan id failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EFAULT;
    }

    ret = trs_chan_inst_init(ts_inst, para, chan);
    if (ret != 0) {
        trs_chan_id_free(chan);
        trs_kfree(chan);
        return ret;
    }

    *chan_id = chan->id;
    chan_proc_fs_add_chan(chan);

    trs_debug("Create success. (devid=%u; tsid=%u; id=%d; type=%u; sub_type=%u; flag=%x)\n", inst->devid, inst->tsid,
              chan->id, chan->types.type, chan->types.sub_type, chan->flag);

    return 0;
}

static void trs_chan_inst_release(struct kref_safe *kref)
{
    struct trs_chan *chan = ka_container_of(kref, struct trs_chan, ref);
    _trs_chan_inst_release(chan);
}

static void trs_chan_inst_destroy_sync(struct trs_chan *chan)
{
    if (trs_chan_reserved_sq_id(chan) || trs_chan_reserved_cq_id(chan)) {
#ifndef EMU_ST
        while (kref_safe_read(&chan->ref) > 1) { /* ref == 1 means other async task has been done */
            ka_system_usleep_range(1000, 1100);  /* min 1000 us, max 1100 us */
        }
#endif
    }
}

static void trs_chan_inst_destroy(struct trs_chan *chan)
{
    struct trs_chan_ts_inst *ts_inst = chan->ts_inst;
    struct trs_id_inst *inst = &ts_inst->inst;

    if (trs_chan_has_sq(chan)) {
        trs_chan_submit_wakeup(chan);
    }

    if (trs_chan_has_cq(chan)) {
        trs_chan_fetch_wakeup(chan);
    }

    trs_chan_inst_destroy_sync(chan);

    trs_debug("Destroy success. (devid=%u; tsid=%u; id=%d; flag=0x%x)\n", inst->devid, inst->tsid, chan->id,
              chan->flag);

    kref_safe_put(&chan->ref, trs_chan_inst_release);
}

struct trs_chan *trs_chan_get(struct trs_id_inst *inst, u32 chan_id)
{
    struct trs_chan *chan = NULL;

    if (trs_id_inst_check(inst) != 0) {
        return NULL;
    }

    ka_task_read_lock_bh(&chan_idr_lock);
    chan = (struct trs_chan *)ka_base_idr_find(&chan_idr, chan_id);
    if (chan != NULL) {
        if ((chan->inst.devid == inst->devid) && (chan->inst.tsid == inst->tsid) && (chan->id == chan_id)) {
            kref_safe_get(&chan->ref);
        } else {
            ka_task_read_unlock_bh(&chan_idr_lock);
            return NULL;
        }
    }
    ka_task_read_unlock_bh(&chan_idr_lock);

    return chan;
}

void trs_chan_put(struct trs_chan *chan)
{
    kref_safe_put(&chan->ref, trs_chan_inst_release);
}

int trs_sq_switch_stream_chan_update(struct trs_id_inst *inst, u32 chan_id, u32 sq_depth)
{
    struct trs_chan *chan = trs_chan_get(inst, chan_id);
    if (chan == NULL) {
        trs_err("Failed to get chan. (devid=%u; chan_id=%u)\n", inst->devid, chan_id);
        return -ENODEV;
    }

    if (sq_depth > TRS_SQCQ_DEPTH_MAX) {
        trs_err("Invalid sq depth. (sq_depth=%u)\n", sq_depth);
        trs_chan_put(chan);
        return -EINVAL;
    }
    chan->sq.para.sq_depth = sq_depth;
    trs_debug("Update sq depth. (sqid=%u; sq_depth=%u)\n", chan->sq.sqid, sq_depth);

    /* reset urpc sq head/tail */
    if (chan->ts_inst->ops.sqcq_ctrl != NULL) {
        (void)chan->ts_inst->ops.sqcq_ctrl(inst, &chan->types, chan->sq.sqid, CTRL_CMD_SQ_RESET, 0);
    }
    trs_chan_put(chan);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_sq_switch_stream_chan_update);

void trs_all_chan_cq_work_cancel(struct trs_chan_ts_inst *ts_inst)
{
    struct trs_chan *chan = NULL;
    u32 chan_id;

    for (chan_id = 0; chan_id < MAX_CHAN_NUM; chan_id++) {
        chan = trs_chan_get(&ts_inst->inst, chan_id);
        if (chan != NULL) {
            ka_base_atomic_set(&chan->chan_status, 0);
            (void)ka_task_cancel_work_sync(&chan->work);
            trs_debug("Cancel work success. (devid=%u; tsid=%u; chan_id=%d)\n", chan->inst.devid, chan->inst.tsid,
                      chan->id);
            trs_chan_put(chan);
        }
    }
}

int hal_kernel_trs_chan_create(struct trs_id_inst *inst, struct trs_chan_para *para, int *chan_id)
{
    struct trs_chan_ts_inst *ts_inst = NULL;
    int ret;
    if ((para == NULL) || (chan_id == NULL)) {
        trs_err("Invalid para. (para=%pK; chan_id=%pK)\n", para, chan_id);
        return -EINVAL;
    }

    ret = trs_id_inst_check(inst);
    if (ret != 0) {
        return ret;
    }

    ts_inst = trs_chan_ts_inst_get(inst);
    if (ts_inst == NULL) {
#ifdef CFG_FEATURE_SUPPORT_LOWPOWER_MODE
        /* lowpower mode ts0 will not enable */
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
#else
        /* will be called by kworker */
        trs_debug("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
#endif
        return -EINVAL;
    }

    ka_task_mutex_lock(&ts_inst->mutex);
    ret = trs_chan_inst_create(ts_inst, para, chan_id);
    ka_task_mutex_unlock(&ts_inst->mutex);
    if (ret != 0) { /* if success, put 'ts inst' in destroy */
        trs_chan_ts_inst_put(ts_inst);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_trs_chan_create);

static bool trs_chan_is_need_show(struct trs_chan *chan)
{
    return (chan->stat.hw_err != 0);
}

static void trs_chan_detail_show(struct trs_id_inst *inst, struct trs_chan *chan)
{
    char buff[512]; /* 512 Bytes */

    trs_info("Chan stat show. (devid=%u; tsid=%u; id=%u)\n", inst->devid, inst->tsid, chan->id);
    if (trs_chan_to_string(inst, chan->id, buff, 512) > 0) { /* 512 Bytes */
        trs_info("%s\n", buff);
    }
}

void hal_kernel_trs_chan_destroy_ex(struct trs_id_inst *inst, u32 flag, int chan_id)
{
    struct trs_chan_ts_inst *ts_inst = NULL;
    struct trs_chan *chan = NULL;

    if (((flag & ~((0x1 << CHAN_FLAG_RESERVED_SQ_ID_BIT) | (0x1 << CHAN_FLAG_RESERVED_CQ_ID_BIT))) != 0)) {
        trs_err("Invalid param. (chan_flag=0x%x)\n", flag);
        return;
    }

    ts_inst = trs_chan_ts_inst_get(inst);
    if (ts_inst == NULL) {
        return;
    }

    ka_task_mutex_lock(&ts_inst->mutex);
    chan = trs_chan_get(inst, (u32)chan_id);
    if (chan == NULL) {
        ka_task_mutex_unlock(&ts_inst->mutex);
        trs_chan_ts_inst_put(ts_inst);
        trs_warn("Not create. (chan_id=%d)\n", chan_id);
        return;
    }

    if (trs_chan_is_need_show(chan)) {
        trs_chan_detail_show(inst, chan);
    }

    chan->flag |= flag;

    trs_chan_id_free(chan);
    trs_chan_put(chan);

    ka_task_mutex_unlock(&ts_inst->mutex);
    trs_chan_ts_inst_put(ts_inst);

    trs_chan_inst_destroy(chan);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_trs_chan_destroy_ex);

void hal_kernel_trs_chan_destroy(struct trs_id_inst *inst, int chan_id)
{
    return hal_kernel_trs_chan_destroy_ex(inst, 0, chan_id);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_trs_chan_destroy);

int trs_chan_get_sq_info(struct trs_id_inst *inst, int chan_id, struct trs_chan_sq_info *info)
{
    struct trs_chan *chan = NULL;

    if (info == NULL) {
        trs_err("Null ptr. (chan_id=%d)\n", chan_id);
        return -EINVAL;
    }

    chan = trs_chan_get(inst, chan_id);
    if (chan == NULL) {
        trs_err("Invalid para. (devid=%u; chan_id=%d)\n", inst->devid, chan_id);
        return -EINVAL;
    }

    if (!trs_chan_has_sq(chan)) {
        trs_chan_put(chan);
        trs_err("Chan does not have sq\n");
        return -EINVAL;
    }

    _trs_chan_get_sq_info(chan, info);
    trs_chan_put(chan);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_get_sq_info);

bool trs_chan_get_virt_flag_test(struct trs_id_inst *inst)
{
    struct trs_chan_ts_inst *ts_inst = NULL;
    bool flag = false;

    ts_inst = trs_chan_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return false;
    }

    flag = ts_inst->vaddr_enable;
    trs_chan_ts_inst_put(ts_inst);
    return flag;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_get_virt_flag_test);

int trs_chan_get_chan_id(struct trs_id_inst *inst, int res_type, u32 res_id, int *chan_id)
{
    struct trs_chan_ts_inst *ts_inst = NULL;

    if ((chan_id == NULL) || ((res_type != TRS_HW_SQ) && (res_type != TRS_HW_CQ) && (res_type != TRS_MAINT_SQ) &&
                              (res_type != TRS_MAINT_CQ))) {
        trs_err("Null ptr. (res_type=%d; res_id=%u)\n", res_type, res_id);
        return -EINVAL;
    }

    ts_inst = trs_chan_ts_inst_get(inst);
    if (ts_inst == NULL) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -EINVAL;
    }

    if (res_type == TRS_HW_SQ) {
        *chan_id = trs_chan_sq_to_chan_id(ts_inst, res_id);
    } else if (res_type == TRS_HW_CQ) {
        *chan_id = trs_chan_cq_to_chan_id(ts_inst, res_id);
    } else if (res_type == TRS_MAINT_SQ) {
        *chan_id = trs_chan_maint_sq_to_chan_id(ts_inst, res_id);
    } else {
        *chan_id = trs_chan_maint_cq_to_chan_id(ts_inst, res_id);
    }
    if (*chan_id == -1) {
        trs_err("Invalid para. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        trs_chan_ts_inst_put(ts_inst);
        return -EINVAL;
    }
    trs_chan_ts_inst_put(ts_inst);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_get_chan_id);

int trs_chan_get_cq_info(struct trs_id_inst *inst, int chan_id, struct trs_chan_cq_info *info)
{
    struct trs_chan *chan = NULL;

    if (info == NULL) {
        trs_err("Null ptr. (chan_id=%d)\n", chan_id);
        return -EINVAL;
    }

    chan = trs_chan_get(inst, chan_id);
    if (chan == NULL) {
        trs_err("Invalid para. (chan_id=%d)\n", chan_id);
        return -EINVAL;
    }

    if (!trs_chan_has_cq(chan)) {
        trs_chan_put(chan);
        trs_err("Chan does not have cq\n");
        return -EINVAL;
    }

    _trs_chan_get_cq_info(chan, info);
    trs_chan_put(chan);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_get_cq_info);
