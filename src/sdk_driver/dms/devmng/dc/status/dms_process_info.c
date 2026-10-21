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
#include "ascend_dev_num.h"
#include "pbl/pbl_runenv_config.h"
#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_urd.h"
#include "pbl/pbl_urd_main_cmd_def.h"
#include "pbl/pbl_urd_sub_cmd_def.h"
#include "ascend_hal_define.h"
#include "pbl_mem_alloc_interface.h"
#include "dms_define.h"
#include "devdrv_manager_container.h"
#include "ka_memory_pub.h"
#include "dpa_kernel_interface.h"

#define DMS_PROCESS_INFO_CMD_NAME "DMS_PROCESS_INFO"
#define DMS_VDEV_MAX_NUM_PER_PHY_DEV 16U
#define DMS_DEV_MAX_NUM_PER_PHY_DEV (DMS_VDEV_MAX_NUM_PER_PHY_DEV + 1U)
#define DMS_PROCESS_MAX_NUM 64
#define DMS_MASTER_PROC_MAX_NUM 2048U
#define DMS_DEV_PROCESS_CONTAINER_PID 10

int hal_kernel_apm_query_slave_tgid_by_master(int master_tgid, u32 udevid, processType_t proc_type, int *slave_tgid);

STATIC void dms_get_valid_dev_list(struct urd_cmd_kernel_para *kernel_para, unsigned int *dev_list,
                                   unsigned int *dev_num)
{
    unsigned int vdev_udevid;
    unsigned int num = 0;
    unsigned int i;

    if (run_in_normal_docker()) { /* in normal docker, only query master in udevid */
        dev_list[num++] = kernel_para->udevid;
    } else { /* in phy, admin docker or virtaul-machine, query master in phy id */
        /* get all vnpus udevids base on phy dev */
        dev_list[num++] = kernel_para->phyid;
        for (i = 0; i < DMS_VDEV_MAX_NUM_PER_PHY_DEV; i++) {
            vdev_udevid = (ASCEND_VDEV_ID_START + kernel_para->phyid * DMS_VDEV_MAX_NUM_PER_PHY_DEV);
            if (uda_is_udevid_exist(vdev_udevid)) {
                dev_list[num++] = vdev_udevid;
            }
        }
    }
    *dev_num = num;
}

#ifndef DMS_UT
STATIC bool process_is_in_current_container(ka_task_struct_t *tsk)
{
    if (!tsk->nsproxy || !ka_task_get_current_nsproxy()) {
        return false;
    }
    /* Mount namespace, file system isolation. */
    if (tsk->nsproxy->mnt_ns != ka_task_get_current_mnt_ns()) {
        return false;
    }

    return true;
}
#endif

STATIC int dms_get_normal_docker_process_list(unsigned int udevid, unsigned int resource_type, int *master_tgid)
{
#ifndef DMS_UT
    ka_task_struct_t *tsk = NULL;
    int ret;

    ka_task_rcu_read_lock();
    ka_for_each_process(tsk)
    {
        if ((tsk != NULL) && (tsk->pid == *master_tgid)) {
            if (process_is_in_current_container(tsk) ||
                ((resource_type == DMS_DEV_PROCESS_CONTAINER_PID) && (run_in_admin_docker()))) {
                break;
            }
            ka_task_rcu_read_unlock();
            return -EINVAL;
        }
    }
    ka_task_rcu_read_unlock();

    ret = dbl_host_pid_to_container_pid(*master_tgid, master_tgid);
    if (ret != 0) {
        dms_err("Convert container pid failed. (udevid=%u; master_tgid=%d; ret=%d)\n", udevid, *master_tgid, ret);
        return ret;
    }
#endif
    return 0;
}

STATIC int _dms_get_process_list(struct urd_cmd_kernel_para *kernel_para, struct urd_cmd_para *para, int *master_tgids,
                                 unsigned int master_num, unsigned int resource_type)
{
    unsigned int dev_list[DMS_DEV_MAX_NUM_PER_PHY_DEV];
    unsigned int dev_num = 0;
    int *pid_list = NULL;
    unsigned int pid_num = 0;
    unsigned int i, j;
    int slave_tgid;
    int ret = 0;

    dms_get_valid_dev_list(kernel_para, dev_list, &dev_num);

    pid_list = (int *)dbl_kmalloc(sizeof(int) * DMS_PROCESS_MAX_NUM, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pid_list == NULL) {
        dms_err("kmalloc buf failed.\n");
        return -ENOMEM;
    }

    for (i = 0; i < master_num; i++) {
        for (j = 0; j < dev_num; j++) {
            if (hal_kernel_apm_query_slave_tgid_by_master(master_tgids[i], dev_list[j], PROCESS_CP1, &slave_tgid) !=
                0) {
                continue;
            }
            if (run_in_normal_docker() &&
                (dms_get_normal_docker_process_list(kernel_para->udevid, resource_type, &master_tgids[i]) != 0)) {
                continue;
            } else if (resource_type == DMS_DEV_PROCESS_CONTAINER_PID) {
                (void)dms_get_normal_docker_process_list(kernel_para->udevid, resource_type, &master_tgids[i]);
            }
            pid_list[pid_num++] = master_tgids[i];
            break;
        }
        if (pid_num == DMS_PROCESS_MAX_NUM) {
            break;
        }
    }

    ret = memcpy_s(para->output, para->output_len, pid_list, sizeof(int) * pid_num);
    if (ret != 0) {
        dms_err("The memcpy_s failed. (udevid=%u; ret=%d)\n", kernel_para->udevid, ret);
    }

    dbl_kfree(pid_list);
    pid_list = NULL;
    return ret;
}

