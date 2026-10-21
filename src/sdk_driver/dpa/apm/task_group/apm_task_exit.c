/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_list_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_dfx_pub.h"

#include "apm_auto_init.h"

#include "apm_fops.h"
#include "apm_task_exit.h"
#include "apm_slab.h"

#ifdef CFG_SOC_PLATFORM_ESL /* about 40s */

#ifdef DRV_HOST
#define APM_TASK_EXIT_SCHED_NUM_MAX 5000
#else
#define APM_TASK_EXIT_SCHED_NUM_MAX 250
#endif

#else
#define APM_TASK_EXIT_SCHED_NUM_MAX 1500
#endif

#define APM_TASK_EXIT_CHECK_PERIOD 100 /* 100 ms */
#define APM_TASK_EXIT_RETRY_DELAY_SHORT_MS 1
#define APM_TASK_EXIT_RETRY_DELAY_LONG_MS 200
#define APM_TASK_EXIT_RETRY_SHORT_NUM 150
#define APM_TASK_EXIT_LOG_INTERVAL 100 /* log every 100 retries */

static ka_list_head_t exit_ctrl_head;
static ka_mutex_t exit_ctrl_mutex;
ka_delayed_work_t task_exit_check_work;

struct apm_task_exit_ctrl {
    ka_list_head_t node;
    ka_delayed_work_t dwork;
    enum apm_exit_stage stage;
    int tgid;
    int sched_num;
    int sync_flag;
    ka_blocking_notifier_head_t *nh;
    bool (*is_tasks_in_group_exit_synchronized)(int tgid, enum apm_exit_stage stage);
};

static int apm_task_exit_notify_handle(ka_blocking_notifier_head_t *nh, int tgid, int stage, bool is_force_exit)
{
    apm_debug("Call task exit notify. (tgid=%d; stage=%d; is_force_exit=%d)\n", tgid, stage, is_force_exit);
    return ka_dfx_notifier_to_errno(
        ka_dfx_blocking_notifier_call_chain(nh, apm_get_exit_val(stage, tgid, is_force_exit), NULL));
}

static void apm_task_exit_notify_all_stage(ka_blocking_notifier_head_t *nh, int tgid, int start_stage)
{
    int stage;

    for (stage = start_stage; stage < APM_STAGE_MAX; stage++) {
        (void)apm_task_exit_notify_handle(nh, tgid, stage, true);
    }
}

static void apm_task_exit_finish(struct apm_task_exit_ctrl *exit_ctrl)
{
    apm_info("Exit finish. (tgid=%d; sched_num=%d)\n", exit_ctrl->tgid, exit_ctrl->sched_num);
    ka_task_mutex_lock(&exit_ctrl_mutex);
    ka_list_del(&exit_ctrl->node);
    ka_task_mutex_unlock(&exit_ctrl_mutex);
    apm_vfree(exit_ctrl);
}

static int apm_task_exit_work_get_delay_time(struct apm_task_exit_ctrl *exit_ctrl)
{
    if ((exit_ctrl->sched_num <= APM_TASK_EXIT_RETRY_SHORT_NUM) || (exit_ctrl->stage > APM_STAGE_RECYCLE_RES)) {
        return APM_TASK_EXIT_RETRY_DELAY_SHORT_MS;
    } else {
        return APM_TASK_EXIT_RETRY_DELAY_LONG_MS;
    }
}

static void apm_task_exit_work(ka_work_struct_t *p_work)
{
    struct apm_task_exit_ctrl *exit_ctrl = ka_container_of(p_work, struct apm_task_exit_ctrl, dwork.work);
    struct apm_task_exit_ctrl *exit_ctrl_tmp = NULL;
    int ret;

    ka_task_mutex_lock(&exit_ctrl_mutex);
    ka_list_for_each_entry(exit_ctrl_tmp, &exit_ctrl_head, node)
    {
        if ((exit_ctrl_tmp->tgid == exit_ctrl->tgid) && (exit_ctrl_tmp->nh == exit_ctrl->nh)) {
            if (exit_ctrl_tmp == exit_ctrl) {
#ifndef EMU_ST
                break;
#endif
            } else { /* wait for last same-pid task finish */
                ka_task_mutex_unlock(&exit_ctrl_mutex);
                goto resched;
            }
        }
    }
    ka_task_mutex_unlock(&exit_ctrl_mutex);

    exit_ctrl->sched_num++;

    /* Periodically log exit progress to help diagnose which stage is stuck */
    if ((exit_ctrl->sched_num % APM_TASK_EXIT_LOG_INTERVAL) == 0) {
        apm_info("Exit in progress. (tgid=%d; stage=%d; sync_flag=%d; sched_num=%d)\n", exit_ctrl->tgid,
                 exit_ctrl->stage, exit_ctrl->sync_flag, exit_ctrl->sched_num);
    }

    if (exit_ctrl->sync_flag == 1) {
        if (exit_ctrl->is_tasks_in_group_exit_synchronized(exit_ctrl->tgid, exit_ctrl->stage)) {
            exit_ctrl->sync_flag = 0;
            exit_ctrl->stage++;
        }
    }

    if (exit_ctrl->sync_flag == 0) {
        ret = apm_task_exit_notify_handle(exit_ctrl->nh, exit_ctrl->tgid, exit_ctrl->stage, false);
        if (ret == 0) {
            /* recycle res and post recycle res not need sync */
            if (exit_ctrl->stage >= APM_STAGE_POST_RECYCLE_RES) {
                exit_ctrl->stage++;
            } else {
                exit_ctrl->sync_flag = 1;
            }
        } else {
            apm_warn("Exit notify not success. (tgid=%d; stage=%d; sched_num=%d; ret=%d)\n", exit_ctrl->tgid,
                     exit_ctrl->stage, exit_ctrl->sched_num, ret);
        }
    }

    if ((exit_ctrl->stage >= APM_STAGE_MAX) || (exit_ctrl->sched_num >= APM_TASK_EXIT_SCHED_NUM_MAX)) {
        if (exit_ctrl->stage < APM_STAGE_MAX) {
            /* Ensure that the process exit process is complete. */
            int start_stage = (exit_ctrl->sync_flag == 1) ? exit_ctrl->stage + 1 : exit_ctrl->stage;
            apm_warn("Exit timeout. (tgid=%d; stage=%d; sync_flag=%d)\n", exit_ctrl->tgid, exit_ctrl->stage,
                     exit_ctrl->sync_flag);
            apm_task_exit_notify_all_stage(exit_ctrl->nh, exit_ctrl->tgid, start_stage);
        }
        apm_task_exit_finish(exit_ctrl);
        return;
    }

resched:
    (void)ka_task_schedule_delayed_work(&exit_ctrl->dwork,
                                        ka_system_msecs_to_jiffies(apm_task_exit_work_get_delay_time(exit_ctrl)));
}

