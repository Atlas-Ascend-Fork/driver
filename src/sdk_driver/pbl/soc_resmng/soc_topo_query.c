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
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "pbl/pbl_uda.h"
#include "soc_resmng_log.h"
#include "pbl/pbl_soc_res_attr.h"
#include "pbl/pbl_soc_res.h"
#include "ascend_dev_num.h"
#include "ascend_platform.h"
#include "ascend_kernel_hal.h"
#include "comm_kernel_interface.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

typedef struct __soc_topology_check {
    int topology_type;
    int (*topology_check_handler)(unsigned int dev_id1, unsigned int dev_id2, bool *result);
} soc_topology_check_t;

#ifdef CFG_HOST_ENV
STATIC int topo_check_in_the_same_os(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    unsigned int master_id_1 = 0;
    unsigned int master_id_2 = 0;

    master_id_1 = uda_get_master_id(dev_id1);
    master_id_2 = uda_get_master_id(dev_id2);
    if ((master_id_1 == master_id_2) && (master_id_1 != UDA_INVALID_UDEVID) && (master_id_2 != UDA_INVALID_UDEVID)) {
        *result = true;
    } else {
        *result = false;
    }

    return 0;
}
#else
STATIC int topo_check_in_the_same_os(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    int ret;
    unsigned int i;
    unsigned int dev_num = 0;
    unsigned int remote_udevid;
    unsigned int devids[ASCEND_PDEV_MAX_NUM] = {0};

    dev_num = uda_get_detected_phy_dev_num();
    if (dev_num > ASCEND_PDEV_MAX_NUM) {
        soc_err("Failed to obtain the number of devices. (dev_id=%u; dev_num=%d)\n", dev_id1, dev_num);
        return -EINVAL;
    }

    ret = uda_get_cur_ns_udevids(devids, dev_num);
    if (ret != 0) {
        soc_err("Failed to obtain the device IDs. (dev_id=%u; ret=%d)\n", dev_id1, ret);
        return ret;
    }

    for (i = 0; i < dev_num; i++) {
        ret = uda_dev_get_remote_udevid(devids[i], &remote_udevid);
        if ((ret == 0) && (remote_udevid == dev_id2)) {
            *result = true;
            return 0;
        }
    }
    *result = false;

    return 0;
}
#endif

#ifdef CFG_FEATURE_CHIP_DIE
STATIC int topo_check_sio(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    unsigned int remote_udevid;
    int ret;

    ret = uda_dev_get_remote_udevid(dev_id1, &remote_udevid);
    if (ret != 0) {
        soc_err("Failed to obtain the remote udevid. (dev_id=%u; ret=%d)\n", dev_id1, ret);
        return ret;
    } else if (remote_udevid == dev_id2) {
        *result = false;
        return 0;
    }

    ret = topo_check_in_the_same_os(dev_id1, dev_id2, result);
    if (ret != 0) {
        soc_err("Failed to check whether they are in the same OS. (dev_id1=%u; dev_id2=%u; ret=%d)\n", dev_id1, dev_id2,
                ret);
        return ret;
    }

    return 0;
}

/* NOTICE:
    When the mainboard ID is 0x11, the address is unified addressing(same as hccs switch type),
    but connected via PCIe.
 */
#define HCCS_SWITCH_RANGE_START 0x10 /* bitmap: 0001_0000 */
#define HCCS_SWITCH_RANGE_END 0x1F   /* bitmap: 0001_1111 */
/* In order to distinguish between uniform and non-uniform address, return HCCS_SW when dev_id1=devid2.
   Runtime component will converts it back to HCCS.
 */
STATIC int topo_check_hccs_sw(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    soc_res_board_hw_info_t soc_res_hw_info = {0};
    int ret;

    ret = soc_resmng_dev_get_attr(dev_id1, BOARD_HW_INFO, &soc_res_hw_info, sizeof(soc_res_hw_info));
    if (ret != 0) {
        soc_err("Get hardware info from soc resmng failed. (dev_id=%u; ret=%d)\n", dev_id1, ret);
        return ret;
    }

    if ((soc_res_hw_info.mainboard_id >= HCCS_SWITCH_RANGE_START) &&
        (soc_res_hw_info.mainboard_id <= HCCS_SWITCH_RANGE_END)) {
        *result = true;
    } else {
        *result = false;
    }

    return 0;
}
#endif

