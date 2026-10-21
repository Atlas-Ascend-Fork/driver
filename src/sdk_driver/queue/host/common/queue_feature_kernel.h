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

#ifndef QUEUE_FEATURE_KERNEL_H
#define QUEUE_FEATURE_KERNEL_H

/* queue_feature_id is defined in queue_ioctl.h (shared with user-space) */
#include "queue_ioctl.h"

/*
 * Check if a feature exists in the feature array
 * @param features: feature array
 * @param feature_count: number of elements in feature array
 * @param feature: feature ID to check
 * @return: 1 if feature exists, 0 if not
 */
static inline int queue_feature_array_has(const enum queue_feature_id *features, unsigned int feature_count,
                                          enum queue_feature_id feature)
{
    unsigned int i;

    for (i = 0; i < feature_count; i++) {
        if (features[i] == feature) {
            return 1;
        }
    }
    return 0;
}

#endif /* QUEUE_FEATURE_KERNEL_H */
