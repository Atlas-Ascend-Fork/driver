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
#include "ka_memory_pub.h"
#include "ka_barrier_pub.h"
#include "devdrv_dma.h"
#include "devdrv_ctrl.h"
#include "devdrv_common_msg.h"
#include "devdrv_msg.h"
#include "devdrv_pci.h"
#include "devdrv_msg_def.h"
#include "devdrv_util.h"
#include "nvme_adapt.h"
#include "comm_kernel_interface.h"
#include "pbl/pbl_spod_info.h"
#include "devdrv_s2s_msg.h"
#include "devdrv_adapt.h"
#include "pbl/pbl_uda.h"
#include "devdrv_mem_alloc.h"

#define SUPER_SERVER_BASE_ADDR 0x300000000000 // remote addr base

#define LOCAL_SERVER_BASE_ADDR 0x201054000000 // local addr base

STATIC s2s_msg_recv g_s2s_msg_recv_proc[DEVDRV_S2S_MSG_TYPE_MAX] = {
    NULL,
};
ka_rw_semaphore_t g_register_func_sem[DEVDRV_S2S_MSG_TYPE_MAX];

void devdrv_s2s_rwsem_init(void)
{
    int i = 0;

    for (i = 0; i < DEVDRV_S2S_MSG_TYPE_MAX; i++) {
        ka_task_init_rwsem(&g_register_func_sem[i]);
    }
}

STATIC u32 devdrv_get_s2s_host_chan_num(struct devdrv_pci_ctrl *pci_ctrl)
{
    return pci_ctrl->ops.get_max_server_num(pci_ctrl) * DEVDRV_S2S_MAX_CHIP_NUM * DEVDRV_S2S_DIE_NUM *
           DEVDRV_S2S_HOST_CHAN_EACH;
}

void devdrv_set_s2s_chan_pre_reset(u32 dev_id)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u32 chan_num, i;

    pci_ctrl = devdrv_get_bottom_half_pci_ctrl_by_id(dev_id);
    if ((pci_ctrl == NULL) || (pci_ctrl->msg_dev == NULL)) {
        devdrv_warn("Device is not online. (dev_id=%u)\n", dev_id);
        return;
    }

    if (pci_ctrl->msg_dev->s2s_chan == NULL) {
        devdrv_warn("s2s is not init. (dev_id=%u)\n", dev_id);
        return;
    }

    if ((pci_ctrl->addr_mode != DEVDRV_ADMODE_FULL_MATCH) || (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF)) {
        return;
    }

    chan_num = devdrv_get_s2s_host_chan_num(pci_ctrl);
    if (chan_num >= DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM) {
        devdrv_err("Channel number is out of range. (dev_id=%u; chan_num=%u; max_chan_num=%u)\n", dev_id, chan_num,
                   DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM);
        return;
    }

    for (i = 0; i < chan_num; i++) {
        if (pci_ctrl->msg_dev->s2s_chan[i].status == DEVDRV_S2S_NORMAL) {
            pci_ctrl->msg_dev->s2s_chan[i].status = DEVDRV_S2S_PRE_RESET;
        }
    }
}

int devdrv_pci_register_s2s_msg_proc_func(enum devdrv_s2s_msg_type msg_type, s2s_msg_recv func)
{
    if ((u32)msg_type >= DEVDRV_S2S_MSG_TYPE_MAX) {
        devdrv_err("Msg type is not supported yet. (msg_type=%d)\n", (int)msg_type);
        return -EOPNOTSUPP;
    }

    /* here change the value of g_s2s_msg_recv_proc, need to use write lock */
    ka_task_down_write(&g_register_func_sem[msg_type]);
    if (g_s2s_msg_recv_proc[msg_type] != NULL) {
        devdrv_err("Message type has been registered. (msg_type=%d)\n", (int)msg_type);
        ka_task_up_write(&g_register_func_sem[msg_type]);
        return -EINVAL;
    } else {
        g_s2s_msg_recv_proc[msg_type] = func;
    }
    ka_task_up_write(&g_register_func_sem[msg_type]);
    devdrv_info("register success.(msg_type=%u)\n", (u32)msg_type);

    return 0;
}

int devdrv_pci_unregister_s2s_msg_proc_func(enum devdrv_s2s_msg_type msg_type)
{
    if ((u32)msg_type >= DEVDRV_S2S_MSG_TYPE_MAX) {
        devdrv_err("Msg type is not support yet. (msg_type=%d)\n", (int)msg_type);
        return -EOPNOTSUPP;
    }

    /* here change the value of g_s2s_msg_recv_proc, need to use write lock */
    ka_task_down_write(&g_register_func_sem[msg_type]);
    g_s2s_msg_recv_proc[msg_type] = NULL;
    ka_task_up_write(&g_register_func_sem[msg_type]);
    devdrv_info("unregister success.(msg_type=%u)\n", (u32)msg_type);

    return 0;
}

STATIC void devdrv_fill_sq_data(struct devdrv_s2s_msg_chan *chan, enum devdrv_s2s_msg_type msg_type,
                                struct devdrv_s2s_msg_send_data_para *data_para, u32 in_len, struct spod_info s)
{
    chan->sq.data_buf->head_info.msg_type = (u32)msg_type;
    chan->sq.data_buf->head_info.buf_len = data_para->data_len;
    chan->sq.data_buf->head_info.in_len = in_len;
    chan->sq.data_buf->head_info.out_len = 0;
    chan->sq.data_buf->head_info.status = DEVDRV_MSG_CMD_BEGIN;
    chan->sq.data_buf->head_info.send_direction = data_para->direction;
    chan->sq.data_buf->head_info.sdid = s.sdid;
    chan->sq.data_buf->head_info.version = chan->version;
    chan->sq.data_buf->head_info.seq_num = chan->tx_seq_num;
    // desc recv remote side reply, need to set status here first
    chan->sq.desc->head_info.status = DEVDRV_MSG_CMD_BEGIN;
}

STATIC void devdrv_s2s_msg_chan_send_status_handle(struct devdrv_s2s_msg_chan *chan, enum devdrv_s2s_msg_type msg_type)
{
    int status = chan->sq.desc->head_info.status;

    if (status == DEVDRV_MSG_CMD_BEGIN) {
        devdrv_warn("s2s message send finish, no resp. (msg_type=%d; status=%d; devid=%u; chan_id=%u)\n", msg_type,
                    status, chan->devid, chan->chan_id);
    } else if (status == DEVDRV_MSG_CMD_INVALID_PARA) {
        devdrv_warn("s2s message send finish, para check invalid. (msg_type=%d; status=%d; devid=%u; chan_id=%u)\n",
                    msg_type, status, chan->devid, chan->chan_id);
    } else if (status == DEVDRV_MSG_CMD_CB_PROCESS_FAILED) {
        devdrv_warn("s2s message send finish, cb process abnormal. (msg_type=%d; status=%d; devid=%u; chan_id=%u)\n",
                    msg_type, status, chan->devid, chan->chan_id);
    } else if (status == DEVDRV_MSG_CMD_NULL_PROCESS_CB) {
        devdrv_warn("s2s message send finish, no callback. (msg_type=%d; status=%d; devid=%u; chan_id=%u)\n", msg_type,
                    status, chan->devid, chan->chan_id);
    } else if (status == DEVDRV_MSG_CMD_FINISH_FAILED) {
        devdrv_warn("s2s message send finish, need check remote flow. (msg_type=%d; status=%d; devid=%u; chan_id=%u)\n",
                    msg_type, status, chan->devid, chan->chan_id);
    } else if (status == DEVDRV_MSG_CMD_SEND_HOST_FAILED) {
        devdrv_warn("s2s message send to remote host unsuccessful. (msg_type=%d; status=%d; devid=%u; chan_id=%u)\n",
                    msg_type, status, chan->devid, chan->chan_id);
    } else if (status == DEVDRV_MSG_CMD_COPY_FAILED) {
        devdrv_warn("s2s message sdma copy to remote unsuccessful. (msg_type=%d; status=%d; devid=%u; chan_id=%u)\n",
                    msg_type, status, chan->devid, chan->chan_id);
    } else {
        devdrv_warn("s2s message send finish. (msg_type=%d; status=%d; devid=%u; chan_id=%u)\n", msg_type, status,
                    chan->devid, chan->chan_id);
    }
}

