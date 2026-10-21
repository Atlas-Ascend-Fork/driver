/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "bbox_dump_process_david.h"
#include "bbox_dump_process.h"
#include "bbox_print.h"
#include "bbox_drv_adapter.h"
#include "bbox_system_api.h"
#include "bbox_ddr_int.h"
#include "bbox_rdr_data_parser.h"
#include "bbox_dump_interface.h"
#include "bbox_parse_interface.h"
#include "ascend_hal.h"
#include "dsmi_common_interface.h"

/**
 * @brief       get SRAM bin data. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     offset of BIOS SRAM data
 * @param [in]  size:       size of BIOS SRAM data
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */
STATIC bbox_status bbox_get_sram_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_dump_sram_data(phy_id, offset, size, buf);
}

/**
 * @brief       : check bios stage
 * @param [in]  : u32 phy_id           device phy id
 * @return      : 0 on success otherwise -1
 */
STATIC bbox_status bbox_check_bios_stage(u32 phy_id)
{
    u32 master_id = 0;
    drvError_t ret = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (ret != DRV_ERROR_NONE) {
        BBOX_INF("use device id as master id.");
        master_id = phy_id;
    }

    u32 key_point = 0;
    ret = bbox_get_sram_data(master_id, BBOX_BIOS_KEYPOINT_OFFSET, sizeof(u32), (u8 *)&key_point);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            BBOX_ERR("dump bootup bios stage failed with %d", (int)ret);
        }
        return BBOX_FAILURE;
    }
    BBOX_INF("bootup stage(%u)", key_point);
    if (key_point != BOOTUP_STAGE_BIOS_SUCC) {
        return BBOX_FAILURE;
    }

    return BBOX_SUCCESS;
}

STATIC bbox_status bbox_dump_bbox_ddr(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    bbox_status ret = bbox_dump_bbox_data_read(phy_id, BBOX_DDR_OFFSET, size, buf);
    if (ret != BBOX_SUCCESS) {
        return bbox_dump_bbox_data(phy_id, offset, size, buf);
    }
    return ret;
}
/**
 * @brief       get bbox static memory data. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     offset of bbox ddr data
 * @param [in]  size:       size of bbox ddr data & buffer
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */
STATIC bbox_status bbox_get_bbox_ddr(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    u32 master_id = 0;
    bbox_status ret;

    drvError_t err = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (err != DRV_ERROR_NONE) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "get master id failed(%d).", (int)err);
    }

    if (phy_id == master_id) {
        return bbox_dump_bbox_ddr(phy_id, offset, size, buf);
    }
#ifndef BBOX_ST_TEST
    ret = bbox_dump_bbox_ddr(master_id, offset, size, buf);
    if (ret != BBOX_SUCCESS) {
        BBOX_WAR("master device memory dump not complete.");
    }

    u8 *curr_buf = (u8 *)bbox_malloc(size);
    if (curr_buf == NULL) {
        BBOX_ERR("Malloc failed");
        return BBOX_FAILURE;
    }

    ret = bbox_dump_bbox_ddr(phy_id, offset, size, curr_buf);
    if (ret != BBOX_SUCCESS) {
        BBOX_INF("current device memory dump not complete.");
    }

    ret = bbox_ddr_dump_joint_dump(curr_buf, buf, size);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR("joint dump error.");
    }

    bbox_free(curr_buf);
#endif
    return ret;
}

/**
 * @brief       : check bbox event, if ddr has event, then need dump
 * @param [in]  : u32 phy_id           device phy id
 * @param [out] : u16 *event          event flag
 * @param [out] : char *tms           timestamp string
 * @param [in]  : s32 len             string length
 * @return      : 0 on success otherwise -1
 */
