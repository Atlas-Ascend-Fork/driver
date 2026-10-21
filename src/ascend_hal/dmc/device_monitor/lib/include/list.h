/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef LIST_H
#define LIST_H
#include <stdint.h>

struct list_node {
    struct list_node *next;
    struct list_node *prev;
};

static inline void INIT_LIST_HEAD(struct list_node *list)
{
    list->next = list;
    list->prev = list;
}

static inline void _list_add_node(struct list_node *new_, struct list_node *prev, struct list_node *next)
{
    next->prev = new_;
    new_->next = next;
    new_->prev = prev;
    prev->next = new_;
}

static inline void list_add_node(struct list_node *new_, struct list_node *head)
{
    _list_add_node(new_, head, head->next);
}

static inline void list_add_node_tail(struct list_node *new_, struct list_node *head)
{
    _list_add_node(new_, head->prev, head);
}

static inline void _list_del_node(struct list_node *prev, struct list_node *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del_node(struct list_node *entry)
{
    _list_del_node(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

static inline int list_empty(const struct list_node *head)
{
    return head->next == head;
}

#define list_for_each_node(pos, head) for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)
#define list_for_each_node_safe(pos, n, head) \
    for ((pos) = (head)->next, (n) = (pos)->next; (pos) != (head); (pos) = (n), (n) = (pos)->next)

#endif
