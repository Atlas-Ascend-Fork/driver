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
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"

#include "ascend_hal_define.h"
#include "dpa_kernel_interface.h"
#ifdef CFG_FEATURE_SUPPORT_APM
#include "ascend_kernel_hal.h"
#endif
#ifdef CFG_FEATURE_SUPPORT_CP_PID_BY_DEVMNG
#include "dms/dms_devdrv_manager_comm.h"
#endif
#include "trs_pm_adapt.h"
#include "trs_chan_mem.h"
#include "securec.h"
#include "pbl/pbl_soc_res.h"
#include "pbl/pbl_uda.h"
#include "soc_adapt.h"
#include "trs_chan_mbox.h"

#define TRS_SQCQ_INFO_LEN ((size_t)(SQCQ_INFO_LENGTH) * (sizeof(u32)))
#define TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS 3000

typedef int (*chan_mbox_func)(struct trs_id_inst *, struct trs_chan_info *, struct trs_chan_adapt_info *);

static const u16 chan_create_mbox_cmd[CHAN_TYPE_MAX][CHAN_SUB_TYPE_MAX] = {
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_RTS] = TRS_MBOX_CREATE_CQSQ_CALC,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_TOPIC_SCHED] = TRS_MBOX_CREATE_TOPIC_SQCQ,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_DVPP] = TRS_MBOX_CREATE_KERNEL_SQCQ,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_TS] = TRS_MBOX_NOTICE_TS_SQCQ_CREATE,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_RSV_TS] = TRS_MBOX_NOTICE_TS_SQCQ_CREATE,
    [CHAN_TYPE_SW][CHAN_SUB_TYPE_SW_CTRL] = TRS_MBOX_CREATE_CTRL_CQSQ,
    [CHAN_TYPE_MAINT][CHAN_SUB_TYPE_MAINT_HB] = TRS_MBOX_CREATE_HB_SQCQ,
    [CHAN_TYPE_MAINT][CHAN_SUB_TYPE_MAINT_DBG] = TRS_MBOX_DBG_CQSQ_CREATE,
    [CHAN_TYPE_TASK_SCHED][CHAN_SUB_TYPE_TASK_SCHED_ASYNC_CB] = TRS_MBOX_CREATE_TASKSCHED_SQCQ,
};

static const u16 chan_release_mbox_cmd[CHAN_TYPE_MAX][CHAN_SUB_TYPE_MAX] = {
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_RTS] = TRS_MBOX_RELEASE_CQSQ_CALC,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_TOPIC_SCHED] = TRS_MBOX_RELEASE_TOPIC_SQCQ,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_DVPP] = TRS_MBOX_RELEASE_KERNEL_SQCQ,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_TS] = TRS_MBOX_NOTICE_TS_SQCQ_FREE,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_RSV_TS] = TRS_MBOX_NOTICE_TS_SQCQ_FREE,
    [CHAN_TYPE_SW][CHAN_SUB_TYPE_SW_CTRL] = TRS_MBOX_RELEASE_CTRL_CQSQ,
    [CHAN_TYPE_MAINT][CHAN_SUB_TYPE_MAINT_HB] = TRS_MBOX_RELEASE_HB_SQCQ,
    [CHAN_TYPE_MAINT][CHAN_SUB_TYPE_MAINT_DBG] = TRS_MBOX_DBG_CQSQ_RELEASE,
    [CHAN_TYPE_TASK_SCHED][CHAN_SUB_TYPE_TASK_SCHED_ASYNC_CB] = TRS_MBOX_RELEASE_TASKSCHED_SQCQ,
};

static u16 trs_chan_ops_get_mbox_cmd(u32 op, u32 type, u32 sub_type)
{
    return (op != 0) ? chan_create_mbox_cmd[type][sub_type] : chan_release_mbox_cmd[type][sub_type];
}

int trs_chan_ops_get_hw_irq(struct trs_id_inst *inst, int irq_type, u32 irq, u32 *hwirq)
{
    struct res_inst_info res_inst;

    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);
    return soc_resmng_get_hwirq(&res_inst, (u32)irq_type, irq, hwirq);
}

