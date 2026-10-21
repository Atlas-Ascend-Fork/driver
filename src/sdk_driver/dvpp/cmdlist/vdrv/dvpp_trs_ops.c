/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#include "dvpp_trs_ops.h"

#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_barrier_pub.h"
#include "securec.h"
#include "ts_agent_dvpp.h"
#include "comm_kernel_interface.h"
#include "dvpp_cmdlist.h"
#include "dvpp_cmdlist_log.h"
#include "dvpp_share_mem_ctrl.h"
#include "dvpp_sqe_para_check.h"
#include "dvpp_svm_mem_ctrl.h"
#include "dvpp_vcm_agent.h"
#include "dvpp_cmdlist_ioctl.h"

static int32_t get_svm_pa_list(ka_pid_t pid, dvpp_sqe_args *sqe_args, dvpp_share_blk *blk)
{
    int32_t ret = 0;
    uint64_t page_size = 0;
    uint64_t pa_num = 0;
    uint64_t mem_list_size = 0;
    dvpp_svm_pa_info *pa_info = NULL;
    uint64_t pa_info_size = 0;
    page_size = dvpp_get_page_size(pid, sqe_args->cmdbuf_uva, sqe_args->cmdbuf_size);
    if (page_size == 0) {
        DVPP_CMDLIST_LOG_ERROR("get page size fail.\n");
        return -1;
    }

    pa_num = dvpp_get_pa_num(sqe_args->cmdbuf_uva, sqe_args->cmdbuf_size, page_size);
    if (pa_num == 0) {
        DVPP_CMDLIST_LOG_ERROR("get page num fail.\n");
        return -1;
    }
    mem_list_size = sizeof(uint64_t) * pa_num;
    pa_info_size = sizeof(dvpp_svm_pa_info) + mem_list_size * 2; // 2 mem_list_size
    if (sqe_args->args_size + pa_info_size > blk->size) {
        DVPP_CMDLIST_LOG_ERROR("args size:%u and painfo size %llu bigger than blk size %llu\n", sqe_args->args_size,
                               pa_info_size, blk->size);
        return -1;
    }

    pa_info = (dvpp_svm_pa_info *)(sqe_args->args_addr + sqe_args->args_size);
    pa_info->pa_list = (uint64_t *)((uint64_t)(uintptr_t)(pa_info) + sizeof(dvpp_svm_pa_info));
    pa_info->size_list = (uint64_t *)((uint64_t)(uintptr_t)(pa_info->pa_list) + mem_list_size);
    pa_info->num = pa_num;
    pa_info->page_size = page_size;
    ret = dvpp_get_pa_list_from_svm_addr(pid, sqe_args->cmdbuf_uva, sqe_args->cmdbuf_size, pa_info);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("get phy addr list based on svm addr fail, uva:%pK, size:%u, ret:%d\n",
                               (void *)(sqe_args->cmdbuf_uva), sqe_args->cmdbuf_size, ret);
        return -1;
    }

    // put回去，防止内存无法释放
    dvpp_put_pa_list(pid, sqe_args->cmdbuf_uva, sqe_args->cmdbuf_size, pa_info);

    return 0;
}

static int32_t dvpp_trs_sqe_update(uint32_t devid, uint32_t tsid, int32_t pid, void *sqe)
{
    uint64_t n = 0;
    dvpp_share_blk_mod mod_id;
    dvpp_share_blk_type blk_type;
    dvpp_sqe_args *sqe_args = NULL;
    dvpp_share_blk *blk = NULL;
    dvpp_share_mem_pool *pool = NULL;

    if (sqe == NULL) {
        DVPP_CMDLIST_LOG_ERROR("sqe passed from outside is null.\n");
        return -1;
    }
    sqe_args = (dvpp_sqe_args *)sqe;

    mod_id = trans_sqe_type_to_mod_id(sqe_args->sqe_type);
    if (mod_id == DVPP_SHARE_BLK_MOD_BUTT) {
        DVPP_CMDLIST_LOG_ERROR("get mod id fail sqe_type=%u.\n", sqe_args->sqe_type);
        return -1;
    }

    blk_type = trans_ptr_mode_to_blk_type(sqe_args->ptr_mode);

    if (check_dvpp_sqe_args(sqe_args) != 0) {
        DVPP_CMDLIST_LOG_ERROR("check dvpp sqe args fail.\n");
        return -1;
    }

    // 初始化共享内存池
    pool = ka_smp_load_acquire(&g_share_mem_pool[devid]);
    if (pool == NULL) {
        ka_task_spin_lock(&g_share_mem_pool_lock[devid]);
        if (g_share_mem_pool[devid] == NULL) {
            dvpp_share_mem_pool *new_pool = dvpp_init_share_mem_pool(devid, sqe_args);
            if (new_pool == NULL) {
                ka_task_spin_unlock(&g_share_mem_pool_lock[devid]);
                return -1;
            }
            ka_smp_store_release(&g_share_mem_pool[devid], new_pool);
        }
        pool = g_share_mem_pool[devid];
        ka_task_spin_unlock(&g_share_mem_pool_lock[devid]);
    }

    // 基于模块从共享内存池取出内存块
    // 用户态有反压机制，确保走到这里一定可以申请到内存块
    blk = dvpp_get_share_mem_blk_from_pool(blk_type, mod_id, pool);
    if (blk == NULL) {
        DVPP_CMDLIST_LOG_ERROR("get share memory block from pool fail.\n");
        return -1;
    }

    if (sqe_args->args_size > blk->size) {
        DVPP_CMDLIST_LOG_ERROR("args size %u is bigger than blk size %llu.\n", sqe_args->args_size, blk->size);
        return -1;
    }

    // args内存拷贝到共享内存中
    n = ka_base_copy_from_user((void *)blk->addr, (void *)(sqe_args->args_addr), sqe_args->args_size);
    if (n != 0) {
        DVPP_CMDLIST_LOG_ERROR("copy args from user fail, need copy size is %u, remain %llu is not copied.\n",
                               sqe_args->args_size, n);
        return -1;
    }
    sqe_args->args_addr = blk->addr;

    // cmdlist内存转成pa地址后拷贝到共享内存中
    if (get_svm_pa_list(pid, sqe_args, blk) != 0) {
        DVPP_CMDLIST_LOG_ERROR("get svm pa list fail.\n");
        return -1;
    }

    // 这里保存offset，因为在物理机上基地址不一样
    sqe_args->args_addr = (uintptr_t)(blk) - (uintptr_t)(pool);

    return 0;
}

