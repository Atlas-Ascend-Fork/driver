/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef UT_TEST
#include <stdlib.h>

#include "ascend_hal.h"
#include "securec.h"

#include "prof_common.h"
#include "prof_adapt.h"
#include "prof_buff.h"

#define PROF_DATA_HEAD_RSV_LENGTH 1021
struct prof_data_head {
    uint32_t read_ptr;
    uint32_t write_ptr;
    uint32_t data_buf_len;
    uint32_t rsv[PROF_DATA_HEAD_RSV_LENGTH]; /* align 4kb */
};

drvError_t prof_buff_init(uint32_t chan_id, uint8_t **buff)
{
    uint8_t *buff_addr = NULL;
    struct prof_data_head *data_head = NULL;
    uint32_t buff_len = 0x400000; /* 0x400000: 4MB */
    size_t alignment = 0x1000;    /* align 4kb */
    int ret;

    if (chan_id == CHANNEL_AICPU) {
        buff_len = 0x800000;
    }

    ret = posix_memalign((void **)&buff_addr, alignment, buff_len);
    if (ret != 0) {
        PROF_ERR("Failed to alloc mem. (chan_id=%u, ret=%d)\n", chan_id, ret);
        return DRV_ERROR_MALLOC_FAIL;
    }
    (void)memset_s(buff_addr, buff_len, 0, buff_len);

    data_head = (struct prof_data_head *)buff_addr;
    data_head->write_ptr = 0;
    data_head->read_ptr = 0;
    data_head->data_buf_len = buff_len - (uint32_t)sizeof(struct prof_data_head);

    *buff = buff_addr;
    return DRV_ERROR_NONE;
}

void prof_buff_uninit(uint8_t **buff)
{
    if (*buff != NULL) {
        free(*buff);
        *buff = NULL;
    }
}

