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
#include "ka_barrier_pub.h"
#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"
#include "pbl_kref_safe.h"

#include "trs_pub_def.h"
#include "trs_mailbox_def.h"
#include "trs_chan_update.h"
#include "trs_timestamp.h"
#include "trs_ts_status.h"
#include "trs_hard_mbox.h"

static KA_TASK_DEFINE_MUTEX(trs_mbox_mutex);

struct trs_mbox_chan_fault {
    int cont_tx_timeout;
    int cont_tx_timeout_max; /* ms */
    unsigned long broken_timestamp;
    u32 retry_duration;
};

struct trs_mbox_chan {
    struct trs_id_inst inst;

    void __ka_mm_iomem *base;
    size_t size;
    u64 tx_time_us;
    u64 txdone_time_us;
    u64 wakeup_time_us;

    ka_completion_t tx_complete;
    ka_mutex_t mutex;

    struct kref_safe ref;

    void *priv;
    struct trs_mbox_ops ops;

    u64 tx_cnt;
    u64 irq_cnt;
    struct trs_mbox_chan_fault fault;
    enum trs_mbox_mem_type mem_type;
};

static struct trs_mbox_chan *g_mbox_chan[TRS_TS_INST_MAX_NUM];

static struct trs_mbox_chan *trs_mbox_chan_create(struct trs_id_inst *inst, struct trs_mbox_chan_attr *attr)
{
    struct trs_mbox_chan *chan = trs_kzalloc(sizeof(struct trs_mbox_chan), KA_GFP_KERNEL);

    if (chan == NULL) {
        trs_err("Alloc mbox chan fail\n");
        return NULL;
    }
    if (attr->mem_type == TRS_MBOX_MEM_TYPE_NORMAL) {
        chan->base = ka_mm_ioremap_wc(attr->base, attr->size);
    } else {
        chan->base = ka_mm_ioremap(attr->base, attr->size);
    }
    if (chan->base == NULL) {
        trs_kfree(chan);
        trs_err("Ioremap chan base fail. (size=0x%lx)\n", attr->size);
        return NULL;
    }
    ka_mm_memset_io(chan->base, 0, attr->size);
    chan->size = attr->size;
    chan->priv = attr->priv;
    chan->ops = attr->ops;
    chan->inst = *inst;
    chan->tx_cnt = 0;
    chan->irq_cnt = 0;
    chan->mem_type = attr->mem_type;
    kref_safe_init(&chan->ref);
    chan->fault.cont_tx_timeout = 0;
    chan->fault.cont_tx_timeout_max = attr->cont_tx_timeout_max;
    chan->fault.retry_duration = 10000; /* 10000ms */
    ka_task_init_completion(&chan->tx_complete);
    ka_task_mutex_init(&chan->mutex);

    return chan;
}

static void trs_mbox_chan_destroy(struct trs_mbox_chan *chan)
{
    if (chan != NULL) {
        if (chan->base != NULL) {
            ka_mm_iounmap(chan->base);
            chan->base = NULL;
        }
        ka_task_mutex_destroy(&chan->mutex);
        trs_kfree(chan);
    }
}

static void trs_mbox_chan_release(struct kref_safe *kref)
{
    struct trs_mbox_chan *chan = ka_container_of(kref, struct trs_mbox_chan, ref);

    if (chan->ops.mbox_release != NULL) {
        chan->ops.mbox_release(chan->priv);
    }
    trs_mbox_chan_destroy(chan);
}

static int trs_mbox_chan_add(struct trs_mbox_chan *chan)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(&chan->inst);

    ka_task_mutex_lock(&trs_mbox_mutex);
    if (g_mbox_chan[ts_inst] != NULL) {
        ka_task_mutex_unlock(&trs_mbox_mutex);
        return -ENODEV;
    }
    g_mbox_chan[ts_inst] = chan;
    ka_task_mutex_unlock(&trs_mbox_mutex);

    return 0;
}

static void trs_mbox_chan_del(struct trs_id_inst *inst)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(inst);
    struct trs_mbox_chan *chan = NULL;

    ka_task_mutex_lock(&trs_mbox_mutex);
    chan = g_mbox_chan[ts_inst];
    g_mbox_chan[ts_inst] = NULL;
    ka_task_mutex_unlock(&trs_mbox_mutex);

    if (chan != NULL) {
        chan->ops.free_irq(chan->priv);
        kref_safe_put(&chan->ref, trs_mbox_chan_release);
    }
}

static struct trs_mbox_chan *trs_mbox_chan_get(struct trs_id_inst *inst)
{
    u32 ts_inst = trs_id_inst_to_ts_inst(inst);
    struct trs_mbox_chan *chan = NULL;

