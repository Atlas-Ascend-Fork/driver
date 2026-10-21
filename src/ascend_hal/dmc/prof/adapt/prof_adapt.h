/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef PROF_ADAPT_H
#define PROF_ADAPT_H
#include <stdbool.h>
#include "prof_common.h"

typedef enum {
    PROF_CHAN_MODE_USER = 0,
    PROF_CHAN_MODE_KERNEL,
} PROF_CHAN_MODE_TYPE;

struct prof_chan_ops {
    drvError_t (*init)(uint32_t dev_id, uint32_t chan_id, bool event_flag, char **priv);
    void (*uninit)(char **priv);
    drvError_t (*start)(uint32_t dev_id, uint32_t chan_id, struct prof_user_start_para *para, char *priv);
    drvError_t (*stop)(uint32_t dev_id, uint32_t chan_id, struct prof_user_stop_para *para, char *priv);
    drvError_t (*flush)(uint32_t dev_id, uint32_t chan_id, uint32_t *data_len, char *priv);
    int (*read)(uint32_t dev_id, uint32_t chan_id, struct prof_user_read_para *para, char *priv);
    drvError_t (*query)(uint32_t dev_id, uint32_t chan_id, uint32_t *avail_len, char *priv);
    drvError_t (*report)(uint32_t dev_id, uint32_t chan_id, void *data, uint32_t data_len, char *priv);
};
drvError_t prof_adapt_get_chan_ops(uint32_t dev_id, uint32_t chan_mode, struct prof_chan_ops **ops,
                                   bool support_host_sample);

drvError_t prof_adapt_register_channel(uint32_t dev_id, uint32_t chan_id, struct prof_sample_register_para *para,
                                       bool support_host_sample);
drvError_t prof_adapt_get_channels(uint32_t dev_id, struct prof_channel_list *channels);

struct prof_adapt_core_notifier {
    void (*poll_report)(uint32_t dev_id, uint32_t chan_id);
    drvError_t (*chan_report)(uint32_t dev_id, uint32_t chan_id, void *data, uint32_t data_len, bool hal_flag);
};
void prof_adapt_register_notifier(struct prof_adapt_core_notifier *notifier);
struct prof_adapt_core_notifier *prof_adapt_get_notifier(void);

drvError_t prof_adapt_check_cc_mode(unsigned int dev_id);

bool prof_is_v4_extra_channel(unsigned int chan_id);

bool prof_is_support_host_sample(void);

#endif
