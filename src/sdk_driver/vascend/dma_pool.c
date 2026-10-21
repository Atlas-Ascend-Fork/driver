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
#include "ka_kvm_pub.h"
#include "ka_list_pub.h"
#include "ka_errno_pub.h"
#include "ka_system_pub.h"
#include "dvt.h"
#include "log.h"
#include "vfio_ops.h"
#include "kvmdt.h"
#include "schedule.h"
#include "dma_pool.h"

/**
 * try to get rwsem down lock or read lock,
 * the time will last forever if wait_mins is 0
 */
STATIC int hw_vdavinci_rwsem_trylock(struct hw_vdavinci *vdavinci, struct rw_semaphore *sem, unsigned long wait_mins,
                                     bool is_write)
{
#define VDAVINCI_LOCK_INTERVAL 500
#define VDAVINCI_LOCK_WARN_INTERVAL (1 * 60 * 1000)
    unsigned long wait_time = 0;

    while (true) {
        if (is_write && ka_task_down_write_trylock(sem) != 0) {
            return 0;
        }
        if (!is_write && ka_task_down_read_trylock(sem) != 0) {
            return 0;
        }
        if (wait_mins != 0 && wait_time >= wait_mins * VDAVINCI_LOCK_WARN_INTERVAL) {
            return -EAGAIN;
        }
        /* Without this, wait_mins == 0 spins forever and the caller cannot be
         * interrupted or killed (stuck D state). */
        if (ka_task_signal_pending_state(KA_TASK_INTERRUPTIBLE, ka_task_get_current())) {
            return -EINTR;
        }
        ka_task_set_current_state(KA_TASK_INTERRUPTIBLE);
        ka_task_schedule_timeout((long)ka_system_msecs_to_jiffies(VDAVINCI_LOCK_INTERVAL));
        wait_time += VDAVINCI_LOCK_INTERVAL;
        if (wait_time % VDAVINCI_LOCK_WARN_INTERVAL == 0) {
            vascend_warn(vdavinci_get_device(vdavinci), "the time of getting down %s lock is more than %lu minutes\n",
                         is_write ? "write" : "read", wait_time / VDAVINCI_LOCK_WARN_INTERVAL);
        }
    }
}

STATIC void hw_vdavinci_rwsem_unlock(struct hw_vdavinci *vdavinci, struct rw_semaphore *sem, bool is_write)
{
    if (is_write) {
        ka_task_up_write(sem);
    } else {
        ka_task_up_read(sem);
    }
}

STATIC void dev_pin_scheduled(struct hw_vdavinci *vdavinci, unsigned long *count, ka_page_t *page)
{
    (*count)++;
    if (hw_vdavinci_scheduled(vdavinci, (*count) * KA_VFIO_PIN_PAGES_MAX_ENTRIES, VDAVINCI_PIN_PAGES_OF_SCHEDULE,
                              VDAVINCI_PIN_TIME_OF_SCHEDULE, page)) {
        (*count) = 0;
    }
}

STATIC void dev_unpin_scheduled(struct hw_vdavinci *vdavinci, unsigned long *count, ka_page_t *page)
{
    if (vdavinci_pfn_is_vpmem(ka_mm_page_to_pfn(page))) {
        return;
    }
    (*count)++;
    if (hw_vdavinci_scheduled(vdavinci, (*count) * KA_VFIO_PIN_PAGES_MAX_ENTRIES, VDAVINCI_PIN_PAGES_OF_SCHEDULE,
                              VDAVINCI_PIN_TIME_OF_SCHEDULE, page)) {
        (*count) = 0;
    }
}

STATIC bool hw_vdavinci_pfn_valid(ka_page_t *page)
{
    if (ka_mm_pfn_valid(ka_mm_page_to_pfn(page))) {
        return true;
    }
    if (vdavinci_pfn_is_vpmem(ka_mm_page_to_pfn(page))) {
        return true;
    }

    return false;
}

STATIC int hw_vdavinci_vfio_pin_pages(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info)
{
    int ret = 0, i = 0;

    if (pin_info->npage <= 0 || pin_info->npage > (int)KA_VFIO_PIN_PAGES_MAX_ENTRIES) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci error npage or gfn: %d, %lx\n", pin_info->npage,
                    pin_info->gfn);
        return -EINVAL;
    }
    if (pin_info->pages == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci's pages is null");
        return -EINVAL;
    }
    ret = vdavinci_pin_pages(vdavinci, pin_info);
    if (ret < 0) {
        vascend_warn(vdavinci_to_dev(vdavinci), "pin pages invalid, ret: %d\n", ret);
        return ret;
    }
    for (i = 0; i < pin_info->npage; i++) {
        if (!hw_vdavinci_pfn_valid(pin_info->pages[i])) {
            vascend_warn(vdavinci_to_dev(vdavinci), "vid: %u pfn 0x%lx is not mem backed\n", vdavinci->id,
                         ka_mm_page_to_pfn(pin_info->pages[i]));
            ret = -EFAULT;
            goto unpin;
        }
    }

    return 0;

unpin:
    vdavinci_unpin_pages(vdavinci, pin_info);
    return ret;
}

STATIC void hw_vdavinci_vfio_unpin_pages(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info)
{
    if (pin_info->npage <= 0 || pin_info->npage > (int)KA_VFIO_PIN_PAGES_MAX_ENTRIES) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci error npage: %d\n", pin_info->npage);
        return;
    }
    vdavinci_unpin_pages(vdavinci, pin_info);
}

STATIC int add_dma_page_list(struct page_info_list *dma_page_list, unsigned long gfn, unsigned int size,
                             ka_page_t *page)
{
    struct page_info_entry *dma_page_info = NULL;

    dma_page_info = ka_list_empty(&(dma_page_list->head)) == true ?
                        NULL :
                        ka_list_last_entry(&(dma_page_list->head), struct page_info_entry, list);
    if (dma_page_info) {
        if (ka_mm_page_to_pfn(dma_page_info->page) == ka_mm_page_to_pfn(page)) {
            dma_page_info->length = size;
            return 0;
        }
    }

    dma_page_info = ka_mm_kzalloc(sizeof(struct page_info_entry), KA_GFP_KERNEL);
    if (dma_page_info == NULL) {
        return -ENOMEM;
    }

    dma_page_info->gfn = gfn;
    dma_page_info->length = size;
    dma_page_info->page = page;
    ka_list_add_tail(&(dma_page_info->list), &(dma_page_list->head));
    dma_page_list->elem_num++;

    return 0;
}

