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
#ifndef EMU_ST
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_task_pub.h"
#include "ka_barrier_pub.h"
#include "ka_errno_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"

#include "comm_kernel_interface.h"
#include "soc_adapt.h"
#include "trs_pub_def.h"
#include "trs_host_msg.h"
#include "trs_chan.h"
#include "trs_msg.h"
#include "trs_chan_irq.h"
#include "trs_ub_init_common.h"
#include "trs_ub_info.h"
#include "trs_sia_adapt_auto_init.h"

#include "securec.h"

#include "uba/ubcore_adapt.h"

#define TRS_UB_JETTY_BIND_WORK_PERIOD 10U /* unit ms */

#define TRS_UB_CQ_RECV_BUFFER_SIZE 32U
#define TRS_UB_CQ_RECV_BUFFER_NUM 32768U
#define TRS_UB_CQ_SEG_SIZE (TRS_UB_CQ_RECV_BUFFER_SIZE * TRS_UB_CQ_RECV_BUFFER_NUM)

#define TRS_UB_CQ_JFC_DEPTH TRS_UB_CQ_RECV_BUFFER_NUM
#define TRS_UB_SQ_JFC_DEPTH 32768U

#define TRS_UB_CQ_JFR_DEPTH TRS_UB_CQ_RECV_BUFFER_NUM // ub limited
#define TRS_UB_SQ_JFR_DEPTH 32768U                    // ub limited
#define TRS_UB_JFC_CTX_SIZE 128U

#define TRS_UB_CHAN_INFO_UPDATE_ADD 1U
#define TRS_UB_CHAN_INFO_UPDATE_DEL 0U

#define TRS_UB_OP_CMD_GET_CQ_HEAD 0U
#define TRS_UB_OP_CMD_SET_CQ_HEAD 1U
#define TRS_UB_OP_CMD_GET_CQ_TAIL 2U
#define TRS_UB_OP_CMD_FILL_CQ_CQE 3U
#define TRS_UB_OP_CMD_CQ_RESET 4U
#define TRS_UB_OP_CMD_CQ_PAUSE 5U
#define TRS_UB_OP_CMD_CQ_RESUME 6U

struct trs_ub_reg_cfg_para {
    void *src;
    size_t size;
    u64 src_offset;
    u64 dst_offset;
};

uba_ubcore_device *trs_ub_get_ubcore_dev(u32 dev_id)
{
    ka_device_t *uda_dev;

    uda_dev = uda_get_device(dev_id);
    if (uda_dev == NULL) {
        trs_err("Failed to get ub dev. (devid=%u)\n", dev_id);
        return NULL;
    }

    return ka_container_of(uda_dev, uba_ubcore_device, dev);
}

int trs_ub_get_sq_tail_paddr(struct trs_id_inst *inst, u32 sq_id, u64 *paddr)
{
    struct trs_ub_dev *ub_dev = NULL;

    ub_dev = trs_get_ub_dev(inst->devid);
    if (ub_dev == NULL) {
        trs_err("Failed to get ub dev. (devid=%u)\n", inst->devid);
        return -ENODEV;
    }

    *paddr = (u64)ka_mm_page_to_phys(ub_dev->sq_seg.seg_pages) + sq_id * TRS_UB_SQ_BUFFER_SIZE;
    trs_put_ub_dev(ub_dev);

    return 0;
}

int trs_ub_query_reset_sq_tail_paddr(struct trs_id_inst *inst, u32 sq_id, u64 *paddr)
{
    int ret = trs_ub_get_sq_tail_paddr(inst, sq_id, paddr);
    if (ret == 0) {
        *((u16 *)(ka_mm_phys_to_virt(*paddr))) = 0;
    }
    return ret;
}

int trs_ub_get_sq_head_paddr(struct trs_id_inst *inst, u32 sq_id, u64 *paddr)
{
    struct trs_ub_dev *ub_dev = NULL;

    ub_dev = trs_get_ub_dev(inst->devid);
    if (ub_dev == NULL) {
        trs_err("Failed to get uda dev. (devid=%u)\n", inst->devid);
        return -ENODEV;
    }

    *paddr = (u64)ka_mm_page_to_phys(ub_dev->sq_seg.seg_pages) + sq_id * TRS_UB_SQ_BUFFER_SIZE +
             TRS_UB_SQ_TAIL_BUFFER_SIZE;
    trs_put_ub_dev(ub_dev);

    return 0;
}

int trs_ub_query_reset_sq_head_paddr(struct trs_id_inst *inst, u32 sq_id, u64 *paddr)
{
    int ret = trs_ub_get_sq_head_paddr(inst, sq_id, paddr);
    if (ret == 0) {
        *((u16 *)(ka_mm_phys_to_virt(*paddr))) = 0;
    }
    return ret;
}

int trs_ub_get_sq_tail(struct trs_id_inst *inst, u32 sq_id, u32 *tail)
{
    u64 paddr;
    int ret;

    ret = trs_ub_get_sq_tail_paddr(inst, sq_id, &paddr);
    if (ret != 0) {
        return ret;
    }

    *tail = *((u16 *)(ka_mm_phys_to_virt(paddr)));
    return 0;
}

int trs_ub_get_sq_head(struct trs_id_inst *inst, u32 sq_id, u32 *head)
{
    u64 paddr;
    int ret;

    ret = trs_ub_get_sq_head_paddr(inst, sq_id, &paddr);
    if (ret != 0) {
        return ret;
    }

    *head = *((u16 *)(ka_mm_phys_to_virt(paddr)));
    return 0;
}

#define TRS_UB_CQE_PHASE_MASK 0xFEU
static int _trs_ub_cq_ctx_op_fill_cqe(struct trs_ub_cq_ctx *cq_ctx, void *cqe)
{
    int ret = 0;

    /* update phase value */
    *((u8 *)cqe) &= TRS_UB_CQE_PHASE_MASK;
    *((u8 *)cqe) |= ((cq_ctx->loop + 1U) & 0x1U);

    if (((cq_ctx->cq_tail + 1) % cq_ctx->cq_depth) == cq_ctx->cq_head) {
        trs_err("Cq full. (head=%u; tail=%u; depth=%u)\n", cq_ctx->cq_head, cq_ctx->cq_tail, cq_ctx->cq_depth);
        return -EINVAL;
    }

    ret = memcpy_s((u8 *)cq_ctx->cq_addr + cq_ctx->cqe_size * cq_ctx->cq_tail, cq_ctx->cqe_size, cqe, cq_ctx->cqe_size);
    if (ret != 0) {
        trs_err("Memcpy cqe failed.\n");
        return -EINVAL;
    }

    ka_wmb();
    cq_ctx->cq_tail = (cq_ctx->cq_tail + 1) % cq_ctx->cq_depth;
    if (cq_ctx->cq_tail == 0) {
        cq_ctx->loop++;
    }

    return 0;
}

