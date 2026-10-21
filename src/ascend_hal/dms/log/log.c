/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "dms/dms_misc_interface.h"
#include "dms_cmd_def.h"
#include "ascend_kernel_hal.h"
#include "dms_user_common.h"
#include "devdrv_ioctl.h"
#include "devmng_user_common.h"
#include "securec.h"
#include "ascend_dev_num.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DMS_LOG_BUF_SIZE 0x1400000
#define DMS_DIR_MAXLEN 256U
#define DMS_LOG_FILE_NAME "host_kernel.log"
struct LogInfo {
    void *buffer;
    u32 length;
    u32 buf_len;
};

static int drvKlogSaveFs(const void *buffer, u32 len, const char *logPath)
{
#ifndef DMS_UT
    char fullPath[DMS_DIR_MAXLEN] = {0};
    char *resolvedPath = NULL;
    ssize_t cnt = 0;
    int fd = -1;
    int ret;

    if (len == 0) {
        DMS_INFO("Skip saving empty log to file. (len=%u, path=%s)\n", len, logPath);
        return DRV_ERROR_NONE;
    }

    resolvedPath = realpath(logPath, NULL);
    if (resolvedPath == NULL) {
        DMS_ERR("Invalid directory for the log path. (dir=%s)\n", logPath);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = sprintf_s(fullPath, DMS_DIR_MAXLEN, "%s/%s", resolvedPath, DMS_LOG_FILE_NAME);
    if (ret < 0) {
        DMS_ERR("Format full path failed. (dir=%s, fileName=%s)\n", resolvedPath, DMS_LOG_FILE_NAME);
        free(resolvedPath);
        resolvedPath = NULL;
        return DRV_ERROR_INVALID_VALUE;
    }

    free(resolvedPath);
    resolvedPath = NULL;

    fd = open(fullPath, (O_CREAT | O_RDWR | O_TRUNC), S_IRUSR | S_IWUSR);
    if (fd < 0) {
        DMS_ERR("Open failed. (errno=%s, path=%s)\n", strerror(errno), fullPath);
        return DRV_ERROR_INVALID_VALUE;
    }

    cnt = write(fd, buffer, len);
    if (cnt < 0) {
        DMS_ERR("Write failed. (errno=%s, path=%s)\n", strerror(errno), fullPath);
        ret = DRV_ERROR_INVALID_VALUE;
        goto cleanup;
    }

    if (cnt != len) {
        DMS_WARN("Write file return bad write length. (path=%s, ret=%d, len=%u)", fullPath, ret, len);
    }

    if (fsync(fd) != 0) {
        DMS_ERR("Sync failed. (errno=%s, path=%s)\n", strerror(errno), fullPath);
        ret = DRV_ERROR_INVALID_VALUE;
        goto cleanup;
    }

    ret = DRV_ERROR_NONE;
cleanup:
    close(fd);
    fd = -1;
    return ret;
#else
    return DRV_ERROR_NONE;
#endif
}

static bool drvIsVritEnv(void)
{
#ifdef CFG_FEATURE_VDEVMNG_IN_VIRTUAL
    return DmsGetVirtFlag() != 0 ? true : false;
#else
    return false;
#endif
}

static int drvGetKlogBufParaCheck(uint32_t devId, const char *path, unsigned int *pSize)
{
    unsigned int maxLen = (unsigned int)(DMS_DIR_MAXLEN - strlen(DMS_LOG_FILE_NAME));

    if ((devId >= ASCEND_PDEV_MAX_NUM) || (path == NULL) || (pSize == NULL)) {
        DMS_ERR("Invalid parameters. (devId=%u, buf%s, p_size%s)\n", devId, path == NULL ? "=NULL" : "!=NULL",
                pSize == NULL ? "=NULL" : "!=NULL");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((*pSize >= maxLen) || (strnlen(path, (size_t)maxLen) != *pSize)) {
        DMS_ERR("Invalid path or size. (devId=%u, *pSize=%u, pathLen=%d, maxLen=%u)\n", devId, *pSize,
                strnlen(path, (size_t)maxLen), maxLen);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static int drvGetKlogIoctl(uint32_t devId, struct LogInfo *log_info)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

#ifndef DMS_UT
    if (drvIsVritEnv()) {
        ret = dmanage_common_ioctl((int)DEVDRV_MANAGER_GET_HOST_KERN_LOG, log_info);
        if (ret != 0) {
            DMS_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (dev_id=%u; ret=%d; errno=%d)\n", devId, ret, errno);
            return ret;
        }
    } else {
        ioarg.main_cmd = DMS_MAIN_CMD_LOG;
        ioarg.sub_cmd = DMS_SUBCMD_GET_LOG_INFO;
        ioarg.filter_len = 0;
        ioarg.input = log_info;
        ioarg.input_len = (unsigned int)sizeof(struct LogInfo);
        ioarg.output = &log_info->buf_len;
        ioarg.output_len = (unsigned int)sizeof(u32);
        ret = errno_to_user_errno(DmsIoctl((int)DMS_IOCTL_CMD, &ioarg));
        if (ret != 0) {
            DMS_EX_NOTSUPPORT_ERR(ret, "drvGetKlogBuf failed. (devId=%u, ret=%d)", devId, ret);
            return ret;
        }
    }
#endif

    return DRV_ERROR_NONE;
}

int drvGetKlogBuf(uint32_t devId, const char *path, unsigned int *pSize)
{
    struct LogInfo log_info = {0};
    int ret;

    ret = drvGetKlogBufParaCheck(devId, path, pSize);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    log_info.buffer = (char *)malloc(DMS_LOG_BUF_SIZE);
    if (log_info.buffer == NULL) {
#ifndef DMS_UT
        DMS_ERR("Malloc failed. (devId=%u, size=%u)\n", devId, DMS_LOG_BUF_SIZE);
        return DRV_ERROR_OUT_OF_MEMORY;
#endif
    }
    log_info.length = DMS_LOG_BUF_SIZE;

    ret = drvGetKlogIoctl(devId, &log_info);
    if (ret != DRV_ERROR_NONE) {
#ifndef DMS_UT
        DMS_EX_NOTSUPPORT_ERR(ret, "drvGetKlogIoctl failed. (devId=%u, ret=%d)", devId, ret);
        (void)free(log_info.buffer);
        return ret;
#endif
    }

    if (log_info.buf_len > DMS_LOG_BUF_SIZE) {
#ifndef DMS_UT
        DMS_ERR("Buf len is invalid. (devId=%u, buf_len=%u)\n", devId, log_info.buf_len);
        (void)free(log_info.buffer);
        return DRV_ERROR_INVALID_VALUE;
#endif
    }

    ret = drvKlogSaveFs((const char *)log_info.buffer, log_info.buf_len, path);
    if (ret != 0) {
#ifndef DMS_UT
        DMS_ERR("Save klog failed. (ret=%d)\n", ret);
        (void)free(log_info.buffer);
        return ret;
#endif
    }

    (void)free(log_info.buffer);
    return DRV_ERROR_NONE;
}

STATIC int g_log_fd = -1;
static pthread_mutex_t g_log_mtx = PTHREAD_MUTEX_INITIALIZER;
#define DSMI_LOG_PROC "/proc/dsmi_log"
#define MAX_LOG_BUF 1024

// The fd is automatically closed when the process exits.
static void __attribute__((destructor)) dsmi_log_exit(void)
{
    pthread_mutex_lock(&g_log_mtx);
    if (g_log_fd >= 0) {
        (void)close(g_log_fd);
        g_log_fd = -1;
    }
    pthread_mutex_unlock(&g_log_mtx);
}

// only open once
static int dsmi_log_get_fd(void)
{
    if (g_log_fd >= 0) {
        return g_log_fd;
    }

    int fd = open(DSMI_LOG_PROC, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return -errno;
    }

    g_log_fd = fd;
    return g_log_fd;
}

static int vdsmi_printf(const char *fmt, va_list ap)
{
    char buf[MAX_LOG_BUF];
    int fd;
    ssize_t len, nwrite;
    ssize_t off = 0;

    if (fmt == NULL) {
        return -EINVAL;
    }

    len = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1U, fmt, ap);
    if (len < 0 || (size_t)len >= sizeof(buf)) {
        return -EINVAL;
    }

    pthread_mutex_lock(&g_log_mtx);
    fd = dsmi_log_get_fd();
    if (fd < 0) {
        pthread_mutex_unlock(&g_log_mtx);
        return fd;
    }

    while (off < len) {
        nwrite = write(fd, buf + off, (size_t)(len - off));
        if (nwrite > 0) {
            off += nwrite;
            continue;
        }
        if (nwrite < 0 && errno == EINTR) {
            continue;
        }
        pthread_mutex_unlock(&g_log_mtx);
        return (nwrite == 0) ? -EIO : -errno;
    }

    pthread_mutex_unlock(&g_log_mtx);
    return (int)len;
}

int dsmi_printf(const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = vdsmi_printf(fmt, ap);
    va_end(ap);
    return ret;
}

int halDsmiPrintf(const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = vdsmi_printf(fmt, ap);
    va_end(ap);
    return ret;
}

const char *halGetProcessName(void)
{
    if (program_invocation_short_name == NULL || program_invocation_short_name[0] == '\0') {
        return "unknown";
    }

    return program_invocation_short_name;
}