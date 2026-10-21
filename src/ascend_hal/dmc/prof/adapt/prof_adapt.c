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
#include "prof_adapt.h"
#include "prof_adapt_user.h"
#include "prof_adapt_kernel.h"
#include "prof_common.h"

static struct prof_adapt_core_notifier g_adapt_core_notifier = {NULL};

void prof_adapt_register_notifier(struct prof_adapt_core_notifier *notifier)
{
    g_adapt_core_notifier.poll_report = notifier->poll_report;
    g_adapt_core_notifier.chan_report = notifier->chan_report;
}

struct prof_adapt_core_notifier *prof_adapt_get_notifier(void)
{
    return &g_adapt_core_notifier;
}

drvError_t prof_adapt_register_channel(uint32_t dev_id, uint32_t chan_id, struct prof_sample_register_para *para,
                                       bool support_host_sample)
{
    return prof_user_register_channel(dev_id, chan_id, para, support_host_sample);
}

drvError_t prof_adapt_get_channels(uint32_t dev_id, struct prof_channel_list *channels)
{
    return prof_kernel_get_channels(dev_id, channels);
}

drvError_t __attribute__((weak)) prof_user_get_host_sample_chan_ops(struct prof_chan_ops **ops)
{
    PROF_ERR("Error function.\n");
    *ops = NULL;
    return (drvError_t)0;
}

drvError_t prof_adapt_get_chan_ops(uint32_t dev_id, uint32_t chan_mode, struct prof_chan_ops **ops,
                                   bool support_host_sample)
{
    drvError_t ret = prof_adapt_check_cc_mode(dev_id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    if (support_host_sample) {
        return prof_user_get_host_sample_chan_ops(ops);
    }
    if (chan_mode == (uint32_t)PROF_CHAN_MODE_USER) {
        return prof_user_get_chan_ops(ops);
    } else {
        return prof_kernel_get_chan_ops(dev_id, ops);
    }
}

#else
int prof_adapt_ut_test(void)
{
    return 0;
}
#endif
