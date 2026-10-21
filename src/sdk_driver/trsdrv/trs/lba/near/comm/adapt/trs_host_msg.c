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
#include "ka_kernel_def_pub.h"
#include "ka_common_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"

#include "pbl_kref_safe.h"
#include "comm_kernel_interface.h"

#include "trs_chip_def_comm.h"
#include "trs_host_msg.h"
#ifdef CFG_FEATURE_TRS_SIA_ADAPT
#include "trs_sia_adapt_auto_init.h"
#elif defined(CFG_FEATURE_TRS_SEC_EH_ADAPT)
#include "trs_sec_eh_auto_init.h"
#endif

static KA_TASK_DEFINE_MUTEX(host_msg_mutex);

struct trs_host_msg {
    u32 devid;
    void *chan;
    struct kref_safe ref;
};

static struct trs_host_msg *g_host_msg[TRS_DEV_MAX_NUM];

static struct trs_host_msg *trs_host_msg_create(u32 devid, struct devdrv_non_trans_msg_chan_info *chan_info)
{
    struct trs_host_msg *host_msg = NULL;
    void *chan = NULL;
    int ret;

    host_msg = (struct trs_host_msg *)trs_kmalloc(sizeof(struct trs_host_msg), KA_GFP_KERNEL);
    if (host_msg == NULL) {
        return NULL;
    }

    chan = devdrv_pcimsg_alloc_non_trans_queue(devid, chan_info);
    if (chan == NULL) {
        trs_kfree(host_msg);
        trs_err("Msg chan alloc fail. (devid=%u)\n", devid);
        return NULL;
    }
    ret = devdrv_set_msg_chan_priv(chan, (void *)(uintptr_t)devid);
    if (ret != 0) {
        (void)devdrv_pcimsg_free_non_trans_queue(chan);
        trs_kfree(host_msg);
        trs_err("Set msg chan priv fail. (devid=%u; ret=%d)\n", devid, ret);
        return NULL;
    }
    host_msg->devid = devid;
    host_msg->chan = chan;
    kref_safe_init(&host_msg->ref);

    return host_msg;
}

static void trs_host_msg_destroy(struct trs_host_msg *host_msg)
{
    if (host_msg->chan != NULL) {
        (void)devdrv_pcimsg_free_non_trans_queue(host_msg->chan);
        host_msg->chan = NULL;
    }
    trs_kfree(host_msg);
}

static void trs_host_msg_release(struct kref_safe *kref)
{
    struct trs_host_msg *host_msg = ka_container_of(kref, struct trs_host_msg, ref);

    trs_host_msg_destroy(host_msg);
}

static int trs_host_msg_add(struct trs_host_msg *host_msg)
{
    ka_task_mutex_lock(&host_msg_mutex);
    if (g_host_msg[host_msg->devid] != NULL) {
        ka_task_mutex_unlock(&host_msg_mutex);
        return -ENODEV;
    }
    g_host_msg[host_msg->devid] = host_msg;
    ka_task_mutex_unlock(&host_msg_mutex);

    return 0;
}

static void trs_host_msg_del(u32 devid)
{
    struct trs_host_msg *host_msg = NULL;

    ka_task_mutex_lock(&host_msg_mutex);
    host_msg = g_host_msg[devid];
    g_host_msg[devid] = NULL;
    ka_task_mutex_unlock(&host_msg_mutex);

    if (host_msg != NULL) {
        kref_safe_put(&host_msg->ref, trs_host_msg_release);
    }
}

static struct trs_host_msg *trs_host_msg_get(u32 devid)
{
    struct trs_host_msg *host_msg = NULL;

    if (devid >= TRS_DEV_MAX_NUM) {
        trs_err("Invalid devid. (devid=%u)\n", devid);
        return NULL;
    }
    ka_task_mutex_lock(&host_msg_mutex);
    host_msg = g_host_msg[devid];
    if (host_msg != NULL) {
        kref_safe_get(&host_msg->ref);
    }
    ka_task_mutex_unlock(&host_msg_mutex);

    return host_msg;
}

static void trs_host_msg_put(struct trs_host_msg *host_msg)
{
    kref_safe_put(&host_msg->ref, trs_host_msg_release);
}

int trs_host_msg_chan_recv_check(u32 devid, struct trs_msg_data *data, u32 in_data_len, u32 out_data_len,
                                 u32 *real_out_len)
{
    if (devid >= TRS_DEV_MAX_NUM) {
        trs_err("Invalid devid. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((data == NULL) || (in_data_len != (u32)sizeof(struct trs_msg_data))) {
        return -EINVAL;
    }

    if (data->header.valid != TRS_MSG_SEND_MAGIC) {
        trs_err("Invalid magic. (magic=0x%x)\n", data->header.valid);
        return -EINVAL;
    }

    if (data->header.tsid >= TRS_TS_MAX_NUM) {
        trs_err("Invalid tsid. (tsid=%u)\n", data->header.tsid);
        return -EINVAL;
    }

    if (real_out_len == NULL) {
        return -EINVAL;
    }

    if (data->header.cmdtype >= TRS_MSG_MAX) {
        trs_err("Invalid cmd. (cmd=%u)\n", data->header.cmdtype);
        return -EINVAL;
    }

    return 0;
}

static int trs_host_msg_send_comm(u32 devid, void *msg, size_t size)
{
    struct trs_msg_data *tmp_msg = (struct trs_msg_data *)msg;
    struct trs_host_msg *host_msg = NULL;
    u32 out_len;
    int ret;

    if ((msg == NULL) || (size > sizeof(struct trs_msg_data))) {
        return -EINVAL;
    }

    host_msg = trs_host_msg_get(devid);
    if (host_msg == NULL) {
        return -ENODEV;
    }
    ret = devdrv_sync_msg_send(host_msg->chan, tmp_msg, sizeof(struct trs_msg_data), sizeof(struct trs_msg_data),
                               &out_len);
    trs_host_msg_put(host_msg);

    return ret;
}

int trs_host_msg_send(u32 devid, void *msg, size_t size)
{
    int ret = trs_host_msg_send_comm(devid, msg, size);
    return (ret == 0) ? (int)(((struct trs_msg_data *)msg)->header.result) : ret;
}
KA_EXPORT_SYMBOL_GPL(trs_host_msg_send);

int trs_host_msg_send_pure_ret(u32 devid, void *msg, size_t size)
{
    return trs_host_msg_send_comm(devid, msg, size);
}

int trs_host_msg_init(u32 ts_inst_id)
{
    struct trs_id_inst inst;
    struct trs_host_msg *host_msg = NULL;
    int ret;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (inst.tsid > 0) {
        return 0;
    }
    host_msg = trs_host_msg_create(inst.devid, trs_get_msg_chan_info());
    if (host_msg == NULL) {
        return -ENOMEM;
    }
    ret = trs_host_msg_add(host_msg);
    if (ret != 0) {
        trs_host_msg_destroy(host_msg);
        trs_err("Add host msg fail. (devid=%u; ret=%d)\n", inst.devid, ret);
        return ret;
    }
    trs_info("Host msg init success. (devid=%u)\n", inst.devid);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(trs_host_msg_init, FEATURE_LOADER_STAGE_0);

void trs_host_msg_uninit(u32 ts_inst_id)
{
    struct trs_id_inst inst;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (inst.tsid == 0) {
        trs_host_msg_del(inst.devid);
    }
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(trs_host_msg_uninit, FEATURE_LOADER_STAGE_0);
