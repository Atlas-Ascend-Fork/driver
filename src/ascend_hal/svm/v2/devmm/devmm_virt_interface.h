/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVM_VIRT_INTERFACE_H
#define SVM_VIRT_INTERFACE_H
#include <sys/types.h>
#include <unistd.h>

#include "devmm_svm.h"
#include "devmm_rbtree.h"
#include "sched.h"

#define DEVMM_ALLOC_PAGESIZE_WATERSHED 2
#define DEVMM_INVALID_TRYAGAIN 0UL
#define DEVMM_INVALID_STOP 1UL
#define DEVMM_OUT_OF_VIRT_MEM 2UL
#define DEVMM_OUT_OF_PHYS_MEM 3UL
#define DEVMM_NO_DEVICE 4UL
#define DEVMM_ERR_PTR 5UL
#define DEVMM_DEV_PROC_EXIT 6UL
#define DEVMM_ADDR_BUSY 7UL

#define DEVMM_POOL_DESTROY_THREADHOLD 1
#define DEVMM_POOL_PINNED_MEM_DESTROY_THREDHOLD 1

/* align */
#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)
#define ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define ALIGN(x, a) ALIGN_MASK(x, (typeof(x))(a) - 1)

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(val, al) ((val) & ~((typeof(val))(al) - 1))
#endif
#ifndef ALIGN_UP
#define ALIGN_UP(val, al) (((val) + ((typeof(val))(al) - 1)) & ~((typeof(val))(al) - 1))
#endif

#define POOL_MGMT_INITED_FLAG 0xFF1234EEUL

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

enum devmm_heap_list_type {
    SVM_LIST,
    HOST_LIST,
    DEVICE_AGENT0_LIST,
    DEVICE_AGENT63_LIST = DEVICE_AGENT0_LIST + DEVMM_MAX_PHY_DEVICE_NUM - 1,
    HOST_AGENT_LIST,
    RESERVE_LIST,
#ifndef UVM_OPEN
    UVM_LIST,
#endif
    SOMA_LIST,
    HEAP_MAX_LIST,
};

enum devmm_memtype {
    DEVMM_MEM_NORMAL = 0,
    DEVMM_MEM_RDONLY,
    DEVMM_MEMTYPE_MAX,
};

#define DEVMM_ARRAY_NUM(array) (sizeof(array) / sizeof(array[0]))

static inline uint32_t devmm_heap_list_type_by_device(uint32_t device)
{
    return (DEVICE_AGENT0_LIST + device);
}

static inline uint32_t devmm_heap_device_by_list_type(uint32_t sub_type)
{
    return (sub_type - DEVICE_AGENT0_LIST);
}

/* For mem stats. */
static inline uint32_t devmm_heap_sub_type_to_mem_val(uint32_t heap_sub_type)
{
    static uint32_t mem_val[SUB_MAX_TYPE] = {
        [SUB_SVM_TYPE] = MEM_SVM_VAL,          [SUB_DEVICE_TYPE] = MEM_DEV_VAL,    [SUB_HOST_TYPE] = MEM_HOST_VAL,
        [SUB_DVPP_TYPE] = MEM_DEV_VAL,         [SUB_READ_ONLY_TYPE] = MEM_DEV_VAL, [SUB_RESERVE_TYPE] = MEM_RESERVE_VAL,
        [SUB_DEV_READ_ONLY_TYPE] = MEM_DEV_VAL};

    return mem_val[heap_sub_type];
}

static inline bool ptr_is_err(DVdeviceptr ptr)
{
    return (ptr < (DVdeviceptr)DEVMM_ERR_PTR) ? true : false;
}

static inline bool ptr_is_valid(DVdeviceptr ptr)
{
    return (ptr_is_err(ptr) == false);
}

static inline uint64_t get_ptr_err(DVdeviceptr ptr)
{
    return ptr;
}

static inline DVresult ptr_to_errcode(DVdeviceptr ptr)
{
    if (ptr == DEVMM_NO_DEVICE) {
        return DRV_ERROR_NO_DEVICE;
    } else if (ptr == DEVMM_DEV_PROC_EXIT) {
        return DRV_ERROR_PROCESS_EXIT;
    } else if (ptr == DEVMM_ADDR_BUSY) {
        return DRV_ERROR_BUSY;
    }
    return DRV_ERROR_OUT_OF_MEMORY;
}

static inline DVdeviceptr errcode_to_ptr(DVresult err, DVresult default_ptr)
{
    if (err == DRV_ERROR_NO_DEVICE) {
        return DEVMM_NO_DEVICE;
    } else if (err == DRV_ERROR_PROCESS_EXIT) {
        return DEVMM_DEV_PROC_EXIT;
    } else if (err == DRV_ERROR_BUSY) {
        return DEVMM_ADDR_BUSY;
    }
    return default_ptr;
}

