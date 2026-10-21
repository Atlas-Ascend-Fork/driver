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

#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_fs_pub.h"
#include "ka_memory_pub.h"
#include "devdrv_manager_container.h"
#include "devdrv_pcie.h"
#include "devdrv_device_online.h"

KA_TASK_DECLARE_WAIT_QUEUE_HEAD(devid_upgrade_waitq);
KA_TASK_DEFINE_SPINLOCK(devdrv_online_fifo_lock);

#define FIFO_CELL_SIZE sizeof(u32)
#define DEVDRV_MAX_DEVID_BUFFER (ASCEND_PDEV_MAX_NUM * FIFO_CELL_SIZE)

static ka_kfifo_t devdrv_devid_kfifo;
volatile u32 devid_upgrade_flag = 0;

STATIC int devdrv_manager_online_save_devids(ka_kfifo_t *pkfifo, u32 dev_id)
{
    ka_task_spin_lock_bh(&devdrv_online_fifo_lock);
    if (ka_base_kfifo_avail(pkfifo) < FIFO_CELL_SIZE) {
        ka_task_spin_unlock_bh(&devdrv_online_fifo_lock);
        devdrv_drv_err("dev_id(%u) ka_base_kfifo_avail failed, maybe user thread exit.\n", dev_id);
        return -ENOMEM;
    }
    ka_base_kfifo_in(pkfifo, &dev_id, FIFO_CELL_SIZE);
    ka_task_spin_unlock_bh(&devdrv_online_fifo_lock);

    return 0;
}

void devdrv_manager_online_del_devids(u32 dev_id)
{
    u32 dev_ids[ASCEND_PDEV_MAX_NUM] = {0};
    u32 id_buf;
    int i = 0;
    int j;

    if (!devdrv_manager_is_pf_device(dev_id)) {
        return;
    }

    ka_task_spin_lock_bh(&devdrv_online_fifo_lock);
    while (ka_base_kfifo_len(&devdrv_devid_kfifo) >= FIFO_CELL_SIZE) {
        if (ka_base_kfifo_out(&devdrv_devid_kfifo, &id_buf, FIFO_CELL_SIZE) && (i < ASCEND_PDEV_MAX_NUM)) {
            dev_ids[i++] = id_buf;
        }
    }

    ka_base_kfifo_reset(&devdrv_devid_kfifo);
    for (j = 0; j < i; j++) {
        if ((ka_base_kfifo_avail(&devdrv_devid_kfifo) >= FIFO_CELL_SIZE) && (dev_ids[j] != dev_id)) {
            ka_base_kfifo_in(&devdrv_devid_kfifo, &dev_ids[j], FIFO_CELL_SIZE);
        }
    }
    ka_task_spin_unlock_bh(&devdrv_online_fifo_lock);
}

/* devid update we need inform user */
void devdrv_manager_online_devid_update(u32 dev_id)
{
    int ret;

    if (!devdrv_manager_is_pf_device(dev_id)) {
        return;
    }

    ret = devdrv_manager_online_save_devids(&devdrv_devid_kfifo, dev_id);
    if (ret) {
        devdrv_drv_err("devdrv_save_devid failed, ret = %d.\n", ret);
    }
    devid_upgrade_flag = 1;
    ka_task_wake_up_interruptible(&devid_upgrade_waitq);
}

int devdrv_manager_online_get_devids(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    u32 dev_ids[ASCEND_PDEV_MAX_NUM];
    u32 dev_id;
    int j = 0;
    int ret;
    int i;

    if (devdrv_manager_container_is_in_container()) {
        devdrv_drv_err("device online interface does not support container\n");
        return -EPERM;
    }

    for (i = 0; i < ASCEND_PDEV_MAX_NUM; i++) {
        dev_ids[i] = ASCEND_PDEV_MAX_NUM;
    }

    i = 0;
    ka_task_spin_lock_bh(&devdrv_online_fifo_lock);
    while (ka_base_kfifo_len(&devdrv_devid_kfifo) >= FIFO_CELL_SIZE) {
        if (ka_base_kfifo_out(&devdrv_devid_kfifo, &dev_id, FIFO_CELL_SIZE) && (i < ASCEND_PDEV_MAX_NUM)) {
            dev_ids[i++] = dev_id;
        }
    }
    ka_task_spin_unlock_bh(&devdrv_online_fifo_lock);
    if (copy_to_user_safe((void *)(uintptr_t)arg, dev_ids, FIFO_CELL_SIZE * ASCEND_PDEV_MAX_NUM)) {
        devdrv_drv_err("copy_to_user_safe failed\n");
        /* copy to user failed, need to add devids to fifo */
        while (j < i) {
            ret = devdrv_manager_online_save_devids(&devdrv_devid_kfifo, dev_ids[j]);
            if (ret) {
                devdrv_drv_err("devdrv_manager_online_save_devids failed, ret(%d)\n", ret);
                return ret;
            }
            j++;
        }
        return -EFAULT;
    }
    devid_upgrade_flag = 0;

    return 0;
}

u32 devdrv_manager_poll(ka_file_t *filep, ka_poll_table_struct_t *wait)
{
    u32 ret = 0;

    if (devdrv_manager_container_is_in_container()) {
        devdrv_drv_err("device online poll interface does not support container\n");
        return KA_POLLERR;
    }

    ka_task_poll_wait(filep, &devid_upgrade_waitq, wait);
    if (devid_upgrade_flag) {
        ret = KA_POLLIN | KA_POLLRDNORM;
    }
    return ret;
}

int devdrv_manager_online_kfifo_alloc(void)
{
    return ka_base_kfifo_alloc(&devdrv_devid_kfifo, DEVDRV_MAX_DEVID_BUFFER, KA_GFP_KERNEL);
}

void devdrv_manager_online_kfifo_free(void)
{
    ka_base_kfifo_free(&devdrv_devid_kfifo);
}
