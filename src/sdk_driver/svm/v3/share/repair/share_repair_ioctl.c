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
#include "ka_ioctl_pub.h"
#include "ka_task_pub.h"

#include "dpa_kernel_interface.h"
#include "pbl_feature_loader.h"
#include "pbl_uda.h"

#include "framework_cmd.h"
#include "svm_ioctl_ex.h"
#include "svm_kern_log.h"
#include "share_repair.h"

static int svm_share_repair_ioctl_para_check(u32 udevid, struct svm_share_repair_para *para)
{
    struct svm_global_va *src_va = &para->src_va;

    if ((src_va->udevid != udevid) || (src_va->va == 0) || (src_va->size == 0) ||
        (src_va->va > (U64_MAX - src_va->size)) || (udevid == uda_get_host_id())) {
        svm_err("Invalid share repair para. (udevid=%u; src_udevid=%u; va=0x%llx; size=0x%llx)\n", udevid,
                src_va->udevid, src_va->va, src_va->size);
        return -EINVAL;
    }

    return 0;
}

static int svm_share_repair_ioctl(u32 udevid, u32 cmd, unsigned long __ka_user arg)
{
    struct svm_share_repair_para para;
    int ret;
    int src_tgid;
    SVM_UNUSED(cmd);

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(para));
    if (ret != 0) {
        svm_err("Share repair copy arg from user failed. (ret=%d; arg=0x%lx)\n", ret, arg);
        return -EINVAL;
    }

    ret = svm_share_repair_ioctl_para_check(udevid, &para);
    if (ret != 0) {
        return ret;
    }

    ret = hal_kernel_apm_query_slave_tgid_by_master(ka_task_get_current_tgid(), udevid, PROCESS_CP1, &src_tgid);
    if (ret != 0) {
        svm_err("Query source slave tgid failed in share repair. (ret=%d; udevid=%u; master_tgid=%d)\n", ret, udevid,
                ka_task_get_current_tgid());
        return ret;
    }

    para.src_va.tgid = src_tgid;
    return svm_repair_share_dst(&para.src_va);
}

int svm_share_repair_ioctl_init(void)
{
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_SHARE_REPAIR), svm_share_repair_ioctl);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_share_repair_ioctl_init, FEATURE_LOADER_STAGE_4);
