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

/* 10101 : bar0 bar2 bar4 */
#define DVT_MMIO_ASCEND910_FEATURE_VAL 0x15
#define DVT_MMIO_ASCEND910_93_XLOADER_OFFSET 0x2908430
#define DVT_MMIO_ASCEND910_93_XLOADER_SIZE 0x4
#define DVT_MMIO_ASCEND910_93_FEATURE_OFFSET 0x29085f8
#define DVT_MMIO_ASCEND910_93_FEATURE_SIZE 0x4
#define DVT_MMIO_ASCEND910_93_VIRT_MSG_OFFSET 0x2944ba0
#define DVT_MMIO_ASCEND910_93_VIRT_MSG_SIZE 0x200

#define reg_is_mmio(dvt, reg) ((reg) >= 0 && (reg) < (dvt)->device_info.mmio_size)

typedef u32 (*mmio_quirk_read_cb)(struct hw_vdavinci *vdavinci, u32 offset);

struct mmio_quirk_entry {
    unsigned short device;
    unsigned int offset;
    unsigned int size;
    char name[HW_DVT_MAX_TYPE_NAME];
    mmio_quirk_read_cb read_fn;
};

int hw_dvt_doorbell_write(struct hw_vdavinci *vdavinci, unsigned int offset, void *p_data, unsigned int bytes);
u32 hw_vdavinci_mmio_quirk_xloader_read(struct hw_vdavinci *vdavinci, u32 offset);
u32 hw_vdavinci_mmio_quirk_feature_read(struct hw_vdavinci *vdavinci, u32 offset);
u32 hw_vdavinci_mmio_quirk_virt_read(struct hw_vdavinci *vdavinci, u32 offset);

static struct hw_dvt_mmio_info mmio_info_table[MMIO_INFO_TYPE_MAX] = {
    {DOORBELL, 0, DOORBELL_MAX *DOORBELL_SIZE, 0, DOORBELL_SIZE, NULL, hw_dvt_doorbell_write},
};

static const struct mmio_quirk_entry common_quirks[] = {
    {PCI_DEVICE_ID_ASCEND910_93, DVT_MMIO_ASCEND910_93_XLOADER_OFFSET, DVT_MMIO_ASCEND910_93_XLOADER_SIZE, "xloader",
     hw_vdavinci_mmio_quirk_xloader_read},
    {PCI_DEVICE_ID_ASCEND910_93, DVT_MMIO_ASCEND910_93_FEATURE_OFFSET, DVT_MMIO_ASCEND910_93_FEATURE_SIZE, "feature",
     hw_vdavinci_mmio_quirk_feature_read},
    {PCI_DEVICE_ID_ASCEND910_93, DVT_MMIO_ASCEND910_93_VIRT_MSG_OFFSET, DVT_MMIO_ASCEND910_93_VIRT_MSG_SIZE, "virt msg",
     hw_vdavinci_mmio_quirk_virt_read},
    {}};

STATIC inline struct hw_dvt_mmio_info *find_mmio_info(struct hw_dvt *dvt, u32 offset)
{
    int i;

    for (i = 0; i < MMIO_INFO_TYPE_MAX; i++) {
        if (offset >= mmio_info_table[i].offset && offset < mmio_info_table[i].end) {
            return mmio_info_table + i;
        }
    }
    return NULL;
}

STATIC inline u64 hw_vdavinci_get_bar_gpa(struct hw_vdavinci *vdavinci, int bar)
{
    /* We are 64bit bar. */
    return (*(u64 *)(vdavinci->cfg_space.config + bar)) & KA_PCI_BASE_ADDRESS_MEM_MASK;
}

STATIC inline unsigned int hw_vdavinci_gpa_to_mmio_offset(struct hw_vdavinci *vdavinci, u64 gpa, int bar)
{
    u64 bar_gpa = hw_vdavinci_get_bar_gpa(vdavinci, bar);

    /* Reject underflow/overflow before truncating to u32: a wrapped 64-bit
     * offset could alias into the valid mmio window and bypass the range
     * check. UINT_MAX is rejected by reg_is_mmio in the callers. */
    if (gpa < bar_gpa || (gpa - bar_gpa) > UINT_MAX) {
        return UINT_MAX;
    }
    return (unsigned int)(gpa - bar_gpa);
}

