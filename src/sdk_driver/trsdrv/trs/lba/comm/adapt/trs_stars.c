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
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_compiler_pub.h"
#include "soc_adapt.h"
#include "pbl_kref_safe.h"

#include "trs_pub_def.h"
#include "trs_stars.h"
struct trs_stars {
    struct trs_id_inst inst;

    void __ka_mm_iomem *base;
    phys_addr_t paddr;
    size_t size;

    u32 stride; // stars topic sched stride size
    int type;
    u32 cq_num;
    u32 cq_grp_num;

    trs_stars_set_cq_l1_mask_t set_cq_l1_mask;
    trs_stars_get_valid_cq_list_t get_valid_cq_list;
    trs_stars_addr_adjust_t addr_adjust;
    struct kref_safe ref;
};

static struct trs_stars *g_trs_stars[TRS_TS_INST_MAX_NUM][TRS_STARS_MAX];
static KA_TASK_DEFINE_RWLOCK(trs_stars_lock);
static struct trs_stars *trs_stars_create(struct trs_id_inst *inst, int type, struct trs_stars_attr *attr)
{
    struct trs_stars *stars = trs_kzalloc(sizeof(struct trs_stars), KA_GFP_KERNEL);

    if (stars == NULL) {
        return NULL;
    }
    stars->base = ka_mm_ioremap(attr->paddr, attr->size);
    if (stars->base == NULL) {
        trs_kfree(stars);
        return NULL;
    }
    stars->inst = *inst;
    stars->paddr = attr->paddr;
    stars->size = attr->size;
    stars->stride = attr->stride;
    stars->type = type;
    stars->cq_num = attr->cq_num;
    stars->cq_grp_num = attr->cq_grp_num;
    stars->set_cq_l1_mask = attr->set_cq_l1_mask;
    stars->get_valid_cq_list = attr->get_valid_cq_list;

    kref_safe_init(&stars->ref);
    return stars;
}

static void trs_stars_destroy(struct trs_stars *stars)
{
    if (stars->base != NULL) {
        ka_mm_iounmap(stars->base);
        stars->base = NULL;
    }
    trs_kfree(stars);
}

int trs_stars_test_bit(u32 nr, u32 val)
{
    return val & (1U << nr);
}

static int trs_stars_add(struct trs_stars *stars)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(&stars->inst);
    unsigned long flags;

    ka_task_write_lock_irqsave(&trs_stars_lock, flags);
    if (g_trs_stars[ts_inst][stars->type] != NULL) {
        ka_task_write_unlock_irqrestore(&trs_stars_lock, flags);
        return -ENODEV;
    }
    g_trs_stars[ts_inst][stars->type] = stars;
    ka_task_write_unlock_irqrestore(&trs_stars_lock, flags);
    return 0;
}

static void trs_stars_release(struct kref_safe *kref)
{
    struct trs_stars *stars = ka_container_of(kref, struct trs_stars, ref);

    trs_info("Trs stars uninit. (devid=%u; tsid=%u; type=%d)\n", stars->inst.devid, stars->inst.tsid, stars->type);
    trs_stars_destroy(stars);
}

static void trs_stars_del(struct trs_id_inst *inst, int type)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(inst);
    struct trs_stars *stars = NULL;
    unsigned long flags;

    ka_task_write_lock_irqsave(&trs_stars_lock, flags);
    stars = g_trs_stars[ts_inst][type];
    g_trs_stars[ts_inst][type] = NULL;
    ka_task_write_unlock_irqrestore(&trs_stars_lock, flags);

    if (stars != NULL) {
        kref_safe_put(&stars->ref, trs_stars_release);
    }
}

static struct trs_stars *trs_stars_get(struct trs_id_inst *inst, int type)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(inst);
    struct trs_stars *stars = NULL;
    unsigned long flags;

    ka_task_read_lock_irqsave(&trs_stars_lock, flags);
    stars = g_trs_stars[ts_inst][type];
    if (stars != NULL) {
        kref_safe_get(&stars->ref);
    }
    ka_task_read_unlock_irqrestore(&trs_stars_lock, flags);
    return stars;
}

