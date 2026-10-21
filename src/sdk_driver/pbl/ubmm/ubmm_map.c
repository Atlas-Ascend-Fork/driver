/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "ka_memory_pub.h"
#include "ka_common_pub.h"
#include "ka_kernel_def_pub.h"

#include "uba/ummu_core_adapt.h"
#include "pbl/pbl_soc_res.h"
#include "pbl_uda.h"

#include "ubmm_log.h"
#include "ubmm_map.h"

static ka_device_t *ubmm_device = NULL;

ka_device_t *ubmm_ummu_tdev_attr_init(u32 tid)
{
#ifdef DRV_HOST
    uba_hisi_ummu_tdev_info info;
#endif
    uba_tdev_attr attr;

    uba_tdev_attr_init(&attr);
#ifdef DRV_HOST
    attr.priv = (u8 *)&info;
    attr.priv_len = sizeof(info);
    info.v2.on_chip = false;
    info.v2.tid = tid;
#endif
    return uba_ummu_core_alloc_tdev(UBA_MODULE_TYPE_SVM, &attr, (u32 *)(uintptr_t)&tid);
}

int _ubmm_init_map(u32 udevid)
{
    ka_device_t *device = NULL;
    u32 udevid_ex = 0;
    u64 ub_tid;
    int ret;

    if (ubmm_device != NULL) {
        return 0;
    }

    (void)uda_get_cur_ns_udevids(&udevid_ex, 1);
    udevid_ex = (udevid == uda_get_host_id()) ? udevid_ex : udevid; /* Not host_id real dev, change to first udevid. */
    ret = soc_resmng_dev_get_key_value(udevid_ex, "UB_TID", &ub_tid);
    if (ret != 0) {
        return -EAGAIN;
    }

    device = ubmm_ummu_tdev_attr_init(ub_tid);
    if (device == NULL) {
        ubmm_err("uba_ummu_core_alloc_tdev failed.\n");
        return -ENODEV;
    }

    ubmm_device = device;
    return 0;
}

int ubmm_init_map(u32 udevid)
{
    static KA_TASK_DEFINE_MUTEX(mutex);
    int ret;

    ka_task_mutex_lock(&mutex);
    ret = _ubmm_init_map(udevid);
    ka_task_mutex_unlock(&mutex);
    return ret;
}

void ubmm_uninit_map(u32 udevid)
{
    if (ubmm_device != NULL) {
        uba_ummu_core_free_tdev(UBA_MODULE_TYPE_SVM, ubmm_device);
        ubmm_device = NULL;
    }
}

static ka_iommu_domain_t *ubmm_get_iommu_domain(void)
{
    if (ubmm_device == NULL) {
        return NULL;
    }

    return ka_mm_iommu_get_domain_for_dev(ubmm_device);
}

static int ubmm_map_check_align(u64 uba, u64 size, struct ubmm_pa_seg *pa_seg, u64 seg_num)
{
    u64 i;

    if ((KA_MM_IS_ALIGNED(uba, KA_MM_PAGE_SIZE) == false) || (KA_MM_IS_ALIGNED(size, KA_MM_PAGE_SIZE) == false)) {
        ubmm_err("Uba should be aligned by PAGE_SIZE.\n");
        return -EINVAL;
    }
#ifndef EMU_ST /* todo: emu_st support alloc hpage size */
    for (i = 0; i < seg_num; i++) {
        if ((KA_MM_IS_ALIGNED(pa_seg[i].pa, KA_MM_PAGE_SIZE) == false) ||
            (KA_MM_IS_ALIGNED(pa_seg[i].size, KA_MM_PAGE_SIZE) == false)) {
            ubmm_err("Pa seg should be aligned by PAGE_SIZE. (i=%llu; pa_low_8bit=0x%llx; size=%llu)\n", i,
                     (pa_seg[i].pa & 0xffULL), pa_seg[i].size);
            return -EINVAL;
        }
    }
#endif
    return 0;
}

static int _ubmm_unmap(ka_iommu_domain_t *domain, u64 uba, u64 size)
{
    u64 unmapped_size;

    unmapped_size = ka_mm_iommu_unmap(domain, uba, size);
    return (unmapped_size != size) ? -EINVAL : 0;
}

static int _ubmm_map(ka_iommu_domain_t *domain, u64 uba, u64 size, struct ubmm_pa_seg *pa_seg, u64 seg_num)
{
    u64 i, map_size = 0;
    int ret;

    for (i = 0; i < seg_num; i++) {
        ret = ka_mm_iommu_map(domain, uba + map_size, pa_seg[i].pa, pa_seg[i].size,
                              KA_IOMMU_READ | KA_IOMMU_WRITE | KA_IOMMU_CACHE,
                              KA_GFP_KERNEL | __KA_GFP_NOWARN | __KA_GFP_ACCOUNT);
        if (ret != 0) {
            ubmm_err("Iommu_map failed. (ret=%d)\n", ret);
            (void)_ubmm_unmap(domain, uba, map_size);
            return ret;
        }
        map_size += pa_seg[i].size;
    }

    return 0;
}

int ubmm_map(u32 udevid, u64 uba, u64 size, struct ubmm_pa_seg *pa_seg, u64 seg_num)
{
    ka_iommu_domain_t *domain = NULL;
    int ret;

    ret = ubmm_map_check_align(uba, size, pa_seg, seg_num);
    if (ret != 0) {
        return ret;
    }

    domain = ubmm_get_iommu_domain();
    if (domain == NULL) {
        ubmm_err("Get iommu domain failed.\n");
        return -EINVAL;
    }

    return _ubmm_map(domain, uba, size, pa_seg, seg_num);
}
KA_EXPORT_SYMBOL_GPL(ubmm_map);

int ubmm_unmap(u32 udevid, u64 uba, u64 size)
{
    ka_iommu_domain_t *domain = NULL;

    domain = ubmm_get_iommu_domain();
    if (domain == NULL) {
        ubmm_err("Get iommu domain failed.\n");
        return -EINVAL;
    }

    return _ubmm_unmap(domain, uba, size);
}
KA_EXPORT_SYMBOL_GPL(ubmm_unmap);
