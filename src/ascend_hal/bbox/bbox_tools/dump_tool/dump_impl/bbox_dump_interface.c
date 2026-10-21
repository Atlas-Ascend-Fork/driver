/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "bbox_dump_interface.h"
#include "bbox_dump_process.h"
#include "bbox_drv_adapter.h"
#include "bbox_print.h"

/**
 * @brief       read specified data of specified device id from pcie
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buffer:      buffer to store read data
 * @param [in]  read_func:    function pointer
 * @return      == -1 failed, == 0 success
 */
STATIC bbox_status bbox_pcie_dump_data(u32 phy_id, u32 offset, u32 size, u8 *buffer, const bbox_drv_pcie_read read_func)
{
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(read_func, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(size == 0, return BBOX_FAILURE, "%u", size);

    u32 packet_len, i;
    u32 surplus = size % PCIE_BAR_DDR_MAXLEN;
    u32 packet_num = (surplus == 0) ? 0 : 1;
    packet_num += size / PCIE_BAR_DDR_MAXLEN;

    for (i = 0; i < packet_num; i++) {
        u32 packet_offset = i * PCIE_BAR_DDR_MAXLEN;
        if (i < (packet_num - 1)) {
            packet_len = PCIE_BAR_DDR_MAXLEN;
        } else {
            packet_len = ((surplus == 0) ? PCIE_BAR_DDR_MAXLEN : surplus);
        }
        drvError_t ret = read_func(phy_id, offset + packet_offset, &buffer[packet_offset], packet_len);
        if (ret != DRV_ERROR_NONE) {
            if (ret == DRV_ERROR_NOT_SUPPORT) {
                /* to handle the hboot log */
                return ret;
            }
            BBOX_ERR("Get data failed with %d. offset[0x%x], size[0x%x]", (int)ret, offset, size);
            return BBOX_FAILURE;
        }
    }

    return BBOX_SUCCESS;
}

/**
 * @brief       read bbox data of specified device id from dma
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also is buffer size
 * @param [out] buffer:      buffer to store read data
 * @param [in]  read_func:    function pointer
 * @return      == -1 failed, == 0 success
 */
STATIC bbox_status bbox_dma_dump_data(u32 phy_id, u32 offset, u32 size, u8 *buffer, const bbox_drv_dma_read read_func)
{
    const u32 blck = DMA_MEMDUMP_MAXLEN; // 1MB

    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(read_func, return BBOX_FAILURE);
    if (size < blck) {
        BBOX_ERR("size is too small. (size=%u, min_size=%u)", size, blck);
        return BBOX_FAILURE;
    }

    u32 l;
    u8 *buf = buffer;
    u32 start = offset;
    u32 length = size & (~blck + 1); // 1MB align
    for (l = 0; l < length && l < size; l += blck, start += blck, buf += blck) {
        drvError_t ret = read_func(phy_id, start, blck, buf);
        if (ret != DRV_ERROR_NONE) {
            BBOX_ERR("dump ddr data failed(%d)", (int)ret);
            break;
        }
    }
    if (l != size) {
        return BBOX_FAILURE;
    }

    return BBOX_SUCCESS;
}

STATIC bbox_status bbox_read_sram_data(u32 phy_id, u32 offset, u32 size, u8 *buffer,
                                       const bbox_drv_ub_sram_read read_func)
{
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(read_func, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(size == 0, return BBOX_FAILURE, "%u", size);

    u32 packet_len, i;
    u32 surplus = size % UB_DUMP_MAXLEN;
    u32 packet_num = (surplus == 0) ? 0 : 1;
    packet_num += size / UB_DUMP_MAXLEN;

    for (i = 0; i < packet_num; i++) {
        u32 packet_offset = i * UB_DUMP_MAXLEN;
        if (i < (packet_num - 1)) {
            packet_len = UB_DUMP_MAXLEN;
        } else {
            packet_len = ((surplus == 0) ? UB_DUMP_MAXLEN : surplus);
        }
        bbox_status ret = read_func(phy_id, offset + packet_offset, &buffer[packet_offset], packet_len);
        if (ret != DRV_ERROR_NONE) {
            if (ret == DRV_ERROR_NOT_SUPPORT) {
                return ret;
            }
            BBOX_ERR("Get sram data failed with %d. offset[0x%x], size[0x%x]", (int)ret, offset, size);
            return BBOX_FAILURE;
        }
    }

    return BBOX_SUCCESS;
}

STATIC bbox_status bbox_drv_read_sram_data(u32 phy_id, u32 offset, u8 *buf, u32 size)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_PCIE_SRAM, offset, buf, size);
}

bbox_status bbox_dump_sram_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_read_sram_data(phy_id, offset, size, buf, bbox_drv_read_sram_data);
}

