/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef PROF_SAMPLE_AICPU_CHANNEL_H
#define PROF_SAMPLE_AICPU_CHANNEL_H

#include <stdint.h>

#define PROF_CHANNEL_AICPU (143U)
#define PROF_CHANNEL_CUS_AICPU (144U)

#define PROFILE_BUFFER_NUM (16U)
#define AICPU_BUF_UNIT (256U)
#define HOST_MOVE_AICPU_CHANNEL_COUNT (2U)

#define MEM_PAGE_SIZE (0x1000U)

#define PROF_HOST_STOP_STAGE_PAUSE 1U
#define PROF_HOST_STOP_STAGE_RELEASE 2U
#define PROF_HOST_STOP_STAGE_PAUSE_AND_RELEASE 3U

typedef enum {
    AICPU_CHANNEL_IDX = 0,
    CUS_AICPU_CHANNEL_IDX = 1
} host_move_aicpu_channel;

typedef struct aicpu_user_profile_buffer_info_t {
    unsigned int buffer_size;
    unsigned int reserved;
    uint64_t buffer_base_user_va;
    uint64_t buffer_read_ptr_user_va;
    uint64_t buffer_write_ptr_user_va;
} aicpu_user_profile_buffer_info_t;

typedef struct host_user_aicpu_buffer_info {
    aicpu_user_profile_buffer_info_t user_profile_buffer_info;
    uint32_t read_index;
    int host_pid;
    void *phase1_buffer_ptr;
    void *phase1_wptr_ptr;
    void *phase1_rptr_ptr;
} host_user_aicpu_buffer_info;

int register_aicpu_channel(unsigned int devid);
int register_cus_aicpu_channel(unsigned int devid);
#endif