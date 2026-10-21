/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bbox_rdr_data_parser.h"

#include "securec.h"

#include "bbox_ddr_int.h"
#include "bbox_fs_api.h"
#include "bbox_system_api.h"
#include "bbox_print.h"
#include "bbox_common.h"
#include "bbox_file_list.h"
#include "bbox_log_common.h"
#include "os_data_parser/bbox_ddr_ap_adapter.h"
#include "bbox_cdr_max_len.h"
#include "common_parser/bbox_common_parser.h"
#include "bbox_adapt.h"
#include "bbox_comm_adapt.h"

#define TOP_HEAD_DUMP "bbox_info.txt"

#define PRINT_VALID_STR(str) (((str) != NULL) ? (str) : "UNKNOWN")

static inline s32 bbox_proxy_is_start_up(u16 type)
{
    return (((type) == (u16)BLOCK_TYPE_STARTUP) ? BBOX_TRUE : BBOX_FALSE);
}

/**
 * @brief       write top head info to file
 * @param [in]  head:       head info
 * @param [in]  log_path:    log path to save
 * @param [in]  text:       buffer to write
 * @return      success: 0; fail: other
 */
STATIC bbox_status bbox_ddr_dump_top_head(const struct rdr_top_head *head, const char *log_path,
                                          struct bbox_data_info *text)
{
    (void)bbox_data_reinit(text);
    bbox_status ret = bbox_data_print(text,
                                      "product info:\n"
                                      "==================================================\n"
                                      "product version\t 0x%x\n"
                                      "product name\t %s\n"
                                      "area number\t 0x%x\n",
                                      head->version, (char *)(uintptr_t)head->product_name, head->area_number);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "bbox_data_print top head info failed");
    ret = bbox_data_save_to_fs(text, log_path, TOP_HEAD_DUMP);
    BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "save top head info failed");
    return BBOX_SUCCESS;
}

/**
 * @brief       write base info to file
 * @param [in]  head:    head info
 * @param [in]  log_path: log path to save
 * @param [in]  text:    buffer to write
 * @return      success: 0; fail: other
 */
STATIC bbox_status bbox_ddr_dump_base_info(const struct rdr_base_info *base_info, const char *log_path,
                                           struct bbox_data_info *text, bool bcore)
{
    bbox_status ret;
    (void)bbox_data_reinit(text);
    if (bcore == true) {
        ret = bbox_data_print(text,
                              "\ncore current info:\n"
                              "==================================================\n"
                              "exceptionId\t\t 0x%x\n"
                              "dev\t\t 0x%x\n"
                              "arg\t\t 0x%x\n"
                              "core\t\t 0x%x\n"
                              "eType\t\t 0x%x\n"
                              "module\t\t %s\n"
                              "desc\t\t %s\n"
                              "date\t\t %s UTC\n",
                              base_info->excepid, base_info->devid, base_info->arg, base_info->coreid,
                              base_info->e_type, (char *)(uintptr_t)base_info->module,
                              (char *)(uintptr_t)base_info->desc, base_info->date);
    } else {
        ret = bbox_data_print(text,
                              "\ncurrent info:\n"
                              "==================================================\n"
                              "exceptionId\t\t 0x%x\n"
                              "dev\t\t 0x%x\n"
                              "arg\t\t 0x%x\n"
                              "core\t\t 0x%x\n"
                              "eType\t\t 0x%x\n"
                              "module\t\t %s\n"
                              "desc\t\t %s\n"
                              "date\t\t %s UTC\n",
                              base_info->excepid, base_info->devid, base_info->arg, base_info->coreid,
                              base_info->e_type, (char *)(uintptr_t)base_info->module,
                              (char *)(uintptr_t)base_info->desc, base_info->date);
    }
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "bbox_data_print base info failed");
    ret = bbox_data_save_to_fs(text, log_path, TOP_HEAD_DUMP);
    BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "save base info failed");
    return BBOX_SUCCESS;
}

/**
 * @brief       write info to buffer
 * @param [in]  log_buffer:  log buffer data
 * @param [out] text:       buffer to write
 * @return      success: 0; fail: other
 */
static inline bbox_status bbox_ddr_dump_define_exception(const struct rdr_log_buffer *log_buffer,
                                                         struct bbox_data_info *text)
{
    const char *module = bbox_get_core_name(log_buffer->record_info.coreid);
    const char *reason = bbox_get_reboot_reason(log_buffer->record_info.e_type);
    return bbox_data_print(
        text, "system exception code[0x%08X]: Device[%d], ModuleName[%s], ExceptionReason[%s], TimeStamp [%s].\n",
        log_buffer->record_info.excepid, log_buffer->record_info.devid, PRINT_VALID_STR(module),
        PRINT_VALID_STR(reason), log_buffer->record_info.date);
}