/**
 * @brief       read sram data through pcie
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_pcie_dump_sram_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_pcie_sram_read);
}

/**
 * @brief       read hdr data through pcie
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_pcie_dump_hdr_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    bbox_status ret;
    u32 magic = 0;

    ret = bbox_pcie_dump_data(phy_id, offset, sizeof(magic), (u8 *)&magic, bbox_drv_pcie_hdr_ddr_read);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR("bbox_pcie_dump_data failed. (ret=%d)", ret);
        return BBOX_FAILURE;
    }

    if (magic != BIOS_MAGIC) {
        BBOX_INF("magic has not been initialized. (magic=0x%x)", magic);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_pcie_hdr_ddr_read);
}

/**
 * @brief       read Cdr data through pcie
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_pcie_dump_cdr_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_pcie_cdr_ddr_read);
}

/**
 * @brief       read Sram bin data through pcie
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_pcie_dump_hboot_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_pcie_hboot_read);
}

/**
 * @brief       read kernel log data through pcie
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_pcie_dump_klog_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_pcie_klog_ddr_read);
}

/**
 * @brief       read kernel log data through URD cmd protocol (David platform version)
 *              Milan counterpart bbox_pcie_dump_klog_data uses drvMemRead
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dump_klog_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_klog_ddr_read);
}

/**
 * @brief       read ts log data through pcie
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_pcie_dump_ts_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_pcie_ts_log_ddr_read);
}

/**
 * @brief       read ts log data through dma
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dma_dump_ts_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_dma_ts_log_ddr_read);
}

/**
 * @brief       read run device os log data through dma
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dma_dump_run_dev_os_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump_run_os_log);
}

/**
 * @brief       read debug device os log data through dma
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dma_dump_debug_dev_os_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump_debug_os_log);
}

/**
 * @brief       read debug device log data through dma
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dma_dump_debug_dev_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump_debug_dev_log);
}

/**
 * @brief       read run event log data through dma
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dma_dump_run_event_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump_run_event_log);
}

/**
 * @brief       read sec log data through dma
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dma_dump_sec_log_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump_sec_log);
}

/**
 * @brief       read bbox data through dma
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dma_dump_bbox_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump);
}

/**
 * @brief       read bbox data through DMA using URD cmd protocol (David platform version)
 *              Milan counterpart bbox_dma_dump_bbox_data uses drvMemRead
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dump_bbox_data_read(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump_read);
}

/**
 * @brief       read bbox data through PCIe
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_pcie_dump_bbox_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_memory_pcie_dump);
}

/**
 * @brief       read bbox data through PCIe using URD cmd protocol (David platform version)
 *              Milan counterpart bbox_pcie_dump_bbox_data uses drvMemRead
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dump_bbox_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump_read_data);
}

/**
 * @brief       read chip full dfx through dma
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dma_dump_cdr_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump_cdr);
}

/**
 * @brief       read chip full dfx through DMA using URD cmd protocol (David platform version)
 *              Milan counterpart bbox_dma_dump_cdr_data uses drvMemRead
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dump_cdr_data_read(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump_read_cdr);
}

/**
 * @brief       read bbox data through pcie
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_pcie_dump_cdr_full_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_memory_pcie_dump_cdr);
}

/**
 * @brief       read chip full dfx through PCIe using URD cmd protocol (David platform version)
 *              Milan counterpart bbox_pcie_dump_cdr_full_data uses drvMemRead
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read. also size of buffer
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dump_cdr_full_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_memory_dump_cdr_read);
}

/**
 * @brief       read vmcore data through dma
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dma_dump_vmcore_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_pcie_vmcore_read);
}

/**
 * @brief       read vmcore data through DMA using URD cmd protocol (David platform version)
 *              Milan counterpart bbox_dma_dump_vmcore_data uses drvMemRead
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dma_dump_vmcore_data_read(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dma_dump_data(phy_id, offset, size, buf, bbox_drv_vmcore_read);
}

/**
 * @brief       read vmcore status and size
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_pcie_dump_vmcore_stat_read(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_pcie_vmcore_stat_read);
}

/**
 * @brief       read vmcore status and size through PCIe using URD cmd protocol (David platform version)
 *              Milan counterpart bbox_pcie_dump_vmcore_stat_read uses drvMemRead
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to read
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dump_vmcore_stat_read(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_pcie_dump_data(phy_id, offset, size, buf, bbox_drv_vmcore_stat_read);
}

/**
 * @brief       set specified data to specified device id from pcie
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to set. also size of buffer
 * @param [out] buffer:      buffer to store set data
 * @param [in]  write_func:    function pointer
 * @return      == -1 failed, == 0 success
 */
STATIC bbox_status bbox_pcie_set_data(u32 phy_id, u32 offset, u32 size, u8 *buffer,
                                      const bbox_drv_pcie_write write_func)
{
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(write_func, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(size == 0, return BBOX_FAILURE, "%u", size);

    u32 packet_len, i;
    u32 surplus = size % PCIE_BAR_DDR_MAXLEN;
    u32 packet_num = (surplus == 0) ? 0 : 1;
    packet_num += size / PCIE_BAR_DDR_MAXLEN;

    for (i = 0; i < packet_num; i++) {
        u32 packet_offset = i * PCIE_BAR_DDR_MAXLEN;
        if (i < (packet_num - 1)) {
            packet_len = PCIE_BAR_DDR_MAXLEN;
        } else {
            packet_len = ((surplus == 0) ? PCIE_BAR_DDR_MAXLEN : surplus);
        }
        drvError_t ret = write_func(phy_id, offset + packet_offset, &buffer[packet_offset], packet_len);
        if (ret != DRV_ERROR_NONE) {
            if (ret == DRV_ERROR_NOT_SUPPORT) {
                return ret;
            }
            BBOX_ERR("Get data failed with %d. offset[0x%x], size[0x%x]", (int)ret, offset, size);
            return BBOX_FAILURE;
        }
    }

    return BBOX_SUCCESS;
}

/**
 * @brief       write kdump flag for vmcore generation
 * @param [in]  phy_id:       device phy id
 * @param [in]  offset:      data offset
 * @param [in]  size:        size of data to write
 * @param [out] buf:         buffer to store write data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_pcie_set_kdump_flag(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_kdump_flag_set_data(phy_id, offset, size, buf, bbox_pcie_set_data);
}
