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

#include "ka_kernel_def_pub.h"
#include "pbl/pbl_feature_loader.h"
#include "msg_chan_main.h"

struct devdrv_comm_dev_ops g_comm_ops[DEVDRV_COMMNS_TYPE_MAX];
struct devdrv_faultmgr_dev_ops g_faultmgr_ops;
struct devdrv_serdes_dump_dev_ops g_serdes_dump_ops;
struct devdrv_msg_client g_client_info;

struct devdrv_comm_dev_ops *devdrv_get_comm_ops_ctrl(enum devdrv_communication_type type)
{
    if (type >= DEVDRV_COMMNS_TYPE_MAX) {
        return NULL;
    }
    return &g_comm_ops[type];
}

struct devdrv_msg_client *devdrv_get_msg_client()
{
    return &g_client_info;
}

struct devdrv_comm_dev_ops *devdrv_get_comm_ops()
{
    return g_comm_ops;
}

int devdrv_check_communication_api_proc(struct devdrv_comm_ops *ops)
{
    if (ops->sync_msg_send == NULL) {
        devdrv_err("Invalid ops, sync send ops is null. (type=%u)\n", ops->comm_type);
        return -EINVAL;
    }
    if ((ops->register_common_msg_client == NULL) || (ops->unregister_common_msg_client == NULL) ||
        (ops->common_msg_send == NULL)) {
        devdrv_err("Invalid ops, register common ops is null. (type=%u)\n", ops->comm_type);
        return -EINVAL;
    }
    return 0;
}

int devdrv_check_dev_manager_api_proc(struct devdrv_comm_ops *ops)
{
    if ((ops->set_msg_chan_priv == NULL) || (ops->get_msg_chan_priv == NULL)) {
        devdrv_err("Invalid ops, set msg chan priv is null. (type=%u)\n", ops->comm_type);
        return -EINVAL;
    }
    if ((ops->get_pfvf_type_by_devid == NULL) || (ops->mdev_vm_boot_mode == NULL) || (ops->sriov_support == NULL)) {
        devdrv_err("Invalid ops, pfvf or sriov is null. (type=%u)\n", ops->comm_type);
        return -EINVAL;
    }
    if (ops->get_connect_type == NULL) {
        devdrv_err("Invalid ops, get connect type is null. (type=%u)\n", ops->comm_type);
        return -EINVAL;
    }
    if (ops->get_msg_chan_devid == NULL) {
        devdrv_err("Invalid ops, get msg chan devid is null. (type=%u)\n", ops->comm_type);
        return -EINVAL;
    }
    return 0;
}

STATIC int devdrv_check_ops(struct devdrv_comm_ops *ops)
{
    if (ops == NULL) {
        devdrv_err("Invalid ops, ops is null.\n");
        return -EINVAL;
    }
    if (ops->comm_type >= DEVDRV_COMMNS_TYPE_MAX) {
        devdrv_err("Invalid comm type. (type=%u)\n", ops->comm_type);
        return -EINVAL;
    }
    if (devdrv_check_communication_api(ops) != 0) {
        devdrv_err("Check communication api failed. (type=%u)\n", ops->comm_type);
        return -EINVAL;
    }
    if (devdrv_check_dev_manager_api(ops) != 0) {
        devdrv_err("Check dev manager api failed. (type=%u)\n", ops->comm_type);
        return -EINVAL;
    }
    return 0;
}

int devdrv_register_communication_ops(struct devdrv_comm_ops *ops)
{
    u32 type;
    unsigned long flags;

    if (devdrv_check_ops(ops) != 0) {
        devdrv_err("Invalid ops, ops is null.\n");
        return -EINVAL;
    }
    type = ops->comm_type;
    ka_task_write_lock_irqsave(&g_comm_ops[type].rwlock, flags);
    (void)memcpy_s(&g_comm_ops[type].ops, sizeof(g_comm_ops[type].ops), ops, sizeof(*ops));
    g_comm_ops[type].status = DEVDRV_COMM_OPS_TYPE_INIT;
    ka_base_atomic_set(&g_comm_ops[type].ops.ref_cnt, 0);
    ka_task_write_unlock_irqrestore(&g_comm_ops[type].rwlock, flags);
    devdrv_info("Ops register success. (type=%u)\n", type);
    return 0;
}
KA_EXPORT_SYMBOL(devdrv_register_communication_ops);

