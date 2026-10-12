/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <fcntl.h>
#include <math.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/mman.h>
#endif
#include "securec.h"
#include "dsmi_common_interface.h"

#ifndef _WIN32
#include "ascend_hal.h"
#endif
#include "dsmi_network_interface.h"
#include "dcmi_fault_manage_intf.h"
#include "dcmi_log.h"
#include "dcmi_init_basic.h"
#include "dcmi_product_judge.h"
#include "dcmi_virtual_intf.h"
#include "dcmi_product_judge.h"
#include "dcmi_i2c_operate.h"
#include "dcmi_inner_cfg_manage.h"
#include "dcmi_elabel_operate.h"
#include "dcmi_mcu_intf.h"
#include "dcmi_inner_info_get.h"
#include "dcmi_environment_judge.h"
#include "dcmi_hot_reset_intf.h"
#include "dcmi_ipmi.h"
#include "dcmi_npu_link_intf.h"
#include "dcmi_network_intf.h"
#include "dcmi_basic_info_intf.h"

// 天成 POD  1D 2+6场景 David NPU节点
const struct dcmi_port_list_info DCMI_A5_POD_1D_port_list_info = {
    .die_nums = 2,
    .die_list_data[0] = {.die_index = 0,
                         .port_nums = 9,
                         .port_list_data = {{0, UB_MODE},
                                            {1, UB_MODE},
                                            {2, UB_MODE},
                                            {3, UB_MODE},
                                            {4, UB_MODE},
                                            {5, UB_MODE},
                                            {6, UB_MODE},
                                            {7, UB_MODE},
                                            {8, UB_MODE}}},
    .die_list_data[1] = {.die_index = 1,
                         .port_nums = 8,
                         .port_list_data = {{0, UB_MODE},
                                            {1, UB_MODE},
                                            {2, UB_MODE},
                                            {3, UB_MODE},
                                            {4, UB_MODE},
                                            {5, UB_MODE},
                                            {6, UB_MODE},
                                            {7, UB_MODE}}}};

// A+K  0+8 SERVER DAVID NPU节点/port 8 UB
const struct dcmi_port_list_info DCMI_A_K_950_0_8_port_list_info = {
    .die_nums = 2,
    .die_list_data[0] = {.die_index = 0,
                         .port_nums = 9,
                         .port_list_data = {{0, UB_MODE},
                                            {1, UB_MODE},
                                            {2, UB_MODE},
                                            {3, UB_MODE},
                                            {4, UB_MODE},
                                            {5, UB_MODE},
                                            {6, UB_MODE},
                                            {7, UB_MODE},
                                            {8, UB_MODE}}},
    .die_list_data[1] = {.die_index = 1,
                         .port_nums = 9,
                         .port_list_data = {{0, UB_MODE},
                                            {1, UB_MODE},
                                            {2, UB_MODE},
                                            {3, UB_MODE},
                                            {4, UB_MODE},
                                            {5, UB_MODE},
                                            {6, UB_MODE},
                                            {7, UB_MODE},
                                            {8, UB_MODE}}}};

// A+K  0+8 SERVER DAVID NPU节点/port 8 2*UBoE X2
const struct dcmi_port_list_info DCMI_A_K_950_0_8_UBOE_port_list_info = {
    .die_nums = 2,
    .die_list_data[0] = {.die_index = 0,
                         .port_nums = 9,
                         .port_list_data = {{0, UB_MODE},
                                            {1, UB_MODE},
                                            {2, UB_MODE},
                                            {3, UB_MODE},
                                            {4, UB_MODE},
                                            {5, UB_MODE},
                                            {6, UB_MODE},
                                            {7, UB_MODE},
                                            {8, UB_MODE}}},
    .die_list_data[1] = {.die_index = 1,
                         .port_nums = 9,
                         .port_list_data = {{0, UB_MODE},
                                            {1, UB_MODE},
                                            {2, UB_MODE},
                                            {3, UB_MODE},
                                            {4, UB_MODE},
                                            {5, UB_MODE},
                                            {6, UB_MODE},
                                            {7, UB_MODE},
                                            {8, UBOE_MODE}}}};

// A+K  0口 SERVER DAVID NPU节点/port 8 UB
const struct dcmi_port_list_info DCMI_A_K_950_0_0_port_list_info = {
    .die_nums = 2,
    .die_list_data[0] = {.die_index = 0, .port_nums = 1, .port_list_data = {{0, UB_MODE}}},
    .die_list_data[1] = {.die_index = 1,
                         .port_nums = 9,
                         .port_list_data = {{0, UB_MODE},
                                            {1, UB_MODE},
                                            {2, UB_MODE},
                                            {3, UB_MODE},
                                            {4, UB_MODE},
                                            {5, UB_MODE},
                                            {6, UB_MODE},
                                            {7, UB_MODE},
                                            {8, UB_MODE}}}};

// A+K  0口 SERVER DAVID NPU节点/port 8 2*UBoE X2
const struct dcmi_port_list_info DCMI_A_K_950_0_0_UBOE_port_list_info = {
    .die_nums = 2,
    .die_list_data[0] = {.die_index = 0, .port_nums = 1, .port_list_data = {{0, UB_MODE}}},
    .die_list_data[1] = {.die_index = 1,
                         .port_nums = 9,
                         .port_list_data = {{0, UB_MODE},
                                            {1, UB_MODE},
                                            {2, UB_MODE},
                                            {3, UB_MODE},
                                            {4, UB_MODE},
                                            {5, UB_MODE},
                                            {6, UB_MODE},
                                            {7, UB_MODE},
                                            {8, UBOE_MODE}}}};

