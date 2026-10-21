/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef BUFF_MANAGE_KERNEL_UT
#include "drv_buff_unibuff.h"
#include "drv_buff_mbuf.h"
#include "drv_buff_memzone.h"
#include "drv_buff_common_mempool.h"
#include "buff_mng.h"
#include "buff_recycle.h"
#include "buff_range.h"

#define ATOMIC_SET(x, y) __sync_lock_test_and_set((x), (y))
#define CAS(ptr, oldval, newval) __sync_bool_compare_and_swap(ptr, oldval, newval)

void buff_show_info(void *buff, uint32_t blk_id)
{
    struct uni_buff_head_t *head = NULL;
    struct uni_buff_tail_t *tail = NULL;
    struct uni_buff_ext_info *ext_info = NULL;

    head = buff_get_head(buff, blk_id);
    if (head == NULL) {
        buff_err("head is NULL\n");
        return;
    }
    tail = buff_get_tail(head, blk_id);
    if (tail == NULL) {
        buff_err("uni_tail is NULL\n");
        buff_put_head(head, blk_id);
        return;
    }

    buff_err("Head. (head=%pK; image=0x%x; timestamp=%u; ref=%u; status=%d; buff_type=%d; "
             "mbuf_data_flag=%d; recycle_flag=%d; ext_flag=%d; resv_head=%d; align_flag=%d; resv=%d; index=%u; "
             "size:0x%lx)\n",
             head, head->image, head->timestamp, head->ref, head->status, head->buff_type, head->mbuf_data_flag,
             head->recycle_flag, head->ext_flag, head->resv_head, head->align_flag, head->resv, head->index,
             (unsigned long)head->size);
    buff_err("Tail. (tail=%pK; image=0x%x; size=0x%x)\n", tail, tail->image, tail->size);
    buff_put_tail(tail, blk_id);

    ext_info = buff_get_ext_info(head, blk_id);
    if (ext_info != NULL) {
        buff_err("Ext. (ext=%pK; alloc_pid=%d; use_pid:%d)\n", ext_info, ext_info->alloc_pid, ext_info->use_pid);
        buff_put_ext_info(ext_info, blk_id);
    }
    buff_put_head(head, blk_id);

    return;
}

#ifndef EMU_ST
static int buff_trace_id_alloc(struct uni_buff_trace_t *buff_trace, struct share_mbuf *s_mbuf)
{
    int i;
    for (i = 0; i < BUFF_REF_MAX_MBUF_NUM; i++) {
        if (CAS(&buff_trace->mbuf[i], NULL, s_mbuf) == 1) {
            return i;
        }
    }
    return -1;
}
#endif

void buff_trace(void *buff, struct share_mbuf *s_mbuf, int pid, int opt_type, int qid)
{
#ifndef EMU_ST
    struct uni_buff_head_t *mbuf_head = NULL;
    struct uni_buff_trace_t *buff_trace = NULL;
    drvError_t ret;
    int id;

    mbuf_head = buff_mempool_get_head((void *)s_mbuf);
    if (mbuf_head->buff_type != MBUF_BY_POOL) {
        return;
    }

    buff_trace = buff_mempool_get_trace(buff);
    ret = buff_range_get(s_mbuf->data_blk_id, buff_trace, sizeof(struct uni_buff_trace_t));
    if (ret != DRV_ERROR_NONE) {
        buff_warn("Trace is illegal. (pid=%d; opt_type=%d; qid=%u; ret=%d)\n", pid, opt_type, qid, ret);
        return;
    }

    if ((opt_type == MBUF_ALLOC_BY_POOL) || (opt_type == MBUF_ALLOC_BY_COPYREF)) {
        id = buff_trace_id_alloc(buff_trace, s_mbuf);
        s_mbuf->buff_trace_id = (short)id;
        if (id != -1) {
            buff_trace->alloc_uid[id] = (uint32)buff_get_process_uni_id();
            buff_trace->proc_uid[id] = (uint32)buff_get_process_uni_id();
            buff_trace->mbuf_info[id].pid = (unsigned int)pid & 0xFFFF;
            buff_trace->mbuf_info[id].opt_type = (unsigned int)opt_type & 0xF;
            buff_trace->mbuf_info[id].qid = 0xFFF;
        }
    } else if ((opt_type == MBUF_ENQUEUE) || (opt_type == MBUF_DEQUEUE)) {
        id = s_mbuf->buff_trace_id;
        if ((id >= 0) && (id < BUFF_REF_MAX_MBUF_NUM)) {
            buff_trace->proc_uid[id] = (uint32)buff_get_process_uni_id();
            buff_trace->mbuf_info[id].pid = (unsigned int)pid & 0xFFFF;
            buff_trace->mbuf_info[id].opt_type = (unsigned int)opt_type & 0xF;
            buff_trace->mbuf_info[id].qid = (unsigned int)qid & 0xFFF;
        }
    } else if ((opt_type >= MBUF_FREE_BEGIN) && (opt_type <= MBUF_FREE_END)) {
        id = s_mbuf->buff_trace_id;
        if ((id >= 0) && (id < BUFF_REF_MAX_MBUF_NUM)) {
            (void)ATOMIC_SET(&(buff_trace->mbuf[id]), NULL);
        }
    }

    buff_range_put(s_mbuf->data_blk_id, buff_trace);
    return;
#endif
}

