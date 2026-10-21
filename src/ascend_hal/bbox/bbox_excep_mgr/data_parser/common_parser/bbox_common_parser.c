/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "bbox_common_parser.h"

#include "securec.h"

#include "bbox_ddr_int.h"
#include "bbox_parser_inner.h"
#include "bbox_system_api.h"
#include "bbox_print.h"
#include "bbox_utils.h"
#include "bbox_log_common.h"
#include "bbox_commlog_parser.h"
#include "bbox_log_file.h"

#define CHECK_TABLE_FIELD_LEN_ACT(offset, length, space, action) \
    do {                                                         \
        if (((space) - (offset)) < (length)) {                   \
            action;                                              \
        }                                                        \
    } while (0)

/**
 * @brief       check data offset + length in rang
 * @param [in]  offset:     data offset to read
 * @param [in]  length:     data length to read
 * @param [in]  space_size:  data total size
 * @return      0: success, other: failed
 */
STATIC bool bbox_plaintext_check_data_offset(u32 offset, u32 length, u32 space_size)
{
    if ((offset >= space_size) || (length > space_size) || ((space_size - offset) < length)) {
        return false;
    }
    return true;
}

STATIC void bbox_plaintext_element_name(const char *log_path, const char *file_name, const char *key)
{
    const size_t name_len = ELEMENT_NAME_MAX_LEN; // output element name
    char *name = (char *)bbox_malloc(name_len);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, name == NULL, return, "malloc failed.");
    s32 ret = snprintf_s(name, name_len, name_len - 1U, "%s:\n", key);
    if (ret == -1) {
        BBOX_ERR("snprintf_s %s error", key);
    } else {
        (void)bbox_save_buf_to_fs(log_path, file_name, name, (u32)ret, BBOX_TRUE);
    }
    bbox_free(name);
}

/**
 * @brief       write a newline to given log file
 * @param [in]  logpath:    path to write file
 * @param [in]  file_name:   log filename
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_plaintext_nl(const char *log_path, const char *file_name)
{
    const char *text = "\n";
    (void)bbox_save_buf_to_fs(log_path, file_name, text, (u32)strlen(text), BBOX_TRUE);
    return BBOX_SUCCESS;
}

/**
 * @brief       write hexadecimal data to file
 * @param [in]  log_path:    path to write file
 * @param [in]  file_name:   log filename
 * @param [in]  key:        key name for current data
 * @param [in]  data:       data buffer
 * @param [in]  len:        data buffer length
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_plaintext_hex_data(const char *log_path, const char *file_name, const char *key,
                                           const char *data, u32 len)
{
    BBOX_CHK_NULL_PTR(data, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(file_name, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(key, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(len > ELEM_OUTPUT_HEX_MAX_LEN, return BBOX_FAILURE, "value of %s is too large(%u).", key, len);

    struct bbox_data_info text;
    bbox_status ret = bbox_data_init(&text, (size_t)TMP_BUFF_B_LEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "bbox_data_init hex data cache failed.");

    ret = bbox_data_print(&text, "%-32s: ", key);
    if (ret != BBOX_SUCCESS) {
        (void)bbox_data_clear(&text);
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox_data_print hex data key(%s) failed", key);
    }

    s32 i;
    for (i = 0; i <= (s32)len; i++) {
        if (i < (s32)len) {
            ret = bbox_data_print(&text, "%02x", *(const u8 *)(data + i));
        } else {
            ret = bbox_data_print(&text, "\n");
        }
        if (ret != BBOX_SUCCESS) {
            (void)bbox_data_clear(&text);
            BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox_data_print hex data value failed");
        }
    }
    (void)bbox_data_save_to_fs(&text, log_path, file_name);
    (void)bbox_data_clear(&text);
    return BBOX_SUCCESS;
}

static bbox_status bbox_plaintext_str_default(const char *log_path, const char *file_name,
                                              const struct model_element *info)
{
    const size_t text_len = TMP_BUFF_S_LEN;
    char *text = (char *)bbox_malloc(text_len);
    if (text == NULL) {
        BBOX_ERR("malloc default string failed.");
        return BBOX_FAILURE;
    }

    s32 ret = snprintf_s(text, text_len, text_len - 1U, "%s: no available information.\n", info->name);
    if (ret == -1) {
        BBOX_ERR("snprintf_s %s error", info->name);
        bbox_free(text);
        return BBOX_FAILURE;
    }
    (void)bbox_save_buf_to_fs(log_path, file_name, text, (u32)ret, BBOX_TRUE);
    bbox_free(text);
    return BBOX_SUCCESS;
}

#if (!defined BBOX_UT_TEST) && (!defined BBOX_ST_TEST)
STATIC bbox_status bbox_save_text_str_default(const char *log_path, const char *file_name)
{
    const size_t text_len = TMP_BUFF_S_LEN;
    char *text = (char *)bbox_malloc(text_len);
    if (text == NULL) {
        BBOX_ERR("malloc default string failed.");
        return BBOX_FAILURE;
    }

    s32 ret = snprintf_s(text, text_len, text_len - 1U, "no available information.\n");
    if (ret == -1) {
        BBOX_ERR("snprintf_s error");
        bbox_free(text);
        return BBOX_FAILURE;
    }
    (void)bbox_save_buf_to_fs(log_path, file_name, text, (u32)ret, BBOX_TRUE);
    bbox_free(text);
    return BBOX_SUCCESS;
}
#endif

/**
 * @brief       write string data to file
 * @param [in]  log_path:    path to write file
 * @param [in]  file_name:   log filename
 * @param [in]  key:        key name for current data
 * @param [in]  data:       data buffer
 * @param [in]  len:        data buffer length
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_plaintext_str_data(const char *log_path, const char *file_name,
                                           const struct model_element *info, const char *data, u32 len)
{
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(file_name, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(info, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(info->name, return BBOX_FAILURE);

    if ((data == NULL) || (len == 0)) {
        return bbox_plaintext_str_default(log_path, file_name, info);
    }

    s32 ret;
    const size_t text_len = TMP_BUFF_S_LEN;
    char *text = (char *)bbox_malloc(text_len);
    if (text == NULL) {
        BBOX_ERR("malloc string failed.");
        return BBOX_FAILURE;
    }

    if (info->type == ELEM_OUTPUT_STR) {
        ret = snprintf_s(text, text_len, text_len - 1U, "%s: ", info->name);
    } else {
        ret = snprintf_s(text, text_len, text_len - 1U, "%s:\n", info->name);
    }

    if (ret == -1) {
        BBOX_ERR("snprintf_s %s error", info->name);
        bbox_free(text);
        return BBOX_FAILURE;
    }
    (void)bbox_save_buf_to_fs(log_path, file_name, text, (u32)ret, BBOX_TRUE);
    (void)bbox_save_buf_to_fs(log_path, file_name, data, len, BBOX_TRUE);
    if (data[len - 1U] != '\n') {
        (void)bbox_plaintext_nl(log_path, file_name);
    }
    bbox_free(text);
    return BBOX_SUCCESS;
}

/**
 * @brief       save string data to file
 * @param [in]  log_path:    path to write file
 * @param [in]  file_name:   log filename
 * @param [in]  data:       data buffer
 * @param [in]  len:        data buffer length
 * @return      0: success, other: failed
 */
