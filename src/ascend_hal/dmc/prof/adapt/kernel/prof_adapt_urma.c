/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef UT_TEST
#include "prof_adapt_h2d.h"
#include "prof_adapt.h"
#include "prof_buff.h"
#include "prof_event_master.h"
#include "prof_urma.h"

drvError_t prof_urma_kernel_get_channels(uint32_t dev_id, struct prof_channel_list *channels)
{
    return prof_urma_get_channels(dev_id, channels);
}

struct prof_urma_chan_priv {
    uint8_t *buff;
    struct prof_urma_chan_info *urma_chan_info;
};

static drvError_t prof_urma_chan_init(uint32_t dev_id, uint32_t chan_id, bool event_flag, char **priv)
{
    struct prof_urma_chan_priv *chan_priv = NULL;
    drvError_t ret;

    (void)event_flag;

    chan_priv = (struct prof_urma_chan_priv *)malloc(sizeof(struct prof_urma_chan_priv));
    if (chan_priv == NULL) {
        PROF_ERR("Failed to alloc chan_priv. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_MALLOC_FAIL;
    }

    chan_priv->urma_chan_info = prof_urma_chan_info_creat(dev_id, chan_id);
    if (chan_priv->urma_chan_info == NULL) {
        free(chan_priv);
        PROF_ERR("Failed to create urma_info. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = prof_buff_init(chan_id, &chan_priv->buff);
    if (ret != DRV_ERROR_NONE) {
        prof_urma_chan_info_destroy(chan_priv->urma_chan_info);
        free(chan_priv);
        PROF_ERR("Failed to init buff. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return ret;
    }

    *priv = (char *)chan_priv;
    return DRV_ERROR_NONE;
}

static void prof_urma_chan_uninit(char **priv)
{
    struct prof_urma_chan_priv *chan_priv = (struct prof_urma_chan_priv *)(*priv);

    prof_buff_uninit(&chan_priv->buff);
    prof_urma_chan_info_destroy(chan_priv->urma_chan_info);
    free(chan_priv);
    *priv = NULL;
}

static drvError_t prof_urma_chan_start(uint32_t dev_id, uint32_t chan_id, struct prof_user_start_para *para, char *priv)
{
    struct prof_urma_chan_priv *chan_priv = (struct prof_urma_chan_priv *)priv;
    struct prof_urma_start_para urma_start_para = {0};
    struct prof_user_stop_para stop_para = {0};
    drvError_t ret;

    urma_start_para.data_buff_addr = (uint64_t)(uintptr_t)prof_buff_get_buf_addr(chan_priv->buff);
    urma_start_para.data_buff_len = prof_buff_get_buf_size(chan_priv->buff);
    urma_start_para.rptr_addr = (uint64_t)(uintptr_t)prof_buff_get_readptr_addr(chan_priv->buff);
    urma_start_para.rptr_size = prof_buff_get_readptr_size(chan_priv->buff);

    ret = prof_urma_start(dev_id, chan_id, para, &urma_start_para, chan_priv->urma_chan_info);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to start prof by urma. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
        return ret;
    }

    if (para->remote_pid != 0) {
        ret = prof_event_start(dev_id, chan_id, para);
        if (ret != DRV_ERROR_NONE) {
            stop_para.remote_pid = para->remote_pid;
            (void)prof_urma_stop(dev_id, chan_id, &stop_para, chan_priv->urma_chan_info);
            PROF_ERR("Failed to send start event. (devid=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

static drvError_t prof_urma_chan_stop(uint32_t dev_id, uint32_t chan_id, struct prof_user_stop_para *para, char *priv)
{
    struct prof_urma_chan_priv *chan_priv = (struct prof_urma_chan_priv *)priv;
    drvError_t ret;

    if (para->remote_pid != 0) {
        ret = prof_event_stop(dev_id, chan_id, para);
        if (ret != DRV_ERROR_NONE) {
            PROF_ERR("Failed to send stop event. (devid=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
            return ret;
        }
    }

    ret = prof_urma_stop(dev_id, chan_id, para, chan_priv->urma_chan_info);

    prof_buff_wait_read_empty(chan_priv->buff, dev_id, chan_id);

    return ret;
}

static drvError_t prof_urma_chan_flush(uint32_t dev_id, uint32_t chan_id, uint32_t *data_len, char *priv)
{
    struct prof_urma_chan_priv *chan_priv = (struct prof_urma_chan_priv *)priv;
    drvError_t ret;

    ret = prof_urma_flush(dev_id, chan_id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    *data_len = prof_buff_get_data_len(chan_priv->buff);
    return DRV_ERROR_NONE;
}

static int prof_urma_chan_read(uint32_t dev_id, uint32_t chan_id, prof_user_read_para_t *read_para, char *priv)
{
    struct prof_urma_chan_priv *chan_priv = (struct prof_urma_chan_priv *)priv;
    drvError_t ret;
    int read_len;

    read_len = prof_buff_read(chan_priv->buff, read_para->out_buf, read_para->buf_size);
    if (read_len <= 0) {
        return read_len;
    }

    if (read_para->write_rptr_flag == false) {
        /* the addr of remote rptr may be null when channel is stopping or starting */
        PROF_WARN("The channel is not ready yet, don't write remote rptr. (dev_id=%u, chan_id=%u)\n", dev_id, chan_id);
        return read_len;
    }

    ret = prof_urma_write_remote_r_ptr(dev_id, chan_id, chan_priv->urma_chan_info);
    if (ret != DRV_ERROR_NONE) {
        PROF_ERR("Failed to write remote r_ptr. (dev_id=%u, chan_id=%u, ret=%d)\n", dev_id, chan_id, (int)ret);
    }

    return read_len;
}

static drvError_t prof_urma_chan_query(uint32_t dev_id, uint32_t chan_id, uint32_t *avail_len, char *priv)
{
    (void)dev_id;
    (void)chan_id;
    (void)avail_len;
    (void)priv;
    return DRV_ERROR_NOT_SUPPORT;
}

static drvError_t prof_urma_chan_report(uint32_t dev_id, uint32_t chan_id, void *data, uint32_t data_len, char *priv)
{
    struct prof_urma_chan_priv *chan_priv = (struct prof_urma_chan_priv *)priv;
    struct prof_adapt_core_notifier *notifier = prof_adapt_get_notifier();
    struct prof_urma_chan_info *urma_chan_info = NULL;
    uint32_t new_wptr = *((uint32_t *)data);
    uint32_t old_wptr;

    (void)data_len;

    old_wptr = prof_buff_get_writeptr(chan_priv->buff);
    if (new_wptr != old_wptr) {
        prof_buff_update_writeptr(chan_priv->buff, new_wptr);
        notifier->poll_report(dev_id, chan_id);
    }

    urma_chan_info = chan_priv->urma_chan_info;

    return prof_urma_post_recv(urma_chan_info, 1);
}

static struct prof_chan_ops prof_urma_chan_ops = {
    .init = prof_urma_chan_init,
    .uninit = prof_urma_chan_uninit,
    .start = prof_urma_chan_start,
    .stop = prof_urma_chan_stop,
    .flush = prof_urma_chan_flush,
    .read = prof_urma_chan_read,
    .query = prof_urma_chan_query,
    .report = prof_urma_chan_report,
};

drvError_t prof_urma_get_chan_ops(struct prof_chan_ops **ops)
{
    *ops = &prof_urma_chan_ops;
    return DRV_ERROR_NONE;
}

STATIC void __attribute__((constructor)) prof_urma_h2d_register(void)
{
    struct prof_h2d_ops urma_ops = {
        .get_channels = prof_urma_kernel_get_channels,
        .get_chan_ops = prof_urma_get_chan_ops,
    };

    prof_h2d_regiser_urma_ops(&urma_ops);
}

#else
int prof_adapt_urma_ut_test(void)
{
    return 0;
}
#endif
