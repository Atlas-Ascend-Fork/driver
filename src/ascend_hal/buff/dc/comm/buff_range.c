/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <unistd.h>

#include "drv_buff_unibuff.h"
#include "drv_buff_common.h"
#include "atomic_ref.h"
#include "buff_mng.h"
#include "buff_ioctl.h"
#include "drv_buff_mbuf.h"
#include "drv_buff_memzone.h"
#include "buff_user_interface.h"
#include "buff_range.h"

#define RANGE_IDLE_CNT_MAX 5

#define BUFF_RANGE_MAX_BLOCKS_NUM XSMEM_BLOCK_MAX_NUM /* 512 * 512 */
#define BUFF_RANGE_MAX_ROWS_NUM 512
#define BUFF_RANGE_MAX_COLUMNS_NUM 512

#define BUFF_RANGE_COLUMN_MASK (0x1FF)
#define BUFF_RANGE_ROW_BIT_SHIFT (9)

#define ATOMIC_SET(x, y) __sync_lock_test_and_set((x), (y))
#define CAS(ptr, oldval, newval) __sync_bool_compare_and_swap(ptr, oldval, newval)

struct section_base {
    unsigned int owner : 1;
    unsigned int idlecnt : 7;  /* RANGE_IDLE_CNT_MAX */
    unsigned int pool_id : 24; /* MAX_XSM_POOL_NUM */
};

typedef union range_section_base {
    struct section_base info;
    unsigned int value;
} range_section_base;

struct range_section {
    unsigned int devid;
    union atomic_status status;
    range_section_base base;
    void *start;
    unsigned long size;
    unsigned long long mem_mng; /* pointer to struct common_handle_t */
};

struct range_section_array {
    struct range_section *cur_addr;
};

static THREAD struct range_section_array g_range_section[BUFF_RANGE_MAX_ROWS_NUM] = {};

static drvError_t add_others_buff_to_range(unsigned int block_id, int pool_id, void *start, unsigned long size,
                                           unsigned long long mem_mng);

static inline drvError_t range_get_index(unsigned int block_id, unsigned int *row, unsigned int *column)
{
    if (block_id >= BUFF_RANGE_MAX_BLOCKS_NUM) {
        buff_err("Block id is invalid. (block_id=%u)\n", block_id);
        return DRV_ERROR_PARA_ERROR;
    }

    *row = block_id >> BUFF_RANGE_ROW_BIT_SHIFT;
    *column = block_id & BUFF_RANGE_COLUMN_MASK;
    return DRV_ERROR_NONE;
}

static inline struct range_section_array *range_get_array(unsigned int row)
{
    return &g_range_section[row];
}

static inline bool range_section_array_is_valid(struct range_section_array *array)
{
    return (array->cur_addr != NULL);
}

static bool range_section_array_get(struct range_section_array *array)
{
    struct range_section *addr = NULL;
    if (!range_section_array_is_valid(array)) {
        addr = (struct range_section *)calloc(BUFF_RANGE_MAX_COLUMNS_NUM, sizeof(struct range_section));
        if (addr == NULL) {
            buff_err("Malloc section array failed.\n");
            return false;
        }

        if (!CAS(&array->cur_addr, NULL, addr)) {
            free(addr);
        }
    }

    return true;
}

static inline bool range_section_is_valid(struct range_section *section)
{
    return atomic_ref_is_valid(&section->status);
}

static inline bool is_others_section(const struct range_section *section)
{
    return (section->base.info.owner == RANGE_OWNER_OTHERS);
}

static inline bool is_self_section(const struct range_section *section)
{
    return (section->base.info.owner == RANGE_OWNER_SELF);
}

static inline bool buff_range_check(const void *blk_start, unsigned long blk_size, const void *start,
                                    unsigned long size)
{
    if (size > (ULONG_MAX - (unsigned long)(uintptr_t)start)) {
        return false;
    }

    if ((start < blk_start) || (((uintptr_t)start + size) > ((uintptr_t)blk_start + blk_size))) {
        return false;
    }

    return true;
}

static inline bool range_section_check(const struct range_section *section, const void *start, unsigned long size)
{
    return buff_range_check(section->start, section->size, start, size);
}

