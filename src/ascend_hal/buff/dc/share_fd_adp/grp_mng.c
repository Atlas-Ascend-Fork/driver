/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>

#include "dms_user_interface.h"

#include "ascend_hal_define.h"
#include "drv_buff_common.h"
#include "drv_buff_mbuf.h"
#include "drv_buff_adp.h"
#include "buff_mng.h"
#include "buff_manage_base.h"
#include "buff_recycle.h"
#include "buff_ioctl.h"
#include "buff_feature.h"
#include "grp_mng.h"

#define BUF_LEN 128

#define GRP_ATTACH_MAX_TIMEOUT 10000 /* 10s */
#define MS_PER_SECOND 1000
#define US_PER_MSECOND 1000

#define SOCKETF_FILE_OUT_OF_DATA_TIME (2 * GRP_ATTACH_MAX_TIMEOUT / MS_PER_SECOND) /* 20s */

struct grp_shr_info {
    unsigned long long max_mem_size;
    int pool_id;
};

int THREAD shr_mem_fd = -1;
int THREAD grp_pool_id = -1;
int THREAD grp_attach_pid = -1;
int THREAD grp_create_pid = -1;
unsigned long long THREAD grp_max_mem_size = 0;
unsigned int THREAD grp_cache_type = BUFF_NOCACHE;

pthread_mutex_t grp_mutex = PTHREAD_MUTEX_INITIALIZER;

#define SOCKET_CONNECT_MIN_TIMEOUT 3              /* 3s */
#define SOCKET_CONNECT_MAX_TIMEOUT 100            /* 100s */
static unsigned int g_socket_connect_timeout = 3; /* default 3s */

static void set_grp_owner(void)
{
    grp_create_pid = buff_get_current_pid();
}

static bool is_grp_owner(int pid)
{
    return (grp_create_pid == pid);
}

static void set_grp_cache_type(unsigned int cache_type)
{
    grp_cache_type = cache_type;
}

bool buff_is_enable_cache(void)
{
    return (grp_cache_type == BUFF_CACHE);
}

static void set_grp_attach(void)
{
    grp_attach_pid = buff_get_current_pid();
}

static bool is_grp_attach(void)
{
    return (grp_attach_pid == buff_get_current_pid());
}

static void set_grp_pool_id(int pool_id)
{
    grp_pool_id = pool_id;
}

static int get_grp_pool_id(void)
{
    return grp_pool_id;
}

STATIC void set_grp_shr_mem_fd(int fd)
{
    shr_mem_fd = fd;
}

static int get_grp_shr_mem_fd(void)
{
    return shr_mem_fd;
}

static void set_grp_max_mem_size(unsigned long long size)
{
    grp_max_mem_size = size;
}

static unsigned long long get_grp_max_mem_size(void)
{
    return grp_max_mem_size;
}

drvError_t buff_group_addr_query(GrpQueryGroupAddrInfo *addr_buff, unsigned int *query_cnt)
{
    *query_cnt = 1;
    addr_buff[0].addr = buff_get_base_addr();
    addr_buff[0].size = get_grp_max_mem_size();

    return DRV_ERROR_NONE;
}

