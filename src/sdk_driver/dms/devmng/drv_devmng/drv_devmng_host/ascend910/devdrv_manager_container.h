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

#ifndef DEVDRV_MANAGER_CONTAINER_H
#define DEVDRV_MANAGER_CONTAINER_H
#include "ka_system_pub.h"
#include "ka_common_pub.h"
#include "devdrv_manager_common.h"
#include "devdrv_user_common.h"
#include "devdrv_manager.h"
#include "kernel_version_adapt.h"
#include "ka_task_pub.h"

struct devdrv_manager_container_para {
    ka_system_timespec start_time;
    ka_system_timespec real_start_time;
    u32 dev_num;
    u32 dev_id[ASCEND_DEV_MAX_NUM]; /* device list read from dev directory */
    ka_mnt_namespace_t *mnt_ns;
    ka_pid_namespace_t *pid_ns;
    u64 container_id;
};
extern struct devdrv_manager_info *dev_manager_info;
ka_mnt_namespace_t *devdrv_manager_get_host_mnt_ns(void);
int devdrv_manager_container_table_init(struct devdrv_manager_info *manager_info);
void devdrv_manager_container_table_exit(struct devdrv_manager_info *manager_info);
int devdrv_manager_container_logical_id_to_physical_id(u32 logical_dev_id, u32 *physical_dev_id, u32 *vfid);
int devdrv_manager_container_get_docker_id(u32 *docker_id);
int devdrv_manager_container_process(ka_file_t *filep, unsigned long arg);
#define devdrv_manager_container_is_in_normal_container() devdrv_manager_container_is_in_container()
int devdrv_manager_container_is_in_container(void);
int devdrv_manager_container_is_in_admin_container(void);
int devdrv_manager_container_check_devid_in_container(u32 devid, ka_pid_t hostpid);
bool devdrv_manager_container_is_admin(void);
int devdrv_devpid_container_convert(int *ipid);
bool devdrv_manager_container_is_host_system(ka_mnt_namespace_t *mnt_ns);
#if defined(CFG_HOST_ENV) || defined(CFG_FEATURE_BIND_TGID)
int devdrv_get_tgid_by_pid(int pid, int *tgid);
#endif

#endif
