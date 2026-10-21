/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "devmng_common.h"
#include "dms/dms_misc_interface.h"
#include "devdrv_pcie.h"

#define SERVER_ID_MAX 47

#define VNIC_IPADDR_FIRST_OCTET_DEFAULT 192U
#define VNIC_IPADDR_SECOND_OCTET_DEFAULT 168U

#define DMANAGE_VNIC_IPADDR_CALCULATE(server_id, local_id, dev_id)                      \
    ((0xFF & 192u) | ((0xFF & (server_id)) << 8) | ((0xFF & (2u + (local_id))) << 16) | \
     ((0xFF & (199u - (dev_id))) << 24))

int devdrv_get_vnic_ip(unsigned int dev_id, unsigned int *ip_addr)
{
    unsigned int device_dev_id;
    int ret;
    int64_t server_id = 0;

    if (ip_addr == NULL) {
        DEVDRV_DRV_ERR("invalid input para, ip_addr is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = drvGetDeviceDevIDByHostDevID(dev_id, &device_dev_id);
    if (ret != DRV_ERROR_NONE) {
        DEVDRV_DRV_ERR("Host devid transform to local devid failed. (devid=%u; ret=%d)", dev_id, ret);
        return DRV_ERROR_NO_DEVICE;
    }
#ifndef CFG_FEATURE_VNIC_IP_STATIC
    ret = dms_get_spod_item(device_dev_id, INFO_TYPE_SERVER_ID, &server_id);
    if ((ret != DRV_ERROR_NONE) || (server_id > SERVER_ID_MAX)) {
        *ip_addr = DMANAGE_VNIC_IPADDR_CALCULATE(VNIC_IPADDR_SECOND_OCTET_DEFAULT, device_dev_id, dev_id);
    } else {
        *ip_addr = DMANAGE_VNIC_IPADDR_CALCULATE(server_id, device_dev_id, dev_id);
    }
#else
    *ip_addr = DMANAGE_VNIC_IPADDR_CALCULATE(VNIC_IPADDR_SECOND_OCTET_DEFAULT, device_dev_id, dev_id);
#endif
    return DRV_ERROR_NONE;
}

int devdrv_get_vnic_ip_by_sdid(unsigned int sdid, unsigned int *ip_addr)
{
    int ret;
    unsigned int device_dev_id;
    struct halSDIDParseInfo sdid_parse = {0};

    if (ip_addr == NULL) {
        DEVDRV_DRV_ERR("invalid input para, ip_addr is NULL. (sdid=%u)\n", sdid);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = dms_parse_sdid(sdid, &sdid_parse);
    if (ret != 0) {
        DEVDRV_DRV_ERR_EXTEND(ret, DRV_ERROR_NOT_SUPPORT, "parse sdid failed. (sdid=%u)\n", sdid);
        return ret;
    }

    ret = drvGetDeviceDevIDByHostDevID(sdid_parse.udevid, &device_dev_id);
    if (ret != DRV_ERROR_NONE) {
        DEVDRV_DRV_ERR("Host devid transform to local devid failed. (devid=%u; ret=%d)", sdid_parse.udevid, ret);
        return DRV_ERROR_NO_DEVICE;
    }

    if (sdid_parse.server_id > SERVER_ID_MAX) {
        *ip_addr = DMANAGE_VNIC_IPADDR_CALCULATE(VNIC_IPADDR_SECOND_OCTET_DEFAULT, device_dev_id, sdid_parse.udevid);
    } else {
        *ip_addr = DMANAGE_VNIC_IPADDR_CALCULATE(sdid_parse.server_id, device_dev_id, sdid_parse.udevid);
    }
    return DRV_ERROR_NONE;
}