/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "ascend_hal.h"

#include "esched_user_interface.h"

#include "svm_log.h"
#include "svm_sub_event_type_uk_msg.h"
#include "smp_uk_msg.h"
#include "svm_pagesize.h"
#include "svm_dbi.h"
#include "malloc_mng.h"
#include "cache_malloc.h"
#include "svm_pipeline.h"
#include "svm_master_init.h"
#include "mms.h"
#include "svm_mem_stats_show.h"
#include "svm_addr_desc.h"
#include "svm_soma.h"

static inline u32 svm_flag_to_mms_type(u64 flag)
{
    if (svm_flag_attr_is_hpage(flag) || svm_flag_attr_is_gpage(flag)) {
        if (svm_flag_attr_is_p2p(flag)) {
            return MMS_TYPE_P2P_HPAGE;
        } else {
            return MMS_TYPE_HPAGE;
        }
    } else {
        if (svm_flag_attr_is_p2p(flag)) {
            return MMS_TYPE_P2P_NPAGE;
        } else {
            return MMS_TYPE_NPAGE;
        }
    }
}

static u64 svm_get_prop_aligned_size(u64 start)
{
    struct svm_prop prop;
    int ret;

    ret = svm_get_prop(start, &prop);
    if (ret != 0) {
        return 0;
    }

    return prop.aligned_size;
}

int svm_module_mem_malloc(u32 devid, u32 numa_id, u64 flag, u64 *start, u64 size, u32 module_id)
{
    struct svm_malloc_location location;
    u64 tmp_flag = flag & (~SVM_FLAG_ATTR_VA_ONLY);
    u64 align;
    int ret;

    ret = svm_query_page_size_by_svm_flag(devid, tmp_flag, &align);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Query page size failed. (ret=%d; devid=%u; flag=0x%llx)\n", ret, devid, tmp_flag);
        return ret;
    }

    svm_malloc_location_pack(devid, numa_id, &location);
    svm_flag_set_module_id(&tmp_flag, module_id);

    ret = svm_malloc(start, size, align, tmp_flag, &location);
    if (ret == 0) {
        svm_mms_add(devid, module_id, svm_flag_to_mms_type(flag), svm_get_prop_aligned_size(*start));
    } else {
        svm_mem_stats_show(devid);
    }

    return ret;
}

int svm_module_mem_free(u32 devid, u64 flag, u64 start, u64 size, u32 module_id)
{
    u64 aligned_size = svm_get_prop_aligned_size(start);
    int ret = svm_free(start);
    SVM_UNUSED(size);
    SVM_UNUSED(module_id);

    if (ret == DRV_ERROR_BUSY) {
        /* return to the user success, free again rely on an event. */
        ret = 0;
    } else if (ret == DRV_ERROR_CLIENT_BUSY) {
        return DRV_ERROR_BUSY;
    }
    if (ret == 0) {
        svm_mms_sub(devid, module_id, svm_flag_to_mms_type(flag), aligned_size);
    }
    return ret;
}

static int svm_parse_alloc_devid(u64 flag, u32 *devid)
{
    u32 virt_mem_type = (flag >> MEM_VIRT_BIT) & ((1UL << MEM_VIRT_WIDTH) - 1);

    *devid = flag & MEM_DEVID_MASK;

    if ((virt_mem_type == MEM_SVM_VAL) || (virt_mem_type == MEM_HOST_AGENT_VAL) || (virt_mem_type == MEM_RESERVE_VAL)) {
        return DRV_ERROR_NOT_SUPPORT;
    } else if ((virt_mem_type == MEM_DEV_VAL) || (virt_mem_type == MEM_DVPP_VAL)) { /* dvpp now is same with device */
        if (*devid >= SVM_MAX_DEV_AGENT_NUM) {
            svm_err("Invalid devid. (devid=%u; flag=%llx)\n", *devid, flag);
            return DRV_ERROR_INVALID_DEVICE;
        }
        return 0;
    } else if ((virt_mem_type == MEM_HOST_VAL) || (virt_mem_type == MEM_HOST_UVA_VAL)) {
        *devid = svm_get_host_devid();
        return 0;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }
}

static int svm_parse_alloc_module_id(u64 flag, u32 *module_id)
{
    u32 tmp_module_id = svm_flag_get_module_id(flag);

    /* RUNTIME will fill invalid model_id in some scenarios */
    *module_id = (tmp_module_id >= MAX_MODULE_ID) ? UNKNOWN_MODULE_ID : tmp_module_id;
    return 0;
}

