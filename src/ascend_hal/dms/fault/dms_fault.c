/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <time.h>
#include <sys/ioctl.h>

#include "securec.h"
#include "mmpa_api.h"
#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "dms_user_common.h"
#include "dms/dms_misc_interface.h"
#include "dsmi_common_interface.h"
#include "devdrv_user_common.h"
#include "pbl_uda_user.h"
#include "dms_fault.h"
#include "ascend_dev_num.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define INVALID_NODE_TYPE (0xFFFFFFFFU)
#define SENSOR_NODE_CFG_MAX_NAME_LEN 20
#define DMS_FAULT_DEVICE_ALL (-1)
#define DMS_READ_FAULT_NO_TIMEOUT_FLAG (-1)
/* dms sensor ioctl args */
struct dms_sensor_user {
    unsigned int dev_id;
    struct halSensorNodeCfg cfg;
    uint64_t handle;
    int value;
    int assertion;
};

#ifdef CFG_FEATURE_FAULT_EVENT
static inline unsigned long long GetSysCnt(void)
{
    unsigned long long syscnt = 0;
    asm volatile("mrs %0, CNTVCT_EL0" : "=r"(syscnt)::"memory");
    return syscnt;
}
#endif

STATIC drvError_t DmsGetEventPara(int dev_id, struct dms_event_filter *filter, int timeout, enum cmd_source cmd_src,
                                  struct dms_event_para *event_para)
{
    struct dms_ioctl_arg ioarg = {0};
    struct dms_read_event_ioctl input = {0};
    struct dms_event_filter event_filter = {0};
    int ret;

    if (filter != NULL) {
        event_filter = *filter;
    } else {
        return DRV_ERROR_PARA_ERROR;
    }
    (void)memset_s(event_para, sizeof(struct dms_event_para), 0, sizeof(struct dms_event_para));
    input.timeout = timeout;
    input.cmd_src = cmd_src;
    input.dev_id = dev_id;
    input.filter = event_filter;
    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_FAULT_EVENT;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&input;
    ioarg.input_len = (unsigned int)sizeof(struct dms_read_event_ioctl);
    ioarg.output = (void *)event_para;
    ioarg.output_len = (unsigned int)sizeof(struct dms_event_para);

    ret = (int)DmsIoctl((int)DMS_GET_FAULT_EVENT, &ioarg);
    if (ret != 0) {
        if ((ret == ETIMEDOUT) || (ret == ERESTARTSYS)) {
            return ret;
        }
        DMS_EX_NOTSUPPORT_ERR(ret, "Get fault event failed. (ret=%d)\n", ret);
        return ret;
    }

#ifdef CFG_FEATURE_FAULT_EVENT
    DMS_WARN("Get fault event success. (syscnt=%llu)\n", GetSysCnt());
#else
    DMS_DEBUG("Get fault event success.\n");
#endif
    return DRV_ERROR_NONE;
}

static drvError_t dms_event_para_to_dms_event(struct dms_event_para *event_para, struct dms_fault_event *event)
{
    u32 phyid, locid;
    int ret;
    phyid = (u32)event_para->deviceid;

    ret = drvDeviceGetIndexByPhyId(phyid, &locid);
    if (ret != 0) {
        DMS_ERR("Transform phyId to locId failed. (phy_id=%u)\n", phyid);
        return DRV_ERROR_INNER_ERR;
    }

    event->deviceid = (unsigned short)locid;
    event->event_id = event_para->event_id;
    event->node_type = (unsigned char)event_para->node_type;
    event->node_id = event_para->node_id;
    event->sub_node_type = (unsigned char)event_para->sub_node_type;
    event->sub_node_id = event_para->sub_node_id;
    event->severity = event_para->severity;
    event->assertion = event_para->assertion;
    event->event_serial_num = event_para->event_serial_num;
    event->notify_serial_num = event_para->notify_serial_num;
    event->alarm_raised_time = event_para->alarm_raised_time;
    event->node_type_ex = event_para->node_type;
    event->sub_node_type_ex = event_para->sub_node_type;
    if (sprintf_s(event->event_name, DMS_MAX_EVENT_NAME_LENGTH, "deviceid=%u, %s", locid, event_para->event_name) < 0) {
        DMS_ERR("sprintf_s event_name failed.\n");
        return DRV_ERROR_INNER_ERR;
    }
    ret = memcpy_s(event->additional_info, DMS_MAX_EVENT_DATA_LENGTH, event_para->additional_info,
                   DMS_MAX_EVENT_DATA_LENGTH);
    if (ret != 0) {
        DMS_ERR("memcpy_s additional_Info failed.\n");
        return DRV_ERROR_INNER_ERR;
    }
    event->os_id = 0;
    return DRV_ERROR_NONE;
}

