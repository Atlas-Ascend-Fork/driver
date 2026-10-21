/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <string.h>
#include "securec.h"
#include "ds_data.h"
#include "dsmi_common_interface.h"
#include "udis_user.h"
#include "hccn_udis_comm.h"
#include "tool_comm.h"
#include "tool_udis.h"

STATIC int udis_get_info_init(struct command_context *ctx, struct udis_dev_info *get_info, const char *name,
                              enum query_mode type)
{
    size_t name_len = 0;
    int ret = 0;

    get_info->module_type = UDIS_MODULE_NETWORK;

    if (type == QUERY_PORT_TYPE) {
        name_len = strlen(name) + HCCN_NAME_PORT_TYPE_LEN;
        CHK_PRT_RETURN(name_len >= HCCN_MAX_NAME_LEN,
                       hccn_warn("udis_info->name will be out of range:%d, name:%s, name_len:%zu", HCCN_MAX_NAME_LEN,
                                 name, name_len),
                       ret);

        ret = snprintf_s(get_info->name, HCCN_MAX_NAME_LEN, name_len, "%s_%d_%d", name, ctx->udie_id, ctx->port_id);
    } else if (type == QUERY_NPU_TYPE) {
        name_len = strlen(name);
        CHK_PRT_RETURN(name_len >= HCCN_MAX_NAME_LEN,
                       hccn_warn("udis_info->name will be out of range:%d, name:%s, name_len:%zu", HCCN_MAX_NAME_LEN,
                                 name, name_len),
                       ret);

        ret = snprintf_s(get_info->name, HCCN_MAX_NAME_LEN, name_len, "%s", name);
    } else {
        hccn_warn("query mode:%d is not supported", type);
        return -EINVAL;
    }

    CHK_PRT_RETURN(ret <= 0, hccn_warn("snprintf_s unsuccessful, ret:%d, name_len:%zu", ret, name_len), -EIO);

    return 0;
}

int tool_udis_get_net_health_exec(struct command_context *ctx, DSMI_NET_HEALTH_STATUS *presult)
{
    struct udis_dev_info get_info = {0};
    int ret;

    ret = udis_get_info_init(ctx, &get_info, HCCN_UDIS_NAME_NET_HEALTH, QUERY_NPU_TYPE);
    CHK_PRT_RETURN(ret != 0, hccn_warn("udis_get_info_init unsuccessful, ret:%d", ret), ret);

    ret = udis_get_device_info((unsigned int)ctx->npu_info.logic_id, &get_info);
    CHK_PRT_RETURN(
        ret != 0, hccn_warn("udis_get_device_info unsuccessful, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    CHK_PRT_RETURN(get_info.data_len != sizeof(int),
                   hccn_warn("data_len:%d is invalid logic_id:%d", get_info.data_len, ctx->npu_info.logic_id), -EINVAL);

    *presult = *(DSMI_NET_HEALTH_STATUS *)get_info.data;
    return 0;
}
