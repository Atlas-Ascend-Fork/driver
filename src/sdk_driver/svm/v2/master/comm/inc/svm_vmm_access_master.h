/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
#ifndef SVM_VMM_ACCESS_MASTER_H
#define SVM_VMM_ACCESS_MASTER_H

#include "devmm_proc_info.h"
#include "svm_vmma_mng.h"

int svm_vmm_set_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma, u32 access_udevid,
                       drv_mem_access_type type);
int svm_vmm_unset_access(struct devmm_svm_process *svm_proc, struct devmm_vmma_struct *vmma, u32 access_udevid);

#endif /* SVM_VMM_ACCESS_MASTER_H */
