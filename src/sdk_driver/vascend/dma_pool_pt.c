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
#include "dvt.h"
#include "domain_manage.h"
#include "dma_pool.h"
#include "vfio_ops.h"

void hw_vdavinci_iommu_detach_group(struct hw_vdavinci *vdavinci)
{
    struct hw_vf_info *vf = &vdavinci->vf;
    ka_iommu_group_t *group;
    ka_device_t *dev = vdavinci_resource_dev(vdavinci);

    group = ka_mm_iommu_group_get(dev);
    if (!group) {
        return;
    }
    ka_mm_iommu_detach_group(vf->domain, group);
    ka_mm_iommu_domain_free(vf->domain);
    vf->domain = NULL;
    ka_mm_put_iova_domain(&vf->iovad);
    ka_mm_iommu_group_put(group);
}

STATIC bool vfio_iommu_has_sw_msi(ka_iommu_group_t *group, phys_addr_t *base)
{
    bool ret = false;
#if IS_VDAVINCI_KERNEL_VERSION_SUPPORT
    ka_list_head_t group_resv_regions;
    ka_iommu_resv_region_t *region, *next;

    KA_INIT_LIST_HEAD(&group_resv_regions);
    ka_mm_iommu_get_group_resv_regions(group, &group_resv_regions);
    ka_list_for_each_entry(region, &group_resv_regions, list)
    {
        /*
         * The presence of any 'real' MSI regions should take
         * precedence over the software-managed one if the
         * IOMMU driver happens to advertise both types.
         */
        if (region->type == IOMMU_RESV_MSI) {
            ret = false;
            break;
        }

        if (region->type == IOMMU_RESV_SW_MSI) {
            *base = region->start;
            ret = true;
        }
    }

    ka_list_for_each_entry_safe(region, next, &group_resv_regions, list)
    {
        ka_mm_kfree(region);
    }
#endif
    return ret;
}

STATIC void init_vf_iovad(struct hw_vdavinci *vdavinci)
{
    struct vm_dom_info *vm_dom = vdavinci->vdev.domain;
    struct hw_vf_info *vf = &vdavinci->vf;

    ka_init_iova_domain(&vf->iovad);
    ka_copy_reserved_iova(&vm_dom->iovad, &vf->iovad);
}

STATIC int hw_vdavinci_get_msi_cookie(struct hw_vdavinci *vdavinci, ka_iommu_group_t *group)
{
    int ret = 0;
    struct hw_vf_info *vf = &vdavinci->vf;
    bool resv_msi = false;
    phys_addr_t resv_msi_base = 0;
    ka_device_t *dev = vdavinci_resource_dev(vdavinci);

    if (group == NULL) {
        return -ENODEV;
    }
    resv_msi = vfio_iommu_has_sw_msi(group, &resv_msi_base);
    if (resv_msi) {
#if IS_VDAVINCI_KERNEL_VERSION_SUPPORT
        ret = ka_mm_iommu_get_msi_cookie(vf->domain, resv_msi_base);
#endif
        if (ret != 0) {
            vascend_err(dev, "Failed to allocate msi cookie.");
            return ret;
        }
    }

    return 0;
}

int hw_vdavinci_iommu_attach_group(struct hw_vdavinci *vdavinci)
{
    struct hw_vf_info *vf = &vdavinci->vf;
    ka_iommu_group_t *group;
    ka_device_t *dev = vdavinci_resource_dev(vdavinci);
    int ret;

    if (vf->domain) {
        vascend_err(dev, "domain exist.");
        return -EEXIST;
    }

    vf->domain = ka_mm_iommu_domain_alloc(&pci_bus_type);
    if (!vf->domain) {
        vascend_err(dev, "Failed to allocate iommu domain.");
        return -EIO;
    }

    group = ka_mm_iommu_group_get(dev);
    if (!group) {
        vascend_err(dev, "Failed to get iommu group.");
        ret = -ENODEV;
        goto out_domain;
    }

    ret = ka_mm_iommu_attach_group(vf->domain, group);
    if (ret) {
        vascend_err(dev, "Failed to attach group.");
        goto out_domain;
    }
    ret = hw_vdavinci_get_msi_cookie(vdavinci, group);
    if (ret != 0) {
        goto out_group;
    }
    init_vf_iovad(vdavinci);
    ka_mm_iommu_group_put(group);

    return 0;

out_group:
    ka_mm_iommu_detach_group(vf->domain, group);
out_domain:
    ka_mm_iommu_domain_free(vf->domain);
    vf->domain = NULL;
    if (group != NULL) {
        ka_mm_iommu_group_put(group);
    }
    return ret;
}

