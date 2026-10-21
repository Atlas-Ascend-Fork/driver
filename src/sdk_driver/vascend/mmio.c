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

#include "ka_task_pub.h"
#include "ka_compiler_pub.h"
#include "ka_pci_pub.h"
#include "ka_barrier_pub.h"
#include "dvt.h"
#include "vfio_ops.h"
#include "mmio.h"
#include "priv_ops.h"

#define VF_BAR2_SPARSE_SIZE 5
#define VF_BAR2_STARS_OFFSET 0x8000
#define VF_BAR2_STARS_SIZE 0x2000000
#define VF_BAR2_TS_DOORBELL_OFFSET 0x2008000
#define VF_BAR2_TS_DOORBELL_SIZE 0x40000
#define VF_BAR2_HWTS_OFFSET 0x2408000
#define VF_BAR2_HWTS_SIZE 0x10000
#define VF_BAR2_SOC_DOORBELL_OFFSET 0x2808000
#define VF_BAR2_SOC_DOORBELL_SIZE 0x1000
#define VF_BAR2_PARA_OFFSET 0x2908000
#define VF_BAR2_PARA_SIZE 0x4000

#define DVT_MMIO_BAR0_SIZE_910B 0x40000000
#define DVT_MMIO_BAR2_SIZE_910B 0x20000000
#define DVT_MMIO_BAR4_SIZE_910B 0x1000000000
#define DVT_MMIO_BAR0_SIZE_910_93 0x40000000
#define DVT_MMIO_BAR2_SIZE_910_93 0x20000000
#define DVT_MMIO_BAR4_SIZE_910_93 0x1000000000
#define DVT_MMIO_BAR0_SIZE_950 0x40000000
#define DVT_MMIO_BAR2_SIZE_950 0x20000000
#define DVT_MMIO_BAR4_SIZE_950 0x1000000000

struct mmio_device_init_info {
    unsigned short device;
    unsigned int cfg_space_size;
    unsigned int mmio_size;
    unsigned int mmio_bar;
};

STATIC const struct mmio_device_init_info mmio_init_info[] = {
    {PCI_DEVICE_ID_ASCEND910B, KA_PCI_CFG_SPACE_EXP_SIZE, DVT_MMIO_BAR0_SIZE_910B, 0},
    {PCI_DEVICE_ID_ASCEND910_93, KA_PCI_CFG_SPACE_EXP_SIZE, DVT_MMIO_BAR0_SIZE_910_93, 0},
    {PCI_DEVICE_ID_ASCEND950, KA_PCI_CFG_SPACE_EXP_SIZE, DVT_MMIO_BAR0_SIZE_950, 0},
    {KA_PCI_ANY_ID, KA_PCI_CFG_SPACE_EXP_SIZE, DVT_MMIO_BAR0_SIZE, 0},
    {}};

STATIC void hw_vdavinci_reset_sparse_mmio(struct hw_vdavinci *vdavinci, struct vdavinci_mapinfo *mmio_map_info)
{
    int ret = 0;
    u64 i = 0;
    struct vdavinci_bar_map *map;

    for (i = 0; i < mmio_map_info->num; i++) {
        map = &mmio_map_info->map_info[i];
        if (map->map_type == MAP_TYPE_BACKEND) {
            ret = memset_s(map->vaddr, map->size, 0, map->size);
            if (ret != 0) {
                vascend_err(vdavinci_to_dev(vdavinci),
                            "vdavinci reset mmio space "
                            "failed, vid: %u, ret: %d\n",
                            vdavinci->id, ret);
            }
        }
    }
}

/* vdavinci Device Model Level Reset */
void hw_vdavinci_reset_mmio(struct hw_vdavinci *vdavinci)
{
    hw_vdavinci_reset_sparse_mmio(vdavinci, &vdavinci->mmio.bar0_sparse);
    hw_vdavinci_reset_sparse_mmio(vdavinci, &vdavinci->mmio.bar2_sparse);
    hw_vdavinci_reset_sparse_mmio(vdavinci, &vdavinci->mmio.bar4_sparse);
}

