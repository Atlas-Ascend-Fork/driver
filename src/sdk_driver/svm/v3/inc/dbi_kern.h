/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef DBI_KERN_H
#define DBI_KERN_H

#include "ka_type.h"

#include "dbi_def.h"

/*
    DBI: device basic information
*/

/* query in host */
int svm_dbi_kern_query_npage_size(u32 udevid, u64 *npage_size);
int svm_dbi_kern_query_hpage_size(u32 udevid, u64 *hpage_size);
int svm_dbi_kern_query_gpage_size(u32 udevid, u64 *gpage_size);
int svm_dbi_kern_query_bus_inst_eid(u32 udevid, dbi_bus_inst_eid_t *eid);
int svm_dbi_kern_query_smmu_mode(u32 udevid, enum svm_smmu_mode *mode);

bool svm_dbi_kern_is_support_sva(u32 udevid);
bool svm_dbi_kern_is_cc_vm(void);
bool svm_dbi_kern_is_support_ubmem(u32 udevid);

/* cfg in host/device */
int svm_enable_ubmem(u32 udevid);

/* cfg in device */
int svm_enable_assign_gap(u32 udevid);

#endif
