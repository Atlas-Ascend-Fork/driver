/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "devdrv_manager.h"
#include "devdrv_manager_common.h"
#include "devdrv_black_box.h"
#include "devdrv_common.h"
#include "comm_kernel_interface.h"
#include "dms_urd_forward.h"
#include "hvdevmng_init.h"
#include "pbl_mem_alloc_interface.h"
#include "davinci_interface.h"
#include "devdrv_user_common.h"
#include "pbl/pbl_davinci_api.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "ascend_kernel_hal.h"
#include "pbl/pbl_soc_res_attr.h"
#include "pbl/pbl_chip_config.h"
#include "adapter_api.h"
#include "ka_ioctl_pub.h"
#include "ka_compiler_pub.h"
#include "ka_system_pub.h"
#include "ka_task_pub.h"
#include "ka_dfx_pub.h"
#include "devdrv_manager_container.h"
#include "devdrv_pcie.h"

#define DEVMNG_TSLOG_MAX_SIZE (1024 * 1024) /* 1M bytes */

#define BBOX_AP_BIAS 0x202000ul
#define BBOX_VMCORE_MASK_OFFSET 24

STATIC struct devdrv_dev_log *g_devdrv_dev_log_array[DEVDRV_LOG_DUMP_TYPE_MAX][ASCEND_PDEV_MAX_NUM] = {{NULL}};
STATIC struct devdrv_ts_log *g_devdrv_ts_log_array[ASCEND_DEV_MAX_NUM] = {NULL};
STATIC ka_mutex_t g_devdrv_devlog_mutex;
STATIC ka_mutex_t g_devdrv_tslog_mutex;

STATIC struct devdrv_dev_log *devdrv_get_devlog_info(u32 dev_id, u32 log_type)
{
    if (dev_id >= ASCEND_PDEV_MAX_NUM || log_type >= DEVDRV_LOG_DUMP_TYPE_MAX) {
        devdrv_drv_err("Invalid parameter. (dev_id=%u; dev_maxnum=%d; log_type=%u; logtype_maxnum=%d)\n", dev_id,
                       ASCEND_PDEV_MAX_NUM, log_type, DEVDRV_LOG_DUMP_TYPE_MAX);
        return NULL;
    }

    return g_devdrv_dev_log_array[log_type][dev_id];
}

STATIC struct devdrv_ts_log *devdrv_get_tslog_info(u32 dev_id)
{
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Invalid parameter. (device id=%u)\n", dev_id);
        return NULL;
    }

    return g_devdrv_ts_log_array[dev_id];
}

#ifdef CFG_FEATURE_PCIE_BBOX_DUMP
void devdrv_devlog_init(struct devdrv_info *dev_info, struct devdrv_device_info *drv_info)
{
    if (!devdrv_manager_is_pf_device(dev_info->dev_id) || dev_info->dev_id >= ASCEND_PDEV_MAX_NUM) {
        return;
    }
    ka_task_mutex_lock(&g_devdrv_devlog_mutex);
    g_devdrv_dev_log_array[LOG_SLOG_BBOX_DDR][dev_info->dev_id]->dma_addr = drv_info->dump_ddr_dma_addr;
    g_devdrv_dev_log_array[LOG_SLOG_BBOX_DDR][dev_info->dev_id]->mem_size = drv_info->dump_ddr_size;
    g_devdrv_dev_log_array[LOG_SLOG_REG_DDR][dev_info->dev_id]->dma_addr = drv_info->reg_ddr_dma_addr;
    g_devdrv_dev_log_array[LOG_SLOG_REG_DDR][dev_info->dev_id]->mem_size = drv_info->reg_ddr_size;
    g_devdrv_dev_log_array[LOG_VMCORE_FILE_DDR][dev_info->dev_id]->dma_addr = drv_info->vmcore_ddr_dma_addr;
    g_devdrv_dev_log_array[LOG_VMCORE_FILE_DDR][dev_info->dev_id]->mem_size = drv_info->vmcore_ddr_size;
    ka_task_mutex_unlock(&g_devdrv_devlog_mutex);
}
#endif

int align_to_4k(u32 size_in, u64 *aligned_size)
{
    u64 temp = (u64)size_in + KA_MM_PAGE_SIZE - 1;

    *aligned_size = (temp / KA_MM_PAGE_SIZE) * KA_MM_PAGE_SIZE;
    return 0;
}

int devdrv_manager_receive_devlog_addr(void *msg, u32 *ack_len)
{
    u32 dev_id;
    u32 log_type;
    struct devdrv_dev_log *dev_log = NULL;
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    dev_id = dev_manager_msg_info->header.dev_id;

    if (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_VALID) {
        devdrv_drv_err("Invalid message from device. (device id=%u)\n", dev_id);
        return -EINVAL;
    }

    dev_log = (struct devdrv_dev_log *)dev_manager_msg_info->payload;
    log_type = dev_log->log_type;

    if (dev_id >= ASCEND_PDEV_MAX_NUM || log_type >= DEVDRV_LOG_DUMP_TYPE_MAX) {
        devdrv_drv_err("Invalid log info. (dev_id=%u; dev_maxnum=%d; logtype=%u; logtype_maxnum=%d)\n", dev_id,
                       ASCEND_PDEV_MAX_NUM, log_type, DEVDRV_LOG_DUMP_TYPE_MAX);
        return -ENODEV;
    }

    ka_task_mutex_lock(&g_devdrv_devlog_mutex);
    g_devdrv_dev_log_array[log_type][dev_id]->devid = dev_log->devid;
    g_devdrv_dev_log_array[log_type][dev_id]->dma_addr = dev_log->dma_addr;
    g_devdrv_dev_log_array[log_type][dev_id]->mem_size = dev_log->mem_size;
    g_devdrv_dev_log_array[log_type][dev_id]->log_type = dev_log->log_type;
    ka_task_mutex_unlock(&g_devdrv_devlog_mutex);

    *ack_len = sizeof(*dev_manager_msg_info);
    dev_manager_msg_info->header.result = 0;

    return 0;
}

