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
#include "pbl/pbl_urd_main_cmd_def.h"
#include "pbl/pbl_urd_sub_cmd_def.h"
#include "pbl/pbl_feature_loader.h"
#include "dms/dms_cmd_def.h"

#include "asdrv_ub_hotreset.h"
#include "asdrv_ub_urd.h"
#include "asdrv_ub_main.h"

STATIC int ascend_ub_backup_online(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    u32 expect_len = sizeof(struct dms_device_replace_stru);
    struct dms_device_replace_stru *data;
    u32 dev_id;
    (void)feature;
    (void)out;
    (void)out_len;

    if ((in == NULL) || (in_len != expect_len)) {
        ubdrv_err("Check dsmi data len fail, or in is null. (in_len=%u;expect_len=%u)\n", in_len, expect_len);
        return -EINVAL;
    }

    data = (struct dms_device_replace_stru *)in;
    dev_id = (u32)data->dst_dev_attr.phy_dev_id;
    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Check dsmi data dev_id fail. (sdev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    return ubdrv_normal_preset(dev_id);
}

STATIC int ascend_ub_get_dev_id_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    u32 expect_in_len = sizeof(unsigned int);
    u32 expect_out_len = sizeof(struct devdrv_dev_id_info);
    struct devdrv_dev_id_info *info;
    u32 dev_id, udevid;
    int ret;

    if ((in == NULL) || (in_len != expect_in_len)) {
        ubdrv_err("Input argument is null, or in_len is wrong. (in_len=%u;expect_len=%u)\n", in_len, expect_in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != expect_out_len)) {
        ubdrv_err("Input argument is null, or out_len is wrong. (out_len=%u;expect_len=%u)\n", out_len, expect_out_len);
        return -EINVAL;
    }
    dev_id = *(unsigned int *)in;
    info = (struct devdrv_dev_id_info *)out;
    ret = uda_devid_to_udevid(dev_id, &udevid);
    if (ret != 0) {
        ubdrv_err("Get udevid failed. (dev_id=%u;udevid=%u;ret=%d)\n", dev_id, udevid, ret);
        return -EINVAL;
    }

    ret = devdrv_get_dev_id_info(udevid, info);
    if (ret != 0) {
        ubdrv_err("Get device id_info failed. (dev_id=%u;udevid=%u;ret=%d)\n", dev_id, udevid, ret);
        return ret;
    }
    return 0;
}

BEGIN_DMS_MODULE_DECLARATION(ASCEND_UB_CMD_NAME)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(ASCEND_UB_CMD_NAME, ASCEND_UB_CMD_BASIC, ASCEND_UB_SUBCMD_GET_URMA_NAME, NULL, NULL,
                    DMS_SUPPORT_ALL, ascend_ub_get_urma_name)
ADD_FEATURE_COMMAND(ASCEND_UB_CMD_NAME, ASCEND_UB_CMD_BASIC, ASCEND_UB_SUBCMD_GET_EID_INDEX, NULL, NULL,
                    DMS_SUPPORT_ALL, ascend_ub_get_eid_index)
ADD_FEATURE_COMMAND(ASCEND_UB_CMD_NAME, ASCEND_UB_CMD_BASIC, ASCEND_UB_SUBCMD_HOST_DEVICE_RELINK, NULL, NULL,
                    DMS_SUPPORT_ROOT_PHY, ascend_ub_backup_online)
ADD_FEATURE_COMMAND(ASCEND_UB_CMD_NAME, ASCEND_UB_CMD_BASIC, ASCEND_UB_SUBCMD_GET_DEV_ID_INFO, NULL, NULL,
                    DMS_SUPPORT_ALL, ascend_ub_get_dev_id_info)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int ascend_ub_init_urd(void)
{
    if (ubdrv_get_ub_pcie_sel() != UBDRV_UB_SEL) {
        return 0;
    }
    CALL_INIT_MODULE(ASCEND_UB_CMD_NAME);
    return 0;
}

void ascend_ub_uninit_urd(void)
{
    if (ubdrv_get_ub_pcie_sel() != UBDRV_UB_SEL) {
        return;
    }
    CALL_EXIT_MODULE(ASCEND_UB_CMD_NAME);
    return;
}

DECLAER_FEATURE_AUTO_INIT(ascend_ub_init_urd, FEATURE_LOADER_STAGE_5);
DECLAER_FEATURE_AUTO_UNINIT(ascend_ub_uninit_urd, FEATURE_LOADER_STAGE_5);