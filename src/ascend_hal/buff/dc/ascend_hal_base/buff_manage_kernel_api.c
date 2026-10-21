/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#include "davinci_interface.h"

#include "securec.h"
#include "dmc_user_interface.h"

#include "buff_manage_base.h"
#include "buff_manage_kernel_api.h"
#include "buff_ioctl.h"
#include "buff_feature.h"
#include "drv_buff_adp.h"
#include "drv_buff_log.h"

#define BUFF_POOL_ID_MAX_BASE 0x7FFFFFFF

#ifdef EMU_ST
#define STATIC
#else
#define STATIC static
#endif

#ifdef EMU_ST
#define THREAD __thread
int buff_get_current_pid_base(void);
#else
#include <sys/types.h>
#include <unistd.h>
#define THREAD
static inline int buff_get_current_pid_base(void)
{
    return getpid();
}
#endif

#define XSMEM_BLK_ALIGN 16
#ifdef EMU_ST
#define XSMEM_DRV_NAME "/dev/xsmem_dev"
#else
#define XSMEM_DRV_NAME davinci_intf_get_dev_path()
#endif
#ifdef EMU_ST
int pool_file_open(const char *pathname, int flags);
int pool_ioctl(int fd, unsigned long cmd, void *para);
#else
#define pool_file_open open
#define pool_ioctl(fd, cmd, ...) ioctl((fd), (unsigned int)(cmd), ##__VA_ARGS__)
#endif

#define BUFF_INVALID_TGID (-1)

volatile pid_t THREAD g_buff_pid = (pid_t)BUFF_INVALID_TGID;

void buff_set_pid_base(pid_t pid)
{
    g_buff_pid = pid;
}

int buff_api_getpid_base(void)
{
    return g_buff_pid;
}

STATIC int THREAD pool_fd = -1;
STATIC int THREAD pool_fd_open_pid = -1;

#ifndef EMU_ST
STATIC int buff_pool_dev_open(void)
{
    struct davinci_intf_open_arg arg = {0};
    int fd = -1;
    int ret;

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_XSMEM_SUB_MODULE_NAME);
    if (ret < 0) {
        buff_err("Failed to invoke the strcpy_s. (ret=%d).\n", ret);
        return fd;
    }

    share_log_create(HAL_MODULE_TYPE_BUF_MANAGER, SHARE_LOG_MAX_SIZE);

    fd = pool_file_open(XSMEM_DRV_NAME, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        buff_err("Open dev file failed. (errno=%d)\n", errno);
        return fd;
    }

    ret = pool_ioctl(fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
    if (ret != 0) {
        buff_err("Failed to invoke the DAVINCI_INTF_IOCTL_CLOSE. (ret=%d)\n", ret);
        close(fd);
        fd = -1;
    }

    return fd;
}

STATIC void buff_pool_dev_close(void)
{
    struct davinci_intf_open_arg arg = {0};
    int32_t ret;

    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, DAVINCI_XSMEM_SUB_MODULE_NAME);
    if (ret < 0) {
        buff_err("Failed to invoke the strcpy_s. (ret=%d).\n", ret);
        return;
    }

    ret = pool_ioctl(pool_fd, DAVINCI_INTF_IOCTL_CLOSE, &arg);
    if (ret != 0) {
        buff_err("Failed to invoke the DAVINCI_INTF_IOCTL_CLOSE. (ret=%d)\n", ret);
    }

    (void)close(pool_fd);
}
#else
STATIC int buff_pool_dev_open(void)
{
    return pool_file_open(XSMEM_DRV_NAME, O_RDWR);
}

STATIC void buff_pool_dev_close(void)
{
    (void)close(pool_fd);
}
#endif

static int buff_pool_get_fd(void)
{
    static pthread_mutex_t pool_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

    /* fork child proccess, should not use parent fd */
    if ((pool_fd >= 0) && (pool_fd_open_pid == buff_get_current_pid_base())) {
        return pool_fd;
    }

    (void)pthread_mutex_lock(&pool_fd_mutex);

    if ((pool_fd >= 0) && (pool_fd_open_pid == buff_get_current_pid_base())) {
        (void)pthread_mutex_unlock(&pool_fd_mutex);
        return pool_fd;
    }

    if (pool_fd >= 0) {
        buff_pool_dev_close();
    }

    pool_fd = buff_pool_dev_open();
    if (pool_fd < 0) {
        buff_err("open dev file failed, errno %d\n", errno);
    } else {
        int flags = fcntl(pool_fd, F_GETFD);
        flags = (int)((unsigned int)flags | FD_CLOEXEC);
        (void)fcntl(pool_fd, F_SETFD, flags);

        buff_info("pid %d open dev file, pool_fd %d\n", buff_get_current_pid_base(), pool_fd);
        pool_fd_open_pid = buff_get_current_pid_base();

        buff_set_pid_base(buff_get_current_pid_base());
    }

    (void)pthread_mutex_unlock(&pool_fd_mutex);
    return pool_fd;
}

