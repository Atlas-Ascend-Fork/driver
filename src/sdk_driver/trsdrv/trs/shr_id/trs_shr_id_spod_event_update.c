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

#include "ascend_hal_define.h"
#include "pbl/pbl_spod_info.h"

#include "esched_kernel_interface.h"

#include "trs_pub_def.h"
#include "trs_event.h"
#include "trs_res_id_def.h"
#include "trs_shr_id_spod.h"
#include "trs_shr_id_event_update.h"
#include "trs_shr_id_auto_init.h"
#include "trs_shr_id_spod_event_update.h"

/* for shrid pa addr manage */
#define SHRID_ASCEND910_93_CROSS_NODE_BASE_ADDR 0x300000000000ULL // 48T
#define SHRID_ASCEND910_93_SERVER_ADDR_OFFSET 0x10000000000ULL    // 1T
#define SHRID_ASCEND910_93_CHIP_ADDR_OFFSET 0x2000000000ULL       // 128G
#define SHRID_ASCEND910_93_DIE_ADDR_OFFSET 0x1000000000ULL        // 64G
#define SHRID_ASCEND910_93_NOTIFY_ADDR_OFFSET_PER_DIE 0x1100000ULL

static int shr_id_get_notify_base_addr(u32 devid, u32 sdid, u64 *base_addr)
{
    struct sdid_parse_info parse_info;
    int ret;

    ret = dbl_parse_sdid(sdid, &parse_info);
    if (ret != 0) {
        trs_err("Parse sdid failed. (devid=%u; sdid=%u)\n", devid, sdid);
        return ret;
    }

    DBL_SPOD_CHECK_SDID(parse_info, ret);
    if (ret != 0) {
        trs_err("Invalid sdid. (server_id=%u; chip_id=%u; die_id=%u; udevid=%u; sdid=%u)\n", parse_info.server_id,
                parse_info.chip_id, parse_info.die_id, devid, sdid);
        return ret;
    }

    *base_addr = SHRID_ASCEND910_93_CROSS_NODE_BASE_ADDR +
                 parse_info.server_id * SHRID_ASCEND910_93_SERVER_ADDR_OFFSET +
                 parse_info.chip_id * SHRID_ASCEND910_93_CHIP_ADDR_OFFSET +
                 parse_info.die_id * SHRID_ASCEND910_93_DIE_ADDR_OFFSET + SHRID_ASCEND910_93_NOTIFY_ADDR_OFFSET_PER_DIE;

    trs_debug("Sdid info. (devid=%u; sdid=0x%x; server_id=%u; chip_id=%u; die_id=%u)\n", devid, sdid,
              parse_info.server_id, parse_info.chip_id, parse_info.die_id);
    return 0;
}

STATIC int _shr_id_spod_event_update(unsigned int devid, struct sched_published_event_info *event_info,
                                     struct sched_published_event_func *event_func)
{
    struct drvShrIdInfo *info = NULL;
    u64 base_addr, notify_addr;
    u32 sdid, tsid;
    int res_type;
    int ret;

    if (event_info->subevent_id != DRV_SUBEVENT_TRS_SHR_ID_CONFIG_MSG) {
        return 0;
    }

    if ((event_info->msg == NULL) ||
        (event_info->msg_len < sizeof(struct event_sync_msg) + sizeof(struct drvShrIdInfo))) {
        trs_err("Invalid msg. (msg_is_null=%d; msg_len=%u)\n", (int)(event_info->msg == NULL), event_info->msg_len);
        return -EINVAL;
    }

    info = (struct drvShrIdInfo *)(event_info->msg + sizeof(struct event_sync_msg));
    sdid = info->devid;
    tsid = info->tsid;

    if ((info->flag & TSDRV_FLAG_SHR_ID_SHADOW) == 0) {
        trs_debug("Goto event update. (flag=0x%x).\n", info->flag);
        return 0; /* Not spod, goto shr_id_event_update. */
    }

    res_type = shr_id_type_trans_res_type(info->id_type);
    if (res_type >= TRS_MAX_ID_TYPE) {
        trs_err("Type is invalid. (type=%u)\n", info->id_type);
        return -EINVAL;
    }

    if (!hal_kernel_trs_is_belong_to_pod_proc(sdid, tsid, ka_task_get_current_tgid(), res_type, info->shrid)) {
        trs_err("Id invalid. (devid=%u; sdid=%u; tsid=%u; pid=%d; type=%u; shrid=%u)\n", devid, sdid, tsid,
                ka_task_get_current_tgid(), info->id_type, info->shrid);
        return -EACCES;
    }

    ret = shr_id_get_notify_base_addr(devid, info->devid, &base_addr);
    if (ret != 0) {
        return ret;
    }

    notify_addr = base_addr + shr_id_get_notify_offset(info->shrid);
    info->rsv[0] = (u32)(notify_addr & 0xffffffffULL); /* 0xffffffffULL is low 32 bit */
    info->rsv[1] = (u32)(notify_addr >> 32);           /* 32 is high bit */

    trs_debug("Notify info. (devid=%u; remote_devid=%u; type=%u; shrid=%u; sub_id=%u; flag=0x%x)\n", devid, info->devid,
              info->id_type, info->shrid, event_info->subevent_id, info->flag);

    return 0;
}

int shr_id_spod_event_update(unsigned int devid, struct sched_published_event_info *event_info,
                             struct sched_published_event_func *event_func)
{
    int ret = _shr_id_spod_event_update(devid, event_info, event_func);
    return trs_event_kerror_to_uerror(ret);
}

STATIC int shr_id_spod_event_init(void)
{
    return hal_kernel_sched_register_event_pre_proc_handle(EVENT_DRV_MSG, SCHED_PRE_PROC_POS_LOCAL,
                                                           shr_id_spod_event_update);
}
DECLAER_FEATURE_AUTO_INIT(shr_id_spod_event_init, FEATURE_LOADER_STAGE_5);

STATIC void shr_id_spod_event_uninit(void)
{
#ifndef EMU_ST
    hal_kernel_sched_unregister_event_pre_proc_handle(EVENT_DRV_MSG, SCHED_PRE_PROC_POS_LOCAL,
                                                      shr_id_spod_event_update);
#endif
}
DECLAER_FEATURE_AUTO_UNINIT(shr_id_spod_event_uninit, FEATURE_LOADER_STAGE_5);
