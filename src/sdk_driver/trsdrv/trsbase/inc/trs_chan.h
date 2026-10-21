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

#ifndef TRS_CHAN_H
#define TRS_CHAN_H

#include "ka_task_pub.h"
#include "ka_memory_pub.h"

#include "pbl_ka_mem_query.h"
#include "trs_pub_def.h"
#include "ascend_kernel_hal.h"
#include "trs_abnormal_info.h"
#include "trs_id.h"
#include "trs_adapt.h"

#define SQE_CACHE_SIZE 256
#define SQE_ALIGN_SIZE 8
#define CQE_CACHE_SIZE 128
#define CQE_ALIGN_SIZE 4

static const char *trs_chan_type_name[CHAN_TYPE_MAX][CHAN_SUB_TYPE_MAX] = {
    {"rts", "esched", "dvpp", "ts", "rsv_ts"},
    {"logic", "shm", "unknown", "unknown", "unknown"},
    {"log", "prof", "hb", "dbg", "unknown"},
    {"cb", "scb", "unknown", "unknown", "unknown"},
};

static inline const char *trs_chan_type_to_name(struct trs_chan_type *types)
{
    if ((types->type < CHAN_TYPE_MAX) && (types->sub_type < CHAN_SUB_TYPE_MAX)) {
        return trs_chan_type_name[types->type][types->sub_type];
    }
    return "unknown";
}

int trs_chan_send(struct trs_id_inst *inst, int chan_id, struct trs_chan_send_para *para);
int trs_chan_recv(struct trs_id_inst *inst, int chan_id, struct trs_chan_recv_para *para);

#define CHAN_CTRL_CMD_SQ_HEAD_UPDATE 0       /* para: sq head, update sw sq head */
#define CHAN_CTRL_CMD_SQ_HEAD_SET 1          /* para: sq head, set hw sq head */
#define CHAN_CTRL_CMD_SQ_STATUS_SET 2        /* para: 0: disable, 1: enable */
#define CHAN_CTRL_CMD_SQ_DISABLE_TO_ENABLE 3 /* para: timeout */
#define CHAN_CTRL_CMD_CQ_SCHED 4             /* no para */
#define CHAN_CTRL_CMD_NOT_NOTICE_TS 5        /* no para */
#define CHAN_CTRL_CMD_SQCQ_RESET 6           /* no para */
#define CHAN_CTRL_CMD_CQ_PAUSE 7             /* para: cqid */
#define CHAN_CTRL_CMD_CQ_RESUME 8            /* para: cqid */
#define CHAN_CTRL_CMD_SQ_TAIL_SET 9          /* para: sq tail, set hw sq tail */
int trs_chan_ctrl(struct trs_id_inst *inst, int chan_id, u32 cmd, u32 para);

#define CHAN_QUERY_CMD_SQ_STATUS 0 /* value: sq status 0: disable, 1: enable */
#define CHAN_QUERY_CMD_SQ_HEAD 1   /* value: sq head */
#define CHAN_QUERY_CMD_SQ_TAIL 2   /* value: sq tail */
#define CHAN_QUERY_CMD_SQ_POS 3    /* value: sq pos */
#define CHAN_QUERY_CMD_CQ_HEAD 4   /* value: cq head */

int trs_chan_query(struct trs_id_inst *inst, int chan_id, u32 cmd, u32 *value);

struct trs_chan_sq_info {
    u32 sqid;
    void *sq_vaddr;                             /* contiguous kva */
    struct ka_vmap_pfn_handle *vmap_pfn_handle; /* vmap_pfn handle for (< 5.10 fallback) */
    union {
        u64 sq_phy_addr;
        struct ka_pa_wraper *sq_pa_wraper_list;
    };
    void *sq_dev_vaddr;
    u64 head_addr;
    u64 tail_addr;
    u64 db_addr;
    u32 mem_type;
    u32 addr_num;
    u32 addr_offset;
    struct trs_chan_sq_para sq_para;
};

struct trs_chan_cq_info {
    u32 cqid;
    int irq;
    void *cq_vaddr;
    u64 cq_phy_addr;
    u32 addr_num;
    struct trs_chan_cq_para cq_para;
};

bool trs_chan_get_virt_flag_test(struct trs_id_inst *inst); /* used for simulation with virtual address */
int trs_chan_get_chan_id(struct trs_id_inst *inst, int res_type, u32 res_id, int *chan_id);
int trs_chan_get_sq_info(struct trs_id_inst *inst, int chan_id, struct trs_chan_sq_info *info);
int trs_chan_get_cq_info(struct trs_id_inst *inst, int chan_id, struct trs_chan_cq_info *info);
int trs_chan_to_string(struct trs_id_inst *inst, int chan_id, char *buff, u32 buff_len);
int trs_chan_stream_task_update(struct trs_id_inst *inst, int pid, void *vaddr, u32 *long_task_cnt);
int trs_sq_switch_stream_chan_update(struct trs_id_inst *inst, u32 chan_id, u32 sq_depth);

