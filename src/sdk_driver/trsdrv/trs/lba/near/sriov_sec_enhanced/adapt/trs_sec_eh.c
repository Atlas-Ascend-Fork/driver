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

#include "pbl/pbl_uda.h"

#include "comm_kernel_interface.h"
#include "vmng_kernel_interface.h"

#include "trs_chan_mem_pool.h"
#include "trs_host_msg.h"
#include "trs_host_comm.h"
#include "trs_host_accelerator_util.h"
#include "trs_pub_def.h"
#include "trs_sec_eh_core.h"
#include "trs_sec_eh_vpc.h"
#include "trs_sec_eh_init.h"
#include "trs_sec_eh.h"
#include "trs_sec_eh_auto_init.h"

static const ka_pci_device_id_t sec_eh_adapt_tbl[] = {{KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
                                                      {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
                                                      {KA_PCI_VDEVICE(HUAWEI, 0xd804), 0},
                                                      {KA_PCI_VDEVICE(HUAWEI, 0xd806), 0},
                                                      {KA_PCI_VDEVICE(HUAWEI, 0xd807), 0},
                                                      {KA_PCI_VDEVICE(HUAWEI, 0xd808), 0},
                                                      {KA_PCI_VDEVICE(HUAWEI, 0xd200), 0},
                                                      {0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                      {0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                      {0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                      {0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                      {0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                      {0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
                                                      {}};
KA_MODULE_DEVICE_TABLE(pci, sec_eh_adapt_tbl);

static const trs_msg_rdv_func_t rcv_ops[TRS_MSG_MAX] = {
    [TRS_MSG_CHAN_ABNORMAL] = trs_host_ts_adapt_abnormal_proc,
    [TRS_MSG_SET_TS_STATUS] = trs_host_set_ts_status,
    [TRS_MSG_FLUSH_RES_ID] = trs_host_flush_id,
};

static int trs_sec_eh_msg_chan_recv(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    struct trs_msg_data *_data = (struct trs_msg_data *)data;
    u32 devid = (u32)(uintptr_t)devdrv_get_msg_chan_priv(msg_chan);
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
        _data->header.result = (s16)ret;
    }

    return 0;
}

#define TRS_SEC_EH_MSG_CHAN_SIZE (TRS_MSG_TOTAL_LEN + 128) /* 128 rsv for non trans msg head */
static struct devdrv_non_trans_msg_chan_info trs_sec_eh_msg_chan_info = {
    .msg_type = devdrv_msg_client_tsdrv,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_LOW,
    .s_desc_size = TRS_SEC_EH_MSG_CHAN_SIZE,
    .c_desc_size = TRS_SEC_EH_MSG_CHAN_SIZE,
    .rx_msg_process = trs_sec_eh_msg_chan_recv,
};

struct devdrv_non_trans_msg_chan_info *trs_get_msg_chan_info(void)
{
    return &trs_sec_eh_msg_chan_info;
}

/*
 * module_feature_auto_init_dev adds features as follows:
 * 0. trs_host_msg_init, trs_res_map_ops_init(only ascend950)
 * 1. trs_sec_eh_vpc_init, trs_ts_doorbell_init(ascend950 not need)
 * 3. trs_id_init, trs_host_group_init(only ascend950)
 * 4. trs_chan_init
 * 5. trs_core_init
 * 6. trs_res_ops_init
 * 7. trs_ts_status_init(ascend950 not need)
 */
static int trs_sec_eh_ts_inst_notifier(struct trs_id_inst *inst, enum uda_notified_action action)
{
    int ret = 0;

    if (inst->devid >= TRS_DEV_MAX_NUM) {
#ifndef EMU_ST
        trs_err("Invalid para. (udevid=%u)\n", inst->devid);
        return -EINVAL;
#endif
    }

    if (action == UDA_INIT) {
        ret = module_feature_auto_init_dev(trs_id_inst_to_ts_inst(inst));
    } else if (action == UDA_UNINIT) {
        module_feature_auto_uninit_dev(trs_id_inst_to_ts_inst(inst));
        trs_chan_mem_node_recycle_by_dev(inst->devid);
    }

    trs_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", inst->devid, action, ret);

    return ret;
}

#define TRS_SEC_EH_NOTIFIER "trs_sec_eh"
static int trs_sec_eh_notifier_func(u32 udevid, enum uda_notified_action action)
{
    struct trs_id_inst inst;
    u32 ts_num = 0;
    u32 tsid, i;
    int ret;

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
        trs_id_inst_pack(&inst, udevid, tsid);
        ret = trs_sec_eh_ts_inst_notifier(&inst, action);
        if (ret != 0) {
            trs_err("Notifier action failed. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
            goto fail;
        }
    }

    trs_info("Notifier action success. (udevid=%u; action=%d)\n", udevid, action);
    return 0;

fail:
#ifndef EMU_ST
    for (i = 0; i < tsid; i++) {
        trs_id_inst_pack(&inst, udevid, i);
        if (action == UDA_INIT) {
            (void)trs_sec_eh_ts_inst_notifier(&inst, UDA_UNINIT);
        }
    }
#endif
    return ret;
}

int init_trs_sec_eh(void)
{
    struct uda_dev_type type;
    int ret;

    ret = module_feature_auto_init();
    if (ret != 0) {
        trs_err("Sec eh auto init failed. (ret=%d)\n", ret);
        return ret;
    }

    trs_accelerator_util_init();
    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(TRS_SEC_EH_NOTIFIER, &type, UDA_PRI2, trs_sec_eh_notifier_func);
    if (ret != 0) {
        trs_err("Sec eh notifier register failed. (ret=%d)\n", ret);
        trs_accelerator_util_uninit();
        module_feature_auto_uninit();
        return ret;
    }

    return ret;
}

void exit_trs_sec_eh(void)
{
    struct uda_dev_type type;

    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(TRS_SEC_EH_NOTIFIER, &type);
    trs_chan_mem_node_recycle();
    trs_accelerator_util_uninit();
    module_feature_auto_uninit();
}
