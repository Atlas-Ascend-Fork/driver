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

#ifndef UVM_HEAP_MNG_H
#define UVM_HEAP_MNG_H

#include "svm_log.h"
#include "ascend_hal_define.h"
#include "devmm_adapt.h"
#include "ka_compiler_pub.h"

#define DEVMM_UVM_MAX_DEVICE_NUM 64
/* uvm heap mng */
#define UVM_PAGE_BITMAP_ALLOCED_BIT 0
#define UVM_PAGE_IS_FIRST_PAGE_BIT 1
#define UVM_PAGE_BITMAP_HOST_MAPPED_BIT 2
#define UVM_PAGE_BITMAP_DEVICE_MAPPED_BIT 3
#define UVM_PAGE_BITMAP_READ_MOSTLY_BIT 4
#define UVM_PAGE_BITMAP_IS_LOCKED_BIT 5 /* lock bit */
#define UVM_PAGE_BITMAP_PREFERRED_LOC_BIT 6
#define UVM_PAGE_BITMAP_LAST_PREFETCH_BIT 13
#define UVM_PAGE_BITMAP_PREFERRED_LOC_VALID_BIT 20
#define UVM_PAGE_BITMAP_LAST_PREFETCH_VALID_BIT 21
#define UVM_PAGE_BITMAP_PREFERRED_LOC_TYPE_BIT 22
#define UVM_PAGE_BITMAP_LAST_PREFETCH_TYPE_BIT 24
#define UVM_PAGE_LENGTH_OF_FIRST_ALLOC 26

/* deviceID field length macro */
#define PAGE_BITMAP_DEVICE_ID_BITS 7
/* preferred_loc type length macro */
#define PAGE_BITMAP_PREFERRED_LOC_TYPE_LEN_BITS 2
/* last_prefetch_loc type length macro */
#define PAGE_BITMAP_LAST_PREFETCH_TYPE_LEN_BITS 2
/* virtual memory allocation length macro */
#define PAGE_BITMAP_ALLOC_LEN_BITS 22

/* bit mask */
#define UVM_PAGE_BITMAP_ALLOCED_MASK (1ULL << UVM_PAGE_BITMAP_ALLOCED_BIT)
#define UVM_PAGE_IS_FIRST_PAGE_MASK (1ULL << UVM_PAGE_IS_FIRST_PAGE_BIT)
#define UVM_PAGE_BITMAP_HOST_MAPPED_MASK (1ULL << UVM_PAGE_BITMAP_HOST_MAPPED_BIT)
#define UVM_PAGE_BITMAP_DEVICE_MAPPED_MASK (1ULL << UVM_PAGE_BITMAP_DEVICE_MAPPED_BIT)
#define UVM_PAGE_BITMAP_READ_MOSTLY_MASK (1ULL << UVM_PAGE_BITMAP_READ_MOSTLY_BIT)
#define UVM_PAGE_BITMAP_IS_LOCKED_MASK (1ULL << UVM_PAGE_BITMAP_IS_LOCKED_BIT)
#define PAGE_BITMAP_DEVICE_ID_MASK ((1ULL << PAGE_BITMAP_DEVICE_ID_BITS) - 1)
#define PAGE_BITMAP_DEVICE_TYPE_ID_MASK ((1ULL << PAGE_BITMAP_PREFERRED_LOC_TYPE_LEN_BITS) - 1)
#define PAGE_BITMAP_LAST_PREFETCH_ID_MASK ((1ULL << PAGE_BITMAP_LAST_PREFETCH_TYPE_LEN_BITS) - 1)
#define PAGE_BITMAP_PREFERRED_LOC_DEVICE_ID_FULL_MASK (PAGE_BITMAP_DEVICE_ID_MASK << UVM_PAGE_BITMAP_PREFERRED_LOC_BIT)
#define PAGE_BITMAP_LAST_PREFETCH_DEVICE_ID_FULL_MASK (PAGE_BITMAP_DEVICE_ID_MASK << UVM_PAGE_BITMAP_LAST_PREFETCH_BIT)
#define PAGE_BITMAP_PREFERRED_LOC_VALID_MASK (1U << UVM_PAGE_BITMAP_PREFERRED_LOC_VALID_BIT)
#define PAGE_BITMAP_PREFETCH_VALID_MASK (1U << UVM_PAGE_BITMAP_LAST_PREFETCH_VALID_BIT)
#define PAGE_BITMAP_PREFERRED_LOC_TYPE_FULL_MASK \
    (PAGE_BITMAP_DEVICE_TYPE_ID_MASK << UVM_PAGE_BITMAP_PREFERRED_LOC_TYPE_BIT)
