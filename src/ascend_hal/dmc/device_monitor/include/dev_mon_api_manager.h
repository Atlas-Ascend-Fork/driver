/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MON_API_MANAGER_H
#define DEV_MON_API_MANAGER_H
#include "hash_table.h"
#include "dm_common.h"
#include "device_monitor_type.h"

#define DEV_MON_ADD_CMD(table, op1, op2, op_prop, uid, op_type, handle)          \
    if (cmd_hash_insert(table, op1, op2, op_prop, uid, op_type, handle) != OK) { \
        DEV_MON_ERR("cmd add failed!,op_1:%d,op_2:%d\n", op1, op2);              \
        return FAILED;                                                           \
    }

#ifndef __DEV_MON_HOST__
signed int IT_cmd_insert_to_table(hash_table *hashtable);
signed int davinci_cmd_insert_to_table(hash_table *hashtable);
int dev_mon_cmd_hash_get(hash_table *hashtable, DM_RECV_ST *msg, void **value);
signed int cmd_hash_insert(hash_table *hashtable, unsigned char op_fun, unsigned char op_cmd, unsigned char cntr_prop,
                           unsigned char uid, unsigned char op_type, dev_mon_cmd_handle handle);
#else
static inline signed int IT_cmd_insert_to_table(hash_table *hashtable)
{
    return 0;
}
static inline signed int davinci_cmd_insert_to_table(hash_table *hashtable)
{
    return 0;
}
static inline int dev_mon_cmd_hash_get(hash_table *hashtable, DM_RECV_ST *msg, **value)
{
    return -1;
}
signed int cmd_hash_insert(hash_table *hashtable, unsigned char op_fun, unsigned char op_cmd, unsigned char cntr_prop,
                           unsigned char uid, unsigned char op_type, dev_mon_cmd_handle handle)
{
    return -1;
}
#endif

#endif
