/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "mmpa_api.h"
#include "securec.h"
#include "ascend_hal.h"
#include "hdc_cmn.h"
#include "hdc_cfg_parse.h"

signed int is_digit(const char str[], signed int len)
{
    signed int i;
    signed int str_len;

    if ((str == NULL) || (len < 1) || (strlen(str) == 0) || (strlen(str) >= MAX_VALUE_NUM)) {
        HDC_LOG_ERR("Input parameter is error.\n");
        return false;
    }
    str_len = (signed int)strlen(str);
    for (i = 0; i < str_len; i++) {
        if (str[i] < '0' || str[i] > '9') {
            HDC_LOG_INFO("Can't convert string to number.\n");
            return false;
        }
    }
    return true;
}

STATIC void str_convert(char *str, signed int len)
{
    signed int str_len;
    signed int i = 0;
    signed int j;
    signed int k;
    bool flag = false;

    if (str == NULL) {
        return;
    }

    str_len = (signed int)strlen(str);
    if ((str_len < 1) || (str_len >= len)) {
        return;
    }
    flag = (str[i] == ' ') || (str[i] == '\t') || (str[i] == '\r') || (str[i] == '\n');

    while (flag) {
        i++;
        flag = (str[i] == ' ') || (str[i] == '\t') || (str[i] == '\r') || (str[i] == '\n');
    }

    j = str_len - 1;
    while ((j > 0) && ((str[j] == ' ') || (str[j] == '\t') || (str[j] == '\r') || (str[j] == '\n'))) {
        j--;
    }

    if (i > j) {
        *str = 0;
        return;
    }

    if (((str_len - 1) == j) && (i == 0)) {
        return;
    }

    for (k = 0; k <= j - i; k++) {
        str[k] = str[(long)k + i];
    }
    str[k] = 0;
}

STATIC void comment_trim(char *str, signed int len)
{
    signed int i = 0;
    signed int str_len;
    bool uc_quote_flag = false;

    if (str == NULL) {
        return;
    }

    str_len = (signed int)strlen(str);
    if ((str_len < 1) || (str_len >= len)) {
        return;
    }

    while ((i < str_len) &&
           ((uc_quote_flag == true) ||
            ((str[i] != '#') && (!((i < str_len - 1) && (str[i] == '/') && (str[(long)i + 1] == '/')))))) {
        if (str[i] == '"') {
            uc_quote_flag = !uc_quote_flag;
        }
        i++;
    }
    str[i] = 0;
}

STATIC void destroy_cfg_parse_cb(PCFGPARSE_CB_T pCB)
{
    signed int i;

    if (pCB == NULL) {
        HDC_LOG_ERR("Input parameter is error.\n");
        return;
    }

    if (pCB->fp != NULL) {
        (void)fclose(pCB->fp);
        pCB->fp = NULL;
    }

    for (i = 0; i < MAX_VALUE_NUM; i++) {
        if (pCB->value[i] != NULL) {
            free(pCB->value[i]);
            pCB->value[i] = NULL;
        }
    }

    if (memset_s(pCB->file_path, MAX_FILE_NAME_SIZE, 0, MAX_FILE_NAME_SIZE) != 0) {
        HDC_LOG_ERR("Call memset_s error. (strerror=\"%s\")\n", strerror(errno));
    }

    free(pCB);
}

STATIC PCFGPARSE_CB_T create_cfg_parse_cb(void)
{
    signed int i;
    PCFGPARSE_CB_T p_cfg_parse_cb;

    p_cfg_parse_cb = (CFGPARSE_CB_T *)malloc(sizeof(CFGPARSE_CB_T));
    if (p_cfg_parse_cb == NULL) {
        HDC_LOG_ERR("Call malloc failed.\n");
        return NULL;
    }

    if (memset_s(p_cfg_parse_cb, sizeof(CFGPARSE_CB_T), 0, sizeof(CFGPARSE_CB_T)) != 0) {
        HDC_LOG_ERR("Call memset_s error. (strerror=\"%s\")\n", strerror(errno));
        free(p_cfg_parse_cb);
        p_cfg_parse_cb = NULL;
        return NULL;
    }

    for (i = 0; i < MAX_VALUE_NUM; i++) {
        p_cfg_parse_cb->value[i] = (char *)malloc(MAX_LINE_SIZE);
        if (p_cfg_parse_cb->value[i] == NULL) {
            HDC_LOG_ERR("Call malloc failed. (i=%d)\n", i);
            destroy_cfg_parse_cb(p_cfg_parse_cb);
            return NULL;
        }

        if (memset_s(p_cfg_parse_cb->value[i], MAX_LINE_SIZE, 0, MAX_LINE_SIZE) != 0) {
            HDC_LOG_ERR("Call memset_s error. (strerror=\"%s\")\n", strerror(errno));
            destroy_cfg_parse_cb(p_cfg_parse_cb);
            return NULL;
        }
    }
    return p_cfg_parse_cb;
}

void cfg_file_close(PCFGPARSE_CB_T handle)
{
    if (handle == NULL) {
        HDC_LOG_ERR("Input parameter is error.\n");
        return;
    }
    destroy_cfg_parse_cb(handle);
}

