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
#include "asdrv_ub_main.h"
#include "asdrv_ub_mem_decoder.h"
#include "pbl/pbl_soc_res.h"
#include "pbl/pbl_uda.h"

#define UBDRV_IDX_INVALID (-1)

int ubdrv_set_mem_decoder_info(struct ubdrv_ubmem_set_decoder_info *info)
{
    struct soc_reg_base_info io_base = {0};
    int ret;

    io_base.io_base = (phys_addr_t)info->uba;
    io_base.io_base_size = (size_t)info->max_share_mem_size;
    ret = soc_resmng_dev_set_reg_base(info->dev_id, UB_MEM_KEY_UB_BASE, &io_base);
    if (ret != 0) {
        ubdrv_err("Failed to set uba base info to soc_res. (dev_id=%u;ret=%d)\n", info->dev_id, ret);
        return ret;
    }

    ret = soc_resmng_dev_set_key_value(info->dev_id, UB_MEM_KEY_UB_TID, info->tid);
    if (ret != 0) {
        ubdrv_err("Failed to set uba key info to soc_res. (dev_id=%u;ret=%d)\n", info->dev_id, ret);
        return ret;
    }

    ret = soc_resmng_dev_set_key_value(info->dev_id, UB_MEM_KEY_UB_MEM_ID, info->mem_id);
    if (ret != 0) {
        ubdrv_err("Failed to set uba to soc_res. (dev_id=%u;ret=%d)\n", info->dev_id, ret);
        return ret;
    }

    return 0;
}

STATIC int ubdrv_save_mem_decoder_info_first_time(u32 dev_id, struct ubmem_decoder_msg_data **new_info,
                                                  struct ubmem_decoder_msg_data *data)
{
    size_t len;

    len = sizeof(struct ubmem_decoder_msg_data) + data->total_nonlinear_pa_num * sizeof(struct ubmem_nonlinear_pa_info);
    *new_info = (struct ubmem_decoder_msg_data *)ubdrv_kzalloc(len, KA_GFP_KERNEL);
    if (*new_info == NULL) {
        ubdrv_err("Fail to alloc mem to save decoder. (dev_id=%u; total_pa_num=%llu; len=%zu)\n", dev_id,
                  (u64)data->total_nonlinear_pa_num, len);
        return -ENOMEM;
    }

    (void)memcpy_s(*new_info, sizeof(struct ubmem_decoder_msg_data), data, sizeof(struct ubmem_decoder_msg_data));
    return 0;
}

STATIC int ubdrv_save_nonlinear_info(struct ubmem_decoder_msg_data *new_info, struct ubmem_decoder_msg_data *data,
                                     u64 *idx)
{
    size_t arr_size = data->nonlinear_pa_num * sizeof(struct ubmem_nonlinear_pa_info);

    if (*idx + data->nonlinear_pa_num > new_info->total_nonlinear_pa_num) {
        ubdrv_err("Actual length of nonlinear has exceeded. (idx=%llu; data_len=%llu; total_len=%llu)\n", *idx,
                  (u64)data->nonlinear_pa_num, (u64)new_info->total_nonlinear_pa_num);
        return -EINVAL;
    }

    (void)memcpy_s(&new_info->nonlinear_pa_info[*idx], arr_size, data->nonlinear_pa_info, arr_size);
    *idx += data->nonlinear_pa_num;

    return 0;
}

STATIC int ubmem_save_mem_decoder_info_v1(u32 dev_id, struct ubmem_decoder_msg_data **new_info,
                                          struct ubmem_decoder_msg_data *data, u64 *idx)
{
    int ret;

    if (*new_info == NULL) {
        ret = ubdrv_save_mem_decoder_info_first_time(dev_id, new_info, data);
        if (ret != 0) {
            ubdrv_err("Fail to save mem decoder info for the first time. (ret=%d, dev_id=%u)\n", ret, dev_id);
            return ret;
        }
    }