/**
 * @brief       write info to buffer
 * @param [in]  log_buffer:  log buffer data
 * @param [out] text:       buffer to write
 * @return      success: 0; fail: other
 */
static inline bbox_status bbox_ddr_dump_undefine_exception(const struct rdr_log_buffer *log_buffer,
                                                           struct bbox_data_info *text)
{
    return bbox_data_print(text, "system undefined exception code[0x%08X]: Device[%d], Arg[0x%x], TimeStamp[%s].\n",
                           log_buffer->record_info.excepid, log_buffer->record_info.devid, log_buffer->record_info.arg,
                           log_buffer->record_info.date);
}

/**
 * @brief       write info to buffer
 * @param [in]  log_buffer:  log buffer data
 * @param [out] text:       buffer to write
 * @return      success: 0; fail: other
 */
static inline bbox_status bbox_ddr_dump_reset_exception(const struct rdr_log_buffer *log_buffer,
                                                        struct bbox_data_info *text)
{
    const char *module = bbox_get_core_name(log_buffer->record_info.coreid);
    const char *reason = bbox_get_reboot_reason(log_buffer->record_info.e_type);
    return bbox_data_print(
        text, "system recovery code[0x%08X]: Device[%d], ModuleName[%s], ExceptionReason[%s], TimeStamp [%s].\n",
        log_buffer->record_info.excepid, log_buffer->record_info.devid, PRINT_VALID_STR(module),
        PRINT_VALID_STR(reason), log_buffer->record_info.date);
}

/**
 * @brief       write rdr log info
 * @param [in]  info:     log info
 * @param [in]  log_path:  path to write log
 * @param [in]  text:     buffer to write
 * @return      success: 0; fail: other
 */
STATIC bbox_status bbox_ddr_dump_log_info(const struct rdr_log_info *info, const char *log_path,
                                          struct bbox_data_info *text, bool bcore)
{
    bbox_status ret;
    (void)bbox_data_reinit(text);
    if (bcore == true) {
        ret = bbox_data_print(text,
                              "\ncore log info (UTC time):\n"
                              "==================================================\n"
                              "event flag\t %u\n"
                              "log num\t\t %u\n",
                              info->event_flag, info->log_num);
    } else {
        ret = bbox_data_print(text,
                              "\nlog info (UTC time):\n"
                              "==================================================\n"
                              "event flag\t %u\n"
                              "log num\t\t %u\n",
                              info->event_flag, info->log_num);
    }
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "bbox_data_print rdr log info failed");
    ret = bbox_data_save_to_fs(text, log_path, TOP_HEAD_DUMP);
    BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "save log info head failed");

    u32 i;
    u32 next = (info->log_num < RDR_LOG_BUFFER_NUM) ? 0UL : ((u32)info->next_valid_index % RDR_LOG_BUFFER_NUM);
    for (i = 0; i < RDR_MIN(info->log_num, RDR_LOG_BUFFER_NUM); i++) {
        if (info->log_buffer[next].record_type == RDR_RECORD_DEFINE_EXCEPTION) {
            ret = bbox_ddr_dump_define_exception(&info->log_buffer[next], text);
        } else if (info->log_buffer[next].record_type == RDR_RECORD_UNDEFINE_EXCEPTION) {
            ret = bbox_ddr_dump_undefine_exception(&info->log_buffer[next], text);
        } else if (info->log_buffer[next].record_type == RDR_RECORD_RESET_EXCEPTION) {
            ret = bbox_ddr_dump_reset_exception(&info->log_buffer[next], text);
        } else {
            next++;
            next %= RDR_LOG_BUFFER_NUM;
            continue;
        }

        if (ret == BBOX_SUCCESS) {
            ret = bbox_data_save_to_fs(text, log_path, TOP_HEAD_DUMP);
        }

        BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "save log(%d) failed",
                           (s32)info->log_buffer[next].record_type);
        next++;
        next %= RDR_LOG_BUFFER_NUM;
    }

    return BBOX_SUCCESS;
}

/**
 * @brief       write area info
 * @param [in]  info:       area info
 * @param [in]  log_path:    log path to save
 * @return      success: 0; fail: other
 */