STATIC int hw_vdavinci_mmio_check_sparse(struct hw_vdavinci *vdavinci)
{
    u64 i = 0;
    struct vdavinci_bar_map *map = NULL;

    if (vdavinci->mmio.mem_sparse.num == 0) {
        return 0;
    }

    if (vdavinci->mmio.mem_sparse.num > HW_BAR_SPARSE_MAP_MAX) {
        vascend_err(vdavinci_to_dev(vdavinci), "invalid sparse num %llu\n", vdavinci->mmio.mem_sparse.num);
        return -EINVAL;
    }

    for (i = 0; i < vdavinci->mmio.mem_sparse.num; i++) {
        map = &vdavinci->mmio.mem_sparse.map_info[i];
        if (map->offset == 0 || map->offset > vdavinci->type->bar4_size || map->size == 0 ||
            map->size > vdavinci->type->bar4_size - map->offset || !KA_MM_PAGE_ALIGNED(map->offset) ||
            !KA_MM_PAGE_ALIGNED(map->size) || !KA_MM_PAGE_ALIGNED(map->paddr)) {
            vascend_err(vdavinci_to_dev(vdavinci),
                        "check sparse failed: invalid map: offset:%lx size:%lx paddr:%llx, bar4_size:%lx\n",
                        map->offset, map->size, map->paddr, vdavinci->type->bar4_size);
            return -EINVAL;
        }

        if (i < (vdavinci->mmio.mem_sparse.num - 1)) {
            if (map->offset + map->size != vdavinci->mmio.mem_sparse.map_info[i + 1].offset) {
                vascend_err(vdavinci_to_dev(vdavinci),
                            "check sparse failed: map not continuous:"
                            "offset:%lx size:%lx, next offset:%lx\n",
                            map->offset, map->size, vdavinci->mmio.mem_sparse.map_info[i + 1].offset);
                return -EINVAL;
            }
        }

        if (i == (vdavinci->mmio.mem_sparse.num - 1)) {
            if (map->offset + map->size != vdavinci->type->bar4_size) {
                vascend_err(vdavinci_to_dev(vdavinci),
                            "check sparse failed: last map "
                            "offset:%lx + size:%lx != bar4_size:%lx\n",
                            map->offset, map->size, vdavinci->type->bar4_size);
                return -EINVAL;
            }
        }
    }

    return 0;
}

STATIC int hw_dvt_vdavinci_getmapinfo(struct hw_vdavinci *vdavinci)
{
    int ret = -EINVAL;
    struct vdavinci_type tp;
    struct hw_dvt *dvt = vdavinci->dvt;

    ret = init_vdavinci_type(vdavinci->type, &tp);
    if (ret != 0) {
        return ret;
    }
    ret = vdavinci_priv_vdev_getmapinfo(vdavinci, &tp, VFIO_PCI_BAR4_REGION_INDEX, &vdavinci->mmio.mem_sparse);
    if (ret != 0) {
        vascend_err(dvt->vdavinci_priv->dev, "get map info failed, vid:%u type bar4_size:%lx ret:%d\n", vdavinci->id,
                    vdavinci->type->bar4_size, ret);
        return ret;
    }

    return 0;
}

STATIC void hw_vdavinci_sparse_mmio_uninit(struct hw_vdavinci *vdavinci, struct vdavinci_mapinfo *mmio_map_info)
{
    u64 i = 0;
    ka_pci_dev_t *pdev = NULL;
    ka_device_t *res_dev = NULL;
    struct vdavinci_bar_map *map = NULL;

    for (i = 0; i < mmio_map_info->num; i++) {
        map = &mmio_map_info->map_info[i];
        if (map->map_type == MAP_TYPE_BACKEND && map->vaddr) {
            ka_mm_vfree(map->vaddr);
            map->vaddr = NULL;
        }
    }

    mmio_map_info->num = 0;
    /* Safe without cache_lock: mmio_uninit only runs on the destroy path,
     * after the device is deactivated and its fd closed, so no concurrent
     * hw_vdavinci_get_bar_io_addr can be using this mapping. */
    if (mmio_map_info->io_addr != NULL) {
        res_dev = vdavinci_resource_dev(vdavinci);
        if (res_dev == NULL) {
            vascend_err(vdavinci_to_dev(vdavinci), "resource dev is null before iounmap. (io_addr=%p, vid=%u)\n",
                        mmio_map_info->io_addr, vdavinci->id);
            return;
        }
        pdev = ka_container_of(res_dev, ka_pci_dev_t, dev);
        ka_mm_pci_iounmap(pdev, mmio_map_info->io_addr);
        mmio_map_info->io_addr = NULL;
    }
}

