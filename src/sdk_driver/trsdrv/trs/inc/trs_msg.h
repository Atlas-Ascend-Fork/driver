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
#ifndef TRS_MSG_H
#define TRS_MSG_H

#include "ka_type.h"
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
#include "uba/ubcore_adapt.h"
#endif
#include "trs_chan.h"
#include "trs_pm_adapt.h"
#include "trs_id.h"
#include "trs_mailbox_def.h"
#include "trs_h2d_msg.h"

typedef int (*trs_msg_send_func_t)(u32, struct trs_msg_data *);
typedef int (*trs_msg_rdv_func_t)(u32, struct trs_msg_data *);

#endif /* TRS_MSG_H */
