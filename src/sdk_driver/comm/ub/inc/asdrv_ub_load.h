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

#ifndef ASDRV_UB_LOAD_H
#define ASDRV_UB_LOAD_H

#include "ka_memory_pub.h"
#include "ka_common_pub.h"
#include "ka_task_pub.h"
#include "ka_system_pub.h"
#include <ub/ubus/ubus.h>
#include <linux/ummu_core.h>
#include "ubcore_types.h"
#include "cis.h"
#include "securec.h"

typedef struct ubdrv_uvb_msg_ctrl {
    u32 call_id;
    u32 receiver_id;
    int (*func)(struct cis_message *msg);
} ubdrv_uvb_msg_ctrl_t;

struct ubdrv_load_file_cfg *ubdrv_get_load_file_cfg(void);
int ubdrv_load_device_init(void);
void ubdrv_load_device_uninit(void);

#define UBDRV_LOAD_HOST_READY 0x11111111U      // driver set:init finish
#define UBDRV_BIOS_REQUIRE_FILE 0x22222222U    // bios set:request a file
#define UBDRV_LOAD_FILE_PART_READY 0x33333333U // driver set:file prepare part ready

#define UBDRV_LOAD_FILE_READY 0x44444444U        // driver set:file prepare ready
#define UBDRV_BIOS_LOAD_FILE_FINISH 0x55555555U  // bios set:single or part file load finish
#define UBDRV_BIOS_LOAD_FILE_SUCCESS 0x66666666U // bios set:all files load finish

// DFX
#define UBDRV_LOAD_NO_SUCH_FILE 0XAAAAAAAAU      // driver set: no such file
#define UBDRV_BIOS_LOAD_FILE_FAILED 0XBBBBBBBBU  // bios set: load failed
#define UBDRV_BIOS_CHECK_FILE_FAILED 0XCCCCCCCCU // bios set: file check failed

#define DEVDRV_HOST_FILE_PATH "/home/bios"

#define UBDRV_STR_MAX_LEN 128ULL
#define UBDRV_MAX_FILE_SIZE (48 * 1024ul * 1024ul) // 48MB

#define UBDRV_LOAD_FILE_CHECK_TIME 20 // 20ms
#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define UBDRV_TIMER_SCHEDULE_TIMES 1800 // 30min
#define UBDRV_LOAD_WAIT_CNT 18000       // 18000 * 20ms = 6min
#else
#define UBDRV_TIMER_SCHEDULE_TIMES 300 // 5min
#define UBDRV_LOAD_WAIT_CNT 3000       // 3000 * 20ms = 1min
#endif

#define UBDRV_TIMER_EXPIRES (1 * KA_HZ)
#define UBDRV_WAIT_LOAD_FILE_TIME 600 // 10min

#define UBDRV_BLOCKS_NAME_SIZE 32
#define UBDRV_BLOCKS_ADDR_PAIR_NUM 1

#define UBDRV_UB_MEMORY_MASK KA_BASE_BIT(0)
#define UBDRV_UB_MESSAGE_MASK KA_BASE_BIT(1)

#define UBDRV_CLOUD_V4_DEVICE_ID 0xd806
#define UBDRV_CLOUD_V5_DEVICE_ID 0xd807
#define UBDRV_MINI_V4_DEVICE_ID 0xd808
#define UBDRV_MINI_V5_DEVICE_ID 0xd200

struct ubdrv_load_addr_pair {
    void *addr;
    ka_dma_addr_t dma_addr;
    u64 size;      /* block size */
    u64 data_size; /* data length is this block */
};

struct ubdrv_load_blocks {
    char name[UBDRV_BLOCKS_NAME_SIZE];
    u64 blocks_num;
    u64 blocks_valid_num;
    struct ubdrv_load_addr_pair blocks_addr[UBDRV_BLOCKS_ADDR_PAIR_NUM];
};