STATIC bbox_status bbox_ddr_dump_areas(const struct rdr_area_info *info, const char *log_path)
{
    struct bbox_data_info buf;
    bbox_status ret = bbox_data_init(&buf, (size_t)TMP_BUFF_B_LEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "bbox_data_init areas info failed.");

    ret = bbox_data_print(&buf, "\narea info:\n"
                                "==================================================\n");
    if (ret != BBOX_SUCCESS) {
        (void)bbox_data_clear(&buf);
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox_data_print areas info failed");
    }
    (void)bbox_data_save_to_fs(&buf, log_path, TOP_HEAD_DUMP);

    s32 i;
    for (i = 0; i < RDR_AREA_MAXIMUM; i++) {
        if (bbox_core_id_valid(info[i].coreid)) {
            ret = bbox_data_print(&buf, "%-15s\t offset:0x%llx\t length:0x%x\n",
                                  PRINT_VALID_STR(bbox_get_core_name(info[i].coreid)),
                                  bbox_address_mask(info[i].offset), info[i].length);
            if (ret != BBOX_SUCCESS) {
                (void)bbox_data_clear(&buf);
                BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox_data_print areas item failed");
            }
            (void)bbox_data_save_to_fs(&buf, log_path, TOP_HEAD_DUMP);
        }
    }
    (void)bbox_data_clear(&buf);
    return BBOX_SUCCESS;
}

/**
 * @brief       write rdr head info to specified log path
 * @param [in]  head:     rdr head info
 * @param [in]  logpath:  path to write log
 * @return      NA
 */
STATIC void bbox_ddr_dump_rdr(const struct rdr_head *head, const char *log_path)
{
    BBOX_CHK_NULL_PTR(head, return);
    BBOX_CHK_NULL_PTR(log_path, return);

    struct bbox_data_info text;
    bbox_status ret = bbox_data_init(&text, (size_t)TMP_BUFF_B_LEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return, "bbox_data_init rdr data failed.");

    // output rdr_top_head
    ret = bbox_ddr_dump_top_head(&head->top_head, log_path, &text);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR("save rdr top head failed with %d", ret);
    }

    // output struct rdr_base_info
    ret = bbox_ddr_dump_base_info(&head->base_info, log_path, &text, false);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR("save rdr base info failed with %d", ret);
    }

    // output rdr_log_info
    ret = bbox_ddr_dump_log_info(&head->log_info, log_path, &text, false);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR("save rdr log info failed with %d", ret);
    }

    // output rdr_area_info
    ret = bbox_ddr_dump_areas(head->area_info, log_path);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR("save rdr area info failed with %d", ret);
    }

    (void)bbox_data_clear(&text);
}

/**
 * @brief       check whether rdr head info valid
 * @param [in]  head:    rdr head info
 * @return      true: valid, false: invalid
 */
bool bbox_ddr_dump_check(const struct rdr_head *head)
{
    return ((head != NULL) && (head->top_head.magic == FILE_MAGIC));
}

/**
 * @brief       get ddr module block number
 * @param [in]  buffer:    ddr data
 * @return      NA
 */
STATIC s32 bbox_get_block_num(const buff *buffer)
{
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);

    s32 num = 0;
    u32 magic = bbox_get_magic(buffer);
    switch (magic) {
        case BBOX_PROXY_MAGIC:
            num = ((const struct bbox_proxy_module_ctrl *)(buffer))->config.e_block_num;
            break;
        case BBOX_MODULE_MAGIC:
            num = ((const struct bbox_module_ctrl *)(buffer))->e_block_num;
            break;
        case MODULE_MAGIC:
            num = 1; // for mini & cloud, default 1 block
            break;
        default:
            break;
    }
    num = (num > BBOX_MODULE_CTRL_NUM) ? BBOX_MODULE_CTRL_NUM : num;
    return num;
}

