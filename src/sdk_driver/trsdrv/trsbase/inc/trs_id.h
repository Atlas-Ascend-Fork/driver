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
#ifndef TRS_ID_H
#define TRS_ID_H

#include "ka_type.h"

#include "ka_base_pub.h"
#include "ka_task_pub.h"

#include "ascend_hal_define.h"

#include "pbl/pbl_soc_res.h"
#include "trs_pub_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TRS_ID_RESERVED_BIT 0
#define TRS_ID_SPECIFIED_BIT 1
#define TRS_ID_RANGE_BIT 2
#define TRS_ID_RTS_RSV_BIT 3

static inline int trs_id_type_convert(int type)
{
    switch (type) {
        case TRS_EVENT_ID:
            return MIA_STARS_EVENT;
        case TRS_NOTIFY_ID:
            return MIA_STARS_NOTIFY;
        case TRS_CNT_NOTIFY_ID:
            return MIA_STARS_CNT_NOTIFY;
        case TRS_CMO_ID:
            return MIA_STARS_CMO;
        case TRS_HW_SQ_ID:
        case TRS_HW_CQ_ID:
        case TRS_RSV_HW_SQ_ID:
        case TRS_RSV_HW_CQ_ID:
            return MIA_STARS_RTSQ;
        case TRS_CDQM_ID:
            return MIA_STARS_CDQ;
        case TRS_STARS_H2D_JETTY_ID:
            return MIA_STARS_H2D_JETTY;
        case TRS_STARS_D2D_IN_BOARD_JETTY_ID:
            return MIA_STARS_D2D_IN_BOARD_JETTY;
        case TRS_STARS_D2D_CROSS_BOARD_JETTY_ID:
            return MIA_STARS_D2D_CROSS_BOARD_JETTY;
        default:
            return MIA_MAX_RES_TYPE;
    }
}

#define TRS_MAX_ID_BIT_NUM 16
struct trs_id_alloc_para {
    u32 flag;
    u32 vfid;
};

struct trs_id_attr {
    u32 id_start;
    u32 id_end;
    u32 id_num;
    u32 batch_num;
    u32 split;       // id larger than spli will be added to tail, otherwise add to head
    u32 isolate_num; // id num of page table isolated
};

typedef int (*trs_id_alloc_batch)(struct trs_id_inst *inst, int type, u32 flag, u32 id[], u32 *id_num);
typedef int (*trs_id_free_batch)(struct trs_id_inst *inst, int type, u32 id[], u32 id_num);
typedef int (*trs_id_trans)(struct trs_id_inst *inst, int type, u32 id, u32 *phy_id);
typedef int (*trs_id_res_avail_query)(struct trs_id_inst *inst, int type, u32 *num);
typedef bool (*trs_id_is_non_cache_type)(int type);

struct trs_id_ops {
    ka_module_t *owner;
    trs_id_alloc_batch alloc_batch;
    trs_id_free_batch free_batch;
    trs_id_res_avail_query avail_query;
    trs_id_trans trans;
    trs_id_is_non_cache_type is_non_cache_type;
};

struct trs_id_stat {
    u32 alloc;
    u32 allocatable;
    u32 rsv_num;
};

static inline bool trs_id_is_specified(u32 flag)
{
    return ((flag & (0x1 << TRS_ID_SPECIFIED_BIT)) != 0);
}

static inline bool trs_id_is_reserved(u32 flag)
{
    return ((flag & (0x1 << TRS_ID_RESERVED_BIT)) != 0);
}

static inline bool trs_id_is_ranged(u32 flag)
{
    return ((flag & (0x1 << TRS_ID_RANGE_BIT)) != 0);
}

static inline void trs_id_set_specified_flag(u32 *flag)
{
    *flag |= (0x1 << TRS_ID_SPECIFIED_BIT);
}

static inline void trs_id_set_reserved_flag(u32 *flag)
{
    *flag |= (0x1 << TRS_ID_RESERVED_BIT);
}

static inline void trs_id_set_range_flag(u32 *flag)
{
    *flag |= (0x1 << TRS_ID_RANGE_BIT);
}

static inline bool trs_id_is_rts_rsv_flag(u32 flag)
{
    return ((flag & (0x1U << TRS_ID_RTS_RSV_BIT)) != 0);
}

/* Stream Id, Event id, etc., initialized by adapt */
int trs_id_register(struct trs_id_inst *inst, int type, struct trs_id_attr *attr, struct trs_id_ops *ops);
int trs_id_unregister(struct trs_id_inst *inst, int type);

int trs_id_get_total_num(struct trs_id_inst *inst, int type, u32 *total_num);
int trs_id_get_avail_num(struct trs_id_inst *inst, int type, u32 *avail_num);
int trs_id_get_avail_num_in_pool(struct trs_id_inst *inst, int type, u32 *avail_num);
int trs_id_get_used_num(struct trs_id_inst *inst, int type, u32 *used_num);
int trs_id_get_split(struct trs_id_inst *inst, int type, u32 *split);
int trs_id_get_stat(struct trs_id_inst *inst, int type, struct trs_id_stat *stat);

int trs_id_get_max_id(struct trs_id_inst *inst, int type, u32 *max_id);

/* para only used in alloc_in_range, otherwise set 0 */
int trs_id_alloc_ex(struct trs_id_inst *inst, int type, u32 flag, u32 *id, u32 para);
int trs_id_free_ex(struct trs_id_inst *inst, int type, u32 flag, u32 id);
static inline int trs_id_alloc(struct trs_id_inst *inst, int type, u32 *id)
{
    return trs_id_alloc_ex(inst, type, 0, id, 1);
}

static inline int trs_id_free(struct trs_id_inst *inst, int type, u32 id)
{
    return trs_id_free_ex(inst, type, 0, id);
}

int trs_id_free_batch_by_type(struct trs_id_inst *inst, int type);
int trs_id_flush_to_pool(struct trs_id_inst *inst);
void trs_id_clear_reserved_flag(struct trs_id_inst *inst, ka_pid_t pid);
int trs_id_to_string(struct trs_id_inst *inst, int type, u32 id, char *msg, u32 msg_len);
int trs_id_get_range(struct trs_id_inst *inst, int type, u32 *start, u32 *end);

#ifdef __cplusplus
}
#endif
int init_trs_id(void);
void exit_trs_id(void);
#endif /* TRS_ID_H */
