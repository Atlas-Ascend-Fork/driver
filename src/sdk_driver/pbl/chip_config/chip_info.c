/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#include "ascend_platform.h"
#include "ascend_dev_num.h"
#include "ascend_kernel_hal.h"
#include "devdrv_hardware_version.h"
#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "pbl_spod_info.h"
#include "pbl_uda.h"
#include "pbl_soc_res_attr.h"
#include "pbl_soc_res.h"
#include "pbl_chip_config.h"
#include "uda_pub_def.h"

#ifdef DRV_HOST

static KA_TASK_DEFINE_SPINLOCK(dev_base_info_spinlock);
static struct dbl_dev_base_info g_dev_base_info[ASCEND_DEV_MAX_NUM] = {
    {KA_U32_MAX, KA_U32_MAX, KA_U32_MAX, KA_U32_MAX, 0}};

void dbl_set_dev_base_info(u32 dev_id, struct dbl_dev_base_info dev_base_info)
{
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        uda_err("Invalid device id. (dev_id=%u)\n", dev_id);
        return;
    }
    ka_task_spin_lock(&dev_base_info_spinlock);
    g_dev_base_info[dev_id].chip_id = dev_base_info.chip_id;
    g_dev_base_info[dev_id].die_id = dev_base_info.die_id;
    g_dev_base_info[dev_id].addr_mode = dev_base_info.addr_mode;
    g_dev_base_info[dev_id].multi_die = dev_base_info.multi_die;
    g_dev_base_info[dev_id].dev_ready = dev_base_info.dev_ready;
    ka_task_spin_unlock(&dev_base_info_spinlock);
}
KA_EXPORT_SYMBOL_GPL(dbl_set_dev_base_info);

void dbl_dev_base_info_init(u32 dev_id)
{
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        uda_err("Invalid device id. (dev_id=%u; valid range=0-%u)\n", dev_id, ASCEND_DEV_MAX_NUM - 1);
        return;
    }
    ka_task_spin_lock(&dev_base_info_spinlock);
    g_dev_base_info[dev_id].dev_ready = 0;
    ka_task_spin_unlock(&dev_base_info_spinlock);
}
KA_EXPORT_SYMBOL_GPL(dbl_dev_base_info_init);

struct dbl_dev_base_info *dbl_get_dev_base_info(u32 dev_id)
{
    struct dbl_dev_base_info *dev_base_info = NULL;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        uda_err("Invalid device id. (dev_id=%u; valid range=0-%u)\n", dev_id, ASCEND_DEV_MAX_NUM - 1);
        return NULL;
    }
    ka_task_spin_lock(&dev_base_info_spinlock);
    if (g_dev_base_info[dev_id].dev_ready == 0) {
        ka_task_spin_unlock(&dev_base_info_spinlock);
        return NULL;
    }
    dev_base_info = &g_dev_base_info[dev_id];
    ka_task_spin_unlock(&dev_base_info_spinlock);
    return dev_base_info;
}
KA_EXPORT_SYMBOL_GPL(dbl_get_dev_base_info);

