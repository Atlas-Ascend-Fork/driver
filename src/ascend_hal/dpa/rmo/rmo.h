/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef RMO_H
#define RMO_H

#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "rmo_ioctl.h"

#ifndef EMU_ST
#include "dmc_user_interface.h"
#else
#include <pthread.h>
#include "ut_log.h"
#endif

#define rmo_err(fmt, ...) DRV_ERR(HAL_MODULE_TYPE_DP_PROC_MNG, fmt, ##__VA_ARGS__)
#define rmo_warn(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_DP_PROC_MNG, fmt, ##__VA_ARGS__)
#define rmo_info(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_DP_PROC_MNG, fmt, ##__VA_ARGS__)
#define rmo_debug(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_DP_PROC_MNG, fmt, ##__VA_ARGS__)
#define rmo_event(fmt, ...) DRV_NOTICE(HAL_MODULE_TYPE_DP_PROC_MNG, fmt, ##__VA_ARGS__)

#ifdef EMU_ST
#define THREAD__ __thread
#else
#define THREAD__
#endif

int rmo_get_fd(void);
int rmo_cmd_ioctl(unsigned long cmd, void *para);

int rmo_res_info_check(struct res_map_info *res_info);

drvError_t dpa_res_map(unsigned int dev_id, struct res_map_info *res_info, unsigned long *va, unsigned int *len);
drvError_t dpa_res_unmap(unsigned int dev_id, struct res_map_info *res_info);
drvError_t dpa_res_read(unsigned int dev_id, struct res_map_info *res_info, void *data, unsigned int len);
drvError_t dpa_res_write(unsigned int dev_id, struct res_map_info *res_info, void *data, unsigned int len);

#endif