#ifdef CFG_FEATURE_TOPOLOGY_BY_HCCS_LINK_STATUS
#define SOC_BOARD_TYPE_PCIE 0x10
#define SOC_BOARD_TYPE_EVB 0
#define SOC_BOARD_TYPE_MODULE 1
#define SOC_MODULE_DEVICE_NUM 8
struct topo_id_info {
    unsigned int dev_id;
    unsigned int host_devid;
    unsigned int chip_id;
    unsigned int board_id;
    unsigned int mainboard_id;
    unsigned int hccs_group_id[SOC_HCCS_GROUP_SUPPORT_MAX_CHIPNUM];
};

static int topo_get_id_info(struct topo_id_info *id_info, unsigned int *hccs_link_status)
{
    soc_res_board_hw_info_t soc_res_hw_info = {0};
    int ret;

    ret = uda_dev_get_remote_udevid(id_info->dev_id, &id_info->host_devid);
    if (ret != 0) {
        soc_err("Get remote udevid failed. (dev_id=%u)\n", id_info->dev_id);
        return ret;
    }

    ret = soc_resmng_dev_get_attr(id_info->dev_id, BOARD_HW_INFO, &soc_res_hw_info, sizeof(soc_res_hw_info));
    if (ret != 0) {
        soc_err("Get hardware info from soc resmng failed. (dev_id=%u; ret=%d)\n", id_info->dev_id, ret);
        return ret;
    }
    id_info->chip_id = soc_res_hw_info.chip_id;
    id_info->board_id = soc_res_hw_info.board_id;
    id_info->mainboard_id = soc_res_hw_info.mainboard_id;

    /* hccs_group_id: Check whether they are in the same group. */
    /* hccs_link_status: The bit corresponding to chipid is 1, which is HCCS. */
    ret = soc_resmng_get_hccs_link_status_and_group_id(id_info->dev_id, hccs_link_status, id_info->hccs_group_id,
                                                       SOC_HCCS_GROUP_SUPPORT_MAX_CHIPNUM);
    if (ret != 0) {
        soc_err("Get hccs link status and group id failed. (dev_id=%u; ret=%d)\n", id_info->dev_id, ret);
        return ret;
    }

    return 0;
}

STATIC int topo_get_board_type(unsigned int board_id)
{
    /* board_id bit7~bit4: Board Type */
    if ((board_id & 0xF0) == SOC_BOARD_TYPE_PCIE) {
        return SOC_BOARD_TYPE_PCIE;
    } else if ((board_id & 0xF0) == SOC_BOARD_TYPE_EVB) {
        return SOC_BOARD_TYPE_EVB;
    } else {
        return SOC_BOARD_TYPE_MODULE;
    }
}

STATIC void topo_evb_check_hccs(struct topo_id_info *id_info, unsigned int hccs_link_status, bool *result)
{
    unsigned int evb_device_num = 1;
    unsigned int i;

    if (id_info->mainboard_id == 0) {
        evb_device_num = 2; /* EVB 2P */
    } else if (id_info->mainboard_id == 0x1) {
        evb_device_num = 4; /* EVB 4P */
    }

    for (i = 0; i < evb_device_num; i++) {
        if ((i == id_info->chip_id) || ((id_info->hccs_group_id[id_info->chip_id] == id_info->hccs_group_id[i]) &&
                                        (hccs_link_status & (0x1U << i)))) {
            continue;
        } else {
            *result = false;
            return;
        }
    }
    *result = true;
}

STATIC void topo_module_check_hccs(struct topo_id_info *id_info1, struct topo_id_info *id_info2,
                                   unsigned int hccs_link_status, bool *result)
{
    if (((id_info1->host_devid < SOC_MODULE_DEVICE_NUM) && (id_info2->host_devid < SOC_MODULE_DEVICE_NUM)) ||
        ((id_info1->host_devid >= SOC_MODULE_DEVICE_NUM) && (id_info2->host_devid >= SOC_MODULE_DEVICE_NUM))) {
        id_info2->chip_id = (id_info2->host_devid < SOC_MODULE_DEVICE_NUM) ?
                                id_info2->host_devid :
                                (id_info2->host_devid - SOC_MODULE_DEVICE_NUM);
        if ((id_info1->hccs_group_id[id_info1->chip_id] == id_info1->hccs_group_id[id_info2->chip_id]) &&
            (hccs_link_status & (0x1U << id_info2->chip_id))) {
            *result = true;
        } else {
            *result = false;
        }
    }
}

