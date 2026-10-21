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
#ifndef KA_KERNEL_ADPAT_H
#define KA_KERNEL_ADPAT_H

/* Use the native kernel interfaces on the device */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
enum profile_type {
    PROFILE_TASK_EXIT,
    PROFILE_MUNMAP
};

int profile_event_register(enum profile_type type, ka_notifier_block_t *n);
int profile_event_unregister(enum profile_type type, ka_notifier_block_t *n);
#endif

#endif