#ifdef DAVINCI_TEST
int hw_vdavinci_310_mmio_init(struct hw_vdavinci *vdavinci)
{
    int ret;

    if (vdavinci->type == NULL) {
        return -EINVAL;
    }

    ret = hw_dvt_vdavinci_getmapinfo(vdavinci);
    if (ret) {
        return ret;
    }

    if (vdavinci->mmio.mem_sparse.num != 0) {
        goto put_mapinfo;
    }

    vdavinci->mmio.bar0_sparse.num = 1;
    vdavinci->mmio.bar0_sparse.map_info[0].map_type = MAP_TYPE_TRAP;
    vdavinci->mmio.bar0_sparse.map_info[0].offset = 0;
    vdavinci->mmio.bar0_sparse.map_info[0].size = vdavinci->type->bar0_size;

    vdavinci->mmio.bar2_sparse.num = 1;
    vdavinci->mmio.bar2_sparse.map_info[0].map_type = MAP_TYPE_BACKEND;
    vdavinci->mmio.bar2_sparse.map_info[0].offset = 0;
    vdavinci->mmio.bar2_sparse.map_info[0].size = vdavinci->type->bar2_size;

    vdavinci->mmio.bar4_sparse.num = 1;
    vdavinci->mmio.bar4_sparse.map_info[0].map_type = MAP_TYPE_BACKEND;
    vdavinci->mmio.bar4_sparse.map_info[0].offset = 0;
    vdavinci->mmio.bar4_sparse.map_info[0].size = vdavinci->type->bar4_size;

    vdavinci->mmio.bar0_sparse.map_info[0].vaddr = ka_mm_vzalloc(vdavinci->mmio.bar0_sparse.map_info[0].size);
    if (!vdavinci->mmio.bar0_sparse.map_info[0].vaddr) {
        goto put_mapinfo;
    }

    vdavinci->mmio.bar2_sparse.map_info[0].vaddr = vmalloc_user(vdavinci->mmio.bar2_sparse.map_info[0].size);
    if (!vdavinci->mmio.bar2_sparse.map_info[0].vaddr) {
        goto io_failed;
    }

    vdavinci->mmio.bar4_sparse.map_info[0].vaddr = vmalloc_user(vdavinci->mmio.bar4_sparse.map_info[0].size);
    if (!vdavinci->mmio.bar4_sparse.map_info[0].vaddr) {
        goto mem_failed;
    }

    vdavinci->mmio.msix_base = vdavinci->mmio.bar0_sparse.map_info[0].vaddr;
    vdavinci->mmio.io_base = vdavinci->mmio.bar2_sparse.map_info[0].vaddr;
    vdavinci->mmio.mem_base = vdavinci->mmio.bar4_sparse.map_info[0].vaddr;

    hw_vdavinci_reset_mmio(vdavinci);

    vascend_info(vdavinci_to_dev(vdavinci), "mmio init success");
    return 0;

mem_failed:
    ka_mm_vfree(vdavinci->mmio.bar2_sparse.map_info[0].vaddr);
io_failed:
    ka_mm_vfree(vdavinci->mmio.bar0_sparse.map_info[0].vaddr);
put_mapinfo:
    (void)vdavinci_priv_vdev_putmapinfo(vdavinci);
    return -ENOMEM;
}

void hw_vdavinci_310_mmio_uninit(struct hw_vdavinci *vdavinci)
{
    (void)vdavinci_priv_vdev_putmapinfo(vdavinci);

    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar0_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar2_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar4_sparse);
    vdavinci->mmio.msix_base = NULL;
    vdavinci->mmio.io_base = NULL;
    vdavinci->mmio.mem_base = NULL;

    vascend_info(vdavinci_to_dev(vdavinci), "mmio uninit success");
}
#endif

int hw_vdavinci_310pro_mmio_init(struct hw_vdavinci *vdavinci)
{
    int ret;
    const u64 bar4_sparse_num = 2;

    if (vdavinci->type == NULL) {
        return -EINVAL;
    }

    ret = hw_dvt_vdavinci_getmapinfo(vdavinci);
    if (ret != 0) {
        return ret;
    }

    if (vdavinci->mmio.mem_sparse.num != 1) {
        goto put_mapinfo;
    }

    if (hw_vdavinci_mmio_check_sparse(vdavinci) != 0) {
        goto put_mapinfo;
    }

    vdavinci->mmio.bar0_sparse.num = 1;
    vdavinci->mmio.bar0_sparse.map_info[0].map_type = MAP_TYPE_TRAP;
    vdavinci->mmio.bar0_sparse.map_info[0].offset = 0;
    vdavinci->mmio.bar0_sparse.map_info[0].size = vdavinci->type->bar0_size;

    vdavinci->mmio.bar2_sparse.num = 1;
    vdavinci->mmio.bar2_sparse.map_info[0].map_type = MAP_TYPE_BACKEND;
    vdavinci->mmio.bar2_sparse.map_info[0].offset = 0;
    vdavinci->mmio.bar2_sparse.map_info[0].size = vdavinci->type->bar2_size;

    vdavinci->mmio.bar4_sparse.num = bar4_sparse_num;
    vdavinci->mmio.bar4_sparse.map_info[0].map_type = MAP_TYPE_BACKEND;
    vdavinci->mmio.bar4_sparse.map_info[0].offset = 0;
    vdavinci->mmio.bar4_sparse.map_info[0].size = vdavinci->mmio.mem_sparse.map_info[0].offset;
    vdavinci->mmio.bar4_sparse.map_info[1].map_type = MAP_TYPE_PASSTHROUGH;
    vdavinci->mmio.bar4_sparse.map_info[1].offset = vdavinci->mmio.mem_sparse.map_info[0].offset;
    vdavinci->mmio.bar4_sparse.map_info[1].size = vdavinci->mmio.mem_sparse.map_info[0].size;
    vdavinci->mmio.bar4_sparse.map_info[1].paddr = vdavinci->mmio.mem_sparse.map_info[0].paddr;

    vdavinci->mmio.bar2_sparse.map_info[0].vaddr = vmalloc_user(vdavinci->mmio.bar2_sparse.map_info[0].size);
    if (!vdavinci->mmio.bar2_sparse.map_info[0].vaddr) {
        goto put_mapinfo;
    }

    vdavinci->mmio.bar4_sparse.map_info[0].vaddr = vmalloc_user(vdavinci->mmio.bar4_sparse.map_info[0].size);
    if (!vdavinci->mmio.bar4_sparse.map_info[0].vaddr) {
        goto mem_failed;
    }

    vdavinci->mmio.io_base = vdavinci->mmio.bar2_sparse.map_info[0].vaddr;
    vdavinci->mmio.mem_base = vdavinci->mmio.bar4_sparse.map_info[0].vaddr;

    hw_vdavinci_reset_mmio(vdavinci);

    vascend_info(vdavinci_to_dev(vdavinci), "mmio init success");
    return 0;

mem_failed:
    ka_mm_vfree(vdavinci->mmio.bar2_sparse.map_info[0].vaddr);
put_mapinfo:
    (void)vdavinci_priv_vdev_putmapinfo(vdavinci);
    return -ENOMEM;
}

