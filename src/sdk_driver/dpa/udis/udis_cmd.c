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

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "securec.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_runenv_config.h"
#include "urd_acc_ctrl.h"
#include "dms/dms_cmd_def.h"
#include "pbl_urd_sub_cmd_def.h"
#include "udis_management.h"
#include "udis_log.h"
#include "udis_cmd.h"

int udis_feature_get_device_info(struct urd_cmd *cmd, struct urd_cmd_kernel_para *kernel_para,
                                 struct urd_cmd_para *cmd_para);
int udis_feature_set_device_info(struct urd_cmd *cmd, struct urd_cmd_kernel_para *kernel_para,
                                 struct urd_cmd_para *cmd_para);

#define DMS_MODULE_UDIS "udis"
INIT_MODULE_FUNC(DMS_MODULE_UDIS);
EXIT_MODULE_FUNC(DMS_MODULE_UDIS);
BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_UDIS)
BEGIN_FEATURE_COMMAND()
#ifdef DRV_HOST
ADD_DEV_FEATURE_COMMAND(DMS_MODULE_UDIS, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_UDIS_DEVICE_INFO, NULL, NULL,
                        DMS_SUPPORT_ALL, udis_feature_get_device_info)
ADD_DEV_FEATURE_COMMAND(DMS_MODULE_UDIS, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_SET_UDIS_DEVICE_INFO, NULL, NULL,
                        DMS_ACC_ROOT_ONLY | DMS_ENV_ALL | DMS_VDEV_ALL, udis_feature_set_device_info)
#else
ADD_DEV_FEATURE_COMMAND(DMS_MODULE_UDIS, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_UDIS_DEVICE_INFO, NULL, NULL,
                        DMS_SUPPORT_ALL_USER, udis_feature_get_device_info)
ADD_DEV_FEATURE_COMMAND(DMS_MODULE_UDIS, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_SET_UDIS_DEVICE_INFO, NULL, "dmp_daemon",
                        DMS_SUPPORT_ALL, udis_feature_set_device_info)
#endif
END_FEATURE_COMMAND()
END_MODULE_DECLARATION();

STATIC int udis_ioctl_input_check(unsigned int udevid, unsigned int module_type, const char *name, const void *buf,
                                  unsigned int buf_len)
{
    unsigned int name_len = 0;

    if (module_type >= UDIS_MODULE_MAX) {
        udis_debug("Not support module_type. (udevid=%u; module_type=%u; name=%s; max_module_type=%u)\n", udevid,
                   module_type, name, UDIS_MODULE_MAX - 1);
        return -EOPNOTSUPP;
    }

    name_len = ka_base_strnlen(name, UDIS_MAX_NAME_LEN);
    if ((name_len == 0) || (name_len >= UDIS_MAX_NAME_LEN)) {
        udis_err("Invalid name. (udevid=%u; module_type=%u; name_len=%u; max_name_len=%d)\n", udevid, module_type,
                 name_len, UDIS_MAX_NAME_LEN - 1);
        return -EINVAL;
    }

    if (buf == NULL) {
        udis_err("Invalid param, buf is NULL. (udevid=%u; module_type=%u; name=%s)\n", udevid, module_type, name);
        return -EINVAL;
    }

    if ((buf_len == 0) || (buf_len > UDIS_MAX_DATA_LEN)) {
        udis_err("Invalid buf len. (udevid=%u; module_type=%u; name=%s; buf_len=%u; max_buf_len=%u)\n", udevid,
                 module_type, name, buf_len, UDIS_MAX_DATA_LEN);
        return -EINVAL;
    }
    return 0;
}

