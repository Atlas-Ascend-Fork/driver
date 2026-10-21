/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "bbox_tool_fs.h"
#include <stdbool.h>
#include "bbox_int.h"
#include "bbox_print.h"
#include "bbox_file_list.h"
#include "bbox_log_common.h"
#include "bbox_system_api.h"

#define DONE_FILE "DONE"

static const char *g_done_file_str[] = {
    // keep strings in same size
    "STARTING\n", // STORE_STARTING
    "COMPLETE\n", // STORE_COMPLETED
    "FILEDONE\n", // STORE_DONE
    "PROCFAIL\n", // STORE_FAILED
    NULL,
};

static inline bool bbox_file_is_writable(const char *file)
{
    bbox_stat64_t stat = {0};
    bbox_status ret = bbox_stat64(file, &stat);
    return (ret != BBOX_SUCCESS && ERRNO_IS_NOT_FOUND) || (ret == BBOX_SUCCESS && (stat.st_mode & M_IWUSR) != 0);
}

STATIC int bbox_check_path_perm(const char *path)
{
    bbox_stat64_t stat = {0};

    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(strnlen(path, DIR_MAXLEN) == 0, return BBOX_FAILURE, "Empty path.");

    if (bbox_stat64(path, &stat) != BBOX_SUCCESS) {
        BBOX_ERR("Stat failed. (path=%s, errno=%s)", path, strerror(errno));
        return BBOX_FAILURE;
    }

    if ((stat.st_mode & MMPA_DEFAULT_PIPE_PERMISSION) > DIR_CHECK_MODE) {
        BBOX_ERR("Directory permissions exceed maximum allowed. (path=%s, current_mode=%04o, max_mode=%04o)", path,
                 (stat.st_mode & MMPA_DEFAULT_PIPE_PERMISSION), DIR_CHECK_MODE);
        return BBOX_FAILURE;
    }

    if (stat.st_uid != 0) {
        BBOX_ERR("Directory owner must be root. (path=%s, current_uid=%u)", path, (unsigned int)stat.st_uid);
        return BBOX_FAILURE;
    }
    return BBOX_SUCCESS;
}

STATIC bbox_status bbox_check_path(const char *dir_path)
{
    bbox_status ret = BBOX_SUCCESS;
    u32 index = 0;
    char cur_path[DIR_MAXLEN] = {0};

    BBOX_CHK_NULL_PTR(dir_path, return BBOX_FAILURE);
    BBOX_CHK_EXPR_ACTION(strnlen(dir_path, DIR_MAXLEN) == 0, return BBOX_FAILURE, "Empty path.");

    char *abs_path = realpath(dir_path, NULL);
    if (abs_path == NULL) {
        BBOX_ERR("Realpath failed (path=%s, error=%s)", dir_path, strerror(errno));
        return BBOX_FAILURE;
    }

    ret = bbox_check_path_perm(OS_ROOT_DIR);
    if (ret != BBOX_SUCCESS) {
        bbox_free(abs_path);
        return ret;
    }

    const char *path = abs_path;
    while ((*path != '\0') && (ret == BBOX_SUCCESS)) {
        if (*path == OS_PATH_SPLIT) {
            cur_path[index] = '\0';
            if (index > 0) {
                ret = bbox_check_path_perm(cur_path);
            }
            while (*(path + 1) == OS_PATH_SPLIT)
                path++;
        }

        if (index >= (DIR_MAXLEN - 1)) {
            BBOX_ERR("Path too long. (max_len=%d, path=%s, cur_len=%u)", DIR_MAXLEN, abs_path, index);
            ret = BBOX_FAILURE;
            break;
        }
        cur_path[index++] = *path++;
    }

    if ((ret == BBOX_SUCCESS) && (index > 0) && (cur_path[index - 1] != OS_PATH_SPLIT)) {
        cur_path[index] = '\0';
        ret = bbox_check_path_perm(cur_path);
    }
    bbox_free(abs_path);
    return ret;
}

/**
 * @brief       : append(save) data to path.
 * @param [in]  : const char *log_path     path of save file.
 * @param [in]  : const char *file_name    filename
 * @param [in]  : const buff *buf         save data.
 * @param [in]  : u32  len                data length.
 * @param [in]  : u32  is_append           determine whether write with append
 * @return      : <0 failure; >=0 success
 */
