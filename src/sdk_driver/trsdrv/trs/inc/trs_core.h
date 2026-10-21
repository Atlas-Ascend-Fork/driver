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

#ifndef TRS_CORE_H
#define TRS_CORE_H

#include "ka_base_pub.h"

#include "trs_pub_def.h"
#include "ascend_kernel_hal.h"
#include "trs_res_id_def.h"
#include "trs_chan.h"
#include "trs_stl_comm.h"
#include "trs_mailbox_def.h"

#define TRS_INST_ALL_FEATUR_MODE 0
#define TRS_INST_PART_FEATUR_MODE 1

/* res id ctrl cmd */
enum {
    TRS_RES_OP_RESET = 0,
    TRS_RES_OP_RECORD,
    TRS_RES_OP_ENABLE,
    TRS_RES_OP_DISABLE,
    TRS_RES_OP_CHECK_AND_RESET,
    TRS_RES_OP_MAX,
};

/* res id query cmd */
enum {
    TRS_RES_QUERY_ADDR = 0,
    TRS_RES_QUERY_MAX,
};

/* set ts_inst_feature_mode force level */
enum {
    TRS_SET_TS_INST_MODE_FORCE_LEVEL_NONE = 0, /* check proc_list and exit proc_list */
    TRS_SET_TS_INST_MODE_FORCE_LEVEL_PART,     /* check proc_list */
    TRS_SET_TS_INST_MODE_FORCE_LEVEL_ALL,      /* ignore if any proc exist */
};

/* stars scenario define soft logic cqe */
struct trs_logic_cqe {
    u16 stream_id;
    u16 task_id;
    u32 error_code;
    u8 error_type;
    u8 sqe_type;
    u16 sq_id;
    u16 sq_head;
    u16 match_flag : 1;
    u16 drop_flag : 1;
    u16 error_bit : 1;
    u16 acc_error : 1;
    u16 next_task_vld : 1;
    u16 notify_attached : 1;
    u16 reserved0 : 10;
    union {
        u64 timestamp;
        u16 sqe_index;
        struct {
            u16 jettyid0;
            u16 jettyid1;
            uint8_t status0;
            uint8_t sub_status0;
            uint8_t status1;
            uint8_t sub_status1;
        };
    };
    /* Union description:
     *  Internal: enque_timestamp temporarily used as dfx
     *  External: reserved1
     */
    union {
        u64 enque_timestamp;
        u64 reserved1;
    };
};

struct trs_sqcq_reg_map_para {
    u32 stream_id;
    u32 sqid;
    u32 cqid;
    ka_pid_t host_pid;
};

struct trs_notify_reg_map_para {
    unsigned int rudevid;
    unsigned int res_id;
    unsigned int flag;
    unsigned long va; /* output */
    unsigned int len; /* output */
    enum res_addr_type res_type;
};

struct trs_res_query_para {
    int type;
    u32 alloc_num;
    u32 res[4];
};

struct trs_res_info_query {
    union {
        struct { /* for stream */
            u32 stream_depth;
            u32 task_size;
            void *stream_base_uva;
            void *stream_base_kva;                             /* contiguous kva */
            struct ka_vmap_pfn_handle *stream_vmap_pfn_handle; /* vmap_pfn handle (< 5.10 fallback) */
            u64 stream_base_pa;
            u32 mem_type;
            u32 addr_num;
            u32 addr_offset;
        } stream_info;
    };
};

