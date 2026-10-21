/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/ioctl.h>
#include <stdio.h>

#include "ascend_hal_error.h"
#include "ascend_hal.h"
#include "svm_sys_cmd.h"
#include "svm_ioctl_ex.h"
#include "svm_pub.h"
#include "svm_log.h"
#include "svm_dbi.h"
#include "casm_ioctl.h"
#include "casm.h"

int svm_casm_create_key(struct svm_dst_va *dst_va, const struct casm_src_prop *prop, u64 *key)
{
    struct svm_casm_create_key_para para;
    int ret;

    if ((dst_va == NULL) || (prop == NULL) || (key == NULL)) {
        svm_err("Null ptr.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    para.task_type = dst_va->task_type;
    para.va = dst_va->va;
    para.size = dst_va->size;
    para.src_prop = *prop;

    ret = svm_cmd_ioctl(dst_va->devid, SVM_CASM_CREATE_KEY, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Create failed. (devid=%u; task_type=%u; va=0x%llx; size=0x%llx; ret=%d)\n", dst_va->devid,
                dst_va->task_type, dst_va->va, dst_va->size, ret);
        return ret;
    }

    *key = para.key;

    return 0;
}

int svm_casm_destroy_key(u64 key)
{
    struct svm_casm_destroy_key_para para;
    int ret;

    para.key = key;

    ret = svm_cmd_ioctl(svm_get_host_devid(), SVM_CASM_DESTROY_KEY, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Destroy failed. (key=0x%llx; ret=%d)\n", key, ret);
        return ret;
    }

    return 0;
}

static int svm_casm_op_task(u64 key, u32 server_id, int tgid[], u32 num, u32 op)
{
    struct svm_casm_op_task_para para;
    u32 op_pid_num = 0;
    u32 host_devid, i;
    int ret;

    if ((tgid == NULL) || (num == 0)) {
        svm_err("Invalid para. (key=0x%llx; num=%u)\n", key, num);
        return DRV_ERROR_INVALID_VALUE;
    }

    host_devid = svm_get_host_devid();

    for (i = 0; i < num; i++) {
        if (tgid[i] == 0) {
            svm_warn("Zero tgid will not op.\n");
            if (op == SVM_CASM_TASK_OP_ADD) {
                continue;
            } else {
                return ret;
            }
        }

        para.op = op;
        para.key = key;
        para.server_id = server_id;
        para.tgid = tgid[i];
        ret = svm_cmd_ioctl(host_devid, SVM_CASM_OP_TASK, (void *)&para);
        if (ret != DRV_ERROR_NONE) {
            svm_debug("Invalid pid will not op. (i=%u; op=%u; tgid=%d; ret=%d)\n", i, op, tgid[i], ret);
            if ((op == SVM_CASM_TASK_OP_ADD) && (ret != DRV_ERROR_OPER_NOT_PERMITTED)) {
                continue;
            } else {
                return ret;
            }
        }
        op_pid_num++;
    }

    if (op_pid_num == 0) {
        svm_err("Op task failed. (op=%u; key=0x%llx; ret=%d)\n", op, key, ret);
        return DRV_ERROR_PARA_ERROR;
    }

    return 0;
}

int svm_casm_add_task(u64 key, u32 server_id, int tgid[], u32 num)
{
    return svm_casm_op_task(key, server_id, tgid, num, SVM_CASM_TASK_OP_ADD);
}

int svm_casm_del_task(u64 key, u32 server_id, int tgid[], u32 num)
{
    return svm_casm_op_task(key, server_id, tgid, num, SVM_CASM_TASK_OP_DEL);
}

int svm_casm_check_task(u64 key, u32 server_id, int tgid[], u32 num)
{
    return svm_casm_op_task(key, server_id, tgid, num, SVM_CASM_TASK_OP_CHECK);
}

int svm_casm_get_src_va_ex(u32 devid, u64 key, struct svm_global_va *src_va, struct casm_src_va_ex_info *info)
{
    struct svm_casm_get_src_va_para para = {0};
    int ret;

    if ((src_va == NULL) || (info == NULL)) {
        svm_err("Null ptr. (key=0x%llx)\n", key);
        return DRV_ERROR_PARA_ERROR;
    }

    para.key = key;

    ret = svm_cmd_ioctl(devid, SVM_CASM_GET_SRC_VA, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Get failed. (key=0x%llx; ret=%d)\n", key, ret);
        return ret;
    }

    *src_va = para.src_va;
    info->src_prop = para.src_prop;
    info->update_va = para.updated_va;
    info->ex_info = para.ex_info;

    return 0;
}

int svm_casm_mem_pin(u32 devid, u64 va, u64 size, u64 key)
{
    struct svm_casm_mem_pin_para para;
    int ret;

    para.va = va;
    para.size = size;
    para.key = key;

    ret = svm_cmd_ioctl(devid, SVM_CASM_MEM_PIN, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Mem pin failed. (devid=%u; va=0x%llx; size=0x%llx; key=0x%llx; ret=%d)\n", devid, va, size, key, ret);
        return ret;
    }

    return 0;
}

int svm_casm_mem_unpin(u32 devid, u64 va, u64 size)
{
    struct svm_casm_mem_unpin_para para;
    int ret;

    para.va = va;
    para.size = size;

    ret = svm_cmd_ioctl(devid, SVM_CASM_MEM_UNPIN, (void *)&para);
    if (ret != DRV_ERROR_NONE) {
        svm_err("Mem unpin failed. (devid=%u; va=0x%llx; size=0x%llx; ret=%d)\n", devid, va, size, ret);
        return ret;
    }

    return 0;
}