void buff_trace_print(void *buff, struct mempool_t *mp)
{
#ifndef EMU_ST
    struct uni_buff_trace_t *buff_trace = NULL;
    struct uni_buff_head_t *head = NULL;
    int i;

    if (buff == NULL) {
        return;
    }

    head = buff_mempool_get_head(buff);
    if (head == NULL) {
        return;
    }

    buff_trace = buff_mempool_get_trace(buff);
    buff_event("mp=%p, idx=%d, buff=%p, ref=%d\n", mp, head->index, buff, head->ref);
    for (i = 0; i < BUFF_REF_MAX_MBUF_NUM; i++) {
        if (buff_trace->mbuf[i] != NULL) {
            buff_event("buff=%p, mbuf=%p, pid=%u, opt_type=%u, qid=%d\n", buff, buff_trace->mbuf[i],
                       buff_trace->mbuf_info[i].pid, buff_trace->mbuf_info[i].opt_type, buff_trace->mbuf_info[i].qid);
        }
    }
#endif
}

struct uni_buff_head_t *buff_get_head(void *buff, uint32_t blk_id)
{
    struct uni_buff_head_t *head = NULL;
    uint32 offset;
    drvError_t ret;

    if (buff == NULL) {
        buff_err("buff is NULL.\n");
        return NULL;
    }

    /* buff - head is verified by Mbuf_get */
    head = (struct uni_buff_head_t *)((char *)buff - sizeof(struct uni_buff_head_t));
    ret = buff_range_get(blk_id, (void *)head, sizeof(*head));
    if (ret != DRV_ERROR_NONE) {
        buff_err("Get buf range fail. (ret=%d; blk_id=%u)\n", ret, blk_id);
        return NULL;
    }
    offset = head->offset;

    if (head->align_flag == UNI_ALIGN_FLAG_ENABLE) {
        if ((head->image == UNI_HEAD_IMAGE) && (offset <= (UNI_ALIGN_MAX - UNI_ALIGN_MIN))) {
            buff_range_put(blk_id, head);
            head = (struct uni_buff_head_t *)((char *)head - offset);
            ret = buff_range_get(blk_id, (void *)head, sizeof(*head));
            if (ret != DRV_ERROR_NONE) {
                return NULL;
            }
        } else {
            buff_err("align head is broken, image:0x%x, offset:0x%x\n", head->image, offset);
            buff_range_put(blk_id, head);
            return NULL;
        }
    }

    return head;
}

