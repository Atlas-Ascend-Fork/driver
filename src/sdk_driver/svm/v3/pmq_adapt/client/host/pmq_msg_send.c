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
#include "pbl_uda.h"

#include "svm_pub.h"
#include "svm_kern_log.h"
#include "kmc_h2d.h"
#include "pmq_msg.h"

int pmq_msg_send(u32 src_udevid, u32 dst_udevid, void *msg, u32 msg_len, u32 *reply_len)
{
    if (dst_udevid == uda_get_host_id()) {
        svm_err("Invalid dst_udevid, can't be host_udevid. (dst_udevid=%u)\n", dst_udevid);
        return -EINVAL;
    } else if (src_udevid == uda_get_host_id()) {
        return svm_kmc_h2d_send(dst_udevid, msg, msg_len, *reply_len, reply_len);
    } else {
        svm_err("Invalid udevid. (src_udevid=%u; dst_udevid=%u)\n", src_udevid, dst_udevid);
        return -EINVAL;
    }
}

u32 pmq_get_max_msg_len(void)
{
    return svm_kmc_h2d_get_max_msg_len();
}