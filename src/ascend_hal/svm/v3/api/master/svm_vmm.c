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
#include "pbl_uda_user.h"
#include "pbl_ubmm.h"

#include "svm_atomic.h"
#include "va_allocator.h"
#include "svm_pub.h"
#include "svm_user_adapt.h"
#include "svm_log.h"
#include "svm_master_init.h"
#include "svm_pagesize.h"
#include "malloc_mng.h"
#include "svm_alloc.h"
#include "svmm.h"
#include "casm.h"
#include "casm_cs.h"
#include "svm_vmm.h"
#include "smm_client.h"
#include "svm_share_type.h"
#include "svm_share_align.h"
#include "svm_pipeline.h"
#include "svm_addr_desc.h"
#include "svm_dbi.h"
#include "svm_init_pri.h"
#include "svm_ub_mem.h"
#include "svm_criu.h"

#define VMM_ALLOC_GRANULARITY_2M (2ULL * SVM_BYTES_PER_MB)
#define VMM_ALLOC_GRANULARITY_1G (1ULL * SVM_BYTES_PER_GB)

#define VMM_ALLOC_MINIMUN_GRANULARITY VMM_ALLOC_GRANULARITY_2M
#define VMM_ALLOC_RECOMMENDED_GRANULARITY VMM_ALLOC_MINIMUN_GRANULARITY
#define VMM_ALLOC_GIANT_PAGE_GRANULARITY SVM_BYTES_PER_GB

struct svm_vmm_node {
    struct svm_share_priv_head head;
    void *svmm_inst;
};

struct svm_vmm_access_node {
    struct svm_share_priv_head head; /* should be at first */
    u64 va;
    u64 size;
    u32 owner_devid;
    drv_mem_handle_t *handle;
    struct svm_global_va src_info;
    void *seg_handle;
    drv_mem_access_type access_type[SVM_MAX_DEV_NUM];
    pthread_rwlock_t rwlock;
};

struct svm_vmm_set_access_para {
    u64 va;
    u64 size;
    struct drv_mem_access_desc *desc;
    u32 count;
};

struct svm_vmm_get_access_para {
    struct drv_mem_location *location;
    drv_mem_access_type type;
};

enum SVM_VMM_HANDLE_TYPE {
    SVM_VMM_HANDLE_NORMAL_TYPE = 0U,
    SVM_VMM_HANDLE_EXPORT_TYPE,
    SVM_VMM_HANDLE_IMPORT_TYPE,
    SVM_VMM_HANDLE_MAX_TYPE
};

typedef struct drv_mem_handle {
    enum SVM_VMM_HANDLE_TYPE type;
    u32 devid;
    int clr_cs_flag; /* used for import handle to clr cs info when destroy handle */
    u64 ref; /* create and import is 1, map and retain +1, unmap and release -1, handle will be freed when ref is 0  */
    u64 key; /* valid when export or import */
    u64 va;
    u32 map_route;
    struct drv_mem_prop src_prop;
    enum drv_mem_pg_type src_pg_type; /* Actual source PA page type for import mappings. */
    struct svm_global_va src_info;    /* used for import handle, query from casm */
} drv_mem_handle_t;

struct svm_share_handle {
    drv_mem_handle_type handle_type;
    u64 key;
    int cs_valid;
    int owner_pid;
    int src_prop_valid;
    struct drv_mem_prop src_prop;
    struct svm_global_va src_va;
};

static void vmm_recycle_pa_handle(drv_mem_handle_t *handle);
static struct svm_vmm_access_node *vmm_get_access_node(void *seg_handle);
static void vmm_access_non_owner_unmap(struct svm_vmm_access_node *access_node, u32 devid);

#define SVM_VMM_OPS_MAX_NUM 3
struct svm_vmm_ops *vmm_ops[SVM_VMM_OPS_MAX_NUM] = {NULL};

/*
 * Owner is the device specified by the PA handle. With this flag, halMemMap
 * only records the mapping and does not map it on the owner device. The owner
 * mapping is established when halMemSetAccess grants access to the owner.
 */
#define VMM_MAP_FLAG_OWNER_NO_ACCESS (1ULL << 0)

static bool vmm_map_flag_is_owner_no_access(u64 vmm_map_flag)
{
    return (vmm_map_flag & VMM_MAP_FLAG_OWNER_NO_ACCESS) != 0;
}

#define VMM_RESERVE_FLAG_PG_TYPE_MASK 0xFFu

static inline enum drv_mem_pg_type vmm_parse_pg_type_by_reserve_flag(u64 flag)
{
    return (enum drv_mem_pg_type)(flag & VMM_RESERVE_FLAG_PG_TYPE_MASK);
}

#ifdef EMU_ST /* for emu st, do not delete */
u64 ut_exp_vmm_get_handle_va(drv_mem_handle_t *handle)
{
    return handle->va;
}
#endif

void svm_vmm_set_ops(struct svm_vmm_ops *ops)
{
    static u32 i = 0;

    if (i >= SVM_VMM_OPS_MAX_NUM) {
        svm_err("Vmm ops is out of range. (ops=%u; valid range=[0, %u))\n", i, (u32)SVM_VMM_OPS_MAX_NUM);
        return;
    }

    vmm_ops[i++] = ops;
}

void *vmm_get_svmm_inst(void *priv)
{
    struct svm_vmm_node *vmm_node = (struct svm_vmm_node *)priv;
    void *svmm_inst = NULL;

    if (vmm_node == NULL) {
        return NULL;
    }

    svmm_inst = vmm_node->svmm_inst;
    if (svmm_inst == NULL) { /* alloc failed, to free */
        svm_ua_free(vmm_node);
        return NULL;
    }

    return svmm_inst;
}

void svm_vmm_svmm_destroy_priv(void *priv)
{
    struct svm_vmm_node *vmm_node = (struct svm_vmm_node *)priv;
    if (vmm_node == NULL) {
        return;
    }

    svm_svmm_destroy_inst(vmm_node->svmm_inst);
    svm_ua_free(vmm_node);
}

static u32 vmm_map_route_to_casm_flag(u64 map_route)
{
    u32 casm_flag = 0;

    casm_flag |= (map_route == MEM_MAP_UB_ONE_PORT_PATH) ? SVM_CASM_FLAG_MAP_UB_ONE_PORT_PATH : 0;
    casm_flag |= (map_route == MEM_MAP_UB_MULTI_PORT_PATH) ? SVM_CASM_FLAG_MAP_UB_MULTI_PORT_PATH : 0;

    return casm_flag;
}

static int vmm_check_map_route(drv_mem_handle_t *handle, u32 map_route)
{
    struct casm_src_va_ex_info src_ex_info;
    struct svm_global_va src_va = {0};
    u64 offset;
    u32 src_server_id;
    u32 src_mem_id;
    int ret;

    if ((map_route != MEM_MAP_DEFAULT_PATH) && (map_route != MEM_MAP_UB_ONE_PORT_PATH) &&
        (map_route != MEM_MAP_UB_MULTI_PORT_PATH)) {
        svm_debug("Map route is not supported. (map_route=%u)\n", map_route);
        return DRV_ERROR_NOT_EXIST;
    }

    ret = svm_casm_get_src_va_ex(handle->devid, handle->key, &src_va, &src_ex_info);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get src va failed. (key=%llu)\n", handle->key);
        return ret;
    }
    if (src_ex_info.update_va == 0) {
        svm_debug("Not ubmem map. (key=%llu)\n", handle->key);
        return DRV_ERROR_NOT_EXIST;
    }
    svm_ub_va_to_mem_id_and_offset(src_ex_info.update_va, &src_mem_id, &offset);

    src_server_id = (src_va.server_id == SVM_INVALID_SERVER_ID) ? UBMM_INVALID_SERVER_ID : src_va.server_id;
    return ubmemMapRouteCheck(handle->devid, src_server_id, src_mem_id, map_route);
}