struct trs_chan_info {
    u32 op; /* 1 add, 0 del */
    u16 ssid;
    u8 master_pid_flag : 1;
    u8 no_cq_mem_flag : 1;
    u8 remote_id_flag : 1;
    u8 agent_id_flag : 1;
    u8 sq_addr_is_virtual : 1;
    u8 rsv : 3;
    ka_pid_t pid;
    int irq_type;
    struct trs_chan_type types;
    struct trs_chan_sq_info sq_info;
    struct trs_chan_cq_info cq_info;
    u32 msg[SQCQ_INFO_LENGTH]; /* send to ts */
    void *ext_msg;             /* send to ts */
    u32 ext_msg_len;
};

struct trs_sq_mem_map_para {
    struct trs_chan_type chan_types;
    struct trs_chan_sq_para sq_para;
    u64 sq_phy_addr;
    ka_pid_t host_pid;
    u32 mem_type;
};

#define CTRL_CMD_SQ_HEAD_UPDATE 0       /* para: sq head */
#define CTRL_CMD_SQ_TAIL_UPDATE 1       /* para: sq tail */
#define CTRL_CMD_CQ_HEAD_UPDATE 2       /* para: cq head */
#define CTRL_CMD_SQ_STATUS_SET 3        /* para: 0: disable, 1: enable */
#define CTRL_CMD_SQ_DISABLE_TO_ENABLE 4 /* para: timeout */
#define CTRL_CMD_CQ_RESET 5             /* no para */
#define CTRL_CMD_CQ_PAUSE 6             /* para: cqid */
#define CTRL_CMD_CQ_RESUME 7            /* para: cqid */
#define CTRL_CMD_SQ_RESET 8             /* no para */
#define CTRL_CMD_SQCQ_MAX 9

#define QUERY_CMD_SQ_HEAD 0       /* value: head */
#define QUERY_CMD_SQ_TAIL 1       /* value: tail */
#define QUERY_CMD_CQ_HEAD 2       /* value: head */
#define QUERY_CMD_CQ_TAIL 3       /* value: tail */
#define QUERY_CMD_SQ_STATUS 4     /* value: sq status */
#define QUERY_CMD_SQ_HEAD_PADDR 5 /* value: head head phy addr, for uio */
#define QUERY_CMD_SQ_TAIL_PADDR 6 /* value: head tail phy addr, for uio */
#define QUERY_CMD_SQ_DB_PADDR 7   /* value: sq db phy addr, for uio */
#define QUERY_CMD_STARS_SQ_HEAD 8 /* value: head */
#define QUERY_CMD_STARS_SQ_TAIL 9 /* value: tail */
#define QUERY_CMD_SQCQ_MAX 10

#define TRS_CHAN_DEV_RSV_MEM 0U /* dev rsv mem */
#define TRS_CHAN_HOST_MEM 1U
#define TRS_CHAN_HOST_PHY_MEM 2U
#define TRS_CHAN_DEV_MEM_PRI 3U /* dev mem pri, others alloc host mem, not for mem_attr */
#define TRS_CHAN_DEV_SVM_MEM 4U /* svm dev mem, alloced in user space */
#define TRS_CHAN_MEM_LOCAL (0x1U << 31)
#define TRS_CHAN_MEM_TYPE_LOCAL_MASK 0x80000000U
#define TRS_CHAN_MEM_TYPE_SIDE_MASK 0x7FFFFFFFU

struct trs_chan_mem_attr {
    u64 size;
    u32 mem_type;
    int tgid;
    void *specified_uva;
    u64 phy_addr;                               /* physical addr (used when pa_num == 1) */
    struct ka_vmap_pfn_handle *vmap_pfn_handle; /* mapping handle (SVM: vmap_pfn_create; device DDR: ka_mm_vmap via
                                                   handle->kva; others: NULL) */
    struct ka_pa_wraper *pa_wraper_list;        /* valid for SVM/ device DDR (vzalloc) */
    u32 pa_num;
    struct ka_mem_attr ka_attr;
};

static inline bool trs_chan_mem_is_local_mem(u32 mem_type)
{
    return ((mem_type & TRS_CHAN_MEM_TYPE_LOCAL_MASK) != 0);
}

static inline bool trs_chan_mem_is_dev_mem(u32 mem_type)
{
    u32 type = (mem_type & TRS_CHAN_MEM_TYPE_SIDE_MASK);
    return ((type == TRS_CHAN_DEV_RSV_MEM) || (type == TRS_CHAN_DEV_SVM_MEM));
}

struct trs_chan_dma_desc {
    uint32_t tsid;               /* input */
    drvSqCqType_t type;          /* input */
    void *src;                   /* input */
    uint32_t sq_id;              /* input */
    uint32_t sqe_pos;            /* input */
    uint32_t len;                /* input */
    uint32_t dir;                /* input */
    unsigned long long dma_base; /* output */
    unsigned int dma_node_num;   /* output */
};