STATIC int devdrv_s2s_msg_chan_ack_data(u32 devid, struct devdrv_s2s_msg_chan *chan, void *data, u32 data_len,
                                        u32 *out_len)
{
    int ret;

    *out_len = chan->sq.desc->head_info.out_len;
    if (*out_len >= (DEVDRV_S2S_HOST_MSG_SIZE - DEVDRV_S2S_MSG_HEAD_LEN) || (*out_len > data_len)) {
        devdrv_err("s2s out len is out of range.(devid=%u; chan_id=%u; data_len=%u; *out_len=%u)\n", devid,
                   chan->chan_id, data_len, *out_len);
        return -EIO;
    }

    if (*out_len > 0) {
        if (chan->msg_dev->pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) {
            ret = devdrv_dma_sync_copy_inner(devid, DEVDRV_DMA_DATA_PCIE_MSG, chan->sq_phy_addr, chan->sq.ack_addr,
                                             *out_len + DEVDRV_S2S_MSG_HEAD_LEN, DEVDRV_DMA_SYS_TO_SYS);
            if (ret != 0) {
                devdrv_err("s2s ack message recv failed.(devid=%u; chan_id=%u; ret=%d)\n", devid, chan->chan_id, ret);
                return -EIO;
            }

            ret = memcpy_s(data, data_len, (void *)chan->sq.ack_buf->data, *out_len);
            if (ret != 0) {
                devdrv_err("memcpy_s failed.(devid=%u; chan_id=%u; ret=%d)\n", devid, chan->chan_id, ret);
                return -EIO;
            }
        } else {
            ret = memcpy_s(data, data_len, (void *)chan->sq.desc->data, *out_len);
            if (ret != 0) {
                devdrv_err("memcpy_s failed.(devid=%u; chan_id=%u;ret=%d)\n", devid, chan->chan_id, ret);
                return -EIO;
            }
        }
    }

    return 0;
}

STATIC int devdrv_s2s_submit_sq(u32 devid, u32 sdid, struct devdrv_s2s_msg_chan *chan, u32 in_len)
{
    struct devdrv_s2s_non_trans_msg msg = {0};
    struct devdrv_s2s_sdma_msg *sdma_msg = &msg.msg_data.sdma_msg;
    void *non_trans_chan = NULL;
    u32 real_out_len;
    int ret;

    msg.msg_type = DEVDRV_S2S_NON_TRANS_SDMA_TYPE;
    sdma_msg->in_len = in_len;
    sdma_msg->host_dev_id = devid;
    sdma_msg->dst_sdid = sdid;
    sdma_msg->chan_id = chan->chan_id;
    non_trans_chan = devdrv_get_s2s_non_trans_chan(chan->msg_dev);

    ret = devdrv_sync_msg_send(non_trans_chan, &msg, sizeof(struct devdrv_s2s_non_trans_msg),
                               sizeof(struct devdrv_s2s_non_trans_msg), &real_out_len);
    if (ret != 0) {
        devdrv_err("Submit sq failed.(ret=%d; real_out_len=%u; msg_ret=%d)\n", ret, real_out_len, msg.ret);
        return -EINVAL;
    }

    return 0;
}

STATIC bool devdrv_is_s2s_msg_need_retry(struct devdrv_s2s_msg_chan *chan, u32 retry_cnt)
{
    if (chan->sq.desc->head_info.status == DEVDRV_MSG_CMD_BEGIN) {
        return false;
    }

    if (chan->sq.desc->head_info.version == DEVDRV_S2S_MSG_VERSION_0) {
        return false; /* Not support seq_num, don't need retry */
    }

    if ((chan->tx_seq_num != chan->sq.desc->head_info.seq_num) && (retry_cnt < DEVDRV_MSG_RETRY_LIMIT)) {
        return true;
    }

    return false;
}

STATIC int devdrv_s2s_msg_chan_send(u32 devid, u32 sdid, struct devdrv_s2s_msg_chan *chan,
                                    enum devdrv_s2s_msg_type msg_type, struct devdrv_s2s_msg_send_data_para *data_para)
{
    int timeout = (chan->status == DEVDRV_S2S_PRE_RESET) ? DEVDRV_S2S_MSG_TMOUT_SHORT : DEVDRV_S2S_MSG_TMOUT;
    u32 max_data_len = DEVDRV_S2S_HOST_MSG_SIZE - DEVDRV_S2S_MSG_HEAD_LEN;
    u32 in_len = data_para->in_len;
    int ret, status = 0;
    u32 retry_cnt = 0;
    struct spod_info s;

    ret = dbl_get_spod_info(devid, &s);
    if (ret != 0) {
        devdrv_err("dbl_get_spod_info failed.(dev_id=%u; ret=%d)\n", devid, ret);
        return ret;
    }
    chan->tx_seq_num++;

s2s_retry:
    devdrv_fill_sq_data(chan, msg_type, data_para, in_len, s);
    if ((in_len <= 0) ||
        (memcpy_s((void *)chan->sq.data_buf + DEVDRV_S2S_MSG_HEAD_LEN, max_data_len, data_para->data, in_len) != 0)) {
        devdrv_err("in_len is invalid or memcpy_s failed.(dev_id=%u; in_len=%u)\n", devid, in_len);
        return -EINVAL;
    }

    ret = devdrv_s2s_submit_sq(devid, sdid, chan, in_len);
    if (ret != 0) {
        devdrv_err("Submit s2s sq fail.(ret=%d; devid=%u; dst_sdid=%u; msg_type=%u)\n", ret, devid, sdid, msg_type);
        return ret;
    }

    if (data_para->msg_mode == DEVDRV_S2S_ASYNC_MODE) {
        return 0;
    }

    /* wait result */
    while (timeout > 0) {
        status = chan->sq.desc->head_info.status;
        if (status != DEVDRV_MSG_CMD_BEGIN) {
            break;
        }

        if (chan->msg_dev->pci_ctrl->is_sriov_enabled == DEVDRV_SRIOV_ENABLE) {
            devdrv_info("Sriov enabled, s2s break.(devid=%u;msg_type=%d)\n", chan->devid, msg_type);
            return -EPERM;
        }

        if ((chan->msg_dev->pci_ctrl->device_status == DEVDRV_DEVICE_DEAD) ||
            (chan->msg_dev->pci_ctrl->device_status == DEVDRV_DEVICE_UDA_RM)) {
            devdrv_info("Device is abnormal. (devid=%u;msg_type=%d)\n", chan->devid, msg_type);
            return -ENODEV;
        }

        if ((chan->status == DEVDRV_S2S_PRE_RESET) && (timeout > DEVDRV_S2S_MSG_TMOUT_SHORT)) {
            timeout = DEVDRV_S2S_MSG_TMOUT_SHORT;
        }

        ka_rmb();
        ka_system_usleep_range(DEVDRV_MSG_WAIT_MIN_TIME, DEVDRV_MSG_WAIT_MAX_TIME);
        timeout -= DEVDRV_MSG_WAIT_MIN_TIME;
    }
    ka_mb();