STATIC unsigned long aligned_nrpages(unsigned long addr, size_t size)
{
    unsigned long page_addr = addr;

    page_addr &= ~KA_MM_PAGE_MASK;
    return KA_MM_PAGE_ALIGN(page_addr + size) >> KA_MM_PAGE_SHIFT;
}

STATIC ka_dma_addr_t vdavinci_do_map(struct hw_vdavinci *vdavinci, phys_addr_t paddr, size_t size, int dir,
                                     u64 dma_mask)
{
    ka_dma_addr_t iova_addr = 0, start_paddr = 0;
    int ret = 0, prot = 0;
    unsigned long iova_pfn = 0, paddr_pfn = paddr >> KA_MM_PAGE_SHIFT;
    ka_iova_t *new_iova = NULL;
    struct hw_vf_info *vf = &vdavinci->vf;
    unsigned long nr_pages = aligned_nrpages(paddr, size);
    ka_device_t *dev = vdavinci_resource_dev(vdavinci);

    new_iova = ka_alloc_iova(&vf->iovad, nr_pages, dma_mask >> KA_MM_PAGE_SHIFT, true);
    if (new_iova == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "alloc iova failed");
        goto error;
    }

    iova_pfn = new_iova->pfn_lo;
    iova_addr = iova_pfn << ka_iova_shift(&vf->iovad);
    if (dir == DMA_TO_DEVICE || dir == KA_DMA_BIDIRECTIONAL) {
        prot |= KA_IOMMU_READ;
    }

    if (dir == DMA_FROM_DEVICE || dir == KA_DMA_BIDIRECTIONAL) {
        prot |= KA_IOMMU_WRITE;
    }

    ret = vdavinci_iommu_map(dev, iova_pfn << KA_MM_PAGE_SHIFT, paddr_pfn << KA_MM_PAGE_SHIFT, KA_MM_PAGE_ALIGN(size),
                             prot);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "iommu map failed %d", ret);
        goto error;
    }

    start_paddr = (phys_addr_t)iova_pfn << KA_MM_PAGE_SHIFT;
    start_paddr += paddr & ~KA_MM_PAGE_MASK;

    return start_paddr;
error:
    if (iova_pfn != 0) {
        ka_free_iova(&vf->iovad, iova_pfn);
    }

    return KA_DMA_MAPPING_ERROR;
}

STATIC ka_dma_addr_t vdavinci_do_map_single(ka_device_t *dev, phys_addr_t paddr, size_t size, int dir, u64 dma_mask)
{
    struct hw_vdavinci *vdavinci = find_vdavinci(dev);
    struct vm_dom_info *vm_dom = NULL;
    struct dev_dom_info *dev_dom = NULL;
    phys_addr_t start_paddr;

    if (!vdavinci || !vdavinci->is_passthrough) {
        vascend_err(dev, "not passthrough, domain not switch\n");
        return KA_DMA_MAPPING_ERROR;
    }

    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    vm_dom = vdavinci->vdev.domain;
    if (!vm_dom || ka_base_atomic_read(&vm_dom->status) != DOMAIN_PIN_STATUS_READY) {
        ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
        vascend_err(dev, "dma pool status invalid\n");
        return KA_DMA_MAPPING_ERROR;
    }

    ka_task_down_read(&vm_dom->sem);
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (!dev_dom || dev_dom->status != DOMAIN_MAP_STATUS_READY) {
        vascend_err(dev, "dma pool not ready, not map\n");
        goto error;
    }

    start_paddr = vdavinci_do_map(vdavinci, paddr, size, dir, dma_mask);
    if (start_paddr == KA_DMA_MAPPING_ERROR) {
        goto error;
    }

    ka_task_up_read(&vm_dom->sem);
    return start_paddr;

error:
    ka_task_up_read(&vm_dom->sem);

    vascend_err(dev, "Device request: %zx@%llx dir %d --- failed\n", size, (unsigned long long)paddr, dir);
    return KA_DMA_MAPPING_ERROR;
}

