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

#include "comm_kernel_interface.h"
#include "apm_msg.h"
#include "apm_host_msg.h"
#include "apm_dev.h"

static struct devdrv_common_msg_client apm_host_msg_client = {
    .type = DEVDRV_COMMON_MSG_SMMU,
    .common_msg_recv = apm_msg_recv,
};

int apm_msg_send(u32 udevid, struct apm_msg_header *header, u32 size)
{
    u32 real_out_len;
    int ret;

    if (apm_dev_is_host_reset(udevid)) {
        apm_warn("Dev is in host reset status. (udevid=%u)\n", udevid);
        return 0;
    }

    ret = devdrv_common_msg_send(udevid, (void *)header, size, size, &real_out_len, DEVDRV_COMMON_MSG_SMMU);
    if (ret != 0) {
        apm_err_if((header->msg_type != APM_MSG_TYPE_MASTER_DESTROY),
                   "Msg send fail. (ret=%d; udevid=%u; msg_type=%u)\n", ret, udevid, header->msg_type);
        return ret;
    }

    if (header->result != 0) {
        apm_warn("Msg process result warn. (result=%d; udevid=%u; msg_type=%u)\n", header->result, udevid,
                 header->msg_type);
        return header->result;
    }

    return 0;
}

int apm_host_msg_init(void)
{
    int ret = devdrv_register_common_msg_client(&apm_host_msg_client);
    if (ret != 0) {
        apm_err("Msg client register fail. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(apm_host_msg_init, FEATURE_LOADER_STAGE_2);

void apm_host_msg_uninit(void)
{
    (void)devdrv_unregister_common_msg_client(0, &apm_host_msg_client);
}
DECLAER_FEATURE_AUTO_UNINIT(apm_host_msg_uninit, FEATURE_LOADER_STAGE_2);
