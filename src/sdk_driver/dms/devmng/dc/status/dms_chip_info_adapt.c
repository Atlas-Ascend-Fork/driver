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
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "ka_task_pub.h"
#include "securec.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_urd.h"
#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_soc_res.h"
#include "ascend_kernel_hal.h"
#include "pbl/pbl_user_cfg_interface.h"
#include "pbl_mem_alloc_interface.h"
#include "dms_define.h"
#include "dms_chip_info.h"
#include "devdrv_common.h"
#include "hvtsdrv_tsagent.h"
#include "devdrv_manager_container.h"
#include "ka_base_pub.h"
#include "ka_errno_pub.h"
#include "ka_task_pub.h"
#include "dms_feature_pub.h"
#ifdef CFG_FEATURE_GET_DEV_UUID
#include "ascend_platform/ascend_platform.h"
#endif

typedef enum {
    CORE_NUM_A_INDEX = 1,
    CORE_NUM_B_INDEX,
    CORE_NUM_C_INDEX,
    CORE_NUM_LEVEL_MAX
} AICORE_NUM_LEVEL;

typedef enum {
    FREQ_LITE_INDEX = 1,
    FREQ_PRO_INDEX,
    FREQ_PRE_INDEX,
    CORE_FREQ_LEVEL_MAX,
} AICORE_FREQ_LEVEL;

/* add for ai computing power */
typedef enum {
    AI_COMPUTING_LEVEL1 = 0,
    AI_COMPUTING_LEVEL2,
    AI_COMPUTING_LEVEL3,
    AI_COMPUTING_LEVEL4,
} AI_COMPUTING_LEVEL;

typedef enum {
    UPGRADE_POLICY_NORMAL = 0,
    UPGRADE_POLICY_FORCE = 1,
    UPGRADE_POLICY_IGNORE = 2,
    UPGRADE_POLICY_MAX,
} SWPLUGIN_UPGRADE_POLICY;

#define DEV_AICORE_FREQ_LEVEL_CLOUD 1
#define DEV_AICORE_FREQ_LEVEL_MINIV2 2

/* add for partial good */
#define CLOUD_FULL_GOOD_CORE_NUM 32
#define AICORE_FREQ_LEVEL_PRO 1100
#define AICORE_FREQ_LEVEL_PRE 1200

#define AICORE_LEVEL_LIMIT(num_level, freq_level)                                     \
    (((num_level) > 0) && ((num_level) < CORE_NUM_LEVEL_MAX) && ((freq_level) > 0) && \
     ((freq_level) < CORE_FREQ_LEVEL_MAX))

STATIC char *g_aicore_num_level[] = {"NULL", "A", "B", "C"};
STATIC char *g_aicore_freq_level[] = {"NULL", "", "Pro", "Premium"};
STATIC char *g_ai_computing_level[] = {"1", "2", "3", "4"};

STATIC chip_value_name_map_t g_chip_value_name_map[] = {
    {6416, "310"}, {6528, "910"}, {6481, "310P"}, {6417, "310B"}, {6529, "910B"}, {6514, "950"},
};

struct dms_cust_board_info g_cust_board_info[ASCEND_PDEV_MAX_NUM] = {0};
#if (defined CFG_FEATURE_UPGRADE_SWPLUGIN_POLICY) && (!defined CFG_HOST_ENV)
STATIC ka_atomic_t g_swplugin_upgrade_policy[ASCEND_PDEV_MAX_NUM] = {KA_BASE_ATOMIC_INIT(UPGRADE_POLICY_NORMAL)};
#endif
#ifdef CFG_FEATURE_GET_DEV_UUID
#define UUID_MAX_LEN 16
STATIC char g_mac_info[ASCEND_PDEV_MAX_NUM][UUID_MAX_LEN];
STATIC ka_atomic_t g_mac_init_flag[ASCEND_PDEV_MAX_NUM] = {KA_BASE_ATOMIC_INIT(0)};
#define UUID_VENDOR_ID 0xCC08
#define GUID_BASE_LEN 0x04
#endif

STATIC int dms_get_chip_aicore_info(unsigned int dev_id, unsigned int vfid, bool is_container_split,
                                    dms_chip_aicore_info_t *chip_aicore_info)
{
    int ret;
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
#ifndef CFG_FEATURE_REFACTOR
    struct devdrv_platform_data *pdata = NULL;
#endif

    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        dms_err("Get device ctrl block failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    if (dev_cb->dev_info == NULL) {
        dms_err("Device ctrl dev_info is null. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    dev_info = (struct devdrv_info *)dev_cb->dev_info;
#ifndef CFG_FEATURE_REFACTOR
    pdata = dev_info->pdata;
    if (pdata == NULL) {
        return -ENODEV;
    }
    chip_aicore_info->freq_level = pdata->ai_core_freq_level;
    chip_aicore_info->num_level = pdata->ai_core_num_level;
#else
    chip_aicore_info->freq_level = 0;
    chip_aicore_info->num_level = 0;
#endif
    chip_aicore_info->freq = dev_info->aicore_freq;
    chip_aicore_info->chip_name = dev_info->chip_name;
    chip_aicore_info->chip_ver = dev_info->chip_version;
    if (is_container_split == true) {
        ret = hvdevmng_get_aicore_num(dev_id, vfid, &chip_aicore_info->num);
        if (ret != 0) {
            dms_err("Get container aicore num failed. (dev_id=%u; vfid=%u; ret=%d)", dev_id, vfid, ret);
            return ret;
        }
    } else {
        chip_aicore_info->num = dev_info->ai_core_num;
    }

    dms_debug("(Freq level=%u; num_level=%u; num=%u; freq=%u Mhz; chip_name=0x%x; chip_ver=%u)\n",
              chip_aicore_info->freq_level, chip_aicore_info->num_level, chip_aicore_info->num, chip_aicore_info->freq,
              chip_aicore_info->chip_name, chip_aicore_info->chip_ver);
    return 0;
}

STATIC int dms_set_chip_name_by_aicore_num_and_freq(devdrv_query_chip_info_t *chip_info,
                                                    dms_chip_aicore_info_t *chip_aicore_info)
{
    int ret;
    unsigned char *chip_name = chip_info->info.name;

    if (AICORE_LEVEL_LIMIT(chip_aicore_info->num_level, chip_aicore_info->freq_level)) {
        ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_freq_level[chip_aicore_info->freq_level]);
        if (ret < 0) {
            dms_err("Strcat chip name by aicore freq level failed. (ret=%d)\n", ret);
            return ret;
        }

        ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_num_level[chip_aicore_info->num_level]);
        if (ret < 0) {
            dms_err("Strcat chip name by aicore num level failed. (ret=%d)\n", ret);
            return ret;
        }
    } else {
        if (chip_aicore_info->freq < AICORE_FREQ_LEVEL_PRO) {
            ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_freq_level[FREQ_LITE_INDEX]);
        } else if (chip_aicore_info->freq > AICORE_FREQ_LEVEL_PRE) {
            ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_freq_level[FREQ_PRE_INDEX]);
        } else {
            ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_freq_level[FREQ_PRO_INDEX]);
        }

        if (ret < 0) {
            dms_err("Strcat chip name by aicore freq fail. (dev_id=%u; frequency=%u; ret=%d)\n", chip_info->dev_id,
                    chip_aicore_info->freq, ret);
            return ret;
        }

        if (chip_aicore_info->num < CLOUD_FULL_GOOD_CORE_NUM) {
            ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_num_level[CORE_NUM_B_INDEX]);
        } else {
            ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_aicore_num_level[CORE_NUM_A_INDEX]);
        }

        if (ret < 0) {
            dms_err("Strcat chip name by aicore num fail. (dev_id=%u; num=%u; ret=%d)\n", chip_info->dev_id,
                    chip_aicore_info->num, ret);
            return ret;
        }
    }

    return 0;
}

