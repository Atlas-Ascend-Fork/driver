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

#include "svm_kern_log.h"
#include "pma_ub_ctx.h"
#include "pma_ub_ubdevshm_wrapper.h"
#include "pma_ub_um_handle.h"
#include "pma_ub_init.h"

int pma_ub_init(void)
{
    int ret;

    ret = pma_ub_ctx_init();
    if (ret != 0) {
        svm_err("Ctx init failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = pma_ub_ubdevshm_wrapper_init();
    if (ret != 0) {
        svm_err("Ubdevshm wrapper init failed. (ret=%d)\n", ret);
        pma_ub_ctx_uninit();
        return ret;
    }

    pma_ub_um_handle_init();

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(pma_ub_init, FEATURE_LOADER_STAGE_6);

void pma_ub_uninit(void)
{
    pma_ub_ubdevshm_wrapper_uninit();
    pma_ub_ctx_uninit();
}
DECLAER_FEATURE_AUTO_UNINIT(pma_ub_uninit, FEATURE_LOADER_STAGE_6);