static int _trs_ub_cq_ctx_op(struct trs_id_inst *inst, u32 cq_id, u32 *data, u32 cq_ctx_op_cmd)
{
    struct trs_ub_cq_ctx *cq_ctx = NULL;
    struct trs_ub_dev *ub_dev;
    int ret = 0;

    ub_dev = trs_get_ub_dev(inst->devid);
    if (ub_dev == NULL) {
        trs_err("Failed to get uda dev. (devid=%u)\n", inst->devid);
        return -ENODEV;
    }

    if (cq_id >= TRS_UB_HOST_CQ_MAX) {
        trs_err("The cqid exceeds the range. (cq_id=%u; max=%d)\n", cq_id, TRS_UB_HOST_CQ_MAX);
        trs_put_ub_dev(ub_dev);
        return -EINVAL;
    }

    ka_task_write_lock_bh(&ub_dev->rw_lock);
    cq_ctx = &ub_dev->cq_ctx[cq_id];
    if (cq_ctx_op_cmd == TRS_UB_OP_CMD_CQ_RESET) {
        cq_ctx->cq_head = 0;
        cq_ctx->cq_tail = 0;
        cq_ctx->loop = 0;
        goto unlock_and_put;
    }

    if (cq_ctx_op_cmd == TRS_UB_OP_CMD_CQ_PAUSE) {
        cq_ctx->is_valid = false;
        goto unlock_and_put;
    }
    if (cq_ctx_op_cmd == TRS_UB_OP_CMD_CQ_RESUME) {
        cq_ctx->is_valid = true;
        goto unlock_and_put;
    }

    if (cq_ctx->is_valid == false) {
        ret = -EINVAL;
        goto unlock_and_put;
    }

    if (cq_ctx_op_cmd == TRS_UB_OP_CMD_GET_CQ_HEAD) {
        *data = cq_ctx->cq_head;
    } else if (cq_ctx_op_cmd == TRS_UB_OP_CMD_GET_CQ_TAIL) {
        *data = cq_ctx->cq_tail;
    } else if (cq_ctx_op_cmd == TRS_UB_OP_CMD_SET_CQ_HEAD) {
        cq_ctx->cq_head = *data;
    } else if (cq_ctx_op_cmd == TRS_UB_OP_CMD_FILL_CQ_CQE) {
        ret = _trs_ub_cq_ctx_op_fill_cqe(cq_ctx, data);
    } else {
        ret = -EINVAL;
    }

unlock_and_put:
    ka_task_write_unlock_bh(&ub_dev->rw_lock);
    trs_put_ub_dev(ub_dev);
    return ret;
}

static int trs_ub_cq_ctx_op(struct trs_id_inst *inst, u32 cq_id, u32 *data, u32 cq_ctx_op_cmd)
{
    int ret = 0;

    ret = _trs_ub_cq_ctx_op(inst, cq_id, data, cq_ctx_op_cmd);

    return ret;
}

int trs_ub_get_cq_head(struct trs_id_inst *inst, u32 cq_id, u32 *head)
{
    return trs_ub_cq_ctx_op(inst, cq_id, head, TRS_UB_OP_CMD_GET_CQ_HEAD);
}

int trs_ub_get_cq_tail(struct trs_id_inst *inst, u32 cq_id, u32 *tail)
{
    return trs_ub_cq_ctx_op(inst, cq_id, tail, TRS_UB_OP_CMD_GET_CQ_TAIL);
}

int trs_ub_set_cq_head(struct trs_id_inst *inst, u32 cq_id, u32 head)
{
    u32 cq_head = head;
    return _trs_ub_cq_ctx_op(inst, cq_id, &cq_head, TRS_UB_OP_CMD_SET_CQ_HEAD);
}

int trs_ub_cq_reset(struct trs_id_inst *inst, u32 cq_id)
{
    u32 value = 0;
    return _trs_ub_cq_ctx_op(inst, cq_id, &value, TRS_UB_OP_CMD_CQ_RESET);
}

int trs_ub_sq_reset(struct trs_id_inst *inst, u32 sq_id)
{
    u64 head_paddr, tail_paddr;
    int ret;

    ret = trs_ub_get_sq_head_paddr(inst, sq_id, &head_paddr);
    if (ret != 0) {
        trs_err("Failed to reset sq head. (devid=%u; sqid=%u; ret=%d)\n", inst->devid, sq_id, ret);
        return ret;
    }
    *((u16 *)(ka_mm_phys_to_virt(head_paddr))) = 0;
    ret = trs_ub_get_sq_tail_paddr(inst, sq_id, &tail_paddr);
    if (ret != 0) {
        trs_err("Failed to reset sq tail. (devid=%u; sqid=%u; ret=%d)\n", inst->devid, sq_id, ret);
        return ret;
    }
    *((u16 *)(ka_mm_phys_to_virt(tail_paddr))) = 0;
    return 0;
}

int trs_ub_cq_pause(struct trs_id_inst *inst, u32 cq_id)
{
    u32 value = 0;
    return _trs_ub_cq_ctx_op(inst, cq_id, &value, TRS_UB_OP_CMD_CQ_PAUSE);
}

int trs_ub_cq_resume(struct trs_id_inst *inst, u32 cq_id)
{
    u32 value = 0;
    return _trs_ub_cq_ctx_op(inst, cq_id, &value, TRS_UB_OP_CMD_CQ_RESUME);
}

static int trs_ub_fill_cqe(struct trs_id_inst *inst, u32 cqid, void *cqe)
{
    return _trs_ub_cq_ctx_op(inst, cqid, cqe, TRS_UB_OP_CMD_FILL_CQ_CQE);
}

