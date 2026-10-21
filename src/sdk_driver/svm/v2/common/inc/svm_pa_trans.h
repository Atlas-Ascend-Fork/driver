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
#ifndef SVM_PA_TRANS_H
#define SVM_PA_TRANS_H

#include "svm_pa_seg.h"

#define SVM_PA_TRANS_FLAG_SVSP (1U << 0)

int svm_pa_seg_v2p(u32 dst_udevid, const struct svm_pa_seg vm_pa_seg[], u32 vm_seg_num, struct svm_pa_seg pm_pa_seg[],
                   u32 *pm_seg_num);
int svm_pa_seg_d2h(u32 src_udevid, struct svm_pa_seg pa_seg[], u32 seg_num);
int svm_pa_seg_h2d(u32 dst_udevid, struct svm_pa_seg pa_seg[], u32 seg_num, u32 flag);
/* src_udevid is host view */
int svm_pa_seg_d2d(u32 src_udevid, u32 dst_udevid, struct svm_pa_seg pa_seg[], u32 seg_num);

#endif /* SVM_PA_TRANS_H */