void hw_vdavinci_310pro_mmio_uninit(struct hw_vdavinci *vdavinci)
{
    (void)vdavinci_priv_vdev_putmapinfo(vdavinci);

    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar0_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar2_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar4_sparse);
    vdavinci->mmio.io_base = NULL;
    vdavinci->mmio.mem_base = NULL;

    vascend_info(vdavinci_to_dev(vdavinci), "mmio uninit success");
}

int hw_vdavinci_910_mmio_init(struct hw_vdavinci *vdavinci)
{
    int ret;

    if (vdavinci->type == NULL) {
        return -EINVAL;
    }

    ret = hw_dvt_vdavinci_getmapinfo(vdavinci);
    if (ret != 0) {
        return ret;
    }

    if (vdavinci->mmio.mem_sparse.num != 0) {
        goto put_mapinfo;
    }

    vdavinci->mmio.bar0_sparse.num = 1;
    vdavinci->mmio.bar0_sparse.map_info[0].map_type = MAP_TYPE_TRAP;
    vdavinci->mmio.bar0_sparse.map_info[0].offset = 0;
    vdavinci->mmio.bar0_sparse.map_info[0].size = vdavinci->type->bar0_size;

    vdavinci->mmio.bar2_sparse.num = 1;
    vdavinci->mmio.bar2_sparse.map_info[0].map_type = MAP_TYPE_BACKEND;
    vdavinci->mmio.bar2_sparse.map_info[0].offset = 0;
    vdavinci->mmio.bar2_sparse.map_info[0].size = vdavinci->type->bar2_size;

    vdavinci->mmio.bar4_sparse.num = 1;
    vdavinci->mmio.bar4_sparse.map_info[0].map_type = MAP_TYPE_BACKEND;
    vdavinci->mmio.bar4_sparse.map_info[0].offset = 0;
    vdavinci->mmio.bar4_sparse.map_info[0].size = vdavinci->type->bar4_size;

    vdavinci->mmio.bar2_sparse.map_info[0].vaddr = vmalloc_user(vdavinci->mmio.bar2_sparse.map_info[0].size);
    if (!vdavinci->mmio.bar2_sparse.map_info[0].vaddr) {
        goto put_mapinfo;
    }

    vdavinci->mmio.bar4_sparse.map_info[0].vaddr = vmalloc_user(vdavinci->mmio.bar4_sparse.map_info[0].size);
    if (!vdavinci->mmio.bar4_sparse.map_info[0].vaddr) {
        goto mem_failed;
    }

    vdavinci->mmio.io_base = vdavinci->mmio.bar2_sparse.map_info[0].vaddr;
    vdavinci->mmio.mem_base = vdavinci->mmio.bar4_sparse.map_info[0].vaddr;

    hw_vdavinci_reset_mmio(vdavinci);

    vascend_info(vdavinci_to_dev(vdavinci), "mmio init success");
    return 0;

mem_failed:
    ka_mm_vfree(vdavinci->mmio.bar2_sparse.map_info[0].vaddr);
put_mapinfo:
    (void)vdavinci_priv_vdev_putmapinfo(vdavinci);
    return -ENOMEM;
}

void hw_vdavinci_910_mmio_uninit(struct hw_vdavinci *vdavinci)
{
    (void)vdavinci_priv_vdev_putmapinfo(vdavinci);

    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar0_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar2_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar4_sparse);
    vdavinci->mmio.io_base = NULL;
    vdavinci->mmio.mem_base = NULL;

    vascend_info(vdavinci_to_dev(vdavinci), "mmio uninit success");
}

