/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bbox_drv_adapter.h"
#include "bbox_feature.h"
#include "bbox_print.h"
#include "pbl_user_interface.h"
#include "pbl_urd_main_cmd_def.h"
#include "pbl_urd_sub_cmd_common.h"

/**
 * @brief       use drvMemRead type MEM_TYPE_BBOX_DDR to dump device memory via DMA
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return drvMemRead(phy_id, MEM_TYPE_BBOX_DDR, offset, buffer, size);
}

/**
 * @brief       use bbox_drv_read_data type MEM_TYPE_BBOX_DDR to dump device memory via URD cmd protocol
 *              (David/cloud v4 platform version, Milan counterpart uses drvMemRead)
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump_read(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_BBOX_DDR, offset, buffer, size);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_BBOX_PCIE_BAR to dump device memory via PCIe
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to PCIe
 * @param [in]  size:   buffer length (always <= 512K)
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_pcie_dump(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return drvMemRead(phy_id, MEM_TYPE_BBOX_PCIE_BAR, offset, value, len);
}

/**
 * @brief       use bbox_drv_read_data type MEM_TYPE_BBOX_PCIE_BAR to dump device memory via URD cmd protocol
 *              (David/cloud v4 platform version, Milan counterpart uses drvMemRead)
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to PCIe
 * @param [in]  size:   buffer length (always <= 512K)
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump_read_data(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_BBOX_PCIE_BAR, offset, value, len);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_REG_DDR to dump device memory via DMA
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump_cdr(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return drvMemRead(phy_id, MEM_TYPE_REG_DDR, offset, buffer, size);
}

/**
 * @brief       use bbox_drv_read_data type MEM_TYPE_REG_DDR to dump device memory via URD cmd protocol
 *              (David/cloud v4 platform version, Milan counterpart uses drvMemRead)
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump_read_cdr(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_REG_DDR, offset, buffer, size);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_CHIP_LOG_PCIE_BAR to dump device memory via PCIe
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to PCIe
 * @param [in]  size:   buffer length (always <= 512K)
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_pcie_dump_cdr(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return drvMemRead(phy_id, MEM_TYPE_CHIP_LOG_PCIE_BAR, offset, value, len);
}

/**
 * @brief       use bbox_drv_read_data type MEM_TYPE_CHIP_LOG_PCIE_BAR to dump device memory via URD cmd protocol
 *              (David/cloud v4 platform version, Milan counterpart uses drvMemRead)
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to PCIe
 * @param [in]  size:   buffer length (always <= 512K)
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump_cdr_read(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_CHIP_LOG_PCIE_BAR, offset, value, len);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_PCIE_SRAM to read data from SRAM via PCIe
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: offset in PCIe Bar space
 * @param [out] value:  buffer to read data in
 * @param [in]  len:    buffer length (always <= 512K)
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_pcie_sram_read(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return drvMemRead(phy_id, MEM_TYPE_PCIE_SRAM, offset, value, len);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_PCIE_DDR to read data from DDR via PCIe
 * @param [in]  dev_id:  device physical id
 * @param [in]  offset: offset in PCIe Bar space
 * @param [out] value:  buffer to read data in
 * @param [in]  len:    buffer length (always <= 512K)
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_pcie_klog_ddr_read(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return drvMemRead(phy_id, MEM_TYPE_PCIE_DDR, offset, value, len);
}

/**
 * @brief       use bbox_drv_read_data type MEM_TYPE_PCIE_DDR to read kernel log from DDR via URD cmd protocol
 *              (David/cloud v4 platform version, Milan counterpart uses drvMemRead)
 * @param [in]  dev_id:  device physical id
 * @param [in]  offset: offset in PCIe Bar space
 * @param [out] value:  buffer to read data in
 * @param [in]  len:    buffer length (always <= 512K)
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_klog_ddr_read(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_PCIE_DDR, offset, value, len);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_TS_LOG to read data from DDR via DMA
 * @param [in]  dev_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [out] value:  buffer to read data in
 * @param [in]  len:    buffer length
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_dma_ts_log_ddr_read(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return drvMemRead(phy_id, MEM_TYPE_TS_LOG, offset, buffer, size);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_TS_LOG_PCIE_BAR to read data from DDR via PCIe
 * @param [in]  dev_id:  device physical id
 * @param [in]  offset: physical addr offset in device to PCIe
 * @param [out] value:  buffer to read data in
 * @param [in]  len:    buffer length (always <= 512K)
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_pcie_ts_log_ddr_read(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return drvMemRead(phy_id, MEM_TYPE_TS_LOG_PCIE_BAR, offset, value, len);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_RUN_OS_LOG to read data from DDR via DMA
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump_run_os_log(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return drvMemRead(phy_id, MEM_TYPE_RUN_OS_LOG, offset, buffer, size);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_DEBUG_OS_LOG to read data from DDR via DMA
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump_debug_os_log(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return drvMemRead(phy_id, MEM_TYPE_DEBUG_OS_LOG, offset, buffer, size);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_DEBUG_DEV_LOG to read data from DDR via DMA
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump_debug_dev_log(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return drvMemRead(phy_id, MEM_TYPE_DEBUG_DEV_LOG, offset, buffer, size);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_RUN_EVENT_LOG to read data from DDR via DMA
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump_run_event_log(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return drvMemRead(phy_id, MEM_TYPE_RUN_EVENT_LOG, offset, buffer, size);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_SEC_LOG to read data from DDR via DMA
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_memory_dump_sec_log(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return drvMemRead(phy_id, MEM_TYPE_SEC_LOG, offset, buffer, size);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_BBOX_HDR
 *              to read at most 512B data from 512K hdr data
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: read addr offset
 * @param [in]  value:  buffer to store read data
 * @param [in]  len:    read length
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_pcie_hdr_ddr_read(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return drvMemRead(phy_id, MEM_TYPE_BBOX_HDR, offset, value, len);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_REG_SRAM
 *              to read at most 512B data from 48K cdr data
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: read addr offset
 * @param [in]  value:  buffer to store read data
 * @param [in]  len:    read length
 * @return      0 on success otherwise is fail
 */
