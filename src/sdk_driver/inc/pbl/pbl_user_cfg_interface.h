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

#ifndef PBL_USER_CFG_INTERFACE_H
#define PBL_USER_CFG_INTERFACE_H

#define PKCS_SIGN_TYPE_OFF 1
#define PKCS_SIGN_TYPE_ON 0

/* same as struct agentdrv_cpu_info */
typedef struct dev_cpu_nums_cfg_stru {
    unsigned int ccpu_num;
    unsigned int ccpu_os_sched;
    unsigned int dcpu_num;
    unsigned int dcpu_os_sched;
    unsigned int aicpu_num;
    unsigned int aicpu_os_sched;
    unsigned int tscpu_num;
    unsigned int tscpu_os_sched;
    unsigned int comcpu_num;
    unsigned int comcpu_os_sched;
} dev_cpu_nums_cfg_t;

int devdrv_config_set_pss_cfg(unsigned int dev_id, int sign);
int devdrv_config_get_pss_cfg(unsigned int dev_id, int *sign);

int dev_user_cfg_get_cpu_number(unsigned int dev_id, dev_cpu_nums_cfg_t *cpu_nums_cfg);
int soc_platform_get_cpu_number(unsigned int dev_id, dev_cpu_nums_cfg_t *cpu_nums_cfg);
int devdrv_config_get_mac_info(unsigned int dev_id, unsigned char *buf, unsigned int buf_size, unsigned int *info_size);
#endif
