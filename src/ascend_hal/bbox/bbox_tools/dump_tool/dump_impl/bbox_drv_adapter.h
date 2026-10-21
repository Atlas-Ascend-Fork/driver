/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BBOX_DRV_ADAPTER_H
#define BBOX_DRV_ADAPTER_H

#include "bbox_int.h"
#include "ascend_hal.h"

drvError_t bbox_drv_memory_dump(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_memory_dump_read(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_memory_dump_cdr(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_memory_dump_read_cdr(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_memory_dump_run_os_log(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_memory_dump_debug_os_log(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_memory_dump_debug_dev_log(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_memory_dump_run_event_log(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_memory_dump_sec_log(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_pcie_vmcore_read(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_vmcore_read(u32 phy_id, u32 offset, u32 size, u8 *buffer);
drvError_t bbox_drv_dma_ts_log_ddr_read(u32 phy_id, u32 offset, u32 size, u8 *buffer);

drvError_t bbox_drv_pcie_sram_read(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_pcie_klog_ddr_read(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_klog_ddr_read(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_pcie_ts_log_ddr_read(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_pcie_hdr_ddr_read(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_pcie_cdr_ddr_read(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_pcie_hboot_read(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_pcie_vmcore_stat_read(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_vmcore_stat_read(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_memory_pcie_dump(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_memory_dump_read_data(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_memory_pcie_dump_cdr(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_memory_dump_cdr_read(u32 phy_id, u32 offset, u8 *value, u32 len);

drvError_t bbox_drv_get_master_dev_id(u32 phy_id, u32 *master_id);

drvError_t bbox_drv_pcie_kdump_write(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_kdump_write(u32 phy_id, u32 offset, u8 *value, u32 len);
drvError_t bbox_drv_read_data(u32 phy_id, MEM_CTRL_TYPE mem_type, u32 offset, u8 *buf, u32 size);
drvError_t bbox_drv_write_data(u32 phy_id, MEM_CTRL_TYPE mem_type, u32 offset, u8 *buf, u32 size);

#endif /* BBOX_DRV_ADAPTER_H */
