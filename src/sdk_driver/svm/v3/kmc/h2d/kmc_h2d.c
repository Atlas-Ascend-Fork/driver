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
#include "ka_dfx_pub.h"

#include "comm_kernel_interface.h"
#include "pbl_feature_loader.h"
#include "pbl_uda.h"

#include "svm_pub.h"
#include "svm_kern_log.h"
#include "framework_dev.h"
#include "kmc_msg.h"
#include "kmc_h2d.h"

static u32 kmc_h2d_feature_id;
static struct svm_kmc_d2h_recv_handle *g_d2h_handle[SVM_KMC_MSG_ID_MAX] = {NULL};
static u32 g_max_msg_len;

u32 svm_kmc_h2d_get_max_msg_len(void)
{
    return g_max_msg_len;
}

void svm_kmc_d2h_recv_handle_register(enum svm_kmc_msg_id msg_id, struct svm_kmc_d2h_recv_handle *handle)
{
    g_d2h_handle[msg_id] = handle;
}

int svm_kmc_h2d_send(u32 udevid, void *msg, u32 in_len, u32 out_len, u32 *real_out_len)
{
    struct svm_kmc_msg_head *head = (struct svm_kmc_msg_head *)msg;
    void *msg_chan = NULL;
    void *dev_ctx = NULL;
    int ret;

    dev_ctx = svm_dev_ctx_get(udevid);
    if (dev_ctx == NULL) {
        svm_err("No dev. (udevid=%u)\n", udevid);
        return -ENODEV;
    }

    msg_chan = svm_dev_get_feature_priv(dev_ctx, kmc_h2d_feature_id);
    if (msg_chan == NULL) {
        svm_err("Get msg chan failed. (udevid=%u; feature_id=%u)\n", udevid, kmc_h2d_feature_id);
        svm_dev_ctx_put(dev_ctx);
        return -EINVAL;
    }

    ret = devdrv_sync_msg_send(msg_chan, msg, in_len, out_len, real_out_len);
    svm_dev_ctx_put(dev_ctx);
    if (ret != 0) {
        svm_err("devdrv_sync_msg_send failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }
    return (out_len != 0) ? head->ret : 0;
}

static int kmc_d2h_recv(u32 udevid, void *msg, u32 in_len, u32 out_len, u32 *real_out_len)
{
    struct svm_kmc_msg_head *head = NULL;
    struct svm_kmc_d2h_recv_handle *handle = NULL;
    u32 actual_len, data_len;

    if (msg == NULL) {
        svm_err("Data is NULL. \n");
        return -EINVAL;
    }

    if (real_out_len == NULL) {
        svm_err("Out_len is NULL. \n");
        return -EINVAL;
    }

    if (in_len < sizeof(struct svm_kmc_msg_head)) {
        svm_err("In_data_len is invalid. (in_len=%u)\n", in_len);
        return -EMSGSIZE;
    }

    head = (struct svm_kmc_msg_head *)msg;
    if ((head->msg_id >= SVM_KMC_MSG_ID_MAX) || (g_d2h_handle[head->msg_id] == NULL)) {
        svm_err("Invalid msg id. (msg_id=%u)\n", head->msg_id);
        return -EINVAL;
    }

    handle = g_d2h_handle[head->msg_id];
    if ((handle->raw_msg_size + head->extend_num * handle->extend_gran_size) > g_max_msg_len) {
        svm_err("Invalid extend num. (extend_num=%llu; extend_gran_size=%u; raw_msg_size=%u; g_max_msg_len=%u)\n",
                head->extend_num, handle->extend_gran_size, handle->raw_msg_size, g_max_msg_len);
        return -EINVAL;
    }

    data_len = ka_base_max_t(u32, in_len, out_len);
    actual_len = handle->raw_msg_size + head->extend_num * handle->extend_gran_size;
    if (data_len < actual_len) {
        svm_err("Invalid data len. (data_len=%u; actual_len=%u; msg_id=%u; extend_num=%u)\n", data_len, actual_len,
                head->msg_id, head->extend_num);
        return -EMSGSIZE;
    }

    head->ret = handle->func(udevid, msg, real_out_len);
    return 0;
}

static int kmc_rx_msg_process(void *msg_chan, void *msg, u32 in_len, u32 out_len, u32 *real_out_len)
{
    u32 udevid;
    int ret;

    ret = devdrv_get_msg_chan_devid(msg_chan);
    if (ret < 0) {
        svm_err("agentdrv_get_msg_chan_devid failed. (ret=%d)\n", ret);
        return ret;
    }
    udevid = (u32)ret;

    return kmc_d2h_recv(udevid, msg, in_len, out_len, real_out_len);
}

#define SVM_NON_TRANS_MSG_DESC_SIZE (64ULL * SVM_BYTES_PER_KB)
static struct devdrv_non_trans_msg_chan_info chan_info = {
    .msg_type = devdrv_msg_client_devmm,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_LOW,
    .s_desc_size = SVM_NON_TRANS_MSG_DESC_SIZE,
    .c_desc_size = SVM_NON_TRANS_MSG_DESC_SIZE,
    .rx_msg_process = kmc_rx_msg_process,
};

int kmc_h2d_init_dev(u32 udevid)
{
    void *msg_chan = NULL;
    void *dev_ctx = NULL;
    int ret;

    if (udevid == uda_get_host_id()) {
        return 0;
    }

    msg_chan = devdrv_pcimsg_alloc_non_trans_queue(udevid, &chan_info);
    if (msg_chan == NULL) {
        svm_err("devdrv_pcimsg_alloc_non_trans_queue failed. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }

    ret = devdrv_get_non_trans_chan_capacity(msg_chan, &g_max_msg_len);
    if (ret != 0) {
        (void)devdrv_pcimsg_free_non_trans_queue(msg_chan);
        svm_err("devdrv_get_non_trans_chan_capacity failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    dev_ctx = svm_dev_ctx_get(udevid);
    if (dev_ctx == NULL) {
        (void)devdrv_pcimsg_free_non_trans_queue(msg_chan);
        return -ENODEV;
    }

    ret = svm_dev_set_feature_priv(dev_ctx, kmc_h2d_feature_id, "kmc_h2d", msg_chan);
    if (ret != 0) {
        svm_err("Set dev feature priv failed. (udevid=%u; ret=%d)\n", udevid, ret);
        (void)devdrv_pcimsg_free_non_trans_queue(msg_chan);
        svm_dev_ctx_put(dev_ctx);
        return ret;
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(kmc_h2d_init_dev, FEATURE_LOADER_STAGE_0);

void kmc_h2d_uninit_dev(u32 udevid)
{
    void *msg_chan = NULL;
    void *dev_ctx = NULL;

    if (udevid == uda_get_host_id()) {
        return;
    }

    dev_ctx = svm_dev_ctx_get(udevid);
    if (dev_ctx == NULL) {
        return;
    }

    msg_chan = svm_dev_get_feature_priv(dev_ctx, kmc_h2d_feature_id);
    if (msg_chan != NULL) {
        (void)svm_dev_set_feature_priv(dev_ctx, kmc_h2d_feature_id, NULL, NULL);
        svm_dev_ctx_put(dev_ctx); /* paired with init */
        svm_dev_ctx_put(dev_ctx);
        (void)devdrv_pcimsg_free_non_trans_queue(msg_chan);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(kmc_h2d_uninit_dev, FEATURE_LOADER_STAGE_0);

int kmc_h2d_init(void)
{
    kmc_h2d_feature_id = svm_dev_obtain_feature_id();
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(kmc_h2d_init, FEATURE_LOADER_STAGE_0);

void kmc_h2d_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(kmc_h2d_uninit, FEATURE_LOADER_STAGE_0);