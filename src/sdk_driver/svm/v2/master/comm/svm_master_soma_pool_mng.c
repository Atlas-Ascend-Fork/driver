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

#include "ka_type.h"

#include "svm_kernel_msg.h"
#include "svm_msg_client.h"
#include "svm_master_soma_pool_mng.h"

static bool devmm_soma_pool_range_is_valid(u64 va, u64 size)
{
    const u64 soma_start = DEVMM_SOMA_MEM_START;
    const u64 soma_end = DEVMM_SOMA_MEM_START + DEVMM_SOMA_MEM_SIZE;

    if ((va == 0) || (size == 0) || (va < soma_start) || (va >= soma_end)) {
        return false;
    }

    return size <= (soma_end - va);
}

static int devmm_soma_pool_check_proc_and_dev(struct devmm_svm_process *svm_proc, const struct devmm_devid *devids)
{
#ifndef EMU_ST
    if ((svm_proc == NULL) || (devids == NULL)) {
        devmm_drv_err("Invalid soma pool context. (svm_proc_is_null=%d; devids_is_null=%d)\n", (svm_proc == NULL),
                      (devids == NULL));
        return -EINVAL;
    }

    if (devids->devid >= DEVMM_MAX_DEVICE_NUM) {
        devmm_drv_err("Invalid soma pool device. (hostpid=%d; devid=%u)\n", svm_proc->process_id.hostpid,
                      devids->devid);
        return -EINVAL;
    }
#endif
    return 0;
}

static int devmm_soma_create_pool_para_check(struct devmm_svm_process *svm_proc,
                                             struct devmm_soma_pool_create_para *para, struct devmm_devid *devids)
{
#ifndef EMU_ST
    int ret = devmm_soma_pool_check_proc_and_dev(svm_proc, devids);
    if (ret != 0) {
        return ret;
    }

    if (para == NULL) {
        devmm_drv_err("Create soma pool para is NULL. (hostpid=%d; devid=%u)\n", svm_proc->process_id.hostpid,
                      devids->devid);
        return -EINVAL;
    }
#endif
    if (!devmm_soma_pool_range_is_valid(para->va, para->size)) {
#ifndef EMU_ST
        devmm_drv_err("Invalid soma pool range. (hostpid=%d; devid=%u; pool_id=0x%llx; va=0x%llx; size=%llu)\n",
                      svm_proc->process_id.hostpid, devids->devid, para->pool_id, para->va, para->size);
        return -EINVAL;
#endif
    }
#ifndef EMU_ST
    if ((KA_DRIVER_IS_ALIGNED(para->va, DEVMM_HUGE_PAGE_SIZE) == false) ||
        (KA_DRIVER_IS_ALIGNED(para->size, DEVMM_HUGE_PAGE_SIZE) == false)) {
        devmm_drv_err("Soma pool range is not aligned. (hostpid=%d; devid=%u; pool_id=0x%llx; va=0x%llx; size=%llu)\n",
                      svm_proc->process_id.hostpid, devids->devid, para->pool_id, para->va, para->size);
        return -EINVAL;
    }
#endif
    return 0;
}

static int devmm_soma_destroy_pool_para_check(struct devmm_svm_process *svm_proc,
                                              struct devmm_soma_pool_destroy_para *para, struct devmm_devid *devids)
{
#ifndef EMU_ST
    int ret = devmm_soma_pool_check_proc_and_dev(svm_proc, devids);
    if (ret != 0) {
        return ret;
    }

    if (para == NULL) {
        devmm_drv_err("Destroy soma pool para is NULL. (hostpid=%d; devid=%u)\n", svm_proc->process_id.hostpid,
                      devids->devid);
        return -EINVAL;
    }
#endif
    return 0;
}

static int devmm_soma_send_create_pool_msg(struct devmm_svm_process *svm_proc, struct devmm_soma_pool_create_para *para,
                                           struct devmm_devid *devids)
{
    struct devmm_chan_soma_pool_create msg = {{{0}}};
    int ret = 0;

    msg.head.msg_id = DEVMM_CHAN_SOMA_CREATE_POOL_H2D_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.dev_id = (u16)devids->devid;
    msg.pool_id = (u64)para->pool_id;
    msg.va = (u64)para->va;
    msg.size = (u64)para->size;
    ret = devmm_chan_msg_send(&msg, sizeof(struct devmm_chan_soma_pool_create),
                              sizeof(struct devmm_chan_soma_pool_create));
    if (ret != 0) {
        devmm_drv_err("Mem pool create msg send failed. (ret=%d; hostpid=%d; devid=%u; pool_id=0x%llx)\n", ret,
                      svm_proc->process_id.hostpid, devids->devid, para->pool_id);
    }
    return ret;
}

