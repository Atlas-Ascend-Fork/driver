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

#include "pbl_uda.h"
#include "ka_system_pub.h"
#include "uda_access_adapt.h"
#include "uda_pub_def.h"

void uda_init_phy_dev_num(void)
{
#ifndef DRV_HOST
    u32 cpu_count = ka_system_num_online_cpus();
    int node_id;

    if (cpu_count == 0) {
        uda_err("No online CPUs found, use default phy dev num.\n");
        return;
    }

    node_id = ka_system_cpu_to_node((int)cpu_count - 1);
    if (node_id < 0) {
        uda_err("Failed to get NUMA node ID (node_id=%d), use default phy dev num.\n", node_id);
        return;
    }

    (void)uda_set_detected_phy_dev_num((u32)node_id + 1);
#endif
}