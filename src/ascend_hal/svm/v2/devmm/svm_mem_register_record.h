/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVM_MEM_REGISTER_RECORD_H
#define SVM_MEM_REGISTER_RECORD_H
#include "ascend_hal.h"

enum mem_register_src_side {
    HOST_MEM_REGISTER = 0,
    DEV_MEM_REGISTER,
    MEM_REGISTER_MAX_SIDE
};

DVresult svm_mem_register_record_add(uint32_t devid, DVdeviceptr src_ptr, uint64_t size, uint32_t map_type,
                                     DVdeviceptr dst_ptr, uint32_t flag);
void svm_mem_register_record_del(uint32_t devid, DVdeviceptr src_ptr, uint32_t map_type);
void svm_mem_register_records_del(uint32_t devid);

DVresult svm_mem_register_record_query(DVdeviceptr src_ptr, uint32_t devid, uint32_t side, DVdeviceptr *dst_ptr,
                                       uint32_t *flag);
DVresult svm_mem_register_record_range_query(DVdeviceptr src_ptr, uint32_t devid, uint32_t side, uint32_t *map_type,
                                             DVdeviceptr *dst_ptr);
DVresult svm_mem_register_record_range_query_single(DVdeviceptr src_ptr, uint32_t devid, uint32_t side,
                                                    uint32_t tree_type, uint32_t *map_type, DVdeviceptr *dst_ptr);

bool svm_mem_register_record_is_exist(DVdeviceptr ptr, uint32_t side);

enum register_record_tree_type {
    NO_DMA_MAP_RECORD_TREE = 0,
    DMA_MAP_RECORD_TREE,
    RECORD_TREE_MAX_TYPE
};

#endif
