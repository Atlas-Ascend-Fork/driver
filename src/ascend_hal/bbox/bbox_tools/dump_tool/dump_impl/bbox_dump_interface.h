/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BBOX_DUMP_INTERFACE_H
#define BBOX_DUMP_INTERFACE_H

#include "bbox_int.h"
#include "ascend_hal.h"

#define PCIE_BAR_DDR_MAXLEN 512
#define DMA_MEMDUMP_MAXLEN 0x100000UL
#define UB_DUMP_MAXLEN 256
#define BIOS_MAGIC 0xEAEA2020U

typedef enum bbox_dump_data_file {
    BBOX_DUMP_FILE_DDR_SRAM = 0,
    BBOX_DUMP_FILE_BIOS_SRAM,
    BBOX_DUMP_FILE_HDR,
    BBOX_DUMP_FILE_KLOG,
    BBOX_DUMP_FILE_TS_LOG,
    BBOX_DUMP_FILE_RUN_DEVICE_OS_LOG,
    BBOX_DUMP_FILE_DEBUG_DEVICE_OS_LOG,
    BBOX_DUMP_FILE_DEBUG_DEVICE_FW_LOG,
    BBOX_DUMP_FILE_RUN_EVENT_LOG,
    BBOX_DUMP_FILE_SEC_DEVICE_OS_LOG,
    BBOX_DUMP_FILE_DDR_DMA,
    BBOX_DUMP_FILE_IMU_BOOT,
    BBOX_DUMP_FILE_IMU_RUN,
    BBOX_DUMP_FILE_UEFI_BOOT,
    BBOX_DUMP_FILE_CDR_DDR,
    BBOX_DUMP_FILE_CDR_SRAM,
    BBOX_DUMP_FILE_CDR_CLINK,
    BBOX_DUMP_FILE_HBM_SRAM,
    BBOX_DUMP_FILE_SRAM_SNAPSHOT,
    BBOX_DUMP_FILE_BIOS_HISS,
    BBOX_DUMP_FILE_HBOOT,
    BBOX_DUMP_VMCORE_STAT,
    BBOX_DUMP_FILE_VMCORE,
    BBOX_DUMP_FILE_IMP_LOG,
    BBOX_DUMP_FILE_IMU_LOG,
    BBOX_DUMP_FILE_HSM_LOG,
    BBOX_DUMP_FILE_NUM,
    BBOX_DUMP_FILE_NONE = BBOX_DUMP_FILE_NUM
} bbox_dump_data_file_enum;

typedef drvError_t (*bbox_drv_dma_read)(u32 phy_id, u32 offset, u32 size, u8 *buffer);
typedef drvError_t (*bbox_drv_pcie_read)(u32 phy_id, u32 offset, u8 *buffer, u32 len);
typedef drvError_t (*bbox_drv_pcie_write)(u32 phy_id, u32 offset, u8 *buffer, u32 len);
typedef bbox_status (*bbox_drv_ub_sram_read)(u32 phy_id, u32 offset, u8 *buffer, u32 len);

bbox_status bbox_pcie_dump_sram_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_pcie_dump_hdr_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_pcie_dump_cdr_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_pcie_dump_cdr_full_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dump_cdr_full_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_pcie_dump_bbox_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dump_bbox_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_pcie_dump_hboot_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_pcie_dump_klog_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dump_klog_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_pcie_dump_ts_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_pcie_dump_vmcore_stat_read(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dump_vmcore_stat_read(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dma_dump_bbox_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dump_bbox_data_read(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dma_dump_cdr_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dump_cdr_data_read(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dma_dump_run_dev_os_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dma_dump_debug_dev_os_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dma_dump_debug_dev_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dma_dump_run_event_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dma_dump_sec_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dma_dump_vmcore_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dma_dump_vmcore_data_read(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dma_dump_ts_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_dump_sram_data(u32 phy_id, u32 offset, u32 size, u8 *buf);

bbox_status bbox_pcie_set_kdump_flag(u32 phy_id, u32 offset, u32 size, u8 *buf);
bbox_status bbox_set_kdump_flag(u32 phy_id, u32 offset, u32 size, u8 *buf);

#endif
