/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef UT_TEST
#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "prof_common.h"
#include "prof_sample_aicpu_channel.h"
#include "dms/dms_devdrv_info_comm.h"

typedef int (*prof_sample_register_func)(unsigned int);

static prof_sample_register_func g_prof_sample_info[] = {register_aicpu_channel, register_cus_aicpu_channel};

static void __attribute__((constructor)) halProfInit(void)
{
    unsigned int i;
    int ret;
    unsigned int split_mode = 0;
    unsigned int devid;

    ret = drvGetDeviceSplitMode(0, &split_mode);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("get split mode failed. (ret=%d)\n", ret);
        return;
    }

    if (split_mode != VMNG_NORMAL_NONE_SPLIT_MODE) {
        PROF_INFO("It is not a physical machine. (split_mode=%u)\n", split_mode);
        return;
    }

    for (i = 0; i < sizeof(g_prof_sample_info) / sizeof(prof_sample_register_func); i++) {
        for (devid = 0; devid < DEV_NUM; devid++) {
            ret = g_prof_sample_info[i](devid);
            if (ret != 0) {
                PROF_ERR("register failed. (devid=%u, i=%u, ret=%d)\n", devid, i, ret);
            }
        }
    }
    return;
}

#else
int prof_sample_ut_test(void)
{
    return 0;
}
#endif
