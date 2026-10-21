/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdint.h>

#include "ubmm_dev_inner.h"
#include "pbl_ubmm.h"
#include "pbl_urd_user.h"
#include "pbl_urd_main_cmd_def.h"
#include "pbl_urd_sub_cmd_common.h"

#define UBMEM_FAULT_EVENT_ID 0x81AFAA06
#define UBMEM_FAULT_WAIT_MAX_TIMES 30000
#define UBMEM_FAULT_WAIT_TIME 1000 // 1 ms

drvError_t ubMemWaitFaultRepair(uint32_t devid)
{
    struct halEventFilter filter = {0};
    drvError_t ret;
    int i;

    filter.filter_flag = 0x1;
    filter.event_id = UBMEM_FAULT_EVENT_ID;
    filter.node_type = 0;
    filter.severity = 0;

    for (i = 0; i < UBMEM_FAULT_WAIT_MAX_TIMES; i++) {
        struct halFaultEventInfo info = {0};
        uint32_t count = 0;
        ret = halGetFaultEvent(devid, &filter, &info, 1, &count);
        if ((ret == 0) && (count == 1) && (info.event_id == UBMEM_FAULT_EVENT_ID)) {
            usleep(UBMEM_FAULT_WAIT_TIME);
            continue;
        }
        break;
    }

    return (i == UBMEM_FAULT_WAIT_MAX_TIMES) ? DRV_ERROR_WAIT_TIMEOUT : ret;
}

drvError_t ubmemRepairFault(uint32_t devid, halRepairFaultInfo *info)
{
    struct ubmem_dev_repair_msg_in in = {0};
    struct ubmem_dev_repair_msg_out out = {0};
    struct urd_cmd_para cmd_para = {0};
    struct urd_cmd cmd = {0};
    int ret;

    if (info == NULL) {
        ubmem_err("Info is null. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    in.udevid = devid;

    urd_usr_cmd_fill(&cmd, URD_UBMEM_REPAIR_FAULT_CMD, ZERO_CMD, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&in, (uint32_t)sizeof(in), (void *)&out, (uint32_t)sizeof(out));

    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret != 0) {
        ubmem_err_if(ret != DRV_ERROR_NOT_SUPPORT, "ubmem repair fault failed. (devid=%u; ret=%d)\n", devid, ret);
        return (drvError_t)ret;
    }

    if (out.result != UBMEM_DEV_REPAIR_SUCCESS) {
        return DRV_ERROR_NONE;
    }

    return ubMemWaitFaultRepair(devid);
}

drvError_t ubmemMapRouteCheck(uint32_t dst_devid, uint32_t src_server_id, uint32_t src_mem_id, uint32_t map_route)
{
    struct ubmem_map_route_check_msg_in in = {0};
    struct ubmem_map_route_check_msg_out out = {0};
    struct urd_cmd_para cmd_para = {0};
    struct urd_cmd cmd = {0};
    int ret;

    in.dst_devid = dst_devid;
    in.src_server_id = src_server_id;
    in.src_mem_id = src_mem_id;
    in.map_route = map_route;

    urd_usr_cmd_fill(&cmd, URD_UBMEM_MAP_ROUTE_CHECK_CMD, ZERO_CMD, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&in, (uint32_t)sizeof(in), (void *)&out, (uint32_t)sizeof(out));

    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret != 0) {
        ubmem_err("ubmem map route check failed. (dst_devid=%u; src_server_id=%u; src_mem_id=%u; route=%u; ret=%d)\n",
                  dst_devid, src_server_id, src_mem_id, map_route, ret);
        return ret;
    }

    return (out.is_exist == 1) ? DRV_ERROR_NONE : DRV_ERROR_NOT_EXIST;
}