/**
 * When dealing with pfn, it is necessary to judge whether it is
 * continuous with the last area in dma_page_list. When a discontinuous
 * pfn or the last pfn is found, this area will be added to the
 * dma_page_list. Return success only when all pfns has added to dma_page_list.
 * NOTICE: the dma_page_list must be and should be empty at first.
 */
STATIC int hw_vdavinci_add_pfn_to_dma_list(struct hw_vdavinci *vdavinci, struct page_info_list *dma_page_list,
                                           ka_pin_info *pin_info)
{
    int i, ret, last_end;
    unsigned int length = 0;
    unsigned long gfn = 0;
    ka_page_t **pages = pin_info->pages;
    struct page_info_entry *dma_page_info, *tmp;
    struct vm_dom_info *vm_dom = NULL;

    if (dma_page_list->elem_num != 0 || vdavinci == NULL) {
        return -EINVAL;
    }
    vm_dom = vdavinci->vdev.domain;
    last_end = -1;
    for (i = 0; i < pin_info->npage; i++) {
        /* if launch the last pfn or find the pfn and the next pfn are
         * discontinuous, add this region into list.
         */
        if (i + 1 == pin_info->npage || ka_mm_page_to_pfn(pages[i]) + 1 != ka_mm_page_to_pfn(pages[i + 1])) {
            length = (unsigned int)(i - last_end) * KA_MM_PAGE_SIZE;
            gfn = pin_info->gfn + (last_end + 1);
            ret = vm_add_and_merge_pfn(vm_dom, ka_mm_page_to_pfn(pages[last_end + 1]), length);
            if (ret != 0) {
                goto clean_dma_page_list;
            }
            ret = add_dma_page_list(dma_page_list, gfn, length, pages[last_end + 1]);
            if (ret != 0) {
                goto clean_dma_page_list;
            }

            last_end = i;
        }
    }
    return 0;

clean_dma_page_list:
    ka_list_for_each_entry_safe(dma_page_info, tmp, &dma_page_list->head, list)
    {
        ka_list_del(&dma_page_info->list);
        ka_mm_kfree(dma_page_info);
    }
    dma_page_list->elem_num = 0;
    return ret;
}

STATIC int hw_vdavinci_init_ram_dma_array(struct ram_range_info *ram_info)
{
    unsigned long total_steps = 0, npages = ram_info->npages;

    total_steps = KA_BASE_DIV_ROUND_UP(npages, KA_VFIO_PIN_PAGES_MAX_ENTRIES);
    ram_info->dma_array = ka_mm_kzalloc(sizeof(struct dma_info_2m *) * total_steps, KA_GFP_KERNEL);
    if (ram_info->dma_array == NULL) {
        return -ENOMEM;
    }
    ram_info->dma_array_len = 0;

    return 0;
}

STATIC void hw_vdavinci_clean_ram_dma_array(struct ram_range_info *ram_info)
{
    if (ram_info->dma_array != NULL) {
        ka_mm_kfree(ram_info->dma_array);
    }
    ram_info->dma_array_len = 0;
    ram_info->dma_array = NULL;
}

STATIC int hw_vdavinci_init_pin_info(ka_pin_info *pin_info, unsigned long gfn, int npage, bool is_alloc)
{
    if (npage <= 0 || npage > (int)KA_VFIO_PIN_PAGES_MAX_ENTRIES) {
        return -EINVAL;
    }
    pin_info->gfn = gfn;
    pin_info->npage = npage;
    if (!is_alloc) {
        return 0;
    }
    if (pin_info->pages != NULL) {
        return 0;
    }
    pin_info->pages = ka_mm_kzalloc(sizeof(ka_page_t *) * KA_VFIO_PIN_PAGES_MAX_ENTRIES, KA_GFP_KERNEL);
    if (KA_IS_ERR_OR_NULL(pin_info->pages)) {
        return -ENOMEM;
    }

    return 0;
}

STATIC void hw_vdavinci_clean_pin_info(ka_pin_info *pin_info)
{
    if (pin_info->pages != NULL) {
        ka_mm_kfree(pin_info->pages);
        pin_info->pages = NULL;
    }
    pin_info->gfn = 0;
    pin_info->npage = 0;
}

/**
 * In order to DMA, we need to get the physical page according to gfn.
 * dma_page_list saves a series of continous physical pages. Doing pin pages
 * during DMA will reduce performance. The solution of using memory pool can
 * solve this problem. User will pin the physical memory corresponding to
 * a large section of gfn before DMA
 *
 * Improve performance by reducing the number of calls to vfio_pin_pages.
 * And the step size of the processing page is the same as vfio_pin_pages
 * and both are KA_VFIO_PIN_PAGES_MAX_ENTRIES
 *
 * The length entered by the user is not necessarily aligned with the page
 * and needs to be modified
 */
STATIC int hw_vdavinci_pin_page_2m(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info,
                                   struct page_info_list *dma_page_list)
{
    int ret = 0;

    ret = hw_vdavinci_vfio_pin_pages(vdavinci, pin_info);
    if (ret != 0) {
        return ret;
    }
    ret = hw_vdavinci_add_pfn_to_dma_list(vdavinci, dma_page_list, pin_info);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci add pfns to dma list failed");
        goto unpin;
    }

    return 0;

unpin:
    hw_vdavinci_vfio_unpin_pages(vdavinci, pin_info);
    return ret;
}

STATIC void hw_vdavinci_unpin_page_2m(struct hw_vdavinci *vdavinci, ka_pin_info *pin_info,
                                      struct page_info_list *dma_page_list)
{
    int ret = 0;
    struct page_info_entry *dma_page_info = NULL;
    ka_list_head_t *pos = NULL, *next = NULL;
    unsigned long pfn = 0;
    unsigned int i = 0, npage = 0;

    ka_list_for_each_safe(pos, next, &(dma_page_list->head))
    {
        dma_page_info = ka_list_entry(pos, struct page_info_entry, list);
        npage = KA_BASE_DIV_ROUND_UP(dma_page_info->length, KA_MM_PAGE_SIZE);
        pfn = ka_mm_page_to_pfn(dma_page_info->page);

        ret = hw_vdavinci_init_pin_info(pin_info, dma_page_info->gfn, (int)npage, true);
        if (ret != 0) {
            vascend_err(vdavinci_to_dev(vdavinci), "init pin info failed\n");
            ka_list_del(pos);
            ka_mm_kfree(dma_page_info);
            continue;
        }
        for (i = 0; i < npage; i++) {
            pin_info->pages[i] = ka_mm_pfn_to_page(pfn + i);
        }
        hw_vdavinci_vfio_unpin_pages(vdavinci, pin_info);

        ka_list_del(pos);
        ka_mm_kfree(dma_page_info);
    }
    hw_vdavinci_clean_pin_info(pin_info);
}

