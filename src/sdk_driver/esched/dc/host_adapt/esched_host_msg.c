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
#ifndef EVENT_SCHED_UT

#include "pbl/pbl_uda.h"
#include "pbl/pbl_feature_loader.h"

#include "comm_kernel_interface.h"
#include "securec.h"
#include "esched.h"
#include "esched_h2d_msg.h"
#include "esched_fops.h"
#include "esched_host_msg.h"

#include "esched_drv_adapt.h"

#include "esched_feature_check.h"
#include "esched_adapt_platform.h"

STATIC void esched_convert_dst_engine(struct sched_published_event_info *event_info)
{
    if (event_info->dst_engine == VIRTUAL_CCPU_HOST) {
        event_info->dst_engine = CCPU_HOST;
    }
    return;
}

STATIC int esched_host_msg_send(u32 dev_id, struct esched_ctrl_msg *msg, u32 msg_len)
{
    int ret;
    u32 out_len;

    ret = devdrv_common_msg_send(dev_id, (void *)msg, msg_len, msg_len, &out_len, DEVDRV_COMMON_MSG_ESCHED);
    if ((ret != 0) || (msg->head.error_code != 0)) {
        if ((ret != 0) && (!esched_log_limited(SCHED_LOG_LIMIT_MSG_SEND))) {
            sched_err("Send message to device failed. (dev_id=%d; deal_code=%d; ret=%d)\n", dev_id,
                      msg->head.error_code, ret);
        }
        return ret != 0 ? ret : msg->head.error_code;
    }

    return 0;
}

int sched_query_remote_trace_msg_send(u32 chip_id, struct sched_trace_input *para,
                                      struct sched_sync_event_trace *sched_trace)
{
    struct esched_ctrl_msg msg = {0};
    int ret;

    sched_remote_msg_head_init(&msg.head, ESCHED_MSG_TYPE_REMOTE_QUERY_TRACE);

    msg.trace_msg.para = *para;

    ret = esched_host_msg_send(chip_id, (void *)&msg, sizeof(msg));
    if (ret == 0) {
        *sched_trace = msg.trace_msg.sched_trace;
    }

    return ret;
}

int sched_publish_event_to_remote(u32 chip_id, u32 event_src, const struct sched_published_event_info *event_info,
                                  struct sched_published_event_func *event_func)
{
    struct esched_publish_msg msg;
    int ret, real_devid;
    u32 total_len = sizeof(struct esched_publish_msg) - SCHED_MAX_EVENT_MSG_LEN_EX * sizeof(char);

    if ((event_func->event_ack_func != NULL) || (event_func->event_finish_func != NULL)) {
#ifndef EMU_ST
        sched_warn("The callback function is not supported for remote submission.\n");
        return DRV_ERROR_NOT_SUPPORT;
#endif
    }
    sched_remote_msg_head_init(&msg.head, ESCHED_MSG_TYPE_REMOTE_SUBMIT);

    msg.submit_msg.event_info = *event_info;
    if (event_info->msg_len > 0) {
        ret = memcpy_s(msg.submit_msg.msg, SCHED_MAX_EVENT_MSG_LEN_EX, event_info->msg, event_info->msg_len);
        if (ret != 0) {
            sched_err("Failed to copy variable msg. (pid=%d; gid=%u; event_id=%u)\n", event_info->pid, event_info->gid,
                      event_info->event_id);
            return ret;
        }
        total_len += event_info->msg_len;
    }
    real_devid = (chip_id == uda_get_host_id()) ? event_info->dst_devid : chip_id;

    return esched_host_msg_send(real_devid, (void *)&msg, total_len);
}

