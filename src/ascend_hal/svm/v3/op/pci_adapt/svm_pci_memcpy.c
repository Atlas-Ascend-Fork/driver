/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/ioctl.h>

#include "svm_pub.h"
#include "svm_log.h"
#include "svm_ioctl_ex.h"
#include "svm_sys_cmd.h"
#include "svm_user_adapt.h"
#include "svm_memcpy.h"
#include "svm_sys_cmd.h"
#include "svm_memcpy_ops_register.h"
#include "svm_memcpy.h"

#define SVM_COPY_SYNC_SIZE (512 * SVM_BYTES_PER_KB)

static inline bool svm_pci_memcpy_judge_sync(u64 size)
{
    return size <= SVM_COPY_SYNC_SIZE;
}

static int _svm_pci_copy_submit(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info,
                                u32 flag, int *id)
{
    struct svm_async_copy_submit_para para = {0};
    int ret;

    para.src_va = src_info->va;
    para.dst_va = dst_info->va;
    para.size = src_info->size;
    para.src_devid = src_info->devid;
    para.dst_devid = dst_info->devid;
    para.src_host_tgid = src_info->host_tgid;
    para.dst_host_tgid = dst_info->host_tgid;
    para.flag = flag;

    ret = svm_cmd_ioctl(devid, SVM_ASYNC_COPY_SUBMIT, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Ioctl async copy submit failed. (ret=%d; devid=%u; src_va=0x%llx; dst_va=0x%llx; size=%llu; "
                "src_devid=%u; dst_devid=%u)\n",
                ret, devid, src_info->va, dst_info->va, src_info->size, src_info->devid, dst_info->devid);
        return ret;
    }

    *id = para.id;
    return DRV_ERROR_NONE;
}

int svm_pci_async_copy_submit(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info, int *id)
{
    return _svm_pci_copy_submit(devid, src_info, dst_info, 0, id);
}

static int svm_pci_async_copy_submit_2d(u32 devid, struct svm_copy_va_2d_info *src_info,
                                        struct svm_copy_va_2d_info *dst_info, int *id)
{
    struct svm_async_copy_submit_2d_para para = {0};
    int ret;

    para.src_va = src_info->va;
    para.dst_va = dst_info->va;
    para.spitch = src_info->pitch;
    para.dpitch = dst_info->pitch;
    para.width = src_info->width;
    para.height = dst_info->height;
    para.src_devid = src_info->devid;
    para.dst_devid = dst_info->devid;

    ret = svm_cmd_ioctl(devid, SVM_ASYNC_COPY_SUBMIT_2D, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err(
            "Submit async copy 2d failed. (ret=%d; devid=%u; src_va=0x%llx; dst_va=0x%llx; spitch=%llu; dpitch=%llu; "
            "width=%llu; height=%llu; src_devid=%u; dst_devid=%u)\n",
            ret, devid, src_info->va, dst_info->va, src_info->pitch, dst_info->pitch, src_info->width, src_info->height,
            src_info->devid, dst_info->devid);
        return ret;
    }

    *id = para.id;
    return DRV_ERROR_NONE;
}