int dms_get_process_list(const struct urd_cmd *cmd, struct urd_cmd_kernel_para *kernel_para, struct urd_cmd_para *para)
{
    unsigned int master_num = 0;
    unsigned int resource_type;
    int *master_tgids;
    int ret = 0;

    if ((cmd == NULL) || (kernel_para == NULL) || (para == NULL)) {
        dms_err("Input urd argument is null. (cmd_is_NULL=%d; kernel_para_is_NULL=%d; para_is_NULL=%d)\n", cmd == NULL,
                kernel_para == NULL, para == NULL);
        return -EINVAL;
    }

    if ((para->input == NULL) || (para->input_len != sizeof(unsigned int))) {
        dms_err("Input argument is null, or len is wrong. (input_len=%u)\n", para->input_len);
        return -EINVAL;
    }

    if ((para->output == NULL) || (para->output_len < sizeof(int) * DMS_PROCESS_MAX_NUM)) {
        dms_err("Output argument is null, or len is wrong. (output_len=%u)\n", para->output_len);
        return -EINVAL;
    }

    resource_type = *(unsigned int *)para->input;
    master_tgids = (int *)dbl_kmalloc(sizeof(int) * DMS_MASTER_PROC_MAX_NUM, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (master_tgids == NULL) {
        dms_err("kmalloc buf failed.\n");
        return -ENOMEM;
    }

    ret = apm_get_all_master_tgids(master_tgids, DMS_MASTER_PROC_MAX_NUM, &master_num);
    if (ret != 0) {
        dbl_kfree(master_tgids);
        master_tgids = NULL;
        dms_err("Get master tgid failed. (ret=%d)\n", ret);
        return ret;
    }

    master_num = master_num < DMS_MASTER_PROC_MAX_NUM ? master_num : DMS_MASTER_PROC_MAX_NUM;
    ret = _dms_get_process_list(kernel_para, para, master_tgids, master_num, resource_type);
    if (ret != 0) {
        dms_err("Get process list failed. (ret=%d)\n", ret);
    }

    dbl_kfree(master_tgids);
    master_tgids = NULL;
    return ret;
}

int dms_get_process_memory(const struct urd_cmd *cmd, struct urd_cmd_kernel_para *kernel_para,
                           struct urd_cmd_para *para)
{
    unsigned long long mem_size;
    int pid;
    int master_tgid;
    int ret = 0;

    if ((cmd == NULL) || (kernel_para == NULL) || (para == NULL)) {
        dms_err("Input urd argument is null. (cmd_is_NULL=%d; kernel_para_is_NULL=%d; para_is_NULL=%d)\n", cmd == NULL,
                kernel_para == NULL, para == NULL);
        return -EINVAL;
    }

    if ((para->input == NULL) || (para->input_len != sizeof(int))) {
        dms_err("Input argument is null, or len is wrong. (input_len=%u)\n", para->input_len);
        return -EINVAL;
    }

    if ((para->output == NULL) || (para->output_len != sizeof(unsigned long long))) {
        dms_err("Output argument is null, or len is wrong. (output_len=%u)\n", para->output_len);
        return -EINVAL;
    }

    pid = *(int *)para->input;
    if (run_in_normal_docker()) {
        ret = devdrv_get_tgid_by_pid(pid, &master_tgid);
        if (ret != 0) {
            dms_err("Failed to get master tgid. (udevid=%u; pid=%d; ret=%d)\n", kernel_para->udevid, pid, ret);
            return ret;
        }
    } else {
        master_tgid = pid;
    }

    ret = apm_query_slave_all_meminfo_by_master(master_tgid, kernel_para->udevid, PROCESS_CP1, &mem_size);
    if (ret != 0) {
        if (ret == -ESRCH) {
            return -EOPNOTSUPP;
        }
        dms_err("Failed to query slave memory through master. (udevid=%u; ret=%d)", kernel_para->udevid, ret);
        return ret;
    }
    *(unsigned long long *)para->output = mem_size;

    return 0;
}

BEGIN_DMS_MODULE_DECLARATION(DMS_PROCESS_INFO_CMD_NAME)
BEGIN_FEATURE_COMMAND()
ADD_DEV_FEATURE_COMMAND(DMS_PROCESS_INFO_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_PROCESS_LIST, NULL, NULL,
                        DMS_SUPPORT_ALL, dms_get_process_list)
ADD_DEV_FEATURE_COMMAND(DMS_PROCESS_INFO_CMD_NAME, DMS_MAIN_CMD_BASIC, DMS_SUBCMD_GET_PROCESS_MEMORY, NULL, NULL,
                        DMS_SUPPORT_ALL, dms_get_process_memory)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

STATIC int dms_process_info_init(void)
{
    CALL_INIT_MODULE(DMS_PROCESS_INFO_CMD_NAME);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_process_info_init, FEATURE_LOADER_STAGE_5);

STATIC void dms_process_info_uninit(void)
{
    CALL_EXIT_MODULE(DMS_PROCESS_INFO_CMD_NAME);
}
DECLAER_FEATURE_AUTO_UNINIT(dms_process_info_uninit, FEATURE_LOADER_STAGE_5);