struct trs_core_adapt_ops {
    ka_module_t *owner;
    void *(*cq_mem_alloc)(struct trs_id_inst *inst, size_t size);
    void (*cq_mem_free)(struct trs_id_inst *inst, void *vaddr, size_t size);
    void *(*proc_bind_smmu)(struct trs_id_inst *inst);
    void (*proc_unbind_smmu)(void *smmu_inst, int pid);
    int (*ssid_query)(struct trs_id_inst *inst, int *user_visible_flag, int *ssid);
    int (*res_id_check)(struct trs_id_inst *inst, int type, u32 id);
    int (*res_id_remote_get)(struct trs_id_inst *inst, int type, u32 id, u32 hpid, u32 seq);
    int (*res_id_remote_put)(struct trs_id_inst *inst, int type, u32 id, u32 hpid, u32 seq);
    int (*get_res_support_proc_num)(struct trs_id_inst *inst, u32 *proc_num);                            /* not must */
    int (*id_alloc)(struct trs_id_inst *inst, int type, u32 flag, u32 *id, u32 para);                    /* not must */
    int (*id_free)(struct trs_id_inst *inst, int type, u32 flag, u32 id);                                /* not must */
    int (*get_sq_id_head_from_hw_cqe)(struct trs_id_inst *inst, void *hw_cqe, u32 *sqid, u32 *sq_head);  /* not must */
    int (*get_stream_id_from_hw_cqe)(struct trs_id_inst *inst, void *hw_cqe, u32 *stream_id);            /* not must */
    int (*hw_cqe_to_logic_cqe)(struct trs_id_inst *inst, void *hw_cqe, struct trs_logic_cqe *logic_cqe); /* not must */
    bool (*is_drop_cqe)(struct trs_id_inst *inst, struct trs_logic_cqe *logic_cqe);                      /* not must */
    int (*notice_ts)(struct trs_id_inst *inst, u8 *msg, u32 len);
    int (*sqcq_reg_map)(struct trs_id_inst *inst, struct trs_sqcq_reg_map_para *para);
    int (*sqcq_reg_unmap)(struct trs_id_inst *inst, struct trs_sqcq_reg_map_para *para);
    int (*res_id_ctrl)(struct trs_id_inst *inst, u32 type, u32 id, u32 cmd);
    int (*res_id_query)(struct trs_id_inst *inst, u32 type, u32 id, u32 cmd, u64 *value);
    int (*get_res_reg_offset)(struct trs_id_inst *inst, int type, u32 id, u32 *offset);
    int (*get_res_reg_total_size)(struct trs_id_inst *inst, int type, u32 *total_size);
    int (*get_sq_trigger_irq)(struct trs_id_inst *inst, u32 *irq, u32 *irq_type); /* not must */
    int (*get_trigger_sqid)(struct trs_id_inst *inst, u32 *sqid);                 /* not must */
    void (*set_trigger_irq_affinity)(struct trs_id_inst *inst, u32 irq, u32 op);  /* not must */
    int (*request_irq)(struct trs_id_inst *inst, u32 irq_type, u32 irq, void *para,
                       ka_irqreturn_t (*handler)(int irq, void *para));
    void (*free_irq)(struct trs_id_inst *inst, u32 irq_type, u32 irq, void *para);
    void (*set_thread_affinity)(struct trs_id_inst *inst, ka_task_struct_t *thread);
    int (*ts_rpc_call)(struct trs_id_inst *inst, u8 *msg, u32 len);
    int (*get_connect_protocol)(struct trs_id_inst *inst);
    int (*get_host_mach_flag)(struct trs_id_inst *inst, u32 *host_flag);
    /* not must (tscpu phy device set) */
    int (*get_thread_bind_irq)(struct trs_id_inst *inst, u32 irq[], u32 irq_num, u32 *valid_irq_num, u32 *irq_type);
    int (*get_ts_inst_status)(struct trs_id_inst *inst, u32 *status);
    void (*trace_sqe_fill)(struct trs_id_inst *inst, struct trs_chan_sq_trace *sq_trace, void *sqe);
    void (*trace_cqe_fill)(struct trs_id_inst *inst, struct trs_chan_cq_trace *cq_trace, void *cqe);
    /* stl interface */
    int (*stl_bind)(struct trs_id_inst *inst);
    int (*stl_launch)(struct trs_id_inst *inst, struct trs_stl_launch_para *para);
    int (*stl_query)(struct trs_id_inst *inst, struct trs_stl_query_para *para);
    int (*ub_info_query)(struct trs_id_inst *inst, u32 *die_id, u32 *func_id);
    int (*notify_reg_map)(struct trs_id_inst *inst, struct trs_notify_reg_map_para *para);
    int (*notify_reg_unmap)(struct trs_id_inst *inst, struct trs_notify_reg_map_para *para);
    bool (*proc_cp2_type_check)(void);
    int (*proc_cp2_get_shr_pid)(struct trs_id_inst *inst, u32 *share_pid);
    int (*get_sq_send_mode)(u32 devid);
    int (*ts_cmdlist_mem_map)(struct trs_id_inst *inst);
    int (*ts_cmdlist_mem_unmap)(struct trs_id_inst *inst);
    int (*ras_report)(struct trs_id_inst *inst);
    int (*mem_update)(struct trs_id_inst *inst, u64 in_addr, u64 *out_addr, int flag); /* not must, flag 0:d2h 1:h2d */
    int (*notice_proc_release)(struct trs_id_inst *inst, int pid);
    int (*res_num_query)(struct trs_id_inst *inst, struct trs_res_query_para *para);                /* not must */
    int (*get_sq_mem_handle)(struct trs_id_inst *inst, int pid, struct trs_sq_mem_handle *handle);  /* not must */
    void (*put_sq_mem_handle)(struct trs_id_inst *inst, int pid, struct trs_sq_mem_handle *handle); /* not must */
    int (*get_stars_die_id)(struct trs_id_inst *inst);                                              /* not must */
};