int devdrv_manager_receive_tslog_addr(void *msg, u32 *ack_len)
{
    u32 dev_id;
    struct devdrv_ts_log *ts_log = NULL;
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    dev_id = dev_manager_msg_info->header.dev_id;

    if (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_VALID) {
        devdrv_drv_err("Invalid message from device. (device id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Invalid device id. (device id=%u; dev_maxnum=%d)\n", dev_id, ASCEND_DEV_MAX_NUM);
        return -ENODEV;
    }

    ts_log = (struct devdrv_ts_log *)dev_manager_msg_info->payload;
    ka_task_mutex_lock(&g_devdrv_tslog_mutex);
    g_devdrv_ts_log_array[dev_id]->devid = ts_log->devid;
    g_devdrv_ts_log_array[dev_id]->dma_addr = ts_log->dma_addr;
    g_devdrv_ts_log_array[dev_id]->mem_size = ts_log->mem_size;
    ka_task_mutex_unlock(&g_devdrv_tslog_mutex);

    *ack_len = sizeof(*dev_manager_msg_info);
    dev_manager_msg_info->header.result = 0;

    return 0;
}

STATIC int devdrv_manager_devlog_dump_process(struct devdrv_black_box_user *black_box_user)
{
    int ret = 0;
    u64 align_size = 0;
    void *buffer = NULL;
    ka_dma_addr_t host_addr_dma = 0;
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_get_devdrv_info_array(black_box_user->devid);
    if (dev_info == NULL) {
        devdrv_drv_err("Device is nonexistent. (devid=%u)\n", black_box_user->devid);
        return -ENODEV;
    }

    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        devdrv_drv_warn("Device has been reset. (devid=%u)\n", dev_info->dev_id);
        ret = -ENODEV;
        goto FLAG_DEC;
    }

    ret = align_to_4k(black_box_user->size, &align_size);
    if (ret != 0) {
        devdrv_drv_err("Align size to 4k failed. (dev_id=%u)\n", dev_info->dev_id);
        ret = -EINVAL;
        goto FLAG_DEC;
    }

    buffer = hal_kernel_devdrv_dma_alloc_coherent(dev_info->dev, align_size, &host_addr_dma,
                                                  KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buffer == NULL) {
        devdrv_drv_err("Dma alloc coherent failed. (devid=%u; size=%u)\n", black_box_user->devid, black_box_user->size);
        ret = -ENOMEM;
        goto FLAG_DEC;
    }

    ret = hal_kernel_devdrv_dma_sync_copy(dev_info->pci_dev_id, DEVDRV_DMA_DATA_COMMON,
                                          (u64)black_box_user->addr_offset, (u64)host_addr_dma, black_box_user->size,
                                          DEVDRV_DMA_DEVICE_TO_HOST);
    if (ret != 0) {
        devdrv_drv_err("Dma sync copy failed. (ret=%d; devid=%u)\n", ret, black_box_user->devid);
        goto DMA_FREE;
    }

    ret = copy_to_user_safe(black_box_user->dst_buffer, buffer, black_box_user->size);
    if (ret != 0) {
        devdrv_drv_err("Copy to user failed. (ret=%d; devid=%u)\n", ret, black_box_user->devid);
    }

DMA_FREE:
    hal_kernel_devdrv_dma_free_coherent(dev_info->dev, align_size, buffer, host_addr_dma);
    buffer = NULL;
FLAG_DEC:
    ka_base_atomic_dec(&dev_info->occupy_ref);
    return ret;
}

