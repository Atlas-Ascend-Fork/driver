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
#include "pbl/pbl_soc_res.h"
#include "trs_pub_def.h"
#include "trs_id.h"
#include "trs_id_range.h"

static u32 trs_get_id_bitmap_in_grp_with_type(struct soc_mia_grp_info *grp_info, u32 type)
{
    if ((type == TRS_NOTIFY_ID) || (type == TRS_CNT_NOTIFY_ID)) {
        return (u32)grp_info->notify_info.bitmap;
    } else {
        return (u32)grp_info->rtsq_info.bitmap;
    }
}

static int trs_id_get_num_per_bit(struct trs_id_inst *inst, int type, u32 *num_per_bit)
{
    struct res_inst_info res_inst;
    struct soc_mia_res_info_ex info;
    enum soc_mia_res_type res_type = MIA_MAX_RES_TYPE;
    int ret;

    res_inst.devid = inst->devid;
    res_inst.subid = inst->tsid;
    res_inst.sub_type = TS_SUBSYS;
    if (trs_id_is_hw_divide_type(type)) {
        res_type = trs_id_type_convert(type);
    }
    if (type == TRS_STREAM_ID) {
        res_type = MIA_STARS_STREAM;
    }
    if (type == TRS_MODEL_ID) {
        res_type = MIA_STARS_MODEL;
    }

    ret = soc_resmng_get_mia_res_ex(&res_inst, res_type, &info);
    if ((ret != 0) || (info.unit_per_bit == 0)) {
        trs_err("Failed to get mia res. (ret=%d; devid=%u; unit_per_bit=%u)\n", ret, inst->devid, info.unit_per_bit);
        return ret;
    }
    *num_per_bit = info.unit_per_bit;
    trs_debug("Get num per bit. (devid=%u; type=%u; num_per_bit=%u)\n", inst->devid, type, *num_per_bit);
    return 0;
}

static int trs_id_get_res_bitmap(struct trs_id_inst *inst, int type, u32 vfid, u32 *bitmap)
{
    struct soc_mia_grp_info grp_info;
    int grp_id;
    int ret;

    for (grp_id = 0; grp_id < SOC_MAX_MIA_GROUP_NUM; grp_id++) {
        ret = soc_resmng_dev_get_mia_grp_info(inst->devid, grp_id, &grp_info);
        if (ret != 0) {
            trs_err("Failed to get mia group info. (devid=%u; grp_id=%u)\n", inst->devid, grp_id);
            return ret;
        }
        if ((grp_info.valid == 1) && (vfid == grp_info.vfid)) {
            *bitmap = trs_get_id_bitmap_in_grp_with_type(&grp_info, type);
            trs_debug("Get bitmap in group. (udevid=%u; vfid=%u; grp_id=%u; type=%u; bitmap=%u)\n", inst->devid, vfid,
                      grp_id, type, *bitmap);
            return 0;
        }
    }

    return -EINVAL;
}

int trs_id_alloc_in_range(struct trs_id_inst *inst, int type, u32 *id, u32 vfid)
{
    u32 bitmap = 0, num_per_bit = 0;
    int bit, ret;
    u32 flag = 0;

    ret = trs_id_get_res_bitmap(inst, type, vfid, &bitmap);
    if (ret != 0) {
        trs_err("Failed to get res range. (devid=%u; vfid=%u; type=%s; ret=%d)\n", inst->devid, vfid,
                trs_id_type_to_name(type), ret);
        return ret;
    }

    ret = trs_id_get_num_per_bit(inst, type, &num_per_bit);
    if (ret != 0) {
        trs_err("Failed to get num_per_bit. (devid=%u; type=%s; ret=%d)\n", inst->devid, trs_id_type_to_name(type),
                ret);
        return ret;
    }

    trs_id_set_specified_flag(&flag);
    for (bit = 0; bit < TRS_MAX_ID_BIT_NUM; bit++) {
        if (trs_bitmap_bit_is_vaild(bitmap, bit)) {
            *id = ((u32)bit) * num_per_bit;
            ret = trs_id_alloc_ex(inst, type, flag, id, num_per_bit);
            if (ret == 0) {
                trs_debug("Alloc id in range success. (devid=%u; vfid=%u; type=%s; bitmap=0x%x; bit=%d; id=%u)\n",
                          inst->devid, vfid, trs_id_type_to_name(type), bitmap, bit, *id);
                return 0;
            }
        }
    }
    trs_err("Failed to alloc id in range. (devid=%u; vfid=%u; type=%s; bitmap=0x%x; bit=%d; ret=%d)\n", inst->devid,
            vfid, trs_id_type_to_name(type), bitmap, bit, ret);
    return ret;
}
