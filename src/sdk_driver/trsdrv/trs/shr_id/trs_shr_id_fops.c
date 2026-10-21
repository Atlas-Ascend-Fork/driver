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
#include "ka_pci_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_fs_pub.h"
#include "ka_ioctl_pub.h"

#include "trs_pub_def.h"
#include "pbl/pbl_davinci_api.h"

#include "trs_core.h"
#include "trs_shr_id.h"
#include "trs_shr_id_node.h"
#include "trs_shr_id_auto_init.h"

#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
static const ka_pci_device_id_t trs_shrid_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd100), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xa126), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd801), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd501), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd804), 0},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {}};
KA_MODULE_DEVICE_TABLE(pci, trs_shrid_tbl);

static int (*shr_id_ioctl_handles[SHR_ID_MAX_CMD])(struct shr_id_proc_ctx *proc_ctx, unsigned long arg) = {
    [_KA_IOC_NR(SHR_ID_CREATE)] = shr_id_create,     [_KA_IOC_NR(SHR_ID_OPEN)] = shr_id_open,
    [_KA_IOC_NR(SHR_ID_CLOSE)] = shr_id_close,       [_KA_IOC_NR(SHR_ID_DESTROY)] = shr_id_destroy,
    [_KA_IOC_NR(SHR_ID_SET_PID)] = shr_id_set_pid,   [_KA_IOC_NR(SHR_ID_RECORD)] = shr_id_record,
    [_KA_IOC_NR(SHR_ID_SET_ATTR)] = shr_id_set_attr, [_KA_IOC_NR(SHR_ID_GET_ATTR)] = shr_id_get_attr,
    [_KA_IOC_NR(SHR_ID_GET_INFO)] = shr_id_get_info,
};

void shr_id_register_ioctl_cmd_func(int nr, int (*fn)(struct shr_id_proc_ctx *proc_ctx, unsigned long arg))
{
    shr_id_ioctl_handles[nr] = fn;
}

static int shr_id_file_open(ka_inode_t *inode, ka_file_t *file)
{
    struct shr_id_proc_ctx *proc_ctx = NULL;
    int ret;

    ret = shr_id_wait_for_proc_exit(ka_task_get_current_tgid());
    if (ret != 0) {
        trs_err("Wait for proc exit fail. (ret=%d)\n", ret);
        return ret;
    }

    proc_ctx = shr_id_proc_create(ka_task_get_current_tgid());
    if (proc_ctx == NULL) {
        trs_err("Open failed.\n");
        return -ENOMEM;
    }

    ret = shr_id_proc_add(proc_ctx);
    if (ret != 0) {
        shr_id_proc_destroy(proc_ctx);
        return ret;
    }
    ka_fs_set_file_private_data(file, proc_ctx);

    return 0;
}

static int shr_id_file_release(ka_inode_t *inode, ka_file_t *file)
{
    struct shr_id_proc_ctx *proc_ctx = ka_fs_get_file_private_data(file);

    if (proc_ctx == NULL) {
        trs_err("Not open.\n");
        return -EFAULT;
    }
    ka_fs_set_file_private_data(file, NULL);
    shr_id_proc_del(proc_ctx);
    return 0;
}

static long shr_id_file_ioctl(ka_file_t *file, u32 cmd, unsigned long arg)
{
    struct shr_id_proc_ctx *proc_ctx = ka_fs_get_file_private_data(file);
    u32 cmd_nr = _KA_IOC_NR(cmd);
    if ((proc_ctx == NULL) || (arg == 0) || (cmd_nr >= SHR_ID_MAX_CMD) || (shr_id_ioctl_handles[cmd_nr] == NULL)) {
        trs_err("Unsupported command. (cmd=%u; arg=0x%lx)\n", cmd_nr, arg);
        return -EOPNOTSUPP;
    }

    return shr_id_ioctl_handles[cmd_nr](proc_ctx, arg);
}

static ka_file_operations_t shr_id_notify_fops = {
    .owner = KA_THIS_MODULE,
    .open = shr_id_file_open,
    .release = shr_id_file_release,
    .unlocked_ioctl = shr_id_file_ioctl,
};

int shr_id_init_module(void)
{
    int ret;

    ret = drv_davinci_register_sub_module(DAVINCI_INTF_MODULE_TRS_SHR_ID, &shr_id_notify_fops);
    if (ret != 0) {
        trs_err("Register sub module fail. (ret=%d)\n", ret);
        return ret;
    }
    trs_res_share_proc_ops_register(TRS_NOTIFY, shr_id_is_belong_to_proc);
    shr_id_node_init();

    ret = module_feature_auto_init();
    if (ret != 0) {
        trs_res_share_proc_ops_register(TRS_NOTIFY, NULL);
        (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_TRS_SHR_ID);
        trs_err("Register event update fail. (ret=%d)\n", ret);
        return ret;
    }

    trs_debug("Register module success.\n");

    return 0;
}

void shr_id_exit_module(void)
{
    module_feature_auto_uninit();
    trs_res_share_proc_ops_register(TRS_NOTIFY, NULL);
    (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_TRS_SHR_ID);
    trs_info("Unregister module success.\n");
}