void hw_vdavinci_910b_vf_mmio_uninit(struct hw_vdavinci *vdavinci)
{
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar0_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar2_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar4_sparse);

    vdavinci->mmio.io_base = NULL;
    vdavinci->mmio.mem_base = NULL;

    vascend_info(vdavinci_to_dev(vdavinci), "vf mmio uninit success\n");
}

void hw_vdavinci_910_93_vf_mmio_uninit(struct hw_vdavinci *vdavinci)
{
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar0_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar2_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar4_sparse);

    vdavinci->mmio.io_base = NULL;
    vdavinci->mmio.mem_base = NULL;

    vascend_info(vdavinci_to_dev(vdavinci), "vf mmio uninit success\n");
}

void hw_vdavinci_950_vf_mmio_uninit(struct hw_vdavinci *vdavinci)
{
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar0_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar2_sparse);
    hw_vdavinci_sparse_mmio_uninit(vdavinci, &vdavinci->mmio.bar4_sparse);

    vdavinci->mmio.io_base = NULL;
    vdavinci->mmio.mem_base = NULL;

    vascend_info(vdavinci_to_dev(vdavinci), "vf mmio uninit success\n");
}

STATIC int hw_vdavinci_vf_bar0_init(struct hw_vdavinci *vdavinci, phys_addr_t base, int *io_base_idx, int *mem_base_idx)
{
    int idx = 0;
    vdavinci->mmio.bar0_sparse.num = VF_BAR0_SPARSE_SIZE;
    /* doorbell 128k */
    vdavinci->mmio.bar0_sparse.map_info[idx].map_type = MAP_TYPE_TRAP;
    vdavinci->mmio.bar0_sparse.map_info[idx].offset = VF_BAR0_DOORBELL_OFFSET;
    vdavinci->mmio.bar0_sparse.map_info[idx].size = VF_BAR0_DOORBELL_SIZE;
    if (++idx >= vdavinci->mmio.bar0_sparse.num) {
        return -EINVAL;
    }

    /* vpc sq and cq: 64MB */
    vdavinci->mmio.bar0_sparse.map_info[idx].map_type = MAP_TYPE_BACKEND;
    vdavinci->mmio.bar0_sparse.map_info[idx].offset = VF_BAR0_VPC_OFFSET;
    vdavinci->mmio.bar0_sparse.map_info[idx].size = VF_BAR0_VPC_SIZE;
    vdavinci->mmio.bar0_sparse.map_info[idx].vaddr = vmalloc_user(vdavinci->mmio.bar0_sparse.map_info[idx].size);
    if (!vdavinci->mmio.bar0_sparse.map_info[idx].vaddr) {
        vascend_err(vdavinci_to_dev(vdavinci), "not enough memory");
        return -ENOMEM;
    }
    *io_base_idx = idx;
    if (++idx >= vdavinci->mmio.bar0_sparse.num) {
        ka_mm_vfree(vdavinci->mmio.bar0_sparse.map_info[*io_base_idx].vaddr);
        return -EINVAL;
    }

    /* dvpp share memory 24MB */
    vdavinci->mmio.bar0_sparse.map_info[idx].map_type = MAP_TYPE_BACKEND;
    vdavinci->mmio.bar0_sparse.map_info[idx].offset = VF_BAR0_DVPP_OFFSET;
    vdavinci->mmio.bar0_sparse.map_info[idx].size = VF_BAR0_DVPP_SIZE;
    vdavinci->mmio.bar0_sparse.map_info[idx].vaddr = vmalloc_user(vdavinci->mmio.bar0_sparse.map_info[idx].size);
    if (!vdavinci->mmio.bar0_sparse.map_info[idx].vaddr) {
        ka_mm_vfree(vdavinci->mmio.bar0_sparse.map_info[*io_base_idx].vaddr);
        vascend_err(vdavinci_to_dev(vdavinci), "not enough memory");
        return -ENOMEM;
    }
    *mem_base_idx = idx;
    if (++idx >= vdavinci->mmio.bar0_sparse.num) {
        goto out_invaild_idx;
    }

    /* pcie msg 16M */
    vdavinci->mmio.bar0_sparse.map_info[idx].map_type = MAP_TYPE_PASSTHROUGH;
    vdavinci->mmio.bar0_sparse.map_info[idx].offset = VF_BAR0_MSG_OFFSET;
    vdavinci->mmio.bar0_sparse.map_info[idx].size = VF_BAR0_MSG_SIZE;
    vdavinci->mmio.bar0_sparse.map_info[idx].paddr = base;
    if (++idx >= vdavinci->mmio.bar0_sparse.num) {
        goto out_invaild_idx;
    }

    /* topic sched 64k */
    vdavinci->mmio.bar0_sparse.map_info[idx].map_type = MAP_TYPE_PASSTHROUGH;
    vdavinci->mmio.bar0_sparse.map_info[idx].offset = VF_BAR0_TOPIC_OFFSET;
    vdavinci->mmio.bar0_sparse.map_info[idx].size = VF_BAR0_TOPIC_SIZE;
    vdavinci->mmio.bar0_sparse.map_info[idx].paddr = base + VF_BAR0_MSG_SIZE;
    if (++idx >= vdavinci->mmio.bar0_sparse.num) {
        goto out_invaild_idx;
    }

    /* msi-x 16k */
    vdavinci->mmio.bar0_sparse.map_info[idx].map_type = MAP_TYPE_TRAP;
    vdavinci->mmio.bar0_sparse.map_info[idx].offset = VF_BAR0_MSIX_OFFSET;
    vdavinci->mmio.bar0_sparse.map_info[idx].size = VF_BAR0_MSIX_SIZE;
    return 0;

out_invaild_idx:
    ka_mm_vfree(vdavinci->mmio.bar0_sparse.map_info[*io_base_idx].vaddr);
    ka_mm_vfree(vdavinci->mmio.bar0_sparse.map_info[*mem_base_idx].vaddr);
    return -EINVAL;
}

