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
#include "ka_memory_pub.h"

#include "securec.h"

#ifndef EMU_ST
#include "hw_vdavinci.h"
#endif
#include "vmng_kernel_interface.h"
#include "comm_kernel_interface.h"
#include "pbl/pbl_uda.h"
#include "trs_chan_near_ops_mem.h"
#include "trs_chan.h"
#include "trs_chan_update.h"

#include "trs_sec_eh_id.h"
#include "trs_sec_eh_sq.h"
#include "trs_mailbox_def.h"

#define TRS_SEC_EH_MAX_SQE_SIZE 128
#define TRS_SEC_EH_MAX_SQDEPTH_SIZE (64 * 1024)
#define TRS_SEC_EH_CQE_ALIGN_SIZE 4

void trs_sec_eh_sq_ctx_init(struct trs_sec_eh_ts_inst *sec_eh_cfg, struct trs_sec_eh_sq_ctx_info *ctx)
{
    ka_task_mutex_lock(&sec_eh_cfg->mutex);
    sec_eh_cfg->sq_ctx[ctx->sqid].sqe_size = ctx->sqesize;
    sec_eh_cfg->sq_ctx[ctx->sqid].sq_depth = ctx->sqdepth;
    sec_eh_cfg->sq_ctx[ctx->sqid].mem_type = ctx->mem_type;
    sec_eh_cfg->sq_ctx[ctx->sqid].d_addr = ctx->sq_addr;
    sec_eh_cfg->sq_ctx[ctx->sqid].sq_paddr = ctx->sq_paddr;
    sec_eh_cfg->sq_ctx[ctx->sqid].pid = ctx->pid;
    sec_eh_cfg->sq_ctx[ctx->sqid].sq_dev_vaddr = ctx->sq_dev_vaddr;
    sec_eh_cfg->sq_ctx[ctx->sqid].sq_create_cmd_type = ctx->sq_create_cmd_type;
    ka_task_mutex_unlock(&sec_eh_cfg->mutex);
}

void trs_sec_eh_sq_ctx_uninit(struct trs_sec_eh_ts_inst *sec_eh_cfg, u32 sqid)
{
    ka_task_mutex_lock(&sec_eh_cfg->mutex);
    sec_eh_cfg->sq_ctx[sqid].sqe_size = 0;
    sec_eh_cfg->sq_ctx[sqid].sq_depth = 0;
    sec_eh_cfg->sq_ctx[sqid].mem_type = 0;
    sec_eh_cfg->sq_ctx[sqid].d_addr = NULL;
    sec_eh_cfg->sq_ctx[sqid].sq_paddr = 0;
    sec_eh_cfg->sq_ctx[sqid].pid = 0;
    sec_eh_cfg->sq_ctx[sqid].sq_dev_vaddr = NULL;
    sec_eh_cfg->sq_ctx[sqid].sq_create_cmd_type = 0;
    ka_task_mutex_unlock(&sec_eh_cfg->mutex);
}

static void sec_sq_map_param_pack(struct trs_sq_mem_map_para *sq_map_param, struct trs_sec_eh_sq_ctx_info *ctx)
{
    sq_map_param->chan_types.type = CHAN_TYPE_HW;
    sq_map_param->chan_types.sub_type = CHAN_SUB_TYPE_HW_RTS;
    sq_map_param->sq_para.sqe_size = ctx->sqesize;
    sq_map_param->sq_para.sq_depth = ctx->sqdepth;
    sq_map_param->host_pid = ctx->pid;
    sq_map_param->sq_phy_addr = ctx->sq_paddr;
    sq_map_param->mem_type = ctx->mem_type;
}

u64 trs_sec_eh_alloc_sq_mem(struct trs_sec_eh_ts_inst *sec_eh_cfg, struct trs_sec_eh_sq_ctx_info *ctx)
{
    struct trs_chan_mem_attr mem_attr = {0};
    u64 size = ctx->sqesize * ctx->sqdepth;
    int ret;
    struct trs_sq_mem_map_para sec_sq_map_param;
    if ((size == 0) || (ctx->sqesize > TRS_SEC_EH_MAX_SQE_SIZE) || (ctx->sqdepth > TRS_SEC_EH_MAX_SQDEPTH_SIZE)) {
        trs_err("Invalid. (devid=%u; tsid=%u; sqid=%u; offset=0x%pK)\n", sec_eh_cfg->inst.devid, sec_eh_cfg->inst.tsid,
                ctx->sqid, (void *)ctx->addr_offset);
        return -EINVAL;
    }

    ka_task_mutex_lock(&sec_eh_cfg->mutex);
    if (sec_eh_cfg->sq_ctx[ctx->sqid].d_addr != NULL) {
        ka_task_mutex_unlock(&sec_eh_cfg->mutex);
        return -EEXIST;
    }

    ctx->sq_addr = trs_chan_sq_mem_alloc(&sec_eh_cfg->pm_inst, ctx->sqesize, ctx->sqdepth, &mem_attr);
    if (ctx->sq_addr == NULL) {
        ka_task_mutex_unlock(&sec_eh_cfg->mutex);
        return -ENOMEM;
    }
    ctx->sq_paddr = mem_attr.phy_addr;
    ctx->mem_type = mem_attr.mem_type;

    sec_sq_map_param_pack(&sec_sq_map_param, ctx);
    ret = trs_chan_sq_rsvmem_map(&sec_eh_cfg->inst, &sec_sq_map_param, &ctx->sq_dev_vaddr);
    if (ret != 0) {
#ifndef EMU_ST
        trs_chan_sq_mem_free(&sec_eh_cfg->pm_inst, ctx->sq_addr, &mem_attr);
        trs_err("map sq mem failed. (devid=%u; tsid=%u)\n", sec_eh_cfg->inst.devid, sec_eh_cfg->inst.tsid);
        ka_task_mutex_unlock(&sec_eh_cfg->mutex);
        return ret;
#endif
    }

    ka_task_mutex_unlock(&sec_eh_cfg->mutex);

    return 0;
}

