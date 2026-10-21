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

#include "ka_driver_pub.h"
#include "dms_define.h"
#include "dms_template.h"
#include "dms/dms_cmd_def.h"
#include "urd_acc_ctrl.h"
#ifndef CFG_HOST_ENV
#include "dms_device_time_zone.h"
#endif
#include "devdrv_user_common.h"
#include "devdrv_manager_common.h"
#include "pbl_mem_alloc_interface.h"
#include "dms_chip_info.h"
#include "dms_vdev.h"
#include "ascend_kernel_hal.h"
#include "dms_basic_info.h"
#ifdef CFG_FEATURE_DEV_TOPOLOGY
#include "dms_dev_topology.h"
#endif
#ifdef CFG_FEATURE_MEM
#include "dms_mem_info.h"
#endif

#include "vmng_kernel_interface.h"
#include "adapter_api.h"
#include "pbl/pbl_soc_res.h"
#include "pbl/pbl_uda.h"
#include "dms_feature_pub.h"
#include "dms_sdk_ex_version.h"

#if defined(CFG_FEATURE_VFIO) && !defined(CFG_HOST_ENV) && !defined(CFG_FEATURE_VFIO_SOC)
#define VMNG_NORMAL_SPLIT_MODE 3
#define UDA_SUB_DEV_MAX_NUM 16

