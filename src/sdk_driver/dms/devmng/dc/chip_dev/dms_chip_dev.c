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

#include "ascend_hal_error.h"
#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_runenv_config.h"
#include "dms/dms_cmd_def.h"
#include "devdrv_common.h"
#include "devdrv_manager.h"
#include "devdrv_user_common.h"
#include "dms_chip_dev_map.h"
#include "ka_memory_pub.h"
#include "dms_chip_dev.h"

BEGIN_DMS_MODULE_DECLARATION(DMS_CHIP_DEV_CMD_NAME)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_CHIP_DEV_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_COUNT, NULL, NULL,
                    DMS_SUPPORT_ALL_USER, dms_ioctl_get_chip_count)
ADD_FEATURE_COMMAND(DMS_CHIP_DEV_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_LIST, NULL, NULL,
                    DMS_SUPPORT_ALL_USER, dms_ioctl_get_chip_list)
ADD_FEATURE_COMMAND(DMS_CHIP_DEV_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEVICE_FROM_CHIP, NULL, NULL,
                    DMS_SUPPORT_ALL_USER, dms_ioctl_get_device_from_chip)
ADD_FEATURE_COMMAND(DMS_CHIP_DEV_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_FROM_DEVICE, NULL, NULL,
                    DMS_SUPPORT_ALL_USER, dms_ioctl_get_chip_from_device)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_ioctl_get_chip_count(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    unsigned int count = 0;
#ifdef CFG_HOST_ENV
    unsigned int udevid = 0;
#endif
    (void)in;
    (void)in_len;

    if ((out == NULL) || (out_len != sizeof(int))) {
        dms_err("Input argument is null, or out_len is wrong. (out=%s; out_len=%u)\n",
                ((out == NULL) ? "NULL" : "not NULL"), out_len);
        return -EINVAL;
    }

#ifdef CFG_HOST_ENV
    ret = uda_devid_to_udevid(0, &udevid);
    if (ret != 0) {
        dms_err("Transfer logical id to udevid id failed. (ret=%d)\n", ret);
        return -EPERM;
    }

    if (!uda_is_pf_dev(udevid) || run_in_normal_docker()) {
        count = devdrv_manager_get_devnum();
        *(int *)out = (int)count;
        return 0;
    }
#endif

    ret = dms_get_chip_count(&count);
    if (ret != 0) {
        dms_err("Get chip count failed. (ret=%d).\n", ret);
        return ret;
    }

    *(int *)out = (int)count;
    return 0;
}

int dms_ioctl_get_chip_list(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    struct devdrv_chip_list chip_list = {0};
#ifdef CFG_HOST_ENV
    unsigned int i;
    unsigned int udevid = 0;
#endif
    (void)in;
    (void)in_len;

    if ((out == NULL) || (out_len != sizeof(struct devdrv_chip_list))) {
        dms_err("Input argument is null, or out_len is wrong. (out=%s; out_len=%u)\n",
                ((out == NULL) ? "NULL" : "Not_NULL"), out_len);
        return -EINVAL;
    }

#ifdef CFG_HOST_ENV
    ret = uda_devid_to_udevid(0, &udevid);
    if (ret != 0) {
        dms_err("Transfer logical id to udevid id failed. (ret=%d)\n", ret);
        return -EPERM;
    }

    if (!uda_is_pf_dev(udevid) || run_in_normal_docker()) {
        chip_list.count = devdrv_manager_get_devnum();
        /*
         * devdrv_manager_get_devnum's max return is UDA_DEV_MAX_NUM here.
         * host: UDA_DEV_MAX_NUM = 100; VDAVINCI_VDEV_OFFSET = 100;
         * the VDAVINCI_VDEV_OFFSET must be equal to UDA_DEV_MAX_NUM
         **/
        if (chip_list.count > VDAVINCI_VDEV_OFFSET) {
            dms_err("Invalid device number. (devnum=%u; max=%u)\n", chip_list.count, VDAVINCI_VDEV_OFFSET);
            return -EINVAL;
        }
        for (i = 0; i < chip_list.count; i++) {
            chip_list.chip_list[i] = i;
        }
        *(struct devdrv_chip_list *)out = chip_list;
        return 0;
    }
#endif

    ret = dms_get_chip_list(&chip_list);
    if (ret != 0) {
        dms_err("Get chip list failed. (ret=%d)\n", ret);
        return ret;
    }

    *(struct devdrv_chip_list *)out = chip_list;
    return 0;
}

