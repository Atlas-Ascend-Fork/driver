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
#ifndef TRS_CHAN_IRQ_H
#define TRS_CHAN_IRQ_H

#include "ka_type.h"

#include "ascend_kernel_hal.h"

#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_system_pub.h"

#include "trs_pub_def.h"

struct trs_chan_irq_attr {
    u32 group;
    void *para;

    int (*handler)(int irq_type, int irq_index, void *para, u32 cqid[], u32 cq_num);
    int (*get_valid_cq)(struct trs_id_inst *inst, u32 group, u32 cqid[], u32 cq_id_num, u32 *valid_cq_num);
    void (*intr_mask_config)(struct trs_id_inst *inst, u32 group, u32 irq, int val);
    int (*request_chan_irq)(struct trs_id_inst *inst, u32 irq_type, u32 irq, void *para,
                            ka_irqreturn_t (*handler)(int irq, void *para));
    void (*free_chan_irq)(struct trs_id_inst *inst, u32 irq_type, u32 irq, void *para);
};

#define MAX_PROC_CQ_NUM 2048
struct trs_chan_irq {
    struct trs_chan_irq_attr attr;
    int irq_index;
    u32 irq_type;
    u32 irq;
    int ref;
    struct trs_id_inst inst;
    ka_spinlock_t lock;
    ka_list_head_t head;
    u32 cqid_list[MAX_PROC_CQ_NUM];
    unsigned long timeout;
    ka_tasklet_struct_t task;
    ka_work_struct_t unmask_work;
};

int trs_chan_get_irq(struct trs_id_inst *inst, u32 irq_type, u32 irq[], u32 irq_num, u32 *valid_irq_num);
int trs_chan_request_irq(struct trs_id_inst *inst, int irq_type, int irq_index, struct trs_chan_irq_attr *attr);
int trs_chan_free_irq(struct trs_id_inst *inst, int irq_type, int irq_index, void *para);

int trs_chan_get_irq_by_index(struct trs_id_inst *inst, int irq_type, int irq_index, u32 *irq);
void trs_destroy_chan_irq(struct trs_chan_irq *chan_irq);
#endif
