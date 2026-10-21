/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "svm_addr_desc.h"
#include "svm_log.h"
#include "svm_register_to_master.h"

static struct svm_register_to_master_ops *register_to_master_ops[SVM_MAX_DEV_NUM][SVM_MAX_DEV_NUM] = {
    NULL,
};

void svm_register_to_master_set_ops(u32 user_devid, u32 devid, struct svm_register_to_master_ops *ops)
{
    if ((user_devid < SVM_MAX_DEV_NUM) && (devid < SVM_MAX_DEV_NUM)) {
        register_to_master_ops[user_devid][devid] = ops;
    }
}

int svm_register_to_master(u32 user_devid, struct svm_dst_va *register_va, u32 flag)
{
    u32 devid = register_va->devid;

    if ((user_devid >= SVM_MAX_DEV_NUM) || (devid >= SVM_MAX_DEV_NUM)) {
        svm_err("Invalid devid. (user_devid=%u; devid=%u)\n", user_devid, devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (register_to_master_ops[user_devid][devid] == NULL) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return register_to_master_ops[user_devid][devid]->register_to_master(user_devid, register_va, flag);
}

int svm_unregister_to_master(u32 user_devid, struct svm_dst_va *register_va, u32 flag)
{
    u32 devid = register_va->devid;

    if ((user_devid >= SVM_MAX_DEV_NUM) || (devid >= SVM_MAX_DEV_NUM)) {
        svm_err("Invalid devid. (user_devid=%u; devid=%u)\n", user_devid, devid);
        return DRV_ERROR_PARA_ERROR;
    }

    if (register_to_master_ops[user_devid][devid] == NULL) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return register_to_master_ops[user_devid][devid]->unregister_to_master(user_devid, register_va, flag);
}