enum {
    TRS_CORE_SQ_TRIGGER_WQ_UNBIND_FLAG_BIT = 0,
};

#define TRS_CORE_SQ_TRIGGER_WQ_UNBIND_FLAG (1U << TRS_CORE_SQ_TRIGGER_WQ_UNBIND_FLAG_BIT)

int trs_core_ts_inst_register(struct trs_id_inst *inst, int hw_type, int location, u32 ts_inst_flag,
                              struct trs_core_adapt_ops *ops);
void trs_core_ts_inst_unregister(struct trs_id_inst *inst);

int trs_stream_bind_remote_sqcq(struct trs_id_inst *inst, u32 stream_id, u32 sqid, u32 cqid, int host_pid);
int trs_set_sq_reg_vaddr(struct trs_id_inst *inst, u32 sqid, u64 va, size_t size);
int trs_get_sq_reg_vaddr(struct trs_id_inst *inst, u32 sqid, u64 *va, size_t *size);
int trs_get_sq_bind_stream_info(struct trs_id_inst *inst, u32 sq_id, struct trs_res_info_query *res_info);

int trs_res_id_get(struct trs_id_inst *inst, int res_type, u32 res_id);
int trs_res_id_put(struct trs_id_inst *inst, int res_type, u32 res_id);
int _trs_res_id_remote_get(struct trs_id_inst *inst, int pid, int res_type, u32 res_id, u32 hpid, u32 seq);
int _trs_res_id_remote_put(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq);
int trs_res_id_ctrl(struct trs_id_inst *inst, int res_type, u32 res_id, u32 cmd);

bool trs_res_is_belong_to_proc(struct trs_id_inst *inst, int pid, int res_type, u32 res_id);

typedef bool (*trs_res_share_by_proc_ops_t)(struct trs_id_inst *inst, int pid, int res_type, u32 res_id);
void trs_res_share_proc_ops_register(int res_type, trs_res_share_by_proc_ops_t func);
int trs_set_ts_inst_feature_mode(struct trs_id_inst *inst, u32 mode, u32 force);
bool trs_check_ts_inst_has_proc(struct trs_id_inst *inst);

int trs_core_get_ssid(struct trs_id_inst *inst, int pid, u32 *passid);
int trs_res_id_check(struct trs_id_inst *inst, int res_type, u32 res_id);
int trs_res_id_remote_get(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq);
int trs_res_id_remote_put(struct trs_id_inst *inst, int res_type, u32 res_id, u32 hpid, u32 seq);
int trs_rpc_msg_ctrl(struct trs_id_inst *inst, int pid, void *msg, u32 msg_len, struct trs_rpc_call_msg *rpc_call_msg);

int trs_notify_config_with_ts(struct trs_id_inst *inst, u32 notify_id, u32 notify_type, u32 config_type);
int trs_get_stream_with_sq(struct trs_id_inst *inst, u32 sqid, u32 *stream_id);
int trs_notice_exit_proc_list_recycle(struct trs_id_inst *inst, int pid);
int trs_notice_check_proc_recycle(struct trs_id_inst *inst, int pid);

int trs_core_init_module(void);
void trs_core_exit_module(void);
#endif
