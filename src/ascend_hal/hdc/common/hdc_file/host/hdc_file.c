/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <sys/mman.h>
#include <sys/stat.h>
#include "hdc_cmn.h"
#include "hdcdrv_cmd_ioctl.h"
#include "hdc_file_common.h"
#include "hdc_core.h"

void hdc_create_dir_info_output(void)
{
    return;
}

void *drv_hdc_mmap(mmProcess fd, void *addr, unsigned int alloc_len, unsigned int flag)
{
    (void)flag;
    unsigned int map_flags = MAP_SHARED;
    mmProcess mmap_fd = fd;
    return (void *)mmap(addr, alloc_len, PROT_READ | PROT_WRITE, (signed int)map_flags, mmap_fd, 0);
}

signed int drv_hdc_alloc_len_check(enum drvHdcMemType mem_type, unsigned int len, unsigned int flag)
{
    unsigned int alloc_len;
    alloc_len = hdc_get_alloc_len(len, flag);
    if (((mem_type == HDC_MEM_TYPE_TX_CTRL) || (mem_type == HDC_MEM_TYPE_RX_CTRL)) &&
        (alloc_len > HDCDRV_CTRL_MEM_MAX_LEN)) {
        HDC_LOG_ERR("alloc_len is not support for ctrl. (alloc_len=%d)\n", alloc_len);
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

void drv_hdc_set_malloc_flag(unsigned int *flag)
{
    *flag = 0;
}

bool drv_hdc_is_support_session_close(void)
{
    bool res = hdc_is_in_ub();
    if (!res) {
        HDC_LOG_WARN("local close only support in UB.\n");
    }
    return res;
}

signed int get_local_trusted_base_path(signed int user_mode, char *path, signed int dev_id)
{
    (void)user_mode;
    (void)dev_id;
    signed int ret;
    ret = sprintf_s(path, HDC_NAME_MAX, "%s", HDC_HOST_BASE_PATH_NAME);
    return ret;
}

signed int get_peer_trusted_base_path(signed int user_mode, char *path, signed int peer_devid)
{
    signed int ret;
    if (user_mode == HDC_FILE_TRANS_MODE_UPGRADE) {
        ret = sprintf_s(path, HDC_NAME_MAX, "%s%d", HDC_SLAVE_DM_PATH_NAME, peer_devid);
    } else if (user_mode == HDC_FILE_TRANS_MODE_CANN) {
        ret = sprintf_s(path, HDC_NAME_MAX, "%s%d%s", HDC_SLAVE_BASE_PATH_NAME, peer_devid, HDC_SLAVE_CANN_FOLDER_NAME);
    } else {
        ret = sprintf_s(path, HDC_NAME_MAX, "%s%d", HDC_SLAVE_BASE_PATH_NAME, peer_devid);
    }
    return ret;
}

hdcError_t drv_hdc_dst_path_depth_check(const char *path)
{
    (void)path;
    return DRV_ERROR_NONE;
}

#ifndef CFG_SOC_PLATFORM_RC
STATIC hdcError_t validate_free_inode(void);
hdcError_t drv_hdc_dst_path_right_check(const char *dst_path, const char *base_path, int root_privilege)
{
    (void)dst_path;
    (void)base_path;
    (void)root_privilege;
    (void)validate_free_inode();
    return DRV_ERROR_NONE;
}

hdcError_t hdc_get_session_attr_check(HDC_SESSION session, int *devid)
{
    (void)session;
    (void)devid;
    return DRV_ERROR_NONE;
}

int hdc_directory_path_compose(int devid, char *base_path, char *dm_base_path, char *cann_base_path)
{
    (void)devid;
    (void)dm_base_path;
    (void)cann_base_path;
    int s_ret;
    s_ret = sprintf_s(base_path, HDC_NAME_MAX, "%s", HDC_HOST_BASE_PATH_NAME);
    return s_ret;
}

STATIC int hdc_total_file_size(const char *fpath, const struct stat *sb, int typeflag)
{
    (void)fpath;
    (void)sb;
    (void)typeflag;
    return 0;
}

STATIC hdcError_t validate_free_space(void)
{
    if (hdc_total_file_size(NULL, NULL, 0) != 0) {
        return DRV_ERROR_INVALID_VALUE;
    }
    return DRV_ERROR_NONE;
}

STATIC int calc_file_count(const char *path, int depth)
{
    (void)path;
    (void)depth;
    return 0;
}

STATIC hdcError_t validate_free_inode(void)
{
    if (calc_file_count(NULL, 0) == 0) {
        return DRV_ERROR_NONE;
    }
    return validate_free_space();
}
#endif

hdcError_t validate_resource(struct filesock *fs)
{
    (void)fs;
    return DRV_ERROR_NONE;
}