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

/* SOFT_FAULT_ACC_ROOT:          allow running as root */
/* SOFT_FAULT_ACC_OPERATE:       allow running as HwHiAiUser's or HwBaseUser's group */
/* SOFT_FAULT_ACC_DM_USER:       allow running as HwDmUser */
/* SOFT_FAULT_ACC_ALL_USER:      allow running all user */

/* SOFT_FAULT_ENV_PHYSICAL:      physical */
/* SOFT_FAULT_ENV_VIRTUAL:       virtual */
/* SOFT_FAULT_ENV_DOCKER:        docker */
/* SOFT_FAULT_ENV_ADMIN_DOCKER:  admin docker*/
/* SOFT_FAULT_ENV_ALL:           all env */

/* proc_name,       node_type,   sensor_type,    err_type,     user_acc,     run_env */
#ifdef CFG_DEVICE_ENV
{"pm_child",         0x601,      0xD0,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"process-manager",  0x602,      0x27,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"process-manager",  0x603,      0x27,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"iammgr",           0x604,      0x27,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"iammgr",           0x605,      0x27,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"iammgr",           0x606,      0x27,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"iammgr",           0x607,      0x27,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"iammgr",           0x608,      0x27,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"proc_launcher",    0x609,      0x27,            0x00,        SOFT_FAULT_ACC_OPERATE,   SOFT_FAULT_ENV_PHYSICAL},
{"tsdaemon",         0x60B,      0xD0,            0x05,        SOFT_FAULT_ACC_OPERATE,   SOFT_FAULT_ENV_PHYSICAL},
{"tsdaemon",         0x60C,      0xD0,            0x05,        SOFT_FAULT_ACC_OPERATE,   SOFT_FAULT_ENV_PHYSICAL},
{"tsdaemon",         0x60E,      0xD0,            0x05,        SOFT_FAULT_ACC_OPERATE,   SOFT_FAULT_ENV_PHYSICAL},
{"hccp_service.bin", 0x60F,      0xC3,            0x08,        SOFT_FAULT_ACC_OPERATE,   SOFT_FAULT_ENV_PHYSICAL},
{"tsdaemon",         0x60F,      0xD0,            0x06,        SOFT_FAULT_ACC_OPERATE,   SOFT_FAULT_ENV_PHYSICAL},
{"proc_launcher",    0x610,      0x27,            0x00,        SOFT_FAULT_ACC_OPERATE,   SOFT_FAULT_ENV_PHYSICAL},
{"process-manager",  0x617,      0xD0,            0x01,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"iammgr",           0x623,      0x27,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"iammgr",           0x624,      0x27,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"iammgr",           0x626,      0x27,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
{"ubcfglite",        0x626,      0xD0,            0x00,        SOFT_FAULT_ACC_ROOT,      SOFT_FAULT_ENV_PHYSICAL},
#else
#endif