static void trs_stars_put(struct trs_stars *stars)
{
    if (stars != NULL) {
        kref_safe_put(&stars->ref, trs_stars_release);
    }
}

static int trs_stars_range_check(struct trs_stars *stars, unsigned long offset, size_t size)
{
    if (ka_unlikely((offset > (unsigned long)stars->size) || (offset + (unsigned long)size) > stars->size)) {
        return -ENOMEM;
    }
    return 0;
}

static u32 trs_stars_addr_adjust(struct trs_stars *stars, u32 val)
{
    if (stars->addr_adjust != NULL) {
        return stars->addr_adjust(stars->inst.devid, val);
    }
    return 0;
}

int trs_stars_get_sq_tail(struct trs_id_inst *inst, u32 sqid, u32 *tail)
{
    struct trs_stars *stars = NULL;
    void __ka_mm_iomem *vaddr = NULL;
    unsigned long offset;
    int ret, reg_offset;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }

    ret = trs_soc_get_sq_tail_reg_offset(inst, &reg_offset);
    if (ret != 0) {
        trs_stars_put(stars);
        return ret;
    }

    offset = (unsigned long)sqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        vaddr = stars->base + offset + trs_stars_addr_adjust(stars, sqid);
        *tail = ka_mm_readl(vaddr);
    }
    trs_stars_put(stars);
    return ret;
}

int trs_stars_set_sq_tail(struct trs_id_inst *inst, u32 sqid, u32 tail)
{
    struct trs_stars *stars = NULL;
    void __ka_mm_iomem *vaddr = NULL;
    unsigned long offset;
    int ret, reg_offset;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }

    ret = trs_soc_get_sq_tail_reg_offset(inst, &reg_offset);
    if (ret != 0) {
        trs_stars_put(stars);
        return ret;
    }

    offset = (unsigned long)sqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        vaddr = stars->base + offset + trs_stars_addr_adjust(stars, sqid);
        ka_mm_writel(tail, vaddr);
    }
    trs_stars_put(stars);
    return ret;
}

int trs_stars_get_cq_head(struct trs_id_inst *inst, u32 cqid, u32 *head)
{
    struct trs_stars *stars = NULL;
    void __ka_mm_iomem *vaddr = NULL;
    unsigned long offset;
    int ret, reg_offset;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }

    ret = trs_soc_get_cq_head_reg_offset(inst, &reg_offset);
    if (ret != 0) {
        trs_stars_put(stars);
        return ret;
    }

    offset = (unsigned long)cqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        vaddr = stars->base + offset + trs_stars_addr_adjust(stars, cqid);
        *head = ka_mm_readl(vaddr);
    }
    trs_stars_put(stars);
    return ret;
}

int trs_stars_set_cq_head(struct trs_id_inst *inst, u32 cqid, u32 head)
{
    struct trs_stars *stars = NULL;
    void __ka_mm_iomem *vaddr = NULL;
    unsigned long offset;
    int ret, reg_offset;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }

    ret = trs_soc_get_cq_head_reg_offset(inst, &reg_offset);
    if (ret != 0) {
        trs_stars_put(stars);
        return ret;
    }

    offset = (unsigned long)cqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        vaddr = stars->base + offset + trs_stars_addr_adjust(stars, cqid);
        ka_mm_writel(head, vaddr);
    }
    trs_stars_put(stars);
    return ret;
}

int trs_stars_get_cq_tail(struct trs_id_inst *inst, u32 cqid, u32 *tail)
{
    struct trs_stars *stars = NULL;
    void __ka_mm_iomem *vaddr = NULL;
    unsigned long offset;
    int ret, reg_offset;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }

    ret = trs_soc_get_cq_tail_reg_offset(inst, &reg_offset);
    if (ret != 0) {
        trs_stars_put(stars);
        return ret;
    }

    offset = (unsigned long)cqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        vaddr = stars->base + offset + trs_stars_addr_adjust(stars, cqid);
        *tail = ka_mm_readl(vaddr);
    }
    trs_stars_put(stars);
    return ret;
}