signed int cfg_file_open(const char *filename, PCFGPARSE_CB_T *handle)
{
    PCFGPARSE_CB_T p_cfg_parse_cb = NULL;
    char *path = NULL;

    path = (char *)malloc(PATH_MAX);
    if (path == NULL) {
        HDC_LOG_ERR("Call malloc failed.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    if ((strlen(filename) >= MAX_FILE_NAME_SIZE) || (realpath(filename, path) == NULL)) {
        goto ret;
    }

    p_cfg_parse_cb = create_cfg_parse_cb();
    if (p_cfg_parse_cb == NULL) {
        HDC_LOG_ERR("Call create_cfg_parse_cb failed.\n");
        goto ret;
    }

    if (strncpy_s(p_cfg_parse_cb->file_path, MAX_FILE_NAME_SIZE, filename, MAX_FILE_NAME_SIZE - 1) != EOK) {
        HDC_LOG_ERR("Call strncpy_s failed.\n");
        destroy_cfg_parse_cb(p_cfg_parse_cb);
        goto ret;
    }

    p_cfg_parse_cb->fp = fopen(path, "r");
    if (p_cfg_parse_cb->fp == NULL) {
        HDC_LOG_WARN("Open file not success. (path=\"%s\")\n", path);
        destroy_cfg_parse_cb(p_cfg_parse_cb);
        goto ret;
    }

    *handle = p_cfg_parse_cb;
    free(path);
    path = NULL;
    return 0;
ret:
    free(path);
    path = NULL;
    return DRV_ERROR_INVALID_HANDLE;
}

STATIC char *str_to_ok(char *str, const char *delim, char **saveptr, signed int len)
{
    (void)len;
    return strtok_r(str, delim, saveptr);
}

STATIC signed int Search(PCFGPARSE_CB_T handle, const char *str, char **value, signed int *value_num)
{
    char tmp[MAX_LINE_SIZE];
    char *pvalue[MAX_VALUE_NUM];
    const char *var_sep = ",";
    const char *separator = "=";
    char *p_save = NULL;
    signed int i = 0;

    while ((memset_s(tmp, MAX_LINE_SIZE, 0, MAX_LINE_SIZE) == 0) && (fgets(tmp, MAX_LINE_SIZE, handle->fp) != NULL)) {
        tmp[MAX_LINE_SIZE - 1] = '\0';
        /* Trim leading/trailing spaces, tabs, carriage returns and newlines */
        str_convert(tmp, MAX_LINE_SIZE);
        /* Remove inline comments from each line */
        comment_trim(tmp, MAX_LINE_SIZE);

        /* If the entire line is a comment, the string length will be 0 after trimming */
        if (strlen(tmp) == 0) {
            continue;
        }

        /* Check if the line contains the target string */
        if (strstr(tmp, str) == NULL) {
            continue;
        }

        /* Extract the part before the first separator */
        if ((pvalue[0] = str_to_ok(tmp, separator, &p_save, MAX_LINE_SIZE)) == NULL) {
            continue;
        }

        /* Trim leading/trailing spaces and tabs from the keyword */
        str_convert(pvalue[0], MAX_LINE_SIZE);

        if (strcmp(pvalue[0], str) != 0) {
            continue;
        }
        while ((pvalue[i] = str_to_ok(NULL, var_sep, &p_save, 0)) != NULL) {
            value[i] = handle->value[i];
            str_convert(pvalue[i], MAX_LINE_SIZE);
            if (memcpy_s(value[i], MAX_LINE_SIZE, pvalue[i], strlen(pvalue[i]) + 1) != EOK) {
                return 0;
            }
            *value_num = i + 1;

            if (*value_num >= MAX_VALUE_NUM) {
                HDC_LOG_ERR("Parameter value_num is out of range. (value_num=%d, max=%d)\n", *value_num, MAX_VALUE_NUM);
                break;
            }
            i++;
        }
        return (int)ftell(handle->fp);
    }
    return 0;
}

signed int str_search(PCFGPARSE_CB_T handle, const char *str, char **value, signed int *value_num)
{
    signed int j = 0;

    if ((value == NULL) || (value_num == NULL) || (handle == NULL)) {
        HDC_LOG_ERR("Input parameter is error.\n");
        return 0;
    }

    if (strlen(str) < 1) {
        HDC_LOG_ERR("Input parameter str length is error.\n");
        return 0;
    }

    /* Initialize buffer space */
    for (j = 0; j < MAX_VALUE_NUM; j++) {
        if (memset_s(handle->value[j], MAX_LINE_SIZE, 0, MAX_LINE_SIZE) != 0) {
            HDC_LOG_ERR("Call memset_s error. (strerror=\"%s\")\n", strerror(errno));
            return 0;
        }
    }

    /* Read from the beginning of the file */
    (void)fseek(handle->fp, 0, SEEK_SET);
    /* Initialize value count to 0 */
    *value_num = 0;

    return Search(handle, str, value, value_num);
}
