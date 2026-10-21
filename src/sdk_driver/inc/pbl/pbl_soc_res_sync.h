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

#ifndef PBL_SOC_RES_SYNC_H
#define PBL_SOC_RES_SYNC_H

#include "ka_memory_pub.h"
#include "pbl_soc_res_sync_cmd.h"

struct res_sync_irq {
    int irq_type;
    u32 num;
};

/* res type is addr, need encode and decode func
   pcie connect:
        device: encode addr is device address, output encode_addr is 'bar << 32 + rxatu base addr'
        host: decode output host address is 'bar addr + rxatu base addr'
   hccs connect:
        to be add
   ub connect:
        device: encode not change addr
        host: decode query iodecode host address from ub driver
*/
typedef int (*soc_res_addr_encode)(u32 udevid, u64 addr, u64 len, u64 *encode_addr);
typedef int (*soc_res_addr_decode)(u32 udevid, u64 encode_addr, u64 len, u64 *addr);

u32 soc_res_sync_get_sub_num(u32 udevid, enum soc_res_sync_scope scope);
/* *len: input buf len, output extract buf len */
int soc_res_extract(u32 udevid, struct res_sync_target *target, char *buf, u32 *len, soc_res_addr_encode func);
int soc_res_inject(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len, soc_res_addr_decode func);

static inline int _soc_res_sync_type(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len,
                                     int (*sync)(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len))
{
    for (target->type = 0; target->type < SOC_RES_TYPE_MAX; target->type++) {
        int ret = sync(udevid, target, buf, buf_len);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static inline int _soc_res_sync_scope(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len,
                                      int (*sync)(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len))
{
    u32 sub_num = soc_res_sync_get_sub_num(udevid, target->scope);
    for (target->id = 0; target->id < sub_num; target->id++) {
        int ret = _soc_res_sync_type(udevid, target, buf, buf_len, sync);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static inline int _soc_res_sync(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len,
                                int (*sync)(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len))
{
    for (target->scope = 0; target->scope < SOC_SCOPE_MAX; target->scope++) {
        int ret = _soc_res_sync_scope(udevid, target, buf, buf_len, sync);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

#define RES_SYNC_BUF_LEN 2048
static inline int soc_res_sync(u32 udevid, u32 dst_udevid, enum soc_res_sync_dir dir,
                               int (*sync)(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len))
{
    int ret = -ENOMEM;
    char *buf = ka_mm_kzalloc(RES_SYNC_BUF_LEN, KA_GFP_KERNEL);
    if (buf != NULL) {
        struct res_sync_target target;
        target.dir = dir;
        target.dst_udevid = dst_udevid;
        ret = _soc_res_sync(udevid, &target, buf, RES_SYNC_BUF_LEN, sync);
        ka_mm_kfree(buf);
    }

    return ret;
}

static inline int soc_res_sync_d2h(u32 udevid,
                                   int (*sync)(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len))
{
    return soc_res_sync(udevid, udevid, SOC_DIR_D2H, sync);
}

static inline int _soc_res_sync_p2v(u32 udevid, struct res_sync_target *target, char *buf, u32 buf_len)
{
    int ret = 0;

    if (target->type != SOC_IRQ_RES) {
        u32 out_len = buf_len;
        ret = soc_res_extract(udevid, target, buf, &out_len, NULL);
        if ((ret == 0) && (out_len > 0)) {
            ret = soc_res_inject(target->dst_udevid, target, buf, out_len, NULL);
        }
    }

    return ret;
}

static inline int soc_res_sync_p2v(u32 phy_udevid, u32 vf_udevid)
{
    return soc_res_sync(phy_udevid, vf_udevid, SOC_DIR_P2V, _soc_res_sync_p2v);
}
#endif
