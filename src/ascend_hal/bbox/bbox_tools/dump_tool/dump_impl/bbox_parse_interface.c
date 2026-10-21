/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bbox_parse_interface.h"
#include "bbox_data_parser.h"
#include "bbox_int.h"

/**
 * @brief       parse common ddr data with given type to mntn dir
 * @param [in]  type:        data parse type
 * @param [in]  data:        data buffer
 * @param [in]  len:         buffer length
 * @param [in]  path:        path to store data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_parse_sram_mntn_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path)
{
    return bbox_mntn_plaintext_data(path, type, (const char *)data, len);
}

/**
 * @brief       parse common log data with given parse type
 * @param [in]  type:        data parse type
 * @param [in]  data:        data buffer
 * @param [in]  len:         buffer length
 * @param [in]  path:        path to store data
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_parse_log_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path)
{
    return bbox_log_plaintext_data(path, type, (const char *)data, len);
}

bbox_status bbox_parse_slog_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path)
{
    return bbox_slog_dump(type, (const char *)data, len, path);
}

bbox_status bbox_parse_hdr_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path)
{
    UNUSED(type);
    return bbox_hdr_dump((const char *)data, len, path);
}

bbox_status bbox_parse_klog_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path)
{
    UNUSED(type);
    return bbox_klog_dump((const char *)data, len, path);
}

bbox_status bbox_parse_bbox_ddr_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path)
{
    UNUSED(type);
    return bbox_ddr_dump((const char *)data, len, path);
}

bbox_status bbox_parse_cdr_min_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path)
{
    return bbox_cdr_min_dump(type, (const char *)data, len, path);
}

bbox_status bbox_parse_cdr_full_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path)
{
    return bbox_cdr_full_dump(type, (const char *)data, len, path);
}

bbox_status bbox_parse_cdr_clink_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path)
{
    return bbox_cdr_clink_dump(type, (const char *)data, len, path);
}