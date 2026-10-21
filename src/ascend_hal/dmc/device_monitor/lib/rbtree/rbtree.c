/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "rbtree.h"

static inline void rbtree_dummy_propagate(struct rbtree_node *node, struct rbtree_node *stop)
{
    (void)node;
    (void)stop;
}
static inline void rbtree_dummy_copy(struct rbtree_node *rb_old, struct rbtree_node *rb_new)
{
    (void)rb_old;
    (void)rb_new;
}
static inline void rbtree_dummy_rotate(struct rbtree_node *rb_old, struct rbtree_node *rb_new)
{
    (void)rb_old;
    (void)rb_new;
}

static const struct rbtree_augment_callbacks g_dummy_callbacks = {
    .propagate = rbtree_dummy_propagate, .copy = rbtree_dummy_copy, .rotate = rbtree_dummy_rotate};

static inline struct rbtree_node *rbtree_red_parent(struct rbtree_node *red)
{
    return (struct rbtree_node *)(red->rbtree_parent_color);
}

static inline void rbtree_set_black(struct rbtree_node *rb)
{
    rb->rbtree_parent_color |= BLACK;
}

static inline void rbtree_set_parent(struct rbtree_node *rb, struct rbtree_node *p)
{
    rb->rbtree_parent_color = rb_black(rb) | (uintptr_t)p;
}

void rbtree_set_parent_color(struct rbtree_node *rb, struct rbtree_node *p, int color)
{
    rb->rbtree_parent_color = (unsigned long)(uintptr_t)p | (unsigned long)(unsigned int)color;
}

static void rbtree_change_child(struct rbtree_node *rb_old, struct rbtree_node *rb_new, struct rbtree_node *parent,
                                struct rbtree_root *root)
{
    if (parent != NULL) {
        if (parent->rbtree_left == rb_old) {
            parent->rbtree_left = rb_new;
        } else {
            parent->rbtree_right = rb_new;
        }
    } else {
        root->rbtree_node = rb_new;
    }
}

static inline void rbtree_rotate_set_parents(struct rbtree_node *rb_old, struct rbtree_node *rb_new,
                                             struct rbtree_root *root, int color)
{
    struct rbtree_node *parent = (struct rbtree_node *)rb_parent_node(rb_old);
    rb_new->rbtree_parent_color = rb_old->rbtree_parent_color;
    rbtree_set_parent_color(rb_old, rb_new, color);
    rbtree_change_child(rb_old, rb_new, parent, root);
}

static void rbtree_color_flips(struct rbtree_node *gparent, struct rbtree_node *gparent_child,
                               struct rbtree_node **parent, struct rbtree_node **node)
{
    rbtree_set_parent_color(gparent_child, gparent, BLACK);
    rbtree_set_parent_color(*parent, gparent, BLACK);
    *node = gparent;
    *parent = (struct rbtree_node *)rb_parent_node(*node);
    rbtree_set_parent_color(*node, *parent, RED);
}

static void rbtree_insert_parent_rotate(void (*augment_rotate)(struct rbtree_node *rb_old, struct rbtree_node *rb_new),
                                        int32_t flag, struct rbtree_node *node, struct rbtree_node **parent,
                                        struct rbtree_node **parent_child)
{
    if (flag == RIGHT_FLAG) {
        (*parent_child) = node->rbtree_left;
        (*parent)->rbtree_right = *parent_child;
        node->rbtree_left = *parent;
    } else {
        (*parent_child) = node->rbtree_right;
        (*parent)->rbtree_left = *parent_child;
        node->rbtree_right = *parent;
    }

    if (*parent_child != NULL) {
        rbtree_set_parent_color(*parent_child, *parent, BLACK);
    }

    rbtree_set_parent_color(*parent, node, RED);
    augment_rotate(*parent, node);
    *parent = node;

    if (flag == RIGHT_FLAG) {
        *parent_child = node->rbtree_right;
    } else {
        *parent_child = node->rbtree_left;
    }
}

static void rbtree_insert_gparent_rotate(int32_t flag, struct rbtree_root *root, struct rbtree_node *parent,
                                         struct rbtree_node *gparent, struct rbtree_node *gparent_child)
{
    if (flag == RIGHT_FLAG) {
        gparent->rbtree_left = gparent_child;
        parent->rbtree_right = gparent;
    } else {
        gparent->rbtree_right = gparent_child;
        parent->rbtree_left = gparent;
    }
    if (gparent_child != NULL) {
        rbtree_set_parent_color(gparent_child, gparent, BLACK);
    }
    rbtree_rotate_set_parents(gparent, parent, root, RED);
}

