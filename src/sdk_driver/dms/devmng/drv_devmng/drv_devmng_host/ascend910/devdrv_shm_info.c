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

#include "devdrv_manager.h"
#include "devdrv_manager_common.h"
#include "dms/dms_shm_info.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_soc_res.h"
#include "adapter_api.h"
#include "comm_kernel_interface.h"
#include "vmng_kernel_interface.h"
#include "ka_memory_pub.h"

int devdrv_manager_shm_info_check(struct devdrv_info *dev_info)
{
    if ((dev_info == NULL) || (dev_info->shm_status == NULL) || (dev_info->shm_head == NULL)) {
        devdrv_drv_err("dev_info is NULL or dev_info->shm_status/shm_head is NULL.\n");
        return -EFAULT;
    }

    if ((dev_info->shm_head->head_info.version != DEVMNG_SHM_INFO_HEAD_VERSION) ||
        (dev_info->shm_head->head_info.magic != DEVMNG_SHM_INFO_HEAD_MAGIC)) {
        devdrv_drv_warn("dev(%u) version of share memory in host is 0x%llx, "
                        "the version in device is 0x%llx, magic is 0x%x.\n",
                        dev_info->dev_id, DEVMNG_SHM_INFO_HEAD_VERSION, dev_info->shm_head->head_info.version,
                        dev_info->shm_head->head_info.magic);
        return -EFAULT;
    }

    return 0;
}

int devmng_dms_get_health_code(u32 devid, u32 *health_code, u32 health_len)
{
    struct devdrv_info *dev_info = NULL;
    int i, ret;

    if ((devid >= ASCEND_DEV_MAX_NUM) || (health_code == NULL) || (health_len != VMNG_VDEV_MAX_PER_PDEV)) {
        devdrv_drv_err("Invalid parameter. (devid=%u; health_code=\"%s\"; health_len=%u)\n", devid,
                       (health_code == NULL) ? "NULL" : "OK", health_len);
        return -EINVAL;
    }

    dev_info = devdrv_manager_get_devdrv_info(devid);
    ret = devdrv_try_get_dev_info_occupy(dev_info);
    if (ret != 0) {
        devdrv_drv_err("Get dev_info occupy failed. (ret=%d; devid=%u)\n", ret, devid);
        return ret;
    }

    if (devdrv_manager_shm_info_check(dev_info)) {
        devdrv_drv_err("The dev_info is invalid. (devid=%u)\n", devid);
        devdrv_put_dev_info_occupy(dev_info);
        return -EFAULT;
    }

    for (i = 0; i < VMNG_VDEV_MAX_PER_PDEV; i++) {
        health_code[i] = dev_info->shm_status->dms_health_status[i];
    }
    devdrv_put_dev_info_occupy(dev_info);

    return 0;
}