int trs_ub_sqcq_info_update(struct trs_id_inst *inst, struct trs_chan_info *chan_info)
{
    struct trs_ub_cq_ctx *cq_ctx = NULL;
    struct trs_ub_dev *ub_dev;

    ub_dev = trs_get_ub_dev(inst->devid);
    if (ub_dev == NULL) {
        trs_err("Failed to get uda dev. (devid=%u)\n", inst->devid);
        return -ENODEV;
    }

    if (chan_info->cq_info.cqid >= TRS_UB_HOST_CQ_MAX) {
        trs_err("The cqid exceeds the range. (cq_id=%u; max=%d)\n", chan_info->cq_info.cqid, TRS_UB_HOST_CQ_MAX);
        trs_put_ub_dev(ub_dev);
        return -EINVAL;
    }

    ka_task_write_lock_bh(&ub_dev->rw_lock);
    cq_ctx = &ub_dev->cq_ctx[chan_info->cq_info.cqid];
    if (chan_info->op == TRS_UB_CHAN_INFO_UPDATE_ADD) {
        cq_ctx->cq_addr = ka_mm_phys_to_virt(chan_info->cq_info.cq_phy_addr);
        cq_ctx->cqe_size = chan_info->cq_info.cq_para.cqe_size;
        cq_ctx->cq_depth = chan_info->cq_info.cq_para.cq_depth;
        cq_ctx->cq_head = 0;
        cq_ctx->cq_tail = 0;
        cq_ctx->loop = 0;
        cq_ctx->is_valid = true;
    } else {
        cq_ctx->is_valid = false;
        cq_ctx->cq_addr = NULL;
    }
    ka_task_write_unlock_bh(&ub_dev->rw_lock);
    trs_put_ub_dev(ub_dev);

    return 0;
}

int trs_ub_sq_jetty_init(struct trs_ub_dev *ub_dev, u32 vfid)
{
    uba_ubcore_jfc_cfg jfc_cfg = {0};
    uba_ubcore_jfr_cfg jfr_cfg = {0};
    uba_ubcore_jfc *jfc;
    uba_ubcore_jfr *jfr;

    jfc_cfg.depth = TRS_UB_SQ_JFC_DEPTH;
    jfc = uba_ubcore_create_jfc(UBA_MODULE_TYPE_TRS, ub_dev->ubc_dev, &jfc_cfg, NULL, NULL, NULL);
    if (KA_IS_ERR_OR_NULL(jfc)) {
        trs_err("Create sq head recv jfc failed.\n");
        return -ENOMEM;
    }

    jfr_cfg.depth = TRS_UB_SQ_JFR_DEPTH;
    jfr_cfg.trans_mode = UBA_UBCORE_TP_RM;
    jfr_cfg.min_rnr_timer = (u8)TRS_UB_JFR_RNR_TIME;
    jfr_cfg.max_sge = TRS_UB_JFR_MAX_SGE;
    jfr_cfg.jfc = jfc;
    jfr_cfg.flag.bs.token_policy = UBA_UBCORE_TOKEN_PLAIN_TEXT;
    jfr_cfg.token_value.token = ub_dev->token_val;
    jfr_cfg.eid_index = ub_dev->eid_info.eid_index;
    jfr = uba_ubcore_create_jfr(UBA_MODULE_TYPE_TRS, ub_dev->ubc_dev, &jfr_cfg, NULL, NULL);
    if (KA_IS_ERR_OR_NULL(jfr)) {
        trs_err("Create sq head jfr failed. (devid=%u)\n", ub_dev->devid);
        goto destroy_jfc;
    }
    trs_info("Sq head jfr. (jfr_id=%u; jfc_id=%u)\n", jfr->jfr_id.id, jfc->id);

    ub_dev->jetty_info[vfid].sq_jfc = jfc;
    ub_dev->jetty_info[vfid].sq_jfr = jfr;
    return 0;

destroy_jfc:
    uba_ubcore_delete_jfc(UBA_MODULE_TYPE_TRS, jfc);
    return -ENOMEM;
}

void trs_ub_sq_jetty_uninit(struct trs_ub_dev *ub_dev, u32 vfid)
{
    uba_ubcore_delete_jfr(UBA_MODULE_TYPE_TRS, ub_dev->jetty_info[vfid].sq_jfr);
    uba_ubcore_delete_jfc(UBA_MODULE_TYPE_TRS, ub_dev->jetty_info[vfid].sq_jfc);
    ub_dev->jetty_info[vfid].sq_jfc = NULL;
    ub_dev->jetty_info[vfid].sq_jfr = NULL;
}

static int trs_ub_import_jfr_segment(uba_ubcore_jfr *jfr, uba_ubcore_target_seg *target_seg, u32 index, u32 len)
{
    uba_ubcore_jfr_wr *bad_wr = NULL;
    uba_ubcore_jfr_wr wr;
    uba_ubcore_sge sge;
    int ret;

    wr.src.sge = &sge;
    wr.src.sge->tseg = target_seg;
    wr.src.sge->addr = target_seg->seg.ubva.va + index * len;
    wr.src.sge->len = len;
    wr.src.num_sge = TRS_UB_JFR_MAX_SGE;
    wr.user_ctx = index;
    wr.next = NULL;

    ret = uba_ubcore_post_jfr_wr(UBA_MODULE_TYPE_TRS, jfr, &wr, &bad_wr);
    if (ret != 0) {
        trs_err("uba_ubcore_post_jfr_wr failed. (seg_id=%d; len=%u)\n", index, len);
        return -ENOMEM;
    }

    return 0;
}

static int trs_ub_import_jfr_all_segment(uba_ubcore_jfr *jfr, uba_ubcore_target_seg *target_seg, u32 num, u32 len)
{
    int ret;
    u32 i;

    for (i = 0; i < num; i++) {
        ret = trs_ub_import_jfr_segment(jfr, target_seg, (u32)i, len);
        if (ret != 0) {
            trs_err("Import jfr all segment failed. (index=%d; len=%u)\n", i, len);
            return ret;
        }
    }

    return 0;
}