int dcmi_get_npu_device_gateway(int card_id, int device_id, enum dcmi_port_type input_type, int port_id,
                                struct dcmi_ip_addr *gateway)
{
    int ret;
    int device_logic_id = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_gateway_addr(device_logic_id, input_type, port_id, (ip_addr_t *)gateway);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_gateway_addr failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_ip(int card_id, int device_id, enum dcmi_port_type input_type, int port_id,
                           struct dcmi_ip_addr *ip, struct dcmi_ip_addr *mask)
{
    int ret;
    int device_logic_id = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_device_ip_address(device_logic_id, input_type, port_id, (ip_addr_t *)ip, (ip_addr_t *)mask);
    if (ret == DSMI_ERR_NO_DEVICE) {
        gplog(LOG_ERR, "call dsmi_get_device_ip_address failed. IP not exist. err is %d.", ret);
        return DCMI_ERR_CODE_CONFIG_INFO_NOT_EXIST;
    } else if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_device_ip_address failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_network_health(int card_id, int device_id, enum dcmi_rdfx_detect_result *result)
{
    int ret;
    int device_logic_id = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_network_health(device_logic_id, (DSMI_NET_HEALTH_STATUS *)(void *)result);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_network_health failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_fan_count(int card_id, int device_id, int *count)
{
    int ret;
    int device_logic_id = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_fan_count(device_logic_id, count);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_fan_count failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_fan_speed(int card_id, int device_id, int fan_id, int *speed)
{
    int ret;
    int device_logic_id = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_fan_speed(device_logic_id, fan_id, speed);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_fan_speed failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_user_config(int card_id, int device_id, const char *config_name, unsigned int buf_size,
                                    unsigned char *buf)
{
    int ret;
    int device_logic_id = 0;
    if (strcmp(config_name, "cpu_num_cfg") == 0 && buf_size != DCMI_CPU_NUM_CFG_LEN) {
        gplog(LOG_ERR, "buf_size invalid, the value should be 16!");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    } else {
#ifndef ENABLE_EQUIPMENT
        ret = dcmiv2_get_device_logic_id(&device_logic_id, card_id, device_id);
#else
        ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
#endif
    }

    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_user_config(device_logic_id, config_name, buf_size, buf);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_user_config failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_i2c_get_elable_info(int card_id, int item_type, char *elable_data, int *len)
{
    int err = 0;
    unsigned short data_len;
    err = dcmi_elabel_get_data(item_type, (unsigned char *)elable_data, MAX_LENTH, &data_len);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_elabel_get_data failed.%d.\n", err);
        return err;
    } else {
        *len = data_len;
    }
    return err;
}

int dcmi_i2c_get_npu_device_elable_info(int card_id, struct dcmi_elabel_info *elabel_info)
{
    int err;
    int length = 0;

    err = dcmi_i2c_get_elable_info(card_id, ELABEL_ITEM_ID_BOARD_PRD_NAME, elabel_info->product_name, &length);
    if (err != DCMI_OK || strlen(elabel_info->product_name) == 0) {
        gplog(LOG_ERR, "dcmi_i2c_get_elable_info product name failed.%d.", err);
        dcmi_set_default_elabel_str(elabel_info->product_name, sizeof(elabel_info->product_name));
    }

    err = dcmi_i2c_get_elable_info(card_id, ELABEL_ITEM_MODEL, elabel_info->model, &length);
    if (err != DCMI_OK || strlen(elabel_info->model) == 0) {
        gplog(LOG_ERR, "dcmi_i2c_get_elable_info model failed.%d.", err);
        dcmi_set_default_elabel_str(elabel_info->model, sizeof(elabel_info->model));
    }

    err = dcmi_i2c_get_elable_info(card_id, ELABEL_ITEM_ID_BOARD_MANF, elabel_info->manufacturer, &length);
    if (err != DCMI_OK || strlen(elabel_info->manufacturer) == 0) {
        gplog(LOG_ERR, "dcmi_i2c_get_elable_info manufacturer failed.%d.", err);
        dcmi_set_default_elabel_str(elabel_info->manufacturer, sizeof(elabel_info->manufacturer));
    }

    err = dcmi_i2c_get_elable_info(card_id, ELABEL_ITEM_ID_BOARD_SN, elabel_info->serial_number, &length);
    if (err != DCMI_OK || strlen(elabel_info->serial_number) == 0) {
        gplog(LOG_ERR, "dcmi_i2c_get_elable_info serial_number failed.%d.", err);
        dcmi_set_default_elabel_str(elabel_info->serial_number, sizeof(elabel_info->serial_number));
    }

    return DCMI_OK;
}

int dcmi_get_npu_device_elable_info(int card_id, int device_id, struct dcmi_elabel_info *elabel_info)
{
    int err, ret;
    int device_logic_id = 0;
    int dcmi_elabel_sn = 0;
    int dcmi_elabel_mf = 0;
    int dcmi_elabel_pn = 0;
    int dcmi_elabel_model = 0;
    int length = 0;

    err = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_logic_id failed.%d.", err);
        return err;
    }

    dcmi_get_elabel_item_it(&dcmi_elabel_sn, &dcmi_elabel_mf, &dcmi_elabel_pn, &dcmi_elabel_model);
    err = dsmi_dft_get_elable(device_logic_id, dcmi_elabel_sn, elabel_info->serial_number, &length);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dsmi_dft_get_elable sn failed.%d.", err);
        dcmi_set_default_elabel_str(elabel_info->serial_number, sizeof(elabel_info->serial_number));
    }
    err = dsmi_dft_get_elable(device_logic_id, dcmi_elabel_mf, elabel_info->manufacturer, &length);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dsmi_dft_get_elable mf failed.%d.", err);
        dcmi_set_default_elabel_str(elabel_info->manufacturer, sizeof(elabel_info->manufacturer));
    }
    err = dsmi_dft_get_elable(device_logic_id, dcmi_elabel_pn, elabel_info->product_name, &length);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dsmi_dft_get_elable pn failed.%d.", err);
        dcmi_set_default_elabel_str(elabel_info->product_name, sizeof(elabel_info->product_name));
    }
    err = dsmi_dft_get_elable(device_logic_id, dcmi_elabel_model, elabel_info->model, &length);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dsmi_dft_get_elable model failed.%d.", err);
        dcmi_set_default_elabel_str(elabel_info->model, sizeof(elabel_info->model));
    }

    ret = dcmi_convert_error_code(err);
    if (ret == DCMI_ERR_CODE_NOT_SUPPORT) {
        return ret;
    }
    return DCMI_OK;
}

int dcmi_ao_get_npu_device_elabel_info(int card_id, int device_id, struct dcmi_elabel_info *elabel_info)
{
    int err;

    if (elabel_info == NULL) {
        gplog(LOG_ERR, "elabel_info is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    err = dcmi_ao_get_elabel_info(card_id, device_id, ELABEL_ITEM_ID_950_BOARD_PRODUCT_NAME, elabel_info->product_name,
                                  MAX_LENTH);
    if (err != DCMI_OK || strlen(elabel_info->product_name) == 0) {
        gplog(LOG_ERR, "dcmi_ao_get_elabel_info product name failed.err is %d.", err);
        dcmi_set_default_elabel_str(elabel_info->product_name, sizeof(elabel_info->product_name));
    }

    err = dcmi_ao_get_elabel_info(card_id, device_id, ELABEL_ITEM_ID_950_BOARD_MODEL, elabel_info->model, MAX_LENTH);
    if (err != DCMI_OK || strlen(elabel_info->model) == 0) {
        gplog(LOG_ERR, "dcmi_ao_get_elabel_info model failed.err is %d.", err);
        dcmi_set_default_elabel_str(elabel_info->model, sizeof(elabel_info->model));
    }

    err = dcmi_ao_get_elabel_info(card_id, device_id, ELABEL_ITEM_ID_950_BOARD_MANUFACTURER, elabel_info->manufacturer,
                                  MAX_LENTH);
    if (err != DCMI_OK || strlen(elabel_info->manufacturer) == 0) {
        gplog(LOG_ERR, "dcmi_ao_get_elabel_info manufacturer failed.err is %d.", err);
        dcmi_set_default_elabel_str(elabel_info->manufacturer, sizeof(elabel_info->manufacturer));
    }

    err = dcmi_ao_get_elabel_info(card_id, device_id, ELABEL_ITEM_ID_950_BOARD_SERIAL_NUMBER,
                                  elabel_info->serial_number, MAX_LENTH);
    if (err != DCMI_OK || strlen(elabel_info->serial_number) == 0) {
        gplog(LOG_ERR, "dcmi_ao_get_elabel_info serial_number failed.err is %d.", err);
        dcmi_set_default_elabel_str(elabel_info->serial_number, sizeof(elabel_info->serial_number));
    }

    return DCMI_OK;
}

int dcmi_get_npu_device_share_enable(int card_id, int device_id, int *enable_flag)
{
    int err;
    int device_logic_id = 0;
    const int device_share_main_cmd = 0x8001;
    const int device_share_sub_cmd = 0;
    unsigned int size = (unsigned int)sizeof(int);

    err = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", err);
        return DCMI_ERR_CODE_INNER_ERR;
    }

    err = dsmi_get_device_info(device_logic_id, device_share_main_cmd, device_share_sub_cmd, (void *)enable_flag,
                               &size);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", err);
    }

    return dcmi_convert_error_code(err);
}

