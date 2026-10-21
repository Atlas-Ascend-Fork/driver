/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>

#include "ascend_hal.h"
#include "securec.h"

#include "drv_user_common.h" /* user list */
#include "pbl_uda_user.h"

#include "svm_user_adapt.h"
#include "svm_log.h"
#include "svm_pagesize.h"
#include "malloc_mng.h"
#include "va_allocator.h"
#include "svm_vmm.h"
#include "svmm.h"
#include "smm_client.h"
#include "svm_share_type.h"
#include "svm_share_align.h"
#include "svm_pipeline.h"
#include "svm_addr_desc.h"
#include "svm_init_pri.h"
#include "svm_criu.h"

struct svm_prefetch_node {
    struct svm_share_priv_head head;
    struct list_head node;
    void *svmm_inst;
    u64 va;
};

static struct list_head prefetch_head = LIST_HEAD_INIT(prefetch_head);
static pthread_mutex_t prefetch_mutex = PTHREAD_MUTEX_INITIALIZER;

static u32 svm_show_prefetch_node(struct svm_prefetch_node *prefetch_node, char *buf, u32 buf_len)
{
    void *va_handle = NULL;
    int len;

    if (buf_len == 0) {
        return 0;
    }

    len = snprintf_s(buf, buf_len, buf_len - 1, "va 0x%llx\n", prefetch_node->va);
    if (len < 0) {
        return 0;
    }

    va_handle = svm_handle_get(prefetch_node->va);
    if (va_handle != NULL) {
        len += (int)svm_svmm_inst_show_detail(prefetch_node->svmm_inst, buf + len, buf_len - (u32)len);
    }
    svm_handle_put(va_handle);

    return (u32)len;
}

void svm_show_prefetch(u32 devid, char *buf, u32 buf_len)
{
    struct list_head *node = NULL, *tmp = NULL;
    u32 len = 0;

    if (devid != SVM_INVALID_DEVID) {
        return;
    }

    (void)pthread_mutex_lock(&prefetch_mutex);
    list_for_each_safe(node, tmp, &prefetch_head)
    {
        struct svm_prefetch_node *prefetch_node = list_entry(node, struct svm_prefetch_node, node);
        len += svm_show_prefetch_node(prefetch_node, buf + len, buf_len - len);
    }
    (void)pthread_mutex_unlock(&prefetch_mutex);
}

static void svm_prefetch_src_recycle(u32 devid, u32 udevid, void *svmm_inst)
{
    u32 recyle_num = 0;

    /* recycle dst devid match */
    while (1) {
        struct svm_global_va src_info;
        struct svm_dst_va dst_info;
        u64 va, svm_flag;
        u32 smm_flag = 0;
        int ret;

        ret = svm_svmm_get_seg_by_devid(svmm_inst, &devid, &va, &svm_flag, &src_info);
        if (ret != 0) {
            break;
        }

        svm_dst_va_pack(devid, PROCESS_CP1, va, src_info.size, &dst_info);
        (void)svm_smm_client_unmap(&dst_info, &src_info, smm_flag);
        (void)svm_svmm_del_seg(svmm_inst, devid, va, src_info.size, true);
        recyle_num++;
    }

    if (recyle_num > 0) {
        svm_info("Recycle success. (devid=%u; udevid=%u; recycle_num=%u)\n", devid, udevid, recyle_num);
    }
}

void svm_prefetch_recycle(u32 devid)
{
    struct list_head *node = NULL, *tmp = NULL;
    u32 udevid;

    if (uda_get_udevid_by_devid_ex(devid, &udevid) != 0) {
        svm_err("Get udevid failed. (devid=%u)\n", devid);
        return;
    }

    (void)pthread_mutex_lock(&prefetch_mutex);
    list_for_each_safe(node, tmp, &prefetch_head)
    {
        struct svm_prefetch_node *prefetch_node = list_entry(node, struct svm_prefetch_node, node);
        void *va_handle = svm_handle_get(prefetch_node->va);
        if (va_handle != NULL) { /* get va handle to access its priv svmm_inst in node */
            svm_prefetch_src_recycle(devid, udevid, prefetch_node->svmm_inst);
            svm_handle_put(va_handle);
        }
    }
    (void)pthread_mutex_unlock(&prefetch_mutex);
}

