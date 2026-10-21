/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "devdrv_ipc_common.h"
#include "devmng_user_common.h"
#include "ascend_dev_num.h"

int ipc_msg_request(unsigned int dev_id, unsigned char target_id, unsigned char main_cmd, unsigned char sub_cmd,
                    struct ipc_msg *msg)
{
    struct ioctl_ipc ipc_data;
    int ret;

    if (dev_id >= ASCEND_DEV_MAX_NUM || msg == NULL) {
        DEVDRV_DRV_ERR("invalid parameter devid(%u).\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ipc_data.dev_id = dev_id;
    ipc_data.target_id = target_id;
    ipc_data.main_cmd = main_cmd;
    ipc_data.sub_cmd = sub_cmd;

    ret = memcpy_s(&ipc_data.msg, sizeof(struct ipc_msg), msg, sizeof(struct ipc_msg));
    if (ret != 0) {
        DEVDRV_DRV_ERR("ipc_msg_request memcpy_s failed, ret(%d).\n", ret);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_IPC_UNIFY_MSG, (void *)&ipc_data);
    if (ret != 0) {
        DEVDRV_DRV_ERR("dmanage_common_ioctl failed. ret(%d) devid(%u).\n", ret, dev_id);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return 0;
}
