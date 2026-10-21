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

#ifndef DEVMM_PAGEFAULT_INFO_H
#define DEVMM_PAGEFAULT_INFO_H

#include "ka_type.h"

#include "ascend_hal_define.h"
#include "devmm_proc_info.h"
#include "svm_ioctl.h"

/* Record pagefault failure info when device reports SVM page fault error.
 * Called from devmm_chan_report_process_status_d2h when pid_status == STATUS_SVM_PAGE_FALUT_ERR_OCCUR.
 * Finds the left-nearest contiguous mapped segment and extracts side/devid/shared info. */
void devmm_pagefault_info_record_access(struct devmm_svm_process *svm_process, u32 devid, u64 fault_va);

/* Clear pagefault info record for a specific device. Called on device offline. */
void devmm_pagefault_info_clear_by_devid(struct devmm_svm_process *svm_proc, u32 devid);

/* Ioctl handler: query pagefault info for a specific device. */
int devmm_ioctl_pagefault_info_query(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);

/* Ioctl handler: clear pagefault info record for a specific device. */
int devmm_ioctl_pagefault_info_clear(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg);

/* Initialize pagefault_info_records rwlocks for a new process. */
void devmm_pagefault_info_proc_init(struct devmm_svm_process *svm_proc);

#endif /* DEVMM_PAGEFAULT_INFO_H */