static drvError_t buff_pool_errno_map(int cmd, int tmp_errno)
{
    if (cmd == XSMEM_CACHE_DESTROY) {
        if (tmp_errno == EBUSY) {
            buff_warn("Cache is in use.");
            return DRV_ERROR_BUSY;
        }
        if (tmp_errno == EOPNOTSUPP) {
            return DRV_ERROR_NOT_SUPPORT;
        }
        if ((tmp_errno == ENODEV) || (tmp_errno == ENXIO)) {
            return DRV_ERROR_NO_DEVICE;
        }
    }

    return DRV_ERROR_IOCRL_FAIL;
}

static drvError_t buff_pool_ioctrl(int cmd, void *para)
{
    int fd = buff_pool_get_fd();
    int ret;

    do {
        ret = pool_ioctl(fd, (unsigned int)cmd, para);
    } while ((ret == -1) && (errno == EINTR));

    if (ret < 0) {
        ret = buff_pool_errno_map(cmd, errno);
        share_log_read_err(HAL_MODULE_TYPE_BUF_MANAGER);
        share_log_read_run_info(HAL_MODULE_TYPE_BUF_MANAGER);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t buff_pool_register(const char *name, unsigned long pool_size, int algo, unsigned int priv_mbuf_flag,
                                     int *pool_id)
{
    struct xsm_reg_arg arg;
    int fd = buff_pool_get_fd();
    int tmp_errno;

    arg.algo = algo;
    arg.pool_size = pool_size;
    arg.block_size = XSMEM_BLK_ALIGN;
    arg.key_len = (unsigned int)strlen(name);
    arg.key = name;
    arg.priv_flag = priv_mbuf_flag;

    *pool_id = pool_ioctl(fd, XSMEM_POOL_REGISTER, &arg);
    if (*pool_id < 0) {
        tmp_errno = errno;
        buff_err("Pool Register failed. (fd=%d, err=%d)\n", fd, tmp_errno);
        if (tmp_errno == EEXIST) {
            return DRV_ERROR_GROUP_EXIST;
        }
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_algo_vma_register(const char *name, unsigned long pool_size, unsigned int priv_mbuf_flag,
                                       int *pool_id)
{
    return buff_pool_register(name, pool_size, XSMEM_ALGO_VMA, priv_mbuf_flag, pool_id);
}

drvError_t buff_pool_algo_cache_vma_register(const char *name, unsigned long pool_size, unsigned int priv_mbuf_flag,
                                             int *pool_id)
{
    return buff_pool_register(name, pool_size, XSMEM_ALGO_CACHE_VMA, priv_mbuf_flag, pool_id);
}

drvError_t buff_pool_algo_sp_register(const char *name, unsigned long pool_size, unsigned int priv_mbuf_flag,
                                      int *pool_id)
{
    return buff_pool_register(name, pool_size, XSMEM_ALGO_SP, priv_mbuf_flag, pool_id);
}

drvError_t buff_pool_algo_cache_sp_register(const char *name, unsigned long pool_size, unsigned int priv_mbuf_flag,
                                            int *pool_id)
{
    return buff_pool_register(name, pool_size, XSMEM_ALGO_CACHE_SP, priv_mbuf_flag, pool_id);
}

drvError_t buff_pool_unregister(int pool_id)
{
    unsigned long arg = (unsigned int)pool_id;
    return buff_pool_ioctrl(XSMEM_POOL_UNREGISTER, (void *)(uintptr_t)arg);
}

drvError_t buff_pool_cache_create(int pool_id, unsigned int dev_id, unsigned int mem_flag, unsigned long long mem_size,
                                  unsigned long long alloc_max_size)
{
    struct xsm_cache_create_arg arg;

    arg.pool_id = pool_id;
    arg.dev_id = dev_id;
    arg.mem_flag = mem_flag;
    arg.mem_size = mem_size;
    arg.alloc_max_size = alloc_max_size;

    return buff_pool_ioctrl(XSMEM_CACHE_CREATE, (void *)&arg);
}

drvError_t buff_pool_cache_destroy(int pool_id, unsigned int dev_id)
{
    struct xsm_cache_destroy_arg arg;

    arg.pool_id = pool_id;
    arg.dev_id = dev_id;

    return buff_pool_ioctrl(XSMEM_CACHE_DESTROY, (void *)&arg);
}

drvError_t buff_pool_add_proc(int pool_id, int pid, GroupShareAttr attr)
{
    int fd = buff_pool_get_fd();
    struct xsm_task_arg arg;
    int ret;

    arg.pool_id = pool_id;
    arg.pid = pid;
    arg.attr = attr;

    ret = pool_ioctl(fd, XSMEM_POOL_TASK_ADD, (void *)&arg);
    if (ret < 0) {
        if (errno == EEXIST) {
            return DRV_ERROR_REPEATED_INIT;
        }
        share_log_read_err(HAL_MODULE_TYPE_BUF_MANAGER);
        share_log_read_run_info(HAL_MODULE_TYPE_BUF_MANAGER);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_del_proc(int pool_id, int pid)
{
    struct xsm_task_arg arg;

    arg.pool_id = pool_id;
    arg.pid = pid;

    return buff_pool_ioctrl(XSMEM_POOL_TASK_DEL, (void *)&arg);
}

drvError_t buff_pool_attach_ex(int pool_id, int timeout, unsigned long long *proc_uid, unsigned int *cache_type)
{
    struct xsm_task_attach_arg arg;
    int fd = buff_pool_get_fd();
    int ret;

    arg.pool_id = pool_id;
    arg.timeout = timeout;

    ret = pool_ioctl(fd, XSMEM_POOL_ATTACH, &arg);
    if (ret != 0) {
        buff_err("pool %d fd %d attach failed, errno %d.\n", pool_id, fd, errno);
        share_log_read_err(HAL_MODULE_TYPE_BUF_MANAGER);
        share_log_read_run_info(HAL_MODULE_TYPE_BUF_MANAGER);
        return DRV_ERROR_IOCRL_FAIL;
    }

    *proc_uid = arg.uid;
    *cache_type = arg.cache_type;
    buff_info("Pool attach. (pool=%d; fd=%d; attach_pid=%d; uid=%llu; cache_type=%u)\n", pool_id, fd,
              buff_get_current_pid_base(), *proc_uid, *cache_type);

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_attach(int pool_id, int timeout)
{
    unsigned long long proc_uid;
    unsigned int cache_type;

    return buff_pool_attach_ex(pool_id, timeout, &proc_uid, &cache_type);
}

drvError_t buff_pool_detach(int pool_id)
{
    unsigned long arg = (unsigned int)pool_id;
    return buff_pool_ioctrl(XSMEM_POOL_DETACH, (void *)(uintptr_t)arg);
}

drvError_t buff_pool_blk_alloc(int pool_id, unsigned long size, unsigned long flag, unsigned long *ptr,
                               uint32_t *blk_id)
{
    struct xsm_block_arg arg;
    drvError_t ret;

    arg.pool_id = pool_id;
    arg.alloc.flag = flag;
    arg.alloc.size = size;

    ret = buff_pool_ioctrl((int)XSMEM_BLOCK_ALLOC, (void *)&arg);
    if (ret != 0) {
        buff_event("Can not alloc. (pool_id=%d; size=%lx)\n", pool_id, size);
        return ret;
    }

    *blk_id = arg.alloc.blkid;
    *ptr = arg.alloc.offset;
    return DRV_ERROR_NONE;
}

drvError_t buff_pool_blk_free(int pool_id, unsigned long ptr)
{
    struct xsm_block_arg arg;

    arg.pool_id = pool_id;
    arg.free.offset = ptr;

    return buff_pool_ioctrl(XSMEM_BLOCK_FREE, (void *)&arg);
}

drvError_t buff_pool_blk_get(unsigned long ptr, int *pool_id, unsigned long *alloc_ptr, unsigned long *alloc_size,
                             uint32_t *blk_id)
{
    struct xsm_block_arg arg;
    drvError_t ret;

    arg.get.offset = ptr;

    ret = buff_pool_ioctrl((int)XSMEM_BLOCK_GET, (void *)&arg);
    if (ret != 0) {
        buff_warn("get not success, ptr %lx\n", ptr);
        return ret;
    }
    *blk_id = arg.get.blkid;
    *alloc_ptr = arg.get.alloc_offset;
    *alloc_size = arg.get.alloc_size;
    *pool_id = arg.pool_id;

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_blk_put(int pool_id, unsigned long ptr)
{
    struct xsm_block_arg arg;

    arg.pool_id = pool_id;
    arg.free.offset = ptr;

    return buff_pool_ioctrl(XSMEM_BLOCK_PUT, (void *)&arg);
}

drvError_t buff_pool_id_query(const char *name, int *pool_id)
{
    struct xsm_pool_query_arg arg;
    int fd = buff_pool_get_fd();
    int ret;

    arg.key_len = (unsigned int)strnlen(name, BUFF_GRP_NAME_LEN);
    arg.key = name;

    ret = pool_ioctl(fd, XSMEM_POOL_ID_QUERY, (void *)&arg);
    if (ret != 0) {
        return DRV_ERROR_IOCRL_FAIL;
    }

    *pool_id = arg.pool_id;

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_priv_flag_query(int pool_id, unsigned int *flag)
{
    struct xsm_query_pool_flag_arg arg;
    drvError_t ret;

    arg.pool_id = pool_id;

    ret = buff_pool_ioctrl((int)XSMEM_POOL_FLAG_QUERY, (void *)&arg);
    if (ret != 0) {
        buff_err("pool id %d query flag failed\n", pool_id);
        return ret;
    }

    *flag = arg.priv_flag;

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_va_check(int pool_id, unsigned long va, int *result)
{
    struct xsm_check_va_arg arg = {0};
    drvError_t ret;

    arg.va = va;
    arg.pool_id = pool_id;

    ret = buff_pool_ioctrl((int)XSMEM_VADDR_CHECK, (void *)&arg);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Va check failed. (pool_id=%d; va=0x%lx)\n", pool_id, (uintptr_t)va);
        return ret;
    }

    *result = arg.result;

    return DRV_ERROR_NONE;
}

drvError_t buff_cache_info_query(int pool_id, unsigned int dev_id, GrpQueryGroupAddrInfo *cache_buff,
                                 unsigned int cache_cnt, unsigned int *query_cnt)
{
    struct xsm_query_cache_arg arg;
    drvError_t ret;

    arg.pool_id = pool_id;
    arg.dev_id = dev_id;
    arg.cache_buff = cache_buff;
    arg.cache_cnt = cache_cnt;

    ret = buff_pool_ioctrl((int)XSMEM_CACHE_QUERY, (void *)&arg);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    *query_cnt = arg.cache_cnt;

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_name_query(int pool_id, char *name, int name_len)
{
    struct xsm_pool_query_arg arg;
    int ret;

    arg.pool_id = pool_id;
    arg.key_len = (unsigned int)name_len;
    arg.key = name;

    ret = memset_s(name, (unsigned int)name_len, 0, (unsigned int)name_len);
    if (ret != 0) {
        buff_err("pool_id %d memset_s failed name_len %d\n", pool_id, name_len);
        return DRV_ERROR_INNER_ERR;
    }

    ret = (int)buff_pool_ioctrl((int)XSMEM_POOL_NAME_QUERY, (void *)&arg);
    if (ret != 0) {
        buff_err("pool_id %d query pool name failed\n", pool_id);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_task_query(int pool_id, int *pid, int pid_num, int *query_num)
{
    struct xsm_query_pool_task_arg arg;
    drvError_t ret;

    arg.pool_id = pool_id;
    arg.pid = pid;
    arg.pid_num = pid_num;

    ret = buff_pool_ioctrl((int)XSMEM_POOL_TASK_QUERY, (void *)&arg);
    if (ret != 0) {
        buff_err("pool id %d query task failed\n", pool_id);
        return ret;
    }

    *query_num = arg.pid_num;

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_task_attr_query(int pool_id, int pid, GroupShareAttr *attr)
{
    struct xsm_task_arg arg;
    drvError_t ret;

    arg.pool_id = pool_id;
    arg.pid = pid;

    ret = buff_pool_ioctrl((int)XSMEM_POOL_TASK_ATTR_QUERY, (void *)&arg);
    if (ret != 0) {
        /* Do not print error log */
        return ret;
    }

    *attr = arg.attr;

    return DRV_ERROR_NONE;
}

static drvError_t buff_task_pool_query_inner(int type, int pid, int *pool_id, int pool_num, int *query_num)
{
    struct xsm_query_task_pool_arg arg;
    int fd = buff_pool_get_fd();
    int ret;

    arg.pid = pid;
    arg.pool_id = pool_id;
    arg.pool_num = pool_num;
    arg.type = type;

    ret = pool_ioctl(fd, XSMEM_TASK_POOL_QUERY, (void *)&arg);
    if (ret != 0) {
        return DRV_ERROR_IOCRL_FAIL;
    }

    *query_num = arg.pool_num;

    return DRV_ERROR_NONE;
}

drvError_t buff_task_pool_query(int pid, int *pool_id, int pool_num, int *query_num)
{
    return buff_task_pool_query_inner(XSMEM_TASK_IN_POOL, pid, pool_id, pool_num, query_num);
}

drvError_t buff_task_adding_pool_query(int pid, int *pool_id, int pool_num, int *query_num)
{
    return buff_task_pool_query_inner(XSMEM_TASK_ADDING_TO_POOL, pid, pool_id, pool_num, query_num);
}

STATIC drvError_t buff_pool_set_prop_inner(int pool_id, const char *prop_name, int owner, unsigned long value)
{
    struct xsm_prop_op_arg arg;
    int fd = buff_pool_get_fd();
    int ret;

    if (pool_id == BUFF_POOL_ID_MAX_BASE) {
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.prop = prop_name;
    arg.prop_len = (unsigned int)strlen(prop_name);
    arg.owner = owner;
    arg.pool_id = pool_id;
    arg.op = XSMEM_PROP_OP_SET;
    arg.value = value;

    ret = pool_ioctl(fd, XSMEM_PROP_OP, (void *)&arg);
    if (ret != 0) {
        if (errno == EEXIST) {
            return DRV_ERROR_REPEATED_USERD;
        }
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_set_prop(int pool_id, const char *prop_name, unsigned long value)
{
    return buff_pool_set_prop_inner(pool_id, prop_name, XSMEM_PROP_OWNER_TASK, value);
}

drvError_t buff_pool_set_grp_prop(int pool_id, const char *prop_name, unsigned long value)
{
    return buff_pool_set_prop_inner(pool_id, prop_name, XSMEM_PROP_OWNER_TASK_GRP, value);
}

drvError_t buff_pool_get_prop(int pool_id, const char *prop_name, unsigned long *value)
{
    struct xsm_prop_op_arg arg;
    int fd = buff_pool_get_fd();
    int ret;

    if (pool_id == BUFF_POOL_ID_MAX_BASE) {
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.prop = prop_name;
    arg.prop_len = (unsigned int)strlen(prop_name);
    arg.pool_id = pool_id;
    arg.op = XSMEM_PROP_OP_GET;

    ret = pool_ioctl(fd, XSMEM_PROP_OP, (void *)&arg);
    if (ret != 0) {
        return DRV_ERROR_IOCRL_FAIL;
    }

    *value = arg.value;

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_del_prop(int pool_id, const char *prop_name)
{
    struct xsm_prop_op_arg arg;
    int fd = buff_pool_get_fd();
    int ret;

    if (pool_id == BUFF_POOL_ID_MAX_BASE) {
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.prop = prop_name;
    arg.prop_len = (unsigned int)strlen(prop_name);
    arg.pool_id = pool_id;
    arg.op = XSMEM_PROP_OP_DEL;

    ret = pool_ioctl(fd, XSMEM_PROP_OP, (void *)&arg);
    if (ret != 0) {
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

int buff_pool_poll_exit_task(int pool_id, unsigned long long *proc_uid)
{
    struct xsm_poll_exit_task_arg arg;
    int fd = buff_pool_get_fd();
    int ret;

    arg.pool_id = pool_id;
    ret = pool_ioctl(fd, XSMEM_POLL_EXIT_TASK, (void *)&arg);
    if (ret != 0) {
        return DRV_ERROR_IOCRL_FAIL;
    }

    *proc_uid = arg.uid;

    return DRV_ERROR_NONE;
}

drvError_t buff_pool_feature_get_all(struct xsm_feature_get_arg *all_features)
{
    drvError_t ret;
    all_features->user_feature_num = BUFF_FEATURE_MAX;
    ret = buff_pool_ioctrl((int)XSMEM_FEATURE_GET, (void *)all_features);
    if (ret != DRV_ERROR_NONE) {
        buff_err("buff_pool_feature_get_all: ioctl XSMEM_FEATURE_GET failed. (ret=%d)\n", ret);
    }
    return ret;
}