int dcmi_get_npu_device_ssh_enable(int card_id, int device_id, int *enable_flag)
{
    int err;
    int device_logic_id = 0;
    unsigned char ssh_status = 0;

    err = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed.%d.\n", err);
        return DCMI_ERR_CODE_INNER_ERR;
    }
    err = dsmi_get_user_config(device_logic_id, "ssh_status", 1, &ssh_status);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_user_config failed. err is %d.", err);
        return dcmi_convert_error_code(err);
    }
    *enable_flag = (int)ssh_status;
    return DCMI_OK;
}

int dcmi_get_npu_device_aicpu_count_info(int card_id, int device_id, unsigned char *count)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (count == NULL) {
        gplog(LOG_ERR, "count is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_user_config(card_id, device_id, "aicpu_config", sizeof(unsigned char), count);
    } else {
        gplog(LOG_ERR, "device_type %d is error.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_npu_device_list(int *device_list, int list_size, int *device_count)
{
    int err;
    int count;

    /* Atlas 500 存在不带Asccend 310发货场景 */
    if (access("/run/minid_not_present", F_OK) == 0) {
        *device_count = 0;
        return DCMI_OK;
    }

    err = dsmi_get_device_count(&count);
    if (err != DSMI_OK) {
        gplog(LOG_ERR, "dsmi_get_device_count failed. err is %d.", err);
        return dcmi_convert_error_code(err);
    }

    if (count > list_size) {
        gplog(LOG_ERR, "dsmi_get_device_count count %d list_size %d.", count, list_size);
        return DCMI_ERR_CODE_INNER_ERR;
    }

    err = dsmi_list_device(device_list, count);
    if (err != DSMI_OK) {
        gplog(LOG_ERR, "dsmi_list_device failed. err is %d.", err);
        return dcmi_convert_error_code(err);
    }

    *device_count = count;
    return DCMI_OK;
}

int dcmi_get_npu_fault_event(int card_id, int device_id, int timeout, struct dcmi_event_filter filter,
                             struct dcmi_event *event)
{
#ifndef _WIN32
    int ret;
    int ret_clr;
    int device_logic_id = 0;

    if (dcmi_board_chip_type_is_ascend_950()) {
        ret = dcmiv2_get_device_logic_id(&device_logic_id, card_id, device_id);
    } else {
        ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    }
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. (ret=%d)", ret);
        return ret;
    }

    ret = dsmi_read_fault_event(device_logic_id, timeout, *(struct dsmi_event_filter *)&filter,
                                (struct dsmi_event *)event);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "call dsmi_read_fault_event failed. (ret=%d)", ret);
        ret_clr = memset_s(event, sizeof(struct dcmi_event), 0, sizeof(struct dcmi_event));
        if (ret_clr != 0) {
            gplog(LOG_ERR, "memset_s event failed. (ret=%d)", ret_clr);
        }
    }
    return dcmi_convert_error_code(ret);
#else
    gplog(LOG_ERR, "call dsmi_read_fault_event failed. not supprot in windows.");
    return DCMI_ERR_CODE_NOT_SUPPORT;
#endif
}

int dcmi_get_npu_current_fault_event(int card_id, int device_id, struct dcmi_event *event_buf,
                                     int input_event_buf_length, int *output_event_cnt)
{
#ifndef _WIN32
    int ret;
    int ret_clr;
    int device_logic_id = 0;
    struct dsmi_event *dsmi_event_buf = NULL;

    if (dcmi_board_chip_type_is_ascend_950()) {
        ret = dcmiv2_get_device_logic_id(&device_logic_id, card_id, device_id);
    } else {
        ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    }
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. (ret=%d)", ret);
        return ret;
    }

    if (sizeof(struct dcmi_event) != sizeof(struct dsmi_event)) {
        gplog(LOG_ERR, "dcmi_event and dsmi_event size not match.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    dsmi_event_buf = (struct dsmi_event *)calloc(input_event_buf_length, sizeof(struct dsmi_event));
    if (dsmi_event_buf == NULL) {
        return DCMI_ERR_CODE_MEM_OPERATE_FAIL;
    }

    ret = dsmi_get_fault_event(device_logic_id, input_event_buf_length, dsmi_event_buf, output_event_cnt);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "call dsmi_get_fault_event failed. (ret=%d)", ret);
        ret_clr = memset_s(event_buf, input_event_buf_length * sizeof(struct dcmi_event), 0,
                           input_event_buf_length * sizeof(struct dcmi_event));
        if (ret_clr != 0) {
            gplog(LOG_ERR, "memset_s event_buf failed. (ret=%d)", ret_clr);
        }
        free(dsmi_event_buf);
        return dcmi_convert_error_code(ret);
    }

    ret = memcpy_s(event_buf, input_event_buf_length * sizeof(struct dcmi_event), dsmi_event_buf,
                   input_event_buf_length * sizeof(struct dsmi_event));
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "memcpy_s event_buf failed. (ret=%d)", ret);
        free(dsmi_event_buf);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }
    free(dsmi_event_buf);
    return dcmi_convert_error_code(ret);
#else
    gplog(LOG_INFO, "call dsmi_get_fault_event failed. not supprot in windows.");
    return DCMI_ERR_CODE_NOT_SUPPORT;
#endif
}

int dcmi_get_npu_device_dvpp_ratio_info(int card_id, int device_id, struct dcmi_dvpp_ratio *usage)
{
    int err;
    int device_logic_id = 0;
    unsigned int size = (unsigned int)sizeof(int);
    int buf = 0;

    err = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", err);
        return err;
    }

    err = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_DVPP, DSMI_SUB_CMD_DVPP_VDEC_RATE, (void *)&buf, &size);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "dsmi_get_device_info vdec failed. err is %d.", err);
        return dcmi_convert_error_code(err);
    }
    usage->vdec_ratio = buf;

    err = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_DVPP, DSMI_SUB_CMD_DVPP_VPC_RATE, (void *)&buf, &size);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "dsmi_get_device_info vpc failed. err is %d.", err);
        return dcmi_convert_error_code(err);
    }
    usage->vpc_ratio = buf;

    err = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_DVPP, DSMI_SUB_CMD_DVPP_VENC_RATE, (void *)&buf, &size);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "dsmi_get_device_info venc failed. err is %d.", err);
        return dcmi_convert_error_code(err);
    }
    usage->venc_ratio = (err == DSMI_ERR_NOT_SUPPORT) ? (int)((unsigned int)buf | (0x1 << DCMI_VF_FLAG_BIT)) : buf;

    err = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_DVPP, DSMI_SUB_CMD_DVPP_JPEGE_RATE, (void *)&buf, &size);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "dsmi_get_device_info jpege failed. err is %d.", err);
        return dcmi_convert_error_code(err);
    }
    usage->jpege_ratio = (err == DSMI_ERR_NOT_SUPPORT) ? (int)((unsigned int)buf | (0x1 << DCMI_VF_FLAG_BIT)) : buf;

    err = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_DVPP, DSMI_SUB_CMD_DVPP_JPEGD_RATE, (void *)&buf, &size);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "dsmi_get_device_info jpegd failed. err is %d.", err);
        return dcmi_convert_error_code(err);
    }
    usage->jpegd_ratio = buf;

    return dcmi_convert_error_code(err);
}