static void trs_chan_ops_hw_create_mbox_pack_host_streamid(struct trs_id_inst *inst, u32 *host_ssid)
{
    u32 pcie_vfid = 0;
    int ret = 0;

    if (uda_is_phy_dev(inst->devid) == true) {
        pcie_vfid = 0;
    } else {
        struct uda_mia_dev_para mia_para = {0};
        ret = uda_udevid_to_mia_devid(inst->devid, &mia_para);
        if (ret != 0) {
            trs_warn("Get mia vf id failed. (devid=%u; ret=%d)\n", inst->devid, ret);
            pcie_vfid = 0;
        } else {
            pcie_vfid = mia_para.sub_devid + 1;
        }
    }

    *host_ssid = pcie_vfid; /* see swapbuffer host streamid */
}

static void trs_chan_ops_hw_create_mbox_fill_ext_info(struct trs_id_inst *inst, struct trs_chan_info *chan_info,
                                                      struct trs_normal_cqsq_mailbox *mbox_data)
{
    struct trs_ext_info_header *header = NULL;
#if defined(CFG_FEATURE_SUPPORT_APM) || defined(CFG_FEATURE_SUPPORT_CP_PID_BY_DEVMNG)
    int ret;
    int slave_tgid;
#endif

    if ((chan_info->ext_msg != NULL) && (chan_info->ext_msg_len > 0)) {
        mbox_data->ts_info.info = chan_info->msg;
        mbox_data->ts_info.ext_msg = chan_info->ext_msg;
        mbox_data->ts_info.ext_msg_len = chan_info->ext_msg_len;
        header = (struct trs_ext_info_header *)mbox_data->ts_info.ext_msg;
        if ((chan_info->remote_id_flag == 1) || (chan_info->agent_id_flag == 1)) {
#ifndef EMU_ST
            int master_tgid = 0;
            int hccp_tgid = 0;
            header->cp_pid = (u32)ka_task_get_current_tgid();
#ifdef CFG_FEATURE_SUPPORT_APM
            ret = apm_query_master_tgid_by_slave(ka_task_get_current_tgid(), &master_tgid);
            if (ret == 0) {
                ret = hal_kernel_apm_query_slave_tgid_by_master(master_tgid, inst->devid, PROCESS_HCCP, &hccp_tgid);
                header->hccp_pid = (ret == 0) ? hccp_tgid : 0;
            }
#endif
            trs_debug("(device_pid=%d; host_pid=%d; hccp_pid=%d)\n", ka_task_get_current_tgid(), master_tgid,
                      hccp_tgid);
#endif
        } else {
#ifdef CFG_FEATURE_SUPPORT_APM
            ret = hal_kernel_apm_query_slave_tgid_by_master(ka_task_get_current_tgid(), inst->devid, PROCESS_CP1,
                                                            &slave_tgid);
            if (ret == 0) {
                header->cp_pid = slave_tgid;
            }
            ret = hal_kernel_apm_query_slave_tgid_by_master(ka_task_get_current_tgid(), inst->devid, PROCESS_HCCP,
                                                            &slave_tgid);
            if (ret == 0) {
                header->hccp_pid = slave_tgid;
            }
#endif
#ifdef CFG_FEATURE_SUPPORT_CP_PID_BY_DEVMNG
            ret = devdrv_query_process_by_host_pid(ka_task_get_current_tgid(), inst->devid, DEVDRV_PROCESS_CP1, 0,
                                                   &slave_tgid);
            if (ret == 0) {
                header->cp_pid = slave_tgid;
            }
#endif
            trs_debug("(current=%d; cp_pid=%d; hccp_pid=%d)\n", ka_task_get_current_tgid(), header->cp_pid,
                      header->hccp_pid);
        }

        trs_chan_ops_hw_create_mbox_pack_host_streamid(inst, &header->host_ssid);
        if ((chan_info->types.sub_type == CHAN_SUB_TYPE_HW_RTS) ||
            (chan_info->types.sub_type == CHAN_SUB_TYPE_HW_TOPIC_SCHED)) {
            trs_debug("Mailbox info. (devid=%u; cmd_type=%u; sq_index=%u; cq0_index=%u; app_type=%u; sw_reg_flag=%u; "
                      "fid=%u; sq_cq_side=%u; master_pid_flag=%u; sq_addr_is_virtual=%u; cq_addr_is_virtual=%u; "
                      "is_convert_pid=%u; sqesize=%u; cqesize=%u; cqdepth=%u; sqdepth=%u; pid=%d; "
                      "cq_irq=%u; ssid=%u; info[0]=%u; ext_msg_len=%u; sub_type=%u)\n",
                      inst->devid, mbox_data->header.cmd_type, mbox_data->sq_index, mbox_data->cq0_index,
                      mbox_data->app_type, mbox_data->sw_reg_flag, mbox_data->fid, mbox_data->sq_cq_side,
                      mbox_data->master_pid_flag, mbox_data->sq_addr_is_virtual, mbox_data->cq_addr_is_virtual,
                      mbox_data->is_convert_pid, mbox_data->sqesize, mbox_data->cqesize, mbox_data->cqdepth,
                      mbox_data->sqdepth, mbox_data->pid, mbox_data->cq_irq, mbox_data->ssid,
                      mbox_data->ts_info.info[0], mbox_data->ts_info.ext_msg_len, chan_info->types.sub_type);
        }

        trs_debug("Mailbox msg. (pid=%d; device_pid=%u; is_convert_pid=%u; host_ssid=%u; ext_msg_len=%u)\n",
                  mbox_data->pid, header->cp_pid, mbox_data->is_convert_pid, header->host_ssid,
                  mbox_data->ts_info.ext_msg_len);
    }
}