STATIC int topo_check_hccs(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    struct topo_id_info id_info1 = {0};
    struct topo_id_info id_info2 = {0};
    unsigned int hccs_link_status;
    unsigned int board_type;
    int ret;

    if (dev_id1 >= SOC_HCCS_GROUP_SUPPORT_MAX_CHIPNUM || dev_id2 >= SOC_HCCS_GROUP_SUPPORT_MAX_CHIPNUM) {
        soc_err("Invalid parameter. (dev_id1=%u; dev_id2=%u; max_dev_id=%u)\n", dev_id1, dev_id2,
                SOC_HCCS_GROUP_SUPPORT_MAX_CHIPNUM - 1);
        return -EINVAL;
    }

    id_info1.dev_id = dev_id1;
    id_info2.dev_id = dev_id2;
    id_info2.host_devid = dev_id2;
    ret = topo_get_id_info(&id_info1, &hccs_link_status);
    if (ret != 0) {
        return ret;
    }

    if (id_info1.host_devid == id_info2.host_devid) {
        *result = true;
        return 0;
    }

    board_type = (unsigned int)topo_get_board_type(id_info1.board_id);
    if (board_type == SOC_BOARD_TYPE_EVB) {
        topo_evb_check_hccs(&id_info1, hccs_link_status, result);
    } else if (board_type == SOC_BOARD_TYPE_MODULE) {
        topo_module_check_hccs(&id_info1, &id_info2, hccs_link_status, result);
    } else {
        *result = false;
    }

    return 0;
}
#else
STATIC int topo_check_hccs(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
    int ret;

    ret = topo_check_in_the_same_os(dev_id1, dev_id2, result);
    if (ret != 0) {
        soc_err("Failed to check whether they are in the same OS. (dev_id1=%u; dev_id2=%u; ret=%d)\n", dev_id1, dev_id2,
                ret);
        return ret;
    }

    return 0;
}
#endif

#ifdef CFG_FEATURE_D2D_UB
STATIC int topo_check_ub_for_pcie_card(u32 dev_id1, u32 host_dev_id1, u32 host_dev_id2, bool *result)
{
    struct udevid_reorder_para info = {0};
    soc_res_board_hw_info_t hw_info = {0};
    u32 group_devid_mini = 0;
    u32 group_devid_max = 0;
    int ret;
#ifndef CFG_HOST_ENV
    void __iomem *info_vaddr = NULL;
#endif

    ret = soc_resmng_dev_get_attr(dev_id1, BOARD_HW_INFO, &hw_info, sizeof(soc_res_board_hw_info_t));
    if (ret != 0) {
        soc_err("Get board hw_info failed. (dev_id=%u, ret=%d)\n", dev_id1, ret);
        return ret;
    }
    /* 0x1E: bit[4:1], 0x8: pcie card 1P */
    if ((hw_info.mainboard_id & 0x1E) == 0x8) {
        *result = false;
        return 0;
    }

#ifdef CFG_HOST_ENV
    ret = uda_get_udevid_reorder_para(host_dev_id1, &info);
    if (ret != 0) {
        soc_err("Get reorder info failed. (dev_id1=%u; ret=%d)\n", host_dev_id1, ret);
        return ret;
    }
#else
    info_vaddr = ka_mm_ioremap(BASE_IMP_SRAM_INFO_MEM_ADDR, sizeof(struct udevid_reorder_para));
    if (info_vaddr == NULL) {
        soc_err("Remap imp sram info fail.\n");
        return -ENOMEM;
    }

    info = *(struct udevid_reorder_para __iomem *)info_vaddr;
    ka_mm_iounmap(info_vaddr);
    info_vaddr = NULL;
#endif

    if (info.group_dev_num == 0 || info.group_dev_num > DEV_NUM_IN_GROUP) {
        soc_err("The number of devices in group is invalid. (dev_id=%u; group_dev_num=%u)\n", dev_id1,
                info.group_dev_num);
        return -EINVAL;
    }

    group_devid_mini = host_dev_id1 / info.group_dev_num * info.group_dev_num;
    group_devid_max = host_dev_id1 / info.group_dev_num * info.group_dev_num + info.group_dev_num;
    if ((info.ub_link_status == 1) && (host_dev_id2 >= group_devid_mini) && (host_dev_id2 < group_devid_max)) {
        *result = true;
        return 0;
    } else {
        *result = false;
    }

    return 0;
}
#endif