    if (devdrv_is_s2s_msg_need_retry(chan, retry_cnt)) {
        retry_cnt++;
        goto s2s_retry;
    }

    if (status != DEVDRV_MSG_CMD_FINISH_SUCCESS) {
        devdrv_s2s_msg_chan_send_status_handle(chan, msg_type);
        devdrv_err("Get finish status remain time.(dev_id=%u; dst_sdid=%u; status=%d; time=%d(us))\n", devid, sdid,
                   status, timeout);
#ifdef CFG_BUILD_DEBUG
        ka_base_dump_stack();
#endif
        return -ENOSYS;
    }

    ret = devdrv_s2s_msg_chan_ack_data(devid, chan, data_para->data, data_para->data_len, &data_para->out_len);
    if (ret != 0) {
        devdrv_err("S2S ack data fail.(ret=%d; devid=%u; dst_sdid=%u; msg_type=%u)\n", ret, devid, sdid, msg_type);
        return ret;
    }

    return 0;
}

STATIC u32 devdrv_calc_s2s_chan_id(u32 dst_server_id, u32 dst_chip_id, u32 dst_die_id,
                                   enum devdrv_s2s_msg_type msg_type)
{
    /* DEVMM use channel N+0, others use channel N+1 */
    return (dst_server_id * DEVDRV_S2S_MAX_CHIP_NUM * DEVDRV_S2S_DIE_NUM + dst_chip_id * DEVDRV_S2S_DIE_NUM +
            dst_die_id) *
               DEVDRV_S2S_HOST_CHAN_EACH +
           (msg_type == DEVDRV_S2S_MSG_DEVMM ? 0 : 1);
}

STATIC void devdrv_fill_s2s_data_para(struct devdrv_s2s_msg_send_data_para *data_para,
                                      struct data_input_info *data_info, u32 direction)
{
    data_para->data = data_info->data;
    data_para->data_len = data_info->data_len;
    data_para->in_len = data_info->in_len;
    data_para->out_len = data_info->out_len;
    data_para->msg_mode = data_info->msg_mode;
    data_para->direction = direction;
}

STATIC int devdrv_s2s_set_and_check_para(u32 devid, enum devdrv_s2s_msg_type msg_type, u32 direction,
                                         struct data_input_info *data_info,
                                         struct devdrv_s2s_msg_send_data_para *data_para)
{
    u32 max_data_len = DEVDRV_S2S_HOST_MSG_SIZE - sizeof(struct devdrv_s2s_msg);

    if (msg_type >= DEVDRV_S2S_MSG_TYPE_MAX) {
        devdrv_err("Msg type is not support yet. (devid=%u;msg_type=%u)\n", devid, (u32)msg_type);
        return -EOPNOTSUPP;
    }

    if (data_info == NULL) {
        devdrv_err("data_info is NULL.(devid=%u)\n", devid);
        return -EINVAL;
    }

    devdrv_fill_s2s_data_para(data_para, data_info, direction);

    if ((data_para->data_len > max_data_len) || (data_para->in_len > max_data_len)) {
        devdrv_err("buf_len or in_len overflow.(devid=%u; max_data_len=%u, buf_len=%u; in_len=%d)\n", devid,
                   max_data_len, data_para->data_len, data_para->in_len);
        return -EINVAL;
    }