    ka_task_mutex_lock(&trs_mbox_mutex);
    chan = g_mbox_chan[ts_inst];
    if (chan != NULL) {
        kref_safe_get(&chan->ref);
    }
    ka_task_mutex_unlock(&trs_mbox_mutex);

    return chan;
}

static void trs_mbox_chan_put(struct trs_mbox_chan *chan)
{
    kref_safe_put(&chan->ref, trs_mbox_chan_release);
}

void *trs_mbox_chan_init(struct trs_id_inst *inst, struct trs_mbox_chan_attr *attr)
{
    struct trs_mbox_chan *chan = NULL;
    int ret;

    chan = trs_mbox_chan_create(inst, attr);
    if (chan == NULL) {
        return NULL;
    }

    ret = trs_mbox_chan_add(chan);
    if (ret != 0) {
        trs_mbox_chan_destroy(chan);
        return NULL;
    }

    return chan;
}

void trs_mbox_chan_uninit(struct trs_id_inst *inst)
{
    trs_mbox_chan_del(inst);
}

static void trs_mbox_chan_write(struct trs_mbox_chan *chan, void *data, size_t size)
{
    if (chan->mem_type == TRS_MBOX_MEM_TYPE_NORMAL) {
        (void)memcpy_s(chan->base, chan->size, data, size);
    } else {
        ka_mm_memcpy_toio(chan->base, data, size);
    }
    ka_wmb();
}

static void trs_mbox_chan_get_result(struct trs_mbox_chan *chan, void *data)
{
    struct trs_mb_header *mbox_msg_header = (struct trs_mb_header *)chan->base;
    struct trs_mb_header *header = (struct trs_mb_header *)data;
    header->result = mbox_msg_header->result;

    if ((header->cmd_type == TRS_MBOX_RPC_CALL) || (header->cmd_type == TRS_MBOX_DSMI_RPC_CALL)) {
        struct trs_rpc_call_msg *rpc_msg_back = (struct trs_rpc_call_msg *)chan->base;
        struct trs_rpc_call_msg *rpc_msg = (struct trs_rpc_call_msg *)data;

        (void)memcpy_s(rpc_msg, sizeof(struct trs_rpc_call_msg), rpc_msg_back, sizeof(struct trs_rpc_call_msg));
        rpc_msg->rpc_call_header.len = 44; /* 44 is max mbox data msg len, tmp! */
    }

    ka_rmb();
    if ((header->result != 0) && (header->cmd_type != TRS_MBOX_RECYCLE_CHECK)) {
        trs_warn("Pay attention to result. (result=%u; tx_time_us=%llu; txdone_time_us=%llu; wakeup_time_us=%llu)\n",
                 header->result, chan->tx_time_us, chan->txdone_time_us, chan->wakeup_time_us);
    }
}

static u16 trs_mbox_chan_get_valid(struct trs_mbox_chan *chan)
{
    struct trs_mb_header *mbox_msg_header = (struct trs_mb_header *)chan->base;
    return mbox_msg_header->valid;
}

static int trs_mbox_chan_wait(struct trs_mbox_chan *chan, int timeout)
{
    if (timeout <= 0) {
        ka_task_wait_for_completion(&chan->tx_complete);
    } else {
        int ret = ka_task_wait_for_completion_timeout(&chan->tx_complete, ka_system_msecs_to_jiffies(timeout));
        if (ret == 0) {
            return -ETIMEDOUT;
        }
    }
    return 0;
}

void trs_mbox_chan_txdone(void *mbox_chan)
{
    struct trs_mbox_chan *chan = (struct trs_mbox_chan *)mbox_chan;

    if (chan != NULL) {
        chan->irq_cnt++;
        chan->txdone_time_us = trs_get_us_timestamp();
        ka_task_complete(&chan->tx_complete);
    }
}

static int trs_mbox_chan_trigger_irq(struct trs_mbox_chan *chan)
{
    int ret = -ENODEV;

    if (chan->ops.trigger_irq != NULL) {
        ret = chan->ops.trigger_irq(chan->priv);
    }
    return ret;
}

static bool trs_mbox_chan_is_available(struct trs_mbox_chan *chan)
{
    u32 status = TRS_INST_STATUS_NORMAL;
    int ret;

    ret = trs_get_ts_status(&chan->inst, &status);
    if ((ret != 0) || (status == TRS_INST_STATUS_ABNORMAL)) {
        return false;
    }

    if (chan->fault.cont_tx_timeout < chan->fault.cont_tx_timeout_max) {
        return true;
    }

    if (ka_system_jiffies_to_msecs(ka_jiffies - chan->fault.broken_timestamp) < chan->fault.retry_duration) {
        trs_err("Mbox chan not available. (devid=%u; tsid=%u; cur_jiffies=%lu; broken_timestamp=%lu; interval=%u(ms)\n",
                chan->inst.devid, chan->inst.tsid, ka_jiffies, chan->fault.broken_timestamp,
                ka_system_jiffies_to_msecs(ka_jiffies - chan->fault.broken_timestamp));
        return false;
    }

    chan->fault.cont_tx_timeout = 0;

    return true;
}

