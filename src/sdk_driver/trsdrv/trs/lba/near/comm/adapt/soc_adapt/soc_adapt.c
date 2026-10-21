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
#include "ka_kernel_def_pub.h"
#include "comm_kernel_interface.h"
#include "trs_chip_def_comm.h"
#include "trs_chan_mem.h"
#include "soc_adapt_res_mini_v3.h"
#include "soc_adapt_res_cloud_v2.h"
#include "soc_adapt_res_cloud_v4.h"
#include "soc_adapt_res_cloud_v5.h"
#include "soc_adapt_res_mini_v2.h"
#include "soc_adapt.h"

static const int trs_soc_chip_type[HISI_CHIP_UNKNOWN] = {
    [HISI_MINI_V1] = TRS_CHIP_TYPE_MINI_V1,   [HISI_CLOUD_V1] = TRS_CHIP_TYPE_CLOUD_V1,
    [HISI_MINI_V2] = TRS_CHIP_TYPE_MINI_V2,   [HISI_CLOUD_V2] = TRS_CHIP_TYPE_CLOUD_V2,
    [HISI_MINI_V3] = TRS_CHIP_TYPE_MINI_V3,   [HISI_CLOUD_V4] = TRS_CHIP_TYPE_CLOUD_V4,
    [HISI_CLOUD_V5] = TRS_CHIP_TYPE_CLOUD_V5, [HISI_MINI_V4] = TRS_CHIP_TYPE_CLOUD_V4,
};

int trs_soc_get_chip_type(u32 phy_devid)
{
    u32 chip_type = uda_get_chip_type(phy_devid);
    if (chip_type >= HISI_CHIP_UNKNOWN) {
        trs_err("Get chip type fail. (devid=%u; chip_type=%u)\n", phy_devid, chip_type);
        return TRS_CHIP_TYPE_UNKNOWN;
    }
    return trs_soc_chip_type[chip_type];
}
KA_EXPORT_SYMBOL_GPL(trs_soc_get_chip_type);

int trs_soc_get_hw_type(u32 phy_devid)
{
    return trs_get_hw_type_by_chip_type(trs_soc_get_chip_type(phy_devid));
}

static size_t (*const soc_get_db_stride[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_db_stride,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_db_stride,
    [TRS_CHIP_TYPE_MINI_V2] = trs_soc_get_mini_v2_db_stride,
};
int trs_soc_get_db_stride(struct trs_id_inst *inst, size_t *size)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_db_stride[chip_type] != NULL) {
        *size = soc_get_db_stride[chip_type]();
        return 0;
    }

    return -EINVAL;
}

static int (*const soc_get_db_cfg[TRS_CHIP_TYPE_MAX])(int db_type, u32 *start, u32 *end) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_db_cfg,   [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_db_cfg,
    [TRS_CHIP_TYPE_MINI_V2] = trs_soc_get_mini_v2_db_cfg,   [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_db_cfg,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_db_cfg,
};
int trs_soc_get_db_cfg(struct trs_id_inst *inst, int db_type, u32 *start, u32 *end)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    int ret = -EINVAL;

    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_db_cfg[chip_type] != NULL) {
        ret = soc_get_db_cfg[chip_type](db_type, start, end);
    }

    return ret;
}

static size_t (*const soc_get_notify_size[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_notify_size,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_notify_size,
    [TRS_CHIP_TYPE_MINI_V2] = trs_soc_get_mini_v2_notify_size,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_notify_size,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_notify_size,
};
int trs_soc_get_notify_size(struct trs_id_inst *inst, size_t *notify_size)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_notify_size[chip_type] != NULL) {
        *notify_size = soc_get_notify_size[chip_type]();
        return 0;
    }

    return -EINVAL;
}

static u32 (*const soc_get_notify_offset[TRS_CHIP_TYPE_MAX])(u32 notify_id) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_notify_offset,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_notify_offset,
    [TRS_CHIP_TYPE_MINI_V2] = trs_soc_get_mini_v2_notify_offset,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_notify_offset,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_notify_offset,
};

int trs_soc_get_notify_offset(struct trs_id_inst *inst, u32 notify_id, u32 *offset)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_notify_offset[chip_type] != NULL) {
        *offset = soc_get_notify_offset[chip_type](notify_id);
        return 0;
    }

    return -EINVAL;
}

static size_t (*const soc_get_cnt_notify_size[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_cnt_notify_size,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_cnt_notify_size,
};
int trs_soc_get_cnt_notify_size(struct trs_id_inst *inst, size_t *cnt_notify_size)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_cnt_notify_size[chip_type] != NULL) {
        *cnt_notify_size = soc_get_cnt_notify_size[chip_type]();
        return 0;
    }

    return -EINVAL;
}

