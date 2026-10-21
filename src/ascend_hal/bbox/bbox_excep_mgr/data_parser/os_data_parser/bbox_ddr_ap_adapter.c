/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bbox_ddr_ap_adapter.h"
#include <stddef.h>
#include "securec.h"
#include "bbox_print.h"
#include "bbox_fs_api.h"
#include "bbox_system_api.h"
#include "bbox_log_common.h"
#include "bbox_file_list.h"
#include "bbox_common.h"

/*
 * @brief       : save ap base info to filesystem
 * @param [in]  : const struct ap_top_head *top_head   ap data
 * @param [in]  : const struct ap_current_info *current_info   ap data
 * @param [in]  : const char *log_path           save to dir
 * @param [in]  : struct bbox_data_info *text     data cache to write
 * @param [in]  : bool bcore     core or linux
 * @return      : NA
 */
STATIC void bbox_ap_base_info(const struct ap_top_head *top_head, const struct ap_current_info *current_info,
                              const char *log_path, struct bbox_data_info *text, bool bcore)
{
    bbox_status ret;
    (void)bbox_data_reinit(text);
    if (bcore == true) {
        ret = bbox_data_print(text,
                              "\ncore basic info:\n"
                              "==================================================\n"
                              "version\t\t0x%lx\n"
                              "device num\t0x%x\n"
                              "cpu num\t\t0x%x\n"
                              "\ncore current info:\n"
                              "==================================================\n"
                              "device id\t0x%x\n"
                              "exception id\t0x%x\n"
                              "exception type\t0x%x\n"
                              "core id\t\t0x%x\n",
                              top_head->version, top_head->device_num, top_head->cpu_num, current_info->devid,
                              current_info->excepid, current_info->e_type, current_info->coreid);
    } else {
        ret = bbox_data_print(text,
                              "basic info:\n"
                              "==================================================\n"
                              "version\t\t0x%lx\n"
                              "device num\t0x%x\n"
                              "cpu num\t\t0x%x\n"
                              "\ncurrent info:\n"
                              "==================================================\n"
                              "device id\t0x%x\n"
                              "exception id\t0x%x\n"
                              "exception type\t0x%x\n"
                              "core id\t\t0x%x\n",
                              top_head->version, top_head->device_num, top_head->cpu_num, current_info->devid,
                              current_info->excepid, current_info->e_type, current_info->coreid);
    }
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return, "print ap base info failed");
    ret = bbox_data_save_to_fs(text, log_path, FILE_AP_EH_ROOT);
    BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "save ap base info failed");
}

/*
 * @brief       : save ap base info to filesystem
 * @param [in]  : const struct ap_root_info *ap   ap data
 * @param [in]  : const char *log_path           save to dir
 * @param [in]  : struct bbox_data_info *text     data cache to write
 * @return      : NA
 */
STATIC void bbox_ap_base_info_dump(const struct ap_root_info *ap, const char *log_path, struct bbox_data_info *text)
{
    bbox_ap_base_info(&ap->top_head, &ap->current_info, log_path, text, false);
}

/*
 * @brief       : save ap log info to filesystem
 * @param [in]  : const struct ap_root_info *ap   ap data
 * @param [in]  : const char *log_path           save to dir
 * @param [in]  : struct bbox_data_info *text     data cache to write
 * @return      : NA
 */
STATIC void bbox_ap_log_info(const struct ap_log_info *log_info, const char *log_path, struct bbox_data_info *text,
                             bool bcore)
{
    bbox_status ret;

    (void)bbox_data_reinit(text);
    if (bcore == true) {
        ret = bbox_data_print(text,
                              "\ncore log info (UTC time):\n"
                              "==================================================\n"
                              "event flag\t%u\n"
                              "log num\t\t%u\n",
                              log_info->event_flag, log_info->log_num);
    } else {
        ret = bbox_data_print(text,
                              "\nlog info (UTC time):\n"
                              "==================================================\n"
                              "event flag\t%u\n"
                              "log num\t\t%u\n",
                              log_info->event_flag, log_info->log_num);
    }
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return, "print ap log info failed");
    ret = bbox_data_save_to_fs(text, log_path, FILE_AP_EH_ROOT);
    BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "save ap log info failed");

    u32 i;
    u32 log_index = (log_info->log_num < AP_LOG_BUFFER_NUM) ? 0U : log_info->next;
    for (i = 0; i < RDR_MIN(log_info->log_num, AP_LOG_BUFFER_NUM); i++) {
        log_index %= AP_LOG_BUFFER_NUM;
        const struct ap_log_record *log_buffer = &log_info->log_buffer[log_index];
        const char *module = bbox_get_core_name(log_buffer->coreid);
        const char *reason = bbox_get_reboot_reason(log_buffer->e_type);
        ret = bbox_data_print(
            text, "system exception code[0x%08X]: Device[%d], ModuleName[%s], ExceptionReason[%s], TimeStamp [%s].\n",
            log_buffer->excepid, log_buffer->devid, PRINT_VALID_STR(module), PRINT_VALID_STR(reason), log_buffer->date);
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return, "print ap log item failed");
        ret = bbox_data_save_to_fs(text, log_path, FILE_AP_EH_ROOT);
        BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "save info in file failed");
        log_index++;
    }

    return;
}

