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
#ifndef SOC_ADAPT_RES_CLOUD_V4_H__
#define SOC_ADAPT_RES_CLOUD_V4_H__

#include "ka_type.h"

#include "trs_pm_adapt.h"
#include "trs_pub_def.h"
#include "trs_rsv_mem.h"
#include "trs_stars_v2_reg_def.h"

#include "comm_kernel_interface.h"

int trs_soc_cloud_v4_chan_stars_init(struct trs_id_inst *inst);
void trs_soc_cloud_v4_chan_stars_uninit(struct trs_id_inst *inst);
int trs_soc_cloud_v4_chan_stars_ops_init(struct trs_id_inst *inst);
void trs_soc_cloud_v4_chan_stars_ops_uninit(struct trs_id_inst *inst);
struct trs_chan_adapt_ops *trs_chan_cloud_v4_get_stars_adapt_ops(void);
struct trs_core_adapt_ops *trs_core_cloud_v4_get_stars_adapt_ops(void);
int trs_soc_cloud_v4_sq_send_trigger_db_init(struct trs_id_inst *inst);
void trs_soc_cloud_v4_sq_send_trigger_db_uninit(struct trs_id_inst *inst);

/* Notify */
#define TRS_CLOUD_V4_NOTIFY_SIZE 4
#define TRS_CLOUD_V4_NOTIFY_INTERVAL 8
#define TRS_CLOUD_V4_NOTIFY_SEPARATE_CNT 16
#define TRS_CLOUD_V4_NOTIFY_SEPARATE_SIZE 0x1000
#define TRS_CLOUD_V4_NOTIFY_SLICE_SIZE 0x200000
#define TRS_CLOUD_V4_NOTIFY_NUM_PER_SLICE 4096 /* total slice num is 16 */

static inline u32 trs_soc_get_cloud_v4_notify_offset(u32 id)
{
    return (id % TRS_CLOUD_V4_NOTIFY_NUM_PER_SLICE) % TRS_CLOUD_V4_NOTIFY_SEPARATE_CNT * TRS_CLOUD_V4_NOTIFY_INTERVAL +
           (id % TRS_CLOUD_V4_NOTIFY_NUM_PER_SLICE) / TRS_CLOUD_V4_NOTIFY_SEPARATE_CNT *
               TRS_CLOUD_V4_NOTIFY_SEPARATE_SIZE +
           (id / TRS_CLOUD_V4_NOTIFY_NUM_PER_SLICE) * TRS_CLOUD_V4_NOTIFY_SLICE_SIZE;
}

static inline size_t trs_soc_get_cloud_v4_notify_size(void)
{
    return (size_t)TRS_CLOUD_V4_NOTIFY_SIZE;
}

/* Cnt notify */
#define TRS_CLOUD_V4_CNT_NOTIFY_SIZE 4
#define TRS_CLOUD_V4_CNT_NOTIFY_INTERVAL 32
#define TRS_CLOUD_V4_CNT_NOTIFY_SEPARATE_CNT 16
#define TRS_CLOUD_V4_CNT_NOTIFY_SEPARATE_SIZE 0x1000
#define TRS_CLOUD_V4_CNT_NOTIFY_SLICE_SIZE 0x10000
#define TRS_CLOUD_V4_CNT_NOTIFY_NUM_PER_SLICE 128 /* total slice num is 16 */

static inline u32 trs_soc_get_cloud_v4_cnt_notify_offset(u32 id)
{
    return (id % TRS_CLOUD_V4_CNT_NOTIFY_NUM_PER_SLICE) % TRS_CLOUD_V4_CNT_NOTIFY_SEPARATE_CNT *
               TRS_CLOUD_V4_CNT_NOTIFY_INTERVAL +
           (id % TRS_CLOUD_V4_CNT_NOTIFY_NUM_PER_SLICE) / TRS_CLOUD_V4_CNT_NOTIFY_SEPARATE_CNT *
               TRS_CLOUD_V4_CNT_NOTIFY_SEPARATE_SIZE +
           (id / TRS_CLOUD_V4_CNT_NOTIFY_NUM_PER_SLICE) * TRS_CLOUD_V4_CNT_NOTIFY_SLICE_SIZE;
}

static inline size_t trs_soc_get_cloud_v4_cnt_notify_size(void)
{
    return (size_t)TRS_CLOUD_V4_CNT_NOTIFY_SIZE;
}

#define TRS_CLOUD_V4_STARS_SCHED_STRIDE (4 * 1024)
static inline size_t trs_soc_get_cloud_v4_stars_sched_stride(void)
{
    return (u32)TRS_CLOUD_V4_STARS_SCHED_STRIDE;
}

static inline u32 trs_soc_get_cloud_v4_sq_mem_side(u32 devid, struct trs_chan_type *types)
{
    if (devdrv_get_connect_protocol(devid) == CONNECT_PROTOCOL_PCIE) {
        if ((types->type == CHAN_TYPE_HW) && (types->sub_type == CHAN_SUB_TYPE_HW_RTS)) {
            return TRS_CHAN_DEV_SVM_MEM;
        }

        return TRS_CHAN_HOST_MEM;
    } else { /* UB use host ddr, copy to dev mem by urma */
        return TRS_CHAN_HOST_PHY_MEM;
    }
}

static inline u32 trs_soc_get_cloud_v4_cq_mem_side(u32 devid)
{
    if (devdrv_get_connect_protocol(devid) == CONNECT_PROTOCOL_PCIE) {
        return TRS_CHAN_HOST_MEM;
    } else {
        return TRS_CHAN_HOST_PHY_MEM;
    }
}

static inline int trs_soc_get_cloud_v4_sq_head_reg_offset(void)
{
    return TRS_STARS_V2_SCHED_SQ_HEAD_OFFSET;
}

static inline int trs_soc_get_cloud_v4_sq_tail_reg_offset(void)
{
    return TRS_STARS_V2_SCHED_SQ_TAIL_OFFSET;
}

static inline int trs_soc_get_cloud_v4_sq_status_reg_offset(void)
{
    return TRS_STARS_V2_SCHED_SQ_STATUS_OFFSET;
}

static inline int trs_soc_get_cloud_v4_cq_head_reg_offset(void)
{
    return TRS_STARS_V2_SCHED_CQ_HEAD_OFFSET;
}

static inline int trs_soc_get_cloud_v4_cq_tail_reg_offset(void)
{
    return TRS_STARS_V2_SCHED_CQ_TAIL_OFFSET;
}

static inline int trs_soc_get_cloud_v4_db_cfg(int db_type, u32 *start, u32 *end)
{
    return -EOPNOTSUPP;
}

static inline bool trs_cloud_v4_is_support_soft_mbox(void)
{
    return true;
}

static inline bool trs_soc_is_cloud_v4_support_virt(u32 devid)
{
    return true;
}
#endif /* SOC_ADAPT_RES_CLOUD_V4_H__ */
