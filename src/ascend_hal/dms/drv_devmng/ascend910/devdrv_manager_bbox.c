/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "securec.h"
#include "devmng_common.h"
#include "mmpa_api.h"
#include "devdrv_ioctl.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devmng_user_common.h"
#include "dms_user_common.h"
#include "ascend_hal.h"
#ifdef CFG_FEATURE_SUPPORT_DEVMNG_BBOX
#include "dms_cmd_def.h"
#endif
#include "drv_user_common.h"
#include "ascend_dev_num.h"
#include "drv_devmng_adapt.h"

#ifdef __linux
#include <sys/prctl.h>
#endif

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC mmUserBlock_t func_block = {0};

STATIC mmMutex_t devdrv_black_box_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    drvDeviceExceptionReporFunc exp_report_func;
    drvDeviceStartupNotify start_notifunc;
#ifdef __linux
    drvDeviceStateNotify state_notifunc;
#endif
} drvBlackBoxCallbackFunc_t;

STATIC drvBlackBoxCallbackFunc_t BBoxCallbackFunc = {
    .exp_report_func = NULL,
    .start_notifunc = NULL,
#ifdef __linux
    .state_notifunc = NULL,
#endif
};

STATIC mmThread devdrv_black_box_thread;
STATIC u8 devdrv_black_box_thread_work = 0;

STATIC void devdrv_black_box_exception_process(const struct devdrv_black_box_user *black_box_user,
                                               drvBlackBoxCallbackFunc_t *callback)
{
    drvDeviceExceptionReporFunc exp_rep_func = callback->exp_report_func;
    struct timespec stamp;

    stamp.tv_sec = (long)black_box_user->tv_sec;
    stamp.tv_nsec = (long)black_box_user->tv_nsec;

    if (exp_rep_func == NULL) {
        DEVDRV_DRV_DEBUG("exception callback function is not registered\n");
        return;
    }
    exp_rep_func(black_box_user->devid, black_box_user->exception_code, stamp);
}

STATIC void devdrv_black_box_dev_id_notify_process(struct devdrv_black_box_user *black_box_user,
                                                   drvBlackBoxCallbackFunc_t *callback)
{
    drvDeviceStartupNotify start_noti_func = callback->start_notifunc;

    if (start_noti_func == NULL) {
        DEVDRV_DRV_DEBUG("exception callback function is not registered\n");
        return;
    }

    start_noti_func(black_box_user->priv_data.bbox_devids.dev_num, black_box_user->priv_data.bbox_devids.devids);
}

#ifdef __linux
STATIC void devdrv_black_box_dev_state_notify_process(const struct devdrv_black_box_user *black_box_user,
                                                      drvBlackBoxCallbackFunc_t *callback)
{
    drvDeviceStateNotify state_noti_func = callback->state_notifunc;
    devdrv_state_info_t state_info = {0};
    if (state_noti_func == NULL) {
        DEVDRV_DRV_ERR("exception callback function is not registered.\n");
        return;
    }

    state_info.state = (devdrv_state_t)black_box_user->priv_data.bbox_state.state;
    state_info.devId = black_box_user->priv_data.bbox_state.devId;
    DEVDRV_DRV_INFO("state_notifunc called(devId:%d, state(%d), code(0x%x)).\n", state_info.devId, state_info.state,
                    black_box_user->exception_code);
    state_noti_func(&state_info);
}
#endif

STATIC void devdrv_black_box_notify_process(struct devdrv_black_box_user *black_box_user,
                                            drvBlackBoxCallbackFunc_t *callback)
{
    if (black_box_user->exception_code == 0) {
        return;
    }

    switch (black_box_user->exception_code) {
#ifdef __linux
        case DEVDRV_BB_DEVICE_STATE_INFORM:
            devdrv_black_box_dev_state_notify_process(black_box_user, callback);
            break;
#endif
        case DEVDRV_BB_DEVICE_ID_INFORM:
            devdrv_black_box_dev_id_notify_process(black_box_user, callback);
            break;
        default:
            devdrv_black_box_exception_process(black_box_user, callback);
            break;
    }

    return;
}

