/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bbox_utils.h"
#include <stdarg.h>
#include "securec.h"
#include "bbox_print.h"
#include "bbox_system_api.h"

/*
 * @brief       : transform time stamp to fixed format string.
 * @param [out] : char *s                   string buffer
 * @param [in]  : u32 max                   string buffer length
 * @param [in]  : const char *format        string format
 * @param [in]  : const time_t *timep       time stamp
 * @return      : <0 failure; >=0 success
 */
STATIC u32 bbox_str_fc_time(char *s, u32 max, const char *format, const time_t *timep)
{
    struct tm result = {0};

    BBOX_CHK_NULL_PTR(s, return 0);
    BBOX_CHK_NULL_PTR(timep, return 0);
    BBOX_CHK_NULL_PTR(format, return 0);

    bbox_status ret = bbox_local_time_r(timep, &result);
    if (ret != BBOX_SUCCESS) {
        BBOX_PERROR("localtime_r", "");
        BBOX_ERR_CTRL(BBOX_ERR, return 0, "get local time failed(time: %lld).", (s64)*timep);
    }

    BBOX_DBG("time: [%4d-%02d-%02d-%02d:%02d:%02d]", result.tm_year, result.tm_mon, result.tm_mday, result.tm_hour,
             result.tm_min, result.tm_sec);

    s32 ret_len = snprintf_s(s, max, max - 1U, format, result.tm_year, result.tm_mon, result.tm_mday, result.tm_hour,
                             result.tm_min, result.tm_sec);
    if (ret_len == -1) {
        BBOX_ERR("snprintf_s error");
        *s = '\0';
        ret_len = 0;
    }

    return (u32)ret_len;
}

/*
 * @brief       : transform device time stamp to string.
 * @param [in]  : const bbox_time_t *tm       the time stamp
 * @param [out] : char *buf                 return buf
 * @param [in]  : unsigned int len          buf length
 * @return      : BBOX_FAILURE; BBOX_SUCCESS
 */
bbox_status bbox_time_to_str(char *buf, u32 len, const bbox_time_t *tm)
{
    BBOX_CHK_NULL_PTR(tm, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buf, return BBOX_FAILURE);

    time_t sec = (time_t)tm->tv_sec;
    u32 ret_len = bbox_str_fc_time(buf, len, "%04d%02d%02d%02d%02d%02d", &sec);
    if (ret_len >= len) {
        BBOX_WAR("Extra length format(%u/%u).", ret_len, len);
        return BBOX_FAILURE;
    }
    // if nsec invalid, set it as 0
    u64 nsec = (tm->tv_nsec >= NSEC_PER_SEC) ? 0U : tm->tv_nsec;
    s32 ret = sprintf_s(&buf[ret_len], len - ret_len, "-%09llu", nsec);
    BBOX_DBG("time-str: %s", buf);
    return (ret > 0) ? BBOX_SUCCESS : BBOX_FAILURE;
}

#define USEC_MAX 999999

/**
 * @brief       transform host time stamp to string.
 * @param [out] buf:        return buffer
 * @param [in]  len:        buffer length
 * @param [in]  tv:         the time stamp
 * @return      void
 */
void bbox_host_time_to_str(char *buf, u32 len, struct timeval *tv)
{
    BBOX_CHK_NULL_PTR(tv, return);
    BBOX_CHK_NULL_PTR(buf, return);

    if ((tv->tv_usec < 0) || (tv->tv_usec > USEC_MAX)) {
        BBOX_ERR("Bad tv.usec. (tv_usec=%ld, valid_range=[0, %ld])", tv->tv_usec, (long)USEC_MAX);
        tv->tv_usec = 0;
    }

    time_t sec = (time_t)tv->tv_sec;
    u32 ret_len = bbox_str_fc_time(buf, len, "%04d-%02d-%02d-%02d:%02d:%02d", &sec);
    if (ret_len >= len) {
        BBOX_WAR("Extra length format(%u/%u).", ret_len, len);
        return;
    }

    s32 ret = sprintf_s(&buf[ret_len], len - ret_len, ".%06ld", tv->tv_usec);
    if (ret == -1) {
        BBOX_WAR("usec was not appended to time string(%06ld).", tv->tv_usec);
    }
    BBOX_DBG("host-time-str: %s", buf);
    return;
}

/**
 * @brief       transform host time stamp to string.
 * @param [out] buf:        return buffer
 * @param [in]  len:        buffer length
 * @param [in]  tv:         the time stamp
 * @return      void
 */