#if (!defined BBOX_UT_TEST) && (!defined BBOX_ST_TEST)
STATIC bbox_status bbox_savetext_str_data(const file_hdl_t *file_hdl, const char *data, u32 length)
{
    BBOX_CHK_NULL_PTR(file_hdl->path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(file_hdl->name, return BBOX_FAILURE);

    if ((data == NULL) || (length == 0)) {
        return bbox_save_text_str_default(file_hdl->path, file_hdl->name);
    }

    (void)bbox_save_buf_to_fs(file_hdl->path, file_hdl->name, data, length, BBOX_TRUE);
    if (data[length - 1U] != '\n') {
        (void)bbox_plaintext_nl(file_hdl->path, file_hdl->name);
    }

    return BBOX_SUCCESS;
}
#endif

/**
 * @brief       write integer data to file
 * @param [in]  log_path:    path to write file
 * @param [in]  file_name:   log filename
 * @param [in]  key:        key name for current data
 * @param [in]  data:       data buffer
 * @param [in]  len:        data buffer length
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_plaintext_int_data(const char *log_path, const char *file_name, const char *key,
                                           const char *data, u32 len)
{
    BBOX_CHK_NULL_PTR(data, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(file_name, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(key, return BBOX_FAILURE);

    u64 value = 0;
    bbox_status ret = bbox_get_val_with_size(data, len, &value);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "get value with size(%u) failed.", len);

    struct bbox_data_info buf;
    ret = bbox_data_init(&buf, (size_t)TMP_BUFF_S_LEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "bbox_data_init failed.");

    ret = bbox_data_print(&buf, "%-32s: 0x%llx\n", key, value);
    if (ret != BBOX_SUCCESS) {
        (void)bbox_data_clear(&buf);
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox_data_print int data failed");
    }
    (void)bbox_data_save_to_fs(&buf, log_path, file_name);
    (void)bbox_data_clear(&buf);
    return BBOX_SUCCESS;
}

/**
 * @brief       write char data to file
 * @param [in]  log_path:    path to write file
 * @param [in]  file_name:   log filename
 * @param [in]  key:        key name for current data
 * @param [in]  data:       data buffer
 * @param [in]  len:        data buffer length
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_plaintext_char_data(const char *log_path, const char *file_name, const char *key,
                                            const char *data, u32 len)
{
    BBOX_CHK_NULL_PTR(data, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(file_name, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(key, return BBOX_FAILURE);

    bbox_plaintext_element_name(log_path, file_name, key);
    (void)bbox_save_buf_to_fs(log_path, file_name, data, len, BBOX_TRUE);
    (void)bbox_plaintext_nl(log_path, file_name);
    return BBOX_SUCCESS;
}

#define ELEM_CONST_STR_MAX_LEN (ELEMENT_NAME_MAX_LEN + 2) // 2 for extra '\n' and '\0'
/**
 * @brief       write given string into log file
 * @param [in]  log_path:    path to write file
 * @param [in]  file_name:   log filename
 * @param [in]  key:        string to written
 */
STATIC bbox_status bbox_plaintext_str_constant(const char *log_path, const char *file_name, const char *key)
{
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(file_name, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(key, return BBOX_FAILURE);

    char const_str[ELEM_CONST_STR_MAX_LEN] = {0};
    const u32 const_str_len = (u32)ELEM_CONST_STR_MAX_LEN;
    s32 ret = snprintf_s(const_str, const_str_len, const_str_len - 1, "%s\n", key);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret == -1, return BBOX_FAILURE, "concat const str type failed.");

    (void)bbox_save_buf_to_fs(log_path, file_name, const_str, (u32)strlen(const_str), BBOX_TRUE);
    return BBOX_SUCCESS;
}

#define DIVIDE_FRONT_LEN 20
#define DIVIDE_TOTAL_LEN (DIVIDE_FRONT_LEN + ELEMENT_NAME_MAX_LEN + 1) // 1 for '\n'
#define VISIBLE_MARK_START 33                                          // '!'
#define VISIBLE_MARK_END 126                                           // '~'
#define UINT_TO_VISIBLE_CHAR(mark) \
    ((((mark) >= VISIBLE_MARK_START) && ((mark) <= VISIBLE_MARK_END)) ? (char)(mark) : '=')

/**
 * @brief       write a divider line with given string. if string exceed max length, truncated str
 * @param [in]  log_path:    path to write file
 * @param [in]  file_name:   log filename
 * @param [in]  key:        string in divider line
 * @param [in]  mark:       given divider mark
 */
STATIC bbox_status bbox_plaintext_divide(const char *log_path, const char *file_name, const char *key, u32 mark)
{
    s32 i;

    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(file_name, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(key, return BBOX_FAILURE);

    char *text = (char *)bbox_malloc(DIVIDE_TOTAL_LEN + 1);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, text == NULL, return BBOX_FAILURE, "malloc failed.");

    // use given ascii mark if it is visable mark, otherwise use default '='
    char divide = UINT_TO_VISIBLE_CHAR(mark);
    for (i = 0; i < DIVIDE_TOTAL_LEN; i++) {
        text[i] = divide;
    }

    // string length is restricted
    s32 ret = snprintf_truncated_s(text + DIVIDE_FRONT_LEN, ELEM_OUTPUT_DIVIDE_MAX_LEN + 1, "%s", key);
    if (ret == -1) {
        BBOX_ERR("snprintf_s failed");
        bbox_free(text);
        text = NULL;
        return BBOX_FAILURE;
    }
    // replace null terminator filled by snprintf_s
    const s32 key_end_index = DIVIDE_FRONT_LEN + ret;
    text[key_end_index] = divide;
    // make sure divider line is end with '\n''\0'
    text[DIVIDE_TOTAL_LEN - 1] = '\n';
    text[DIVIDE_TOTAL_LEN] = '\0';

    (void)bbox_save_buf_to_fs(log_path, file_name, text, DIVIDE_TOTAL_LEN, BBOX_TRUE);

    bbox_free(text);
    text = NULL;
    return BBOX_SUCCESS;
}

/**
 * @brief       write reg data to file
 * @param [in]  log_path:    path to write file
 * @param [in]  file_name:   log filename
 * @param [in]  key:        key name for current data
 * @param [in]  buffer:     data buffer
 * @param [in]  len:        data buffer length
 * @return      0: success, other: failed
 */
#define REG_SIZE 4U                                 // register size is 4
#define REG_MAX_NUM 1000000U                        // register max num is 1000000
#define REG_BLOCK_MAX_BYTE (REG_SIZE * REG_MAX_NUM) // registers block size is 4000000 byte(1000000 * 4)
#define REG_MAX_COL_NUM 4U                          // max has 4 columns per row, represent 4 registers
#define REG_MAX_ROW_NUM (REG_MAX_NUM / 4U)          // 1000000 / 4 = 250000, max row num
#define REG_BYTE_PRE_ROW (REG_MAX_COL_NUM * REG_SIZE)
STATIC bbox_status bbox_plaintext_reg_data(const char *log_path, const char *file_name, const char *key,
                                           const char *buffer, u32 len)
{
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(file_name, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(key, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(len > REG_BLOCK_MAX_BYTE, return BBOX_FAILURE, "length is too large: %u.", len);
    BBOX_CHK_EXPR_ACTION((len % REG_SIZE) != 0, return BBOX_FAILURE, "length is invalid: %u.", len);

    bbox_plaintext_element_name(log_path, file_name, key);

    struct bbox_data_info text;
    bbox_status ret = bbox_data_init(&text, (size_t)TMP_BUFF_S_LEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "bbox_data_init reg data failed.");

    u32 i;
    const char *p_str = buffer;
    const char *q = NULL;
    for (i = 0; (i < REG_MAX_ROW_NUM) && (p_str < (buffer + len)); i++) {
        u32 cnt = 0;
        ret = bbox_data_print(&text, "0x%x ", i);
        if (ret != BBOX_SUCCESS) {
            (void)bbox_data_clear(&text);
            BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox_data_print regs data line nums failed");
        }
        for (q = p_str; (q < (p_str + REG_BYTE_PRE_ROW)) && (q <= (buffer + len - REG_SIZE)); q += REG_SIZE) {
            ret = bbox_data_print(&text, "%08x", *(const u32 *)q);
            if (ret != BBOX_SUCCESS) {
                (void)bbox_data_clear(&text);
                BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox_data_print regs data value failed");
            }
            cnt++;
            if ((cnt % REG_MAX_COL_NUM) != 0) {
                ret = bbox_data_print(&text, " ");
                BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "bbox_data_print blank failed");
            }
        }
        ret = bbox_data_print(&text, "\n");
        BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "bbox_data_print enter failed");
        p_str += REG_BYTE_PRE_ROW;
        (void)bbox_data_save_to_fs(&text, log_path, file_name);
    }
    (void)bbox_data_clear(&text);
    return BBOX_SUCCESS;
}

/**
 * @brief       write sram bin data to file
 * @param [in]  log_path:    path to write file
 * @param [in]  file_name:   log filename
 * @param [in]  key:        key name for current data
 * @param [in]  buffer:     data buffer
 * @param [in]  len:        data buffer length
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_plaintext_bin_data(const char *log_path, const char *file_name, const char *key,
                                           const char *buffer, u32 len)
{
    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(file_name, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(key, return BBOX_FAILURE);

    (void)bbox_save_buf_to_fs(log_path, file_name, (char *)(uintptr_t)buffer, len, BBOX_FALSE);

    return BBOX_SUCCESS;
}

/**
 * @brief       parse item in data model table with given item table
 * @param [in]  file_hdl:    path struct for write file
 * @param [in]  table:      plaintext map table item
 * @param [in]  num:        num of item in table
 * @param [in]  data:       data buffer
 * @param [in]  length:     data buffer length
 * @return      0: success, other: failed
 */
bbox_status bbox_plaintext_out_data(const file_hdl_t *file_hdl, const model_elem_t *table, u32 num, const char *data,
                                    u32 length)
{
    s32 ret;
    u32 len;

    if ((table->offset >= length) || (num != 1)) {
        BBOX_ERR("output item: offset[%u], count[%u], out of range: length(%u) count(1), filename=%s", table->offset,
                 num, length, file_hdl->name);
        return BBOX_FAILURE;
    }

    if ((table->type == ELEM_OUTPUT_STR) || (table->type == ELEM_OUTPUT_STR_NL)) {
        char *str = (char *)(uintptr_t)(data + table->offset);
        u32 rest = length - table->offset;
        rest = RDR_MIN(rest, table->max_size);
        len = (rest > 0) ? (rest - 1U) : 0U; // get end index of string
        str[len] = '\0';                     // append string termintor to end of data block
        u32 tmp = (u32)strlen(str);
        len = RDR_MIN(rest, tmp);
        ret = bbox_plaintext_str_data(file_hdl->path, file_hdl->name, table, data + table->offset, len);
    } else if (table->type == ELEM_OUTPUT_HEX) {
        CHECK_TABLE_FIELD_LEN_ACT(table->offset, table->size, length, return BBOX_FAILURE);
        len = RDR_MIN(length, table->size);
        len = RDR_MIN(len, (u32)ELEM_OUTPUT_HEX_MAX_LEN); // strict hex length no more than max len
        ret = bbox_plaintext_hex_data(file_hdl->path, file_hdl->name, table->name, data + table->offset, len);
    } else if (table->type == ELEM_OUTPUT_INT) {
        CHECK_TABLE_FIELD_LEN_ACT(table->offset, table->size, length, return BBOX_FAILURE);
        ret = bbox_plaintext_int_data(file_hdl->path, file_hdl->name, table->name, data + table->offset, table->size);
    } else if (table->type == ELEM_OUTPUT_CHAR) {
        len = RDR_MIN(length - table->offset, table->size);
        ret = bbox_plaintext_char_data(file_hdl->path, file_hdl->name, table->name, data + table->offset, len);
    } else if (table->type == ELEM_OUTPUT_NL) {
        ret = bbox_plaintext_nl(file_hdl->path, file_hdl->name);
    } else if (table->type == ELEM_OUTPUT_DIVIDE) {
        ret = bbox_plaintext_divide(file_hdl->path, file_hdl->name, table->name, table->split_char);
    } else if (table->type == ELEM_OUTPUT_STR_CONST) {
        ret = bbox_plaintext_str_constant(file_hdl->path, file_hdl->name, table->name);
    } else if (table->type == ELEM_OUTPUT_REG) {
        CHECK_TABLE_FIELD_LEN_ACT(table->offset, table->size, length, return BBOX_FAILURE);
        ret = bbox_plaintext_int_data(file_hdl->path, file_hdl->name, table->name, data + table->offset, table->size);
    } else if (table->type == ELEM_OUTPUT_R4_BLOCK) {
        CHECK_TABLE_FIELD_LEN_ACT(table->offset, table->size, length, return BBOX_FAILURE);
        ret = bbox_plaintext_reg_data(file_hdl->path, file_hdl->name, table->name, data + table->offset, table->size);
    } else if (table->type == ELEM_OUTPUT_BIN) {
        CHECK_TABLE_FIELD_LEN_ACT(table->offset, table->size, length, return BBOX_FAILURE);
        ret = bbox_plaintext_bin_data(file_hdl->path, file_hdl->name, table->name, data + table->offset, table->size);
    } else {
        ret = BBOX_SUCCESS;
    }

    BBOX_CHK_EXPR(ret != BBOX_SUCCESS, "plaintext print item[0x%x] failed with %d", (u32)table->type, ret);
    return ret;
}

/**
 * @brief       check whether table condition is met.
 *              if not, return index offset to skip specified model elements
 *              if condition met, return 0 to continue parsing next elements
 * @param [in]  file_hdl:    path struct for write file
 * @param [in]  table:      plaintext map table item
 * @param [in]  num:        num of item in table
 * @param [in]  data:       data buffer
 * @param [in]  length:     data buffer length
 * @return      failed: -1, success: >=0, value of parsed item count
 */
static s32 bbox_plaintext_ctrl_cond(const file_hdl_t *file_hdl, const model_elem_t *table, u32 num, const char *data,
                                    u32 length)
{
    u64 value = 0;
    u32 condition;

    s32 ret = bbox_get_val_with_size(data, length, &value);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "get value with size(%u) failed.", length);
    UNUSED(num);

    u64 cmp_value = table->value;
    if (cmp_value == value) {
        condition = ELEM_EQUAL;
    } else if (cmp_value > value) {
        condition = ELEM_GRATER;
    } else {
        condition = ELEM_LESS;
    }

    // if condition is met, do not jump item(offset 0). if not met, jump to specified item with index offset
    u32 cmp_result = (compare_condition(table->type, condition) ? BBOX_FALSE : BBOX_TRUE);
    u32 index_offset = ((cmp_result == BBOX_TRUE) ? 0U : table->index_cnt);

    // if label is empty, jump table without write to file
    if (strlen(table->name) == 0) {
        return (s32)index_offset;
    }

    const u32 text_len = TMP_BUFF_S_LEN;
    char *text = (char *)bbox_malloc(text_len);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, text == NULL, return BBOX_FAILURE, "malloc failed.");

    ret = snprintf_s(text, text_len, text_len - 1U, "%-32s: 0x%llx\n", table->name, value);
    if (ret == -1) {
        bbox_free(text);
        BBOX_ERR("snprintf_s error");
        return BBOX_FAILURE;
    }
    (void)bbox_save_buf_to_fs(file_hdl->path, file_hdl->name, text, (u32)ret, BBOX_TRUE);
    bbox_free(text);
    return (s32)index_offset;
}

