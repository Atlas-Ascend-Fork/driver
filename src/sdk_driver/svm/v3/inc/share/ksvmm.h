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

#ifndef KSVMM_H
#define KSVMM_H

#include "ka_type.h"

#include "svm_addr_desc.h"

/* KSVMM: kern share virtual mem map, record mapped va and it's src addr. */
int ksvmm_add_seg(u32 udevid, int tgid, u64 start, struct svm_global_va *src_info);
/* If mem ref if bigger than 1(pin by others), del method will return -EBUSY. */
int ksvmm_del_seg(u32 udevid, int tgid, u64 start); /* start must same with the start in added */
int ksvmm_get_seg(u32 udevid, int tgid, u64 va, u64 *start, struct svm_global_va *src_info);
int ksvmm_for_each_seg(u32 udevid, int tgid, int (*func)(u64 start, struct svm_global_va *src_info, void *priv),
                       void *priv);
int ksvmm_pin_seg(u32 udevid, int tgid, u64 va, u64 size); /* va and size is in range of added mapped va */
int ksvmm_unpin_seg(u32 udevid, int tgid, u64 va, u64 size);
int ksvmm_check_range(u32 udevid, int tgid, u64 va, u64 size);

#endif
