/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "dsmi_common_interface.h"
#include "dsmi_common.h"
#include "dev_mon_log.h"
#include "dsmi_product.h"

DSMI_CMD_DEF_DAVINCI_INSTANCE(DMP_MON_CMD_GET_MINI2MCU_STATUS, CMD_LENGTH_INVALID, 0, STATE_MANAGE_TYPE)
DSMI_CMD_DEF_DAVINCI_INSTANCE(DEV_MON_CMD_D_SENDMSG, CMD_LENGTH_INVALID, CMD_LENGTH_INVALID, STATE_MANAGE_TYPE)

#define DSMI_CTRL_PRINT_TIME 20

int __attribute__((weak)) dsmi_product_set_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd,
                                                       unsigned int sub_cmd, const void *buf, unsigned int buf_size)
{
    (void)device_id;
    (void)main_cmd;
    (void)sub_cmd;
    (void)buf;
    (void)buf_size;
    return DRV_ERROR_NOT_SUPPORT;
}

int __attribute__((weak)) dsmi_product_get_device_info(unsigned int device_id, DSMI_MAIN_CMD main_cmd,
                                                       unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)device_id;
    (void)main_cmd;
    (void)sub_cmd;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_i2c_heartbeat_status
 Description  : get mini2mcu heartbeat status
 Return Value : 0 success
              other value is error
*****************************************************************************/
int dsmi_cmd_get_i2c_heartbeat_status(int device_id, unsigned char *status, unsigned int *disconn_cnt)
{
    DM_COMMAND_BIGIN(DMP_MON_CMD_GET_MINI2MCU_STATUS, device_id, 0, (sizeof(char) + sizeof(int)))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(status, (sizeof(char)))
    DM_COMMAND_PUSH_OUT(disconn_cnt, (sizeof(int)))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_passthru_mcu
 Description  : cmd passthru command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_passthru_mcu(int device_id, unsigned char pass_para, struct passthru_message_stru *passthru_message)
{
    unsigned int pass_rsp_len = 0;
    DRV_CHECK_RETV(passthru_message, DRV_ERROR_PARA_ERROR)
    DRV_CHECK_RETV((passthru_message->src_len <= sizeof(struct dmp_message_stru)), DRV_ERROR_PARA_ERROR)

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SENDMSG, device_id, (unsigned short)(passthru_message->src_len + 1UL),
                     sizeof(struct dmp_message_stru))
    DM_COMMAND_ADD_REQ(&pass_para, sizeof(unsigned char))
    DM_COMMAND_ADD_REQ(&(passthru_message->src_message.data.req), (passthru_message->src_len))
    DM_COMMAND_SEND()
    DM_COMMAND_GET_RSP_LEN(&pass_rsp_len)
    if (pass_rsp_len > DSMI_RSP_MSG_DATA_LEN) {
        DEV_MON_ERR("devid %d passthru rsp len %u is out of compliance\n", device_id, pass_rsp_len);
        dsmi_cmd_req_free(dm_dmp);
        return DRV_ERROR_PARA_ERROR;
    }
    DM_COMMAND_PUSH_OUT(&(passthru_message->dest_message.data.rsp), (pass_rsp_len + (unsigned int)DMP_MSG_HEAD_LENGTH))
    DM_COMMAND_END()
}

int dsmi_get_mini2mcu_heartbeat_status(int device_id, unsigned char *status, unsigned int *disconn_cnt)
{
    unsigned int tmp_disconn_cnt = 0;
    unsigned char tmp_status = 0;
    int ret;

    if (status == NULL || disconn_cnt == NULL) {
        DEV_MON_ERR("devid %d input para of query_heartbeat_status is error\n", device_id);
        return -EINVAL;
    }

    ret = dsmi_cmd_get_i2c_heartbeat_status(device_id, &tmp_status, &tmp_disconn_cnt);
    if (ret != 0) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "devid %d get i2c heartbeat status failed ret = %d\n", device_id, ret);
        return ret;
    }

    *status = tmp_status;
    *disconn_cnt = tmp_disconn_cnt;

    return ret;
}

int dsmi_passthru_mcu(int device_id, struct passthru_message_stru *passthru_message)
{
    static struct timespec tp_last = {0};
    static struct timespec tp_now = {0};
    static unsigned long counts = 0;
    unsigned char pass_para;
    long time_gap;

    if (passthru_message == NULL) {
        DEV_MON_ERR("devid %d dsmi_passthru_mcu parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

    counts++;
    (void)clock_gettime(CLOCK_MONOTONIC, &tp_now);
    time_gap = tp_now.tv_sec - tp_last.tv_sec;
    if (time_gap > DSMI_CTRL_PRINT_TIME) {
        tp_last.tv_sec = tp_now.tv_sec;
        DEV_MON_DEBUG("Dsmi transmit to mcu. (user_id=%d; device_id=%d; rw_flag=%d; transmit_count=%lu)\n", getuid(),
                      device_id, passthru_message->rw_flag, counts);
    }

    if (passthru_message->rw_flag > 1) { // 1 rw_flag support  0 read ,1 write , if greater than 1, then not support
        DEV_MON_WARNING("rw_flag is out of compliance. (device_id=%d; rw_flag=%u)\n", device_id,
                        passthru_message->rw_flag);
        return DRV_ERROR_NOT_SUPPORT;
    }
    pass_para = (unsigned char)(passthru_message->rw_flag << 7U) |
                (unsigned char)(MCU); // 7 if rw_flag equal 1 then offset 7 bit

    return dsmi_cmd_passthru_mcu(device_id, pass_para, passthru_message);
}

bool __attribute__((weak)) is_product_user_config_item_by_name(const char *config_name, unsigned char config_name_len)
{
    (void)config_name;
    (void)config_name_len;
    return false;
}

int __attribute__((weak)) dsmi_product_set_user_config(int device_id, const char *config_name, unsigned int buf_size,
                                                       unsigned char *buf)
{
    (void)device_id;
    (void)config_name;
    (void)buf_size;
    (void)buf;
    return DRV_ERROR_NOT_SUPPORT;
}

int __attribute__((weak)) dsmi_product_get_user_config(int device_id, const char *config_name, unsigned int buf_size,
                                                       unsigned char *buf)
{
    (void)device_id;
    (void)config_name;
    (void)buf_size;
    (void)buf;
    return DRV_ERROR_NOT_SUPPORT;
}

int __attribute__((weak)) dsmi_product_clear_user_config(int device_id, const char *config_name)
{
    (void)device_id;
    (void)config_name;
    return DRV_ERROR_NOT_SUPPORT;
}
