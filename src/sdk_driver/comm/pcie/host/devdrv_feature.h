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

#ifndef __DEVDRV_FEATURE_H__
#define __DEVDRV_FEATURE_H__

#include "ka_type.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"

enum devdrv_feature {
    DEVDRV_FEATURE_PCIE_PROTO_DIP,
    DEVDRV_FEATURE_S2S,
    DEVDRV_FEATURE_PCIE_SENTRY,
    DEVDRV_FEATURE_SRIOV,
    DEVDRV_FEATURE_BYPASS_SMMU,
    DEVDRV_FEATURE_DUMP_PCIE_DFX,
    DEVDRV_FEATURE_TIME_COST_DFX,
    DEVDRV_FEATURE_SEC_COMM_L3,
    DEVDRV_FEATURE_DMA_COPY_SVA,
    DEVDRV_FEATURE_DMI,
    DEVDRV_FEATURE_SYSFS_DUMP_DFX,
    DEVDRV_FEATURE_P2P_EXTEND,
    DEVDRV_FEATURE_PHY_MACH_FLAG_SKIP_PAGE,
    DEVDRV_FEATURE_PCIE_LINK_INFO,
    DEVDRV_FEATURE_HCCS_LINK_INFO,
    DEVDRV_FEATURE_PRE_CFG,
    DEVDRV_FEATURE_SHR_PARA_REBUILD,
    DEVDRV_FEATURE_MAX,
};

#define DEVDRV_FEATURE_OPS_BIT_NUM 10
#define DEVDRV_FEATURE_PFVF_BIT_OFFSET 0
#define DEVDRV_FEATURE_CONNECT_PROTO_BIT_OFFSET 2
#define DEVDRV_FEATURE_ENV_BOOT_BIT_OFFSET 4

struct devdrv_feature_ops_meta {
    const char *name;
    const char *bit;
};

typedef u64 devdrv_feature_bitmap_t;
typedef u32 devdrv_feature_ops_bitmap_t;

struct devdrv_feature_meta {
    const char *name;
    const char *desc;
};

struct devdrv_feature_mgr {
    ka_mutex_t lock;
    const struct devdrv_feature_meta *meta;
    const struct devdrv_feature_ops_meta *ops_meta;
    bool inited;
};

int devdrv_feature_mgr_init(void);
void devdrv_feature_mgr_uninit(void);
bool devdrv_feature_is_support(devdrv_feature_bitmap_t bitmap, enum devdrv_feature feat);
struct devdrv_feature_mgr *devdrv_feature_get_feature_mgr(void);

#endif /* __DEVDRV_FEATURE_H__ */