STATIC void hw_vdavinci_unpin_page_range(struct hw_vdavinci *vdavinci, struct ram_range_info *ram_info)
{
    int j;
    unsigned long count = 0;
    unsigned int dma_start_cpu;
    struct dma_info_2m *dma_array_node = NULL;
    struct page_info_entry *dma_page_info = NULL;
    ka_pin_info pin_info = {0};

    dma_start_cpu = ka_system_smp_processor_id();
    for (j = 0; j < ram_info->dma_array_len; j++) {
        dma_array_node = ram_info->dma_array[j];
        dma_page_info = ka_list_first_entry(&(dma_array_node->dma_page_list.head), struct page_info_entry, list);
        dev_unpin_scheduled(vdavinci, &count, dma_page_info->page);
        hw_vdavinci_unpin_page_2m(vdavinci, &pin_info, &(dma_array_node->dma_page_list));
        ka_mm_kfree(dma_array_node);
    }

    (void)hw_vdavinci_changed_cpu(ka_task_get_current(), ka_cpumask_of(dma_start_cpu));
    hw_vdavinci_clean_ram_dma_array(ram_info);
}

STATIC int hw_vdavinci_pin_page_range(struct hw_vdavinci *vdavinci, struct ram_range_info *ram_info)
{
    int ret = 0;
    unsigned int dma_start_cpu;
    gfn_t base_gfn = ram_info->base_gfn;
    unsigned long npages_step = 0, count = 0, npages = ram_info->npages;
    struct dma_info_2m *new = NULL, **dma_array_temp = NULL;
    ka_pin_info pin_info = {0};

    ret = hw_vdavinci_init_ram_dma_array(ram_info);
    if (ret != 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "init ram dma array error: %d", ret);
        return ret;
    }
    dma_array_temp = ram_info->dma_array;
    dma_start_cpu = ka_system_smp_processor_id();

    while (npages) {
        npages_step = npages > KA_VFIO_PIN_PAGES_MAX_ENTRIES ? KA_VFIO_PIN_PAGES_MAX_ENTRIES : npages;
        ret = hw_vdavinci_init_pin_info(&pin_info, base_gfn, (int)npages_step, true);
        if (ret != 0) {
            goto out;
        }

        new = (struct dma_info_2m *)ka_mm_kzalloc(sizeof(struct dma_info_2m), KA_GFP_KERNEL);
        if (!new) {
            ret = -ENOMEM;
            goto out;
        }

        KA_INIT_LIST_HEAD(&(new->dma_page_list.head));
        ret = hw_vdavinci_pin_page_2m(vdavinci, &pin_info, &(new->dma_page_list));
        if (ret != 0) {
            ka_mm_kfree(new);
            goto out;
        }

        new->gfn = base_gfn;
        new->size = npages_step *KA_MM_PAGE_SIZE;
        *dma_array_temp = new;

        npages -= npages_step;
        base_gfn += npages_step;
        dma_array_temp++;
        ram_info->dma_array_len++;
        dev_pin_scheduled(vdavinci, &count, pin_info.pages[0]);
    }

    (void)hw_vdavinci_changed_cpu(ka_task_get_current(), ka_cpumask_of(dma_start_cpu));
    hw_vdavinci_clean_pin_info(&pin_info);
    return 0;
out:
    hw_vdavinci_unpin_page_range(vdavinci, ram_info);
    hw_vdavinci_clean_pin_info(&pin_info);
    hw_vdavinci_clean_ram_dma_array(ram_info);
    return ret;
}

STATIC void dma_dom_pool_unpin(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    ka_list_head_t *pos = NULL, *next = NULL;
    struct ram_range_info *ram_info = NULL;

    if (!vm_dom || !vm_dom->ram_info_list) {
        return;
    }

    ka_list_for_each_safe(pos, next, &(vm_dom->ram_info_list->head))
    {
        ram_info = ka_list_entry(pos, struct ram_range_info, list);
        hw_vdavinci_unpin_page_range(vdavinci, ram_info);
        ka_list_del(pos);
        ka_mm_kfree(ram_info);
    }

    ka_base_atomic_set(&vm_dom->status, DOMAIN_PIN_STATUS_INVALID);
}

STATIC void dma_dom_pool_ensure_unmapped(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom);

void hw_vdavinci_dma_pool_uninit(struct hw_vdavinci *vdavinci)
{
    struct vm_dom_info *vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    ka_device_t *dev = vdavinci_get_device(vdavinci);

    if (!vm_dom) {
        vascend_err(dev, "dma pool uninit failed\n");
        return;
    }
    vascend_info(dev, "dma pool uninit start\n");
    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    dma_dom_pool_ensure_unmapped(vdavinci, vm_dom);
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
    vascend_info(dev, "dma pool uninit success\n");
}

STATIC int raminfo_init(struct ram_range_info **ram, ka_kvm_memory_slot_t *memslot)
{
    struct ram_range_info *ram_info;

    if (memslot->npages >= KA_U64_MAX - memslot->base_gfn) {
        return -EINVAL;
    }

    ram_info = ka_mm_kzalloc(sizeof(struct ram_range_info), KA_GFP_KERNEL);
    if (!ram_info) {
        return -ENOMEM;
    }

    ram_info->base_gfn = memslot->base_gfn;
    ram_info->userspace_addr = memslot->userspace_addr;
    ram_info->npages = memslot->npages;
    ka_base_atomic_set(&ram_info->map_count, 0);

    *ram = ram_info;

    return 0;
}

#define ASCEND_RESERVE_IOVA_LENGTH 0x10000000 /* 256M */
STATIC int get_reserve_iova(ka_device_t *dev, ka_dma_addr_t *iova_addr, size_t *size)
{
    if (dev->coherent_dma_mask < ASCEND_RESERVE_IOVA_LENGTH) {
        return -EINVAL;
    }

    *iova_addr = dev->coherent_dma_mask - (ASCEND_RESERVE_IOVA_LENGTH - 1);
    *size = ASCEND_RESERVE_IOVA_LENGTH;

    return 0;
}

int get_reserve_iova_for_check(ka_device_t *dev, ka_dma_addr_t *iova_addr, size_t *size)
{
    struct hw_vdavinci *vdavinci = find_vdavinci(dev);

    if (vdavinci == NULL || !vdavinci->is_passthrough) {
        return -EINVAL;
    }

    return get_reserve_iova(dev, iova_addr, size);
}

