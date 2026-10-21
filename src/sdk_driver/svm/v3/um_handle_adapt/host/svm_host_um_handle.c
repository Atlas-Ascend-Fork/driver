/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "pbl_feature_loader.h"

#include "assign_um_handle.h"
#include "op_um_handle.h"
#include "svm_host_um_handle.h"

int svm_host_um_handle_init(void)
{
    assign_um_handle_init();
    op_um_handle_init();

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_host_um_handle_init, FEATURE_LOADER_STAGE_4);