    ret = ubdrv_save_nonlinear_info(*new_info, data, idx);
    if (ret != 0) {
        ubdrv_err("Fail to save mem decoder info. (ret=%d, dev_id=%u, idx=%llu)\n", ret, dev_id, *idx);
        return ret;
    }

    return 0;
}

STATIC int ubmem_save_mem_decoder_info_v0(u32 dev_id, struct ubmem_decoder_msg_data **new_info,
                                          struct ubmem_decoder_msg_data *data, u64 *idx)
{
    size_t len;

    (void)idx;

    if (data->total_nonlinear_pa_num > ASCEND_UB_MEM_NONLINEAR_NUM_V0) {
        ubdrv_err("total_nonlinear_pa_num is invalid. (dev_id=%u; total_pa_num=%llu)\n", dev_id,
                  (u64)data->total_nonlinear_pa_num);
        return -ENOMEM;
    }

    data->total_nonlinear_pa_num = data->nonlinear_pa_num;
    data->eof = (u64)1; // set the end flag of the query to 1;

    len = sizeof(struct ubmem_decoder_msg_data) + data->total_nonlinear_pa_num * sizeof(struct ubmem_nonlinear_pa_info);
    *new_info = (struct ubmem_decoder_msg_data *)ubdrv_kzalloc(len, KA_GFP_KERNEL);
    if (*new_info == NULL) {
        ubdrv_err("Fail to alloc mem to save decoder. (dev_id=%u; total_pa_num=%llu; len=%zu)\n", dev_id,
                  (u64)data->total_nonlinear_pa_num, len);
        return -ENOMEM;
    }
    (void)memcpy_s(*new_info, len, data, len);

    return 0;
}

STATIC int ubdrv_save_mem_decoder_info_adapt(u32 dev_id, struct ubmem_decoder_msg_data **new_info,
                                             struct ubmem_decoder_msg_data *data, u64 *idx)
{
    int ret = 0;

    switch (data->version) {
        case UBMEM_DECODER_MSG_VERSION_ORIGIN:
            ret = ubmem_save_mem_decoder_info_v0(dev_id, new_info, data, idx);
            break;
        case UBMEM_DECODER_MSG_VERSION_ONEPATH:
            ret = ubmem_save_mem_decoder_info_v1(dev_id, new_info, data, idx);
            break;
        default:
            ubdrv_warn("The ubdrv version is lower than ubmem. (ubdrv_ver=%u, ubmem_ver=%u)\n",
                       UBMEM_DECODER_MSG_VERSION_ONEPATH, data->version);
            ret = -EACCES;
            break;
    }

    return ret;
}

STATIC void ubdrv_free_mem_decoder_info(void)
{
    g_decoder_info.valid = ASCEND_UB_INVALID;
    if (g_decoder_info.decoder_info != NULL) {
        ubdrv_kfree(g_decoder_info.decoder_info);
        g_decoder_info.decoder_info = NULL;
    }
}

/*
 * Check whether the newly queried decoder info is identical to the cached one.
 * Return true: identical, the caller can discard the new buffer and keep the cache.
 * Return false: no cache yet, or any field/PA entry has changed.
 */
STATIC bool ubdrv_mem_decoder_info_is_same(const struct ubmem_decoder_msg_data *new_info)
{
    const struct ubmem_decoder_msg_data *old_info = g_decoder_info.decoder_info;
    size_t len;

    if (old_info == NULL) {
        return false;
    }

    if (old_info->total_nonlinear_pa_num != new_info->total_nonlinear_pa_num) {
        return false;
    }

    len = sizeof(struct ubmem_decoder_msg_data) +
          new_info->total_nonlinear_pa_num * sizeof(struct ubmem_nonlinear_pa_info);
    return ka_base_memcmp(old_info, new_info, len) == 0;
}

