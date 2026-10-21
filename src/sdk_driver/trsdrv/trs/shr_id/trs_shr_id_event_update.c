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

#include "esched_kernel_interface.h"
#include "dms/dms_interface.h"
#include "dms/dms_devdrv_manager_comm.h"
#ifndef EMU_ST
#include "devdrv_common.h"
#include "pbl/pbl_board_config.h"
#include "pbl/pbl_chip_config.h"
#include "pbl/pbl_uda.h"
#endif

#include "trs_pub_def.h"
#include "trs_event.h"
#include "trs_res_id_def.h"
#include "trs_shr_id.h"
#include "trs_shr_id_event_update.h"
#include "trs_shr_id_auto_init.h"

/* for shrid pa addr manage */
#define SHRID_STARS_BASE_ADDR 0x06a0000000ULL
#define SHRID_ASCEND910_93_CHIP_ADDR_OFFSET 0x80000000000ULL
#define SHRID_ASCEND910_93_DIE_ADDR_OFFSET 0x10000000000ULL
#define SHRID_ASCEND910_93_HCCS_CHIP_ADDR_OFFSET 0x20000000000ULL /* 2T */
#define SHRID_CHIP_BASE_ADDR 0x200000000000ULL                    /* 32T */

#define SHRID_STARS_PCIE_BASE_ADDR 0x400004008000ULL /* PCIE BAR */
#define SHRID_PCIE_LOCAL_DEV_OFFSET 36ULL
#define SHRID_PCIE_REMOTE_DEV_OFFSET 32ULL

#define SHRID_STARS_NOTIFY_BASE_ADDR 0x100000ULL

static int shr_id_get_notify_base_addr(u32 devid, u32 remote_phy_id, u64 *base_addr)
{
#ifndef EMU_ST
    int topology_type, ret;
    u32 local_phy_id = devid;
    unsigned long long phy_base_addr = 0;
    unsigned long long chip_offset = SHRID_ASCEND910_93_CHIP_ADDR_OFFSET;
    u32 chip_type = HISI_CHIP_UNKNOWN;

    /* others chip no need */
    chip_type = uda_get_chip_type(devid);
    if (!((chip_type == HISI_CLOUD_V2) || (chip_type == HISI_MINI_V3) || (chip_type == HISI_CHIP_UNKNOWN))) {
        *base_addr = 0;
        return 0;
    }

    ret = dms_get_dev_topology(local_phy_id, remote_phy_id, &topology_type);
    if (ret != 0) {
        trs_err("Get topology_type failed. (devid=%u; remote_devid=%u; ret=%d)\n", devid, remote_phy_id, ret);
        return ret;
    }

    if ((topology_type == TOPOLOGY_HCCS) || (topology_type == TOPOLOGY_SIO) || (topology_type == TOPOLOGY_HCCS_SW)) {
        struct dbl_dev_base_info *info;
        info = dbl_get_dev_base_info(remote_phy_id);
        if (info == NULL) {
            trs_err("Get chipid failed. (remote_phy_id=%u)\n", remote_phy_id);
            return -EINVAL;
        }
        trs_debug("Notify info. (devid=%u; remote_devid=%u; chip=%u; die=%u; type=%d; addrmode=%u)\n", devid,
                  remote_phy_id, info->chip_id, info->die_id, topology_type, info->addr_mode);

        if (info->addr_mode == ADDR_MODE_UNIFIED) {
            phy_base_addr = SHRID_CHIP_BASE_ADDR;
            chip_offset = SHRID_ASCEND910_93_HCCS_CHIP_ADDR_OFFSET;
        }
        *base_addr = SHRID_STARS_BASE_ADDR + SHRID_STARS_NOTIFY_BASE_ADDR + chip_offset * info->chip_id +
                     SHRID_ASCEND910_93_DIE_ADDR_OFFSET * info->die_id + phy_base_addr;
    } else if ((topology_type == TOPOLOGY_PIX) || (topology_type == TOPOLOGY_PIB) || (topology_type == TOPOLOGY_PHB) ||
               (topology_type == TOPOLOGY_SYS)) {
        *base_addr = SHRID_STARS_PCIE_BASE_ADDR + SHRID_STARS_NOTIFY_BASE_ADDR +
                     ((u64)remote_phy_id << SHRID_PCIE_REMOTE_DEV_OFFSET) +
                     ((u64)local_phy_id << SHRID_PCIE_LOCAL_DEV_OFFSET);
        trs_debug("Notify info. (devid=%u; remote_devid=%u; type=%d)\n", devid, remote_phy_id, topology_type);
    } else {
        trs_warn("Not support. (devid=%u; rudevid=%u; topology_type=%u)\n", devid, remote_phy_id, topology_type);
        return -EINVAL;
    }
#endif
    return 0;
}