    if (data_para->data == NULL) {
        devdrv_err("Data is null. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((data_para->direction != DEVDRV_S2S_TO_DEVICE) && (data_para->direction != DEVDRV_S2S_TO_HOST)) {
        devdrv_err("Trans direction is invalid.(devid=%u; direction=%u)\n", devid, data_para->direction);
        return -EINVAL;
    }

    if (devid >= DEVDRV_S2S_MAX_DEV_NUM) {
        devdrv_err("Invalid dev_id.(devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((data_para->msg_mode != DEVDRV_S2S_SYNC_MODE) && (data_para->msg_mode != DEVDRV_S2S_ASYNC_MODE)) {
        devdrv_err("Message mode is invalid. (devid=%u; msg_mode=%u)\n", devid, data_para->msg_mode);
        return -EINVAL;
    }

    return 0;
}

STATIC struct devdrv_s2s_msg_chan *devdrv_find_s2s_chan(u32 devid, u32 sdid, struct devdrv_pci_ctrl *pci_ctrl,
                                                        enum devdrv_s2s_msg_type msg_type)
{
    struct devdrv_s2s_msg_chan *chan = NULL;
    struct sdid_parse_info sdid_info;
    u32 chan_id;
    int ret;

    if (pci_ctrl->addr_mode != DEVDRV_ADMODE_FULL_MATCH) {
        devdrv_err("Not full match, can not use s2s channel.(dev_id=%u)\n", devid);
        return NULL;
    }

    if (pci_ctrl->device_status != DEVDRV_DEVICE_ALIVE) {
        devdrv_err("Pci_ctrl device_status not alive.(dev_id=%u)\n", devid);
        return NULL;
    }

    if (pci_ctrl->is_sriov_enabled == DEVDRV_SRIOV_ENABLE) {
        devdrv_warn("Sriov enabled, can not use s2s channel.(dev_id=%u)\n", devid);
        return NULL;
    }

    if (pci_ctrl->msg_dev == NULL) {
        devdrv_err("msg_dev is null.(devid=%u)\n", devid);
        return NULL;
    }

    ret = dbl_parse_sdid(sdid, &sdid_info);
    if (ret != 0) {
        devdrv_err("dbl_parse_sdid failed.(sdid=%u; ret=%d)\n", sdid, ret);
        return NULL;
    }

    /* check sdid by parse info and check server id, chip id and die_id */
    if ((sdid_info.server_id >= DEVDRV_S2S_MAX_SERVER_NUM) || (sdid_info.chip_id >= DEVDRV_S2S_MAX_CHIP_NUM) ||
        (sdid_info.die_id >= DEVDRV_S2S_DIE_NUM) || (sdid_info.udevid >= DEVDRV_S2S_MAX_UDEVID_NUM) ||
        ((sdid_info.chip_id * DEVDRV_S2S_DIE_NUM + sdid_info.die_id) != sdid_info.udevid)) {
        devdrv_err("sdid parse info is invalid.(server_id=%u; chip_id=%u; die_id=%u; udevid=%u)\n", sdid_info.server_id,
                   sdid_info.chip_id, sdid_info.die_id, sdid_info.udevid);
        return NULL;
    }

    chan_id = devdrv_calc_s2s_chan_id(sdid_info.server_id, sdid_info.chip_id, sdid_info.die_id, msg_type);
    chan = &pci_ctrl->msg_dev->s2s_chan[chan_id];
    if (chan->valid == DEVDRV_DISABLE) {
        devdrv_err("Channel not ready or has been uninit.(devid=%u; chan_id=%u)\n", devid, chan_id);
        return NULL;
    }

    return chan;
}

STATIC int devdrv_s2s_msg_send_lock(struct devdrv_s2s_msg_chan *chan, u32 msg_mode)
{
    int ret = 0;

    if (msg_mode == DEVDRV_S2S_ASYNC_MODE) {
        ret = ka_task_down_trylock(&chan->sema);
        if (ret != 0) {
            return -EBUSY;
        }
        chan->msg_mode = DEVDRV_S2S_ASYNC_MODE;
    } else {
        ka_task_down(&chan->sema);
        chan->msg_mode = DEVDRV_S2S_SYNC_MODE;
    }
    return ret;
}

STATIC void devdrv_s2s_msg_send_unlock(struct devdrv_s2s_msg_chan *chan, u32 msg_mode, int ret)
{
    if ((ret != 0) || (msg_mode == DEVDRV_S2S_SYNC_MODE)) {
        chan->msg_mode = DEVDRV_S2S_IDLE_MODE;
        ka_task_up(&chan->sema);
    }
}

int devdrv_pci_s2s_msg_send(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type, u32 direction,
                            struct data_input_info *data_info)
{
    struct devdrv_s2s_msg_send_data_para data_para = {0};
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_s2s_msg_chan *chan = NULL;
    int ret;

    pci_ctrl = devdrv_pci_ctrl_get(devid);
    if (pci_ctrl == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("Get pci_ctrl in hot_reset.(dev_id=%u)\n", devid);
        } else {
            devdrv_err_limit("Get pci_ctrl failed.(dev_id=%u)\n", devid);
        }
        return -EINVAL;
    }

    if (!devdrv_feature_is_support(pci_ctrl->features, DEVDRV_FEATURE_S2S)) {
        ret = -EOPNOTSUPP;
        devdrv_err("s2s feature is not support. (devid=%u; dst_sdid=%u; msg_type=%u; ret=%d)\n", devid, sdid,
                   (u32)msg_type, ret);
        return ret;
    }

    ret = devdrv_s2s_set_and_check_para(devid, msg_type, direction, data_info, &data_para);
    if (ret != 0) {
        devdrv_err("s2s para check failed. (devid=%u; dst_sdid=%u; msg_type=%u; ret=%d)\n", devid, sdid, (u32)msg_type,
                   ret);
        devdrv_pci_ctrl_put(pci_ctrl);
        return ret;
    }

    chan = devdrv_find_s2s_chan(devid, sdid, pci_ctrl, msg_type);
    if (chan == NULL) {
        devdrv_err("Find s2s chan failed. (devid=%u; dst_sdid=%u; msg_type=%u)\n", devid, sdid, (u32)msg_type);
        devdrv_pci_ctrl_put(pci_ctrl);
        return -EINVAL;
    }

    ret = devdrv_s2s_msg_send_lock(chan, data_para.msg_mode);
    if (ret != 0) {
        devdrv_pci_ctrl_put(pci_ctrl);
        return ret;
    }

    ret = devdrv_s2s_msg_chan_send(devid, sdid, chan, msg_type, &data_para);
    if (ret != 0) {
        devdrv_err("s2s send failed.(dev_id=%u; dst_sdid=%u; msg_type=%u; ret=%d; data_len=%u; out_len=%u)\n", devid,
                   sdid, msg_type, ret, data_info->data_len, data_info->out_len);
    }

    if (data_para.msg_mode == DEVDRV_S2S_SYNC_MODE) {
        data_info->out_len = data_para.out_len;
    }

    devdrv_s2s_msg_send_unlock(chan, data_para.msg_mode, ret);
    devdrv_pci_ctrl_put(pci_ctrl);

    return ret;
}

STATIC int devdrv_s2s_check_recv_para(u32 devid, enum devdrv_s2s_msg_type msg_type, struct data_recv_info *data_info)
{
    if (msg_type >= DEVDRV_S2S_MSG_TYPE_MAX) {
        devdrv_err("Msg type is not support yet. (devid=%u;msg_type=%u)\n", devid, (u32)msg_type);
        return -EOPNOTSUPP;
    }

    if (data_info == NULL) {
        devdrv_err("Function agentdrv_s2s_msg_send parameters data_info is NULL(devid=%u).\n", devid);
        return -EINVAL;
    }

    if (data_info->data == NULL) {
        devdrv_err("Input parameter is null. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if ((data_info->flag != DEVDRV_S2S_KEEP_RECV) && (data_info->flag != DEVDRV_S2S_END_RECV)) {
        devdrv_err("Message mode is invalid. (devid=%u; flag=%u)\n", devid, data_info->flag);
        return -EINVAL;
    }

    return 0;
}

int devdrv_pci_s2s_async_msg_recv(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type,
                                  struct data_recv_info *data_info)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_s2s_msg_chan *chan = NULL;
    int ret = 0;
    u32 status;

    pci_ctrl = devdrv_pci_ctrl_get(devid);
    if (pci_ctrl == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("Get pci_ctrl in hot_reset.(dev_id=%u)\n", devid);
        } else {
            devdrv_err_limit("Get pci_ctrl failed.(dev_id=%u)\n", devid);
        }
        return -EINVAL;
    }

    if (!devdrv_feature_is_support(pci_ctrl->features, DEVDRV_FEATURE_S2S)) {
        ret = -EOPNOTSUPP;
        devdrv_err("s2s feature is not support. (devid=%u; dst_sdid=%u; msg_type=%u; ret=%d)\n", devid, sdid,
                   (u32)msg_type, ret);
        return ret;
    }

    ret = devdrv_s2s_check_recv_para(devid, msg_type, data_info);
    if (ret != 0) {
        devdrv_err("s2s recv para check failed. (dev_id=%u; dst_sdid=%u; msg_type=%u; ret=%d)\n", devid, sdid,
                   (u32)msg_type, ret);
        devdrv_pci_ctrl_put(pci_ctrl);
        return ret;
    }

    chan = devdrv_find_s2s_chan(devid, sdid, pci_ctrl, msg_type);
    if (chan == NULL) {
        devdrv_err("Find s2s chan failed. (devid=%u; dst_sdid=%u; msg_type=%u)\n", devid, sdid, (u32)msg_type);
        devdrv_pci_ctrl_put(pci_ctrl);
        return -EINVAL;
    }

    if (chan->msg_mode != DEVDRV_S2S_ASYNC_MODE) {
        devdrv_err("Channel msg_mode is invalid. (devid=%u; dst_sdid=%u; msg_type=%u; msg_mode=%u)\n", devid, sdid,
                   (u32)msg_type, chan->msg_mode);
        devdrv_pci_ctrl_put(pci_ctrl);
        return -EINVAL;
    }

    status = chan->sq.desc->head_info.status;
    if ((status == DEVDRV_MSG_CMD_BEGIN) && (data_info->flag == DEVDRV_S2S_KEEP_RECV)) {
        devdrv_pci_ctrl_put(pci_ctrl);
        return -EAGAIN;
    }

    if (status != DEVDRV_MSG_CMD_FINISH_SUCCESS) {
        devdrv_s2s_msg_chan_send_status_handle(chan, msg_type);
        devdrv_err("Get finish status remain time.(dev_id=%u; dst_sdid=%u; msg_type=%u; status=%d)\n", devid, sdid,
                   (u32)msg_type, status);
        ret = -ENOSYS;
        goto msg_exit;
    }

