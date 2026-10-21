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
#include "ka_common_pub.h"
#include "ka_driver_pub.h"

#include "ascend_hal_define.h"
#include "trs_id.h"
#include "trs_ts_inst.h"
#include "trs_ioctl.h"
#include "trs_proc.h"
#include "trs_shr_sqcq.h"

struct trs_shr_ctx_mng {
    struct trs_sq_ctx *sq_ctx;
    struct kref_safe ref;
};
struct trs_shr_ctx_mng *shr_ctx_mng[TRS_TS_INST_MAX_NUM] = {
    NULL,
};
static KA_TASK_DEFINE_RWLOCK(shr_ctx_mng_lock);

struct trs_shr_ctx_mng *trs_get_shr_ctx(struct trs_id_inst *inst)
{
    u32 ts_inst_id = trs_id_inst_to_ts_inst(inst);
    struct trs_shr_ctx_mng *shr_ctx = NULL;

    if (trs_id_inst_check(inst) != 0) {
        return NULL;
    }

#ifndef EMU_ST
    ka_task_read_lock_bh(&shr_ctx_mng_lock);
    shr_ctx = shr_ctx_mng[ts_inst_id];
    if (shr_ctx != NULL) {
        if (kref_safe_get_unless_zero(&shr_ctx->ref) == 0) {
            ka_task_read_unlock_bh(&shr_ctx_mng_lock);
            return NULL;
        }
    }
    ka_task_read_unlock_bh(&shr_ctx_mng_lock);
#endif

    return shr_ctx;
}

void trs_shr_ctx_mng_release(struct kref_safe *kref)
{
    struct trs_shr_ctx_mng *shr_ctx = ka_container_of(kref, struct trs_shr_ctx_mng, ref);
#ifndef EMU_ST
    if (shr_ctx->sq_ctx != NULL) {
        trs_info("Shr sq ctx mng release\n");
        trs_vfree(shr_ctx->sq_ctx);
        shr_ctx->sq_ctx = NULL;
    }

    trs_vfree(shr_ctx);

    trs_info("Shr ctx mng release.\n");
#endif
}

void trs_put_shr_ctx(struct trs_shr_ctx_mng *shr_ctx)
{
    kref_safe_put(&shr_ctx->ref, trs_shr_ctx_mng_release);
}

int trs_shr_ctx_mng_init(struct trs_id_inst *inst)
{
    u32 ts_inst_id = trs_id_inst_to_ts_inst(inst);
    struct trs_shr_ctx_mng *shr_ctx = NULL;
    u32 sqid_max_num = 0;
    int ret;

#ifndef EMU_ST
    shr_ctx = trs_vzalloc(sizeof(struct trs_shr_ctx_mng));
    if (shr_ctx == NULL) {
        trs_err("Mem alloc failed. (devid=%u; tsid=%u; size=%lx)\n", inst->devid, inst->tsid,
                sizeof(struct trs_shr_ctx_mng));
        return -ENOMEM;
    }

    ret = trs_id_get_max_id(inst, TRS_HW_SQ_ID, &sqid_max_num);
    if (ret != 0) {
        trs_vfree(shr_ctx);
        trs_err("Get sq max id failed. (devid=%u; tsid=%u; res_type=%d; ret=%d)\n", inst->devid, inst->tsid,
                TRS_HW_SQ_ID, ret);
        return ret;
    }

    shr_ctx->sq_ctx = (struct trs_sq_ctx *)trs_vzalloc(sizeof(struct trs_sq_ctx) * sqid_max_num);
    if (shr_ctx->sq_ctx == NULL) {
        trs_vfree(shr_ctx);
        trs_err("Mem alloc failed. (devid=%u; tsid=%u; res_type=%d; size=0x%lx)\n", inst->devid, inst->tsid,
                TRS_HW_SQ_ID, sizeof(struct trs_sq_ctx) * sqid_max_num);
        return -ENOMEM;
    }

    kref_safe_init(&shr_ctx->ref);
    shr_ctx_mng[ts_inst_id] = shr_ctx;

    trs_info("Shr init info. (devid=%u; tsid=%u; res_type=%d; size=%lx; num=%u)\n", inst->devid, inst->tsid,
             TRS_HW_SQ_ID, sizeof(struct trs_sq_ctx) * sqid_max_num, sqid_max_num);
#endif

    return 0;
}

