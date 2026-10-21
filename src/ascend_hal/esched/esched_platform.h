/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ESCHED_PLATFORM_H
#define ESCHED_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include "securec.h"
#include "ascend_hal.h"
#include "event_sched.h"
#include "dpa_user_interface.h"

#define SVM_SMM_MMAP_EVENT (DRV_SUBEVENT_SVM_DEV_OPEN_MSG + 6)
#define SVM_SMM_MUNMAP_EVENT (DRV_SUBEVENT_SVM_DEV_OPEN_MSG + 7)
#define SVM_VA_RESERVE_EVENT (DRV_SUBEVENT_SVM_DEV_OPEN_MSG + 13)

bool esched_is_host_virtual_dev(unsigned int dev_id);

unsigned int esched_get_sync_pid(unsigned int dst_engine);

unsigned int esched_get_sync_dst_engine(void);

void esched_fill_sync_msg_peer_platform(uint32_t dev_id, struct event_summary *event, const struct event_res *res);

int esched_wait_sync_event(uint32_t dev_id, struct sync_event_wait_info *wait_info, struct event_info *back_event_info);

bool esched_is_svm_alloc_event(uint32_t dev_id, uint32_t event_id, uint32_t sub_event_id);
#endif /* ESCHED_PLATFORM_H */