static drvError_t pool_id_name_check(int pool_id, const char *name)
{
    drvError_t ret;
    int query_pool_id;

    ret = buff_pool_id_query(name, &query_pool_id);
    if (ret != DRV_ERROR_NONE) {
        buff_err("name %s query pool id failed\n", name);
        return ret;
    }

    if (pool_id != query_pool_id) {
        buff_err("name %s query pool id %d cur pool id %d mismatch\n", name, query_pool_id, pool_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

/* return /home/user or /root */
static int format_user_path(char *user_path, int path_len)
{
    struct passwd *user = NULL;
    int ret;

    user = getpwuid(getuid());
    if (user == NULL) {
        buff_err("Failed to get the current user.\n");
        return -1;
    }

    if (strcmp(user->pw_name, "root") == 0) {
        ret = sprintf_s(user_path, (unsigned int)path_len, "/root");
    } else {
        ret = sprintf_s(user_path, (unsigned int)path_len, "/home/%s", user->pw_name);
    }
    if (ret <= 0) {
        buff_err("Failed to sprintf_s.\n");
        return -1;
    }

    return 0;
}

static int format_buff_socket_dir_path(char *dir_path)
{
    char user_path[BUF_LEN];
    int ret;

    ret = format_user_path(user_path, BUF_LEN);
    if (ret != 0) {
        buff_err("Failed to get the user path.\n");
        return ret;
    }

    if (sprintf_s(dir_path, (unsigned int)BUF_LEN, "%s/%s", user_path, "buff_socket") <= 0) {
        buff_err("Failed to sprintf_s.\n");
        return -1;
    }

    return ret;
}

static void format_shr_mem_file_name(const char *name, int pid, char *file_name, int file_len)
{
    char dir_path[BUF_LEN];
    int ret;

    ret = format_buff_socket_dir_path(dir_path);
    if (ret != 0) {
        return;
    }

    if (access(dir_path, F_OK) == -1) {
        ret = mkdir(dir_path, S_IRWXU);
        if (ret != 0) {
            buff_err("Failed to mkdir. (ret=%d; dir_path=%s)\n", ret, dir_path);
        }
    }

    if (sprintf_s(file_name, (unsigned int)file_len, "%s/%s_%d", dir_path, name, pid) <= 0) {
        buff_warn("pid %d name %s sprintf_s not success\n", pid, name);
    }
}

static void init_unix_socket_addr(const char *name, int pid, struct sockaddr_un *addr)
{
    char file_name[BUF_LEN] = {0};

    format_shr_mem_file_name(name, pid, file_name, BUF_LEN);

    buff_info("pid %d socket name %s\n", pid, file_name);

    if (memset_s(addr, sizeof(struct sockaddr_un), 0, sizeof(struct sockaddr_un)) != 0) {
        buff_warn("pid %d socket name %s memset_s not success\n", pid, file_name);
    }
    addr->sun_family = AF_UNIX;
    if (strcpy_s(addr->sun_path, sizeof(addr->sun_path), file_name) != 0) {
        buff_warn("pid %d socket name %s strcpy_s not success\n", pid, file_name);
    }
}

/* timeout unit ms */
static drvError_t set_socket_timeout(int sfd, int timeout)
{
    struct timeval tv;
    int ret;

    tv.tv_sec = timeout / MS_PER_SECOND;
    tv.tv_usec = (timeout % MS_PER_SECOND) * US_PER_MSECOND;
    ret = setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    if (ret < 0) {
        buff_err("socket %d setsockopt SO_RCVTIMEO failed errno %d\n", sfd, errno);
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

static int client_socket_connect(const char *name, int pid)
{
    unsigned int retry_cnt, max_retry_cnt;
    struct sockaddr_un addr;
    int sfd, ret;

    sfd = socket(AF_UNIX, (int)SOCK_STREAM, 0);
    if (sfd < 0) {
        buff_err("name %s socket failed errno %d\n", name, errno);
        return sfd;
    }

    init_unix_socket_addr(name, pid, &addr);

    max_retry_cnt = g_socket_connect_timeout * MS_PER_SECOND;
    retry_cnt = 0;

    buff_debug("Connect begin. (name=%s; pid=%d; max_retry_cnt=%u)\n", name, pid, max_retry_cnt);

    while (1) {
        ret = connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
        if (ret >= 0) {
            break;
        }
        if (retry_cnt >= max_retry_cnt) {
            buff_err("name %s pid %d connect failed errno %d\n", name, pid, errno);
            (void)close(sfd);
            return ret;
        }
        (void)usleep(1000); /* 1000us */
        retry_cnt++;
    };
    buff_debug("Connect finish. (name=%s; pid=%d; retry_cnt=%u)\n", name, pid, retry_cnt);

    return sfd;
}

static void socket_close(int sfd)
{
    (void)close(sfd);
}

static void scan_remove_residual_socket_file(const char *name)
{
    char dir_path[BUF_LEN];
    char file_path[BUF_LEN];
    struct dirent *ptr = NULL;
    DIR *dir = NULL;
    struct stat sb;
    struct timeval now;
    struct timespec ts;
    int ret;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        buff_warn("get cur time not success\n");
        return;
    }
    now.tv_sec = ts.tv_sec;
    now.tv_usec = ts.tv_nsec / NSEC_PER_USEC;

    ret = format_buff_socket_dir_path(dir_path);
    if (ret != 0) {
        return;
    }

    if ((dir = opendir(dir_path)) == NULL) {
        buff_warn("opendir not success\n");
        return;
    }

    while ((ptr = readdir(dir)) != NULL) {
        if (strncmp(ptr->d_name, name, strlen(name)) == 0) {
            ret = sprintf_s(file_path, (unsigned int)sizeof(file_path), "%s/%s", dir_path, ptr->d_name);
            if (ret < 0) {
                continue;
            }
            if (stat(file_path, &sb) == -1) {
                continue;
            }

            if (((sb.st_mode & S_IFMT) != S_IFSOCK) ||
                ((now.tv_sec - sb.st_mtim.tv_sec) < SOCKETF_FILE_OUT_OF_DATA_TIME)) {
                continue;
            }

            (void)unlink(file_path);
            buff_info("remove %s socket file\n", file_path);
        }
    }

    (void)closedir(dir);
    return;
}

static void server_socket_file_remove(const char *name)
{
    char file_name[BUF_LEN];

    format_shr_mem_file_name(name, buff_get_current_pid(), file_name, BUF_LEN);
    (void)unlink(file_name);

    scan_remove_residual_socket_file(name);
}

static int server_socket_create(const char *name)
{
    struct sockaddr_un addr;
    int sfd, ret;

    sfd = socket(AF_UNIX, (int)SOCK_STREAM, 0);
    if (sfd < 0) {
        buff_err("name %s socket failed errno %d\n", name, errno);
        return sfd;
    }

    init_unix_socket_addr(name, buff_get_current_pid(), &addr);

    server_socket_file_remove(name);

    ret = bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
    if (ret < 0) {
        buff_err("name %s socket %d bind failed errno %d\n", name, sfd, errno);
        (void)close(sfd);
        return -1;
    }

    ret = listen(sfd, 1);
    if (ret < 0) {
        buff_err("name %s socket %d listen failed errno %d\n", name, sfd, errno);
        (void)close(sfd);
        return -1;
    }

    return sfd;
}

static void server_socket_close(const char *name, int sfd)
{
    server_socket_file_remove(name);
    socket_close(sfd);
}

static int server_socket_accept(int sfd)
{
    int cfd;

    cfd = accept(sfd, NULL, NULL);
    if (cfd < 0) {
        buff_err("socket accept failed. (fd=%d; errno=%d)\n", sfd, errno);
    }

    return cfd;
}

static void msg_hdr_init(struct msghdr *msg, struct iovec *msg_iov, void *msg_control, size_t msg_controllen)
{
    struct cmsghdr *cmsg;

    msg->msg_iov = msg_iov;
    msg->msg_iovlen = 1;
    msg->msg_control = msg_control;
    msg->msg_controllen = msg_controllen;
    msg->msg_name = NULL;
    msg->msg_namelen = 0;
    msg->msg_flags = 0;

    cmsg = CMSG_FIRSTHDR(msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
}

static drvError_t send_shared_info(int sfd, struct grp_shr_info *shr_info)
{
    struct msghdr msg;
    char msg_control[CMSG_SPACE(sizeof(int))];
    struct iovec io = {.iov_base = shr_info, .iov_len = sizeof(*shr_info)};

    msg_hdr_init(&msg, &io, msg_control, sizeof(msg_control));

    *(int *)CMSG_DATA(CMSG_FIRSTHDR(&msg)) = get_grp_shr_mem_fd();

    buff_info("memfd %d max_mem_size %llx pool_id %d\n", get_grp_shr_mem_fd(), shr_info->max_mem_size,
              shr_info->pool_id);

    if (sendmsg(sfd, &msg, 0) < 0) {
        buff_err("sfd %d send msg failed errno %d\n", sfd, errno);
        return DRV_ERROR_SEND_MESG;
    }

    return DRV_ERROR_NONE;
}

static drvError_t recv_shared_info(int sfd, struct grp_shr_info *shr_info)
{
    struct msghdr msg;
    char msg_control[CMSG_SPACE(sizeof(int))];
    struct iovec io = {.iov_base = shr_info, .iov_len = sizeof(*shr_info)};

    msg_hdr_init(&msg, &io, msg_control, sizeof(msg_control));

    if (recvmsg(sfd, &msg, 0) < 0) {
        buff_err("sfd %d recv msg failed errno %d\n", sfd, errno);
        return DRV_ERROR_RECV_MESG;
    }

    set_grp_shr_mem_fd(*(int *)CMSG_DATA(CMSG_FIRSTHDR(&msg)));
    set_grp_pool_id(shr_info->pool_id);
    set_grp_max_mem_size(shr_info->max_mem_size);

    buff_info("memfd %d max_mem_size %llx pool_id %d\n", get_grp_shr_mem_fd(), shr_info->max_mem_size,
              shr_info->pool_id);

    return DRV_ERROR_NONE;
}

static drvError_t set_proc_init_result(int sfd, int result)
{
    long cnt;

    cnt = send(sfd, &result, sizeof(result), 0);
    if (cnt != (long)sizeof(result)) {
        buff_err("Send msg failed. (sfd=%d; cnt=%ld; errno=%d)\n", sfd, cnt, errno);
        return DRV_ERROR_SEND_MESG;
    }

    return DRV_ERROR_NONE;
}

static drvError_t get_proc_init_result(int sfd, int *result)
{
    long cnt;

    cnt = recv(sfd, result, sizeof(*result), 0);
    if (cnt != (long)sizeof(*result)) {
        buff_err("Resv msg failed. (sfd=%d; cnt=%ld; errno=%d)\n", sfd, cnt, errno);
        return DRV_ERROR_RECV_MESG;
    }

    return DRV_ERROR_NONE;
}

static drvError_t grp_add_proc(const char *name, int pid)
{
    struct grp_shr_info shr_info;
    drvError_t ret;
    int result = 0;
    int sfd;

    sfd = client_socket_connect(name, pid);
    if (sfd < 0) {
        buff_err("name %s socket failed\n", name);
        return DRV_ERROR_SOCKET_CONNECT;
    }

    ret = set_socket_timeout(sfd, GRP_ATTACH_MAX_TIMEOUT);
    if (ret != DRV_ERROR_NONE) {
        buff_err("name %s pid %d socket %d setsockopt failed\n", name, pid, sfd);
        goto out;
    }

    shr_info.max_mem_size = get_grp_max_mem_size();
    shr_info.pool_id = get_grp_pool_id();

    ret = send_shared_info(sfd, &shr_info);
    if (ret != DRV_ERROR_NONE) {
        buff_err("name %s shared info to proc %d failed\n", name, pid);
        goto out;
    }

    ret = get_proc_init_result(sfd, &result);
    if ((ret != DRV_ERROR_NONE) || (result != 0)) {
        buff_err("Proc init failed. (name=%s; proc=%d; ret=%d; result=%d)\n", name, pid, ret, result);
        ret = DRV_ERROR_INNER_ERR;
        goto out;
    }

    buff_info("Grp add success. (name=%s; proc=%d)\n", name, pid);

out:
    socket_close(sfd);
    return ret;
}

static drvError_t grp_buff_init(const char *name, int mem_fd, unsigned long long max_mem_size)
{
    unsigned long long proc_uid;
    unsigned int cache_type;
    GroupShareAttr attr;
    int pool_id;
    drvError_t ret;

    pool_id = get_grp_pool_id();

    ret = pool_id_name_check(pool_id, name);
    if (ret != DRV_ERROR_NONE) {
        buff_err("pool id %d name %s check failed\n", pool_id, name);
        return ret;
    }

    ret = buff_pool_attach_ex(pool_id, 0, &proc_uid, &cache_type);
    if (ret != DRV_ERROR_NONE) {
        buff_err("pool_id %d attach failed\n", pool_id);
        return ret;
    }

    buff_set_process_uni_id(proc_uid); // lint !e571
    set_grp_cache_type(cache_type);

    ret = buff_pool_task_attr_query(pool_id, buff_get_current_pid(), &attr);
    if (ret != DRV_ERROR_NONE) {
        (void)buff_pool_detach(pool_id);
        buff_err("pool_id %d get pid %d attr failed\n", pool_id, buff_get_current_pid());
        return ret;
    }

    ret = buff_pool_init(pool_id, mem_fd, max_mem_size, attr);
    if (ret != DRV_ERROR_NONE) {
        (void)buff_pool_detach(pool_id);
        buff_err("pool_id %d buff pool init failed\n", pool_id);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static drvError_t grp_attach(const char *name, int timeout)
{
    int sfd, cfd;
    drvError_t ret;
    struct grp_shr_info shr_info;

    sfd = server_socket_create(name);
    if (sfd < 0) {
        buff_err("name %s socket failed\n", name);
        return DRV_ERROR_SOCKET_CREATE;
    }

    ret = set_socket_timeout(sfd, timeout);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Setsockopt failed. (name=%s; socket_fd=%d; timeout=%dms)\n", name, sfd, timeout);
        goto close_server_socket;
    }

    buff_debug("accept begin. (name=%s; pid=%d)\n", name, buff_get_current_pid());
    cfd = server_socket_accept(sfd);
    if (cfd < 0) {
        buff_err("socket accept failed. (name=%s; fd=%d; errno=%d)\n", name, sfd, errno);
        ret = DRV_ERROR_SOCKET_ACCEPT;
        goto close_server_socket;
    }

    ret = set_socket_timeout(cfd, timeout);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Setsockopt failed. (name=%s; socket_fd=%d; timeout=%dms)\n", name, sfd, timeout);
        goto set_result;
    }

    ret = recv_shared_info(cfd, &shr_info);
    if (ret != DRV_ERROR_NONE) {
        buff_err("name %s cfd %d get shared info failed\n", name, cfd);
        goto set_result;
    }

    ret = grp_buff_init(name, get_grp_shr_mem_fd(), shr_info.max_mem_size);
    if (ret != DRV_ERROR_NONE) {
        buff_err("name %s buff pool init failed\n", name);
        goto set_result;
    }

    buff_info("get shared info success. (name=%s)\n", name);

set_result:
    (void)set_proc_init_result(cfd, (int)ret);
    socket_close(cfd);

close_server_socket:
    server_socket_close(name, sfd);

    return ret;
}

static drvError_t create_grp_para_check(const char *name, GroupCfg *cfg)
{
    int len;

    if ((name == NULL) || (cfg == NULL)) {
        buff_err("name or cfg is null\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    len = (int)strnlen(name, BUFF_GRP_NAME_LEN);
    if ((len >= BUFF_GRP_NAME_LEN) || (len == 0)) {
        buff_err("Name len is err. (len=%d; max len=%d)\n", len, BUFF_GRP_NAME_LEN);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (strchr(name, '/') != NULL) {
        buff_err("Name %s shouldn't have '/'.\n", name);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (buff_kb_to_b(cfg->maxMemSize) > BUFF_MEM_MAX_SIZE) {
        buff_err("name %s max_mem_size %llxKB error\n", name, cfg->maxMemSize);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((cfg->cacheAllocFlag == 1) && (cfg->maxMemSize == 0)) {
        buff_err("Max_mem_size is 0. (grp_name=%s)\n", name);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static drvError_t add_proc_para_check(const char *name, int pid, GroupShareAttr attr)
{
    int pool_id, pool_num, len;
    drvError_t ret;

    if (name == NULL) {
        buff_err("name is null\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    len = (int)strnlen(name, BUFF_GRP_NAME_LEN);
    if ((len >= BUFF_GRP_NAME_LEN) || (len == 0)) {
        buff_err("Name len err. (pid=%d; len=%d; max=%d)\n", pid, len, BUFF_GRP_NAME_LEN);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!is_authed_read(attr)) {
        buff_err("name %s should have read attr\n", name);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!is_authed_write(attr)) {
        buff_err("name %s should have write attr\n", name);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!is_authed_alloc(attr)) {
        buff_err("name %s should have alloc attr\n", name);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = buff_task_pool_query(pid, &pool_id, 1, &pool_num);
    if (ret == DRV_ERROR_NONE) {
        if (pool_num != 0) {
            buff_err("proc %d has been added to pool %d\n", pid, pool_id);
            return DRV_ERROR_REPEATED_INIT;
        }
    }

    ret = buff_task_adding_pool_query(pid, &pool_id, 1, &pool_num);
    if (ret == DRV_ERROR_NONE) {
        if (pool_num != 0) {
            buff_err("proc %d is adding to pool %d\n", pid, pool_id);
            return DRV_ERROR_REPEATED_INIT;
        }
    }

    return DRV_ERROR_NONE;
}

static drvError_t attach_grp_para_check(const char *name, int timeout)
{
    unsigned long len;

    if ((name == NULL) || (timeout > GRP_ATTACH_MAX_TIMEOUT) || (timeout <= 0)) {
        buff_err("Name is null or timeout. (timeout=%dms)\n", timeout);
        return DRV_ERROR_INVALID_VALUE;
    }

    len = strnlen(name, BUFF_GRP_NAME_LEN);
    if ((len >= BUFF_GRP_NAME_LEN) || (len == 0)) {
        buff_err("Name len is err. (len=%lu; max_len=%d)\n", len, BUFF_GRP_NAME_LEN);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (strchr(name, '/') != NULL) {
        buff_err("Name %s shouldn't have '/'.\n", name);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static unsigned long long get_grp_max_mem_size_from_cfg(GroupCfg *cfg)
{
    if (cfg->cacheAllocFlag == 0) {
        /* cache_alloc feature no support default or min mem_size */
        if (cfg->maxMemSize == 0) {
            buff_info("max_mem_size not config, use default %llx\n", cfg->maxMemSize);
            return BUFF_MEM_MAX_SIZE;
        } else if (buff_kb_to_b(cfg->maxMemSize) < BUFF_MEM_MIN_SIZE) {
            buff_info("max_mem_size is too small, use min %llx\n", cfg->maxMemSize);
            return BUFF_MEM_MIN_SIZE;
        }
    }

    return buff_kb_to_b(cfg->maxMemSize);
}

static drvError_t create_shr_mem_fd(const char *name, unsigned long long max_mem_size)
{
    int fd, ret;

    fd = (int)syscall(SYS_memfd_create, name, 0);
    if (fd < 0) {
        buff_err("file name %s memfd create failed errno %d\n", name, errno);
        return DRV_ERROR_INNER_ERR;
    }

    ret = ftruncate(fd, (long)max_mem_size);
    if (ret != 0) {
        buff_err("file name %s ftruncate failed errno %d\n", name, errno);
        (void)close(fd);
        return DRV_ERROR_INNER_ERR;
    }

    set_grp_shr_mem_fd(fd);

    buff_info("file memfd create success. (name=%s; fd=%d)\n", name, fd);

    return DRV_ERROR_NONE;
}

bool is_cache_size_valid(unsigned long long cache_size)
{
    if ((cache_size == 0) || (cache_size > (ULLONG_MAX / 1024))) {
        return false;
    }
    return (buff_kb_to_b(cache_size) == get_grp_max_mem_size());
}

drvError_t buff_is_support(void)
{
    static bool support_flag = false;
    unsigned int split_mode;
    drvError_t ret;
    if (support_flag == true) {
#ifndef EMU_ST
        return DRV_ERROR_NONE;
#endif
    }
    ret = halGetDeviceSplitMode(0, &split_mode);
    if (ret != DRV_ERROR_NONE) {
        buff_err("Get split mode failed. (ret=%d).\n", ret);
        return ret;
    }
    if (split_mode != VMNG_NORMAL_NONE_SPLIT_MODE) {
        buff_info("Capacity splitting is not supported. (split_mode=%u).\n", split_mode);
        return DRV_ERROR_NOT_SUPPORT;
    }
    support_flag = true;
    return DRV_ERROR_NONE;
}

static void set_add_grp_timeout(unsigned int add_grp_timeout)
{
    unsigned int timeout;

    timeout = (add_grp_timeout < SOCKET_CONNECT_MIN_TIMEOUT) ? SOCKET_CONNECT_MIN_TIMEOUT : add_grp_timeout;
    g_socket_connect_timeout = (timeout > SOCKET_CONNECT_MAX_TIMEOUT) ? SOCKET_CONNECT_MAX_TIMEOUT : timeout;
}

int halGrpCreate(const char *name, GroupCfg *cfg)
{
    int pool_id;
    drvError_t ret;
    unsigned long long max_mem_size;
    ret = buff_is_support();
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = create_grp_para_check(name, cfg);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    max_mem_size = get_grp_max_mem_size_from_cfg(cfg);

    (void)pthread_mutex_lock(&grp_mutex);

    if (get_grp_shr_mem_fd() >= 0) {
        buff_info("Pool has been registered, only support register one pool. (name=%s; grp_pool_id=%d)\n", name,
                  get_grp_pool_id());
        ret = DRV_ERROR_REPEATED_INIT;
        goto out;
    }

    if (cfg->cacheAllocFlag == 0) {
        ret = buff_pool_algo_vma_register(name, (unsigned long)max_mem_size, cfg->privMbufFlag, &pool_id);
        set_grp_cache_type(BUFF_NOCACHE);
    } else {
        ret = buff_pool_algo_cache_vma_register(name, (unsigned long)max_mem_size, cfg->privMbufFlag, &pool_id);
        set_grp_cache_type(BUFF_CACHE);
    }
    if (ret != DRV_ERROR_NONE) {
        buff_err("name %s register pool failed, len %llx\n", name, max_mem_size);
        goto out;
    }

    ret = create_shr_mem_fd(name, max_mem_size);
    if (ret != DRV_ERROR_NONE) {
        (void)buff_pool_unregister(pool_id);
        buff_err("name %s create_shr_mem_fd failed\n", name);
        goto out;
    }

    set_add_grp_timeout(cfg->addGrpTimeout);
    set_grp_pool_id(pool_id);
    set_grp_owner();
    set_grp_max_mem_size(max_mem_size);

    buff_info("Register success. (grp_name=%s; pool_id=%d; max_size=%llu; cache_flag=%u)\n", name, pool_id,
              cfg->maxMemSize, cfg->cacheAllocFlag);

out:
    (void)pthread_mutex_unlock(&grp_mutex);
    return ret;
}

int halGrpAddProc(const char *name, int pid, GroupShareAttr attr)
{
    int pool_id;
    drvError_t ret;

    ret = add_proc_para_check(name, pid, attr);
    if (ret != DRV_ERROR_NONE) {
        return (int)ret;
    }

    pool_id = get_grp_pool_id();
    if (pool_id < 0) {
        buff_err("name %s has not created grp.\n", name);
        return (int)DRV_ERROR_INVALID_VALUE;
    }

    ret = pool_id_name_check(pool_id, name);
    if (ret != DRV_ERROR_NONE) {
        buff_err("pool id %d name %s check failed.\n", pool_id, name);
        return (int)ret;
    }

    ret = buff_pool_add_proc(pool_id, pid, attr);
    if (ret != DRV_ERROR_NONE) {
#ifndef EMU_ST
        if (ret != DRV_ERROR_REPEATED_INIT) {
            buff_err("add proc failed. (pool_id=%d; pid=%d; ret=%d)\n", pool_id, pid, (int)ret);
        }
#endif
        return (int)ret;
    }

    /* add self not need share fd to peer */
    if (pid != buff_get_current_pid()) {
        ret = grp_add_proc(name, pid);
        if (ret != DRV_ERROR_NONE) {
            (void)buff_pool_del_proc(pool_id, pid);
            buff_err("pool_id %d share mem fd to pid %d failed\n", pool_id, pid);
            return (int)ret;
        }
    }

    return (int)ret;
}

int halGrpAttach(const char *name, int timeout)
{
    drvError_t ret;
    unsigned int flag;

    ret = attach_grp_para_check(name, timeout);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    (void)pthread_mutex_lock(&grp_mutex);

    if (is_grp_attach()) {
        buff_err("name %s buff pool is inited\n", name);
        ret = DRV_ERROR_INVALID_VALUE;
        goto out;
    }
    /* self not need get share info */
    if (is_grp_owner(buff_get_current_pid())) {
        ret = grp_buff_init(name, get_grp_shr_mem_fd(), get_grp_max_mem_size());
        if (ret != DRV_ERROR_NONE) {
            buff_err("name %s buff pool init failed\n", name);
            goto out;
        }
    } else {
        ret = grp_attach(name, timeout);
        if (ret != DRV_ERROR_NONE) {
            buff_err("name %s attach failed\n", name);
            goto out;
        }
    }

    set_grp_attach();

    if (buff_pool_priv_flag_query(get_grp_pool_id(), &flag) == DRV_ERROR_NONE) {
        mbuf_set_priv_flag(flag);
    }
    (void)pthread_mutex_unlock(&grp_mutex);

    buff_info("Group attach success. (name=%s; pid=%d; grp_id=%d)\n", name, buff_get_current_pid(), get_grp_pool_id());

    buf_recycle_init(get_grp_pool_id());

    return DRV_ERROR_NONE;

out:
    (void)pthread_mutex_unlock(&grp_mutex);

    return ret;
}