STATIC int sched_publish_event_proxy(u32 devid, struct sched_numa_node *node, struct esched_publish_msg *msg)
{
    struct esched_remote_submit_msg *submit_msg = &msg->submit_msg;
    struct sched_published_event_func event_func;
    int32_t ret;
    u32 chip_id = devid;

    event_func.event_ack_func = NULL;
    event_func.event_finish_func = NULL;

    if (submit_msg->event_info.msg_len != 0) {
        submit_msg->event_info.msg = submit_msg->msg;
    } else {
        submit_msg->event_info.msg = NULL;
    }
    node->curr_msg.dst_pid = submit_msg->event_info.pid;
    node->curr_msg.dst_event_id = submit_msg->event_info.event_id;
    node->curr_msg.dst_subevent_id = submit_msg->event_info.subevent_id;
    submit_msg->event_info.publish_timestamp = sched_get_cur_timestamp();

    ret = sched_publish_event_para_check(&submit_msg->event_info);
    if (ret != 0) {
        return ret;
    }
    esched_convert_dst_engine(&submit_msg->event_info);
    ret = sched_submit_event_pre_proc(chip_id, SCHED_PRE_PROC_POS_REMOTE, &submit_msg->event_info, &event_func);
    if (ret != 0) {
        return (ret == SCHED_EVENT_PRE_PROC_SUCCESS_RETURN) ? 0 : ret;
    }

    return sched_publish_event(chip_id, SCHED_PUBLISH_FORM_KERNEL, &submit_msg->event_info, &event_func);
}

STATIC int sched_query_gid_proxy(u32 devid, struct esched_ctrl_msg *msg)
{
    u32 host_devid = uda_get_host_id();

    struct esched_remote_query_gid_msg *query_msg = &msg->query_gid_msg;
    if (query_msg->dst_devid == host_devid) {
        return sched_query_local_task_gid(host_devid, query_msg->pid, query_msg->grp_name, &query_msg->gid);
    } else {
        return sched_query_local_task_gid(devid, query_msg->pid, query_msg->grp_name, &query_msg->gid);
    }
}

int sched_query_remote_task_gid_msg_send(u32 chip_id, u32 dst_chip_id, int pid, const char *grp_name, u32 *gid)
{
    struct esched_ctrl_msg msg = {0};
    int ret, real_devid;

    sched_remote_msg_head_init(&msg.head, ESCHED_MSG_TYPE_REMOTE_QUERY_GID);

    msg.query_gid_msg.pid = pid;
    msg.query_gid_msg.dst_devid = dst_chip_id;
    ret = strcpy_s(msg.query_gid_msg.grp_name, EVENT_MAX_GRP_NAME_LEN, grp_name);
    if (ret != 0) {
        sched_err("Failed to invoke strcpy_s. (chip_id=%u; ret=%d)\n", chip_id, ret);
        return DRV_ERROR_INNER_ERR;
    }

    real_devid = (chip_id == uda_get_host_id()) ? dst_chip_id : chip_id;
    ret = esched_host_msg_send(real_devid, (void *)&msg, sizeof(msg));
    if (ret == 0) {
        *gid = msg.query_gid_msg.gid;
    }

    return ret;
}

STATIC int32_t sched_dispatch_param_check(u32 src_devid, u32 dst_devid, u32 src_pid,
                                          struct esched_remote_submit_msg *submit_msg)
{
    int32_t ret;
    u32 dst_pid, src_device_master_pid, dst_device_master_pid;

