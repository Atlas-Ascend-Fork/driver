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

#ifndef DPA_APM_KERNEL_H
#define DPA_APM_KERNEL_H

#include "ka_dfx_pub.h"
#include "ka_common_pub.h"
#include "ka_memory_pub.h"

#include "dpa_pids_map.h"
#include "pbl/pbl_task_ctx.h"

/************************ task group *****************************/
static inline int apm_trans_proc_type_from_bitmap(unsigned int proc_type_bitmap)
{
    int proc_type;

    for (proc_type = 0; proc_type < PROCESS_CPTYPE_MAX; proc_type++) {
        if ((proc_type_bitmap & (0x1 << proc_type)) != 0) {
            break;
        }
    }

    return proc_type;
}

int apm_proc_mem_query_handle_register(int (*proc_mem_query)(u32 udevid, int tgid, u64 *out_size));
void apm_proc_mem_query_handle_unregister(void);

int hal_kernel_apm_query_slave_tgid_by_master(int master_tgid, u32 udevid, processType_t proc_type, int *slave_tgid);
int apm_query_master_info_by_slave(int slave_tgid, int *master_tgid, u32 *udevid, int *mode, u32 *proc_type_bitmap);
static inline int apm_query_master_tgid_by_slave(int slave_tgid, int *master_tgid)
{
    u32 udevid, proc_type_bitmap;
    int mode;

    return apm_query_master_info_by_slave(slave_tgid, master_tgid, &udevid, &mode, &proc_type_bitmap);
}

static inline int apm_query_proc_type_bitmap_by_slave(int slave_tgid, u32 *proc_type_bitmap)
{
    u32 udevid;
    int master_tgid, mode;

    return apm_query_master_info_by_slave(slave_tgid, &master_tgid, &udevid, &mode, proc_type_bitmap);
}

static inline int apm_query_proc_type_by_slave(int slave_tgid, processType_t *proc_type)
{
    u32 proc_type_bitmap;
    int ret = apm_query_proc_type_bitmap_by_slave(slave_tgid, &proc_type_bitmap);
    if (ret == 0) {
        *proc_type = (processType_t)apm_trans_proc_type_from_bitmap(proc_type_bitmap);
    }

    return ret;
}

/*
 * HOST:        query form master domain, or apm msg query form svm
 * DEVICE EP:   query from host master proxy domain, or query form svm
 * DEVICE RC:   query form master domain, or query form svm
 */
int hal_kernel_apm_query_slave_ssid_by_master(u32 udevid, int master_tgid, processType_t proc_type, u32 *ssid);

/*
 * HOST:    not support
 * DEVICE:  query form slave domain, or query form svm
 */
int apm_query_slave_ssid(u32 udevid, int slave_tgid, int *ssid);

/************************ task exit *****************************/
/* apm task exit control sequence:
   1: set when task exit release need use apm
   2: master or slave do exit, in this stage module can recycle self resource
   3: slave(cp) stop stream
   4: master stop stream
   5: slave(cp) recycle resource
   6: master recycle resource
   7: use APM_STAGE_(PRE_)* to meet "master release before slave" scene
   n->notifier_call: int exit_notify(ka_notifier_block_t *self, unsigned long val, void *data)
                     val : stage+pid, data : null
   n->priority: 0, lowest pri */
enum apm_exit_stage {
    APM_STAGE_PRE_EXIT = 1,
    APM_STAGE_DO_EXIT,
    APM_STAGE_STOP_STREAM,
    APM_STAGE_PRE_RECYCLE_RES,
    APM_STAGE_RECYCLE_RES,
    APM_STAGE_POST_RECYCLE_RES,
    APM_STAGE_MAX
};

#define APM_STAGE_OFFSET 32U
#define APM_FORCE_EXIT_FLAG_OFFSET 48U
static inline bool apm_get_exit_force_flag(unsigned long val)
{
    return (((val >> APM_FORCE_EXIT_FLAG_OFFSET) & 0x1ULL) == 1U);
}

static inline int apm_get_exit_stage(unsigned long val)
{
    return (int)((val >> APM_STAGE_OFFSET) & 0xFFFF);
}

static inline int apm_get_exit_tgid(unsigned long val)
{
    return (int)((val) & 0xFFFFFFFF);
}

