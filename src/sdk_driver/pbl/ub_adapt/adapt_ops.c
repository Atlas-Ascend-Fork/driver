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

#include "adapt_ops.h"
#include "ka_kernel_def_pub.h"
#include "securec.h"
#include "msg_chan_main.h"

static struct uba_ubcore_adapt_ops g_ubcore_ops_storage;
static struct uba_ummu_adapt_ops g_ummu_ops_storage;

struct uba_dev_ops g_adapt_dev_ops[UBA_OPS_TYPE_MAX];
ka_atomic_t g_adapt_module_cnt[UBA_MODULE_TYPE_MAX][UBA_FUNC_TYPE_MAX];

void uba_add_module(enum uba_module_type module_type, enum uba_func_type func_type)
{
    if ((module_type >= UBA_MODULE_TYPE_MAX) || (func_type >= UBA_FUNC_TYPE_MAX)) {
        devdrv_err("Invalid module_type or func_type. (module_type=%u; func_type=%u).\n", module_type, func_type);
        return;
    }
    ka_base_atomic_add(1, &g_adapt_module_cnt[module_type][func_type]);
}

ka_atomic_t *uba_get_module_cnt(void)
{
    return &g_adapt_module_cnt[0][0];
}

struct uba_dev_ops *uba_add_adapt_ops_ref(enum uba_ops_type ops_type)
{
    struct uba_dev_ops *dev_ops;

    if (ops_type >= UBA_OPS_TYPE_MAX) {
        devdrv_err("Invalid adapt ops type when add ref. (ops_type=%u).\n", ops_type);
        return NULL;
    }

    dev_ops = &g_adapt_dev_ops[ops_type];
    ka_task_read_lock(&dev_ops->rwlock);
    if (dev_ops->status != UBA_OPS_STATUS_ENABLE) {
        ka_task_read_unlock(&dev_ops->rwlock);
        return NULL;
    }
    ka_base_atomic_add(1, &dev_ops->ref_cnt);
    ka_task_read_unlock(&dev_ops->rwlock);
    return dev_ops;
}

void uba_sub_adapt_ops_ref(struct uba_dev_ops *dev_ops)
{
    ka_base_atomic_sub(1, &dev_ops->ref_cnt);
}

struct uba_dev_ops *uba_get_adapt_ops(enum uba_ops_type ops_type)
{
    if (ops_type >= UBA_OPS_TYPE_MAX) {
        devdrv_err("Invalid adapt ops type. (ops_type=%u).\n", ops_type);
        return NULL;
    }
    return &g_adapt_dev_ops[ops_type];
}

static int register_ubcore_ops(struct uba_dev_ops *dev_ops)
{
    if (dev_ops->ops.ubcore == NULL) {
        return -EINVAL;
    }
    (void)memcpy_s(&g_ubcore_ops_storage, sizeof(g_ubcore_ops_storage), dev_ops->ops.ubcore,
                   sizeof(g_ubcore_ops_storage));
    g_adapt_dev_ops[UBA_OPS_UBCORE].ops.ubcore = &g_ubcore_ops_storage;
    return 0;
}

static int register_ummu_ops(struct uba_dev_ops *dev_ops)
{
    if (dev_ops->ops.ummu == NULL) {
        return -EINVAL;
    }
    (void)memcpy_s(&g_ummu_ops_storage, sizeof(g_ummu_ops_storage), dev_ops->ops.ummu, sizeof(g_ummu_ops_storage));
    g_adapt_dev_ops[UBA_OPS_UMMU].ops.ummu = &g_ummu_ops_storage;
    return 0;
}

static void unregister_ubcore_ops(void)
{
    (void)memset_s(&g_ubcore_ops_storage, sizeof(g_ubcore_ops_storage), 0, sizeof(g_ubcore_ops_storage));
    g_adapt_dev_ops[UBA_OPS_UBCORE].ops.ubcore = NULL;
}