int dcmi_get_npu_proc_mem_info(int card_id, int device_id, struct dcmi_proc_mem_info *proc_info, int *proc_num,
                               int proc_info_capacity)
{
    int err;
    int i;
    struct dsmi_resource_para para = {0};
    struct dsmi_resource_info info = {0};
    int proc_pids[MAX_PROC_NUM_IN_DEVICE] = {0};
    unsigned long mem_usage = 0;
    int logic_id = 0;

    para.owner_type = DSMI_PROCESS_RESOURCE;
    para.resource_type = DSMI_DEV_PROCESS_PID;
    info.buf = proc_pids;
    info.buf_len = sizeof(int) * MAX_PROC_NUM_IN_DEVICE;

    if (dcmi_board_chip_type_is_ascend_950()) {
        err = dcmiv2_get_device_logic_id(&logic_id, card_id, device_id);
    } else {
        err = dcmi_get_device_logic_id(&logic_id, card_id, device_id);
    }
    if (err != DSMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_logic_id failed. (ret=%d)", err);
        return err;
    }

    err = dsmi_get_resource_info((unsigned int)logic_id, &para, &info);
    if (err != DSMI_OK) {
        gplog(LOG_ERR, "dsmi_get_resource_info get pids failed. (ret=%d)", err);
        return dcmi_convert_error_code(err);
    }

    for (i = 0; (i < MAX_PROC_NUM_IN_DEVICE) && (i < proc_info_capacity) && (proc_pids[i] != 0); i++) {
        para.resource_type = DSMI_DEV_PROCESS_MEM;
        para.owner_id = (unsigned int)proc_pids[i];
        info.buf = &mem_usage;
        info.buf_len = sizeof(mem_usage);
        err = dsmi_get_resource_info((unsigned int)logic_id, &para, &info);
        if (err != DSMI_OK) {
            gplog(LOG_ERR, "dsmi_get_resource_info failed get memory. (ret=%d)", err);
            return dcmi_convert_error_code(err);
        }
        proc_info[i].proc_id = proc_pids[i];
        proc_info[i].proc_mem_usage = mem_usage;
        mem_usage = 0;
    }
    *proc_num = i;
    return DCMI_OK;
}

int dcmi_npu_get_capability_group_info(int card_id, int device_id, int ts_id, int group_id,
                                       struct dcmi_capability_group_info *group_info, int group_count)
{
    int ret;
    int device_logic_id = 0;
    int group_count_num = 0;

    struct dsmi_capability_group_info capability_group_info[DCMI_CAPABILITY_GROUP_MAX_COUNT_NUM] = {0};

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_capability_group_info(device_logic_id, ts_id, group_id, capability_group_info, group_count);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "dsmi_get_capability_group failed. err is %d", ret);
        return dcmi_convert_error_code(ret);
    }

    if (group_id == DCMI_CAPABILITY_GROUP_GROUP_ID_ALL) {
        group_count_num = DCMI_CAPABILITY_GROUP_MAX_COUNT_NUM;
    } else {
        group_count_num = DCMI_CAPABILITY_GROUP_MIN_COUNT_NUM;
    }

    ret = memcpy_s(group_info, group_count_num * sizeof(struct dcmi_capability_group_info), capability_group_info,
                   group_count_num * sizeof(struct dsmi_capability_group_info));
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "memcpy_s dcmi_capability_group_info failed. err is %d.", ret);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }

    return ret;
}

int dcmi_npu_get_capability_group_aicore_usage(int card_id, int device_id, int group_id, int *rate)
{
    int i;
    int ret;
    int count = 0;
    int device_logic_id = 0;
    int ts_id = DSMI_TS_AICORE;
    char aicore_utilization[DCMI_AICORE_NUM_MAX] = {0};
    unsigned int aicore_list_size = DCMI_AICORE_NUM_MAX;
    struct dsmi_capability_group_info capability_group_info = {0};

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_capability_group_info(device_logic_id, ts_id, group_id, &capability_group_info, 1);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "dsmi_get_capability_group failed. err is %d", ret);
        return dcmi_convert_error_code(ret);
    }

    ret = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_TS, DSMI_TS_SUB_CMD_AICORE_UTILIZATION_RATE,
                               (void *)aicore_utilization, &aicore_list_size);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "dsmi_get_device_info failed. err is %d", ret);
        return dcmi_convert_error_code(ret);
    }

    // 根据aicore_mask的bit获取到占用的aic编号
    for (i = 0; (i < (int)aicore_list_size) && ((unsigned int)count < capability_group_info.aicore_number); i++) {
        if (((capability_group_info.aicore_mask[0] >> i) & 1) == 1) {
            *rate += aicore_utilization[i];
            count++;
        }
    }

    if (count != 0) {
        *rate /= count;
    } else {
        *rate = 0;
    }

    return DCMI_OK;
}

int dcmi_get_npu_device_cpu_freq_info(int card_id, int device_id, int *enable_flag)
{
    int ret;
    int device_logic_id = 0;
    unsigned char cpu_freq_status = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed.%d.", ret);
        return DCMI_ERR_CODE_INNER_ERR;
    }
    ret = dsmi_get_user_config(device_logic_id, "get_cpu_freq", CPU_FREQ_UP_CONFIG_LEN, &cpu_freq_status);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_user_config failed. err is %d.", ret);
        return dcmi_convert_error_code(ret);
    }
    *enable_flag = (int)cpu_freq_status;
    return DCMI_OK;
}

#if defined DCMI_VERSION_2
int dcmi_get_device_boot_area(int card_id, int device_id, int *status)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (status == NULL) {
        gplog(LOG_ERR, "status is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_type_is_station()) {
        err = dcmi_get_device_type(card_id, device_id, &device_type);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
            return err;
        }
        if (device_type == MCU_TYPE) {
            return dcmi_mcu_get_boot_area(card_id, status);
        } else {
            gplog(LOG_INFO, "device_type is not support.%d.", device_type);
            return DCMI_ERR_CODE_NOT_SUPPORT;
        }
    } else {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}
#endif

STATIC int check_permission_of_get_vnpu_memory(void *buf)
{
    int ret;
    unsigned int env_flag = ENV_PHYSICAL;

    if (dcmi_check_run_not_root()) {
        gplog(LOG_OP, "Operation not permitted, non-root user is not supported.");
        return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
    }

    if (dcmi_check_run_in_docker()) {
        ret = dcmi_get_environment_flag(&env_flag);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_environment_flag failed. err is %d.", ret);
            return ret;
        }
        if (env_flag != ENV_PHYSICAL_PRIVILEGED_CONTAINER && env_flag != ENV_VIRTUAL_PRIVILEGED_CONTAINER) {
            gplog(LOG_OP, "Operation not permitted, only privileged containers are supported.");
            return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
        }
    }

    if (buf == NULL) {
        gplog(LOG_ERR, "buf is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if ((dcmi_board_chip_type_is_ascend_310p() != TRUE) || (dcmi_board_type_is_soc() == TRUE)) {
        gplog(LOG_OP, "This device does not support getting vnpu memory.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    return DCMI_OK;
}

STATIC int dcmi_get_vnpu_memory(void *buf)
{
    int ret;
    struct dsmi_resource_para para = {0};
    struct dsmi_resource_info info = {0};
    struct dcmi_vdev_query_stru *single_query = NULL;
    unsigned long long data = 0;
    unsigned int vdev_id = 0;

    ret = check_permission_of_get_vnpu_memory(buf);
    if (ret != DCMI_OK) {
        return ret;
    }
    single_query = (struct dcmi_vdev_query_stru *)buf;
    vdev_id = single_query->vdev_id;

    // 310P为DDR内存，无HBM，获取TOTAL
    para.owner_type = DSMI_VDEV_RESOURCE;
    para.resource_type = DSMI_DEV_DDR_TOTAL;
    info.buf = &data;
    info.buf_len = sizeof(data);
    ret = dsmi_get_resource_info(vdev_id, &para, &info);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "Failed to get DDR_TOTAL. err is %d.", ret);
        return dcmi_convert_error_code(ret);
    }
    single_query->query_info.computing.vdev_memory_total = data;

    // 获取FREE
    data = 0;
    para.resource_type = DSMI_DEV_DDR_FREE;
    ret = dsmi_get_resource_info(vdev_id, &para, &info);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "Failed to get DDR_FREE. err is %d.", ret);
        return dcmi_convert_error_code(ret);
    }
    single_query->query_info.computing.vdev_memory_free = data;

    return DCMI_OK;
}