static int trs_ub_trigger_cq_update_irq(struct trs_id_inst *inst, u32 cqid[], u32 cq_num)
{
    struct trs_ub_dev *ub_dev;
    int ret;

    ub_dev = trs_get_ub_dev(inst->devid);
    if (ub_dev == NULL) {
        trs_err("Failed to get uda dev. (devid=%u)\n", inst->devid);
        return -ENODEV;
    }

    if ((ub_dev->cq_irq_handler == NULL) || (ub_dev->cq_irq_para == NULL)) {
        trs_err("No irq handler registered. (devid=%u)\n", inst->devid);
        trs_put_ub_dev(ub_dev);
        return -ENODEV;
    }

    if (cq_num == 0) {
        trs_put_ub_dev(ub_dev);
        return 0;
    }

    ret = ub_dev->cq_irq_handler(0, 0, ub_dev->cq_irq_para, cqid, cq_num);
    trs_put_ub_dev(ub_dev);
    return ret;
}

static void trs_ub_cq_dispatch_task(unsigned long data)
{
    struct trs_jetty_info *jetty_info = (struct trs_jetty_info *)((uintptr_t)data);
    struct trs_id_inst inst = {jetty_info->devid, 0};
    int cr_num;
    int ret, i;

    trs_debug("In jfce tasklet handle. (devid=%u; jfr_idx=%u)\n", inst.devid, jetty_info->cq_jfr->jfr_id.id);

    cr_num = uba_ubcore_poll_jfc(UBA_MODULE_TYPE_TRS, jetty_info->cq_jfc, TRS_UB_CQ_JFC_CR_EXPERT_NUM, jetty_info->cr);
    if (cr_num <= 0) {
        trs_err("Polled cqe failed. (devid=%u; cr_num=%d)\n", inst.devid, cr_num);
        goto rearm_jfc;
    }

    jetty_info->cq_num = 0;
    (void)memset_s((void *)jetty_info->cq_id_bucket, sizeof(jetty_info->cq_id_bucket), 0,
                   sizeof(jetty_info->cq_id_bucket));
    for (i = 0; i < cr_num; i++) {
        u32 cq_id = (u32)(jetty_info->cr[i].imm_data);
        u32 buff_index = jetty_info->cr[i].user_ctx;
        void *cqe_addr = (void *)(jetty_info->cq_seg.target_seg->seg.ubva.va + buff_index * TRS_UB_CQ_RECV_BUFFER_SIZE);

        if (buff_index >= TRS_UB_CQ_RECV_BUFFER_NUM) {
            trs_err("The buffer index exceeds the range. (buff_index=%u; max=%u)\n", buff_index,
                    TRS_UB_CQ_RECV_BUFFER_NUM);
            continue;
        }

        if (cq_id >= TRS_UB_HOST_CQ_MAX) {
            trs_err("The cqid exceeds the range. (cq_id=%u; max=%d)\n", cq_id, TRS_UB_HOST_CQ_MAX);
            goto import_jfr_seg;
        }

        ret = trs_ub_fill_cqe(&inst, cq_id, cqe_addr);
        if (ret != 0) {
            trs_debug("Cqe fill failed. (devid=%u; index=%d)\n", inst.devid, i);
            goto import_jfr_seg;
        }

        if (jetty_info->cq_id_bucket[cq_id] == 0) {
            jetty_info->cq_id[jetty_info->cq_num] = cq_id;
            jetty_info->cq_num++;
            jetty_info->cq_id_bucket[cq_id] = 1;
        }
        trs_debug("Cq info. (devid=%u; cq_id=%u; cq_num=%u; buff_index=%u; jfc_id=%u)\n", inst.devid, cq_id,
                  jetty_info->cq_num, buff_index, jetty_info->cq_jfc->id);

    import_jfr_seg:
        ret = trs_ub_import_jfr_segment(jetty_info->cq_jfr, jetty_info->cq_seg.target_seg, buff_index,
                                        TRS_UB_CQ_RECV_BUFFER_SIZE);
        if (ret != 0) {
            trs_err("Reimport jfr segment failed. (index=%d; len=%u)\n", buff_index, TRS_UB_CQ_RECV_BUFFER_SIZE);
        }
    }

    ret = trs_ub_trigger_cq_update_irq(&inst, jetty_info->cq_id, jetty_info->cq_num);
    if (ret != 0) {
        trs_debug("Dispatch cqe warn. (devid=%u; ret=%d)\n", jetty_info->devid, ret);
    }

rearm_jfc:
    ret = uba_ubcore_rearm_jfc(UBA_MODULE_TYPE_TRS, jetty_info->cq_jfc, false);
    if (ret != 0) {
        trs_err("Rearm jfc fail. (devid=%u; ret=%d)\n", jetty_info->devid, ret);
    }
}

int trs_ub_request_cq_update_irq(struct trs_id_inst *inst, int irq_type, int irq_index, struct trs_chan_irq_attr *attr)
{
    struct trs_ub_dev *ub_dev;
    int i;

    ub_dev = trs_get_ub_dev(inst->devid);
    if (ub_dev == NULL) {
        trs_err("Failed to get uda dev. (devid=%u)\n", inst->devid);
        return -ENODEV;
    }

    ub_dev->cq_irq_handler = attr->handler;
    ub_dev->cq_irq_para = attr->para;

    for (i = 0; i < TRS_VF_MAX_NUM; i++) {
        ka_system_tasklet_init(&ub_dev->jetty_info[i].cq_dispatch_task, trs_ub_cq_dispatch_task,
                               (uintptr_t)(&ub_dev->jetty_info[i]));
    }
    trs_put_ub_dev(ub_dev);
    return 0;
}

int trs_ub_free_cq_update_irq(struct trs_id_inst *inst, int irq_type, int irq_index, void *para)
{
    struct trs_ub_dev *ub_dev;
    int i;

    ub_dev = trs_get_ub_dev(inst->devid);
    if (ub_dev == NULL) {
        trs_err("Failed to get uda dev. (devid=%u)\n", inst->devid);
        return -ENODEV;
    }

    for (i = 0; i < TRS_VF_MAX_NUM; i++) {
        ka_system_tasklet_kill(&ub_dev->jetty_info[i].cq_dispatch_task);
    }
    ub_dev->cq_irq_handler = NULL;
    ub_dev->cq_irq_para = NULL;
    trs_put_ub_dev(ub_dev);
    return 0;
}

