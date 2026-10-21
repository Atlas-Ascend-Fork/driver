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

#ifndef URD_MSG_H
#define URD_MSG_H

#define DEVDRV_MAX_COMPUTING_POWER_TYPE 1
#define SOC_VERSION_LENGTH 32U

struct devdrv_manager_hccl_devinfo {
    u8 env_type;
    u32 dev_id;
    u32 ctrl_cpu_ip;
    u32 ctrl_cpu_id;
    u32 ctrl_cpu_core_num;
    u64 ctrl_cpu_occupy_bitmap;
    u32 ctrl_cpu_endian_little;
    u32 ts_cpu_core_num;
    u32 ai_cpu_core_num;
    u32 ai_core_num;
    u64 ai_cpu_bitmap;
    u32 ai_core_id;
    u32 ai_cpu_core_id;
    u32 hardware_version; /* mini, cloud, lite, etc. */

    u32 ts_num;
    u64 aicore_freq;
    u64 cpu_system_count;
    u64 monotonic_raw_time_ns;
    u32 vector_core_num;
    u64 vector_core_freq;
    u64 computing_power[DEVDRV_MAX_COMPUTING_POWER_TYPE];
    u32 ffts_type;
    u32 chip_id;
    u32 die_id;
    u32 addr_mode;
    u32 host_device_connect_type;
    u32 mainboard_id;
    char soc_version[SOC_VERSION_LENGTH];
    u64 aicore_bitmap[2];      /* support max 128 aicore */
    u64 vector_core_bitmap[2]; /* support max 128 vector core */
    u8 product_type;
    u8 resv1[7];
    u64 real_time_ms;
    u32 l2_num;
    u32 hbm_num;
    u32 resv2[22];
};

#endif
