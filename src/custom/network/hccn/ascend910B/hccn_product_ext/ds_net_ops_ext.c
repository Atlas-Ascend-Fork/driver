/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <pwd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include "securec.h"
#include "user_log.h"
#include "ascend_hal.h"
#include "ds_net.h"
#include "hccn_dfx_struct.h"
#include "ds_net_ext.h"
#include "tool.h"
#include "tool_ext.h"
#include "hccn_comm.h"
#include "dsmi_common_interface.h"
#include "ds_net_common.h"
#include "ds_net_tool_ext.h"

int dsmi_get_pfc_storm_info(int logic_id, struct pfc_storm_info *info)
{
    int ret;
    struct ds_trans_data trans_data = {0};
    unsigned int size_out;

    if (logic_id > DS_MAX_LOGIC_ID || logic_id < 0) {
        roce_err("%s logic id:%d is invalid! expect [0]-[%d]", __func__, logic_id, DS_MAX_LOGIC_ID);
        return -EINVAL;
    }
    if (!info) {
        roce_err("%s input is null. (logic_id=%d)", __func__, logic_id);
        return -EINVAL;
    }

    size_out = sizeof(struct pfc_storm_info);
    DSMI_SET_TRANS_DATA(trans_data, DS_GET_PFC_STORM_INFO, NULL, 0, (char *)info, &size_out);

    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err("%s dsmi trans fail. (ret=%d; logic_id=%d)", __func__, ret, logic_id);
        return ret;
    }

    if (trans_data.result != 0) {
        roce_err("dsmi trans res error. (result=%d)", trans_data.result);
        return trans_data.result;
    }
    return trans_data.result;
}

int dsmi_clear_pfc_storm(int logic_id, int mode)
{
    struct ds_trans_data trans_data = {0};
    int ret;
    int out_buf = 0;
    int size_out = 0;
    if ((logic_id > DS_MAX_LOGIC_ID) || (logic_id < 0)) {
        roce_err("%s logic id:%d is invalid! expect [0]-[%d]", __func__, logic_id, DS_MAX_LOGIC_ID);
        return -EINVAL;
    }
    if (mode < 0 || mode > DS_CLEAR_PFC_MODE_MAX) {
        roce_err("duration mode is invalid. (logic_id=%d)", logic_id);
        return -EINVAL;
    }

    DSMI_SET_TRANS_DATA(trans_data, DS_CLEAR_PFC_STORM, (char *)&mode, sizeof(int), NULL, &size_out);

    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err("Dsmi clear pfc storm warn err cnt fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    if (trans_data.result != 0) {
        roce_err("Dsmi set cdr mode result failed. (result=%d; logic_id=%d)", trans_data.result, logic_id);
        return trans_data.result;
    }

    return trans_data.result;
}

int dsmi_set_pfc_storm_watchdog_param(int logic_id, struct pfc_storm_cfg_info *cfg_info)
{
    struct ds_trans_data trans_data = {0};
    unsigned int size_in, size_out = 0;
    int ret;

    if (logic_id > DS_MAX_LOGIC_ID || logic_id < 0) {
        roce_err("%s logic id:%d is invalid! expect [0]-[%d]", __func__, logic_id, DS_MAX_LOGIC_ID);
        return (-EINVAL);
    }

    if (cfg_info == NULL) {
        roce_err("set_pfc_storm_watchdog_param cfg_info is null. (logic_id=%d)", logic_id);
        return -EINVAL;
    }

    size_in = sizeof(struct pfc_storm_cfg_info);
    DSMI_SET_TRANS_DATA(trans_data, DS_SET_PFC_STORM_WATCHDOG, (char *)cfg_info, size_in, NULL, &size_out);

    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err(" set_pfc_storm_watchdog_param dsmi trans fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    if (trans_data.result != 0) {
        roce_err("set_pfc_storm_watchdog_param dsmi trans res error. (result=%d)", trans_data.result);
        return trans_data.result;
    }
    return trans_data.result;
}