STATIC int hw_vdavinci_mmio_reg_read(struct hw_vdavinci *vdavinci, u32 offset, void *pdata, unsigned int bytes)
{
    struct hw_dvt_mmio_info *info = find_mmio_info(vdavinci->dvt, offset);

    if (ka_unlikely(info == NULL || info->read == NULL)) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "untracked MMIO read, "
                    "offset: %08x, len: %d, vid: %u\n",
                    offset, bytes, vdavinci->id);
        return -ENXIO;
    }

    return info->read(vdavinci, offset, pdata, bytes);
}

u32 hw_vdavinci_mmio_quirk_xloader_read(struct hw_vdavinci *vdavinci, u32 offset)
{
    if (ka_unlikely(vdavinci == NULL)) {
        return 0;
    }

    return 0;
}

u32 hw_vdavinci_mmio_quirk_feature_read(struct hw_vdavinci *vdavinci, u32 offset)
{
    if (ka_unlikely(vdavinci == NULL)) {
        return 0;
    }
    if (hw_vdavinci_is_support_normal_nc()) {
        return (u32)DVT_MMIO_ASCEND910_FEATURE_VAL;
    }

    return 0;
}

u32 hw_vdavinci_mmio_quirk_virt_read(struct hw_vdavinci *vdavinci, u32 offset)
{
    ka_pci_dev_t *pdev = NULL;
    ka_device_t *res_dev = NULL;

    if (ka_unlikely(vdavinci == NULL)) {
        return -1;
    }
    res_dev = vdavinci_resource_dev(vdavinci);
    if (ka_unlikely(res_dev == NULL)) {
        return -1;
    }
    pdev = ka_container_of(res_dev, ka_pci_dev_t, dev);

    return ka_driver_dev_to_node(&pdev->dev);
}

STATIC void __ka_mm_iomem *hw_vdavinci_get_bar_io_addr(struct hw_vdavinci *vdavinci, int bar)
{
    ka_device_t *res_dev = NULL;
    ka_pci_dev_t *pdev = NULL;
    void __ka_mm_iomem *io_addr = NULL;

    if (ka_unlikely(vdavinci == NULL || bar < 0)) {
        return NULL;
    }
    res_dev = vdavinci_resource_dev(vdavinci);
    if (ka_unlikely(res_dev == NULL)) {
        return NULL;
    }
    pdev = ka_container_of(res_dev, ka_pci_dev_t, dev);
    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    if (bar == VFIO_PCI_BAR0_REGION_INDEX) {
        io_addr = vdavinci->mmio.bar0_sparse.io_addr;
    } else if (bar == VFIO_PCI_BAR2_REGION_INDEX) {
        io_addr = vdavinci->mmio.bar2_sparse.io_addr;
    } else if (bar == VFIO_PCI_BAR4_REGION_INDEX) {
        io_addr = vdavinci->mmio.bar4_sparse.io_addr;
    } else {
        ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
        return NULL;
    }
    if (io_addr == NULL) {
        io_addr = ka_mm_pci_iomap(pdev, bar, pci_resource_len(pdev, bar));
        if (io_addr == NULL) {
            ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
            return NULL;
        }
        if (bar == VFIO_PCI_BAR0_REGION_INDEX) {
            vdavinci->mmio.bar0_sparse.io_addr = io_addr;
        } else if (bar == VFIO_PCI_BAR2_REGION_INDEX) {
            vdavinci->mmio.bar2_sparse.io_addr = io_addr;
        } else if (bar == VFIO_PCI_BAR4_REGION_INDEX) {
            vdavinci->mmio.bar4_sparse.io_addr = io_addr;
        }
    }
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);

    return io_addr;
}

