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

#ifndef __DMS_BBOX_H
#define __DMS_BBOX_H

#include "pbl_uda.h"

#define DMS_MODULE_BBOX_DUMP "dms_bbox_dump"

#ifndef UNUSED
#define UNUSED(x)  \
    do {           \
        (void)(x); \
    } while (0)
#endif

#define KLOG_BASE_ADDR 0x17B00000U
#define KLOG_TOTAL_SIZE 0x200000U // 2M

#define DDR_BASE_ADDR 0x17D00000U
#define DDR_BASE_OFFSET 0x0U
#define DDR_HDR_OFFSET 0x920000U
#define DDR_REG_OFFSET 0xA00000U
#define DDR_TOTAL_SIZE 0x1E00000U // 30MB

#define KDUMP_MAGIC_OFFSET 0x9A0000U
#define VMCORE_STAT_OFFSET 0x9A0004U

#define SRAM_BASE_ADDR 0xE8000000U
#define SRAM_BASE_ADDR_V5 0x45EB000000ULL
#define SRAM_TOTAL_SIZE 0x40000U // 256k

#define BBOX_DONE 1
#define BBOX_UNDONE 0

#define MEM_TYPE_DEBUG_OS_LOG_LEN (0x300000U)
#define MEM_TYPE_SEC_LOG_LEN (0x200000U)
#define MEM_TYPE_RUN_OS_LOG_LEN (0x300000U)
#define MEM_TYPE_RUN_EVENT_LOG_LEN (0x200000U)
#define MEM_TYPE_DEBUG_DEV_LOG_LEN (0x1400000UL)

#define BBOX_DUMP_TS_LOG_ADDRESS 0x5040000UL
#define BBOX_DUMP_TS_LOG_SIZE 0xC0000UL

#define BBOX_DUMP_IMP_LOG_ADDRESS 0x15B00000UL
#define BBOX_DUMP_IMP_LOG_SIZE 0x200000UL

#define BBOX_DUMP_IMU_LOG_ADDRESS 0x18580000UL
#define BBOX_DUMP_IMU_LOG_SIZE 0x80000UL

#define BBOX_DUMP_HSM_LOG_ADDRESS 0x4400000UL
#define BBOX_DUMP_HSM_LOG_SIZE 0x100000UL

/* The enumeration definition must be the same as that of MEM_CTRL_TYPE in ascend_hal.h */
enum bbox_data_type {
    MEM_TYPE_PCIE_SRAM = 0,
    MEM_TYPE_PCIE_DDR,
    MEM_TYPE_IMU_DDR,
    MEM_TYPE_BBOX_DDR,
    MEM_TYPE_BBOX_HDR,
    MEM_TYPE_REG_SRAM,
    MEM_TYPE_REG_DDR,
    MEM_TYPE_TS_LOG,
    MEM_TYPE_HBOOT_SRAM,
    MEM_TYPE_DEBUG_OS_LOG,
    MEM_TYPE_SEC_LOG,
    MEM_TYPE_RUN_OS_LOG,
    MEM_TYPE_RUN_EVENT_LOG,
    MEM_TYPE_DEBUG_DEV_LOG,
    MEM_TYPE_KDUMP_MAGIC,
    MEM_TYPE_VMCORE_FILE,
    MEM_TYPE_VMCORE_STAT,
    MEM_TYPE_CHIP_LOG_PCIE_BAR,
    MEM_TYPE_TS_LOG_PCIE_BAR,
    MEM_TYPE_BBOX_PCIE_BAR,
    MEM_TYPE_IMP_LOG,
    MEM_TYPE_IMU_LOG,
    MEM_TYPE_HSM_LOG,
    MEM_CTRL_TYPE_MAX,
};

struct dev_mem_region {
    void *va;
    void *(*alloc_func)(unsigned long addr, unsigned int size);
    void (*free_func)(struct dev_mem_region *region);
    uint64_t base_addr;
    unsigned int size;
    bool reg_flag;
};

struct persistent_export_region {
    void *base_addr;
    struct dev_mem_region
        booked; // Memory region reserved for: MEM_TYPE_DEBUG_OS_LOG, MEM_TYPE_RUN_OS_LOG, MEM_TYPE_RUN_EVENT_LOG
    struct dev_mem_region sec_log;       // Dynamic alloc
    struct dev_mem_region debug_dev_log; // Dynamic alloc
};

struct mem_mng {
    ka_rw_semaphore_t sem;
    struct dev_mem_region klog;
    struct dev_mem_region ddr;
    struct dev_mem_region sram;
    struct persistent_export_region persistent;
    u32 uda_init_flag;
};

struct bbox_dump_type_info {
    u64 address;
    u64 size;
};

enum bbox_data_type *get_bbox_dump_type(u32 dev_id);
int get_bbox_dump_type_size(u32 dev_id);
struct bbox_dump_type_info *get_bbox_dump_info(void);
int get_bbox_dump_info_size(void);
struct mem_mng *bbox_mem_mng_get(unsigned int dev_id);
int dms_get_bbox_data(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
int dms_set_bbox_data(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len);
void *bbox_alloc_klog_mem(unsigned long addr, unsigned int size);
void bbox_free_klog_mem(struct dev_mem_region *region);
void *bbox_alloc_region_mem(unsigned long addr, unsigned int size);
void bbox_free_region_mem(struct dev_mem_region *region);
void bbox_uda_davinci_type_pack(struct uda_dev_type *type, bool is_init);
int bbox_register_persistent_export(u32 udevid, struct mem_mng *ctx);
int bbox_unregister_persistent_export(u32 udevid, struct mem_mng *ctx);
int dms_bbox_dump_init(void);
void dms_bbox_dump_uninit(void);
int bbox_set_mem_and_register(u32 udevid, struct mem_mng *ctx, u32 log_type, void *kva, u32 size);
void bbox_persistent_unregister_by_type(u32 udevid, struct mem_mng *ctx, u32 log_type);
int bbox_client_init(void);
void bbox_client_uninit(void);
int bbox_dump_ctx_init(u32 dev_id, u32 type);
void bbox_dump_ctx_uninit(u32 dev_id, u32 type);
int bbox_dma_addrs_init(u32 udevid);
void bbox_dma_addrs_uninit(u32 udevid);
int bbox_device_set_mem_platform(u32 udevid, u32 log_type, void *kva, u32 size);
void bbox_device_persistent_unregister(u32 udevid, u32 log_type);
int bbox_device_register_persistent_export(u32 udevid, struct mem_mng *ctx);
int bbox_device_unregister_persistent_export(u32 udevid, struct mem_mng *ctx);
#endif