int ubdrv_query_device_mem_decoder_info(u32 dev_id, enum ubdrv_log_level log_level)
{
    struct ubmem_communication_info_req_data in_msg = {0};
    struct ubmem_decoder_msg_data *new_info = NULL;
    struct ubmem_decoder_msg_data *out_msg = NULL;
    struct ubcore_user_ctl user_ctl = {0};
    struct ubcore_device *ubc_dev = NULL;
    int update_cnt = 0, ret = 0;
    size_t out_msg_len;
    u64 idx;

    ubc_dev = ubdrv_get_default_user_ctrl_urma_dev();
    if (ubc_dev == NULL) {
        ka_base_atomic_inc(&g_decoder_info.query_stats.other_fail_cnt);
        UBDRV_LOG_LEVEL(log_level, "Get ub device unsuccessful. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    out_msg_len = sizeof(struct ubmem_decoder_msg_data) +
                  ASCEND_UB_MEM_NONLINEAR_NUM_V0 * sizeof(struct ubmem_nonlinear_pa_info);
    out_msg = (struct ubmem_decoder_msg_data *)ubdrv_kzalloc(out_msg_len, KA_GFP_KERNEL);
    if (out_msg == NULL) {
        ka_base_atomic_inc(&g_decoder_info.query_stats.other_fail_cnt);
        ubdrv_err("Alloc mem to query mem decoder failed. (dev_id=%u, out_msg_len=%zu)\n", dev_id, out_msg_len);
        return -ENOMEM;
    }

    in_msg.nonlinear_pa_start_idx = 0;
    in_msg.nonlinear_pa_num = UB_MEM_SINGLE_QUERY_NUM;

    user_ctl.uctx = NULL;
    user_ctl.in.opcode = UDMA_USER_CTL_QUERY_UBMEM_INFO;
    user_ctl.in.addr = (u64)(uintptr_t)(&in_msg);
    user_ctl.in.len = sizeof(struct ubmem_communication_info_req_data);
    user_ctl.out.addr = (u64)(uintptr_t)out_msg;
    user_ctl.out.len = out_msg_len;

    do {
        ret = ubcore_user_control(ubc_dev, &user_ctl);
        if (ret != 0) {
            ka_base_atomic_inc(&g_decoder_info.query_stats.ubcore_fail_cnt);
            UBDRV_LOG_LEVEL(log_level, "Get ub mem decoder info unsuccessful. (dev_id=%u;ret=%d)\n", dev_id, ret);
            goto query_failed;
        }
        idx = in_msg.nonlinear_pa_start_idx;
        ret = ubdrv_save_mem_decoder_info_adapt(dev_id, &new_info, out_msg, &idx);
        if (ret != 0) {
            ka_base_atomic_inc(&g_decoder_info.query_stats.other_fail_cnt);
            ubdrv_warn("Save ub mem decoder info abnormal. (dev_id=%u, ret=%d)\n", dev_id, ret);
            goto query_failed;
        }
        in_msg.nonlinear_pa_start_idx = idx;
    } while (out_msg->eof == 0);

    ka_task_down_write(&g_decoder_info.rw_sem);
    if (ubdrv_mem_decoder_info_is_same(new_info)) {
        /* Config unchanged, discard the shadow buffer and keep the old cache. */
        ubdrv_kfree(new_info);
    } else {
        /* Sync device side resources only after the config is confirmed changed, set fails keeps the old cache. */
        ret = ubdrv_set_device_mem_decoder_info(dev_id, new_info);
        if (ret != 0) {
            ubdrv_err("Set device mem decoder info failed, keep the old cache. (dev_id=%u;ret=%d)\n", dev_id, ret);
            ubdrv_kfree(new_info);
            ka_task_up_write(&g_decoder_info.rw_sem);
            ubdrv_kfree(out_msg);
            return ret;
        }

        update_cnt = ka_base_atomic_inc_return(&g_decoder_info.query_stats.update_cnt);
        ubdrv_info("Ub mem decoder info changed, update cache. (dev_id=%u; total_nonlinear_pa_num=%llu; "
                   "update_cnt=%d)\n",
                   dev_id, (u64)new_info->total_nonlinear_pa_num, update_cnt);
        ubdrv_free_mem_decoder_info();
        g_decoder_info.decoder_info = new_info;
        g_decoder_info.valid = ASCEND_UB_VALID;
    }
    ka_task_up_write(&g_decoder_info.rw_sem);
    ubdrv_kfree(out_msg);
    return 0;

query_failed:
    /* Keep the old cache on failure, it will be refreshed by the next query. */
    if (new_info != NULL) {
        ubdrv_kfree(new_info);
    }
    ubdrv_kfree(out_msg);
    return ret;
}

int ubdrv_mem_decoder_ctrl_init(void)
{
    int ret;
    u32 i;

    ka_task_init_rwsem(&g_decoder_info.rw_sem);
    ka_base_atomic_set(&g_decoder_info.query_stats.update_cnt, 0);
    ka_base_atomic_set(&g_decoder_info.query_stats.uda_trigger_cnt, 0);
    ka_base_atomic_set(&g_decoder_info.query_stats.ubcore_fail_cnt, 0);
    ka_base_atomic_set(&g_decoder_info.query_stats.other_fail_cnt, 0);
    for (i = 0; i < UBDRV_TRANS_PATH_TYPE_MAX; i++) {
        ka_base_atomic_set(&g_decoder_info.trans_stats.req_cnt[i], 0);
    }
    for (i = 0; i < UBDRV_TRANS_RESULT_TYPE_MAX; i++) {
        ka_base_atomic_set(&g_decoder_info.trans_stats.result_cnt[i], 0);
    }
    ka_task_down_write(&g_decoder_info.rw_sem);
    g_decoder_info.valid = ASCEND_UB_INVALID;
    ka_task_up_write(&g_decoder_info.rw_sem);

    ret = ubdrv_query_device_mem_decoder_info(0, UBDRV_INFO_LEVEL);
    if (ret != 0) {
        ubdrv_warn("Query ub mem decoder from control plane unsuccessful. (ret=%d)\n", ret);
    }
    return 0;
}

void ubdrv_mem_decoder_ctrl_uninit(void)
{
    ka_task_down_write(&g_decoder_info.rw_sem);
    ubdrv_free_mem_decoder_info();
    ka_task_up_write(&g_decoder_info.rw_sem);
    return;
}

bool ubdrv_addr_trans_valid_check(u64 base_addr_start, u64 addr_size, u64 target_addr)
{
    u64 valid_addr_end = base_addr_start + addr_size;

    if (base_addr_start >= valid_addr_end) {
        return false;
    }

    if ((target_addr < base_addr_start) || (target_addr >= valid_addr_end)) {
        return false;
    }

    return true;
}

STATIC void ubdrv_addr_trans_get_nonlinear_index_v1_to_host(int *one_idx, int *multi_idx, bool *linear_valid)
{
    int idx1 = UBDRV_IDX_INVALID, idx2 = UBDRV_IDX_INVALID;
    struct ubmem_nonlinear_pa_info *info = NULL;
    u64 i;

    for (i = 0; i < g_decoder_info.decoder_info->total_nonlinear_pa_num; i++) {
        info = &g_decoder_info.decoder_info->nonlinear_pa_info[i];
        if (info->to_host == 1) {
            if (info->onepath == UB_MEM_ONEPATH_MODE) {
                if (idx1 == UBDRV_IDX_INVALID) {
                    idx1 = (int)i;
                }
            } else {
                if (idx2 == UBDRV_IDX_INVALID) {
                    idx2 = (int)i;
                }
            }
            if ((idx1 != UBDRV_IDX_INVALID) && (idx2 != UBDRV_IDX_INVALID)) {
                break;
            }
        }
    }
    *one_idx = idx1;
    *multi_idx = idx2;
    *linear_valid = false;
}

STATIC void ubdrv_addr_trans_get_nonlinear_index_v1_to_dev(u64 mem_id, int *one_idx, int *multi_idx, bool *linear_valid)
{
    int idx1 = UBDRV_IDX_INVALID, idx2 = UBDRV_IDX_INVALID;
    struct ubmem_nonlinear_pa_info *info = NULL;
    bool valid = !ubdrv_addr_trans_is_H2D(); // D2D
    u64 i;

    for (i = 0; i < g_decoder_info.decoder_info->total_nonlinear_pa_num; i++) {
        info = &g_decoder_info.decoder_info->nonlinear_pa_info[i];
        if ((mem_id == info->remote_id) && (info->to_host == 0)) {
            if (info->onepath == UB_MEM_ONEPATH_MODE) {
                if (idx1 == UBDRV_IDX_INVALID) {
                    idx1 = (int)i;
                }
            } else {
                if (idx2 == UBDRV_IDX_INVALID) {
                    idx2 = (int)i;
                    valid = false;
                }
            }
            valid = (info->linear_pa_valid == 0) ? false : valid;
            if ((idx1 != UBDRV_IDX_INVALID) && (idx2 != UBDRV_IDX_INVALID)) {
                break;
            }
        }
    }
    *one_idx = idx1;
    *multi_idx = idx2;
    *linear_valid = valid;
}

STATIC void ubdrv_addr_trans_get_nonlinear_index_v1(u64 to_host, u64 mem_id, int *one_idx, int *multi_idx,
                                                    bool *linear_valid)
{
    if (to_host == 0) {
        ubdrv_addr_trans_get_nonlinear_index_v1_to_dev(mem_id, one_idx, multi_idx, linear_valid);
    } else {
        ubdrv_addr_trans_get_nonlinear_index_v1_to_host(one_idx, multi_idx, linear_valid);
    }
}

STATIC int ubdrv_addr_trans_calc_nonlinear_addr(u64 index, u64 offset, u64 *addr)
{
    u64 pa_addr_size, pa_base = 0;

    if (index >= g_decoder_info.decoder_info->total_nonlinear_pa_num) {
        return -EINVAL;
    }
    pa_addr_size = g_decoder_info.decoder_info->nonlinear_pa_info[index].pa_size;
    pa_base = g_decoder_info.decoder_info->nonlinear_pa_info[index].pa_base;
    if ((offset >= pa_addr_size) || (pa_base >= (pa_base + pa_addr_size))) {
        return -EINVAL;
    }
    *addr = pa_base + offset;
    return 0;
}

STATIC int ubdrv_addr_trans_calc_linear_addr(u64 mem_id, u64 offset, u64 *trans_addr)
{
    u64 pa_share_size = g_decoder_info.decoder_info->max_share_mem_size;
    u64 pa_addr_size = g_decoder_info.decoder_info->linear_pa_size;
    u64 pa_base = g_decoder_info.decoder_info->linear_pa_base;
    u64 addr_tmp;

    if (offset >= pa_share_size) {
        return -EINVAL;
    }
    addr_tmp = pa_base + mem_id * pa_share_size + offset;
    if (!ubdrv_addr_trans_valid_check(pa_base, pa_addr_size, addr_tmp)) {
        return -EINVAL;
    }
    *trans_addr = addr_tmp;
    return 0;
}

STATIC int ubdrv_addr_trans_get_one_path_addr(int index, u64 offset, int mode, u64 *trans_addr)
{
    if (index == UBDRV_IDX_INVALID) {
        ubdrv_warn("This mem_id does not exist in the one_path nonlinear decoder. (offset=0x%llx; mode=%d)\n", offset,
                   mode);
        return -EINVAL;
    }
    if (ubdrv_addr_trans_calc_nonlinear_addr((u64)index, offset, trans_addr) != 0) {
        ubdrv_warn("This mem_id is nonlinear, but offset has exceed limit len. (offset=0x%llx; mode=%d; "
                   "pa_size=0x%llx)\n",
                   offset, mode, g_decoder_info.decoder_info->nonlinear_pa_info[index].pa_size);
        return -EINVAL;
    }
    return 0;
}

STATIC int ubdrv_addr_trans_get_multi_path_addr(int index, u64 mem_id, u64 offset, bool linear_valid, u64 *trans_addr)
{
    if (index != UBDRV_IDX_INVALID) {
        if (ubdrv_addr_trans_calc_nonlinear_addr((u64)index, offset, trans_addr) != 0) {
            ubdrv_warn(
                "This mem_id is nonlinear, but offset has exceed limit len for nonlinear. (mem_id=%llu; offset=0x%llx; "
                "pa_size=0x%llx)\n",
                mem_id, offset, g_decoder_info.decoder_info->nonlinear_pa_info[index].pa_size);
            return -EINVAL;
        }
    } else if (linear_valid) {
        if (ubdrv_addr_trans_calc_linear_addr(mem_id, offset, trans_addr) != 0) {
            ubdrv_warn("This mem_id is linear, but offset has exceed limit len for linear. (mem_id=%llu; "
                       "offset=0x%llx; share_size=0x%llx; pa_size=0x%llx)\n",
                       mem_id, offset, g_decoder_info.decoder_info->max_share_mem_size,
                       g_decoder_info.decoder_info->linear_pa_size);
            return -EINVAL;
        }
    } else {
        ubdrv_warn("This mem_id does not exist in the mem decoder. (mem_id=%llu; offset=0x%llx; linear_valid=%d)\n",
                   mem_id, offset, (int)linear_valid);
        return -EINVAL;
    }
    return 0;
}

STATIC void ubdrv_addr_trans_print_decoder_dfx(int one_idx, int multi_idx)
{
    struct ubmem_nonlinear_pa_info *info = NULL;

    ubdrv_info("Decoder linear info. (linear_pa_size=0x%llx; pa_share_size=0x%llx)\n",
               g_decoder_info.decoder_info->linear_pa_size, g_decoder_info.decoder_info->max_share_mem_size);
    if (one_idx != UBDRV_IDX_INVALID) {
        info = &g_decoder_info.decoder_info->nonlinear_pa_info[one_idx];
        ubdrv_info("Decoder info. (one_idx=%d; mem_id=%llu; to_host=%llu; linear_valid=%llu; onepath=%llu; "
                   "pa_size=0x%llx)\n",
                   one_idx, (u64)info->remote_id, (u64)info->to_host, (u64)info->linear_pa_valid, (u64)info->onepath,
                   info->pa_size);
    } else {
        ubdrv_info("One_path nonlinear decoder info does not exist.\n");
    }
    if (multi_idx != UBDRV_IDX_INVALID) {
        info = &g_decoder_info.decoder_info->nonlinear_pa_info[multi_idx];
        ubdrv_info("Decoder info. (multi_idx=%d; mem_id=%llu; to_host=%llu; linear_valid=%llu; onepath=%llu; "
                   "pa_size=0x%llx)\n",
                   multi_idx, (u64)info->remote_id, (u64)info->to_host, (u64)info->linear_pa_valid, (u64)info->onepath,
                   info->pa_size);
    } else {
        ubdrv_info("Multi_path nonlinear decoder info does not exist.\n");
    }
}
int ubdrv_addr_trans_v1(u64 to_host, u64 mem_id, u32 mode, u64 offset, u64 *trans_addr)
{
    enum ubdrv_trans_result_type result;
    int one_idx, multi_idx;
    bool linear_valid;
    int ret = 0;

    ubdrv_addr_trans_get_nonlinear_index_v1(to_host, mem_id, &one_idx, &multi_idx, &linear_valid);
    if (mode == DEVDRV_ADDR_DESC_MODE_ONEPATH) {
        result = UBDRV_TRANS_RESULT_ONEPATH;
        ret = ubdrv_addr_trans_get_one_path_addr(one_idx, offset, mode, trans_addr);
        if (ret != 0) {
            ubdrv_warn("Trans one_path addr unsuccess.(one_idx=%d; mem_id=%llu; to_host=%llu; mode=%u)\n", one_idx,
                       mem_id, to_host, mode);
            ret = -EINVAL;
        }
    } else if (mode == DEVDRV_ADDR_DESC_MODE_MULTIPATH) {
        /* one_idx/multi_idx are never modified by the helpers, judge before the call. */
        result = (multi_idx != UBDRV_IDX_INVALID) ? UBDRV_TRANS_RESULT_MULTIPATH : UBDRV_TRANS_RESULT_LINEAR;
        ret = ubdrv_addr_trans_get_multi_path_addr(multi_idx, mem_id, offset, linear_valid, trans_addr);
        if (ret != 0) {
            ubdrv_warn("Trans multi_path addr unsuccess.(multi_idx=%d; mem_id=%llu; to_host=%llu; mode=%u;"
                       "linear_valid=%d)\n",
                       multi_idx, mem_id, to_host, mode, (int)linear_valid);
            ret = -EINVAL;
        }
    } else {
        if (one_idx != UBDRV_IDX_INVALID) {
            result = UBDRV_TRANS_RESULT_ONEPATH;
            ret = ubdrv_addr_trans_get_one_path_addr(one_idx, offset, mode, trans_addr);
            if (ret != 0) {
                ubdrv_warn("Trans default_path addr in one_path addr unsuccess.(one_idx=%d; mem_id=%llu; "
                           "to_host=%llu; mode=%u)\n",
                           one_idx, mem_id, to_host, mode);
                ret = -EINVAL;
            }
        } else {
            result = (multi_idx != UBDRV_IDX_INVALID) ? UBDRV_TRANS_RESULT_MULTIPATH : UBDRV_TRANS_RESULT_LINEAR;
            ret = ubdrv_addr_trans_get_multi_path_addr(multi_idx, mem_id, offset, linear_valid, trans_addr);
            if (ret != 0) {
                ubdrv_warn(
                    "Trans default_path addr in multi_path unsuccess.(multi_idx=%d; mem_id=%llu; to_host=%llu; mode=%u;"
                    "linear_valid=%d)\n",
                    multi_idx, mem_id, to_host, mode, (int)linear_valid);
                ret = -EINVAL;
            }
        }
    }
    /* Count at the single exit: result counters on success, fail counter on error. */
    ka_base_atomic_inc(&g_decoder_info.trans_stats.result_cnt[(ret == 0) ? result : UBDRV_TRANS_RESULT_FAIL]);
    if (ret != 0) {
        ubdrv_addr_trans_print_decoder_dfx(one_idx, multi_idx);
    }
    return ret;
}

STATIC int ubdrv_addr_trans_get_nonlinear_index_v0(u64 to_host, u64 mem_id, int *index)
{
    struct ubmem_nonlinear_pa_info *info = NULL;
    u64 i;

    for (i = 0; i < g_decoder_info.decoder_info->total_nonlinear_pa_num; i++) {
        info = &g_decoder_info.decoder_info->nonlinear_pa_info[i];
        if ((info->to_host == to_host) && (info->remote_id == mem_id)) {
            *index = (int)i;
            return 0;
        }
    }

    return -ENOENT;
}

int ubdrv_addr_trans_v0(u64 to_host, u64 mem_id, u32 mode, u64 offset, u64 *trans_addr)
{
    enum ubdrv_trans_result_type result;
    u64 pa_addr_size = 0, pa_base = 0, addr_tmp = 0, pa_share_size = 0;
    int index, ret;

    ret = ubdrv_addr_trans_get_nonlinear_index_v0(to_host, mem_id, &index);
    if (ret == 0) {
        result = UBDRV_TRANS_RESULT_ONEPATH;
        pa_addr_size = g_decoder_info.decoder_info->nonlinear_pa_info[index].pa_size;
        if (offset >= pa_addr_size) {
            ubdrv_warn("This mem_id is nonlinear, but offset has exceed limit len for nonlinear. (mem_id=%llu; "
                       "offset=0x%llx; mode=%u; pa_size=0x%llx)\n",
                       mem_id, offset, mode, pa_addr_size);
            ret = -EINVAL;
            goto trans_failed;
        }
        pa_base = g_decoder_info.decoder_info->nonlinear_pa_info[index].pa_base;
        addr_tmp = pa_base + offset;
    } else {
        if (to_host != 0) {
            ubdrv_warn("This mem_id does not exist in the d2h memory. (mem_id=%llu; offset=0x%llx; mode=%u)\n", mem_id,
                       offset, mode);
            ret = -EINVAL;
            goto trans_failed;
        } else if (ubdrv_addr_trans_is_H2D()) {
            ubdrv_warn("This H2D mem_id does not exist in the nonlinear decoder. (mem_id=%llu; offset=0x%llx; "
                       "mode=%u)\n",
                       mem_id, offset, mode);
            ret = -EINVAL;
            goto trans_failed;
        }
        result = UBDRV_TRANS_RESULT_LINEAR;
        pa_base = g_decoder_info.decoder_info->linear_pa_base;
        pa_addr_size = g_decoder_info.decoder_info->linear_pa_size;
        pa_share_size = g_decoder_info.decoder_info->max_share_mem_size;
        if (offset >= pa_share_size) {
            ubdrv_warn("This mem_id is linear, but offset has exceed limit len for linear. (mem_id=%llu; "
                       "offset=0x%llx; mode=%u; addr_size=0x%llx; share_size=0x%llx)\n",
                       mem_id, offset, mode, pa_addr_size, pa_share_size);
            ret = -EINVAL;
            goto trans_failed;
        }
        addr_tmp = pa_base + mem_id * pa_share_size + offset;
    }
    if (!ubdrv_addr_trans_valid_check(pa_base, pa_addr_size, addr_tmp)) {
        ubdrv_warn("Addr trans exceed max addr size. (mem_id=%llu; offset=0x%llx; mode=%u; share_size=0x%llx; "
                   "pa_size=0x%llx)\n",
                   mem_id, offset, mode, pa_share_size, pa_addr_size);
        ret = -EINVAL;
        goto trans_failed;
    }
    *trans_addr = addr_tmp;
    ka_base_atomic_inc(&g_decoder_info.trans_stats.result_cnt[result]);
    return 0;

trans_failed:
    ka_base_atomic_inc(&g_decoder_info.trans_stats.result_cnt[UBDRV_TRANS_RESULT_FAIL]);
    return ret;
}

int ubdrv_addr_trans_pre_check(u32 udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr)
{
    if (udevid >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid udevid. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if ((addr_desc == NULL) || (trans_addr == NULL)) {
        ubdrv_err("Invalid param. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if ((addr_desc->peer_ub.mode < DEVDRV_ADDR_DESC_MODE_DEFAULT) ||
        (addr_desc->peer_ub.mode > DEVDRV_ADDR_DESC_MODE_MULTIPATH)) {
        ubdrv_err("Invalid addr trans mode. (udevid=%u; mode=%d)\n", udevid, addr_desc->peer_ub.mode);
        return -EINVAL;
    }

    if ((g_decoder_info.valid != ASCEND_UB_VALID) || (g_decoder_info.decoder_info == NULL)) {
        return -ENOENT;
    }

    return 0;
}

int ubdrv_addr_trans_version_check(u32 udevid, struct devdrv_addr_desc *addr_desc)
{
    if ((g_decoder_info.decoder_info->version == UBMEM_DECODER_MSG_VERSION_ORIGIN) &&
        (addr_desc->peer_ub.mode != DEVDRV_ADDR_DESC_MODE_DEFAULT)) {
        ubdrv_warn("Current ubmem version does not support this mode. (udevid=%u; version=%llu; mode=%d)\n", udevid,
                   (u64)g_decoder_info.decoder_info->version, addr_desc->peer_ub.mode);
        return -EINVAL;
    }
    if (g_decoder_info.decoder_info->version >= UBMEM_DECODER_MSG_VERSION_INVALID) {
        ubdrv_warn("Unsupported version. (udevid=%u; version=%u; mode=%d)\n", udevid,
                   g_decoder_info.decoder_info->version, addr_desc->peer_ub.mode);
        return -ENOTSUPP;
    }
    return 0;
}