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
#include <unistd.h>
#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "securec.h"
#include "dms_cmd_def.h"
#include "dms_user_common.h"
#include "ascend_kernel_hal.h"
#include "ascend_hal_define.h"
#include "devdrv_user_common.h"
#include "dms/dms_misc_interface.h"
#include "hbm_ctrl.h"
#include "ascend_dev_num.h"

int dev_ecc_config_get_total_isolated_pages_info(unsigned int dev_id, struct ecc_statistics_s_all *pvalue)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};

    if (dev_id >= DEVICE_NUM_MAX) {
        DMS_ERR("Invalid devid. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    if (pvalue == NULL) {
        DMS_ERR("pvalue must not be null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ioarg.main_cmd = DMS_GET_ISOLATED_PAGES_INFO_CMD;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)pvalue;
    ioarg.output_len = sizeof(struct ecc_statistics_s_all);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get isolated pages info ioctl failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

int dev_ecc_config_clear_isolated_info(unsigned int dev_id)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};

    if (dev_id >= DEVICE_NUM_MAX) {
        DMS_ERR("Invalid devid. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ioarg.main_cmd = DMS_CLEAR_ISOLATED_INFO_CMD;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Clear ECC isolated info Ioctl failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        ret = DRV_ERROR_IOCRL_FAIL;
    }

    return ret;
}

STATIC int dev_ecc_config_ioctl_read(unsigned int dev_id, unsigned int op_type,
                                     struct ecc_config_udata_s *ecc_config_udata)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};

    ecc_config_udata->dev_id = dev_id;
    ecc_config_udata->op_type = op_type;

    ioarg.main_cmd = DMS_GET_ECC_RECORD_CMD;
    ioarg.input = (void *)ecc_config_udata;
    ioarg.input_len = sizeof(struct ecc_config_udata_s);
    ioarg.output = (void *)ecc_config_udata;
    ioarg.output_len = sizeof(struct ecc_config_udata_s);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        ret = DRV_ERROR_IOCRL_FAIL;
    }

    return ret;
}

