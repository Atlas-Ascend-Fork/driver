/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MULTI_RBTREE_H
#define MULTI_RBTREE_H

#include "rbtree.h"
#include "list.h"

struct multi_rb_node {
    struct rb_node multi_rbtree_node;
    struct list_node list;
    uint8_t is_list_first;
};

void multi_rbtree_insert(uint64_t key, struct rbtree_root *root, struct multi_rb_node *node);
void multi_rbtree_erase(struct rbtree_root *root, struct multi_rb_node *node);
struct multi_rb_node *multi_rbtree_get(uint64_t key, struct rbtree_root *root);
struct multi_rb_node *multi_rbtree_get_upper_bound(uint64_t key, struct rbtree_root *root);
struct multi_rb_node *multi_rbtree_get_node_from_rb_node(struct rbtree_node *rb_node);

static inline void multi_rb_node_init(struct multi_rb_node *node)
{
    RB_CLEAR_NODE(&node->multi_rbtree_node.rbtree_node);
    INIT_LIST_HEAD(&node->list);
    node->is_list_first = 0;
}
#endif
