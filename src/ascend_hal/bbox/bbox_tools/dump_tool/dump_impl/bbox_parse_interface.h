/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef BBOX_PARSE_INTERFACE_H
#define BBOX_PARSE_INTERFACE_H

#include "bbox_data_parser.h"
#include "bbox_int.h"

bbox_status bbox_parse_sram_mntn_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path);
bbox_status bbox_parse_log_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path);
bbox_status bbox_parse_klog_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path);
bbox_status bbox_parse_bbox_ddr_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path);
bbox_status bbox_parse_hdr_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path);
bbox_status bbox_parse_cdr_min_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path);
bbox_status bbox_parse_cdr_full_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path);
bbox_status bbox_parse_slog_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path);
bbox_status bbox_parse_cdr_clink_data(enum plain_text_table_type type, u8 *data, u32 len, const char *path);

#endif