static void trs_ub_cq_jfce_recv_handle(uba_ubcore_jfc *jfc)
{
    struct trs_jetty_info *jetty_info = (struct trs_jetty_info *)jfc->jfc_cfg.jfc_context;

    ka_system_tasklet_schedule(&jetty_info->cq_dispatch_task);
}

static int trs_ub_init_segment(struct trs_ub_dev *ub_dev, size_t seg_size, struct trs_ub_seg *seg, int local_flag)
{
    uba_ubcore_reg_seg_flag flag = {0};
    uba_ubcore_seg_cfg seg_cfg = {0};

    seg->seg_pages = ka_mm_alloc_pages(KA_GFP_KERNEL, ka_mm_get_order(seg_size));
    if (seg->seg_pages == NULL) {
        trs_err("Alloc pages for segment failed.\n");
        return -ENOMEM;
    }

    flag.bs.token_policy = UBA_UBCORE_TOKEN_PLAIN_TEXT;
    flag.bs.access = (local_flag == 1) ? UBA_UBCORE_ACCESS_LOCAL_ONLY :
                                         (UBA_UBCORE_ACCESS_READ | UBA_UBCORE_ACCESS_WRITE | UBA_UBCORE_ACCESS_ATOMIC);
    flag.bs.non_pin = TRS_UB_DEFAULT_NON_PIN;
    seg_cfg.va = (u64)ka_mm_page_to_virt(seg->seg_pages);
    seg_cfg.len = seg_size;
    seg_cfg.flag = flag;
    seg_cfg.token_value.token = ub_dev->token_val;

    seg->target_seg = uba_ubcore_register_seg(UBA_MODULE_TYPE_TRS, ub_dev->ubc_dev, &seg_cfg, NULL);
    if (KA_IS_ERR_OR_NULL(seg->target_seg)) {
        trs_err("uba_ubcore_register_seg fail.\n");
        __ka_mm_free_pages(seg->seg_pages, ka_mm_get_order(seg_size));
        seg->seg_pages = NULL;
        return -EFAULT;
    }

    seg->seg_size = seg_size;
    return 0;
}

static void trs_ub_uninit_segment(struct trs_ub_seg *seg)
{
    (void)uba_ubcore_unregister_seg(UBA_MODULE_TYPE_TRS, seg->target_seg);
    seg->target_seg = NULL;

    if (seg->seg_pages != NULL) {
        __ka_mm_free_pages(seg->seg_pages, ka_mm_get_order(seg->seg_size));
        seg->seg_size = 0;
        seg->seg_pages = NULL;
    }
}

int trs_ub_cq_jetty_init(struct trs_ub_dev *ub_dev, u32 vfid)
{
    uba_ubcore_jfc_cfg jfc_cfg = {0}; // ceqn in jfce must set zero
    uba_ubcore_jfr_cfg jfr_cfg = {0};
    uba_ubcore_jfc *jfc;
    uba_ubcore_jfr *jfr;
    int ret;

    ret = trs_ub_init_segment(ub_dev, TRS_UB_CQ_SEG_SIZE, &(ub_dev->jetty_info[vfid].cq_seg), 1);
    if (ret != 0) {
        return ret;
    }

    jfc_cfg.depth = TRS_UB_CQ_JFC_DEPTH;
    jfc_cfg.jfc_context = (void *)&ub_dev->jetty_info[vfid];
    jfc = uba_ubcore_create_jfc(UBA_MODULE_TYPE_TRS, ub_dev->ubc_dev, &jfc_cfg, trs_ub_cq_jfce_recv_handle, NULL, NULL);
    if (KA_IS_ERR_OR_NULL(jfc)) {
        trs_err("Create cq recv jfc failed.\n");
        goto uninit_segment;
    }

    ret = uba_ubcore_rearm_jfc(UBA_MODULE_TYPE_TRS, jfc, false);
    if (ret != 0) {
        trs_err("Ubcore rearm recv jfc fail. (ret=%d; devid=%u)\n", ret, ub_dev->devid);
        goto destroy_jfc;
    }

    jfr_cfg.depth = TRS_UB_CQ_JFR_DEPTH;
    jfr_cfg.trans_mode = UBA_UBCORE_TP_RM;
    jfr_cfg.min_rnr_timer = (u8)TRS_UB_JFR_RNR_TIME;
    jfr_cfg.max_sge = TRS_UB_JFR_MAX_SGE;
    jfr_cfg.jfc = jfc;
    jfr_cfg.flag.bs.token_policy = UBA_UBCORE_TOKEN_PLAIN_TEXT;
    jfr_cfg.token_value.token = ub_dev->token_val;
    jfr_cfg.eid_index = ub_dev->eid_info.eid_index;
    jfr = uba_ubcore_create_jfr(UBA_MODULE_TYPE_TRS, ub_dev->ubc_dev, &jfr_cfg, NULL, NULL);
    if (KA_IS_ERR_OR_NULL(jfr)) {
        trs_err("Create cq jfr failed. (devid=%u)\n", ub_dev->devid);
        goto destroy_jfc;
    }

    ret = trs_ub_import_jfr_all_segment(jfr, ub_dev->jetty_info[vfid].cq_seg.target_seg, TRS_UB_CQ_RECV_BUFFER_NUM,
                                        TRS_UB_CQ_RECV_BUFFER_SIZE);
    if (ret != 0) {
        trs_err("Import cq jfr segment failed. (devid=%u)\n", ub_dev->devid);
        goto destroy_jfr;
    }
    trs_info("Cqe jfr. (jfr_id=%u; jfc_id=%u)\n", jfr->jfr_id.id, jfc->id);

    ub_dev->jetty_info[vfid].cq_jfc = jfc;
    ub_dev->jetty_info[vfid].cq_jfr = jfr;
    return 0;

destroy_jfr:
    uba_ubcore_delete_jfr(UBA_MODULE_TYPE_TRS, jfr);
destroy_jfc:
    uba_ubcore_delete_jfc(UBA_MODULE_TYPE_TRS, jfc);
uninit_segment:
    trs_ub_uninit_segment(&ub_dev->jetty_info[vfid].cq_seg);
    return -ENOMEM;
}

