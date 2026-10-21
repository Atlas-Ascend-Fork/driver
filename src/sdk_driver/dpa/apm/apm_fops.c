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
#include "ka_ioctl_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

#include "apm_auto_init.h"
#include "apm_slab.h"
#include "pbl/pbl_task_ctx.h"

#include "pbl/pbl_davinci_api.h"
#include "apm_fops.h"

static int apm_open(ka_inode_t *inode, ka_file_t *file)
{
    struct task_id_entity *task = NULL;

    task = (struct task_id_entity *)apm_kzalloc(sizeof(struct task_id_entity), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (task == NULL) {
#ifndef EMU_ST
        apm_err("Failed to kzalloc task_id_entity.\n");
        return -ENOMEM;
#endif
    }

    task->tgid = ka_task_get_current_tgid();
    task_get_start_time_by_tgid(ka_task_get_current_tgid(), &task->start_time);
    ka_fs_set_file_private_data(file, (void *)task);

    apm_info("Open. (tgid=%d)\n", ka_task_get_current_tgid());
    return 0;
}

static int apm_release(ka_inode_t *inode, ka_file_t *file)
{
    return 0;
}

static int apm_pre_release(ka_file_t *file, unsigned long mode)
{
#ifndef EMU_ST
    struct task_id_entity *task = (struct task_id_entity *)ka_fs_get_file_private_data(file);
    if (task != NULL) {
        module_feature_auto_uninit_task(0, task->tgid, (void *)&(task->start_time));
        apm_info("Release. (tgid=%d)\n", task->tgid);
        apm_kfree(task);
        ka_fs_set_file_private_data(file, NULL);
    }
#endif
    return 0;
}

static int (*apm_ioctl_handler[APM_MAX_CMD])(u32 cmd, unsigned long arg) = {
    NULL,
};

void apm_register_ioctl_cmd_func(int nr, int (*fn)(u32 cmd, unsigned long arg))
{
    apm_ioctl_handler[nr] = fn;
}

static long apm_ioctl(ka_file_t *file, u32 cmd, unsigned long arg)
{
    if (arg == 0) {
        return -EINVAL;
    }
    if (_KA_IOC_NR(cmd) >= APM_MAX_CMD) {
        apm_err("The command is invalid, which is out of range. (cmd=%u, valid_range=[0, %d])\n", _KA_IOC_NR(cmd),
                APM_MAX_CMD - 1);
        return -EINVAL;
    }

    if (apm_ioctl_handler[_KA_IOC_NR(cmd)] == NULL) {
#ifndef EMU_ST
        apm_warn("The command is not supported. (cmd=%u)\n", _KA_IOC_NR(cmd));
#endif
        return -EOPNOTSUPP;
    }

    return apm_ioctl_handler[_KA_IOC_NR(cmd)](cmd, arg);
}

static ka_file_operations_t apm_fops = {
    .owner = KA_THIS_MODULE, .open = apm_open, .release = apm_release, .unlocked_ioctl = apm_ioctl};

static const struct notifier_operations apm_notifier_ops = {
    .notifier_call = apm_pre_release,
};

int apm_fops_init(void)
{
    int ret;

    ret = drv_davinci_register_sub_module(APM_CHAR_DEV_NAME, &apm_fops);
    if (ret != 0) {
        apm_err("Module register fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = drv_ascend_register_notify(APM_CHAR_DEV_NAME, &apm_notifier_ops);
    if (ret != 0) {
        (void)drv_ascend_unregister_sub_module(APM_CHAR_DEV_NAME);
        apm_err("Notifier register fail. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

void apm_fops_uninit(void)
{
    (void)drv_ascend_unregister_notify(APM_CHAR_DEV_NAME);
    (void)drv_ascend_unregister_sub_module(APM_CHAR_DEV_NAME);
}