drvError_t buff_verify_and_get_head(void *buff, struct uni_buff_head_t **uni_head, uint32_t blk_id)
{
    struct uni_buff_head_t *head = NULL;
    struct uni_buff_tail_t *tail = NULL;

    if (buff != NULL) {
        head = buff_get_head(buff, blk_id);
        if (head == NULL) {
            buff_err("head is NULL\n");
            return DRV_ERROR_INVALID_VALUE;
        }

        if (head->image != UNI_HEAD_IMAGE) {
            buff_err("image check failed:head:0x%08x\n", head->image);
            buff_put_head(head, blk_id);
            return DRV_ERROR_INVALID_VALUE;
        }

        tail = buff_get_tail(head, blk_id);
        if (tail == NULL) {
            buff_err("Tail is NULL.\n");
            buff_put_head(head, blk_id);
            return DRV_ERROR_INNER_ERR;
        }
        if (tail->image != UNI_TAIL_IMAGE) {
            buff_err("Image check failed. (tail_image=0x%08x)\n", tail->image);
            buff_put_tail(tail, blk_id);
            buff_put_head(head, blk_id);
            return DRV_ERROR_INVALID_VALUE;
        }
        buff_put_tail(tail, blk_id);

        if (uni_head != NULL) {
            *uni_head = head;
        }

        /* buff_put_head by caller */
        return DRV_ERROR_NONE;
    }
    buff_err("buff is NULL\n");
    return DRV_ERROR_INVALID_VALUE;
}

drvError_t mbuf_verify_and_get_head(void *mbuf, struct uni_buff_head_t **uni_head)
{
    struct uni_buff_head_t *head = NULL;
    struct uni_buff_tail_t *tail = NULL;

    if (mbuf != NULL) {
        head = mbuf_get_head(mbuf);
        if (head->image != UNI_HEAD_IMAGE) {
            buff_err("Image check failed. (head_image=0x%08x)\n", head->image);
            return DRV_ERROR_INVALID_VALUE;
        }

        tail = mbuf_get_tail(mbuf);
        if (tail->image != UNI_TAIL_IMAGE) {
            buff_err("Image check failed. (tail_image=0x%08x)\n", tail->image);
            return DRV_ERROR_INVALID_VALUE;
        }

        if (uni_head != NULL) {
            *uni_head = head;
        }

        return DRV_ERROR_NONE;
    }
    buff_err("Buff is NULL.\n");
    return DRV_ERROR_INVALID_VALUE;
}

struct uni_buff_head_t *buff_head_init(uintptr_t start, uintptr_t end, uint32 align, uint32 ext_info_flag,
                                       uint32 trace_flag)
{
    struct uni_buff_head_t *head = NULL;
    struct uni_buff_tail_t *tail = NULL;

    head = buff_get_head_by_start(start, end, align, ext_info_flag, trace_flag);

    head->image = UNI_HEAD_IMAGE;
    head->status = UNI_STATUS_ALLOC;
    head->timestamp = 0;
    head->ref = 0;
    head->ext_flag = (uint64)ext_info_flag & 0x1;
    head->recycle_flag = 0;
    head->resv_head = (uint64)(((uintptr_t)head - start) / UNI_UNIT_SIZE) & 0x1FF; // lint !e507
    head->mbuf_data_flag = UNI_MBUF_DATA_DISABLE;
    head->buff_type = TYPE_NONE;
    head->align_flag = 0;
    head->size = (uint64)(end - (uintptr_t)head) & 0xFFFFFFFFFF; // lint !e507
    head->resv = 0;
    head->index = 0;
    head->offset = 0;

    tail = (struct uni_buff_tail_t *)(uintptr_t)(end - sizeof(struct uni_buff_tail_t));
    tail->size = (uint32)((char *)tail - (char *)head);
    tail->image = UNI_TAIL_IMAGE;

    return head;
}

void buff_ext_info_init(struct uni_buff_head_t *head, int pid, uint64 uni_process_id)
{
    struct uni_buff_ext_info *ext_info = NULL;

    if (head->ext_flag != 0) {
        ext_info = (struct uni_buff_ext_info *)((char *)head - sizeof(struct uni_buff_ext_info));
    }

    if (ext_info != NULL) {
        ext_info->alloc_pid = pid;
        ext_info->use_pid = pid;
        ext_info->process_uni_id = uni_process_id;
    }
}