static inline void range_section_info_fill(struct range_section *section, range_section_base *base, void *start,
                                           unsigned long size, unsigned long long mem_mng)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
    (void)ATOMIC_SET(&(section->base.value), base->value);
    (void)ATOMIC_SET(&(section->start), start);
    (void)ATOMIC_SET(&(section->size), size);
    (void)ATOMIC_SET(&(section->mem_mng), mem_mng);
    (void)ATOMIC_SET(&(section->devid),
                     (mem_mng != 0) ? ((struct common_handle_t *)(uintptr_t)mem_mng)->devid : BUFF_INVALID_DEV);
#pragma GCC diagnostic pop
}

static bool range_section_add(struct range_section *section)
{
    do {
        if (atomic_ref_try_init(&section->status)) {
            return true;
        }
        if (atomic_ref_inc(&section->status)) {
            break;
        }
        atomic_ref_self_healing(&section->status);
    } while (true);

    return false;
}

static drvError_t add_buff_to_range(uint32_t block_id, range_section_base *base, void *start, unsigned long size,
                                    unsigned long long mem_mng)
{
    struct range_section_array *array = NULL;
    struct range_section *section = NULL;
    unsigned int row = 0, column = 0;

    if (range_get_index(block_id, &row, &column) != DRV_ERROR_NONE) {
        return DRV_ERROR_PARA_ERROR;
    }

    array = range_get_array(row);
    if (!range_section_array_get(array)) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    section = &array->cur_addr[column];
    range_section_info_fill(section, base, start, size, mem_mng);

    if (range_section_add(section)) {
        return DRV_ERROR_NONE;
    } else {
        // increment the reference count to the array only when the section is added
        return DRV_ERROR_REPEATED_INIT;
    }
}

drvError_t add_self_buff_to_range(uint32_t blk_id, int pool_id, void *start, unsigned long size,
                                  unsigned long long mem_mng)
{
    range_section_base base;

    base.info.owner = RANGE_OWNER_SELF;
    base.info.idlecnt = 0;
    base.info.pool_id = (uint32_t)pool_id & 0xFFFFFF;

    return add_buff_to_range(blk_id, &base, start, size, mem_mng);
}

static drvError_t add_others_buff_to_range(unsigned int block_id, int pool_id, void *start, unsigned long size,
                                           unsigned long long mem_mng)
{
    range_section_base base;

    base.info.owner = RANGE_OWNER_OTHERS;
    base.info.idlecnt = 0;
    base.info.pool_id = (unsigned int)pool_id & 0xFFFFFF;

    return add_buff_to_range(block_id, &base, start, size, mem_mng);
}

static inline bool range_section_refer(struct range_section *section)
{
    return atomic_ref_inc(&section->status);
}

static inline void range_section_unref(struct range_section *section)
{
    (void)atomic_ref_dec(&section->status);
}

STATIC struct range_section *range_section_get(unsigned int block_id, const void *start, unsigned long size,
                                               struct range_section_array **out_array)
{
    struct range_section_array *array = NULL;
    struct range_section *section = NULL;
    unsigned int row = 0, column = 0;

    if (range_get_index(block_id, &row, &column) != DRV_ERROR_NONE) {
        return NULL;
    }

    array = range_get_array(row);
    if (!range_section_array_is_valid(array)) {
        return NULL;
    }

    section = &array->cur_addr[column];
    if (!range_section_refer(section)) {
        return NULL;
    }

    if (!range_section_check(section, start, size)) {
        range_section_unref(section);
        buff_warn("Section range check not successful. (block_id=%u, start=%p; size=%lu; section->start=%p, "
                  "section->size=%lu)\n",
                  block_id, start, size, section->start, section->size);
        return NULL;
    }

    *out_array = array;
    return section;
}

static void range_section_put(struct range_section *section)
{
    range_section_unref(section);
}

drvError_t buff_range_owner_get(uint32_t blk_id, void *start, unsigned long size, int *owner)
{
    struct range_section_array *array = NULL;
    struct range_section *section = NULL;

    section = range_section_get(blk_id, start, size, &array);
    if (section == NULL) {
        buff_err("Range Section Get fail.\n");
        return DRV_ERROR_NO_RESOURCES;
    }
    *owner = section->base.info.owner;

    range_section_put(section);
    return DRV_ERROR_NONE;
}

