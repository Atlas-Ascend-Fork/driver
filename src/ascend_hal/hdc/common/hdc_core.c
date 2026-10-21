/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdint.h>
#include <limits.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>

#include "dms_user_interface.h"
#include "hdc_cmn.h"
#include "hdc_cfg_parse.h"
#include "hdc_server.h"
#include "hdc_client.h"
#include "hdc_pcie_drv.h"
#include "hdc_ub_drv.h"
#include "hdcdrv_cmd_ioctl.h"
#include "hdc_core.h"
#include "hdc_adapt.h"
#include "dmc_log_user.h"

static unsigned int g_hdcPageSize = PAGE_SIZE;
static unsigned int g_hdcHugePageSize = HUGE_PAGE_SIZE;
static unsigned int g_hdcPageMask = PAGE_MASK;
static unsigned int g_hdcHugePageMask = HUGE_PAGE_MASK;
static unsigned int g_hdcPageSizeBit = PAGE_BIT;

signed int hdc_handle_count = HDC_INIT_GET_HANDLE_COUNT;
signed int hdc_access_count = HDC_ACCESS_COUNT;
signed int hdc_config_count = HDC_CONFIG_COUNT;
signed int hdc_max_device_num = HDC_MAX_DEVICE_NUM;
unsigned int hdc_max_vf_devid_start = MAX_VF_DEVID_START;
/* ----------------------------------------------*
 * Internal function prototype description                             *
 *---------------------------------------------- */
/* ----------------------------------------------*
 * Global variables                                     *
 *---------------------------------------------- */
struct hdcConfig g_hdcConfig = {0};

const char *g_errno_str[] = {
    "ok",
    "error",
    "invalid parameter",
    "copy from user fail",
    "copy to user fail",
    "service listening",
    "service not listening",
    "service accepting",
    "dma mem alloc fail",
    "no session",
    "send ctrl msg fail",
    "remote refused connect",
    "connect timeout",
    "tx queue full",
    "tx length error",
    "tx remote close",
    "rx buf small",
    "device not ready",
    "device reset",
    "no fast chan",
    "remote service no listening",
    "no block",
    "session has closed",
    "mem not match",
    "convert virtual id to physical id failed",
    "low power state",
    "no epoll fd ",
    "rx timeout",
    "tx timeout",
    "dma mem is used",
    "session id miss match",
    "mem alloc fail",
    "sq desc null",
    "fast node search fail",
    "dma copy fail",
    "safe mem operation fail ",
    "char dev create fail",
    "dma map fail",
    "find vma fail",
};
/* ----------------------------------------------*
 * Module-level variable                                   *
 *---------------------------------------------- */
/* ----------------------------------------------*
 * Constant definitions                                     *
 *---------------------------------------------- */

unsigned int get_err_str_count(void)
{
    return (sizeof(g_errno_str) / sizeof(char *));
}

signed int hdc_get_handle_count(void)
{
    return hdc_handle_count;
}

signed int hdc_get_access_count(void)
{
    return hdc_access_count;
}

signed int hdc_get_config_count(void)
{
    return hdc_config_count;
}

signed int hdc_get_max_device_num(void)
{
    return hdc_max_device_num;
}

unsigned int hdc_get_max_vf_dev_id_start(void)
{
    return hdc_max_vf_devid_start;
}

/*
 * hdc_pcie_create_bind_fd hdc_pcie_close_bind_fd hdc_pcie_mem_bind_fd
 * hdc_pcie_mem_unbind_fd
 * onetrack
 */
mmProcess hdc_pcie_create_bind_fd(void)
{
    mmProcess fd;
    int flag;

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
        fd = mmOpen2(PCIE_DEV_NAME, M_RDONLY, M_IRUSR);
        if (fd < 0) {
            HDC_LOG_ERR("open hdc dev file failed, errno %d\n", errno);
        } else {
            flag = fcntl(fd, F_GETFD);
            flag = (int)((unsigned int)flag | (unsigned int)FD_CLOEXEC);
            (void)fcntl(fd, F_SETFD, flag);
        }
        return fd;
    } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        return hdc_ub_open();
    } else {
        HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
        return EN_ERROR;
    }
}

void hdc_pcie_close_bind_fd(mmProcess fd)
{
    if (fd != HDC_SESSION_FD_INVALID) {
        if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
            (void)mm_close_file(fd);
        } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
            hdc_ub_close(fd);
        } else {
            HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
        }
    }
}

bool hdc_is_in_ub(void)
{
    if ((g_hdcConfig.trans_type != HDC_TRANS_USE_PCIE) || (g_hdcConfig.h2d_type != HDC_TRANS_USE_UB)) {
        return false;
    }

    return true;
}

STATIC hdcError_t hdc_get_trans_type(PCFGPARSE_CB_T handle, struct hdcConfig *config)
{
    char *value[MAX_VALUE_NUM] = {NULL};
    const char *val = NULL;
    signed int value_num, offset, trans_type;

    val = HDC_TRANS_TYPE;
    trans_type = 1;
    value[0] = NULL;
    offset = str_search(handle, val, value, &value_num);
    if ((offset != 0) && (value_num == 1)) {
        if (is_digit(value[0], MAX_VALUE_NUM)) {
            trans_type = atoi(value[0]);
            if ((trans_type > (int)HDC_TRANS_USE_PCIE) || (trans_type < 0)) {
                HDC_LOG_INFO("Hdc Config File(hdcBasic.cfg) TRANS_TYPE not correct, "
                             "Please Correct it And Restart Process hdcd. Use Default Type(PCIE)\n");
            }
        } else {
            HDC_LOG_INFO("Hdc Config File(hdcBasic.cfg) TRANS_TYPE not correct, "
                         "Please Correct it And Restart Process hdcd. Use Default Type(PCIE)\n");
        }
        config->trans_type = (enum halHdcTransType)trans_type;
    } else {
        return DRV_ERROR_CONFIG_READ_FAIL;
    }
    return DRV_ERROR_NONE;
}

STATIC hdcError_t hdc_get_segment_config(PCFGPARSE_CB_T handle, struct hdcConfig *config)
{
    char *value[MAX_VALUE_NUM];
    const char *val = NULL;
    signed int value_num, offset;

    val = HDC_SOCKET_SEGMENT;
    value[0] = NULL;
    offset = str_search(handle, val, value, &value_num);
    if ((offset != 0) && (value_num == 1)) {
        config->socket_segment = atoi(value[0]);
    } else {
        return DRV_ERROR_CONFIG_READ_FAIL;
    }

    val = HDC_PCIE_SEGMENT;
    offset = str_search(handle, val, value, &value_num);
    if ((offset != 0) && (value_num == 1)) {
        config->pcie_segment = atoi(value[0]);
    } else {
        return DRV_ERROR_CONFIG_READ_FAIL;
    }

    return DRV_ERROR_NONE;
}

STATIC bool hdc_mem_va_32bit_check(uintptr_t ptr)
{
#ifdef HDC_DFX
    if (sizeof(uintptr_t) == HDC_DFX_INT_SIZEOF) {
        return TRUE;
    }

    if ((((ptr >> HDC_DFX_SHIFT_32) & HDC_DFX_16_BIT_MASK) == HDC_DFX_16_BIT_MASK) ||
        (((ptr >> HDC_DFX_SHIFT_32) & HDC_DFX_15_BIT_MASK) == HDC_DFX_15_BIT_MASK)) {
        return TRUE;
    }
#endif
    return FALSE;
}

STATIC void hdc_sys_mem_info_show(unsigned int log_level)
{
#ifdef HDC_DFX
    unsigned long memTotal;
    unsigned long memFree;
    unsigned long buffers;
    unsigned long huge_page_free;
    struct sysinfo si = {0};

    (void)sysinfo(&si);
    memTotal = si.totalram / MEM_SIZE_1M;
    memFree = si.freeram / MEM_SIZE_1M;
    buffers = si.bufferram / MEM_SIZE_1M;
    huge_page_free = si.freehigh / MEM_SIZE_1M;

    if (log_level == HDC_MEM_LOG_LEVEL_ERR) {
        HDC_LOG_ERR("The current system mem_information (MB). (MemTotal=%lu; MemFree=%lu; Buffers=%lu; "
                    "HugeFree=%lu)\n",
                    memTotal, memFree, buffers, huge_page_free);
    } else {
        HDC_LOG_WARN("The current system mem_information (MB). (MemTotal=%lu; MemFree=%lu; Buffers=%lu; "
                     "HugeFree=%lu)\n",
                     memTotal, memFree, buffers, huge_page_free);
    }
#endif
}

STATIC void hdc_mem_info_update(unsigned int mem_flag, unsigned int len, bool is_alloc)
{
#ifdef HDC_DFX
    struct hdc_mem_fd_mng *fd_mng = &g_mem_fd_mng;

    (void)mmMutexLock(&fd_mng->mutex);

    if (is_alloc) {
        if (mem_flag & HDC_FLAG_MAP_HUGE) {
            fd_mng->mem_stat.huge_alloc_count++;
            fd_mng->mem_stat.huge_alloc_size += len;
        }

        if (mem_flag & HDC_FLAG_MAP_VA32BIT) {
            fd_mng->mem_stat.va32bit_alloc_count++;
            fd_mng->mem_stat.va32bit_alloc_size += len;
        }

        fd_mng->mem_stat.total_alloc_count++;
        fd_mng->mem_stat.total_alloc_size += len;
    } else {
        if (mem_flag & HDC_FLAG_MAP_HUGE) {
            fd_mng->mem_stat.huge_free_count++;
            fd_mng->mem_stat.huge_free_size += len;
        }

        if (mem_flag & HDC_FLAG_MAP_VA32BIT) {
            fd_mng->mem_stat.va32bit_free_count++;
            fd_mng->mem_stat.va32bit_free_size += len;
        }

        fd_mng->mem_stat.total_free_count++;
        fd_mng->mem_stat.total_free_size += len;
    }

    (void)mmMutexUnLock(&fd_mng->mutex);

#endif
}

