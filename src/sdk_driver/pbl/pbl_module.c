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
#include "ka_compiler_pub.h"

#include "pbl_module.h"

#ifdef CFG_ENV_HOST
#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
#define PCI_DEVICE_CLOUD (0xa126U)

static const ka_pci_device_id_t g_pbl_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd100), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
    {KA_PCI_VDEVICE(HUAWEI, PCI_DEVICE_CLOUD), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd801), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd501), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd804), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd805), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd806), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd807), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd808), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd809), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd200), 0},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {}};
KA_MODULE_DEVICE_TABLE(pci, g_pbl_tbl);
#endif

int __attribute__((weak)) ka_mem_ops_module_init(void)
{
    return 0;
}

void __attribute__((weak)) ka_mem_ops_module_exit(void)
{
    return;
}

int __attribute__((weak)) prof_framework_init(void)
{
    return 0;
}

void __attribute__((weak)) prof_framework_exit(void)
{
    return;
}

int __attribute__((weak)) ascend_ctl_init(void)
{
    return 0;
}

void __attribute__((weak)) ascend_ctl_exit(void)
{
    return;
}

int __attribute__((weak)) ubmem_dev_init(void)
{
    return 0;
}

void __attribute__((weak)) ubmem_dev_exit(void)
{
    return;
}

struct submodule_ops {
    int (*init)(void);
    void (*uninit)(void);
};

static struct submodule_ops g_sub_table[] = {
#ifdef CFG_ENV_HOST
#ifdef CFG_FEATURE_VPBL
    {log_drv_module_init, log_drv_module_exit},
    {ka_mem_ops_module_init, ka_mem_ops_module_exit},
    {drv_ascend_intf_init, drv_davinci_intf_exit},
    {uda_init_module, uda_exit_module},
    {recfg_init, recfg_exit},
    {ubmem_dev_init, ubmem_dev_exit},
#else
    {log_drv_module_init, log_drv_module_exit},
    {ka_mem_ops_module_init, ka_mem_ops_module_exit},
    {drv_ascend_intf_init, drv_davinci_intf_exit},
    {uda_init_module, uda_exit_module},
    {resmng_init_module, resmng_exit_module},
    {recfg_init, recfg_exit},
    {ubmem_dev_init, ubmem_dev_exit},
    {urd_init, urd_exit},
#if !defined(CFG_FEATURE_KO_ALONE_COMPILE) && !defined(CFG_SOC_PLATFORM_RC)
    {devdrv_base_comm_init, devdrv_base_comm_exit},
#endif
#endif
#elif defined PKICMS_UT_TEST
    {pkicms_dev_init, pkicms_dev_exit},
#else
    {ka_mem_ops_module_init, ka_mem_ops_module_exit},
    {drv_ascend_intf_init, drv_davinci_intf_exit},
    {uda_init_module, uda_exit_module},
    {resmng_init_module, resmng_exit_module},
    {soc_config_init, soc_config_exit},
    {bdcfg_init, bdcfg_exit},
#ifdef CFG_FEATURE_SUPPORT_DRV_NOTIFY
    {drv_notify_init, drv_notify_exit},
#endif
    {recfg_init, recfg_exit},
    {ccfg_init, ccfg_exit},
    {ubmem_dev_init, ubmem_dev_exit},
    {prof_framework_init, prof_framework_exit},
#ifndef CFG_FEATURE_KO_ALONE_COMPILE
    {dev_user_cfg_module_init, dev_user_cfg_module_exit},
#endif
    {ascend_ctl_init, ascend_ctl_exit},
#ifndef CFG_FEATURE_NO_DFM
    {dfm_init, dfm_exit},
#endif
    {urd_init, urd_exit},
    {ipcdrv_pbl_init_module, ipcdrv_pbl_exit_module},
#ifndef CFG_FEATURE_KO_ALONE_COMPILE
    {icmdrv_pbl_init_module, icmdrv_pbl_exit_module},
    {pkicms_dev_init, pkicms_dev_exit},
#ifndef CFG_SOC_PLATFORM_RC
    {devdrv_base_comm_init, devdrv_base_comm_exit},
#endif
#endif
#endif
};

STATIC int __ka_init init_pbl_base(void)
{
    int index, ret;
    int table_size = sizeof(g_sub_table) / sizeof(struct submodule_ops);

    for (index = 0; index < table_size; index++) {
        ret = g_sub_table[index].init();
        if (ret != 0) {
            goto out;
        }
    }
    return 0;
out:
    for (; index > 0; index--) {
        g_sub_table[index - 1].uninit();
    }
    return ret;
}

STATIC void __ka_exit exit_pbl_base(void)
{
    int index;
    int table_size = sizeof(g_sub_table) / sizeof(struct submodule_ops);

    for (index = table_size; index > 0; index--) {
        g_sub_table[index - 1].uninit();
    }
}

ka_module_init(init_pbl_base);
ka_module_exit(exit_pbl_base);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("PBL BASE");
