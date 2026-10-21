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

#include "esched_adapt.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "pbl/pbl_runenv_config.h"
#include "esched_ioctl.h"
#include "esched_fops.h"
#include "esched_adapt_platform.h"
#include "ka_compiler_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
#include "ts_drv_init.h"
#endif

int32_t sched_fop_query_sync_msg_trace(u32 devid, unsigned long arg)
{
#ifndef EMU_ST
    int32_t ret;
    struct sched_ioctl_para_trace para_info = {{0}};
    struct sched_trace_input *para = &para_info.input;

    if (copy_from_user_safe(para, (void *)(uintptr_t)arg, sizeof(struct sched_trace_input)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }

    ret = sched_query_sync_event_trace(sched_ioctl_devid(devid, para->dev_id), para, &para_info.trace);
    if (ka_unlikely(ret != 0)) {
        return ret;
    }

    if (copy_to_user_safe((void *)((uintptr_t)arg + sizeof(struct sched_trace_input)), &para_info.trace,
                          sizeof(struct sched_sync_event_trace)) != 0) {
        return DRV_ERROR_COPY_USER_FAIL;
    }
#endif
    return 0;
}

STATIC int32_t esched_get_trace_index(struct sched_event *event, u32 *gid, u32 *event_id, u32 *subevent_id)
{
    struct event_sync_msg *sync_msg = NULL;

    if (event->event_id == EVENT_DRV_MSG) {
        if (event->msg_len < sizeof(struct event_sync_msg)) {
            return DRV_ERROR_PARA_ERROR;
        }
        sync_msg = (struct event_sync_msg *)event->msg;
        *gid = sync_msg->gid;
        *event_id = sync_msg->event_id;
        *subevent_id = sync_msg->subevent_id;
    } else {
        *gid = event->gid;
        *event_id = event->event_id;
        *subevent_id = event->subevent_id;
    }
    return 0;
}

void esched_wait_trace_update(struct sched_proc_ctx *proc_ctx, struct sched_event *event)
{
    int32_t ret;
    int dfx_gid, dfx_tid;
    u32 gid, event_id, subevent_id;

    if ((proc_ctx == NULL) || (event == NULL)) {
        sched_warn("wait event abnormal.\n");
        return;
    }
    ret = esched_get_trace_index(event, &gid, &event_id, &subevent_id);
    if (ret != 0) {
        return;
    }

    dfx_gid = gid - SCHED_MAX_DEFAULT_GRP_NUM;
    dfx_tid = event_id - SCHED_SYNC_START_EVENT_ID;
    if ((dfx_gid < 0) || (dfx_gid >= SCHED_MAX_EX_GRP_NUM) || (dfx_tid < 0) ||
        (dfx_tid >= SCHED_MAX_SYNC_THREAD_NUM_PER_GRP)) {
        return;
    }

    esched_adapt_wait_trace_update(proc_ctx, dfx_gid, dfx_tid, event);
    return;
}

void esched_publish_trace_update(struct sched_proc_ctx *proc_ctx, struct sched_event *event)
{
    int32_t ret;
    int dfx_gid, dfx_tid;
    u32 gid, event_id, subevent_id;
    u64 curr_time = sched_get_cur_timestamp();
    if ((proc_ctx == NULL) || (event == NULL)) {
        sched_warn("publish event abnormal.\n");
        return;
    }
    ret = esched_get_trace_index(event, &gid, &event_id, &subevent_id);
    if (ret != 0) {
        return;
    }

    dfx_gid = gid - SCHED_MAX_DEFAULT_GRP_NUM;
    dfx_tid = event_id - SCHED_SYNC_START_EVENT_ID;
    if ((dfx_gid < 0) || (dfx_gid >= SCHED_MAX_EX_GRP_NUM) || (dfx_tid < 0) ||
        (dfx_tid >= SCHED_MAX_SYNC_THREAD_NUM_PER_GRP)) {
        return;
    }
    esched_adapt_publish_trace_update(proc_ctx, dfx_gid, dfx_tid, curr_time, gid, event_id, subevent_id);
    return;
}

void esched_submit_trace_update(u32 chip_id, u32 event_src, struct sched_published_event_info *event_info)
{
    int dfx_gid, dfx_tid;
    u32 gid, event_id, subevent_id;
    int pid = ka_task_get_current()->tgid;
    u64 curr_time = sched_get_cur_timestamp();
    struct sched_proc_ctx *proc_ctx = NULL;
    struct event_sync_msg *sync_msg = NULL;
    struct sched_numa_node *node = NULL;

    if (event_info->event_id == EVENT_DRV_MSG) {
        if (event_info->msg_len < sizeof(struct event_sync_msg)) {
            return;
        }
        sync_msg = (struct event_sync_msg *)event_info->msg;
        gid = sync_msg->gid;
        event_id = sync_msg->event_id;
        subevent_id = sync_msg->subevent_id;
    } else {
        gid = event_info->gid;
        event_id = event_info->event_id;
        subevent_id = event_info->subevent_id;
    }

    dfx_gid = gid - SCHED_MAX_DEFAULT_GRP_NUM;
    dfx_tid = event_id - SCHED_SYNC_START_EVENT_ID;
    if ((dfx_gid < 0) || (dfx_gid >= SCHED_MAX_EX_GRP_NUM) || (dfx_tid < 0) ||
        (dfx_tid >= SCHED_MAX_SYNC_THREAD_NUM_PER_GRP)) {
        return;
    }

    node = esched_dev_get(chip_id);
    if (node == NULL) {
        return;
    }

    proc_ctx = esched_proc_get(node, pid);
    if (proc_ctx == NULL) {
        esched_dev_put(node);
        return;
    }

    esched_adapt_submit_trace_update(proc_ctx, dfx_gid, dfx_tid, event_info, curr_time, gid, event_id, subevent_id);
    esched_proc_put(proc_ctx);
    esched_dev_put(node);
    return;
}

STATIC void sched_free_thread_map(ka_kref_t *kref)
{
    struct sched_event_thread_map *thread_map = ka_container_of(kref, struct sched_event_thread_map, ref);

    sched_kfree(thread_map);
}

void sched_get_thread_map(struct sched_event_thread_map *thread_map)
{
    ka_base_kref_get(&thread_map->ref);
}

void sched_put_thread_map(struct sched_event_thread_map *thread_map)
{
    ka_base_kref_put(&thread_map->ref, sched_free_thread_map);
}

int sched_event_add_thread(struct sched_event *event, u32 tid)
{
    event->event_thread_map = sched_kzalloc(sizeof(struct sched_event_thread_map) + sizeof(u32),
                                            KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (event->event_thread_map == NULL) {
        sched_err("Failed to kzalloc memory for event_thread_map and thread. (size=0x%lx)\n",
                  sizeof(struct sched_event_thread_map) + sizeof(u32));
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ka_base_kref_init(&event->event_thread_map->ref);

    event->event_thread_map->thread = (u32 *)(event->event_thread_map + 1);

    event->event_thread_map->thread[0] = tid;
    event->event_thread_map->thread_num = 1;

    return 0;
}

int32_t sched_get_firt_ctrlcpu(void)
{
    return 0;
}

int32_t sched_node_sched_cpu_uda_init(u32 dev_id)
{
    return 0;
}

int32_t sched_node_sched_cpu_module_init(u32 dev_id)
{
    return 0;
}

bool esched_is_support_uda_online()
{
    return true;
}

int esched_pm_shutdown(u32 chip_id)
{
    return 0;
}
KA_EXPORT_SYMBOL_GPL(esched_pm_shutdown);

int esched_ts_platform_init(void)
{
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    return ts_drv_platform_init();
#else
    return 0;
#endif
}

void esched_ts_platform_uninit(void)
{
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    return ts_drv_platform_exit();
#else
    return;
#endif
}

int sched_get_sentry_mode(struct sched_numa_node *node, int *mode)
{
    *mode = 0;
    return 0;
}

int sched_get_cpu_num(u32 *cpu_num)
{
    *cpu_num = ka_system_num_online_cpus();
    return 0;
}

int sched_sentry_init(void)
{
    return 0;
}

void sched_sentry_uninit(void)
{
    return;
}