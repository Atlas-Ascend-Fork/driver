/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <pthread.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <sys/syscall.h>

#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "dpa_user_interface.h"
#include "dms_user_interface.h"

#include "svm_ioctl.h"
#include "devmm_virt_comm.h"
#include "devmm_virt_interface.h"
#include "devmm_dynamic_addr.h"
#include "devmm_svm_init.h"
#include "devmm_virt_base_heap.h"
#include "devmm_virt_com_heap.h"
#include "devmm_virt_dvpp_heap.h"
#include "devmm_virt_read_only_heap.h"
#include "devmm_virt_host_pin_heap.h"
#include "devmm_cache_coherence.h"
#include "devmm_map_dev_reserve.h"
#include "svm_mem_statistics.h"
#include "svm_mem_register_record.h"
#include "svm_user_interface.h"
#include "devmm_host_mem_pool.h"
#include "svm_user_msg.h"
#include "devmm_record.h"
#include "devmm_svm.h"
#ifndef UVM_OPEN
#include "devmm_virt_uvm_heap.h"
#endif
#include "devmm_virt_soma_heap.h"

#define MEM_VIRT_MASK ((1UL << MEM_VIRT_WIDTH) - 1)

struct devmm_snapshot_mem_node {
    uint64_t dev_va;
    uint64_t size;
    void *host_copy;
    struct devmm_snapshot_mem_node *next;
    bool required;
};

static struct devmm_snapshot_mem_node *g_snapshot_mem_head;
static uint8_t *g_snapshot_host_copy;

#define DEVMM_MEM_ALLOC_RECOMMENDED_GRANULARITY 0x200000
#define DEVMM_MEM_ALLOC_MINIMUN_GRANULARITY 0x200000

static THREAD uint32_t svm_devid;
static THREAD bool g_get_user_malloc_attr = false;
static THREAD bool g_register_query_and_get_attr = false;
static THREAD bool g_pro_snapshot_state = false;
#ifndef UVM_OPEN
static THREAD int g_uvm_heap_initialized = 0;
#endif
static THREAD int g_soma_heap_initialized = 0;

struct svm_map_info {
    struct rbtree_node rb_node;
    uint64_t va;
    uint64_t size;
    uint32_t side;
    uint32_t devid;
    bool is_share;
};

static THREAD struct rbtree_root map_info_root = RB_ROOT;
static THREAD pthread_rwlock_t map_info_mutex = PTHREAD_RWLOCK_INITIALIZER;

static void svm_clear_map_info_of_va_range(uint64_t va, uint64_t size)
{
    struct rbtree_node *rb_node = NULL;
    struct rbtree_node *n = NULL;
    struct svm_map_info *map_info = NULL;

    (void)pthread_rwlock_wrlock(&map_info_mutex);
    rbtree_node_for_each_prev_safe(rb_node, n, &map_info_root)
    {
        map_info = rb_entry(rb_node, struct svm_map_info, rb_node);
        if ((map_info->va >= va) && ((map_info->va + map_info->size) <= (va + size))) {
            _rbtree_erase(&map_info_root, &map_info->rb_node);
            free(map_info);
        }
    }
    (void)pthread_rwlock_unlock(&map_info_mutex);
}

static void __attribute__((constructor)) svm_map_info_init(void)
{
    svm_da_free_set_ops(&svm_clear_map_info_of_va_range);
}

static void svm_clear_map_info_by_dev(uint32_t devid)
{
    struct rbtree_node *rb_node = NULL;
    struct rbtree_node *n = NULL;
    struct svm_map_info *map_info = NULL;

    (void)pthread_rwlock_wrlock(&map_info_mutex);
    rbtree_node_for_each_prev_safe(rb_node, n, &map_info_root)
    {
        map_info = rb_entry(rb_node, struct svm_map_info, rb_node);
        if (map_info->devid != devid) {
            continue;
        }
        _rbtree_erase(&map_info_root, &map_info->rb_node);
        free(map_info);
    }
    (void)pthread_rwlock_unlock(&map_info_mutex);
}

void __attribute__((destructor)) svm_map_info_uninit(void)
{
    uint32_t host_devid;

    (void)halGetHostID(&host_devid);
    svm_clear_map_info_by_dev(host_devid);
}

static void svm_map_info_range_handle(struct rbtree_node *rb_node, struct rb_range_handle *range_handle)
{
    struct svm_map_info *map_info = rb_entry(rb_node, struct svm_map_info, rb_node);

    range_handle->start = map_info->va;
    range_handle->end = map_info->va + map_info->size - 1;
}

static struct svm_map_info *svm_find_map_info_nolock(uint64_t va)
{
    struct rb_range_handle range = {.start = va, .end = va};
    struct rbtree_node *rb_node = NULL;

    rb_node = rbtree_search_by_range(&map_info_root, &range, svm_map_info_range_handle);
    if (rb_node == NULL) {
        return NULL;
    }

    return rb_entry(rb_node, struct svm_map_info, rb_node);
}

static bool svm_map_range_is_valid(uint64_t va, uint64_t size)
{
    return ((size != 0) && (va <= va + size - 1));
}

