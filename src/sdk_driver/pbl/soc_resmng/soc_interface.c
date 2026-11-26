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
#include "ka_kernel_def_pub.h"
#include "ka_base_pub.h"
#include "securec.h"
#include "ascend_kernel_hal.h"
#include "ascend_platform.h"
#include "ascend_dev_num.h"
#include "soc_resmng_log.h"
#include "pbl/pbl_soc_res.h"
#include "pbl/pbl_uda.h"
#include <linux/bitops.h>
#include <linux/cpumask.h>
#include "soc_interface_adapt.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

int hal_kernel_get_pg_info(unsigned int dev_id, HAL_PG_INFO_TYPE info_type, char *data, unsigned int size,
                           unsigned int *ret_size)
{
    return hal_kernel_get_pg_info_adapt(dev_id, info_type, data, size, ret_size);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_pg_info);

#ifndef CFG_EDGE_HOST
STATIC int dms_pf_get_cpu_topo(struct cpu_topology_info *cpu_topo)
{
    const struct cpumask *mask;
    unsigned int i = 0;
    cpu_topo->total_nums = num_online_cpus();

    if (cpu_topo->total_nums > HAL_KERNEL_MAX_CPU_TOPO_NUM) {
        soc_err("cpu num exceeds max topo num. (cpu_total_nums=%u; max_topo_num=%u)\n", cpu_topo->total_nums,
                HAL_KERNEL_MAX_CPU_TOPO_NUM);
        return -EINVAL;
    }

    for (; i < cpu_topo->total_nums; i++) {
        mask = topology_sibling_cpumask(i);
        cpu_topo->single_cpu_topo[i].cpu_id = (unsigned char)i;
        cpu_topo->single_cpu_topo[i].cpu_mask = *(unsigned long long *)cpumask_bits(mask);
        cpu_topo->single_cpu_topo[i].phy_cpu_id = (unsigned char)topology_core_id(i);
        if (cpumask_weight(mask) > 1) {
            cpu_topo->single_cpu_topo[i].is_share = 1;
        }
        cpu_topo->single_cpu_topo[i].hyperthread_id = (i == cpumask_first(mask)) ? 0 : 1;
    }
    return 0;
}

STATIC int dms_vf_get_cpu_topo(uint32_t dev_id, struct cpu_topology_info *cpu_topo)
{
    const struct cpumask *mask;
    int ret = 0;
    unsigned int i = 0;
    int id = 0;
    struct soc_mia_res_info_ex info = {0};
    ret = soc_resmng_dev_get_mia_res_ex(dev_id, MIA_CPU_DEV_ACPU, &info);
    if (ret != 0) {
        soc_err("Failed to get aicpu info. (ret=%d; dev_id=%u;)\n", ret, dev_id);
        return ret;
    }

    if (info.total_num > HAL_KERNEL_MAX_CPU_TOPO_NUM) {
        soc_err("ai cpu total num exceeds max topo num. (dev_id=%u; cpu_total_nums=%u; max_topo_num=%u)\n", dev_id,
                info.total_num, HAL_KERNEL_MAX_CPU_TOPO_NUM);
        return -EINVAL;
    }

    cpu_topo->total_nums = info.total_num;
    id = __ka_base_ffs(info.bitmap);
    for (; i < cpu_topo->total_nums; i++) {
        if (id >= nr_cpu_ids) {
            soc_err("ai cpu id over max cpu id. (dev_id=%u; id=%u; max_cpu_id=%u)\n", dev_id, id, nr_cpu_ids);
            return -EINVAL;
        }
        mask = topology_sibling_cpumask(id);
        cpu_topo->single_cpu_topo[i].cpu_id = (unsigned char)id;
        cpu_topo->single_cpu_topo[i].cpu_mask = *(unsigned long long *)cpumask_bits(mask);
        cpu_topo->single_cpu_topo[i].phy_cpu_id = (unsigned char)topology_core_id(id);
        if (cpumask_weight(mask) > 1) {
            cpu_topo->single_cpu_topo[i].is_share = 1;
        }
        cpu_topo->single_cpu_topo[i].hyperthread_id = (id == cpumask_first(mask)) ? 0 : 1;
        id++;
    }
    return 0;
}
#endif

int hal_kernel_get_cpu_topo(uint32_t dev_id, struct cpu_topology_info *res)
{
#ifndef CFG_EDGE_HOST
    int ret;
    struct cpu_topology_info cpu_topo = {0};

    if (res == NULL) {
        soc_err("res is NULL.\n");
        return -EINVAL;
    }
    if (!uda_is_udevid_exist(dev_id)) {
        soc_err("Device id does not exist. (dev_id=%u;)\n", dev_id);
        return -ENODEV;
    }
    if (uda_is_pf_dev(dev_id)) {
        ret = dms_pf_get_cpu_topo(&cpu_topo);
        if (ret != 0) {
            soc_err("Failed to get pf cpu_topo. (ret=%d; dev_id=%u)\n", ret, dev_id);
            return ret;
        }
    } else {
        ret = dms_vf_get_cpu_topo(dev_id, &cpu_topo);
        if (ret != 0) {
            soc_err("Failed to get vf cpu_topo. (ret=%d; dev_id=%u;)\n", ret, dev_id);
            return ret;
        }
    }
    ret = memcpy_s(res, sizeof(struct cpu_topology_info), &cpu_topo, sizeof(cpu_topo));
    if (ret != 0) {
        soc_err("Failed to copy cpu_topo. (ret=%d; dev_id=%u;)\n", ret, dev_id);
        return -ENOMEM;
    }
    return 0;
#else
    (void)dev_id;
    (void)res;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_cpu_topo);