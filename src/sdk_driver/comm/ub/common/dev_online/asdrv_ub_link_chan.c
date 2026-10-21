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

#include "ka_errno_pub.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_runenv_config.h"
#include "asdrv_ub_main.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_pair_dev_info.h"
#include "asdrv_ub_link_chan.h"
#include "asdrv_ub_hotreset.h"

#define ASCEND_UB_LINK_CHAN_SEG_SIZE 4096 // 4k
#define ASCEND_UB_LINK_CHAN_SEND_SEG_COUNT 4
#define ASCEND_UB_LINK_CHAN_RECV_SEG_COUNT 16

struct ubdrv_link_work_mng g_link_work = {0};

struct ubdrv_link_work_mng *get_global_link_work(void)
{
    return &g_link_work;
}

int ubdrv_link_jetty_cfg_init_proc(struct ub_idev *idev, struct ascend_ub_sync_jetty *jetty, u32 link_token)
{
    jetty->send_jetty.jfr_msg_depth = ASCEND_UB_LINK_CHAN_RECV_SEG_COUNT;
    jetty->send_jetty.jfs_msg_depth = ASCEND_UB_LINK_CHAN_SEND_SEG_COUNT;
    jetty->send_jetty.recv_msg_len = ASCEND_UB_LINK_CHAN_SEG_SIZE;
    jetty->send_jetty.send_msg_len = ASCEND_UB_LINK_CHAN_SEG_SIZE;
    jetty->send_jetty.access = MEM_ACCESS_LOCAL;
    jetty->send_jetty.token_value = link_token;
    jetty->send_jetty.idev = idev;
    jetty->send_jetty.ubc_dev = idev->ubc_dev;
    jetty->send_jetty.eid_index = idev->eid_index;
    jetty->send_jetty.chan_mode = UBDRV_MSG_CHAN_FOR_LINK;

    jetty->recv_jetty.jfr_msg_depth = ASCEND_UB_LINK_CHAN_RECV_SEG_COUNT;
    jetty->recv_jetty.jfs_msg_depth = ASCEND_UB_LINK_CHAN_SEND_SEG_COUNT;
    jetty->recv_jetty.recv_msg_len = ASCEND_UB_LINK_CHAN_SEG_SIZE;
    jetty->recv_jetty.send_msg_len = ASCEND_UB_LINK_CHAN_SEG_SIZE;
    jetty->recv_jetty.access = MEM_ACCESS_LOCAL;
    jetty->recv_jetty.token_value = link_token;
    jetty->recv_jetty.idev = idev;
    jetty->recv_jetty.ubc_dev = idev->ubc_dev;
    jetty->recv_jetty.eid_index = idev->eid_index;
    jetty->recv_jetty.chan_mode = UBDRV_MSG_CHAN_FOR_LINK;
    return 0;
}

STATIC void ubdrv_link_jetty_cfg_uninit(struct ascend_ub_sync_jetty *jetty)
{
    jetty->send_jetty.jfr_msg_depth = 0;
    jetty->send_jetty.jfs_msg_depth = 0;
    jetty->send_jetty.recv_msg_len = 0;
    jetty->send_jetty.send_msg_len = 0;
    jetty->send_jetty.access = 0;
    jetty->send_jetty.token_value = 0;
    jetty->send_jetty.idev = NULL;
    jetty->send_jetty.ubc_dev = NULL;
    jetty->send_jetty.eid_index = 0;
    jetty->send_jetty.chan_mode = UBDRV_MSG_CHAN_FOR_MAX;

    jetty->recv_jetty.jfr_msg_depth = 0;
    jetty->recv_jetty.jfs_msg_depth = 0;
    jetty->recv_jetty.recv_msg_len = 0;
    jetty->recv_jetty.send_msg_len = 0;
    jetty->recv_jetty.access = 0;
    jetty->recv_jetty.token_value = 0;
    jetty->recv_jetty.idev = NULL;
    jetty->recv_jetty.ubc_dev = NULL;
    jetty->recv_jetty.eid_index = 0;
    jetty->recv_jetty.chan_mode = UBDRV_MSG_CHAN_FOR_MAX;
    return;
}

