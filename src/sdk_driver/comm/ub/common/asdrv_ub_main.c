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
#include "ka_driver_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_compiler_pub.h"
#include "ka_list_pub.h"
#include "ka_barrier_pub.h"

#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_soc_res.h"
#include "asdrv_ub_hotreset.h"
#include "ascend_kernel_hal.h"
#include "asdrv_ub_admin_msg.h"
#include "asdrv_ub_non_trans_chan.h"
#include "asdrv_ub_common_msg.h"
#include "asdrv_ub_link_chan.h"
#include "asdrv_ub_pair_dev_info.h"
#include "asdrv_ub_rao.h"
#include "asdrv_ub_urma_chan.h"
#include "asdrv_ub_mem_decoder.h"
#include "asdrv_ub_main.h"
#include "ummu_core_adapt.h"
#include "ubcore_adapt.h"
#include "adapt_ops.h"

struct ascend_ub_ctrl *g_ub_ctrl = NULL;
u32 g_add_davinci_flag = 0;

u32 get_global_add_davinci_flag(void)
{
    return g_add_davinci_flag;
}

struct ascend_ub_ctrl *get_global_ub_ctrl(void)
{
    return g_ub_ctrl;
}

int ubdrv_get_local_token(u32 dev_id, u32 *token_value)
{
    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Check dev_id fail, when get token value. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ka_task_down_read(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
    if (g_ub_ctrl->asd_dev[dev_id].token.token_valid != ASCEND_VALID) {
        ka_task_up_read(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
        ubdrv_err("Token value is invalid. (dev_id=%u;token_valid=%u;valid=%u)\n", dev_id,
                  g_ub_ctrl->asd_dev[dev_id].token.token_valid, ASCEND_VALID);
        return -EAGAIN;
    }
    *token_value = g_ub_ctrl->asd_dev[dev_id].token.local_token;
    ka_task_up_read(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
    return 0;
}

void ubdrv_set_local_token(u32 dev_id, u32 token_value, u32 token_valid)
{
    ka_task_down_write(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
    g_ub_ctrl->asd_dev[dev_id].token.local_token = token_value;
    g_ub_ctrl->asd_dev[dev_id].token.token_valid = token_valid;
    ka_task_up_write(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
}

int ubdrv_get_dev_id_info(u32 dev_id, struct devdrv_dev_id_info *id_info)
{
    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Check devid fail, when get device id_info. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    if (id_info == NULL) {
        ubdrv_err("Input parameter is null, when get device id_info. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    ka_task_down_read(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
    if (g_ub_ctrl->asd_dev[dev_id].id_info_valid != ASCEND_VALID) {
        ka_task_up_read(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
        ubdrv_err("ID info is invalid. (dev_id=%u)\n", dev_id);
        return -EAGAIN;
    }
    id_info->device_id = g_ub_ctrl->asd_dev[dev_id].id_info.device_id;
    id_info->module_id = g_ub_ctrl->asd_dev[dev_id].id_info.module_id;
    id_info->module_vendor_id = g_ub_ctrl->asd_dev[dev_id].id_info.module_vendor_id;
    id_info->vendor_id = g_ub_ctrl->asd_dev[dev_id].id_info.vendor_id;
    ka_task_up_read(&g_ub_ctrl->asd_dev[dev_id].rw_sem);

    return 0;
}

void ubdrv_set_dev_id_info(u32 dev_id, struct ubdrv_id_info *id_info, u32 info_valid)
{
    ka_task_down_write(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
    g_ub_ctrl->asd_dev[dev_id].id_info_valid = info_valid;
    if (id_info != NULL) {
        (void)memcpy_s(&g_ub_ctrl->asd_dev[dev_id].id_info, sizeof(struct ubdrv_id_info), id_info,
                       sizeof(struct ubdrv_id_info));
    }
    ka_task_up_write(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
}

struct ascend_ub_dev_status *ubdrv_get_dev_status_mng(u32 dev_id)
{
    return &g_ub_ctrl->dev_status[dev_id];
}

struct ascend_ub_link_res *ubdrv_get_link_res_by_devid(u32 dev_id)
{
    return &g_ub_ctrl->link_res[dev_id];
}

int ubdrv_get_device_status(u32 dev_id)
{
    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Get device status fail, dev_id is invalid. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    return g_ub_ctrl->dev_status[dev_id].device_status;
}

void ubdrv_set_device_status(u32 dev_id, u32 status)
{
    struct ascend_ub_dev_status *status_mng = ubdrv_get_dev_status_mng(dev_id);
    int i = 0;

    // ref_cnt is 0, when set offline status
    ka_task_down_write(&status_mng->rw_sem);
    if (status == UBDRV_DEVICE_OFFLINE) {
        for (i = 0; i < UBDRV_DEVICE_OFFLINE_WAIT_CNT; i++) {
            if (ka_base_atomic_read(&status_mng->ref_cnt) == 0) {
                break;
            }
            ka_system_usleep_range(1, 2); // wait 1~2 us
        }
    }
    if (i == UBDRV_DEVICE_OFFLINE_WAIT_CNT) {
        ubdrv_warn("Wait device ref cnt timeout. (dev_id=%u; ref_cnt=%d)\n", dev_id,
                   ka_base_atomic_read(&status_mng->ref_cnt));
    }
    status_mng->device_status = status;
    ka_task_up_write(&status_mng->rw_sem);
}

int ubdrv_add_device_status_ref(u32 dev_id)
{
    struct ascend_ub_dev_status *status_mng = NULL;
    int status;
    int ret = 0;

    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    status_mng = ubdrv_get_dev_status_mng(dev_id);
    ka_task_down_read(&status_mng->rw_sem);
    status = status_mng->device_status;
    if ((status == UBDRV_DEVICE_BEGIN_ONLINE) || (status == UBDRV_DEVICE_ONLINE) ||
        (status == UBDRV_DEVICE_BEGIN_OFFLINE)) {
        ka_base_atomic_inc(&status_mng->ref_cnt);
    } else {
        ubdrv_warn("Add device status ref unsuccessful. (device_status=%d;dev_id=%u)\n", status, dev_id);
        ret = -ENODEV;
    }
    ka_task_up_read(&status_mng->rw_sem);
    return ret;
}

void ubdrv_sub_device_status_ref(u32 dev_id)
{
    struct ascend_ub_dev_status *status_mng = NULL;

    status_mng = ubdrv_get_dev_status_mng(dev_id);
    ka_base_atomic_sub(1, &status_mng->ref_cnt);
}

struct dev_eid_info *ubdrv_get_eid_info_by_devid(u32 devid)
{
    return &g_ub_ctrl->asd_dev[devid].eid_info;
}

struct ubcore_device *ubdrv_get_default_user_ctrl_urma_dev(void)
{
    return g_ub_ctrl->idev[USER_CTRL_DEFAULT_DEV_ID].ubc_dev;
}

enum ubdrv_eid_cmp_ret ubdrv_cmp_eid(union ubcore_eid *src_eid, union ubcore_eid *dst_eid)
{
    int i;

    for (i = 0; i < UBCORE_EID_SIZE; i++) {
        if (src_eid->raw[i] < dst_eid->raw[i]) {
            return UBDRV_CMP_SMALL;
        }
        if (src_eid->raw[i] > dst_eid->raw[i]) {
            return UBDRV_CMP_LARGE;
        }
    }
    return UBDRV_CMP_EQUAL;
}

STATIC bool ubdrv_check_is_bind(struct ub_idev *idev, u32 dev_id)
{
    u32 i;

    for (i = 0; i < ASCEND_UB_DEV_NUM_PER_FE; i++) {
        if (idev->dev_id[i] == dev_id) {
            return true;
        }
    }
    return false;
}

int ubdrv_davinci_bind_fe(struct ub_idev *idev, u32 dev_id)
{
    int cnt;
    u32 i;

    ka_task_down_write(&idev->rw_sem);
    if ((idev->valid == ASCEND_UB_INVALID) || (idev->status == UBDRV_UDMA_UNPROBE)) {
        ubdrv_err("Urma device is invalid. (idev_id=%u;ue_idx=%u)\n", idev->idev_id, idev->ue_idx);
        ka_task_up_write(&idev->rw_sem);
        return -EINVAL;
    }
    if (ubdrv_check_is_bind(idev, dev_id) == true) {
        ka_task_up_write(&idev->rw_sem);
        return 0;
    }
    for (i = 0; i < ASCEND_UB_DEV_NUM_PER_FE; i++) {
        if (idev->dev_id[i] == KA_U32_MAX) {
            idev->dev_id[i] = dev_id;
            ka_base_atomic_add(1, &idev->ref_cnt);
            ka_task_up_write(&idev->rw_sem);
            return 0;
        }
    }
    cnt = ka_base_atomic_read(&idev->ref_cnt);
    ubdrv_err("The urma device is bound to too many accounts. (idev_id=%u;ue_idx=%u;dev_id=%u;cnt=%d)\n", idev->idev_id,
              idev->ue_idx, dev_id, cnt);
    ka_task_up_write(&idev->rw_sem);
    return -EINVAL;
}

void ubdrv_davinci_unbind_fe(struct ub_idev *idev, u32 dev_id)
{
    u32 i;

    ka_task_down_write(&idev->rw_sem);
    for (i = 0; i < ASCEND_UB_DEV_NUM_PER_FE; i++) {
        if (idev->dev_id[i] == dev_id) {
            ka_base_atomic_sub(1, &idev->ref_cnt);
            idev->dev_id[i] = KA_U32_MAX;
            ka_task_up_write(&idev->rw_sem);
            return;
        }
    }
    ka_task_up_write(&idev->rw_sem);
    return;
}

int ubdrv_find_eid_index_by_eid(struct ub_idev *idev, struct ubcore_eid_info *eid, u32 *eid_idx)
{
    struct ubcore_eid_info *eid_info;
    u32 cnt, i;

    eid_info = ubcore_get_eid_list(idev->ubc_dev, &cnt);
    if (KA_IS_ERR_OR_NULL(eid_info)) {
        ubdrv_err("ubcore_get_eid_list failed.\n");
        return -EINVAL;
    }
    for (i = 0; i < cnt; i++) {
        if (ubdrv_cmp_eid(&eid->eid, &eid_info[i].eid) == UBDRV_CMP_EQUAL) {
            *eid_idx = eid_info[i].eid_index;
            ubcore_free_eid_list(eid_info);
            return 0;
        }
    }
    ubcore_free_eid_list(eid_info);
    return -EINVAL;
}

struct ub_idev *ubdrv_get_idev_by_eid_and_feidx(struct ubcore_eid_info *eid, u32 ue_idx, enum ubdrv_log_level log_level)
{
    struct ub_idev *idev;
    u32 i, eid_idx;

    if (ue_idx >= ASCEND_UDMA_MAX_FE_NUM) {
        ubdrv_err("ue_idx is invalid. (ue_idx=%u)\n", ue_idx);
        return NULL;
    }
    for (i = 0; i < ASCEND_UDMA_DEV_MAX_NUM; i++) {
        idev = &g_ub_ctrl->idev[i];
        ka_task_down_read(&idev->rw_sem);
        if ((idev->ubc_dev == NULL) || (idev->status == UBDRV_UDMA_UNPROBE)) {
            ka_task_up_read(&idev->rw_sem);
            continue;
        }
        if (ubdrv_find_eid_index_by_eid(idev, eid, &eid_idx) == 0) {
            ka_task_up_read(&idev->rw_sem);
            UBDRV_LOG_LEVEL(log_level, "Idev match eid success. (idev_id=%u;ue_idx=%u;dev_name=%s)\n", idev->idev_id,
                            ue_idx, idev->ubc_dev->dev_name);
            return idev;
        }
        ka_task_up_read(&idev->rw_sem);
    }
    return NULL;
}

struct ub_idev *ubdrv_get_idev_by_eid(struct ubcore_eid_info *eid, enum ubdrv_log_level log_level)
{
    struct ub_idev *idev = NULL;
    u32 i = 0;

    for (i = 0; i < ASCEND_UDMA_MAX_FE_NUM; i++) {
        idev = ubdrv_get_idev_by_eid_and_feidx(eid, i, log_level);
        if (idev != NULL) {
            return idev;
        }
    }
    return NULL;
}

STATIC struct ub_idev *ubdrv_get_idev_by_ubc_dev(struct ubcore_device *ubc_dev)
{
    struct ub_idev *idev = NULL;
    u32 i;

    for (i = 0; i < ASCEND_UDMA_DEV_MAX_NUM; i++) {
        idev = &g_ub_ctrl->idev[i];
        ka_task_down_read(&idev->rw_sem);
        if ((idev->ubc_dev == ubc_dev) && ((idev->status == UBDRV_UDMA_PROBE))) {
            ka_task_up_read(&idev->rw_sem);
            return idev;
        }
        ka_task_up_read(&idev->rw_sem);
    }
    ubdrv_err("Idev match fail. (name=%s)\n", ubc_dev->dev_name);
    return NULL;
}

void ubdrv_set_startup_flag(u32 dev_id, enum ubdrv_dev_startup_flag_type startup_flag)
{
    g_ub_ctrl->asd_dev[dev_id].startup_flag = startup_flag;
}

enum ubdrv_dev_startup_flag_type ubdrv_get_startup_flag(u32 dev_id)
{
    return g_ub_ctrl->asd_dev[dev_id].startup_flag;
}

void ubdrv_set_msg_retry_cnt(u32 dev_id, u32 cnt)
{
    g_ub_ctrl->asd_dev[dev_id].msg_retry_cnt = cnt;
}

u32 ubdrv_get_msg_retry_cnt(u32 dev_id)
{
    return g_ub_ctrl->asd_dev[dev_id].msg_retry_cnt;
}

void ubdrv_mutex_lock_polling(u32 dev_id, ka_mutex_t *lock)
{
    if (ubdrv_get_msg_retry_cnt(dev_id) == ASCEND_MSG_NORMAL_RETRY_CNT) {
        ka_task_mutex_lock(lock);
    } else {
        while (!ka_task_mutex_trylock(lock)) {
            ka_system_usleep_range(1, 2); // wait 1~2 us
        }
    }
}

struct ascend_ub_msg_dev *ubdrv_get_msg_dev_by_devid(u32 dev_id)
{
    struct ascend_ub_msg_dev *msg_dev;

    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id, get msg_dev failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    msg_dev = g_ub_ctrl->asd_dev[dev_id].msg_dev;
    return msg_dev;
}

struct ascend_dev *ubdrv_get_asd_dev_by_devid(u32 dev_id)
{
    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id, get ubus dev failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    return &g_ub_ctrl->asd_dev[dev_id];
}

STATIC struct ascend_ub_msg_dev *ubdrv_msg_dev_init(u32 dev_id, u32 remote_id, struct ub_idev *idev)
{
    struct ascend_ub_dev_status *dev_status;
    struct ascend_ub_msg_dev *msg_dev;
    struct ascend_dev *asd_dev;
    int msg_dev_len = sizeof(struct ascend_ub_msg_dev);

    msg_dev = ubdrv_kzalloc(msg_dev_len, KA_GFP_KERNEL);
    if (msg_dev == NULL) {
        ubdrv_err("Alloc msg_dev mem failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    msg_dev->dev_id = dev_id;
    msg_dev->remote_id = remote_id;
    asd_dev = &(g_ub_ctrl->asd_dev[dev_id]);
    msg_dev->idev = idev;
    msg_dev->ubc_dev = msg_dev->idev->ubc_dev;
    msg_dev->asd_dev = asd_dev;
    msg_dev->dev_valid = 1;
    msg_dev->chan_cnt = UBDRV_NON_TRANS_CHAN_CNT;
    ka_task_mutex_init(&msg_dev->mutex_lock);
    dev_status = &(g_ub_ctrl->dev_status[dev_id]);
    ka_base_atomic_set(&dev_status->ref_cnt, 0);
    return msg_dev;
}

STATIC void ubdrv_msg_dev_uninit(struct ascend_ub_msg_dev *msg_dev, enum ubdrv_dev_status final_state)
{
    struct ascend_ub_dev_status *dev_status;

    dev_status = &(g_ub_ctrl->dev_status[msg_dev->dev_id]);
    ka_base_atomic_set(&dev_status->ref_cnt, 0);
    dev_status->device_status = final_state;
    ka_task_mutex_destroy(&msg_dev->mutex_lock);
    ubdrv_davinci_unbind_fe(msg_dev->idev, msg_dev->dev_id);
    msg_dev->idev = NULL;
    msg_dev->ubc_dev = NULL;
    msg_dev->asd_dev = NULL;
    msg_dev->dev_valid = 0;
    ubdrv_kfree(msg_dev);
}

STATIC int ubdrv_admin_msg_chan_init(u32 dev_id, struct ascend_ub_msg_dev *msg_dev, struct jetty_exchange_data *data,
                                     struct ub_idev *idev)
{
    struct ascend_ub_admin_chan *msg_chan;
    struct ascend_ub_sync_jetty *admin_jetty;
    int ret = 0;

    admin_jetty = g_ub_ctrl->link_res[dev_id].admin_jetty;
    if (admin_jetty == NULL) {
        ubdrv_err("Admin jetty is null. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    ka_base_atomic_add(1, &(admin_jetty->user_cnt));
    msg_chan = &(msg_dev->admin_msg_chan);
    msg_chan->admin_jetty = admin_jetty;
    msg_chan->msg_dev = msg_dev;
    msg_chan->dev_id = msg_dev->dev_id;
    msg_chan->chan_stat.dev_id = dev_id;
    msg_chan->chan_stat.chan_id = 0;
    ka_task_mutex_init(&msg_chan->mutex_lock);

    ret = ubdrv_admin_chan_import_jfr(msg_dev->idev, msg_chan, data, dev_id, UBDRV_ERR_LEVEL);
    if (ret != 0) {
        ubdrv_err("Admin chan import jetty failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        goto destroy_mutex;
    }

    ret = ubdrv_create_admin_msg_chan(dev_id, msg_dev);
    if (ret != 0) {
        goto create_err;
    }

    ret = ubdrv_rearm_sync_jfc(admin_jetty);
    if (ret != 0) {
        ubdrv_err("Admin chan rearm jfc failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        goto rearm_err;
    }

    return 0;

rearm_err:
    ubdrv_del_admin_msg_chan(msg_dev, dev_id);
create_err:
    ubdrv_admin_chan_unimport_jfr(msg_chan, dev_id);
destroy_mutex:
    ka_task_mutex_destroy(&msg_chan->mutex_lock);
    msg_chan->msg_dev = NULL;
    msg_chan->admin_jetty = NULL;
    ka_base_atomic_sub(1, &(admin_jetty->user_cnt));
    return ret;
}

void ubdrv_admin_msg_chan_uninit(u32 dev_id, struct ascend_ub_msg_dev *msg_dev)
{
    struct ascend_ub_admin_chan *msg_chan;
    struct ascend_ub_sync_jetty *admin_jetty;

    msg_chan = &(msg_dev->admin_msg_chan);
    admin_jetty = msg_chan->admin_jetty;
    ubdrv_del_admin_msg_chan(msg_dev, dev_id);
    ubdrv_admin_chan_unimport_jfr(msg_chan, dev_id);
    ka_task_mutex_destroy(&msg_chan->mutex_lock);
    msg_chan->dev_id = 0;
    msg_chan->msg_dev = NULL;
    msg_chan->admin_jetty = NULL;
    ka_base_atomic_sub(1, &(admin_jetty->user_cnt));
}

struct ub_idev *ubdrv_get_idev(u32 idev_id)
{
    if (idev_id >= ASCEND_UDMA_DEV_MAX_NUM) {
        return NULL;
    }
    return &g_ub_ctrl->idev[idev_id];
}

STATIC struct ub_idev *ubdrv_alloc_idev_id(struct ubcore_device *ubc_dev)
{
    u32 ue_idx = (u32)ubc_dev->attr.ue_idx;
    struct ub_idev *idev = NULL;
    u32 i;

    if (ue_idx >= ASCEND_UDMA_MAX_FE_NUM) {
        ubdrv_err("Invalid ue_idx. (idx=%u)\n", ue_idx);
        return NULL;
    }

    for (i = 0; i < ASCEND_UDMA_DEV_MAX_NUM; ++i) {
        idev = &g_ub_ctrl->idev[i];
        ka_task_down_write(&idev->rw_sem);
        if (idev->ubc_dev != NULL) {
            ka_task_up_write(&idev->rw_sem);
            continue;
        }
        idev->ubc_dev = ubc_dev;
        idev->ue_idx = ue_idx;
        ka_base_atomic_set(&idev->link_chan.msg_num, 0); // It can be initialized only once.
        ka_task_up_write(&idev->rw_sem);
        ubdrv_info("Alloc idev id success. (idev_id=%u;ue_idx=%u;name=%s)\n", idev->idev_id, idev->ue_idx,
                   ubc_dev->dev_name);
        return idev;
    }
    ubdrv_err("Fe num is over. (name=%s;fe_num=%u;max=%u)\n", ubc_dev->dev_name, i, ASCEND_UDMA_DEV_MAX_NUM);
    return NULL;
}

STATIC void ubdrv_free_idev_id(u32 idev_id)
{
    ka_task_down_write(&g_ub_ctrl->idev[idev_id].rw_sem);
    g_ub_ctrl->idev[idev_id].valid = ASCEND_UB_INVALID;
    g_ub_ctrl->idev[idev_id].ubc_dev = NULL;
    ka_task_up_write(&g_ub_ctrl->idev[idev_id].rw_sem);
}

int ubdrv_ub_enable_funcs(u32 devid, u32 boot_mode)
{
    return -EOPNOTSUPP;
}

int ubdrv_ub_disable_funcs(u32 devid, u32 boot_mode)
{
    return -EOPNOTSUPP;
}

STATIC struct ubcore_client g_ascend_client = {
    .list_node = KA_LIST_HEAD_INIT(g_ascend_client.list_node),
    .client_name = "ascend_udma",
    .add = ubdrv_add_udma_device,
    .remove = ubdrv_remove_udma_device,
};

struct ubcore_client *get_global_ascend_client()
{
    return &g_ascend_client;
}

int devdrv_ub_get_connect_protocol(u32 dev_id)
{
    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id, get connect protocol failed. (dev_id=%u)\n", dev_id);
        return CONNECT_PROTOCOL_UNKNOWN;
    }
    return CONNECT_PROTOCOL_UB;
}

void devdrv_ub_set_device_boot_status(u32 dev_id, u32 status)
{
    g_ub_ctrl->asd_dev[dev_id].device_boot_status = status;
}

int devdrv_ub_get_device_boot_status(u32 devid, u32 *boot_status)
{
    if (boot_status == NULL) {
        ubdrv_err("Param boot_status is null, get boot status failed. (dev_id=%u)\n", devid);
        return -EINVAL;
    }

    if (ubdrv_is_valid_devid(devid) == false) {
        ubdrv_warn("Invalid devid, get boot status failed. (dev_id=%u)\n", devid);
        *boot_status = DSMI_BOOT_STATUS_UNINIT;
        return -ENXIO;
    }

    *boot_status = g_ub_ctrl->asd_dev[devid].device_boot_status;
    return 0;
}

int devdrv_ub_get_env_boot_type(u32 dev_id)
{
    return 0;
}

int devdrv_ub_get_pfvf_type_by_devid(u32 dev_id)
{
    if (dev_id < ASCEND_UB_PF_DEV_MAX_NUM) {
        return DEVDRV_SRIOV_TYPE_PF;
    }
    return DEVDRV_SRIOV_TYPE_VF;
}

bool devdrv_ub_is_mdev_vm_boot_mode(u32 dev_id)
{
    return false;
}

bool devdrv_ub_is_sriov_support(u32 dev_id)
{
    return true;
}

/*
 * EID Selection Logic:
 * Device-side: Use the smallest value from `local_eid`.
 * Host-side: Use the smallest value from `remote_eid`.
 * Output Constraint: Only the selected EID (based on the above rules) is returned to the business logic.
 */
int ubdrv_get_ub_dev_info(u32 dev_id, struct devdrv_ub_dev_info *eid_query_info, int *num)
{
    struct dev_eid_info *eid_info = NULL;
    struct ub_idev *idev_tmp = NULL;
    u32 min_idx;
    int ret;

    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id, get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if ((eid_query_info == NULL) || (num == NULL)) {
        ubdrv_err("Param is null, get ub dev info failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    eid_info = &g_ub_ctrl->asd_dev[dev_id].eid_info;
    if ((eid_info->num == 0) || (eid_info->min_idx >= eid_info->num)) {
        *num = 0;
        ubdrv_err("Invalid eid num or min idx. (dev_id=%u;num=%u;min_idx=%u)\n", dev_id, eid_info->num,
                  eid_info->min_idx);
        return -EINVAL;
    }

    min_idx = eid_info->min_idx;
    // find fe according to local_eid
    idev_tmp = ubdrv_get_idev_by_eid(&eid_info->local_eid[min_idx], UBDRV_DEBUG_LEVEL);
    if (idev_tmp == NULL) {
        ubdrv_err("Find fe according to local_eid fail. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    // find eid_cnt according idev and fe
    ret = ubdrv_find_eid_index_by_eid(idev_tmp, &eid_info->local_eid[min_idx], &eid_query_info->eid_index[0]);
    if (ret != 0) {
        ubdrv_err("Find eid_idx according eid and fe fail. (ret=%d; dev_id=%u; min_idx=%u; eid=" EID_FMT ")\n", ret,
                  dev_id, min_idx, EID_ARGS(eid_info->local_eid[min_idx].eid));
        return -EINVAL;
    }

    eid_query_info->udma_dev[0] = idev_tmp->ubc_dev;
    *num = 1;

    return 0;
}

int ubdrv_process_add_pasid(u32 dev_id, u64 pasid)
{
    return -EOPNOTSUPP;
}

int ubdrv_process_del_pasid(u32 dev_id, u64 pasid)
{
    return -EOPNOTSUPP;
}

STATIC void ubdrv_print_udma_device_info(struct ubcore_device *ubc_dev)
{
    ubdrv_info("udma device info. (dev_name=%s;attr_fe_id=%u;fe_id=%u;virtual=%d;port_cnt=%u)\n", ubc_dev->dev_name,
               ubc_dev->attr.ue_idx, ubc_dev->cfg.ue_idx, ubc_dev->attr.virtualization, ubc_dev->attr.port_cnt);
    if (ubc_dev->dma_dev != NULL) {
        ubdrv_info("ubc_dev->dma_dev=%pK;dma_dev->parent=%pK\n", ubc_dev->dma_dev, ubc_dev->dma_dev->parent);
    }

    ubdrv_info("Udma device eid info. (eid_cnt=%u)\n", ubc_dev->eid_table.eid_cnt);
}

int ubdrv_add_udma_device(struct ubcore_device *ubc_dev)
{
    struct ub_idev *idev;

    if (ubc_dev == NULL) {
        ubdrv_err("Param ubc_dev is null, add udma device fail.\n");
        return -EINVAL;
    }
    if ((ubc_dev->eid_table.eid_cnt > UBCORE_MAX_EID_CNT) || (ubc_dev->eid_table.eid_cnt == 0)) {
        ubdrv_err("Invalid eid_cnt. (eid_cnt=%u;dev_name=%s)\n", ubc_dev->eid_table.eid_cnt, ubc_dev->dev_name);
        return -EINVAL;
    }
    ubdrv_info("Start add udma device.\n");
    ubdrv_print_udma_device_info(ubc_dev);
    idev = ubdrv_alloc_idev_id(ubc_dev);
    if (idev == NULL) {
        ubdrv_err("Alloc idev id unsuccessful. (dev_name=%s;ue_idx=%u)\n", ubc_dev->dev_name, ubc_dev->attr.ue_idx);
        return -EINVAL;
    }

    ubcore_set_client_ctx_data(ubc_dev, &g_ascend_client, idev);
    if (ubdrv_get_ub_pcie_sel() == UBDRV_UB_SEL) {
        devdrv_set_communication_ops_status(DEVDRV_COMMNS_UB, DEVDRV_COMM_OPS_TYPE_ENABLE, idev->idev_id);
        ubdrv_pair_info_init_work(idev);
    }
    ka_task_down_write(&idev->rw_sem);
    idev->status = UBDRV_UDMA_PROBE;
    ka_task_up_write(&idev->rw_sem);
    ubdrv_info("Add udma device success. (idev_id=%u;ue_idx=%u;udma_name=%s)\n", idev->idev_id, idev->ue_idx,
               idev->ubc_dev->dev_name);
    return 0;
}

STATIC void ubdrv_remove_device_resource(u32 dev_id, enum ubdrv_dev_status final_state)
{
    struct ascend_ub_dev_status *status_mng = NULL;

    status_mng = ubdrv_get_dev_status_mng(dev_id);
    ka_task_down_write(&status_mng->rw_sem);
    if (status_mng->device_status == UBDRV_DEVICE_ONLINE) {
        status_mng->device_status = UBDRV_DEVICE_DEAD;
        ka_task_up_write(&status_mng->rw_sem);
    } else {
        ka_task_up_write(&status_mng->rw_sem);
        return;
    }
    // remove davinci_dev
    if (dev_id < ASCEND_UB_PF_DEV_MAX_NUM) {
        ubdrv_remove_davinci_dev(dev_id, UDA_REAL);
    } else {
        ubdrv_remove_davinci_dev_proc(dev_id);
    }
    ubdrv_del_msg_device(dev_id, final_state);
    return;
}

STATIC void ubdrv_remove_davinci_bind_fe(struct ub_idev *idev)
{
    u32 i = 0, dev_id = 0, j = 0;

retry:
    for (i = 0; i < ASCEND_UB_DEV_NUM_PER_FE; i++) {
        if (idev->dev_id[i] != KA_U32_MAX) {
            dev_id = idev->dev_id[i];
            ubdrv_remove_device_resource(dev_id, UBDRV_DEVICE_FE_RESET);
        }
    }
    if ((ka_base_atomic_read(&idev->ref_cnt) == 0) || (j > UBDRV_DEVICE_REMOVE_WAIT_CNT)) {
        ubdrv_info("Remove davinci finish. (ref_cnt=%d;cnt=%u)\n", ka_base_atomic_read(&idev->ref_cnt), j);
        return;
    }
    ka_system_usleep_range(200, 200); // wait 200us
    j++;
    goto retry;
}

void ubdrv_remove_udma_device(struct ubcore_device *ubc_dev, void *client_ctx)
{
    struct ub_idev *idev;
    u32 idev_id;
    u32 ue_idx;

    if (ubc_dev == NULL) {
        ubdrv_err("Param ubc_dev is null, remove udma device fail.\n");
        return;
    }

    idev = (struct ub_idev *)ubcore_get_client_ctx_data(ubc_dev, &g_ascend_client);
    if (idev == NULL) {
        ubdrv_err("ubcore_get_client_ctx_data fail, when remove udma device.\n");
        return;
    }
    idev_id = idev->idev_id;
    ue_idx = idev->ue_idx;
    if ((idev_id >= ASCEND_UDMA_DEV_MAX_NUM) || (ue_idx >= ASCEND_UDMA_MAX_FE_NUM)) {
        ubdrv_err("Invalid idev_id. (idev_id=%u;ue_idx=%u)\n", idev_id, ue_idx);
        return;
    }
    ubdrv_info("Start remove udma device. (idev_id=%u;ue_idx=%u)\n", idev_id, ue_idx);
    ka_task_down_write(&idev->rw_sem);
    idev->valid = ASCEND_UB_INVALID;
    idev->status = UBDRV_UDMA_UNPROBE;
    ka_task_up_write(&idev->rw_sem);
    ubdrv_remove_davinci_bind_fe(idev); // remove all davinci of bind the fe
    ubdrv_free_single_link_chan(idev);
    if (ubdrv_get_ub_pcie_sel() == UBDRV_UB_SEL) {
        devdrv_set_communication_ops_status(DEVDRV_COMMNS_UB, DEVDRV_COMM_OPS_TYPE_DISABLE, idev_id);
        ubdrv_pair_info_uninit_work(idev);
    }
    ubcore_set_client_ctx_data(ubc_dev, &g_ascend_client, NULL);
    ubdrv_free_idev_id(idev_id);
    ubdrv_info("Remove udma device success. (idev_id=%u)\n", idev_id);
    return;
}

int ubdrv_add_davinci_dev(u32 dev_id, u32 dev_type)
{
    struct ascend_ub_msg_dev *msg_dev;
    struct uda_dev_type uda_type = {0};
    struct uda_dev_para uda_para = {0};
    u32 chip_type;
    u32 udevid = 0;
    int ret;

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    uda_dev_type_pack_proc(&uda_type, dev_type);
    ret = ub_check_pack_master_id_to_uda(msg_dev, &uda_para, dev_id);
    if (ret != 0) {
        return -EINVAL;
    }

    chip_type = ubdrv_get_chip_type();
    if (chip_type == HISI_CHIP_UNKNOWN) {
        ubdrv_err("Get chip type fail. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    uda_para.udevid = msg_dev->dev_id;           // host 0 1 2, device 0
    uda_para.remote_udevid = msg_dev->remote_id; // host 0, device 0 1 2
    uda_para.chip_type = chip_type;
    uda_para.dev = &msg_dev->ubc_dev->dev;
    uda_para.pf_flag = (dev_type == UDA_VIRTUAL) ? 0 : 1;
    ret = uda_add_dev(&uda_type, &uda_para, &udevid);
    if (ret != 0) {
        ubdrv_err("Add davinci_dev fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -EINVAL;
    }
    ubdrv_set_device_status(dev_id, UBDRV_DEVICE_ONLINE);
    ubdrv_info("Add davinci_dev success. (dev_id=%u; udevid=%u)\n", dev_id, udevid);
    return 0;
}

void ubdrv_remove_davinci_dev(u32 dev_id, u32 dev_type)
{
    struct ascend_ub_msg_dev *msg_dev;
    struct uda_dev_type uda_type = {0};
    int ret;

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return;
    }
    if (ubdrv_get_device_status(dev_id) != UBDRV_DEVICE_DEAD) {
        ubdrv_set_device_status(dev_id, UBDRV_DEVICE_BEGIN_OFFLINE);
    }
    uda_dev_type_pack_proc(&uda_type, dev_type);

    ret = uda_remove_dev(&uda_type, msg_dev->dev_id);
    if (ret != 0) {
        ubdrv_err("Remove davinci_dev fail. (dev_id=%u; ret=%d)\n", msg_dev->dev_id, ret);
        return;
    }
    ubdrv_info("Remove davinci_dev success. (dev_id=%u)\n", msg_dev->dev_id);
}

STATIC void ubdrv_chan_ctrl_init(struct ascend_ub_msg_dev *msg_dev)
{
    ubdrv_non_trans_msg_chan_init(msg_dev);
    ubdrv_rao_msg_chan_init(msg_dev);
    ubdrv_urma_chan_init(msg_dev);
}

STATIC void ubdrv_chan_ctrl_uninit(struct ascend_ub_msg_dev *msg_dev)
{
    ubdrv_urma_chan_uninit(msg_dev);
    ubdrv_rao_msg_chan_uninit(msg_dev);
    ubdrv_non_trans_msg_chan_uninit(msg_dev);
}

int ubdrv_add_msg_device(u32 dev_id, u32 remote_id, u32 idev_id, u32 ue_idx, struct jetty_exchange_data *data)
{
    struct ascend_ub_msg_dev *msg_dev = NULL;
    struct ub_idev *idev = ubdrv_get_idev(idev_id);
    int ret;

    if ((dev_id >= ASCEND_UB_DEV_MAX_NUM) || (idev == NULL) || (data == NULL)) {
        ubdrv_err("ubdrv_add_msg_device failed. (dev_id=%u;idev_id=%u;fe_id=%u)\n", dev_id, idev_id, ue_idx);
        return -EINVAL;
    }
    ubdrv_info("Start add msg device. (dev_id=%u;idev_id=%u;fe_id=%u)\n", dev_id, idev_id, ue_idx);
    msg_dev = ubdrv_msg_dev_init(dev_id, remote_id, idev);
    if (msg_dev == NULL) {
        ubdrv_err("Msg_dev init failed. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }
    ubdrv_chan_ctrl_init(msg_dev);
    ka_task_down_write(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
    g_ub_ctrl->asd_dev[dev_id].msg_dev = msg_dev;
    ka_task_up_write(&g_ub_ctrl->asd_dev[dev_id].rw_sem);
    ret = ubdrv_admin_msg_chan_init(dev_id, msg_dev, data, idev);
    if (ret != 0) {
        ubdrv_err("ubdrv_create_admin_msg_chan failed. (ret=%d;dev_id=%u)\n", ret, dev_id);
        goto msg_chan_uninit;
    }
    ret = ubdrv_add_msg_device_proc(msg_dev, dev_id);
    if (ret != 0) {
        goto chan_ctrl_uninit;
    }
    ubdrv_set_startup_flag(dev_id, UBDRV_DEV_STARTUP_BOTTOM_HALF_OK);
    ubdrv_set_device_status(dev_id, UBDRV_DEVICE_BEGIN_ONLINE);
    ubdrv_info("Add msg device success. (dev_id=%u)\n", dev_id);
    g_add_davinci_flag = 1;
    return 0;

chan_ctrl_uninit:
    ubdrv_admin_msg_chan_uninit(dev_id, msg_dev);
msg_chan_uninit:
    g_ub_ctrl->asd_dev[dev_id].msg_dev = NULL;
    ubdrv_chan_ctrl_uninit(msg_dev);
    ubdrv_msg_dev_uninit(msg_dev, UBDRV_DEVICE_UNINIT);
    return ret;
}

STATIC void ubdrv_clear_link_chan_dfx(u32 dev_id)
{
    struct ascend_ub_admin_chan *link_chan = NULL;
    struct ascend_ub_msg_dev *msg_dev = NULL;
    struct ub_idev *idev = NULL;

    msg_dev = ubdrv_get_msg_dev_by_devid(dev_id);
    if (msg_dev == NULL) {
        ubdrv_err("Get msg_dev failed. (dev_id=%u)\n", dev_id);
        return;
    }
    idev = msg_dev->idev;
    link_chan = &idev->link_chan;
    // clear old link chan dfx
    ka_task_down_write(&idev->rw_sem);
    if ((link_chan->valid != UB_ADMIN_MSG_ENABLE) || (idev->valid != ASCEND_UB_VALID)) {
        ka_task_up_write(&idev->rw_sem);
        return;
    }
    (void)memset_s(&link_chan->chan_stat, sizeof(link_chan->chan_stat), 0, sizeof(struct ubdrv_msg_chan_stat));
    // recoder chan ub res info
    ubdrv_record_chan_jetty_info(&link_chan->chan_stat, link_chan->admin_jetty);
    ka_task_up_write(&idev->rw_sem);
}

STATIC void ubdrv_exit_release_msg_chan(u32 dev_id)
{
    ubdrv_exit_release_msg_chan_proc(dev_id);
    ubdrv_free_all_non_trans_chan(dev_id);
    ubdrv_free_all_rao_chan(dev_id);
    ubdrv_clear_link_chan_dfx(dev_id);
}

void ubdrv_del_msg_device(u32 dev_id, enum ubdrv_dev_status final_state)
{
    struct ascend_dev *asd_dev;
    struct ascend_ub_msg_dev *msg_dev;

    if (dev_id >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("ubdrv_del_msg_device failed. (dev_id=%u)\n", dev_id);
        return;
    }
    ubdrv_info("Start del msg device. (dev_id=%u)\n", dev_id);
    ubdrv_set_startup_flag(dev_id, UBDRV_DEV_STARTUP_UNPROBED);
    asd_dev = &(g_ub_ctrl->asd_dev[dev_id]);
    ka_task_down_write(&asd_dev->rw_sem);
    msg_dev = asd_dev->msg_dev;
    if (msg_dev == NULL) {
        ka_task_up_write(&asd_dev->rw_sem);
        ubdrv_err("Msg_dev is null. (dev_id=%u)\n", dev_id);
        return;
    }
    ubdrv_exit_release_msg_chan(dev_id);
    if (ubdrv_get_device_status(dev_id) != UBDRV_DEVICE_DEAD) {
        ubdrv_set_device_status(dev_id, UBDRV_DEVICE_OFFLINE);
    }
    asd_dev->msg_dev = NULL;
    ka_task_up_write(&asd_dev->rw_sem);
    ubdrv_admin_msg_chan_uninit(dev_id, msg_dev);
    ubdrv_delete_admin_jetty(dev_id);
    ubdrv_chan_ctrl_uninit(msg_dev);
    ubdrv_msg_dev_uninit(msg_dev, final_state);
    ubdrv_set_msg_retry_cnt(dev_id, ASCEND_MSG_MAX_RETRY_CNT);
    ubdrv_info("Del msg device success. (dev_id=%u)\n", dev_id);
    return;
}

STATIC void ubdrv_asd_dev_init(void)
{
    struct ascend_ub_dev_status *dev_status;
    struct ascend_dev *asd_dev;
    u32 i;

    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; i++) {
        asd_dev = &(g_ub_ctrl->asd_dev[i]);
        asd_dev->dev_id = i;
        asd_dev->token.token_valid = ASCEND_INVALID;
        asd_dev->phy_flag = true;
        asd_dev->msg_retry_cnt = ASCEND_MSG_MAX_RETRY_CNT;
        dev_status = &(g_ub_ctrl->dev_status[i]);
        dev_status->device_status = UBDRV_DEVICE_UNINIT;
        ka_base_atomic_set(&dev_status->ref_cnt, 0);
        ka_task_init_rwsem(&dev_status->rw_sem);
        ka_task_init_rwsem(&asd_dev->rw_sem);
    }
}

STATIC void ubdrv_asd_dev_uninit(void)
{
    struct ascend_ub_dev_status *dev_status;
    struct ascend_dev *asd_dev;
    int i;

    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; i++) {
        asd_dev = &(g_ub_ctrl->asd_dev[i]);
        dev_status = &(g_ub_ctrl->dev_status[i]);
        ka_base_atomic_set(&dev_status->ref_cnt, 0);
        dev_status->device_status = UBDRV_DEVICE_UNINIT;
        asd_dev->token.token_valid = ASCEND_INVALID;
        asd_dev->dev_id = 0;
        asd_dev->phy_flag = true;
        asd_dev->msg_retry_cnt = ASCEND_MSG_MAX_RETRY_CNT;
    }
}

void ubdrv_print_exchange_data(struct ubdrv_jetty_exchange_data *data)
{
    ubdrv_info("(eid%d: " EID_FMT ").\n", data->admin_jetty_info.eid.eid_index,
               EID_ARGS(data->admin_jetty_info.eid.eid));
    ubdrv_info("ue_idx = %u; jetty_id = %u\n", data->ue_idx, data->admin_jetty_info.id);
}

struct ub_idev *ubdrv_find_idev_by_udevid(u32 dev_id)
{
    struct devdrv_ub_dev_info eid_query_info = {0};
    struct ub_idev *idev = NULL;
    void *udma_dev = NULL;
    int num = 0;
    int ret = 0;

    ret = ubdrv_get_ub_dev_info(dev_id, &eid_query_info, &num);
    if (ret != 0) {
        return NULL;
    }

    udma_dev = (struct ubcore_device *)eid_query_info.udma_dev[0];
    if (udma_dev == NULL) {
        ubdrv_err("Check ubc dev is null. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    idev = ubdrv_get_idev_by_ubc_dev((struct ubcore_device *)udma_dev);
    if (idev == NULL) {
        ubdrv_err("Find idev fail. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    return idev;
}

int ubdrv_mia_dev_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (action == UDA_INIT) {
        ret = ubdrv_fe_init_instance(udevid);
    } else if (action == UDA_UNINIT) {
        ret = ubdrv_fe_uninit_instance(udevid);
    }
    ubdrv_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

int ubdrv_init_ub_ctrl(void)
{
    size_t len = sizeof(struct ascend_ub_ctrl);
    u32 i, k;

    g_ub_ctrl = ubdrv_kzalloc(len, KA_GFP_KERNEL);
    if (g_ub_ctrl == NULL) {
        ubdrv_err("Failed to ubdrv_kzalloc ub_ctrl. (len=%zu)\n", len);
        return -ENOMEM;
    }

    for (i = 0; i < ASCEND_UDMA_DEV_MAX_NUM; i++) {
        ka_task_init_rwsem(&g_ub_ctrl->idev[i].rw_sem);
        ka_base_atomic_set(&g_ub_ctrl->idev[i].ref_cnt, 0);
        g_ub_ctrl->idev[i].idev_id = i;
        g_ub_ctrl->idev[i].valid = ASCEND_UB_INVALID;
        g_ub_ctrl->idev[i].status = UBDRV_UDMA_UNPROBE;
        for (k = 0; k < ASCEND_UB_DEV_NUM_PER_FE; k++) {
            g_ub_ctrl->idev[i].dev_id[k] = KA_U32_MAX;
        }
    }
    ka_task_mutex_init(&g_ub_ctrl->mutex_lock);
    ubdrv_asd_dev_init();
    return 0;
}

void ubdrv_uninit_ub_ctrl(void)
{
    ubdrv_asd_dev_uninit();
    ka_task_mutex_destroy(&g_ub_ctrl->mutex_lock);
    ubdrv_kfree(g_ub_ctrl);
    g_ub_ctrl = NULL;
}

struct uba_dev_ops *ubdrv_get_ubcore_dev_ops(void);
struct uba_dev_ops *ubdrv_get_ummu_dev_ops(void);

int ubdrv_adapt_init(void)
{
    int ret;
    ret = uba_register_adapt_ops(UBA_OPS_UBCORE, ubdrv_get_ubcore_dev_ops());
    if (ret != 0) {
        ubdrv_err("Register ubcore adapt ops failed. (ret=%d)\n", ret);
        return ret;
    }
    ret = uba_register_adapt_ops(UBA_OPS_UMMU, ubdrv_get_ummu_dev_ops());
    if (ret != 0) {
        uba_unregister_adapt_ops(UBA_OPS_UBCORE);
        ubdrv_err("Register ummu adapt ops failed. (ret=%d)\n", ret);
    }
    return ret;
}

void ubdrv_adapt_uninit(void)
{
    uba_unregister_adapt_ops(UBA_OPS_UMMU);
    uba_unregister_adapt_ops(UBA_OPS_UBCORE);
}

STATIC int __ka_init ubdrv_module_init(void)
{
    struct devdrv_comm_ops *comm_ops = NULL;
    int ret;

    ret = ubdrv_adapt_init();
    if (ret != 0) {
        ubdrv_err("Register adapt ops failed. (ret=%d)\n", ret);
        return ret;
    }

    if (ubdrv_get_ub_pcie_sel() != UBDRV_UB_SEL) {
        ret = ubdrv_module_init_for_pcie();
        if (ret != 0) {
            goto unregister_adapt_ops;
        }
        return 0;
    }

    comm_ops = get_global_ubdrv_ops();
    ret = devdrv_register_communication_ops(comm_ops);
    if (ret != 0) {
        ubdrv_err("Register communication ops failed. (ret=%d)\n", ret);
        goto unregister_adapt_ops;
    }
    devdrv_set_communication_ops_status_proc(DEVDRV_COMMNS_UB, DEVDRV_COMM_OPS_TYPE_ENABLE, 0);

    if (ubdrv_init_ub_ctrl() != 0) {
        goto unregister_ops;
    }

    ubdrv_init_common_msg_ctrl_rwsem();
    ret = ubdrv_register_uda_notifier();
    if (ret != 0) {
        ubdrv_err("Register uda notifier failed. (ret=%d)\n", ret);
        goto unregister_uda_fail;
    }
#ifndef EMU_ST
    (void)module_feature_auto_init();
#endif
    ubdrv_link_init_work();
    ret = ubcore_register_client(&g_ascend_client);
    if (ret != 0) {
        ubdrv_err("Register urma client failed. (ret=%d)\n", ret);
        goto register_ub_client_fail;
    }
    ret = ubdrv_rao_client_ctrl_init();
    if (ret != 0) {
        ubdrv_err("RAO client ctrl init failed. (ret=%d)\n", ret);
        goto rao_ctrl_init_fail;
    }
    if (ubdrv_module_init_proc() != 0) {
        goto module_init_fail;
    }
    return 0;

module_init_fail:
    ubdrv_rao_client_ctrl_uninit();
rao_ctrl_init_fail:
    ubcore_unregister_client(&g_ascend_client);
register_ub_client_fail:
    ubdrv_link_uninit_work();
#ifndef EMU_ST
    module_feature_auto_uninit();
#endif
    ubdrv_unregister_uda_notifier();
unregister_uda_fail:
    ubdrv_uninit_ub_ctrl();
unregister_ops:
    devdrv_set_communication_ops_status_proc(DEVDRV_COMMNS_UB, DEVDRV_COMM_OPS_TYPE_DISABLE, 0);
    devdrv_unregister_communication_ops(comm_ops);
unregister_adapt_ops:
    ubdrv_adapt_uninit();
    return -EINVAL;
}

STATIC void __ka_exit ubdrv_module_exit(void)
{
    struct devdrv_comm_ops *comm_ops = NULL;
    u32 i;

    if (ubdrv_get_ub_pcie_sel() != UBDRV_UB_SEL) {
        ubdrv_module_uninit_for_pcie();
        ubdrv_adapt_uninit();
        return;
    }
    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; i++) {
        ubdrv_remove_device_resource(i, UBDRV_DEVICE_UNINIT);
    }

    ubdrv_module_exit_proc();
    ubdrv_rao_client_ctrl_uninit();
    ubcore_unregister_client(&g_ascend_client);
    ubdrv_link_uninit_work();
    ubdrv_put_all_pair_dev_info();
#ifndef EMU_ST
    module_feature_auto_uninit();
#endif
    ubdrv_unregister_uda_notifier();
    ubdrv_uninit_ub_ctrl();
    devdrv_set_communication_ops_status_proc(DEVDRV_COMMNS_UB, DEVDRV_COMM_OPS_TYPE_DISABLE, 0);
    comm_ops = get_global_ubdrv_ops();
    devdrv_unregister_communication_ops(comm_ops);
    ubdrv_adapt_uninit();
}

ka_module_init(ubdrv_module_init);
ka_module_exit(ubdrv_module_exit);
KA_MODULE_LICENSE("GPL");