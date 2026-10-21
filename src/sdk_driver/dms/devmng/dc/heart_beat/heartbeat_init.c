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
#include "ka_dfx_pub.h"

#include "pbl/pbl_uda.h"
#include "pbl/pbl_feature_loader.h"
#include "dms/dms_notifier.h"
#include "devdrv_common.h"
#include "dms_define.h"
#include "heart_beat.h"

#define HEART_BEAT_NOTIFIER "heart_beat"
void heart_beat_stub(void)
{
    return;
}

#ifndef DMS_UT
int heart_beat_device_up(u32 udevid)
{
    int ret = 0;
#ifdef CFG_FEATURE_HEART_BEAT
#ifdef CFG_HOST_ENV /* host side */
    ret = heartbeat_dev_register(udevid);
    if (ret != 0) {
        dms_err("heartbeat dev register failed. (dev_id=%u; ret=%d) \n", udevid, ret);
        return ret;
    }

    ret = heart_beat_register_urgent_timer(udevid);
    if (ret != 0) {
        heartbeat_dev_unregister();
        dms_err("register urgent timer failed. (dev_id=%u; ret=%d) \n", udevid, ret);
        return ret;
    }
#endif
    heart_beat_write_status_init(udevid);
    ret = heart_beat_read_item_init(udevid);
    if (ret != 0) {
#ifdef CFG_HOST_ENV /* host side */
        heartbeat_dev_unregister();
        (void)heart_beat_unregister_urgent_timer(udevid);
#endif
        dms_err("Heartbeat init failed. (device id=%u)\n", udevid);
    }
#endif

    return ret;
}

int heart_beat_device_down(u32 udevid)
{
    heart_beat_read_item_uninit(udevid);
    heart_beat_write_status_uninit(udevid);
#ifdef CFG_HOST_ENV /* host side */
    heart_beat_unregister_urgent_timer(udevid);
#endif

    return 0;
}

int heart_beat_device_suspend(void *data)
{
#if (defined CFG_FEATURE_HEART_BEAT) && (defined CFG_HOST_ENV) /* host side */
    struct devdrv_info *dev = NULL;
    dev = (struct devdrv_info *)data;
    heart_beat_read_item_uninit(dev->dev_id);
#endif

    return 0;
}

int heart_beat_device_resume(void *data)
{
#if (defined CFG_FEATURE_HEART_BEAT) && (defined CFG_HOST_ENV) /* host side */
    struct devdrv_info *dev = NULL;
    dev = (struct devdrv_info *)data;
    (void)heart_beat_read_item_init(dev->dev_id);
#endif

    return 0;
}

STATIC int heart_beat_notifier(ka_notifier_block_t *nb, unsigned long mode, void *data)
{
    int ret = 0;

    if (data == NULL) {
        dms_err("Data is null, invalid parameter. \n");
        return -EINVAL;
    }
    switch (mode) {
        case DMS_DEVICE_RESUME:
            ret = heart_beat_device_resume(data);
            break;
        case DMS_DEVICE_SUSPEND:
            ret = heart_beat_device_suspend(data);
            break;
        default:
            break;
    }

    return 0;
}

STATIC ka_notifier_block_t g_heart_beat_notifier = {
    .notifier_call = heart_beat_notifier,
};

STATIC int heart_beat_uda_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= (u32)ASCEND_DEV_MAX_NUM) {
        dms_warn("Device id invalid. (udev_id=%u; valid range=[0, %u)).\n", udevid, ASCEND_DEV_MAX_NUM);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = heart_beat_device_up(udevid);
    } else if (action == UDA_UNINIT) {
        ret = heart_beat_device_down(udevid);
    }

    dms_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

#ifndef CFG_HOST_ENV
int __attribute__((weak)) heart_beat_registr_panic_notifier(void)
{
    return 0;
}

void __attribute__((weak)) heart_beat_registr_panic_unnotifier(void)
{
    return;
}
#endif