bbox_status bbox_check_dev_event(u32 phy_id, int logic_id, u16 *event, char *tms, s32 len)
{
    char dev_buf[DEV_BUF_MAX_LEN];

    BBOX_CHK_NULL_PTR(event, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(tms, return BBOX_FAILURE);

    u32 master_id = 0;
    drvError_t ret = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (ret != DRV_ERROR_NONE) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "[%s] get master id failed(%d).",
                      bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)), (int)ret);
    }

    u8 *buffer = (u8 *)bbox_malloc(DMA_MEMDUMP_MAXLEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, buffer == NULL, return BBOX_FAILURE, "malloc failed.");

    // read data
    bbox_status err = bbox_dump_bbox_ddr(master_id, BBOX_DDR_BASE_OFFSET, DMA_MEMDUMP_MAXLEN, buffer);
    if (err != BBOX_SUCCESS) {
        bbox_free(buffer);
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "read rdr data failed.");
    }

    const struct rdr_head *head = (const struct rdr_head *)buffer;
    if (bbox_ddr_dump_check(head) == false) {
        BBOX_ERR("[%s] magic(0x%x) and version(0x%x) are wrong.",
                 bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)), head->top_head.magic,
                 head->top_head.version);
        bbox_free(buffer);
        return BBOX_FAILURE;
    }

    *event = head->log_info.event_flag;
    (void)bbox_get_device_log_tms(&head->log_info, tms, len);

    bbox_free(buffer);
    return BBOX_SUCCESS;
}

STATIC bbox_status bbox_get_hdr_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    drvError_t ret;
    u32 magic = 0;

    ret = bbox_drv_read_data(phy_id, MEM_TYPE_BBOX_HDR, offset, (u8 *)&magic, sizeof(magic));
    if (ret != BBOX_SUCCESS) {
        if (ret == DRV_ERROR_NOT_SUPPORT) {
            return ret;
        }
        BBOX_ERR("bbox_drv_read_data Failed. (ret=%d)", (int)ret);
        return BBOX_FAILURE;
    }

    if (magic != BIOS_MAGIC) {
        BBOX_INF("magic has not been initialized. (magic=0x%x)", magic);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return bbox_drv_read_data(phy_id, MEM_TYPE_BBOX_HDR, offset, buf, size);
}

/**
 * @brief       get SRAM bin data before boot. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     offset of BIOS SRAM data
 * @param [in]  size:       size of BIOS SRAM data
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */
STATIC bbox_status bbox_get_hboot_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    u32 boot_status = DSMI_BOOT_STATUS_UNINIT;
    drvError_t ret;
    u32 magic = 0;

    drvError_t err = drvGetDeviceBootStatus((s32)phy_id, &boot_status);
    if (err != DRV_ERROR_NONE) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "get boot status failed with %d.", (int)err);
    }

    /* L2BUFF was used as the SRAM during the HBoot boot phase and is released after the boot is complete. */
    if ((boot_status == DSMI_BOOT_STATUS_OS) || (boot_status == DSMI_BOOT_STATUS_FINISH)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = bbox_drv_read_data(phy_id, MEM_TYPE_HBOOT_SRAM, offset, (u8 *)&magic, sizeof(magic));
    if (ret != BBOX_SUCCESS) {
        if (ret == DRV_ERROR_NOT_SUPPORT) {
            return ret;
        }
        BBOX_ERR("bbox_drv_read_data failed. (ret=%d)", (int)ret);
        return BBOX_FAILURE;
    }

    if (magic != BIOS_MAGIC) {
        BBOX_INF("magic has not been initialized. (magic=0x%x)", magic);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return bbox_drv_read_data(phy_id, MEM_TYPE_HBOOT_SRAM, offset, buf, size);
}

static drvError_t bbox_get_vmcore_stat_time_out(u32 phy_id, MEM_CTRL_TYPE mem_type, u32 offset, u8 *buf, u32 size,
                                                u32 timeout)
{
    drvError_t ret;
    unsigned int read_times = 0;

    if (buf == NULL) {
        BBOX_ERR("Value is NULL.");
        return DRV_ERROR_PARA_ERROR;
    }

    while ((*buf) != FLAG_ALL_DONE) {
        if (read_times >= timeout) {
            ret = bbox_drv_read_data(phy_id, mem_type, offset, buf, size);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
            return DRV_ERROR_NONE;
        }

        ret = bbox_drv_read_data(phy_id, mem_type, offset, buf, size);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }

        read_times++;
        (void)sleep(1);
    }
    return DRV_ERROR_NONE;
}

/**
 * @brief       get vmcore status. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     data offset
 * @param [in]  size:       size of vmcore status
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */
STATIC bbox_status bbox_get_vmcore_stat(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    u32 master_id = 0;
    drvError_t ret = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (ret != DRV_ERROR_NONE) {
        BBOX_INF("use device id as master id.");
        master_id = phy_id;
    }
    if (phy_id == master_id) {
        return bbox_get_vmcore_stat_time_out(phy_id, MEM_TYPE_VMCORE_STAT, offset, buf, size, KDUMP_TIMEOUT_TIMES);
    }
    return DRV_ERROR_NOT_SUPPORT;
}

