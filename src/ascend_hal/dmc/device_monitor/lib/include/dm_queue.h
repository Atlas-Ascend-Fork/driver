/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DM_QUEUE_H
#define DM_QUEUE_H

#include "mmpa_api.h"

#define QUEUE_PATH "/mnt/queue/"
/* ID */
#define QUE_PROJECT_ID 0x18
#define DEFAULT_VFS_MODE 0750
#define DEFAULT_FILE_MODE 0640

#define DM_MSG_MODE_PRIVATE 0600

#define DM_QUEUE_NAME_LEN_MAX 8

#define DM_TIMEOUT_EACH_TIME_MS 100
#define DM_NULL_LONG (0xFFFFFFFFU)
#define DM_ERR 1
#define DEV_MON_MAIN_QUE_SIZE 0x100
#define DM_ERROR_WAIT_TIMEOUT 16

typedef struct st_dm_queue_msg_type {
    signed long mtype; /* type of message */
    void *mtext;       /* message text */
} ST_DM_QUEUE_MSG_TYPE;

unsigned long dm_queue_read(unsigned long ul_queue_id, unsigned long ul_time_out, void *p_buffer_addr,
                            unsigned long ul_buffer_size);

int dm_queue_asy_write(unsigned long ul_queue_id, const void *p_buffer_addr, unsigned long ul_buffer_size);

int dm_queue_delete(unsigned long ul_queue_id);

unsigned long dm_private_queue_create(const char *pch_name, mmKey_t msgkey, unsigned long ul_length,
                                      unsigned long *pul_queue_id, unsigned long ul_max_msg_size);

#endif
