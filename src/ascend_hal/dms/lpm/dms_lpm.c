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
#include "dms/dms_lpm_interface.h"
#include "devdrv_user_common.h"
#include "devdrv_ioctl.h"
#include "dms_device_info.h"
#include "dms/dms_devdrv_info_comm.h"
#include "ascend_hal_external.h"
#include "devmng_user_common.h"
#include "ascend_dev_num.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DEVDRV_DDR_TEMP_GEAR_MAX 32

STATIC int dms_lpm_parameter_check(struct dms_lpm_info_in *in, void *result)
{
    if (in->dev_id >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Invalid device id. (dev_id=%u)\n", in->dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (in->vfid >= VDAVINCI_MAX_VFID_NUM) {
        DMS_ERR("Invalid parameter. (dev_id=%u; vfid=%d)\n", in->dev_id, in->vfid);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (in->core_id >= INVALID_TSENSOR_ID) {
        DMS_ERR("Invalid core id. (dev_id=%u; core_id=%d)\n", in->dev_id, in->core_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (in->sub_cmd >= DMS_LPM_SUB_CMD_MAX) {
        DMS_ERR("Invalid subcommand. (dev_id=%u; sub_cmd=%d)\n", in->dev_id, in->sub_cmd);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (result == NULL) {
        DMS_ERR("Parameter result is NULL. (dev_id=%u)\n", in->dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return DRV_ERROR_NONE;
}

drvError_t DmsGetLpmInfo(struct dms_lpm_info_in *in, void *result, unsigned int result_size)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret, user_errno, ioctl_cmd;

#ifdef CFG_FEATURE_RECONSITUTION_TEMPORARY
    struct dms_filter_st filter = {0};
#endif

    ret = dms_lpm_parameter_check(in, result);
    if (ret != 0) {
        DMS_ERR("Invalid parameter. (dev_id=%u; ret=%d)\n", in->dev_id, ret);
        return ret;
    }

#ifdef CFG_FEATURE_SRIOV
    if (in->dev_id >= ASCEND_VDEV_ID_START) {
        /*
         * VF device id is start from ASCEND_VDEV_ID_START, each PF has VDAVINCI_MAX_VFID_NUM of VFs
         */
        in->dev_id = (in->dev_id - ASCEND_VDEV_ID_START) / VDAVINCI_MAX_VFID_NUM;
    }
#endif

    ioarg.main_cmd = DMS_MAIN_CMD_LPM;
    ioarg.sub_cmd = in->sub_cmd;
    ioarg.filter_len = 0;
    ioarg.input = (void *)in;
    ioarg.input_len = sizeof(struct dms_lpm_info_in);
    ioarg.output = result;
    ioarg.output_len = result_size;

#ifdef CFG_FEATURE_RECONSITUTION_TEMPORARY
    if (in->sub_cmd == DMS_LPM_GET_FREQUENCY && in->core_id == HBM_ID) {
        filter.filter_len = (unsigned int)sprintf_s(filter.filter, sizeof(filter.filter), "frequency_of_hbm");
        ioarg.filter = &filter.filter[0];
        ioarg.filter_len = filter.filter_len;
    }
#endif
    /* DMS_LPM_GET_FREQUENCY value is equal to DMS_SUBCMD_GET_FREQUENCY */
    ioctl_cmd = (in->sub_cmd == DMS_LPM_GET_FREQUENCY) ? DMS_GET_LPM_FREQUENCY : DMS_IOCTL_CMD;
    ret = DmsIoctl(ioctl_cmd, &ioarg);
    if (ret != 0) {
        user_errno = errno_to_user_errno(ret);
        DMS_EX_NOTSUPPORT_ERR(user_errno,
                              "Dms get device lpm info failed."
                              "(dev_id=%u; sub_cmd=%u; core_id=%u; ret=%d; user_errno=%d)\n",
                              in->dev_id, in->sub_cmd, in->core_id, ret, user_errno);
        return user_errno;
    }

    DMS_DEBUG("Dms get device lpm info success. (dev_id=%u; sub_cmd=%d)\n", in->dev_id, in->sub_cmd);
    return DRV_ERROR_NONE;
}

drvError_t dms_get_lpm_freq_v2(struct dms_lpm_info_in_v2 *in, unsigned int *frequency)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};

    if ((in == NULL) || (frequency == NULL)) {
        DMS_ERR("in or freq is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_LPM;
    ioarg.sub_cmd = DMS_SUBCMD_GET_FREQUENCY_V2;
    ioarg.filter_len = 0;
    ioarg.input = (void *)in;
    ioarg.input_len = (unsigned int)sizeof(struct dms_lpm_info_in_v2);
    ioarg.output = (void *)frequency;
    ioarg.output_len = (unsigned int)sizeof(unsigned int);

    ret = errno_to_user_errno(DmsIoctl((int)DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret,
                              "Dms get device lpm freq failed."
                              "(dev_id=%u; part_id=%u; core_id=%u; ret=%d;)\n",
                              in->dev_id, in->part_id, in->core_id, ret);
        return (drvError_t)ret;
    }

    DMS_DEBUG("Dms get device lpm freq success. (dev_id=%u; part_id=%u; core_id=%u;)\n", in->dev_id, in->part_id,
              in->core_id);
    return DRV_ERROR_NONE;
}

#define BUFSIZE_MIN 8
#define LP_ERRCODE_LENGTH 16

#define LP_ERRCODE_QUERY_FAIL 1
#define LP_ERRCODE_QUERY_TIMEOUT 2
#define LP_ERRCODE_QUERY_PARA_ERR 3
#define LP_ERRCODE_QUERY_NONSUPPORT 4

STATIC int dms_get_dsmi_errcode(u32 err_code)
{
    int ret;
    switch (err_code) {
        case LP_ERRCODE_QUERY_FAIL:
            DMS_ERR("[lp_errcode] LP_ERRCODE_QUERY_FAIL.\n");
            ret = DRV_ERROR_INNER_ERR;
            break;

        case LP_ERRCODE_QUERY_TIMEOUT:
            DMS_ERR("[lp_errcode] LP_ERRCODE_QUERY_TIMEOUT.\n");
            ret = DRV_ERROR_WAIT_TIMEOUT;
            break;

        case LP_ERRCODE_QUERY_PARA_ERR:
            DMS_ERR("[lp_errcode] LP_ERRCODE_QUERY_PARA_ERR.\n");
            ret = DRV_ERROR_INNER_ERR;
            break;

        case LP_ERRCODE_QUERY_NONSUPPORT:
            ret = DRV_ERROR_NOT_SUPPORT;
            break;

        default:
            ret = 0;
            break;
    }

    return ret;
}

STATIC int DmsCheckLpErrcode(struct ioctl_arg *arg)
{
    int ret;
    u32 voltage_err_code;
    u32 current_err_code;

    voltage_err_code = (u32)arg->data2;
    current_err_code = voltage_err_code & 0x0000ffff;
    voltage_err_code = voltage_err_code >> LP_ERRCODE_LENGTH;

    ret = dms_get_dsmi_errcode(voltage_err_code);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "get dsmi err_code failed, voltage_err_code = %d.\n", voltage_err_code);
        return ret;
    }

    ret = dms_get_dsmi_errcode(current_err_code);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "get dsmi err_code failed, current_err_code = %d.\n", current_err_code);
        return ret;
    }

    return ret;
}

STATIC int dms_get_voltage_current_from_lp(unsigned int dev_id, unsigned int sub_cmd, void *out_buf,
                                           unsigned int *buf_size)
{
    int ret;
    struct ioctl_arg arg = {0};
    unsigned int arg_size = sizeof(struct ioctl_arg);

    if ((sub_cmd >= DSMI_LP_SUB_CMD_MAX) || out_buf == NULL || buf_size == NULL) {
        DMS_ERR("input para illegal, sub_cmd=%u, out_buf or buf_size is null\n", sub_cmd);
        return DRV_ERROR_PARA_ERROR;
    }

    if (*buf_size < BUFSIZE_MIN) {
        DMS_ERR("input para illegal, buf_size=%u\n", *buf_size);
        return DRV_ERROR_PARA_ERROR;
    }

    arg.dev_id = dev_id;
    arg.vfid = DMS_VF_ID_PF;
    arg.type = DMS_LPM_SOC_ID; // core_id

    ret = DmsGetDeviceInfo(dev_id, DMS_MAIN_CMD_LP, sub_cmd, (void *)&arg, &arg_size);
    if (ret != 0) {
        DMS_ERR("DmsGetDeviceInfo failed, devid(%u), sub_cmd(%u), ret(%d).\n", dev_id, sub_cmd, ret);
        return ret;
    }

    ret = DmsCheckLpErrcode(&arg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "check lp_errcode failed.\n");
        return ret;
    }

    // 4 bytes voltage, 4 bytes current
    ret = memcpy_s(out_buf, sizeof(arg.data1), &arg.data1, sizeof(arg.data1));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed, ret = %d\n", ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    ret = memcpy_s(((char *)out_buf + sizeof(arg.data1)), sizeof(arg.data3), &arg.data3, sizeof(arg.data3));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed, ret = %d\n", ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    *buf_size = sizeof(arg.data1) + sizeof(arg.data3);
    return DRV_ERROR_NONE;
}

