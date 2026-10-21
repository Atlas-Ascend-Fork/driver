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
#include "ka_task_pub.h"
#include "devdrv_manager_common.h"
#include "devdrv_pm.h"
#include "devmng_dms_adapt.h"
#include "hvdevmng_init.h"
#include "devdrv_manager_pid_map.h"
#include "adapter_api.h"
#include "devdrv_manager.h"
#include "devdrv_black_box_dump.h"

void *dev_manager_no_trasn_chan[ASCEND_DEV_MAX_NUM];
STATIC struct devdrv_common_msg_client devdrv_manager_common_chan;

STATIC int devdrv_manager_device_inform_handler(void *msg, u32 *ack_len, enum devdrv_ts_status status)
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    struct devdrv_info *dev_info = NULL;
    u32 dev_id;
    int ret;

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    if (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_VALID) {
        devdrv_drv_err("invalid message from host\n");
        return -EINVAL;
    }

    dev_id = dev_manager_msg_info->header.dev_id;
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("invalid dev_id(%u)\n", dev_id);
        return -ENODEV;
    }

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if (dev_info == NULL) {
        devdrv_drv_err("device(%u) is not ready.\n", dev_id);
        return -EINVAL;
    }

    switch (status) {
        case TS_DOWN:
            devdrv_drv_err("receive ts exception message from device(%u).\n", dev_id);
            devdrv_host_manager_device_exception(dev_info);
            ret = 0;
            break;
        case TS_WORK:
            devdrv_drv_err("receive ts resume message from device(%u).\n", dev_id);
            ret = devdrv_host_manager_device_resume(dev_info);
            break;
        case TS_FAIL_TO_SUSPEND:
            devdrv_drv_err("receive ts fail to suspend message from device(%u).\n", dev_id);
            ret = devdrv_host_manager_device_resume(dev_info);
            break;
        case TS_SUSPEND:
            devdrv_drv_err("receive ts suspend message from device(%u).\n", dev_id);
            ret = devdrv_host_manager_device_suspend(dev_info);
            break;
        default:
            devdrv_drv_err("invalid input ts status. dev_id(%u)\n", dev_id);
            return -EINVAL;
    }

    *ack_len = sizeof(*dev_manager_msg_info);
    /* return result */
    dev_manager_msg_info->header.result = ret;
    return 0;
}

STATIC int devdrv_manager_device_suspend(void *msg, u32 *ack_len)
{
    return devdrv_manager_device_inform_handler(msg, ack_len, TS_SUSPEND);
}

STATIC int devdrv_manager_device_down(void *msg, u32 *ack_len)
{
    return devdrv_manager_device_inform_handler(msg, ack_len, TS_DOWN);
}

STATIC int devdrv_manager_device_resume(void *msg, u32 *ack_len)
{
    return devdrv_manager_device_inform_handler(msg, ack_len, TS_WORK);
}

STATIC int devdrv_manager_device_fail_to_suspend(void *msg, u32 *ack_len)
{
    return devdrv_manager_device_inform_handler(msg, ack_len, TS_FAIL_TO_SUSPEND);
}

