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

#include "devdrv_manager.h"
#include "devdrv_manager_common.h"
#include "pbl_mem_alloc_interface.h"
#include "davinci_interface.h"
#include "adapter_api.h"
#include "securec.h"
#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_common_pub.h"

int devdrv_manager_ipc_notify_init(struct devdrv_manager_context *dev_manager_context)
{
    struct ipc_notify_info *ipc_notify_info = NULL;
    size_t ipc_size = sizeof(struct ipc_notify_info);

    ipc_notify_info = (struct ipc_notify_info *)dbl_kzalloc(ipc_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (ipc_notify_info == NULL) {
        devdrv_drv_err("vmalloc ipc_notify_info is NULL.\n");
        return -ENOMEM;
    }

    ipc_notify_info->create_fd_num = 0;
    ipc_notify_info->open_fd_num = 0;
    KA_INIT_LIST_HEAD(&ipc_notify_info->create_list_head);
    KA_INIT_LIST_HEAD(&ipc_notify_info->open_list_head);
    ka_task_mutex_init(&ipc_notify_info->info_mutex);

    dev_manager_context->ipc_notify_info = ipc_notify_info;

    return 0;
}

void devdrv_manager_ipc_notify_uninit(struct devdrv_manager_context *dev_manager_context)
{
    struct ipc_notify_info *ipc_notify_info = NULL;

    ipc_notify_info = dev_manager_context->ipc_notify_info;
    if (ipc_notify_info == NULL) {
        return;
    }

    ka_task_mutex_destroy(&ipc_notify_info->info_mutex);
    dbl_kfree(ipc_notify_info);
    ipc_notify_info = NULL;
    dev_manager_context->ipc_notify_info = NULL;
}

void devdrv_manager_resource_recycle(struct devdrv_manager_context *dev_manager_context)
{
    struct ipc_notify_info *ipc_notify_info = NULL;
    u32 ipc_notify_create_num = 0;
    u32 ipc_notify_open_num = 0;

    ipc_notify_info = dev_manager_context->ipc_notify_info;
    if (ipc_notify_info != NULL) {
        ipc_notify_open_num = ipc_notify_info->open_fd_num;
        ipc_notify_create_num = ipc_notify_info->create_fd_num;
    }

    if ((ipc_notify_open_num > 0) || (ipc_notify_create_num > 0)) {
        devdrv_drv_info("ipc resource leak, "
                        "ipc_notify_create_num = %u, "
                        "ipc_notify_open_num = %u\n",
                        ipc_notify_create_num, ipc_notify_open_num);
#ifndef CFG_FEATURE_REFACTOR
        devdrv_manager_ops_sem_down_read();
        if (devdrv_manager_get_drv_ops()->ipc_notify_release_recycle != NULL) {
            devdrv_manager_get_drv_ops()->ipc_notify_release_recycle(dev_manager_context);
        }
        devdrv_manager_ops_sem_up_read();
#endif
    }

    adap_flush_p2p(dev_manager_context->pid);
    devdrv_manager_context_uninit(dev_manager_context);
}

#ifndef CFG_FEATURE_REFACTOR
STATIC inline int devdrv_manager_drv_ops_check(void)
{
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    if (devdrv_manager_get_drv_ops()->ipc_notify_create == NULL ||
        devdrv_manager_get_drv_ops()->ipc_notify_open == NULL ||
        devdrv_manager_get_drv_ops()->ipc_notify_close == NULL ||
        devdrv_manager_get_drv_ops()->ipc_notify_destroy == NULL ||
        devdrv_manager_get_drv_ops()->ipc_notify_set_pid == NULL ||
        devdrv_manager_get_drv_ops()->ipc_notify_get_info == NULL ||
        devdrv_manager_get_drv_ops()->ipc_notify_set_attr == NULL ||
        devdrv_manager_get_drv_ops()->ipc_notify_get_attr == NULL ||
        devdrv_manager_get_drv_ops()->ipc_notify_record == NULL) {
        return -EINVAL;
    }
#endif
    return 0;
}

int devdrv_manager_ipc_notify_ioctl(ka_file_t *filep, unsigned int cmd, unsigned long arg)
{
    struct devdrv_notify_ioctl_info notify_ioctl_info;
    struct devdrv_manager_context *context = NULL;
    int ret;

    if (filep == NULL) {
        devdrv_drv_err("filep is NULL.\n");
        return -EINVAL;
    }

    if (filep->private_data == NULL) {
        devdrv_drv_err("filep private_data is NULL.\n");
        return -EINVAL;
    }

    context = filep->private_data;
    if (context->ipc_notify_info == NULL) {
        devdrv_drv_err("context->ipc_notify_info is NULL.\n");
        return -ENODEV;
    }

    ret = copy_from_user_safe(&notify_ioctl_info, (void *)((uintptr_t)arg), sizeof(struct devdrv_notify_ioctl_info));
    if (ret) {
        devdrv_drv_err("copy from user failed, ret(%d).\n", ret);
        return -EFAULT;
    }
    notify_ioctl_info.name[DEVDRV_IPC_NAME_SIZE - 1] = '\0';

    devdrv_manager_ops_sem_down_read();
    if (devdrv_manager_drv_ops_check()) {
        devdrv_manager_ops_sem_up_read();
        devdrv_drv_err("ipc notify function not init\n");
        return -EINVAL;
    }

    switch (cmd) {
        case DEVDRV_MANAGER_IPC_NOTIFY_CREATE:
            ret = devdrv_manager_get_drv_ops()->ipc_notify_create((void *)context, arg,
                                                                  (void *)(uintptr_t)&notify_ioctl_info);
            break;
        case DEVDRV_MANAGER_IPC_NOTIFY_OPEN:
            ret = devdrv_manager_get_drv_ops()->ipc_notify_open((void *)context, arg,
                                                                (void *)(uintptr_t)&notify_ioctl_info);
            break;
        case DEVDRV_MANAGER_IPC_NOTIFY_CLOSE:
            ret = devdrv_manager_get_drv_ops()->ipc_notify_close((void *)context,
                                                                 (void *)(uintptr_t)&notify_ioctl_info);
            break;
        case DEVDRV_MANAGER_IPC_NOTIFY_DESTROY:
            ret = devdrv_manager_get_drv_ops()->ipc_notify_destroy((void *)context,
                                                                   (void *)(uintptr_t)&notify_ioctl_info);
            break;
        case DEVDRV_MANAGER_IPC_NOTIFY_SET_PID:
            ret = devdrv_manager_get_drv_ops()->ipc_notify_set_pid((void *)context,
                                                                   (void *)(uintptr_t)&notify_ioctl_info);
            break;
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
        case DEVDRV_MANAGER_IPC_NOTIFY_RECORD:
            ret = devdrv_manager_get_drv_ops()->ipc_notify_record((void *)context,
                                                                  (void *)(uintptr_t)&notify_ioctl_info);
            break;
        case DEVDRV_MANAGER_IPC_NOTIFY_SET_ATTR:
            ret = devdrv_manager_get_drv_ops()->ipc_notify_set_attr((void *)context,
                                                                    (void *)(uintptr_t)&notify_ioctl_info);
            break;
        case DEVDRV_MANAGER_IPC_NOTIFY_GET_INFO:
            ret = devdrv_manager_get_drv_ops()->ipc_notify_get_info((void *)context, arg,
                                                                    (void *)(uintptr_t)&notify_ioctl_info);
            break;
        case DEVDRV_MANAGER_IPC_NOTIFY_GET_ATTR:
            ret = devdrv_manager_get_drv_ops()->ipc_notify_get_attr((void *)context, arg,
                                                                    (void *)(uintptr_t)&notify_ioctl_info);
            break;
#endif
        default:
            devdrv_drv_err("invalid cmd, cmd = %d\n", _KA_IOC_NR(cmd));
            ret = -EFAULT;
            break;
    };

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    if ((ret == 0) && ((cmd == DEVDRV_MANAGER_IPC_NOTIFY_CLOSE) || (cmd == DEVDRV_MANAGER_IPC_NOTIFY_DESTROY))) {
        ret = copy_to_user_safe((void *)((uintptr_t)arg), (void *)&notify_ioctl_info,
                                sizeof(struct devdrv_notify_ioctl_info));
        if (ret != 0) {
            devdrv_drv_err("copy to user failed. (cmd=%u; ret=%d).\n", cmd, ret);
        }
    }
#endif
    devdrv_manager_ops_sem_up_read();
    (void)memset_s(notify_ioctl_info.name, DEVDRV_IPC_NAME_SIZE, 0, DEVDRV_IPC_NAME_SIZE);

    return ret;
}
#endif