static void svm_prefetch_svmm_destroy_locked(struct svm_prefetch_node *prefetch_node)
{
    svm_svmm_destroy_inst(prefetch_node->svmm_inst);
    drv_user_list_del(&prefetch_node->node);
    svm_ua_free(prefetch_node);
}

static void svm_prefetch_svmm_destroy(struct svm_prefetch_node *prefetch_node)
{
    (void)pthread_mutex_lock(&prefetch_mutex);
    svm_prefetch_svmm_destroy_locked(prefetch_node);
    (void)pthread_mutex_unlock(&prefetch_mutex);
}

static int svm_prefetch_svmm_release(void *priv, bool force)
{
    struct svm_prefetch_node *prefetch_node = (struct svm_prefetch_node *)priv;
    void *svmm_inst = prefetch_node->svmm_inst;
    u32 recyle_num = 0;
    SVM_UNUSED(force);

    while (1) {
        struct svm_global_va src_info;
        struct svm_dst_va dst_info;
        u64 va, svm_flag;
        u32 smm_flag = 0;
        u32 devid;
        int ret;

        ret = svm_svmm_get_first_seg(svmm_inst, &devid, &va, &svm_flag, &src_info);
        if (ret != 0) {
            break;
        }

        svm_dst_va_pack(devid, PROCESS_CP1, va, src_info.size, &dst_info);
        (void)svm_smm_client_unmap(&dst_info, &src_info, smm_flag);
        (void)svm_svmm_del_seg(svmm_inst, devid, va, src_info.size, true);
        recyle_num++;
    }

    if (recyle_num > 0) {
        u64 svmma_start, svmma_size, svm_flag;
        svm_svmm_parse_inst_info(svmm_inst, &svmma_start, &svmma_size, &svm_flag);
        svm_info("Release success. (va=0x%llx; size=0x%llx; recycle_num=%u)\n", svmma_start, svmma_size, recyle_num);
    }

    svm_prefetch_svmm_destroy(prefetch_node);

    return 0;
}

static u32 prefetch_show(void *priv, char *buf, u32 buf_len)
{
    if (priv != NULL) {
        struct svm_prefetch_node *prefetch_node = (struct svm_prefetch_node *)priv;
        svm_info("prefetch show:\n");
        return svm_svmm_inst_show_detail(prefetch_node->svmm_inst, buf, buf_len);
    }

    return 0;
}

static bool svm_prefetch_should_criu_reset(void *priv)
{
    SVM_UNUSED(priv);
    return false;
}

static struct svm_priv_ops prefetch_priv_ops = {
    .release = svm_prefetch_svmm_release,
    .get_prop = NULL,
    .show = prefetch_show,
    .should_criu_reset = svm_prefetch_should_criu_reset,
};

static struct svm_svmm_seg_priv_ops prefetch_seg_priv_ops = {
    .release = svm_prefetch_svmm_release,
    .show = prefetch_show,
};

static struct svm_prefetch_node *svm_prefetch_svmm_create(u64 start, u64 size, u64 svm_flag)
{
    struct svm_prefetch_node *prefetch_node = NULL;
    void *svmm_inst = NULL;

    prefetch_node = (struct svm_prefetch_node *)svm_ua_calloc(1, sizeof(*prefetch_node));
    if (prefetch_node == NULL) {
        svm_err("Malloc failed. (va=0x%llx)\n", start);
        return NULL;
    }

    svmm_inst = svm_svmm_create_inst(start, size, SVMM_DEV_NON_OVERLAP, svm_flag);
    if (svmm_inst == NULL) {
        svm_err("Create inst priv failed. (va=0x%llx)\n", start);
        svm_ua_free(prefetch_node);
        return NULL;
    }

    prefetch_node->head.type = SVM_SHARE_TYPE_PREFETCH;
    prefetch_node->svmm_inst = svmm_inst;
    prefetch_node->va = start;
    drv_user_list_add_tail(&prefetch_node->node, &prefetch_head);

    return prefetch_node;
}

static void *svm_normal_get_prefetch_svmm(void *va_handle)
{
    struct svm_prefetch_node *prefetch_node = svm_get_priv(va_handle);
    return ((prefetch_node != NULL) && (prefetch_node->head.type == SVM_SHARE_TYPE_PREFETCH)) ?
               prefetch_node->svmm_inst :
               NULL;
}