STATIC int hw_vdavinci_910_93_vf_bar0_init(struct hw_vdavinci *vdavinci, phys_addr_t base, phys_addr_t len,
                                           int *io_base_idx, int *mem_base_idx)
{
    int idx = 0;
    vdavinci->mmio.bar0_sparse.num = VF_BAR0_SPARSE_SIZE;
    /* doorbell 128k */
    vdavinci->mmio.bar0_sparse.map_info[idx].map_type = MAP_TYPE_TRAP;
    vdavinci->mmio.bar0_sparse.map_info[idx].offset = VF_BAR0_DOORBELL_OFFSET;
    vdavinci->mmio.bar0_sparse.map_info[idx].size = VF_BAR0_DOORBELL_SIZE;
    if (++idx >= vdavinci->mmio.bar0_sparse.num) {
        return -EINVAL;
    }

    /* vpc sq and cq: 64MB */
    vdavinci->mmio.bar0_sparse.map_info[idx].map_type = MAP_TYPE_BACKEND;
    vdavinci->mmio.bar0_sparse.map_info[idx].offset = VF_BAR0_VPC_OFFSET;
    vdavinci->mmio.bar0_sparse.map_info[idx].size = VF_BAR0_VPC_SIZE;
    vdavinci->mmio.bar0_sparse.map_info[idx].vaddr = vmalloc_user(vdavinci->mmio.bar0_sparse.map_info[idx].size);
    if (!vdavinci->mmio.bar0_sparse.map_info[idx].vaddr) {
        vascend_err(vdavinci_to_dev(vdavinci), "not enough memory");
        return -ENOMEM;
    }
    *io_base_idx = idx;
    if (++idx >= vdavinci->mmio.bar0_sparse.num) {
        ka_mm_vfree(vdavinci->mmio.bar0_sparse.map_info[*io_base_idx].vaddr);
        return -EINVAL;
    }

    /* dvpp share memory 24MB */
    vdavinci->mmio.bar0_sparse.map_info[idx].map_type = MAP_TYPE_BACKEND;
    vdavinci->mmio.bar0_sparse.map_info[idx].offset = VF_BAR0_DVPP_OFFSET;
    vdavinci->mmio.bar0_sparse.map_info[idx].size = VF_BAR0_DVPP_SIZE;
    vdavinci->mmio.bar0_sparse.map_info[idx].vaddr = vmalloc_user(vdavinci->mmio.bar0_sparse.map_info[idx].size);
    if (!vdavinci->mmio.bar0_sparse.map_info[idx].vaddr) {
        ka_mm_vfree(vdavinci->mmio.bar0_sparse.map_info[*io_base_idx].vaddr);
        vascend_err(vdavinci_to_dev(vdavinci), "not enough memory");
        return -ENOMEM;
    }
    *mem_base_idx = idx;
    if (++idx >= vdavinci->mmio.bar0_sparse.num) {
        goto out_invaild_idx;
    }

    /* map the remaining lengths */
    vdavinci->mmio.bar0_sparse.map_info[idx].map_type = MAP_TYPE_PASSTHROUGH;
    vdavinci->mmio.bar0_sparse.map_info[idx].offset = VF_BAR0_MSG_OFFSET;
    vdavinci->mmio.bar0_sparse.map_info[idx].size = ka_base_min(
        (unsigned long)len, (unsigned long)(DVT_MMIO_BAR0_SIZE_910_93 - VF_BAR0_MSG_OFFSET));
    vdavinci->mmio.bar0_sparse.map_info[idx].paddr = base;
    if (++idx >= vdavinci->mmio.bar0_sparse.num) {
        goto out_invaild_idx;
    }

    return 0;

out_invaild_idx:
    ka_mm_vfree(vdavinci->mmio.bar0_sparse.map_info[*io_base_idx].vaddr);
    ka_mm_vfree(vdavinci->mmio.bar0_sparse.map_info[*mem_base_idx].vaddr);
    return -EINVAL;
}

