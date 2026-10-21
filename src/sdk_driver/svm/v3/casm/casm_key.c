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
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_common_pub.h"
#include "ka_fs_pub.h"
#include "ka_compiler_pub.h"
#include "ka_ioctl_pub.h"

#include "pbl_feature_loader.h"
#include "dpa_kernel_interface.h"
#include "pbl_uda.h"

#include "svm_kern_log.h"
#include "framework_dev.h"
#include "framework_cmd.h"
#include "svm_ioctl_ex.h"
#include "svm_smp.h"
#include "svm_slab.h"
#include "dbi_kern.h"
#include "mwl.h"
#include "casm_ctx.h"
#include "casm_src.h"
#include "casm_key.h"

#define KEY_SHOW_NODE_MAX 100000

struct casm_key_ctx {
    u32 udevid;
    void *dev_ctx;
    ka_mutex_t mutex;
    ka_idr_t idr;
    u32 id_num;
};

static u32 casm_dev_feature_id;
static struct svm_casm_key_ops *casm_key_ops = NULL;

void svm_casm_register_key_ops(const struct svm_casm_key_ops *ops)
{
    casm_key_ops = (struct svm_casm_key_ops *)ops;
}

static int casm_key_update_handle(u64 *key)
{
    if (casm_key_ops != NULL) {
        return casm_key_ops->update_key(key);
    } else {
        return 0;
    }
}

static int casm_key_parse_handle(u64 key, u32 *udevid, u32 *id)
{
    if (casm_key_ops != NULL) {
        return casm_key_ops->parse_key(key, udevid, id);
    } else {
        return -EOPNOTSUPP;
    }
}

static bool casm_key_is_local_handle(u64 key)
{
    if (casm_key_ops != NULL) {
        return casm_key_ops->is_local_key(key);
    } else {
        return true;
    }
}

static int casm_udevid_and_id_to_key(u32 udevid, u32 id, u64 *key)
{
    *key = casm_make_key(udevid, id);
    return casm_key_update_handle(key);
}

static int casm_key_to_udevid_and_id(u64 key, u32 *udevid, u32 *id)
{
    int ret = casm_key_parse_handle(key, udevid, id);
    if (ret == -EOPNOTSUPP) {
        *id = casm_key_to_id(key);
        *udevid = casm_key_to_udevid(key);
        ret = 0;
    }
    return ret;
}

bool casm_is_local_key(u64 key)
{
    return casm_key_is_local_handle(key);
}

static struct casm_key_ctx *casm_key_ctx_get(u32 udevid)
{
    struct casm_key_ctx *key_ctx = NULL;
    void *dev_ctx = NULL;

    dev_ctx = svm_dev_ctx_get(udevid);
    if (dev_ctx == NULL) {
        return NULL;
    }

    key_ctx = (struct casm_key_ctx *)svm_dev_get_feature_priv(dev_ctx, casm_dev_feature_id);
    if (key_ctx == NULL) {
        svm_dev_ctx_put(dev_ctx);
    }

    return key_ctx;
}

static void casm_key_ctx_put(struct casm_key_ctx *key_ctx)
{
    svm_dev_ctx_put(key_ctx->dev_ctx);
}

static int casm_key_id_alloc(struct casm_key_ctx *key_ctx, struct casm_src_node *src_node, u32 *id)
{
    int ret;

    ka_base_idr_lock(&key_ctx->idr);
    ret = ka_base_idr_alloc_cyclic(&key_ctx->idr, src_node, 0, -1, KA_GFP_ATOMIC); /* -1 use INT_MAX */
    if (ret >= 0) {
        key_ctx->id_num++;
    }
    ka_base_idr_unlock(&key_ctx->idr);

    if (ret < 0) {
        return ret;
    }

    *id = (u32)ret;
    return 0;
}

static struct casm_src_node *casm_key_id_free(struct casm_key_ctx *key_ctx, u32 id)
{
    struct casm_src_node *src_node = NULL;

    ka_base_idr_lock(&key_ctx->idr);
    src_node = ka_base_idr_remove(&key_ctx->idr, (unsigned long)id);
    if ((src_node != NULL) && (key_ctx->id_num > 0)) {
        key_ctx->id_num--;
    }
    ka_base_idr_unlock(&key_ctx->idr);

    return src_node;
}

static struct casm_src_node *casm_key_id_find(struct casm_key_ctx *key_ctx, u32 id)
{
    struct casm_src_node *src_node = NULL;