ka_dma_addr_t vdavinci_dma_map_page(ka_device_t *dev, ka_page_t *page, size_t offset, size_t size,
                                    ka_dma_data_direction_t dir)
{
    if (!ka_valid_dma_direction(dir)) {
        vascend_err(dev, "invalid dma direction %d\n", dir);
        return KA_DMA_MAPPING_ERROR;
    }

    if (offset != 0) {
        vascend_err(dev, "offset should be 0 %zx\n", offset);
        return KA_DMA_MAPPING_ERROR;
    }

    return vdavinci_do_map_single(dev, ka_mm_page_to_phys(page) + offset, size, dir, *dev->dma_mask);
}

ka_dma_addr_t vdavinci_dma_map_single(ka_device_t *dev, void *ptr, size_t size, ka_dma_data_direction_t dir)
{
    ka_page_t *page;
    size_t offset;

    if (ka_mm_is_vmalloc_addr(ptr)) {
        vascend_err(dev, "rejecting DMA map of vmalloc memory\n");
        return KA_DMA_MAPPING_ERROR;
    }

    if (!ka_valid_dma_direction(dir)) {
        vascend_err(dev, "invalid dma direction %d\n", dir);
        return KA_DMA_MAPPING_ERROR;
    }

    page = ka_mm_virt_to_page(ptr);
    offset = ka_mm_offset_in_page(ptr);
    if (offset != 0) {
        vascend_err(dev, "address should be page aligned, %zx\n", offset);
        return KA_DMA_MAPPING_ERROR;
    }

    return vdavinci_do_map_single(dev, ka_mm_page_to_phys(page) + offset, size, dir, *dev->dma_mask);
}

STATIC void vdavinci_do_unmap(ka_device_t *dev, ka_dma_addr_t dev_addr, size_t size)
{
    struct hw_vdavinci *vdavinci = find_vdavinci(dev);
    struct hw_vf_info *vf = NULL;
    unsigned long iova_pfn = dev_addr >> KA_MM_PAGE_SHIFT;

    if (!vdavinci || !vdavinci->is_passthrough) {
        vascend_err(dev, "unmap not passthrough\n");
        return;
    }

    vf = &vdavinci->vf;
    vdavinci_iommu_unmap(dev, iova_pfn << KA_MM_PAGE_SHIFT, KA_MM_PAGE_ALIGN(size));
    ka_free_iova(&vf->iovad, iova_pfn);
}

void vdavinci_dma_unmap_single(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir)
{
    vdavinci_do_unmap(dev, addr, size);
}

void vdavinci_dma_unmap_page(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir)
{
    vdavinci_do_unmap(dev, addr, size);
}

void *vdavinci_dma_alloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_handle, ka_gfp_t flags)
{
    ka_page_t *page = NULL;
    size_t align_size = KA_MM_PAGE_ALIGN(size);
    int order = ka_mm_get_order(align_size);

    page = ka_mm_alloc_pages(flags, order);
    if (!page) {
        vascend_debug("dev alloc page failed\n");
        return NULL;
    }

    *dma_handle = vdavinci_do_map_single(dev, ka_mm_page_to_phys(page), align_size, KA_DMA_BIDIRECTIONAL,
                                         dev->coherent_dma_mask);
    if (*dma_handle != KA_DMA_MAPPING_ERROR) {
        return ka_mm_page_address(page);
    }

    __ka_mm_free_pages(page, order);
    return NULL;
}

void vdavinci_dma_free_coherent(ka_device_t *dev, size_t size, void *vaddr, ka_dma_addr_t dma_handle)
{
    size_t align_size = KA_MM_PAGE_ALIGN(size);
    int order = ka_mm_get_order(align_size);
    ka_page_t *page = ka_mm_virt_to_page(vaddr);

    vdavinci_do_unmap(dev, dma_handle, align_size);
    __ka_mm_free_pages(page, order);
}