    if (dst_devid >= SCHED_MAX_CHIP_NUM) {
        sched_err("The devid is invalid. (src_devid=%u; dst_devid=%u)\n", src_devid, dst_devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    ret = devdrv_query_master_pid_by_device_slave(src_devid, src_pid, &src_device_master_pid);
    if (ret != 0) {
        sched_err("Query master pid by src_pid failed. (src_devid=%u; src_pid=%u; ret=%d)\n", src_devid, src_pid, ret);
        return DRV_ERROR_NO_PROCESS;
    }

    dst_pid = submit_msg->event_info.pid;
    ret = devdrv_query_master_pid_by_device_slave(dst_devid, dst_pid, &dst_device_master_pid);
    if (ret != 0) {
        sched_err("Query master pid by dst_pid failed. (dst_devid=%u; dest_pid=%u; ret=%d)\n", dst_devid, dst_pid, ret);
        return DRV_ERROR_NO_PROCESS;
    }

    if (src_device_master_pid != dst_device_master_pid) {
        sched_err("Pid does not match. (src_devid=%u; dst_devid=%u; src_pid=%u; dst_pid=%u; src_master_pid=%u; "
                  "dst_master_pid=%u)\n",
                  src_devid, dst_devid, src_pid, dst_pid, src_device_master_pid, dst_device_master_pid);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

STATIC int sched_dispatch_event_to_remote(u32 devid, struct esched_publish_msg *msg)
{
    int ret;
    u32 dst_devid;
    struct esched_publish_msg msg_local;
    struct esched_remote_submit_msg *submit_msg = &msg->submit_msg;
    u32 total_len = sizeof(struct esched_publish_msg) - SCHED_MAX_EVENT_MSG_LEN_EX * sizeof(char);

    dst_devid = submit_msg->event_info.dst_devid;
    ret = sched_dispatch_param_check(devid, dst_devid, msg->head.src_pid, submit_msg);
    if (ret != 0) {
        return ret;
    }

    ret = memcpy_s(&msg_local.submit_msg, sizeof(struct esched_remote_submit_msg), submit_msg,
                   sizeof(struct esched_remote_submit_msg));
    if (ret != 0) {
        sched_err("Failed to copy msg. (devid=%u; dst_devid=%u)\n", devid, dst_devid);
        return ret;
    }

    sched_remote_msg_head_init(&msg_local.head, ESCHED_MSG_TYPE_REMOTE_SUBMIT);

    total_len += submit_msg->event_info.msg_len;
    return esched_host_msg_send(dst_devid, (void *)&msg_local, total_len);
}

STATIC int esched_drv_remote_config_pid(u32 dev_id, u32 msg_type, u32 host_ctrl_pid, u32 pid_type, u32 pid)
{
    struct esched_ctrl_msg msg = {0};
    int ret;

    sched_remote_msg_head_init(&msg.head, msg_type);

    msg.host_pid_msg.vfid = 0;
    msg.host_pid_msg.host_ctrl_pid = host_ctrl_pid;
    msg.host_pid_msg.pid_type = pid_type;
    msg.host_pid_msg.pid = pid;

    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_err("Failed to invoke the esched_host_msg_send. (dev_id=%u; pid=%u; error_code=%d; ret=%d)\n", dev_id,
                  pid, msg.head.error_code, ret);
        return -EFAULT;
    }

    return 0;
}

int esched_drv_remote_add_pid(u32 dev_id, u32 host_ctrl_pid, u32 pid_type, u32 pid)
{
    return esched_drv_remote_config_pid(dev_id, ESCHED_MSG_TYPE_ADD_HOST_PID, host_ctrl_pid, pid_type, pid);
}

int esched_drv_remote_del_pid(u32 dev_id, u32 host_ctrl_pid, u32 pid)
{
    return esched_drv_remote_config_pid(dev_id, ESCHED_MSG_TYPE_DEL_HOST_PID, host_ctrl_pid, 0, pid);
}

int esched_drv_remote_add_pool(u32 dev_id, u32 cpu_type)
{
    struct esched_ctrl_msg msg = {0};
    int ret;

    sched_remote_msg_head_init(&msg.head, ESCHED_MSG_TYPE_ADD_POOL);

    msg.pool_msg.cpu_type = cpu_type;
    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_err("Failed to send msg. (dev_id=%u; error_code=%x; ret=%d)\n", dev_id, msg.head.error_code, ret);
        return -EFAULT;
    }

    return 0;
}

int esched_drv_remote_get_cpu_mbid(u32 dev_id, u32 cpu_type, u32 *mb_id, u32 *wait_mb_id)
{
    struct esched_ctrl_msg msg = {0};
    int ret;

    sched_remote_msg_head_init(&msg.head, ESCHED_MSG_TYPE_GET_CPU_MBID);

    msg.mbid_msg.cpu_type = cpu_type;
    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_err("Failed to send msg. (dev_id=%u; error_code=%x; ret=%d)\n", dev_id, msg.head.error_code, ret);
        return -EFAULT;
    }

    *mb_id = msg.mbid_msg.mb_id;
    *wait_mb_id = msg.mbid_msg.wait_mb_id;

    return 0;
}

int esched_drv_remote_config_intr(u32 dev_id, u32 irq)
{
    struct esched_ctrl_msg msg = {0};
    int ret;

    sched_remote_msg_head_init(&msg.head, ESCHED_MSG_TYPE_CONF_INTR);

    msg.intr_msg.irq = irq;
    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_debug("Invoke the esched_host_msg_send not successful. "
                    "(dev_id=%u; irq=%u; deal_code=%d; ret=%d)\n",
                    dev_id, irq, msg.head.error_code, ret);
        return -EFAULT;
    }