drvError_t buff_range_get_ref(void *start, unsigned long size, uint32_t *refcnt, uint32_t blk_id)
{
    struct range_section_array *array = NULL;
    struct range_section *section = NULL;

    section = range_section_get(blk_id, start, size, &array);
    if (section == NULL) {
        buff_err("Range Section Get fail.\n");
        return DRV_ERROR_NO_RESOURCES;
    }
    *refcnt = atomic_value_get(&section->status);
    range_section_put(section);
    return DRV_ERROR_NONE;
}

drvError_t buff_range_mem_mng_get(uint32_t blk_id, void *start, unsigned long size, unsigned long long *mem_mng)
{
    struct range_section_array *array = NULL;
    struct range_section *section = NULL;

    section = range_section_get(blk_id, start, size, &array);
    if (section == NULL) {
        buff_err("Range Section Get fail.\n");
        return DRV_ERROR_NO_RESOURCES;
    }
    *mem_mng = section->mem_mng;
    range_section_put(section);

    return DRV_ERROR_NONE;
}

static drvError_t buff_range_section_get(unsigned int block_id, void *start, unsigned long size)
{
    struct range_section_array *array = NULL;
    struct range_section *section = NULL;

    section = range_section_get(block_id, start, size, &array);
    if (section == NULL) {
        return DRV_ERROR_NO_RESOURCES;
    }
    if (is_others_section(section)) {
        (void)range_section_refer(section);
        section->base.info.idlecnt = 0;
    }
    range_section_put(section);
    return DRV_ERROR_NONE;
}

drvError_t buff_range_get(uint32_t blk_id, void *start, unsigned long size)
{
    void *alloc_addr = NULL;
    unsigned long alloc_size;
    int pool_id;
    unsigned int new_id;
    drvError_t ret;
    if (buff_range_section_get(blk_id, start, size) == DRV_ERROR_NONE) {
        return DRV_ERROR_NONE;
    }

    ret = buff_blk_get(start, &pool_id, &alloc_addr, &alloc_size, &new_id);
    if (ret != DRV_ERROR_NONE) {
        buff_warn("Not alloc. (block_id=%u, size=%lu)\n", blk_id, size);
        return DRV_ERROR_NO_RESOURCES;
    }
    if (new_id != blk_id) {
        (void)buff_blk_put(pool_id, alloc_addr);
        buff_warn("Buff has been realloced. (block_id=%u, size=%lu)\n", blk_id, size);
        return DRV_ERROR_NO_RESOURCES;
    }
    if (!buff_range_check(alloc_addr, alloc_size, start, size)) {
        buff_warn("Invalid buff. (block_id=%u, alloc_size=%lu, size=%lu)\n", blk_id, alloc_size, size);
        buff_blk_put(pool_id, alloc_addr);
        return DRV_ERROR_NO_RESOURCES;
    }

    ret = add_others_buff_to_range(new_id, pool_id, alloc_addr, alloc_size, 0);
    if (ret == DRV_ERROR_NONE) {
        return DRV_ERROR_NONE;
    } else if (ret == DRV_ERROR_REPEATED_INIT) {
        /* the other thread has added */
        buff_blk_put(pool_id, alloc_addr);
        return DRV_ERROR_NONE;
    } else {
        buff_blk_put(pool_id, alloc_addr);
        buff_err("Add buff to range failed. (block_id=%u, size=%lu)\n", new_id, size);
        return ret;
    }
}

void buff_range_put(uint32_t blk_id, void *start)
{
    struct range_section_array *array = NULL;
    struct range_section *section = NULL;

    section = range_section_get(blk_id, start, 1UL, &array);
    if (section == NULL) {
        buff_warn("Put invalid blk_id, buff may have been released. (blk_id=%u; start=%p)\n", blk_id, start);
        return;
    }
    if (is_others_section(section)) {
        range_section_unref(section);
    }
    range_section_put(section);
}

void del_self_buff_from_range(uint32_t blk_id, void *start)
{
    struct range_section_array *array = NULL;
    struct range_section *section = NULL;

    section = range_section_get(blk_id, start, 1UL, &array);
    if (section == NULL) {
        buff_err("Put invalid buff. (blk_id=%u)\n", blk_id);
        return;
    }
    range_section_unref(section);
    range_section_put(section);
}

static bool is_idle_section(struct range_section *section)
{
    return (section->status.info.value == 0);
}

