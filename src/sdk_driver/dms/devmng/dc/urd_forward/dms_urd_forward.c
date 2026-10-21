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

#include "devdrv_common.h"
#include "dms_define.h"
#include "dms_template.h"
#include "dms/dms_cmd_def.h"
#include "urd_feature.h"
#include "urd_acc_ctrl.h"
#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_uda.h"
#include "dms_hotreset.h"
#include "ascend_kernel_hal.h"
#include "devmng_forward_info.h"
#include "pbl/pbl_runenv_config.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "dpa_kernel_interface.h"

#include "dms_urd_forward.h"

#define MAX_PACKET_SIZE 300

static int dms_l2buff_m_ecc_resume_cnt[ASCEND_DEV_MAX_NUM] = {0};

STATIC int dms_send_msg_to_device_get_sdk_ex_version(void *feature, char *in, u32 in_len, char *out, u32 out_len);
STATIC int dms_send_msg_to_device_fault_event_resume(void *feature, char *in, u32 in_len, char *out, u32 out_len);
STATIC int urd_forward_ubmem_dev_repair(void *feature, char *in, u32 in_len, char *out, u32 out_len);
STATIC int urd_forward_ubmem_map_route_check(void *feature, char *in, u32 in_len, char *out, u32 out_len);
BEGIN_DMS_MODULE_DECLARATION(DMS_URD_FORWARD_CMD_NAME)
BEGIN_FEATURE_COMMAND()
#ifdef CFG_FEATURE_CC_INFO
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_CC_INFO, NULL, NULL, DMS_SUPPORT_ALL,
                    dms_send_msg_to_device_by_h2d)
#endif
#ifdef CFG_FEATURE_QUERY_FREQ_INFO
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_LPM, DMS_SUBCMD_GET_FREQUENCY, NULL, NULL, DMS_SUPPORT_ALL,
                    dms_send_msg_to_device_by_h2d)
#endif
#ifdef CFG_FEATURE_QUERY_QOS_CFG_INFO
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_QOS, DMS_SUBCMD_GET_CONFIG_INFO, NULL, NULL, DMS_SUPPORT_ALL,
                    dms_send_msg_to_device_by_h2d)
#endif
#ifdef CFG_FEATURE_GET_QOS_MASTER_CFG
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x9,info=0x3a", NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_by_h2d)
#endif
#ifdef CFG_FEATURE_QUERY_VA_INFO
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_MEMORY, DMS_SUBCMD_HBM_GET_VA, NULL, NULL, DMS_SUPPORT_ALL,
                    dms_send_msg_to_device_by_h2d_get_va)
#endif
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_MEMORY, DMS_SUBCMD_DEV_MEM_INFO, NULL, NULL, DMS_SUPPORT_ALL,
                    dms_send_msg_to_device_by_h2d)