#ifdef CFG_FEATURE_SHM_DEVMNG
int devdrv_manager_get_pf_vf_id(u32 udevid, u32 *pf_id, u32 *vf_id)
{
    int ret;

    if (!uda_is_phy_dev(udevid)) {
        /* mia device */
        struct uda_mia_dev_para mia_para = {0};
        ret = uda_udevid_to_mia_devid(udevid, &mia_para);
        if (ret != 0) {
            devdrv_drv_err("Dev udevid to mia devid failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
        *pf_id = mia_para.phy_devid;
        *vf_id = mia_para.sub_devid;
    } else {
        *pf_id = udevid;
        *vf_id = 0;
    }

    return 0;
}

STATIC int devmng_get_mem_by_devmanage_shm(u32 dev_id, u64 *shm_addr, size_t *shm_size)
{
    int ret;
    struct soc_rsv_mem_info rsv_mem = {0};

    ret = soc_resmng_dev_get_rsv_mem(dev_id, "DEVMNG_RSV_MEM", &rsv_mem);
    if (ret != 0) {
        devdrv_drv_err("Get devmng rsv mem addr failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    *shm_addr = rsv_mem.rsv_mem;
    *shm_size = rsv_mem.rsv_mem_size;

    return ret;
}
#endif

STATIC int devmng_shm_init_pcie(struct devdrv_info *dev_info)
{
    size_t shm_size;
    u64 shm_addr;
    int ret;

#ifdef CFG_FEATURE_SHM_DEVMNG
    ret = devmng_get_mem_by_devmanage_shm(dev_info->dev_id, &shm_addr, &shm_size);
    if (ret != 0) {
        devdrv_drv_err("Get shm addr by devmanage fail. (dev_id=%u; ret=%d)\n", dev_info->dev_id, ret);
        return ret;
    }
#else
    ret = adap_get_addr_info(dev_info->dev_id, DEVDRV_ADDR_DEVMNG_RESV_BASE, 0, &shm_addr, &shm_size);
    if (ret != 0) {
        devdrv_drv_err("Get shm addr by pcie fail. (dev_id=%u; ret=%d)\n", dev_info->dev_id, ret);
        return ret;
    }
#endif

    dev_info->shm_size = shm_size;
    dev_info->shm_vaddr = ka_mm_ioremap(shm_addr, shm_size);
    if (dev_info->shm_vaddr == NULL) {
        devdrv_drv_err("[devid=%u] ka_mm_ioremap shm_vaddr fail.\n", dev_info->dev_id);
        return -ENOMEM;
    }

    return 0;
}
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
STATIC int devmng_shm_init_ub(struct devdrv_info *dev_info)
{
    int ret = 0;

    dev_info->shm_size_register_rao = DEVDRV_SHM_GEGISTER_RAO_SIZE;
    dev_info->shm_vaddr = ka_mm_kzalloc(dev_info->shm_size_register_rao, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (dev_info->shm_vaddr == NULL) {
        devdrv_drv_err("Kzalloc rao mem fail. (dev_id=%u, shm_size=0x%x)\n", dev_info->dev_id, dev_info->shm_size);
        return -ENOMEM;
    }

    ret = devdrv_register_rao_client(dev_info->dev_id, DEVDRV_RAO_CLIENT_DEVMNG, (u64)(dev_info->shm_vaddr),
                                     dev_info->shm_size_register_rao, DEVDRV_RAO_PERM_RMT_READ);
    if (ret != 0) {
        devdrv_drv_err("Register rao client fail. (dev_id=%u, ret=%d)\n", dev_info->dev_id, ret);
        ka_mm_kfree(dev_info->shm_vaddr);
        dev_info->shm_vaddr = NULL;
        return ret;
    }
    devdrv_drv_info("Register raorequest client success. (dev_id=%u, size=0x%x)\n", dev_info->dev_id,
                    dev_info->shm_size_register_rao);

    dev_info->shm_size = dev_info->shm_size_register_rao;
    return ret;
}
#endif

int devmng_shm_init(struct devdrv_info *dev_info)
{
    int ret;

    if (devdrv_get_connect_protocol(dev_info->dev_id) == CONNECT_PROTOCOL_UB) {
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
        ret = devmng_shm_init_ub(dev_info);
#endif
    } else {
        ret = devmng_shm_init_pcie(dev_info);
    }
    if (ret != 0) {
        devdrv_drv_err("Get mem failed. (dev_id=%u; ret=%d)\n", dev_info->dev_id, ret);
        return ret;
    }

    dev_info->shm_head = (U_SHM_INFO_HEAD __ka_mm_iomem *)((uintptr_t)dev_info->shm_vaddr);
    dev_info->shm_head->head_info.offset_soc = sizeof(U_SHM_INFO_HEAD);
    dev_info->shm_head->head_info.offset_board = dev_info->shm_head->head_info.offset_soc + sizeof(U_SHM_INFO_SOC);
    dev_info->shm_head->head_info.offset_status = dev_info->shm_head->head_info.offset_board + sizeof(U_SHM_INFO_BOARD);
    dev_info->shm_head->head_info.offset_heartbeat = dev_info->shm_head->head_info.offset_status +
                                                     sizeof(U_SHM_INFO_STATUS);

    dev_info->shm_soc = (U_SHM_INFO_SOC __ka_mm_iomem *)((uintptr_t)((uintptr_t)dev_info->shm_vaddr +
                                                                     dev_info->shm_head->head_info.offset_soc));
    dev_info->shm_status = (U_SHM_INFO_STATUS __ka_mm_iomem *)((
        uintptr_t)((uintptr_t)dev_info->shm_vaddr + dev_info->shm_head->head_info.offset_status));
    dev_info->shm_board = (U_SHM_INFO_BOARD __ka_mm_iomem *)((uintptr_t)((uintptr_t)dev_info->shm_vaddr +
                                                                         dev_info->shm_head->head_info.offset_board));
    dev_info->shm_heartbeat = (U_SHM_INFO_HEARTBEAT __ka_mm_iomem *)((
        uintptr_t)((uintptr_t)dev_info->shm_vaddr + dev_info->shm_head->head_info.offset_heartbeat));
    return 0;
}

void devmng_shm_uninit(struct devdrv_info *dev_info)
{
    int ret;
    if (devdrv_get_connect_protocol(dev_info->dev_id) == CONNECT_PROTOCOL_UB) {
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
        ret = devdrv_unregister_rao_client(dev_info->dev_id, DEVDRV_RAO_CLIENT_DEVMNG);
        if (ret != 0) {
            devdrv_drv_err("Unregister rao client failed. (dev_id=%u)\n", dev_info->dev_id);
            return;
        }

        if (dev_info->shm_vaddr != NULL) {
            ka_mm_kfree(dev_info->shm_vaddr);
        }
#endif
    } else {
        if (dev_info->shm_vaddr != NULL) {
            ka_mm_iounmap(dev_info->shm_vaddr);
        }
    }

    dev_info->shm_vaddr = NULL;
    dev_info->shm_head = NULL;
    dev_info->shm_soc = NULL;
    dev_info->shm_board = NULL;
    dev_info->shm_status = NULL;
    dev_info->shm_heartbeat = NULL;
    return;
}