static bool idle_section_can_free(struct range_section *section, bool is_free_immediately)
{
    if (is_free_immediately) {
        return true;
    }

    if (!is_others_section(section)) {
        return true;
    }

    if (section->base.info.idlecnt > RANGE_IDLE_CNT_MAX) {
        return true;
    }
    section->base.info.idlecnt++;

    return false;
}

static inline bool range_section_try_del(struct range_section *section)
{
    return atomic_ref_try_uninit(&section->status);
}

static drvError_t _idle_buff_range_free(struct range_section *section, bool is_free_immediately)
{
    void *mem_mng = NULL;
    void *start = NULL;
    int pool_id;

    if (range_section_is_valid(section) == false) {
        return DRV_ERROR_NONE;
    }
    if (is_idle_section(section) == false) {
        return DRV_ERROR_BUSY;
    }
    if (idle_section_can_free(section, is_free_immediately) == false) {
        return DRV_ERROR_TRY_AGAIN;
    }

    pool_id = (int)section->base.info.pool_id;
    start = section->start;
    mem_mng = (void *)(uintptr_t)section->mem_mng;
    if (!range_section_try_del(section)) {
        return DRV_ERROR_STATUS_FAIL;
    }
    section->mem_mng = 0;
    buff_blk_put(pool_id, start);
    if (mem_mng != NULL) {
        /* section->mem_mng can't set 0. Because the section may be reused after range_section_try_del in other thread.
         */
        free(mem_mng);
    }
    return DRV_ERROR_NONE;
}

drvError_t idle_buff_range_free(uint32_t devid, uint32_t using_buff_max_show_cnt, bool only_free_cache_pool)
{
    unsigned int using_buff_show_cnt = 0;
    struct range_section *section = NULL;
    unsigned int row;
    drvError_t ret = DRV_ERROR_NONE;
    drvError_t tmp_ret;

    for (row = 0; row < BUFF_RANGE_MAX_ROWS_NUM; row++) {
        struct range_section_array *array = range_get_array(row);
        unsigned int column;

        if (array->cur_addr == NULL) {
            continue;
        }

        for (column = 0; column < BUFF_RANGE_MAX_COLUMNS_NUM; column++) {
            section = &array->cur_addr[column];
            if ((devid != BUFF_INVALID_DEV) && (devid != section->devid)) {
                continue;
            }

            if (only_free_cache_pool == true) {
                if ((int)section->base.info.pool_id == buff_get_svm_private_pool_id()) {
                    continue;
                }
            }

            tmp_ret = _idle_buff_range_free(section, true);
            if ((using_buff_show_cnt < using_buff_max_show_cnt) && (tmp_ret != DRV_ERROR_NONE)) {
                buff_debug("Buff is in use. (start=0x%lx; size=%lu; ret=%u)\n", (uint64_t)(uintptr_t)section->start,
                           section->size, tmp_ret);
                using_buff_show_cnt++;
                ret = DRV_ERROR_BUSY;
            }
        }
    }
    return ret;
}

/* Large buff cannot be released in the recycling thread because the recycling
 * thread releases the memory slowly. If the memory is not released in time,
 * The OOM occurs.
 */
void idle_buff_range_free_ahead(uint32_t blk_id)
{
    struct range_section_array *array = NULL;
    struct range_section *section = NULL;
    unsigned int row = 0, column = 0;

    if (range_get_index(blk_id, &row, &column) != DRV_ERROR_NONE) {
        return;
    }

    array = range_get_array(row);
    if (!range_section_array_is_valid(array)) {
        return;
    }

    section = &array->cur_addr[column];
    if (is_self_section(section)) {
        (void)_idle_buff_range_free(section, false);
    } else if (section->size > MZ_DEFAULT_CFG_MAX_SIZE) {
#ifndef EMU_ST
        (void)_idle_buff_range_free(section, true);
#endif
    }
}