    ret = devdrv_s2s_msg_chan_ack_data(devid, chan, data_info->data, data_info->data_len, &data_info->out_len);
    if (ret != 0) {
        devdrv_err("S2s ack data fail.(ret=%d; devid=%u; dst_sdid=%u; msg_type=%u)\n", ret, devid, sdid, msg_type);
    }

msg_exit:
    chan->msg_mode = DEVDRV_S2S_IDLE_MODE;
    ka_task_up(&chan->sema);
    devdrv_pci_ctrl_put(pci_ctrl);

    return ret;
}

STATIC phys_addr_t devdrv_s2s_get_msg_cq_addr(u32 dev_id, u32 chan_id)
{
    u64 base_addr, msg_size;
    u32 chan_id_use;

    if (chan_id >= DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM_DEV) {
        base_addr = DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM_DEV * DEVDRV_S2S_DEVICE_MSG_SIZE;
        msg_size = DEVDRV_S2S_HOST_MSG_SIZE;
        chan_id_use = chan_id - DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM_DEV;
    } else {
        base_addr = 0;
        msg_size = DEVDRV_S2S_DEVICE_MSG_SIZE;
        chan_id_use = chan_id;
    }

    return DEVDRV_LOCAL_SUPER_NODE_START_ADDR + dev_id * DEVDRV_S2S_SERVER_ADDR_OFFSET + base_addr +
           chan_id_use * msg_size + DEVDRV_S2S_RECV_MSG_ADDR_OFFSET;
}

STATIC int devdrv_s2s_msg_edge_check(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *p_real_out_len)
{
    u32 max_data_len = DEVDRV_S2S_NON_TRANS_MSG_DESC_SIZE - DEVDRV_NON_TRANS_MSG_HEAD_LEN;
    struct devdrv_s2s_msg *recv_msg = NULL;
    phys_addr_t cq_phy_addr;

    if ((devid >= DEVDRV_S2S_MAX_DEV_NUM) || (data == NULL) || (p_real_out_len == NULL)) {
        devdrv_err("Input parameter is invalid. (devid=%u;data_is_null=%d;p_real_out_len_is_null=%d;)\n", devid,
                   (data == NULL), (p_real_out_len == NULL));
        return -EINVAL;
    }

    if (in_data_len < sizeof(struct devdrv_s2s_msg) || in_data_len > max_data_len) {
        devdrv_err("Input parameter in_data_len is error.(devid=%u, in_data_len=0x%x)\n", devid, in_data_len);
        return -EINVAL;
    }

    recv_msg = (struct devdrv_s2s_msg *)data;
    if (recv_msg->head_info.msg_type >= (u32)DEVDRV_S2S_MSG_TYPE_MAX) {
        devdrv_err("S2S message channel recv message type is not support yet. (dev_id=%u;msg_type=%u)\n", devid,
                   recv_msg->head_info.msg_type);
        return -EOPNOTSUPP;
    }

    if (recv_msg->head_info.chan_id >= DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM) {
        devdrv_err("Chan_id is invalid.(devid=%u; chan_id=%u)\n", devid, recv_msg->head_info.chan_id);
        return -EINVAL;
    }

    cq_phy_addr = devdrv_s2s_get_msg_cq_addr(devid, recv_msg->head_info.chan_id);
    if (recv_msg->head_info.cq_phy_addr != cq_phy_addr) {
        devdrv_err("Cq info is error.(devid=%u; chan_id=%u)\n", devid, recv_msg->head_info.chan_id);
        return -EINVAL;
    }

    return 0;
}

int devdrv_s2s_non_trans_msg_recv(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    enum devdrv_dma_data_type data_type = DEVDRV_DMA_DATA_PCIE_MSG;
    u32 devid = (u32)devdrv_get_msg_chan_devid_inner(msg_chan);
    struct devdrv_msg_chan *non_trans_chan = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    struct devdrv_s2s_msg *recv_msg = NULL;
    struct data_input_info data_info;
    ka_dma_addr_t data_dma_addr;
    u32 msg_type;
    u64 call_start;
    u32 resq_time;
    int ret;

    if (devdrv_s2s_msg_edge_check(devid, data, in_data_len, out_data_len, real_out_len) != 0) {
        devdrv_err("Input parameters check failed.(devid=%u)\n", devid);
        return -EINVAL;
    }
    *real_out_len = 0;
    recv_msg = (struct devdrv_s2s_msg *)data;
    msg_type = recv_msg->head_info.msg_type;

    pci_ctrl = devdrv_pci_ctrl_get(devid);
    if (pci_ctrl == NULL) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("Get pci_ctrl unsuccessful.(dev_id=%u)\n", devid);
        } else {
            devdrv_err_limit("Get pci_ctrl failed.(dev_id=%u)\n", devid);
        }
        return -EINVAL;
    }

    if (pci_ctrl->device_status != DEVDRV_DEVICE_ALIVE) {
        if (devdrv_is_dev_hot_reset() == true) {
            devdrv_warn_limit("Pci_ctrl device_status not alive.(dev_id=%u)\n", devid);
        } else {
            devdrv_err_limit("Pci_ctrl device_status not alive.(dev_id=%u)\n", devid);
        }
        devdrv_pci_ctrl_put(pci_ctrl);
        return -EINVAL;
    }

    ka_task_down_read(&g_register_func_sem[msg_type]);
    if (g_s2s_msg_recv_proc[msg_type] != NULL) {
        data_info.data = recv_msg->data;
        data_info.data_len = recv_msg->head_info.buf_len;
        data_info.in_len = recv_msg->head_info.in_len;
        call_start = ka_jiffies;
        ret = g_s2s_msg_recv_proc[msg_type](devid, recv_msg->head_info.sdid, &data_info);
        resq_time = ka_system_jiffies_to_msecs(ka_jiffies - call_start);
        if (resq_time > DEVDRV_S2S_CB_TIME) {
            devdrv_info("Get resq_time. (dev_id=%u;msg_type=%u;resq_time=%ums;cpu=%d)\n", devid, msg_type, resq_time,
                        ka_system_raw_smp_processor_id());
        }
        ka_task_up_read(&g_register_func_sem[msg_type]);

        recv_msg->head_info.out_len = data_info.out_len;
        if ((ret != 0) || (recv_msg->head_info.out_len > recv_msg->head_info.buf_len) ||
            (recv_msg->head_info.out_len > DEVDRV_S2S_HOST_MSG_SIZE - sizeof(struct devdrv_s2s_msg))) {
            devdrv_err("S2S msg process failed.(devid=%u; msg_type=%u; out_len=%u; buf_len=%u; in_len=%u; ret=%d)\n",
                       devid, msg_type, recv_msg->head_info.out_len, recv_msg->head_info.buf_len,
                       recv_msg->head_info.in_len, ret);
            recv_msg->head_info.status = DEVDRV_MSG_CMD_CB_PROCESS_FAILED;
            devdrv_pci_ctrl_put(pci_ctrl);
            return -EINVAL;
        }
    } else {
        ka_task_up_read(&g_register_func_sem[msg_type]);
        devdrv_err("s2s message channel recv message type not register.(devid=%u; msg_type=%u)\n", devid, msg_type);
        recv_msg->head_info.status = DEVDRV_MSG_CMD_NULL_PROCESS_CB;
        devdrv_pci_ctrl_put(pci_ctrl);
        return -EINVAL;
    }

    non_trans_chan = devdrv_find_msg_chan(msg_chan);
    if (non_trans_chan == NULL) {
        recv_msg->head_info.status = DEVDRV_MSG_CMD_INVALID_PARA;
        devdrv_pci_ctrl_put(pci_ctrl);
        return -EINVAL;
    }
    data_dma_addr = non_trans_chan->cq_info.dma_handle + ka_offsetof(struct devdrv_non_trans_msg_desc, data);

    recv_msg->head_info.status = DEVDRV_MSG_CMD_FINISH_SUCCESS;
    ret = devdrv_dma_sync_copy_inner(
        devid, data_type, data_dma_addr + sizeof(u32), recv_msg->head_info.cq_dma_addr + sizeof(u32),
        recv_msg->head_info.out_len + DEVDRV_S2S_MSG_HEAD_LEN - sizeof(u32), DEVDRV_DMA_HOST_TO_DEVICE);
    if (ret != 0) {
        devdrv_err("s2s message channel recv reply failed.(devid=%u; msg_type=%u; ret=%d)\n", devid, msg_type, ret);
        recv_msg->head_info.status = DEVDRV_MSG_CMD_FINISH_FAILED;
    }

    /* respond to host side to execute result status */
    ret = devdrv_dma_sync_copy_inner(devid, data_type, data_dma_addr, recv_msg->head_info.cq_dma_addr, sizeof(u32),
                                     DEVDRV_DMA_HOST_TO_DEVICE);
    if (ret != 0) {
        devdrv_err("s2s msg_chan recv reply status fail.(devid=%u; msg_type=%u; ret=%d)\n", devid, msg_type, ret);
    }

    devdrv_pci_ctrl_put(pci_ctrl);
    return ret;
}

