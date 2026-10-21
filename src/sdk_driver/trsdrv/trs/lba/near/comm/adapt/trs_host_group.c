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
#include "pbl/pbl_uda.h"
#include "comm_kernel_interface.h"
#include "pbl/pbl_soc_res.h"
#include "trs_id_range.h"
#include "trs_stars.h"
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
#include "trs_ub_init_common.h"
#endif
#include "trs_host_group.h"
#ifdef CFG_FEATURE_TRS_SIA_ADAPT
#include "trs_sia_adapt_auto_init.h"
#elif defined(CFG_FEATURE_TRS_SEC_EH_ADAPT)
#include "trs_sec_eh_auto_init.h"
#endif

int trs_get_vfid_by_grp_id(u32 devid, u32 grp_id, u32 *vfid)
{
    struct soc_mia_grp_info grp_info;
    int ret;

    ret = soc_resmng_dev_get_mia_grp_info(devid, grp_id, &grp_info);
    if (ret != 0) {
        trs_err("Failed to get vfid. (devid=%u; grp_id=%u; ret=%d)\n", devid, grp_id, ret);
        return ret;
    }
    if (grp_info.vfid >= TRS_VF_MAX_NUM) {
        trs_err("Invalid vfid. (udevid=%u; grp_id=%u; vfid=%u)\n", devid, grp_id, grp_info.vfid);
        return -EINVAL;
    }

    *vfid = grp_info.vfid;
    return 0;
}

#define TRS_SQCQ_MAX_SLICE_NUM 16
static bool *g_trs_sqcq_change_flag[TRS_DEV_MAX_NUM] = {NULL};
static int trs_sqcq_change_flag_create(u32 udevid)
{
    g_trs_sqcq_change_flag[udevid] = (bool *)ka_mm_vzalloc(sizeof(bool) * TRS_SQCQ_MAX_SLICE_NUM);
    if (g_trs_sqcq_change_flag[udevid] == NULL) {
        return -ENOMEM;
    }
    return 0;
}

static void trs_sqcq_change_flag_destroy(u32 udevid)
{
    if (g_trs_sqcq_change_flag[udevid] != NULL) {
        ka_mm_vfree(g_trs_sqcq_change_flag[udevid]);
        g_trs_sqcq_change_flag[udevid] = NULL;
    }
}

bool trs_get_sqcq_change_flag(u32 devid, u32 slice)
{
    if (!uda_is_phy_dev(devid) && (devid < TRS_DEV_MAX_NUM) && (slice < TRS_SQCQ_MAX_SLICE_NUM)) {
        return g_trs_sqcq_change_flag[devid][slice];
    }
    return false;
}

int trs_sqcq_change_flag_init(struct trs_id_inst *inst, u32 grp_id, u32 vfid)
{
    u32 vfgid, vnpu_vfid;
    int ret, bit;

    ret = soc_resmng_dev_get_mia_base_info(inst->devid, &vfgid, &vnpu_vfid);
    if (ret != 0) {
        trs_err("Get mia_base_info failed. (udevid=%u)\n", inst->devid);
        return ret;
    }

    if (vfid != vnpu_vfid) {
        struct soc_mia_grp_info grp_info;
        ret = soc_resmng_dev_get_mia_grp_info(inst->devid, grp_id, &grp_info);
        if (ret != 0) {
            trs_err("Failed to get mia group info. (udevid=%u; grp_id=%u)\n", inst->devid, grp_id);
            return ret;
        }
        trs_info("Set change flag. (devid=%u; vfid=%u; vnpu_vfid=%u; bitmap=0x%llx)\n", inst->devid, vfid, vnpu_vfid,
                 grp_info.rtsq_info.bitmap);
        for (bit = 0; bit < TRS_MAX_ID_BIT_NUM; bit++) {
            if (trs_bitmap_bit_is_vaild(grp_info.rtsq_info.bitmap, bit)) {
                g_trs_sqcq_change_flag[inst->devid][bit] = true;
            }
        }
    }

    return 0;
}

void trs_sqcq_change_flag_uninit(struct trs_id_inst *inst, u32 grp_id, u32 vfid)
{
    u32 vfgid, vnpu_vfid;
    int ret, bit;

    ret = soc_resmng_dev_get_mia_base_info(inst->devid, &vfgid, &vnpu_vfid);
    if (ret != 0) {
        trs_err("Get mia_base_info failed. (udevid=%u)\n", inst->devid);
        return;
    }

    if (vfid != vnpu_vfid) {
        struct soc_mia_grp_info grp_info;
        ret = soc_resmng_dev_get_mia_grp_info(inst->devid, grp_id, &grp_info);
        if (ret != 0) {
            trs_err("Failed to get mia group info. (udevid=%u; grp_id=%u)\n", inst->devid, grp_id);
            return;
        }
        for (bit = 0; bit < TRS_MAX_ID_BIT_NUM; bit++) {
            if (trs_bitmap_bit_is_vaild(grp_info.rtsq_info.bitmap, bit)) {
                g_trs_sqcq_change_flag[inst->devid][bit] = false;
            }
        }
    }
}

#define TRS_SQCQ_REG_BAR_OFFSET 0x2000000U /* The second group has a 32M bar offset in host */
static bool trs_get_sqcq_change_flag_with_id(u32 devid, u32 id)
{
    struct res_inst_info res_inst;
    struct soc_mia_res_info_ex info;
    u32 slice;
    int ret;

    res_inst.devid = devid;
    res_inst.subid = 0;
    res_inst.sub_type = TS_SUBSYS;
    ret = soc_resmng_get_mia_res_ex(&res_inst, MIA_STARS_RTSQ, &info);
    if ((ret != 0) || (info.unit_per_bit == 0)) {
        trs_err("Failed to get mia res ex. (ret=%d; devid=%u)\n", ret, devid);
        return false;
    }
    slice = id / info.unit_per_bit;
    return trs_get_sqcq_change_flag(devid, slice);
}

