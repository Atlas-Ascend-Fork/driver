/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DM_LIST_H
#define DM_LIST_H
#include <stdbool.h>
#include "mmpa_api.h"
typedef int (*LIST_CMP_T)(const void *item1, const void *item2);
typedef void (*LIST_DEL_T)(void *item);
typedef void (*LIST_HNDL_T)(void *item, void *user_data, int data_len);
typedef struct LIST_NODE_S LIST_NODE_T;
/* the node structure in linked list */
struct LIST_NODE_S {
    void *list; /* pointer to the list table where the node itself is located */

    void *item;        /* content mounted on the node (such as poller entry information, intf in intf_list, etc.)) */
    LIST_NODE_T *prev; /* previous node of the list */
    LIST_NODE_T *next; /* next node of the list */

    int removed; /* indicates whether a node is deleted */
    int ref_cnt; /* calculation of the number of times a node is introduced */
};

/* LIST linked list data structure */
struct LIST_S {
    int size;         /* number of nodes in the list */
    LIST_NODE_T head; /* head node */
    LIST_CMP_T cmp;   /* node comparison operation handle */
    LIST_DEL_T del;   /* node delete operation handle */

    int (*lock)(mmMutex_t *mutex);
    int (*unlock)(mmMutex_t *mutex);
    mmMutex_t mutex;
};
typedef struct LIST_S LIST_T;

/* structure used for traversing nodes in a list, making it convenient to access all nodes */
typedef struct LIST_ITER_S {
    LIST_T *i_list;     /* list */
    LIST_NODE_T *i_cur; /* nodes */
} LIST_ITER_T;
extern int list_create(LIST_T **list, LIST_CMP_T cmp, LIST_DEL_T del);
extern void list_destroy(LIST_T *list);
extern int list_size(LIST_T *list);
extern bool list_is_empty(LIST_T *list);
extern void *list_to_item(LIST_NODE_T *node);
extern void *list_first_item(LIST_T *list);
extern void *list_last_item(LIST_T *list);
extern int list_append(LIST_T *list, void *item);
int list_append_get_node(LIST_T *list, void *item, LIST_NODE_T **new_node);
void list_node_put(LIST_T *list, LIST_NODE_T *node);
extern int list_prepend(LIST_T *list, void *item);
extern int list_insert_pos(LIST_T *list, void *item, int pos);
extern int list_insert_ascend(LIST_T *list, void *item, LIST_CMP_T custom_cmp);
extern int list_insert_descend(LIST_T *list, void *item, LIST_CMP_T custom_cmp);
extern LIST_NODE_T *list_search(LIST_T *list, const void *tag, LIST_CMP_T custom_cmp);
extern int list_remove(LIST_T *list, LIST_NODE_T *node);
extern int list_remove_by_tag(LIST_T *list, const void *tag, LIST_CMP_T custom_cmp);
extern int list_del_by_tag(LIST_T *list, const void *tag, LIST_CMP_T custom_cmp);
extern void list_iter_init(LIST_T *list, LIST_ITER_T *iter);
extern LIST_NODE_T *list_iter_next(LIST_ITER_T *iter);
extern void list_iter_destroy(LIST_ITER_T *iter);
extern void list_iterate(LIST_T *list, LIST_HNDL_T hndl, void *user_data, int data_len);

#endif
