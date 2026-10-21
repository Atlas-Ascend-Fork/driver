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

#ifndef VIRTMNG_RES_CLOUD_V4_H
#define VIRTMNG_RES_CLOUD_V4_H

#include "virtmnghost_ctrl.h"
#include "virtmnghost_feature.h"

int vmngh_res_init_cloud_v4(u32 devid, struct vmngh_ctrl_ops *ops);
vmng_feature_bitmap_t vmngh_feature_init_cloud_v4(void);

#endif
