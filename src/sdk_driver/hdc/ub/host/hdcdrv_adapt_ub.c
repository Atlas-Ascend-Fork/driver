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
#include "comm/comm_pcie.h"
#include "pbl/pbl_runenv_config.h"
#include "pbl/pbl_feature_loader.h"
#include "hdcdrv_log.h"
#include "hdcdrv_cmd_ioctl.h"
#include "hdcdrv_core_ub.h"
#include "hdcdrv_ka_memory.h"

void hdcdrv_set_msg_env(int *msg_env, int env)
{
    *msg_env = env;
}

int hdcdrv_service_scope_init_ub(int service_type)
{
    if ((service_type == HDCDRV_SERVICE_TYPE_PROFILING) || (service_type == HDCDRV_SERVICE_TYPE_LOG) ||
        (service_type == HDCDRV_SERVICE_TYPE_DUMP)) {
        return HDCDRV_SERVICE_SCOPE_PROCESS;
    }

    return HDCDRV_SERVICE_SCOPE_GLOBAL;
}

int hdcdrv_service_log_limit_init_ub(int service_type)
{
    return HDCDRV_SERVICE_NO_LOG_LIMIT;
}

void hdcdrv_ub_gen_unique_value_proc(u32 *value)
{
    (*value) = HDCDRV_SESSION_UNIQUE_VALUE_HOST_FLAG | ((*value) & HDCDRV_SESSION_UNIQUE_VALUE_MASK);
}

int hdcdrv_ub_get_session_run_env(u32 dev_id, struct hdcdrv_dev *hdc_dev)
{
    int run_env = HDCDRV_SESSION_RUN_ENV_UNKNOW;
    bool is_in_container = false;

    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        return HDCDRV_SESSION_RUN_ENV_VIRTUAL;
    }

    is_in_container = run_in_normal_docker();
    if (hdc_dev->host_pm_or_vm_flag == DEVDRV_HOST_PHY_MACH_FLAG) {
        if (is_in_container) {
            run_env = HDCDRV_SESSION_RUN_ENV_PHYSICAL_CONTAINER;
        } else {
            run_env = HDCDRV_SESSION_RUN_ENV_PHYSICAL;
        }
    } else {
        if (is_in_container) {
            run_env = HDCDRV_SESSION_RUN_ENV_VIRTUAL_CONTAINER;
        } else {
            run_env = HDCDRV_SESSION_RUN_ENV_VIRTUAL;
        }
    }

    return run_env;
}

int hdcdrv_ub_get_session_run_env_proc(u32 dev_id, struct hdcdrv_dev *hdc_dev, struct hdcdrv_event_connect *conn_info)
{
    (void)conn_info;
    return hdcdrv_ub_get_session_run_env(dev_id, hdc_dev);
}

STATIC int hdcdrv_container_vir_to_phs_devid(u32 virtual_devid, u32 *physical_devid, u32 *vfid)
{
    return devdrv_manager_container_logical_id_to_physical_id(virtual_devid, physical_devid, vfid);
}

long hdcdrv_ub_convert_id_from_vir_to_phy(u32 drv_cmd, union hdcdrv_cmd *cmd_data, u32 *vfid)
{
    long ret = HDCDRV_OK;

    int *p_devid = NULL;
    switch (drv_cmd) {
        case HDCDRV_CMD_GET_PEER_DEV_ID:
        case HDCDRV_CMD_CLIENT_DESTROY:
        case HDCDRV_CMD_SERVER_CREATE:
        case HDCDRV_CMD_SERVER_DESTROY:
        case HDCDRV_CMD_ACCEPT:
        case HDCDRV_CMD_CONNECT:
        case HDCDRV_CMD_CLOSE:
        case HDCDRV_CMD_GET_SESSION_UID:
        case HDCDRV_CMD_GET_SESSION_ATTR:
        case HDCDRV_CMD_WAIT_CTRL_MSG:
        case HDCDRV_CMD_SEND_CTRL_MSG:
        case HDCDRV_CMD_INIT_CTRL_NODE:
        case HDCDRV_CMD_UNINIT_CTRL_NODE:
            p_devid = &cmd_data->cmd_com.dev_id;
            break;
        default:
            return ret;
    }
    ret = hdcdrv_container_vir_to_phs_devid((u32)(*p_devid), (u32 *)p_devid, vfid);

    return ret;
}

