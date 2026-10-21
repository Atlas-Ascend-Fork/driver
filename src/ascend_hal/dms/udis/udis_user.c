/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "securec.h"
#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "dms/dms_devdrv_info_comm.h"
#include "dms_user_common.h"
#include "pbl_urd_sub_cmd_common.h"
#include "udis_user.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC int udis_is_supported(unsigned int dev_id)
{
    int ret;
    halChipInfo chip_info = {{0}, {0}, {0}};
    const char *chip_type = "Ascend";

    ret = halGetChipInfo(dev_id, &chip_info);
    if (ret != 0) {
        DMS_ERR("Get chip info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (strncmp((const char *)chip_info.type, chip_type, strlen(chip_type)) != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

STATIC int udis_input_check(unsigned int dev_id, const struct udis_dev_info *info)
{
    size_t name_len = 0;

    if (info == NULL) {
        DMS_ERR("Invalid para. info is NULL (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (info->module_type >= UDIS_MODULE_MAX) {
        DMS_INFO("Invalid module type. (dev_id=%u; module_type=%d; max_module_type=%u)\n", dev_id, info->module_type,
                 UDIS_MODULE_MAX - 1);
        return DRV_ERROR_PARA_ERROR;
    }

    name_len = strnlen(info->name, UDIS_MAX_NAME_LEN);
    if ((name_len == 0) || (name_len >= UDIS_MAX_NAME_LEN)) {
        DMS_ERR("Invalid para, name length invalid. (dev_id=%u; module_type=%u; name_len=%u; max_name_len=%u)\n",
                dev_id, info->module_type, name_len, UDIS_MAX_NAME_LEN - 1);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

int udis_get_device_info(unsigned int dev_id, struct udis_dev_info *info)
{
    int ret = 0;
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct udis_get_ioctl_in in = {0};
    struct udis_get_ioctl_out out = {0};

    ret = udis_is_supported(dev_id);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Udis support check failed. (dev_id=%u; ret=%d)", dev_id, ret);
        return ret;
    }

    ret = udis_input_check(dev_id, info);
    if (ret != 0) {
        return ret;
    }

    in.module_type = info->module_type;
    ret = strcpy_s(in.name, UDIS_MAX_NAME_LEN, info->name);
    if (ret != 0) {
        DMS_ERR("Failed to copy info name, (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }
    in.data = info->data;
    in.in_len = sizeof(info->data);

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_UDIS_DEVICE_INFO, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&in, sizeof(struct udis_get_ioctl_in), &out,
                          sizeof(struct udis_get_ioctl_out));
    ret = urd_dev_usr_cmd(dev_id, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get info from udis failed. (dev_id=%u; module_type=%u; name=%s; ret=%d)", dev_id,
                              info->module_type, info->name, ret);
        return ret;
    }

    info->data_len = out.out_len;
    info->acc_ctrl = out.acc_ctrl;
    info->last_update_time = out.last_update_time;

    return 0;
}

int halUdisGetDeviceInfo(unsigned int dev_id, void *info)
{
    return udis_get_device_info(dev_id, (struct udis_dev_info *)info);
}

int udis_set_device_info(unsigned int dev_id, const struct udis_dev_info *info)
{
    int ret = 0;
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct udis_set_ioctl_in in = {0};

    ret = udis_is_supported(dev_id);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Udis support check failed. (dev_id=%u; ret=%d)", dev_id, ret);
        return ret;
    }

    ret = udis_input_check(dev_id, info);
    if (ret != 0) {
        return ret;
    }

    if ((info->data_len == 0) || (info->data_len > UDIS_MAX_DATA_LEN)) {
        DMS_ERR("Invalid data len. (dev_id=%u; module_type=%u; name=%s; data_len=%u; max_data_len=%u)\n", dev_id,
                info->module_type, info->name, info->data_len, UDIS_MAX_DATA_LEN);
        return DRV_ERROR_PARA_ERROR;
    }

    in.module_type = info->module_type;
    ret = strcpy_s(in.name, UDIS_MAX_NAME_LEN, info->name);
    if (ret != 0) {
        DMS_ERR("Failed to copy info name, (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }
    in.data = info->data;
    in.data_len = info->data_len;
    in.acc_ctrl = info->acc_ctrl;

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_SET_UDIS_DEVICE_INFO, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&in, sizeof(struct udis_set_ioctl_in), NULL, 0);
    ret = urd_dev_usr_cmd(dev_id, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Set info from udis failed. (dev_id=%u; module_type=%u; name=%s; ret=%d)", dev_id,
                              info->module_type, info->name, ret);
        return ret;
    }

    DMS_INFO("Set udis info success. (dev_id=%u; module_type=%u; name=%s)\n", dev_id, info->module_type, info->name);

    return 0;
}