s32 bbox_save_buf_to_fs(const char *log_path, const char *file_name, const buff *buf, u32 len, u32 is_append)
{
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(file_name, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buf, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(len == 0, return BBOX_FAILURE, "%u", len);

    char path[DIR_MAXLEN] = {0};
    u32 len_temp = len;
    u32 offset = 0;

    if ((strcmp(file_name, "vmcore") == 0) && (bbox_check_path(log_path) != BBOX_SUCCESS)) {
        return BBOX_FAILURE;
    }

    s32 ret = bbox_format_path(path, DIR_MAXLEN, log_path, file_name);
    if (ret == -1) {
        BBOX_ERR("format path with dir(%s) file(%s) failed.", log_path, file_name);
        return BBOX_FAILURE;
    }

    if (!bbox_file_is_writable(path)) {
        bbox_chmod(path, FILE_RW_MODE);
    }

    s32 flags = O_CREAT | O_RDWR | ((is_append != 0) ? O_APPEND : O_TRUNC);
    s32 fd = bbox_open(path, flags, FILE_RW_MODE);
    if (fd < 0) {
        BBOX_PERROR("open", path);
        return BBOX_FAILURE;
    }

    ret = (s32)bbox_lseek(fd, 0L, SEEK_END);
    if (ret < 0) {
        BBOX_PERROR("lseek", path);
        bbox_close(fd);
        return BBOX_FAILURE;
    }

    /* Maximum single read/write limit, see include/linux/fs.h, #define MAX_RW_COUNT (INT_MAX & PAGE_MASK) */
    while (len_temp) {
        ret = bbox_write(fd, (const void *)((const char *)(buf) + offset), len_temp);
        if (ret <= 0) {
            bbox_close(fd);
            return ret;
        }
        len_temp -= (u32)ret;
        offset += (u32)ret;
    }

    if (offset != len) {
        BBOX_ERR("write file %s exception. (offset=%u, len=%u)", path, offset, len);
    }

    bbox_close(fd);
    return ret;
}

/**
 * @brief       : write done file
 * @param [in]  : char *path            file path
 * @param [in]  : s32 stat              file stat
 * @return      : <0 failure; ==0 success
 */
s32 bbox_write_done_file(const char *path, s32 stat)
{
    if (stat < STORE_STARTING || stat >= STORE_MAX) {
        BBOX_ERR("stat out of range. (stat=%d, valid_range=[%d, %d))", stat, STORE_STARTING, STORE_MAX);
        return BBOX_FAILURE;
    }
    const char *str = g_done_file_str[stat];
    BBOX_DBG("write done file stat: %s", str);
    return bbox_save_buf_to_fs(path, DONE_FILE, str, (u32)strlen(str), BBOX_FALSE);
}

/**
 * @brief       : chmod file mode
 * @param [in]  : const char *file_path      file path
 * @param [in]  : MODE mode                 file mode
 * @return      : < 0 fail == 0 success
 */
STATIC_INLINE bbox_status bbox_file_chmod(const char *file_path, s32 mode)
{
    if (bbox_chmod(file_path, mode) != BBOX_SUCCESS) {
        BBOX_ERR_CTRL(BBOX_PERROR, return BBOX_FAILURE, "chmod", file_path);
    }
    return BBOX_SUCCESS;
}

/**
 * @brief       : chmod dir mode by recursive function
 * @param [in]  : const char *log_path       dir path
 * @param [in]  : MODE mode                 file mode
 * @param [in]  : u32 rec_num                number of recursions
 * @return      : < 0 fail == 0 success
 */
bbox_status bbox_dir_chmod(const char *log_path, s32 mode, u32 rec_num)
{
    BBOX_CHK_NULL_PTR(log_path, return BBOX_FAILURE);

    if (rec_num == 0) {
        BBOX_WAR("Dir chmod reached the number of recursions.");
        return BBOX_SUCCESS;
    }

    bbox_dirent **dir = NULL;
    s32 num = bbox_scan_dir(log_path, &dir, NULL, NULL);
    if (num < 0) {
        BBOX_PERROR("scandir", log_path);
        return BBOX_FAILURE;
    }

    s32 i;
    for (i = 0; i < num && dir != NULL && dir[i] != NULL; i++) {
        if ((strcmp(dir[i]->d_name, CURRENT_DIRNAME) == 0) || (strcmp(dir[i]->d_name, PARENT_DIRNAME) == 0)) {
            continue;
        }
        char path[DIR_MAXLEN] = {0};
        s32 err = bbox_format_path(path, DIR_MAXLEN, log_path, dir[i]->d_name);
        if (err == -1) {
            bbox_scan_dir_free(dir, num);
            BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox_format_path error");
        }

        if (dir[i]->d_type == DT_DIR) {
            bbox_status ret = bbox_dir_chmod(path, mode, rec_num - 1U);
            if (ret != BBOX_SUCCESS) {
                bbox_scan_dir_free(dir, num);
                BBOX_ERR_CTRL(BBOX_ERR, return ret, "dir[%s] files chmod failed", path);
            }
            ret = bbox_file_chmod(path, DIR_MODE);
            if (ret != BBOX_SUCCESS) {
                bbox_scan_dir_free(dir, num);
                BBOX_ERR_CTRL(BBOX_ERR, return ret, "dir[%s] chmod failed", path);
            }
        } else {
            bbox_status ret = bbox_file_chmod((const char *)path, mode);
            if (ret != BBOX_SUCCESS) {
                bbox_scan_dir_free(dir, num);
                BBOX_ERR_CTRL(BBOX_ERR, return ret, "file[%s] chmod failed", path);
            }
        }
    }

    bbox_scan_dir_free(dir, num);
    return BBOX_SUCCESS;
}

/**
 * @brief        create dir under log_path
 * @param [out]  sub_path     sub path
 * @param [in]   len         length of log_path
 * @param [in]   log_path     log path
 * @param [in]   dir         dir name
 * @return      0 on success otherwise -1
 */
STATIC bbox_status bbox_create_sub_path(char *sub_path, u32 len, const char *log_path, const char *dir)
{
    return bbox_age_add_folder(log_path, dir, sub_path, len);
}

/**
 * @brief        create sub path under log_path to store ddr maintenance data
 * @param [out]  sub_path     log sub path
 * @param [in]   len         length of log_path
 * @param [in]   log_path     log path
 * @return      0 on success otherwise -1
 */
bbox_status bbox_create_bbox_sub_path(char *sub_path, u32 len, const char *log_path)
{
    return bbox_create_sub_path(sub_path, len, log_path, DIR_BBOXDUMP);
}

/**
 * @brief        create sub path under log_path to store register & sram logs
 * @param [out]  sub_path     log sub path
 * @param [in]   len         length of log_path
 * @param [in]   log_path     log path
 * @return      0 on success otherwise -1
 */
bbox_status bbox_create_mntn_sub_path(char *sub_path, u32 len, const char *log_path)
{
    return bbox_create_sub_path(sub_path, len, log_path, DIR_MNTN);
}

/**
 * @brief        create sub path under log_path to store module log data
 * @param [out]  sub_path     mntn sub path
 * @param [in]   len         length of log_path
 * @param [in]   log_path     log path
 * @return      0 on success otherwise -1
 */
bbox_status bbox_create_log_sub_path(char *sub_path, u32 len, const char *log_path)
{
    return bbox_create_sub_path(sub_path, len, log_path, DIR_MODULELOG);
}

/**
 * @brief       : append(save) data to log path.
 * @param [in]  : const char *log_path    path of save file.
 * @param [in]  : const char *file_name   filename
 * @param [in]  : const buff *buf        save data.
 * @param [in]  : u32  len               data length.
 * @param [in]  : u32  is_append          determine whether write with append
 * @return      : <0 failure; >=0 success
 */
s32 bbox_save_log_buf_to_fs(const char *log_path, const char *file_name, const buff *buf, u32 len, u32 is_append)
{
    char sub_path[DIR_MAXLEN];
    bbox_status ret = bbox_create_log_sub_path(sub_path, DIR_MAXLEN, log_path);
    if (ret != BBOX_SUCCESS) {
        BBOX_ERR_CTRL(BBOX_ERR, return ret, "bbox create log sub path failed.");
    }

    return bbox_save_buf_to_fs(sub_path, file_name, buf, len, is_append);
}

/**
 * @brief       : create dir if not present
 * @param [in]  : const char *path          path.
 * @return      : <0 failure; ==0 success
 */
STATIC bbox_status bbox_mkdir_not_present(const char *path)
{
    bbox_stat64_t stat = {0};

    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(strlen(path) == 0, return BBOX_FAILURE, "%s", path);

    if (bbox_stat64(path, &stat) != BBOX_SUCCESS) {
        BBOX_DBG("create dir: %s", path);
        bbox_status ret = bbox_mkdir(path, DIR_MODE);
        BBOX_CHK_EXPR_CTRL(BBOX_PERROR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "mkdir", path);
    }

    return BBOX_SUCCESS;
}

/**
 * @brief       : recursively create dir
 * @param [in]  : const char *dir_path          dir path.
 * @return      : <0 failure; ==0 success
 */
bbox_status bbox_mkdir_recur(const char *dir_path)
{
    bbox_status ret = BBOX_SUCCESS;
    u32 index;
    u32 rt_len = strlen(OS_ROOT_DIR);
    char cur_path[DIR_MAXLEN] = {0};
    const char *path = dir_path;

    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);

    // get the root dir
    for (index = 0; index < rt_len && index < DIR_MAXLEN && (*path != '\0');) {
        cur_path[index++] = *path++;
    }

    // get the sub dir, step-by-step
    while (*path != '\0' && ret == BBOX_SUCCESS) {
        if (*path == OS_PATH_SPLIT) {
            ret = bbox_mkdir_not_present(cur_path);
        }

        cur_path[index] = *path;
        if (++index == DIR_MAXLEN) {
            BBOX_ERR("path name(%s) too long. (max_len=%d)", path, DIR_MAXLEN);
            return BBOX_FAILURE;
        }
        path++;
    }

    // Create the last subdir
    if (ret == BBOX_SUCCESS) {
        ret = bbox_mkdir_not_present(cur_path);
    }
    return ret;
}