#pragma pack(4)
struct ubdrv_load_sram_info {
    u32 flag;
    u32 file_id;
    u32 resv0; // add resv*3 for access sram 64bit dma_addr(s) must 64bit align
    u32 resv1;
    u32 resv2;
    u32 eid;
    u32 cna;
    u32 upi;
    u32 tid;
    u32 block_num; // must at last; followed by 64bit dma_addr(s)
};
#pragma pack()

struct ascend_ub_dev;
struct ascend_ub_ctrl;
struct ubdrv_loader {
    struct ascend_ub_dev *udev;
    void __ka_mm_iomem *mem_sram_base;
    ka_device_t *ummu_tdev;
    ka_iommu_sva_t *sva;
    struct ubdrv_load_blocks *blocks;
    u64 translated_size;
    loff_t remain_size;
    loff_t load_offset;
    u32 part_num;
    u32 tid;
    int load_timeout;
    bool load_abort;
    char file_path[UBDRV_STR_MAX_LEN];
};

struct ubdrv_load_work {
    struct ascend_ub_dev *udev;
    ka_work_struct_t work;
};

struct ubdrv_timer_work {
    struct ascend_ub_dev *udev;
    ka_timer_list_t load_timer; /* device os load time out timer */
    int timer_remain;           /* timer_remain <= 0 means time out */
    u32 timer_expires;
};

struct devdrv_work {
    struct ascend_ub_ctrl *ub_ctrl;
    struct ascend_ub_dev *udev;
    ka_work_struct_t work;
};

#define UBDRV_LOAD_FILE_NUM 0x20
#define UBDRV_NON_CRITICAL_FILE 0
#define UBDRV_CRITICAL_FILE 1
struct ubdrv_load_file_cfg {
    u32 id;
    char *file_name;
    u8 file_type;
};

/* uvb load msg cmd type */
enum ubdrv_uvb_msg_type {
    UBDRV_UVB_BIOS_REQUIRE_FILE = 0,  // bios request a file
    UBDRV_UVB_BIOS_LOAD_FILE_SUCCESS, // bios finish load all files
    UBDRV_UVB_BIOS_LOAD_FILE_FAILED,
    UBDRV_UVB_CMD_MAX
};

#define UBDRV_UVB_LOAD_INPUT_RESV 3

// Call id: 0xC00B0021
struct ubdrv_uvb_get_ability_input {
    u32 eid;
    u32 version;
    u32 device_id;
};

struct ubdrv_uvb_get_ability_output {
    u32 capability;
};

#define UBDRV_UVB_INVALID_FILE_ID -1
// Call id: 0xC00B0022
struct ubdrv_uvb_get_file_info_input {
    u32 server_eid;
    u32 client_cna;
    u32 client_eid;
    u32 type;
    char file_name[UBDRV_STR_MAX_LEN]; // file_id
    u32 module_id;
    u32 slot_id;
};

struct ubdrv_uvb_get_file_info_output {
    u64 file_address;
    u64 file_total_size;
    u32 crc;
    u32 file_id;
    u32 token_id;
    u32 section_size;
};

// Call id: 0xC00B0025
struct ubdrv_uvb_get_loading_state_input {
    u32 server_eid;
    u32 client_cna;
    u32 client_eid;
    u32 file_id;
    u64 offset;
    u32 state;
    u32 module_id;
    u32 slot_id;
};

// Call id: 0xC00B2803
struct ubdrv_uvb_get_final_state_input {
    u32 server_eid;
    u32 slot_id;
    u32 module_id;
    u32 state;
};

void ubdrv_load_file(ka_work_struct_t *p_work);
void ubdrv_load_exit(struct ascend_ub_dev *udev);
int ubdrv_sdk_path_init(void);
void ubdrv_free_load_segment(u32 dev_id, struct ubdrv_loader *loader);
int ubdrv_alloc_load_segment(u32 dev_id, struct ubdrv_loader *loader, u32 seg_size);
int ubdrv_single_file_path_init(char *file_path, u32 len, const u32 file_id);
int ubdrv_load_get_file_size(u32 dev_id, const char *file_name, u32 file_id, ka_file_t **p_file, loff_t *file_size);

#endif