int dev_ecc_config_get_multi_ecc_time_info(unsigned int dev_id, MULTI_ECC_TIMES *multi_eccs_timestamp)
{
    int ret;
    struct ecc_config_udata_s ecc_config_udata = {0};

    if (multi_eccs_timestamp == NULL) {
        DMS_ERR("Invalid ECC time info parameter.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (dev_id >= DEVICE_NUM_MAX) {
        DMS_ERR("Invalid devid. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = dev_ecc_config_ioctl_read(dev_id, MULTI_ECC_TIMES_READ, &ecc_config_udata);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Read ECC config timestamp failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = memcpy_s(multi_eccs_timestamp, sizeof(MULTI_ECC_TIMES), &ecc_config_udata.multi_ecc_time_data,
                   sizeof(MULTI_ECC_TIMES));
    if (ret != 0) {
        DMS_ERR("Memcpy multi ECC timestamp failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }
    return ret;
}

int dev_ecc_config_get_multi_ecc_info(unsigned int dev_id, unsigned int data_index,
                                      struct ecc_common_data_s *ecc_detail_info)
{
    int ret;
    struct ecc_config_udata_s ecc_config_udata = {0};

    if (ecc_detail_info == NULL) {
        DMS_ERR("Invalid multi ECC info parameter.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (dev_id >= DEVICE_NUM_MAX) {
        DMS_ERR("Invalid devid. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ecc_config_udata.data_index = data_index;
    ret = dev_ecc_config_ioctl_read(dev_id, MULTI_ECC_INFO_READ, &ecc_config_udata);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Read ECC config timestamp failed. (dev_id=%u; data_index=%u; ret=%d)\n", dev_id,
                              data_index, ret);
        return ret;
    }
    ecc_detail_info->physical_addr = ecc_config_udata.multi_ecc_data.physical_addr;
    ecc_detail_info->timestamp = ecc_config_udata.multi_ecc_data.timer_stamp;
#if defined CFG_FEATURE_HBM_FLASH || defined CFG_FEATURE_DDR_FLASH
    ecc_detail_info->stack_pc_id = ecc_config_udata.multi_ecc_data.module_id;
    ecc_detail_info->reg_addr_h = (uint32_t)(ecc_config_udata.multi_ecc_data.row << LOW_ADDR_ROW_BITS_OFFSET) |
                                  ecc_config_udata.multi_ecc_data.column;
    ecc_detail_info->reg_addr_l = (uint32_t)(ecc_config_udata.multi_ecc_data.rank << HIGH_ADDR_SID_BITS_COUNT) |
                                  ecc_config_udata.multi_ecc_data.bank;
#else
    ecc_detail_info->reg_addr_h = (uint32_t)(ecc_config_udata.multi_ecc_data.row << HIGH_ADDR_COLUMN_BITS_COUNT) |
                                  ecc_config_udata.multi_ecc_data.column;
    ecc_detail_info->reg_addr_l = (uint32_t)(ecc_config_udata.multi_ecc_data.rank << LOW_ADDR_RANK_BITS_OFFSET) |
                                  ecc_config_udata.multi_ecc_data.bank;
#endif
    return ret;
}

int dev_ecc_config_get_single_ecc_info(unsigned int dev_id, unsigned int data_index,
                                       struct ecc_common_data_s *ecc_detail_info)
{
    int ret;
    struct ecc_config_udata_s ecc_config_udata = {0};

    if (ecc_detail_info == NULL) {
        DMS_ERR("Invalid single ECC info parameter.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (dev_id >= DEVICE_NUM_MAX) {
        DMS_ERR("Invalid devid. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ecc_config_udata.data_index = data_index;
    ret = dev_ecc_config_ioctl_read(dev_id, SINGLE_ECC_INFO_READ, &ecc_config_udata);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Read single ECC info failed. (devid=%u; data_index=%u; ret=%d)\n", dev_id,
                              data_index, ret);
        return ret;
    }

    ecc_detail_info->stack_pc_id = ecc_config_udata.single_ecc_data.hbmc_id;
    ecc_detail_info->reg_addr_h = ecc_config_udata.single_ecc_data.single_bit_high_addr;
    ecc_detail_info->reg_addr_l = ecc_config_udata.single_ecc_data.single_bit_low_addr;
    ecc_detail_info->ecc_count = ecc_config_udata.single_ecc_data.single_bit_count;
    ecc_detail_info->timestamp = (int)ecc_config_udata.single_ecc_data.last_appear_time_stamp;
    return ret;
}

int dev_ecc_config_get_ecc_addr_count(unsigned int dev_id, unsigned char err_type,
                                      unsigned int count_value[ECC_ERROR_TYPE_COUNT])
{
    int ret;
    struct ecc_config_udata_s ecc_config_udata = {0};

    if (dev_id >= DEVICE_NUM_MAX) {
        DMS_ERR("Invalid devid. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (err_type > ECC_ERROR_TYPE_COUNT) {
        DMS_ERR("Invalid ECC address count parameter. (devid=%u; err_type=%u)\n", dev_id, err_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = dev_ecc_config_ioctl_read(dev_id, ECC_ADDRESS_COUNT_READ, &ecc_config_udata);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Read ECC address count failed. (devid=%u; err_type=%u; ret=%d)\n", dev_id, err_type,
                              ret);
        return ret;
    }
    count_value[0] = ecc_config_udata.ecc_address_count.single_ecc_addr_cnt;
    count_value[1] = ecc_config_udata.ecc_address_count.multi_ecc_addr_cnt;
    /* set return value 0 to multi ecc address count if only read multi ecc error */
    if (err_type == MULTI_ECC_INFO_READ) {
        count_value[0] = ecc_config_udata.ecc_address_count.multi_ecc_addr_cnt;
    }
    return ret;
}

int dev_ecc_config_get_va_info(unsigned int devId, void *buf, unsigned int *size)
{
    int ret = 0;
    unsigned int i;
    struct hbm_pa_va_info va_info = {0};
    struct MemRepairInPara *mem_para = NULL;
    struct dms_ioctl_arg ioarg = {0};

    if (devId >= ASCEND_DEV_MAX_NUM || buf == NULL || size == NULL) {
        DMS_ERR("Invalid parameters. (dev_id=%u; buf%s; size%s)\n", devId, buf == NULL ? "=NULL" : "!=NULL",
                size == NULL ? "=NULL" : "!=NULL");
        return DRV_ERROR_PARA_ERROR;
    }
    if (*size != sizeof(struct MemRepairInPara)) {
        DMS_ERR("Invalid parameters. (dev_id=%u; size=%u)\n", devId, *size);
        return DRV_ERROR_PARA_ERROR;
    }

    mem_para = (struct MemRepairInPara *)buf;
    va_info.dev_id = devId;

    ioarg.main_cmd = DMS_MAIN_CMD_MEMORY;
    ioarg.sub_cmd = DMS_SUBCMD_HBM_GET_VA;
    ioarg.filter_len = 0;
    ioarg.input = &va_info;
    ioarg.input_len = sizeof(struct hbm_pa_va_info);
    ioarg.output = &va_info;
    ioarg.output_len = sizeof(struct hbm_pa_va_info);
    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "dev_ecc_config_get_va_info failed. (dev_id=%u; ret=%d)", devId, ret);
        return ret;
    }
#ifndef DRV_HBM_UT
    if (va_info.va_num > MEM_REPAIR_MAX_CNT) {
        DMS_ERR("Va num is incorrect. (devid=%u; va_num=%u)\n", devId, va_info.va_num);
        return DRV_ERROR_INVALID_VALUE;
    }
    mem_para->devid = devId;
    mem_para->count = va_info.va_num;

    for (i = 0; i < va_info.va_num; i++) {
        mem_para->repairAddrs[i].ptr = va_info.va_info[i].va_addr;
        mem_para->repairAddrs[i].len = (unsigned long long)(1ULL << va_info.va_info[i].size);
    }
#endif
    return ret;
}

drvError_t dms_memory_get_hbm_ecc_syscnt(unsigned int dev_id, void *buf, unsigned int *size)
{
#if (defined(CFG_FEATURE_GET_CURRENT_EVENTINFO) && defined(DRV_HOST)) || (defined(URD_FORWARD_UT))
    struct urd_ioctl_arg ioarg = {0};
    struct memory_fault_timestamp mem_para = {0};
    HAL_FAULT_OCCUR_SYSCNT_STRU *input = NULL;
    int ret;

    if (dev_id >= ASCEND_DEV_MAX_NUM || buf == NULL || size == NULL) {
        DMS_ERR("Invalid parameters. (dev_id=%u; buf%s; size%s)\n", dev_id, buf == NULL ? "=NULL" : "!=NULL",
                size == NULL ? "=NULL" : "!=NULL");
        return DRV_ERROR_PARA_ERROR;
    }
    if (*size != sizeof(HAL_FAULT_OCCUR_SYSCNT_STRU)) {
        DMS_ERR("Invalid parameter. (dev_id=%u; input_size=%u; expected=%u)\n", dev_id, *size,
                sizeof(HAL_FAULT_OCCUR_SYSCNT_STRU));
        return DRV_ERROR_PARA_ERROR;
    }

    input = (HAL_FAULT_OCCUR_SYSCNT_STRU *)buf;
    mem_para.dev_id = dev_id;
    mem_para.event_id = input->event_id;

    ioarg.devid = dev_id;
    ioarg.cmd.main_cmd = DMS_MAIN_CMD_MEMORY;
    ioarg.cmd.sub_cmd = DMS_SUBCMD_GET_FAULT_SYSCNT;
    ioarg.cmd.filter = NULL;
    ioarg.cmd.filter_len = 0;
    ioarg.cmd_para.input = &mem_para;
    ioarg.cmd_para.input_len = sizeof(struct memory_fault_timestamp);
    ioarg.cmd_para.output = &mem_para;
    ioarg.cmd_para.output_len = sizeof(struct memory_fault_timestamp);

    ret = DmsIoctlConvertErrno(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (dev_id=%u; event_id=%u; ret=%d)\n", dev_id, input->event_id, ret);
        return ret;
    }

    input->sys_cnt = mem_para.syscnt;
    return 0;
#else
    (void)dev_id;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}