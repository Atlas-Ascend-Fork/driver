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

#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_runenv_config.h"
#include "pbl/pbl_urd_main_cmd_def.h"
#include "pbl/pbl_urd_sub_cmd_def.h"

#include "urd_acc_ctrl.h"
#include "ascend_dev_num.h"

#include "msg_chan_main.h"

int ascend_msg_chan_init_urd(void);
void ascend_msg_chan_uninit_urd(void);
STATIC int devdrv_get_device_boot_status_urd(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    u32 phys_id, boot_status;
    int ret;

    if ((in == NULL) || (in_len != sizeof(u32))) {
        devdrv_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    if ((out == NULL) || (out_len < sizeof(u32))) {
        devdrv_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }
    phys_id = *(u32 *)in;

    if (phys_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_err("phys_id %d is invalid\n", phys_id);
        return -EINVAL;
    }

    /* only judge the physical id is available in container */
    if (run_in_normal_docker()) {
        if (!uda_task_can_access_udevid_inherit(ka_task_get_current(), phys_id)) {
#ifndef DRV_UT
            devdrv_err("device phyid %u does not belong to current docker\n", phys_id);
#endif
            return -EFAULT;
        }
    }

    ret = devdrv_get_device_boot_status(phys_id, &boot_status);
    if (ret != 0) {
#ifndef EMU_ST
        devdrv_warn("cannot get device boot status, ret(%d), dev_id(%u).\n", ret, phys_id);
        return ret;
#endif
    }

    ret = memcpy_s((void *)out, out_len, (void *)&boot_status, sizeof(u32));
    if (ret != 0) {
#ifndef EMU_ST
        devdrv_err("Call memcpy_s failed. (ret=%d)\n", ret);
        return -EINVAL;
#endif
    }
    devdrv_debug("read boot status success.(devid=%u, status=%u)\n", phys_id, boot_status);

    return 0;
}

STATIC int devdrv_get_p2p_attr_urd(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct urd_p2p_attr *p2p_attr = NULL;
    struct devdrv_base_comm_p2p_attr base_attr;
    u32 phys_id = 0;
    int ret;

    if ((in == NULL) || (in_len != sizeof(struct urd_p2p_attr))) {
        devdrv_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    if ((out == NULL) || (out_len < sizeof(struct urd_p2p_attr))) {
        devdrv_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    p2p_attr = (struct urd_p2p_attr *)in;

    if (uda_devid_to_udevid(p2p_attr->dev_id, &phys_id) != 0) {
        devdrv_err("Can't transform virt id. (devid=%u) \n", p2p_attr->dev_id);
        return -EFAULT;
    }

    base_attr.devid = phys_id;
    base_attr.peer_dev_id = p2p_attr->peer_dev_id;
    base_attr.pid = ka_task_get_current_tgid();
    base_attr.status = &p2p_attr->status;
    base_attr.capability = &p2p_attr->capability;
    base_attr.op = p2p_attr->op;
    base_attr.type = p2p_attr->type;

    ret = devdrv_p2p_attr_op(&base_attr);
    if (ret == -EOPNOTSUPP) {
        return ret;
    } else if (ret != 0) {
        devdrv_err("p2p_attr_op fail. (devid=%u; ret=%d)\n", phys_id, ret);
        return ret;
    }

    ret = memcpy_s((void *)out, out_len, p2p_attr, sizeof(struct urd_p2p_attr));
    if (ret) {
#ifndef EMU_ST
        devdrv_err("copy_to_user_safe failed.\n");
        return -EINVAL;
#endif
    }

    devdrv_debug("P2P attr. (op=%u; devid=%u; peerid=%u; pid=%d; status=%u; cap=%llx)\n", p2p_attr->op,
                 p2p_attr->dev_id, p2p_attr->peer_dev_id, ka_task_get_current_tgid(), p2p_attr->status,
                 p2p_attr->capability);

    return ret;
}

STATIC int devdrv_get_device_info_urd(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct urd_vender_id_info device_id_info = {0};
    struct devdrv_base_device_info device_info = {0};
    unsigned int dev_id, logic_devid, vfid;
    int ret;

    if ((in == NULL) || (in_len != sizeof(u32))) {
        devdrv_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    if ((out == NULL) || (out_len < sizeof(struct urd_vender_id_info))) {
        devdrv_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }
    logic_devid = *(u32 *)in;

    ret = uda_devid_to_phy_devid(logic_devid, &dev_id, &vfid);
    if (ret != 0) {
        devdrv_err("can't transform logic_devid %u, ret=%d\n", logic_devid, ret);
        return ret;
    }

    ret = devdrv_get_device_info(dev_id, &device_info);
    if (ret == -EOPNOTSUPP) {
        return ret;
    } else if (ret != 0) {
        devdrv_err("devdrv_get_device_info failed.\n");
        return ret;
    }

    device_id_info.venderid = device_info.venderid;
    device_id_info.subvenderid = device_info.subvenderid;
    device_id_info.deviceid = device_info.deviceid;
    device_id_info.subdeviceid = device_info.subdeviceid;
    device_id_info.bus = device_info.bus;
    device_id_info.device = device_info.device;
    device_id_info.fn = device_info.fn;
    device_id_info.domain = device_info.domain;
    ret = memcpy_s((void *)out, out_len, &device_id_info, sizeof(struct urd_vender_id_info));
    if (ret) {
        devdrv_err("copy_to_user_safe failed.\n");
#ifndef EMU_ST
        return -EINVAL;
#endif
    }

    devdrv_debug("Dev info. (vender=%u; subv=%u; devid=%u; subd=%d; bus=%#x; dev=%#x; fn=%#x)\n",
                 device_id_info.venderid, device_id_info.subvenderid, device_id_info.deviceid,
                 device_id_info.subdeviceid, device_id_info.bus, device_id_info.device, device_id_info.fn);

    return 0;
}

int init_module_ASCEND_MSG_CHAN_CMD_NAME(void);
int exit_module_ASCEND_MSG_CHAN_CMD_NAME(void);
#define ASCEND_MSG_CHAN_CMD_NAME "ASCEND_MSG_CHAN"
BEGIN_DMS_MODULE_DECLARATION(ASCEND_MSG_CHAN_CMD_NAME)
// host only config
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(ASCEND_MSG_CHAN_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_BOOT_STATUS, NULL, NULL,
                    DMS_SUPPORT_ALL, devdrv_get_device_boot_status_urd)
ADD_FEATURE_COMMAND(ASCEND_MSG_CHAN_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_PROBE_LIST, NULL, NULL,
                    DMS_ACC_ALL | DMS_ENV_NOT_NORMAL_DOCKER, devdrv_get_device_probe_list_urd)
ADD_FEATURE_COMMAND(ASCEND_MSG_CHAN_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_DEV_P2P_ATTR, NULL, NULL,
                    DMS_ACC_ALL | DMS_ENV_ALL, devdrv_get_p2p_attr_urd)
ADD_FEATURE_COMMAND(ASCEND_MSG_CHAN_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_PCIE_INFO, NULL, NULL, DMS_SUPPORT_ALL,
                    devdrv_get_device_info_urd)
ADD_FEATURE_COMMAND(ASCEND_MSG_CHAN_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_PROBE_NUM, NULL, NULL,
                    DMS_SUPPORT_ALL, devdrv_get_device_probe_num_urd)
ADD_FEATURE_COMMAND(ASCEND_MSG_CHAN_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_ALL_DEV_LIST, NULL, NULL,
                    DMS_SUPPORT_ALL, devdrv_get_device_probe_list_urd)

ADD_FEATURE_COMMAND(ASCEND_MSG_CHAN_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CONNECT_TYPE, NULL, NULL,
                    DMS_SUPPORT_ALL_USER, devdrv_get_connect_type_urd)
ADD_FEATURE_COMMAND(ASCEND_MSG_CHAN_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_TOKEN_VAL, NULL, NULL, DMS_SUPPORT_ALL,
                    devdrv_get_token_val_urd)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int ascend_msg_chan_init_urd(void)
{
    CALL_INIT_MODULE(ASCEND_MSG_CHAN_CMD_NAME);
    devdrv_info("Register urd feature finish.\n");
    return 0;
}

void ascend_msg_chan_uninit_urd(void)
{
    CALL_EXIT_MODULE(ASCEND_MSG_CHAN_CMD_NAME);
    devdrv_info("Unregister urd feature finish.\n");
    return;
}
DECLAER_FEATURE_AUTO_INIT(ascend_msg_chan_init_urd, FEATURE_LOADER_STAGE_5);
DECLAER_FEATURE_AUTO_UNINIT(ascend_msg_chan_uninit_urd, FEATURE_LOADER_STAGE_5);
