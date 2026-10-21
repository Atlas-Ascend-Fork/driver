/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#ifndef APM_TASK_GROUP_DEF_H
#define APM_TASK_GROUP_DEF_H

#include "apm_auto_init.h"
#include "pbl/pbl_task_ctx.h"
#include "pbl/pbl_uda.h"

#include "dpa/dpa_apm_kernel.h"
#include "apm_fops.h"
#include "apm_proc_fs.h"
#include "apm_kernel_msg.h"

#define APM_ONLINE_MODE 0
#define APM_OFFLINE_MODE 1
#define APM_MODE_NUM 2
#define APM_PROC_TYPE_NUM PROCESS_CPTYPE_MAX

#define APM_OP_BIND 0
#define APM_OP_UNBIND 1

static inline void apm_fill_task_group_cfg(struct apm_task_group_cfg *cfg, int master_tgid, int slave_tgid,
                                           struct apm_cmd_bind *para)
{
    cfg->master_tgid = master_tgid;
    cfg->slave_tgid = slave_tgid;
    cfg->para = *para;
}

static const char *proc_type_name[APM_PROC_TYPE_NUM] = {
    [PROCESS_CP1] = "aicpu",          [PROCESS_CP2] = "aicpu custom", [PROCESS_DEV_ONLY] = "tdt",
    [PROCESS_QS] = "queue scheduler", [PROCESS_HCCP] = "hccp",        [PROCESS_USER] = "user"};

static inline const char *apm_proc_type_to_name(int type)
{
    if ((type >= PROCESS_CP1) && (type < APM_PROC_TYPE_NUM)) {
        return proc_type_name[type];
    }
    return "UnknownId";
}

static inline bool apm_is_surport_multi_slave(int proc_type)
{
    return (proc_type == PROCESS_USER);
}

static inline int apm_devid_to_udevid(u32 devid, u32 *udevid)
{
    int ret;

    if (devid == UDA_INVALID_UDEVID) {
        *udevid = UDA_INVALID_UDEVID;
        return 0;
    }

    if (devid >= APM_LOGIC_DEV_MAX_NUM) {
        return -ENODEV;
    }

    ret = uda_devid_to_udevid(devid, udevid);
    if (ret != 0) {
        return -ENODEV;
    }

    return 0;
}

#endif
