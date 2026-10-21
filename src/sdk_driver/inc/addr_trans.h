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

#ifndef ADDR_TRANS_H
#define ADDR_TRANS_H

#include <linux/types.h>

#define DEVDRV_ADDR_DESC_MODE_DEFAULT 0
#define DEVDRV_ADDR_DESC_MODE_ONEPATH 1
#define DEVDRV_ADDR_DESC_MODE_MULTIPATH 2

struct svm_peer_ub_addr {
    u32 mem_id;
    u64 offset;
    int mode;
    int cross_server;
};

struct devdrv_addr_desc {
    union {
        struct svm_peer_ub_addr peer_ub;
        u64 pa;
    };
    u64 size;
};

/* h2d, d2h. support latter */
int devdrv_addr_trans_local_to_peer(u32 udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr);
int devdrv_addr_trans_peer_to_local(u32 udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr);

/* p2p, peer_udevid is host view udevid */
int devdrv_addr_trans_p2p_peer_to_local(u32 udevid, u32 peer_udevid, struct devdrv_addr_desc *addr_desc,
                                        u64 *trans_addr);

/* p2p, cs: cross server */
int devdrv_addr_trans_cs_p2p_peer_to_local(u32 udevid, u32 peer_sdid, struct devdrv_addr_desc *addr_desc,
                                           u64 *trans_addr);

/* addr trans capability type, supported means the related mem decoder info query succeeded */
enum devdrv_addr_trans_cap_type {
    DEVDRV_ADDR_TRANS_CAP_D2H = 0, /* device access host ub mem, depends on host mem cfg info */
    DEVDRV_ADDR_TRANS_CAP_H2D,     /* host access device ub mem, depends on device mem decoder info */
    DEVDRV_ADDR_TRANS_CAP_TYPE_MAX
};

/* query whether the addr trans capability is supported */
bool devdrv_query_addr_trans_support(enum devdrv_addr_trans_cap_type cap_type);

#endif