STATIC int dms_get_split_mode_in_device(unsigned int dev_id)
{
    unsigned int udevid;
    unsigned int i;
    struct uda_mia_dev_para mia_para;
    int ret;

    ret = uda_devid_to_udevid(dev_id, &udevid);
    if (ret != 0) {
        dms_err("Can't transform devid to udevid. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    if (!uda_is_pf_dev(udevid)) {
        return VMNG_NORMAL_SPLIT_MODE;
    }
    mia_para.phy_devid = udevid;
    for (i = 0; i < UDA_SUB_DEV_MAX_NUM; i++) {
        mia_para.sub_devid = i;
        if (uda_is_udevid_exist(uda_make_udevid(&mia_para))) {
            return VMNG_NORMAL_SPLIT_MODE;
        }
    }
    return VMNG_NORMAL_NONE_SPLIT_MODE;
}
#endif

STATIC int get_device_split_mode(u32 dev_id, u32 phy_id)
{
#ifndef CFG_FEATURE_VFIO
    return VMNG_NORMAL_NONE_SPLIT_MODE;
#else
#if !defined(CFG_HOST_ENV) && !defined(CFG_FEATURE_VFIO_SOC)
    return dms_get_split_mode_in_device(dev_id);
#else
    return vmng_get_device_split_mode(phy_id);
#endif
#endif
}

STATIC int dms_get_device_split_mode(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int dev_id, phy_id, vfid;

    if ((in == NULL) || (in_len != sizeof(u32))) {
        dms_err("input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(u32))) {
        dms_err("output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    dev_id = *(unsigned int *)in;

    ret = dms_trans_and_check_id(dev_id, &phy_id, &vfid);
    if (ret != 0) {
        dms_err("can't transform virt id. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    *(unsigned int *)out = get_device_split_mode(dev_id, phy_id);
    return 0;
}

STATIC s32 dms_get_gpio_status(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_get_gpio *arg = (struct dms_get_gpio *)in;
    unsigned int *status = (unsigned int *)out;
    unsigned int gpio_val;
    unsigned int gpio_num;
    int ret;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_gpio)) || (out == NULL) || (out_len != sizeof(unsigned int))) {
        dms_err("Invalid para. (in=%s; in_len=%u; out=%s; out_len=%u)\n", (in == NULL) ? "NULL" : "OK", in_len,
                (out == NULL) ? "NULL" : "OK", out_len);
        return -EINVAL;
    }
    if (arg->dev_id >= ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid device id. (dev_id=%u)\n", arg->dev_id);
        return -EINVAL;
    }
    gpio_num = arg->gpio_num;
    if (!ka_driver_gpio_is_valid(gpio_num)) {
        dms_err("Invalid GPIO. (num=%u)\n", gpio_num);
        return -EINVAL;
    }

    ret = ka_driver_gpio_request(gpio_num, DEVDRV_GPIO_NAME);
    if (ret != 0) {
        dms_err("GPIO request failed. (num=%u)\n", gpio_num);
        return ret;
    }

    gpio_val = (unsigned int)ka_driver_gpio_get_value(gpio_num);

    (void)ka_driver_gpio_free(gpio_num);

    *status = gpio_val;
    return 0;
}

#ifdef CFG_FEATURE_TRS_HB_REFACTOR
STATIC int dms_get_device_init_status(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct devdrv_manager_info *manager_info = devdrv_get_manager_info();
    u32 logic_id, ret;
    u32 init_status = DRV_STATUS_INITING;
    u32 phys_id = ASCEND_DEV_MAX_NUM;
    u32 vf_id = 0;

    if ((in == NULL) || (in_len != sizeof(u32)) || (out == NULL) || (out_len != sizeof(u32))) {
        dms_err("Invalid parameter. (in=%s; in_len=%u; out=%s; out_len=%u)\n", (in == NULL) ? "NULL" : "OK", in_len,
                (out == NULL) ? "NULL" : "OK", out_len);
        return -EINVAL;
    }

    logic_id = *(u32 *)in;
    ret = devdrv_manager_container_logical_id_to_physical_id(logic_id, &phys_id, &vf_id);
    if (ret != 0) {
        devdrv_drv_err("Failed to transfer logical ID to physical ID. (logic_id=%u; ret=%d)\n", logic_id, ret);
        return ret;
    }

    if (manager_info == NULL || manager_info->dev_info[phys_id] == NULL) {
        init_status = DRV_STATUS_INITING;
#ifdef CFG_HOST_ENV
    } else if (manager_info->device_status[phys_id] == DRV_STATUS_COMMUNICATION_LOST) {
        init_status = DRV_STATUS_COMMUNICATION_LOST;
#endif
    } else {
        /* TS heartbeat needs to be checked to determine if the device is normal. */
        init_status = DRV_STATUS_WORK;
    }

    *(u32 *)out = init_status;

    return 0;
}
#endif

#ifdef CFG_HOST_ENV
STATIC int dms_get_basic_info(unsigned int dev_id, int sub_cmd, unsigned int *basic_buffer)
{
    int ret;
    u32 phys_id;
    int vfid = 0;
    struct devdrv_board_info_cache *basic_info_host = NULL;

    if (dev_id >= ASCEND_DEV_MAX_NUM || basic_buffer == NULL) {
        dms_err("Invalid parameter. (dev_id=%u; dev_maxnum=%d; basic_buffer=%s)\n", dev_id, ASCEND_DEV_MAX_NUM,
                (basic_buffer == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    ret = uda_devid_to_phy_devid(dev_id, &phys_id, &vfid);
    if (ret != 0) {
        dms_err("Transform virt id failed. (phys_id=%u; ret=%d)\n", phys_id, ret);
        return ret;
    }

    basic_info_host = devdrv_get_board_info_host(phys_id);
    if (basic_info_host == NULL) {
        return -EOPNOTSUPP;
    }

    if (sub_cmd == DMS_SUBCMD_GET_BOARD_ID_HOST) {
        ret = memcpy_s(basic_buffer, sizeof(unsigned int), &(basic_info_host->board_id), sizeof(unsigned int));
    } else if (sub_cmd == DMS_SUBCMD_GET_SLOT_ID_HOST) {
        ret = memcpy_s(basic_buffer, sizeof(unsigned int), &(basic_info_host->slot_id), sizeof(unsigned int));
    } else if (sub_cmd == DMS_SUBCMD_GET_BOM_ID_HOST) {
        ret = memcpy_s(basic_buffer, sizeof(unsigned int), &(basic_info_host->bom_id), sizeof(unsigned int));
    } else if (sub_cmd == DMS_SUBCMD_GET_PCB_ID_HOST) {
        ret = memcpy_s(basic_buffer, sizeof(unsigned int), &(basic_info_host->pcb_id), sizeof(unsigned int));
    } else {
        dms_debug("Not support subcmd. (phys_id=%u; sub_cmd=%u).\n", phys_id, sub_cmd);
        return -EOPNOTSUPP;
    }
    if (ret != 0) {
        dms_err("basic info memcpy fail. (phys_id=%u; sub_cmd=%u).\n", phys_id, sub_cmd);
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_get_basic_info_op(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret;
    int sub_cmd;
    unsigned int dev_id;

    if (in == NULL || out == NULL || feature == NULL) {
        dms_err("Invalid parameter. (in=%s; out=%s; feature=%s)\n", (in == NULL) ? "NULL" : "OK",
                (out == NULL) ? "NULL" : "OK", (feature == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    if (in_len != sizeof(unsigned int) || out_len != sizeof(unsigned int)) {
        dms_err("Error parameter. (in_len=%u; correct in_len=%lu; out_len=%u; correct out_len=%lu)\n", in_len,
                sizeof(unsigned int), out_len, sizeof(unsigned int));
        return -EINVAL;
    }

    dev_id = *(unsigned int *)in;
    sub_cmd = ((DMS_FEATURE_S *)feature)->sub_cmd;
    ret = dms_get_basic_info(dev_id, sub_cmd, (unsigned int *)out);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Get basic info failed. (ret=%d)\n", ret);
        return ret;
    }
    return 0;
}

STATIC int dms_get_master_dev(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    unsigned int phy_id;
    unsigned int master_dev_id;

    if ((in == NULL) || (in_len != sizeof(u32))) {
        dms_err("Input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(u32))) {
        dms_err("Output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    phy_id = *(unsigned int *)in;
    master_dev_id = uda_get_master_id(phy_id);
    if (master_dev_id == UDA_INVALID_UDEVID) {
        dms_err("The value of master_id is invalid. (phy_id=%u)\n", phy_id);
        return -EINVAL;
    }
    *(u32 *)out = master_dev_id;
    dms_debug("Succeeded in obtaining the master device. (phy_id=%u; master_dev_id=%u)\n", phy_id, master_dev_id);

    return 0;
}
#endif

#ifdef CFG_FEATURE_AICORE_DIE_NUM
STATIC int dms_get_aicore_die_num(const struct urd_cmd *cmd, struct urd_cmd_kernel_para *kernel_para,
                                  struct urd_cmd_para *para)
{
    unsigned long long die_num = 0;
    int ret = 0;

    if ((cmd == NULL) || (kernel_para == NULL) || (para == NULL)) {
        dms_err("Input urd argument is null.\n");
        return -EINVAL;
    }

    if ((para->output == NULL) || (para->output_len != sizeof(unsigned int))) {
        dms_err("Output argument is null, or len is wrong. (output_len=%u)\n", para->output_len);
        return -EINVAL;
    }

    if (uda_is_pf_dev(kernel_para->udevid)) {
        ret = soc_resmng_dev_get_key_value(kernel_para->udevid, "soc_die_num", &die_num);
        if (ret != 0) {
            dms_err("Failed to obtain the number of aicore die num. (udevid=%u; ret=%d)\n", kernel_para->udevid, ret);
            return ret;
        }
    } else {
        die_num = 1; /* There's only 1, no cross-die segmentation */
    }
    *((unsigned int *)para->output) = (unsigned int)die_num;
    dms_debug("Succeeded in obtaining the aicore die num. (udevid=%u; die_num=%llu)\n", kernel_para->udevid, die_num);

    return 0;
}
#endif

#ifdef CFG_FEATURE_CPU_WORK_MODE_CONFIG
STATIC int dms_get_cpu_work_mode(const struct urd_cmd *cmd, struct urd_cmd_kernel_para *kernel_para,
                                 struct urd_cmd_para *para)
{
    unsigned long long type = 0;
    int ret = 0;

    if ((cmd == NULL) || (kernel_para == NULL) || (para == NULL)) {
        dms_err("Input urd argument is null.\n");
        return -EINVAL;
    }

    if ((para->output == NULL) || (para->output_len != sizeof(unsigned long long))) {
        dms_err("Output argument is null, or len is wrong. (output_len=%u)\n", para->output_len);
        return -EINVAL;
    }

    ret = soc_resmng_dev_get_key_value(kernel_para->phyid, "cpu_work_mode", &type);
    if (ret != 0) {
        dms_err("Failed to obtain the cpu work mode. (phyid=%u; ret=%d)\n", kernel_para->phyid, ret);
        return ret;
    }
    *((unsigned long long *)para->output) = type;
    dms_debug("Succeeded in obtaining the cpu work mode. (udevid=%u; cpu_work_mode=%llu)\n", kernel_para->udevid, type);

    return 0;
}
#endif

#ifdef CFG_FEATURE_DEVICE_PCIE_INFO
STATIC int dms_soc_get_pcie_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dmanage_pcie_id_info pcie_id_info = {0};
    struct devdrv_pcie_id_info id_info = {0};
    unsigned int dev_id, virt_id, vfid;
    int ret;

    if ((in == NULL) || (in_len != sizeof(u32))) {
        dms_err("input arg is NULL, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((out == NULL) || (out_len != sizeof(struct dmanage_pcie_id_info))) {
        dms_err("output arg is NULL, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    ret = memcpy_s((void *)&virt_id, sizeof(u32), (void *)in, in_len);
    if (ret != 0) {
        dms_err("call memcpy_s failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = dms_trans_and_check_id(virt_id, &dev_id, &vfid);
    if (ret != 0) {
        dms_err("can't transform virt id. (virt_id=%u; ret=%d)\n", virt_id, ret);
        return ret;
    }

    ret = devdrv_get_pcie_id_info(dev_id, &id_info);
    if (ret != 0) {
        dms_err("get pcie id info failed. (ret=%d)\n", ret);
        return ret;
    }

    pcie_id_info.venderid = id_info.venderid;
    pcie_id_info.subvenderid = id_info.subvenderid;
    pcie_id_info.deviceid = id_info.deviceid;
    pcie_id_info.subdeviceid = id_info.subdeviceid;
    pcie_id_info.bus = id_info.bus;
    pcie_id_info.device = id_info.device;
    pcie_id_info.fn = id_info.fn;

    ret = memcpy_s((void *)out, out_len, (void *)&pcie_id_info, sizeof(struct dmanage_pcie_id_info));
    if (ret != 0) {
        dms_err("call memcpy_s failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}
#endif

#ifdef CFG_FEATURE_COM_CPU_CONFIG
STATIC int dms_soc_get_hcom_cpu_num(const struct urd_cmd *cmd, struct urd_cmd_kernel_para *kernel_para,
                                    struct urd_cmd_para *para)
{
    struct soc_mia_res_info_ex info = {0};
    int ret = 0;

    if ((cmd == NULL) || (kernel_para == NULL) || (para == NULL)) {
        dms_err("Input urd argument is null.\n");
        return -EINVAL;
    }

    if ((para->output == NULL) || (para->output_len != sizeof(unsigned int))) {
        dms_err("Output argument is null, or len is wrong. (output_len=%u)\n", para->output_len);
        return -EINVAL;
    }

    ret = soc_resmng_dev_get_mia_res_ex(kernel_para->udevid, MIA_CPU_DEV_COMCPU, &info);
    if (ret != 0) {
        dms_err("Failed to obtain the number of hcom cpu num. (udevid=%u; ret=%d)\n", kernel_para->udevid, ret);
        return ret;
    }
    *((unsigned int *)para->output) = (unsigned int)info.total_num;
    dms_debug("Succeeded in obtaining the hcom cpu num. (udevid=%u; num=%u)\n", kernel_para->udevid, info.total_num);

    return 0;
}
#endif

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_BASIC_INFO)
BEGIN_FEATURE_COMMAND()
#ifdef CFG_HOST_ENV
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_BOARD_ID_HOST, NULL, NULL,
                    DMS_SUPPORT_ALL, dms_get_basic_info_op)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_SLOT_ID_HOST, NULL, NULL, DMS_SUPPORT_ALL,
                    dms_get_basic_info_op)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_BOM_ID_HOST, NULL, NULL, DMS_SUPPORT_ALL,
                    dms_get_basic_info_op)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_PCB_ID_HOST, NULL, NULL, DMS_SUPPORT_ALL,
                    dms_get_basic_info_op)
#endif
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_SPLIT_MODE, NULL, NULL,
                    DMS_SUPPORT_ALL, dms_get_device_split_mode)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_GET_GET_GPIO_STATUS_CMD, ZERO_CMD, NULL, NULL, DMS_SUPPORT_ROOT_ONLY,
                    dms_get_gpio_status)
#ifdef CFG_FEATURE_VDEV_MEM
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_GET_VDEVICE_INFO, NULL, "dmp_daemon",
                    DMS_SUPPORT_ALL, dms_drv_get_vdevice_info)
#endif
#ifdef CFG_FEATURE_DEV_TOPOLOGY
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_TOPOLOGY, NULL, NULL, DMS_SUPPORT_ALL,
                    dms_feature_get_dev_topology)

ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEVICES_TOPOLOGY, NULL, NULL,
                    DMS_SUPPORT_ALL, dms_feature_get_phy_devices_topology)

#endif
#if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_SRIOV)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_SRIOV_SWITCH, NULL, NULL,
                    DMS_SUPPORT_ROOT_PHY | DMS_ENV_ADMIN_DOCKER, dms_feature_set_sriov_switch)
#endif
#ifndef CFG_HOST_ENV

ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x2d", NULL,
                    DMS_SUPPORT_ALL, dms_get_sdk_ex_version)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x2d",
#ifdef CFG_BUILD_DEBUG
                    "tsdaemon,drv_tsd_daemon",
#else
                    "tsdaemon",
#endif
                    DMS_SUPPORT_ALL, dms_set_sdk_ex_version)
#endif
#ifdef CFG_FEATURE_TRS_HB_REFACTOR
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_DEV_INIT_STATUS, NULL, NULL,
                    DMS_SUPPORT_ALL_USER, dms_get_device_init_status)
#endif
#ifdef CFG_HOST_ENV
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_MASTER_DEV, NULL, NULL,
                    DMS_ACC_ALL | DMS_ENV_ALL, dms_get_master_dev)
#endif
#ifdef CFG_FEATURE_AICORE_DIE_NUM
ADD_DEV_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_AICORE_DIE_NUM, NULL, NULL,
                        DMS_SUPPORT_ALL, dms_get_aicore_die_num)
#endif
#ifdef CFG_FEATURE_CPU_WORK_MODE_CONFIG
ADD_DEV_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CPU_WORK_MODE, NULL, NULL,
                        DMS_ACC_ALL | DMS_ENV_ALL, dms_get_cpu_work_mode)
#endif
#ifdef CFG_FEATURE_DEVICE_PCIE_INFO
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_PCIE_INFO, NULL, NULL, DMS_SUPPORT_ALL,
                    dms_soc_get_pcie_info)
#endif
#ifdef CFG_FEATURE_COM_CPU_CONFIG
ADD_DEV_FEATURE_COMMAND(DMS_MODULE_BASIC_INFO, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_HCOM_CPU_NUM, NULL, NULL,
                        DMS_SUPPORT_ALL, dms_soc_get_hcom_cpu_num)
#endif
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()
