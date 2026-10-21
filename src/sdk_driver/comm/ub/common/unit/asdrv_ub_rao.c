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

#include "asdrv_ub_main.h"
#include "asdrv_ub_non_trans_chan.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_rao.h"
#include "ka_compiler_pub.h"

struct ubdrv_rao_client_ctrl (*g_rao_client_ctrl)[DEVDRV_RAO_CLIENT_MAX] = NULL;

rao_client_ctrl_arr_ptr get_global_rao_client_ctrl(void)
{
    return g_rao_client_ctrl;
}

int ubdrv_rao_client_ctrl_init(void)
{
    int i, j;

    g_rao_client_ctrl = ubdrv_kzalloc(
        sizeof(struct ubdrv_rao_client_ctrl) * ASCEND_UB_DEV_MAX_NUM * DEVDRV_RAO_CLIENT_MAX, KA_GFP_KERNEL);
    if (g_rao_client_ctrl == NULL) {
        ubdrv_err("Alloc rao client ctrl failed.\n");
        return -ENOMEM;
    }

    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; ++i) {
        for (j = 0; j < DEVDRV_RAO_CLIENT_MAX; ++j) {
            ka_task_mutex_init(&g_rao_client_ctrl[i][j].mutex_lock);
            g_rao_client_ctrl[i][j].status = UBDRV_RAO_CLIENT_DISABLE;
        }
    }
    return 0;
}

void ubdrv_rao_client_ctrl_uninit(void)
{
    int i, j;

    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; ++i) {
        for (j = 0; j < DEVDRV_RAO_CLIENT_MAX; ++j) {
            ka_task_mutex_destroy(&g_rao_client_ctrl[i][j].mutex_lock);
            g_rao_client_ctrl[i][j].status = UBDRV_RAO_CLIENT_DISABLE;
        }
    }
    ubdrv_kfree(g_rao_client_ctrl);
    g_rao_client_ctrl = NULL;
}

int ubdrv_register_rao_para_check(u32 dev_id, enum devdrv_rao_client_type type, u64 va, u64 len,
                                  enum devdrv_rao_permission_type perm)
{
    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id, get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (type >= DEVDRV_RAO_CLIENT_MAX) {
        ubdrv_err("Invalid client type. (dev_id=%u; type=%d)\n", dev_id, type);
        return -EINVAL;
    }
    if ((va == 0) || (len == 0)) {
        ubdrv_err("Invalid va or len. (dev_id=%u; type=%d; len=0x%llx)\n", dev_id, type, len);
        return -EINVAL;
    }
    if (perm != DEVDRV_RAO_PERM_RMT_READ) {
        ubdrv_err("Invalid perm type. (dev_id=%u; type=%d; perm=%d)\n", dev_id, type, perm);
        return -EINVAL;
    }
    return 0;
}