void trs_ub_cq_jetty_uninit(struct trs_ub_dev *ub_dev, u32 vfid)
{
    uba_ubcore_delete_jfr(UBA_MODULE_TYPE_TRS, ub_dev->jetty_info[vfid].cq_jfr);
    uba_ubcore_delete_jfc(UBA_MODULE_TYPE_TRS, ub_dev->jetty_info[vfid].cq_jfc);
    trs_ub_uninit_segment(&ub_dev->jetty_info[vfid].cq_seg);
    ub_dev->jetty_info[vfid].cq_jfc = NULL;
    ub_dev->jetty_info[vfid].cq_jfr = NULL;
}

static int trs_ub_import_reg_segment(struct trs_ub_dev *ub_dev, struct trs_msg_jetty_info *jetty_info)
{
    uba_ubcore_target_seg_cfg seg_cfg = {0};

    seg_cfg.seg.attr.bs.token_policy = UBA_UBCORE_TOKEN_PLAIN_TEXT;
    seg_cfg.token_value.token = jetty_info->rmt_token_value;
    seg_cfg.seg = jetty_info->notify_seg;
    ub_dev->notify_tseg = uba_ubcore_import_seg(UBA_MODULE_TYPE_TRS, ub_dev->ubc_dev, &seg_cfg, NULL);
    if (KA_IS_ERR_OR_NULL(ub_dev->notify_tseg)) {
        trs_err("Failed to import notify segment. (devid=%u)\n", ub_dev->devid);
        return -EFAULT;
    }

    seg_cfg.seg = jetty_info->cnt_notify_seg;
    ub_dev->cnt_notify_tseg = uba_ubcore_import_seg(UBA_MODULE_TYPE_TRS, ub_dev->ubc_dev, &seg_cfg, NULL);
    if (KA_IS_ERR_OR_NULL(ub_dev->cnt_notify_tseg)) {
        (void)uba_ubcore_unimport_seg(UBA_MODULE_TYPE_TRS, ub_dev->notify_tseg);
        ub_dev->notify_tseg = NULL;
        trs_err("Failed to import cnt notify segment. (devid=%u)\n", ub_dev->devid);
        return -EFAULT;
    }

    seg_cfg.seg = jetty_info->sq_reg_seg;
    ub_dev->sq_reg_tseg = uba_ubcore_import_seg(UBA_MODULE_TYPE_TRS, ub_dev->ubc_dev, &seg_cfg, NULL);
    if (KA_IS_ERR_OR_NULL(ub_dev->sq_reg_tseg)) {
        (void)uba_ubcore_unimport_seg(UBA_MODULE_TYPE_TRS, ub_dev->cnt_notify_tseg);
        ub_dev->cnt_notify_tseg = NULL;
        (void)uba_ubcore_unimport_seg(UBA_MODULE_TYPE_TRS, ub_dev->notify_tseg);
        ub_dev->notify_tseg = NULL;
        trs_err("Failed to import sq reg segment. (devid=%u)\n", ub_dev->devid);
        return -EFAULT;
    }

    trs_info("Import segment success. (devid=%u; len=%llu)\n", ub_dev->devid, seg_cfg.seg.len);
    return 0;
}

static void trs_ub_unimport_reg_segment(struct trs_ub_dev *ub_dev)
{
    (void)uba_ubcore_unimport_seg(UBA_MODULE_TYPE_TRS, ub_dev->sq_reg_tseg);
    ub_dev->sq_reg_tseg = NULL;
    (void)uba_ubcore_unimport_seg(UBA_MODULE_TYPE_TRS, ub_dev->notify_tseg);
    ub_dev->notify_tseg = NULL;
    (void)uba_ubcore_unimport_seg(UBA_MODULE_TYPE_TRS, ub_dev->cnt_notify_tseg);
    ub_dev->cnt_notify_tseg = NULL;
}

static int trs_ub_stars_reg_op(struct trs_id_inst *inst, enum trs_id_type id_type, struct trs_ub_reg_cfg_para *para,
                               enum devdrv_urma_copy_dir dir)
{
    struct trs_ub_dev *ub_dev = NULL;
    struct devdrv_urma_copy local, peer;
    u8 *vaddr = NULL;
    int ret = 0;

    ub_dev = trs_get_ub_dev(inst->devid);
    if (ub_dev == NULL) {
        trs_err("Failed to get uda dev. (devid=%u)\n", inst->devid);
        return -ENODEV;
    }

    local.seg = ub_dev->reg_seg.target_seg;
    local.len = (u64)para->size;
    local.offset = para->src_offset;
    peer.len = (u64)para->size;
    peer.offset = para->dst_offset;

    if (id_type == TRS_NOTIFY) {
        peer.seg = ub_dev->notify_tseg;
    } else if (id_type == TRS_CNT_NOTIFY) {
        peer.seg = ub_dev->cnt_notify_tseg;
    } else if (id_type == TRS_HW_SQ) {
        peer.seg = ub_dev->sq_reg_tseg;
    } else {
        trs_put_ub_dev(ub_dev);
        return -ENOTSUPP;
    }

    vaddr = (u8 *)ka_mm_page_to_virt(ub_dev->reg_seg.seg_pages);
    vaddr += para->src_offset;

    if (dir == LOCAL_TO_PEER) {
        (void)memcpy_s(vaddr, TRS_UB_REG_SEG_SIZE - para->src_offset, para->src, para->size);
    }

    ret = devdrv_urma_copy(inst->devid, URMA_CHAN_TSDRV, dir, &local, &peer);
    if (ret != 0) {
        trs_err("Failed to copy with urma. (ret=%d; devid=%u; dir=%d; type=%d; len=%lu; src_offset=0x%llx; "
                "dst_offset=0x%llx)\n",
                ret, inst->devid, dir, id_type, para->size, para->src_offset, para->dst_offset);
        trs_put_ub_dev(ub_dev);
        return ret;
    }

    if (dir == PEER_TO_LOCAL) {
        (void)memcpy_s(para->src, para->size, vaddr, para->size);
    }

    trs_debug("Reg op success. (devid=%u; dir=%d; id_type=%d; len=%lu; src_offset=0x%llx; dst_offset=0x%llx)\n",
              inst->devid, dir, id_type, para->size, para->src_offset, para->dst_offset);
    trs_put_ub_dev(ub_dev);
    return 0;
}

