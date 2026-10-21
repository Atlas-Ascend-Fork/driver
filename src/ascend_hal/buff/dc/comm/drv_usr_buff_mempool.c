/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>
#include <unistd.h>

#include "drv_buff_common_mempool.h"
#include "drv_buff_unibuff.h"
#include "drv_buff_common.h"
#include "drv_buff_adp.h"
#include "buff_event.h"
#include "buff_mempool_adapt.h"
#include "buff_user_interface.h"
#include "drv_usr_buff_mempool.h"

#define ATOMIC_SET(x, y) __sync_lock_test_and_set((x), (y))
#define CAS(ptr, oldval, newval) __sync_bool_compare_and_swap(ptr, oldval, newval)

#define MBUF_MP_INITED 1
#define MBUF_MP_TOTAL_SIZE 1677721 /* 1.6M */

static pthread_rwlock_t g_mbuf_mp_list_rwlock = PTHREAD_RWLOCK_INITIALIZER;
struct list_head THREAD g_mbuf_mp_list = {0};
static unsigned int THREAD g_mbuf_mp_cnt = 0;
static struct mempool_t THREAD *g_latest_mbuf_mp = NULL;

static void mp_mng_list_del(struct mempool_t *mp);

void *mp_get_buff_uva_by_index(struct mempool_t *mp, unsigned long index)
{
    uint64 *uva_list = (uint64 *)mp_get_valist_start_addr(mp, mp->bit_num);
    return (void *)(uintptr_t)uva_list[index];
}

static drvError_t mp_create_mbuf_mp(uint32_t devid, struct mempool_t **mp)
{
    drvError_t ret = DRV_ERROR_NONE;
    mpAttr attr;

    attr.devid = (int)devid;
    attr.mGroupId = buff_get_default_pool_id();
    attr.blkSize = (unsigned int)sizeof(struct share_mbuf);
    attr.blkNum = MBUF_MP_TOTAL_SIZE / attr.blkSize;
    attr.align = UNI_ALIGN_MIN;
    attr.hugePageFlag = BUFF_SP_HUGEPAGE_PRIOR;
    (void)strcpy_s(attr.poolName, BUFF_POOL_NAME_LEN, "mbuf_head");

    ret = mp_create(&attr, MEMPOOL_MBUF_LIST, mp);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Create mbuf mp failed. (ret=%d; grp_id=%u; blk size=%u; blk num=%u; align=%u; flag=%lu; devid=%u)\n",
                 ret, attr.mGroupId, attr.blkSize, attr.blkNum, attr.align, attr.hugePageFlag, devid);
        *mp = NULL;
    }

    return ret;
}

static bool mp_can_destroy_mbuf_mp(struct mempool_t *mp)
{
    unsigned int blk_available;

    blk_available = buff_api_atomic_read(&mp->blk_available);
    if (mp->blk_num != blk_available) {
        return false;
    }
    return true;
}

void mp_destroy_mbuf_mp(struct mempool_t *mp)
{
    if (mp_can_destroy_mbuf_mp(mp) == false) {
        return;
    }

    mp_mng_list_del(mp);
}

#ifdef EMU_ST
void mp_destroy_all_mbuf_mp(void)
{
    struct buff_req_mp_destroy arg;
    struct list_head *pos = NULL;
    struct mempool_t *mp = NULL;
    struct list_head *n = NULL;

    /* g_mbuf_mp_list not inited */
    if (g_mbuf_mp_list.next == NULL) {
        return;
    }

    (void)pthread_rwlock_wrlock(&g_mbuf_mp_list_rwlock);
    list_for_each_safe(pos, n, &g_mbuf_mp_list)
    {
        mp = list_entry(pos, struct mempool_t, user_list);
        if (mp == g_latest_mbuf_mp) {
            (void)ATOMIC_SET(&g_latest_mbuf_mp, NULL);
        }
        drv_user_list_del(&mp->user_list);
        g_mbuf_mp_cnt--;
        arg.mp = (uint64)mp;
        ;
        (void)buff_usr_mp_delete(&arg);
    }
    (void)pthread_rwlock_unlock(&g_mbuf_mp_list_rwlock);
}
#endif

