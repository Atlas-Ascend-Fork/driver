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

#ifndef UDA_ACCESS_H
#define UDA_ACCESS_H

#include "ka_system_pub.h"
#include "ka_common_pub.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"

#include "pbl_uda.h"
#include "uda_pub_def.h"

#define UDA_DEV_MAX_NUM UDA_MAX_PHY_DEV_NUM
#define UDA_DEV_NAME_LEN 32

#if (defined(DRV_HOST)) || (defined(CFG_FEATURE_VFIO_SOC))
#define DEV_MODE_PERMISSION 0660
#else
#define DEV_MODE_PERMISSION 0600
#endif
#define UDA_HOST_ID 65

struct uda_ns_node {
    ka_list_head_t node;
    ka_mnt_namespace_t *ns;
    ka_delayed_work_t wait_destroy_work;
    u32 root_tgid;
    TASK_TIME_TYPE tgid_time;
    u32 ns_id;
    u32 dev_num;
    ka_atomic_t destroy_try_count;
    u64 identify;
    u32 devid_to_udevid[UDA_DEV_MAX_NUM]; /* Use arrays to speed ka_task_up the conversion from logical id to udevid */
};

struct uda_access_share_node {
    ka_list_head_t node;
    u32 ns_id;
    ka_mnt_namespace_t *ns;
};

struct uda_access {
    char name[UDA_DEV_NAME_LEN];
    ka_device_t *dev;
    ka_cdev_t *cdev;
    ka_dev_t devno;
    u32 ns_id;
    ka_mnt_namespace_t *ns;
    ka_mutex_t mutex;
    ka_list_head_t share_head;
    u32 dev_logic_id;
};

bool uda_cur_is_admin(void);
void uda_for_each_ns_node_safe(void *priv, void (*func)(struct uda_ns_node *ns_node, void *priv));

bool uda_is_dev_shared(u32 udevid);

int uda_setup_ns_node(u32 dev_num);
int uda_udevid_to_devid(u32 udevid, u32 *devid);

int uda_access_add_dev(u32 udevid, u32 logic_id, struct uda_access *access);
int uda_access_remove_dev(u32 udevid, struct uda_access *access);

int devdrv_get_devnum(u32 *dev_num);
int devdrv_get_vdevnum(u32 *dev_num);
u32 devdrv_manager_get_devnum(void);
int devdrv_get_devids(u32 *devices, u32 device_num);
int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);

void uda_recycle_ns_node(void);
bool uda_cur_is_host(void);

int uda_access_init(void);
void uda_access_uninit(void);

ka_mnt_namespace_t *uda_get_current_ns(void);
void uda_recycle_idle_ns_node_immediately(void);
#endif
