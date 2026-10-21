/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifdef CFG_FEATURE_EMMC_INFO

#include <limits.h>
#include <sys/ioctl.h>
#include "securec.h"
#include "dms_user_common.h"
#include "dms/dms_misc_interface.h"
#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devdrv_user_common.h"
#include "dsmi_common_interface.h"
#include "dms_emmc.h"
#include "ascend_dev_num.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define EMMC_MANUFACTORY_INFO_LEN 512

STATIC int dms_get_emmc_standard_info(unsigned int dev_id, unsigned int vfid, unsigned int subcmd, void *buf,
                                      unsigned int *size)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    struct dsmi_emmc_standard_info_stru emmc_info = {0};
    struct devdrv_emmc_info_para ioarg_in = {0};

    (void)vfid;

    if (*size < sizeof(struct dsmi_emmc_standard_info_stru)) {
        DMS_ERR("Get emmc info failed. input size err. (size=%u; min=%lu)\n", *size,
                sizeof(struct dsmi_emmc_standard_info_stru));
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg_in.dev_id = dev_id;
    ioarg_in.info_type = subcmd;

    ioarg.main_cmd = DMS_MAIN_CMD_EMMC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_EMMC_INFO;
    ioarg.filter_len = 0;
    ioarg.input = &ioarg_in;
    ioarg.input_len = sizeof(struct devdrv_emmc_info_para);
    ioarg.output = (void *)(&emmc_info);
    ioarg.output_len = sizeof(struct dsmi_emmc_standard_info_stru);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_ERR("Get emmc info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = memcpy_s(buf, *size, &emmc_info, sizeof(struct dsmi_emmc_standard_info_stru));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed, (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    *size = sizeof(struct dsmi_emmc_standard_info_stru);
    return DRV_ERROR_NONE;
}

STATIC int DmsGetEmmcManufacturerInfo(unsigned int dev_id, unsigned int vfid, unsigned int subcmd, void *buf,
                                      unsigned int *size)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    char emmc_info[EMMC_MANUFACTORY_INFO_LEN] = {0};

    (void)vfid;

    struct devdrv_emmc_info_para ioarg_in = {0};

    if (*size < EMMC_MANUFACTORY_INFO_LEN) {
        DMS_ERR("Get emmc info failed. input size err. (size=%u; min=%u)\n", *size, EMMC_MANUFACTORY_INFO_LEN);
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg_in.dev_id = dev_id;
    ioarg_in.info_type = subcmd;
    ioarg.main_cmd = DMS_MAIN_CMD_EMMC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_EMMC_INFO;
    ioarg.filter_len = 0;
    ioarg.input = &ioarg_in;
    ioarg.input_len = sizeof(struct devdrv_emmc_info_para);
    ioarg.output = (void *)(emmc_info);
    ioarg.output_len = EMMC_MANUFACTORY_INFO_LEN;

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_ERR("Get emmc info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = memcpy_s(buf, *size, emmc_info, EMMC_MANUFACTORY_INFO_LEN);
    if (ret != 0) {
        DMS_ERR("memcpy_s failed, (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    *size = EMMC_MANUFACTORY_INFO_LEN;
    return DRV_ERROR_NONE;
}

int dms_get_emmc_info(unsigned int dev_id, unsigned int vfid, unsigned int subcmd, void *buf, unsigned int *size)
{
    int ret;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (buf == NULL) || size == NULL) {
        DMS_ERR("Invalid dev_id or buf is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }
    DMS_EVENT("Get emmc info subcmd = %d\n", subcmd);

    switch (subcmd) {
        case DSMI_EMMC_SUB_CMD_STANDARD_INFO:
            ret = dms_get_emmc_standard_info(dev_id, vfid, subcmd, buf, size);
            break;
        case DSMI_EMMC_SUB_CMD_MANUFACTURER_INFO:
            ret = DmsGetEmmcManufacturerInfo(dev_id, vfid, subcmd, buf, size);
            break;
        default:
            DMS_ERR("Invalid subcmd. (subcmd=%u)\n", subcmd);
            return DRV_ERROR_PARA_ERROR;
    }
    DMS_DEBUG("Get emmc info success.\n");
    return ret;
}
#else
#include "ascend_hal_error.h"
#include "dms_emmc.h"

int dms_get_emmc_info(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)dev_id;
    (void)vfid;
    (void)sub_cmd;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}
#endif