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

#include "udma_ctl.h"
#include "asdrv_ub_mem_decoder.h"
#include "asdrv_ub_main.h"

struct ubdrv_ubmem_host_cfg_info g_host_cfg_info = {0};
struct ubdrv_ubmem_decoder_info g_decoder_info = {0};

int ubdrv_set_device_mem_decoder_info(u32 dev_id, struct ubmem_decoder_msg_data *data)
{
    (void)dev_id;
    (void)data;
    return 0;
}

bool ubdrv_addr_trans_is_H2D(void)
{
    return true;
}

STATIC int ubdrv_addr_trans_to_local(u32 udevid, u32 peer_udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr)
{
    u64 mem_id, offset;
    int ret, mode;

    (void)peer_udevid;

    ka_task_down_read(&g_decoder_info.rw_sem);
    ret = ubdrv_addr_trans_pre_check(udevid, addr_desc, trans_addr);
    if (ret != 0) {
        ka_task_up_read(&g_decoder_info.rw_sem);
        if (ret != -ENOENT) {
            return ret;
        }
        ret = ubdrv_query_device_mem_decoder_info(udevid, UBDRV_WARN_LEVEL);
        if (ret != 0) {
            ubdrv_warn("Mem decoder info not set and query abnormal. (udevid=%u)\n", udevid);
            return ret;
        }
        ka_task_down_read(&g_decoder_info.rw_sem);
    }

    ret = ubdrv_addr_trans_version_check(udevid, addr_desc);
    if (ret != 0) {
        ka_task_up_read(&g_decoder_info.rw_sem);
        return ret;
    }

    mem_id = addr_desc->peer_ub.mem_id;
    offset = addr_desc->peer_ub.offset;
    mode = addr_desc->peer_ub.mode;
    /* Mode value has been validated by ubdrv_addr_trans_pre_check under the same lock. */
    ka_base_atomic_inc(&g_decoder_info.trans_stats.req_cnt[mode]);

    if (g_decoder_info.decoder_info->version == UBMEM_DECODER_MSG_VERSION_ORIGIN) {
        ret = ubdrv_addr_trans_v0(0, mem_id, mode, offset, trans_addr);
    } else {
        ret = ubdrv_addr_trans_v1(0, mem_id, mode, offset, trans_addr);
    }
    ka_task_up_read(&g_decoder_info.rw_sem);

    return ret;
}

int ubdrv_addr_trans_local_to_peer(u32 udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr)
{
    int ret = ubdrv_addr_trans_to_local(udevid, 0, addr_desc, trans_addr);
    if (ret != 0) {
        ubdrv_warn("Trans the l2p addr abnormal. (udevid=%u; ret=%d)\n", udevid, ret);
    }
    return ret;
}

bool ubdrv_query_addr_trans_support(enum devdrv_addr_trans_cap_type cap_type)
{
    bool support = false;

    if (cap_type >= DEVDRV_ADDR_TRANS_CAP_TYPE_MAX) {
        ubdrv_warn("Invalid addr trans cap type. (cap_type=%u)\n", (u32)cap_type);
        return false;
    }

    if (cap_type == DEVDRV_ADDR_TRANS_CAP_D2H) {
        ka_task_down_read(&g_host_cfg_info.rw_sem);
        support = (g_host_cfg_info.valid == ASCEND_UB_VALID);
        ka_task_up_read(&g_host_cfg_info.rw_sem);
    } else {
        ka_task_down_read(&g_decoder_info.rw_sem);
        support = (g_decoder_info.valid == ASCEND_UB_VALID);
        ka_task_up_read(&g_decoder_info.rw_sem);
    }

    return support;
}

int ubdrv_mem_host_cfg_ctrl_init(void)
{
    size_t len = sizeof(struct ubmem_host_cfg_msg_data);

    ka_task_init_rwsem(&g_host_cfg_info.rw_sem);
    ka_base_atomic_set(&g_host_cfg_info.update_cnt, 0);
    ka_task_down_write(&g_host_cfg_info.rw_sem);
    g_host_cfg_info.valid = ASCEND_UB_INVALID;
    g_host_cfg_info.cfg_info = ubdrv_kzalloc(len, KA_GFP_KERNEL);
    if (g_host_cfg_info.cfg_info == NULL) {
        ubdrv_err("Failed to ubdrv_kzalloc mem cfg info (len=%zu)\n", len);
        ka_task_up_write(&g_host_cfg_info.rw_sem);
        return -ENOMEM;
    }
    ka_task_up_write(&g_host_cfg_info.rw_sem);
    return 0;
}

void ubdrv_mem_host_cfg_ctrl_uninit(void)
{
    ka_task_down_write(&g_host_cfg_info.rw_sem);
    g_host_cfg_info.valid = ASCEND_UB_INVALID;
    if (g_host_cfg_info.cfg_info != NULL) {
        ubdrv_kfree(g_host_cfg_info.cfg_info);
        g_host_cfg_info.cfg_info = NULL;
    }
    ka_task_up_write(&g_host_cfg_info.rw_sem);
    return;
}

