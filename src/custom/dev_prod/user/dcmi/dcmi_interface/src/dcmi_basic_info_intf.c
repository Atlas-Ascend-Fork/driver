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
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "securec.h"
#include "dsmi_common_interface.h"
#include "dcmi_interface_api.h"
#include "dcmi_init_basic.h"
#include "dcmi_fault_manage_intf.h"
#include "dcmi_log.h"
#include "dcmi_os_adapter.h"
#include "dcmi_i2c_operate.h"
#include "dcmi_inner_cfg_manage.h"
#include "dcmi_mcu_intf.h"
#include "dcmi_elabel_operate.h"
#include "dcmi_product_judge.h"
#include "dcmi_hot_reset_intf.h"
#include "dcmi_inner_info_get.h"
#include "dcmi_permission_judge.h"
#include "dcmi_environment_judge.h"
#include "dcmi_basic_info_intf.h"

int dcmi_get_device_logic_id(int *device_logic_id, int card_id, int device_id)
{
    int num_id;
    struct dcmi_card_info *card_info = NULL;
    bool unsupported_type = false;

    if (device_logic_id == NULL) {
        gplog(LOG_ERR, "device_logic_id is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if ((card_id < 0) || (device_id < 0)) {
        gplog(LOG_ERR, "input para is invalid. card_id=%d, device_id=%d", card_id, device_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    if (dcmi_get_run_env_init_flag() != TRUE) {
        return DCMI_ERR_CODE_NOT_REDAY;
    }

    if (g_board_details.device_count == 0) {
        gplog(LOG_ERR, "g_board_details.device_count is 0.");
        return DCMI_ERR_CODE_INVALID_DEVICE_ID;
    }

    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];

        if (card_id != card_info->card_id) {
            continue;
        }

        if (device_id >= card_info->device_count) {
            gplog(LOG_ERR, "card (%d) device (%d) >= device_count(%d).", card_id, device_id, card_info->device_count);
            unsupported_type = (card_info->mcu_id != DCMI_INVALID_VALUE && device_id == card_info->mcu_id) ||
                               (card_info->cpu_id != DCMI_INVALID_VALUE && device_id == card_info->cpu_id);
            if (unsupported_type) {
                return DCMI_ERR_CODE_NOT_SUPPORT;
            }

            return DCMI_ERR_CODE_INVALID_PARAMETER;
        }

        *device_logic_id = card_info->device_info[device_id].logic_id;

        return DCMI_OK;
    }

    gplog(LOG_ERR, "Can not find card (%d) device (%d). Please check the input parameter.", card_id, device_id);
    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int dcmiv2_get_device_logic_id(int *device_logic_id, int card_id, int device_id)
{
    int num_id;
    struct dcmi_card_info *card_info = NULL;
    bool unsupported_type = false;

    if (device_logic_id == NULL) {
        gplog(LOG_ERR, "device_logic_id is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if ((card_id < 0) || (device_id < 0)) {
        gplog(LOG_ERR, "input para is invalid. card_id=%d, device_id=%d", card_id, device_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (dcmi_get_run_env_init_flag() != TRUE) {
        return DCMI_ERR_CODE_NOT_REDAY;
    }

    if (g_board_details.device_count == 0) {
        gplog(LOG_ERR, "g_board_details.device_count is 0.");
        return DCMI_ERR_CODE_INVALID_DEVICE_ID;
    }
    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];
        if (card_id != card_info->card_id) {
            continue;
        }

        if (device_id >= card_info->device_count) {
            gplog(LOG_ERR, "card (%d) device (%d) >= device_count(%d).", card_id, device_id, card_info->device_count);
            unsupported_type = (card_info->mcu_id != DCMI_INVALID_VALUE && device_id == card_info->mcu_id) ||
                               (card_info->cpu_id != DCMI_INVALID_VALUE && device_id == card_info->cpu_id);
            if (unsupported_type) {
                return DCMI_ERR_CODE_NOT_SUPPORT;
            }

            return DCMI_ERR_CODE_INVALID_PARAMETER;
        }

        *device_logic_id = card_info->device_info[device_id].logic_id;

        return DCMI_OK;
    }

    gplog(LOG_ERR, "Can not find card (%d) device (%d). Please check the input parameter.", card_id, device_id);
    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int check_get_device_type_para_valid(int card_id, int device_id, enum dcmi_unit_type *device_type)
{
    if ((card_id < 0) || (device_id < 0) || (device_type == NULL)) {
        gplog(LOG_ERR, "Card_id or device_id or device_type is invalid. (card_id=%d; device_id=%d; device_type[%s])",
              card_id, device_id, (device_type == NULL ? "NULL" : "NOT NULL"));
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    return DCMI_OK;
}

int dcmi_get_device_type(int card_id, int device_id, enum dcmi_unit_type *device_type)
{
    struct dcmi_card_info *card_info = NULL;
    int num_id;

    if (check_get_device_type_para_valid(card_id, device_id, device_type) != DCMI_OK) {
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    if ((dcmi_get_run_env_init_flag() != TRUE) && (g_board_details.device_count == 0)) {
        *device_type = INVALID_TYPE;
        return DCMI_OK;
    }

    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];

        if (card_id != card_info->card_id) {
            continue;
        }

        if (device_id < card_info->device_count) {
            *device_type = NPU_TYPE;
            if (dcmi_board_type_is_station() && (g_board_details.is_has_npu == FALSE)) {
                return DCMI_ERR_CODE_NOT_SUPPORT;
            }
            return DCMI_OK;
        } else if ((device_id == card_info->mcu_id) && (card_info->mcu_id != -1)) {
            *device_type = MCU_TYPE;
            return DCMI_OK;
        } else if ((device_id == card_info->cpu_id) && (card_info->cpu_id != -1)) {
            *device_type = CPU_TYPE;
            return DCMI_OK;
        } else {
            gplog(LOG_ERR, "card_id(%d) device_id(%d) >= device_count(%d).", card_id, device_id,
                  card_info->device_count);
            return DCMI_ERR_CODE_INVALID_PARAMETER;
        }
    }

    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int dcmi_get_card_list(int *card_num, int *card_list, int list_len)
{
    int num_id;
    struct dcmi_card_info *card_info = NULL;
    int *card_id_curr = NULL;

    if (card_num == NULL || card_list == NULL) {
        gplog(LOG_ERR, "input para card_num or card_list is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }
    if (list_len <= 0) {
        gplog(LOG_ERR, "input para list_len is invalid, list_len=%d.", list_len);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    if (dcmi_get_run_env_init_flag() != TRUE) {
        gplog(LOG_ERR, "dcmi is not init.");
        return DCMI_ERR_CODE_INNER_ERR;
    }

    if (g_board_details.card_count > list_len) {
        gplog(LOG_ERR, "card_count is bigger than list_len.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    card_id_curr = card_list;
    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];
        if (num_id < list_len) {
            card_id_curr[num_id] = card_info->card_id;
        }
    }
    *card_num = g_board_details.card_count;

    return DCMI_OK;
}

int dcmi_get_device_num_in_card(int card_id, int *device_num)
{
    int num_id;
    struct dcmi_card_info *card_info = NULL;

    if (device_num == NULL) {
        gplog(LOG_ERR, "device_num is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    if ((dcmi_get_run_env_init_flag() != TRUE) && (g_board_details.device_count == 0)) {
        *device_num = 0;
        return DCMI_OK;
    }

    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];
        if (card_info->card_id == card_id) {
            *device_num = card_info->device_count;
            return DCMI_OK;
        }
    }
    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int dcmi_get_device_id_in_card(int card_id, int *device_id_max, int *mcu_id, int *cpu_id)
{
    int num_id;
    struct dcmi_card_info *card_info = NULL;

    if ((device_id_max == NULL) || (mcu_id == NULL) || (cpu_id == NULL)) {
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    *mcu_id = DCMI_INVALID_VALUE;
    *cpu_id = DCMI_INVALID_VALUE;

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    if ((dcmi_get_run_env_init_flag() != TRUE) && (g_board_details.device_count == 0)) {
        *device_id_max = 0;
        return DCMI_OK;
    }

    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];
        if (card_info->card_id == card_id) {
            *device_id_max = card_info->device_count;
            if (card_info->mcu_id != DCMI_INVALID_VALUE) {
                *mcu_id = card_info->mcu_id;
            }
            if (card_info->cpu_id != DCMI_INVALID_VALUE) {
                *cpu_id = card_info->cpu_id;
            }
            return DCMI_OK;
        }
    }

    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int dcmi_get_card_pcie_info(int card_id, char *pcie_info, int pcie_info_len)
{
    int num_id, ret;
    struct dcmi_card_info *card_info = NULL;

    if (dcmi_get_run_env_init_flag() != TRUE) {
        gplog(LOG_ERR, "not init.");
        return DCMI_ERR_CODE_NOT_REDAY;
    }

    if (pcie_info == NULL || pcie_info_len <= 0) {
        gplog(LOG_ERR, "pcie_info is invalid or pcie_info_len is %d.", pcie_info_len);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    ret = dcmi_check_card_id(card_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "check card id %d failed %d.", card_id, ret);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!(dcmi_board_type_is_card()) || dcmi_board_chip_type_is_ascend_910b() ||
        dcmi_board_chip_type_is_ascend_910_93() || dcmi_board_chip_type_is_ascend_950()) {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];
        if (card_info->card_id == card_id) {
            if (strlen(card_info->pcie_info_pre) + 1 > (size_t)pcie_info_len) {
                return DCMI_ERR_CODE_INNER_ERR;
            }

            ret = memcpy_s(pcie_info, pcie_info_len, &card_info->pcie_info_pre[0],
                           strlen(card_info->pcie_info_pre) + 1);
            if (ret != EOK) {
                gplog(LOG_ERR, "call memcpy_s failed.%d.", ret);
                return DCMI_ERR_CODE_SECURE_FUN_FAIL;
            }

            return DCMI_OK;
        }
    }

    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int dcmi_get_card_pcie_slot(int card_id, int *pcie_slot)
{
    int num_id;
    struct dcmi_card_info *card_info = NULL;

    if (pcie_slot == NULL) {
        gplog(LOG_ERR, "input pcie slot is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_310() || dcmi_board_chip_type_is_ascend_310p()) {
        if (!dcmi_is_in_phy_machine_root()) {
            *pcie_slot = -1;
            gplog(LOG_ERR, "Operation not permitted, only root user int physical machine can call this api.");
            return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
        }
    }

    if (card_id < 0) {
        gplog(LOG_ERR, "card_id %d is invalid.", card_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (!(dcmi_board_type_is_card() || dcmi_board_type_is_server())) {
        *pcie_slot = -1;
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];
        if (card_info->card_id == card_id) {
            *pcie_slot = card_info->slot_id;
            return DCMI_OK;
        }
    }

    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int dcmi_get_fault_device_num_in_card(int card_id, int *device_num)
{
    struct dcmi_card_info *card_info = NULL;
    int num_id;

    if (device_num == NULL) {
        gplog(LOG_ERR, "input device_num is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_310() && dcmi_check_run_in_docker()) {
        *device_num = -1;
        gplog(LOG_ERR, "Operation not permitted, this api cannot be called in docker.");
        return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
    }

    if (card_id < 0) {
        gplog(LOG_ERR, "card_id %d is invalid.", card_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
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

int dcmi_get_device_chip_slot(int card_id, int device_id, int *chip_pos_id)
{
    int err;
    struct dcmi_card_info *card_info = NULL;
    enum dcmi_unit_type device_type = INVALID_TYPE;

    if (chip_pos_id == NULL) {
        gplog(LOG_ERR, "input chip_pos_id id NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    err = dcmi_get_card_info(card_id, &card_info);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_card_info failed. err is %d.", err);
        return err;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_card_info failed. err is %d.", err);
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

int dcmiv2_inner_get_card_id_device_id_from_logicid(int *card_id, int *device_id, int device_logic_id)
{
    int card_index;
    int chip_index;

    if (card_id == NULL || device_id == NULL || device_logic_id < 0 || device_logic_id >= MAX_CARD_NUM) {
        gplog(LOG_ERR, "Invalid input: card_id[%d] or device_id[%d] is NULL, or device_logic_id[%d] is invalid.",
              card_id == NULL, device_id == NULL, device_logic_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    for (card_index = 0; card_index < g_board_details.card_count; card_index++) {
        for (chip_index = 0; chip_index < g_board_details.card_info[card_index].device_count; chip_index++) {
            if (device_logic_id == g_board_details.card_info[card_index].device_info[chip_index].logic_id) {
                *card_id = g_board_details.card_info[card_index].card_id;
                *device_id = chip_index;
                return DCMI_OK;
            }
        }
    }

    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int dcmi_get_card_id_device_id_from_logicid(int *card_id, int *device_id, unsigned int device_logic_id)
{
    int card_index;
    int chip_index;

    if (card_id == NULL || device_id == NULL) {
        gplog(LOG_ERR, "card_id or device_id is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    for (card_index = 0; card_index < g_board_details.card_count; card_index++) {
        for (chip_index = 0; chip_index < g_board_details.card_info[card_index].device_count; chip_index++) {
            if ((int)device_logic_id == g_board_details.card_info[card_index].device_info[chip_index].logic_id) {
                *card_id = g_board_details.card_info[card_index].card_id;
                *device_id = chip_index;
                return DCMI_OK;
            }
        }
    }

    return DCMI_ERR_CODE_DEVICE_NOT_EXIST;
}

int dcmi_get_card_id_device_id_from_phyid(int *card_id, int *device_id, unsigned int device_phy_id)
{
    int card_index;
    int chip_index;

    if (card_id == NULL || device_id == NULL) {
        gplog(LOG_ERR, "card_id or device_id is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    for (card_index = 0; card_index < g_board_details.card_count; card_index++) {
        for (chip_index = 0; chip_index < g_board_details.card_info[card_index].device_count; chip_index++) {
            if (device_phy_id == g_board_details.card_info[card_index].device_info[chip_index].phy_id) {
                *card_id = g_board_details.card_info[card_index].card_id;
                *device_id = chip_index;
                return DCMI_OK;
            }
        }
    }
    return DCMI_ERR_CODE_DEVICE_NOT_EXIST;
}

#if defined DCMI_VERSION_2
int dcmi_get_dcmi_version(char *dcmi_ver, unsigned int len)
{
    int err;

    if ((dcmi_ver == NULL) || (len < DCMI_VERSION_MIN_LEN)) {
        gplog(LOG_ERR, "dcmi_ver or len %u is invalid.", len);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = sprintf_s(dcmi_ver, len, "%s", DCMI_VERSION);
    if (err < EOK) {
        gplog(LOG_ERR, "sprintf_s failed. err is %d.", err);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }

    return DCMI_OK;
}

int dcmi_get_os_version(char *os_ver, unsigned int len)
{
    int ret;
    char *str_tmp = NULL;
    char str_version[MAX_LENTH] = {0};
    FILE *pfd = NULL;

    if ((os_ver == NULL) || (len == 0)) {
        gplog(LOG_ERR, "os_ver or len %u is invalid.", len);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_get_product_type_inner() != DCMI_A500_A2) {
        gplog(LOG_ERR, "This product(%d) does not support queries os_version.", dcmi_get_product_type_inner());
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    pfd = fopen(OS_VERSION_FILE, "r");
    if (!pfd) {
        gplog(LOG_ERR, "fopen OS_VERSION_FILE failed.");
        return DCMI_ERR_CODE_FILE_OPERATE_FAIL;
    }

    while ((str_tmp = fgets(str_version, sizeof(str_version), pfd)) != NULL) {
        str_tmp = strstr(str_version, "PRETTY_NAME=");
        if (str_tmp == NULL) {
            (void)memset_s(str_version, sizeof(str_version), 0, sizeof(str_version));
            continue;
        }
        break;
    }

    if (str_tmp == NULL) {
        ret = strncpy_s(os_ver, len, "NA", sizeof("NA"));
        if (ret != EOK) {
            (void)fclose(pfd);
            gplog(LOG_ERR, "strncpy_s failed. err is %d", ret);
            return DCMI_ERR_CODE_SECURE_FUN_FAIL;
        }
    } else {
        unsigned int copy_count = strlen(str_version) - strlen("PRETTY_NAME=\"\"\n");
        ret = strncpy_s(os_ver, len, str_version + strlen("PRETTY_NAME=\""), copy_count);
        if (ret != EOK) {
            (void)fclose(pfd);
            gplog(LOG_ERR, "strncpy_s failed. err is %d", ret);
            return DCMI_ERR_CODE_SECURE_FUN_FAIL;
        }
    }

    (void)fclose(pfd);
    return DCMI_OK;
}

int dcmi_get_driver_version(char *driver_ver, unsigned int len)
{
    int err;
    int device_id_list[MAX_DEVICE_NUM] = {0};
    unsigned int length = 0;
    int device_index, device_count = 0;
    unsigned int mode = 0;

    if ((driver_ver == NULL) || (len == 0)) {
        gplog(LOG_ERR, "driver_ver or len %d is invalid.", len);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    /* Atlas 500 存在不带Asccend 310发货场景 */
    if (access("/run/minid_not_present", F_OK) == 0) {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dsmi_get_device_count(&device_count);
    if (err != DSMI_OK) {
        gplog(LOG_ERR, "dsmi_get_device_count failed. err is %d.", err);
        return dcmi_convert_error_code(err);
    }

    if (device_count > MAX_DEVICE_NUM) {
        gplog(LOG_ERR, "dsmi_get_device_count count %d.", device_count);
        return DCMI_ERR_CODE_INNER_ERR;
    }

    err = dsmi_list_device(&device_id_list[0], device_count);
    if (err != DSMI_OK) {
        gplog(LOG_ERR, "dsmi_list_device failed. err is %d.", err);
        return dcmi_convert_error_code(err);
    }

    err = dcmi_get_rc_ep_mode(&mode);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_rc_ep_mode failed. ret is %d", err);
        return dcmi_convert_error_code(err);
    }

    for (device_index = 0; device_index < device_count; device_index++) {
        if (dcmi_get_boot_status(mode, device_id_list[device_index]) != DCMI_OK) {
            continue;
        }

        err = dsmi_get_version(device_id_list[device_index], driver_ver, len, &length);
        if (err != DSMI_OK) {
            gplog(LOG_ERR, "dsmi_get_version %d failed. err is %d.", device_id_list[device_index], err);
            continue;
        }
        driver_ver[(length < len) ? length : (len - 1)] = '\0';
        break;
    }

    return dcmi_convert_error_code(err);
}

int dcmi_get_device_chip_info_v2(int card_id, int device_id, struct dcmi_chip_info_v2 *chip_info)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (chip_info == NULL) {
        gplog(LOG_ERR, "chip_info is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_chip_info(card_id, device_id, chip_info);
    } else if (device_type == MCU_TYPE) {
        return dcmi_mcu_get_chip_info(card_id, chip_info);
#ifndef _WIN32
    } else if (device_type == CPU_TYPE) {
        return dcmi_cpu_get_chip_info(card_id, chip_info);
#endif
    } else {
        gplog(LOG_ERR, "device_type %d is error.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
    return DCMI_OK;
}

// dcmi_get_chip_info仅用于500小站的内部客户，不对外开放
int dcmi_get_chip_info(int card_id, int device_id, struct dsmi_chip_info_stru *chip_info)
{
    int ret;
    int device_logic_id = 0;

    if (chip_info == NULL) {
        gplog(LOG_ERR, "pcie_info is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    ret = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", ret);
        return ret;
    }

    ret = dsmi_get_chip_info(device_logic_id, chip_info);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "call dsmi_get_chip_info failed. err is %d.", ret);
        return dcmi_convert_error_code(ret);
    }
    return dcmi_convert_error_code(ret);
}

int dcmi_get_device_pcie_info(int card_id, int device_id, struct dcmi_pcie_info *pcie_info)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (pcie_info == NULL) {
        gplog(LOG_ERR, "pcie_info is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (dcmi_mainboard_is_a900_a5_ub(g_mainboard_info.mainboard_id)) {
        gplog(LOG_ERR, "This device does not support.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_pcie_info(card_id, device_id, pcie_info);
    } else {
        gplog(LOG_ERR, "device_type %d is error.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_pcie_info_v2(int card_id, int device_id, struct dcmi_pcie_info_all *pcie_info)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (pcie_info == NULL) {
        gplog(LOG_ERR, "pcie_info is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (dcmi_mainboard_is_a900_a5_ub(g_mainboard_info.mainboard_id)) {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_pcie_info_v2(card_id, device_id, pcie_info);
    } else {
        gplog(LOG_ERR, "device_type %d is error.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_board_info(int card_id, int device_id, struct dcmi_board_info *board_info)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (board_info == NULL) {
        gplog(LOG_ERR, "board_info is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_board_info(card_id, device_id, board_info);
    } else if (device_type == MCU_TYPE) {
        err = dcmi_mcu_get_board_info(card_id, board_info);
        if (err == DCMI_OK) {
            board_info->slot_id = (unsigned int)device_id;
        }
        return err;
#ifndef _WIN32
    } else if (device_type == CPU_TYPE) {
        err = dcmi_cpu_get_board_info(card_id, board_info);
        if (err == DCMI_OK) {
            board_info->slot_id = (unsigned int)device_id;
        }
        return err;
#endif
    } else {
        gplog(LOG_ERR, "device_type %d is error.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_elabel_info(int card_id, int device_id, struct dcmi_elabel_info *elabel_info)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (elabel_info == NULL) {
        gplog(LOG_ERR, "elabel_info is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        if (dcmi_board_chip_type_is_ascend_310b()) {
            dcmi_set_i2c_dev_name(I2C9_DEV_NAME); // 模组elabel的iic设备名称
            return dcmi_i2c_get_npu_device_elable_info(card_id, elabel_info);
        } else if (dcmi_board_chip_type_is_ascend_950()) {
            return dcmi_ao_get_npu_device_elabel_info(card_id, device_id, elabel_info);
        } else {
            return dcmi_get_npu_device_elable_info(card_id, device_id, elabel_info);
        }
    } else if (device_type == MCU_TYPE) {
        return dcmi_mcu_get_device_elabel_info(card_id, elabel_info);
#ifndef _WIN32
    } else if (device_type == CPU_TYPE) {
        return dcmi_cpu_get_device_elabel_info(card_id, elabel_info);
#endif
    } else {
        gplog(LOG_ERR, "device_type %d is error.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_die_v2(int card_id, int device_id, enum dcmi_die_type input_type, struct dcmi_die_id *die_id)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (die_id == NULL) {
        gplog(LOG_ERR, "die_id is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    if (input_type >= INVALID_DIE) {
        gplog(LOG_ERR, "input_type is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_die(card_id, device_id, input_type, die_id);
#ifndef _WIN32
    } else if (device_type == CPU_TYPE) {
        return dcmi_cpu_get_device_die(card_id, device_id, input_type, die_id);
#endif
    } else {
        gplog(LOG_ERR, "device_type %d is error.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_board_id(int card_id, int device_id, unsigned int *board_id)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (board_id == NULL) {
        gplog(LOG_ERR, "board_id is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_board_id(card_id, device_id, board_id);
    } else if (device_type == MCU_TYPE) {
        return dcmi_mcu_get_board_id(card_id, board_id);
#ifndef _WIN32
    } else if (device_type == CPU_TYPE) {
        return dcmi_cpu_get_board_id(card_id, board_id);
#endif
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_component_count(int card_id, int device_id, unsigned int *component_count)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (component_count == NULL) {
        gplog(LOG_ERR, "component_count is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_component_count(card_id, device_id, component_count);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_component_list(int card_id, int device_id, enum dcmi_component_type *component_table,
                                   unsigned int component_count)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (component_table == NULL) {
        gplog(LOG_ERR, "component_table is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (component_count == 0) {
        gplog(LOG_ERR, "component_count %u is invalid.", component_count);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_component_list(card_id, device_id, component_table, component_count);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_component_static_version(int card_id, int device_id, enum dcmi_component_type component_type,
                                             unsigned char *version_str, unsigned int len)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (version_str == NULL) {
        gplog(LOG_ERR, "version_str is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (len < COMPONENT_VERSION_MIN_LEN) {
        gplog(LOG_ERR, "len %u is invalid.", len);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_component_static_version(card_id, device_id, component_type, version_str, len);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

static int dcmi_set_device_info_para_check(enum dcmi_main_cmd main_cmd, unsigned int sub_cmd, const void *buf)
{
    int err;

    err = dcmi_set_device_info_permission_check(main_cmd, sub_cmd);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "check permission failed. main_cmd=%u, sub_cmd=%u, err=%d", main_cmd, sub_cmd, err);
        return err;
    }

    if (buf == NULL || main_cmd >= DCMI_MAIN_CMD_MAX) {
        gplog(LOG_ERR, "buf is NULL or main_cmd is invalid. main_cmd=%u", main_cmd);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    return DCMI_OK;
}

int dcmi_set_device_info(int card_id, int device_id, enum dcmi_main_cmd main_cmd, unsigned int sub_cmd, const void *buf,
                         unsigned int buf_size)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    err = dcmi_set_device_info_para_check(main_cmd, sub_cmd, buf);
    if (err != DCMI_OK) {
        return err;
    }
    if (main_cmd == DCMI_MAIN_CMD_SEC && sub_cmd == DCMI_SEC_SUB_CMD_PSS) {
        if (check_pkcs_support_product_type()) {
            // pkcs使能整机生效，与device_id, card_id无关，参数不会使用，此处强制写0
            return dcmi_set_npu_device_info(0, 0, main_cmd, sub_cmd, buf, buf_size);
        }
        gplog(LOG_OP, "This device does not support setting pkcs-enable status.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_cmd_product_support_check(main_cmd, sub_cmd);
    if (err != DCMI_OK) {
        return err;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (dcmi_board_chip_type_is_ascend_910_93() == TRUE && main_cmd == DCMI_MAIN_CMD_DEVICE_SHARE &&
        sub_cmd == DCMI_DEVICE_SHARE_SUB_CMD_COMMON) {
        return dcmi_set_device_share_for_910_93(card_id, device_type, DCMI_MAIN_CMD_DEVICE_SHARE,
                                                DCMI_DEVICE_SHARE_SUB_CMD_COMMON, buf, buf_size);
    }

    if (device_type == NPU_TYPE) {
        err = dcmi_set_npu_device_info(card_id, device_id, main_cmd, sub_cmd, buf, buf_size);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "set device info failed. card_id=%d, device_id=%d, main_cmd=%u, sub_cmd=%u, err=%d", card_id,
                  device_id, main_cmd, sub_cmd, err);
            return err;
        }
        gplog(LOG_OP, "set device info success. card_id=%d, device_id=%d, main_cmd=%u, sub_cmd=%u", card_id, device_id,
              main_cmd, sub_cmd);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    return DCMI_OK;
}

int dcmi_get_device_logic_id_and_type(int card_id, int device_id, enum dcmi_unit_type *device_type,
                                      int *device_logic_id)
{
    int err;
    if (dcmi_board_chip_type_is_ascend_950()) {
        err = dcmiv2_get_device_logic_id(device_logic_id, card_id, device_id);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "call dcmiv2_get_device_logic_id failed. err is %d.", err);
            return err;
        }
        err = dcmiv2_get_device_type(*device_logic_id, device_type);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "call dcmiv2_get_device_type failed. err is %d.", err);
            return err;
        }
        return DCMI_OK;
    } else {
        err = dcmi_get_device_type(card_id, device_id, device_type);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_type failed. err is %d.", err);
            return err;
        }
        err = dcmi_get_device_logic_id(device_logic_id, card_id, device_id);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", err);
            return err;
        }
        return DCMI_OK;
    }
}

static int dcmiv2_get_card_id_device_id_and_type(int device_logic_id, int *card_id, int *device_id,
                                                 enum dcmi_unit_type *device_type)
{
    int err;
    if (dcmi_board_chip_type_is_ascend_950()) {
        err = dcmiv2_inner_get_card_id_device_id_from_logicid(card_id, device_id, device_logic_id);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "dcmiv2_inner_get_card_id_device_id_from_logicid failed. (err=%d)", err);
            return err;
        }
        err = dcmiv2_get_device_type(device_logic_id, device_type);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "call dcmiv2_get_device_type failed. err is %d.", err);
            return err;
        }
        return DCMI_OK;
    }

    return DCMI_ERR_CODE_NOT_SUPPORT;
}

int dcmi_set_custom_op_secverify_mode(int card_id, int device_id, const void *buf, unsigned int buf_size)
{
    int err;
    int device_logic_id = card_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    // 仅支持物理机root + 虚机的root + 特权容器root
    if (!(dcmi_is_in_phy_machine_root() || dcmi_is_in_vm_root() || dcmi_is_in_privileged_docker_root())) {
        return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
    }

    if (buf == NULL) {
        gplog(LOG_ERR, "dcmi_set_custom_op_secverify_mode buf is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!(dcmi_board_chip_type_is_ascend_910b() || dcmi_board_chip_type_is_ascend_910_93() ||
          dcmi_board_chip_type_is_ascend_950())) {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        err = dcmiv2_get_card_id_device_id_and_type(device_logic_id, &card_id, &device_id, &device_type);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "call dcmiv2_get_card_id_device_id_and_type failed. err is %d.", err);
            return err;
        }
    } else {
        err = dcmi_get_device_logic_id_and_type(card_id, device_id, &device_type, &device_logic_id);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_logic_id_and_type failed, err is %d.", err);
            return err;
        }
    }

    if (device_type == NPU_TYPE) {
        err = dcmi_set_npu_device_info(card_id, device_id, DCMI_MAIN_CMD_SEC, DCMI_SEC_SUB_CMD_CUST_SIGN_FLAG, buf,
                                       buf_size);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "set device info failed. card_id=%d, device_id=%d, err=%d", card_id, device_id, err);
            return err;
        }
        gplog(LOG_OP, "set device info success. card_id=%d, device_id=%d", card_id, device_id);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    return DCMI_OK;
}

int dcmi_set_custom_op_secverify_enable(int card_id, int device_id, const char *config_name, unsigned char *buf,
                                        unsigned int buf_size)
{
    int err;
    int device_logic_id = card_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    // 仅支持物理机root
    if (!dcmi_is_in_phy_machine_root()) {
        gplog(LOG_OP, "Operation not permitted, only root user on physical machine can call this api.");
        return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
    }

    if (buf == NULL) {
        gplog(LOG_ERR, "buf is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!(dcmi_board_chip_type_is_ascend_910b() || dcmi_board_chip_type_is_ascend_910_93() ||
          dcmi_board_chip_type_is_ascend_950())) {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        err = dcmiv2_get_device_type(device_logic_id, &device_type);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "call dcmiv2_get_device_type failed. err is %d.", err);
            return err;
        }
    } else {
        err = dcmi_get_device_logic_id_and_type(card_id, device_id, &device_type, &device_logic_id);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "call dcmi_get_device_logic_id_and_type failed, err is %d.", err);
            return err;
        }
    }

    if (device_type == NPU_TYPE) {
        err = dsmi_set_user_config(device_logic_id, config_name, buf_size, buf);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "set device info failed. (card_id=%d, device_id=%d, err=%d)", card_id, device_id, err);
            return err;
        }
        gplog(LOG_OP, "set device info success. (card_id=%d, device_id=%d)", card_id, device_id);
    } else {
        gplog(LOG_INFO, "device_type %d is not support.", device_type);
    }

    return DCMI_OK;
}

int dcmi_get_device_info(int card_id, int device_id, enum dcmi_main_cmd main_cmd, unsigned int sub_cmd, void *buf,
                         unsigned int *size)
{
    int err;
    int cmd_permission = 0;
    enum dcmi_unit_type device_type = NPU_TYPE;
    err = dcmi_get_device_info_cmd_permission(main_cmd, sub_cmd, &cmd_permission);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "input main_cmd is invalid. main_cmd=%d", main_cmd);
        return err;
    }
    err = dcmi_device_info_is_support(cmd_permission);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "Device info isn't supported to query.");
        return err;
    }

    if (buf == NULL || size == NULL || main_cmd >= DCMI_MAIN_CMD_MAX) {
        gplog(LOG_ERR, "input para buf is NULL or para size is NULL or para main_cmd is invalid.main_cmd=%d.",
              main_cmd);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    if (main_cmd == DCMI_MAIN_CMD_SEC && sub_cmd == DCMI_SEC_SUB_CMD_PSS) {
        if (check_pkcs_support_product_type()) {
            // pkcs使能整机生效，与device_id, card_id无关，参数不会使用，此处强制写0
            return dcmi_get_npu_device_info(0, 0, main_cmd, sub_cmd, buf, size);
        }
        gplog(LOG_OP, "This device does not support getting pkcs-enable status.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_cmd_product_support_check(main_cmd, sub_cmd);
    if (err != DCMI_OK) {
        return err;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_info(card_id, device_id, main_cmd, sub_cmd, buf, size);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    return DCMI_OK;
}

int dcmi_get_device_mac_count(int card_id, int device_id, int *count)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (!dcmi_is_in_phy_machine_root() && dcmi_board_chip_type_is_ascend_910b_300i_a2() == TRUE) {
        gplog(LOG_OP, "Operation not permitted, only root user on physical machine can call this api.");
        return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
    }

    if (count == NULL) {
        gplog(LOG_ERR, "count is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (dcmi_board_chip_type_is_ascend_910b_300i_a2() || dcmi_board_chip_type_is_ascend_950_card()) {
        gplog(LOG_OP, "This device does not support get device mac count.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_mac_count(card_id, device_id, count);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    return DCMI_OK;
}

int dcmi_get_card_elabel_v2(int card_id, struct dcmi_elabel_info *elabel_info)
{
    int err;
    int elabel_id = -1;

    if (card_id < 0) {
        gplog(LOG_ERR, "card_id %d is invalid.", card_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (elabel_info == NULL) {
        gplog(LOG_ERR, "elabel_info is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_get_elabel_pos_in_card(card_id, &elabel_id);
    if ((err != DCMI_OK) || (elabel_id == -1)) {
        gplog(LOG_ERR, "dcmi_get_elabel_pos_in_card failed.card_id = %d, err = %d, elabel_id=%d", card_id, err,
              elabel_id);
        dcmi_set_default_elabel_str(elabel_info->serial_number, sizeof(elabel_info->serial_number));
        dcmi_set_default_elabel_str(elabel_info->manufacturer, sizeof(elabel_info->manufacturer));
        dcmi_set_default_elabel_str(elabel_info->product_name, sizeof(elabel_info->product_name));
        dcmi_set_default_elabel_str(elabel_info->model, sizeof(elabel_info->model));
    } else {
#ifndef NPU_SMI_V2
        err = dcmi_get_device_elabel_info(card_id, elabel_id, elabel_info);
#else
        err = dcmiv2_get_device_elabel_info(elabel_id, elabel_info);
#endif
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "dcmi_get_device_elabel_info failed.card_id = %d, err= %d.", card_id, err);
        }
    }
    return err;
}

int dcmi_get_card_board_info(int card_id, struct dcmi_board_info *board_info)
{
    int err;
    int board_id_pos;

    bool check_result = ((dcmi_check_run_in_vm() || dcmi_check_run_in_docker()) &&
                         (dcmi_board_chip_type_is_ascend_310() &&
                          (dcmi_board_type_is_station() || dcmi_board_type_is_card())));
    if (check_result) {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
    if (card_id < 0) {
        gplog(LOG_ERR, "intput para card_id is invalid, card_id=%d.", card_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }
    if (board_info == NULL) {
        gplog(LOG_ERR, "intput para board_info is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    err = dcmi_get_board_id_pos_in_card(card_id, &board_id_pos);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_board_id_pos_in_card failed. err is %d", err);
        return err;
    }
    err = dcmi_get_device_board_info(card_id, board_id_pos, board_info);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_board_info failed. err is %d", err);
        return err;
    }

    if (dcmi_board_type_is_soc() && !dcmi_board_chip_type_is_ascend_950()) {
        struct dcmi_board_info soc_board_info = {0};
        err = dcmi_get_board_info_for_develop(&soc_board_info);
        if (err == DCMI_OK) {
            board_info->board_id = soc_board_info.board_id;
            board_info->pcb_id = soc_board_info.pcb_id;
        } else if (dcmi_board_chip_type_is_ascend_310b()) {
            board_info->board_id = 0;
        }
    }

    board_info->slot_id = 0;
    return DCMI_OK;
}

int dcmi_get_device_ssh_enable(int card_id, int device_id, int *enable_flag)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;
    bool supported_type = false;

    if (enable_flag == NULL) {
        gplog(LOG_ERR, "enable_flag is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    supported_type = dcmi_board_chip_type_is_ascend_310() ||
                     (dcmi_board_chip_type_is_ascend_310p() && dcmi_board_type_is_card());
    if (supported_type) {
        if (device_type == NPU_TYPE) {
            return dcmi_get_npu_device_ssh_enable(card_id, device_id, enable_flag);
        } else {
            gplog(LOG_INFO, "device_type is not support.%d.", device_type);
            return DCMI_ERR_CODE_NOT_SUPPORT;
        }
    } else {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_dvpp_ratio_info(int card_id, int device_id, struct dcmi_dvpp_ratio *usage)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;
    if (usage == NULL) {
        gplog(LOG_ERR, "usage is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (dcmi_board_chip_type_is_ascend_310p()) {
        if (device_type == NPU_TYPE) {
            return dcmi_get_npu_device_dvpp_ratio_info(card_id, device_id, usage);
        } else {
            gplog(LOG_INFO, "device_type is not support.%d.", device_type);
        }
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    return DCMI_ERR_CODE_NOT_SUPPORT;
}

int dcmi_get_product_type(int card_id, int device_id, char *product_type_str, int buf_size)
{
    int ret;
    enum dcmi_unit_type device_type = NPU_TYPE;
    if ((product_type_str == NULL) || (buf_size < DCMI_PRODUCT_TYPE_MIN_LEN)) {
        gplog(LOG_ERR, "dcmi_get_product_type product_type_str is null or buf size %d is invaild", buf_size);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    ret = dcmi_get_device_type(card_id, device_id, &device_type);
    if (ret != 0) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", ret);
        return ret;
    }

    if (device_type == NPU_TYPE) {
        if (dcmi_board_chip_type_is_ascend_910() || dcmi_board_chip_type_is_ascend_910b() ||
            dcmi_board_chip_type_is_ascend_910_93() || dcmi_board_chip_type_is_ascend_950()) {
            return DCMI_ERR_CODE_NOT_SUPPORT;
        }
        return dcmi_get_product_type_str(card_id, device_id, product_type_str, buf_size);
    } else {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_aicpu_count_info(int card_id, int device_id, unsigned char *count_info)
{
    int ret;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (count_info == NULL) {
        gplog(LOG_ERR, "count_info is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    ret = dcmi_get_device_type(card_id, device_id, &device_type);
    if (ret != 0) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", ret);
        return ret;
    }

    bool support_chip_type = (dcmi_board_chip_type_is_ascend_310());
    if (support_chip_type == true && device_type == NPU_TYPE) {
        return dcmi_get_npu_device_aicpu_count_info(card_id, device_id, count_info);
    }
    gplog(LOG_ERR, "This device does not support dcmi_get_device_aicpu_count_info.");
    return DCMI_ERR_CODE_NOT_SUPPORT;
}

int dcmi_get_board_id(int card_id, int device_id, int *board_id)
{
    int err;
    struct dcmi_board_info board_info = {0};

    if (board_id == NULL) {
        gplog(LOG_ERR, "board_id is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_get_device_board_info(card_id, device_id, &board_info);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_board_info failed. err is %d.", err);
        return err;
    }
    *board_id = board_info.board_id;
    return DCMI_OK;
}

int dcmi_get_first_power_on_date(int card_id, unsigned int *first_power_on_date)
{
    int err;

    if (!dcmi_is_in_phy_machine()) {
        gplog(LOG_ERR, "Operation not permitted, only physical machine can call this api.");
        return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
    }

    err = dcmi_check_card_id(card_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "check card id %d failed %d.", card_id, err);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (first_power_on_date == NULL) {
        gplog(LOG_ERR, "first_power_on_date is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_310p() &&
        !(dcmi_board_chip_type_is_ascend_910b() && dcmi_board_type_is_card())) {
        gplog(LOG_OP, "This device does not support get first power on date.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_mcu_get_first_power_on_date(card_id, first_power_on_date);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_mcu_get_first_power_on_date failed.card_id = %d, err = %d", card_id, err);
    }
    return err;
}

static int dcmi_get_compatibility_para_check(int card_id, int device_id, enum dcmi_device_compat *compatibility)
{
    int ret;

    ret = dcmi_check_permission_of_get_compatibility(card_id, device_id, compatibility);
    if (ret != DCMI_OK) {
        return ret;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    return DCMI_OK;
}

int dcmi_get_device_compatibility(int card_id, int device_id, enum dcmi_device_compat *compatibility)
{
    int ret;
    unsigned char firmware_version[MAX_VER_LEN] = {0};
    unsigned char driver_version[MAX_VER_LEN] = {0};
    unsigned char compat_list_drv[COMPAT_ITEM_SIZE_MAX] = {0}; // 驱动兼容性列表（从flash中获取）
    unsigned char compat_list_fw[COMPAT_ITEM_SIZE_MAX] = {0};  // 固件兼容性列表（从驱动version.info获取）

    ret = dcmi_get_compatibility_para_check(card_id, device_id, compatibility);
    if (ret != DCMI_OK) {
        return ret;
    }

    /* 1. 获取固件运行版本 dcmi_get_device_component_static_version */
    ret = dcmi_get_firmware_version(card_id, device_id, firmware_version, MAX_VER_LEN);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "Get firmware version failed. card_id=%d, device_id=%d, err=%d", card_id, device_id, ret);
        return ret;
    }

    // 不兼容6.4以前的固件版本。避免出现 用的新驱动+固件先装新的，再装老的，这样 flash中还是新的，导致显示OK
    if (strcmp((const char *)firmware_version, (const char *)COMPAT_VERSION_START_FW) < 0) {
        *compatibility = DCMI_COMPAT_NOK;
        return DCMI_OK;
    }

    /* 2. 使用固件运行版本对比驱动的version.info中的固件兼容性列表 */
    ret = dcmi_version_info_of_drv_by_field(COMPAT_FIELD_FW, compat_list_fw, COMPAT_ITEM_SIZE_MAX);
    if (ret == DCMI_OK) {
        ret = dcmi_judge_compatibility(firmware_version, MAX_VER_LEN, compat_list_fw, COMPAT_ITEM_SIZE_MAX,
                                       compatibility);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "1th: Judge compatibility failed. card_id=%d, device_id=%d, err=%d", card_id, device_id,
                  ret);
            return ret;
        } else if (*compatibility == DCMI_COMPAT_OK) {
            return DCMI_OK;
        }
    }

    /* 3. 获取驱动运行版本 dcmi_get_driver_version */
    ret = dcmi_get_driver_version((char *)driver_version, sizeof(driver_version));
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "Get driver version failed. card_id=%d, device_id=%d, err=%d", card_id, device_id, ret);
        return ret;
    }
    /* 4. 使用驱动运行版本对比固件的flash中的驱动兼容性列表 */
    ret = dcmi_get_user_config(card_id, device_id, COMPAT_LIST_NAME, COMPAT_ITEM_SIZE_MAX, compat_list_drv);
    if (ret != DCMI_OK) {
        /* 获取字段失败，输出UNKNOW */
        gplog(LOG_ERR, "Get flash compat list %s. card_id=%d, device_id=%d, err=%d",
              (ret == DCMI_ERR_CODE_NOT_SUPPORT) ? "not support" : "faild", card_id, device_id, ret);
        *compatibility = DCMI_COMPAT_UNKNOWN;
        return DCMI_OK;
    }

    ret = dcmi_judge_compatibility(driver_version, MAX_VER_LEN, compat_list_drv, COMPAT_ITEM_SIZE_MAX, compatibility);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "2th: Judge compatibility failed. card_id=%d, device_id=%d, err=%d", card_id, device_id, ret);
        return ret;
    }
    return DCMI_OK;
}
#endif

#if defined DCMI_VERSION_1
int dcmi_get_card_num_list(int *card_num, int *card_list, int list_len)
{
    return dcmi_get_card_list(card_num, card_list, list_len);
}

int dcmi_get_pcie_info(int card_id, int device_id, struct dcmi_tag_pcie_idinfo *pcie_idinfo)
{
    return dcmi_get_device_pcie_info(card_id, device_id, (struct dcmi_pcie_info *)pcie_idinfo);
}

int dcmi_get_board_info(int card_id, int device_id, struct dcmi_board_info_stru *board_info)
{
    return dcmi_get_device_board_info(card_id, device_id, (struct dcmi_board_info *)board_info);
}

int dcmi_copy_elabel_info(struct dcmi_elabel_info_stru *elabel_info, struct dcmi_elabel_info *new_elabel_info)
{
    int ret;

    ret = strncpy_s(elabel_info->product_name, sizeof(elabel_info->product_name), new_elabel_info->product_name,
                    strlen(new_elabel_info->product_name));
    if (ret != EOK) {
        gplog(LOG_ERR, "strncpy_s failed. err is %d", ret);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }
    ret = strncpy_s(elabel_info->model, sizeof(elabel_info->model), new_elabel_info->model,
                    strlen(new_elabel_info->model));
    if (ret != EOK) {
        gplog(LOG_ERR, "strncpy_s failed. err is %d", ret);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }
    ret = strncpy_s(elabel_info->manufacturer, sizeof(elabel_info->manufacturer), new_elabel_info->manufacturer,
                    strlen(new_elabel_info->manufacturer));
    if (ret != EOK) {
        gplog(LOG_ERR, "strncpy_s failed. err is %d", ret);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }
    ret = strncpy_s(elabel_info->serial_number, sizeof(elabel_info->serial_number), new_elabel_info->serial_number,
                    strlen(new_elabel_info->serial_number));
    if (ret != EOK) {
        gplog(LOG_ERR, "strncpy_s failed. err is %d", ret);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }
    return DCMI_OK;
}

int dcmi_get_card_elabel(int card_id, struct dcmi_elabel_info_stru *elabel_info)
{
    int ret;
    struct dcmi_elabel_info new_elabel_info = {{0}};

    if (elabel_info == NULL) {
        gplog(LOG_ERR, "elabel_info is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_910b() || dcmi_board_chip_type_is_ascend_910_93() ||
        dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_ERR, "This device does not support.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    ret = dcmi_get_card_elabel_v2(card_id, &new_elabel_info);
    if (ret != DCMI_OK) {
        return ret;
    }

    ret = dcmi_copy_elabel_info(elabel_info, &new_elabel_info);
    if (ret != DCMI_OK) {
        return ret;
    }

    return DCMI_OK;
}

int dcmi_get_device_chip_info(int card_id, int device_id, struct dcmi_chip_info *chip_info)
{
    int ret;
    struct dcmi_chip_info_v2 chip_info_v2 = {{0}};

    if (chip_info == NULL) {
        gplog(LOG_ERR, "chip_info is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    ret = dcmi_get_device_chip_info_v2(card_id, device_id, &chip_info_v2);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_chip_info_v2 failed. err is %d.", ret);
        return ret;
    }

    ret = memcpy_s(chip_info, sizeof(struct dcmi_chip_info), &chip_info_v2, sizeof(struct dcmi_chip_info));
    if (ret != EOK) {
        gplog(LOG_ERR, "memcpy_s chip_info_v2 to chip_info failed. err is %d.", ret);
        return DCMI_ERR_CODE_SECURE_FUN_FAIL;
    }

    return DCMI_OK;
}

int dcmi_get_device_die(int card_id, int device_id, struct dcmi_soc_die_stru *device_die)
{
    return dcmi_get_device_die_v2(card_id, device_id, VDIE, (struct dcmi_die_id *)device_die);
}

int dcmi_get_device_ndie(int card_id, int device_id, struct dsmi_soc_die_stru *device_ndie)
{
    return dcmi_get_device_die_v2(card_id, device_id, NDIE, (struct dcmi_die_id *)device_ndie);
}

int dcmi_get_version(int card_id, int device_id, char *version_str, unsigned int version_len, int *len)
{
    int err;
    int device_logic_id = 0;

    if (version_str == NULL) {
        gplog(LOG_ERR, "input para version_str is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }
    if (version_len < DRIVER_VERSION_MIN_LEN) {
        gplog(LOG_ERR, "input para version_len is invalid. version_len=%u.", version_len);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }
    if (len == NULL) {
        gplog(LOG_ERR, "output para len is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_logic_id failed. err is %d.", err);
        return err;
    }

    err = dsmi_get_version(device_logic_id, version_str, version_len, (unsigned int *)(void *)len);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dsmi_get_version failed. err is %d.", err);
    }

    return dcmi_convert_error_code(err);
}

int dcmi_get_computing_power_info(int card_id, int device_id, int type,
                                  struct dsmi_computing_power_info *computing_power)
{
#ifndef _WIN32
    int err;
    struct dcmi_chip_info chip_info = {{0}};
    const int aicore_cnt_type = 1;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (computing_power == NULL) {
        gplog(LOG_ERR, "computing_power is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (type != aicore_cnt_type) {
        gplog(LOG_ERR, "type %d is invalid.", type);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (dcmi_board_chip_type_is_ascend_310() || dcmi_board_chip_type_is_ascend_310b()) {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (device_type == NPU_TYPE) {
        err = dcmi_get_device_chip_info(card_id, device_id, &chip_info);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "dcmi_get_device_chip_info failed. err is %d.", err);
            return err;
        }
        computing_power->data1 = chip_info.aicore_cnt;
        return DCMI_OK;
    } else {
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#else
    return DCMI_ERR_CODE_NOT_SUPPORT;
#endif
}

int dcmi_get_all_device_count(int *all_device_count)
{
    int ret;

    if (!(dcmi_is_in_phy_machine() || (dcmi_is_in_virtual_machine() == TRUE))) {
        gplog(LOG_ERR, "Operation not permitted, only physical and virtual machine can call this api.");
        return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
    }

    if (all_device_count == NULL) {
        gplog(LOG_ERR, "The all_device_count is null.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (dcmi_board_type_is_soc() == TRUE) {
        gplog(LOG_ERR, "This device is not support.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    ret = dsmi_get_all_device_count(all_device_count);
    if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "Call dsmi_get_all_device_count failed. err is %d.", ret);
    }

    return dcmi_convert_error_code(ret);
}
#endif

int dcmi_get_netdev_brother_device(int card_id, int device_id, int *brother_card_id)
{
    unsigned int main_board_id = 0;
    if (brother_card_id == NULL) {
        gplog(LOG_ERR, "brother_card_id is null.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_910_93() ||
        !(dcmi_is_in_phy_machine() || dcmi_check_run_in_privileged_docker())) {
        gplog(LOG_ERR, "this device can't call this api on virtual machine or container or not in 910_93.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    main_board_id = dcmi_get_maindboard_id_inner();
    if ((int)main_board_id == 0) {
        gplog(LOG_ERR, "get main board id %u failed.", main_board_id);
        return DCMI_ERR_CODE_INNER_ERR;
    }

    // 天工无互助链路关系
    if ((main_board_id == Atlas_9000_A3_SuperPoD_MAIN_BOARD_ID1) ||
        (main_board_id == Atlas_9000_A3_SuperPoD_MAIN_BOARD_ID2)) {
        *brother_card_id = -1;
        goto success;
    }

    if (card_id < 0 || card_id >= MAX_RESET_CARD_NUM) {
        gplog(LOG_ERR, "card_id is error.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (device_id < 0 || device_id >= MAX_RESET_DEVICE_NUM) {
        gplog(LOG_ERR, "device_id is error.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    *brother_card_id = (card_id >= BRO_CARD_OFFSET) ? (card_id - BRO_CARD_OFFSET) : (card_id + BRO_CARD_OFFSET);
success:
    gplog(LOG_OP, "dcmi_get_netdev_brother_device success.");
    return DCMI_OK;
}

int dcmi_get_mainboard_id(int card_id, int device_id, unsigned int *mainboard_id)
{
    int ret;
    enum dcmi_unit_type device_type = INVALID_TYPE;

    if (mainboard_id == NULL) {
        gplog(LOG_ERR, "The mainboard_id is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (!dcmi_board_chip_type_is_ascend_910b() && !dcmi_board_chip_type_is_ascend_910_93()) {
        gplog(LOG_OP, "This device does not support get main board id.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    ret = dcmi_get_device_type(card_id, device_id, &device_type);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "Call dcmi_get_device_type failed, err is %d.", ret);
        return ret;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_mainboard_id(card_id, device_id, mainboard_id);
    } else {
        gplog(LOG_ERR, "The device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmi_get_device_phyid_from_logicid(unsigned int logic_id, unsigned int *phy_id)
{
    int err;

    if (phy_id == NULL) {
        gplog(LOG_ERR, "phy_id is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    err = dsmi_get_phyid_from_logicid(logic_id, phy_id);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "dsmi_get_phyid_from_logicid failed. err is %d.", err);
    }
    return dcmi_convert_error_code(err);
}

int dcmi_get_device_logicid_from_phyid(unsigned int phy_id, unsigned int *logic_id)
{
    int err;

    if (logic_id == NULL) {
        gplog(LOG_ERR, "logic_id is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

#ifndef ENABLE_EQUIPMENT
    if (dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
#endif

    err = dsmi_get_logicid_from_phyid(phy_id, logic_id);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "dsmi_get_logicid_from_phyid failed. err is %d.", err);
    }
    return dcmi_convert_error_code(err);
}

int dcmi_get_spod_node_status(int card_id, int device_id, unsigned int sdid, unsigned int *status)
{
    int ret;
    unsigned int out_size = sizeof(unsigned int);
    union {
        unsigned int sdid;
        unsigned int status;
    } para;

    if (status == NULL) {
        gplog(LOG_ERR, "Invalid param, status is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    para.sdid = sdid;

    ret = dcmi_get_device_info(card_id, device_id, DCMI_MAIN_CMD_CHIP_INF, DCMI_CHIP_INF_SUB_CMD_SPOD_NODE_STATUS,
                               (void *)&para, &out_size);
    if ((ret != DCMI_OK) && (ret != DCMI_ERR_CODE_NOT_SUPPORT)) {
        gplog(LOG_ERR, "get spod node status failed. err is %d.", ret);
    }

    *status = (ret == DCMI_OK ? para.status : *status);

    return ret;
}

int dcmi_set_custom_op_secverify_cert(int card_id, int device_id, char *cert_chain_data, unsigned int cert_chain_size)
{
    int err, device_logic_id = 0;
    enum dcmi_unit_type device_type;
    unsigned int show_info_size = 0;
    char *show_info = NULL, *cert_head = NULL;

    err = dcmi_get_device_type(card_id, device_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    err = dcmi_get_device_logic_id(&device_logic_id, card_id, device_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "call dcmi_get_device_logic_id failed. err is %d.", err);
        return err;
    }

    cert_head = cert_chain_data;
    if (cert_head == NULL) {
        gplog(LOG_ERR, "cert chain data is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }
    cert_head = strstr(cert_head, CERT_SPLIT_NAME);
    while (cert_head) {
        show_info_size += sizeof(struct cert_show_info);
        cert_head = strstr(cert_head + 1, CERT_SPLIT_NAME);
    }
    show_info = (char *)calloc(show_info_size, sizeof(char));
    if (show_info == NULL) {
        gplog(LOG_ERR, "calloc fail.");
        return DCMI_ERR_CODE_MEM_OPERATE_FAIL;
    }
    err = dsmi_get_custom_op_secverify_cert_show_info(device_logic_id, cert_chain_data, cert_chain_size, show_info,
                                                      show_info_size);
    free(show_info);
    if (err != DSMI_OK) {
        gplog(LOG_ERR, "custom op secverify cert verify fail. err is %d.", err);
        return dcmi_convert_error_code(err);
    }

    if (device_type == NPU_TYPE) {
        err = dcmi_set_custom_cert_info(device_logic_id, cert_chain_data, cert_chain_size);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "set device info failed. card_id=%d, device_id=%d, err=%d", card_id, device_id, err);
            return err;
        }
        gplog(LOG_OP, "set device info success. card_id=%d, device_id=%d", card_id, device_id);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
    return DCMI_OK;
}