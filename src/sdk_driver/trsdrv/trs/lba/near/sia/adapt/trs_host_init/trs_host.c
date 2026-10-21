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
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

#include "pbl/pbl_uda.h"
#include "pbl/pbl_soc_res.h"
#include "comm_kernel_interface.h"
#include "trs_chip_def_comm.h"
#include "trs_host_msg.h"
#include "trs_host_comm.h"
#include "trs_chan_mem_pool.h"
#include "trs_msg.h"
#include "trs_cdqm.h"
#include "trs_stars_comm.h"
#include "trs_core.h"
#include "trs_host_accelerator_util.h"
#include "trs_host_group.h"
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
#include "trs_ub_init_common.h"
#endif
#include "trs_host.h"
#include "trs_sia_adapt_auto_init.h"
#include "trs_host_mode_config.h"

#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
static const ka_pci_device_id_t trs_pm_adapt_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd100), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xa126), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd801), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd501), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd804), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd806), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd807), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd808), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd200), 0},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {}};
KA_MODULE_DEVICE_TABLE(pci, trs_pm_adapt_tbl);

static int trs_host_notice_process_recycle(u32 devid, struct trs_msg_data *data)
{
    struct trs_msg_proc_recycle *msg = (struct trs_msg_proc_recycle *)data->payload;
    struct trs_id_inst inst;

    trs_id_inst_pack(&inst, devid, data->header.tsid);
    return trs_notice_check_proc_recycle(&inst, msg->pid);
}

static const trs_msg_rdv_func_t rcv_ops[TRS_MSG_MAX] = {
    [TRS_MSG_CHAN_ABNORMAL] = trs_host_ts_adapt_abnormal_proc,
    [TRS_MSG_SET_TS_STATUS] = trs_host_set_ts_status,
    [TRS_MSG_FLUSH_RES_ID] = trs_host_flush_id,
    [TRS_MSG_RES_ID_CHECK] = trs_host_res_is_check_msg_proc,
    [TRS_MSG_TS_CQ_PROCESS] = trs_host_ts_cq_process,
    [TRS_MSG_NOTICE_PROC_RECYCLE] = trs_host_notice_process_recycle,
};

int trs_host_msg_chan_recv(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    u32 devid = (u32)(uintptr_t)devdrv_get_msg_chan_priv(msg_chan);
    struct trs_msg_data *_data = (struct trs_msg_data *)data;
    int ret = ENODEV;

    ret = trs_host_msg_chan_recv_check(devid, _data, in_data_len, out_data_len, real_out_len);
    if (ret != 0) {
        trs_err("Msg rcv check fail. (ret=%d)\n", ret);
        return ret;
    }
    if (rcv_ops[_data->header.cmdtype] != NULL) {
        ret = rcv_ops[_data->header.cmdtype](devid, _data);
        *real_out_len = (u32)sizeof(struct trs_msg_data);
        _data->header.valid = TRS_MSG_RCV_MAGIC;
        _data->header.result = ret;
    }
    return 0;
}

#define TRS_HOST_MSG_CHAN_SIZE (TRS_MSG_TOTAL_LEN + 128) /* 128 rsv for non trans msg head */
static struct devdrv_non_trans_msg_chan_info trs_host_msg_chan_info = {
    .msg_type = devdrv_msg_client_tsdrv,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_LOW,
    .s_desc_size = TRS_HOST_MSG_CHAN_SIZE,
    .c_desc_size = TRS_HOST_MSG_CHAN_SIZE,
    .rx_msg_process = trs_host_msg_chan_recv,
};

struct devdrv_non_trans_msg_chan_info *trs_get_msg_chan_info(void)
{
    return &trs_host_msg_chan_info;
}

/*
 module_feature_auto_init_dev adds features as follows:
 0. trs_host_msg_init, trs_host_mode_config_init_dev
 1. trs_ts_doorbell_init (ascend950 not need)
 2. trs_mbox_init, trs_soft_mbox_init
 3. trs_id_init, trs_host_group_init(only ascend950), trs_ub_dev_init
 4. trs_chan_init
 5. trs_core_init
 6. trs_res_ops_init, trs_sqcq_event_dev_init
 7. trs_ts_status_init (ascend950 not need)
 8. trs_host_cdqm_init (ascend950 not need)
 */