static void *svm_normal_create_prefetch_svmm(void *va_handle, u64 start, u64 size, u64 svm_flag)
{
    void *svmm_inst = NULL;

    (void)pthread_mutex_lock(&prefetch_mutex);
    svmm_inst = svm_normal_get_prefetch_svmm(va_handle);
    if (svmm_inst == NULL) {
        struct svm_prefetch_node *prefetch_node = svm_prefetch_svmm_create(start, size, svm_flag);
        if (prefetch_node != NULL) {
            int ret = svm_set_priv(va_handle, (void *)prefetch_node, &prefetch_priv_ops);
            if (ret == 0) {
                svmm_inst = prefetch_node->svmm_inst;
            } else { /* may be set by register */
                svm_err("Set va priv failed. (start=0x%llx)\n", start);
                svm_prefetch_svmm_destroy_locked(prefetch_node);
            }
        }
    }
    (void)pthread_mutex_unlock(&prefetch_mutex);

    return svmm_inst;
}

static void *svm_vmm_get_prefetch_svmm(void *seg_handle)
{
    struct svm_prefetch_node *prefetch_node = svm_svmm_get_seg_priv(seg_handle);
    return ((prefetch_node != NULL) && (prefetch_node->head.type == SVM_SHARE_TYPE_PREFETCH)) ?
               prefetch_node->svmm_inst :
               NULL;
}

static void *svm_vmm_create_prefetch_svmm(void *seg_handle, u64 start, u64 size, u64 svm_flag)
{
    void *svmm_inst = NULL;

    (void)pthread_mutex_lock(&prefetch_mutex);
    svmm_inst = svm_vmm_get_prefetch_svmm(seg_handle);
    /* if svmm_inst is null, but svm_svmm_get_seg_priv(seg_handle) is not null, the priv is occpy by vmm set access */
    if ((svmm_inst == NULL) && (svm_svmm_get_seg_priv(seg_handle) == NULL)) {
        struct svm_prefetch_node *prefetch_node = svm_prefetch_svmm_create(start, size, svm_flag);
        if (prefetch_node != NULL) {
            int ret = svm_svmm_set_seg_priv(seg_handle, (void *)prefetch_node, &prefetch_seg_priv_ops);
            if (ret == 0) {
                svmm_inst = prefetch_node->svmm_inst;
            } else { /* may be set by register */
                svm_err("Set seg priv failed. (start=0x%llx)\n", start);
                svm_prefetch_svmm_destroy_locked(prefetch_node);
            }
        }
    }
    (void)pthread_mutex_unlock(&prefetch_mutex);

    return svmm_inst;
}

static int svm_prefetch_seg(void *svmm_inst, u64 va, u64 size, u32 devid, struct svm_global_va *src_info)
{
    struct svm_dst_va dst_info;
    u64 svm_flag = 0;
    u32 smm_flag = 0;
    int ret;

    ret = svm_svmm_add_seg(svmm_inst, devid, va, svm_flag, src_info);
    if (ret != 0) {
        svm_err("Add seg failed. (devid=%u; va=0x%llx)\n", devid, va);
        return ret;
    }

    svm_dst_va_pack(devid, PROCESS_CP1, va, size, &dst_info);

    ret = svm_smm_client_map(&dst_info, src_info, smm_flag);
    if (ret != 0) {
        svm_err("Smm map failed. (devid=%u; va=0x%llx)\n", devid, va);
        (void)svm_svmm_del_seg(svmm_inst, devid, va, size, true);
        return ret;
    }

    return ret;
}

static int svm_seek_hole_segs_to_prefetch(void *svmm_inst, u64 start, u64 size, u32 devid,
                                          struct svm_global_va *src_info)
{
    struct svm_global_va sub_src_info;
    u64 hole_start, hole_size;
    u64 cur_va = start;
    u64 end = start + size;
    int ret;