STATIC int devdrv_manager_check_process_sign(void *msg, u32 *ack_len)
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    int ret = DEVDRV_MANAGER_MSG_INVALID_RESULT;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    struct devdrv_manager_info *d_info = NULL;
    struct devdrv_process_sign *d_sign = NULL;
    struct process_sign *process_sign = NULL;
    u32 dev_id;
    u16 result = ESRCH;

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    if (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_VALID) {
        devdrv_drv_err("invalid message from host\n");
        return -EINVAL;
    }

    dev_id = dev_manager_msg_info->header.dev_id;
    if ((dev_id >= ASCEND_DEV_MAX_NUM)) {
        devdrv_drv_err("Invalid device id. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    d_info = devdrv_get_manager_info();
    if (d_info == NULL) {
        devdrv_drv_err("d_info is null. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    process_sign = (struct process_sign *)dev_manager_msg_info->payload;
    ka_task_mutex_lock(&d_info->devdrv_sign_list_lock);
    if (ka_list_empty_careful(&d_info->hostpid_list_header)) {
        devdrv_drv_err("Hostpid sign list is empty. (dev_id=%u; hostpid=%d)\n", dev_id, process_sign->tgid);
        goto out;
    }
    ka_list_for_each_safe(pos, n, &d_info->hostpid_list_header)
    {
        d_sign = ka_list_entry(pos, struct devdrv_process_sign, list);
        if (d_sign->hostpid == process_sign->tgid) {
            ret = devdrv_manager_container_check_devid_in_container(dev_id, d_sign->hostpid);
            if (ret != 0) {
                result = EINVAL;
                devdrv_drv_err("Device id and hostpid mismatch in container. (dev_id=%u; hostpid=%d; ret=%d)\n", dev_id,
                               d_sign->hostpid, ret);
            } else {
                result = 0;
                devdrv_drv_info("Process sign check success. (dev_id=%u; hostpid=%d)\n", dev_id, process_sign->tgid);
            }
            goto out;
        }
    }

    devdrv_drv_err("Host pid is not in process sign list. (dev_id=%u; hostpid=%d)\n", dev_id, process_sign->tgid);

out:
    ka_task_mutex_unlock(&d_info->devdrv_sign_list_lock);
    *ack_len = sizeof(*dev_manager_msg_info);
    dev_manager_msg_info->header.result = result;
    return 0;
}

STATIC int devdrv_manager_get_pcie_id(void *msg, u32 *ack_len)
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    struct devdrv_pcie_id_info pcie_id_info = {0};
    struct devdrv_pcie_id_info *host_pcie_id_info = NULL;
    struct devdrv_info *dev_info = NULL;
    u32 dev_id;
    int ret;

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    if (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_VALID) {
        devdrv_drv_err("invalid message from host\n");
        return -EINVAL;
    }

    dev_id = dev_manager_msg_info->header.dev_id;
    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (devdrv_get_devdrv_info_array(dev_id) == NULL)) {
        devdrv_drv_err("invalid dev_id(%u), or devdrv_info_array[dev_id] is NULL\n", dev_id);
        return -EINVAL;
    }

    dev_info = devdrv_get_devdrv_info_array(dev_id);

    ret = adap_get_pcie_id_info(dev_info->pci_dev_id, &pcie_id_info);
    if (ret) {
        devdrv_drv_err("devdrv_manager_get_pcie_id_info failed. dev_id(%u)\n", dev_id);
        goto out;
    }

    host_pcie_id_info = (struct devdrv_pcie_id_info *)dev_manager_msg_info->payload;

    host_pcie_id_info->venderid = pcie_id_info.venderid;
    host_pcie_id_info->subvenderid = pcie_id_info.subvenderid;
    host_pcie_id_info->deviceid = pcie_id_info.deviceid;
    host_pcie_id_info->subdeviceid = pcie_id_info.subdeviceid;
    host_pcie_id_info->bus = pcie_id_info.bus;
    host_pcie_id_info->fn = pcie_id_info.fn;
    host_pcie_id_info->device = pcie_id_info.device;

out:
    *ack_len = sizeof(*dev_manager_msg_info);
    /* return result */
    dev_manager_msg_info->header.result = ret;
    return 0;
}

STATIC int devdrv_manager_d2h_sync_matrix_ready(void *msg, u32 *ack_len)
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    int ret = 0;
    u32 dev_id;

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    if (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_VALID) {
        devdrv_drv_err("invalid message from host\n");
        return -EINVAL;
    }

    dev_id = dev_manager_msg_info->header.dev_id;
    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (devdrv_get_devdrv_info_array(dev_id) == NULL)) {
        devdrv_drv_err("invalid dev_id(%u), or devdrv_info_array[dev_id] is NULL\n", dev_id);
        return -EINVAL;
    }

    *ack_len = sizeof(*dev_manager_msg_info);
    dev_manager_msg_info->header.result = ret;
    return 0;
}

STATIC int devdrv_manager_d2h_get_device_index(void *msg, u32 *ack_len)
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    u32 *host_devid_buf = NULL;
    int device_index, ret;

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    if (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_VALID) {
        devdrv_drv_err("invalid message from host\n");
        return -EINVAL;
    }
    *ack_len = sizeof(*dev_manager_msg_info);

    host_devid_buf = (u32 *)dev_manager_msg_info->payload;
    ret = uda_dev_get_remote_udevid(*host_devid_buf, &device_index);
    if (ret != 0 || device_index >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("invalid host devid(%u), (ret=%d).\n", *host_devid_buf, ret);
        dev_manager_msg_info->header.result = DEVDRV_MANAGER_MSG_INVALID_RESULT;
    } else {
        *host_devid_buf = (u32)device_index;
        dev_manager_msg_info->header.result = 0;
    }

    return 0;
}