#define PAGE_BITMAP_LAST_PREFETCH_TYPE_FULL_MASK \
    (PAGE_BITMAP_LAST_PREFETCH_ID_MASK << UVM_PAGE_BITMAP_LAST_PREFETCH_TYPE_BIT)
#define PAGE_BITMAP_ALLOC_LEN_MASK ((1ULL << PAGE_BITMAP_ALLOC_LEN_BITS) - 1)
#define PAGE_BITMAP_ALLOC_LEN_FULL_MASK (PAGE_BITMAP_ALLOC_LEN_MASK << UVM_PAGE_LENGTH_OF_FIRST_ALLOC)

#define BITMAP_BITS_COUNT (DEVMM_UVM_MAX_DEVICE_NUM + 1)
#define BITMAP_SIZE ((BITMAP_BITS_COUNT + 31) / 32)
#define DEVICE_BITMAP_BIT_NUM 32

#define UVM_INVALID_DEVICE_ID ((1ULL << PAGE_BITMAP_DEVICE_ID_BITS) - 1)

struct uvm_page_mapping {
    u64 pa_addr;
    u32 devid;
};

typedef ka_atomic_t device_bitmap[BITMAP_SIZE];

struct uvm_page_cnt_status {
    ka_atomic64_t page_num[DEVMM_UVM_MAX_DEVICE_NUM + 1];
};

typedef u64 page_bitmap_t;

struct devmm_uvm_heap {
    u32 chunk_page_size;
    u64 start;
    u64 heap_size;
    u32 max_used_page;
    ka_rwlock_t uvm_heap_lock;
    device_bitmap invalid_devices;
    page_bitmap_t page_bitmaps[DEVMM_UVM_PAGE_NUM];
    /* page va to pa lists */
    struct uvm_page_mapping page_maps[DEVMM_UVM_PAGE_NUM];
    /* record va mapped devices */
    device_bitmap device_bitmaps[DEVMM_UVM_PAGE_NUM];
    device_bitmap access_by_bitmaps[DEVMM_UVM_PAGE_NUM];
    /* read/write lock of every page */
    ka_rw_semaphore_t lock_array[DEVMM_UVM_PAGE_NUM];
    struct uvm_page_cnt_status page_cnt_status;
};

struct uvm_page_info {
    uint32_t chunk_page_size;
    page_bitmap_t *page_bitmap;
    struct uvm_page_mapping *page_map;
    device_bitmap *dev_bitmap;
    device_bitmap *access_by_bitmap;
    ka_rw_semaphore_t *page_rwlock;
    uint64_t va_align;
};

struct uvm_mem_range {
    uint64_t aligned_va;
    uint64_t aligned_cnt;
};

int devmm_get_virt_pfn_by_uvm_heap(const struct devmm_uvm_heap *heap, u64 va, unsigned long *pfn);
int uvm_get_page_info_by_va(struct devmm_uvm_heap *heap, u64 va, struct uvm_page_info *page_info);
ka_rw_semaphore_t *uvm_get_page_lock_array(struct devmm_uvm_heap *heap, uint64_t va);

static inline void set_page_mapping(struct uvm_page_mapping *page_map, uint64_t pa_addr, uint32_t devid)
{
    page_map->pa_addr = pa_addr;
    page_map->devid = devid;
}

static inline void clear_page_mapping(struct uvm_page_mapping *page_map)
{
    page_map->pa_addr = 0;
    page_map->devid = 0;
}

static inline uint64_t get_page_mapping_pa(struct uvm_page_mapping *page_map)
{
    return page_map->pa_addr;
}

static inline uint64_t get_page_mapping_devid(struct uvm_page_mapping *page_map)
{
    return page_map->devid;
}

/* Map virtual memory to a specific device (atomic operation) */
static inline void set_device_present(device_bitmap *bitmap, int device_id)
{
    int index;
    int bit;

    if (device_id < 0 || device_id >= BITMAP_BITS_COUNT) {
        return;
    }
    index = device_id / DEVICE_BITMAP_BIT_NUM;
    bit = device_id % DEVICE_BITMAP_BIT_NUM;
    ka_base_atomic_or(BIT_ULL(bit), &(*bitmap)[index]);
}