/**
 * @brief       must use together: ELEM_CTRL_LOOP_BLOCK, ELEM_CTRL_BLOCK_VALUE, ELEM_CTRL_BLOCK_TABLE
 * @param [in]  file_hdl:    path struct for write file
 * @param [in]  table:      plaintext map table item
 * @param [in]  num:        num of item in table
 * @param [in]  data:       data buffer
 * @param [in]  length:     data buffer length
 * @return      failed: -1, success: >=0, value of parsed item count
 */
STATIC s32 bbox_plaintext_loop_block(const file_hdl_t *file_hdl, const model_elem_t table[], u32 num, const char *data,
                                     u32 length)
{
    u32 i;
    u64 block_total = 0;
    const s32 block_step = 0;       // ELEM_CTRL_LOOP_BLOCK
    const s32 block_value_step = 1; // ELEM_CTRL_BLOCK_VALUE
    const s32 block_table_step = 2; // ELEM_CTRL_BLOCK_TABLE
    const s32 block_elem_num = 3;   // ELEM_CTRL_LOOP_BLOCK, ELEM_CTRL_BLOCK_VALUE, ELEM_CTRL_BLOCK_TABLE

    // check next elemts
    if ((num < (u32)block_elem_num) || (table[block_value_step].type != ELEM_CTRL_BLOCK_VALUE) ||
        (table[block_table_step].type != ELEM_CTRL_BLOCK_TABLE)) {
        BBOX_ERR("invalid ctrl item type[%d] or item type[%d].", (s32)table[block_value_step].type,
                 (s32)table[block_table_step].type);
        return BBOX_FAILURE;
    }

    // check block config
    block_total = (u64)table[block_value_step].num * (u64)table[block_value_step].size;
    if (block_total < (u64)table[block_step].size) {
        BBOX_ERR("invalid ctrl item, block[%u * %u], but block size[%u].", table[block_value_step].num,
                 table[block_value_step].size, table[block_step].size);
        return BBOX_FAILURE;
    }

    // process block data
    for (i = 0; i < table[block_value_step].num; i++) {
        const u32 size = table[block_value_step].size;
        u64 offset_in_block = (u64)size * i;
        if (offset_in_block > UINT32_MAX) {
            BBOX_ERR("offset_in_block overflow. (size=%u; i=%u)", size, i);
            return BBOX_FAILURE;
        }
        bool check = bbox_plaintext_check_data_offset(table[block_step].offset, (u32)offset_in_block, length);
        BBOX_CHK_EXPR_ACTION(check == false, return BBOX_FAILURE, "check item[ELEM_CTRL_LOOP_BLOCK] failed.");
        const char *block = (const char *)data + table[block_step].offset + offset_in_block;
        s32 ret = bbox_plaintext_data(file_hdl->path, table[block_table_step].table_enum_type, block, size);
        if (ret == BBOX_FAILURE) {
            return BBOX_FAILURE;
        }
    }
    return (block_elem_num - 1);
}

