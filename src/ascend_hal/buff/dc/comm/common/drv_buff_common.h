/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DRV_BUFF_COMMON_H
#define DRV_BUFF_COMMON_H

#if defined(__arm__) || defined(__aarch64__)
#else
#include <sys/time.h>
#endif

#define MODULE_BUFF "drv_buff"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "securec.h"
#include "bitmap.h"
#include "ascend_hal.h"
#include "atomic_lock.h"
#include "drv_buff_log.h"

#ifndef EMU_ST
#else
#include "ascend_inpackage_hal.h"
#endif

#define PRINT printf

#define BUFF_EXPORT_SYMBOL(sym)

void set_sys_running_flag(unsigned int value);
sig_atomic_t get_sys_running_flag(void);

#ifndef __uint16_defined
typedef unsigned short uint16;
#define __uint16_defined
#endif

#ifndef __uint32_defined
typedef unsigned int uint32;
#define __uint32_defined
#endif

#ifndef __uint64_defined
typedef unsigned long long uint64;
#define __uint64_defined
#endif

#ifdef EMU_ST
#define STATIC
#else
#define STATIC static
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef EMU_ST
#include "drv_buff_adp.h"
#endif

#define USER_DATA_LEN 256
#define BUFF_POOL_SEPARATE_BLK_SIZE (1UL * 1024UL * 1024UL)
#define BUFF_2M (2UL * 1024UL * 1024UL)
#define SP_ALLOC_ALIGN 4096

#define BUFF_BINARY_VAL 2

#define BUFF_CMD_MAX 30
#define BUFF_FILE_LEN 8

#define BUFF_MAX_RECYLE_NUM_IN_LIST 512

#define BUFF_ZONE_NUM 1
#define BUFF_TIME_OVERTURN_OFFSET 0x100000000UL
#define BUFF_TIME_STAT_ENABLE 1
#define BUFF_TIME_STAT_DISABLE 0
#define BUFF_32_BIT_MASK 0xFFFFFFFF
#define BUFF_DVPP_16G_MEM 0

#define BUFF_SPG_ID_NONE (-1)
#define BUFF_SPG_ID_DEFAULT 0
#define BUFF_SPG_ID_MIN 1

#define PTHREAD_STOP_FLAG 0
#define PTHREAD_RUNNING_FLAG 1
#define PTHREAD_EXIT_FLAG 2

#define MZ_CFG_CNT_MAX 8

#define BUFF_MAX_DEV 64
#define BUFF_MIN_DEV 0
#define BUFF_INVALID_DEV 0xFF

#define BUFF_MAX_RECORD_NUM (8 * 1024)
#define BUFF_MIN_RECORD_NUM (256)
#define BUFF_TRY_LOCK_FAIL (0)

#define BUFF_VALID 1
#define BUFF_INVALID 0

#define BUFF_GRP_ID_DIRECT (-1)
#define BUFF_GRP_ALLOC_DEFAULT 0
#define BUFF_GRP_ID_MIN 1

#define ULONG_MAX 0xffffffffffffffff
#define ULLONG_MAX 0xffffffffffffffff

enum buff_inner_list_type {
    MEMZONE_LIST = 0,
    HUGE_BUF_LIST,
    MEMPOOL_LIST,
    MEMPOOL_MBUF_LIST,
    BUFF_LIST_TYPE_MAX,
};

struct mem_attr_t {
    uint32 align;          /* must be power of 2 */
    uint32 phys_cont_flag; /* contiguous physical addr flag. 0--false, 1--true */
    uint32 recycle_flag;   /* flag of whether to release memory when the process exits */
};
#define BUFF_MAX_ALLOC_TIME 10000

struct mem_info_t {
    uint64_t size;
    uint64_t flag;
    uint32 align;
    uint32 offset;
    int grp_id;
    uint32 alloc_type;
};

struct buff_req_mp_destroy {
    uint64 mp;
};

