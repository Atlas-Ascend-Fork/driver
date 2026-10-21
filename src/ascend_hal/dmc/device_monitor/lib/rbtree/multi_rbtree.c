/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "multi_rbtree.h"

void multi_rbtree_insert(uint64_t key, struct rbtree_root *root, struct multi_rb_node *node)
{
    struct multi_rb_node *this = NULL;
    struct rb_node *rb_node = NULL;

    rb_node = rbtree_insert_get_node(key, root, &node->multi_rbtree_node);
    if (rb_node != &node->multi_rbtree_node) {
        node->multi_rbtree_node.key = key;
        node->is_list_first = 0;
        this = rb_entry(rb_node, struct multi_rb_node, multi_rbtree_node);
        list_add_node_tail(&node->list, &this->list);
        root->rbtree_len++;
    } else {
        node->is_list_first = 1;
    }
}

void multi_rbtree_erase(struct rbtree_root *root, struct multi_rb_node *node)
{
    struct multi_rb_node *tmp = NULL;

    if (list_empty(&node->list)) {
        rbtree_erase(root, &node->multi_rbtree_node);
    } else {
        if (node->is_list_first) {
            tmp = rb_entry(node->list.next, struct multi_rb_node, list);
            rbtree_replace_node(&node->multi_rbtree_node.rbtree_node, &tmp->multi_rbtree_node.rbtree_node, root);
            tmp->is_list_first = 1;
        }
        list_del_node(&node->list);
        root->rbtree_len--;
    }
}

struct multi_rb_node *multi_rbtree_get_with_list(struct multi_rb_node *node)
{
    if (list_empty(&node->list)) {
        return node;
    } else {
        return rb_entry(node->list.prev, struct multi_rb_node, list);
    }
}

struct multi_rb_node *multi_rbtree_get(uint64_t key, struct rbtree_root *root) // lint !e527
{
    struct multi_rb_node *this = NULL;
    struct multi_rb_node *node = NULL;
    struct rb_node *rb_node = NULL;

    rb_node = rbtree_get(key, root);
    if (rb_node != NULL) {
        this = rb_entry(rb_node, struct multi_rb_node, multi_rbtree_node);
        node = multi_rbtree_get_with_list(this);
        return node;
    }

    return NULL;
}

struct multi_rb_node *multi_rbtree_get_upper_bound(uint64_t key, struct rbtree_root *root)
{
    struct multi_rb_node *this = NULL;
    struct rb_node *rb_node = NULL;

    rb_node = rbtree_get_upper_bound(key, root);
    if (rb_node != NULL) {
        this = rb_entry(rb_node, struct multi_rb_node, multi_rbtree_node);
        return multi_rbtree_get_with_list(this);
    }

    return NULL;
}

struct multi_rb_node *multi_rbtree_get_node_from_rb_node(struct rbtree_node *rb_node)
{
    struct rb_node *tmp = rb_entry(rb_node, struct rb_node, rbtree_node);
    return rb_entry(tmp, struct multi_rb_node, multi_rbtree_node);
}