void trs_shr_ctx_mng_uninit(struct trs_id_inst *inst)
{
    u32 ts_inst_id = trs_id_inst_to_ts_inst(inst);
    struct trs_shr_ctx_mng *shr_ctx = shr_ctx_mng[ts_inst_id];

#ifndef EMU_ST
    if (shr_ctx_mng[ts_inst_id] == NULL) {
        trs_err("shr ctx mng is null. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return;
    }

    ka_task_write_lock_bh(&shr_ctx_mng_lock);
    shr_ctx_mng[ts_inst_id] = NULL;
    ka_task_write_unlock_bh(&shr_ctx_mng_lock);

    trs_put_shr_ctx(shr_ctx);
#endif
}

void trs_fill_map_addr_info(struct trs_sq_map_addr *addr_info, struct trs_mem_map_para *map_para)
{
#ifndef EMU_ST
    addr_info->uva = map_para->va;
    addr_info->len = map_para->len;
    addr_info->mem_type = map_para->type;
#endif
}

void trs_clear_map_addr_info(struct trs_sq_map_addr *addr_info)
{
#ifndef EMU_ST
    addr_info->uva = 0;
    addr_info->len = 0;
    addr_info->mem_type = 0;
#endif
}

#ifndef EMU_ST
ka_task_struct_t *trs_shr_get_task_struct(ka_pid_t pid)
{
    ka_task_struct_t *tsk = NULL;

    ka_task_rcu_read_lock();
    ka_for_each_process(tsk)
    {
        if (tsk->tgid == pid) {
            ka_task_get_task_struct(tsk);
            ka_task_rcu_read_unlock();
            return tsk;
        }
    }
    ka_task_rcu_read_unlock();

    return NULL;
}

void trs_shr_put_task_struct(ka_task_struct_t *tsk)
{
    ka_task_put_task_struct(tsk);
}

inline ka_rw_semaphore_t *trs_shr_get_mmap_sem(ka_mm_struct_t *mm)
{
    return ka_mm_get_mmap_sem(mm);
}

int trs_shr_unmap_sq(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_mem_unmap_para *para)
{
    ka_task_struct_t *tsk = NULL;
    ka_vm_area_struct_t *vma = NULL;
    ka_mm_struct_t *tsk_mm = NULL;
    ka_pid_t pid = proc_ctx->cp2_pid;
    int ret;

    if (pid == 0) {
        trs_debug("Not support\n");
        return 0;
    }

    tsk = trs_shr_get_task_struct(pid);
    if (tsk == NULL) {
        trs_debug("Find task fail. (pid=%d)\n", pid);
        return 0;
    }

    tsk_mm = ka_task_get_task_mm(tsk);
    if (tsk_mm == NULL) {
        trs_debug("Task mm is NULL. (pid=%d)\n", pid);
        ret = 0;
        goto get_task_mm_failed;
    }

    ka_task_down_write(trs_shr_get_mmap_sem(tsk_mm));
    vma = ka_mm_find_vma(tsk_mm, para->va);
    if (vma == NULL) {
        trs_debug("Find vma fail. (pid=%d)\n", pid);
        ret = -ENOMEM;
        goto find_vma_failed;
    }

    ret = trs_unmap_sq_mem(proc_ctx, vma, para);

find_vma_failed:
    ka_task_up_write(trs_shr_get_mmap_sem(tsk_mm));
    ka_mm_mmput(tsk_mm);
get_task_mm_failed:
    trs_shr_put_task_struct(tsk);

    return ret;
}

int trs_shr_remap_sq(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_mem_map_para *para)
{
    ka_vm_area_struct_t *vma = NULL;
    int ret;

    ka_task_down_write(trs_shr_get_mmap_sem(ka_task_get_current_mm()));

    vma = ka_mm_find_vma(ka_task_get_current_mm(), para->va);
    if (vma == NULL) {
        ka_task_up_write(trs_shr_get_mmap_sem(ka_task_get_current_mm()));
        trs_err("Find vma failed. (va=0x%pK)\n", (void *)(uintptr_t)para->va);
        return -EINVAL;
    }

    ret = trs_remap_sq_mem(proc_ctx, vma, para);
    ka_task_up_write(trs_shr_get_mmap_sem(ka_task_get_current_mm()));

    return ret;
}
#else
int trs_shr_unmap_sq(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_mem_unmap_para *para)
{
    return 0;
}

int trs_shr_remap_sq(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_mem_map_para *para)
{
    return 0;
}
#endif

void trs_shr_sq_unmap(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct trs_sq_ctx *sq_ctx)
{
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_shr_ctx_mng *shr_ctx = NULL;
    struct trs_sq_ctx *shr_sq_ctx = NULL;
    struct trs_mem_unmap_para unmap_para;
    u32 sqid = sq_ctx->sqid;

#ifndef EMU_ST
    shr_ctx = trs_get_shr_ctx(inst);
    if (shr_ctx == NULL) {
        return;
    }

    shr_sq_ctx = &shr_ctx->sq_ctx[sqid];
    if (shr_sq_ctx->db.uva != 0) {
        trs_unmap_fill_para(&unmap_para, shr_sq_ctx->db.mem_type, shr_sq_ctx->db.uva, shr_sq_ctx->db.len);
        (void)trs_shr_unmap_sq(proc_ctx, ts_inst, &unmap_para);
        trs_clear_map_addr_info(&shr_sq_ctx->db);
    }

    if (shr_sq_ctx->head.uva != 0) {
        trs_unmap_fill_para(&unmap_para, shr_sq_ctx->head.mem_type, shr_sq_ctx->head.uva, shr_sq_ctx->head.len);
        (void)trs_shr_unmap_sq(proc_ctx, ts_inst, &unmap_para);
        trs_clear_map_addr_info(&shr_sq_ctx->head);
    }

    if (shr_sq_ctx->que_mem.uva != 0) {
        trs_unmap_fill_para(&unmap_para, shr_sq_ctx->que_mem.mem_type, shr_sq_ctx->que_mem.uva,
                            shr_sq_ctx->que_mem.len);
        (void)trs_shr_unmap_sq(proc_ctx, ts_inst, &unmap_para);
        trs_clear_map_addr_info(&shr_sq_ctx->que_mem);
    }

    if (shr_sq_ctx->shr_info.uva != 0) {
        trs_unmap_fill_para(&unmap_para, shr_sq_ctx->shr_info.mem_type, shr_sq_ctx->shr_info.uva,
                            shr_sq_ctx->shr_info.len);
        (void)trs_shr_unmap_sq(proc_ctx, ts_inst, &unmap_para);
        trs_clear_map_addr_info(&shr_sq_ctx->shr_info);
    }

    shr_sq_ctx->map_flag = 0;
    trs_put_shr_ctx(shr_ctx);
#endif

    return;
}

int trs_shr_sq_remap(struct trs_proc_ctx *proc_ctx, struct trs_core_ts_inst *ts_inst, struct halSqCqInputInfo *para,
                     struct trs_sq_ctx *sq_ctx, struct trs_chan_sq_info *sq_info)
{
    struct trs_alloc_para *alloc_para = get_alloc_para_addr(para);
    struct trs_uio_info *uio_info = alloc_para->uio_info;
    struct trs_id_inst *inst = &ts_inst->inst;
    struct trs_shr_ctx_mng *shr_ctx = NULL;
    struct trs_sq_ctx *shr_sq_ctx = NULL;
    struct trs_mem_map_para map_para;
    unsigned long que_mem_len = 0;
    u32 sqid = sq_ctx->sqid;
    int ret;

    uio_info->uio_flag = 0;
    uio_info->soft_que_flag = 0;

    shr_ctx = trs_get_shr_ctx(inst);
    if (shr_ctx == NULL) {
        return 0;
    }

#ifndef EMU_ST
    shr_sq_ctx = &shr_ctx->sq_ctx[sqid];
    if (shr_sq_ctx->map_flag == 1) {
        trs_put_shr_ctx(shr_ctx);
        trs_err("Current shared sq has been mapped. (devid=%u; sqid=%u)\n", inst->devid, sqid);
        return -EINVAL;
    }

    if ((sq_info->sq_phy_addr == 0) || (sq_info->db_addr == 0) || (uio_info->sq_que_addr == 0)) {
        trs_put_shr_ctx(shr_ctx);
        return 0;
    }

    trs_remap_fill_para(&map_para, TRS_MAP_TYPE_MEM, false, uio_info->sq_ctrl_addr[TRS_UIO_SHR_INFO],
                        ka_mm_virt_to_phys(sq_ctx->shr_info.kva), KA_MM_PAGE_SIZE);
    ret = trs_shr_remap_sq(proc_ctx, ts_inst, &map_para);
    if (ret != 0) {
        goto out;
    }
    trs_fill_map_addr_info(&shr_sq_ctx->shr_info, &map_para);

    que_mem_len = KA_MM_PAGE_ALIGN(sq_info->sq_para.sq_depth * sq_info->sq_para.sqe_size);
    trs_remap_fill_para(&map_para, TRS_MAP_TYPE_MEM, sq_ctx->mode != SEND_MODE_UIO_T, uio_info->sq_que_addr,
                        sq_info->sq_phy_addr, que_mem_len);
    ret = trs_shr_remap_sq(proc_ctx, ts_inst, &map_para);
    if (ret != 0) {
        goto out;
    }
    trs_fill_map_addr_info(&shr_sq_ctx->que_mem, &map_para);

    trs_remap_fill_para(&map_para, TRS_MAP_TYPE_REG, true, uio_info->sq_ctrl_addr[TRS_UIO_HEAD],
                        KA_DRIVER_ALIGN_DOWN(sq_info->head_addr, KA_MM_PAGE_SIZE), KA_MM_PAGE_SIZE);
    uio_info->sq_ctrl_addr[TRS_UIO_HEAD] += sq_info->head_addr % KA_MM_PAGE_SIZE;
    ret = trs_shr_remap_sq(proc_ctx, ts_inst, &map_para);
    if (ret != 0) {
        uio_info->sq_ctrl_addr[TRS_UIO_HEAD] = 0;
        goto out;
    }
    trs_fill_map_addr_info(&shr_sq_ctx->head, &map_para);

    trs_remap_fill_para(&map_para, TRS_MAP_TYPE_REG, true, uio_info->sq_ctrl_addr[TRS_UIO_DB],
                        KA_DRIVER_ALIGN_DOWN(sq_info->db_addr, KA_MM_PAGE_SIZE), KA_MM_PAGE_SIZE);
    uio_info->sq_ctrl_addr[TRS_UIO_DB] += sq_info->db_addr % KA_MM_PAGE_SIZE;
    ret = trs_shr_remap_sq(proc_ctx, ts_inst, &map_para);
    if (ret != 0) {
        goto out;
    }
    trs_fill_map_addr_info(&shr_sq_ctx->db, &map_para);

    /* In uio_d scenario, user tail va need set to null, when user set tail replacing va with db */
    uio_info->sq_ctrl_addr[TRS_UIO_TAIL] = 0;

    shr_sq_ctx->map_flag = 1;
    uio_info->uio_flag = 1;
    trs_put_shr_ctx(shr_ctx);

    trs_debug("Share sq send use uio mode. (devid=%u; tsid=%u; sqid=%u)\n", inst->devid, inst->tsid, para->sqId);

    return 0;

out:
    trs_shr_sq_unmap(proc_ctx, ts_inst, sq_ctx);
    trs_put_shr_ctx(shr_ctx);
    trs_debug("Share sq send use kernel mode. (devid=%u; tsid=%u; sqid=%u; type=%u; flag=%u, ret=%d)\n", inst->devid,
              inst->tsid, para->sqId, para->type, para->flag, ret);
#endif

    return ret;
}
