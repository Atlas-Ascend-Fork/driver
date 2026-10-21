/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/stat.h>
#include <sys/file.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include "securec.h"
#include "user_log.h"
#include "file_opt.h"

static int read_file_to_buf_check_param(const char *path, const char *content, unsigned int *content_len)
{
    if (path == NULL || content == NULL || content_len == NULL) {
        roce_err("path[%d] or conf_path[%d] or content_len[%d]is NULL, invalid", (path == NULL), (content == NULL),
                 (content_len == NULL));
        return -EINVAL;
    }

    if (strlen(path) > PATH_MAX) {
        roce_err("path_len[%lu] > [%d](PATH_MAX)", strlen(path), PATH_MAX);
        return -EFAULT;
    }

    return 0;
}

int ReadFileToBuf(const char *path, char *content, unsigned int *content_len)
{
    int ret, ret_val;
    int len;
    FILE *read_file = NULL;
    char real_conf_path[PATH_MAX + 1] = {0}; // lint !e813

    ret = read_file_to_buf_check_param(path, content, content_len);
    if (ret) {
        roce_err("read_file_to_buf_check_param failed, ret[%d]", ret);
        return ret;
    }

    if (realpath(path, real_conf_path) == NULL) {
        ret = -errno;
        if (ret != -ENOENT) {
            roce_err("conf_path[%s] is invalid, err[%d]", path, ret);
        }
        return ret;
    }

    read_file = fopen(real_conf_path, "r");
    if (read_file == NULL) {
        roce_err("read_file is NULL, invalid");
        return -EINVAL;
    }

    ret = fseek(read_file, 0, SEEK_END);
    if (ret < 0) {
        roce_err("fseek failed with error:%d", errno);
        goto out;
    }

    len = ftell(read_file);
    if (len <= 0 || len > (int)*content_len) {
        ret = -EINVAL;
        roce_err("ftell failed with error:%d, len=%d, content_len[%u]", errno, len, *content_len);
        goto out;
    }

    rewind(read_file);
    ret = (int)fread((void *)content, len, 1, read_file); /* read a buf which size is len */
    if (ret != 1) {
        roce_err("fread failed ret:%d, error:%d, ferror(fp):%d", ret, errno, ferror(read_file));
        ret = -EINVAL;
        goto out;
    }
    ret = 0;
    *content_len = len;
out:
    ret_val = fclose(read_file);
    if (ret_val) {
        roce_warn("fclose fail, ret_val:%d, errno:%d", ret_val, errno);
    }

    return ret;
}

int WriteBufToFile(const unsigned char *buf, unsigned int buf_len, const char *file, mode_t mode)
{
    int ret, ret_val;
    int fd = -1;

    if (buf == NULL || file == NULL) {
        roce_err("buf[%d] or file[%d] is NULL, invalid", (buf == NULL), (file == NULL));
        return -EINVAL;
    }

    fd = creat(file, mode);
    if (fd < 0) {
        ret = -errno;
        roce_err("create file %s failed errno %d", file, ret);
        return ret;
    }

    ret = write(fd, buf, buf_len);
    if (ret != (int)buf_len) {
        roce_err("write file %s failed %d, buf_len:%u", file, ret, buf_len);
        ret = -errno;
        do {
            ret_val = close(fd);
        } while ((ret_val < 0) && (errno == EINTR));
        RemoveFile(file);
        return ret;
    } else {
        ret = 0;
    }

    do {
        ret_val = close(fd);
    } while ((ret_val < 0) && (errno == EINTR));

    fd = -1;

    ret = chmod(file, mode);
    if (ret != 0) {
        roce_err("file[%s] chmod fail, errno: %d.", file, ret);
    }
    return ret;
}

void RemoveFile(const char *file)
{
    int ret, err;

    if (file == NULL) {
        roce_err("file is NULL, invalid");
        return;
    }

    ret = remove(file);
    if (ret < 0) {
        err = -errno;
        if (err != -ENOENT) {
            roce_err("remove end file failed %d", err);
            return;
        }
    }
    return;
}

int check_file_path(const char *path, mode_t mode)
{
    int ret;
    char real_conf_path[PATH_MAX + 1] = {0}; // lint !e813

    if (path == NULL) {
        roce_err("path is NULL");
        return -EINVAL;
    }

    if (strlen(path) > PATH_MAX) {
        roce_err("path_len[%lu] > [%d](PATH_MAX)", strlen(path), PATH_MAX);
        return -EINVAL;
    }

    if (realpath(path, real_conf_path) == NULL) {
        ret = -errno;
        if (ret == -ENOENT) {
            roce_warn("path[%s] is not exist, real_path[%s]", path, real_conf_path);
            ret = mkdir(real_conf_path, mode);
            if (ret) {
                roce_err("mkdir real_conf_path[%s] failed, ret[%d]", real_conf_path, ret);
                return ret;
            }
            ret = chmod(real_conf_path, mode);
            if (ret != 0) {
                roce_err("file[%s] chmod fail, errno: %d.", real_conf_path, ret);
            }
            return ret;
        }

        roce_err("path[%s] is invalid, err[%d]", path, ret);
        return ret;
    }

    return 0;
}

void close_fd_security(int fd)
{
    int ret;
    int err_no = -1;

    do {
        ret = close(fd);
        if (ret < 0) {
            err_no = errno;
            if (err_no != EINTR) {
                roce_err("close fd[%d] failed, ret:%d, err_no[%d]", fd, ret, err_no);
                return;
            }
        }
    } while ((ret < 0) && (err_no == EINTR));

    return;
}

int get_file_lock(const char *path, int *lock_fd)
{
    int ret, err;
    int fd = -1;

    if (lock_fd == NULL || path == NULL) {
        roce_err("lock_fd[%d] is NULL or path[%d] is NULL", (lock_fd == NULL), (path == NULL));
        return -EINVAL;
    }

    if (strlen(path) > PATH_MAX) {
        roce_err("path_len[%lu] > [%d](PATH_MAX)", strlen(path), PATH_MAX);
        return -EINVAL;
    }

    fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        ret = -errno;
        roce_err("open path[%s] failed, ret[%d]", path, ret);
        return ret;
    }

    ret = flock(fd, LOCK_EX | LOCK_NB);
    if (ret) {
        err = -errno;
        close_fd_security(fd);
        if (err == -EWOULDBLOCK) {
            return -EBUSY;
        }
        roce_err("lock fd[%d] failed, err[%d]", fd, err);
        return err;
    }

    *lock_fd = fd;
    return 0;
}

int release_file_lock(int lock_fd, const char *lock_file_path)
{
    int ret, err;

    if (lock_fd < 0) {
        roce_err("invalid lock_fd[%d]", lock_fd);
        return -EINVAL;
    }

    ret = flock(lock_fd, LOCK_UN);
    if (ret) {
        err = -errno;
        roce_err("release lock fd[%d] failed, err[%d]", lock_fd, err);
        close_fd_security(lock_fd);
        return err;
    }

    close_fd_security(lock_fd);

    RemoveFile(lock_file_path);
    return 0;
}