int trs_ub_stars_soc_res_ctrl(struct trs_id_inst *inst, u32 type, u32 id, u32 cmd)
{
    struct trs_ub_reg_cfg_para para = {0};
    u32 reg_offset, val = 0;
    int ret;

    if ((cmd != TRS_RES_OP_RESET) && (cmd != TRS_RES_OP_CHECK_AND_RESET)) {
        return -ENOTSUPP;
    }

    if (type == TRS_NOTIFY) {
        para.src_offset = TRS_UB_REG_SEG_NOTIFY_OFFSET;
        ret = trs_soc_get_notify_size(inst, &para.size);
        ret |= trs_soc_get_notify_offset(inst, id, &reg_offset);
        if (ret != 0) {
            return -EFAULT;
        }
    } else if (type == TRS_CNT_NOTIFY) {
        para.src_offset = TRS_UB_REG_SEG_CNT_NOTIFY_OFFSET;
        ret = trs_soc_get_cnt_notify_size(inst, &para.size);
        ret |= trs_soc_get_cnt_notify_offset(inst, id, &reg_offset);
        if (ret != 0) {
            return -EFAULT;
        }
    } else {
        return -ENOTSUPP;
    }

    para.src = &val;
    para.dst_offset = reg_offset;
    ret = trs_ub_stars_reg_op(inst, type, &para, LOCAL_TO_PEER);
    if (ret != 0) {
        trs_err("Failed to cfg reg by urma. (ret=%d; devid=%u; type=%u; id=%u; len=%lu; dst_offset=0x%llx)\n", ret,
                inst->devid, type, id, para.size, para.dst_offset);
        return ret;
    }

    trs_debug("Reg cfg success. (devid=%u; type=%u; cmd=%u; id=%u; len=%lu; dst_offset=0x%llx)\n", inst->devid, type,
              cmd, id, para.size, para.dst_offset);
    return 0;
}

#define TRS_UB_REG_SQ_HEAD_TAIL_SIZE 8U
/* op: 0 for head, 1 for tail; dir: LOCAL_TO_PEER for set, PEER_TO_LOCAL for get */
int trs_ub_sq_head_tail_op(struct trs_id_inst *inst, u32 sq_id, u32 *value, int op, enum devdrv_urma_copy_dir dir)
{
    struct trs_ub_reg_cfg_para para = {0};
    u64 sq_reg_src_offset = 0;
    u32 reg_offset, cpy_value = 0;
    size_t stride = 0;
    int ret = 0;

    ret = trs_soc_get_sq_head_tail_reg_size(inst, &para.size);
    ret |= trs_soc_get_stars_sched_stride(inst, &stride);
    if (op == 0) {
        ret |= trs_soc_get_sq_head_reg_offset(inst, &reg_offset);
    } else {
        ret |= trs_soc_get_sq_tail_reg_offset(inst, &reg_offset);
    }
    if (ret != 0) {
        return -EFAULT;
    }

    sq_reg_src_offset = sq_id * TRS_UB_REG_SQ_HEAD_TAIL_SIZE + ((op == 0) ? 0 : sizeof(u32));
    if ((para.size != sizeof(u32)) ||
        ((sq_reg_src_offset + sizeof(u32)) > (TRS_UB_REG_SEG_SIZE - TRS_UB_REG_SEG_SQ_OFFSET))) {
        return -EFAULT;
    }

    if (dir == LOCAL_TO_PEER) {
        cpy_value = *value;
    }

    para.src = &cpy_value;
    para.src_offset = TRS_UB_REG_SEG_SQ_OFFSET + sq_reg_src_offset;
    para.dst_offset = (sq_id * stride + reg_offset);
    ret = trs_ub_stars_reg_op(inst, TRS_HW_SQ, &para, dir);
    if (ret != 0) {
        return ret;
    }

    if (dir == LOCAL_TO_PEER) {
        u64 paddr;
        if (op == 0) {
            ret = trs_ub_get_sq_head_paddr(inst, sq_id, &paddr);
        } else {
            ret = trs_ub_get_sq_tail_paddr(inst, sq_id, &paddr);
        }

        if (ret == 0) {
            *((u16 *)(ka_mm_phys_to_virt(paddr))) = *value;
        }
        return ret;
    }

    *value = cpy_value;
    return 0;
}

int trs_ub_set_sq_head(struct trs_id_inst *inst, u32 sq_id, u32 head)
{
    return trs_ub_sq_head_tail_op(inst, sq_id, &head, 0, LOCAL_TO_PEER);
}

int trs_ub_set_sq_tail(struct trs_id_inst *inst, u32 sq_id, u32 tail)
{
    return trs_ub_sq_head_tail_op(inst, sq_id, &tail, 1, LOCAL_TO_PEER);
}

int trs_ub_get_stars_sq_head(struct trs_id_inst *inst, u32 sq_id, u32 *head)
{
    return trs_ub_sq_head_tail_op(inst, sq_id, head, 0, PEER_TO_LOCAL);
}

int trs_ub_get_stars_sq_tail(struct trs_id_inst *inst, u32 sq_id, u32 *tail)
{
    return trs_ub_sq_head_tail_op(inst, sq_id, tail, 1, PEER_TO_LOCAL);
}

