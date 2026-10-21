/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "mmpa_api.h"
#include "devdrv_ioctl.h"
#include "devmng_common.h"
#include "dsmi_common_interface.h"
#include "devmng_user_common.h"
#include "dms/dms_devdrv_info_comm.h"
#include "ascend_dev_num.h"

struct compute_group_conf {
    char name[DSMI_VDEV_RES_NAME_LEN];
    unsigned int aic;
};

#define VDEV_AICORE_NUM_ONE 1
#define VDEV_AICORE_NUM_TWO 2
#define VDEV_AICORE_NUM_FOUR 4
#define VDEV_AICORE_NUM_EIGHT 8
#define VDEV_AICORE_NUM_SIXTEEN 16
#define VDEV_FLOAT_DEFAULT_VAL (-1.0f)
#define VDEV_RATIO_EXPAND 10000

#ifdef CFG_FEATURE_VF_SINGLE_AICORE
#define VDEV_TOKEN_TIME_1MS 0xFFFFFULL
#define VDEV_TOKEN_TIME_2MS 0x1FFFFFULL
#define VDEV_GROUP_TYPE 2 /* Current RC VFIO only 1/4 and 1/2 are supported.  */

STATIC unsigned long long compute_group_token_map[VDEV_GROUP_TYPE][2] = {
    /* 2 is for token and ratio map. */
    {VDEV_TOKEN_TIME_1MS, 2500}, /* 2500 = 25.00% */
    {VDEV_TOKEN_TIME_2MS, 5000}, /* 5000 = 50.00% */
};
#endif

STATIC struct compute_group_conf name_to_aic[] = {
#ifdef CFG_SOC_PLATFORM_MINIV2
    {"vir01", VDEV_AICORE_NUM_ONE},
    {"vir02", VDEV_AICORE_NUM_TWO},
    {"vir04", VDEV_AICORE_NUM_FOUR},
#elif defined CFG_SOC_PLATFORM_CLOUD_V2
    /* asic total 24 aicore */
    {"vir01", 1},  /* 1 aicore for 1/24 */
    {"vir02", 2},  /* 2 aicore for 1/12 */
    {"vir04", 4},  /* 4 aicore for 1/6 */
    {"vir06", 6},  /* 6 aicore for 1/4 */
    {"vir12", 12}, /* 12 aicore for 1/2 */
    {"vir24", 24}, /* 24 aicore for 1 */
#else
    {"vir02", VDEV_AICORE_NUM_TWO},
    {"vir04", VDEV_AICORE_NUM_FOUR},
    {"vir08", VDEV_AICORE_NUM_EIGHT},
    {"vir16", VDEV_AICORE_NUM_SIXTEEN},
#endif
};

