/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <unistd.h>
#include <dlfcn.h>

#include "devmng_common.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devdrv_ioctl.h"
#include "devmng_user_common.h"
#include "dms_device_info.h"
#include "ascend_dev_num.h"

#ifndef CFG_EDGE_HOST
#include "i2cdev_drv.h"
#endif

#define FLASH_BLOCK_SIZE 65536
#define DDR_CHAN_BITMAP 0x3

#define OPTICAL_PRESENT 1
#ifdef CFG_FEATURE_SCAN_XSFP
int dmanage_get_optical_module_temp_adapt(unsigned int dev_id, int *value)
{
#ifndef DRV_HOST
    int ret;
    unsigned int present;
    struct xsfp_dev_info xsfp_info = {0};

    if (dev_id >= ASCEND_DEV_MAX_NUM || value == NULL) {
        DEVDRV_DRV_ERR("invalid devid(%u) or value is NULL.\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = xsfp_get_present(dev_id, 0, &present);
    if (ret != DRV_ERROR_NONE) {
        DEVDRV_DRV_ERR("xsfg_get_present failed. devid(%u), ret = %d\n", dev_id, ret);
        goto out;
    }

    // if optical not present, log will omitted in case of flush windows
    if (present != OPTICAL_PRESENT) {
        ret = DRV_ERROR_NOT_EXIST;
        goto out;
    }

    ret = xsfp_get_info(dev_id, 0, &xsfp_info);
    if (ret != DRV_ERROR_NONE) {
        DEVDRV_DRV_ERR("xsfp_get_info failed. devid(%u), ret = %d\n", dev_id, ret);
        goto out;
    }

    *value = xsfp_info.temp;

out:
    return ret;
#else
    (void)dev_id;
    (void)value;
    DEVDRV_DRV_ERR("dmanage_get_optical_module_temp_adapt does not support on host side !\n");
    return DRV_ERROR_INVALID_HANDLE;
#endif
}
#endif

int devdrv_get_slot_id(unsigned int dev_id, unsigned int *slot_id)
{
    struct ioctl_arg arg;
    int ret;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (slot_id == NULL) {
        DEVDRV_DRV_ERR("slot_id is NULL. devid(%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }
    *slot_id = 0;

    arg.dev_id = dev_id;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_SLOT_ID, (void *)&arg);
    if (ret) {
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    *slot_id = arg.data1;
    return DRV_ERROR_NONE;
}

void devdrv_p2p_mem_sync_to_flash(void)
{
    return;
}

int dmanage_get_lp_status(int device_id, struct dsmi_lp_status_stru *lp_status_data)
{
    (void)device_id;
    (void)lp_status_data;
    return DRV_ERROR_NOT_SUPPORT;
}

int dmanage_get_ts_group_num(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                             unsigned int *ret_size)
{
    (void)dev_id;
    (void)vfid;
    (void)sub_cmd;
    (void)out_buf;
    (void)ret_size;
    return DRV_ERROR_NOT_SUPPORT;
}

int dmanage_get_temperature(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                            unsigned int *size)
{
    (void)dev_id;
    (void)vfid;
    (void)sub_cmd;
    (void)out_buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}

int dmanage_set_temperature(unsigned int dev_id, unsigned int sub_cmd, void *out_buf, unsigned int ret_size)
{
    (void)dev_id;
    (void)sub_cmd;
    (void)out_buf;
    (void)ret_size;
    return DRV_ERROR_NOT_SUPPORT;
}

int dmanage_get_capability_group_info(int dev_id, int ts_id, int group_id, struct capability_group_info *group_info,
                                      int group_count)
{
    (void)dev_id;
    (void)ts_id;
    (void)group_info;
    (void)group_count;
    (void)group_id;
    return DRV_ERROR_NOT_SUPPORT;
}

int dmanage_delete_capability_group(int dev_id, int ts_id, int group_id)
{
    (void)dev_id;
    (void)ts_id;
    (void)group_id;
    return DRV_ERROR_NOT_SUPPORT;
}

int dmanage_create_capability_group(int dev_id, int ts_id, struct capability_group_info *group_info)
{
    (void)dev_id;
    (void)ts_id;
    (void)group_info;
    return DRV_ERROR_NOT_SUPPORT;
}

int dmanage_restart_ssh(const char *ip_addr)
{
    (void)ip_addr;
    return DRV_ERROR_NONE;
}

#ifdef CFG_FEATURE_SOC_INFO
int dmanage_get_soc_info(uint32_t dev_id, uint32_t vfid, uint32_t sub_cmd, void *out_buf, unsigned int *ret_size)
{
    int ret;

    if (out_buf == NULL || ret_size == NULL) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; vfid=%u; buf_is_null=%d; size_is_null=%d)\n", dev_id, vfid,
                       (out_buf == NULL), (ret_size == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    switch (sub_cmd) {
        case DSMI_SOC_INFO_SUB_CMD_DOMAIN_INFO:
            return DRV_ERROR_NOT_SUPPORT;

        case DSMI_SOC_INFO_SUB_CMD_CUST_OP_ENHANCE:
            ret = DmsHalGetDeviceInfoEx(dev_id, MODULE_TYPE_SYSTEM, INFO_TYPE_CUST_OP_ENHANCE, out_buf, ret_size);
            if (ret != 0) {
                DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get cust op enhance. (dev_id=%u; ret=%d)\n", dev_id, ret);
            }
            break;

        default:
            DEVDRV_DRV_ERR("Invalid sub cmd. (dev_id=%u; sub_cmd=%u)\n", dev_id, sub_cmd);
            return DRV_ERROR_PARA_ERROR;
    }

    return ret;
}
#endif