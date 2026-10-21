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
#include "ka_memory_pub.h"
#include "soc_adapt.h"
#include "pbl/pbl_soc_res.h"
#include "trs_msg.h"
#include "trs_host_id.h"
#include "trs_host_msg.h"
#include "trs_stars_reg_def.h"
#include "trs_chan_stars_v1_ops_stars.h"

static inline u32 _trs_chan_ops_stars_cqint_get_mid_status(struct trs_stars_cqint *cqint)
{
    return ka_mm_readl(cqint->base + TRS_STARS_HOST_CQINT_MID_STATUS_OFFSET(cqint->group));
}

static inline u32 _trs_chan_ops_stars_cqint_get_l2_status(struct trs_stars_cqint *cqint, u32 mid_bit)
{
    return ka_mm_readl(cqint->base + TRS_STARS_HOST_CQINT_L2_STATUS_OFFSET(cqint->group, mid_bit));
}

static void _trs_chan_ops_stars_cqint_set_l2_ctrl(struct trs_stars_cqint *cqint, u32 mid_bit, u32 l2_status)
{
    ka_mm_writel(l2_status, cqint->base + TRS_STARS_HOST_CQINT_L2_CTRL_OFFSET(cqint->group, mid_bit));
}

static int trs_chan_ops_stars_ops_get_valid_cq_list(struct trs_stars_cqint *cqint, u32 cqid[], u32 num, u32 *valid_num)
{
    u32 per_grp_cq_num, mid_bit, l2_status, mid_status, mid_width, l2_bit, cq_num = 0;

    *valid_num = 0;
    per_grp_cq_num = cqint->cq_num / cqint->cq_grp_num;
    mid_width = per_grp_cq_num / TRS_STARS_CQINT_L2_STATUS_WIDTH;
    mid_status = _trs_chan_ops_stars_cqint_get_mid_status(cqint);
    for (mid_bit = 0; mid_bit < mid_width; mid_bit++) {
        if (trs_stars_test_bit(mid_bit, mid_status) == 0) {
            continue;
        }
        l2_status = _trs_chan_ops_stars_cqint_get_l2_status(cqint, mid_bit);
        _trs_chan_ops_stars_cqint_set_l2_ctrl(cqint, mid_bit, l2_status);

        for (l2_bit = 0; l2_bit < TRS_STARS_CQINT_L2_STATUS_WIDTH; l2_bit++) {
            if (trs_stars_test_bit(l2_bit, l2_status) == 0) {
                continue;
            }

            if (cq_num >= num) {
                *valid_num = cq_num;
                return 0;
            }
            cqid[cq_num] = cqint->group * per_grp_cq_num + mid_bit * TRS_STARS_CQINT_L2_STATUS_WIDTH + l2_bit;
            trs_debug("Clear irq group %u mid_status %u l2_status %u cqid %u\n", cqint->group, mid_status, l2_status,
                      cqid[cq_num]);
            cq_num++;
        }
    }
    *valid_num = cq_num;

    return 0;
}

static void trs_chan_ops_stars_ops_set_cq_l1_mask(struct trs_stars_cqint *cqint, int val)
{
    if ((val & 0x1) == 0) {
        ka_mm_writel(1, cqint->base + TRS_STARS_HOST_CQINT_L1_CTRL_OFFSET(cqint->group));
    }
}