struct buff_req_mp_create {
    uint64 mp_uva; /* output mp handle */
    uint64 total_size;
    void *start;
    int devid;
    int groupid;
    uint32 obj_size;
    uint32 obj_num;
    uint32 align; /* must be power of 2 */
    uint32 type;
    uint64 sp_flag;
    char pool_name[BUFF_POOL_NAME_LEN];
};

struct buff_req_mz_create {
    uint64_t mz_uva; /* output */
    uint64_t total_size;
    void *start;
    int devid;
    uint32 cfg_id;
    uint32 blk_size;
    uint64 sp_flag;
    int grp_id;
    uint32 page_type;
    uint32 resv_size;
    struct buff_memzone_list_node *mz_list_node;
};

struct buff_req_mz_alloc_huge_buf {
    uint64_t buf_uva; /* output uva */
    uint64_t size;
    uint32_t blk_id;

    int devid;
    int grp_id;
    uint64 sp_flag;
    uint32 alloc_type;
};

struct buff_req_mz_free_huge_buf {
    uint64 buf_uva;
};

struct buff_req_mz_delete {
    uint64 mz_user_mng_uva;
};

struct buff_req_pid_add_group {
    int pid;
    int grp_id;
    GROUP_ID_TYPE type;
};

struct buff_req_get_process_uni_id {
    uint64 process_uni_id;
};

struct buff_req_mbuf_check_arg {
    struct MbufTimeoutCheckPara check_para;
};

struct buff_req_timeout_mbuf_info {
    uint32 buff_size;
    void *buff;
};

union buff_req_arg {
    struct buff_req_mz_create mz_create_arg;
    struct buff_req_mz_alloc_huge_buf mz_alloc_huge_buf_arg;
    struct buff_req_mz_free_huge_buf mz_free_huge_buf_arg;
    struct buff_req_mz_delete mz_delete_arg;
    struct buff_req_mp_create mp_create_arg;
    struct buff_req_mp_destroy mp_destroy_arg;
    struct buff_req_pid_add_group pid_add_group;
    struct buff_req_get_process_uni_id uni_id_arg;
    struct buff_req_mbuf_check_arg mbuf_check_arg;
    struct buff_req_timeout_mbuf_info mbuf_timeout_info;
};

#define MAX_MBUF_RECORD_NUM 8
#define MBUF_MUTEX_LEN 32
struct share_mbuf {
    uint64_t total_len;
    uint64_t data_len;
    void *datablock;
    void *data;
    uint64_t buff_type;
    struct atomic_lock lock;
    struct share_mbuf *prev;
    struct share_mbuf *next;
    struct share_mbuf *s_mbuf;
    char user_data[USER_DATA_LEN];
    unsigned long long timestamp;
    int record_pid[MAX_MBUF_RECORD_NUM];
    unsigned char record_opt[MAX_MBUF_RECORD_NUM];
    unsigned int record_cur_idx;
    unsigned short record_total_num;
    short buff_trace_id;
    uint32_t blk_id;
    uint32_t data_blk_id;
    uint32_t next_blk_id;
    uint32_t prev_blk_id;
};

struct Mbuf {
    uint64_t total_len;
    uint64_t data_len;
    void *datablock;
    void *data;
    uint64_t buff_type;
    struct atomic_lock lock;
    struct Mbuf *prev;
    struct Mbuf *next;
    struct share_mbuf *s_mbuf;
    char user_data[USER_DATA_LEN];
    unsigned long long timestamp;
    int record_pid[MAX_MBUF_RECORD_NUM];
    unsigned char record_opt[MAX_MBUF_RECORD_NUM];
    unsigned int record_cur_idx;
    unsigned short record_total_num;
    short buff_trace_id;
    uint32_t blk_id;
    uint32_t data_blk_id;
    uint32_t next_blk_id;
    uint32_t prev_blk_id;
};

#define MBUF_MUTEX_ASSERT(condition, diag) extern char ___StaticAssertVar___[(condition) ? 2 : -1]

MBUF_MUTEX_ASSERT((sizeof(char[MBUF_MUTEX_LEN]) > sizeof(struct atomic_lock)), "need to extended");