STATIC int _shr_id_event_update(unsigned int devid, struct sched_published_event_info *event_info,
                                struct sched_published_event_func *event_func)
{
    struct drvShrIdInfo *info = NULL;
    u64 base_addr, notify_addr;
    struct trs_id_inst inst;
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
    inst.devid = info->devid;
    inst.tsid = info->tsid;

    if ((info->flag & TSDRV_FLAG_SHR_ID_SHADOW) != 0) {
        trs_debug("Goto spod event update. (flag=0x%x).\n", info->flag);
        return 0; /* Spod, goto shr_id_spod_event_update. */
    }

    if (info->id_type >= SHR_ID_TYPE_MAX) {
        trs_err("Type is invalid. (type=%u)\n", info->id_type);
        return -EINVAL;
    }

    res_type = shr_id_type_trans_res_type(info->id_type);
    if (res_type >= TRS_MAX_ID_TYPE) {
        trs_err("Type is invalid. (type=%u)\n", info->id_type);
        return -EINVAL;
    }

    if (!shr_id_is_belong_to_proc(&inst, ka_task_get_current_tgid(), res_type, info->shrid)) {
        trs_err("Id invalid. (devid=%u; rudevid=%u; tsid=%u; pid=%d; type=%u; shrid=%u)\n", devid, info->devid,
                info->tsid, ka_task_get_current_tgid(), info->id_type, info->shrid);
        return -EACCES;
    }

    ret = shr_id_get_notify_base_addr(devid, info->devid, &base_addr);
    if (ret != 0) {
        return ret;
    }

    notify_addr = base_addr + shr_id_get_notify_offset(info->shrid);
    info->rsv[0] = (u32)(notify_addr & 0xffffffffULL); /* 0xffffffffULL is low 32 bit */
    info->rsv[1] = (u32)(notify_addr >> 32);           /* 32 is high bit */

    trs_debug("Notify info. (devid=%u; remote_devid=%u; type=%u; shrid=%u; sub_id=%u)\n", devid, info->devid,
              info->id_type, info->shrid, event_info->subevent_id);

    return 0;
}

int shr_id_event_update(unsigned int devid, struct sched_published_event_info *event_info,
                        struct sched_published_event_func *event_func)
{
    int ret = _shr_id_event_update(devid, event_info, event_func);
    return trs_event_kerror_to_uerror(ret);
}

STATIC int shr_id_event_init(void)
{
    return hal_kernel_sched_register_event_pre_proc_handle(EVENT_DRV_MSG, SCHED_PRE_PROC_POS_LOCAL,
                                                           shr_id_event_update);
}
DECLAER_FEATURE_AUTO_INIT(shr_id_event_init, FEATURE_LOADER_STAGE_5);

STATIC void shr_id_event_uninit(void)
{
#ifndef EMU_ST
    hal_kernel_sched_unregister_event_pre_proc_handle(EVENT_DRV_MSG, SCHED_PRE_PROC_POS_LOCAL, shr_id_event_update);
#endif
}
DECLAER_FEATURE_AUTO_UNINIT(shr_id_event_uninit, FEATURE_LOADER_STAGE_5);