STATIC struct devdrv_non_trans_msg_chan_info devdrv_s2s_non_trans_msg_chan_info = {
    .msg_type = devdrv_msg_client_s2s,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_HIGH,
    .s_desc_size = DEVDRV_S2S_NON_TRANS_MSG_DESC_SIZE,
    .c_desc_size = DEVDRV_S2S_NON_TRANS_MSG_DESC_SIZE,
    .rx_msg_process = devdrv_s2s_non_trans_msg_recv,
};

STATIC u64 devdrv_s2s_base_addr_get(u32 server_id, u32 chip_id, u32 die_id)
{
    return (server_id * DEVDRV_S2S_SERVER_MEM_SIZE + // server mem offset
            chip_id * DEVDRV_S2S_CHIP_MEM_SIZE +     // chip mem offset
            die_id * DEVDRV_S2S_DEV_MEM_SIZE +       // die mem offset
            SUPER_SERVER_BASE_ADDR);                 // base offset
}

STATIC u64 devdrv_s2s_get_dma_addr(struct sdid_parse_info src_sdid, struct sdid_parse_info dst_sdid, u64 offset)
{
    u64 addr_base;

    // get huge offset
    addr_base = devdrv_s2s_base_addr_get(dst_sdid.server_id, dst_sdid.chip_id, dst_sdid.die_id);

    // get chan start addr in die
    addr_base = addr_base + offset +
                (src_sdid.server_id * DEVDRV_S2S_MAX_DEV_NUM + src_sdid.chip_id * DEVDRV_S2S_DIE_NUM +
                 src_sdid.die_id) *
                    DEVDRV_S2S_HOST_MSG_SIZE +
                DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM_DEV * DEVDRV_S2S_DEVICE_MSG_SIZE;

    return addr_base;
}

STATIC u64 devdrv_s2s_get_local_dma_addr(struct sdid_parse_info src_sdid, u32 chan_id, u64 offset)
{
    u64 addr_base;

    // get huge offset
    addr_base = src_sdid.chip_id * DEVDRV_S2S_CHIP_MEM_SIZE_LOCAL + // chip mem offset
                src_sdid.die_id * DEVDRV_S2S_DEV_MEM_SIZE_LOCAL +   // die mem offset
                LOCAL_SERVER_BASE_ADDR;

    // get chan start addr in die
    addr_base = addr_base + offset + DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM_DEV * DEVDRV_S2S_DEVICE_MSG_SIZE +
                chan_id * DEVDRV_S2S_HOST_MSG_SIZE;

    return addr_base;
}

STATIC u64 devdrv_s2s_get_msg_bar_addr(struct sdid_parse_info src_sdid, struct devdrv_s2s_msg_chan *msg_chan,
                                       u64 offset)
{
    u64 bar_addr = msg_chan->msg_dev->pci_ctrl->res.s2s_msg.addr;

    // get chan start addr in die
    bar_addr = bar_addr + offset + DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM_DEV * DEVDRV_S2S_DEVICE_MSG_SIZE +
               msg_chan->chan_id * DEVDRV_S2S_HOST_MSG_SIZE;

    return bar_addr;
}

STATIC void devdrv_s2s_get_sdid_from_chan(u32 chan_id, struct sdid_parse_info *sdid)
{
    u32 chan_id_new;

    if (chan_id >= DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM_DEV) {
        chan_id_new = chan_id - DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM_DEV;
    } else {
        chan_id_new = chan_id;
    }

    sdid->server_id = chan_id_new / DEVDRV_S2S_MAX_DEV_NUM;
    sdid->chip_id = (chan_id_new % DEVDRV_S2S_MAX_DEV_NUM) / DEVDRV_S2S_DIE_NUM;
    sdid->die_id = chan_id_new % DEVDRV_S2S_DIE_NUM;
}

void *devdrv_get_s2s_non_trans_chan(struct devdrv_msg_dev *msg_dev)
{
    u32 chan_id;

    ka_task_mutex_lock(&msg_dev->s2s_non_trans->mutex);
    msg_dev->s2s_non_trans->last_use = (msg_dev->s2s_non_trans->last_use + 1) % DEVDRV_S2S_NON_TRANS_MSG_CHAN_NUM;
    chan_id = msg_dev->s2s_non_trans->last_use;
    ka_task_mutex_unlock(&msg_dev->s2s_non_trans->mutex);

    return msg_dev->s2s_non_trans->chan[chan_id];
}

STATIC int devdrv_init_s2s_chan_sdma_addr(struct sdid_parse_info dst_sdid_info, struct sdid_parse_info src_sdid_info,
                                          struct devdrv_s2s_msg_chan *msg_chan)
{
    struct devdrv_s2s_non_trans_msg msg = {0};
    struct devdrv_s2s_init_msg *init_msg = &msg.msg_data.init_msg;
    void *non_trans_chan = NULL;
    u32 real_out_len;
    int ret;

    non_trans_chan = devdrv_get_s2s_non_trans_chan(msg_chan->msg_dev);
    msg.msg_type = DEVDRV_S2S_NON_TRANS_INIT_TYPE;
    init_msg->host_sq_phy_addr = msg_chan->sq.data_buf_addr;
    init_msg->chan_id = msg_chan->chan_id;

    ret = devdrv_sync_msg_send(non_trans_chan, (void *)&msg, sizeof(struct devdrv_s2s_non_trans_msg),
                               sizeof(struct devdrv_s2s_non_trans_msg), &real_out_len);
    if ((ret != 0) || (real_out_len != sizeof(struct devdrv_s2s_non_trans_msg)) || (msg.ret != 0)) {
        devdrv_err("Host sq SDMA addr init failed.(dev_id=%u; ret=%d; real_out_len=%u; msg_ret=%d)\n", msg_chan->devid,
                   ret, real_out_len, msg.ret);
        return -EINVAL;
    }

    return 0;
}

