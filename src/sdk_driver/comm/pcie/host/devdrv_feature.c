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

#include "devdrv_util.h"
#include "devdrv_feature.h"

struct devdrv_feature_mgr g_devdrv_feature_mgr;

// clang-format off
static const struct devdrv_feature_meta g_devdrv_feature_meta[] = {
    [DEVDRV_FEATURE_PCIE_PROTO_DIP] = {
        .name = "PCIE PROTO DIP",
        .desc = "soc res sync func"
    },
    [DEVDRV_FEATURE_S2S] = {
        .name = "S2S",
        .desc = "msg communication between supernodes"
    },
    [DEVDRV_FEATURE_PCIE_SENTRY] = {
        .name = "PCIE SENTRY",
        .desc = "enable sentry mode"
    },
    [DEVDRV_FEATURE_SRIOV] = {
        .name = "SRIOV",
        .desc = "enable sriov"
    },
    [DEVDRV_FEATURE_BYPASS_SMMU] = {
        .name = "BYPASS SMMU",
        .desc = "bypass smmu, use PA for dma"
    },
    [DEVDRV_FEATURE_DUMP_PCIE_DFX] = {
        .name = "DUMP PCIE DFX",
        .desc = "PCIe debug info for ras and dma error stat"
    },
    [DEVDRV_FEATURE_TIME_COST_DFX] = {
        .name = "TIME COST DFX",
        .desc = "detecting and printing of time-consuming operations"
    },
    [DEVDRV_FEATURE_SEC_COMM_L3] = {
        .name = "SEC COMM L3",
        .desc = "dma copy of sq/cq enables host-device msg communication"
    },
    [DEVDRV_FEATURE_DMA_COPY_SVA] = {
        .name = "DMA COPY SVA",
        .desc = "dma copy with sva support, no dma addr conversion"
    },
    [DEVDRV_FEATURE_DMI] = {
        .name = "DMI",
        .desc = "get dev id via dmi slot id"
    },
    [DEVDRV_FEATURE_SYSFS_DUMP_DFX] = {
        .name = "SYSFS DUMP DFX",
        .desc = "dump the reg info recorded in g_dfx_reg_item"
    },
    [DEVDRV_FEATURE_P2P_EXTEND] = {
        .name = "P2P EXTEND",
        .desc = "P2P extended from 16p to 40p"
    },
    [DEVDRV_FEATURE_PHY_MACH_FLAG_SKIP_PAGE] = {
        .name = "PHY MACH FLAG SKIP PAGE",
        .desc = "64KB tail guard in msg buf prevents dma copy overflow"
    },
    [DEVDRV_FEATURE_PCIE_LINK_INFO] = {
        .name = "PCIE LINK INFO",
        .desc = "get or set the pcie link status"
    },
    [DEVDRV_FEATURE_HCCS_LINK_INFO] = {
        .name = "HCCS LINK INFO",
        .desc = "get or set the hccs link status"
    },
    [DEVDRV_FEATURE_PRE_CFG] = {
        .name = "PRE CFG",
        .desc = "enable the high-performance mode"
    },
    [DEVDRV_FEATURE_SHR_PARA_REBUILD] = {
        .name = "SHR PARA REBUILD",
        .desc = "rebuild the shr para addr, from A addr to B addr"
    },
};

static const struct devdrv_feature_ops_meta g_devdrv_feature_ops_meta[] = {
    {
        .name = "DEVDRV_SRIOV_TYPE_PF",
        .bit = "0"
    },
    {
        .name = "DEVDRV_SRIOV_TYPE_VF",
        .bit = "1"
    },
    {
        .name = "CONNECT_PROTOCOL_PCIE",
        .bit = "2"
    },
    {
        .name = "CONNECT_PROTOCOL_HCCS",
        .bit = "3"
    },
    {
        .name = "DEVDRV_PHY_BOOT",
        .bit = "4"
    },
    {
        .name = "DEVDRV_SRIOV_VF_BOOT",
        .bit = "5"
    },
    {
        .name = "DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT",
        .bit = "6"
    },
    {
        .name = "DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT",
        .bit = "7"
    },
    {
        .name = "DEVDRV_MDEV_VF_PM_BOOT",
        .bit = "8"
    },
    {
        .name = "DEVDRV_MDEV_VF_VM_BOOT",
        .bit = "9"
    },
};
// clang-format on

int devdrv_feature_mgr_init(void)
{
    if (g_devdrv_feature_mgr.inited) {
        devdrv_warn("devdrv Feature manager already inited.\n");
        return 0;
    }

    if (KA_BASE_ARRAY_SIZE(g_devdrv_feature_meta) != DEVDRV_FEATURE_MAX) {
        devdrv_err("devdrv feature meta table size mismatch with enum devdrv_feature.\n");
        return -EINVAL;
    }

    if (KA_BASE_ARRAY_SIZE(g_devdrv_feature_ops_meta) != DEVDRV_FEATURE_OPS_BIT_NUM) {
        devdrv_err("devdrv feature ops meta table size mismatch.\n");
        return -EINVAL;
    }

    ka_task_mutex_init(&g_devdrv_feature_mgr.lock);
    g_devdrv_feature_mgr.meta = g_devdrv_feature_meta;
    g_devdrv_feature_mgr.ops_meta = g_devdrv_feature_ops_meta;

    g_devdrv_feature_mgr.inited = true;

    devdrv_info("devdrv Feature manager init success. (feat_num=%d)\n", DEVDRV_FEATURE_MAX);
    return 0;
}

void devdrv_feature_mgr_uninit(void)
{
    if (!g_devdrv_feature_mgr.inited) {
        return;
    }

    g_devdrv_feature_mgr.inited = false;
    g_devdrv_feature_mgr.ops_meta = NULL;
    g_devdrv_feature_mgr.meta = NULL;
    ka_task_mutex_destroy(&g_devdrv_feature_mgr.lock);

    devdrv_info("devdrv Feature manager uninit success.\n");
}

struct devdrv_feature_mgr *devdrv_feature_get_feature_mgr(void)
{
    return &g_devdrv_feature_mgr;
}

bool devdrv_feature_is_support(devdrv_feature_bitmap_t bitmap, enum devdrv_feature feat)
{
    if (feat >= DEVDRV_FEATURE_MAX) {
        devdrv_err("Invalid devdrv feature. (feat=%d;feat_max_num=%d;)\n", feat, DEVDRV_FEATURE_MAX - 1);
        return false;
    }
    return (bitmap & (1ULL << feat)) ? true : false;
}