int dsmi_get_pfc_storm_watchdog_param(int logic_id, struct pfc_storm_cfg_param *cfg_param)
{
    struct ds_trans_data trans_data = {0};
    unsigned int size_out;
    int ret;

    if (logic_id > DS_MAX_LOGIC_ID || logic_id < 0) {
        roce_err("%s logic id:%d is invalid! expect [0]-[%d]", __func__, logic_id, DS_MAX_LOGIC_ID);
        return (-EINVAL);
    }

    if (cfg_param == NULL) {
        roce_err("get_pfc_storm_watchdog_param cfg_param is null. (logic_id=%d)", logic_id);
        return -EINVAL;
    }

    size_out = sizeof(struct pfc_storm_cfg_param);
    DSMI_SET_TRANS_DATA(trans_data, DS_GET_PFC_STORM_WATCHDOG, NULL, 0, (char *)cfg_param, &size_out);

    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err("get_pfc_storm_watchdog_param dsmi trans fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    if (trans_data.result != 0) {
        roce_err("get_pfc_storm_watchdog_param dsmi trans res error. (result=%d)", trans_data.result);
        return trans_data.result;
    }
    return trans_data.result;
}

int dsmi_dump_optical_info_cmd(int logic_id, int port, struct ds_optical_register_info *info)
{
    struct ds_trans_data trans_data = {0};
    unsigned int size_out = sizeof(struct ds_optical_register_info);
    int ret;
    if ((logic_id > DS_MAX_LOGIC_ID) || (logic_id < 0) || (port > MAX_PORT_ID) || (port < 0)) {
        roce_err("Logic id or port id is invalid. (logic_id=%d; port_id=%d)", logic_id, port);
        return -EINVAL;
    }
    DSMI_SET_TRANS_DATA(trans_data, DS_DUMP_OPTICAL_INFO_CMD, (char *)info, sizeof(struct ds_optical_register_info),
                        (char *)info, &size_out);

    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err("Dsmi dump optical info fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }
    if (trans_data.result != 0) {
        roce_err("Dsmi dump optical info result failed. (result=%d; logic_id=%d)", trans_data.result, logic_id);
    }
    return trans_data.result;
}

int dsmi_get_hccs_ping_mesh_info(int logic_id, int task_id, struct ping_mesh_stat *info)
{
    int ret;
    struct ds_trans_data trans_data = {0};
    unsigned int size_out;

    if (logic_id > DS_MAX_LOGIC_ID || logic_id < 0) {
        roce_err("logic id:%d is invalid! expect [0]-[%d]", logic_id, DS_MAX_LOGIC_ID);
        return -EINVAL;
    }
    if (info == NULL) {
        roce_err("input is null. (logic_id=%d)", logic_id);
        return -EINVAL;
    }

    size_out = sizeof(struct ping_mesh_stat);
    DSMI_SET_TRANS_DATA(trans_data, DS_GET_PING_MESH_INFO, (char *)&task_id, sizeof(int), (char *)info, &size_out);
    trans_data.pid = getpid();
    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err("dsmi trans fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    if (trans_data.result) {
        roce_err("dsmi trans res error. (result=%d)", trans_data.result);
    }
    return trans_data.result;
}

int dsmi_get_hccs_ping_mesh_state(int logic_id, int task_id, unsigned int *state)
{
    int ret;
    struct ds_trans_data trans_data = {0};
    unsigned int size_out;

    if (logic_id > DS_MAX_LOGIC_ID || logic_id < 0) {
        roce_err("logic id:%d is invalid! expect [0]-[%d]", logic_id, DS_MAX_LOGIC_ID);
        return -EINVAL;
    }
    if (!state) {
        roce_err("input is null. (logic_id=%d)", logic_id);
        return -EINVAL;
    }

    size_out = sizeof(int);
    DSMI_SET_TRANS_DATA(trans_data, DS_GET_PING_MESH_STATE, (char *)&task_id, sizeof(int), (char *)state, &size_out);
    trans_data.pid = getpid();
    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err("dsmi trans fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    if (trans_data.result) {
        roce_err("dsmi trans res error. (result=%d)", trans_data.result);
    }
    return trans_data.result;
}

int dsmi_stop_hccs_ping_mesh(int logic_id, int task_id)
{
    struct ds_trans_data trans_data = {0};
    int ret;
    unsigned int size_out = 0;

    if ((logic_id > DS_MAX_LOGIC_ID) || (logic_id < 0)) {
        roce_err("Logic id is invalid. (logic_id=%d)", logic_id);
        return -EINVAL;
    }

    size_out = 0;
    DSMI_SET_TRANS_DATA(trans_data, DS_STOP_HCCS_PING_MESH, (char *)&task_id, sizeof(int), NULL, &size_out);
    trans_data.pid = getpid();
    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret != 0) {
        roce_err("Dsmi stop hccs ping mesh fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    if (trans_data.result != 0) {
        roce_err("Dsmi stop hccs ping mesh task error failed. (result=%d; logic_id=%d)", trans_data.result, logic_id);
    }

    return trans_data.result;
}

int dsmi_start_hccs_ping_mesh(int logic_id, struct hccs_ping_mesh_operate *operate_info)
{
    struct ds_trans_data trans_data = {0};
    unsigned int size_in, size_out = 0;
    int ret, ret_disable;

    if (logic_id > DS_MAX_LOGIC_ID || logic_id < 0) {
        roce_err("logic id is invalid, expect [0]-[%d]. (logic_id=%d)", DS_MAX_LOGIC_ID, logic_id);
        return -EINVAL;
    }

    if (operate_info == NULL) {
        roce_err("operate_info is NULL. (logic_id=%d)", logic_id);
        return -EINVAL;
    }

    size_in = sizeof(struct hccs_ping_mesh_operate);
    DSMI_SET_TRANS_DATA(trans_data, DS_START_PING_MESH_TASK, (char *)operate_info, size_in, NULL, &size_out);
    trans_data.pid = getpid();
    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret != 0) {
        roce_err("dsmi trans fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    if (trans_data.result != 0) {
        roce_err("dsmi start hccs ping mesh task error. (result=%d)", trans_data.result);
    }

    return trans_data.result;
}

int dsmi_handle_scdr_reg(int logic_id, int port_id, struct scdr_reg_info *reg_info)
{
    int ret;
    unsigned int size;
    struct ds_trans_data trans_data = {0};

    DSMI_CHECK_PTR_VALID_RETURN_VAL(reg_info, -EINVAL);

    if ((logic_id > DS_MAX_LOGIC_ID) || (logic_id < 0) || (port_id > MAX_PORT_ID) || (port_id < 0)) {
        roce_err("Logic id or port id is invalid.(logic_id=%d; port_id=%d)", logic_id, port_id);
        return -EINVAL;
    }

    size = sizeof(struct scdr_reg_info);
    DSMI_SET_TRANS_DATA(trans_data, DS_HANDLE_CDR_REG, (char *)(void *)reg_info, size, (char *)(void *)reg_info, &size);

    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err("Dsmi handle scdr reg failed.(ret=%d; logic_id=%d; port=%d)", ret, logic_id, port_id);
        return ret;
    }

    if (trans_data.result != 0) {
        roce_err("Handle scdr reg result failed.(result=%d; logic_id=%d; port=%d)", trans_data.result, logic_id,
                 port_id);
    }

    return trans_data.result;
}