/* Check if virtual memory is mapped on a specific device (atomic operation) */
static inline bool is_device_present(device_bitmap *bitmap, int device_id)
{
    int index;
    int bit;

    if (device_id < 0 || device_id >= BITMAP_BITS_COUNT) {
        return false;
    }
    index = device_id / DEVICE_BITMAP_BIT_NUM;
    bit = device_id % DEVICE_BITMAP_BIT_NUM;
    return ka_base_atomic_read(&(*bitmap)[index]) & BIT_ULL(bit);
}

/* Set virtual memory to map to all devices (atomic operation) */
static inline void set_all_device_present(device_bitmap *bitmap)
{
    int i;
    for (i = 0; i < BITMAP_SIZE; i++) {
        ka_base_atomic_set(&(*bitmap)[i], ~0U);
    }
}

/* Query which devices have the virtual address (atomic operation) */
static inline int get_present_devices(device_bitmap *bitmap, int *device_ids, int max_count)
{
    int count = 0;
    int device_id = 0;
    int index;
    int bit;

    for (device_id = 0; device_id < BITMAP_BITS_COUNT; device_id++) {
        index = device_id / DEVICE_BITMAP_BIT_NUM;
        bit = device_id % DEVICE_BITMAP_BIT_NUM;

        if (ka_base_atomic_read(&(*bitmap)[index]) & BIT_ULL(bit)) {
            if (count >= max_count) {
                break;
            }
            device_ids[count++] = device_id;
        }
    }

    return count;
}

static inline int get_present_devices_count(device_bitmap *bitmap)
{
    int count = 0;
    int device_id = 0;
    int index;
    int bit;

    for (device_id = 0; device_id < BITMAP_BITS_COUNT; device_id++) {
        index = device_id / DEVICE_BITMAP_BIT_NUM;
        bit = device_id % DEVICE_BITMAP_BIT_NUM;

        if (ka_base_atomic_read(&(*bitmap)[index]) & BIT_ULL(bit)) {
            count++;
        }
    }

    return count;
}

/* Clear the mapping for a specific device (atomic operation) */
static inline void clear_device_present(device_bitmap *bitmap, int device_id)
{
    int index;
    int bit;
    if (device_id < 0 || device_id >= BITMAP_BITS_COUNT) {
        return;
    }

    index = device_id / DEVICE_BITMAP_BIT_NUM;
    bit = device_id % DEVICE_BITMAP_BIT_NUM;
    ka_base_atomic_and(~BIT_ULL(bit), &(*bitmap)[index]);
}

/* Clear all device mappings (atomic operation) */
static inline void clear_all_devices(device_bitmap *bitmap)
{
    int i;
    for (i = 0; i < BITMAP_SIZE; i++) {
        ka_base_atomic_set(&(*bitmap)[i], 0);
    }
}

static inline void uvm_page_bitmap_lock(page_bitmap_t *bitmap)
{
    while (ka_base_test_and_set_bit(UVM_PAGE_BITMAP_IS_LOCKED_BIT, (unsigned long *)bitmap)) {
        ka_system_cpu_relax();
    }
}

static inline void uvm_page_bitmap_unlock(page_bitmap_t *bitmap)
{
    ka_base_clear_bit(UVM_PAGE_BITMAP_IS_LOCKED_BIT, (unsigned long *)bitmap);
}

static inline void page_bitmap_set_unlocked(page_bitmap_t *b, uint64_t mask)
{
    (*b) |= mask;
}

static inline void page_bitmap_set(page_bitmap_t *b, uint64_t mask)
{
    uvm_page_bitmap_lock(b);
    page_bitmap_set_unlocked(b, mask);
    uvm_page_bitmap_unlock(b);
}

static inline void page_bitmap_unset(page_bitmap_t *b, uint64_t mask)
{
    uvm_page_bitmap_lock(b);
    (*b) &= (~mask);
    uvm_page_bitmap_unlock(b);
}

/* If set, returns true */
static inline bool page_bitmap_get_unlocked(page_bitmap_t *b, uint64_t mask)
{
    return ((*b) & mask) != 0;
}

/* If set, returns true */
static inline bool page_bitmap_get(page_bitmap_t *b, uint64_t mask)
{
    bool flag;
    uvm_page_bitmap_lock(b);
    flag = page_bitmap_get_unlocked(b, mask);
    uvm_page_bitmap_unlock(b);
    return flag;
}