STATIC bool hw_vdavinci_bar_len_check(struct hw_vdavinci *vdavinci, int bar, void *pdata, unsigned int bytes,
                                      u32 offset)
{
    ka_device_t *res_dev = NULL;
    phys_addr_t bar_len = 0;
    ka_pci_dev_t *pdev = NULL;

    if (vdavinci == NULL || pdata == NULL) {
        return false;
    }
    res_dev = vdavinci_resource_dev(vdavinci);
    if (res_dev == NULL) {
        return false;
    }
    pdev = ka_container_of(res_dev, ka_pci_dev_t, dev);
    if (bytes > sizeof(u64) || !ka_mm_is_power_of_2(bytes)) {
        vascend_warn(vdavinci_to_dev(vdavinci), "not support MMIO quirk read bytes: %u\n", bytes);
        return false;
    }
    bar_len = ka_pci_resource_len(pdev, bar);
    if (offset >= bar_len || bytes > bar_len - offset || bytes == 0) {
        return false;
    }

    return true;
}

STATIC int hw_vdavinci_read_bar(struct hw_vdavinci *vdavinci, int bar, void *pdata, unsigned int bytes, u32 offset)
{
    void __ka_mm_iomem *io_addr = NULL;
    u64 val = 0;

    if (!hw_vdavinci_bar_len_check(vdavinci, bar, pdata, bytes, offset)) {
        return -EINVAL;
    }
    io_addr = hw_vdavinci_get_bar_io_addr(vdavinci, bar);
    if (io_addr == NULL) {
        return -EINVAL;
    }
    if (bytes == sizeof(u64)) {
        val = ka_mm_readq(io_addr + offset);
    } else if (bytes == sizeof(u32)) {
        val = ka_mm_readl(io_addr + offset);
    } else if (bytes == sizeof(u16)) {
        val = ka_mm_readw(io_addr + offset);
    } else if (bytes == sizeof(u8)) {
        val = ka_mm_readb(io_addr + offset);
    }
#ifndef DRV_UT
    ka_rmb();
#endif

    return memcpy_s(pdata, bytes, &val, bytes);
}

STATIC int hw_vdavinci_write_bar(struct hw_vdavinci *vdavinci, int bar, void *pdata, unsigned int bytes, u32 offset)
{
    void __ka_mm_iomem *io_addr = NULL;
    int ret = 0;
    u64 val = 0;

    if (!hw_vdavinci_bar_len_check(vdavinci, bar, pdata, bytes, offset)) {
        return -EINVAL;
    }
    io_addr = hw_vdavinci_get_bar_io_addr(vdavinci, bar);
    if (io_addr == NULL) {
        return -EINVAL;
    }
    ret = memcpy_s(&val, bytes, pdata, bytes);
    if (ret != 0) {
        return ret;
    }
    if (bytes == sizeof(u64)) {
        ka_mm_writeq(val, io_addr + offset);
    } else if (bytes == sizeof(u32)) {
        ka_mm_writel(val, io_addr + offset);
    } else if (bytes == sizeof(u16)) {
        ka_mm_writew((u16)val, io_addr + offset);
    } else if (bytes == sizeof(u8)) {
        ka_mm_writeb((u8)val, io_addr + offset);
    }
#ifndef DRV_UT
    ka_wmb();
#endif

    return 0;
}

STATIC bool hw_vdavinci_quirk_check(struct hw_vdavinci *vdavinci, u32 offset, void *pdata, unsigned int bytes)
{
    unsigned int bar2_size;

    if (ka_unlikely(vdavinci == NULL || pdata == NULL)) {
        return false;
    }
    if (ka_unlikely(vdavinci->dvt == NULL || vdavinci->type == NULL)) {
        return false;
    }
    if (bytes > sizeof(u64)) {
        vascend_warn(vdavinci_to_dev(vdavinci), "not support MMIO quirk read bytes: %u\n", bytes);
        return false;
    }
    bar2_size = vdavinci->type->bar2_size;
    if (offset >= bar2_size || bytes > bar2_size - offset || bytes == 0) {
        return false;
    }

    return true;
}