/*
 * @brief       : save ap log info to filesystem
 * @param [in]  : const struct ap_root_info *ap   ap data
 * @param [in]  : const char *log_path           save to dir
 * @param [in]  : struct bbox_data_info *text     data cache to write
 * @return      : NA
 */
STATIC void bbox_ap_log_info_dump(const struct ap_root_info *ap, const char *log_path, struct bbox_data_info *text)
{
    bbox_ap_log_info(&ap->log_info, log_path, text, false);
}

/*
 * @brief       : save ap reg info to filesystem
 * @param [in]  : const struct ap_regs_info *dump_regs_info       register data
 * @param [in]  : u32 reg_regions_num                           register number
 * @param [in]  : const char *logpath                         save to dir
 * @param [in]  : struct bbox_data_info *text                   data cache to write
 * @param [in]  : bool bcore                                  core or linux
 * @return      : NA
 */
STATIC void bbox_ap_regs_info(const struct ap_regs_info *dump_regs_info, u32 reg_regions_num, const char *log_path,
                              struct bbox_data_info *text, bool bcore)
{
    bbox_status ret;
    u32 i;
    (void)bbox_data_reinit(text);
    if (bcore == true) {
        ret = bbox_data_print(text,
                              "\ncore register: \t%u\n"
                              "==================================================\n",
                              reg_regions_num);
    } else {
        ret = bbox_data_print(text,
                              "\nregister: \t%u\n"
                              "==================================================\n",
                              reg_regions_num);
    }
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return, "print regs info failed");
    ret = bbox_data_save_to_fs(text, log_path, FILE_AP_EH_ROOT);
    BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "save regs info failed");

    for (i = 0; (i < reg_regions_num) && (i < REGS_DUMP_MAX_NUM); i++) {
        ret = bbox_data_print(text,
                              "reg_name\t%s\n"
                              "reg_size\t0x%x\n"
                              "reg_base\t0x%llx\n",
                              dump_regs_info[i].reg_name, dump_regs_info[i].reg_size,
                              bbox_address_mask(dump_regs_info[i].reg_base));
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return, "print regs item failed");
        ret = bbox_data_save_to_fs(text, log_path, FILE_AP_EH_ROOT);
        BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "save regs item failed");
    }
}

/*
 * @brief       : save ap reg info to filesystem
 * @param [in]  : const struct ap_root_info *ap       ap data
 * @param [in]  : const char *logpath               save to dir
 * @param [in]  : struct bbox_data_info *text         data cache to write
 * @return      : NA
 */
STATIC void bbox_ap_regs_info_dump(const struct ap_root_info *ap, const char *log_path, struct bbox_data_info *text)
{
    bbox_ap_regs_info(ap->area_info.dump_regs_info[0], ap->area_info.reg_regions_num, log_path, text, false);
}

/*
 * @brief       : save ap info to filesystem
 * @param [in]  : const struct ap_root_info *ap       ap data
 * @param [in]  : const char *log_path               save to dir
 * @param [in]  : struct bbox_data_info *buf          data cache to writes
 * @return      : NA
 */
STATIC void bbox_ap_info_dump(const struct ap_root_info *ap, const char *log_path)
{
    struct bbox_data_info text;
    bbox_status ret = bbox_data_init(&text, (size_t)TMP_BUFF_S_LEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return, "bbox_data_init ap info buf failed.");
    bbox_ap_base_info_dump(ap, log_path, &text);
    bbox_ap_log_info_dump(ap, log_path, &text);
    bbox_ap_regs_info_dump(ap, log_path, &text);
    (void)bbox_data_clear(&text);
}

/*
 * @brief       : save ap pmu regs data to filesystem
 * @param [in]  : const void *addr              data
 * @param [in]  : u32 len                       data len
 * @param [in]  : const char *log_path           save to dir
 * @param [in]  : u32 devid                     device id
 * @return      : NA
 */