static inline void page_bitmap_set_field_unlocked(page_bitmap_t *b, uint64_t mask, uint32_t shift, uint64_t val)
{
    *b = ((*b) & ~mask) | ((val << shift) & mask);
}

static inline void page_bitmap_set_field(page_bitmap_t *b, uint64_t mask, uint32_t shift, uint64_t val)
{
    uvm_page_bitmap_lock(b);
    page_bitmap_set_field_unlocked(b, mask, shift, val);
    uvm_page_bitmap_unlock(b);
}

static inline uint64_t page_bitmap_get_field_unlocked(page_bitmap_t *b, uint64_t mask, uint32_t shift)
{
    return ((*b) & mask) >> shift;
}

static inline uint64_t page_bitmap_get_field(page_bitmap_t *b, uint64_t mask, uint32_t shift)
{
    uint64_t field;
    uvm_page_bitmap_lock(b);
    field = page_bitmap_get_field_unlocked(b, mask, shift);
    uvm_page_bitmap_unlock(b);
    return field;
}

/* Clear the flag bit; the lock bit remains set. To release the lock, you must explicitly call unlock. */
static inline void page_bitmap_clean(page_bitmap_t *bitmap)
{
    uvm_page_bitmap_lock(bitmap);
    (*bitmap) &= UVM_PAGE_BITMAP_IS_LOCKED_MASK;
    uvm_page_bitmap_unlock(bitmap);
}

static inline page_bitmap_t page_bitmap_read(page_bitmap_t *bitmap)
{
    return ((*bitmap) & (~UVM_PAGE_BITMAP_IS_LOCKED_MASK));
}

/* alloced op */
static inline void page_bitmap_set_alloced(page_bitmap_t *b)
{
    page_bitmap_set(b, UVM_PAGE_BITMAP_ALLOCED_MASK);
}

static inline void page_bitmap_unset_alloced(page_bitmap_t *b)
{
    page_bitmap_unset(b, UVM_PAGE_BITMAP_ALLOCED_MASK);
}

static inline bool page_bitmap_get_alloced(page_bitmap_t *b)
{
    return page_bitmap_get(b, UVM_PAGE_BITMAP_ALLOCED_MASK);
}

static inline void page_bitmap_set_first_page(page_bitmap_t *b)
{
    page_bitmap_set(b, UVM_PAGE_IS_FIRST_PAGE_MASK);
}

static inline void page_bitmap_unset_first_page(page_bitmap_t *b)
{
    page_bitmap_unset(b, UVM_PAGE_IS_FIRST_PAGE_MASK);
}

static inline bool page_bitmap_get_first_page(page_bitmap_t *b)
{
    return page_bitmap_get(b, UVM_PAGE_IS_FIRST_PAGE_MASK);
}

/* host maped op */
static inline void page_bitmap_set_host_mapped(page_bitmap_t *b)
{
    page_bitmap_set(b, UVM_PAGE_BITMAP_HOST_MAPPED_MASK);
}
static inline void page_bitmap_unset_host_mapped(page_bitmap_t *b)
{
    page_bitmap_unset(b, UVM_PAGE_BITMAP_HOST_MAPPED_MASK);
}
static inline bool page_bitmap_get_host_mapped(page_bitmap_t *b)
{
    return page_bitmap_get(b, UVM_PAGE_BITMAP_HOST_MAPPED_MASK);
}

/* device maped op */
static inline void page_bitmap_set_device_mapped(page_bitmap_t *b)
{
    page_bitmap_set(b, UVM_PAGE_BITMAP_DEVICE_MAPPED_MASK);
}
static inline void page_bitmap_unset_device_mapped(page_bitmap_t *b)
{
    page_bitmap_unset(b, UVM_PAGE_BITMAP_DEVICE_MAPPED_MASK);
}
static inline bool page_bitmap_get_device_mapped(page_bitmap_t *b)
{
    return page_bitmap_get(b, UVM_PAGE_BITMAP_DEVICE_MAPPED_MASK);
}

