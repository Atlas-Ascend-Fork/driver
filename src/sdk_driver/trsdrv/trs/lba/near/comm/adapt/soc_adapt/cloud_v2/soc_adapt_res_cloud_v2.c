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
#include "pbl/pbl_uda.h"
#include "trs_pub_def.h"
#include "trs_chan_mem.h"
#include "trs_chip_def_comm.h"
#include "trs_chan.h"
#include "trs_core.h"
#include "trs_core_stars_v1_ops.h"
#include "trs_chan_stars_v1_ops.h"
#include "soc_adapt_res_cloud_v2.h"

/* Trs notify */
#define TRS_CLOUD_V2_NOTIFY_SIZE 4
#define TRS_CLOUD_V2_NOTIFY_SLICE_SIZE (64 * 1024)
#define TRS_CLOUD_V2_NOTIFY_NUM_PER_SLICE 512       /* total silce num is 16 */
#define TRS_CLOUD_V2_NOTIFY_AVG_SIZE_PER_NOTIFY 128 /* total slice num * per slice size / total notify num */

/* Event */
#define TRS_CLOUD_V2_EVENT_SIZE 4
#define TRS_CLOUD_V2_EVENT_SLICE_SIZE (64 * 1024)
#define TRS_CLOUD_V2_EVENT_NUM_PER_SLICE 4096 /* total silce num is 16 */

/* Doorbell */
#define TRS_SOC_CLOUD_V2_DB_STRIDE (4 * 1024)
#define TRS_CLOUD_V2_STARS_SCHED_STRIDE (4 * 1024)

#define TRS_DB_CLOUD_V2_ONLINE_MBOX_START 0u
#define TRS_DB_CLOUD_V2_ONLINE_MBOX_END 1u

#define TRS_DB_CLOUD_V2_TRIGGER_SQ_START 1u
#define TRS_DB_CLOUD_V2_TRIGGER_SQ_END 2u

#define TRS_DB_CLOUD_V2_MAINT_SQ_START 992u
#define TRS_DB_CLOUD_V2_MAINT_SQ_END 996u

#define TRS_DB_CLOUD_V2_MAINT_CQ_START 996u
#define TRS_DB_CLOUD_V2_MAINT_CQ_END 1006u

u32 trs_soc_get_cloud_v2_notify_offset(u32 id)
{
    return (id % TRS_CLOUD_V2_NOTIFY_NUM_PER_SLICE) * TRS_CLOUD_V2_NOTIFY_SIZE +
           (id / TRS_CLOUD_V2_NOTIFY_NUM_PER_SLICE) * TRS_CLOUD_V2_NOTIFY_SLICE_SIZE;
}

size_t trs_soc_get_cloud_v2_notify_size(void)
{
    return (size_t)TRS_CLOUD_V2_NOTIFY_AVG_SIZE_PER_NOTIFY;
}

u32 trs_soc_get_cloud_v2_event_offset(u32 event_id)
{
    return (event_id % TRS_CLOUD_V2_EVENT_NUM_PER_SLICE) * TRS_CLOUD_V2_EVENT_SIZE +
           (event_id / TRS_CLOUD_V2_EVENT_NUM_PER_SLICE) * TRS_CLOUD_V2_EVENT_SLICE_SIZE;
}

size_t trs_soc_get_cloud_v2_db_stride(void)
{
    return (size_t)TRS_SOC_CLOUD_V2_DB_STRIDE;
}

size_t trs_soc_get_cloud_v2_stars_sched_stride(void)
{
    return (u32)TRS_CLOUD_V2_STARS_SCHED_STRIDE;
}

