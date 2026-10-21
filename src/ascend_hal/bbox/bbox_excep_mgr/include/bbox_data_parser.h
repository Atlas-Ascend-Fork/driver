/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BBOX_DATA_PARSER_H
#define BBOX_DATA_PARSER_H

#include "bbox_int.h"

enum plain_text_table_type {
    PLAINTEXT_TABLE_NONE = 0,
    PLAINTEXT_TABLE_DRIVER,
    PLAINTEXT_TABLE_TS,
    PLAINTEXT_TABLE_LPM,
    PLAINTEXT_TABLE_DVPP,
    PLAINTEXT_TABLE_BIOS,
    PLAINTEXT_TABLE_TEEOS,
    PLAINTEXT_TABLE_LPFW,
    PLAINTEXT_TABLE_MICROWATT,
    PLAINTEXT_TABLE_NETWORK,
    PLAINTEXT_TABLE_LPM_START,
    PLAINTEXT_TABLE_TF,
    PLAINTEXT_TABLE_BIOS_SRAM,
    PLAINTEXT_TABLE_AP_EPRINT,
    PLAINTEXT_TABLE_TS_START,
    PLAINTEXT_TABLE_TS_LOG,
    PLAINTEXT_TABLE_RUN_DEVICE_OS_LOG,
    PLAINTEXT_TABLE_DEBUG_DEVICE_OS_LOG,
    PLAINTEXT_TABLE_DEBUG_DEVICE_FW_LOG,
    PLAINTEXT_TABLE_RUN_EVENT_LOG,
    PLAINTEXT_TABLE_SEC_DEVICE_OS_LOG,
    PLAINTEXT_TABLE_LPM_SRAM,
    PLAINTEXT_TABLE_LPM_PMU,
    PLAINTEXT_TABLE_LPM_LOG,
    PLAINTEXT_TABLE_LPFW_SRAM,
    PLAINTEXT_TABLE_LPFW_PMU,
    PLAINTEXT_TABLE_LPFW_LOG,
    PLAINTEXT_TABLE_MICROWATT_SRAM,
    PLAINTEXT_TABLE_MICROWATT_PMU,
    PLAINTEXT_TABLE_MICROWATT_LOG,
    PLAINTEXT_TABLE_IMU_BOOT_LOG,
    PLAINTEXT_TABLE_UEFI_BOOT_LOG,
    PLAINTEXT_TABLE_IMU_RUN_LOG,
    PLAINTEXT_TABLE_HSM,
    PLAINTEXT_TABLE_HSM_START,
    PLAINTEXT_TABLE_HSM_LOG,
    PLAINTEXT_TABLE_ISP,
    PLAINTEXT_TABLE_SIL,
    PLAINTEXT_TABLE_CPUCORE,
    PLAINTEXT_TABLE_DDR_SRAM,
    PLAINTEXT_TABLE_BBOX_KBOX,
    PLAINTEXT_TABLE_AOS_LINUX,
    PLAINTEXT_TABLE_AOS_CORE,
    PLAINTEXT_TABLE_DP,
    PLAINTEXT_TABLE_SD,
    PLAINTEXT_TABLE_HDR_BOOT_BIOS,
    PLAINTEXT_TABLE_HDR_BOOT_DDR,
    PLAINTEXT_TABLE_HDR_BOOT_TEE,
    PLAINTEXT_TABLE_HDR_BOOT_HSM,
    PLAINTEXT_TABLE_HDR_BOOT_ATF,
    PLAINTEXT_TABLE_HDR_BOOT_AREA,
    PLAINTEXT_TABLE_HDR_BOOT,
    PLAINTEXT_TABLE_HDR_RUN_OS,
    PLAINTEXT_TABLE_HDR_RUN_LPM,
    PLAINTEXT_TABLE_HDR_RUN_LPFW,
    PLAINTEXT_TABLE_HDR_RUN_MICROWATT,
    PLAINTEXT_TABLE_HDR_RUN_TEE,
    PLAINTEXT_TABLE_HDR_RUN_HSM,
    PLAINTEXT_TABLE_HDR_RUN_ATF,
    PLAINTEXT_TABLE_HDR_RUN_AREA,
    PLAINTEXT_TABLE_HDR_RUN,
    PLAINTEXT_TABLE_HDR_BOOT_INFO,
    PLAINTEXT_TABLE_HDR_RUN_INFO,
    PLAINTEXT_TABLE_HDR_LOG,
    PLAINTEXT_TABLE_HDR_STATUS_BLOCK_BOOT,
    PLAINTEXT_TABLE_HDR_STATUS_BLOCK_RUN,
    PLAINTEXT_TABLE_HDR_STATUS_BLOCK,
    PLAINTEXT_TABLE_HDR_STATUS_DATA,
    PLAINTEXT_TABLE_HDR_STATUS,
    PLAINTEXT_TABLE_HDR,
    PLAINTEXT_TABLE_HDR_REBOOT_INFO,
    PLAINTEXT_TABLE_CDR,
    PLAINTEXT_TABLE_CDR_FULL,
    PLAINTEXT_TABLE_CDR_MIN,
    PLAINTEXT_TABLE_CDR_SRAM_MIN,
    PLAINTEXT_TABLE_CDR_SRAM,
    PLAINTEXT_TABLE_CDR_CLINK,
    PLAINTEXT_TABLE_CDR_SRAM_LOOSE,
    PLAINTEXT_TABLE_CDR_LOOSE,
    PLAINTEXT_TABLE_REG_DUMP,
    PLAINTEXT_TABLE_HBM_SRAM,
    PLAINTEXT_TABLE_SRAM_SNAPSHOT,
    PLAINTEXT_TABLE_BIOS_HISS,
    PLAINTEXT_TABLE_HBOOT,
    PLAINTEXT_TABLE_VMCORE_STAT,
    PLAINTEXT_TABLE_VMCORE,
    PLAINTEXT_TABLE_UB,
    PLAINTEXT_TABLE_IMP_LOG,
    PLAINTEXT_TABLE_IMU_LOG,
    PLAINTEXT_TABLE_MAX,
};

