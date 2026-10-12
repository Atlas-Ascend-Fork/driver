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

#if defined DCMI_VERSION_2
static int dcmiv2_check_type_para_vaild(int dev_id, int *card_id, int *device_id, enum dcmi_unit_type *device_type)
{
    int err, tmp_card_id, tmp_device_id;

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&tmp_card_id, &tmp_device_id, dev_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_inner_get_card_id_device_id_from_logicid failed. (ret=%d)", err);
        return err;
    }

    if (check_get_device_type_para_valid(tmp_card_id, tmp_device_id, device_type) != DCMI_OK) {
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    *card_id = tmp_card_id;
    *device_id = tmp_device_id;
    return DCMI_OK;
}

int dcmiv2_check_device_heartbeat_state(int device_id)
{
    int ret;
    unsigned int health_err_first[DCMI_ERROR_CODE_MAX_NUM] = {0};
    int health_err_cnt = 0;

    if (device_id < 0 || device_id >= MAX_CARD_NUM) {
        gplog(LOG_ERR, "Invalid input: device_id[%d] is invalid.", device_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    ret = dsmi_get_device_errorcode(device_id, &health_err_cnt, health_err_first);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "call dsmi_get_device_errorcode failed, (ret=%d)", ret);
        return ret;
    }

    for (int i = 0; i < health_err_cnt && i < DCMI_ERROR_CODE_MAX_NUM; ++i) {
        if (health_err_first[i] == DEVICE_HEARTBEAT_LOSS) {
            gplog(LOG_ERR, "device heartbeat loss, (device_id=%d)", device_id);
            return DCMI_ERR_CODE_INNER_ERR;
        }
    }
    return DCMI_OK;
}