STATIC void *devdrv_black_box_notify_thread(void *data)
{
    struct devdrv_black_box_user black_box_user = {0};
    drvBlackBoxCallbackFunc_t *callback = NULL;
    int ret;
    mmIoctlBuf buf = {0};

#ifdef __linux
    (void)prctl(PR_SET_NAME, "devmng_exception_notify");
#endif

    DEVDRV_DRV_DEBUG("thread for black box started.\n");

    callback = (drvBlackBoxCallbackFunc_t *)data;
    if (callback == NULL) {
        DEVDRV_DRV_ERR("invalid callback function, thread stop.\n");
        return NULL;
    }

    while (devdrv_black_box_thread_work != 0) {
        ret = memset_s(&black_box_user, sizeof(struct devdrv_black_box_user), 0, sizeof(struct devdrv_black_box_user));
        if (ret != EOK) {
            DEVDRV_DRV_ERR("memset_s returned error: %d.\n", ret);
            return NULL;
        }

        drv_ioctl_param_init(&buf, (void *)&black_box_user, sizeof(struct devdrv_black_box_user));
        ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_BLACK_BOX_GET_EXCEPTION);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "ioctl failed. (ret=%d)\n", ret);
            break;
        }

        if (black_box_user.thread_should_stop != 0) {
            DEVDRV_DRV_ERR("thread for black box should stop.\n");
            break;
        }

        devdrv_black_box_notify_process(&black_box_user, callback);
    }

    DEVDRV_DRV_DEBUG("thread for black box stop.\n");
    return NULL;
}

