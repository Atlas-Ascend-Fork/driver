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

#ifndef PBL_URD_SUB_CMD_DEF_H
#define PBL_URD_SUB_CMD_DEF_H

#include "pbl/pbl_urd_sub_cmd_common.h"

struct urd_vender_id_info {
    unsigned int venderid;
    unsigned int subvenderid;
    unsigned int deviceid;
    unsigned int subdeviceid;
    unsigned int bus;
    unsigned int device;
    unsigned int fn;
    unsigned int domain;
};

#endif
