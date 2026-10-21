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
#include "securec.h"
#include "ascend_hal.h"
#include "dms/dms_misc_interface.h"
#include "dms_user_common.h"
#include "dmc_user_interface.h"
#ifdef CFG_FEATURE_CC_INFO
#include "ascend_dev_num.h"
#endif

/* CC: confidential computing */

drvError_t dms_set_cc_info(unsigned int device_id, void *buf, unsigned int buf_size)
{
#ifdef CFG_FEATURE_CC_INFO
    int ret;
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct dms_cc_mode mode = {0};

    if (device_id >= ASCEND_DEV_MAX_NUM || buf == NULL || buf_size != sizeof(struct dms_cc_mode)) {
        DMS_ERR("Parameter is invalid. (device_id=%u; info_is_null=%d; buf_size=%u)\n", device_id, (buf == NULL),
                buf_size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = memcpy_s(&mode, sizeof(struct dms_cc_mode), buf, buf_size);
    if (ret != 0) {
        DMS_ERR("Memcpy_s failed, (ret=%d, device_id=%u).\n", ret, device_id);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_SET_CC_INFO, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&mode, sizeof(struct dms_cc_mode), NULL, 0);
    ret = urd_dev_usr_cmd(device_id, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (ret=%d; device_id=%u)\n", ret, device_id);
        return ret;
    }

    DMS_EVENT("Set cc mode success. (device_id=%u; cc_mode=%u; crypto_mode=%u)\n", device_id, mode.cc_mode,
              mode.crypto_mode);
    return DRV_ERROR_NONE;
#else
    (void)device_id;
    (void)buf;
    (void)buf_size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t dms_get_cc_info(unsigned int device_id, void *buf, unsigned int *buf_size)
{
#ifdef CFG_FEATURE_CC_INFO
    int ret;
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct dms_cc_info cc_info = {0};

    if (device_id >= ASCEND_DEV_MAX_NUM || buf == NULL || buf_size == NULL) {
        DMS_ERR("Parameter is invalid. (device_id=%u; info_is_null=%d; buf_size_is_null=%d)\n", device_id,
                (buf == NULL), (buf_size == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    if (*buf_size != sizeof(struct dms_cc_info)) {
        DMS_ERR("The value of buf_size is invalid. (buf_size=%u; size=%u)\n", *buf_size, sizeof(struct dms_cc_info));
        return DRV_ERROR_PARA_ERROR;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CC_INFO, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&device_id, sizeof(unsigned int), (void *)&cc_info,
                          sizeof(struct dms_cc_info));
    ret = urd_dev_usr_cmd(device_id, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (ret=%d; device_id=%u)\n", ret, device_id);
        return ret;
    }

    ret = memcpy_s(buf, *buf_size, &cc_info, sizeof(struct dms_cc_info));
    if (ret != 0) {
        DMS_ERR("Memcpy_s failed, (ret=%d, device_id=%u).\n", ret, device_id);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    DMS_DEBUG("Get cc mode success. (device_id=%u; cc_running=%u; crypto_running=%u; cc_cfg=%u; crypto_cfg=%u)\n",
              device_id, cc_info.cc_running_info.cc_mode, cc_info.cc_running_info.crypto_mode,
              cc_info.cc_cfg_info.cc_mode, cc_info.cc_cfg_info.crypto_mode);
    return DRV_ERROR_NONE;
#else
    (void)device_id;
    (void)buf;
    (void)buf_size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}