bool apm_master_domain_check_set_pre_exit(int tgid, struct task_start_time *time);
bool apm_slave_domain_check_set_pre_exit(int tgid, struct task_start_time *time);
/* notify apm call release, others release by user self */
static inline bool apm_notify_task_exit(int tgid, struct task_start_time *time)
{
    return (apm_master_domain_check_set_pre_exit(tgid, time) || apm_slave_domain_check_set_pre_exit(tgid, time));
}

int apm_master_exit_register(ka_notifier_block_t *n);
void apm_master_exit_unregister(ka_notifier_block_t *n);
int apm_slave_exit_register(ka_notifier_block_t *n);
void apm_slave_exit_unregister(ka_notifier_block_t *n);
int apm_remote_master_exit_register(ka_notifier_block_t *n);
void apm_remote_master_exit_unregister(ka_notifier_block_t *n);

/* notifier_block->priority: the number larger, the priority higher */
enum apm_task_exit_notify_pri {
    APM_EXIT_NOTIFIY_PRI_APM_MASTER = 8,
    APM_EXIT_NOTIFIY_PRI_APM_SLAVE = 9,
    APM_EXIT_NOTIFIY_PRI_APM_SLAVE_DEVICE_PROXY = 10,
    APM_EXIT_NOTIFIY_PRI_SVM_START = 11, /* svm range 11~15 */
    APM_EXIT_NOTIFIY_PRI_SVM = 15,
    APM_EXIT_NOTIFIY_PRI_TSDRV = 16,
    APM_EXIT_NOTIFIY_PRI_APM_RES_SLAVE = 17,
    APM_EXIT_NOTIFIY_PRI_APM_RES = 18
};

static inline int apm_task_exit_register(ka_notifier_block_t *master_nb, ka_notifier_block_t *slave_nb)
{
    int ret = 0;

    ret = apm_master_exit_register(master_nb);
    if (ret != 0) {
        return ret;
    }

    ret = apm_slave_exit_register(slave_nb);
    if (ret != 0) {
        apm_master_exit_unregister(master_nb);
    }

    return ret;
}

static inline void apm_task_exit_unregister(ka_notifier_block_t *master_nb, ka_notifier_block_t *slave_nb)
{
    apm_slave_exit_unregister(slave_nb);
    apm_master_exit_unregister(master_nb);
}

/************************ resource map *****************************/
/*
    ep: master in host
        host master res map: host current master task check, device get res addr
    ep/rc: master in device
        device master res map: device current master task check, device get res addr
        device slave res map: device current slave task check, device current get res addr
*/
struct apm_res_map_ops {
    ka_module_t *owner;
    bool (*res_is_belong_to_proc)(int master_tgid, int slave_tgid, u32 udevid, struct res_map_info_in *res_info);
    int (*get_res_addr)(u32 udevid, struct res_map_info_in *res_info, u64 *pa, u32 *len);
    /* res is mem, may have multi pages, every pa len is one page, len res total len, must page align */
    int (*get_res_addr_array)(u32 udevid, struct res_map_info_in *res_info, u64 pa[], u32 num, u32 *len);
    void (*put_res_addr_array)(u32 udevid, struct res_map_info_in *res_info, u64 pa[], u32 len);
    int (*update_res_info)(u32 udevid, struct res_map_info_in *res_info);
    int (*res_map_post_handle)(u32 udevid, int slave_tgid, struct res_map_info_in *res_info, u64 va, u64 size);
    int (*res_unmap_pre_handle)(u32 udevid, int slave_tgid, struct res_map_info_in *res_info, u64 va, u64 size);
};

int hal_kernel_apm_res_map_ops_register(enum res_addr_type res_type, struct apm_res_map_ops *ops);
void hal_kernel_apm_res_map_ops_unregister(enum res_addr_type res_type);

enum drv_module_type {
    SVM_MODULE,
    MAX_DRV_MODULE_TYPE
};

typedef bool (*vma_check_handle)(ka_vm_area_struct_t *vma);
int hal_kernel_apm_vma_check_register(enum drv_module_type type, vma_check_handle func);
void hal_kernel_apm_vma_check_unregister(enum drv_module_type type);

int apm_master_use(int master_tgid, u32 udevid);
int apm_master_unuse(int master_tgid, u32 udevid);

#endif