STATIC int vm_reserve_iova(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    int ret;
    ka_dma_addr_t iova_addr;
    size_t size;
    struct ram_range_info *ram_info;
    ka_list_head_t *pos = NULL, *next = NULL;
    ka_iova_t *iova_re;
    ka_device_t *dev = vdavinci_get_device(vdavinci);

    if (vdavinci->is_passthrough) {
        ret = get_reserve_iova(dev, &iova_addr, &size);
        if (ret != 0) {
            return ret;
        }

        ka_list_for_each_safe(pos, next, &(vm_dom->ram_info_list->head))
        {
            ram_info = ka_list_entry(pos, struct ram_range_info, list);
            if (ram_info->base_gfn > (iova_addr >> KA_MM_PAGE_SHIFT) ||
                ram_info->base_gfn + ram_info->npages > (iova_addr >> KA_MM_PAGE_SHIFT)) {
                vascend_err(dev, "reserve iova failed, ram base : 0x%llx, len : %ld\n", ram_info->base_gfn,
                            ram_info->npages);
                return -EINVAL;
            }
        }

        iova_re = ka_reserve_iova(&vm_dom->iovad, 0, (iova_addr >> KA_MM_PAGE_SHIFT) - 1);
        if (iova_re == NULL) {
            vascend_debug("dev iova reserve failed\n");
            return -EINVAL;
        }
    }
    return 0;
}

STATIC void raminfo_list_cleanup(ka_list_head_t *slot_ram_list)
{
    struct ram_range_info *ram_info = NULL;
    ka_list_head_t *pos = NULL, *next = NULL;

    ka_list_for_each_safe(pos, next, slot_ram_list)
    {
        ram_info = ka_list_entry(pos, struct ram_range_info, list);
        ka_list_del(pos);
        ka_mm_kfree(ram_info);
    }
}

STATIC void raminfo_list_destroy(struct hw_vdavinci *vdavinci, ka_list_head_t *slot_ram_list)
{
    raminfo_list_cleanup(slot_ram_list);
}

STATIC int raminfo_list_init(struct hw_vdavinci *vdavinci, ka_list_head_t *slot_ram_list)
{
    int ret = -1;
    struct ram_range_info *ram_info = NULL;
    ka_kvm_memslots_t *slots = NULL;
    ka_kvm_memory_slot_t *slot = NULL;
    int ka_maybe_unused iter = 0;

    ka_task_mutex_lock(&(vdavinci->vdev.kvm->slots_lock));
    slots = ka_kvm_memslots(vdavinci->vdev.kvm);

    davinci_for_each_memslot(slot, slots, iter)
    {
        if (slot->flags & KA_KVM_MEM_READONLY) {
            continue;
        }
        ret = raminfo_init(&ram_info, slot);
        if (ret != 0) {
            vascend_err(vdavinci_to_dev(vdavinci), "vdavinci ram init failed, ret: %d\n", ret);
            ka_task_mutex_unlock(&(vdavinci->vdev.kvm->slots_lock));
            goto out;
        }

        vascend_debug("pin add ram gfn 0x%llx\n", ram_info->base_gfn);
        ka_list_add_tail(&(ram_info->list), slot_ram_list);
    }
    ka_task_mutex_unlock(&(vdavinci->vdev.kvm->slots_lock));

    return 0;
out:
    raminfo_list_cleanup(slot_ram_list);

    return ret;
}

STATIC bool dom_pfn_is_ram(unsigned long pfn)
{
    if (ka_mm_page_is_ram(pfn)) {
        return true;
    }
    if (vdavinci_pfn_is_vpmem(pfn)) {
        return true;
    }

    return false;
}

STATIC int dma_dom_pool_pin(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    int ret_t = 0, ret = 0;
    struct ram_range_info *ram_info = NULL;
    ka_list_head_t slot_ram_list;
    unsigned long pfn = 0;
    ka_list_head_t *pos = NULL, *next = NULL;

    KA_INIT_LIST_HEAD(&(slot_ram_list));

    ret = raminfo_list_init(vdavinci, &slot_ram_list);
    if (ret != 0) {
        return ret;
    }

    ka_list_for_each_safe(pos, next, &(slot_ram_list))
    {
        ram_info = ka_list_entry(pos, struct ram_range_info, list);
        pfn = g_hw_kvmdt_ops.gfn_to_mfn(vdavinci, ram_info->base_gfn);
        if (!dom_pfn_is_ram(pfn)) {
            vascend_warn(vdavinci_to_dev(vdavinci), "page is not ram, npage: %lu", ram_info->npages);
            continue;
        }

        ret_t = hw_vdavinci_pin_page_range(vdavinci, ram_info);
        if (ret_t) {
            vascend_warn(vdavinci_to_dev(vdavinci), "page may not be ram : %lu, ret : %d\n", ram_info->npages, ret_t);
            continue;
        }

        ka_list_del(pos);
        ka_list_add_tail(&(ram_info->list), &(vm_dom->ram_info_list->head));
    }

    raminfo_list_destroy(vdavinci, &slot_ram_list);

    if (ka_list_empty(&(vm_dom->ram_info_list->head))) {
        return -EFAULT;
    }

    ka_base_atomic_set(&vm_dom->status, DOMAIN_PIN_STATUS_READY);
    return 0;
}

STATIC void hw_vdavinci_set_dev_dom_ops(struct dev_dom_info *dom, bool is_passthrough)
{
    dom->is_passthrough = is_passthrough;
    vascend_debug("set dom is vf passthrough %d\n", is_passthrough);

    if (is_passthrough) {
        dom->ops.dev_dma_map_ram_range = vf_map_ram_range;
        dom->ops.dev_dma_unmap_ram_range = vf_unmap_ram_range;
        dom->ops.hw_vdavinci_get_iova_sg = vf_get_iova_sg;
        dom->ops.hw_vdavinci_get_iova_array = vf_get_iova_array;
    } else {
        dom->ops.dev_dma_map_ram_range = dev_dma_map_ram_range;
        dom->ops.dev_dma_unmap_ram_range = dev_dma_unmap_ram_range;
        dom->ops.hw_vdavinci_get_iova_sg = hw_vdavinci_get_iova_sg;
        dom->ops.hw_vdavinci_get_iova_array = hw_vdavinci_get_iova_array;
    }
}

STATIC void dma_dom_pool_ensure_unpinned(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    if (vdavinci == NULL || vm_dom == NULL) {
        return;
    }
    vascend_info(vdavinci_to_dev(vdavinci), "dma pool unpin pages start\n");
    ka_task_down_write(&vm_dom->sem);
    dma_dom_pool_unpin(vdavinci, vm_dom);
    ka_task_up_write(&vm_dom->sem);
    vascend_info(vdavinci_to_dev(vdavinci), "dma pool unpin pages end\n");
}