void apm_task_exit(int tgid, ka_blocking_notifier_head_t *nh,
                   bool (*is_tasks_in_group_exit_synchronized)(int tgid, enum apm_exit_stage stage))
{
    struct apm_task_exit_ctrl *exit_ctrl = NULL;

    (void)apm_task_exit_notify_handle(nh, tgid, APM_STAGE_DO_EXIT, false);

    exit_ctrl = apm_vmalloc(sizeof(struct apm_task_exit_ctrl), KA_GFP_KERNEL | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
    if (exit_ctrl == NULL) {
        apm_warn("Malloc exit ctrl failed. (tgid=%d)\n", tgid);
        /* Ensure that the process exit process is complete. */
        apm_task_exit_notify_all_stage(nh, tgid, APM_STAGE_STOP_STREAM);
        return;
    }

    exit_ctrl->sched_num = 0;
    exit_ctrl->sync_flag = 1;
    exit_ctrl->tgid = tgid;
    exit_ctrl->stage = APM_STAGE_DO_EXIT;
    exit_ctrl->nh = nh;
    exit_ctrl->is_tasks_in_group_exit_synchronized = is_tasks_in_group_exit_synchronized;
    KA_TASK_INIT_DELAYED_WORK(&exit_ctrl->dwork, apm_task_exit_work);
    ka_task_mutex_lock(&exit_ctrl_mutex);
    ka_list_add_tail(&exit_ctrl->node, &exit_ctrl_head);
    ka_task_mutex_unlock(&exit_ctrl_mutex);
    (void)ka_task_schedule_delayed_work(&exit_ctrl->dwork, 0);
}

bool apm_is_all_task_exit_finish(void)
{
    return ka_list_empty(&exit_ctrl_head);
}

void apm_task_exit_check_work(ka_work_struct_t *p_work)
{
    (void)module_feature_auto_call_by_scope(EXIT_CHECK_SCOPE_STR, NULL);
    ka_task_schedule_delayed_work(&task_exit_check_work, ka_system_msecs_to_jiffies(APM_TASK_EXIT_CHECK_PERIOD));
}

int apm_task_exit_init(void)
{
    KA_INIT_LIST_HEAD(&exit_ctrl_head);
    ka_task_mutex_init(&exit_ctrl_mutex);

    /* task may not open apm file, use work to check if task is exited */
    KA_TASK_INIT_DELAYED_WORK(&task_exit_check_work, apm_task_exit_check_work);
    ka_task_schedule_delayed_work(&task_exit_check_work, ka_system_msecs_to_jiffies(APM_TASK_EXIT_CHECK_PERIOD));

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(apm_task_exit_init, FEATURE_LOADER_STAGE_9);

void apm_task_exit_uninit(void)
{
    struct apm_task_exit_ctrl *exit_ctrl = NULL, *tmp = NULL;

    (void)ka_task_cancel_delayed_work_sync(&task_exit_check_work);

    ka_list_for_each_entry_safe(exit_ctrl, tmp, &exit_ctrl_head, node)
    {
        ka_task_cancel_delayed_work_sync(&exit_ctrl->dwork);
        ka_list_del(&exit_ctrl->node);
        apm_vfree(exit_ctrl);
    }
    ka_task_mutex_destroy(&exit_ctrl_mutex);
}
DECLAER_FEATURE_AUTO_UNINIT(apm_task_exit_uninit, FEATURE_LOADER_STAGE_9);