static void sec_sq_unmap_param_pack(struct trs_sq_mem_map_para *sq_map_param, struct trs_sec_eh_sq_ctx *sq_ctx)
{
    sq_map_param->chan_types.type = CHAN_TYPE_HW;
    sq_map_param->chan_types.sub_type = CHAN_SUB_TYPE_HW_RTS;
    sq_map_param->sq_para.sqe_size = sq_ctx->sqe_size;
    sq_map_param->sq_para.sq_depth = sq_ctx->sq_depth;
    sq_map_param->host_pid = sq_ctx->pid;
    sq_map_param->sq_phy_addr = sq_ctx->sq_paddr;
    sq_map_param->mem_type = sq_ctx->mem_type;
}

void trs_sec_eh_free_sq_mem(struct trs_sec_eh_ts_inst *sec_eh_cfg, u32 sqid)
{
    struct trs_sec_eh_sq_ctx *sq_ctx = sec_eh_cfg->sq_ctx;
    struct trs_sq_mem_map_para sec_sq_map_param;

    ka_task_mutex_lock(&sec_eh_cfg->mutex);
    if (sq_ctx[sqid].d_addr != NULL) {
        struct trs_chan_mem_attr mem_attr = {sq_ctx[sqid].sq_paddr, sq_ctx[sqid].sq_depth * sq_ctx[sqid].sqe_size,
                                             sq_ctx[sqid].mem_type};
        sec_sq_unmap_param_pack(&sec_sq_map_param, &sq_ctx[sqid]);
        trs_chan_sq_rsvmem_unmap(&sec_eh_cfg->inst, &sec_sq_map_param, sq_ctx[sqid].sq_dev_vaddr);
        sq_ctx[sqid].sq_dev_vaddr = NULL;

        trs_chan_sq_mem_free(&sec_eh_cfg->pm_inst, sq_ctx[sqid].d_addr, &mem_attr);
        sq_ctx[sqid].d_addr = NULL;
    }
    ka_task_mutex_unlock(&sec_eh_cfg->mutex);
}

void trs_sec_eh_free_sq_mem_all(struct trs_id_inst *inst)
{
    struct trs_sec_eh_ts_inst *sec_eh_cfg = NULL;
    u32 num = 0;
    u32 sqid;

    sec_eh_cfg = trs_sec_eh_ts_inst_get(inst);
    if (sec_eh_cfg != NULL) {
        for (sqid = 0; sqid < sec_eh_cfg->id_info[TRS_HW_SQ].end; sqid++) {
            trs_sec_eh_free_sq_mem(sec_eh_cfg, sqid);
            num++;
        }

        trs_info("Kernel free sq success. (devid=%u; pm_devid=%u; num=%u)\n", sec_eh_cfg->inst.devid,
                 sec_eh_cfg->pm_inst.devid, num);
        trs_sec_eh_ts_inst_put(sec_eh_cfg);
    }
}