static u32 (*const soc_get_cnt_notify_offset[TRS_CHIP_TYPE_MAX])(u32 cnt_notify_id) = {
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_cnt_notify_offset,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_cnt_notify_offset,
};

int trs_soc_get_cnt_notify_offset(struct trs_id_inst *inst, u32 cnt_notify_id, u32 *offset)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_cnt_notify_offset[chip_type] != NULL) {
        *offset = soc_get_cnt_notify_offset[chip_type](cnt_notify_id);
        return 0;
    }

    return -EINVAL;
}

static u32 (*const soc_get_event_offset[TRS_CHIP_TYPE_MAX])(u32 event_id) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_event_offset,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_event_offset,
#ifndef EMU_ST
    [TRS_CHIP_TYPE_MINI_V2] = trs_soc_get_mini_v2_event_offset,
#endif
};

int trs_soc_get_event_offset(struct trs_id_inst *inst, u32 event_id, u32 *offset)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
#ifndef EMU_ST
        return -ENODEV;
#endif
    }

    if (soc_get_event_offset[chip_type] != NULL) {
        *offset = soc_get_event_offset[chip_type](event_id);
        return 0;
    }
#ifndef EMU_ST
    return -EINVAL;
#endif
}

static u32 (*const soc_get_sq_mem_side[TRS_CHIP_TYPE_MAX])(u32 devid, struct trs_chan_type *types) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_sq_mem_side,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_sq_mem_side,
    [TRS_CHIP_TYPE_MINI_V2] = trs_soc_get_mini_v2_sq_mem_side,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_sq_mem_side,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_sq_mem_side,
};
u32 trs_soc_get_sq_mem_side(struct trs_id_inst *inst, struct trs_chan_type *types)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    u32 sq_mem_side = TRS_CHAN_DEV_RSV_MEM;

    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return sq_mem_side;
    }

    if (soc_get_sq_mem_side[chip_type] != NULL) {
        sq_mem_side = soc_get_sq_mem_side[chip_type](inst->devid, types);
    }
    trs_debug("Get sq mem side. (devid=%d; sq mem side=%d; type=%u; sub_type=%u)\n", inst->devid, sq_mem_side,
              types->type, types->sub_type);
    return sq_mem_side;
}

static u32 (*const soc_get_cq_mem_side[TRS_CHIP_TYPE_MAX])(u32 devid) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_cq_mem_side,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_cq_mem_side,
    [TRS_CHIP_TYPE_MINI_V2] = trs_soc_get_mini_v2_cq_mem_side,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_cq_mem_side,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_cq_mem_side,
};
u32 trs_soc_get_cq_mem_side(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    u32 cq_mem_side = TRS_CHAN_DEV_RSV_MEM;

    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return cq_mem_side;
    }

    if (soc_get_cq_mem_side[chip_type] != NULL) {
        cq_mem_side = soc_get_cq_mem_side[chip_type](inst->devid);
    }

    return cq_mem_side;
}

static size_t (*const soc_get_stars_sched_stride[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_stars_sched_stride,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_stars_sched_stride,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_stars_sched_stride,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_stars_sched_stride,
};
int trs_soc_get_stars_sched_stride(struct trs_id_inst *inst, size_t *stride)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_stars_sched_stride[chip_type] != NULL) {
        *stride = soc_get_stars_sched_stride[chip_type]();
        return 0;
    }

    return -EINVAL;
}

static int (*const soc_get_hwcq_rsv_mem_type[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_hwcq_rsv_mem_type,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_hwcq_rsv_mem_type,
    [TRS_CHIP_TYPE_MINI_V2] = trs_soc_get_mini_v2_hwcq_rsv_mem_type,
};
int trs_soc_get_hwcq_rsv_mem_type(struct trs_id_inst *inst, int *rsv_mem_type)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_hwcq_rsv_mem_type[chip_type] != NULL) {
        *rsv_mem_type = soc_get_hwcq_rsv_mem_type[chip_type]();
        return 0;
    }

    return -EINVAL;
}

#define TRS_SQ_HEAD_TAIL_REG_SIZE 4
int trs_soc_get_sq_head_tail_reg_size(struct trs_id_inst *inst, size_t *reg_size)
{
    *reg_size = TRS_SQ_HEAD_TAIL_REG_SIZE;
    return 0;
}

