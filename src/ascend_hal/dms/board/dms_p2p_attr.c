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
#include "ascend_dev_num.h"
#include "dms_device_info.h"
#include "dms_p2p_com.h"

STATIC drvError_t dms_p2p_attr_operate(struct urd_p2p_attr *p2p_attr)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    if ((p2p_attr->dev_id >= ASCEND_DEV_MAX_NUM) || (p2p_attr->peer_dev_id >= ASCEND_DEV_MAX_NUM)) {
        DMS_ERR("Invalid parameter. (dev_id=%u; peer_phy_id=%u; max_dev_num=%u)\n", p2p_attr->dev_id,
                p2p_attr->peer_dev_id, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_DEV_P2P_ATTR, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)p2p_attr, sizeof(struct urd_p2p_attr), (void *)p2p_attr,
                          sizeof(struct urd_p2p_attr));
    ret = urd_usr_cmd_ex(DMS_GET_P2P_ATTR, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "P2p operate ioctl failed. (dev_id=%u; peer_dev=%u; ret=%d)\n", p2p_attr->dev_id,
                              p2p_attr->peer_dev_id, ret);
#ifdef CFG_FEATURE_ERR_CODE_NOT_OPTIMIZATION
        if (p2p_attr->op == DEVDRV_P2P_CAPABILITY_QUERY) {
            return DRV_ERROR_INVALID_VALUE;
        } else {
            return DRV_ERROR_IOCRL_FAIL;
        }
#else
        return ret;
#endif
    }

    if (p2p_attr->op == DEVDRV_P2P_ADD) {
        ret = dms_set_p2p_restore_info(p2p_attr->dev_id, p2p_attr->peer_dev_id, p2p_attr->type, DEVDRV_P2P_ADD);
        if (ret != 0) {
            return ret;
        }
        DMS_EVENT("Enable P2P. (logic_devid=%u; peer_phy_devid=%u)\n", p2p_attr->dev_id, p2p_attr->peer_dev_id);
    } else if (p2p_attr->op == DEVDRV_P2P_DEL) {
        ret = dms_set_p2p_restore_info(p2p_attr->dev_id, p2p_attr->peer_dev_id, p2p_attr->type, DEVDRV_P2P_DEL);
        if (ret != 0) {
            return ret;
        }
        DMS_EVENT("Disable P2P. (logic_devid=%u; peer_phy_devid=%u)\n", p2p_attr->dev_id, p2p_attr->peer_dev_id);
    }

    return DRV_ERROR_NONE;
}

drvError_t DmsGetP2PStatus(unsigned int dev_id, unsigned int peer_dev_id, unsigned int *status)
{
    struct urd_p2p_attr p2p_attr = {0};
    drvError_t ret;

    if (status == NULL) {
        DMS_ERR("The input parameter is NULL. (dev_id=%u; peer_dev_id=%u)\n", dev_id, peer_dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    p2p_attr.op = DEVDRV_P2P_QUERY;
    p2p_attr.dev_id = dev_id;
    p2p_attr.peer_dev_id = peer_dev_id;
    ret = dms_p2p_attr_operate(&p2p_attr);
    if (ret != 0) {
        *status = DRV_P2P_STATUS_DISABLE;
    } else {
        *status = (unsigned int)p2p_attr.status;
    }

    return DRV_ERROR_NONE;
}

drvError_t DmsGetP2PCapbility(unsigned int dev_id, unsigned long long *capbility)
{
    struct urd_p2p_attr p2p_attr = {0};
    int ret;

    if (capbility == NULL) {
        DMS_ERR("The input parameter is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    p2p_attr.op = DEVDRV_P2P_CAPABILITY_QUERY;
    p2p_attr.dev_id = dev_id;
    ret = dms_p2p_attr_operate(&p2p_attr);
    if (ret != 0) {
        return ret;
    }
    *capbility = p2p_attr.capability;
    DMS_DEBUG("Succeeded in obtaining the P2P capability. (capability=0x%llx)\n", *capbility);

    return DRV_ERROR_NONE;
}

drvError_t DmsEnableP2P(unsigned int dev_id, unsigned int peer_dev_id, unsigned int type)
{
    struct urd_p2p_attr p2p_attr = {0};

    p2p_attr.op = DEVDRV_P2P_ADD;
    p2p_attr.dev_id = dev_id;
    p2p_attr.peer_dev_id = peer_dev_id;
    p2p_attr.type = type;
    return dms_p2p_attr_operate(&p2p_attr);
}

drvError_t DmsDisableP2P(unsigned int dev_id, unsigned int peer_dev_id, unsigned int type)
{
    struct urd_p2p_attr p2p_attr = {0};

    p2p_attr.op = DEVDRV_P2P_DEL;
    p2p_attr.dev_id = dev_id;
    p2p_attr.peer_dev_id = peer_dev_id;
    p2p_attr.type = type;
    return dms_p2p_attr_operate(&p2p_attr);
}

drvError_t DmsCanAccessPeer(unsigned int dev_id, unsigned int peer_dev_id, int *can_access_peer)
{
    struct urd_p2p_attr p2p_attr = {0};
    int ret;

    if (can_access_peer == NULL) {
        DMS_ERR("The input parameter is NULL. (dev_id=%u; peer_dev_id=%u)\n", dev_id, peer_dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    p2p_attr.op = DEVDRV_P2P_ACCESS_STATUS_QUERY;
    p2p_attr.dev_id = dev_id;
    p2p_attr.peer_dev_id = peer_dev_id;
    ret = dms_p2p_attr_operate(&p2p_attr);
    if (ret != 0) {
        return ret;
    }

    *can_access_peer = p2p_attr.status;
    DMS_DEBUG("Obtained successfully. (can_access_peer=%u)\n", *can_access_peer);
    return DRV_ERROR_NONE;
}