static bool _trs_stars_sq_is_enabled(struct trs_stars *stars, u32 sqid)
{
    void __ka_mm_iomem *vaddr = NULL;
    unsigned long offset;
    u32 val, reg_offset;
    int ret;

    ret = trs_soc_get_sq_status_reg_offset(&stars->inst, &reg_offset);
    if (ret != 0) {
        return ret;
    }

    offset = (unsigned long)sqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        vaddr = stars->base + offset + trs_stars_addr_adjust(stars, sqid);
        val = ka_mm_readl(vaddr);
        if ((val & 0x1) == 1) {
            return true;
        }
    }
    return false;
}

static int _trs_stars_set_sq_status(struct trs_stars *stars, u32 sqid, int val)
{
    void __ka_mm_iomem *vaddr = NULL;
    unsigned long offset;
    u32 reg_offset;
    int ret;

    ret = trs_soc_get_sq_status_reg_offset(&stars->inst, &reg_offset);
    if (ret != 0) {
        return ret;
    }

    offset = (unsigned long)sqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        vaddr = stars->base + offset + trs_stars_addr_adjust(stars, sqid);
        ka_mm_writel((val & 0x1U), vaddr);
    }
    return ret;
}

static int _trs_stars_get_sq_status(struct trs_stars *stars, u32 sqid, u32 *val)
{
    void __ka_mm_iomem *vaddr = NULL;
    unsigned long offset;
    u32 reg_offset;
    int ret;

    ret = trs_soc_get_sq_status_reg_offset(&stars->inst, &reg_offset);
    if (ret != 0) {
        return ret;
    }

    offset = (unsigned long)sqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        vaddr = stars->base + offset + trs_stars_addr_adjust(stars, sqid);
        *val = ka_mm_readl(vaddr);
    }
    return ret;
}

static int _trs_stars_set_sq_head(struct trs_stars *stars, u32 sqid, u32 head)
{
    void __ka_mm_iomem *vaddr = NULL;
    unsigned long offset;
    u32 reg_offset;
    int ret;

    ret = trs_soc_get_sq_head_reg_offset(&stars->inst, &reg_offset);
    if (ret != 0) {
        return ret;
    }

    offset = (unsigned long)sqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        vaddr = stars->base + offset + trs_stars_addr_adjust(stars, sqid);
        ka_mm_writel(head, vaddr);
    }
    return ret;
}

static int _trs_stars_get_sq_head(struct trs_stars *stars, u32 sqid, u32 *head)
{
    void __ka_mm_iomem *vaddr = NULL;
    unsigned long offset;
    u32 reg_offset;
    int ret;

    ret = trs_soc_get_sq_head_reg_offset(&stars->inst, &reg_offset);
    if (ret != 0) {
        return ret;
    }

    offset = (unsigned long)sqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        vaddr = stars->base + offset + trs_stars_addr_adjust(stars, sqid);
        *head = ka_mm_readl(vaddr);
    }
    return ret;
}

static void trs_stars_cqint_pack(struct trs_stars_cqint *cqint, struct trs_stars *stars, u32 group)
{
    cqint->base = stars->base + trs_stars_addr_adjust(stars, group);
    cqint->size = stars->size;
    cqint->group = group;
    cqint->cq_num = stars->cq_num;
    cqint->cq_grp_num = stars->cq_grp_num;
}

int trs_stars_set_cq_l1_mask(struct trs_id_inst *inst, u32 val, u32 group)
{
    struct trs_stars *stars = NULL;
    struct trs_stars_cqint cqint;

    stars = trs_stars_get(inst, TRS_STARS_CQINT);
    if (stars == NULL) {
        return -ENODEV;
    }

    trs_stars_cqint_pack(&cqint, stars, group);
    if (stars->set_cq_l1_mask != NULL) {
        stars->set_cq_l1_mask(&cqint, val);
    }
    trs_stars_put(stars);
    return 0;
}

