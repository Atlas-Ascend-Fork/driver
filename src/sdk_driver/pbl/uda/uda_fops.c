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
#include "ka_common_pub.h"
#include "ka_fs_pub.h"
#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_compiler_pub.h"

#include "pbl/pbl_davinci_api.h"

#include "uda_cmd.h"
#include "uda_dev.h"
#include "uda_notifier.h"
#include "uda_proc_fs.h"
#include "uda_pub_def.h"
#include "uda_fops.h"
#include "spod_info.h"

static int ioctl_uda_get_user_info(unsigned int cmd, unsigned long arg)
{
    struct uda_user_info info;

    info.admin_flag = uda_cur_is_admin() ? 1 : 0;
    info.local_flag = UDA_LOCAL_FLAG;
    info.max_dev_num = UDA_MAX_PHY_DEV_NUM;
    info.max_udev_num = UDA_UDEV_MAX_NUM;
    if (uda_is_support_udev_mng()) {
        info.support_udev_mng = 1;
    } else {
        info.support_udev_mng = uda_cur_is_admin() ? 1 : 0;
    }

    return (int)ka_base_copy_to_user((struct uda_user_info __ka_user *)arg, &info, sizeof(info));
}

static int ioctl_uda_setup_dev_table(unsigned int cmd, unsigned long arg)
{
    struct uda_setup_table para;
    int ret;

    ret = (int)ka_base_copy_from_user(&para, (struct uda_setup_table __ka_user *)arg, sizeof(para));
    if (ret != 0) {
        uda_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    return uda_setup_ns_node(para.dev_num);
}

#ifndef EMU_ST
static int check_uda_dev_list_para(struct uda_dev_list *para)
{
    if ((para->start_devid > UDA_UDEV_MAX_NUM) || (para->end_devid > UDA_UDEV_MAX_NUM) ||
        (para->start_devid >= para->end_devid)) {
        uda_err("Invalid parameter. (start_devid=%u, end_devid=%u)\n", para->start_devid, para->end_devid);
        return -EINVAL;
    }

    return 0;
}
#endif

static int ioctl_uda_get_dev_list(unsigned int cmd, unsigned long arg)
{
    struct uda_dev_list *usr_arg = (struct uda_dev_list __ka_user *)arg;
    struct uda_dev_list para;
    u32 devid;
    int ret;
    bool is_admin = false;
    bool is_first_read = true;

    ret = (int)ka_base_copy_from_user(&para, usr_arg, sizeof(para));
    if (ret != 0) {
        uda_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

#ifndef EMU_ST
    ret = check_uda_dev_list_para(&para);
    if (ret != 0) {
        uda_err("Invalid parameter. (ret=%d)\n", ret);
        return -EINVAL;
    }
#endif

    for (devid = para.start_devid; devid < para.end_devid; devid++) {
        struct uda_dev_inst *dev_inst = NULL;
        struct uda_logic_dev logic_dev;
        u32 udevid;

        if (devid >= UDA_MIA_UDEV_OFFSET) {
            if (is_first_read) {
                is_admin = uda_cur_is_admin();
                is_first_read = false;
            }
            if (is_admin) {
                udevid = devid;
            } else {
                continue;
            }
        } else {
            if (uda_devid_to_udevid(devid, &udevid) != 0) {
                continue;
            }
        }

        dev_inst = uda_dev_inst_get(udevid);
        if (dev_inst == NULL) {
            continue;
        }

#ifndef DRV_HOST
        if (dev_inst->type.location != UDA_LOCAL) {
            uda_dev_inst_put(dev_inst);
            continue;
        }
#endif

        logic_dev.valid = 1;
        logic_dev.devid = (unsigned short)devid;
        logic_dev.udevid = (unsigned short)udevid;
        logic_dev.hw_type = (dev_inst->type.hw == UDA_DAVINCI) ? UDA_HW_DAVINCI : UDA_HW_KUNPENG;
        if (uda_is_phy_dev(udevid)) {
            logic_dev.phy_devid = (unsigned short)udevid;
            logic_dev.sub_devid = 0;
        } else {
            struct uda_mia_dev_para mia_para;
            (void)uda_udevid_to_mia_devid(udevid, &mia_para);
            logic_dev.phy_devid = (unsigned short)mia_para.phy_devid;
            logic_dev.sub_devid = (unsigned char)mia_para.sub_devid;
        }

        uda_dev_inst_put(dev_inst);

        ret = ka_base_copy_to_user(&para.logic_dev[devid], &logic_dev, sizeof(logic_dev));
        if (ret != 0) {
            uda_err("Copy to user failed. (ret=%d; devid=%u)\n", ret, devid);
            break;
        }
    }

    return ret;
}

static int (*const uda_devid_trans_func[UDA_MAX_CMD])(u32 raw_devid, u32 *trans_devid) = {
    [_KA_IOC_NR(UDA_UDEVID_TO_DEVID)] = uda_udevid_to_devid,
    [_KA_IOC_NR(UDA_DEVID_TO_UDEVID)] = uda_devid_to_udevid,
    [_KA_IOC_NR(UDA_LUDEVID_TO_RUDEVID)] = uda_udevid_to_remote_udevid,
    [_KA_IOC_NR(UDA_RUDEVID_TO_LUDEVID)] = uda_remote_udevid_to_udevid,
};

static int ioctl_uda_devid_trans(unsigned int cmd, unsigned long arg)
{
    struct uda_devid_trans *usr_arg = (struct uda_devid_trans __ka_user *)arg;
    struct uda_devid_trans trans;
    int ret;

    ret = (int)ka_base_copy_from_user(&trans, usr_arg, sizeof(trans));
    if (ret != 0) {
        uda_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = uda_devid_trans_func[_KA_IOC_NR(cmd)](trans.raw_devid, &trans.trans_devid);
    if (ret != 0) {
        return ret;
    }

    return (int)ka_base_put_user(trans.trans_devid, &usr_arg->trans_devid);
}

#ifdef CFG_FEATURE_ASCEND950_STUB
static u32 g_raw_proc_contain_flag = 0;

static int ioctl_uda_set_raw_proc_is_contain_flag(unsigned int cmd, unsigned long arg)
{
    int ret;

    ret = (int)ka_base_copy_from_user(&g_raw_proc_contain_flag, (u32 __ka_user *)arg, sizeof(g_raw_proc_contain_flag));
    if (ret != 0) {
        uda_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

static int ioctl_uda_get_raw_proc_is_contain_flag(unsigned int cmd, unsigned long arg)
{
    int ret;

    ret = (int)ka_base_copy_to_user((u32 __ka_user *)arg, &g_raw_proc_contain_flag, sizeof(g_raw_proc_contain_flag));
    if (ret != 0) {
        uda_err("Copy to user failed. (ret=%d)\n", ret);
    }
    return ret;
}

u32 uda_get_raw_proc_is_contain_flag(void)
{
    return g_raw_proc_contain_flag;
}
#endif

static int (*const uda_ioctl_handles[UDA_MAX_CMD])(unsigned int cmd, unsigned long arg) = {
    [_KA_IOC_NR(UDA_GET_USER_INFO)] = ioctl_uda_get_user_info,
    [_KA_IOC_NR(UDA_SETUP_DEV_TABLE)] = ioctl_uda_setup_dev_table,
    [_KA_IOC_NR(UDA_GET_DEV_LIST)] = ioctl_uda_get_dev_list,
    [_KA_IOC_NR(UDA_UDEVID_TO_DEVID)] = ioctl_uda_devid_trans,
    [_KA_IOC_NR(UDA_DEVID_TO_UDEVID)] = ioctl_uda_devid_trans,
    [_KA_IOC_NR(UDA_LUDEVID_TO_RUDEVID)] = ioctl_uda_devid_trans,
    [_KA_IOC_NR(UDA_RUDEVID_TO_LUDEVID)] = ioctl_uda_devid_trans,
#ifdef CFG_FEATURE_ASCEND950_STUB
    [_KA_IOC_NR(UDA_SET_RAW_PROC_IS_CONTAIN_FLAG)] = ioctl_uda_set_raw_proc_is_contain_flag,
    [_KA_IOC_NR(UDA_GET_RAW_PROC_IS_CONTAIN_FLAG)] = ioctl_uda_get_raw_proc_is_contain_flag,
#endif
};

static long uda_ioctl(ka_file_t *file, unsigned int cmd, unsigned long arg)
{
    int cmd_nr = _KA_IOC_NR(cmd);
    if ((cmd_nr < 0) || (cmd_nr >= UDA_MAX_CMD) || (uda_ioctl_handles[cmd_nr] == NULL)) {
        uda_debug("Unsupported command. (cmd_nr=%d)\n", cmd_nr);
        return -EOPNOTSUPP;
    }

    return (long)uda_ioctl_handles[cmd_nr](cmd, arg);
}

static int uda_open(ka_inode_t *inode, ka_file_t *file)
{
    return 0;
}

static int uda_release(ka_inode_t *inode, ka_file_t *file)
{
    return 0;
}

// clang-format off
static ka_file_operations_t uda_fops = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_open(uda_open)
    ka_fs_init_f_release(uda_release)
    ka_fs_init_f_unlocked_ioctl(uda_ioctl)
};
// clang-format on

int uda_init_module(void)
{
    int ret;

    ret = uda_dev_init();
    if (ret != 0) {
        return ret;
    }

    ret = uda_notifier_init();
    if (ret != 0) {
        uda_dev_uninit();
        return ret;
    }

    ret = uda_access_init();
    if (ret != 0) {
        uda_notifier_uninit();
        uda_dev_uninit();
        return ret;
    }

    ret = drv_davinci_register_sub_module(UDA_CHAR_DEV_NAME, &uda_fops);
    if (ret != 0) {
        uda_access_uninit();
        uda_notifier_uninit();
        uda_dev_uninit();
        uda_err("Register sub module fail. (ret=%d)\n", ret);
        return ret;
    }

    uda_proc_fs_init();

#ifdef DRV_HOST
#ifdef CFG_FEATURE_SPOD_INFO
    spod_info_init();
#endif
#endif

    return 0;
}

void uda_exit_module(void)
{
    uda_proc_fs_uninit();
    (void)drv_ascend_unregister_sub_module(UDA_CHAR_DEV_NAME);
    uda_access_uninit();
    uda_notifier_uninit();
    uda_dev_uninit();
}