/**
 * @brief       get vmcore data. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     data offset
 * @param [in]  size:       size of vmcore data partition
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */
STATIC bbox_status bbox_get_vmcore_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    u32 master_id = 0;
    drvError_t ret = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (ret != DRV_ERROR_NONE) {
        BBOX_INF("use device id as master id.");
        master_id = phy_id;
    }
    if (phy_id == master_id) {
        return bbox_dma_dump_vmcore_data_read(phy_id, offset, size, buf);
    }
    return DRV_ERROR_NOT_SUPPORT;
}

/**
 * @brief       : check bbox ddr, if ddr has exception, then need dump
 * @param [in]  : u32 phy_id           device phy id
 * @return      : 0 on success otherwise -1
 */
STATIC int bbox_check_bbox_ddr(u32 phy_id)
{
    u32 master_id = 0;
    u32 boot_status = DSMI_BOOT_STATUS_UNINIT;

    drvError_t err = drvGetDeviceBootStatus((s32)phy_id, &boot_status);
    if (err != DRV_ERROR_NONE) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "get boot status failed with %d.", (int)err);
    }

    if (boot_status != DSMI_BOOT_STATUS_FINISH) {
        return BBOX_FAILURE;
    }

    err = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (err != DRV_ERROR_NONE) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "get master id failed(%d).", (int)err);
    }

    u8 *buffer = (u8 *)bbox_malloc(DMA_MEMDUMP_MAXLEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, buffer == NULL, return BBOX_FAILURE, "Malloc failed.");

    // read data
    bbox_status ret = bbox_dump_bbox_ddr(master_id, BBOX_DDR_BASE_OFFSET, DMA_MEMDUMP_MAXLEN, buffer);
    if (ret != BBOX_SUCCESS) {
        BBOX_SAFE_FREE(buffer);
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "read rdr data failed.");
    }

    const struct rdr_head *head = (const struct rdr_head *)buffer;
    if (!bbox_ddr_dump_check(head)) {
        BBOX_ERR("magic(0x%x) and version(0x%x) are wrong.", head->top_head.magic, head->top_head.version);
        BBOX_SAFE_FREE(buffer);
        return BBOX_FAILURE;
    }

    if (head->log_info.log_num != 0) {
        BBOX_SAFE_FREE(buffer);
        return BBOX_SUCCESS;
    }

    BBOX_SAFE_FREE(buffer);
    return BBOX_FAILURE;
}

STATIC int bbox_get_cdr_data(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    int ret = bbox_dump_cdr_data_read(phy_id, BBOX_DDR_CHIP_DFX_BASE_OFFSET, size, buf);
    if (ret != BBOX_SUCCESS) {
        return bbox_dump_cdr_full_data(phy_id, offset, size, buf);
    }
    return ret;
}

/**
 * @brief       get debug device os log data. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     offset of log ddr data
 * @param [in]  size:       size of log ddr data
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */
STATIC bbox_status bbox_dma_get_debug_dev_os_log(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    u32 master_id = 0;
    drvError_t ret = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (ret != DRV_ERROR_NONE) {
        BBOX_INF("use device id as master id.");
        master_id = phy_id;
    }
    if (phy_id == master_id) {
        return bbox_drv_read_data(phy_id, MEM_TYPE_DEBUG_OS_LOG, offset, buf, size);
    }
    return DRV_ERROR_NOT_SUPPORT;
}

/**
 * @brief       get sec log data. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     offset of log ddr data
 * @param [in]  size:       size of log ddr data
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */

STATIC bbox_status bbox_dma_get_sec_log(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    u32 master_id = 0;
    drvError_t ret = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (ret != DRV_ERROR_NONE) {
        BBOX_INF("use device id as master id.");
        master_id = phy_id;
    }
    if (phy_id == master_id) {
        return bbox_drv_read_data(phy_id, MEM_TYPE_SEC_LOG, offset, buf, size);
    }
    return DRV_ERROR_NOT_SUPPORT;
}