drvError_t bbox_drv_pcie_cdr_ddr_read(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return drvMemRead(phy_id, MEM_TYPE_REG_SRAM, offset, value, len);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_HBOOT_SRAM
 *              to read at most 512B data from 48K cdr data
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: read addr offset
 * @param [in]  value:  buffer to store read data
 * @param [in]  len:    read length
 * @return      0 on success otherwise is fail
 */
drvError_t bbox_drv_pcie_hboot_read(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return drvMemRead(phy_id, MEM_TYPE_HBOOT_SRAM, offset, value, len);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_VMCORE_FILE to dump device memory via DMA
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_pcie_vmcore_read(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return drvMemRead(phy_id, MEM_TYPE_VMCORE_FILE, offset, buffer, size);
}

/**
 * @brief       use bbox_drv_read_data type MEM_TYPE_VMCORE_FILE to dump vmcore data via URD cmd protocol
 *              (David/cloud v4 platform version, Milan counterpart uses drvMemRead)
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: physical addr offset in device to DMA
 * @param [in]  size:   buffer length
 * @param [out] buffer: buffer to store data in
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_vmcore_read(u32 phy_id, u32 offset, u32 size, u8 *buffer)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_VMCORE_FILE, offset, buffer, size);
}

/**
 * @brief       use drvMemRead type MEM_TYPE_VMCORE_STAT
 *              to read vmcore status and size
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: read addr offset
 * @param [in]  value:  buffer to store read data
 * @param [in]  len:    read length
 * @return      0 on success otherwise is fail
 */
drvError_t bbox_drv_pcie_vmcore_stat_read(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return drvMemRead(phy_id, MEM_TYPE_VMCORE_STAT, offset, value, len);
}

/**
 * @brief       use bbox_drv_read_data type MEM_TYPE_VMCORE_STAT to read vmcore status via URD cmd protocol
 *              (David/cloud v4 platform version, Milan counterpart uses drvMemRead)
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: read addr offset
 * @param [in]  value:  buffer to store read data
 * @param [in]  len:    read length
 * @return      0 on success otherwise is fail
 */
drvError_t bbox_drv_vmcore_stat_read(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_VMCORE_STAT, offset, value, len);
}

/**
 * @brief       use drvMemWrite type MEM_TYPE_KDUMP_MAGIC
 *              to write kdump flag to device
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: write addr offset
 * @param [in]  value:  buffer to store write data
 * @param [in]  len:    write length
 * @return      0 on success otherwise is fail
 */
drvError_t bbox_drv_pcie_kdump_write(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return drvMemWrite(phy_id, MEM_TYPE_KDUMP_MAGIC, offset, value, len);
}

/**
 * @brief       use bbox_drv_write_data type MEM_TYPE_KDUMP_MAGIC to write kdump flag via URD cmd protocol
 *              (David/cloud v4 platform version, Milan counterpart uses drvMemWrite)
 * @param [in]  phy_id:  device physical id
 * @param [in]  offset: write addr offset
 * @param [in]  value:  buffer to store write data
 * @param [in]  len:    write length
 * @return      0 on success otherwise is fail
 */
drvError_t bbox_drv_kdump_write(u32 phy_id, u32 offset, u8 *value, u32 len)
{
    return bbox_drv_write_data(phy_id, MEM_TYPE_KDUMP_MAGIC, offset, value, len);
}

/**
 * @brief       get matser device id in same os
 * @param [in]  phy_id:  device physical id
 * @param [out] status: boot-up status
 * @return      0 on success otherwise error code
 */
drvError_t bbox_drv_get_master_dev_id(u32 phy_id, u32 *master_id)
{
    int64_t value = 0;
    drvError_t ret = halGetDeviceInfo(phy_id, MODULE_TYPE_SYSTEM, INFO_TYPE_MASTERID, &value);
    if (ret == DRV_ERROR_NONE) {
        *master_id = (u32)(int32_t)value; // valid devid < 64, not afraid of loss value
    }

    return ret;
}

drvError_t bbox_drv_read_data(u32 phy_id, MEM_CTRL_TYPE mem_type, u32 offset, u8 *buf, u32 size)
{
    struct urd_cmd_para cmd_para = {0};
    bbox_data_info_t bbox_data = {0};
    struct urd_cmd cmd = {0};
    drvError_t ret;

    bbox_data.dev_id = phy_id;
    bbox_data.type = mem_type;
    bbox_data.offset = offset;
    bbox_data.len = size;
    bbox_data.bbox_data_buf = buf;
    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BBOX, DMS_SUBCMD_GET_BBOX_DATA, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&bbox_data, sizeof(bbox_data_info_t), NULL, 0);
    ret = (drvError_t)urd_dev_usr_cmd(phy_id, &cmd, &cmd_para);
    if (ret != DRV_ERROR_NONE) {
        if ((ret != DRV_ERROR_NOT_SUPPORT) && (ret != EOPNOTSUPP)) {
            BBOX_ERR("Read bbox data failed. (ret=%d; offset=0x%x, size=0x%x)", (int)ret, offset, size);
        }
    }
    return ret;
}