STATIC int dma_dom_pool_ensure_pinned(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, bool *pinned_by_self)
{
    ka_device_t *dev = vdavinci_get_device(vdavinci);
    int ret = 0;

    ret = hw_vdavinci_rwsem_trylock(vdavinci, &vm_dom->sem, 0, true);
    if (ret) {
        vascend_err(dev, "get down write lock failed, ret: %d", ret);
        return ret;
    }

    *pinned_by_self = (ka_base_atomic_read(&vm_dom->status) == DOMAIN_PIN_STATUS_INVALID);
    if (*pinned_by_self) {
        ret = dma_dom_pool_pin(vdavinci, vm_dom);
        if (ret) {
            vascend_err(dev, "vm pin page failed, ret : %d", ret);
        }
    }
    hw_vdavinci_rwsem_unlock(vdavinci, &vm_dom->sem, true);

    return ret;
}

void hw_vdavinci_unpin_pages(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    if (vm_dom && ka_base_atomic_read(&vm_dom->status) != DOMAIN_PIN_STATUS_INVALID) {
        dma_dom_pool_ensure_unpinned(vdavinci, vm_dom);
    }
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
}

/*
 * Get (or create) this vdavinci's dev_dom and reserve the passthrough
 * iova window. Runs entirely under the write lock so the reserve and the
 * ram list snapshot it validates stay consistent. Returns the dev_dom or
 * an ERR_PTR.
 */
STATIC struct dev_dom_info *dma_dom_pool_get_dev_dom(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    struct dev_dom_info *dev_dom = NULL;
    ka_device_t *dev = vdavinci_get_device(vdavinci);
    int ret = 0;

    ret = hw_vdavinci_rwsem_trylock(vdavinci, &vm_dom->sem, 0, true);
    if (ret) {
        vascend_err(dev, "get down write lock failed, ret: %d", ret);
        return KA_ERR_PTR(ret);
    }

    dev_dom = dev_dom_info_get(vm_dom, vdavinci);
    if (dev_dom == NULL) {
        vascend_err(dev, "dev get dma domain failed, ret : %d", -ENOMEM);
        ret = -ENOMEM;
        goto unlock;
    }
    hw_vdavinci_set_dev_dom_ops(dev_dom, vdavinci->is_passthrough);

    ret = vm_reserve_iova(vdavinci, vm_dom);
    if (ret) {
        vascend_err(dev, "reserve iova failed, ret : %d", ret);
        dev_dom_info_put(dev_dom, vdavinci);
    }

unlock:
    hw_vdavinci_rwsem_unlock(vdavinci, &vm_dom->sem, true);
    return ret != 0 ? KA_ERR_PTR(ret) : dev_dom;
}

/*
 * Build the dma mappings of a freshly prepared dev_dom on top of the shared
 * pool. Runs without vm_dom->sem held: attach and map are slow and must not
 * block get_iova readers. Idempotent for an already-mapped dev_dom.
 */
STATIC int dma_dom_pool_map_dev_dom(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom,
                                    struct dev_dom_info *dev_dom)
{
    struct ram_range_info *ram_info = NULL;
    ka_list_head_t *pos = NULL, *next = NULL;
    ka_device_t *dev = vdavinci_get_device(vdavinci);
    int ret = 0;

    if (dev_dom->status != DOMAIN_MAP_STATUS_INVALID) {
        return 0;
    }
    if (vdavinci->is_passthrough) {
        ret = hw_vdavinci_iommu_attach_group(vdavinci);
        if (ret) {
            vascend_err(dev, "dev attach new group failed, ret : %d", ret);
            return ret;
        }
    }
    ka_list_for_each_safe(pos, next, &(vm_dom->ram_info_list->head))
    {
        ram_info = ka_list_entry(pos, struct ram_range_info, list);
        ret = dev_dom->ops.dev_dma_map_ram_range(vdavinci, ram_info);
        if (ret) {
            vascend_err(dev, "dev dma map failed, ret : %d", ret);
            return ret;
        }
    }
    dev_dom->status = DOMAIN_MAP_STATUS_READY;

    return 0;
}

STATIC int dma_dom_pool_ensure_mapped(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    struct dev_dom_info *dev_dom = NULL;
    int ret = 0;

    dev_dom = dma_dom_pool_get_dev_dom(vdavinci, vm_dom);
    if (KA_IS_ERR(dev_dom)) {
        return KA_PTR_ERR(dev_dom);
    }

    ret = dma_dom_pool_map_dev_dom(vdavinci, vm_dom, dev_dom);
    if (ret != 0) {
        ka_task_down_write(&vm_dom->sem);
        dev_dom_info_put(dev_dom, vdavinci);
        ka_task_up_write(&vm_dom->sem);
        return ret;
    }

    return 0;
}

/*
 * Reverse of dma_dom_pool_get_dev_dom + dma_dom_pool_map_dev_dom: drop this
 * vdavinci's dev_dom; the last reference unmaps the ram ranges, detaches
 * the iommu group and frees the dev_dom. Idempotent: nothing to do when
 * the paired init never completed. The put runs under the write lock so
 * the release callback (list_del on the shared dev_dom list, unmap of the
 * ram ranges) stays mutually exclusive with the lockless-on-cache_lock
 * dev_dom_info_find readers of the other vdavincis. The shared pool
 * itself is kept so a following dma_pool_init (VM reset path) reuses it.
 */
STATIC void dma_dom_pool_ensure_unmapped(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom)
{
    struct dev_dom_info *dev_dom = NULL;

    ka_task_down_write(&vm_dom->sem);
    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (dev_dom == NULL) {
        ka_task_up_write(&vm_dom->sem);
        vascend_info(vdavinci_get_device(vdavinci), "dma pool had already uninited\n");
        return;
    }

    dev_dom_info_put(dev_dom, vdavinci);
    ka_task_up_write(&vm_dom->sem);
}

STATIC int hw_vdavinci_dma_pool_init_locked(struct hw_vdavinci *vdavinci)
{
    struct vm_dom_info *vm_dom = vdavinci->vdev.domain;
    ka_device_t *dev = vdavinci_get_device(vdavinci);
    bool pinned_by_self = false;
    int ret = 0;

    vascend_info(dev, "dma pool init start\n");

    ret = dma_dom_pool_ensure_pinned(vdavinci, vm_dom, &pinned_by_self);
    if (ret) {
        return ret;
    }

    ret = dma_dom_pool_ensure_mapped(vdavinci, vm_dom);
    if (ret) {
        if (pinned_by_self) {
            dma_dom_pool_ensure_unpinned(vdavinci, vm_dom);
        }
        return ret;
    }

    vascend_info(dev, "dma pool init success\n");
    return 0;
}