/**
 * @brief       parse common info from ddr data with different magic version
 * @param [in]  b_index:  block index
 * @param [in]  buffer:  ddr data
 * @param [out] ddr_info: used to store ddr common info
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_parse_ddr_dump_info(s32 b_index, const buff *buffer, struct ddr_dump_info *ddr_info)
{
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(ddr_info, return BBOX_FAILURE);
    if (b_index >= BBOX_MODULE_CTRL_NUM) {
        BBOX_ERR("b_index out of range. (b_index=%d, max_num=%d)", b_index, BBOX_MODULE_CTRL_NUM);
        return BBOX_FAILURE;
    }

    bbox_status ret = BBOX_FAILURE;
    u32 magic = bbox_get_magic(buffer);
    switch (magic) {
        case BBOX_PROXY_MAGIC: {
            const struct bbox_proxy_module_ctrl *info = ((const struct bbox_proxy_module_ctrl *)(buffer));
            ddr_info->excep_id = info->block[b_index].e_main_excepid;
            ddr_info->offset = info->config.block_info[b_index].info_offset;
            ddr_info->len = info->config.block_info[b_index].info_block_len;
            // proxy module may has mixed block type. use normal parser for mixed type
            ddr_info->is_start_up = bbox_proxy_is_start_up(info->config.block_info[b_index].ctrl_type);
            ret = BBOX_SUCCESS;
            break;
        }
        case BBOX_MODULE_MAGIC: {
            const struct bbox_module_ctrl *info = ((const struct bbox_module_ctrl *)(buffer));
            ddr_info->excep_id = info->block[b_index].e_excep_id;
            ddr_info->offset = info->block[b_index].e_block_offset;
            ddr_info->len = info->block[b_index].e_info_len;
            ddr_info->is_start_up = BBOX_FALSE;
            ret = BBOX_SUCCESS;
            break;
        }
        case MODULE_MAGIC: {
            const struct exc_module_info_s *info = (const struct exc_module_info_s *)buffer;
            ddr_info->excep_id = info->cur_info.e_excep_id;
            ddr_info->offset = info->e_info_offset;
            ddr_info->len = info->e_info_len;
            ddr_info->is_start_up = BBOX_FALSE;
            ret = BBOX_SUCCESS;
            break;
        }
        default:
            break;
    }
    return ret;
}

/**
 * @brief       dump module ddr data
 * @param [in]  log_path:  path for written log
 * @param [in]  coreid:   core id
 * @param [in]  buffer:   ddr data
 * @param [in]  length:   buffer length
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_ddr_dump_data(const char *log_path, u8 coreid, const buff *buffer, u32 length)
{
    bbox_status ret = BBOX_SUCCESS;

    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);

    s32 i;
    s32 block_num = bbox_get_block_num(buffer);
    for (i = 0; i < block_num; i++) {
        struct ddr_dump_info ddr_info = {0};
        bbox_status err = bbox_parse_ddr_dump_info(i, buffer, &ddr_info);
        if (err != BBOX_SUCCESS) {
            continue;
        }

        if ((ddr_info.is_start_up == BBOX_FALSE) && (!bbox_check_excep_id(ddr_info.excep_id))) {
            BBOX_DBG("exception_id invalid, coreid[0x%x], exception_id[%u].", coreid, ddr_info.excep_id);
            continue;
        }

        BBOX_DBG("exception_id[0x%x], coreid[0x%x]", ddr_info.excep_id, coreid);
        if ((ddr_info.len > 0) && (((u64)ddr_info.offset + (u64)ddr_info.len) <= (u64)length)) {
            enum plain_text_table_type type;
            const void *addr = (const void *)(&((const char *)buffer)[ddr_info.offset]);
            enum data_parse_type data_parse_type = (ddr_info.is_start_up == BBOX_TRUE) ? STARTUP_DATA_TYPE :
                                                                                         NORMAL_DATA_TYPE;
            type = bbox_plaintext_get_type(coreid, data_parse_type);
            bbox_plain_text_header(log_path, type, buffer, i);
            err = bbox_plaintext_data(log_path, type, addr, ddr_info.len);
            ret = ((err != BBOX_SUCCESS) ? BBOX_FAILURE : ret);
        }
    }
    return ret;
}

/**
 * @brief       dump proxy module ddr data
 * @param [in]  log_path:  path for written log
 * @param [in]  coreid:   core id
 * @param [in]  buffer:   ddr data
 * @param [in]  length:   buffer length
 * @return      0: success, other: failed
 */
STATIC s32 bbox_ddr_dump_get_rmodule(const char *log_path, u8 coreid, const buff *buffer, u32 length)
{
    s32 ret = 0;

    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(length == 0, return BBOX_FAILURE, "invalid param, length[0].");

    u32 magic = bbox_get_magic(buffer);
    if ((magic != BBOX_PROXY_MAGIC) && (magic != MODULE_MAGIC)) {
        if (coreid == (u8)BBOX_LPM) {
            ret = bbox_plaintext_data(log_path, PLAINTEXT_TABLE_LPM_START, buffer, length);
        } else if (coreid == (u8)BBOX_REGDUMP) {
            ret = bbox_plaintext_data(log_path, PLAINTEXT_TABLE_REG_DUMP, buffer, length);
        } else {
            ret = BBOX_SUCCESS;
            BBOX_DBG("exception data invalid, magic[0x%x], coreid[0x%x].", magic, coreid);
        }
        return ret;
    }
    return bbox_ddr_dump_data(log_path, coreid, buffer, length);
}

