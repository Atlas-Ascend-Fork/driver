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
#include "ka_base_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "securec.h"
#include "pbl/pbl_soc_res.h"

#include "pbl_uda.h"
#include "soc_adapt.h"
#include "comm_kernel_interface.h"
#include "trs_host_comm.h"
#include "trs_chip_def_comm.h"
#include "trs_mailbox_def.h"
#include "trs_mbox.h"
#include "trs_hard_mbox.h"
#include "trs_ts_db.h"
#include "trs_host_hard_mbox.h"
#include "trs_sia_adapt_auto_init.h"

#define TRS_MBXO_DEFAULT_TIMEOUT 3000
#define TRS_HOST_MB_RSV_MEM_OFFSET 64
#define TRS_MB_SIZE 64

struct trs_ts_mbox {
    struct trs_id_inst inst;

    ka_atomic_t retry;

    phys_addr_t base; // shared memory base
    size_t size;      // shared memory size

    u32 db_index;
    int db_type;

    char *name; // irq name
    u32 irq_type;
    u32 irq;   // ack irq
    u32 hwirq; // ack hwirq
    void *chan;

    u32 val; // value for wring to db
};

static ka_irqreturn_t trs_ts_mbox_ack_handler(int irq, void *data)
{
    struct trs_ts_mbox *ts_mbox = (struct trs_ts_mbox *)data;

    trs_mbox_chan_txdone(ts_mbox->chan);
    return KA_IRQ_HANDLED;
}

static int trs_ts_mbox_irq_setup(struct trs_ts_mbox *ts_mbox)
{
    struct trs_adapt_irq_attr attr;
    char *name = NULL;
    int ret;

    /* name allocated inside of kasprintf with kmalloc. name should be freed with kfree */
    name = ka_base_kasprintf(KA_GFP_KERNEL, "trs-mbox-%u-%u", ts_mbox->inst.devid, ts_mbox->inst.tsid);
    if (name == NULL) {
        trs_err("Irq name pack fail.\n");
        return -ENOMEM;
    }

    attr.irq_type = ts_mbox->irq_type;
    attr.irq = ts_mbox->irq;
    ret = trs_host_request_irq(&ts_mbox->inst, &attr, (void *)ts_mbox, name, trs_ts_mbox_ack_handler);
    if (ret != 0) {
        trs_kfree(name);
        trs_err("Request irq fail. (devid=%u; tsid=%u; irq=%u; ret=%d)\n", ts_mbox->inst.devid, ts_mbox->inst.tsid,
                ts_mbox->irq, ret);
        return ret;
    }

    ts_mbox->name = name;
    trs_debug("Mbox irq register. (devid=%u; tsid=%u; irq=%u; name=%s)\n", ts_mbox->inst.devid, ts_mbox->inst.tsid,
              ts_mbox->irq, name);
    return 0;
}

static void trs_ts_mbox_irq_cleanup(struct trs_ts_mbox *ts_mbox)
{
    struct trs_adapt_irq_attr attr;
    trs_info("Mbox irq unregister. (devid=%u, tsid=%u; irq=%u; name=%s)\n", ts_mbox->inst.devid, ts_mbox->inst.tsid,
             ts_mbox->irq, ts_mbox->name);

    attr.irq_type = ts_mbox->irq_type;
    attr.irq = ts_mbox->irq;
    trs_host_free_irq(&ts_mbox->inst, &attr, (void *)ts_mbox);
    if (ts_mbox->name != NULL) {
        trs_kfree(ts_mbox->name);
        ts_mbox->name = NULL;
    }
}

static int trs_ts_mbox_trigger_irq(void *data)
{
    struct trs_ts_mbox *ts_mbox = (struct trs_ts_mbox *)data;
    int ret = -ENODEV;

    if (ts_mbox != NULL) {
        ret = trs_ring_ts_db(&ts_mbox->inst, ts_mbox->db_type, ts_mbox->db_index, ++ts_mbox->val);
    }
    return ret;
}

static int trs_ts_mbox_setup(struct trs_ts_mbox *ts_mbox)
{
    int ret;

    ret = trs_ts_mbox_irq_setup(ts_mbox);
    if (ret != 0) {
        trs_err("Mbox irq setup fail. (ret=%d)\n", ret);
    }

    return ret;
}

static void trs_ts_mbox_cleanup(void *priv)
{
    struct trs_ts_mbox *ts_mbox = (struct trs_ts_mbox *)priv;
    trs_ts_mbox_irq_cleanup(ts_mbox);
}