enum data_parse_type {
    NORMAL_DATA_TYPE,
    STARTUP_DATA_TYPE,
    COMMON_LOG_TYPE
};

/**
 * @brief       Interface for dump DDR data into file-system.
 * @param [in]  buffer  The buffer of DDR data.
 * @param [in]  len     The length of the buffer.
 * @param [in]  log_path The path for DDRDUMP module dump data into(contain "ddrdump/").
 * @return      0 on success otherwise -1
 */
bbox_status bbox_ddr_dump(const buff *buffer, u32 len, const char *log_path);

/**
 * @brief       Interface for dump kernel log into file-system.
 * @param [in]  buffer  The buffer of kernel log data.
 * @param [in]  len     The length of the buffer.
 * @param [in]  logpath The path for DDRDUMP module dump data into.
 * @return      0 on success otherwise -1
 */
bbox_status bbox_klog_dump(const void *buffer, u32 len, const char *logpath);

/**
 * @brief       dump hdr data in given buffer to specified log path
 * @param [in]  buffer:     hdr data buffer
 * @param [in]  len:        buffer length
 * @param [in]  logpath:    path to write file
 */
bbox_status bbox_hdr_dump(const buff *buffer, u32 len, const char *log_path);

/**
 * @brief       dump slog data in given buffer to specified log path
 * @param [in]  type:       slog data type
 * @param [in]  buffer:     slog data buffer
 * @param [in]  len:        buffer length
 * @param [in]  log_path:    path to write file
 */
bbox_status bbox_slog_dump(enum plain_text_table_type type, const buff *buffer, u32 len, const char *log_path);