#ifdef CFG_FEATURE_FIXED_TEMPLATE
STATIC int template_name_check(struct dsmi_create_vdev_res_stru *in)
{
    unsigned int j;

    for (j = 0; j < sizeof(name_to_aic) / sizeof(struct compute_group_conf); j++) {
        if (strcmp(in->name, name_to_aic[j].name) == 0) {
            break;
        }
    }

    if (j == sizeof(name_to_aic) / sizeof(struct compute_group_conf)) {
        DEVDRV_DRV_ERR("Check name failed. ()\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (in->computing.aic != name_to_aic[j].aic) {
        DEVDRV_DRV_ERR("Check aic failed. (aic=%u)\n", in->computing.aic);
        return DRV_ERROR_PARA_ERROR;
    }
    return 0;
}
#endif

static drvError_t drv_set_ioctl_create_info(u32 devid, u32 vdev_id, struct vdev_create_info *cinfo,
                                            const struct dsmi_create_vdev_res_stru *vdev_res)
{
    int ret;

    cinfo->devid = devid;
    cinfo->vdevid = vdev_id;
    cinfo->vdev_info.base.vfg_id = vdev_res->base.vfg_id;
    cinfo->vdev_info.base.token = vdev_res->base.token;
    cinfo->vdev_info.base.token_max = vdev_res->base.token_max;
    cinfo->vdev_info.base.task_timeout = vdev_res->base.task_timeout;
    cinfo->vdev_info.computing.aic = vdev_res->computing.aic;
    cinfo->spec.core_num = (unsigned int)vdev_res->computing.aic;
    cinfo->vdev_info.computing.device_aicpu = vdev_res->computing.device_aicpu;
    if (strnlen(vdev_res->name, DSMI_VDEV_RES_NAME_LEN) == 0) {
        DEVDRV_DRV_ERR("Input vdev name is \"\".\n");
        return DRV_ERROR_PARA_ERROR;
    }
    ret = strcpy_s(cinfo->vdev_info.name, DSMI_VDEV_RES_NAME_LEN, vdev_res->name);
    if (ret != 0) {
        DEVDRV_DRV_ERR("strncpy_s vdev computing name failed, (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    /**
     * we can't trans float to unsigned int on the ARM platform directly
     * its behavior you can do something like u = f < 0 ? 0 : f > UCHAR_MAX ? UCHAR_MAX : f;
     **/
    cinfo->vdev_info.media.jpegd = (u32)(int)vdev_res->media.jpegd;
    cinfo->vdev_info.media.jpege = (u32)(int)vdev_res->media.jpege;
    cinfo->vdev_info.media.vpc = (u32)(int)vdev_res->media.vpc;
    cinfo->vdev_info.media.vdec = (u32)(int)vdev_res->media.vdec;
    cinfo->vdev_info.media.pngd = (u32)(int)vdev_res->media.pngd;
    cinfo->vdev_info.media.venc = (u32)(int)vdev_res->media.venc;
    cinfo->vdev_info.computing.memory_size = vdev_res->computing.memory_size;
    return 0;
}

drvError_t drvCreateVdevice(u32 devid, u32 vdev_id, struct dsmi_create_vdev_res_stru *vdev_res,
                            struct dsmi_create_vdev_result *vdev_result)
{
    char name_buf[DEVDRV_DEVICE_NAME_BUF_SIZE] = {0};
    struct vdev_create_info cinfo = {0};
    struct stat st = {0};
    int ret;

    if (devid >= ASCEND_DEV_MAX_NUM || vdev_res == NULL || vdev_result == NULL) {
        DEVDRV_DRV_ERR("Invalid parameter. (devid=%u; in_is_null=%d; out_is_null=%d)\n", devid, vdev_res == NULL,
                       vdev_result == NULL);
        return DRV_ERROR_PARA_ERROR;
    }

#ifdef CFG_FEATURE_FIXED_TEMPLATE
    if (template_name_check(vdev_res)) {
        DEVDRV_DRV_ERR("Invalid name or aicore nums.\n");
        return DRV_ERROR_PARA_ERROR;
    }
#endif
    ret = drv_set_ioctl_create_info(devid, vdev_id, &cinfo, vdev_res);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Set create info for ioctl failed, (ret=%d).\n", ret);
        return ret;
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_CREATE_VDEV, (void *)&cinfo);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "ioctl failed, devid(%u), ret(%d).\n", devid, ret);
        return ret;
    }

    (void)memset_s((void *)vdev_result, sizeof(struct dsmi_create_vdev_result), 0xff,
                   sizeof(struct dsmi_create_vdev_result));
    vdev_result->vdev_id = cinfo.vdevid;
    vdev_result->vfg_id = cinfo.vdev_info.base.vfg_id;
    vdev_result->vf_id = cinfo.vfid;
    /*
     * It is used to clear the queue for creating VF devices in user-space.
     * It is hard-coding here and it will invoke waitpid in 'system' implementation.
     * Even if it fails to be invoked, there is no effect (privileged-docker).
     */
    (void)system("udevadm settle > /dev/null 2>&1");
#ifdef CFG_FEATURE_VFIO_SOC
    ret = stat("/dev/davinci0", &st);
#else
    ret = stat("/dev/davinci_manager", &st);
#endif
    if (ret != 0) {
        DEVDRV_DRV_ERR("get device manager stat failed, ret(%d).\n", ret);
        return DRV_ERROR_FILE_OPS;
    }

    /* change vdevice mode and owner */
#ifdef CFG_FEATURE_VFIO_SOC
    if (sprintf_s(name_buf, DEVDRV_DEVICE_NAME_BUF_SIZE, DEVDRV_DEVICE_PATH DEVDRV_DEVICE_NAME "%u",
                  vdev_result->vdev_id) > 0) {
#else
    if (sprintf_s(name_buf, DEVDRV_DEVICE_NAME_BUF_SIZE, DEVDRV_DEVICE_PATH DEVDRV_VDEVICE_NAME "%u",
                  vdev_result->vdev_id) > 0) {
#endif
        ret = chown(name_buf, st.st_uid, st.st_gid);
        if (ret != 0) {
            DEVDRV_DRV_INFO("The file does not exist. (file=\"%s\"; vdevid=%d; ret=%d)\n", name_buf,
                            vdev_result->vdev_id, ret);
        }
    }

    return DRV_ERROR_NONE;
}

drvError_t halDrvCreateVdevice(unsigned int devid, unsigned int vdev_id, void *vdev_res, void *vdev_result)
{
    return drvCreateVdevice((u32)devid, (u32)vdev_id, (struct dsmi_create_vdev_res_stru *)vdev_res,
                            (struct dsmi_create_vdev_result *)vdev_result);
}

/*
 * if vdevid = 0xffff, destroy the all vdev in devid
 * if vdevid != 0xffff, destroy the vdevice
 */
drvError_t drvDestroyVdevice(u32 devid, u32 vdevid)
{
    struct devdrv_vdev_id_info vinfo = {0};
    int ret;

    if (devid >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid parameter devid(%u).\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    vinfo.devid = devid;
    vinfo.vdevid = vdevid;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_DESTROY_VDEV, (void *)&vinfo);
    if (ret == DRV_ERROR_NO_DEVICE) {
        DEVDRV_DRV_ERR("no vdevice alloced, devid(%u), vdevid(%u), ret(%d).\n", devid, vdevid, ret);
        return ret;
    } else if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "ioctl failed, devid(%u), vdevid(%u), ret(%d).\n", devid, vdevid, ret);
        return ret;
    }
    return 0;
}