/**
 * @brief       check switch/case option
 * @param [in]  table:          plaintext map table item
 * @param [in]  num:            num of item in table
 * @param [in]  case_max_num:   max num
 * @return      failed: -1, success: >=0, case count
 */
STATIC s32 bbox_plaintext_switch_check(const model_elem_t table[], u32 num, u32 case_max_num)
{
    // check num
    if (num <= 1) {
        BBOX_ERR("invalid ctrl item type[0x%x], num[%u].", (u32)table[0].type, num);
        return BBOX_FAILURE;
    }
    // check next type, mid must be ELEM_CTRL_OUT_CASE, end must be ELEM_CTRL_OUT_DCASE
    u32 i;
    for (i = 1; i < RDR_MIN(num - 1U, case_max_num); i++) {
        if (table[i].type == ELEM_CTRL_OUT_DCASE) {
            return (s32)i;
        }
        if (table[i].type != ELEM_CTRL_OUT_CASE) {
            BBOX_ERR("invalid ctrl item type[0x%x], num[%u].", (u32)table[i].type, i);
            return BBOX_FAILURE;
        }
    }

    return BBOX_FAILURE;
}

/**
 * @brief       print swtich key: case key
 * @param [in]  file_hdl:    path struct for write file
 * @param [in]  key:        swtich key
 * @param [in]  data:       case key
 * @return      failed: -1, success: 0
 */