static void __rbtree_insert(struct rbtree_node *node, struct rbtree_root *root,
                            void (*augment_rotate)(struct rbtree_node *rb_old, struct rbtree_node *rb_new))
{
    struct rbtree_node *parent = rbtree_red_parent(node);
    struct rbtree_node *gparent = NULL;
    struct rbtree_node *tmp = NULL;
    int32_t flag;

    while (true) {
        if (!parent) {
            rbtree_set_parent_color(node, NULL, BLACK);
            break;
        } else if (rb_black(parent) != 0) {
            break;
        }

        gparent = rbtree_red_parent(parent);

        tmp = gparent->rbtree_right;
        if (parent != tmp) {
            if (tmp && rb_red(tmp)) {
                rbtree_color_flips(gparent, tmp, &parent, &node);
                continue;
            }

            tmp = parent->rbtree_right;
            flag = RIGHT_FLAG;
            if (node == tmp) {
                rbtree_insert_parent_rotate(rbtree_dummy_rotate, flag, node, &parent, &tmp);
            }

            rbtree_insert_gparent_rotate(flag, root, parent, gparent, tmp);
            augment_rotate(gparent, parent);
            break;
        } else {
            tmp = gparent->rbtree_left;
            if (tmp && rb_red(tmp)) {
                rbtree_color_flips(gparent, tmp, &parent, &node);
                continue;
            }

            tmp = parent->rbtree_left;
            flag = LEFT_FLAG;
            if (node == tmp) {
                rbtree_insert_parent_rotate(rbtree_dummy_rotate, flag, node, &parent, &tmp);
            }

            rbtree_insert_gparent_rotate(flag, root, parent, gparent, tmp);
            augment_rotate(gparent, parent);
            break;
        }
    }
}

void rbtree_insert_color(struct rbtree_node *node, struct rbtree_root *root)
{
    __rbtree_insert(node, root, rbtree_dummy_rotate);
}

void rbtree_link_node(struct rbtree_node *node, struct rbtree_node *parent, struct rbtree_node **rb_link)
{
    node->rbtree_parent_color = (uintptr_t)parent;
    node->rbtree_left = NULL;
    node->rbtree_right = NULL;

    *rb_link = node;
}

struct rbtree_node *rbtree_next(const struct rbtree_node *node)
{
    struct rbtree_node *parent = NULL;

    if (RB_EMPTY_NODE(node)) {
        return NULL;
    }

    if (node->rbtree_right != NULL) {
        node = node->rbtree_right;
        while (node->rbtree_left != NULL) {
            node = node->rbtree_left;
        }
        return (struct rbtree_node *)node;
    }

    while ((parent = (struct rbtree_node *)rb_parent_node(node)) && (node == parent->rbtree_right)) {
        node = parent;
    }

    return parent;
}

static struct rbtree_node *rbtree_erase_one_child(struct rbtree_node *node, struct rbtree_root *root,
                                                  struct rbtree_node *rb_right, struct rbtree_node **rb_left)
{
    struct rbtree_node *rebalance = NULL;
    struct rbtree_node *parent = NULL;
    unsigned long pc;

    if (!(*rb_left)) {
        pc = node->rbtree_parent_color;
        parent = (struct rbtree_node *)rb_parent(pc);
        rbtree_change_child(node, rb_right, parent, root);
        if (rb_right != NULL) {
            rb_right->rbtree_parent_color = pc;
        } else {
            rebalance = rb_is_black(pc) ? parent : NULL;
        }
        *rb_left = parent;
    } else {
        pc = node->rbtree_parent_color;
        (*rb_left)->rbtree_parent_color = pc;
        parent = rb_parent(pc);
        rbtree_change_child(node, *rb_left, parent, root);
        *rb_left = parent;
    }

    return rebalance;
}

static struct rbtree_node *rbtree_erase_multi_child(struct rbtree_node *node, struct rbtree_root *root,
                                                    const struct rbtree_augment_callbacks *augment,
                                                    struct rbtree_node *rb_right, struct rbtree_node **rb_left)
{
    struct rbtree_node *successor = rb_right;
    struct rbtree_node *rebalance = NULL;
    struct rbtree_node *parent = NULL;
    struct rbtree_node *child = NULL;
    unsigned long pc2;
    unsigned long pc;