drvError_t halMemHandleSetAttribute(drv_mem_handle_t *handle, enum HandleAttrType type, struct HandleAttr attr)
{
    int ret;

    if (handle == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((type != HANDLE_ATTR_MEM_MAP_ROUTE) || (handle->type != SVM_VMM_HANDLE_IMPORT_TYPE)) {
        svm_debug("Set attribute not supported. (type=%d; handle_type=%u)\n", type, handle->type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = vmm_check_map_route(handle, attr.mem_map_route);
    if (ret != DRV_ERROR_NONE) {
        return (drvError_t)ret;
    }

    handle->map_route = attr.mem_map_route;
    return DRV_ERROR_NONE;
}

drvError_t halMemHandleGetAttribute(drv_mem_handle_t *handle, enum HandleAttrType type, struct HandleAttr *attr)
{
    if ((handle == NULL) || (attr == NULL)) {
        return DRV_ERROR_INVALID_VALUE;
    }

    if (type != HANDLE_ATTR_MEM_MAP_ROUTE) {
        svm_debug("Set attribute not supported. (type=%d)\n", type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    attr->mem_map_route = handle->map_route;
    return DRV_ERROR_NONE;
}

static void vmm_handle_free(drv_mem_handle_t *handle)
{
    free(handle);
}

static drv_mem_handle_t *vmm_handle_alloc(void)
{
    return malloc(sizeof(drv_mem_handle_t));
}

drv_mem_handle_t *vmm_normal_handle_create(u32 devid, u64 va, const struct drv_mem_prop *src_prop)
{
    drv_mem_handle_t *handle = NULL;

    handle = vmm_handle_alloc();
    if (handle != NULL) {
        handle->type = SVM_VMM_HANDLE_NORMAL_TYPE;
        handle->ref = 1ULL;
        handle->devid = devid;
        handle->clr_cs_flag = 0;
        handle->key = 0;
        handle->va = va;
        handle->map_route = MEM_MAP_DEFAULT_PATH;
        handle->src_prop = *src_prop;
        handle->src_pg_type = src_prop->pg_type;
    }

    return handle;
}

u64 vmm_get_mem_handle_va(drv_mem_handle_t *handle)
{
    return handle->va;
}

static void vmm_normal_handle_destroy(drv_mem_handle_t *handle)
{
    vmm_handle_free(handle);
}

static void vmm_normal_handle_no_ref_destroy(drv_mem_handle_t *handle)
{
    if (svm_atomic64_sub(&handle->ref, 1ULL) == 0ULL) {
        vmm_normal_handle_destroy(handle);
    }
}

static drv_mem_handle_t *vmm_import_handle_create(u32 devid, u64 key, const struct drv_mem_prop *src_prop,
                                                  enum drv_mem_pg_type src_pg_type,
                                                  const struct svm_global_va *src_info)
{
    drv_mem_handle_t *handle = NULL;

    handle = vmm_handle_alloc();
    if (handle != NULL) {
        handle->type = SVM_VMM_HANDLE_IMPORT_TYPE;
        handle->ref = 1ULL;
        handle->devid = devid;
        handle->clr_cs_flag = 0;
        handle->key = key;
        handle->va = src_info->va;
        handle->map_route = MEM_MAP_DEFAULT_PATH;
        handle->src_prop = *src_prop;
        handle->src_pg_type = src_pg_type;
        handle->src_info = *src_info;
    }

    return handle;
}

static void vmm_import_handle_destroy(drv_mem_handle_t *handle)
{
    if (handle->clr_cs_flag != 0) {
        (void)svm_casm_cs_clr_src_info(handle->devid, handle->key);
    }
    vmm_handle_free(handle);
}

static void vmm_import_handle_no_ref_destroy(drv_mem_handle_t *handle)
{
    if (svm_atomic64_sub(&handle->ref, 1ULL) == 0ULL) {
        vmm_import_handle_destroy(handle);
    }
}

static size_t vmm_get_granularity_by_pg_type(enum drv_mem_pg_type pg_type)
{
    return (pg_type == MEM_GIANT_PAGE_TYPE) ? VMM_ALLOC_GIANT_PAGE_GRANULARITY : VMM_ALLOC_RECOMMENDED_GRANULARITY;
}

#define SVM_NUMA_NO_NODE 0xFFFFFFFFU
static int vmm_prop_check(const struct drv_mem_prop *prop, bool is_from_create)
{
    if (prop == NULL) {
        svm_err("Prop is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop->side >= MEM_MAX_SIDE) {
        svm_err("Invalid side type. (side=%u)\n", prop->side);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((prop->side == MEM_DEV_SIDE) && (prop->devid >= SVM_MAX_DEV_AGENT_NUM)) {
        svm_err("Invalid side devid. (side=%u; devid=%u)\n", prop->side, prop->devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop->side == MEM_HOST_SIDE) {
        if ((is_from_create) && (prop->devid != 0)) {
            svm_run_info("Devid must be 0 in host side.\n");
            return DRV_ERROR_NOT_SUPPORT;
        } else if ((is_from_create == false) && (prop->devid > svm_get_host_devid())) {
            svm_err("Devid must be less than host devid. (devid=%u)\n", prop->devid);
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    if ((prop->side == MEM_HOST_NUMA_SIDE) && (prop->devid >= SVM_MALLOC_NUMA_NO_NODE) &&
        (prop->devid != SVM_NUMA_NO_NODE)) {
        svm_err("Invalid numa id. (numa_id=%u)\n", prop->devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (prop->pg_type >= MEM_MAX_PAGE_TYPE) {
        svm_err("Invalid page type. (pg_type=%u)\n", prop->pg_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop->mem_type >= MEM_MAX_TYPE) {
        svm_err("Invalid mem type. (mem_type=%u)\n", prop->mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop->reserve != 0) {
        svm_err("prop reserve should be zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop->pg_type == MEM_GIANT_PAGE_TYPE) {
        if (prop->mem_type == MEM_TS_DDR_TYPE) {
            svm_run_info("Giant page does not support ts_ddr.\n");
            return DRV_ERROR_NOT_SUPPORT;
        }
        if (prop->side != MEM_DEV_SIDE) {
            svm_run_info("Giant page does not support host.\n");
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    return DRV_ERROR_NONE;
}

static int vmm_get_granularity(drv_mem_granularity_options option, enum drv_mem_pg_type pg_type, u32 module_id,
                               u32 devid, size_t *gran)
{
    size_t granularity = vmm_get_granularity_by_pg_type(pg_type);
    u32 host_devid = svm_get_host_devid();
    u64 npage_size;
    int ret;

    ret = svm_dbi_query_npage_size(host_devid, &npage_size);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Dbi query failed. (devid=%u; ret=%d)\n", host_devid, ret);
        return ret;
    }
    granularity = ((pg_type == MEM_NORMAL_PAGE_TYPE) && (option == MEM_ALLOC_GRANULARITY_MINIMUM) &&
                   (devid == host_devid) && (module_id == RUNTIME_MODULE_ID)) ?
                      npage_size :
                      granularity;
    *gran = granularity;
    return DRV_ERROR_NONE;
}

drvError_t halMemGetAllocationGranularity(const struct drv_mem_prop *prop, drv_mem_granularity_options option,
                                          size_t *granularity)
{
    drvError_t ret;

    if (option >= MEM_ALLOC_GRANULARITY_INVALID) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (granularity == NULL) {
        svm_err("granularity is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = vmm_prop_check(prop, false);
    if (ret != 0) {
        return ret;
    }

    ret = svm_master_init();
    if (ret != DRV_ERROR_NONE) {
        svm_err("Svm master init failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = vmm_get_granularity(option, prop->pg_type, prop->module_id, prop->devid, granularity);
    svm_debug(
        "MemGetAllocationGranularity exit. (ret=%d; option=%u; pg_type=%u; module_id=%u; devid=%u; granularity=%llu)\n",
        ret, (u32)option, prop->pg_type, prop->module_id, prop->devid, (u64)(*granularity));
    return ret;
}

static void vmm_recycle_single(void *svmm_inst, u32 udevid)
{
    u32 recyle_num = 0;

    while (1) {
        drv_mem_handle_t *handle = NULL;
        struct svm_global_va src_info;
        struct svm_dst_va dst_info;
        u64 va, svm_flag;
        u32 smm_flag = 0;
        u32 devid;
        int ret;

        src_info.udevid = udevid;
        ret = svm_svmm_get_seg_by_src_udevid(svmm_inst, &devid, &va, &svm_flag, &src_info);
        if (ret != 0) {
            break;
        }

        handle = (drv_mem_handle_t *)(uintptr_t)src_info.va;
        if (!svm_flag_is_vmm_map_owner_no_access(svm_flag)) {
            struct svm_global_va real_src_info = src_info;

            svm_dst_va_pack(devid, PROCESS_CP1, va, src_info.size, &dst_info);
            vmm_restore_real_src_va(&real_src_info);
            (void)svm_smm_client_unmap(&dst_info, &real_src_info, smm_flag);
        }

        (void)svm_svmm_del_seg(svmm_inst, devid, va, src_info.size, true);
        vmm_recycle_pa_handle(handle);
        recyle_num++;
    }

    if (recyle_num > 0) {
        svm_info("Recycle success. (udevid=%u; recyle_num=%u)\n", udevid, recyle_num);
    }
}

static int vmm_recycle_access_dev_single(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    struct svm_vmm_access_node *access_node = vmm_get_access_node(seg_handle);
    u32 devid = *(u32 *)priv;

    SVM_UNUSED(start);
    SVM_UNUSED(src_info);

    if ((access_node == NULL) || (devid == access_node->owner_devid)) {
        return 0;
    }

    (void)pthread_rwlock_wrlock(&access_node->rwlock);
    if (access_node->access_type[devid] != MEM_ACCESS_TYPE_NONE) {
        vmm_access_non_owner_unmap(access_node, devid);
    }
    (void)pthread_rwlock_unlock(&access_node->rwlock);

    return 0;
}

static void vmm_recycle_access_dev(void *svmm_inst, u32 devid)
{
    (void)svm_svmm_for_each_seg_handle(svmm_inst, vmm_recycle_access_dev_single, &devid);
}

struct vmm_recycle_priv {
    u32 devid;
    u32 udevid;
};

static int vmm_try_recycle_single(void *va_handle, u64 start, struct svm_prop *prop, void *priv)
{
    void *svmm_inst = vmm_get_svmm(va_handle);
    struct vmm_recycle_priv *recycle_priv = (struct vmm_recycle_priv *)priv;

    SVM_UNUSED(start);
    SVM_UNUSED(prop);

    if (svmm_inst != NULL) {
        vmm_recycle_single(svmm_inst, recycle_priv->udevid);
        vmm_recycle_access_dev(svmm_inst, recycle_priv->devid);
    }

    return 0;
}

void vmm_recycle(u32 devid)
{
    u32 udevid;
    struct vmm_recycle_priv recycle_priv = {0};

    if (uda_get_udevid_by_devid_ex(devid, &udevid) != 0) {
        svm_err("Get udevid failed. (devid=%u)\n", devid);
        return;
    }

    recycle_priv.devid = devid;
    recycle_priv.udevid = udevid;

    (void)svm_for_each_handle(vmm_try_recycle_single, &recycle_priv);
}

static void svm_vmm_svmm_destroy(struct svm_vmm_node *vmm_node)
{
    svm_svmm_destroy_inst(vmm_node->svmm_inst);
    svm_ua_free(vmm_node);
}

static int vmm_svmm_release(void *priv, bool force)
{
    struct svm_vmm_node *vmm_node = (struct svm_vmm_node *)priv;
    void *svmm_inst = NULL;
    u32 recyle_num = 0;

    if (vmm_node == NULL) {
        return 0;
    }

    svmm_inst = vmm_node->svmm_inst;
    if (svmm_inst == NULL) { /* alloc failed, to free */
        svm_ua_free(vmm_node);
        return 0;
    }

    while (1) {
        struct svm_global_va src_info;
        struct svm_dst_va dst_info;
        u64 va, svm_flag;
        u32 devid;
        u32 smm_flag = 0;
        int ret;

        ret = svm_svmm_get_first_seg(svmm_inst, &devid, &va, &svm_flag, &src_info);
        if (ret != 0) {
            break;
        }

        if (force == false) {
            return DRV_ERROR_BUSY;
        }

        if (!svm_flag_is_vmm_map_owner_no_access(svm_flag)) {
            svm_dst_va_pack(devid, PROCESS_CP1, va, src_info.size, &dst_info);
            vmm_restore_real_src_va(&src_info);
            (void)svm_smm_client_unmap(&dst_info, &src_info, smm_flag);
        }

        (void)svm_svmm_del_seg(svmm_inst, devid, va, src_info.size, true);
        recyle_num++;
    }

    if (recyle_num > 0) {
        u64 svmma_start, svmma_size, svm_flag;
        svm_svmm_parse_inst_info(svmm_inst, &svmma_start, &svmma_size, &svm_flag);
        svm_info("Force release success. (va=0x%llx; size=0x%llx; recyle_num=%u)\n", svmma_start, svmma_size,
                 recyle_num);
    }

    svm_vmm_svmm_destroy(vmm_node);

    return 0;
}

static int vmm_get_prop(void *priv, u64 va, struct svm_prop *prop)
{
    struct svm_vmm_node *vmm_node = (struct svm_vmm_node *)priv;
    void *svmm_inst = NULL;
    struct svm_global_va src_info;
    u64 svm_flag;
    u32 devid;
    int ret;

    if (vmm_node == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    svmm_inst = vmm_node->svmm_inst;
    if (svmm_inst == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_svmm_get_seg_by_va(svmm_inst, &devid, &va, &svm_flag, &src_info);
    if (ret != 0) { /* not map, use vmm va prop */
        prop->devid = SVM_INVALID_DEVID;
        prop->tgid = 0;
        svm_svmm_parse_inst_info(svmm_inst, &prop->start, &prop->size, &prop->flag);
        prop->aligned_size = prop->size;
        ret = 0;
    } else {
        prop->devid = devid;
        prop->tgid = src_info.tgid; /* should use map dst tgid, fixed it later */
        prop->flag = svm_flag;
        prop->start = va;
        prop->size = src_info.size;
        ret = svm_get_aligned_size(prop->devid, prop->flag, prop->size, &prop->aligned_size);
    }

    return ret;
}

static u32 vmm_show(void *priv, char *buf, u32 buf_len)
{
    struct svm_vmm_node *vmm_node = (struct svm_vmm_node *)priv;
    void *svmm_inst = NULL;

    if (vmm_node == NULL) {
        return 0;
    }

    svmm_inst = vmm_node->svmm_inst;
    if (svmm_inst != NULL) {
        svm_info("vmm show:\n");
        return svm_svmm_inst_show_detail(svmm_inst, buf, buf_len);
    }

    return 0;
}

static bool vmm_should_criu_reset(void *priv)
{
    SVM_UNUSED(priv);
    return false;
}

static struct svm_priv_ops vmm_priv_ops = {
    .release = vmm_svmm_release,
    .get_prop = vmm_get_prop,
    .show = vmm_show,
    .should_criu_reset = vmm_should_criu_reset,
};

void *vmm_get_svmm(void *va_handle)
{
    struct svm_vmm_node *vmm_node = svm_get_priv(va_handle);
    return ((vmm_node != NULL) && (vmm_node->head.type == SVM_SHARE_TYPE_VMM)) ? vmm_node->svmm_inst : NULL;
}

static int _vmm_create_svmm_inst(void *va_handle, u64 start, u64 size, u64 svm_flag)
{
    struct svm_vmm_node *vmm_node = NULL;
    void *svmm_inst = NULL;

    vmm_node = (struct svm_vmm_node *)svm_ua_calloc(1, sizeof(*vmm_node));
    if (vmm_node == NULL) {
        /* must set priv, to free the va */
        (void)svm_set_priv(va_handle, NULL, &vmm_priv_ops);
        svm_err("Malloc failed. (va=0x%llx)\n", start);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    svmm_inst = svm_svmm_create_inst(start, size, SVMM_NON_OVERLAP, svm_flag);
    if (svmm_inst == NULL) {
        /* must set priv, to free the va, free vmm_node in release */
        (void)svm_set_priv(va_handle, vmm_node, &vmm_priv_ops);
        svm_err("Create svmm failed. (start=%llx)\n", start);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    vmm_node->head.type = SVM_SHARE_TYPE_VMM;
    vmm_node->svmm_inst = svmm_inst;

    (void)svm_set_priv(va_handle, vmm_node, &vmm_priv_ops);

    return 0;
}

static int vmm_create_svmm_inst(u64 start, u64 size, u64 svm_flag)
{
    void *va_handle = NULL;
    int ret;

    va_handle = svm_handle_get(start);
    if (va_handle == NULL) { /* maybe va has been free after get prop */
        svm_err("Get va handle failed. (start=%llx)\n", start);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = _vmm_create_svmm_inst(va_handle, start, size, svm_flag);
    svm_handle_put(va_handle);
    return ret;
}

static int vmm_malloc_va(u64 align, void **va, u64 size, u64 flag)
{
    struct svm_malloc_location location;
    u64 start, svm_flag = 0;
    int ret;

    svm_flag |= SVM_FLAG_CAP_VMM_VA_FREE;
    svm_flag |= SVM_FLAG_CAP_VMM_MAP;
    svm_flag |= SVM_FLAG_ATTR_VA_ONLY;
    svm_flag |= SVM_FLAG_MUST_WITH_PRIV;
    svm_flag |= SVM_FLAG_BY_PASS_CACHE;
    svm_flag |= svm_dbi_is_cc_vm() ? SVM_FLAG_DEV_PRIVATE : 0;
    svm_flag_set_module_id(&svm_flag, SVM_FLAG_INVALID_MODULE_ID);

    start = (u64)(uintptr_t)(*va);
    if (start != 0) {
        svm_flag |= SVM_FLAG_ATTR_SPACIFIED_VA;
        if (svm_is_in_dcache_va_range(start, size)) {
            svm_flag |= SVM_FLAG_ATTR_VA_WITHOUT_MASTER;
        }
    }

    if ((flag & MEM_RSV_TYPE_DEVICE_SHARE) != 0) {
        svm_flag |= SVM_FLAG_ATTR_VA_WITHOUT_MASTER;
    }

    /* host uva: route to pcie th allocator in pcie scene via master uva flag */
    if ((flag & MEM_RSV_TYPE_HOST_UVA) != 0) {
        svm_flag |= SVM_FLAG_ATTR_MASTER_UVA;
    }

    svm_malloc_location_pack(SVM_INVALID_DEVID, SVM_MALLOC_NUMA_NO_NODE, &location);

    ret = svm_malloc(&start, size, align, svm_flag, &location);
    if (ret != 0) {
        return ret;
    }

    ret = vmm_create_svmm_inst(start, size, svm_flag);
    if (ret != 0) {
        (void)svm_free(start);
        return ret;
    }

    (*va) = (void *)(uintptr_t)start;

    return 0;
}

static int vmm_free_va(void *va)
{
    struct svm_prop prop;
    u64 start = (u64)(uintptr_t)va;
    int ret;

    ret = svm_get_prop(start, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (start=%llx)\n", start);
        svm_report_addr_not_allocated("ptr", start);
        return ret;
    }

    if (!svm_flag_cap_is_support_vmm_va_free(prop.flag)) {
        svm_err("Addr cap does not support vmm va free. (va=0x%llx)\n", start);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_free(start);
    return (ret == DRV_ERROR_CLIENT_BUSY) ? DRV_ERROR_BUSY : ret;
}

static int vmm_address_reserve_para_check(void **ptr, size_t size, size_t alignment, void *addr, uint64_t flag)
{
    u64 specify_va = (u64)(uintptr_t)addr;
    enum drv_mem_pg_type pg_type = vmm_parse_pg_type_by_reserve_flag(flag);

    if (ptr == NULL) {
        svm_err("Ptr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (size == 0) {
        svm_err("size is zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((specify_va != 0) && ((specify_va != svm_align_up(specify_va, VMM_ALLOC_RECOMMENDED_GRANULARITY)) ||
                              (svm_is_valid_range(specify_va, size) == false))) {
        svm_err("Specified addr not aligned up with recommended granularity or mix range. (addr=0x%llx; size=0x%lx)\n",
                specify_va, size);
        if (specify_va != svm_align_up(specify_va, VMM_ALLOC_RECOMMENDED_GRANULARITY)) {
            svm_report_addr_not_aligned("addr", specify_va, VMM_ALLOC_RECOMMENDED_GRANULARITY);
        }
        return DRV_ERROR_INVALID_VALUE;
    }

    /* reserved para, verify by zero for subsequent compatibility */
    if (alignment != 0) {
        svm_report_para_only_zero("alignment", (u64)alignment);
        svm_err("Alignment should be zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pg_type >= MEM_MAX_PAGE_TYPE) {
        svm_err("Invalid pg type. (pg_type=%u)\n", pg_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((specify_va != 0) && (svm_is_in_dcache_va_range(specify_va, size) && (pg_type > MEM_HUGE_PAGE_TYPE))) {
        svm_err("Dcache page_type invalid. (addr=0x%llx; page_type=%u)\n", specify_va, pg_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* host_uva needs with master, device_share sets without master, host_uva map host handle only */
    if ((flag & MEM_RSV_TYPE_HOST_UVA) != 0) {
        if (specify_va != 0) {
            svm_run_info("Host uva not support specified address. (addr=0x%llx)\n", specify_va);
            return DRV_ERROR_NOT_SUPPORT;
        }
        if ((flag & (MEM_RSV_TYPE_REMOTE_MAP | MEM_RSV_TYPE_DEVICE_SHARE)) != 0) {
            svm_run_info("Host uva conflict with remote_map or device_share. (flag=0x%llx)\n", flag);
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    return 0;
}

static u64 vmm_get_va_align(enum drv_mem_pg_type pg_type)
{
    static u64 pg_type_to_align[MEM_MAX_PAGE_TYPE] = {[MEM_NORMAL_PAGE_TYPE] = VMM_ALLOC_GRANULARITY_2M,
                                                      [MEM_HUGE_PAGE_TYPE] = VMM_ALLOC_GRANULARITY_2M,
                                                      [MEM_GIANT_PAGE_TYPE] = VMM_ALLOC_GRANULARITY_1G};

    return pg_type_to_align[pg_type];
}

drvError_t halMemAddressReserve(void **ptr, size_t size, size_t alignment, void *addr, uint64_t flag)
{
    void *malloc_va = addr; /* Application for a specified address */
    u64 len = (u64)size;
    enum drv_mem_pg_type pg_type;
    int ret;

    DRV_CURRENT_API_SCOPE();

    pg_type = vmm_parse_pg_type_by_reserve_flag(flag);
    ret = vmm_address_reserve_para_check(ptr, size, alignment, addr, flag);
    if (ret != 0) {
        return (drvError_t)ret;
    }

    if (!svm_is_in_dcache_va_range((u64)malloc_va, size)) { /* Dcache range size isn't aligned by GB */
        /* Inheriting from the old version, the user wants to alloc 1GB aligned va,
           but the passed flag is MEM_HUGE_PAGE_TYPE. */
        if (size >= SVM_BYTES_PER_GB) {
            pg_type = MEM_GIANT_PAGE_TYPE;
        }

        if (((flag & MEM_RSV_TYPE_DEVICE_SHARE) != 0) || (size >= SVM_VA_RESERVE_ALIGN)) {
            len = svm_align_up((u64)size, SVM_VA_RESERVE_ALIGN);
        }
    }

    svm_use_pipeline();
    ret = vmm_malloc_va(vmm_get_va_align(pg_type), &malloc_va, len, flag);
    svm_unuse_pipeline();

    if (ret == DRV_ERROR_NONE) {
        svm_debug("Vmm reserve. (va=0x%llx; size=%llu; flag=0x%llx; pg_type=%u)\n", (u64)(uintptr_t)malloc_va, size,
                  flag, pg_type);
    }

    *ptr = (ret == 0) ? malloc_va : *ptr;
    return (drvError_t)ret;
}

drvError_t halMemAddressFree(void *ptr)
{
    drvError_t ret;

    DRV_CURRENT_API_SCOPE();

    if (ptr == NULL) {
        svm_err("Ptr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_debug("Vmm free. (Va=0x%llx)\n", (u64)(uintptr_t)ptr);
    svm_use_pipeline();
    ret = vmm_free_va(ptr);
    svm_unuse_pipeline();

    if (ret != 0) {
        svm_err("Vmm free va failed. (ret=%d; ptr=0x%llx)\n", ret, (u64)(uintptr_t)ptr);
    }
    if (ret == DRV_ERROR_BUSY) {
        svm_report_mem_in_use((u64)(uintptr_t)ptr);
    }

    return (drvError_t)ret;
}

static bool vmm_is_normal_pg_type(enum drv_mem_pg_type pg_type, u32 module_id, size_t size)
{
    return (pg_type == MEM_NORMAL_PAGE_TYPE) && (size < VMM_ALLOC_GRANULARITY_2M) && (module_id == RUNTIME_MODULE_ID);
}

static enum drv_mem_pg_type vmm_get_real_pg_type(enum drv_mem_pg_type pg_type, u32 module_id, size_t size)
{
    return vmm_is_normal_pg_type(pg_type, module_id, size) ?
               MEM_NORMAL_PAGE_TYPE :
               ((pg_type == MEM_GIANT_PAGE_TYPE) ? MEM_GIANT_PAGE_TYPE : MEM_HUGE_PAGE_TYPE);
}

static u64 vmm_get_real_pg_size(enum drv_mem_pg_type pg_type, u32 module_id, size_t size)
{
    u64 npage_size;

    (void)svm_dbi_query_npage_size(svm_get_host_devid(), &npage_size);
    return vmm_is_normal_pg_type(pg_type, module_id, size) ?
               npage_size :
               ((pg_type == MEM_GIANT_PAGE_TYPE) ? VMM_ALLOC_GRANULARITY_1G : VMM_ALLOC_GRANULARITY_2M);
}

static u64 vmm_get_handle_pg_size(const drv_mem_handle_t *handle, size_t size)
{
    u64 npage_size;

    /* A normal handle keeps the requested page type, which must be resolved with its allocation parameters. */
    if (handle->type != SVM_VMM_HANDLE_IMPORT_TYPE) {
        return vmm_get_real_pg_size(handle->src_prop.pg_type, handle->src_prop.module_id, size);
    }

    /* An import handle keeps the actual source PA page type returned by CASM and must not resolve it again. */
    (void)svm_dbi_query_npage_size(svm_get_host_devid(), &npage_size);
    return (handle->src_pg_type == MEM_NORMAL_PAGE_TYPE) ?
               npage_size :
               ((handle->src_pg_type == MEM_GIANT_PAGE_TYPE) ? VMM_ALLOC_GRANULARITY_1G : VMM_ALLOC_GRANULARITY_2M);
}

static bool vmm_is_need_pa_continuous(u32 devid, u32 side, enum drv_mem_pg_type pg_type)
{
    u64 host_npage_size, dev_npage_size;

    (void)svm_dbi_query_npage_size(svm_get_host_devid(), &host_npage_size);
    (void)svm_dbi_query_npage_size(devid, &dev_npage_size);
    return ((side == MEM_DEV_SIDE) && (pg_type == MEM_NORMAL_PAGE_TYPE) && (host_npage_size != dev_npage_size));
}

static inline u32 vmm_get_numa_id(u32 side, u32 devid)
{
    return ((side == MEM_HOST_NUMA_SIDE) && (devid != SVM_NUMA_NO_NODE)) ? devid : SVM_MALLOC_NUMA_NO_NODE;
}

static int vmm_malloc_pa(drv_mem_handle_t **handle, u64 size, const struct drv_mem_prop *prop)
{
    u32 module_id = (prop->module_id == UNKNOWN_MODULE_ID) ? ASCENDCL_MODULE_ID : prop->module_id;
    u32 numa_id = vmm_get_numa_id(prop->side, prop->devid);
    enum drv_mem_pg_type pg_type = vmm_get_real_pg_type(prop->pg_type, module_id, size);
    u32 devid = prop->devid;
    u64 start = 0;
    u64 svm_flag = 0;
    int ret;

    if ((prop->mem_type == MEM_P2P_HBM_TYPE) || (prop->mem_type == MEM_P2P_DDR_TYPE)) {
        svm_flag |= SVM_FLAG_ATTR_PA_P2P;
    }
    if ((prop->side == MEM_HOST_SIDE) || (prop->side == MEM_HOST_NUMA_SIDE)) {
        devid = svm_get_host_devid();
    }

    svm_flag |= SVM_FLAG_CAP_VMM_PA_FREE;
    svm_flag |= SVM_FLAG_CAP_VMM_EXPORT;
    svm_flag |= SVM_FLAG_BY_PASS_CACHE;
    svm_flag |= (pg_type == MEM_GIANT_PAGE_TYPE) ? SVM_FLAG_ATTR_PA_GPAGE :
                                                   ((pg_type == MEM_NORMAL_PAGE_TYPE) ? 0 : SVM_FLAG_ATTR_PA_HPAGE);
    svm_flag |= (vmm_is_need_pa_continuous(devid, prop->side, pg_type) ? SVM_FLAG_ATTR_PA_CONTIGUOUS : 0);

    ret = svm_module_mem_malloc(devid, numa_id, svm_flag, &start, size, module_id);
    if (ret != 0) {
        svm_no_err_if((ret == DRV_ERROR_OUT_OF_MEMORY), "Module mem malloc failed. (ret=%d; size=%llu; devid=%u)\n",
                      ret, size, devid);
        return ret;
    }

    *handle = vmm_normal_handle_create(devid, start, prop);
    if (*handle == NULL) {
        svm_module_mem_free(devid, svm_flag, start, size, prop->module_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    return 0;
}

static int vmm_free_pa(drv_mem_handle_t *handle)
{
    struct svm_prop prop;
    u64 start = handle->va;
    int ret;

    ret = svm_get_prop(start, &prop);
    if (ret != 0) {
        /* The log cannot be modified, because in the failure mode library. */
        svm_err("Vmm release find handle failed. (start=0x%llx)\n", start);
        return ret;
    }

    if (!svm_flag_cap_is_support_vmm_pa_free(prop.flag)) {
        svm_err("Addr cap does not support vmm pa free. (start=0x%llx)\n", start);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_atomic64_sub(&handle->ref, 1ULL) > 0ULL) {
        return 0;
    }

    ret = svm_module_mem_free(prop.devid, prop.flag, start, prop.size, svm_flag_get_module_id(prop.flag));
    if (ret != 0) {
        svm_atomic64_inc(&handle->ref);
        return ret;
    }

    vmm_normal_handle_destroy(handle);

    return 0;
}

static void vmm_recycle_pa_handle(drv_mem_handle_t *handle)
{
    if (handle->type == SVM_VMM_HANDLE_IMPORT_TYPE) {
        vmm_import_handle_no_ref_destroy(handle);
    } else {
        vmm_normal_handle_no_ref_destroy(handle);
    }
}

static size_t vmm_get_create_granularity(enum drv_mem_pg_type pg_type, u32 module_id, size_t size)
{
    size_t granularity;
    u64 npage_size;

    (void)svm_dbi_query_npage_size(svm_get_host_devid(), &npage_size);
    granularity = vmm_get_granularity_by_pg_type(pg_type);
    granularity = vmm_is_normal_pg_type(pg_type, module_id, size) ? npage_size : granularity;
    return granularity;
}

drvError_t halMemCreateInner(drv_mem_handle_t **handle, size_t size, const struct drv_mem_prop *prop, uint64_t flag);
drvError_t halMemCreateInner(drv_mem_handle_t **handle, size_t size, const struct drv_mem_prop *prop, uint64_t flag)
{
    size_t granularity;
    u32 module_id;
    u32 devid;
    int ret;

    if (handle == NULL) {
        svm_err("Handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    /* reserved para, verify by zero for subsequent compatibility */
    if (flag != 0) {
        svm_err("Flag should be zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_master_init();
    if (ret != DRV_ERROR_NONE) {
        svm_err("Svm master init failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = vmm_prop_check(prop, true);
    if (ret != 0) {
        return (drvError_t)ret;
    }

    granularity = vmm_get_create_granularity(prop->pg_type, prop->module_id, size);
    if ((size == 0) || ((size % granularity) != 0)) {
        /* The log cannot be modified, because in the failure mode library. */
        svm_err("Size is invalid. (size=%llu; granularity=%llu; side=%u; pg_type=%u; module_id=%u)\n", (u64)size,
                (u64)granularity, prop->side, prop->pg_type, prop->module_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_use_pipeline();
    ret = vmm_malloc_pa(handle, (u64)size, prop);
    svm_unuse_pipeline();
    if (ret == DRV_ERROR_OUT_OF_MEMORY) {
        module_id = (prop->module_id >= MAX_MODULE_ID) ? UNKNOWN_MODULE_ID : prop->module_id;
        devid = ((prop->side == MEM_HOST_SIDE) || (prop->side == MEM_HOST_NUMA_SIDE)) ? svm_get_host_devid() :
                                                                                        prop->devid;
        svm_report_oom(size, halGetMemModuleName(module_id), devid);
    }
    if (ret == 0) {
        svm_debug("Vmm create. (size=%llu; side=%d; devid=%u; module_id=%u; pg_type=%u; mem_type=%u; handle_va=0x%llx)"
                  "\n",
                  size, prop->side, prop->devid, prop->module_id, prop->pg_type, prop->mem_type, (*handle)->va);
    }

    return (drvError_t)ret;
}

drvError_t halMemCreate(drv_mem_handle_t **handle, size_t size, const struct drv_mem_prop *prop, uint64_t flag)
{
    DRV_CURRENT_API_SCOPE();

    return halMemCreateInner(handle, size, prop, flag);
}

drvError_t halMemReleaseInner(drv_mem_handle_t *handle);
drvError_t halMemReleaseInner(drv_mem_handle_t *handle)
{
    int ret;

    if (handle == NULL) {
        svm_err("Handle or prop is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_debug("Vmm release. (handle_va=0x%llx; type=%d; devid=%u; key=%llu; pg_type=%u)\n", handle->va, handle->type,
              handle->devid, handle->key, handle->src_prop.pg_type);
    if (handle->type == SVM_VMM_HANDLE_IMPORT_TYPE) {
        vmm_import_handle_no_ref_destroy(handle);
        return DRV_ERROR_NONE;
    } else {
        if (handle->type == SVM_VMM_HANDLE_EXPORT_TYPE) {
            ret = svm_casm_destroy_key(handle->key);
            if (ret != 0) {
                svm_err("Casm destroy key failed. (ret=%d; key=%llu)\n", ret, handle->key);
                return ret;
            }
        }

        svm_use_pipeline();
        ret = vmm_free_pa(handle);
        svm_unuse_pipeline();
        if (ret != 0) {
            svm_err("Vmm free pa failed. (ret=%d)\n", ret);
        }
        return (drvError_t)ret;
    }
}

drvError_t halMemRelease(drv_mem_handle_t *handle)
{
    return halMemReleaseInner(handle);
}

#define CASM_KEY_UDEVID_OFFSET 32
#define CASM_KEY_UDEVID_MASK 0xffff
static inline u32 casm_key_to_devid(u64 key)
{
    u32 udevid = (((u32)((key) >> CASM_KEY_UDEVID_OFFSET)) & CASM_KEY_UDEVID_MASK);
    u32 devid;
    int ret;

    ret = uda_get_devid_by_udevid_ex(udevid, &devid);
    if (ret != 0) {
        return udevid;
    }
    return devid;
}

drvError_t halMemGetAllocationPropertiesFromHandle(struct drv_mem_prop *prop, drv_mem_handle_t *handle)
{
    if ((handle == NULL) || (prop == NULL)) {
        svm_err("Input invalid. (handle=%u; prop=%u)\n", (handle == NULL), (prop == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    *prop = handle->src_prop;
    return DRV_ERROR_NONE;
}

static int _vmm_retain_alloction_handle(u32 devid, void *va_handle, u64 va, drv_mem_handle_t **handle)
{
    struct svm_global_va src_info;
    void *svmm_inst = NULL;
    u64 start = va;
    u64 svm_flag;
    int ret;

    svmm_inst = vmm_get_svmm(va_handle);
    if (svmm_inst == NULL) {
        svm_err("Get svmm inst failed. (va=%llx)\n", va);
        return DRV_ERROR_INVALID_VALUE;
    }

    src_info.udevid = SVM_INVALID_UDEVID;
    ret = svm_svmm_get_seg(svmm_inst, &devid, &start, &svm_flag, &src_info);
    if (ret != 0) {
        svm_err("Get seg failed. (ret=%d; devid=%u; va=0x%llx)\n", ret, devid, va);
        return ret;
    }

    *handle = (drv_mem_handle_t *)(uintptr_t)src_info.va;
    svm_atomic64_inc(&(*handle)->ref);

    return 0;
}

static int vmm_retain_alloction_handle(u32 devid, u64 va, drv_mem_handle_t **handle)
{
    void *va_handle = NULL;
    int ret;

    va_handle = svm_handle_get(va);
    if (va_handle == NULL) { /* maybe va has been free after get prop */
        svm_err("Get va handle failed. (va=%llx)\n", va);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = _vmm_retain_alloction_handle(devid, va_handle, va, handle);
    svm_handle_put(va_handle);

    return ret;
}

drvError_t halMemRetainAllocationHandle(drv_mem_handle_t **handle, void *ptr)
{
    struct svm_prop prop;
    u64 start = (u64)(uintptr_t)ptr;
    int ret;

    if ((handle == NULL) || (ptr == NULL)) {
        svm_err("Invalid para. (handle=%d; ptr=%d)\n", (handle == NULL), (ptr == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_get_prop(start, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (start=0x%llx)\n", start);
        return ret;
    }

    if ((!svm_flag_cap_is_support_vmm_unmap(prop.flag)) && (!svm_flag_cap_is_support_vmm_ipc_unmap(prop.flag))) {
        svm_err("Not vmm map addr. (va=0x%llx)\n", start);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = vmm_retain_alloction_handle(prop.devid, start, handle);
    if (ret == 0) {
        svm_debug("RetainAllocationHandle. (ptr=0x%llx; start=%llu; size=%u; devid=%u)\n", (u64)(uintptr_t)ptr,
                  prop.start, prop.size, prop.devid);
    }

    return ret;
}

static void vmm_pa_handle_pin(drv_mem_handle_t *handle)
{
    svm_atomic64_inc(&handle->ref);
}

static void vmm_pa_handle_unpin(drv_mem_handle_t *handle)
{
    if (handle->type == SVM_VMM_HANDLE_IMPORT_TYPE) {
        vmm_import_handle_no_ref_destroy(handle);
    } else {
        (void)vmm_free_pa(handle);
    }
}

void vmm_restore_real_src_va(struct svm_global_va *src_info)
{
    drv_mem_handle_t *handle = (drv_mem_handle_t *)(uintptr_t)src_info->va;
    src_info->va = handle->va; /* restore real va */
}

static int vmm_ops_post_map(u32 devid, u64 va, u64 svm_flag, struct svm_global_va *src_info)
{
    u32 i;

    for (i = 0; i < SVM_VMM_OPS_MAX_NUM; ++i) {
        if ((vmm_ops[i] != NULL) && (vmm_ops[i]->post_map != NULL)) {
            int ret = vmm_ops[i]->post_map(devid, va, svm_flag, src_info);
            if (ret != 0) {
                return ret;
            }
        }
    }
    return 0;
}

static int vmm_ops_pre_unmap(u32 devid, u64 va, u64 svm_flag, struct svm_global_va *src_info)
{
    u32 i;

    for (i = 0; i < SVM_VMM_OPS_MAX_NUM; ++i) {
        if ((vmm_ops[i] != NULL) && (vmm_ops[i]->pre_unmap != NULL)) {
            int ret = vmm_ops[i]->pre_unmap(devid, va, svm_flag, src_info);
            if (ret != 0) {
                return ret;
            }
        }
    }
    return 0;
}

static int vmm_do_single_app_mmap(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info)
{
    struct svm_dst_va dst_info;
    u64 size = src_info->size;
    u32 smm_flag = 0;
    int ret;

    svm_dst_va_pack(devid, PROCESS_CP1, start, size, &dst_info);
    ret = svm_smm_client_map(&dst_info, src_info, smm_flag);
    if (ret != 0) {
        svm_err("Smm map failed. (devid=%u; start=%llx; size=%llx)\n", devid, start, size);
        if (ret == DRV_ERROR_NO_RESOURCES) {
            svm_report_ubmem_cfg_abnormal();
            ret = DRV_ERROR_INNER_ERR;
        }
        return ret;
    }
    ret = vmm_ops_post_map(devid, start, svm_flag, src_info);
    if (ret != 0) {
        svm_err("Post map failed. (devid=%u; start=%llx; size=%llx)\n", devid, start, size);
        (void)svm_smm_client_unmap(&dst_info, src_info, smm_flag);
        return ret;
    }

    return 0;
}

static int vmm_do_single_app_unmap(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info)
{
    struct svm_dst_va dst_info;
    u64 size = src_info->size;
    u32 smm_flag = 0;
    int ret;

    ret = vmm_ops_pre_unmap(devid, start, svm_flag, src_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_dst_va_pack(devid, PROCESS_CP1, start, size, &dst_info);
    ret = svm_smm_client_unmap(&dst_info, src_info, smm_flag);
    if (ret != 0) {
        /* no need rollback */
        svm_err("Smm unmap failed. (devid=%u; start=0x%llx; size=0x%llx)\n", devid, start, size);
        return ret;
    }

    return 0;
}

static int vmm_do_cross_app_mmap(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info,
                                 drv_mem_handle_t *handle)
{
    u64 size = src_info->size;
    u64 key = handle->key;
    u32 casm_flag = vmm_map_route_to_casm_flag(handle->map_route);
    int ret;

    SVM_UNUSED(svm_flag);

    ret = svm_casm_mem_map(devid, start, size, key, casm_flag);
    if (ret != 0) {
        svm_err("Casm map failed. (devid=%u; start=%llx; size=%llx)\n", devid, start, size);
        if (ret == DRV_ERROR_NO_RESOURCES) {
            svm_report_ubmem_cfg_abnormal();
            ret = DRV_ERROR_INNER_ERR;
        }
        return ret;
    }
    return 0;
}

static int vmm_do_cross_app_unmap(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info)
{
    u64 size = src_info->size;
    int ret;

    SVM_UNUSED(svm_flag);

    ret = svm_casm_mem_unmap(devid, start, size);
    if (ret != 0) {
        svm_err("Casm unmap failed. (devid=%u; start=%llx; size=%llx)\n", devid, start, size);
    }

    return ret;
}

static int vmm_do_mmap(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info, drv_mem_handle_t *handle)
{
    if (svm_flag_cap_is_support_vmm_ipc_unmap(svm_flag)) {
        return vmm_do_cross_app_mmap(devid, start, svm_flag, src_info, handle);
    } else {
        return vmm_do_single_app_mmap(devid, start, svm_flag, src_info);
    }
}

static int vmm_do_unmap(u32 devid, u64 start, u64 svm_flag, struct svm_global_va *src_info)
{
    if (svm_flag_cap_is_support_vmm_ipc_unmap(svm_flag)) {
        return vmm_do_cross_app_unmap(devid, start, svm_flag, src_info);
    } else {
        return vmm_do_single_app_unmap(devid, start, svm_flag, src_info);
    }
}

struct vmm_dst_map_info {
    u32 devid;
    u64 va;
    u64 svm_flag;
};

struct vmm_src_map_info {
    struct svm_global_va va_info;
    drv_mem_handle_t *pa_handle;
};

static void vmm_src_map_info_pack(struct svm_global_va *va_info, drv_mem_handle_t *pa_handle,
                                  struct vmm_src_map_info *s_info)
{
    s_info->va_info = *va_info;
    s_info->pa_handle = pa_handle;
}

static void vmm_dst_map_info_pack(u32 devid, u64 va, u64 svm_flag, struct vmm_dst_map_info *d_info)
{
    d_info->devid = devid;
    d_info->va = va;
    d_info->svm_flag = svm_flag;
}

static int vmm_save_map_info(void *va_handle, const struct vmm_dst_map_info *d_info,
                             const struct vmm_src_map_info *s_info)
{
    struct svm_global_va src_info = s_info->va_info;
    void *svmm_inst = NULL;
    int ret;

    svmm_inst = vmm_get_svmm(va_handle);
    if (svmm_inst == NULL) { /* vmm map addr first prefetch */
        svm_err("Get svmm inst failed. (va=%llx)\n", d_info->va);
        return DRV_ERROR_PARA_ERROR;
    }

    src_info.va = (u64)(uintptr_t)s_info->pa_handle; /* use va to store handle, get real va from handle */
    ret = svm_svmm_add_seg(svmm_inst, d_info->devid, d_info->va, d_info->svm_flag, &src_info);
    if (ret != 0) {
        svm_err("Add seg failed. (devid=%u; va=%llx; size=%llx)\n", d_info->devid, d_info->va, src_info.size);
        return ret;
    }

    return 0;
}

static int vmm_query_map_info(void *va_handle, u32 devid, u64 dst_va, struct vmm_dst_map_info *d_info,
                              struct vmm_src_map_info *s_info)
{
    void *svmm_inst = NULL;
    drv_mem_handle_t *handle = NULL;
    struct svm_global_va src_info;
    u64 svm_flag;
    int ret;

    svmm_inst = vmm_get_svmm(va_handle);
    if (svmm_inst == NULL) { /* vmm map addr first prefetch */
        svm_debug("Get svmm inst failed. (va=0x%llx)\n", dst_va);
        return DRV_ERROR_PARA_ERROR;
    }

    src_info.udevid = SVM_INVALID_UDEVID;
    ret = svm_svmm_get_seg(svmm_inst, &devid, &dst_va, &svm_flag, &src_info);
    if (ret != 0) {
        svm_debug("Get seg failed. (devid=%u; va=0x%llx)\n", devid, dst_va);
        return ret;
    }

    handle = (drv_mem_handle_t *)(uintptr_t)src_info.va;
    vmm_restore_real_src_va(&src_info);

    vmm_src_map_info_pack(&src_info, handle, s_info);
    vmm_dst_map_info_pack(devid, dst_va, svm_flag, d_info);
    return 0;
}

static int vmm_clear_map_info(void *va_handle, u32 devid, u64 dst_va, u64 size, bool force,
                              struct vmm_dst_map_info *d_info, struct vmm_src_map_info *s_info)
{
    void *svmm_inst = vmm_get_svmm(va_handle);
    int ret;

    if (svmm_inst == NULL) { /* vmm map addr first prefetch */
        svm_err("Get svmm inst failed. (va=0x%llx)\n", dst_va);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = vmm_query_map_info(va_handle, devid, dst_va, d_info, s_info);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Query map info failed. (ret=%d; devid=%u; va=0x%llx; size=0x%llx)\n", ret, devid, dst_va, size);
        return ret;
    }

    /* The svm_flag may be updated by SetAccess after the query. Return the final state when deleting the segment. */
    return svm_svmm_del_seg_with_flag(svmm_inst, devid, dst_va, size, force, &d_info->svm_flag);
}

static int vmm_check_addr_no_map(void *va_handle, u32 devid, u64 dst_va, u64 size)
{
    void *svmm_inst = vmm_get_svmm(va_handle);

    if (svmm_inst == NULL) {
        svm_err("Get svmm inst failed. (va=0x%llx)\n", dst_va);
        return DRV_ERROR_PARA_ERROR;
    }

    return svm_svmm_can_add_seg(svmm_inst, devid, dst_va, size);
}

static int _vmm_mmap(void *va_handle, struct vmm_dst_map_info *d_info, struct vmm_src_map_info *s_info)
{
    u32 devid = d_info->devid;
    u64 dst_va = d_info->va;
    u64 svm_flag = d_info->svm_flag;
    u64 size = s_info->va_info.size;
    bool owner_no_access = svm_flag_is_vmm_map_owner_no_access(svm_flag);
    int ret;

    ret = vmm_check_addr_no_map(va_handle, devid, dst_va, size);
    if (ret != 0) {
        svm_err("Addr has been mapped. (ret=%d; devid=%u; va=0x%llx; size=%llu)\n", ret, devid, dst_va, size);
        return ret;
    }

    if (!owner_no_access) {
        ret = vmm_do_mmap(devid, dst_va, svm_flag, &s_info->va_info, s_info->pa_handle);
        if (ret != 0) {
            return ret;
        }
    }

    ret = vmm_save_map_info(va_handle, d_info, s_info);
    if (ret != DRV_ERROR_NONE) {
        if (!owner_no_access) {
            (void)vmm_do_unmap(devid, dst_va, svm_flag, &s_info->va_info);
        }
        return ret;
    }

    vmm_pa_handle_pin(s_info->pa_handle);
    return 0;
}

static int _vmm_munmap(void *va_handle, u32 devid, u64 dst_va, u64 size)
{
    struct vmm_src_map_info s_info;
    struct vmm_dst_map_info d_info;
    int ret;

    ret = vmm_clear_map_info(va_handle, devid, dst_va, size, false, &d_info, &s_info);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Clear map info failed. (ret=%d; devid=%u; dst_va=0x%llx; size=0x%llx)\n", ret, devid, dst_va, size);
        return ret;
    }

    vmm_pa_handle_unpin(s_info.pa_handle);

    if (!svm_flag_is_vmm_map_owner_no_access(d_info.svm_flag)) {
        ret = vmm_do_unmap(devid, dst_va, d_info.svm_flag, &s_info.va_info);
        if (ret != 0) {
            /* no need rollback */
            return ret;
        }
    }

    return 0;
}

static u64 vmm_get_single_app_map_owner_access_svm_flag_cap(u64 va, u64 size)
{
    u64 svm_flag = 0;

    svm_flag |= SVM_FLAG_CAP_VMM_MAP;
    svm_flag |= SVM_FLAG_CAP_VMM_UNMAP;

    if (!svm_is_in_dcache_va_range(va, size)) {
        svm_flag |= SVM_FLAG_CAP_GET_ATTR;
#ifdef EMU_ST /* Simulation ST support test. */
        svm_flag |= SVM_FLAG_CAP_PREFETCH;
        svm_flag |= SVM_FLAG_CAP_REGISTER | SVM_FLAG_CAP_UNREGISTER;
#endif
        svm_flag |= SVM_FLAG_CAP_REGISTER_ACCESS;
        svm_flag |= SVM_FLAG_CAP_SYNC_COPY | SVM_FLAG_CAP_SYNC_COPY_BATCH;
        svm_flag |= SVM_FLAG_CAP_DMA_DESC_CONVERT | SVM_FLAG_CAP_DMA_DESC_DESTROY | SVM_FLAG_CAP_DMA_DESC_SUBMIT |
                    SVM_FLAG_CAP_DMA_DESC_WAIT;
        svm_flag |= SVM_FLAG_CAP_ASYNC_COPY_SUBMIT | SVM_FLAG_CAP_ASYNC_COPY_WAIT;
        svm_flag |= SVM_FLAG_CAP_SYNC_COPY_2D | SVM_FLAG_CAP_DMA_DESC_CONVERT_2D;
        svm_flag |= SVM_FLAG_CAP_SYNC_COPY_EX;
        svm_flag |= SVM_FLAG_CAP_MEMSET;
        svm_flag |= SVM_FLAG_CAP_GET_ADDR_CHECK_INFO | SVM_FLAG_CAP_GET_MEM_TOKEN_INFO;
        svm_flag |= SVM_FLAG_CAP_GET_D2D_TRANS_WAY;
    } else {
        svm_flag |= SVM_FLAG_CAP_SYNC_COPY;
        svm_flag |= SVM_FLAG_CAP_SYNC_COPY_EX;
        svm_flag |= SVM_FLAG_CAP_GET_ATTR;
    }

    return svm_flag;
}

static u64 vmm_get_single_app_map_owner_no_access_svm_flag_cap(void)
{
    u64 svm_flag = 0;

    svm_flag |= SVM_FLAG_CAP_VMM_MAP;
    svm_flag |= SVM_FLAG_CAP_VMM_UNMAP;
    svm_flag |= SVM_FLAG_CAP_GET_ATTR;

    return svm_flag;
}

static inline u64 vmm_inherit_reserve_va_attr_flag(u64 va)
{
    struct svm_prop prop;
    u64 attr_flag = 0;

    if (svm_get_prop(va, &prop) == 0) {
        attr_flag |= (prop.flag & SVM_FLAG_ATTR_MASTER_UVA);
    }

    return attr_flag;
}

static u64 vmm_get_single_app_mmap_svm_flag(u64 va, u64 size, u64 src_svm_flag, u64 vmm_map_flag)
{
    u64 svm_flag = 0;

    if (vmm_map_flag_is_owner_no_access(vmm_map_flag)) {
        svm_flag |= vmm_get_single_app_map_owner_no_access_svm_flag_cap();
        svm_flag |= SVM_FLAG_VMM_MAP_OWNER_NO_ACCESS;
    } else {
        svm_flag |= vmm_get_single_app_map_owner_access_svm_flag_cap(va, size);
    }

    svm_flag |= src_svm_flag & SVM_FLAG_ATTR_PA_GPAGE;
    svm_flag |= src_svm_flag & SVM_FLAG_ATTR_PA_HPAGE;
    svm_flag |= src_svm_flag & SVM_FLAG_ATTR_PA_CONTIGUOUS;
    svm_flag |= src_svm_flag & SVM_FLAG_ATTR_PA_P2P;
    svm_flag |= vmm_inherit_reserve_va_attr_flag(va);
    svm_flag_set_module_id(&svm_flag, svm_flag_get_module_id(src_svm_flag));

    return svm_flag;
}

static u64 vmm_get_cross_app_owner_no_access_svm_flag_cap(void)
{
    u64 svm_flag = 0;

    svm_flag |= SVM_FLAG_CAP_VMM_IPC_UNMAP;
    svm_flag |= SVM_FLAG_CAP_GET_ATTR;

    return svm_flag;
}

static u64 vmm_get_cross_app_owner_access_svm_flag_cap(u64 va, u64 size)
{
    u64 svm_flag = 0;

    svm_flag |= SVM_FLAG_CAP_VMM_IPC_UNMAP;
    svm_flag |= SVM_FLAG_CAP_GET_ATTR;
    if (!svm_is_in_dcache_va_range(va, size)) {
        svm_flag |= SVM_FLAG_CAP_MEMSET;
        svm_flag |= SVM_FLAG_CAP_SYNC_COPY;
        svm_flag |= SVM_FLAG_CAP_DMA_DESC_CONVERT;
        svm_flag |= SVM_FLAG_CAP_GET_MEM_TOKEN_INFO;
        svm_flag |= SVM_FLAG_CAP_GET_D2D_TRANS_WAY;
    }

    return svm_flag;
}

static u64 vmm_get_cross_app_mmap_svm_flag(u64 va, u64 size, u32 module_id, enum drv_mem_pg_type src_pg_type,
                                           u64 vmm_map_flag)
{
    u64 svm_flag = 0;

    if (vmm_map_flag_is_owner_no_access(vmm_map_flag)) {
        svm_flag |= vmm_get_cross_app_owner_no_access_svm_flag_cap();
        svm_flag |= SVM_FLAG_VMM_MAP_OWNER_NO_ACCESS;
    } else {
        svm_flag |= vmm_get_cross_app_owner_access_svm_flag_cap(va, size);
    }

    /* This relies on the VMM map offset being aligned to the page size specified by src_pg_type. */
    svm_flag |= (src_pg_type == MEM_GIANT_PAGE_TYPE) ?
                    SVM_FLAG_ATTR_PA_GPAGE :
                    ((src_pg_type == MEM_HUGE_PAGE_TYPE) ? SVM_FLAG_ATTR_PA_HPAGE : 0);
    svm_flag |= vmm_inherit_reserve_va_attr_flag(va);
    svm_flag_set_module_id(&svm_flag, module_id);

    return svm_flag;
}

static int vmm_prepare_single_app_map_info(u64 va, u64 size, drv_mem_handle_t *handle, u64 offset, u64 vmm_map_flag,
                                           struct vmm_dst_map_info *d_info, struct vmm_src_map_info *s_info)
{
    u64 handle_start = handle->va;
    struct svm_prop src_prop;
    struct svm_global_va src_info;
    u64 svm_flag;
    u32 udevid;
    int ret;

    ret = svm_get_prop(handle_start, &src_prop);
    if (ret != 0) {
        svm_err("Get src prop failed. (handle_start=%llx)\n", handle_start);
        return ret;
    }

    if ((!svm_is_va_page_align(src_prop.devid, src_prop.flag, va)) ||
        (!svm_is_page_align(src_prop.devid, src_prop.flag, offset)) ||
        (!svm_is_page_align(src_prop.devid, src_prop.flag, size))) {
        svm_err("Not align. (devid=%u; flag=0x%llx; va=0x%llx; size=0x%llx; offset=0x%llx)\n", src_prop.devid,
                src_prop.flag, va, size, offset);
        return DRV_ERROR_PARA_ERROR;
    }

    if (offset >= src_prop.size) {
        svm_err("Map offset size overflows. (offset=0x%llx; pa_size=0x%llx)\n", offset, src_prop.size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((size + offset) > src_prop.size) {
        svm_err("Map size overflows. (size=0x%llx; pa_size=0x%llx; offset=0x%llx)\n", size, src_prop.size, offset);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = uda_get_udevid_by_devid_ex(src_prop.devid, &udevid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get udevid failed. (devid=%u; ret=%d)\n", src_prop.devid, ret);
        return ret;
    }
    svm_global_va_pack(udevid, src_prop.tgid, src_prop.start, size, &src_info);

    svm_flag = vmm_get_single_app_mmap_svm_flag(va, size, src_prop.flag, vmm_map_flag);

    vmm_src_map_info_pack(&src_info, handle, s_info);
    vmm_dst_map_info_pack(src_prop.devid, va, svm_flag, d_info);
    return 0;
}

static int vmm_prepare_cross_app_map_info(u64 va, u64 size, drv_mem_handle_t *handle, u64 offset, u64 vmm_map_flag,
                                          struct vmm_dst_map_info *d_info, struct vmm_src_map_info *s_info)
{
    struct svm_global_va src_info = handle->src_info;
    u32 devid = handle->devid;
    u64 svm_flag;

    if (offset >= src_info.size) {
        svm_err("Map offset size overflows. (offset=0x%llx; pa_size=0x%llx)\n", offset, src_info.size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((size + offset) > src_info.size) {
        svm_err("Map size overflows. (size=0x%llx; pa_size=0x%llx; offset=0x%llx)\n", size, src_info.size, offset);
        return DRV_ERROR_INVALID_VALUE;
    }

    src_info.size = size; /* Adjust to map size. */

    svm_flag = vmm_get_cross_app_mmap_svm_flag(va, size, handle->src_prop.module_id, handle->src_pg_type, vmm_map_flag);

    vmm_src_map_info_pack(&src_info, handle, s_info);
    vmm_dst_map_info_pack(devid, va, svm_flag, d_info);
    return 0;
}

static int vmm_prepare_map_info(u64 va, u64 size, drv_mem_handle_t *handle, u64 offset, u64 vmm_map_flag,
                                struct vmm_dst_map_info *d_info, struct vmm_src_map_info *s_info)
{
    if (handle->type == SVM_VMM_HANDLE_IMPORT_TYPE) {
        return vmm_prepare_cross_app_map_info(va, size, handle, offset, vmm_map_flag, d_info, s_info);
    } else {
        return vmm_prepare_single_app_map_info(va, size, handle, offset, vmm_map_flag, d_info, s_info);
    }
}

static int vmm_mmap(void *va, u64 size, drv_mem_handle_t *handle, u64 offset, u64 vmm_map_flag)
{
    struct vmm_dst_map_info d_info;
    struct vmm_src_map_info s_info;
    void *va_handle = NULL;
    struct svm_prop prop;
    u64 start = (u64)(uintptr_t)va;
    int ret;

    ret = svm_get_prop(start, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (start=%llx)\n", start);
        svm_report_addr_not_allocated("ptr", start);
        return ret;
    }

    if (!svm_flag_cap_is_support_vmm_map(prop.flag)) {
        svm_err("Addr cap does not support vmm map. (start=0x%llx)\n", start);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = vmm_prepare_map_info(start, size, handle, offset, vmm_map_flag, &d_info, &s_info);
    if (ret != 0) {
        svm_err("Prepare map info failed. (ret=%d)\n", ret);
        return ret;
    }

    /* host uva only map host physical address */
    if (svm_flag_attr_is_master_uva(prop.flag) && (s_info.va_info.udevid != svm_get_host_devid())) {
        svm_run_info("Host uva only support host physical. (udevid=%u)\n", s_info.va_info.udevid);
        return DRV_ERROR_NOT_SUPPORT;
    }

    va_handle = svm_handle_get(start);
    if (va_handle == NULL) { /* maybe va has been free after get prop */
        svm_err("Get va handle failed. (start=%llx)\n", start);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = _vmm_mmap(va_handle, &d_info, &s_info);
    svm_handle_put(va_handle);

    return ret;
}

static int vmm_munmap(void *va)
{
    void *va_handle = NULL;
    struct svm_prop prop;
    u64 start = (u64)(uintptr_t)va;
    int ret;

    ret = svm_get_prop(start, &prop);
    if (ret != 0) {
        /* The log cannot be modified, because in the failure mode library. */
        svm_err("Vmm unmap find addr failed. (start=0x%llx)\n", start);
        svm_report_addr_not_allocated("ptr", start);
        return ret;
    }

    if (!svm_flag_cap_is_support_vmm_unmap(prop.flag) && !svm_flag_cap_is_support_vmm_ipc_unmap(prop.flag)) {
        /* The log cannot be modified, because in the failure mode library. */
        svm_err("Addr can not be unmapped. (va=0x%llx)\n", start);
        return DRV_ERROR_PARA_ERROR;
    }

    va_handle = svm_handle_get(start);
    if (va_handle == NULL) { /* maybe va has been free after get prop */
        svm_err("Get va handle failed. (start=%llx)\n", start);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = _vmm_munmap(va_handle, prop.devid, start, prop.aligned_size);
    svm_handle_put(va_handle);

    return ret;
}

static void vmm_report_mmap_align_error(void *ptr, size_t size, drv_mem_handle_t *handle)
{
    u64 page_size, dst_npage_size;
    int ret;

    page_size = vmm_get_handle_pg_size(handle, size);
    ret = svm_dbi_query_npage_size(handle->devid, &dst_npage_size);
    if ((ret == DRV_ERROR_NONE) && (dst_npage_size > page_size)) {
        page_size = dst_npage_size;
    }

    if (!SVM_IS_ALIGNED((u64)(uintptr_t)ptr, page_size)) {
        svm_report_addr_not_aligned("ptr", (u64)(uintptr_t)ptr, page_size);
    } else if (!SVM_IS_ALIGNED(size, page_size)) {
        svm_report_size_not_aligned("size", size, page_size);
    }
}

static int vmm_mmap_para_check(void *ptr, u64 size, u64 offset, drv_mem_handle_t *handle, u64 flag)
{
    int ret;
    u64 aligned, page_size;

    if ((ptr == NULL) || (handle == NULL) || (size == 0)) {
        svm_err("Invalid para. (ptr=%u; handle=%u; size=%u)\n", (ptr == NULL), (handle == NULL), size);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* reserved para, verify by zero for subsequent compatibility */
    if (offset != 0) {
        svm_debug("Offset should be zero. (offset=%llu)\n", offset);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* reserved para, verify by zero for subsequent compatibility */
    if (flag != 0) {
        svm_debug("Flag should be zero. (flag=0x%llx)\n", flag);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_share_get_dst_align((u64)(uintptr_t)ptr, size, handle->devid, &aligned);
    if (ret != 0) {
        svm_err("Not aligned. (ptr=0x%llx; size=%llu; devid=%u)\n", (u64)(uintptr_t)ptr, size, handle->devid);
        vmm_report_mmap_align_error(ptr, size, handle);
        return DRV_ERROR_PARA_ERROR;
    }

    page_size = vmm_get_handle_pg_size(handle, size);
    if (page_size > aligned) {
        svm_err("Not aligned. (ptr=0x%llx; pg_type=%u; size=%llu; module_id=%u; page_size=%llu; aligned=%llu)\n",
                (u64)(uintptr_t)ptr, handle->src_prop.pg_type, size, handle->src_prop.module_id, page_size, aligned);
        if (!SVM_IS_ALIGNED((u64)(uintptr_t)ptr, page_size)) {
            svm_report_addr_not_aligned("ptr", (u64)(uintptr_t)ptr, page_size);
        } else if (!SVM_IS_ALIGNED(size, page_size)) {
            svm_report_size_not_aligned("size", size, page_size);
        }
        return DRV_ERROR_PARA_ERROR;
    }
    return DRV_ERROR_NONE;
}

static int hal_vmm_mmap(void *ptr, u64 size, u64 offset, drv_mem_handle_t *handle, u64 flag, u64 vmm_map_flag)
{
    int ret;

    ret = vmm_mmap_para_check(ptr, size, offset, handle, flag);
    if (ret != 0) {
        return ret;
    }

    svm_use_pipeline();
    ret = vmm_mmap(ptr, (u64)size, handle, (u64)offset, vmm_map_flag);
    svm_unuse_pipeline();
    if (ret == 0) {
        svm_debug("Vmm map. (va=0x%llx; size=%llu; handle_va=0x%llx; devid=%u; key=%llu; flag=0x%llx)\n",
                  (u64)(uintptr_t)ptr, size, handle->va, handle->devid, handle->key, vmm_map_flag);
    }

    return (drvError_t)ret;
}

drvError_t halMemMap(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle, uint64_t flag)
{
    DRV_CURRENT_API_SCOPE();
    return (drvError_t)hal_vmm_mmap(ptr, size, offset, handle, flag, 0);
}

drvError_t halMemMapNoAccess(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle, uint64_t flag)
{
    DRV_CURRENT_API_SCOPE();
    return (drvError_t)hal_vmm_mmap(ptr, size, offset, handle, flag, VMM_MAP_FLAG_OWNER_NO_ACCESS);
}

drvError_t halMemUnmap(void *ptr)
{
    int ret;

    DRV_CURRENT_API_SCOPE();

    if (ptr == NULL) {
        svm_err("Ptr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_debug("Vmm Unmap. (va=0x%llx)\n", (u64)(uintptr_t)ptr);

    svm_use_pipeline();
    ret = vmm_munmap(ptr);
    svm_unuse_pipeline();

    return (drvError_t)ret;
}

static int vmm_query_map_info_by_va(u64 va, struct vmm_dst_map_info *d_info, struct vmm_src_map_info *s_info)
{
    void *va_handle = NULL;
    int ret;

    va_handle = svm_handle_get(va);
    if (va_handle == NULL) {
        return DRV_ERROR_NOT_EXIST;
    }

    if (vmm_get_svmm(va_handle) == NULL) {
        svm_handle_put(va_handle);
        return DRV_ERROR_NOT_EXIST;
    }

    ret = vmm_query_map_info(va_handle, SVM_INVALID_DEVID, va, d_info, s_info);
    svm_handle_put(va_handle);
    return ret;
}

int vmm_query_src_info(u64 va, u64 size, struct svm_global_va *src_info)
{
    struct vmm_src_map_info s_info;
    struct vmm_dst_map_info d_info;
    u64 offset;
    int ret;

    ret = vmm_query_map_info_by_va(va, &d_info, &s_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    offset = va - d_info.va;
    if ((offset >= s_info.va_info.size) || (size > (s_info.va_info.size - offset))) {
        svm_debug("Is vmm mixed seg. (va=0x%llx; size=0x%llx; seg_va=0x%llx; seg_size=0x%llx)\n", va, size, d_info.va,
                  s_info.va_info.size);
        return DRV_ERROR_INVALID_VALUE;
    }

    *src_info = s_info.va_info;
    src_info->va += offset;
    src_info->size = size;
    return DRV_ERROR_NONE;
}

static void svm_fill_share_handle(struct svm_share_handle *share_handle, drv_mem_handle_type handle_type, u64 key,
                                  struct drv_mem_prop *src_prop)
{
    struct svm_global_va src_va = {0};
    struct svm_global_va tmp_src_va;
    int owner_pid = 0;
    int tmp_owner_pid;
    int cs_valid = 0;

    share_handle->handle_type = handle_type;
    share_handle->key = key;

    if (handle_type == MEM_HANDLE_TYPE_FABRIC) {
        int ret = svm_casm_cs_query_src_info(key, &tmp_src_va, &tmp_owner_pid);
        if (ret == 0) {
            if (tmp_src_va.server_id != SVM_INVALID_SERVER_ID) {
                cs_valid = 1;
                src_va = tmp_src_va;
                owner_pid = tmp_owner_pid;
            }
        }
    }

    share_handle->cs_valid = cs_valid;
    share_handle->owner_pid = owner_pid;
    share_handle->src_va = src_va;
    share_handle->src_prop_valid = 1;
    share_handle->src_prop = *src_prop;
}

static void vmm_casm_src_prop_pack(enum drv_mem_pg_type src_pg_type, struct casm_src_prop *prop)
{
    prop->casm_pg_type = (src_pg_type == MEM_GIANT_PAGE_TYPE) ?
                             CASM_GPAGE :
                             ((src_pg_type == MEM_HUGE_PAGE_TYPE) ? CASM_HPAGE : CASM_NPAGE);
}

static enum drv_mem_pg_type vmm_casm_pg_type_to_drv(enum casm_pg_type casm_pg_type)
{
    return (casm_pg_type == CASM_GPAGE) ? MEM_GIANT_PAGE_TYPE :
                                          ((casm_pg_type == CASM_HPAGE) ? MEM_HUGE_PAGE_TYPE : MEM_NORMAL_PAGE_TYPE);
}

static void vmm_legacy_src_prop_pack(u64 key, const struct casm_src_prop *casm_prop, struct drv_mem_prop *src_prop)
{
    u32 devid = casm_key_to_devid(key);

    src_prop->side = (devid == svm_get_host_devid()) ? MEM_HOST_SIDE : MEM_DEV_SIDE;
    src_prop->devid = (devid == svm_get_host_devid()) ? 0 : devid;
    src_prop->module_id = APP_MODULE_ID;
    src_prop->pg_type = vmm_casm_pg_type_to_drv(casm_prop->casm_pg_type);
    src_prop->mem_type = MEM_HBM_TYPE;
    src_prop->reserve = 0;
}

static int vmm_export(drv_mem_handle_t *handle, drv_mem_handle_type handle_type, struct svm_share_handle *share_handle)
{
    struct casm_src_prop src_prop;
    struct svm_prop prop;
    struct svm_dst_va dst_va;
    u64 start = handle->va;
    u64 key;
    int ret;

    ret = svm_get_prop(start, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (start=%llx)\n", start);
        return ret;
    }

    if (!svm_flag_cap_is_support_vmm_export(prop.flag)) {
        svm_err("Addr cap does not support vmm export. (start=0x%llx)\n", start);
        return DRV_ERROR_PARA_ERROR;
    }

    if (prop.devid == svm_get_host_devid()) {
        svm_dst_va_pack(prop.devid, DEVDRV_PROCESS_CPTYPE_MAX, prop.start, prop.aligned_size, &dst_va);
    } else {
        svm_dst_va_pack(prop.devid, DEVDRV_PROCESS_CP1, prop.start, prop.aligned_size, &dst_va);
    }

    vmm_casm_src_prop_pack(svm_flag_attr_is_gpage(prop.flag) ?
                               MEM_GIANT_PAGE_TYPE :
                               (svm_flag_attr_is_hpage(prop.flag) ? MEM_HUGE_PAGE_TYPE : MEM_NORMAL_PAGE_TYPE),
                           &src_prop);
    svm_use_pipeline();
    ret = svm_casm_create_key(&dst_va, &src_prop, &key);
    if (ret == 0) {
        handle->type = SVM_VMM_HANDLE_EXPORT_TYPE;
        handle->key = key;
        svm_fill_share_handle(share_handle, handle_type, key, &handle->src_prop);
    }
    svm_unuse_pipeline();

    return ret;
}

/* dir: dst device process va -> src device pa */
static int vmm_dir_check(u32 dst_devid, const struct svm_global_va *src_va)
{
#define CONNECT_TYPE_NUM (HOST_DEVICE_CONNECT_TYPE_UB + 1)
    u32 host_devid = svm_get_host_devid();

    if (src_va->udevid == host_devid) { /* D2H, D2rH */
        /* D2H is same with D2rH */
        /* not support connect with pcie, because pcie th va is not same */
        int d2h_support_flag[CONNECT_TYPE_NUM] = {
            [HOST_DEVICE_CONNECT_TYPE_HCCS] = 1, /* use global pa */
            [HOST_DEVICE_CONNECT_TYPE_UB] = 1,   /* use memory decoder */
        };
        if (dst_devid != host_devid) {
            u32 connect_type = svm_get_device_connect_type(dst_devid);
            if (connect_type > HOST_DEVICE_CONNECT_TYPE_UB) {
                svm_err("Invalid connect type. (devid=%u; connect_type=%u)\n", dst_devid, connect_type);
                return DRV_ERROR_INVALID_VALUE;
            }

            return (d2h_support_flag[connect_type] == 1) ? 0 : DRV_ERROR_NOT_SUPPORT;
        }
    }

    return 0;
}

static int vmm_import_host_npage_check(u32 dst_devid, const struct svm_global_va *src_va,
                                       const struct casm_src_prop *src_prop)
{
    if ((dst_devid != svm_get_host_devid()) && (src_va->udevid == svm_get_host_devid()) &&
        (src_prop->casm_pg_type == CASM_NPAGE)) {
        svm_run_info("Importing host normal-page memory to a device is not supported. (devid=%u)\n", dst_devid);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

struct vmm_import_context {
    struct casm_src_va_ex_info src_ex_info;
    struct drv_mem_prop src_prop;
    struct svm_global_va src_va;
    bool cs_info_set;
    bool dir_checked;
};

static int vmm_import_cs_info_set(const struct svm_share_handle *share_handle, u32 devid,
                                  struct vmm_import_context *context)
{
    struct casm_src_prop casm_src_prop;
    u64 key = share_handle->key;
    int ret;

    if ((share_handle->handle_type != MEM_HANDLE_TYPE_FABRIC) || (share_handle->cs_valid == 0)) {
        return 0;
    }

    ret = vmm_dir_check(devid, &share_handle->src_va);
    if (ret != 0) {
        return ret;
    }
    context->dir_checked = true;

    if (share_handle->src_va.server_id == svm_get_cur_server_id()) {
        return 0;
    }

    if (!share_handle->src_prop_valid) {
        svm_err("Legacy shareable handle does not support cross-server import. (key=0x%llx)\n", key);
        return DRV_ERROR_NOT_SUPPORT;
    }

    vmm_casm_src_prop_pack(vmm_get_real_pg_type(share_handle->src_prop.pg_type, share_handle->src_prop.module_id,
                                                share_handle->src_va.size),
                           &casm_src_prop);
    ret = svm_casm_cs_set_src_info(devid, key, &share_handle->src_va, share_handle->owner_pid, &casm_src_prop);
    if (ret != 0) {
        svm_err("Set cs src failed. (key=0x%llx)\n", key);
        return ret;
    }
    context->cs_info_set = true;

    return 0;
}

static void vmm_import_cs_info_clear(const struct svm_share_handle *share_handle, u32 devid,
                                     const struct vmm_import_context *context)
{
    if (context->cs_info_set) {
        (void)svm_casm_cs_clr_src_info(devid, share_handle->key);
    }
}

static int vmm_import_src_info_query(const struct svm_share_handle *share_handle, u32 devid,
                                     struct vmm_import_context *context)
{
    u64 key = share_handle->key;
    int ret;

    ret = svm_casm_get_src_va_ex(devid, key, &context->src_va, &context->src_ex_info);
    if (ret != 0) {
        svm_err("Get src va failed. (key=0x%llx)\n", key);
        return ret;
    }

    ret = vmm_import_host_npage_check(devid, &context->src_va, &context->src_ex_info.src_prop);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (!context->dir_checked) {
        ret = vmm_dir_check(devid, &context->src_va);
        if (ret != 0) {
            return ret;
        }
    }

    if (!share_handle->src_prop_valid) {
        vmm_legacy_src_prop_pack(key, &context->src_ex_info.src_prop, &context->src_prop);
    } else {
        context->src_prop = share_handle->src_prop;
    }

    return 0;
}

static int vmm_import(struct svm_share_handle *share_handle, u32 devid, drv_mem_handle_t **handle)
{
    struct vmm_import_context context = {0};
    u64 key = share_handle->key;
    int ret;

    ret = vmm_import_cs_info_set(share_handle, devid, &context);
    if (ret != 0) {
        return ret;
    }

    ret = vmm_import_src_info_query(share_handle, devid, &context);
    if (ret != 0) {
        vmm_import_cs_info_clear(share_handle, devid, &context);
        return ret;
    }

    *handle = vmm_import_handle_create(devid, key, &context.src_prop,
                                       vmm_casm_pg_type_to_drv(context.src_ex_info.src_prop.casm_pg_type),
                                       &context.src_va);
    if (*handle == NULL) {
        vmm_import_cs_info_clear(share_handle, devid, &context);
        svm_err("Import handle create failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    (*handle)->clr_cs_flag = share_handle->cs_valid;

    return 0;
}

drvError_t halMemExportToShareableHandleV2(drv_mem_handle_t *handle, drv_mem_handle_type handle_type, uint64_t flags,
                                           struct MemShareHandle *share_handle)
{
    struct svm_share_handle *_share_handle = (struct svm_share_handle *)(void *)share_handle;
    int ret;

    if (share_handle == NULL) {
        svm_err("shareable_handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_type >= MEM_HANDLE_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (flags != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (handle == NULL) {
        svm_err("Handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle->type != SVM_VMM_HANDLE_NORMAL_TYPE) {
        /* The log cannot be modified, because in the failure mode library. */
        svm_err("Handle type should be normal type. (type=%u)\n", handle->type);
        return DRV_ERROR_PARA_ERROR;
    }

    svm_use_pipeline();
    ret = vmm_export(handle, handle_type, _share_handle);
    svm_unuse_pipeline();

    svm_debug("Vmm export. (handle_type=%u; devid=%u; key=%llu; handle_va=0x%llx; cs_valid=%d; "
              "owner_pid=%d; key=%llu)\n",
              handle_type, handle->devid, handle->key, handle->va, _share_handle->cs_valid, _share_handle->owner_pid,
              _share_handle->key);

    return (drvError_t)ret;
}

drvError_t halMemExportToShareableHandle(drv_mem_handle_t *handle, drv_mem_handle_type handle_type, uint64_t flags,
                                         uint64_t *shareable_handle)
{
    struct svm_share_handle _share_handle;
    int ret;

    (void)flags;
    if (shareable_handle == NULL) {
        svm_err("shareable_handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_type != MEM_HANDLE_TYPE_NONE) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = halMemExportToShareableHandleV2(handle, handle_type, 0, (struct MemShareHandle *)(void *)&_share_handle);
    if (ret == 0) {
        *shareable_handle = _share_handle.key;
    }

    return (drvError_t)ret;
}

static drvError_t vmm_share_mem_set_pid_para_check(uint64_t shareable_handle, int pid[], uint32_t pid_num)
{
    SVM_UNUSED(shareable_handle);

#define SVM_SHARE_MEM_MAX_PID_CNT 65535 /* > 64 process * 384 server node * 2 die */
    if (pid == NULL) {
        svm_err("Pid is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pid_num == 0) {
        svm_err("Pid num is zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pid_num > SVM_SHARE_MEM_MAX_PID_CNT) {
        svm_err("Pid num is invalid. (pid_num=%u)\n", pid_num);
        return DRV_ERROR_INVALID_VALUE;
    }
    return 0;
}

drvError_t halMemImportFromShareableHandleV2(drv_mem_handle_type handle_type, struct MemShareHandle *share_handle,
                                             uint32_t devid, drv_mem_handle_t **handle)
{
    struct svm_share_handle *_share_handle = (struct svm_share_handle *)(void *)share_handle;
    int ret;

    DRV_CURRENT_API_SCOPE();

    if (handle == NULL) {
        svm_err("Handle is null.\n");
        svm_report_null_pointer("handle");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (share_handle == NULL) {
        svm_err("Share handle is null.\n");
        svm_report_null_pointer("share_handle");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (devid >= SVM_MAX_DEV_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if ((handle_type >= MEM_HANDLE_TYPE_MAX) || (handle_type != _share_handle->handle_type)) {
        svm_err("Invalid para. (handle_type=%u)\n", handle_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_use_pipeline();
    ret = vmm_import(_share_handle, devid, handle);
    svm_unuse_pipeline();
    if (ret == DRV_ERROR_INVALID_HANDLE) {
        svm_report_invalid_handle("halMemExportToShareableHandle", _share_handle->key);
    }
    svm_debug("Vmm import. (ret=%d; handle_type=%u; devid=%u; cs_valid=%d; owner_pid=%d; key=%llu; "
              "handle_va=0x%llx)\n",
              ret, handle_type, devid, _share_handle->cs_valid, _share_handle->owner_pid, _share_handle->key,
              (ret == 0) ? (*handle)->va : 0);

    return (drvError_t)ret;
}

drvError_t halMemImportFromShareableHandle(uint64_t shareable_handle, uint32_t devid, drv_mem_handle_t **handle)
{
    struct svm_share_handle _share_handle = {0};

    _share_handle.handle_type = MEM_HANDLE_TYPE_NONE;
    _share_handle.key = shareable_handle;
    _share_handle.cs_valid = 0;
    _share_handle.src_prop_valid = 0;

    return halMemImportFromShareableHandleV2(MEM_HANDLE_TYPE_NONE, (struct MemShareHandle *)(void *)&_share_handle,
                                             devid, handle);
}

static inline int vmm_set_mwl_attr(u64 shareable_handle, struct ShareHandleAttr *attr)
{
    u64 key = shareable_handle;
    int tgid = SVM_ANY_TASK_ID;

    if (attr->enableFlag == 0) {
        svm_run_info("Not support disable mwl.\n");
        return DRV_ERROR_NOT_SUPPORT;
    } else if (attr->enableFlag == 1) {
        return svm_casm_add_local_task(key, &tgid, 1);
    } else {
        svm_err("Invalid share handle mwl attr val. (val=%u)\n", attr->enableFlag);
        return DRV_ERROR_PARA_ERROR;
    }
}

static inline int vmm_get_mwl_attr(u64 shareable_handle, struct ShareHandleAttr *attr)
{
    u64 key = shareable_handle;
    int tgid = SVM_ANY_TASK_ID;
    int ret;

    ret = svm_casm_check_local_task(key, &tgid, 1);
    if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_NO_RESOURCES)) {
        svm_err("Check any task failed. (ret=%d; key=0x%llx)\n", ret, key);
        return ret;
    }

    attr->enableFlag = (ret == DRV_ERROR_NONE) ? 1 : 0;
    return DRV_ERROR_NONE;
}

drvError_t halMemShareHandleSetAttribute(uint64_t shareable_handle, enum ShareHandleAttrType type,
                                         struct ShareHandleAttr attr)
{
    static int (*vmm_set_attr_func[SHR_HANDLE_ATTR_TYPE_MAX])(
        u64 shareable_handle, struct ShareHandleAttr *attr) = {[SHR_HANDLE_ATTR_NO_WLIST_IN_SERVER] = vmm_set_mwl_attr};
    int ret;

    if ((type >= SHR_HANDLE_ATTR_TYPE_MAX) || (vmm_set_attr_func[type] == NULL)) {
        svm_debug("Not support type. (type=%d)\n", type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = vmm_set_attr_func[type](shareable_handle, &attr);
    if (ret == DRV_ERROR_NONE) {
        svm_debug("Share handle set attr successfully. (shareable_handle=0x%llx; type=%u)\n", shareable_handle, type);
    }

    return ret;
}

drvError_t halMemShareHandleGetAttribute(uint64_t shareable_handle, enum ShareHandleAttrType type,
                                         struct ShareHandleAttr *attr)
{
    static int (*vmm_get_attr_func[SHR_HANDLE_ATTR_TYPE_MAX])(
        u64 shareable_handle, struct ShareHandleAttr *attr) = {[SHR_HANDLE_ATTR_NO_WLIST_IN_SERVER] = vmm_get_mwl_attr};
    int ret;

    if (attr == NULL) {
        svm_err("Attr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((type >= SHR_HANDLE_ATTR_TYPE_MAX) || (vmm_get_attr_func[type] == NULL)) {
        svm_debug("Not support type. (type=%d)\n", type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = vmm_get_attr_func[type](shareable_handle, attr);
    if (ret == DRV_ERROR_NONE) {
        svm_debug("Share handle get attr successfully. (shareable_handle=0x%llx; type=%u)\n", shareable_handle, type);
    }

    return ret;
}

drvError_t halMemShareHandleInfoGet(uint64_t shareable_handle, struct ShareHandleGetInfo *info)
{
    struct svm_global_va src_va;
    u64 key = shareable_handle;
    int ret;

    if (info == NULL) {
        svm_err("Info is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_casm_get_src_va(svm_get_host_devid(), key, &src_va);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Casm get src va failed. (ret=%d; key=0x%llx)\n", ret, key);
        return ret;
    }

    info->phyDevid = src_va.udevid;

    svm_debug("Share handle get info successfully. (shareable_handle=0x%llx; phy_devid=%u)\n", shareable_handle,
              info->phyDevid);
    return DRV_ERROR_NONE;
}

static u32 vmm_access_location_to_devid(struct drv_mem_location *location)
{
    if (location->side == MEM_DEV_SIDE) {
        return location->id;
    } else {
        return svm_get_host_devid();
    }
}

static int vmm_access_location_check(struct drv_mem_location *location)
{
    if (location->side == MEM_HOST_SIDE) {
        if (location->id != 0) {
            svm_err("Invalid host id. (id=%u)\n", location->id);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else if (location->side == MEM_DEV_SIDE) {
        if (location->id >= SVM_MAX_DEV_AGENT_NUM) {
            svm_err("Invalid dev id. (id=%u)\n", location->id);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else {
        svm_err("Invalid side. (side=%u)\n", location->side);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static int vmm_access_desc_check(struct drv_mem_access_desc *desc, size_t count)
{
    size_t i;

    if ((desc == NULL) || (count <= 0) || (count > SVM_MAX_DEV_NUM)) {
        svm_err("Invalid desc. (count=%u)\n", (u32)count);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < count; i++) {
        int ret;

        if (desc[i].type == MEM_ACCESS_TYPE_READ) {
            svm_run_info("Not support type. (index=%u; type=%u)\n", i, desc[i].type);
            return DRV_ERROR_NOT_SUPPORT;
        }

        if ((desc[i].type != MEM_ACCESS_TYPE_NONE) && (desc[i].type != MEM_ACCESS_TYPE_READ) &&
            (desc[i].type != MEM_ACCESS_TYPE_READWRITE)) {
            svm_err("Invalid type. (index=%u; type=%u)\n", i, desc[i].type);
            return DRV_ERROR_INVALID_VALUE;
        }

        ret = vmm_access_location_check(&desc[i].location);
        if (ret != DRV_ERROR_NONE) {
            svm_err("Invalid location. (index=%u)\n", i);
            return ret;
        }

        svm_debug("SetAccess. (i=%llu; count=%llu; type=%u; id=%u; side=%u)\n", i, count, desc[i].type,
                  desc[i].location.id, desc[i].location.side);
    }

    return DRV_ERROR_NONE;
}

static int vmm_access_ptr_check(void *ptr)
{
    struct svm_prop prop;
    int ret;

    if (ptr == NULL) {
        svm_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_get_prop((u64)(uintptr_t)ptr, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (ptr=0x%llx)\n", (u64)(uintptr_t)ptr);
        return ret;
    }

    /* host uva not support set access to share with other devices */
    if (svm_flag_attr_is_master_uva(prop.flag)) {
        svm_run_info("Host uva not support set access. (ptr=0x%llx)\n", (u64)(uintptr_t)ptr);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return 0;
}

static struct svm_vmm_access_node *vmm_get_access_node(void *seg_handle)
{
    struct svm_vmm_access_node *access_node = svm_svmm_get_seg_priv(seg_handle);
    return ((access_node != NULL) && (access_node->head.type == SVM_SHARE_TYPE_VMM_ACCESS)) ? access_node : NULL;
}

static struct svm_vmm_access_node *vmm_access_node_create(u32 owner_devid, u64 va, u64 size,
                                                          struct svm_global_va *src_info, void *seg_handle,
                                                          drv_mem_access_type owner_access_type)
{
    struct svm_vmm_access_node *access_node = NULL;
    u32 i;

    access_node = (struct svm_vmm_access_node *)svm_ua_calloc(1, sizeof(*access_node));
    if (access_node == NULL) {
        svm_err("Malloc failed. (owner_devid=%u; va=0x%llx; size=0x%llx)\n", owner_devid, va, size);
        return NULL;
    }

    access_node->head.type = SVM_SHARE_TYPE_VMM_ACCESS;

    access_node->owner_devid = owner_devid;
    access_node->va = va;
    access_node->size = size;
    access_node->handle = (drv_mem_handle_t *)(uintptr_t)src_info->va;
    access_node->src_info = *src_info;
    access_node->seg_handle = seg_handle;
    vmm_restore_real_src_va(&access_node->src_info);

    for (i = 0; i < SVM_MAX_DEV_NUM; i++) {
        if (i == access_node->owner_devid) {
            access_node->access_type[i] = owner_access_type;
        } else {
            access_node->access_type[i] = MEM_ACCESS_TYPE_NONE;
        }
    }

    (void)pthread_rwlock_init(&access_node->rwlock, NULL);

    return access_node;
}

static void vmm_access_node_destroy(struct svm_vmm_access_node *access_node)
{
    (void)pthread_rwlock_destroy(&access_node->rwlock);
    svm_ua_free(access_node);
}

static int vmm_access_non_owner_do_mmap(struct svm_vmm_access_node *access_node, u32 devid, drv_mem_access_type type)
{
    drv_mem_handle_t *handle = access_node->handle;
    int ret;

    if (handle->type == SVM_VMM_HANDLE_IMPORT_TYPE) {
        u32 casm_flag = (type == MEM_ACCESS_TYPE_READ) ? SVM_CASM_FLAG_PG_RDONLY : 0;
        ret = svm_casm_mem_map(devid, access_node->va, access_node->size, handle->key, casm_flag);
    } else {
        u32 smm_flag = (type == MEM_ACCESS_TYPE_READ) ? SVM_SMM_FLAG_PG_RDONLY : 0;
        struct svm_dst_va dst_info;

        svm_dst_va_pack(devid, PROCESS_CP1, access_node->va, access_node->size, &dst_info);
        ret = svm_smm_client_map(&dst_info, &access_node->src_info, smm_flag);
    }
    if (ret != 0) { /* not need rollback */
        svm_err("Mmap failed. (devid=%u; va=%llx; size=%llx; handle type=%u)\n", devid, access_node->va,
                access_node->size, handle->type);
        if (ret == DRV_ERROR_NO_RESOURCES) {
            svm_report_ubmem_cfg_abnormal();
            ret = DRV_ERROR_INNER_ERR;
        }
        return ret;
    }

    return 0;
}

static int vmm_access_non_owner_do_unmap(struct svm_vmm_access_node *access_node, u32 devid)
{
    drv_mem_handle_t *handle = access_node->handle;
    int ret;

    if (handle->type == SVM_VMM_HANDLE_IMPORT_TYPE) {
        ret = svm_casm_mem_unmap(devid, access_node->va, access_node->size);
    } else {
        u32 smm_flag = 0;
        struct svm_dst_va dst_info;

        svm_dst_va_pack(devid, PROCESS_CP1, access_node->va, access_node->size, &dst_info);
        ret = svm_smm_client_unmap(&dst_info, &access_node->src_info, smm_flag);
    }
    if (ret != 0) {
        svm_warn("Unmap failed. (devid=%u; va=%llx; size=%llx)\n", devid, access_node->va, access_node->size);
    }

    return ret;
}

static u64 vmm_access_get_owner_mapped_svm_flag(struct svm_vmm_access_node *access_node, u64 svm_flag)
{
    if (access_node->handle->type == SVM_VMM_HANDLE_IMPORT_TYPE) {
        return vmm_get_cross_app_mmap_svm_flag(access_node->va, access_node->size, svm_flag_get_module_id(svm_flag),
                                               access_node->handle->src_pg_type, 0);
    } else {
        return vmm_get_single_app_mmap_svm_flag(access_node->va, access_node->size, svm_flag, 0);
    }
}

static u64 vmm_access_get_owner_no_access_svm_flag(struct svm_vmm_access_node *access_node, u64 svm_flag)
{
    if (access_node->handle->type == SVM_VMM_HANDLE_IMPORT_TYPE) {
        return vmm_get_cross_app_mmap_svm_flag(access_node->va, access_node->size, svm_flag_get_module_id(svm_flag),
                                               access_node->handle->src_pg_type, VMM_MAP_FLAG_OWNER_NO_ACCESS);
    } else {
        return vmm_get_single_app_mmap_svm_flag(access_node->va, access_node->size, svm_flag,
                                                VMM_MAP_FLAG_OWNER_NO_ACCESS);
    }
}

static int vmm_access_owner_do_mmap(struct svm_vmm_access_node *access_node, u32 devid)
{
    u64 svm_flag = svm_svmm_get_seg_svm_flag(access_node->seg_handle);
    int ret;

    if (svm_flag_is_vmm_map_owner_no_access(svm_flag)) {
        svm_flag = vmm_access_get_owner_mapped_svm_flag(access_node, svm_flag);
    }

    ret = vmm_do_mmap(devid, access_node->va, svm_flag, &access_node->src_info, access_node->handle);
    if (ret != 0) {
        return ret;
    }

    svm_svmm_mod_seg_svm_flag(access_node->seg_handle, svm_flag);
    return 0;
}

static int vmm_access_owner_do_unmap(struct svm_vmm_access_node *access_node, u32 devid)
{
    u64 svm_flag = svm_svmm_get_seg_svm_flag(access_node->seg_handle);
    int ret;

    ret = vmm_do_unmap(devid, access_node->va, svm_flag, &access_node->src_info);
    if (ret != 0) {
        return ret;
    }

    svm_flag = vmm_access_get_owner_no_access_svm_flag(access_node, svm_flag);
    svm_svmm_mod_seg_svm_flag(access_node->seg_handle, svm_flag);
    return 0;
}

static int vmm_access_single_do_mmap(struct svm_vmm_access_node *access_node, u32 devid, drv_mem_access_type type)
{
    int ret;

    if (devid == access_node->handle->devid) {
        /* TODO: Pass type to owner mmap when MEM_ACCESS_TYPE_READ is supported. */
        ret = vmm_access_owner_do_mmap(access_node, devid);
    } else {
        ret = vmm_access_non_owner_do_mmap(access_node, devid, type);
    }
    if (ret == 0) {
        access_node->access_type[devid] = type;
    }

    return ret;
}

static int vmm_access_single_do_unmap(struct svm_vmm_access_node *access_node, u32 devid)
{
    int ret;

    if (devid == access_node->owner_devid) {
        ret = vmm_access_owner_do_unmap(access_node, devid);
    } else {
        ret = vmm_access_non_owner_do_unmap(access_node, devid);
    }
    if (ret == 0) {
        access_node->access_type[devid] = MEM_ACCESS_TYPE_NONE;
    }

    return ret;
}

static void vmm_access_non_owner_unmap(struct svm_vmm_access_node *access_node, u32 devid)
{
    (void)vmm_access_non_owner_do_unmap(access_node, devid);
    access_node->access_type[devid] = MEM_ACCESS_TYPE_NONE;
}

static int vmm_access_node_release(void *priv, bool force)
{
    struct svm_vmm_access_node *access_node = (struct svm_vmm_access_node *)priv;
    u32 i;
    SVM_UNUSED(force);

    for (i = 0; i < SVM_MAX_DEV_NUM; i++) {
        if ((i == access_node->owner_devid) || (access_node->access_type[i] == MEM_ACCESS_TYPE_NONE)) {
            continue;
        }

        vmm_access_non_owner_unmap(access_node, i);
    }

    vmm_access_node_destroy(access_node);

    return 0;
}

static u32 vmm_access_node_show(void *priv, char *buf, u32 buf_len)
{
    struct svm_vmm_access_node *access_node = (struct svm_vmm_access_node *)priv;
    u32 data_len = 0;
    u32 i;

    if (priv == NULL) {
        return 0;
    }

    svm_info("vmm access node show:\n");

    if (buf == NULL) {
        svm_info("owner_devid 0x%llx va 0x%llx size 0x%llx\n", access_node->owner_devid, access_node->va,
                 access_node->size);
        for (i = 0; i < SVM_MAX_DEV_NUM; i++) {
            if (access_node->access_type[i] != MEM_ACCESS_TYPE_NONE) {
                svm_info("    devid %u access type %u\n", i, access_node->access_type[i]);
            }
        }
    } else {
        int len;

        if (buf_len == 0) {
            return 0;
        }

        len = snprintf_s(buf, buf_len, buf_len - 1, "owner_devid 0x%llx va 0x%llx size 0x%llx\n",
                         access_node->owner_devid, access_node->va, access_node->size);
        if (len < 0) {
            return 0;
        }
        data_len = (u32)len;

        for (i = 0; i < SVM_MAX_DEV_NUM; i++) {
            if (access_node->access_type[i] != MEM_ACCESS_TYPE_NONE) {
                if (data_len >= buf_len) {
                    break;
                }
                len = snprintf_s(buf + data_len, buf_len - data_len, buf_len - data_len - 1,
                                 "    devid %u access type %u\n", i, access_node->access_type[i]);
                if (len < 0) {
                    break;
                }

                data_len += (u32)len;
            }
        }
    }

    return data_len;
}

static struct svm_svmm_seg_priv_ops vmm_access_node_priv_ops = {
    .release = vmm_access_node_release,
    .show = vmm_access_node_show,
};

static struct svm_vmm_access_node *vmm_create_access_node(void *seg_handle, u32 owner_devid, u64 va, u64 size,
                                                          struct svm_global_va *src_info,
                                                          drv_mem_access_type owner_access_type)
{
    struct svm_vmm_access_node *access_node = NULL;

    access_node = vmm_get_access_node(seg_handle);
    /* if access_node is null, but svm_svmm_get_seg_priv(seg_handle) is not null, the priv is occpy by prefetch */
    if ((access_node == NULL) && (svm_svmm_get_seg_priv(seg_handle) == NULL)) {
        access_node = vmm_access_node_create(owner_devid, va, size, src_info, seg_handle, owner_access_type);
        if (access_node != NULL) {
            int ret = svm_svmm_set_seg_priv(seg_handle, (void *)access_node, &vmm_access_node_priv_ops);
            if (ret != 0) {
                svm_err("Set seg priv failed. (start=0x%llx)\n", va);
                vmm_access_node_destroy(access_node);
                access_node = NULL;
            }
        }
    }

    return access_node;
}

static int _vmm_access_node_handle(void *va_handle, u64 va,
                                   int (*func)(struct svm_vmm_access_node *access_node, void *priv), void *priv)
{
    struct svm_vmm_access_node *access_node = NULL;
    drv_mem_access_type owner_access_type;
    void *svmm_inst = NULL;
    void *seg_handle = NULL;
    u32 seg_devid;
    u64 svm_flag, seg_va;
    struct svm_global_va src_info;
    int ret;

    svmm_inst = vmm_get_svmm(va_handle);
    if (svmm_inst == NULL) {
        svm_err("Vmm alloc not finish. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    seg_handle = svm_svmm_seg_handle_get(svmm_inst, va);
    if (seg_handle == NULL) { /* maybe va has been unmap after get prop or not map */
        svm_err("Get seg handle failed. (va=%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    seg_va = va;
    ret = svm_svmm_get_seg_by_va(svmm_inst, &seg_devid, &seg_va, &svm_flag, &src_info);
    if (ret != 0) {
        /* should not in, because we has been get seg handle */
        svm_err("Get seg info failed. (va=%llx)\n", va);
        goto out_put;
    }

    access_node = vmm_get_access_node(seg_handle);
    if (access_node == NULL) { /* vmm map addr first set/get access */
        owner_access_type = svm_flag_is_vmm_map_owner_no_access(svm_flag) ? MEM_ACCESS_TYPE_NONE :
                                                                            MEM_ACCESS_TYPE_READWRITE;
        svm_svmm_inst_occupy_pipeline(svmm_inst);
        access_node = vmm_create_access_node(seg_handle, seg_devid, seg_va, src_info.size, &src_info,
                                             owner_access_type);
        svm_svmm_inst_release_pipeline(svmm_inst);
        if (access_node == NULL) {
            ret = DRV_ERROR_OUT_OF_MEMORY;
            goto out_put;
        }
    }

    ret = func(access_node, priv);

out_put:
    svm_svmm_seg_handle_put(seg_handle);
    return ret;
}

static int vmm_access_node_handle(void *ptr, int (*func)(struct svm_vmm_access_node *access_node, void *priv),
                                  void *priv)
{
    u64 va = (u64)(uintptr_t)ptr;
    void *va_handle = NULL;
    struct svm_prop prop;
    int ret;

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (va=0x%llx)\n", va);
        return ret;
    }

    if ((!svm_flag_cap_is_support_vmm_unmap(prop.flag)) && (!svm_flag_cap_is_support_vmm_ipc_unmap(prop.flag))) {
        svm_err("Addr cap does not support set access. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    va_handle = svm_handle_get(va);
    if (va_handle == NULL) { /* maybe va has been free after get prop */
        svm_err("Get va handle failed. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = _vmm_access_node_handle(va_handle, va, func, priv);
    svm_handle_put(va_handle);

    return ret;
}

static int vmm_set_access_check(struct svm_vmm_access_node *access_node, u64 va, u64 size,
                                struct drv_mem_access_desc *desc, u32 count)
{
    u32 i;

    if ((va != access_node->va) || (size != access_node->size)) {
        svm_err("va and size not same with mmap. (va=0x%llx; size=0x%llx; mmap start=0x%llx; size=0x%llx)\n", va, size,
                access_node->va, access_node->size);
        return DRV_ERROR_PARA_ERROR;
    }

    for (i = 0; i < count; i++) {
        u32 dst_devid = vmm_access_location_to_devid(&desc[i].location);
        int ret;

        if ((dst_devid == access_node->owner_devid) || (desc[i].type == MEM_ACCESS_TYPE_NONE)) {
            /* Owner access and unset do not require a new source-to-destination route. */
            continue;
        }

        ret = vmm_dir_check(dst_devid, &access_node->src_info);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    return 0;
}

static int _vmm_set_access(struct svm_vmm_access_node *access_node, u64 va, u64 size, struct drv_mem_access_desc *desc,
                           u32 count)
{
    u32 i;

    for (i = 0; i < count; i++) {
        u32 dst_devid = vmm_access_location_to_devid(&desc[i].location);
        int ret;

        if (access_node->access_type[dst_devid] == desc[i].type) {
            continue;
        }

        if ((access_node->access_type[dst_devid] != MEM_ACCESS_TYPE_NONE) && (desc[i].type != MEM_ACCESS_TYPE_NONE)) {
            svm_warn("Repeat set with different type. (devid=%u; current_type=%u; new_type=%u)\n", dst_devid,
                     access_node->access_type[dst_devid], desc[i].type);
            return DRV_ERROR_BUSY;
        }

        if (desc[i].type == MEM_ACCESS_TYPE_NONE) {
            ret = vmm_access_single_do_unmap(access_node, dst_devid);
        } else {
            ret = vmm_access_single_do_mmap(access_node, dst_devid, desc[i].type);
        }
        if (ret != 0) { /* not need rollback */
            svm_err("Set access failed. (ret=%d; devid=%u; va=%llx; size=%llx; type=%u)\n", ret, dst_devid, va, size,
                    desc[i].type);
            return ret;
        }
    }

    return 0;
}

static int vmm_set_access(struct svm_vmm_access_node *access_node, void *priv)
{
    struct svm_vmm_set_access_para *para = priv;
    struct drv_mem_access_desc *desc = para->desc;
    u64 va = para->va;
    u64 size = para->size;
    u32 count = para->count;
    int ret;

    pthread_rwlock_wrlock(&access_node->rwlock);

    ret = vmm_set_access_check(access_node, va, size, desc, count);
    if (ret == 0) {
        ret = _vmm_set_access(access_node, va, size, desc, count);
    }

    pthread_rwlock_unlock(&access_node->rwlock);

    return ret;
}

static int vmm_get_access(struct svm_vmm_access_node *access_node, void *priv)
{
    struct svm_vmm_get_access_para *para = priv;
    struct drv_mem_location *location = para->location;
    u32 dst_devid = vmm_access_location_to_devid(location);

    (void)pthread_rwlock_rdlock(&access_node->rwlock);

    para->type = access_node->access_type[dst_devid];

    pthread_rwlock_unlock(&access_node->rwlock);

    return 0;
}

drvError_t halMemSetAccess(void *ptr, size_t size, struct drv_mem_access_desc *desc, size_t count)
{
    struct svm_vmm_set_access_para para;
    drvError_t ret;

    DRV_CURRENT_API_SCOPE();

    ret = vmm_access_ptr_check(ptr);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = vmm_access_desc_check(desc, count);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    para.va = (u64)(uintptr_t)ptr;
    para.size = (u64)size;
    para.desc = desc;
    para.count = (u32)count;
    svm_debug("SetAccess. (ptr=0x%llx; size=%llu; count=%llu)\n", (u64)(uintptr_t)ptr, size, count);

    svm_use_pipeline();
    ret = vmm_access_node_handle(ptr, vmm_set_access, &para);
    svm_unuse_pipeline();
    return ret;
}

drvError_t halMemGetAccess(void *ptr, struct drv_mem_location *location, uint64_t *flags)
{
    struct svm_vmm_get_access_para para;
    drvError_t ret;

    if ((ptr == NULL) || (flags == NULL) || (location == NULL)) {
        svm_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = vmm_access_location_check(location);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    para.location = location;
    svm_use_pipeline();
    ret = vmm_access_node_handle(ptr, vmm_get_access, &para);
    svm_unuse_pipeline();
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get access failed. (side=%u; id=%u)\n", location->side, location->id);
        return ret;
    }

    *flags = (uint64_t)para.type;
    svm_debug("GetAccess. (ptr=0x%llx; side=%u; id=%u; flags=0x%llx)\n", (u64)(uintptr_t)ptr, location->side,
              location->id, *flags);
    return 0;
}

/* trans struct MemShareHandle to u64 shareable_handle(devid + share id), to set/get attribute in local */
drvError_t halMemTransShareableHandle(drv_mem_handle_type handle_type, struct MemShareHandle *share_handle,
                                      uint32_t *server_id, uint64_t *shareable_handle)
{
    struct svm_share_handle *_share_handle = (struct svm_share_handle *)(void *)share_handle;

    if ((share_handle == NULL) || (server_id == NULL) || (shareable_handle == NULL)) {
        svm_err("handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((_share_handle->handle_type != handle_type) || (handle_type >= MEM_HANDLE_TYPE_MAX)) {
        svm_err("Invalid para. (handle_type=%u; share_handle.handle_type=%u)\n", handle_type,
                _share_handle->handle_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    *shareable_handle = _share_handle->key;
    *server_id = _share_handle->src_va.server_id;
    return DRV_ERROR_NONE;
}

drvError_t halMemSetPidToShareableHandle(uint64_t shareable_handle, int pid[], uint32_t pid_num)
{
    int ret;

    ret = vmm_share_mem_set_pid_para_check(shareable_handle, pid, pid_num);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    svm_debug("Setpid. (shareable_handle=%llu; pid_num=%u)\n", shareable_handle, pid_num);
    return svm_casm_add_local_task(shareable_handle, pid, pid_num);
}

drvError_t halMemGetAddressReserveRange(void **ptr, size_t *size, drv_mem_addr_reserve_type type, uint64_t flag)
{
    u64 get_va, get_size;

    if ((ptr == NULL) || (size == NULL)) {
        svm_err("Ptr or size is null. (ptr_is_null=%d; size_is_null=%d)\n", (ptr == NULL), (size == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    /* reserved para, verify by zero for subsequent compatibility */
    if (flag != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    switch (type) {
        case MEM_ADDR_RESERVE_TYPE_DCACHE:
            svm_get_dcache_va_range(&get_va, &get_size);
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    *ptr = (void *)(uintptr_t)get_va;
    *size = (size_t)get_size;
    svm_debug("GetAddressReserveRange. (va=0x%llx; size=%llu; type=%u; flag=0x%llx)\n", get_va, get_size, (u32)type,
              flag);
    return DRV_ERROR_NONE;
}

static drv_mem_handle_t *vmm_get_seg_pa_handle(struct svm_global_va *src_info)
{
    return (drv_mem_handle_t *)(uintptr_t)src_info->va;
}

static bool vmm_seg_is_cross_app_map(struct svm_global_va *src_info)
{
    drv_mem_handle_t *handle = vmm_get_seg_pa_handle(src_info);
    return (handle != NULL) && (handle->type == SVM_VMM_HANDLE_IMPORT_TYPE);
}

static bool vmm_seg_is_single_app_map(struct svm_global_va *src_info)
{
    return !vmm_seg_is_cross_app_map(src_info);
}

static int vmm_seg_access_criu_restore(struct svm_vmm_access_node *access_node)
{
    struct svm_prop src_prop;
    u32 devid;
    int ret;

    ret = svm_get_prop(access_node->src_info.va, &src_prop);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get seg prop failed. (ret=%d; va=0x%llx)\n", ret, access_node->src_info.va);
        return ret;
    }
    access_node->src_info.tgid = src_prop.tgid;

    for (devid = 0; devid < SVM_MAX_DEV_NUM; devid++) {
        if ((devid != access_node->owner_devid) && (access_node->access_type[devid] != MEM_ACCESS_TYPE_NONE)) {
            return vmm_access_non_owner_do_mmap(access_node, devid, access_node->access_type[devid]);
        }
    }
    return DRV_ERROR_NONE;
}

static void vmm_seg_cross_app_map_criu_reset(void *svmm_inst, void *seg_handle, u32 devid, u64 start,
                                             struct svm_global_va *src_info)
{
    drv_mem_handle_t *handle = vmm_get_seg_pa_handle(src_info);
    u64 svm_flag = svm_svmm_get_seg_svm_flag(seg_handle);

    svm_svmm_del_seg_handle(svmm_inst, seg_handle);
    if (!svm_flag_is_vmm_map_owner_no_access(svm_flag)) {
        (void)svm_casm_mem_unmap(devid, start, src_info->size);
    }
    vmm_import_handle_no_ref_destroy(handle);
}

static int vmm_seg_single_app_map_criu_restore(void *seg_handle, u32 devid, u64 start, struct svm_global_va *src_info)
{
    struct svm_global_va real_src_info = *src_info;
    struct svm_prop src_prop;
    u64 svm_flag = svm_svmm_get_seg_svm_flag(seg_handle);
    int ret;

    vmm_restore_real_src_va(&real_src_info);

    ret = svm_get_prop(real_src_info.va, &src_prop);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get seg prop failed. (ret=%d; va=0x%llx)\n", ret, real_src_info.va);
        return ret;
    }
    svm_svmm_mod_seg_src_tgid(seg_handle, src_prop.tgid);
    real_src_info.tgid = src_prop.tgid;

    if (!svm_flag_is_vmm_map_owner_no_access(svm_flag)) {
        ret = vmm_do_single_app_mmap(devid, start, svm_flag, &real_src_info);
        if (ret != 0) {
            svm_err("Do single app mmap failed. (ret=%d; devid=%u; start=0x%llx; size=%llu)\n", ret, devid, start,
                    real_src_info.size);
            return ret;
        }
    }
    return 0;
}

struct vmm_handle_criu_reset_priv {
    void *svmm_inst;
    u32 devid;
};

static int vmm_seg_criu_reset(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    struct vmm_handle_criu_reset_priv *seg_priv = (struct vmm_handle_criu_reset_priv *)priv;
    u32 target_devid = svm_svmm_get_seg_devid(seg_handle);
    u32 devid = seg_priv->devid;

    if ((devid == target_devid) && vmm_seg_is_cross_app_map(src_info)) {
        vmm_seg_cross_app_map_criu_reset(seg_priv->svmm_inst, seg_handle, devid, start, src_info);
    }

    return DRV_ERROR_NONE;
}

static int vmm_seg_criu_restore(void *seg_handle, u64 start, struct svm_global_va *src_info, void *priv)
{
    struct svm_vmm_access_node *access_node = vmm_get_access_node(seg_handle);
    u32 target_devid = svm_svmm_get_seg_devid(seg_handle);
    u32 devid = *(u32 *)priv;
    int ret;

    if ((devid == target_devid) && vmm_seg_is_single_app_map(src_info)) {
        ret = vmm_seg_single_app_map_criu_restore(seg_handle, devid, start, src_info);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }

        if (access_node != NULL) {
            ret = vmm_seg_access_criu_restore(access_node);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
        }
    }

    return DRV_ERROR_NONE;
}

static int vmm_handle_criu_reset(void *va_handle, u64 start, struct svm_prop *prop, void *priv)
{
    void *svmm_inst = vmm_get_svmm(va_handle);
    u32 devid = *(u32 *)priv;
    struct vmm_handle_criu_reset_priv seg_priv = {.svmm_inst = svmm_inst, .devid = devid};

    SVM_UNUSED(start);
    SVM_UNUSED(prop);

    if (svmm_inst == NULL) {
        return DRV_ERROR_NONE;
    }

    return svm_svmm_for_each_seg_handle(svmm_inst, vmm_seg_criu_reset, &seg_priv);
}

static int vmm_handle_criu_restore(void *va_handle, u64 start, struct svm_prop *prop, void *priv)
{
    void *svmm_inst = vmm_get_svmm(va_handle);

    SVM_UNUSED(start);
    SVM_UNUSED(prop);

    if (svmm_inst == NULL) {
        return DRV_ERROR_NONE;
    }

    return svm_svmm_for_each_seg_handle(svmm_inst, vmm_seg_criu_restore, priv);
}

static int svm_vmm_criu_reset(u32 devid, void *data)
{
    SVM_UNUSED(data);
    return svm_for_each_valid_handle(vmm_handle_criu_reset, &devid);
}

static int svm_vmm_criu_restore(u32 devid, void *data)
{
    SVM_UNUSED(data);
    return svm_for_each_valid_handle(vmm_handle_criu_restore, &devid);
}

static const struct svm_criu_ops g_vmm_criu_ops = {
    .name = "vmm",
    .reset = svm_vmm_criu_reset,
    .restore = svm_vmm_criu_restore,
};

void __attribute__((constructor(SVM_INIT_PRI_FINAL - 1))) svm_vmm_init(void)
{
    int ret = svm_criu_register_ops(&g_vmm_criu_ops);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Register CRIU ops failed. (ret=%d)\n", ret);
    }
}
