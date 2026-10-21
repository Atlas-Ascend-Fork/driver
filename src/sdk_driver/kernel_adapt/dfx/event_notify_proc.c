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
#include <linux/version.h>
#include "ka_dfx_pub.h"
#include "event_notify_proc.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
#ifdef DRV_HOST
int ka_dfx_profile_event_register(ka_profile_type_t type, ka_notifier_block_t *nb)
{
    (void)type;
    (void)nb;
    return 0;
}

int ka_dfx_profile_event_unregister(ka_profile_type_t type, ka_notifier_block_t *nb)
{
    (void)type;
    (void)nb;
    return 0;
}

#else

#ifdef CFG_FEATURE_KA_EVENT_NOTIFY_PROC
#include <linux/export.h>
#include <linux/notifier.h>
#include <linux/sched/stat.h>
#include <trace/events/sched.h>

#include "kernel_adapt_init.h"
#include "ka_kernel_adapt.h"

STATIC BLOCKING_NOTIFIER_HEAD(task_exit_notifier);
STATIC void kernel_adapt_task_exit_info(void *name, struct task_struct *task)
{
    (void)name;
    blocking_notifier_call_chain(&task_exit_notifier, 0, task);
}

int profile_event_register(enum profile_type type, struct notifier_block *n)
{
    (void)type;
    return blocking_notifier_chain_register(&task_exit_notifier, n);
}
EXPORT_SYMBOL_GPL(profile_event_register);

int profile_event_unregister(enum profile_type type, struct notifier_block *n)
{
    (void)type;
    return blocking_notifier_chain_unregister(&task_exit_notifier, n);
}
EXPORT_SYMBOL_GPL(profile_event_unregister);

int ka_dfx_profile_event_register(ka_profile_type_t type, ka_notifier_block_t *nb)
{
    (void)type;
    return blocking_notifier_chain_register(&task_exit_notifier, nb);
}

int ka_dfx_profile_event_unregister(ka_profile_type_t type, ka_notifier_block_t *nb)
{
    (void)type;
    return blocking_notifier_chain_unregister(&task_exit_notifier, nb);
}
#else
#include <linux/profile.h>
int ka_dfx_profile_event_register(ka_profile_type_t type, ka_notifier_block_t *nb)
{
    return profile_event_register((enum profile_type)type, nb);
}

int ka_dfx_profile_event_unregister(ka_profile_type_t type, ka_notifier_block_t *nb)
{
    return profile_event_unregister((enum profile_type)type, nb);
}

#endif /* CFG_FEATURE_KA_EVENT_NOTIFY_PROC */
#endif /* DRV_HOST */

EXPORT_SYMBOL_GPL(ka_dfx_profile_event_register);
EXPORT_SYMBOL_GPL(ka_dfx_profile_event_unregister);
#endif

int event_notify_proc_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0) && defined(CFG_FEATURE_KA_EVENT_NOTIFY_PROC)
    int ret = register_trace_sched_process_exit(kernel_adapt_task_exit_info, NULL);
    if (ret != 0) {
        ka_err("Failed to register proc exit trace. (ret=%d)\n", ret);
        return -1;
    }
    return 0;
#else
    return 0;
#endif
}

void event_notify_proc_uninit(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0) && defined(CFG_FEATURE_KA_EVENT_NOTIFY_PROC)
    unregister_trace_sched_process_exit(kernel_adapt_task_exit_info, NULL);
    tracepoint_synchronize_unregister();
    ka_info("Unregister proc exit trace.\n");
#endif
}