    *rb_left = rb_right->rbtree_left;
    if (!(*rb_left)) {
        parent = successor;
        child = successor->rbtree_right;
        augment->copy(node, successor);
    } else {
        do {
            parent = successor;
            successor = *rb_left;
            *rb_left = (*rb_left)->rbtree_left;
        } while (*rb_left != NULL);
        child = successor->rbtree_right;
        parent->rbtree_left = child;
        successor->rbtree_right = rb_right;
        rbtree_set_parent(rb_right, successor);
        augment->copy(node, successor);
        augment->propagate(parent, successor);
    }

    *rb_left = node->rbtree_left;
    successor->rbtree_left = *rb_left;
    rbtree_set_parent(*rb_left, successor);

    pc = node->rbtree_parent_color;
    *rb_left = (struct rbtree_node *)rb_parent(pc);
    rbtree_change_child(node, successor, *rb_left, root);
    if (child != NULL) {
        successor->rbtree_parent_color = pc;
        rbtree_set_parent_color(child, parent, BLACK);
    } else {
        pc2 = successor->rbtree_parent_color;
        successor->rbtree_parent_color = pc;
        rebalance = rb_is_black(pc2) ? parent : NULL;
    }
    *rb_left = successor;

    return rebalance;
}

static struct rbtree_node *rbtree_erase_augmented(struct rbtree_node *node, struct rbtree_root *root,
                                                  const struct rbtree_augment_callbacks *augment)
{
    struct rbtree_node *rb_left = node->rbtree_left;
    struct rbtree_node *rb_right = node->rbtree_right;
    struct rbtree_node *rebalance = NULL;

    if ((!rb_left) || (!rb_right)) {
        rebalance = rbtree_erase_one_child(node, root, rb_right, &rb_left);
    } else {
        rebalance = rbtree_erase_multi_child(node, root, augment, rb_right, &rb_left);
    }

    augment->propagate(rb_left, NULL);
    return rebalance;
}

static void rbtree_erase_parent_rotate(void (*augment_rotate)(struct rbtree_node *rb_old, struct rbtree_node *rb_new),
                                       struct rbtree_root *root, struct rbtree_node *rb_node,
                                       struct rbtree_node *parent, struct rbtree_node **sibling)
{
    if (rb_node->rbtree_parent_color == RIGHT_FLAG) { /* rbtree_erase_color function rb_node->rbtree_parent_color =
                                                         RIGHT_FLAG */
        rb_node->rbtree_right = (*sibling)->rbtree_left;
        parent->rbtree_right = rb_node->rbtree_right;
        (*sibling)->rbtree_left = parent;
    } else {
        rb_node->rbtree_right = (*sibling)->rbtree_right;
        parent->rbtree_left = rb_node->rbtree_right;
        (*sibling)->rbtree_right = parent;
    }
    rbtree_set_parent_color(rb_node->rbtree_right, parent, BLACK);
    rbtree_rotate_set_parents(parent, *sibling, root, RED);
    augment_rotate(parent, *sibling);
    *sibling = rb_node->rbtree_right;
}

static void rbtree_erase_sibling_rotate(void (*augment_rotate)(struct rbtree_node *rb_old, struct rbtree_node *rb_new),
                                        struct rbtree_node *parent, struct rbtree_node *rb_node,
                                        struct rbtree_node **sibling)
{
    if (rb_node->rbtree_parent_color == RIGHT_FLAG) { /* rbtree_erase_color function rb_node->rbtree_parent_color =
                                                         RIGHT_FLAG */
        rb_node->rbtree_right = rb_node->rbtree_left->rbtree_right;
        (*sibling)->rbtree_left = rb_node->rbtree_right;
        rb_node->rbtree_left->rbtree_right = *sibling;
        parent->rbtree_right = rb_node->rbtree_left;
    } else {
        rb_node->rbtree_right = rb_node->rbtree_left->rbtree_left;
        (*sibling)->rbtree_right = rb_node->rbtree_right;
        rb_node->rbtree_left->rbtree_left = *sibling;
        parent->rbtree_left = rb_node->rbtree_left;
    }
    if (rb_node->rbtree_right != NULL) {
        rbtree_set_parent_color(rb_node->rbtree_right, *sibling, BLACK);
    }
    augment_rotate(*sibling, rb_node->rbtree_left);
    rb_node->rbtree_right = *sibling;
    *sibling = rb_node->rbtree_left;
}

