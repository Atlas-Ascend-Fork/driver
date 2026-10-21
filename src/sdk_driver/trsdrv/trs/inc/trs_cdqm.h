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

#ifndef CDQM_KERNEL_H
#define CDQM_KERNEL_H

#include "trs_pub_def.h"
#include "trs_msg.h"

#ifndef CFG_SOC_PLATFORM_MINIV3
#define MAX_CDQM_CDQ_NUM 128
#else
#define MAX_CDQM_CDQ_NUM 16
#endif

struct cdqm_adapt_ops {
    int (*send_sync_msg)(u32 devid, void *msg, size_t size);
    int (*ssid_query)(struct trs_id_inst *inst, int *user_visible_flag, int *ssid);
};
int cdqm_proc_sync_msg(u32 devid, struct trs_msg_data *msg);

int tsdrv_cdqm_set_topic_id(u32 devid, u32 topic_id);
u32 tsdrv_cdqm_get_instance_by_cdqid(u32 devid, u32 tsid, u32 cdq_id);
int tsdrv_cdqm_get_name_by_cdqid(u32 devid, u32 tsid, u32 cdq_id, char *name, int buf_len);

int cdqm_ts_inst_register(struct trs_id_inst *inst, struct cdqm_adapt_ops *ops);
int cdqm_ts_inst_unregister(struct trs_id_inst *inst);

bool cdqid_is_belong_to_proc(struct trs_id_inst *inst, ka_pid_t tgid, int res_type, u32 id);

#endif