void devdrv_unregister_communication_ops(struct devdrv_comm_ops *ops)
{
    int i;
    int len = sizeof(struct devdrv_comm_ops);
    u32 type;
    unsigned long flags;

    if (ops == NULL) {
        devdrv_err("Invalid ops when unregister, ops is null.\n");
        return;
    }
    if (ops->comm_type >= DEVDRV_COMMNS_TYPE_MAX) {
        devdrv_err("Invalid type when unregister. (type=%u)\n", ops->comm_type);
        return;
    }
    type = ops->comm_type;
    for (i = 0; i < COMMU_WAIT_MAX_CNT; ++i) {
        if (ka_base_atomic_read(&g_comm_ops[type].ops.ref_cnt) == 0) {
            break;
        }
        ka_system_usleep_range(COMMU_WAIT_PER_TIME, COMMU_WAIT_PER_TIME);
    }
    if (i == COMMU_WAIT_MAX_CNT) {
        devdrv_info("Ops will force to unregister. (ref_cnt=%u; type=%u)\n",
                    ka_base_atomic_read(&g_comm_ops[type].ops.ref_cnt), type);
    }
    ka_task_write_lock_irqsave(&g_comm_ops[type].rwlock, flags);
    g_comm_ops[type].status = DEVDRV_COMM_OPS_TYPE_UNINIT;
    (void)memset_s(&g_comm_ops[type].ops, len, 0, len);
    ka_task_write_unlock_irqrestore(&g_comm_ops[type].rwlock, flags);
    devdrv_info("Ops unregister success. (type=%u)\n", type);
}
KA_EXPORT_SYMBOL(devdrv_unregister_communication_ops);

void devdrv_register_save_client_info_proc(struct devdrv_comm_dev_ops *dev_ops)
{
    int i;
    int ret;

    ka_task_mutex_lock(&g_client_info.lock);
    for (i = 0; i < DEVDRV_COMMON_MSG_TYPE_MAX; i++) {
        if (g_client_info.comm[i] == NULL) {
            continue;
        }
        ret = dev_ops->ops.register_common_msg_client(g_client_info.comm[i]);
        if (ret != 0) {
            devdrv_err("Ops register common client failed. (type=%u)\n", g_client_info.comm[i]->type);
        }
    }
}

STATIC void devdrv_set_communication_set_ops_enable(u32 type, u32 dev_id)
{
    unsigned long flags;

    ka_task_write_lock_irqsave(&g_comm_ops[type].rwlock, flags);
    g_comm_ops[type].status = DEVDRV_COMM_OPS_TYPE_ENABLE;
    ka_task_write_unlock_irqrestore(&g_comm_ops[type].rwlock, flags);
    if (ka_base_atomic_add_return(1, &g_comm_ops[type].dev_cnt) == 1) {
        /* first enable ops, will register all save info */
        devdrv_register_save_client_info(&g_comm_ops[type]);
    }
    devdrv_info("Ops enable success. (dev_id=%u;type=%u)\n", dev_id, type);
    return;
}

STATIC void devdrv_set_communication_set_ops_disable(u32 type, u32 dev_id)
{
    int cnt;
    unsigned long flags;

    ka_task_write_lock_irqsave(&g_comm_ops[type].rwlock, flags);
    cnt = ka_base_atomic_sub_return(1, &g_comm_ops[type].dev_cnt);
    if (cnt == 0) {
        g_comm_ops[type].status = DEVDRV_COMM_OPS_TYPE_DISABLE;
    }
    ka_task_write_unlock_irqrestore(&g_comm_ops[type].rwlock, flags);

    if (cnt > 0) {
        devdrv_info("There are other devices. (dev_id=%u;type=%u;dev_cnt=%d)\n", dev_id, type, cnt);
    } else if (cnt < 0) {
        devdrv_err("Get dev cnt error. (dev_id=%u;type=%u;dev_cnt=%d)\n", dev_id, type, cnt);
    }
    devdrv_info("Ops disable success. (dev_id=%u;type=%u)\n", dev_id, type);
    return;
}

