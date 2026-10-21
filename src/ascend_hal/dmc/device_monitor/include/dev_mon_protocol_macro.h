/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_MON_PROTOCOL_MACRO_H
#define DEV_MON_PROTOCOL_MACRO_H
#include <string.h>
#include "dm_msg_intf.h"
#include "dev_mon_dmp_mult.h"
#define DM_REP_DEVICE_CAPABILITY_DATA_LEN 5
#define DM_REP_HEALTH_DATA_LEN 1
#define DM_REP_ERROR_CODE_LEN 4
#define DM_REP_CHIP_TEMP_DATA_LEN 2
#define DM_REP_POWER_DATA_LEN 2
#define DM_REP_FW_VERSION_DATA_LEN 3
#define DM_REP_VID_DATA_LEN 2
#define DM_REP_DID_DATA_LEN 2
#define DM_REP_SN_DATA_LEN 0
#define DM_REP_SUBVID_DATA_LEN 2
#define DM_REP_SUBDID_DATA_LEN 2
#define DM_REP_CHIP_VOLTAGE_DATA_LEN 2
#define DM_REP_DEV_NAME 0
#define DM_REP_DRV_VERSION 3
#define DM_REP_DAVINCI_INFO 4
#define DM_REP_PERIPHERAL_DEV_INFO 0
#define DM_REP_ECC_STATISTICS 8
#define DM_REP_SYS_TIME 4
#define DM_REP_FLASH_BASE_INFO_LEN 8
#define DM_REP_FLASH_INFO_LEN 0
#define DM_REP_BORAD_INFO_REQ 0
#define DM_REP_BORAD_INFO_RSP 0
#define DM_REP_PERIPHERAL_DEV_VOLT_REQ 4
#define DM_REP_PERIPHERAL_DEV_VOLT_RSP 4
#define DM_REP_PERIPHERAL_DEV_TEMP_REQ 4
#define DM_REP_PERIPHERAL_DEV_TEMP_RSP 4
#define DM_REP_FAN_INFO_REQ 0
#define DM_REP_FAN_INFO_RSP 0
#define DM_REP_CHIP_ALL_TEMP_DATA_LEN 20

#define DDMP_CMD_HEAD_LEN 12U
#define DDMP_CMD_RESP_HEAD_LEN 12

#define DFT_CTRL_REP_MSG_LEN 4
#define DFT_CTRL_REP_MSG_CHAR_LEN 1

#define DFT_LOAD_TESTLIB_RES_LEN 0

#define P_MAX_LENGTH 4096

#define DMP_COMMON_RES_LEN 0

typedef unsigned char *byte_pointer;

#define BEGIN_DM_COMMAND(pdata, data_len)                          \
    DM_REP_MSG *ob = NULL;                                         \
    DM_MSG_ST res = {0};                                           \
    unsigned char *p = NULL;                                       \
    int ret = 0;                                                   \
    unsigned int i = 0;                                            \
    DEV_MP_MSG_ST *tmp = NULL;                                     \
    ob = (DM_REP_MSG *)malloc(sizeof(DM_REP_MSG));                 \
    if (ob == NULL) {                                              \
        DEV_MON_ERR("ob malloc failed.\n");                        \
        return -ENOMEM;                                            \
    }                                                              \
    ret = memset_s(ob, sizeof(DM_REP_MSG), 0, sizeof(DM_REP_MSG)); \
    if (ret != 0) {                                                \
        free(ob);                                                  \
        ob = NULL;                                                 \
        DEV_MON_ERR("ob memset_s failed.\n");                      \
        return -EINVAL;                                            \
    }                                                              \
    tmp = (DEV_MP_MSG_ST *)(pdata)->msg.data;                      \
    ob->op_fun = tmp->op_fun;                                      \
    ob->op_cmd = tmp->op_cmd;                                      \
    ob->data_length = 0;                                           \
    p = ob->data;                                                  \
    ob->total_length = data_len;                                   \
    (void)res;                                                     \
    (void)i;                                                       \
    (void)p;

