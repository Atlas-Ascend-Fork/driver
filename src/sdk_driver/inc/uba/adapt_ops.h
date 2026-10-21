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

#ifndef __ADAPT_OPS_H__
#define __ADAPT_OPS_H__

#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "module_type.h"
#include "adapt_ops_table.h"

#define OPS_WAIT_MAX_CNT 10000
#define OPS_WAIT_PER_TIME 1000

enum uba_ops_type {
    UBA_OPS_UBCORE = 0,
    UBA_OPS_UMMU,
    UBA_OPS_TYPE_MAX,
};

enum uba_ops_status {
    UBA_OPS_STATUS_UNINIT = 0,
    UBA_OPS_STATUS_INIT = 1,
    UBA_OPS_STATUS_ENABLE = 2,
    UBA_OPS_STATUS_DISABLE = 3,
    UBA_OPS_STATUS_MAX = 4,
};

enum uba_func_type {
    UBA_FUNC_ALLOC_TDEV = 0,
    UBA_FUNC_FREE_TDEV,
    UBA_FUNC_CREATE_JFC,
    UBA_FUNC_DELETE_JFC,
    UBA_FUNC_CREATE_JFR,
    UBA_FUNC_DELETE_JFR,
    UBA_FUNC_POST_JFR_WR,
    UBA_FUNC_USER_CONTROL,
    UBA_FUNC_POLL_JFC,
    UBA_FUNC_REARM_JFC,
    UBA_FUNC_REGISTER_SEG,
    UBA_FUNC_UNREGISTER_SEG,
    UBA_FUNC_IMPORT_SEG,
    UBA_FUNC_UNIMPORT_SEG,
    UBA_FUNC_IMPORT_JFR,
    UBA_FUNC_IMPORT_JFR_EX,
    UBA_FUNC_UNIMPORT_JFR,
    UBA_FUNC_IMPORT_JETTY,
    UBA_FUNC_UNIMPORT_JETTY,
    UBA_FUNC_CREATE_JETTY,
    UBA_FUNC_DELETE_JETTY,
    UBA_FUNC_POST_JETTY_SEND_WR,
    UBA_FUNC_IMPORT_JETTY_EX,
    UBA_FUNC_ALLOC_JFC,
    UBA_FUNC_SET_JFC_OPT,
    UBA_FUNC_ACTIVE_JFC,
    UBA_FUNC_DEACTIVE_JFC,
    UBA_FUNC_FREE_JFC,
    UBA_FUNC_ALLOC_JFS,
    UBA_FUNC_SET_JFS_OPT,
    UBA_FUNC_ACTIVE_JFS,
    UBA_FUNC_DEACTIVE_JFS,
    UBA_FUNC_FREE_JFS,
    UBA_FUNC_GET_TP_LIST,
    UBA_FUNC_PUT_DEVICE,
    UBA_FUNC_GET_JFC_OPT_BY_ID,
    UBA_FUNC_GET_JETTY_OPT_BY_ID,
    UBA_FUNC_GET_DEVICE_BY_EID,
    UBA_FUNC_TYPE_MAX,
};

struct uba_dev_ops {
    enum uba_ops_status status;
    union {
        struct uba_ubcore_adapt_ops *ubcore;
        struct uba_ummu_adapt_ops *ummu;
    } ops;
    enum uba_ops_type type;
    ka_rwlock_t rwlock;
    ka_atomic_t ref_cnt;
};

int uba_register_adapt_ops(enum uba_ops_type ops_type, struct uba_dev_ops *dev_ops);
void uba_unregister_adapt_ops(enum uba_ops_type ops_type);
struct uba_dev_ops *uba_add_adapt_ops_ref(enum uba_ops_type ops_type);
void uba_sub_adapt_ops_ref(struct uba_dev_ops *dev_ops);
struct uba_dev_ops *uba_get_adapt_ops(enum uba_ops_type ops_type);

void uba_add_module(enum uba_module_type module_type, enum uba_func_type func_type);
ka_atomic_t *uba_get_module_cnt(void);

#define UBA_ADD_MODULE_ON_SUCC(success_cond, module_type, func_type) \
    do {                                                             \
        if (success_cond) {                                          \
            uba_add_module((module_type), (func_type));              \
        }                                                            \
    } while (0)

#endif
