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

#include "ka_task_pub.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_runenv_config.h"
#include "hdcdrv_core.h"
#ifdef CFG_FEATURE_PFSTAT
#include "hdcdrv_pfstat.h"
#endif
#include "hdcdrv_host.h"
#include "hdcdrv_host_adapt.h"

bool hdcdrv_is_phy_dev(u32 devid)
{
    return uda_is_phy_dev(devid);
}

u32 hdcdrv_get_chip_type(u32 devid)
{
    return uda_get_chip_type(devid);
}

int hdcdrv_container_vir_to_phs_devid(u32 virtual_devid, u32 *physical_devid, u32 *vfid)
{
    return devdrv_manager_container_logical_id_to_physical_id(virtual_devid, physical_devid, vfid);
}

int hdcdrv_check_in_container(void)
{
    return (int)run_in_normal_docker();
}

u32 hdcdrv_get_container_id(void)
{
    u32 docker_id = HDCDRV_PHY_HOST_ID;
    int ret;
#ifndef CFG_FEATURE_NOT_SUPPORT_UDA
    if (hdcdrv_check_in_container() != 1) {
        return docker_id;
    }

    ret = uda_get_cur_ns_id(&docker_id);
#else
    if (devdrv_manager_container_is_in_container() != 1) {
        return docker_id;
    }
    ret = devdrv_manager_container_get_docker_id(&docker_id);
#endif
    if (ret != 0) {
        docker_id = HDCDRV_DOCKER_MAX_NUM;
    }

    return docker_id;
}

void hdcdrv_init_dev(struct hdcdrv_dev *hdc_dev)
{
    struct hdcdrv_ctrl_msg msg = {0};
    u32 len = 0;
    int ret;

    /* wait for device init */
    msg.type = HDCDRV_CTRL_MSG_TYPE_SYNC;
    msg.sync_msg.segment = hdcdrv_get_packet_segment();
    msg.sync_msg.peer_dev_id = (int)hdc_dev->dev_id;
    msg.error_code = HDCDRV_OK;

    hdcdrv_init_host_phy_mach_flag(hdc_dev);

    ret = (int)hdcdrv_ctrl_msg_send(hdc_dev->dev_id, (void *)&msg, (u32)sizeof(msg), (u32)sizeof(msg), &len);
    if ((ret != HDCDRV_OK) || (len != sizeof(msg)) || (msg.error_code != HDCDRV_OK)) {
#ifndef DRV_UT
        hdcdrv_info_limit("Wait for device startup. (dev_id=%d;ret=%d;code=%d;msg_len=%d;sizeof_msg=%ld)\n",
                          hdc_dev->dev_id, ret, msg.error_code, len, sizeof(msg));
        ka_task_schedule_delayed_work(&hdc_dev->init, 1 * KA_HZ);
        return;
#endif
    }

    hdcdrv_set_peer_dev_id((int)hdc_dev->dev_id, msg.sync_msg.peer_dev_id);

    /* init msg channel */
    ret = hdcdrv_init_msg_chan(hdc_dev->dev_id);
    if (ret != HDCDRV_OK) {
        hdcdrv_err("Calling hdcdrv_init_msg_chan failed. (dev_id=%d)\n", hdc_dev->dev_id);
#ifndef DRV_UT
        return;
#endif
    }

    ret = hdcdrv_register_common_msg();
    if (ret != 0) {
        hdcdrv_err("Calling devdrv_register_common_msg_client failed.\n");
        hdcdrv_uninit_msg_chan(hdc_dev);
#ifndef DRV_UT
        return;
#endif
    }

    if (hdc_dev->msg_chan_cnt > 0) {
        hdcdrv_set_device_status((int)hdc_dev->dev_id, HDCDRV_VALID);
    }

    (void)devdrv_set_module_init_finish((int)hdc_dev->dev_id, DEVDRV_HOST_MODULE_HDC);

    if (hdcdrv_get_running_status() == HDCDRV_RUNNING_RESUME) {
        hdcdrv_set_running_status(HDCDRV_RUNNING_NORMAL);
    }
    ka_task_up(&hdc_dev->hdc_instance_sem);

    hdcdrv_info("Device enable work. (dev_id=%u;peer_dev_id=%d;msg_chan_count=%d;normal_chan_cnt=%u)\n",
                hdc_dev->dev_id, hdc_dev->peer_dev_id, hdc_dev->msg_chan_cnt, hdc_dev->normal_chan_num);
    return;
}

#define HDCDRV_HOST_NOTIFIER "hdc_host"
STATIC int hdcdrv_host_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= HDCDRV_SUPPORT_MAX_DEV) {
        hdcdrv_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = hdcdrv_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = hdcdrv_uninit_instance(udevid);
    } else if (action == UDA_SUSPEND) {
        ret = hdcdrv_suspend(udevid);
    } else if (action == UDA_RESUME) {
        ret = hdcdrv_resume(udevid, uda_get_device(udevid));
    } else {
#ifndef DRV_UT
        return 0;
#endif
    }

    hdcdrv_info("notifier action. (udevid=%u;action=%d;ret=%d)\n", udevid, (u32)action, ret);

    return ret;
}

int hdcdrv_pcie_init_module(void)
{
    struct uda_dev_type type;
    int ret;

    hdcdrv_module_param_init();
    hdcdrv_init_hotreset_param();

    ret = hdcdrv_init();
    if (ret != 0) {
        hdcdrv_err("Calling hdcdrv_init failed.\n");
        return ret;
    }

    hdcdrv_set_segment(hdcdrv_get_packet_segment());

    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(HDCDRV_HOST_NOTIFIER, &type, UDA_PRI1, hdcdrv_host_notifier_func);
    if (ret != HDCDRV_OK) {
        hdcdrv_uninit();
        hdcdrv_err("Calling uda_notifier_register failed.\n");
        return ret;
    }

#ifdef CFG_FEATURE_VFIO
    ret = vhdch_init();
    if (ret != HDCDRV_OK) {
        (void)uda_notifier_unregister(HDCDRV_HOST_NOTIFIER, &type);
        hdcdrv_uninit();
        hdcdrv_err("Calling vhdch_vmngh_init failed. (ret=%d)\n", ret);
        return ret;
    }
#endif

#ifdef CFG_FEATURE_PFSTAT
    (void)hdcdrv_pfstat_init();
#endif

#ifdef CFG_FEATURE_HDC_REG_MEM
    if (!ka_system_try_module_get(KA_THIS_MODULE)) {
        hdcdrv_warn("can not get module.\n");
    }
#endif
    return HDCDRV_OK;
}

void hdcdrv_pcie_exit_module(void)
{
    struct uda_dev_type type;
#ifdef CFG_FEATURE_PFSTAT
    hdcdrv_pfstat_exit();
#endif
    uda_davinci_near_real_entity_type_pack(&type);

#ifdef CFG_FEATURE_VFIO
    vhdch_uninit();
#endif
    (void)hdcdrv_unregister_common_msg_client(0);
    (void)uda_notifier_unregister(HDCDRV_HOST_NOTIFIER, &type);
    hdcdrv_uninit_hotreset_param();
    hdcdrv_uninit();
}