static void rbtree_insert_parent_rotate_color_flips(struct rbtree_root *root, struct rbtree_node *sibling,
                                                    struct rbtree_node *parent, struct rbtree_node *rb_node,
                                                    void (*augment_rotate)(struct rbtree_node *rb_old,
                                                                           struct rbtree_node *rb_new))
{
    if (rb_node->rbtree_parent_color == RIGHT_FLAG) { /* rbtree_erase_color function rb_node->rbtree_parent_color =
                                                         RIGHT_FLAG */
        rb_node->rbtree_left = sibling->rbtree_left;
        parent->rbtree_right = rb_node->rbtree_left;
        sibling->rbtree_left = parent;
    } else {
        rb_node->rbtree_left = sibling->rbtree_right;
        parent->rbtree_left = rb_node->rbtree_left;
        sibling->rbtree_right = parent;
    }
    rbtree_set_parent_color(rb_node->rbtree_right, sibling, BLACK);
    if (rb_node->rbtree_left != NULL) {
        rbtree_set_parent(rb_node->rbtree_left, parent);
    }
    rbtree_rotate_set_parents(parent, sibling, root, BLACK);
    augment_rotate(parent, sibling);
}

static int32_t rbtree_erase_color_flips(struct rbtree_node *sibling, struct rbtree_node *sibling_child,
                                        struct rbtree_node **parent, struct rbtree_node **node)
{
    int32_t flag = -1;

    if (!sibling_child || rb_black(sibling_child)) {
        rbtree_set_parent_color(sibling, *parent, RED);
        if (rb_red(*parent)) {
            rbtree_set_black(*parent);
        } else {
            *node = *parent;
            *parent = (struct rbtree_node *)rb_parent_node(*node);
            if (*parent != NULL) {
                flag = CONTINUE_FLAG;
                return flag;
            }
        }
        flag = BREAK_FLAG;
        return flag;
    }

    return flag;
}

static int32_t rbtree_erase_color_augmented(struct rbtree_root *root, struct rbtree_node *rb_node,
                                            struct rbtree_node **parent, struct rbtree_node **sibling,
                                            struct rbtree_node **node)
{
    int32_t flag = -1;

    if (rb_red(*sibling)) {
        rbtree_erase_parent_rotate(rbtree_dummy_rotate, root, rb_node, *parent, sibling);
    }
    if (rb_node->rbtree_parent_color == RIGHT_FLAG) { /* rbtree_erase_color function rb_node->rbtree_parent_color =
                                                         RIGHT_FLAG */
        rb_node->rbtree_right = (*sibling)->rbtree_right;
    } else {
        rb_node->rbtree_right = (*sibling)->rbtree_left;
    }
    if (!rb_node->rbtree_right || rb_black(rb_node->rbtree_right)) {
        if (rb_node->rbtree_parent_color == RIGHT_FLAG) {
            rb_node->rbtree_left = (*sibling)->rbtree_left;
        } else {
            rb_node->rbtree_left = (*sibling)->rbtree_right;
        }
        flag = rbtree_erase_color_flips(*sibling, rb_node->rbtree_left, parent, node);
        if ((flag == BREAK_FLAG) || (flag == CONTINUE_FLAG)) {
            goto out;
        }
        rbtree_erase_sibling_rotate(rbtree_dummy_rotate, *parent, rb_node, sibling);
    }
    rbtree_insert_parent_rotate_color_flips(root, *sibling, *parent, rb_node, rbtree_dummy_rotate);
    flag = BREAK_FLAG;

out:
    return flag;
}

