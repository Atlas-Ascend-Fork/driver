/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef MEM_POOL_UK_MSG_H
#define MEM_POOL_UK_MSG_H

#include "svm_pub.h"

/* MEM_POOL_EVENT */
struct mem_pool_create_msg {
    u64 pool_id;   /* input */
    u64 va;        /* input */
    u64 size;      /* input */
    u64 global_va; /* input */
};

struct mem_pool_destroy_msg {
    u64 pool_id; /* input */
    u64 va;      /* output */
};

/* MEM_POOL_SET_ATTR_EVENT */
struct mem_pool_set_attr_msg {
    u64 pool_id; /* input: memory pool id */
    u64 attr;    /* input: attribute type (aclrtMemPoolAttr) */
    u64 value;   /* input: attribute value to set */
};

/* MEM_POOL_GET_ATTR_EVENT */
struct mem_pool_get_attr_msg {
    u64 pool_id; /* input: memory pool id */
    u64 attr;    /* input: attribute type (aclrtMemPoolAttr) */
    u64 value;   /* output: attribute value */
};

struct mem_pool_mem_free_msg {
    u64 pool_id; /* input */
    u64 va;      /* input */
    u64 size;    /* input */
};

#endif