static struct bar_info {
    unsigned int map_type;
    size_t offset;
    size_t size;
} bar2_sparse_info[VF_BAR2_SPARSE_SIZE] = {
    {MAP_TYPE_PASSTHROUGH, VF_BAR2_STARS_OFFSET, VF_BAR2_STARS_SIZE},
    {MAP_TYPE_PASSTHROUGH, VF_BAR2_TS_DOORBELL_OFFSET, VF_BAR2_TS_DOORBELL_SIZE},
    {MAP_TYPE_PASSTHROUGH, VF_BAR2_HWTS_OFFSET, VF_BAR2_HWTS_SIZE},
    {MAP_TYPE_PASSTHROUGH, VF_BAR2_SOC_DOORBELL_OFFSET, VF_BAR2_SOC_DOORBELL_SIZE},
    {MAP_TYPE_PASSTHROUGH, VF_BAR2_PARA_OFFSET, VF_BAR2_PARA_SIZE},
};

STATIC void hw_vdavinci_vf_bar2_init(struct hw_vdavinci *vdavinci, phys_addr_t base)
{
    int i = 0;

    vdavinci->mmio.bar2_sparse.num = VF_BAR2_SPARSE_SIZE;
    for (i = 0; i < VF_BAR2_SPARSE_SIZE; i++) {
        vdavinci->mmio.bar2_sparse.map_info[i].map_type = bar2_sparse_info[i].map_type;
        vdavinci->mmio.bar2_sparse.map_info[i].offset = bar2_sparse_info[i].offset;
        vdavinci->mmio.bar2_sparse.map_info[i].size = bar2_sparse_info[i].size;
        vdavinci->mmio.bar2_sparse.map_info[i].paddr = base + bar2_sparse_info[i].offset;
    }
}

STATIC void hw_vdavinci_vf_bar4_init(struct hw_vdavinci *vdavinci, phys_addr_t base)
{
    int idx = 0;
    vdavinci->mmio.bar4_sparse.num = VF_BAR4_SPARSE_SIZE;
    /* HBM 4GB */
    vdavinci->mmio.bar4_sparse.map_info[idx].map_type = MAP_TYPE_PASSTHROUGH;
    vdavinci->mmio.bar4_sparse.map_info[idx].offset = VF_BAR4_HBM_OFFSET;
    vdavinci->mmio.bar4_sparse.map_info[idx].size = vdavinci->type->bar4_size;
    vdavinci->mmio.bar4_sparse.map_info[idx].paddr = base;

    vascend_info(vdavinci_to_dev(vdavinci), "VF mmio init bar4 size success: 0x%lx\n", VF_BAR4_HBM_SIZE);
}

int hw_vdavinci_910b_vf_mmio_init(struct hw_vdavinci *vdavinci)
{
    ka_pci_dev_t *pdev = ka_container_of(vdavinci_resource_dev(vdavinci), ka_pci_dev_t, dev);
    phys_addr_t bar0_base, bar2_base, bar4_base;
    int io_base_idx, mem_base_idx;
    int ret;

    bar0_base = ka_pci_resource_start(pdev, VFIO_PCI_BAR0_REGION_INDEX);
    bar2_base = ka_pci_resource_start(pdev, VFIO_PCI_BAR2_REGION_INDEX);
    bar4_base = ka_pci_resource_start(pdev, VFIO_PCI_BAR4_REGION_INDEX);

    ret = hw_vdavinci_vf_bar0_init(vdavinci, bar0_base, &io_base_idx, &mem_base_idx);
    if (ret) {
        return ret;
    }

    hw_vdavinci_vf_bar2_init(vdavinci, bar2_base);
    hw_vdavinci_vf_bar4_init(vdavinci, bar4_base);

    /* mmio.io_base ref for vpc cqsq, mmio.mem_base ref for dvpp share memory */
    vdavinci->mmio.io_base = vdavinci->mmio.bar0_sparse.map_info[io_base_idx].vaddr;
    vdavinci->mmio.mem_base = vdavinci->mmio.bar0_sparse.map_info[mem_base_idx].vaddr;

    vascend_info(vdavinci_to_dev(vdavinci), "VF mmio init success, vf_index: %d\n", vdavinci->vf_index);
    return 0;
}

