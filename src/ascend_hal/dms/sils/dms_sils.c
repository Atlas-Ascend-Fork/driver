/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/ioctl.h>

#include "ascend_hal.h"
#include "dms_user_common.h"
#include "dsmi_common_interface.h"
#include "dms_sils.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC void dms_fill_sils_input(struct dms_sils_info_in *in, unsigned int dev_id, unsigned int cmd, void *buf,
                                unsigned int size)
{
    in->dev_id = dev_id;
    in->resv[0] = cmd;
    in->buff = buf;
    in->size = size;
}

STATIC int dms_sils_ioctl(unsigned int main_cmd, unsigned int sub_cmd, struct dms_sils_info_in *in,
                          struct dms_sils_info_out *out)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    ioarg.main_cmd = main_cmd;
    ioarg.sub_cmd = sub_cmd;
    ioarg.filter_len = 0;
    ioarg.input = (void *)in;
    ioarg.input_len = (unsigned int)sizeof(struct dms_sils_info_in);
    ioarg.output = (void *)out;
    ioarg.output_len = (unsigned int)sizeof(struct dms_sils_info_out);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_ERR("Sils ioctl failed. (main_cmd=%u; sub_cmd=%u; ret=%d)\n", main_cmd, sub_cmd, ret);
        return errno_to_user_errno(ret);
    }
    return 0;
}

int dms_get_emu_subsys_status(unsigned int dev_id, struct dsmi_emu_subsys_state_stru *emu_subsys_state_data)
{
    int ret;
    struct dms_sils_info_in in = {0};
    struct dms_sils_info_out out = {0};

    dms_fill_sils_input(&in, dev_id, 0, (void *)emu_subsys_state_data,
                        (unsigned int)sizeof(struct dsmi_emu_subsys_state_stru));

    ret = dms_sils_ioctl(DMS_MAIN_CMD_SILS, DMS_SUBCMD_GET_SILS_EMU_INFO, &in, &out);
    if (ret != 0) {
        DMS_ERR("Get emu subsys status failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return 0;
}

int dms_get_sils_status(unsigned int dev_id, struct dsmi_safetyisland_status_stru *safetyisland_status_data)
{
    int ret;
    struct dms_sils_info_in in = {0};
    struct dms_sils_info_out out = {0};

    dms_fill_sils_input(&in, dev_id, 0, (void *)safetyisland_status_data,
                        (unsigned int)sizeof(struct dsmi_safetyisland_status_stru));

    ret = dms_sils_ioctl(DMS_MAIN_CMD_SILS, DMS_SUBCMD_GET_SILS_HEALTH_STATUS, &in, &out);
    if (ret != 0) {
        DMS_ERR("Get sils status failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return 0;
}

int dms_set_sils_info(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size)
{
    int ret;
    struct dms_sils_info_in in = {0};
    struct dms_sils_info_out out = {0};

    dms_fill_sils_input(&in, dev_id, sub_cmd, buf, size);

    ret = dms_sils_ioctl(DMS_MAIN_CMD_SILS, DMS_SUBCMD_SET_SILS_DEVICE_INFO, &in, &out);
    if (ret != 0) {
        DMS_ERR("Set sils info failed. (ret=%d; sub_cmd=%u)\n", ret, sub_cmd);
        return ret;
    }

    return 0;
}

int dms_get_sils_info(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    int ret;
    struct dms_sils_info_in in = {0};
    struct dms_sils_info_out out = {0};

    if ((buf == NULL) || (size == NULL)) {
        DMS_ERR("Invalid parameter. (buf_is_null=%d; size_is_null=%d)\n", (buf == NULL), (size == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    dms_fill_sils_input(&in, dev_id, sub_cmd, buf, *size);
    ret = dms_sils_ioctl(DMS_MAIN_CMD_SILS, DMS_SUBCMD_GET_SILS_DEVICE_INFO, &in, &out);
    if (ret != 0) {
        DMS_ERR("Get sils info failed. (ret=%d; sub_cmd=%u)\n", ret, sub_cmd);
        return ret;
    }

    *size = out.size;
    (void)vfid;
    return 0;
}

int dms_equipment_set_sils_info(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size)
{
    int ret;
    struct dms_sils_info_in in = {0};
    struct dms_sils_info_out out = {0};

    dms_fill_sils_input(&in, dev_id, sub_cmd, buf, size);

    ret = dms_sils_ioctl(DMS_MAIN_CMD_SILS, DMS_SUBCMD_SET_SILS_HARDWARE_TEST, &in, &out);
    if (ret != 0) {
        DMS_ERR("Set equipment sils info failed. (ret=%d; sub_cmd=%u)\n", ret, sub_cmd);
        return ret;
    }

    return 0;
}

int dms_equipment_get_sils_info(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    int ret;
    struct dms_sils_info_in in = {0};
    struct dms_sils_info_out out = {0};

    if ((buf == NULL) || (size == NULL)) {
        DMS_ERR("Invalid parameter. (buf_is_null=%d; size_is_null=%d)\n", (buf == NULL), (size == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    dms_fill_sils_input(&in, dev_id, sub_cmd, buf, *size);

    ret = dms_sils_ioctl(DMS_MAIN_CMD_SILS, DMS_SUBCMD_GET_SILS_HARDWARE_TEST, &in, &out);
    if (ret != 0) {
        DMS_ERR("Get equipment sils info failed. (ret=%d; sub_cmd=%u)\n", ret, sub_cmd);
        return ret;
    }

    *size = out.size;
    return 0;
}