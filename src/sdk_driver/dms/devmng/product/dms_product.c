/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "dms_product.h"
#ifdef CFG_FEATURE_AO_LOG_COLLECT /* 只在device侧编译 */
#include "ao_log_collect_fs.h"
#endif

int dms_product_init(void)
{
#ifdef CFG_FEATURE_AO_LOG_COLLECT
    u32 i;
    dms_ao_log_top_fs_init();
    for (i = 0; i < AO_LOG_MAX_DEV_NUM; i++) {
        dms_ao_log_fs_init(i);
        dms_ao_uart_fs_init(i); // 串口录音
        dms_ao_cnt_fs_init(i);
    }

#endif
    return 0;
}

void dms_product_exit(void)
{
#ifdef CFG_FEATURE_AO_LOG_COLLECT
    dms_ao_log_top_fs_uninit();
#endif
}