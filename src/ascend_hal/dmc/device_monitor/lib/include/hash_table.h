/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MON_HASH_TABLE_H
#define DEV_MON_HASH_TABLE_H
#pragma once
typedef struct hash_table hash_table;

#ifdef __cplusplus
extern "C" {
#endif

#define HASH_TABLE_TAG_LEN_MAX 256

/* new an instance of hash_table */
hash_table *hash_table_new(void);

/*
delete an instance of hash_table,
all values are removed automatically.
*/
void hash_table_delete(hash_table *ht); // lint !e101 !e132

/*
add or update a value to ht,
free_value(if not NULL) is called automatically when the value is removed.
return 0 if success, -1 if error occurred.
*/
#define hash_table_put(ht, tag, value) hash_table_put2(ht, tag, value, NULL);
int hash_table_put2(hash_table *ht, const char *tag, void *value, void (*free_value)(void *)); // lint !e101

/* get a value indexed by tag, return NULL if not found. */
void *hash_table_get(const hash_table *ht, const char *tag);

/* remove a value indexed by tag */
void hash_table_rm(hash_table *ht, const char *tag);

void display_hash_table(const hash_table *ht);

unsigned short count_hash_table(const hash_table *ht);

unsigned short get_hash_table_value(hash_table *ht, void **node_buff, unsigned short count);

#ifdef __cplusplus
}
#endif

#endif
