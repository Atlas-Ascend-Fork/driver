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

#include "securec.h"
#include "ka_kernel_def_pub.h"
#include "devdrv_common.h"
#include "devdrv_user_common.h"
#include "devdrv_manager_common.h"
#include "ascend_kernel_hal.h"
#include "devdrv_info.h"

int devdrv_manager_get_chip_type(int *chip_type)
{
    *chip_type = CHIP_TYPE_NOT_ASCEND;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_manager_get_chip_type);

int devdrv_fresh_event_code_to_shm(u32 devid, u32 *health_code, u32 health_len, struct shm_event_code *event_code,
                                   u32 event_len)
{
    struct devdrv_info *dev_info = NULL;
    int event_cnt = 0;
    int i;

    dev_info = devdrv_manager_get_devdrv_info(devid);
    if (dev_info == NULL) {
        devdrv_drv_warn("Dev_info is NULL. (device id=%u)\n", devid);
        return -EINVAL;
    } else if (dev_info->shm_status == NULL) {
        devdrv_drv_warn("Shm_status is NULL. (device id=%u)\n", devid);
        return -EINVAL;
    }

    for (i = 0; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        dev_info->shm_status->dms_health_status[i] = (u16)health_code[i];
    }
    for (i = 0; i < DEVMNG_SHM_INFO_EVENT_CODE_LEN; i++) {
        if (event_code[i].event_code == 0) {
            break;
        }
        dev_info->shm_status->event_code[i].event_code = event_code[i].event_code;
        dev_info->shm_status->event_code[i].fid = event_code[i].fid;
        event_cnt++;
    }
    dev_info->shm_status->event_cnt = event_cnt;
    return 0;
}

int devdrv_check_ts_id(int ts_id)
{
    if (ts_id < 0) {
        devdrv_drv_err("ts id %d is invalid, ts id must be greater than 0\n", ts_id);
        return -EINVAL;
    }

    if (ts_id != DEVDRV_TS_AICORE) {
        devdrv_drv_err("ts id %d is invalid, ts id must equal %d ts aicore\n", ts_id, DEVDRV_TS_AICORE);
        return -EINVAL;
    }
    return 0;
}

unsigned int map_ts_grp_oper_errcode[GROUP_OPER_ERROR_MAX] = {
    [GROUP_OPER_SUCCESS] = DRV_ERROR_NONE,
    [GROUP_OPER_COMMON_ERROR] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_INPUT_DATA_NULL] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_GROUP_ID_INVALID] = DRV_ERROR_PARA_ERROR,
    [GROUP_OPER_STATE_ILLEGAL] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_NO_MORE_GROUP_CREATE] = DRV_ERROR_NO_RESOURCES,
    [GROUP_OPER_NO_LESS_GROUP_DELETE] = DRV_ERROR_NOT_EXIST,
    [GROUP_OPER_DEFAULT_GROUP_ALREADY_CREATE] = DRV_ERROR_REPEATED_INIT,
    [GROUP_OPER_NO_MORE_VALID_AICORE_CREATE] = DRV_ERROR_NO_RESOURCES,
    [GROUP_OPER_NO_MORE_VALID_AIVECTOR_CREATE] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_NO_MORE_VALID_SDMA_CREATE] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_NO_MORE_VALID_AICPU_CREATE] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_NO_MORE_VALID_ACTIVE_SQ_CREATE] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_NO_LESS_VALID_AICORE_DELETE] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_NO_LESS_VALID_AIVECTOR_DELETE] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_NO_LESS_VALID_SDMA_DELETE] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_NO_LESS_VALID_AICPU_DELETE] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_NO_LESS_VALID_ACTIVE_SQ_DELETE] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_BUILDIN_GROUP_NOT_CREATE] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_SPECIFY_GROUPID_ALREADY_CREATE] = DRV_ERROR_REPEATED_INIT,
    [GROUP_OPER_SPECIFY_GROUPID_NOT_CREATE] = DRV_ERROR_NOT_EXIST,
    [GROUP_OPER_DISABLE_HWTS_ALLGROUP_FAILED] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_AICORE_POOL_FULL] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_AIVECTOR_POOL_FULL] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_SDMA_POOL_FULL] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_AICPU_POOL_FULL] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_ACTIVE_SQ_POOL_FULL] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_CREATE_NULL_GROUP] = DRV_ERROR_PARA_ERROR,
    [GROUP_OPER_CREATE_NULL_SQ_GROUP] = DRV_ERROR_PARA_ERROR,
    [GROUP_OPER_VM_CONFIG_NOT_INIT] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_VM_CONFIG_FAILD] = DRV_ERROR_INNER_ERR,
    [GROUP_OPER_DELETE_GROUP_STREAM_RUNNING] = DRV_ERROR_BUSY,
    [GROUP_OPER_CREATE_NOT_SUPPORT_IN_DC] = DRV_ERROR_NOT_SUPPORT,
};

void devdrv_map_ts_grp_oper_err_to_user(unsigned int ts_errcode, void *usr_arg)
{
    unsigned int drv_errcode = DRV_ERROR_INNER_ERR;
    int ret;

    if (ts_errcode < GROUP_OPER_ERROR_MAX) {
        drv_errcode = map_ts_grp_oper_errcode[ts_errcode];
    }

    ret = copy_to_user_safe(usr_arg, &drv_errcode, sizeof(drv_errcode));
    if (ret) {
        devdrv_drv_warn("Usr errcode may not be accurate. (ret=%d; ts_errcode=%u; drv_errcode=%u)\n", ret, ts_errcode,
                        drv_errcode);
    }
}

bool devdrv_manager_ts_is_enable(void)
{
    return true;
}

int devdrv_manager_get_soc_version(struct devdrv_manager_hccl_devinfo *info, struct devdrv_info *dev_info)
{
#ifdef CFG_FEATURE_PG
    int ret;
#ifndef CFG_FEATURE_REFACTOR
    ret = strncpy_s(info->soc_version, SOC_VERSION_LENGTH, dev_info->pg_info.spePgInfo.socVersion,
                    SOC_VERSION_LENGTH - 1);
#else
    ret = strncpy_s(info->soc_version, SOC_VERSION_LENGTH, dev_info->soc_version, SOC_VERSION_LEN - 1);
#endif
    if (ret != 0) {
        devdrv_drv_err("Strncpy_s soc_version failed. (ret=%d)\n", ret);
        return ret;
    }
#endif
    return 0;
}