STATIC inline const struct mmio_quirk_entry *hw_vdavinci_find_mmio_quirk(struct hw_vdavinci *vdavinci, u32 offset,
                                                                         unsigned int bytes)
{
    int i;
    u32 q_start, q_end;

    if (ka_unlikely(vdavinci == NULL || vdavinci->dvt == NULL)) {
        return NULL;
    }
    for (i = 0; i < KA_BASE_ARRAY_SIZE(common_quirks); i++) {
        if (vdavinci->dvt->device != common_quirks[i].device) {
            continue;
        }
        q_start = common_quirks[i].offset;
        q_end = common_quirks[i].offset + common_quirks[i].size;
        if (offset < q_end && (offset + bytes) > q_start) {
            return &common_quirks[i];
        }
    }

    return NULL;
}

STATIC int hw_vdavinci_mmio_quirk_read(struct hw_vdavinci *vdavinci, u32 offset, void *pdata, unsigned int bytes)
{
    int i = 0;
    u64 val = 0;
    u32 q_end = 0;
    const struct mmio_quirk_entry *quirk = NULL;

    if (!hw_vdavinci_quirk_check(vdavinci, offset, pdata, bytes)) {
        return -EINVAL;
    }
    quirk = hw_vdavinci_find_mmio_quirk(vdavinci, offset, bytes);
    if (quirk != NULL) {
        q_end = quirk->offset + quirk->size;
        if (offset + bytes > q_end) {
            vascend_warn(vdavinci_to_dev(vdavinci), "not support MMIO quirk read bytes: %u\n", bytes);
            return -EINVAL;
        }
        val = quirk->read_fn(vdavinci, offset);
        return memcpy_s(pdata, bytes, &val, bytes);
    }
    for (i = 0; i < KA_BASE_ARRAY_SIZE(common_quirks); i++) {
        if (vdavinci->dvt->device != common_quirks[i].device) {
            continue;
        }
        if ((offset >> KA_MM_PAGE_SHIFT) == (common_quirks[i].offset >> KA_MM_PAGE_SHIFT)) {
            return hw_vdavinci_read_bar(vdavinci, VFIO_PCI_BAR2_REGION_INDEX, pdata, bytes, offset);
        }
    }

    vascend_err(vdavinci_to_dev(vdavinci),
                "untracked MMIO quirk read, "
                "offset: %08x, len: %d, vid: %u\n",
                offset, bytes, vdavinci->id);
    return -ENXIO;
}

STATIC int hw_vdavinci_mmio_quirk_write(struct hw_vdavinci *vdavinci, u32 offset, void *pdata, unsigned int bytes)
{
    int i = 0;
    const struct mmio_quirk_entry *quirk = NULL;

    if (!hw_vdavinci_quirk_check(vdavinci, offset, pdata, bytes)) {
        return -EINVAL;
    }
    quirk = hw_vdavinci_find_mmio_quirk(vdavinci, offset, bytes);
    if (quirk != NULL) {
        vascend_warn(vdavinci_to_dev(vdavinci), "dropped write to quirk hole: %s\n", quirk->name);
        return 0;
    }
    for (i = 0; i < KA_BASE_ARRAY_SIZE(common_quirks); i++) {
        if (vdavinci->dvt->device != common_quirks[i].device) {
            continue;
        }
        if ((offset >> KA_MM_PAGE_SHIFT) == (common_quirks[i].offset >> KA_MM_PAGE_SHIFT)) {
            return hw_vdavinci_write_bar(vdavinci, VFIO_PCI_BAR2_REGION_INDEX, pdata, bytes, offset);
        }
    }

    vascend_err(vdavinci_to_dev(vdavinci),
                "untracked MMIO quirk write, "
                "offset: %08x, len: %d, vid: %u\n",
                offset, bytes, vdavinci->id);
    return -ENXIO;
}

STATIC int hw_vdavinci_mmio_reg_write(struct hw_vdavinci *vdavinci, u32 offset, void *pdata, unsigned int bytes)
{
    struct hw_dvt_mmio_info *info = find_mmio_info(vdavinci->dvt, offset);

    if (ka_unlikely(info == NULL || info->write == NULL)) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "untracked MMIO write, "
                    "offset: %08x, len: %d, vid: %u\n",
                    offset, bytes, vdavinci->id);
        return -ENXIO;
    }

    return info->write(vdavinci, offset, pdata, bytes);
}

