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
#ifndef DEVDRV_MANAGER_COMMON_MSG_H
#define DEVDRV_MANAGER_COMMON_MSG_H

typedef int pid_t;
#ifndef ASCEND_DEV_MAX_NUM
#define ASCEND_DEV_MAX_NUM 64
#endif

#define DEVDRV_MAX_COMPUTING_POWER_TYPE 1
#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(DEVMNG_UT) && !defined(LOG_UT)
#define DEVDRV_CONTIANER_NUM_OF_LONG ((ASCEND_DEV_MAX_NUM - 1) / 64 + 1)
struct devdrv_device_info {
    u64 dump_ddr_dma_addr;
    u64 cpu_system_count;
    u64 dev_nominal_osc_freq;
    u64 monotonic_raw_time_ns;

    u32 ai_cpu_broken_map;
    u32 ai_core_broken_map;
    u64 aicore_bitmap;
    u32 ctrl_cpu_ip;
    u32 ctrl_cpu_id;
    u32 ctrl_cpu_core_num;
    u64 ctrl_cpu_occupy_bitmap;
    u32 ctrl_cpu_endian_little;
    u32 ts_cpu_core_num;
    u32 ai_cpu_core_num;
    u32 ai_core_num;
    u32 ai_cpu_core_id;
    u32 ai_core_id;
    u64 aicpu_occupy_bitmap;
    u32 hardware_version;
    u32 ts_load_fail;
    u32 dump_ddr_size;
    u32 ffts_type;
    u32 chip_name;
    u32 chip_version;
    u32 chip_info;

    u8 env_type;
    u8 ai_cpu_ready_num;
    u8 ai_core_ready_num;
    u8 ai_subsys_ip_map;
};
#else
#define DEVDRV_CONTIANER_NUM_OF_LONG ((VDAVINCI_MAX_VDEV_ID - 1) / 64 + 1)
#define MAX_CHIP_NAME 32
#define DEVMNG_SHM_INFO_RANDOM_SIZE 24
struct devdrv_device_info { /* for device sync to host */
    u64 dump_ddr_dma_addr;
    u64 aicore_freq;
    u64 cpu_system_count;
    u64 dev_nominal_osc_freq;
    u64 monotonic_raw_time_ns;
    u64 vector_core_freq;
    u64 reg_ddr_dma_addr;
    u64 vmcore_ddr_dma_addr;
    u64 computing_power[DEVDRV_MAX_COMPUTING_POWER_TYPE];

    u32 ai_cpu_broken_map;
    u32 ai_core_broken_map;
    u64 aicore_bitmap;
    u64 vector_core_bitmap;
    u32 ai_subsys_ip_map;

    u32 ctrl_cpu_ip;
    u32 ctrl_cpu_id;
    u32 ctrl_cpu_core_num;
    u64 ctrl_cpu_occupy_bitmap;
    u32 ctrl_cpu_endian_little;
    u32 ts_cpu_core_num;
    u32 ai_cpu_core_num;
    u32 ai_core_num;
    u32 ai_cpu_core_id;
    u32 ai_core_id;
    u64 aicpu_occupy_bitmap;

    u32 hardware_version;
    u32 ts_load_fail;
    u32 capability;
    u32 dump_ddr_size;
    u32 vector_core_num;
    u32 reg_ddr_size;
    u32 vmcore_ddr_size;
    u32 ffts_type;

    u32 chip_name;
    u32 chip_version;
    u32 chip_info;
    u8 soc_version[MAX_CHIP_NAME];
    u8 chip_id;
    u8 multi_chip;
    u8 multi_die;
    u8 mainboard_id;
    u16 connect_type;
    u16 board_id;
    u32 die_id;

    u8 env_type;
    u8 ai_cpu_ready_num;
    u8 ai_core_ready_num;
    u8 ai_core_num_level;
    u8 ai_core_freq_level;
    u8 template_name[MAX_CHIP_NAME];
    u8 resv[3]; /* 3 bytes for aligned */
    u16 server_id;
    u16 scale_type;
    u32 super_pod_id;
    u16 addr_mode;
    char random_number[DEVMNG_SHM_INFO_RANDOM_SIZE];
    u8 resv2[190];
};
#endif

#ifndef DEVDRV_HEART_BEAT_STRUCT__
#define DEVDRV_HEART_BEAT_STRUCT__
struct devdrv_aicore_msg {
    u32 syspcie_sysdma_status; /* upper 16 bit: syspcie, lower 16 bit: sysdma */
    u32 aicpu_heart_beat_exception;
    u32 aicore_bitmap; /* every bit identify one aicore, bit0 for core0, value 0 is ok */
    u32 ts_status;
};
#endif

struct devdrv_pid_map_sync {
    int op; /* 1 add, 0 del */
    pid_t pid;
    unsigned int chip_id;
    unsigned int vfid;
    pid_t host_pid;
    unsigned int cp_type;
    unsigned int resv[4];
};

struct devdrv_manager_msg_head {
    u32 dev_id;
    u32 msg_id;
    u16 valid;  /* validity judgement, 0x5A5A is valid */
    u16 result; /* process result from rp, zero for succ, non zero for fail */
    u32 tsid;
    u32 vfid;
};

#if defined(CFG_SOC_PLATFORM_MINI) && !defined(CFG_SOC_PLATFORM_MINIV2) && !defined(DEVMNG_UT) && !defined(LOG_UT)
#define DEVDRV_MANAGER_INFO_LEN 512UL
#else
#define DEVDRV_MANAGER_INFO_LEN 512UL
#endif

#define DEVDRV_MANAGER_INFO_PAYLOAD_LEN (DEVDRV_MANAGER_INFO_LEN - sizeof(struct devdrv_manager_msg_head))
struct devdrv_manager_msg_info {
    struct devdrv_manager_msg_head header;
    u8 payload[DEVDRV_MANAGER_INFO_PAYLOAD_LEN];
};

struct devdrv_manager_msg_resource_info {
    u32 vfid;
    u32 owner_type;
    u32 owner_id;
    u32 info_type;
    u64 value;
    u64 value_ext; /* Extended Value Variable */
    u64 resv[8];
};

struct devmng_msg_h2d_info {
    u64 cpu_system_count;
    u64 monotonic_raw_time_ns;
    u32 ffts_type;
    u64 computing_power[DEVDRV_MAX_COMPUTING_POWER_TYPE];
    u64 resv[4];
};

#define PROCESS_SIGN_LENGTH 49
#define PROCESS_RESV_LENGTH 4
struct process_sign {
    pid_t tgid;
    char sign[PROCESS_SIGN_LENGTH];
    char resv[PROCESS_RESV_LENGTH];
};

struct devdrv_ts_log {
    u32 devid;
    u32 mem_size;
    dma_addr_t dma_addr;
};

struct devdrv_dev_log {
    u32 devid;
    u32 mem_size;
    dma_addr_t dma_addr;
    u32 log_type;
};

#endif