int devmm_ioctl_soma_create_pool(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *args)
{
    struct devmm_soma_pool_create_para *para = NULL;
    struct devmm_devid *devids = NULL;
    struct devmm_svm_heap *heap = NULL;
    u64 find_va = 0;
    int ret = 0;

#ifndef EMU_ST
    if (args == NULL) {
        devmm_drv_err("Create soma pool args is NULL.\n");
        return -EINVAL;
    }
#endif
    para = &args->data.soma_pool_create_para;
    devids = &args->head;
    heap = devmm_soma_get_heap(svm_proc, DEVMM_SOMA_MEM_START);
#ifndef EMU_ST
    if (heap == NULL) {
        devmm_drv_err("Mem pool get heap failed. (hostpid=%d; devid=%u; pool_id=0x%llx)\n",
                      svm_proc->process_id.hostpid, devids->devid, para->pool_id);
        return -EINVAL;
    }
#endif
    ka_task_down_read(&svm_proc->bitmap_sem);
    ret = devmm_soma_find_best_va(heap, (u64)para->size, &find_va);
    if (ret != 0) {
        ka_task_up_read(&svm_proc->bitmap_sem);
        devmm_drv_err("No suitable VA region for soma pool. (hostpid=%d; devid=%u; pool_id=0x%llx; size=%llu)\n",
                      svm_proc->process_id.hostpid, devids->devid, para->pool_id, para->size);
        return ret;
    }
    para->va = find_va;

    ret = devmm_soma_create_pool_para_check(svm_proc, para, devids);
    if (ret != 0) {
        ka_task_up_read(&svm_proc->bitmap_sem);
        return ret;
    }

    ret = devmm_soma_send_create_pool_msg(svm_proc, para, devids);
    if (ret != 0) {
        ka_task_up_read(&svm_proc->bitmap_sem);
        return ret;
    }
    devmm_soma_set_heap_bits(heap, para->va, para->size, devids->logical_devid);
    ka_task_up_read(&svm_proc->bitmap_sem);

    return ret;
}

int devmm_ioctl_soma_destroy_pool(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *args)
{
    struct devmm_soma_pool_destroy_para *para = NULL;
    struct devmm_devid *devids = NULL;
    struct devmm_svm_heap *heap = NULL;
    struct devmm_chan_soma_pool_destroy msg = {{{0}}};
    int ret = 0;

    devmm_drv_debug("devmm ioctl destroy pool.\n");
#ifndef EMU_ST
    if (args == NULL) {
        devmm_drv_err("Destroy soma pool args is NULL.\n");
        return -EINVAL;
    }
#endif
    para = &args->data.soma_pool_destroy_para;
    devids = &args->head;
    ret = devmm_soma_destroy_pool_para_check(svm_proc, para, devids);
#ifndef EMU_ST
    if (ret != 0) {
        return ret;
    }
#endif
    heap = devmm_soma_get_heap(svm_proc, DEVMM_SOMA_MEM_START);
#ifndef EMU_ST
    if (heap == NULL) {
        devmm_drv_err("Mem pool get heap failed. (hostpid=%d; devid=%u; pool_id=0x%llx)\n",
                      svm_proc->process_id.hostpid, devids->devid, para->pool_id);
        return -EINVAL;
    }
#endif
    msg.head.msg_id = DEVMM_CHAN_SOMA_DESTROY_POOL_H2D_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.dev_id = (u16)devids->devid;
    msg.pool_id = (u64)para->pool_id;
    ret = devmm_chan_msg_send(&msg, sizeof(struct devmm_chan_soma_pool_destroy),
                              sizeof(struct devmm_chan_soma_pool_destroy));
    if (ret != 0) {
        devmm_drv_err("Mem pool destroy msg send failed. (ret=%d; hostpid=%d; devid=%u; pool_id=0x%llx)\n", ret,
                      svm_proc->process_id.hostpid, devids->devid, para->pool_id);
        return ret;
    }
    ka_task_down_read(&svm_proc->bitmap_sem);
    devmm_soma_unset_heap_bits(heap, msg.pool_va, msg.pool_size);
    ka_task_up_read(&svm_proc->bitmap_sem);

    return 0;
}

int devmm_ioctl_soma_trim_pool(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *args)
{
    struct devmm_soma_pool_trim_para *para = NULL;
    struct devmm_devid *devids = NULL;
    struct devmm_chan_soma_pool_trim msg = {{{0}}};
    int ret = 0;

    devmm_drv_debug("devmm ioctl trim pool.\n");
#ifndef EMU_ST
    if (args == NULL) {
        devmm_drv_err("Trim soma pool args is NULL.\n");
        return -EINVAL;
    }
#endif
    para = &args->data.soma_pool_trim_para;
    devids = &args->head;

    if ((KA_DRIVER_IS_ALIGNED(para->reserved_size, DEVMM_HUGE_PAGE_SIZE) == false) ||
        (KA_DRIVER_IS_ALIGNED(para->used_size, DEVMM_HUGE_PAGE_SIZE) == false) ||
        (KA_DRIVER_IS_ALIGNED(para->free_size, DEVMM_HUGE_PAGE_SIZE) == false)) {
        devmm_drv_err("Soma pool trim size is not aligned. (hostpid=%d; devid=%u; pool_id=0x%llx; "
                      "reserved_size=%llu; used_size=%llu; free_size=%llu)\n",
                      svm_proc->process_id.hostpid, devids->devid, para->pool_id, para->reserved_size, para->used_size,
                      para->free_size);
        return -EINVAL;
    }
    msg.head.msg_id = DEVMM_CHAN_SOMA_TRIM_POOL_H2D_ID;
    msg.head.process_id.hostpid = svm_proc->process_id.hostpid;
    msg.head.dev_id = (u16)devids->devid;
    msg.pool_id = (u64)para->pool_id;
    msg.reserved_size = (u64)para->reserved_size;
    msg.used_size = (u64)para->used_size;
    msg.free_size = (u64)para->free_size;
    ret = devmm_chan_msg_send(&msg, sizeof(struct devmm_chan_soma_pool_trim), sizeof(struct devmm_chan_soma_pool_trim));
    if (ret != 0) {
        devmm_drv_err("Mem pool trim msg send failed. (ret=%d; hostpid=%d; devid=%u; pool_id=0x%llx)\n", ret,
                      svm_proc->process_id.hostpid, devids->devid, para->pool_id);
        return ret;
    }

    para->reserved_size = msg.reserved_size;

    return 0;
}