static int trs_near_ts_inst_notifier(struct trs_id_inst *pm_inst, enum uda_notified_action action)
{
    int ret = 0;

    if (action == UDA_INIT) {
        ret = module_feature_auto_init_dev(trs_id_inst_to_ts_inst(pm_inst));
    } else if (action == UDA_UNINIT) {
        module_feature_auto_uninit_dev(trs_id_inst_to_ts_inst(pm_inst));
        trs_chan_mem_node_recycle_by_dev(pm_inst->devid);
    } else if (action == UDA_TO_MIA) {
        ret = trs_set_ts_inst_feature_mode(pm_inst, TRS_INST_PART_FEATUR_MODE, TRS_SET_TS_INST_MODE_FORCE_LEVEL_NONE);
        if (ret != 0) {
            trs_err("Failed to get mia base info. (devid=%u; ret=%d)\n", pm_inst->devid, ret);
            return ret;
        }
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
        trs_ub_dev_uninit(trs_id_inst_to_ts_inst(pm_inst));
#endif
        (void)trs_mode_config_to_mia(pm_inst->devid);
    } else if (action == UDA_TO_SIA) {
        (void)trs_mode_config_to_sia(pm_inst->devid);
        (void)trs_set_ts_inst_feature_mode(pm_inst, TRS_INST_ALL_FEATUR_MODE, TRS_SET_TS_INST_MODE_FORCE_LEVEL_NONE);
#ifdef CFG_FEATURE_SUPPORT_UB_CONNECTION
        trs_ub_dev_init(trs_id_inst_to_ts_inst(pm_inst));
#endif
    } else if (action == UDA_ADD_GROUP) {
        u32 grp_id;
        ret = uda_get_action_para(pm_inst->devid, action, &grp_id);
        if (ret != 0) {
            trs_err("Failed to get grp_id. (udevid=%u)\n", pm_inst->devid);
            return ret;
        }
        return trs_add_group(pm_inst, grp_id);
    } else if (action == UDA_DEL_GROUP) {
        u32 grp_id;
        ret = uda_get_action_para(pm_inst->devid, action, &grp_id);
        if (ret != 0) {
            trs_err("Failed to get grp_id. (udevid=%u)\n", pm_inst->devid);
            return ret;
        }
        return trs_delete_group(pm_inst, grp_id);
    } else if (action == UDA_HOTRESET) {
        ret = trs_set_ts_inst_feature_mode(pm_inst, TRS_INST_PART_FEATUR_MODE, TRS_SET_TS_INST_MODE_FORCE_LEVEL_PART);
        if (ret != 0) {
            trs_err("Not meet host_reset conditions. (devid=%u; ret=%d)\n", pm_inst->devid, ret);
            return ret;
        }
    } else if (action == UDA_HOTRESET_CANCEL) {
        (void)trs_set_ts_inst_feature_mode(pm_inst, TRS_INST_ALL_FEATUR_MODE, TRS_SET_TS_INST_MODE_FORCE_LEVEL_NONE);
    } else {
        /* Ignore other actions. */
        return 0;
    }
    return ret;
}

static bool trs_near_need_proc_action(enum uda_notified_action action)
{
    return (action == UDA_INIT) || (action == UDA_UNINIT) || (action == UDA_TO_MIA) || (action == UDA_TO_SIA) ||
           (action == UDA_ADD_GROUP) || (action == UDA_DEL_GROUP) || (action == UDA_HOTRESET) ||
           (action == UDA_HOTRESET_CANCEL);
}

#define TRS_NEAR_NOTIFIER "trs_near"
static int trs_near_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct trs_id_inst pm_inst;
    u32 ts_num = 0;
    u32 tsid, i;
    int ret;

    if (!trs_near_need_proc_action(action)) {
        return 0;
    }

    if (udevid >= TRS_DEV_MAX_NUM) {
        trs_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = soc_resmng_subsys_get_num(udevid, TS_SUBSYS, &ts_num);
    if ((ret != 0) || (ts_num == 0) || (ts_num > TRS_TS_MAX_NUM)) {
        trs_err("Get ts num failed. (devid=%u; ts_num=%u; ret=%d)\n", udevid, ts_num, ret);
        return (ret != 0) ? ret : -EINVAL;
    }

    for (tsid = 0; tsid < ts_num; tsid++) {
        trs_id_inst_pack(&pm_inst, udevid, tsid);
        ret = trs_near_ts_inst_notifier(&pm_inst, action);
        if (ret != 0) {
            trs_err("Notifier action failed. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
            goto fail;
        }
    }

    trs_info("Notifier action success. (udevid=%u; action=%d)\n", udevid, action);
    return 0;

fail:
    for (i = 0; i < tsid; i++) {
#ifndef EMU_ST
        trs_id_inst_pack(&pm_inst, udevid, i);
        if (action == UDA_INIT) {
            (void)trs_near_ts_inst_notifier(&pm_inst, UDA_UNINIT);
        } else if (action == UDA_TO_MIA) {
            (void)trs_near_ts_inst_notifier(&pm_inst, UDA_TO_SIA);
        } else if (action == UDA_TO_SIA) {
            (void)trs_near_ts_inst_notifier(&pm_inst, UDA_TO_MIA);
        }
#endif
    }
    return ret;
}

/*
 * module_feature_auto_init adds features as follows:
 * 0. trs_res_map_ops_init
 * 1. trs_ub_host_init
 * 2. trs_sqcq_event_init
 */
int init_trs_adapt(void)
{
    struct uda_dev_type type;
    int ret;

    ret = module_feature_auto_init();
    if (ret != 0) {
        trs_err("module_feature_auto_init failed. (ret=%d)\n", ret);
        return ret;
    }

    trs_chan_mem_node_proc_fs_init();
    trs_accelerator_util_init();
    uda_davinci_near_real_entity_type_pack(&type);

    ret = uda_notifier_register(TRS_NEAR_NOTIFIER, &type, UDA_PRI2, trs_near_notifier_func);
    if (ret != 0) {
        trs_accelerator_util_uninit();
        trs_chan_mem_node_proc_fs_uninit();
        module_feature_auto_uninit();
        trs_err("Register uda notifier failed. (ret=%d)\n", ret);
        return ret;
    }
    trs_info("Notifier register. (name=%s)\n", TRS_NEAR_NOTIFIER);

    return ret;
}

void exit_trs_adapt(void)
{
    struct uda_dev_type type;
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(TRS_NEAR_NOTIFIER, &type);
    trs_chan_mem_node_recycle();
    trs_accelerator_util_uninit();
    trs_chan_mem_node_proc_fs_uninit();
    module_feature_auto_uninit();
}