STATIC void devdrv_s2s_msg_chan_mem_uninit(struct devdrv_msg_dev *msg_dev, struct devdrv_s2s_msg_chan *msg_chan)
{
    if (msg_chan->sq.ack_buf != NULL) {
        devdrv_pci_dma_free_coherent(msg_dev->dev, DEVDRV_S2S_HOST_MSG_SIZE, (void *)msg_chan->sq.ack_buf,
                                     msg_chan->sq.ack_addr);
        msg_chan->sq.ack_buf = NULL;
        msg_chan->sq.ack_addr = (~(ka_dma_addr_t)0);
    }

    if (msg_chan->sq.data_buf != NULL) {
        devdrv_pci_dma_free_coherent(msg_dev->dev, DEVDRV_S2S_HOST_MSG_SIZE, (void *)msg_chan->sq.data_buf,
                                     msg_chan->sq.data_buf_addr);
        msg_chan->sq.data_buf = NULL;
        msg_chan->sq.data_buf_addr = (~(ka_dma_addr_t)0);
    }

    if (msg_chan->sq.desc != NULL) {
        ka_mm_iounmap(msg_chan->sq.desc);
        msg_chan->sq.desc = NULL;
    }
}

STATIC int devdrv_s2s_msg_chan_mem_init(struct devdrv_msg_dev *msg_dev, struct devdrv_s2s_msg_chan *msg_chan,
                                        struct sdid_parse_info src_sdid_info)
{
    struct sdid_parse_info dst_sdid_info;
    phys_addr_t sq_bar_addr;
    int ret = 0;

    // service data transfer through data_buf
    msg_chan->sq.data_buf = (struct devdrv_s2s_msg *)devdrv_pci_dma_alloc_coherent(
        msg_dev->dev, DEVDRV_S2S_HOST_MSG_SIZE, &msg_chan->sq.data_buf_addr, KA_GFP_KERNEL);
    if (msg_chan->sq.data_buf == NULL) {
        devdrv_err("sq data_buf alloc failed.(devid=%u; chan=%u)\n", msg_chan->devid, msg_chan->chan_id);
        return -ENOMEM;
    }

    msg_chan->sq.ack_buf = (struct devdrv_s2s_msg *)devdrv_pci_dma_alloc_coherent(
        msg_dev->dev, DEVDRV_S2S_HOST_MSG_SIZE, &msg_chan->sq.ack_addr, KA_GFP_KERNEL);
    if (msg_chan->sq.ack_buf == NULL) {
        devdrv_pci_dma_free_coherent(msg_dev->dev, DEVDRV_S2S_HOST_MSG_SIZE, (void *)msg_chan->sq.data_buf,
                                     msg_chan->sq.data_buf_addr);
        msg_chan->sq.data_buf = NULL;
        msg_chan->sq.data_buf_addr = (~(ka_dma_addr_t)0);

        devdrv_err("sq ack_buf alloc failed.(devid=%u; chan=%u)\n", msg_chan->devid, msg_chan->chan_id);
        return -ENOMEM;
    }

    devdrv_s2s_get_sdid_from_chan(msg_chan->chan_id, &dst_sdid_info);

    // get remote addr
    msg_chan->remote_cq_msg_phy_addr = devdrv_s2s_get_dma_addr(src_sdid_info, dst_sdid_info,
                                                               DEVDRV_S2S_RECV_MSG_ADDR_OFFSET);
    msg_chan->remote_sq_msg_phy_addr = devdrv_s2s_get_dma_addr(src_sdid_info, dst_sdid_info,
                                                               DEVDRV_S2S_SEND_MSG_ADDR_OFFSET);
    // get local sq_addr
    msg_chan->sq_phy_addr = devdrv_s2s_get_local_dma_addr(src_sdid_info, msg_chan->chan_id,
                                                          DEVDRV_S2S_SEND_MSG_ADDR_OFFSET);

    sq_bar_addr = devdrv_s2s_get_msg_bar_addr(src_sdid_info, msg_chan, DEVDRV_S2S_SEND_MSG_ADDR_OFFSET);
    msg_chan->sq.desc = (struct devdrv_s2s_msg *)ka_mm_ioremap(sq_bar_addr, DEVDRV_S2S_HOST_MSG_SIZE);
    if (msg_chan->sq.desc == NULL) {
        devdrv_err("sq desc ka_mm_ioremap failed(devid=%u; chan_id=%u)\n", msg_chan->devid, msg_chan->chan_id);
        ret = -ENOMEM;
        goto init_s2s_chan_init_fail;
    }

    ret = devdrv_init_s2s_chan_sdma_addr(dst_sdid_info, src_sdid_info, msg_chan);
    if (ret != 0) {
        goto init_s2s_chan_init_fail;
    }

    ka_task_sema_init(&msg_chan->sema, 1);
    return 0;

init_s2s_chan_init_fail:
    devdrv_s2s_msg_chan_mem_uninit(msg_dev, msg_chan);
    return ret;
}

STATIC int devdrv_s2s_msg_mem_alloc(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_msg_dev *msg_dev = pci_ctrl->msg_dev;

    msg_dev->s2s_chan = devdrv_kzalloc(sizeof(struct devdrv_s2s_msg_chan) * DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM,
                                       KA_GFP_KERNEL);
    if (msg_dev->s2s_chan == NULL) {
        devdrv_err("s2s_chan devdrv_kzalloc failed. (dev_id=%u)\n", pci_ctrl->dev_id);
        return -ENOMEM;
    }

    msg_dev->s2s_non_trans = devdrv_kzalloc(sizeof(struct devdrv_s2s_non_trans_ctrl), KA_GFP_KERNEL);
    if (msg_dev->s2s_non_trans == NULL) {
        devdrv_err("s2s_non_trans devdrv_kzalloc failed. (dev_id=%u)\n", pci_ctrl->dev_id);
        devdrv_kfree(msg_dev->s2s_chan);
        msg_dev->s2s_chan = NULL;
        return -ENOMEM;
    }

    return 0;
}

STATIC void devdrv_s2s_msg_mem_free(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_msg_dev *msg_dev = pci_ctrl->msg_dev;

    devdrv_kfree(msg_dev->s2s_non_trans);
    msg_dev->s2s_non_trans = NULL;

    devdrv_kfree(msg_dev->s2s_chan);
    msg_dev->s2s_chan = NULL;
}

STATIC int devdrv_s2s_get_sdid_parse_info(struct devdrv_pci_ctrl *pci_ctrl, struct sdid_parse_info *src_sdid_info)
{
    u32 server_id_max;

    if ((pci_ctrl->ops.get_server_id != NULL) && (pci_ctrl->ops.get_max_server_num != NULL)) {
        src_sdid_info->server_id = pci_ctrl->ops.get_server_id(pci_ctrl);
        server_id_max = pci_ctrl->ops.get_max_server_num(pci_ctrl);
        if (server_id_max > DEVDRV_S2S_MAX_SERVER_NUM) {
            devdrv_warn("Get max_server_num invalid.(support_max_server_num=%u, real_max_server_num=%u)\n",
                        (u32)DEVDRV_S2S_MAX_SERVER_NUM, pci_ctrl->ops.get_max_server_num(pci_ctrl));
            return -EINVAL;
        }
        if (src_sdid_info->server_id >= server_id_max) {
            devdrv_warn("Invalid server_id or scale_type. set scale_type and server_id is matched"
                        "(support_max_server_id=%u, real_server_id=%u)\n",
                        server_id_max - 1, src_sdid_info->server_id);
            return -EINVAL;
        }
    } else {
        devdrv_err("Get server id failed, get_server_id is NULL.(devid=%u)\n", pci_ctrl->dev_id);
        return -EINVAL;
    }

    src_sdid_info->chip_id = pci_ctrl->dev_id / DEVDRV_S2S_DIE_NUM;
    src_sdid_info->die_id = pci_ctrl->dev_id % DEVDRV_S2S_DIE_NUM;

    if (src_sdid_info->server_id >= DEVDRV_S2S_MAX_SERVER_NUM) {
        devdrv_warn("server_id is invalid, s2s init failed(server_id=%u; devid=%u)\n", src_sdid_info->server_id,
                    pci_ctrl->dev_id);
        return -EINVAL;
    }

    return 0;
}

