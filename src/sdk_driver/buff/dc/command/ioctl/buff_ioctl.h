/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef BUFF_IOCTL_H
#define BUFF_IOCTL_H
#include <linux/ioctl.h>
#include "hal_pkg/buff_pkg.h"

struct xsm_reg_arg {
    int algo;
    unsigned int priv_flag;
    unsigned long pool_size;
    unsigned int block_size; /* used for blk algorithm */
    unsigned int key_len;
    const char *key;
};

struct xsm_task_arg {
    int pool_id;
    int pid;
    GroupShareAttr attr;
};

struct xsm_task_attach_arg {
    int pool_id;
    int timeout;
    unsigned long long uid;
    unsigned int cache_type;
};

struct xsm_block_alloc_arg {
    unsigned long flag;   /* input */
    unsigned long size;   /* input */
    unsigned long offset; /* output */
    unsigned int blkid;
};

struct xsm_block_free_arg { /* for free and put */
    unsigned long offset;   /* input */
};

struct xsm_block_get_arg {
    unsigned long offset;       /* input, offset in the range of alloc offset */
    unsigned long alloc_size;   /* output */
    unsigned long alloc_offset; /* output */
    unsigned int blkid;
};

struct xsm_block_arg {
    int pool_id; /* get: output, others: input */
    union {
        struct xsm_block_alloc_arg alloc;
        struct xsm_block_free_arg free;
        struct xsm_block_get_arg get;
    };
};

struct xsm_pool_query_arg {
    const char *key;
    unsigned int key_len;
    int pool_id;
};

struct xsm_query_pool_task_arg {
    int pool_id;
    int *pid;
    int pid_num; /* intput, output  */
};

struct xsm_query_task_pool_arg {
    union {
        int type;
        int query_num;
    };
    int pid;
    int *pool_id;
    int pool_num; /* intput, output  */
};

struct xsm_prop_op_arg {
    const char *prop;
    unsigned int prop_len;
    int owner;
    int pool_id;
    int op;
    unsigned long value;
};

struct xsm_poll_exit_task_arg {
    int pool_id;
    unsigned long long uid;
};

struct xsm_cache_create_arg {
    int pool_id;
    unsigned int dev_id;
    unsigned int mem_flag;
    unsigned long long mem_size;
    unsigned long long alloc_max_size;
};

struct xsm_cache_destroy_arg {
    int pool_id;
    unsigned int dev_id;
};

struct xsm_query_cache_arg {
    int pool_id;                       /* intput */
    unsigned int dev_id;               /* intput */
    unsigned int cache_cnt;            /* intput, output */
    GrpQueryGroupAddrInfo *cache_buff; /* output  */
};

struct xsm_query_pool_flag_arg {
    int pool_id;            /* intput */
    unsigned int priv_flag; /* output */
};

struct xsm_check_va_arg {
    unsigned long va; /* intput */
    int pool_id;      /* intput */
    int result;       /* output */
};

#define BUFF_FEATURE_DEV_NUM 1140

/* buff feature ID definitions (shared between kernel and user-space) */
enum buff_feature_id {
    BUFF_FEATURE_EXTERNAL_CDEV = 0,
    BUFF_FEATURE_SUPPORT_VMA,
    BUFF_FEATURE_SHARE_LOG,
    BUFF_FEATURE_KA_ALLOC_INTERFACE,
    BUFF_FEATURE_HOST_LOG,
    BUFF_FEATURE_SUPPORT_SP,
    BUFF_FEATURE_SUPPORT_HUGE_PAGE,
    BUFF_FEATURE_NO_SUPPORT_DVPP_MZ,
    BUFF_FEATURE_MAX
};
#define BUFF_FEATURE_MAX_LIMIT 10000

