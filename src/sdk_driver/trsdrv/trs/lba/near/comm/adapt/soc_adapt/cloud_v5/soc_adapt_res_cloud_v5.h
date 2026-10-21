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
#ifndef SOC_ADAPT_RES_CLOUD_V5_H__
#define SOC_ADAPT_RES_CLOUD_V5_H__

#include "ka_type.h"

#include "comm_kernel_interface.h"
#include "trs_pm_adapt.h"
#include "trs_pub_def.h"
#include "trs_rsv_mem.h"
#include "trs_stars_v2_reg_def.h"

int trs_soc_cloud_v5_chan_stars_init(struct trs_id_inst *inst);
void trs_soc_cloud_v5_chan_stars_uninit(struct trs_id_inst *inst);
int trs_soc_cloud_v5_chan_stars_ops_init(struct trs_id_inst *inst);
void trs_soc_cloud_v5_chan_stars_ops_uninit(struct trs_id_inst *inst);
struct trs_chan_adapt_ops *trs_chan_cloud_v5_get_stars_adapt_ops(void);
struct trs_core_adapt_ops *trs_core_cloud_v5_get_stars_adapt_ops(void);
int trs_soc_cloud_v5_sq_send_trigger_db_init(struct trs_id_inst *inst);
void trs_soc_cloud_v5_sq_send_trigger_db_uninit(struct trs_id_inst *inst);

/* Notify */
#define TRS_CLOUD_V5_NOTIFY_SIZE 4
#define TRS_CLOUD_V5_NOTIFY_INTERVAL 128
#define TRS_CLOUD_V5_NOTIFY_SEPARATE_CNT 16
#define TRS_CLOUD_V5_NOTIFY_SEPARATE_SIZE 0x1000
#define TRS_CLOUD_V5_NOTIFY_SLICE_SIZE 0x130000
#define TRS_CLOUD_V5_NOTIFY_NUM_PER_SLICE 4864

static inline u32 trs_soc_get_cloud_v5_notify_offset(u32 id)
{
    return (id % TRS_CLOUD_V5_NOTIFY_NUM_PER_SLICE) % TRS_CLOUD_V5_NOTIFY_SEPARATE_CNT * TRS_CLOUD_V5_NOTIFY_INTERVAL +
           (id % TRS_CLOUD_V5_NOTIFY_NUM_PER_SLICE) / TRS_CLOUD_V5_NOTIFY_SEPARATE_CNT *
               TRS_CLOUD_V5_NOTIFY_SEPARATE_SIZE +
           (id / TRS_CLOUD_V5_NOTIFY_NUM_PER_SLICE) * TRS_CLOUD_V5_NOTIFY_SLICE_SIZE;
}

static inline size_t trs_soc_get_cloud_v5_notify_size(void)
{
    return (size_t)TRS_CLOUD_V5_NOTIFY_SIZE;
}

/* Cnt notify */
#define TRS_CLOUD_V5_CNT_NOTIFY_SIZE 4
#define TRS_CLOUD_V5_CNT_NOTIFY_INTERVAL 128
#define TRS_CLOUD_V5_CNT_NOTIFY_SEPARATE_CNT 16
#define TRS_CLOUD_V5_CNT_NOTIFY_SEPARATE_SIZE 0x1000
#define TRS_CLOUD_V5_CNT_NOTIFY_SLICE_SIZE 0x130000
#define TRS_CLOUD_V5_CNT_NOTIFY_NUM_PER_SLICE 4846

static inline u32 trs_soc_get_cloud_v5_cnt_notify_offset(u32 id)
{
    return (id % TRS_CLOUD_V5_CNT_NOTIFY_NUM_PER_SLICE) % TRS_CLOUD_V5_CNT_NOTIFY_SEPARATE_CNT *
               TRS_CLOUD_V5_CNT_NOTIFY_INTERVAL +
           (id % TRS_CLOUD_V5_CNT_NOTIFY_NUM_PER_SLICE) / TRS_CLOUD_V5_CNT_NOTIFY_SEPARATE_CNT *
               TRS_CLOUD_V5_CNT_NOTIFY_SEPARATE_SIZE +
           (id / TRS_CLOUD_V5_CNT_NOTIFY_NUM_PER_SLICE) * TRS_CLOUD_V5_CNT_NOTIFY_SLICE_SIZE;
}

static inline size_t trs_soc_get_cloud_v5_cnt_notify_size(void)
{
    return (size_t)TRS_CLOUD_V5_CNT_NOTIFY_SIZE;
}

#define TRS_cloud_v5_STARS_SCHED_STRIDE (4 * 1024)
static inline size_t trs_soc_get_cloud_v5_stars_sched_stride(void)
{
    return (u32)TRS_cloud_v5_STARS_SCHED_STRIDE;
}

static inline u32 trs_soc_get_cloud_v5_sq_mem_side(u32 devid, struct trs_chan_type *types)
{
    if (devdrv_get_connect_protocol(devid) == CONNECT_PROTOCOL_PCIE) {
        if ((types->type == CHAN_TYPE_HW) && (types->sub_type == CHAN_SUB_TYPE_HW_RTS)) {
            return TRS_CHAN_DEV_SVM_MEM;
        }

        return TRS_CHAN_HOST_MEM;
    } else {
        return TRS_CHAN_HOST_PHY_MEM;
    }
}

static inline u32 trs_soc_get_cloud_v5_cq_mem_side(u32 devid)
{
    if (devdrv_get_connect_protocol(devid) == CONNECT_PROTOCOL_PCIE) {
        return TRS_CHAN_HOST_MEM;
    } else {
        return TRS_CHAN_HOST_PHY_MEM;
    }
}

static inline int trs_soc_get_cloud_v5_sq_head_reg_offset(void)
{
    return TRS_STARS_V2_SCHED_SQ_HEAD_OFFSET;
}

static inline int trs_soc_get_cloud_v5_sq_tail_reg_offset(void)
{
    return TRS_STARS_V2_SCHED_SQ_TAIL_OFFSET;
}

static inline int trs_soc_get_cloud_v5_sq_status_reg_offset(void)
{
    return TRS_STARS_V2_SCHED_SQ_STATUS_OFFSET;
}

static inline int trs_soc_get_cloud_v5_cq_head_reg_offset(void)
{
    return TRS_STARS_V2_SCHED_CQ_HEAD_OFFSET;
}

static inline int trs_soc_get_cloud_v5_cq_tail_reg_offset(void)
{
    return TRS_STARS_V2_SCHED_CQ_TAIL_OFFSET;
}

static inline int trs_soc_get_cloud_v5_db_cfg(int db_type, u32 *start, u32 *end)
{
    return -EOPNOTSUPP;
}

static inline bool trs_cloud_v5_is_support_soft_mbox(void)
{
    return true;
}

static inline bool trs_soc_is_cloud_v5_support_virt(u32 devid)
{
    return true;
}
#endif /* SOC_ADAPT_RES_CLOUD_V5_H__ */