/**
 * @brief       dump local module ddr data
 * @param [in]  log_path:  path for written log
 * @param [in]  coreid:   core id
 * @param [in]  buffer:   ddr data
 * @param [in]  length:   buffer length
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_ddr_dump_get_lmodule(const char *log_path, u8 coreid, const buff *buffer, u32 length)
{
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(length == 0, return BBOX_FAILURE, "invalid param, length[0].");

    u32 magic = bbox_get_magic(buffer);
    if ((magic != BBOX_MODULE_MAGIC) && (magic != MODULE_MAGIC)) {
        BBOX_DBG("exception data without valid control head, possible magic[0x%x], coreid[0x%x].", magic, coreid);
        return BBOX_SUCCESS;
    }

    if (magic == MODULE_MAGIC) { // for old ddr structure
        u32 offset = ((const struct rdr_ddr_module_infos *)(buffer))->e_info_offset;
        u32 len = ((const struct rdr_ddr_module_infos *)(buffer))->e_info_len;
        if ((len > 0) && (((u64)offset + (u64)len) <= (u64)length)) {
            const void *addr = (const void *)(&((const char *)buffer)[offset]);
            enum plain_text_table_type type = bbox_plaintext_get_type(coreid, NORMAL_DATA_TYPE);
            return bbox_plaintext_data(log_path, type, addr, len);
        }
        return BBOX_SUCCESS; // no data written
    }

    return bbox_ddr_dump_data(log_path, coreid, buffer, length);
}

/**
 * @brief       dump os kbox data if magic is valid
 * @param [in]  log_path:  path for written log
 * @param [out] buffer:   os kbox data
 * @param [in]  length:   buffer length
 * @return      0: success, other: failed
 */
STATIC s32 bbox_ddr_dump_get_dp(const char *log_path, const buff *buffer, u32 length)
{
    u32 magic = bbox_get_magic(buffer);
    if (magic != DP_MAGIC) {
        BBOX_WAR("os kbox magic[0x%x] invalid, do not parse", magic);
        return BBOX_SUCCESS;
    }
    return bbox_plaintext_data(log_path, PLAINTEXT_TABLE_DP, buffer, length);
}

/**
 * @brief       dump aos-linux kbox data if magic is valid
 * @param [in]  log_path:  path for written log
 * @param [out] buffer:   os kbox data
 * @param [in]  length:   buffer length
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_ddr_dump_get_aos_linux(const char *log_path, const buff *buffer, u32 length)
{
    u32 magic = bbox_get_magic(buffer);
    if (magic != DP_MAGIC) {
        BBOX_WAR("os kbox magic[0x%x] invalid, do not parse", magic);
        return BBOX_SUCCESS;
    }
    return bbox_plaintext_data(log_path, PLAINTEXT_TABLE_AOS_LINUX, buffer, length);
}

/**
 * @brief       dump aos-core kbox data if magic is valid
 * @param [in]  log_path:  path for written log
 * @param [out] buffer:   os kbox data
 * @param [in]  length:   buffer length
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_ddr_dump_get_aos_core(const char *log_path, const buff *buffer, u32 length)
{
    u32 magic = bbox_get_magic(buffer);
    if (magic != DP_MAGIC) {
        BBOX_WAR("aos-core kbox magic[0x%x] invalid, do not parse", magic);
        return BBOX_SUCCESS;
    }
    return bbox_plaintext_data(log_path, PLAINTEXT_TABLE_AOS_CORE, buffer, length);
}

/**
 * @brief       dump os kbox data if magic is valid
 * @param [in]  log_path:  path for written log
 * @param [out] buffer:   os kbox data
 * @param [in]  length:   buffer length
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_ddr_dump_get_sd(const char *log_path, const buff *buffer, u32 length)
{
    u32 magic = bbox_get_magic(buffer);
    if (magic != DP_MAGIC) {
        BBOX_WAR("os kbox magic[0x%x] invalid, do not parse", magic);
        return BBOX_SUCCESS;
    }
    return bbox_plaintext_data(log_path, PLAINTEXT_TABLE_SD, buffer, length);
}

/**
 * @brief       dump tee data
 * @param [in]  log_path:  path for written log
 * @param [in]  coreid:   core id
 * @param [out] buffer:   tee data
 * @param [in]  length:   buffer length
 * @return      0: success, -1: failed
 */
STATIC bbox_status bbox_ddr_dump_get_tee(const char *log_path, u8 coreid, const buff *buffer, u32 length)
{
    return bbox_ddr_dump_get_lmodule(log_path, coreid, buffer, length);
}

STATIC bbox_status bbox_ddr_dump_get_atf(const char *log_path, u8 coreid, const buff *buffer, u32 length)
{
    return bbox_ddr_dump_get_rmodule(log_path, coreid, buffer, length);
}

/**
 * @brief       dump ddr data for different core id
 * @param [in]  log_path:  path for written log
 * @param [in]  coreid:   core id
 * @param [out] buffer:   ddr data
 * @param [in]  length:   buffer length
 * @return      0: success, other: failed
 */
