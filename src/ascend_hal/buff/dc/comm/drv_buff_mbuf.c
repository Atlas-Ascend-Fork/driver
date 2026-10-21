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

#include "drv_buff_unibuff.h"
#include "drv_buff_common_mempool.h"
#include "drv_usr_buff_mempool.h"
#include "drv_buff_list.h"
#include "drv_buff_memzone.h"
#include "ascend_hal.h"
#include "drv_buff_adp.h"
#include "buff_manage_kernel_api.h"
#include "buff_feature.h"
#include "buff_platform.h"
#include "buff_mng.h"
#include "buff_user_interface.h"
#include "buff_range.h"
#include "drv_buff_mbuf.h"

#define MBUF_VERIFY_TYPE_MAX (2)

typedef drvError_t (*mbuf_verify_func)(struct Mbuf *);

static drvError_t mbuf_verify_single_mbuf(struct Mbuf *mbuf);
static drvError_t mbuf_verify_mbuf_chain(struct Mbuf *mbuf);
static void mbuf_trace(struct share_mbuf *s_mbuf, int opt_type);
static void mbuf_trace_print(struct share_mbuf *s_mbuf);

static mbuf_verify_func g_mbuf_verify_handle[MBUF_VERIFY_TYPE_MAX] = {
    mbuf_verify_single_mbuf,
    mbuf_verify_mbuf_chain,
};

STATIC THREAD unsigned int g_mbuf_priv_flag = 0;

/*lint -e454 -e455*/
static int mbuf_atomlock_lock(struct atomic_lock *lock)
{
    return get_atomic_lock(lock, ATOMIC_LOCK_TIME_DEFAULT);
}

static void mbuf_atomlock_unlock(struct atomic_lock *lock)
{
    release_atomic_lock(lock);
}

/*lint +e454 +e455*/
STATIC void mbuf_atomlock_init(struct atomic_lock *lock)
{
    init_atomic_lock(lock);
}

static void mbuf_atomlock_destroy(struct atomic_lock *lock)
{
    init_atomic_lock(lock);
}

STATIC THREAD bool is_set = false;
void mbuf_set_priv_flag(unsigned int flag)
{
    if (is_set == false) {
        g_mbuf_priv_flag = flag;
        is_set = true;
        buff_event("Set mbuf priv flag success. (flag=%u, g_mbuf_priv_flag=%u)\n", flag, g_mbuf_priv_flag);
    }
}

struct share_mbuf *get_share_mbuf_by_mbuf(Mbuf *mbuf)
{
    if (g_mbuf_priv_flag == BUFF_ENABLE_PRIVATE_MBUF) {
        return mbuf->s_mbuf;
    }
    return (struct share_mbuf *)(void *)mbuf;
}

static struct Mbuf *create_priv_mbuf(struct share_mbuf *s_mbuf, void *data, uint64_t total_len, uint64_t data_len,
                                     uint64_t type)
{
    struct Mbuf *mbuf = (struct Mbuf *)(void *)s_mbuf;

    if (g_mbuf_priv_flag == BUFF_ENABLE_PRIVATE_MBUF) {
        mbuf = malloc(sizeof(struct Mbuf));
        if (mbuf == NULL) {
            return NULL;
        }

        mbuf_atomlock_init(&mbuf->lock);
        mbuf->total_len = total_len;
        mbuf->data_len = data_len;
        mbuf->datablock = data;
        mbuf->data = data;
        mbuf->s_mbuf = s_mbuf;
        mbuf->buff_type = type;
        mbuf->prev = NULL;
        mbuf->next = NULL;
        mbuf->blk_id = s_mbuf->blk_id;
        mbuf->data_blk_id = s_mbuf->data_blk_id;
    }

    return mbuf;
}

static void destroy_priv_mbuf(struct Mbuf *mbuf)
{
    if (g_mbuf_priv_flag == BUFF_ENABLE_PRIVATE_MBUF) {
        free(mbuf);
    }
}

static bool mbuf_data_is_invalid(struct share_mbuf *s_mbuf)
{
    return ((unsigned long)(uintptr_t)s_mbuf->data == 0);
}

static int mbuf_len_is_invalid(struct share_mbuf *s_mbuf)
{
    return ((s_mbuf->data_len > s_mbuf->total_len) || (s_mbuf->total_len == 0));
}

