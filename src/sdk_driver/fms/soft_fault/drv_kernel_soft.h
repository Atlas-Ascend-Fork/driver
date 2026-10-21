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

#ifndef DRV_KERNEL_SOFT_FAULT_H
#define DRV_KERNEL_SOFT_FAULT_H

void drv_kernel_soft_fault_unregister(void);

/* user role */
#define SOFT_FAULT_ACC_MASK 0x00FFU
/* allow running as root */
#define SOFT_FAULT_ACC_ROOT 0x0001U
/* allow running as HwHiAiUser's or HwBaseUser's group */
#define SOFT_FAULT_ACC_OPERATE 0x0002U
/* allow running as HwDmUser */
#define SOFT_FAULT_ACC_DM_USER 0x0004U
/* allow running as gust */
#define SOFT_FAULT_ACC_USER 0x0008U

#define SOFT_FAULT_ACC_ALL_USER (SOFT_FAULT_ACC_ROOT | SOFT_FAULT_ACC_OPERATE | SOFT_FAULT_ACC_DM_USER)

/* runtime environment */
#define SOFT_FAULT_RUN_ENV_MASK 0x0F00U
/* support physical */
#define SOFT_FAULT_ENV_PHYSICAL 0x0100U
/* support virtual */
#define SOFT_FAULT_ENV_VIRTUAL 0x0200U
/* support docker */
#define SOFT_FAULT_ENV_DOCKER 0x0400U
/* support admin docker */
#define SOFT_FAULT_ENV_ADMIN_DOCKER 0x0800U

#define SOFT_FAULT_ENV_ALL \
    (SOFT_FAULT_ENV_PHYSICAL | SOFT_FAULT_ENV_VIRTUAL | SOFT_FAULT_ENV_DOCKER | SOFT_FAULT_ENV_ADMIN_DOCKER)

struct soft_fault_acc {
    const char *proc_name;
    unsigned short node_type;
    unsigned char sensor_type;
    unsigned int err_type;
    unsigned int user_acc;
    unsigned int run_env;
};

#endif