#define OWNER_ID_LEN 8
#define HEXADECIMAL 16
STATIC int udis_get_process_mem_name(unsigned int udevid, const char *in_name, char *out_name)
{
    int ret;
#ifdef DRV_HOST
    u32 vfid = 0, owner_id = 0, hostpid = 0;
    char temp_name[UDIS_MAX_NAME_LEN] = {0};
    ka_struct_pid_t *pgrp = NULL;
    struct uda_mia_dev_para mia_para = {0};

    if (!uda_is_pf_dev(udevid)) {
        ret = uda_udevid_to_mia_devid(udevid, &mia_para);
        if (ret != 0) {
            udis_err("The udevid to mia devid failed. (udev_id=%u; ret=%d)\n", udevid, ret);
            return ret;
        }

        vfid = mia_para.sub_devid + 1;
    }

    ret = strncpy_s(temp_name, UDIS_MAX_NAME_LEN, in_name, OWNER_ID_LEN);
    if (ret != 0) {
        udis_err("Strncpy_s failed. (udevid=%u; name=%s; ret=%d)\n", udevid, in_name, ret);
        return ret;
    }
    ret = ka_base_kstrtou32(temp_name, HEXADECIMAL, &owner_id);
    if (ret != 0) {
        udis_err("Get owner id from name failed. (udevid=%u; name=%s; ret=%d)\n", udevid, in_name, ret);
        return ret;
    }

    if (run_in_normal_docker() == true) {
        pgrp = ka_task_find_get_pid(owner_id);
        if (pgrp == NULL) {
            udis_err("Pgrp is NULL\n");
            return -EINVAL;
        }

        hostpid = pgrp->numbers[0].nr; // 0: hostpid
        ka_task_put_pid(pgrp);
    } else {
        hostpid = owner_id;
    }

    ret = snprintf_s(out_name, UDIS_MAX_NAME_LEN, UDIS_MAX_NAME_LEN - 1, "%08x%04xmem", hostpid, vfid);
    if (ret < 0) {
        udis_err("Snprintf_s failed. (udevid=%u; in_name=%s; ret=%d)\n", udevid, in_name, ret);
        return -EINVAL;
    }
#else
    ret = strcpy_s(out_name, UDIS_MAX_NAME_LEN, in_name);
    if (ret != 0) {
        udis_err("Strcpy_s failed. (udevid=%u; in_name=%s; ret=%d)\n", udevid, in_name, ret);
        return -EINVAL;
    }
#endif
    return 0;
}

