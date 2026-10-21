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
#ifndef TRS_UB_INIT_COMMON_H
#define TRS_UB_INIT_COMMON_H
#include "ka_common_pub.h"
#include "ka_system_pub.h"

#include "trs_pub_def.h"
#include "pbl_kref_safe.h"
#include "uba/ubcore_adapt.h"

#define TRS_UB_PRIORITY_HIGH 10

#define TRS_UB_HOST_CQ_MAX 2048U
#define TRS_UB_HOST_SQ_MAX 2048U

#define TRS_UB_SHARE_JFR 1U
#define TRS_UB_JFR_RNR_TIME 10U
#define TRS_UB_JFR_MAX_SGE 1U
#define TRS_UB_JETTY_RNR_RETRY 7U
#define TRS_UB_JETTY_ERR_TIMEOUT 17U // 0-7: 128ms, 8-15:1s, 16-23:8s, 24-31:64s

#define TRS_UB_CQ_JFC_CR_EXPERT_NUM 1024
#define TRS_UB_DEFAULT_KEY_POLICY 0U
#define TRS_UB_DEFAULT_NON_PIN 1U

#define TRS_UB_SQ_HEAD_BUFFER_SIZE 2U
#define TRS_UB_SQ_TAIL_BUFFER_SIZE 2U
#define TRS_UB_SQ_BUFFER_SIZE (TRS_UB_SQ_HEAD_BUFFER_SIZE + TRS_UB_SQ_TAIL_BUFFER_SIZE)
#define TRS_UB_SQ_SEG_SIZE (TRS_UB_SQ_BUFFER_SIZE * TRS_UB_HOST_SQ_MAX)
#define TRS_UB_REG_SEG_SIZE (5 * 4096U) /* one page for (cnt_)notify, four pages for sq head/tail */
#define TRS_UB_REG_SEG_NOTIFY_OFFSET 0U
#define TRS_UB_REG_SEG_CNT_NOTIFY_OFFSET 2048U
#define TRS_UB_REG_SEG_SQ_OFFSET 4096U

struct trs_ub_seg {
    size_t seg_size;
    ka_page_t *seg_pages;
    uba_ubcore_target_seg *target_seg;
};

struct trs_ub_cq_ctx {
    void *cq_addr;
    u32 cq_depth;
    u32 cqe_size;
    u32 cq_head;
    u32 cq_tail;
    u32 loop;
    bool is_valid;
};

#define TRS_UB_UNINIT_FLAG 0U
#define TRS_UB_INIT_FLAG 1U

struct trs_jetty_info {
    u32 devid;
    u32 status;
    uba_ubcore_jfc *cq_jfc;
    uba_ubcore_jfc *sq_jfc;
#ifdef TRS_HOST
    uba_ubcore_jfr *cq_jfr;
    uba_ubcore_jfr *sq_jfr;
    struct trs_ub_seg cq_seg;
    uba_ubcore_cr cr[TRS_UB_CQ_JFC_CR_EXPERT_NUM];
    ka_tasklet_struct_t cq_dispatch_task;
    char cq_id_bucket[TRS_UB_HOST_CQ_MAX];
    u32 cq_id[TRS_UB_HOST_CQ_MAX];
    u32 cq_num;
#else
    uba_ubcore_jfs *cq_jfs;
    uba_ubcore_jfs *sq_jfs;
    uba_ubcore_tjetty *cq_tjetty;
    uba_ubcore_tjetty *sq_tjetty;
    ka_page_t *sq_base_page;
    ka_page_t *cq_base_page;
#endif
};

struct trs_ub_dev {
    u32 devid;
    u32 vfid;
    u32 sec_vfid; /* one vnpu supports two vf */
    u32 die_id;   /* u die id*/
    u32 func_id;
    u32 token_val;
    struct kref_safe ref;
    uba_ubcore_device *ubc_dev;
    uba_ubcore_eid_info eid_info;
    struct trs_jetty_info jetty_info[TRS_VF_MAX_NUM];
    uba_ubcore_target_seg *notify_tseg;
    uba_ubcore_target_seg *cnt_notify_tseg;
    uba_ubcore_target_seg *sq_reg_tseg;
#ifdef TRS_HOST
    struct trs_ub_seg sq_seg;
    struct trs_ub_cq_ctx cq_ctx[TRS_UB_HOST_CQ_MAX];
    ka_rwlock_t rw_lock;
    struct trs_ub_seg reg_seg;
    ka_rwlock_t reg_rw_lock;
    void *cq_irq_para;
    int (*cq_irq_handler)(int irq_type, int irq_index, void *para, u32 cqid[], u32 cq_num);
#else
    u32 db_offset_len;
    uintptr_t db_base_addr;
    void *urpc_cqe_addr;
    void *notify_kva;
    void *cnt_notify_kva;
    void *sq_reg_kva;
    u32 sq_jetty_id;
    u32 cq_jetty_id;
#endif
};

int trs_ub_dev_init(u32 ts_inst_id);
void trs_ub_dev_uninit(u32 ts_inst_id);
#ifdef TRS_HOST
int trs_ub_dev_init_with_group(u32 devid, u32 vfid);
void trs_ub_dev_uninit_with_group(u32 devid, u32 vfid);
#else
int trs_device_add_group(struct trs_id_inst *inst, u32 grp_id);
#endif

struct trs_ub_dev *trs_get_ub_dev(u32 devid);
void trs_put_ub_dev(struct trs_ub_dev *ub_dev);
uba_ubcore_device *trs_ub_get_ubcore_dev(u32 dev_id);

int trs_ub_dev_adapt_init(struct trs_ub_dev *ub_dev, u32 vfid);
void trs_ub_dev_adapt_uninit(struct trs_ub_dev *ub_dev, u32 vfid);

int trs_ub_sq_jetty_init(struct trs_ub_dev *ub_dev, u32 vfid);
void trs_ub_sq_jetty_uninit(struct trs_ub_dev *ub_dev, u32 vfid);
int trs_ub_cq_jetty_init(struct trs_ub_dev *ub_dev, u32 vfid);
void trs_ub_cq_jetty_uninit(struct trs_ub_dev *ub_dev, u32 vfid);

int trs_ub_create_jetty(struct trs_ub_dev *ub_dev, u32 vfid);
void trs_ub_destroy_jetty(struct trs_ub_dev *ub_dev, u32 vfid);

int trs_ub_stars_soc_res_ctrl(struct trs_id_inst *inst, u32 type, u32 id, u32 cmd);

#endif /* TRS_UB_INIT_COMMON_H */