#ifdef EMU_ST
void del_others_range(void)
{
    unsigned int row;

    for (row = 0; row < BUFF_RANGE_MAX_ROWS_NUM; row++) {
        struct range_section_array *array = range_get_array(row);
        struct range_section *addr = NULL;
        unsigned int column;
        if (array->cur_addr == NULL) {
            continue;
        }

        addr = array->cur_addr;
        for (column = 0; column < BUFF_RANGE_MAX_COLUMNS_NUM; column++) {
            struct range_section *section = &array->cur_addr[column];

            if (!range_section_is_valid(section)) {
                continue;
            }

            if (section->mem_mng != 0) {
                free((void *)section->mem_mng);
                section->mem_mng = 0;
            }
        }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
        ATOMIC_SET(&array->cur_addr, NULL);
#pragma GCC diagnostic pop
        free(addr);
    }
}
#endif
void buff_range_show(void)
{
    unsigned int row;
    int num = 0;

    for (row = 0; row < BUFF_RANGE_MAX_ROWS_NUM; row++) {
        struct range_section_array *array = range_get_array(row);
        unsigned int column;

        if (!range_section_array_is_valid(array)) {
            continue;
        }
        for (column = 0; column < BUFF_RANGE_MAX_COLUMNS_NUM; column++) {
            struct range_section *section = &array->cur_addr[column];
            if (!range_section_is_valid(section)) {
                continue;
            }
            buff_show("Buff range show. (id=%d, owner=%u, refcnt=%u, size=%lx)\n", num, section->base.info.owner,
                      section->status.info.value, section->size);
            num++;
        }
    }
    buff_show("Buff range show. (total_num=%d, owner_self=%d, others=%d)\n", num, RANGE_OWNER_SELF, RANGE_OWNER_OTHERS);
}

#ifdef EMU_ST
uint32_t buff_range_get_valid_section_num(void)
{
    uint32_t num = 0;
    uint32_t row;

    for (row = 0; row < BUFF_RANGE_MAX_ROWS_NUM; row++) {
        struct range_section_array *array = range_get_array(row);
        uint32_t column;

        if (!range_section_array_is_valid(array)) {
            continue;
        }
        for (column = 0; column < BUFF_RANGE_MAX_COLUMNS_NUM; column++) {
            struct range_section *section = &array->cur_addr[column];
            if (range_section_is_valid(section)) {
                num++;
            }
        }
    }
    return num;
}
#endif

static drvError_t buff_get_blk_id_from_buf(void *buf, uint32_t *blk_id)
{
    unsigned long size;
    drvError_t ret;
    int pool_id;
    void *addr;

    ret = buff_blk_get(buf, &pool_id, &addr, &size, blk_id);
    if (ret != DRV_ERROR_NONE) {
        buff_err("buff_blk_get fail. (ret=%d; buf=%p)\n", ret, buf);
        return ret;
    }
    buff_blk_put(pool_id, addr);
    return DRV_ERROR_NONE;
}

drvError_t halBuffGet(Mbuf *mbuf, void *buf, unsigned long size)
{
    uint32_t blk_id;

    /* buf may not be valid, Is_buff_addr is used for blocking, and no error print */
    if (is_buff_addr((uintptr_t)buf) == false) {
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf == NULL) {
        drvError_t ret = buff_get_blk_id_from_buf(buf, &blk_id);
        if (ret != DRV_ERROR_NONE) {
            buff_err("Get blk from buf fail. (ret=%d; buf=%p)\n", ret, buf);
            return ret;
        }
    } else {
        blk_id = mbuf->data_blk_id;
    }
    return buff_range_get(blk_id, buf, size);
}

void halBuffPut(Mbuf *mbuf, void *buf)
{
    uint32_t blk_id;

    if (mbuf == NULL) {
        drvError_t ret = buff_get_blk_id_from_buf(buf, &blk_id);
        if (ret != DRV_ERROR_NONE) {
            buff_err("Get blk from buf fail. (ret=%d)\n", ret);
            return;
        }
    } else {
        blk_id = mbuf->data_blk_id;
    }
    buff_range_put(blk_id, buf);
}

#ifndef EMU_ST
int halBuffPoolGet(void *poolStart)
{
    (void)poolStart;
    return (int)DRV_ERROR_NOT_SUPPORT;
}

int halBuffPoolPut(void *poolStart)
{
    (void)poolStart;
    return (int)DRV_ERROR_NOT_SUPPORT;
}
#endif

drvError_t buff_device_open(uint32_t devid, halDevOpenIn *in, halDevOpenOut *out)
{
    (void)in;
    (void)out;
    buff_info("buff open finish. (devid = %u)\n", devid);
    return DRV_ERROR_NONE;
}

drvError_t buff_device_close(uint32_t devid, halDevCloseIn *in)
{
    (void)in;
    buff_info("buff close finish. (devid = %u)\n", devid);
    return DRV_ERROR_NONE;
}