int trs_stars_get_valid_cq_list(struct trs_id_inst *inst, u32 group, u32 cqid[], u32 num, u32 *valid_num)
{
    struct trs_stars *stars = NULL;
    struct trs_stars_cqint cqint;
    int ret = -EEXIST;

    stars = trs_stars_get(inst, TRS_STARS_CQINT);
    if (stars == NULL) {
        return -ENODEV;
    }
    trs_stars_cqint_pack(&cqint, stars, group);
    if (stars->get_valid_cq_list != NULL) {
        ret = stars->get_valid_cq_list(&cqint, cqid, num, valid_num);
    }
    trs_stars_put(stars);

    return ret;
}

int trs_stars_get_cq_affinity_group(struct trs_id_inst *inst, u32 cq_id, u32 *group)
{
    struct trs_stars *stars = trs_stars_get(inst, TRS_STARS_CQINT);

    if (stars == NULL) {
        return -ENODEV;
    }

    if ((stars->cq_grp_num == 0) || (stars->cq_num < stars->cq_grp_num)) {
        trs_stars_put(stars);
        return -EINVAL;
    }

    *group = cq_id / (stars->cq_num / stars->cq_grp_num);
    trs_stars_put(stars);

    return 0;
}

/*
 * Sq is disabled, then enable it and return succeed.
 * Sq is already in enable state, return -EAGAIN.
 */
int trs_stars_sq_enable(struct trs_id_inst *inst, u32 sqid)
{
    struct trs_stars *stars = NULL;
    int ret = -EAGAIN;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }
    if (!_trs_stars_sq_is_enabled(stars, sqid)) {
        /* Disabled sq status, re-enable it */
        ret = _trs_stars_set_sq_status(stars, sqid, 1);
    }
    trs_stars_put(stars);

    return ret;
}

int trs_stars_get_sq_head_paddr(struct trs_id_inst *inst, u32 sqid, u64 *paddr)
{
    struct trs_stars *stars = NULL;
    unsigned long offset;
    int ret, reg_offset;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }

    ret = trs_soc_get_sq_head_reg_offset(inst, &reg_offset);
    if (ret != 0) {
        trs_stars_put(stars);
        return ret;
    }

    offset = (unsigned long)sqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        *paddr = (u64)(stars->paddr + offset + trs_stars_addr_adjust(stars, sqid));
    }

    trs_stars_put(stars);

    return ret;
}

int trs_stars_get_sq_tail_paddr(struct trs_id_inst *inst, u32 sqid, u64 *paddr)
{
    struct trs_stars *stars = NULL;
    unsigned long offset;
    int ret, reg_offset;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        trs_err("Stars uninit. (devid=%u; tsid=%u; sqid=%u)\n", inst->devid, inst->tsid, sqid);
        return -ENODEV;
    }

    ret = trs_soc_get_sq_tail_reg_offset(inst, &reg_offset);
    if (ret != 0) {
        trs_stars_put(stars);
        return ret;
    }

    offset = (unsigned long)sqid * stars->stride + reg_offset;
    ret = trs_stars_range_check(stars, offset, sizeof(u32));
    if (ret == 0) {
        *paddr = (u64)(stars->paddr + offset + trs_stars_addr_adjust(stars, sqid));
    }

    trs_stars_put(stars);

    return ret;
}

int trs_stars_get_sq_head(struct trs_id_inst *inst, u32 sqid, u32 *head)
{
    struct trs_stars *stars = NULL;
    int ret;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }
    ret = _trs_stars_get_sq_head(stars, sqid, head);
    trs_stars_put(stars);

    return ret;
}

