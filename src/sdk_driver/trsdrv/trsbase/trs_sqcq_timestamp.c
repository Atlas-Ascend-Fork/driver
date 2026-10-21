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

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>

#include "trs_sqcq_timestamp.h"
#include "ka_kernel_def_pub.h"

#ifdef CFG_FEATURE_SUPPORT_CQ_RECV_DFX

#ifdef TSDRV_UT
bool trs_sqcq_dfx_ratelimited(void)
{
    return false;
}
#else
#define TRS_SQCQ_RATELIMIT_INTERVAL (5 * HZ)
#define TRS_SQCQ_RATELIMIT_BURST 10
bool trs_sqcq_dfx_ratelimited(void)
{
    static DEFINE_RATELIMIT_STATE(trs_sqcq_dfx_rate_limit, TRS_SQCQ_RATELIMIT_INTERVAL, TRS_SQCQ_RATELIMIT_BURST);

    if (__ratelimit(&trs_sqcq_dfx_rate_limit)) {
        return false;
    }
    return true;
}
#endif
KA_EXPORT_SYMBOL_GPL(trs_sqcq_dfx_ratelimited);

static struct trs_sqcq_dfx *g_tsdrv_sqcq_dfx[TRS_TS_MAX_NUM];

static struct trs_sqcq_dfx *trs_sqcq_dfx_get(u32 tsid)
{
    if (tsid >= TRS_TS_MAX_NUM) {
        return NULL;
    }
    return g_tsdrv_sqcq_dfx[tsid];
}

int trs_sqcq_dfx_init(void)
{
    u32 i;

    for (i = 0; i < TRS_TS_MAX_NUM; i++) {
        g_tsdrv_sqcq_dfx[i] = trs_vzalloc(sizeof(struct trs_sqcq_dfx));
        if (g_tsdrv_sqcq_dfx[i] == NULL) {
            trs_sqcq_dfx_uninit();
            trs_err("Alloc sqcq dfx failed. (tsid=%u)\n", i);
            return -ENOMEM;
        }
    }
    return 0;
}
KA_EXPORT_SYMBOL(trs_sqcq_dfx_init);

void trs_sqcq_dfx_uninit(void)
{
    u32 i;
    for (i = 0; i < TRS_TS_MAX_NUM; i++) {
        if (g_tsdrv_sqcq_dfx[i] != NULL) {
            trs_vfree(g_tsdrv_sqcq_dfx[i]);
            g_tsdrv_sqcq_dfx[i] = NULL;
        }
    }
}
KA_EXPORT_SYMBOL(trs_sqcq_dfx_uninit);

void trs_dfx_set_cq_irq_tick(u32 tsid)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if (dfx != NULL) {
        dfx->cq_irq_tick = trs_get_cpu_tick();
    }
}
KA_EXPORT_SYMBOL(trs_dfx_set_cq_irq_tick);

void trs_dfx_set_cq_tasklet_tick(u32 tsid)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if (dfx != NULL) {
        dfx->cq_tasklet_tick = trs_get_cpu_tick();
    }
}
KA_EXPORT_SYMBOL(trs_dfx_set_cq_tasklet_tick);

static u64 trs_dfx_get_cq_tasklet_tick(u32 tsid)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if (dfx != NULL) {
        return trs_tick_to_millisecond(dfx->cq_tasklet_tick);
    }
    return 0;
}

static u64 trs_dfx_get_cq_irq_tick(u32 tsid)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if (dfx != NULL) {
        return trs_tick_to_millisecond(dfx->cq_irq_tick);
    }
    return 0;
}

static u64 trs_dfx_get_hw_cq_recv_tick(u32 tsid, u32 logic_cqid)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if ((dfx != NULL) && (logic_cqid < TRS_SQCQ_DFX_MAX_LOGIC_CQ_NUM)) {
        return trs_tick_to_millisecond(dfx->logic_cq_dfx[logic_cqid].logic_cq_dispatch_tick);
    }
    return 0;
}

static u64 trs_dfx_get_logic_cq_wakeup_tick(u32 tsid, u32 logic_cqid)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if ((dfx != NULL) && (logic_cqid < TRS_SQCQ_DFX_MAX_LOGIC_CQ_NUM)) {
        return trs_tick_to_millisecond(dfx->logic_cq_dfx[logic_cqid].logic_cq_wakeup_tick);
    }
    return 0;
}