    while (cur_va < end) {
        ret = svm_svmm_get_first_hole(svmm_inst, devid, cur_va, end - cur_va, &hole_start, &hole_size);
        if (ret == DRV_ERROR_NOT_EXIST) {
            /* No hole, just return success. */
            return DRV_ERROR_NONE;
        } else if (ret != DRV_ERROR_NONE) {
            svm_err("Get first hole failed. (ret=%d; devid=%u; start=0x%llx; size=%llu)\n", ret, devid, cur_va,
                    end - cur_va);
            return ret;
        }

        svm_global_va_pack(src_info->udevid, src_info->tgid, src_info->va + (hole_start - start), hole_size,
                           &sub_src_info);
        ret = svm_prefetch_seg(svmm_inst, hole_start, hole_size, devid, &sub_src_info);
        if (ret != DRV_ERROR_NONE) {
            /* No need undo. */
            svm_err("Prefetch seg failed. (ret=%d; va=0x%llx; size=%llu; devid=%u)\n", ret, hole_start, hole_size,
                    devid);
            return ret;
        }

        cur_va = hole_start + hole_size;
    }

    return DRV_ERROR_NONE;
}

static int svm_prefetch(void *svmm_inst, u64 va, u64 size, u32 devid, struct svm_global_va *src_info)
{
    int ret;

    svm_svmm_inst_occupy_pipeline(svmm_inst);
    /* Prefetch supports overlapping and ignores already prefetched segments. */
    ret = svm_seek_hole_segs_to_prefetch(svmm_inst, va, size, devid, src_info);
    svm_svmm_inst_release_pipeline(svmm_inst);

    return ret;
}