/**
 * @brief       get run device os log data. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     offset of log ddr data
 * @param [in]  size:       size of log ddr data
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */
STATIC bbox_status bbox_dma_get_run_dev_os_log(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    u32 master_id = 0;
    drvError_t ret = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (ret != DRV_ERROR_NONE) {
        BBOX_INF("use device id as master id.");
        master_id = phy_id;
    }
    if (phy_id == master_id) {
        return bbox_drv_read_data(phy_id, MEM_TYPE_RUN_OS_LOG, offset, buf, size);
    }
    return DRV_ERROR_NOT_SUPPORT;
}

/**
 * @brief       get run event log data. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     offset of log ddr data
 * @param [in]  size:       size of log ddr data
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */
STATIC bbox_status bbox_dma_get_run_event_log(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    u32 master_id = 0;
    drvError_t ret = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (ret != DRV_ERROR_NONE) {
        BBOX_INF("use device id as master id.");
        master_id = phy_id;
    }
    if (phy_id == master_id) {
        return bbox_drv_read_data(phy_id, MEM_TYPE_RUN_EVENT_LOG, offset, buf, size);
    }
    return DRV_ERROR_NOT_SUPPORT;
}

/**
 * @brief       get debug device fw log data. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     offset of log ddr data
 * @param [in]  size:       size of log ddr data
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */
STATIC bbox_status bbox_dma_get_debug_dev_fw_log(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_DEBUG_DEV_LOG, offset, buf, size);
}

/**
 * @brief       get kernel log data. arguments was checked by caller
 * @param [in]  phy_id:      device phy id
 * @param [in]  offset:     offset of kernel log ddr data
 * @param [in]  size:       size of kernel log ddr data
 * @param [out] buf:        buffer to store data
 * @return      0 on success, otherwise -1
 */
STATIC bbox_status bbox_get_kernel_log(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    u32 master_id = 0;
    drvError_t ret;

    ret = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (ret != DRV_ERROR_NONE) {
        BBOX_INF("Use device id as master id.");
        master_id = phy_id;
    }

    return bbox_dump_klog_data(master_id, offset, size, buf);
}

STATIC bbox_status bbox_get_ts_log(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_TS_LOG, offset, buf, size);
}

STATIC bbox_status bbox_get_imp_log(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_IMP_LOG, offset, buf, size);
}

STATIC bbox_status bbox_get_imu_log(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_IMU_LOG, offset, buf, size);
}

STATIC bbox_status bbox_get_hsm_log(u32 phy_id, u32 offset, u32 size, u8 *buf)
{
    return bbox_drv_read_data(phy_id, MEM_TYPE_HSM_LOG, offset, buf, size);
}

static dump_data_config_st g_boot_failed_config[] = {
    {"hboot", BBOX_SRAM_HBOOT_OFFSET, BBOX_SRAM_HBOOT_LEN, BBOX_DUMP_FILE_HBOOT, PLAINTEXT_TABLE_HBOOT, NULL,
     bbox_get_hboot_data, bbox_parse_sram_mntn_data},
    {"hbm_sram", BBOX_SRAM_HBM_DFX_OFFSET, BBOX_SRAM_HBM_DFX_LEN, BBOX_DUMP_FILE_HBM_SRAM, PLAINTEXT_TABLE_HBM_SRAM,
     NULL, bbox_get_sram_data, bbox_parse_sram_mntn_data},
    {"sram_snapshot", BBOX_SRAM_SNAPSHOT_OFFSET, BBOX_SRAM_SNAPSHOT_LEN, BBOX_DUMP_FILE_SRAM_SNAPSHOT,
     PLAINTEXT_TABLE_SRAM_SNAPSHOT, NULL, bbox_get_sram_data, bbox_parse_sram_mntn_data},
    {"chip_dfx_clink", BBOX_SRAM_CHIP_CLINK_OFFSET, BBOX_SRAM_CHIP_CLINK_LEN, BBOX_DUMP_FILE_CDR_CLINK,
     PLAINTEXT_TABLE_CDR_CLINK, NULL, bbox_get_sram_data, bbox_parse_cdr_clink_data},
    {"hdr_snapshot", BBOX_DDR_HDR_OFFSET, BBOX_DDR_HDR_LEN, BBOX_DUMP_FILE_HDR, PLAINTEXT_TABLE_MAX,
     bbox_check_bios_stage, bbox_get_hdr_data, bbox_parse_hdr_data},
    {"kernel_log", BBOX_DDR_KLOG_OFFSET, BBOX_DDR_KLOG_LEN, BBOX_DUMP_FILE_KLOG, PLAINTEXT_TABLE_MAX, NULL,
     bbox_get_kernel_log, bbox_parse_klog_data},
};

