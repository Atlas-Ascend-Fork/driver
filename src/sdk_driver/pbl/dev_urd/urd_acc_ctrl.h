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

#ifndef URD_ACC_CTRL_H
#define URD_ACC_CTRL_H
#include "pbl/pbl_urd.h"

#define DMS_ACCESS_PROC_STRING_MAX 4096U

s32 dms_feature_access_identify(u32 feature_prof, u32 msg_source);

s32 dms_feature_whitelist_check(const char **proc_ctrl, u32 proc_num);
int dms_feature_parse_proc_ctrl(const char *proc_str, char **buf, char ***proc_ctl, u32 *proc_num);
#define DMI_VIRTUAL_SYSVENDOR "QEMU"
#define DMI_VIRTUAL_PRODUCT_NAME "KVM Virtual Machine"

#endif
