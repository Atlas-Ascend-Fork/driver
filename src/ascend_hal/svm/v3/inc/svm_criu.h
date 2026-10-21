/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef SVM_CRIU_H
#define SVM_CRIU_H

#include <stdbool.h>

#include "svm_pub.h"

struct svm_criu_ops {
    const char *name; /* optional, used for log */

    int (*backup)(u32 devid, void *data);
    /*
     * Called before restore to clear stale globals/resources that
     * should not be reused after CRIU restore.
     */
    int (*reset)(u32 devid, void *data);
    int (*restore)(u32 devid, void *data);
};

int svm_criu_register_ops(const struct svm_criu_ops *ops);
int svm_criu_unregister_ops(const struct svm_criu_ops *ops);

/* Backup/restore callbacks are invoked in registration order, reset callbacks in reverse registration order. */
int svm_criu_backup(u32 devid, void *data);
int svm_criu_reset(u32 devid);
int svm_criu_restore(u32 devid, void *data);

bool svm_criu_is_resetting(u32 devid);
bool svm_criu_is_device_reopen(u32 devid);

#endif