static void rbtree_erase_color(struct rbtree_node *parent, struct rbtree_root *root,
                               void (*augment_rotate)(struct rbtree_node *rb_old, struct rbtree_node *rb_new))
{
    struct rbtree_node *sibling = NULL;
    struct rbtree_node *node = NULL;
    struct rbtree_node rb_node; /* rbtree_right is tmp1, rbtree_left is tmp2 */
    int32_t flag = -1;

    (void)augment_rotate;

    while (true) {
        sibling = parent->rbtree_right;
        rb_node.rbtree_parent_color = RIGHT_FLAG;
        if (node != sibling) {
            flag = rbtree_erase_color_augmented(root, &rb_node, &parent, &sibling, &node);
            if (flag == CONTINUE_FLAG) {
                continue;
            } else if (flag == BREAK_FLAG) {
                break;
            }
        } else {
            sibling = parent->rbtree_left;
            rb_node.rbtree_parent_color = LEFT_FLAG;
            flag = rbtree_erase_color_augmented(root, &rb_node, &parent, &sibling, &node);
            if (flag == CONTINUE_FLAG) {
                continue;
            } else if (flag == BREAK_FLAG) {
                break;
            }
        }
    }
}

void __rbtree_erase(struct rbtree_node *node, struct rbtree_root *root)
{
    struct rbtree_node *rebalance = NULL;
    rebalance = rbtree_erase_augmented(node, root, &g_dummy_callbacks);
    if (rebalance != NULL) {
        rbtree_erase_color(rebalance, root, rbtree_dummy_rotate);
    }
}

struct rbtree_node *rbtree_prev(const struct rbtree_node *node)
{
    struct rbtree_node *parent = NULL;

    if (RB_EMPTY_NODE(node)) {
        return NULL;
    }

    if (node->rbtree_left != NULL) {
        node = node->rbtree_left;
        while (node->rbtree_right != NULL) {
            node = node->rbtree_right;
        }
        return (struct rbtree_node *)node;
    }

    while ((parent = (struct rbtree_node *)rb_parent_node(node)) && (node == parent->rbtree_left)) {
        node = parent;
    }

    return parent;
}

struct rbtree_node *rbtree_first(const struct rbtree_root *root)
{
    struct rbtree_node *n = root->rbtree_node;

    if (!n) {
        return NULL;
    }
    while (n->rbtree_left != NULL) {
        n = n->rbtree_left;
    }
    return n;
}

struct rbtree_node *rbtree_last(const struct rbtree_root *root)
{
    struct rbtree_node *n;

    n = root->rbtree_node;
    if (!n) {
        return NULL;
    }
    while (n->rbtree_right != NULL) {
        n = n->rbtree_right;
    }
    return n;
}

void rbtree_replace_node(struct rbtree_node *victim, struct rbtree_node *new_, struct rbtree_root *root)
{
    struct rbtree_node *parent = rb_parent_node(victim);

    /* Copy the pointers/colour from the victim to the replacement */
    *new_ = *victim;

    /* Set the surrounding nodes to point to the replacement */
    if (victim->rbtree_left != NULL) {
        rbtree_set_parent(victim->rbtree_left, new_);
    }
    if (victim->rbtree_right != NULL) {
        rbtree_set_parent(victim->rbtree_right, new_);
    }
    rbtree_change_child(victim, new_, parent, root);
}

struct rb_node *rbtree_insert_get_node(uint64_t key, struct rbtree_root *root, struct rb_node *node)
{
    struct rbtree_node **tmp = &(root->rbtree_node);
    struct rb_node *this = NULL;
    struct rbtree_node *parent = NULL;

    while (*tmp != NULL) {
        this = rb_entry(*tmp, struct rb_node, rbtree_node);

        parent = *tmp;
        if (key < this->key) {
            tmp = &((*tmp)->rbtree_left);
        } else if (key > this->key) {
            tmp = &((*tmp)->rbtree_right);
        } else {
            /* return the same key */
            return this;
        }
    }

    /* Add new node and rebalance tree. */
    node->key = key;
    rbtree_link_node(&node->rbtree_node, parent, tmp);
    rbtree_insert_color(&node->rbtree_node, root);
    root->rbtree_len++;

    return node;
}

int rbtree_insert(uint64_t key, struct rbtree_root *root, struct rb_node *node)
{
    struct rb_node *this = NULL;

    this = rbtree_insert_get_node(key, root, node);
    if (this == node) {
        return 0;
    } else {
        return -1;
    }
}

void _rbtree_erase(struct rbtree_root *root, struct rbtree_node *node)
{
    __rbtree_erase(node, root);
    root->rbtree_len--;
}

void rbtree_erase(struct rbtree_root *root, struct rb_node *node)
{
    _rbtree_erase(root, &node->rbtree_node);
}

struct rb_node *rbtree_get(uint64_t key, struct rbtree_root *root)
{
    struct rbtree_node *tmp = root->rbtree_node;
    struct rb_node *this = NULL;

