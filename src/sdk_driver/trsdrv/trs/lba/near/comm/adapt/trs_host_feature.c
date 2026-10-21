/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "ka_base_pub.h"
#include "securec.h"

#include "ascend_kernel_hal.h"
#ifdef CFG_FEATURE_TRS_SIA_ADAPT
#include "trs_sia_adapt_auto_init.h"
#elif defined(CFG_FEATURE_TRS_SEC_EH_ADAPT)
#include "trs_sec_eh_auto_init.h"
#endif
#include "pbl_kernel_interface.h"

#include "trs_feature_ioctl.h"
#include "trs_pub_def.h"
#include "trs_ioctl.h"

static trs_feat_bitmap_t g_trs_chip_features[HISI_CHIP_NUM] = {
    [HISI_CLOUD_V2] = {BIT(TRS_FEATURE_MAX)},
    [HISI_MINI_V3] = {BIT(TRS_FEATURE_MAX)},
    [HISI_CLOUD_V4] = {BIT(TRS_FEATURE_MAX)},
    [HISI_CLOUD_V5] = {BIT(TRS_FEATURE_MAX)},
};

static trs_feat_bitmap_t g_trs_dev_features[TRS_DEV_MAX_NUM] = {0};

static int trs_ioctl_feature_query(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct trs_feature_query_para *in_para = (struct trs_feature_query_para *)in;
    struct trs_feature_query_para *out_para = (struct trs_feature_query_para *)out;
    u32 udevid;
    u32 copy_size;
    int ret;

    if ((in == NULL) || (in_len < sizeof(*in_para)) || (out == NULL) || (out_len < sizeof(*out_para))) {
        trs_debug("Input param not valid. (in_is_null=%d, in_len=%u, expected=%u, out_is_null=%d, out_len=%u)\n",
                  (in == NULL), in_len, (u32)sizeof(*in_para), (out == NULL), out_len);
        return -EINVAL;
    }
    if (in_para->bitmap_size == 0) {
        trs_debug("Bitmap size not valid. (size=%u)\n", in_para->bitmap_size);
        return -EINVAL;
    }

    ret = uda_devid_to_udevid_ex(in_para->dev_id, &udevid);
    if (ret != 0) {
        trs_debug("Device id convert not succeed. (dev_id=%u, ret=%d)\n", in_para->dev_id, ret);
        return ret;
    }

    copy_size = (in_para->bitmap_size < (u32)sizeof(trs_feat_bitmap_t)) ? in_para->bitmap_size :
                                                                          (u32)sizeof(trs_feat_bitmap_t);
    ret = memcpy_s(out_para->bitmap, copy_size, g_trs_dev_features[udevid], copy_size);
    if (ret != 0) {
        return -EINVAL;
    }
    return 0;
}

#define DMS_MODULE_TRS_FEATURE "trs_feature"
BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_TRS_FEATURE)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_TRS_FEATURE, DMS_MAIN_CMD_TRS, DMS_SUBCMD_QUERY_TRS_FEATURE, NULL, NULL,
                    DMS_SUPPORT_ALL_USER, trs_ioctl_feature_query)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

static int trs_feature_dev_init(u32 ts_inst_id)
{
    struct trs_id_inst inst = {0};
    u32 chip_type;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (inst.tsid != 0) {
        return 0;
    }

    chip_type = uda_get_chip_type(inst.devid);
    if (chip_type >= HISI_CHIP_UNKNOWN) {
        trs_debug("Unrecognized chip type. (devid=%u, chip_type=%u)\n", inst.devid, chip_type);
        return -EINVAL;
    }

    if (memcpy_s(g_trs_dev_features[inst.devid], sizeof(trs_feat_bitmap_t), g_trs_chip_features[chip_type],
                 sizeof(trs_feat_bitmap_t)) != 0) {
        trs_debug("Feature memcpy_s not succeed. (devid=%u, chip_type=%u)\n", inst.devid, chip_type);
        return -EFAULT;
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(trs_feature_dev_init, FEATURE_LOADER_STAGE_0);

static void trs_feature_dev_uninit(u32 ts_inst_id)
{
    struct trs_id_inst inst = {0};

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (inst.tsid != 0) {
        return;
    }

    (void)memset_s(g_trs_dev_features[inst.devid], sizeof(g_trs_dev_features[inst.devid]), 0,
                   sizeof(g_trs_dev_features[inst.devid]));
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(trs_feature_dev_uninit, FEATURE_LOADER_STAGE_0);

static int trs_feature_init(void)
{
    CALL_INIT_MODULE(DMS_MODULE_TRS_FEATURE);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(trs_feature_init, FEATURE_LOADER_STAGE_6);

static void trs_feature_uninit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_TRS_FEATURE);
}
DECLAER_FEATURE_AUTO_UNINIT(trs_feature_uninit, FEATURE_LOADER_STAGE_6);