STATIC void bbox_ap_pmu_regs_dump(const void *addr, u32 len, const char *log_path, u32 devid)
{
    BBOX_CHK_NULL_PTR(addr, return);
    BBOX_CHK_NULL_PTR(log_path, return);

    struct bbox_data_info text;
    bbox_status ret = bbox_data_init(&text, (size_t)TMP_BUFF_S_LEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return, "bbox_data_init pmu regs buf failed.");

    ret = bbox_data_print(&text, "pmu-%u[%u]: ", devid, len);
    if (ret != BBOX_SUCCESS) {
        (void)bbox_data_clear(&text);
        BBOX_ERR_CTRL(BBOX_ERR, return, "bbox_data_print pmu regs key failed");
    }

    u32 i;
    for (i = 0; i < len; i++) {
        if ((i + 1U) == len) {
            ret = bbox_data_print(&text, "%02x\n", *(const u8 *)((const u8 *)addr + i));
        } else {
            ret = bbox_data_print(&text, "%02x", *(const u8 *)((const u8 *)addr + i));
        }
        if (ret != BBOX_SUCCESS) {
            (void)bbox_data_clear(&text);
            BBOX_ERR_CTRL(BBOX_ERR, return, "bbox_data_print pmu regs value failed");
        }
    }
    ret = bbox_data_save_to_fs(&text, log_path, BBOX_FILE_NAME_RESET_REG);
    BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "failed to save pmu regs info in file");
    (void)bbox_data_clear(&text);
}

/*
 * @brief       : save ap reg data to filesystem
 * @param [in]  : const struct ap_root_info *aproot      ap data
 * @param [in]  : u32 len                       data len
 * @param [in]  : const char *log_path           save to dir
 * @param [in]  : u32 devid                     device id
 * @param [in]  : u32 regid                     reg id
 * @return      : NA
 */
static void bbox_ap_reg_dump(const struct ap_root_info *aproot, u32 len, const char *log_path, u32 devid, u32 regid)
{
    u32 size = 0;
    const u8 *addr = (u8 *)bbox_rdr_ap_vaddr_convert(
        aproot, aproot->area_info.dump_regs_info[devid][regid].reg_dump_addr, &size);
    if (((uintptr_t)addr > (uintptr_t)aproot) &&
        (uintptr_t)addr + aproot->area_info.dump_regs_info[devid][regid].reg_size < (uintptr_t)aproot + len) {
        if (strncmp(REG_NAME_PMU, aproot->area_info.dump_regs_info[devid][regid].reg_name, strlen(REG_NAME_PMU)) == 0) {
            bbox_ap_pmu_regs_dump(addr, aproot->area_info.dump_regs_info[devid][regid].reg_size, log_path, devid);
        }
    }
}

/*
 * @brief       : save ap regs data to filesystem
 * @param [in]  : const struct ap_root_info *aproot      ap data
 * @param [in]  : u32 len                       data len
 * @param [in]  : const char *log_path           save to dir
 * @return      : NA
 */
STATIC void bbox_ap_regs_dump(const struct ap_root_info *aproot, u32 len, const char *log_path)
{
    char path[DIR_MAXLEN] = {0};
    bbox_status ret = bbox_age_add_folder(log_path, DIR_REGS, path, DIR_MAXLEN);
    BBOX_CHK_EXPR_ACTION(ret != BBOX_SUCCESS, return, "add path[%s] fail", path);

    u32 i;
    u32 devid;
    for (devid = 0; devid < aproot->top_head.device_num; devid++) {
        for (i = 0; (i < aproot->area_info.reg_regions_num) && (i < REGS_DUMP_MAX_NUM); i++) {
            bbox_ap_reg_dump(aproot, len, path, devid, i);
        }
    }
}

/*
 * @brief       : check ap ddr data
 * @param [in]  : const struct ap_root_info *ap      ap data
 * @return      : ture or false
 */
static inline bool bbox_ddr_check_ap(const struct ap_root_info *ap)
{
    return ((ap != NULL) && (ap->top_head.dump_magic == AP_DUMP_MAGIC) &&
            (ap->top_head.end_magic == AP_DUMP_END_MAGIC) && (ap->top_head.device_num <= DEVICE_OS_MAX_NUM));
}

/*
 * @brief       : bbox ap ddr dump
 * @param [in]  : const struct ap_root_info *ap   ap data
 * @param [in]  : u32 len                       data len
 * @param [in]  : const char *log_path           save to dir
 * @return      : NA
 */
void bbox_ddr_dump_ap(const struct ap_root_info *ap, u32 len, const char *log_path)
{
    BBOX_CHK_NULL_PTR(ap, return);
    BBOX_CHK_NULL_PTR(log_path, return);
    if (len < sizeof(struct ap_root_info)) {
        BBOX_ERR("len is too small. (len=%u, min_size=%u)", len, (u32)sizeof(struct ap_root_info));
        return;
    }

    if (!bbox_ddr_check_ap(ap)) {
        BBOX_WAR("data is invalid. magic(0x%x) version(0x%x) end magic(0x%x) device_num(%u)", ap->top_head.dump_magic,
                 ap->top_head.version, ap->top_head.end_magic, ap->top_head.device_num);
        return;
    }

    // 1.dump struct ap_root_info
    bbox_ap_info_dump(ap, log_path);

    // 2.dump regs
    bbox_ap_regs_dump(ap, len, log_path);
}