static int svm_save_map_info(uint64_t va, uint64_t size, uint32_t side, uint32_t devid, bool is_share)
{
    struct svm_map_info *map_info = NULL;
    int ret;

    if (svm_map_range_is_valid(va, size) == false) {
        DEVMM_DRV_ERR("Invalid map info range. (va=0x%llx; size=0x%llx)\n", va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    map_info = malloc(sizeof(*map_info));
    if (map_info == NULL) {
        DEVMM_DRV_ERR("Malloc map info failed. (va=0x%llx; size=0x%llx)\n", va, size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    RB_CLEAR_NODE(&map_info->rb_node);
    map_info->va = va;
    map_info->size = size;
    map_info->side = side;
    map_info->devid = devid;
    map_info->is_share = is_share;

    (void)pthread_rwlock_wrlock(&map_info_mutex);
    ret = rbtree_insert_by_range(&map_info_root, &map_info->rb_node, svm_map_info_range_handle);
    (void)pthread_rwlock_unlock(&map_info_mutex);
    if (ret != 0) {
        free(map_info);
        DEVMM_DRV_ERR("Insert map info failed. (va=0x%llx; size=0x%llx; ret=%d)\n", va, size, ret);
        return DRV_ERROR_REPEATED_USERD;
    }

    return DRV_ERROR_NONE;
}

static int svm_query_map_info(uint64_t va, struct svm_map_info *info)
{
    struct svm_map_info *map_info = NULL;

    (void)pthread_rwlock_rdlock(&map_info_mutex);
    map_info = svm_find_map_info_nolock(va);
    if (map_info != NULL) {
        (void)memcpy_s(info, sizeof(struct svm_map_info), map_info, sizeof(struct svm_map_info));
    }
    (void)pthread_rwlock_unlock(&map_info_mutex);

    if (map_info == NULL) {
        return DRV_ERROR_BAD_ADDRESS;
    }

    return DRV_ERROR_NONE;
}

static bool svm_is_vmm_mix_map(uint64_t va, uint64_t size)
{
    struct svm_map_info info;
    uint64_t cur_va;
    uint32_t pre_devid;
    bool pre_is_share;
    int ret;

    for (cur_va = va; cur_va < (va + size);) {
        ret = svm_query_map_info(cur_va, &info);
        if (ret != DRV_ERROR_NONE) {
            return false;
        }

        if ((cur_va != va) && ((info.devid != pre_devid) || (info.is_share != pre_is_share))) {
            return true;
        }

        pre_devid = info.devid;
        pre_is_share = info.is_share;
        cur_va = info.va + info.size;
    }
    return false;
}

static void svm_del_map_info(uint64_t va)
{
    struct svm_map_info *map_info = NULL;

    (void)pthread_rwlock_wrlock(&map_info_mutex);
    map_info = svm_find_map_info_nolock(va);
    if ((map_info != NULL) && (map_info->va == va)) {
        _rbtree_erase(&map_info_root, &map_info->rb_node);
    } else {
        map_info = NULL;
    }
    (void)pthread_rwlock_unlock(&map_info_mutex);

    if (map_info != NULL) {
        free(map_info);
    }
}

static bool devmm_vaddr_and_size_is_in_soma_range(DVdeviceptr va, uint64_t size)
{
    const uint64_t soma_start = DEVMM_SOMA_MEM_START;
    const uint64_t soma_end = DEVMM_SOMA_MEM_START + DEVMM_SOMA_MEM_SIZE;
    uint64_t addr = (uint64_t)va;

    if ((addr == 0) || (size == 0) || (addr < soma_start) || (addr >= soma_end)) {
        return false;
    }

    return size <= (soma_end - addr);
}

static bool devmm_vaddr_and_size_crosses_soma_boundary(DVdeviceptr va, uint64_t size)
{
    const uint64_t soma_start = DEVMM_SOMA_MEM_START;
    const uint64_t soma_end = DEVMM_SOMA_MEM_START + DEVMM_SOMA_MEM_SIZE;
    uint64_t addr = (uint64_t)va;

    if ((size == 0) || (addr >= soma_end)) {
        return false;
    }

    if (addr < soma_start) {
        return size > (soma_start - addr);
    }

    return size > (soma_end - addr);
}

struct devmm_advise_record_node {
    DVdeviceptr ptr;
    size_t count;
    struct devmm_virt_list_head node;
};

#define ADVISE_RECORD_MAX_DEV_NUM DEVMM_MAX_PHY_DEVICE_NUM
STATIC struct devmm_virt_list_head advise_record_list[ADVISE_RECORD_MAX_DEV_NUM];
STATIC pthread_mutex_t advise_record_lock[ADVISE_RECORD_MAX_DEV_NUM];
STATIC pthread_rwlock_t svm_pipeline_rwlock = PTHREAD_RWLOCK_INITIALIZER;

STATIC void devmm_del_mem_advise_record(bool is_del_all, DVdeviceptr free_ptr, size_t free_cnt, DVdevice device);
static drvError_t devmm_remote_unmap(void *src_ptr, UINT32 devid, UINT32 flag);
static int devmm_mem_get_access(void *ptr, size_t *size, struct drv_mem_location *location, drv_mem_access_type *type);

static inline void devmm_atomic_dec(uint32_t *val)
{
    __sync_fetch_and_sub(val, 1);
}

static inline void devmm_atomic_inc(uint32_t *val)
{
    __sync_fetch_and_add(val, 1);
}

/* If multiple threads concurrently obtain read locks, write locks may starve.
 * In this case, a write request counter needs to be added to ensure that write locks are prioritized and prevent write
 * lock starvation.
 */
static uint32_t svm_occupy_req = 0;
static void devmm_use_pipeline(void)
{
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = 100; // sleep 100 ns
    while (svm_occupy_req != 0) {
        nanosleep(&ts, NULL);
    }

    (void)pthread_rwlock_rdlock(&svm_pipeline_rwlock);
}

static void devmm_unuse_pipeline(void)
{
    (void)pthread_rwlock_unlock(&svm_pipeline_rwlock);
}

static void devmm_occupy_pipeline(void)
{
    devmm_atomic_inc(&svm_occupy_req);
    (void)pthread_rwlock_wrlock(&svm_pipeline_rwlock);
    devmm_atomic_dec(&svm_occupy_req);
}

static void devmm_release_pipeline(void)
{
    (void)pthread_rwlock_unlock(&svm_pipeline_rwlock);
}

bool devmm_is_snapshot_state(void)
{
    return g_pro_snapshot_state;
}

bool devmm_is_split_mode(void)
{
    static unsigned int split_mode;
    static bool query_flag = false;
    drvError_t ret;

    if (query_flag == false) {
        ret = halGetDeviceSplitMode(0, &split_mode);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_WARN("Get split mode not succ. (ret=%d)\n", ret);
            return false;
        }
        DEVMM_RUN_INFO("Get split mode succ. (split_mode=%u)\n", split_mode);
        query_flag = true;
    }

    return (split_mode != VMNG_NORMAL_NONE_SPLIT_MODE);
}

static void devmm_set_device_info(struct devmm_virt_heap_mgmt *mgmt, uint32_t devid,
                                  struct devmm_setup_dev_para *dev_para)
{
    bool is_alloced_by_malloc = (devmm_get_host_mem_alloc_mode() == SVM_HOST_MEM_ALLOCED_BY_MALLOC);

    mgmt->dvpp_mem_size[devid] = dev_para->dvpp_mem_size;
    mgmt->support_bar_mem[devid] = is_alloced_by_malloc ? false : dev_para->support_bar_mem;
    mgmt->support_dev_read_only[devid] = dev_para->support_dev_read_only;
    mgmt->support_dev_mem_map_host[devid] = is_alloced_by_malloc ? false : dev_para->support_dev_mem_map_host;
    mgmt->support_bar_huge_mem[devid] = is_alloced_by_malloc ? false : dev_para->support_bar_huge_mem;
    mgmt->host_support_pin_user_pages_interface = dev_para->host_support_pin_user_pages_interface;
    mgmt->support_host_rw_dev_ro = dev_para->support_host_rw_dev_ro;
    mgmt->double_pgtable_offset[devid] = dev_para->double_pgtable_offset;
    mgmt->support_remote_mmap[devid] = dev_para->support_remote_mmap;
    mgmt->support_host_pin_pre_register = is_alloced_by_malloc ? false : dev_para->support_host_pin_pre_register;
    mgmt->support_host_mem_pool = is_alloced_by_malloc ? false : dev_para->support_host_mem_pool;
    mgmt->support_agent_giant_page[devid] = dev_para->support_agent_giant_page;
    mgmt->support_shmem_map_exbus[devid] = dev_para->support_shmem_map_exbus;
    mgmt->support_mem_host_uva[devid] = dev_para->support_mem_host_uva;
    DEVMM_RUN_INFO("Device info. (devid=%u; dvpp_size=%llu; support_bar_mem=%u; support_dev_read_only=%u; "
                   "support_dev_mem_map_host=%u; support_bar_huge_mem=%u; allocated_by_malloc=%d; host_rw_dev_ro=%u)\n",
                   devid, dev_para->dvpp_mem_size, dev_para->support_bar_mem, dev_para->support_dev_read_only,
                   dev_para->support_dev_mem_map_host, dev_para->support_bar_huge_mem, is_alloced_by_malloc,
                   dev_para->support_host_rw_dev_ro);
    DEVMM_RUN_INFO("Device info. (double_pgtable_offset=%llu; support_host_pin_pre_register=%u; "
                   "support_host_mem_pool=%u; is_support_agent_giant_page=%u; is_support_host_giant_page=%u; "
                   "support_remote_mmap=%u; support_shmem_map_exbus=%u; support_mem_host_uva=%u)\n",
                   dev_para->double_pgtable_offset, dev_para->support_host_pin_pre_register,
                   dev_para->support_host_mem_pool, dev_para->support_agent_giant_page, mgmt->support_host_giant_page,
                   dev_para->support_remote_mmap, dev_para->support_shmem_map_exbus, dev_para->support_mem_host_uva);
}

static void devmm_setup_device_init_heap(struct devmm_virt_heap_mgmt *mgmt, uint32_t devid,
                                         struct devmm_ioctl_arg *parg)
{
    if (devid < DEVMM_MAX_PHY_DEVICE_NUM) {
        devmm_set_device_info(mgmt, devid, &parg->data.setup_dev_para);
        (void)devmm_init_dvpp_heap_by_devid(devid);
        if (mgmt->support_dev_read_only[devid] != 0) {
            (void)devmm_init_read_only_heap_by_devid(devid, SUB_READ_ONLY_TYPE);
        }
        (void)devmm_init_read_only_heap_by_devid(devid, SUB_DEV_READ_ONLY_TYPE);
        devmm_init_dev_reserve_addr(devid);
    }

    devmm_host_pin_dma_register_prepare(mgmt, devid);

    mgmt->is_dev_inited[devid] = true; /* host_mem_pool need is_dev_inited set */
    if (mgmt->support_host_mem_pool && !devmm_is_snapshot_state()) {
        devmm_host_mem_pool_init(devid);
    }

    (void)devmm_init_host_pin_heap_by_devid(devid, SUB_HOST_TYPE);
    svm_devid = devid;
}

/* In training scenarios, one process can be bound to only one device. */
static DVresult devmm_setup_device(uint32_t devid)
{
    struct devmm_virt_heap_mgmt *mgmt = NULL;
    struct devmm_ioctl_arg arg = {0};
    int ret;

    DEVMM_RUN_INFO("DrvMemDeviceOpen. (devid=%u)\n", devid);
    if (devid >= SVM_MAX_AGENT_NUM) {
        DEVMM_DRV_ERR("Devid overflow. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    mgmt = devmm_virt_init_get_heap_mgmt();
    if (mgmt == NULL) {
        DEVMM_DRV_ERR("Init heap mgmt failed. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (mgmt->can_init_dev[devid] == false) {
#ifndef EMU_ST
        DEVMM_DRV_ERR("Not allowed to initialize device. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
#endif
    }

    ret = svm_da_add_dev(devid);
    if (ret != 0) {
        DEVMM_DRV_ERR("Da mng add dev failed. (devid=%u)\n", devid);
        return ret;
    }

    svm_init_mem_stats_mng(devid);
    arg.head.devid = devid;
    arg.data.setup_dev_para.mem_stats_va = svm_get_mem_stats_va(devid);
    ret = (int)devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_SETUP_DEVICE, &arg);
    share_log_read_run_info(HAL_MODULE_TYPE_DEVMM);
    if (ret != 0) {
        svm_da_del_dev(devid);
        DEVMM_DRV_ERR("Setup device error. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    devmm_setup_device_init_heap(mgmt, devid, &arg);
    return 0;
}

static DVresult devmm_free_managed_nomal(DVdeviceptr p)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;
    struct devmm_virt_com_heap *heap = NULL;
    uint32_t heap_list_type;
    uint32_t heap_sub_type;
    uint64_t free_len;
    DVresult ret;

    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        DEVMM_DRV_ERR("Please call device_open and alloc_mem api first. (alloc_mem=0x%llx)\n", p);
        return DRV_ERROR_INVALID_HANDLE;
    }

    heap = devmm_va_to_heap(p);
    if ((heap == NULL) || heap->heap_type == DEVMM_HEAP_IDLE) {
        DEVMM_DRV_ERR("Address is not allocated. please check ptr. (offset=%llx, va = %lx\n", ADDR_TO_OFFSET(p), p);
        return DRV_ERROR_INVALID_VALUE;
    }

    heap_sub_type = heap->heap_sub_type;
    if (heap_sub_type == SUB_RESERVE_TYPE) {
        DEVMM_DRV_ERR("Addr should be freed by halMemAddressFree api. (va=0x%llx)\n", p);
        return DRV_ERROR_INVALID_VALUE;
    }

    heap_list_type = heap->heap_list_type;
    if (heap_list_type == HOST_AGENT_LIST) {
        ret = halHostUnregister((void *)p, SVM_HOST_AGENT_ID);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Remote_map failed. (free_ptr=0x%llx)\n", p);
            return ret;
        }
    }

    if (heap == &p_heap_mgmt->heap_queue.host_base_heap || devmm_virt_heap_is_primary(heap)) {
        free_len = heap->mapped_size;
        ret = devmm_free_to_base_heap(p_heap_mgmt, heap, p);
    } else {
        ret = devmm_free_to_normal_heap(p_heap_mgmt, heap, p, &free_len);
    }

    if ((ret == DRV_ERROR_NONE) && (heap_sub_type != SUB_SVM_TYPE) && (heap_sub_type != SUB_HOST_TYPE)) {
        uint32_t devid = devmm_heap_device_by_list_type(heap_list_type);
        devmm_del_mem_advise_record(false, p, free_len, devid);
    }

    return ret;
}

/* can not declared static, it patch use */
DVresult devmm_free_managed(DVdeviceptr p)
{
    if (devmm_va_is_soma(p)) {
        return devmm_soma_heap_free_pages(p);
    }
    if (!devmm_va_is_svm(p) && (devmm_get_host_mem_alloc_mode() == SVM_HOST_MEM_ALLOCED_BY_MALLOC)) {
        free((void *)p);
        return 0;
    }

    return devmm_free_managed_nomal(p);
}

static inline bool devmm_should_alloc_from_base(struct devmm_virt_heap_type *heap_type, size_t bytesize,
                                                DVmem_advise advise)
{
    bool is_reserve_uva_check;
    bool size_check;

#ifndef UVM_OPEN
    size_check = ((heap_type->heap_sub_type != SUB_DVPP_TYPE) && (heap_type->heap_sub_type != SUB_READ_ONLY_TYPE) &&
                  (heap_type->heap_sub_type != SUB_DEV_READ_ONLY_TYPE) && (heap_type->heap_sub_type != SUB_UVM_TYPE) &&
                  (bytesize > DEVMM_LARGE_MEM_THRESHOLD_SIZE)) ||
                 ((advise & DV_ADVISE_GIANTPAGE) != 0);
#else
    size_check = ((heap_type->heap_sub_type != SUB_DVPP_TYPE) && (heap_type->heap_sub_type != SUB_READ_ONLY_TYPE) &&
                  (heap_type->heap_sub_type != SUB_DEV_READ_ONLY_TYPE) &&
                  (bytesize > DEVMM_LARGE_MEM_THRESHOLD_SIZE)) ||
                 ((advise & DV_ADVISE_GIANTPAGE) != 0);
#endif

    is_reserve_uva_check = ((heap_type->heap_sub_type != SUB_RESERVE_TYPE) && ((advise & DV_ADVISE_HOST_UVA) != 0));

    /* if alloc size larger than half heap size, alloc mem form base heap  */
    /* reserve     uva: large size goes through base heap path, small size goes through normal heap path;
     * halMemAlloc uva: always goes through base heap path */
    return (size_check || is_reserve_uva_check);
}

/* can not declared static, it patch use */
DVresult devmm_alloc_managed(DVdeviceptr *pp, size_t bytesize, struct devmm_virt_heap_type *heap_type,
                             DVmem_advise advise)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;
    DVdeviceptr temp_ptr;

    /* asan: host just use libc malloc */
    if ((heap_type->heap_sub_type == SUB_HOST_TYPE) &&
        (devmm_get_host_mem_alloc_mode() == SVM_HOST_MEM_ALLOCED_BY_MALLOC)) {
        *pp = (DVdeviceptr)malloc(bytesize);
        if (*pp == (DVdeviceptr)NULL) {
#ifndef EMU_ST
            return DRV_ERROR_OUT_OF_MEMORY;
#endif
        }
        return 0;
    }

    /* 1.if heap mgmt not inited,init it. */
    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        DEVMM_DRV_ERR("Devmm_virt_get_heap_mgmt heap mgmt is NULL.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    /* 2.the max size can be allocated is DEVMM_POOL_VM_RANGE */
    if ((pp == NULL) || (bytesize == 0)) {
        DEVMM_DRV_ERR("PP is Null or bytesize is 0 or bytesize is invalid. "
                      "(pp_is_null=%d; bytesize=%lu; max_size=%llu)\n",
                      (pp == NULL), bytesize, p_heap_mgmt->max_conti_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (bytesize > p_heap_mgmt->max_conti_size) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    if (devmm_should_alloc_from_base(heap_type, bytesize, advise)) {
        temp_ptr = devmm_alloc_from_base_heap(p_heap_mgmt, bytesize, heap_type, advise, *pp);
    } else {
        temp_ptr = devmm_alloc_from_normal_heap(p_heap_mgmt, bytesize, heap_type, advise, *pp);
    }

    if (temp_ptr < DEVMM_HOST_PIN_START) {
        return ptr_to_errcode(temp_ptr);
    }

    *pp = temp_ptr;
    return DRV_ERROR_NONE;
}

static uint32_t devmm_get_memtype_from_advise(DVmem_advise advise)
{
    if ((advise & DV_ADVISE_P2P_HBM) != 0) {
        return DEVMM_P2P_HBM_MEM;
    } else if ((advise & DV_ADVISE_HBM) != 0) {
        return DEVMM_HBM_MEM;
    } else if ((advise & DV_ADVISE_P2P_DDR) != 0) {
        return DEVMM_P2P_DDR_MEM;
    } else if ((advise & DV_ADVISE_TS_DDR) != 0) {
        return DEVMM_TS_DDR_MEM;
    } else {
        return DEVMM_DDR_MEM;
    }
}

static void devmm_fill_host_heap_type(struct devmm_virt_heap_type *heap_type)
{
    heap_type->heap_type = DEVMM_HEAP_PINNED_HOST;
    heap_type->heap_list_type = HOST_LIST;
    heap_type->heap_sub_type = SUB_HOST_TYPE;
    heap_type->heap_mem_type = DEVMM_DDR_MEM;
}

static void devmm_fill_svm_heap_type(DVmem_advise advise, struct devmm_virt_heap_type *heap_type)
{
    heap_type->heap_list_type = SVM_LIST;
    heap_type->heap_sub_type = SUB_SVM_TYPE;
    heap_type->heap_mem_type = devmm_get_memtype_from_advise(advise);
    if ((advise & DV_ADVISE_HUGEPAGE) != 0) {
        heap_type->heap_type = DEVMM_HEAP_HUGE_PAGE;
    } else {
        heap_type->heap_type = DEVMM_HEAP_CHUNK_PAGE;
    }
}

#ifndef UVM_OPEN
static pthread_once_t uvm_heap_is_initialized = PTHREAD_ONCE_INIT;
static struct devmm_virt_heap_mgmt *heap_mgmt = NULL;

void init_uvm_heap(void)
{
    if (heap_mgmt == NULL) {
        DEVMM_DRV_ERR("Global heap mgmt is NULL\n");
        return;
    }
    if (__sync_val_compare_and_swap(&g_uvm_heap_initialized, 0, 1) == 0) {
        if (devmm_virt_init_uvm_heap(heap_mgmt) != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Init uvm heap fail.\n");
            return;
        }
        // add uvm heap to heap list
        (void)pthread_rwlock_wrlock(&heap_mgmt->huge_list[UVM_LIST][SUB_UVM_TYPE][DEVMM_UVM_MEM].list_lock);
        devmm_virt_list_add(&heap_mgmt->heap_queue.uvm_heap.list,
                            &heap_mgmt->huge_list[UVM_LIST][SUB_UVM_TYPE][DEVMM_UVM_MEM].heap_list);
        heap_mgmt->huge_list[UVM_LIST][SUB_UVM_TYPE][DEVMM_UVM_MEM].heap_cnt++;
        (void)pthread_rwlock_unlock(&heap_mgmt->huge_list[UVM_LIST][SUB_UVM_TYPE][DEVMM_UVM_MEM].list_lock);
    }
}

static void devmm_fill_uvm_heap_type(struct devmm_virt_heap_type *heap_type)
{
    heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();

    if (heap_mgmt == NULL) {
        DEVMM_DRV_ERR("Global heap mgmt is NULL\n");
        return;
    }
#ifdef EMU_ST
    init_uvm_heap();
#else
    (void)pthread_once(&uvm_heap_is_initialized, init_uvm_heap);
#endif

    heap_type->heap_list_type = UVM_LIST;
    heap_type->heap_sub_type = SUB_UVM_TYPE;
    heap_type->heap_mem_type = DEVMM_UVM_MEM;
    heap_type->heap_type = DEVMM_HEAP_HUGE_PAGE;
}
#endif
void devmm_fill_soma_heap_type(struct devmm_virt_heap_type *heap_type)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;

    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();

    if (p_heap_mgmt == NULL) {
        DEVMM_DRV_ERR("Global heap mgmt is NULL\n");
        return;
    }

    /* init soma heap to manage heaps */
    if (__sync_val_compare_and_swap(&g_soma_heap_initialized, 0, 1) == 0) {
        if (devmm_virt_init_soma_heap(p_heap_mgmt) != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Init soma heap fail.\n");
            (void)__sync_val_compare_and_swap(&g_soma_heap_initialized, 1, 0);
            return;
        }
        // add soma heap to heap list
        (void)pthread_rwlock_wrlock(&p_heap_mgmt->huge_list[SOMA_LIST][SUB_SOMA_TYPE][DEVMM_SOMA_MEM].list_lock);
        devmm_virt_list_add(&p_heap_mgmt->heap_queue.soma_heap.list,
                            &p_heap_mgmt->huge_list[SOMA_LIST][SUB_SOMA_TYPE][DEVMM_SOMA_MEM].heap_list);
        p_heap_mgmt->huge_list[SOMA_LIST][SUB_SOMA_TYPE][DEVMM_SOMA_MEM].heap_cnt++;
        (void)pthread_rwlock_unlock(&p_heap_mgmt->huge_list[SOMA_LIST][SUB_SOMA_TYPE][DEVMM_SOMA_MEM].list_lock);
    }

    heap_type->heap_list_type = SOMA_LIST;
    heap_type->heap_sub_type = SUB_SOMA_TYPE;
    heap_type->heap_mem_type = DEVMM_SOMA_MEM;
    heap_type->heap_type = DEVMM_HEAP_HUGE_PAGE;
}
static void devmm_fill_dev_heap_type(uint32_t devid, DVmem_advise advise, struct devmm_virt_heap_type *heap_type)
{
    heap_type->heap_list_type = devmm_heap_list_type_by_device(devid);
    heap_type->heap_sub_type = SUB_DEVICE_TYPE;
    heap_type->heap_mem_type = devmm_get_memtype_from_advise(advise);
    if ((advise & DV_ADVISE_HUGEPAGE) != 0) {
        heap_type->heap_type = DEVMM_HEAP_HUGE_PAGE;
    } else {
        heap_type->heap_type = DEVMM_HEAP_CHUNK_PAGE;
    }
}

static void devmm_fill_reserve_heap_type(DVmem_advise advise, struct devmm_virt_heap_type *heap_type)
{
    heap_type->heap_list_type = RESERVE_LIST;
    heap_type->heap_sub_type = SUB_RESERVE_TYPE;
    /* uva reserve uses independent mem_type slot to separate heap_list from non-uva reserve */
    heap_type->heap_mem_type = (advise & DV_ADVISE_HOST_UVA) ? DEVMM_HOST_PIN_MEM : 0;
    if ((advise & DV_ADVISE_HUGEPAGE) != 0) {
        heap_type->heap_type = DEVMM_HEAP_HUGE_PAGE;
    } else {
        heap_type->heap_type = DEVMM_HEAP_CHUNK_PAGE;
    }
}

static void devmm_fill_heap_type(uint32_t devid, uint32_t sub_mem_type, DVmem_advise advise,
                                 struct devmm_virt_heap_type *heap_type)
{
    if (sub_mem_type == SUB_DEVICE_TYPE) {
        devmm_fill_dev_heap_type(devid, advise, heap_type);
    } else if (sub_mem_type == SUB_DVPP_TYPE) {
        devmm_fill_dvpp_heap_type(devid, heap_type);
    } else if ((sub_mem_type == SUB_READ_ONLY_TYPE) || (sub_mem_type == SUB_DEV_READ_ONLY_TYPE)) {
        devmm_fill_read_only_heap_type(devid, sub_mem_type, heap_type);
    } else if (sub_mem_type == SUB_HOST_TYPE) {
        devmm_fill_host_heap_type(heap_type);
    } else if (sub_mem_type == SUB_SVM_TYPE) {
        devmm_fill_svm_heap_type(advise, heap_type);
    } else if (sub_mem_type == SUB_RESERVE_TYPE) {
        devmm_fill_reserve_heap_type(advise, heap_type);
#ifndef UVM_OPEN
    } else if (sub_mem_type == SUB_UVM_TYPE) {
        devmm_fill_uvm_heap_type(heap_type);
#endif
    } else if (sub_mem_type == SUB_SOMA_TYPE) {
        devmm_fill_soma_heap_type(heap_type);
    } else {
        /* impossible branch, just print, not return */
        DEVMM_DRV_ERR("sub_mem_type is invalid. (sub_mem_type=%u)\n", sub_mem_type);
    }
}

static inline bool devmm_is_mem_host_side(uint32_t side)
{
    return (side == MEM_HOST_SIDE) || (side == MEM_HOST_NUMA_SIDE);
}

drvError_t devmm_alloc_proc(uint32_t devid, uint32_t sub_mem_type, DVmem_advise advise, size_t size, DVdeviceptr *pp)
{
    struct devmm_virt_heap_type heap_type;
    void *dst_ptr = NULL;
    DVresult result;

    if (devid >= SVM_MAX_AGENT_NUM) {
        DEVMM_DRV_ERR("Devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }
    devmm_fill_heap_type(devid, sub_mem_type, advise, &heap_type);
    result = devmm_alloc_managed(pp, size, &heap_type, advise);
    if ((result == DRV_ERROR_NONE) && (heap_type.heap_list_type == HOST_AGENT_LIST)) {
        result = halHostRegister((void *)*pp, size, DEV_SVM_MAP_HOST, devid, &dst_ptr);
        if (result != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Register failed. (ptr=%llx; size=%llu)\n", *(DVdeviceptr *)pp, size);
            (void)halMemFreeInner((void *)(uintptr_t)*pp);
        }
    }

    return result;
}

#ifndef EMU_ST
/* because memcpy_s 2nd para must less than(or equal to) 2G, devmm will copy big memory by loop */
STATIC DVresult devmm_h2h_copy(DVdeviceptr dst, size_t dst_max, DVdeviceptr src, size_t count)
{
    size_t rest_count, per_count;
    int ret;

    if (dst_max < count) {
        DEVMM_DRV_ERR("Count bigger than dstMax. (dst=0x%llx; src=0x%llx; count=%lu; dstMax=%lu)\n", dst, src, count,
                      dst_max);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (count <= SECUREC_MEM_MAX_LEN && dst_max <= SECUREC_MEM_MAX_LEN) {
        ret = memcpy_s((void *)(uintptr_t)dst, dst_max, (void *)(uintptr_t)src, count);
        if (ret != 0) {
            DEVMM_DRV_ERR("Memcpy error. (dst=0x%llx; dstMax=%llu; src=0x%llx; count=%llu; ret=%d)\n", dst, dst_max,
                          src, count, ret);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else if (count <= SECUREC_MEM_MAX_LEN && dst_max > SECUREC_MEM_MAX_LEN) {
        ret = memcpy_s((void *)(uintptr_t)dst, SECUREC_MEM_MAX_LEN, (void *)(uintptr_t)src, count);
        if (ret != 0) {
            DEVMM_DRV_ERR("Memcpy error. (dst=0x%llx; dstMax=%llu; src=0x%llx; count=%llu; ret=%d)\n", dst,
                          SECUREC_MEM_MAX_LEN, src, count, ret);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else if (count > SECUREC_MEM_MAX_LEN) {
        for (rest_count = count; rest_count > 0;) {
            per_count = rest_count > SECUREC_MEM_MAX_LEN ? SECUREC_MEM_MAX_LEN : rest_count;
            ret = memcpy_s((void *)(uintptr_t)dst, per_count, (void *)(uintptr_t)src, per_count);
            if (ret != 0) {
                DEVMM_DRV_ERR("Memcpy error. (dst=0x%llx; src=0x%llx; per_count=%lu; ret=%d)\n", dst, src, per_count,
                              ret);
                return DRV_ERROR_INVALID_VALUE;
            }
            dst = dst + per_count;
            src = src + per_count;
            rest_count = rest_count - per_count;
        }
    }

    return DRV_ERROR_NONE;
}
#endif

STATIC DVresult devmm_copy_ioctl(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t count, unsigned long cmd)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.data.copy_para.dst = dst;
    arg.data.copy_para.src = src;
    arg.data.copy_para.ByteCount = count;
    arg.data.copy_para.direction = DEVMM_COPY_INVILED_DIRECTION;
    arg.data.copy_para.is_support_dev_local_addr = false;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, cmd, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Ioctl error. (cmd=%lu; ret=%d; dst=0x%llx; src=0x%llx; size=%lu)\n", cmd, ret, dst, src,
                         count);
        devmm_print_svm_va_info(src, ret);
        devmm_print_svm_va_info(dst, ret);
        return ret;
    }

    if ((cmd == (unsigned int)DEVMM_SVM_MEMCPY) && (arg.data.copy_para.direction == DEVMM_COPY_HOST_TO_HOST)) {
        return devmm_h2h_copy(dst, dest_max, src, count);
    }

    return DRV_ERROR_NONE;
}

#ifdef CFG_HOST_CHIP_HI3559A
DVresult devmm_copy_cache_incoherence(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count)
{
    DVdeviceptr dst_p;
    DVdeviceptr src_p;
    int ret;
    /*
     * if host dma not support cache consistency
     * svm host mem is 4k per blk, can ensure cache consistency
     * so malloc svm host mem to dma use
     */
    if (!devmm_va_is_svm(dst)) {
        ret = drv_mem_alloc_host((void **)&dst_p, byte_count);
        if (ret != 0) {
            DEVMM_DRV_ERR("DrvMemAllocHost failed. (dst=0x%llx; byte_count=%lu)\n", dst, byte_count);
            return ret;
        }
        ret = devmm_copy_ioctl(dst_p, dest_max, src, byte_count, DEVMM_SVM_MEMCPY);
        if (ret != 0) {
            DEVMM_DRV_ERR("Copy failed. (dst_p=0x%llx; dst=0x%llx; src=0x%llx; byte_count=%lu)\n", dst_p, dst, src,
                          byte_count);
            (void)halMemFreeInner((void *)dst_p);
            return ret;
        }
        ret = devmm_h2h_copy(dst, dest_max, dst_p, byte_count);
        (void)halMemFreeInner((void *)dst_p);
        return ret;
    }

    if (!devmm_va_is_svm(src)) {
        ret = drv_mem_alloc_host((void **)&src_p, byte_count);
        if (ret != 0) {
            DEVMM_DRV_ERR("DrvMemAllocHost failed. (src=0x%llx; byte_count=%lu)\n", src, byte_count);
            return ret;
        }
        ret = devmm_h2h_copy(src_p, byte_count, src, byte_count);
        if (ret != 0) {
            DEVMM_DRV_ERR("Trans copy failed. (src_p=0x%llx; src=0x%llx)\n", src_p, src);
            (void)halMemFreeInner((void *)src_p);
            return ret;
        }
        ret = devmm_copy_ioctl(dst, dest_max, src_p, byte_count, DEVMM_SVM_MEMCPY);
        (void)halMemFreeInner((void *)src_p);
        return ret;
    }

    return 0;
}
#endif

STATIC INLINE DVresult devmm_copy_cache_coherence(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count)
{
    return devmm_copy_ioctl(dst, dest_max, src, byte_count, DEVMM_SVM_MEMCPY);
}
#ifndef UVM_OPEN
STATIC INLINE DVresult devmm_uvm_copy_cache_coherence(DVdeviceptr dst, size_t dest_max, DVdeviceptr src,
                                                      size_t byte_count)
{
    return devmm_copy_ioctl(dst, dest_max, src, byte_count, DEVMM_UVM_MEMCPY);
}
#endif
static DVresult devmm_get_devid_by_va(DVdeviceptr va, uint32_t *devid)
{
    struct devmm_virt_com_heap *heap = NULL;

    heap = devmm_va_to_heap((virt_addr_t)va);
    if (heap == NULL) {
        return DRV_ERROR_INVALID_VALUE;
    }
    *devid = devmm_heap_device_by_list_type(heap->heap_list_type);
    return DRV_ERROR_NONE;
}

static DVresult devmm_try_copy_by_host_mem_pool(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count,
                                                bool src_is_svm)
{
    DVdeviceptr svm_va = src_is_svm ? src : dst;
    void *cache_va = NULL;
    void *fd = NULL;
    uint32_t devid;
    DVresult ret;

    ret = devmm_get_devid_by_va(svm_va, &devid);
    if (ret != DRV_ERROR_NONE) {
        return devmm_copy_cache_coherence(dst, dest_max, src, byte_count);
    }

    fd = devmm_host_mem_pool_get(devid, byte_count, &cache_va);
    if (fd == NULL) {
        return devmm_copy_cache_coherence(dst, dest_max, src, byte_count);
    }
#ifndef EMU_ST
    if (src_is_svm) { /* D2H */
        ret = devmm_copy_cache_coherence((DVdeviceptr)(uintptr_t)cache_va, byte_count, src, byte_count);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Devmm_copy_cache_coherence failed. (ret=%d; dst=0x%llx; src=0x%llx; byte_count=%zu)\n", ret,
                          (uint64_t)(uintptr_t)cache_va, src, byte_count);
            devmm_host_mem_pool_put(devid, fd);
            return ret;
        }
        if (memcpy_s((void *)(uintptr_t)dst, byte_count, cache_va, byte_count) != 0) {
            DEVMM_DRV_ERR("Memcpy_s failed. (dst=0x%llx; src=0x%llx; byte_count=%zu)\n", dst,
                          (uint64_t)(uintptr_t)cache_va, byte_count);
            devmm_host_mem_pool_put(devid, fd);
            return DRV_ERROR_PARA_ERROR;
        }
    } else { /* H2D */
        if (memcpy_s(cache_va, byte_count, (void *)(uintptr_t)src, byte_count) != 0) {
            DEVMM_DRV_ERR("Memcpy_s failed. (dst=0x%llx; src=0x%llx; byte_count=%zu)\n", (uint64_t)(uintptr_t)cache_va,
                          src, byte_count);
            devmm_host_mem_pool_put(devid, fd);
            return DRV_ERROR_PARA_ERROR;
        }
        ret = devmm_copy_cache_coherence(dst, byte_count, (DVdeviceptr)(uintptr_t)cache_va, byte_count);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Devmm_copy_cache_coherence failed. (ret=%d; dst=0x%llx; src=0x%llx; byte_count=%zu)\n", ret,
                          dst, (uint64_t)(uintptr_t)cache_va, byte_count);
            devmm_host_mem_pool_put(devid, fd);
            return ret;
        }
    }
#endif
    devmm_host_mem_pool_put(devid, fd);
    DEVMM_DRV_DEBUG_ARG("Copy by host mem pool success. (dst=0x%llx; src=0x%llx; byte_count=%zu; cache_va=0x%llx)\n",
                        dst, src, byte_count, (uint64_t)(uintptr_t)cache_va);
    return DRV_ERROR_NONE;
}

static bool devmm_is_need_host_mem_pool(void *src, void *dst)
{
    bool src_is_svm_range = devmm_va_is_in_svm_range((uint64_t)(uintptr_t)src);
    bool dst_is_svm_range = devmm_va_is_in_svm_range((uint64_t)(uintptr_t)dst);
    void *svm_va = src_is_svm_range ? src : dst;
    struct devmm_virt_heap_mgmt *mgmt = NULL;
    struct devmm_virt_com_heap *heap = NULL;

#ifndef EMU_ST
    if ((src_is_svm_range && dst_is_svm_range) || ((src_is_svm_range == false) && (dst_is_svm_range == false))) {
        return false;
    }
#endif

    mgmt = devmm_virt_get_heap_mgmt();
    if ((mgmt == NULL) || (mgmt->support_host_mem_pool == false)) {
        return false;
    }

    if (devmm_is_in_host_pin_range((virt_addr_t)(uintptr_t)svm_va)) {
        heap = devmm_va_to_heap((virt_addr_t)(uintptr_t)svm_va);
    } else {
        heap = devmm_virt_get_heap_mgmt_virt_heap(devmm_va_to_heap_idx(mgmt, (virt_addr_t)(uintptr_t)svm_va, false));
    }

    if (heap == NULL) {
        return false;
    }
    if ((heap->heap_sub_type == SUB_DEVICE_TYPE) || (heap->heap_sub_type == SUB_DEV_READ_ONLY_TYPE)) {
        return true;
    }

    return false;
}

static DVresult _drvMemcpyInnerCheckArgs(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count)
{
    DEVMM_DRV_DEBUG_ARG("Argument. (dst=0x%llx; src=0x%llx; byte_count=%lu)\n", dst, src, byte_count);
    /* Similar to memcpy_s, (byte_count == 0) need to return 0 */
    if ((dst == 0) || (src == 0) || (dest_max == 0) || (dest_max < byte_count)) {
        DEVMM_DRV_ERR("Invalid argument. (dst=0x%llx; src=0x%llx; byte_count=%lu; dest_max=%lu)\n", dst, src,
                      byte_count, dest_max);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_is_vmm_mix_map(src, byte_count) || svm_is_vmm_mix_map(dst, byte_count)) {
        DEVMM_DRV_ERR("Vmm mix map is invalid. (dst=0x%llx; src=0x%llx; byte_count=%lu; dest_max=%lu)\n", dst, src,
                      byte_count, dest_max);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

static DVresult _drvMemcpyInner(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count)
{
    bool src_is_svm, dst_is_svm;
    DVresult ret;
    ret = _drvMemcpyInnerCheckArgs(dst, dest_max, src, byte_count);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    /* Similar to memcpy_s, (byte_count == 0) need to return 0 */
    if (byte_count == 0) {
        return DRV_ERROR_NONE;
    }

    if (devmm_vaddr_and_size_crosses_soma_boundary(src, byte_count) ||
        devmm_vaddr_and_size_crosses_soma_boundary(dst, byte_count)) {
        DEVMM_DRV_ERR("Memcpy range crosses soma boundary. (dst=0x%llx; src=0x%llx; byte_count=%lu)\n", dst, src,
                      byte_count);
        return DRV_ERROR_INVALID_VALUE;
    }
    src_is_svm = devmm_va_is_svm(src);
    dst_is_svm = devmm_va_is_svm(dst);
#ifndef UVM_OPEN
    bool src_is_uvm, dst_is_uvm;
    src_is_uvm = devmm_va_is_uvm(src);
    dst_is_uvm = devmm_va_is_uvm(dst);
    if (src_is_uvm || dst_is_uvm) {
        return devmm_uvm_copy_cache_coherence(dst, dest_max, src, byte_count);
    }
#endif
    bool src_is_soma, dst_is_soma;
    src_is_soma = devmm_vaddr_and_size_is_in_soma_range(src, byte_count);
    dst_is_soma = devmm_vaddr_and_size_is_in_soma_range(dst, byte_count);
    if ((src_is_svm == false) && (dst_is_svm == false) && (src_is_soma == false) && (dst_is_soma == false)) {
        return devmm_h2h_copy(dst, dest_max, src, byte_count);
    }

#ifdef CFG_HOST_CHIP_HI3559A
    if (!src_is_svm || !dst_is_svm || !src_is_soma || !dst_is_soma) {
        return devmm_copy_cache_incoherence(dst, dest_max, src, byte_count);
    }
#endif

    if (devmm_is_need_host_mem_pool((void *)(uintptr_t)src, (void *)(uintptr_t)dst)) {
        return devmm_try_copy_by_host_mem_pool(dst, dest_max, src, byte_count, devmm_va_is_in_svm_range(src));
    }

    return devmm_copy_cache_coherence(dst, dest_max, src, byte_count);
}

static bool devmm_host_support_pin_user_pages(struct devmm_virt_heap_mgmt *p_heap_mgmt)
{
    return p_heap_mgmt->host_support_pin_user_pages_interface;
}

static size_t devmm_get_host_page_size(struct devmm_virt_heap_mgmt *p_heap_mgmt)
{
    return p_heap_mgmt->local_page_size;
}

static DVresult devmm_mlock_first_page(DVdeviceptr va, size_t byte_count)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;
    uint64_t page_size, offset, lock_len;
    int ret;

    p_heap_mgmt = devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        DEVMM_DRV_WARN("Get heap mgmt failed, should drvMemDeviceOpen first.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    page_size = devmm_get_host_page_size(p_heap_mgmt);
    offset = va % page_size;
    lock_len = (byte_count >= (page_size - offset)) ? (page_size - offset) : byte_count;

    /*
     * If kernel support pin_user_pages*, no need mlock,
     * pin_user_pages* will prevent concurrency with page swap.
     */
#ifndef UVM_OPEN
    if (devmm_host_support_pin_user_pages(p_heap_mgmt) || devmm_va_is_svm(va) || devmm_va_is_uvm(va) || (offset == 0)) {
        return DRV_ERROR_INVALID_HANDLE;
    }
#else
    if (devmm_host_support_pin_user_pages(p_heap_mgmt) || devmm_va_is_svm(va) || (offset == 0)) {
        return DRV_ERROR_INVALID_HANDLE;
    }
#endif
    ret = mlock((const void *)va, lock_len);
    if (ret != 0) {
        DEVMM_DRV_WARN("Mlock failed. (ret=%d; va=0x%llx; len=%llu; lock_len=%llu)\n", ret, va, (uint64_t)byte_count,
                       (uint64_t)lock_len);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return DRV_ERROR_NONE;
}

static void devmm_munlock_first_page(DVdeviceptr va, size_t byte_count)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;
    uint64_t page_size, offset, unlock_len;
    int ret;

    p_heap_mgmt = devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        DEVMM_DRV_WARN("Get heap mgmt failed, should drvMemDeviceOpen first.\n");
        return;
    }

    page_size = devmm_get_host_page_size(p_heap_mgmt);
    offset = va % page_size;
    unlock_len = (byte_count >= (page_size - offset)) ? (page_size - offset) : byte_count;

    ret = munlock((const void *)va, unlock_len);
    if (ret != 0) {
        DEVMM_DRV_WARN("Munlock failed. (ret=%d; va=0x%llx; len=%llu; lock_len=%llu)\n", ret, va, (uint64_t)byte_count,
                       (uint64_t)unlock_len);
    }
}

DVresult drvMemcpyInner(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count)
{
    DVresult mlock_ret, ret;

    /* Mlock to prevent page swap, dma copy with page swap may cause data consistency problems. */
    mlock_ret = devmm_mlock_first_page(dst, byte_count);
    ret = _drvMemcpyInner(dst, dest_max, src, byte_count);
    if (mlock_ret == DRV_ERROR_NONE) {
        devmm_munlock_first_page(dst, byte_count);
    }
    return ret;
}

DVresult drvMemcpy(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count)
{
    return drvMemcpyInner(dst, dest_max, src, byte_count);
}

static drvError_t devmm_mem_cpy_para_check(void *dst, size_t dst_size, void *src, size_t count,
                                           struct memcpy_info *info)
{
    if (dst == NULL) {
        DEVMM_DRV_ERR("Dst is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (src == NULL) {
        DEVMM_DRV_ERR("Src is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dst_size == 0) {
        DEVMM_DRV_ERR("Dst size is zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dst_size < count) {
        DEVMM_DRV_ERR("Dst size larger than cpy size. (dst_size=%lu; count=%lu)\n", dst_size, count);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (info == NULL) {
        DEVMM_DRV_ERR("Info is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (info->dir != DRV_MEMCPY_DEVICE_TO_HOST) {
        DEVMM_DRV_INFO("Invalid dir type. (dir_type=%u)\n", info->dir);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (info->devid >= DEVMM_MAX_DEVICE_NUM) {
        DEVMM_DRV_ERR("Invalid devid. (devid=%u)\n", info->devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (devmm_dev_is_inited(info->devid) == false) {
        DEVMM_DRV_ERR("Device is not inited. (devid=%u)\n", info->devid);
        return DRV_ERROR_NOT_EXIST;
    }
    return DRV_ERROR_NONE;
}

drvError_t halMemcpy(void *dst, size_t dst_size, void *src, size_t count, struct memcpy_info *info)
{
    struct devmm_ioctl_arg arg = {0};
    void *tmp_dst = dst;
    void *fd = NULL;
    drvError_t ret;

    // drvMemcpy
    if (info == NULL) {
        return drvMemcpyInner((DVdeviceptr)dst, dst_size, (DVdeviceptr)src, count);
    }

    ret = devmm_mem_cpy_para_check(dst, dst_size, src, count, info);
    if (ret != 0) {
        return ret;
    }

    DEVMM_DRV_DEBUG_ARG("Argument. (dst=0x%llx; src=0x%llx; ByteCount=%lu; devid=%u)\n", dst, src, count, info->devid);

    /* Similar to memcpy_s, (count == 0) need to return 0 */
    if (count == 0) {
        return DRV_ERROR_NONE;
    }

    if (devmm_is_need_host_mem_pool(src, dst)) {
        fd = devmm_host_mem_pool_get(info->devid, count, &tmp_dst);
    }

    arg.head.devid = info->devid;
    arg.data.copy_para.dst = (uint64_t)(uintptr_t)tmp_dst;
    arg.data.copy_para.src = (uint64_t)(uintptr_t)src;
    arg.data.copy_para.ByteCount = count;
    arg.data.copy_para.direction = (uint32_t)info->dir;
    arg.data.copy_para.is_support_dev_local_addr = true;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEMCPY, &arg);
    if (ret != DRV_ERROR_NONE) {
        if (fd != NULL) {
            devmm_host_mem_pool_put(info->devid, fd);
        }
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Ioctl error. (ret=%d; dst=0x%llx; src=0x%llx; size=%zu; tmp_dst=0x%llx)\n", ret,
                         (uint64_t)(uintptr_t)dst, (uint64_t)(uintptr_t)src, count, (uint64_t)(uintptr_t)tmp_dst);
        devmm_print_svm_va_info((uint64_t)(uintptr_t)src, ret);
        devmm_print_svm_va_info((uint64_t)(uintptr_t)dst, ret);
        return ret;
    }

    if (fd != NULL) {
        if (memcpy_s(dst, dst_size, tmp_dst, count) != 0) {
            DEVMM_DRV_ERR("Memcpy_s error. (dst=0x%llx; tmp_dst=0x%llx; size=%zu)\n", dst, tmp_dst, count);
#ifndef EMU_ST
            ret = DRV_ERROR_PARA_ERROR;
#endif
        }
        devmm_host_mem_pool_put(info->devid, fd);
        return ret;
    }

    return DRV_ERROR_NONE;
}

#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000UL
#endif
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC 1000UL
#endif
static inline uint64_t devmm_get_time_us(void)
{
    struct timespec ts = {0};

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * USEC_PER_SEC + (uint64_t)ts.tv_nsec / NSEC_PER_USEC;
}

DVresult halMemCpyAsyncInner(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count, uint64_t *copy_fd)
{
    struct devmm_ioctl_arg *fd_arg = NULL;
    struct devmm_ioctl_arg ioctl_arg;
    DVresult ret;

    if ((dest_max < byte_count) || (copy_fd == NULL)) {
        DEVMM_DRV_ERR("Input parameter is invalid. (dst=0x%llx; src=0x%llx; byte_count=%lu; dest_max=%lu)\n", dst, src,
                      byte_count, dest_max);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_is_vmm_mix_map(src, byte_count) || svm_is_vmm_mix_map(dst, byte_count)) {
        DEVMM_DRV_ERR("Vmm mix map is invalid. (dst=0x%llx; src=0x%llx; byte_count=%lu; dest_max=%lu)\n", dst, src,
                      byte_count, dest_max);
        return DRV_ERROR_INVALID_VALUE;
    }

    fd_arg = malloc(sizeof(struct devmm_ioctl_arg));
    if (fd_arg == NULL) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    fd_arg->data.async_copy_para.dst = dst;
    fd_arg->data.async_copy_para.src = src;
    fd_arg->data.async_copy_para.byte_count = byte_count;
    fd_arg->data.async_copy_para.cpy_state = 0;
    ioctl_arg = *fd_arg;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_ASYNC_MEMCPY, &ioctl_arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Asynchronous copy error. (ret=%d; dst=0x%llx; src=0x%llx; byte_count=%lu)\n", ret, dst, src,
                      byte_count);
        free(fd_arg);
        return ret;
    }
    fd_arg->data.async_copy_para.task_id = ioctl_arg.data.async_copy_para.task_id;
    fd_arg->data.async_copy_para.start_time = devmm_get_time_us();
    *copy_fd = (uint64_t)(uintptr_t)fd_arg;
    DEVMM_DRV_DEBUG_ARG("Asynchronous copy committed. (dst=0x%llx; src=0x%llx; size=%lu; task_id=%d)\n",
                        fd_arg->data.async_copy_para.dst, fd_arg->data.async_copy_para.src,
                        fd_arg->data.async_copy_para.byte_count, fd_arg->data.async_copy_para.task_id);

    return DRV_ERROR_NONE;
}

DVresult halMemCpyAsync(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count, uint64_t *copy_fd)
{
    return halMemCpyAsyncInner(dst, dest_max, src, byte_count, copy_fd);
}

#define DEVMM_ASYNC_CPY_FINISH_TRY_TIME 5000000UL /* 5000000us */
DVresult halMemCpyAsyncWaitFinishInner(uint64_t copy_fd)
{
    struct devmm_ioctl_arg *arg = (struct devmm_ioctl_arg *)(uintptr_t)copy_fd;
    uint64_t retry_time, finish_val, start_time, now_time;
    DVresult ret = DRV_ERROR_NONE;

    if (arg == NULL) {
        return DRV_ERROR_NOT_EXIST;
    }

    retry_time = DEVMM_ASYNC_CPY_FINISH_TRY_TIME * (arg->data.async_copy_para.byte_count / 65536UL + 1); /* 65536 64k */
    start_time = devmm_get_time_us();
    now_time = start_time;
    while (arg->data.async_copy_para.cpy_state == 0) {
        now_time = devmm_get_time_us();
        if ((now_time - start_time) > retry_time) {
            break;
        }
        ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEMCPY_RESLUT_REFRESH, arg);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Copy result refresh error, check memory validity. "
                          "(dst=0x%llx; src=0x%llx; byte_count=%lu; ret=%d)\n",
                          arg->data.async_copy_para.dst, arg->data.async_copy_para.src,
                          arg->data.async_copy_para.byte_count, ret);
            break;
        }
    }
    finish_val = arg->data.async_copy_para.cpy_state;
    if (finish_val == 0) {
        DEVMM_DRV_ERR("Asynchronous copy timeout. "
                      "(dst=0x%llx; src=0x%llx; size=%lu; wait_time=%lluus; dma_time=%lluus)\n",
                      arg->data.async_copy_para.dst, arg->data.async_copy_para.src,
                      arg->data.async_copy_para.byte_count, (now_time - start_time),
                      now_time - arg->data.async_copy_para.start_time);
        ret = DRV_ERROR_WAIT_TIMEOUT;
    } else if (finish_val != DEVMM_ASYNC_CPY_FINISH_VALUE) {
        DEVMM_DRV_ERR("Asynchronous copy error. (dst=0x%llx; src=0x%llx; byte_count=%lu; dma_status=%llu)\n",
                      arg->data.async_copy_para.dst, arg->data.async_copy_para.src,
                      arg->data.async_copy_para.byte_count, finish_val);
        ret = DRV_ERROR_INNER_ERR;
    }
    DEVMM_DRV_DEBUG_ARG("Asynchronous copy success. "
                        "(dst=0x%llx; src=0x%llx; size=%lu; wait_time=%llu us; dma_time=%llu us; task_id=%d)\n",
                        arg->data.async_copy_para.dst, arg->data.async_copy_para.src,
                        arg->data.async_copy_para.byte_count, (now_time - start_time),
                        now_time - arg->data.async_copy_para.start_time, arg->data.async_copy_para.task_id);

    free(arg);
    return ret;
}

DVresult halMemCpyAsyncWaitFinish(uint64_t copy_fd)
{
    return halMemCpyAsyncWaitFinishInner(copy_fd);
}

DVresult halMemcpySumbitInner(struct DMA_ADDR *dma_addr, int flag)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    if ((dma_addr == NULL) || (flag >= (int)MEMCPY_SUMBIT_MAX_TYPE) || (flag < (int)MEMCPY_SUMBIT_SYNC)) {
        DEVMM_DRV_ERR("Input parameter is invalid, please check. (addr_is_null=%u; flag=%d)\n", (dma_addr == NULL),
                      flag);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* host agent not support convert */
    if (dma_addr->virt_id >= DEVMM_MAX_PHY_DEVICE_NUM) {
        DEVMM_DRV_ERR("Virt_id is out of range. (virt_id=%u; range=0-%u)\n", dma_addr->virt_id,
                      (UINT32)(DEVMM_MAX_PHY_DEVICE_NUM - 1));
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.head.devid = dma_addr->virt_id;
    arg.data.convert_copy_para.dmaAddr = *dma_addr;
    arg.data.convert_copy_para.sync_flag = flag;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_SUMBIT_CONVERT_CPY, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Asynchronous copy error. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

DVresult halMemcpySumbit(struct DMA_ADDR *dma_addr, int flag)
{
    return halMemcpySumbitInner(dma_addr, flag);
}

DVresult halMemcpyWaitInner(struct DMA_ADDR *dma_addr)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    if (dma_addr == NULL) {
        DEVMM_DRV_ERR("Ptr is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    /* host agent not support convert */
    if (dma_addr->virt_id >= DEVMM_MAX_PHY_DEVICE_NUM) {
        DEVMM_DRV_ERR("Virt_id is out of range. (virt_id=%u; range=0-%u)\n", dma_addr->virt_id,
                      (UINT32)(DEVMM_MAX_PHY_DEVICE_NUM - 1));
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.head.devid = dma_addr->virt_id;
    arg.data.convert_copy_para.dmaAddr = *dma_addr;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_WAIT_CONVERT_CPY_RESLUT, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Asynchronous copy wait error. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

DVresult halMemcpyWait(struct DMA_ADDR *dma_addr)
{
    return halMemcpyWaitInner(dma_addr);
}

/* Sunset interface: external statement has been removed from ascend_hal.h. */
DVresult drvLoadProgram(DVdevice device_id, void *program, unsigned int offset, size_t byte_count, void **v_ptr)
{
    (void)device_id;
    (void)program;
    (void)offset;
    (void)byte_count;
    (void)v_ptr;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC INLINE DVresult devmm_convert_proc(struct devmm_ioctl_arg *arg, struct DMA_ADDR *dma_addr)
{
    uint64_t dst = arg->data.convrt_para.pDst;
    uint64_t src = arg->data.convrt_para.pSrc;
    uint64_t dpitch = arg->data.convrt_para.dpitch;
    uint64_t spitch = arg->data.convrt_para.spitch;
    uint64_t width = arg->data.convrt_para.len;
    uint64_t height = arg->data.convrt_para.height;
    uint64_t fixed_size = arg->data.convrt_para.fixed_size;
    uint64_t dir = (uint64_t)arg->data.convrt_para.direction;
    void *priv = NULL;
    DVresult ret;

    ret = devmm_convert_pre(&arg->data.convrt_para, &priv);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Convert_address_pre failed. (dst=0x%llx; src=0x%llx; dpitch=%llu; spitch=%llu; width=%llu; "
                      "height=%llu; fixed_size=%llu; direction=%u; ret=%d)\n",
                      dst, src, dpitch, spitch, width, height, fixed_size, dir, ret);
        priv = NULL;
        return ret;
    }

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_CONVERT_ADDR, arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Ioctl convert error. (dst=0x%llx; src=0x%llx; dpitch=%llu; spitch=%llu; width=%llu; "
                         "height=%llu; fixed_size=%llu; direction=%u; ret=%d)\n",
                         dst, src, dpitch, spitch, width, height, fixed_size, dir, ret);
        (void)devmm_destroy_convert_task_node(priv);
        priv = NULL;
        return ret;
    }

    *dma_addr = arg->data.convrt_para.dmaAddr;
    devmm_convert_post(&arg->data.convrt_para, &priv, (void *)dma_addr);

    DEVMM_DRV_DEBUG_ARG("Convert_proc. (dst=0x%llx; src=0x%llx; dpitch=%llu; spitch=%llu; width=%llu; "
                        "height=%llu; fixed_size=%llu; direction=%u; ret=%d; virt_id=%u; fix_size=%u; "
                        "offset=%llu)\n",
                        dst, src, dpitch, spitch, width, height, fixed_size, dir, ret, dma_addr->virt_id,
                        dma_addr->fixed_size, dma_addr->offsetAddr.offset);
    return DRV_ERROR_NONE;
}

DVresult drvMemConvertAddr(DVdeviceptr p_src, DVdeviceptr p_dst, UINT32 len, struct DMA_ADDR *dma_addr)
{
    struct devmm_ioctl_arg arg = {0};

    DEVMM_DRV_DEBUG_ARG("Convert address. (pSrc=0x%llx; pDst=0x%llx; len=%u)\n", p_src, p_dst, len);

    if (dma_addr == NULL) {
        DEVMM_DRV_ERR("Parameter dmaAddr is NULL. (pSrc=0x%llx; pDst=0x%llx; len=%u)\n", p_src, p_dst, len);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* host agent not support convert */
    if (dma_addr->offsetAddr.devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
        DEVMM_DRV_ERR("Devid is out of range. (devid=%u; range=0~%u)\n", dma_addr->offsetAddr.devid,
                      (UINT32)(DEVMM_MAX_PHY_DEVICE_NUM - 1));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_is_vmm_mix_map(p_src, len) || svm_is_vmm_mix_map(p_dst, len)) {
        DEVMM_DRV_ERR("Vmm mix map is invalid. (dst=0x%llx; src=0x%llx; len=%lu)\n", p_dst, p_src, len);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.head.devid = dma_addr->offsetAddr.devid;
    arg.data.convrt_para.virt_id = dma_addr->offsetAddr.devid;
    arg.data.convrt_para.pSrc = p_src;
    arg.data.convrt_para.pDst = p_dst;
    arg.data.convrt_para.len = len;
    arg.data.convrt_para.dpitch = len;
    arg.data.convrt_para.spitch = len;
    arg.data.convrt_para.height = 1;
    arg.data.convrt_para.fixed_size = 0;
    arg.data.convrt_para.direction = DEVMM_COPY_INVILED_DIRECTION;

    return devmm_convert_proc(&arg, dma_addr);
}

static DVresult devmm_destroy_para_check(struct DMA_ADDR *ptr[], uint32_t num)
{
    uint32_t i;

    if (ptr == NULL) {
        DEVMM_DRV_ERR("Ptr[] is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (num == 0) {
        DEVMM_DRV_ERR("num is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < num; i++) {
        if (ptr[i] == NULL) {
            DEVMM_DRV_ERR("Ptr is NULL.\n");
            return DRV_ERROR_INVALID_VALUE;
        }

        /* host agent not support convert */
        if (ptr[i]->virt_id >= DEVMM_MAX_PHY_DEVICE_NUM) {
            DEVMM_DRV_ERR("Virt_id is out of range. (virt_id=%u; range=0-%u)\n", ptr[i]->virt_id,
                          (UINT32)(DEVMM_MAX_PHY_DEVICE_NUM - 1));
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    return DRV_ERROR_NONE;
}

static DVresult devmm_destroy_addr(struct DMA_ADDR *ptr)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.head.devid = ptr->virt_id;
    arg.data.desty_para.dmaAddr = *ptr;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_DESTROY_ADDR, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Destroy address error. (offset=%llu; ret=%d)\n", arg.data.desty_para.dmaAddr.offsetAddr.offset,
                      ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static DVresult devmm_destroy_addr_batch(struct DMA_ADDR *ptr[], uint32_t num)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.head.devid = ptr[0]->virt_id;
    arg.data.destroy_batch_para.dmaAddr = ptr;
    arg.data.destroy_batch_para.num = num;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_DESTROY_ADDR_BATCH, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Destroy address batch error. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

DVresult drvMemDestroyAddr(struct DMA_ADDR *ptr)
{
    DVresult ret;

    DEVMM_DRV_DEBUG_ARG("Argument. (ptr=0x%p)\n", ptr);
    ret = devmm_destroy_para_check(&ptr, 1);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = devmm_destroy_addr(ptr);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = devmm_destroy_convert((void *)ptr);
    if (ret != DRV_ERROR_NONE) {
        /* do not need print */
        return ret;
    }

    return DRV_ERROR_NONE;
}

static DVresult devmm_destroy_post(struct DMA_ADDR *ptr[], uint32_t num)
{
    DVresult ret;
    uint32_t i;

    for (i = 0; i < num; i++) {
        ret = devmm_destroy_convert((void *)ptr[i]);
        if (ret != DRV_ERROR_NONE) {
            /* do not need print */
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

DVresult halMemDestroyAddrBatch(struct DMA_ADDR *ptr[], uint32_t num)
{
    DVresult ret;

    ret = devmm_destroy_para_check(ptr, num);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (num == 1) {
        ret = devmm_destroy_addr(ptr[0]);
    } else {
        ret = devmm_destroy_addr_batch(ptr, num);
    }
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return devmm_destroy_post(ptr, num);
}

STATIC void devmm_init_mem_advise_record(void)
{
    uint32_t i;

    for (i = 0; i < ADVISE_RECORD_MAX_DEV_NUM; i++) {
        SVM_INIT_LIST_HEAD(&advise_record_list[i]);
        pthread_mutex_init(&advise_record_lock[i], NULL);
    }
}

STATIC void devmm_add_mem_advise_record(DVdeviceptr ptr, size_t count, DVmem_advise advise, DVdevice device)
{
    struct devmm_advise_record_node *record_node = NULL;
    struct devmm_virt_list_head *pos = NULL;
    struct devmm_virt_list_head *n = NULL;

    if ((device >= ADVISE_RECORD_MAX_DEV_NUM) || (advise != DV_ADVISE_PERSISTENT)) {
        return;
    }

    pthread_mutex_lock(&advise_record_lock[device]);
    devmm_virt_list_for_each_safe(pos, n, &advise_record_list[device])
    {
        record_node = devmm_virt_list_entry(pos, struct devmm_advise_record_node, node);
        if ((record_node->ptr == ptr) && (record_node->count == count)) {
            pthread_mutex_unlock(&advise_record_lock[device]);
            return;
        }
    }

    record_node = (struct devmm_advise_record_node *)malloc(sizeof(struct devmm_advise_record_node));
    if (record_node == NULL) {
        DEVMM_DRV_WARN("Malloc not success, not add advise record.\n");
        pthread_mutex_unlock(&advise_record_lock[device]);
        return;
    }

    record_node->ptr = ptr;
    record_node->count = count;
    SVM_INIT_LIST_HEAD(&record_node->node);
    devmm_virt_list_add_tail(&record_node->node, &advise_record_list[device]);
    DEVMM_DRV_DEBUG_ARG("Add mem advise record. (advise=%x; ptr=0x%llx; count=%lu; device=%u)\n", (uint32_t)advise, ptr,
                        count, device);

    pthread_mutex_unlock(&advise_record_lock[device]);
}

STATIC bool devmm_mem_advise_range_is_overlap(DVdeviceptr free_ptr, size_t free_cnt,
                                              struct devmm_advise_record_node *record_node)
{
    if ((free_ptr + free_cnt <= record_node->ptr) || (free_ptr >= record_node->ptr + record_node->count)) {
        return false;
    }
    return true;
}

STATIC void devmm_del_mem_advise_record(bool is_del_all, DVdeviceptr free_ptr, size_t free_cnt, DVdevice device)
{
    struct devmm_advise_record_node *record_node = NULL;
    struct devmm_virt_list_head *pos = NULL;
    struct devmm_virt_list_head *n = NULL;

    if (device >= ADVISE_RECORD_MAX_DEV_NUM) {
        return;
    }

    pthread_mutex_lock(&advise_record_lock[device]);
    devmm_virt_list_for_each_safe(pos, n, &advise_record_list[device])
    {
        record_node = devmm_virt_list_entry(pos, struct devmm_advise_record_node, node);
        if ((is_del_all == true) || devmm_mem_advise_range_is_overlap(free_ptr, free_cnt, record_node)) {
            devmm_virt_list_del(&record_node->node);
            DEVMM_DRV_DEBUG_ARG("Del mem advise record. (ptr=0x%llx; count=%lu; device=%u; is_del_all=%u)\n",
                                record_node->ptr, record_node->count, device, is_del_all);
            free(record_node);
        }
    }
    pthread_mutex_unlock(&advise_record_lock[device]);
}

STATIC DVresult devmm_mem_ioctl_advise(DVdeviceptr ptr, size_t count, DVmem_advise advise, DVdevice device)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.head.devid = device;
    arg.data.advise_para.ptr = ptr;
    arg.data.advise_para.count = count;
    arg.data.advise_para.advise = advise;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_ADVISE, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Ioctl device error. (ret=%d; ptr=0x%llx; count=%lu; advise=%x; device=%u; devid=%u)\n", ret, ptr,
                      count, (uint32_t)advise, device, arg.head.devid);
        return ret;
    }
    return ret;
}
#ifndef UVM_OPEN
static DVresult mem_single_attr_check(uint32_t attribute, uint64_t ptr, size_t size, size_t data_size)
{
    (void)attribute;
    (void)ptr;
    (void)size;
    if (data_size != sizeof(int32_t)) {
        DEVMM_DRV_ERR("Memory single attr data size invalid. (data_size=%d)\n", data_size);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

static DVresult mem_batch_attr_check(uint32_t attribute, uint64_t ptr, size_t size, size_t data_size)
{
    (void)attribute;
    (void)ptr;
    (void)size;
    if ((data_size % sizeof(int32_t)) != 0) {
        DEVMM_DRV_ERR("Memory batch attr data size invalid. (data_size=%d)\n", data_size);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}
typedef drvError_t (*drv_mem_attr_policy)(uint32_t attribute, uint64_t ptr, size_t size, size_t data_size);
static DVresult devmm_mem_range_attribute_check(uint32_t attribute, uint64_t ptr, size_t size, void *data,
                                                size_t data_size)
{
    static const drv_mem_attr_policy mem_attr_policy[MEM_RANGE_ATTR_MAX] = {
        [MEM_RANGE_ATTR_READ_MOSTLY] = mem_single_attr_check,
        [MEM_RANGE_ATTR_PREFERRED_LOC] = mem_single_attr_check,
        [MEM_RANGE_ATTR_ACCESS_BY] = mem_batch_attr_check,
        [MEM_RANGE_ATTR_PREFERRED_LOC_TYPE] = mem_single_attr_check,
        [MEM_RANGE_ATTR_PREFERRED_LOC_ID] = mem_single_attr_check,
        [MEM_RANGE_ATTR_LAST_PREFETCH_LOC] = mem_single_attr_check,
        [MEM_RANGE_ATTR_LAST_PREFETCH_LOC_TYPE] = mem_single_attr_check,
        [MEM_RANGE_ATTR_LAST_PREFETCH_LOC_ID] = mem_single_attr_check,
    };

    if (!devmm_va_is_uvm(ptr)) {
        DEVMM_DRV_ERR("Memory ptr is not uvm.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if ((attribute >= MEM_RANGE_ATTR_MAX) || (attribute == 0) || (size == 0) || (data == NULL) || (data_size == 0)) {
        return DRV_ERROR_INVALID_VALUE;
    }

    return mem_attr_policy[attribute](attribute, ptr, size, data_size);
}

STATIC DVresult devmm_mem_range_attributes_check(uint32_t *attributes, size_t attribute_num, uint64_t ptr, size_t size,
                                                 void **data, size_t *data_sizes)
{
    int ret;
    size_t attr_idx;

    if ((attributes == NULL) || (attribute_num == 0) || (size == 0) || (data == NULL) || (data_sizes == NULL)) {
        return DRV_ERROR_INVALID_VALUE;
    }

    for (attr_idx = 0; attr_idx < attribute_num; attr_idx++) {
        DEVMM_DRV_DEBUG_ARG("Mem managed range get attributes. (attribute=%u, ptr=0x%llx, ptr_size=%zu,"
                            "data_ptr=0x%llx, data_size=%llx, attr_idx=%lu) \n",
                            attributes[attr_idx], ptr, size, (uint64_t)data[attr_idx], data_sizes[attr_idx], attr_idx);
        ret = devmm_mem_range_attribute_check(attributes[attr_idx], ptr, size, data[attr_idx], data_sizes[attr_idx]);
        if (ret) {
            DEVMM_DRV_ERR("Mem managed range attributes check failed. (attr_idx=%lu, ret=%d). \n", attr_idx, ret);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

static DVresult devmm_mem_range_ioctl_attributes(uint32_t *attributes, size_t attribute_num, uint64_t ptr, size_t size,
                                                 void **data, size_t *data_sizes)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.head.devid = 0;
    arg.data.mem_range_attribute_paras.attributes = attributes;
    arg.data.mem_range_attribute_paras.attribute_num = attribute_num;
    arg.data.mem_range_attribute_paras.ptr = (uint64_t)(uintptr_t)ptr;
    arg.data.mem_range_attribute_paras.size = size;
    arg.data.mem_range_attribute_paras.data = data;
    arg.data.mem_range_attribute_paras.data_sizes = data_sizes;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_UVM_MEM_RANGE_GET_ATTRIBUTES, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Ioctl device error. (ret=%d, ptr=0x%llx, size=0x%llx) \n", ret, ptr, size);
        return ret;
    }
    return ret;
}

drvError_t halMemManagedRangeGetAttributes(void **data, size_t *data_sizes, uint32_t *attributes, size_t attribute_num,
                                           DVdeviceptr ptr, size_t size)
{
    int ret = DRV_ERROR_NONE;

    ret = devmm_mem_range_attributes_check(attributes, attribute_num, ptr, size, data, data_sizes);
    if (ret) {
        DEVMM_DRV_ERR("Mem managed range attributes check failed. (ret=%d) \n", ret);
        return ret;
    }

    return devmm_mem_range_ioctl_attributes(attributes, attribute_num, ptr, size, data, data_sizes);
}

STATIC DVresult devmm_mem_managed_ioctl_advise(DVdeviceptr ptr, size_t size, uint32_t advise, int type, int id)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.head.devid = 0;
    arg.data.mem_managed_advise_para.ptr = ptr;
    arg.data.mem_managed_advise_para.size = size;
    arg.data.mem_managed_advise_para.advise = advise;
    arg.data.mem_managed_advise_para.type = type;
    arg.data.mem_managed_advise_para.id = id;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_UVM_ADVISE, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Ioctl  uvm advise error. (ret=%d; ptr=0x%llx; size=%lu; advise=%x)\n", ret, ptr, size, advise);
        return ret;
    }
    return ret;
}
#endif

STATIC DVresult devmm_mem_advise_restore_per_dev(DVdevice device)
{
    struct devmm_advise_record_node *record_node = NULL;
    struct devmm_virt_list_head *pos = NULL;
    struct devmm_virt_list_head *n = NULL;
    DVresult ret;

    pthread_mutex_lock(&advise_record_lock[device]);
    devmm_virt_list_for_each_safe(pos, n, &advise_record_list[device])
    {
        record_node = devmm_virt_list_entry(pos, struct devmm_advise_record_node, node);
        ret = devmm_mem_ioctl_advise(record_node->ptr, record_node->count, DV_ADVISE_PERSISTENT, device);
        if (ret != DRV_ERROR_NONE) {
            pthread_mutex_unlock(&advise_record_lock[device]);
            return ret;
        }

        DEVMM_RUN_INFO("Mem advise restore succ. (ptr=0x%llx; count=%lu; device=%u)\n", record_node->ptr,
                       record_node->count, device);
    }
    pthread_mutex_unlock(&advise_record_lock[device]);
    return DRV_ERROR_NONE;
}

STATIC DVresult devmm_mem_advise_restore(void)
{
    DVresult ret;
    uint32_t i;

    for (i = 0; i < ADVISE_RECORD_MAX_DEV_NUM; i++) {
        ret = devmm_mem_advise_restore_per_dev(i);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Mem advise restore failed. (device=%u; ret=%d)\n", i, ret);
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

STATIC DVresult drv_mem_advise(DVdeviceptr ptr, size_t count, DVmem_advise advise, DVdevice device)
{
    DVresult ret;

    if (!DEVMM_IS_SVM_ADDR(ptr) || (device >= SVM_MAX_AGENT_NUM) || (devmm_dev_is_inited((uint32_t)device) == false)) {
        DEVMM_DRV_ERR("Input parameter error. (ptr=0x%llx; count=%lu; advise=%x; device=%u)\n", ptr, count,
                      (uint32_t)advise, device);
        return DRV_ERROR_INVALID_VALUE;
    }
    /* DV_ADVISE_DDR and DV_ADVISE_P2P_HBM can just set one */
    if (((advise & DV_ADVISE_DDR) != 0) && ((advise & DV_ADVISE_P2P_HBM) != 0)) {
        DEVMM_DRV_ERR("Advise parameter error. (ptr=0x%llx; count=%lu; advise=%x; device=%u)\n", ptr, count,
                      (uint32_t)advise, device);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = devmm_mem_ioctl_advise(ptr, count, advise, device);
    if (ret) {
        DEVMM_DRV_ERR("Advise ioctl failed. (ptr=0x%llx; count=%lu; advise=%x; device=%u)\n", ptr, count,
                      (uint32_t)advise, device);
        return ret;
    }

    devmm_add_mem_advise_record(ptr, count, advise, device);
    DEVMM_DRV_DEBUG_ARG("Argument. (ptr=0x%llx; count=0x%llx; advise=0x%x; device=%u)\n", ptr, count, (uint32_t)advise,
                        device);

    return DRV_ERROR_NONE;
}
#ifndef UVM_OPEN
STATIC DVresult drv_mem_managed_advise(DVdeviceptr ptr, size_t size, uint32_t advise, int type, int id)
{
    DVresult ret;
    if (!devmm_vaddr_and_size_is_in_uvm_range(ptr, size)) {
        DEVMM_DRV_ERR("Input parameter error. (ptr=0x%llx; size=%lu; advise=%x)\n", ptr, size, advise);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = devmm_mem_managed_ioctl_advise(ptr, size, advise, type, id);
    if (ret) {
        DEVMM_DRV_ERR("devmm_mem_managed_ioctl_advise failed. (ptr=0x%llx; size=%lu; advise=%x; ret=%d)\n", ptr, size,
                      advise, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int halGetCurrentThreadNumaNode(void)
{
    uint32_t cpu_id;
    uint32_t numa_node_id;
    long ret;

    ret = syscall(SYS_getcpu, &cpu_id, &numa_node_id, NULL);
    if (ret == -1) {
        DEVMM_DRV_ERR("getcpu failed.\n");
        return -1;
    }

    return (int)numa_node_id;
}

drvError_t halMemManagedPrefetch(DVdeviceptr ptr, size_t size, struct drv_uvm_location location, unsigned int flags)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    if (!DEVMM_IS_UVM_ADDR(ptr) || !DEVMM_IS_UVM_ADDR(ptr + size - 1)) {
        DEVMM_DRV_ERR("Ptr is invalid. (ptr=0x%llx; len=%lu)\n", ptr, size);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (location.type == DRV_UVM_LOCATION_TYPE_DEVICE) {
        if (location.id < 0 || location.id > UVM_MAX_DEVICE_ID) {
            DEVMM_DRV_ERR("Invalid device id. (id=%d)\n", location.id);
            return DRV_ERROR_INVALID_VALUE;
        }
    }
    if (flags != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    arg.data.uvm_prefetch_para.va = ptr;
    arg.data.uvm_prefetch_para.size = size;
    arg.data.uvm_prefetch_para.location.id = location.id;
    arg.data.uvm_prefetch_para.location.type = location.type;
    arg.data.uvm_prefetch_para.flags = flags;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_UVM_PREFETCH, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Prefetch failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t devmm_check_prefetch_batch_param(DVdeviceptr *ptrs, size_t *sizes, size_t count,
                                                   struct drv_uvm_location *prefetchLocs, size_t *prefetchLocIdxs,
                                                   size_t numPrefetchLocs)
{
    size_t i;

    if (ptrs == NULL || sizes == NULL) {
        DEVMM_DRV_ERR("Invalid null pointer. (ptrs=%p, sizes=%p)\n", ptrs, sizes);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (numPrefetchLocs == 0) {
        DEVMM_DRV_ERR("Invalid zero numPrefetchLocs. (numPrefetchLocs=%zu)\n", numPrefetchLocs);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prefetchLocs == NULL || prefetchLocIdxs == NULL) {
        DEVMM_DRV_ERR("Invalid prefetch location pointer. (locs=%p, locIdxs=%p)\n", prefetchLocs, prefetchLocIdxs);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (count < numPrefetchLocs) {
        DEVMM_DRV_ERR("Count is less than numPrefetchLocs. (count=%zu, numPrefetchLocs=%zu)\n", count, numPrefetchLocs);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < count; i++) {
        if (sizes[i] == 0) {
            DEVMM_DRV_ERR("Invalid zero size. (i=%zu, ptr=0x%llx)\n", i, (unsigned long long)ptrs[i]);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (!DEVMM_IS_UVM_ADDR(ptrs[i]) || !DEVMM_IS_UVM_ADDR(ptrs[i] + sizes[i] - 1)) {
            DEVMM_DRV_ERR("Ptr is invalid. (i=%zu, ptr=0x%llx; len=%zu)\n", i, (unsigned long long)ptrs[i], sizes[i]);
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t halMemManagedPrefetchBatch(DVdeviceptr *ptrs, size_t *sizes, size_t count,
                                      struct drv_uvm_location *prefetchLocs, size_t *prefetchLocIdxs,
                                      size_t numPrefetchLocs, unsigned long long flags)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    ret = devmm_check_prefetch_batch_param(ptrs, sizes, count, prefetchLocs, prefetchLocIdxs, numPrefetchLocs);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    arg.data.uvm_prefetch_batch_para.vas = (uint64_t *)ptrs;
    arg.data.uvm_prefetch_batch_para.sizes = (uint64_t *)sizes;
    arg.data.uvm_prefetch_batch_para.count = (uint64_t)count;
    arg.data.uvm_prefetch_batch_para.locations = (struct drv_uvm_location *)prefetchLocs;
    arg.data.uvm_prefetch_batch_para.loc_ids = (uint64_t *)prefetchLocIdxs;
    arg.data.uvm_prefetch_batch_para.num_loc = (uint64_t)numPrefetchLocs;
    arg.data.uvm_prefetch_batch_para.flags = (uint64_t)flags;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_UVM_PREFETCH_BATCH, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Prefetch failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
#endif

DVresult drvMemPrefetchToDevice(DVdeviceptr dev_ptr, size_t len, DVdevice device)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    if (!DEVMM_IS_SVM_ADDR(dev_ptr) || (device >= SVM_MAX_AGENT_NUM)) {
        DEVMM_DRV_ERR("DevPtr is invalid. (dev_ptr=0x%llx; len=%lu; device=%u)\n", dev_ptr, len, device);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_is_vmm_mix_map(dev_ptr, len)) {
        DEVMM_DRV_ERR("Vmm mix map is invalid. (dev_ptr=0x%llx; len=%lu; device=%u)\n", dev_ptr, len, device);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.data.prefetch_para.ptr = dev_ptr;
    arg.data.prefetch_para.count = len;
    arg.head.devid = device;
    DEVMM_DRV_DEBUG_ARG("Argument. (dev_ptr=0x%llx; len=%lu; device=%u)\n", dev_ptr, len, device);

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_PREFETCH, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Ioctl device error. (ret=%d; dev_ptr=0x%llx; len=%lu; device=%u; arg_head_devid=%u)\n",
                         (int)ret, dev_ptr, len, device, arg.head.devid);
        devmm_print_svm_va_info(dev_ptr, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

#ifndef EMU_ST
/* because memset_s 2nd para must less than(or equal to) 2G, devmm will set big memory by loop */
STATIC DVresult devmm_set_host(DVdeviceptr dst, size_t dst_max, UINT8 value, size_t count)
{
    size_t rest_count, per_count;
    int ret;

    if (dst_max < count) {
        DEVMM_DRV_ERR("Count bigger than dst_max. (dst=0x%llx; count=%lu; dst_max=%lu)\n", dst, count, dst_max);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (count <= SECUREC_MEM_MAX_LEN && dst_max <= SECUREC_MEM_MAX_LEN) {
        ret = memset_s((void *)(uintptr_t)dst, dst_max, value, count);
        if (ret != 0) {
            DEVMM_DRV_ERR("Memset error. (dst=0x%llx; dst_max=%llu; value=%u; count=%llu; ret=%d)\n", dst, dst_max,
                          value, count, ret);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else if (count <= SECUREC_MEM_MAX_LEN && dst_max > SECUREC_MEM_MAX_LEN) {
        ret = memset_s((void *)(uintptr_t)dst, SECUREC_MEM_MAX_LEN, value, count);
        if (ret != 0) {
            DEVMM_DRV_ERR("Memset error. (dst=0x%llx; dst_max=%llu; value=%u; count=%llu; ret=%d)\n", dst,
                          SECUREC_MEM_MAX_LEN, value, count, ret);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else if (count > SECUREC_MEM_MAX_LEN) {
        for (rest_count = count; rest_count > 0;) {
            per_count = rest_count > SECUREC_MEM_MAX_LEN ? SECUREC_MEM_MAX_LEN : rest_count;
            ret = memset_s((void *)(uintptr_t)dst, per_count, value, per_count);
            if (ret != 0) {
                DEVMM_DRV_ERR("Memset error. (dst=0x%llx; value=%u; per_count=%lu; ret=%d)\n", dst, value, per_count,
                              ret);
                return DRV_ERROR_INVALID_VALUE;
            }
            dst = dst + per_count;
            rest_count = rest_count - per_count;
        }
    }

    return DRV_ERROR_NONE;
}
#endif

#ifndef UVM_OPEN
STATIC DVresult devmm_svm_memset8_ioctl_slice(DVdeviceptr dst, UINT8 value, size_t num)
{
    const uint64_t memset_slice_size = 1ULL << 30; /* 1GB */
    uint64_t remaining = num;
    uint64_t cur_dst = dst;
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    while (remaining > 0) {
        uint64_t cur_count = (remaining > memset_slice_size) ? memset_slice_size : remaining;

        arg.data.memset_para.dst = cur_dst;
        arg.data.memset_para.value = value;
        arg.data.memset_para.count = cur_count;
        arg.data.memset_para.hostmapped = 0;

        ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEMSET8, &arg);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Memset error. (dst=0x%llx; value=%u; cur_count=%llu; num=%lu; ret=%d)\n", cur_dst, value,
                          cur_count, num, ret);
            return ret;
        }

        if (arg.data.memset_para.hostmapped != 0) {
            ret = devmm_set_host(cur_dst, cur_count, value, cur_count);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
        }

        cur_dst += cur_count;
        remaining -= cur_count;
    }

    return DRV_ERROR_NONE;
}
#endif

DVresult drvMemsetD8Inner(DVdeviceptr dst, size_t dest_max, UINT8 value, size_t num)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    DEVMM_DRV_DEBUG_ARG("Show information. (dst=0x%llx; value=%u; num=%lu)\n", dst, value, num);

    if ((dst == (DVdeviceptr)NULL) || (dest_max < num) || (num == 0)) {
        DEVMM_DRV_ERR("Input parameter error. (dst=0x%llx; value=%u; num=%lu)\n", dst, value, num);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (devmm_vaddr_and_size_crosses_soma_boundary(dst, num)) {
        DEVMM_DRV_ERR("Memset range crosses soma boundary. (dst=0x%llx; value=%u; num=%lu)\n", dst, value, num);
        return DRV_ERROR_INVALID_VALUE;
    }
#ifndef UVM_OPEN
    if (!DEVMM_IS_SVM_ADDR(dst) && !devmm_vaddr_and_size_is_in_uvm_range(dst, num) &&
        !devmm_vaddr_and_size_is_in_soma_range(dst, num)) {
        return devmm_set_host(dst, dest_max, value, num);
    }
#else
    if (!DEVMM_IS_SVM_ADDR(dst) && !devmm_vaddr_and_size_is_in_soma_range(dst, num)) {
        return devmm_set_host(dst, dest_max, value, num);
    }
#endif

#ifndef UVM_OPEN
    if (devmm_vaddr_and_size_is_in_uvm_range(dst, num)) {
        return devmm_svm_memset8_ioctl_slice(dst, value, num);
    }
#endif

    if (svm_is_vmm_mix_map(dst, num)) {
        DEVMM_DRV_ERR("Vmm mix map is invalid. (dst=0x%llx; size=%lu;)\n", dst, num);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.data.memset_para.dst = dst;
    arg.data.memset_para.value = value;
    arg.data.memset_para.count = num;
    arg.data.memset_para.hostmapped = 0;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEMSET8, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Memset error. (dst=0x%llx; value=%u; num=%lu; ret=%d)\n", dst, value, num, ret);
        return ret;
    }

    if (arg.data.memset_para.hostmapped != 0) {
        return devmm_set_host(dst, dest_max, value, num);
    }

    return DRV_ERROR_NONE;
}

DVresult drvMemsetD8(DVdeviceptr dst, size_t destMax, UINT8 value, size_t num)
{
    return drvMemsetD8Inner(dst, destMax, value, num);
}

DVresult drvMemAddressTranslate(DVdeviceptr vptr, UINT64 *pptr)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    if ((pptr == NULL) || !DEVMM_IS_SVM_ADDR(vptr)) {
        DEVMM_DRV_ERR("Vptr is invalid. (vptr=0x%llx)\n", vptr);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.data.translate_para.vptr = vptr;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_TRANSLATE, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Ioctl device error. (vptr=0x%llx; ret=%d)\n", vptr, ret);
        return ret;
    }

    *pptr = (UINT64)arg.data.translate_para.pptr;

    DEVMM_DRV_DEBUG_ARG("Argument. (vptr=0x%llx; *pptr=0x%llx)\n", vptr, *pptr);

    return DRV_ERROR_NONE;
}

/* Sunset interface: external statement has been removed from ascend_hal.h. */
DVresult drvMemAllocL2buffAddr(DVdevice device, void **l2buff, UINT64 *pte)
{
    struct AddrMapInPara in_para;
    struct AddrMapOutPara out_para;
    size_t in_size = sizeof(struct AddrMapInPara);
    size_t out_size = sizeof(struct AddrMapOutPara);
    DVresult ret;
#ifndef DEVMM_UT
    /* host agent not support l2buff */
    if ((l2buff == NULL) || (device >= DEVMM_MAX_PHY_DEVICE_NUM)) {
        DEVMM_DRV_ERR("Input parameter error. (l2buff=%p; pte=%p; device=%u)\n", l2buff, pte, device);
        return DRV_ERROR_INVALID_VALUE;
    }
#endif
    in_para.devid = device;
    in_para.addr_type = ADDR_MAP_TYPE_L2_BUFF;

    ret = devmm_ctrl_map_addr((void *)&in_para, in_size, (void *)&out_para, &out_size);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Map l2buff failed. (ret=%d)\n", ret);
        return ret;
    }

    *l2buff = (void *)(uintptr_t)out_para.ptr;
    DEVMM_DRV_DEBUG_ARG("Argument. (device=%d; *l2buff=%p)\n", device, *l2buff);

    return DRV_ERROR_NONE;
}

/* Sunset interface: external statement has been removed from ascend_hal.h. */
DVresult drvMemReleaseL2buffAddr(DVdevice device, void *l2buff)
{
    uint32_t addr_type = ADDR_MAP_TYPE_L2_BUFF;
    struct AddrUnmapInPara in_para;
    uint64_t p, size;
    DVresult ret;
#ifndef DEVMM_UT
    if (device >= DEVMM_MAX_PHY_DEVICE_NUM) {
        DEVMM_DRV_ERR("Device id error. (l2buff=%p; device=%u)\n", l2buff, device);
        return DRV_ERROR_INVALID_VALUE;
    }
#endif
    devmm_get_dev_reserve_addr(device, addr_type, &p, &size);
    in_para.devid = device;
    in_para.addr_type = addr_type;
    in_para.ptr = (UINT64)(uintptr_t)l2buff;
    in_para.len = size;
    ret = devmm_ctrl_unmap_addr((void *)&in_para, sizeof(struct AddrUnmapInPara), NULL, NULL);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Unmap l2buff failed. (ret=%d)\n", ret);
        return ret;
    }
    DEVMM_DRV_DEBUG_ARG("Argument. (device=%d; l2buff=%p; p=0x%llx; size=%llu)\n", device, l2buff, p, size);

    return DRV_ERROR_NONE;
}

static DVresult devmm_shmem_destroy_handle(const char *name, size_t str_len)
{
    struct devmm_ioctl_arg arg = {0};

    if (memcpy_s(arg.data.ipc_destroy_para.name, sizeof(arg.data.ipc_destroy_para.name), name, str_len) != 0) {
#ifndef EMU_ST
        DEVMM_DRV_ERR("Memcpy_s error. (str_len=%lu)\n", str_len);
        return DRV_ERROR_INVALID_VALUE;
#endif
    }
    return devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_IPC_MEM_DESTROY, &arg);
}

DVresult halShmemCreateHandle(DVdeviceptr vptr, size_t byte_count, char *name, uint32_t name_len)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    if ((name == NULL) || (name_len < DEVMM_MAX_NAME_SIZE)) {
#ifndef EMU_ST
        DEVMM_DRV_ERR("Invalid input. (name_len=%u; minimum_len=%u; vptr=0x%llx)\n", name_len, DEVMM_MAX_NAME_SIZE,
                      vptr);
#endif
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_is_vmm_mix_map(vptr, byte_count)) {
        DEVMM_DRV_ERR("Vmm mix map is invalid. (vptr=0x%llx; size=%lu)\n", vptr, byte_count);
        return DRV_ERROR_INVALID_VALUE;
    }

    DEVMM_DRV_DEBUG_ARG("Create share memory. (vptr=0x%llx; size=%lu)\n", vptr, byte_count);
    arg.data.ipc_create_para.vptr = vptr;
    arg.data.ipc_create_para.len = byte_count;
    arg.data.ipc_create_para.name_len = DEVMM_MAX_NAME_SIZE;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_IPC_MEM_CREATE, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Share memory create device error. (ret=%d; vptr=0x%llx)\n", ret, vptr);
        return ret;
    }

    if (memcpy_s(name, DEVMM_MAX_NAME_SIZE, arg.data.ipc_create_para.name, DEVMM_MAX_NAME_SIZE) != 0) {
        DEVMM_DRV_ERR("Memcpy_s error. (name_len=%u; vptr=0x%llx)\n", name_len, vptr);
        (void)devmm_shmem_destroy_handle(arg.data.ipc_create_para.name, DEVMM_MAX_NAME_SIZE);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static DVresult halShmemSetPid(const char *name, uint32_t sdid, int pid[], int num, unsigned long cmd)
{
    struct devmm_ioctl_arg arg = {0};
    uint64_t str_len;
    DVresult ret;
    int i;

    /* IpcCreate takes one, DEVMM_SVM_MAX_PROCESS_NUM - 1 left */
    if ((pid == NULL) || (num > (DEVMM_SECONDARY_PROCESS_NUM - 1)) || (num <= 0)) {
        DEVMM_DRV_ERR("Invalid input. (pid_null=%d; num=%d)\n", (pid == NULL), num);
        return DRV_ERROR_INVALID_VALUE;
    }

    str_len = (name != NULL) ? strnlen(name, DEVMM_MAX_NAME_SIZE) : 0;
    if ((str_len == 0) || (str_len >= DEVMM_MAX_NAME_SIZE)) {
        DEVMM_DRV_ERR("Invalid input. (str_len=%lu)\n", str_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (memcpy_s(arg.data.ipc_set_pid_para.name, sizeof(arg.data.ipc_set_pid_para.name), name, str_len) != 0) {
        DEVMM_DRV_ERR("Memcpy_s error. (str_len=%lu)\n", str_len);
        return DRV_ERROR_INVALID_VALUE;
    }
    arg.data.ipc_set_pid_para.sdid = sdid;
    arg.data.ipc_set_pid_para.num = (uint32_t)num;
    for (i = 0; i < num; i++) {
        DEVMM_DRV_DEBUG_ARG("Shmmem_set_pid. (pid=%d; idx=%d)\n", pid[i], i);
        arg.data.ipc_set_pid_para.set_pid[i] = pid[i];
    }
    ret = devmm_svm_ioctl(g_devmm_mem_dev, cmd, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Shmmem_set_pid error. (ret=%d; num=%d; pid=%d)\n", ret, num, pid[0]);
        return ret;
    }

    return DRV_ERROR_NONE;
}

DVresult halShmemSetPidHandle(const char *name, int pid[], int num)
{
    return halShmemSetPid(name, UINT32_MAX, pid, num, DEVMM_SVM_IPC_MEM_SET_PID);
}

DVresult halShmemSetPodPid(const char *name, uint32_t sdid, int pid[], int num)
{
    return halShmemSetPid(name, sdid, pid, num, DEVMM_SVM_IPC_MEM_SET_PID_POD);
}

DVresult halShmemDestroyHandle(const char *name)
{
    size_t str_len;
    DVresult ret;

    str_len = (name != NULL) ? strnlen(name, DEVMM_MAX_NAME_SIZE) : 0;
    if ((str_len == 0) || (str_len >= DEVMM_MAX_NAME_SIZE)) {
        DEVMM_DRV_ERR("Invalid input. (str_len=%lu)\n", str_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = devmm_shmem_destroy_handle(name, str_len);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Destroy share memory error. (ret=%d)\n", (int)ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC DVresult devmm_ipc_mem_query(struct devmm_ioctl_arg *arg, const char *name, size_t str_len)
{
    DVresult ret;

    if (memcpy_s(arg->data.query_size_para.name, sizeof(arg->data.query_size_para.name), name, str_len) != 0) {
        DEVMM_DRV_ERR("Memcpy_s error. (str_len=%lu)\n", str_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg->data.query_size_para.len = 0;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_IPC_MEM_QUERY, arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Memory query error. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

void devmm_set_module_id_to_advise(uint32_t model_id, DVmem_advise *advise)
{
    *advise = *advise | ((model_id & DV_ADVISE_MODULE_ID_MASK) << DV_ADVISE_MODULE_ID_BIT);
}

STATIC DVresult devmm_ipc_mem_open(struct devmm_ioctl_arg *arg, DVdeviceptr *ptr, const char *name, size_t str_len,
                                   uint32_t devid)
{
    DVmem_advise advise = 0;
    DVresult ret;

    advise |= (arg->data.query_size_para.is_huge != 0) ? DV_ADVISE_HUGEPAGE : 0;
    devmm_set_module_id_to_advise(HCCL, &advise);
    ret = devmm_alloc_proc(0, SUB_SVM_TYPE, advise, arg->data.query_size_para.len, ptr);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Alloc memory failed. (len=%lu; is_huge=%d; ptr=0x%llx)\n", arg->data.query_size_para.len,
                      arg->data.query_size_para.is_huge, *ptr);
        return ret;
    }

    if (memcpy_s(arg->data.ipc_open_para.name, sizeof(arg->data.ipc_open_para.name), name, str_len) != 0) {
        DEVMM_DRV_ERR("Memcpy_s error. (str_len=%lu; ptr=0x%llx)\n", str_len, *ptr);
        (void)devmm_free_managed(*ptr);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg->head.devid = devid;
    arg->data.ipc_open_para.vptr = *ptr;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_IPC_MEM_OPEN, arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Open memory error. (ret=%d; *ptr=0x%llx)\n", ret, *ptr);
        (void)devmm_free_managed(*ptr);
        return ret;
    }

    return DRV_ERROR_NONE;
}

#define DEVMM_SERVER_ID_IN_SDID_OFFSET 22
#define DEVMM_SERVER_ID_IN_KEY_OFFSET 54
#define DEVMM_REF_MASK 0x3FFFFFFFFFFFFF
/* key: server_id(10 bit) + ref(54 bit) */
static uint64_t devmm_ipc_name_to_key(const char *name)
{
    uint32_t devid, vfid, sdid, server_id;
    uint64_t ref, key;
    int pid, ret;

    ret = sscanf_s(name, "%08x%016llx%02x%02x%08x", &pid, &ref, &devid, &vfid, &sdid);
    if (ret != 5) { /* 5 for para nums */
        return DEVMM_RECORD_KEY_INVALID;
    }
    server_id = (sdid >> DEVMM_SERVER_ID_IN_SDID_OFFSET);
    key = (((uint64_t)server_id << DEVMM_SERVER_ID_IN_KEY_OFFSET) | (ref & DEVMM_REF_MASK));
    return key;
}

DVresult halShmemOpenHandleByDevId(DVdevice dev_id, const char *name, DVdeviceptr *vptr)
{
    struct devmm_ioctl_arg arg = {0};
    DVdeviceptr ptr = (DVdeviceptr)0;
    struct devmm_record_data data = {0};
    size_t str_len, mem_len;
    DVresult ret;

    str_len = (name != NULL) ? strnlen(name, DEVMM_MAX_NAME_SIZE) : 0;
    if ((str_len == 0) || (str_len >= DEVMM_MAX_NAME_SIZE) || (vptr == NULL)) {
        DEVMM_DRV_ERR("Invalid input. (str_len=%lu; vptr=%p)\n", str_len, vptr);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dev_id >= DEVMM_MAX_DEVICE_NUM) {
#ifndef EMU_ST
        DEVMM_DRV_ERR("Invalid devid. (devid=%u)\n", dev_id);
#endif
        return DRV_ERROR_INVALID_VALUE;
    }

    data.key1 = devmm_ipc_name_to_key(name);
    data.key2 = DEVMM_RECORD_KEY_INVALID;
    ret = devmm_record_create_and_get(DEVMM_FEATURE_IPC, dev_id, DEVMM_KEY_TYPE1, DEVMM_NODE_INITING, &data);
    if (ret != DRV_ERROR_TRY_AGAIN) {
        if (ret == DRV_ERROR_NONE) {
            *vptr = data.key2;
        }
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NONE), "Record get failed. (devId=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = devmm_ipc_mem_query(&arg, name, str_len);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        (void)devmm_record_put(DEVMM_FEATURE_IPC, dev_id, DEVMM_KEY_TYPE1, data.key1, DEVMM_NODE_UNINITED);
#endif
        DEVMM_DRV_ERR("Memory query error. (ret=%d)\n", ret);
        return ret;
    }
    mem_len = arg.data.query_size_para.len;
    ret = devmm_ipc_mem_open(&arg, &ptr, name, str_len, dev_id);
    if (ret != DRV_ERROR_NONE) {
        (void)devmm_record_put(DEVMM_FEATURE_IPC, dev_id, DEVMM_KEY_TYPE1, data.key1, DEVMM_NODE_UNINITED);
        DEVMM_DRV_ERR("Alloc ipc memory and set name failed. (ret=%d)\n", ret);
        return ret;
    }
    /* In the same OS, the service does not invoke the prefetch. */
    ret = drvMemPrefetchToDevice(ptr, mem_len, dev_id);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        (void)halShmemCloseHandle(ptr);
        (void)devmm_record_put(DEVMM_FEATURE_IPC, dev_id, DEVMM_KEY_TYPE1, data.key1, DEVMM_NODE_UNINITED);
        DEVMM_DRV_ERR("Prefetch failed. (devid=%d; ret=%d)\n", dev_id, ret);
#endif
        return ret;
    }

    data.key2 = ptr;
    (void)devmm_record_create_and_get(DEVMM_FEATURE_IPC, dev_id, DEVMM_KEY_TYPE1, DEVMM_NODE_INITED, &data);
    *vptr = ptr;
    DEVMM_DRV_DEBUG_ARG("ShmemOpenHandle succeeded. (devid=%u; open_ptr=0x%llx)\n", dev_id, ptr);

    return DRV_ERROR_NONE;
}

DVresult halShmemOpenHandle(const char *name, DVdeviceptr *vptr)
{
    return halShmemOpenHandleByDevId(svm_devid, name, vptr);
}

DVresult halShmemCloseHandle(DVdeviceptr vptr)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    DEVMM_DRV_DEBUG_ARG("ShmemCloseHandle. (vptr=0x%llx)\n", vptr);

    ret = devmm_record_put(DEVMM_FEATURE_IPC, DEVMM_RECORD_INVALID_DEVID, DEVMM_KEY_TYPE2, vptr, DEVMM_NODE_UNINITING);
    if (ret != DRV_ERROR_NONE) {
        return (ret == DRV_ERROR_INNER_ERR) ? ret : DRV_ERROR_NONE;
    }

    arg.data.ipc_close_para.vptr = vptr;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_IPC_MEM_CLOSE, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Close error. (vptr=0x%llx)\n", vptr);
        return ret;
    }

    ret = devmm_free_managed(vptr);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_WARN("Free memory failed. (vptr=0x%llx)\n", vptr);
    }

    ret = devmm_record_put(DEVMM_FEATURE_IPC, DEVMM_RECORD_INVALID_DEVID, DEVMM_KEY_TYPE2, vptr, DEVMM_NODE_UNINITED);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return DRV_ERROR_NONE;
}

DVresult halShmemSetAttribute(const char *name, uint32_t type, uint64_t attr)
{
    struct devmm_ioctl_arg arg = {0};
    size_t str_len;
    DVresult ret;

    str_len = (name != NULL) ? strnlen(name, DEVMM_MAX_NAME_SIZE) : 0;
    if ((str_len == 0) || (str_len >= DEVMM_MAX_NAME_SIZE)) {
        DEVMM_DRV_ERR("Invalid name. (str_len=%zu)\n", str_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (type >= SHMEM_ATTR_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (memcpy_s(arg.data.ipc_set_attr_para.name, sizeof(arg.data.ipc_set_attr_para.name), name, str_len) != 0) {
        DEVMM_DRV_ERR("Memcpy_s error. (str_len=%zu)\n", str_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.data.ipc_set_attr_para.type = type;
    arg.data.ipc_set_attr_para.attr = attr;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_IPC_MEM_SET_ATTR, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT), "Shmem set attribute failed. (ret=%d)\n", ret);
        return ret;
    }

    DEVMM_DRV_DEBUG_ARG("Shmem set attribute successfully. (name=%s; type=%u; attr=%llu)\n", name, type, attr);

    return DRV_ERROR_NONE;
}

DVresult halShmemGetAttribute(const char *name, enum ShmemAttrType type, uint64_t *attr)
{
    struct devmm_ioctl_arg arg = {0};
    size_t str_len;
    DVresult ret;

    str_len = (name != NULL) ? strnlen(name, DEVMM_MAX_NAME_SIZE) : 0;
    if ((str_len == 0) || (str_len >= DEVMM_MAX_NAME_SIZE) || (attr == NULL)) {
        DEVMM_DRV_ERR("Invalid para. (str_len=%zu; attr_is_null=%u)\n", str_len, (attr == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((u32)type >= SHMEM_ATTR_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (memcpy_s(arg.data.ipc_set_attr_para.name, sizeof(arg.data.ipc_set_attr_para.name), name, str_len) != 0) {
        DEVMM_DRV_ERR("Memcpy_s error. (str_len=%zu)\n", str_len);
        return DRV_ERROR_INVALID_VALUE;
    }
    arg.data.ipc_set_attr_para.type = type;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_IPC_MEM_GET_ATTR, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT), "Shmem get attribute failed. (ret=%d; type=%u)\n", ret, type);
        return ret;
    }
    *attr = arg.data.ipc_set_attr_para.attr;

    DEVMM_DRV_DEBUG_ARG("Shmem get attribute successfully. (name=%s; type=%u; attr=%llu)\n", name, type, *attr);

    return DRV_ERROR_NONE;
}

DVresult halShmemInfoGet(const char *name, struct ShmemGetInfo *info)
{
    struct devmm_ioctl_arg arg = {0};
    size_t str_len;
    DVresult ret;

    str_len = (name != NULL) ? strnlen(name, DEVMM_MAX_NAME_SIZE) : 0;
    if ((str_len == 0) || (str_len >= DEVMM_MAX_NAME_SIZE) || (info == NULL)) {
        DEVMM_DRV_ERR("Invalid para. (str_len=%zu; info_is_null=%u)\n", str_len, (info == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = devmm_ipc_mem_query(&arg, name, str_len);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Memory query error. (ret=%d; name=%s)\n", ret, name);
        return ret;
    }
    info->phyDevid = arg.data.query_size_para.phy_devid;
    DEVMM_DRV_DEBUG_ARG("Shmem get info successfully. (name=%s; phyDevid=%u)\n", name, info->phyDevid);

    return DRV_ERROR_NONE;
}

STATIC DVresult devmm_get_attribute(DVdeviceptr vptr, struct DVattribute *attr, struct devmm_virt_com_heap *heap,
                                    struct devmm_virt_heap_mgmt *p_heap_mgmt)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.data.status_va_info_para.va = vptr;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_DBG_VA_STATUS, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Ioctl device error. (ret=%d)\n", ret);
        return ret;
    }
    attr->memType = arg.data.status_va_info_para.mem_type;
    attr->devId = arg.data.status_va_info_para.devid;
    attr->pageSize = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? p_heap_mgmt->huge_page_size :
                                                                 p_heap_mgmt->svm_page_size;
    return DRV_ERROR_NONE;
}

bool svm_support_get_user_malloc_attr(uint32_t dev_id)
{
    (void)dev_id;
    g_get_user_malloc_attr = true;
    return true;
}

bool svm_support_mem_register_query_and_get_attr(uint32_t dev_id)
{
    (void)dev_id;
    if (g_register_query_and_get_attr == false) {
        g_register_query_and_get_attr = true;
        DEVMM_RUN_INFO("Support query and get attr of mem register.\n");
    }

    return true;
}

bool svm_support_mem_register_host_pinned(uint32_t dev_id)
{
    return (dev_id < DEVMM_MAX_LOGIC_DEVICE_NUM);
}

STATIC INLINE DVresult devmm_get_host_memory_attribute(struct devmm_virt_com_heap *heap,
                                                       struct devmm_virt_heap_mgmt *p_heap_mgmt,
                                                       struct DVattribute *attr, DVdeviceptr vptr)
{
    uint32_t host_devid = SVM_MAX_AGENT_NUM;

    if ((heap == NULL) || (heap->heap_type == DEVMM_HEAP_PINNED_HOST)) {
        if (heap == NULL) {
            bool user_register = svm_mem_register_record_is_exist(vptr, HOST_MEM_REGISTER);
            attr->memType = g_get_user_malloc_attr ? DV_MEM_USER_MALLOC : DV_MEM_LOCK_HOST;
            attr->memType = (user_register && g_register_query_and_get_attr) ? DV_MEM_USER_REGISTER : attr->memType;
        } else {
            attr->memType = DV_MEM_LOCK_HOST;
        }
    }

    if ((heap != NULL) && (heap->heap_list_type == HOST_AGENT_LIST)) {
        attr->memType = DV_MEM_LOCK_HOST_AGENT;
    }
    (void)halGetHostID(&host_devid);
    attr->devId = host_devid;
    attr->pageSize = p_heap_mgmt->local_page_size;
    return DRV_ERROR_NONE;
}

STATIC INLINE bool devmm_heap_is_host_attribute(struct devmm_virt_com_heap *heap)
{
    if ((heap == NULL) || (heap->heap_type == DEVMM_HEAP_PINNED_HOST) || (heap->heap_list_type == HOST_AGENT_LIST)) {
        return true;
    }
    return false;
}

static int devmm_get_dyn_reserve_addr_attr(struct devmm_virt_heap_mgmt *p_heap_mgmt, uint64_t va,
                                           struct DVattribute *attr)
{
    struct devmm_mem_info mem_info;
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    int ret;

    (void)halGetHostID(&host_id);

    ret = devmm_get_reserve_addr_info(va, &mem_info);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Get add info failed. (va=0x%llx)\n", va);
        return ret;
    }

    attr->pageSize = p_heap_mgmt->huge_page_size;
    attr->devId = mem_info.devid;
    attr->memType = (mem_info.devid == host_id) ? DV_MEM_LOCK_HOST : DV_MEM_LOCK_DEV;
    return DRV_ERROR_NONE;
}

static int devmm_get_reserve_addr_attr_from_map_info(struct devmm_virt_heap_mgmt *p_heap_mgmt, uint64_t va,
                                                     struct DVattribute *attr, uint32_t *side, uint32_t *devid)
{
    struct svm_map_info info = {0};
    int ret;

    ret = svm_query_map_info(va, &info);
    if (ret != DRV_ERROR_NONE) {
        if (svm_is_dyn_addr(va) == false) {
            return DRV_ERROR_NOT_EXIST;
        }
        return devmm_get_dyn_reserve_addr_attr(p_heap_mgmt, va, attr);
    }

    attr->pageSize = p_heap_mgmt->huge_page_size;
    attr->devId = info.devid;
    attr->memType = (info.side == MEM_HOST_SIDE) ? DV_MEM_LOCK_HOST : DV_MEM_LOCK_DEV;
    *side = info.side;
    *devid = info.devid;
    return DRV_ERROR_NONE;
}

static int devmm_get_reserve_addr_attr(struct devmm_virt_heap_mgmt *p_heap_mgmt, struct devmm_virt_com_heap *heap,
                                       uint64_t va, struct DVattribute *attr)
{
    (void)heap;
    uint32_t side, devid;
    int ret;

    ret = devmm_get_reserve_addr_attr_from_map_info(p_heap_mgmt, va, attr, &side, &devid);
    if (ret != 0) {
        if (devmm_virt_heap_is_primary(heap)) { /* For vmm va not map scene. */
            attr->devId = heap->devid;
            attr->pageSize = p_heap_mgmt->huge_page_size;
            attr->memType = DV_MEM_LOCK_HOST;
            return DRV_ERROR_NONE;
        }
        DEVMM_DRV_ERR("Va is invalid. (va=0x%llx; ret=%d)\n", va, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((side != MEM_HOST_SIDE) && (side != MEM_DEV_SIDE)) {
        DEVMM_DRV_ERR("Addr hasn't been mapped. (va=0x%llx)\n", va);
        return DRV_ERROR_INVALID_HANDLE;
    }

    attr->devId = devid;
    if (side == MEM_HOST_SIDE) {
        attr->pageSize = (heap->heap_type == DEVMM_HEAP_CHUNK_PAGE) ? p_heap_mgmt->local_page_size :
                                                                      p_heap_mgmt->huge_page_size;
        attr->memType = DV_MEM_LOCK_HOST;
    } else {
        attr->pageSize = (heap->heap_type == DEVMM_HEAP_CHUNK_PAGE) ? DEVMM_DEV_PAGE_SIZE : p_heap_mgmt->huge_page_size;
        attr->memType = DV_MEM_LOCK_DEV;
    }
    return DRV_ERROR_NONE;
}

static DVresult devmm_get_heap_addr_attr(DVdeviceptr vptr, struct DVattribute *attr,
                                         struct devmm_virt_heap_mgmt *p_heap_mgmt, struct devmm_virt_com_heap *heap)
{
#ifndef UVM_OPEN
    if ((heap != NULL) && (devmm_va_is_in_uvm_range(vptr))) {
        attr->memType = DV_MEM_UVM;
        attr->pageSize = p_heap_mgmt->uvm_page_size;
        return DRV_ERROR_NONE;
    }
#endif
    if ((heap != NULL) && devmm_va_is_in_soma_range(vptr)) {
        attr->memType = DV_MEM_SOMA;
        attr->pageSize = p_heap_mgmt->huge_page_size;
        return DRV_ERROR_NONE;
    } else if ((heap == NULL) &&
               (devmm_va_is_in_svm_range(vptr) || devmm_va_is_in_soma_range(vptr) || devmm_va_is_in_uvm_range(vptr))) {
        DEVMM_DRV_ERR("Addr isn't allocated. (va=0x%llx)\n", vptr);
        return DRV_ERROR_NOT_EXIST;
    }

    if (devmm_heap_is_host_attribute(heap) == true) {
        return devmm_get_host_memory_attribute(heap, p_heap_mgmt, attr, vptr);
    }
    if (heap->heap_sub_type == SUB_SVM_TYPE) {
        return devmm_get_attribute(vptr, attr, heap, p_heap_mgmt);
    } else if (heap->heap_sub_type == SUB_DVPP_TYPE) {
        attr->memType = DV_MEM_LOCK_DEV_DVPP;
    } else if (heap->heap_sub_type == SUB_RESERVE_TYPE) {
        return devmm_get_reserve_addr_attr(p_heap_mgmt, heap, vptr, attr);
    } else {
        attr->memType = DV_MEM_LOCK_DEV;
    }

    attr->devId = devmm_heap_device_by_list_type(heap->heap_list_type);
    attr->pageSize = (heap->heap_type == DEVMM_HEAP_HUGE_PAGE) ? p_heap_mgmt->huge_page_size :
                                                                 p_heap_mgmt->svm_page_size;

    return DRV_ERROR_NONE;
}

static DVresult devmm_mem_get_attribute(DVdeviceptr vptr, struct DVattribute *attr)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;
    struct devmm_virt_com_heap *heap = NULL;
    uint32_t side, devid;
    int ret;

    if ((vptr == 0) || (attr == NULL)) {
        DEVMM_DRV_ERR("Vptr or attr is NULL. (vptr=0x%llx)\n", vptr);
        return DRV_ERROR_INVALID_VALUE;
    }

    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        DEVMM_DRV_ERR("Please call device_open and alloc_mem api first. (alloc_mem=0x%llx)\n", vptr);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (vptr >= DEVMM_MAX_DYN_ALLOC_BASE) {
        ret = devmm_get_reserve_addr_attr_from_map_info(p_heap_mgmt, vptr, attr, &side, &devid);
        if (ret != DRV_ERROR_NOT_EXIST) {
            return ret;
        }
    }

    heap = devmm_va_to_heap((virt_addr_t)vptr);
    return devmm_get_heap_addr_attr(vptr, attr, p_heap_mgmt, heap);
}

DVresult drvMemGetAttribute(DVdeviceptr vptr, struct DVattribute *attr)
{
    uint32_t host_devid;
    DVresult ret;

    ret = devmm_mem_get_attribute(vptr, attr);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if ((halGetHostID(&host_devid) == DRV_ERROR_NONE) && (attr->devId == host_devid)) {
        attr->devId = 0;
    }
    return DRV_ERROR_NONE;
}

static DVresult devmm_get_mem_size_info(uint32_t devid, unsigned int type, struct MemInfo *info)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        DEVMM_DRV_ERR("Heap_mgmt wasn't inited, please call device open api first. (device=%u)\n", devid);
        return DRV_ERROR_INVALID_HANDLE;
    }

    arg.head.devid = devid;
    arg.data.query_device_mem_usedinfo_para.mem_type = type;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_QUERY_MEM_USEDINFO, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Ioctl error. (device=%u; arg_head_devid=%u; type=%x; ret=%d)\n", devid, arg.head.devid, type,
                      ret);
        return ret;
    }

    info->phy_info.total = arg.data.query_device_mem_usedinfo_para.normal_total_size;
    info->phy_info.free = arg.data.query_device_mem_usedinfo_para.normal_free_size;
    info->phy_info.huge_total = arg.data.query_device_mem_usedinfo_para.huge_total_size;
    info->phy_info.huge_free = arg.data.query_device_mem_usedinfo_para.huge_free_size;
    info->phy_info.giant_total = arg.data.query_device_mem_usedinfo_para.giant_total_size;
    info->phy_info.giant_free = arg.data.query_device_mem_usedinfo_para.giant_free_size;

    DEVMM_DRV_DEBUG_ARG("Argument. (ram free=%lu; total=%lu; huge free=%lu; huge total=%lu; "
                        "giant free=%lu; giant total=%lu)\n",
                        info->phy_info.free, info->phy_info.total, info->phy_info.huge_free, info->phy_info.huge_total,
                        info->phy_info.giant_free, info->phy_info.giant_total);

    return DRV_ERROR_NONE;
}

static DVresult devmm_get_mem_check_info_para_check(struct MemAddrInfo *info)
{
    if (info->addr == NULL) {
        DEVMM_DRV_ERR("Info addr para is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((info->cnt == 0) || (info->cnt > DEVMM_DEV_ADDR_NUM_MAX)) {
        DEVMM_DRV_ERR("Cnt out of range. (cnt=%u; range=[1, %u])\n", info->cnt, (uint32_t)DEVMM_DEV_ADDR_NUM_MAX);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((info->mem_type & MEM_SVM_TYPE) || (info->mem_type & MEM_HOST_TYPE) || (info->mem_type & MEM_HOST_AGENT_TYPE)) {
        DEVMM_RUN_INFO("Mem_type not support. (mem_type=%u)\n", info->mem_type);
        return DRV_ERROR_NOT_SUPPORT;
    }
    return DRV_ERROR_NONE;
}

static DVresult devmm_get_mem_check_info(uint32_t devid, uint32_t type, struct MemInfo *info)
{
    (void)type;
    struct MemAddrInfo *addr_info = &info->addr_info;
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    ret = devmm_get_mem_check_info_para_check(addr_info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    arg.head.devid = devid;
    arg.data.check_meminfo_para.va = (uint64_t *)addr_info->addr;
    arg.data.check_meminfo_para.cnt = addr_info->cnt;
    arg.data.check_meminfo_para.heap_subtype_mask = devmm_memtype_mask_to_heap_subtype_mask(addr_info->mem_type);
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_CHECK_MEMINFO, &arg);
    if (ret != DRV_ERROR_NONE) {
        addr_info->flag = false;
        return ret;
    }

    addr_info->flag = true;
    return DRV_ERROR_NONE;
}

static DVresult (*devmm_mem_get_info[MEM_INFO_TYPE_MAX])(uint32_t devid, unsigned int type, struct MemInfo *info) = {
    [MEM_INFO_TYPE_DDR_SIZE] = devmm_get_mem_size_info,     [MEM_INFO_TYPE_HBM_SIZE] = devmm_get_mem_size_info,
    [MEM_INFO_TYPE_DDR_P2P_SIZE] = devmm_get_mem_size_info, [MEM_INFO_TYPE_HBM_P2P_SIZE] = devmm_get_mem_size_info,
    [MEM_INFO_TYPE_ADDR_CHECK] = devmm_get_mem_check_info,
};

static DVresult devmm_mem_get_info_para_check(DVdevice devid, unsigned int type, struct MemInfo *info)
{
    /* host agent not support get meminfo */
    if (devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
        DEVMM_DRV_ERR("Devid out of range. (devid=%u; range=[0, %u))\n", devid, (uint32_t)DEVMM_MAX_PHY_DEVICE_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (info == NULL) {
        DEVMM_DRV_ERR("Info para is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (type >= MEM_INFO_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (devmm_mem_get_info[type] == NULL) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

DVresult halMemGetInfo(DVdevice device, unsigned int type, struct MemInfo *info)
{
    DVresult ret;

    DEVMM_DRV_DEBUG_ARG("Enter. (device=%u; type=0x%x)\n", device, type);

    ret = devmm_mem_get_info_para_check(device, type, info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return devmm_mem_get_info[type](device, type, info);
}

static bool devmm_support_pcie_bar_feature(struct devmm_virt_heap_mgmt *p_heap_mgmt, uint32_t devid)
{
    return p_heap_mgmt->support_bar_mem[devid];
}

static bool devmm_support_dev_mem_register_feature(struct devmm_virt_heap_mgmt *p_heap_mgmt, uint32_t devid)
{
    return p_heap_mgmt->support_dev_mem_map_host[devid];
}

static bool devmm_support_pcie_bar_huge_feature(struct devmm_virt_heap_mgmt *p_heap_mgmt, uint32_t devid)
{
    return p_heap_mgmt->support_bar_huge_mem[devid];
}

static uint64_t devmm_get_double_pgtable_offset(struct devmm_virt_heap_mgmt *p_heap_mgmt, uint32_t devid)
{
    return p_heap_mgmt->double_pgtable_offset[devid];
}

static bool devmm_support_agent_giant_page_feature(struct devmm_virt_heap_mgmt *p_heap_mgmt, uint32_t devid)
{
    return p_heap_mgmt->support_agent_giant_page[devid];
}

static bool devmm_support_shmem_map_exbus_feature(struct devmm_virt_heap_mgmt *p_heap_mgmt, uint32_t devid)
{
    return p_heap_mgmt->support_shmem_map_exbus[devid];
}

#ifndef EMU_ST
bool svm_support_mem_host_uva(uint32_t dev_id)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;

    if (dev_id >= DEVMM_MAX_PHY_DEVICE_NUM) {
        return false;
    }
    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        return false;
    }

    if (devmm_is_host_pin_memory_map_failed()) {
        return false;
    }
    return p_heap_mgmt->support_mem_host_uva[dev_id];
}
#endif

typedef bool (*devmm_is_feature_support)(struct devmm_virt_heap_mgmt *p_heap_mgmt, uint32_t devid);

#define SUPPORT_FEATURE_MAX_NUM 6 /* svm cannot add new feature here, use halSupportFeature */
static void devmm_get_support_feature(uint32_t devid, unsigned long long in_support_feature,
                                      unsigned long long *out_support_feature)
{
    static const devmm_is_feature_support is_feature_support[SUPPORT_FEATURE_MAX_NUM] = {
        [CTRL_SUPPORT_PCIE_BAR_MEM_BIT] = devmm_support_pcie_bar_feature,
        [CTRL_SUPPORT_DEV_MEM_REGISTER_BIT] = devmm_support_dev_mem_register_feature,
        [CTRL_SUPPORT_PCIE_BAR_HUGE_MEM_BIT] = devmm_support_pcie_bar_huge_feature,
        [CTRL_SUPPORT_GIANT_PAGE_BIT] = devmm_support_agent_giant_page_feature,
        [CTRL_SUPPORT_SHMEM_MAP_EXBUS_BIT] = devmm_support_shmem_map_exbus_feature,
    };
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;
    uint32_t i;

    if (devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
        return;
    }
    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        return;
    }

    for (i = CTRL_SUPPORT_PCIE_BAR_MEM_BIT; i < SUPPORT_FEATURE_MAX_NUM; i++) {
        uint64_t feature_mask = (1ULL << i);
        if ((in_support_feature & feature_mask) != 0) {
            if (is_feature_support[i](p_heap_mgmt, devid)) {
                *out_support_feature |= feature_mask;
            }
        }
    }
}

static THREAD bool g_support_numa_ts = false;
static DVresult devmm_ctrl_support_feature(void *param_value, size_t param_value_size, void *out_value,
                                           size_t *out_size_ret)
{
    (void)param_value_size;
    (void)out_size_ret;
    struct supportFeaturePara *in_para = (struct supportFeaturePara *)param_value;
    struct supportFeaturePara *out_para = (struct supportFeaturePara *)out_value;

    if ((in_para == NULL) || (out_para == NULL)) {
        DEVMM_DRV_ERR("Check in_para out_para is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    out_para->support_feature = 0;
    if ((in_para->support_feature & CTRL_SUPPORT_NUMA_TS_MASK) != 0) {
        g_support_numa_ts = 1;
        out_para->support_feature |= CTRL_SUPPORT_NUMA_TS_MASK;
    }

    devmm_get_support_feature(in_para->devid, in_para->support_feature, &out_para->support_feature);

    DEVMM_DRV_INFO("RTS note drv support feature. (in_feature=0x%llx; out_feature=0x%llx)\n", in_para->support_feature,
                   out_para->support_feature);
    return DRV_ERROR_NONE;
}

static DVresult devmm_get_double_pgtable_offset_para_check(void *param_value, size_t param_value_size, void *out_value,
                                                           size_t *out_size_ret)
{
    if (param_value == NULL) {
        DEVMM_DRV_ERR("Param_value is NULL. \n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (out_value == NULL) {
        DEVMM_DRV_ERR("Out_value is NULL. \n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (out_size_ret == NULL) {
        DEVMM_DRV_ERR("Out_size_ret is NULL. \n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (param_value_size != sizeof(uint32_t)) {
        DEVMM_DRV_ERR("Param_value_size not equal sizeof(uint32_t). \n");
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

static DVresult devmm_ctrl_get_double_pgtable_offset(void *param_value, size_t param_value_size, void *out_value,
                                                     size_t *out_size_ret)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;
    uint32_t devid;
    DVresult ret;

    ret = devmm_get_double_pgtable_offset_para_check(param_value, param_value_size, out_value, out_size_ret);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Parameter is invalid. \n");
        return ret;
    }

    devid = *(uint32_t *)param_value;
    if (devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
        DEVMM_DRV_ERR("Devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }
#ifndef UVM_OPEN
    if (__sync_val_compare_and_swap(&g_uvm_heap_initialized, 0, 0) != 0) {
        *(uint64_t *)out_value = 0;
    } else {
        p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
        if (p_heap_mgmt == NULL) {
            return DRV_ERROR_INNER_ERR;
        }

        *(uint64_t *)out_value = devmm_get_double_pgtable_offset(p_heap_mgmt, devid);
    }
#else
    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        return DRV_ERROR_INNER_ERR;
    }

    *(uint64_t *)out_value = devmm_get_double_pgtable_offset(p_heap_mgmt, devid);
#endif
    *out_size_ret = sizeof(uint64_t);

    DEVMM_DRV_INFO("RTS note drv double pgtable offset. (offset=0x%llx)\n", *(uint64_t *)out_value);
    return DRV_ERROR_NONE;
}

static DVresult devmm_mem_repair_para_check(void *param_value, size_t param_value_size, void *out_value,
                                            size_t *out_size_ret)
{
    struct MemRepairInPara *in_para = NULL;

    if (param_value == NULL) {
        DEVMM_DRV_ERR("Param_value is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (param_value_size != sizeof(struct MemRepairInPara)) {
        DEVMM_DRV_ERR("Param_value_size is invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((out_value != NULL) || (out_size_ret != NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    in_para = (struct MemRepairInPara *)param_value;
    if ((in_para->devid >= DEVMM_MAX_PHY_DEVICE_NUM) || (in_para->count == 0) ||
        (in_para->count > MEM_REPAIR_MAX_CNT)) {
        DEVMM_DRV_ERR("InPara is invalid. (devid=%u; repair_cnt=%u)\n", in_para->devid, in_para->count);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static DVresult devmm_ctrl_mem_repair(void *para_value, size_t para_value_size, void *out_value, size_t *out_size_ret)
{
    struct MemRepairInPara *in_para = NULL;
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;
    u32 i;

    ret = devmm_mem_repair_para_check(para_value, para_value_size, out_value, out_size_ret);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    in_para = (struct MemRepairInPara *)para_value;
    arg.head.devid = in_para->devid;
    arg.data.mem_repair_para.count = in_para->count;
    (void)memcpy_s(arg.data.mem_repair_para.repair_addrs, MEM_REPAIR_MAX_CNT * sizeof(struct MemRepairAddr),
                   in_para->repairAddrs, in_para->count * sizeof(struct MemRepairAddr));
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_REPLAIR, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Mem repair failed. (devid=%u; repair_cnt=%u)\n", in_para->devid, in_para->count);
        return ret;
    }

    DEVMM_RUN_INFO("Mem repair succ. (devid=%u; repair_cnt=%u)\n", in_para->devid, in_para->count);
    for (i = 0; i < in_para->count; i++) {
        DEVMM_RUN_INFO("Repair info. (addr[%u]=0x%llx; size=0x%llx).\n", i, in_para->repairAddrs[i].ptr,
                       in_para->repairAddrs[i].len);
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_ctrl_get_addr_module_id(void *param_value, size_t param_value_size, void *out_value,
                                                size_t *out_size_ret)
{
    if ((param_value == NULL) || (out_value == NULL) || (out_size_ret == NULL)) {
        DEVMM_DRV_ERR("Input is invalid. (param_value=%u; out_value=%u; out_size_ret=%u)\n", (param_value != NULL),
                      (out_value != NULL), (out_size_ret != NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (param_value_size != sizeof(uint64_t)) {
        DEVMM_DRV_ERR("Param_value_size is invalid. (param_value_size=%lu; size=%lu)\n", param_value_size,
                      sizeof(uint64_t));
        return DRV_ERROR_INVALID_VALUE;
    }

    devmm_get_addr_module_id(*(uint64_t *)param_value, (uint32_t *)out_value, (uint64_t *)out_size_ret);
    return DRV_ERROR_NONE;
}

#define DEVMM_SPECIFIED_ALLOC_START_ADDR 0x140000000000ULL
#define DEVMM_SPECIFIED_ALLOC_SIZE 0X40000000000ULL
struct devmm_specified_addr_alloc_range {
    uint64_t start_addr;
    uint64_t size;
};

drvError_t devmm_get_specified_addr_alloc_range(void *param_value, size_t param_value_size, void *alloc_range_value,
                                                size_t *alloc_range_size)
{
    struct devmm_specified_addr_alloc_range *alloc_range = alloc_range_value;

    if ((param_value != NULL) || (param_value_size != 0)) {
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((alloc_range == NULL) || (alloc_range_size == NULL)) {
        DEVMM_DRV_ERR("Out para is invalid. (alloc_range=%u; alloc_range_size=%u)\n", (alloc_range != NULL),
                      (alloc_range_size != NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    alloc_range->start_addr = DEVMM_SPECIFIED_ALLOC_START_ADDR;
    alloc_range->size = DEVMM_SPECIFIED_ALLOC_SIZE;
    *alloc_range_size = sizeof(struct devmm_specified_addr_alloc_range);
    return DRV_ERROR_NONE;
}

static DVresult devmm_check_remote_mmap_feature(uint32_t devid)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;

    if (devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
        DEVMM_DRV_ERR("devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if ((p_heap_mgmt == NULL) || (p_heap_mgmt->is_dev_inited[devid] == false)) {
        DEVMM_DRV_ERR("devid is uninited. (devid=%u)\n", devid);
        return DRV_ERROR_UNINIT;
    }

    return p_heap_mgmt->support_remote_mmap[devid] ? DRV_ERROR_NONE : DRV_ERROR_NOT_SUPPORT;
}

static DVresult devmm_ctrl_cp_process_mmap(void *para_value, size_t para_value_size, void *out_value,
                                           size_t *out_size_ret)
{
    struct ProcessCpMmap *mmap_info = (struct ProcessCpMmap *)para_value;
    DVdeviceptr vptr;
    DVresult ret;

    if ((para_value == NULL) || (out_value == NULL) || (out_size_ret == NULL)) {
        DEVMM_DRV_ERR("Input para is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para_value_size != sizeof(struct ProcessCpMmap)) {
        DEVMM_DRV_ERR("Para_value_size is invalid. (value_size=%lu)\n", para_value_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mmap_info->flag != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (svm_is_dcache_addr(mmap_info->ptr, mmap_info->size) == false) {
        DEVMM_RUN_INFO("Not support not dcache addr. (va=0x%llx; size=0x%llx)\n", mmap_info->ptr, mmap_info->size);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_check_remote_mmap_feature(mmap_info->devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    vptr = mmap_info->ptr;
    ret = devmm_process_cp_mmap(mmap_info->devid, &vptr, mmap_info->size);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_INFO("Cp process mmap check. (ret=%d)\n", ret);
        return ret;
    }

    *(DVdeviceptr *)out_value = vptr;
    *out_size_ret = sizeof(DVdeviceptr);

    DEVMM_DRV_DEBUG_ARG("Mmap_info_ptr=0x%llx; vptr=0x%llx; size=0x%lx.\n", mmap_info->ptr, vptr, mmap_info->size);

    return DRV_ERROR_NONE;
}

static DVresult devmm_ctrl_cp_process_munmap(void *para_value, size_t para_value_size, void *out_value,
                                             size_t *out_size_ret)
{
    struct ProcessCpMunmap *munmap_info = (struct ProcessCpMunmap *)para_value;
    DVresult ret;

    if ((para_value == NULL) || (out_value != NULL) || (out_size_ret != NULL)) {
        DEVMM_DRV_ERR("Input param is invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (para_value_size != sizeof(struct ProcessCpMunmap)) {
        DEVMM_DRV_ERR("Para_value_size is invalid. (value_size=%lu)\n", para_value_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (munmap_info->size != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (svm_is_dcache_addr(munmap_info->ptr, 1) == false) {
        DEVMM_RUN_INFO("Not support not dcache addr. (va=0x%llx)\n", munmap_info->ptr);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_check_remote_mmap_feature(munmap_info->devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return devmm_process_cp_munmap(munmap_info->devid, munmap_info->ptr);
}

static DVresult devmm_get_dcache_addr(void *para_value, size_t para_value_size, void *out_value, size_t *out_size_ret)
{
    if ((para_value != NULL) || (para_value_size != 0) || (out_value == NULL) || (out_size_ret == NULL)) {
        DEVMM_DRV_ERR("Input param is invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    *(DVdeviceptr *)out_value = DEVMM_DCACHE_ADDR_START;
    *out_size_ret = sizeof(DVdeviceptr);

    DEVMM_DRV_DEBUG_ARG("dcache addr=0x%llx.\n", *(DVdeviceptr *)out_value);

    return DRV_ERROR_NONE;
}

static DVresult devmm_ctrl_pagefault_info_query(void *param, size_t param_size, void *out_param, size_t *out_size)
{
    struct devmm_ioctl_arg arg = {0};
    struct devmm_pagefault_info_ioctl_para *para = &arg.data.pagefault_info_para;
    DVresult ret;

    if ((param == NULL) || (param_size != sizeof(uint32_t)) || (out_param == NULL) || (out_size == NULL) ||
        (*out_size < sizeof(struct svmPagefaultInfo))) {
        DEVMM_DRV_ERR("Param is invalid. (param=%p; param_size=%lu; out_param=%p; out_size=%p; *out_size=%lu)\n", param,
                      param_size, out_param, out_size, out_size == NULL ? 0 : *out_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.head.devid = *(uint32_t *)param;
    if (arg.head.devid >= SVM_MAX_AGENT_NUM) {
        DEVMM_DRV_ERR("Devid is out of range. (devid=%u)\n", arg.head.devid);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (devmm_dev_is_inited(arg.head.devid) == false) {
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_PAGEFAULT_INFO_QUERY, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT), "Pagefault info query ioctl ret. (ret=%d)\n", ret);
        return ret;
    }

    if (para->has_record == 0) {
        return DRV_ERROR_NOT_EXIST;
    }

    *(struct svmPagefaultInfo *)out_param = para->info;
    *out_size = sizeof(struct svmPagefaultInfo);
    return DRV_ERROR_NONE;
}

static DVresult devmm_ctrl_pagefault_info_clear(void *param, size_t param_size, void *out_param, size_t *out_size)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    if ((param == NULL) || (param_size != sizeof(uint32_t)) || (out_param != NULL) || (out_size != NULL)) {
        DEVMM_DRV_ERR("Input param is invalid. (param=%p; param_size=%lu; out_param=%p; out_size=%p)\n", param,
                      param_size, out_param, out_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.head.devid = *(uint32_t *)param;
    if (arg.head.devid >= SVM_MAX_AGENT_NUM) {
        DEVMM_DRV_ERR("Devid is out of range. (devid=%u)\n", arg.head.devid);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (devmm_dev_is_inited(arg.head.devid) == false) {
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_PAGEFAULT_INFO_CLEAR, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT), "Pagefault info clear ioctl ret. (ret=%d)\n", ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

static DVresult (*devmm_mem_ctrl_handlers[CTRL_TYPE_MAX])(void *param, size_t param_size, void *out_param,
                                                          size_t *out_size) = {
    [CTRL_TYPE_ADDR_MAP] = devmm_ctrl_map_addr,
    [CTRL_TYPE_ADDR_UNMAP] = devmm_ctrl_unmap_addr,
    [CTRL_TYPE_SUPPORT_FEATURE] = devmm_ctrl_support_feature,
    [CTRL_TYPE_GET_DOUBLE_PGTABLE_OFFSET] = devmm_ctrl_get_double_pgtable_offset,
    [CTRL_TYPE_MEM_REPAIR] = devmm_ctrl_mem_repair,
    [CTRL_TYPE_GET_ADDR_MODULE_ID] = devmm_ctrl_get_addr_module_id,
    [CTRL_TYPE_PROCESS_CP_MMAP] = devmm_ctrl_cp_process_mmap,
    [CTRL_TYPE_PROCESS_CP_MUNMAP] = devmm_ctrl_cp_process_munmap,
    [CTRL_TYPE_GET_DCACHE_ADDR] = devmm_get_dcache_addr,
    [CTRL_TYPE_PAGEFAULT_INFO_QUERY] = devmm_ctrl_pagefault_info_query,
    [CTRL_TYPE_PAGEFAULT_INFO_CLEAR] = devmm_ctrl_pagefault_info_clear,
};

drvError_t halMemCtl(int type, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret)
{
    drvError_t ret;

    if ((type < 0) || (type >= (int)CTRL_TYPE_MAX)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_mem_ctrl_handlers[type](param_value, param_value_size, out_value, out_size_ret);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_RUN_INFO_IF((ret != DRV_ERROR_NOT_SUPPORT), "Mem_ctrl check. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvMemDeviceOpenInner(uint32_t devid, halDevOpenIn *in, halDevOpenOut *out)
{
    drvError_t ret;

    (void)in;
    (void)out;

    devmm_occupy_pipeline();
    ret = devmm_setup_device(devid);
    devmm_release_pipeline();
    return ret;
}

int drvMemDeviceOpen(uint32_t devid, int devfd)
{
    (void)devfd;
    return (int)drvMemDeviceOpenInner(devid, NULL, NULL);
}

static void devmm_heap_close(struct devmm_virt_heap_mgmt *p_heap_mgmt, uint32_t page_type, uint32_t heap_list_type,
                             uint32_t heap_sub_type)
{
    struct devmm_heap_list *heap_list = NULL;
    struct devmm_virt_list_head *pos = NULL;
    struct devmm_virt_list_head *n = NULL;
    struct devmm_virt_com_heap *heap = NULL;
    struct devmm_virt_heap_type heap_type;
    uint32_t heap_mem_type;

    heap_type.heap_type = page_type;
    heap_type.heap_list_type = heap_list_type;
    heap_type.heap_sub_type = heap_sub_type;

    for (heap_mem_type = DEVMM_HBM_MEM; heap_mem_type < DEVMM_MEM_TYPE_MAX; heap_mem_type++) {
        heap_type.heap_mem_type = heap_mem_type;
        if (devmm_get_heap_list_by_type(p_heap_mgmt, &heap_type, &heap_list) != DRV_ERROR_NONE) {
            return;
        }

        (void)pthread_rwlock_wrlock(&heap_list->list_lock);
        devmm_virt_list_for_each_safe(pos, n, &heap_list->heap_list)
        {
            heap = devmm_virt_list_entry(pos, struct devmm_virt_com_heap, list);
            devmm_virt_list_del_init(&(heap->list));
            (void)devmm_virt_destroy_heap(p_heap_mgmt, heap, true);
            heap_list->heap_cnt--;
        }
        (void)__sync_fetch_and_add(&heap_list->version, 1);
        (void)pthread_rwlock_unlock(&heap_list->list_lock);
    }

    return;
}

static void devmm_close_device(UINT32 devid)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.head.devid = devid;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_CLOSE_DEVICE, &arg);
    share_log_read_run_info(HAL_MODULE_TYPE_DEVMM);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Ioctl close device error. (ret=%d; devid=%u))\n", ret, devid);
    }
    return;
}

static int devmm_prepare_close_device(UINT32 devid)
{
    struct devmm_ioctl_arg arg = {0};

    arg.head.devid = devid;
    return (int)devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_PREPARE_CLOSE_DEVICE, &arg);
}

static drvError_t _drvMemDeviceCloseInner(uint32_t devid, halDevCloseIn *in)
{
    (void)in;
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;
    uint32_t list_type;
    int ret;

    DEVMM_RUN_INFO("DrvMemDeviceClose. (devid=%u)\n", devid);
    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        DEVMM_DRV_ERR("Heap_mgmt wasn't inited, please call device open api first. (devid=%u)\n", devid);
        return (int)DRV_ERROR_INVALID_HANDLE;
    }

    if (devid >= SVM_MAX_AGENT_NUM) {
        DEVMM_DRV_ERR("Devid is invalid. (devid=%u)\n", devid);
        return (int)DRV_ERROR_INVALID_DEVICE;
    }
    svm_mem_stats_show_all_svm_mem(devid);

    if (p_heap_mgmt->support_host_mem_pool) {
        ret = devmm_host_mem_pool_uninit(devid);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Devmm_host_mem_pool_uninit error. (ret=%d; devid=%u))\n", ret, devid);
            return ret;
        }
    }

    ret = devmm_prepare_close_device(devid);
    if (ret != 0) {
        p_heap_mgmt->can_init_dev[devid] = false;
        DEVMM_DRV_ERR("Ioctl prepare close device error. (ret=%d; devid=%u))\n", ret, devid);
        return ret;
    }

    p_heap_mgmt->is_dev_inited[devid] = false;
    p_heap_mgmt->can_init_dev[devid] = true;
    list_type = devmm_heap_list_type_by_device(devid);
    devmm_heap_close(p_heap_mgmt, DEVMM_HEAP_HUGE_PAGE, list_type, SUB_DEVICE_TYPE);
    devmm_heap_close(p_heap_mgmt, DEVMM_HEAP_HUGE_PAGE, list_type, SUB_DVPP_TYPE);
    devmm_heap_close(p_heap_mgmt, DEVMM_HEAP_HUGE_PAGE, list_type, SUB_READ_ONLY_TYPE);
    devmm_heap_close(p_heap_mgmt, DEVMM_HEAP_HUGE_PAGE, list_type, SUB_DEV_READ_ONLY_TYPE);
    devmm_heap_close(p_heap_mgmt, DEVMM_HEAP_CHUNK_PAGE, list_type, SUB_DEVICE_TYPE);

    devmm_record_recycle(devid);
    svm_mem_register_records_del(devid);
    devmm_del_mem_advise_record(true, 0, 0, devid);
    devmm_reset_dev_reserve_addr(devid);
    devmm_reset_event_devpid(devid);
    devmm_close_device(devid);
    svm_da_del_dev(devid);
    svm_clear_map_info_by_dev(devid);

    return (int)DRV_ERROR_NONE;
}

drvError_t drvMemDeviceCloseInner(uint32_t devid, halDevCloseIn *in)
{
    drvError_t ret;
    devmm_occupy_pipeline();
    ret = _drvMemDeviceCloseInner(devid, in);
    devmm_release_pipeline();
    return ret;
}

int drvMemDeviceClose(uint32_t devid)
{
    return (int)drvMemDeviceCloseInner(devid, NULL);
}

static bool devmm_agent_open_close_flag_is_valid(uint32_t flag)
{
    return (flag <= SVM_AGENT_DEVICE);
}

drvError_t halMemAgentOpen(uint32_t devid, uint32_t flag)
{
    if (devmm_agent_open_close_flag_is_valid(flag) == false) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return (drvError_t)drvMemDeviceOpen(devid, 0);
}

drvError_t halMemAgentClose(uint32_t devid, uint32_t flag)
{
    if (devmm_agent_open_close_flag_is_valid(flag) == false) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return (drvError_t)drvMemDeviceClose(devid);
}

static void devmm_restore_func_register(void);
drvError_t drvMemDeviceCloseUserRes(uint32_t devid, halDevCloseIn *in)
{
    (void)in;
    DEVMM_RUN_INFO("CloseUserRes start. (devid=%u)\n", devid);
    if (devmm_is_split_mode()) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    devmm_reset_event_devpid(devid);
    svm_uninit_mem_stats_mng(devid);
    svm_mem_register_records_del(devid);

    if (g_pro_snapshot_state == false) {
        devmm_virt_uninit_heap_mgmt();
        g_devmm_mem_dev = -1;
        devmm_svm_unmap();
        devmm_restore_func_register();
        g_pro_snapshot_state = true;
    }

    DEVMM_RUN_INFO("CloseUserRes end. (devid=%u)\n", devid);

    return DRV_ERROR_NONE;
}

static void devmm_snapshot_mem_reset(void)
{
    struct devmm_snapshot_mem_node *node = g_snapshot_mem_head;

    free(g_snapshot_host_copy);
    g_snapshot_host_copy = NULL;
    while (node != NULL) {
        struct devmm_snapshot_mem_node *next = node->next;
        free(node);
        node = next;
    }
    g_snapshot_mem_head = NULL;
}

static DVresult devmm_snapshot_mem_add(uint64_t dev_va, uint64_t size, bool required, void *priv)
{
    struct devmm_snapshot_mem_node *node = NULL;

    (void)priv;
    if ((dev_va == 0) || (size == 0) || (size > (uint64_t)SIZE_MAX)) {
        DEVMM_DRV_ERR("Invalid snapshot memory. (dev_va=0x%llx; size=%llu)\n", dev_va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    node = (struct devmm_snapshot_mem_node *)calloc(1, sizeof(*node));
    if (node == NULL) {
        DEVMM_DRV_ERR("Allocate snapshot node failed. (dev_va=0x%llx; size=%llu)\n", dev_va, size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    node->dev_va = dev_va;
    node->size = size;
    node->required = required;
    node->next = g_snapshot_mem_head;
    g_snapshot_mem_head = node;
    return DRV_ERROR_NONE;
}

static drvError_t devmm_snapshot_mem_backup(void)
{
    struct devmm_snapshot_mem_node *node = NULL;
    uint8_t *host_copy = NULL;
    size_t total_size = 0;
    drvError_t ret;

    for (node = g_snapshot_mem_head; node != NULL; node = node->next) {
        if (node->size > (uint64_t)(SIZE_MAX - total_size)) {
            DEVMM_DRV_ERR("Snapshot host copy size overflow. (total_size=%zu; size=%llu)\n", total_size, node->size);
            devmm_snapshot_mem_reset();
            return DRV_ERROR_OUT_OF_MEMORY;
        }
        total_size += (size_t)node->size;
    }

    if (total_size == 0) {
        return DRV_ERROR_NONE;
    }

    g_snapshot_host_copy = (uint8_t *)malloc(total_size);
    if (g_snapshot_host_copy == NULL) {
        DEVMM_DRV_ERR("Allocate snapshot host copy failed. (total_size=%zu)\n", total_size);
        devmm_snapshot_mem_reset();
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    host_copy = g_snapshot_host_copy;
    for (node = g_snapshot_mem_head; node != NULL; node = node->next) {
        node->host_copy = host_copy;
        ret = drvMemcpy((DVdeviceptr)(uintptr_t)node->host_copy, (size_t)node->size, (DVdeviceptr)node->dev_va,
                        (size_t)node->size);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Backup device memory failed. (ret=%d; dev_va=0x%llx; size=%llu)\n", ret, node->dev_va,
                          node->size);
            devmm_snapshot_mem_reset();
            return ret;
        }
        host_copy += (size_t)node->size;
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_snapshot_mem_restore(bool include_optional)
{
    struct devmm_snapshot_mem_node *node = NULL;
    drvError_t ret;

    for (node = g_snapshot_mem_head; node != NULL; node = node->next) {
        if (!include_optional && !node->required) {
            continue;
        }
        if (node->host_copy == NULL) {
            DEVMM_DRV_ERR("Snapshot host copy is null. (dev_va=0x%llx; size=%llu)\n", node->dev_va, node->size);
            return DRV_ERROR_OPER_NOT_PERMITTED;
        }

        ret = drvMemcpy((DVdeviceptr)node->dev_va, (size_t)node->size, (DVdeviceptr)(uintptr_t)node->host_copy,
                        (size_t)node->size);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Restore device memory failed. (ret=%d; dev_va=0x%llx; size=%llu)\n", ret, node->dev_va,
                          node->size);
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

drvError_t drvMemProcResBackup(halProcResBackupInfo *info)
{
    drvError_t ret;
    bool include_optional = ((info->reserve[0] & HAL_PROC_RES_BACKUP_INCLUDE_OPTIONAL) != 0);

    DEVMM_RUN_INFO("Backup start\n");
    if (devmm_is_split_mode()) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    devmm_occupy_pipeline();
    devmm_snapshot_mem_reset();
    ret = devmm_snapshot_collect_memory(include_optional, devmm_snapshot_mem_add, NULL);
    if (ret == DRV_ERROR_NONE) {
        ret = devmm_snapshot_mem_backup();
    }
    if (ret == DRV_ERROR_NONE) {
        ret = devmm_virt_backup_heap_mgmt();
    }
    if (ret != DRV_ERROR_NONE) {
        devmm_snapshot_mem_reset();
        devmm_release_pipeline();
        return ret;
    }
    devmm_release_pipeline();
    DEVMM_RUN_INFO("Backup end\n");

    return DRV_ERROR_NONE;
}

drvError_t drvMemProcResRestore(halProcResRestoreInfo *info)
{
    DVresult ret;
    bool include_optional = ((info->reserve[0] & HAL_PROC_RES_RESTORE_INCLUDE_OPTIONAL) != 0);

    DEVMM_RUN_INFO("Restore start\n");
    if (devmm_is_split_mode()) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    devmm_occupy_pipeline();
    ret = devmm_virt_restore_heap_mgmt();
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("devmm_virt_restore_heap_mgmt fail. (ret=%d)\n", ret);
        goto out;
    }
    devmm_restore_host_mem_pool();

    ret = devmm_record_restore();
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("record_restore fail. (ret=%d)\n", ret);
        goto out;
    }

    ret = devmm_mem_advise_restore();
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Mem advise restore fail. (ret=%d)\n", ret);
        goto out;
    }

    ret = devmm_ctrl_map_mem_restore();
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Ctrl_map_mem restore fail. (ret=%d)\n", ret);
        goto out;
    }

    ret = devmm_snapshot_mem_restore(include_optional);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Device memory restore fail. (ret=%d)\n", ret);
        goto out;
    }
    devmm_snapshot_mem_reset();
    g_pro_snapshot_state = false;
    DEVMM_RUN_INFO("Restore end\n");

out:
    devmm_release_pipeline();
    return ret;
}

/* Sunset interface: external statement has been removed from ascend_hal.h. */
DVresult drvMbindHbm(DVdeviceptr devPtr, size_t len, uint32_t type, uint32_t dev_id)
{
    (void)devPtr;
    (void)len;
    (void)type;
    (void)dev_id;
    return DRV_ERROR_NONE;
}

DVresult halSdmaCopyInner(DVdeviceptr dst, size_t dst_size, DVdeviceptr src, size_t len)
{
    (void)dst;
    (void)dst_size;
    (void)src;
    (void)len;
    return DRV_ERROR_NOT_SUPPORT;
}

DVresult halSdmaCopy(DVdeviceptr dst, size_t dst_size, DVdeviceptr src, size_t len)
{
    return halSdmaCopyInner(dst, dst_size, src, len);
}

static drvError_t devmm_remote_map_va_devid_check(void *src_ptr, uint32_t devid)
{
    struct DVattribute attr = {0};
    int ret;

    ret = drvMemGetAttribute((DVdeviceptr)(uintptr_t)src_ptr, &attr);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Get attr failed. (src_va=0x%llx; ret=%d)\n", (uint64_t)(uintptr_t)src_ptr, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((attr.memType != DV_MEM_LOCK_DEV) || (attr.devId != devid)) {
        DEVMM_DRV_ERR("Va devid mismatch. (va_devid=%u; devid=%u; src_va=0x%llx)\n", attr.devId, devid,
                      (uint64_t)(uintptr_t)src_ptr);
        return DRV_ERROR_PARA_ERROR;
    }

    return DRV_ERROR_NONE;
}

static drvError_t devmm_remote_map_input_check(void *src_ptr, uint32_t map_type, uint32_t proc_type, uint32_t devid,
                                               void **dst_ptr)
{
    if (map_type == DEV_SVM_MAP_HOST) {
        drvError_t ret = devmm_remote_map_va_devid_check(src_ptr, devid);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    if ((devid >= SVM_MAX_AGENT_NUM) || (devmm_dev_is_inited(devid) == false)) {
        DEVMM_DRV_ERR("Devid is invalid. (devid=%u; map_type=%u)\n", devid, map_type);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if ((map_type >= HOST_REGISTER_MAX_TPYE) || (proc_type >= DEVDRV_PROCESS_CPTYPE_MAX)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (dst_ptr == NULL) {
        DEVMM_DRV_ERR("Dst_ptr is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

#define SVM_MAP_TYPE_MASK ((1U << MEM_PROC_TYPE_BIT) - 1)
static inline uint32_t devmm_get_register_map_type(uint32_t flag)
{
    return (flag & SVM_MAP_TYPE_MASK);
}

/* just support read_only memory */
#define SVM_MAP_ADVISE_MASK MEM_REGISTER_READ_ONLY
static inline uint32_t devmm_get_register_access(uint32_t flag)
{
    return (flag & SVM_MAP_ADVISE_MASK);
}

static inline uint32_t devmm_is_register_host_pinned(uint32_t flag)
{
    return (flag & MEM_REGISTER_HOST_PINNED);
}

static drvError_t devmm_register_host_pinned_flag_check(uint32_t flag)
{
    uint32_t map_type = devmm_get_register_map_type(flag);
    uint32_t access = devmm_get_register_access(flag);

    if (access == MEM_REGISTER_READ_ONLY) {
        DEVMM_RUN_INFO("MEM_REGISTER_HOST_PINNED cannot combine with MEM_REGISTER_READ_ONLY. (flag=0x%x)\n", flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

    /* HOST_PINNED can only be used alone or with HOST_MEM_MAP_DEV_V2 */
    if (map_type == 0 || map_type == HOST_MEM_MAP_DEV_V2) {
        return DRV_ERROR_NONE;
    }

    DEVMM_RUN_INFO("MEM_REGISTER_HOST_PINNED can only combine with HOST_MEM_MAP_DEV_V2. (flag=0x%x)\n", flag);
    return DRV_ERROR_NOT_SUPPORT;
}

/* DEVDRV_PROCESS_CPTYPE_MAX represent the max valid bit number for proc_type*/
#define SVM_MAP_PROC_MASK ((1U << DEVDRV_PROCESS_CPTYPE_MAX) - 1)
static inline uint32_t devmm_get_register_proc_type(uint32_t flag)
{
    return ((flag >> MEM_PROC_TYPE_BIT) & SVM_MAP_PROC_MASK);
}

static drvError_t devmm_alloc_svm_mem(uint64_t size, uint32_t devid, void **dst_ptr)
{
    DVmem_advise advise = 0;

    devmm_set_module_id_to_advise(HCCL, &advise);
    return devmm_alloc_proc(devid, SUB_SVM_TYPE, advise, size, (DVdeviceptr *)dst_ptr);
}

static void devmm_free_svm_mem(void *dst_ptr)
{
    (void)devmm_free_managed((DVdeviceptr)dst_ptr);
}

static drvError_t _devmm_mem_remote_map(void *src_va, uint64_t size, uint32_t flag, uint32_t devid, void **dst_va)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.head.devid = devid;
    arg.data.map_para.src_va = (UINT64)(uintptr_t)src_va;
    arg.data.map_para.size = size;
    arg.data.map_para.map_type = devmm_get_register_map_type(flag);
    arg.data.map_para.proc_type = devmm_get_register_proc_type(flag);
    arg.data.map_para.access = devmm_get_register_access(flag);
    arg.data.map_para.dst_va = *(uint64_t *)dst_va;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_REMOTE_MAP, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Mem_remote_map ioctl error. (src_va=0x%llx; size=%llu; devid=%u; ret=%d)\n", src_va, size,
                         devid, ret);
        return ret;
    }

    *dst_va = (void *)(uintptr_t)arg.data.map_para.dst_va;
    DEVMM_DRV_DEBUG_ARG("Mem_remote_map. (devid=%u; src_va=0x%llx; size=%llu; dst_va=%p; flag=%u)\n", devid,
                        (UINT64)(uintptr_t)src_va, size, *dst_va, flag);
    return DRV_ERROR_NONE;
}

static drvError_t devmm_mem_remote_map(void *src_va, uint64_t size, uint32_t flag, uint32_t devid, void **dst_va)
{
    u64 map_size = size;
    void *tmp_dst_va = NULL;
    drvError_t ret;

    if (devmm_is_in_host_pin_range((uint64_t)(uintptr_t)src_va)) {
        tmp_dst_va = src_va;
        map_size = ALIGN_UP(size, (u64)(DEVMM_SOMA_PAGE_SIZE));
    }

    ret = _devmm_mem_remote_map(src_va, map_size, flag, devid, &tmp_dst_va);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    *dst_va = tmp_dst_va;
    return DRV_ERROR_NONE;
}

static drvError_t devmm_dev_mem_remote_map(void *src_va, uint64_t size, uint32_t flag, uint32_t devid, void **dst_va)
{
    void *tmp_dst_va = NULL;
    drvError_t ret;

    ret = devmm_alloc_svm_mem(size, devid, &tmp_dst_va);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Alloc memory failed. (ret=%d; size=%llu; flag=%u; devid=%u)\n", ret, size, flag, devid);
        return ret;
    }

    ret = _devmm_mem_remote_map(src_va, size, flag, devid, &tmp_dst_va);
    if (ret != DRV_ERROR_NONE) {
        devmm_free_svm_mem(tmp_dst_va);
        DEVMM_DRV_ERR("Mem remote map fail. (devid=%u; src_va=0x%llx; size=%llu; dst_va=0x%llx; flag=%u; ret=%d)\n",
                      devid, (UINT64)(uintptr_t)src_va, size, (UINT64)(uintptr_t)tmp_dst_va, flag, ret);
        return ret;
    }
    *dst_va = tmp_dst_va;
    return DRV_ERROR_NONE;
}

static drvError_t _devmm_register_mem_to_dma(void *src_va, uint64_t size, uint32_t flag, uint32_t devid)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    if ((src_va == NULL) || (size == 0) || (size > DEVMM_MAX_MAPPED_RANGE)) {
        DEVMM_DRV_ERR("Src_va is zero or size is invalid. (src_va=0x%llx; size=%llu)\n", (uint64_t)(uintptr_t)src_va,
                      size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devmm_get_register_map_type(flag) == HOST_MEM_MAP_DMA) &&
        (devmm_get_register_access(flag) == MEM_REGISTER_READ_ONLY)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (svm_is_vmm_mix_map((UINT64)(uintptr_t)src_va, size)) {
        DEVMM_DRV_ERR("Vmm mix map is invalid. (vptr=0x%llx; size=%lu)\n", (UINT64)(uintptr_t)src_va, size);
        return DRV_ERROR_INVALID_VALUE;
    }

#ifndef EMU_ST
    if ((devmm_va_is_in_svm_range((uint64_t)(uintptr_t)src_va) == false) &&
        devmm_va_is_in_svm_range((uint64_t)(uintptr_t)src_va + size)) {
        DEVMM_DRV_ERR("Start_va and end_va have cross between svm and non svm. (src_va=0x%llx; size=%llu)\n", src_va,
                      size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (devmm_get_host_mem_alloc_mode() == SVM_HOST_MEM_ALLOCED_BY_MALLOC) {
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif

    arg.head.devid = devid;
    arg.data.register_dma_para.vaddr = (uint64_t)(uintptr_t)src_va;
    arg.data.register_dma_para.size = size;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_REGISTER_DMA, &arg);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            DEVMM_DRV_ERR("Register mem to dma. ioctl error. (src_va=0x%llx; size=%llu; devid=%u; ret=%d)\n", src_va,
                          size, devid, ret);
        }
        return ret;
    }

    DEVMM_DRV_DEBUG_ARG("Register mem to dma. (devid=%u; src_va=0x%llx; size=%llu, flag=0x%x)\n", devid,
                        (uint64_t)(uintptr_t)src_va, size, flag);
    return DRV_ERROR_NONE;
}

drvError_t devmm_register_mem_to_dma(void *src_va, uint64_t size, uint32_t flag, uint32_t devid)
{
    if ((devid >= SVM_MAX_AGENT_NUM) || (devmm_dev_is_inited(devid) == false)) {
        DEVMM_DRV_ERR("Devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    return _devmm_register_mem_to_dma(src_va, size, flag, devid);
}

drvError_t devmm_pre_register_mem_to_dma(void *src_va, uint64_t size, uint32_t flag, uint32_t devid)
{
    return _devmm_register_mem_to_dma(src_va, size, flag, devid);
}

typedef drvError_t (*devmm_remote_map_policy)(void *src_va, uint64_t size, uint32_t flag, uint32_t devid,
                                              void **dst_va);

static drvError_t devmm_remote_map(void *src_ptr, UINT64 size, UINT32 flag, UINT32 devid, void **dst_ptr)
{
    static const devmm_remote_map_policy remote_map_policy[HOST_REGISTER_MAX_TPYE] = {
        [HOST_MEM_MAP_DEV] = devmm_mem_remote_map, [HOST_SVM_MAP_DEV] = devmm_mem_remote_map,
        [DEV_SVM_MAP_HOST] = devmm_mem_remote_map, [HOST_MEM_MAP_DEV_PCIE_TH] = devmm_mem_remote_map,
        [HOST_IO_MAP_DEV] = devmm_mem_remote_map,  [DEV_MEM_MAP_HOST] = devmm_dev_mem_remote_map,
    };
    uint32_t map_type = devmm_get_register_map_type(flag);
    uint32_t proc_type = devmm_get_register_proc_type(flag);
    DVresult ret;

    ret = devmm_remote_map_input_check(src_ptr, map_type, proc_type, devid, dst_ptr);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (svm_is_vmm_mix_map((UINT64)(uintptr_t)src_ptr, size)) {
        DEVMM_DRV_ERR("Vmm mix map is invalid. (vptr=0x%llx; size=%lu)\n", (UINT64)(uintptr_t)src_ptr, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    return remote_map_policy[map_type](src_ptr, size, flag, devid, dst_ptr);
}

struct devmm_record_host_register_data {
    void *ptr;
    uint64_t size;
    uint32_t flag;
    uint32_t devid;
};

#ifndef EMU_ST
static int devmm_host_register_record_restore(uint64_t key1, uint64_t key2, uint32_t devid, void *data)
{
    struct devmm_record_host_register_data *register_data = data;
    void *dst_ptr = NULL;
    uint32_t map_type = devmm_get_register_map_type(register_data->flag);
    int ret;

    if (map_type != DEV_SVM_MAP_HOST) {
        DEVMM_DRV_DEBUG_ARG("Not restore. (map_type=%u)\n", map_type);
        return DRV_ERROR_NONE;
    }

    DEVMM_RUN_INFO("Host register restore. (va=0x%llx; key2=%llu; size=%llu; flag=0x%x; devid=%d)\n", key1, key2,
                   register_data->size, register_data->flag, devid);
    ret = devmm_remote_map((void *)key1, register_data->size, register_data->flag, register_data->devid, &dst_ptr);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    (void)svm_mem_register_record_add(register_data->devid, key1, register_data->size, map_type,
                                      (DVdeviceptr)(uintptr_t)dst_ptr, register_data->flag);

    return DRV_ERROR_NONE;
}
#endif

static int devmm_host_register_record_data_get(void *ptr, uint32_t devid,
                                               struct devmm_record_host_register_data *register_data)
{
    uint64_t data_len = sizeof(struct devmm_record_host_register_data);
    struct devmm_record_data data = {0};
    drvError_t ret;

    data.key1 = (uint64_t)(uintptr_t)ptr;
    data.key2 = DEVMM_RECORD_KEY_INVALID;
    data.data_len = data_len;
    data.data = (void *)register_data;
    ret = devmm_record_get(DEVMM_FEATURE_HOST_REGISTER, devid, DEVMM_KEY_TYPE1, &data);
    if (ret != 0) {
        return ret;
    }
    return DRV_ERROR_NONE;
}

static void devmm_host_register_record_data_create(void *ptr, size_t size, uint32_t flag, uint32_t devid)
{
    uint64_t data_len = sizeof(struct devmm_record_host_register_data);
    struct devmm_record_host_register_data register_data = {0};
    uint32_t map_type = devmm_get_register_map_type(flag);
    struct devmm_record_data data = {0};
    drvError_t ret;

    if (map_type != DEV_SVM_MAP_HOST) {
        DEVMM_DRV_DEBUG_ARG("Not restore. (map_type=%u)\n", map_type);
        return;
    }

    register_data.ptr = ptr;
    register_data.size = size;
    register_data.flag = flag;
    register_data.devid = devid;

    data.key1 = (uint64_t)(uintptr_t)ptr;
    data.key2 = DEVMM_RECORD_KEY_INVALID;
    data.data_len = data_len;
    data.data = (void *)&register_data;
    ret = devmm_record_create_and_get(DEVMM_FEATURE_HOST_REGISTER, devid, DEVMM_KEY_TYPE1, DEVMM_NODE_INITED, &data);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_WARN("Record get fail. (ret=%d; ptr=0x%llx; size=%llu; flag=0x%x; devid=%d)\n", ret,
                       (uint64_t)(uintptr_t)ptr, (uint64_t)size, flag, devid);
    }
    return;
}

static void devmm_host_register_record_data_destroy(void *ptr, uint32_t devid)
{
    struct devmm_record_host_register_data register_data;
    uint64_t va = (uint64_t)(uintptr_t)ptr;
    uint32_t map_type;
    drvError_t ret;

    ret = devmm_host_register_record_data_get(ptr, devid, &register_data);
    if (ret != 0) {
        DEVMM_DRV_DEBUG_ARG("Not restore. (ptr=0x%llx)\n", va);
        return;
    }

    map_type = devmm_get_register_map_type(register_data.flag);
    if (map_type != DEV_SVM_MAP_HOST) {
        (void)devmm_record_put(DEVMM_FEATURE_HOST_REGISTER, devid, DEVMM_KEY_TYPE1, va, DEVMM_NODE_UNINITED);
        DEVMM_DRV_DEBUG_ARG("Not restore. (map_type=%u)\n", map_type);
        return;
    }

    /* The second put is for create */
    devmm_record_put(DEVMM_FEATURE_HOST_REGISTER, devid, DEVMM_KEY_TYPE1, va, DEVMM_NODE_UNINITED);
    ret = devmm_record_put(DEVMM_FEATURE_HOST_REGISTER, devid, DEVMM_KEY_TYPE1, va, DEVMM_NODE_UNINITED);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_WARN("Record put warn. (ret=%d; va=0x%llx)\n", ret, va);
    }
}

static int devmm_register_host_pinned_para_check(void *src_ptr, uint64_t size, uint32_t flag)
{
    if (devmm_va_is_in_svm_range((uint64_t)(uintptr_t)src_ptr)) {
        DEVMM_DRV_ERR("HOST_PINNED does not support svm address. (src_va=0x%llx)\n", (uint64_t)(uintptr_t)src_ptr);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (devmm_va_is_in_svm_range((uint64_t)(uintptr_t)src_ptr + size - 1)) {
        DEVMM_DRV_ERR("HOST_PINNED does not support svm address. (src_va=0x%llx; size=%llu)\n",
                      (uint64_t)(uintptr_t)src_ptr, size);
        return DRV_ERROR_INVALID_VALUE;
    }
    return devmm_register_host_pinned_flag_check(flag);
}

static bool devmm_need_register_dma(uint32_t map_type, void *src_ptr)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;

    if (map_type != HOST_MEM_MAP_DEV_V2) {
        return false;
    }
    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        return false;
    }
    if (p_heap_mgmt->support_host_mem_pool == false) {
        return true;
    }
    /* svm addr has been dma mapped, no need register */
    return (devmm_va_is_in_svm_range((uint64_t)(uintptr_t)src_ptr) == false);
}

static drvError_t devmm_host_register_mem(void *src_ptr, uint64_t size, uint32_t flag, uint32_t devid,
                                          uint32_t map_type, void **dst_ptr)
{
    drvError_t ret = DRV_ERROR_NONE;

    if (map_type == HOST_MEM_MAP_DMA) {
        ret = devmm_register_mem_to_dma(src_ptr, size, flag, devid);
    } else {
        if (devmm_need_register_dma(map_type, src_ptr)) {
            ret = devmm_register_mem_to_dma(src_ptr, size, flag, devid);
        }
        if (ret == DRV_ERROR_NONE) {
            ret = devmm_remote_map(src_ptr, size, flag, devid, dst_ptr);
            if (ret != DRV_ERROR_NONE) {
                if (devmm_need_register_dma(map_type, src_ptr)) {
                    (void)devmm_unregister_mem_to_dma(src_ptr, devid);
                }
            }
        }
    }

    return ret;
}

static drvError_t devmm_host_unregister_mem(void *src_ptr, uint32_t flag, uint32_t devid, uint32_t map_type)
{
    drvError_t ret = DRV_ERROR_NONE;

    if (map_type == HOST_MEM_MAP_DMA) {
        ret = devmm_unregister_mem_to_dma(src_ptr, devid);
    } else {
        ret = devmm_remote_unmap(src_ptr, devid, flag);
        if ((ret == DRV_ERROR_NONE) && devmm_need_register_dma(map_type, src_ptr)) {
            ret = devmm_unregister_mem_to_dma(src_ptr, devid);
        }
    }

    return ret;
}

drvError_t halHostRegister(void *src_ptr, UINT64 size, UINT32 flag, UINT32 devid, void **dst_ptr)
{
    uint32_t map_type = devmm_get_register_map_type(flag);
    DVdeviceptr tmp_dst;
    drvError_t ret;

    /* HOST_PINNED uses the same DMA pin-page path as HOST_MEM_MAP_DMA */
    if (devmm_is_register_host_pinned(flag) != 0) {
        ret = devmm_register_host_pinned_para_check(src_ptr, size, flag);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
        map_type = (map_type == HOST_MEM_MAP_DEV_V2) ? HOST_MEM_MAP_DEV_V2 : HOST_MEM_MAP_DMA;
    }

    if (map_type >= HOST_REGISTER_MAX_TPYE) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_host_register_mem(src_ptr, size, flag, devid, map_type, dst_ptr);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    tmp_dst = ((map_type == HOST_MEM_MAP_DMA) || (dst_ptr == NULL)) ? 0 : *((DVdeviceptr *)(uintptr_t)dst_ptr);
    ret = svm_mem_register_record_add(devid, (DVdeviceptr)(uintptr_t)src_ptr, size, map_type, tmp_dst, flag);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Register record add failed. (map_type=%u; src_ptr=0x%llx; size=%lu; devid=%u; ret=%u)\n",
                      map_type, src_ptr, size, devid, ret);
        (void)devmm_host_unregister_mem(src_ptr, flag, devid, map_type);
        return ret;
    }
    devmm_host_register_record_data_create(src_ptr, size, flag, devid);
    return DRV_ERROR_NONE;
}

drvError_t halMemHostGetDevPointer(void *src_ptr, uint32_t devid, void **dst_ptr)
{
    uint32_t map_type;
    drvError_t ret;

    if ((src_ptr == NULL) || (dst_ptr == NULL)) {
        return DRV_ERROR_PARA_ERROR;
    }

    if (devid >= DEVMM_MAX_LOGIC_DEVICE_NUM) {
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (g_register_query_and_get_attr == false) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = svm_mem_register_record_range_query_single((DVdeviceptr)(uintptr_t)src_ptr, devid, HOST_MEM_REGISTER,
                                                     NO_DMA_MAP_RECORD_TREE, &map_type,
                                                     (DVdeviceptr *)(uintptr_t)dst_ptr);
    if (ret != DRV_ERROR_NONE) {
        return DRV_ERROR_NOT_EXIST;
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_remote_unmap_input_check(uint32_t map_type, uint32_t proc_type, uint32_t devid)
{
    if (devid >= SVM_MAX_AGENT_NUM) {
        DEVMM_DRV_ERR("Devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if ((map_type >= HOST_REGISTER_MAX_TPYE) || (proc_type >= DEVDRV_PROCESS_CPTYPE_MAX)) {
        return DRV_ERROR_NOT_SUPPORT;
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_mem_remote_unmap(void *src_va, uint32_t flag, uint32_t devid, uint64_t *dst_va)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.head.devid = devid;
    arg.data.unmap_para.src_va = (UINT64)(uintptr_t)src_va;
    arg.data.unmap_para.map_type = devmm_get_register_map_type(flag);
    arg.data.unmap_para.proc_type = devmm_get_register_proc_type(flag);

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_REMOTE_UNMAP, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Mem_remote_unmap error. (src_va=0x%llx; flag=%u; devid=%u; ret=%d)\n", (UINT64)(uintptr_t)src_va,
                      flag, devid, ret);
        return ret;
    }

    *dst_va = arg.data.unmap_para.dst_va;
    DEVMM_DRV_DEBUG_ARG("Mem_remote_unmap. (devid=%u; src_va=0x%llx; dst_va=0x%llx; flag=%u)\n", devid,
                        (UINT64)(uintptr_t)src_va, *dst_va, flag);
    return DRV_ERROR_NONE;
}

static drvError_t devmm_dev_mem_remote_unmap(void *src_va, uint32_t flag, uint32_t devid, uint64_t *dst_va)
{
    drvError_t ret;

    ret = devmm_mem_remote_unmap(src_va, flag, devid, dst_va);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    devmm_free_svm_mem((void *)(uintptr_t)(*dst_va));
    return DRV_ERROR_NONE;
}

drvError_t devmm_unregister_mem_to_dma(void *src_va, uint32_t devid)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    if (devid >= SVM_MAX_AGENT_NUM) {
        DEVMM_DRV_ERR("Devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    arg.head.devid = devid;
    arg.data.unregister_dma_para.vaddr = (uint64_t)(uintptr_t)src_va;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_UNREGISTER_DMA, &arg);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    DEVMM_DRV_DEBUG_ARG("Unregister mem to dma. (devid=%u; src_va=0x%llx)\n", devid, (uint64_t)(uintptr_t)src_va);
    return DRV_ERROR_NONE;
}

typedef drvError_t (*devmm_remote_unmap_policy)(void *src_va, uint32_t devid, uint32_t flag, uint64_t *dst_va);

static drvError_t devmm_remote_unmap(void *src_ptr, UINT32 devid, UINT32 flag)
{
    static const devmm_remote_unmap_policy remote_unmap_policy[HOST_REGISTER_MAX_TPYE] = {
        [HOST_MEM_MAP_DEV] = devmm_mem_remote_unmap, [HOST_SVM_MAP_DEV] = devmm_mem_remote_unmap,
        [DEV_SVM_MAP_HOST] = devmm_mem_remote_unmap, [HOST_MEM_MAP_DEV_PCIE_TH] = devmm_mem_remote_unmap,
        [HOST_IO_MAP_DEV] = devmm_mem_remote_unmap,  [DEV_MEM_MAP_HOST] = devmm_dev_mem_remote_unmap,
    };
    uint32_t map_type = devmm_get_register_map_type(flag);
    uint32_t proc_type = devmm_get_register_proc_type(flag);
    uint64_t dst_va;
    DVresult ret;

    ret = devmm_remote_unmap_input_check(map_type, proc_type, devid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return remote_unmap_policy[map_type](src_ptr, flag, devid, &dst_va);
}

drvError_t svm_unregister(void *src_ptr, UINT32 devid, UINT32 flag)
{
    uint32_t map_type = devmm_get_register_map_type(flag);
    drvError_t ret = DRV_ERROR_NONE;

    if (devmm_is_register_host_pinned(flag) != 0) {
        ret = devmm_register_host_pinned_para_check(src_ptr, 1, flag);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
        map_type = (map_type == HOST_MEM_MAP_DEV_V2) ? HOST_MEM_MAP_DEV_V2 : HOST_MEM_MAP_DMA;
    }

    if (map_type >= HOST_REGISTER_MAX_TPYE) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_host_unregister_mem(src_ptr, flag, devid, map_type);
    if (ret != DRV_ERROR_NONE) {
        return DRV_ERROR_NOT_EXIST;
    }

    devmm_host_register_record_data_destroy(src_ptr, devid);
    svm_mem_register_record_del(devid, (DVdeviceptr)(uintptr_t)src_ptr, map_type);

    return DRV_ERROR_NONE;
}

drvError_t halHostUnregister(void *src_ptr, UINT32 devid)
{
    enum mem_register_src_side side;
    uint32_t flag;
    drvError_t ret;

    for (side = HOST_MEM_REGISTER; side < MEM_REGISTER_MAX_SIDE; side++) {
        ret = svm_mem_register_record_query((DVdeviceptr)(uintptr_t)src_ptr, devid, side, NULL, &flag);
        if (ret == DRV_ERROR_NONE) {
            return svm_unregister(src_ptr, devid, flag);
        }
    }

    DEVMM_DRV_ERR("HostUnregister error. (src_ptr=0x%llx; devid=%u)\n", (uint64_t)(uintptr_t)src_ptr, devid);
    return DRV_ERROR_NOT_EXIST;
}

/* DEV_MEM_MAP_HOST must use the correct flag */
drvError_t halHostUnregisterEx(void *src_ptr, UINT32 devid, UINT32 flag)
{
    if (devmm_get_register_map_type(flag) == DEV_MEM_MAP_HOST) {
        return svm_unregister(src_ptr, devid, flag);
    }
    return halHostUnregister(src_ptr, devid);
}

static inline bool devmm_is_advise_ts(uint64_t flag)
{
    uint32_t advise_4g = (flag >> MEM_ADVISE_4G_BIT) & 1;
    uint32_t advise_ts = (flag >> MEM_ADVISE_TS_BIT) & 1;
    return ((advise_4g != 0) || (advise_ts != 0));
}

static inline bool devmm_mem_is_readonly(uint64_t flag)
{
    return (((flag >> MEM_READONLY_BIT) & 1) == 1) ? true : false;
}

static inline bool devmm_mem_is_dev_readonly(uint64_t flag)
{
    return (((flag >> MEM_HOST_RW_DEV_RO_BIT) & 1) == 1) ? true : false;
}

static inline bool devmm_mem_is_giant_page(uint64_t flag)
{
    return (((flag >> MEM_PAGE_GIANT_BIT) & 1) == 1) ? true : false;
}

static inline void devmm_phy_page_type_set_advise(uint32_t phy_page_type, DVmem_advise *advise)
{
    *advise |= (phy_page_type != 0) ? DV_ADVISE_HUGEPAGE : 0;
}

static DVresult devmm_host_set_advise(uint32_t devid, uint32_t virt_mem_type, uint64_t flag, uint32_t phy_page_type,
                                      DVmem_advise *advise)
{
    (void)devid;
    if (devmm_mem_is_readonly(flag) || devmm_mem_is_dev_readonly(flag)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (devmm_mem_is_giant_page(flag)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (virt_mem_type == MEM_HOST_AGENT_VAL) {
        if (devmm_is_advise_ts(flag) == true) {
            return DRV_ERROR_NOT_SUPPORT;
        }
        /* host_agent default alloc normal_page ddr */
        *advise |= DV_ADVISE_DDR;
    }

    if (virt_mem_type == MEM_HOST_VAL || virt_mem_type == MEM_HOST_UVA_VAL) {
        /* host default alloc normal_page ddr */
        *advise |= DV_ADVISE_HOST | DV_ADVISE_DDR;
    }
    if (virt_mem_type == MEM_HOST_UVA_VAL) {
        *advise |= (DV_ADVISE_HOST_UVA | DV_ADVISE_DDR | DV_ADVISE_HUGEPAGE);
    }

    devmm_phy_page_type_set_advise(phy_page_type, advise);
    return DRV_ERROR_NONE;
}

static inline void devmm_hbm_ddr_p2p_set_advise(uint64_t flag, DVmem_advise *advise)
{
    uint32_t advise_p2p = (((flag >> MEM_ADVISE_P2P_BIT) & 1) != 0) || (((flag >> MEM_ADVISE_BAR_BIT) & 1) != 0);
    uint32_t hbm_mem_type = (flag >> MEM_PHY_BIT) & 1;

    if (hbm_mem_type != 0) {
        *advise |= (advise_p2p != 0) ? DV_ADVISE_P2P_HBM : 0;
        *advise |= DV_ADVISE_HBM;
    } else {
        *advise |= (advise_p2p != 0) ? DV_ADVISE_P2P_DDR : 0;
        *advise |= DV_ADVISE_DDR;
    }
}

static inline bool devmm_support_dev_readonly(void)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;

    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if ((p_heap_mgmt != NULL) && (p_heap_mgmt->support_host_rw_dev_ro == true)) {
        return true;
    }
    return false;
}

static inline drvError_t devmm_readonly_mem_set_advise(uint64_t flag, uint32_t virt_mem_type, DVmem_advise *advise)
{
    if ((devmm_mem_is_readonly(flag) == true) || (devmm_mem_is_dev_readonly(flag) == true)) {
        if ((virt_mem_type == MEM_DEV_VAL) ||
            ((virt_mem_type == MEM_DVPP_VAL) && (devmm_mem_is_dev_readonly(flag) == false))) {
            *advise |= DV_ADVISE_READONLY;
        } else {
            return DRV_ERROR_NOT_SUPPORT;
        }

        if ((devmm_mem_is_dev_readonly(flag)) && (devmm_support_dev_readonly() == false)) {
            *advise &= ~((uint32_t)DV_ADVISE_READONLY);
        }
    }
    return DRV_ERROR_NONE;
}

static inline drvError_t devmm_continuty_mem_set_advise(uint64_t flag, uint64_t size, uint32_t virt_mem_type,
                                                        uint32_t phy_page_type, DVmem_advise *advise)
{
    /* dvpp mem ignores 4G and continuty flag */
    if (virt_mem_type != MEM_DVPP_VAL) {
        uint32_t advise_continuty = (flag >> MEM_CONTINUTY_BIT) & 1;
        if (advise_continuty != 0) {
            if ((size > MAX_CONTINUTY_PHYS_SIZE) || (phy_page_type != 0)) {
                return DRV_ERROR_INVALID_VALUE;
            }
            *advise |= DV_ADVISE_CONTINUTY;
        }
    }
    return DRV_ERROR_NONE;
}

static inline uint32_t devmm_get_devid_from_flag(uint64_t flag)
{
    uint32_t virt_mem_type = (flag >> MEM_VIRT_BIT) & MEM_VIRT_MASK;

    if (virt_mem_type == MEM_HOST_AGENT_VAL) {
        return SVM_HOST_AGENT_ID;
    }
    return flag & MEM_DEVID_MASK;
}

static inline uint32_t devmm_get_virt_mem_type_from_flag(uint64_t flag)
{
    return (flag >> MEM_VIRT_BIT) & MEM_VIRT_MASK;
}

static inline uint32_t devmm_get_page_type_from_flag(uint64_t flag)
{
    return (flag >> MEM_PAGE_BIT) & 1;
}

static inline drvError_t devmm_ts_mem_set_advise(uint64_t flag, uint32_t virt_mem_type, DVmem_advise *advise)
{
    (void)virt_mem_type;
    uint32_t advise_p2p = (((flag >> MEM_ADVISE_P2P_BIT) & 1) != 0) || (((flag >> MEM_ADVISE_BAR_BIT) & 1) != 0);
    uint32_t hbm_mem_type = (flag >> MEM_PHY_BIT) & 1;
    bool no_support_numa_ts = (g_support_numa_ts == false) && (hbm_mem_type == 0) && (advise_p2p == 0);

    if (devmm_is_advise_ts(flag) == true) {
        *advise |= DV_ADVISE_TS_DDR;
    } else if (no_support_numa_ts != 0) {
        /* adapt rts old version ddr use hbm to replace */
        *advise &= ~((uint32_t)DV_ADVISE_DDR);
        *advise |= DV_ADVISE_HBM;
    }
    return DRV_ERROR_NONE;
}

static inline void devmm_get_mem_type(uint64_t flag, uint32_t *virt_mem_type, uint32_t *phy_page_type)
{
    *virt_mem_type = devmm_get_virt_mem_type_from_flag(flag);
    *phy_page_type = devmm_get_page_type_from_flag(flag);
}

static bool devmm_is_host_advise(uint32_t devid, uint32_t virt_mem_type)
{
    (void)devid;
    if ((virt_mem_type == MEM_HOST_VAL) || (virt_mem_type == MEM_HOST_UVA_VAL) ||
        (virt_mem_type == MEM_HOST_AGENT_VAL)) {
        return true;
    }
    return false;
}

static uint32_t devmm_get_module_id_by_flag(uint64_t flag)
{
    uint32_t model_id = (uint32_t)((flag >> MEM_MODULE_ID_BIT) & MEM_MODULE_ID_MASK);

    /* RUNTIME will fill invalid model_id in some scenarios */
    model_id = (model_id >= SVM_MAX_MODULE_ID) ? UNKNOWN_MODULE_ID : model_id;
    return model_id;
}

static bool devmm_is_support_agent_giant_page_feature(uint32_t devid)
{
    struct devmm_virt_heap_mgmt *mgmt = NULL;

    mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (mgmt == NULL) {
        DEVMM_DRV_ERR("Init heap mgmt failed. (devid=%u)\n", devid);
        return false;
    }

    if (devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
        DEVMM_DRV_ERR("Invalid devid. (devid=%u)\n", devid);
        return false;
    }
    return mgmt->support_agent_giant_page[devid];
}

static bool devmm_is_support_host_giant_page_feature(void)
{
    struct devmm_virt_heap_mgmt *mgmt = NULL;

    mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (mgmt == NULL) {
        DEVMM_DRV_ERR("Init heap mgmt failed.\n");
        return false;
    }

#ifndef EMU_ST
    return mgmt->support_host_giant_page;
#else
    return true;
#endif
}

static drvError_t devmm_giant_page_set_advise(uint64_t flag, uint32_t virt_mem_type, DVmem_advise *advise)
{
    if (devmm_mem_is_giant_page(flag)) {
        uint32_t devid = devmm_get_devid_from_flag(flag);

        if (virt_mem_type != MEM_DEV_VAL) {
            DEVMM_RUN_INFO("Giant page only support dev mem. (flag=0x%llx; virt_mem_type=%u)\n", flag, virt_mem_type);
            return DRV_ERROR_NOT_SUPPORT;
        }
        if (devmm_is_support_agent_giant_page_feature(devid) == false) {
            DEVMM_RUN_INFO("Not support giant page feature. (devid=%u)\n", devid);
            return DRV_ERROR_NOT_SUPPORT;
        }
        if (devmm_is_advise_ts(flag)) {
            DEVMM_RUN_INFO("Giant page not support 4g and ts. (flag=0x%llx)\n", flag);
            return DRV_ERROR_NOT_SUPPORT;
        }
        if (((flag >> MEM_CONTINUTY_BIT) & 1) != 0) {
            DEVMM_RUN_INFO("Giant page not support continuous phy. (flag=0x%llx)\n", flag);
            return DRV_ERROR_NOT_SUPPORT;
        }
        if (devmm_mem_is_readonly(flag) || devmm_mem_is_dev_readonly(flag)) {
            DEVMM_RUN_INFO("Giant page not support readonly. (flag=0x%llx)\n", flag);
            return DRV_ERROR_NOT_SUPPORT;
        }
        if (((flag >> MEM_PAGE_BIT) & 1) != 0) {
#ifndef EMU_ST
            DEVMM_DRV_ERR("Alloc both giant page and huge page is invalid. (flag=0x%llx)\n", flag);
            return DRV_ERROR_INVALID_VALUE;
#endif
        }
        *advise |= DV_ADVISE_GIANTPAGE;
        *advise |= DV_ADVISE_HUGEPAGE;
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_dev_set_advise(uint64_t flag, uint64_t size, uint32_t virt_mem_type, uint32_t phy_page_type,
                                       DVmem_advise *advise)
{
    if (devmm_readonly_mem_set_advise(flag, virt_mem_type, advise) != DRV_ERROR_NONE) {
        DEVMM_RUN_INFO("Not support mem_readonly flag. (alloc_size=%llu; flag=0x%llx)\n", size, flag);
        return DRV_ERROR_NOT_SUPPORT;
    }
    /* dvpp mem ignores huge/p2p/TS/continuty flag */
    if (virt_mem_type != MEM_DVPP_VAL) {
        devmm_phy_page_type_set_advise(phy_page_type, advise);
        devmm_hbm_ddr_p2p_set_advise(flag, advise);

        if (devmm_continuty_mem_set_advise(flag, size, virt_mem_type, phy_page_type, advise) != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Invalid continuity size or huge page. (alloc_size=%llu; flag=0x%llx)\n", size, flag);
            return DRV_ERROR_INVALID_VALUE;
        }
        if (devmm_ts_mem_set_advise(flag, virt_mem_type, advise) != DRV_ERROR_NONE) {
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    return devmm_giant_page_set_advise(flag, virt_mem_type, advise);
}

static DVresult devmm_set_advise(uint32_t devid, uint64_t flag, uint64_t size, DVmem_advise *advise)
{
    uint32_t virt_mem_type, phy_page_type;
    DVresult ret;

    devmm_get_mem_type(flag, &virt_mem_type, &phy_page_type);

    if (!devmm_is_host_advise(devid, virt_mem_type)) {
        ret = devmm_dev_set_advise(flag, size, virt_mem_type, phy_page_type, advise);
    } else {
        ret = devmm_host_set_advise(devid, virt_mem_type, flag, phy_page_type, advise);
    }

    devmm_set_module_id_to_advise(devmm_get_module_id_by_flag(flag), advise);
    if ((flag & MEM_SNAPSHOT_REQUIRED) != 0) {
        *advise |= DV_ADVISE_SNAPSHOT_REQUIRED;
    } else if ((flag & MEM_SNAPSHOT_OPTIONAL) != 0) {
        *advise |= DV_ADVISE_SNAPSHOT_OPTIONAL;
    }
    DEVMM_DRV_DEBUG_ARG("Flag. (flag=0x%llx; size=0x%llx; ad=0x%x; devid=%u; virt=%u; huge=%u; numa_ts=%u)\n", flag,
                        size, *advise, devid, virt_mem_type, phy_page_type, g_support_numa_ts);
    return ret;
}

static bool devmm_check_support_dev_read_only(uint32_t devid)
{
    struct devmm_virt_heap_mgmt *mgmt = NULL;

    if (devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
        return false;
    }
    mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if ((mgmt == NULL) || (mgmt->support_dev_read_only[devid] == 0)) {
        return false;
    }
    return true;
}

static drvError_t devmm_get_sub_mem_type(uint32_t devid, uint64_t flag, unsigned long long size, uint32_t *sub_mem_type)
{
    (void)size;
    uint32_t virt_mem_type = (flag >> MEM_VIRT_BIT) & MEM_VIRT_MASK;

    if (virt_mem_type == MEM_DVPP_VAL) {
        if (devmm_is_host_agent(devid) == true) {
            return DRV_ERROR_NOT_SUPPORT;
        }
        *sub_mem_type = SUB_DVPP_TYPE;
    } else if ((virt_mem_type == MEM_DEV_VAL) || (virt_mem_type == MEM_HOST_AGENT_VAL)) {
        bool mem_readonly = false, mem_dev_readonly = false;

        *sub_mem_type = SUB_DEVICE_TYPE;
        mem_readonly = devmm_mem_is_readonly(flag);
        if (mem_readonly == true) {
            if (devmm_check_support_dev_read_only(devid) == false) {
                return DRV_ERROR_NOT_SUPPORT;
            }
            *sub_mem_type = SUB_READ_ONLY_TYPE;
        }

        mem_dev_readonly = devmm_mem_is_dev_readonly(flag);
        if ((mem_dev_readonly == true) && devmm_support_dev_readonly()) {
            *sub_mem_type = SUB_DEV_READ_ONLY_TYPE;
        }

        if ((mem_readonly == true) && (mem_dev_readonly == true)) {
            DEVMM_DRV_ERR("Invalid flag, readonly and dev_readonly only support one type. \n");
            return DRV_ERROR_PARA_ERROR;
        }
    } else if (virt_mem_type == MEM_HOST_VAL) {
        *sub_mem_type = SUB_HOST_TYPE;
    } else if (virt_mem_type == MEM_HOST_UVA_VAL) {
        if ((flag & MEM_CONTIGUOUS_PHY) || (flag & MEM_ADVISE_P2P) || (flag & MEM_ADVISE_BAR) ||
            (flag & MEM_DEV_CP_ONLY) || (flag & MEM_PAGE_HUGE) || (flag & MEM_PAGE_GIANT)) {
            return DRV_ERROR_NOT_SUPPORT;
        }

        *sub_mem_type = SUB_HOST_TYPE;
    } else if (virt_mem_type == MEM_SVM_VAL) {
        if (devmm_get_host_mem_alloc_mode() == SVM_HOST_MEM_ALLOCED_BY_MALLOC) {
            DEVMM_DRV_ERR("Not support svm mem, because asan is opened or os not support mmap 8T.\n");
            return DRV_ERROR_NOT_SUPPORT;
        }

        *sub_mem_type = SUB_SVM_TYPE;
#ifndef UVM_OPEN
    } else if (virt_mem_type == MEM_UVM_VAL) {
        if (devmm_get_uvm_host_mem_alloc_mode() == UVM_HOST_MEM_ALLOCED_BY_MALLOC) {
            DEVMM_DRV_ERR("Not support uvm mem, because asan is opened or os not support mmap 3T.\n");
            return DRV_ERROR_NOT_SUPPORT;
        }

        *sub_mem_type = SUB_UVM_TYPE;
#endif
    } else {
        DEVMM_DRV_ERR("Not support for flag(0x%llx), virt_mem_type=0x%x\n", flag, virt_mem_type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

drvError_t halMemAllocInner(void **pp, unsigned long long size, unsigned long long flag)
{
    DVmem_advise advise = 0;
    uint32_t sub_mem_type, devid;
    DVresult ret;

    if (pp == NULL) {
        DEVMM_DRV_ERR("pp is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    devid = devmm_get_devid_from_flag(flag);
    ret = devmm_get_sub_mem_type(devid, flag, size, &sub_mem_type);
    if (ret != DRV_ERROR_NONE) {
        goto show_mem_stats;
    }

    ret = devmm_set_advise(devid, flag, size, &advise);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT), "Invalid flag. (flag=0x%llx; ret=%d)\n", flag, ret);
        goto show_mem_stats;
    }

    *pp = NULL;
    ret = devmm_alloc_proc(devid, sub_mem_type, advise, size, (DVdeviceptr *)pp);
    if (ret != DRV_ERROR_NONE) {
        goto show_mem_stats;
    }

    DEVMM_DRV_DEBUG_ARG("Alloc. (ptr=%llx; size=%llu; virt_type=%u; flag=0x%llx)\n", *(DVdeviceptr *)pp, size,
                        sub_mem_type, flag);

    return 0;

show_mem_stats:
    if (sub_mem_type == SUB_HOST_TYPE) {
        svm_mem_stats_show_host();
    } else {
        svm_mem_stats_show_device(devid);
    }
    return ret;
}

drvError_t halMemAlloc(void **pp, unsigned long long size, unsigned long long flag)
{
    drvError_t ret;

    DRV_CURRENT_API_SCOPE();

    devmm_use_pipeline();
    ret = halMemAllocInner(pp, size, flag);
    devmm_unuse_pipeline();

    return ret;
}

drvError_t halMemFreeInner(void *pp)
{
    drvError_t ret;

    DEVMM_DRV_DEBUG_ARG("Free enter. (ptr=0x%llx)\n", (DVdeviceptr)(uintptr_t)pp);

    ret = devmm_free_managed((DVdeviceptr)(uintptr_t)pp);
    if (ret == DRV_ERROR_BUSY) {
        devmm_report_mem_in_use((unsigned long long)(uintptr_t)pp);
    }
    return ret;
}

drvError_t halMemFree(void *pp)
{
    drvError_t ret;

    DRV_CURRENT_API_SCOPE();

    devmm_use_pipeline();
    ret = halMemFreeInner(pp);
    devmm_unuse_pipeline();

    return ret;
}

drvError_t drvDeviceGetTransWay(void *src, void *dest, uint8_t *trans_type)
{
    DVdeviceptr dst_tmp = (DVdeviceptr)(uintptr_t)dest;
    DVdeviceptr src_tmp = (DVdeviceptr)(uintptr_t)src;

    if (trans_type == NULL) {
        DEVMM_DRV_ERR("Trans_type is NULL. (dst=0x%llx; src=0x%llx)\n", dst_tmp, src_tmp);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* 1:pcie dma, 0:sdma */
    *trans_type = 0;
    DEVMM_DRV_DEBUG_ARG("Argument. (dst=0x%llx; src=0x%llx; trans_type=%u)\n", dst_tmp, src_tmp, *trans_type);
    return DRV_ERROR_NONE;
}

DVresult halMemcpy3D(void *pCopy)
{
    (void)pCopy;
    return DRV_ERROR_NOT_SUPPORT;
}

STATIC INLINE DVresult devmm_check_memcpy2d_input_valid(struct drvMem2D *copy2d)
{
    uint64_t src_size = (copy2d->height - 1) * copy2d->spitch + copy2d->width;
    uint64_t dst_size = (copy2d->height - 1) * copy2d->dpitch + copy2d->width;

    if ((copy2d->dst == NULL) || (copy2d->src == NULL)) {
        DEVMM_DRV_ERR("Input parameter src or dst is NULL. (dst_is_null=%d; src_is_null=%d)\n", (copy2d->dst == NULL),
                      (copy2d->src == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((copy2d->width > copy2d->dpitch) || (copy2d->width > copy2d->spitch)) {
        DEVMM_DRV_ERR("Dpitch and spitch should both be larger than width. (dpitch=%llu; spitch=%llu; "
                      "width=%llu)\n",
                      copy2d->dpitch, copy2d->spitch, copy2d->width);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((copy2d->width == 0) || (copy2d->height == 0)) {
        DEVMM_DRV_ERR("Width and height should both be larger than 0. (width=%llu; height=%llu)\n", copy2d->width,
                      copy2d->height);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((!devmm_va_is_svm((virt_addr_t)(uintptr_t)copy2d->src)) &&
        (!devmm_va_is_svm((virt_addr_t)(uintptr_t)copy2d->dst))) {
        DEVMM_RUN_INFO("Not support h2h copy. (dst=0x%llx; src=0x%llx)\n", copy2d->dst, copy2d->src);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((copy2d->direction == DRV_MEMCPY_HOST_TO_HOST) || (copy2d->direction >= DRV_MEMCPY_DEVICE_TO_DEVICE)) {
        DEVMM_RUN_INFO("Not support h2h&d2d copy. (direction=%u)\n", copy2d->direction);
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((copy2d->height != 1) &&
        (((copy2d->src + copy2d->spitch) < copy2d->src) || ((copy2d->dst + copy2d->dpitch) < copy2d->dst))) {
        DEVMM_DRV_ERR("Pitch is invalid. (src=0x%llx; dst=0x%llx; spitch=%llu; dpitch=%llu)\n", copy2d->src,
                      copy2d->dst, copy2d->spitch, copy2d->dpitch);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (svm_is_vmm_mix_map((virt_addr_t)(uintptr_t)copy2d->src, src_size) ||
        svm_is_vmm_mix_map((virt_addr_t)(uintptr_t)copy2d->dst, dst_size)) {
        DEVMM_DRV_ERR("Vmm mix map is invalid. (src=0x%llx; dst=0x%llx; spitch=%llu; dpitch=%llu)\n", copy2d->src,
                      copy2d->dst, copy2d->spitch, copy2d->dpitch);
        return DRV_ERROR_INVALID_VALUE;
    }
    return 0;
}

STATIC INLINE DVresult devmm_memcpy2d_sync(struct drvMem2D *memcopy2d)
{
    struct drvMem2D *copy2d = memcopy2d;
    struct devmm_ioctl_arg arg = {0};
    int incoherence_flag = 0;
    DVresult ret;

    DEVMM_DRV_DEBUG_ARG("Memcpy2d synchronize. (dst=0x%llx; src=0x%llx; dpitch=%llu; spitch=%llu; width=%llu; "
                        "height=%llu; direction=%u)\n",
                        copy2d->dst, copy2d->src, copy2d->dpitch, copy2d->spitch, copy2d->width, copy2d->height,
                        copy2d->direction);

    ret = devmm_check_memcpy2d_input_valid(copy2d);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    arg.data.copy2d_para.dst = (DVdeviceptr)copy2d->dst;
    arg.data.copy2d_para.src = (DVdeviceptr)copy2d->src;
    arg.data.copy2d_para.dpitch = copy2d->dpitch;
    arg.data.copy2d_para.spitch = copy2d->spitch;
    arg.data.copy2d_para.width = copy2d->width;
    arg.data.copy2d_para.height = copy2d->height;
    arg.data.copy2d_para.direction = copy2d->direction;

    ret = devmm_memcpy2d_cache_incoherence(&arg.data.copy2d_para, &incoherence_flag);
    if (incoherence_flag != 0) {
        return ret;
    }

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEMCPY2D, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Memcpy2d synchronize error. (ret=%d; dst=0x%llx; src=0x%llx; dpitch=%llu; spitch=%llu; "
                         "width=%llu; height=%llu; direction=%u)\n",
                         ret, copy2d->dst, copy2d->src, copy2d->dpitch, copy2d->spitch, copy2d->width, copy2d->height,
                         copy2d->direction);
        devmm_print_svm_va_info((uint64_t)(uintptr_t)copy2d->src, ret);
        devmm_print_svm_va_info((uint64_t)(uintptr_t)copy2d->dst, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

STATIC INLINE DVresult devmm_memcpy2d_convert(struct drvMem2DAsync *copy2d_async)
{
    struct devmm_ioctl_arg arg = {0};
    struct drvMem2D *copy2d = &copy2d_async->copy2dInfo;
    struct DMA_ADDR *dma_addr = copy2d_async->dmaAddr;
    uint64_t len = copy2d->width * copy2d->height;
    DVresult ret;

    DEVMM_DRV_DEBUG_ARG("Memcpy2d_convert. (dst=0x%llx; src=0x%llx; dpitch=%llu; spitch=%llu; width=%llu; "
                        "height=%llu; fixed_size=%llu; direction=%u)\n",
                        copy2d->dst, copy2d->src, copy2d->dpitch, copy2d->spitch, copy2d->width, copy2d->height,
                        copy2d->fixed_size, copy2d->direction);

    ret = devmm_check_memcpy2d_input_valid(copy2d);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    if (copy2d->height > DEVMM_CONVERT2D_HEIGHT_MAX) {
        DEVMM_DRV_ERR("Height is larger than 5M. (height=%llu)\n", copy2d->height);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (copy2d->fixed_size >= len) {
        DEVMM_DRV_ERR("Fixed_size should be smaller than width*height. (fixed_size=%llu; width=%llu; "
                      "height=%llu)\n",
                      copy2d->fixed_size, copy2d->width, copy2d->height);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (dma_addr == NULL) {
        DEVMM_DRV_ERR("Out parameter dmaAddr is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    /* host agent not support convert */
    if (dma_addr->offsetAddr.devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
        DEVMM_DRV_ERR("Devid is out of range. (devid=%u; range=0-%u)\n", dma_addr->offsetAddr.devid,
                      (UINT32)(DEVMM_MAX_PHY_DEVICE_NUM - 1));
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.head.devid = dma_addr->offsetAddr.devid;
    arg.data.convrt_para.virt_id = dma_addr->offsetAddr.devid;
    arg.data.convrt_para.pDst = (DVdeviceptr)copy2d->dst;
    arg.data.convrt_para.pSrc = (DVdeviceptr)copy2d->src;
    arg.data.convrt_para.len = copy2d->width;
    arg.data.convrt_para.dpitch = copy2d->dpitch;
    arg.data.convrt_para.spitch = copy2d->spitch;
    arg.data.convrt_para.height = copy2d->height;
    arg.data.convrt_para.fixed_size = copy2d->fixed_size;
    arg.data.convrt_para.direction = copy2d->direction;

    return devmm_convert_proc(&arg, dma_addr);
}

drvError_t halMemcpy2DInner(struct MEMCPY2D *p_copy)
{
    struct MEMCPY2D *memcpy2d = p_copy;
    DVresult ret;

    if (memcpy2d == NULL) {
        DEVMM_DRV_ERR("Input parameter pCopy is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (memcpy2d->type == DEVMM_MEMCPY2D_SYNC) {
        ret = devmm_memcpy2d_sync(&memcpy2d->copy2d);
    } else if (memcpy2d->type == DEVMM_MEMCPY2D_ASYNC_CONVERT) {
        ret = devmm_memcpy2d_convert(&memcpy2d->copy2dAsync);
    } else if (memcpy2d->type == DEVMM_MEMCPY2D_ASYNC_DESTROY) {
        ret = drvMemDestroyAddr(memcpy2d->copy2dAsync.dmaAddr);
    } else {
        DEVMM_DRV_ERR("Input parameter type is invalid. (type=%u)\n", memcpy2d->type);
        ret = DRV_ERROR_INVALID_VALUE;
    }

    return ret;
}

drvError_t halMemcpy2D(struct MEMCPY2D *p_copy)
{
    return halMemcpy2DInner(p_copy);
}

static drvError_t devmm_batch_va_is_valid(uint64_t dst[], uint64_t src[], size_t size[], size_t count)
{
    unsigned int i;

    for (i = 0; i < count; i++) {
        if (size[i] == 0) {
#ifndef EMU_ST
            DEVMM_DRV_ERR("Size is 0. (i=%u)\n", i);
            return DRV_ERROR_INVALID_VALUE;
#endif
        }
        if ((devmm_va_is_svm(dst[i]) == 0) && (devmm_va_is_svm(src[i]) == 0)) {
            DEVMM_RUN_INFO("Memcpy batch not support h2h. (dst=0x%llx; src=0x%llx; index=%u)\n", dst[i], src[i], i);
            return DRV_ERROR_NOT_SUPPORT;
        }

        if (svm_is_vmm_mix_map(src[i], size[i]) || svm_is_vmm_mix_map(dst[i], size[i])) {
            DEVMM_DRV_ERR("Vmm mix map is invalid. (dst=0x%llx; src=0x%llx; size=%lu)\n", dst, src, size[i]);
            return DRV_ERROR_INVALID_VALUE;
        }
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_memcpy_batch_para_check(uint64_t dst[], uint64_t src[], size_t size[], size_t count)
{
    if ((dst == NULL) || (src == NULL) || (size == NULL) || (count == 0)) {
        DEVMM_DRV_ERR("Memcpy batch para check failed. (dst_is_null=%d; src_is_null=%d; size_is_null=%d; count=%lu)\n",
                      (dst == NULL), (src == NULL), (size == NULL), count);
        return DRV_ERROR_PARA_ERROR;
    }
    if (count > DEVMM_MEMCPY_BATCH_MAX_COUNT) {
        return DRV_ERROR_NOT_SUPPORT;
    }
    return devmm_batch_va_is_valid(dst, src, size, count);
}

static drvError_t halMemcpyBatchInner(uint64_t dst[], uint64_t src[], size_t size[], size_t count)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    ret = devmm_memcpy_batch_para_check(dst, src, size, count);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
    arg.data.copy_batch_para.dst = (uint64_t *)dst;
    arg.data.copy_batch_para.src = (uint64_t *)src;
    arg.data.copy_batch_para.size = size;
    arg.data.copy_batch_para.addr_count = (uint32_t)count;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEMCPY_BATCH, &arg);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            DEVMM_DRV_ERR("Memcpy batch ioctl error. (ret=%d)\n", ret);
        }
        return ret;
    }
    return DRV_ERROR_NONE;
}

DVresult halMemcpyBatch(uint64_t dst[], uint64_t src[], size_t size[], size_t count)
{
    return halMemcpyBatchInner(dst, src, size, count);
}

static DVresult devmm_set_mem_advise(unsigned int type, DVmem_advise *advise)
{
    if (type == ADVISE_PERSISTENT) {
        *advise = DV_ADVISE_PERSISTENT;
    } else if (type == ADVISE_DEV_MEM) {
        *advise = (DV_ADVISE_POPULATE | DV_ADVISE_LOCK_DEV);
    } else {
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t halMemAdvise(DVdeviceptr ptr, size_t count, unsigned int type, DVdevice device)
{
    DVmem_advise mem_advise;
    drvError_t ret;

    if (type == ADVISE_SNAPSHOT_REQUIRED) {
        return devmm_snapshot_mark_required(ptr, count, device);
    }

    if ((type == ADVISE_ACCESS_READONLY) || (type == ADVISE_ACCESS_READWRITE)) {
        return DRV_ERROR_NONE;
    }

    ret = devmm_set_mem_advise(type, &mem_advise);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return drv_mem_advise(ptr, count, mem_advise, device);
}

#ifndef UVM_OPEN
drvError_t halMemManagedAdvise(DVdeviceptr ptr, size_t size, uint32_t advise, struct drv_uvm_location location)
{
    if (size == 0) {
        DEVMM_DRV_ERR("Invalid size, size should not be 0. (ptr=0x%llx; size=%zu)\n", ptr, size);
        return DRV_ERROR_INVALID_VALUE;
    }

    return drv_mem_managed_advise(ptr, size, advise, (int)location.type, location.id);
}
#endif

static void devmm_process_status_result_show(DVdevice device, processType_t process_type, processStatus_t status,
                                             u32 result)
{
    if (status != STATUS_SVM_PAGE_FALUT_ERR_CNT) {
        if (result == (u32) true) {
            if (status == STATUS_NOMEM) {
                svm_mem_stats_show_device(device);
            }
            DEVMM_RUN_INFO("Process status occurs. (devid=%u; process_type=%d; status=%d)\n", device, process_type,
                           status);
        }
    }
}

drvError_t halCheckProcessStatus(DVdevice device, processType_t process_type, processStatus_t status, bool *is_matched)
{
    struct drv_process_status_output out = {0};
    drvError_t ret;

    if (is_matched == NULL) {
        DEVMM_DRV_ERR("Input parameter isMatched is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = halCheckProcessStatusEx(device, process_type, status, &out);
    if (ret == 0) {
        if ((status == STATUS_NOMEM) || (status == STATUS_SVM_PAGE_FALUT_ERR_OCCUR)) {
            *is_matched = ((out.result != 0) ? true : false);
        } else {
            ret = DRV_ERROR_NOT_SUPPORT;
        }
    }

    return ret;
}

drvError_t halCheckProcessStatusEx(DVdevice device, processType_t process_type, processStatus_t status,
                                   struct drv_process_status_output *out)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    if (device >= DEVMM_MAX_PHY_DEVICE_NUM || (int)status <= 0) {
        DEVMM_DRV_ERR("Only support devid 0-63, status %d-%d(devid=%u, status=%d)\n", STATUS_NOMEM, STATUS_MAX - 1,
                      device, status);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (process_type != PROCESS_CP1 || status >= STATUS_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (out == NULL) {
        DEVMM_DRV_ERR("Input parameter out is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.head.devid = device;
    arg.data.query_process_status_para.process_type = process_type;
    arg.data.query_process_status_para.pid_status = status;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_PROCESS_STATUS_QUERY, &arg);
    if (ret != 0) {
        DEVMM_DRV_ERR("Query process status error. (ret=%d; devid=%u; processType=%d)\n", ret, device, process_type);
        return ret;
    }

    out->result = arg.data.query_process_status_para.status_result;
    devmm_process_status_result_show(device, process_type, status, out->result);

    return DRV_ERROR_NONE;
}

static pthread_mutex_t g_vmm_handle_lock[DEVMM_MAX_LOGIC_DEVICE_NUM];
static void __attribute__((constructor)) devmm_vmm_handle_lock_init(void)
{
    uint64_t devid;

    for (devid = 0; devid < DEVMM_MAX_LOGIC_DEVICE_NUM; devid++) {
        (void)pthread_mutex_init(&g_vmm_handle_lock[devid], NULL);
    }
}

static drvError_t devmm_giant_page_para_check(const struct drv_mem_prop *prop)
{
    if (prop->side != MEM_DEV_SIDE) {
        if (prop->mem_type != MEM_P2P_DDR_TYPE) {
            DEVMM_RUN_INFO("Only p2p ddr support host giant page. (side=%u; mem_type=%u)\n", prop->side,
                           prop->mem_type);
            return DRV_ERROR_NOT_SUPPORT;
        }

        if (devmm_is_support_host_giant_page_feature() == false) {
            DEVMM_RUN_INFO("Not support host giant page feature.\n");
            return DRV_ERROR_NOT_SUPPORT;
        }
    } else {
        if (devmm_is_support_agent_giant_page_feature(prop->devid) == false) {
            DEVMM_RUN_INFO("Not support agent giant page feature. (devid=%u)\n", prop->devid);
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    if (prop->mem_type == MEM_TS_DDR_TYPE) {
        DEVMM_RUN_INFO("Giant page not support ts_ddr.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_mem_prop_check(const struct drv_mem_prop *prop, bool is_chk_host_id)
{
    if (prop == NULL) {
        DEVMM_DRV_ERR("Prop is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop->side >= MEM_MAX_SIDE) {
        DEVMM_DRV_ERR("Invalid side type. (side=%u)\n", prop->side);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (is_chk_host_id) {
        uint32_t host_id = SVM_MAX_AGENT_NUM;
        (void)halGetHostID(&host_id);
        if ((prop->side != MEM_HOST_NUMA_SIDE) && (prop->devid >= DEVMM_MAX_PHY_DEVICE_NUM) &&
            (prop->devid != host_id)) {
            DEVMM_DRV_ERR("Invalid devid. (devid=%u; host_id=%u)\n", prop->devid, host_id);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else {
        if ((prop->side != MEM_HOST_NUMA_SIDE) && prop->devid >= DEVMM_MAX_PHY_DEVICE_NUM) {
            DEVMM_DRV_ERR("Invalid devid. (devid=%u)\n", prop->devid);
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    if ((prop->pg_type == MEM_NORMAL_PAGE_TYPE) && (prop->side == MEM_HOST_NUMA_SIDE)) {
        DEVMM_RUN_INFO("Host numa not support normal page.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (prop->pg_type >= MEM_MAX_PAGE_TYPE) {
        DEVMM_DRV_ERR("Invalid page type. (page_type=%u)\n", prop->pg_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop->mem_type >= MEM_MAX_TYPE) {
        DEVMM_DRV_ERR("Invalid mem type. (mem_type=%u)\n", prop->mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop->pg_type == MEM_GIANT_PAGE_TYPE) {
        drvError_t ret = devmm_giant_page_para_check(prop);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }

    if (prop->reserve != 0) {
        DEVMM_DRV_ERR("Prop reserve should be zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t devmm_mem_create_para_check(drv_mem_handle_t **handle, size_t size, const struct drv_mem_prop *prop,
                                              uint64_t flag)
{
    drvError_t ret;

    if (handle == NULL) {
        DEVMM_DRV_ERR("Handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (size == 0) {
        DEVMM_DRV_ERR("Size is zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    /* reserved para, verify by zero for subsequent compatibility */
    if (flag != 0) {
        DEVMM_DRV_ERR("Flag should be zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = devmm_mem_prop_check(prop, false);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if ((prop->side == MEM_HOST_SIDE) && (prop->devid != 0)) {
        DEVMM_RUN_INFO("Devid must be 0 in host side.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_vmm_mem_create(const struct drv_mem_prop *prop, size_t size, int *id, uint64_t *pg_num,
                                       uint64_t *pa_pg_size)
{
    uint32_t logic_devid, module_id, page_type, host_numa_id;
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    struct devmm_ioctl_arg arg = {0};
    size_t real_size;
    drvError_t ret;

    if (devmm_is_mem_host_side(prop->side)) {
        (void)halGetHostID(&host_id);
    }

    page_type = (prop->pg_type == MEM_GIANT_PAGE_TYPE) ? MEM_GIANT_PAGE_TYPE : MEM_HUGE_PAGE_TYPE;
    page_type = (prop->pg_type == MEM_NORMAL_PAGE_TYPE) && (size < DEVMM_MEM_ALLOC_MINIMUN_GRANULARITY) &&
                        (prop->module_id == RUNTIME_MODULE_ID) ?
                    MEM_NORMAL_PAGE_TYPE :
                    page_type;
    module_id = (prop->module_id == UNKNOWN_MODULE_ID) ? ASCENDCL_MODULE_ID : prop->module_id;
    logic_devid = devmm_is_mem_host_side(prop->side) ? host_id : prop->devid;
    real_size = (prop->pg_type == MEM_GIANT_PAGE_TYPE) ? align_up(size, DEVMM_GIANT_PAGE_SIZE) : size;
    host_numa_id = (prop->side == MEM_HOST_NUMA_SIDE) ? prop->devid : 0U;

    arg.head.devid = logic_devid;
    arg.data.mem_create_para.size = real_size;
    arg.data.mem_create_para.flag = 0;
    arg.data.mem_create_para.side = prop->side;
    arg.data.mem_create_para.module_id = module_id;
    arg.data.mem_create_para.pg_type = page_type;
    arg.data.mem_create_para.mem_type = prop->mem_type;
    arg.data.mem_create_para.host_numa_id = host_numa_id;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_CREATE, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_RUN_INFO_IF((ret != DRV_ERROR_NOT_SUPPORT),
                          "Mem create completed, "
                          "(ret=%d; size=%u; side=%u; devid=%u; module_id=%u; pg_type=%u; mem_type=%u)\n",
                          ret, real_size, prop->side, logic_devid, module_id, prop->pg_type, prop->mem_type);
        if (devmm_is_mem_host_side(prop->side)) {
            svm_mem_stats_show_host();
        } else {
            svm_mem_stats_show_device(logic_devid);
        }
        return ret;
    }
    *id = arg.data.mem_create_para.id;
    *pg_num = arg.data.mem_create_para.pg_num; /* Just to control msg num of dev mem_release. */
    *pa_pg_size = arg.data.mem_create_para.pa_pg_size;
    return DRV_ERROR_NONE;
}

static int devmm_vmm_create_record_restore(uint64_t key1, uint64_t key2, uint32_t devid, void *data)
{
    (void)key2;
    (void)devid;
    (void)data;
    drv_mem_handle_t *handle = (drv_mem_handle_t *)(uintptr_t)key1;
    struct devmm_virt_heap_mgmt *mgmt = NULL;
    struct drv_mem_prop prop = {0};
    uint32_t page_size;
    uint64_t pg_num;
    int ret, id;

    mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (mgmt == NULL) {
        DEVMM_DRV_ERR("Get heap management error.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    prop.side = handle->side;
    prop.pg_type = handle->pg_type;
    prop.mem_type = handle->phy_mem_type;
    prop.devid = handle->devid;
    prop.module_id = handle->module_id;
    if (prop.pg_type == MEM_GIANT_PAGE_TYPE) {
        page_size = DEVMM_GIANT_PAGE_SIZE;
    } else {
        if (devmm_is_mem_host_side(prop.side) && (prop.pg_type == MEM_NORMAL_PAGE_TYPE)) {
            page_size = mgmt->local_page_size;
        } else {
            page_size = (prop.pg_type == MEM_HUGE_PAGE_TYPE) ? mgmt->huge_page_size : DEVMM_DEV_PAGE_SIZE;
        }
    }
    DEVMM_RUN_INFO("Create restore. (pg_num=%lu; page_size=%u; devid=%u; module_id=%u; pg_type=%u; mem_type=%u; "
                   "side=%u; id=%d)\n",
                   handle->pg_num, page_size, handle->devid, handle->module_id, handle->pg_type, handle->phy_mem_type,
                   handle->side, handle->id);
    ret = devmm_vmm_mem_create(&prop, handle->pg_num * page_size, &id, &pg_num, &handle->pa_pg_size);
    if (ret != 0) {
        return ret;
    }
    handle->id = id;
    return 0;
}

static drvError_t devmm_vmm_pa_alloc(const struct drv_mem_prop *prop, size_t size, uint64_t flag, int *id,
                                     uint64_t *pg_num, uint64_t *pa_pg_size)
{
    struct devmm_virt_heap_mgmt *mgmt = NULL;
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    uint32_t logic_devid, module_id, mem_val;
    struct svm_mem_stats_type type;
    size_t real_size;
    drvError_t ret;

    mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (mgmt == NULL) {
        DEVMM_DRV_ERR("Get heap management error.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }
    if (devmm_is_mem_host_side(prop->side)) {
        (void)halGetHostID(&host_id);
    }
    ret = devmm_vmm_mem_create(prop, size, id, pg_num, pa_pg_size);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    logic_devid = devmm_is_mem_host_side(prop->side) ? host_id : prop->devid;
    real_size = (prop->pg_type == MEM_GIANT_PAGE_TYPE) ? align_up(size, DEVMM_GIANT_PAGE_SIZE) : size;
    module_id = (prop->module_id == UNKNOWN_MODULE_ID) ? ASCENDCL_MODULE_ID : prop->module_id;
    mem_val = devmm_is_mem_host_side(prop->side) ? MEM_HOST_VAL : MEM_DEV_VAL;
    svm_mem_stats_type_pack(&type, mem_val, MEM_HUGE_PAGE_TYPE, prop->mem_type);
    svm_mapped_size_inc(&type, logic_devid, real_size);
    svm_module_alloced_size_inc(&type, logic_devid, module_id, real_size);
    DEVMM_DRV_DEBUG_ARG("Create. (size=%lu; real_size=%lu; devid=%u; module_id=%u; pg_type=%u; mem_type=%u; "
                        "flag=0x%lx; id=%d)\n",
                        size, real_size, logic_devid, prop->module_id, prop->pg_type, prop->mem_type, flag, *id);
    return DRV_ERROR_NONE;
}

static drvError_t devmm_vmm_pa_free(const struct drv_mem_prop *prop, int id, uint64_t pg_num, uint64_t pa_pg_size)
{
    struct devmm_ioctl_arg arg = {0};
    struct svm_mem_stats_type type;
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    uint32_t mem_val, logic_devid;
    size_t size;
    drvError_t ret;

    if (devmm_is_mem_host_side(prop->side)) {
        (void)halGetHostID(&host_id);
    }
    logic_devid = devmm_is_mem_host_side(prop->side) ? host_id : prop->devid;
    arg.head.devid = logic_devid;
    arg.data.mem_release_para.id = id;
    arg.data.mem_release_para.side = prop->side;
    arg.data.mem_release_para.pg_num = pg_num;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_RELEASE, &arg);
    if (ret != DRV_ERROR_NONE) {
        /* The log cannot be modified, because in the failure mode library. */
        DEVMM_DRV_ERR("Mem release failed. (ret=%d; id=%d; side=%u; devid=%u)\n", ret, id, prop->side, logic_devid);
        return ret;
    }

    if (arg.data.mem_release_para.handle_type != SVM_MEM_HANDLE_IMPORT_TYPE) {
        mem_val = devmm_is_mem_host_side(prop->side) ? MEM_HOST_VAL : MEM_DEV_VAL;
        svm_mem_stats_type_pack(&type, mem_val, MEM_HUGE_PAGE_TYPE, prop->mem_type);
        size = pg_num * pa_pg_size;
        svm_mapped_size_dec(&type, logic_devid, size);
        svm_module_alloced_size_dec(&type, logic_devid, prop->module_id, size);
    }
    return ret;
}

static drvError_t devmm_vmm_create_record_data_create(drv_mem_handle_t *handle, const struct drv_mem_prop *prop)
{
    struct devmm_record_data data = {0};
    uint32_t logic_devid;
    drvError_t ret;

    logic_devid = devmm_is_mem_host_side(prop->side) ? DEVMM_RECORD_HOST_DEVID : prop->devid;
    data.key1 = (uint64_t)(uintptr_t)handle;
    data.key2 = DEVMM_RECORD_KEY_INVALID;
    ret = devmm_record_create_and_get(DEVMM_FEATURE_VMM_CREATE, logic_devid, DEVMM_KEY_TYPE1, DEVMM_NODE_INITED, &data);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Record get fail. (ret=%d; side=%u; devid=%u; module_id=%u; pg_type=%u; mem_type=%u)\n", ret,
                      prop->side, prop->devid, prop->module_id, prop->pg_type, prop->mem_type);
        return ret;
    }
    return DRV_ERROR_NONE;
}

static void devmm_vmm_create_record_data_destroy(drv_mem_handle_t *handle)
{
    drvError_t ret;

    ret = devmm_record_put(DEVMM_FEATURE_VMM_CREATE, handle->devid, DEVMM_KEY_TYPE1, (uint64_t)(uintptr_t)handle,
                           DEVMM_NODE_UNINITED);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_WARN("Record put warn. (ret=%d; id=%d; side=%u; devid=%u)\n", ret, handle->id, handle->side,
                       handle->devid);
    }
}

static drv_mem_handle_t *devmm_vmm_pa_handle_create(const struct drv_mem_prop *prop, size_t size, int id,
                                                    uint64_t pg_num, uint64_t pa_pg_size)
{
    drv_mem_handle_t *handle = NULL;
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    uint32_t page_type;
    drvError_t ret;
    int64_t value;

    ret = halGetDeviceInfo((devmm_is_mem_host_side(prop->side) ? 0 : prop->devid), MODULE_TYPE_SYSTEM, INFO_TYPE_SDID,
                           &value);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            DEVMM_DRV_ERR("Get sdid failed. (ret=%d; devid=%u)\n", ret, prop->devid);
            return NULL;
        }
        value = 0;
    }
    if (devmm_is_mem_host_side(prop->side)) {
        (void)halGetHostID(&host_id);
    }

    handle = malloc(sizeof(drv_mem_handle_t));
    if (handle == NULL) {
        DEVMM_DRV_ERR("Malloc handle failed.\n");
        return NULL;
    }

    ret = devmm_vmm_create_record_data_create(handle, prop);
    if (ret != DRV_ERROR_NONE) {
        free(handle);
        return NULL;
    }

    handle->id = id;
    handle->side = devmm_is_mem_host_side(prop->side) ? MEM_HOST_SIDE : MEM_DEV_SIDE;
    handle->sdid = (uint32_t)value;
    handle->devid = devmm_is_mem_host_side(prop->side) ? host_id : prop->devid;
    handle->pg_num = pg_num; /* Just to control msg num of dev mem_release. */
    handle->module_id = (prop->module_id == UNKNOWN_MODULE_ID) ? ASCENDCL_MODULE_ID : prop->module_id;
    page_type = (prop->pg_type == MEM_GIANT_PAGE_TYPE && devmm_is_mem_host_side(prop->side)) ? MEM_GIANT_PAGE_TYPE :
                                                                                               MEM_HUGE_PAGE_TYPE;
    page_type = (prop->pg_type == MEM_NORMAL_PAGE_TYPE) && (size < DEVMM_MEM_ALLOC_MINIMUN_GRANULARITY) &&
                        (prop->module_id == RUNTIME_MODULE_ID) ?
                    MEM_NORMAL_PAGE_TYPE :
                    page_type;
    handle->pg_type = page_type;
    handle->pa_pg_size = pa_pg_size;
    handle->phy_mem_type = prop->mem_type;
    handle->prop = *prop;
    handle->support_restore = true;
    handle->ref = 1;
    handle->map_ref = 0;
    handle->is_share = false;
    return handle;
}

static void devmm_vmm_pa_handle_destroy(drv_mem_handle_t *handle)
{
    devmm_vmm_create_record_data_destroy(handle);
    free(handle);
}

bool svm_support_vmm_normal_granularity(uint32_t dev_id)
{
    (void)dev_id;
    return !devmm_is_split_mode();
}

drvError_t halMemCreate(drv_mem_handle_t **handle, size_t size, const struct drv_mem_prop *prop, uint64_t flag)
{
    drv_mem_handle_t *tmp_handle = NULL;
    uint64_t pg_num;
    uint64_t pa_pg_size;
    drvError_t ret;
    int id;
    uint32_t devid;

    DRV_CURRENT_API_SCOPE();

    ret = devmm_mem_create_para_check(handle, size, prop, flag);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_vmm_pa_alloc(prop, size, flag, &id, &pg_num, &pa_pg_size);
    if (ret != DRV_ERROR_NONE) {
        if (ret == DRV_ERROR_OUT_OF_MEMORY) {
            devid = devmm_is_mem_host_side(prop->side) ? SVM_HOST_AGENT_ID : prop->devid;
            devmm_report_oom(size, halGetMemModuleName(prop->module_id), devid);
        }
        return ret;
    }

    tmp_handle = devmm_vmm_pa_handle_create(prop, size, id, pg_num, pa_pg_size);
    if (tmp_handle == NULL) {
        devmm_vmm_pa_free(prop, id, pg_num, pa_pg_size);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    *handle = tmp_handle;
    return DRV_ERROR_NONE;
}

static drvError_t devmm_mem_handle_check(drv_mem_handle_t *handle)
{
    uint32_t host_id = SVM_MAX_AGENT_NUM;

    (void)halGetHostID(&host_id);

    if (handle == NULL) {
        DEVMM_DRV_ERR("Handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((handle->side != MEM_HOST_SIDE) && (handle->side != MEM_DEV_SIDE)) {
        /* The log cannot be modified, because in the failure mode library. */
        DEVMM_DRV_ERR("Invalid handle side. (side=%u)\n", handle->side);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((handle->side == MEM_HOST_SIDE) && (handle->devid != host_id)) {
        /* The log cannot be modified, because in the failure mode library. */
        DEVMM_DRV_ERR("Host_side's devid should be zero. (devid=%u)\n", handle->devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((handle->side == MEM_DEV_SIDE) && (handle->devid >= DEVMM_MAX_LOGIC_DEVICE_NUM)) {
        /* The log cannot be modified, because in the failure mode library. */
        DEVMM_DRV_ERR("Invalid handle devid. (devid=%u)\n", handle->devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle->pg_type >= MEM_MAX_PAGE_TYPE) {
        DEVMM_DRV_ERR("Invalid page type. (page_type=%u)\n", handle->pg_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle->phy_mem_type >= MEM_MAX_TYPE) {
        DEVMM_DRV_ERR("Invalid mem type. (mem_type=%u)\n", handle->phy_mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t devmm_vmm_handle_ref_inc(drv_mem_handle_t *handle)
{
    drvError_t ret;
    ret = ((handle->ref > UINT32_MAX) || (handle->ref == 0)) ? DRV_ERROR_NO_RESOURCES : DRV_ERROR_NONE;
    if (ret == DRV_ERROR_NONE) {
        handle->ref++;
    }
    return ret;
}

static drvError_t devmm_vmm_handle_ref_dec(drv_mem_handle_t *handle)
{
    if (handle->ref == 0) {
        return DRV_ERROR_INVALID_VALUE;
    }
    handle->ref--;
    return DRV_ERROR_NONE;
}

static uint64_t devmm_vmm_handle_ref_read(drv_mem_handle_t *handle)
{
    return handle->ref;
}

static drvError_t devmm_vmm_map_handle_ref_inc(drv_mem_handle_t *handle)
{
    drvError_t ret;

    if (handle->map_ref > UINT32_MAX) {
        return DRV_ERROR_PARA_ERROR;
    }
    ret = devmm_vmm_handle_ref_inc(handle);
    if (ret == DRV_ERROR_NONE) {
        handle->map_ref++;
    }
    return ret;
}

static drvError_t devmm_vmm_map_handle_ref_dec(drv_mem_handle_t *handle)
{
    drvError_t ret;

    if (handle->map_ref == 0) {
        return DRV_ERROR_PARA_ERROR;
    }
    ret = devmm_vmm_handle_ref_dec(handle);
    if (ret == DRV_ERROR_NONE) {
        handle->map_ref--;
    }
    return ret;
}

static uint64_t devmm_vmm_map_handle_ref_read(drv_mem_handle_t *handle)
{
    return handle->map_ref;
}

static drvError_t devmm_vmm_handle_ref_check(drv_mem_handle_t *handle, bool is_unmap_release)
{
    uint64_t map_ref;
    uint64_t ref;

    map_ref = devmm_vmm_map_handle_ref_read(handle);
    ref = devmm_vmm_handle_ref_read(handle);
    if ((ref == 0) || (ref > UINT32_MAX) || (map_ref > UINT32_MAX) || (is_unmap_release && (map_ref == 0))) {
        DEVMM_DRV_ERR("Handle ref is invalid. (ref=%llu; map_ref=%llu; unmap_release=%u)\n", ref, map_ref,
                      is_unmap_release);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((is_unmap_release == false) && (map_ref != 0) && (ref <= map_ref)) {
        DEVMM_DRV_ERR("Call halMemRelease too many times. (ref=%llu; map_ref=%llu; unmap_release=%u)\n", ref, map_ref,
                      is_unmap_release);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

static drvError_t svm_mem_release(drv_mem_handle_t *handle, bool is_unmap_release)
{
    struct drv_mem_prop prop = {0};
    drvError_t ret;

    prop.side = handle->side;
    prop.devid = handle->devid;
    prop.module_id = handle->module_id;
    prop.pg_type = handle->pg_type;
    prop.mem_type = handle->phy_mem_type;
    (void)pthread_mutex_lock(&g_vmm_handle_lock[handle->devid]);
    ret = devmm_vmm_handle_ref_check(handle, is_unmap_release);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&g_vmm_handle_lock[handle->devid]);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (devmm_vmm_handle_ref_read(handle) > 1) {
        if (is_unmap_release) {
            devmm_vmm_map_handle_ref_dec(handle);
        } else {
            devmm_vmm_handle_ref_dec(handle);
        }
        (void)pthread_mutex_unlock(&g_vmm_handle_lock[handle->devid]);
        return DRV_ERROR_NONE;
    }

    ret = devmm_vmm_pa_free(&prop, handle->id, handle->pg_num, handle->pa_pg_size);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&g_vmm_handle_lock[handle->devid]);
        return ret;
    }
    (void)devmm_record_put(DEVMM_FEATURE_IMPORT, handle->devid, DEVMM_KEY_TYPE2, (uint64_t)handle, DEVMM_NODE_UNINITED);
    if (is_unmap_release) {
        devmm_vmm_map_handle_ref_dec(handle);
    } else {
        devmm_vmm_handle_ref_dec(handle);
    }
    (void)pthread_mutex_unlock(&g_vmm_handle_lock[handle->devid]);

    devmm_vmm_pa_handle_destroy(handle);
    return DRV_ERROR_NONE;
}

drvError_t halMemRelease(drv_mem_handle_t *handle)
{
    drvError_t ret;

    ret = devmm_mem_handle_check(handle);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    DEVMM_DRV_DEBUG_ARG("Release. (devid=%u; module_id=%u; pg_type=%u; mem_type=%u; pg_num=%llu; id=%d)\n",
                        handle->devid, handle->module_id, handle->pg_type, handle->phy_mem_type, handle->pg_num,
                        handle->id);
    return svm_mem_release(handle, false);
}

static uint32_t devmm_size_to_order(size_t size)
{
    size_t tmp_size = size;
    uint32_t order = 0;

    while (tmp_size >>= 1) {
        order++;
    }

    if ((size & ~(1UL << order)) != 0) {
        order++;
    }
    return order;
}

static drvError_t devmm_mem_address_reserve_addr_size_check(uint64_t addr, size_t size)
{
    size_t align_size = DEVMM_MEM_ALLOC_RECOMMENDED_GRANULARITY;

    if (addr == 0) {
        return DRV_ERROR_NONE;
    }

    if (size > DEVMM_LARGE_MEM_THRESHOLD_SIZE) {
        align_size = DEVMM_HEAP_SIZE;
    } else if (size > DEVMM_MEM_ALLOC_RECOMMENDED_GRANULARITY) {
        align_size = 1UL << devmm_size_to_order(size);
    }

    if (IS_ALIGNED(addr, align_size) == false) {
        DEVMM_DRV_ERR("Addr should align. (addr=0x%llx; size=%lu; align_size=%lu)\n", addr, size, align_size);
        devmm_report_addr_not_aligned("addr", addr, align_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t devmm_mem_address_reserve_para_check(void **ptr, size_t size, size_t alignment, uint64_t addr,
                                                       uint64_t flag)
{
    if (ptr == NULL) {
        DEVMM_DRV_ERR("Ptr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (size == 0) {
        DEVMM_DRV_ERR("Size is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    /* reserved para, verify by zero for subsequent compatibility */
    if (alignment != 0) {
        devmm_report_para_only_zero("alignment", (unsigned long long)alignment);
        DEVMM_DRV_ERR("Alignment should be zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (devmm_mem_address_reserve_addr_size_check(addr, size) != DRV_ERROR_NONE) {
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((flag & 0xFF) >= MEM_MAX_PAGE_TYPE) { /* 0xFF, pgtype mask */
        DEVMM_DRV_ERR("Invalid pg type. (flag=%llu)\n", flag);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((((flag & MEM_RSV_TYPE_REMOTE_MAP) != 0) || ((flag & MEM_RSV_TYPE_DEVICE_SHARE) != 0)) && (addr == 0)) {
        DEVMM_RUN_INFO("Only support specified address reserve. (flag=0x%llx)\n", flag);
        return DRV_ERROR_NOT_SUPPORT;
    }

    /* remote_map/device_share require specified address, host_uva requires non-specified */
    if ((flag & MEM_RSV_TYPE_HOST_UVA) != 0) {
        if (addr != 0) {
            DEVMM_RUN_INFO("Host uva not support specified address. (addr=0x%llx)\n", addr);
            return DRV_ERROR_NOT_SUPPORT;
        }
        if ((flag & (MEM_RSV_TYPE_REMOTE_MAP | MEM_RSV_TYPE_DEVICE_SHARE)) != 0) {
            DEVMM_RUN_INFO("Host uva conflict with remote_map or device_share. (flag=0x%llx)\n", flag);
            return DRV_ERROR_NOT_SUPPORT;
        }
    }
    return DRV_ERROR_NONE;
}

static int svm_vmm_dynamic_reserve(struct devmm_virt_heap_mgmt *mgmt, uint64_t *va, uint64_t size, DVmem_advise advise,
                                   uint64_t flag)
{
    uint32_t da_flag = ((flag & MEM_RSV_TYPE_DEVICE_SHARE) != 0) ? 0 : SVM_DA_FLAG_WITH_MASTER;
    struct devmm_virt_com_heap heap = {0};
    uint32_t heap_idx;
    int ret;

    ret = svm_da_alloc(va, size, da_flag);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Da alloc failed. (va=0x%llx; size=0x%llx; da_flag=%x)\n", *va, size, da_flag);
        return ret;
    }

    heap_idx = devmm_va_to_heap_idx(mgmt, (virt_addr_t)*va, false);
    ret = devmm_ioctl_enable_heap(heap_idx, DEVMM_HEAP_HUGE_PAGE, SUB_RESERVE_TYPE, size, RESERVE_LIST, false);
    if (ret != DRV_ERROR_NONE) {
        (void)svm_da_free(*va);
        DEVMM_DRV_ERR("Enable heap failed. (heap_idx=%u; va=0x%llx; size=0x%llx)\n", heap_idx, *va, size);
        return ret;
    }

    heap.heap_sub_type = SUB_RESERVE_TYPE;
    if (devmm_virt_heap_alloc_ops(&heap, *va, size, advise) < DEVMM_SVM_MEM_START) {
        DEVMM_DRV_ERR("Alloc heap ops failed. (heap_idx=%u; va=0x%llx; size=0x%llx; advise=%u)\n", heap_idx, *va, size,
                      advise);
        (void)devmm_ioctl_disable_heap(heap_idx, DEVMM_HEAP_HUGE_PAGE, SUB_RESERVE_TYPE, size, false);
        (void)svm_da_free(*va);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    return DRV_ERROR_NONE;
}

static int svm_vmm_dynamic_free(struct devmm_virt_heap_mgmt *mgmt, uint64_t va)
{
    struct devmm_virt_com_heap heap = {0};
    uint64_t size;
    uint32_t heap_idx;
    int ret;

    ret = svm_da_query_size(va, &size);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Query size failed. (va=0x%llx)\n", va);
        return ret;
    }

    heap.heap_sub_type = SUB_RESERVE_TYPE;
    ret = devmm_virt_heap_free_ops(&heap, va);
    if (ret != 0) {
        DEVMM_DRV_ERR("Free heap ops failed. (va=0x%llx; size=0x%llx)\n", va, size);
        return DRV_ERROR_IOCRL_FAIL;
    }

    heap_idx = devmm_va_to_heap_idx(mgmt, (virt_addr_t)va, false);
    ret = devmm_ioctl_disable_heap(heap_idx, DEVMM_HEAP_HUGE_PAGE, SUB_RESERVE_TYPE, size, false);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Disable heap failed. (va=0x%llx; size=0x%llx)\n", va, size);
        return ret;
    }

    (void)svm_da_free(va);
    return DRV_ERROR_NONE;
}

drvError_t halMemAddressReserve(void **ptr, size_t size, size_t alignment, void *addr, uint64_t flag)
{
    struct devmm_virt_heap_mgmt *mgmt = NULL;
    DVmem_advise advise = {0};
    size_t real_size = size;
    drvError_t ret;

    DRV_CURRENT_API_SCOPE();

    ret = devmm_mem_address_reserve_para_check(ptr, size, alignment, (uint64_t)(uintptr_t)addr, flag);
    if (ret != 0) {
        return ret;
    }

    mgmt = devmm_virt_get_heap_mgmt();
    if (mgmt == NULL) {
        DEVMM_DRV_ERR("Get heap management error.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    *ptr = addr;
    advise = (addr != NULL) ? DV_ADVISE_NOCACHE : 0;
    if (size >= DEVMM_MEM_ALLOC_MINIMUN_GRANULARITY) {
        advise |= DV_ADVISE_HUGEPAGE;
    } else {
        real_size = DEVMM_MEM_ALLOC_MINIMUN_GRANULARITY; /* Ensure va alignment according to page_size */
    }

    /* host uva: route to base heap host_pin via DV_ADVISE_HOST_UVA */
    if ((flag & MEM_RSV_TYPE_HOST_UVA) != 0) {
        advise |= DV_ADVISE_HOST_UVA;
    }

    if ((flag & MEM_RSV_TYPE_REMOTE_MAP) != 0) {
        return svm_vmm_dynamic_reserve(mgmt, (uint64_t *)ptr, (uint64_t)real_size, advise, flag);
    }

    ret = devmm_alloc_proc(0, SUB_RESERVE_TYPE, advise, real_size, (DVdeviceptr *)ptr);
    DEVMM_DRV_DEBUG_ARG("Reserve. (ptr=0x%llx; size=%lu; real_size=%lu; addr=0x%llx; flag=0x%llx; ret=%u)\n",
                        *(DVdeviceptr *)ptr, size, real_size, (DVdeviceptr)(uintptr_t)addr, flag, ret);
    return ret;
}

drvError_t halMemAddressFree(void *ptr)
{
    struct devmm_virt_heap_mgmt *p_heap_mgmt = NULL;
    struct devmm_virt_com_heap *heap = NULL;
    u64 va = (DVdeviceptr)(uintptr_t)ptr;
    u64 free_len;
    drvError_t ret;

    DRV_CURRENT_API_SCOPE();

    DEVMM_DRV_DEBUG_ARG("Free. (ptr=0x%llx)\n", va);
    p_heap_mgmt = (struct devmm_virt_heap_mgmt *)devmm_virt_get_heap_mgmt();
    if (p_heap_mgmt == NULL) {
        DEVMM_DRV_ERR("Please call device_open and alloc_mem api first. (alloc_mem=0x%llx)\n", va);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (va >= DEVMM_MAX_DYN_ALLOC_BASE) {
        ret = svm_vmm_dynamic_free(p_heap_mgmt, va);
        if (ret == DRV_ERROR_BUSY) {
            devmm_report_mem_in_use((unsigned long long)va);
        }
        return ret;
    }

    heap = devmm_va_to_heap(va);
    if ((heap == NULL) || heap->heap_type == DEVMM_HEAP_IDLE) {
        DEVMM_DRV_ERR("Address is not allocated. please check ptr. (offset=%llx)\n", ADDR_TO_OFFSET(va));
        devmm_report_addr_not_allocated("ptr", va);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (heap->heap_sub_type != SUB_RESERVE_TYPE) {
        DEVMM_DRV_ERR("Is not reserve addr. (va=0x%llx)\n", va);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (devmm_virt_heap_is_primary(heap)) {
        ret = devmm_free_to_base_heap(p_heap_mgmt, heap, va);
    } else {
        ret = devmm_free_to_normal_heap(p_heap_mgmt, heap, va, (uint64_t *)(uintptr_t)&free_len);
    }
    if (ret == DRV_ERROR_BUSY) {
        devmm_report_mem_in_use((unsigned long long)va);
    }
    return ret;
}

static int devmm_mem_set_access(void *ptr, size_t size, struct drv_mem_location location, drv_mem_access_type type)
{
    struct devmm_ioctl_arg arg = {0};
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    int ret;

    (void)halGetHostID(&host_id);

    arg.data.mem_set_access_para.va = (uint64_t)(uintptr_t)ptr;
    arg.data.mem_set_access_para.size = size;
    arg.data.mem_set_access_para.logic_devid = (location.side == MEM_DEV_SIDE) ? location.id : host_id;
    arg.data.mem_set_access_para.type = type;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_SET_ACCESS, &arg);
    if (ret != DRV_ERROR_NONE) {
        /* not rollback */
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT), "Set access failed. (ret=%d; ptr=%p; size=%llu)\n", ret, ptr,
                         (uint64_t)size);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int devmm_mem_get_access(void *ptr, size_t *size, struct drv_mem_location *location, drv_mem_access_type *type)
{
    struct devmm_ioctl_arg arg = {0};
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    int ret;

    (void)halGetHostID(&host_id);

    arg.data.mem_get_access_para.va = (uint64_t)(uintptr_t)ptr;
    arg.data.mem_get_access_para.size = (uint64_t)*size;
    arg.data.mem_get_access_para.logic_devid = (location->side == MEM_DEV_SIDE) ? location->id : host_id;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_GET_ACCESS, &arg);
    if (ret != DRV_ERROR_NONE) {
        /* not rollback */
        DEVMM_DRV_ERR("Get access failed. (ret=%d; ptr=%p; size=%llu)\n", ret, ptr, (uint64_t)(uintptr_t)size);
        return ret;
    }

    *type = arg.data.mem_get_access_para.type;
    *size = (size_t)arg.data.mem_get_access_para.size;

    return DRV_ERROR_NONE;
}

static int devmm_mem_map_para_check(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle, uint64_t flag)
{
    int ret;

    if (ptr == NULL) {
        DEVMM_DRV_ERR("Ptr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (size == 0) {
        DEVMM_DRV_ERR("Size is 0.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    /* reserved para, verify by zero for subsequent compatibility */
    if (offset != 0) {
        DEVMM_DRV_ERR("Offset should be zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    /* reserved para, verify by zero for subsequent compatibility */
    if (flag != 0) {
        DEVMM_DRV_ERR("Flag should be zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = devmm_mem_handle_check(handle);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    /* host uva only map host physical address */
    if (devmm_is_in_host_pin_range((uint64_t)(uintptr_t)ptr) &&
        ((handle->prop.side != MEM_HOST_SIDE) && (handle->prop.side != MEM_HOST_NUMA_SIDE))) {
        DEVMM_RUN_INFO("Host uva only support host physical. (ptr=0x%llx; side=%u)\n", (uint64_t)(uintptr_t)ptr,
                       handle->prop.side);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

struct devmm_record_vmm_map_data {
    uint64_t size;
    uint64_t offset;
    drv_mem_handle_t *handle;
    uint64_t flag;
    uint64_t vmm_map_flag; /* Initial map semantics, immutable until unmap. */
};

static bool devmm_vmm_map_flag_is_owner_no_access(uint64_t vmm_map_flag)
{
    return (vmm_map_flag & DEVMM_VMM_MAP_FLAG_OWNER_NO_ACCESS) != 0;
}

static drvError_t devmm_mem_map(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle, uint64_t flag,
                                uint64_t vmm_map_flag)
{
    (void)offset;
    (void)flag;
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    arg.head.devid = handle->devid;
    arg.data.mem_map_para.va = (uint64_t)(uintptr_t)ptr;
    arg.data.mem_map_para.size = size;
    arg.data.mem_map_para.id = handle->id;
    arg.data.mem_map_para.side = handle->side;
    arg.data.mem_map_para.module_id = handle->module_id;
    arg.data.mem_map_para.pg_num = handle->pg_num;
    arg.data.mem_map_para.pg_type = handle->pg_type;
    arg.data.mem_map_para.vmm_map_flag = vmm_map_flag;
    arg.data.mem_map_para.pa_pg_size = handle->pa_pg_size;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_MAP, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Mem map failed. (ret=%d; ptr=0x%llx; size=%llu; id=%d; side=%u; devid=%u)\n", ret,
                      (uint64_t)(uintptr_t)ptr, (uint64_t)size, handle->id, handle->side, handle->devid);
        return ret;
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_mem_unmap(void *ptr, uint32_t *devid)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    arg.data.mem_unmap_para.va = (uint64_t)(uintptr_t)ptr;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_UNMAP, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Mem unmap failed. (ret=%d; ptr=0x%llx)\n", ret, (uint64_t)(uintptr_t)ptr);
    }

    *devid = arg.data.mem_unmap_para.logic_devid;
    if (arg.data.mem_unmap_para.side == MEM_DEV_SIDE) {
        uint64_t unmap_size = arg.data.mem_unmap_para.unmap_size;
        devmm_del_mem_advise_record(false, (uint64_t)(uintptr_t)ptr, unmap_size, *devid);
    }

    return ret;
}

static int devmm_vmm_map_record_restore(uint64_t key1, uint64_t key2, uint32_t devid, void *data)
{
    (void)key2;
    (void)devid;
    struct devmm_record_vmm_map_data *map_data = data;
    drv_mem_handle_t *handle = map_data->handle;

    if (handle->support_restore == false) {
        return 0;
    }

    DEVMM_RUN_INFO("Map restore. (va=0x%llx; size=%lu; offset=%lu; flag=0x%lx; id=%d; devid=%u; side=%u)\n", key1,
                   map_data->size, map_data->offset, map_data->flag, handle->id, handle->devid, handle->side);
    return devmm_mem_map((void *)key1, map_data->size, map_data->offset, handle, map_data->flag,
                         map_data->vmm_map_flag);
}

static drvError_t devmm_vmm_map_record_data_create(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle,
                                                   uint64_t flag, uint64_t vmm_map_flag)
{
    uint64_t data_len = sizeof(struct devmm_record_vmm_map_data);
    struct devmm_record_vmm_map_data map_data = {0};
    struct devmm_record_data data = {0};
    drvError_t ret;

    map_data.size = size;
    map_data.offset = offset;
    map_data.handle = handle;
    map_data.flag = flag;
    map_data.vmm_map_flag = vmm_map_flag;

    data.key1 = (uint64_t)(uintptr_t)ptr;
    data.key2 = DEVMM_RECORD_KEY_INVALID;
    data.data_len = data_len;
    data.data = (void *)&map_data;
    ret = devmm_record_create_and_get(DEVMM_FEATURE_VMM_MAP, handle->devid, DEVMM_KEY_TYPE1, DEVMM_NODE_INITED, &data);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Record get failed. (ret=%d; ptr=0x%llx; size=%llu; id=%d; side=%u; devid=%u)\n", ret,
                      (uint64_t)(uintptr_t)ptr, (uint64_t)size, handle->id, handle->side, handle->devid);
        return ret;
    }
    return DRV_ERROR_NONE;
}

static void devmm_vmm_map_record_data_destroy(void *ptr, uint32_t devid)
{
    uint64_t va = (uint64_t)(uintptr_t)ptr;
    drvError_t ret;

    ret = devmm_record_put(DEVMM_FEATURE_VMM_MAP, devid, DEVMM_KEY_TYPE1, va, DEVMM_NODE_UNINITED);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_WARN("Record put warn. (ret=%d; ptr=0x%llx)\n", ret, va);
    }
}

static drvError_t devmm_vmm_map_record_data_get(void *ptr, uint32_t devid, drv_mem_handle_t **handle)
{
    uint64_t data_len = sizeof(struct devmm_record_vmm_map_data);
    struct devmm_record_vmm_map_data map_data = {0};
    struct devmm_record_data data = {0};
    drvError_t ret;

    data.key1 = (uint64_t)(uintptr_t)ptr;
    data.key2 = DEVMM_RECORD_KEY_INVALID;
    data.data_len = data_len;
    data.data = (void *)&map_data;
    ret = devmm_record_get(DEVMM_FEATURE_VMM_MAP, devid, DEVMM_KEY_TYPE1, &data);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Record get failed. (ret=%d; ptr=0x%llx; devid=%u)\n", ret, (uint64_t)(uintptr_t)ptr, devid);
        return ret;
    }
    *handle = map_data.handle;
    return DRV_ERROR_NONE;
}

static void devmm_vmm_map_record_data_put(void *ptr, uint32_t devid)
{
    uint64_t va = (uint64_t)(uintptr_t)ptr;
    drvError_t ret;

    ret = devmm_record_put(DEVMM_FEATURE_VMM_MAP, devid, DEVMM_KEY_TYPE1, va, DEVMM_NODE_UNINITED);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_WARN("Record put warn. (ret=%d; ptr=0x%llx)\n", ret, va);
    }
}

static drvError_t devmm_vmm_map_no_access_check(const drv_mem_handle_t *handle)
{
    uint32_t host_devid = SVM_MAX_AGENT_NUM;
    uint32_t device_devid;
    int64_t connect_type;
    drvError_t ret;

    if (handle->side != MEM_HOST_SIDE) {
        return DRV_ERROR_NONE;
    }

    (void)halGetHostID(&host_devid);
    device_devid = (handle->devid == host_devid) ? 0 : handle->devid;
    ret = halGetDeviceInfo(device_devid, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, &connect_type);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        DEVMM_DRV_ERR("Get host-device connect type failed. (ret=%d; devid=%u)\n", ret, device_devid);
        return ret;
#endif
    }

    if (connect_type == HOST_DEVICE_CONNECT_TYPE_PCIE) {
#ifndef EMU_ST
        DEVMM_RUN_INFO("Host memory map no access is not supported on PCIe. (devid=%u)\n", device_devid);
        return DRV_ERROR_NOT_SUPPORT;
#endif
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_hal_mem_map(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle, uint64_t flag,
                                    uint64_t vmm_map_flag)
{
    drvError_t ret;

    ret = devmm_mem_map_para_check(ptr, size, offset, handle, flag);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }
#ifndef EMU_ST /* For ut test, delete later. */
    if (devmm_vmm_map_flag_is_owner_no_access(vmm_map_flag)) {
        ret = devmm_vmm_map_no_access_check(handle);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }
    }
#endif
    DEVMM_DRV_DEBUG_ARG("%s. (ptr=0x%llx; size=%lu; offset=%u; id=%d; flag=0x%llx)\n",
                        devmm_vmm_map_flag_is_owner_no_access(vmm_map_flag) ? "Map no access" : "Map",
                        (DVdeviceptr)(uintptr_t)ptr, size, offset, handle->id, flag);
    ret = devmm_vmm_map_record_data_create(ptr, size, offset, handle, flag, vmm_map_flag);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    (void)pthread_mutex_lock(&g_vmm_handle_lock[handle->devid]);
    ret = devmm_vmm_map_handle_ref_inc(handle);
    (void)pthread_mutex_unlock(&g_vmm_handle_lock[handle->devid]);
    if (ret != DRV_ERROR_NONE) {
        devmm_vmm_map_record_data_destroy(ptr, handle->devid);
        return ret;
    }

    ret = devmm_mem_map(ptr, size, offset, handle, flag, vmm_map_flag);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_lock(&g_vmm_handle_lock[handle->devid]);
        devmm_vmm_map_handle_ref_dec(handle);
        (void)pthread_mutex_unlock(&g_vmm_handle_lock[handle->devid]);
        devmm_vmm_map_record_data_destroy(ptr, handle->devid);
        return ret;
    }

    /* For drvMemGetAttribute, quickly get side and devid, won't fail, just save not clear, kernel will verify. */
    (void)svm_save_map_info((uint64_t)(uintptr_t)ptr, size, handle->side, handle->devid, handle->is_share);
    return DRV_ERROR_NONE;
}

drvError_t halMemMap(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle, uint64_t flag)
{
    return devmm_hal_mem_map(ptr, size, offset, handle, flag, 0);
}

drvError_t halMemMapNoAccess(void *ptr, size_t size, size_t offset, drv_mem_handle_t *handle, uint64_t flag)
{
    return devmm_hal_mem_map(ptr, size, offset, handle, flag, DEVMM_VMM_MAP_FLAG_OWNER_NO_ACCESS);
}

drvError_t halMemUnmap(void *ptr)
{
    drv_mem_handle_t *handle = NULL;
    drvError_t ret;
    uint32_t devid;

    DRV_CURRENT_API_SCOPE();

    DEVMM_DRV_DEBUG_ARG("Unmap. (ptr=0x%llx)\n", (DVdeviceptr)(uintptr_t)ptr);
    ret = halMemRetainAllocationHandle(&handle, ptr);
    if (ret != DRV_ERROR_NONE) {
        /* The log cannot be modified, because in the failure mode library. */
        DEVMM_DRV_ERR("Addr can not unmap. (ret=%d; ptr=0x%llx)\n", ret, ptr);
        devmm_report_addr_not_allocated("ptr", (uint64_t)(uintptr_t)ptr);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = devmm_mem_unmap(ptr, &devid);
    if (ret != DRV_ERROR_NONE) {
        halMemRelease(handle);
        return ret;
    }

    svm_del_map_info((uint64_t)(uintptr_t)ptr);

    (void)pthread_mutex_lock(&g_vmm_handle_lock[devid]);
    devmm_vmm_map_record_data_destroy(ptr, devid);
    (void)pthread_mutex_unlock(&g_vmm_handle_lock[devid]);
    svm_mem_release(handle, true); /* ref dec */
    halMemRelease(handle);
    return DRV_ERROR_NONE;
}

drvError_t halMemRetainAllocationHandle(drv_mem_handle_t **handle, void *ptr)
{
    struct DVattribute attr = {0};
    drv_mem_handle_t *tmp_handle = NULL;
    uint64_t va = (uint64_t)(uintptr_t)ptr;
    drvError_t ret;

    if ((handle == NULL) || (ptr == NULL)) {
        DEVMM_DRV_ERR("Invalid para. (handle=%d; ptr=%d)\n", (handle == NULL), (ptr == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = devmm_mem_get_attribute(va, &attr);
    if (ret != 0) {
        DEVMM_DRV_ERR("Va is invalid. (va=0x%llx; ret=%d)\n", va, ret);
        return ret;
    }

    (void)pthread_mutex_lock(&g_vmm_handle_lock[attr.devId]);
    ret = devmm_vmm_map_record_data_get(ptr, attr.devId, &tmp_handle);
    if (ret != DRV_ERROR_NONE) {
        (void)pthread_mutex_unlock(&g_vmm_handle_lock[attr.devId]);
        DEVMM_DRV_ERR("Get record data failed. (va=0x%llx; ret=%d)\n", va, ret);
        return ret;
    }
    ret = devmm_vmm_handle_ref_inc(tmp_handle);
    if (ret == DRV_ERROR_NONE) {
        *handle = tmp_handle;
        DEVMM_DRV_DEBUG_ARG("Retain. (ptr=0x%llx; pg_num=%llu; side=%u; id=%d; devid=%u)\n",
                            (DVdeviceptr)(uintptr_t)ptr, tmp_handle->pg_num, tmp_handle->side, tmp_handle->id,
                            tmp_handle->devid);
    }
    devmm_vmm_map_record_data_put(ptr, attr.devId);
    (void)pthread_mutex_unlock(&g_vmm_handle_lock[attr.devId]);
    return ret;
}

static bool svm_is_mem_access_type_valid(drv_mem_access_type type)
{
    return ((type == MEM_ACCESS_TYPE_NONE) || (type == MEM_ACCESS_TYPE_READ) || (type == MEM_ACCESS_TYPE_READWRITE));
}

static drvError_t svm_mem_set_access(void *ptr, size_t size, struct drv_mem_location location, drv_mem_access_type type)
{
    if ((ptr == NULL) || (location.side >= MEM_MAX_SIDE) || (!svm_is_mem_access_type_valid(type))) {
        DEVMM_DRV_ERR("Invalid para. (side=%u; type=%u)\n", location.side, type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return devmm_mem_set_access(ptr, size, location, type);
}

static int devmm_access_location_check(struct drv_mem_location *location)
{
    if (location->side == MEM_HOST_SIDE) {
        if (location->id != 0) {
            DEVMM_DRV_ERR("Invalid host id. (id=%u)\n", location->id);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else if (location->side == MEM_DEV_SIDE) {
        if (location->id >= DEVMM_MAX_PHY_DEVICE_NUM) {
            DEVMM_DRV_ERR("Invalid dev id. (id=%u)\n", location->id);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else {
        DEVMM_DRV_ERR("Invalid side. (side=%u)\n", location->side);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static int devmm_access_desc_check(struct drv_mem_access_desc *desc, size_t count)
{
    size_t i;

    if ((desc == NULL) || (count == 0)) {
        DEVMM_DRV_ERR("Invalid desc. (count=%u)\n", (uint32_t)count);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < count; i++) {
        int ret;

        if (desc[i].type == MEM_ACCESS_TYPE_READ) {
            DEVMM_RUN_INFO("Not support type. (index=%u; type=%u)\n", i, desc[i].type);
            return DRV_ERROR_NOT_SUPPORT;
        }

        if ((desc[i].type != MEM_ACCESS_TYPE_NONE) && (desc[i].type != MEM_ACCESS_TYPE_READ) &&
            (desc[i].type != MEM_ACCESS_TYPE_READWRITE)) {
            DEVMM_DRV_ERR("Invalid type. (index=%u; type=%u)\n", i, desc[i].type);
            return DRV_ERROR_INVALID_VALUE;
        }

        ret = devmm_access_location_check(&desc[i].location);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Invalid location. (index=%u)\n", i);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t halMemSetAccess(void *ptr, size_t size, struct drv_mem_access_desc *desc, size_t count)
{
    drvError_t ret;
    size_t i;

    if (ptr == NULL) {
        DEVMM_DRV_ERR("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = devmm_access_desc_check(desc, count);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    /* host uva not support set access to share with other devices */
    if (devmm_is_in_host_pin_range((uint64_t)(uintptr_t)ptr)) {
        DEVMM_RUN_INFO("Host uva not support set access. (ptr=0x%llx)\n", (uint64_t)(uintptr_t)ptr);
        return DRV_ERROR_NOT_SUPPORT;
    }

    for (i = 0; i < count; i++) {
        drv_mem_access_type type;
        size_t get_size = size;

        ret = devmm_mem_get_access(ptr, &get_size, &desc[i].location, &type);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Get access failed. (ret=%d; ptr=%p; size=0x%llx)\n", ret, ptr, (uint64_t)size);
            return ret;
        }

        if (get_size != size) {
            DEVMM_DRV_ERR("Set access size doesn't match mapped range. (ptr=%p; size=0x%llx; map_size=0x%llx)\n", ptr,
                          (uint64_t)size, (uint64_t)get_size);
            return DRV_ERROR_PARA_ERROR;
        }

        if (desc[i].type == type) {
            continue;
        }

        if ((type != MEM_ACCESS_TYPE_NONE) && (desc[i].type != MEM_ACCESS_TYPE_NONE)) {
            DEVMM_RUN_INFO("Repeat set with different type. (index=%u; current_type=%u; new_type=%u)\n", (uint32_t)i,
                           (uint32_t)type, (uint32_t)desc[i].type);
            return DRV_ERROR_BUSY;
        }

        ret = svm_mem_set_access(ptr, size, desc[i].location, desc[i].type);
        if (ret != DRV_ERROR_NONE) { /* not need rollback */
            return ret;
        }
    }
    return DRV_ERROR_NONE;
}

drvError_t halMemGetAccess(void *ptr, struct drv_mem_location *location, uint64_t *flags)
{
    drv_mem_access_type type;
    size_t size = 1;
    drvError_t ret;

    if ((ptr == NULL) || (flags == NULL) || (location == NULL)) {
        DEVMM_DRV_ERR("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = devmm_access_location_check(location);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = devmm_mem_get_access(ptr, &size, location, &type);
    if (ret == DRV_ERROR_NONE) {
        *flags = (uint64_t)type;
    }

    return ret;
}

#define DEVMM_SHARE_HANDLE_DEVID_OFFSET 32
#define DEVMM_SHARE_HANDLE_DEVID_MASK 0xff
#define DEVMM_SHARE_HANDLE_ID_MASK 0xffffffff
/* shareable_handle(64 bit) = reserve(24 bit) + share_devid(8 bit) + share_id(32 bit) */
static void devmm_share_handle_pack(uint64_t *shareable_handle, int share_id, uint32_t share_devid)
{
    *shareable_handle = (uint64_t)share_id;
    *shareable_handle |= ((uint64_t)share_devid << DEVMM_SHARE_HANDLE_DEVID_OFFSET);
}

static int devmm_get_share_id(uint64_t shareable_handle)
{
    return (int)(shareable_handle & DEVMM_SHARE_HANDLE_ID_MASK);
}

static uint32_t devmm_get_share_devid(uint64_t shareable_handle)
{
    return (uint32_t)((shareable_handle >> DEVMM_SHARE_HANDLE_DEVID_OFFSET) & DEVMM_SHARE_HANDLE_DEVID_MASK);
}

static drvError_t devmm_mem_export_para_check(drv_mem_handle_t *handle)
{
    uint32_t host_id = SVM_MAX_AGENT_NUM;

    (void)halGetHostID(&host_id);

    if (devmm_is_split_mode()) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (handle == NULL) {
        DEVMM_DRV_ERR("Handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((handle->side != MEM_HOST_SIDE) && (handle->side != MEM_DEV_SIDE)) {
        DEVMM_DRV_ERR("Invalid handle side. (side=%u)\n", handle->side);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (((handle->side == MEM_HOST_SIDE) && (handle->devid != host_id)) ||
        ((handle->side == MEM_DEV_SIDE) && (handle->devid >= SVM_MAX_AGENT_NUM))) {
#ifndef EMU_ST
        DEVMM_DRV_ERR("Invalid handle devid. (devid=%u)\n", handle->devid);
        return DRV_ERROR_INVALID_VALUE;
#endif
    }

    return DRV_ERROR_NONE;
}

struct devmm_share_handle {
    drv_mem_handle_type handle_type;
    uint32_t sdid;
    uint32_t devid;
    uint32_t phy_devid; /* cross server use */
    int share_id;
};

static drvError_t devmm_export_to_share_handle(drv_mem_handle_t *handle, drv_mem_handle_type handle_type,
                                               uint64_t flags, struct devmm_share_handle *share_handle)
{
    (void)flags;
    struct devmm_ioctl_arg arg = {0};
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    uint32_t phy_devid;
    drvError_t ret;

    ret = devmm_mem_export_para_check(handle);
    if (ret != 0) {
        return ret;
    }

    (void)halGetHostID(&host_id);

    if (handle->devid == host_id) {
        phy_devid = host_id;
    } else {
        ret = drvDeviceGetPhyIdByIndex(handle->devid, &phy_devid);
        if (ret != DRV_ERROR_NONE) {
            DEVMM_DRV_ERR("Get phy devid failed. (ret=%d; devid=%u; id=%d)\n", ret, handle->devid, handle->id);
            return ret;
        }
    }

    arg.head.devid = handle->devid;
    arg.data.mem_export_para.side = (int)handle->side;
    arg.data.mem_export_para.id = handle->id;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_EXPORT, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Mem export failed. (ret=%d; devid=%u; id=%d)\n", ret, handle->devid, handle->id);
        return ret;
    }

    handle->support_restore = true;
    share_handle->handle_type = handle_type;
    share_handle->sdid = handle->sdid;
    share_handle->devid = handle->devid;
    share_handle->phy_devid = phy_devid;
    share_handle->share_id = arg.data.mem_export_para.share_id;

    return DRV_ERROR_NONE;
}

drvError_t halMemExportToShareableHandleV2(drv_mem_handle_t *handle, drv_mem_handle_type handle_type, uint64_t flags,
                                           struct MemShareHandle *share_handle)
{
    drvError_t ret;

    if (share_handle == NULL) {
        DEVMM_DRV_ERR("share_handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_type >= MEM_HANDLE_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (flags != 0) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_export_to_share_handle(handle, handle_type, flags, (struct devmm_share_handle *)(void *)share_handle);
    if (ret != 0) {
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t halMemExportToShareableHandle(drv_mem_handle_t *handle, drv_mem_handle_type handle_type, uint64_t flags,
                                         uint64_t *shareable_handle)
{
    struct devmm_share_handle share_handle;
    drvError_t ret;

    if (shareable_handle == NULL) {
        DEVMM_DRV_ERR("shareable_handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle_type != MEM_HANDLE_TYPE_NONE) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_export_to_share_handle(handle, handle_type, flags, &share_handle);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    devmm_share_handle_pack(shareable_handle, share_handle.share_id, handle->devid);
    DEVMM_DRV_DEBUG_ARG("Mem export. (devid=%u; id=%d; share_id=%d; shareableHandle=%llu)\n", handle->devid, handle->id,
                        share_handle.share_id, *shareable_handle);

    return DRV_ERROR_NONE;
}

/* trans struct MemShareHandle to u64 shareable_handle(devid + share id), to set/get attribute in local */
drvError_t halMemTransShareableHandle(drv_mem_handle_type handle_type, struct MemShareHandle *share_handle,
                                      uint32_t *server_id, uint64_t *shareable_handle)
{
    struct devmm_share_handle *_share_handle = (struct devmm_share_handle *)(void *)share_handle;
    struct halSDIDParseInfo sdid_parse;
    drvError_t ret;

    if ((share_handle == NULL) || (server_id == NULL) || (shareable_handle == NULL)) {
        DEVMM_DRV_ERR("handle is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((_share_handle->handle_type != handle_type) || (handle_type >= MEM_HANDLE_TYPE_MAX)) {
        DEVMM_DRV_ERR("Invalid para. (handle_type=%u; share_handle.handle_type=%u)\n", handle_type,
                      _share_handle->handle_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = halParseSDID(_share_handle->sdid, &sdid_parse);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Parse sdid failed. (handle_type=%u; sdid=%u)\n", handle_type, _share_handle->sdid);
        return ret;
    }

    devmm_share_handle_pack(shareable_handle, _share_handle->share_id, _share_handle->devid);
    *server_id = sdid_parse.server_id;
    return DRV_ERROR_NONE;
}

static drvError_t devmm_share_handle_para_check(uint64_t shareable_handle)
{
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    uint32_t devid = devmm_get_share_devid(shareable_handle);

    (void)halGetHostID(&host_id);
    if ((devid >= SVM_MAX_AGENT_NUM) && (devid != host_id)) {
        DEVMM_DRV_ERR("Invalid shareable_handle. (shareable_handle=%llu; devid=%u)\n", shareable_handle, devid);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t devmm_mem_import_para_check(drv_mem_handle_type handle_type, struct devmm_share_handle *share_handle,
                                              uint32_t devid, drv_mem_handle_t **handle)
{
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    uint64_t shareable_handle;
    drvError_t ret;

    if (devmm_is_split_mode()) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (handle == NULL) {
        DEVMM_DRV_ERR("handle is null.\n");
        devmm_report_null_pointer("handle");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (share_handle == NULL) {
        DEVMM_DRV_ERR("share_handle is null.\n");
        devmm_report_null_pointer("handle");
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)halGetHostID(&host_id);
    if ((devid >= DEVMM_MAX_LOGIC_DEVICE_NUM) && (devid != host_id)) {
        DEVMM_DRV_ERR("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if ((share_handle->handle_type != handle_type) || (handle_type >= MEM_HANDLE_TYPE_MAX)) {
        DEVMM_DRV_ERR("Invalid para. (handle_type=%u; share_handle.handle_type=%u)\n", handle_type,
                      share_handle->handle_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    devmm_share_handle_pack(&shareable_handle, share_handle->share_id, share_handle->devid);

    ret = devmm_share_handle_para_check(shareable_handle);
    if (ret == DRV_ERROR_INVALID_HANDLE) {
        devmm_report_invalid_handle("halMemExportToShareableHandle", shareable_handle);
    }
    return ret;
}

static drvError_t devmm_import_record_create_and_get(uint64_t shareable_handle, uint32_t devid,
                                                     drv_mem_handle_t **handle, enum devmm_record_node_status status)
{
    struct devmm_record_data data = {0};
    drvError_t ret;

    if (status == DEVMM_NODE_INITING) {
        data.key1 = shareable_handle;
        data.key2 = DEVMM_RECORD_KEY_INVALID;
        (void)pthread_mutex_lock(&g_vmm_handle_lock[devid]);
        ret = devmm_record_create_and_get(DEVMM_FEATURE_IMPORT, devid, DEVMM_KEY_TYPE1, DEVMM_NODE_INITING, &data);
        if (ret != DRV_ERROR_TRY_AGAIN) {
            if (ret == DRV_ERROR_NONE) {
                *handle = (drv_mem_handle_t *)(uintptr_t)data.key2;
                ret = devmm_vmm_handle_ref_inc(*handle);
                (void)devmm_record_put(DEVMM_FEATURE_IMPORT, devid, DEVMM_KEY_TYPE1, shareable_handle,
                                       DEVMM_NODE_UNINITED);
                (void)pthread_mutex_unlock(&g_vmm_handle_lock[devid]);
                return ret;
            }
            (void)pthread_mutex_unlock(&g_vmm_handle_lock[devid]);
            DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NONE), "Record get failed. (ret=%d; shareable_handle=%llu; devid=%u)\n",
                             ret, shareable_handle, devid);
            return ret;
        }
        (void)pthread_mutex_unlock(&g_vmm_handle_lock[devid]);
        return ret;
    } else {
        data.key1 = shareable_handle;
        data.key2 = (uint64_t)(uintptr_t)(*handle);
        (void)devmm_record_create_and_get(DEVMM_FEATURE_IMPORT, devid, DEVMM_KEY_TYPE1, DEVMM_NODE_INITED, &data);
        return DRV_ERROR_NONE;
    }
}

static bool devmm_import_is_support_record(drv_mem_handle_type handle_type, struct devmm_share_handle *share_handle)
{
    struct halSDIDParseInfo sdid_parse;
    int64_t value;
    int ret;

    if (handle_type == MEM_HANDLE_TYPE_NONE) {
        return true;
    }

    ret = halParseSDID(share_handle->sdid, &sdid_parse);
    if (ret != DRV_ERROR_NONE) {
        return false;
    }

    ret = halGetDeviceInfo(0, MODULE_TYPE_SYSTEM, INFO_TYPE_SERVER_ID, &value);
    return (ret == DRV_ERROR_NONE) ? (value == sdid_parse.server_id) : false;
}

static drvError_t devmm_import_from_share_handle(drv_mem_handle_type handle_type,
                                                 struct devmm_share_handle *share_handle, uint32_t devid,
                                                 drv_mem_handle_t **handle)
{
    drv_mem_handle_t *tmp_handle = NULL;
    struct devmm_ioctl_arg arg = {0};
    uint64_t shareable_handle;
    struct drv_mem_prop prop = {0};
    drvError_t ret;
    uint32_t host_id = SVM_MAX_AGENT_NUM;

    ret = devmm_mem_import_para_check(handle_type, share_handle, devid, handle);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    tmp_handle = calloc(1, sizeof(drv_mem_handle_t));
    if (tmp_handle == NULL) {
        DEVMM_DRV_ERR("Malloc handle failed.\n");
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    devmm_share_handle_pack(&shareable_handle, share_handle->share_id, share_handle->devid);

    if (devmm_import_is_support_record(handle_type, share_handle)) {
        ret = devmm_import_record_create_and_get(shareable_handle, devid, handle, DEVMM_NODE_INITING);
        if (ret != DRV_ERROR_TRY_AGAIN) {
            free(tmp_handle);
            return ret;
        }
    }

    arg.head.devid = devid;
    arg.data.mem_import_para.share_sdid = share_handle->sdid;
    arg.data.mem_import_para.handle_type = handle_type;
    arg.data.mem_import_para.share_id = share_handle->share_id;
    arg.data.mem_import_para.share_devid = share_handle->devid;
    arg.data.mem_import_para.share_phy_devid = share_handle->phy_devid;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_IMPORT, &arg);
    if (ret != DRV_ERROR_NONE) {
        if (devmm_import_is_support_record(handle_type, share_handle)) {
            (void)devmm_record_put(DEVMM_FEATURE_IMPORT, devid, DEVMM_KEY_TYPE1, shareable_handle, DEVMM_NODE_UNINITED);
        }
        free(tmp_handle);
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Mem import failed. (ret=%d; shareable_handle=%llu; devid=%u)\n", ret, shareable_handle,
                         devid);
        if (ret == DRV_ERROR_INVALID_HANDLE) {
            devmm_report_invalid_handle("halMemExportToShareableHandle", shareable_handle);
        }
        return ret;
    }

    if (devmm_import_is_support_record(handle_type, share_handle)) {
        (void)devmm_import_record_create_and_get(shareable_handle, devid, &tmp_handle, DEVMM_NODE_INITED);
    }

    (void)halGetHostID(&host_id);
    /* share: id side and devid is belong to export dev */
    prop.devid = share_handle->devid;
    prop.module_id = arg.data.mem_import_para.module_id;
    prop.pg_type = arg.data.mem_import_para.pg_type;
    prop.side = (share_handle->devid == host_id) ? MEM_HOST_SIDE : MEM_DEV_SIDE;
    prop.mem_type = arg.data.mem_import_para.mem_type;
    tmp_handle->prop = prop;
    tmp_handle->id = arg.data.mem_import_para.id;
    tmp_handle->side = arg.data.mem_import_para.side;
    tmp_handle->devid = devid;
    tmp_handle->pg_num = arg.data.mem_import_para.pg_num;
    tmp_handle->pa_pg_size = arg.data.mem_import_para.pa_pg_size;
    tmp_handle->pg_type = arg.data.mem_import_para.pg_type;
    tmp_handle->module_id = arg.data.mem_import_para.module_id;
    tmp_handle->support_restore = false;
    tmp_handle->ref = 1;
    tmp_handle->map_ref = 0;
    tmp_handle->is_share = true;

    *handle = tmp_handle;
    DEVMM_DRV_DEBUG_ARG("Mem import. (share_devid=%u; share_id=%d; devid=%u; id=%d)\n",
                        arg.data.mem_import_para.share_devid, arg.data.mem_import_para.share_id, devid, tmp_handle->id);
    return DRV_ERROR_NONE;
}

drvError_t halMemImportFromShareableHandleV2(drv_mem_handle_type handle_type, struct MemShareHandle *share_handle,
                                             uint32_t devid, drv_mem_handle_t **handle)
{
    struct devmm_share_handle *_share_handle = (struct devmm_share_handle *)(void *)share_handle;

    DRV_CURRENT_API_SCOPE();
    return devmm_import_from_share_handle(handle_type, _share_handle, devid, handle);
}

drvError_t halMemImportFromShareableHandle(uint64_t shareable_handle, uint32_t devid, drv_mem_handle_t **handle)
{
    struct devmm_share_handle share_handle;
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    uint32_t tmp_devid = devid;
    int64_t value;
    drvError_t ret;

    DRV_CURRENT_API_SCOPE();

    if (devid >= DEVMM_MAX_LOGIC_DEVICE_NUM) {
        DEVMM_DRV_ERR("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    (void)halGetHostID(&host_id);

    if (devid == host_id) {
        tmp_devid = 0; /* host import use devid 0 to get sdid */
    }

    ret = halGetDeviceInfo(tmp_devid, MODULE_TYPE_SYSTEM, INFO_TYPE_SDID, &value);
    if (ret != DRV_ERROR_NONE) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            DEVMM_DRV_ERR("Get sdid failed. (ret=%d; devid=%u)\n", ret, tmp_devid);
            return ret;
        }
        value = 0;
    }

    share_handle.handle_type = MEM_HANDLE_TYPE_NONE;
    share_handle.sdid = (uint32_t)value;
    share_handle.devid = devmm_get_share_devid(shareable_handle);
    share_handle.phy_devid = 0; /* not care in single app */
    share_handle.share_id = devmm_get_share_id(shareable_handle);

    return devmm_import_from_share_handle(MEM_HANDLE_TYPE_NONE, &share_handle, devid, handle);
}

static drvError_t devmm_set_pid_para_check(uint64_t shareable_handle, int pid[], uint32_t pid_num)
{
    if (pid == NULL) {
        DEVMM_DRV_ERR("Pid is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pid_num == 0) {
        DEVMM_DRV_ERR("Pid num is zero.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pid_num > DEVMM_SHARE_MEM_MAX_PID_CNT) {
        DEVMM_DRV_ERR("Pid num is invalid. (pid_num=%u)\n", pid_num);
        return DRV_ERROR_INVALID_VALUE;
    }
    return devmm_share_handle_para_check(shareable_handle);
}

drvError_t halMemSetPidToShareableHandle(uint64_t shareable_handle, int pid[], uint32_t pid_num)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    if (devmm_is_split_mode()) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_set_pid_para_check(shareable_handle, pid, pid_num);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    arg.head.devid = devmm_get_share_devid(shareable_handle);
    arg.data.mem_set_pid_para.share_id = devmm_get_share_id(shareable_handle);
    arg.data.mem_set_pid_para.pid_num = pid_num;
    arg.data.mem_set_pid_para.pid_list = pid;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_SET_PID, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Set pid failed. (ret=%d; shareable_handle=%llu; pid_num=%u)\n", ret, shareable_handle, pid_num);
    }
    return ret;
}

static drvError_t devmm_mem_set_attr_para_check(uint64_t shareable_handle, enum ShareHandleAttrType type)
{
    if ((u32)type >= SHR_HANDLE_ATTR_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return devmm_share_handle_para_check(shareable_handle);
}

drvError_t halMemShareHandleSetAttribute(uint64_t shareable_handle, enum ShareHandleAttrType type,
                                         struct ShareHandleAttr attr)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    if (devmm_is_split_mode()) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_mem_set_attr_para_check(shareable_handle, type);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    arg.head.devid = devmm_get_share_devid(shareable_handle);
    arg.data.mem_set_attr_para.share_id = devmm_get_share_id(shareable_handle);
    arg.data.mem_set_attr_para.type = type;
    arg.data.mem_set_attr_para.attr = attr;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_SET_ATTR, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Share handle set attr failed. (ret=%d; shareable_handle=%llu; type=%u)\n", ret,
                         shareable_handle, type);
        return ret;
    }

    DEVMM_DRV_DEBUG_ARG("Share handle set attr successfully. (shareable_handle=%llu; type=%u)\n", shareable_handle,
                        type);

    return DRV_ERROR_NONE;
}

static drvError_t devmm_mem_get_attr_para_check(uint64_t shareable_Handle, enum ShareHandleAttrType type,
                                                struct ShareHandleAttr *attr)
{
    if ((u32)type >= SHR_HANDLE_ATTR_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (attr == NULL) {
        DEVMM_DRV_ERR("attr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    return devmm_share_handle_para_check(shareable_Handle);
}

drvError_t halMemShareHandleGetAttribute(uint64_t shareable_handle, enum ShareHandleAttrType type,
                                         struct ShareHandleAttr *attr)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    if (devmm_is_split_mode()) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_mem_get_attr_para_check(shareable_handle, type, attr);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    arg.data.mem_get_attr_para.type = type;
    arg.data.mem_get_attr_para.share_devid = devmm_get_share_devid(shareable_handle);
    arg.data.mem_get_attr_para.share_id = devmm_get_share_id(shareable_handle);
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_GET_ATTR, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Share handle get attr failed. (ret=%d; shareable_handle=%llu; type=%u)\n", ret,
                         shareable_handle, type);
        return ret;
    }

    *attr = arg.data.mem_get_attr_para.attr;
    DEVMM_DRV_DEBUG_ARG("Share handle get attr successfully. (shareable_handle=%llu; type=%u)\n", shareable_handle,
                        type);

    return DRV_ERROR_NONE;
}

drvError_t halMemHandleSetAttribute(drv_mem_handle_t *handle, enum HandleAttrType type, struct HandleAttr attr)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    if (devmm_is_split_mode() || (type >= HANDLE_ATTR_TYPE_MAX)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_mem_handle_check(handle);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    arg.data.mem_handle_set_attr_para.type = type;
    arg.head.devid = handle->devid;
    arg.data.mem_handle_set_attr_para.side = handle->side;
    arg.data.mem_handle_set_attr_para.id = handle->id;
    arg.data.mem_handle_set_attr_para.attr = attr;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_HANDLE_SET_ATTR, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Set attr failed. (ret=%d; devid=%u; id=%d; side=%u; type=%u; map_route=%u)\n", ret,
                         handle->devid, handle->id, handle->side, type, attr.mem_map_route);
        return ret;
    }

    DEVMM_DRV_DEBUG_ARG("Handle set attr successfully. (devid=%u; id=%d; side=%u; type=%u; map_route=%u)\n",
                        handle->devid, handle->id, handle->side, type, attr.mem_map_route);

    return DRV_ERROR_NONE;
}

drvError_t halMemHandleGetAttribute(drv_mem_handle_t *handle, enum HandleAttrType type, struct HandleAttr *attr)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    if (devmm_is_split_mode() || (type >= HANDLE_ATTR_TYPE_MAX)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (attr == NULL) {
        DEVMM_DRV_ERR("Attr is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = devmm_mem_handle_check(handle);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    arg.data.mem_handle_get_attr_para.type = type;
    arg.head.devid = handle->devid;
    arg.data.mem_handle_get_attr_para.side = handle->side;
    arg.data.mem_handle_get_attr_para.id = handle->id;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_HANDLE_GET_ATTR, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Get attr failed. (ret=%d; devid=%u; id=%d; side=%u; type=%u)\n", ret, handle->devid,
                         handle->id, handle->side, type);
        return ret;
    }

    *attr = arg.data.mem_handle_get_attr_para.attr;
    DEVMM_DRV_DEBUG_ARG("Handle get attr successfully. (devid=%u; id=%d; side=%u; type=%u; map_route=%u)\n",
                        handle->devid, handle->id, handle->side, type, attr->mem_map_route);

    return DRV_ERROR_NONE;
}

static drvError_t devmm_mem_info_get_para_check(uint64_t shareable_handle, struct ShareHandleGetInfo *info)
{
    if (info == NULL) {
        DEVMM_DRV_ERR("info is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    return devmm_share_handle_para_check(shareable_handle);
}

drvError_t halMemShareHandleInfoGet(uint64_t shareable_handle, struct ShareHandleGetInfo *info)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    if (devmm_is_split_mode()) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = devmm_mem_info_get_para_check(shareable_handle, info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    arg.data.mem_get_info_para.share_devid = devmm_get_share_devid(shareable_handle);
    arg.data.mem_get_info_para.share_id = devmm_get_share_id(shareable_handle);
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_GET_INFO, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Share handle get info failed. (ret=%d; shareable_handle=%llu)\n", ret, shareable_handle);
        return ret;
    }

    *info = arg.data.mem_get_info_para.info;

    DEVMM_DRV_DEBUG_ARG("Share handle get info successfully. (shareable_handle=%llu; phy_devid=%u)\n", shareable_handle,
                        info->phyDevid);

    return DRV_ERROR_NONE;
}

drvError_t halMemGetAllocationGranularity(const struct drv_mem_prop *prop, drv_mem_granularity_options option,
                                          size_t *granularity)
{
    uint32_t host_id = SVM_MAX_AGENT_NUM;
    drvError_t ret;

    if (option >= MEM_ALLOC_GRANULARITY_INVALID) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (granularity == NULL) {
        DEVMM_DRV_ERR("granularity is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = devmm_mem_prop_check(prop, true);
    if (ret != 0) {
        return ret;
    }
    (void)halGetHostID(&host_id);
    if ((option == MEM_ALLOC_GRANULARITY_MINIMUM) && (prop->pg_type == MEM_NORMAL_PAGE_TYPE) &&
        (prop->devid == host_id) && (prop->module_id == RUNTIME_MODULE_ID)) {
        *granularity = (size_t)getpagesize();
    } else if (prop->pg_type == MEM_GIANT_PAGE_TYPE) {
        *granularity = DEVMM_GIANT_PAGE_SIZE;
    } else {
        *granularity = DEVMM_MEM_ALLOC_RECOMMENDED_GRANULARITY;
    }
    return DRV_ERROR_NONE;
}

static drvError_t devmm_host_register_capability(uint32_t devid, uint32_t acc_module_type, uint32_t *mem_map_cap)
{
    struct devmm_ioctl_arg arg = {0};
    DVresult ret;

    arg.head.devid = devid;
    arg.data.mem_map_cap_para.acc_module_type = acc_module_type;

    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_MEM_MAP_CAP, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR_IF((ret != DRV_ERROR_NOT_SUPPORT),
                         "Host_register_cap ioctl error. (devid=%u; acc_module_type=%u; ret=%d)\n", devid,
                         acc_module_type, ret);
        return ret;
    }

    *mem_map_cap = arg.data.mem_map_cap_para.mem_map_capability;
    DEVMM_DRV_DEBUG_ARG("Host_register_cap. (devid=%u; acc_module_type=%u; flag=%u)\n", devid, acc_module_type,
                        mem_map_cap);
    return DRV_ERROR_NONE;
}

drvError_t halHostRegisterCapabilities(uint32_t devid, uint32_t acc_module_type, uint32_t *mem_map_cap)
{
    if (devid >= DEVMM_MAX_LOGIC_DEVICE_NUM || devid == SVM_HOST_AGENT_ID) {
        DEVMM_DRV_ERR("Devid is invalid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (mem_map_cap == NULL) {
        DEVMM_DRV_ERR("Mem_map_cap is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (acc_module_type >= DRV_ACC_MODULE_TYPE_MAX) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return devmm_host_register_capability(devid, acc_module_type, mem_map_cap);
}

drvError_t halMemGetAddressRange(DVdeviceptr ptr, DVdeviceptr *pbase, size_t *psize)
{
    struct devmm_mem_info mem_info = {
        .start = 0, .end = 0, .module_id = SVM_INVALID_MODULE_ID, .devid = SVM_MAX_AGENT_NUM};

    if ((pbase == NULL) && (psize == NULL)) {
        DEVMM_DRV_ERR("Invalid argument. (pbase=NULL; psize=NULL)\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    devmm_get_svm_va_info(ptr, &mem_info);
    if (mem_info.start == 0) {
        DEVMM_DRV_ERR("ptr is not allocated. (ptr=0x%llx)\n", ptr);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pbase != NULL) {
        *pbase = mem_info.start;
    }

    if (psize != NULL) {
        *psize = mem_info.end - mem_info.start + 1;
    }

    return DRV_ERROR_NONE;
}

drvError_t halMemGetAllocationPropertiesFromHandle(struct drv_mem_prop *prop, drv_mem_handle_t *handle)
{
    uint64_t ref;

    if ((handle == NULL) || (prop == NULL)) {
        DEVMM_DRV_ERR("Input invalid. (handle=%u; prop=%u)\n", (handle == NULL), (prop == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (handle->devid >= DEVMM_MAX_LOGIC_DEVICE_NUM) {
        DEVMM_DRV_ERR("Handle is invalid. (devid=%u)\n", handle->devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_mutex_lock(&g_vmm_handle_lock[handle->devid]);
    ref = devmm_vmm_handle_ref_read(handle);
    if ((ref == 0) || (ref > UINT32_MAX)) {
        DEVMM_DRV_ERR("Handle is invalid. (ref=%llu)\n", ref);
        (void)pthread_mutex_unlock(&g_vmm_handle_lock[handle->devid]);
        return DRV_ERROR_INVALID_VALUE;
    }

    *prop = handle->prop;
    (void)pthread_mutex_unlock(&g_vmm_handle_lock[handle->devid]);
    return DRV_ERROR_NONE;
}

static void devmm_restore_func_register(void)
{
    devmm_record_restore_func_register(DEVMM_FEATURE_VMM_CREATE, devmm_vmm_create_record_restore);
    devmm_record_restore_func_register(DEVMM_FEATURE_VMM_MAP, devmm_vmm_map_record_restore);
#ifndef EMU_ST
    devmm_record_restore_func_register(DEVMM_FEATURE_HOST_REGISTER, devmm_host_register_record_restore);
#endif
}

static void __attribute__((constructor)) devmm_svm_pre_init(void)
{
    devmm_init_mem_advise_record();
}

static drvError_t devmm_soma_pool_create_para_check(soma_mem_pool_t pool, soma_mem_pool_prop prop)
{
    DEVMM_DRV_DEBUG_ARG("Mem pool create.\n");

    if ((prop.handle_type != MEM_HANDLE_TYPE_NONE && prop.handle_type != MEM_HANDLE_TYPE_POSIX) ||
        prop.mem_prop.side != MEM_DEV_SIDE) {
        DEVMM_DRV_ERR("Handle type or side is not supported.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop.mem_prop.module_id != ASCENDCL_MODULE_ID) {
        DEVMM_DRV_ERR("module id is not supported. (module_id=%u)\n", prop.mem_prop.module_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop.mem_prop.pg_type != MEM_HUGE_PAGE_TYPE || prop.mem_prop.mem_type != MEM_HBM_TYPE) {
        DEVMM_DRV_ERR("page type or mem type is not supported. (pg_type=%u; mem_type=%u)\n", prop.mem_prop.pg_type,
                      prop.mem_prop.mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (prop.maxSize == 0) {
        DEVMM_DRV_ERR("Zero va or maxSize.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((IS_ALIGNED(prop.va, DEVMM_SOMA_PAGE_SIZE) == false) ||
        (IS_ALIGNED(prop.maxSize, DEVMM_SOMA_PAGE_SIZE) == false)) {
        DEVMM_DRV_ERR("Soma pool range is not aligned. (devId=%u; poolId=0x%llx; va=0x%llx; size=%llu)\n", pool.devId,
                      pool.poolId, prop.va, prop.maxSize);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t halMemPoolCreate(soma_mem_pool_t pool, soma_mem_pool_prop *prop)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    DEVMM_DRV_DEBUG_ARG("Mem pool create.\n");

    ret = devmm_soma_pool_create_para_check(pool, *prop);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    arg.head.devid = pool.devId;
    arg.data.soma_pool_create_para.pool_id = pool.poolId;
    arg.data.soma_pool_create_para.va = prop->va;
    arg.data.soma_pool_create_para.size = prop->maxSize;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_SOMA_POOL_CREATE, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Mem pool create failed. (ret=%d; devId=%u; poolId=0x%llx; va=0x%llx; size=%llu)\n", ret,
                      pool.devId, pool.poolId, prop->va, prop->maxSize);
        return ret;
    }
    prop->va = arg.data.soma_pool_create_para.va;
    return DRV_ERROR_NONE;
}

drvError_t halMemPoolDestroy(soma_mem_pool_t pool)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    DEVMM_DRV_DEBUG_ARG("Mem pool destroy.\n");

    if ((pool.devId >= DEVMM_MAX_DEVICE_NUM) || (devmm_dev_is_inited(pool.devId) == false)) {
        DEVMM_DRV_ERR("Invalid or uninitialized device. (devId=%u; poolId=0x%llx)\n", pool.devId, pool.poolId);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.head.devid = pool.devId;
    arg.data.soma_pool_destroy_para.pool_id = pool.poolId;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_SOMA_POOL_DESTROY, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Mem pool destroy failed. (ret=%d; devId=%u; poolId=0x%llx)\n", ret, pool.devId, pool.poolId);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t halMemPoolTrim(soma_mem_pool_t pool, uint64_t *reservedSize, uint64_t usedSize, uint64_t freeSize)
{
    struct devmm_ioctl_arg arg = {0};
    drvError_t ret;

    if (reservedSize == NULL) {
        DEVMM_DRV_ERR("NULL pointer detected. (devId=%u; poolId=0x%llx)\n", pool.devId, pool.poolId);
        return DRV_ERROR_INVALID_VALUE;
    }

    DEVMM_DRV_DEBUG_ARG("Mem pool trim.\n");

    arg.head.devid = pool.devId;
    arg.data.soma_pool_trim_para.pool_id = pool.poolId;
    arg.data.soma_pool_trim_para.reserved_size = *reservedSize;
    arg.data.soma_pool_trim_para.used_size = usedSize;
    arg.data.soma_pool_trim_para.free_size = freeSize;
    ret = devmm_svm_ioctl(g_devmm_mem_dev, DEVMM_SVM_SOMA_POOL_TRIM, &arg);
    if (ret != DRV_ERROR_NONE) {
        DEVMM_DRV_ERR("Mem pool trim failed. (ret=%d; devId=%u; poolId=0x%llx)\n", ret, pool.devId, pool.poolId);
        return ret;
    }

    *reservedSize = arg.data.soma_pool_trim_para.reserved_size;

    return DRV_ERROR_NONE;
}