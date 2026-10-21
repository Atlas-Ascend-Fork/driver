/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DRV_LOG_URMA_H__
#define __DRV_LOG_URMA_H__

#include <stdint.h>
#include "ascend_hal_define.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*alog_print_func)(int32_t module_id, int32_t level, const char *fmt, ...);

int32_t drv_log_urma_register(alog_print_func log_print, uint32_t log_level);
void drv_log_urma_unregister(void);
int32_t drv_log_set_urma_log_level(uint32_t level);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_LOG_URMA_H__ */
