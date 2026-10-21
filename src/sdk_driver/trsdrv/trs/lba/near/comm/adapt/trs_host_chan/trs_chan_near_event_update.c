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
#include "ka_memory_pub.h"
#include "trs_pub_def.h"
#ifndef EMU_ST
#include "comm_kernel_interface.h"
#include "ascend_hal_define.h"
#include "esched_kernel_interface.h"
#include "trs_event.h"
#include "trs_id.h"
#include "trs_chan_near_event_update.h"
#ifdef CFG_FEATURE_TRS_SIA_ADAPT
#include "trs_sia_adapt_auto_init.h"
#elif defined(CFG_FEATURE_TRS_SEC_EH_ADAPT)
#include "trs_sec_eh_auto_init.h"
#endif

static u32 **sq_pid_lists = NULL;
static u32 **cq_pid_lists = NULL;

static u32 *trs_speicified_sqcq_id_pid_get(struct trs_id_inst *inst, int type, u32 id)
{
    u32 **sqcq_pid_lists;
    u32 max_id;
    int id_type;
    int ret;

    if (type == TRS_HW_SQ_ID || type == TRS_RSV_HW_SQ_ID) {
        sqcq_pid_lists = sq_pid_lists;
        id_type = TRS_HW_SQ_ID;
    } else if ((type == TRS_HW_CQ_ID) || (type == TRS_RSV_HW_CQ_ID)) {
        sqcq_pid_lists = cq_pid_lists;
        id_type = TRS_HW_CQ_ID;
    } else {
        trs_err("Invalid type. (devid=%u; tsid=%u; type=%d; id=%u).\n", inst->devid, inst->tsid, type, id);
        return NULL;
    }

    ret = trs_id_get_max_id(inst, id_type, &max_id);
    if (ret != 0) {
        trs_err("Failed to get max id. (devid=%u; tsid=%u; type=%d).\n", inst->devid, inst->tsid, type);
        return NULL;
    }

    if ((inst->devid >= TRS_DEV_MAX_NUM) || (id >= max_id)) {
        trs_err("Invalid para devid or id. (devid=%u; id=%u; max_id=%u; type=%d).\n", inst->devid, id, max_id, type);
        return NULL;
    }

    return ((sqcq_pid_lists != NULL) && (sqcq_pid_lists[inst->devid] != NULL)) ? &(sqcq_pid_lists[inst->devid][id]) :
                                                                                 NULL;
}

int trs_stars_v2_chan_ops_sqcq_speicified_id_alloc(struct trs_id_inst *inst, int type, u32 *id)
{
    u32 *sqcq_pid = trs_speicified_sqcq_id_pid_get(inst, type, *id);
    if (sqcq_pid == NULL) {
        return -EINVAL;
    }

    trs_debug("Check sqcq. (id=%u; type=%d; host_pid=%u)\n", *id, type, *sqcq_pid);
    return (ka_task_get_current_tgid() == *sqcq_pid) ? 0 : -EPERM;
}

int trs_stars_v2_chan_ops_sqcq_speicified_id_free(struct trs_id_inst *inst, int type, u32 id)
{
    u32 *sqcq_pid = trs_speicified_sqcq_id_pid_get(inst, type, id);
    if (sqcq_pid == NULL) {
        return -EINVAL;
    }

    *sqcq_pid = 0;
    return 0;
}

int _trs_sqcq_event_update(u32 devid, struct sched_published_event_info *event_info,
                           struct sched_published_event_func *event_func)
{
    struct halSqCqOutputInfo *info = NULL;
    struct trs_id_inst inst;
    u32 *sq_pid, *cq_pid;
    u32 host_pid;

    if (event_info->subevent_id != DRV_SUBEVENT_TRS_ALLOC_SQCQ_WITH_URMA_MSG) {
        return 0;
    }

    if (event_info->msg == NULL) {
        trs_err("Invalid event info msg. (devid=%u)\n", devid);
        return -EINVAL;
    }

    if (event_info->msg_len < (sizeof(int) + sizeof(struct halSqCqOutputInfo) + sizeof(u32))) { /* device proc failed */
        trs_warn("Msg is incomplete. (msg_is_NULL=%d; msg_len=%u)\n", (event_info->msg == NULL), event_info->msg_len);
        return 0;
    }

    info = (struct halSqCqOutputInfo *)(event_info->msg + sizeof(int));
    host_pid = *(u32 *)(event_info->msg + sizeof(int) + sizeof(struct halSqCqOutputInfo));

    trs_id_inst_pack(&inst, devid, 0);
    sq_pid = trs_speicified_sqcq_id_pid_get(&inst, TRS_HW_SQ_ID, info->sqId);
    cq_pid = trs_speicified_sqcq_id_pid_get(&inst, TRS_HW_CQ_ID, info->cqId);
    if ((sq_pid == NULL) || (cq_pid == NULL)) {
        trs_err("Failed to get pid. (devid=%u; sqid=%u; cqid=%u; hostpid=%u)\n", devid, info->sqId, info->cqId,
                host_pid);
        return 0;
    }