static dump_data_config_st g_heatbeat_lost_config[] = {
    {"hboot", BBOX_SRAM_HBOOT_OFFSET, BBOX_SRAM_HBOOT_LEN, BBOX_DUMP_FILE_HBOOT, PLAINTEXT_TABLE_HBOOT, NULL,
     bbox_get_hboot_data, bbox_parse_sram_mntn_data},
    {"hbm_sram", BBOX_SRAM_HBM_DFX_OFFSET, BBOX_SRAM_HBM_DFX_LEN, BBOX_DUMP_FILE_HBM_SRAM, PLAINTEXT_TABLE_HBM_SRAM,
     NULL, bbox_get_sram_data, bbox_parse_sram_mntn_data},
    {"sram_snapshot", BBOX_SRAM_SNAPSHOT_OFFSET, BBOX_SRAM_SNAPSHOT_LEN, BBOX_DUMP_FILE_SRAM_SNAPSHOT,
     PLAINTEXT_TABLE_SRAM_SNAPSHOT, NULL, bbox_get_sram_data, bbox_parse_sram_mntn_data},
    {"chip_dfx_min", BBOX_SRAM_CHIP_DFX_OFFSET, BBOX_SRAM_CHIP_DFX_LEN, BBOX_DUMP_FILE_CDR_SRAM,
     PLAINTEXT_TABLE_CDR_SRAM, NULL, bbox_get_sram_data, bbox_parse_cdr_min_data},
    {"chip_dfx_clink", BBOX_SRAM_CHIP_CLINK_OFFSET, BBOX_SRAM_CHIP_CLINK_LEN, BBOX_DUMP_FILE_CDR_CLINK,
     PLAINTEXT_TABLE_CDR_CLINK, NULL, bbox_get_sram_data, bbox_parse_cdr_clink_data},
    {"kernel_log", BBOX_DDR_KLOG_OFFSET, BBOX_DDR_KLOG_LEN, BBOX_DUMP_FILE_KLOG, PLAINTEXT_TABLE_MAX, NULL,
     bbox_get_kernel_log, bbox_parse_klog_data},
    {"chip_dfx_full", BBOX_DDR_CHIP_DFX_OFFSET, BBOX_DDR_CHIP_DFX_LEN, BBOX_DUMP_FILE_CDR_DDR, PLAINTEXT_TABLE_CDR,
     NULL, bbox_get_cdr_data, bbox_parse_cdr_full_data},
    {"ts_log", BBOX_DUMP_TS_LOG_OFFSET, BBOX_DUMP_TS_LOG_LEN, BBOX_DUMP_FILE_TS_LOG, PLAINTEXT_TABLE_TS_LOG, NULL,
     bbox_get_ts_log, bbox_parse_log_data},
    {"imp_log", BBOX_DUMP_IMP_LOG_OFFSET, BBOX_DUMP_IMP_LOG_LEN, BBOX_DUMP_FILE_IMP_LOG, PLAINTEXT_TABLE_IMP_LOG, NULL,
     bbox_get_imp_log, bbox_parse_log_data},
    {"imu_log", BBOX_DUMP_IMU_LOG_OFFSET, BBOX_DUMP_IMU_LOG_LEN, BBOX_DUMP_FILE_IMU_LOG, PLAINTEXT_TABLE_IMU_LOG, NULL,
     bbox_get_imu_log, bbox_parse_log_data},
    {"hsm_log", BBOX_DUMP_HSM_LOG_OFFSET, BBOX_DUMP_HSM_LOG_LEN, BBOX_DUMP_FILE_HSM_LOG, PLAINTEXT_TABLE_HSM_LOG, NULL,
     bbox_get_hsm_log, bbox_parse_log_data},
    {"debug_device_os_log", BBOX_DDR_DEBUG_DEVICE_OS_LOG_OFFSET, BBOX_DDR_DEBUG_DEVICE_OS_LOG_LEN,
     BBOX_DUMP_FILE_DEBUG_DEVICE_OS_LOG, PLAINTEXT_TABLE_DEBUG_DEVICE_OS_LOG, NULL, bbox_dma_get_debug_dev_os_log,
     bbox_parse_slog_data},
    {"sec_device_os_log", BBOX_DDR_SEC_DEVICE_OS_LOG_OFFSET, BBOX_DDR_SEC_DEVICE_OS_LOG_LEN,
     BBOX_DUMP_FILE_SEC_DEVICE_OS_LOG, PLAINTEXT_TABLE_SEC_DEVICE_OS_LOG, NULL, bbox_dma_get_sec_log,
     bbox_parse_slog_data},
    {"run_device_os_log", BBOX_DDR_RUN_DEVICE_OS_LOG_OFFSET, BBOX_DDR_RUN_DEVICE_OS_LOG_LEN,
     BBOX_DUMP_FILE_RUN_DEVICE_OS_LOG, PLAINTEXT_TABLE_RUN_DEVICE_OS_LOG, NULL, bbox_dma_get_run_dev_os_log,
     bbox_parse_slog_data},
    {"run_event_log", BBOX_DDR_RUN_EVENT_LOG_OFFSET, BBOX_DDR_RUN_EVENT_LOG_LEN, BBOX_DUMP_FILE_RUN_EVENT_LOG,
     PLAINTEXT_TABLE_RUN_EVENT_LOG, NULL, bbox_dma_get_run_event_log, bbox_parse_slog_data},
    {"debug_device_fw_log", BBOX_DDR_DEBUG_DEVICE_FW_LOG_OFFSET, BBOX_DDR_DEBUG_DEVICE_FW_LOG_LEN,
     BBOX_DUMP_FILE_DEBUG_DEVICE_FW_LOG, PLAINTEXT_TABLE_DEBUG_DEVICE_FW_LOG, bbox_check_bbox_ddr,
     bbox_dma_get_debug_dev_fw_log, bbox_parse_slog_data},
    {"bbox_ddr_dump", BBOX_DDR_BASE_OFFSET, BBOX_DDR_BASE_LEN, BBOX_DUMP_FILE_DDR_DMA, PLAINTEXT_TABLE_MAX, NULL,
     bbox_get_bbox_ddr, bbox_parse_bbox_ddr_data},
};

