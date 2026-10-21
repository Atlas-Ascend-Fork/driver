/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SVM_FLAG_H
#define SVM_FLAG_H

#include <stdint.h>
#include <stdbool.h>

#include "svm_pub.h"

/* addr capability */
#define SVM_FLAG_CAP_NORMAL_FREE (1ULL << 0ULL)
#define SVM_FLAG_CAP_VMM_PA_FREE (1ULL << 1ULL)
#define SVM_FLAG_CAP_VMM_VA_FREE (1ULL << 2ULL)
#define SVM_FLAG_CAP_IPC_CREATE (1ULL << 3ULL)
#define SVM_FLAG_CAP_IPC_DESTROY (1ULL << 4ULL)
#define SVM_FLAG_CAP_IPC_CLOSE (1ULL << 5ULL)
#define SVM_FLAG_CAP_REGISTER (1ULL << 6ULL)
#define SVM_FLAG_CAP_UNREGISTER (1ULL << 7ULL)
#define SVM_FLAG_CAP_PREFETCH (1ULL << 8ULL)
#define SVM_FLAG_CAP_SYNC_COPY (1ULL << 9ULL)
#define SVM_FLAG_CAP_ASYNC_COPY_SUBMIT (1ULL << 10ULL)
#define SVM_FLAG_CAP_ASYNC_COPY_WAIT (1ULL << 11ULL)
#define SVM_FLAG_CAP_DMA_DESC_CONVERT (1ULL << 12ULL)
#define SVM_FLAG_CAP_DMA_DESC_SUBMIT (1ULL << 13ULL)
#define SVM_FLAG_CAP_DMA_DESC_WAIT (1ULL << 14ULL)
#define SVM_FLAG_CAP_DMA_DESC_DESTROY (1ULL << 15ULL)
#define SVM_FLAG_CAP_GET_ATTR (1ULL << 16ULL)
#define SVM_FLAG_CAP_SYNC_COPY_EX (1ULL << 17ULL)
#define SVM_FLAG_CAP_SYNC_COPY_2D (1ULL << 18ULL)
#define SVM_FLAG_CAP_SYNC_COPY_BATCH (1ULL << 19ULL)
#define SVM_FLAG_CAP_DMA_DESC_CONVERT_2D (1ULL << 20ULL)
#define SVM_FLAG_CAP_VMM_MAP (1ULL << 21ULL)
#define SVM_FLAG_CAP_VMM_UNMAP (1ULL << 22ULL)
#define SVM_FLAG_CAP_VMM_IPC_UNMAP (1ULL << 23ULL)
#define SVM_FLAG_CAP_VMM_EXPORT (1ULL << 24ULL)
#define SVM_FLAG_CAP_MEMSET (1ULL << 25ULL)
#define SVM_FLAG_CAP_LDST (1ULL << 26ULL)
#define SVM_FLAG_CAP_GET_ADDR_CHECK_INFO (1ULL << 27ULL)
#define SVM_FLAG_CAP_GET_MEM_TOKEN_INFO (1ULL << 28ULL)
#define SVM_FLAG_CAP_GET_D2D_TRANS_WAY (1ULL << 29ULL)
#define SVM_FLAG_CAP_MADVISE (1ULL << 30ULL)
#define SVM_FLAG_CAP_REGISTER_ACCESS (1ULL << 31ULL)

/* va attr */
#define SVM_FLAG_ATTR_VA_ONLY (1ULL << 32ULL)
#define SVM_FLAG_ATTR_SPACIFIED_VA (1ULL << 33ULL)
#define SVM_FLAG_ATTR_VA_WITHOUT_MASTER (1ULL << 34ULL)
#define SVM_FLAG_ATTR_MASTER_UVA (1ULL << 35ULL)
#define SVM_FLAG_ATTR_VA_ONLY_WITHOUT_POPULATE (1ULL << 36ULL)
#define SVM_FLAG_SNAPSHOT_OPTIONAL (1ULL << 37ULL)
#define SVM_FLAG_SNAPSHOT_REQUIRED (1ULL << 38ULL)

/* pa attr */
#define SVM_FLAG_ATTR_PA_HPAGE (1ULL << 40ULL)
#define SVM_FLAG_ATTR_PA_GPAGE (1ULL << 41ULL)
#define SVM_FLAG_ATTR_PA_CONTIGUOUS (1ULL << 42ULL)
#define SVM_FLAG_ATTR_PA_P2P (1ULL << 43ULL)

/* pgtable attr */
#define SVM_FLAG_ATTR_PG_NC (1ULL << 48ULL)
#define SVM_FLAG_ATTR_PG_RDONLY (1ULL << 49ULL)

/* others */
#define SVM_FLAG_VMM_MAP_OWNER_NO_ACCESS (1ULL << 51ULL)
#define SVM_FLAG_MUST_WITH_PRIV (1ULL << 52ULL)
#define SVM_FLAG_BY_PASS_CACHE (1ULL << 53ULL)
#define SVM_FLAG_DEV_CP_ONLY (1ULL << 54ULL) /* not support share and op(host ldst/memcpy/memset) */
#define SVM_FLAG_DEV_PRIVATE (1ULL << 55ULL) /* device-private memory, copy is forbidden */

/* model id: bit56~63 */
#define SVM_FLAG_MODULE_ID_BIT 56ULL
#define SVM_FLAG_MODULE_ID_WIDTH 8ULL
#define SVM_FLAG_MODULE_ID_MASK ((1ULL << SVM_FLAG_MODULE_ID_WIDTH) - 1)
#define SVM_FLAG_INVALID_MODULE_ID 0xFFU

