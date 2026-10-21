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
#include "pbl_task_ctx.h"
#include "dpa_kernel_interface.h"
#include "pbl_uda.h"

#include "svm_pub.h"
#include "svm_kern_log.h"
#include "framework_cmd.h"
#include "svm_ioctl_ex.h"
#include "copy_pub.h"
#include "va_mng.h"
#include "dma_desc_ctx.h"
#include "dma_desc_node.h"
#include "dma_desc_core.h"

#define SVM_CONVERT2D_64M_SIZE (64ULL * SVM_BYTES_PER_MB)
#define SVM_CONVERT2D_128M_SIZE (128ULL * SVM_BYTES_PER_MB)
#define SVM_CONVERT2D_DMA_DEPTH (32ULL * SVM_BYTES_PER_KB)
#define SVM_CONVERT2D_HEIGHT_MAX (5ULL * SVM_BYTES_PER_MB)

static inline int dma_desc_convert_2d_para_check(struct svm_dma_desc_convert_2d_para *para)
{
    u64 svm_va, svm_size;

    if ((para->width > para->dpitch) || (para->width > para->spitch)) {
        svm_err("Dpitch and spitch should both be larger than width. (dpitch=%llu; spitch=%llu; width=%llu)\n",
                para->dpitch, para->spitch, para->width);
        return -EINVAL;
    }

    if ((para->width == 0) || (para->height == 0)) {
        svm_err("Width & height para invalid. (width=%llu; height=%llu)\n", para->width, para->height);
        return -EINVAL;
    }

    if (para->height > SVM_CONVERT2D_HEIGHT_MAX) {
        svm_err("Height is larger than max. (height=%llu; max=%llu)\n", para->height, SVM_CONVERT2D_HEIGHT_MAX);
        return -EINVAL;
    }

    svm_get_va_range(&svm_va, &svm_size);
    if (para->width >= svm_size) {
        svm_err("Invalid width. (width=%llu)\n", para->width);
        return -EINVAL;
    }

    if (para->fixed_size > (para->width * para->height)) {
        svm_err("Fixed_size should be smaller than width*height. (fixed_size=%llu; width=%llu; height=%llu)\n",
                para->fixed_size, para->width, para->height);
        return -EINVAL;
    }

    return 0;
}

static void svm_make_convrt2d_input(struct svm_dma_desc_convert_2d_para *para)
{
    u32 one_addr_limit_size = SVM_CONVERT2D_128M_SIZE;
    u32 convert_64m_size = SVM_CONVERT2D_64M_SIZE;
    u64 finished_height, width_offset, width_left;
    u64 spitch = para->spitch;
    u64 dpitch = para->dpitch;
    u64 width = para->width;
    u64 height = para->height;
    u64 fixed_size = para->fixed_size;

    finished_height = fixed_size / width;
    width_offset = fixed_size % width;
    width_left = width - width_offset;
    para->src_va = para->src_va + finished_height * spitch + width_offset;
    para->dst_va = para->dst_va + finished_height * dpitch + width_offset;
    para->fixed_size = 0;
    if (width <= one_addr_limit_size) {
        if (width_offset != 0) {
            para->width = width_left;
            /* convert the left len in one addr, set height=1 */
            para->height = 1;
        } else {
            para->height = height - finished_height;
        }
    } else {
        if (width_left < one_addr_limit_size) {
            para->width = width_left;
            /* convert the left len in one addr, set height=1 */
            para->height = 1;
        } else {
            para->width = convert_64m_size;
            para->height = width_left / convert_64m_size;
            para->spitch = para->width;
            para->dpitch = para->width;
        }
    }
    if (para->height > SVM_CONVERT2D_DMA_DEPTH) {
        para->height = SVM_CONVERT2D_DMA_DEPTH;
    }
}

static int dma_desc_extract_va_info_from_convert_2d_para(struct svm_dma_desc_convert_2d_para *para,
                                                         struct copy_2d_va_info *info)
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
    info->spitch = para->spitch;
    info->dpitch = para->dpitch;
    info->width = para->width;
    info->height = para->height;

    return 0;
}

static int _dma_desc_ioctl_convert_2d(struct dma_desc_ctx *ctx, unsigned long __ka_user arg)
{
    struct svm_dma_desc_convert_2d_para para;
    struct copy_2d_va_info info;
    int ret;

    ret = ka_base_copy_from_user(&para, (void __ka_user *)(uintptr_t)arg, sizeof(struct svm_dma_desc_convert_2d_para));
    if (ret != 0) {
        svm_err("Copy_from_user fail.\n");
        return -EINVAL;
    }

    ret = dma_desc_convert_2d_para_check(&para);
    if (ret != 0) {
        return ret;
    }
    svm_make_convrt2d_input(&para);

    ret = dma_desc_extract_va_info_from_convert_2d_para(&para, &info);
    if (ret != 0) {
        return ret;
    }

    ret = dma_desc_convert_2d(ctx, &info, &para.dma_desc);
    if (ret != 0) {
        return ret;
    }

    ret = (int)ka_base_copy_to_user((void *)(uintptr_t)arg, (void *)&para, sizeof(struct svm_dma_desc_convert_2d_para));
    if (ret != 0) {
        svm_err("Copy to user failed. (ret=%d)\n", ret);
        dma_desc_destroy(ctx, &para.dma_desc);
        return -EINVAL;
    }

    return 0;
}

static int dma_desc_ioctl_convert_2d(u32 udevid, u32 cmd, unsigned long __ka_user arg)
{
    struct dma_desc_ctx *ctx = NULL;
    int ret;

    ctx = dma_desc_ctx_get(udevid, ka_task_get_current_tgid());
    if (ctx == NULL) {
        return -EINVAL;
    }

    ret = _dma_desc_ioctl_convert_2d(ctx, arg);
    dma_desc_ctx_put(ctx);
    return ret;
}

int dma_desc_2d_ioctl_init(void)
{
    svm_register_ioctl_cmd_handle(_KA_IOC_NR(SVM_DMA_DESC_CONVERT_2D), dma_desc_ioctl_convert_2d);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dma_desc_2d_ioctl_init, FEATURE_LOADER_STAGE_6);
