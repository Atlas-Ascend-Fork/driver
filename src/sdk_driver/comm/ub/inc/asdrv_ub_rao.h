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
#ifndef ASDRV_UB_RAO_H
#define ASDRV_UB_RAO_H
#include "ubcore_opcode.h"
#include "asdrv_ub_main_adapt.h"

#define UBDRV_RAO_CLIENT_DISABLE 0
#define UBDRV_RAO_CLIENT_ENABLE 1

struct ubdrv_rao_client_ctrl {
    u32 status;
    ka_mutex_t mutex_lock;
};

struct ubdrv_rao_operate {
    enum devdrv_rao_client_type type;
    enum ubcore_opcode opcode;
    u64 offset;
    u64 len;
};

typedef struct ubdrv_rao_client_ctrl (*rao_client_ctrl_arr_ptr)[DEVDRV_RAO_CLIENT_MAX];

int ubdrv_rao_client_ctrl_init(void);
void ubdrv_rao_client_ctrl_uninit(void);
int ubdrv_register_rao_client(u32 dev_id, enum devdrv_rao_client_type type, u64 va, u64 len,
                              enum devdrv_rao_permission_type perm);
int ubdrv_unregister_rao_client(u32 dev_id, enum devdrv_rao_client_type type);
void ubdrv_free_all_rao_chan(u32 dev_id);
rao_client_ctrl_arr_ptr get_global_rao_client_ctrl(void);
int ubdrv_register_rao_para_check(u32 dev_id, enum devdrv_rao_client_type type, u64 va, u64 len,
                                  enum devdrv_rao_permission_type perm);

#endif // ASDRV_UB_RAO_H