void devdrv_set_communication_ops_status_inner(u32 type, u32 status, u32 index_id)
{
    if ((type >= DEVDRV_COMMNS_TYPE_MAX) || (status >= DEVDRV_COMM_OPS_TYPE_MAX)) {
        devdrv_err("Invalid type or status. (dev_id=%u;type=%u;status=%u)\n", index_id, type, status);
        return;
    }

    if (status == DEVDRV_COMM_OPS_TYPE_ENABLE) {
        devdrv_set_communication_set_ops_enable(type, index_id);
    } else if (status == DEVDRV_COMM_OPS_TYPE_DISABLE) {
        devdrv_set_communication_set_ops_disable(type, index_id);
    } else {
        devdrv_warn("Ops status invalid. (dev_id=%u;type=%u;status=%u)\n", index_id, type, status);
    }
    return;
}
KA_EXPORT_SYMBOL(devdrv_set_communication_ops_status_inner);

void devdrv_set_communication_ops_status(u32 type, u32 status, u32 dev_id)
{
    u32 index_id;
    index_id = devdrv_get_index_id_by_devid(dev_id);
    devdrv_set_communication_ops_status_inner(type, status, index_id);
}
KA_EXPORT_SYMBOL(devdrv_set_communication_ops_status);

struct devdrv_comm_dev_ops *devdrv_add_ops_ref()
{
    int i;
    struct devdrv_comm_dev_ops *dev_ops;

    for (i = 0; i < DEVDRV_COMMNS_TYPE_MAX; i++) {
        dev_ops = &g_comm_ops[i];
        ka_task_read_lock(&dev_ops->rwlock);
        if (dev_ops->status != DEVDRV_COMM_OPS_TYPE_ENABLE) {
            ka_task_read_unlock(&dev_ops->rwlock);
            continue;
        } else {
            ka_base_atomic_add(1, &dev_ops->ops.ref_cnt);
            ka_task_read_unlock(&dev_ops->rwlock);
            return dev_ops;
        }
    }
    devdrv_info("Ops status list. (type=%u;status=%u;type=%u;status=%u)\n", DEVDRV_COMMNS_PCIE,
                g_comm_ops[DEVDRV_COMMNS_PCIE].status, DEVDRV_COMMNS_UB, g_comm_ops[DEVDRV_COMMNS_UB].status);
    return NULL;
}
struct devdrv_comm_dev_ops *devdrv_add_ops_ref_after_unbind()
{
    int i;
    struct devdrv_comm_dev_ops *dev_ops;

    for (i = 0; i < DEVDRV_COMMNS_TYPE_MAX; i++) {
        dev_ops = &g_comm_ops[i];
        ka_task_read_lock(&dev_ops->rwlock);
        if ((dev_ops->status != DEVDRV_COMM_OPS_TYPE_ENABLE) && (dev_ops->status != DEVDRV_COMM_OPS_TYPE_DISABLE)) {
            ka_task_read_unlock(&dev_ops->rwlock);
            continue;
        } else {
            ka_base_atomic_add(1, &dev_ops->ops.ref_cnt);
            ka_task_read_unlock(&dev_ops->rwlock);
            return dev_ops;
        }
    }
    devdrv_info("Ops status list. (type=%u;status=%u;type=%u;status=%u)\n", DEVDRV_COMMNS_PCIE,
                g_comm_ops[DEVDRV_COMMNS_PCIE].status, DEVDRV_COMMNS_UB, g_comm_ops[DEVDRV_COMMNS_UB].status);
    return NULL;
}
struct devdrv_comm_dev_ops *devdrv_add_ops_ref_by_type(u32 type)
{
    struct devdrv_comm_dev_ops *dev_ops = NULL;

    if (type >= DEVDRV_COMMNS_TYPE_MAX) {
        devdrv_err("Invalid connect type get.(max_type=%u; type=%u).\n", DEVDRV_COMMNS_TYPE_MAX, type);
        return NULL;
    }