static int devdrv_set_device_info_aicpu(u32 dev_id, u32 aicpu_num)
{
    struct devdrv_info *dev_info = NULL;
    u32 ai_cpu_core_id;
    u32 aicpu_occupy_bitmap;

    /* aicpu_num set to 7 when SRIOV enable */
    if (aicpu_num == 7) {
        ai_cpu_core_id = 1;
        aicpu_occupy_bitmap = 0xFE;
    } else {
        /* when SRIOV disable, aicpu_num set to 6, aicpu ID begin from 2 */
        ai_cpu_core_id = 2;
        aicpu_occupy_bitmap = 0xFC;
    }

    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        return -EINVAL;
    }

    dev_info->ai_cpu_core_num = aicpu_num;
    dev_info->ai_cpu_core_id = ai_cpu_core_id;
    dev_info->aicpu_occupy_bitmap = aicpu_occupy_bitmap;
    dev_info->inuse.ai_cpu_num = aicpu_num;
    return 0;
}

STATIC int devdrv_set_host_aicpu_num_from_device(void *msg, u32 *ack_len)
{
    u32 dev_id;
    u32 *aicpu_num = NULL;
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    int ret;

    if (msg == NULL || ack_len == NULL) {
        devdrv_drv_err("Invalid para. (msg is NULL=%d; ack_len is NULL=%d)\n", msg == NULL, ack_len == NULL);
        return -EINVAL;
    }

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    dev_id = dev_manager_msg_info->header.dev_id;

    if (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_D2H_MAGIC) {
        devdrv_drv_err("Invalid message from device. (device id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (dev_id >= ASCEND_PDEV_MAX_NUM) {
        devdrv_drv_err("Invalid device id. (device id=%u)\n", dev_id);
        return -ENODEV;
    }

    aicpu_num = (int *)dev_manager_msg_info->payload;

    /* aicpu_num must only be 6 or 7 */
    if (*aicpu_num > 7 || *aicpu_num < 6) {
        devdrv_drv_err("Invalid aicpu number. (dev_id=%u, aicpu_num=%u, valid range=6-7)\n", dev_id, *aicpu_num);
        return -EINVAL;
    }

    ret = devdrv_set_device_info_aicpu(dev_id, *aicpu_num);
    if (ret != 0) {
        devdrv_drv_err("Set aicpu_num failed. (dev_id=%u, aicpu_num=%u)\n", dev_id, *aicpu_num);
        return ret;
    }

    ret = hvdevmng_set_core_num(dev_id, 0, 0);
    if (ret != 0) {
        devdrv_drv_err("hvdevmng_set_core_num failed. (dev_id=%u, aicpu_num=%u)\n", dev_id, *aicpu_num);
        return ret;
    }

    *ack_len = sizeof(u32) + sizeof(struct devdrv_manager_msg_head);
    dev_manager_msg_info->header.result = 0;

    return 0;
}

STATIC int (*devdrv_manager_chan_msg_processes[DEVDRV_MANAGER_CHAN_MAX_ID + 1])(void *msg, u32 *ack_len) = {
    [DEVDRV_MANAGER_CHAN_D2H_DEVICE_READY] = devdrv_manager_device_ready,
    [DEVDRV_MANAGER_CHAN_D2H_DOWN] = devdrv_manager_device_down,
    [DEVDRV_MANAGER_CHAN_D2H_SUSNPEND] = devdrv_manager_device_suspend,
    [DEVDRV_MANAGER_CHAN_D2H_RESUME] = devdrv_manager_device_resume,
    [DEVDRV_MANAGER_CHAN_D2H_FAIL_TO_SUSPEND] = devdrv_manager_device_fail_to_suspend,
    [DEVDRV_MANAGER_CHAN_D2H_CORE_INFO] = NULL,
    [DEVDRV_MANAGER_CHAN_D2H_GET_PCIE_ID_INFO] = devdrv_manager_get_pcie_id,
    [DEVDRV_MANAGER_CHAN_D2H_SYNC_MATRIX_READY] = devdrv_manager_d2h_sync_matrix_ready,
    [DEVDRV_MANAGER_CHAN_D2H_CHECK_PROCESS_SIGN] = devdrv_manager_check_process_sign,
    [DEVDRV_MANAGER_CHAN_D2H_GET_DEVICE_INDEX] = devdrv_manager_d2h_get_device_index,
    [DEVDRV_MANAGER_CHAN_D2H_DMS_EVENT_DISTRIBUTE] = dms_event_get_exception_from_device,
    [DEVDRV_MANAGER_CHAN_D2H_SEND_TSLOG_ADDR] = devdrv_manager_receive_tslog_addr,
    [DEVDRV_MANAGER_CHAN_D2H_SEND_DEVLOG_ADDR] = devdrv_manager_receive_devlog_addr,
#if (!defined(DEVMNG_UT)) && (!defined(DEVDRV_MANAGER_HOST_UT_TEST))
    [DEVDRV_MANAGER_CHAN_PID_MAP_SYNC] = devdrv_pid_map_sync_proc,
#endif
    [DEVDRV_MANAGER_CHAN_D2H_SET_HOST_AICPU_NUM] = devdrv_set_host_aicpu_num_from_device,
    [DEVDRV_MANAGER_CHAN_MAX_ID] = NULL,
};

STATIC int devdrv_chan_msg_dispatch(void *data, u32 *real_out_len)
{
    u32 msg_id = ((struct devdrv_manager_msg_head *)data)->msg_id;

    return devdrv_manager_chan_msg_processes[msg_id](data, real_out_len);
}

int devdrv_manager_rx_common_msg_process(u32 dev_id, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    u32 msg_id;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (data == NULL) || (real_out_len == NULL) ||
        (in_data_len < sizeof(struct devdrv_manager_msg_info))) {
        devdrv_drv_err("data(%pK) or real_out_len(%pK) is NULL, devid(%u), in_data_len(%u)\n", data, real_out_len,
                       dev_id, in_data_len);
        return -EINVAL;
    }
    msg_id = ((struct devdrv_manager_msg_head *)data)->msg_id;

    if (msg_id >= DEVDRV_MANAGER_CHAN_MAX_ID) {
        devdrv_drv_err("invalid msg_id(%u)\n", msg_id);
        return -EINVAL;
    }
    if (devdrv_manager_chan_msg_processes[msg_id] == NULL) {
        devdrv_drv_err("devdrv_manager_chan_msg_processes[%u] is NULL\n", msg_id);
        return -EINVAL;
    }

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)data;
    dev_manager_msg_info->header.dev_id = dev_id;
    return devdrv_chan_msg_dispatch(data, real_out_len);
}
EXPORT_SYMBOL_UNRELEASE(devdrv_manager_rx_common_msg_process);