static int svm_pci_async_copy_submit_batch(u32 devid, struct svm_async_copy_submit_batch_para *submit_para)
{
    int ret;

    ret = svm_cmd_ioctl(devid, SVM_ASYNC_COPY_SUBMIT_BATCH, (void *)submit_para);
    if (ret != DRV_ERROR_NONE) {
        svm_info("Async copy batch submit completed. (ret=%d; src_devid=%u; dst_devid=%u)\n", ret,
                 submit_para->src_devid, submit_para->dst_devid);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int svm_pci_async_copy_wait(u32 devid, int id)
{
    struct svm_async_copy_wait_para para = {.id = id};
    int ret;

    ret = svm_cmd_ioctl(devid, SVM_ASYNC_COPY_WAIT, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Ioctl async copy wait failed. (ret=%d; devid=%u; id=%d)\n", ret, devid, id);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int svm_pci_sync_copy(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info)
{
    int ret, id;

    if (svm_pci_memcpy_judge_sync(src_info->size)) {
        return _svm_pci_copy_submit(devid, src_info, dst_info, SVM_COPY_FLAG_SYNC, &id);
    }

    ret = svm_pci_async_copy_submit(devid, src_info, dst_info, &id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return svm_pci_async_copy_wait(devid, id);
}

int svm_pci_sync_copy_2d(u32 devid, struct svm_copy_va_2d_info *src_info, struct svm_copy_va_2d_info *dst_info)
{
    int ret, id;

    ret = svm_pci_async_copy_submit_2d(devid, src_info, dst_info, &id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return svm_pci_async_copy_wait(devid, id);
}

int svm_pci_sync_copy_batch(u64 src[], u64 dst[], u64 size[], u64 count, u32 src_devid, u32 dst_devid)
{
    struct svm_async_copy_submit_batch_para submit_para = {
        .src_va = src, .dst_va = dst, .size = size, .count = count, .src_devid = src_devid, .dst_devid = dst_devid};
    enum svm_cpy_dir dir = copy_dir_get_by_devid(src_devid, dst_devid);
    u32 devid = (dir == SVM_H2D_CPY) ? dst_devid : src_devid;
    int ret;

    ret = svm_pci_async_copy_submit_batch(devid, &submit_para);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return svm_pci_async_copy_wait(devid, submit_para.id);
}

int svm_pci_dma_desc_convert(u32 devid, struct svm_copy_va_info *src_info, struct svm_copy_va_info *dst_info,
                             struct DMA_ADDR *dma_desc)
{
    struct svm_dma_desc_convert_para para = {0};
    int ret;

    para.src_va = src_info->va;
    para.dst_va = dst_info->va;
    para.size = src_info->size;
    para.src_devid = src_info->devid;
    para.dst_devid = dst_info->devid;
    para.src_host_tgid = src_info->host_tgid;
    para.dst_host_tgid = dst_info->host_tgid;

    ret = svm_cmd_ioctl(devid, SVM_DMA_DESC_CONVERT, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Ioctl dma desc convert failed. (ret=%d; devid=%u; src_va=0x%llx; dst_va=0x%llx; size=%llu; "
                "src_devid=%u; dst_devid=%u)\n",
                ret, devid, src_info->va, dst_info->va, src_info->size, src_info->devid, dst_info->devid);
        return ret;
    }

    para.dma_desc.virt_id = devid;
    *dma_desc = para.dma_desc;
    return DRV_ERROR_NONE;
}

int svm_pci_dma_desc_submit(u32 devid, struct DMA_ADDR *dma_desc, int flag)
{
    struct svm_dma_desc_submit_para para = {.dma_desc = *dma_desc, .sync_flag = flag};
    int ret;

    ret = svm_cmd_ioctl(devid, SVM_DMA_DESC_SUBMIT, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Ioctl dma desc submit failed. (ret=%d; devid=%u; flag=%d)\n", ret, devid, flag);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int svm_pci_dma_desc_wait(u32 devid, struct DMA_ADDR *dma_desc)
{
    struct svm_dma_desc_wait_para para = {.dma_desc = *dma_desc};
    int ret;

    ret = svm_cmd_ioctl(devid, SVM_DMA_DESC_WAIT, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Ioctl dma desc wait failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int svm_pci_dma_desc_destroy(u32 devid, struct DMA_ADDR *dma_desc)
{
    struct svm_dma_desc_destroy_para para = {.dma_desc = *dma_desc};
    int ret;

    ret = svm_cmd_ioctl(devid, SVM_DMA_DESC_DESTROY, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Ioctl dma desc destroy failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int svm_pci_dma_desc_convert_2d(u32 devid, struct svm_copy_va_2d_info *src_info, struct svm_copy_va_2d_info *dst_info,
                                u64 fixed_size, struct DMA_ADDR *dma_desc)
{
    struct svm_dma_desc_convert_2d_para para = {0};
    int ret;

    para.src_va = src_info->va;
    para.dst_va = dst_info->va;
    para.spitch = src_info->pitch;
    para.dpitch = dst_info->pitch;
    para.width = src_info->width;
    para.height = dst_info->height;
    para.src_devid = src_info->devid;
    para.dst_devid = dst_info->devid;
    para.fixed_size = fixed_size;

    ret = svm_cmd_ioctl(devid, SVM_DMA_DESC_CONVERT_2D, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Convert 2d failed. (ret=%d; devid=%u; src_va=0x%llx; dst_va=0x%llx; spitch=%llu; dpitch=%llu; "
                "width=%llu; height=%llu; src_devid=%u; dst_devid=%u)\n",
                ret, devid, src_info->va, dst_info->va, src_info->pitch, dst_info->pitch, src_info->width,
                src_info->height, src_info->devid, dst_info->devid);
        return ret;
    }

    para.dma_desc.virt_id = devid;
    *dma_desc = para.dma_desc;
    return DRV_ERROR_NONE;
}

static struct svm_copy_ops g_pci_copy_ops = {.sync_copy = svm_pci_sync_copy,
                                             .async_copy_submit = svm_pci_async_copy_submit,
                                             .async_copy_wait = svm_pci_async_copy_wait,
                                             .dma_desc_convert = svm_pci_dma_desc_convert,
                                             .dma_desc_submit = svm_pci_dma_desc_submit,
                                             .dma_desc_wait = svm_pci_dma_desc_wait,
                                             .dma_desc_destroy = svm_pci_dma_desc_destroy,
                                             .sync_copy_2d = svm_pci_sync_copy_2d,
                                             .dma_desc_convert_2d = svm_pci_dma_desc_convert_2d,
                                             .sync_copy_batch = svm_pci_sync_copy_batch};

void svm_pci_memcpy_ops_register(u32 devid)
{
    svm_copy_ops_register(devid, &g_pci_copy_ops);
}