int devdrv_manager_devlog_dump(struct devdrv_bbox_logdump *in)
{
    int ret;
    struct devdrv_dev_log *dev_log = NULL;
    struct devdrv_black_box_user *black_box_user = NULL;

    if (in == NULL) {
        devdrv_drv_err("Invalid black_box_user parameter.(in=%s)\n", (in == NULL) ? "NULL" : "OK");
        return -ENOMEM;
    }

    if (in->bbox_user == NULL) {
        devdrv_drv_err("Invalid black_box_user parameter.(bbox_user=%s)\n", (in->bbox_user == NULL) ? "NULL" : "OK");
        return -ENOMEM;
    }

    black_box_user = (struct devdrv_black_box_user *)ka_mm_kzalloc(sizeof(struct devdrv_black_box_user),
                                                                   KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (black_box_user == NULL) {
        devdrv_drv_err("Allocate memory for black box failed.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(black_box_user, in->bbox_user, sizeof(struct devdrv_black_box_user));
    if (ret != 0) {
        devdrv_drv_err("Copy from user failed. (ret=%d).\n", ret);
        goto FREE_BBOX_EXIT;
    }

    if (black_box_user->devid >= ASCEND_PDEV_MAX_NUM || in->log_type >= DEVDRV_LOG_DUMP_TYPE_MAX) {
        devdrv_drv_err("Invalid black_box_user parameter. (dev_id=%u; dev_maxnum=%d; log_type=%u; logtype_maxnum=%d)\n",
                       black_box_user->devid, ASCEND_PDEV_MAX_NUM, in->log_type, DEVDRV_LOG_DUMP_TYPE_MAX);
        ret = -EFAULT;
        goto FREE_BBOX_EXIT;
    }

    if (black_box_user->size == 0) {
        devdrv_drv_err("Invalid black_box_user parameter. (size=%u)\n", black_box_user->size);
        ret = -EFAULT;
        goto FREE_BBOX_EXIT;
    }

    ka_task_mutex_lock(&g_devdrv_devlog_mutex);
    dev_log = devdrv_get_devlog_info(black_box_user->devid, in->log_type);
    if (dev_log == NULL || dev_log->mem_size == 0) {
        ka_task_mutex_unlock(&g_devdrv_devlog_mutex);
        devdrv_drv_err("devlog dma addr info is not refreshed by device yet. (devid=%u; log_type=%u).\n",
                       black_box_user->devid, in->log_type);
        ret = -ENODEV;
        goto FREE_BBOX_EXIT;
    }

    if ((KA_U64_MAX - black_box_user->addr_offset <= black_box_user->size) ||
        (black_box_user->addr_offset + black_box_user->size > dev_log->mem_size)) {
        ka_task_mutex_unlock(&g_devdrv_devlog_mutex);
        devdrv_drv_err("Invalid user input parameter. (devid=%u; addr_offset=%llu; size=%u; mem_size=%u)\n",
                       black_box_user->devid, black_box_user->addr_offset, black_box_user->size, dev_log->mem_size);
        ret = -EFAULT;
        goto FREE_BBOX_EXIT;
    }

    black_box_user->addr_offset += dev_log->dma_addr;
    ka_task_mutex_unlock(&g_devdrv_devlog_mutex);
    ret = devdrv_manager_devlog_dump_process(black_box_user);
    if (ret != 0) {
        devdrv_drv_ex_notsupport_err(ret, "Dev log dump failed. (ret=%d; devid=%u)\n", ret, black_box_user->devid);
        goto FREE_BBOX_EXIT;
    }

FREE_BBOX_EXIT:
    ka_mm_kfree(black_box_user);
    black_box_user = NULL;
    return ret;
}

#if !defined(DEVDRV_MANAGER_HOST_UT_TEST) && defined(CFG_FEATURE_PCIE_BBOX_DUMP)
#define DEVDRV_DUMP_SINGLE_LEN 0x200000U
STATIC int devdrv_dma_bbox_dump_para_check(unsigned int dev_id, void *value, u32 log_type)
{
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Invalid device id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (!devdrv_manager_is_pf_device(dev_id)) {
        return -EOPNOTSUPP;
    }

    if (value == NULL) {
        devdrv_drv_err("Invalid buffer. (dev_id=%u)\n", dev_id);
        return -EFAULT;
    }

    if ((log_type >= DEVDRV_LOG_DUMP_TYPE_MAX) || (g_devdrv_dev_log_array[log_type][dev_id] == NULL)) {
        devdrv_drv_err("Invalid log_type or dev log not initialized. (dev_id=%u; log_type=%u)\n", dev_id, log_type);
        return -EINVAL;
    }

    return 0;
}

int devdrv_dma_bbox_dump(struct bbox_dma_dump *dma_dump)
{
    struct devdrv_info *dev_info = NULL;
    ka_dma_addr_t host_addr_dma = 0;
    void *buffer = NULL;
    u64 dump_dma_addr;
    u32 dump_size, i, count, copy_size;
    int ret;
    u32 dev_id = dma_dump->dev_id;
    void *dst_buf;

    ka_task_mutex_lock(&g_devdrv_devlog_mutex);
    ret = devdrv_dma_bbox_dump_para_check(dev_id, dma_dump->dst_buf, dma_dump->log_type);
    if (ret != 0) {
        ka_task_mutex_unlock(&g_devdrv_devlog_mutex);
        devdrv_drv_ex_notsupport_err(ret, "Check black box parameter fail. (dev_id=%u)\n", dev_id);
        return ret;
    }

    dump_size = g_devdrv_dev_log_array[dma_dump->log_type][dev_id]->mem_size;
    dump_dma_addr = g_devdrv_dev_log_array[dma_dump->log_type][dev_id]->dma_addr;
    ka_task_mutex_unlock(&g_devdrv_devlog_mutex);

    if ((dump_size == 0) || (dump_dma_addr == 0)) {
        return -EOPNOTSUPP;
    }

    if ((dma_dump->len <= 0) || (dma_dump->offset >= dump_size) || (dma_dump->len > dump_size - dma_dump->offset)) {
        devdrv_drv_err("Invalid size. (dev_id=%u, len=%u, offset=%u, mem_size=%u)\n", dev_id, dma_dump->len,
                       dma_dump->offset, dump_size);
        return -EINVAL;
    }

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if (dev_info == NULL) {
        devdrv_drv_err("No device. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    dump_dma_addr += dma_dump->offset;
    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_warn("Device has been reset. (dev_id=%d)\n", dev_info->dev_id);
        return -EINVAL;
    }

    buffer = hal_kernel_devdrv_dma_alloc_coherent(dev_info->dev, DEVDRV_DUMP_SINGLE_LEN, &host_addr_dma,
                                                  KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buffer == NULL) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_err("dma_alloc_coherent fail. (dev_id=%u; size=%d)\n", dev_id, dma_dump->len);
        return -ENOMEM;
    }

    count = KA_BASE_DIV_ROUND_UP(dma_dump->len, DEVDRV_DUMP_SINGLE_LEN);
    dst_buf = dma_dump->dst_buf;
    for (i = 0; i < count; i++) {
        copy_size = DEVDRV_DUMP_SINGLE_LEN;
        if (i == (count - 1)) {
            copy_size = dma_dump->len - (count - 1) * DEVDRV_DUMP_SINGLE_LEN;
        }

        ret = hal_kernel_devdrv_dma_sync_copy(dev_info->pci_dev_id, DEVDRV_DMA_DATA_COMMON, (u64)dump_dma_addr,
                                              (u64)host_addr_dma, copy_size, DEVDRV_DMA_DEVICE_TO_HOST);
        if (ret != 0) {
            devdrv_drv_err("Dma sync copy failed. (ret=%d; devid=%u; len=%u; copy_size=%u; idx=%u)\n", ret, dev_id,
                           dma_dump->len, copy_size, i);
            goto free_alloc;
        }

        ret = copy_to_user_safe(dst_buf, buffer, copy_size);
        if (ret != 0) {
            devdrv_drv_err("copy_to_user_safe fail. (ret=%d; dev_id=%u; len=%u; copy_size=%u; idx=%u))\n", ret, dev_id,
                           dma_dump->len, copy_size, i);
            goto free_alloc;
        }
        dump_dma_addr += copy_size;
        dst_buf += copy_size;
    }

free_alloc:
    ka_base_atomic_dec(&dev_info->occupy_ref);
    hal_kernel_devdrv_dma_free_coherent(dev_info->dev, DEVDRV_DUMP_SINGLE_LEN, buffer, host_addr_dma);
    buffer = NULL;
    return ret;
}
#endif

STATIC int devdrv_manager_get_black_box_exception_index(void)
{
    int i;

    /* find the same pid */
    for (i = 0; i < MAX_EXCEPTION_THREAD; i++) {
        if (devdrv_get_manager_info()->black_box.black_box_pid[i] == ka_task_get_current_tgid()) {
            return i;
        }
    }

    /* no same pid and inset new pid */
    for (i = 0; i < MAX_EXCEPTION_THREAD; i++) {
        if (devdrv_get_manager_info()->black_box.black_box_pid[i] == 0) {
            devdrv_get_manager_info()->black_box.black_box_pid[i] = ka_task_get_current_tgid();
            return i;
        }
    }

    /* thread num exceed the MAX_EXCEPTION_THREAD */
    return MAX_EXCEPTION_THREAD;
}

int devdrv_manager_black_box_get_exception(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_black_box_user *black_box_user = NULL;
    unsigned long flags;
    int ret, index;

    ret = devdrv_manager_container_is_in_container();
    if (ret) {
        devdrv_drv_err("not support using in container, ret(%d).\n", ret);
        return -EINVAL;
    }

    if (devdrv_get_manager_info() == NULL) {
        devdrv_drv_err("device does not exist.\n");
        return -EINVAL;
    }

    ka_task_spin_lock_irqsave(&devdrv_get_manager_info()->black_box.spinlock, flags);
    index = devdrv_manager_get_black_box_exception_index();
    if (index >= MAX_EXCEPTION_THREAD) {
        ka_task_spin_unlock_irqrestore(&devdrv_get_manager_info()->black_box.spinlock, flags);
        devdrv_drv_err("thread num exceed the support MAX_EXCEPTION_THREAD.\n");
        return -EINVAL;
    }

    if (devdrv_get_manager_info()->black_box.exception_num[index] > 0) {
        ka_task_spin_unlock_irqrestore(&devdrv_get_manager_info()->black_box.spinlock, flags);
        devdrv_drv_info("black box exception_num[%d] :%d\n", index,
                        devdrv_get_manager_info()->black_box.exception_num[index]);
        goto no_wait_black_box_sema;
    }
    ka_task_spin_unlock_irqrestore(&devdrv_get_manager_info()->black_box.spinlock, flags);

    ret = ka_task_down_interruptible(&devdrv_get_manager_info()->black_box.black_box_sema[index]);
    if (ret == -EINTR) {
        devdrv_drv_info("interrupted. ret(%d)\n", ret);
        return 0;
    }
    if (ret) {
        devdrv_drv_err("ka_task_down_interruptible fail. ret(%d)\n", ret);
        return ret;
    }

no_wait_black_box_sema:
    black_box_user = (struct devdrv_black_box_user *)dbl_kzalloc(sizeof(struct devdrv_black_box_user),
                                                                 KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (black_box_user == NULL) {
        devdrv_drv_err("Allocate memory for black box failed.\n");
        return -ENOMEM;
    }

    devdrv_host_black_box_get_exception(black_box_user, index);
    ret = copy_to_user_safe((void *)((uintptr_t)arg), black_box_user, sizeof(struct devdrv_black_box_user));
    if (ret) {
        devdrv_drv_err("copy_to_user_safe fail.\n");
        dbl_kfree(black_box_user);
        black_box_user = NULL;
        return ret;
    }

    dbl_kfree(black_box_user);
    black_box_user = NULL;
    return 0;
}

STATIC int devdrv_manager_check_black_box_info(struct devdrv_black_box_user *black_box_user, unsigned int check_size)
{
    if (black_box_user->devid >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Invalid device id. (dev_id=%u)\n", black_box_user->devid);
        return -EINVAL;
    }

    if (!devdrv_manager_is_pf_device(black_box_user->devid)) {
        return -EOPNOTSUPP;
    }

    if (black_box_user->dst_buffer == NULL) {
        devdrv_drv_err("Invalid buffer. (dev_id=%u)\n", black_box_user->devid);
        return -EFAULT;
    }

    if ((black_box_user->size <= 0) || (black_box_user->size > check_size)) {
        devdrv_drv_err("Invalid size. (dev_id=%u)\n", black_box_user->devid);
        return -EINVAL;
    }

    return 0;
}

int devdrv_manager_device_memory_dump(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_black_box_user *black_box_user = NULL;
    struct devdrv_info *dev_info = NULL;
    ka_dma_addr_t host_addr_dma = 0;
    u64 align_size = 0;
    void *buffer = NULL;
    int dev_id;
    int ret;

    if (devdrv_manager_container_is_in_container()) {
        devdrv_drv_err("is not supported in container.\n");
        return -EINVAL;
    }

    black_box_user = (struct devdrv_black_box_user *)dbl_kzalloc(sizeof(struct devdrv_black_box_user),
                                                                 KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (black_box_user == NULL) {
        devdrv_drv_err("Allocate memory for black box failed.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(black_box_user, (void *)((uintptr_t)arg), sizeof(struct devdrv_black_box_user));
    if (ret) {
        devdrv_drv_err("copy_from_user_safe fail, ret(%d).\n", ret);
        goto free_black_box_exit;
    }

    ret = devdrv_manager_check_black_box_info(black_box_user, DEVDRV_MAX_MEMORY_DUMP_SIZE);
    if (ret != 0) {
        devdrv_drv_ex_notsupport_err(ret, "Check black box parameter fail. (dev_id=%u)\n", black_box_user->devid);
        goto free_black_box_exit;
    }

    dev_info = devdrv_get_devdrv_info_array(black_box_user->devid);
    if (dev_info == NULL) {
        devdrv_drv_err("no device(%u).\n", black_box_user->devid);
        ret = -ENODEV;
        goto free_black_box_exit;
    }

    if ((black_box_user->addr_offset >= dev_info->dump_ddr_size) ||
        (black_box_user->addr_offset + black_box_user->size > dev_info->dump_ddr_size)) {
        devdrv_drv_err("invalid phy offset addr. dev_id(%u), size(%u), info size(%u)\n", black_box_user->devid,
                       black_box_user->size, dev_info->dump_ddr_size);
        ret = -EFAULT;
        goto free_black_box_exit;
    }

    ret = uda_dev_get_remote_udevid(black_box_user->devid, &dev_id);
    if (ret != 0 || dev_id < 0 || dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("get device(%u) index(%d) fail, (ret=%d).\n", black_box_user->devid, dev_id, ret);
        ret = -EINVAL;
        goto free_black_box_exit;
    }

    black_box_user->addr_offset += dev_info->dump_ddr_dma_addr;

    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_warn("dev %d has been reset\n", dev_info->dev_id);
        ret = -EINVAL;
        goto free_black_box_exit;
    }

    ret = align_to_4k(black_box_user->size, &align_size);
    if (ret != 0) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_err("Align size to 4k failed. (dev_id=%u)\n", dev_info->dev_id);
        ret = -EINVAL;
        goto free_black_box_exit;
    }

    buffer = hal_kernel_devdrv_dma_alloc_coherent(dev_info->dev, align_size, &host_addr_dma,
                                                  KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buffer == NULL) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_err("dma_alloc_coherent fail devid(%u), size(%d).\n", black_box_user->devid, black_box_user->size);
        ret = -ENOMEM;
        goto free_black_box_exit;
    }
    devdrv_drv_debug("devid: %u, len: %d.\n", black_box_user->devid, black_box_user->size);

    ret = hal_kernel_devdrv_dma_sync_copy(dev_info->pci_dev_id, DEVDRV_DMA_DATA_COMMON,
                                          (u64)black_box_user->addr_offset, (u64)host_addr_dma, black_box_user->size,
                                          DEVDRV_DMA_DEVICE_TO_HOST);
    if (ret) {
        devdrv_drv_err("hal_kernel_devdrv_dma_sync_copy fail, ret(%d). dev_id(%u)\n", ret, black_box_user->devid);
        ret = -1;
        goto free_alloc;
    }

    ret = copy_to_user_safe(black_box_user->dst_buffer, buffer, black_box_user->size);
    if (ret) {
        devdrv_drv_err("copy_to_user_safe fail, ret(%d). dev_id(%u)\n", ret, black_box_user->devid);
        ret = -1;
        goto free_alloc;
    }

free_alloc:
    ka_base_atomic_dec(&dev_info->occupy_ref);
    hal_kernel_devdrv_dma_free_coherent(dev_info->dev, align_size, buffer, host_addr_dma);
    buffer = NULL;
free_black_box_exit:
    dbl_kfree(black_box_user);
    black_box_user = NULL;
    return ret;
}

int devdrv_manager_device_vmcore_dump(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
#ifdef CFG_FEATURE_BBOX_KDUMP
    struct devdrv_black_box_user *black_box_user = NULL;
    struct devdrv_info *dev_info = NULL;
    ka_dma_addr_t host_addr_dma = 0;
    void *buff = NULL;
    int ret;
    u64 align_size = 0;
    struct devdrv_dma_node dma_info = {0};
    u64 vmcore_bar_addr;
    u64 *vmcore_addr = NULL;
    size_t vmcore_bar_size;

    ret = devdrv_manager_check_permission();
    if (ret != 0) {
        return ret;
    }

    black_box_user = (struct devdrv_black_box_user *)dbl_kzalloc(sizeof(struct devdrv_black_box_user),
                                                                 KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (black_box_user == NULL) {
        devdrv_drv_err("Allocate memory for black box failed.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(black_box_user, (void *)((uintptr_t)arg), sizeof(struct devdrv_black_box_user));
    if (ret != 0) {
        devdrv_drv_err("copy_from_user_safe failed. (ret=%d)\n", ret);
        goto free_black_box_exit;
    }

    if (!devdrv_manager_is_pf_device(black_box_user->devid)) {
        ret = -EOPNOTSUPP;
        goto free_black_box_exit;
    }

    if ((black_box_user->size == 0) || (black_box_user->size > DEVDRV_VMCORE_MAX_SIZE)) {
        devdrv_drv_err("Invalid size. (dev_id=%u; size=%u)\n", black_box_user->devid, black_box_user->size);
        ret = -EINVAL;
        goto free_black_box_exit;
    }

    dev_info = devdrv_get_devdrv_info_array(black_box_user->devid);
    if (dev_info == NULL) {
        devdrv_drv_err("Device is not initialized. (dev_id=%u)\n", black_box_user->devid);
        ret = -ENODEV;
        goto free_black_box_exit;
    }

    ret = align_to_4k(black_box_user->size, &align_size);
    if (ret != 0) {
        devdrv_drv_err("Align size to 4k failed. (dev_id=%u)\n", dev_info->dev_id);
        ret = -EINVAL;
        goto free_black_box_exit;
    }

    buff = hal_kernel_devdrv_dma_alloc_coherent(dev_info->dev, align_size, &host_addr_dma,
                                                KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buff == NULL) {
        devdrv_drv_err("dma_alloc_coherent failed. (devid=%u; size=%u).\n", black_box_user->devid,
                       black_box_user->size);
        ret = -ENOMEM;
        goto free_black_box_exit;
    }

    devdrv_drv_debug("Black box info. (devid=%u; len=%d)\n", black_box_user->devid, black_box_user->size);

    ret = devdrv_get_addr_info(black_box_user->devid, DEVDRV_ADDR_BBOX_BASE, 0, &vmcore_bar_addr, &vmcore_bar_size);
    if (ret) {
        devdrv_drv_err("get bar_addr failed (devid=%u; ret=%d)\n", black_box_user->devid, ret);
        goto free_alloc;
    }
    vmcore_bar_addr += BBOX_AP_BIAS; /* Add the size of (PCIe loop + RDR CONTROL AREA) offset */
    vmcore_addr = ka_mm_ioremap(vmcore_bar_addr, sizeof(u64));
    if (vmcore_addr == NULL) {
        devdrv_drv_err("ka_mm_ioremap failed (dev_id=%u)\n", black_box_user->devid);
        goto free_alloc;
    }

    /* Shifting 24 bits is used for encryption to avoid direct exposure of kernel-addr information. */
    dma_info.src_addr = ((u64)(*vmcore_addr) << BBOX_VMCORE_MASK_OFFSET) + black_box_user->addr_offset;
    if (KA_U64_MAX - black_box_user->addr_offset <= ((u64)(*vmcore_addr) << BBOX_VMCORE_MASK_OFFSET)) {
        devdrv_drv_err("Source address is out of range. (dev_id=%u; offset=%llu)\n", black_box_user->devid,
                       black_box_user->addr_offset);
        ka_mm_iounmap(vmcore_addr);
        ret = -EINVAL;
        goto free_alloc;
    }
    ka_mm_iounmap(vmcore_addr);
    dma_info.dst_addr = (u64)host_addr_dma;
    dma_info.size = black_box_user->size;
    dma_info.direction = DEVDRV_DMA_DEVICE_TO_HOST;
    dma_info.loc_passid = 0;

    ret = hal_kernel_devdrv_dma_sync_link_copy_extend(black_box_user->devid, DEVDRV_DMA_DATA_TRAFFIC,
                                                      DEVDRV_DMA_WAIT_INTR, &dma_info, 1);
    if (ret != 0) {
        devdrv_drv_err("hal_kernel_devdrv_dma_sync_link_copy_extend fail. (ret=%d; devid=%u)\n", ret,
                       black_box_user->devid);
        ret = -ENOMEM;
        goto free_alloc;
    }

    ret = copy_to_user_safe(black_box_user->dst_buffer, buff, black_box_user->size);
    if (ret) {
        devdrv_drv_err("copy_to_user_safe fail. (ret=%d; devid=%u)\n", ret, black_box_user->devid);
        ret = -ENOMEM;
        goto free_alloc;
    }

free_alloc:
    hal_kernel_devdrv_dma_free_coherent(dev_info->dev, align_size, buff, host_addr_dma);
    buff = NULL;
free_black_box_exit:
    dbl_kfree(black_box_user);
    black_box_user = NULL;
    return ret;
#else
    (void)filep;
    (void)cmd;
    (void)arg;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

int devdrv_manager_tslog_dump_process(struct devdrv_black_box_user *black_box_user, void **buffer)
{
    int ret = 0;
    u64 align_size = 0;
    ka_dma_addr_t host_addr_dma = 0;
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_get_devdrv_info_array(black_box_user->devid);
    if (dev_info == NULL) {
        devdrv_drv_err("Device is nonexistent. (device id=%u)\n", black_box_user->devid);
        return -ENODEV;
    }

    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        devdrv_drv_warn("Device has been reset. (device id=%u)\n", dev_info->dev_id);
        ret = -ENODEV;
        goto FLAG_DEC;
    }

    ret = align_to_4k(black_box_user->size, &align_size);
    if (ret != 0) {
        devdrv_drv_err("Align size to 4k failed. (dev_id=%u)\n", dev_info->dev_id);
        ret = -EINVAL;
        goto FLAG_DEC;
    }

    *buffer = hal_kernel_devdrv_dma_alloc_coherent(dev_info->dev, align_size, &host_addr_dma,
                                                   KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (*buffer == NULL) {
        devdrv_drv_err("Dma alloc coherent failed. (device id=%u; size=%d)\n", black_box_user->devid,
                       black_box_user->size);
        ret = -ENOMEM;
        goto FLAG_DEC;
    }

    ret = hal_kernel_devdrv_dma_sync_copy(dev_info->pci_dev_id, DEVDRV_DMA_DATA_COMMON,
                                          (u64)black_box_user->addr_offset, (u64)host_addr_dma, black_box_user->size,
                                          DEVDRV_DMA_DEVICE_TO_HOST);
    if (ret) {
        devdrv_drv_err("Dma sync copy failed. (ret=%d; device id=%u)\n", ret, black_box_user->devid);
        goto DMA_FREE;
    }

    ret = copy_to_user_safe(black_box_user->dst_buffer, *buffer, black_box_user->size);
    if (ret) {
        devdrv_drv_err("Copy to user failed. (ret=%d; device id=%u)\n", ret, black_box_user->devid);
    }

DMA_FREE:
    hal_kernel_devdrv_dma_free_coherent(dev_info->dev, align_size, *buffer, host_addr_dma);
    *buffer = NULL;
FLAG_DEC:
    ka_base_atomic_dec(&dev_info->occupy_ref);
    return ret;
}

int devdrv_manager_tslog_dump(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    int ret;
    void *buffer = NULL;
    struct devdrv_ts_log *ts_log = NULL;
    struct devdrv_black_box_user *black_box_user = NULL;

    if (devdrv_manager_container_is_in_container()) {
        return -EOPNOTSUPP;
    }

    black_box_user = (struct devdrv_black_box_user *)dbl_kzalloc(sizeof(struct devdrv_black_box_user),
                                                                 KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (black_box_user == NULL) {
        devdrv_drv_err("Allocate memory for black box failed.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(black_box_user, (void *)((uintptr_t)arg), sizeof(struct devdrv_black_box_user));
    if (ret) {
        devdrv_drv_err("Copy from user failed. (ret=%d).\n", ret);
        goto FREE_BBOX_EXIT;
    }

    if (!devdrv_manager_is_pf_device(black_box_user->devid)) {
        ret = -EOPNOTSUPP;
        goto FREE_BBOX_EXIT;
    }

    ka_task_mutex_lock(&g_devdrv_tslog_mutex);
    ts_log = devdrv_get_tslog_info(black_box_user->devid);
    if (ts_log == NULL || ts_log->mem_size == 0) {
        ka_task_mutex_unlock(&g_devdrv_tslog_mutex);
        devdrv_drv_err("Ts log dma addr info is not refreshed by device yet. (device id=%u).\n", black_box_user->devid);
        ret = -ENODEV;
        goto FREE_BBOX_EXIT;
    }

    if ((ts_log->mem_size > DEVMNG_TSLOG_MAX_SIZE) || (black_box_user->addr_offset >= ts_log->mem_size) ||
        (black_box_user->size == 0) || (black_box_user->addr_offset + black_box_user->size > ts_log->mem_size)) {
        ka_task_mutex_unlock(&g_devdrv_tslog_mutex);
        devdrv_drv_err("Invalid user input parameter. (dev_id=%u; addr offset=%llu; size=%u; info size=%u)\n",
                       black_box_user->devid, black_box_user->addr_offset, black_box_user->size, ts_log->mem_size);
        ret = -EFAULT;
        goto FREE_BBOX_EXIT;
    }

    black_box_user->addr_offset += ts_log->dma_addr;
    ka_task_mutex_unlock(&g_devdrv_tslog_mutex);

    ret = devdrv_manager_tslog_dump_process(black_box_user, &buffer);
    if (ret) {
        devdrv_drv_ex_notsupport_err(ret, "Ts log dump failed. (ret=%d; device id=%u)\n", ret, black_box_user->devid);
    }

FREE_BBOX_EXIT:
    dbl_kfree(black_box_user);
    black_box_user = NULL;
    return ret;
}

int devdrv_manager_check_capability(u32 dev_id, devdrv_capability_type type)
{
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if (dev_info == NULL) {
        devdrv_drv_err("dev_id(%u) get device info failed.\n", dev_id);
        return -EINVAL;
    }

    return (dev_info->capability & (unsigned int)(1U << (unsigned int)type)) ? 0 : -EOPNOTSUPP;
}

int devdrv_manager_reg_ddr_read(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_black_box_user *black_box_user = NULL;
    struct devdrv_info *dev_info = NULL;
    ka_dma_addr_t host_addr_dma = 0;
    void *buffer = NULL;
    u64 align_size = 0;
    int dev_id;
    int ret;

    if (devdrv_manager_container_is_in_container()) {
        devdrv_drv_err("Not support using in container.\n");
        return -EINVAL;
    }

    black_box_user = (struct devdrv_black_box_user *)dbl_kzalloc(sizeof(struct devdrv_black_box_user),
                                                                 KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (black_box_user == NULL) {
        devdrv_drv_err("Allocate memory for black box failed.\n");
        return -ENOMEM;
    }

    ret = copy_from_user_safe(black_box_user, (void *)((uintptr_t)arg), sizeof(struct devdrv_black_box_user));
    if (ret) {
        devdrv_drv_err("copy_from_user_safe fail. (ret=%d)\n", ret);
        goto free_black_box_exit;
    }

    ret = devdrv_manager_check_black_box_info(black_box_user, DEVDRV_MAX_REG_DDR_READ_SIZE);
    if (ret != 0) {
        devdrv_drv_ex_notsupport_err(ret, "Check black box parameter fail. (dev_id=%u)\n", black_box_user->devid);
        goto free_black_box_exit;
    }

#ifndef CFG_FEATURE_CHIP_REG_FORCED_EXPORT
    ret = devdrv_manager_check_capability(black_box_user->devid, DEVDRV_CAP_IMU_REG_EXPORT);
    if (ret) {
        devdrv_drv_ex_notsupport_err(ret, "Do not support read reg ddr. (dev_id=%u)\n", black_box_user->devid);
        ret = -EINVAL;
        goto free_black_box_exit;
    }
#endif

    dev_info = devdrv_get_devdrv_info_array(black_box_user->devid);
    if (dev_info == NULL) {
        devdrv_drv_err("No device. (dev_id=%u)\n", black_box_user->devid);
        ret = -ENODEV;
        goto free_black_box_exit;
    }

    if ((black_box_user->addr_offset >= dev_info->reg_ddr_size) ||
        (black_box_user->addr_offset + black_box_user->size > dev_info->reg_ddr_size)) {
        devdrv_drv_err("Invalid phy offset addr. (dev_id=%u; size=%u; info_size=%u)\n", black_box_user->devid,
                       black_box_user->size, dev_info->reg_ddr_size);
        ret = -EFAULT;
        goto free_black_box_exit;
    }

    ret = uda_dev_get_remote_udevid(black_box_user->devid, &dev_id);
    if (ret != 0 || dev_id < 0 || dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Get device index failed. (dev_id=%u; index=%d)\n", black_box_user->devid, dev_id);
        ret = -EINVAL;
        goto free_black_box_exit;
    }

    black_box_user->addr_offset += dev_info->reg_ddr_dma_addr;

    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_warn("Device has been reset. (dev_id=%d)\n", dev_info->dev_id);
        ret = -EINVAL;
        goto free_black_box_exit;
    }

    ret = align_to_4k(black_box_user->size, &align_size);
    if (ret != 0) {
        devdrv_drv_err("Align size to 4k failed. (dev_id=%u)\n", dev_info->dev_id);
        ret = -EINVAL;
        goto free_black_box_exit;
    }

    buffer = hal_kernel_devdrv_dma_alloc_coherent(dev_info->dev, align_size, &host_addr_dma,
                                                  KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (buffer == NULL) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        devdrv_drv_err("dma_alloc_coherent fail. (dev_id=%u; size=%d)\n", black_box_user->devid, black_box_user->size);
        ret = -ENOMEM;
        goto free_black_box_exit;
    }
    devdrv_drv_debug("devid: %u, len: %d.\n", black_box_user->devid, black_box_user->size);

    ret = hal_kernel_devdrv_dma_sync_copy(dev_info->pci_dev_id, DEVDRV_DMA_DATA_COMMON,
                                          (u64)black_box_user->addr_offset, (u64)host_addr_dma, black_box_user->size,
                                          DEVDRV_DMA_DEVICE_TO_HOST);
    if (ret != 0) {
        devdrv_drv_err("hal_kernel_devdrv_dma_sync_copy fail. (ret=%d; dev_id=%u)\n", ret, black_box_user->devid);
        ret = -1;
        goto free_alloc;
    }

    ret = copy_to_user_safe(black_box_user->dst_buffer, buffer, black_box_user->size);
    if (ret != 0) {
        devdrv_drv_err("copy_to_user_safe fail. (ret=%d; dev_id=%u)\n", ret, black_box_user->devid);
        ret = -1;
        goto free_alloc;
    }

free_alloc:
    ka_base_atomic_dec(&dev_info->occupy_ref);
    hal_kernel_devdrv_dma_free_coherent(dev_info->dev, align_size, buffer, host_addr_dma);
    buffer = NULL;
free_black_box_exit:
    dbl_kfree(black_box_user);
    black_box_user = NULL;
    return ret;
}

/* This interface does not support using in containers */
int devdrv_manager_device_reset_inform(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    ka_timespec_t stamp;
    u32 devid;
    int virt_id;
    u32 vfid = 0;
    int ret;

    ret = copy_from_user_safe(&devid, (void *)((uintptr_t)arg), sizeof(u32));
    if (ret) {
        devdrv_drv_err("copy_from_user_safe failed. (ret=%d)\n", ret);
        return ret;
    }

    virt_id = devid;
    if (devdrv_manager_container_logical_id_to_physical_id(virt_id, &devid, &vfid)) {
        devdrv_drv_err("Can't transform logical id. (virtual_id=%u)\n", virt_id);
        return -EINVAL;
    }

    if (vfid != 0) {
        return -EOPNOTSUPP;
    }

#ifndef CFG_FEATURE_SRIOV
    ret = devdrv_manager_check_permission();
    if (ret != 0) {
        devdrv_drv_err("Failed to invoke devdrv_manager_check_permission. (ret=%d)\n", ret);
#ifdef CFG_FEATURE_ERRORCODE_ON_NEW_CHIPS
        return ret;
#else
        return -EINVAL;
#endif
    }
#endif
    stamp = current_kernel_time();

    ret = devdrv_host_black_box_add_exception(devid, DEVDRV_BB_DEVICE_RESET_INFORM, stamp, NULL);
    if (ret) {
        devdrv_drv_err("devdrv_host_black_box_add_exception failed, ret(%d). dev_id(%u)\n", ret, devid);
        return ret;
    }

    return 0;
}

void devmng_devlog_addr_uninit(void)
{
    int devid;
    int log_type;

    ka_task_mutex_lock(&g_devdrv_tslog_mutex);
    for (devid = 0; devid < ASCEND_DEV_MAX_NUM; devid++) {
        if (g_devdrv_ts_log_array[devid] != NULL) {
            dbl_kfree(g_devdrv_ts_log_array[devid]);
            g_devdrv_ts_log_array[devid] = NULL;
        }
    }
    ka_task_mutex_unlock(&g_devdrv_tslog_mutex);

    ka_task_mutex_lock(&g_devdrv_devlog_mutex);
    for (devid = 0; devid < ASCEND_PDEV_MAX_NUM; devid++) {
        for (log_type = 0; log_type < DEVDRV_LOG_DUMP_TYPE_MAX; log_type++) {
            if (g_devdrv_dev_log_array[log_type][devid] != NULL) {
                ka_mm_kfree(g_devdrv_dev_log_array[log_type][devid]);
                g_devdrv_dev_log_array[log_type][devid] = NULL;
            }
        }
    }
    ka_task_mutex_unlock(&g_devdrv_devlog_mutex);
}

int devmng_devlog_addr_init(void)
{
    int devid;
    int log_type;

    ka_task_mutex_init(&g_devdrv_devlog_mutex);
    ka_task_mutex_init(&g_devdrv_tslog_mutex);

    for (devid = 0; devid < ASCEND_DEV_MAX_NUM; devid++) {
        g_devdrv_ts_log_array[devid] = dbl_kzalloc(sizeof(struct devdrv_ts_log), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (g_devdrv_ts_log_array[devid] == NULL) {
            devmng_devlog_addr_uninit();
            return -ENOMEM;
        }
    }

    for (devid = 0; devid < ASCEND_PDEV_MAX_NUM; devid++) {
        for (log_type = 0; log_type < DEVDRV_LOG_DUMP_TYPE_MAX; log_type++) {
            g_devdrv_dev_log_array[log_type][devid] = ka_mm_kzalloc(sizeof(struct devdrv_dev_log),
                                                                    KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
            if (g_devdrv_dev_log_array[log_type][devid] == NULL) {
                devmng_devlog_addr_uninit();
                return -ENOMEM;
            }
        }
    }
    return 0;
}