static int trs_chan_ops_hw_create_mbox_send(struct trs_id_inst *inst, struct trs_chan_info *chan_info,
                                            struct trs_chan_adapt_info *adapt)
{
    struct trs_normal_cqsq_mailbox mbox_data;
    u32 cq_irq;
    u16 cmd;
    int ret;

    cmd = trs_chan_ops_get_mbox_cmd(chan_info->op, chan_info->types.type, chan_info->types.sub_type);
    trs_mbox_init_header(&mbox_data.header, cmd);

    ret = trs_chan_ops_get_hw_irq(inst, chan_info->irq_type, chan_info->cq_info.irq, &cq_irq);
    if (ret != 0) {
        trs_debug("Without hwirq. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
    }

    mbox_data.cq_irq = (u16)cq_irq;

    mbox_data.ssid = chan_info->ssid;
    mbox_data.app_type = adapt->app_type;
    if (adapt->app_type == TRS_MBOX_SEND_FROM_DEVICE) {
        mbox_data.app_type = (adapt->cq_side == 1) ? TRS_MBOX_SEND_FROM_HOST : TRS_MBOX_SEND_FROM_DEVICE;
    }
    mbox_data.sw_reg_flag = (cmd == TRS_MBOX_CREATE_CTRL_CQSQ) ? 1 : 0;
    mbox_data.pid = chan_info->pid;
    mbox_data.master_pid_flag = (chan_info->master_pid_flag == 1) ? (adapt->cq_side == 0) : 0;
    mbox_data.sq_index = chan_info->sq_info.sqid;
    mbox_data.sq_addr_is_virtual = chan_info->sq_addr_is_virtual;
    mbox_data.sq_addr = chan_info->sq_addr_is_virtual ? (u64)(uintptr_t)chan_info->sq_info.sq_para.sq_que_uva :
                                                        chan_info->sq_info.sq_phy_addr;
    mbox_data.sqesize = chan_info->sq_info.sq_para.sqe_size;
    mbox_data.sqdepth = chan_info->sq_info.sq_para.sq_depth;
    mbox_data.cq0_index = chan_info->cq_info.cqid;
    mbox_data.cq0_addr = chan_info->cq_info.cq_phy_addr;
    mbox_data.cqdepth = chan_info->cq_info.cq_para.cq_depth;
    mbox_data.cqesize = chan_info->cq_info.cq_para.cqe_size;
    mbox_data.sq_cq_side = ((adapt->sq_side << TRS_CHAN_SQ_MEM_OFFSET) | (adapt->cq_side << TRS_CHAN_CQ_MEM_OFFSET));
    mbox_data.cq_addr_is_virtual = 0;
    mbox_data.is_convert_pid = (chan_info->remote_id_flag == 1) ? 0 : 1;
    if (chan_info->types.sub_type == CHAN_SUB_TYPE_HW_TOPIC_SCHED) {
        mbox_data.is_convert_pid = 0;
    }
    mbox_data.fid = adapt->fid;

    ret = memcpy_s(mbox_data.info, TRS_SQCQ_INFO_LEN, chan_info->msg, TRS_SQCQ_INFO_LEN);
    if (ret != EOK) {
        return ret;
    }

    trs_chan_ops_hw_create_mbox_fill_ext_info(inst, chan_info, &mbox_data);

    ret = trs_mbox_send(inst, 0, &mbox_data, sizeof(struct trs_normal_cqsq_mailbox), TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);
    if ((ret != 0) || (mbox_data.header.result != 0)) {
        trs_err("Mbox send fail. (devid=%u; tsid=%u; cmd=%u; result=%u; ret=%d)\n", inst->devid, inst->tsid, cmd,
                mbox_data.header.result, ret);
        ret = -EFAULT;
    }
    return ret;
}

static int trs_chan_ops_hw_release_mbox_send(struct trs_id_inst *inst, struct trs_chan_info *chan_info,
                                             struct trs_chan_adapt_info *adapt)
{
    struct trs_normal_cqsq_mailbox mbox_data;
    u16 cmd;
    int ret;

