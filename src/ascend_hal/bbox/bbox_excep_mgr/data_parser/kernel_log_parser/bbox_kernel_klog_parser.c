/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bbox_kernel_log_parser.h"
#include "bbox_kernel_klog_parser.h"

#include "securec.h"
#include "bbox_print.h"

/**
 * @brief       : get index of next message
 * @param [in]  : const char *buffer
 * @param [in]  : u64 size buffer size
 * @param [in]  : u64 idx buffer index
 * @return      : next buffer index, 0 for the start of the ring buffer
 */
STATIC u64 bbox_klog_next(const char *buffer, u64 size, u64 idx)
{
    if (size <= (idx + sizeof(struct printk_log))) {
        return 0;
    }

    const struct printk_log *msg = (const struct printk_log *)(buffer + idx);
    if (msg->len == 0) {
        return 0;
    }

    return idx + msg->len;
}

/**
 * @brief       check whether message is valid
 * @param [in]  length: max length
 * @param [in]  mng:    kernel print management info
 * @return      0: success, other: failed
 */
STATIC bbox_status bbox_klog_mng_check(u64 length, const struct printk_management *mng)
{
    BBOX_CHK_NULL_PTR(mng, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(length == 0, return BBOX_FAILURE, "%llu", length);

    if ((mng->printk_start == mng->printk_next) || (mng->printk_start > length) || (mng->printk_next > length) ||
        (mng->printk_buf_size > length) || (mng->printk_buf_size == 0) ||
        (mng->printk_buf_size < sizeof(struct printk_log))) {
#ifndef BBOX_UT_TEST
        BBOX_WAR("invalid param, start_gt[%d], end_gt[%d], start_end_equal[%d], buf_size[0x%llx], length[0x%llx]",
                 (int)(mng->printk_start > length), (int)(mng->printk_next > length),
                 (int)(mng->printk_start == mng->printk_next), mng->printk_buf_size, length);
#endif
        return BBOX_FAILURE;
    }
    return BBOX_SUCCESS;
}

/**
 * @brief       dump kernel message to file
 * @param [in]  msg:     kernel message
 * @param [in]  msg_len:  message length
 * @param [in]  log_path: path to write log
 * @return      saved message length on success, otherwise -1.
 */
STATIC s32 bbox_klog_msg_dump(const struct printk_log *msg, u32 msg_len, const char *log_path)
{
    const char *char_log;
    u64 ts_usec;

    BBOX_CHK_NULL_PTR(msg, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);

    if ((msg->text_len > msg_len) || ((msg_len - msg->text_len) < sizeof(struct printk_log))) {
        BBOX_WAR("invalid param, msg length[%u], text length[%u].", msg_len, msg->text_len);
        return BBOX_FAILURE;
    }

    // collect timestamp
    ts_usec = msg->ts_nsec / (u64)NSEC_PER_USEC;
    char_log = (const char *)msg + sizeof(struct printk_log);
    return bbox_klog_save_buf_to_fs(char_log, msg->text_len, ts_usec, log_path);
}

/**
 * @brief       Interface for dump kernel log into file-system.
 *              The kernel log data may be incorrect, resulting in interpretation failure.
 * @param [in]  buffer  The buffer of kernel log data.
 * @param [in]  len     The length of the buffer.
 * @param [in]  log_path The path for DDRDUMP module dump data into.
 * @return      0 on success otherwise -1
 */
bbox_status bbox_klog_save_fs_printk_info_log(const void *buffer, u32 len, const char *log_path)
{
    u64 head_size = (u64)sizeof(struct printk_area_head);
    const struct printk_area_head *head = (const struct printk_area_head *)buffer;
    const struct printk_management *mng = (struct printk_management *)(uintptr_t)head->manage_data;
    const char *buf_start = (const char *)buffer + head_size;
    u64 buf_size = mng->printk_buf_size;
    u64 log_start = mng->printk_start;
    u64 log_end = mng->printk_next;
    u64 total = 0;

    bbox_status ret = bbox_klog_mng_check((len - head_size), mng);
    BBOX_CHK_EXPR_CTRL(BBOX_WAR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "check klog msg unsuccessfully.");
    /*
     *      Memory distribution
     *      -----------------------------------------------------
     *      | PAGE_SIZE  RO                                     |
     *      -----------------------------------------------------
     *      | PAGE_SIZE  manage_data  (printk_management)        |
     *      -----------------------------------------------------
     *      | PAGE_SIZE  RO                                     |
     *      -----------------------------------------------------
     *      | Remaining  printk_log:                             |
     *      |            printk_log+char_log ......               |
     *      -----------------------------------------------------
     *      | PAGE_SIZE  RO                                     |
     *      -----------------------------------------------------
     */
    // prevent infinite loops
    const struct printk_log *msg = (const struct printk_log *)buf_start;
    BBOX_CHK_EXPR_CTRL(BBOX_WAR, msg->len == 0, return BBOX_FAILURE, "bad message length at offset 0.");

    u64 start = log_start;
    while (start != log_end) {
        if ((total > (buf_size - sizeof(struct printk_log))) || (start > (buf_size - sizeof(struct printk_log)))) {
            break;
        }
        msg = (const struct printk_log *)(buf_start + start);
        if (msg->len != 0) {
            if ((msg->len > (buf_size - total)) || (msg->len > (buf_size - start))) {
                break;
            }
            total += msg->len;
            s32 dump_len = bbox_klog_msg_dump(msg, msg->len, log_path);
            BBOX_CHK_EXPR_CTRL(BBOX_WAR, dump_len <= 0, return BBOX_FAILURE, "msg dump was not successful. ret [%d]",
                               dump_len);
        }

        // get next printk log entry
        start = bbox_klog_next(buf_start, buf_size, start);
        if ((start == log_start) || (start > buf_size)) {
#ifndef BBOX_USER_UT
            BBOX_ERR_CTRL(BBOX_WAR, return BBOX_FAILURE, "get bad klog next head eq[%d], gt[%d]",
                          (int)(start == log_start), (int)(start > buf_size));
#endif
        }
    }
    return BBOX_SUCCESS;
}

bool bbox_klog_is_printk_info_log(const char *manage_data)
{
    const struct printk_management *mng = (struct printk_management *)(uintptr_t)manage_data;
    if (mng->power_flag == PRINTK_MANAGEMENT_ENG_FLAG) {
        return true;
    }
    return false;
}
