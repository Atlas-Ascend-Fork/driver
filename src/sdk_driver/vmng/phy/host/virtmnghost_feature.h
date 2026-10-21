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

#ifndef VIRTMNG_FEATURE_H
#define VIRTMNG_FEATURE_H

#include "ka_type.h"
#include "ka_base_pub.h"

/*
 * vmng module extension features of a chip.
 * Each chip type owns a feature bitmap, which is initialized in
 * vmngh_init_pci_pdev based on chip type when a PF device is going online.
 */
enum vmng_feature {
    VMNG_FEATURE_BW_CTRL,       /* bandwidth control */
    VMNG_FEATURE_MIA_RES,       /* mia device resource setting in host */
    VMNG_FEATURE_VM_FULL_VF_EN, /* vm_full_vf_enable sysfs node */
    VMNG_FEATURE_SMP,           /* chip supports SMP deploy mode, need AMP check before vdev create */
    VMNG_FEATURE_MAX,
};

typedef u64 vmng_feature_bitmap_t;
vmng_feature_bitmap_t vmngh_feature_init(u32 dev_id);
bool vmngh_feature_not_support(u32 dev_id, enum vmng_feature feat);

#endif /* VIRTMNG_FEATURE_H */