struct xsm_feature_get_arg {
    /* input: user-space's current feature count (covers both chip and version features) */
    unsigned int user_feature_num;
    /* output: feature support bitmap (only indices < user_feature_num are valid) */
    unsigned char feature_bitmaps[BUFF_FEATURE_DEV_NUM][BUFF_FEATURE_MAX];
};

#define XSMEM_POOL_REGISTER _IOW('X', 1, struct xsm_reg_arg)
#define XSMEM_POOL_UNREGISTER _IO('X', 2)
#define XSMEM_POOL_TASK_ADD _IOW('X', 3, struct xsm_task_arg)
#define XSMEM_POOL_TASK_DEL _IOW('X', 4, struct xsm_task_arg)
#define XSMEM_POOL_ATTACH _IOWR('X', 5, struct xsm_task_attach_arg)
#define XSMEM_POOL_DETACH _IO('X', 6)
#define XSMEM_BLOCK_ALLOC _IOWR('X', 7, struct xsm_block_arg)
#define XSMEM_BLOCK_FREE _IOW('X', 8, struct xsm_block_arg)
#define XSMEM_BLOCK_GET _IOWR('X', 9, struct xsm_block_arg)
#define XSMEM_BLOCK_PUT _IOW('X', 10, struct xsm_block_arg)
#define XSMEM_POOL_ID_QUERY _IOWR('X', 11, struct xsm_pool_query_arg)
#define XSMEM_POOL_NAME_QUERY _IOWR('X', 12, struct xsm_pool_query_arg)
#define XSMEM_POOL_TASK_QUERY _IOWR('X', 13, struct xsm_query_pool_task_arg)
#define XSMEM_POOL_TASK_ATTR_QUERY _IOWR('X', 14, struct xsm_task_arg)
#define XSMEM_TASK_POOL_QUERY _IOWR('X', 15, struct xsm_query_task_pool_arg)
#define XSMEM_PROP_OP _IOWR('X', 16, struct xsm_prop_op_arg)
#define XSMEM_POLL_EXIT_TASK _IOWR('X', 17, struct xsm_poll_exit_task_arg)
#define XSMEM_CACHE_CREATE _IOW('X', 18, struct xsm_cache_create_arg)
#define XSMEM_CACHE_DESTROY _IOW('X', 19, struct xsm_cache_destroy_arg)
#define XSMEM_CACHE_QUERY _IOWR('X', 20, struct xsm_query_cache_arg)
#define XSMEM_POOL_FLAG_QUERY _IOWR('X', 21, struct xsm_query_pool_flag_arg)
#define XSMEM_VADDR_CHECK _IOWR('X', 22, struct xsm_check_va_arg)
#define XSMEM_FEATURE_GET _IOWR('X', 23, struct xsm_feature_get_arg)

#define XSMEM_BLOCK_MAX_NUM 262144 /* 512 * 512*/
#define XSHM_KEY_SIZE 128U

#define BUFF_NOCACHE 0U
#define BUFF_CACHE 1U

#define XSMEM_TASK_IN_POOL 0
#define XSMEM_TASK_ADDING_TO_POOL 1

#define XSMEM_PROP_OP_SET 0
#define XSMEM_PROP_OP_GET 1
#define XSMEM_PROP_OP_DEL 2

#define XSMEM_PROP_OWNER_TASK 0
#define XSMEM_PROP_OWNER_TASK_GRP 1

#define XSMEM_MAX_CMD 24

#define XSMEM_ALGO_EMPTY (-1)
#define XSMEM_ALGO_VMA 0
#define XSMEM_ALGO_SP 1
#define XSMEM_ALGO_CACHE_VMA 2
#define XSMEM_ALGO_CACHE_SP 3
#define XSMEM_ALGO_MAX 4

#ifndef EMU_ST
#define TASK_BLK_MAX_GET_NUM (32 * 1024)
#else
#define TASK_BLK_MAX_GET_NUM (1024)
#endif
#define DAVINCI_XSMEM_SUB_MODULE_NAME "XSMEM"

#endif