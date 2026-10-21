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
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_compiler_pub.h"
#include "ka_fs_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_kernel_def_pub.h"

#include "dp_proc_mng_interface.h"
#include "dp_proc_mng_ioctl.h"
#include "dp_proc_mng_log.h"
#include "dp_proc_mng_register_ops.h"
#include "dp_proc_mng_proc_info.h"
#include "dpa/dpa_dp_proc_mng_pid_maps.h"

STATIC int dp_proc_mng_open(ka_inode_t *inode, ka_file_t *file)
{
    return 0;
}

STATIC int dp_proc_mng_release(ka_inode_t *inode, ka_file_t *filp)
{
    return 0;
}

int dp_proc_mng_convert_id_from_vir_to_phy(struct dp_proc_mng_ioctl_arg *buffer)
{
    int ret;
    u32 logic_id = DP_PROC_MNG_INVALID_DEVICE_PHYID;
    u32 vfid = DP_PROC_MNG_INVALID_DEVICE_PHYID;
    u32 phyid = DP_PROC_MNG_INVALID_DEVICE_PHYID;

    logic_id = buffer->head.logical_devid;
    if (logic_id >= DP_PROC_MNG_MAX_DEVICE_NUM) {
        dp_proc_mng_drv_err("Logic_id is invalid. (logic_id=%d)\n", logic_id);
        return -ENODEV;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(logic_id, &phyid, &vfid);
    if ((ret != 0) || (phyid >= DP_PROC_MNG_MAX_DEVICE_NUM) || (vfid >= DP_PROC_MNG_MAX_VF_NUM)) {
        dp_proc_mng_drv_err("Convert devid failed. (ret=%d; devid=%u; phyid=%u; vfid=%u)\n", ret, logic_id, phyid,
                            vfid);
        return -EINVAL;
    }

    buffer->head.logical_devid = logic_id;
    buffer->head.devid = phyid;
    buffer->head.vfid = vfid;
    return 0;
}

STATIC int dp_proc_mng_dispatch_ioctl(ka_file_t *file, u32 cmd, struct dp_proc_mng_ioctl_arg *buffer)
{
    int ret;
    u32 cmd_id = _KA_IOC_NR(cmd);
    if (dp_proc_mng_ioctl_handlers[cmd_id] == NULL) {
        dp_proc_mng_drv_err("Cmd is not supported. (cmd=0x%x; cmd_id=0x%x)\n", cmd, cmd_id);
        return -EOPNOTSUPP;
    }

    ret = dp_proc_mng_convert_id_from_vir_to_phy(buffer);
    if (ret != 0) {
        return ret;
    }

    ret = dp_proc_mng_ioctl_handlers[_KA_IOC_NR(cmd)](file, buffer);

    return ret;
}

STATIC long dp_proc_mng_ioctl(ka_file_t *file, u32 cmd, unsigned long arg)
{
    struct dp_proc_mng_ioctl_arg buffer = {{0}};
    u32 cmd_id = _KA_IOC_NR(cmd);
    int ret;

    if ((file == NULL) || (arg == 0)) {
        dp_proc_mng_drv_err("File is NULL, check dp_proc_mng init. (cmd=0x%x; _KA_IOC_NR(cmd)=0x%x)\n", cmd,
                            _KA_IOC_NR(cmd));
        return -EINVAL;
    }

    if ((_KA_IOC_TYPE(cmd) != DP_PROC_MNG_MAGIC) || (cmd_id >= DP_PROC_MNG_CMD_MAX_CMD)) {
        dp_proc_mng_drv_err("Cmd is not supported. (cmd=0x%x; cmd_id=0x%x)\n", cmd, cmd_id);
        return -EINVAL;
    }

    if ((_KA_IOC_DIR(cmd) & _KA_IOC_WRITE) != 0) {
        if (ka_base_copy_from_user(&buffer, (void __ka_user *)(uintptr_t)arg, sizeof(struct dp_proc_mng_ioctl_arg)) !=
            0) {
            dp_proc_mng_drv_err("Copy_from_user fail. (cmd=0x%x; cmd_id=0x%x)\n", cmd, cmd_id);
            return -EINVAL;
        }
    }

    ret = dp_proc_mng_dispatch_ioctl(file, cmd, &buffer);
    if (ret != 0) {
        return ret;
    }

    if ((_KA_IOC_DIR(cmd) & _KA_IOC_READ) != 0) {
        if (ka_base_copy_to_user((void __ka_user *)(uintptr_t)arg, &buffer, sizeof(struct dp_proc_mng_ioctl_arg)) !=
            0) {
            dp_proc_mng_drv_err("Copy_to_user fail. (cmd=0x%x; cmd_id=0x%x)\n", cmd, cmd_id);
            return -EINVAL;
        }
    }

    return 0;
}

STATIC ka_file_operations_t dp_proc_mng_fops = {
    .owner = KA_THIS_MODULE,
    .open = dp_proc_mng_open,
    .release = dp_proc_mng_release,
    .unlocked_ioctl = dp_proc_mng_ioctl,
};

int dp_proc_mng_init(void)
{
    int ret;

    dp_proc_mng_drv_info("Data plane proc mng module init.\n");

    ret = dp_proc_mng_info_init();
    if (ret != 0) {
        return ret;
    }

    ret = dp_proc_mng_create_work();
    if (ret != 0) {
        goto create_work_fail;
    }

    ret = dp_proc_mng_davinci_module_init(&dp_proc_mng_fops);
    if (ret != 0) {
        goto register_davinci_fail;
    }

    ret = dp_proc_mng_register_ops_init();
    if (ret != 0) {
        goto register_ops_fail;
    }

    dp_pid_maps_init();

    dp_proc_mng_drv_info("Data plane proc mng module init success.\n");
    return 0;

register_ops_fail:
    dp_proc_mng_davinci_module_uninit();
register_davinci_fail:
    dp_proc_mng_destroy_work();
create_work_fail:
    dp_proc_mng_info_unint();

    return ret;
}

void dp_proc_mng_exit(void)
{
    dp_proc_mng_unregister_ops_init();
    dp_proc_mng_davinci_module_uninit();
    dp_proc_mng_destroy_work();
    dp_proc_mng_info_unint();
}