int hw_vdavinci_910_93_vf_mmio_init(struct hw_vdavinci *vdavinci)
{
    ka_pci_dev_t *pdev = ka_container_of(vdavinci_resource_dev(vdavinci), ka_pci_dev_t, dev);
    ka_pci_dev_t *parent = ka_container_of(vdavinci->dvt->vdavinci_priv->dev, ka_pci_dev_t, dev);
    phys_addr_t bar0_base, bar2_base, bar4_base;
    phys_addr_t bar0_len, bar4_len;
    int io_base_idx, mem_base_idx;
    int ret;

    bar0_base = ka_pci_resource_start(pdev, VFIO_PCI_BAR0_REGION_INDEX);
    bar2_base = ka_pci_resource_start(pdev, VFIO_PCI_BAR2_REGION_INDEX);

    bar0_len = ka_pci_resource_len(pdev, VFIO_PCI_BAR0_REGION_INDEX);
    bar4_len = ka_pci_resource_len(pdev, VFIO_PCI_BAR4_REGION_INDEX);

    ret = hw_vdavinci_910_93_vf_bar0_init(vdavinci, bar0_base, bar0_len, &io_base_idx, &mem_base_idx);
    if (ret != 0) {
        return ret;
    }

    hw_vdavinci_vf_bar2_init(vdavinci, bar2_base);
    hw_vdavinci_map_quirk_split(vdavinci, bar2_base);
    if (vdavinci->type->bar4_size != bar4_len && vdavinci->type->is_pf_bar4) {
        bar4_base = ka_pci_resource_start(parent, VFIO_PCI_BAR4_REGION_INDEX);
    } else {
        bar4_base = ka_pci_resource_start(pdev, VFIO_PCI_BAR4_REGION_INDEX);
    }
    hw_vdavinci_vf_bar4_init(vdavinci, bar4_base);

    /* mmio.io_base ref for vpc cqsq, mmio.mem_base ref for dvpp share memory */
    vdavinci->mmio.io_base = vdavinci->mmio.bar0_sparse.map_info[io_base_idx].vaddr;
    vdavinci->mmio.mem_base = vdavinci->mmio.bar0_sparse.map_info[mem_base_idx].vaddr;

    vascend_info(vdavinci_to_dev(vdavinci), "VF mmio init success, vf_index: %d\n", vdavinci->vf_index);
    return 0;
}

int hw_vdavinci_950_vf_mmio_init(struct hw_vdavinci *vdavinci)
{
    ka_pci_dev_t *pdev = ka_container_of(vdavinci_resource_dev(vdavinci), ka_pci_dev_t, dev);
    phys_addr_t bar0_base, bar2_base, bar4_base;
    int io_base_idx, mem_base_idx;
    int ret;

    bar0_base = ka_pci_resource_start(pdev, VFIO_PCI_BAR0_REGION_INDEX);
    bar2_base = ka_pci_resource_start(pdev, VFIO_PCI_BAR2_REGION_INDEX);
    bar4_base = ka_pci_resource_start(pdev, VFIO_PCI_BAR4_REGION_INDEX);

    ret = hw_vdavinci_vf_bar0_init(vdavinci, bar0_base, &io_base_idx, &mem_base_idx);
    if (ret != 0) {
        return ret;
    }

    hw_vdavinci_vf_bar2_init(vdavinci, bar2_base);
    hw_vdavinci_vf_bar4_init(vdavinci, bar4_base);

    /* mmio.io_base ref for vpc cqsq, mmio.mem_base ref for dvpp share memory */
    vdavinci->mmio.io_base = vdavinci->mmio.bar0_sparse.map_info[io_base_idx].vaddr;
    vdavinci->mmio.mem_base = vdavinci->mmio.bar0_sparse.map_info[mem_base_idx].vaddr;

    vascend_info(vdavinci_to_dev(vdavinci), "VF mmio init success, vf_index: %d\n", vdavinci->vf_index);
    return 0;
}

int hw_vdavinci_910b_mmio_init(struct hw_vdavinci *vdavinci)
{
    return 0;
}

void hw_vdavinci_910b_mmio_uninit(struct hw_vdavinci *vdavinci)
{}

int hw_vdavinci_910_93_mmio_init(struct hw_vdavinci *vdavinci)
{
    return 0;
}

void hw_vdavinci_910_93_mmio_uninit(struct hw_vdavinci *vdavinci)
{}

int hw_vdavinci_950_mmio_init(struct hw_vdavinci *vdavinci)
{
    return 0;
}

void hw_vdavinci_950_mmio_uninit(struct hw_vdavinci *vdavinci)
{}

int hw_dvt_set_mmio_device_info(struct hw_dvt *dvt)
{
    int i;
    struct hw_dvt_device_info *info = &dvt->device_info;

    for (i = 0; mmio_init_info[i].device != 0; i++) {
        if (dvt->device == mmio_init_info[i].device || mmio_init_info[i].device == (unsigned short)KA_PCI_ANY_ID) {
            info->cfg_space_size = mmio_init_info[i].cfg_space_size;
            info->mmio_bar = mmio_init_info[i].mmio_bar;
            info->mmio_size = mmio_init_info[i].mmio_size;

            return 0;
        }
    }

    return -ENOTSUPP;
}