int devdrv_manager_rx_msg_process(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    struct devdrv_info *dev_info = NULL;
    u32 msg_id;
    int dev_id;

    if ((msg_chan == NULL) || (data == NULL) || (real_out_len == NULL) ||
        (in_data_len < sizeof(struct devdrv_manager_msg_info))) {
        devdrv_drv_err("msg_chan(%pK) or data(%pK) or real_out_len(%pK) is NULL, in_data_len(%u)\n", msg_chan, data,
                       real_out_len, in_data_len);
        return -EINVAL;
    }
    msg_id = ((struct devdrv_manager_msg_head *)data)->msg_id;

    if ((msg_id >= DEVDRV_MANAGER_CHAN_MAX_ID) || (devdrv_manager_chan_msg_processes[msg_id] == NULL)) {
        devdrv_drv_err("invalid msg_id(%u) or devdrv_manager_chan_msg_processes[msg_id] is NULL\n", msg_id);
        return -EINVAL;
    }

    dev_info = (struct devdrv_info *)devdrv_get_msg_chan_priv(msg_chan);
    dev_manager_msg_info = (struct devdrv_manager_msg_info *)data;

    /* get dev_id by msg_chan */
    if ((dev_id = devdrv_get_msg_chan_devid(msg_chan)) < 0) {
        devdrv_drv_err("msg_chan to devid failed\n");
        return -EINVAL;
    }

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("invalid dev_id(%d).\r\n", dev_id);
        return -EINVAL;
    }
    dev_manager_msg_info->header.dev_id = dev_id;

    return devdrv_chan_msg_dispatch(data, real_out_len);
}
EXPORT_SYMBOL_UNRELEASE(devdrv_manager_rx_msg_process);

#define DEV_MNG_NON_TRANS_MSG_DESC_SIZE 1024
struct devdrv_non_trans_msg_chan_info dev_manager_msg_chan_info = {
    .msg_type = devdrv_msg_client_devmanager,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_LOW,
    .s_desc_size = DEV_MNG_NON_TRANS_MSG_DESC_SIZE,
    .c_desc_size = DEV_MNG_NON_TRANS_MSG_DESC_SIZE,
    .rx_msg_process = devdrv_manager_rx_msg_process,
};

STATIC void devdrv_manager_msg_chan_notify(u32 dev_id, int status)
{}

