/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVM_DBI_H
#define SVM_DBI_H
#include <stdbool.h>

#include "ascend_hal.h"

#include "svm_pub.h"

static inline u32 svm_get_host_devid(void)
{
    u32 host_devid = SVM_DEFAULT_HOST_DEVID;
    (void)halGetHostID(&host_devid);
    return host_devid;
}

u32 svm_get_cur_server_id(void);
u32 svm_get_device_connect_type(u32 devid);

int svm_dbi_query_npage_size(u32 devid, u64 *npage_size);
int svm_dbi_query_hpage_size(u32 devid, u64 *hpage_size);
int svm_dbi_query_gpage_size(u32 devid, u64 *gpage_size);
int svm_dbi_query_host_align_page_size(u32 devid, u64 *page_size);
int svm_dbi_query_d2h_acc_mask(u32 devid, u64 *acc_mask);

bool svm_dbi_is_support_sva(u32 devid);
bool svm_dbi_is_cc_vm(void);
bool svm_dbi_is_support_assign_gap(u32 devid);
bool svm_dbi_is_support_host_access_dev(u32 devid);

#endif
