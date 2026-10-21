/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DEVMM_SVM_H
#define DEVMM_SVM_H

#include "errno.h"
#include "securec.h"

#include "ascend_inpackage_hal.h"
#include "devmm_virt_comm.h"
#include "drv_current.h"
#include "svm_ioctl.h"

#ifndef EMU_ST
#include "dmc_user_interface.h"
#else
#include "ut_log.h"
#endif

#ifdef EMU_ST
#define THREAD __thread
#else
#define THREAD
#endif

#ifdef EMU_ST
#define STATIC
#define INLINE
int32_t errno_to_user_errno(int32_t kern_err_no);
#else
#define STATIC static
#define INLINE inline
#endif

#define DEVMM_THREAD_STACK_SIZE 0x20000
#define DEVMM_DEV_PAGE_SIZE 4096
#define DEVMM_HPAGE_SIZE (1 << 21)
#define BYTES_IN_EACH_KB 1024
#define DEVMM_LARGE_MEM_THRESHOLD_SIZE (DEVMM_HEAP_SIZE / 2) /* 512M */

extern void *g_devmm_mem_mapped_addr;
extern THREAD int g_devmm_mem_dev;

extern pthread_mutex_t g_devmm_mutex;

#define DEVMM_IS_SVM_ADDR(va) devmm_va_is_svm(va)

#ifndef UVM_OPEN
#define DEVMM_IS_UVM_ADDR(va) devmm_va_is_uvm(va)
#endif

#define DEVMM_SVM_MODULE 0
#define DEVMM_DAVINCI_MODULE 1

#define DEVMM_KERNEL_ERR(err)                         \
    ((DVresult)(((err) == 0) ? DRV_ERROR_IOCRL_FAIL : \
                               (((err) != ESRCH) ? errno_to_user_errno(err) : DRV_ERROR_PROCESS_EXIT)))

