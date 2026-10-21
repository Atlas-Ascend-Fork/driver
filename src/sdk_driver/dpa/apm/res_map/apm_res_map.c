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
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_compiler_pub.h"
#include "ka_dfx_pub.h"
#include "ka_ioctl_pub.h"

#include "apm_auto_init.h"
#include "pbl/pbl_uda.h"
#include "securec.h"

#include "apm_kernel_ioctl.h"
#include "apm_fops.h"
#include "apm_res_map_ops.h"
#include "apm_res_map.h"
#include "apm_slab.h"

static struct task_ctx_domain *res_map_domain = NULL;
static struct apm_task_res_map_ops *task_res_ops = NULL;

void apm_task_res_map_ops_register(struct apm_task_res_map_ops *ops)
{
    task_res_ops = ops;
}

int apm_fops_res_info_check(struct res_map_info_in *res_info)
{
    if (res_info == NULL) {
        apm_err("Null ptr.\n");
        return -EINVAL;
    }

    if ((res_info->res_type < 0) || (res_info->res_type >= RES_ADDR_TYPE_MAX)) {
        apm_err("Invalid para. (res_type=%d)\n", res_info->res_type);
        return -ERANGE;
    }

    if ((res_info->target_proc_type < 0) || (res_info->target_proc_type >= PROCESS_CPTYPE_MAX)) {
        apm_err("Invalid para. (target_proc_type=%d)\n", res_info->target_proc_type);
        return -ERANGE;
    }

    if (apm_res_use_svm_va(res_info->flag) && (res_info->res_type != RES_ADDR_TYPE_STARS_NOTIFY_RECORD)) {
        apm_err("Invalid para. (flag=0x%x, res_type=%d)\n", res_info->flag, res_info->res_type);
        return -EINVAL;
    }

    return 0;
}

bool apm_res_use_svm_va(unsigned int flag)
{
    return ((flag & RES_MAP_USE_UNIFIED_VA) != 0);
}

static int apm_res_map_get_map_tgid(u32 udevid, struct res_map_info_in *res_info, int *master_tgid, int *slave_tgid)
{
    int current_tgid = ka_task_get_current_tgid();
    int ret = 0;

    /* Check whether the map is called by the master. */
    *master_tgid = current_tgid;
    ret = hal_kernel_apm_query_slave_tgid_by_master(current_tgid, udevid, res_info->target_proc_type, slave_tgid);
    if (ret != 0) {
        u32 proc_type_bitmap, query_udevid;
        int mode;

        /* Check whether the map is called by the slave. */
        *slave_tgid = current_tgid;
        ret = apm_query_master_info_by_slave(*slave_tgid, master_tgid, &query_udevid, &mode, &proc_type_bitmap);
        if ((ret != 0) || (query_udevid != udevid)) {
            apm_err("Current is not apm task. (udevid=%u; q_udevid=%u; res_type=%u; res_id=%u; proc_type=%d)\n", udevid,
                    query_udevid, res_info->res_type, res_info->res_id, res_info->target_proc_type);
            return -ESRCH;
        }

        /* check whether the map is called by another slave. */
        if ((proc_type_bitmap & (0x1 << res_info->target_proc_type)) == 0) {
            ret = hal_kernel_apm_query_slave_tgid_by_master(*master_tgid, udevid, res_info->target_proc_type,
                                                            slave_tgid);
            if (ret != 0) {
                apm_err("Get slave tgid failed. (master_tgid=%d; udevid=%u; res_type=%u; res_id=%u; proc_type=%d)\n",
                        *master_tgid, udevid, res_info->res_type, res_info->res_id, res_info->target_proc_type);
                return -ESRCH;
            }
        }
    }

    return 0;
}