static int32_t dvpp_send_sqe_to_phy(uint32_t devid, int pid, struct dvpp_sqe *sqe)
{
    int32_t ret;
    struct dvpp_vcm_msg msg;

    msg.cmd = DVPP_VCM_GEN_CMDLIST;
    msg.msg_data_len = sizeof(struct dvpp_sqe);
    ret = memcpy_s((void *)&msg.msg_data[0], DVPP_VCM_MSG_DATA_MAXLEN, (void *)sqe, sizeof(struct dvpp_sqe));
    if (ret != EOK) {
        DVPP_CMDLIST_LOG_ERROR("gen cmdlist devid=%u pid=%d memcpy failed. ret=%d,(%u,%lu)\n", devid, pid, ret,
                               DVPP_VCM_MSG_DATA_MAXLEN, sizeof(struct dvpp_sqe));
        return ret;
    }

    ret = dvpp_vcm_msg_send(devid, &msg);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("gen cmdlist devid=%u pid=%d msg send failed ret=%d\n", devid, pid, ret);
        return ret;
    }

    // dvpp_vcm_msg_send执行完成后，msg的内存会被刷新成recv端填充的内容
    if (msg.msg_data_len != sizeof(struct dvpp_sqe)) {
        DVPP_CMDLIST_LOG_ERROR("gen cmdlist devid=%u pid=%u msg data len=%u invalid\n", devid, pid, msg.msg_data_len);
        return -1;
    }

    ret = memcpy_s((void *)sqe, sizeof(struct dvpp_sqe), (void *)&msg.msg_data[0], msg.msg_data_len);
    if (ret != EOK) {
        DVPP_CMDLIST_LOG_ERROR("gen cmdlist devid=%u pid=%u memcpy failed. ret=%d (%lu,%u)\n", devid, pid, ret,
                               sizeof(struct dvpp_sqe), msg.msg_data_len);
        return ret;
    }
    return 0;
}

static int32_t dvpp_gen_cmdlist_handler(dvpp_cmdlist_ioctl_args *arg)
{
    int32_t ret;
    int32_t pid;
    uint32_t devid; // 虚拟机场景，使用用户配置的aclrtSetDevice的devid，虚拟机有自己的内核态，需要使用虚拟devid
    uint32_t phyid;
    dvpp_gen_cmdlist_user_data user_data = {0};
    struct dvpp_sqe sqe = {0};

    ret = dvpp_get_gen_cmdlist_info_from_ioctl(arg, &user_data, &pid, &devid, &phyid, &sqe);
    if (ret != 0) {
        return ret;
    }

    if (devid >= DVPP_VMNG_DEVICE_NUM_MAX) {
        DVPP_CMDLIST_LOG_ERROR("invalid devid:%u must be less than:%u\n", devid, DVPP_VMNG_DEVICE_NUM_MAX);
        return -1;
    }

    // 完成cmdbuf的uva地址转pa地址，该处理只能在虚拟机侧完成
    ret = dvpp_trs_sqe_update(devid, 0, pid, &sqe);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("gen cmdlist devid=%u pid=%d seq update failed ret=%d\n", devid, pid, ret);
        return ret;
    }

    // 将sqe发给物理机，在物理机侧根据args_addr中存的tlvlist结构信息，生成cmdlist并拷入pa内存中
    ret = dvpp_send_sqe_to_phy(devid, pid, &sqe);
    if (ret != 0) {
        return ret;
    }

    ret = dvpp_set_gen_cmdlist_sqe_to_user_data(&sqe, &user_data);
    if (ret != 0) {
        return ret;
    }
    return 0;
}

static int32_t dvpp_trs_sqe_update_ts(uint32_t devid, uint32_t tsid, int32_t pid, void *sqe)
{
    // do nothing
    (void)devid;
    (void)tsid;
    (void)pid;
    (void)sqe;
    return 0;
}

void dvpp_trs_sqe_update_init(void)
{
    cmdlist_case cmd_case;
    int32_t ret = 0;
    // 挂接响应tsagent的sqe update请求钩子函数
    ts_agent_dvpp_ops_t dvpp_ops = {
        .dvpp_sqe_update = dvpp_trs_sqe_update_ts,
    };
    tsagent_dvpp_register(&dvpp_ops);

    // 挂接响应iotcl的sqe update请求钩子函数，虚拟机场景
    cmd_case.cmd = GEN_CMDLIST;
    cmd_case.handler = dvpp_gen_cmdlist_handler;
    ret = dvpp_set_gen_cmdlist_func(&cmd_case);
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("dvpp_set_gen_cmdlist_func failed cmd=%d\n", (int32_t)GEN_CMDLIST);
        return;
    }
}

void dvpp_trs_sqe_update_uninit(void)
{
    tsagent_dvpp_unregister();
    dvpp_uninit_share_mem_pool();
}