STATIC unsigned int dcmi_convert_qos_sub_cmd_v2(unsigned int qos_main_index, unsigned int qos_sub_index,
                                                unsigned int qos_third_index, unsigned int qos_sub_cmd)
{
    return (((qos_main_index) & ((1U << DCMI_QOS_INDEX_LEN) - 1U)) << DCMI_QOS_MAIN_INDEX_OFFSET) |
           (((qos_sub_index) & ((1U << DCMI_QOS_INDEX_LEN) - 1U)) << DCMI_QOS_SUB_INDEX_OFFSET) |
           (((qos_third_index) & ((1U << DCMI_QOS_INDEX_LEN) - 1U)) << DCMI_QOS_THIRD_INDEX_OFFSET) | (qos_sub_cmd);
}

static int dcmi_info_get_device_logic_id(int card_id, int device_id, int *device_logic_id)
{
    int ret;

    if (dcmi_board_chip_type_is_ascend_950()) {
        ret = dcmiv2_get_device_logic_id(device_logic_id, card_id, device_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmiv2_get_device_logic_id failed. (ret=%d)", ret);
            return ret;
        }
    } else {
        ret = dcmi_get_device_logic_id(device_logic_id, card_id, device_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. (ret=%d)", ret);
            return ret;
        }
    }

    return DCMI_OK;
}

int dcmi_get_npu_device_info(int card_id, int device_id, enum dcmi_main_cmd main_cmd, unsigned int sub_cmd, void *buf,
                             unsigned int *size)
{
    int ret;
    int device_logic_id = 0;

    if (main_cmd == DCMI_MAIN_CMD_SEC && sub_cmd == DCMI_SEC_SUB_CMD_PSS) {
        //  pkcs使能整机生效，与device_id, card_id无关，参数不会使用，此处强制写0
        ret = dsmi_get_device_info(0, (DSMI_MAIN_CMD)main_cmd, sub_cmd, buf, size);
        if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
            gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
        }
        return dcmi_convert_error_code(ret);
    }

    ret = dcmi_info_get_device_logic_id(card_id, device_id, &device_logic_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_info_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    if (main_cmd == DCMI_MAIN_CMD_QOS && sub_cmd == DCMI_QOS_SUB_MASTER_CONFIG) {
        sub_cmd = dcmi_convert_qos_sub_cmd_v2((unsigned int)(((struct dcmi_qos_master_config *)buf)->master), 0, 0,
                                              DCMI_QOS_SUB_MASTER_CONFIG);
    }

    ret = dsmi_get_device_info(device_logic_id, (DSMI_MAIN_CMD)main_cmd, sub_cmd, buf, size);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
        return dcmi_convert_error_code(ret);
    }

    if ((main_cmd == DCMI_MAIN_CMD_VDEV_MNG) && (sub_cmd == DCMI_VMNG_SUB_CMD_GET_VDEV_ACTIVITY)) {
        ret = dcmi_get_vnpu_memory(buf);
        if ((ret != DCMI_OK)) {
            gplog(LOG_ERR, "Get vnpu memory failed. err is %d.", ret);
            return ret;
        }
        return DCMI_OK;
    }

    return (ret == DSMI_ERR_NOT_SUPPORT) ? dcmi_convert_error_code(ret) : DCMI_OK;
}

int dcmiv2_get_npu_device_info(int card_id, int device_id, enum dcmi_main_cmd main_cmd, unsigned int sub_cmd, void *buf,
                               unsigned int *size)
{
    int ret;
    int device_logic_id = 0;

    ret = dcmiv2_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    if (main_cmd == DCMI_MAIN_CMD_QOS && sub_cmd == DCMI_QOS_SUB_MASTER_CONFIG) {
        sub_cmd = dcmi_convert_qos_sub_cmd_v2(((struct dcmi_qos_master_config *)buf)->master, 0, 0,
                                              DCMI_QOS_SUB_MASTER_CONFIG);
    }

    ret = dsmi_get_device_info(device_logic_id, (DSMI_MAIN_CMD)main_cmd, sub_cmd, buf, size);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
        return dcmi_convert_error_code(ret);
    }

    return (ret == DSMI_ERR_NOT_SUPPORT) ? dcmi_convert_error_code(ret) : DCMI_OK;
}

int dcmi_get_npu_device_mac_count(int card_id, int device_id, int *count)
{
    int ret;
    int device_logic_id = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_mac_count(device_logic_id, count);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_mac_count failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_mac(int card_id, int device_id, int mac_id, char *mac_addr, unsigned int len)
{
    int ret;
    int device_logic_id = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_mac_addr(device_logic_id, mac_id, mac_addr, len);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_mac_addr failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_custom_op_secverify_enable(int card_id, int device_id, unsigned char *enable)
{
    int ret;
    int device_logic_id = card_id;
    enum dcmi_unit_type device_type = NPU_TYPE;
    // 仅支持物理机root + 虚机的root + 特权容器root
    if (!(dcmi_is_in_phy_machine_root() || dcmi_is_in_vm_root() || dcmi_is_in_privileged_docker_root())) {
        return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
    }

    if (card_id < 0) {
        gplog(LOG_ERR, "input para is invalid. card_id=%d\n", card_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (enable == NULL) {
        gplog(LOG_ERR, "enable pointer is NULL.\n");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!(dcmi_board_chip_type_is_ascend_910b() || dcmi_board_chip_type_is_ascend_910_93() ||
          dcmi_board_chip_type_is_ascend_950())) {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        ret = dcmiv2_get_device_type(device_logic_id, &device_type);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmiv2_get_device_type failed. err is %d.", ret);
            return ret;
        }
    } else {
        ret = dcmi_get_device_logic_id_and_type(card_id, device_id, &device_type, &device_logic_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_logic_id_and_type failed, err is %d.", ret);
            return ret;
        }
    }

    // chip为mcu时不涉及查询enable
    if (device_type != NPU_TYPE) {
        *enable = 0;
        return DCMI_OK;
    }

    ret = dsmi_get_user_config(device_logic_id, "sign_auth_enable", sizeof(unsigned char), enable);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "call dsmi_get_custom_op_secverify_enable failed. err is %d\n", ret);
        return dcmi_convert_error_code(ret);
    }

    return DCMI_OK;
}

int dcmi_get_custom_op_secverify_cert(int card_id, int device_id, void *buf, unsigned int *size)
{
    int ret;
    int device_logic_id = 0;
    unsigned int certs_size = CUSTOM_OP_SECVERIFY_CERT_CHAIN_SIZE_MAX;
    char *certs_chain_data = NULL;
    char *cert_head = NULL;

    if (buf == NULL || size == NULL) {
        gplog(LOG_ERR, "input para buf is NULL or para size is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    certs_chain_data = (char *)calloc(CUSTOM_OP_SECVERIFY_CERT_CHAIN_SIZE_MAX, sizeof(char));
    if (certs_chain_data == NULL) {
        gplog(LOG_ERR, "The certs_chain_data calloc failed.");
        return DCMI_ERR_CODE_MEM_OPERATE_FAIL;
    }

    ret = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_SEC, DSMI_SEC_SUB_CMD_CUST_SIGN_USER_CERT,
                               certs_chain_data, &certs_size);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
        goto OUT;
    }

    cert_head = certs_chain_data;
    while ((cert_head = strstr(cert_head, CERT_SPLIT_NAME))) {
        (*size) += sizeof(struct cert_show_info);
        cert_head += 1;
    }
    ret = dsmi_get_custom_op_secverify_cert_show_info(device_logic_id, certs_chain_data, certs_size, buf, *size);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "dsmi_get_custom_op_secverify_cert_show_info failed. err is %d.", ret);
        goto OUT;
    }