int trs_ub_bind_jetty(struct trs_ub_dev *ub_dev, u32 vfid)
{
    struct trs_msg_data msg = {0};
    struct trs_msg_jetty_info *jetty_info = (struct trs_msg_jetty_info *)msg.payload;
    u32 devid = ub_dev->devid;
    int ret;

    msg.header.devid = devid;
    msg.header.cmdtype = TRS_MSG_INIT_JETTY;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    jetty_info->cq_jfr_id = ub_dev->jetty_info[vfid].cq_jfr->jfr_id.id;
    jetty_info->sq_jfr_id = ub_dev->jetty_info[vfid].sq_jfr->jfr_id.id;
    jetty_info->eid_info = ub_dev->eid_info;
    jetty_info->token_value = ub_dev->token_val;
    (void)memcpy_s(&(jetty_info->sq_seg), sizeof(uba_ubcore_seg), &(ub_dev->sq_seg.target_seg->seg),
                   sizeof(uba_ubcore_seg));
    (void)memcpy_s(&(jetty_info->cq_seg), sizeof(uba_ubcore_seg), &(ub_dev->jetty_info[vfid].cq_seg.target_seg->seg),
                   sizeof(uba_ubcore_seg));
    jetty_info->vfid = vfid;

    ret = trs_host_msg_send(devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        trs_err("Fail send jetty bind msg. (devid=%u; vfid=%u; ret=%d; result=%d)\n", devid, vfid, ret,
                msg.header.result);
        return ret;
    }

    ret = trs_ub_import_reg_segment(ub_dev, jetty_info);
    if (ret != 0) {
        trs_err("Failed to import reg segment. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    ub_dev->die_id = jetty_info->die_id;
    ub_dev->func_id = jetty_info->func_id;

    trs_info("dieid=%u, vfid=%u, funcid=%u\n", jetty_info->die_id, jetty_info->vfid, jetty_info->func_id);

    return 0;
}

void trs_ub_unbind_jetty(struct trs_ub_dev *ub_dev, u32 vfid)
{
    struct trs_msg_data msg = {0};
    struct trs_msg_unbind_jetty *info = (struct trs_msg_unbind_jetty *)msg.payload;
    int ret;

    trs_ub_unimport_reg_segment(ub_dev);

    msg.header.devid = ub_dev->devid;
    msg.header.cmdtype = TRS_MSG_UNINIT_JETTY;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    info->vfid = vfid;

    ret = trs_host_msg_send(ub_dev->devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        trs_warn("Msg send result. (devid=%u; ret=%d; result=%d)\n", ub_dev->devid, ret, msg.header.result);
        return;
    }
}

static void trs_ub_mng_info_set(struct trs_ub_dev *ub_dev)
{
    struct trs_ub_info ub_info;

    ub_info.die_id = ub_dev->die_id;
    ub_info.func_id = ub_dev->func_id;

    trs_ub_info_set(ub_dev->devid, &ub_info);
}

int trs_ub_jetty_init(struct trs_ub_dev *ub_dev, u32 vfid)
{
    int ret;

    ret = trs_ub_create_jetty(ub_dev, vfid);
    if (ret != 0) {
        trs_err("Failed to create host jetty. (devid=%u; vfid=%u; ret=%d)\n", ub_dev->devid, vfid, ret);
        return ret;
    }

    ret = trs_ub_bind_jetty(ub_dev, vfid);
    if (ret != 0) {
        trs_ub_destroy_jetty(ub_dev, vfid);
        trs_err("Failed to bind host-device jetty. (devid=%u; vfid=%u; ret=%d)\n", ub_dev->devid, vfid, ret);
        return ret;
    }

    ub_dev->jetty_info[vfid].devid = ub_dev->devid;
    trs_ub_mng_info_set(ub_dev);

    trs_info("Create jetty success. (devid=%u; vfid=%u)\n", ub_dev->devid, vfid);
    return 0;
}

void trs_ub_jetty_uninit(struct trs_ub_dev *ub_dev, u32 vfid)
{
    trs_ub_unbind_jetty(ub_dev, vfid);
    trs_ub_destroy_jetty(ub_dev, vfid);
}

int trs_ub_dev_adapt_init(struct trs_ub_dev *ub_dev, u32 vfid)
{
    void *vaddr = NULL;
    int ret = 0;

    ret = trs_ub_init_segment(ub_dev, TRS_UB_SQ_SEG_SIZE, &(ub_dev->sq_seg), 0);
    if (ret != 0) {
        trs_err("Init sq head jfr segment failed. (devid=%u)\n", ub_dev->devid);
        return -ENOMEM;
    }

    ret = trs_ub_init_segment(ub_dev, TRS_UB_REG_SEG_SIZE, &(ub_dev->reg_seg), 0);
    if (ret != 0) {
        trs_ub_uninit_segment(&ub_dev->sq_seg);
        trs_err("Init sq head jfr segment failed. (devid=%u)\n", ub_dev->devid);
        return -ENOMEM;
    }
    vaddr = ka_mm_page_to_virt(ub_dev->reg_seg.seg_pages);
    (void)memset_s(vaddr, ub_dev->reg_seg.seg_size, 0, ub_dev->reg_seg.seg_size);

    ret = trs_ub_jetty_init(ub_dev, vfid);
    if (ret != 0) {
        trs_ub_uninit_segment(&ub_dev->reg_seg);
        trs_ub_uninit_segment(&ub_dev->sq_seg);
        return ret;
    }

    return 0;
}

void trs_ub_dev_adapt_uninit(struct trs_ub_dev *ub_dev, u32 vfid)
{
    trs_ub_jetty_uninit(ub_dev, vfid);
    trs_ub_uninit_segment(&ub_dev->reg_seg);
    trs_ub_uninit_segment(&ub_dev->sq_seg);
}

int trs_ub_dev_init_with_group(u32 devid, u32 vfid)
{
    struct trs_ub_dev *ub_dev = NULL;
    int ret;

    ub_dev = trs_get_ub_dev(devid);
    if (ub_dev == NULL) {
        trs_err("Vnpu not exist. (devid=%u; vfid=%u)\n", devid, vfid);
        return -ENODEV;
    }

    if (vfid == ub_dev->vfid) {
        trs_info("Use default jetty when first create group. (udevid=%u; vfid=%u)\n", devid, vfid);
        trs_put_ub_dev(ub_dev);
        return 0;
    }

    ret = trs_ub_jetty_init(ub_dev, vfid);
    if (ret != 0) {
        trs_put_ub_dev(ub_dev);
        return ret;
    }

    ub_dev->sec_vfid = vfid;
    trs_info("Create group success. (udevid=%u; vfid=%u)\n", devid, vfid);
    trs_put_ub_dev(ub_dev);
    return 0;
}

void trs_ub_dev_uninit_with_group(u32 devid, u32 vfid)
{
    struct trs_ub_dev *ub_dev = trs_get_ub_dev(devid);
    if (ub_dev != NULL) {
        if (vfid != ub_dev->vfid) {
            trs_ub_jetty_uninit(ub_dev, vfid);
            trs_info("Destroy group success. (udevid=%u; vfid=%u)\n", devid, vfid);
        }
        trs_put_ub_dev(ub_dev);
    }
}
#else
void trs_ub_host_stub(void)
{}
#endif
