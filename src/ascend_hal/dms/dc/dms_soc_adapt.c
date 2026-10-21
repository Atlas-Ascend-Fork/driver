/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifdef CFG_FEATURE_BIST
#include "dms_user_common.h"
#include "dms/dms_soc_interface.h"
#include "devmng_user_common.h"
#include "dms/dms_devdrv_info_comm.h"
#include "dms_device_info.h"

drvError_t DmsSetBistInfoMultiCmd(unsigned int dev_id, unsigned int cmd, unsigned char *in_buf, unsigned int buf_len)
{
    int ret;
    struct urd_cmd urd_cmd_data = {0};
    struct urd_cmd_para urd_cmd_para_data = {0};
    struct dms_set_bist_info_multi_cmd_in bist_info;

#ifdef CFG_FEATURE_MBIST_VOLT_FREQ_INIT
    urd_usr_cmd_fill(&urd_cmd_data, DMS_SET_BIST_INFO_CMD, DMS_BIST_CMD_SET_FREQVOLT, NULL, 0);
    urd_usr_cmd_para_fill(&urd_cmd_para_data, NULL, 0, NULL, 0);
    ret = urd_dev_usr_cmd(dev_id, &urd_cmd_data, &urd_cmd_para_data);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Set mbist volt freq init failed. (ret=%d; dev_id=%u)\n", ret, dev_id);
        return ret;
    }
#endif

    bist_info.sub_cmd = cmd;
    bist_info.dev_id = dev_id;
    bist_info.buff = in_buf;
    bist_info.buff_size = buf_len;

    urd_usr_cmd_fill(&urd_cmd_data, DMS_SET_BIST_INFO_CMD, ZERO_CMD, NULL, 0);
    urd_usr_cmd_para_fill(&urd_cmd_para_data, (void *)&bist_info, sizeof(struct dms_set_bist_info_multi_cmd_in), NULL,
                          0);
    ret = urd_dev_usr_cmd(dev_id, &urd_cmd_data, &urd_cmd_para_data);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (ret=%d; dev_id=%u)\n", ret, dev_id);
        return ret;
    }

    DMS_INFO("Set bist info success. (dev_id=0x%x, cmd=0x%x)\n", dev_id, cmd);
    return DRV_ERROR_NONE;
}
#endif