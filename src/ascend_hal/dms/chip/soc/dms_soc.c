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

#include "dms_user_common.h"
#include "dms/dms_soc_interface.h"
#include "devmng_user_common.h"
#include "devdrv_ioctl.h"
#include "dms/dms_devdrv_info_comm.h"
#include "ascend_dev_num.h"
#include "dms_device_info.h"
#include "dms_soc.h"

drvError_t DmsGetBoardId(unsigned int dev_id, unsigned int *board_id)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (board_id == NULL)) {
        DMS_ERR("Invalid dev_id or board_id is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_SOC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_BOARD_ID;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)board_id;
    ioarg.output_len = sizeof(unsigned int);

    ret = errno_to_user_errno(DmsIoctl(DMS_GET_BOARD_ID, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get board id failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    DMS_DEBUG("Get board id success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsGetPcbId(unsigned int dev_id, unsigned int *pcb_id)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (pcb_id == NULL)) {
        DMS_ERR("Invalid dev_id or pcb_id is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_SOC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_PCB_ID;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)pcb_id;
    ioarg.output_len = sizeof(unsigned int);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get pcb id failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    DMS_DEBUG("Get pcb id success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsGetBomId(unsigned int dev_id, unsigned int *bom_id)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (bom_id == NULL)) {
        DMS_ERR("Invalid dev_id or bom_id is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_SOC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_BOM_ID;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)bom_id;
    ioarg.output_len = sizeof(unsigned int);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get bom id failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    DMS_DEBUG("Get bom id success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsGetSlotId(unsigned int dev_id, unsigned int *slot_id)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (slot_id == NULL)) {
        DMS_ERR("Invalid dev_id or slot_id is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_SOC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_SLOT_ID;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)slot_id;
    ioarg.output_len = sizeof(unsigned int);

    ret = errno_to_user_errno(DmsIoctl(DMS_GET_SLOT_ID, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get slot id failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    DMS_DEBUG("Get slot id success.\n");
    return DRV_ERROR_NONE;
}

static int dms_virt_get_pcie_id_info(unsigned int dev_id, dms_pcie_id_info_t *pcie_idinfo)
{
    int ret;

    pcie_idinfo->davinci_id = dev_id;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_PCIE_ID_INFO, pcie_idinfo);
    if (ret != 0) {
        DMS_ERR("Dms virt get pcie info ioctl failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int dms_phy_get_pcie_id_info(unsigned int dev_id, dms_pcie_id_info_t *pcie_idinfo)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_PCIE_INFO;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)pcie_idinfo;
    ioarg.output_len = sizeof(dms_pcie_id_info_t);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms phy get pcie info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }

    return ret;
}

drvError_t DmsGetPcieIdInfo(unsigned int dev_id, dms_pcie_id_info_t *pcie_idinfo)
{
    int ret;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (pcie_idinfo == NULL)) {
        DMS_ERR("Invalid dev_id or pcie_idinfo is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = DMS_VIRT_ADAPT_FUNC(dms_virt_get_pcie_id_info, dms_phy_get_pcie_id_info)(dev_id, pcie_idinfo);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get pcie info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    DMS_DEBUG("Get pcie info success.\n");
    return DRV_ERROR_NONE;
}

static drvError_t DmsGetPcieIdInfoV2(unsigned int dev_id, void *buf, int size)
{
    int ret;
    HAL_PCIE_INFO *pcie_info = NULL;
    dms_pcie_id_info_t pcie_idinfo = {0};

    if (size != sizeof(HAL_PCIE_INFO)) {
        DMS_ERR("Invalid size. (size=%d)\n", size);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = DmsGetPcieIdInfo(dev_id, &pcie_idinfo);
    if (ret) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get pcie info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    pcie_info = (HAL_PCIE_INFO *)buf;
    pcie_info->domain = pcie_idinfo.domain;
    pcie_info->bus = pcie_idinfo.bus;
    pcie_info->device = pcie_idinfo.device;
    pcie_info->function = pcie_idinfo.fn;
    return DRV_ERROR_NONE;
}

drvError_t dms_get_pcie_info(unsigned int dev_id, unsigned int info_type, void *buf, int *size)
{
    int ret;

    if (dev_id >= ASCEND_DEV_MAX_NUM || buf == NULL || size == NULL) {
        DMS_ERR("Invalid dev_id or buf is null or size is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    switch (info_type) {
        case INFO_TYPE_PCIE_ID_INFO:
            ret = DmsGetPcieIdInfoV2(dev_id, buf, *size);
            break;

        default:
            ret = DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get pcie info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    return ret;
}

drvError_t DmsGetCpuInfo(unsigned int dev_id, drvCpuInfo_t *cpu_info)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (cpu_info == NULL)) {
        DMS_ERR("Invalid dev_id or cpu_info is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_SOC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_CPU_INFO;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)cpu_info;
    ioarg.output_len = sizeof(drvCpuInfo_t);

    ret = errno_to_user_errno(DmsIoctl(DMS_GET_CPU_INFO, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get cpu info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    DMS_DEBUG("Get cpu info success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsGetSocDieId(dms_soc_die_id_t *soc_die_id)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};

    if (soc_die_id == NULL) {
        DMS_ERR("Soc die id is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_GET_GET_DIE_ID_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter_len = 0;
    ioarg.input = (void *)soc_die_id;
    ioarg.input_len = sizeof(dms_soc_die_id_t);
    ioarg.output = (void *)soc_die_id;
    ioarg.output_len = sizeof(dms_soc_die_id_t);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get soc die id failed. (dev_id=%u; ret=%d)\n", soc_die_id->dev_id, ret);
        return ret;
    }

    DMS_DEBUG("Get soc die id success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsGetSocDieIdV2(dms_soc_die_id_v2_t *soc_die_id)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};

    if (soc_die_id == NULL) {
        DMS_ERR("Soc die id is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.main_cmd = DMS_GET_GET_DIE_ID_V2_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter_len = 0;
    ioarg.input = (void *)soc_die_id;
    ioarg.input_len = sizeof(dms_soc_die_id_v2_t);
    ioarg.output = (void *)soc_die_id;
    ioarg.output_len = sizeof(dms_soc_die_id_v2_t);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get soc die id failed. (dev_id=%u; ret=%d)\n", soc_die_id->dev_id, ret);
        return ret;
    }

    DMS_DEBUG("Get soc die id success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsGetChipVersion(u32 devid, u8 *chip_version)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};

    ioarg.main_cmd = DMS_GET_CHIP_EXPAND_VERSION_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&devid;
    ioarg.input_len = sizeof(u32);
    ioarg.output = (void *)chip_version;
    ioarg.output_len = sizeof(u8);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        if (ret != DRV_ERROR_NOT_SUPPORT) {
            DMS_ERR("Ioctl failed for chip version. (dev_id=%u; ret=%d)\n", devid, ret);
        }
        return ret;
    }

    DMS_DEBUG("Get chip version success.\n");
    return DRV_ERROR_NONE;
}

#ifndef CFG_FEATURE_SRIOV
static int dms_virt_get_chip_info(dms_query_chip_info_t *chip_info)
{
    int ret;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_CHIP_INFO, chip_info);
    if (ret != 0) {
        DMS_ERR("Ioctl failed. (ret=%d, errno=%d)\n", ret, errno);
        return ret;
    }

    return DRV_ERROR_NONE;
}
#endif

drvError_t DmsGetChipInfo(dms_query_chip_info_t *chip_info)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    unsigned int dev_id;

    if (chip_info == NULL) {
        DMS_ERR("Chip info is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (chip_info->dev_id >= ASCEND_DEV_MAX_NUM) {
        DMS_ERR("Device id is invalid. (dev_id=%u; max=%u)\n", chip_info->dev_id, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

#ifndef CFG_FEATURE_SRIOV
    if (DmsGetVirtFlag() != 0) {
        ret = dms_virt_get_chip_info(chip_info);
        if (ret != 0) {
            DMS_ERR("Get chip info failed. (dev_id=%u; ret=%d)\n", chip_info->dev_id, ret);
        }
        return ret;
    }
#endif

    dev_id = chip_info->dev_id;
    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_CHIP_INFO;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)chip_info;
    ioarg.output_len = sizeof(dms_query_chip_info_t);

    ret = errno_to_user_errno(DmsIoctl(DMS_GET_CHIP_INFO, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get chip info failed. (dev_id=%u; ret=%d)\n", chip_info->dev_id, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

drvError_t dms_user_get_reboot_reason(unsigned int dev_id, void *reboot_reason, unsigned int len)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};

    (void)len;

    ioarg.main_cmd = DMS_GET_GET_REBOOT_REASON_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = (unsigned int)sizeof(unsigned int);
    ioarg.output = reboot_reason;
    ioarg.output_len = (unsigned int)sizeof(struct dsmi_reboot_reason);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get soc reboot reason failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    DMS_DEBUG("Get soc reboot reason success.\n");
    return DRV_ERROR_NONE;
}

drvError_t halGetDeviceSplitMode(unsigned int dev_id, unsigned int *mode)
{
    int ret;
    unsigned int hostflag;
    struct dms_ioctl_arg ioarg = {0};

    if (mode == NULL) {
        DMS_ERR("mode is null.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = drvGetHostPhyMachFlag(dev_id, &hostflag);
    if (ret != 0) {
        DMS_ERR("Get host flag failed. (dev_id=%u; ret=%d).\n", dev_id, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    if (hostflag == DEVDRV_HOST_VM_MACH_FLAG) {
        *mode = VMNG_VIRTUAL_SPLIT_MODE; /* in VM SPLIT, return directly. */
        return DRV_ERROR_NONE;
    }

    ioarg.main_cmd = DMS_MAIN_CMD_BASIC;
    ioarg.sub_cmd = DMS_SUBCMD_GET_DEV_SPLIT_MODE;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(unsigned int);
    ioarg.output = (void *)mode;
    ioarg.output_len = sizeof(unsigned int);

    ret = errno_to_user_errno(DmsIoctl((int)DMS_GET_DEV_SPLIT_MODE, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "IOCTL failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvGetDeviceSplitMode(unsigned int dev_id, unsigned int *mode)
{
    return halGetDeviceSplitMode(dev_id, mode);
}

drvError_t DmsSetBistInfo(unsigned int dev_id, unsigned int cmd, unsigned char *in_buf, unsigned int buf_len)
{
    int ret;
    struct dms_set_bist_info_in bist_info;
    struct dms_ioctl_arg ioarg = {0};

    bist_info.dev_id = dev_id;
    bist_info.buff = in_buf;
    bist_info.buff_size = buf_len;

    ioarg.main_cmd = DMS_SET_BIST_INFO_CMD;
    ioarg.sub_cmd = cmd;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&bist_info;
    ioarg.input_len = sizeof(struct dms_set_bist_info_in);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms set bist info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    DMS_INFO("Set bist info success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsGetBistInfo(unsigned int dev_id, unsigned int cmd, unsigned char *buf, unsigned int *len)
{
    int ret;
    struct dms_get_bist_info_in bist_info_in;
    struct dms_get_bist_info_out bist_info_out;
    struct dms_ioctl_arg ioarg = {0};

    bist_info_in.dev_id = dev_id;
    bist_info_in.sub_cmd = cmd;
    bist_info_in.buff = buf;
    bist_info_in.size = *len;

    ioarg.main_cmd = DMS_GET_BIST_INFO_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&bist_info_in;
    ioarg.input_len = sizeof(struct dms_get_bist_info_in);
    ioarg.output = &bist_info_out;
    ioarg.output_len = sizeof(struct dms_get_bist_info_out);

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Dms get bist info failed, dev_id=%u ret=%d.\n", dev_id, ret);
        return ret;
    }

    *len = bist_info_out.size;

    DMS_DEBUG("Get bist info success.\n");
    return DRV_ERROR_NONE;
}

drvError_t DmsCtrlDeviceNode(unsigned int dev_id, struct dsmi_dtm_node_s dtm_node, DSMI_DTM_OPCODE opcode,
                             IN_OUT_BUF buf)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_ctrl_device_node_in ctrl_device_node_in;
    ctrl_device_node_in.dev_id = dev_id;
    ctrl_device_node_in.node_type = dtm_node.node_type;
    ctrl_device_node_in.node_id = dtm_node.node_id;
    ctrl_device_node_in.sub_cmd = opcode;
    ctrl_device_node_in.in_buf = buf.in_buf;
    ctrl_device_node_in.in_size = buf.in_size;

    ioarg.main_cmd = DMS_CTRL_DEVICE_NODE_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&(ctrl_device_node_in);
    ioarg.input_len = sizeof(struct dms_ctrl_device_node_in);
    ioarg.output = NULL;
    ioarg.output_len = 0;

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "DmsCtrlDeviceNode failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        ret = errno_to_user_errno(ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t DmsGetAllDeviceNode(unsigned int dev_id, DEV_DTM_CAP capability, struct dsmi_dtm_node_s node_info[],
                               unsigned int *size)
{
    int ret;
    struct dms_ioctl_arg ioarg = {0};
    struct dms_get_all_device_node_in get_all_device_node_in;
    struct dms_get_all_device_node_out get_all_device_node_out = {0};
    get_all_device_node_in.dev_id = dev_id;
    get_all_device_node_in.capability = capability;
    get_all_device_node_in.size = *size;
    get_all_device_node_in.node_info = node_info;

    ioarg.main_cmd = DMS_GET_ALL_DEVICE_NODE_CMD;
    ioarg.sub_cmd = ZERO_CMD;
    ioarg.filter_len = 0;
    ioarg.input = (void *)&(get_all_device_node_in);
    ioarg.input_len = sizeof(struct dms_get_all_device_node_in);
    ioarg.output = (void *)&get_all_device_node_out;
    ioarg.output_len = sizeof(struct dms_get_all_device_node_out);

    ret = DmsIoctl(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "DmsGetAllDeviceNode failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        ret = errno_to_user_errno(ret);
        return ret;
    }

    *size = get_all_device_node_out.out_size;
    return DRV_ERROR_NONE;
}

#ifdef CFG_FEATURE_GET_DEV_INDEX_IN_GROUP
drvError_t DmsGetIndexInGroup(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                              unsigned int *size)
{
    int ret = 0;

    ret = DmsGetDeviceInfoExV2(dev_id, main_cmd, sub_cmd, buf, size);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Failed to obtain the device information. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}
#endif