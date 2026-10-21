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

#ifndef DEVDRV_MANAGER_VDEVICE_H
#define DEVDRV_MANAGER_VDEVICE_H

#include "ka_common_pub.h"
#include "ka_list_pub.h"
#include "devdrv_user_common.h"
#include "ka_task_pub.h"

#define VDAVINCI_IDLE 0
#define VDAVINCI_INIT 2
#define VDAVINCI_USED 3
#define VDAVINCI_STATE_INUSED 1
#define VDAVINCI_STATE_IDLE 0

#define NONE_ROOT_ACCESS 0660

enum MNT_VDAVINCI_TYPE {
    MNT_VDAVINCI_TYPE_C1,
    MNT_VDAVINCI_TYPE_C2,
    MNT_VDAVINCI_TYPE_C4,
    MNT_VDAVINCI_TYPE_C8,
    MNT_VDAVINCI_TYPE_C16,
    MNT_VDAVINCI_TYPE_C1_4 = 10, /* It is for reserved between MNT_VDAVINCI_TYPE_C16 and 10. */
    MNT_VDAVINCI_TYPE_C2_4,
    MNT_VDAVINCI_TYPE_MAX
};

struct dev_mnt_vdev_action {
    unsigned int vdev_id;
    vdev_action action;
    ka_mnt_namespace_t *ns;
    u64 container_id;
    ka_list_head_t list_node;
};

struct dev_mnt_vdev_inform {
    ka_list_head_t vdev_action_head;
    ka_task_spinlock_t spinlock;
};

struct vdavinci_info {
    void *ns;                               /* ns namespace pointer */
    u32 used;                               /* whether the vdevice is used */
    u32 phy_devid;                          /* which physical device alloc the vdevice */
    u32 vfid;                               /* alloced by vmanager */
    u32 container_devid;                    /* container logic_devid */
    struct devdrv_vdev_spec_info spec_info; /* specification of the vdevice */
    u64 container_id;
};

typedef struct dev_mnt_vdev_engine {
    u32 alloc_state;
    ka_mutex_t lock;
    ka_cdev_t cdev;
    ka_device_t *dev;
    struct vdavinci_info info;
} DEV_MNT_VDEV_ENGINE_T;

typedef struct dev_mnt_vdev_cb {
    unsigned int vdev_major;
    ka_class_t *vdev_class;
    ka_mutex_t global_lock;
    DEV_MNT_VDEV_ENGINE_T vdev[ASCEND_VDEV_MAX_NUM];
} DEV_MNT_VDEV_CB_T;

int dev_mnt_vdevice_init(void);
int dev_mnt_vdevice_add_inform(unsigned int vdev_id, vdev_action action, ka_mnt_namespace_t *ns, u64 container_id);
int dev_mnt_create_one_vm_vdevice(unsigned int phy_id, unsigned int fid);
void dev_mnt_vdevice_uninit(void);
void dev_mnt_vdevice_inform(void);
void dev_mnt_released_one_vm_vdevice(unsigned int phy_id, unsigned int fid);

int devdrv_manager_ioctl_create_vdev(ka_file_t *filep, unsigned int cmd, unsigned long arg);
int devdrv_manager_ioctl_destroy_vdev(ka_file_t *filep, unsigned int cmd, unsigned long arg);
int devdrv_manager_ioctl_get_vdevinfo(ka_file_t *filep, unsigned int cmd, unsigned long arg);
#ifndef CFG_FEATURE_VFIO_SOC
int devdrv_manager_get_vdev_resource_info(struct devdrv_resource_info *dinfo);
#endif
#endif
