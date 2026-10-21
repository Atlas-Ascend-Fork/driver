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
#include "ka_base_pub.h"
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_errno_pub.h"
#include "comm_kernel_interface.h"
#include "pbl_kernel_interface.h"
#include "pbl/pbl_uda.h"
#include "uba/ubcore_adapt.h"
#include "securec.h"

#include "rmo_kern_log.h"
#include "rmo_mem_sharing.h"

static uba_ubcore_device *rmo_get_ubcore_dev(u32 dev_id)
{
    ka_device_t *uda_dev = uda_get_device(dev_id);

    if (uda_dev == NULL) {
        rmo_err("Failed to get ub dev. (devid=%u)\n", dev_id);
        return NULL;
    }

    return ka_container_of(uda_dev, uba_ubcore_device, dev);
}

int rmo_mem_addr_map(u32 devid, u64 paddr, u64 size, struct rmo_mem_map_addr *mapped_addr)
{
    uba_ubcore_seg_cfg seg_cfg = {0};
    uba_ubcore_reg_seg_flag flag = {0};
    uba_ubcore_target_seg *tseg = NULL;
    uba_ubcore_device *ubc_dev = NULL;
    u32 token_val = 0;
    int ret, ret_tmp;

    if ((sizeof(uba_ubcore_seg) + sizeof(u32)) > RMO_MEM_RAW_ADDR_MAX_LEN) {
        return -EFAULT;
    }

    ret = devdrv_get_token_val(devid, &token_val);
    if (ret != 0) {
        rmo_err("Failed to get token value. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    ubc_dev = rmo_get_ubcore_dev(devid);
    if (ubc_dev == NULL) {
        return -ENODEV;
    }

    flag.bs.token_policy = UBA_UBCORE_TOKEN_PLAIN_TEXT;
    flag.bs.access = (UBA_UBCORE_ACCESS_READ | UBA_UBCORE_ACCESS_WRITE | UBA_UBCORE_ACCESS_ATOMIC);
    flag.bs.non_pin = 1;
    seg_cfg.va = (uint64_t)ka_mm_phys_to_virt(paddr);
    seg_cfg.len = size;
    seg_cfg.flag = flag;
    seg_cfg.token_value.token = token_val;

    tseg = uba_ubcore_register_seg(UBA_MODULE_TYPE_DPA, ubc_dev, &seg_cfg, NULL);
    if (KA_IS_ERR_OR_NULL(tseg)) {
        rmo_err("uba_ubcore_register_seg fail. (devid=%u)\n", devid);
        return -ENOMEM;
    }

    ret = memcpy_s(mapped_addr->raw_addr.raw_addr, RMO_MEM_RAW_ADDR_MAX_LEN, &tseg->seg, sizeof(tseg->seg));
    ret_tmp = memcpy_s(mapped_addr->raw_addr.raw_addr + sizeof(tseg->seg), RMO_MEM_RAW_ADDR_MAX_LEN - sizeof(tseg->seg),
                       &token_val, sizeof(token_val));
    if ((ret != 0) || (ret_tmp != 0)) {
        (void)uba_ubcore_unregister_seg(UBA_MODULE_TYPE_DPA, tseg);
        rmo_err("Memcpy seg failed. (len=0x%lx)\n", sizeof(tseg->seg));
        return -EFAULT;
    }

    mapped_addr->raw_addr.raw_addr_len = sizeof(tseg->seg) + sizeof(u32);
    mapped_addr->addr_ptr = (void *)tseg;
    return 0;
}

int rmo_mem_addr_unmap(u32 devid, struct rmo_mem_map_addr *mapped_addr, u64 size)
{
    if (mapped_addr->addr_ptr != NULL) {
        (void)uba_ubcore_unregister_seg(UBA_MODULE_TYPE_DPA, (uba_ubcore_target_seg *)mapped_addr->addr_ptr);
        mapped_addr->addr_ptr = NULL;
    }

    return 0;
}