static int svm_parse_alloc_svm_flag(u64 flag, u32 devid, u64 *svm_flag)
{
    u32 virt_mem_type = (flag >> MEM_VIRT_BIT) & ((1UL << MEM_VIRT_WIDTH) - 1);
    *svm_flag = 0;

    if ((flag & MEM_ADVISE_4G) || (flag & MEM_ADVISE_TS)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (flag & MEM_READONLY) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (((virt_mem_type == MEM_HOST_VAL) || (virt_mem_type == MEM_HOST_UVA_VAL)) &&
        ((flag & MEM_PAGE_HUGE) || (flag & MEM_PAGE_GIANT))) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((virt_mem_type == MEM_HOST_UVA_VAL) && ((flag & MEM_CONTIGUOUS_PHY) || (flag & MEM_ADVISE_P2P) ||
                                                (flag & MEM_ADVISE_BAR) || (flag & MEM_DEV_CP_ONLY))) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (((flag & MEM_PAGE_HUGE) || (flag & MEM_PAGE_GIANT)) && (flag & MEM_CONTIGUOUS_PHY)) {
        svm_run_info("Continuous can not alloc huge/giant page. (flag=0x%llx)\n", flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((flag & MEM_PAGE_HUGE) && (flag & MEM_PAGE_GIANT)) {
        svm_err("Alloc both giant page and huge page is invalid. (flag=0x%llx)\n", flag);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (flag & MEM_DEV_CP_ONLY) {
        if (!(flag & MEM_DEV)) {
            svm_err("Dev cp only should be dev mem. (flag=0x%llx)\n", flag);
            return DRV_ERROR_INVALID_VALUE;
        }

        if ((flag & MEM_PAGE_HUGE) || (flag & MEM_PAGE_GIANT)) {
            svm_run_info("Dev cp only not support huge/giant page. (flag=0x%llx)\n", flag);
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    if (flag & MEM_HOST_RW_DEV_RO) {
        if (!(flag & MEM_DEV) || (flag & MEM_PAGE_GIANT) || (flag & MEM_DEV_CP_ONLY)) {
            svm_run_info("Not support MEM_HOST_RW_DEV_RO. (flag=0x%llx)\n", flag);
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    if (flag & MEM_DEV_PRIVATE) {
        if ((devid == svm_get_host_devid()) || !svm_dbi_is_cc_vm()) {
            svm_run_info("MEM_DEV_PRIVATE only supports device memory in confidential VM. (devid=%u)\n", devid);
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    if (flag & MEM_PAGE_HUGE) {
        *svm_flag |= SVM_FLAG_ATTR_PA_HPAGE;
    }

    if (flag & MEM_PAGE_GIANT) {
        *svm_flag |= SVM_FLAG_ATTR_PA_GPAGE;
    }

    if (flag & MEM_CONTIGUOUS_PHY) {
        *svm_flag |= SVM_FLAG_ATTR_PA_CONTIGUOUS;
    }

    if ((flag & MEM_ADVISE_P2P) || (flag & MEM_ADVISE_BAR)) {
        *svm_flag |= SVM_FLAG_ATTR_PA_P2P;
    }

    if (flag & MEM_DEV_CP_ONLY) {
        *svm_flag |= SVM_FLAG_DEV_CP_ONLY;
    }

    if (flag & MEM_DEV_PRIVATE) {
        *svm_flag |= SVM_FLAG_DEV_PRIVATE;
    }

    if (virt_mem_type == MEM_HOST_UVA_VAL) {
        *svm_flag |= SVM_FLAG_ATTR_MASTER_UVA;
    }

    if ((flag & MEM_SNAPSHOT_REQUIRED) != 0) {
        *svm_flag |= SVM_FLAG_SNAPSHOT_REQUIRED;
    } else if ((flag & MEM_SNAPSHOT_OPTIONAL) != 0) {
        *svm_flag |= SVM_FLAG_SNAPSHOT_OPTIONAL;
    }

    return 0;
}

int svm_mem_malloc(void **va, u64 size, u64 flag)
{
    u32 devid, module_id;
    u64 start, svm_flag;
    int ret;

    ret = svm_parse_alloc_devid(flag, &devid);
    if (ret != 0) {
        return ret;
    }

    ret = svm_parse_alloc_module_id(flag, &module_id);
    if (ret != 0) {
        return ret;
    }

    ret = svm_parse_alloc_svm_flag(flag, devid, &svm_flag);
    if (ret != 0) {
        return ret;
    }

    svm_flag |= SVM_FLAG_CAP_NORMAL_FREE;
    if ((svm_flag & SVM_FLAG_DEV_CP_ONLY) == 0) {
        svm_flag |= SVM_FLAG_CAP_REGISTER | SVM_FLAG_CAP_UNREGISTER | SVM_FLAG_CAP_REGISTER_ACCESS;
        if (devid < SVM_MAX_DEV_AGENT_NUM) { /* host mem not support prefetch, ipc */
            svm_flag |= SVM_FLAG_CAP_IPC_CREATE | SVM_FLAG_CAP_IPC_DESTROY;
            svm_flag |= SVM_FLAG_CAP_PREFETCH;
        } else {
            svm_flag |= SVM_FLAG_CAP_LDST;
        }
        if (!svm_flag_is_dev_private(svm_flag)) {
            svm_flag |= SVM_FLAG_CAP_SYNC_COPY | SVM_FLAG_CAP_SYNC_COPY_BATCH;
            svm_flag |= SVM_FLAG_CAP_DMA_DESC_CONVERT | SVM_FLAG_CAP_DMA_DESC_DESTROY | SVM_FLAG_CAP_DMA_DESC_SUBMIT |
                        SVM_FLAG_CAP_DMA_DESC_WAIT;
            svm_flag |= SVM_FLAG_CAP_ASYNC_COPY_SUBMIT | SVM_FLAG_CAP_ASYNC_COPY_WAIT;
            svm_flag |= SVM_FLAG_CAP_SYNC_COPY_2D | SVM_FLAG_CAP_DMA_DESC_CONVERT_2D;
            svm_flag |= SVM_FLAG_CAP_SYNC_COPY_EX;
            svm_flag |= svm_dbi_is_cc_vm() ? SVM_FLAG_BY_PASS_CACHE : 0;
        }
        svm_flag |= SVM_FLAG_CAP_MEMSET;
        svm_flag |= SVM_FLAG_CAP_GET_ATTR | SVM_FLAG_CAP_GET_ADDR_CHECK_INFO | SVM_FLAG_CAP_GET_MEM_TOKEN_INFO;
        svm_flag |= SVM_FLAG_CAP_GET_D2D_TRANS_WAY;
        if (flag & MEM_HOST_RW_DEV_RO) {
            svm_flag |= SVM_FLAG_ATTR_PG_RDONLY;
            svm_flag |= SVM_FLAG_CAP_MADVISE;
        }
    } else {
        svm_flag |= SVM_FLAG_BY_PASS_CACHE;
        svm_flag |= SVM_FLAG_ATTR_VA_WITHOUT_MASTER;
        svm_flag |= SVM_FLAG_CAP_GET_ATTR;
    }

    start = 0;
    ret = svm_module_mem_malloc(devid, SVM_MALLOC_NUMA_NO_NODE, svm_flag, &start, size, module_id);
    if (ret == 0) {
        (*va) = (void *)(uintptr_t)start;
    }

    return ret;
}

int svm_mem_free(void *va)
{
    struct svm_prop prop;
    u64 start = (u64)(uintptr_t)va;
    int ret;

    ret = svm_get_prop(start, &prop);
    if (ret != 0) {
        /* The log cannot be modified, because in the failure mode library. */
        svm_err("Addr is not alloced or free repeatedly. (start=0x%llx)\n", start);
        svm_report_addr_not_allocated("pp", start);
        return ret;
    }

    if (!svm_flag_cap_is_support_normal_free(prop.flag)) {
        svm_err("Addr cap is not support normal free. (va=0x%llx)\n", start);
        return DRV_ERROR_INVALID_VALUE;
    }

    return svm_module_mem_free(prop.devid, prop.flag, start, prop.size, svm_flag_get_module_id(prop.flag));
}

drvError_t halMemAllocInner(void **pp, unsigned long long size, unsigned long long flag);
drvError_t halMemAllocInner(void **pp, unsigned long long size, unsigned long long flag)
{
    int ret;

    if ((pp == NULL) || (size == 0)) {
        svm_err("Para is invalid. (ptr_is_null=0x%llx; size=%llu)\n", (pp == NULL), size);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_debug("Alloc enter. (ptr=0x%llx; size=%llu; flag=0x%llx)\n", (u64)(uintptr_t)*pp, size, flag);

    ret = svm_master_init();
    if (ret != DRV_ERROR_NONE) {
        svm_err("Svm master init failed. (ret=%d)\n", ret);
        return ret;
    }

    svm_use_pipeline();
    ret = svm_mem_malloc(pp, size, flag);
    svm_unuse_pipeline();
    if (ret == DRV_ERROR_NOT_SUPPORT) {
        svm_run_info("Not support. (size=%llu; flag=0x%llx)\n", size, flag);
    }

    svm_debug("Alloc exit. (ret=%d; ptr=0x%llx; size=%llu; flag=0x%llx)\n", ret, (ret == 0) ? (u64)(uintptr_t)*pp : 0,
              size, flag);

    return (drvError_t)ret;
}

drvError_t halMemAlloc(void **pp, unsigned long long size, unsigned long long flag)
{
    DRV_CURRENT_API_SCOPE();

    return halMemAllocInner(pp, size, flag);
}

static bool svm_va_is_soma(void *va)
{
    void *va_handle = NULL;
    va_handle = svm_handle_get((u64)(uintptr_t)va);
    if (va_handle == NULL) {
        svm_err("Get va handle failed. (va=0x%llx)\n", va);
        return false;
    }

    u32 mem_source = svm_get_mem_source(va_handle);
    svm_handle_put(va_handle);
    return (mem_source == SVM_POOL_MEM);
}

drvError_t halMemFreeInner(void *pp);
drvError_t halMemFreeInner(void *pp)
{
    int ret;

    svm_debug("Free enter. (ptr=0x%llx)\n", (u64)(uintptr_t)pp);

    if (pp == NULL) {
        svm_err("pp is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_va_is_soma(pp)) {
        u64 va = (u64)(uintptr_t)pp;
        ret = soma_mem_sync_free(va);
        return (drvError_t)ret;
    }

    svm_use_pipeline();
    ret = svm_mem_free(pp);
    svm_unuse_pipeline();

    svm_debug("Free exit. (ret=%d; ptr=0x%llx)\n", ret, (u64)(uintptr_t)pp);

    if (ret == DRV_ERROR_BUSY) {
        svm_report_mem_in_use((u64)(uintptr_t)pp);
    }
    return (drvError_t)ret;
}

drvError_t halMemFree(void *pp)
{
    DRV_CURRENT_API_SCOPE();

    return halMemFreeInner(pp);
}

static int svm_smp_del_mem_event_proc(u64 va)
{
    int ret;

    ret = svm_cache_recycle_seg_release(va);
    if (ret == DRV_ERROR_NOT_EXIST) {
        ret = svm_free(va);
        return (ret == DRV_ERROR_CLIENT_BUSY) ? DRV_ERROR_BUSY : ret;
    }

    return ret;
}

static drvError_t svm_smp_del_mem_event_proc_func(unsigned int devid, const void *msg, int msg_len,
                                                  struct drv_event_proc_rsp *rsp)
{
    int ret, retry_cnt;
    const struct svm_smp_del_msg *del_msg = (const struct svm_smp_del_msg *)msg;
    SVM_UNUSED(msg_len);

    retry_cnt = 0;
    do {
        svm_use_pipeline();
        ret = svm_smp_del_mem_event_proc(del_msg->va);
        svm_unuse_pipeline();
        if (ret != DRV_ERROR_NOT_EXIST) {
            /*
             * There may be the following scenarios:
             * 1. Svm_free is being executed, va_handle is erased, smp_del returns busy, and va_handle has not yet been
             * re-inserted;
             * 2. Kern sends smp del mem event, and before the user receives the event, close_device is executed, and
             * the handle is released.
             */
            break;
        }
        usleep(10); /* wait 10 us */
    } while (retry_cnt++ < 10); /* max retry 10 times */

    if (ret != 0) {
        svm_warn("Free unsuccessful. (ret=%d; devid=%u; va=%llx; size=%llx; retry=%d)\n", ret, devid, del_msg->va,
                 del_msg->size, retry_cnt);
    }

    rsp->need_rsp = false;

    return 0;
}

static struct drv_event_proc svm_smp_del_mem_event_proc_handle = {
    svm_smp_del_mem_event_proc_func, sizeof(struct svm_smp_del_msg), "svm_smp_del_mem_event"};

static int __attribute__((constructor)) svm_alloc_init(void)
{
    drv_registert_event_proc(SVM_SMP_DEL_MEM_EVENT, &svm_smp_del_mem_event_proc_handle);
    return 0;
}
