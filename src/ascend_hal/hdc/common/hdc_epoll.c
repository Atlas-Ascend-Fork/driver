/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>

#include "ascend_hal.h"
#include "hdc_cmn.h"
#include "hdcdrv_cmd_ioctl.h"
#include "hdc_epoll.h"

STATIC struct hdc_epoll_ops *drv_hdc_epoll_get_ops(const struct hdcConfig *conf)
{
    struct hdc_epoll_ops *epoll_ops = NULL;

    switch (conf->trans_type) {
        case HDC_TRANS_USE_PCIE:
#ifdef CFG_FEATURE_SUPPORT_UB
            if (conf->h2d_type == HDC_TRANS_USE_UB) {
                epoll_ops = drv_get_hdc_ub_epoll_ops(); /* UB */
            } else if (conf->h2d_type == HDC_TRANS_USE_PCIE) {
                epoll_ops = drv_get_hdc_pcie_epoll_ops();
            } else {
                HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", conf->h2d_type);
            }
#else
            epoll_ops = drv_get_hdc_pcie_epoll_ops();
#endif
            break;
        case HDC_TRANS_USE_SOCKET:
            epoll_ops = drv_get_hdc_sock_epoll_ops();
            break;
        default:
            HDC_LOG_ERR("Invalid trans type.%u\n", conf->trans_type);
            break;
    }

    return epoll_ops;
}
drvError_t drvHdcEpollCreate(signed int size, HDC_EPOLL *epoll)
{
    signed int ret;
    struct hdc_epoll_head *epoll_head = NULL;
    struct hdc_epoll_ops *epoll_ops = NULL;

    if ((size <= 0) || (size > HDCDRV_EPOLL_FD_EVENT_NUM)) {
        HDC_LOG_ERR("Input parameter size is error. (size=%d)\n", size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (epoll == NULL) {
        HDC_LOG_ERR("Input parameter epoll is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    epoll_ops = drv_hdc_epoll_get_ops(&g_hdcConfig);
    if ((epoll_ops == NULL) || (epoll_ops->hdc_epoll_create == NULL)) {
        HDC_LOG_ERR("get epoll object failed.\n");
        ret = DRV_ERROR_MALLOC_FAIL;
        goto err;
    }

    epoll_head = (struct hdc_epoll_head *)malloc(sizeof(struct hdc_epoll_head));
    if (epoll_head == NULL) {
        HDC_LOG_ERR("Call malloc failed.\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    epoll_head->magic = HDC_MAGIC_WORD;
    epoll_head->size = size;

    ret = epoll_ops->hdc_epoll_create(epoll_head, size);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Epoll create error. (ret=%d)\n", ret);
        goto err;
    }

    *epoll = (HDC_EPOLL)epoll_head;

    return DRV_ERROR_NONE;
err:
    free(epoll_head);
    epoll_head = NULL;
    return (drvError_t)ret;
}

drvError_t drvHdcEpollClose(HDC_EPOLL epoll)
{
    signed int ret;
    struct hdc_epoll_head *epoll_head = (struct hdc_epoll_head *)epoll;
    struct hdc_epoll_ops *epoll_ops = drv_hdc_epoll_get_ops(&g_hdcConfig);

    if (epoll_head == NULL) {
        HDC_LOG_ERR("Input parameter epoll_head is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (epoll_head->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Not HDC handle. (magic=0x%x)\n", epoll_head->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((epoll_ops == NULL) || (epoll_ops->hdc_epoll_close == NULL)) {
        HDC_LOG_ERR("get epoll ops object failed.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = epoll_ops->hdc_epoll_close(epoll_head);
    if (ret != 0) {
        HDC_LOG_ERR("Epoll free fd failed. (epfd=%d; ret=%d)\n", epoll_head->epfd, ret);
        return DRV_ERROR_INNER_ERR;
    }

    free(epoll_head);
    epoll_head = NULL;

    return DRV_ERROR_NONE;
}

STATIC drvError_t drv_hdc_epoll_ctl_para_check(HDC_EPOLL epoll, signed int op, const void *target,
                                               const struct drvHdcEvent *event)
{
    struct hdc_epoll_head *epoll_head = (struct hdc_epoll_head *)epoll;
    signed int epfd;
    unsigned int event_mask = (HDC_EPOLL_DATA_IN | HDC_EPOLL_FAST_DATA_IN | HDC_EPOLL_CONN_IN |
                               HDC_EPOLL_SESSION_CLOSE);

    if ((epoll_head == NULL) || (target == NULL) || (event == NULL)) {
        HDC_LOG_ERR("Input parameter is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (epoll_head->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Not HDC handle. (magic=0x%x)\n", epoll_head->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    epfd = (signed int)epoll_head->epfd;

    if ((event->events & (HDC_EPOLL_DATA_IN | HDC_EPOLL_FAST_DATA_IN | HDC_EPOLL_SESSION_CLOSE)) &&
        (event->events & HDC_EPOLL_CONN_IN)) {
        HDC_LOG_ERR("Epoll conflict event. (epfd=%d; event=0x%x)\n", epfd, event->events);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((event->events & ~event_mask) != 0) {
        HDC_LOG_ERR("Input parameter events is error. (epfd=%d; event=0x%x)\n", epfd, event->events);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((event->events & event_mask) == 0) {
        HDC_LOG_ERR("Input parameter is error. (epfd=%d; event=0x%x)\n", epfd, event->events);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((op != HDC_EPOLL_CTL_ADD) && (op != HDC_EPOLL_CTL_DEL)) {
        HDC_LOG_ERR("Input parameter op  is error. (epfd=%d; op=%d)\n", epfd, op);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvHdcEpollCtl(HDC_EPOLL epoll, signed int op, void *target, struct drvHdcEvent *event)
{
    signed int ret;
    struct hdc_epoll_head *epoll_head = (struct hdc_epoll_head *)epoll;
    struct hdc_epoll_ops *epoll_ops = drv_hdc_epoll_get_ops(&g_hdcConfig);

    if ((epoll_ops == NULL) || (epoll_ops->hdc_epoll_ctl == NULL)) {
        HDC_LOG_ERR("get epoll ops object failed.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = drv_hdc_epoll_ctl_para_check(epoll, op, target, event);
    if (ret != DRV_ERROR_NONE) {
        return (drvError_t)ret;
    }

    /* epoll_ctl cannot mocker for ut, check ret here */
    ret = epoll_ops->hdc_epoll_ctl(epoll_head, op, target, event);
    if (ret != 0) {
        HDC_LOG_ERR("Call epoll_ctl failed. (epfd=%d; op=%d; ret=%d; strerror=\"%s\"; errno=%d)\n", epoll_head->epfd,
                    op, ret, strerror(errno), errno);
        if (ret == (-HDCDRV_SESSION_HAS_CLOSED)) {
            return DRV_ERROR_SOCKET_CLOSE;
        } else if (ret == (-HDCDRV_NO_PERMISSION)) {
            return DRV_ERROR_OPER_NOT_PERMITTED;
        } else if ((ret == (-HDCDRV_ERR)) || (ret == (-HDCDRV_PARA_ERR))) {
            return DRV_ERROR_INVALID_VALUE;
        } else if (ret == (-HDCDRV_DEVICE_NOT_READY)) {
            return DRV_ERROR_DEVICE_NOT_READY;
        }

        return (drvError_t)ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t drv_hdc_epoll_wait_para_check(HDC_EPOLL epoll, const struct drvHdcEvent *events, signed int maxevents,
                                                signed int timeout, const signed int *eventnum)
{
    struct hdc_epoll_head *epoll_head = (struct hdc_epoll_head *)epoll;

    if ((epoll_head == NULL) || (events == NULL) || (eventnum == NULL)) {
        HDC_LOG_ERR("Input parameter is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((maxevents <= 0) || (maxevents > HDCDRV_EPOLL_FD_EVENT_NUM)) {
        HDC_LOG_ERR("Input parameter maxevents is error. (maxevents=%d)\n", maxevents);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (epoll_head->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Not HDC handle. (magic=0x%x)\n", epoll_head->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (epoll_head->size < maxevents) {
        HDC_LOG_ERR("maxevents out of range. (epfd=%d; maxevents=%d; range=%d)\n", (signed int)epoll_head->epfd,
                    maxevents, epoll_head->size);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (timeout <= 0) {
        HDC_LOG_ERR("Input parameter timeout is error. (epfd=%d; timeout=%d)\n", (signed int)epoll_head->epfd, timeout);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvHdcEpollWait(HDC_EPOLL epoll, struct drvHdcEvent *events, signed int maxevents, signed int timeout,
                           signed int *eventnum)
{
    signed int ret;
    struct hdc_epoll_head *epoll_head = (struct hdc_epoll_head *)epoll;
    struct hdc_epoll_ops *epoll_ops = drv_hdc_epoll_get_ops(&g_hdcConfig);

    if ((epoll_ops == NULL) || (epoll_ops->hdc_epoll_wait == NULL)) {
        HDC_LOG_ERR("get epoll ops object failed.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = drv_hdc_epoll_wait_para_check(epoll, events, maxevents, timeout, eventnum);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Parameter check failed.\n");
        return ret;
    }

    return epoll_ops->hdc_epoll_wait(epoll_head, events, maxevents, timeout, eventnum);
}