STATIC int get_name_by_aic(unsigned int aicore, char *template_name, int len)
{
    unsigned int i;
    int ret;

    for (i = 0; i < sizeof(name_to_aic) / sizeof(struct compute_group_conf); i++) {
        if (name_to_aic[i].aic == aicore) {
            ret = strcpy_s(template_name, len, name_to_aic[i].name);
            if (ret != 0) {
                DEVDRV_DRV_ERR("strcpy_s failed. (aicore=%u; template_name=\"%s\")\n", aicore, name_to_aic[i].name);
                return DRV_ERROR_OUT_OF_MEMORY;
            }

            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_INNER_ERR;
}

#ifndef CFG_FEATURE_VFG
STATIC void drvSetVdevFloatDefaultValue(struct dsmi_computing_resource *computing, struct dsmi_media_resource *media)
{
    computing->aiv = VDEV_FLOAT_DEFAULT_VAL;
    media->jpegd = VDEV_FLOAT_DEFAULT_VAL;
    media->jpege = VDEV_FLOAT_DEFAULT_VAL;
    media->vpc = VDEV_FLOAT_DEFAULT_VAL;
    media->vdec = VDEV_FLOAT_DEFAULT_VAL;
    media->pngd = VDEV_FLOAT_DEFAULT_VAL;
    media->venc = VDEV_FLOAT_DEFAULT_VAL;
}
#endif

static void drv_get_computing_and_media_info(struct dsmi_computing_resource *computing,
                                             struct dsmi_media_resource *media, struct vdev_query_info *vinfo)
{
#ifdef CFG_FEATURE_VFG
    computing->device_aicpu = vinfo->computing.device_aicpu;
    media->jpegd = (float)vinfo->media.jpegd;
    media->jpege = (float)vinfo->media.jpege;
    media->vpc = (float)vinfo->media.vpc;
    media->vdec = (float)vinfo->media.vdec;
    media->venc = (float)vinfo->media.venc;
    media->pngd = VDEV_FLOAT_DEFAULT_VAL;
    computing->memory_size = vinfo->computing.memory_size;
    computing->aiv = VDEV_FLOAT_DEFAULT_VAL;
#else
    (void)vinfo;
    drvSetVdevFloatDefaultValue(computing, media);
#endif
}

STATIC int get_info_by_vdev_id(struct dsmi_vdev_query_stru *single_query, struct vdev_query_info *vinfo)
{
    unsigned int i = 0;
    int ret;
    unsigned int core_num;

    for (i = 0; i < vinfo->vdev_num; i++) {
        if (single_query->vdev_id == vinfo->vdev[i].id) {
            core_num = vinfo->vdev[i].spec.core_num;
            if (strnlen(vinfo->name, DSMI_VDEV_RES_NAME_LEN) == 0) {
                ret = get_name_by_aic(core_num, single_query->query_info.name, DSMI_VDEV_RES_NAME_LEN);
            } else {
                ret = strcpy_s(single_query->query_info.name, DSMI_VDEV_RES_NAME_LEN, vinfo->name);
            }
            if (ret != 0) {
                DEVDRV_DRV_ERR("Failed to invoke get_name_by_aic. (ret=%d)", ret);
                return ret;
            }
            single_query->query_info.is_container_used = vinfo->vdev[i].status;
            single_query->query_info.vfid = vinfo->vdev[i].vfid;
            single_query->query_info.container_id = vinfo->vdev[i].cid;
            single_query->query_info.computing.aic = (float)vinfo->vdev[i].spec.core_num;
#ifdef CFG_FEATURE_VFG
            single_query->query_info.base.vfg_id = vinfo->base.vfg_id;
            single_query->query_info.base.vip_mode = vinfo->base.vip_mode;
#endif
#ifdef CFG_FEATURE_VFIO_SOC
            single_query->query_info.base.token = vinfo->base.token;
            single_query->query_info.base.token_max = vinfo->base.token_max;
            single_query->query_info.base.task_timeout = vinfo->base.task_timeout;
#endif
            drv_get_computing_and_media_info(&single_query->query_info.computing, &single_query->query_info.media,
                                             vinfo);
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_PARA_ERROR;
}

STATIC int get_ratio_by_vinfo(unsigned int vdevid, unsigned int *tops_ratio, struct vdev_query_info vinfo)
{
    (void)vdevid;
#ifdef CFG_FEATURE_VF_SINGLE_AICORE
    /**
     * The calculation of RC compute group, which is different from the others,
     * is calculated according to the token instead of aicore numbers.
     *
     * If the tops_ratio's value is 625, which means the VF compute group ratio is 6.25%.
     **/
    int i;

    for (i = 0; i < VDEV_GROUP_TYPE; i++) {
        if (compute_group_token_map[i][0] == vinfo.base.token) {
            *tops_ratio = (unsigned int)compute_group_token_map[i][1];
            return DRV_ERROR_NONE;
        }
    }

    DEVDRV_DRV_ERR("Invalid token value. (token=%llu)\n", vinfo.base.token);
    return DRV_ERROR_PARA_ERROR;
#else
    (void)(tops_ratio);
    (void)(vinfo);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t drvGetVdevTotalInfo(u32 devid, u32 main_cmd, u32 sub_cmd, void *vdev_res, u32 *size)
{
    unsigned int i;
    int ret;
    struct vdev_query_info vinfo = {0};
    struct dsmi_soc_total_resource *total = NULL;
    (void)(main_cmd);

    if (devid >= ASCEND_DEV_MAX_NUM || vdev_res == NULL || size == NULL) {
        DEVDRV_DRV_ERR("invalid parameter. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (*size < sizeof(struct dsmi_soc_total_resource)) {
        DEVDRV_DRV_ERR("invalid parameter. (devid=%u; size=%u)\n", devid, *size);
        return DRV_ERROR_PARA_ERROR;
    }

    vinfo.devid = devid;
    vinfo.cmd_type = sub_cmd;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_VDEVINFO, (void *)&vinfo);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    total = (struct dsmi_soc_total_resource *)vdev_res;
    (void)memset_s((void *)total, *size, 0xff, *size);
    total->vdev_num = vinfo.vdev_num;
    for (i = 0; i < total->vdev_num; i++) {
        total->vdev_id[i] = vinfo.vdev[i].id;
    }
    total->computing.aic = (float)(vinfo.aic_total);
    drv_get_computing_and_media_info(&total->computing, &total->media, &vinfo);

    return DRV_ERROR_NONE;
}

drvError_t drvGetVdevFreeInfo(u32 devid, u32 main_cmd, u32 sub_cmd, void *vdev_res, u32 *size)
{
    int ret;
    struct vdev_query_info vinfo = {0};
    struct dsmi_soc_free_resource *free = NULL;
    (void)(main_cmd);

    if (devid >= ASCEND_DEV_MAX_NUM || vdev_res == NULL || size == NULL) {
        DEVDRV_DRV_ERR("invalid parameter. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (*size < sizeof(struct dsmi_soc_free_resource)) {
        DEVDRV_DRV_ERR("invalid parameter. (devid=%u; size=%u)\n", devid, *size);
        return DRV_ERROR_PARA_ERROR;
    }

    vinfo.devid = devid;
    vinfo.cmd_type = sub_cmd;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_VDEVINFO, (void *)&vinfo);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    free = (struct dsmi_soc_free_resource *)vdev_res;
    (void)memset_s((void *)free, *size, 0xff, *size);
    free->computing.aic = (float)(vinfo.aic_unused);
    drv_get_computing_and_media_info(&free->computing, &free->media, &vinfo);

    return DRV_ERROR_NONE;
}

drvError_t drvGetVdevActivityInfo(u32 devid, u32 main_cmd, u32 sub_cmd, void *vdev_res, u32 *size)
{
    int ret;
    struct vdev_query_info vinfo = {0};
    struct dsmi_vdev_query_stru *single_query = NULL;
    (void)(main_cmd);

    if (devid >= ASCEND_DEV_MAX_NUM || vdev_res == NULL || size == NULL) {
        DEVDRV_DRV_ERR("invalid parameter. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (*size < sizeof(struct dsmi_vdev_query_stru)) {
        DEVDRV_DRV_ERR("Invalid parameter. (devid=%u; size=%u)\n", devid, *size);
        return DRV_ERROR_PARA_ERROR;
    }

    vinfo.devid = devid;
    single_query = (struct dsmi_vdev_query_stru *)vdev_res;
    vinfo.vdev_id_single = single_query->vdev_id;
    vinfo.cmd_type = sub_cmd;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_VDEVINFO, (void *)&vinfo);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    (void)memset_s((void *)&single_query->query_info, sizeof(struct dsmi_vdev_query_info), 0xff,
                   sizeof(struct dsmi_vdev_query_info));

    single_query->query_info.computing.vdev_aicore_utilization = vinfo.computing.vdev_aicore_utilization;

    return DRV_ERROR_NONE;
}

drvError_t drvGetSingleVdevInfo(u32 devid, u32 main_cmd, u32 sub_cmd, void *vdev_res, u32 *size)
{
    int ret;
    struct vdev_query_info vinfo = {0};
    struct dsmi_vdev_query_stru *single_query = NULL;
    (void)(main_cmd);

    if (devid >= ASCEND_DEV_MAX_NUM || vdev_res == NULL || size == NULL) {
        DEVDRV_DRV_ERR("invalid parameter. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (*size < sizeof(struct dsmi_vdev_query_stru)) {
        DEVDRV_DRV_ERR("invalid parameter. (devid=%u; size=%u; expected_size=[%lu,))\n", devid, *size,
                       sizeof(struct dsmi_vdev_query_stru));
        return DRV_ERROR_PARA_ERROR;
    }

    vinfo.devid = devid;
    single_query = (struct dsmi_vdev_query_stru *)vdev_res;
    vinfo.vdev_id_single = single_query->vdev_id;
    vinfo.cmd_type = sub_cmd;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_VDEVINFO, (void *)&vinfo);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    (void)memset_s((void *)&single_query->query_info, sizeof(struct dsmi_vdev_query_info), 0xff,
                   sizeof(struct dsmi_vdev_query_info));
    ret = get_info_by_vdev_id(single_query, &vinfo);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke get_info_by_vdev_id. (devid=%u; vdevid=%u; ret=%d)\n", devid,
                       single_query->vdev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvGetVdevTopsPercentage(u32 devid, u32 main_cmd, u32 sub_cmd, void *tops_ratio, u32 *size)
{
    int ret;
    struct vdev_query_info vinfo = {0};
    struct dsmi_soc_vdev_ratio *in_vinfo = NULL;
    (void)(main_cmd);

    if (devid >= ASCEND_DEV_MAX_NUM || tops_ratio == NULL || size == NULL) {
        DEVDRV_DRV_ERR("invalid parameter. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (*size < sizeof(struct dsmi_soc_vdev_ratio)) {
        DEVDRV_DRV_ERR("invalid parameter. (devid=%u; size=%u; expected_size=[%lu,))\n", devid, *size,
                       sizeof(struct dsmi_soc_vdev_ratio));
        return DRV_ERROR_PARA_ERROR;
    }

    in_vinfo = (struct dsmi_soc_vdev_ratio *)tops_ratio;
    vinfo.devid = devid;
    vinfo.vdev_id_single = in_vinfo->vdev_id;
    vinfo.cmd_type = sub_cmd;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_VDEVINFO, (void *)&vinfo);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. (devid=%u; ret=%d)\n", devid, ret);
        return ret;
    }

    ret = get_ratio_by_vinfo(vinfo.vdev_id_single, &in_vinfo->ratio, vinfo);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to invoke get_ratio_by_vinfo. (devid=%u; vdevid=%u; ret=%d)\n", devid,
                                     vinfo.vdev_id_single, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvSetSvmVdevInfo(unsigned int devid, unsigned int sub_cmd, const void *buf, unsigned int buf_size)
{
    struct devdrv_svm_vdev_info vinfo = {0};
    int ret;

    if ((sub_cmd >= DSMI_SVM_SUB_CMD_MAX) || (buf == NULL) || (buf_size != DEVDRV_SVM_VDEV_LEN)) {
        DEVDRV_DRV_ERR("Invalid parameter. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    vinfo.devid = devid;
    vinfo.type = sub_cmd;
    vinfo.buf_size = buf_size;
    ret = memcpy_s(vinfo.buf, DEVDRV_SVM_VDEV_LEN, buf, vinfo.buf_size);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Memory copy failed. (dev_id=%u; ret=%d)\n", devid, ret);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_SET_SVM_VDEVINFO, (void *)&vinfo);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (devid=%u; ret=%d).\n", devid, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvGetSvmVdevInfo(unsigned int devid, unsigned int sub_cmd, void *buf, unsigned int *buf_size)
{
    struct devdrv_svm_vdev_info vinfo = {0};
    int ret;

    if ((sub_cmd >= DSMI_SVM_SUB_CMD_MAX) || (buf == NULL) || (buf_size == NULL)) {
        DEVDRV_DRV_ERR("Invalid parameter. (devid=%u)\n", devid);
        return DRV_ERROR_PARA_ERROR;
    }

    vinfo.devid = devid;
    vinfo.type = sub_cmd;
    vinfo.buf_size = *buf_size;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_SVM_VDEVINFO, (void *)&vinfo);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (devid=%u; ret=%d).\n", devid, ret);
        return ret;
    }

    ret = memcpy_s(buf, *buf_size, vinfo.buf, vinfo.buf_size);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Memory copy failed. (dev_id=%u; ret=%d)\n", devid, ret);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    *buf_size = vinfo.buf_size;

    return DRV_ERROR_NONE;
}

drvError_t drvGetVdeviceMode(int *mode)
{
    int ret;

    if (mode == NULL) {
        DEVDRV_DRV_ERR("Invalid para. mode is NULL\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_VDEVMODE, (void *)mode);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret(%d).\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

drvError_t drvSetVdeviceMode(int mode)
{
    int ret;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_SET_VDEVMODE, (void *)&mode);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Ioctl failed. ret=%d.\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