static inline uint32_t advise_to_mapped_tree_type(DVmem_advise advise)
{
    return ((advise & DV_ADVISE_READONLY) != 0) ? DEVMM_MAPPED_RDONLY_TREE : DEVMM_MAPPED_RW_TREE;
}

static inline uint32_t advise_to_memtype(DVmem_advise advise)
{
    return ((advise & DV_ADVISE_READONLY) != 0) ? DEVMM_MEM_RDONLY : DEVMM_MEM_NORMAL;
}

static inline bool advise_is_nocache(DVmem_advise advise)
{
    return ((advise & DV_ADVISE_NOCACHE) != 0) ? true : false;
}

struct devmm_virt_heap_type {
    uint32_t heap_type;
    uint32_t heap_list_type;
    uint32_t heap_sub_type;
    uint32_t heap_mem_type; /* A heap belongs to only one physical memory type. --devmm_mem_type */
};

struct devmm_virt_heap_para {
    virt_addr_t start;
    uint64_t heap_size;
    uint32_t page_size;
    uint32_t kernel_page_size;
    uint32_t map_size;
    uint32_t need_cache_thres[DEVMM_MEMTYPE_MAX];
    bool is_limited;
    bool is_base_heap;
#ifndef UVM_OPEN
    bool is_uvm_heap;
#endif
    bool is_soma_heap;
    bool is_host_pin; /* 1: heap belongs to host_pin_heaps[] */
};

struct devmm_virt_com_heap {
    uint32_t inited;
    uint32_t heap_type;
    uint32_t heap_sub_type;
    uint32_t heap_list_type;
    uint32_t heap_mem_type;
    uint32_t heap_idx;
    bool is_base_heap;
#ifndef UVM_OPEN
    bool is_uvm_heap;
#endif
    bool is_soma_heap;
    bool is_host_pin;                                 /* true: heap belongs to host_pin_heaps[] */
    uint64_t cur_cache_mem[DEVMM_MEMTYPE_MAX];        /* current cached mem */
    uint64_t cache_mem_thres[DEVMM_MEMTYPE_MAX];      /* cached mem threshold */
    uint64_t cur_alloc_cache_mem[DEVMM_MEMTYPE_MAX];  /* current alloc can cache total mem */
    uint64_t peak_alloc_cache_mem[DEVMM_MEMTYPE_MAX]; /* peak alloc can cache */
    time_t peak_alloc_cache_time[DEVMM_MEMTYPE_MAX];  /* the time peak alloc can cache */
    uint32_t need_cache_thres[DEVMM_MEMTYPE_MAX];     /* alloc size need to cache threshold */
    bool is_limited; /* true: this kind of heap is resource-limited, not allowed to be alloced another new heap.
                        The heap's cache will be shrunken forcibly, when it's not enough for nocache's allocation */
    bool is_cache;   /* true: follow the cache rule, devmm_get_free_threshold_by_type, used by normal heap.
                        false: no cache, free the heap immediately, used by specified va alloc. For example,
                               alloc 2M success, free 2M success, alloc 2G will fail because of cache heap. */
    virt_addr_t start;
    virt_addr_t end;

    uint32_t module_id; /* used for large heap (>=512M) */
    uint32_t devid;     /* used for large heap (>=512M) */
    uint64_t mapped_size;
    uint64_t reserve_size; /* used for snapshot restore of SUB_RESERVE_TYPE primary_heap */
    uint32_t chunk_size;
    uint32_t kernel_page_size; /* get from kernel */
    uint32_t map_size;
    uint64_t heap_size;
    DVmem_advise advise;

    struct devmm_com_heap_ops *ops;
    pthread_mutex_t tree_lock;
    pthread_rwlock_t heap_rw_lock;
    uint64_t sys_mem_alloced;
    uint64_t sys_mem_freed;
    uint64_t sys_mem_alloced_num;
    uint64_t sys_mem_freed_num;

    struct devmm_virt_list_head list; /* associated to base heap's devmm_heap_list */
    struct devmm_heap_rbtree rbtree_queue;
};

#define DEVMM_HEAP_INVALID DEVMM_MAX_HEAP_NUM
#define DEVMM_FREE_HEAP_HEAD 64
#define DEVMM_DVPP_HEAP_NUM 64

