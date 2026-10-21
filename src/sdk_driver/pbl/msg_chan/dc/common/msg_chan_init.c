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

#include "msg_chan_main.h"
#include "msg_chan_init.h"
#include "pbl/pbl_feature_loader.h"

int devdrv_base_comm_init(void)
{
    int ret, i;
    struct devdrv_msg_client *client_info = devdrv_get_msg_client();
    struct devdrv_comm_dev_ops *comm_ops = devdrv_get_comm_ops();
    struct devdrv_faultmgr_dev_ops *faultmgr_ops = devdrv_get_faultmgr_ops();
    struct devdrv_serdes_dump_dev_ops *serdes_dump_ops = devdrv_get_serdes_dump_ops();

    (void)memset_s(comm_ops, sizeof(struct devdrv_comm_dev_ops) * DEVDRV_COMMNS_TYPE_MAX, 0,
                   sizeof(struct devdrv_comm_dev_ops) * DEVDRV_COMMNS_TYPE_MAX);
    (void)memset_s(client_info, sizeof(struct devdrv_msg_client), 0, sizeof(struct devdrv_msg_client));
    ka_task_mutex_init(&client_info->lock);
    for (i = 0; i < DEVDRV_COMMNS_TYPE_MAX; ++i) {
        comm_ops[i].status = DEVDRV_COMM_OPS_TYPE_UNINIT;
        ka_base_atomic_set(&comm_ops[i].ops.ref_cnt, 0);
        ka_base_atomic_set(&comm_ops[i].dev_cnt, 0);
        ka_task_rwlock_init(&comm_ops[i].rwlock);
    }

    (void)memset_s(faultmgr_ops, sizeof(struct devdrv_faultmgr_dev_ops), 0, sizeof(struct devdrv_faultmgr_dev_ops));
    faultmgr_ops->status = DEVDRV_COMM_OPS_TYPE_UNINIT;
    ka_base_atomic_set(&faultmgr_ops->ops.ref_cnt, 0);
    ka_base_atomic_set(&faultmgr_ops->dev_cnt, 0);
    ka_task_rwlock_init(&faultmgr_ops->rwlock);

    (void)memset_s(serdes_dump_ops, sizeof(struct devdrv_serdes_dump_dev_ops), 0,
                   sizeof(struct devdrv_serdes_dump_dev_ops));
    serdes_dump_ops->status = DEVDRV_COMM_OPS_TYPE_UNINIT;
    ka_base_atomic_set(&serdes_dump_ops->ops.ref_cnt, 0);
    ka_base_atomic_set(&serdes_dump_ops->dev_cnt, 0);
    ka_task_rwlock_init(&serdes_dump_ops->rwlock);

#ifndef EMU_ST
    ret = module_feature_auto_init();
    if (ret != 0) {
        ka_task_mutex_destroy(&client_info->lock);
        devdrv_err("Module feature init failed.\n");
        return ret;
    }
#endif
    return 0;
}

void devdrv_base_comm_exit(void)
{
    int i;
    struct devdrv_msg_client *client_info = devdrv_get_msg_client();
    struct devdrv_comm_dev_ops *comm_ops = devdrv_get_comm_ops();
    struct devdrv_faultmgr_dev_ops *faultmgr_ops = devdrv_get_faultmgr_ops();
    struct devdrv_serdes_dump_dev_ops *serdes_dump_ops = devdrv_get_serdes_dump_ops();

#ifndef EMU_ST
    module_feature_auto_uninit();
#endif

    faultmgr_ops->status = DEVDRV_COMM_OPS_TYPE_DISABLE;
    ka_base_atomic_set(&faultmgr_ops->ops.ref_cnt, 0);
    ka_base_atomic_set(&faultmgr_ops->dev_cnt, 0);
    (void)memset_s(faultmgr_ops, sizeof(struct devdrv_faultmgr_dev_ops), 0, sizeof(struct devdrv_faultmgr_dev_ops));

    serdes_dump_ops->status = DEVDRV_COMM_OPS_TYPE_DISABLE;
    ka_base_atomic_set(&serdes_dump_ops->ops.ref_cnt, 0);
    ka_base_atomic_set(&serdes_dump_ops->dev_cnt, 0);
    (void)memset_s(serdes_dump_ops, sizeof(struct devdrv_serdes_dump_dev_ops), 0,
                   sizeof(struct devdrv_serdes_dump_dev_ops));

    for (i = 0; i < DEVDRV_COMMNS_TYPE_MAX; ++i) {
        comm_ops[i].status = DEVDRV_COMM_OPS_TYPE_DISABLE;
        ka_base_atomic_set(&comm_ops[i].ops.ref_cnt, 0);
        ka_base_atomic_set(&comm_ops[i].dev_cnt, 0);
    }
    ka_task_mutex_destroy(&client_info->lock);
    (void)memset_s(client_info, sizeof(struct devdrv_msg_client), 0, sizeof(struct devdrv_msg_client));
    (void)memset_s(comm_ops, sizeof(struct devdrv_comm_dev_ops) * DEVDRV_COMMNS_TYPE_MAX, 0,
                   sizeof(struct devdrv_comm_dev_ops) * DEVDRV_COMMNS_TYPE_MAX);
    return;
}