static int svm_vmm_prefetch(void *va_handle, u64 va, u64 size, u32 devid)
{
    void *vmm_svmm_inst = NULL, *prefetch_svmm_inst = NULL;
    void *seg_handle = NULL;
    u32 seg_devid;
    u64 svm_flag, seg_va;
    struct svm_global_va src_info;
    int ret;

    vmm_svmm_inst = vmm_get_svmm(va_handle);
    if (vmm_svmm_inst == NULL) {
        svm_err("Vmm alloc not finish. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    seg_handle = svm_svmm_seg_handle_get(vmm_svmm_inst, va);
    if (seg_handle == NULL) { /* maybe va has been unmap after get prop or not map */
        svm_err("Get seg handle failed. (va=%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    /* seg va may be update to seg start va which is small than prefetch va */
    seg_va = va;
    ret = svm_svmm_get_seg_by_va(vmm_svmm_inst, &seg_devid, &seg_va, &svm_flag, &src_info);
    if (ret != 0) {
        svm_svmm_seg_handle_put(seg_handle);
        /* should not in, because we has been get seg handle */
        svm_err("Get seg info failed. (va=%llx)\n", va);
        return ret;
    }

    vmm_restore_real_src_va(&src_info);

    prefetch_svmm_inst = svm_vmm_get_prefetch_svmm(seg_handle);
    if (prefetch_svmm_inst == NULL) { /* vmm map addr first prefetch */
        svm_svmm_inst_occupy_pipeline(vmm_svmm_inst);
        prefetch_svmm_inst = svm_vmm_create_prefetch_svmm(seg_handle, seg_va, src_info.size, svm_flag);
        svm_svmm_inst_release_pipeline(vmm_svmm_inst);
        if (prefetch_svmm_inst == NULL) {
            svm_svmm_seg_handle_put(seg_handle);
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    /* update prefetch va real src info */
    src_info.va += va - seg_va;
    src_info.size = size;
    ret = svm_prefetch(prefetch_svmm_inst, va, size, devid, &src_info);

    svm_svmm_seg_handle_put(seg_handle);

    return ret;
}

static int svm_normal_prefetch(void *va_handle, u64 va, u64 size, u32 devid, struct svm_prop *prop)
{
    void *svmm_inst = NULL;
    struct svm_global_va src_info;
    int ret;

    svmm_inst = svm_normal_get_prefetch_svmm(va_handle);
    if (svmm_inst == NULL) { /* vmm map addr first prefetch */
        svmm_inst = svm_normal_create_prefetch_svmm(va_handle, prop->start, prop->aligned_size, prop->flag);
        if (svmm_inst == NULL) {
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    svm_global_va_pack(0, prop->tgid, va, size, &src_info);

    ret = uda_get_udevid_by_devid_ex(prop->devid, &src_info.udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", prop->devid, ret);
        return ret;
    }

    ret = svm_prefetch(svmm_inst, va, size, devid, &src_info);

    return ret;
}

static int svm_prefetch_to_device(u64 va, u64 size, u32 devid)
{
    void *va_handle = NULL;
    struct svm_prop prop;
    u64 aligned_size;
    int ret;

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (va=0x%llx)\n", va);
        return ret;
    }

    if (!svm_flag_cap_is_support_prefetch(prop.flag)) {
        svm_err("Addr cap does not support prefetch. (va=0x%llx)\n", va);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_share_get_src_aligned_size(devid, prop.flag, va, size, &aligned_size);
    if (ret != 0) {
        svm_err("Get aligned size failed. (devid=%u; flag=0x%llx; va=0x%llx; size=0x%llx)\n", devid, prop.flag, va,
                size);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((svm_is_valid_range(va, size) == false) || (va + size) > (prop.start + prop.size)) {
        svm_err("Size is out of bounds. (va=0x%llx; align_size=0x%llx; prop start=0x%llx; size=0x%llx)\n", va,
                aligned_size, prop.start, prop.size);
        return DRV_ERROR_PARA_ERROR;
    }

    if (devid == prop.devid) {
        return DRV_ERROR_NONE;
    }

    va_handle = svm_handle_get(va);
    if (va_handle == NULL) { /* maybe va has been free after get prop */
        svm_err("Get va handle failed. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    if (svm_flag_cap_is_support_vmm_unmap(prop.flag)) {
        ret = svm_vmm_prefetch(va_handle, va, aligned_size, devid);
    } else {
        ret = svm_normal_prefetch(va_handle, va, aligned_size, devid, &prop);
    }

    svm_handle_put(va_handle);
    return ret;
}

DVresult drvMemPrefetchToDevice(DVdeviceptr dev_ptr, size_t len, DVdevice device)
{
    int ret;

    if (device >= SVM_MAX_AGENT_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", device);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_use_pipeline();
    ret = svm_prefetch_to_device((u64)dev_ptr, (u64)len, (u32)device);
    svm_unuse_pipeline();

    return (DVresult)ret;
}

static int svm_prefetch_seg_criu_restore(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    struct svm_dst_va dst_info;
    struct svm_prop src_prop;
    u32 target_devid = *(u32 *)priv;
    u32 devid = svm_svmm_get_seg_devid(seg_handle);
    u32 smm_flag = 0;
    int ret;

    if (target_devid != devid) {
        return DRV_ERROR_NONE;
    }

    ret = svm_get_prop(src_info->va, &src_prop);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get seg prop failed. (ret=%d; va=0x%llx)\n", ret, src_info->va);
        return ret;
    }
    svm_svmm_mod_seg_src_tgid(seg_handle, src_prop.tgid);
    src_info->tgid = src_prop.tgid;

    svm_dst_va_pack(devid, PROCESS_CP1, start, src_info->size, &dst_info);
    ret = svm_smm_client_map(&dst_info, src_info, smm_flag);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Restore prefetch map failed. (ret=%d; devid=%u; va=0x%llx; size=0x%llx)\n", ret, devid, start,
                src_info->size);
    }

    return ret;
}

static int svm_prefetch_criu_restore(u32 devid, void *data)
{
    struct list_head *node = NULL;
    struct list_head *tmp = NULL;
    int ret = DRV_ERROR_NONE;

    SVM_UNUSED(data);
    (void)pthread_mutex_lock(&prefetch_mutex);
    list_for_each_safe(node, tmp, &prefetch_head)
    {
        struct svm_prefetch_node *prefetch_node = list_entry(node, struct svm_prefetch_node, node);
        ret = svm_svmm_for_each_seg_handle(prefetch_node->svmm_inst, svm_prefetch_seg_criu_restore, &devid);
        if (ret != DRV_ERROR_NONE) {
            break;
        }
    }
    (void)pthread_mutex_unlock(&prefetch_mutex);

    return ret;
}

static const struct svm_criu_ops prefetch_criu_ops = {
    .name = "prefetch",
    .restore = svm_prefetch_criu_restore,
};

void __attribute__((constructor)) svm_prefetch_init(void)
{
    int ret = svm_criu_register_ops(&prefetch_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed.\n");
    }
}
