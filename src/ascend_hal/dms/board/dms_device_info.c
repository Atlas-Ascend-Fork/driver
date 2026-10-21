/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/ioctl.h>
#include "securec.h"
#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "dms_user_common.h"
#include "dms_cmd_def.h"
#include "dmc_user_interface.h"
#include "dms_device_info.h"
#include "ascend_dev_num.h"
#ifdef CFG_FEATURE_TRS_MODE
#include "trs_user_interface.h"
#endif

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

drvError_t dms_get_basic_info_host(unsigned int dev_id, void *buff, unsigned int sub_cmd, unsigned int size)
{
#ifdef DRV_HOST
    int ret;
    struct urd_ioctl_arg ioarg = {0};

    ioarg.devid = dev_id;
    ioarg.cmd.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.cmd.sub_cmd = sub_cmd;
    ioarg.cmd.filter = NULL;
    ioarg.cmd.filter_len = 0;
    ioarg.cmd_para.input = &dev_id;
    ioarg.cmd_para.input_len = sizeof(unsigned int);
    ioarg.cmd_para.output = buff;
    ioarg.cmd_para.output_len = size;

    ret = DmsIoctlConvertErrno(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "get basic info ioctl failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    return 0;
#else
    (void)dev_id;
    (void)buff;
    (void)sub_cmd;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

static int dms_errno_convert(int ret)
{
    if (ret == DRV_ERROR_NOT_SUPPORT || ret == DRV_ERROR_OPER_NOT_PERMITTED) {
        return ret;
    }
    return errno_to_user_errno(ret);
}

static drvError_t dms_set_ioctl(DSMI_MAIN_CMD main_cmd, struct dms_filter_st filter, struct dms_set_device_info_in in)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    (void)main_cmd;

    ioarg.main_cmd = DMS_GET_SET_DEVICE_INFO_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter = &filter.filter[0];
    ioarg.filter_len = filter.filter_len;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_set_device_info_in);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (main_cmd == DSMI_MAIN_CMD_SEC && ret != 0) {
        ret = dms_errno_convert(ret);
        if (ret != 0) {
            DMS_EX_NOTSUPPORT_ERR(ret, "Set device info failed. (ret=%d; filter:\"%s\")\n", ret, filter.filter);
            return ret;
        }
        DMS_DEBUG("Set device info success.\n");
        return DRV_ERROR_NONE;
    }

    ret = errno_to_user_errno(ret);
    if (ret != 0) {
        ret = (ret == DRV_ERROR_OPER_NOT_PERMITTED) ? DRV_ERROR_IOCRL_FAIL : ret;
        DMS_EX_NOTSUPPORT_ERR(ret, "Set Device info failed. (ret=%d; filter:\"%s\")\n", ret, filter.filter);
        return ret;
    }
    DMS_DEBUG("Set Device info success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsSetDeviceInfoEx(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                              unsigned int size)
{
    struct dms_filter_st filter = {0};
    struct dms_set_device_info_in in = {0};

    if ((buf == NULL) || (size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(&filter, main_cmd, sub_cmd);

    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = size;

    return dms_set_ioctl(main_cmd, filter, in);
}

drvError_t halDmsSetDeviceInfoEx(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, const void *buf,
                                 unsigned int size)
{
    return DmsSetDeviceInfoEx(dev_id, (DSMI_MAIN_CMD)main_cmd, sub_cmd, buf, size);
}

drvError_t DmsSetTsInfo(unsigned int dev_id, unsigned int sub_cmd, const void *buf, unsigned int size)
{
    struct urd_ioctl_arg ioarg = {0};
    struct dms_filter_st filter = {0};
    struct dms_set_device_info_in in = {0};

    if ((buf == NULL) || (size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(&filter, DSMI_MAIN_CMD_TS, sub_cmd);

    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = size;

    ioarg.devid = dev_id;
    ioarg.cmd.main_cmd = DMS_GET_SET_DEVICE_INFO_CMD;
    ioarg.cmd.sub_cmd = ZERO_CMD;
    ioarg.cmd.filter = &filter.filter[0];
    ioarg.cmd.filter_len = filter.filter_len;
    ioarg.cmd_para.input = (void *)&in;
    ioarg.cmd_para.input_len = sizeof(struct dms_set_device_info_in);
    ioarg.cmd_para.output = NULL;
    ioarg.cmd_para.output_len = 0;

    return DmsIoctlConvertErrno(DMS_IOCTL_CMD, &ioarg);
}

drvError_t DmsGetTsInfoEx(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                          unsigned int *size)
{
    struct urd_ioctl_arg ioarg = {0};
    struct dms_filter_st filter = {0};
    struct dms_set_device_info_in in = {0};
    struct dms_get_device_info_out out = {0};
    int ret;

    if ((buf == NULL) || (size == NULL)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(&filter, main_cmd, sub_cmd);

    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = *size;

    ioarg.devid = dev_id;
    ioarg.cmd.main_cmd = DMS_GET_GET_DEVICE_INFO_CMD;
    ioarg.cmd.sub_cmd = ZERO_CMD;
    ioarg.cmd.filter = &filter.filter[0];
    ioarg.cmd.filter_len = filter.filter_len;
    ioarg.cmd_para.input = (void *)&in;
    ioarg.cmd_para.input_len = sizeof(struct dms_set_device_info_in);
    ioarg.cmd_para.output = (void *)&out;
    ioarg.cmd_para.output_len = sizeof(struct dms_get_device_info_out);

    ret = DmsIoctlConvertErrno(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get Ts info ioctl failed. (ret=%d; filter:\"%s\")\n", ret, filter.filter);
        return ret;
    }

    *size = out.out_size;
    DMS_DEBUG("Get Ts info success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsSetDeviceInfo(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                            unsigned int size)
{
    struct dms_filter_st filter = {0};
    struct dms_set_device_info_in in = {0};

    if ((buf == NULL) || (size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }
    DMS_MAKE_UP_FILTER_DEVICE_INFO(&filter, main_cmd);

    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = size;

    return dms_set_ioctl(main_cmd, filter, in);
}

static drvError_t dms_get_ioctl(DSMI_MAIN_CMD main_cmd, struct dms_filter_st filter, struct dms_get_device_info_in in,
                                unsigned int *size)
{
    struct dms_ioctl_arg ioarg = {0};
    struct dms_get_device_info_out out = {0};
    int ret;

    ioarg.main_cmd = DMS_GET_GET_DEVICE_INFO_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter = &filter.filter[0];
    ioarg.filter_len = filter.filter_len;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_get_device_info_in);
    ioarg.output = (void *)&out;
    ioarg.output_len = sizeof(struct dms_get_device_info_out);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    /* when main_cmd is DSMI_MAIN_CMD_PCIE, only DSMI_PCIE_SUB_CMD_PCIE_INFO sub_cmd calls current function */
    if (ret != 0 && (main_cmd == DSMI_MAIN_CMD_SEC || main_cmd == DSMI_MAIN_CMD_PCIE)) {
        ret = dms_errno_convert(ret);
        if (ret != 0) {
            DMS_EX_NOTSUPPORT_ERR(ret, "Get device info failed. (ret=%d; filter:\"%s\")\n", ret, filter.filter);
            return ret;
        }
        DMS_DEBUG("Set device info success.\n");
        return DRV_ERROR_NONE;
    }

    ret = errno_to_user_errno(ret);
    if (ret != 0) {
        if (main_cmd != DSMI_MAIN_CMD_HCCS) {
            /* it is used to be compatible with the old errno. */
            ret = (ret == DRV_ERROR_OPER_NOT_PERMITTED) ? DRV_ERROR_IOCRL_FAIL : ret;
        }
        DMS_EX_NOTSUPPORT_ERR(ret, "Get Device info failed. (ret=%d; filter:\"%s\")\n", ret, filter.filter);
        return ret;
    }
    *size = out.out_size;
    DMS_DEBUG("Get Device info success.\n");
    return DRV_ERROR_NONE;
}

/* this function error code 65534 will be converted to 17 except for DSMI_MAIN_CMD_SEC and DSMI_MAIN_CMD_PCIE */
drvError_t DmsGetDeviceInfoEx(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                              unsigned int *size)
{
    struct dms_filter_st filter = {0};
    struct dms_get_device_info_in in = {0};

    if ((buf == NULL) || (size == NULL) || (*size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(&filter, main_cmd, sub_cmd);

    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = *size;

    return dms_get_ioctl(main_cmd, filter, in, size);
}

/* this function error code 65534 will not be converted to 17 */
drvError_t DmsGetDeviceInfoExV2(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                                unsigned int *size)
{
    struct dms_filter_st filter = {0};
    struct dms_get_device_info_in in = {0};
    struct dms_get_device_info_out out = {0};
    struct urd_cmd_para cmd_para = {0};
    struct urd_cmd cmd = {0};
    int ret;

    if ((buf == NULL) || (size == NULL) || (*size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(&filter, main_cmd, sub_cmd);

    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = *size;

    urd_usr_cmd_fill(&cmd, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, &filter.filter[0], filter.filter_len);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&in, sizeof(struct dms_get_device_info_in), (void *)&out,
                          sizeof(struct dms_get_device_info_out));
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get device info failed. (ret=%d; filter:\"%s\")\n", ret, filter.filter);
        return ret;
    }
    return DRV_ERROR_NONE;
}

drvError_t halDmsGetDeviceInfoEx(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                                 unsigned int *size)
{
    return DmsGetDeviceInfoEx(dev_id, (DSMI_MAIN_CMD)main_cmd, sub_cmd, buf, size);
}

drvError_t DmsGetDeviceInfo(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                            unsigned int *size)
{
    struct dms_filter_st filter = {0};
    struct dms_get_device_info_in in = {0};

    if ((buf == NULL) || (size == NULL) || (*size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO(&filter, main_cmd);

    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = *size;

    return dms_get_ioctl(main_cmd, filter, in, size);
}

drvError_t DmsGetDevBootStatus(int phy_id, unsigned int *boot_status)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    if ((uint32_t)phy_id >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Invalid physical ID. (phy_id=%d; max_dev_num=%u)\n", phy_id, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (boot_status == NULL) {
        DMS_ERR("boot_status is NULL. (phy_id=%d)\n", phy_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_BOOT_STATUS, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&phy_id, sizeof(unsigned int), (void *)boot_status, sizeof(unsigned int));
    ret = urd_usr_cmd(&cmd, &cmd_para);
#ifdef CFG_FEATURE_NO_DEVICE_NO_ERR
    if (ret == DRV_ERROR_NO_DEVICE) {
        DMS_WARN("No such device. (phy_id=%d; ret=%d)\n", phy_id, ret);
        return DRV_ERROR_NONE;
    }
#endif
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Failed to obtain the device startup status. (phy_id=%d; ret=%d)\n", phy_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t DmsGetChipType(unsigned int phy_id, unsigned int *chip_type)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    if (chip_type == NULL || phy_id >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Invalid parameter. (phy_id=%u; chip_type_is_NULL=%d)\n", phy_id, (chip_type == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CHIP_TYPE, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&phy_id, sizeof(unsigned int), (void *)chip_type, sizeof(unsigned int));
    ret = urd_usr_cmd_ex(DMS_GET_CHIP_TYPE, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get chip type ioctl failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
#ifdef CFG_FEATURE_ERR_CODE_NOT_OPTIMIZATION
        return ret == DRV_ERROR_NOT_SUPPORT ? DRV_ERROR_PARA_ERROR : ret;
#else
        return ret;
#endif
    }

    return DRV_ERROR_NONE;
}

drvError_t DmsGetMasterDevInTheSameOs(unsigned int phy_id, unsigned int *master_dev_id)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    if (phy_id >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Invalid physical ID. (phy_id=%u; max_dev_num=%u)\n", phy_id, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (master_dev_id == NULL) {
        DMS_ERR("The input parameter is NULL. (phy_id=%u)\n", phy_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_MASTER_DEV, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&phy_id, sizeof(unsigned int), (void *)master_dev_id,
                          sizeof(unsigned int));
    ret = urd_usr_cmd_ex(DMS_GET_MASTER_DEV, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get master device ioctl failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
#ifdef CFG_FEATURE_ERR_CODE_NOT_OPTIMIZATION
        if (ret == DRV_ERROR_NOT_SUPPORT) {
            return DRV_ERROR_NOT_SUPPORT;
        }
        return DRV_ERROR_INVALID_VALUE;
#else
        return ret;
#endif
    }

    return DRV_ERROR_NONE;
}

drvError_t DmsGetDevProbeNum(unsigned int *num)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    if (num == NULL) {
        DMS_ERR("Output parameter num is NULL.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_PROBE_NUM, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, NULL, 0, (void *)num, sizeof(unsigned int));
    ret = urd_usr_cmd_ex(DMS_GET_DEV_PROBE_NUM, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get probe num ioctl failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t DmsGetDevProbeList(unsigned int *devices, unsigned int len)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct urd_probe_dev_info para = {0};
    unsigned int i;
    int ret;

    if (devices == NULL || len > ASCEND_DEV_MAX_NUM || len == 0) {
        DMS_ERR("Invalid parameter. (len=%u; devices_is_NULL=%d)\n", len, (devices == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_PROBE_LIST, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, NULL, 0, (void *)&para, sizeof(struct urd_probe_dev_info));
    ret = urd_usr_cmd_ex(DMS_GET_DEV_PROBE_LIST, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get probe list ioctl failed. (ret=%d)\n", ret);
        return ret;
    }

    for (i = 0; i < len; i++) {
        devices[i] = para.devids[i];
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t dms_hal_dev_info_ioctl(unsigned int main_cmd, struct dms_filter_st filter,
                                         struct dms_hal_device_info_stru *info)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    uint32_t len = DMS_HAL_DEV_INFO_HEAD_LEN + info->buff_size;
    unsigned long ioctl_cmd = (main_cmd == DMS_GET_GET_DEVICE_INFO_CMD) ? URD_QUERY_IOCTL_CMD : URD_IOCTL_CMD;
    int ret;

    urd_usr_cmd_fill(&cmd, main_cmd, ZERO_CMD, &filter.filter[0], filter.filter_len);
    urd_usr_cmd_para_fill(&cmd_para, (void *)info, len, (void *)info, len);
    ret = urd_usr_cmd_ex(ioctl_cmd, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "The hal_dev_info ioctl failed. (ret=%d)\n", ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

drvError_t DmsHalGetDeviceInfoEx(unsigned int dev_id, int module_type, int info_type, void *buf, unsigned int *size)
{
    struct dms_filter_st filter = {0};
    struct dms_hal_device_info_stru info = {0};
    drvError_t ret;
    int s_ret;

    if ((buf == NULL) || (size == NULL) || (*size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_HAL_DEV_INFO_EX(&filter, module_type, info_type);

    info.dev_id = dev_id;
    info.module_type = module_type;
    info.info_type = info_type;
    info.buff_size = *size;
    s_ret = memcpy_s(info.payload, sizeof(info.payload), buf, *size);
    if (s_ret != 0) {
        DMS_ERR("memcpy fail.(ret=%d;size=%u;param_size=%u)\n", s_ret, sizeof(info.payload), *size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dms_hal_dev_info_ioctl(DMS_GET_GET_DEVICE_INFO_CMD, filter, &info);
    if (ret != 0) {
        return ret;
    }
    s_ret = memcpy_s(buf, *size, info.payload, info.buff_size);
    if (s_ret != 0) {
        DMS_ERR("memcpy fail.(ret=%d;ret_size=%u;param_size=%u)\n", s_ret, info.buff_size, *size);
        return DRV_ERROR_PARA_ERROR;
    }
    *size = info.buff_size;
    return ret;
}

drvError_t DmsHalSetDeviceInfoEx(unsigned int dev_id, int module_type, int info_type, const void *buf,
                                 unsigned int size)
{
    struct dms_filter_st filter = {0};
    struct dms_hal_device_info_stru info = {0};
    drvError_t ret;
    int s_ret;

    if ((buf == NULL) || (size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_HAL_DEV_INFO_EX(&filter, module_type, info_type);

    info.dev_id = dev_id;
    info.module_type = module_type;
    info.info_type = info_type;
    info.buff_size = size;
    s_ret = memcpy_s(info.payload, sizeof(info.payload), buf, size);
    if (s_ret != 0) {
        DMS_ERR("memcpy fail.(ret=%d;size=%u;param_size=%u)\n", s_ret, sizeof(info.payload), size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dms_hal_dev_info_ioctl(DMS_GET_SET_DEVICE_INFO_CMD, filter, &info);
    return ret;
}

drvError_t dms_set_detect_info(unsigned int dev_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                               unsigned int size)
{
    struct dms_set_device_info_in in = {0};
    struct dms_filter_st filter = {0};
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    DMS_MAKE_UP_FILTER_DEVICE_INFO(&filter, main_cmd);
    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = size;

    ioarg.main_cmd = DMS_GET_SET_DETECT_INFO_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter = &filter.filter[0];
    ioarg.filter_len = filter.filter_len;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_set_device_info_in);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        ret = (ret == DRV_ERROR_OPER_NOT_PERMITTED) ? DRV_ERROR_IOCRL_FAIL : ret;
        DMS_EX_NOTSUPPORT_ERR(ret, "Set Detect info failed. (ret=%d; filter:\"%s\")\n", ret, filter.filter);
        return ret;
    }

    DMS_DEBUG("Set detect info success.\n");
    return DRV_ERROR_NONE;
}

drvError_t dms_get_detect_info(unsigned int dev_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                               unsigned int *size)
{
    struct dms_get_device_info_out out = {0};
    struct dms_get_device_info_in in = {0};
    struct dms_filter_st filter = {0};
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    if ((buf == NULL) || (size == NULL) || (*size == 0)) {
        DMS_ERR("Invalid parameter. (dev_id=%u; buf=%d; size=%d)\n", dev_id, (buf != NULL), (size != NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO(&filter, main_cmd);
    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = *size;

    ioarg.main_cmd = DMS_GET_GET_DETECT_INFO_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter = &filter.filter[0];
    ioarg.filter_len = filter.filter_len;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_get_device_info_in);
    ioarg.output = (void *)&out;
    ioarg.output_len = sizeof(struct dms_get_device_info_out);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get Detect info failed. (ret=%d; filter:\"%s\")\n", ret, filter.filter);
        return ret;
    }
    *size = out.out_size;
    DMS_DEBUG("Get Detect info success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsGetAiCoreDieNum(unsigned int dev_id, long long *value)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    unsigned int die_num = 0;
    int ret;

    if (value == NULL) {
        DMS_ERR("Invalid parameter. (dev_id=%u; value_is_NULL=%d)\n", dev_id, (value == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_AICORE_DIE_NUM, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, NULL, 0, (void *)&die_num, sizeof(unsigned int));
    ret = urd_dev_usr_cmd_ex(dev_id, DMS_GET_AICORE_DIE_NUM, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get aicore die num failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    *value = (long long)die_num;

    return DRV_ERROR_NONE;
}

drvError_t dms_get_hcom_cpu_num(unsigned int dev_id, long long *value)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    unsigned int hcom_cpu_num = 0;
    int ret;

    if (value == NULL) {
        DMS_ERR("Invalid parameter. (dev_id=%u; value_is_NULL=%d)\n", dev_id, (value == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_HCOM_CPU_NUM, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, NULL, 0, (void *)&hcom_cpu_num, sizeof(unsigned int));
    ret = urd_dev_usr_cmd_ex(dev_id, DMS_GET_HCOM_CPU_NUM, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get hcom cpu num failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    *value = (long long)hcom_cpu_num;

    return DRV_ERROR_NONE;
}

drvError_t dms_get_connect_type(int64_t *type)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    if (type == NULL) {
        DMS_ERR("type is Null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CONNECT_TYPE, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, NULL, 0, (void *)type, sizeof(int64_t));
    ret = urd_usr_cmd(&cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get connect type ioctl failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t DmsGetCpuWorkMode(unsigned int dev_id, long long *value)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    if (value == NULL) {
        DMS_ERR("Invalid parameter. (dev_id=%u; value_is_NULL=%d)\n", dev_id, (value == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CPU_WORK_MODE, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, NULL, 0, (void *)value, sizeof(long long));
    ret = urd_dev_usr_cmd_ex(dev_id, DMS_GET_CPU_WORK_MODE, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get cpu work mode failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t DmsSetTrsMode(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, const void *buf,
                         unsigned int size)
{
#ifdef CFG_FEATURE_TRS_MODE
    const DSMI_TRS_CONFIG_STRU *input = NULL;
    struct trs_mode_info info = {0};
    int ret;
    (void)main_cmd;

    if (buf == NULL || size != sizeof(DSMI_TRS_CONFIG_STRU)) {
        DMS_ERR("Invalid parameter. (dev_id=%u; sizeof=%u; buf_is_NULL=%d)\n", dev_id, size, (buf == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    input = (const DSMI_TRS_CONFIG_STRU *)buf;
    info.dev_id = dev_id;
    info.ts_id = input->ts_id;
    info.mode = input->mode;
    info.mode_type = (trs_mode_type_t)sub_cmd;
    ret = trs_mode_config(&info);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Failed to configure kernel launch mode. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
#else
    (void)dev_id;
    (void)main_cmd;
    (void)sub_cmd;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t DmsGetTrsMode(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                         unsigned int *size)
{
#ifdef CFG_FEATURE_TRS_MODE
    DSMI_TRS_CONFIG_STRU *input = NULL;
    struct trs_mode_info info = {0};
    int ret;
    (void)main_cmd;

    if (buf == NULL || *size != sizeof(DSMI_TRS_CONFIG_STRU)) {
        DMS_ERR("Invalid parameter. (dev_id=%u; size=%u; need_size=%lu, buf_is_NULL=%d)\n", dev_id, *size,
                sizeof(DSMI_TRS_CONFIG_STRU), (buf == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    input = (DSMI_TRS_CONFIG_STRU *)buf;
    info.dev_id = dev_id;
    info.ts_id = input->ts_id;
    info.mode_type = (trs_mode_type_t)sub_cmd;
    ret = trs_mode_query(&info);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Failed to query mode. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    input->mode = info.mode;

    return DRV_ERROR_NONE;
#else
    (void)dev_id;
    (void)main_cmd;
    (void)sub_cmd;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t dms_get_process_resource(unsigned int dev_id, struct dsmi_resource_para *para, void *buf,
                                    unsigned int buf_len)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    if (para == NULL || buf == NULL) {
        DMS_ERR("Invalid parameter. (dev_id=%u; para_is_NULL=%d; buf_is_NULL=%d)\n", dev_id, (para == NULL),
                (buf == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    if ((para->resource_type == DSMI_DEV_PROCESS_PID) || (para->resource_type == DSMI_DEV_PROCESS_CONTAINER_PID)) {
        urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_PROCESS_LIST, NULL, 0);
        urd_usr_cmd_para_fill(&cmd_para, &para->resource_type, sizeof(unsigned int), buf, buf_len);
    } else if (para->resource_type == DSMI_DEV_PROCESS_MEM) {
        urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_PROCESS_MEMORY, NULL, 0);
        urd_usr_cmd_para_fill(&cmd_para, (void *)&para->owner_id, sizeof(unsigned int), buf, buf_len);
    } else {
        DMS_ERR("Invalid parameter. (dev_id=%u; resource_type=%u; valid=DSMI_DEV_PROCESS_PID, "
                "DSMI_DEV_PROCESS_CONTAINER_PID, DSMI_DEV_PROCESS_MEM)\n",
                dev_id, para->resource_type);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = urd_dev_usr_cmd(dev_id, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Failed to retrieve process information. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t dms_set_sign_flag_ioctl(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                                   unsigned int size)
{
    struct dms_filter_st filter = {0};
    struct dms_hal_device_info_stru info = {0};
    drvError_t ret;
    int s_ret;

    if ((buf == NULL) || (size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(&filter, main_cmd, sub_cmd);

    info.dev_id = dev_id;
    info.buff_size = size;
    s_ret = memcpy_s(info.payload, sizeof(info.payload), buf, size);
    if (s_ret != 0) {
        DMS_ERR("Memcpy fail.(ret=%d;size=%u;param_size=%u)\n", s_ret, sizeof(info.payload), size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dms_hal_dev_info_ioctl(DMS_GET_SET_DEVICE_INFO_CMD, filter, &info);
    return ret;
}

drvError_t halDmsSetSignFlag(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, const void *buf,
                             unsigned int size)
{
    return dms_set_sign_flag_ioctl(dev_id, (DSMI_MAIN_CMD)main_cmd, sub_cmd, buf, size);
}

drvError_t dms_get_sign_flag_ioctl(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                                   unsigned int *size)
{
    struct dms_filter_st filter = {0};
    struct dms_hal_device_info_stru info = {0};
    drvError_t ret;
    int s_ret;

    if ((buf == NULL) || (size == NULL) || (*size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_DEVICE_INFO_EX(&filter, main_cmd, sub_cmd);

    info.dev_id = dev_id;
    info.buff_size = *size;
    s_ret = memcpy_s(info.payload, sizeof(info.payload), buf, *size);
    if (s_ret != 0) {
        DMS_ERR("Memcpy fail.(ret=%d;size=%u;param_size=%u)\n", s_ret, sizeof(info.payload), *size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dms_hal_dev_info_ioctl(DMS_GET_GET_DEVICE_INFO_CMD, filter, &info);
    if (ret != 0) {
        return ret;
    }
    s_ret = memcpy_s(buf, *size, info.payload, info.buff_size);
    if (s_ret != 0) {
        DMS_ERR("Memcpy fail.(ret=%d;ret_size=%u;param_size=%u)\n", s_ret, info.buff_size, *size);
        return DRV_ERROR_PARA_ERROR;
    }
    *size = info.buff_size;
    return ret;
}

drvError_t halDmsGetSignFlag(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                             unsigned int *size)
{
    return dms_get_sign_flag_ioctl(dev_id, (DSMI_MAIN_CMD)main_cmd, sub_cmd, buf, size);
}

drvError_t dms_get_phy_dev_info(unsigned int phy_id, int info_type, unsigned int *val)
{
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    struct dms_get_phy_dev_info_out output = {0};
    int ret;

    if (val == NULL) {
        DMS_ERR("The input parameter is NULL. (phy_id=%u)\n", phy_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_PHY_DEVICE_INFO, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&phy_id, sizeof(unsigned int), (void *)&output,
                          sizeof(struct dms_get_phy_dev_info_out));
    ret = urd_usr_cmd_ex(DMS_GET_PHY_DEVICE_INFO, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get physical device info failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }

    switch (info_type) {
        case PHY_INFO_TYPE_PHY_CHIP_ID:
            *val = output.chip_id;
            break;
        case PHY_INFO_TYPE_PHY_DIE_ID:
            *val = output.die_id;
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}
