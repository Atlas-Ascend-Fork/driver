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
#include "ts_agent_interface.h"
#include "ts_agent_vsq_worker.h"
#include "ts_agent_vsq_proc.h"
#include "hvtsdrv_tsagent.h"
#ifdef CFG_SOC_PLATFORM_STARS
#include "securec.h"
#include "tsch/task_struct.h"
#include "ts_agent_update_sqe.h"
#include "trs_adapt.h"
#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
#include "pbl/pbl_soc_res.h"
#include "pbl/pbl_uda.h"
#endif
#endif
#include "ka_kernel_def_pub.h"
#include "ka_pci_pub.h"
#include "ka_compiler_pub.h"

#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
#define TS_AGENT_HOST "ts_agent_host"
#define TS_AGENT_HOST_VIRT "ts_agent_host_virt"
#define TS_AGENT_DEFAULT_TSID 0U

static int tsagent_get_ts_sram_para(u32 udevid, struct trs_sqcq_agent_para *para)
{
    struct res_inst_info res_inst;
    struct soc_rsv_mem_info rsv_mem;
    int ret = 0;

    if (para == NULL) {
        return -EINVAL;
    }

    soc_resmng_inst_pack(&res_inst, udevid, TS_SUBSYS, TS_AGENT_DEFAULT_TSID);
    ret = soc_resmng_get_rsv_mem(&res_inst, "TS_SRAM_MEM", &rsv_mem);
    if (ret != 0) {
        return ret;
    }

    para->rsv_phy_addr = rsv_mem.rsv_mem;
    para->rsv_size = rsv_mem.rsv_mem_size;
    return EOK;
}

static int ts_agent_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct trs_sqcq_agent_para para = {0};
    int ret = 0;

    if (action == UDA_INIT) {
        ret = tsagent_get_ts_sram_para(udevid, &para);
        if (ret == 0) {
            ret = tsagent_device_init(udevid, TS_AGENT_DEFAULT_TSID, &para);
        }
    } else if (action == UDA_UNINIT) {
        ret = tsagent_device_uninit(udevid, TS_AGENT_DEFAULT_TSID);
    }

    ts_agent_info("UDA action, udevid=%u, action=%d, ret=%d.", udevid, action, ret);
    return ret;
}

static int tsagent_uda_init(void)
{
    struct uda_dev_type type;
    int ret = 0;

    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(TS_AGENT_HOST, &type, UDA_PRI2, ts_agent_notifier_func);
    if (ret != 0) {
        ts_agent_err("Register host notifier failed, ret=%d.", ret);
        return ret;
    }

#ifndef CFG_HOST_VIRTUAL_MACHINES
    uda_dev_type_pack(&type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_REAL_SEC_EH);
    ret = uda_notifier_register(TS_AGENT_HOST_VIRT, &type, UDA_PRI2, ts_agent_notifier_func);
    if (ret != 0) {
        ts_agent_err("Register host virt notifier failed, ret=%d.", ret);
        uda_davinci_near_real_entity_type_pack(&type);
        (void)uda_notifier_unregister(TS_AGENT_HOST, &type);
        return ret;
    }
#endif

    return EOK;
}

static void tsagent_uda_uninit(void)
{
    struct uda_dev_type type;

#ifndef CFG_HOST_VIRTUAL_MACHINES
    uda_dev_type_pack(&type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_REAL_SEC_EH);
    (void)uda_notifier_unregister(TS_AGENT_HOST_VIRT, &type);
#endif
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(TS_AGENT_HOST, &type);
}
#endif

STATIC int __ka_init ts_agent_init(void)
{
    int ret = 0;
#ifdef CFG_SOC_PLATFORM_STARS
    struct trs_sqcq_agent_ops ops = {0};
    init_task_convert_func();
    ret = tsagent_stream_id_to_sq_id_init();
    if (ret != EOK) {
        return ret;
    }
    tsagent_dev_base_info_init();

    ts_agent_info("ts_agent_init begin.");
// register methods to ts_drv
#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
    tsagent_sq_base_info_init();
    ret = tsagent_uda_init();
#endif
    if (ret != 0) {
        tsagent_stream_id_to_sq_id_uninit();
        return ret;
    }

#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
    ops.sq_send_mode_update = tsagent_sq_send_mode_update;
#endif
    ops.sqe_update = tsagent_sqe_update;
#if defined(CFG_SOC_PLATFORM_CLOUD_V2) || defined(CFG_SOC_PLATFORM_MINIV3_EP)
    ops.mb_update = tsagent_mailbox_update;
    ops.cqe_update = tsagent_cqe_update;
#endif
    trs_sqcq_agent_ops_register(&ops);
#else
    struct hvtsdrv_tsagent_ops intf_info;
    ts_agent_info("ts_agent_init begin.");
    init_task_convert_func();
    ret = init_all_vf_work_ctx();
    if (ret != 0) {
        ts_agent_err("init all vf work ctx failed, ret=%d.", ret);
        return ret;
    }

    // register methods to ts_drv
    intf_info.tsagent_vf_create = ts_agent_vf_create;
    intf_info.tsagent_vf_destroy = ts_agent_vf_destroy;
    intf_info.tsagent_vsq_proc = ts_agent_vsq_proc;
    intf_info.tsagent_trans_mailbox_msg = ts_agent_trans_mailbox_msg;
    hal_kernel_hvtsdrv_tsagent_register(&intf_info);
#endif
    ts_agent_info("ts_agent_init end.");
    return 0;
}

STATIC void __ka_exit ts_agent_exit(void)
{
    ts_agent_info("ts_agent_exit begin.");
#ifdef CFG_SOC_PLATFORM_STARS
    trs_sqcq_agent_ops_unregister();
#if defined(CFG_SOC_PLATFORM_CLOUD_V2)
    tsagent_uda_uninit();
#endif
    tsagent_stream_id_to_sq_id_uninit();
#else
    // unregister to tsdrv
    hal_kernel_hvtsdrv_tsagent_unregister();
    // clear ts agent work queue
    destroy_all_vf_work_ctx();
#endif
    ts_agent_info("ts_agent_exit end.");
}

#define PCI_VENDOR_ID_HUAWEI 0x19e5

static const ka_pci_device_id_t g_ts_agent_tbl[] = {{KA_PCI_VDEVICE(HUAWEI, 0xd801), 0},
                                                    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
                                                    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
                                                    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
                                                    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
                                                    {0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                    {0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                    {0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                    {0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                    {0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                    {0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                    {}};
KA_MODULE_DEVICE_TABLE(pci, g_ts_agent_tbl);

ka_module_init(ts_agent_init);
ka_module_exit(ts_agent_exit);

KA_MODULE_LICENSE("GPL v2");
KA_MODULE_INFO(supported, "ts agent");
KA_MODULE_VERSION("v0.1");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
