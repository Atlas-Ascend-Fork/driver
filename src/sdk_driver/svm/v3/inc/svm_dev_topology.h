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

#ifndef SVM_DEV_TOPOLOGY_H
#define SVM_DEV_TOPOLOGY_H

#include "comm_kernel_interface.h"
#include "pbl_spod_info.h"
#include "pbl_soc_res.h"

#include "svm_kern_log.h"
#include "svm_pub.h"

static inline bool svm_dev_is_in_same_os(u32 local_udevid, u32 peer_udevid)
{
    /* todo: use dbl interface after dbl implementation */
    return (local_udevid == peer_udevid);
}

/* peer_server_id = SVM_INVALID_SERVER_ID means in the same server. */
static inline bool svm_is_cross_server(u32 local_udevid, u32 peer_server_id)
{
    struct spod_info info;
    u32 local_server_id = SVM_INVALID_SERVER_ID;
    int ret;

    if (peer_server_id == SVM_INVALID_SERVER_ID) {
        return false;
    }

    ret = dbl_get_spod_info(local_udevid, &info);
    if (ret != 0) {
        svm_warn("Get local server id failed. (udevid=%u; peer_server_id=%u; ret=%d)\n", local_udevid, peer_server_id,
                 ret);
    } else {
        local_server_id = info.server_id;
    }

    return (peer_server_id != local_server_id);
}

/* peer_server_id = SVM_INVALID_SERVER_ID means check topology in the same server. */
static inline bool svm_dev_is_ub_connect(u32 local_udevid, u32 peer_server_id, u32 peer_udevid)
{
    int topo_type, ret;

    if (svm_is_cross_server(local_udevid, peer_server_id)) {
        /* david cross server is UB connect */
        return true;
    }

    if (svm_dev_is_in_same_os(local_udevid, peer_udevid)) {
        return false;
    }

    if (local_udevid == uda_get_host_id() || (peer_udevid == uda_get_host_id())) {
        u32 chip_id = (local_udevid == uda_get_host_id()) ? peer_udevid : local_udevid;
        return devdrv_get_connect_protocol(chip_id) == CONNECT_PROTOCOL_UB;
    }

    ret = soc_get_dev_topology(local_udevid, peer_udevid, &topo_type);
    if (ret != 0) {
        svm_warn("Get topology failed. (udevid=%u; peer_udevid=%u; ret=%d)\n", local_udevid, peer_udevid, ret);
        return false;
    }

    return (topo_type == SOC_TOPOLOGY_UB);
}

#endif