STATIC int topo_check_ub(unsigned int dev_id1, unsigned int dev_id2, bool *result)
{
#ifdef CFG_FEATURE_D2D_UB
    unsigned long long product_type = 0;
    unsigned int host_dev_id1 = dev_id1;
    unsigned int host_dev_id2 = dev_id2;
    int ret;

#ifdef CFG_HOST_ENV
    if (!uda_is_udevid_exist(dev_id1) || !uda_is_udevid_exist(dev_id2)) {
        soc_err("Device does not exist. (dev_id1=%u; dev_id2=%u)\n", dev_id1, dev_id2);
        return -EINVAL;
    }
#else
    ret = uda_dev_get_remote_udevid(dev_id1, &host_dev_id1);
    if (ret != 0) {
        soc_err("Failed to get remote udevid. (dev_id1=%u; ret=%d)\n", dev_id1, ret);
        return ret;
    }
#endif

    if (host_dev_id1 == host_dev_id2) {
        soc_err("Device id is the same. (dev_id1=%u; dev_id2=%u)\n", dev_id1, dev_id2);
        return -EINVAL;
    }

    ret = soc_resmng_dev_get_key_value(dev_id1, "PRODUCT_TYPE", &product_type);
    if (ret != 0) {
        soc_err("Get product type failed. (dev_id=%u, ret=%d)\n", dev_id1, ret);
        return ret;
    }

    if (product_type == 0x3) { /* 0x3: pcie card */
        /* pcie card */
        ret = topo_check_ub_for_pcie_card(dev_id1, host_dev_id1, host_dev_id2, result);
        if (ret != 0) {
            return ret;
        }
    } else {
        *result = true;
    }

#else
    (void)dev_id1;
    (void)dev_id2;
    *result = false;
#endif
    return 0;
}

static soc_topology_check_t g_topology_check_info[] = {
    {SOC_TOPOLOGY_UB, topo_check_ub},
#ifdef CFG_FEATURE_CHIP_DIE
    {SOC_TOPOLOGY_SIO, topo_check_sio},
    {SOC_TOPOLOGY_HCCS_SW, topo_check_hccs_sw},
#endif
    {SOC_TOPOLOGY_HCCS, topo_check_hccs},
};

/*
  In host side invoke, dev_id1 and dev_id2 is physic id on the host.
  In device side invoke, dev_id1 is physic id on the device, dev_id2 is physic id on the host.
*/
int soc_get_dev_topology(unsigned int dev_id1, unsigned int dev_id2, int *topology_type)
{
    unsigned int check_num;
    bool result = false;
    int ret, i;

    if ((dev_id1 >= ASCEND_PDEV_MAX_NUM) || (dev_id2 >= ASCEND_HOST_PDEV_MAX_NUM) || (topology_type == NULL)) {
        soc_err("Invalid parameter. (dev_id1=%u; dev_id2=%u; topology_type_is_null=%d)\n", dev_id1, dev_id2,
                topology_type == NULL);
        return -EINVAL;
    }

    *topology_type = SOC_TOPOLOGY_PIX;
    check_num = sizeof(g_topology_check_info) / sizeof(soc_topology_check_t);
    for (i = 0; i < check_num; i++) {
        ret = g_topology_check_info[i].topology_check_handler(dev_id1, dev_id2, &result);
        if (ret != 0) {
            soc_err("Check topology failed. (ret=%d; i=%d)\n", ret, i);
            return ret;
        }

        if (result) {
            *topology_type = g_topology_check_info[i].topology_type;
            return ret;
        }
    }

#ifdef CFG_HOST_ENV
    ret = soc_resmng_get_dev_topology(dev_id1, dev_id2, topology_type);
    if (ret != 0) {
        soc_err("Get devices topology from pcie failed. (ret=%d; dev_id1=%u; dev_id2=%u)\n", ret, dev_id1, dev_id2);
        return ret;
    }
#endif
    soc_debug("Get topology type success. (dev_id1=%u; dev_id2=%u; topology_type=%d)\n", dev_id1, dev_id2,
              *topology_type);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(soc_get_dev_topology);