STATIC void hdc_mem_info_show(unsigned int log_level)
{
#ifdef HDC_DFX
    struct hdc_mem_fd_mng *fd_mng = &g_mem_fd_mng;
    struct hdc_mem_stat_info *mem_stat = &(fd_mng->mem_stat);
    struct hdc_mem_stat_info mem_stat_show;

    (void)mmMutexLock(&fd_mng->mutex);
    mem_stat_show.huge_alloc_count = mem_stat->huge_alloc_count;
    mem_stat_show.huge_free_count = mem_stat->huge_free_count;
    mem_stat_show.huge_alloc_size = mem_stat->huge_alloc_size;
    mem_stat_show.huge_free_size = mem_stat->huge_free_size;
    mem_stat_show.va32bit_alloc_count = mem_stat->va32bit_alloc_count;
    mem_stat_show.va32bit_free_count = mem_stat->va32bit_free_count;
    mem_stat_show.va32bit_alloc_size = mem_stat->va32bit_alloc_size;
    mem_stat_show.va32bit_free_size = mem_stat->va32bit_free_size;
    mem_stat_show.total_alloc_count = mem_stat->total_alloc_count;
    mem_stat_show.total_free_count = mem_stat->total_free_count;
    mem_stat_show.total_alloc_size = mem_stat->total_alloc_size;
    mem_stat_show.total_free_size = mem_stat->total_free_size;
    (void)mmMutexUnLock(&fd_mng->mutex);

    if (log_level == HDC_MEM_LOG_LEVEL_ERR) {
        HDC_LOG_ERR("Get huge mem_information. (alloc_count=%llu; free_count=%llu; alloc_size=%llu B; "
                    "free_size=%llu B)\n",
                    mem_stat_show.huge_alloc_count, mem_stat_show.huge_free_count, mem_stat_show.huge_alloc_size,
                    mem_stat_show.huge_free_size);
        HDC_LOG_ERR("Get va32 mem_information. (alloc_count=%llu; free_count=%llu; alloc_size=%llu B; "
                    "free_size=%llu B)\n",
                    mem_stat_show.va32bit_alloc_count, mem_stat_show.va32bit_free_count,
                    mem_stat_show.va32bit_alloc_size, mem_stat_show.va32bit_free_size);
        HDC_LOG_ERR("Get total mem_information. (alloc_count=%llu; free_count=%llu; alloc_size=%llu B; "
                    "free_size=%llu B)\n",
                    mem_stat_show.total_alloc_count, mem_stat_show.total_free_count, mem_stat_show.total_alloc_size,
                    mem_stat_show.total_free_size);
    } else {
        HDC_LOG_WARN("Get huge mem_information. (alloc_count=%llu; free_count=%llu;"
                     "alloc_size=%llu B; free_size=%llu B)\n",
                     mem_stat_show.huge_alloc_count, mem_stat_show.huge_free_count, mem_stat_show.huge_alloc_size,
                     mem_stat_show.huge_free_size);
        HDC_LOG_WARN("Get va32 mem_information. (alloc_count=%llu; free_count=%llu;"
                     "alloc_size=%llu B; free_size=%llu B)\n",
                     mem_stat_show.va32bit_alloc_count, mem_stat_show.va32bit_free_count,
                     mem_stat_show.va32bit_alloc_size, mem_stat_show.va32bit_free_size);
        HDC_LOG_WARN("Get total mem_information. (alloc_count=%llu; free_count=%llu;"
                     "alloc_size=%llu B; free_size=%llu B)\n",
                     mem_stat_show.total_alloc_count, mem_stat_show.total_free_count, mem_stat_show.total_alloc_size,
                     mem_stat_show.total_free_size);
    }

#endif
}

STATIC signed int drv_hdc_get_page_size(mmProcess handle)
{
    unsigned int hdc_huge_page_size = 0;
    unsigned int hdc_page_size_bit = 0;
    unsigned int hdc_page_size = 0;
    signed int ret;

    if (handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Input parameter is error.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = hdc_pcie_get_page_size(handle, &hdc_page_size, &hdc_huge_page_size, &hdc_page_size_bit);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Get page size failed. (ret=%d; STRERROR=\"%s\")\n", ret, STRERROR(ret));
        return DRV_ERROR_IOCRL_FAIL;
    }
    g_hdcPageSize = hdc_page_size;
    g_hdcHugePageSize = hdc_huge_page_size;
    g_hdcPageSizeBit = hdc_page_size_bit;
    g_hdcPageMask = (~(g_hdcPageSize - 1));
    g_hdcHugePageMask = (~(g_hdcHugePageSize - 1));

    return DRV_ERROR_NONE;
}

STATIC hdcError_t hdc_init_get(PCFGPARSE_CB_T handle, struct hdcConfig *hdcConfig)
{
    hdcError_t ret;

    if ((ret = hdc_get_segment_config(handle, hdcConfig)) != DRV_ERROR_NONE) {
        HDC_LOG_INFO("Can not get segment Config. (socket=%d; pcie=%d)\n", hdcConfig->socket_segment,
                     hdcConfig->pcie_segment);
        return ret;
    }
    if ((ret = hdc_get_trans_type(handle, hdcConfig)) != DRV_ERROR_NONE) {
        HDC_LOG_INFO("Trans_Type not in Config File, use default trans_type.(trans_type=%d)\n", hdcConfig->trans_type);
    }

    return ret;
}

void *drv_hdc_zalloc(size_t size)
{
    void *pBuf = NULL;

    if (size == 0) {
        HDC_LOG_ERR("Input parameter is error. (size=%lu)\n", (unsigned long)size);
        return NULL;
    }

    pBuf = malloc(size);
    if (pBuf == NULL) {
        HDC_LOG_ERR("Call malloc failed. (size=%lu)\n", (unsigned long)size);
        return NULL;
    }

    if (memset_s(pBuf, size, 0, size) != 0) {
        free(pBuf);
        pBuf = NULL;
        HDC_LOG_ERR("Call memset_s failed. (strerror=\"%s\")\n", strerror(errno));
        return NULL;
    }

    return pBuf;
}

STATIC hdcError_t hdc_phandle_get(struct hdcConfig *hdcConfig)
{
    hdcError_t ret = DRV_ERROR_NONE;
    signed int i = hdc_get_handle_count();

    /* Waiting for permission to open /dev/hisi_dev */
    HDC_LOG_INFO("HDC trans_type=%d, h2d_type=%#x.\n", hdcConfig->trans_type, hdcConfig->h2d_type);
    while (i--) {
        if (hdcConfig->h2d_type == HDC_TRANS_USE_UB) {
            hdcConfig->pcie_handle = hdc_ub_open();
            HDC_LOG_INFO("Open ub.\n");
        } else if (hdcConfig->h2d_type == HDC_TRANS_USE_PCIE) {
            hdcConfig->pcie_handle = hdc_pcie_open();
            HDC_LOG_INFO("Open pcie.\n");
        } else {
            HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
            hdcConfig->pcie_handle = EN_ERROR;
            break;
        }
        if (hdcConfig->pcie_handle != (mmProcess)EN_ERROR) {
            break;
        }

        if (mm_get_error_code() == EACCES) {
            HDC_LOG_WARN("Open device not success, no permission. (strerror=\"%s\")", strerror(errno));
            return DRV_ERROR_DST_PERMISSION_DENIED;
        }

        HDC_LOG_WARN("Open device not success, will try again. (strerror=\"%s\")", strerror(errno));
        hdc_phandle_get_sleep();
    }

    if (hdcConfig->pcie_handle == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Open pcie device failed. (strerror=\"%s\")\n", strerror(errno));
        ret = DRV_ERROR_INVALID_HANDLE;
    }

    return ret;
}

hdcError_t hdc_pcie_init(struct hdcConfig *hdcConfig)
{
    hdcError_t ret;
    signed int count = 0;
    signed int max_access_num = hdc_get_access_count();

    if (hdcConfig->h2d_type != HDC_TRANS_USE_UB) {
        while (mmAccess(PCIE_DEV_NAME) != 0) {
            hdc_pcie_init_sleep();
            if ((count++) > max_access_num) {
                HDC_LOG_ERR("HDC init failed, driver may not load.\n");
                break;
            }
        }
    }

    if (hdc_phandle_get(hdcConfig) != DRV_ERROR_NONE) {
        HDC_LOG_WARN("HDC init not success, Got handle not success.");
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (hdcConfig->h2d_type != HDC_TRANS_USE_UB) {
        ret = drv_hdc_get_page_size(hdcConfig->pcie_handle);
        if (ret != DRV_ERROR_NONE) {
            HDC_LOG_ERR("Got page size failed. (ret=%d; strerror=\"%s\")\n", ret, STRERROR(ret));
            (void)mm_close_file(hdcConfig->pcie_handle);
            hdcConfig->pcie_handle = (mmProcess)EN_ERROR;
            return ret;
        }
    }

    count = 0;
reconfig:
    if ((ret = hdc_pcie_config(hdcConfig->pcie_handle, &hdcConfig->pcie_segment)) != DRV_ERROR_NONE) {
        hdc_pcie_init_sleep();
        count++;
        if (count < hdc_get_config_count()) {
            goto reconfig;
        }

        HDC_LOG_ERR("HDC init, set config failed. (ret=%d; strerror=\"%s\")\n", ret, STRERROR(ret));
        (void)mm_close_file(hdcConfig->pcie_handle);
        hdcConfig->pcie_handle = (mmProcess)EN_ERROR;
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC int hdc_get_h2d_type(struct hdcConfig *hdcConfig)
{
    int ret;
    int64_t h2d_type = 0;

    /* get h2d type from devmng:PCIE or UB */
    ret = dms_get_connect_type(&h2d_type);
    if (ret != 0) {
        HDC_LOG_INFO("Get h2d type from devmng not correct. (ret=%d)\n", ret);
#ifndef CFG_FEATURE_SUPPORT_UB
        hdcConfig->trans_type = HDC_TRANS_USE_PCIE;
        hdcConfig->h2d_type = HDC_TRANS_USE_PCIE;
#else
        hdcConfig->trans_type = HDC_TRANS_USE_PCIE;
        hdcConfig->h2d_type = HDC_TRANS_USE_UB;
#endif
        return DRV_ERROR_CONFIG_READ_FAIL;
    } else {
        hdcConfig->trans_type = HDC_TRANS_USE_PCIE;
        HDC_LOG_INFO("Get h2d type from devmng. (h2d_type=%lu)\n", h2d_type);
        if (h2d_type == HOST_DEVICE_CONNECT_TYPE_UB) {
            hdcConfig->h2d_type = HDC_TRANS_USE_UB;
        } else {
            hdcConfig->h2d_type = HDC_TRANS_USE_PCIE;
        }
    }

    return DRV_ERROR_NONE;
}

hdcError_t hdc_init(struct hdcConfig *hdcConfig)
{
    hdcError_t ret = DRV_ERROR_NONE;
    PCFGPARSE_CB_T handle = NULL;
    signed int result;

    HDC_SHARE_LOG_CREATE();
    hdcConfig->pcie_handle = (mmProcess)EN_ERROR;

    ret = hdc_set_init_info();
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("HDC set chip type not success. (ret=%d)\n", ret);
        HDC_SHARE_LOG_DESTROY();
        return ret;
    }

    result = cfg_file_open(HDC_CFG_FILE_NAME, &handle);
    if ((handle == NULL) || (result != DRV_ERROR_NONE)) {
#ifdef CFG_SOC_PLATFORM_RC
        hdcConfig->trans_type = HDC_TRANS_USE_SOCKET;
        hdcConfig->socket_segment = HDC_PCIE_MAX_SEGMENT;
#else
        hdcConfig->trans_type = HDC_TRANS_USE_PCIE;
#endif
        hdcConfig->pcie_segment = HDC_PCIE_MAX_SEGMENT;
    } else {
        ret = hdc_init_get(handle, hdcConfig);
        if (ret != DRV_ERROR_NONE) {
            hdcConfig->trans_type = HDC_TRANS_USE_PCIE;
            hdcConfig->pcie_segment = HDC_PCIE_MAX_SEGMENT;
        }
        cfg_file_close(handle);
    }

    if (hdcConfig->trans_type == HDC_TRANS_USE_PCIE) {
        (void)hdc_get_h2d_type(hdcConfig);
        ret = hdc_pcie_init(hdcConfig);
        if (ret != DRV_ERROR_NONE) {
            HDC_LOG_WARN("HDC Pcie Init not success. (ret=%d)\n", ret);
            HDC_SHARE_LOG_DESTROY();
            return ret;
        }
    }

    return DRV_ERROR_NONE;
}

hdcError_t hdc_init_mutex(struct hdcConfig *hdcConfig)
{
    hdcError_t ret;

    ret = hdc_init(hdcConfig);

    return ret;
}

signed int drv_hdc_socket_send(signed int sockfd, const char *buf, signed int len)
{
    mmSsize_t sendLen;
    signed int msg_len;
    int ret;
    int buf_pos = 0;

    if (buf == NULL) {
        HDC_LOG_ERR("Input parameter buf is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    msg_len = (signed int)htonl((unsigned int)len);

    sendLen = send(sockfd, (void *)&msg_len, sizeof(signed int), MSG_NOSIGNAL);
    if (sendLen != sizeof(signed int)) {
        HDC_LOG_ERR("Send data failed. (sockfd=%d; StrError=\"%s\"; errno=%d; sendLen=%ld; len=%d; sizeof_int=%d)\n",
                    sockfd, StrError(mm_get_error_code()), mm_get_error_code(), (long)sendLen, len,
                    (signed int)sizeof(signed int));
        return DRV_ERROR_SEND_MESG;
    }

    while (len > 0) {
        do {
            ret = (int)send(sockfd, buf + buf_pos, (unsigned long)len, MSG_NOSIGNAL);
        } while ((ret < 0) && (mm_get_error_code() == EINTR));

        if (ret < 0) {
            HDC_LOG_ERR("(Send data failed. (sockfd=%d; StrError=\"%s\"; errno=%d; sendLen=%ld; len=%d)\n", sockfd,
                        StrError(mm_get_error_code()), mm_get_error_code(), (long)sendLen, len);
            return DRV_ERROR_SEND_MESG;
        }
        buf_pos += ret;
        len -= ret;
    }

    return DRV_ERROR_NONE;
}

#define HDC_REPORT_ERR_MSG_STR_LEN 32
#define HDC_OUT_OF_MEM_ERROR_MSG_ARG_NUM 3
void hdc_report_out_of_mem_err_msg(const char *func_name, size_t size)
{
    char size_str[HDC_REPORT_ERR_MSG_STR_LEN] = {0};
    const char *key[] = {"func_name", "size", "moduleName"};
    const char *value[] = {func_name, size_str, "hdc"};
    int ret;

    ret = snprintf_s(size_str, sizeof(size_str), sizeof(size_str) - 1, "%zu bytes", size);
    if (ret < 0) {
        HDC_LOG_ERR("snprintf_s error(size=%lu).\n", (unsigned long)size);
        return;
    }
    REPORT_PREDEFINED_ERR_MSG("EL0018", key, value, HDC_OUT_OF_MEM_ERROR_MSG_ARG_NUM);
}

STATIC void drv_socket_set_recv_time_out(signed int sockfd, signed int wait, unsigned long timeout)
{
    struct timeval timeo;

    if ((wait == HDC_WAIT_ALWAYS) || (timeout == 0)) {
        /* unix socket default blocked */
        return;
    }

    if (wait == HDC_NOWAIT) {
        timeo.tv_sec = 0;
        timeo.tv_usec = HDC_SOCKET_DEFAULT_TIMEOUT;
    } else {
        timeo.tv_sec = (int)(timeout / CONVERT_MS_TO_S);
        timeo.tv_usec = (long)(timeout % CONVERT_MS_TO_S) * CONVERT_MS_TO_US;
    }

    /* set timeout */
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeo, sizeof(timeo)) < 0) {
        HDC_LOG_ERR("Set socket timeout failed. (strerror=\"%s\")\n", strerror(mm_get_error_code()));
    }

    return;
}

STATIC void drv_socket_clean_recv_time_out(signed int sockfd, signed int wait, unsigned int timeout)
{
    struct timeval timeo;

    if ((wait == HDC_WAIT_ALWAYS) || (timeout == 0)) {
        /* unix socket default blocked */
        return;
    }

    /* clean timeout */
    timeo.tv_sec = 0;
    timeo.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeo, sizeof(timeo)) < 0) {
        HDC_LOG_ERR("Set socket timeout failed. (strerror=\"%s\")\n", strerror(mm_get_error_code()));
    }

    return;
}