int trs_chan_get_cq_group_num(struct trs_id_inst *inst, u32 *group_num)
{
    struct trs_msg_data msg;
    struct trs_msg_cq_group *cq_group = (struct trs_msg_cq_group *)msg.payload;
    int ret;

    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_MSG_GET_CQ_GROUP;
    msg.header.tsid = inst->tsid;

    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        trs_err("Get group num failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    *group_num = cq_group->group_num;
    return 0;
}

int trs_chan_get_cq_group(struct trs_id_inst *inst, u32 group_index, u32 *group)
{
    struct trs_msg_data msg;
    struct trs_msg_cq_group *cq_group = (struct trs_msg_cq_group *)msg.payload;
    int ret;

    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_MSG_GET_CQ_GROUP;
    msg.header.tsid = inst->tsid;

    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        trs_err("Get group num failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    if (group_index >= cq_group->group_num) {
        trs_err("Para error. (devid=%u; tsid=%u; group_index=%u; group_num=%u)\n", inst->devid, inst->tsid, group_index,
                cq_group->group_num);
#ifndef EMU_ST
        return -EINVAL;
#endif
    }

    *group = cq_group->group[group_index];
    return 0;
}

int trs_chan_get_cq_group_index(struct trs_id_inst *inst, u32 group, u32 *group_index)
{
    struct trs_msg_data msg;
    struct trs_msg_cq_group *cq_group = (struct trs_msg_cq_group *)msg.payload;
    int ret;
    u32 i;

    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_MSG_GET_CQ_GROUP;
    msg.header.tsid = inst->tsid;

    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        trs_err("Get group num failed. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return ret;
    }

    for (i = 0; i < cq_group->group_num; i++) {
        if (cq_group->group[i] == group) {
            *group_index = i;
            return 0;
        }
    }

    return -EINVAL;
}

static int trs_chan_ops_stars_sched_attr_pack(struct trs_id_inst *inst, struct trs_stars_attr *attr, phys_addr_t paddr,
                                              size_t size)
{
    size_t stride;
    int ret;

    attr->paddr = paddr;
    attr->size = size;
    attr->set_cq_l1_mask = NULL;
    attr->get_valid_cq_list = NULL;
    ret = trs_soc_get_stars_sched_stride(inst, &stride);
    if (ret != 0) {
        trs_err("Get stars sched stride fail. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }
    attr->stride = (u32)stride;
    return 0;
}

static int trs_chan_ops_stars_cqint_attr_pack(struct trs_id_inst *inst, struct trs_stars_attr *attr, phys_addr_t paddr,
                                              size_t size)
{
    struct trs_msg_id_cap hw_id_cap, sw_id_cap;
    int ret;

    attr->paddr = paddr;
    attr->size = size;
    attr->set_cq_l1_mask = trs_chan_ops_stars_ops_set_cq_l1_mask;
    attr->get_valid_cq_list = trs_chan_ops_stars_ops_get_valid_cq_list;

    ret = trs_chan_get_cq_group_num(inst, &attr->cq_grp_num);
    ret |= trs_host_get_id_cap(inst, TRS_HW_CQ_ID, &hw_id_cap);
    ret |= trs_host_get_id_cap(inst, TRS_RSV_HW_CQ_ID, &sw_id_cap);
    if (ret == 0) {
        attr->cq_num = hw_id_cap.total_num + sw_id_cap.total_num;
    }

    return ret;
}

int trs_chan_stars_v1_ops_stars_init(struct trs_id_inst *inst)
{
    struct soc_reg_base_info io_base;
    struct trs_stars_attr stars_attr;
    struct res_inst_info res_inst;
    int ret;

    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);
    ret = soc_resmng_get_reg_base(&res_inst, "TS_STARS_RTSQ_SCHED_REG", &io_base);
    if (ret == 0) {
        ret = trs_chan_ops_stars_sched_attr_pack(inst, &stars_attr, io_base.io_base, io_base.io_base_size);
        if (ret != 0) {
            return ret;
        }
        ret = trs_stars_init(inst, TRS_STARS_SCHED, &stars_attr);
        if (ret != 0) {
            return ret;
        }
    }

    ret = soc_resmng_get_reg_base(&res_inst, "TS_STARS_CQINT_REG", &io_base);
    if (ret == 0) {
        ret = trs_chan_ops_stars_cqint_attr_pack(inst, &stars_attr, io_base.io_base, io_base.io_base_size);
        if (ret != 0) {
            trs_err("Cqint attr pack failed. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
            trs_stars_uninit(inst, TRS_STARS_SCHED);
            return ret;
        }
        trs_debug("Cq int. (devid=%u; tsid=%u; cq_grp_num=%u; cq_num=%u)\n", inst->devid, inst->tsid,
                  stars_attr.cq_grp_num, stars_attr.cq_num);

        ret = trs_stars_init(inst, TRS_STARS_CQINT, &stars_attr);
        if (ret != 0) {
            trs_stars_uninit(inst, TRS_STARS_SCHED);
            return ret;
        }
    }

    return 0;
}

void trs_chan_stars_v1_ops_stars_uninit(struct trs_id_inst *inst)
{
    trs_stars_uninit(inst, TRS_STARS_CQINT);
    trs_stars_uninit(inst, TRS_STARS_SCHED);
}