int dcmiv2_get_device_type(int dev_id, enum dcmi_unit_type *device_type)
{
    struct dcmi_card_info *card_info = NULL;
    int num_id, err, card_id, device_id;

    err = dcmiv2_check_type_para_vaild(dev_id, &card_id, &device_id, device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_check_type_para_vaild failed. (ret=%d)", err);
        return err;
    }

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

int dcmiv2_get_device_elabel_info(int dev_id, struct dcmi_elabel_info *elabel_info)
{
    int err, card_id, device_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (elabel_info == NULL) {
        gplog(LOG_ERR, "elabel_info is invalid.");
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

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_get_device_type failed. err is %d.", err);
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

int dcmiv2_get_device_board_info(int dev_id, struct dcmi_board_info *board_info)
{
    int err, card_id, device_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (board_info == NULL) {
        gplog(LOG_ERR, "board_info is invalid.");
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

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_get_device_type failed. err is %d.", err);
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

int dcmiv2_get_device_pcie_info(int dev_id, struct dcmi_pcie_info_all *pcie_info)
{
    int err, card_id, device_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (pcie_info == NULL) {
        gplog(LOG_ERR, "pcie_info is invalid.");
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

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_get_device_type failed. err is %d.", err);
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

int dcmiv2_get_device_ub_id_info(int dev_id, struct dcmi_ub_id_info *ub_id_info)
{
    int ret;
    enum dcmi_unit_type device_type = NPU_TYPE;
    unsigned int out_size = sizeof(struct dcmi_ub_id_info);

    if (ub_id_info == NULL) {
        gplog(LOG_ERR, "Input param ub_id_info is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_mainboard_is_a900_a5_ub(g_mainboard_info.mainboard_id) == 0) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    ret = dcmiv2_get_device_type(dev_id, &device_type);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_get_device_type failed. err is %d.", ret);
        return ret;
    }

    if (device_type == NPU_TYPE) {
        ret = dsmi_get_device_info(dev_id, DSMI_MAIN_CMD_UB, DSMI_UB_INFO_SUB_CMD_ID, (void *)ub_id_info, &out_size);
        if (ret != DSMI_OK) {
            gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
            return dcmi_convert_error_code(ret);
        }
    } else {
        gplog(LOG_ERR, "device_type %d is error.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    return DCMI_OK;
}

int dcmiv2_get_device_chip_info(int dev_id, struct dcmi_chip_info_v2 *chip_info)
{
    int err, card_id, device_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (chip_info == NULL) {
        gplog(LOG_ERR, "chip_info is invalid.");
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

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_get_device_type failed. err is %d.", err);
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

int dcmiv2_get_driver_version(char *driver_ver, unsigned int len)
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

    if (!dcmi_board_chip_type_is_ascend_950()) {
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

        break;
    }

    (void)length;
    return dcmi_convert_error_code(err);
}

int dcmiv2_get_dcmi_version(char *dcmi_ver, unsigned int len)
{
    int err;

    if ((dcmi_ver == NULL) || (len < DCMI_VERSION_MIN_LEN)) {
        gplog(LOG_ERR, "dcmi_ver or len %u is invalid.", len);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
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

int dcmiv2_get_device_die_id(int dev_id, enum dcmi_die_type input_type, struct dcmi_die_id *die_id)
{
    int err, card_id, device_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (die_id == NULL) {
        gplog(LOG_ERR, "die_id is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (input_type >= INVALID_DIE) {
        gplog(LOG_ERR, "input_type is invalid.");
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

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_get_device_type failed. err is %d.", err);
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

int dcmiv2_get_all_device_count(int *all_device_count)
{
    int ret;

    if (!(dcmi_is_in_phy_machine() || (dcmi_is_in_virtual_machine() == TRUE) ||
          dcmi_check_run_in_privileged_docker())) {
        gplog(LOG_ERR,
              "Operation not permitted, only physical or virtual machine or privileged container can call this api.");
        return DCMI_ERR_CODE_OPER_NOT_PERMITTED;
    }

    if (all_device_count == NULL) {
        gplog(LOG_ERR, "The all_device_count is null.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
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

int dcmiv2_set_device_info(int dev_id, enum dcmi_main_cmd main_cmd, unsigned int sub_cmd, const void *buf,
                           unsigned int buf_size)
{
    int err, card_id, device_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    err = dcmi_set_device_info_permission_check(main_cmd, sub_cmd);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "check permission failed. dev_id=%d, main_cmd=%u, sub_cmd=%u, err=%d", dev_id, main_cmd, sub_cmd,
              err);
        return err;
    }

    if (buf == NULL || main_cmd >= DCMI_MAIN_CMD_MAX) {
        gplog(LOG_ERR, "buf is NULL or main_cmd is invalid. main_cmd=%u", main_cmd);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (main_cmd == DCMI_MAIN_CMD_SEC && sub_cmd == DCMI_SEC_SUB_CMD_PSS) {
        gplog(LOG_OP, "This device does not support setting pkcs-enable status.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_cmd_product_support_check(main_cmd, sub_cmd);
    if (err != DCMI_OK) {
        return err;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &device_id, dev_id);
    if (err != DCMI_OK) {
        return err;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (device_type == NPU_TYPE) {
        err = dcmi_set_npu_device_info(card_id, device_id, main_cmd, sub_cmd, buf, buf_size);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "set device info failed. dev_id=%d, main_cmd=%u, sub_cmd=%u, err=%d", dev_id, main_cmd,
                  sub_cmd, err);
            return err;
        }
        gplog(LOG_OP, "set device info success. dev_id=%d, main_cmd=%u, sub_cmd=%u", dev_id, main_cmd, sub_cmd);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    return DCMI_OK;
}

int dcmiv2_get_device_info(int dev_id, enum dcmi_main_cmd main_cmd, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    int err, card_id, device_id;
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

    if (main_cmd == DCMI_MAIN_CMD_SEC && sub_cmd == DCMI_SEC_SUB_CMD_PSS) {
        gplog(LOG_OP, "This device does not support getting pkcs-enable status.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmi_cmd_product_support_check(main_cmd, sub_cmd);
    if (err != DCMI_OK) {
        return err;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &device_id, dev_id);
    if (err != DCMI_OK) {
        return err;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (device_type == NPU_TYPE) {
        return dcmiv2_get_npu_device_info(card_id, device_id, main_cmd, sub_cmd, buf, size);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    return DCMI_OK;
}

int dcmiv2_get_device_mac_cnt(int dev_id, int *count)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (count == NULL) {
        gplog(LOG_ERR, "count is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (dcmi_board_chip_type_is_ascend_950_card()) {
        gplog(LOG_OP, "This device does not support get device mac count.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (device_type == NPU_TYPE) {
        err = dsmi_get_mac_count(dev_id, count);
        if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
            gplog(LOG_ERR, "call dsmi_get_mac_count failed. err is %d.", err);
        }
        return dcmi_convert_error_code(err);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    return DCMI_OK;
}

int dcmiv2_get_device_component_static_version(int dev_id, enum dcmi_component_type component_type,
                                               unsigned char *version_str, unsigned int len)
{
    int err;
    enum dcmi_unit_type device_type = NPU_TYPE;
    unsigned int length;

    if (version_str == NULL) {
        gplog(LOG_ERR, "version_str is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (len < COMPONENT_VERSION_MIN_LEN) {
        gplog(LOG_ERR, "len %u is invalid.", len);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (device_type == NPU_TYPE) {
        err = dsmi_upgrade_get_component_static_version(dev_id, (DSMI_COMPONENT_TYPE)component_type, version_str, len,
                                                        &length);
        if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
            gplog(LOG_ERR, "call dsmi_upgrade_get_component_static_version failed. err is %d.", err);
        }

        return dcmi_convert_error_code(err);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

STATIC int dcmi_get_and_check_firmware_version(int dev_id, unsigned char *firmware_version, size_t version_len,
                                               enum dcmi_device_compat *compatibility)
{
    int ret;

    /* 获取固件运行版本 */
    ret = dcmiv2_get_firmware_version(dev_id, firmware_version, version_len);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "Get firmware version failed. (dev_id=%d, ret=%d)", dev_id, ret);
        return ret;
    }

    /* 不兼容6.4以前的固件版本。避免出现 用的新驱动+固件先装新的，再装老的，这样 flash中还是新的，导致显示OK */
    if (strcmp((const char *)firmware_version, (const char *)COMPAT_VERSION_START_FW) < 0) {
        *compatibility = DCMI_COMPAT_NOK;
        return DCMI_OK;
    }

    return DCMI_OK;
}

STATIC int check_driver_compatibility(int dev_id, enum dcmi_device_compat *compatibility)
{
    int ret;
    unsigned char driver_version[MAX_VER_LEN] = {0};
    unsigned char compat_list_drv[COMPAT_ITEM_SIZE_MAX] = {0};

    /* 1. 获取驱动运行版本 dcmi_get_driver_version 区分A5和其他代际 */
    ret = dcmiv2_get_driver_version((char *)driver_version, sizeof(driver_version));
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "Get driver version failed. (dev_id=%d, ret=%d)", dev_id, ret);
        return ret;
    }

    /* 2. 使用驱动运行版本对比固件的flash中的驱动兼容性列表 */
    ret = dcmiv2_get_device_user_config(dev_id, COMPAT_LIST_NAME, COMPAT_ITEM_SIZE_MAX, compat_list_drv);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "Get flash compat list %s. (dev_id=%d, ret=%d)",
              (ret == DCMI_ERR_CODE_NOT_SUPPORT) ? "not support" : "failed", dev_id, ret);
        *compatibility = DCMI_COMPAT_UNKNOWN;
        return DCMI_OK;
    }

    ret = dcmi_judge_compatibility(driver_version, MAX_VER_LEN, compat_list_drv, COMPAT_ITEM_SIZE_MAX, compatibility);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "2nd: Judge compatibility failed. (dev_id=%d, ret=%d)", dev_id, ret);
        return ret;
    }

    return DCMI_OK;
}

int dcmiv2_get_device_compatibility(int dev_id, enum dcmi_device_compat *compatibility)
{
    int ret, card_id, device_id;
    unsigned char firmware_version[MAX_VER_LEN] = {0};
    // 固件兼容性列表（从驱动version.info获取）
    unsigned char compat_list_fw[COMPAT_ITEM_SIZE_MAX] = {0};

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    ret = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &device_id, dev_id);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_inner_get_card_id_device_id_from_logicid failed. (ret=%d)", ret);
        return ret;
    }

    ret = dcmi_check_permission_of_get_compatibility(card_id, device_id, compatibility);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_check_permission_of_get_compatibility failed. (ret=%d)", ret);
        return ret;
    }

    /* 1. 获取固件版本并检查最低版本要求 */
    ret = dcmi_get_and_check_firmware_version(dev_id, firmware_version, MAX_VER_LEN, compatibility);
    if (ret != DCMI_OK) {
        return ret;
    } else if (*compatibility == DCMI_COMPAT_NOK) {
        return DCMI_OK; // 版本过低，直接返回
    }

    /* 2. 对比驱动的固件兼容性列表 */
    ret = dcmi_version_info_of_drv_by_field(COMPAT_FIELD_FW, compat_list_fw, COMPAT_ITEM_SIZE_MAX);
    if (ret == DCMI_OK) {
        ret = dcmi_judge_compatibility(firmware_version, MAX_VER_LEN, compat_list_fw, COMPAT_ITEM_SIZE_MAX,
                                       compatibility);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "1st: Judge compatibility failed. (device_id=%d, ret=%d)", device_id, ret);
            return ret;
        } else if (*compatibility == DCMI_COMPAT_OK) {
            return DCMI_OK;
        }
    }

    /* 2. 固件兼容性不通过时，再检查驱动兼容性 */
    return check_driver_compatibility(dev_id, compatibility);
}

int dcmiv2_get_computing_power_info(int dev_id, int type, struct dsmi_computing_power_info *computing_power)
{
#ifndef _WIN32
    int err;
    struct dcmi_chip_info_v2 chip_info = {{0}};
    const int aicore_cnt_type = 1;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (computing_power == NULL) {
        gplog(LOG_ERR, "computing_power is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (type != aicore_cnt_type) {
        gplog(LOG_ERR, "type %d is invalid.", type);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. (ret=%d)", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        err = dcmiv2_get_device_chip_info(dev_id, &chip_info);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "dcmi_get_device_chip_info failed. (ret=%d)", err);
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
#endif

int dcmiv2_get_mainboard_id(int dev_id, unsigned int *mainboard_id)
{
    int ret;
    unsigned int device_phy_id = 0;
    enum dcmi_unit_type device_type = INVALID_TYPE;

    if (mainboard_id == NULL) {
        gplog(LOG_ERR, "The mainboard_id is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    ret = dcmiv2_get_device_type(dev_id, &device_type);
    if (ret != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. (ret=%d)", ret);
        return ret;
    }

    if (device_type == NPU_TYPE) {
        ret = dcmiv2_get_chip_phy_id_by_dev_id(dev_id, &device_phy_id);
        if (ret != DCMI_OK) {
            gplog(LOG_ERR, "dcmiv2_get_chip_phy_id_by_dev_id failed. (ret=%d)", ret);
            return ret;
        }

        ret = dsmi_get_mainboard_id(device_phy_id, mainboard_id);
        if ((ret != DSMI_OK) && (ret != DSMI_ERR_NOT_SUPPORT)) {
            gplog(LOG_ERR, "Call dsmi_get_mainboard_id failed, (err=%d)", ret);
        }

        return dcmi_convert_error_code(ret);
    } else {
        gplog(LOG_ERR, "The device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmiv2_get_device_pcie_slot_id(int dev_id, int *pcie_slot)
{
    int err;
    int num_id, card_id, device_id;
    struct dcmi_card_info *card_info = NULL;

    if (pcie_slot == NULL) {
        gplog(LOG_ERR, "input pcie slot is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dev_id < 0) {
        gplog(LOG_ERR, "dev_id %d is invalid.", dev_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    if (!(dcmi_board_type_is_card() || dcmi_board_type_is_server())) {
        *pcie_slot = -1;
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &device_id, dev_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_inner_get_card_id_device_id_from_logicid failed. (ret=%d)", err);
        return err;
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

int dcmiv2_get_device_ub_slot_id(int dev_id, int *ub_slot)
{
    int err;
    int num_id, card_id, device_id;
    struct dcmi_card_info *card_info = NULL;

    if (ub_slot == NULL) {
        gplog(LOG_ERR, "input pcie slot is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dev_id < 0) {
        gplog(LOG_ERR, "dev_id %d is invalid.", dev_id);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!(dcmi_board_chip_type_is_ascend_950_pod())) {
        gplog(LOG_OP, "This product does not support this api.");
        *ub_slot = -1;
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &device_id, dev_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_inner_get_card_id_device_id_from_logicid failed. (ret=%d)", err);
        return err;
    }

    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];
        if (card_info->card_id == card_id) {
            *ub_slot = card_info->slot_id;
            return DCMI_OK;
        }
    }

    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int dcmiv2_get_chip_phy_id_by_dev_id(int dev_id, unsigned int *phyid)
{
    int err;
    unsigned int devid_u = (unsigned int)dev_id;

    if (phyid == NULL) {
        gplog(LOG_ERR, "phyid is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dsmi_get_phyid_from_logicid(devid_u, phyid);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "dsmi_get_phyid_from_logicid failed. (err=%d)", err);
    }
    return dcmi_convert_error_code(err);
}

int dcmiv2_get_dev_id_by_chip_phy_id(int phyid, unsigned int *dev_id)
{
    int err;
    unsigned int phyid_u = (unsigned int)phyid;

    if (dev_id == NULL) {
        gplog(LOG_ERR, "dev_id is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dsmi_get_logicid_from_phyid(phyid_u, dev_id);
    if ((err != DSMI_OK) && (err != DSMI_ERR_NOT_SUPPORT)) {
        gplog(LOG_ERR, "dsmi_get_logicid_from_phyid failed. (err=%d)", err);
    }
    return dcmi_convert_error_code(err);
}

STATIC int cmp_card_id_curr(const void *a, const void *b)
{
    int logic_id_a = *(int *)a;
    int logic_id_b = *(int *)b;
    return (logic_id_a > logic_id_b) ? 1 : -1;
}

int dcmiv2_get_device_list(int *device_list, int *device_cnt, int list_len)
{
    int num_id, chip_index, ret, device_logic_id;
    int cnt = 0;
    struct dcmi_card_info *card_info = NULL;
    int *card_id_curr = NULL;

    if (device_list == NULL || device_cnt == NULL) {
        gplog(LOG_ERR, "input para device_list or device_cnt is NULL.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (list_len <= 0) {
        gplog(LOG_ERR, "input para list_len is invalid, list_len is %d.", list_len);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (dcmi_get_run_env_init_flag() != TRUE) {
        gplog(LOG_ERR, "dcmi is not init.");
        return DCMI_ERR_CODE_INNER_ERR;
    }

    if (g_board_details.card_count > list_len) {
        gplog(LOG_ERR, "card_count is bigger than list_len.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (g_board_details.device_count > list_len) {
        gplog(LOG_ERR, "device_count is bigger than list_len.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    card_id_curr = device_list;
    for (num_id = 0; num_id < g_board_details.card_count; num_id++) {
        card_info = &g_board_details.card_info[num_id];
        for (chip_index = 0; chip_index < card_info->device_count; chip_index++) {
            ret = dcmiv2_get_device_logic_id(&device_logic_id, card_info->card_id, chip_index);
            if (ret != DCMI_OK) {
                gplog(LOG_ERR,
                      "dcmiv2_get_device_logic_id failed. "
                      "(card_id=%d, chip_index=%d,ret=%d)",
                      card_info->card_id, chip_index, ret);
                return ret;
            }
            if (cnt < list_len) {
                card_id_curr[cnt++] = device_logic_id;
            }
        }
    }

    qsort(card_id_curr, cnt, sizeof(card_id_curr[0]), cmp_card_id_curr);
    *device_cnt = cnt;
    return DCMI_OK;
}

int dcmiv2_get_device_voltage(int dev_id, unsigned int *voltage)
{
    int err, card_id, device_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (voltage == NULL) {
        gplog(LOG_ERR, "voltage is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &device_id, dev_id);
    if (err != DCMI_OK) {
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_voltage(card_id, device_id, voltage);
    } else if (device_type == MCU_TYPE) {
        return dcmi_mcu_get_voltage(card_id, voltage);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmiv2_get_device_temperature(int dev_id, int *temperature)
{
    int err, card_id, device_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (temperature == NULL) {
        gplog(LOG_ERR, "temperature is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_device_type failed. err is %d.", err);
        return err;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &device_id, dev_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_inner_get_card_id_device_id_from_logicid failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_temperature(card_id, device_id, temperature);
    } else if (device_type == MCU_TYPE) {
        return dcmi_mcu_get_temperature(card_id, temperature);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmiv2_get_device_board_id(int dev_id, unsigned int *board_id)
{
    int err, card_id, dev_phy_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (board_id == NULL) {
        gplog(LOG_ERR, "board_id is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &dev_phy_id, (unsigned int)dev_id);
    if (err != DCMI_OK) {
        return err;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_board_id(card_id, dev_phy_id, board_id);
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

int dcmiv2_get_device_component_cnt(int dev_id, unsigned int *component_count)
{
    int err, card_id, dev_phy_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (component_count == NULL) {
        gplog(LOG_ERR, "component_count is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &dev_phy_id, (unsigned int)dev_id);
    if (err != DCMI_OK) {
        return err;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_component_count(card_id, dev_phy_id, component_count);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmiv2_get_device_component_list(int dev_id, enum dcmi_component_type *component_table,
                                     unsigned int component_count)
{
    int err, card_id, dev_phy_id;
    enum dcmi_unit_type device_type = NPU_TYPE;

    if (component_table == NULL) {
        gplog(LOG_ERR, "component_table is invalid.");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (component_count == 0) {
        gplog(LOG_ERR, "component_count %u is invalid.", component_count);
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &dev_phy_id, (unsigned int)dev_id);
    if (err != DCMI_OK) {
        return err;
    }

    err = dcmiv2_get_device_type(dev_id, &device_type);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_get_device_type failed. err is %d.", err);
        return err;
    }

    if (device_type == NPU_TYPE) {
        return dcmi_get_npu_device_component_list(card_id, dev_phy_id, component_table, component_count);
    } else {
        gplog(LOG_ERR, "device_type %d is not support.", device_type);
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }
}

int dcmiv2_get_slot_id_and_chip_id_by_dev_id(int dev_id, unsigned int *slot_id, unsigned int *chip_id)
{
    int err;
    int card_id, device_id;
    struct dcmi_card_info *card_info = NULL;

    if (slot_id == NULL || chip_id == NULL || dev_id < 0 || dev_id >= MAX_CARD_NUM) {
        gplog(LOG_ERR,
              "Input param is invalid. "
              "(dev_id=%d, slot_id_is_NULL=%d, chip_id_is_NULL=%d, )",
              dev_id, (slot_id == NULL), (chip_id == NULL));
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_inner_get_card_id_device_id_from_logicid(&card_id, &device_id, dev_id);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_inner_get_card_id_device_id_from_logicid failed. err is %d.", err);
        return err;
    }

    err = dcmi_get_card_info(card_id, &card_info);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmi_get_card_info failed. err is %d.", err);
        return err;
    }

    *slot_id = card_info->slot_id;
    *chip_id = card_info->device_info[device_id].chip_slot;
    gplog(LOG_INFO, "Get slot_id %u, chip_id %u from dev_id %u.", *slot_id, *chip_id, dev_id);
    return DCMI_OK;
}

int dcmiv2_get_dev_id_by_slot_id_and_chip_id(unsigned int slot_id, unsigned int chip_id, int *dev_id)
{
    int err;
    struct dcmi_card_info *card_info = NULL;
    int device_list[MAX_CARD_NUM];
    int device_cnt = 0;

    if (dev_id == NULL) {
        gplog(LOG_ERR, "dev_id is NULL");
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    err = dcmiv2_get_device_list(device_list, &device_cnt, MAX_CARD_NUM);
    if (err != DCMI_OK) {
        gplog(LOG_ERR, "dcmiv2_get_device_list failed. err is %d.", err);
        return err;
    }
    for (int i = 0; i < device_cnt; i++) {
        err = dcmi_get_card_info(device_list[i], &card_info);
        if (err != DCMI_OK) {
            gplog(LOG_ERR, "dcmi_get_card_info failed. err is %d.", err);
            return err;
        }
        if (card_info->slot_id != slot_id) {
            continue;
        }
        for (int j = 0; j < card_info->device_count; j++) {
            if (card_info->device_info[j].chip_slot == chip_id) {
                *dev_id = card_info->device_info[j].logic_id;
                gplog(LOG_INFO, "Get dev_id %u by slot_id %u, chip_id %u.", *dev_id, slot_id, chip_id);
                return DCMI_OK;
            }
        }
    }
    gplog(LOG_ERR, "Can not find dev_id by slot_id %u, chip_id %u.", slot_id, chip_id);
    return DCMI_ERR_CODE_INVALID_PARAMETER;
}

int dcmiv2_get_group_intra_id_by_dev_id(int dev_id, unsigned int *group_intra_id)
{
    int ret;
    unsigned char buf;
    unsigned int buf_size = sizeof(unsigned char);

    if (group_intra_id == NULL || dev_id < 0 || dev_id >= MAX_CARD_NUM) {
        gplog(LOG_ERR,
              "Input param is invalid. "
              "(dev_id=%d, group_intra_id_is_NULL=%d)",
              dev_id, (group_intra_id == NULL));
        return DCMI_ERR_CODE_INVALID_PARAMETER;
    }

    if (!dcmi_board_chip_type_is_ascend_950()) {
        gplog(LOG_OP, "This product does not support this api.");
        return DCMI_ERR_CODE_NOT_SUPPORT;
    }

    ret = dsmi_get_device_info(dev_id, DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_INDEX_IN_GROUP, (void *)&buf,
                               &buf_size);
    if (ret != DSMI_OK) {
        gplog(LOG_ERR, "call dsmi_get_device_info failed. err is %d.", ret);
        return dcmi_convert_error_code(ret);
    }

    *group_intra_id = (unsigned char)buf;
    return DCMI_OK;
}
