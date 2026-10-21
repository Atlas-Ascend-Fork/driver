/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <limits.h>
#include <sys/ioctl.h>
#include "securec.h"
#include "dms_user_common.h"
#include "dms/dms_misc_interface.h"
#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devdrv_user_common.h"
#include "dms_device_info.h"
#include "dev_mon_log.h"
#include "ascend_hal_define.h"
#include "dms_ts_info_ioctl.h"
#include "ascend_dev_num.h"

#define DMS_AI_OR_VECTOR_CORE_NUM_MAX 72
#if (defined CFG_FEATURE_AIC_AIV_CONTINUOUS_UTILIZATION) || (defined CFG_FEATURE_SET_STL_RUNNING_STATUS)
#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif
#endif
#ifdef CFG_FEATURE_AIC_AIV_CONTINUOUS_UTILIZATION
STATIC int dms_set_core_utilization_asyn(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size)
{
    int ret = 0;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_set_device_info_in in = {0};

    if (buf == NULL || size == 0) {
        DMS_ERR("Input buf is null or size is zero . (buf=%d; size=%d)\n", (buf != NULL), size);
        return DRV_ERROR_PARA_ERROR;
    }
    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = size;
    ioarg.main_cmd = DMS_MAIN_CMD_TRS;
    ioarg.sub_cmd = DMS_SUBCMD_SET_AI_INFO_ASYN;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_set_device_info_in);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (ret=%d; user_errno=%d)\n", ret, errno_to_user_errno(ret));
        return errno_to_user_errno(ret);
    }
    return DRV_ERROR_NONE;
}

STATIC int dms_get_core_utilization_asyn(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    int ret = 0;
    struct dms_ioctl_arg ioarg = {};
    struct dms_get_device_info_in in = {};
    struct dms_get_device_info_out out = {};

    if (buf == NULL || size == NULL) {
        DMS_ERR("Input buf or size is null. (buf=%d; size=%d)\n", (buf != NULL), (size != NULL));
        return DRV_ERROR_PARA_ERROR;
    }
    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = *size;

    ioarg.main_cmd = DMS_MAIN_CMD_TRS;
    ioarg.sub_cmd = DMS_SUBCMD_GET_AI_INFO_ASYN;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_get_device_info_in);
    ioarg.output = (void *)&out;
    ioarg.output_len = sizeof(struct dms_get_device_info_out);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_ERR("Ioctl failed. (ret=%d; user_errno=%d)\n", ret, errno_to_user_errno(ret));
        return errno_to_user_errno(ret);
    }
    *size = out.out_size;
    return DRV_ERROR_NONE;
}
#endif

#ifdef CFG_FEATURE_SET_STL_RUNNING_STATUS
static drvError_t tsdrv_check_stl_params(unsigned int sub_cmd, uint32_t devId, void *buf, unsigned int paramSize)
{
    struct ts_stl_start_info *info = (struct ts_stl_start_info *)buf;

    if (devId >= (uint32_t)ASCEND_DEV_MAX_NUM) {
        DMS_ERR("DevId is invalid. (devId=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }
    if (info == NULL) {
        DMS_ERR("Null ptr.\n");
        return DRV_ERROR_PARA_ERROR;
    }
    if (sub_cmd == DSMI_TS_SUB_CMD_START_PERIOD_AICORE_STL) {
        if (paramSize != sizeof(struct ts_stl_start_info)) {
            DMS_ERR("Size invalid. (now=%lu; target=%lu)\n", paramSize, sizeof(struct ts_stl_start_info));
            return DRV_ERROR_PARA_ERROR;
        }

        if (info->period > TSDRV_STL_PERIOD_MAX || info->period < TSDRV_STL_PERIOD_MIN) {
            DMS_ERR("Period invalid. (now=%lu)\n", info->period);
            return DRV_ERROR_PARA_ERROR;
        }
    }
    return DRV_ERROR_NONE;
}

STATIC int dms_set_stl_running_status(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size)
{
    int ret = 0;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_set_device_info_in in = {0};

    ret = tsdrv_check_stl_params(sub_cmd, dev_id, buf, size);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    in.dev_id = dev_id;
    in.sub_cmd = sub_cmd;
    in.buff = buf;
    in.buff_size = size;
    ioarg.main_cmd = DMS_MAIN_CMD_TRS;
    ioarg.sub_cmd = DMS_SUBCMD_SET_STL_RUNNING_STATUS;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_set_device_info_in);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        if (ret == DRV_ERROR_NOT_SUPPORT) {
            DMS_INFO("STL set running status is not supported.\n");
        } else if (ret == DRV_ERROR_UNINIT) {
            DMS_ERR("STL is not bound.\n");
        } else if (ret == DRV_ERROR_BUSY) {
            DMS_WARN("STL start repeat.\n");
        } else {
            DMS_ERR("Ioctl failed. (ret=%d;)\n", ret);
            ret = DRV_ERROR_IOCRL_FAIL;
        }
        return ret;
    }
    return DRV_ERROR_NONE;
}
#endif

