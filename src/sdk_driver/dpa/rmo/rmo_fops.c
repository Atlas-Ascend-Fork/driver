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
#include "ka_fs_pub.h"
#include "ka_memory_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_kernel_def_pub.h"
#include "rmo_auto_init.h"
#include "pbl/pbl_task_ctx.h"

#include "pbl/pbl_davinci_api.h"
#include "rmo_fops.h"
#include "rmo_slab.h"

static int rmo_open(ka_inode_t *inode, ka_file_t *file)
{
    struct task_id_entity *task = NULL;

    task = (struct task_id_entity *)rmo_kzalloc(sizeof(struct task_id_entity), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (task == NULL) {
#ifndef EMU_ST
        rmo_err("Failed to kzalloc task_id_entity.\n");
        return -ENOMEM;
#endif
    }

    task->tgid = ka_task_get_current_tgid();
    task_get_start_time_by_tgid(ka_task_get_current_tgid(), &task->start_time);
    ka_fs_set_file_private_data(file, (void *)task);

    rmo_info("Open. (tgid=%d)\n", ka_task_get_current_tgid());
    return 0;
}

static int rmo_release(ka_inode_t *inode, ka_file_t *file)
{
    struct task_id_entity *task = (struct task_id_entity *)ka_fs_get_file_private_data(file);
    if (task != NULL) {
        module_feature_auto_uninit_task(0, task->tgid, (void *)&(task->start_time));
        rmo_info("Release. (tgid=%d)\n", task->tgid);
        rmo_kfree(task);
        ka_fs_set_file_private_data(file, NULL);
    }

    return 0;
}

static int (*rmo_ioctl_handler[RMO_MAX_CMD])(u32 cmd, unsigned long arg) = {
    NULL,
};

void rmo_register_ioctl_cmd_func(int nr, int (*fn)(u32 cmd, unsigned long arg))
{
    rmo_ioctl_handler[nr] = fn;
}

static long rmo_ioctl(ka_file_t *file, u32 cmd, unsigned long arg)
{
    if (arg == 0) {
        return -EINVAL;
    }
    if (_KA_IOC_NR(cmd) >= RMO_MAX_CMD) {
        rmo_err("The command is invalid, out of range. (cmd=%u; valid range=[0, %u])\n", _KA_IOC_NR(cmd),
                (u32)(RMO_MAX_CMD - 1));
        return -EINVAL;
    }

    if (rmo_ioctl_handler[_KA_IOC_NR(cmd)] == NULL) {
        rmo_warn("The command is not supported. (cmd=%u)\n", _KA_IOC_NR(cmd));
        return -EOPNOTSUPP;
    }

    return rmo_ioctl_handler[_KA_IOC_NR(cmd)](cmd, arg);
}

static int rmo_mremap(ka_vm_area_struct_t *area)
{
    return -ENOTSUPP;
}

static ka_vm_operations_struct_t rmo_vm_ops = {ka_vm_ops_init_mremap(rmo_mremap)};

static int rmo_mmap(ka_file_t *file, ka_vm_area_struct_t *vma)
{
    ka_mm_set_vm_ops(vma, &rmo_vm_ops);
    ka_mm_set_vm_flags(vma, ka_mm_get_vm_flags(vma) | KA_VM_LOCKED | KA_VM_DONTEXPAND | KA_VM_DONTDUMP |
                                KA_VM_DONTCOPY | KA_VM_IO | KA_VM_PFNMAP);
    if (!(ka_mm_get_vm_flags(vma) & KA_VM_WRITE)) {
        ka_mm_vm_flags_clear(vma, KA_VM_MAYWRITE);
    }
    ka_mm_set_vm_private_data(vma, (void *)(uintptr_t)RMO_VMA_MAP_MAGIC_VERIFY);

    return 0;
}

static ka_file_operations_t rmo_fops = {
    .owner = KA_THIS_MODULE,
    .open = rmo_open,
    .release = rmo_release,
    .unlocked_ioctl = rmo_ioctl,
    .mmap = rmo_mmap,
};

int rmo_fops_init(void)
{
    int ret;

    ret = drv_davinci_register_sub_module(RMO_CHAR_DEV_NAME, &rmo_fops);
    if (ret != 0) {
        rmo_err("Module register fail. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}

void rmo_fops_uninit(void)
{
    (void)drv_ascend_unregister_sub_module(RMO_CHAR_DEV_NAME);
}