/* read_mostly op */
static inline void page_bitmap_set_read_mostly(page_bitmap_t *b)
{
    page_bitmap_set(b, UVM_PAGE_BITMAP_READ_MOSTLY_MASK);
}
static inline void page_bitmap_unset_read_mostly(page_bitmap_t *b)
{
    page_bitmap_unset(b, UVM_PAGE_BITMAP_READ_MOSTLY_MASK);
}
static inline bool page_bitmap_get_read_mostly(page_bitmap_t *b)
{
    return page_bitmap_get(b, UVM_PAGE_BITMAP_READ_MOSTLY_MASK);
}

static inline void page_bitmap_set_field_entry(page_bitmap_t *b, uint64_t entry_mask, uint32_t entry_shift,
                                               uint64_t valid_mask, uint16_t id)
{
    uvm_page_bitmap_lock(b);
    page_bitmap_set_field_unlocked(b, entry_mask, entry_shift, id);
    page_bitmap_set_unlocked(b, valid_mask);
    uvm_page_bitmap_unlock(b);
}

static inline uint16_t page_bitmap_get_field_entry(page_bitmap_t *b, uint64_t entry_mask, uint32_t entry_shift,
                                                   uint64_t valid_mask)
{
    uint16_t id;
    uvm_page_bitmap_lock(b);
    if (!page_bitmap_get_unlocked(b, valid_mask)) {
        id = UVM_INVALID_DEVICE_ID;
    } else {
        id = (uint16_t)page_bitmap_get_field_unlocked(b, entry_mask, entry_shift);
    }
    uvm_page_bitmap_unlock(b);
    return id;
}
/* preferred op */
static inline void page_bitmap_set_preferred_loc(page_bitmap_t *b, uint16_t id)
{
    page_bitmap_set_field_entry(b, PAGE_BITMAP_PREFERRED_LOC_DEVICE_ID_FULL_MASK, UVM_PAGE_BITMAP_PREFERRED_LOC_BIT,
                                PAGE_BITMAP_PREFERRED_LOC_VALID_MASK, id);
}
static inline void page_bitmap_unset_preferred_loc(page_bitmap_t *b)
{
    page_bitmap_unset(b, PAGE_BITMAP_PREFERRED_LOC_VALID_MASK);
}
static inline uint16_t page_bitmap_get_preferred_loc(page_bitmap_t *b)
{
    return page_bitmap_get_field_entry(b, PAGE_BITMAP_PREFERRED_LOC_DEVICE_ID_FULL_MASK,
                                       UVM_PAGE_BITMAP_PREFERRED_LOC_BIT, PAGE_BITMAP_PREFERRED_LOC_VALID_MASK);
}
/* preferred_type op */
static inline void page_bitmap_set_preferred_loc_type(page_bitmap_t *b, uint16_t type)
{
    uvm_page_bitmap_lock(b);
    page_bitmap_set_field_unlocked(b, PAGE_BITMAP_PREFERRED_LOC_TYPE_FULL_MASK, UVM_PAGE_BITMAP_PREFERRED_LOC_TYPE_BIT,
                                   type);
    uvm_page_bitmap_unlock(b);
}
static inline void page_bitmap_unset_preferred_loc_type(page_bitmap_t *b)
{
    uvm_page_bitmap_lock(b);
    page_bitmap_set_field_unlocked(b, PAGE_BITMAP_PREFERRED_LOC_TYPE_FULL_MASK, UVM_PAGE_BITMAP_PREFERRED_LOC_TYPE_BIT,
                                   0);
    uvm_page_bitmap_unlock(b);
}
static inline uint16_t page_bitmap_get_preferred_loc_type(page_bitmap_t *b)
{
    return (uint16_t)page_bitmap_get_field_unlocked(b, PAGE_BITMAP_PREFERRED_LOC_TYPE_FULL_MASK,
                                                    UVM_PAGE_BITMAP_PREFERRED_LOC_TYPE_BIT);
}

/* last prefetch op */
static inline void page_bitmap_set_last_prefetch_loc(page_bitmap_t *b, uint16_t id)
{
    page_bitmap_set_field_entry(b, PAGE_BITMAP_LAST_PREFETCH_DEVICE_ID_FULL_MASK, UVM_PAGE_BITMAP_LAST_PREFETCH_BIT,
                                PAGE_BITMAP_PREFETCH_VALID_MASK, id);
}
static inline void page_bitmap_unset_last_prefetch_loc(page_bitmap_t *b)
{
    page_bitmap_unset(b, PAGE_BITMAP_PREFETCH_VALID_MASK);
}
static inline uint16_t page_bitmap_get_last_prefetch_loc(page_bitmap_t *b)
{
    return page_bitmap_get_field_entry(b, PAGE_BITMAP_LAST_PREFETCH_DEVICE_ID_FULL_MASK,
                                       UVM_PAGE_BITMAP_LAST_PREFETCH_BIT, PAGE_BITMAP_PREFETCH_VALID_MASK);
}