drvError_t bbox_drv_write_data(u32 phy_id, MEM_CTRL_TYPE mem_type, u32 offset, u8 *buf, u32 size)
{
    struct urd_cmd_para cmd_para = {0};
    bbox_data_info_t bbox_data = {0};
    struct urd_cmd cmd = {0};
    drvError_t ret;

    bbox_data.dev_id = phy_id;
    bbox_data.type = mem_type;
    bbox_data.offset = ((mem_type != MEM_TYPE_KDUMP_MAGIC) ? (offset) :
                                                             (0xBA0000U)); // equal to BBOX_HBM_VMCORE_FLAG_OFFSET
    bbox_data.len = size;
    bbox_data.bbox_data_buf = buf;
    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BBOX, DMS_SUBCMD_SET_BBOX_DATA, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&bbox_data, sizeof(bbox_data_info_t), NULL, 0);
    ret = (drvError_t)urd_dev_usr_cmd(phy_id, &cmd, &cmd_para);
    if (ret != DRV_ERROR_NONE) {
        if ((ret != DRV_ERROR_NOT_SUPPORT) && (ret != EOPNOTSUPP)) {
            BBOX_ERR("Write bbox data failed. (ret=%d; offset=0x%x, size=0x%x)", (int)ret, offset, size);
        }
    }
    return ret;
}