hdcError_t hdc_socket_recv_peek(signed int sockfd, unsigned int *msg_len, signed int wait, unsigned int timeout)
{
    signed int error_code = 0;
    struct timespec now_time = {0, 0};
    unsigned long start_time;
    unsigned long cost_time = 0;
    unsigned int recvlen;
    signed int ret = -1;

    (void)clock_gettime(CLOCK_MONOTONIC, &now_time);
    start_time = (unsigned long)((now_time.tv_sec * CONVERT_MS_TO_S) + (now_time.tv_nsec / CONVERT_MS_TO_NS));
    do {
        drv_socket_set_recv_time_out(sockfd, wait, (unsigned long)timeout - cost_time);

        ret = (int)recv(sockfd, (char *)&recvlen, sizeof(signed int), MSG_PEEK);
        if (ret < 0) {
            error_code = mm_get_error_code();
            (void)clock_gettime(CLOCK_MONOTONIC, &now_time);
            cost_time = ((unsigned long)((now_time.tv_sec * CONVERT_MS_TO_S) + (now_time.tv_nsec / CONVERT_MS_TO_NS)) -
                         start_time);
            if (cost_time >= (unsigned long)timeout) {
                ret = 0;
            }
        }
    } while ((ret < 0) && (error_code == EINTR));

    drv_socket_clean_recv_time_out(sockfd, wait, timeout);

    if (ret == 0) {
        HDC_LOG_INFO("Client connection closed. (Closed reason=\"%s\"; errno=%d; sock=%d)\n", StrError(error_code),
                     error_code, sockfd);
        return DRV_ERROR_SOCKET_CLOSE;
    } else if (ret < 0) {
        if (error_code == EAGAIN) {
            return DRV_ERROR_WAIT_TIMEOUT;
        } else {
            HDC_LOG_ERR("Receive error. (StrError=\"%s\"; errno=%d; sock=%d)\n", StrError(error_code), error_code,
                        sockfd);
            return DRV_ERROR_RECV_MESG;
        }
    } else if (ret != sizeof(signed int)) {
        HDC_LOG_ERR("Length should be sizeof_int. (sizeof_int=%d; sock=%d)\n", (signed int)sizeof(signed int), sockfd);
        return DRV_ERROR_INVALID_VALUE;
    }

    *msg_len = ntohl(recvlen);
    return DRV_ERROR_NONE;
}