void devdrv_manager_common_chan_init(void)
{
    /* this function will be called at ka_module_init, doesn't need lock */
    devdrv_manager_common_chan.type = DEVDRV_COMMON_MSG_DEVDRV_MANAGER;
    devdrv_manager_common_chan.common_msg_recv = devdrv_manager_rx_common_msg_process;
    devdrv_manager_common_chan.init_notify = devdrv_manager_msg_chan_notify;
}

void devdrv_manager_common_chan_uninit(void)
{
    /* this function will be called at ka_module_init, doesn't need lock */
    devdrv_manager_common_chan.type = DEVDRV_COMMON_MSG_TYPE_MAX;
    devdrv_manager_common_chan.common_msg_recv = NULL;
    devdrv_manager_common_chan.init_notify = NULL;
}

struct devdrv_common_msg_client *devdrv_manager_get_common_chan(u32 dev_id)
{
    return &devdrv_manager_common_chan;
}

void *devdrv_manager_get_no_trans_chan(u32 dev_id)
{
    void *no_trans_chan = NULL;

    /* dev_manager_no_trasn_chan doesn't need lock */
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("get no trans chan failed, dev_id(%u)\n", dev_id);
        return NULL;
    }
    no_trans_chan = dev_manager_no_trasn_chan[dev_id];
    return no_trans_chan;
}

void devdrv_manager_set_no_trans_chan(u32 dev_id, void *no_trans_chan)
{
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("set no trans chan failed, dev_id(%u)\n", dev_id);
        return;
    }
    dev_manager_no_trasn_chan[dev_id] = no_trans_chan;
}

int devdrv_agent_sync_msg_send(u32 dev_id, struct devdrv_manager_msg_info *msg_info, u32 payload_len, u32 *out_len)
{
    u32 in_len;
    void *no_trans_chan = NULL;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (msg_info == NULL) || (out_len == NULL) ||
        (payload_len > sizeof(msg_info->payload))) {
        devdrv_drv_err("invalid dev_id(%u) or msg_info(%pK) is null or out_len(%pK) is null.\n", dev_id, msg_info,
                       out_len);
        return -EINVAL;
    }

    no_trans_chan = devdrv_manager_get_no_trans_chan(dev_id);
    if (no_trans_chan == NULL) {
        devdrv_drv_err("get device(%u) no trans chan failed", dev_id);
        return -ENODEV;
    }

    in_len = sizeof(struct devdrv_manager_msg_head) + payload_len;

    return devdrv_sync_msg_send(no_trans_chan, msg_info, in_len, in_len, out_len);
}
KA_EXPORT_SYMBOL(devdrv_agent_sync_msg_send);

int devdrv_manager_init_common_chan(u32 dev_id)
{
    int ret;
    struct devdrv_common_msg_client *devdrv_commn_chan = NULL;

    devdrv_commn_chan = devdrv_manager_get_common_chan(dev_id);
    if (devdrv_commn_chan->init_notify == NULL) {
        devdrv_drv_err("common chan get failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    ret = devdrv_register_common_msg_client(devdrv_commn_chan);
    if (ret) {
        devdrv_drv_err("devdrv register common msg channel failed. (ret=%d, dev_id=%u)\n", ret, dev_id);
        return ret;
    }

    return 0;
}

int devdrv_manager_none_trans_init(u32 dev_id)
{
    void *no_trans_chan = NULL;
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if (dev_info == NULL) {
        devdrv_drv_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    no_trans_chan = devdrv_pcimsg_alloc_non_trans_queue(dev_id, &dev_manager_msg_chan_info);
    if (no_trans_chan == NULL) {
        devdrv_drv_err("no_trans_chan alloc failed. (dev_id=%u)\n", dev_id);
        return -EIO;
    }

    devdrv_manager_set_no_trans_chan(dev_id, no_trans_chan);
    devdrv_set_msg_chan_priv(no_trans_chan, (void *)dev_info);
    return 0;
}

void devdrv_manager_non_trans_uninit(u32 dev_id)
{
    void *no_trans_chan = NULL;

    no_trans_chan = devdrv_manager_get_no_trans_chan(dev_id);
    if (no_trans_chan != NULL) {
        devdrv_set_msg_chan_priv(no_trans_chan, NULL);
        devdrv_pcimsg_free_non_trans_queue(no_trans_chan);
        devdrv_manager_set_no_trans_chan(dev_id, NULL);
        no_trans_chan = NULL;
    }
}