#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_MEMORY, DMS_SUBCMD_GET_FAULT_SYSCNT, NULL, NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_ALL | DMS_VDEV_NOTSUPPORT, urd_forward_get_memory_fault_syscnt)
#endif
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_H2D_DEV_INFO, NULL, NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_by_h2d)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0xc,info=0x24", NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT,
                    dms_send_msg_to_device_by_h2d) /* LP GET AIC CPM */
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0xc,info=0x25", NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT,
                    dms_send_msg_to_device_by_h2d) /* LP GET BUS CPM */
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "module=0xc,info=0x26", NULL,
                    DMS_ACC_ROOT | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT,
                    dms_send_msg_to_device_lp_stress_set) /* LP SET STRESS TEST */
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "module=0xd,info=0x2b", NULL,
                    DMS_ACC_ALL | DMS_ENV_ALL, dms_send_msg_to_device_l2buff_comm) /* L2BUFF MULTI ECC RESUME */
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0xd,info=0x2c", NULL,
                    DMS_ACC_ALL | DMS_ENV_ALL, dms_get_l2buff_m_ecc_resume_cnt) /* GET L2BUFF MULTI ECC RESUME CNT */
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "module=0xd,info=0x41", NULL,
                    DMS_ACC_ALL | DMS_ENV_ALL, dms_send_msg_to_device_l2buff_comm) /* L2BUFF INVALID CACHE */
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x2d", NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_get_sdk_ex_version)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x2e", NULL,
                    DMS_ACC_ALL | DMS_ENV_ALL | DMS_VDEV_NOTSUPPORT, dms_send_msg_to_device_by_h2d)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x2e", NULL,
                    DMS_ACC_ROOT | DMS_ENV_PHYSICAL | DMS_VDEV_NOTSUPPORT, dms_send_msg_to_device_by_h2d)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0xf,info=0x35", NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_ALL | DMS_VDEV_NOTSUPPORT, dms_send_msg_to_device_by_h2d)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x36", NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_by_h2d)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x3b", NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_fault_event_resume)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, URD_UBMEM_REPAIR_FAULT_CMD, ZERO_CMD, NULL, NULL,
                    DMS_ACC_ROOT | DMS_ENV_PHYSICAL | DMS_VDEV_NOTSUPPORT, urd_forward_ubmem_dev_repair)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, URD_UBMEM_MAP_ROUTE_CHECK_CMD, ZERO_CMD, NULL, NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_ALL, urd_forward_ubmem_map_route_check)
#ifdef CFG_FEATURE_UPGRADE_SWPLUGIN_POLICY
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x3e", NULL,
                    DMS_SUPPORT_ALL, dms_send_msg_to_device_by_h2d)
ADD_FEATURE_COMMAND(DMS_URD_FORWARD_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x3e", NULL,
                    DMS_ACC_ROOT | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT, dms_send_msg_to_device_by_h2d)
#endif
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_urd_forward_init(void)
{
    dms_info("dms urd_forward init.\n");
    CALL_INIT_MODULE(DMS_URD_FORWARD_CMD_NAME);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_urd_forward_init, FEATURE_LOADER_STAGE_5);

void dms_urd_forward_uninit(void)
{
    dms_info("dms urd_forward uninit.\n");
    CALL_EXIT_MODULE(DMS_URD_FORWARD_CMD_NAME);
}
DECLAER_FEATURE_AUTO_UNINIT(dms_urd_forward_uninit, FEATURE_LOADER_STAGE_5);