struct trs_chan_adapt_ops {
    ka_module_t *owner;
    void *(*sq_mem_alloc)(struct trs_id_inst *inst, struct trs_chan_type *types, struct trs_chan_sq_para *sq_para,
                          struct trs_chan_mem_attr *mem_attr);
    void (*sq_mem_free)(struct trs_id_inst *inst, struct trs_chan_type *types, void *sq_addr,
                        struct trs_chan_mem_attr *mem_attr);
    void *(*cq_mem_alloc)(struct trs_id_inst *inst, struct trs_chan_type *types, struct trs_chan_cq_para *cq_para,
                          struct trs_chan_mem_attr *mem_attr);
    void (*cq_mem_free)(struct trs_id_inst *inst, struct trs_chan_type *types, void *cq_addr,
                        struct trs_chan_mem_attr *mem_attr);

    bool (*is_current_proc_sq)(struct trs_id_inst *inst, u32 sqid); /* not must */
    bool (*is_current_proc_cq)(struct trs_id_inst *inst, u32 cqid); /* not must */

    int (*sqcq_speified_id_alloc)(struct trs_id_inst *inst, int type, u32 flag, u32 *id, void *para);
    int (*sqcq_speified_id_free)(struct trs_id_inst *inst, int type, u32 flag, u32 id);

    void (*flush_cache)(struct trs_id_inst *inst, u32 mem_type, void *addr, u64 pa, u32 len);
    void (*invalid_cache)(struct trs_id_inst *inst, u32 mem_type, void *addr, u64 pa, u32 len);

    bool (*cqe_is_valid)(struct trs_id_inst *inst, void *cqe, u32 loop);
    void (*get_sq_head_in_cqe)(struct trs_id_inst *inst, void *cqe, u32 *sq_head);
    int (*sqe_update)(struct trs_id_inst *inst, struct trs_sqe_update_info *update_info); /* not must */
    int (*cqe_update)(struct trs_id_inst *inst, int pid, u32 cqid, void *cqe);            /* not must */
    int (*sqcq_ctrl)(struct trs_id_inst *inst, struct trs_chan_type *types, u32 id, u32 cmd, u32 para);
    int (*sqcq_query)(struct trs_id_inst *inst, struct trs_chan_type *types, u32 id, u32 cmd, u64 *value);
    int (*notice_ts)(struct trs_id_inst *inst, struct trs_chan_info *chan_info);

    int (*get_irq)(struct trs_id_inst *inst, u32 irq_type, u32 irq[], u32 irq_num, u32 *valid_irq_num);
    int (*get_cq_affinity_irq)(struct trs_id_inst *inst, u32 cq_id, u32 *irq_index);
    int (*request_irq)(struct trs_id_inst *inst, u32 irq_type, int irq_index, void *para,
                       int (*handler)(int irq_type, int irq_index, void *para, u32 cqid[], u32 cq_num));
    int (*free_irq)(struct trs_id_inst *inst, int irq_type, int irq_index, void *para);
    int (*sq_mem_map)(struct trs_id_inst *inst, struct trs_sq_mem_map_para *para, void **sq_dev_vaddr);
    int (*sq_mem_unmap)(struct trs_id_inst *inst, struct trs_sq_mem_map_para *para, void *sq_dev_vaddr);
    bool (*cq_need_resched)(struct trs_id_inst *inst, struct trs_chan_type *types);
    int (*sq_dma_desc_create)(struct trs_id_inst *inst, struct trs_chan_dma_desc *para);
    void (*sq_dma_desc_destroy)(struct trs_id_inst *inst, u32 sqid);
    int (*sqcq_rts_rsv_id_alloc)(struct trs_id_inst *inst, int type, u32 *id);
};

int trs_chan_ts_inst_register(struct trs_id_inst *inst, int hw_type, int location, struct trs_chan_adapt_ops *ops);
void trs_chan_ts_inst_unregister(struct trs_id_inst *inst);

typedef int (*trs_chan_abnormal_handle)(struct trs_id_inst *inst, u32 sqid, void *sqe, void *info);
int trs_chan_register_abnormal_handle(u32 task_type, trs_chan_abnormal_handle handle);
int trs_chan_unregister_abnormal_handle(u32 task_type);

int trs_chan_abnormal_proc(struct trs_id_inst *inst, struct stars_abnormal_info *abnormal_info);

void hal_kernel_trs_chan_destroy_ex(struct trs_id_inst *inst, u32 flag, int chan_id);

int trs_chan_init_module(void);
void trs_chan_exit_module(void);
int trs_chan_dma_desc_create(struct trs_id_inst *inst, int chan_id, struct trs_chan_dma_desc *para);
int trs_chan_dma_desc_destroy(struct trs_id_inst *inst, int chan_id);

#endif