STATIC int dms_heartbeat_init(void)
{
    int ret;
    struct uda_dev_type type = {0};

    ret = dms_register_notifier(&g_heart_beat_notifier);
    if (ret != 0) {
        dms_err("register dms notifier failed. (ret=%d)\n", ret);
        goto DMS_REGISTER_FAIL;
    }

#ifdef CFG_HOST_ENV
    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(HEART_BEAT_NOTIFIER, &type, UDA_PRI2, heart_beat_uda_notifier_func);
    if (ret) {
        dms_err("Host register near real entity type failed. (ret=%d)\n", ret);
        goto UDA_REGISTER_FAIL;
    }
#else
    ret = heart_beat_registr_panic_notifier();
    if (ret != 0) {
        dms_err("Register panic notifier failed. (ret=%d)\n", ret);
        goto PANIC_REGISTERF_FAIL;
    }

    uda_davinci_local_real_agent_type_pack(&type);
    ret = uda_real_virtual_notifier_register(HEART_BEAT_NOTIFIER, &type, UDA_PRI2, heart_beat_uda_notifier_func);
    if (ret) {
        dms_err("Device register local real virtual type failed. (ret=%d)\n", ret);
        goto UDA_REGISTER_FAIL;
    }
#endif
#ifdef CFG_FEATURE_HEART_BEAT
    ret = heart_beat_write_timer_init();
    if (ret != 0) {
        dms_err("Heart beat write timer init failed. (ret=%d)\n", ret);
        goto WRITE_TIMER_REGISTER_FAIL;
    }
    ret = heart_beat_read_timer_init();
    if (ret != 0) {
        dms_err("Heart beat read timer init failed. (ret=%d)\n", ret);
        goto READ_TIMER_REGISTER_FAIL;
    }
#endif

    dms_info("soft event driver init success.\n");
    return 0;
#ifdef CFG_FEATURE_HEART_BEAT
READ_TIMER_REGISTER_FAIL:
    heart_beat_write_timer_exit();
WRITE_TIMER_REGISTER_FAIL:
#endif
#ifdef CFG_HOST_ENV
    uda_davinci_near_real_entity_type_pack(&type);
    uda_notifier_unregister(HEART_BEAT_NOTIFIER, &type);
#else
    uda_davinci_local_real_agent_type_pack(&type);
    uda_real_virtual_notifier_unregister(HEART_BEAT_NOTIFIER, &type);
#endif
UDA_REGISTER_FAIL:
#ifndef CFG_HOST_ENV
    heart_beat_registr_panic_unnotifier();
PANIC_REGISTERF_FAIL:
#endif
    (void)dms_unregister_notifier(&g_heart_beat_notifier);
DMS_REGISTER_FAIL:
    return ret;
}
DECLAER_FEATURE_AUTO_INIT(dms_heartbeat_init, FEATURE_LOADER_STAGE_6);

STATIC void dms_heartbeat_uninit(void)
{
    u32 i;
    struct uda_dev_type type = {0};

#ifdef CFG_FEATURE_HEART_BEAT
    dms_info("heart_beat_read_timer_exit.\n");
    heart_beat_read_timer_exit();
    dms_info("heart_beat_write_timer_exit.\n");
    heart_beat_write_timer_exit();
#endif

#ifdef CFG_HOST_ENV
    dms_info("uda_notifier_unregister.\n");
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(HEART_BEAT_NOTIFIER, &type);
#else
    (void)i;
    uda_davinci_local_real_agent_type_pack(&type);
    (void)uda_real_virtual_notifier_unregister(HEART_BEAT_NOTIFIER, &type);
    heart_beat_registr_panic_unnotifier();
#endif

#ifdef CFG_HOST_ENV /* host side */
    dms_info("heartbeat_dev_unregister.\n");
    heartbeat_dev_unregister();
    dms_info("heart_beat_unregister_urgent_timer.\n");
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        (void)heart_beat_unregister_urgent_timer(i);
    }
#endif
    (void)dms_unregister_notifier(&g_heart_beat_notifier);
}
DECLAER_FEATURE_AUTO_UNINIT(dms_heartbeat_uninit, FEATURE_LOADER_STAGE_6);
#endif