signed int drv_hdc_socket_recv(signed int sockfd, char *pBuf, unsigned int msgLen, unsigned int *recvLen)
{
    unsigned int buf_pos;
    signed int ret = -1;
    signed int msg_len = 0;

    if (pBuf == NULL) {
        HDC_LOG_ERR("Input parameter pBuf is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    /* recv msg length */
    do {
        ret = (int)recv(sockfd, (char *)&msg_len, sizeof(signed int), 0);
    } while ((ret < 0) && (mm_get_error_code() == EINTR));

    if (ret == 0) {
        HDC_LOG_ERR("Client connection closed. (StrError=\"%s\"; errno=%d; sock=%d)\n", StrError(mm_get_error_code()),
                    mm_get_error_code(), sockfd);
        return DRV_ERROR_SOCKET_CLOSE;
    } else if (ret < 0) {
        HDC_LOG_ERR("Receive error. (StrError=\"%s\"; errno=%d; sock=%d)\n", StrError(mm_get_error_code()),
                    mm_get_error_code(), sockfd);
        return DRV_ERROR_RECV_MESG;
    }

    msg_len = (signed int)ntohl((unsigned int)msg_len);
    if ((unsigned int)msg_len != msgLen) {
        HDC_LOG_ERR("Receive message length not equal. (msg_len=%d; msgLen=%u; sock=%d)\n", msg_len, msgLen, sockfd);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* recv msg body */
    buf_pos = 0;

    while (msg_len > 0) {
        ret = -1;
        do {
            ret = (int)recv(sockfd, pBuf + buf_pos, (unsigned long)msg_len, 0);
        } while ((ret < 0) && (mm_get_error_code() == EINTR));

        if (ret == 0) {
            HDC_LOG_ERR("Client connection closed. (StrError=\"%s\"; errno=%d; sock=%d)\n",
                        StrError(mm_get_error_code()), mm_get_error_code(), sockfd);
            return DRV_ERROR_SOCKET_CLOSE;
        } else if (ret < 0) {
            HDC_LOG_ERR("Receive error. (StrError=\"%s\"; errno=%d; sock=%d)\n", StrError(mm_get_error_code()),
                        mm_get_error_code(), sockfd);
            return DRV_ERROR_RECV_MESG;
        }
        buf_pos += (unsigned int)ret;
        msg_len -= ret;
    }

    *recvLen = buf_pos;
    return DRV_ERROR_NONE;
}

/* Length acquisition rules
 * 1.Firstly len align to 4k
 * 2.The part less than 256k is aligned upwards to 4k*2^m.
 * 3.That is, last_len = 4k*n, keeping n aligned upwards yto align_len = 4k*2^m.
 * 4.alloc_len = alloc_len
 */
unsigned int hdc_get_alloc_len(unsigned int len, unsigned int flag)
{
    unsigned int alloc_len, last_len, align_len;
    unsigned int n_bit = 0;
    if (flag & HDC_FLAG_MAP_HUGE) {
        alloc_len = (unsigned int)((len + g_hdcHugePageSize - 1U) & g_hdcHugePageMask);
        return alloc_len;
    }

    alloc_len = (len + g_hdcPageSize - 1U) & g_hdcPageMask;
    last_len = alloc_len & LAST_MASK;

    if (last_len == 0) {
        return alloc_len;
    }

    for (n_bit = HDCDRV_MEM_MIN_LEN_BIT - 1; n_bit > g_hdcPageSizeBit; n_bit--) {
        if (last_len >> n_bit) {
            break;
        }
    }

    if (last_len == (1u << n_bit)) {
        return alloc_len;
    }

    align_len = 1u << (n_bit + 1);
    alloc_len = alloc_len - last_len + align_len;
    return alloc_len;
}

STATIC void drv_hdc_init_buf(const void *buf, unsigned int init_flag, unsigned int alloc_len, unsigned int page_size)
{
    unsigned int offset = 0;
    if (init_flag != 0) {
        for (offset = 0; offset < alloc_len; offset += page_size) {
            *(signed int *)((uintptr_t)buf + offset) = 1;
        }
    }
}

STATIC signed int drv_hdc_malloc_para_check(enum drvHdcMemType mem_type, unsigned int align, unsigned int len,
                                            unsigned int flag)
{
    if (g_hdcConfig.trans_type != HDC_TRANS_USE_PCIE) {
        HDC_LOG_ERR("Socket mode not support. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        HDC_LOG_ERR("UB mode not support. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mem_type >= HDC_MEM_TYPE_MAX) {
        HDC_LOG_ERR("Input parameter mem_type is error. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((len == 0) || (len > HDCDRV_MEM_MAX_LEN)) {
        HDC_LOG_ERR("Input parameter len is error. (len=%d; mem_type=%d)\n", len, mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((align != 0) && (align != g_hdcPageSize) && (align != g_hdcHugePageSize)) {
        HDC_LOG_WARN("Input parameter is invalid, use 4k or 2M instead. (align=%d)\n", align);
    }

    if ((flag & HDC_FLAG_MAP_HUGE) && (flag & HDC_FLAG_MAP_VA32BIT)) {
        HDC_LOG_ERR("Input parameter is error. (flag=0x%x)\n", flag);
        return DRV_ERROR_INVALID_VALUE;
    }

    return drv_hdc_alloc_len_check(mem_type, len, flag);
}

STATIC void drv_hdc_mmap_fail_info_show(enum drvHdcMemType mem_type, unsigned int alloc_len, unsigned int flag)
{
    if (HDC_BIT_MAP_IS_HUGE(flag) == 0) {
        HDC_LOG_ERR("Mmap failed. (mem_type=%d; errno=%d; StrError=\"%s\"; len=%u; flag=%u)\n", mem_type,
                    mm_get_error_code(), StrError(mm_get_error_code()), alloc_len, flag);
        hdc_mem_info_show(HDC_MEM_LOG_LEVEL_ERR);
        hdc_sys_mem_info_show(HDC_MEM_LOG_LEVEL_ERR);
    } else {
        HDC_LOG_WARN("Mmap not success. (mem_type=%d; errno=%d; StrError=\"%s\"; len=%d; flag=%d)\n", mem_type,
                     mm_get_error_code(), StrError(mm_get_error_code()), alloc_len, flag);
        hdc_mem_info_show(HDC_MEM_LOG_LEVEL_WARN);
        hdc_sys_mem_info_show(HDC_MEM_LOG_LEVEL_WARN);
    }
}

void *drvHdcMallocEx(enum drvHdcMemType mem_type, void *addr, unsigned int align, unsigned int len, signed int devid,
                     unsigned int flag)
{
    void *buf = NULL;
    unsigned int alloc_len; // page align
    signed int map = 1;
    mmProcess fd = HDC_SESSION_FD_INVALID;
    signed int ret;
    struct hdc_alloc_mem mem_info;

    /* "addr" is used for mmap, it could be NULL, no need to check. */
    if (drv_hdc_malloc_para_check(mem_type, align, len, flag) != DRV_ERROR_NONE) {
        return NULL;
    }

    drv_hdc_set_malloc_flag(&flag);

    if ((devid >= hdc_get_max_device_num()) || (devid < 0)) {
        map = 0;
        /* set devid valid value, let kernel performance normal. */
        devid = 0;
    }

    alloc_len = hdc_get_alloc_len(len, flag);

    fd = hdc_pcie_mem_bind_fd();
    if (fd == (mmProcess)EN_ERROR) {
        HDC_LOG_ERR("Set reference open pcie device failed. (errno=%d; StrError=\"%s\")\n", mm_get_error_code(),
                    StrError(mm_get_error_code()));
        return NULL;
    }

    buf = drv_hdc_mmap(fd, addr, alloc_len, flag);
    if (buf == MAP_FAILED) {
        hdc_pcie_mem_unbind_fd();
        fd = HDC_SESSION_FD_INVALID;
        drv_hdc_mmap_fail_info_show(mem_type, alloc_len, flag);
        return NULL;
    }

    drv_hdc_init_buf(buf, HDC_BIT_MAP_IS_HUGE(flag), alloc_len, g_hdcHugePageSize);

    mem_info.alloc_len = alloc_len;
    mem_info.devid = devid;
    mem_info.map = map;
    ret = hdc_pcie_alloc_mem(fd, (signed int)mem_type, &mem_info, (UINT64)(uintptr_t)buf, HDC_BIT_MAP_IS_HUGE(flag));
    if (ret != DRV_ERROR_NONE) {
        hdc_mem_info_show(HDC_MEM_LOG_LEVEL_ERR);
        hdc_sys_mem_info_show(HDC_MEM_LOG_LEVEL_ERR);
        hdc_pcie_mem_unbind_fd();
        fd = HDC_SESSION_FD_INVALID;

        HDC_LOG_ERR("Alloc memory failed. (len=%d; errno=%d; STRERROR=\"%s\"; len=%d; flag=%d; mem_type=%d)\n",
                    alloc_len, ret, STRERROR(ret), alloc_len, flag, mem_type);

        if (munmap(buf, alloc_len) != 0) {
            HDC_LOG_ERR("Call munmap failed. (StrError=\"%s\"; errno=%d)\n", StrError(mm_get_error_code()),
                        mm_get_error_code());
        }

        buf = NULL;
        return NULL;
    }

    hdc_mem_info_update(flag, alloc_len, HDC_MEM_ALLOC);

    drv_hdc_init_buf(buf, !(HDC_BIT_MAP_IS_HUGE(flag)), alloc_len, g_hdcPageSize);

    return buf;
}

hdcError_t drvHdcFreeEx(enum drvHdcMemType mem_type, void *buf)
{
    signed int ret;
    unsigned int len = 0;
    unsigned int page_type = 0;
    unsigned int mem_flag;

    if (mem_type >= HDC_MEM_TYPE_MAX) {
        HDC_LOG_ERR("Input parameter mem_type is error. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (buf == NULL) {
        HDC_LOG_ERR("Input parameter buf is NULL. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.trans_type != HDC_TRANS_USE_PCIE) {
        HDC_LOG_ERR("Socket mode not support (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        HDC_LOG_ERR("UB mode not support.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = hdc_pcie_free_mem(g_mem_fd_mng.mem_fd, (signed int)mem_type, (UINT64)buf, &len, &page_type);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Free memory failed. (errno=%d; STRERROR=\"%s\"; mem_type=%d)\n", ret, STRERROR(ret), mem_type);
        return DRV_ERROR_IOCRL_FAIL;
    }

    hdc_pcie_mem_unbind_fd();

    mem_flag = (page_type != 0) ? HDC_FLAG_MAP_HUGE : 0;
    if (hdc_mem_va_32bit_check((uintptr_t)buf)) {
        mem_flag |= HDC_FLAG_MAP_VA32BIT;
    }

    if (munmap(buf, len) != 0) {
        HDC_LOG_ERR("Call munmap failed. (STRERROR=\"%s\"; errno=%d; len=%d)\n", StrError(mm_get_error_code()),
                    mm_get_error_code(), len);
    }

    hdc_mem_info_update(mem_flag, len, HDC_MEM_FREE);

    buf = NULL;
    return DRV_ERROR_NONE;
}

hdcError_t drvHdcDmaMap(enum drvHdcMemType mem_type, void *buf, signed int devid)
{
    signed int ret;

    if (buf == NULL) {
        HDC_LOG_ERR("Input parameter buf is NULL. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devid >= hdc_get_max_device_num()) || (devid < 0)) {
        HDC_LOG_ERR("Input parameter devid is error. (dev_id=%d; mem_type=%d)\n", devid, mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mem_type >= HDC_MEM_TYPE_MAX) {
        HDC_LOG_ERR("Input parameter mem_type is error. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        HDC_LOG_ERR("UB mode not support.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = hdc_pcie_dma_map(g_hdcConfig.pcie_handle, (signed int)mem_type, (uintptr_t)buf, devid);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("DMA map failed. (errno=%d; STRERROR=\"%s\"; mem_type=%d; dev_id=%d)\n", ret, STRERROR(ret),
                    mem_type, devid);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

hdcError_t drvHdcDmaUnMap(enum drvHdcMemType mem_type, void *buf)
{
    signed int ret;

    if (buf == NULL) {
        HDC_LOG_ERR("Input parameter buf is NULL. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mem_type >= HDC_MEM_TYPE_MAX) {
        HDC_LOG_ERR("Input parameter mem_type is error. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        HDC_LOG_ERR("UB mode not support.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((ret = hdc_pcie_dma_unmap(g_hdcConfig.pcie_handle, (signed int)mem_type, (uintptr_t)buf)) != DRV_ERROR_NONE) {
        HDC_LOG_ERR("DMA unmap failed. (errno=%d; STRERROR=\"%s\"; mem_type=%d)\n", ret, STRERROR(ret), mem_type);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

hdcError_t drvHdcDmaReMap(enum drvHdcMemType mem_type, void *buf, signed int devid)
{
    signed int ret;

    if (buf == NULL) {
        HDC_LOG_ERR("Input parameter buf is NULL. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((devid >= hdc_get_max_device_num()) || (devid < 0)) {
        HDC_LOG_ERR("Input parameter devid is error. (dev_id=%d; mem_type=%d)\n", devid, mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (mem_type >= HDC_MEM_TYPE_MAX) {
        HDC_LOG_ERR("Input parameter mem_type is error. (mem_type=%d)\n", mem_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        HDC_LOG_ERR("UB mode not support.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((ret = hdc_pcie_dma_remap(g_hdcConfig.pcie_handle, (signed int)mem_type, (uintptr_t)buf, devid)) !=
        DRV_ERROR_NONE) {
        HDC_LOG_ERR("DMA remap failed. (errno=%d; STRERROR=\"%s\"; mem_type=%d; dev_id=%d)\n", ret, STRERROR(ret),
                    mem_type, devid);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcAllocMsg
 * Function Description  : Apply for MSG descriptor for sending and receiving
 * Input Parameters      : HDC_SESSION session        Specify in which session to receive data
 *                        signed int count           Number of buffers in the message descriptor. Currently only one
 *                                                   is supported
 * Output Parameters     : struct drvHdcMsg *ppMsg    Message descriptor pointer, used to store the send and receive
 *                                                   buffer address and length
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcAllocMsg(HDC_SESSION session, struct drvHdcMsg **ppMsg, signed int count)
{
    struct hdc_server_session *p_serv_session = NULL;
    struct hdc_msg_head *p_hdc_msg_head = NULL;
    size_t hdc_msg_len;

    if (session == NULL) {
        HDC_LOG_ERR("Input parameter session is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    p_serv_session = (struct hdc_server_session *)session;
    if (p_serv_session->session.magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Variable magic is error. (magic=%#x)\n", p_serv_session->session.magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (ppMsg == NULL) {
        HDC_LOG_ERR("Input parameter ppMsg is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (count != 1) {
        HDC_LOG_ERR("Input parameter is error, count just support 1, for future feature.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    hdc_msg_len = sizeof(struct hdc_msg_head) + (size_t)count * sizeof(struct drvHdcMsgBuf);

    p_hdc_msg_head = (struct hdc_msg_head *)drv_hdc_zalloc(hdc_msg_len);
    if (p_hdc_msg_head == NULL) {
        HDC_LOG_ERR("Call malloc failed.\n");
        return DRV_ERROR_MALLOC_FAIL;
    }

    p_hdc_msg_head->freeBuf = false;
    p_hdc_msg_head->msg.count = count;
    if (p_hdc_msg_head->msg.bufList[0].pBuf != NULL) {
        HDC_LOG_WARN("pbuf is not null.\n");
        p_hdc_msg_head->msg.bufList[0].pBuf = NULL;
    }

    *ppMsg = &p_hdc_msg_head->msg;

    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcFreeMsg
 * Function Description  : Release MSG descriptor for sending and receiving
 * Input Parameters      : struct drvHdcMsg *msg     Pointer to message descriptor to be released
 * Output Parameters     : None
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcFreeMsg(struct drvHdcMsg *msg)
{
    struct hdc_msg_head *p_hdc_msg_head = NULL;
    struct drvHdcMsgBuf *p_msg_buf = NULL;
    unsigned int buf_num;
    unsigned int count;

    if (msg == NULL) {
        HDC_LOG_ERR("Input parameter is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (msg->count != 1) {
        HDC_LOG_ERR("Input parameter is error. (pMsg_count=%d)\n", msg->count);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* Release two parts: 1 p_hdc_msg_head, 2 the address pointed to by pBuf in drvHdcMsgBuf. */
    /* Memory allocated by the caller must be released by the caller themselves;
       the driver only releases the memory allocated by the driver. */
    p_hdc_msg_head = container_of(msg, struct hdc_msg_head, msg); // lint !e102 !e42

    /* free is false, no need to free pBuf */
    if (!p_hdc_msg_head->freeBuf) { // lint !e413
        free(p_hdc_msg_head);
        return DRV_ERROR_NONE;
    }

    /* free is true, free pBuf */
    buf_num = (unsigned int)msg->count;
    p_msg_buf = msg->bufList;

    for (count = 0; count < buf_num; count++) {
        if (p_msg_buf[count].pBuf != NULL) {
            free(p_msg_buf[count].pBuf);
            p_msg_buf[count].pBuf = NULL;
        }
    }
    free(p_hdc_msg_head);
    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcReuseMsg
 * Function Description  : Reuse MSG descriptor
 * Input Parameters      : struct drvHdcMsg *msg     The pointer of message need to Reuse
 * Output Parameters     : None
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcReuseMsg(struct drvHdcMsg *msg)
{
    struct hdc_msg_head *p_hdc_msg_head = NULL;
    struct drvHdcMsgBuf *p_msg_buf = NULL;
    bool freeBuf = false;
    unsigned int buf_num;
    unsigned int count;

    if (msg == NULL) {
        HDC_LOG_ERR("Input parameter msg is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (msg->count != 1) {
        HDC_LOG_ERR("Input parameter count is error. (pMsg_count=%d)\n", msg->count);
        return DRV_ERROR_INVALID_VALUE;
    }

    p_hdc_msg_head = container_of(msg, struct hdc_msg_head, msg); // lint !e102 !e14 !e42 !e564
    freeBuf = p_hdc_msg_head->freeBuf;                            // lint !e413

    /* only support offline */
    p_msg_buf = msg->bufList;
    buf_num = (unsigned int)msg->count;
    for (count = 0; count < buf_num; count++) {
        if (freeBuf && (p_msg_buf[count].pBuf != NULL)) {
            free(p_msg_buf[count].pBuf);
        }
        p_msg_buf[count].pBuf = NULL;
        p_msg_buf[count].len = 0;
    }

    /* �ͷű�־ҲҪ�ָ���false */
    p_hdc_msg_head->freeBuf = false; // lint !e413
    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcAddMsgBuffer
 * Function Description  : Add the receiving and sending buffer to the MSG descriptor
 * Input Parameters      : struct drvHdcMsg *msg  The pointer of the message need to be operated
 *                        char *pBuf             Buffer pointer to be added
 *                        signed int len         The length of the effective data to be added
 * Output Parameters     : None
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcAddMsgBuffer(struct drvHdcMsg *msg, char *pBuf, signed int len)
{ // lint !e564
    struct drvHdcMsgBuf *p_msg_buf = NULL;
    unsigned int buf_num;
    unsigned int count;

    if (msg == NULL) {
        HDC_LOG_ERR("Input parameter msg is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pBuf == NULL) {
        HDC_LOG_ERR("Input parameter pBuf is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (len <= 0) {
        HDC_LOG_ERR("Input parameter len is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (msg->count != 1) {
        HDC_LOG_ERR("Input parameter count is error.(pMsg_count=%d)\n", msg->count);
        return DRV_ERROR_INVALID_VALUE;
    }

    p_msg_buf = msg->bufList;
    buf_num = (unsigned int)msg->count;

    for (count = 0; count < buf_num; count++) {
        if (p_msg_buf[count].pBuf == NULL) {
            break;
        }
    }

    if (count == buf_num) {
        HDC_LOG_ERR("No available message BD.\n");
        return DRV_ERROR_OVER_LIMIT;
    }

    p_msg_buf[count].pBuf = pBuf;
    p_msg_buf[count].len = len;

    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcGetMsgBuffer
 * Function Description  : Pointer to the message descriptor to be manipulated
 * Input Parameters      : struct drvHdcMsg *msg  Pointer to the message descriptor to be operated
 *                        signed int index       The first several buffers need to be obtained, but currently only
 *                                                supports one, be fixed to 0
 * Output Parameters     : char **pBuf           Obtained Buffer pointer
 *                        signed int *pLen      Length of valid data that can be obtained from the Buffer
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcGetMsgBuffer(struct drvHdcMsg *msg, signed int index, char **pBuf, signed int *pLen)
{
    struct drvHdcMsgBuf *p_msg_buf = NULL;

    if (msg == NULL) {
        HDC_LOG_ERR("Input parameter msg is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pBuf == NULL) {
        HDC_LOG_ERR("Input parameter pBuf is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pLen == NULL) {
        HDC_LOG_ERR("Input parameter pLen is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (index != 0) {
        HDC_LOG_ERR("Input parameter index is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    p_msg_buf = msg->bufList;

    *pBuf = p_msg_buf[index].pBuf;
    *pLen = p_msg_buf[index].len;

    return DRV_ERROR_NONE;
}

STATIC hdcError_t drv_hdc_recv_msg_len(struct hdc_session *pSession, unsigned int *msgLen,
                                       struct hdc_recv_config *recvConfig)
{
    hdcError_t err = DRV_ERROR_SOCKET_CONNECT;
    signed int session_fd = pSession->sockfd;
    unsigned int msg_len = 0;
    signed int ret = -1;

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
        if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
            ret = hdc_ub_recv_peek(pSession, (signed int *)&msg_len, recvConfig);
        } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
            ret = hdc_pcie_recv_peek(g_hdcConfig.pcie_handle, pSession, (signed int *)&msg_len, recvConfig);
        } else {
            HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
            return DRV_ERROR_INVALID_HANDLE;
        }

        if (ret != DRV_ERROR_NONE) {
            if (ret == (-HDCDRV_NO_BLOCK)) {
                return DRV_ERROR_NON_BLOCK;
            } else if (ret == (-HDCDRV_RX_TIMEOUT)) {
                return DRV_ERROR_WAIT_TIMEOUT;
            } else if (ret == (-HDCDRV_SESSION_HAS_CLOSED)) {
                return DRV_ERROR_SOCKET_CLOSE;
            } else if (ret == (-HDCDRV_NO_PERMISSION)) {
                return DRV_ERROR_OPER_NOT_PERMITTED;
            } else if (ret == DRV_ERROR_INVALID_HANDLE) {
                /* No error log is generated when the module is destructed (DRV_ERROR_INVALID_HANDLE) */
                return DRV_ERROR_INVALID_HANDLE;
            } else if (ret == (-HDCDRV_DEVICE_NOT_READY)) {
                return DRV_ERROR_DEVICE_NOT_READY;
            } else if (ret == (-HDCDRV_PEER_REBOOT)) {
                return DRV_ERROR_SOCKET_CLOSE;
            } else {
                HDC_LOG_ERR("Got receive message length failed. (errno=%d; STRERROR=\"%s\"; session=%d)\n", ret,
                            STRERROR(ret), session_fd);
            }

            return ret;
        }

        if (msg_len == 0) {
            /* session has closed, can't access pSession anymore */
            HDC_LOG_INFO("The session local or remote was closed. (session=%d)\n", session_fd);
            *msgLen = 0;
            return DRV_ERROR_SOCKET_CLOSE;
        }
    } else {
        if ((err = hdc_socket_recv_peek(session_fd, &msg_len, recvConfig->wait, recvConfig->timeout)) !=
            DRV_ERROR_NONE) {
            if (err != DRV_ERROR_SOCKET_CLOSE) {
                HDC_LOG_ERR("Got receive message length failed. (ret=%d)\n", err);
            }
            return err;
        }

        /* unlikely */
        if (msg_len == 0) {
            *msgLen = 0;
            return DRV_ERROR_NONE;
        }
        recvConfig->buf_count = 1;
    }

    if (msg_len > PKT_SIZE_1G) {
        HDC_LOG_ERR("msg_length too large. (msg_len=%d; support_len=%dByte) \n", msg_len, PKT_SIZE_1G);
        return DRV_ERROR_OVER_LIMIT;
    }

    *msgLen = msg_len;
    return DRV_ERROR_NONE;
}

STATIC hdcError_t drv_hdc_add_msg_body(struct hdc_session *pSession, struct drvHdcMsg *pMsg, char **pBuf,
                                       unsigned int *msgLen, struct hdc_recv_config *recvConfig)
{
    hdcError_t ret;
    signed int session_fd = pSession->sockfd;
    unsigned int msg_len = 0;

    ret = drv_hdc_recv_msg_len(pSession, &msg_len, recvConfig);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (msg_len == 0) {
        HDC_LOG_ERR("Parameter msg_len is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    *pBuf = NULL;
    *pBuf = (char *)malloc(msg_len);

    if (*pBuf == NULL) {
        HDC_LOG_ERR("Call malloc failed. (sock=%d)\n", session_fd);
        hdc_report_out_of_mem_err_msg("halHdcRecvEx", (size_t)msg_len);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = drvHdcAddMsgBuffer(pMsg, *pBuf, (signed int)msg_len);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Call drvHdcAddMsgBuffer failed. (ret=%d; sock=%d)\n", ret, session_fd);
        free(*pBuf);
        *pBuf = NULL;
        return ret;
    }

    *msgLen = msg_len;
    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drv_hdc_recv_msg_body
 * Function Description  : Receive data (message length) based on HDC Session
 * Input Parameters      : struct hdc_session *pSession    Specify the session for data reception
 *                        signed int msg_len             Length of each received Buffer in bytes (only meaningful in
 *offline mode) Output Parameters     : struct drvHdcMsg **pMsg        Pointer to the descriptor for receiving messages
 *                        signed int *buf_pos            Actually received message length
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : May 2, 2018
 *  Modification         : Newly generated function (optimized cyclomatic complexity)
 *
 *****************************************************************************/
STATIC hdcError_t drv_hdc_recv_msg_body(struct hdc_session *pSession, char *pBuf, unsigned int bufLen,
                                        unsigned int *msgLen, struct hdc_recv_config *recvConfig)
{
    signed int ret;

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
        if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
            ret = hdc_ub_recv(pSession, pBuf, (signed int)bufLen, (signed int *)msgLen, recvConfig);
        } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
            ret = hdc_pcie_recv(g_hdcConfig.pcie_handle, pSession, pBuf, (signed int)bufLen, (signed int *)msgLen,
                                recvConfig);
        } else {
            HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
            return DRV_ERROR_INVALID_HANDLE;
        }
        if (ret != DRV_ERROR_NONE) {
            /* No error log is generated when the module is destructed (DRV_ERROR_INVALID_HANDLE) */
            if (ret == DRV_ERROR_INVALID_HANDLE) {
                return DRV_ERROR_INVALID_HANDLE;
            }
            HDC_LOG_ERR("Pcie receive message failed. (session_id=%d; errno=%d; STRERROR=\"%s\")\n", pSession->sockfd,
                        ret, STRERROR(ret));
            if (drv_hdc_recv_msg_body_ret_check(ret) == DRV_ERROR_SOCKET_CLOSE) {
                return DRV_ERROR_SOCKET_CLOSE;
            }
            return DRV_ERROR_RECV_MESG;
        }
    } else {
        ret = drv_hdc_socket_recv(pSession->sockfd, pBuf, bufLen, msgLen);
        if (ret != DRV_ERROR_NONE) {
            HDC_LOG_ERR("Socket receive message failed. (session_id=%d; errno=%d)\n", pSession->sockfd, ret);
            return DRV_ERROR_RECV_MESG;
        }
    }
    return DRV_ERROR_NONE;
}

STATIC int drv_hdc_get_wait_type(UINT64 flag) // lint !e527
{
    int wait;

    if (flag & HDC_FLAG_WAIT_TIMEOUT) {
        wait = HDC_WAIT_TIMEOUT;
    } else if (flag & HDC_FLAG_NOWAIT) {
        wait = HDC_NOWAIT;
    } else {
        wait = HDC_WAIT_ALWAYS;
    }

    return wait;
}

hdcError_t drvHdcRecvPeek(HDC_SESSION session, signed int *msgLen, signed int flag)
{
    struct hdc_session *pSession = (struct hdc_session *)session;
    struct hdc_recv_config recvConfig = {0};
    signed int wait = drv_hdc_get_wait_type((unsigned int)flag);

    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Parameter session magic error.(magic=%#x)\n", pSession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->sockfd == -1) {
        HDC_LOG_INFO("The session has been closed.\n");
        return DRV_ERROR_SOCKET_CLOSE;
    }

    if (msgLen == NULL) {
        HDC_LOG_ERR("Input parameter msgLen is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    recvConfig.wait = wait;
    recvConfig.timeout = 0;
    recvConfig.group_flag = 0;
    return drv_hdc_recv_msg_len(pSession, (unsigned int *)msgLen, &recvConfig);
}

hdcError_t drvHdcRecvBuf(HDC_SESSION session, char *pBuf, signed int bufLen, signed int *msgLen)
{
    struct hdc_session *pSession = (struct hdc_session *)session;
    struct hdc_recv_config recvConfig = {0};
    int ret;

    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Parameter session magic error.(magic=%#x)\n", pSession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->sockfd == -1) {
        HDC_LOG_INFO("The session has been closed.\n");
        return DRV_ERROR_SOCKET_CLOSE;
    }
    if (pBuf == NULL) {
        HDC_LOG_ERR("Input parameter pBuf is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (msgLen == NULL) {
        HDC_LOG_ERR("Input parameter msgLen is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    recvConfig.group_flag = 0;
    recvConfig.buf_count = 1;
    /* drvHdcRecvBuf is an external interface. The return value must be the same as the original one */
    ret = drv_hdc_recv_msg_body(pSession, pBuf, (unsigned int)bufLen, (unsigned int *)msgLen, &recvConfig);
    if (ret != DRV_ERROR_NONE) {
        ret = DRV_ERROR_RECV_MESG;
    }
    return ret;
}

/*****************************************************************************
 * Function Name         : drvHdcGetCapacity
 * Function Description  : Before the HDC sends messages, you need to know the size of the sent packet and
 *                        the channel type through this API.
 * Input Parameters      : void
 * Output Parameters     : struct drvHdcCapacity *capacity    get the packet size and channel type currently supported
 *                                                        by HDCget the packet size and channel type currently
 *                                                        supported by HDC
 * Return Value          : 0 for success, others for fail
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcGetCapacity(struct drvHdcCapacity *capacity)
{
    if (capacity == NULL) {
        HDC_LOG_ERR("Input parameter capacity is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    capacity->chanType = (enum drvHdcChanType)g_hdcConfig.trans_type;
    int chanType = (int)(capacity->chanType);
    if (chanType == (int)(HDC_TRANS_USE_PCIE)) {
        capacity->maxSegment = (unsigned int)g_hdcConfig.pcie_segment;
    } else if (chanType == (int)(HDC_TRANS_USE_SOCKET)) {
        capacity->maxSegment = (unsigned int)g_hdcConfig.socket_segment;
    } else {
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcSetSessionReference
 * Function Description  : Set session and process affinity
 * Input Parameters      : HDC_SESSION session        Specified session
 *
 * Output Parameters     : None
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 *****************************************************************************/
hdcError_t drvHdcSetSessionReference(HDC_SESSION session)
{
    struct hdc_session *pSession = (struct hdc_session *)session;
    mmProcess fd = HDC_SESSION_FD_INVALID;
    signed int ret;

    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Parameter session magic error.(magic=%#x)\n", pSession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
        if (pSession->bind_fd != HDC_SESSION_FD_INVALID) {
            HDC_LOG_ERR("Session has already been set reference. (session=%d)\n", pSession->sockfd);
            return DRV_ERROR_INVALID_VALUE;
        }
        fd = hdc_pcie_create_bind_fd();
        if (fd == (mmProcess)EN_ERROR) {
            HDC_LOG_ERR("Set reference open pcie device failed. (strerror=\"%s\")\n", strerror(errno));
            return DRV_ERROR_INVALID_HANDLE;
        }

        ret = hdc_pcie_set_session_owner(fd, pSession);
        if (ret != DRV_ERROR_NONE) {
            hdc_pcie_close_bind_fd(fd);
            fd = HDC_SESSION_FD_INVALID;
            HDC_LOG_ERR("Set session reference failed. (errno=%d; strerror=\"%s\")\n", ret, STRERROR(ret));
            return DRV_ERROR_IOCRL_FAIL;
        }

        pSession->bind_fd = fd;
    } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        ret = hdc_ub_set_session_owner(pSession);
        if (ret != DRV_ERROR_NONE) {
            HDC_LOG_ERR("Set session reference failed. (errno=%d; strerror=\"%s\")\n", ret, STRERROR(ret));
            return ret;
        }
    }

    HDC_LOG_INFO("Get pid number. (session=%d; pid=%d)\n", pSession->sockfd, getpid());
    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : drvHdcSessionClose
 * Function Description  : Closes the HDC session. Distinguish the disabling mode based on the flag.
 * Input Parameters      : HDC_SESSION session     Specify in which session to close
 *                        int type                hdc session close type set by the user
 * Output Parameters     : None
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t halHdcSessionCloseEx(HDC_SESSION session, int type)
{
    struct hdc_session *psession = NULL;

    if ((type < 0) || (type >= HDC_SESSION_CLOSE_FLAG_MAX)) {
        HDC_LOG_ERR("Input parameter flag is invalid.(max=%d; type=%d)\n", HDC_SESSION_CLOSE_FLAG_MAX, type);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (type == HDC_SESSION_CLOSE_FLAG_LOCAL) {
        if (!drv_hdc_is_support_session_close()) {
            return DRV_ERROR_NOT_SUPPORT;
        }
    }

    if (session == NULL) {
        HDC_LOG_ERR("Input parameter session is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    psession = (struct hdc_session *)session;
    if (psession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Parameter psession_magic error.(magic=%#x)\n", psession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (psession->type == HDC_SESSION_CLINET) {
        return drv_hdc_client_session_close(session, HDCDRV_CLOSE_TYPE_USER, type);
    } else {
        return drv_hdc_server_session_close(session, HDCDRV_CLOSE_TYPE_USER, type);
    }
}

/*****************************************************************************
 * Function Name         : drvHdcSessionClose
 * Function Description  : Close HDC Session for communication between Host and Device
 * Input Parameters      : HDC_SESSION session    Specify in which session to receive data
 * Output Parameters     : None
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t drvHdcSessionClose(HDC_SESSION session)
{
    return halHdcSessionCloseEx(session, HDC_SESSION_CLOSE_FLAG_NORMAL);
}

STATIC hdcError_t drv_hdc_get_session_dev_id(HDC_SESSION session, signed int *devid)
{
    struct hdc_client_session *p_client_session = NULL;
    struct hdc_server_session *p_serv_session = NULL;
    struct hdc_session *psession = NULL;

    if ((session == NULL) || (devid == NULL)) {
        HDC_LOG_ERR("Input parameter session or devid is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    psession = (struct hdc_session *)session;

    if (psession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Parameter psession_magic error.(magic=%#x)\n", psession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (psession->type == HDC_SESSION_CLINET) {
        p_client_session = (struct hdc_client_session *)session;
        *devid = p_client_session->devid;
    } else {
        p_serv_session = (struct hdc_server_session *)session;
        *devid = (signed int)p_serv_session->deviceId;
    }

    return DRV_ERROR_NONE;
}

STATIC hdcError_t drv_hdc_get_session_uid(HDC_SESSION session, signed int *root_privilege)
{
    struct hdc_session *pSession = (struct hdc_session *)session;
    signed int ret;

    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (root_privilege == NULL) {
        HDC_LOG_ERR("Input parameter root_privilege is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Parameter psession_magic error. (magic=%#x)\n", pSession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_SOCKET) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = hdc_pcie_get_session_uid(g_hdcConfig.pcie_handle, pSession, root_privilege);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Got session uid failed. (session=%d; errno=%d)\n", pSession->sockfd, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t drv_hdc_get_session_attr(HDC_SESSION session, signed int attr, signed int *value)
{
    struct hdc_session *pSession = (struct hdc_session *)session;
    signed int ret;

    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Parameter psession magic error. (magic=%#x)\n", pSession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (value == NULL) {
        HDC_LOG_ERR("Input parameter value is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_SOCKET) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        ret = hdc_ub_get_session_attr(g_hdcConfig.pcie_handle, pSession, attr, value);
    } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
        ret = hdc_pcie_get_session_attr(g_hdcConfig.pcie_handle, pSession, attr, value);
    } else {
        HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
        return DRV_ERROR_INVALID_HANDLE;
    }
    if (ret != DRV_ERROR_NONE) {
        if (ret == (-HDCDRV_SESSION_HAS_CLOSED)) {
            return DRV_ERROR_SOCKET_CLOSE;
        } else if (ret == (-HDCDRV_NO_PERMISSION)) {
            return DRV_ERROR_OPER_NOT_PERMITTED;
        } else if ((ret == (-HDCDRV_ERR)) || (ret == (-HDCDRV_PARA_ERR))) {
            return DRV_ERROR_INVALID_VALUE;
        }

        HDC_LOG_ERR("Got session attr failed. (session=%d; errno=%d; STRERROR=\"%s\")\n", pSession->sockfd, ret,
                    STRERROR(ret));
        return DRV_ERROR_INNER_ERR;
    }

    return DRV_ERROR_NONE;
}

drvError_t drv_hdc_get_peer_dev_id(signed int devId, signed int *peerDevId)
{
    if (peerDevId == NULL) {
        HDC_LOG_ERR("Input parameter peerDevId is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        return (drvError_t)hdc_ub_get_peer_devId(g_hdcConfig.pcie_handle, devId, peerDevId);
    } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
        return (drvError_t)hdc_pcie_get_peer_devid(g_hdcConfig.pcie_handle, devId, peerDevId);
    } else {
        HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
        return DRV_ERROR_INVALID_HANDLE;
    }
}

drvError_t halHdcGetSessionInfo(HDC_SESSION session, struct drvHdcSessionInfo *info)
{
    struct hdc_session *pSession = (struct hdc_session *)session;
    signed int ret;

    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (info == NULL) {
        HDC_LOG_ERR("Input parameter info is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Parameter psession_magic error.(magic=%#x)\n", pSession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = hdc_pcie_get_session_info(g_hdcConfig.pcie_handle, pSession, info);
    if (ret != DRV_ERROR_NONE) {
        if (ret == (-HDCDRV_NO_PERMISSION)) {
            return DRV_ERROR_OPER_NOT_PERMITTED;
        }
        HDC_LOG_ERR("Got session fid failed. (session=%d; errno=%d)\n", pSession->sockfd, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

STATIC signed int drv_hdc_send_check(const struct hdc_session *pSession, const struct drvHdcMsg *pMsg)
{
    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Input parameter magic is error. (pSession_magic=%#x)\n", pSession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->sockfd == -1) {
        HDC_LOG_INFO("The session has been closed.\n");
        return DRV_ERROR_SOCKET_CLOSE;
    }

    if (pMsg == NULL) {
        HDC_LOG_ERR("Input parameter pMsg is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pMsg->bufList[0].pBuf == NULL) {
        HDC_LOG_ERR("Input parameter pBuf is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pMsg->bufList[0].len <= 0) {
        HDC_LOG_ERR("Input parameter len is error. (pMsg_bufList0_len=%d)\n", pMsg->bufList[0].len);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : halHdcSend
 * Function Description  : Send data based on HDC Session
 * Input Parameters      : HDC_SESSION session        Specify in which session to send data
 *                        struct drvHdcMsg *pMsg     Descriptor pointer for sending messages. The maximum sending length
 *                        UINT64 flag                Reserved parameter, currently fixed 0
 *                        unsigned int timeout        Allow time for send timeout determined by user mode
 * Output Parameters     : None
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t halHdcSend(HDC_SESSION session, struct drvHdcMsg *pMsg, UINT64 flag, UINT32 timeout)
{
    struct hdc_session *pSession = (struct hdc_session *)session;
    signed int ret;
    int wait;
    signed int session_fd;

    ret = drv_hdc_send_check(pSession, pMsg);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    wait = drv_hdc_get_wait_type(flag);
    session_fd = pSession->sockfd;

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) {
        if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
            ret = hdc_ub_send(pSession, pMsg, wait, timeout);
        } else if (g_hdcConfig.h2d_type == HDC_TRANS_USE_PCIE) {
            ret = hdc_pcie_send(g_hdcConfig.pcie_handle, pSession, pMsg, wait, timeout);
        } else {
            HDC_LOG_ERR("Variable h2d_type is invalid. (h2d_type=%#x)\n", g_hdcConfig.h2d_type);
            return DRV_ERROR_INVALID_HANDLE;
        }
        if (ret != DRV_ERROR_NONE) {
            if (ret == (-HDCDRV_NO_BLOCK)) {
                return DRV_ERROR_NON_BLOCK;
            } else if (ret == (-HDCDRV_DMA_MEM_ALLOC_FAIL)) {
                return DRV_ERROR_MALLOC_FAIL;
            } else if (ret == (-HDCDRV_TX_TIMEOUT)) {
                return DRV_ERROR_WAIT_TIMEOUT;
            } else if ((ret == (-HDCDRV_TX_REMOTE_CLOSE)) ||
                       (ret == (-HDCDRV_SESSION_HAS_CLOSED) || (ret == (-HDCDRV_PEER_REBOOT)))) {
                HDC_LOG_WARN("HDC send not success. (errno=%d; STRERROR=\"%s\"; session=%d)\n", ret, STRERROR(ret),
                             session_fd);
                return DRV_ERROR_SOCKET_CLOSE;
            } else if (ret == (-HDCDRV_NO_PERMISSION)) {
                return DRV_ERROR_OPER_NOT_PERMITTED;
            } else if (ret == (-HDCDRV_DEVICE_NOT_READY)) {
                return DRV_ERROR_DEVICE_NOT_READY;
            } else if (ret == DRV_ERROR_TRANS_LINK_ABNORMAL) {
                return ret;
            }

            HDC_LOG_ERR("HDC send failed. (errno=%d; STRERROR=\"%s\"; session=%d)\n", ret, STRERROR(ret), session_fd);
            return DRV_ERROR_SEND_MESG;
        }
    } else {
        /* When sending, the pBuf pointed to by pMsg may be allocated by the caller,
        and the caller is responsible for ensuring its release */
        /* Therefore, do not change the free status here */
        /* only support offline */
        /* Currently only single buffer is supported, so it can be written like this */
        if ((ret = drv_hdc_socket_send(pSession->sockfd, pMsg->bufList[0].pBuf, pMsg->bufList[0].len)) !=
            DRV_ERROR_NONE) {
            return DRV_ERROR_SEND_MESG;
        }
    }
    return DRV_ERROR_NONE;
}

STATIC signed int drv_hdc_fast_send_check(const struct hdc_session *pSession, struct drvHdcFastSendMsg msg)
{
    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Input parameter magic is error.(magic=%#x)\n", pSession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->sockfd == -1) {
        HDC_LOG_INFO("The session has been closed.\n");
        return DRV_ERROR_SOCKET_CLOSE;
    }

    if (g_hdcConfig.trans_type != HDC_TRANS_USE_PCIE) {
        HDC_LOG_ERR("Socket mode not support.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        HDC_LOG_ERR("UB mode not support.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if ((msg.srcDataAddr == 0) && (msg.srcCtrlAddr == 0)) {
        HDC_LOG_ERR("Input parameter src addr is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((msg.dstDataAddr == 0) && (msg.dstCtrlAddr == 0)) {
        HDC_LOG_ERR("Input parameter dst addr is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((msg.dataLen == 0) && (msg.ctrlLen == 0)) {
        HDC_LOG_ERR("Input parameter Len is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (msg.dataLen > (unsigned int)INT_MAX || msg.ctrlLen > (unsigned int)INT_MAX) {
        HDC_LOG_ERR("Input parameter Len is too large. (dataLen=%u; ctrlLen=%u)\n", msg.dataLen, msg.ctrlLen);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

hdcError_t halHdcFastSend(HDC_SESSION session, struct drvHdcFastSendMsg msg, UINT64 flag, UINT32 timeout)
{
    struct hdc_session *pSession = (struct hdc_session *)session;
    signed int ret;
    signed int wait;
    signed int session_fd;

    ret = drv_hdc_fast_send_check(pSession, msg);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    wait = drv_hdc_get_wait_type(flag);
    session_fd = pSession->sockfd;

    if ((ret = hdc_pcie_fast_send(g_hdcConfig.pcie_handle, pSession, wait, &msg, timeout)) != DRV_ERROR_NONE) {
        if (ret == (-HDCDRV_TX_TIMEOUT)) {
            return DRV_ERROR_WAIT_TIMEOUT;
        } else if (ret == (-HDCDRV_NO_PERMISSION)) {
            return DRV_ERROR_OPER_NOT_PERMITTED;
        } else if (ret == (-HDCDRV_TX_QUE_FULL)) {
            HDC_LOG_WARN("HDC send not success mem release info no fetch. (errno=%d; STRERROR=\"%s\"; session=%d)\n",
                         ret, STRERROR(ret), session_fd);
            return DRV_ERROR_NO_RESOURCES;
        } else if ((ret == (-HDCDRV_TX_REMOTE_CLOSE)) || (ret == (-HDCDRV_SESSION_HAS_CLOSED)) ||
                   (ret == (-HDCDRV_PEER_REBOOT))) {
            HDC_LOG_WARN("HDC send not success. (errno=%d; STRERROR=\"%s\"; session=%d)\n", ret, STRERROR(ret),
                         session_fd);
            return DRV_ERROR_SOCKET_CLOSE;
        } else if (ret == (-HDCDRV_DEVICE_NOT_READY)) {
            return DRV_ERROR_DEVICE_NOT_READY;
        }

        HDC_LOG_ERR("Fast send failed. (errno=%d; STRERROR=\"%s\"; session=%d)\n", ret, STRERROR(ret), session_fd);
        return DRV_ERROR_SEND_MESG;
    }
    return DRV_ERROR_NONE;
}

/*****************************************************************************
 * Function Name         : halHdcRecvEx
 * Function Description  : Receive data based on HDC Session
 * Input Parameters      : HDC_SESSION session    Specify in which session to receive data
 *                        signed int bufLen      The length of each receive buffer in bytes
 *                        signed int flag        Fixed 0
 *                        userConfig             Record the parameters set by the user
 * Output Parameters     : struct drvHdcMsg *pMsg  Descriptor pointer for receiving messages
 *                        signed int *recvBufCount  The number of buffers that actually received the data
 * Return Value          : DRV_ERROR_NONE
 *                        DRV_ERROR_INVALID_VALUE
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
hdcError_t halHdcRecvEx(HDC_SESSION session, struct drvHdcMsg *pMsg, signed int bufLen, signed int *recvBufCount,
                        struct drvHdcRecvConfig *userConfig)
{
    struct hdc_session *pSession = (struct hdc_session *)session;
    struct hdc_msg_head *p_hdc_msg_head = NULL;
    struct hdc_recv_config p_recv_config = {0};
    hdcError_t ret = DRV_ERROR_INVALID_VALUE;
    unsigned int msg_len = 0;
    char *pBuf = NULL;

    (void)bufLen;

    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return ret;
    }

    if (pSession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Parameter pSession_magic is error.(magic=%#x)\n", pSession->magic);
        return ret;
    }

    if (pSession->sockfd == -1) {
        HDC_LOG_INFO("The session has been closed.\n");
        return DRV_ERROR_SOCKET_CLOSE;
    }

    if (pMsg == NULL) {
        HDC_LOG_ERR("Input parameter pMsg is NULL.\n");
        return ret;
    }

    if (recvBufCount == NULL) {
        HDC_LOG_ERR("Input parameter recvBufCount is NULL.\n");
        return ret;
    }

    if (userConfig == NULL) {
        HDC_LOG_ERR("Input parameter userConfig is NULL.\n");
        return ret;
    }

    /* The code is written this way because only single buffer is currently supported */
    if (pMsg->bufList[0].pBuf != NULL) {
        HDC_LOG_ERR("Input parameter pBuf is unexpected.\n");
        return ret;
    }

    p_recv_config.buf_count = 0;
    p_recv_config.timeout = userConfig->timeout;
    p_recv_config.group_flag = userConfig->group_flag;
    p_recv_config.wait = drv_hdc_get_wait_type(userConfig->wait_flag);
    /* 0 indicates non-aggregated reception, and buf_count indicates the number of receptions in one time */
    ret = drv_hdc_add_msg_body(pSession, pMsg, &pBuf, &msg_len, &p_recv_config);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    /* change free from false to true */
    p_hdc_msg_head = container_of(pMsg, struct hdc_msg_head, msg); // lint !e102 !e14 !e42 !e564
    p_hdc_msg_head->freeBuf = true;                                // lint !e413

    ret = drv_hdc_recv_msg_body(pSession, pBuf, msg_len, (unsigned int *)&pMsg->bufList[0].len, &p_recv_config);
    if (ret != DRV_ERROR_NONE) {
        (void)drvHdcReuseMsg(pMsg);
        /* No error log is generated when the module is destructed (DRV_ERROR_INVALID_HANDLE) */
        if (ret != DRV_ERROR_INVALID_HANDLE) {
            HDC_LOG_ERR("Call drv_hdc_recv_msg_body failed. (err=%d; sock=%d)\n", ret, pSession->sockfd);
            return ret;
        }
        return DRV_ERROR_RECV_MESG;
    }

    *recvBufCount = p_recv_config.buf_count;
    return ret;
}

hdcError_t halHdcRecv(HDC_SESSION session, struct drvHdcMsg *pMsg, signed int bufLen, UINT64 flag,
                      signed int *recvBufCount, UINT32 timeout)
{
    struct drvHdcRecvConfig userConfig;

    userConfig.wait_flag = flag;
    userConfig.group_flag = 0;
    userConfig.timeout = timeout;
    return halHdcRecvEx(session, pMsg, bufLen, recvBufCount, &userConfig);
}

hdcError_t halHdcFastRecv(HDC_SESSION session, struct drvHdcFastRecvMsg *msg, UINT64 flag, UINT32 timeout)
{ // lint !e564
    struct hdc_session *pSession = (struct hdc_session *)session;
    signed int session_fd;
    signed int ret;
    signed int wait;

    if (pSession == NULL) {
        HDC_LOG_ERR("Input parameter pSession is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->magic != HDC_MAGIC_WORD) {
        HDC_LOG_ERR("Parameter pSession_magic is error.(magic=%#x)\n", pSession->magic);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pSession->sockfd == -1) {
        HDC_LOG_INFO("The session has been closed.\n");
        return DRV_ERROR_SOCKET_CLOSE;
    }

    if (g_hdcConfig.trans_type != HDC_TRANS_USE_PCIE) {
        HDC_LOG_ERR("Socket mode not support.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB) {
        HDC_LOG_ERR("UB mode not support.\n");
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (msg == NULL) {
        HDC_LOG_ERR("Input parameter msg is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    wait = drv_hdc_get_wait_type((unsigned int)flag);
    session_fd = pSession->sockfd;

    if ((ret = hdc_pcie_fast_recv(g_hdcConfig.pcie_handle, pSession, wait, msg, timeout)) != DRV_ERROR_NONE) {
        if (ret == (-HDCDRV_RX_TIMEOUT)) {
            return DRV_ERROR_WAIT_TIMEOUT;
        } else if (ret == (-HDCDRV_NO_PERMISSION)) {
            return DRV_ERROR_OPER_NOT_PERMITTED;
        } else if ((ret == (-HDCDRV_SESSION_HAS_CLOSED)) || (ret == (-HDCDRV_PEER_REBOOT))) {
            HDC_LOG_WARN("The session was closed. (errno=%d; STRERROR=\"%s\"; session=%d)\n", ret, STRERROR(ret),
                         session_fd);
            return DRV_ERROR_SOCKET_CLOSE;
        } else if (ret == (-HDCDRV_DEVICE_NOT_READY)) {
            return DRV_ERROR_DEVICE_NOT_READY;
        }

        HDC_LOG_ERR("Fast receive failed. (errno=%d; STRERROR=\"%s\"; session=%d)\n", ret, STRERROR(ret), session_fd);
        return DRV_ERROR_RECV_MESG;
    }

    if ((msg->dataLen == 0) && (msg->ctrlLen == 0)) {
        /* session has closed, can't access pSession anymore */
        HDC_LOG_INFO("The session local or remote was closed. (session=%d)\n", session_fd);
        return DRV_ERROR_SOCKET_CLOSE;
    }
    return DRV_ERROR_NONE;
}

STATIC hdcError_t drv_hdc_gest_socket_session_status(HDC_SESSION session, int *status)
{
    struct hdc_session *pSession = (struct hdc_session *)session;
    drvError_t ret = DRV_ERROR_NONE;
    int recv_bytes;
    signed int session_fd;
    unsigned int recvlen;
    struct timeval timeo;
    signed int err;

    timeo.tv_sec = 0;
    timeo.tv_usec = 500;

    session_fd = pSession->sockfd;
    /* set timeout */
    if (setsockopt(session_fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeo, sizeof(timeo)) < 0) {
        HDC_LOG_ERR("Set socket timeout failed. (errno=%d; strerror=\"%s\"; session_fd=%d; flag=%d)\n",
                    mm_get_error_code(), strerror(mm_get_error_code()), session_fd, *status);
        return DRV_ERROR_SOCKET_SET;
    }

    recv_bytes = (int)recv(session_fd, (char *)&recvlen, sizeof(signed int), MSG_PEEK);
    err = mm_get_error_code();
    if (recv_bytes > 0 || ((recv_bytes == -1) && (err == EAGAIN || err == EINTR))) {
        *status = HDC_SESSION_STATUS_CONNECT;
    } else if (recv_bytes == -1 && err == EBADF) {
        ret = DRV_ERROR_PARA_ERROR;
    } else if (recv_bytes == 0) {
        *status = HDC_SESSION_STATUS_CLOSE;
    } else {
        *status = HDC_SESSION_STATUS_UNKNOW_ERR;
    }

    /* clean timeout, wait forever */
    timeo.tv_sec = 0;
    timeo.tv_usec = 0;
    if (setsockopt(session_fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeo, sizeof(timeo)) < 0) {
        HDC_LOG_ERR("Clean socket timeout failed. (errno=%d; strerror=\"%s\"; session_fd=%d; flag=%d)\n",
                    mm_get_error_code(), strerror(mm_get_error_code()), session_fd, *status);
    }

    return ret;
}

STATIC hdcError_t drv_hdc_get_pcie_session_status(HDC_SESSION session, int *status)
{
    int value;
    int ret;

    ret = drv_hdc_get_session_attr(session, HDCDRV_SESSION_ATTR_STATUS, &value);
    if (ret == DRV_ERROR_NONE) {
        *status = (value == HDCDRV_OK) ? HDC_SESSION_STATUS_CONNECT : HDC_SESSION_STATUS_CLOSE;
    }

    return ret;
}

STATIC drvError_t drv_hdc_get_session_status(HDC_SESSION session, int *status)
{
    if (session == NULL || status == NULL) {
        HDC_LOG_ERR("Input para invalid, session or status is NULL\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (g_hdcConfig.trans_type == HDC_TRANS_USE_SOCKET) {
        return drv_hdc_gest_socket_session_status(session, status);
    } else {
        return drv_hdc_get_pcie_session_status(session, status);
    }
}

drvError_t halHdcGetSessionAttr(HDC_SESSION session, int attr, int *value)
{
    drvError_t ret;
    switch (attr) {
        case HDC_SESSION_ATTR_DEV_ID:
            ret = drv_hdc_get_session_dev_id(session, value);
            break;
        case HDC_SESSION_ATTR_UID:
            ret = drv_hdc_get_session_uid(session, value);
            break;
        case HDC_SESSION_ATTR_RUN_ENV:
            ret = drv_hdc_get_session_attr(session, HDCDRV_SESSION_ATTR_RUN_ENV, value);
            break;
        case HDC_SESSION_ATTR_VFID:
            ret = drv_hdc_get_session_attr(session, HDCDRV_SESSION_ATTR_VFID, value);
            break;
        case HDC_SESSION_ATTR_LOCAL_CREATE_PID:
            ret = drv_hdc_get_session_attr(session, HDCDRV_SESSION_ATTR_LOCAL_CREATE_PID, value);
            break;
        case HDC_SESSION_ATTR_PEER_CREATE_PID:
            ret = drv_hdc_get_session_attr(session, HDCDRV_SESSION_ATTR_PEER_CREATE_PID, value);
            break;
        case HDC_SESSION_ATTR_STATUS:
            ret = drv_hdc_get_session_status(session, value);
            break;
        case HDC_SESSION_ATTR_DFX:
            ret = drv_hdc_get_session_attr(session, HDCDRV_SESSION_ATTR_DFX, value);
            break;
        default:
            HDC_LOG_WARN("Input parameter is not support.\n");
            ret = DRV_ERROR_NOT_SUPPORT;
            break;
    }
    return ret;
}

hdcError_t halHdcGetServerAttr(HDC_SERVER server, int attr, int *value)
{
    switch (attr) {
        case HDC_SERVER_ATTR_DEV_ID:
            return drv_hdc_get_server_dev_id(server, value);
        default:
            HDC_LOG_ERR("Input parameter attr is invalid.\n");
            return DRV_ERROR_INVALID_VALUE;
    }
}

/*****************************************************************************
 * Prototype    : halHdcNotifyRegister
 * Description  : For UB, this interface is used by the service to register the notify callback function in user mode.
 * Input        : int service_type, struct HdcSessionNotify *notify
 * Output       : NULL
 * Return Value : hdcError_t, [DRV_ERROR_NONE, DRV_ERROR_INVALID_HANDLE]
 *
 *  History        :
 *  1.Date         : 2024/12/25
 *  Modification : Created function
 *
 *****************************************************************************/
hdcError_t halHdcNotifyRegister(int service_type, struct HdcSessionNotify *notify)
{
    if ((g_hdcConfig.trans_type == HDC_TRANS_USE_SOCKET) || (g_hdcConfig.h2d_type != HDC_TRANS_USE_UB)) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return hdc_ub_notify_register(service_type, notify);
}

/*****************************************************************************
 * Prototype    : halHdcNotifyUnregister
 * Description  : For UB, this interface is used by the service to unregister the notify callback function in user mode.
 * Input        : int service_type
 * Output       : NULL
 * Return Value : NULL
 *
 *  History        :
 *  1.Date         : 2024/12/25
 *  Modification : Created function
 *
 *****************************************************************************/
void halHdcNotifyUnregister(int service_type)
{
    if ((g_hdcConfig.trans_type == HDC_TRANS_USE_SOCKET) || (g_hdcConfig.h2d_type != HDC_TRANS_USE_UB)) {
        return;
    }

    hdc_ub_notify_unregister(service_type);
}

signed int __attribute__((constructor)) drv_hdc_init(void) // lint !e527
{
    int ret;
    (void)mmMutexInit(&g_mem_fd_mng.mutex);
    drv_hdc_trans_type_mutex_init();

    ret = (signed int)hdc_init(&g_hdcConfig);
#ifdef CFG_FEATURE_SUPPORT_UB
    if ((ret == 0) && (g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) && (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB)) {
        (void)hdc_ub_init(&g_hdcConfig);
        hdc_max_device_num = HDC_MAX_UB_DEV_CNT;
    }
#endif
    return ret;
}

/*****************************************************************************
 * Function Name         : drv_hdc_exit
 * Function Description  : Cancel HDC driver module initialization
 * Input Parameters      : void
 * Output Parameters     : None
 * Return Value          :
 * Caller Function       :
 * Called Function       :
 *
 * Modification History  :
 * 1.Date                : January 15, 2018
 *  Modification         : New generated function
 *
 *****************************************************************************/
STATIC void __attribute__((destructor)) drv_hdc_exit(void) // lint !e527
{
#ifdef CFG_FEATURE_SUPPORT_UB
    if ((g_hdcConfig.trans_type == HDC_TRANS_USE_PCIE) && (g_hdcConfig.h2d_type == HDC_TRANS_USE_UB)) {
        (void)hdc_ub_uninit(&g_hdcConfig);
    }
#endif
    /* Some services (e.g. log) need to invoke the HDC interface in the destructor process.
        So the handle is no longer closed in the hdc destructor.
        The handle is reclaimed by the OS. */
    HDC_SHARE_LOG_DESTROY();
}