/**
 * @brief       : append(save) data to path.
 * @param [in]  : const char *old_path       the old path
 * @param [in]  : const char *folder        add the folder
 * @param [out] : char *new_path             the new path to return
 * @param [in]  : u32 len                   the new path length
 * @return      : <0 failure; ==0 success
 */
bbox_status bbox_age_add_folder(const char *old_path, const char *folder, char *new_path, u32 len)
{
    BBOX_CHK_NULL_PTR(old_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(new_path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(folder, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(len == 0, return BBOX_FAILURE, "%u", len);

    s32 ret = bbox_format_path(new_path, len, old_path, folder);
    if (ret == -1 || ret == 0) {
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "bbox_format_path [%s][%s] failed.", old_path, folder);
    }
    return bbox_mkdir_recur(new_path);
}

/**
 * @brief       : format the child full path from parent path
 * @param [out] : char *buf             path buffer
 * @param [in]  : u32 len               path buffer len
 * @param [in]  : const char *parent    parent path
 * @param [in]  : const char *child     child dir name
 * @return      : < 0 fail >= 0 success
 */
s32 bbox_format_path(char *buf, u32 len, const char *parent, const char *child)
{
    return sprintf_s(buf, len, OS_PATH_FORMAT, parent, child);
}

/*
 * @brief       : format the exception's device path
 * @param [out] : char *buf             path buffer
 * @param [in]  : u32 len               path buffer len
 * @param [in]  : const char *parent    parent path
 * @param [in]  : u32 dev               device id
 * @return      : < 0 fail >= 0 success
 */
s32 bbox_format_device_path(char *buf, u32 len, const char *parent, u32 dev)
{
    return sprintf_s(buf, len, "%s" OS_DIR_SLASH DEVDIR_FORMAT, parent, dev);
}

s32 bbox_format_device_offline_path(char *buf, u32 len, const char *parent, u32 dev)
{
    return sprintf_s(buf, len, "%s" OS_DIR_SLASH DEVDIR_STR "offline-%u", parent, dev);
}