STATIC bbox_status bbox_plaintext_switch_out(const file_hdl_t *file_hdl, const char *key, const char *data)
{
    const u32 text_len = 256; // 256 is enough.
    char *text = (char *)bbox_malloc(text_len);
    if (text == NULL) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "malloc failed.");
    }
    // write buffer
    s32 ret = snprintf_s(text, text_len, text_len - 1U, "%-32s: %s\n", key, data);
    if (ret == -1) {
        bbox_free(text);
        text = NULL;
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "snprintf_s failed.");
    }
    // save to file
    (void)bbox_save_buf_to_fs(file_hdl->path, file_hdl->name, text, (u32)ret, BBOX_TRUE);
    bbox_free(text);
    text = NULL;
    return BBOX_SUCCESS;
}

/**
 * @brief       parse ELEM_CTRL_SWITCH
 * @param [in]  file_hdl:    path struct for write file
 * @param [in]  table:      plaintext map table item
 * @param [in]  num:        num of item in table
 * @param [in]  data:       data buffer
 * @param [in]  length:     data buffer length
 * @return      failed: -1, success: >=0, value of parsed item count
 */
STATIC s32 bbox_plaintext_switch(const file_hdl_t *file_hdl, const model_elem_t table[], u32 num, const char *data,
                                 u32 length)
{
    const u32 case_max_num = 10; // max num is 10
    // check case type, return case num
    s32 switch_num = bbox_plaintext_switch_check(table, num, case_max_num);
    if (switch_num <= 0) {
        return BBOX_FAILURE;
    }
    if (bbox_plaintext_check_data_offset(table[0].offset, table[0].size, length) == false) {
        return BBOX_FAILURE;
    }
    u64 value;
    s32 ret = bbox_get_val_with_size(data + table[0].offset, table[0].size, &value);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "get value with size(%u) failed.",
                       table[0].size);

    u32 i;
    for (i = 1; i < RDR_MIN(num - 1U, case_max_num); i++) {
        if ((table[i].type == ELEM_CTRL_OUT_DCASE) || ((u64)table[i].value == value)) {
            (void)bbox_plaintext_switch_out(file_hdl, table[0].name, table[i].name);
            return switch_num;
        }
    }
    return BBOX_FAILURE;
}