    return 0;
}

int esched_drv_remote_add_mb(u32 dev_id, u32 vf_id)
{
    struct esched_ctrl_msg msg = {0};
    int ret;

    sched_remote_msg_head_init(&msg.head, ESCHED_MSG_TYPE_ADD_MB);

    msg.mb_msg.vf_id = vf_id;
    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_err("Failed to invoke the esched_host_msg_send. "
                  "(dev_id=%u; vf_id=%u; error_code=%d; ret=%d)\n",
                  dev_id, vf_id, msg.head.error_code, ret);
        return -EFAULT;
    }

    return 0;
}

#ifndef EMU_ST
int esched_drv_remote_get_pool_id(u32 dev_id, u32 *pool_id)
{
    struct esched_ctrl_msg msg = {0};
    int ret;

    sched_remote_msg_head_init(&msg.head, ESCHED_MSG_TYPE_GET_POOL_ID);

    ret = esched_host_msg_send(dev_id, (void *)&msg, sizeof(msg));
    if (ret != 0) {
        sched_err("Failed to send msg. (dev_id=%u; error_code=%x; ret=%d)\n", dev_id, msg.head.error_code, ret);
        return -EFAULT;
    }

    *pool_id = msg.pool_id_msg.pool_id;
    return 0;
}
#endif

STATIC int esched_ctrl_msg_para_check(u32 devid, void *data, u32 in_data_len, u32 *real_out_len)
{
    struct esched_publish_msg *msg = NULL;
    struct esched_msg_head *msg_head = (struct esched_msg_head *)data;
    u32 ctrl_len = sizeof(struct esched_publish_msg) - SCHED_MAX_EVENT_MSG_LEN_EX * sizeof(char);

    if ((data == NULL) || (real_out_len == NULL)) {
        sched_err("The variable devid, data or real_out_len is invalid. (devid=%u; in_data_len=%u)\n", devid,
                  in_data_len);
        return -EINVAL;
    }

    if ((msg_head->type == ESCHED_MSG_TYPE_REMOTE_SUBMIT) || (msg_head->type == ESCHED_MSG_TYPE_D2D_EVENT_DISPATCH)) {
        if (in_data_len < ctrl_len) {
            sched_err("In_data_len is invalid. (devid=%u; in_data_len=%u; ctrl_len=%u)\n", devid, in_data_len,
                      ctrl_len);
            return -EINVAL;
        }

        msg = (struct esched_publish_msg *)data;
        if ((in_data_len - ctrl_len) < msg->submit_msg.event_info.msg_len) {
            sched_err("In_data_len is invalid. (devid=%u; in_data_len=%u; msg_len=%u; ctrl_len=%u)\n", devid,
                      in_data_len, msg->submit_msg.event_info.msg_len, ctrl_len);
            return -EINVAL;
        }
    } else {
        if (in_data_len != sizeof(struct esched_ctrl_msg)) {
            sched_err("In_data_len is invalid. (devid=%u; in_data_len=%u)\n", devid, in_data_len);
            return -EINVAL;
        }
    }
    return 0;
}