s32 bbox_ddr_dump_get_module(const char *log_path, u8 coreid, const buff *buffer, u32 length)
{
    s32 ret = 0;

    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(length == 0, return BBOX_FAILURE, "invalid param, length[0].");

    switch (coreid) {
        case BBOX_TS:
        case BBOX_AICPU:
        case BBOX_BIOS:
        case BBOX_LPM:
        case BBOX_LPFW:
        case BBOX_HSM:
        case BBOX_SIL:
        case BBOX_ISP:
        case BBOX_IMU:
        case BBOX_REGDUMP:
            ret = bbox_ddr_dump_get_rmodule(log_path, coreid, buffer, length);
            break;
        case BBOX_DRIVER:
        case BBOX_DVPP:
        case BBOX_NETWORK:
        case BBOX_UB:
            ret = bbox_ddr_dump_get_lmodule(log_path, coreid, buffer, length);
            break;
        case BBOX_TEEOS:
            ret = bbox_ddr_dump_get_tee(log_path, coreid, buffer, length);
            break;
        case BBOX_ATF:
            ret = bbox_ddr_dump_get_atf(log_path, coreid, buffer, length);
            break;
        case BBOX_AOS_DP:
            ret = bbox_ddr_dump_get_dp(log_path, buffer, length);
            break;
        case BBOX_AOS_LINUX:
            ret = bbox_ddr_dump_get_aos_linux(log_path, buffer, length);
            break;
        case BBOX_AOS_CORE:
            ret = bbox_ddr_dump_get_aos_core(log_path, buffer, length);
            break;
        case BBOX_AOS_SD:
            ret = bbox_ddr_dump_get_sd(log_path, buffer, length);
            break;
        case BBOX_OS:
        case BBOX_DSS:
        case BBOX_COMISOLATOR:
            break;
        default:
            BBOX_ERR("unknown coreid[0x%x].", coreid);
            break;
    }
    return ret;
}

/**
 * @brief       : plaintext process module data
 * @param [in]  : int core_id            module core id
 * @param [in]  : const u8 *data        dump data
 * @param [in]  : u32 len               data length
 * @param [in]  : const char *path      data save to path
 * @return      : != 0 fail; == 0 success
 */
s32 bbox_ddr_dump_module(u8 core_id, const u8 *data, u32 len, const char *path)
{
    char bbox_path[DIR_MAXLEN] = {0};

    BBOX_CHK_NULL_PTR(data, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);

    bbox_status ret = bbox_age_add_folder(path, DIR_BBOXDUMP, bbox_path, DIR_MAXLEN);
    BBOX_CHK_EXPR_ACTION(ret != BBOX_SUCCESS, return BBOX_FAILURE, "path add failed(%d), %s/%s", ret, path,
                         DIR_BBOXDUMP);
    return bbox_ddr_dump_get_module(bbox_path, core_id, data, len);
}

/**
 * @brief       parse ddr head and dump module data
 * @param [in]  head:      ddr data head
 * @param [in]  len:       rdr head length
 * @param [in]  log_path:   log path
 * @return      NA
 */
STATIC void bbox_ddr_dump_modules(const struct rdr_head *head, u32 len, const char *log_path)
{
    s32 i;
    u64 ap = 0;
    const struct ap_root_info *aproot = NULL;

    BBOX_CHK_NULL_PTR(log_path, return);
    BBOX_CHK_NULL_PTR(head, return);
    BBOX_CHK_EXPR_ACTION(len == 0, return, "invalid param, length[0].");

    for (i = 0; i < RDR_AREA_MAXIMUM; i++) {
        if (head->area_info[i].coreid == (u8)BBOX_OS) {
            ap = head->area_info[i].offset;
            break;
        }
    }

    if (ap == 0) {
        BBOX_WAR("Ap ddr addr is not obtained.");
        return;
    }

    aproot = (const struct ap_root_info *)((uintptr_t)head + RDR_BASEINFO_SIZE);
    for (i = 0; i < RDR_AREA_MAXIMUM; i++) {
        u8 coreid = head->area_info[i].coreid;
        if (bbox_core_id_valid(coreid) && (head->area_info[i].offset > ap) &&
            ((head->area_info[i].offset - ap) < (len - RDR_BASEINFO_SIZE))) {
            const u8 *vaddr = (const u8 *)(&((const char *)aproot)[head->area_info[i].offset - ap]);
            (void)bbox_ddr_dump_get_module(log_path, coreid, vaddr, head->area_info[i].length);
        }
    }
}

/**
 * @brief       Interface for dump DDR data into file-system.
 * @param [in]  buffer  The buffer of DDR data.
 * @param [in]  len     The length of the buffer.
 * @param [in]  log_path The path for DDRDUMP module dump data into(contain "ddrdump/").
 * @return      0 on success otherwise -1
 */