drvError_t prof_buff_write(uint8_t *buff, void *data, uint32_t data_len)
{
    struct prof_data_head *data_head = (struct prof_data_head *)buff;
    uint8_t *dest_base = NULL, *src_base = NULL;
    uint32_t dest_len, src_len;
    int ret;

    dest_base = (uint8_t *)data_head + sizeof(struct prof_data_head) + data_head->write_ptr;
    dest_len = data_head->data_buf_len - data_head->write_ptr;
    src_base = data;
    src_len = data_len;

    if (dest_len < src_len) {
        ret = memcpy_s(dest_base, dest_len, src_base, dest_len);
        if (ret != 0) {
            PROF_ERR("Failed to copy. (ret=%d, dest_len=%u, src_len=%u)\n", ret, dest_len, src_len);
            return DRV_ERROR_MEMORY_OPT_FAIL;
        }

        src_base += dest_len;
        src_len -= dest_len;
        /* write_ptr flip */
        ATOMIC_SET(&data_head->write_ptr, 0);
        dest_base = (uint8_t *)data_head + sizeof(struct prof_data_head);
        dest_len = data_head->data_buf_len;
    }

    ret = memcpy_s(dest_base, dest_len, src_base, src_len);
    if (ret != 0) {
        PROF_ERR("Failed to copy. (ret=%d, dest_len=%u, src_len=%u)\n", ret, dest_len, src_len);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    ATOMIC_SET(&data_head->write_ptr, (data_head->write_ptr + src_len) % data_head->data_buf_len);

    return DRV_ERROR_NONE;
}

STATIC drvError_t prof_buff_copy(struct prof_data_head *data_head, char *out_buf, uint32_t buf_size, uint32_t data_len)
{
    uint8_t *data_base = NULL;
    int ret;

    data_base = (uint8_t *)data_head + sizeof(struct prof_data_head) + data_head->read_ptr;
    ret = memcpy_s(out_buf, buf_size, data_base, data_len);
    if (ret != 0) {
        PROF_ERR("Failed to copy. (ret=%d, buf_size=%u, data_len=%u)\n", ret, buf_size, data_len);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }

    ATOMIC_SET(&data_head->read_ptr, (data_head->read_ptr + data_len) % data_head->data_buf_len);
    return DRV_ERROR_NONE;
}

int prof_buff_read(uint8_t *buff, char *out_buf, uint32_t buf_size)
{
    struct prof_data_head *data_head = (struct prof_data_head *)buff;
    uint32_t data_len, copied_len;
    drvError_t ret;

    /* Scenario 1: the buff is empty */
    if (data_head->read_ptr == data_head->write_ptr) {
        return 0;
    }

    /* Scenario 2: read_ptr < write_ptr, read once */
    if (data_head->read_ptr < data_head->write_ptr) {
        data_len = data_head->write_ptr - data_head->read_ptr;
        data_len = (data_len > buf_size) ? buf_size : data_len;
        ret = prof_buff_copy(data_head, out_buf, buf_size, data_len);
        if (ret != DRV_ERROR_NONE) {
            return PROF_ERROR;
        }

        return (int)data_len;
    }

    /* Scenario 3: read_ptr > write_ptr, segmented reading */
    data_len = data_head->data_buf_len - data_head->read_ptr;
    data_len = (data_len > buf_size) ? buf_size : data_len;
    ret = prof_buff_copy(data_head, out_buf, buf_size, data_len);
    if (ret != DRV_ERROR_NONE) {
        return PROF_ERROR;
    }

    if ((data_len >= buf_size) || (data_head->write_ptr == 0)) {
        return (int)data_len;
    }

    copied_len = data_len;
    data_len = data_head->write_ptr;
    data_len = ((data_len + copied_len) > buf_size) ? (buf_size - copied_len) : data_len;
    ret = prof_buff_copy(data_head, out_buf + copied_len, buf_size - copied_len, data_len);
    if (ret != DRV_ERROR_NONE) {
        return (int)copied_len;
    }

    return (int)(data_len + copied_len);
}

uint32_t prof_buff_get_data_len(uint8_t *buff)
{
    struct prof_data_head *data_head = (struct prof_data_head *)buff;
    uint32_t write_ptr = data_head->write_ptr;
    uint32_t read_ptr = data_head->read_ptr;

    if (write_ptr >= read_ptr) {
        return (write_ptr - read_ptr);
    }

    return (data_head->data_buf_len - read_ptr + write_ptr);
}

uint32_t prof_buff_get_avail_len(uint8_t *buff)
{
    struct prof_data_head *data_head = (struct prof_data_head *)buff;

    /* r-w ptr need a position */
    return (data_head->data_buf_len - prof_buff_get_data_len(buff) - 1);
}

void *prof_buff_get_buf_addr(uint8_t *buff)
{
    return (void *)(buff + sizeof(struct prof_data_head));
}

uint32_t prof_buff_get_buf_size(uint8_t *buff)
{
    struct prof_data_head *data_head = (struct prof_data_head *)buff;
    return data_head->data_buf_len;
}

void *prof_buff_get_readptr_addr(uint8_t *buff)
{
    struct prof_data_head *data_head = (struct prof_data_head *)buff;
    return (void *)&data_head->read_ptr;
}

uint32_t prof_buff_get_readptr_size(uint8_t *buff)
{
    struct prof_data_head *data_head = (struct prof_data_head *)buff;
    return sizeof(data_head->read_ptr);
}

uint32_t prof_buff_get_writeptr(uint8_t *buff)
{
    struct prof_data_head *data_head = (struct prof_data_head *)buff;
    return data_head->write_ptr;
}

void prof_buff_update_writeptr(uint8_t *buff, uint32_t write_ptr)
{
    struct prof_data_head *data_head = (struct prof_data_head *)buff;

    if (write_ptr >= data_head->data_buf_len) {
        PROF_ERR("Invalid write_ptr.(write_ptr=%u,data_buf_len=%u)\n", write_ptr, data_head->data_buf_len);
        return;
    }

    ATOMIC_SET(&data_head->write_ptr, write_ptr);
}

void prof_buff_wait_read_empty(uint8_t *buff, uint32_t dev_id, uint32_t chan_id)
{
    struct prof_adapt_core_notifier *notifier = prof_adapt_get_notifier();
    uint32_t data_len, wait_num = 0;

    data_len = prof_buff_get_data_len(buff);
    if (data_len == 0) {
        return;
    }

    notifier->poll_report(dev_id, chan_id);

    while ((data_len != 0) && (wait_num < 1000)) { /* 1000 */
        (void)usleep(1000);                        /* 1000 us */
        wait_num++;
        data_len = prof_buff_get_data_len(buff);
    }

    if (data_len != 0) {
        PROF_WARN("Waiting for read timeout. (dev_id=%u, chan_id=%u, data_len=%u, wait_num=%u)\n", dev_id, chan_id,
                  data_len, wait_num);
        return;
    }

    PROF_INFO("Read completed. (dev_id=%u, chan_id=%u, wait_num=%u)\n", dev_id, chan_id, wait_num);
}

#else
int prof_buff_ut_test(void)
{
    return 0;
}
#endif