static void unregister_ummu_ops(void)
{
    (void)memset_s(&g_ummu_ops_storage, sizeof(g_ummu_ops_storage), 0, sizeof(g_ummu_ops_storage));
    g_adapt_dev_ops[UBA_OPS_UMMU].ops.ummu = NULL;
}

int uba_register_adapt_ops(enum uba_ops_type ops_type, struct uba_dev_ops *dev_ops)
{
    int ret;

    if ((dev_ops == NULL) || (ops_type >= UBA_OPS_TYPE_MAX)) {
        devdrv_err("Invalid adapt ops type or dev_ops is null. (ops_type=%u).\n", ops_type);
        return -EINVAL;
    }

    if (g_adapt_dev_ops[ops_type].status == UBA_OPS_STATUS_ENABLE) {
        devdrv_err("Adapt ops already registered. (ops_type=%u).\n", ops_type);
        return -EEXIST;
    }

    ka_task_rwlock_init(&g_adapt_dev_ops[ops_type].rwlock);
    ka_task_write_lock(&g_adapt_dev_ops[ops_type].rwlock);

    switch (ops_type) {
        case UBA_OPS_UBCORE:
            ret = register_ubcore_ops(dev_ops);
            break;
        case UBA_OPS_UMMU:
            ret = register_ummu_ops(dev_ops);
            break;
        default:
            ret = -EINVAL;
            break;
    }
    if (ret != 0) {
        goto register_fail;
    }

    g_adapt_dev_ops[ops_type].type = dev_ops->type;
    g_adapt_dev_ops[ops_type].status = UBA_OPS_STATUS_ENABLE;
    ka_base_atomic_set(&g_adapt_dev_ops[ops_type].ref_cnt, 0);
    ka_task_write_unlock(&g_adapt_dev_ops[ops_type].rwlock);
    devdrv_info("Adapt ops register success. (ops_type=%u).\n", ops_type);
    return ret;

register_fail:
    ka_task_write_unlock(&g_adapt_dev_ops[ops_type].rwlock);
    devdrv_err("Unknown adapt ops type when register. (ops_type=%u).\n", ops_type);
    return ret;
}
KA_EXPORT_SYMBOL(uba_register_adapt_ops);

void uba_unregister_adapt_ops(enum uba_ops_type ops_type)
{
    int i = 0;

    if (ops_type >= UBA_OPS_TYPE_MAX) {
        devdrv_err("Invalid adapt ops type when unregister. (ops_type=%u).\n", ops_type);
        return;
    }

    ka_task_write_lock(&g_adapt_dev_ops[ops_type].rwlock);
    g_adapt_dev_ops[ops_type].status = UBA_OPS_STATUS_DISABLE;
    ka_task_write_unlock(&g_adapt_dev_ops[ops_type].rwlock);

    while (ka_base_atomic_read(&g_adapt_dev_ops[ops_type].ref_cnt) != 0 && i < OPS_WAIT_MAX_CNT) {
        ka_system_usleep_range(OPS_WAIT_PER_TIME, OPS_WAIT_PER_TIME);
        i++;
    }
    if (i == OPS_WAIT_MAX_CNT) {
        devdrv_info("Adapt ops will force to unregister. (ref_cnt=%u; ops_type=%u).\n",
                    ka_base_atomic_read(&g_adapt_dev_ops[ops_type].ref_cnt), ops_type);
    }
    ka_task_write_lock(&g_adapt_dev_ops[ops_type].rwlock);
    g_adapt_dev_ops[ops_type].status = UBA_OPS_STATUS_UNINIT;

    switch (ops_type) {
        case UBA_OPS_UBCORE:
            unregister_ubcore_ops();
            break;
        case UBA_OPS_UMMU:
            unregister_ummu_ops();
            break;
        default:
            break;
    }
    g_adapt_dev_ops[ops_type].type = 0;
    ka_task_write_unlock(&g_adapt_dev_ops[ops_type].rwlock);
    devdrv_info("Adapt ops unregister success. (ops_type=%u).\n", ops_type);
}
KA_EXPORT_SYMBOL(uba_unregister_adapt_ops);