/**
 * @brief       parse control item in data model table with given item table
 * @param [in]  file_hdl:    path struct for write file
 * @param [in]  table:      plaintext map table item
 * @param [in]  num:        num of item in table
 * @param [in]  data:       data buffer
 * @param [in]  length:     data buffer length
 * @return      failed: -1, success: >=0, value of parsed item count
 */
STATIC s32 bbox_plaintext_ctrl_data(const file_hdl_t *file_hdl, const model_elem_t table[], u32 num, const char *data,
                                    u32 length)
{
    s32 ret;
    const s32 control_label_offset = 1;

    if (table[0].type == ELEM_CTRL_TABLE_GOTO) {
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, num <= 1, return BBOX_FAILURE, "invalid ctrl item num[%u].", num);
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, table[1].type != ELEM_CTRL_TABLE_RANGE, return BBOX_FAILURE,
                           "invalid ctrl item type[0x%x].", (u32)table[1].type);
        ret = bbox_plaintext_data(file_hdl->path, table[1].table_enum_type, data + table[0].offset, table[0].size);
        // move table index to next valid index
        ret = (ret == BBOX_FAILURE) ? ret : control_label_offset;
    } else if (table[0].type == ELEM_CTRL_COMPARE) {
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, num <= 1, return BBOX_FAILURE, "invalid ctrl item num[%u].", num);
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, !Compare_class(table[1].type), return BBOX_FAILURE,
                           "invalid ctrl item type[0x%x].", (u32)table[1].type);
        ret = bbox_plaintext_ctrl_cond(file_hdl, &table[1], num - 1U, data + table[0].offset, table[0].size);
        // move table index to next valid index
        ret = (ret == BBOX_FAILURE) ? ret : (ret + control_label_offset);
    } else if (table[0].type == ELEM_CTRL_LOOP_BLOCK) {
        ret = bbox_plaintext_loop_block(file_hdl, table, num, data, length);
    } else if (table[0].type == ELEM_CTRL_SWITCH) {
        ret = bbox_plaintext_switch(file_hdl, table, num, data, length);
    } else {
        ret = BBOX_SUCCESS;
    }

    return ret;
}

