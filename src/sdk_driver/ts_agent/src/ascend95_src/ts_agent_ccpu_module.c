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

#include "ts_agent_log.h"
#include "ts_agnet_ccpu.h"
#include "trs_adapt.h"
#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"
#include "ka_compiler_pub.h"

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
static const ka_pci_device_id_t ts_agent_adapt_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd804), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd806), 0}, // david pci vdevice id
    {KA_PCI_VDEVICE(HUAWEI, 0xd807), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd808), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd200), 0},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {}};
KA_MODULE_DEVICE_TABLE(pci, ts_agent_adapt_tbl);

STATIC int __ka_init ts_agent_init(void)
{
    struct trs_sqcq_agent_ops ops = {
        .device_init = NULL,
        .device_uninit = NULL,
        .sqe_update = tsagent_sqe_update,
        .cqe_update = NULL,
        .mb_update = NULL,
        .sqe_update_src_check = tsagent_sqe_update_src_check,
    };

    init_task_convert_func();
    trs_sqcq_agent_ops_register(&ops);
    ts_agent_info("ts_agent_init end.");
    return 0;
}

STATIC void __ka_exit ts_agent_exit(void)
{
    ts_agent_info("ts_agent_exit begin.");
    trs_sqcq_agent_ops_unregister();
    ts_agent_info("ts_agent_exit end.");
}

ka_module_init(ts_agent_init);
ka_module_exit(ts_agent_exit);

KA_MODULE_LICENSE("GPL v2");
KA_MODULE_INFO(supported, "ts agent");
KA_MODULE_VERSION("v0.1");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
