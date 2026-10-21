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

#ifndef ESCHED_EVENT_COMMON_H
#define ESCHED_EVENT_COMMON_H

/* For efficient queue management, the follows Must be a power of two */
#define SCHED_NODE_EVENT_RES_NUM (1024 * 8)
#define SCHED_CPU_EVENT_RES_NUM (1024 * 2)
#define SCHED_PUBLISH_EVENT_QUE_DEPTH (1024 * 16)

#endif // ESCHED_EVENT_COMMON_H