int trs_soc_get_cloud_v2_db_cfg(int db_type, u32 *start, u32 *end)
{
    switch (db_type) {
        case TRS_DB_ONLINE_MBOX:
            *start = TRS_DB_CLOUD_V2_ONLINE_MBOX_START;
            *end = TRS_DB_CLOUD_V2_ONLINE_MBOX_END;
            break;
        case TRS_DB_TRIGGER_SQ:
            *start = TRS_DB_CLOUD_V2_TRIGGER_SQ_START;
            *end = TRS_DB_CLOUD_V2_TRIGGER_SQ_END;
            break;
        case TRS_DB_MAINT_SQ:
            *start = TRS_DB_CLOUD_V2_MAINT_SQ_START;
            *end = TRS_DB_CLOUD_V2_MAINT_SQ_END;
            break;
        case TRS_DB_MAINT_CQ:
            *start = TRS_DB_CLOUD_V2_MAINT_CQ_START;
            *end = TRS_DB_CLOUD_V2_MAINT_CQ_END;
            break;
        default:
            trs_err("Unknown db_type. (db_type=%d)\n", db_type);
            return -ENODEV;
    }

    return 0;
}

static u32 trs_get_cloud_v2_sq_mem_side_by_topology(u32 devid)
{
    if (devdrv_get_connect_protocol(devid) != CONNECT_PROTOCOL_HCCS) {
        return TRS_CHAN_DEV_MEM_PRI;
    } else {
        u32 host_flag;
        if (devdrv_get_host_phy_mach_flag(devid, &host_flag) != 0) {
            trs_warn("Get host flag not support. (devid=%u)\n", devid);
            return TRS_CHAN_DEV_RSV_MEM;
        }
        trs_debug("Get host phy flag. (devid=%d; host_flag=%d)\n", devid, host_flag);
        return (host_flag == DEVDRV_HOST_PHY_MACH_FLAG) ? TRS_CHAN_HOST_MEM : TRS_CHAN_DEV_RSV_MEM;
    }
}

u32 trs_soc_get_cloud_v2_sq_mem_side(u32 devid, struct trs_chan_type *types)
{
    switch (types->type) {
        case CHAN_TYPE_MAINT:
            return TRS_CHAN_HOST_MEM;
        default:
            return trs_get_cloud_v2_sq_mem_side_by_topology(devid);
    }
}

u32 trs_soc_get_cloud_v2_cq_mem_side(u32 devid)
{
#ifndef EMU_ST
    if ((devdrv_get_connect_protocol(devid) == CONNECT_PROTOCOL_HCCS) && (devdrv_is_mdev_vm_boot_mode(devid) == true)) {
        return TRS_CHAN_HOST_PHY_MEM;
    }
#endif

    return TRS_CHAN_HOST_MEM;
}

int trs_soc_cloud_v2_chan_stars_init(struct trs_id_inst *inst)
{
    int ret;

    ret = trs_chan_stars_v1_ops_init(inst);
    if (ret != 0) {
        return ret;
    }

    ret = trs_chan_ts_inst_register(inst, TRS_HW_TYPE_STARS, UDA_NEAR, trs_chan_get_stars_v1_adapt_ops());
    if (ret != 0) {
        trs_chan_stars_v1_ops_uninit(inst);
    }
    return ret;
}

void trs_soc_cloud_v2_chan_stars_uninit(struct trs_id_inst *inst)
{
    trs_chan_ts_inst_unregister(inst);
    trs_chan_stars_v1_ops_uninit(inst);
}

int trs_soc_cloud_v2_chan_stars_ops_init(struct trs_id_inst *inst)
{
    return trs_chan_stars_v1_ops_init(inst);
}

void trs_soc_cloud_v2_chan_stars_ops_uninit(struct trs_id_inst *inst)
{
    trs_chan_stars_v1_ops_uninit(inst);
}

struct trs_chan_adapt_ops *trs_chan_cloud_v2_get_stars_adapt_ops(void)
{
    return trs_chan_get_stars_v1_adapt_ops();
}

struct trs_core_adapt_ops *trs_core_cloud_v2_get_stars_adapt_ops(void)
{
    return trs_core_get_stars_v1_adapt_ops();
}

int trs_soc_cloud_v2_sq_send_trigger_db_init(struct trs_id_inst *inst)
{
    return trs_sq_send_trigger_db_init(inst);
}

void trs_soc_cloud_v2_sq_send_trigger_db_uninit(struct trs_id_inst *inst)
{
    trs_sq_send_trigger_db_uninit(inst);
}