    cmd = trs_chan_ops_get_mbox_cmd(chan_info->op, chan_info->types.type, chan_info->types.sub_type);
    trs_mbox_init_header(&mbox_data.header, cmd);

    mbox_data.pid = chan_info->pid;
    mbox_data.sq_index = chan_info->sq_info.sqid;
    mbox_data.sq_addr = 0;
    mbox_data.cq0_index = chan_info->cq_info.cqid;
    mbox_data.cq0_addr = 0;
    mbox_data.app_type = adapt->app_type;
    mbox_data.fid = adapt->fid;

    ret = memcpy_s(mbox_data.info, TRS_SQCQ_INFO_LEN, chan_info->msg, TRS_SQCQ_INFO_LEN);
    if (ret != EOK) {
        trs_err("Memcpy fail. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }

    if ((chan_info->ext_msg != NULL) && (chan_info->ext_msg_len > 0)) {
        mbox_data.ts_info.info = chan_info->msg;
        mbox_data.ts_info.ext_msg = NULL; /* hisi stars not need ext_msg when release sqcq */
        mbox_data.ts_info.ext_msg_len = 0;
    }

    ret = trs_mbox_send(inst, 0, &mbox_data, sizeof(struct trs_normal_cqsq_mailbox), TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);
    if (ret != 0 || mbox_data.header.result != 0) {
        if (ret == -ENXIO) {
            trs_warn("Mb chan does not exist. (devid=%u; tsid=%u; cmd=%u; ret=%d)\n", inst->devid, inst->tsid, cmd,
                     ret);
        } else {
            trs_err("Mbox send fail. (devid=%u; tsid=%u; cmd=%u; result=%u; ret=%d)\n", inst->devid, inst->tsid, cmd,
                    mbox_data.header.result, ret);
            ret = -EFAULT;
        }
    }
    return ret;
}

static int trs_chan_ops_maint_create_mbox_send(struct trs_id_inst *inst, struct trs_chan_info *chan_info,
                                               struct trs_chan_adapt_info *adapt)
{
    struct trs_maint_sqcq_mbox mbox_data;
    u32 cq_irq;
    int ret;
    u16 cmd;

    ret = trs_chan_ops_get_hw_irq(inst, chan_info->irq_type, chan_info->cq_info.irq, &cq_irq);
    if (ret != 0) {
        trs_debug("Without hwirq. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
    }

    mbox_data.cq_irq = (u16)cq_irq;

    cmd = trs_chan_ops_get_mbox_cmd(chan_info->op, chan_info->types.type, chan_info->types.sub_type);
    trs_mbox_init_header(&mbox_data.header, cmd);

    mbox_data.sq_index = chan_info->sq_info.sqid;
    mbox_data.sq_addr = chan_info->sq_info.sq_phy_addr;

    mbox_data.cq0_index = chan_info->cq_info.cqid;
    mbox_data.cq0_addr = chan_info->cq_info.cq_phy_addr;
    mbox_data.plat_type = adapt->app_type;
    if (chan_info->types.sub_type == CHAN_SUB_TYPE_MAINT_DBG) {
        mbox_data.cq1_addr = chan_info->sq_info.sq_para.sqe_size * chan_info->sq_info.sq_para.sq_depth;
        mbox_data.cq2_addr = chan_info->cq_info.cq_para.cqe_size * chan_info->cq_info.cq_para.cq_depth;
        mbox_data.cq3_addr = chan_info->pid;
    }

    ret = trs_mbox_send(inst, 0, &mbox_data, sizeof(struct trs_maint_sqcq_mbox), TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);
    if ((ret != 0) || (mbox_data.header.result != 0)) {
        trs_err("Mbox send fail. (devid=%u; tsid=%u; cmd=%u; result=%u; ret=%d)\n", inst->devid, inst->tsid, cmd,
                mbox_data.header.result, ret);
        ret = -EFAULT;
    }
    return ret;
}

static int trs_chan_ops_maint_release_mbox_send(struct trs_id_inst *inst, struct trs_chan_info *chan_info,
                                                struct trs_chan_adapt_info *adapt)
{
    struct trs_maint_sqcq_mbox mbox_data;
    int ret;
    u16 cmd;

    cmd = trs_chan_ops_get_mbox_cmd(chan_info->op, chan_info->types.type, chan_info->types.sub_type);
    trs_mbox_init_header(&mbox_data.header, cmd);

    mbox_data.sq_index = chan_info->sq_info.sqid;
    mbox_data.sq_addr = 0;

    mbox_data.cq0_index = chan_info->cq_info.cqid;
    mbox_data.cq0_addr = 0;
    mbox_data.plat_type = adapt->app_type;

    ret = trs_mbox_send(inst, 0, &mbox_data, sizeof(struct trs_maint_sqcq_mbox), TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);
    if ((ret != 0) || (mbox_data.header.result != 0)) {
        trs_err("Mbox send fail. (devid=%u; tsid=%u; cmd=%u; result=%u; ret=%d)\n", inst->devid, inst->tsid, cmd,
                mbox_data.header.result, ret);
        ret = -EFAULT;
    }
    return ret;
}

static int trs_chan_ops_hw_ts_mbox_send(struct trs_id_inst *inst, struct trs_chan_info *chan_info,
                                        struct trs_chan_adapt_info *adapt)
{
    u32 chip_type = uda_get_chip_type(inst->devid);
    struct trs_ts_sqcq_mbox mbox_data;
    u16 cmd;
    int ret;

    if (!trs_soc_support_hw_ts_mbox(inst)) {
        return -EOPNOTSUPP;
    }

    cmd = trs_chan_ops_get_mbox_cmd(chan_info->op, chan_info->types.type, chan_info->types.sub_type);
    trs_mbox_init_header(&mbox_data.header, cmd);
    mbox_data.sqid = chan_info->sq_info.sqid;
    mbox_data.cqid = chan_info->cq_info.cqid;
    mbox_data.pid = chan_info->pid;
    mbox_data.vfid = adapt->fid;
    mbox_data.ssid = chan_info->ssid;
    mbox_data.app_type = adapt->app_type;

    if ((chip_type == HISI_CLOUD_V4) || (chip_type == HISI_MINI_V4)) {
        mbox_data.sq_addr = chan_info->sq_info.sq_phy_addr;
        mbox_data.sqesize = chan_info->sq_info.sq_para.sqe_size;
        mbox_data.sqdepth = chan_info->sq_info.sq_para.sq_depth;
        mbox_data.cq_addr = chan_info->cq_info.cq_phy_addr;
        mbox_data.cqesize = chan_info->cq_info.cq_para.cqe_size;
        mbox_data.cqdepth = chan_info->cq_info.cq_para.cq_depth;
    } else {
        ret = memcpy_s(mbox_data.info, TRS_SQCQ_INFO_LEN, chan_info->msg, TRS_SQCQ_INFO_LEN);
        if (ret != EOK) {
            trs_err("Memcpy fail. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
            return ret;
        }
    }

    if (chan_info->types.sub_type == CHAN_SUB_TYPE_HW_RSV_TS) {
        ret = trs_mbox_send_ex(inst, 0, &mbox_data, sizeof(struct trs_ts_sqcq_mbox), TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);
    } else {
        ret = trs_mbox_send(inst, 0, &mbox_data, sizeof(struct trs_ts_sqcq_mbox), TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);
    }

    if (ret != 0 || mbox_data.header.result != 0) {
        trs_err("Mbox send fail. (devid=%u; tsid=%u; cmd=%u; result=%u; ret=%d)\n", inst->devid, inst->tsid, cmd,
                mbox_data.header.result, ret);
        ret = -EFAULT;
    }
    return ret;
}

static int trs_chan_ops_task_sched_create_mbox_send(struct trs_id_inst *inst, struct trs_chan_info *chan_info,
                                                    struct trs_chan_adapt_info *adapt)
{
    struct trs_task_sched_sqcq_alloc_mbox mbox_data;
    u16 cmd;
    int ret;

    cmd = trs_chan_ops_get_mbox_cmd(chan_info->op, chan_info->types.type, chan_info->types.sub_type);
    trs_mbox_init_header(&mbox_data.header, cmd);

    ret = trs_chan_ops_get_hw_irq(inst, chan_info->irq_type, chan_info->cq_info.irq, &mbox_data.cq_irq);
    if (ret != 0) {
        trs_err("Get hwirq fail. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return ret;
    }

    mbox_data.plat_type = adapt->app_type;

    mbox_data.sq_index = chan_info->sq_info.sqid;
    mbox_data.sq_addr = chan_info->sq_info.sq_phy_addr;
    mbox_data.sqe_size = chan_info->sq_info.sq_para.sqe_size;
    mbox_data.sq_depth = chan_info->sq_info.sq_para.sq_depth;

    mbox_data.cq_index = chan_info->cq_info.cqid;
    mbox_data.cq_addr = chan_info->cq_info.cq_phy_addr;
    mbox_data.cqe_size = chan_info->cq_info.cq_para.cqe_size;
    mbox_data.cq_depth = chan_info->cq_info.cq_para.cq_depth;

    ret = trs_mbox_send(inst, 0, &mbox_data, sizeof(mbox_data), TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);
    if (ret != 0 || mbox_data.header.result != 0) {
        trs_err("Mbox send fail. (devid=%u; tsid=%u; cmd=%u; result=%u; ret=%d)\n", inst->devid, inst->tsid, cmd,
                mbox_data.header.result, ret);
        ret = -EFAULT;
    }

    return ret;
}

static int trs_chan_ops_task_sched_release_mbox_send(struct trs_id_inst *inst, struct trs_chan_info *chan_info,
                                                     struct trs_chan_adapt_info *adapt)
{
    struct trs_task_sched_sqcq_free_mbox mbox_data;
    u16 cmd;
    int ret;

    cmd = trs_chan_ops_get_mbox_cmd(chan_info->op, chan_info->types.type, chan_info->types.sub_type);
    trs_mbox_init_header(&mbox_data.header, cmd);

    mbox_data.plat_type = adapt->app_type;
    mbox_data.sq_index = chan_info->sq_info.sqid;
    mbox_data.cq_index = chan_info->cq_info.cqid;

    ret = trs_mbox_send(inst, 0, &mbox_data, sizeof(mbox_data), TRS_DEVICE_CHAN_MBOX_TIMEOUT_MS);
    if (ret != 0 || mbox_data.header.result != 0) {
        trs_err("Mbox send fail. (devid=%u; tsid=%u; cmd=%u; result=%u; ret=%d)\n", inst->devid, inst->tsid, cmd,
                mbox_data.header.result, ret);
        ret = -EFAULT;
    }

    return ret;
}

static const chan_mbox_func chan_create_mbox_func[CHAN_TYPE_MAX][CHAN_SUB_TYPE_MAX] = {
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_RTS] = trs_chan_ops_hw_create_mbox_send,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_TOPIC_SCHED] = trs_chan_ops_hw_create_mbox_send,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_DVPP] = trs_chan_ops_hw_create_mbox_send,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_TS] = trs_chan_ops_hw_ts_mbox_send,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_RSV_TS] = trs_chan_ops_hw_ts_mbox_send,
    [CHAN_TYPE_SW][CHAN_SUB_TYPE_SW_CTRL] = trs_chan_ops_hw_create_mbox_send,
    [CHAN_TYPE_MAINT][CHAN_SUB_TYPE_MAINT_HB] = trs_chan_ops_maint_create_mbox_send,
    [CHAN_TYPE_MAINT][CHAN_SUB_TYPE_MAINT_DBG] = trs_chan_ops_maint_create_mbox_send,
    [CHAN_TYPE_TASK_SCHED][CHAN_SUB_TYPE_TASK_SCHED_ASYNC_CB] = trs_chan_ops_task_sched_create_mbox_send,
};

static const chan_mbox_func chan_release_mbox_func[CHAN_TYPE_MAX][CHAN_SUB_TYPE_MAX] = {
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_RTS] = trs_chan_ops_hw_release_mbox_send,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_TOPIC_SCHED] = trs_chan_ops_hw_release_mbox_send,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_DVPP] = trs_chan_ops_hw_release_mbox_send,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_TS] = trs_chan_ops_hw_ts_mbox_send,
    [CHAN_TYPE_HW][CHAN_SUB_TYPE_HW_RSV_TS] = trs_chan_ops_hw_ts_mbox_send,
    [CHAN_TYPE_SW][CHAN_SUB_TYPE_SW_CTRL] = trs_chan_ops_hw_release_mbox_send,
    [CHAN_TYPE_MAINT][CHAN_SUB_TYPE_MAINT_HB] = trs_chan_ops_maint_release_mbox_send,
    [CHAN_TYPE_MAINT][CHAN_SUB_TYPE_MAINT_DBG] = trs_chan_ops_maint_release_mbox_send,
    [CHAN_TYPE_TASK_SCHED][CHAN_SUB_TYPE_TASK_SCHED_ASYNC_CB] = trs_chan_ops_task_sched_release_mbox_send,
};

int trs_chan_mbox_send(struct trs_id_inst *inst, struct trs_chan_info *chan_info, struct trs_chan_adapt_info *adapt)
{
    u32 sub_type = chan_info->types.sub_type;
    u32 type = chan_info->types.type;
    chan_mbox_func mbox_func;
    int ret = 0;

    if (chan_info->op != 0) {
        mbox_func = chan_create_mbox_func[type][sub_type];
    } else {
        mbox_func = chan_release_mbox_func[type][sub_type];
    }

    if (mbox_func != NULL) {
        ret = mbox_func(inst, chan_info, adapt);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(trs_chan_mbox_send);