drvError_t DmsGetFaultEvent(int dev_id, struct dms_event_filter *filter, int timeout, struct dms_fault_event *event)
{
    struct dms_event_para event_para = {0};
    int ret;
    bool dev_exist = false;

    if (event == NULL) {
        DMS_ERR("Invalid parameter, event=NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = (int)DmsGetEventPara(dev_id, filter, timeout, FROM_DSMI, &event_para);
    if (ret == ETIMEDOUT) {
        return DRV_ERROR_WAIT_TIMEOUT;
    } else if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {
        DMS_ERR("has no pid resource. (ret=%d)\n", ret);
        return (drvError_t)ret;
    } else if (ret == (int)DRV_ERROR_NOT_SUPPORT) {
        return (drvError_t)ret;
    } else if (ret == ERESTARTSYS) {
        return DRV_ERROR_NO_EVENT;
    } else if (ret != 0) {
        DMS_ERR("Dms get event parameter failed. (ret=%d)\n", ret);
        return (drvError_t)ret;
    } else {
    }
    DMS_DEBUG("Dms get event parameter success. (dev_id=%u; event_id=0x%x; assertion=%u)\n", event_para.deviceid,
              event_para.event_id, event_para.assertion);

    ret = uda_dev_is_exist(event_para.deviceid, &dev_exist);
    if (ret != 0) {
        return ret;
    }

    if (!dev_exist) {
        return DRV_ERROR_NO_EVENT;
    }

    ret = (int)dms_event_para_to_dms_event(&event_para, event);
    if (ret != 0) {
        DMS_ERR("Struct EventPara to DmsEvent failed. (dev_id=%u; event_id=0x%x; assertion=%u)\n", event_para.deviceid,
                event_para.event_id, event_para.assertion);
        return ret;
    }
#if defined(CFG_FEATURE_DRV_EVENT_LOG)
    FAULT_DMS_EVENT("eventName=0x%x; eventId=0x%x; deviceId=%u; nodeId=%u; assertion=%u; osId=%u; severity=%u;"
                    " eventSerialNum=%d; notifySerialNum=%d; eventArisedTime=%llu ms; subNodeId=%u.\n",
                    event->node_type, event->event_id, event->deviceid, event->node_id, event->assertion, event->os_id,
                    event->severity, event->event_serial_num, event->notify_serial_num, event->alarm_raised_time,
                    event->sub_node_id);
#endif
    return DRV_ERROR_NONE;
}

drvError_t halDmsGetFaultEvent(int dev_id, void *filter, int timeout, void *event)
{
    return DmsGetFaultEvent(dev_id, (struct dms_event_filter *)filter, timeout, (struct dms_fault_event *)event);
}

static drvError_t dms_get_history_event_para(int device_id, struct dms_event_para *event_para_buf, int event_buf_size)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;
    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_HISTORY_FAULT_EVENT;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&device_id;
    ioarg.input_len = sizeof(int);
    ioarg.output = (void *)event_para_buf;
    ioarg.output_len = (unsigned int)(sizeof(struct dms_event_para) * (long unsigned int)event_buf_size);
    ret = DmsIoctl((int)DMS_GET_HISTORY_FAULT_EVENT, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "DmsIoctl failed. (ret=%d)\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    return DRV_ERROR_NONE;
}

drvError_t DmsGetHistoryFaultEvent(int device_id, struct dsmi_event *event_buf, int event_buf_size, int *event_cnt)
{
    struct dms_event_para *event_para_buf;
    int ret;
    int i;

    if ((event_buf_size <= 0) || (event_buf == NULL) || (event_cnt == NULL)) {
        DMS_ERR("Invalid parameter.(event_buf_size=%d; event_buf_is_null=%d; event_cnt_is_null=%d)\n", event_buf_size,
                (event_buf == NULL), (event_cnt == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    event_para_buf = (struct dms_event_para *)malloc(sizeof(struct dms_event_para) * (long unsigned int)event_buf_size);
    if (event_para_buf == NULL) {
        DMS_ERR("malloc struct dms_event_para failed. \n");
        return DRV_ERROR_MALLOC_FAIL;
    }
    (void)memset_s(event_para_buf, sizeof(struct dms_event_para) * (long unsigned int)event_buf_size, 0,
                   (sizeof(struct dms_event_para) * (long unsigned int)event_buf_size));
    // get history fault events
    ret = dms_get_history_event_para(device_id, event_para_buf, event_buf_size);
    if (ret != DRV_ERROR_NONE) {
        goto out;
    }
    //  trans dms_event_para to dms_fault_event
    for (i = 0; i < event_buf_size; ++i) {
        // if event_id is invalid  value, it mean not  fault event in list
        if (event_para_buf[i].event_id == 0xFFFFFFFFU) {
            *event_cnt = i;
            ret = DRV_ERROR_NONE;
            DMS_DEBUG("The event_id is invalid, so the valid history fault event number is %d .\n", i);
            goto out;
        }
        ret = dms_event_para_to_dms_event(&event_para_buf[i], &event_buf[i].event_t.dms_event);
        if (ret != DRV_ERROR_NONE) {
            DMS_ERR("Struct EventPara to DmsEvent failed. (dev_id=%u; event_id=0x%x; assertion=%u)\n",
                    event_para_buf[i].deviceid, event_para_buf[i].event_id, event_para_buf[i].assertion);
            *event_cnt = 0;
            goto out;
        }
        event_buf[i].type = DMS_FAULT_EVENT;
    }
    *event_cnt = i;
out:
    free(event_para_buf);
    return ret;
}

drvError_t halDmsGetHistoryFaultEvent(int device_id, void *event_buf, int event_buf_size, int *event_cnt)
{
    return DmsGetHistoryFaultEvent(device_id, (struct dsmi_event *)event_buf, event_buf_size, event_cnt);
}

drvError_t dms_clear_fault_event(u32 devid)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_CLEAR_EVENT;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&devid;
    ioarg.input_len = sizeof(u32);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_CLEAR_EVENT, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Clear fault event failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }
    DMS_EVENT("Clear fault event success. (devid=%u)\n", devid);

    return DRV_ERROR_NONE;
}

drvError_t DmsGetHealthCode(u32 devid, u32 *phealth)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    if (phealth == NULL) {
        DMS_ERR("Invalid parameter, phealth=NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_HEALTH_CODE;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&devid;
    ioarg.input_len = sizeof(u32);
    ioarg.output = (void *)phealth;
    ioarg.output_len = sizeof(u32);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret == DRV_ERROR_NOT_SUPPORT) {
        return DRV_ERROR_NOT_SUPPORT;
    } else if (ret != 0) {
        DMS_ERR("Get health code failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }
    DMS_DEBUG("Get health code success. (devid=%u)\n", devid);

    return DRV_ERROR_NONE;
}

drvError_t dms_get_device_state(u32 devid, char *state_out, unsigned int out_len)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_DEVICE_STATE;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = NULL;
    ioarg.input_len = 0;
    ioarg.output = (void *)state_out;
    ioarg.output_len = out_len;
    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret == DRV_ERROR_NOT_SUPPORT) {
        return DRV_ERROR_NOT_SUPPORT;
    } else if (ret != 0) {
        DMS_ERR("Get device state failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

drvError_t DmsGetEventCode(u32 devid, int *event_cnt, u32 *event_code, u32 code_len)
{
    struct devdrv_error_code_para event_para = {0};
    struct dms_ioctl_arg ioarg = {0};
    int ret, i;

    if ((event_cnt == NULL) || (event_code == NULL) || (code_len < DMANAGE_ERROR_ARRAY_NUM)) {
        DMS_ERR("Invalid parameter. (event_cnt=%s; event_code=%s; code_len=%u)\n", (event_cnt == NULL) ? "NULL" : "OK",
                (event_code == NULL) ? "NULL" : "OK", code_len);
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_EVENT_CODE;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&devid;
    ioarg.input_len = sizeof(u32);
    ioarg.output = (void *)&event_para;
    ioarg.output_len = sizeof(struct devdrv_error_code_para);

    ret = DmsIoctl(DMS_GET_EVENT_CODE, &ioarg);
    if (ret == DRV_ERROR_NOT_SUPPORT) {
        return DRV_ERROR_NOT_SUPPORT;
    } else if (ret != 0) {
        DMS_ERR("Get event code failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    if (event_para.error_code_count > DMANAGE_ERROR_ARRAY_NUM) {
        DMS_ERR("Too many event code. (devid=%u; cnt=%d)\n", devid, event_para.error_code_count);
        return DRV_ERROR_INNER_ERR;
    }

    *event_cnt = event_para.error_code_count;
    for (i = 0; i < event_para.error_code_count; i++) {
        event_code[i] = event_para.error_code[i];
    }
    DMS_DEBUG("Get event code success. (devid=%u; cnt=%d)\n", devid, *event_cnt);

    return DRV_ERROR_NONE;
}

drvError_t DmsQueryEventStr(u32 devid, u32 event_code, unsigned char *str, int str_size)
{
    struct dms_ioctl_arg ioarg = {0};
    struct dms_event_ioctrl para = {0};
    int ret;

    if ((str == NULL) || (str_size <= 0)) {
        DMS_ERR("Invalid parameter. (dev_id=%u; event_code=0x%x; str=NULL; str_size=%d)\n", devid, event_code,
                str_size);
        return DRV_ERROR_PARA_ERROR;
    }

    para.devid = devid;
    para.event_code = event_code;

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_QUERY_STR;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&para;
    ioarg.input_len = sizeof(struct dms_event_ioctrl);
    ioarg.output = (void *)str;
    ioarg.output_len = (u32)str_size;

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret == DRV_ERROR_NOT_SUPPORT) {
        return DRV_ERROR_NOT_SUPPORT;
    } else if (ret == DRV_ERROR_NO_EVENT) {
        DMS_WARN("The event code is bbox code. (devi_id=%u; event_code=0x%x)\n", devid, event_code);
        return DRV_ERROR_NO_EVENT;
    } else if (ret != 0) {
        DMS_ERR("Query event string failed. (devid=%u; event_code=0x%x; ret=%d)\n", devid, event_code, ret);
        return ret;
    }

    DMS_DEBUG("Query event string success. (devid=%u; event_code=0x%x; str=\"%.*s\")\n", devid, event_code, str_size,
              str);

    return DRV_ERROR_NONE;
}

drvError_t dms_disable_fault_event(u32 devid, u32 event_code)
{
    struct dms_ioctl_arg ioarg = {0};
    struct dms_event_ioctrl para = {0};
    int ret;

    para.devid = devid;
    para.event_code = event_code;

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_DISABLE_EVENT;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&para;
    ioarg.input_len = sizeof(struct dms_event_ioctrl);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_DISABLE_EVENT, &ioarg);
    if (ret == DRV_ERROR_NOT_SUPPORT) {
        return DRV_ERROR_NOT_SUPPORT;
    } else if (ret == DRV_ERROR_NO_EVENT) {
        DMS_WARN("Disable event code is invalid. (devi_id=%u; event_code=0x%x)\n", devid, event_code);
        return DRV_ERROR_NO_EVENT;
    } else if (ret != 0) {
        DMS_ERR("Disable event code failed. (devid=%u; event_code=0x%x; ret=%d)\n", devid, event_code, ret);
        return ret;
    }

    DMS_DEBUG("Disable event code success. (devid=%u; event_code=0x%x)\n", devid, event_code);
    return DRV_ERROR_NONE;
}

drvError_t dms_enable_fault_event(u32 devid, u32 event_code)
{
    struct dms_ioctl_arg ioarg = {0};
    struct dms_event_ioctrl para = {0};
    int ret;

    para.devid = devid;
    para.event_code = event_code;

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_ENABLE_EVENT;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&para;
    ioarg.input_len = sizeof(struct dms_event_ioctrl);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_ENABLE_EVENT, &ioarg);
    if (ret == DRV_ERROR_NOT_SUPPORT) {
        return DRV_ERROR_NOT_SUPPORT;
    } else if (ret == DRV_ERROR_NO_EVENT) {
        DMS_WARN("Enable event code is invalid. (devid=%u; event_code=0x%x)\n", devid, event_code);
        return DRV_ERROR_NO_EVENT;
    } else if (ret != 0) {
        DMS_ERR("Enable event code failed. (devid=%u; event_code=0x%x; ret=%d)\n", devid, event_code, ret);
        return ret;
    }

    DMS_DEBUG("Enable event code success. (devid=%u; event_code=0x%x)\n", devid, event_code);
    return DRV_ERROR_NONE;
}

drvError_t dms_inject_fault(DSMI_FAULT_INJECT_INFO *info)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};

    if (info == NULL) {
        DMS_ERR("Invalid parameter. (info=%s)\n", (info == NULL) ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_INJECT_FAULT;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)info;
    ioarg.input_len = (unsigned int)sizeof(DSMI_FAULT_INJECT_INFO);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_ERR("Dms inject fault failed. (return_value=%d)\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    return DRV_ERROR_NONE;
}

drvError_t DmsGetFaultInjectInfo(unsigned int device_id, const unsigned int max_info_cnt, void *info_buf,
                                 unsigned int *real_info_cnt)
{
    int ret;
    unsigned int index;
    struct dms_ioctl_arg ioarg = {0};
    DSMI_FAULT_INJECT_INFO *info_arr;

    /* 1 check parameters */
    if ((info_buf == NULL) || (real_info_cnt == NULL)) {
        DMS_ERR("Invalid parameter. (buffer=%d; real_count_pointer=%d)\n", (info_buf != NULL), (real_info_cnt != NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    /* 2 get fault info from drivers */
    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_FAULT_INJECT_INFO;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&device_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = info_buf;
    ioarg.output_len = (unsigned int)(sizeof(DSMI_FAULT_INJECT_INFO) * max_info_cnt);
    ret = DmsIoctl(DMS_GET_FAULT_INJECT_INFO, &ioarg);
    if (ret != 0) {
        DMS_ERR("DmsIoctl failed. (return=%d)\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    /* 3 compute valid info number */
    *real_info_cnt = 0;
    info_arr = (DSMI_FAULT_INJECT_INFO *)info_buf;
    for (index = 0; index < max_info_cnt; index++) {
        if (info_arr[index].node_type == INVALID_NODE_TYPE) {
            break;
        } else {
            (*real_info_cnt)++;
        }
    }
    return DRV_ERROR_NONE;
}

drvError_t halSensorNodeRegister(uint32_t devId, struct halSensorNodeCfg *cfg, uint64_t *handle)
{
    int ret;
    struct dms_sensor_user para = {0};
    struct dms_ioctl_arg ioarg = {0};

    if ((cfg == NULL) || (handle == NULL)) {
        DMS_ERR("Invalid parameter. (cfg NULL=%d; handle NULL=%d)\n", (cfg == NULL), (handle == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    para.dev_id = devId;
    para.cfg = *cfg;

    ioarg.main_cmd = DMS_MAIN_CMD_SOFT_FAULT;
    ioarg.sub_cmd = DMS_SUBCMD_SENSOR_NODE_REGISTER;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&para;
    ioarg.input_len = sizeof(struct dms_sensor_user);
    ioarg.output = (void *)handle;
    ioarg.output_len = sizeof(uint64_t);

    ret = DmsIoctl(DMS_SENSOR_NODE_REGISTER, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Register sensor failed. (devId=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    DMS_EVENT("Sensor node register success. (devid=%u; name=%.*s; NodeType=%u; SensorType=%u; AssertEventMask=0x%x; "
              "DeassertEventMask=0x%x)\n",
              devId, SENSOR_NODE_CFG_MAX_NAME_LEN, cfg->name, cfg->NodeType, cfg->SensorType, cfg->AssertEventMask,
              cfg->DeassertEventMask);
    return DRV_ERROR_NONE;
}

drvError_t halSensorNodeUnregister(uint32_t devId, uint64_t handle)
{
    int ret;
    struct dms_sensor_user para = {0};
    struct dms_ioctl_arg ioarg = {0};

    para.dev_id = devId;
    para.handle = handle;

    ioarg.main_cmd = DMS_MAIN_CMD_SOFT_FAULT;
    ioarg.sub_cmd = DMS_SUBCMD_SENSOR_NODE_UNREGISTER;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&para;
    ioarg.input_len = sizeof(struct dms_sensor_user);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_SENSOR_NODE_UNREGISTER, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Unregister sensor failed. (devId=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    DMS_EVENT("Sensor node unregister success. (devid=%u)\n", devId);
    return DRV_ERROR_NONE;
}

drvError_t halSensorNodeUpdateState(uint32_t devId, uint64_t handle, int val, halGeneralEventType_t assertion)
{
    int ret;
    struct dms_sensor_user para = {0};
    struct dms_ioctl_arg ioarg = {0};

    para.dev_id = devId;
    para.handle = handle;
    para.value = val;
    para.assertion = assertion;

    ioarg.main_cmd = DMS_MAIN_CMD_SOFT_FAULT;
    ioarg.sub_cmd = DMS_SUBCMD_SENSOR_NODE_UPDATE_VAL;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = (void *)&para;
    ioarg.input_len = sizeof(struct dms_sensor_user);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_SENSOR_NODE_UPDATE_VAL, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Update sensor state failed. (devId=%u; ret=%d)\n", devId, ret);
        return ret;
    }

    DMS_DEBUG("Sensor node update state success. (devid=%u; val=%d)\n", devId, val);
    return DRV_ERROR_NONE;
}

#if defined(CFG_EDGE_HOST) || defined(CFG_FEATURE_GET_CURRENT_EVENTINFO)

STATIC drvError_t dms_event_para_to_event_info(struct dms_event_para *event_para, struct halFaultEventInfo *event)
{
    int ret;
    u32 phyid, locid;
    phyid = (u32)event_para->deviceid;

    ret = drvDeviceGetIndexByPhyId(phyid, &locid);
    if (ret != 0) {
        DMS_ERR("Transform phyId to locId failed. (phy_id=%u)\n", phyid);
        return DRV_ERROR_INNER_ERR;
    }

    event->event_id = event_para->event_id;
    event->deviceid = (unsigned short)locid;
    event->node_type = event_para->node_type;
    event->node_id = event_para->node_id;
    event->sub_node_type = event_para->sub_node_type;
    event->sub_node_id = event_para->sub_node_id;
    event->severity = event_para->severity;
    event->assertion = event_para->assertion;
    event->event_serial_num = event_para->event_serial_num;
    event->notify_serial_num = event_para->notify_serial_num;
    event->alarm_raised_time = event_para->alarm_raised_time;
    if (sprintf_s(event->event_name, DMS_MAX_EVENT_NAME_LENGTH, "deviceid=%u, %s", locid, event_para->event_name) < 0) {
        DMS_ERR("sprintf_s event_name failed.\n");
        return DRV_ERROR_INNER_ERR;
    }
    ret = memcpy_s(event->additional_info, DMS_MAX_EVENT_DATA_LENGTH, event_para->additional_info,
                   DMS_MAX_EVENT_DATA_LENGTH);
    if (ret != 0) {
        DMS_ERR("memcpy_s additional_Info failed.\n");
        return DRV_ERROR_INNER_ERR;
    }
    event->tgid = *(int *)(event_para->event_info);
    event->os_id = 0;
    return DRV_ERROR_NONE;
}
#endif

#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
STATIC int dms_fill_event(struct devdrv_event_obj_para *event_para, struct halFaultEventInfo *event_buff,
                          uint32_t *event_count)
{
    uint32_t event_num = 0;
    unsigned int i;
    bool dev_exist;
    int ret;

    for (i = 0; i < event_para->event_count; i++) {
        ret = uda_dev_is_exist(event_para->dms_event[i].deviceid, &dev_exist);
        if (ret != 0 || !dev_exist) {
            continue;
        }
        ret = dms_event_para_to_event_info(&event_para->dms_event[i], &event_buff[event_num]);
        if (ret != 0) {
            DMS_ERR("Struct EventPara to DmsEvent failed. (dev_id=%u; event_id=0x%x; assertion=%u)\n",
                    event_para->dms_event[i].deviceid, event_para->dms_event[i].event_id,
                    event_para->dms_event[i].assertion);
            return ret;
        }
        event_num++;
    }

    *event_count = event_num;
    return DRV_ERROR_NONE;
}
#endif

struct hal_fault_event_infoes {
    struct halFaultEventInfo *event_info;
    uint32_t len;
};

#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
STATIC drvError_t DmsGetHistoryEventFromKernel(struct dms_ioctl_arg *ioarg, struct halFaultEventInfo *event_buff,
                                               struct hal_fault_event_infoes *event_infoes, uint32_t *event_count)
{
    int ret;
    uint32_t i;
    uint32_t devId = *((uint32_t *)ioarg->input);
    struct devdrv_event_obj_para *event_para = (struct devdrv_event_obj_para *)ioarg->output;
    uint32_t event_num = 0;

    ret = DmsIoctl(DMS_IOCTL_CMD, ioarg);
    if (ret == DRV_ERROR_NOT_SUPPORT) {
        goto FREE_EVENT_BUFF;
    } else if (ret == DRV_ERROR_NO_EVENT) {
        *event_count = 0;
        ret = DRV_ERROR_NONE;
        goto FREE_EVENT_BUFF;
    } else if (ret != 0) {
        DMS_ERR("Get fault event failed. (dev_id=%u; ret=%d)\n", devId, ret);
        goto FREE_EVENT_BUFF;
    }

    if (event_para->event_count > DMANAGE_ERROR_ARRAY_NUM) {
        DMS_WARN("Event count exceeds array limit. (event_count=%u; max=%d)\n", event_para->event_count,
                 DMANAGE_ERROR_ARRAY_NUM);
        event_para->event_count = DMANAGE_ERROR_ARRAY_NUM;
    }

    if (event_para->event_count > event_infoes->len) {
        event_para->event_count = event_infoes->len < DMANAGE_ERROR_ARRAY_NUM ? event_infoes->len :
                                                                                DMANAGE_ERROR_ARRAY_NUM;
    }

    ret = dms_fill_event(event_para, event_buff, &event_num);
    if (ret != 0) {
        DMS_ERR("Fill fault event failed. (dev_id=%u; ret=%d)\n", devId, ret);
        goto FREE_EVENT_BUFF;
    }

    for (i = 0; i < event_num; i++) {
        if (memcpy_s(&event_infoes->event_info[i], sizeof(struct halFaultEventInfo), &event_buff[i],
                     sizeof(struct halFaultEventInfo)) != 0) {
            DMS_ERR("memcpy_s failed.\n");
            ret = DRV_ERROR_INNER_ERR;
            goto FREE_EVENT_BUFF;
        }
    }

    *event_count = event_num;
    ret = DRV_ERROR_NONE;

FREE_EVENT_BUFF:
    free(event_buff);
    event_buff = NULL;
    free(event_para);
    event_para = NULL;
    return ret;
}
#endif

STATIC drvError_t DmsGetHistoryEvent(uint32_t devId, unsigned int dms_sub_cmd, struct halEventFilter *filter,
                                     struct hal_fault_event_infoes *event_infoes, uint32_t *event_count)
{
#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
    struct halFaultEventInfo *event_buff = NULL;
    struct devdrv_event_obj_para *event_para = NULL;
    struct dms_get_event_ioctl get_event_input = {0};
    struct dms_ioctl_arg ioarg = {0};

    if (filter == NULL || event_infoes == NULL || event_infoes->event_info == NULL || event_count == NULL ||
        devId >= ASCEND_PDEV_MAX_NUM) {
        DMS_ERR("Invalid parameter. (filter=%s; event_infoes=%s; eventInfo=%s; event_count=%s; devId=%u)\n",
                (filter == NULL) ? "NULL" : "OK", (event_infoes == NULL) ? "NULL" : "OK",
                (event_infoes == NULL || event_infoes->event_info == NULL) ? "NULL" : "OK",
                (event_count == NULL) ? "NULL" : "OK", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    event_para = (struct devdrv_event_obj_para *)calloc(1, sizeof(struct devdrv_event_obj_para));
    if (event_para == NULL) {
        DMS_ERR("Failed to alloc memory for event_para. (devid=%u)\n", devId);
        return DRV_ERROR_MALLOC_FAIL;
    }

    event_buff = (struct halFaultEventInfo *)calloc(DMANAGE_ERROR_ARRAY_NUM, sizeof(struct halFaultEventInfo));
    if (event_buff == NULL) {
        DMS_ERR("Failed to alloc memory for event_buff. (devid=%u)\n", devId);
        free(event_para);
        event_para = NULL;
        return DRV_ERROR_MALLOC_FAIL;
    }

    get_event_input.dev_id = devId;
    get_event_input.filter = *(struct dms_event_filter *)filter;

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = dms_sub_cmd;
    ioarg.filter_len = 0;
    ioarg.filter = NULL;
    ioarg.input = &get_event_input;
    ioarg.input_len = sizeof(struct dms_get_event_ioctl);
    ioarg.output = event_para;
    ioarg.output_len = sizeof(struct devdrv_event_obj_para);

    return DmsGetHistoryEventFromKernel(&ioarg, event_buff, event_infoes, event_count);
#else
    (void)devId;
    (void)dms_sub_cmd;
    (void)filter;
    (void)event_infoes;
    (void)event_count;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t halGetFaultEvent(uint32_t devId, struct halEventFilter *filter, struct halFaultEventInfo *eventInfo,
                            uint32_t len, uint32_t *eventCount)
{
    struct hal_fault_event_infoes event_infoes = {eventInfo, len};

    return DmsGetHistoryEvent(devId, DMS_SUBCMD_GET_FAULT_EVENT_OBJ, filter, &event_infoes, eventCount);
}

drvError_t halGetNotifyEvent(uint32_t devId, struct halEventFilter *filter, struct halFaultEventInfo *eventInfo,
                             uint32_t len, uint32_t *eventCount)
{
    struct hal_fault_event_infoes event_infoes = {eventInfo, len};

    return DmsGetHistoryEvent(devId, DMS_SUBCMD_GET_NOTIFY_EVENT_OBJ, filter, &event_infoes, eventCount);
}

#ifdef CFG_EDGE_HOST
static inline long get_current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    /* 1000 ms per second, 1000000ns per ms */
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int dms_wait_fault_event(int32_t dev_id, int timeout, struct halEventFilter *filter,
                                struct halFaultEventInfo *event_info)
{
    struct dms_event_para event_para = {0};
    struct halFaultEventInfo event_tmp = {0};
    int ret;

    ret = DmsGetEventPara(dev_id, (struct dms_event_filter *)filter, timeout, FROM_HAL, &event_para);
    if ((ret == ETIMEDOUT) || (ret == ERESTARTSYS)) {
        return DRV_ERROR_NO_EVENT;
    } else if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {
        DMS_ERR("has no pid resource. (dev_id=%d; ret=%d)\n", dev_id, ret);
        return ret;
    } else if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get event parameter failed. (dev_id=%d; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = dms_event_para_to_event_info(&event_para, &event_tmp);
    if (ret != 0) {
        DMS_ERR("Struct EventPara to DmsEvent failed. (dev_id=%u; event_id=0x%x; assertion=%u)\n", event_para.deviceid,
                event_para.event_id, event_para.assertion);
        return ret;
    }

    *event_info = event_tmp;
    return 0;
}
#endif

drvError_t halReadFaultEvent(int32_t devId, int timeout, struct halEventFilter *filter,
                             struct halFaultEventInfo *eventInfo)
{
#ifdef CFG_EDGE_HOST
    struct halFaultEventInfo event_tmp = {0};
    long time_last, time_current, times_gap;
    int time_out = timeout;
    int ret;

    if (filter == NULL || eventInfo == NULL) {
        DMS_ERR("Invalid input. (dev_id=%d; filter=%d; eventInfo=%d)\n", devId, (filter != NULL), (eventInfo != NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    time_last = get_current_time_ms();
    do {
        ret = dms_wait_fault_event(devId, time_out, filter, &event_tmp);
        if ((ret == 0) || (ret != DRV_ERROR_NO_EVENT)) {
            break;
        }
        time_current = get_current_time_ms();
        if (time_current < time_last) {
#ifndef DMS_UT
            DMS_WARN("Invalid time. (dev_id=%d; time_current=%ld; time_last=%ld ms)\n", devId, time_current, time_last);
#endif
            break;
        }
        if (time_out > 0) {
            times_gap = time_current - time_last;
            time_last = time_current;
            time_out = (int)(time_out > times_gap ? (time_out - times_gap) : 0);
        }
    } while ((time_out > 0) || (time_out == DMS_READ_FAULT_NO_TIMEOUT_FLAG));

    if (ret == 0) {
        if (memcpy_s(eventInfo, sizeof(struct halFaultEventInfo), &event_tmp, sizeof(struct halFaultEventInfo)) != 0) {
            DMS_ERR("memcpy_s failed. (dev_id=%u)\n", devId);
            return DRV_ERROR_INNER_ERR;
        }
    }

    return ret;
#else
    (void)devId;
    (void)timeout;
    (void)filter;
    (void)eventInfo;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

struct hal_fault_event_thread {
    int device_id;
    bool run_flg;
    int result;
    struct halEventFilter filter;
    halfaulteventcb handler;
    pthread_mutex_t thread_lock;
    pthread_t thread_id;
};
#ifndef DRV_HOST
#define HAL_FAULT_EVT_THREAD_STACK_SIZE (128 * 1024)
#endif
#define HAL_FAULT_THREAD_SLEEP (20 * 1000) /* sleep 20 ms */
#define HAL_FAULT_THREAD_INIT_VAL (-1)
#define HAL_FAULT_EVENT_THREAD_PRIORITY (10)
#define HAL_READ_FAULT_NO_TIMEOUT_FLAG (-1)
#define HAL_EVENT_WAIT_TIME_ZERO 0 /* return immediately no block */

STATIC struct hal_fault_event_thread g_fault_thread_para = {
    .device_id = -1,
    .run_flg = false,
    .result = HAL_FAULT_THREAD_INIT_VAL,
    .filter = {0},
    .handler = NULL,
    .thread_lock = PTHREAD_MUTEX_INITIALIZER,
    .thread_id = 0,
};

typedef enum {
    THREAD_RUNING,
    THREAD_STOPING,
    THREAD_STOPPED,
} hal_thread_state;

static volatile hal_thread_state g_fault_event_running_flag = THREAD_STOPPED;

STATIC void hal_set_fault_event_thread_state(hal_thread_state flag)
{
    g_fault_event_running_flag = flag;
}

STATIC hal_thread_state hal_get_fault_event_thread_state(void)
{
    return g_fault_event_running_flag;
}

STATIC int halUnsubFaultEvent(struct hal_fault_event_thread *thread_para)
{
    int ret;

    if (hal_get_fault_event_thread_state() == THREAD_RUNING) {
        DMS_INFO("unsubscribe fault event\n");
        ret = pthread_cancel(thread_para->thread_id);
        if (ret != 0) {
            DMS_ERR("stop fault event thread failed.\n");
        } else {
            DMS_INFO("stop fault event thread successfully.\n");
        }
        (void)pthread_mutex_lock(&thread_para->thread_lock);
        thread_para->handler = NULL;
        (void)pthread_mutex_unlock(&thread_para->thread_lock);
        return ret;
    }
    DMS_INFO("fault event thread is not running, unsubscribe finished.\n");
    return 0;
}

STATIC int hal_thread_read_fault_event(struct hal_fault_event_thread *thread_para, int timeout,
                                       struct halFaultEventInfo *event)
{
    int ret;

    ret = halReadFaultEvent(thread_para->device_id, timeout, &(thread_para->filter), event);
    if (ret == (int)DRV_ERROR_NONE) {
        if (thread_para->handler != NULL) {
            thread_para->handler(event);
        } else {
            DMS_INFO("Fault event has unsubscribed\n");
            return DRV_ERROR_UNINIT;
        }
    } else if (ret == (int)DRV_ERROR_NO_EVENT || ret == (int)DRV_ERROR_WAIT_TIMEOUT) {
        return ret;
    } else if (ret == -ERESTARTSYS) {
        DMS_WARN("Fault event reading thread will exit as system reboot\n");
        hal_set_fault_event_thread_state(THREAD_STOPPED);
        return ret;
    } else if (ret == (int)DRV_ERROR_NOT_SUPPORT) {
        hal_set_fault_event_thread_state(THREAD_STOPPED);
        return ret;
    } else if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {
        hal_set_fault_event_thread_state(THREAD_STOPPED);
        return ret;
    } else {
        DMS_ERR("Get fault event failed. (ret=%d)\n", ret);
        (void)usleep(HAL_FAULT_THREAD_SLEEP);
    }

    return ret;
}

STATIC void *hal_fault_event_thread_func(void *data)
{
    int ret;
    struct halFaultEventInfo event = {0};
    struct hal_fault_event_thread *thread_para = (struct hal_fault_event_thread *)data;

    (void)mmSetCurrentThreadName("hal_fault_event_subscribe");
    ret = hal_thread_read_fault_event(thread_para, HAL_EVENT_WAIT_TIME_ZERO, &event);
    if (ret == (int)DRV_ERROR_NOT_SUPPORT) {
        thread_para->result = ret;
        return NULL;
    } else if (ret == (int)DRV_ERROR_RESOURCE_OCCUPIED) {
        DMS_ERR("Over max number of process. (device id =%d; ret=%d)\n", thread_para->device_id, ret);
        thread_para->result = ret;
        return NULL;
    } else {
        thread_para->result = DRV_ERROR_NONE;
    }

    while (hal_get_fault_event_thread_state() == THREAD_RUNING) {
        ret = hal_thread_read_fault_event(thread_para, HAL_READ_FAULT_NO_TIMEOUT_FLAG, &event);
        if (ret == (int)DRV_ERROR_UNINIT) {
            DMS_INFO("User unsubscribed, thread exit\n");
            break;
        } else if (ret == -ERESTARTSYS) {
            DMS_WARN("Fault event reading thread exited\n");
            return NULL;
        }

        pthread_testcancel(); /* set thread cancel point */
    }
    hal_set_fault_event_thread_state(THREAD_STOPPED);
    thread_para->run_flg = false;
    DMS_WARN("Something went wrong with dsmi_fault_event_thread_func!\n");

    return NULL;
}

drvError_t halSubscribeFaultEvent(int device_id, struct halEventFilter filter, halfaulteventcb handler)
{
    struct hal_fault_event_thread *thread_para = &g_fault_thread_para;
    struct sched_param thread_sched_param;
    pthread_attr_t attr;
    unsigned int phy_id;
    int ret;

    if ((device_id < 0) && (device_id != DMS_FAULT_DEVICE_ALL)) {
        DMS_ERR("Invalid parameter. (device_id=%d)\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    if (handler == NULL) {
        return halUnsubFaultEvent(thread_para);
    }

    if (device_id != DMS_FAULT_DEVICE_ALL) {
        ret = drvDeviceGetPhyIdByIndex((unsigned int)device_id, &phy_id);
        if (ret != 0) {
            DMS_ERR("Invalid parameter. (log_dev_id=%d)\n", device_id);
            return DRV_ERROR_PARA_ERROR;
        }
    }

    if (pthread_mutex_trylock(&thread_para->thread_lock) != 0) {
        DMS_ERR("has started one thread before.\n");
        return DRV_ERROR_RESOURCE_OCCUPIED;
    }

    if (thread_para->run_flg == true) {
        (void)pthread_mutex_unlock(&thread_para->thread_lock);
        DMS_ERR("A fault event thread has already been started.\n");
        return DRV_ERROR_RESOURCE_OCCUPIED;
    }

    thread_para->run_flg = true;
    thread_para->device_id = device_id;
    thread_para->handler = handler;
    thread_para->filter = filter;

    (void)pthread_attr_init(&attr);
    (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    (void)pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
#ifndef DRV_HOST
    (void)pthread_attr_setstacksize(&attr, HAL_FAULT_EVT_THREAD_STACK_SIZE);
#endif

    /* set the priority */
    (void)pthread_attr_setschedpolicy(&attr, SCHED_RR);
    thread_sched_param.sched_priority = sched_get_priority_min(SCHED_RR) + HAL_FAULT_EVENT_THREAD_PRIORITY;
    (void)pthread_attr_setschedparam(&attr, &thread_sched_param);

    hal_set_fault_event_thread_state(THREAD_RUNING);
    ret = pthread_create(&thread_para->thread_id, &attr, hal_fault_event_thread_func, (void *)thread_para);
    if (ret != 0) {
        DMS_ERR("Create get_event thread failed. (ret=%d)\n", ret);
        thread_para->run_flg = false;
        hal_set_fault_event_thread_state(THREAD_STOPPED);
        (void)pthread_mutex_unlock(&thread_para->thread_lock);
        (void)pthread_attr_destroy(&attr);
        return DRV_ERROR_INNER_ERR;
    }

    while (thread_para->result == HAL_FAULT_THREAD_INIT_VAL) {
        (void)usleep(HAL_FAULT_THREAD_SLEEP);
    }
    ret = thread_para->result;

    (void)pthread_mutex_unlock(&thread_para->thread_lock);
    (void)pthread_attr_destroy(&attr);
    return ret;
}

#if (defined DRV_HOST) && (defined CFG_FEATURE_DEVICE_REPLACE) /* only for host */
STATIC drvError_t dms_dev_replace_para_check(struct dsmi_device_attr *src_dev_attr,
                                             struct dsmi_device_attr *dst_dev_attr, unsigned int timeout,
                                             unsigned long long flag)
{
    (void)flag;

    if ((src_dev_attr == NULL) || (dst_dev_attr == NULL)) {
        DMS_ERR("Invalid parameter. (src_dev_attr=%s; dst_dev_attr=%s)\n", (src_dev_attr == NULL) ? "NULL" : "OK",
                (dst_dev_attr == NULL) ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    if ((timeout == 0) || (timeout > DSMI_MAX_DEVICE_REPLACE_TIMEOUT_SEC)) {
        DMS_ERR("Invalid parameter, timeout out of [1, %u]s. (timeout=%u)\n", DSMI_MAX_DEVICE_REPLACE_TIMEOUT_SEC,
                timeout);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((src_dev_attr->phy_dev_id != dst_dev_attr->phy_dev_id) || (dst_dev_attr->phy_dev_id < 0) ||
        (dst_dev_attr->phy_dev_id >= ASCEND_PDEV_MAX_NUM)) {
        DMS_ERR("Invalid parameter. (src dev_id=%d; dst dev_id=%d)\n", src_dev_attr->phy_dev_id,
                dst_dev_attr->phy_dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if ((src_dev_attr->type < 0) || (src_dev_attr->type >= DSMI_URMA_TYPE_MAX) || (dst_dev_attr->type < 0) ||
        (dst_dev_attr->type >= DSMI_URMA_TYPE_MAX) || (src_dev_attr->eid_num != dst_dev_attr->eid_num)) {
        DMS_ERR("Invalid parameter. (src eid_type=%u; dst eid_type=%u; src eid_num=%u; dst eid_num=%u)\n",
                src_dev_attr->type, dst_dev_attr->type, src_dev_attr->eid_num, dst_dev_attr->eid_num);
        return DRV_ERROR_PARA_ERROR;
    }
    return DRV_ERROR_NONE;
}
#endif

drvError_t DmsDevReplace(struct dsmi_device_attr *src_dev_attr, struct dsmi_device_attr *dst_dev_attr,
                         unsigned int timeout, unsigned long long flag)
{
#if (defined DRV_HOST) && (defined CFG_FEATURE_DEVICE_REPLACE) /* only for host */
    int ret = 0;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_dev_replace_stru dev_replace = {0};

    ret = dms_dev_replace_para_check(src_dev_attr, dst_dev_attr, timeout, flag);
    if (ret != DRV_ERROR_NONE) {
        DMS_ERR("Input para check failed, (ret=%d).\n", ret);
        return ret;
    }

    ret = memcpy_s(&dev_replace.src_dev_attr, sizeof(struct dsmi_device_attr), src_dev_attr,
                   sizeof(struct dsmi_device_attr));
    if (ret != 0) {
        DMS_ERR("Memcpy_s src_dev_attr failed, (ret=%d).\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = memcpy_s(&dev_replace.dst_dev_attr, sizeof(struct dsmi_device_attr), dst_dev_attr,
                   sizeof(struct dsmi_device_attr));
    if (ret != 0) {
        DMS_ERR("Memcpy_s dst_dev_attr failed, (ret=%d).\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    dev_replace.timeout = timeout;
    dev_replace.flag = flag;
    ioarg.main_cmd = ASCEND_UB_CMD_BASIC;
    ioarg.sub_cmd = ASCEND_UB_SUBCMD_HOST_DEVICE_RELINK;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_replace;
    ioarg.input_len = sizeof(struct dms_dev_replace_stru);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "IOCTL failed. (ret=%d)\n", ret);
        return ret;
    }
    return DRV_ERROR_NONE;
#else
    (void)src_dev_attr;
    (void)dst_dev_attr;
    (void)timeout;
    (void)flag;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t halDeviceBindHost(unsigned int dev_id, struct dsmi_eid_pair_info *eid_info)
{
#if (!defined DRV_HOST) && (defined CFG_FEATURE_DEVICE_REPLACE) /* only for device */
    int ret = 0;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_eid_info_stru info = {0};

    if ((dev_id >= ASCEND_PDEV_MAX_NUM) || (eid_info == NULL)) {
        DMS_ERR("Invalid parameter. (dev_id=%u, eid_info=%s)\n", dev_id, (eid_info == NULL) ? "NULL" : "OK");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = memcpy_s(&info.eid, sizeof(struct dsmi_eid_pair_info), eid_info, sizeof(struct dsmi_eid_pair_info));
    if (ret != 0) {
        DMS_ERR("memcpy_s failed. (ret=%d, dev_id=%u)\n", ret, dev_id);
        return DRV_ERROR_INNER_ERR;
    }

    info.dev_id = dev_id;
    ioarg.main_cmd = ASCEND_UB_CMD_BASIC;
    ioarg.sub_cmd = ASCEND_UB_SUBCMD_HOST_DEVICE_RELINK;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&info;
    ioarg.input_len = sizeof(struct dms_eid_info_stru);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "IOCTL failed. (ret=%d, dev_id=%u)\n", ret, dev_id);
        return ret;
    }
    return DRV_ERROR_NONE;
#else
    (void)dev_id;
    (void)eid_info;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}