int dsmi_set_optical_mode(int logic_id, struct ds_optical_mode_info *data)
{
    struct ds_trans_data trans_data = {0};
    unsigned int size_in, size_out = 0;
    int ret;

    if ((logic_id > DS_MAX_LOGIC_ID) || (logic_id < 0)) {
        roce_err("Logic id is invalid. (logic_id=%d)", logic_id);
        return -EINVAL;
    }
    ret = hccn_check_usr_identify();
    if (ret) {
        roce_err("Check usr identify failed. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    size_in = sizeof(struct ds_optical_mode_info);
    DSMI_SET_TRANS_DATA(trans_data, DS_SET_OPTICAL_MODE, (char *)data, size_in, NULL, &size_out);

    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err("Dsmi set optical mode failed. (ret=%d; logic_id=%d; mode=%u)", ret, logic_id, data->mode);
        return ret;
    }

    if (trans_data.result != 0) {
        roce_err("Dsmi set optical mode failed. (result=%d; logic_id=%d; mode=%u)", trans_data.result, logic_id,
                 data->mode);
    }

    return trans_data.result;
}

int dsmi_scdr_reset_info_cmd_send(int logic_id, struct ds_scdr_reset_info *info, enum ds_net_opcode_type_ext cmd)
{
    int size_out = sizeof(struct ds_scdr_reset_info);
    struct ds_trans_data data_info = {0};

    DSMI_CHECK_PTR_VALID_RETURN_VAL(info, -EINVAL);
    DSMI_COMMON_SET_SEND_DATA(data_info, cmd, info, sizeof(struct ds_scdr_reset_info), info, &size_out);

    return dsmi_network_common_send(logic_id, 0, &data_info);
}

int dsmi_set_scdr_reset_info(int logic_id, struct ds_scdr_reset_info *info)
{
    int ret;

    ret = dsmi_scdr_reset_info_cmd_send(logic_id, info, DS_SET_CDR_RESET_INFO);
    if (ret) {
        roce_err("Dsmi get cdr reset info failed.(ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    // 仅HUYANG支持reset info设置
    if (info->retimer_type != DS_CDR_RETIMER_TYPE_HUYANG) {
        roce_err("Dsmi get scdr reset info got unsupported cdr type. (retimer_type=%u; logic_id=%d)",
                 info->retimer_type, logic_id);
        return UDA_DSMI_CDR_NOT_SUPPORT_ERROR;
    }

    return 0;
}

int dsmi_get_scdr_reset_info(int logic_id, struct ds_scdr_reset_info *info)
{
    int ret;

    ret = dsmi_scdr_reset_info_cmd_send(logic_id, info, DS_GET_CDR_RESET_INFO);
    if (ret) {
        roce_err("Dsmi get cdr reset info failed.(ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    // 仅HUYANG读取的reset info有效
    if (info->retimer_type != DS_CDR_RETIMER_TYPE_HUYANG) {
        roce_err("Dsmi get scdr reset info got unsupported cdr type. (retimer_type=%u; logic_id=%d)",
                 info->retimer_type, logic_id);
        return UDA_DSMI_CDR_NOT_SUPPORT_ERROR;
    }
    return 0;
}

int dsmi_get_scdr_esd_status_cmd(int logic_id, struct sds_esd_info_array *info, int buf_size)
{
    struct ds_trans_data data_info = {0};

    DSMI_CHECK_PTR_VALID_RETURN_VAL(info, -EINVAL);
    DSMI_COMMON_SET_SEND_DATA(data_info, DS_GET_SCDR_ESD_STATUS, NULL, 0, info, &buf_size);
    return dsmi_network_common_send(logic_id, 0, &data_info);
}

int dsmi_handle_multi_roce_ports(int logic_id, struct ds_roce_port_info *info, int cmd_mode)
{
    int ret;
    int size_out = sizeof(struct ds_roce_port_info);
    struct ds_trans_data data_info = {0};
    enum ds_net_opcode_type_ext cmd;

    DSMI_CHECK_PTR_VALID_RETURN_VAL(info, -EINVAL);
    cmd = (cmd_mode == SET_MODE) ? DS_SET_MULTI_ROCE_PORTS : DS_GET_MULTI_ROCE_PORTS;
    DSMI_COMMON_SET_SEND_DATA(data_info, cmd, info, sizeof(struct ds_roce_port_info), info, &size_out);

    ret = dsmi_network_common_send(logic_id, 0, &data_info);
    if (ret) {
        roce_err("Dsmi %s multi roce ports failed. (ret=%d; logic_id=%d)", cmd_mode == SET_MODE ? "set" : "get", ret,
                 logic_id);
        return ret;
    }

    return 0;
}

int dsmi_get_cdr_fmea(int logic_id)
{
    int ret;
    int size_out = 0;
    struct ds_trans_data data = {0};

    if (logic_id > DS_MAX_LOGIC_ID || logic_id < 0) {
        roce_err("Logic id is invalid. (logic_id=%d)", logic_id);
        return -EINVAL;
    }

    DSMI_SET_TRANS_DATA(data, DS_GET_HUYANG_FMEA, NULL, 0, NULL, &size_out);
    ret = dsmi_network_transmission_channel(logic_id, &data);
    if (ret) {
        roce_err("Dsmi get cdr fmea channel failed. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }
    if (data.result) {
        roce_err("Dsmi get cdr fmea execute failed. (result=%d; logic_id=%d)", data.result, logic_id);
    }
    return data.result;
}

int dsmi_operate_diagnostic_mode(int logic_id, struct ds_diag_mode_params *params, enum ds_net_opcode_type_ext cmd)
{
    int size_out = sizeof(struct ds_diag_mode_params);
    struct ds_trans_data data_info = {0};

    DSMI_CHECK_PTR_VALID_RETURN_VAL(params, -EINVAL);
    DSMI_COMMON_SET_SEND_DATA(data_info, cmd, params, sizeof(struct ds_diag_mode_params), params, &size_out);

    return dsmi_network_common_send(logic_id, 0, &data_info);
}

int dsmi_set_diagnostic_i2c_access_cmd(int logic_id, struct i2c_access_params *params, enum ds_net_opcode_type_ext cmd)
{
    int size_out = sizeof(struct i2c_access_params);
    struct ds_trans_data data_info = {0};

    DSMI_CHECK_PTR_VALID_RETURN_VAL(params, -EINVAL);
    DSMI_COMMON_SET_SEND_DATA(data_info, cmd, params, sizeof(struct i2c_access_params), params, &size_out);

    return dsmi_network_common_send(logic_id, 0, &data_info);
}

int dsmi_operate_tc_qos_info(int logic_id, char *params, size_t len, enum ds_net_opcode_type_ext cmd)
{
    int size_out = (int)len;
    struct ds_trans_data data_info = {0};

    DSMI_CHECK_PTR_VALID_RETURN_VAL(params, -EINVAL);
    DSMI_COMMON_SET_SEND_DATA(data_info, cmd, params, len, params, &size_out);

    return dsmi_network_common_send(logic_id, 0, &data_info);
}

int dsmi_get_optical_snr_query_info(int logic_id, struct ds_optical_snr_query_info *info)
{
    unsigned int size_out = sizeof(struct ds_optical_snr_query_info);
    struct ds_trans_data data_info = {0};

    DSMI_CHECK_PTR_VALID_RETURN_VAL(info, -EINVAL);
    DSMI_COMMON_SET_SEND_DATA(data_info, DS_GET_XSFP_SNR_QUERY_INFO, info, size_out, info, &size_out);

    return dsmi_network_common_send(logic_id, 0, &data_info);
}

int dsmi_set_optical_snr_query_info(int logic_id, struct ds_optical_snr_query_info *info)
{
    unsigned int size_out = sizeof(struct ds_optical_snr_query_info);
    struct ds_trans_data data_info = {0};

    DSMI_CHECK_PTR_VALID_RETURN_VAL(info, -EINVAL);
    DSMI_COMMON_SET_SEND_DATA(data_info, DS_SET_XSFP_SNR_QUERY_INFO, info, size_out, info, &size_out);

    return dsmi_network_common_send(logic_id, 0, &data_info);
}

int dsmi_set_mem_reserve(int logic_id, struct reserve_mem_info *info)
{
    unsigned int size_in = sizeof(struct reserve_mem_info);
    unsigned int size_out = sizeof(struct reserve_mem_info);
    struct ds_trans_data trans_data = {0};
    struct reserve_mem_info out_info = {0};
    int ret;

    if (logic_id > DS_MAX_LOGIC_ID || logic_id < 0) {
        roce_err("%s logic id:%d is invalid! expect [0]-[%d]", __func__, logic_id, DS_MAX_LOGIC_ID);
        return -EINVAL;
    }

    if (info == NULL) {
        roce_err("set_mem_reserve info is null. (logic_id=%d)", logic_id);
        return -EINVAL;
    }

    DSMI_SET_TRANS_DATA(trans_data, DS_SET_MEM_RESERVE, info, size_in, &out_info, &size_out);

    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err("set_mem_reserve dsmi trans fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    if (trans_data.result != 0) {
        roce_err("set_mem_reserve dsmi trans res error. (result=%d)", trans_data.result);
        return trans_data.result;
    }

    /* Extract result from returned data */
    if (size_out > 0) {
        info->result = out_info.result;
    }

    roce_info("dsmi_set_mem_reserve success. (logic_id=%d; type=%u; mem_size=%uMB; page_size=%uKB; result=%u)",
              logic_id, info->type, info->mem_size, info->page_size, info->result);

    return 0;
}

int dsmi_get_mem_reserve(int logic_id, struct reserve_mem_info *info)
{
    struct ds_trans_data trans_data = {0};
    unsigned int size_out = sizeof(struct reserve_mem_info);
    int ret;

    if (logic_id > DS_MAX_LOGIC_ID || logic_id < 0) {
        roce_err("%s logic id:%d is invalid! expect [0]-[%d]", __func__, logic_id, DS_MAX_LOGIC_ID);
        return -EINVAL;
    }

    if (info == NULL) {
        roce_err("get_mem_reserve info is null. (logic_id=%d)", logic_id);
        return -EINVAL;
    }

    DSMI_SET_TRANS_DATA(trans_data, DS_GET_MEM_RESERVE, NULL, 0, (char *)info, &size_out);

    ret = dsmi_network_transmission_channel(logic_id, &trans_data);
    if (ret) {
        roce_err("get_mem_reserve dsmi trans fail. (ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }

    if (trans_data.result != 0) {
        roce_err("get_mem_reserve dsmi trans res error. (result=%d)", trans_data.result);
        return trans_data.result;
    }

    return trans_data.result;
}