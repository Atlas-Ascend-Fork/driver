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
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"

#include "pbl/pbl_soc_res.h"
#include "trs_chan_irq.h"
#include "trs_sqcq_timestamp.h"

static KA_BASE_RADIX_TREE(irq_tree, KA_GFP_KERNEL);
KA_TASK_DEFINE_MUTEX(irq_mutex);

struct trs_chan_irq_node {
    ka_list_head_t node;
    int (*handler)(int irq_type, int irq_index, void *para, u32 cqid[], u32 cq_num);
    void *para;
};

int trs_chan_get_irq_by_index(struct trs_id_inst *inst, int irq_type, int irq_index, u32 *irq)
{
    struct res_inst_info res_inst;
    int ret;

    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);

    ret = soc_resmng_get_irq_by_index(&res_inst, irq_type, irq_index, irq);
    if (ret != 0) {
        trs_err("Get failed. (devid=%u; tsid=%u; irq_type=%u; id=%u)\n", inst->devid, inst->tsid, irq_type, irq_index);
        return ret;
    }

    return 0;
}

int trs_chan_get_irq(struct trs_id_inst *inst, u32 irq_type, u32 irq[], u32 irq_num, u32 *valid_irq_num)
{
    struct res_inst_info res_inst;
    int ret;
    u32 i;

    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);
    ret = soc_resmng_get_irq_num(&res_inst, irq_type, valid_irq_num);
    if (ret != 0) {
        return ret;
    }

    *valid_irq_num = (*valid_irq_num > irq_num) ? irq_num : *valid_irq_num;
    for (i = 0; i < *valid_irq_num; i++) {
        ret = trs_chan_get_irq_by_index(inst, irq_type, i, &irq[i]);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_get_irq);

static void trs_chan_irq_unmask(ka_work_struct_t *p_work)
{
    struct trs_chan_irq *chan_irq = ka_container_of(p_work, struct trs_chan_irq, unmask_work);

    if (chan_irq->attr.intr_mask_config != NULL) {
        chan_irq->attr.intr_mask_config(&chan_irq->inst, chan_irq->attr.group, chan_irq->irq, 0);
    }
    trs_debug("Unmask with work.\n");
}

static void trs_chan_irq_tasklet(unsigned long data)
{
    struct trs_chan_irq *chan_irq = (struct trs_chan_irq *)data;
#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX
    trs_dfx_set_cq_tasklet_tick(chan_irq->inst.tsid);
#endif
    struct trs_chan_irq_node *irq_node = NULL;
    int proc_cnt = 0;
    u32 cq_num = 0;
    u32 *cqid_list = NULL;
    int ret = 0;

    while (1) {
        if (chan_irq->attr.get_valid_cq != NULL) {
            ret = chan_irq->attr.get_valid_cq(&chan_irq->inst, chan_irq->attr.group, chan_irq->cqid_list,
                                              MAX_PROC_CQ_NUM, &cq_num);
            if (ret == 0) {
                cqid_list = chan_irq->cqid_list;
            }
            if ((ret != 0) || (cq_num == 0)) {
#ifndef EMU_ST /* UT will return cq_num == 0 */
                break;
#endif
            }
            proc_cnt++;
            trs_debug("Current cq num. (cq_num=%u; proc_cnt=%d)\n", cq_num, proc_cnt);
        }

        ka_task_spin_lock_bh(&chan_irq->lock);
        ka_list_for_each_entry(irq_node, &chan_irq->head, node)
        {
            ret |= irq_node->handler((int)chan_irq->irq_type, chan_irq->irq_index, irq_node->para, cqid_list, cq_num);
        }
        ka_task_spin_unlock_bh(&chan_irq->lock);

        if (chan_irq->attr.get_valid_cq == NULL) {
            break; /* func cq doesn't need to get cq cyclically */
        }

        if (ka_system_time_after(ka_jiffies, chan_irq->timeout)) {
            ret = -EBUSY;
            trs_debug("Tasklet process time exceeds 0.5s. (proc_cnt=%d)\n", proc_cnt);
            break;
        }
    }

    if (ret == -EBUSY) {
        ka_task_schedule_work(&chan_irq->unmask_work);
        return;
    }
    if (chan_irq->attr.intr_mask_config != NULL) {
        chan_irq->attr.intr_mask_config(&chan_irq->inst, chan_irq->attr.group, chan_irq->irq, 0);
    }
}

static ka_irqreturn_t trs_adapt_chan_irq_proc(int irq, void *para)
{
    struct trs_chan_irq *chan_irq = (struct trs_chan_irq *)para;
#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX
    trs_dfx_set_cq_irq_tick(chan_irq->inst.tsid);
#endif
    chan_irq->timeout = ka_jiffies + (KA_HZ / 2); /* limit the time of tasklet to 1/2 seconds */

    if (chan_irq->attr.intr_mask_config != NULL) {
        chan_irq->attr.intr_mask_config(&chan_irq->inst, chan_irq->attr.group, irq, 1);
    }
    ka_system_tasklet_schedule(&chan_irq->task);
    return KA_IRQ_HANDLED;
}

static struct trs_chan_irq *trs_create_chan_irq(struct trs_id_inst *inst, u32 irq, int irq_type,
                                                struct trs_chan_irq_attr *attr)
{
    struct trs_chan_irq *chan_irq = trs_vzalloc(sizeof(struct trs_chan_irq));