    dev_ops = &g_comm_ops[type];
    ka_task_read_lock(&dev_ops->rwlock);
    if ((dev_ops->status == DEVDRV_COMM_OPS_TYPE_ENABLE) || (dev_ops->status == DEVDRV_COMM_OPS_TYPE_INIT)) {
        ka_base_atomic_add(1, &dev_ops->ops.ref_cnt);
        ka_task_read_unlock(&dev_ops->rwlock);
        return dev_ops;
    }

    ka_task_read_unlock(&dev_ops->rwlock);

    return NULL;
}

int devdrv_get_global_connect_protocol(void)
{
    int i;
    struct devdrv_comm_dev_ops *dev_ops;
    int conn_type = DEVDRV_COMMNS_TYPE_MAX;

    for (i = 0; i < DEVDRV_COMMNS_TYPE_MAX; i++) {
        dev_ops = &g_comm_ops[i];
        ka_task_read_lock(&dev_ops->rwlock);
        if (dev_ops->status != DEVDRV_COMM_OPS_TYPE_ENABLE) {
            ka_task_read_unlock(&dev_ops->rwlock);
            continue;
        } else {
            conn_type = i;
            ka_task_read_unlock(&dev_ops->rwlock);
            break;
        }
    }

    return conn_type;
}
KA_EXPORT_SYMBOL(devdrv_get_global_connect_protocol);

void devdrv_sub_ops_ref(struct devdrv_comm_dev_ops *dev_ops)
{
    ka_base_atomic_sub(1, &dev_ops->ops.ref_cnt);
}

void devdrv_sub_ops_ref_by_type(struct devdrv_comm_dev_ops *dev_ops)
{
    ka_base_atomic_sub(1, &dev_ops->ops.ref_cnt);
}

int devdrv_register_faultmgr_ops(struct devdrv_faultmgr_ops *ops)
{
    if (ops == NULL) {
        devdrv_err("Invalid ops, ops is null.\n");
        return -EINVAL;
    }

    ka_task_write_lock(&g_faultmgr_ops.rwlock);
    (void)memcpy_s(&g_faultmgr_ops.ops, sizeof(g_faultmgr_ops.ops), ops, sizeof(*ops));
    g_faultmgr_ops.status = DEVDRV_COMM_OPS_TYPE_ENABLE;
    ka_base_atomic_set(&g_faultmgr_ops.ops.ref_cnt, 0);
    ka_task_write_unlock(&g_faultmgr_ops.rwlock);
    devdrv_info("Faultmgr ops register success.\n");
    return 0;
}
KA_EXPORT_SYMBOL(devdrv_register_faultmgr_ops);

void devdrv_unregister_faultmgr_ops(void)
{
    size_t len = sizeof(struct devdrv_faultmgr_ops);
    int i;

    for (i = 0; i < COMMU_WAIT_MAX_CNT; ++i) {
        if (ka_base_atomic_read(&g_faultmgr_ops.ops.ref_cnt) == 0) {
            break;
        }
        ka_system_usleep_range(COMMU_WAIT_PER_TIME, COMMU_WAIT_PER_TIME);
    }
    if (i == COMMU_WAIT_MAX_CNT) {
        devdrv_info("Faultmgr ops will force to unregister. (ref_cnt=%u)\n",
                    ka_base_atomic_read(&g_faultmgr_ops.ops.ref_cnt));
    }
    ka_task_write_lock(&g_faultmgr_ops.rwlock);
    g_faultmgr_ops.status = DEVDRV_COMM_OPS_TYPE_UNINIT;
    (void)memset_s(&g_faultmgr_ops.ops, len, 0, len);
    ka_task_write_unlock(&g_faultmgr_ops.rwlock);
    devdrv_info("Faultmgr ops unregister success.\n");
}
KA_EXPORT_SYMBOL(devdrv_unregister_faultmgr_ops);

struct devdrv_faultmgr_dev_ops *devdrv_get_faultmgr_ops(void)
{
    return &g_faultmgr_ops;
}

