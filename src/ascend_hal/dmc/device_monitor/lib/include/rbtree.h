/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef RBTREE_H
#define RBTREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef NULL
#define NULL 0
#endif

#define RED 0
#define BLACK 1

#define BREAK_FLAG 0
#define CONTINUE_FLAG 1
#define RIGHT_FLAG 2
#define LEFT_FLAG 3

#define rb_parent(nc) ((struct rbtree_node *)(uintptr_t)((nc) & ~3UL))
#define rb_parent_node(rb) rb_parent((rb)->rbtree_parent_color)

#define rb_is_black(nc) ((nc) & 1UL)
#define rb_black(rb) rb_is_black((rb)->rbtree_parent_color)
#define rb_red(rb) (!(rb_black(rb)))

#define RB_CLEAR_NODE(node) ((node)->rbtree_parent_color = (unsigned long)(node))
#define RB_EMPTY_NODE(node) ((node)->rbtree_parent_color == (unsigned long)(node))
#define RB_EMPTY_ROOT(root) ((root)->rbtree_node == NULL)

struct rbtree_node {
    unsigned long rbtree_parent_color;
    struct rbtree_node *rbtree_right;
    struct rbtree_node *rbtree_left;
};

struct rbtree_root {
    struct rbtree_node *rbtree_node;
    uint64_t rbtree_len;
};

struct rb_node {
    struct rbtree_node rbtree_node;
    uint64_t key;
};

struct rbtree_augment_callbacks {
    void (*propagate)(struct rbtree_node *node, struct rbtree_node *stop);
    void (*copy)(struct rbtree_node *rb_old, struct rbtree_node *rb_new);
    void (*rotate)(struct rbtree_node *rb_old, struct rbtree_node *rb_new);
};

#define RB_ROOT          \
    (struct rbtree_root) \
    {                    \
        NULL, 0          \
    }
#define rb_entry(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

void rbtree_set_parent_color(struct rbtree_node *rb, struct rbtree_node *p, int color);
void rbtree_insert_color(struct rbtree_node *node, struct rbtree_root *root);
void rbtree_link_node(struct rbtree_node *node, struct rbtree_node *parent, struct rbtree_node **rb_link);
struct rbtree_node *rbtree_next(const struct rbtree_node *node);
void __rbtree_erase(struct rbtree_node *node, struct rbtree_root *root);
struct rbtree_node *rbtree_prev(const struct rbtree_node *node);
struct rbtree_node *rbtree_first(const struct rbtree_root *root);
struct rbtree_node *rbtree_last(const struct rbtree_root *root);
void rbtree_replace_node(struct rbtree_node *victim, struct rbtree_node *new_, struct rbtree_root *root);

#define rbtree_node_for_each(node, root) for ((node) = rbtree_first(root); (node); (node) = rbtree_next(node))
#define rbtree_node_for_each_prev(node, root) for ((node) = rbtree_last(root); (node); (node) = rbtree_prev(node))
#define rbtree_node_for_each_prev_safe(node, n, root)                                           \
    for ((node) = rbtree_last(root), (n) = ((node) == NULL) ? NULL : rbtree_prev(node); (node); \
         (node) = (n), (n) = ((node) == NULL) ? NULL : rbtree_prev(node))

static inline void rbtree_init(struct rbtree_root *root)
{
    root->rbtree_node = NULL;
    root->rbtree_len = 0;
}

/**
 * @brief This interface is used to insert rb_node to rbtree and return a pointer of struct rb_node.
 * @attention null
 * @return struct rb_node *:
 *  1. if the tree already has a node with the same key, then return the node with the same key
 *  2. if the tree does not have a node with the same key, then return the node to be inserted
 */
struct rb_node *rbtree_insert_get_node(uint64_t key, struct rbtree_root *root, struct rb_node *node);
int rbtree_insert(uint64_t key, struct rbtree_root *root, struct rb_node *node);
void rbtree_erase(struct rbtree_root *root, struct rb_node *node);
struct rb_node *rbtree_get(uint64_t key, struct rbtree_root *root);
struct rb_node *rbtree_get_upper_bound(uint64_t key, struct rbtree_root *root);

struct rb_range_handle {
    uint64_t start;
    uint64_t end;
};

typedef void (*rb_range_handle_func)(struct rbtree_node *node, struct rb_range_handle *range_handle);

void _rbtree_erase(struct rbtree_root *root, struct rbtree_node *node);
bool rbtree_can_insert_range(struct rbtree_root *root, struct rb_range_handle *range, rb_range_handle_func get_range);
int rbtree_insert_by_range(struct rbtree_root *root, struct rbtree_node *node, rb_range_handle_func get_range);
struct rbtree_node *rbtree_search_by_range(struct rbtree_root *root, struct rb_range_handle *range,
                                           rb_range_handle_func get_range);
struct rbtree_node *rbtree_search_upper_bound_range(struct rbtree_root *root, uint64_t val,
                                                    rb_range_handle_func get_range);

struct rbtree_node *rbtree_erase_one_node(struct rbtree_root *root);

#endif