STATIC int THREAD g_mbuf_mp_init_status[BUFF_MAX_DEV] = {0};
STATIC void mp_pre_create_mbuf_mp(uint32_t devid)
{
    static pthread_mutex_t g_mbuf_mp_init_mutex = PTHREAD_MUTEX_INITIALIZER;
    struct mempool_t *mp = NULL;

    if (g_mbuf_mp_init_status[devid] == MBUF_MP_INITED) {
        return;
    }

    (void)pthread_mutex_lock(&g_mbuf_mp_init_mutex);
    if (g_mbuf_mp_init_status[devid] == MBUF_MP_INITED) {
        (void)pthread_mutex_unlock(&g_mbuf_mp_init_mutex);
        return;
    }

    if (g_mbuf_mp_list.next == NULL) {
        INIT_LIST_HEAD(&g_mbuf_mp_list);
    }

    if (mp_create_mbuf_mp(devid, &mp) != 0) {
        buff_err("pre create mbuf mp failed\n");
    }

    g_mbuf_mp_init_status[devid] = MBUF_MP_INITED;
    (void)pthread_mutex_unlock(&g_mbuf_mp_init_mutex);
}

static drvError_t mp_arg_check(mpAttr *attr, struct mempool_t **mp)
{
    (void)mp;
    if ((attr->hugePageFlag & (unsigned long)(~(BUFF_SP_HUGEPAGE_PRIOR | BUFF_SP_HUGEPAGE_ONLY | BUFF_SP_DVPP))) != 0) {
        buff_err("invalid huge_page_flag:%lu\n", attr->hugePageFlag);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!buff_check_align(attr->align)) {
        buff_err("invalid align:%u\n", attr->align);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((attr->blkNum == 0) || (attr->blkSize == 0) || (attr->blkSize >= MP_BLK_SIZE_MAX)) {
        buff_err("error para, blknum:0x%x, blksize:0x%x\n", attr->blkNum, attr->blkSize);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

drvError_t mp_create(mpAttr *attr, uint32 type, struct mempool_t **mp)
{
    struct buff_req_mp_create arg;
    drvError_t ret;

    ret = mp_arg_check(attr, mp);
    if (ret != DRV_ERROR_NONE) {
        buff_err("mp arg check fail:%d\n", ret);
        return ret;
    }

    arg.devid = attr->devid;
    arg.groupid = buff_get_default_pool_id();
    arg.obj_num = attr->blkNum;
    arg.obj_size = attr->blkSize;
    arg.align = attr->align;
    arg.sp_flag = attr->hugePageFlag;
    arg.type = type;
    arg.mp_uva = 0;
    ret = (drvError_t)memcpy_s(arg.pool_name, BUFF_POOL_NAME_LEN, attr->poolName, BUFF_POOL_NAME_LEN);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Memcpy_s error. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    ret = buff_usr_mp_create(&arg);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Create mp fail. (ret=0x%x; blk size=%u; blk num=%u; pid=%d; devid=%u)\n", ret, attr->blkSize,
                 attr->blkNum, buff_get_current_pid(), attr->devid);
        return ret;
    }

    *mp = (struct mempool_t *)(uintptr_t)(arg.mp_uva);
    buff_scale_out(arg.start, arg.total_size);

    buff_event("Create mp. (mp=%p; ret=0x%x; blk size=%u; blk num=%u; pid=%d; devid=%u)\n", *mp, ret, attr->blkSize,
               attr->blkNum, buff_get_current_pid(), attr->devid);

    return ret;
}

int halBuffCreatePool(mpAttr *attr, struct mempool_t **mp)
{
    if ((attr == NULL) || (mp == NULL)) {
        buff_err("Mp create para invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((attr->devid >= BUFF_MAX_DEV) || (attr->devid < BUFF_MIN_DEV)) {
        buff_err("Invalid devid:%d\n", attr->devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    mp_pre_create_mbuf_mp((uint32_t)attr->devid);

    return (int)mp_create(attr, MEMPOOL_LIST, mp);
}

int halBuffDeletePool(struct mempool_t *mp)
{
    drvError_t ret = DRV_ERROR_NONE;
    struct mempool_t *pool = mp;
    struct buff_req_mp_destroy arg;
    unsigned int blk_available;

    if (mp == NULL) {
        buff_err("Mp handle is invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_mutex_lock(&mp->mutex);
    blk_available = buff_api_atomic_read(&mp->blk_available);
    if (mp->blk_num != blk_available) {
        (void)pthread_mutex_unlock(&mp->mutex);
        buff_err("Mp is in use. (name=%s; blk_num=%u; blk_available=%u; start=0x%llx; size=0x%llx; devid=%u)\n",
                 mp->pool_name, mp->blk_num, blk_available, (unsigned long long)mp->blk_start, mp->blk_total_len,
                 mp->head.devid);
        return DRV_ERROR_BUSY;
    }
    pool->head.status = MP_S_DESTROYED;
    (void)pthread_mutex_unlock(&mp->mutex);

    buff_scale_in(mp->blk_start, mp->blk_total_len);

    arg.mp = (uint64)(uintptr_t)mp;
    ret = buff_usr_mp_delete(&arg);
    if (ret != DRV_ERROR_NONE) {
        buff_err("destroy pool %p, ret(%d)\n", mp, ret);
    } else {
        buff_event("destroy pool %p ok.\n", mp);
    }

    return (int)ret;
}

static void mp_trace_print(struct mempool_t *mp)
{
    void *uni_buff = NULL;
    unsigned int idx;

    buff_event("=====mp trace start print=====\n");
    buff_event("mp=%p, blk_addr=%p, blk_num=%d, blk_avai=%d, bitmap=0x%lx, scenes=%d\n", mp, mp->blk_start, mp->blk_num,
               mp->blk_available, mp->bitmap[0], mp->stat.alloc_fail_scence);

    for (idx = 0; idx < mp->blk_num; idx++) {
        uni_buff = mp_get_buff_uva_by_index(mp, idx);
        buff_trace_print(uni_buff, mp);
        if (idx >= MAX_MP_PRINT_NUM) {
            break;
        }
    }
    buff_event("=====mp trace end print=====\n");
}

static int mp_alloc_bitmap(struct mempool_t *mp, unsigned long start, unsigned long end)
{
    unsigned long idx;
    do {
        idx = bitmap_find_next_zero_area(mp->bitmap, end, start, 1, 0);
        if (idx < end) {
            unsigned long offset = idx & (BITS_PER_LONG - 1);
            if (bitmap_set_rtn_atomic(&mp->bitmap[idx / BITS_PER_LONG], offset) == 0) {
                return (int)idx;
            }
        }
    } while (idx < end);

    return MP_BITMAP_INDEX_INVALID;
}

static drvError_t _mp_alloc_buff(struct mempool_t *mp, void **buff, uint32_t *blk_id)
{
    int index;
    unsigned int max_use_num;
    unsigned long end;
    unsigned long curr;
    void *uni_buff = NULL;
    struct uni_buff_head_t *head = NULL;
    struct uni_buff_tail_t *tail = NULL;
    struct uni_buff_ext_info *ext = NULL;

    if ((buff == NULL) || (mp == NULL)) {
        buff_err("invalid buff:0x%lx, or mp:0x%lx\n", (uintptr_t)buff, (uintptr_t)mp);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mp->head.status != MP_S_NORMAL) {
        buff_err("invalid mp status. %d\n", mp->head.status);
        return DRV_ERROR_STATUS_FAIL;
    }

    if (buff_api_atomic_read(&mp->blk_available) == 0) {
        mp->stat.alloc_fail_scence = MP_ALLOC_NO_BLK;
        return DRV_ERROR_NO_RESOURCES;
    }

    curr = mp->curr_index;
    end = mp->blk_num;

    index = mp_alloc_bitmap(mp, curr, end);
    if (index == MP_BITMAP_INDEX_INVALID) {
        index = mp_alloc_bitmap(mp, 0, curr);
        if (index == MP_BITMAP_INDEX_INVALID) {
            mp->stat.alloc_fail_scence = MP_ALLOC_NO_BITMAP;
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    mp->stat.alloc_fail_scence = 0;
#ifdef EMU_ST
    mp->stat.buff_time_stat_enable = 1;
#endif
    buff_api_atomic_dec(&mp->blk_available);

    mp->curr_index = (unsigned int)index;

    uni_buff = mp_get_buff_uva_by_index(mp, (unsigned int)index);
    if (uni_buff == NULL) {
        buff_err("buff is null! mp:0x%lx, index:%d\n", (uintptr_t)mp, index);
        goto alloc_mp_fail;
    }

    /* check buff valid */
    head = buff_mempool_get_head(uni_buff);
    if ((head->index != (uint32)index) || (head->image != UNI_HEAD_IMAGE)) {
        buff_err("Index is invalid. (mp=0x%lx; head_index=%u; index=%d; image=%#x)\n", (uintptr_t)mp, head->index,
                 index,
                 head->image); // lint !e507
        goto alloc_mp_fail;
    }

    tail = buff_mempool_get_tail(uni_buff, mp->blk_size, mp->align_size);
    if (tail->image != UNI_TAIL_IMAGE) {
        buff_err("Image check failed. (tail_image=0x%x)\n", tail->image);
        goto alloc_mp_fail;
    }

    head->timestamp = (unsigned int)buff_api_timestamp();
    head->status = UNI_STATUS_ALLOC;
    head->ref = 1;
    head->buff_type = BUFF_NORMAL;
    head->align_flag = 0;
    head->recycle_flag = 1;

    if (mp->stat.buff_time_stat_enable == 1) {
        max_use_num = mp->blk_num - mp->blk_available;
        if (mp->stat.blk_max_use_num < max_use_num) {
            mp->stat.blk_max_use_num = max_use_num;
        }
    }

    ext = (struct uni_buff_ext_info *)((char *)head - sizeof(struct uni_buff_ext_info));
    ext->alloc_pid = buff_api_getpid();
    ext->use_pid = ext->alloc_pid;
    ext->process_uni_id = buff_get_process_uni_id();

    *buff = uni_buff;
    *blk_id = mp->blk_id;
    return DRV_ERROR_NONE;

alloc_mp_fail:
    /* pool abnormal, blk_available inc for pool deltete, bit map still allocated for abnormal bit */
    buff_api_atomic_inc(&mp->blk_available);
    mp->head.status = MP_S_ABNORMAL;
    return DRV_ERROR_INVALID_VALUE;
}

static void mp_trace_no_resources(struct mempool_t *mp)
{
    uint64 cur_time_stamp;

    cur_time_stamp = buff_get_cur_timestamp();
    if ((mp->trace.last_alloc_fail_timestamp != 0) &&
        (cur_time_stamp - mp->trace.last_alloc_fail_timestamp < MP_TRACE_PRINT_INTERVAL)) {
        return;
    }

    if (!CAS(&mp->trace.print_flag, 0, 1)) {
        return;
    }

    mp->trace.last_alloc_fail_timestamp = cur_time_stamp;
    mp_trace_print(mp);
    ATOMIC_SET(&mp->trace.print_flag, 0);
}

static void mp_trace_strategy(struct mempool_t *mp, int result)
{
    mp->stat.fail_cnt++;
    if ((mp->type == MEMPOOL_LIST) && (result == DRV_ERROR_NO_RESOURCES)) {
        mp_trace_no_resources(mp);
    }
}

drvError_t mp_alloc_buff(struct mempool_t *mp, void **buff, uint32_t *blk_id)
{
    return _mp_alloc_buff(mp, buff, blk_id);
}

static inline void mp_atomic_inc(struct mempool_t *mp)
{
    buff_api_atomic_inc(&mp->head.ref);
}

static inline void mp_atomic_dec(struct mempool_t *mp)
{
    buff_api_atomic_dec(&mp->head.ref);
}

static inline uint32_t mp_atomic_read(struct mempool_t *mp)
{
    return buff_api_atomic_read(&mp->head.ref);
}

static struct mempool_t *mp_mng_get(struct mempool_t *mp, unsigned int cnt)
{
    struct list_head *head = &g_mbuf_mp_list;
    struct list_head *tmp = NULL;
    struct mempool_t *mp_out = NULL;

    (void)pthread_rwlock_rdlock(&g_mbuf_mp_list_rwlock);
    if (mp == NULL) {
        tmp = (g_latest_mbuf_mp != NULL) ? &g_latest_mbuf_mp->user_list : head->next;
    } else {
        tmp = mp->user_list.next;
    }
    if ((tmp == head) || (tmp == NULL)) {
        if (cnt >= g_mbuf_mp_cnt) {
            (void)pthread_rwlock_unlock(&g_mbuf_mp_list_rwlock);
            return NULL;
        }
        tmp = head->next;
    }
    mp_out = list_entry(tmp, struct mempool_t, user_list);

    mp_atomic_inc(mp_out);
    (void)pthread_rwlock_unlock(&g_mbuf_mp_list_rwlock);

    return mp_out;
}

void mp_mng_put(struct mempool_t *mp)
{
    if (mp->type != MEMPOOL_MBUF_LIST) {
        return;
    }

    (void)pthread_rwlock_rdlock(&g_mbuf_mp_list_rwlock);
    mp_atomic_dec(mp);
    (void)pthread_rwlock_unlock(&g_mbuf_mp_list_rwlock);
}

void mp_mng_list_add(struct mempool_t *mp)
{
    if (mp->type != MEMPOOL_MBUF_LIST) {
        return;
    }

    (void)pthread_rwlock_wrlock(&g_mbuf_mp_list_rwlock);
    drv_user_list_add_head(&mp->user_list, &g_mbuf_mp_list);
    g_mbuf_mp_cnt++;
    mp_atomic_inc(mp);
    (void)pthread_rwlock_unlock(&g_mbuf_mp_list_rwlock);
}

static void mp_mng_list_del(struct mempool_t *mp)
{
    uint32_t delete_flag = 0;
    int ret;

    (void)pthread_rwlock_wrlock(&g_mbuf_mp_list_rwlock);
    if (mp_atomic_read(mp) == 1) {
        if (mp == g_latest_mbuf_mp) {
            (void)ATOMIC_SET(&g_latest_mbuf_mp, NULL);
        }
        drv_user_list_del(&mp->user_list);
        g_mbuf_mp_cnt--;
        mp_atomic_dec(mp);
        delete_flag = 1;
    }
    (void)pthread_rwlock_unlock(&g_mbuf_mp_list_rwlock);

    if (delete_flag == 1) {
        ret = halBuffDeletePool(mp);
        if (ret != 0) {
            buff_err("Mbuf mp delete failed. (ret=%d)\n", ret);
        }
    }
}

static drvError_t mp_mbuf_list_alloc(uint32_t devid, void **buff, uint32_t *blk_id)
{
    struct mempool_t *mp = NULL;
    struct mempool_t *next_mp = NULL;
    drvError_t ret = DRV_ERROR_OUT_OF_MEMORY;
    unsigned int cnt = 0;

    while (ret != DRV_ERROR_NONE) {
        next_mp = mp_mng_get(mp, cnt);
        if (mp != NULL) {
            mp_mng_put(mp);
        }
        if (next_mp == NULL) {
            return ret;
        }
        cnt++;
        if (next_mp->head.devid != devid) {
            mp = next_mp;
            continue;
        }
        ret = mp_alloc_buff(next_mp, buff, blk_id);
        if (ret != DRV_ERROR_NONE) {
            mp = next_mp;
            continue;
        }
    }
    /* do not need lock here, if the value is incorrect, only the performance maybe affected. */
    (void)ATOMIC_SET(&g_latest_mbuf_mp, next_mp);
    return DRV_ERROR_NONE;
}

drvError_t mp_alloc_mbuf_head(uint32_t devid, void **buff, uint32_t *blk_id)
{
    struct mempool_t *mp = NULL;
    drvError_t ret, ret_tmp;

    if (devid >= BUFF_MAX_DEV) {
        buff_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_DEVICE;
    }
    mp_pre_create_mbuf_mp(devid);

    do {
        ret = mp_mbuf_list_alloc(devid, buff, blk_id);
        if (ret != DRV_ERROR_NONE) {
            ret_tmp = mp_create_mbuf_mp(devid, &mp);
            if (ret_tmp != DRV_ERROR_NONE) {
                buff_err("Mbuf mp create failed. (ret=%u)\n", ret_tmp);
                return ret_tmp;
            }
        }
    } while (ret != DRV_ERROR_NONE);
    return ret;
}

int halBuffAllocByPool(poolHandle pHandle, void **buff)
{
    uint32_t blk_id;
    int ret;

    if (pHandle == NULL) {
        buff_err("invalid p_handle:0x%lx\n", (uintptr_t)pHandle);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (buff == NULL) {
        buff_err("invalid buff:0x%lx\n", (uintptr_t)buff);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = (int)mp_alloc_buff(pHandle, buff, &blk_id);
    if (ret != 0) {
        mp_trace_strategy(pHandle, ret);
    }
    return ret;
}

drvError_t halMbufGetDqsHandle(Mbuf *mbuf, uint64_t *handle)
{
    (void)mbuf;
    (void)handle;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halMbufDqsTracePrint(poolHandle pHandle)
{
    (void)pHandle;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halBuffGetDQSPoolInfo(struct mempool_t *mp, DqsPoolInfo *poolInfo)
{
    (void)mp;
    (void)poolInfo;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halBuffGetDQSPoolInfoById(unsigned int poolId, DqsPoolInfo *poolInfo)
{
    (void)poolId;
    (void)poolInfo;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t halMbufGetDQSPoolInfo(Mbuf *mbuf, DqsPoolInfo *poolInfo)
{
    (void)mbuf;
    (void)poolInfo;
    return (int)DRV_ERROR_NOT_SUPPORT;
}