OUT:
    free(certs_chain_data);
    return dcmi_convert_error_code(ret);
}

int dcmi_get_custom_op_secverify_mode(int card_id, int device_id, unsigned int *mode)
{
    int ret;
    int device_logic_id = card_id;
    unsigned int mode_size = sizeof(unsigned int);

    // 不支持算力切分场景
    if (dcmi_check_vnpu_in_docker_or_virtual(card_id)) {
        gplog(LOG_OP, "This environment does not support get custom op secverify enable info.");
        return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
    }

    if (card_id < 0) {
        gplog(LOG_ERR, "input para is invalid. card_id=%d\n", card_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (mode == NULL) {
        gplog(LOG_ERR, "mode pointer is NULL.\n");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!(dcmi_board_chip_type_is_ascend_910b() || dcmi_board_chip_type_is_ascend_910_93() ||
          dcmi_board_chip_type_is_ascend_950())) {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        device_logic_id = card_id;
    } else {
        ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
            return ret;
        }
    }

    ret = dsmi_get_device_info(device_logic_id, (DSMI_MAIN_CMD)DCMI_MAIN_CMD_SEC, DCMI_SEC_SUB_CMD_CUST_SIGN_FLAG, mode,
                               &mode_size);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d\n", ret);
        return dcmi_convert_error_code(ret);
    }

    return DCMI_OK;
}

int dcmi_get_npu_ub_port_link_status_info(int card_id, int device_id, struct dcmi_ub_port_link_status *ub_status)
{
    int ret;
    int device_logic_id = 0;
    unsigned int out_size = sizeof(struct dcmi_ub_port_link_status);

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_UB, DSMI_UB_INFO_SUB_CMD_PORT_STATUS, (void *)ub_status,
                               &out_size);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_urma_device_cnt(int card_id, int device_id, unsigned int *dev_cnt)
{
    int ret;
    int device_logic_id = 0;
    unsigned int size = (unsigned int)sizeof(int);

    ret = dcmiv2_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_URMA_INFO, DSMI_URMA_CMD_GET_URMA_DEVICE_COUNT,
                               (void *)dev_cnt, &size);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_eid_list_by_urma_dev_index(int card_id, int device_id, unsigned int dev_index,
                                            dcmi_urma_eid_info_t *eid_list, unsigned int *eid_cnt)
{
    int ret;
    int device_logic_id = 0;
    unsigned int copy_cnt = 0;
    dcmi_urma_eid_list_req_t eid_list_req = {0};
    unsigned int size = (unsigned int)sizeof(dcmi_urma_eid_list_req_t);

    ret = dcmiv2_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    eid_list_req.urma_dev_index = dev_index;
    ret = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_URMA_INFO, DSMI_URMA_CMD_GET_EID_LIST,
                               (void *)&eid_list_req, &size);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
    } else if (ret == DSMI_OK) {
        copy_cnt = (*eid_cnt) < eid_list_req.eid_count ? (*eid_cnt) : eid_list_req.eid_count;
        ret = memcpy_s(eid_list, (*eid_cnt) * sizeof(dcmi_urma_eid_info_t), &eid_list_req.urma_dev_eid_list[0],
                       copy_cnt * sizeof(dcmi_urma_eid_info_t));
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call memcpy_s failed. err is %d.", ret);
            return DCMI_ERR_CODE_SECURE_FUN_FAIL;
        }
        (*eid_cnt) = copy_cnt;
        return DCMI_OK;
    }
    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_rdma_bandwidth_info(int card_id, int device_id, int port_id, unsigned int prof_time,
                                     struct dcmi_network_rdma_bandwidth_info *network_rdma_bandwidth_info)
{
    int ret;
    int device_logic_id = 0;
    struct bandwidth_t rdma_bandwidth_info = {0};

    rdma_bandwidth_info.time_interval = prof_time;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_bandwidth(device_logic_id, port_id, &rdma_bandwidth_info);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_bandwidth failed. err is %d.", ret);
    }

    network_rdma_bandwidth_info->tx_bandwidth = rdma_bandwidth_info.tx_bandwidth / NETWORK_RDMA_BYTE_TO_MBYTE;
    network_rdma_bandwidth_info->rx_bandwidth = rdma_bandwidth_info.rx_bandwidth / NETWORK_RDMA_BYTE_TO_MBYTE;

    return dcmi_convert_error_code(ret);
}

int dcmi_check_port_id_valid(int port_id)
{
    return ((port_id >= NETWORK_PORT_COUNT_DEFAULT) || (port_id < 0)) ? DCMI_ERR_CODE_INVALID_PARAMETER : DCMI_OK;
}