static int (*const soc_get_sq_head_reg_offset[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_sq_head_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_sq_head_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_sq_head_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_sq_head_reg_offset,
};
int trs_soc_get_sq_head_reg_offset(struct trs_id_inst *inst, u32 *offset)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_sq_head_reg_offset[chip_type] != NULL) {
        *offset = soc_get_sq_head_reg_offset[chip_type]();
        return 0;
    }

    return -EINVAL;
}

static int (*const soc_get_sq_tail_reg_offset[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_sq_tail_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_sq_tail_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_sq_tail_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_sq_tail_reg_offset,
};
int trs_soc_get_sq_tail_reg_offset(struct trs_id_inst *inst, u32 *offset)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_sq_tail_reg_offset[chip_type] != NULL) {
        *offset = soc_get_sq_tail_reg_offset[chip_type]();
        return 0;
    }

    return -EINVAL;
}

static int (*const soc_get_sq_status_reg_offset[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_sq_status_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_sq_status_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_sq_status_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_sq_status_reg_offset,
};
int trs_soc_get_sq_status_reg_offset(struct trs_id_inst *inst, u32 *offset)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_sq_status_reg_offset[chip_type] != NULL) {
        *offset = soc_get_sq_status_reg_offset[chip_type]();
        return 0;
    }

    return -EINVAL;
}

static int (*const soc_get_cq_head_reg_offset[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_cq_head_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_cq_head_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_cq_head_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_cq_head_reg_offset,
};
int trs_soc_get_cq_head_reg_offset(struct trs_id_inst *inst, u32 *offset)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_cq_head_reg_offset[chip_type] != NULL) {
        *offset = soc_get_cq_head_reg_offset[chip_type]();
        return 0;
    }

    return -EINVAL;
}

static int (*const soc_get_cq_tail_reg_offset[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_get_mini_v3_cq_tail_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_get_cloud_v2_cq_tail_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_get_cloud_v4_cq_tail_reg_offset,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_get_cloud_v5_cq_tail_reg_offset,
};
int trs_soc_get_cq_tail_reg_offset(struct trs_id_inst *inst, u32 *offset)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_get_cq_tail_reg_offset[chip_type] != NULL) {
        *offset = soc_get_cq_tail_reg_offset[chip_type]();
        return 0;
    }

    return -EINVAL;
}

static int (*const soc_chan_stars_init[TRS_CHIP_TYPE_MAX])(struct trs_id_inst *inst) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_mini_v3_chan_stars_init,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_cloud_v2_chan_stars_init,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_cloud_v4_chan_stars_init,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_cloud_v5_chan_stars_init,
};
int trs_soc_chan_stars_init(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_chan_stars_init[chip_type] != NULL) {
        return soc_chan_stars_init[chip_type](inst);
    }

    return -EINVAL;
}

static void (*const soc_chan_stars_uninit[TRS_CHIP_TYPE_MAX])(struct trs_id_inst *inst) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_mini_v3_chan_stars_uninit,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_cloud_v2_chan_stars_uninit,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_cloud_v4_chan_stars_uninit,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_cloud_v5_chan_stars_uninit,
};
void trs_soc_chan_stars_uninit(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return;
    }

    if (soc_chan_stars_uninit[chip_type] != NULL) {
        soc_chan_stars_uninit[chip_type](inst);
    }
}

static int (*const soc_chan_stars_ops_init[TRS_CHIP_TYPE_MAX])(struct trs_id_inst *inst) = {
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_cloud_v2_chan_stars_ops_init,
#ifndef EMU_ST
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_mini_v3_chan_stars_ops_init,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_cloud_v4_chan_stars_ops_init,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_cloud_v5_chan_stars_ops_init,
#endif
};
int trs_soc_chan_stars_ops_init(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_chan_stars_ops_init[chip_type] != NULL) {
        return soc_chan_stars_ops_init[chip_type](inst);
    }
    return -EINVAL;
}

static void (*const soc_chan_stars_ops_uninit[TRS_CHIP_TYPE_MAX])(struct trs_id_inst *inst) = {
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_cloud_v2_chan_stars_ops_uninit,
#ifndef EMU_ST
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_mini_v3_chan_stars_ops_uninit,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_cloud_v4_chan_stars_ops_uninit,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_cloud_v5_chan_stars_ops_uninit,
#endif
};
void trs_soc_chan_stars_ops_uninit(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return;
    }

    if (soc_chan_stars_ops_uninit[chip_type] != NULL) {
        soc_chan_stars_ops_uninit[chip_type](inst);
    }
}