    while (tmp != NULL) {
        this = rb_entry(tmp, struct rb_node, rbtree_node);

        if (key < this->key) {
            tmp = tmp->rbtree_left;
        } else if (key > this->key) {
            tmp = tmp->rbtree_right;
        } else {
            return this;
        }
    }
    return NULL;
}

struct rb_node *rbtree_get_upper_bound(uint64_t key, struct rbtree_root *root)
{
    struct rbtree_node *tmp = root->rbtree_node;
    struct rb_node *tmp_upper = NULL;
    struct rb_node *this = NULL;

    while (tmp != NULL) {
        this = rb_entry(tmp, struct rb_node, rbtree_node);

        if (key < this->key) {
            tmp_upper = this;
            tmp = tmp->rbtree_left;
        } else if (key > this->key) {
            tmp = tmp->rbtree_right;
        } else {
            return rb_entry(rbtree_next(&this->rbtree_node), struct rb_node, rbtree_node);
        }
    }
    return (tmp_upper != NULL) ? tmp_upper : NULL;
}

bool rbtree_can_insert_range(struct rbtree_root *root, struct rb_range_handle *range, rb_range_handle_func get_range)
{
    struct rbtree_node **tmp = &(root->rbtree_node);
#ifndef COMPILE_UT /* device_monitor's UT macro */
    while (*tmp != NULL) {
        struct rb_range_handle tmp_range;

        get_range(*tmp, &tmp_range);
        if (range->end < tmp_range.start) {
            tmp = &((*tmp)->rbtree_left);
        } else if (range->start > tmp_range.end) {
            tmp = &((*tmp)->rbtree_right);
        } else {
            return false;
        }
    }
#endif
    return true;
}

int rbtree_insert_by_range(struct rbtree_root *root, struct rbtree_node *node, rb_range_handle_func get_range)
{
    struct rbtree_node **tmp = &(root->rbtree_node);
    struct rbtree_node *parent = NULL;
    struct rb_range_handle range;

    get_range(node, &range);
    while (*tmp != NULL) {
        struct rb_range_handle tmp_range;

        get_range(*tmp, &tmp_range);
        parent = *tmp;
        if (range.end < tmp_range.start) {
            tmp = &((*tmp)->rbtree_left);
        } else if (range.start > tmp_range.end) {
            tmp = &((*tmp)->rbtree_right);
        } else {
            return -1;
        }
    }

    /* Add new node and rebalance tree. */
    rbtree_link_node(node, parent, tmp);
    rbtree_insert_color(node, root);
    root->rbtree_len++;

    return 0;
}

struct rbtree_node *rbtree_search_by_range(struct rbtree_root *root, struct rb_range_handle *range,
                                           rb_range_handle_func get_range)
{
    struct rbtree_node *tmp = root->rbtree_node;

    while (tmp != NULL) {
        struct rb_range_handle tmp_range;

        get_range(tmp, &tmp_range);
        if (range->end < tmp_range.start) {
            tmp = tmp->rbtree_left;
        } else if (range->start > tmp_range.end) {
            tmp = tmp->rbtree_right;
        } else if (range->start >= tmp_range.start && range->end <= tmp_range.end) {
            return tmp;
        } else {
            return NULL;
        }
    }
    return NULL;
}
#ifndef COMPILE_UT /* device_monitor's UT macro */
struct rbtree_node *rbtree_search_upper_bound_range(struct rbtree_root *root, uint64_t val,
                                                    rb_range_handle_func get_range)
{
    struct rbtree_node *cur = root->rbtree_node;
    struct rbtree_node *next = NULL;

    while (cur != NULL) {
        struct rb_range_handle cur_range;

        get_range(cur, &cur_range);
        if (val < cur_range.start) {
            next = cur;
            cur = cur->rbtree_left;
        } else if (val > cur_range.end) {
            cur = cur->rbtree_right;
        } else {
            return rbtree_next(cur);
        }
    }
    return next;
}
#endif
struct rbtree_node *rbtree_erase_one_node(struct rbtree_root *root)
{
    struct rbtree_node *node = NULL;

    if (RB_EMPTY_ROOT(root) == true) {
        return NULL;
    }

    node = rbtree_first(root);
    if (node != NULL) {
        _rbtree_erase(root, node);
        RB_CLEAR_NODE(node);
    }

    return node;
}
