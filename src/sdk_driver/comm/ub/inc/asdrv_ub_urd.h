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
#ifndef ASDRV_UB_URD_H
#define ASDRV_UB_URD_H

#include "urd_define.h"
#include "urd_feature.h"
#include "urd_acc_ctrl.h"
#include "ubcore_types.h"

#define ASCEND_UB_CMD_NAME "ASCEND_UB"

struct ascend_ub_urma_info {
    unsigned int dev_id;
    char urma_name[UBCORE_MAX_DEV_NAME];
};

enum ascend_ub_d2d_eid_type {
    UBDRV_D2D_EID_IN_BOARD = 0,
    UBDRV_D2D_EID_CROSS_BOARD,
    UBDRV_D2D_EID_TYPE_MAX
};

int ascend_ub_init_urd(void);
void ascend_ub_uninit_urd(void);
int ascend_ub_get_urma_name(void *feature, char *in, u32 in_len, char *out, u32 out_len);
int ascend_ub_get_eid_index(void *feature, char *in, u32 in_len, char *out, u32 out_len);

#endif // ASDRV_UB_URD_H