static struct trs_chan_adapt_ops *(*const soc_chan_get_stars_adapt_ops[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_chan_cloud_v2_get_stars_adapt_ops,
#ifndef EMU_ST
    [TRS_CHIP_TYPE_MINI_V3] = trs_chan_mini_v3_get_stars_adapt_ops,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_chan_cloud_v4_get_stars_adapt_ops,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_chan_cloud_v5_get_stars_adapt_ops,
#endif
};
struct trs_chan_adapt_ops *trs_soc_chan_get_stars_adapt_ops(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return NULL;
    }

    if (soc_chan_get_stars_adapt_ops[chip_type] != NULL) {
        return soc_chan_get_stars_adapt_ops[chip_type]();
    }

    return NULL;
}

static struct trs_core_adapt_ops *(*const soc_core_get_stars_adapt_ops[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_core_mini_v3_get_stars_adapt_ops,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_core_cloud_v2_get_stars_adapt_ops,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_core_cloud_v4_get_stars_adapt_ops,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_core_cloud_v5_get_stars_adapt_ops,
};
struct trs_core_adapt_ops *trs_soc_core_get_stars_adapt_ops(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return NULL;
    }

    if (soc_core_get_stars_adapt_ops[chip_type] != NULL) {
        return soc_core_get_stars_adapt_ops[chip_type]();
    }

    return NULL;
}

static bool (*const soc_is_support_soft_mbox_ops[TRS_CHIP_TYPE_MAX])(void) = {
    [TRS_CHIP_TYPE_MINI_V2] = trs_mini_v2_is_support_soft_mbox,
    [TRS_CHIP_TYPE_MINI_V3] = trs_mini_v3_is_support_soft_mbox,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_cloud_v2_is_support_soft_mbox,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_cloud_v4_is_support_soft_mbox,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_cloud_v5_is_support_soft_mbox,
};
bool trs_soc_is_support_soft_mbox(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return false;
    }

    if (soc_is_support_soft_mbox_ops[chip_type] != NULL) {
        return soc_is_support_soft_mbox_ops[chip_type]();
    }

    return false;
}

static bool (*const soc_is_support_virt_ops[TRS_CHIP_TYPE_MAX])(u32 devid) = {
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_is_cloud_v4_support_virt,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_is_cloud_v5_support_virt,
};
bool trs_soc_is_support_virt(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return false;
    }

    if (soc_is_support_virt_ops[chip_type] != NULL) {
        return soc_is_support_virt_ops[chip_type](inst->devid);
    }

    return false;
}

static int (*const soc_sq_send_trigger_db_init_ops[TRS_CHIP_TYPE_MAX])(struct trs_id_inst *inst) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_mini_v3_sq_send_trigger_db_init,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_cloud_v2_sq_send_trigger_db_init,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_cloud_v4_sq_send_trigger_db_init,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_cloud_v5_sq_send_trigger_db_init,
};
int trs_soc_sq_send_trigger_db_init(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return -ENODEV;
    }

    if (soc_sq_send_trigger_db_init_ops[chip_type] != NULL) {
        return soc_sq_send_trigger_db_init_ops[chip_type](inst);
    }

    return -EINVAL;
}

static void (*const soc_sq_send_trigger_db_uninit_ops[TRS_CHIP_TYPE_MAX])(struct trs_id_inst *inst) = {
    [TRS_CHIP_TYPE_MINI_V3] = trs_soc_mini_v3_sq_send_trigger_db_uninit,
    [TRS_CHIP_TYPE_CLOUD_V2] = trs_soc_cloud_v2_sq_send_trigger_db_uninit,
    [TRS_CHIP_TYPE_CLOUD_V4] = trs_soc_cloud_v4_sq_send_trigger_db_uninit,
    [TRS_CHIP_TYPE_CLOUD_V5] = trs_soc_cloud_v5_sq_send_trigger_db_uninit,
};
void trs_soc_sq_send_trigger_db_uninit(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_UNKNOWN) {
        return;
    }

    if (soc_sq_send_trigger_db_uninit_ops[chip_type] != NULL) {
        soc_sq_send_trigger_db_uninit_ops[chip_type](inst);
    }
}

#ifndef EMU_ST
bool trs_soc_support_sq_rsvmem_map(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if (chip_type == TRS_CHIP_TYPE_CLOUD_V2) {
        return true;
    }
    return false;
}
#endif

bool trs_soc_support_hw_ts_mbox(struct trs_id_inst *inst)
{
    int chip_type = trs_soc_get_chip_type(inst->devid);
    if ((chip_type == TRS_CHIP_TYPE_CLOUD_V4) || chip_type == TRS_CHIP_TYPE_CLOUD_V5) {
        return false;
    }
    return true;
}