static dump_data_config_st g_hdc_exception_config[] = {
    {"hboot", BBOX_SRAM_HBOOT_OFFSET, BBOX_SRAM_HBOOT_LEN, BBOX_DUMP_FILE_HBOOT, PLAINTEXT_TABLE_HBOOT, NULL,
     bbox_get_hboot_data, bbox_parse_sram_mntn_data},
    {"hbm_sram", BBOX_SRAM_HBM_DFX_OFFSET, BBOX_SRAM_HBM_DFX_LEN, BBOX_DUMP_FILE_HBM_SRAM, PLAINTEXT_TABLE_HBM_SRAM,
     NULL, bbox_get_sram_data, bbox_parse_sram_mntn_data},
    {"sram_snapshot", BBOX_SRAM_SNAPSHOT_OFFSET, BBOX_SRAM_SNAPSHOT_LEN, BBOX_DUMP_FILE_SRAM_SNAPSHOT,
     PLAINTEXT_TABLE_SRAM_SNAPSHOT, NULL, bbox_get_sram_data, bbox_parse_sram_mntn_data},
    {"kernel_log", BBOX_DDR_KLOG_OFFSET, BBOX_DDR_KLOG_LEN, BBOX_DUMP_FILE_KLOG, PLAINTEXT_TABLE_MAX, NULL,
     bbox_get_kernel_log, bbox_parse_klog_data},
    {"bbox_ddr_dump", BBOX_DDR_BASE_OFFSET, BBOX_DDR_BASE_LEN, BBOX_DUMP_FILE_DDR_DMA, PLAINTEXT_TABLE_MAX, NULL,
     bbox_get_bbox_ddr, bbox_parse_bbox_ddr_data},
};

static dump_data_config_st g_oom_config[] = {
    {"kernel_log", BBOX_DDR_KLOG_OFFSET, BBOX_DDR_KLOG_LEN, BBOX_DUMP_FILE_KLOG, PLAINTEXT_TABLE_MAX, NULL,
     bbox_get_kernel_log, bbox_parse_klog_data},
    {"bbox_ddr_dump", BBOX_DDR_BASE_OFFSET, BBOX_DDR_BASE_LEN, BBOX_DUMP_FILE_DDR_DMA, PLAINTEXT_TABLE_MAX, NULL,
     bbox_get_bbox_ddr, bbox_parse_bbox_ddr_data},
};

