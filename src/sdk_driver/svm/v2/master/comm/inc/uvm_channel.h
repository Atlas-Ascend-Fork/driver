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

#ifndef UVM_CHANNEL_H
#define UVM_CHANNEL_H

#include "svm_heap_mng.h"
#include "devmm_proc_info.h"

int devmm_uvm_chan_page_fault_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg,
                                          u32 *ack_len);
int devmm_uvm_chan_free_mem_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg,
                                        u32 *ack_len);
int devmm_uvm_chan_prefetch_ack_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg,
                                        u32 *ack_len);
int devmm_uvm_chan_swap_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg,
                                    u32 *ack_len);
int devmm_uvm_chan_release_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_svm_heap *heap, void *msg,
                                       u32 *ack_len);
#endif /* UVM_CHANNEL_H */