STATIC int dms_get_acg_from_lp(unsigned int dev_id, unsigned int sub_cmd, void *out_buf, unsigned int *buf_size)
{
    int ret;
    struct ioctl_arg arg = {0};
    unsigned int arg_size = sizeof(struct ioctl_arg);

    if ((sub_cmd >= DSMI_LP_SUB_CMD_MAX) || out_buf == NULL || buf_size == NULL) {
        DMS_ERR("input para illegal, sub_cmd=%u, out_buf or buf_size is null\n", sub_cmd);
        return DRV_ERROR_PARA_ERROR;
    }

    if (*buf_size < sizeof(int)) {
        DMS_ERR("input para illegal, buf_size=%u\n", *buf_size);
        return DRV_ERROR_PARA_ERROR;
    }

    arg.dev_id = dev_id;
    arg.vfid = DMS_VF_ID_PF;
    arg.type = DMS_LPM_SOC_ID; // core_id

    ret = DmsGetDeviceInfo(dev_id, DMS_MAIN_CMD_LP, sub_cmd, (void *)&arg, &arg_size);
    if (ret != 0) {
        DMS_ERR("DmsGetDeviceInfo failed, devid(%u), sub_cmd(%u), ret(%d).\n", dev_id, sub_cmd, ret);
        return ret;
    }

    ret = memcpy_s(out_buf, *buf_size, &arg.data1, sizeof(arg.data1));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed, ret = %d\n", ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    *buf_size = sizeof(arg.data1);
    return DRV_ERROR_NONE;
}
int DmsGetInfoFromLp(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                     unsigned int *buf_size)
{
    int ret;
    (void)vfid;
    if (out_buf == NULL || buf_size == NULL) {
        DMS_ERR("input para illegal, out_buf or buf_size is null pointer\n");
        return DRV_ERROR_PARA_ERROR;
    }

    switch (sub_cmd) {
        case DSMI_LP_SUB_CMD_AICORE_VOLTAGE_CURRENT:
        case DSMI_LP_SUB_CMD_HYBIRD_VOLTAGE_CURRENT:
        case DSMI_LP_SUB_CMD_TAISHAN_VOLTAGE_CURRENT:
        case DSMI_LP_SUB_CMD_DDR_VOLTAGE_CURRENT:
            ret = dms_get_voltage_current_from_lp(dev_id, sub_cmd, out_buf, buf_size);
            break;

        case DSMI_LP_SUB_CMD_ACG:
            ret = dms_get_acg_from_lp(dev_id, sub_cmd, out_buf, buf_size);
            break;

        default:
            DMS_ERR("dsmi sub_cmd is illegal! sub_cmd=%u\n", sub_cmd);
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "DmsGetInfoFromLp failed, ret = %d.\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int dms_set_temperature_threshold(unsigned int dev_id, unsigned int sub_cmd, void *in_buf, unsigned int buf_size)
{
    int ret;
    struct ioctl_arg arg = {0};
    unsigned int *temp_thold = (unsigned int *)in_buf;
    unsigned int arg_size = sizeof(struct ioctl_arg);

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (in_buf == NULL)) {
        DMS_ERR("dms_set_temperature_threshold para err, devid %d.\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((sub_cmd != DSMI_TEMP_SUB_CMD_DDR_THOLD) && (sub_cmd != DSMI_TEMP_SUB_CMD_SOC_THOLD) &&
        (sub_cmd != DSMI_TEMP_SUB_CMD_SOC_MIN_THOLD)) {
        DMS_ERR("dms_set_temperature_threshold sub_cmd %d err, devid %d.\n", sub_cmd, dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (buf_size < sizeof(unsigned int)) {
        DMS_ERR("size is invalid. (size=%u; expect_size=%u)\n", buf_size, sizeof(unsigned int));
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.type = DMS_LPM_SOC_ID; // core_id
    arg.dev_id = dev_id;
    arg.data1 = *temp_thold;
    arg.data2 = (int)buf_size;

    DMS_EVENT("dms_set_temperature_threshold: %d.\n", *temp_thold);
    ret = DmsSetDeviceInfo(dev_id, DMS_MAIN_CMD_TEMP, sub_cmd, (void *)&arg, arg_size);
    if (ret != 0) {
        DMS_ERR("DmsSetDeviceInfo failed, devid(%u), sub_cmd(%u), ret(%d).\n", dev_id, sub_cmd, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC int DmsGetDdrTemperature(unsigned int dev_id, unsigned int sub_cmd, void *out_buf, unsigned int *buf_size)
{
    int ret;
    struct dms_ddr_temp_arg arg = {0};
    unsigned int arg_size = sizeof(struct dms_ddr_temp_arg);
    unsigned char ddr_temp[DEVDRV_DDR_TEMP_GEAR_MAX] = {0};

    if (out_buf == NULL || buf_size == NULL) {
        DMS_ERR("input para illegal, out_buf or buf_size is null pointer, devid(%u).\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.dev_id = dev_id;
    arg.commoninfo = ddr_temp;
    arg.commoninfo_len = sizeof(ddr_temp);
    arg.vfid = DMS_VF_ID_PF;
    arg.core_id = DMS_LPM_SOC_ID;

    ret = DmsGetDeviceInfo(dev_id, DMS_MAIN_CMD_TEMP, sub_cmd, (void *)&arg, &arg_size);
    if (ret != 0) {
        DMS_ERR("DmsGetDeviceInfo failed, devid(%u), sub_cmd(%u), ret(%d).\n", dev_id, sub_cmd, ret);
        return ret;
    }

    /* length of actually read data from kernel ipc may exceed out_buf size */
    if (arg.commoninfo_len > *buf_size) {
        DMS_ERR("actual data length(%u) larger than *buf_size(%u), devid(%u).\n", arg.commoninfo_len, *buf_size,
                dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    ret = memcpy_s(out_buf, *buf_size, ddr_temp, sizeof(ddr_temp));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }
    *buf_size = arg.commoninfo_len;

    return DRV_ERROR_NONE;
}

STATIC int DmsGetTemperatureThreshold(unsigned int dev_id, unsigned int sub_cmd, void *out_buf, unsigned int *buf_size)
{
    int ret;
    unsigned int *temp_thold = (unsigned int *)out_buf;
    struct ioctl_arg arg = {0};
    unsigned int arg_size = sizeof(struct ioctl_arg);

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (out_buf == NULL) || (buf_size == NULL)) {
        DMS_ERR("DmsGetTemperatureThreshold para err, devid %d sub_cmd %d.\n", dev_id, sub_cmd);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (*buf_size != sizeof(unsigned int)) {
        DMS_ERR("user len %d is not %ld\n", *buf_size, sizeof(unsigned int));
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((sub_cmd != DSMI_TEMP_SUB_CMD_DDR_THOLD) && (sub_cmd != DSMI_TEMP_SUB_CMD_SOC_THOLD) &&
        (sub_cmd != DSMI_TEMP_SUB_CMD_SOC_MIN_THOLD)) {
        DMS_ERR("DmsGetTemperatureThreshold sub_cmd %d err, devid %d.\n", sub_cmd, dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.dev_id = dev_id;
    arg.vfid = DMS_VF_ID_PF;
    arg.type = DMS_LPM_SOC_ID; // core_id

    ret = DmsGetDeviceInfo(dev_id, DMS_MAIN_CMD_TEMP, sub_cmd, (void *)&arg, &arg_size);
    if (ret != 0) {
        DMS_ERR("DmsGetDeviceInfo failed, devid(%u), sub_cmd(%u), ret(%d).\n", dev_id, sub_cmd, ret);
        return ret;
    }

    *temp_thold = arg.data1;
    *buf_size = (unsigned int)arg.data2;

    return DRV_ERROR_NONE;
}

int DmsGetTemperature(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                      unsigned int *buf_size)
{
    int ret;
    (void)vfid;

    switch (sub_cmd) {
        case DSMI_SUB_CMD_TEMP_DDR:
            ret = DmsGetDdrTemperature(dev_id, sub_cmd, out_buf, buf_size);
            if (ret != 0) {
                DMS_ERR("DmsGetDdrTemperature failed, devid(%u), ret(%d).\n", dev_id, ret);
                return ret;
            }
            break;
        case DSMI_TEMP_SUB_CMD_DDR_THOLD:
        case DSMI_TEMP_SUB_CMD_SOC_THOLD:
        case DSMI_TEMP_SUB_CMD_SOC_MIN_THOLD:
            ret = DmsGetTemperatureThreshold(dev_id, sub_cmd, out_buf, buf_size);
            if (ret != 0) {
                DMS_ERR("DmsGetTemperatureThreshold failed, devid(%u), ret(%d).\n", dev_id, ret);
                return ret;
            }
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

int DmsGetTempFromLp(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)vfid;
    return halDmsGetDeviceInfoEx(dev_id, DMS_MAIN_CMD_TEMP, sub_cmd, buf, size);
}

int DmsGetLowPowerInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)vfid;
    int ret;
    unsigned int flag = 0;

    if (sub_cmd == DSMI_LP_SUB_CMD_GET_WORK_TOPS || sub_cmd == DSMI_LP_SUB_CMD_TOPS_DETAILS) {
        ret = devdrv_get_host_phy_mach_flag(dev_id, &flag);
        if (ret != 0) {
            return DRV_ERROR_NOT_SUPPORT;
        }

        if (flag != DEVDRV_HOST_PHY_MACH_FLAG) {
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    return halDmsGetDeviceInfoEx(dev_id, DMS_MAIN_CMD_LP, sub_cmd, buf, size);
}

int DmsSetLowPowerInfo(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size)
{
    return halDmsSetDeviceInfoEx(dev_id, DMS_MAIN_CMD_LP, sub_cmd, buf, size);
}

#ifdef CFG_FEATURE_PASS_THROUGH_MCU_BY_IMU
int DmsLpmPassThroughMcu(unsigned char rw_flag, unsigned char *buf, unsigned char buf_len, unsigned char *resp_buff,
                         unsigned char *recv_len)
{
    int ret;
    unsigned int dev_id = 0;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_pass_through_mcu_in in = {0};
    struct dms_pass_through_mcu_out out = {0};

    ret = drvGetLocalDevIDs(&dev_id, 1);
    if (ret != DRV_ERROR_NONE) {
        DMS_ERR("Get local device id failed. (ret=%d)\n", ret);
        return ret;
    }

    if (buf == NULL || recv_len == NULL || resp_buff == NULL || dev_id >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Parameter is invalid. (dev_id=%u; buf_is_null=%d; resp_buff_is_null=%d; recv_len_is_null=%d)\n",
                dev_id, (buf == NULL), (resp_buff == NULL), (recv_len == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    in.dev_id = dev_id;
    in.rw_flag = rw_flag;
    in.buf = buf;
    in.buf_len = buf_len;
    ioarg.main_cmd = DMS_MAIN_CMD_LPM;
    ioarg.sub_cmd = DMS_SUBCMD_PASS_THROUGTH_MCU;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_pass_through_mcu_in);
    ioarg.output = (void *)&out;
    ioarg.output_len = sizeof(struct dms_pass_through_mcu_out);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        /* will return fail when imu not inited, do not add log */
        return ret;
    }

    if (!rw_flag) {
        if (out.response_len > MCU_RESP_LEN) {
            DMS_ERR("Response length invalid. (dev_id=%u; response_len=%d)\n", dev_id, out.response_len);
            return DRV_ERROR_INVALID_HANDLE;
        }

        ret = memcpy_s(resp_buff, out.response_len, out.response_data, out.response_len);
        if (ret != 0) {
            DMS_ERR("Copy response data failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            *recv_len = 0;
            return DRV_ERROR_OUT_OF_MEMORY;
        }

        *recv_len = (unsigned char)out.response_len;
    }

    return DRV_ERROR_NONE;
}
#endif

struct aicore_current_freq {
    u32 dev_id;
    u32 freq;
};

int dms_lpm_get_ai_core_curr_freq(unsigned int devId, void *buf, unsigned int *size)
{
    int ret = 0;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_lpm_info_in freq_in = {0};
    unsigned int curr_freq = 0;

    if (devId >= ASCEND_PDEV_MAX_NUM || buf == NULL || size == NULL) {
        DMS_ERR("Invalid parameters. (dev_id=%u, buf%s, size%s)\n", devId, buf == NULL ? "=NULL" : "!=NULL",
                size == NULL ? "=NULL" : "!=NULL");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (*size != sizeof(unsigned int)) {
        DMS_ERR("Invalid parameters. (dev_id=%u, *size=%u)\n", devId, *size);
        return DRV_ERROR_INVALID_VALUE;
    }

#ifdef CFG_FEATURE_VDEVMNG_IN_VIRTUAL
    struct aicore_current_freq freq_info = {0};
    freq_info.dev_id = devId;
    if (DmsGetVirtFlag() != 0) {
        ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_CURRENT_AIC_FREQ, &freq_info);
        if (ret != 0) {
            DMS_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (dev_id=%u; ret=%d; errno=%d)\n", devId, ret, errno);
            return ret;
        } else {
            *(u32 *)buf = freq_info.freq;
            *size = sizeof(u32);
        }

        return ret;
    }
#endif

    freq_in.dev_id = devId;
    freq_in.core_id = AICORE0_ID;

    ioarg.main_cmd = DMS_MAIN_CMD_LPM;
    ioarg.sub_cmd = DMS_SUBCMD_GET_FREQUENCY;
    ioarg.filter_len = 0;
    ioarg.input = &freq_in;
    ioarg.input_len = sizeof(struct dms_lpm_info_in);
    ioarg.output = &curr_freq;
    ioarg.output_len = sizeof(unsigned int);
    ret = errno_to_user_errno(DmsIoctl(DMS_GET_LPM_FREQUENCY, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "drvGetAiCoreCurrFreq failed. (dev_id=%u, ret=%d)", devId, ret);
        return ret;
    }
    ret = memcpy_s(buf, *size, &curr_freq, sizeof(unsigned int));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed. (dev_id=%u, ret=%d)\n", devId, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    return ret;
}