/* last_prefetch_loc_type op */
static inline void page_bitmap_set_last_prefetch_loc_type(page_bitmap_t *b, uint16_t type)
{
    uvm_page_bitmap_lock(b);
    page_bitmap_set_field_unlocked(b, PAGE_BITMAP_LAST_PREFETCH_TYPE_FULL_MASK, UVM_PAGE_BITMAP_LAST_PREFETCH_TYPE_BIT,
                                   type);
    uvm_page_bitmap_unlock(b);
}

static inline void page_bitmap_unset_last_prefetch_loc_type(page_bitmap_t *b)
{
    uvm_page_bitmap_lock(b);
    page_bitmap_set_field_unlocked(b, PAGE_BITMAP_LAST_PREFETCH_TYPE_FULL_MASK, UVM_PAGE_BITMAP_LAST_PREFETCH_TYPE_BIT,
                                   DRV_UVM_LOCATION_TYPE_INVALID);
    uvm_page_bitmap_unlock(b);
}

static inline uint16_t page_bitmap_get_last_prefetch_loc_type(page_bitmap_t *b)
{
    return (uint16_t)page_bitmap_get_field_unlocked(b, PAGE_BITMAP_LAST_PREFETCH_TYPE_FULL_MASK,
                                                    UVM_PAGE_BITMAP_LAST_PREFETCH_TYPE_BIT);
}

/* alloc len op */
static inline void page_bitmap_set_alloc_len(page_bitmap_t *b, uint64_t len)
{
    page_bitmap_set_field(b, PAGE_BITMAP_ALLOC_LEN_FULL_MASK, UVM_PAGE_LENGTH_OF_FIRST_ALLOC, len);
}

static inline uint64_t page_bitmap_get_alloc_len(page_bitmap_t *b)
{
    return (uint64_t)page_bitmap_get_field(b, PAGE_BITMAP_ALLOC_LEN_FULL_MASK, UVM_PAGE_LENGTH_OF_FIRST_ALLOC);
}

static inline bool page_is_no_mapped(page_bitmap_t *bitmap)
{
    return (page_bitmap_get_host_mapped(bitmap) == false) && (page_bitmap_get_device_mapped(bitmap) == false);
}

static inline bool page_is_mapped_other_device(uint16_t dev_id, page_bitmap_t *bitmap,
                                               struct uvm_page_mapping *page_map)
{
    return (page_bitmap_get_device_mapped(bitmap) == true) && (page_map->devid < UVM_HOST_ID) &&
           (page_map->pa_addr != 0) && (page_map->devid != dev_id);
}

static inline bool page_is_mapped_device(page_bitmap_t *bitmap, struct uvm_page_mapping *page_map)
{
    return (page_bitmap_get_device_mapped(bitmap) == true) && (page_map->devid < UVM_HOST_ID) &&
           (page_map->pa_addr != 0);
}

static inline bool page_is_mapped_host(page_bitmap_t *bitmap, struct uvm_page_mapping *page_map)
{
    return (page_bitmap_get_host_mapped(bitmap) == true) && (page_map->devid == UVM_HOST_ID) &&
           (page_map->pa_addr != 0);
}

static inline void uvm_set_bitmap_mapped(page_bitmap_t *page_bitmap, uint32_t dev_id)
{
    if (!page_bitmap_get_alloced(page_bitmap)) {
        devmm_drv_err("Bit map none alloc. (dev_id=%u)\n", dev_id);
        return;
    }

    if (dev_id < UVM_HOST_ID) {
        page_bitmap_set_device_mapped(page_bitmap);
    } else {
        page_bitmap_set_host_mapped(page_bitmap);
    }
}

static inline void uvm_set_page_mapping(struct uvm_page_mapping *page_mapping, ka_page_t *page, int dev_id)
{
    uint64_t pfn;
    uint64_t pa;

    pfn = ka_mm_page_to_pfn(page);
    pa = pfn << KA_MM_PAGE_SHIFT;
    set_page_mapping(page_mapping, pa, dev_id);
}

#endif /* __UVM_HEAP_MNG_H__ */