int ubdrv_procfs_mem_host_cfg_dfx_show(ka_seq_file_t *seq, void *offset)
{
    struct ubmem_host_cfg_msg_data *cfg_info = NULL;

    ka_task_down_read(&g_host_cfg_info.rw_sem);
    if (g_host_cfg_info.valid == ASCEND_UB_INVALID) {
        ka_fs_seq_printf(seq, "Ub mem host cfg info is invalid.\n");
        ka_task_up_read(&g_host_cfg_info.rw_sem);
        return 0;
    }

    cfg_info = g_host_cfg_info.cfg_info;
    ka_fs_seq_printf(seq, "-------------- ub host mem_cfg info --------------.\n");
    ka_fs_seq_printf(seq, "version: %hu\n", cfg_info->version);
    ka_fs_seq_printf(seq, "mem_id: %hu\n", cfg_info->mem_id);
    ka_fs_seq_printf(seq, "tid: %u\n", cfg_info->tid);
    ka_fs_seq_printf(seq, "max_share_mem_size: 0x%llx\n", cfg_info->max_share_mem_size);
    ka_fs_seq_printf(seq, "uba: 0x%pK\n", (void *)(uintptr_t)cfg_info->uba);
    ka_fs_seq_printf(seq, "rsv: %hu\n", cfg_info->rsv);
    ka_fs_seq_printf(seq, "rsv1: %u\n", cfg_info->rsv1);
    ka_fs_seq_printf(seq, "update_cnt: %d\n", ka_base_atomic_read(&g_host_cfg_info.update_cnt));
    ka_task_up_read(&g_host_cfg_info.rw_sem);
    return 0;
}

int ubdrv_query_host_mem_decoder_info(u32 dev_id, enum ubdrv_log_level log_level)
{
    struct ubmem_host_cfg_msg_data *msg_data = NULL;
    struct ubcore_user_ctl user_ctl = {0};
    struct ubcore_device *ubc_dev = NULL;
    struct ubdrv_ubmem_set_decoder_info set_decoder_info;
    size_t cfg_len = sizeof(struct ubmem_host_cfg_msg_data);
    int update_cnt;
    int ret = 0;

    ubc_dev = ubdrv_get_default_user_ctrl_urma_dev();
    if (ubc_dev == NULL) {
        UBDRV_LOG_LEVEL(log_level, "Get ubc_dev unsuccessful. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    if (g_host_cfg_info.cfg_info == NULL) {
        UBDRV_LOG_LEVEL(log_level, "Cfg_info is null. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    msg_data = (struct ubmem_host_cfg_msg_data *)ubdrv_kzalloc(cfg_len, KA_GFP_KERNEL);
    if (msg_data == NULL) {
        ubdrv_err("Alloc mem to query host cfg failed. (dev_id=%u, cfg_len=%zu)\n", dev_id, cfg_len);
        return -ENOMEM;
    }

    /* Query into the shadow buffer first, the old cfg keeps serving readers during the query. */
    user_ctl.uctx = NULL;
    user_ctl.in.opcode = UDMA_USER_CTL_QUERY_HOST_UBMEM_INFO;
    user_ctl.out.addr = (u64)(uintptr_t)msg_data;
    user_ctl.out.len = cfg_len;
    ret = ubcore_user_control(ubc_dev, &user_ctl);
    if (ret != 0) {
        UBDRV_LOG_LEVEL(log_level, "Get ub host mem cfg info unsuccessful. (dev_id=%u;ret=%d)\n", dev_id, ret);
        ubdrv_kfree(msg_data);
        return ret;
    }

    ka_task_down_write(&g_host_cfg_info.rw_sem);
    if ((g_host_cfg_info.valid == ASCEND_UB_VALID) &&
        (ka_base_memcmp(g_host_cfg_info.cfg_info, msg_data, cfg_len) == 0)) {
        /* Config unchanged, keep the old cfg and skip the soc_res set. */
        ka_task_up_write(&g_host_cfg_info.rw_sem);
        ubdrv_kfree(msg_data);
        return 0;
    }

    /* Config changed or first query, log the new snapshot before overwriting, then set to soc_res. */
    update_cnt = ka_base_atomic_inc_return(&g_host_cfg_info.update_cnt);
    ubdrv_info("Ub host mem cfg info changed, update cache. (dev_id=%u; update_cnt=%d; uba_changed=%d; "
               "max_share_mem_size=0x%llx; tid=%llu; mem_id=%llu)\n",
               dev_id, update_cnt, (int)(g_host_cfg_info.cfg_info->uba != msg_data->uba), msg_data->max_share_mem_size,
               (u64)msg_data->tid, (u64)msg_data->mem_id);
    (void)memcpy_s(g_host_cfg_info.cfg_info, cfg_len, msg_data, cfg_len);

    set_decoder_info.dev_id = dev_id;
    set_decoder_info.uba = msg_data->uba;
    set_decoder_info.max_share_mem_size = msg_data->max_share_mem_size;
    set_decoder_info.mem_id = (u64)msg_data->mem_id;
    set_decoder_info.tid = (u64)msg_data->tid;

    ret = ubdrv_set_mem_decoder_info(&set_decoder_info);
    if (ret != 0) {
        ubdrv_err("Set host ub mem cfg info to soc_res fail. (dev_id=%u;ret=%d)\n", dev_id, ret);
    } else {
        g_host_cfg_info.valid = ASCEND_UB_VALID;
    }

    ka_task_up_write(&g_host_cfg_info.rw_sem);
    ubdrv_kfree(msg_data);
    return ret;
}