/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdlib.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "pbl_uda_user.h"

#include "ascend_hal_error.h"

#include "svm_pub.h"
#include "svm_log.h"
#include "svm_pub.h"
#include "svm_pagesize.h"
#include "malloc_mng.h"
#include "svm_flag.h"
#include "smm_flag.h"
#include "smm_client.h"
#include "mpl_client.h"
#include "svm_vmm.h"
#include "svmm.h"
#include "svm_pipeline.h"
#include "svm_addr_desc.h"
#include "svm_mem_repair.h"
#include "svm_ioctl_ex.h"
#include "svm_sys_cmd.h"

struct svm_mem_repair_ops *g_ops = {NULL};
static u64 g_repaired_src_va[MEM_REPAIR_MAX_CNT] = {0};
static u32 g_repaired_cnt = 0;

void svm_mem_repair_set_ops(struct svm_mem_repair_ops *ops)
{
    if (g_ops == NULL) {
        g_ops = ops;
    }
}

static int mem_repair_para_check(struct MemRepairInPara *para)
{
    u64 i;

    if ((para->devid >= SVM_MAX_DEV_NUM) || (para->count == 0) || (para->count > MEM_REPAIR_MAX_CNT)) {
        svm_err("InPara is invalid. (devid=%u; repair_cnt=%u)\n", para->devid, para->count);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < para->count; ++i) {
        struct svm_prop prop;
        int ret;

        ret = svm_get_prop(para->repairAddrs[i].ptr, &prop);
        if (ret != 0) {
            svm_err("Get prop failed, va is invalid. (va=0x%llx)\n", para->repairAddrs[i].ptr);
            return ret;
        }

        if (prop.devid != para->devid) {
            svm_err("Devid is not match. (in_devid=%u; prop_devid=%u; va=0x%llx)\n", para->devid, prop.devid,
                    para->repairAddrs[i].ptr);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (!svm_is_va_single_page_align(prop.devid, prop.flag, para->repairAddrs[i].ptr, para->repairAddrs[i].len)) {
            svm_err("Not align. (devid=%u; flag=0x%x; va=0x%llx; size=0x%llx)\n", prop.devid, prop.flag,
                    para->repairAddrs[i].ptr, para->repairAddrs[i].len);
            return DRV_ERROR_INVALID_VALUE;
        }

        if ((svm_flag_cap_is_support_normal_free(prop.flag) == false) &&
            (svm_flag_cap_is_support_vmm_pa_free(prop.flag) == false) &&
            (svm_flag_cap_is_support_vmm_unmap(prop.flag) == false)) {
            svm_err("Repair addr type invalid. (devid=%u; va=0x%llx; flag=%llu)\n", para->devid,
                    para->repairAddrs[i].ptr, prop.flag);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (svm_flag_attr_is_contiguous(prop.flag)) {
            svm_err("Repair addr is contiguous. (devid=%u; va=0x%llx; flag=%llu)\n", para->devid,
                    para->repairAddrs[i].ptr, prop.flag);
            return DRV_ERROR_INVALID_VALUE;
        }
    }
    return DRV_ERROR_NONE;
}

static u32 mem_repair_tranform_mpl_flag(u64 flag)
{
    u32 mpl_flag = 0;

    mpl_flag |= svm_flag_attr_is_hpage(flag) ? SVM_MPL_FLAG_HPAGE : 0;
    mpl_flag |= svm_flag_attr_is_gpage(flag) ? SVM_MPL_FLAG_GPAGE : 0;
    mpl_flag |= svm_flag_attr_is_contiguous(flag) ? SVM_MPL_FLAG_CONTIGUOUS : 0;
    mpl_flag |= svm_flag_attr_is_p2p(flag) ? SVM_MPL_FLAG_P2P : 0;
    mpl_flag |= svm_flag_attr_is_pg_nc(flag) ? SVM_MPL_FLAG_PG_NC : 0;
    mpl_flag |= svm_flag_attr_is_pg_rdonly(flag) ? SVM_MPL_FLAG_PG_RDONLY : 0;
    mpl_flag |= svm_flag_is_dev_private(flag) ? SVM_MPL_FLAG_DEV_PRIVATE : 0;

    return mpl_flag;
}

static int mem_repair_share_remote_process(struct svm_prop *prop, u64 va, u64 size)
{
    struct svm_share_repair_para para = {0};
    int ret;

    svm_global_va_pack(0, prop->tgid, va, size, &para.src_va);
    ret = uda_get_udevid_by_devid_ex(prop->devid, &para.src_va.udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", prop->devid, ret);
        return ret;
    }

    svm_debug("Submit share repair. (devid=%u; udevid=%u; tgid=%d; va=0x%llx; size=0x%llx)\n", prop->devid,
              para.src_va.udevid, prop->tgid, va, size);
    return svm_cmd_ioctl(prop->devid, SVM_SHARE_REPAIR, (void *)&para);
}

static int _mem_repair_try_map_vmm_va(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    struct svm_prop *src_prop = (struct svm_prop *)priv;
    struct svm_global_va real_src_info = *src_info;
    u32 devid = svm_svmm_get_seg_devid(seg_handle);
    int ret;

    vmm_restore_real_src_va(&real_src_info);

    if (real_src_info.va == src_prop->start) {
        struct svm_dst_va dst_info;
        u32 smm_flag = 0;
        u64 svm_flag = svm_svmm_get_seg_svm_flag(seg_handle);

        svm_dst_va_pack(devid, PROCESS_CP1, start, real_src_info.size, &dst_info);
        ret = svm_smm_client_map(&dst_info, &real_src_info, smm_flag);
        if (ret != 0) {
            svm_err("Smm map failed. (ret=%d; devid=%u; va=%llx; size=%llx)\n", ret, devid, start, real_src_info.size);
            return ret;
        }

        if (g_ops != NULL) {
            ret = g_ops->vmm_map(devid, start, svm_flag, &real_src_info);
            if (ret != 0) {
                svm_err("Smm task group map failed. (ret=%d; devid=%u; va=%llx; size=%llx)\n", ret, devid, start,
                        real_src_info.size);
                return ret;
            }
        }
    }
    return 0;
}

static int _mem_repair_try_unmap_vmm_va(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    struct svm_prop *src_prop = (struct svm_prop *)priv;
    struct svm_global_va real_src_info = *src_info;
    u32 devid = svm_svmm_get_seg_devid(seg_handle);
    int ret;

    vmm_restore_real_src_va(&real_src_info);

    if (real_src_info.va == src_prop->start) {
        struct svm_dst_va dst_info;
        u64 svm_flag = svm_svmm_get_seg_svm_flag(seg_handle);

        if (g_ops != NULL) {
            g_ops->vmm_unmap(devid, start, svm_flag, &real_src_info);
        }

        svm_dst_va_pack(devid, PROCESS_CP1, start, real_src_info.size, &dst_info);
        ret = svm_smm_client_unmap(&dst_info, &real_src_info, 0);
        if (ret != 0) {
            svm_err("Smm unmap failed. (devid=%u; va=%llx; size=%llx)\n", devid, start, real_src_info.size);
            return ret;
        }
    }
    return 0;
}

static int mem_repair_try_map_vmm_va(void *va_handle, u64 start, struct svm_prop *prop, void *priv)
{
    void *vmm_svmm = vmm_get_svmm(va_handle);
    int ret;
    SVM_UNUSED(start);
    SVM_UNUSED(prop);

    if (vmm_svmm != NULL) {
        ret = svm_svmm_for_each_seg_handle(vmm_svmm, _mem_repair_try_map_vmm_va, priv);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static int mem_repair_try_unmap_vmm_va(void *va_handle, u64 start, struct svm_prop *prop, void *priv)
{
    void *vmm_svmm = vmm_get_svmm(va_handle);
    int ret;
    SVM_UNUSED(start);
    SVM_UNUSED(prop);

    if (vmm_svmm != NULL) {
        ret = svm_svmm_for_each_seg_handle(vmm_svmm, _mem_repair_try_unmap_vmm_va, priv);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static int mem_repair_vmm_pa(u32 devid, u64 va, u64 page_size, struct svm_prop *prop)
{
    u32 mpl_flag = mem_repair_tranform_mpl_flag(prop->flag);
    int ret;

    ret = svm_for_each_valid_handle(mem_repair_try_unmap_vmm_va, (void *)prop);
    if (ret != 0) {
        svm_err("Try unmap vmm va failed. (devid=%u; va=%llx; size=%llx; ret=%d)\n", devid, va, page_size, ret);
        return ret;
    }

    ret = svm_mpl_client_depopulate_no_unpin(devid, va, page_size, mpl_flag);
    if (ret != 0) {
        (void)svm_for_each_handle(mem_repair_try_map_vmm_va, (void *)prop);
        svm_err("Mpl client depopulate failed. (devid=%u; va=%llx; size=%llx; ret=%d)\n", devid, va, page_size, ret);
        return ret;
    }

    ret = svm_mpl_client_populate_no_pin(devid, va, page_size, mpl_flag);
    if (ret != 0) {
        svm_err("Mpl client populate failed. (devid=%u; va=%llx; size=%llx; ret=%d)\n", devid, va, page_size, ret);
        return ret;
    }

    ret = svm_for_each_handle(mem_repair_try_map_vmm_va, (void *)prop);
    if (ret != 0) {
        svm_err("Try map vmm va failed. (devid=%u; va=%llx; size=%llx; ret=%d)\n", devid, va, page_size, ret);
        return ret;
    }

    ret = mem_repair_share_remote_process(prop, va, page_size);
    if (ret != 0) {
        svm_err("Share remote process repair failed. (devid=%u; va=%llx; size=%llx; ret=%d)\n", devid, va, page_size,
                ret);
        return ret;
    }

    return 0;
}

static int mem_repair_vmm_va(u32 devid, u64 va, u64 page_size)
{
    struct svm_global_va src_info;
    struct svm_prop src_prop;
    u32 i;
    int ret;

    ret = vmm_query_src_info(va, 1, &src_info);
    if (ret != 0) {
        svm_err("Query vmm va src_info failed. (devid=%u; va=%llx; size=%llx; ret=%d)\n", devid, va, page_size, ret);
        return ret;
    }

    ret = svm_get_prop(src_info.va, &src_prop);
    if (ret != 0) {
        svm_err("Get src_info prop failed. (devid=%u; va=%llx; size=%llx; src_va=0x%llx; ret=%d)\n", devid, va,
                page_size, src_info.va, ret);
        return ret;
    }

    for (i = 0; i < g_repaired_cnt; ++i) {
        if (g_repaired_src_va[i] == src_info.va) {
            return 0;
        }
    }

    ret = mem_repair_vmm_pa(devid, src_info.va, page_size, &src_prop);
    if (ret == 0) {
        g_repaired_src_va[g_repaired_cnt] = src_info.va;
        g_repaired_cnt++;
    }
    return ret;
}

static bool is_support_mem_repair_normal_pa(u64 va)
{
    void *handle = svm_handle_get(va);
    if (handle == NULL) {
        return false;
    }

    if (svm_get_priv(handle) != NULL) {
        svm_handle_put(handle);
        return false;
    }
    svm_handle_put(handle);
    return true;
}

static int mem_repair_normal_pa(u32 devid, u64 va, u64 page_size, struct svm_prop *prop)
{
    u32 mpl_flag = mem_repair_tranform_mpl_flag(prop->flag);
    int ret;

    if (is_support_mem_repair_normal_pa(va) == false) {
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_ops != NULL) {
        g_ops->normal_unmap(va, prop);
    }

    ret = svm_mpl_client_depopulate_no_unpin(devid, va, page_size, mpl_flag);
    if (ret != 0) {
        svm_err("Mpl client depopulate failed. (devid=%u; va=%llx; size=%llx; ret=%d)\n", devid, va, page_size, ret);
        return ret;
    }

    ret = svm_mpl_client_populate_no_pin(devid, va, page_size, mpl_flag);
    if (ret != 0) {
#ifndef EMU_ST
        svm_err("Mpl client populate failed. (devid=%u; va=0x%llx; size=0x%llx; ret=%d)\n", devid, va, page_size, ret);
#endif
        return ret;
    }

    if (g_ops != NULL) {
        ret = g_ops->normal_map(va, prop);
        if (ret != 0) {
            return ret;
        }
    }

    ret = mem_repair_share_remote_process(prop, va, page_size);
    if (ret != 0) {
        svm_err("Share remote process repair failed. (devid=%u; va=%llx; size=%llx; ret=%d)\n", devid, va, page_size,
                ret);
        return ret;
    }

    return 0;
}

static int mem_repair_single_addr(u32 devid, u64 va, u64 page_size)
{
    struct svm_prop prop;
    int ret;

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        return ret;
    }

    if (svm_flag_cap_is_support_normal_free(prop.flag)) {
        ret = mem_repair_normal_pa(devid, va, page_size, &prop);
    } else if (svm_flag_cap_is_support_vmm_unmap(prop.flag)) {
        ret = mem_repair_vmm_va(devid, va, page_size);
    }

    return ret;
}

int _svm_mem_repair(struct MemRepairInPara *para)
{
    int ret;
    u32 i;

    ret = mem_repair_para_check(para);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    for (i = 0; i < g_repaired_cnt; ++i) {
        g_repaired_src_va[i] = 0;
    }
    g_repaired_cnt = 0;

    for (i = 0; i < para->count; ++i) {
        ret = mem_repair_single_addr(para->devid, para->repairAddrs[i].ptr, para->repairAddrs[i].len);
        if (ret != 0) {
            svm_err("Mem repair failed. (devid=%u; va=0x%llx; size=0x%llx; ret=%d)\n", para->devid,
                    para->repairAddrs[i].ptr, para->repairAddrs[i].len, ret);
            return ret;
        }
        svm_run_info("Mem repair success. (devid=%u; va=0x%llx; size=0x%llx; ret=%d)\n", para->devid,
                     para->repairAddrs[i].ptr, para->repairAddrs[i].len, ret);
    }
    return 0;
}

int svm_mem_repair(struct MemRepairInPara *para)
{
    int ret;

    svm_occupy_pipeline();
    ret = _svm_mem_repair(para);
    svm_release_pipeline();
    return ret;
}
