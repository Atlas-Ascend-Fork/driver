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

#include "asdrv_ub_path.h"
#include "asdrv_ub_common.h"
#include "ka_fs_pub.h"
#include "ka_errno_pub.h"

STATIC char g_line_buf[UBDRV_STR_MAX_LEN] = {0};

STATIC loff_t ubvdrv_get_i_size_read(ka_file_t *p_file)
{
    return ka_fs_i_size_read(ka_fs_file_inode(p_file));
}

STATIC void ubvdrv_get_val(const char *env_buf, u32 buf_len, char *env_val, u32 val_len)
{
    const char *buf = env_buf;
    u32 i;
    *env_val = '\0';
    for (i = 0; (i < val_len) && (i < buf_len); i++) {
        if ((*buf == ' ') || (*buf == '\t') || (*buf == '\r') || (*buf == '\n') || (*buf == '\0')) {
            env_val[i] = '\0';
            break;
        }
        env_val[i] = *buf;
        buf++;
    }
    env_val[val_len - 1] = '\0';
}

STATIC u32 ubvdrv_get_env_value(char *env_buf, u32 buf_len, const char *env_name, char *env_val, u32 val_len)
{
    const char *buf = env_buf;
    u32 len;
    u32 offset = 0;

    if (*buf == '#') {
        return UBDRV_CONFIG_FAIL;
    }
    len = (u32)ka_base_strlen(env_name);
    if (ka_base_strncmp(buf, env_name, len) != 0) {
        return UBDRV_CONFIG_FAIL;
    }
    buf += len;
    offset += len;
    if ((*buf != ' ') && (*buf != '\t') && (*buf != '=')) {
        return UBDRV_CONFIG_FAIL;
    }
    while ((*buf == ' ') || (*buf == '\t')) {
        buf++;
        offset++;
    }
    if (*buf != '=') {
        ubdrv_err("Get value error.\n");
        return UBDRV_CONFIG_FAIL;
    }

    buf++;
    offset++;
    while ((*buf == ' ') || (*buf == '\t')) {
        buf++;
        offset++;
    }
    if (offset >= buf_len) {
        ubdrv_err("Buffer out of bound. (offset=%d;buf_len=%d)\n", offset, buf_len);
        return UBDRV_CONFIG_FAIL;
    }
    ubvdrv_get_val(buf, buf_len - offset, env_val, val_len);
    return UBDRV_CONFIG_OK;
}

STATIC char *ubvdrv_get_one_line(char *file_buf, u32 file_buf_len, char *line_buf_tmp, u32 buf_len)
{
    u32 i;
    u32 offset = 0;

    *line_buf_tmp = '\0';

    if (*file_buf == '\0') {
        return NULL;
    }
    while ((*file_buf == ' ') || (*file_buf == '\t')) {
        file_buf++;
        offset++;
    }

    for (i = 0; i < buf_len; i++) {
        if (*file_buf == '\n') {
            line_buf_tmp[i] = '\0';
            file_buf = file_buf + 1;
            offset += 1;
            goto exit;
        }
        if (*file_buf == '\0') {
            line_buf_tmp[i] = '\0';
            goto exit;
        }
        line_buf_tmp[i] = *file_buf;
        file_buf++;
        offset++;
    }
    line_buf_tmp[buf_len - 1] = '\0';
    file_buf = file_buf + 1;
    offset += 1;

exit:
    if (offset >= file_buf_len) {
        ubdrv_err("File buf out of bound. (offset=%d;file_buf_len=%d)\n", offset, file_buf_len);
        return NULL;
    }
    return file_buf;
}

STATIC int ubvdrv_get_env_value_from_buf(char *buf, u32 filesize, const char *env_name, char *env_val, u32 env_val_len)
{
    u32 ret, len;
    char *tmp_buf = NULL;
    char tmp_val[UBDRV_STR_MAX_LEN];
    int ret_val = 0;
    u32 match_flag = 0;

    tmp_buf = buf;
    while (1) {
        tmp_buf = ubvdrv_get_one_line(tmp_buf, filesize + 1, g_line_buf, (u32)sizeof(g_line_buf));
        if (tmp_buf == NULL) {
            break;
        }

        ret = ubvdrv_get_env_value(g_line_buf, UBDRV_STR_MAX_LEN, env_name, tmp_val, (u32)sizeof(tmp_val));
        if (ret != 0) {
            continue;
        }
        len = (u32)ka_base_strlen(tmp_val);
        if (env_val_len < (len + 1)) {
            ubdrv_err("Parameter env_val_len failed.\n");
            return -EINVAL;
        }
        ret_val = strcpy_s(env_val, (size_t)env_val_len, tmp_val);
        if (ret_val != 0) {
            ubdrv_err("strcpy_s failed. (ret=%d)\n", ret_val);
            return -EINVAL;
        }
        env_val[env_val_len - 1] = '\0';
        match_flag = 1;
        break;
    }

    return (match_flag == 1) ? 0 : -EINVAL;
}

STATIC int ubvdrv_read_cfg_file(char *file, char *buf, u32 buf_size)
{
    ka_file_t *fp = NULL;
    u32 len;
    ssize_t len_integer;
    int filesize;
    loff_t pos = 0;
    static int wait_cnt = 0;

retry:
    fp = ka_fs_filp_open(file, KA_O_RDONLY, 0);
    if (KA_IS_ERR_OR_NULL(fp)) {
        if (wait_cnt < UBDRV_OPEN_CFG_FILE_COUNT) {
            wait_cnt++;
            ka_system_msleep(UBDRV_OPEN_CFG_FILE_TIME_MS);
            goto retry;
        }
#ifndef CFG_PLATFORM_FPGA
        ubdrv_info("Can not open file. (file=\"%s\")\n", file);
#endif
        return -EIO;
    }

    filesize = (int)ubvdrv_get_i_size_read(fp);
    if (filesize >= (int)buf_size) {
        ubdrv_err("File is too large. (file=\"%s\";file_size=%d;max_size=%u)\n", file, filesize, buf_size);
        ka_fs_filp_close(fp, NULL);
        return -EIO;
    }
    len_integer = ka_fs_kernel_read(fp, buf, (ssize_t)(filesize), &pos);
    if (len_integer != (ssize_t)(filesize)) {
        ubdrv_err("Read file fail. (file_size=%d)\n", filesize);
        ka_fs_filp_close(fp, NULL);
        return -EIO;
    }

    len = (u32)len_integer;
    buf[len] = '\0';
    ka_fs_filp_close(fp, NULL);

    return filesize;
}

int ubdrv_get_env_value_from_file(char *file, const char *env_name, char *env_val, u32 env_val_len)
{
    int filesize, ret_val, ret;
    char *buf = NULL;

    buf = (char *)ubdrv_vzalloc(UBDRV_MAX_CFG_FILE_SIZE);
    if (buf == NULL) {
        ubdrv_err("Vzalloc failed\n");
        return UBDRV_CONFIG_FAIL;
    }

    filesize = ubvdrv_read_cfg_file(file, buf, UBDRV_MAX_CFG_FILE_SIZE);
    if (filesize < 0) {
        ret = UBDRV_CONFIG_FAIL;
        goto out;
    }

    ret_val = ubvdrv_get_env_value_from_buf(buf, (u32)filesize, env_name, env_val, env_val_len);
    if (ret_val != 0) {
        ret = UBDRV_CONFIG_NO_MATCH;
        goto out;
    }

    ret = UBDRV_CONFIG_OK;
out:
    ubdrv_vfree(buf);
    return ret;
}