static int trs_ts_mbox_db_init(struct trs_id_inst *inst)
{
    u32 start, end;
    int ret;

    ret = trs_soc_get_db_cfg(inst, TRS_DB_ONLINE_MBOX, &start, &end);
    if ((ret != 0) || (start >= end)) {
        trs_err("Trs get db cfg fail. (devid=%u; tsid=%u; ret=%d; start=%u; end=%u)\n", inst->devid, inst->tsid, ret,
                start, end);
        return -ENODEV;
    }

    ret = trs_ts_db_init(inst, TRS_DB_ONLINE_MBOX, start, end);
    if (ret != 0) {
        trs_err("Trs db init fail. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
    }

    return ret;
}

static void trs_ts_mbox_db_uninit(struct trs_id_inst *inst)
{
    trs_ts_db_uninit(inst, TRS_DB_ONLINE_MBOX);
}

static struct trs_ts_mbox *trs_ts_mbox_create(struct trs_id_inst *inst)
{
    struct trs_ts_mbox *ts_mbox = NULL;
    struct res_inst_info res_inst;
    struct soc_rsv_mem_info sram;
    u32 hwirq, irq;
    int ret;

    ts_mbox = trs_kzalloc(sizeof(struct trs_ts_mbox), KA_GFP_KERNEL);
    if (ts_mbox == NULL) {
        return NULL;
    }

    ret = trs_ts_mbox_db_init(inst);
    if (ret != 0) {
        trs_kfree(ts_mbox);
        return NULL;
    }

    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);
    ret = soc_resmng_get_rsv_mem(&res_inst, "TS_SRAM_MEM", &sram);
    ret |= soc_resmng_get_irq_by_index(&res_inst, TS_MAILBOX_ACK_IRQ, 0, &irq);
    ret |= soc_resmng_get_hwirq(&res_inst, TS_MAILBOX_ACK_IRQ, irq, &hwirq);
    if ((ret != 0) || (sram.rsv_mem_size < (TRS_HOST_MB_RSV_MEM_OFFSET + TRS_MB_SIZE))) {
        trs_ts_mbox_db_uninit(inst);
        trs_kfree(ts_mbox);
        trs_err("Get soc resmng fail. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return NULL;
    }
    ts_mbox->db_index = 0;
    ts_mbox->db_type = TRS_DB_ONLINE_MBOX;
    ts_mbox->inst = *inst;
    ts_mbox->irq = irq;
    ts_mbox->base = sram.rsv_mem + TRS_HOST_MB_RSV_MEM_OFFSET;
    ts_mbox->size = TRS_MB_SIZE;
    ts_mbox->val = 0;
    ts_mbox->hwirq = hwirq;
    ts_mbox->irq_type = TS_MAILBOX_ACK_IRQ;

    return ts_mbox;
}

static void trs_ts_mbox_destroy(struct trs_ts_mbox *ts_mbox)
{
    trs_ts_mbox_db_uninit(&ts_mbox->inst);
    trs_kfree(ts_mbox);
}

static void trs_ts_mbox_release(void *priv)
{
    struct trs_ts_mbox *ts_mbox = (struct trs_ts_mbox *)priv;

    trs_info("Trs mbox uninit. (devid=%u; tsid=%u)\n", ts_mbox->inst.devid, ts_mbox->inst.tsid);
    trs_ts_mbox_destroy(ts_mbox);
}

static enum trs_mbox_mem_type trs_mbox_get_mem_type(struct trs_id_inst *inst)
{
    enum trs_mbox_mem_type mem_type = TRS_MBOX_MEM_TYPE_DEVICE;
    u32 host_flag;

    if (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_HCCS) {
        if (!uda_is_phy_dev(inst->devid) || (devdrv_get_host_phy_mach_flag(inst->devid, &host_flag) != 0)) {
#ifndef EMU_ST
            trs_warn("Get host flag not support. (devid=%u)\n", inst->devid);
            return mem_type;
#endif
        }

        mem_type = (host_flag == DEVDRV_HOST_PHY_MACH_FLAG) ? TRS_MBOX_MEM_TYPE_NORMAL : TRS_MBOX_MEM_TYPE_DEVICE;
    }

    return mem_type;
}

static int trs_mbox_get_cont_tx_timeout_max(struct trs_id_inst *inst)
{
    int timeout = 9000; /* 9000 ms */

    if (devdrv_get_connect_protocol(inst->devid) == CONNECT_PROTOCOL_HCCS) {
        timeout = 45000; /* 45000 ms */
    }

    return timeout;
}

static void trs_mbox_chan_attr_pack(struct trs_ts_mbox *ts_mbox, struct trs_mbox_chan_attr *attr)
{
    attr->base = ts_mbox->base;
    attr->size = ts_mbox->size;
    attr->priv = ts_mbox;
    attr->ops.mbox_release = trs_ts_mbox_release;
    attr->ops.trigger_irq = trs_ts_mbox_trigger_irq;
    attr->ops.free_irq = trs_ts_mbox_cleanup;
    attr->mem_type = trs_mbox_get_mem_type(&ts_mbox->inst);
    attr->cont_tx_timeout_max = trs_mbox_get_cont_tx_timeout_max(&ts_mbox->inst);
    trs_info("devid=%u; attr->mem_type=%u; cont_tx_timeout_max=%u(ms)\n", ts_mbox->inst.devid, attr->mem_type,
             attr->cont_tx_timeout_max);
}

static int trs_ts_mbox_ack_irq_send(struct trs_ts_mbox *ts_mbox)
{
    struct trs_mbox_ack_irq_msg msg;
    int ret;

    trs_mbox_init_header(&msg.header, TRS_MBOX_NOTICE_ACK_IRQ_VALUE);
    msg.ack_irq = ts_mbox->hwirq;

    ret = trs_mbox_send(&ts_mbox->inst, 0, &msg, sizeof(struct trs_mbox_ack_irq_msg), 3000); /* timeout 3000 ms */
    if ((ret != 0) || (msg.header.result != 0)) {
        trs_err("Mbox send fail. (ret=%d; result=%d)\n", ret, msg.header.result);
        return -EFAULT;
    }

    return 0;
}

static struct trs_mbox_send_ops g_hard_mbox_ops[TRS_DEV_MAX_NUM];

int trs_mbox_config(struct trs_id_inst *inst)
{
    struct trs_ts_mbox *ts_mbox = NULL;
    struct trs_mbox_chan_attr attr;
    int ret;

    ts_mbox = trs_ts_mbox_create(inst);
    if (ts_mbox == NULL) {
        trs_err("Mbox create fail. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -ENODEV;
    }

    ret = trs_ts_mbox_setup(ts_mbox);
    if (ret != 0) {
        trs_ts_mbox_destroy(ts_mbox);
        trs_err("Mbox setup fail. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }

    trs_mbox_chan_attr_pack(ts_mbox, &attr);
    ts_mbox->chan = trs_mbox_chan_init(&ts_mbox->inst, &attr);
    if (ts_mbox->chan == NULL) {
        trs_ts_mbox_cleanup(ts_mbox);
        trs_ts_mbox_destroy(ts_mbox);
        trs_err("Mbox chan init fail. (devid=%u; tsid=%u)\n", inst->devid, inst->tsid);
        return -ENOMEM;
    }

    if (inst->tsid == 0) {
        g_hard_mbox_ops[inst->devid].mbox_send = trs_hard_mbox_send;
        g_hard_mbox_ops[inst->devid].mbox_send_rpc_call_msg = trs_hard_mbox_send_rpc_call_msg;
        trs_register_hard_mbox_send_ops(inst->devid, &g_hard_mbox_ops[inst->devid]);
    }

    ret = trs_ts_mbox_ack_irq_send(ts_mbox);
    if (ret != 0) {
        /* mbox chan uninit will clean ts_mbox and destroy it */
        if (inst->tsid == 0) {
            trs_unregister_hard_mbox_send_ops(inst->devid);
        }
        trs_mbox_chan_uninit(inst);
        trs_err("Mbox ack irq send fail. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(trs_mbox_config);

void trs_mbox_deconfig(struct trs_id_inst *inst)
{
    if (inst->tsid == 0) {
        trs_unregister_hard_mbox_send_ops(inst->devid);
    }
    trs_mbox_chan_uninit(inst);
}
KA_EXPORT_SYMBOL_GPL(trs_mbox_deconfig);

int trs_mbox_init(u32 ts_inst_id)
{
    struct trs_id_inst inst;
    int ret;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    ret = trs_mbox_config(&inst);
    if (ret != 0) {
        trs_err("Failed to config mailbox. (devid=%u; tsid=%u; ret=%d)\n", inst.devid, inst.tsid, ret);
    }
    return ret;
}
DECLAER_FEATURE_AUTO_INIT_DEV(trs_mbox_init, FEATURE_LOADER_STAGE_2);

void trs_mbox_uninit(u32 ts_inst_id)
{
    struct trs_id_inst inst;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    trs_mbox_deconfig(&inst);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(trs_mbox_uninit, FEATURE_LOADER_STAGE_2);

int devdrv_send_rdmainfo_to_ts(u32 devid, const u8 *buf, u32 len, int *result)
{
    struct trs_id_inst inst;
    struct trs_rdma_info msg = {0};
    u32 tsid = 0;
    int ret;

    if ((buf == NULL) || (result == NULL) || (len > MAX_RDMA_INFO_LEN)) {
        return -EINVAL;
    }

    trs_mbox_init_header(&msg.header, TRS_MBOX_SEND_RDMA_INFO);
    ret = memcpy_s(msg.buf, sizeof(msg.buf), buf, len);
    if (ret != EOK) {
        trs_err("memcpy fail. (devid=%u; tsid=%u; size=0x%lx, len=0x%x)\n", devid, tsid, sizeof(msg.buf), len);
        return ret;
    }

    trs_id_inst_pack(&inst, devid, tsid);
    ret = trs_mbox_send(&inst, 0, (void *)&msg, sizeof(msg), TRS_MBXO_DEFAULT_TIMEOUT);
    if ((ret != 0) || (msg.header.result != 0)) {
        trs_err("Trs mbox send fail. (devid=%u; tsid=%u; ret=%d; result=%d)\n", devid, tsid, ret, msg.header.result);
        return -EFAULT;
    }
    *result = 0;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_send_rdmainfo_to_ts);