STATIC int ubdrv_link_chan_cfg_init(struct ascend_ub_admin_chan *link_chan, struct ascend_ub_sync_jetty *link_jetty)
{
    link_chan->msg_desc = ubdrv_kzalloc(link_jetty->recv_jetty.recv_msg_len, KA_GFP_KERNEL);
    if (link_chan->msg_desc == NULL) {
        ubdrv_err("Alloc chan msg desc mem fail. (mem_len=%u)\n", link_jetty->recv_jetty.recv_msg_len);
        return -ENOMEM;
    }
    link_chan->work_queue = ka_task_create_workqueue("ub_link_workqueue");
    if (link_chan->work_queue == NULL) {
        ubdrv_err("Create link workqueue fail. (dev_id=%u)\n", link_chan->dev_id);
        ubdrv_kfree(link_chan->msg_desc);
        link_chan->msg_desc = NULL;
        return -ENOMEM;
    }
    link_chan->admin_jetty = link_jetty;
    link_jetty->send_jetty.msg_chan = (void *)link_chan;
    link_jetty->recv_jetty.msg_chan = (void *)link_chan;
    link_chan->recv_work.jfc = link_jetty->recv_jetty.jfr_jfc;
    link_chan->recv_work.stat = &link_chan->chan_stat;
    ka_task_mutex_init(&link_chan->mutex_lock);

    KA_TASK_INIT_WORK(&link_chan->recv_work.work, ubdrv_jfce_recv_work);
    ka_base_atomic_set(&link_chan->user_cnt, 0);
    ka_task_mutex_lock(&link_chan->mutex_lock);
    link_chan->valid = UB_ADMIN_MSG_ENABLE;
    ka_task_mutex_unlock(&link_chan->mutex_lock);
    ubdrv_record_chan_jetty_info(&link_chan->chan_stat, link_chan->admin_jetty);
    return 0;
}

STATIC void ubdrv_link_chan_cfg_uninit(struct ascend_ub_admin_chan *link_chan)
{
    struct ascend_ub_sync_jetty *link_jetty = link_chan->admin_jetty;

    ka_task_mutex_lock(&link_chan->mutex_lock);
    link_chan->valid = UB_ADMIN_MSG_DISABLE;
    ka_task_mutex_unlock(&link_chan->mutex_lock);
    ubdrv_wait_chan_jfce_user_cnt(&link_chan->user_cnt, link_chan->dev_id, 0);
    ka_base_atomic_set(&link_chan->user_cnt, 0);
    ka_task_cancel_work_sync(&link_chan->recv_work.work);
    link_chan->recv_work.jfc = NULL;
    link_jetty->send_jetty.msg_chan = NULL;
    link_jetty->recv_jetty.msg_chan = NULL;
    link_chan->admin_jetty = NULL;
    ka_task_destroy_workqueue(link_chan->work_queue);
    link_chan->work_queue = NULL;
    ubdrv_kfree(link_chan->msg_desc);
    link_chan->msg_desc = NULL;
    ubdrv_clear_chan_dfx(&link_chan->chan_stat);
    return;
}

