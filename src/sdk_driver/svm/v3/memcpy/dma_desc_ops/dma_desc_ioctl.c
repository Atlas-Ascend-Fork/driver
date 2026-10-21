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
#include "ka_base_pub.h"
#include "ka_compiler_pub.h"

#include "pbl_feature_loader.h"

#include "svm_pub.h"
#include "svm_kern_log.h"
#include "framework_cmd.h"
#include "copy_pub.h"
#include "svm_ioctl_ex.h"
#include "dma_desc_ctx.h"
#include "dma_desc_node.h"
#include "dma_desc_core.h"

static int dma_desc_extract_va_info_from_convert_para(struct svm_dma_desc_convert_para *para, struct copy_va_info *info)
{
    int ret;

    ret = uda_devid_to_udevid_ex(para->src_devid, &info->src_udevid);
    if (ret != 0) {
        svm_err("uda_devid_to_udevid_ex failed. (ret=%d; src_devid=%u)\n", ret, para->src_devid);
        return ret;
    }

    ret = uda_devid_to_udevid_ex(para->dst_devid, &info->dst_udevid);
    if (ret != 0) {
        svm_err("uda_devid_to_udevid_ex failed. (ret=%d; dst_devid=%u)\n", ret, para->dst_devid);
        return ret;
    }

    info->src_va = para->src_va;
    info->dst_va = para->dst_va;
    info->size = para->size;
    info->src_host_tgid = para->src_host_tgid;
    info->dst_host_tgid = para->dst_host_tgid;

    return 0;
}

static int _dma_desc_ioctl_convert(struct dma_desc_ctx *ctx, unsigned long __ka_user arg)
{
    struct svm_dma_desc_convert_para para;
    struct copy_va_info info;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(struct svm_dma_desc_convert_para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    ret = dma_desc_extract_va_info_from_convert_para(&para, &info);
    if (ret != 0) {
        return ret;
    }

    ret = copy_va_info_check(ctx->udevid, &info);
    if (ret != 0) {
        return ret;
    }

    ret = dma_desc_convert(ctx, &info, &para.dma_desc);
    if (ret != 0) {
        return ret;
    }

    ret = (int)ka_base_copy_to_user((void *)(uintptr_t)arg, (void *)&para, sizeof(struct svm_dma_desc_convert_para));
    if (ret != 0) {
        svm_err("Copy to user failed. (ret=%d)\n", ret);
        (void)dma_desc_destroy(ctx, &para.dma_desc);
        return -EINVAL;
    }

    return 0;
}

static int _dma_desc_ioctl_submit(struct dma_desc_ctx *ctx, unsigned long __ka_user arg)
{
    struct svm_dma_desc_submit_para para;
    int ret;

    ret = (int)ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(struct svm_dma_desc_submit_para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    return dma_desc_submit(ctx, &para.dma_desc, para.sync_flag);
}

static int _dma_desc_ioctl_wait(struct dma_desc_ctx *ctx, unsigned long __ka_user arg)
{
    struct svm_dma_desc_wait_para para;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(struct svm_dma_desc_wait_para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    return dma_desc_wait(ctx, &para.dma_desc);
}

static int _dma_desc_ioctl_destroy(struct dma_desc_ctx *ctx, unsigned long __ka_user arg)
{
    struct svm_dma_desc_destroy_para para;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(struct svm_dma_desc_destroy_para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    return dma_desc_destroy(ctx, &para.dma_desc);
}

static int dma_desc_ioctl_convert(u32 udevid, u32 cmd, unsigned long __ka_user arg)
{
    struct dma_desc_ctx *ctx = NULL;
    int ret;

    ctx = dma_desc_ctx_get(udevid, ka_task_get_current_tgid());
    if (ctx == NULL) {
        return -EINVAL;
    }

    ret = _dma_desc_ioctl_convert(ctx, arg);
    dma_desc_ctx_put(ctx);
    return ret;
}

static int dma_desc_ioctl_submit(u32 udevid, u32 cmd, unsigned long __ka_user arg)
{
    struct dma_desc_ctx *ctx = NULL;
    int ret;

    ctx = dma_desc_ctx_get(udevid, ka_task_get_current_tgid());
    if (ctx == NULL) {
        return -EINVAL;
    }

    ret = _dma_desc_ioctl_submit(ctx, arg);
    dma_desc_ctx_put(ctx);
    return ret;
}

static int dma_desc_ioctl_wait(u32 udevid, u32 cmd, unsigned long __ka_user arg)
{
    struct dma_desc_ctx *ctx = NULL;
    int ret;

    ctx = dma_desc_ctx_get(udevid, ka_task_get_current_tgid());
    if (ctx == NULL) {
        return -EINVAL;
    }

    ret = _dma_desc_ioctl_wait(ctx, arg);
    dma_desc_ctx_put(ctx);
    return ret;
}

static int dma_desc_ioctl_destroy(u32 udevid, u32 cmd, unsigned long __ka_user arg)
{
    struct dma_desc_ctx *ctx = NULL;
    int ret;

    ctx = dma_desc_ctx_get(udevid, ka_task_get_current_tgid());
    if (ctx == NULL) {
        return -EINVAL;
    }

    ret = _dma_desc_ioctl_destroy(ctx, arg);
    dma_desc_ctx_put(ctx);
    return ret;
}

int dma_desc_ioctl_init(void)
{
#ifdef CFG_FEATURE_DMA_DESC_SUBMIT
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_DMA_DESC_SUBMIT), dma_desc_ioctl_submit);
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_DMA_DESC_WAIT), dma_desc_ioctl_wait);
#else
    SVM_UNUSED(dma_desc_ioctl_submit);
    SVM_UNUSED(dma_desc_ioctl_wait);
#endif

    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_DMA_DESC_CONVERT), dma_desc_ioctl_convert);
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_DMA_DESC_DESTROY), dma_desc_ioctl_destroy);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dma_desc_ioctl_init, FEATURE_LOADER_STAGE_6);