int _trs_sec_eh_sqe_update(struct trs_id_inst *inst, int pid, u32 sqid, u32 sqeid, u8 data[])
{
    struct trs_sec_eh_ts_inst *sec_eh_cfg = NULL;
    u8 *s_addr = (void *)data;
    u8 *d_addr = NULL;
    u64 sqe_size, sqe_pa;
    u32 sq_mem_type;
    int ret = -EINVAL;

    sec_eh_cfg = trs_sec_eh_ts_inst_get(inst);
    if (sec_eh_cfg != NULL) {
        struct trs_sqe_update_info update_info = {0};

        if (!trs_sec_eh_id_is_belong_to_vf(&sec_eh_cfg->id_info[TRS_HW_SQ], sqid)) {
            trs_sec_eh_ts_inst_put(sec_eh_cfg);
            trs_debug("Invalid. (devid=%u; type=%d; id=%u)\n", inst->devid, TRS_HW_SQ, sqid);
            return -EACCES;
        }

        ka_task_mutex_lock(&sec_eh_cfg->mutex);
        if ((sqeid >= sec_eh_cfg->sq_ctx[sqid].sq_depth) || (sec_eh_cfg->sq_ctx[sqid].d_addr == NULL)) {
            ka_task_mutex_unlock(&sec_eh_cfg->mutex);
            trs_sec_eh_ts_inst_put(sec_eh_cfg);
            return -EINVAL;
        }

        sqe_size = sec_eh_cfg->sq_ctx[sqid].sqe_size;
        d_addr = sec_eh_cfg->sq_ctx[sqid].d_addr + sqe_size * sqeid;
        sqe_pa = sec_eh_cfg->sq_ctx[sqid].sq_paddr + sqe_size * sqeid;
        sq_mem_type = sec_eh_cfg->sq_ctx[sqid].mem_type;

        update_info.pid = pid;
        update_info.sqid = sqid;
        update_info.sqeid = sqeid;
        update_info.sqe = s_addr;
        update_info.long_sqe_cnt = &sec_eh_cfg->sq_ctx[sqid].long_sqe_cnt;

        if (sec_eh_cfg->sq_ctx[sqid].sq_create_cmd_type == TRS_MBOX_CREATE_TOPIC_SQCQ) {
            ret = esched_drv_sqe_update(sec_eh_cfg->inst.devid, sec_eh_cfg->inst.tsid, pid, sqid, s_addr);
        } else {
            ret = trs_chan_ops_sqe_update(&sec_eh_cfg->inst, &update_info);
        }

        if (ret != 0) {
            ka_task_mutex_unlock(&sec_eh_cfg->mutex);
            trs_sec_eh_ts_inst_put(sec_eh_cfg);
            return ret;
        } else if (sec_eh_cfg->sq_ctx[sqid].long_sqe_cnt != 0) {
            sec_eh_cfg->sq_ctx[sqid].long_sqe_cnt--;
        }

        ka_mm_memcpy_toio(d_addr, s_addr, sqe_size);
        trs_chan_flush_sqe_cache(&sec_eh_cfg->inst, sq_mem_type, sqe_pa, sqe_size);
        ka_task_mutex_unlock(&sec_eh_cfg->mutex);
        trs_sec_eh_ts_inst_put(sec_eh_cfg);
    }

    return ret;
}

int trs_sec_eh_check_and_update_cq_ctx_info(struct trs_sec_eh_ts_inst *sec_eh_cfg, struct trs_sec_eh_cq_ctx_info *ctx)
{
    struct uda_mia_dev_para mia_para = {0};
    u64 cq_addr_gfn, cq_addr_mfn;
    struct trs_id_inst inst;
    void *vdavinci = NULL;
    u32 pf_id, vf_id;
    int ret;

    trs_id_inst_pack(&inst, sec_eh_cfg->inst.devid, sec_eh_cfg->inst.tsid);
    if (devdrv_get_connect_protocol(inst.devid) != CONNECT_PROTOCOL_HCCS) {
        return 0;
    }

#ifndef EMU_ST
    if (((ctx->cqesize % TRS_SEC_EH_CQE_ALIGN_SIZE) != 0) || ((ctx->cqesize * ctx->cqdepth) != KA_MM_PAGE_SIZE) ||
        (ctx->cq_paddr == 0) || ((ctx->cq_paddr & (KA_MM_PAGE_SIZE - 1)) != 0)) {
        trs_err("Invalid cq size or addr. (devid=%u; cqesize=%u; cqdepth=%u; pagesize=0x%lx)\n", inst.devid,
                ctx->cqesize, ctx->cqdepth, KA_MM_PAGE_SIZE);
        return -EINVAL;
    }

    ret = uda_udevid_to_mia_devid(inst.devid, &mia_para);
    if (ret != 0) {
        trs_err("Get pf and vf id failed. (devid=%u; ret=%d)\n", inst.devid, ret);
        return -EINVAL;
    }

    pf_id = mia_para.phy_devid;
    vf_id = mia_para.sub_devid + 1;
    trs_debug("Get vdavinci. (devid=%u; pf_id=%u; vf_id=%u; pm_devid=%u)\n", inst.devid, pf_id, vf_id,
              sec_eh_cfg->pm_inst.devid);

    vdavinci = vmngh_get_vdavinci_by_id(pf_id, vf_id);
    if (vdavinci == NULL) {
        trs_err("Get vdavinci failed. (devid=%u)\n", inst.devid);
        return -EINVAL;
    }

    cq_addr_gfn = (ctx->cq_paddr >> KA_MM_PAGE_SHIFT);
    cq_addr_mfn = hw_dvt_hypervisor_gfn_to_mfn(vdavinci, cq_addr_gfn);
    if ((cq_addr_mfn == 0) || (cq_addr_mfn == 0xffffffffffffffff)) {
        trs_err("Get mfn failed. (devid=%u; pf_id=%u; vf_id=%u)\n", inst.devid, pf_id, vf_id);
        return -EINVAL;
    }

    ctx->cq_paddr = ((u64)cq_addr_mfn << KA_MM_PAGE_SHIFT);
#endif

    return 0;
}