STATIC int esched_ctrl_msg_recv(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    struct esched_msg_head *msg = (struct esched_msg_head *)data;
    int ret;
    struct sched_numa_node *node = NULL;

    ret = esched_ctrl_msg_para_check(devid, data, in_data_len, real_out_len);
    if (ret != 0) {
        return -EINVAL;
    }

    node = esched_dev_get(devid);
    if (node == NULL) {
        sched_err("Invalid device. (chip_id=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }
    node->curr_msg.msg_type = msg->type;
    switch (msg->type) {
        case ESCHED_MSG_TYPE_REMOTE_SUBMIT:
            ret = sched_publish_event_proxy(devid, node, (struct esched_publish_msg *)data);
            *real_out_len = sizeof(struct esched_msg_head);
            break;
        case ESCHED_MSG_TYPE_REMOTE_QUERY_GID:
            ret = sched_query_gid_proxy(devid, (struct esched_ctrl_msg *)data);
            *real_out_len = sizeof(struct esched_ctrl_msg);
            break;
        case ESCHED_MSG_TYPE_D2D_EVENT_DISPATCH:
            ret = sched_dispatch_event_to_remote(devid, (struct esched_publish_msg *)data);
            *real_out_len = sizeof(struct esched_msg_head);
            break;
        default:
            ret = -EINVAL;
            break;
    }

    if ((ret != 0) && !((msg->type == ESCHED_MSG_TYPE_REMOTE_QUERY_GID) && (ret == DRV_ERROR_UNINIT))) {
        sched_warn("invoke the sched_publish_event_proxy not successful. (devid=%u; type=%d; ret=%d)\n", devid,
                   msg->type, ret);
    }
    node->curr_msg.msg_type = ESCHED_MSG_TYPE_MAX_NUM;
    msg->error_code = ret;
    esched_dev_put(node);
    return 0;
}

struct devdrv_common_msg_client esched_host_msg_client = {
    .type = DEVDRV_COMMON_MSG_ESCHED,
    .common_msg_recv = esched_ctrl_msg_recv,
};

STATIC int esched_init_instance(u32 dev_id)
{
    struct sched_dev_ops ops;
    int ret;

    ret = esched_feature_init_by_devid(dev_id);
    if (ret != 0) {
        sched_err("Failed to init feature by devid %u, ret=%d\n", dev_id, ret);
        return ret;
    }

    if (esched_has_feature(dev_id, ESCHED_FEATURE_HARDWARE_SCHED)) {
        esched_setup_dev_hw_ops(&ops);
    } else {
        esched_setup_dev_soft_ops(&ops);
    }

    ret = esched_create_dev(dev_id, &ops);
    if (ret != 0) {
        sched_err("Create dev failed. (dev_id=%u)\n", dev_id);
        return ret;
    }

    ret = module_feature_auto_init_dev(dev_id);
    if (ret != 0) {
#ifndef EMU_ST
        esched_destroy_dev(dev_id);
        sched_err("module feature init failed. (dev_id=%u)\n", dev_id);
        return ret;
#endif
    }

    return 0;
}

STATIC int esched_uninit_instance(u32 dev_id)
{
    module_feature_auto_uninit_dev(dev_id);
    esched_destroy_dev(dev_id);

    return 0;
}

#define ESCHED_HOST_NOTIFIER "esched_host"
static int esched_host_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= SCHED_MAX_CHIP_NUM) {
        sched_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    switch (action) {
        case UDA_INIT:
            ret = esched_init_instance(udevid);
            break;
        case UDA_UNINIT:
            ret = esched_uninit_instance(udevid);
            break;
        case UDA_TO_MIA:
            if (esched_adapt_has_hardware_mia(udevid)) {
                (void)esched_drv_reset_phy_dev(udevid);
                sched_info("Enable dev_id %u sriov\n", udevid);
            }
            break;
        case UDA_TO_SIA:
            if (esched_adapt_has_hardware_mia(udevid)) {
                esched_drv_restore_phy_dev(udevid);
                sched_info("Disable dev_id %u sriov\n", udevid);
            }
            break;
        default:
            /* Ignore other actions. */
            return 0;
    }

    sched_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);

    return ret;
}

int esched_client_init(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(ESCHED_HOST_NOTIFIER, &type, UDA_PRI3, esched_host_notifier_func);
    if (ret != 0) {
        sched_err("Failed to register client. (ret=%d)\n", ret);
        return ret;
    }

    ret = devdrv_register_common_msg_client(&esched_host_msg_client);
    if (ret != 0) {
        (void)uda_notifier_unregister(ESCHED_HOST_NOTIFIER, &type);
        sched_err("Failed to invoke the devdrv_register_common_msg_client. (type=%d)\n", esched_host_msg_client.type);
        return ret;
    }

    return 0;
}

void esched_client_uninit(void)
{
    struct uda_dev_type type;
    (void)devdrv_unregister_common_msg_client(0, &esched_host_msg_client);
    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(ESCHED_HOST_NOTIFIER, &type);
}

#else
int tmp_esched_host_msg()
{
    return 0;
}

int tmp_esched_mia_init()
{
    return 0;
}
#endif