static inline bool svm_flag_bit_is_set(u64 flag, u64 bit_mask)
{
    return ((flag & bit_mask) != 0);
}

static inline bool svm_flag_cap_is_support_normal_free(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_NORMAL_FREE);
}

static inline bool svm_flag_cap_is_support_vmm_pa_free(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_VMM_PA_FREE);
}

static inline bool svm_flag_cap_is_support_vmm_va_free(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_VMM_VA_FREE);
}

static inline bool svm_flag_cap_is_support_ipc_create(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_IPC_CREATE);
}

static inline bool svm_flag_cap_is_support_ipc_destroy(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_IPC_DESTROY);
}

static inline bool svm_flag_cap_is_support_ipc_close(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_IPC_CLOSE);
}

static inline bool svm_flag_cap_is_support_register(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_REGISTER);
}

static inline bool svm_flag_cap_is_support_unregister(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_UNREGISTER);
}

static inline bool svm_flag_cap_is_support_register_access(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_REGISTER_ACCESS);
}

static inline bool svm_flag_cap_is_support_prefetch(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_PREFETCH);
}

static inline bool svm_flag_cap_is_support_sync_copy(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_SYNC_COPY);
}

static inline bool svm_flag_cap_is_support_async_copy_submit(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_ASYNC_COPY_SUBMIT);
}

static inline bool svm_flag_cap_is_support_async_copy_wait(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_ASYNC_COPY_WAIT);
}

static inline bool svm_flag_cap_is_support_dma_desc_convert(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_DMA_DESC_CONVERT);
}

static inline bool svm_flag_cap_is_support_dma_desc_submit(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_DMA_DESC_SUBMIT);
}

static inline bool svm_flag_cap_is_support_dma_desc_wait(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_DMA_DESC_WAIT);
}

static inline bool svm_flag_cap_is_support_dma_desc_destroy(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_DMA_DESC_DESTROY);
}

static inline bool svm_flag_cap_is_support_get_attr(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_GET_ATTR);
}

static inline bool svm_flag_cap_is_support_get_addr_check_info(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_GET_ADDR_CHECK_INFO);
}

static inline bool svm_flag_cap_is_support_get_mem_token_info(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_GET_MEM_TOKEN_INFO);
}

static inline bool svm_flag_cap_is_support_madvise(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_MADVISE);
}

static inline bool svm_flag_cap_is_support_vmm_map(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_VMM_MAP);
}

static inline bool svm_flag_cap_is_support_vmm_unmap(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_VMM_UNMAP);
}

static inline bool svm_flag_cap_is_support_vmm_ipc_unmap(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_VMM_IPC_UNMAP);
}

static inline bool svm_flag_cap_is_support_vmm_export(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_VMM_EXPORT);
}

static inline bool svm_flag_cap_is_support_sync_copy_ex(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_SYNC_COPY_EX);
}

static inline bool svm_flag_cap_is_support_sync_copy_2d(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_SYNC_COPY_2D);
}

static inline bool svm_flag_cap_is_support_dma_desc_convert_2d(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_DMA_DESC_CONVERT_2D);
}

static inline bool svm_flag_cap_is_support_memset(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_MEMSET);
}

static inline bool svm_flag_cap_is_support_get_d2d_transway(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_GET_D2D_TRANS_WAY);
}

static inline bool svm_flag_cap_is_support_ldst(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_CAP_LDST);
}

static inline bool svm_flag_attr_is_va_only(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_VA_ONLY);
}

static inline bool svm_flag_attr_is_specified_va(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_SPACIFIED_VA);
}

static inline bool svm_flag_attr_is_va_without_master(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_VA_WITHOUT_MASTER);
}

static inline bool svm_flag_attr_is_master_uva(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_MASTER_UVA);
}

static inline bool svm_flag_attr_is_hpage(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_PA_HPAGE);
}

static inline bool svm_flag_attr_is_gpage(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_PA_GPAGE);
}

static inline bool svm_flag_attr_is_npage(u64 flag)
{
    return ((svm_flag_attr_is_hpage(flag) == false) && (svm_flag_attr_is_gpage(flag) == false));
}

static inline bool svm_flag_attr_is_contiguous(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_PA_CONTIGUOUS);
}

static inline bool svm_flag_attr_is_p2p(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_PA_P2P);
}

static inline bool svm_flag_attr_is_pg_nc(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_PG_NC);
}

static inline bool svm_flag_attr_is_pg_rdonly(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_PG_RDONLY);
}

static inline bool svm_flag_is_vmm_map_owner_no_access(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_VMM_MAP_OWNER_NO_ACCESS);
}

static inline bool svm_flag_is_must_with_priv(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_MUST_WITH_PRIV);
}

static inline bool svm_flag_is_by_pass_cache(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_BY_PASS_CACHE);
}

static inline bool svm_flag_is_dev_cp_only(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_DEV_CP_ONLY);
}

static inline bool svm_flag_is_dev_private(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_DEV_PRIVATE);
}

static inline bool svm_flag_is_va_only_without_populate(u64 flag)
{
    return svm_flag_bit_is_set(flag, SVM_FLAG_ATTR_VA_ONLY_WITHOUT_POPULATE);
}

static inline void svm_flag_set_module_id(u64 *flag, u32 module_id)
{
    *flag |= (((u64)module_id & SVM_FLAG_MODULE_ID_MASK) << SVM_FLAG_MODULE_ID_BIT);
}

static inline u32 svm_flag_get_module_id(u64 flag)
{
    return (u32)((flag >> SVM_FLAG_MODULE_ID_BIT) & SVM_FLAG_MODULE_ID_MASK);
}

#endif