void bbox_log_time_to_str(char *buf, u32 len, struct timeval *tv)
{
    BBOX_CHK_NULL_PTR(tv, return);
    BBOX_CHK_NULL_PTR(buf, return);

    if ((tv->tv_usec < 0) || (tv->tv_usec > USEC_MAX)) {
        BBOX_ERR("Bad tv.usec. (tv_usec=%ld, valid_range=[0, %ld])", tv->tv_usec, (long)USEC_MAX);
        tv->tv_usec = 0;
    }

    time_t sec = (time_t)tv->tv_sec;
    u32 ret_len = bbox_str_fc_time(buf, len, "%04d-%02d-%02d %02d:%02d:%02d", &sec);
    if (ret_len >= len) {
        BBOX_WAR("Extra length format(%u/%u).", ret_len, len);
        return;
    }

    s32 ret = sprintf_s(&buf[ret_len], len - ret_len, ".%03ld", tv->tv_usec / NSEC_PER_USEC);
    if (ret == -1) {
        BBOX_WAR("usec was not appended to time string(%03ld).", tv->tv_usec / NSEC_PER_USEC);
    }
    BBOX_DBG("log-time-str: %s", buf);
}

/*
 * @brief       : transform time stamp to date string.
 * @param [in]  : const bbox_time_t *tm       the time stamp
 * @param [out] : char *date                return date array
 * @param [in]  : u32 len                   date array length
 * @return      : NA
 */
void bbox_get_date(const bbox_time_t *tm, char *date, u32 len)
{
    if (bbox_time_to_str(date, len, tm) == -1) {
        BBOX_WAR("Get date unsuccessfully");
    }
    return;
}

u32 bbox_get_seq_from_time(const char *time_str)
{
    s32 i;
    const s32 seq_len = 3; // last 3 numbers for sequence
    u32 seq_num = 0;
    const u32 base = 10;

    BBOX_CHK_NULL_PTR(time_str, return 0); // 0 for default if input is null
    if (strlen(time_str) != TMSTMP_LEN) {
        BBOX_DBG("Invalid bbox timestamp %s, time sequence start over", time_str);
        return 0;
    }
    for (i = TMSTMP_LEN - seq_len; i < TMSTMP_LEN; i++) {
        if ((time_str[i] < '0') || (time_str[i] > '9')) {
            BBOX_WAR("Invalid bbox timestamp, time sequence start over.");
            return 0;
        }
        seq_num = (seq_num * base) + (u32)(s32)(time_str[i] - '0');
    }
    return seq_num;
}

bbox_status bbox_data_init(struct bbox_data_info *info, size_t size)
{
    const size_t max_size = 0xA00000;
    BBOX_CHK_NULL_PTR(info, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(size == 0, return BBOX_FAILURE, "length is invalid: %zu.", size);
    BBOX_CHK_EXPR_ACTION(size >= max_size, return BBOX_FAILURE, "length is invalid: %zu(1 - %zu).", size, max_size);
    info->buffer = (char *)bbox_malloc(size);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, info->buffer == NULL, return BBOX_FAILURE, "malloc failed.");
    info->next = info->buffer;
    info->size = size;
    info->empty = size;
    info->used = 0;
    return BBOX_SUCCESS;
}

bbox_status bbox_data_reinit(struct bbox_data_info *info)
{
    BBOX_CHK_NULL_PTR(info, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(info->buffer, return BBOX_FAILURE);
    info->next = info->buffer;
    info->empty = info->size;
    info->used = 0;
    return BBOX_SUCCESS;
}

bbox_status bbox_data_clear(struct bbox_data_info *info)
{
    BBOX_CHK_NULL_PTR(info, return BBOX_FAILURE);
    bbox_free(info->buffer);
    info->buffer = NULL;
    info->next = NULL;
    info->size = 0;
    info->empty = 0;
    info->used = 0;
    return BBOX_SUCCESS;
}

bbox_status bbox_data_print(struct bbox_data_info *info, const char *fmt, ...)
{
    BBOX_CHK_NULL_PTR(info, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(fmt, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(info->buffer, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(info->next, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(info->empty == 0, return BBOX_FAILURE, "space is full.");

    va_list args;
    va_start(args, fmt);
    s32 ret = vsprintf_s(info->next, info->empty, fmt, args);
    va_end(args);
    if ((ret == -1) || ((u32)ret > info->empty)) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "vsprintf_s data buffer failed(%d)", ret);
    }

    size_t used = (u32)ret;
    info->used += used;
    info->empty -= used;
    info->next = &info->next[used];
    return BBOX_SUCCESS;
}