STATIC int dms_set_chip_name_only_by_aicore_num(devdrv_query_chip_info_t *chip_info,
                                                dms_chip_aicore_info_t *chip_aicore_info)
{
    int ret;
    unsigned int ai_core_num = chip_aicore_info->num;
    unsigned char *chip_name = chip_info->info.name;

    if (ai_core_num == MINIV2_PRO_CORE_NUM) {
        ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_ai_computing_level[AI_COMPUTING_LEVEL1]);
        if (ret < 0) {
            dms_err("Strcat by aicore freq failed. (dev_id=%u; aicore_num=%u; ret=%d)\n", chip_info->dev_id,
                    ai_core_num, ret);
            return ret;
        }
    } else if (ai_core_num == MINIV2_CORE_NUM) {
        ret = strcat_s((char *)chip_name, MAX_CHIP_NAME, g_ai_computing_level[AI_COMPUTING_LEVEL3]);
        if (ret < 0) {
            dms_err("Strcat by aicore freq failed. (dev_id=%u; aicore_num=%u; ret=%d)\n", chip_info->dev_id,
                    ai_core_num, ret);
            return ret;
        }
    } else {
        dms_err("Aicore number is invalid. (dev_id=%u; aicore_num=%u)\n", chip_info->dev_id, ai_core_num);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

#if defined(CFG_FEATURE_PG)
static int dms_set_chip_name_by_soc_version(u32 dev_id, u8 *chip_name)
{
    int ret;
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("dev_info is NULL. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }
#ifdef CFG_FEATURE_REFACTOR
    if (ka_base_strlen(dev_info->soc_version) <= ka_base_strlen("Ascend")) {
        dms_err("soc version is too short. (ver=\"%s\")\n", dev_info->soc_version);
        return DRV_ERROR_INVALID_VALUE;
    }
    /* the soc_version read from hsm is like Ascend910xx, Ascend is not wanted */
    ret = strcpy_s(chip_name, MAX_CHIP_NAME, (u8 *)dev_info->soc_version + ka_base_strlen("Ascend"));
    if (ret) {
        dms_err("Call strcpy_s failed. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }
#else
    if (ka_base_strlen(dev_info->pg_info.spePgInfo.socVersion) <= ka_base_strlen("Ascend")) {
        dms_err("soc version is too short. (ver=\"%s\")\n", dev_info->pg_info.spePgInfo.socVersion);
        return DRV_ERROR_INVALID_VALUE;
    }
    /* the soc_version read from hsm is like Ascend910xx, Ascend is not wanted */
    ret = strcpy_s(chip_name, MAX_CHIP_NAME, (u8 *)dev_info->pg_info.spePgInfo.socVersion + ka_base_strlen("Ascend"));
    if (ret) {
        dms_err("Call strcpy_s failed. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }
#endif

    return 0;
}
#endif

#ifdef CFG_FEATURE_CHIP_INFO_FROM_DEVINFO
STATIC int dms_set_chip_template_name_from_devinfo(u32 dev_id, u32 vfid, devdrv_query_chip_info_t *chip_info)
{
    int ret;
    struct devdrv_info *dev_info = NULL;

    if (devdrv_manager_is_pf_device(dev_id) && (vfid == 0)) {
        return 0;
    }

    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        dms_err("dev_info is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = strcat_s((char *)chip_info->info.name, MAX_CHIP_NAME, dev_info->template_name);
    if (ret != 0) {
        dms_err("Set template name failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return -EINVAL;
    }

    return 0;
}
#define dms_set_chip_template_name dms_set_chip_template_name_from_devinfo
#elif defined(CFG_FEATURE_CHIP_INFO_FROM_VMNG)
STATIC int dms_set_chip_template_name_from_vmng(u32 dev_id, u32 vfid, devdrv_query_chip_info_t *chip_info)
{
    int ret;
    struct vmng_soc_resource_enquire info;

    if (devdrv_manager_is_pf_device(dev_id) && (vfid == 0)) {
        return 0;
    }

#ifdef CFG_HOST_ENV
    ret = vmngh_enquire_soc_resource(dev_id, vfid, &info);
#else
    ret = vmngd_enquire_soc_resource(dev_id, vfid, &info);
#endif
    if (ret != 0) {
        dms_err("Enquire soc resource failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return ret;
    }

    ret = strcat_s((char *)chip_info->info.name, MAX_CHIP_NAME, info.each.name);
    if (ret != 0) {
        dms_err("Set template name failed. (dev_id=%u; vfid=%u; ret=%d)\n", dev_id, vfid, ret);
        return -EINVAL;
    }

    return 0;
}
#define dms_set_chip_template_name dms_set_chip_template_name_from_vmng
#else
STATIC int dms_set_chip_template_name(u32 dev_id, u32 vfid, devdrv_query_chip_info_t *chip_info)
{
    return 0;
}
#endif

STATIC int dms_set_chip_type(u32 dev_id, devdrv_query_chip_info_t *chip_info, unsigned int virt_id)
{
    int ret = 0;
    /* Initialize the chip_type to Ascend. */
    ret = strcpy_s((char *)chip_info->info.type, MAX_CHIP_NAME, "Ascend");
    if (ret != 0) {
        dms_err("Copy chip type failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

    return 0;
}

STATIC int dms_set_chip_name(devdrv_query_chip_info_t *chip_info, bool is_container_split,
                             dms_chip_aicore_info_t *chip_aicore_info, unsigned int virt_id)
{
#ifndef DMS_UT
    int ret = 0;
    unsigned int i;

    ret = sprintf_s((char *)chip_info->info.name, MAX_CHIP_NAME, "%s", "unknown");
    if (ret < 0) {
        dms_err("Set initial chip name failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

#if defined(CFG_FEATURE_PG)
    ret = dms_set_chip_name_by_soc_version(chip_info->dev_id, (u8 *)chip_info->info.name);
    if (ret) {
        dms_err("Set chip name by soc version failed. (dev_id=%u)\n", chip_info->dev_id);
        return ret;
    }
    return 0;
#endif

    for (i = 0; i < sizeof(g_chip_value_name_map) / sizeof(chip_value_name_map_t); ++i) {
        if (chip_aicore_info->chip_name != g_chip_value_name_map[i].chip_value) {
            continue;
        }

        if (is_container_split == true) {
#ifndef CFG_FEATURE_VFG
            ret = sprintf_s((char *)chip_info->info.name, MAX_CHIP_NAME, "%svir%02u",
                            g_chip_value_name_map[i].chip_name, chip_aicore_info->num);
#else
            ret = sprintf_s((char *)chip_info->info.name, MAX_CHIP_NAME, "%s", g_chip_value_name_map[i].chip_name);
#endif
            if (ret < 0) {
                dms_err("Translate chip name failed. (dev_id=%u; ret=%d)\n", chip_info->dev_id, ret);
                return ret;
            }

            return 0;
        }

        ret = sprintf_s((char *)chip_info->info.name, MAX_CHIP_NAME, "%s", g_chip_value_name_map[i].chip_name);
        if (ret < 0) {
            dms_err("Translate chip name failed. (ret=%d)\n", ret);
            return ret;
        }

        if (chip_aicore_info->chip_name == g_chip_value_name_map[DEV_AICORE_FREQ_LEVEL_CLOUD].chip_value) {
            ret = dms_set_chip_name_by_aicore_num_and_freq(chip_info, chip_aicore_info);
            if (ret != 0) {
                dms_err("Set chip name by aicore number and freq failed. (dev_id=%u; ret=%d)\n", chip_info->dev_id,
                        ret);
                return ret;
            }
        }

        if (chip_aicore_info->chip_name == g_chip_value_name_map[DEV_AICORE_FREQ_LEVEL_MINIV2].chip_value) {
            ret = dms_set_chip_name_only_by_aicore_num(chip_info, chip_aicore_info);
            if (ret != 0) {
                dms_err("Set chip name only by aicore number fail. (dev_id=%u; ret=%d)\n", chip_info->dev_id, ret);
                return ret;
            }
        }

        break;
    }
#endif
    return 0;
}

STATIC int dms_update_chip_info(devdrv_query_chip_info_t *chip_info, dms_chip_aicore_info_t *chip_aicore_info)
{
    int ret = DRV_ERROR_NONE;
    int tmp;

#if defined(CFG_SOC_PLATFORM_CLOUD) || defined(CFG_SOC_PLATFORM_MINIV2)
    tmp = sprintf_s((char *)chip_info->info.version, MAX_CHIP_NAME, "V%01x", chip_aicore_info->chip_ver);
#else
    /* mini chip info register, chip version is 0-11 bit, 3bytes, so need V%03x */
    tmp = sprintf_s((char *)chip_info->info.version, MAX_CHIP_NAME, "V%03x", chip_aicore_info->chip_ver);
#endif
    if (tmp < 0) {
        dms_err("get version err. (dev=%u chip_ver=%u ret=%d)\n", chip_info->dev_id, chip_aicore_info->chip_ver, tmp);
        return DRV_ERROR_INNER_ERR;
    }

    return ret;
}

int dms_get_chip_info(unsigned int virt_id, devdrv_query_chip_info_t *chip_info)
{
    int ret;
    u32 vfid = 0;
    bool is_in_container;
    bool is_container_split = false;
    dms_chip_aicore_info_t chip_aicore_info = {0};

    ret = dms_trans_and_check_id(virt_id, &chip_info->dev_id, &vfid);
    if (ret != 0) {
        dms_err("Transfer device id failed. (virt_id=%u; ret=%d)", virt_id, ret);
        return ret;
    }

    is_in_container = devdrv_manager_container_is_in_container();
    if ((is_in_container == true) && (vfid > 0)) {
        is_container_split = true;
    }

    ret = dms_get_chip_aicore_info(chip_info->dev_id, vfid, is_container_split, &chip_aicore_info);
    if (ret != 0) {
        dms_err("Get chip version register info fail. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id,
                ret);
        return ret;
    }

    ret = dms_set_chip_type(chip_info->dev_id, chip_info, virt_id);
    if (ret != 0) {
        dms_err("Copy chip type failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

    ret = dms_set_chip_name(chip_info, is_container_split, &chip_aicore_info, virt_id);
    if (ret != 0) {
        dms_err("Set chip name failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

    ret = dms_set_chip_template_name(chip_info->dev_id, vfid, chip_info);
    if (ret != 0) {
        dms_err("Set chip template name failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

    ret = dms_update_chip_info(chip_info, &chip_aicore_info);
    if (ret != 0) {
        dms_err("update chip info failed. (virt_id=%u; dev_id=%u; ret=%d)\n", virt_id, chip_info->dev_id, ret);
        return ret;
    }

    return 0;
}

STATIC int dms_get_phy_device_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    unsigned int phy_id;
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    struct dms_get_phy_dev_info_out *output = (struct dms_get_phy_dev_info_out *)out;

    if ((feature == NULL) || (in == NULL) || (in_len < sizeof(unsigned int)) || (out == NULL) ||
        (out_len < sizeof(struct dms_get_phy_dev_info_out))) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s; out_len=%u)\n",
                (feature == NULL) ? "NULL" : "OK", (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK",
                out_len);
        return -EINVAL;
    }

    phy_id = *(unsigned int *)in;
    if (!uda_is_udevid_exist(phy_id)) {
        dms_err("Physical id is invalid. (phy_id=%u)\n", phy_id);
        return -EINVAL;
    }

    dev_cb = dms_get_dev_cb(phy_id);
    if (dev_cb == NULL) {
        dms_err("Get device ctrl block failed. (phy_id=%u)\n", phy_id);
        return -ENODEV;
    }

    dev_info = (struct devdrv_info *)dev_cb->dev_info;
    if (dev_info == NULL) {
        dms_err("Get device info structure failed. (phy_id=%u)\n", phy_id);
        return -ENODEV;
    }

    output->chip_id = dev_info->chip_id;
    output->die_id = dev_info->die_id;
    return 0;
}

#ifdef CFG_FEATURE_GET_DEV_UUID
/************************************************  UUID  ********************************************************
*  UUID format: |---0--|--1--|--2--|-------3--------|--4--|--5--|--6--|--7---|-----8-----|--9---|-----10~15-----|
*     type:     |  Rsv | Vendor ID | Rsv1 | version | Device id |    Rsv2    | Vnpu id |  Rsv3  |       SN      |
*     size:     | 8bit |   16bit   | 4bit |  4bit   |   16bit   |    16bit   |  6bit   |  10bit |     48bit     |
----------------|------|-----------|------|---------|-----------|------------|---------|--------|---------------|
* value(910B):  |   0  |  0xCC08   |  0   |    0    |  0xD000   |     0      |  vfid   |   0    |    mac_addr   |
* value(910_93):|   0  |  0xCC08   |  0   |    0    |  0xD001   |     0      |  vfid   |   0    |    mac_addr   |
----------------------------------------------------------------------------------------------------------------|
*  UUID format: |--0--|--1--|--2--|--3--|-------4--------|----------5~7----------|---8~15---|
*     type:     | Vendor ID | Device id | version | type | Vnpu id |    Rsv1     |    SN    |
*     size:     |   16bit   |   16bit   |  4bit   | 4bit |  6bit   |    18bit    |  64bit   |
----------------|-----------|-----------|---------|------|---------|-------------|----------|
* value(950):   |  0xCC08   | Device_id | version | type |  vfid   |      0      |    sn    |
* value(960):   |  0xCC08   | Device_id | version | type |  vfid   |      0      |    sn    |
----------------------------------------------------------------------------------------------------------------|
****************************************************************************************************************/
#define DEVICE_VERSION 0
#define VENDOR_ID 0xCC08
STATIC int dms_make_up_uuid_by_guid(unsigned int phy_id, unsigned int vfid, char *uuid_info)
{
    void __ka_mm_iomem *guid_addr = NULL;
    char guid_info[GUID_BASE_LEN];
    void __ka_mm_iomem *guid_group_addr = NULL;

    guid_group_addr = ka_mm_ioremap(CONTROLLER_GUID_GROUP_ADDR, GUID_BASE_LEN);
    if (guid_group_addr == NULL) {
        dms_err("guid_group_addr ioremap fail. (phy_id=%u;)\n", phy_id);
        return -EINVAL;
    }
    if (ka_mm_readl(guid_group_addr) != 3u) {
        ka_mm_writel(0x3, guid_group_addr);
    }
    ka_mm_iounmap(guid_group_addr);
    guid_group_addr = NULL;

    int ret = memset_s(guid_info, GUID_BASE_LEN, 0, GUID_BASE_LEN);
    if (ret != 0) {
        dms_err("memset_s failed. (phy_id=%u; vfid=%u; ret=%d)\n", phy_id, vfid, ret);
        return -EINVAL;
    }
    for (int i = 0; i < UUID_MAX_LEN / GUID_BASE_LEN; i++) {
        guid_addr = ka_mm_ioremap(GUID_BASE_ADDR + GUID_BASE_LEN * i, GUID_BASE_LEN);
        if (guid_addr == NULL) {
            dms_err("guid addr ioremap fail. (phy_id=%u; vfid=%u)\n", phy_id, vfid);
            return -EINVAL;
        }
        ka_mm_memcpy_fromio(guid_info, guid_addr, GUID_BASE_LEN);

        ka_mm_iounmap(guid_addr);
        guid_addr = NULL;
        for (int j = 0; j < GUID_BASE_LEN; j++) {
            uuid_info[UUID_MAX_LEN - GUID_BASE_LEN * i - j - 1] = guid_info[j];
        }
    }
    /* The UUID starts with Vnpu ID at index 5, and the vfid(6 bits in total) needs to be left-shifted by 2 bits. */
    uuid_info[5] = (((uint8_t)vfid) & 0x3F) << 2;
    /* The UUID has reserved part at index 6. */
    uuid_info[6] = 0x00;
    /* The UUID has reserved part at index 7. */
    uuid_info[7] = 0x00;
    return 0;
}

STATIC int dms_make_up_uuid_by_devid(unsigned int device_id, unsigned int phy_id, unsigned int vfid, char *uuid_info)
{
    uuid_info[1] = (VENDOR_ID >> 8) & 0xFF;
    uuid_info[2] = VENDOR_ID & 0xFF;
    uuid_info[3] = (0 << 4) | DEVICE_VERSION;
    uuid_info[4] = (device_id >> 8) & 0xFF;
    uuid_info[5] = device_id & 0xFF;
    uuid_info[8] = (((uint8_t)vfid) & 0x3F) << 2;

    /* The UUID starts with SN at index 10, and the MAC address starts at index 5, 48 bits in total.*/
    for (int i = 0; i < 6; i++) {
        uuid_info[10 + i] = g_mac_info[phy_id][5 + i];
    }
    return 0;
}

STATIC int dms_make_up_uuid(unsigned int phy_id, unsigned int vfid, unsigned int soc_type,
                            struct dms_hal_device_info_stru *output)
{
    char uuid_info[UUID_MAX_LEN];
    unsigned int device_id = 0;
    int ret = 0;

    if (phy_id >= ASCEND_PDEV_MAX_NUM) {
        dms_err("Invalid phy_id. (phy_id=%u; vfid=%u)\n", phy_id, vfid);
        return -EINVAL;
    }

    if (ka_base_atomic_read(&g_mac_init_flag[phy_id]) == 0) {
        dms_err("Mac info init failed. (phy_id=%u; vfid=%u; ret=%d)\n", phy_id, vfid, ret);
        return -EINVAL;
    }

    if (soc_type == SOC_TYPE_CLOUD_V3) {
        device_id = 0xD001;
    } else {
        device_id = 0xD000;
    }

    ret = memset_s(uuid_info, UUID_MAX_LEN, 0, UUID_MAX_LEN);
    if (ret != 0) {
        dms_err("memset_s failed. (phy_id=%u; vfid=%u; ret=%d)\n", phy_id, vfid, ret);
        return -EINVAL;
    }

    if ((soc_type == SOC_TYPE_CLOUD_V4) || (soc_type == SOC_TYPE_CLOUD_V5) || (soc_type == SOC_TYPE_MINI_V4) ||
        (soc_type == SOC_TYPE_CLOUD_V7) || (soc_type == SOC_TYPE_MINI_V5)) {
        ret = dms_make_up_uuid_by_guid(phy_id, vfid, uuid_info);
    } else {
        ret = dms_make_up_uuid_by_devid(device_id, phy_id, vfid, uuid_info);
    }
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to get uuid. (phy_id=%u; vfid=%u; ret=%d)\n", phy_id, vfid, ret);
        return ret;
    }

    ret = memcpy_s(output->payload, sizeof(output->payload), uuid_info, UUID_MAX_LEN);
    if (ret != 0) {
        dms_err("Failed to invoke memcpy_s to copy uuid info. (phy_id=%u; vfid=%u; ret=%d)\n", phy_id, vfid, ret);
        return -EINVAL;
    }

    output->buff_size = UUID_MAX_LEN;
    return 0;
}

STATIC int dms_get_device_uuid(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    unsigned int phy_id, vfid;
    struct uda_mia_dev_para mia_para = {0};
    unsigned int soc_type = CHIP_END;
    struct dms_hal_device_info_stru *input = (struct dms_hal_device_info_stru *)in;
    struct dms_hal_device_info_stru *output = (struct dms_hal_device_info_stru *)out;

    if ((feature == NULL) || (in == NULL) || (in_len < DMS_HAL_DEV_INFO_HEAD_LEN) || (out == NULL) ||
        (out_len < DMS_HAL_DEV_INFO_HEAD_LEN)) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s; out_len=%u)\n",
                (feature == NULL) ? "NULL" : "OK", (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK",
                out_len);
        return -EINVAL;
    }

    if ((input->buff_size == 0) || (input->buff_size < UUID_MAX_LEN) ||
        (in_len < (DMS_HAL_DEV_INFO_HEAD_LEN + input->buff_size))) {
        dms_err("Invalid parameter. (in_len=%u; buff_size=%u)\n", in_len, input->buff_size);
        return -EINVAL;
    }

    if (!uda_is_pf_dev(input->dev_id)) {
        ret = uda_udevid_to_mia_devid(input->dev_id, &mia_para);
        if (ret != 0) {
            dms_err("Failed to convert udevid to mia_devid. (udev_id=%u; ret=%d)", input->dev_id, ret);
            return ret;
        }
        phy_id = mia_para.phy_devid;
        vfid = mia_para.sub_devid + 1;
    } else {
        phy_id = input->dev_id;
        vfid = 0;
    }

    ret = hal_kernel_get_soc_type(0, &soc_type);
    if ((ret != 0) || (soc_type == (uint32_t)CHIP_END)) {
        dms_err("Get soc type failed. (ret=%d; soc_type=%u)\n", ret, soc_type);
        return -EINVAL;
    }

    ret = dms_make_up_uuid(phy_id, vfid, soc_type, output);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Failed to get uuid. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    return 0;
}

STATIC int dms_get_uuid_thread(void *arg)
{
    int ret = 0;
    u32 dev_id = (u32)(uintptr_t)arg;
    u32 out_len = 0;

    ret = devdrv_config_get_mac_info(dev_id, g_mac_info[dev_id], UUID_MAX_LEN, &out_len);
    if (ret != 0) {
        dms_err("Get mac info failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ka_base_atomic_set(&g_mac_init_flag[dev_id], 1);
    dms_info("Get mac info success from flash.\n");
    (void)arg;

    return 0;
}

STATIC void dms_mac_info_init(unsigned int dev_id)
{
    ka_task_struct_t *release_task = NULL;

    if (!uda_is_phy_dev(dev_id)) {
        dms_info("Only support pf device. (dev_id=%d)\n", dev_id);
        return;
    }

    release_task = ka_task_kthread_create(dms_get_uuid_thread, (void *)(uintptr_t)dev_id, "dms_get_uuid");
    if (KA_IS_ERR(release_task) || (release_task == NULL)) {
        dms_err("get uuid Kthread not up to expectations.\n");
        return;
    }

    (void)ka_task_wake_up_process(release_task);
    return;
}

STATIC void dms_mac_info_uninit(unsigned int dev_id)
{
    (void)dev_id;
    return;
}
#endif

#ifdef CFG_FEATURE_GET_DEV_INDEX_IN_GROUP
#define SINGLE_PCIE_CARD 0b01101000

STATIC int dms_get_index_in_group_para_check(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    if ((feature == NULL) || (in == NULL) || (in_len != sizeof(struct dms_get_device_info_in)) || (out == NULL) ||
        (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s;out_len=%u)\n",
                (feature == NULL) ? "NULL" : "OK", (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK",
                out_len);
        return -EINVAL;
    }
    return 0;
}

STATIC int dms_get_device_index_in_group(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret;
    unsigned int udevid = 0;
    struct dms_get_device_info_in *input = NULL;
    struct dms_get_device_info_out *output = NULL;
    soc_res_board_hw_info_t hw_info = {0};

    ret = dms_get_index_in_group_para_check(feature, in, in_len, out, out_len);
    if (ret != 0) {
        return ret;
    }

    input = (struct dms_get_device_info_in *)in;
    if ((input->buff == NULL) || (input->buff_size == 0)) {
        dms_err("Input buff is NULL or buff_size is zero. (dev_id=%u; buff_size=%u)\n", input->dev_id,
                input->buff_size);
        return -EINVAL;
    }

    ret = uda_devid_to_udevid(input->dev_id, &udevid);
    if (ret != 0) {
        dms_err("Fail to convert devid to udevid. (ret=%d; devid=%u)\n", ret, input->dev_id);
        return -EINVAL;
    }

    ret = soc_resmng_dev_get_attr(udevid, BOARD_HW_INFO, &hw_info, sizeof(hw_info));
    if (ret != 0) {
        devdrv_drv_err("Get board hardware info from soc res failed. (dev_id=%u; ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    if (hw_info.mainboard_id == SINGLE_PCIE_CARD) {
        dms_info("This is a 1p pcie card. (dev_id=%u)\n", input->dev_id);
        hw_info.chip_id = 0;
    }

    output = (struct dms_get_device_info_out *)out;
    ret = (int)copy_to_user(input->buff, &hw_info.chip_id, sizeof(unsigned char));
    if (ret != 0) {
        dms_err("Copy_to_user failed. (udevid=%u; ret=%d)\n", input->dev_id, ret);
        return -EINVAL;
    }
    output->out_size = sizeof(unsigned char);

    return ret;
}
#endif

#ifdef CFG_HOST_ENV
STATIC int dms_cust_board_id_check(unsigned int dev_id, unsigned int *udevid)
{
    unsigned int chip_type;
    int ret;

    ret = uda_devid_to_udevid(dev_id, udevid);
    if (ret != 0) {
        dms_err("Fail to cnvert devid to udevid. (devid=%u; ret=%u)\n", dev_id, ret);
        return -EINVAL;
    }

    if (*udevid >= ASCEND_PDEV_MAX_NUM) {
        dms_err("Invalid parameter. (logic_id=%u; udevid=%u; udevid_max=%u)\n", dev_id, *udevid, ASCEND_PDEV_MAX_NUM);
        return -EINVAL;
    }

    chip_type = uda_get_chip_type(*udevid);
    if ((chip_type != HISI_CLOUD_V4) && (chip_type != HISI_CLOUD_V5) && (chip_type != HISI_MINI_V4)) {
        return -EOPNOTSUPP;
    }

    return 0;
}

STATIC int dms_get_cust_board_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_get_device_info_in *input = (struct dms_get_device_info_in *)in;
    struct dms_get_device_info_out *output = (struct dms_get_device_info_out *)out;
    unsigned int udevid;
    int ret;

    if ((in == NULL) || (in_len != sizeof(struct dms_get_device_info_in)) || (out == NULL) ||
        (out_len != sizeof(struct dms_get_device_info_out))) {
        dms_err("Invalid parameter. (in_is_null=%d; in_len_invalid=%u; in_len_valid=%zu; "
                "out_is_null=%d; out_len_invalid=%u; out_len_valid=%zu)\n",
                in == NULL, in_len, sizeof(struct dms_get_device_info_in), out == NULL, out_len,
                sizeof(struct dms_get_device_info_out));
        return -EINVAL;
    }

    if ((input->buff == NULL) || (input->buff_size == 0)) {
        dms_err("Invalid parameter. (devid=%u; buff_is_null=%d;)\n", input->dev_id, input->buff == NULL);
        return -EINVAL;
    }

    ret = dms_cust_board_id_check(input->dev_id, &udevid);
    if (ret != 0) {
        return ret;
    }

    ka_task_mutex_lock(&g_cust_board_info[udevid].lock);
    if (g_cust_board_info[udevid].data == NULL) {
        ka_task_mutex_unlock(&g_cust_board_info[udevid].lock);
        return -EOPNOTSUPP;
    }

    if (input->buff_size < g_cust_board_info[udevid].size) {
        ka_task_mutex_unlock(&g_cust_board_info[udevid].lock);
        dms_err("Invalid parameter. (input_buff_size=%u; cust_board_info_size=%u)\n", input->buff_size,
                g_cust_board_info[udevid].size);
        return -EINVAL;
    }

    output->out_size = g_cust_board_info[udevid].size;
    ret = ka_base_copy_to_user(input->buff, g_cust_board_info[udevid].data, g_cust_board_info[udevid].size);
    ka_task_mutex_unlock(&g_cust_board_info[udevid].lock);
    if (ret != 0) {
        dms_err("Data copy failed. (udevid=%u; ret=%u)\n", udevid, ret);
        return -EINVAL;
    }

    return 0;
}

STATIC int dms_set_cust_board_info(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dms_set_device_info_in *input = (struct dms_set_device_info_in *)in;
    unsigned int udevid;
    int ret;

    if ((in == NULL) || (in_len != sizeof(struct dms_set_device_info_in))) {
        dms_err("Invalid parameter. (in_is_null=%d; in_len_invalid=%u; in_len_valid=%zu)\n", in == NULL, in_len,
                sizeof(struct dms_set_device_info_in));
        return -EINVAL;
    }

    if ((input->buff == NULL) || (input->buff_size > DMS_CUST_BOARD_INFO_MAX_SIZE)) {
        dms_err("The buffer is NULL or the data is too large. (buf_is_NULL=%d; buff_size=%u; max_size=%d)\n",
                (input->buff == NULL), input->buff_size, DMS_CUST_BOARD_INFO_MAX_SIZE);
        return -EINVAL;
    }

    ret = dms_cust_board_id_check(input->dev_id, &udevid);
    if (ret != 0) {
        return ret;
    }

    ka_task_mutex_lock(&g_cust_board_info[udevid].lock);
    if (g_cust_board_info[udevid].data == NULL) {
        g_cust_board_info[udevid].data = (u8 *)dbl_kzalloc(DMS_CUST_BOARD_INFO_MAX_SIZE,
                                                           KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (g_cust_board_info[udevid].data == NULL) {
            ka_task_mutex_unlock(&g_cust_board_info[udevid].lock);
            dms_err("Kzalloc failed. (udevid=%u)\n", udevid);
            return -EINVAL;
        }
    }

    ret = ka_base_copy_from_user(g_cust_board_info[udevid].data, input->buff, input->buff_size);
    if (ret != 0) {
        dbl_kfree(g_cust_board_info[udevid].data);
        g_cust_board_info[udevid].data = NULL;
        g_cust_board_info[udevid].size = 0;
        ka_task_mutex_unlock(&g_cust_board_info[udevid].lock);
        dms_err("Data copy failed. (udevid=%u; ret=%u)\n", udevid, ret);
        return -EINVAL;
    }
    g_cust_board_info[udevid].size = input->buff_size;
    ka_task_mutex_unlock(&g_cust_board_info[udevid].lock);
    dms_info("Set cust board info success. (udevid=%u; size=%u)\n", udevid, input->buff_size);
    return 0;
}
#endif

#if (defined CFG_FEATURE_UPGRADE_SWPLUGIN_POLICY) && (!defined CFG_HOST_ENV)
STATIC int dms_get_swplugin_upgrade_policy(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    unsigned int phy_id, vfid;
    unsigned int tmp_value;
    struct dms_hal_device_info_stru *input = (struct dms_hal_device_info_stru *)in;
    struct dms_hal_device_info_stru *output = (struct dms_hal_device_info_stru *)out;

    if ((feature == NULL) || (in == NULL) || (in_len < DMS_HAL_DEV_INFO_HEAD_LEN) || (out == NULL) ||
        (out_len < DMS_HAL_DEV_INFO_HEAD_LEN)) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s; out_len=%u)\n",
                (feature == NULL) ? "NULL" : "OK", (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK",
                out_len);
        return -EINVAL;
    }

    if ((input->buff_size == 0) || (input->buff_size < sizeof(unsigned int)) ||
        (in_len < (DMS_HAL_DEV_INFO_HEAD_LEN + input->buff_size))) {
        dms_err("Invalid parameter. (in_len=%u; buff_size=%u)\n", in_len, input->buff_size);
        return -EINVAL;
    }

    ret = uda_udevid_to_phy_devid(input->dev_id, &phy_id, &vfid);
    if (ret != 0) {
        dms_err("Failed to invoke uda_udevid_to_phy_devid. (ret=%d; dev_id=%u)\n", ret, input->dev_id);
        return ret;
    }

    tmp_value = (unsigned int)ka_base_atomic_read(&g_swplugin_upgrade_policy[phy_id]);
    ret = memcpy_s(output->payload, sizeof(output->payload), &tmp_value, sizeof(tmp_value));
    if (ret != 0) {
        dms_err("Failed to invoke memcpy_s to copy swplugin upgrade policy. (phy_id=%u; vfid=%u; ret=%d)\n", phy_id,
                vfid, ret);
        return -EINVAL;
    }

    output->buff_size = sizeof(unsigned int);
    return 0;
}

STATIC int dms_set_swplugin_upgrade_policy(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int ret = 0;
    unsigned int phy_id, vfid;
    unsigned int tmp_value;
    struct dms_hal_device_info_stru *input = (struct dms_hal_device_info_stru *)in;

    if ((feature == NULL) || (in == NULL) || (in_len < DMS_HAL_DEV_INFO_HEAD_LEN) || (out == NULL) ||
        (out_len < DMS_HAL_DEV_INFO_HEAD_LEN)) {
        dms_err("Invalid parameter. (feature=%s; in=%s; in_len=%u; out=%s; out_len=%u)\n",
                (feature == NULL) ? "NULL" : "OK", (in == NULL) ? "NULL" : "OK", in_len, (out == NULL) ? "NULL" : "OK",
                out_len);
        return -EINVAL;
    }

    if ((input->buff_size != sizeof(unsigned int)) || (in_len < (DMS_HAL_DEV_INFO_HEAD_LEN + input->buff_size))) {
        dms_err("Invalid parameter. (device_id=%u; buff_size=%u)\n", input->dev_id, input->buff_size);
        return -EINVAL;
    }

    ret = uda_udevid_to_phy_devid(input->dev_id, &phy_id, &vfid);
    if (ret != 0) {
        dms_err("Failed to invoke uda_udevid_to_phy_devid. (ret=%d; dev_id=%u)\n", ret, input->dev_id);
        return ret;
    }

    ret = memcpy_s(&tmp_value, sizeof(tmp_value), input->payload, input->buff_size);
    if (ret != 0) {
        dms_err("Failed to invoke memcpy_s to copy swplugin upgrade policy. (phy_id=%u; vfid=%u; ret=%d)\n", phy_id,
                vfid, ret);
        return -EINVAL;
    }

    if (tmp_value >= UPGRADE_POLICY_MAX) {
        dms_err("Invalid swplugin upgrade policy. (upgrade_policy=%d; upgrade_policy_max=%u)\n", tmp_value,
                UPGRADE_POLICY_MAX - 1);
        return -EINVAL;
    }

    ka_base_atomic_set(&g_swplugin_upgrade_policy[phy_id], (int)tmp_value);
    dms_event("success to set swplugin upgrade policy. (dev_id=%u; upgrade_policy=%u)\n", input->dev_id, tmp_value);
    return 0;
}
#endif

#define DMS_CHIP_INFO_CMD_NAME "DMS_CHIPINFO"
BEGIN_DMS_MODULE_DECLARATION(DMS_CHIP_INFO_CMD_NAME)
BEGIN_FEATURE_COMMAND()
#ifdef CFG_FEATURE_GET_DEV_UUID
ADD_FEATURE_COMMAND(DMS_CHIP_INFO_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x36", NULL,
                    DMS_SUPPORT_ALL, dms_get_device_uuid)
#endif
ADD_FEATURE_COMMAND(DMS_CHIP_INFO_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_PHY_DEVICE_INFO, NULL, NULL,
                    DMS_SUPPORT_ALL_USER, dms_get_phy_device_info)
#ifdef CFG_FEATURE_GET_DEV_INDEX_IN_GROUP
ADD_FEATURE_COMMAND(DMS_CHIP_INFO_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "main_cmd=0xc,sub_cmd=0x5", NULL,
                    DMS_ACC_NOT_LIMIT_USER | DMS_ENV_ALL | DMS_VDEV_NOTSUPPORT, dms_get_device_index_in_group)
#endif
#ifdef CFG_HOST_ENV
ADD_FEATURE_COMMAND(DMS_CHIP_INFO_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD,
                    "main_cmd=0xc,sub_cmd=0x3", /* DSMI_MAIN_CMD_CHIP_INF,DSMI_CHIP_INF_SUB_CMD_CUST_BOARD_INF */
                    NULL, DMS_SUPPORT_ALL, dms_get_cust_board_info)
ADD_FEATURE_COMMAND(DMS_CHIP_INFO_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD,
                    "main_cmd=0xc,sub_cmd=0x3", /* DSMI_MAIN_CMD_CHIP_INF,DSMI_CHIP_INF_SUB_CMD_CUST_BOARD_INF */
                    NULL, DMS_SUPPORT_ALL, dms_set_cust_board_info)
#endif
#if (defined CFG_FEATURE_UPGRADE_SWPLUGIN_POLICY) && (!defined CFG_HOST_ENV)
ADD_FEATURE_COMMAND(DMS_CHIP_INFO_CMD_NAME, DMS_GET_GET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x3e", NULL,
                    DMS_SUPPORT_ALL, dms_get_swplugin_upgrade_policy)
ADD_FEATURE_COMMAND(DMS_CHIP_INFO_CMD_NAME, DMS_GET_SET_DEVICE_INFO_CMD, ZERO_CMD, "module=0x0,info=0x3e", NULL,
                    DMS_ACC_ROOT | DMS_ENV_NOT_NORMAL_DOCKER | DMS_VDEV_NOTSUPPORT, dms_set_swplugin_upgrade_policy)
#endif
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

int dms_chip_info_adapt_dev_init(u32 dev_id)
{
#ifdef CFG_FEATURE_GET_DEV_UUID
    dms_mac_info_init(dev_id);
#endif
    dms_info("dms chip info dev init success. (dev_id=%u)\n", dev_id);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(dms_chip_info_adapt_dev_init, FEATURE_LOADER_STAGE_5);

void dms_chip_info_adapt_dev_uninit(u32 dev_id)
{
#ifdef CFG_FEATURE_GET_DEV_UUID
    dms_mac_info_uninit(dev_id);
#endif
    dms_info("dms chip info dev uninit success. (dev_id=%u)\n", dev_id);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(dms_chip_info_adapt_dev_uninit, FEATURE_LOADER_STAGE_5);

int dms_chip_info_adapt_init(void)
{
    unsigned int i;
    for (i = 0; i < ASCEND_PDEV_MAX_NUM; i++) {
        ka_task_mutex_init(&g_cust_board_info[i].lock);
#if (defined CFG_FEATURE_UPGRADE_SWPLUGIN_POLICY) && (!defined CFG_HOST_ENV)
#ifdef CFG_BUILD_DEBUG
        ka_base_atomic_set(&g_swplugin_upgrade_policy[i], UPGRADE_POLICY_FORCE);
#else
        ka_base_atomic_set(&g_swplugin_upgrade_policy[i], UPGRADE_POLICY_NORMAL);
#endif
#endif
    }

    CALL_INIT_MODULE(DMS_CHIP_INFO_CMD_NAME);
    dms_info("dms chip info init success.\n");
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_chip_info_adapt_init, FEATURE_LOADER_STAGE_5);

void dms_chip_info_adapt_uninit(void)
{
    unsigned int i;

    for (i = 0; i < ASCEND_PDEV_MAX_NUM; i++) {
        if (g_cust_board_info[i].data != NULL) {
            dbl_kfree(g_cust_board_info[i].data);
            g_cust_board_info[i].data = NULL;
        }
        ka_task_mutex_destroy(&g_cust_board_info[i].lock);
    }
    CALL_EXIT_MODULE(DMS_CHIP_INFO_CMD_NAME);
    dms_info("dms chip info uninit success.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(dms_chip_info_adapt_uninit, FEATURE_LOADER_STAGE_5);