#ifdef __linux
drvError_t drvDeviceStateNotifierRegister(drvDeviceStateNotify state_callback)
{
    int ret;

    if (state_callback == NULL) {
        DEVDRV_DRV_ERR("invalid input callback func.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    (void)mmMutexLock(&devdrv_black_box_mutex);
    if (BBoxCallbackFunc.state_notifunc != NULL) {
        (void)mmMutexUnLock(&devdrv_black_box_mutex);
        DEVDRV_DRV_ERR("already create thread for black box.\n");
        return DRV_ERROR_CLIENT_BUSY;
    }
    BBoxCallbackFunc.state_notifunc = state_callback;

    if (devdrv_black_box_thread_work == 1) {
        (void)mmMutexUnLock(&devdrv_black_box_mutex);
        DEVDRV_DRV_INFO("already create thread for black box.\n");
        return DRV_ERROR_NONE;
    }

    devdrv_black_box_thread_work = 1;
    func_block.procFunc = devdrv_black_box_notify_thread;
    func_block.pulArg = (void *)&BBoxCallbackFunc;

    ret = mmCreateTaskWithDetach(&devdrv_black_box_thread, &func_block);
    if (ret != 0) {
        DEVDRV_DRV_ERR("pthread_create fail.\n");
        BBoxCallbackFunc.exp_report_func = NULL;
        devdrv_black_box_thread_work = 0;
        (void)mmMutexUnLock(&devdrv_black_box_mutex);
        return DRV_ERROR_SOCKET_CREATE;
    }
    (void)mmMutexUnLock(&devdrv_black_box_mutex);
    DEVDRV_DRV_INFO("state callback registered.\n");
    return DRV_ERROR_NONE;
}
#endif

drvError_t drvDeviceStartupRegister(drvDeviceStartupNotify startup_callback)
{
    int ret;

    if (startup_callback == NULL) {
        DEVDRV_DRV_ERR("invalid input callback func.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }
#ifndef __linux
    if (devdrv_black_box_mutex == PTHREAD_MUTEX_INITIALIZER) {
        mmMutexInit(&devdrv_black_box_mutex);
    }
#endif
    (void)mmMutexLock(&devdrv_black_box_mutex);
    if (BBoxCallbackFunc.start_notifunc != NULL) {
        DEVDRV_DRV_ERR("already create thread for black box.\n");
        (void)mmMutexUnLock(&devdrv_black_box_mutex);
        return DRV_ERROR_CLIENT_BUSY;
    }
    BBoxCallbackFunc.start_notifunc = startup_callback;

    if (devdrv_black_box_thread_work == 1) {
        DEVDRV_DRV_INFO("already create thread for black box.\n");
        (void)mmMutexUnLock(&devdrv_black_box_mutex);
        return DRV_ERROR_NONE;
    }

    devdrv_black_box_thread_work = 1;
    func_block.procFunc = devdrv_black_box_notify_thread;
    func_block.pulArg = (void *)&BBoxCallbackFunc;

    ret = mmCreateTaskWithDetach(&devdrv_black_box_thread, &func_block);
    if (ret != 0) {
        DEVDRV_DRV_ERR("pthread_create fail.\n");
        BBoxCallbackFunc.exp_report_func = NULL;
        devdrv_black_box_thread_work = 0;
        (void)mmMutexUnLock(&devdrv_black_box_mutex);
        return DRV_ERROR_SOCKET_CREATE;
    }
    (void)mmMutexUnLock(&devdrv_black_box_mutex);
    DEVDRV_DRV_INFO("Startup callback registered.\n");
    return DRV_ERROR_NONE;
}

drvError_t drvDeviceExceptionHookRegister(drvDeviceExceptionReporFunc exception_callback_func)
{
    int ret;

    if (exception_callback_func == NULL) {
        DEVDRV_DRV_ERR("Invalid input callback func.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }
#ifndef __linux
    if (devdrv_black_box_mutex == PTHREAD_MUTEX_INITIALIZER) {
        mmMutexInit(&devdrv_black_box_mutex);
    }
#endif
    (void)mmMutexLock(&devdrv_black_box_mutex);
    if (BBoxCallbackFunc.exp_report_func != NULL) {
        (void)mmMutexUnLock(&devdrv_black_box_mutex);
        DEVDRV_DRV_ERR("already create thread for black box.\n");
        return DRV_ERROR_CLIENT_BUSY;
    }

    BBoxCallbackFunc.exp_report_func = exception_callback_func;
    if (devdrv_black_box_thread_work == 1) {
        (void)mmMutexUnLock(&devdrv_black_box_mutex);
        DEVDRV_DRV_INFO("already create thread for black box.\n");
        return DRV_ERROR_NONE;
    }

    devdrv_black_box_thread_work = 1;
    func_block.procFunc = devdrv_black_box_notify_thread;
    func_block.pulArg = (void *)&BBoxCallbackFunc;

    ret = mmCreateTaskWithDetach(&devdrv_black_box_thread, &func_block);
    if (ret != 0) {
        DEVDRV_DRV_ERR("pthread_create fail.\n");
        devdrv_black_box_thread_work = 0;
        BBoxCallbackFunc.exp_report_func = NULL;
        (void)mmMutexUnLock(&devdrv_black_box_mutex);
        return DRV_ERROR_SOCKET_CREATE;
    }
    (void)mmMutexUnLock(&devdrv_black_box_mutex);

    DEVDRV_DRV_INFO("Exception callback registered.\n");
    return DRV_ERROR_NONE;
}

drvError_t drv_get_klog_info(uint32_t devId, int32_t info_type, void *path, unsigned int *p_size)
{
#ifdef CFG_FEATURE_QUERY_LOG_INFO
    int ret;

    if (info_type == INFO_TYPE_HOST_KERN_LOG) {
        ret = drvGetKlogBuf(devId, (const char *)path, p_size);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get log. (dev_id=%u; ret=%d)\n", devId, ret);
            return ret;
        }
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
#else
    (void)devId;
    (void)info_type;
    (void)path;
    (void)p_size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

#ifdef CFG_FEATURE_SUPPORT_DEVMNG_BBOX
drvError_t drv_device_memory_dump(uint32_t devId, uint64_t bbox_addr_offset, uint32_t size, void *buffer)
{
    struct devdrv_black_box_user black_box_user = {0};
    int ret;
    mmIoctlBuf buf = {0};

    if (buffer == NULL) {
        DEVDRV_DRV_ERR("buffer is NULL. (devid=%u)\n", devId);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (devId >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid = %u.\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    black_box_user.devid = devId;
    black_box_user.size = size;
    black_box_user.addr_offset = bbox_addr_offset;
    black_box_user.dst_buffer = buffer;

    drv_ioctl_param_init(&buf, (void *)&black_box_user, sizeof(struct devdrv_black_box_user));
    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_DEVICE_MEMORY_DUMP);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "ioctl failed ret = %d.\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
#endif

drvError_t drv_vmcore_dump(uint32_t devId, uint64_t bbox_addr_offset, uint32_t size, void *buffer)
{
#ifdef CFG_FEATURE_BBOX_KDUMP
    struct devdrv_black_box_user black_box_user = {0};
    int ret;
    mmIoctlBuf buf = {0};

    if (buffer == NULL) {
        DEVDRV_DRV_ERR("buffer is NULL. (devid=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    if (devId >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("Invalid device id. (dev_id=%u; max_dev_num=%u)\n", devId, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    black_box_user.devid = devId;
    black_box_user.size = size;
    black_box_user.addr_offset = bbox_addr_offset;
    black_box_user.dst_buffer = buffer;

    drv_ioctl_param_init(&buf, (void *)&black_box_user, sizeof(struct devdrv_black_box_user));
    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_DEVICE_VMCORE_DUMP);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
#else
    (void)devId;
    (void)bbox_addr_offset;
    (void)size;
    (void)buffer;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t drv_ts_log_dump(uint32_t devid, uint64_t bbox_addr_offset, uint32_t size, void *buffer)
{
    int ret;
    mmIoctlBuf buf = {0};
    struct devdrv_black_box_user black_box_user = {0};

    if (buffer == NULL) {
        DEVDRV_DRV_ERR("Buffer is NULL. (device id=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (devid >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("Invalid device id. (device id=%u; max_dev_num=%u)\n", devid, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    black_box_user.devid = devid;
    black_box_user.size = size;
    black_box_user.addr_offset = bbox_addr_offset;
    black_box_user.dst_buffer = buffer;

    drv_ioctl_param_init(&buf, (void *)&black_box_user, sizeof(struct devdrv_black_box_user));
    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_TS_LOG_DUMP);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (ret=%d; device id=%u).\n", ret, devid);
        return ret;
    }

    return DRV_ERROR_NONE;
}
#ifdef CFG_FEATURE_SUPPORT_DEVMNG_BBOX
int drv_dev_log_dump(uint32_t devid, uint64_t bbox_addr_offset, uint32_t size, void *buffer, uint32_t log_type)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    struct devdrv_bbox_logdump bbox_log_dump = {0};

    if (buffer == NULL) {
        DEVDRV_DRV_ERR("Buffer is NULL. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (devid >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("Invalid device id. (devid=%u, max_devid=%d).\n", devid, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    bbox_log_dump.bbox_user = (struct devdrv_black_box_user *)malloc(sizeof(struct devdrv_black_box_user));

    if (bbox_log_dump.bbox_user == NULL) {
        DEVDRV_DRV_ERR("Allocate memory for bbox_user failed. (devid=%u).\n", devid);
        return DRV_ERROR_MALLOC_FAIL;
    }

    bbox_log_dump.bbox_user->devid = devid;
    bbox_log_dump.bbox_user->size = size;
    bbox_log_dump.bbox_user->addr_offset = bbox_addr_offset;
    bbox_log_dump.bbox_user->dst_buffer = buffer;

    bbox_log_dump.log_type = log_type;

    ioarg.main_cmd = DMS_MAIN_CMD_BBOX;
    ioarg.sub_cmd = DMS_SUBCMD_GET_LOG_DUMP_INFO;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&bbox_log_dump;
    ioarg.input_len = sizeof(struct devdrv_bbox_logdump);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = errno_to_user_errno(DmsIoctl(DMS_GET_BBOX_LOG_DUMP_INFO, &ioarg));
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "drv_dev_log_dump failed. (devid=%u, ret=%d)", devid, ret);
    }

    free(bbox_log_dump.bbox_user);
    bbox_log_dump.bbox_user = NULL;
    return ret;
}
#endif
drvError_t drv_reg_ddr_read(uint32_t devId, uint64_t reg_addr_offset, uint32_t size, void *buffer)
{
    struct devdrv_black_box_user black_box_user = {0};
    int ret;
    mmIoctlBuf buf = {0};

    if (buffer == NULL) {
        DEVDRV_DRV_ERR("buffer is NULL. (devid=%u)\n", devId);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (devId >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid. (devid=%u, max=%u)\n", devId, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_INVALID_VALUE;
    }

    black_box_user.devid = devId;
    black_box_user.size = size;
    black_box_user.addr_offset = reg_addr_offset;
    black_box_user.dst_buffer = buffer;

    drv_ioctl_param_init(&buf, (void *)&black_box_user, sizeof(struct devdrv_black_box_user));
    ret = drv_common_ioctl(&buf, DEVDRV_MANAGER_REG_DDR_READ);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "ioctl failed ret = %d.\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}
