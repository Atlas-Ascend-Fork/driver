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

#include "ka_ioctl_pub.h"
#include "ka_memory_pub.h"
#include "ka_common_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_compiler_pub.h"
#include "framework_cmd.h"
#include "framework_vma.h"
#include "pbl_feature_loader.h"
#include "dpa_kernel_interface.h"
#include "pbl_uda.h"
#include "svm_pgtable.h"
#include "soma_ioctl.h"
#include "soma_flag.h"
#include "svm_gfp.h"
#include "svm_ioctl_ex.h"
#include "svm_kern_log.h"
#include "svm_slab.h"
#include "pmm.h"
#include "ksvmm.h"
#include "svm_smp.h"
#include "soma_mem.h"

static int soma_comm_para_check(u32 cmd, u64 va, u64 size)
{
    if ((va == 0) || (size == 0) || (va + size < va)) {
        svm_err(SOMA_LOG_TAG "para check failed. (va=0x%llx; size=0x%llx;)\n", va, size);
        return -EINVAL;
    }

    if ((SVM_IS_ALIGNED(va, KA_HPAGE_SIZE) == false) || (SVM_IS_ALIGNED(size, KA_HPAGE_SIZE) == false)) {
        svm_err(SOMA_LOG_TAG "Va or size isn't aligned by page_size. (va=0x%llx; size=%llu; page_size=%llu)\n", va,
                size, KA_HPAGE_SIZE);
        return -EINVAL;
    }

    return 0;
}

static int svm_soma_mem_free_config(u32 udevid, u32 cmd, struct svm_soma_mem_cfg_para *para)
{
    int ret;
    u64 va = para->va;
    u64 size = para->size;

    ret = soma_comm_para_check(cmd, va, size);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "Invalid para. (udevid=%u; cmd=%u; va=%llx; size=%llx)\n", udevid, cmd, va, size);
        return -EINVAL;
    }

    ret = ksvmm_del_seg(udevid, ka_task_get_current_tgid(), va);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "mem del seg failed in soma mem free config. (udevid=%u; dst_va=0x%llx; dst_size=%llu)\n",
                udevid, va, size);
    }

    return ret;
}

static int svm_soma_mem_malloc_config(u32 udevid, u32 cmd, struct svm_soma_mem_cfg_para *para)
{
    int ret;
    u64 va = para->va;
    u64 size = para->size;

    ret = soma_comm_para_check(cmd, va, size);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "Invalid para. (udevid=%u; cmd=%u; va=%llx; size=%llx)\n", udevid, cmd, va, size);
        return -EINVAL;
    }

    ret = ksvmm_add_seg(udevid, ka_task_get_current_tgid(), va, &para->src_info);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG
                "mem add seg failed in soma mem malloc config. (udevid=%u; dst_va=0x%llx; dst_size=%llu)\n",
                udevid, va, size);
    }

    return ret;
}

static int svm_soma_global_va_malloc_config(u32 udevid, u32 cmd, struct svm_soma_mem_cfg_para *para)
{
    int ret;
    u64 va = para->va;
    u64 size = para->size;

    ret = soma_comm_para_check(cmd, va, size);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "Invalid para. (udevid=%u; cmd=%u; va=%llx; size=%llx)\n", udevid, cmd, va, size);
        return -EINVAL;
    }

    ret = svm_smp_add_mem(udevid, ka_task_get_current_tgid(), va, size, 0);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "Smp add mem failed in soma global va config. (udevid=%u; va=0x%llx; size=%llu)\n", udevid,
                va, size);
    }
    return ret;
}

static int svm_soma_global_va_delete_config(u32 udevid, u32 cmd, struct svm_soma_mem_cfg_para *para)
{
    int ret;
    u64 va = para->va;
    u64 size = para->size;

    ret = soma_comm_para_check(cmd, va, size);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "Invalid para. (udevid=%u; cmd=%u; va=%llx; size=%llx)\n", udevid, cmd, va, size);
        return -EINVAL;
    }

    ret = svm_smp_del_mem(udevid, ka_task_get_current_tgid(), va);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "Smp delete mem failed in soma global va config. (udevid=%u; va=0x%llx;)\n", udevid, va);
    }
    return ret;
}

static bool svm_soma_mem_flag_is_valid(u32 flag)
{
    return (flag == SVM_SOMA_GLOBAL_VA_ALLOC) || (flag == SVM_SOMA_GLOBAL_VA_DELETE) || (flag == SVM_SOMA_MEM_ALLOC) ||
           (flag == SVM_SOMA_MEM_FREE) || (flag == SVM_SOMA_GET_CFG);
}

