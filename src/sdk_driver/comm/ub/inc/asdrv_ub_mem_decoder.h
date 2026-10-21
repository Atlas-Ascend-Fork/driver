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
#ifndef ASDRV_UB_MEM_DECODER_H
#define ASDRV_UB_MEM_DECODER_H

#include "asdrv_ub_main_adapt.h"

#define ASCEND_UB_MEM_NONLINEAR_NUM_V0 36u
#define UB_MEM_KEY_UB_BASE "UBA_BASE"
#define UB_MEM_KEY_UB_TID "UB_TID"
#define UB_MEM_KEY_UB_MEM_ID "UB_MEM_ID"
#define UB_MEM_SINGLE_QUERY_NUM 32
#define UB_MEM_ONEPATH_MODE 1
#define UB_MEM_MULTIPATH_MODE 0
#define UBMEM_DECODER_MSG_VERSION_ORIGIN 0U
#define UBMEM_DECODER_MSG_VERSION_ONEPATH 1U
#define UBMEM_DECODER_MSG_VERSION_INVALID 2U

struct ubdrv_ubmem_set_decoder_info {
    u32 dev_id;
    u64 uba;
    u64 max_share_mem_size;
    u64 tid;
    u64 mem_id;
};

struct ubmem_communication_info_req_data {
    u64 nonlinear_pa_start_idx : 16;
    u64 nonlinear_pa_num : 8;
    u64 rsv : 40;
} __attribute__((packed));

struct ubmem_nonlinear_pa_info {
    u64 remote_id : 16;
    u64 to_host : 1;
    u64 linear_pa_valid : 1;
    u64 onepath : 1;
    u64 rsv : 45;
    u64 pa_base;
    u64 pa_size;
};

struct ubmem_decoder_msg_data {
    u32 version;
    u16 super_pod_size; // npu pod size
    u16 mem_id;         // only use in ub mem_decoder info
    u32 tid;
    u64 max_share_mem_size;
    u64 uba;
    u64 linear_pa_base;
    u64 linear_pa_size;
    u64 nonlinear_pa_num : 8;
    u64 total_nonlinear_pa_num : 16;
    u64 eof : 1;
    u64 rsv : 39;
    struct ubmem_nonlinear_pa_info nonlinear_pa_info[];
} __attribute__((packed));

/*
 * Request path type from the caller, values are aligned with
 * DEVDRV_ADDR_DESC_MODE_{DEFAULT, ONEPATH, MULTIPATH} (0/1/2) defined in addr_trans.h.
 */
enum ubdrv_trans_path_type {
    UBDRV_TRANS_PATH_DEFAULT = 0,
    UBDRV_TRANS_PATH_ONEPATH,
    UBDRV_TRANS_PATH_MULTIPATH,
    UBDRV_TRANS_PATH_TYPE_MAX
};

/* Actual translation result type. */
enum ubdrv_trans_result_type {
    UBDRV_TRANS_RESULT_ONEPATH = 0, /* translated via nonlinear onepath entry (v0 nonlinear included) */
    UBDRV_TRANS_RESULT_MULTIPATH,   /* translated via nonlinear multipath entry */
    UBDRV_TRANS_RESULT_LINEAR,      /* translated via linear formula */
    UBDRV_TRANS_RESULT_FAIL,        /* translation failed */
    UBDRV_TRANS_RESULT_TYPE_MAX
};

struct ubdrv_trans_stats {
    ka_atomic_t req_cnt[UBDRV_TRANS_PATH_TYPE_MAX];
    ka_atomic_t result_cnt[UBDRV_TRANS_RESULT_TYPE_MAX];
};

/* Query statistics of the mem decoder info. */
struct ubdrv_query_stats {
    ka_atomic_t update_cnt;      /* config updates applied to decoder_info (first apply included) */
    ka_atomic_t uda_trigger_cnt; /* queries triggered by the uda notifier */
    ka_atomic_t ubcore_fail_cnt; /* queries failed at ubcore_user_control */
    ka_atomic_t other_fail_cnt;  /* queries failed at other stages */
};

struct ubdrv_ubmem_decoder_info {
    ka_rw_semaphore_t rw_sem;
    struct ubdrv_query_stats query_stats;
    struct ubdrv_trans_stats trans_stats; /* translation request/result statistics */
    u32 valid;
    struct ubmem_decoder_msg_data *decoder_info;
};

struct ubmem_host_cfg_msg_data {
    u32 version;
    u16 mem_id;
    u16 rsv;
    u64 uba;
    u64 max_share_mem_size;
    u32 tid;
    u32 rsv1;
} __attribute__((packed));

struct ubdrv_ubmem_host_cfg_info {
    ka_rw_semaphore_t rw_sem;
    ka_atomic_t update_cnt; /* count of config updates applied to cfg_info */
    u32 valid;
    struct ubmem_host_cfg_msg_data *cfg_info;
};

extern struct ubdrv_ubmem_decoder_info g_decoder_info;

bool ubdrv_addr_trans_is_H2D(void);
int ubdrv_set_mem_decoder_info(struct ubdrv_ubmem_set_decoder_info *info);
int ubdrv_set_device_mem_decoder_info(u32 dev_id, struct ubmem_decoder_msg_data *data);
int ubdrv_query_device_mem_decoder_info(u32 dev_id, enum ubdrv_log_level log_level);
bool ubdrv_query_addr_trans_support(enum devdrv_addr_trans_cap_type cap_type);
int ubdrv_addr_trans_v0(u64 to_host, u64 mem_id, u32 mode, u64 offset, u64 *trans_addr);
int ubdrv_addr_trans_v1(u64 to_host, u64 mem_id, u32 mode, u64 offset, u64 *trans_addr);
bool ubdrv_addr_trans_valid_check(u64 base_addr_start, u64 addr_size, u64 target_addr);
int ubdrv_addr_trans_pre_check(u32 udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr);
int ubdrv_addr_trans_version_check(u32 udevid, struct devdrv_addr_desc *addr_desc);
int ubdrv_mem_decoder_ctrl_init(void);
void ubdrv_mem_decoder_ctrl_uninit(void);
int ubdrv_procfs_mem_decoder_dfx_show(ka_seq_file_t *seq, void *offset);
int ubdrv_addr_trans_local_to_peer(u32 udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr);
#endif // ASDRV_UB_MEM_DECODER_H