int hw_vdavinci_dma_pool_init(struct hw_vdavinci *vdavinci)
{
    int ret = -EINVAL;

    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    if (vdavinci->vdev.domain == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "vnpu domain is null\n");
        goto out;
    }
    ret = hw_vdavinci_dma_pool_init_locked(vdavinci);

out:
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
    return ret;
}

struct ram_range_info *get_ram_range_by_gfn(struct vm_dom_info *vm_dom, unsigned long gfn, unsigned long size)
{
    gfn_t npages = 0, range_end = 0;
    struct ram_range_info *ram_info = NULL;
    ka_list_head_t *pos = NULL, *next = NULL;

    if (ka_unlikely(vm_dom == NULL) || ka_list_empty(&vm_dom->ram_info_list->head)) {
        return NULL;
    }
    npages = KA_BASE_DIV_ROUND_UP(size, KA_MM_PAGE_SIZE);
    if (ka_unlikely(npages == 0)) {
        return NULL;
    }
    ka_list_for_each_safe(pos, next, &(vm_dom->ram_info_list->head))
    {
        ram_info = ka_list_entry(pos, struct ram_range_info, list);
        if (ram_info == NULL) {
            continue;
        }
        range_end = ram_info->base_gfn + ram_info->npages;
        if (gfn >= ram_info->base_gfn && gfn < range_end && npages <= range_end - gfn) {
            return ram_info;
        }
    }

    return NULL;
}

void hw_vdavinci_unplug_ram(struct hw_vdavinci *vdavinci, unsigned long start_gfn, unsigned long size)
{
    struct ram_range_info *ram_info = NULL;
    struct dev_dom_info *dev_dom = NULL;
    struct vm_dom_info *vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;

    if (vm_dom == NULL) {
        vascend_warn(vdavinci_to_dev(vdavinci), "vm's domain is null\n");
        return;
    }
    ka_task_down_write(&vm_dom->sem);
    ram_info = get_ram_range_by_gfn(vm_dom, start_gfn, size);
    if (ram_info == NULL) {
        vascend_warn(vdavinci_to_dev(vdavinci), "ram range has already been unplugged, size %lu\n", size);
        ka_task_up_write(&vm_dom->sem);
        return;
    }

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    /* first unmap */
    if (dev_dom != NULL) {
        dev_dom->ops.dev_dma_unmap_ram_range(vdavinci, ram_info);
    }
    if (ka_base_atomic_read(&ram_info->map_count) == 0) {
        hw_vdavinci_unpin_page_range(vdavinci, ram_info);
        ka_list_del(&(ram_info->list));
        ka_mm_kfree(ram_info);
    }
    ka_task_up_write(&vm_dom->sem);
    vascend_info(vdavinci_to_dev(vdavinci), "unplug ram success");
}

void hw_vdavinci_put_iova(ka_sg_table_t *dma_sgt)
{
    if (dma_sgt) {
        ka_base_sg_free_table(dma_sgt);
        ka_mm_kfree(dma_sgt);
    }
}

int hw_vdavinci_get_iova(struct hw_vdavinci *vdavinci, unsigned long gfn, unsigned long size, ka_sg_table_t **dma_sgt)
{
    int ret;
    struct vm_dom_info *vm_dom = NULL;
    struct dev_dom_info *dev_dom = NULL;
    ka_device_t *dev = NULL;

    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    if (!vdavinci->vdev.domain) {
        ret = -EINVAL;
        goto unlock;
    }

    vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    ka_task_down_read(&vm_dom->sem);

    /* unlock to support concurrent read */
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
    dev = vdavinci_get_device(vdavinci);

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (!dev_dom || dev_dom->status != DOMAIN_MAP_STATUS_READY) {
        vascend_err(dev, "dma pool not ready\n");
        ret = -ENODEV;
        goto up_read;
    }

    ret = dev_dom->ops.hw_vdavinci_get_iova_sg(vdavinci, vm_dom, gfn, size, dma_sgt);
up_read:
    ka_task_up_read(&vm_dom->sem);
    return ret;
unlock:
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
    return ret;
}

int hw_vdavinci_get_iova_batch(struct hw_vdavinci *vdavinci, unsigned long *gfn, unsigned long *dma_addr,
                               unsigned long count)
{
    int ret;
    struct dev_dom_info *dev_dom = NULL;
    struct vm_dom_info *vm_dom = NULL;

    ka_task_mutex_lock(&vdavinci->vdev.cache_lock);
    if (!vdavinci->vdev.domain) {
        ret = -EINVAL;
        goto unlock;
    }

    vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    ka_task_down_read(&vm_dom->sem);

    /* unlock to support concurrent read */
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);

    dev_dom = dev_dom_info_find(vm_dom, vdavinci);
    if (!dev_dom || dev_dom->status != DOMAIN_MAP_STATUS_READY) {
        vascend_err(vdavinci_to_dev(vdavinci), "dma pool not ready\n");
        ret = -ENODEV;
        goto up_read;
    }

    ret = dev_dom->ops.hw_vdavinci_get_iova_array(vdavinci, vm_dom, gfn, dma_addr, count);
up_read:
    ka_task_up_read(&vm_dom->sem);
    return ret;
unlock:
    ka_task_mutex_unlock(&vdavinci->vdev.cache_lock);
    return ret;
}

/**
 * The 2M area can be composed of multiple consecutive sgs, to determine
 * whether the gfn is in these sgs
 */
STATIC bool check_gfn_in_dma_sg(unsigned long gfn, unsigned long sg_gfn_base, unsigned long sg_gfn_len)
{
    if (gfn >= sg_gfn_base && gfn < (sg_gfn_base + sg_gfn_len)) {
        return true;
    }
    return false;
}

STATIC unsigned long get_gfn_in_sg_offset(unsigned long gfn, unsigned long sg_gfn_base)
{
    return (gfn - sg_gfn_base) * KA_MM_PAGE_SIZE;
}

STATIC void set_gfn_sgl(ka_scatterlist_t *new, ka_scatterlist_t *ogn, unsigned int gfn_sg_offset)
{
    if (new == NULL) {
        return;
    }
    ka_mm_sg_dma_address(new) = ka_mm_sg_dma_address(ogn) + gfn_sg_offset;
    ka_mm_sg_dma_len(new) = ka_mm_sg_dma_len(ogn) - gfn_sg_offset;
}

