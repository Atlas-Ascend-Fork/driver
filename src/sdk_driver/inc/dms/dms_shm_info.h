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

#ifndef _DEVMNG_SHM_INFO_H_
#define _DEVMNG_SHM_INFO_H_

#define DEVMNG_SHM_INFO_HEAD_LEN 32
#define DEVMNG_SHM_INFO_ERROR_CODE_LEN 32
#define DEVMNG_SHM_INFO_EVENT_CODE_LEN 128
#define DEVMNG_SHM_INFO_HEAD_MAGIC 0x5a5a5a5a

#define DEVMNG_SHM_INFO_DIE_ID_NUM 5
#define VMNG_VDEV_MAX_PER_PDEV 17

#define DEVDRV_SHM_TOTAL_SIZE_PF 0x40000 /* 256KB */
#define DEVDRV_SHM_TOTAL_SIZE_VF 0x8000  /* 32KB */
#define DEVDRV_SHM_TOTAL_VF_OFFSET DEVDRV_SHM_TOTAL_SIZE_PF

#define DEVDRV_SHM_GEGISTER_RAO_SIZE 0x4000 /* 16KB */

/*
 * version of share memory:
 * bit 0~15: substantial version of head_info;
 * bit16~31: substantial version of soc_info;
 * bit32~47: substantial version of board_info;
 * bit48~64: substantial version of status_info;
 */
#define DEVMNG_SHM_INFO_HEAD_VERSION (0x1ULL << 0 | 0x1ULL << 16 | 0x1ULL << 32 | 0x2ULL << 48)

struct shm_event_code {
    u32 event_code;
    u8 fid;
};

typedef union shm_info_head {
    struct {
        u32 magic; /* indicates whether the functional area is valid; the value is 0x5a5a5a5a when valid */
        u32 offset_soc;
        u32 offset_board;
        u32 offset_status;
        u32 offset_heartbeat;
        u64 version;
    } head_info;
    char s8_union[DEVMNG_SHM_INFO_HEAD_LEN];
} U_SHM_INFO_HEAD;

typedef struct shm_info_soc {
    u16 die_id[DEVMNG_SHM_INFO_DIE_ID_NUM];
    u16 chip_info;
    u16 aicore_count;
    u16 cpu_count;
} U_SHM_INFO_SOC;

typedef struct shm_info_board {
    u16 board_id;
    u16 pcb_ver;
    u16 board_type;
    u16 slot_id;
    u16 venderid;    /* vendor id */
    u16 subvenderid; /* vendor sub id */
    u16 deviceid;    /* device id */
    u16 subdeviceid; /* device sub id */
    u16 bus;         /* bus number */
    u16 device;      /* device physical number */
    u16 fn;          /* equipment function number */
    u16 davinci_id;  /* device id */
} U_SHM_INFO_BOARD;

typedef struct shm_info_status {
    u16 os_status;
    u16 health_status;
    int error_cnt;
    u32 error_code[DEVMNG_SHM_INFO_ERROR_CODE_LEN];
    u16 dms_health_status[VMNG_VDEV_MAX_PER_PDEV];
    int event_cnt;
    struct shm_event_code event_code[DEVMNG_SHM_INFO_EVENT_CODE_LEN];
} U_SHM_INFO_STATUS;

#define DEVMNG_HEART_BEAT_MAGIC 0x5a5a5a5a
#define DEVMNG_HEART_BEAT_NO_LOST 0x0
#define DEVMNG_HEART_BEAT_LOST 0x1
typedef struct shm_info_heartbeat {
    u32 magic;
    u32 heartbeat_lost_flag;
    u64 heartbeat_cnt;
} U_SHM_INFO_HEARTBEAT;

#endif