/**
 * @brief       parse feature item in data model table with given item table
 * @param [in]  file_hdl:    path struct for write file
 * @param [in]  table:      plaintext map table item
 * @param [in]  num:        num of item in table
 * @param [in]  data:       data buffer
 * @param [in]  length:     data buffer length
 * @return      failed: -1, success: >=0, value of parsed item count
 */
STATIC s32 bbox_plaintext_ftr_data(const file_hdl_t *file_hdl, const model_elem_t table[], u32 num, const char *data,
                                   u32 length)
{
    s32 parsed_count = 0;
    if (((u64)table->index_offset + table->index_cnt) > num) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "feature item offset %u, count %u, out of range %u",
                      table->index_offset, table->index_cnt, num);
    }

    switch (table->type) {
        case ELEM_FEATURE_LOOPBUF:
        case ELEM_FEATURE_CHARLOG:
        case ELEM_FEATURE_LISTLOG:
            parsed_count = bbox_comm_log_parse_data(file_hdl, table, num, data, length);
            break;
        default:
            break;
    }

    return parsed_count;
}

/**
 * @brief       parse given data and write data to file recursive
 * @param [in]  file_hdl:    path struct for write file
 * @param [in]  map:        plaintext data map
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      BBOX_SUCCESS: success, BBOX_FAILURE: failed
 */
static bbox_status bbox_plaintext_data_recur(const file_hdl_t *file_hdl, const plaintext_map_t *map, const char *data,
                                             u32 length)
{
    u32 i;
    static u32 call_depth;
    const u32 max_depth = 10; // max redirection depth is 10

    if (call_depth >= max_depth) { // if depth reach max limitation, ignore table redirection
        BBOX_WAR("exceed max table redirection depth, ignore redirection to table[%d].", (s32)map->type);
        return BBOX_SUCCESS;
    }
    call_depth++;
    for (i = 0; i < map->num; i++) {
        s32 parsed_count;
        const struct model_element *table = map->table;
        if (output_class(table[i].type)) {
            parsed_count = bbox_plaintext_out_data(file_hdl, &table[i], 1, data, length);
        } else if (Control_class(table[i].type)) {
            parsed_count = bbox_plaintext_ctrl_data(file_hdl, &table[i], map->num - i, data, length);
        } else if (Feature_class(table[i].type)) {
            parsed_count = bbox_plaintext_ftr_data(file_hdl, &table[i], map->num - i, data, length);
        } else {
            parsed_count = -1;
        }
        if (parsed_count < 0) {
            BBOX_ERR("save [%s], item[%u][%s], type[%d] failed.", map->fname, i, table[i].name, (s32)table[i].type);
            call_depth--;
            return BBOX_FAILURE;
        }
        i += (parsed_count > 0) ? (u32)parsed_count : 0U;
    }
    call_depth--;
    return BBOX_SUCCESS;
}

/**
 * @brief       parse given data and write data to file
 * @param [in]  log_path:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      BBOX_SUCCESS: success, BBOX_FAILURE: failed
 */
