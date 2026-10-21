/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>
#include <assert.h>

#include "ascend_hal.h"
#include "securec.h"
#include "pbl_ubmm.h"

#include "svm_user_adapt.h"
#include "svm_log.h"
#include "svm_sys_cmd.h"
#include "svm_dbi.h"
#include "svm_pagesize.h"
#include "svm_share_align.h"
#include "malloc_mng.h"
#include "va_allocator.h"
#include "svm_pipeline.h"
#include "casm.h"
#include "casm_cs.h"
#include "svm_share_type.h"
#include "svm_ub_mem.h"
#include "svm_ipc.h"

/* todo */
#define SVM_IPC_WLIST_MAX_SET_NUM_ONCE 31

#define SVM_IPC_NAME_VERSION_00 0
#define SVM_MAX_IPC_NAME_SIZE 65

/* ipc name len is 64 bytes, ipc info 40 bytes, it is not enough space when converting to a string.
   Therefore, we directly store numbers in the string space.
       When considering the value of a single byte, the end character of a string is marked by a zero value.
       Therefore, 0 must be replaced with a non-zero value, and a bit is used to indicate whether a byte
       has been replaced. If eight consecutive bytes are not replaced, the byte formed by the bits would be zero.
       To prevent this, the highest bit is set to 1 for every eight bits, and only seven bits are used to represent
       the seven bytes in the data. */

struct svm_ipc_info {
    u8 ver;
    u8 cs_valid;
    u16 rsv;
    u16 server_id;
    u16 udevid;
    int owner_pid;
    int tgid;
    u64 va;
    u64 size;
    u64 key;
};

#define IPC_INFO_LEN sizeof(struct svm_ipc_info)
#define IPC_BTYE_NUM_PER_REPLACE_BITMAP_BYTE 7
#define IPC_REPLACE_BITMAP_LEN \
    (IPC_INFO_LEN / IPC_BTYE_NUM_PER_REPLACE_BITMAP_BYTE + IPC_INFO_LEN % IPC_BTYE_NUM_PER_REPLACE_BITMAP_BYTE)
#define IPC_FORMAT_TOTAL_LEN (IPC_INFO_LEN + IPC_REPLACE_BITMAP_LEN)
static_assert(IPC_FORMAT_TOTAL_LEN < SVM_MAX_IPC_NAME_SIZE, "ipc format len is too bigger");

struct svm_ipc_opened_va_priv {
    struct svm_share_priv_head head;
    struct svm_global_va src_info;
};

static int svm_ipc_opened_va_release(void *priv, bool force)
{
    SVM_UNUSED(force);
    svm_ua_free(priv);
    return 0;
}

static bool svm_ipc_opened_va_should_criu_reset(void *priv)
{
    SVM_UNUSED(priv);
    return true;
}

static struct svm_priv_ops ipc_opened_va_priv_ops = {
    .release = svm_ipc_opened_va_release,
    .get_prop = NULL,
    .show = NULL,
    .should_criu_reset = svm_ipc_opened_va_should_criu_reset,
};