int apm_res_addr_map(u32 udevid, struct res_map_info_in *res_info, u64 *va, u32 *len)
{
    int ret, master_tgid, slave_tgid;
    int tgid = ka_task_get_current_tgid();
    struct apm_res_map_info para = {0};

    ret = apm_fops_res_info_check(res_info);
    if (ret != 0) {
        return ret;
    }

    ret = apm_res_map_get_map_tgid(udevid, res_info, &master_tgid, &slave_tgid);
    if (ret != 0) {
        apm_err("Get map tgids failed. (ret=%d; udevid=%u; res_type=%u; res_id=%u; proc_type=%d)\n", ret, udevid,
                res_info->res_type, res_info->res_id, res_info->target_proc_type);
        return ret;
    }

    /* check res perm */
    if (!apm_res_is_belong_to_proc(master_tgid, slave_tgid, udevid, res_info)) {
        apm_err("Current task does not have the resource. (udevid=%u; res_type=%u; res_id=%u; proc_type=%d)\n", udevid,
                res_info->res_type, res_info->res_id, res_info->target_proc_type);
        return -EPERM;
    }

    ret = apm_update_res_info(udevid, res_info);
    if (ret != 0) {
        return ret;
    }
    para.slave_tgid = slave_tgid;
    para.udevid = udevid;
    para.res_info = *res_info;
    para.va = *va;
    ret = task_res_ops->res_map(&para);
    if (ret != 0) {
        apm_err("Res map failed. (ret=%d; udevid=%u; res_type=%u; res_id=%u; proc_type=%d)\n", ret, udevid,
                res_info->res_type, res_info->res_id, res_info->target_proc_type);
        return ret;
    }

    ret = apm_res_map_add_node(res_map_domain, tgid, &para);
    if (ret != 0) {
        (void)task_res_ops->res_unmap(&para);
        apm_err("Add node failed. (ret=%d; udevid=%u; res_type=%u; res_id=%u; proc_type=%d)\n", ret, udevid,
                res_info->res_type, res_info->res_id, res_info->target_proc_type);
        return ret;
    }

    *va = (u64)para.va;
    *len = para.len;

    return 0;
}

int apm_res_addr_unmap(u32 udevid, struct res_map_info_in *res_info, unsigned long *user_va)
{
    int ret, tgid = ka_task_get_current_tgid();
    struct apm_res_map_info para = {0};

    ret = apm_fops_res_info_check(res_info);
    if (ret != 0) {
        return ret;
    }

    para.slave_tgid = 0;
    para.udevid = udevid;
    para.res_info = *res_info;
    para.va = 0;
    ret = apm_res_map_query_node(res_map_domain, tgid, &para);
    if (ret == 0) {
        (void)apm_res_map_del_node(res_map_domain, tgid, &para);
    }

    ret = task_res_ops->res_unmap(&para);
    if (ret != 0) {
#ifndef EMU_ST
        apm_info_ratelimited("Res unmap. (ret=%d; udevid=%u; res_type=%u; res_id=%u; proc_type=%d)\n", ret, udevid,
                             res_info->res_type, res_info->res_id, res_info->target_proc_type);
#endif
    }
    *user_va = para.va;

    return ret;
}

static int _apm_res_addr_query(u32 udevid, struct res_map_info_in *res_info, u64 *va, u32 *len)
{
    struct apm_res_map_info para;
    int ret, tgid = ka_task_get_current_tgid();

    para.udevid = udevid;
    para.res_info = *res_info;

    ret = apm_res_map_query_node(res_map_domain, tgid, &para);
    if (ret != 0) {
        ret = task_res_ops->res_map_query(&para);
        if (ret != 0) {
            apm_debug("Res not map. (ret=%d; tgid=%d; udevid=%u; res_type=%u; res_id=%u; proc_type=%d)\n", ret, tgid,
                      udevid, res_info->res_type, res_info->res_id, res_info->target_proc_type);
            return ret;
        }
    }

    *va = (u64)para.va;
    *len = para.len;

    return 0;
}