#define BEGIN_DM_COMMAND_ERR(pdata, errorcode)                     \
    DM_REP_MSG *ob = NULL;                                         \
    int ret = 0;                                                   \
    DM_MSG_ST res = {0};                                           \
    DEV_MP_MSG_ST *tmp = NULL;                                     \
    ob = (DM_REP_MSG *)malloc(sizeof(DM_REP_MSG));                 \
    if (ob == NULL) {                                              \
        DEV_MON_ERR("ob malloc failed.");                          \
        return -ENOMEM;                                            \
    }                                                              \
    ret = memset_s(ob, sizeof(DM_REP_MSG), 0, sizeof(DM_REP_MSG)); \
    if (ret != 0) {                                                \
        free(ob);                                                  \
        ob = NULL;                                                 \
        DEV_MON_ERR("ob memset_s failed.");                        \
        return -EINVAL;                                            \
    }                                                              \
    tmp = (DEV_MP_MSG_ST *)(pdata)->msg.data;                      \
    ob->err_code = (short)(errorcode);                             \
    ob->op_fun = tmp->op_fun;                                      \
    ob->op_cmd = tmp->op_cmd;                                      \
    ob->total_length = 0;                                          \
    ob->data_length = 0;                                           \
    (void)res;

#ifdef _USE_BIG_ENDIAN
#define ON_COMMAND_DATA_INT(p_data)            \
    for (i = 0; i < sizeof(int); i++) {        \
        *(p++) = ((byte_pointer) & p_data)[i]; \
    }                                          \
    ob->data_length += sizeof(int);

#define ON_COMMAND_DATA_SHORT(p_data)          \
    for (i = 0; i < sizeof(short); i++) {      \
        *(p++) = ((byte_pointer) & p_data)[i]; \
    }                                          \
    ob->data_length += sizeof(short);
#else
#define ON_COMMAND_DATA_INT(p_data)              \
    for (i = 0; i < sizeof(int); i++) {          \
        *(p++) = ((byte_pointer) & (p_data))[i]; \
    }                                            \
    ob->data_length += sizeof(int);

#define ON_COMMAND_DATA_SHORT(p_data)            \
    for (i = 0; i < sizeof(short); i++) {        \
        *(p++) = ((byte_pointer) & (p_data))[i]; \
    }                                            \
    ob->data_length += sizeof(short);

#endif

#define ON_COMMAND_DATA_CHAR(p_data)  \
    *(p++) = (unsigned char)(p_data); \
    ob->data_length += sizeof(unsigned char);

#define ON_COMMAND_DATA_BIN(data_bin, len)                             \
    if (((len) + ob->data_length) > P_MAX_LENGTH) {                    \
        free(ob);                                                      \
        ob = NULL;                                                     \
        return -EINVAL;                                                \
    }                                                                  \
    ret = memmove_s(p, P_MAX_LENGTH - ob->data_length, data_bin, len); \
    if (ret != 0) {                                                    \
        free(ob);                                                      \
        ob = NULL;                                                     \
        return -EINVAL;                                                \
    }                                                                  \
    p += (len);                                                        \
    ob->data_length += (len);

#define ON_COMMAND_DATA_STRING(data_str)                                    \
    unsigned int data_str_len = (unsigned int)(strlen((char *)(data_str))); \
    ON_COMMAND_DATA_BIN(data_str, data_str_len)

#define END_DM_COMMAND_SEND()                                                             \
    res.data = (unsigned char *)ob;                                                       \
    res.data_len = ob->data_length + DDMP_CMD_HEAD_LEN;                                   \
    ret = dm_send_rsp(intf, (DM_ADDR_ST *)data->addr, data->addr_len, &res, data->msgid); \
    free(ob);                                                                             \
    ob = NULL;                                                                            \
    return ret;

#define END_DM_COMMAND()                                \
    res.data = (unsigned char *)ob;                     \
    res.data_len = ob->data_length + DDMP_CMD_HEAD_LEN; \
    ret = rsp_mult_send(intf, data, ob);                \
    free(ob);                                           \
    ob = NULL;                                          \
    return ((unsigned int)ret);

#endif