#define __user_addr
#define __kernel_addr

#define BUFF_BUG_ON(cond)                                                         \
    do {                                                                          \
        if (cond)                                                                 \
            PRINT("###BUG ON:%s, file:%s, line:%u\n", #cond, __FILE__, __LINE__); \
    } while (0)

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(val, align) ((val) & (~((typeof(val))((align) - 1))))
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(val, align) (((val) + ((typeof(val))(align) - 1)) & (~((typeof(val))((align) - 1))))
#endif

#define BITS_PER_BYTE 8
#define BITS_PER_LONG (BITS_PER_BYTE * (int)sizeof(long))
#define BITS_TO_LONGS(nr) (ALIGN_UP(nr, BITS_PER_LONG) / BITS_PER_LONG)

static inline size_t bitmap_size(unsigned int bits)
{
    /*lint -e502 -e502*/
    return (size_t)(sizeof(bitmap_t) * (size_t)BITS_TO_LONGS(bits));
    /*lint +e502 +e502*/
}
#define BIT_WORD(nr) ((nr) / BITS_PER_LONG)

#ifdef DRV_BUFF_MODULE_UT
#define BUFF_GET_CUR_SYSTEM_COUNTER(cnt) (cnt = 1000000)
#define BUFF_GET_SYSTEM_FREQ(cnt) (cnt = 1000000)
#else
#define BUFF_GET_CUR_SYSTEM_COUNTER(cnt) asm volatile("mrs %0, CNTVCT_EL0" : "=r"(cnt) :)
#define BUFF_GET_SYSTEM_FREQ(cnt) asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(cnt) :)
#endif
#define BUFF_FREQ_2_MILLISEC 1000
#define BUFF_FREQ_2_MICROSEC (1000 * 1000)
#define USEC_PER_SEC 1000000
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC 1000
#endif

static inline unsigned long buff_get_cur_cpu_tick(void)
{
#if defined(__arm__) || defined(__aarch64__)
    unsigned long cnt = 0;
    BUFF_GET_CUR_SYSTEM_COUNTER(cnt);
    return cnt;
#else
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)((ts.tv_sec * USEC_PER_SEC) + (ts.tv_nsec / NSEC_PER_USEC));
#endif
}

static inline unsigned long buff_get_sys_freq(void)
{
#if defined(__arm__) || defined(__aarch64__)
    unsigned long cnt = 0;
    BUFF_GET_SYSTEM_FREQ(cnt);
    return cnt;
#else
    return USEC_PER_SEC; /* buff_get_cur_cpu_tick return microsecond */
#endif
}

static inline unsigned long tick_to_millisec(unsigned long tick)
{
    unsigned long freq = buff_get_sys_freq();
    if (freq < BUFF_FREQ_2_MILLISEC) {
        return 0;
    }

    return tick / (freq / BUFF_FREQ_2_MILLISEC);
}

static inline unsigned long tick_to_microsec(unsigned long tick)
{
    unsigned long freq = buff_get_sys_freq();
    if (freq < BUFF_FREQ_2_MICROSEC) {
        return 0;
    }

    return tick / (freq / BUFF_FREQ_2_MICROSEC);
}

static inline unsigned long microsec_to_tick(unsigned long microsec)
{
    unsigned long freq = buff_get_sys_freq();
    if (freq < BUFF_FREQ_2_MICROSEC) {
        return 0;
    }

    return microsec * (freq / BUFF_FREQ_2_MICROSEC);
}

static inline unsigned long buff_get_cur_timestamp(void)
{
    return buff_get_cur_cpu_tick();
}

static inline unsigned long buff_api_timestamp(void)
{
    return tick_to_millisec(buff_get_cur_timestamp());
}

static inline unsigned long long buff_kb_to_b(unsigned long long size_kb)
{
    return size_kb * 1024; /* 1KB = 1024B */
}

#endif /* _DRV_BUFF_COMMON_H_ */