/**
 * @brief       dump cdr ddr data in given buffer to specified log path
 * @param [in]  buffer:     cdr data buffer
 * @param [in]  len:        buffer length
 * @param [in]  log_path:    path to write file
 */
bbox_status bbox_cdr_dump(const buff *buffer, u32 len, const char *log_path);

/**
 * @brief       dump cdr ddr data in given buffer to specified log path
 * @param [in]  type:       cdr data type
 * @param [in]  buffer:     cdr data buffer
 * @param [in]  len:        buffer length
 * @param [in]  log_path:    path to write file
 */
bbox_status bbox_cdr_full_dump(enum plain_text_table_type type, const buff *buffer, u32 len, const char *log_path);

/**
 * @brief       dump cdr sram data in given buffer to specified log path
 * @param [in]  type:       cdr data type
 * @param [in]  buffer:     cdr data buffer
 * @param [in]  len:        buffer length
 * @param [in]  log_path:    path to write file
 */
bbox_status bbox_cdr_min_dump(enum plain_text_table_type type, const buff *buffer, u32 len, const char *log_path);

/**
 * @brief           join DDR module data and register information together in one block data
 * @param [in]      current data block to join in
 * @param [in,out]  out     buffer contained the data to be joined
 * @param [in]      len     buffer length of the joined data(out).
 * @return          0 on success otherwise -1
 */
bbox_status bbox_ddr_dump_joint_dump(const buff *current, buff *out, u64 len);

/**
 * @brief       dump ddr data for different core id
 * @param [in]  log_path:  path for written log
 * @param [in]  coreid:   core id
 * @param [out] buffer:   ddr data
 * @param [in]  length:   buffer length
 * @return      0: success, other: failed
 */
s32 bbox_ddr_dump_get_module(const char *log_path, u8 coreid, const buff *buffer, u32 length);

/**
 * @brief       dump ddr data for different core id
 * @param [in]  coreid:    core id
 * @param [in]  type:      get parese type, COMMON_LOG_TYPE/NORMAL_DATA_TYPE/STARTUP_DATA_TYPE
 * @return      data type in plaintext_table_type_e
 */
enum plain_text_table_type bbox_plaintext_get_type(u8 coreid, enum data_parse_type type);

/**
 * @brief       parse given data and write data to file
 * @param [in]  logpath:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      0: success, other: failed
 */
bbox_status bbox_plaintext_data(const char *log_path, enum plain_text_table_type type, const void *buffer, u32 length);

/**
 * @brief       write data to file
 * @param [in]  log_path:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      BBOX_SUCCESS: success, BBOX_FAILURE: failed
 */
bbox_status bbox_savetext_data(const char *log_path, enum plain_text_table_type type, const void *buffer, u32 length);
/**
 * @brief       parse given data and write data to file under bbox dir
 * @param [in]  logpath:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      0: success, other: failed
 */
bbox_status bbox_bbox_plaintext_data(const char *log_path, enum plain_text_table_type type, const void *data,
                                     u32 length);

/**
 * @brief       write data to file under bbox dir
 * @param [in]  log_path:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      0: success, other: failed
 */
bbox_status bbox_bbox_savetext_data(const char *log_path, enum plain_text_table_type type, const void *data,
                                    u32 length);

/**
 * @brief       parse given data and write data to file under log dir
 * @param [in]  logpath:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      0: success, other: failed
 */
bbox_status bbox_log_plaintext_data(const char *log_path, enum plain_text_table_type type, const void *data,
                                    u32 length);

/**
 * @brief       parse given data and write data to file under mntn dir
 * @param [in]  logpath:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      0: success, other: failed
 */
bbox_status bbox_mntn_plaintext_data(const char *log_path, enum plain_text_table_type type, const void *data,
                                     u32 length);
bbox_status bbox_cdr_clink_dump(enum plain_text_table_type type, const buff *buffer, u32 len, const char *log_path);

#endif /* BBOX_DATA_PARSER_H */