    if (chan_irq != NULL) {
        int ret;
        chan_irq->attr = *attr;
        chan_irq->inst = *inst;
        ka_task_spin_lock_init(&chan_irq->lock);
        KA_INIT_LIST_HEAD(&chan_irq->head);
        ka_system_tasklet_init(&chan_irq->task, trs_chan_irq_tasklet, (uintptr_t)chan_irq);
        KA_TASK_INIT_WORK(&chan_irq->unmask_work, trs_chan_irq_unmask);
        ret = attr->request_chan_irq(inst, irq_type, irq, (void *)chan_irq, trs_adapt_chan_irq_proc);
        if (ret != 0) {
            trs_vfree(chan_irq);
            trs_err("Request irq failed. (irq=%u)\n", irq);
            return NULL;
        }
#ifdef CFG_FEATURE_IRQ_BIND
        (void)ka_base_irq_set_affinity_hint(irq, ka_system_get_cpu_mask(0));
#endif
        trs_debug("Request irq success. (irq=%u)\n", irq);
    }

    return chan_irq;
}

void trs_destroy_chan_irq(struct trs_chan_irq *chan_irq)
{
    trs_info("Free irq success. (irq=%u)\n", chan_irq->irq);

    (void)ka_base_irq_set_affinity_hint(chan_irq->irq, NULL);
    chan_irq->attr.free_chan_irq(&chan_irq->inst, chan_irq->irq_type, chan_irq->irq, chan_irq);
    ka_system_tasklet_kill(&chan_irq->task);
    ka_task_cancel_work_sync(&chan_irq->unmask_work);

    trs_vfree(chan_irq);
}

int trs_chan_request_irq(struct trs_id_inst *inst, int irq_type, int irq_index, struct trs_chan_irq_attr *attr)
{
    struct trs_chan_irq_node *irq_node = NULL;
    struct trs_chan_irq *chan_irq = NULL;
    u32 irq;
    int ret;

    ret = trs_chan_get_irq_by_index(inst, irq_type, irq_index, &irq);
    if (ret != 0) {
        return ret;
    }

    irq_node = trs_kzalloc(sizeof(struct trs_chan_irq_node), KA_GFP_KERNEL);
    if (irq_node == NULL) {
        trs_err("Malloc failed. (size=%lx)\n", sizeof(struct trs_chan_irq_node));
        return -ENOMEM;
    }
    irq_node->handler = attr->handler;
    irq_node->para = attr->para;

    ka_task_mutex_lock(&irq_mutex);
    chan_irq = ka_base_radix_tree_lookup(&irq_tree, irq);
    if (chan_irq == NULL) {
        chan_irq = trs_create_chan_irq(inst, irq, irq_type, attr);
        if (chan_irq == NULL) {
            ka_task_mutex_unlock(&irq_mutex);
            trs_kfree(irq_node);
            return -ENOMEM;
        }

        ret = ka_base_radix_tree_insert(&irq_tree, irq, chan_irq);
        if (ret != 0) {
            trs_destroy_chan_irq(chan_irq);
            ka_task_mutex_unlock(&irq_mutex);
            trs_kfree(irq_node);
            trs_err("Insert irq tree failed. (irq=%u; ret=%d)\n", irq, ret);
            return ret;
        }

        chan_irq->irq_type = irq_type;
        chan_irq->irq_index = irq_index;
        chan_irq->irq = irq;
    }
    chan_irq->ref++;

    ka_task_spin_lock_bh(&chan_irq->lock);
    ka_list_add(&irq_node->node, &chan_irq->head);
    ka_task_spin_unlock_bh(&chan_irq->lock);

    ka_task_mutex_unlock(&irq_mutex);

    return 0;
}

int trs_chan_free_irq(struct trs_id_inst *inst, int irq_type, int irq_index, void *para)
{
    struct trs_chan_irq_node *irq_node = NULL;
    struct trs_chan_irq *chan_irq = NULL;
    struct trs_chan_irq_node *tmp = NULL;
    u32 irq;
    int ret;

    ret = trs_chan_get_irq_by_index(inst, irq_type, irq_index, &irq);
    if (ret != 0) {
        return ret;
    }

    ka_task_mutex_lock(&irq_mutex);
    chan_irq = ka_base_radix_tree_lookup(&irq_tree, irq);
    if (chan_irq == NULL) {
        ka_task_mutex_unlock(&irq_mutex);
        trs_err("Find irq fail. (irq=%d)\n", irq);
        return -ENODEV;
    }

    ret = -EINVAL;
    ka_task_spin_lock_bh(&chan_irq->lock);
    ka_list_for_each_entry_safe(irq_node, tmp, &chan_irq->head, node)
    {
        if (irq_node->para == para) {
            ka_list_del(&irq_node->node);
            trs_kfree(irq_node);
            chan_irq->ref--;
            ret = 0;
            break;
        }
    }
    ka_task_spin_unlock_bh(&chan_irq->lock);

    if (chan_irq->ref <= 0) {
        ka_base_radix_tree_delete(&irq_tree, irq);
        trs_destroy_chan_irq(chan_irq);
    }
    ka_task_mutex_unlock(&irq_mutex);

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_free_irq);