    *sq_pid = host_pid;
    *cq_pid = host_pid;

    trs_debug("Add remote sq and cq to pid_list. (sqid=%u; cqid=%u; host_pid=%u)\n", info->sqId, info->cqId, host_pid);
    return 0;
}

int trs_sqcq_event_update(u32 devid, struct sched_published_event_info *event_info,
                          struct sched_published_event_func *event_func)
{
    int ret = _trs_sqcq_event_update(devid, event_info, event_func);
    return trs_event_kerror_to_uerror(ret);
}

int trs_sqcq_event_dev_init(u32 ts_inst_id)
{
    struct trs_id_inst inst;
    u32 sq_max, cq_max;
    int ret, connection_type;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);

    connection_type = devdrv_get_connect_protocol(inst.devid);
    if (connection_type != CONNECT_PROTOCOL_UB) {
        trs_info("No need to init sqcq event. (devid=%u; connection_type=%d)\n", inst.devid, connection_type);
        return 0;
    }
    ret = trs_id_get_max_id(&inst, TRS_HW_SQ_ID, &sq_max);
    ret |= trs_id_get_max_id(&inst, TRS_HW_CQ_ID, &cq_max);
    if (ret != 0) {
        trs_err("Failed to get sq cq max id. (devid=%u)\n", inst.devid);
        return ret;
    }

    sq_pid_lists[inst.devid] = (u32 *)ka_mm_vzalloc(sizeof(u32) * sq_max);
    if (sq_pid_lists[inst.devid] == NULL) {
        trs_err("Failed to alloc sq_pid_lists. (devid=%u; sq_max=%u)\n", inst.devid, sq_max);
        return -ENOMEM;
    }

    cq_pid_lists[inst.devid] = (u32 *)ka_mm_vzalloc(sizeof(u32) * cq_max);
    if (cq_pid_lists[inst.devid] == NULL) {
        ka_mm_vfree(sq_pid_lists[inst.devid]);
        sq_pid_lists[inst.devid] = NULL;
        trs_err("Failed to alloc cq_pid_lists. (devid=%u; cq_max=%u)\n", inst.devid, cq_max);
        return -ENOMEM;
    }

    trs_info("Alloc sqcq pid_list success. (devid=%u; sq_max=%u; cq_max=%u)\n", inst.devid, sq_max, cq_max);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(trs_sqcq_event_dev_init, FEATURE_LOADER_STAGE_6);

void trs_sqcq_event_dev_uninit(u32 ts_inst_id)
{
    struct trs_id_inst inst;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (sq_pid_lists == NULL) {
#ifndef EMU_ST
        return;
#endif
    }
    if (sq_pid_lists[inst.devid] != NULL) {
        ka_mm_vfree(sq_pid_lists[inst.devid]);
        sq_pid_lists[inst.devid] = NULL;
    }
    if (cq_pid_lists[inst.devid] != NULL) {
        ka_mm_vfree(cq_pid_lists[inst.devid]);
        cq_pid_lists[inst.devid] = NULL;
    }
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(trs_sqcq_event_dev_uninit, FEATURE_LOADER_STAGE_6);

/* only host need */
int trs_sqcq_event_init(void)
{
    sq_pid_lists = (u32 **)ka_mm_vzalloc(sizeof(u32 *) * TRS_DEV_MAX_NUM);
    if (sq_pid_lists == NULL) {
        return -ENOMEM;
    }
    cq_pid_lists = (u32 **)ka_mm_vzalloc(sizeof(u32 *) * TRS_DEV_MAX_NUM);
    if (cq_pid_lists == NULL) {
        ka_mm_vfree(sq_pid_lists);
        sq_pid_lists = NULL;
        return -ENOMEM;
    }

    return hal_kernel_sched_register_event_pre_proc_handle(EVENT_DRV_MSG, SCHED_PRE_PROC_POS_REMOTE,
                                                           trs_sqcq_event_update);
}
DECLAER_FEATURE_AUTO_INIT(trs_sqcq_event_init, FEATURE_LOADER_STAGE_2);

void trs_sqcq_event_uninit(void)
{
    hal_kernel_sched_unregister_event_pre_proc_handle(EVENT_DRV_MSG, SCHED_PRE_PROC_POS_REMOTE, trs_sqcq_event_update);

    if (sq_pid_lists != NULL) {
        ka_mm_vfree(sq_pid_lists);
        sq_pid_lists = NULL;
    }

    if (cq_pid_lists != NULL) {
        ka_mm_vfree(cq_pid_lists);
        cq_pid_lists = NULL;
    }
}
DECLAER_FEATURE_AUTO_UNINIT(trs_sqcq_event_uninit, FEATURE_LOADER_STAGE_2);
#else
int trs_stars_v2_chan_ops_sqcq_speicified_id_alloc(struct trs_id_inst *inst, int type, u32 *id)
{
    return 0;
}

int trs_stars_v2_chan_ops_sqcq_speicified_id_free(struct trs_id_inst *inst, int type, u32 id)
{
    return 0;
}
#endif