struct devmm_heap_queue {
    struct devmm_virt_com_heap base_heap;      /* use for manage 32T heap, heap range 1g */
    struct devmm_virt_com_heap host_base_heap; /* use for manage 3T(5T~8T) heap, host pin memory */
#ifndef UVM_OPEN
    struct devmm_virt_com_heap uvm_heap; /* use for manage UVM 4T heap */
#endif
    struct devmm_virt_com_heap soma_heap; /* use for manage SOMA 3T heap */
    struct devmm_virt_com_heap *heaps[DEVMM_MAX_HEAP_NUM];
    struct devmm_virt_com_heap *host_pin_heaps[DEVMM_MAX_HOST_PIN_HEAP_NUM]; /* reserve uva heap lookup */
};

struct devmm_heap_list {
    int heap_cnt;
    pthread_rwlock_t list_lock;
    struct devmm_virt_list_head heap_list;
    volatile uint64_t version; /* used for checking whether the heap_list has been modified; */
                               /* or some memory space have been return to the heap_list by another thread; */
                               /* so the heap_list needs to be re-scanned. */
};

struct devmm_virt_heap_mgmt {
    uint32_t inited;
    pid_t pid;

    uint64_t max_conti_size; /* eq heap size */

    virt_addr_t start; /* svm page_size aligned */
    virt_addr_t end;   /* svm page_size aligned */

    virt_addr_t host_pin_start; /* Host pin range 5T ~ 8T */
    virt_addr_t host_pin_end;   /* Host pin range 5T ~ 8T */
#ifndef UVM_OPEN
    virt_addr_t uvm_start; /* uvm vaddr start */
    virt_addr_t uvm_end;   /* uvm vaddr end */
#endif
    virt_addr_t soma_start;
    virt_addr_t soma_end;
    virt_addr_t svm_start;
    virt_addr_t svm_end;

    virt_addr_t dvpp_start; /* dvpp vaddr start */
    virt_addr_t dvpp_end;   /* dvpp vaddr end */
    uint64_t dvpp_mem_size[DEVMM_MAX_PHY_DEVICE_NUM];

    virt_addr_t read_only_start; /* read vaddr start */
    virt_addr_t read_only_end;   /* read vaddr end */

