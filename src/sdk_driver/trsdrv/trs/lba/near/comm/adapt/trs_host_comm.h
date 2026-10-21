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
#ifndef TRS_HOST_COMM_H
#define TRS_HOST_COMM_H

#include "ka_base_pub.h"
#include "trs_pub_def.h"
#include "trs_msg.h"

#define ALIGN_UP(len, pagesize) (((len) + (pagesize) - 1) & (~((pagesize) - 1)))

struct trs_adapt_irq_attr {
    u32 irq_type;
    u32 irq;
};

int trs_host_get_sq_mem_handle(struct trs_id_inst *inst, int pid, struct trs_sq_mem_handle *handle);
void trs_host_put_sq_mem_handle(struct trs_id_inst *inst, int pid, struct trs_sq_mem_handle *handle);
int trs_host_get_ssid(struct trs_id_inst *inst, int *user_visible_flag, int *ssid);
int trs_host_ts_adapt_abnormal_proc(u32 devid, struct trs_msg_data *msg);
int trs_host_set_ts_status(u32 devid, struct trs_msg_data *data);
int trs_host_flush_id(u32 devid, struct trs_msg_data *data);
int trs_host_res_is_check_msg_proc(u32 devid, struct trs_msg_data *data);
int trs_host_res_id_check(struct trs_id_inst *inst, int id_type, u32 res_id);
int trs_host_res_id_get(struct trs_id_inst *inst, int id_type, u32 res_id, u32 hpid, u32 seq);
int trs_host_res_id_put(struct trs_id_inst *inst, int id_type, u32 res_id, u32 hpid, u32 seq);
int trs_host_res_num_query(struct trs_id_inst *inst, struct trs_res_query_para *para);
int trs_host_request_irq(struct trs_id_inst *inst, struct trs_adapt_irq_attr *attr, void *para, const char *name,
                         ka_irqreturn_t (*handler)(int irq, void *para));
void trs_host_free_irq(struct trs_id_inst *inst, struct trs_adapt_irq_attr *attr, void *para);
int trs_adapt_ops_request_irq(struct trs_id_inst *inst, u32 irq_type, u32 irq, void *para,
                              ka_irqreturn_t (*handler)(int irq, void *para));
void trs_adapt_ops_free_irq(struct trs_id_inst *inst, u32 irq_type, u32 irq, void *para);
int trs_host_ts_cq_process(u32 devid, struct trs_msg_data *data);
int trs_host_get_connect_protocol(struct trs_id_inst *inst);
int trs_get_host_mach_flag(struct trs_id_inst *inst, u32 *host_flag);
int trs_host_ras_report(struct trs_id_inst *inst);
int trs_host_mem_sharing_register(void);
void trs_host_mem_sharing_unregister(void);
#endif /* TRS_HOST_COMM_H */