/**
 * if iova_info->dma_sgt is not null, return sg length and sg table
 * otherwise return sg length
 */
STATIC int hw_vdavinci_get_gfn_sg(struct hw_vdavinci *vdavinci, struct vdavinci_iova_info *iova_info)
{
    unsigned int i;
    unsigned int sg_len = 0;
    unsigned long sg_gfn_len = 0, dma_length = 0, gfn_sg_offset = 0;
    struct dev_dma_sgt **sgt_array = iova_info->sgt_array;
    unsigned long sgl_gfn_base = (*sgt_array)->gfn;
    ka_scatterlist_t *temp_sgl = NULL, *out_sgl = NULL;
    ka_sg_table_t *sg_table_2m = NULL;

    if (iova_info->dma_sgt != NULL && *iova_info->dma_sgt != NULL) {
        out_sgl = (*iova_info->dma_sgt)->sgl;
    }

    while (dma_length < iova_info->size) {
        sg_table_2m = (*sgt_array)->dma_sgt;

        ka_base_for_each_sg(sg_table_2m->sgl, temp_sgl, sg_table_2m->nents, i)
        {
            /* find the first sgl which gfn is in it, gfn_sg_offset is the
               intervel between the start of temp_sgl and gfn,
               so the gfn_sg_offset is caculated for only once */
            gfn_sg_offset = 0;
            if (sg_len == 0) {
                sg_gfn_len = ka_base_roundup(temp_sgl->length, KA_MM_PAGE_SIZE) / KA_MM_PAGE_SIZE;
                if (!check_gfn_in_dma_sg(iova_info->gfn, sgl_gfn_base, sg_gfn_len)) {
                    /* sgl before the start_sgl, skip */
                    sgl_gfn_base += sg_gfn_len;
                    continue;
                }
                /* the start_sgl */
                gfn_sg_offset = get_gfn_in_sg_offset(iova_info->gfn, sgl_gfn_base);
            }
            sg_len++;

            set_gfn_sgl(out_sgl, temp_sgl, gfn_sg_offset);
            dma_length += (ka_mm_sg_dma_len(temp_sgl) - gfn_sg_offset);
            if (dma_length >= iova_info->size) {
                if (out_sgl != NULL) { /* the last sgl */
                    ka_mm_sg_dma_len(out_sgl) = iova_info->size -
                                                (dma_length - (ka_mm_sg_dma_len(temp_sgl) - gfn_sg_offset));
                }
                break;
            }
            if (out_sgl != NULL) {
                out_sgl = ka_base_sg_next(out_sgl);
            }
        }
        sgt_array++;
    }
    iova_info->sg_len = sg_len;

    return 0;
}

STATIC struct dev_dma_info *get_dma_info_by_ram(struct ram_range_info *ram_info, struct hw_vdavinci *vdavinci)
{
    struct dev_dma_info *dma_info = NULL;
    ka_list_head_t *pos = NULL, *next = NULL;

    ka_list_for_each_safe(pos, next, &(vdavinci->vdev.dev_dma_info_list_head))
    {
        dma_info = ka_list_entry(pos, struct dev_dma_info, list);
        if (dma_info->ram_info == ram_info) {
            return dma_info;
        }
    }

    return NULL;
}

STATIC int get_iova_sgt_info(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom,
                             struct vdavinci_iova_info *iova_info)
{
    int ret = 0;
    unsigned long array_base = 0;
    struct ram_range_info *ram_info = NULL;
    struct dev_dma_info *dma_info = NULL;

    ram_info = get_ram_range_by_gfn(vm_dom, iova_info->gfn, iova_info->size);
    if (ram_info == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "dvt_get_dma_map_page invalid gfn %llx\n",
                    (unsigned long long)iova_info->gfn);
        return -EINVAL;
    }

    dma_info = get_dma_info_by_ram(ram_info, vdavinci);
    if (dma_info == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "get dma info failed\n");
        return -EINVAL;
    }

    /* base should be rounddown */
    array_base = (iova_info->gfn - ram_info->base_gfn) / KA_VFIO_PIN_PAGES_MAX_ENTRIES;
    iova_info->sgt_array = dma_info->sgt_array + array_base;

    ret = hw_vdavinci_get_gfn_sg(vdavinci, iova_info);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci), "get sg length error, ret: %d", ret);
        return ret;
    }
    return 0;
}

STATIC int set_iova_sgt_info(struct hw_vdavinci *vdavinci, ka_sg_table_t **dma_sgt,
                             struct vdavinci_iova_info *iova_info)
{
    int ret;

    *dma_sgt = ka_mm_kzalloc(sizeof(ka_sg_table_t), KA_GFP_KERNEL);
    if (*dma_sgt == NULL) {
        return -ENOMEM;
    }

    ret = ka_base_sg_alloc_table(*dma_sgt, iova_info->sg_len, KA_GFP_KERNEL);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci), "ka_base_sg_alloc_table return error result, ret: %d, sg_len: %u", ret,
                    iova_info->sg_len);
        ret = -ENOMEM;
        goto sgt_free;
    }

    iova_info->dma_sgt = dma_sgt;
    ret = hw_vdavinci_get_gfn_sg(vdavinci, iova_info);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci), "get sg list failed, ret: %d, gfn: 0x%lx, size: 0x%lx", ret,
                    iova_info->gfn, iova_info->size);
        ret = -ENOMEM;
        goto table_free;
    }
    return ret;

table_free:
    ka_base_sg_free_table(*dma_sgt);
    iova_info->dma_sgt = NULL;
sgt_free:
    ka_mm_kfree(*dma_sgt);
    return ret;
}

int hw_vdavinci_get_iova_sg(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, unsigned long gfn,
                            unsigned long size, ka_sg_table_t **dma_sgt)
{
    int ret;
    struct vdavinci_iova_info iova_info;

    iova_info.gfn = gfn;
    iova_info.size = size;
    iova_info.dma_sgt = NULL;
    iova_info.sg_len = 0;

    ret = get_iova_sgt_info(vdavinci, vm_dom, &iova_info);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci), "get iova sg table info failed, ret: %d, gfn: 0x%lx, size: 0x%lx", ret,
                    iova_info.gfn, iova_info.size);
        return ret;
    }
    ret = set_iova_sgt_info(vdavinci, dma_sgt, &iova_info);
    if (ret) {
        vascend_err(vdavinci_to_dev(vdavinci), "set iova sg table info failed, ret: %d", ret);
        return ret;
    }

    return ret;
}