int trs_stars_set_sq_head(struct trs_id_inst *inst, u32 sqid, u32 head)
{
    struct trs_stars *stars = NULL;
    int ret;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }
    if (_trs_stars_sq_is_enabled(stars, sqid)) {
        trs_stars_put(stars);
        trs_err("Sq is enabled. do not set sq head. (devid=%u; tsid=%u; sqid=%u)\n", inst->devid, inst->tsid, sqid);
        return -EBUSY;
    }
    ret = _trs_stars_set_sq_head(stars, sqid, head);
    trs_stars_put(stars);
    return ret;
}

int trs_stars_set_sq_status(struct trs_id_inst *inst, u32 sqid, u32 val)
{
    struct trs_stars *stars = NULL;
    int ret;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }

    if (val == 0) {
        if (!_trs_stars_sq_is_enabled(stars, sqid)) {
            ret = 0;
            goto out;
        }
        ret = _trs_stars_set_sq_status(stars, sqid, 0);
    } else {
        if (_trs_stars_sq_is_enabled(stars, sqid)) {
            ret = 0;
            goto out;
        }
        ret = _trs_stars_set_sq_status(stars, sqid, 1);
    }
out:
    trs_stars_put(stars);
    return ret;
}

int trs_stars_get_sq_status(struct trs_id_inst *inst, u32 sqid, u32 *val)
{
    struct trs_stars *stars = NULL;
    int ret;

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }
    ret = _trs_stars_get_sq_status(stars, sqid, val);
    trs_stars_put(stars);
    return ret;
}

int trs_stars_get_rtsq_paddr(struct trs_id_inst *inst, u32 sqid, phys_addr_t *paddr, size_t *size)
{
    struct trs_stars *stars = NULL;
    unsigned long offset;
    int ret;

    ret = trs_id_inst_check(inst);
    if (ret != 0) {
        return ret;
    }

    stars = trs_stars_get(inst, TRS_STARS_SCHED);
    if (stars == NULL) {
        return -ENODEV;
    }
    offset = (unsigned long)sqid * stars->stride;
    ret = trs_stars_range_check(stars, offset, (size_t)stars->stride);
    if (ret == 0) {
        if (paddr != NULL) {
            *paddr = stars->paddr + offset + trs_stars_addr_adjust(stars, sqid);
        }
        if (size != NULL) {
            *size = stars->stride;
        }
    }
    trs_stars_put(stars);
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_stars_get_rtsq_paddr);

int trs_stars_addr_adjust_register(struct trs_id_inst *inst, int type, trs_stars_addr_adjust_t addr_adjust)
{
    struct trs_stars *stars = NULL;

    stars = trs_stars_get(inst, type);
    if (stars == NULL) {
        return -ENODEV;
    }

    stars->addr_adjust = addr_adjust;
    trs_stars_put(stars);
    return 0;
}

void trs_stars_addr_adjust_unregister(struct trs_id_inst *inst, int type)
{
    struct trs_stars *stars = NULL;

    stars = trs_stars_get(inst, type);
    if (stars == NULL) {
        return;
    }

    stars->addr_adjust = NULL;
    trs_stars_put(stars);
}

int trs_stars_init(struct trs_id_inst *inst, int type, struct trs_stars_attr *attr)
{
    struct trs_stars *stars = NULL;
    int ret;

    if ((type == TRS_STARS_CQINT) && ((attr->cq_grp_num == 0) || (attr->cq_num < attr->cq_grp_num))) {
        trs_err("Invalid cq group config. (devid=%u; tsid=%u; cq_num=%u; cq_grp_num=%u)\n", inst->devid, inst->tsid,
                attr->cq_num, attr->cq_grp_num);
        return -EINVAL;
    }

    stars = trs_stars_create(inst, type, attr);
    if (stars == NULL) {
        return -ENOMEM;
    }

    ret = trs_stars_add(stars);
    if (ret != 0) {
        trs_stars_destroy(stars);
    }
    trs_debug("Trs stars init. (devid=%u; tsid=%u; type=%d; ret=%d)\n", inst->devid, inst->tsid, type, ret);
    return ret;
}

void trs_stars_uninit(struct trs_id_inst *inst, int type)
{
    trs_stars_del(inst, type);
}