static dump_data_config_st g_force_config[] = {
    {"hboot", BBOX_SRAM_HBOOT_OFFSET, BBOX_SRAM_HBOOT_LEN, BBOX_DUMP_FILE_HBOOT, PLAINTEXT_TABLE_HBOOT, NULL,
     bbox_get_hboot_data, bbox_parse_sram_mntn_data},
    {"hbm_sram", BBOX_SRAM_HBM_DFX_OFFSET, BBOX_SRAM_HBM_DFX_LEN, BBOX_DUMP_FILE_HBM_SRAM, PLAINTEXT_TABLE_HBM_SRAM,
     NULL, bbox_get_sram_data, bbox_parse_sram_mntn_data},
    {"sram_snapshot", BBOX_SRAM_SNAPSHOT_OFFSET, BBOX_SRAM_SNAPSHOT_LEN, BBOX_DUMP_FILE_SRAM_SNAPSHOT,
     PLAINTEXT_TABLE_SRAM_SNAPSHOT, NULL, bbox_get_sram_data, bbox_parse_sram_mntn_data},
    {"chip_dfx_min", BBOX_SRAM_CHIP_DFX_OFFSET, BBOX_SRAM_CHIP_DFX_LEN, BBOX_DUMP_FILE_CDR_SRAM,
     PLAINTEXT_TABLE_CDR_SRAM_LOOSE, NULL, bbox_get_sram_data, bbox_parse_cdr_min_data},
    {"chip_dfx_clink", BBOX_SRAM_CHIP_CLINK_OFFSET, BBOX_SRAM_CHIP_CLINK_LEN, BBOX_DUMP_FILE_CDR_CLINK,
     PLAINTEXT_TABLE_CDR_CLINK, NULL, bbox_get_sram_data, bbox_parse_cdr_clink_data},
    {"hdr_snapshot", BBOX_DDR_HDR_OFFSET, BBOX_DDR_HDR_LEN, BBOX_DUMP_FILE_HDR, PLAINTEXT_TABLE_MAX,
     bbox_check_bios_stage, bbox_get_hdr_data, bbox_parse_hdr_data},
    {"kernel_log", BBOX_DDR_KLOG_OFFSET, BBOX_DDR_KLOG_LEN, BBOX_DUMP_FILE_KLOG, PLAINTEXT_TABLE_MAX, NULL,
     bbox_get_kernel_log, bbox_parse_klog_data},
    {"chip_dfx_full", BBOX_DDR_CHIP_DFX_OFFSET, BBOX_DDR_CHIP_DFX_LEN, BBOX_DUMP_FILE_CDR_DDR, PLAINTEXT_TABLE_CDR,
     NULL, bbox_get_cdr_data, bbox_parse_cdr_full_data},
    {"ts_log", BBOX_DUMP_TS_LOG_OFFSET, BBOX_DUMP_TS_LOG_LEN, BBOX_DUMP_FILE_TS_LOG, PLAINTEXT_TABLE_TS_LOG, NULL,
     bbox_get_ts_log, bbox_parse_log_data},
    {"imp_log", BBOX_DUMP_IMP_LOG_OFFSET, BBOX_DUMP_IMP_LOG_LEN, BBOX_DUMP_FILE_IMP_LOG, PLAINTEXT_TABLE_IMP_LOG, NULL,
     bbox_get_imp_log, bbox_parse_log_data},
    {"imu_log", BBOX_DUMP_IMU_LOG_OFFSET, BBOX_DUMP_IMU_LOG_LEN, BBOX_DUMP_FILE_IMU_LOG, PLAINTEXT_TABLE_IMU_LOG, NULL,
     bbox_get_imu_log, bbox_parse_log_data},
    {"hsm_log", BBOX_DUMP_HSM_LOG_OFFSET, BBOX_DUMP_HSM_LOG_LEN, BBOX_DUMP_FILE_HSM_LOG, PLAINTEXT_TABLE_HSM_LOG, NULL,
     bbox_get_hsm_log, bbox_parse_log_data},
    {"debug_device_os_log", BBOX_DDR_DEBUG_DEVICE_OS_LOG_OFFSET, BBOX_DDR_DEBUG_DEVICE_OS_LOG_LEN,
     BBOX_DUMP_FILE_DEBUG_DEVICE_OS_LOG, PLAINTEXT_TABLE_DEBUG_DEVICE_OS_LOG, NULL, bbox_dma_get_debug_dev_os_log,
     bbox_parse_slog_data},
    {"sec_device_os_log", BBOX_DDR_SEC_DEVICE_OS_LOG_OFFSET, BBOX_DDR_SEC_DEVICE_OS_LOG_LEN,
     BBOX_DUMP_FILE_SEC_DEVICE_OS_LOG, PLAINTEXT_TABLE_SEC_DEVICE_OS_LOG, NULL, bbox_dma_get_sec_log,
     bbox_parse_slog_data},
    {"run_device_os_log", BBOX_DDR_RUN_DEVICE_OS_LOG_OFFSET, BBOX_DDR_RUN_DEVICE_OS_LOG_LEN,
     BBOX_DUMP_FILE_RUN_DEVICE_OS_LOG, PLAINTEXT_TABLE_RUN_DEVICE_OS_LOG, NULL, bbox_dma_get_run_dev_os_log,
     bbox_parse_slog_data},
    {"run_event_log", BBOX_DDR_RUN_EVENT_LOG_OFFSET, BBOX_DDR_RUN_EVENT_LOG_LEN, BBOX_DUMP_FILE_RUN_EVENT_LOG,
     PLAINTEXT_TABLE_RUN_EVENT_LOG, NULL, bbox_dma_get_run_event_log, bbox_parse_slog_data},
    {"debug_device_fw_log", BBOX_DDR_DEBUG_DEVICE_FW_LOG_OFFSET, BBOX_DDR_DEBUG_DEVICE_FW_LOG_LEN,
     BBOX_DUMP_FILE_DEBUG_DEVICE_FW_LOG, PLAINTEXT_TABLE_DEBUG_DEVICE_FW_LOG, bbox_check_bbox_ddr,
     bbox_dma_get_debug_dev_fw_log, bbox_parse_slog_data},
    {"bbox_ddr_dump", BBOX_DDR_BASE_OFFSET, BBOX_DDR_BASE_LEN, BBOX_DUMP_FILE_DDR_DMA, PLAINTEXT_TABLE_MAX, NULL,
     bbox_get_bbox_ddr, bbox_parse_bbox_ddr_data},
};