bbox_status bbox_plaintext_data(const char *log_path, enum plain_text_table_type type, const void *buffer, u32 length)
{
    const plaintext_map_t *map = bbox_plaintext_get_map(type);
    if (map == NULL) {
        BBOX_ERR_CTRL(BBOX_WAR, return BBOX_FAILURE, "invalid type %d for plaintext map.", (s32)type);
    }

    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(map->fname, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(map->table, return BBOX_FAILURE);

    file_hdl_t file_hdl = {log_path, map->fname, -1};
    return bbox_plaintext_data_recur(&file_hdl, map, (const char *)buffer, length);
}

/**
 * @brief       write data to file
 * @param [in]  log_path:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      BBOX_SUCCESS: success, BBOX_FAILURE: failed
 */
#if (!defined BBOX_UT_TEST) && (!defined BBOX_ST_TEST)
bbox_status bbox_savetext_data(const char *log_path, enum plain_text_table_type type, const void *buffer, u32 length)
{
    const plaintext_map_t *map = bbox_plaintext_get_map(type);
    if (map == NULL) {
        BBOX_ERR_CTRL(BBOX_WAR, return BBOX_FAILURE, "invalid type %d for plaintext map.", (s32)type);
    }

    BBOX_CHK_NULL_PTR(buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(map->fname, return BBOX_FAILURE);

    file_hdl_t file_hdl = {log_path, map->fname, -1};
    return bbox_savetext_str_data(&file_hdl, (const char *)buffer, length);
}
#endif

/**
 * @brief       parse given data and write data to file under bbox dir
 * @param [in]  log_path:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      0: success, other: failed
 */
bbox_status bbox_bbox_plaintext_data(const char *log_path, enum plain_text_table_type type, const void *data,
                                     u32 length)
{
    char sub_path[DIR_MAXLEN];

    bbox_status ret = bbox_create_bbox_sub_path(sub_path, DIR_MAXLEN, log_path);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox create bbox sub path failed with %d.", ret);
    }

    return bbox_plaintext_data(sub_path, type, data, length);
}

/**
 * @brief       write data to file under bbox dir
 * @param [in]  log_path:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      0: success, other: failed
 */
#if (!defined BBOX_UT_TEST) && (!defined BBOX_ST_TEST)
bbox_status bbox_bbox_savetext_data(const char *log_path, enum plain_text_table_type type, const void *data, u32 length)
{
    char sub_path[DIR_MAXLEN];

    bbox_status ret = bbox_create_bbox_sub_path(sub_path, DIR_MAXLEN, log_path);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox create bbox sub path failed with %d.", ret);
    }

    return bbox_savetext_data(sub_path, type, data, length);
}
#endif

/**
 * @brief       parse given data and write data to file under mntn dir
 * @param [in]  log_path:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      0: success, other: failed
 */
bbox_status bbox_mntn_plaintext_data(const char *log_path, enum plain_text_table_type type, const void *data,
                                     u32 length)
{
    char sub_path[DIR_MAXLEN];

    bbox_status ret = bbox_create_mntn_sub_path(sub_path, DIR_MAXLEN, log_path);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR_CTRL(BBOX_ERR, return ret, "bbox create mntn sub path failed.");
    }

    return bbox_plaintext_data(sub_path, type, data, length);
}

/**
 * @brief       parse given data and write data to file under log dir
 * @param [in]  log_path:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  length:     data buffer length
 * @return      0: success, other: failed
 */
bbox_status bbox_log_plaintext_data(const char *log_path, enum plain_text_table_type type, const void *data, u32 length)
{
    char sub_path[DIR_MAXLEN];

    bbox_status ret = bbox_create_log_sub_path(sub_path, DIR_MAXLEN, log_path);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR_CTRL(BBOX_ERR, return ret, "bbox create log sub path failed.");
    }
    return bbox_plaintext_data(sub_path, type, data, length);
}

/**
 * @brief       if data head has time & exceptionid then write then to file
 * @param [in]  log_path:    path to write file
 * @param [in]  type:       data type
 * @param [in]  buffer:     data buffer
 * @param [in]  block_id:    exception block id to write
 * @return      NA
 */
void bbox_plain_text_header(const char *log_path, enum plain_text_table_type type, const void *buffer, s32 block_id)
{
    const char *fname = bbox_plaintext_get_fname(type);
    BBOX_CHK_NULL_PTR(buffer, return);
    BBOX_CHK_NULL_PTR(log_path, return);
    BBOX_CHK_NULL_PTR(fname, return);

    bbox_time_t tm = {0, 0};
    u32 excep_id = 0;
    u32 magic = bbox_get_magic(buffer);
    switch (magic) {
        case BBOX_PROXY_MAGIC: {
            excep_id = ((const struct bbox_proxy_module_ctrl *)(buffer))->block[block_id].e_main_excepid;
            tm = ((const struct bbox_proxy_module_ctrl *)(buffer))->block[block_id].e_clock;
            break;
        }
        case BBOX_MODULE_MAGIC: {
            excep_id = ((const struct bbox_module_ctrl *)(buffer))->block[block_id].e_excep_id;
            tm = ((const struct bbox_module_ctrl *)(buffer))->block[block_id].e_clock;
            break;
        }
        case MODULE_MAGIC: {
            excep_id = ((const struct exc_module_info_s *)(buffer))->cur_info.e_excep_id;
            tm = ((const struct exc_module_info_s *)(buffer))->cur_info.e_clock;
            break;
        }
        default:
            break;
    }

    BBOX_CHK_EXPR_CTRL(BBOX_DBG, !bbox_check_excep_id(excep_id), return,
                       "check %s block[%d] data(magic: %u, exception_id: %u).", fname, block_id, magic, excep_id);

    char date[DATATIME_MAXLEN] = {0};
    bbox_get_date(&tm, date, DATATIME_MAXLEN);

    struct bbox_data_info text;
    bbox_status ret = bbox_data_init(&text, (size_t)TMP_BUFF_S_LEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return, "bbox_data_init plaintext header failed.");
    ret = bbox_data_print(&text, "block id: %d, exception id: 0x%x, time: %s\n", block_id, excep_id, date);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR("bbox_data_print plaintext header failed");
    } else {
        (void)bbox_data_save_to_fs(&text, log_path, fname);
    }
    (void)bbox_data_clear(&text);
    return;
}