static u32 trs_get_stars_bar_addr_offset_with_slice(u32 devid, u32 val)
{
    return trs_get_sqcq_change_flag(devid, val) ? TRS_SQCQ_REG_BAR_OFFSET : 0;
}

static u32 trs_get_stars_bar_addr_offset_with_id(u32 devid, u32 val)
{
    return trs_get_sqcq_change_flag_with_id(devid, val) ? TRS_SQCQ_REG_BAR_OFFSET : 0;
}

int trs_stars_init_with_group(struct trs_id_inst *inst, u32 vfid)
{
    u32 vfgid, vnpu_vfid;
    int ret;

    ret = soc_resmng_dev_get_mia_base_info(inst->devid, &vfgid, &vnpu_vfid);
    if (ret != 0) {
        trs_err("Failed to get vfid. (udevid=%u)\n", inst->devid);
        return ret;
    }

    if (vfid == vnpu_vfid) {
        return 0;
    }

    ret = trs_stars_addr_adjust_register(inst, TRS_STARS_SCHED, trs_get_stars_bar_addr_offset_with_id);
    if (ret != 0) {
        trs_err("Failed to init TRS_STARS_SCHED group. (ret=%d; devid=%u; vfid=%u)\n", ret, inst->devid, vfid);
        return ret;
    }
    ret = trs_stars_addr_adjust_register(inst, TRS_STARS_CQINT, trs_get_stars_bar_addr_offset_with_slice);
    if (ret != 0) {
        trs_err("Failed to init TRS_STARS_CQINT group. (ret=%d; devid=%u; vfid=%u)\n", ret, inst->devid, vfid);
        return ret;
    }
    trs_info("Init group. (devid=%u; vnpu_vfid=%u; vfid=%u)\n", inst->devid, vnpu_vfid, vfid);
    return 0;
}

void trs_stars_uninit_with_group(struct trs_id_inst *inst, u32 vfid)
{
    u32 vfgid, vnpu_vfid;
    int ret;

    ret = soc_resmng_dev_get_mia_base_info(inst->devid, &vfgid, &vnpu_vfid);
    if (ret != 0) {
        trs_err("Failed to get vfid. (udevid=%u)\n", inst->devid);
        return;
    }
    if (vfid == vnpu_vfid) {
        return;
    }

    trs_stars_addr_adjust_unregister(inst, TRS_STARS_SCHED);
    trs_stars_addr_adjust_unregister(inst, TRS_STARS_CQINT);
    trs_info("Uninit group. (devid=%u; vnpu_vfid=%u; vfid=%u)\n", inst->devid, vnpu_vfid, vfid);
}

int trs_add_group(struct trs_id_inst *inst, u32 grp_id)
{
    u32 vfid;
    int ret;

    ret = trs_get_vfid_by_grp_id(inst->devid, grp_id, &vfid);
    if (ret != 0) {
        return ret;
    }
    trs_info("Init start. (udevid=%u; grp_id=%u; vfid=%u)\n", inst->devid, grp_id, vfid);
    if (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_UB) {
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
        ret = trs_ub_dev_init_with_group(inst->devid, vfid);
#endif
    } else {
        ret = trs_stars_init_with_group(inst, vfid);
        if (ret != 0) {
            trs_err("Failed to init group. (ret=%d; udevid=%u; grp_id=%u; vfid=%u)\n", ret, inst->devid, grp_id, vfid);
            return ret;
        }
        ret = trs_sqcq_change_flag_init(inst, grp_id, vfid);
        if (ret != 0) {
            trs_err("Init change flag failed. (ret=%d; udevid=%u; grp_id=%u; vfid=%u)\n", ret, inst->devid, grp_id,
                    vfid);
        }
    }
    return ret;
}

int trs_delete_group(struct trs_id_inst *inst, u32 grp_id)
{
    u32 vfid;
    int ret;

    ret = trs_get_vfid_by_grp_id(inst->devid, grp_id, &vfid);
    if (ret != 0) {
        return ret;
    }

    if (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_UB) {
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
        trs_ub_dev_uninit_with_group(inst->devid, vfid);
#endif
    } else {
        trs_sqcq_change_flag_uninit(inst, grp_id, vfid);
        trs_stars_uninit_with_group(inst, vfid);
    }
    return 0;
}

int trs_host_group_init(u32 ts_inst_id)
{
    struct trs_id_inst inst;
    int ret = 0;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (!uda_is_phy_dev(inst.devid) && (inst.tsid == 0)) {
        ret = trs_sqcq_change_flag_create(inst.devid);
        trs_info("Init group. (devid=%u; ret=%d)\n", inst.devid, ret);
    }
    return ret;
}
DECLAER_FEATURE_AUTO_INIT_DEV(trs_host_group_init, FEATURE_LOADER_STAGE_3);

void trs_host_group_uninit(u32 ts_inst_id)
{
    struct trs_id_inst inst;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (!uda_is_phy_dev(inst.devid) && (inst.tsid == 0)) {
        trs_sqcq_change_flag_destroy(inst.devid);
        trs_info("Uninit group. (devid=%u)\n", inst.devid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(trs_host_group_uninit, FEATURE_LOADER_STAGE_3);