bbox_status bbox_ddr_dump(const buff *buffer, u32 len, const char *log_path)
{
    char bbox_path[DIR_MAXLEN] = {0};
    char path[DIR_MAXLEN] = {0};

    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION((len < (RDR_BASEINFO_SIZE + sizeof(struct ap_root_info))), return BBOX_FAILURE,
                         "invalid param, length[%u], min_size[%u].", len,
                         (u32)(RDR_BASEINFO_SIZE + sizeof(struct ap_root_info)));

    const struct rdr_head *head = (const struct rdr_head *)buffer;
    if (!bbox_ddr_dump_check(head)) {
        BBOX_ERR("magic(0x%x) and version(0x%x) are wrong", head->top_head.magic, head->top_head.version);
        return BBOX_FAILURE;
    }

    // 1.create ddrdump path
    bbox_status ret = bbox_age_add_folder(log_path, DIR_BBOXDUMP, bbox_path, DIR_MAXLEN);
    BBOX_CHK_EXPR_ACTION(ret != BBOX_SUCCESS, return BBOX_FAILURE, "path add failed(%d), %s/%s.", ret, bbox_path,
                         DIR_BBOXDUMP);

    // 2.dump bbox data
    bbox_ddr_dump_rdr(head, bbox_path);

    // 3.dump ap data
    const void *ap = (const void *)(&((const char *)buffer)[RDR_BASEINFO_SIZE]);
    ret = bbox_age_add_folder(bbox_path, DIR_AP, path, DIR_MAXLEN);
    BBOX_CHK_EXPR_ACTION(ret != BBOX_SUCCESS, return BBOX_FAILURE, "path add failed(%d), %s/%s.", ret, path,
                         DIR_BBOXDUMP);
    bbox_ddr_dump_ap((const struct ap_root_info *)ap, (len - RDR_BASEINFO_SIZE), (const char *)path);

    // 4.dump module log files
    bbox_ddr_dump_modules(head, len, (const char *)bbox_path);
    return BBOX_SUCCESS;
}

/**
 * @brief           join DDR module data and register information together in one block data
 * @param [in]      current data block to join in
 * @param [in,out]  out     buffer contained the data to be joined
 * @param [in]      len     buffer length of the joined data(out).
 * @return          0 on success otherwise -1
 */
