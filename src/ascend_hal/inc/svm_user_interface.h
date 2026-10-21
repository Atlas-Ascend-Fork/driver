/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVM_USER_INTERFACE_H
#define SVM_USER_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>
#include <drv_type.h>

#include "ascend_hal.h"

#define REMOTE_REQUEST 0x5a
#define LOCAL_REQUEST 0xa5

#define SVM_MEM_ACCESS_READ (0x1U << 0)
#define SVM_MEM_ACCESS_WRITE (0x1U << 1)
#define SVM_REGISTER_FLAG_ACCESS_BY_DMA (1ULL << 0ULL)

bool svm_support_get_user_malloc_attr(uint32_t dev_id);
bool svm_support_vmm_normal_granularity(uint32_t dev_id);
bool svm_support_mem_host_uva(uint32_t dev_id);
bool svm_support_mem_register_query_and_get_attr(uint32_t dev_id);
#ifdef CFB_BUILD_DEBUG
DLLEXPORT bool svm_feature_is_support(uint32_t devid, drvFeature_t type);
#else
bool svm_feature_is_support(uint32_t devid, drvFeature_t type);
#endif
bool svm_support_mem_register_host_pinned(uint32_t dev_id);

drvError_t drvMemDeviceOpenInner(uint32_t devid, halDevOpenIn *in, halDevOpenOut *out);
drvError_t drvMemDeviceCloseInner(uint32_t devid, halDevCloseIn *in);
drvError_t drvMemMmapInner(uint32_t device, void **pp, size_t size, int side);
drvError_t drvMemUnmapInner(void *pp, int side, size_t *size);

drvError_t drvMemDeviceCloseUserRes(uint32_t devid, halDevCloseIn *in);
drvError_t drvMemProcResBackup(halProcResBackupInfo *info);
drvError_t drvMemProcResRestore(halProcResRestoreInfo *info);

bool svm_va_is_in_svm_range(uint64_t va);

int halMemGet(unsigned long long addr, unsigned long long size);
int halMemPut(unsigned long long addr, unsigned long long size);

int halMemAllocVaWithNoPopulate(u32 devid, u64 size, u64 *opened_va);
int halMemFreeVaWithNoPopulate(u64 opened_va);

#define SVM_ADDR_WHITELIST_ADD 0ULL
#define SVM_ADDR_WHITELIST_DEL 1ULL

/**
 * @ingroup driver
 * @brief Configure the address whitelist.
 * @attention
 * 1. User need to ensure that the va and size are valid.
 * 2. The va must belong to the aicpu schedule process.
 * @param [in] devid: logical device id.
 * @param [in] va: virtual address.
 * @param [in] size: address range size.
 * @param [in] flag: whitelist operation flag, SVM_ADDR_WHITELIST_ADD/SVM_ADDR_WHITELIST_DEL.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : configure fail
 */
DLLEXPORT drvError_t halSvmAccessWhitelistCfg(uint32_t devid, uint64_t va, uint64_t size, uint64_t flag);

/**
 * @ingroup driver
 * @brief Register an non-svm address range for host-side access.
 * @attention
 * 1. The returned access_va may be the same as va or different from va.
 * 2. The address range must be configured by halSvmAccessWhitelistCfg or hal_kernel_svm_access_whitelist_cfg,
 *    cannot merge va from different whitelist cfg to register one range.
 * 3. User need to ensure that the address is locked in memory, and cannot be swapped out.
 * 4. The va and size must be 4k aligned.
 * 5. Same addr not support register again.
 * 6. In PCIe scenarios and flag with SVM_REGISTER_FLAG_ACCESS_BY_DMA, this interface may fail in parameter checking,
 *    and it will not fail in other scenarios.
 * @param [in] devid: logical device id.
 * @param [in] va: non-svm virtual address.
 * @param [in] size: address range size.
 * @param [in] flag: register flag, SVM_REGISTER_FLAG_ACCESS_BY_DMA.
 * @param [out] access_va: host-accessible virtual address.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : register fail
 */
DLLEXPORT drvError_t halSvmAccessRegister(uint32_t devid, uint64_t va, uint64_t size, uint64_t flag,
                                          uint64_t *access_va);

/**
 * @ingroup driver
 * @brief Unregister an non-svm address range previously registered for host-side access.
 * @attention
 * 1. The va must be 4k aligned.
 * 2. In PCIe scenarios and flag with SVM_REGISTER_FLAG_ACCESS_BY_DMA, this interface may fail in parameter checking,
 *    and it will not fail in other scenarios.
 * @param [in] devid: logical device id.
 * @param [in] va: non-svm virtual address.
 * @param [in] flag: same as register flag.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : unregister fail
 */
DLLEXPORT drvError_t halSvmAccessUnRegister(uint32_t devid, uint64_t va, uint64_t flag);

/**
 * @ingroup driver
 * @brief Access data between local memory and registered SVM access address.
 * @attention null
 * @param [in] devid: logical device id.
 * @param [in] access_va: registered by halSvmAccessRegister.
 * @param [in] local_va: local virtual address.
 * @param [in] size: access size.
 * @param [in] flag: access direction flag, SVM_MEM_ACCESS_READ/SVM_MEM_ACCESS_WRITE.
 * @return DRV_ERROR_NONE : success
 * @return DV_ERROR_XXX : access fail
 */
DLLEXPORT drvError_t halSvmAccess(uint32_t devid, uint64_t access_va, uint64_t local_va, uint64_t size, uint32_t flag);

#endif
