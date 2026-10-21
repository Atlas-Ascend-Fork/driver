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
#include "comm_kernel_interface.h"

#include "trs_pm_adapt.h"
#include "trs_chan_mbox.h"
#include "trs_chan_mem.h"
#include "trs_host_comm.h"
#include "soc_adapt.h"
#include "trs_chan_near_ops_rsv_mem.h"
#include "trs_chan_near_ops_mem.h"
#include "trs_chan_near_ops_mbox.h"

static int trs_chan_update_dev_mem_addr(struct trs_id_inst *inst, struct trs_chan_info *chan_info)
{
    u32 sq_mem_side = trs_chan_get_sqcq_mem_side(chan_info->sq_info.mem_type);
    u32 cq_mem_side = trs_soc_get_cq_mem_side(inst);
    int ret;

    /* when set TSDRV_FLAG_ONLY_SQCQ_ID flag, no sq mem, but sq_mem_side is 0 */
    if (((chan_info->sq_info.sq_vaddr != NULL) || (chan_info->sq_info.vmap_pfn_handle != NULL)) &&
        (trs_chan_mem_is_dev_mem(sq_mem_side) && !(chan_info->sq_addr_is_virtual))) {
        ret = trs_chan_near_sqcq_mem_h2d(inst, chan_info->sq_info.sq_phy_addr, &chan_info->sq_info.sq_phy_addr,
                                         sq_mem_side);
        if (ret != 0) {
            trs_err("Reserve sq mem h2d failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
            return ret;
        }
    }

    if (trs_chan_mem_is_dev_mem(cq_mem_side)) {
        ret = trs_chan_near_sqcq_mem_h2d(inst, chan_info->cq_info.cq_phy_addr, &chan_info->cq_info.cq_phy_addr,
                                         cq_mem_side);
        if (ret != 0) {
            trs_err("Reserve cq mem h2d failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
            return ret;
        }
    }

    return 0;
}

int trs_chan_near_ops_mbox_send(struct trs_id_inst *inst, struct trs_chan_info *chan_info)
{
    struct trs_chan_adapt_info adapt;
    int ret;

    adapt.app_type = TRS_MBOX_SEND_FROM_HOST;
    adapt.sq_side = (trs_chan_mem_is_dev_mem(chan_info->sq_info.mem_type)) ? 0 : 1;
    adapt.cq_side = (trs_chan_mem_is_dev_mem(trs_soc_get_cq_mem_side(inst))) ? 0 : 1;
    adapt.fid = 0;

    ret = trs_chan_update_dev_mem_addr(inst, chan_info);
    if (ret != 0) {
        return ret;
    }

    return trs_chan_mbox_send(inst, chan_info, &adapt);
}