struct trs_logic_cqe_trace_info *trs_dfx_get_logic_cq_enque_trace(u32 tsid, u32 logic_cqid, u32 idx)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if ((dfx != NULL) && (logic_cqid < TRS_SQCQ_DFX_MAX_LOGIC_CQ_NUM) && (idx < TRS_SQCQ_DFX_MAX_CQ_DEPTH)) {
        return &dfx->logic_cq_dfx[logic_cqid].logic_cqes_trace[idx];
    }
    return NULL;
}
KA_EXPORT_SYMBOL(trs_dfx_get_logic_cq_enque_trace);

void trs_dfx_set_phy_cq_tick(u32 tsid, u32 cqid)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if ((dfx != NULL) && (cqid < TRS_SQCQ_DFX_MAX_CQ_NUM)) {
        dfx->phy_cq_tick[cqid] = trs_get_cpu_tick();
    }
}
KA_EXPORT_SYMBOL(trs_dfx_set_phy_cq_tick);

void trs_dfx_set_hw_cq_recv_tick(u32 tsid, u32 logic_cqid)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if ((dfx != NULL) && (logic_cqid < TRS_SQCQ_DFX_MAX_LOGIC_CQ_NUM)) {
        dfx->logic_cq_dfx[logic_cqid].logic_cq_dispatch_tick = trs_get_cpu_tick();
    }
}
KA_EXPORT_SYMBOL(trs_dfx_set_hw_cq_recv_tick);

void trs_dfx_set_logic_cq_wakeup_tick(u32 tsid, u32 logic_cqid)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if ((dfx != NULL) && (logic_cqid < TRS_SQCQ_DFX_MAX_LOGIC_CQ_NUM)) {
        dfx->logic_cq_dfx[logic_cqid].logic_cq_wakeup_tick = trs_get_cpu_tick();
    }
}
KA_EXPORT_SYMBOL(trs_dfx_set_logic_cq_wakeup_tick);

void trs_dfx_set_logic_cq_enque_trace(u32 tsid, u32 logic_cqid, u16 task_id, u16 stream_id, u16 sqid, u32 tail)
{
    struct trs_sqcq_dfx *dfx = trs_sqcq_dfx_get(tsid);
    if ((dfx != NULL) && (logic_cqid < TRS_SQCQ_DFX_MAX_LOGIC_CQ_NUM)) {
        struct trs_logic_cq_dispatch_dfx *lcq_dfx = &dfx->logic_cq_dfx[logic_cqid];
        u32 idx = tail % TRS_SQCQ_DFX_MAX_CQ_DEPTH;
        lcq_dfx->logic_cqes_trace[idx].task_id = task_id;
        lcq_dfx->logic_cqes_trace[idx].stream_id = stream_id;
        lcq_dfx->logic_cqes_trace[idx].sqid = sqid;
        lcq_dfx->logic_cqes_trace[idx].tail = tail;
        lcq_dfx->logic_cqes_trace[idx].timestamp = trs_get_cpu_tick();
    }
}
KA_EXPORT_SYMBOL(trs_dfx_set_logic_cq_enque_trace);

void trs_dfx_print_timestamp_nodes(u32 tsid, u32 logic_cqid, u64 recv_start_tick)
{
    u64 irq_ms = trs_dfx_get_cq_irq_tick(tsid);
    u64 tasklet_ms = trs_dfx_get_cq_tasklet_tick(tsid);
    u64 dispatch_ms = trs_dfx_get_hw_cq_recv_tick(tsid, logic_cqid);
    u64 wakeup_ms = trs_dfx_get_logic_cq_wakeup_tick(tsid, logic_cqid);
    u64 recv_start_ms = trs_tick_to_millisecond(recv_start_tick);
    u64 recv_current_ms = trs_tick_to_millisecond(trs_get_cpu_tick());

    trs_info("DFX timestamp nodes: tsid=%u logic_cqid=%u "
             "irq=%llums tasklet=%llums d_irq2tasklet=%llums "
             "dispatch=%llums wakeup=%llums d_dispatch2wakeup=%llums "
             "recv_start=%llums recv_current=%llums d_recv_wait=%llums\n",
             tsid, logic_cqid, irq_ms, tasklet_ms, (irq_ms != 0 && tasklet_ms >= irq_ms) ? (tasklet_ms - irq_ms) : 0,
             dispatch_ms, wakeup_ms, (dispatch_ms != 0 && wakeup_ms >= dispatch_ms) ? (wakeup_ms - dispatch_ms) : 0,
             recv_start_ms, recv_current_ms,
             (recv_start_ms != 0 && recv_current_ms >= recv_start_ms) ? (recv_current_ms - recv_start_ms) : 0);
}
KA_EXPORT_SYMBOL(trs_dfx_print_timestamp_nodes);

#endif /* CFG_FEATURE_SUPPORT_CQ_RECV_DFX */
