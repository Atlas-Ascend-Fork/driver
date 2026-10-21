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
#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_dev_identity.h"
#include "pbl/pbl_uda.h"
#include "dms_template.h"
#include "dms/dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#include "dms_chip_info.h"

#define DMS_MODULE_CHIP_INFO "dms_module_chip_info"
INIT_MODULE_FUNC(DMS_MODULE_CHIP_INFO);
EXIT_MODULE_FUNC(DMS_MODULE_CHIP_INFO);

STATIC int dms_feature_get_chip_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int virt_id;
    devdrv_query_chip_info_t chip_info = {0};

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input data is NULL or input data length is wrong. (in_len=%u; correct_in_len=%lu)\n", in_len,
                sizeof(unsigned int));
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(devdrv_query_chip_info_t))) {
        dms_err("Output data is NULL or output data length is wrong. (out_len=%u; correct_out_len=%lu)\n", out_len,
                sizeof(devdrv_query_chip_info_t));
        return -EINVAL;
    }

    virt_id = *(unsigned int *)in;
    ret = dms_get_chip_info(virt_id, &chip_info);
    if (ret != 0) {
        dms_err("Get chip info failed. (virt_id=%u; ret=%d)\n", virt_id, ret);
        return ret;
    }

    *(devdrv_query_chip_info_t *)out = chip_info;
    return 0;
}

STATIC int dms_get_chip_type(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    unsigned int phy_id;
    unsigned int chip_type;

    if ((in == NULL) || (in_len != sizeof(u32))) {
        dms_err("input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(u32))) {
        dms_err("output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    phy_id = *(unsigned int *)in;
    chip_type = uda_get_chip_type(phy_id);
    if (chip_type == HISI_CHIP_UNKNOWN) {
        dms_err("Unknown chip type. (phy_id=%u)\n", phy_id);
        return -EINVAL;
    }
    if (chip_type >= HISI_CHIP_NUM) {
        dms_err("Invalid chip type. (phy_id=%u; chip_type=%u)\n", phy_id, chip_type);
        return -ENODEV;
    }
    *(u32 *)out = chip_type;
    dms_debug("The chip type is obtained successfully. (phy_id=%u; chip_type=%u)\n", phy_id, chip_type);

    return 0;
}

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_CHIP_INFO)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_CHIP_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_INFO, NULL, NULL,
                    DMS_SUPPORT_ALL_USER, dms_feature_get_chip_info)
ADD_FEATURE_COMMAND(DMS_MODULE_CHIP_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_TYPE, NULL, NULL,
#ifdef CFG_HOST_ENV
                    DMS_ACC_ALL | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_ALL,
#else
                    DMS_ACC_ALL | DMS_ENV_ALL | DMS_VDEV_ALL,
#endif
                    dms_get_chip_type)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

STATIC int dms_chip_info_init(void)
{
    CALL_INIT_MODULE(DMS_MODULE_CHIP_INFO);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_chip_info_init, FEATURE_LOADER_STAGE_5);

STATIC void dms_chip_info_uninit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_CHIP_INFO);
}
DECLAER_FEATURE_AUTO_UNINIT(dms_chip_info_uninit, FEATURE_LOADER_STAGE_5);