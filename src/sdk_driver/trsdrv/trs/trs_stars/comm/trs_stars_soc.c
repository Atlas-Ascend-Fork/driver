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

#include "pbl/pbl_soc_res.h"
#include "pbl/pbl_uda.h"

#include "trs_pub_def.h"
#include "trs_stars_com.h"
#include "trs_stars_function.h"
#include "trs_stars_auto_init.h"

#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
static const ka_pci_device_id_t trs_stars_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
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
KA_MODULE_DEVICE_TABLE(pci, trs_stars_tbl);

struct trs_stars_ops *g_stars_ops;
int trs_stars_soc_res_ctrl(struct trs_id_inst *inst, u32 type, u32 id, u32 cmd)
{
    if (g_stars_ops->res_id_ctrl == NULL) {
        return -EOPNOTSUPP;
    }

    return g_stars_ops->res_id_ctrl(inst, type, id, cmd);
}
KA_EXPORT_SYMBOL_GPL(trs_stars_soc_res_ctrl);

int trs_stars_soc_get_id_status(struct trs_id_inst *inst, u32 type, u32 id, u32 *status)
{
    int ret;

    if (g_stars_ops->get_id_status == NULL) {
        return -EOPNOTSUPP;
    }

    ret = g_stars_ops->get_id_status(inst, type, id, status);
    if (ret != 0) {
        trs_err("Failed to get id status. (devid=%u; tsid=%u; type=%u; id=%u)\n", inst->devid, inst->tsid, type, id);
        return ret;
    }
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_stars_soc_get_id_status);

int trs_stars_soc_set_sq_head(struct trs_id_inst *inst, u32 sqid, u32 val)
{
    if (g_stars_ops->set_sq_head == NULL) {
        return -EOPNOTSUPP;
    }

    g_stars_ops->set_sq_head(inst, sqid, val);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_stars_soc_set_sq_head);

int trs_stars_soc_get_sq_head(struct trs_id_inst *inst, u32 sqid, u32 *val)
{
    if (g_stars_ops->get_sq_head == NULL) {
        return -EOPNOTSUPP;
    }

    *val = (u32)g_stars_ops->get_sq_head(inst, sqid);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_stars_soc_get_sq_head);

int trs_stars_soc_set_sq_tail(struct trs_id_inst *inst, u32 sqid, u32 val)
{
    if (g_stars_ops->set_sq_tail == NULL) {
        return -EOPNOTSUPP;
    }

    g_stars_ops->set_sq_tail(inst, sqid, val);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_stars_soc_set_sq_tail);

int trs_stars_soc_get_sq_tail(struct trs_id_inst *inst, u32 sqid, u32 *val)
{
    if (g_stars_ops->get_sq_tail == NULL) {
        return -EOPNOTSUPP;
    }

    *val = (u32)g_stars_ops->get_sq_tail(inst, sqid);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_stars_soc_get_sq_tail);

int trs_stars_soc_get_sq_fsm_status(struct trs_id_inst *inst, u32 sqid, u32 *val)
{
    if (g_stars_ops->get_sq_fsm_status == NULL) {
        return -EOPNOTSUPP;
    }

    *val = (u32)g_stars_ops->get_sq_fsm_status(inst, sqid);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_stars_soc_get_sq_fsm_status);

int trs_stars_soc_init(u32 phy_devid)
{
    struct trs_id_inst inst;
    u32 ts_num = 0;
    u32 i, tsid;
    int ret;

    if (!uda_is_phy_dev(phy_devid)) {
        return 0;
    }
    ret = soc_resmng_subsys_get_num(phy_devid, TS_SUBSYS, &ts_num);
    if ((ret != 0) || (ts_num == 0) || (ts_num > TRS_TS_MAX_NUM)) {
        trs_err("Init get ts num failed. (devid=%u; ts_num=%u; ret=%d)\n", phy_devid, ts_num, ret);
        return -EFAULT;
    }

    for (tsid = 0; tsid < ts_num; tsid++) {
        trs_id_inst_pack(&inst, phy_devid, tsid);
        ret = trs_stars_func_init(&inst);
        if (ret != 0) {
            trs_err("Failed to init stars func. (phy_devid=%u; tsid=%u)\n", phy_devid, tsid);
            for (i = 0; i < tsid; i++) {
                trs_id_inst_pack(&inst, phy_devid, i);
                trs_stars_func_uninit(&inst);
            }
            return ret;
        }
    }

    trs_debug("Init stars func ok. (phy_devid=%u)\n", phy_devid);
    return 0;
}

void trs_stars_soc_uninit(u32 phy_devid)
{
    struct trs_id_inst inst;
    u32 ts_num = 0;
    u32 tsid;
    int ret;

    if (!uda_is_phy_dev(phy_devid)) {
        return;
    }

    ret = soc_resmng_subsys_get_num(phy_devid, TS_SUBSYS, &ts_num);
    if ((ret != 0) || (ts_num == 0) || (ts_num > TRS_TS_MAX_NUM)) {
        return;
    }

    for (tsid = 0; tsid < ts_num; tsid++) {
        trs_id_inst_pack(&inst, phy_devid, tsid);
        trs_stars_func_uninit(&inst);
    }

    trs_info("Uninit stars func ok. (phy_devid=%u)\n", phy_devid);
}

int trs_stars_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (action == UDA_INIT) {
        ret = module_feature_auto_init_dev(udevid);
        if (ret != 0) {
            trs_err("Auto init dev failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
        ret = trs_stars_soc_init(udevid);
        if (ret != 0) {
            module_feature_auto_uninit_dev(udevid);
        }
    } else if (action == UDA_UNINIT) {
        trs_stars_soc_uninit(udevid);
        module_feature_auto_uninit_dev(udevid);
    }

    return ret;
}

int init_trs_stars(void)
{
    int ret;

    ret = trs_stars_notifier_register();
    if (ret != 0) {
        trs_err("Register notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    g_stars_ops = trs_stars_func_op_get();
    return 0;
}

void exit_trs_stars(void)
{
    trs_stars_notifier_unregister();
}