static dump_data_config_st g_vmcore_config[] = {
    {"vmcore_stat", BBOX_HBM_VMCORE_STAT_OFFSET, BBOX_HBM_VMCORE_STAT_LEN, BBOX_DUMP_VMCORE_STAT,
     PLAINTEXT_TABLE_VMCORE_STAT, NULL, bbox_get_vmcore_stat, NULL},
    {"vmcore", BBOX_HBM_VMCORE_OFFSET, BBOX_HBM_VMCORE_LEN, BBOX_DUMP_FILE_VMCORE, PLAINTEXT_TABLE_VMCORE, NULL,
     bbox_get_vmcore_data, NULL},
};

const dump_data_config_st *bbox_get_data_config(enum EXCEPTION_EVENT_TYPE event)
{
    switch (event) {
        case EVENT_LOAD_TIMEOUT:
            return g_boot_failed_config;
        case EVENT_HEARTBEAT_LOST:
            return g_heatbeat_lost_config;
        case EVENT_HDC_EXCEPTION:
            return g_hdc_exception_config;
        case EVENT_OOM:
            return g_oom_config;
        case EVENT_DUMP_FORCE:
            return g_force_config;
        case EVENT_DUMP_VMCORE:
            return g_vmcore_config;
        default:
            return NULL;
    }
}

u32 bbox_get_data_config_size(enum EXCEPTION_EVENT_TYPE event)
{
    switch (event) {
        case EVENT_LOAD_TIMEOUT:
            return sizeof(g_boot_failed_config) / sizeof(dump_data_config_st);
        case EVENT_HEARTBEAT_LOST:
            return sizeof(g_heatbeat_lost_config) / sizeof(dump_data_config_st);
        case EVENT_HDC_EXCEPTION:
            return sizeof(g_hdc_exception_config) / sizeof(dump_data_config_st);
        case EVENT_OOM:
            return sizeof(g_oom_config) / sizeof(dump_data_config_st);
        case EVENT_DUMP_FORCE:
            return sizeof(g_force_config) / sizeof(dump_data_config_st);
        case EVENT_DUMP_VMCORE:
            return sizeof(g_vmcore_config) / sizeof(dump_data_config_st);
        default:
            return 0;
    }
}