struct devdrv_faultmgr_dev_ops *devdrv_add_faultmgr_ops_ref(void)
{
    struct devdrv_faultmgr_dev_ops *dev_ops = &g_faultmgr_ops;

    ka_task_read_lock(&dev_ops->rwlock);
    if (dev_ops->status != DEVDRV_COMM_OPS_TYPE_ENABLE) {
        ka_task_read_unlock(&dev_ops->rwlock);
        devdrv_info("Ops status list. (status=%u)\n", dev_ops->status);
        return NULL;
    }
    ka_base_atomic_add(1, &dev_ops->ops.ref_cnt);
    ka_task_read_unlock(&dev_ops->rwlock);
    return dev_ops;
}

void devdrv_sub_faultmgr_ops_ref(struct devdrv_faultmgr_dev_ops *dev_ops)
{
    ka_base_atomic_sub(1, &dev_ops->ops.ref_cnt);
}

int devdrv_register_serdes_dump_ops(struct devdrv_serdes_dump_ops *ops)
{
    if (ops == NULL) {
        devdrv_err("Invalid ops, ops is null.\n");
        return -EINVAL;
    }

    ka_task_write_lock(&g_serdes_dump_ops.rwlock);
    (void)memcpy_s(&g_serdes_dump_ops.ops, sizeof(struct devdrv_serdes_dump_ops), ops,
                   sizeof(struct devdrv_serdes_dump_ops));
    g_serdes_dump_ops.status = DEVDRV_COMM_OPS_TYPE_ENABLE;
    ka_base_atomic_set(&g_serdes_dump_ops.ops.ref_cnt, 0);
    ka_task_write_unlock(&g_serdes_dump_ops.rwlock);
    devdrv_info("Serdes dump ops register success.\n");
    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_register_serdes_dump_ops);

void devdrv_unregister_serdes_dump_ops(void)
{
    size_t len = sizeof(struct devdrv_serdes_dump_ops);
    int i;

    for (i = 0; i < COMMU_WAIT_MAX_CNT; ++i) {
        if (ka_base_atomic_read(&g_serdes_dump_ops.ops.ref_cnt) == 0) {
            break;
        }
        ka_system_usleep_range(COMMU_WAIT_PER_TIME, COMMU_WAIT_PER_TIME);
    }
    if (i == COMMU_WAIT_MAX_CNT) {
        devdrv_info("Serdes dump ops will force to unregister. (ref_cnt=%u)\n",
                    ka_base_atomic_read(&g_serdes_dump_ops.ops.ref_cnt));
    }
    ka_task_write_lock(&g_serdes_dump_ops.rwlock);
    g_serdes_dump_ops.status = DEVDRV_COMM_OPS_TYPE_UNINIT;
    (void)memset_s(&g_serdes_dump_ops.ops, len, 0, len);
    ka_task_write_unlock(&g_serdes_dump_ops.rwlock);
    devdrv_info("Serdes dump ops unregister success.\n");
}
KA_EXPORT_SYMBOL_GPL(devdrv_unregister_serdes_dump_ops);

struct devdrv_serdes_dump_dev_ops *devdrv_get_serdes_dump_ops(void)
{
    return &g_serdes_dump_ops;
}

struct devdrv_serdes_dump_dev_ops *devdrv_add_serdes_dump_ops_ref(void)
{
    struct devdrv_serdes_dump_dev_ops *dev_ops = &g_serdes_dump_ops;

    ka_task_read_lock(&dev_ops->rwlock);
    if (dev_ops->status != DEVDRV_COMM_OPS_TYPE_ENABLE) {
        ka_task_read_unlock(&dev_ops->rwlock);
        devdrv_info("Ops status list. (status=%u)\n", dev_ops->status);
        return NULL;
    }
    ka_base_atomic_add(1, &dev_ops->ops.ref_cnt);
    ka_task_read_unlock(&dev_ops->rwlock);
    return dev_ops;
}

void devdrv_sub_serdes_dump_ops_ref(struct devdrv_serdes_dump_dev_ops *dev_ops)
{
    ka_base_atomic_sub(1, &dev_ops->ops.ref_cnt);
}