static int svm_ipc_set_opened_src_info(u64 opened_va, struct svm_global_va *src_info)
{
    struct svm_ipc_opened_va_priv *priv;
    void *handle;
    int ret;

    priv = (struct svm_ipc_opened_va_priv *)svm_ua_calloc(1, sizeof(*priv));
    if (priv == NULL) {
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    handle = svm_handle_get(opened_va);
    if (handle == NULL) {
        svm_ua_free(priv);
        return DRV_ERROR_NOT_EXIST;
    }

    priv->head.type = SVM_SHARE_TYPE_IPC_OPENED;
    priv->src_info = *src_info;
    ret = svm_set_priv(handle, priv, &ipc_opened_va_priv_ops);
    svm_handle_put(handle);
    if (ret != 0) {
        svm_ua_free(priv);
    }

    return ret;
}

static int _svm_ipc_query_src_info(u64 opened_start, struct svm_global_va *src_info)
{
    struct svm_ipc_opened_va_priv *priv;
    void *handle = NULL;

    handle = svm_handle_get(opened_start);
    if (handle == NULL) {
        return DRV_ERROR_NOT_EXIST;
    }

    priv = (struct svm_ipc_opened_va_priv *)svm_get_priv(handle);
    if ((priv == NULL) || (priv->head.type != SVM_SHARE_TYPE_IPC_OPENED)) {
        svm_handle_put(handle);
        return DRV_ERROR_NOT_EXIST;
    }

    *src_info = priv->src_info;
    svm_handle_put(handle);

    return DRV_ERROR_NONE;
}

int svm_ipc_query_src_info(u64 opened_va, u64 size, struct svm_global_va *src_info)
{
    struct svm_prop prop;
    u64 offset;
    int ret;

    ret = svm_get_prop(opened_va, &prop);
    if (ret != 0) {
        return DRV_ERROR_NOT_EXIST;
    }

    if (!svm_flag_cap_is_support_ipc_close(prop.flag)) {
        return DRV_ERROR_NOT_EXIST;
    }

    ret = _svm_ipc_query_src_info(prop.start, src_info);
    if (ret != 0) {
        return DRV_ERROR_NOT_EXIST;
    }

    offset = opened_va - prop.start;
    if ((offset >= src_info->size) || (size > (src_info->size - offset))) {
        svm_err("Out of range. (va=0x%llx; size=0x%llx; start=0x%llx; opened_size=0x%llx)\n", opened_va, size,
                prop.start, src_info->size);
        return DRV_ERROR_INVALID_VALUE;
    }

    src_info->va += offset;
    src_info->size = size;
    return DRV_ERROR_NONE;
}

static void svm_ipc_info_byte_to_replace_bitmap(u32 info_byte, u32 *bitmap_byte, u32 *bitmap_bit)
{
    *bitmap_byte = info_byte / IPC_BTYE_NUM_PER_REPLACE_BITMAP_BYTE;
    *bitmap_bit = info_byte % IPC_BTYE_NUM_PER_REPLACE_BITMAP_BYTE;
}

static void svm_ipc_replace_bitmap_to_info_byte(u32 bitmap_byte, u32 bitmap_bit, u32 *info_byte)
{
    *info_byte = bitmap_byte * IPC_BTYE_NUM_PER_REPLACE_BITMAP_BYTE + bitmap_bit;
}

static int svm_ipc_parse_name(const char *name, u8 *version, u64 *key, int *cs_valid, struct svm_global_va *src_va,
                              int *owner_pid)
{
    struct svm_ipc_info ipc_info;
    const u8 *replace_bitmap = (const u8 *)(const void *)(name + IPC_INFO_LEN);
    u8 *ipc_info_byte = (u8 *)(void *)&ipc_info;
    u32 i, j;

    if (strnlen(name, SVM_MAX_IPC_NAME_SIZE) < IPC_FORMAT_TOTAL_LEN) {
        return DRV_ERROR_INVALID_VALUE;
    }

    ipc_info = *(const struct svm_ipc_info *)(const void *)name;
    for (i = 0; i < IPC_REPLACE_BITMAP_LEN; i++) {
        for (j = 0; j < IPC_BTYE_NUM_PER_REPLACE_BITMAP_BYTE; j++) {
            if ((replace_bitmap[i] & (0x1 << j)) != 0) {
                u32 info_byte;
                svm_ipc_replace_bitmap_to_info_byte(i, j, &info_byte);
                if (info_byte >= IPC_INFO_LEN) {
                    svm_err("Invalid name. (name=%s; info_byte=%u).\n", name, info_byte);
                    return DRV_ERROR_INVALID_VALUE;
                }
                ipc_info_byte[info_byte] = 0;
            }
        }
    }

    *version = ipc_info.ver;
    *cs_valid = (int)ipc_info.cs_valid;
    *key = ipc_info.key;
    *owner_pid = ipc_info.owner_pid;
    src_va->server_id = (u32)ipc_info.server_id;
    src_va->udevid = (u32)ipc_info.udevid;
    src_va->tgid = ipc_info.tgid;
    src_va->va = ipc_info.va;
    src_va->size = ipc_info.size;

    return DRV_ERROR_NONE;
}

static int svm_ipc_get_key_by_name(const char *name, u64 *key)
{
    struct svm_global_va src_va;
    int cs_valid, owner_pid;
    u8 version;

    return svm_ipc_parse_name(name, &version, key, &cs_valid, &src_va, &owner_pid);
}

static int svm_ipc_format_name(char *name, u32 name_len, u64 key)
{
    struct svm_ipc_info *ipc_info = (struct svm_ipc_info *)(void *)name;
    u8 *replace_bitmap = (u8 *)(void *)(ipc_info + 1);
    u8 *ipc_info_byte = (u8 *)(void *)ipc_info;
    u8 version = SVM_IPC_NAME_VERSION_00;
    struct svm_global_va src_va = {0};
    struct svm_global_va tmp_src_va;
    int owner_pid = 0;
    int tmp_owner_pid;
    int cs_valid = 0;
    int ret;
    u32 i;

    ret = svm_casm_cs_query_src_info(key, &tmp_src_va, &tmp_owner_pid);
    if (ret == 0) {
        if (tmp_src_va.server_id != SVM_INVALID_SERVER_ID) {
            cs_valid = 1;
            src_va = tmp_src_va;
            owner_pid = tmp_owner_pid;
        }
    }

    if (name_len <= (IPC_FORMAT_TOTAL_LEN + 1)) { /* '\0' + 1 */
        svm_err("Not enough space. (name_len=%u; need=%u)\n", name_len, IPC_FORMAT_TOTAL_LEN);
        return DRV_ERROR_INVALID_VALUE;
    }

    ipc_info->ver = version;
    ipc_info->cs_valid = (u8)cs_valid;
    ipc_info->rsv = 0;
    ipc_info->key = key;
    ipc_info->owner_pid = owner_pid;
    ipc_info->server_id = (u16)src_va.server_id;
    ipc_info->udevid = (u16)src_va.udevid;
    ipc_info->tgid = src_va.tgid;
    ipc_info->va = src_va.va;
    ipc_info->size = src_va.size;

    for (i = 0; i < IPC_REPLACE_BITMAP_LEN; i++) {
        replace_bitmap[i] = 0x1 << IPC_BTYE_NUM_PER_REPLACE_BITMAP_BYTE;
    }

    name[IPC_FORMAT_TOTAL_LEN] = '\0';

    for (i = 0; i < IPC_INFO_LEN; i++) {
        if (ipc_info_byte[i] == 0) {
            u32 bitmap_byte, bitmap_bit;
            svm_ipc_info_byte_to_replace_bitmap(i, &bitmap_byte, &bitmap_bit);
            replace_bitmap[bitmap_byte] |= (u8)(0x1 << bitmap_bit);
            ipc_info_byte[i] = 1;
        }
    }

    return DRV_ERROR_NONE;
}

static int svm_ipc_create_handle(u64 va, u64 size, u64 *key)
{
    struct casm_src_prop src_prop;
    struct svm_dst_va dst_va;
    struct svm_prop prop;
    u64 aligned_size;
    int ret;

    ret = svm_get_prop(va, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (va=0x%llx)\n", va);
        return ret;
    }

    if (!svm_flag_cap_is_support_ipc_create(prop.flag)) {
        svm_err("The address does not support IPC sharing. (va=0x%llx)\n", va);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = svm_share_get_src_aligned_size(prop.devid, prop.flag, va, size, &aligned_size);
    if (ret != 0) {
        svm_err("Get aligned size failed. (devid=%u; va=0x%llx; size=0x%llx)\n", prop.devid, va, size);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((va + size) > (prop.start + prop.size)) {
        svm_err("va or size is invalid. (va=0x%llx; align_size=0x%llx; prop start=0x%llx; size=0x%llx)\n", va,
                aligned_size, prop.start, prop.size);
        return DRV_ERROR_PARA_ERROR;
    }

    src_prop.casm_pg_type = svm_flag_attr_is_gpage(prop.flag) ?
                                CASM_GPAGE :
                                (svm_flag_attr_is_hpage(prop.flag) ? CASM_HPAGE : CASM_NPAGE);
    svm_dst_va_pack(prop.devid, DEVDRV_PROCESS_CP1, va, aligned_size, &dst_va);
    ret = svm_casm_create_key(&dst_va, &src_prop, key);
    if (ret != 0) {
        /* The log cannot be modified, because in the failure mode library. */
        svm_err("Create key failed. (va=0x%llx)\n", va);
        return ret;
    }

    return 0;
}

static int svm_ipc_destroy_handle(u64 key)
{
    return svm_casm_destroy_key(key);
}

static int svm_ipc_malloc_opened_va(u32 devid, u64 size, u64 align, enum casm_pg_type casm_src_pg_type, u64 *opened_va)
{
    struct svm_malloc_location location;
    u64 hpage_size;
    u64 start = 0;
    u64 svm_flag = 0;
    int ret;

    svm_flag |= SVM_FLAG_CAP_IPC_CLOSE;
    svm_flag |= SVM_FLAG_CAP_MEMSET;
    svm_flag |= SVM_FLAG_CAP_SYNC_COPY;
    svm_flag |= SVM_FLAG_CAP_DMA_DESC_CONVERT;
    svm_flag |= SVM_FLAG_CAP_GET_MEM_TOKEN_INFO;
    svm_flag |= SVM_FLAG_ATTR_VA_ONLY;
    svm_flag |= SVM_FLAG_BY_PASS_CACHE;
    svm_flag |= SVM_FLAG_CAP_GET_ATTR;
    svm_flag |= SVM_FLAG_CAP_GET_D2D_TRANS_WAY;

    /* Use HPAGE when cross-server IPC cannot provide the source page type, then record the smaller granularity. */
    if ((casm_src_pg_type == CASM_PG_TYPE_INVALID) || (casm_src_pg_type == CASM_HPAGE) ||
        (casm_src_pg_type == CASM_GPAGE)) {
        ret = svm_dbi_query_hpage_size(devid, &hpage_size);
        if (ret != 0) {
            return ret;
        }

        if (align >= hpage_size) {
            svm_flag |= ((casm_src_pg_type == CASM_GPAGE) && (align > hpage_size)) ? SVM_FLAG_ATTR_PA_GPAGE :
                                                                                     SVM_FLAG_ATTR_PA_HPAGE;
        }
    }

    svm_flag_set_module_id(&svm_flag, HCCL_HAL_MODULE_ID);

    svm_malloc_location_pack(devid, SVM_MALLOC_NUMA_NO_NODE, &location);

    ret = svm_malloc(&start, size, align, svm_flag, &location);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    *opened_va = start;
    return 0;
}

static int svm_ipc_free_opened_va(u64 opened_va)
{
    int ret;

    ret = svm_free(opened_va);
    return (ret == DRV_ERROR_CLIENT_BUSY) ? DRV_ERROR_BUSY : ret;
}

static u32 svm_ipc_open_flag_to_casm_flag(u64 ipc_open_flag)
{
    u32 map_route = (u32)ipc_open_flag;
    u32 casm_flag = 0;

    casm_flag |= (map_route == MEM_MAP_UB_ONE_PORT_PATH) ? SVM_CASM_FLAG_MAP_UB_ONE_PORT_PATH : 0;
    casm_flag |= (map_route == MEM_MAP_UB_MULTI_PORT_PATH) ? SVM_CASM_FLAG_MAP_UB_MULTI_PORT_PATH : 0;

    return casm_flag;
}

static int svm_ipc_open_handle(u32 devid, u64 key, u64 *opened_va, uint64_t flag)
{
    struct casm_src_va_ex_info src_ex_info;
    struct svm_global_va src_va;
    u64 align;
    int ret;

    ret = svm_casm_get_src_va_ex(devid, key, &src_va, &src_ex_info);
    if (ret != 0) {
        svm_err("Get src va failed. (devid=%u; key=0x%llx)\n", devid, key);
        return ret;
    }

    if (src_ex_info.ex_info != 0) {
        *opened_va = src_ex_info.ex_info;
        return 0;
    }

    ret = svm_share_get_dst_align(src_va.va, src_va.size, devid, &align);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get dst align failed. (ret=%d; src_va=0x%llx; size=%llu; devid=%u)\n", ret, src_va.va, src_va.size,
                devid);
        return ret;
    }

    ret = svm_ipc_malloc_opened_va(devid, svm_get_non_dev_align_size(src_va.size), align,
                                   src_ex_info.src_prop.casm_pg_type, opened_va);
    if (ret != 0) {
        svm_err("Malloc opened va failed. (devid=%u; key=0x%llx; size=0x%llx; align=0x%llx)\n", devid, key, src_va.size,
                align);
        return ret;
    }

    ret = svm_casm_mem_map(devid, *opened_va, src_va.size, key, svm_ipc_open_flag_to_casm_flag(flag));
    if (ret != 0) {
        (void)svm_ipc_free_opened_va(*opened_va);
        svm_err("Mem map failed. (devid=%u; key=0x%llx; size=0x%llx; opened_va=0x%llx)\n", devid, key, src_va.size,
                *opened_va);
        if (ret == DRV_ERROR_NO_RESOURCES) {
            svm_report_ubmem_cfg_abnormal();
            ret = DRV_ERROR_INNER_ERR;
        }
        return ret;
    }

    svm_mod_va_devid(*opened_va, devid);
    ret = svm_ipc_set_opened_src_info(*opened_va, &src_va);
    if (ret != 0) {
        (void)svm_casm_mem_unmap(devid, *opened_va, src_va.size);
        (void)svm_ipc_free_opened_va(*opened_va);
        return ret;
    }

    return 0;
}

static int svm_ipc_close_handle(u64 opened_va)
{
    struct svm_global_va src_va;
    struct svm_prop prop;
    int ret;

    if (svm_is_in_linear_mem_va_range(opened_va, 1ULL)) {
        return 0;
    }

    ret = svm_get_prop(opened_va, &prop);
    if (ret != 0) {
        svm_err("Get prop failed. (va=0x%llx)\n", opened_va);
        return ret;
    }

    if (!svm_flag_cap_is_support_ipc_close(prop.flag)) {
        svm_err("Addr cap does not support ipc close. (opened_va=0x%llx)\n", opened_va);
        return DRV_ERROR_PARA_ERROR;
    }

    ret = _svm_ipc_query_src_info(prop.start, &src_va);
    if (ret != 0) {
        svm_err("Src_info does not exist. (va=0x%llx)\n", prop.start);
        return DRV_ERROR_NOT_EXIST;
    }

    ret = svm_casm_mem_unmap(prop.devid, opened_va, src_va.size);
    if (ret != 0) {
        svm_err("Mem unmap failed. (devid=%u; size=0x%llx; opened_va=0x%llx)\n", prop.devid, src_va.size, opened_va);
        return ret;
    }

    ret = svm_ipc_free_opened_va(opened_va);
    if (ret != 0) {
        svm_warn("Free failed. (devid=%u; opened_va=0x%llx)\n", prop.devid, opened_va);
    }

    return 0;
}

DVresult halShmemCreateHandle(DVdeviceptr vptr, size_t byte_count, char *name, uint32_t name_len)
{
    u64 key;
    int ret;

    if ((name == NULL) || (byte_count == 0) || (vptr == 0) || (name_len == 0) || (name_len < SVM_MAX_IPC_NAME_SIZE)) {
        svm_err("Invalid para. (vptr=0x%llx; byte_count=0x%llx; name_len=%u)\n", vptr, (u64)byte_count, name_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_use_pipeline();
    ret = svm_ipc_create_handle((u64)vptr, (u64)byte_count, &key);
    if (ret == 0) {
        ret = svm_ipc_format_name(name, name_len, key);
        if (ret != 0) {
            (void)svm_ipc_destroy_handle(key);
            svm_err("Create name failed. (va=0x%llx; name_len=%u; key=0x%llx)\n", vptr, name_len, key);
        } else {
            svm_debug("ShmemCreateHandle. (vptr=0x%llx; byte_count=%llu; name=%s)\n", vptr, (u64)byte_count, name);
        }
    }

    svm_unuse_pipeline();

    return (DVresult)ret;
}

static int svm_ipc_name_check(const char *name)
{
    size_t str_len;

    str_len = (name != NULL) ? strnlen(name, SVM_MAX_IPC_NAME_SIZE) : 0;
    if ((str_len == 0) || (str_len >= SVM_MAX_IPC_NAME_SIZE)) {
        svm_err("Invalid ipc name. (str_len=%lu)\n", str_len);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

static int svm_ipc_pids_check(int pid[], int num)
{
    if ((pid == NULL) || (num <= 0) || (num > SVM_IPC_WLIST_MAX_SET_NUM_ONCE)) {
        svm_err("Invalid para. (num=%u)\n", num);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

DVresult halShmemSetPidHandle(const char *name, int pid[], int num)
{
    u64 key;
    int ret;

    ret = svm_ipc_pids_check(pid, num);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_ipc_name_check(name);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_ipc_get_key_by_name(name, &key);
    if (ret != 0) {
        svm_err("Get key failed. (name=%s)\n", name);
        return (DVresult)ret;
    }

    return (DVresult)svm_casm_add_local_task(key, pid, (u32)num);
}

DVresult halShmemSetPodPid(const char *name, uint32_t sdid, int pid[], int num)
{
    struct halSDIDParseInfo sdid_parse;
    u64 key;
    int ret;

    ret = svm_ipc_pids_check(pid, num);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_ipc_name_check(name);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_ipc_get_key_by_name(name, &key);
    if (ret != 0) {
        svm_err("Get key failed. (name=%s)\n", name);
        return (DVresult)ret;
    }

    ret = halParseSDID(sdid, &sdid_parse);
    if (ret != 0) {
        svm_err("Parse sdid failed. (name=%s; sdid=0x%x)\n", name, sdid);
        return (DVresult)ret;
    }

    if (svm_get_cur_server_id() != sdid_parse.server_id) {
        return (DVresult)svm_casm_add_task(key, sdid_parse.server_id, pid, (u32)num);
    } else {
        return (DVresult)svm_casm_add_local_task(key, pid, (u32)num);
    }
}

DVresult halShmemDestroyHandle(const char *name)
{
    u64 key;
    int ret;

    ret = svm_ipc_name_check(name);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    svm_debug("ShmemDestroyHandle enter. (name=%s)\n", name);

    ret = svm_ipc_get_key_by_name(name, &key);
    if (ret != 0) {
        svm_err("Get key failed. (name=%s)\n", name);
        return (DVresult)ret;
    }

    svm_use_pipeline();
    ret = svm_ipc_destroy_handle(key);
    svm_unuse_pipeline();

    return (DVresult)ret;
}

static int svm_ipc_open_para_check(u32 devid, const char *name, u64 *vptr, u64 flag)
{
    u32 map_route = (u32)flag;

    if (devid >= SVM_MAX_AGENT_NUM) {
        svm_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((svm_ipc_name_check(name) != 0) || (vptr == NULL)) {
        svm_err("Invalid para. (vptr_is_null=%u)\n", (vptr == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((map_route != MEM_MAP_DEFAULT_PATH) && (map_route != MEM_MAP_UB_ONE_PORT_PATH) &&
        (map_route != MEM_MAP_UB_MULTI_PORT_PATH)) {
        svm_debug("Open flag is not supported. (map_route=%u)\n", map_route);
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

DVresult halShmemOpenHandleV2(DVdevice dev_id, const char *name, DVdeviceptr *vptr, uint64_t flag)
{
    struct svm_global_va src_va;
    int cs_valid, owner_pid;
    u8 version;
    u64 key;
    int ret;

    DRV_CURRENT_API_SCOPE();

    ret = svm_ipc_open_para_check((u32)dev_id, name, (u64 *)vptr, flag);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_ipc_parse_name(name, &version, &key, &cs_valid, &src_va, &owner_pid);
    if (ret != 0) {
        svm_err("Get key failed. (name=%s)\n", name);
        return (DVresult)ret;
    }

    if ((cs_valid != 0) && (src_va.server_id != svm_get_cur_server_id())) {
        ret = svm_casm_cs_set_src_info(dev_id, key, &src_va, owner_pid, NULL);
        if (ret != 0) {
            svm_err("Set cs src failed. (name=%s)\n", name);
            return (DVresult)ret;
        }
    }

    svm_use_pipeline();
    ret = svm_ipc_open_handle((u32)dev_id, key, (u64 *)vptr, flag);
    svm_unuse_pipeline();

    if ((cs_valid != 0) && (svm_get_cur_server_id() != src_va.server_id)) {
        (void)svm_casm_cs_clr_src_info(dev_id, key);
    }
    svm_debug("halShmemOpenHandleV2. (devid=%u; name=%s; ptr=0x%llx; flag=0x%llx)\n", (u32)dev_id, name, *vptr, flag);
    return (DVresult)ret;
}

DVresult halShmemOpenHandleByDevId(DVdevice dev_id, const char *name, DVdeviceptr *vptr)
{
    DRV_CURRENT_API_SCOPE();

    return halShmemOpenHandleV2(dev_id, name, vptr, 0);
}

DVresult halShmemOpenHandle(const char *name, DVdeviceptr *vptr)
{
    SVM_UNUSED(name);
    SVM_UNUSED(vptr);

    return DRV_ERROR_NOT_SUPPORT;
}

DVresult halShmemCloseHandle(DVdeviceptr vptr)
{
    int ret;

    svm_debug("ShmemCloseHandle enter. (vptr=0x%llx)\n", vptr);

    if (vptr == 0) {
        svm_err("Invalid para. (vptr=0x%llx)\n", vptr);
        return DRV_ERROR_INVALID_VALUE;
    }

    svm_use_pipeline();
    ret = svm_ipc_close_handle((u64)vptr);
    svm_unuse_pipeline();

    if (ret != 0) {
        svm_err("Ipc close handle failed. (ret=%d; vptr=0x%llx)\n", ret, vptr);
    }
    return (DVresult)ret;
}

static int ipc_set_mwl_attr(u64 key, u64 attr)
{
    int tgid = SVM_ANY_TASK_ID;

    if (attr == SHMEM_WLIST_ENABLE) {
        svm_run_info("Not support enable mwl.\n");
        return DRV_ERROR_NOT_SUPPORT;
    } else if (attr == SHMEM_NO_WLIST_ENABLE) {
        return svm_casm_add_local_task(key, &tgid, 1);
    } else {
        svm_err("Invalid shm mwl attr val. (attr=%llu)\n", attr);
        return DRV_ERROR_PARA_ERROR;
    }
}

static int ipc_get_mwl_attr(u64 key, u64 *attr)
{
    int tgid = SVM_ANY_TASK_ID;
    int ret;

    ret = svm_casm_check_local_task(key, &tgid, 1);
    if ((ret != DRV_ERROR_NONE) && (ret != DRV_ERROR_NO_RESOURCES)) {
        svm_err("Check any task failed. (ret=%d; key=0x%llx)", ret, key);
        return ret;
    }

    *attr = (ret == DRV_ERROR_NONE) ? SHMEM_NO_WLIST_ENABLE : SHMEM_WLIST_ENABLE;
    return DRV_ERROR_NONE;
}

static int ipc_map_route_para_check(const char *name, u32 dst_devid, u32 map_route)
{
    int ret;

    ret = svm_ipc_name_check(name);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    if (dst_devid >= SVM_MAX_DEV_NUM) {
        svm_err("Invalid dst_devid. (dst_devid=%u)\n", dst_devid);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if ((map_route != MEM_MAP_DEFAULT_PATH) && (map_route != MEM_MAP_UB_ONE_PORT_PATH) &&
        (map_route != MEM_MAP_UB_MULTI_PORT_PATH)) {
        svm_debug("Ipc map route not exist. (map_route=%u)\n", map_route);
        return DRV_ERROR_NOT_EXIST;
    }

    return DRV_ERROR_NONE;
}

static int ipc_mem_get_ub_info_by_name(const char *name, u32 dst_devid, u32 *src_server_id, u32 *mem_id)
{
    struct casm_src_va_ex_info casm_ex_info;
    struct svm_global_va src_va;
    int cs_valid, owner_pid;
    u64 key, offset;
    u8 version;
    int ret;

    ret = svm_ipc_parse_name(name, &version, &key, &cs_valid, &src_va, &owner_pid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get key failed. (name=%s)\n", name);
        return ret;
    }

    if ((cs_valid != 0) && (src_va.server_id != svm_get_cur_server_id())) {
        ret = svm_casm_cs_set_src_info(dst_devid, key, &src_va, owner_pid, NULL);
        if (ret != 0) {
            svm_err("Set cs src failed. (name=%s)\n", name);
            return ret;
        }
    }

    ret = svm_casm_get_src_va_ex(dst_devid, key, &src_va, &casm_ex_info);
    if ((cs_valid != 0) && (src_va.server_id != svm_get_cur_server_id())) {
        (void)svm_casm_cs_clr_src_info(dst_devid, key);
    }
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get src va failed. (name=%s)\n", name);
        return ret;
    }
    if (casm_ex_info.update_va == 0) {
        svm_debug("Not ubmem map. (name=%s; key=%llu)\n", name, key);
        return DRV_ERROR_NOT_EXIST;
    }

    svm_ub_va_to_mem_id_and_offset(casm_ex_info.update_va, mem_id, &offset);
    *src_server_id = src_va.server_id;
    return DRV_ERROR_NONE;
}

DVresult halShmemMapRouteCheck(const char *name, uint32_t dst_devid, uint32_t map_route)
{
    u32 src_server_id;
    u32 mem_id;
    int ret;

    ret = ipc_map_route_para_check(name, dst_devid, map_route);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = ipc_mem_get_ub_info_by_name(name, dst_devid, &src_server_id, &mem_id);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    src_server_id = (src_server_id == SVM_INVALID_SERVER_ID) ? UBMM_INVALID_SERVER_ID : src_server_id;
    return ubmemMapRouteCheck(dst_devid, src_server_id, mem_id, map_route);
}

DVresult halShmemSetAttribute(const char *name, uint32_t type, uint64_t attr)
{
    static int (*ipc_set_attr_func[SHMEM_ATTR_TYPE_MAX])(
        u64 key, u64 attr) = {[SHMEM_ATTR_TYPE_NO_WLIST_IN_SERVER] = ipc_set_mwl_attr};
    u64 key;
    int ret;

    if ((type >= SHMEM_ATTR_TYPE_MAX) || (ipc_set_attr_func[type] == NULL)) {
        svm_debug("Not support type. (type=%d)\n", type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = svm_ipc_name_check(name);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_ipc_get_key_by_name(name, &key);
    if (ret != 0) {
        svm_err("Get key failed. (name=%s)\n", name);
        return (DVresult)ret;
    }

    ret = ipc_set_attr_func[type](key, attr);
    if (ret == DRV_ERROR_NONE) {
        svm_debug("Shmem set attribute successfully. (name=%s; type=%u; attr=%llu)\n", name, type, attr);
    }

    return ret;
}

DVresult halShmemGetAttribute(const char *name, enum ShmemAttrType type, uint64_t *attr)
{
    static int (*ipc_get_attr_func[SHMEM_ATTR_TYPE_MAX])(
        u64 key, u64 *attr) = {[SHMEM_ATTR_TYPE_NO_WLIST_IN_SERVER] = ipc_get_mwl_attr};
    u64 key;
    int ret;

    if (attr == NULL) {
        svm_err("Attr is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((type >= SHMEM_ATTR_TYPE_MAX) || (ipc_get_attr_func[type] == NULL)) {
        svm_debug("Not support type. (type=%d)\n", type);
        return DRV_ERROR_NOT_SUPPORT;
    }

    ret = svm_ipc_name_check(name);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_ipc_get_key_by_name(name, &key);
    if (ret != 0) {
        svm_err("Get key failed. (name=%s)\n", name);
        return (DVresult)ret;
    }

    ret = ipc_get_attr_func[type](key, (u64 *)attr);
    if (ret == DRV_ERROR_NONE) {
        svm_debug("Shmem get attribute successfully. (name=%s; type=%u; attr=%llu)\n", name, type, *attr);
    }

    return ret;
}

DVresult halShmemInfoGet(const char *name, struct ShmemGetInfo *info)
{
    struct svm_global_va src_va_cs, src_va;
    int cs_valid, owner_pid;
    u64 key;
    u8 version;
    int ret;

    if (info == NULL) {
        svm_err("Info is null.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = svm_ipc_name_check(name);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    ret = svm_ipc_parse_name(name, &version, &key, &cs_valid, &src_va_cs, &owner_pid);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get key failed. (name=%s)\n", name);
        return ret;
    }

    if ((cs_valid != 0) && (src_va_cs.server_id != svm_get_cur_server_id())) {
        ret = svm_casm_cs_set_src_info(svm_get_host_devid(), key, &src_va_cs, owner_pid, NULL);
        if (ret != 0) {
            svm_err("Set cs src failed. (name=%s)\n", name);
            return (DVresult)ret;
        }
    }

    ret = svm_casm_get_src_va(svm_get_host_devid(), key, &src_va);
    if ((cs_valid != 0) && (src_va_cs.server_id != svm_get_cur_server_id())) {
        (void)svm_casm_cs_clr_src_info(svm_get_host_devid(), key);
    }

    if (ret != DRV_ERROR_NONE) {
        svm_err("Casm get src va failed. (ret=%d; key=0x%llx)\n", ret, key);
        return ret;
    }

    info->phyDevid = src_va.udevid;

    svm_debug("Shmem get info successfully. (name=%s; phyDevid=%u)\n", name, info->phyDevid);

    return DRV_ERROR_NONE;
}