int dms_urd_forward_dev_init(u32 udevid)
{
    if (udevid < ASCEND_DEV_MAX_NUM) {
        dms_l2buff_m_ecc_resume_cnt[udevid] = 0;
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(dms_urd_forward_dev_init, FEATURE_LOADER_STAGE_5);

void dms_urd_forward_dev_uninit(u32 udevid)
{
    if (udevid < ASCEND_DEV_MAX_NUM) {
        dms_l2buff_m_ecc_resume_cnt[udevid] = 0;
    }
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(dms_urd_forward_dev_uninit, FEATURE_LOADER_STAGE_5);

STATIC int dms_urd_forward_para_check(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    if (feature == NULL) {
        dms_err("feature is NULL.");
        return -EINVAL;
    }
    if (in == NULL || in_len < sizeof(u32)) {
        dms_err("Input data is null or input data length is not enough. (in%s; in_len=%u)\n",
                in == NULL ? "=NULL" : "!=NULL", in_len);
        return -EINVAL;
    }

    if (out == NULL || out_len < sizeof(char)) {
        dms_err("Output data is null or output data length is not enough. (out%s; out_len=%u)\n",
                out == NULL ? "=NULL" : "!=NULL", out_len);
        return -EINVAL;
    }

    return 0;
}

int dms_set_urd_msg(DMS_FEATURE_S *feature_cfg, char *in, u32 in_len, u32 out_len, struct urd_forward_msg *urd_msg)
{
    int ret;

    urd_msg->main_cmd = feature_cfg->main_cmd;
    urd_msg->sub_cmd = feature_cfg->sub_cmd;
    if (feature_cfg->filter != NULL) {
        urd_msg->filter_len = ka_base_strlen(feature_cfg->filter);
        if (urd_msg->filter_len > FILTER_LEN_MAX) {
            dms_err("Invalid parameter, filter is oversized. (filter_len=%u)\n", urd_msg->filter_len);
            return -EINVAL;
        }
        if (urd_msg->filter_len != 0) {
            ret = strcpy_s(urd_msg->filter, FILTER_LEN_MAX, feature_cfg->filter);
            if (ret != 0) {
                dms_err("strcpy_s failed. (ret=%d)\n", ret);
                return ret;
            }
        }
    }

    if (in != NULL) {
        ret = memcpy_s((void *)&(urd_msg->payload[0]), PAYLOAD_LEN_MAX, (void *)in, in_len);
        if (ret != 0) {
            dms_err("memcpy_s failed. (size=%d, ret=%d)\n", in_len, ret);
            return ret;
        }
    } else if (in_len != 0) {
        dms_err("Invalid parameter, in is NULL but in_len is not 0. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    urd_msg->payload_len = in_len;
    urd_msg->output_len = out_len;
    return 0;
}

int dms_urd_forward_send_to_device(u32 phy_id, u32 vfid, struct urd_forward_msg *urd_msg, char *out, u32 out_len)
{
    int ret;

    ret = devdrv_manager_h2d_sync_urd_forward(phy_id, vfid, urd_msg);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "devdrv_manager_h2d_sync_urd_forward failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }

    if (urd_msg->payload_len > out_len) {
        dms_err("size of payload is bigger than out_buff. (phy_id=%u; payload_len=%u; out_len=%u)\n", phy_id,
                urd_msg->payload_len, out_len);
        return -EINVAL;
    }
    ret = memcpy_s((void *)out, out_len, (void *)&(urd_msg->payload[0]), urd_msg->payload_len);
    if (ret != 0) {
        dms_err("memcpy_s failed. (phy_id=%u; ret=%d; out_len=%u)\n", phy_id, ret, out_len);
    }

    return ret;
}

int dms_send_msg_to_device_by_h2d(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    DMS_FEATURE_S *feature_cfg = NULL;
    struct devdrv_info *dev_info = NULL;
    struct urd_forward_msg urd_msg = {0};
    u32 dev_id;
    u32 phys_id = 0, vfid = 0;
    int ret = 0;

    ret = dms_urd_forward_para_check(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_err("Para check failed.\n");
        return ret;
    }

    feature_cfg = (DMS_FEATURE_S *)feature;
    dev_id = *((u32 *)in);
    ret = uda_devid_to_phy_devid(dev_id, &phys_id, &vfid);
    if (ret != 0) {
        dms_err("Transform virt id failed. (dev_id=%u, ret=%d)\n", dev_id, ret);
        return ret;
    }

    dev_info = devdrv_manager_get_devdrv_info(phys_id);
    if (dev_info == NULL) {
        dms_err("Device is not initialized. (phys_id=%u)\n", phys_id);
        return -ENODEV;
    }

    ret = dms_hotreset_task_cnt_increase(phys_id);
    if (ret != 0) {
        dms_err("Hotreset task cnt increase failed. (phys_id=%u; ret=%d)\n", phys_id, ret);
        return ret;
    }

    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        dms_warn("Device has been reset. (phy_id=%u)\n", phys_id);
        ret = -EINVAL;
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

    ret = dms_set_urd_msg(feature_cfg, in, in_len, out_len, &urd_msg);
    if (ret != 0) {
        dms_err("dms_set_urd_msg failed. (phy_id=%u; ret=%d)\n", phys_id, ret);
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

    ret = dms_urd_forward_send_to_device(phys_id, vfid, &urd_msg, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret,
                              "Failed to send device by urd_forward. "
                              "(phys_id=%u; ret=%d; main_cmd=%u; sub_cmd=%u)\n",
                              phys_id, ret, urd_msg.main_cmd, urd_msg.sub_cmd);
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

OCCUPY_AND_TASK_CNT_OUT:
    ka_base_atomic_dec(&dev_info->occupy_ref);
    dms_hotreset_task_cnt_decrease(phys_id);
    return ret;
}

STATIC int urd_forward_ubmem_dev_repair(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct ubmem_dev_repair_msg_in *msg = (struct ubmem_dev_repair_msg_in *)in;
    int master_tgid, cp1_tgid, ret;
    u32 devid, chip_type;
    u32 udevid = 0;

    if ((feature == NULL) || (in == NULL) || (in_len < sizeof(struct ubmem_dev_repair_msg_in)) || (out == NULL) ||
        (out_len < sizeof(struct ubmem_dev_repair_msg_out))) {
        dms_err("Invalid para. (feature=%d; in=%d; in_len=%u; out=%d; out_len=%u)\n", feature != NULL, in != NULL,
                in_len, out != NULL, out_len);
        return -EINVAL;
    }

    devid = msg->udevid;
    ret = uda_devid_to_udevid(devid, &udevid);
    if (ret != 0) {
        dms_err("Transform devid to udevid failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    if (uda_is_phy_dev(udevid) == false) {
        dms_debug("Udevid is not physical device. (devid=%u; udevid=%u)\n", devid, udevid);
        return -EOPNOTSUPP;
    }

    chip_type = uda_get_chip_type(udevid);
    if ((chip_type != HISI_CLOUD_V4) && (chip_type != HISI_MINI_V4)) {
        dms_debug("Chip type is not supported. (udevid=%u; chip_type=%u)\n", udevid, chip_type);
        return -EOPNOTSUPP;
    }

    master_tgid = ka_task_get_current_tgid();
    ret = hal_kernel_apm_query_slave_tgid_by_master(master_tgid, udevid, PROCESS_CP1, &cp1_tgid);
    if (ret != 0) {
        dms_err("Query cp1 tgid failed. (udevid=%u; master_tgid=%d; ret=%d)\n", udevid, master_tgid, ret);
        return ret;
    }

    msg->devid = devid;
    msg->udevid = udevid;
    msg->cp1_tgid = cp1_tgid;

    ret = dms_send_msg_to_device_by_h2d(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "ubmem repair send to device failed. (udevid=%u; cp1_tgid=%d; ret=%d)\n", udevid,
                              cp1_tgid, ret);
    }

    return ret;
}

STATIC int urd_forward_ubmem_map_route_check(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct ubmem_map_route_check_msg_in *msg_in = (struct ubmem_map_route_check_msg_in *)in;
    int ret;

    if ((feature == NULL) || (in == NULL) || (in_len < sizeof(struct ubmem_map_route_check_msg_in)) || (out == NULL) ||
        (out_len < sizeof(struct ubmem_map_route_check_msg_out))) {
        dms_err("Invalid para. (feature=%d; in=%d; in_len=%u)\n", feature != NULL, in != NULL, in_len);
        return -EINVAL;
    }

    ret = dms_send_msg_to_device_by_h2d(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "ubmem send to device failed. (devid=%u; ret=%d)\n", msg_in->dst_devid, ret);
    }

    return ret;
}

int dms_send_msg_to_device_by_h2d_kernel(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    DMS_FEATURE_S *feature_cfg = NULL;
    struct devdrv_info *dev_info = NULL;
    struct urd_forward_msg urd_msg = {0};
    u32 dev_id;
    int ret = 0;

    ret = dms_urd_forward_para_check(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_err("Para check failed.\n");
        return ret;
    }

    feature_cfg = (DMS_FEATURE_S *)feature;
    dev_id = *((u32 *)in);
    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    ret = dms_hotreset_task_cnt_increase(dev_id);
    if (ret != 0) {
        dms_err("Hotreset task cnt increase failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        dms_warn("Device has been reset. (dev_id=%u)\n", dev_id);
        ret = -EINVAL;
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

    ret = dms_set_urd_msg(feature_cfg, in, in_len, out_len, &urd_msg);
    if (ret != 0) {
        dms_err("dms_set_urd_msg failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

    ret = dms_urd_forward_send_to_device(dev_id, 0, &urd_msg, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret,
                              "Failed to send device by urd_forward. "
                              "(dev_id=%u; ret=%d; main_cmd=%u; sub_cmd=%u)\n",
                              dev_id, ret, urd_msg.main_cmd, urd_msg.sub_cmd);
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

OCCUPY_AND_TASK_CNT_OUT:
    ka_base_atomic_dec(&dev_info->occupy_ref);
    dms_hotreset_task_cnt_decrease(dev_id);
    return ret;
}

int dms_send_msg_to_device_by_h2d_get_va(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct hbm_pa_va_info *va_info = NULL;
    int ret = 0;

    if (in == NULL || in_len < sizeof(unsigned int) + sizeof(unsigned int)) {
        dms_err("Invalid parameter. (in_is_null=%d; in_len=%u)\n", (in == NULL), in_len);
        return -EINVAL;
    }

    va_info = (struct hbm_pa_va_info *)in;
    va_info->host_pid = ka_task_get_current_tgid();

    ret = dms_send_msg_to_device_by_h2d(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to send msg to device. (ret=%d)\n", ret);
    }
    return ret;
}

struct lpm_soc_stress_hal_cfg_in {
    unsigned int pid;
    unsigned int is_offline;
};
int dms_send_msg_to_device_lp_stress_set(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_hal_device_info_stru *forward_stru = NULL;
    struct lpm_soc_stress_hal_cfg_in *lp_stress_set = NULL;
    int ret = 0;

    if (feature == NULL || in == NULL ||
        in_len < (DMS_HAL_DEV_INFO_HEAD_LEN + sizeof(struct lpm_soc_stress_hal_cfg_in))) {
        dms_err("param invalid.(feature=%d; in=%d; in_len=%u)\n", feature != NULL, in != NULL, in_len);
        return -EINVAL;
    }

    forward_stru = (struct dms_hal_device_info_stru *)in;
    lp_stress_set = (struct lpm_soc_stress_hal_cfg_in *)forward_stru->payload;
    lp_stress_set->pid = ka_task_get_current_tgid();
    lp_stress_set->is_offline = 1;
    ret = dms_send_msg_to_device_by_h2d(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "dms_send_msg_to_device_by_h2d failed. ret=%d\n", ret);
    }
    return ret;
}

#define DMS_INFO_TYPE_L2BUFF_RESUME 0x2b /* INFO_TYPE_L2BUFF_RESUME */
int dms_send_msg_to_device_l2buff_comm(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_hal_device_info_stru *forward_stru = NULL;
    u32 phy_devid, vfid;
    int ret = 0;

    if ((feature == NULL) || (in == NULL) || (in_len < (DMS_HAL_DEV_INFO_HEAD_LEN + sizeof(int)))) {
        dms_err("Params invalid. (feature=%d; in=%d; in_len=%u)\n", feature != NULL, in != NULL, in_len);
        return -EINVAL;
    }

    forward_stru = (struct dms_hal_device_info_stru *)in;
    ret = uda_devid_to_phy_devid(forward_stru->dev_id, &phy_devid, &vfid);
    if (ret != 0) {
        dms_err("Transform udev id failed. (dev_id=%u, ret=%d)\n", forward_stru->dev_id, ret);
        return ret;
    }

    if (phy_devid >= ASCEND_DEV_MAX_NUM) {
        dms_err("Phy_devid invalid. (phy_devid=%u; max=%d)\n", phy_devid, ASCEND_DEV_MAX_NUM);
        return -EINVAL;
    }

    dms_debug("Cfg. (devid=%u, phy_devid=%u; type=%d)\n", forward_stru->dev_id, phy_devid, forward_stru->info_type);

    ret = dms_send_msg_to_device_by_h2d(feature, in, in_len, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "dms_send_msg_to_device_by_h2d failed. ret=%d\n", ret);
        return ret;
    }

    if (forward_stru->info_type == DMS_INFO_TYPE_L2BUFF_RESUME) {
        dms_l2buff_m_ecc_resume_cnt[phy_devid] = 1;
    }

    return 0;
}

int dms_get_l2buff_m_ecc_resume_cnt(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_hal_device_info_stru *forward_stru = NULL;
    u32 phy_devid, vfid;
    int ret = 0;

    if ((feature == NULL) || (in == NULL) || (out == NULL) || (in_len < (DMS_HAL_DEV_INFO_HEAD_LEN + sizeof(int))) ||
        (out_len < (DMS_HAL_DEV_INFO_HEAD_LEN + sizeof(int)))) {
        dms_err("Param invalid. (feature=%d; in=%d; in_len=%u; out=%d, out_len=%u)\n", feature != NULL, in != NULL,
                in_len, out != NULL, out_len);
        return -EINVAL;
    }

    forward_stru = (struct dms_hal_device_info_stru *)in;
    ret = uda_devid_to_phy_devid(forward_stru->dev_id, &phy_devid, &vfid);
    if (ret != 0) {
        dms_err("Transform udev id failed. (dev_id=%u, ret=%d)\n", forward_stru->dev_id, ret);
        return ret;
    }

    if (phy_devid >= ASCEND_DEV_MAX_NUM) {
        dms_err("Devid invalid. (udevid=%u; max=%d)\n", phy_devid, ASCEND_DEV_MAX_NUM);
        return -EINVAL;
    }

    forward_stru = (struct dms_hal_device_info_stru *)out;
    *((u32 *)(forward_stru->payload)) = dms_l2buff_m_ecc_resume_cnt[phy_devid];
    forward_stru->buff_size = sizeof(u32);
    return ret;
}

#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
int urd_forward_get_memory_fault_syscnt(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct memory_fault_timestamp *mem_para = NULL;
    int ret;

    if (feature == NULL || in == NULL || out == NULL) {
        dms_err("NULL pointer. (feature=%d; in=%d; out=%d)\n", feature != NULL, in != NULL, out != NULL);
        return -EINVAL;
    }

    if (in_len != sizeof(struct memory_fault_timestamp) || out_len != sizeof(struct memory_fault_timestamp)) {
        dms_err("Invalid len. (in_len=%u; expected=%lu; out_len=%u; expected=%lu)\n", in_len,
                sizeof(struct memory_fault_timestamp), out_len, sizeof(struct memory_fault_timestamp));
        return -EINVAL;
    }

    mem_para = (struct memory_fault_timestamp *)in;
    mem_para->host_pid = ka_task_get_current_tgid();

    ret = dms_send_msg_to_device_by_h2d(feature, (char *)in, in_len, (char *)out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to send msg to device. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}
#endif

STATIC int dms_send_msg_to_device_get_sdk_ex_version(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
#ifdef CFG_FEATURE_SDK_EX_VERSION
    int ret;
    unsigned int udevid, vf_id, soc_type;
    struct dms_hal_device_info_stru *cfg_in = NULL;

    if ((feature == NULL) || (in == NULL) || (in_len != (DMS_HAL_DEV_INFO_HEAD_LEN + DMS_SDK_EX_VERSION_LEN_MAX)) ||
        (out == NULL) || (out_len != (DMS_HAL_DEV_INFO_HEAD_LEN + DMS_SDK_EX_VERSION_LEN_MAX))) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s; out_len=%u)\n",
                (feature == NULL) ? "NULL" : "OK", (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK",
                out_len);
        return -EINVAL;
    }

    cfg_in = (struct dms_hal_device_info_stru *)in;
    if (cfg_in->buff_size != DMS_SDK_EX_VERSION_LEN_MAX) {
        dms_err("Invalid parameter. (buff_size=%u; buff_max_len=%u)\n", cfg_in->buff_size, DMS_SDK_EX_VERSION_LEN_MAX);
        return -EINVAL;
    }

    ret = uda_devid_to_phy_devid(cfg_in->dev_id, &udevid, &vf_id);
    if (ret != 0) {
        dms_err("Failed to convert the logical_id to the physical_id. (logical_id=%u; ret=%d)\n", cfg_in->dev_id, ret);
        return -EINVAL;
    }

    /* 310B & 310P & 910 not support vf */
    soc_type = uda_get_chip_type(udevid);
    if ((soc_type == HISI_CLOUD_V1) || (soc_type == HISI_MINI_V2) || (soc_type == HISI_MINI_V3)) {
        if ((vf_id != 0) || !uda_is_phy_dev(udevid)) {
            return -EOPNOTSUPP;
        }
    }

    return dms_send_msg_to_device_by_h2d(feature, in, in_len, out, out_len);
#else
    (void)feature;
    (void)in;
    (void)in_len;
    (void)out;
    (void)out_len;
    return -EOPNOTSUPP;
#endif
}

int dms_send_msg_to_device_by_h2d_multi_packets(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int i = 0;
    int ret = 0;
    int start = 0;
    int len = 0;
    int total_packets;
    struct dms_set_device_info_in *cfg_in = NULL;
    struct dms_set_device_info_in_multi_packet cfg_in_temp;

    cfg_in = (struct dms_set_device_info_in *)in;
    dms_info("dms_send_msg_to_device_by_h2d_multi_packets buff size=%d.\n", cfg_in->buff_size);

    total_packets = (cfg_in->buff_size + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
    dms_info("Set sign cert size =%d, packets=%d.\n", cfg_in->buff_size, total_packets);
    for (i = 0; i < total_packets; i++) {
        start = i * MAX_PACKET_SIZE;
        len = (start + MAX_PACKET_SIZE > cfg_in->buff_size) ? (cfg_in->buff_size - start) : MAX_PACKET_SIZE;
        dms_info("Set sign cert start =%d, len=%d.\n", start, len);
        cfg_in_temp.dev_id = cfg_in->dev_id;
        cfg_in_temp.sub_cmd = cfg_in->sub_cmd;
        cfg_in_temp.buff_size = len;
        cfg_in_temp.current_packet = i;
        cfg_in_temp.total_size = cfg_in->buff_size;
        memset_s(cfg_in_temp.buff, MAX_PACKET_SIZE, 0, MAX_PACKET_SIZE);
        ret = ka_base_copy_from_user(cfg_in_temp.buff, cfg_in->buff + start, len);
        if (ret != 0) {
            dms_err("Copy from user fail.(ret=%d;param_size=%u)\n", ret, len);
            return -EINVAL;
        }
        dms_info("Set sign cert current_packet=%d, buff_size=%d, total_size=%d.\n", cfg_in_temp.current_packet,
                 cfg_in_temp.buff_size, cfg_in_temp.total_size);

        ret = dms_send_msg_to_device_by_h2d(feature, (char *)&cfg_in_temp, sizeof(cfg_in_temp), (char *)&cfg_in_temp,
                                            sizeof(cfg_in_temp));
        if (ret != 0) {
            return ret;
        }
    }
    return ret;
}

int dms_send_msg_to_device_by_h2d_multi_packets_kernel(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int i = 0;
    int ret = 0;
    int start = 0;
    int len = 0;
    int total_packets;
    struct dms_set_device_info_in *cfg_in = NULL;
    struct dms_set_device_info_in_multi_packet cfg_in_temp;

    cfg_in = (struct dms_set_device_info_in *)in;

    total_packets = (cfg_in->buff_size + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
    for (i = 0; i < total_packets; i++) {
        start = i * MAX_PACKET_SIZE;
        len = (start + MAX_PACKET_SIZE > cfg_in->buff_size) ? (cfg_in->buff_size - start) : MAX_PACKET_SIZE;
        cfg_in_temp.dev_id = cfg_in->dev_id;
        cfg_in_temp.sub_cmd = cfg_in->sub_cmd;
        cfg_in_temp.buff_size = len;
        cfg_in_temp.current_packet = i;
        cfg_in_temp.total_size = cfg_in->buff_size;
        memset_s(cfg_in_temp.buff, MAX_PACKET_SIZE, 0, MAX_PACKET_SIZE);
        ret = memcpy_s(cfg_in_temp.buff, MAX_PACKET_SIZE, cfg_in->buff + start, len);
        if (ret != 0) {
            dms_err("Memcpy fail.(ret=%d;param_size=%u)\n", ret, len);
            return -EINVAL;
        }
        ret = dms_send_msg_to_device_by_h2d_kernel(feature, (char *)&cfg_in_temp, sizeof(cfg_in_temp),
                                                   (char *)&cfg_in_temp, sizeof(cfg_in_temp));
        if (ret != 0) {
            return ret;
        }
    }
    return ret;
}

struct fault_event_resume {
    unsigned int event_id;
    unsigned short node_type;
    unsigned char node_id;
    unsigned char resv[9];
};

unsigned int fault_event_resume_whitelist[] = {0x81AF8009};

STATIC bool event_in_whiltlist(unsigned int event_id)
{
    int i, len;

    len = sizeof(fault_event_resume_whitelist) / sizeof(unsigned int);
    for (i = 0; i < len; i++) {
        if (event_id == fault_event_resume_whitelist[i]) {
            return true;
        }
    }
    return false;
}

STATIC void make_up_filter_fault_event(struct dms_filter_st *filter, int module_type, int info_type,
                                       unsigned short node_type)
{
    filter->filter_len = (unsigned int)sprintf_s(filter->filter, sizeof(filter->filter),
                                                 "module=0x%x,info=0x%x,node_type=0x%x", (unsigned int)module_type,
                                                 (unsigned int)info_type, (unsigned int)node_type);
}

STATIC int dms_send_msg_to_device_fault_event_resume(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    DMS_FEATURE_S *feature_cfg = NULL;
    struct dms_filter_st filter = {0};
    struct dms_hal_device_info_stru *info_in = NULL;
    int ret;

    if ((feature == NULL) || (in == NULL) || (in_len < DMS_HAL_DEV_INFO_HEAD_LEN + sizeof(struct fault_event_resume))) {
        dms_err("Feature or in are NULL, or in_len is invalid.(feature=%s; in=%s; in_len=%u)\n",
                (feature == NULL) ? "NULL" : "OK", (in == NULL) ? "NULL" : "OK", in_len);
        return -EINVAL;
    }
    info_in = (struct dms_hal_device_info_stru *)in;

    if (!event_in_whiltlist(((struct fault_event_resume *)(info_in->payload))->event_id)) {
        if (ka_task_get_current_cred_euid() != 0) {
            dms_debug("Event_id is not in whitelist, and it cannot be invoked by non-root user.\n");
            return -EOPNOTSUPP;
        }
    }

    make_up_filter_fault_event(&filter, info_in->module_type, info_in->info_type,
                               ((struct fault_event_resume *)(info_in->payload))->node_type);

    feature_cfg = (DMS_FEATURE_S *)feature;
    feature_cfg->filter = (char *)&filter;
    ret = dms_send_msg_to_device_by_h2d(feature_cfg, in, in_len, out, out_len);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "dms_send_msg_to_device_by_h2d failed. (dev_id=%u; ret=%d)\n", info_in->dev_id, ret);
        return ret;
    }

    return 0;
}