int hal_kernel_get_device_chip_die_id(u32 dev_id, u32 *chip_id, u32 *die_id)
{
    struct dbl_dev_base_info *dev_base_info = NULL;

    if ((chip_id == NULL) || (die_id == NULL) || (!uda_is_udevid_exist(dev_id))) {
        uda_err("Invalid parameter. (dev_id=%u; chip_id=%s; die_id=%s)\n", dev_id, (chip_id == NULL) ? "NULL" : "OK",
                (die_id == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    dev_base_info = dbl_get_dev_base_info(dev_id);
    if (dev_base_info == NULL) {
        uda_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    *chip_id = dev_base_info->chip_id;
    *die_id = dev_base_info->die_id;
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_device_chip_die_id);

int hal_kernel_get_soc_type(u32 dev_id, u32 *soc_type)
{
    u32 chip_type;
    struct dbl_dev_base_info *dev_base_info = NULL;

    /*
     * soc type map from uda definition to hal kernel
     ****   uda chip type ******* hal soc type ******
     * HISI_MINI_V1  (0)          SOC_TYPE_MINI
     * HISI_CLOUD_V1 (1)          SOC_TYPE_CLOUD
     * HISI_MINI_V2  (2)          SOC_TYPE_DC
     * HISI_CLOUD_V2 (3)          SOC_TYPE_CLOUD_V2 or SOC_TYPE_CLOUD_V3
     * HISI_MINI_V3  (4)          SOC_TYPE_MINI_V3
     * HISI_CLOUD_V4 (6)          SOC_TYPE_CLOUD_V4
     * HISI_CLOUD_V5 (7)          SOC_TYPE_CLOUD_V5
     * HISI_MINI_V4 (10)          SOC_TYPE_MINI_V4
     * HISI_CLOUD_V6 (11)         SOC_TYPE_CLOUD_V6
     * HISI_CLOUD_V7 (12)         SOC_TYPE_CLOUD_V7
     * HISI_MINI_V5_RC (13)       SOC_TYPE_MINI_V5_RC
     * HISI_MINI_V5 (14)          SOC_TYPE_MINI_V5
     * HISI_CHIP_NUM (15)         SOC_TYPE_MAX
     * HISI_CHIP_UNKNOWN (16)     SOC_TYPE_MAX
     */
    u32 chip_type_map[HISI_CHIP_NUM] = {SOC_TYPE_MINI,     SOC_TYPE_CLOUD,      SOC_TYPE_DC,       SOC_TYPE_CLOUD_V2,
                                        SOC_TYPE_MINI_V3,  SOC_TYPE_RSVD_4,     SOC_TYPE_CLOUD_V4, SOC_TYPE_CLOUD_V5,
                                        SOC_TYPE_RSVD_6,   SOC_TYPE_RSVD_7,     SOC_TYPE_MINI_V4,  SOC_TYPE_CLOUD_V6,
                                        SOC_TYPE_CLOUD_V7, SOC_TYPE_MINI_V5_RC, SOC_TYPE_MINI_V5};

    if ((soc_type == NULL) || (!uda_is_udevid_exist(dev_id))) {
        uda_err("Invalid parameter. (dev_id=%u; soc_type=%s)\n", dev_id, (soc_type == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    chip_type = uda_get_chip_type(dev_id);
    dev_base_info = dbl_get_dev_base_info(dev_id);
    if (dev_base_info == NULL) {
        uda_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    if (chip_type >= HISI_CHIP_NUM) {
        *soc_type = SOC_TYPE_MAX;
    } else if (chip_type == HISI_CLOUD_V2) {
        *soc_type = (dev_base_info->multi_die == 1) ? SOC_TYPE_CLOUD_V3 : CHIP_CLOUD_V2;
    } else {
        *soc_type = chip_type_map[chip_type];
    }

    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_soc_type);
#endif

int hal_kernel_get_device_addr_mode(u32 dev_id, HAL_KERNEL_ADDR_MODE *addr_mode)
{
#ifdef DRV_HOST
    struct dbl_dev_base_info *dev_base_info = NULL;
#else
    int ret;
    struct devdrv_hardware_info hardware_info = {0};
#endif

    if ((addr_mode == NULL) || (!uda_is_udevid_exist(dev_id))) {
        uda_err("Invalid parameter. (dev_id=%u; addr_mode=%s)\n", dev_id, (addr_mode == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

#ifdef DRV_HOST
    dev_base_info = dbl_get_dev_base_info(dev_id);
    if (dev_base_info == NULL) {
        uda_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    *addr_mode = (HAL_KERNEL_ADDR_MODE)dev_base_info->addr_mode;
#else
    ret = hal_kernel_get_hardware_info(dev_id, &hardware_info);
    if (ret != 0) {
        uda_err("Device is not initialized. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    *addr_mode = (HAL_KERNEL_ADDR_MODE)hardware_info.base_hw_info.addr_mode;
#endif
    return 0;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_device_addr_mode);

int __attribute__((weak)) hal_kernel_get_spod_node_status(u32 dev_id, u32 sdid, u32 *status)
{
    (void)dev_id;
    (void)sdid;
    (void)status;
    return -EOPNOTSUPP;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_get_spod_node_status);