static void trs_mbox_chan_fault_record(struct trs_mbox_chan_fault *fault, int timeout)
{
    fault->cont_tx_timeout += timeout;

    if (fault->cont_tx_timeout >= fault->cont_tx_timeout_max) {
        fault->broken_timestamp = ka_jiffies;
    }
}

static void trs_mbox_chan_fault_clear(struct trs_mbox_chan_fault *fault)
{
    fault->cont_tx_timeout = 0;
}

static int trs_mbox_chan_send(struct trs_mbox_chan *chan, void *data, size_t size, int timeout)
{
    int ret;

    if (!trs_mbox_chan_is_available(chan)) {
        return -EIO;
    }

    if (size > chan->size) {
        trs_err("Send size invalid. (size=0x%lx; chan->size=0x%lx)\n", size, chan->size);
        return -EFAULT;
    }
    ka_base_reinit_completion(&chan->tx_complete);
    trs_mbox_chan_write(chan, data, size);
    ret = trs_mbox_chan_trigger_irq(chan);
    if (ret != 0) {
        trs_err("Trs mbox trigger irq fail. (ret=%d)\n", ret);
        return ret;
    }
    chan->tx_time_us = trs_get_us_timestamp();
    ret = trs_mbox_chan_wait(chan, timeout);
    chan->wakeup_time_us = trs_get_us_timestamp();
    if ((ret == 0) || (trs_mbox_chan_get_valid(chan) == 0)) {
        trs_mbox_chan_fault_clear(&chan->fault);
        chan->tx_cnt++;
        trs_mbox_chan_get_result(chan, data);
        return 0;
    } else {
        if (trs_mbox_chan_get_valid(chan) == 0xFFFF) { // timeout reply
            (void)trs_set_ts_status(&chan->inst, TRS_INST_STATUS_ABNORMAL);
        }
        trs_mbox_chan_fault_record(&chan->fault, timeout);
        trs_err("Mbox wait fail. (ret=%d; valid=0x%x; tx_time_us=%llu; txdone_time_us=%llu; wakeup_time_us=%llu)\n",
                ret, trs_mbox_chan_get_valid(chan), chan->tx_time_us, chan->txdone_time_us, chan->wakeup_time_us);
        return ret;
    }
}

static bool trs_is_mb_update_only(void *data)
{
    struct trs_mb_header *header = (struct trs_mb_header *)data;

    if (header->cmd_type == TRS_MBOX_FREE_STREAM) {
        return true;
    }
    return false;
}

int trs_hard_mbox_send(struct trs_id_inst *inst, u32 chan_id, void *data, size_t size, int timeout)
{
    struct trs_mbox_chan *chan = NULL;
    u32 status = TRS_INST_STATUS_NORMAL;
    int ret;

    if ((trs_id_inst_check(inst) != 0) || (data == NULL)) {
        return -EINVAL;
    }

    ret = trs_get_ts_status(inst, &status);
    if ((ret == 0) && (status == TRS_INST_STATUS_ABNORMAL)) {
        return -EIO;
    }

    ret = trs_mb_update(inst, (int)ka_task_get_current_tgid(), data, (u32)size);
    if (ret != 0) {
        return ret;
    }

    if (trs_is_mb_update_only(data)) {
        return 0;
    }

    ret = -ENXIO;
    chan = trs_mbox_chan_get(inst);
    if (chan != NULL) {
        ka_task_mutex_lock(&chan->mutex);
        ret = trs_mbox_chan_send(chan, data, size, timeout);
        ka_task_mutex_unlock(&chan->mutex);
        trs_mbox_chan_put(chan);
    }
    return ret;
}

int trs_hard_mbox_send_rpc_call_msg(struct trs_id_inst *inst, u32 chan_id, void *data, size_t size, int timeout)
{
    struct trs_mbox_chan *chan = NULL;
    int ret = -ENXIO;

    if ((trs_id_inst_check(inst) != 0) || (data == NULL)) {
        return -EINVAL;
    }

    chan = trs_mbox_chan_get(inst);
    if (chan != NULL) {
        ka_task_mutex_lock(&chan->mutex);
        ret = trs_mbox_chan_send(chan, data, size, timeout);
        ka_task_mutex_unlock(&chan->mutex);
        trs_mbox_chan_put(chan);
    }
    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_hard_mbox_send_rpc_call_msg);

int trs_mbox_get_chan_num(struct trs_id_inst *inst)
{
    /* Only one channel number for each tscpu */
    return 1;
}
KA_EXPORT_SYMBOL_GPL(trs_mbox_get_chan_num);
