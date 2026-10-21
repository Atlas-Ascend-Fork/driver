/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SVM_INIT_PRI_H
#define SVM_INIT_PRI_H

/*
    define constructor/destructor priority
    The priority ranges from 0 to 65535, with smaller numbers indicating higher priority.
    The destructor has the opposite priority. If not specified, the default priority is the lowest value 65535.
    When priorities are the same, the order of declaration within the same file determines the priority;
    for different files, the order is not guaranteed.
    0-100 is usually reserved for system use.

    svm functions performed inside the constructor/destructor:
    1. device init/uninit svm_register_ioctl_dev_init_post_handle
        Because there is no priority when registering handles, the framework calls them back in the order
        they are registered. Generally, lower-level modules need to be initialized first, so they should
        have a higher constructor priority and be registered with the framework earlier.
        first: dbi, apbi, urma_seg_mng, pcie/ub share
            urma_seg_mng needs to register the dev uninit pre-handle before
   urma_adapt, so remote segs
            are recycled after urma_adapt unregisters reserved segments during device
   close.
        pre_high: assign_gap
        hign: va allocator
        medium: handle register: op/query low: cache malloc,
   urma_seg_local final: others: mms, host/pcie_th register, urma adapt chan/seg init, mem show, task group
    2. submodule global variables init/uninit
        device init is after constructor, so global variables can be initialized independently using default priority.
    3. registration of hook functions for other functionalities between submodules
        a. Use the default priority if there is no impact
           for example: svm_set_agent_init_ops, drv_register_event_proc
        b. Use the default priority if there is only one register
           for example: svm_normal_set_ops, svm_mng_set_ops, svm_vmm_set_ops, svm_mem_repair_set_ops
        c. ...
*/

#define SVM_INIT_PRI_FISRT 201
#define SVM_INIT_PRI_PRE_HIGH 301
#define SVM_INIT_PRI_HIGH 401
#define SVM_INIT_PRI_MEDIUM 501
#define SVM_INIT_PRI_LOW 601
#define SVM_INIT_PRI_FINAL 65535

#endif
