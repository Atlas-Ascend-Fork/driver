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

#include "pbl/pbl_uda.h"
#include "pbl/pbl_feature_loader.h"

#include "devdrv_manager_common.h"
#include "comm_kernel_interface.h"
#include "dms_define.h"

#define DMS_HOST_NOTIFIER_LOW_PRI "dms_host_low_pri"
#define DMS_HOST_NOTIFIER_HIGH_PRI "dms_host_high_pri"
STATIC int dms_host_high_pri_notifier_init(u32 udevid)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}, {0}};
    int out_len = 0;
    int ret;

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_HOST_SET_REMOTE_UDEVID;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_VALID;
    dev_manager_msg_info.header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;
    dev_manager_msg_info.header.dev_id = udevid;
    *(u32 *)dev_manager_msg_info.payload = udevid;

    ret = devdrv_common_msg_send(udevid, (void *)&dev_manager_msg_info, sizeof(struct devdrv_manager_msg_info),
                                 sizeof(struct devdrv_manager_msg_info), (u32 *)&out_len,
                                 DEVDRV_COMMON_MSG_DEVDRV_MANAGER);
    if (ret != 0) {
        dms_err("Send msg failed. (dev_id=%u; ret=%d)\n", udevid, ret);
        return -EAGAIN;
    }
    if (out_len != sizeof(struct devdrv_manager_msg_head)) {
        dms_err("Send msg out_len invalid. (dev_id=%u; out_len=%d)\n", udevid, out_len);
        return -EAGAIN;
    }
    if (dev_manager_msg_info.header.result != 0) {
        dms_err("Send msg header result failed. (dev_id=%u; result=%u)\n", udevid, dev_manager_msg_info.header.result);
        return -EAGAIN;
    }
    return 0;
}

STATIC int dms_host_low_pri_notifier_init(u32 udevid)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}, {0}};
    int out_len = 0;
    int ret;

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_HOST_NOTIFY_READY;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_VALID;
    dev_manager_msg_info.header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;
    dev_manager_msg_info.header.dev_id = udevid;
    *(u32 *)dev_manager_msg_info.payload = udevid;

    ret = devdrv_common_msg_send(udevid, (void *)&dev_manager_msg_info, sizeof(struct devdrv_manager_msg_info),
                                 sizeof(struct devdrv_manager_msg_info), (u32 *)&out_len,
                                 DEVDRV_COMMON_MSG_DEVDRV_MANAGER);
    if (ret != 0) {
        dms_err("Send msg failed. (dev_id=%u; ret=%d)\n", udevid, ret);
        return -EAGAIN;
    }
    if (out_len != sizeof(struct devdrv_manager_msg_head)) {
        dms_err("Send msg out_len invalid. (dev_id=%u; out_len=%d)\n", udevid, out_len);
        return -EAGAIN;
    }
    if (dev_manager_msg_info.header.result != 0) {
        dms_err("Send msg header result failed. (dev_id=%u; result=%u)\n", udevid, dev_manager_msg_info.header.result);
        return -EAGAIN;
    }
    return 0;
}

STATIC int dms_host_high_pri_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (!uda_udevid_is_reorder()) {
        return 0;
    }

    if (action == UDA_INIT) {
        ret = dms_host_high_pri_notifier_init(udevid);
    }

    dms_info("Dms high priority notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

STATIC int dms_host_low_pri_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = dms_host_low_pri_notifier_init(udevid);
    }

    dms_info("Dms low priority notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

STATIC int dms_host_notify_ready_init(void)
{
    int ret;
    struct uda_dev_type type;

    dms_info("dms host notify ready info to device init.\n");
    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(DMS_HOST_NOTIFIER_HIGH_PRI, &type, UDA_PRI1, dms_host_high_pri_notifier_func);
    if (ret != 0) {
        dms_err("uda_notifier_register failed, ret=%d\n", ret);
        return ret;
    }

    ret = uda_notifier_register(DMS_HOST_NOTIFIER_LOW_PRI, &type, UDA_PRI4, dms_host_low_pri_notifier_func);
    if (ret != 0) {
        dms_err("uda_notifier_register failed, ret=%d\n", ret);
        return ret;
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_host_notify_ready_init, FEATURE_LOADER_STAGE_5);

STATIC void dms_host_notify_ready_uninit(void)
{
    struct uda_dev_type type;

    dms_info("dms host notify ready info to device exit.\n");
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(DMS_HOST_NOTIFIER_LOW_PRI, &type);
    (void)uda_notifier_unregister(DMS_HOST_NOTIFIER_HIGH_PRI, &type);
}
DECLAER_FEATURE_AUTO_UNINIT(dms_host_notify_ready_uninit, FEATURE_LOADER_STAGE_5);