#ifndef DEVMM_UT
#define DEVMM_DRV_ERR(fmt, ...) \
    DRV_ERR(HAL_MODULE_TYPE_DEVMM, "<errno:%d, %d> " fmt, errno, errno_to_user_errno(errno), ##__VA_ARGS__)
#else
#define DEVMM_DRV_ERR(fmt, ...) \
    DRV_ERR(HAL_MODULE_TYPE_DEVMM, "<errno:%d, %d> " fmt, errno, errno_to_user_errno(errno), ##__VA_ARGS__)
#endif

#define DEVMM_DRV_ERR_IF(cond, fmt, ...) \
    if (cond)                            \
    DEVMM_DRV_ERR(fmt, ##__VA_ARGS__)

#define DEVMM_DRV_WARN(fmt, ...) DRV_WARN(HAL_MODULE_TYPE_DEVMM, fmt, ##__VA_ARGS__)
#define DEVMM_DRV_INFO(fmt, ...) DRV_INFO(HAL_MODULE_TYPE_DEVMM, fmt, ##__VA_ARGS__)
#define DEVMM_DRV_DEBUG_ARG(fmt, ...) DRV_DEBUG(HAL_MODULE_TYPE_DEVMM, fmt, ##__VA_ARGS__)
/* alarm event log, non-alarm events use debug or run log */
#define DEVMM_DRV_EVENT(fmt, ...) DRV_EVENT(HAL_MODULE_TYPE_DEVMM, fmt, ##__VA_ARGS__)
/* infrequent log level */
#define DEVMM_DRV_NOTICE(fmt, ...) DRV_NOTICE(HAL_MODULE_TYPE_DEVMM, fmt, ##__VA_ARGS__)
#define DEVMM_RUN_INFO(fmt, ...) DRV_RUN_INFO(HAL_MODULE_TYPE_DEVMM, fmt, ##__VA_ARGS__)

#define DEVMM_RUN_INFO_IF(cond, fmt, ...) \
    if (cond)                             \
    DEVMM_RUN_INFO(fmt, ##__VA_ARGS__)

#define DEVMM_DRV_SWITCH(fmt, ...) (void)0

#define SVM_HOST_MEM_ALLOCED_BY_MMAP 0
#define SVM_HOST_MEM_ALLOCED_BY_MALLOC 1

#ifndef UVM_OPEN
#define UVM_HOST_MEM_ALLOCED_BY_MMAP 0
#define UVM_HOST_MEM_ALLOCED_BY_MALLOC 1
#endif

#define SVM_MASTER_SIDE 0
#define SVM_AGENT_SIDE 1

#define SVM_ADDR_OFFSET_BIT 32
#define SVM_ADDR_OFFSET_MASK ((1ul << SVM_ADDR_OFFSET_BIT) - 1)
#define ADDR_TO_OFFSET(addr) ((addr) & SVM_ADDR_OFFSET_MASK)

void devmm_set_host_mem_alloc_mode(int mode);
int devmm_get_host_mem_alloc_mode(void);

#ifndef UVM_OPEN
void devmm_set_uvm_host_mem_alloc_mode(int mode);
int devmm_get_uvm_host_mem_alloc_mode(void);
#endif

bool devmm_va_is_svm(virt_addr_t va);
#ifndef UVM_OPEN
bool devmm_va_is_uvm(virt_addr_t va);
#endif
bool devmm_va_is_soma(virt_addr_t va);

uint32_t devmm_get_cached_mem_type(DVdeviceptr p);

bool devmm_is_host_pin_memory_map_failed(void);

static inline bool svm_is_dcache_addr(unsigned long long va, unsigned long long size)
{
    return ((va >= DEVMM_DCACHE_ADDR_START) && (size <= DEVMM_DCACHE_OFFSET) &&
            (va < (DEVMM_DCACHE_ADDR_START + DEVMM_DCACHE_OFFSET)) &&
            ((va + size) <= (DEVMM_DCACHE_ADDR_START + DEVMM_DCACHE_OFFSET)));
}

static inline bool devmm_is_host_agent(uint32_t device)
{
    if (device == SVM_HOST_AGENT_ID) {
        return true;
    }
    return false;
}

static inline uint32_t devmm_memtype_to_heap_subtype(uint32_t memtype)
{
    uint32_t memtype_to_heap_subtype[MEM_MAX_VAL] = {
        [MEM_SVM_VAL] = SUB_SVM_TYPE,   [MEM_DEV_VAL] = SUB_DEVICE_TYPE,        [MEM_HOST_VAL] = SUB_HOST_TYPE,
        [MEM_DVPP_VAL] = SUB_DVPP_TYPE, [MEM_HOST_AGENT_VAL] = SUB_DEVICE_TYPE, [MEM_RESERVE_VAL] = SUB_RESERVE_TYPE,
    };

    return memtype_to_heap_subtype[memtype];
}

static inline uint32_t devmm_memtype_mask_to_heap_subtype_mask(uint32_t memtype_mask)
{
    uint32_t memtype, heap_subtype_mask = 0;

    for (memtype = MEM_SVM_VAL; memtype <= MEM_RESERVE_VAL; memtype++) {
        if ((memtype_mask & (1u << memtype)) != 0) {
            heap_subtype_mask |= (1u << devmm_memtype_to_heap_subtype(memtype));
        }
    }
    return heap_subtype_mask;
}

/* for ut */
void *devmm_svm_map_by_size(void *mem_map_addr, uint64_t size);
void *devmm_svm_map_with_flag(void *mem_map_addr, uint64_t size, uint64_t flags, int fixed_va_flag);
void devmm_svm_munmap(void *mem_unmap_addr, uint64_t size);

/* These func are used for stub by tool, we declare here to resolve the compilation alarm */
DVresult drvMemcpyInner(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count);
DVresult halMemCpyAsyncWaitFinishInner(uint64_t copy_fd);
DVresult drvMemsetD8Inner(DVdeviceptr dst, size_t dest_max, UINT8 value, size_t num);
DVresult halSdmaCopyInner(DVdeviceptr dst, size_t dst_size, DVdeviceptr src, size_t len);
drvError_t halMemAllocInner(void **pp, unsigned long long size, unsigned long long flag);
drvError_t halMemFreeInner(void *pp);
DVresult halMemcpy3D(void *pCopy);
drvError_t halMemcpy2DInner(struct MEMCPY2D *p_copy);
DVresult halMemcpySumbitInner(struct DMA_ADDR *dma_addr, int flag);
DVresult halMemCpyAsyncInner(DVdeviceptr dst, size_t dest_max, DVdeviceptr src, size_t byte_count, uint64_t *copy_fd);
DVresult halMemcpyWaitInner(struct DMA_ADDR *dma_addr);

drvError_t devmm_register_mem_to_dma(void *src_va, uint64_t size, uint32_t flag, uint32_t devid);
drvError_t devmm_pre_register_mem_to_dma(void *src_va, uint64_t size, uint32_t flag, uint32_t devid);
drvError_t devmm_unregister_mem_to_dma(void *src_va, uint32_t devid);

int drvGetProcStatus(void);

bool devmm_is_split_mode(void);
bool devmm_is_snapshot_state(void);

/* errmsg */
#define devmm_report_err(error_code, key, value, arg_num) REPORT_PREDEFINED_ERR_MSG(error_code, key, value, arg_num)
#define DEVMM_REPORT_ERR_MSG_STR_LEN 32
#define DEVMM_REPORT_REASON_STR_LEN 256
#define DEVMM_INVALID_ARG_ERR_MSG_ARG_NUM 4
#define DEVMM_NOT_SUPPORT_ERR_MSG_ARG_NUM 2
#define DEVMM_OUT_OF_MEM_ERR_MSG_ARG_NUM 3
#define DEVMM_MEM_IN_USE_ERR_MSG_ARG_NUM 2
#define DEVMM_INVALID_HANDLE_REASON                          \
    "1. The shared handle must be generated by the %s API. " \
    "2. The handle content has been modified or the handle has been released"
#define DEVMM_NULL_POINTER_ERR_MSG_ARG_NUM 2
#define DEVMM_INVALID_ADDR_REASON "The address is not correctly allocated or has been released"
#define DEVMM_PROCESS_NOT_IN_TRUSTLIST_REASON                                    \
    "The process where the share memory resides is not added to the trustlist. " \
    "Add the process to the trustist or disable trustist verification"

static inline void devmm_report_out_of_range(const char *para_name, unsigned long long para_value,
                                             unsigned int min_value, unsigned int max_value)
{
    const char *func_name = drv_current->hal_api_name;
    const char *key[] = {"func_name", "para_value", "para_name", "reason"};
    char value[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    char reason[DEVMM_REPORT_REASON_STR_LEN] = {0};
    const char *report_value[] = {func_name, value, para_name, reason};
    int ret;

    ret = snprintf_s(value, sizeof(value), sizeof(value) - 1, "%llu", para_value);
    if (ret < 0) {
        return;
    }
    ret = snprintf_s(reason, sizeof(reason), sizeof(reason) - 1,
                     "The parameter value is out of range. The valid range is [%u,%u]", min_value, max_value);
    if (ret < 0) {
        return;
    }
    devmm_report_err("EL0016", key, report_value, DEVMM_INVALID_ARG_ERR_MSG_ARG_NUM);
}

static inline void devmm_report_para_zero(const char *para_name, unsigned long long para_value)
{
    const char *func_name = drv_current->hal_api_name;
    const char *key[] = {"func_name", "para_value", "para_name", "reason"};
    const char *reason = "The input parameter must be greater than 0";
    char value[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    const char *report_value[] = {func_name, value, para_name, reason};
    int ret;

    ret = snprintf_s(value, sizeof(value), sizeof(value) - 1, "%llu", para_value);
    if (ret < 0) {
        return;
    }
    devmm_report_err("EL0016", key, report_value, DEVMM_INVALID_ARG_ERR_MSG_ARG_NUM);
}

static inline void devmm_report_para_only_zero(const char *para_name, unsigned long long para_value)
{
    const char *func_name = drv_current->hal_api_name;
    const char *key[] = {"func_name", "para_value", "para_name", "reason"};
    const char *reason = "The input parameter must be 0";
    char value[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    const char *report_value[] = {func_name, value, para_name, reason};
    int ret;

    ret = snprintf_s(value, sizeof(value), sizeof(value) - 1, "%llu", para_value);
    if (ret < 0) {
        return;
    }
    devmm_report_err("EL0016", key, report_value, DEVMM_INVALID_ARG_ERR_MSG_ARG_NUM);
}

static inline void devmm_report_no_permission(void)
{
    const char *func_name = drv_current->hal_api_name;
    const char *key[] = {"func_name", "reason"};
    const char *value[] = {func_name, DEVMM_PROCESS_NOT_IN_TRUSTLIST_REASON};

    devmm_report_err("EL0021", key, value, DEVMM_NOT_SUPPORT_ERR_MSG_ARG_NUM);
}

static inline void devmm_report_invalid_handle(const char *generate_func, uint64_t share_handle)
{
    const char *func_name = drv_current->hal_api_name;
    const char *key[] = {"func_name", "para_value", "para_name", "reason"};
    char handle_value[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    char reason[DEVMM_REPORT_REASON_STR_LEN] = {0};
    const char *value[] = {func_name, handle_value, "share_handle", reason};
    int ret;

    ret = snprintf_s(handle_value, sizeof(handle_value), sizeof(handle_value) - 1, "0x%llx", share_handle);
    if (ret < 0) {
        return;
    }

    ret = snprintf_s(reason, sizeof(reason), sizeof(reason) - 1, DEVMM_INVALID_HANDLE_REASON, generate_func);
    if (ret < 0) {
        return;
    }
    devmm_report_err("EL0016", key, value, DEVMM_INVALID_ARG_ERR_MSG_ARG_NUM);
}

static inline void devmm_report_null_pointer(const char *para_name)
{
    const char *func_name = drv_current->hal_api_name;
    const char *key[] = {"func_name", "para_name"};
    const char *value[] = {func_name, para_name};

    devmm_report_err("EL0017", key, value, DEVMM_NULL_POINTER_ERR_MSG_ARG_NUM);
}

static inline void devmm_report_addr_not_allocated(const char *para_name, unsigned long long para_value)
{
    const char *func_name = drv_current->hal_api_name;
    const char *key[] = {"func_name", "para_value", "para_name", "reason"};
    char value[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    const char *report_value[] = {func_name, value, para_name, DEVMM_INVALID_ADDR_REASON};
    int ret;

    ret = snprintf_s(value, sizeof(value), sizeof(value) - 1, "0x%llx", para_value);
    if (ret < 0) {
        return;
    }
    devmm_report_err("EL0016", key, report_value, DEVMM_INVALID_ARG_ERR_MSG_ARG_NUM);
}

static inline const char *devmm_format_align_size(size_t align_size, char *buf, size_t buf_len)
{
    const size_t bytes_per_mb = BYTES_IN_EACH_KB * BYTES_IN_EACH_KB;
    const size_t bytes_per_gb = bytes_per_mb * BYTES_IN_EACH_KB;
    size_t unit_size = 1;
    const char *unit = "B";
    int ret;

    if (align_size == 0) {
        ret = snprintf_s(buf, buf_len, buf_len - 1, "NA");
    } else {
        if ((align_size % bytes_per_gb) == 0) {
            unit_size = bytes_per_gb;
            unit = " GB";
        } else if ((align_size % bytes_per_mb) == 0) {
            unit_size = bytes_per_mb;
            unit = " MB";
        } else if ((align_size % BYTES_IN_EACH_KB) == 0) {
            unit_size = BYTES_IN_EACH_KB;
            unit = " KB";
        }
        ret = snprintf_s(buf, buf_len, buf_len - 1, "%llu%s", (unsigned long long)(align_size / unit_size), unit);
    }
    return (ret < 0) ? "" : buf;
}

static inline void devmm_report_addr_not_aligned(const char *para_name, unsigned long long para_value,
                                                 size_t align_size)
{
    const char *func_name = drv_current->hal_api_name;
    const char *key[] = {"func_name", "para_value", "para_name", "reason"};
    char value[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    char reason[DEVMM_REPORT_REASON_STR_LEN] = {0};
    char align_size_str[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    const char *report_value[] = {func_name, value, para_name, reason};
    int ret;

    (void)devmm_format_align_size(align_size, align_size_str, sizeof(align_size_str));
    ret = snprintf_s(reason, sizeof(reason), sizeof(reason) - 1,
                     "The input address does not meet the %s alignment requirement", align_size_str);
    if (ret < 0) {
        return;
    }
    ret = snprintf_s(value, sizeof(value), sizeof(value) - 1, "0x%llx", para_value);
    if (ret < 0) {
        return;
    }

    devmm_report_err("EL0016", key, report_value, DEVMM_INVALID_ARG_ERR_MSG_ARG_NUM);
}

static inline void devmm_report_size_not_aligned(const char *para_name, unsigned long long para_value,
                                                 size_t align_size)
{
    const char *func_name = drv_current->hal_api_name;
    const char *key[] = {"func_name", "para_value", "para_name", "reason"};
    char value[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    char align_size_str[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    char reason[DEVMM_REPORT_REASON_STR_LEN] = {0};
    const char *report_value[] = {func_name, value, para_name, reason};
    int ret;

    (void)devmm_format_align_size(align_size, align_size_str, sizeof(align_size_str));
    ret = snprintf_s(
        reason, sizeof(reason), sizeof(reason) - 1,
        "The input size does not meet the memory allocation granularity requirement. It "
        "must be a multiple of %s. For details about the memory allocation granularity of a specified memory "
        "attribute, see the official document",
        align_size_str);
    if (ret < 0) {
        return;
    }
    ret = snprintf_s(value, sizeof(value), sizeof(value) - 1, "0x%llx", para_value);
    if (ret < 0) {
        return;
    }
    devmm_report_err("EL0016", key, report_value, DEVMM_INVALID_ARG_ERR_MSG_ARG_NUM);
}

static inline void devmm_report_oom(size_t size, const char *module_name, uint32_t devid)
{
    const char *func_name = drv_current->hal_api_name;
    char size_str[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    const char *error_code = devmm_is_host_agent(devid) ? "EL0018" : "EL0019";
    const char *key[] = {"func_name", "size", "moduleName"};
    const char *value[] = {func_name, size_str, (module_name == NULL) ? "UNKNOWN" : module_name};
    int ret;

    ret = snprintf_s(size_str, sizeof(size_str), sizeof(size_str) - 1, "%zu bytes", size);
    if (ret < 0) {
        return;
    }
    devmm_report_err(error_code, key, value, DEVMM_OUT_OF_MEM_ERR_MSG_ARG_NUM);
}

static inline void devmm_report_mem_in_use(unsigned long long address)
{
    const char *func_name = drv_current->hal_api_name;
    const char *key[] = {"func_name", "address"};
    char address_str[DEVMM_REPORT_ERR_MSG_STR_LEN] = {0};
    const char *value[] = {func_name, address_str};
    int ret;

    ret = snprintf_s(address_str, sizeof(address_str), sizeof(address_str) - 1, "0x%llx", address);
    if (ret < 0) {
        return;
    }
    devmm_report_err("EL0024", key, value, DEVMM_MEM_IN_USE_ERR_MSG_ARG_NUM);
}

#endif /* __DEVMM_SVM_H__ */