int apm_res_addr_query(u32 udevid, struct res_map_info_in *res_info, u64 *va, u32 *len)
{
    int ret;

    if ((va == NULL) || (len == NULL)) {
        apm_err("Null ptr.\n");
        return -EINVAL;
    }

    ret = apm_fops_res_info_check(res_info);
    if (ret != 0) {
        return ret;
    }

    return _apm_res_addr_query(udevid, res_info, va, len);
}

static bool apm_res_map_support_priv(struct res_map_info_in *res_info)
{
    if ((res_info->res_type == RES_ADDR_TYPE_HCCP_URMA_JETTY) || (res_info->res_type == RES_ADDR_TYPE_HCCP_URMA_JFC) ||
        (res_info->res_type == RES_ADDR_TYPE_HCCP_URMA_DB) || (res_info->res_type == RES_ADDR_TYPE_NDA_URMA_DB) ||
        (res_info->res_type == RES_ADDR_TYPE_STARS_NOTIFY_RECORD)) {
        return true;
    }
    return false;
}

/* priv need use heap mem in scene when the res mapped in kthread context */
static int apm_init_res_map_info_priv(struct res_map_info_in *res_info)
{
    char priv_tmp_buffer[APM_RES_MAP_INFO_PRIV_LEN_MAX] = {0};
    void *priv_buffer = NULL;
    int ret = 0;

    if (((res_info->res_type == RES_ADDR_TYPE_HCCP_URMA_JETTY) || (res_info->res_type == RES_ADDR_TYPE_HCCP_URMA_JFC) ||
         (res_info->res_type == RES_ADDR_TYPE_HCCP_URMA_DB) || (res_info->res_type == RES_ADDR_TYPE_NDA_URMA_DB)) &&
        ((res_info->priv == NULL) || (res_info->priv_len == 0) ||
         (res_info->priv_len > APM_RES_MAP_INFO_PRIV_LEN_MAX))) {
        apm_err("Invalid para. (res_type=%d)\n", res_info->res_type);
        return -EINVAL;
    }

    if (res_info->priv == NULL) {
        return 0;
    }

    if (!apm_res_map_support_priv(res_info)) {
        return -EOPNOTSUPP;
    }

    if ((res_info->priv != NULL) &&
        ((res_info->priv_len == 0) || (res_info->priv_len > APM_RES_MAP_INFO_PRIV_LEN_MAX))) {
        apm_err("Invalid para. (priv_len=%u)\n", res_info->priv_len);
        return -EINVAL;
    }

    ret = (int)ka_base_copy_from_user(priv_tmp_buffer, res_info->priv, res_info->priv_len);
    if (ret != 0) {
        apm_err("Copy from user failed. (ret=%d)\n", ret);
        return -EINVAL;
    }

    priv_buffer = apm_kmalloc(res_info->priv_len, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (priv_buffer == NULL) {
        apm_err("Kmalloc priv buffer failed. (len=0x%x)\n", res_info->priv_len);
        return -ENOMEM;
    }

    ret = memcpy_s(priv_buffer, res_info->priv_len, (void *)priv_tmp_buffer, res_info->priv_len);
    if (ret != 0) {
        apm_kfree(priv_buffer);
        return -EINVAL;
    }

    res_info->priv = priv_buffer;
    return 0;
}

static void apm_uninit_res_map_info_priv(struct res_map_info_in *res_info)
{
    if (res_info->priv != NULL) {
        apm_kfree(res_info->priv);
        res_info->priv = NULL;
    }
}

static int apm_fops_res_map(u32 cmd, unsigned long arg)
{
    struct apm_cmd_res_map *usr_arg = (struct apm_cmd_res_map __ka_user *)(uintptr_t)arg;
    struct apm_cmd_res_map para;
    int ret;

    ret = (int)ka_base_copy_from_user(&para, usr_arg, sizeof(para));
    if (ret != 0) {
        apm_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = uda_devid_to_udevid(para.devid, &para.devid);
    if (ret != 0) {
        return -ENODEV;
    }

    ret = apm_init_res_map_info_priv(&para.res_info);
    if (ret != 0) {
        return ret;
    }

    ret = apm_res_addr_map(para.devid, &para.res_info, (u64 *)&para.va, &para.len);
    if (ret != 0) {
        apm_uninit_res_map_info_priv(&para.res_info);
        return ret;
    }

    apm_uninit_res_map_info_priv(&para.res_info);
    return (int)ka_base_copy_to_user(usr_arg, &para, sizeof(para));
}

static int apm_fops_res_unmap(u32 cmd, unsigned long arg)
{
    struct apm_cmd_res_unmap *usr_arg = (struct apm_cmd_res_unmap __ka_user *)(uintptr_t)arg;
    struct apm_cmd_res_unmap para;
    int ret;

    ret = (int)ka_base_copy_from_user(&para, usr_arg, sizeof(para));
    if (ret != 0) {
        apm_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = uda_devid_to_udevid(para.devid, &para.devid);
    if (ret != 0) {
        return -ENODEV;
    }

    ret = apm_res_addr_unmap(para.devid, &para.res_info, &para.va);
    if (ret != 0) {
        return ret;
    }

    return (int)ka_base_copy_to_user(usr_arg, &para, sizeof(para));
}

void apm_res_map_domain_task_exit(u32 udevid, int tgid, struct task_start_time *start_time)
{
    if (!apm_notify_task_exit(tgid, start_time)) {
        apm_res_map_ctx_destroy(res_map_domain, tgid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_TASK(apm_res_map_domain_task_exit, FEATURE_LOADER_STAGE_6);

static int apm_res_map_domain_task_exit_notifier(ka_notifier_block_t *self, unsigned long val, void *data)
{
    apm_debug("Res map exit notifier. (tgid=%d; stage=%d)\n", apm_get_exit_tgid(val), apm_get_exit_stage(val));

    if (apm_get_exit_stage(val) == APM_STAGE_DO_EXIT) {
        apm_res_map_ctx_destroy(res_map_domain, apm_get_exit_tgid(val));
    }

    return KA_NOTIFY_OK;
}

static ka_notifier_block_t apm_res_map_master_task_exit_nb = {
    .notifier_call = apm_res_map_domain_task_exit_notifier,
    .priority = APM_EXIT_NOTIFIY_PRI_APM_RES,
};

static ka_notifier_block_t apm_res_map_slave_task_exit_nb = {
    .notifier_call = apm_res_map_domain_task_exit_notifier,
    .priority = APM_EXIT_NOTIFIY_PRI_APM_RES,
};

void apm_res_map_domain_task_show(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    if (tgid != 0) {
        apm_res_map_ctx_show(res_map_domain, tgid, seq);
    } else {
        task_ctx_domain_show(res_map_domain, seq);
    }
}
DECLAER_FEATURE_AUTO_SHOW_TASK(apm_res_map_domain_task_show, FEATURE_LOADER_STAGE_6);

int apm_res_map_init(void)
{
    res_map_domain = task_ctx_domain_create("res_map_domain", 0);
    if (res_map_domain == NULL) {
        return -ENOMEM;
    }

    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_RES_ADDR_MAP), apm_fops_res_map);
    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_RES_ADDR_UNMAP), apm_fops_res_unmap);

    /* support master and slave task call res map in user space */
    (void)apm_master_exit_register(&apm_res_map_master_task_exit_nb);
    (void)apm_slave_exit_register(&apm_res_map_slave_task_exit_nb);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(apm_res_map_init, FEATURE_LOADER_STAGE_6);

void apm_res_map_uninit(void)
{
    apm_master_exit_unregister(&apm_res_map_master_task_exit_nb);
    apm_slave_exit_unregister(&apm_res_map_slave_task_exit_nb);

    task_ctx_domain_destroy(res_map_domain);
    res_map_domain = NULL;
}
DECLAER_FEATURE_AUTO_UNINIT(apm_res_map_uninit, FEATURE_LOADER_STAGE_6);