int ubdrv_create_single_link_chan(struct ub_idev *idev)
{
    struct ascend_ub_admin_chan *link_chan = &idev->link_chan;
    struct ascend_ub_sync_jetty *link_jetty;
    int ret = 0;

    ka_task_down_write(&idev->rw_sem);
    if ((link_chan->valid == UB_ADMIN_MSG_ENABLE) || (idev->valid == ASCEND_UB_INVALID)) {
        goto unlock_rw;
    }
    link_jetty = ubdrv_kzalloc(sizeof(struct ascend_ub_sync_jetty), KA_GFP_KERNEL);
    if (link_jetty == NULL) {
        ubdrv_err("Alloc link jetty mem failed. (idev_id=%u;ue_idx=%u)\n", idev->idev_id, idev->ue_idx);
        ret = -ENOMEM;
        goto unlock_rw;
    }

    ret = ubdrv_link_jetty_cfg_init(idev, link_jetty);
    if (ret != 0) {
        goto free_jetty_mem;
    }
    ret = ubdrv_create_basic_jetty(link_jetty, UBDRV_LINK_CHAN_JETTY_ID); // only host is fix
    if (ret != 0) {
        ubdrv_err("Ascend link jetty init failed. (ret=%d;idev_id=%u;ue_idx=%u)\n", ret, idev->idev_id, idev->ue_idx);
        goto link_jetty_cfg_uninit;
    }

    ret = ubdrv_link_chan_cfg_init(link_chan, link_jetty);
    if (ret != 0) {
        ubdrv_err("Ascend link chan init failed. (ret=%d;idev_id=%u;ue_idx=%u)\n", ret, idev->idev_id, idev->ue_idx);
        goto link_jetty_chan_uninit;
    }
    ret = ubdrv_rearm_sync_jfc(link_chan->admin_jetty);
    if (ret != 0) {
        ubdrv_err("Link chan rearm jfc failed. (ret=%u;idev_id=%u;ue_idx=%u)\n", ret, idev->idev_id, idev->ue_idx);
        goto del_baisc_jetty;
    }
    ubdrv_info("Create Link jetty success. (idev_id=%u;ue_idx=%u;send_jfr_id=%u; recv_jfr_id=%u)\n", idev->idev_id,
               idev->ue_idx, link_jetty->send_jetty.jfr->jfr_id.id, link_jetty->recv_jetty.jfr->jfr_id.id);
    ka_task_up_write(&idev->rw_sem);
    return 0;

del_baisc_jetty:
    ubdrv_link_chan_cfg_uninit(link_chan);
link_jetty_chan_uninit:
    ubdrv_delete_basic_jetty(link_jetty);
link_jetty_cfg_uninit:
    ubdrv_link_jetty_cfg_uninit(link_jetty);
free_jetty_mem:
    ubdrv_kfree(link_jetty);
unlock_rw:
    ka_task_up_write(&idev->rw_sem);
    return ret;
}

void ubdrv_create_link_chan(void)
{
    struct ub_idev *idev = NULL;
    u32 idev_id;
    int ret;

    for (idev_id = 0; idev_id < ASCEND_UDMA_DEV_MAX_NUM; idev_id++) {
        idev = ubdrv_get_idev(idev_id);
        ret = ubdrv_create_single_link_chan(idev);
        if (ret != 0) {
            ubdrv_err("Create link chan fail. (ret=%u;idev_id=%u;ue_idx=%u)\n", ret, idev_id, idev->ue_idx);
        }
    }
    return;
}

void ubdrv_free_single_link_chan(struct ub_idev *idev)
{
    struct ascend_ub_admin_chan *link_chan;
    struct ascend_ub_sync_jetty *link_jetty;

    ka_task_down_write(&idev->rw_sem);
    link_chan = &(idev->link_chan);
    if (link_chan->valid == UB_ADMIN_MSG_DISABLE) {
        ubdrv_warn("Link chan is null. (idev_id=%u)\n", idev->idev_id);
        ka_task_up_write(&idev->rw_sem);
        return;
    }
    link_jetty = link_chan->admin_jetty;
    ubdrv_link_chan_cfg_uninit(link_chan);
    ubdrv_delete_basic_jetty(link_jetty);
    ubdrv_link_jetty_cfg_uninit(link_jetty);
    ubdrv_kfree(link_jetty);
    ka_task_up_write(&idev->rw_sem);
    return;
}

void ubdrv_free_link_chan(void)
{
    struct ub_idev *idev = NULL;
    u32 idev_id;

    for (idev_id = 0; idev_id < ASCEND_UDMA_DEV_MAX_NUM; idev_id++) {
        idev = ubdrv_get_idev(idev_id);
        if (idev->ubc_dev == NULL) {
            continue;
        }
        ubdrv_free_single_link_chan(idev);
    }
    return;
}