int devdrv_s2s_msg_chan_init(struct devdrv_pci_ctrl *pci_ctrl)
{
    int ret = 0, k, n, j, i;
    struct devdrv_s2s_msg_chan *msg_chan = NULL;
    struct sdid_parse_info src_sdid_info;
    struct devdrv_msg_dev *msg_dev = pci_ctrl->msg_dev;
    u32 real_support_chan_num_host;

    if ((pci_ctrl->addr_mode != DEVDRV_ADMODE_FULL_MATCH) || (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF)) {
        devdrv_warn("Only full match pf supports this func. (dev_id=%u;addr_mode=%d;virtfn_flag=%u)\n",
                    pci_ctrl->dev_id, pci_ctrl->addr_mode, pci_ctrl->virtfn_flag);
        return 0;
    }

    ret = devdrv_s2s_get_sdid_parse_info(pci_ctrl, &src_sdid_info);
    if (ret != 0) {
        return ret;
    }

    ret = devdrv_s2s_msg_mem_alloc(pci_ctrl);
    if (ret != 0) {
        return ret;
    }

    ka_task_mutex_init(&msg_dev->s2s_non_trans->mutex);
    msg_dev->s2s_non_trans->last_use = 0;

    for (k = 0; k < DEVDRV_S2S_NON_TRANS_MSG_CHAN_NUM; k++) {
        msg_dev->s2s_non_trans->chan[k] = devdrv_pcimsg_alloc_non_trans_queue_inner_msg(
            pci_ctrl->dev_id, &devdrv_s2s_non_trans_msg_chan_info);
        if (msg_dev->s2s_non_trans->chan[k] == NULL) {
            devdrv_err("alloc trans queue for s2s failed.(dev_id=%u; chan_id=%d)\n", pci_ctrl->dev_id, k);
            goto s2s_non_trans_msg_fail;
        }
    }
    devdrv_info("alloc trans queue for s2s success.(dev_id=%u;chan_num=%d)\n", pci_ctrl->dev_id, k);

    real_support_chan_num_host = devdrv_get_s2s_host_chan_num(pci_ctrl);
    for (i = 0; i < real_support_chan_num_host; i++) {
        msg_chan = &msg_dev->s2s_chan[i];
        msg_chan->chan_id = i;
        msg_chan->devid = pci_ctrl->dev_id;
        msg_chan->msg_dev = msg_dev;
        msg_chan->status = DEVDRV_S2S_NORMAL;
        msg_chan->version = DEVDRV_S2S_MSG_VERSION_1;
        msg_chan->tx_seq_num = 0;
        ret = devdrv_s2s_msg_chan_mem_init(msg_dev, msg_chan, src_sdid_info);
        if (ret != 0) {
            goto s2s_msg_chan_mem_init_fail;
        }
        msg_chan->dev = msg_dev->dev;
        msg_chan->valid = DEVDRV_ENABLE;
    }

    devdrv_event("s2s chan init success(devid=%u)\n", pci_ctrl->dev_id);
    return 0;

s2s_msg_chan_mem_init_fail:
    for (j = i - 1; j >= 0; j--) {
        msg_chan = &msg_dev->s2s_chan[j];
        msg_chan->valid = DEVDRV_DISABLE;
        msg_chan->dev = NULL;
        devdrv_s2s_msg_chan_mem_uninit(msg_dev, msg_chan);
    }

s2s_non_trans_msg_fail:
    for (n = k - 1; n >= 0; n--) {
        devdrv_pcimsg_free_non_trans_queue(msg_dev->s2s_non_trans->chan[n]);
        msg_dev->s2s_non_trans->chan[n] = NULL;
    }

    devdrv_s2s_msg_mem_free(pci_ctrl);

    return ret;
}

void devdrv_s2s_msg_chan_uninit(struct devdrv_pci_ctrl *pci_ctrl)
{
    int i, k;
    struct devdrv_s2s_msg_chan *msg_chan = NULL;
    struct devdrv_msg_dev *msg_dev = pci_ctrl->msg_dev;

    if ((pci_ctrl->addr_mode != DEVDRV_ADMODE_FULL_MATCH) || (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF)) {
        devdrv_warn("Only full match pf support this func. (dev_id=%u)\n", pci_ctrl->dev_id);
        return;
    }

    if (msg_dev->s2s_chan != NULL) {
        for (i = DEVDRV_S2S_SUPPORT_MAX_CHAN_NUM_HOST - 1; i >= 0; i--) {
            msg_chan = &msg_dev->s2s_chan[i];
            if (msg_chan->valid == DEVDRV_DISABLE) {
                continue;
            }
            msg_chan->valid = DEVDRV_DISABLE;
            devdrv_s2s_msg_chan_mem_uninit(msg_dev, &msg_dev->s2s_chan[i]);
        }
    }

    if (msg_dev->s2s_non_trans != NULL) {
        for (k = 0; k < DEVDRV_S2S_NON_TRANS_MSG_CHAN_NUM; k++) {
            if (msg_dev->s2s_non_trans->chan[k] != NULL) {
                devdrv_pcimsg_free_non_trans_queue(msg_dev->s2s_non_trans->chan[k]);
                msg_dev->s2s_non_trans->chan[k] = NULL;
            }
        }
    }

    devdrv_s2s_msg_mem_free(pci_ctrl);

    return;
}

int devdrv_s2s_npu_link_check(u32 dev_id, u32 sdid)
{
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u32 err_code = 0;
    int ret;

    pci_ctrl = devdrv_pci_ctrl_get(dev_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Get pci_ctrl failed. (dev_id=%u; pci_ctrl=NULL)\n", dev_id);
        return -EINVAL;
    }

    if ((pci_ctrl->addr_mode != DEVDRV_ADMODE_FULL_MATCH) || (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF)) {
        devdrv_pci_ctrl_put(pci_ctrl);
        devdrv_warn("Only full match pf support this func. (dev_id=%u;addr_mode=%d;virtfn_flag=%u)\n", pci_ctrl->dev_id,
                    pci_ctrl->addr_mode, pci_ctrl->virtfn_flag);
        return -EOPNOTSUPP;
    }

    ret = devdrv_admin_msg_chan_send(pci_ctrl->msg_dev, DEVDRV_HCCS_LINK_CHECK, &sdid, sizeof(u32), &err_code,
                                     sizeof(u32));
    if ((ret != 0) || (err_code != 0)) {
        devdrv_pci_ctrl_put(pci_ctrl);
        devdrv_err("s2s npu link check remote data failed. (devid=%u, sdid=%u, ret=%d, err_code=%u)\n", dev_id, sdid,
                   ret, err_code);
        return -ETIMEDOUT;
    }

    devdrv_pci_ctrl_put(pci_ctrl);
    return 0;
}
KA_EXPORT_SYMBOL(devdrv_s2s_npu_link_check);