int dms_ioctl_get_device_from_chip(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    struct devdrv_chip_dev_list *chip_dev_list = NULL;
#ifdef CFG_HOST_ENV
    unsigned int udevid = 0;
#endif

    if ((in == NULL) || (in_len != sizeof(int)) || (out == NULL) || (out_len != sizeof(struct devdrv_chip_dev_list))) {
        dms_err("Input argument is null, or len is wrong. (in=%s; out=%s; in_len=%u, out_len=%u)\n",
                ((in == NULL) ? "NULL" : "Not_NULL"), ((out == NULL) ? "NULL" : "Not_NULL"), in_len, out_len);
        return -EINVAL;
    }

    chip_dev_list = (struct devdrv_chip_dev_list *)ka_mm_kzalloc(sizeof(struct devdrv_chip_dev_list),
                                                                 KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (chip_dev_list == NULL) {
        dms_err("Allocate memory for chip device list failed.\n");
        return -ENOMEM;
    }
    chip_dev_list->chip_id = *(int *)in;

#ifdef CFG_HOST_ENV
    ret = uda_devid_to_udevid(0, &udevid);
    if (ret != 0) {
        dms_err("Transfer logical id to udevid failed. (ret=%d)\n", ret);
        goto FREE_CHIP_DEV_LIST;
    }

    if (!uda_is_pf_dev(udevid) || run_in_normal_docker()) {
        if ((chip_dev_list->chip_id < devdrv_manager_get_devnum()) && (chip_dev_list->chip_id < DEVDRV_MAX_CHIP_NUM) &&
            (chip_dev_list->chip_id >= 0)) {
            chip_dev_list->count = 1;
            chip_dev_list->dev_list[0] = chip_dev_list->chip_id;
            ret = memcpy_s(out, out_len, chip_dev_list, sizeof(struct devdrv_chip_dev_list));
            if (ret != 0) {
                dms_err("Call memcpys_s failed. (ret=%d)\n", ret);
            }
        } else {
            dms_err("Chip id is invalid. (chip_id=%d)\n", chip_dev_list->chip_id);
            ret = -EINVAL;
        }
        goto FREE_CHIP_DEV_LIST;
    }
#endif

    ret = dms_get_device_from_chip(chip_dev_list);
    if (ret != 0) {
        dms_err("Get device list from chip id failed. (ret=%d)\n", ret);
        goto FREE_CHIP_DEV_LIST;
    }

    ret = memcpy_s(out, out_len, chip_dev_list, sizeof(struct devdrv_chip_dev_list));
    if (ret != 0) {
        dms_err("Call memcpys_s failed. (ret=%d)\n", ret);
    }
FREE_CHIP_DEV_LIST:
    ka_mm_kfree(chip_dev_list);
    chip_dev_list = NULL;
    return ret;
}

int dms_ioctl_get_chip_from_device(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    struct devdrv_get_dev_chip_id chip_from_dev = {0};
#ifdef CFG_HOST_ENV
    unsigned int udevid = 0;
#endif

    if ((in == NULL) || (in_len != sizeof(struct devdrv_get_dev_chip_id)) || (out == NULL) ||
        (out_len != sizeof(struct devdrv_get_dev_chip_id))) {
        dms_err("Input argument is null, or in_len is wrong. (in_len=%u; out_len=%u; in=%s; out=%s)\n", in_len, out_len,
                ((in == NULL) ? "NULL" : "Not_NULL"), ((out == NULL) ? "NULL" : "Not_NULL"));
        return -EINVAL;
    }
    chip_from_dev.dev_id = ((struct devdrv_get_dev_chip_id *)in)->dev_id;

#ifdef CFG_HOST_ENV
    ret = uda_devid_to_udevid(0, &udevid);
    if (ret != 0) {
        dms_err("Transfer logical id to udevid failed. (ret=%d)\n", ret);
        return -EPERM;
    }

    if (!uda_is_pf_dev(udevid) || run_in_normal_docker()) {
        if ((chip_from_dev.dev_id < devdrv_manager_get_devnum()) && (chip_from_dev.dev_id < DEVDRV_MAX_CHIP_NUM)) {
            chip_from_dev.chip_id = chip_from_dev.dev_id;
            *(struct devdrv_get_dev_chip_id *)out = chip_from_dev;
            return 0;
        } else {
            dms_err("Device id is invalid. (dev_id=%u)\n", chip_from_dev.dev_id);
            return -EINVAL;
        }
    }
#endif

    ret = dms_get_chip_from_device(&chip_from_dev);
    if (ret != 0) {
        dms_err("Get chip from device failed. (ret=%d)\n", ret);
        return ret;
    }

    *(struct devdrv_get_dev_chip_id *)out = chip_from_dev;
    return 0;
}

int dms_chip_dev_init(void)
{
    dms_info("dms chip_dev init urd node.\n");
    CALL_INIT_MODULE(DMS_CHIP_DEV_CMD_NAME);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_chip_dev_init, FEATURE_LOADER_STAGE_5);

void dms_chip_dev_uninit(void)
{
    dms_info("dms chip_dev uninit urd node.\n");
    CALL_EXIT_MODULE(DMS_CHIP_DEV_CMD_NAME);
}
DECLAER_FEATURE_AUTO_UNINIT(dms_chip_dev_uninit, FEATURE_LOADER_STAGE_5);