    ka_base_idr_lock(&key_ctx->idr);
    src_node = ka_base_idr_find(&key_ctx->idr, (unsigned long)id);
    ka_base_idr_unlock(&key_ctx->idr);

    return src_node;
}

static void casm_key_ctx_init(struct casm_key_ctx *key_ctx)
{
    ka_task_mutex_init(&key_ctx->mutex);
    ka_base_idr_init(&key_ctx->idr);
    key_ctx->id_num = 0;
}

static void casm_key_ctx_uninit(struct casm_key_ctx *key_ctx)
{
    ka_base_idr_destroy(&key_ctx->idr);
    ka_task_mutex_destroy(&key_ctx->mutex);
}

static int _casm_create_key(struct casm_key_ctx *key_ctx, struct svm_global_va *src_va,
                            const struct casm_src_prop *src_prop, u64 *key)
{
    struct casm_src_node *src_node = NULL;
    u32 udevid = key_ctx->udevid;
    int ret, tgid;
    u32 id;

    tgid = ka_task_get_current_tgid();

    src_node = (struct casm_src_node *)svm_kzalloc(sizeof(*src_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (src_node == NULL) {
        svm_err("No mem. (udevid=%u; va=0x%llx)\n", udevid, src_va->va);
        return -ENOMEM;
    }

    ret = casm_key_id_alloc(key_ctx, src_node, &id);
    if (ret != 0) {
        svm_kfree(src_node);
        svm_err("Alloc id failed. (udevid=%u; va=0x%llx)\n", udevid, src_va->va);
        return -EINVAL;
    }

    ret = casm_udevid_and_id_to_key(udevid, id, key);
    if (ret != 0) {
        (void)casm_key_id_free(key_ctx, id);
        svm_kfree(src_node);
        return -EINVAL;
    }

    src_node->key = *key;
    src_node->src_va = *src_va;
    src_node->src_ex.src_prop = *src_prop;

    ret = casm_add_src_node(udevid, tgid, src_node);
    if (ret != 0) {
        (void)casm_key_id_free(key_ctx, id);
        svm_kfree(src_node);
        svm_err("Alloc id failed. (udevid=%u; va=0x%llx)\n", udevid, src_va->va);
        return -EINVAL;
    }

    ret = svm_mwl_add_mem(udevid, tgid, *key, src_va->va, src_va->size);
    if (ret != 0) {
        casm_del_src_node(udevid, tgid, src_node);
        (void)casm_key_id_free(key_ctx, id);
        svm_kfree(src_node);
        svm_err("Add white list failed. (udevid=%u; va=0x%llx)\n", udevid, src_va->va);
        return ret;
    }

    return 0;
}

static int casm_destroy_permission_check(struct casm_key_ctx *key_ctx, u32 id, bool force)
{
    struct casm_src_node *src_node = NULL;

    if (force == false) {
        src_node = casm_key_id_find(key_ctx, id);
        if (src_node == NULL) {
            svm_err("Invalid key. (udevid=%u; id=%u)\n", key_ctx->udevid, id);
            return -EINVAL;
        }

        if (ka_unlikely(ka_task_get_current_tgid() != src_node->src_ex.owner_tgid)) {
            svm_err("Non-creator has no permission to destroy key. (udevid=%u; id=%d; owner_tgid=%d)\n",
                    key_ctx->udevid, id, src_node->src_ex.owner_tgid);
            return -EPERM;
        }
    }

    return 0;
}

static int _casm_destroy_key(struct casm_key_ctx *key_ctx, u32 id, bool force)
{
    struct casm_src_node *src_node = NULL;
    u32 udevid = key_ctx->udevid;
    int ret;

    ret = casm_destroy_permission_check(key_ctx, id, force);
    if (ret != 0) {
        return ret;
    }

    src_node = casm_key_id_free(key_ctx, id);
    if (src_node == NULL) {
        svm_err("Invalid key. (udevid=%u; id=%u)\n", udevid, id);
        return -EINVAL;
    }

    /* id is legal, do not need va and va_size */
    ret = svm_mwl_del_mem(udevid, src_node->src_ex.owner_tgid, src_node->key, 0, 0);
    if (ret != 0) {
        svm_warn("Del white list failed. (udevid=%u; id=%u)\n", udevid, id);
    }

    casm_del_src_node(udevid, src_node->src_ex.owner_tgid, src_node);
    svm_kfree(src_node);

    return 0;
}

static int casm_create_key(struct svm_global_va *src_va, const struct casm_src_prop *src_prop, u64 *key)
{
    struct casm_key_ctx *key_ctx = NULL;
    u32 udevid = src_va->udevid;
    int ret;

    key_ctx = casm_key_ctx_get(udevid);
    if (key_ctx == NULL) {
        svm_err("Get key ctx failed. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&key_ctx->mutex);
    ret = _casm_create_key(key_ctx, src_va, src_prop, key);
    ka_task_mutex_unlock(&key_ctx->mutex);
    casm_key_ctx_put(key_ctx);

    return ret;
}

int casm_destroy_key(u64 key, bool force)
{
    struct casm_key_ctx *key_ctx = NULL;
    u32 udevid, id;
    int ret;

    ret = casm_key_to_udevid_and_id(key, &udevid, &id);
    if (ret != 0) {
        svm_err("Parse key failed. (key=0x%llx)\n", key);
        return ret;
    }

    key_ctx = casm_key_ctx_get(udevid);
    if (key_ctx == NULL) {
        svm_err("Get key ctx failed. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&key_ctx->mutex);
    ret = _casm_destroy_key(key_ctx, id, force);
    ka_task_mutex_unlock(&key_ctx->mutex);
    casm_key_ctx_put(key_ctx);

    return ret;
}

static int casm_check_task(u32 udevid, int tgid, u64 id, u32 checked_server_id, int checked_tgid)
{
    return svm_mwl_task_is_trusted(udevid, tgid, id, checked_server_id, checked_tgid) ? 0 : -ENOSPC;
}

static int _casm_op_task(struct casm_key_ctx *key_ctx, u32 id, u32 server_id, int tgid, u32 op)
{
    static int (*casm_op_task_func[SVM_CASM_TASK_OP_MAX])(u32 udevid, int tgid, u64 id, u32 op_server_id,
                                                          int op_tgid) = {
        [SVM_CASM_TASK_OP_ADD] = svm_mwl_add_trusted_task,
        [SVM_CASM_TASK_OP_DEL] = svm_mwl_del_trusted_task,
        [SVM_CASM_TASK_OP_CHECK] = casm_check_task};
    struct casm_src_node *src_node = NULL;
    u32 udevid = key_ctx->udevid;
    int ret;

    if ((op >= SVM_CASM_TASK_OP_MAX) || (casm_op_task_func[op] == NULL)) {
        svm_debug("Not support op. (op=%u)\n", op);
        return -EOPNOTSUPP;
    }

    src_node = casm_key_id_find(key_ctx, id);
    if (src_node == NULL) {
        svm_err("Invalid key. (udevid=%u; id=%u)\n", udevid, id);
        return -EINVAL;
    }

    ret = casm_op_task_func[op](udevid, src_node->src_ex.owner_tgid, src_node->key, server_id, tgid);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

static int casm_op_task(u64 key, u32 server_id, int tgid, u32 op)
{
    struct casm_key_ctx *key_ctx = NULL;
    u32 udevid, id;
    int ret;

    ret = casm_key_to_udevid_and_id(key, &udevid, &id);
    if (ret != 0) {
        svm_err("Parse key failed. (key=0x%llx)\n", key);
        return ret;
    }

    key_ctx = casm_key_ctx_get(udevid);
    if (key_ctx == NULL) {
        svm_err("Get key ctx failed. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ka_task_mutex_lock(&key_ctx->mutex);
    ret = _casm_op_task(key_ctx, key, server_id, tgid, op);
    ka_task_mutex_unlock(&key_ctx->mutex);
    casm_key_ctx_put(key_ctx);

    return ret;
}

static int _casm_get_src_va(struct casm_key_ctx *key_ctx, u32 id, u64 key, struct svm_global_va *src_va,
                            struct casm_src_ex *src_ex)
{
    struct casm_src_node *src_node = NULL;
    u32 udevid = key_ctx->udevid;

    src_node = casm_key_id_find(key_ctx, id);
    if ((src_node == NULL) || (key != src_node->key)) {
        svm_err("Invalid key. (udevid=%u; id=%u; key=0x%llx)\n", udevid, id, key);
        /* -EFAULT is mapped to DRV_ERROR_INVALID_HANDLE for the upper-layer errmsg. */
        return -EFAULT;
    }

    *src_ex = src_node->src_ex;
    *src_va = src_node->src_va;
    return 0;
}

int casm_get_src_va(u64 key, struct svm_global_va *src_va, struct casm_src_ex *src_ex)
{
    struct casm_key_ctx *key_ctx = NULL;
    u32 udevid, id;
    int ret;

    ret = casm_key_to_udevid_and_id(key, &udevid, &id);
    if (ret != 0) {
        svm_err("Parse key failed. (key=0x%llx)\n", key);
        /* -EFAULT is mapped to DRV_ERROR_INVALID_HANDLE for the upper-layer errmsg. */
        return -EFAULT;
    }

    key_ctx = casm_key_ctx_get(udevid);
    if (key_ctx == NULL) {
        svm_err("Get key ctx failed. (udevid=%u)\n", udevid);
        /* -EFAULT is mapped to DRV_ERROR_INVALID_HANDLE for the upper-layer errmsg. */
        return -EFAULT;
    }

    ka_task_mutex_lock(&key_ctx->mutex);
    ret = _casm_get_src_va(key_ctx, id, key, src_va, src_ex);
    ka_task_mutex_unlock(&key_ctx->mutex);
    casm_key_ctx_put(key_ctx);

    return ret;
}

void casm_key_show_dev(u32 udevid, int feature_id, ka_seq_file_t *seq)
{
    struct casm_key_ctx *key_ctx = NULL;
    struct casm_src_node *node = NULL;
    struct casm_src_node *dump_info = NULL;
    u32 dump_num = 0;
    u32 node_num, i;
    int j;

    if (feature_id != (int)casm_dev_feature_id) {
        return;
    }

    key_ctx = casm_key_ctx_get(udevid);
    if (key_ctx == NULL) {
        svm_err("Get key ctx failed. (udevid=%u)\n", udevid);
        return;
    }

    node_num = key_ctx->id_num;

    if ((node_num == 0) || (node_num > KEY_SHOW_NODE_MAX)) {
        svm_info("Skip show key ctx. (udevid=%u; node_num=%u)\n", udevid, node_num);
        casm_key_ctx_put(key_ctx);
        return;
    }

    dump_info = (struct casm_src_node *)svm_vzalloc(sizeof(*dump_info) * node_num);
    if (dump_info == NULL) {
        svm_warn("No mem for key show. (udevid=%u; num=%u)\n", udevid, node_num);
        casm_key_ctx_put(key_ctx);
        return;
    }

    ka_base_idr_lock(&key_ctx->idr);
    ka_base_idr_for_each_entry(&key_ctx->idr, node, j)
    {
        if (dump_num >= node_num) {
            break;
        }
        dump_info[dump_num] = *node;
        dump_num++;
    }
    ka_base_idr_unlock(&key_ctx->idr);

    casm_key_ctx_put(key_ctx);

    ka_fs_seq_printf(seq,
                     "casm udevid(%u) key info: "
                     "index    key    owner_tgid  udevid   tgid   va   size   updated_va\n",
                     udevid);
    for (i = 0; i < dump_num; i++) {
        struct svm_global_va *src_va = &dump_info[i].src_va;
        ka_fs_seq_printf(seq, "    %u   0x%llx    %d   %u  %d      0x%llx      0x%llx       0x%llx\n", i,
                         dump_info[i].key, dump_info[i].src_ex.owner_tgid, src_va->udevid, src_va->tgid, src_va->va,
                         src_va->size, dump_info[i].src_ex.updated_va);
    }

    svm_vfree(dump_info);
}
DECLAER_FEATURE_AUTO_SHOW_DEV(casm_key_show_dev, FEATURE_LOADER_STAGE_5);

int casm_key_init_dev(u32 udevid)
{
    struct casm_key_ctx *key_ctx = NULL;
    void *dev_ctx = NULL;
    int ret;

    key_ctx = (struct casm_key_ctx *)svm_kzalloc(sizeof(*key_ctx), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (key_ctx == NULL) {
        svm_err("No mem. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }

    dev_ctx = svm_dev_ctx_get(udevid);
    if (dev_ctx == NULL) {
        svm_kfree(key_ctx);
        return -ENODEV;
    }

    ret = svm_dev_set_feature_priv(dev_ctx, casm_dev_feature_id, "casm_key", key_ctx);
    if (ret != 0) {
        svm_err("Set dev feature priv failed. (udevid=%u; ret=%d)\n", udevid, ret);
        svm_kfree(key_ctx);
        svm_dev_ctx_put(dev_ctx);
        return ret;
    }

    key_ctx->dev_ctx = dev_ctx;
    key_ctx->udevid = udevid;
    casm_key_ctx_init(key_ctx);
    svm_inst_trace("Init success. (udevid=%u)\n", udevid);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(casm_key_init_dev, FEATURE_LOADER_STAGE_5);

void casm_key_uninit_dev(u32 udevid)
{
    struct casm_key_ctx *key_ctx = NULL;
    void *dev_ctx = NULL;

    dev_ctx = svm_dev_ctx_get(udevid);
    if (dev_ctx == NULL) {
        return;
    }

    key_ctx = (struct casm_key_ctx *)svm_dev_get_feature_priv(dev_ctx, casm_dev_feature_id);
    if (key_ctx != NULL) {
        (void)svm_dev_set_feature_priv(dev_ctx, casm_dev_feature_id, NULL, NULL);
        svm_dev_ctx_put(dev_ctx); /* paired with init */
        svm_dev_ctx_put(dev_ctx);
        casm_key_ctx_uninit(key_ctx);
        svm_kfree(key_ctx);
    }

    svm_inst_trace("Uninit success. (udevid=%u)\n", udevid);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(casm_key_uninit_dev, FEATURE_LOADER_STAGE_5);

static int casm_ioctl_create_key(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_casm_create_key_para para;
    struct svm_global_va src_va;
    u64 npage_size;
    int ret;

    ret = (int)ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    if ((udevid != uda_get_host_id()) && (para.task_type != PROCESS_CP1)) {
        svm_err("Invalid task type. (task_type=%u)\n", para.task_type);
        return -EINVAL;
    }

    ret = svm_dbi_kern_query_npage_size(udevid, &npage_size);
    if (ret != 0) {
        svm_err("Query page size failed. (udevid=%u)\n", udevid);
        return ret;
    }

    if ((!SVM_IS_ALIGNED(para.va, npage_size)) || (!SVM_IS_ALIGNED(para.size, npage_size))) {
        svm_err("Not align. (udevid=%u; va=0x%llx; size=0x%llx; npage_size=%llx)\n", udevid, para.va, para.size,
                npage_size);
        return -EINVAL;
    }

    ret = svm_smp_check_mem_exists(udevid, ka_task_get_current_tgid(), para.va, para.size);
    if (ret != 0) {
        svm_err("Not alloc mem. (udevid=%u; va=0x%llx; size=0x%llx)\n", udevid, para.va, para.size);
        return -EINVAL;
    }

    if (udevid == uda_get_host_id()) {
        svm_global_va_pack(udevid, ka_task_get_current_tgid(), para.va, para.size, &src_va);
    } else {
        int slave_tgid;
        ret = hal_kernel_apm_query_slave_tgid_by_master(ka_task_get_current_tgid(), udevid, para.task_type,
                                                        &slave_tgid);
        if (ret != 0) {
            svm_err("Query slave tgid failed. (udevid=%u; task_type=%u)\n", udevid, para.task_type);
            return -EINVAL;
        }
        svm_global_va_pack(udevid, slave_tgid, para.va, para.size, &src_va);
    }

    if ((para.src_prop.casm_pg_type <= CASM_PG_TYPE_INVALID) || (para.src_prop.casm_pg_type >= CASM_PG_TYPE_MAX)) {
        svm_err("Invalid source page type. (pg_type=%u)\n", (u32)para.src_prop.casm_pg_type);
        return -EINVAL;
    }

    ret = casm_create_key(&src_va, &para.src_prop, &para.key);
    if (ret == 0) {
        if (ka_base_copy_to_user((void __ka_user *)(uintptr_t)arg, &para, sizeof(para)) != 0) {
            svm_err("copy_to_user fail.\n");
            (void)casm_destroy_key(para.key, false);
            return -EFAULT;
        }
    }

    return ret;
}

static int casm_ioctl_destroy_key(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_casm_destroy_key_para para;
    int ret;

    ret = (int)ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    return casm_destroy_key(para.key, false);
}

static int casm_ioctl_op_task(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_casm_op_task_para para;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    /* udevid is host devid, not use continue */
    return casm_op_task(para.key, para.server_id, para.tgid, para.op);
}

int casm_key_init(void)
{
    casm_dev_feature_id = svm_dev_obtain_feature_id();

    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_CASM_CREATE_KEY), casm_ioctl_create_key);
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_CASM_DESTROY_KEY), casm_ioctl_destroy_key);
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_CASM_OP_TASK), casm_ioctl_op_task);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(casm_key_init, FEATURE_LOADER_STAGE_5);