drvError_t mbuf_is_invalid(struct Mbuf *mbuf)
{
    struct share_mbuf *s_mbuf = get_share_mbuf_by_mbuf(mbuf);
    struct uni_buff_head_t *s_mbuf_head = NULL;

    if (mbuf_verify_and_get_head((void *)s_mbuf, &s_mbuf_head) != 0) {
        buff_err("mbuf is invalid. (s_mbuf=%lx)\n", (uintptr_t)s_mbuf);
        return DRV_ERROR_BAD_ADDRESS;
    }

    if ((s_mbuf_head->buff_type != MBUF_NORMAL) && (s_mbuf_head->buff_type != MBUF_BY_POOL) &&
        (s_mbuf_head->buff_type != MBUF_BARE_BUFF) && (s_mbuf_head->buff_type != MBUF_BY_BUILD)) {
        buff_err("mbuf %p buff type invalid:%u\n", s_mbuf, s_mbuf_head->buff_type);
        mbuf_trace_print(s_mbuf);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf_data_is_invalid(s_mbuf) || (mbuf_len_is_invalid(s_mbuf) != 0)) {
        buff_err("mbuf %p check fail:data:%p, %p, len:%lu, %lu\n", s_mbuf, s_mbuf->data, s_mbuf->datablock,
                 s_mbuf->data_len, s_mbuf->total_len);
        mbuf_trace_print(s_mbuf);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static bool mbuf_chain_head_is_invalid(struct Mbuf *mbuf_chain_head)
{
    struct share_mbuf *s_mbuf = NULL;

    if ((mbuf_chain_head == NULL) || (mbuf_chain_head->prev != NULL)) {
        buff_err("mbuf chain head is invalid!\n");
        return true;
    }

    s_mbuf = get_share_mbuf_by_mbuf(mbuf_chain_head);
    if ((s_mbuf == NULL) || (s_mbuf->prev != NULL)) {
        buff_err("share mbuf_chain_head is invalid!\n");
        return true;
    }

    return false;
}

static drvError_t mbuf_chain_get_tail(struct Mbuf *mbuf_chain_head, struct Mbuf **tail, unsigned int *cnt)
{
    struct Mbuf *prev = mbuf_chain_head;
    struct Mbuf *tmp = mbuf_chain_head;
    unsigned int n = 0;

    while (tmp != NULL) {
        if (n >= MBUF_CHAIN_MAX_LEN) {
            buff_err("mbuf chain is too long, head: %p, max chain len: %u\n", mbuf_chain_head, MBUF_CHAIN_MAX_LEN);
            return DRV_ERROR_OVER_LIMIT;
        }

        if (mbuf_is_invalid(tmp) != DRV_ERROR_NONE) {
            buff_err("Mbuf chain is invalid. (head=%p; index=%u; tmp=%p)\n", mbuf_chain_head, n, tmp);
            return DRV_ERROR_INVALID_VALUE;
        }
        prev = tmp;
        tmp = tmp->next;
        n++;
    }

    *tail = prev;
    *cnt = n;
    return DRV_ERROR_NONE;
}

static struct share_mbuf *create_share_mbuf(uint32_t devid, void *data, uint64_t total_len, uint64_t data_len,
                                            uint64_t type)
{
    struct uni_buff_head_t *s_mbuf_head = NULL;
    struct share_mbuf *s_mbuf = NULL;
    uint32_t blk_id;

    if (mp_alloc_mbuf_head(devid, (void **)&s_mbuf, &blk_id) != DRV_ERROR_NONE) {
        buff_err("buff_alloc buff_ctrl error!\n");
        return NULL;
    }

    s_mbuf->total_len = total_len;
    s_mbuf->data = data;
    s_mbuf->data_len = data_len;
    s_mbuf->datablock = data;
    s_mbuf->timestamp = buff_get_cur_timestamp();
    s_mbuf->buff_type = type;
    s_mbuf->next = NULL;
    s_mbuf->prev = NULL;
    s_mbuf->s_mbuf = s_mbuf;
    s_mbuf->blk_id = blk_id;
    (void)memset_s(s_mbuf->user_data, USER_DATA_LEN, 0, USER_DATA_LEN);
    mbuf_atomlock_init(&s_mbuf->lock);

    s_mbuf_head = buff_mempool_get_head((void *)s_mbuf);
    s_mbuf_head->buff_type = type & 0xFU;

    return s_mbuf;
}

static void destroy_share_mbuf(struct share_mbuf *s_mbuf)
{
    struct uni_buff_head_t *s_mbuf_head = NULL;

    s_mbuf->datablock = NULL;
    s_mbuf->data = NULL;
    s_mbuf->s_mbuf = NULL;
    mbuf_atomlock_destroy(&s_mbuf->lock);

    s_mbuf_head = buff_mempool_get_head((void *)s_mbuf);
    (void)buff_free(s_mbuf->blk_id, s_mbuf, s_mbuf_head);
}

static void mbuf_alloc_info_set(struct mem_info_t *info, int grp_id, unsigned long flag, uint64_t size,
                                unsigned int align)
{
    int grp_id_val = grp_id;
    if (grp_id != buff_get_default_pool_id()) {
        buff_warn("Grp_id is not alloc_id. (grp_id=%d; alloc_id=%d)\n", grp_id_val, buff_get_default_pool_id());
        grp_id_val = buff_get_default_pool_id();
    }
    info->grp_id = grp_id_val;
    info->flag = flag;
    info->size = size;
    info->align = align;
    info->alloc_type = MZ_ALLOC_TYPE_NORMAL;
}

static drvError_t mbuf_data_block_alloc(struct mem_info_t *info, char **buff_data, uint32_t *data_blk_id,
                                        struct uni_buff_head_t **data_head)
{
    char *data = NULL;
    drvError_t ret;

    ret = memzone_alloc_buff(info, (void **)&data, data_blk_id);
    if (ret != DRV_ERROR_NONE) {
        buff_err("buff_alloc buff_data error:%d!\n", ret);
        return ret;
    }

    *data_head = (struct uni_buff_head_t *)(((uintptr_t)data - info->offset) - sizeof(**data_head));
    if (*data_head == NULL) {
        buff_err("set data type error, get head failed\n");
        (void)halBuffFree(data);
        return DRV_ERROR_INVALID_VALUE;
    }
    (*data_head)->mbuf_data_flag = UNI_MBUF_DATA_ENABLE;
    *buff_data = data;

    return DRV_ERROR_NONE;
}

int halMbufAllocEx(uint64_t size, unsigned int align, unsigned long flag, int grp_id, Mbuf **mbuf)
{
    struct share_mbuf *s_mbuf = NULL;
    struct Mbuf *tmp_mbuf = NULL;
    char *buff_data = NULL;
    struct uni_buff_head_t *data_head = NULL;
    struct mem_info_t info;
    drvError_t ret;
    uint32_t data_blk_id;

    if ((mbuf == NULL) || (size == 0)) {
        buff_err("para is invalid, mbuf:0x%lx, size:%lu\n", (uintptr_t)mbuf, size); // lint !e507
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    if (!buff_check_align(align)) {
        buff_err("align is invalid:%u\n", align);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    mbuf_alloc_info_set(&info, grp_id, flag, size, align);
    ret = mbuf_data_block_alloc(&info, &buff_data, &data_blk_id, &data_head);
    if (ret != DRV_ERROR_NONE) {
        return (int)ret;
    }

    s_mbuf = create_share_mbuf((uint32_t)buff_get_devid_from_flags(flag), buff_data, size, 0, MBUF_NORMAL);
    if (s_mbuf == NULL) {
        buff_err("buff_alloc buff_ctrl error!\n");
        data_head->mbuf_data_flag = UNI_MBUF_DATA_DISABLE;
        (void)halBuffFree(buff_data);
        return (int)DRV_ERROR_INVALID_VALUE;
    }
    s_mbuf->data_blk_id = data_blk_id;

    tmp_mbuf = create_priv_mbuf(s_mbuf, buff_data, size, 0, MBUF_NORMAL);
    if (tmp_mbuf == NULL) {
        buff_err("Create priv mbuf failed\n");
        data_head->mbuf_data_flag = UNI_MBUF_DATA_DISABLE;
        (void)halBuffFree(buff_data);
        destroy_share_mbuf(s_mbuf);
        return (int)DRV_ERROR_INVALID_VALUE;
    }
    mbuf_trace(s_mbuf, MBUF_ALLOC_BY_ZONE);
    *mbuf = tmp_mbuf;

    return (int)DRV_ERROR_NONE;
}

drvError_t hal_mbuf_alloc_align(uint64_t size, unsigned int align, Mbuf **mbuf)
{
    int grp_id = buff_get_default_pool_id();
    unsigned long flag = 0;

    flag = buff_make_devid_to_flags(buff_get_current_devid(), flag);
    flag |= buff_get_mbuf_hugepage_policy();

    return halMbufAllocEx(size, align, flag, grp_id, mbuf);
}

int halMbufAlloc(uint64_t size, Mbuf **mbuf)
{
    return (int)hal_mbuf_alloc_align(size, UNI_ALIGN_MIN, mbuf);
}

int halMbufAllocByPool(poolHandle pHandle, Mbuf **mbuf)
{
    struct uni_buff_head_t *data_head = NULL;
    struct share_mbuf *s_mbuf = NULL;
    struct Mbuf *tmp_mbuf = NULL;
    char *buff_data = NULL;
    uint32_t buf_blk_id;
    drvError_t ret;

    if ((mbuf == NULL) || (pHandle == NULL)) {
        buff_err("mbuf is NULL!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = mp_alloc_buff(pHandle, (void **)&buff_data, &buf_blk_id);
    if (ret != DRV_ERROR_NONE) {
        return (int)ret;
    }
    data_head = buff_mempool_get_head((void *)buff_data);
    data_head->mbuf_data_flag = UNI_MBUF_DATA_ENABLE;

    s_mbuf = create_share_mbuf(pHandle->head.devid, buff_data, pHandle->blk_size, 0, MBUF_BY_POOL);
    if (s_mbuf == NULL) {
        buff_err("buff_alloc buff_ctrl error:%d!\n", ret);
        buff_free(buf_blk_id, buff_data, data_head);
        return (int)DRV_ERROR_INVALID_VALUE;
    }
    s_mbuf->data_blk_id = buf_blk_id;
    tmp_mbuf = create_priv_mbuf(s_mbuf, buff_data, pHandle->blk_size, 0, MBUF_BY_POOL);
    if (tmp_mbuf == NULL) {
        buff_err("Create priv mbuf failed\n");
        buff_free(buf_blk_id, buff_data, data_head);
        destroy_share_mbuf(s_mbuf);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    mbuf_trace(s_mbuf, MBUF_ALLOC_BY_POOL);
    buff_trace(buff_data, s_mbuf, buff_api_getpid(), MBUF_ALLOC_BY_POOL, -1);
    *mbuf = tmp_mbuf;

    return DRV_ERROR_NONE;
}

void destroy_priv_mbuf_for_queue(struct Mbuf *mbuf)
{
    struct Mbuf *tmp = mbuf;
    struct Mbuf *next = NULL;
    unsigned int num = 0;

    while (tmp != NULL) {
        if (num >= MBUF_CHAIN_MAX_LEN) {
            buff_err("Mbuf chain limit.\n");
            return;
        }
        if (tmp->buff_type != MBUF_BARE_BUFF) {
            buff_range_put(tmp->data_blk_id, tmp->datablock);
        }
        next = tmp->next;
        buff_range_put(tmp->blk_id, get_share_mbuf_by_mbuf(tmp));
        destroy_priv_mbuf(tmp);
        tmp = next;
        num++;
    }
}

drvError_t create_priv_mbuf_for_queue(struct Mbuf **mbuf, void *buff, uint32_t blk_id)
{
    struct share_mbuf *tmp = (struct share_mbuf *)(void *)buff;
    struct Mbuf *tmp_mbuf = NULL;
    struct Mbuf *prev = NULL;
    uint32_t block_id = blk_id;
    unsigned int num = 0;
    drvError_t ret;

    while (tmp != NULL) {
        struct uni_buff_ext_info *ext_info = mbuf_get_ext_info(tmp);

        if (num >= MBUF_CHAIN_MAX_LEN) {
            buff_err("Mbuf chain limit.\n");
            ret = DRV_ERROR_OVER_LIMIT;
            goto err;
        }
        /* rangeget should include ext, because mbuf_owner_update will use this mem */
        ret = buff_range_get(block_id, ext_info, sizeof(*ext_info) + mbuf_get_verify_size());
        if (ret != DRV_ERROR_NONE) {
            buff_warn("Mbuf is illegal. (tmp=%p)\n", tmp);
            goto err;
        }
        if (tmp->blk_id != block_id) {
            buff_range_put(block_id, ext_info);
            buff_err("Que entity blk_id and share_mbuf blk_id are not same. (entity_blk_id=%u; s_mbuf->blk_id=%u)\n",
                     tmp->blk_id, block_id);
            goto err;
        }
        tmp_mbuf = create_priv_mbuf(tmp, tmp->data, tmp->total_len, tmp->data_len, tmp->buff_type);
        if (tmp_mbuf == NULL) {
            buff_range_put(tmp->blk_id, ext_info);
            buff_warn("Create tmp_priv_mbuf not success. (num=%u)\n", num);
            ret = DRV_ERROR_OUT_OF_MEMORY;
            goto err;
        }
        if (tmp_mbuf->buff_type != MBUF_BARE_BUFF) {
            ret = buff_range_get(tmp_mbuf->data_blk_id, tmp_mbuf->datablock, tmp_mbuf->total_len);
            if (ret != DRV_ERROR_NONE) {
                destroy_priv_mbuf(tmp_mbuf);
                buff_range_put(tmp->blk_id, ext_info);
                buff_warn("mbuf datablock illegal\n");
                goto err;
            }
        }
        if (num == 0) {
            *mbuf = tmp_mbuf;
        } else {
            prev->next = tmp_mbuf;
            prev->next_blk_id = tmp_mbuf->blk_id;
            tmp_mbuf->prev = prev;
            tmp_mbuf->prev_blk_id = prev->blk_id;
        }
        prev = tmp_mbuf;
        block_id = tmp->next_blk_id;
        tmp = tmp->next;

        num++;
    }

    return DRV_ERROR_NONE;

err:
    if (num > 0) {
        destroy_priv_mbuf_for_queue(*mbuf);
    }
    *mbuf = NULL;

    return ret;
}

static drvError_t mbuf_free_one_buf(struct Mbuf *mbuf, int type)
{
    struct share_mbuf *s_mbuf = get_share_mbuf_by_mbuf(mbuf);
    struct uni_buff_head_t *s_mbuf_head = NULL;
    struct uni_buff_head_t *data_head = NULL;
    void *datablock = NULL;
    drvError_t ret;

    if (mbuf_verify_and_get_head((void *)s_mbuf, &s_mbuf_head) != 0) {
        buff_err("mbuf is invalid. (s_mbuf=%lx)\n", (uintptr_t)s_mbuf);
        return DRV_ERROR_BAD_ADDRESS;
    }

    if ((s_mbuf_head->buff_type != MBUF_NORMAL) && (s_mbuf_head->buff_type != MBUF_BY_POOL) &&
        (s_mbuf_head->buff_type != MBUF_BARE_BUFF) && (s_mbuf_head->buff_type != MBUF_BY_BUILD)) {
        buff_err("buff type invalid:type:%u\n", s_mbuf_head->buff_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    datablock = mbuf->datablock;
    if (datablock != NULL) {
        if (mbuf->buff_type != MBUF_BARE_BUFF) {
            if (buff_verify_and_get_head(datablock, &data_head, mbuf->data_blk_id) != 0) {
                buff_err("mbuff data is invalid. (datablock=%p; data_blk_id=%u)\n", mbuf->datablock, mbuf->data_blk_id);
                return DRV_ERROR_BAD_ADDRESS;
            }

            buff_trace(datablock, s_mbuf, buff_api_getpid(), type, -1);
            ret = buff_free(mbuf->data_blk_id, datablock, data_head);
            if (ret != DRV_ERROR_NONE) {
                buff_put_head(data_head, mbuf->data_blk_id);
                buff_err("buff_free datablock free failed! ret:0x%x\n", ret);
                return ret;
            }
            buff_put_head(data_head, mbuf->data_blk_id);
            buff_range_put(mbuf->data_blk_id, datablock);
            idle_buff_range_free_ahead(mbuf->data_blk_id);
        }
        s_mbuf->datablock = NULL;
        s_mbuf->data = NULL;
    }
    s_mbuf->next = NULL;
    s_mbuf->prev = NULL;
    mbuf_atomlock_destroy(&s_mbuf->lock);

    mbuf_trace(s_mbuf, type);
    /* If you buff_range_put first and then buff_free,
     * it is possible that buff_free fails because the internal buff_range_owner_get of buff_free fails.
     * After buff_free, s_mbuf can no longer be accessed, because s_mbuf may be recycled.
     */
    ret = buff_free(s_mbuf->blk_id, (void *)s_mbuf, s_mbuf_head);
    if (ret != DRV_ERROR_NONE) {
        buff_err("buff_free mbuf free failed!. (s_mbuf->blk_id=%d)\n", s_mbuf->blk_id);
        return ret;
    }
    buff_range_put(mbuf->blk_id, s_mbuf);
    destroy_priv_mbuf(mbuf);
    return DRV_ERROR_NONE;
}

int halMbufGetDataPtr(Mbuf *mbuf, void **buf, uint64_t *size)
{
    if ((mbuf == NULL) || (get_share_mbuf_by_mbuf(mbuf) == NULL) || (buf == NULL) || (size == NULL)) {
        buff_err("Mbuf_get_data_ptr input parameter error!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf_is_invalid(mbuf) != 0) {
        buff_err("mbuf invalid!\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    *buf = mbuf->datablock;
    *size = mbuf->total_len;

    return DRV_ERROR_NONE;
}

int halMbufGetBuffAddr(Mbuf *mbuf, void **buf)
{
    if ((mbuf == NULL) || (get_share_mbuf_by_mbuf(mbuf) == NULL) || (buf == NULL)) {
        buff_err("Mbuf_get_data_ptr input parameter error!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf_is_invalid(mbuf) != 0) {
        buff_err("mbuf %p invalid!\n", mbuf);
        return DRV_ERROR_INVALID_VALUE;
    }
    *buf = mbuf->datablock;

    return DRV_ERROR_NONE;
}

int halMbufGetBuffSize(Mbuf *mbuf, uint64_t *totalSize)
{
    if ((mbuf == NULL) || (get_share_mbuf_by_mbuf(mbuf) == NULL) || (totalSize == NULL)) {
        buff_err("Mbuf_get_data_ptr input parameter error!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf_is_invalid(mbuf) != 0) {
        buff_err("mbuf invalid!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    *totalSize = mbuf->total_len;
    return DRV_ERROR_NONE;
}

int halMbufSetDataLen(Mbuf *mbuf, uint64_t len)
{
    struct share_mbuf *s_mbuf = NULL;
    uint64_t offset;

    if ((mbuf == NULL) || (get_share_mbuf_by_mbuf(mbuf) == NULL) || (len == 0)) {
        buff_err("Mbuf_set_data_len input parameter error! mbuf:0x%lx, len:%lu\n", (uintptr_t)mbuf, len);
        return DRV_ERROR_INVALID_VALUE;
    }

    offset = (uint64_t)(uintptr_t)mbuf->data - (uint64_t)(uintptr_t)mbuf->datablock;
    if ((offset >= mbuf->total_len) || (len > (mbuf->total_len - offset))) {
        buff_err("Mbuf_set_data_len error, offset:0x%lx, total_len:0x%lx, len:0x%lx\n", offset, mbuf->total_len, len);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf_is_invalid(mbuf) != 0) {
        buff_err("mbuf invalid!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    mbuf->data_len = len;
    s_mbuf = get_share_mbuf_by_mbuf(mbuf);
    s_mbuf->data_len = len;
    return DRV_ERROR_NONE;
}

int halMbufGetDataLen(Mbuf *mbuf, uint64_t *len)
{
    if ((mbuf == NULL) || (get_share_mbuf_by_mbuf(mbuf) == NULL) || (len == NULL)) {
        buff_err("Mbuf_get_data_len input parameter error!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((mbuf_is_invalid(mbuf) != DRV_ERROR_NONE) || (mbuf->data_len > mbuf->total_len)) {
        buff_err("mbuf invalid!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    *len = mbuf->data_len;
    return DRV_ERROR_NONE;
}

static drvError_t buff_copy_one_mbuf_ref(struct Mbuf *mbuf, struct Mbuf **new_mbuf)
{
    struct uni_buff_head_t *head = NULL;
    struct share_mbuf *tmp_share_mbuf = NULL;
    struct share_mbuf *s_mbuf = NULL;
    struct Mbuf *tmp_mbuf = NULL;
    drvError_t ret;
    int sub_ret;

    if (mbuf_is_invalid(mbuf) != DRV_ERROR_NONE) {
        buff_err("mbuf invalid!, mbuf: %p\n", mbuf);
        return DRV_ERROR_INVALID_VALUE;
    }

    tmp_share_mbuf = create_share_mbuf((uint32_t)buff_get_current_devid(), mbuf->datablock, mbuf->total_len,
                                       mbuf->data_len, mbuf->buff_type);
    if (tmp_share_mbuf == NULL) {
        buff_err("buff_alloc buff_ctrl error!\n");
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    s_mbuf = get_share_mbuf_by_mbuf(mbuf);
    tmp_share_mbuf->timestamp = s_mbuf->timestamp;
    tmp_share_mbuf->data_blk_id = s_mbuf->data_blk_id;

    sub_ret = memcpy_s(tmp_share_mbuf->user_data, USER_DATA_LEN, s_mbuf->user_data, USER_DATA_LEN);
    if (sub_ret != 0) {
        buff_err("memcpy_s error:%d!\n", sub_ret);
        destroy_share_mbuf(tmp_share_mbuf);
        return DRV_ERROR_INNER_ERR;
    }

    tmp_mbuf = create_priv_mbuf(tmp_share_mbuf, mbuf->data, mbuf->total_len, mbuf->data_len, mbuf->buff_type);
    if (tmp_mbuf == NULL) {
        buff_err("Create priv mbuf failed\n");
        destroy_share_mbuf(tmp_share_mbuf);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    if (tmp_mbuf->buff_type != MBUF_BARE_BUFF) {
        ret = buff_range_get(tmp_mbuf->data_blk_id, tmp_mbuf->datablock, tmp_mbuf->total_len);
        if (ret != 0) {
            destroy_share_mbuf(tmp_share_mbuf);
            destroy_priv_mbuf(tmp_mbuf);
            buff_err("Mbuf get datablock failed. (buff=%p, size=%lu)\n", mbuf->datablock, mbuf->total_len);
            return ret;
        }

        head = buff_get_head(tmp_mbuf->datablock, tmp_mbuf->data_blk_id);
        if (head == NULL) {
            buff_err("uni_head buff_get_head error!\n");
            buff_range_put(tmp_mbuf->data_blk_id, tmp_mbuf->datablock);
            destroy_share_mbuf(tmp_share_mbuf);
            destroy_priv_mbuf(tmp_mbuf);
            return DRV_ERROR_INVALID_VALUE;
        }
        buff_api_atomic_inc(&head->ref);
        buff_put_head(head, tmp_mbuf->data_blk_id);
    }
    mbuf_trace(tmp_share_mbuf, MBUF_ALLOC_BY_COPYREF);
    buff_trace(tmp_mbuf->datablock, tmp_share_mbuf, buff_api_getpid(), MBUF_ALLOC_BY_COPYREF, -1);
    *new_mbuf = tmp_mbuf;
    return DRV_ERROR_NONE;
}

static drvError_t mbuf_copy_ref_check(struct Mbuf *mbuf, struct Mbuf **new_mbuf)
{
    if ((mbuf == NULL) || (get_share_mbuf_by_mbuf(mbuf) == NULL) || (new_mbuf == NULL)) {
        buff_err("copy ref input parameter error!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf_is_invalid(mbuf) != DRV_ERROR_NONE) {
        buff_err("mbuf invalid!, mbuf: %p\n", mbuf);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

int halMbufCopyRef(Mbuf *mbuf, Mbuf **newMbuf)
{
    struct Mbuf *new_head = NULL, *new_tail = NULL, *new_tmp = NULL;
    struct share_mbuf *new_tail_share_mbuf = NULL, *new_tmp_share_mbuf = NULL;
    struct Mbuf *tmp = mbuf;
    unsigned int cnt = 0;
    drvError_t ret;
    int sub_ret;

    ret = mbuf_copy_ref_check(mbuf, newMbuf);
    if (ret != DRV_ERROR_NONE) {
        return (int)ret;
    }
    sub_ret = mbuf_atomlock_lock(&mbuf->lock);
    if (sub_ret != 0) {
        buff_err("Apply atomic_lock error. (ret=%d)\n", sub_ret);
        return (int)DRV_ERROR_INNER_ERR;
    }
    while (tmp != NULL) {
        if (cnt == MBUF_CHAIN_MAX_LEN) {
            ret = DRV_ERROR_OVER_LIMIT;
            buff_err("mbuf chain len out limit. (chain_len=%u, max_len=%u, ret=%d)\n", cnt, MBUF_CHAIN_MAX_LEN, ret);
            break;
        }

        ret = buff_copy_one_mbuf_ref(tmp, &new_tmp);
        if (ret != DRV_ERROR_NONE) {
            buff_err("mbuf chain copy ref err, ret: %d\n", ret);
            break;
        }
        if ((new_head != NULL) && (new_tail != NULL)) {
            new_tail->next = new_tmp;
            new_tail->next_blk_id = new_tmp->blk_id;
            new_tmp->prev = new_tail;
            new_tmp->prev_blk_id = new_tail->blk_id;
            new_tail_share_mbuf = get_share_mbuf_by_mbuf(new_tail);
            new_tmp_share_mbuf = get_share_mbuf_by_mbuf(new_tmp);
            new_tail_share_mbuf->next = new_tmp_share_mbuf;
            new_tail_share_mbuf->next_blk_id = new_tmp_share_mbuf->blk_id;
            new_tmp_share_mbuf->prev = new_tail_share_mbuf;
            new_tmp_share_mbuf->prev_blk_id = new_tail_share_mbuf->blk_id;
            new_tail = new_tmp;
        } else {
            new_head = new_tmp;
            new_tail = new_tmp;
        }
        cnt++;
        tmp = tmp->next;
    }
    mbuf_atomlock_unlock(&mbuf->lock);

    if ((ret != DRV_ERROR_NONE) && (new_head != NULL)) {
        int ret_free;
        ret_free = halMbufFree(new_head);
        if (ret_free != 0) {
            buff_err("mbuf free err, ret: %d\n", ret_free);
        }
    } else {
        *newMbuf = new_head;
    }

    return (int)ret;
}

int halMbufGetPrivInfo(Mbuf *mbuf, void **priv, unsigned int *size)
{
    struct share_mbuf *s_mbuf = NULL;

    if ((mbuf == NULL) || (get_share_mbuf_by_mbuf(mbuf) == NULL) || (priv == NULL) || (size == NULL)) {
        buff_err("Mbuf_get_priv_info input parameter error!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf_is_invalid(mbuf) != 0) {
        buff_err("mbuf invalid!\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    s_mbuf = get_share_mbuf_by_mbuf(mbuf);
    *priv = s_mbuf->user_data;
    *size = USER_DATA_LEN;
    return DRV_ERROR_NONE;
}

#ifndef EMU_ST
int halMbufGetMbufInfo(Mbuf *mbuf, MbufInfoConverge *mbufInfo)
{
    (void)mbuf;
    (void)mbufInfo;
    return (int)DRV_ERROR_NOT_SUPPORT;
}
#endif

int halMbufBuild(void *buff, uint64_t len, Mbuf **mbuf)
{
    struct uni_buff_head_t *align_head = NULL;
    struct uni_buff_head_t *buff_head = NULL;
    struct share_mbuf *s_mbuf = NULL;
    struct Mbuf *tmp_mbuf = NULL;
    unsigned long alloc_size;
    void *alloc_addr = NULL;
    uint32 buff_blk_id;
    uint64_t buf_size;
    int pool_id;
    drvError_t ret;

    if ((mbuf == NULL) || (buff == NULL)) {
        buff_err("Mbuf_build input parameter error!\n");
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    ret = buff_blk_get(buff, &pool_id, &alloc_addr, &alloc_size, &buff_blk_id);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Not alloc.\n");
        return (int)DRV_ERROR_INVALID_VALUE;
    }
    buff_blk_put(pool_id, alloc_addr);

    if (buff_verify_and_get_head(buff, &buff_head, buff_blk_id) != 0) {
        buff_err("Buff is invalid.\n");
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    if (buff_head->mbuf_data_flag == UNI_MBUF_DATA_ENABLE) {
        buff_err("Input buff is mbuf_data. (buff=%p)\n", buff);
        buff_put_head(buff_head, buff_blk_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    align_head = (struct uni_buff_head_t *)((char *)buff - sizeof(struct uni_buff_head_t));
    buf_size = (uint64_t)(((buff_head->size - align_head->offset) - sizeof(struct uni_buff_head_t)) -
                          sizeof(struct uni_buff_tail_t));
    if (len > buf_size) {
        buff_err("len is invalid, len:0x%lx, buf_size:0x%lx\n", len, buf_size);
        buff_put_head(buff_head, buff_blk_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    *mbuf = NULL;

    s_mbuf = create_share_mbuf((uint32_t)buff_get_current_devid(), buff, len, len, MBUF_BY_BUILD);
    if (s_mbuf == NULL) {
        buff_err("buff_alloc buff_ctrl error!\n");
        buff_put_head(buff_head, buff_blk_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }
    s_mbuf->data_blk_id = buff_blk_id;

    tmp_mbuf = create_priv_mbuf(s_mbuf, buff, len, len, MBUF_BY_BUILD);
    if (tmp_mbuf == NULL) {
        buff_err("Create priv mbuf failed\n");
#ifndef EMU_ST
        buff_put_head(buff_head, buff_blk_id);
#endif
        destroy_share_mbuf(s_mbuf);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    mbuf_trace(s_mbuf, MBUF_ALLOC_BY_BUILD);

    *mbuf = tmp_mbuf;
    buff_head->mbuf_data_flag = UNI_MBUF_DATA_ENABLE;
    buff_put_head(buff_head, buff_blk_id);

    return (int)DRV_ERROR_NONE;
}

int halMbufUnBuild(Mbuf *mbuf, void **buff, uint64_t *len)
{
    struct uni_buff_head_t *head = NULL;
    struct share_mbuf *s_mbuf = NULL;

    if ((buff == NULL) || (len == NULL)) {
        buff_err("input para invalid.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if ((mbuf == NULL) || (mbuf_is_invalid(mbuf) != DRV_ERROR_NONE)) {
        buff_err("Mbuf invalid. (mbuf=0x%llx)\n", (unsigned long long)(uintptr_t)mbuf);
        return DRV_ERROR_INVALID_VALUE;
    }

    s_mbuf = get_share_mbuf_by_mbuf(mbuf);
    if (s_mbuf->next != NULL || s_mbuf->prev != NULL) {
        buff_err("mbuf chain is not allowed to unbuild.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (buff_verify_and_get_head(mbuf->datablock, &head, s_mbuf->data_blk_id) != 0) {
        buff_err("Buff is invalid.\n");
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    if (head->ref != 1) {
        buff_err("Input mbuff data ref(%u) is not equal to 1. \n", head->ref);
        buff_put_head(head, s_mbuf->data_blk_id);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    *buff = mbuf->datablock;
    *len = mbuf->total_len;

    head->mbuf_data_flag = UNI_MBUF_DATA_DISABLE;
    buff_put_head(head, s_mbuf->data_blk_id);
    /* If you buff_range_put first and then destroy_share_mbuf,
     * it is possible that destroy_share_mbuf fails because the internal buff_range_owner_get of destroy_share_mbuf
     * fails. After destroy_share_mbuf, s_mbuf can no longer be accessed, because s_mbuf may be recycled.
     */
    destroy_share_mbuf(s_mbuf);
    buff_range_put(mbuf->blk_id, s_mbuf);

    destroy_priv_mbuf(mbuf);

    return DRV_ERROR_NONE;
}

drvError_t mbuf_build_bare_buff(void *buff, uint64_t len, struct Mbuf **mbuf)
{
    struct share_mbuf *s_mbuf = NULL;

    if ((mbuf == NULL) || (buff == NULL) || (len == 0)) {
        buff_err("Mbuf_build input parameter error!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    s_mbuf = create_share_mbuf((uint32_t)buff_get_current_devid(), buff, len, len, MBUF_BARE_BUFF);
    if (s_mbuf == NULL) {
        buff_err("buff_alloc buff_ctrl error!\n");
        return (int)DRV_ERROR_INNER_ERR;
    }

    *mbuf = create_priv_mbuf(s_mbuf, buff, len, len, MBUF_BARE_BUFF);
    if (*mbuf == NULL) {
        buff_err("Create priv mbuf failed\n");
        destroy_share_mbuf(s_mbuf);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    mbuf_trace(s_mbuf, MBUF_ALLOC_BY_BUILD_BARE_BUFF);

    return DRV_ERROR_NONE;
}

static drvError_t mbuf_chain_append_para_check(Mbuf *mbufChainHead, Mbuf *mbuf)
{
    if (mbufChainHead == mbuf) {
        buff_err("mbuf chain head is equal to mbuf: %p\n", mbuf);
        return DRV_ERROR_PARA_ERROR;
    }

    if (mbuf_chain_head_is_invalid(mbufChainHead)) {
        buff_err("mbuf chain head is invalid\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf_chain_head_is_invalid(mbuf)) {
        buff_err("mbuf chain mbuf target is invalid\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

int halMbufChainAppend(Mbuf *mbufChainHead, Mbuf *mbuf)
{
    struct share_mbuf *prev_share_mbuf = NULL, *s_mbuf = NULL;
    struct Mbuf *tmp = NULL, *prev = NULL;
    unsigned int chain_cnt = 0, mbuf_cnt = 0;
    drvError_t ret;
    int sub_ret;

    ret = mbuf_chain_append_para_check(mbufChainHead, mbuf);
    if (ret != DRV_ERROR_NONE) {
        return (int)ret;
    }

    sub_ret = mbuf_atomlock_lock(&mbufChainHead->lock);
    if (sub_ret != 0) {
        buff_err("Apply atomic_lock error. (ret=%d)\n", sub_ret);
        return (int)DRV_ERROR_INNER_ERR;
    }

    if (mbuf_chain_head_is_invalid(mbufChainHead)) {
        mbuf_atomlock_unlock(&mbufChainHead->lock);
        buff_err("mbuf chain head is invalid\n");
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    ret = mbuf_chain_get_tail(mbufChainHead, &prev, &chain_cnt);
    if (ret != DRV_ERROR_NONE) {
        mbuf_atomlock_unlock(&mbufChainHead->lock);
        buff_err("mbuf chain get tail err\n");
        return (int)ret;
    }

    ret = mbuf_chain_get_tail(mbuf, &tmp, &mbuf_cnt);
    if (ret != DRV_ERROR_NONE) {
        mbuf_atomlock_unlock(&mbufChainHead->lock);
        buff_err("mbuf chain target get tail err\n");
        return (int)ret;
    }
    if ((chain_cnt + mbuf_cnt) > MBUF_CHAIN_MAX_LEN) {
        mbuf_atomlock_unlock(&mbufChainHead->lock);
        buff_err("mbuf total len is out of range, head: %p, len: %u, head: %p, len: %u, range: %u\n", mbufChainHead,
                 chain_cnt, mbuf, mbuf_cnt, MBUF_CHAIN_MAX_LEN);
        return (int)DRV_ERROR_OVER_LIMIT;
    }

    prev_share_mbuf = get_share_mbuf_by_mbuf(prev);
    s_mbuf = get_share_mbuf_by_mbuf(mbuf);

    prev->next = mbuf;
    prev->next_blk_id = mbuf->blk_id;
    mbuf->prev = prev;
    mbuf->prev_blk_id = prev->blk_id;

    prev_share_mbuf->next = s_mbuf;
    prev_share_mbuf->next_blk_id = s_mbuf->blk_id;
    s_mbuf->prev = prev_share_mbuf;
    s_mbuf->prev_blk_id = prev_share_mbuf->blk_id;

    mbuf_atomlock_unlock(&mbufChainHead->lock);

    return (int)DRV_ERROR_NONE;
}

static drvError_t mbuf_chain_free(struct Mbuf *mbuf_chain_head, int type)
{
    struct Mbuf *tail = NULL;
    struct Mbuf *tmp = NULL;
    unsigned int cnt = 0;
    drvError_t ret;
    int sub_ret;

    sub_ret = mbuf_atomlock_lock(&mbuf_chain_head->lock);
    if (sub_ret != 0) {
        buff_err("Apply atomic_lock error. (ret=%d)\n", sub_ret);
        return DRV_ERROR_INNER_ERR;
    }
    if (mbuf_chain_head_is_invalid(mbuf_chain_head)) {
        mbuf_atomlock_unlock(&mbuf_chain_head->lock);
        buff_err("mbuf chain head invalid! \n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = mbuf_chain_get_tail(mbuf_chain_head, &tail, &cnt);
    if (ret != DRV_ERROR_NONE) {
        mbuf_atomlock_unlock(&mbuf_chain_head->lock);
        buff_err("mbuf chain get tail\n");
        return ret;
    }

    while ((tail->prev != NULL) && (cnt > 0)) {
        tmp = tail;
        tail = tmp->prev;
        ret = mbuf_free_one_buf(tmp, type);
        if (ret != DRV_ERROR_NONE) {
            buff_err("mbuf chain head: 0x%llx, index: %u from end, pointer: 0x%llx is invalid! ret: %d\n",
                     (unsigned long long)(uintptr_t)mbuf_chain_head, cnt, (unsigned long long)(uintptr_t)tmp, ret);
            mbuf_atomlock_unlock(&mbuf_chain_head->lock);
            return ret;
        }
        cnt--;
    }

    mbuf_atomlock_unlock(&mbuf_chain_head->lock);
    ret = mbuf_free_one_buf(mbuf_chain_head, type);
    if (ret != DRV_ERROR_NONE) {
        buff_err("mbuf chain free err, head: %p, is invalid! ret: %d\n", mbuf_chain_head, ret);
        return ret;
    }
    return DRV_ERROR_NONE;
}

drvError_t mbuf_free_with_opt_type(struct Mbuf *mbuf, int type)
{
    if ((mbuf == NULL) || (get_share_mbuf_by_mbuf(mbuf) == NULL) || (mbuf_is_invalid(mbuf) != 0)) {
        buff_err("Input para is invalid.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    return mbuf_chain_free(mbuf, type);
}

int halMbufFree(Mbuf *mbuf)
{
    return (int)mbuf_free_with_opt_type(mbuf, MBUF_FREE_BY_USER);
}

STATIC drvError_t mbuf_free_data_block(void *buff, unsigned long size, struct share_mbuf *s_mbuf)
{
    struct uni_buff_head_t *head = NULL;
    drvError_t ret;

    ret = buff_range_get(s_mbuf->data_blk_id, buff, size);
    if (ret != DRV_ERROR_NONE) {
        buff_warn("Buff has been free. (buff=%p)\n", buff);
        return DRV_ERROR_NONE;
    }

    if (buff_verify_and_get_head(buff, &head, s_mbuf->data_blk_id) != 0) {
        buff_range_put(s_mbuf->data_blk_id, buff);
        buff_warn("Mbuf data is invalid. (buff=%p)\n", buff);
        return DRV_ERROR_BAD_ADDRESS;
    }

    buff_trace(buff, s_mbuf, buff_api_getpid(), MBUF_FREE_BY_RECYCLE, -1);
    ret = buff_free(s_mbuf->data_blk_id, buff, head);
    buff_put_head(head, s_mbuf->data_blk_id);
    buff_range_put(s_mbuf->data_blk_id, buff);
    return ret;
}

drvError_t s_mbuf_free(void *mp, struct share_mbuf *s_mbuf)
{
    void *datablock = s_mbuf->datablock;
    /* free mbuf data */
    if (datablock != NULL) {
        struct uni_buff_head_t *mbuf_head = buff_mempool_get_head((void *)s_mbuf);
        if (mbuf_head->buff_type != MBUF_BARE_BUFF) {
            drvError_t ret;
            ret = mbuf_free_data_block(datablock, s_mbuf->total_len, s_mbuf);
            if (ret != 0) {
                buff_warn("Mbuf free datablock not success. (ret=%d)\n", ret);
            }
        }
        s_mbuf->data = NULL;
        s_mbuf->datablock = NULL;
    } else {
        buff_warn("Mbuf data is NULL. (mbuf=%p)\n", s_mbuf);
    }

    (void)mp_free_buff(mp, s_mbuf);
    mbuf_trace(s_mbuf, MBUF_FREE_BY_RECYCLE);
    return DRV_ERROR_NONE;
}

static drvError_t buff_verify(struct uni_buff_head_t *head, uint32_t blk_id)
{
    struct uni_buff_tail_t *tail = NULL;

    if (head == NULL) {
        buff_err("head is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (head->image != UNI_HEAD_IMAGE) {
        buff_err("head image check failed, head image:0x%x.\n", head->image);
        return DRV_ERROR_INVALID_VALUE;
    }

    tail = buff_get_tail(head, blk_id);
    if (tail == NULL) {
        buff_err("Tail is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (tail->image != UNI_TAIL_IMAGE) {
        buff_err("Tail image check failed. (image=0x%x)\n", tail->image);
        buff_put_tail(tail, blk_id);
        return DRV_ERROR_INVALID_VALUE;
    }
    buff_put_tail(tail, blk_id);

    return DRV_ERROR_NONE;
}

static drvError_t mbuf_verify_single_mbuf(struct Mbuf *mbuf)
{
    struct uni_buff_head_t *s_mbuf_head = NULL;
    struct uni_buff_head_t *data_head = NULL;
    drvError_t ret;

    if (mbuf_verify_and_get_head((void *)get_share_mbuf_by_mbuf(mbuf), &s_mbuf_head) != 0) {
        buff_err("mbuf is invalid. (buf=%p)\n", mbuf);
        return DRV_ERROR_BAD_ADDRESS;
    }

    if (mbuf->buff_type != MBUF_BARE_BUFF) {
        data_head = buff_get_head((void *)mbuf->data, mbuf->data_blk_id);
        ret = buff_verify(data_head, mbuf->data_blk_id);
        if (ret != DRV_ERROR_NONE) {
            buff_err("mbuf_verify_single_mbuf mbuf_data check failed, mbuf data(%p), ret(%d).\n", mbuf->data, ret);
            buff_put_head(data_head, mbuf->data_blk_id);
            return ret;
        }
        buff_put_head(data_head, mbuf->data_blk_id);
    }

    return DRV_ERROR_NONE;
}

static drvError_t mbuf_verify_mbuf_chain(struct Mbuf *mbuf)
{
    struct uni_buff_head_t *data_head = NULL;
    struct Mbuf *tmp = NULL;
    unsigned int n = 0;

    tmp = mbuf;
    while (tmp != NULL) {
        drvError_t ret;

        if (n >= MBUF_CHAIN_MAX_LEN) {
            buff_err("mbuf chain is too long, max chain len: %u\n", MBUF_CHAIN_MAX_LEN);
            return DRV_ERROR_OVER_LIMIT;
        }

        if (mbuf_is_invalid(tmp) != 0) {
            buff_err("Mbuf is invalid. (tmp=%p)\n", tmp);
            return DRV_ERROR_BAD_ADDRESS;
        }

        if (tmp->buff_type != MBUF_BARE_BUFF) {
            data_head = buff_get_head(tmp->data, tmp->data_blk_id);
            ret = buff_verify(data_head, tmp->data_blk_id);
            if (ret != DRV_ERROR_NONE) {
                buff_err("mbuf_verify_mbuf_chain mbuf_data check failed, mbuf data(%p), ret(%d).\n", tmp->data, ret);
                buff_put_head(data_head, tmp->data_blk_id);
                return ret;
            }
            buff_put_head(data_head, tmp->data_blk_id);
        }
        tmp = tmp->next;
        n++;
    }

    return DRV_ERROR_NONE;
}

int halMbufVerify(Mbuf *mbuf, unsigned int type)
{
    if ((mbuf == NULL) || (get_share_mbuf_by_mbuf(mbuf) == NULL)) {
        buff_err("hal_mbuf_verify input parameter error: mbuf(%p) type(%u)!\n", mbuf, type);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((type >= MBUF_VERIFY_TYPE_MAX) || (g_mbuf_verify_handle[type] == NULL)) {
        return DRV_ERROR_NOT_SUPPORT;
    }
    return (int)g_mbuf_verify_handle[type](mbuf);
}

int halMbufChainGetMbufNum(Mbuf *mbufChainHead, unsigned int *num)
{
    struct Mbuf *tail = NULL;
    unsigned int cnt = 0;
    int ret;

    if ((mbufChainHead == NULL) || (get_share_mbuf_by_mbuf(mbufChainHead) == NULL) || num == NULL) {
        buff_err("Input para is invalid!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf_is_invalid(mbufChainHead) != DRV_ERROR_NONE) {
        buff_err("mbuf invalid!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = mbuf_atomlock_lock(&mbufChainHead->lock);
    if (ret != 0) {
        buff_err("Apply atomic_lock error. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    if (mbuf_chain_head_is_invalid(mbufChainHead)) {
        mbuf_atomlock_unlock(&mbufChainHead->lock);
        return DRV_ERROR_INVALID_VALUE;
    }
    ret = (int)mbuf_chain_get_tail(mbufChainHead, &tail, &cnt);
    mbuf_atomlock_unlock(&mbufChainHead->lock);
    if (ret != 0) {
        return ret;
    }
    *num = cnt;

    return DRV_ERROR_NONE;
}

int halMbufChainGetMbuf(Mbuf *mbufChainHead, unsigned int index, Mbuf **mbuf)
{
    struct Mbuf *tmp = mbufChainHead;
    unsigned int cnt = 0;
    int ret;

    if ((mbufChainHead == NULL) || (get_share_mbuf_by_mbuf(mbufChainHead) == NULL) || mbuf == NULL) {
        buff_err("num is NULL!\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mbuf_is_invalid(mbufChainHead) != 0) {
        buff_err("mbuf chain head invalid! head: 0x%llx\n", (unsigned long long)(uintptr_t)mbufChainHead);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (index >= MBUF_CHAIN_MAX_LEN) {
        buff_err("index is out number mbuf chain max len. (index=%u, max_len=%u)\n", index, MBUF_CHAIN_MAX_LEN);
        return DRV_ERROR_INVALID_VALUE;
    }
    ret = mbuf_atomlock_lock(&mbufChainHead->lock);
    if (ret != 0) {
        buff_err("Apply atomic_lock error. (ret=%d)\n", ret);
        return DRV_ERROR_INNER_ERR;
    }
    if (mbuf_chain_head_is_invalid(mbufChainHead)) {
        mbuf_atomlock_unlock(&mbufChainHead->lock);
        return DRV_ERROR_INVALID_VALUE;
    }

    while (tmp != NULL) {
        if (mbuf_is_invalid(tmp) != DRV_ERROR_NONE) {
            buff_err("mbuf chain head: 0x%llx, index: %u, pointer: 0x%llx is invalid!\n",
                     (unsigned long long)(uintptr_t)mbufChainHead, cnt, (unsigned long long)(uintptr_t)tmp);
            mbuf_atomlock_unlock(&mbufChainHead->lock);
            return DRV_ERROR_INVALID_VALUE;
        }
        if (cnt == index) {
            *mbuf = tmp;
            mbuf_atomlock_unlock(&mbufChainHead->lock);
            return DRV_ERROR_NONE;
        }
        tmp = tmp->next;
        cnt++;
        if (cnt >= MBUF_CHAIN_MAX_LEN) {
            buff_err("mbuf chain head: 0x%llx, index: %u, pointer: 0x%llx is invalid!\n",
                     (unsigned long long)(uintptr_t)mbufChainHead, cnt, (unsigned long long)(uintptr_t)tmp);
            mbuf_atomlock_unlock(&mbufChainHead->lock);
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    mbuf_atomlock_unlock(&mbufChainHead->lock);
    buff_err("mbuf chain get mbuf err, chain head: %p, max cnt: %u, index: %u out of range\n", mbufChainHead, cnt,
             index);

    return DRV_ERROR_PARA_ERROR;
}

drvError_t mbuf_free_for_queue(Mbuf *mbuf, int type)
{
    return mbuf_free_with_opt_type(mbuf, type);
}

static void mbuf_update(struct share_mbuf *s_mbuf, void *datablock, int pid, uint32 enque_flag, unsigned int qid)
{
    if (enque_flag == UNI_BUFF_REFERENCE_FLAG_ENABLE) {
        mbuf_trace(s_mbuf, MBUF_ENQUEUE);
        buff_trace(datablock, s_mbuf, pid, MBUF_ENQUEUE, (int)qid);
    } else {
        struct uni_buff_ext_info *info = mbuf_get_ext_info(s_mbuf);
        info->use_pid = pid;
        info->process_uni_id = buff_get_process_uni_id();
        mbuf_trace(s_mbuf, MBUF_DEQUEUE);
        buff_trace(datablock, s_mbuf, pid, MBUF_DEQUEUE, (int)qid);
    }
}

static void mbuf_owner_update(struct Mbuf *mbuf, int pid, uint32 enque_flag, unsigned int qid)
{
    struct Mbuf *tmp = mbuf;
    unsigned int n = 0;

    if (mbuf_chain_head_is_invalid(mbuf)) {
        buff_err("Mbuf chain head is invalid.\n");
        return;
    }
    while (tmp != NULL) {
        if (n >= MBUF_CHAIN_MAX_LEN) {
            buff_err("Mbuf chain is too long. (max chain len: %u)\n", MBUF_CHAIN_MAX_LEN);
            return;
        }
        mbuf_update(get_share_mbuf_by_mbuf(tmp), tmp->datablock, pid, enque_flag, qid);
        tmp = tmp->next;
        n++;
    }
    return;
}

void mbuf_owner_update_for_enque(struct Mbuf *mbuf, int pid, unsigned int qid)
{
    mbuf_owner_update(mbuf, pid, UNI_BUFF_REFERENCE_FLAG_ENABLE, qid);
}

void mbuf_owner_update_for_deque(struct Mbuf *mbuf, int pid, unsigned int qid)
{
    mbuf_owner_update(mbuf, pid, UNI_BUFF_REFERENCE_FLAG_DISABLE, qid);
}

static void mbuf_trace(struct share_mbuf *s_mbuf, int opt_type)
{
    unsigned int idx = s_mbuf->record_cur_idx;
    idx = (idx >= MAX_MBUF_RECORD_NUM) ? 0 : idx;
    s_mbuf->record_pid[idx] = buff_api_getpid();
    s_mbuf->record_opt[idx] = (unsigned char)opt_type;
    if (s_mbuf->record_total_num < MAX_MBUF_RECORD_NUM) {
        s_mbuf->record_total_num++;
    }
    s_mbuf->record_cur_idx = idx + 1;
}

static void mbuf_trace_print(struct share_mbuf *s_mbuf)
{
    uint32_t max_loop_times = s_mbuf->record_total_num;
    uint32_t id = s_mbuf->record_cur_idx;
    uint32_t i;

    if (id > MAX_MBUF_RECORD_NUM) {
        return;
    }
    max_loop_times = (max_loop_times > MAX_MBUF_RECORD_NUM) ? MAX_MBUF_RECORD_NUM : max_loop_times;
    for (i = 0; i < max_loop_times; i++) {
        id--;
        if (id > MAX_MBUF_RECORD_NUM) {
            id = MAX_MBUF_RECORD_NUM - 1;
        }
        buff_err("Mbuf trace. (mbuf=%p; idx=%d; pid=%d, operation=%u)\n", s_mbuf, id, s_mbuf->record_pid[id],
                 s_mbuf->record_opt[id]);
    }
}

drvError_t buff_set_mbuf_timestamp(struct buff_config_handle_arg *para_in)
{
    struct Mbuf *mbuf = (struct Mbuf *)para_in->data;
    struct share_mbuf *s_mbuf = get_share_mbuf_by_mbuf(mbuf);
    drvError_t ret;

    ret = mbuf_is_invalid(mbuf);
    if (ret != 0) {
        buff_err("Mbuf is invalid. (ret=%d)\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    s_mbuf->timestamp = buff_get_cur_timestamp();

    return DRV_ERROR_NONE;
}

unsigned long long mbuf_get_timestamp(struct Mbuf *mbuf)
{
    struct share_mbuf *s_mbuf = get_share_mbuf_by_mbuf(mbuf);

    return s_mbuf->timestamp;
}