static drvError_t buff_local_free(uint32_t blk_id, void *buff, struct uni_buff_head_t *head)
{
    struct common_handle_t *handle = NULL;
    unsigned long long mem_mng;
    drvError_t ret;

    ret = buff_range_mem_mng_get(blk_id, buff, 1UL, &mem_mng); // minimum size is 1
    if (ret != 0) {
        buff_err("Buff invalid. (buff=%p)\n", buff);
        return DRV_ERROR_INVALID_VALUE;
    }

    handle = (struct common_handle_t *)(uintptr_t)mem_mng;
    if (handle == NULL) {
        buff_err("buff %p handle is NULL\n", buff);
        buff_show_info(buff, blk_id);
        return DRV_ERROR_NO_RESOURCES;
    }

    if (handle->type == UNI_TYPE_LARGE) {
        buff_debug("type is huge_buff, %p\n", buff);
        return memzone_free_huge(handle, buff);
    } else if (handle->type == UNI_TYPE_ZONE) {
        return memzone_free_normal(handle, buff, head);
    } else if (handle->type == UNI_TYPE_MP) {
        return mp_free_buff(handle, buff);
    } else {
        buff_err("invalid parent:%p or type:%u\n", handle, handle->type);
        return DRV_ERROR_BAD_ADDRESS;
    }
}

drvError_t halBuffProcCacheFree(unsigned long flag)
{
    uint32_t all_devid_flag = buff_get_all_devid_flag(flag);
    drvError_t ret;
    uint32_t devid;

    if (all_devid_flag == BUFF_ALL_DEVID) {
        devid = BUFF_INVALID_DEV;
    } else {
        devid = (uint32_t)buff_get_devid_from_flags(flag);
        if (devid >= BUFF_MAX_DEV) {
            buff_err("Invalid devid. (flag=0x%lx)\n", flag);
            return DRV_ERROR_INVALID_DEVICE;
        }
    }

    ret = buff_proc_cache_free(devid);
    buff_event("hal_buff_proc_cache_free done. (flag=0x%lx; ret=%d)\n", flag, ret);
    return ret;
}

drvError_t buff_free(uint32_t blk_id, void *buff, struct uni_buff_head_t *head)
{
    drvError_t ret;
    int owner;

    ret = buff_range_owner_get(blk_id, buff, 1UL, &owner); // minimum size is 1
    if (ret != 0) {
        buff_err("buff %p invalid\n", buff);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (head->status != UNI_STATUS_ALLOC) {
        buff_err("Invalid buffer status. (buff_status=%d; buffer=%p)\n", head->status, buff);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (head->ref == 0) {
        buff_err("buff %p invalid ref:%u\n", buff, head->ref);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (buff_api_atomic_dec_and_return(&head->ref) > 0) {
        return DRV_ERROR_NONE;
    }

    if (owner == RANGE_OWNER_OTHERS) {
        /* set release status, the alloc procecss or recycle thread will free */
        head->status = UNI_STATUS_RELEASE;
        return DRV_ERROR_NONE;
    }

    return buff_local_free(blk_id, buff, head);
}

int halBuffFree(void *buff)
{
    struct uni_buff_head_t *head = NULL;
    unsigned long alloc_size;
    void *alloc_addr = NULL;
    drvError_t ret;
    uint32_t blk_id;
    int pool_id;

    ret = buff_blk_get(buff, &pool_id, &alloc_addr, &alloc_size, &blk_id);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Not alloc. (ret=%d)\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }
    buff_blk_put(pool_id, alloc_addr);

    if (buff_verify_and_get_head(buff, &head, blk_id) != 0) {
        buff_err("buff is invalid, 0x%lx\n", (uintptr_t)buff); // lint !e507
        return (int)DRV_ERROR_BAD_ADDRESS;
    }

    if ((head->buff_type == MBUF_NORMAL) || (head->buff_type == MBUF_BY_POOL) || (head->buff_type == MBUF_BARE_BUFF) ||
        (head->buff_type == MBUF_BY_BUILD)) {
        buff_err("Buff type invalid. (buff_type=%d)\n", head->buff_type);
        buff_put_head(head, blk_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    if (head->mbuf_data_flag == UNI_MBUF_DATA_ENABLE) {
        buff_err("buff %p belongs to mbuf, should free mbuf first\n", buff);
        buff_put_head(head, blk_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    ret = buff_free(blk_id, buff, head);
    buff_put_head(head, blk_id);
    if (ret == DRV_ERROR_NONE) {
        buff_range_put(blk_id, buff);
        idle_buff_range_free_ahead(blk_id);
    }
    return (int)ret;
}

#else
void drv_buff_unibuff_ut(void)
{
    return;
}
#endif
