/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "dms_user_common.h"

#define BITS_PER_INT 32U
static inline void parse_bit_shift(unsigned int *rcv, unsigned int *src, unsigned int bit_width)
{
    *rcv = (*src) & (~(0U) >> (BITS_PER_INT - bit_width));
    (*src) >>= bit_width;
}

/* SDID total 32 bits, low to high: */
#define UDEVID_BIT_LEN 16
#define DIE_ID_BIT_LEN 2
#define CHIP_ID_BIT_LEN 4
#define SERVER_ID_BIT_LEN 10
drvError_t dms_parse_sdid(uint32_t sdid, struct halSDIDParseInfo *sdid_parse)
{
    if (sdid_parse == NULL) {
        DMS_ERR("sdid parse is NULL. (sdid=%u)\n", sdid);
        return DRV_ERROR_PARA_ERROR;
    }

    parse_bit_shift(&sdid_parse->udevid, &sdid, UDEVID_BIT_LEN);
    parse_bit_shift(&sdid_parse->die_id, &sdid, DIE_ID_BIT_LEN);
    parse_bit_shift(&sdid_parse->chip_id, &sdid, CHIP_ID_BIT_LEN);
    parse_bit_shift(&sdid_parse->server_id, &sdid, SERVER_ID_BIT_LEN);

    return DRV_ERROR_NONE;
}