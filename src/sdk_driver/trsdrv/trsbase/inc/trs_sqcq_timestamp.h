/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TRS_SQCQ_TIMESTAMP_H
#define TRS_SQCQ_TIMESTAMP_H

#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX

#include "ka_base_pub.h"
#include "ka_system_pub.h"
#include "trs_pub_def.h"

#define TRS_USEC_SEC 1000

#if defined(TSDRV_UT)
static inline u64 trs_get_cpu_tick(void)
{
    return 0;
}

static inline u64 trs_get_sys_freq(void)
{
    return 0;
}
#else
static inline u64 trs_get_cpu_tick(void)
{
#if defined(__arm__) || defined(__aarch64__)
    u64 cnt = 0;
    asm volatile("mrs %0, CNTVCT_EL0" : "=r"(cnt) :);
    return cnt;
#else
    struct timespec64 timestamp;
    ktime_get_ts64(&timestamp);
    return ((u64)timestamp.tv_sec * USEC_PER_SEC) + ((u64)timestamp.tv_nsec / NSEC_PER_USEC);
#endif
}

static inline u64 trs_get_sys_freq(void)
{
#if defined(__arm__) || defined(__aarch64__)
    u64 cnt = 0;
    asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(cnt) :);
    return cnt;
#else
    return USEC_PER_SEC;
#endif
}
#endif

static inline u64 trs_tick_to_millisecond(u64 tick)
{
    u64 freq = trs_get_sys_freq();
    if ((freq / TRS_USEC_SEC) != 0) {
        return tick / (freq / TRS_USEC_SEC);
    }
    return 0;
}

#define TRS_SQCQ_DFX_MAX_CQ_NUM 160U
#define TRS_SQCQ_DFX_MAX_LOGIC_CQ_NUM 2048U
#define TRS_SQCQ_DFX_MAX_CQ_DEPTH 1024U

struct trs_logic_cqe_trace_info {
    u16 task_id;
    u16 stream_id;
    u16 sqid;
    u32 tail;
    u64 timestamp;
};

struct trs_logic_cq_dispatch_dfx {
    u64 logic_cq_dispatch_tick;
    u64 logic_cq_wakeup_tick;
    struct trs_logic_cqe_trace_info logic_cqes_trace[TRS_SQCQ_DFX_MAX_CQ_DEPTH];
};

struct trs_sqcq_dfx {
    u64 cq_irq_tick;
    u64 cq_tasklet_tick;
    u64 phy_cq_tick[TRS_SQCQ_DFX_MAX_CQ_NUM];
    struct trs_logic_cq_dispatch_dfx logic_cq_dfx[TRS_SQCQ_DFX_MAX_LOGIC_CQ_NUM];
};

void trs_dfx_set_cq_irq_tick(u32 tsid);
void trs_dfx_set_cq_tasklet_tick(u32 tsid);
void trs_dfx_set_phy_cq_tick(u32 tsid, u32 cqid);
void trs_dfx_set_hw_cq_recv_tick(u32 tsid, u32 logic_cqid);
void trs_dfx_set_logic_cq_wakeup_tick(u32 tsid, u32 logic_cqid);
void trs_dfx_set_logic_cq_enque_trace(u32 tsid, u32 logic_cqid, u16 task_id, u16 stream_id, u16 sqid, u32 tail);
struct trs_logic_cqe_trace_info *trs_dfx_get_logic_cq_enque_trace(u32 tsid, u32 logic_cqid, u32 idx);

bool trs_sqcq_dfx_ratelimited(void);
void trs_dfx_print_timestamp_nodes(u32 tsid, u32 logic_cqid, u64 recv_start_tick);

int trs_sqcq_dfx_init(void);
void trs_sqcq_dfx_uninit(void);

#endif /* CFG_FEATURE_SUPPORT_CQ_RECV_DFX */

#endif /* TRS_SQCQ_TIMESTAMP_H */