STATIC int get_iova_by_sg(struct hw_vdavinci *vdavinci, struct dev_dma_sgt **sgt_array_base, unsigned long gfn,
                          unsigned long *dma_addr)
{
    unsigned int i;
    unsigned long sg_gfn_len;
    unsigned long gfn_sg_offset = 0;
    unsigned long sgl_gfn_base = 0;
    ka_scatterlist_t *temp_sgl = NULL;
    ka_sg_table_t *sg_table_2m = NULL;

    if (!(*sgt_array_base)) {
        return -EINVAL;
    }

    sgl_gfn_base = (*sgt_array_base)->gfn;
    sg_table_2m = (*sgt_array_base)->dma_sgt;

    ka_base_for_each_sg(sg_table_2m->sgl, temp_sgl, sg_table_2m->nents, i)
    {
        sg_gfn_len = ka_base_roundup(temp_sgl->length, KA_MM_PAGE_SIZE) / KA_MM_PAGE_SIZE;
        if (!check_gfn_in_dma_sg(gfn, sgl_gfn_base, sg_gfn_len)) {
            sgl_gfn_base += sg_gfn_len;
            continue;
        }

        gfn_sg_offset = get_gfn_in_sg_offset(gfn, sgl_gfn_base);
        *dma_addr = ka_mm_sg_dma_address(temp_sgl) + gfn_sg_offset;
        return 0;
    }

    vascend_err(vdavinci_to_dev(vdavinci), "can not find gfn in sg list, gfn: 0x%lx, the base gfn sgl: 0x%llx", gfn,
                (*sgt_array_base)->gfn);
    return -ENODEV;
}

int hw_vdavinci_get_iova_array(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, unsigned long *gfn,
                               unsigned long *dma_addr, unsigned long count)
{
    int ret = 0;
    unsigned long index = 0, array_base = 0;
    struct ram_range_info *ram_info = NULL;
    struct dev_dma_sgt **sgt_array_base = NULL;
    struct dev_dma_info *dma_info = NULL;

    while (index != count) {
        if (ram_info == NULL || gfn[index] < ram_info->base_gfn ||
            gfn[index] >= ram_info->base_gfn + ram_info->npages) {
            ram_info = get_ram_range_by_gfn(vm_dom, gfn[index], KA_MM_PAGE_SIZE);
            if (ram_info == NULL) {
                vascend_err(vdavinci_to_dev(vdavinci), "get iova batch failed, invalid gfn %lx\n", gfn[index]);
                return -EINVAL;
            }

            dma_info = get_dma_info_by_ram(ram_info, vdavinci);
            if (dma_info == NULL) {
                return -ENODEV;
            }
        }

        /* base should be rounddown */
        array_base = (gfn[index] - ram_info->base_gfn) / KA_VFIO_PIN_PAGES_MAX_ENTRIES;
        sgt_array_base = dma_info->sgt_array + array_base;

        ret = get_iova_by_sg(vdavinci, sgt_array_base, gfn[index], &dma_addr[index]);
        if (ret) {
            vascend_err(vdavinci_to_dev(vdavinci), "get iova batch failed, invalid gfn %lx\n", gfn[index]);
            return ret;
        }
        index++;
    }

    return ret;
}

int vf_get_iova_sg(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, unsigned long gfn, unsigned long size,
                   ka_sg_table_t **dma_sgt)
{
    int ret;
    struct ram_range_info *ram_info = NULL;
    struct dev_dma_info *dma_info = NULL;

    ram_info = get_ram_range_by_gfn(vm_dom, gfn, size);
    if (ram_info == NULL) {
        vascend_err(vdavinci_to_dev(vdavinci), "get iova failed, invalid gfn %llx\n", (unsigned long long)gfn);
        return -EINVAL;
    }

    *dma_sgt = ka_mm_kzalloc(sizeof(ka_sg_table_t), KA_GFP_KERNEL);
    if (*dma_sgt == NULL) {
        return -ENOMEM;
    }

    ret = ka_base_sg_alloc_table(*dma_sgt, 1, KA_GFP_KERNEL);
    if (ret) {
        goto free_sgt;
    }

    dma_info = get_dma_info_by_ram(ram_info, vdavinci);
    if (dma_info == NULL) {
        ret = -ENODEV;
        goto free_table;
    }

    ka_mm_sg_dma_address((*dma_sgt)->sgl) = dma_info->base_iova + (gfn - ram_info->base_gfn) * KA_MM_PAGE_SIZE;
    ka_mm_sg_dma_len((*dma_sgt)->sgl) = size;

    return 0;

free_table:
    ka_base_sg_free_table(*dma_sgt);
free_sgt:
    ka_mm_kfree(*dma_sgt);
    *dma_sgt = NULL;

    return ret;
}

int vf_get_iova_array(struct hw_vdavinci *vdavinci, struct vm_dom_info *vm_dom, unsigned long *gfn,
                      unsigned long *dma_addr, unsigned long count)
{
    unsigned long index = 0;
    struct ram_range_info *ram_info = NULL;
    struct dev_dma_info *dma_info = NULL;

    while (index != count) {
        if (ram_info == NULL || gfn[index] < ram_info->base_gfn ||
            gfn[index] >= ram_info->base_gfn + ram_info->npages) {
            ram_info = get_ram_range_by_gfn(vm_dom, gfn[index], KA_MM_PAGE_SIZE);
            if (ram_info == NULL) {
                vascend_err(vdavinci_to_dev(vdavinci), "get iova array failed, invalid gfn %lx\n", gfn[index]);
                return -EINVAL;
            }

            dma_info = get_dma_info_by_ram(ram_info, vdavinci);
            if (dma_info == NULL) {
                return -ENODEV;
            }
        }

        dma_addr[index] = dma_info->base_iova + (gfn[index] - ram_info->base_gfn) * KA_MM_PAGE_SIZE;
        index++;
    }

    return 0;
}

bool is_vm_pfn_valid(ka_device_t *dev, unsigned long pfn, unsigned long size)
{
    bool ret = 0;
    struct vm_dom_info *vm_dom = NULL;
    struct hw_vdavinci *vdavinci = find_vdavinci(dev);

    if (ka_unlikely(vdavinci == NULL || vdavinci->vdev.domain == NULL)) {
        return false;
    }
    vm_dom = (struct vm_dom_info *)vdavinci->vdev.domain;
    ka_task_down_read(&vm_dom->sem);
    ret = is_vm_pfn_managed(vm_dom, pfn, size);
    ka_task_up_read(&vm_dom->sem);

    return ret;
}