bbox_status bbox_ddr_dump_joint_dump(const buff *current, buff *out, u64 len)
{
    u64 ap = 0;

    BBOX_CHK_NULL_PTR(current, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(out, return BBOX_FAILURE);

    // copy module info from current block to out buffer
    const struct rdr_head *rdr = (struct rdr_head *)out;
    u32 i;
    for (i = 0; i < RDR_AREA_MAXIMUM; i++) {
        if (rdr->area_info[i].coreid == (u8)BBOX_OS) {
            ap = rdr->area_info[i].offset;
            break;
        }
    }
    BBOX_CHK_EXPR_ACTION(ap == 0, return BBOX_FAILURE, "ap addr error, NULL.");

    // joint module information
    for (i = 0; i < RDR_AREA_MAXIMUM; i++) {
        if (bbox_core_id_valid(rdr->area_info[i].coreid) && (rdr->area_info[i].offset > ap) &&
            ((rdr->area_info[i].offset - ap) < (len - RDR_BASEINFO_SIZE))) {
            u64 offset = RDR_BASEINFO_SIZE + rdr->area_info[i].offset - ap;
            s32 ret = memcpy_s((void *)(&((char *)out)[offset]), (u32)(len - offset),
                               (const void *)(&((const char *)current)[offset]), rdr->area_info[i].length);
            BBOX_CHK_EXPR_ACTION(ret != EOK, return BBOX_FAILURE, "memcpy_s failed, ret(%d)", ret);
        }
    }

    return BBOX_SUCCESS;
}

/**
 * @brief       dump hdr data in given buffer to specified log path
 * @param [in]  buffer:     hdr data buffer
 * @param [in]  len:        buffer length
 * @param [in]  log_path:    path to write file
 */
bbox_status bbox_hdr_dump(const buff *buffer, u32 len, const char *log_path)
{
    const u32 hdr_size = 0x80000; // 512k
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(len != hdr_size, return BBOX_FAILURE, "data len invalid, now is %u, at least %u bytes.", len,
                         hdr_size);

    char path[DIR_MAXLEN] = {0};
    bbox_status ret = bbox_age_add_folder(log_path, DIR_SNAPSHOT, path, DIR_MAXLEN);
    BBOX_CHK_EXPR_ACTION(ret != BBOX_SUCCESS, return BBOX_FAILURE, "path add failed(%d), %s/%s", ret, log_path,
                         DIR_SNAPSHOT);

    return bbox_plaintext_data(path, PLAINTEXT_TABLE_HDR, buffer, hdr_size);
}

/**
 * @brief       dump cdr data in given buffer to specified log path
 * @param [in]  type:       cdr data type
 * @param [in]  buffer:     cdr data buffer
 * @param [in]  len:        buffer length
 * @param [in]  log_path:    path to write file
 */
bbox_status bbox_cdr_full_dump(enum plain_text_table_type type, const buff *buffer, u32 len, const char *log_path)
{
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(len > CDR_DDR_DATA_MAX_LEN, return BBOX_FAILURE,
                         "data len invalid, %u bytes, out of range[1 - %d].", len, (int)CDR_DDR_DATA_MAX_LEN);

    char path[DIR_MAXLEN] = {0};
    bbox_status ret = bbox_age_add_folder(log_path, DIR_MNTN, path, DIR_MAXLEN);
    BBOX_CHK_EXPR_ACTION(ret != BBOX_SUCCESS, return BBOX_FAILURE, "path add failed(%d), %s/%s", ret, log_path,
                         DIR_MNTN);

    return bbox_plaintext_data((const char *)path, type, buffer, len);
}

bbox_status bbox_cdr_dump(const buff *buffer, u32 len, const char *log_path)
{
    return bbox_cdr_full_dump(PLAINTEXT_TABLE_CDR, buffer, len, log_path);
}

bbox_status bbox_cdr_min_dump(enum plain_text_table_type type, const buff *buffer, u32 len, const char *log_path)
{
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(len > CDR_SRAM_DATA_MAX_LEN, return BBOX_FAILURE,
                         "data len invalid, %u bytes, out of range[1 - %d].", len, (int)CDR_SRAM_DATA_MAX_LEN);

    char path[DIR_MAXLEN] = {0};
    bbox_status ret = bbox_age_add_folder(log_path, DIR_MNTN, path, DIR_MAXLEN);
    BBOX_CHK_EXPR_ACTION(ret != BBOX_SUCCESS, return BBOX_FAILURE, "path add failed(%d), %s/%s", ret, log_path,
                         DIR_MNTN);

    return bbox_plaintext_data((const char *)path, type, buffer, len);
}

/**
 * @brief       mkdir parent dir for slog and dump data
 * @param [in]  type:       slog data type
 * @param [in]  buffer:     slog buffer
 * @param [in]  len:        buffer length
 * @param [in]  log_path:    path to write file
 */
bbox_status bbox_slog_dump(enum plain_text_table_type type, const buff *buffer, u32 len, const char *log_path)
{
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);

    s32 ret = -1;
    char path[DIR_MAXLEN] = {0};
    char folder[DIR_MAXLEN] = {0};
    switch (type) {
        case PLAINTEXT_TABLE_RUN_DEVICE_OS_LOG:
        case PLAINTEXT_TABLE_RUN_EVENT_LOG:
            ret = sprintf_s(folder, DIR_MAXLEN, OS_EXCPT_PATH_FORMAT, DIR_MODULELOG, DIR_SLOG, DIR_RUN);
            break;
        case PLAINTEXT_TABLE_DEBUG_DEVICE_OS_LOG:
        case PLAINTEXT_TABLE_DEBUG_DEVICE_FW_LOG:
            ret = sprintf_s(folder, DIR_MAXLEN, OS_EXCPT_PATH_FORMAT, DIR_MODULELOG, DIR_SLOG, DIR_DEBUG);
            break;
        case PLAINTEXT_TABLE_SEC_DEVICE_OS_LOG:
            ret = sprintf_s(folder, DIR_MAXLEN, OS_EXCPT_PATH_FORMAT, DIR_MODULELOG, DIR_SLOG, DIR_SEC);
            break;
        default:
            BBOX_ERR("invalid slog log file type %u.", type);
            break;
    }
    BBOX_CHK_EXPR_ACTION(ret == -1, return BBOX_FAILURE, "sprintf_s failed with %d.", ret);

    ret = bbox_age_add_folder(log_path, folder, path, DIR_MAXLEN);
    BBOX_CHK_EXPR_ACTION(ret != BBOX_SUCCESS, return BBOX_FAILURE, "path add failed(%d), %s/%s", ret, log_path,
                         DIR_MNTN);

    return bbox_plaintext_data((const char *)path, type, buffer, len);
}

bbox_status bbox_cdr_clink_dump(enum plain_text_table_type type, const buff *buffer, u32 len, const char *log_path)
{
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);

    char path[DIR_MAXLEN] = {0};
    bbox_status ret = bbox_age_add_folder(log_path, DIR_MNTN, path, DIR_MAXLEN);
    BBOX_CHK_EXPR_ACTION(ret != BBOX_SUCCESS, return BBOX_FAILURE, "path add failed(%d), %s/%s", ret, log_path,
                         DIR_MNTN);

    return bbox_plaintext_data((const char *)path, type, buffer, len);
}