/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef PROF_SAMPLE_TS_CHANNEL_H
#define PROF_SAMPLE_TS_CHANNEL_H

#include <stdint.h>
#include "ascend_hal_error.h"

/* Channel ID definitions*/
#define STARS_PROFILE_SOC_LOG_CHANNEL (50U)    /* SOC log channel*/
#define STARS_ACC_PROFILE_BUFFER_CHANNEL (53U) /* ACC performance profile channel*/

/* Buffer parameters*/
#define PROFILE_LOG_BUFFER_NUM (16U)         /* Sub channel number*/
#define STARS_LOG_BUF_UNIT (32U)             /* SOC log buffer unit size*/
#define STARS_ACC_PROFILE_BUF_UNIT (128U)    /* ACC profile buffer unit size*/
#define HOST_MOVE_PROFILE_CHANNEL_COUNT (2U) /* Total channel count*/

#define MEM_PAGE_SIZE (0x1000U) /* page size 4KB*/
#define SIZE_2M (0x200000U)

/* SVM registration entry table*/
#define PROF_SVM_REG_ENTRY_COUNT (3U)

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(val, align) ((val) & ~((typeof(val))(align) - 1))
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(val, align) (((val) + ((typeof(val))(align) - 1)) & ~((typeof(val))(align) - 1))
#endif

enum prof_svm_reg_type {
    PROF_SVM_REG_WPTR = 0,
    PROF_SVM_REG_RPTR,
    PROF_SVM_REG_BUFFER_BASE
};

struct prof_svm_reg_entry {
    uint64_t user_va;        /* user virtual address during registration */
    uint64_t size;           /* memory size during registration */
    const char *name;        /* name, used for log output */
    uint64_t *access_va_ptr; /* a pointer to the corresponding access_va field in host_info */
};

/* Channel index enumeration*/
typedef enum {
    STARS_PROFILE_SOC_LOG_CHANNEL_IDX = 0,
    STARS_ACC_PROFILE_BUFFER_CHANNEL_IDX
} host_move_profile_channel;

typedef struct ts_user_profile_buffer_info_t {
    unsigned int buffer_index;         /* Buffer index*/
    unsigned int buffer_size;          /* Buffer size*/
    uint64_t buffer_base_user_va;      /* Buffer base address (user VA)*/
    uint64_t buffer_read_ptr_user_va;  /* Read pointer address (user VA)*/
    uint64_t buffer_write_ptr_user_va; /* Write pointer address (user VA)*/
} ts_user_profile_buffer_info_t;

/**
 * Host side buffer info
 */
typedef struct host_user_profile_buffer_info {
    ts_user_profile_buffer_info_t user_profile_buffer_info;
    uint32_t read_index;
    int host_pid;
    uint64_t wptr_access_va;
    uint64_t wptr_aligned_user_va;
    uint64_t rptr_access_va;
    uint64_t rptr_aligned_user_va;
    uint64_t buffer_base_access_va;
} host_user_profile_buffer_info;

/* Register SOC log channel*/
int register_soc_log_channel(unsigned int devid);

/* Register ACC profile channel*/
int register_acc_profile_channel(unsigned int devid);

#endif