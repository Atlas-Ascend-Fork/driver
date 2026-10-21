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

#include "apm_auto_init.h"

#include "dpa/dpa_apm_kernel.h"
#include "apm_res_map.h"
#include "apm_msg.h"
#include "apm_res_map_host.h"

#include "securec.h"

static int apm_res_map_host_res_map_unmap(enum apm_msg_type msg_type, struct apm_res_map_info *para)
{
    struct apm_msg_map_unmap msg = {0};
    int ret;

    apm_msg_fill_header(&msg.header, msg_type);
    msg.para = *para;

    if ((msg_type == APM_MSG_TYPE_MAP) && (para->res_info.priv != NULL)) {
        ret = memcpy_s(msg.res_map_priv, APM_RES_MAP_INFO_PRIV_LEN_MAX, para->res_info.priv, para->res_info.priv_len);
        if (ret != 0) {
            return -EINVAL;
        }
    }

    ret = apm_msg_send(para->udevid, &msg.header, sizeof(msg));
    if ((ret == 0) && ((msg_type == APM_MSG_TYPE_MAP) || (msg_type == APM_MSG_TYPE_MAP_QUERY))) {
        para->va = msg.para.va;
        para->pa = msg.para.pa;
        para->len = msg.para.len;
    }

    return ret;
}

int apm_res_map_host_res_map(struct apm_res_map_info *para)
{
    return apm_res_map_host_res_map_unmap(APM_MSG_TYPE_MAP, para);
}

int apm_res_map_host_res_unmap(struct apm_res_map_info *para)
{
    /* map from device slave task */
    if (para->slave_tgid == 0) {
        struct res_map_info_in *res_info = &para->res_info;
        int tgid = ka_task_get_current_tgid();
        int ret = hal_kernel_apm_query_slave_tgid_by_master(tgid, para->udevid, res_info->target_proc_type,
                                                            &para->slave_tgid);
        if (ret != 0) {
            apm_err("Query slave tgid failed. (udevid=%u; res_type=%u; res_id=%u; proc_type=%d)\n", para->udevid,
                    res_info->res_type, res_info->res_id, res_info->target_proc_type);
            return ret;
        }
    }

    return apm_res_map_host_res_map_unmap(APM_MSG_TYPE_UNMAP, para);
}

static int apm_res_map_host_res_map_query(struct apm_res_map_info *para)
{
    struct res_map_info_in *res_info = &para->res_info;
    int tgid = ka_task_get_current_tgid();
    int ret = hal_kernel_apm_query_slave_tgid_by_master(tgid, para->udevid, res_info->target_proc_type,
                                                        &para->slave_tgid);
    if (ret != 0) {
        apm_debug("Query slave tgid failed. (udevid=%u; res_type=%u; res_id=%u; proc_type=%d)\n", para->udevid,
                  res_info->res_type, res_info->res_id, res_info->target_proc_type);
        return ret;
    }

    return apm_res_map_host_res_map_unmap(APM_MSG_TYPE_MAP_QUERY, para);
}

int hal_kernel_apm_vma_check_register(enum drv_module_type type, vma_check_handle func)
{
    (void)type;
    (void)func;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_apm_vma_check_register);

void hal_kernel_apm_vma_check_unregister(enum drv_module_type type)
{
    (void)type;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_apm_vma_check_unregister);

static struct apm_task_res_map_ops host_task_res_map_ops = {
    .res_map = apm_res_map_host_res_map,
    .res_unmap = apm_res_map_host_res_unmap,
    .res_map_query = apm_res_map_host_res_map_query,
};

int apm_res_map_host_init(void)
{
    apm_task_res_map_ops_register(&host_task_res_map_ops);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(apm_res_map_host_init, FEATURE_LOADER_STAGE_7);

void apm_res_map_host_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(apm_res_map_host_uninit, FEATURE_LOADER_STAGE_7);