static int svm_soma_src_info_check(u32 udevid, const struct svm_soma_mem_cfg_para *para)
{
    int current_tgid = ka_task_get_current_tgid();

    if (!svm_soma_mem_flag_is_valid(para->flag)) {
        svm_err(SOMA_LOG_TAG "ioctl flag not support. (flag=%u)\n", para->flag);
        return -EINVAL;
    }

    if (para->flag == SVM_SOMA_GET_CFG && para->va == 0) {
        svm_err(SOMA_LOG_TAG "src_info invalid. (para_va=0x%llx; current_tgid=%d)\n", para->va, current_tgid);
        return -EINVAL;
    }

    if (para->flag != SVM_SOMA_GET_CFG) {
        if ((para->src_info.va == 0) || (para->src_info.size == 0) || (para->src_info.size != para->size)) {
            svm_err(SOMA_LOG_TAG "src_info invalid. (src_va=0x%llx; src_size=%llu; size=%llu; src_tgid=%d; "
                                 "current_tgid=%d)\n",
                    para->src_info.va, para->src_info.size, para->size, para->src_info.tgid, current_tgid);
            return -EINVAL;
        }
    }

    return 0;
}

static int svm_soma_ioctl_para_check(u32 cmd, unsigned long arg)
{
    if (cmd != SVM_SOMA_MEM_CFG) {
        svm_err(SOMA_LOG_TAG "cmd invalid. (cmd=%u)\n", cmd);
        return -EINVAL;
    }

    if (_IOC_SIZE(cmd) != sizeof(struct svm_soma_mem_cfg_para)) {
        svm_err(SOMA_LOG_TAG "_IOC_SIZE invalid. (cmd=%u; size=%u)\n", cmd, _IOC_SIZE(cmd));
        return -EINVAL;
    }

    if (arg == 0) {
        svm_err(SOMA_LOG_TAG "arg invalid.\n");
        return -EINVAL;
    }

    return 0;
}

static int svm_soma_ioctl_get_config(u32 udevid, u32 cmd, struct svm_soma_mem_cfg_para *para)
{
    int ret;
    u64 start;

    ret = ksvmm_get_seg(udevid, ka_task_get_current_tgid(), para->va, &start, &para->src_info);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "mem get seg failed in soma sync free config. (udevid=%u; dst_va=0x%llx)\n", udevid,
                para->va);
        return ret;
    }

    return ret;
}

static int svm_soma_ioctl_comm(u32 udevid, u32 cmd, unsigned long arg)
{
    struct svm_soma_mem_cfg_para para;
    int ret;

    ret = svm_soma_ioctl_para_check(cmd, arg);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "ioctl param check failed. (ret=%d; arg=0x%lx)\n", ret, arg);
        return -EINVAL;
    }

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(struct svm_soma_mem_cfg_para));
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "copy arg from user failed. (ret=%d; arg=0x%lx)\n", ret, arg);
        return -EINVAL;
    }

    ret = svm_soma_src_info_check(udevid, &para);
    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "src info check failed. (flag=%u)\n", para.flag);
        return ret;
    }

    switch (para.flag) {
        case SVM_SOMA_GLOBAL_VA_ALLOC:
            ret = svm_soma_global_va_malloc_config(udevid, cmd, &para);
            break;
        case SVM_SOMA_GLOBAL_VA_DELETE:
            ret = svm_soma_global_va_delete_config(udevid, cmd, &para);
            break;
        case SVM_SOMA_MEM_ALLOC:
            ret = svm_soma_mem_malloc_config(udevid, cmd, &para);
            break;
        case SVM_SOMA_MEM_FREE:
            ret = svm_soma_mem_free_config(udevid, cmd, &para);
            break;
        case SVM_SOMA_GET_CFG:
            ret = svm_soma_ioctl_get_config(udevid, cmd, &para);
            if (ret == 0) {
                unsigned long retCpy = ka_base_copy_to_user((void __ka_user *)(uintptr_t)arg, &para, sizeof(para));
                if (retCpy != 0) {
                    svm_err(SOMA_LOG_TAG "copy arg to user failed. (ret=%lu; arg=0x%lx)\n", retCpy, arg);
                    return -EINVAL;
                }
            }
            break;
        default:
            return -EINVAL;
    }

    if (ret != 0) {
        svm_err(SOMA_LOG_TAG "ioctl process failed. (ret=%d)\n", ret);
    }

    return ret;
}

static int svm_soma_feature_init(void)
{
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_SOMA_MEM_CFG), svm_soma_ioctl_comm);
    return 0;
}

DECLAER_FEATURE_AUTO_INIT(svm_soma_feature_init, FEATURE_LOADER_STAGE_6);