struct udis_ts_info_adapter {
    unsigned int sub_cmd;
    const char *name;
    int (*callback)(unsigned int device_id, const char *name, void *buf, unsigned int *size);
};

static struct udis_ts_info_adapter g_get_ts_info_table[] = {
#ifdef CFG_FEATURE_UDIS_UTIL_FROM_TS
    {DSMI_TS_SUB_CMD_AICORE_UTILIZATION_RATE, "aic_util", dms_get_ts_info_from_udis},
    {DSMI_TS_SUB_CMD_VECTORCORE_UTILIZATION_RATE, "aiv_util", dms_get_ts_info_from_udis},
    {DSMI_TS_SUB_CMD_NPU_MULTI_UTILIZATION_RATE, "npu_multi_util", dms_get_npu_multi_util_from_udis},
    {DSMI_TS_SUB_CMD_NPU_MULTI_UTILIZATION_RATE_V2, "npu_multi_util_v2", dms_get_npu_multi_util_from_udis},
#endif
};

int dms_get_ts_info(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf, unsigned int *size)
{
    int ret = 0;
    unsigned int i;
    unsigned int table_size = sizeof(g_get_ts_info_table) / sizeof(g_get_ts_info_table[0]);

    if (sub_cmd >= DSMI_TS_SUB_CMD_MAX) {
        DMS_ERR("Sub_cmd is invalid. (dev_id=%u; vfid=%u; sub_cmd=%u; max=%u)\n", dev_id, vfid, sub_cmd,
                DSMI_TS_SUB_CMD_MAX);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < table_size; i++) {
        if (g_get_ts_info_table[i].sub_cmd == sub_cmd) {
            return g_get_ts_info_table[i].callback(dev_id, g_get_ts_info_table[i].name, out_buf, size);
        }
    }

    /* CFG_FEATURE_UDIS_UTIL_FROM_TS not enable */
    switch (sub_cmd) {
#ifdef CFG_FEATURE_AIC_AIV_UTIL_FROM_TS
        case DSMI_TS_SUB_CMD_AICORE_UTILIZATION_RATE:
        case DSMI_TS_SUB_CMD_VECTORCORE_UTILIZATION_RATE:
        case DSMI_TS_SUB_CMD_NPU_MULTI_UTILIZATION_RATE:
            ret = dms_get_single_util_from_ts(dev_id, vfid, sub_cmd, out_buf, size);
            if (ret != 0) {
                DMS_EX_NOTSUPPORT_ERR(ret, "Get single util from ts failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id,
                                      vfid, ret);
                return ret;
            }
            break;
#endif
#ifdef CFG_FEATURE_AIC_AIV_CONTINUOUS_UTILIZATION
        case DSMI_TS_SUB_CMD_AIC_UTILIZATION_RATE_ASYN:
        case DSMI_TS_SUB_CMD_AIV_UTILIZATION_RATE_ASYN:
            ret = dms_get_core_utilization_asyn(dev_id, sub_cmd, out_buf, size);
            break;
#endif
        case DSMI_TS_SUB_CMD_GET_FAULT_MASK:
            ret = DmsGetDeviceInfo(dev_id, DSMI_MAIN_CMD_TS, sub_cmd, out_buf, size);
            if (ret != 0) {
                DEV_MON_EX_NOTSUPPORT_ERR(ret, "Get ts fault mask failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid,
                                          ret);
                return ret;
            }
            break;
        case DSMI_TS_SUB_CMD_AICORE_STL_STATUS:
            ret = halTsdrvCtl(dev_id, TSDRV_CTL_CMD_QUERY_STL, (void *)NULL, 0, out_buf, (size_t *)size);
            if (ret != 0) {
                DMS_ERR("Get STL status failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
                return ret;
            }
            break;
        case DSMI_TS_SUB_CMD_STREAM_AVAILABLE:
            ret = DmsGetDeviceInfoExV2(dev_id, DSMI_MAIN_CMD_TS, sub_cmd, out_buf, size);
            if (ret != 0) {
                DEV_MON_EX_NOTSUPPORT_ERR(ret, "Get ts stream available failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
            }
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    return ret;
}

int dms_set_ts_info(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size)
{
    int ret;
    if (sub_cmd >= DSMI_TS_SUB_CMD_MAX) {
        DMS_ERR("Sub_cmd is invalid. (dev_id=%u; sub_cmd=%u)\n", dev_id, sub_cmd);
        return DRV_ERROR_INVALID_VALUE;
    }
    switch (sub_cmd) {
        case DSMI_TS_SUB_CMD_LAUNCH_AICORE_STL:
            ret = halTsdrvCtl(dev_id, TSDRV_CTL_CMD_LAUNCH_STL, buf, size, NULL, NULL);
            if (ret != 0) {
                DMS_ERR("Launch STL test fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
            }
            break;
#ifdef CFG_FEATURE_SET_STL_RUNNING_STATUS
        case DSMI_TS_SUB_CMD_START_PERIOD_AICORE_STL:
            ret = dms_set_stl_running_status(dev_id, sub_cmd, buf, size);
            if (ret != 0) {
                DMS_ERR("Start STL test fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
            }
            break;
        case DSMI_TS_SUB_CMD_STOP_PERIOD_AICORE_STL:
            ret = dms_set_stl_running_status(dev_id, sub_cmd, buf, size);
            if (ret != 0) {
                DMS_ERR("Stop STL test fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
            }
            break;
#endif
#ifdef CFG_FEATURE_AIC_AIV_CONTINUOUS_UTILIZATION
        case DSMI_TS_SUB_CMD_AIC_UTILIZATION_RATE_ASYN:
        case DSMI_TS_SUB_CMD_AIV_UTILIZATION_RATE_ASYN:
            ret = dms_set_core_utilization_asyn(dev_id, sub_cmd, buf, size);
            break;
#endif
        default:
            ret = DmsSetDeviceInfo(dev_id, DSMI_MAIN_CMD_TS, sub_cmd, buf, size);
            break;
    }
    return ret;
}

int dms_get_single_util_from_ts(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                                unsigned int *size)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    core_utilization_rate_t core_status = {0};
    unsigned char core_util[DMS_AI_OR_VECTOR_CORE_NUM_MAX] = {0};

    if (out_buf == NULL || size == NULL) {
        DMS_ERR("out_buf or size pointer is null. (out_buf=%d; size=%d)\n", (out_buf != NULL), (size != NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    core_status.dev_id = dev_id;
    core_status.vfid = vfid;
    core_status.core_utilization_rate = core_util;
    core_status.cmd_type = sub_cmd;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&core_status;
    ioarg.input_len = sizeof(core_status);
    ioarg.output = (void *)&core_status.core_num;
    ioarg.output_len = sizeof(core_status.core_num);
    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_AI_INFO_FROM_TS;
    ret = DmsIoctl(DMS_GET_AI_INFO_FROM_TS, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "IOCTL failed. (ret=%d)\n", ret);
        return errno_to_user_errno(ret);
    }
    /* length of actually read data from kernel ipc may exceed out_buf size */
    if (core_status.core_num > *size) {
        DMS_ERR("data length error. (core_num=%u; *size=%u; devid=%u)\n", core_status.core_num, *size, dev_id);
        return DRV_ERROR_INNER_ERR;
    }
    *size = core_status.core_num;
    ret = memcpy_s(out_buf, *size, core_util, core_status.core_num);
    if (ret != 0) {
        DMS_ERR("memcpy_s failed. (devid=%u; core_num=%u; ret=%d)\n", dev_id, core_status.core_num, ret);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    return DRV_ERROR_NONE;
}

static unsigned int dms_calculate_average_utilization(unsigned char *core_util, unsigned int core_num)
{
    unsigned int value = 0;
    unsigned int damaged_count = 0;
    unsigned int invalid_count = 0;
    unsigned int sum_utl = 0;
    unsigned int i;

    for (i = 0; i < core_num; i++) {
        if (core_util[i] == 0xEE) { /* 0xEE: means the damaged core */
            damaged_count++;
            continue;
        } else if (core_util[i] == 0xEF) { /* 0xEF: means conflict with profiling */
            value = 0xEF;
            return value;
        } else if (core_util[i] > 100) { /* 100: Utilization can never over 100% */
            invalid_count++;
            DMS_WARN("The utilization of core %u is more than 100%%.\n", i);
            continue;
        } else {
            sum_utl += core_util[i];
        }
    }

    if (core_num != (damaged_count + invalid_count)) {
        value = sum_utl / (core_num - damaged_count - invalid_count);
    } else {
        value = 0;
        DMS_EVENT("Average utilization is 0. (total_count=%u; invalid_count=%u; damaged_count=%u)\n", core_num,
                  invalid_count, damaged_count);
    }

    return value;
}

int dms_get_average_util_from_ts(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, unsigned int *value)
{
    int ret;
    unsigned char core_util[DMS_AI_OR_VECTOR_CORE_NUM_MAX] = {0};
    unsigned int core_num = DMS_AI_OR_VECTOR_CORE_NUM_MAX;
#ifdef CFG_FEATURE_VFIO_SOC /* If in 310Brc compute-group container, the logical id will range from 0 to 3. */
    unsigned int physical_id = 0;

    if ((drvDeviceGetPhyIdByIndex(dev_id, &physical_id) != DRV_ERROR_NONE) || (value == NULL)) {
        DMS_ERR("Failed to get physical_id. (dev_id=%u; value_is_null=%d)\n", dev_id, value == NULL);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (physical_id >= ASCEND_VDEV_ID_START) {
        *value = 0;
        return DRV_ERROR_NONE;
    }
#else
    if ((dev_id >= DEVDRV_MANGER_MAX_DEVICE_NUM) || (value == NULL)) {
        DMS_ERR("Invalid dev_id or value. (devid=%u; max=%u; value_is_NULL=%d)\n", dev_id, DEVDRV_MANGER_MAX_DEVICE_NUM,
                (value == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }
#endif

    ret = dms_get_single_util_from_ts(dev_id, vfid, sub_cmd, core_util, &core_num);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get average utilization failed. (sub_cmd=%u; ret=%d)\n", sub_cmd, ret);
        return ret;
    }

    *value = dms_calculate_average_utilization(core_util, core_num);
    return DRV_ERROR_NONE;
}

int DmsGetTsInfo(unsigned int dev_id, unsigned int vfid, unsigned int core_id, void *result, unsigned int result_size)
{
    int ret;
    struct dms_ts_info_in in = {0};
    struct dms_ioctl_arg ioarg = {0};

    if ((dev_id >= DEVDRV_MANGER_MAX_DEVICE_NUM) || (result == NULL)) {
        DMS_ERR("Invalid dev_id or value. (devid=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    in.dev_id = dev_id;
    in.vfid = vfid;
    in.core_id = core_id;

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_AI_INFO_FROM_TS;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&in;
    ioarg.input_len = sizeof(struct dms_ts_info_in);
    ioarg.output = result;
    ioarg.output_len = result_size;

    ret = errno_to_user_errno(DmsIoctl(DMS_GET_AI_INFO_FROM_TS, &ioarg));
    if (ret == DRV_ERROR_NOT_SUPPORT) {
        return ret;
    } else if (ret != 0) {
        DMS_ERR("Dms get device ts info failed. (device_id=%u; ret=%d)\n", in.dev_id, ret);
        return ret;
    }
    DMS_DEBUG("Dms get device ts info success. (dev_id=%u; core_id=%u)\n", in.dev_id, in.core_id);
    return DRV_ERROR_NONE;
}

drvError_t DmsDeviceInitStatus(unsigned int dev_id, unsigned int *status)
{
    struct dms_ioctl_arg ioarg = {0};
    unsigned int dev_init_status;
    int ret;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (status == NULL)) {
        DMS_ERR("Invalid dev_id or value. (devid=%u; max=%u; status_is_NULL=%d)\n", dev_id, ASCEND_DEV_MAX_NUM,
                (status == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_DEV_INIT_STATUS;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = &dev_init_status;
    ioarg.output_len = sizeof(unsigned int);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get device init status failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return errno_to_user_errno(ret);
    }

    *status = dev_init_status;

    return DRV_ERROR_NONE;
}

drvError_t DmsTsHeartbeatStatus(unsigned int dev_id, unsigned int vf_id, unsigned int ts_id, unsigned int *status)
{
    struct dms_ioctl_arg ioarg = {0};
    struct dms_ts_hb_status_in ts_hb_inst = {0};
    unsigned int ts_hb_status;
    int ret;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (vf_id >= VDAVINCI_MAX_VFID_NUM) || (status == NULL)) {
        DMS_ERR("Invalid parameter. (dev_id=%u; vf_id=%u; ts_id=%u)", dev_id, vf_id, ts_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ts_hb_inst.dev_id = dev_id;
    ts_hb_inst.vf_id = vf_id;
    ts_hb_inst.ts_id = ts_id;

    ioarg.main_cmd = DMS_MAIN_CMD_TRS;
    ioarg.sub_cmd = DMS_SUBCMD_GET_TS_HB_STATUS;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&ts_hb_inst;
    ioarg.input_len = sizeof(struct dms_ts_hb_status_in);
    ioarg.output = &ts_hb_status;
    ioarg.output_len = sizeof(unsigned int);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get ts hb status failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return errno_to_user_errno(ret);
    }

    *status = ts_hb_status;

    return DRV_ERROR_NONE;
}