    uint32_t svm_page_size;
    uint32_t local_page_size;
    uint32_t huge_page_size;
#ifndef UVM_OPEN
    uint32_t uvm_page_size;
#endif
    bool support_bar_mem[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_dev_read_only[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_dev_mem_map_host[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_bar_huge_mem[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_agent_giant_page[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_shmem_map_exbus[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_remote_mmap[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_mem_host_uva[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_host_giant_page;
    bool host_support_pin_user_pages_interface;
    bool support_host_rw_dev_ro;
    uint64_t double_pgtable_offset[DEVMM_MAX_PHY_DEVICE_NUM];
    bool support_host_pin_pre_register;
    bool support_host_mem_pool;
    bool is_dev_inited[SVM_MAX_AGENT_NUM];
    bool can_init_dev[SVM_MAX_AGENT_NUM];

    struct devmm_heap_queue heap_queue;
    struct devmm_heap_list huge_list[HEAP_MAX_LIST][SUB_MAX_TYPE][DEVMM_MEM_TYPE_MAX];
    struct devmm_heap_list normal_list[HEAP_MAX_LIST][SUB_MAX_TYPE][DEVMM_MEM_TYPE_MAX];
};

DVresult devmm_virt_init_heap_mgmt(void);
void *devmm_virt_init_get_heap_mgmt(void);
void devmm_virt_uninit_heap_mgmt(void);
DVresult devmm_virt_backup_heap_mgmt(void);
DVresult devmm_virt_restore_heap_mgmt(void);
typedef DVresult (*devmm_snapshot_add_fn)(uint64_t va, uint64_t size, bool required, void *priv);
DVresult devmm_snapshot_collect_memory(bool include_optional, devmm_snapshot_add_fn add, void *priv);
DVresult devmm_snapshot_mark_required(uint64_t va, uint64_t size, uint32_t device);

void *devmm_virt_get_heap_mgmt(void);
bool devmm_dev_is_inited(uint32_t devid);
struct devmm_virt_com_heap *devmm_virt_get_heap_mgmt_virt_heap(uint32_t heap_idx);
virt_addr_t devmm_virt_heap_alloc_ops(struct devmm_virt_com_heap *heap, virt_addr_t alloc_ptr, size_t alloc_size,
                                      DVmem_advise advise);
int devmm_virt_heap_free_ops(struct devmm_virt_com_heap *heap, virt_addr_t ptr);
DVresult devmm_free_to_normal_heap(struct devmm_virt_heap_mgmt *p_heap_mgmt, struct devmm_virt_com_heap *heap,
                                   DVdeviceptr p, uint64_t *free_len);

DVresult devmm_virt_destroy_heap(struct devmm_virt_heap_mgmt *mgmt, struct devmm_virt_com_heap *heap,
                                 bool need_mem_stats_dec);
DVresult devmm_virt_set_heap_idle(struct devmm_virt_heap_mgmt *mgmt, struct devmm_virt_com_heap *heap);

uint32_t devmm_virt_status_get_free_pgcnt_buddy(uint32_t heap_idx);
uint32_t devmm_virt_status_get_page_cache_cnt(uint32_t heap_idx);

bool devmm_virt_check_idle_heap(struct devmm_virt_com_heap *heap);
uint32_t devmm_va_to_heap_idx(const struct devmm_virt_heap_mgmt *mgmt, virt_addr_t va, bool is_host_pin);
struct devmm_virt_com_heap *devmm_va_to_heap(virt_addr_t va);
DVresult devmm_get_heap_list_by_type(struct devmm_virt_heap_mgmt *p_heap_mgmt, struct devmm_virt_heap_type *heap_type,
                                     struct devmm_heap_list **heap_list);
uint32_t devmm_virt_get_page_size_by_heap_type(struct devmm_virt_heap_mgmt *mgmt, uint32_t heap_type,
                                               uint32_t heap_sub_type);
uint32_t devmm_virt_get_kernel_page_size_by_heap_type(struct devmm_virt_heap_mgmt *mgmt, uint32_t heap_type,
                                                      uint32_t heap_sub_type);
DVresult devmm_alloc_managed(DVdeviceptr *pp, size_t bytesize, struct devmm_virt_heap_type *heap_type,
                             DVmem_advise advise);
DVresult devmm_free_managed(DVdeviceptr p);
drvError_t devmm_alloc_proc(uint32_t devid, uint32_t sub_mem_type, DVmem_advise advise, size_t size, DVdeviceptr *pp);
void devmm_set_module_id_to_advise(uint32_t model_id, DVmem_advise *advise);
void devmm_virt_status_init(struct devmm_virt_com_heap *heap);
DVresult devmm_ioctl_enable_heap(uint32_t heap_idx, uint32_t heap_type, uint32_t heap_sub_type, uint64_t heap_size,
                                 uint32_t heap_list_type, bool is_host_pin);
int devmm_ioctl_disable_heap(uint32_t heap_idx, uint32_t heap_type, uint32_t heap_sub_type, uint64_t heap_size,
                             bool is_host_pin);
struct devmm_virt_com_heap *devmm_virt_get_heap_from_queue(struct devmm_virt_heap_mgmt *mgmt, uint32_t heap_idx,
                                                           size_t heap_size, bool is_host_pin);
virt_addr_t devmm_alloc_from_normal_heap(struct devmm_virt_heap_mgmt *p_heap_mgmt, size_t bytesize,
                                         struct devmm_virt_heap_type *heap_type, DVmem_advise advise, DVdeviceptr va);
uint32_t devmm_virt_heap_size_to_order(uint64_t step, size_t size);
void devmm_virt_heap_update_info(struct devmm_virt_heap_mgmt *mgmt, struct devmm_virt_com_heap *heap,
                                 struct devmm_virt_heap_type *heap_type, struct devmm_com_heap_ops *ops,
                                 struct devmm_virt_heap_para *heap_info);
void devmm_virt_normal_heap_update_info(struct devmm_virt_heap_mgmt *mgmt, struct devmm_virt_com_heap *heap,
                                        struct devmm_virt_heap_type *heap_type, struct devmm_com_heap_ops *ops,
                                        uint64_t alloc_size);
int devmm_ioctl_alloc_dev(DVdeviceptr p, size_t size, uint32_t devid, DVmem_advise advise);
DVresult devmm_ioctl_free_pages(virt_addr_t ptr);
bool devmm_virt_heap_is_primary(struct devmm_virt_com_heap *heap);
DVresult devmm_virt_init_heap_customize(struct devmm_virt_heap_mgmt *mgmt, struct devmm_virt_heap_type *heap_type,
                                        struct devmm_virt_heap_para *heap_info, struct devmm_com_heap_ops *ops);
virt_addr_t devmm_virt_heap_alloc_device(struct devmm_virt_com_heap *heap, virt_addr_t ret_val, size_t alloc_size,
                                         DVmem_advise advise);
DVresult devmm_virt_heap_free_pages(struct devmm_virt_com_heap *heap, virt_addr_t ptr);
DVresult devmm_soma_heap_free_pages(virt_addr_t ptr);
void devmm_print_svm_va_info(uint64_t va, DVresult ret);
void devmm_host_pin_pre_register_release(virt_addr_t ptr);
void devmm_host_pin_dma_register_prepare(struct devmm_virt_heap_mgmt *mgmt, uint32_t devid);

static inline bool devmm_is_specified_va_alloc(uint64_t va)
{
    return (va != 0);
}
#endif