STATIC int hw_vdavinci_emulate_mmio_rw(struct hw_vdavinci *vdavinci, u32 offset, void *buf, unsigned int bytes,
                                       dvt_mmio_func fn)
{
#define BYTES_MAX 8
    if (ka_unlikely(bytes == 0 || bytes > BYTES_MAX)) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "failed to emulate MMIO "
                    "read %08x len %d, vid %u\n",
                    offset, bytes, vdavinci->id);
        return -EINVAL;
    }
    if (ka_unlikely(!reg_is_mmio(vdavinci->dvt, offset) || !reg_is_mmio(vdavinci->dvt, offset + bytes - 1))) {
        vascend_err(vdavinci_to_dev(vdavinci),
                    "failed to emulate MMIO "
                    "read %08x len %d, vid %u\n",
                    offset, bytes, vdavinci->id);
        return -EINVAL;
    }

    return fn(vdavinci, offset, buf, bytes);
}

int hw_vdavinci_emulate_mmio_read(struct hw_vdavinci *vdavinci, uint64_t pa, void *buf, unsigned int bytes)
{
    unsigned int offset = hw_vdavinci_gpa_to_mmio_offset(vdavinci, pa, KA_PCI_BASE_ADDRESS_0);

    return hw_vdavinci_emulate_mmio_rw(vdavinci, offset, buf, bytes, hw_vdavinci_mmio_reg_read);
}

int hw_vdavinci_emulate_mmio_write(struct hw_vdavinci *vdavinci, uint64_t pa, void *buf, unsigned int bytes)
{
    unsigned int offset = hw_vdavinci_gpa_to_mmio_offset(vdavinci, pa, KA_PCI_BASE_ADDRESS_0);

    return hw_vdavinci_emulate_mmio_rw(vdavinci, offset, buf, bytes, hw_vdavinci_mmio_reg_write);
}

int hw_vdavinci_emulate_mmio_quirk_read(struct hw_vdavinci *vdavinci, uint64_t pa, void *buf, unsigned int bytes)
{
    unsigned int offset = hw_vdavinci_gpa_to_mmio_offset(vdavinci, pa, KA_PCI_BASE_ADDRESS_2);

    return hw_vdavinci_emulate_mmio_rw(vdavinci, offset, buf, bytes, hw_vdavinci_mmio_quirk_read);
}

int hw_vdavinci_emulate_mmio_quirk_write(struct hw_vdavinci *vdavinci, uint64_t pa, void *buf, unsigned int bytes)
{
    unsigned int offset = hw_vdavinci_gpa_to_mmio_offset(vdavinci, pa, KA_PCI_BASE_ADDRESS_2);

    return hw_vdavinci_emulate_mmio_rw(vdavinci, offset, buf, bytes, hw_vdavinci_mmio_quirk_write);
}

int hw_dvt_doorbell_write(struct hw_vdavinci *vdavinci, unsigned int offset, void *p_data, unsigned int bytes)
{
    unsigned int index = offset / DOORBELL_SIZE;

    if (ka_unlikely(index >= DOORBELL_MAX)) {
        return -EINVAL;
    }
    vdavinci_priv_vdev_notify(vdavinci, (int)index);

    vdavinci->debugfs.notify_count++;
    return 0;
}

/**
 * hw_vdavinci_split_single_map - Exclude a single page quirk hole from a map entry.
 * @vdavinci: Device context pointer.
 * @map: Base pointer to the sparse map array.
 * @num_ptr: Pointer to the total number of valid map entries.
 * @idx: The current map index being processed.
 * @pfn_offset: The aligned page-frame-number offset of the quirk hole.
 * @base: Physical base address of the BAR region.
 */
