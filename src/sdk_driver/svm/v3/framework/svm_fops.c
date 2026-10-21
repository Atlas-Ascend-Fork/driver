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
#include "ka_ioctl_pub.h"
#include "ka_fs_pub.h"
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"

#include "pbl_feature_loader.h"
#include "pbl/pbl_davinci_api.h"
#include "pbl_uda.h"

#include "svm_addr_desc.h"
#include "svm_ioctl_ex.h"
#include "svm_kern_log.h"
#include "svm_task.h"
#include "svm_slab.h"
#include "framework_cmd.h"
#include "svm_fops.h"

static int svm_setup_private_data(ka_file_t *file, int tgid)
{
    struct task_id_entity *task_id = NULL;

    if (ka_fs_get_file_private_data(file) != NULL) {
        svm_err("Private_data isn't null.\n");
        return -EINVAL;
    }

    task_id = (struct task_id_entity *)svm_kzalloc(sizeof(struct task_id_entity), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (task_id == NULL) {
        svm_err("Failed to kzalloc task_id_entity.\n");
        return -ENOMEM;
    }

    task_id->tgid = tgid;
    task_get_start_time_by_tgid(tgid, &task_id->start_time);
    ka_fs_set_file_private_data(file, (void *)task_id);

    return 0;
}

static void svm_clear_private_data(ka_file_t *file)
{
    struct task_id_entity *task_id = (struct task_id_entity *)ka_fs_get_file_private_data(file);

    if (task_id != NULL) {
        svm_kfree(task_id);
        ka_fs_set_file_private_data(file, NULL);
    }
}

static int svm_open(ka_inode_t *inode, ka_file_t *file)
{
    struct task_id_entity *task_id = NULL;
    u32 udevid = drv_davinci_get_device_id(file);
    int tgid = (int)ka_task_get_current_tgid();
    int ret;

    if (!uda_can_access_udevid(udevid) && (udevid != uda_get_host_id())) {
        svm_info("Can not access device. (udevid=%u; tgid=%d)\n", udevid, tgid);
        return -EPERM;
    }

    ret = svm_setup_private_data(file, tgid);
    if (ret != 0) {
        return ret;
    }

    task_id = (struct task_id_entity *)ka_fs_get_file_private_data(file);
    ret = svm_add_task(udevid, tgid, &task_id->start_time);
    if (ret != 0) {
        svm_clear_private_data(file);
        return ret;
    }

    svm_info("Open. (udevid=%u; tgid=%d)\n", udevid, tgid);

    return 0;
}

static int svm_release(ka_inode_t *inode, ka_file_t *file)
{
    return 0;
}

static int svm_pre_release(ka_file_t *file, unsigned long mode)
{
    u32 udevid = drv_davinci_get_device_id(file);
    struct task_id_entity *task_id = (struct task_id_entity *)ka_fs_get_file_private_data(file);
    if (task_id != NULL) {
        int tgid = task_id->tgid;
        int ret = svm_del_task(udevid, tgid, &task_id->start_time);

        svm_info("Release. (udevid=%u; tgid=%d; ret=%d)\n", udevid, tgid, ret);
        svm_clear_private_data(file);
    }

    return 0;
}

static int (*svm_ioctl_handler[SVM_MAX_CMD])(u32 udevid, u32 cmd, unsigned long arg) = {
    NULL,
};
static int (*svm_ioctl_pre_handler[SVM_MAX_CMD])(u32 udevid, u32 cmd, void *para) = {
    NULL,
};
static void (*svm_ioctl_pre_cancle_handler[SVM_MAX_CMD])(u32 udevid, u32 cmd, void *para) = {
    NULL,
};
static int (*svm_ioctl_post_handler[SVM_MAX_CMD])(u32 udevid, u32 cmd, void *para) = {
    NULL,
};

void svm_register_ioctl_cmd_handle(int nr, int (*fn)(u32 udevid, u32 cmd, unsigned long arg))
{
    svm_ioctl_handler[nr] = fn;
}

void svm_register_ioctl_cmd_pre_handle(int nr, int (*fn)(u32 udevid, u32 cmd, void *para))
{
    svm_ioctl_pre_handler[nr] = fn;
}

void svm_register_ioctl_cmd_pre_cancle_handle(int nr, void (*fn)(u32 udevid, u32 cmd, void *para))
{
    svm_ioctl_pre_cancle_handler[nr] = fn;
}

void svm_register_ioctl_cmd_post_handle(int nr, int (*fn)(u32 udevid, u32 cmd, void *para))
{
    svm_ioctl_post_handler[nr] = fn;
}

int svm_call_ioctl_pre_handler(u32 udevid, u32 cmd, void *para)
{
    u32 cmd_nr = _KA_IOC_NR(cmd);
    int ret;

    if (svm_ioctl_pre_handler[cmd_nr] != NULL) {
        ret = svm_ioctl_pre_handler[cmd_nr](udevid, cmd, para);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

void svm_call_ioctl_pre_cancle_handler(u32 udevid, u32 cmd, void *para)
{
    u32 cmd_nr = _KA_IOC_NR(cmd);
    if (svm_ioctl_pre_cancle_handler[cmd_nr] != NULL) {
        svm_ioctl_pre_cancle_handler[cmd_nr](udevid, cmd, para);
    }
}

int svm_call_ioctl_post_handler(u32 udevid, u32 cmd, void *para)
{
    u32 cmd_nr = _KA_IOC_NR(cmd);
    int ret;

    if (svm_ioctl_post_handler[cmd_nr] != NULL) {
        ret = svm_ioctl_post_handler[cmd_nr](udevid, cmd, para);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static long svm_ioctl(ka_file_t *file, u32 cmd, unsigned long arg)
{
    u32 cmd_nr = _KA_IOC_NR(cmd);
    u32 udevid;

    if ((file == NULL) || (arg == 0)) {
        svm_err("The file or arg is null.\n");
        return -EINVAL;
    }

    if (cmd_nr >= SVM_MAX_CMD) {
        svm_err("The command is invalid, which is out of range. (cmd=%u; valid range=[0, %u))\n", cmd_nr,
                (u32)SVM_MAX_CMD);
        return -EINVAL;
    }

    if (svm_ioctl_handler[cmd_nr] == NULL) {
        return -EOPNOTSUPP;
    }

    udevid = drv_davinci_get_device_id(file);
    return (long)svm_ioctl_handler[cmd_nr](udevid, cmd, arg);
}

static int svm_mmap(ka_file_t *file, ka_vm_area_struct_t *vma)
{
    return -EOPNOTSUPP;
}

static ka_file_operations_t svm_fops = {
    .owner = KA_THIS_MODULE, .open = svm_open, .release = svm_release, .unlocked_ioctl = svm_ioctl, .mmap = svm_mmap};

static const struct notifier_operations svm_notifier_ops = {
    .notifier_call = svm_pre_release,
};

int svm_fops_init(void)
{
    int ret;

    ret = drv_davinci_register_sub_module(SVM_CHAR_DEV_NAME, &svm_fops);
    if (ret != 0) {
        svm_err("Module register fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = drv_ascend_register_notify(SVM_CHAR_DEV_NAME, &svm_notifier_ops);
    if (ret != 0) {
        (void)drv_ascend_unregister_sub_module(SVM_CHAR_DEV_NAME);
        svm_err("Notifier register fail. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

void svm_fops_uninit(void)
{
    (void)drv_ascend_unregister_notify(SVM_CHAR_DEV_NAME);
    (void)drv_ascend_unregister_sub_module(SVM_CHAR_DEV_NAME);
}
