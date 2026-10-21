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
#include <dlfcn.h>
#ifdef CFG_FEATURE_SUPPORT_UB
#include "urma_types.h"
#include "urma_api.h"
#endif
#ifdef CFG_FEATURE_SUPPORT_MAMI
#include "mami_api.h"
#endif
#include "securec.h"
#include "devmng_common.h"
#include "dms_user_common.h"
#include "dms/dms_misc_interface.h"
#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "dms/dms_devdrv_info_comm.h"
#include "dsmi_common_interface.h"
#include "dms_cmd_def.h"
#include "dmc_user_interface.h"
#include "dms_device_info.h"

#ifndef STATIC
#define STATIC static
#endif

#define DMS_MAX_DEV_NUM (64U + 2U) // 64 means pf, but for 64+1, dev_num may add 2
struct dms_eid_query_info g_ub_dev_info[DMS_MAX_DEV_NUM] = {0};
struct dms_ub_dev_info g_ub_urma_dev_info[DMS_MAX_DEV_NUM] = {0};

pthread_mutex_t ub_dev_info_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ub_token_val_lock = PTHREAD_MUTEX_INITIALIZER;

int dms_get_urma_name_by_devid(u32 dev_id, char *name, u32 len)
{
    struct dms_ioctl_arg ioarg = {0};
    int ret;

    if (!name || len == 0) {
        DMS_ERR("Name is null or len is 0. (len=%u)\n", len);
        return DRV_ERROR_PARA_ERROR;
    }

    ioarg.input = (void *)&dev_id;
    ioarg.input_len = sizeof(u32);
    ioarg.output = (void *)name;
    ioarg.output_len = len;
    ioarg.main_cmd = ASCEND_UB_CMD_BASIC;
    ioarg.sub_cmd = ASCEND_UB_SUBCMD_GET_URMA_NAME;
    ioarg.filter_len = 0;

    ret = errno_to_user_errno(DmsIoctl(DMS_IOCTL_CMD, &ioarg));
    if (ret == DRV_ERROR_NO_DEVICE) {
        DMS_WARN("IOCTL no dev, please retry. (ret=%d)\n", ret);
        return DRV_ERROR_NO_DEVICE;
    } else if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "IOCTL failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

#ifdef CFG_FEATURE_SUPPORT_UB
void dms_copy_urma_eid_from_addr_info(urma_eid_t *eid, union dms_urma_addr_info *addr_info)
{
    int i;

    for (i = 0; i < URMA_EID_SIZE; i++) {
        eid->raw[i] = addr_info->eid[i];
    }

    return;
}

STATIC int dms_cmp_urma_eid(urma_eid_t *eid1, urma_eid_t *eid2)
{
    int i;

    for (i = 0; i < URMA_EID_SIZE; i++) {
        if (eid1->raw[i] != eid2->raw[i]) {
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    return 0;
}

STATIC int dms_fill_eid_index_and_urma_dev(struct dms_eid_query_info *query_info, struct dms_ub_dev_info *eid_info,
                                           uint32_t dev_id)
{
    urma_device_t *urma_dev = NULL;
    urma_eid_t eid;
    urma_eid_info_t *eid_list;
    uint32_t eid_cnt, i;

    dms_copy_urma_eid_from_addr_info(&eid, &query_info->local_eid[query_info->min_idx]);
    urma_dev = urma_get_device_by_eid(eid, URMA_TRANSPORT_UB);
    if (urma_dev == NULL) {
        DMS_ERR("urma_get_device_by_eid failed.(devid=%u)\n", dev_id);
        return DRV_ERROR_NO_RESOURCES;
    }

    eid_list = urma_get_eid_list(urma_dev, &eid_cnt);
    if (eid_list == NULL) {
        DMS_ERR("urma_get_eid_list failed.(devid=%u)\n", dev_id);
        return DRV_ERROR_NO_RESOURCES;
    }

    for (i = 0; i < eid_cnt; i++) {
        if (dms_cmp_urma_eid(&eid_list[i].eid, &eid) == 0) {
            eid_info->eid_index[0] = eid_list[i].eid_index;
            break;
        }
    }

    if (i == eid_cnt) {
        DMS_ERR("urma_dev find eid_index failed.(devid=%u; eid=" EID_FMT ")\n", dev_id, EID_ARGS(eid));
        urma_free_eid_list(eid_list);
        return DRV_ERROR_NO_RESOURCES;
    }

    eid_info->urma_dev[0] = (void *)urma_dev;
    urma_free_eid_list(eid_list);
    return 0;
}

STATIC void dms_fill_ub_dev_info(unsigned int dev_id, struct dms_ub_dev_info *eid_info, int *num)
{
    eid_info->eid_index[0] = g_ub_urma_dev_info[dev_id].eid_index[0];
    eid_info->urma_dev[0] = g_ub_urma_dev_info[dev_id].urma_dev[0];
    *num = 1;
}

STATIC int dms_query_ub_eid_info(unsigned int dev_id, struct dms_eid_query_info *query_info)
{
    struct urd_cmd_para cmd_para = {0};
    struct urd_cmd cmd = {0};
    int ret;

    urd_usr_cmd_fill(&cmd, ASCEND_UB_CMD_BASIC, ASCEND_UB_SUBCMD_GET_EID_INDEX, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&dev_id, sizeof(unsigned int), (void *)query_info,
                          sizeof(struct dms_eid_query_info));
    ret = urd_dev_usr_cmd(dev_id, &cmd, &cmd_para);
    if ((ret != 0) || (query_info->num > DMS_MAX_EID_PAIR_NUM) || (query_info->num <= 0) ||
        (query_info->min_idx >= query_info->num)) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get ub dev info failed. (dev_id=%u; ret=%d; num=%d; min_idx=%d)\n", dev_id, ret,
                              query_info->num, query_info->min_idx);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return 0;
}

#endif

drvError_t dms_get_ub_dev_info(unsigned int dev_id, struct dms_ub_dev_info *eid_info, int *num)
{
#ifdef CFG_FEATURE_SUPPORT_UB
    struct dms_eid_query_info query_info = {0};
    unsigned int i;
    int ret;

    if ((eid_info == NULL) || (num == NULL)) {
        DMS_ERR("eid_info or num is Null.(devid=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (dev_id >= DMS_MAX_DEV_NUM) {
        DMS_ERR("dev_id is invalid.(devid=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    pthread_mutex_lock(&ub_dev_info_lock);
    if (g_ub_dev_info[dev_id].num != 0) {
        dms_fill_ub_dev_info(dev_id, eid_info, num);
        pthread_mutex_unlock(&ub_dev_info_lock);
        // Resource has been initialized, only need to fill in eid_info
        return 0;
    }

    ret = dms_query_ub_eid_info(dev_id, &query_info);
    if (ret != 0) {
        pthread_mutex_unlock(&ub_dev_info_lock);
        *num = 0;
        return ret;
    }

    g_ub_dev_info[dev_id].num = query_info.num;
    g_ub_dev_info[dev_id].min_idx = query_info.min_idx;
    for (i = 0; i < query_info.num; i++) {
        (void)memcpy_s(&g_ub_dev_info[dev_id].local_eid[i].eid, sizeof(union dms_urma_addr_info),
                       &query_info.local_eid[i].eid, sizeof(union dms_urma_addr_info));
    }
    ret = dms_fill_eid_index_and_urma_dev(&g_ub_dev_info[dev_id], &g_ub_urma_dev_info[dev_id], dev_id);
    if (ret != 0) {
        g_ub_dev_info[dev_id].num = 0;
        *num = 0;
    } else {
        dms_fill_ub_dev_info(dev_id, eid_info, num);
    }
    pthread_mutex_unlock(&ub_dev_info_lock);
    return ret;
#else
    (void)dev_id;
    (void)eid_info;
    (void)num;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

unsigned int g_share_token_val[DMS_MAX_DEV_NUM] = {0};
#define DMS_INVALID_TOKEN_VAL 0
drvError_t dms_get_token_val(unsigned int dev_id, unsigned int type, unsigned int *val)
{
    struct urd_cmd_para cmd_para = {0};
    struct urd_cmd cmd = {0};
    unsigned int tmp_val;
    int ret;

    if ((type >= TOKEN_VAL_TYPE_MAX) || (val == NULL)) {
        DMS_ERR("Type is invalid or val is Null.(type=%u; devid=%u)\n", type, dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (dev_id >= DMS_MAX_DEV_NUM) {
        DMS_ERR("dev_id is invalid.(devid=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    pthread_mutex_lock(&ub_token_val_lock);
    if ((g_share_token_val[dev_id] != DMS_INVALID_TOKEN_VAL) && (type == SHARED_TOKEN_VAL)) {
        *val = g_share_token_val[dev_id];
        pthread_mutex_unlock(&ub_token_val_lock);
        return DRV_ERROR_NONE;
    }

    urd_usr_cmd_fill(&cmd, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_TOKEN_VAL, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&dev_id, sizeof(unsigned int), (void *)&tmp_val, sizeof(unsigned int));
    ret = urd_dev_usr_cmd(dev_id, &cmd, &cmd_para);
    if (ret != 0) {
        pthread_mutex_unlock(&ub_token_val_lock);
        DMS_EX_NOTSUPPORT_ERR(ret, "Get token_val failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        *val = 0;
        return DRV_ERROR_IOCRL_FAIL;
    }

    if ((type == SHARED_TOKEN_VAL) && (g_share_token_val[dev_id] == DMS_INVALID_TOKEN_VAL)) {
        g_share_token_val[dev_id] = tmp_val;
    }

    *val = tmp_val;
    pthread_mutex_unlock(&ub_token_val_lock);
    return DRV_ERROR_NONE;
}

drvError_t DmsGetUbInfo(unsigned int dev_id, int module_type, int info_type, void *buf, unsigned int *size)
{
    struct dms_hal_device_info_stru in = {0};
    struct dms_filter_st filter = {0};
    struct urd_cmd_para cmd_para = {0};
    struct urd_cmd cmd = {0};
    int ret;

    if ((buf == NULL) || (size == NULL) || (*size == 0)) {
        return DRV_ERROR_PARA_ERROR;
    }

    DMS_MAKE_UP_FILTER_HAL_DEV_INFO_EX(&filter, module_type, info_type);

    in.dev_id = dev_id;
    in.module_type = module_type;
    in.info_type = info_type;
    in.buff_size = 0;

    urd_usr_cmd_fill(&cmd, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, &filter.filter[0], filter.filter_len);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&in, DMS_HAL_DEV_INFO_HEAD_LEN, buf, *size);
    ret = urd_dev_usr_cmd_ex(dev_id, URD_QUERY_IOCTL_CMD, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get ub_info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    return ret;
}

drvError_t dms_get_ub_info(uint32_t devId, int32_t infoType, void *buf, int32_t *size)
{
    int ret;

    switch (infoType) {
        case INFO_TYPE_UB_STATUS:
            ret = DmsHalGetDeviceInfoEx(devId, MODULE_TYPE_UB, infoType, buf, (unsigned int *)size);
            break;

        case INFO_TYPE_UB_PACKET_STATISTICS:
            ret = DmsGetUbInfo(devId, MODULE_TYPE_UB, infoType, buf, (unsigned int *)size);
            break;

        case INFO_TYPE_UB_QOS_INFO:
            ret = DmsGetUbInfo(devId, MODULE_TYPE_UB, infoType, buf, (unsigned int *)size);
            break;

        case INFO_TYPE_UB_CONFIG_INFO:
            ret = DmsGetUbInfo(devId, MODULE_TYPE_UB, infoType, buf, (unsigned int *)size);
            break;

        default:
            return DRV_ERROR_NOT_SUPPORT;
    }

    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get ub info. (dev_id=%u, infoType=%d, ret=%d)\n", devId, infoType,
                                     ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

STATIC int dms_get_ub_d2d_eid_info(unsigned int devid, struct dms_d2d_eid_input *d2d_eid_in,
                                   struct dms_d2d_eid_output *d2d_eid_out, struct dms_ub_d2d_eid *d2d_eid,
                                   unsigned int *eid_index)
{
#if (defined CFG_FEATURE_SUPPORT_UB) && (!defined DRV_HOST)
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    urd_usr_cmd_fill(&cmd, ASCEND_UB_CMD_BASIC, ASCEND_UB_SUBCMD_GET_D2D_EID, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)d2d_eid_in, sizeof(struct dms_d2d_eid_input), (void *)d2d_eid_out,
                          sizeof(struct dms_d2d_eid_output));
    ret = urd_dev_usr_cmd(devid, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get d2d_eid failed. (dev_id=%u; ret=%d)\n", devid, ret);
        return (ret == DRV_ERROR_NOT_SUPPORT) ? DRV_ERROR_NOT_SUPPORT : DRV_ERROR_IOCRL_FAIL;
    }

    (void)memcpy_s(d2d_eid, sizeof(struct dms_ub_d2d_eid), &d2d_eid_out->d2d_eid, sizeof(struct dms_ub_d2d_eid));
    *eid_index = d2d_eid_out->eid_index;

    return 0;
#else
    (void)devid;
    (void)d2d_eid_in;
    (void)d2d_eid_out;
    (void)d2d_eid;
    (void)eid_index;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t dms_get_ub_d2d_eid_by_type(unsigned int devid, unsigned int d2d_eid_type, struct dms_ub_d2d_eid *d2d_eid,
                                      uint32_t *eid_index)
{
    struct dms_d2d_eid_input d2d_eid_in = {0};
    struct dms_d2d_eid_output d2d_eid_out = {0};

    if ((d2d_eid == NULL) || (eid_index == NULL)) {
        DMS_ERR("d2d_eid or eid_index is Null.(devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((devid >= DMS_MAX_DEV_NUM) || (d2d_eid_type >= D2D_EID_TYPE_MAX)) {
        DMS_ERR("dev_id or d2d_eid_type is invalid.(devid=%u; d2d_eid_type=%u)\n", devid, d2d_eid_type);
        return DRV_ERROR_PARA_ERROR;
    }

    d2d_eid_in.local_devid = devid;
    d2d_eid_in.query_type = D2D_EID_QUERY_BY_TYPE;
    d2d_eid_in.d2d_eid_type = d2d_eid_type;

    return dms_get_ub_d2d_eid_info(devid, &d2d_eid_in, &d2d_eid_out, d2d_eid, eid_index);
}

drvError_t dms_get_ub_d2d_eid(unsigned int dev_id, struct dms_ub_d2d_eid *d2d_eid)
{
    uint32_t eid_index;

    return dms_get_ub_d2d_eid_by_type(dev_id, D2D_EID_IN_BOARD, d2d_eid, &eid_index);
}

drvError_t dms_get_ub_d2d_eid_v2(unsigned int devid, unsigned int peer_udevid, struct dms_ub_d2d_eid *d2d_eid,
                                 uint32_t *eid_index)
{
    struct dms_d2d_eid_input d2d_eid_in = {0};
    struct dms_d2d_eid_output d2d_eid_out = {0};

    if ((d2d_eid == NULL) || (eid_index == NULL)) {
        DMS_ERR("d2d_eid or eid_index is null.(devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((devid >= DMS_MAX_DEV_NUM) || (peer_udevid >= DMS_MAX_DEV_NUM)) {
        DMS_ERR("dev_id is invalid.(local_devid=%u; peer_udevid=%u)\n", devid, peer_udevid);
        return DRV_ERROR_PARA_ERROR;
    }

    d2d_eid_in.local_devid = devid;
    d2d_eid_in.query_type = D2D_EID_QUERY_BY_DEVID;
    d2d_eid_in.remote_udevid = peer_udevid;

    return dms_get_ub_d2d_eid_info(devid, &d2d_eid_in, &d2d_eid_out, d2d_eid, eid_index);
}

drvError_t dms_get_ub_bus_inst_eid(unsigned int dev_id, struct dms_ub_bus_inst_eid *bus_inst_eid)
{
#if (defined CFG_FEATURE_SUPPORT_UB) && (!defined DRV_HOST)
    struct urd_cmd cmd = {0};
    struct urd_cmd_para cmd_para = {0};
    int ret;

    if (bus_inst_eid == NULL) {
        DMS_ERR("bus_inst_eid is Null.(devid=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (dev_id >= DMS_MAX_DEV_NUM) {
        DMS_ERR("dev_id is invalid.(devid=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    urd_usr_cmd_fill(&cmd, ASCEND_UB_CMD_BASIC, ASCEND_UB_SUBCMD_GET_BUS_INST_EID, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&dev_id, sizeof(unsigned int), (void *)bus_inst_eid,
                          sizeof(struct dms_ub_bus_inst_eid));
    ret = urd_dev_usr_cmd(dev_id, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get bus_inst_eid failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    return DRV_ERROR_NONE;
#else
    (void)dev_id;
    (void)bus_inst_eid;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t dms_get_ub_dev_id_info(unsigned int dev_id, struct dms_ubdev_id_info *id_info)
{
#if (defined CFG_FEATURE_SUPPORT_UB) && (defined DRV_HOST)
    struct urd_cmd_para cmd_para = {0};
    struct urd_cmd cmd = {0};
    int ret;

    if (id_info == NULL) {
        DMS_ERR("id_info is Null.(devid=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    if (dev_id >= DMS_MAX_DEV_NUM) {
        DMS_ERR("dev_id is invalid.(devid=%u)\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    urd_usr_cmd_fill(&cmd, ASCEND_UB_CMD_BASIC, ASCEND_UB_SUBCMD_GET_DEV_ID_INFO, NULL, 0);
    urd_usr_cmd_para_fill(&cmd_para, (void *)&dev_id, sizeof(unsigned int), (void *)id_info,
                          sizeof(struct dms_ubdev_id_info));
    ret = urd_dev_usr_cmd(dev_id, &cmd, &cmd_para);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "Get id_info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    return DRV_ERROR_NONE;
#else
    (void)dev_id;
    (void)id_info;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

#ifdef CFG_FEATURE_SUPPORT_MAMI
#define MAMI_INPUT_MAX_SIZE 4096
typedef int (*mami_init)(void *);
typedef int (*mami_get_local_devid)(unsigned int *);
typedef int (*mami_open_device)(unsigned int, struct mamiOpenDevInfo *);
typedef int (*mami_get)(unsigned int, struct mamiCmdInfo *, unsigned char *, unsigned int);
STATIC int dms_mami_int(void *handle, unsigned int *mami_dev_id)
{
    mami_init mami_init_func;
    mami_get_local_devid mami_get_local_devid_func;
    mami_open_device mami_open_device_func;
    struct mamiOpenDevInfo info = {0};
    int ret = 0;

    mami_init_func = (mami_init)dlsym(handle, "mamiInit");
    mami_get_local_devid_func = (mami_get_local_devid)dlsym(handle, "mamiGetLocalDevid");
    mami_open_device_func = (mami_open_device)dlsym(handle, "mamiOpenDevice");
    if ((mami_init_func == NULL) || (mami_get_local_devid_func == NULL) || (mami_open_device_func == NULL)) {
        DMS_ERR("Unable to find the API for mamiOpenDevice. (mami_init_is_NULL=%d; mami_get_local_devid_is_NULL=%d; "
                "mami_open_device_is_NULL=%d)\n",
                (mami_init_func == NULL), (mami_get_local_devid_func == NULL), (mami_open_device_func == NULL));
        return DRV_ERROR_INNER_ERR;
    }

    ret = mami_init_func(NULL);
    if (ret != 0) {
        DMS_ERR("Failed to invoke mami_init_func. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = mami_get_local_devid_func(mami_dev_id);
    if (ret != 0) {
        DMS_ERR("Failed to invoke mamiGetLocalDevid. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }

    ret = mami_open_device_func(*mami_dev_id, &info);
    if (ret != 0) {
        DMS_ERR("Failed to invoke mamiOpenDevice. (mami_dev_id=%u; ret=%d)\n", *mami_dev_id, ret);
        ret = DRV_ERROR_INNER_ERR;
    }

    return ret;
}

STATIC int dms_mami_get(void *handle, unsigned int mami_dev_id, struct mamiCmdInfo *cmd, void *buf, unsigned int size)
{
    mami_get mami_get_func = NULL;
    int ret;

    mami_get_func = (mami_get)dlsym(handle, "mamiGet");
    if (mami_get_func == NULL) {
        DMS_ERR("Unable to find the mamiGet.\n");
        return DRV_ERROR_INNER_ERR;
    }

    ret = mami_get_func(mami_dev_id, cmd, (unsigned char *)buf, size);
    if (ret != 0) {
        DMS_ERR("Failed to query the ub dfx info. (mami_dev_id=%u; major=%u; minor=%u; size=%u; ret=%d)\n", mami_dev_id,
                cmd->major, cmd->minor, size, ret);
        ret = DRV_ERROR_INNER_ERR;
    }

    return ret;
}

STATIC int dms_get_ub_dfx_info(unsigned int dev_id, void *buf, unsigned int *size)
{
    struct dsmi_ub_dfx_input *input = (struct dsmi_ub_dfx_input *)buf;
    unsigned char *input_buf = NULL;
    struct mamiCmdInfo cmd = {0};
    unsigned int mami_dev_id;
    void *handle = NULL;
    int ret = 0;

    handle = dlopen("/usr/local/lib64/mami/libmami.so", RTLD_GLOBAL | RTLD_NOW);
    if (handle == NULL) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (input->buf_size > MAMI_INPUT_MAX_SIZE || input->buf_size + offsetof(struct dsmi_ub_dfx_input, buf) > *size) {
        DMS_ERR("Invalid input buf size. (dev_id=%u; size=%u; max_buf_size=%u; actual_size=%u)\n", dev_id,
                input->buf_size, MAMI_INPUT_MAX_SIZE, *size);
        dlclose(handle);
        return DRV_ERROR_PARA_ERROR;
    } else if (input->buf_size != 0) {
        input_buf = (unsigned char *)malloc(input->buf_size);
        if (input_buf == NULL) {
            DMS_ERR("Failed to allocate memory. (dev_id=%u)", dev_id);
            dlclose(handle);
            return DRV_ERROR_OUT_OF_MEMORY;
        }

        ret = memcpy_s(input_buf, input->buf_size, input->buf, input->buf_size);
        if (ret != 0) {
            DMS_ERR("Failed to copy the input buf memory. (dev_id=%u; ret=%d)\n", dev_id, ret);
            ret = DRV_ERROR_INNER_ERR;
            goto UB_DFX_INFO_OUT;
        }
    }

    ret = dms_mami_int(handle, &mami_dev_id);
    if (ret != 0) {
        goto UB_DFX_INFO_OUT;
    }

    cmd.major = input->major;
    cmd.minor = input->minor;
    cmd.size = input->buf_size;
    cmd.buffer = input_buf;
    ret = dms_mami_get(handle, mami_dev_id, &cmd, buf, *size);
UB_DFX_INFO_OUT:
    dlclose(handle);
    if (input_buf != NULL) {
        free(input_buf);
        input_buf = NULL;
    }
    return ret;
}
#else
STATIC int dms_get_ub_dfx_info(unsigned int dev_id, void *buf, unsigned int *size)
{
    (void)dev_id;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}
#endif

int dms_get_ub_info_by_dmp(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)vfid;
    if (dev_id >= DMS_MAX_DEV_NUM || buf == NULL || size == NULL) {
        DMS_ERR("Invalid parameter. (dev_id=%u; buf_is_NULL=%d; size_is_NULL=%d)\n", dev_id, (buf == NULL),
                (size == NULL));
        return DRV_ERROR_PARA_ERROR;
    }

    if (sub_cmd == DSMI_UB_INFO_SUB_CMD_UB_DFX_INFO) {
        return dms_get_ub_dfx_info(dev_id, buf, size);
    }

    return DRV_ERROR_NOT_SUPPORT;
}