int dcmi_get_npu_serdes_quality_info(int card_id, int device_id, unsigned int macro_id,
                                     struct dcmi_serdes_quality_info *serdes_quality_info)
{
    int ret;
    int device_logic_id = 0;
    unsigned int out_size = sizeof(struct dcmi_serdes_quality_info);

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    serdes_quality_info->macro_id = macro_id;
    ret = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_SERDES, DSMI_SERDES_SUB_CMD_QUALITY_INFO,
                               (void *)serdes_quality_info, &out_size);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_component_static_version(int card_id, int device_id, enum dcmi_component_type component_type,
                                                 unsigned char *version_str, unsigned int len)
{
    int ret;
    int device_logic_id = 0;
    unsigned int length;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_upgrade_get_component_static_version(device_logic_id, (DSMI_COMPONENT_TYPE)component_type, version_str,
                                                    len, &length);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_upgrade_get_component_static_version failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_cgroup_info(int card_id, int device_id, struct dcmi_cgroup_info *cg_info)
{
    int ret;
    int device_logic_id = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_device_cgroup_info(device_logic_id, (struct tag_cgroup_info *)cg_info);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_device_cgroup_info failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_board_id(int card_id, int device_id, unsigned int *board_id)
{
    int ret;
    int device_logic_id = 0;
    struct dsmi_board_info_stru board_info = {0};
    const int chip_910_board_id_shif = 4;

    if (dcmi_board_chip_type_is_ascend_950()) {
        ret = dcmiv2_get_device_logic_id(&device_logic_id, card_id, device_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmiv2_get_device_logic_id failed. (ret=%d)", ret);
            return ret;
        }
    } else {
        ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. (ret=%d)", ret);
            return ret;
        }
    }

    ret = dsmi_get_board_info(device_logic_id, &board_info);
    if (ret == DSMI_OK) {
        if (dcmi_board_chip_type_is_ascend_910()) {
            // ascend910接口返回的boardid有误，低4位是预留位，非boardid信息位，需要右移丢掉
            *board_id = (board_info.board_id) >> chip_910_board_id_shif;
        } else {
            *board_id = board_info.board_id;
        }
    } else if (ret != DSMI_ERR_NOT_SUPPORT) {
        gplog(LOG_ERR, "call dsmi_get_board_info failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_component_count(int card_id, int device_id, unsigned int *component_count)
{
    int ret;
    int device_logic_id = 0;

    if (dcmi_board_chip_type_is_ascend_950()) {
        ret = dcmiv2_get_device_logic_id(&device_logic_id, card_id, device_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmiv2_get_device_logic_id failed. (ret=%d)", ret);
            return ret;
        }
    } else {
        ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. (ret=%d)", ret);
            return ret;
        }
    }

    ret = dsmi_get_component_count(device_logic_id, component_count);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_component_count failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_component_list(int card_id, int device_id, enum dcmi_component_type *component_table,
                                       unsigned int component_count)
{
    int ret;
    int device_logic_id = 0;

    if (dcmi_board_chip_type_is_ascend_950()) {
#ifndef ENABLE_EQUIPMENT
        ret = dcmiv2_get_device_logic_id(&device_logic_id, card_id, device_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmiv2_get_device_logic_id failed. (ret=%d)", ret);
            return ret;
        }
#else
        ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. (ret=%d)", ret);
            return ret;
        }
#endif
    } else {
        ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. (ret=%d)", ret);
            return ret;
        }
    }

    ret = dsmi_get_component_list(device_logic_id, (DSMI_COMPONENT_TYPE *)(void *)component_table, component_count);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_component_count failed. (ret=%d)", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_llc_perf_para(int card_id, int device_id, struct dcmi_llc_perf *perf_para)
{
    int ret, device_logic_id = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_llc_perf_para(device_logic_id, (struct dsmi_llc_perf_stru *)perf_para);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_llc_perf_para failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_mainboard_id(int card_id, int device_id, unsigned int *mainboard_id)
{
    int ret;
    int device_logic_id = 0;
    unsigned int device_phy_id = 0;

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "Call dcmi_get_device_logic_id failed, err is %d.", ret);
        return ret;
    }

    ret = dcmi_get_device_phyid_from_logicid((unsigned int)device_logic_id, &device_phy_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_phyid_from_logicid failed. ret is %d", ret);
        return ret;
    }

    ret = dsmi_get_mainboard_id(device_phy_id, mainboard_id);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "Call dsmi_get_mainboard_id failed, err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_device_utilization_rate_v2(int card_id, int device_id, struct dcmi_multi_utilization_info *util_info)
{
    int ret = 0;
    int device_logic_id = 0;
    struct dcmi_multi_utilization_rate multi_util_info = {0};
    unsigned int out_size = sizeof(struct dcmi_multi_utilization_rate);

    if (dcmi_board_chip_type_is_ascend_950()) {
        ret = dcmiv2_get_device_logic_id(&device_logic_id, card_id, device_id);
    } else {
        ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    }
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "Call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_TS, DSMI_TS_SUB_CMD_NPU_MULTI_UTILIZATION_RATE,
                               (void *)&multi_util_info, &out_size);
    if (ret != DSMI_OK) {
        // DSMI_ERR_NOT_SUPPORT means the backend wrote no response, so the
        // caller must not observe it as a valid zero-utilization sample.
        // Keep the log suppressed for the not-support case only.
        if (ret != DSMI_ERR_NOT_SUPPORT) {
            gplog(LOG_ERR, "Call dsmi_get_device_info failed. err is %d.", ret);
        }
        return dcmi_convert_error_code(ret);
    }

    // 赋值
    util_info->aic_util = multi_util_info.aic_avg_util;
    util_info->aiv_util = multi_util_info.aiv_avg_util;
    util_info->aicore_util = multi_util_info.aicore_util;
    util_info->npu_util = multi_util_info.npu_util;
    // npu_util = max(npu_util, aicore_util)
    util_info->npu_util = (util_info->aicore_util > util_info->npu_util) ? util_info->aicore_util : util_info->npu_util;

    return DCMI_OK;
}

int dcmi_get_device_health_950(int card_id, int device_id, unsigned int *health)
{
    int ret;
    unsigned int health_ao_index = 0;
    ret = dcmi_get_npu_device_health(card_id, device_id, health);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_npu_device_health failed. health=%u, err is %d.", *health, ret);
        return ret;
    }
    ret = dcmi_mcu_get_health(card_id, &health_ao_index);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_ao_health failed. err is %d.", ret);
        return ret;
    }
    *health = (*health > health_ao_index) ? *health : health_ao_index;
    return ret;
}

int dcmi_get_npu_pcie_link_bandwidth_info(int card_id, int device_id,
                                          struct dcmi_pcie_link_bandwidth_info *pcie_link_bandwidth_info)
{
    int ret;
    int device_logic_id = 0;
    unsigned int out_size = sizeof(struct dcmi_pcie_link_bandwidth_info);

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_device_info(device_logic_id, DSMI_MAIN_CMD_PCIE_BANDWIDTH, DSMI_PCIE_CMD_GET_BANDWIDTH,
                               (void *)pcie_link_bandwidth_info, &out_size);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}

int dcmi_get_npu_outband_channel_state(int *channel_state)
{
#ifdef _WIN32
    return DCMI_ERR_CODE_NOT_SUPPORT;
#else
    int ret;

    // 用该函数查询复位结果,用以判断通路状态共获取两次
    ret = dcmi_ipmi_get_npu_outband_channel_state(channel_state);
    if (ret != DCMI_OK) {
        sleep(1); // 延迟1s
        ret = dcmi_ipmi_get_npu_outband_channel_state(channel_state);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "Call dcmi_ipmi_get_npu_outband_channel_state failed. err is %d.", ret);
            return ret;
        }
    }

    return DCMI_OK;
#endif
}

int dcmi_get_device_npu_outband_channel_state(int card_id, int device_id, int *channel_state)
{
    int ret = 0;
    unsigned int main_board_id = 0;
    unsigned char reset_state = BMC_RESET_CHIP_FAILED; /* 默认是复位失败 */
    const int CHANNEL_STATUS_SUCCESS = 1;

    main_board_id = dcmi_get_maindboard_id_inner();
    if (dcmi_mainboard_is_arm_910_93(main_board_id) || dcmi_board_chip_type_is_ascend_950_pod() ||
        dcmi_board_chip_type_is_ascend_950_server()) {
        // 910_93场景下bmc不支持直接获取复位状态
        ret = dcmi_get_npu_outband_channel_state(channel_state);
        if (ret != DCMI_OK || *channel_state != CHANNEL_STATUS_SUCCESS) {
            gplog(LOG_ERR, "call dcmi_get_npu_outband_channel_state failed. err is %d.", ret);
            return ret;
        }
        gplog(LOG_OP, "call dcmi_get_npu_outband_channel_state success. card_id=%d, device_id=%d", card_id, device_id);
    } else {
        ret = dcmi_get_npu_outband_reset_state(card_id, device_id, &reset_state);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_npu_outband_reset_state failed. err is %d.", ret);
            return ret;
        }
        bool check_result = ((reset_state == BMC_RESET_CHIP_SUCCESS) || (reset_state == BMC_RESET_CHIP_FAILED) ||
                             (reset_state == BMC_RESET_CHIP_UNKNOWN));
        *channel_state = check_result ? CHANNEL_STATUS_SUCCESS : *channel_state;
    }

    return DCMI_OK;
}