int ubdrv_dev_online_process(u32 dev_id, u32 remote_devid, struct ub_idev *idev, struct jetty_exchange_data *data)
{
    int ret = 0;

    ret = ubdrv_add_msg_device(dev_id, remote_devid, idev->idev_id, 0, data);
    if (ret != 0) {
        ubdrv_err("Add msg dev fail. (dev_id=%u;ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = ubdrv_add_davinci_dev(dev_id, UDA_REAL);
    if (ret != 0) {
        ubdrv_err("Add uda dev fail. (dev_id=%u;ret=%d)\n", dev_id, ret);
        goto del_msg_dev;
    }
    devdrv_ub_set_device_boot_status(dev_id, DSMI_BOOT_STATUS_FINISH);
    return 0;

del_msg_dev:
    ubdrv_del_msg_device(dev_id, UBDRV_DEVICE_UNINIT);
    return ret;
}

void ubdrv_dev_offline_process(u32 dev_id)
{
    devdrv_ub_set_device_boot_status(dev_id, DSMI_BOOT_STATUS_UNINIT);
    ubdrv_remove_davinci_dev(dev_id, UDA_REAL);
    ubdrv_del_msg_device(dev_id, UBDRV_DEVICE_UNINIT);
    return;
}

int ubdrv_set_device_init_status(u32 dev_id)
{
    struct ascend_ub_dev_status *status_mng = ubdrv_get_dev_status_mng(dev_id);
    int ret = 0;

    ka_task_down_write(&status_mng->rw_sem);
    if (status_mng->device_status == UBDRV_DEVICE_UNINIT) {
        status_mng->device_status = UBDRV_DEVICE_BEGIN_INIT;
    } else {
        ubdrv_err("Device status is not uninit. (dev_id=%u;status=%d)\n", dev_id, status_mng->device_status);
        ret = -EINVAL;
    }
    ka_task_up_write(&status_mng->rw_sem);
    return ret;
}

void ubdrv_set_device_uninit_status(u32 dev_id)
{
    ubdrv_set_device_status(dev_id, UBDRV_DEVICE_UNINIT);
}

/* device interface */
int ubdrv_import_server(struct ub_idev *idev, struct ascend_ub_admin_chan *chan, struct ubcore_eid_info *eid,
                        u32 dev_id)
{
    struct jetty_exchange_data data = {0};
    int i;

    data.eid.eid_index = idev->eid_index;
    for (i = 0; i < UBCORE_EID_SIZE; i++) {
        data.eid.eid.raw[i] = eid->eid.raw[i];
    }
    data.id = UBDRV_SERVER_JETTY_ID;
    data.token_value = DEFAULT_TOKEN_VALUE; // peer token only in link_chan
    return ubdrv_admin_chan_import_jfr(idev, chan, &data, dev_id, UBDRV_WARN_LEVEL);
}

void ubdrv_unimport_server(struct ascend_ub_admin_chan *chan, u32 dev_id)
{
    ubdrv_admin_chan_unimport_jfr(chan, dev_id);
    return;
}

int ubdrv_link_chan_send_msg(u32 dev_id, struct ub_idev *idev, struct ascend_ub_user_data *data)
{
    struct ascend_ub_admin_chan *link_chan = &idev->link_chan;
    int ret;

    ka_task_mutex_lock(&link_chan->mutex_lock);
    if (link_chan->valid != UB_ADMIN_MSG_ENABLE) {
        ka_task_mutex_unlock(&link_chan->mutex_lock);
        ubdrv_err("Link chan is invalid. (idev_id=%u;ue_idx=%u)\n", idev->idev_id, idev->ue_idx);
        return -EAGAIN;
    }
    ret = ubdrv_basic_chan_send(dev_id, data, link_chan);
    if ((ret != 0) && (ret != UB_MSG_CHECK_VERSION_FAILED)) {
        ret = -ETIMEDOUT;
    }
    ka_task_mutex_unlock(&link_chan->mutex_lock);
    return ret;
}

void ubdrv_link_work_sched(void)
{
    if (g_link_work.work_magic == UBDRV_WORK_MAGIC) {
        ka_task_schedule_delayed_work(&g_link_work.link_work, 0);
    }
    return;
}

void ubdrv_link_init_work(void)
{
    ubdrv_link_init_work_proc(&g_link_work);
    g_link_work.work_magic = UBDRV_WORK_MAGIC;
}

void ubdrv_link_uninit_work(void)
{
    g_link_work.work_magic = 0;
    (void)ka_task_cancel_delayed_work_sync(&g_link_work.link_work);
}

STATIC void ubdrv_link_chan_exchange_data_process_rollback(u32 dev_id)
{
    ubdrv_delete_admin_jetty(dev_id);
    ubdrv_set_dev_id_info(dev_id, NULL, ASCEND_INVALID);
    ubdrv_set_local_token(dev_id, 0, ASCEND_INVALID);
    ubdrv_set_device_uninit_status(dev_id);
    return;
}

STATIC void ubdrv_link_check_status_rollback(u32 dev_id)
{
    int dev_status = ubdrv_get_device_status(dev_id);
    if (dev_status == UBDRV_DEVICE_BEGIN_INIT) {
        ubdrv_link_chan_exchange_data_process_rollback(dev_id);
        return;
    }
    ubdrv_warn("Rollback is not supported, uda call is finished. (dev_id=%u;dev_status=%d)\n", dev_id, dev_status);
}

STATIC void ubdrv_link_chan_exchange_data_process(u32 dev_id, struct ub_idev *idev,
                                                  struct ubdrv_link_exchange_data *recv_data, u32 *status)
{
    char version[UBDRV_VERSION_LEN] = {0};
    struct ascend_ub_link_res *link_res;
    int ret = 0;

    ubdrv_info("Enter link exchange data process. (dev_id=%u)\n", dev_id);
    *status = UB_MSG_PROCESS_FAILED;
    if (ubdrv_set_device_init_status(dev_id) != 0) {
        ubdrv_link_check_status_rollback(dev_id);
        return;
    }

    ret = dbl_runenv_get_drv_version(version, UBDRV_VERSION_LEN);
    if (ret != 0) {
        ubdrv_err("Get drv version fail. (dev_id=%u;ret=%d)\n", dev_id, ret);
        goto set_dev_uninit;
    }
    if (ka_base_strcmp(recv_data->version, version) != 0) {
        ubdrv_warn("Drv version does not match. (dev_id=%u;local_version=\"%s\";remote_version=\"%s\")\n", dev_id,
                   version, recv_data->version);
    }
    ubdrv_set_chip_type_from_exchange_data(recv_data->chip_type);
    ubdrv_set_local_token(dev_id, recv_data->host_token, ASCEND_VALID);
    ubdrv_set_dev_id_info(dev_id, &recv_data->id_info, ASCEND_VALID);
    /* create admin jetty */
    ret = ubdrv_create_admin_jetty(idev, dev_id, UBDRV_DEFAULT_JETTY_ID);
    if (ret != 0) {
        ubdrv_err("Create admin jetty failed. (ret=%d)\n", ret);
        goto process_fail_return;
    }

    /* reply host admin jetty info */
    link_res = ubdrv_get_link_res_by_devid(dev_id);
    ret = ubdrv_get_send_jetty_info(&link_res->admin_jetty->send_jetty, &recv_data->send_admin_info);
    if (ret != 0) {
        ubdrv_err("Get send admin jetty info fail. (dev_id=%u)\n", dev_id);
        goto del_admin_jetty;
    }
    *status = UB_MSG_PROCESS_SUCCESS;
    return;

del_admin_jetty:
    ubdrv_delete_admin_jetty(dev_id);
process_fail_return:
    ubdrv_set_dev_id_info(dev_id, NULL, ASCEND_INVALID);
    ubdrv_set_local_token(dev_id, 0, ASCEND_INVALID);
set_dev_uninit:
    ubdrv_set_device_uninit_status(dev_id);
    return;
}

STATIC void ubdrv_link_chan_notify_online_process(u32 dev_id, struct ub_idev *idev,
                                                  struct ubdrv_link_exchange_data *recv_data, u32 *status)
{
    int dev_status;

    ubdrv_info("Enter link online process. (dev_id=%u)\n", dev_id);
    dev_status = ubdrv_get_device_status(dev_id);
    if (dev_status != UBDRV_DEVICE_BEGIN_INIT) {
        ubdrv_err("Dev status is not begin init. (dev_id=%u;dev_status=%d)\n", dev_id, dev_status);
        *status = UB_MSG_PROCESS_FAILED;
        return;
    }
    *status = UB_MSG_PROCESS_SUCCESS;
}

STATIC void ubdrv_link_chan_recv_process(u32 dev_id, struct ub_idev *idev, struct ubdrv_link_exchange_data *recv_data,
                                         u32 *status)
{
    if (*status == UB_MSG_CHECK_VERSION_FAILED) {
        return;
    }
    if (recv_data->opcode == UBDRV_LINK_EXCHANGE_DATA) {
        ubdrv_link_chan_exchange_data_process(dev_id, idev, recv_data, status);
    } else if (recv_data->opcode == UBDRV_LINK_NOTIFIY_ONLINE) {
        ubdrv_link_chan_notify_online_process(dev_id, idev, recv_data, status);
    } else {
        ubdrv_err("Invalid opcode. (dev_id=%u;opcode=%u)\n", dev_id, recv_data->opcode);
        *status = UB_MSG_NULL_PROCESS;
    }
}

STATIC void ubdrv_link_bottom_half_process(u32 dev_id, struct ub_idev *idev, struct ubdrv_link_exchange_data *recv_data,
                                           u32 *status)
{
    u32 remote_devid = 0;
    int ret = 0;

    if ((*status == UB_MSG_PROCESS_SUCCESS) && (recv_data->opcode == UBDRV_LINK_NOTIFIY_ONLINE)) {
        ubdrv_info("Enter bottom half, not support retry. (dev_id=%u)\n", dev_id);
        ret = ubdrv_dev_online_process(dev_id, remote_devid, idev, &recv_data->recv_admin_info);
        if (ret != 0) {
            ubdrv_err("Host dev online fail in bottom half. (dev_id=%u;idev_id=%u;ret=%d)\n", dev_id, idev->idev_id,
                      ret);
            ubdrv_link_chan_exchange_data_process_rollback(dev_id);
        }
    }
}

STATIC int ubdrv_import_client(u32 dev_id, struct ub_idev *idev, struct jetty_exchange_data *data)
{
    struct ascend_ub_link_res *link_res = ubdrv_get_link_res_by_devid(dev_id);
    struct ubcore_tjetty *tjetty;

    if (link_res->link_tjetty != NULL) {
        ubdrv_err("Link res is not null. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    tjetty = ascend_import_jfr(idev->ubc_dev, idev->eid_index, data);
    if (KA_IS_ERR_OR_NULL(tjetty)) {
        ubdrv_err("Link chan import jetty fail. (dev_id=%u;errno=%ld)\n", dev_id, KA_PTR_ERR(tjetty));
        return -EUBOPFAIL;
    }

    link_res->link_tjetty = tjetty;
    return 0;
}

STATIC void ubdrv_unimport_client(u32 dev_id)
{
    struct ascend_ub_link_res *link_res = ubdrv_get_link_res_by_devid(dev_id);

    if (link_res->link_tjetty == NULL) {
        ubdrv_err("Link res is null. (dev_id=%u)\n", dev_id);
        return;
    }
    ubcore_unimport_jfr(link_res->link_tjetty);
    link_res->link_tjetty = NULL;
    return;
}

/* host interface */
STATIC struct ub_idev *ubdrv_find_idev_and_dev_id(u32 *dev_id, struct ubdrv_link_exchange_data *recv_data)
{
    struct ubcore_eid_info local_eid = {0};
    int ret;

    ret = ubdrv_query_pair_devinfo_by_remote_eid(dev_id, &local_eid, &recv_data->recv_admin_info.eid);
    if ((ret != 0) || (*dev_id >= ASCEND_UB_PF_DEV_MAX_NUM)) {
        ubdrv_err("Get pair dev info fail. (dev_id=%u;ret=%d)\n", *dev_id, ret);
        return NULL;
    }
    return ubdrv_get_idev_by_eid_and_feidx(&local_eid, ASCEND_PFE_IDX, UBDRV_INFO_LEVEL);
}

STATIC void ubdrv_link_chan_recv_process_rollback(u32 dev_id, enum ubdrv_link_request_type opcode)
{
    if ((opcode == UBDRV_LINK_EXCHANGE_DATA) || (opcode == UBDRV_LINK_NOTIFIY_ONLINE)) {
        ubdrv_link_chan_exchange_data_process_rollback(dev_id);
    } else {
        ubdrv_err("Invalid opcode. (dev_id=%u;opcode=%u)\n", dev_id, opcode);
    }
}

STATIC int ubdrv_send_link_reply_msg(u32 dev_id, struct ascend_ub_jetty_ctrl *cfg, struct ascend_ub_msg_desc *src_desc)
{
    struct ascend_ub_admin_chan *admin_chan;
    struct ascend_ub_jetty_ctrl *recv_jetty;
    struct ascend_ub_msg_desc *jfs_desc;
    struct ascend_ub_link_res *link_res;
    struct ubdrv_msg_chan_stat *stat;
    struct send_wr_cfg wr_cfg = {0};
    u32 seg_id = 0;
    int ret;

    admin_chan = (struct ascend_ub_admin_chan *)cfg->msg_chan;
    stat = &admin_chan->chan_stat;
    stat->dev_id = dev_id;
    stat->chan_id = 0;
    recv_jetty = &admin_chan->admin_jetty->recv_jetty;
    jfs_desc = ubdrv_get_send_desc(recv_jetty, seg_id); // jfs seg_id is 0
    // copy result data to jfs_desc
    ret = memcpy_s(jfs_desc, recv_jetty->send_msg_len, src_desc, recv_jetty->recv_msg_len);
    if (ret != 0) {
        ubdrv_err("Copy result data to jfs_desc failed. (ret=%d;dst_len=%u;src_len=%u;dev_id=%u)", ret,
                  recv_jetty->send_msg_len, recv_jetty->recv_msg_len, dev_id);
        return ret;
    }

    link_res = ubdrv_get_link_res_by_devid(dev_id);
    wr_cfg.user_ctx = src_desc->msg_num;
    wr_cfg.tjetty = link_res->link_tjetty;
    wr_cfg.sva = (u64)jfs_desc;
    wr_cfg.sseg = admin_chan->admin_jetty->recv_jetty.send_seg;
    wr_cfg.tseg = NULL; // Not used in send opcode
    wr_cfg.jfs = admin_chan->admin_jetty->recv_jetty.jfs;
    wr_cfg.len = (recv_jetty->send_msg_len - ASCEND_UB_MSG_DESC_LEN);
    return ubdrv_send_reply_msg(dev_id, 0, &wr_cfg, cfg, stat);
}

void ubdrv_link_chan_recv_prepare_process(struct ascend_ub_jetty_ctrl *cfg, struct ascend_ub_msg_desc *desc, u32 seg_id)
{
    u32 msg_len = sizeof(struct ubdrv_link_exchange_data);
    struct ubdrv_link_exchange_data *recv_data;
    enum ubdrv_link_request_type opcode;
    struct ub_idev *idev;
    u32 dev_id = 0;
    int ret;

    desc->real_data_len = msg_len;
    if (desc->in_data_len != msg_len) {
        ubdrv_err("Recv data len invalid. (len=%u;expect_len=%u)\n", desc->in_data_len, msg_len);
        desc->status = UB_MSG_CHECK_VERSION_FAILED;
        goto set_msg_num;
    }

    recv_data = (struct ubdrv_link_exchange_data *)desc->user_data;
    idev = ubdrv_find_idev_and_dev_id(&dev_id, recv_data);
    if (idev == NULL) {
        ubdrv_err("Find idev and dev_id fail by msg data. (dev_id=%u)\n", dev_id);
        goto set_msg_num;
    }
    if (ubdrv_davinci_bind_fe(idev, dev_id) != 0) {
        goto set_msg_num;
    }
    recv_data->dev_id = dev_id;
    opcode = recv_data->opcode;
    ret = ubdrv_import_client(dev_id, idev, &recv_data->client_info);
    if (ret != 0) {
        ubdrv_err("Host import client fail. (dev_id=%u)\n", dev_id);
        desc->status = UB_MSG_PROCESS_FAILED;
        goto import_client_fail;
    }
    // Can't reply msg, because the import of the device jetty failed.
    ubdrv_link_chan_recv_process(dev_id, idev, recv_data, &desc->status);
    ret = ubdrv_send_link_reply_msg(dev_id, cfg, desc);
    if (ret != 0) {
        ubdrv_err("Send link reply msg fail. (dev_id=%u;opcode=%u;ret=%d)\n", dev_id, opcode, ret);
        ubdrv_link_chan_recv_process_rollback(dev_id, opcode);
        desc->status = UB_MSG_REPLY_FAILED;
    }
    ubdrv_unimport_client(dev_id);
    ubdrv_link_bottom_half_process(dev_id, idev, recv_data, &desc->status);
    if (desc->status == UB_MSG_PROCESS_SUCCESS) {
        return;
    }

import_client_fail:
    ubdrv_davinci_unbind_fe(idev, dev_id);
set_msg_num:
    desc->msg_num = 0;
    return;
}