STATIC void hw_vdavinci_split_single_map(struct hw_vdavinci *vdavinci, u64 *num_ptr, int idx, size_t pfn_offset,
                                         phys_addr_t base)
{
    struct vdavinci_bar_map *map = vdavinci->mmio.bar2_sparse.map_info;
    size_t start = map[idx].offset;
    size_t end = start + map[idx].size;
    size_t next_part = pfn_offset + PAGE_SIZE;
    size_t move_count, dest_max;
    int ret = 0;

    if (end > next_part) {
        if (ka_unlikely((*num_ptr) >= HW_BAR_SPARSE_MAP_MAX)) {
            vascend_err(vdavinci_to_dev(vdavinci), "sparse map array overflow!\n");
            return;
        }
        move_count = sizeof(struct vdavinci_bar_map) * ((*num_ptr) - idx);
        dest_max = sizeof(struct vdavinci_bar_map) * (HW_BAR_SPARSE_MAP_MAX - (idx + 1));

        ret = memmove_s(&map[idx + 1], dest_max, &map[idx], move_count);
        if (ka_unlikely(ret != 0)) {
            vascend_err(vdavinci_to_dev(vdavinci), "memmove_s failed at split backward!\n");
            return;
        }
        (*num_ptr)++;

        map[idx + 1].offset = next_part;
        map[idx + 1].size = end - next_part;
        map[idx + 1].paddr = base + next_part;
    }

    if (pfn_offset > start) {
        map[idx].size = pfn_offset - start;
    } else {
        move_count = sizeof(struct vdavinci_bar_map) * ((*num_ptr) - (idx + 1));
        dest_max = sizeof(struct vdavinci_bar_map) * (HW_BAR_SPARSE_MAP_MAX - idx);

        ret = memmove_s(&map[idx], dest_max, &map[idx + 1], move_count);
        if (ka_unlikely(ret != 0)) {
            vascend_err(vdavinci_to_dev(vdavinci), "memmove_s failed at split forward!\n");
            return;
        }
        (*num_ptr)--;
    }
}

/**
 * hw_vdavinci_map_quirk_split - In-place binary splitting for memory holes
 * @vdavinci: Pointer to the hw_vdavinci device context
 * @base: Physical base address of the VF BAR2 region
 *
 * This function scans the static `bar2_sparse_info` map populated inside
 * `map_info` and matches it against device-specific hardware quirks listed in
 * `common_quirks`. If a specified PFN (page frame number) offset marks a known
 * hardware memory hole, the overlapping map region is sliced up in-place.
 *
 * The function covers three edge cases for a targeted hole:
 * 1. Hole in the middle: Splits the interval into two distinct sub-regions,
 *    shifting trailing elements forward via memmove().
 * 2. Hole at the start: Shrinks the region by cutting off the head page.
 * 3. Exact-page match: Completely expunges the single-page region from the array.
 *
 * Context: Safe to be called during driver probe or VF BAR initialization.
 *          Assumes the destination array has enough padding to prevent OOB.
 */
void hw_vdavinci_map_quirk_split(struct hw_vdavinci *vdavinci, phys_addr_t base)
{
    int i = 0, j = 0;
    size_t pfn_offset = 0;
    struct vdavinci_bar_map *map = NULL;
    u64 *num_ptr = NULL;

    if (ka_unlikely(vdavinci == NULL || vdavinci->dvt == NULL)) {
        return;
    }
    num_ptr = &vdavinci->mmio.bar2_sparse.num;
    map = vdavinci->mmio.bar2_sparse.map_info;

    for (i = 0; i < KA_BASE_ARRAY_SIZE(common_quirks); i++) {
        if (vdavinci->dvt->device != common_quirks[i].device) {
            continue;
        }
        pfn_offset = (common_quirks[i].offset >> KA_MM_PAGE_SHIFT) << KA_MM_PAGE_SHIFT;
        for (j = 0; j < (*num_ptr); j++) {
            if (pfn_offset < map[j].offset || pfn_offset >= (map[j].offset + map[j].size)) {
                continue;
            }
            hw_vdavinci_split_single_map(vdavinci, num_ptr, j, pfn_offset, base);

            if (pfn_offset <= map[j].offset) {
                j--;
            }
            break;
        }
    }
}