ka_page_t *hdcdrv_ub_alloc_pages_node(u32 dev_id, gfp_t gfp_mask, u32 order)
{
    ka_page_t *page = NULL;
    page = hdcdrv_alloc_pages(gfp_mask, order, KA_SUB_MODULE_TYPE_2);
    if (page != NULL) {
        return page;
    }

    return page;
}

void hdcdrv_set_host_pm_or_vm_flag(struct hdcdrv_dev *hdc_dev)
{
    int ret = 0;
    u32 host_pm_or_vm_flag = 0;

    ret = devdrv_get_host_phy_mach_flag(hdc_dev->dev_id, &host_pm_or_vm_flag);
    if (ret != 0) {
        hdcdrv_warn("Get_host_phy_mach_flag has problem. (dev_id=%u; ret=%d)\n", hdc_dev->dev_id, ret);
    }
    hdc_dev->host_pm_or_vm_flag = (ret == 0) ? host_pm_or_vm_flag : 0;
}

void uda_davinci_real_entity_type_pack_proc(struct uda_dev_type *type)
{
    uda_davinci_near_real_entity_type_pack(type);
}

int uda_notifier_register_proc(struct uda_dev_type *type)
{
    (void)type;
#ifndef EMU_ST
    (void)module_feature_auto_init();
#endif

    return 0;
}

void hdcdrv_set_session_run_env_ub(struct hdcdrv_session *session, int run_env)
{
    (void)session;
    (void)run_env;
}

int hdcdrv_ub_non_trans_ctrl_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    int ret = 0;
    struct hdcdrv_dev *hdc_dev = hdcdrv_ub_get_dev(devid);

    if (hdc_dev == NULL) {
        hdcdrv_err("hdc dev has been invalid. (dev_id=%u)\n", devid);
        return HDCDRV_ERR;
    }

    ret = devdrv_sync_msg_send(hdc_dev->ctrl_msg_chan, data, in_data_len, out_data_len, real_out_len);

    hdcdrv_put_dev(devid);
    return ret;
}

STATIC int hdcdrv_non_trans_ctrl_msg_recv(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len,
                                          u32 *real_out_len)
{
    u32 devid = (u32)devdrv_get_msg_chan_devid(msg_chan);

    return hdcdrv_ub_ctrl_msg_recv(devid, data, in_data_len, out_data_len, real_out_len);
}

STATIC struct devdrv_non_trans_msg_chan_info hdcdrv_ub_non_trans_msg_chan_info = {
    .msg_type = devdrv_msg_client_hdc,
    .flag = 0,
    .level = DEVDRV_MSG_CHAN_LEVEL_HIGH,
    .s_desc_size = HDCDRV_UB_NON_TRANS_MSG_S_DESC_SIZE,
    .c_desc_size = HDCDRV_UB_NON_TRANS_MSG_C_DESC_SIZE,
    .rx_msg_process = hdcdrv_non_trans_ctrl_msg_recv,
};

int hdcdrv_ub_init_msg_chan(struct hdcdrv_dev *hdc_dev)
{
    hdc_dev->ctrl_msg_chan = devdrv_pcimsg_alloc_non_trans_queue(hdc_dev->dev_id, &hdcdrv_ub_non_trans_msg_chan_info);
    if (hdc_dev->ctrl_msg_chan == NULL) {
        hdcdrv_err("Calling devdrv_pcimsg_alloc_non_trans_queue failed. (dev_id=%d)\n", hdc_dev->dev_id);
        return HDCDRV_ERR;
    }

    return 0;
}

void hdcdrv_ub_uninit_msg_chan(struct hdcdrv_dev *hdc_dev)
{
    if (hdc_dev->ctrl_msg_chan != NULL) {
        (void)devdrv_pcimsg_free_non_trans_queue(hdc_dev->ctrl_msg_chan);
        hdc_dev->ctrl_msg_chan = NULL;
    }
}

int hdcdrv_register_non_trans_msg_chan(void)
{
    return 0;
}

void hdcdrv_unregister_non_trans_msg_chan(void)
{
    return;
}