int dcmiv2_query_topo_type(int dev_id1, int dev_id2, int *topo_type)
{
    int ret;
    int64_t value;
    unsigned int phyid1, phyid2;

    ret = dcmiv2_get_chip_phy_id_by_dev_id(dev_id1, &phyid1);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "Get phyid by dev_id1 failed. (dev_id=%d)", dev_id1);
        return ret;
    }

    ret = dcmiv2_get_chip_phy_id_by_dev_id(dev_id2, &phyid2);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "Get phyid by dev_id2 failed. (dev_id=%d)", dev_id2);
        return ret;
    }

    if (phyid1 == phyid2) {
        *topo_type = DCMI_TOPO_TYPE_SELF;
        return DCMI_OK;
    }
    ret = halGetPairPhyDevicesInfo((int)phyid1, (int)phyid2, 0, &value);
    if (ret) {
        gplog(LOG_ERR, "get pcie chain hccs status failed. (ret=%d)", ret);
        *topo_type = DCMI_TOPO_TYPE_BUTT;
        return DCMI_OK;
    }

    *topo_type = dcmi_hal_to_npu_topo_type(value);

    return DCMI_OK;
}

int dcmiv2_get_firmware_version(int dev_id, unsigned char *firmware_version, int len_firmware_version)
{
    int ret;
    // 查询一个组件版本---后续可支持查询多个组件版本
    enum dcmi_component_type comp = DCMI_UPGRADE_ALL_COMPONENT;
    unsigned int len_comp = sizeof(comp) / sizeof(enum dcmi_component_type);

    ret = dcmiv2_get_device_component_list(dev_id, &comp, len_comp);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_component_list failed. (dev_id=%d, ret=%d)", dev_id, ret);
        return ret;
    }
    ret = dcmiv2_get_device_component_static_version(dev_id, comp, firmware_version, len_firmware_version);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "get firmware version failed. (dev_id=%d, ret=%d)", dev_id, ret);
        return ret;
    }

    return DCMI_OK;
}

int dcmiv2_get_device_chip_slot(int dev_id, int *chip_pos_id)
{
    int err, card_id, device_id;
    struct dcmi_card_info *card_info = NULL;
    enum dcmi_unit_type device_type = INVALID_TYPE;

    if (chip_pos_id == NULL) {
        gplog(LOG_ERR, "input chip_pos_id id NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &device_id, dev_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_inner_get_card_id_device_id_from_logicid failed. (ret=%d)", err);
        return err;
    }

    err = dcmi_get_card_info(card_id, &card_info);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_card_info failed. (ret=%d)", err);
        return err;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_card_info failed. (ret=%d)", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        if ((device_id < 0) || (device_id >= MAX_DEVICE_NUM_IN_CARD)) {
            gplog(LOG_ERR, "device_id (%d) is invalid.", device_id);
            return DCMI_ERR_CODE_INVALID_PARAMETER;
        }
        *chip_pos_id = card_info->device_info[device_id].chip_slot;
    } else if (device_type == MCU_TYPE) {
        *chip_pos_id = card_info->mcu_id;
    } else if (device_type == CPU_TYPE) {
        *chip_pos_id = card_info->cpu_id;
    } else {
        return DCMI_ERR_CODE_INNER_ERR;
    }
    return DCMI_OK;
}

int dcmiv2_get_fault_device_num_in_card(int dev_id, int *device_num)
{
    int num_id, card_id, device_id;
    int err;
    struct dcmi_card_info *card_info = NULL;

    if (device_num == NULL) {
        gplog(LOG_ERR, "input device_num is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &device_id, dev_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_inner_get_card_id_device_id_from_logicid failed. (ret=%d)", err);
        return err;
    }

    if (card_id < 0) {
        gplog(LOG_ERR, "device_id %d is invalid.", card_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_get_run_env_init_flag() != TRUE) {
        return DCMI_OK;
    }

    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];
        if (card_info->card_id == card_id) {
            *device_num = card_info->device_loss;
            return DCMI_OK;
        }
    }

    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int dcmi_get_device_port_list_info_inner(unsigned int *main_board_id, struct dcmi_port_list_info *port_list_info)
{
    const struct dcmi_port_list_info *port_list_info_ptr = NULL;

    if (main_board_id == NULL || port_list_info == NULL) {
        gplog(LOG_ERR, "The parameter is invalid. main_board_id[%d] or port_list_info[%d] is NULL",
              main_board_id == NULL, port_list_info == NULL);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    switch (*main_board_id) {
        case DCMI_A5_POD_1D_MAIN_BOARD_ID:
            port_list_info_ptr = &DCMI_A5_POD_1D_port_list_info;
            break;
        case DCMI_A_K_950_0_8_MAIN_BOARD_ID:
            port_list_info_ptr = &DCMI_A_K_950_0_8_port_list_info;
            break;
        case DCMI_A_K_950_0_8_UBOE_MAIN_BOARD_ID:
            port_list_info_ptr = &DCMI_A_K_950_0_8_UBOE_port_list_info;
            break;
        case DCMI_A_K_950_0_0_MAIN_BOARD_ID:
            port_list_info_ptr = &DCMI_A_K_950_0_0_port_list_info;
            break;
        case DCMI_A_K_950_0_0_UBOE_MAIN_BOARD_ID:
            port_list_info_ptr = &DCMI_A_K_950_0_0_UBOE_port_list_info;
            break;
        default:
            gplog(LOG_OP, "This device does not support get device port list info.");
            return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    memcpy_s(port_list_info, sizeof(struct dcmi_port_list_info), port_list_info_ptr,
             sizeof(struct dcmi_port_list_info));

    return DCMI_OK;
}

int dcmi_get_ub_cpu_affinity_by_device_id(int dev_id, char *affinity_cpu, int *length)
{
    int ret;
    int numa_node = 0;
    char name[URMA_MAX_NAME] = {0};
    unsigned int size = sizeof(name);
    char numa_file_path[MAX_LENTH] = {0};
    char content[TOPO_INFO_MAX_LENTH] = {0};
    char affinity_file_path[MAX_LENTH] = {0};

    ret = dsmi_get_device_info(dev_id, DSMI_MAIN_CMD_UB, DCMI_UB_INFO_SUB_CMD_URMA_DEV_NAME, (void *)name, &size);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "dsmi_get_device_info failed. (ret=%d)", ret);
        return dcmi_convert_error_code(ret);
    }

    // snprintf_s 返回的是需要的长度（不包括\0），如果 >= 缓冲区大小，说明被截断了
    ret = snprintf_s(numa_file_path, MAX_LENTH, MAX_LENTH - 1, "%s/%s/device/numa", SYS_CLASS_UBURMA_PATH, name);
    if (ret < 0 || ret >= MAX_LENTH - 1) {
        gplog(LOG_ERR, "failed to snprintf_s. (ret=%d, max_len=%d)", ret, MAX_LENTH);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }

    ret = dcmi_topo_read_line_info_from_file(numa_file_path, content);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "failed to dcmi_topo_read_line_info_from_file. (ret=%d)", ret);
        return ret;
    }

    ret = sscanf_s(content, "%x", &numa_node);
    if (ret <= 0) {
        gplog(LOG_ERR, "failed to sscanf_s. (ret=%d)", ret);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }

    ret = snprintf_s(affinity_file_path, MAX_LENTH, MAX_LENTH - 1, "/sys/devices/system/node/node%d/cpulist",
                     numa_node);
    if (ret < 0 || ret >= MAX_LENTH - 1) {
        gplog(LOG_ERR, "failed to snprintf_s for affinity path. (ret=%d)", ret);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }

    ret = dcmi_topo_read_line_info_from_file(affinity_file_path, affinity_cpu);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "failed to dcmi_topo_read_line_info_from_file. (ret=%d)", ret);
        return ret;
    }

    if (affinity_cpu == NULL) {
        gplog(LOG_ERR, "affinity_cpu is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    *length = strlen(affinity_cpu);
    return DCMI_OK;
}