STATIC int udis_get_dev_name(unsigned int udevid, UDIS_MODULE_TYPE module_type, const char *in_name, char *out_name)
{
    int ret;

    if ((module_type == UDIS_MODULE_SVM || module_type == UDIS_MODULE_APM) && ka_base_strstr(in_name, "mem") != NULL) {
        return udis_get_process_mem_name(udevid, in_name, out_name);
    }

    ret = strcpy_s(out_name, UDIS_MAX_NAME_LEN, in_name);
    if (ret != 0) {
        udis_err("Strcpy_s failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid, module_type, in_name, ret);
        return -EINVAL;
    }

    return 0;
}

STATIC int udis_get_device_info(unsigned int udevid, struct udis_get_ioctl_in *input, struct udis_get_ioctl_out *output)
{
    int ret;
    struct udis_dev_info info = {{0}};

    ret = udis_get_dev_name(udevid, input->module_type, input->name, info.name);
    if (ret != 0) {
        udis_err("Udis get dev name failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid, input->module_type,
                 input->name, ret);
        return ret;
    }

    ret = hal_kernel_get_udis_info(udevid, input->module_type, &info);
    if (ret != 0) {
        /* If a certain type of information does not support VF, the driver will not register or set the `name` for VF.
         * In this case, `get intf` will return `-ENODATA` because it cannot find the same name,
         * so `-ENODATA` is considered equivalent to `-EOPNOTSUPP` here.
         */
        ret = (ret == -ENODATA ? -EOPNOTSUPP : ret);
        udis_ex_notsupport_err(ret, "Get udis info failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
                               input->module_type, info.name, ret);
        return ret;
    }

    if (input->in_len < info.data_len) {
        udis_err("User buf length is too small. (udevid=%u; module_type=%u; name=%s; in_len=%u; data_len=%u)\n", udevid,
                 input->module_type, info.name, input->in_len, info.data_len);
        return -EINVAL;
    }

    ret = (int)ka_base_copy_to_user((void *)((uintptr_t)input->data), &info.data, info.data_len);
    if (ret != 0) {
        udis_err("Copy_to_user failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid, input->module_type,
                 info.name, ret);
        return ret;
    }

    output->out_len = info.data_len;
    output->last_update_time = info.last_update_time;
    output->acc_ctrl = info.acc_ctrl;

    return 0;
}

STATIC int udis_set_device_info(unsigned int udevid, const struct udis_set_ioctl_in *input)
{
    int ret;
    struct udis_dev_info info = {{0}};

    ret = strcpy_s(info.name, UDIS_MAX_NAME_LEN, input->name);
    if (ret != 0) {
        udis_err("Strcpy_s failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid, input->module_type,
                 input->name, ret);
        return -EINVAL;
    }

    ret = (int)ka_base_copy_from_user(&info.data, (void *)((uintptr_t)input->data), input->data_len);
    if (ret != 0) {
        udis_err("Copy_from_user failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid, input->module_type,
                 info.name, ret);
        return ret;
    }
    info.acc_ctrl = input->acc_ctrl;
    info.data_len = input->data_len;

    ret = hal_kernel_set_udis_info(udevid, input->module_type, &info);
    if (ret != 0) {
        udis_ex_notsupport_err(ret, "Set udis info failed. (udevid=%u; module_type=%u; name=%s; ret=%d)\n", udevid,
                               input->module_type, info.name, ret);
        return ret;
    }

    return 0;
}

#define UDEVID_2_PHYID_TABLE_SIZE 4
static char *g_udis_trans_udevid2phyid_table[UDEVID_2_PHYID_TABLE_SIZE] = {"aic_util", "aiv_util", "aicore_util",
                                                                           "stars_util"};
int udis_feature_get_device_info(struct urd_cmd *cmd, struct urd_cmd_kernel_para *kernel_para,
                                 struct urd_cmd_para *cmd_para)
{
    int ret;
    struct udis_get_ioctl_in *input = NULL;
    struct udis_get_ioctl_out *output = NULL;
    unsigned int i;
    unsigned int dev_id;

    if ((cmd_para->input == NULL) || (cmd_para->input_len != sizeof(struct udis_get_ioctl_in))) {
        udis_err("Invalid input. (in_is_null=%d; in_len=%d; valid_in_len=%zu)\n", cmd_para->input == NULL,
                 cmd_para->input_len, sizeof(struct udis_get_ioctl_in));
        return -EINVAL;
    }

    if ((cmd_para->output == NULL) || (cmd_para->output_len != sizeof(struct udis_get_ioctl_out))) {
        udis_err("Invalid output. (out_is_null=%d; out_len=%d; valid_out_len=%zu)\n", cmd_para->output == NULL,
                 cmd_para->output_len, sizeof(struct udis_get_ioctl_out));
        return -EINVAL;
    }

    input = (struct udis_get_ioctl_in *)cmd_para->input;
    output = (struct udis_get_ioctl_out *)cmd_para->output;

    ret = udis_ioctl_input_check(kernel_para->udevid, input->module_type, input->name, input->data, input->in_len);
    if (ret != 0) {
        return ret;
    }

    dev_id = kernel_para->udevid;
    for (i = 0; i < UDEVID_2_PHYID_TABLE_SIZE; i++) {
        if (ka_base_strcmp(g_udis_trans_udevid2phyid_table[i], input->name) == 0) {
            dev_id = kernel_para->phyid;
            break;
        }
    }
    ret = udis_get_device_info(dev_id, input, output);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

int udis_feature_set_device_info(struct urd_cmd *cmd, struct urd_cmd_kernel_para *kernel_para,
                                 struct urd_cmd_para *cmd_para)
{
    int ret;
    struct udis_set_ioctl_in *input = NULL;

    if ((cmd_para->input == NULL) || (cmd_para->input_len != sizeof(struct udis_set_ioctl_in))) {
        udis_err("Invalid input. (in_is_null=%d; in_len=%d; valid_in_len=%zu)\n", cmd_para->input == NULL,
                 cmd_para->input_len, sizeof(struct udis_set_ioctl_in));
        return -EINVAL;
    }

    input = (struct udis_set_ioctl_in *)cmd_para->input;

    ret = udis_ioctl_input_check(kernel_para->udevid, input->module_type, input->name, input->data, input->data_len);
    if (ret != 0) {
        return ret;
    }

    ret = udis_set_device_info(kernel_para->udevid, input);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

void udis_cmd_init(void)
{
    CALL_INIT_MODULE(DMS_MODULE_UDIS);
    udis_info("Udis register urd handle success.\n");
}

void udis_cmd_exit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_UDIS);
    udis_info("Udis unregister urd handle success.\n");
}