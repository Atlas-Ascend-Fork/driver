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

#include "esched_adapt_platform.h"
#include "topic_sched_drv_common.h"
#include "esched_drv_interface.h"
#include "securec.h"
#include "esched_feature_check.h"
#include "esched_fops.h"
#include "esched.h"
#include "esched_drv_adapt.h"
#include "dpa/dpa_pids_map.h"
#include "pbl/pbl_uda.h"

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF
const ka_pci_device_id_t g_esched_driver_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd100), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd105), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xa126), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd801), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd500), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd501), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd802), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd803), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd804), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd805), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd806), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd807), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd808), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd200), 0},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20C6, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x203F, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd802, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {0x20E9, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {}};
KA_MODULE_DEVICE_TABLE(pci, g_esched_driver_tbl);

void esched_adapt_wait_trace_update(struct sched_proc_ctx *proc_ctx, int dfx_gid, int dfx_tid,
                                    struct sched_event *event)
{
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].src_wait_start_timestamp = tick_to_millisecond(
        event->timestamp.subscribe_in_kernel);
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].src_wait_end_timestamp = tick_to_millisecond(
        event->timestamp.subscribe_out_kernel);
}

void esched_adapt_publish_trace_update(struct sched_proc_ctx *proc_ctx, int dfx_gid, int dfx_tid, u64 curr_time,
                                       u32 gid, u32 event_id, u32 subevent_id)
{
    (void)gid;
    (void)event_id;
    (void)subevent_id;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].src_publish_timestamp = tick_to_millisecond(curr_time);
}

void esched_adapt_submit_trace_update(struct sched_proc_ctx *proc_ctx, int dfx_gid, int dfx_tid,
                                      struct sched_published_event_info *event_info, u64 curr_time, u32 gid,
                                      u32 event_id, u32 subevent_id)
{
    (void)memset_s(&proc_ctx->sched_dfx[dfx_gid][dfx_tid], sizeof(struct sched_sync_event_trace), 0,
                   sizeof(struct sched_sync_event_trace));
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].gid = gid;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].event_id = event_id;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].subevent_id = subevent_id;
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].src_submit_user_timestamp = tick_to_millisecond(
        event_info->publish_timestamp);
    proc_ctx->sched_dfx[dfx_gid][dfx_tid].src_submit_kernel_timestamp = tick_to_millisecond(curr_time);
}

u32 esched_publish_event_adapt_chip_id(u32 chip_id, struct sched_published_event_info *event_info)
{
    if (event_info->dst_devid != SCHED_INVALID_DEVID) {
        return event_info->dst_devid;
    }
    return chip_id;
}

bool esched_is_need_local_os_filter(u32 dst_engine)
{
    return ((dst_engine != (u32)ACPU_HOST) || (dst_engine != (u32)CCPU_HOST));
}

bool esched_adapt_is_cpu_belongs_to_vf(struct sched_numa_node *node, struct sched_cpu_ctx *cpu_ctx)
{
    (void)node;
    (void)cpu_ctx;
    return true;
}

bool esched_adapt_is_cpu_belongs_to_vf_mia(struct sched_numa_node *node, u32 vfid, struct sched_cpu_ctx *cpu_ctx)
{
    (void)node;
    (void)vfid;
    (void)cpu_ctx;
    return true;
}

void esched_adapt_query_remote_trace(u32 chip_id, struct sched_trace_input *para,
                                     struct sched_sync_event_trace *trace_result)
{
#ifndef EMU_ST
    struct sched_sync_event_trace remote_trace = {0};
    int ret;

    ret = sched_query_remote_trace_msg_send(chip_id, para, &remote_trace);
    if (ret != 0) {
        sched_info_printk_ratelimited("query remote event trace. (ret=%d, chip_id=%u; pid=%d; trace_id=%u)\n", ret,
                                      chip_id, para->dst_pid, para->msg.tid);
        return;
    }
    if (para->msg.subevent_id == remote_trace.subevent_id) {
        trace_result->dst_publish_timestamp = remote_trace.dst_publish_timestamp;
        trace_result->dst_wait_start_timestamp = remote_trace.dst_wait_start_timestamp;
        trace_result->dst_wait_end_timestamp = remote_trace.dst_wait_end_timestamp;
        trace_result->dst_submit_user_timestamp = remote_trace.dst_submit_user_timestamp;
        trace_result->dst_submit_kernel_timestamp = remote_trace.dst_submit_kernel_timestamp;
    } else {
        sched_info_printk_ratelimited(
            "remote trace query. (chip_id=%u; pid=%d; tid=%u; local_subevent=%d;remote_subevent=%d)\n", chip_id,
            para->dst_pid, para->msg.tid, trace_result->subevent_id, remote_trace.subevent_id);
    }
#else
    (void)chip_id;
    (void)para;
    (void)trace_result;
#endif
}

int esched_adapt_drv_init_sched_task_submit_chan_irq(u32 chip_id, u32 pool_id)
{
    (void)chip_id;
    (void)pool_id;
    return 0;
}

int esched_drv_get_normal_sched_submit_chan(struct sched_rtsq_res *rtsq, u32 qos, int *chan_id)
{
    int rtsq_index;
    u32 compress_qos = qos / TOPIC_SCHED_QOS_COMPRESS_RATE;
    if (rtsq->sched_rtsq[compress_qos].rtsq_num == 0) {
        return DRV_ERROR_NO_RESOURCES;
    }

    rtsq_index = ka_base_atomic_inc_return(&rtsq->sched_rtsq[compress_qos].cur_rtsq_index) %
                 rtsq->sched_rtsq[compress_qos].rtsq_num;
    *chan_id = rtsq->sched_rtsq[compress_qos].sqe_submit[rtsq_index].chan_id;

    return 0;
}

void esched_drv_set_topic_type(u8 topic_types[][DST_ENGINE_MAX])
{
    topic_types[ONLY][ACPU_LOCAL] = TOPIC_TYPE_AICPU_HOST_ONLY;
    topic_types[ONLY][CCPU_LOCAL] = TOPIC_TYPE_CCPU_HOST;
}

int esched_adapt_restore_mb_user_data_aicpu(struct topic_sched_mailbox *mb, u32 *submit_devid, u32 *tid, u32 *pid)
{
    (void)mb;
    (void)submit_devid;
    (void)tid;
    (void)pid;
    return 0;
}

#if !defined(EMU_ST) && !defined(CFG_FEATURE_STARS_V2)
int esched_drv_submit_split_event(struct sched_numa_node *node, u32 event_src,
                                  struct sched_published_event_info *event_info, u32 timeout)
{
    (void)node;
    (void)event_src;
    (void)event_info;
    (void)timeout;
    return -EFAULT;
}
#endif

int esched_adapt_proc_mnt_ns_check(u32 chip_id, struct sched_published_event_info *event_info)
{
#if !defined(EVENT_SCHED_UT) && !defined(EMU_ST)
    if ((event_info->dst_engine == CCPU_HOST) || (event_info->dst_engine == ACPU_HOST)) {
        ka_mnt_namespace_t *mns_ns = NULL;

        mns_ns = sched_get_proc_mnt_ns(chip_id, event_info->pid);
        if (mns_ns != ka_task_get_current()->nsproxy->mnt_ns) {
            sched_err("Not in the same container. (chip_id=%u; pid=%u)\n", chip_id, event_info->pid);
            return DRV_ERROR_NOT_SUPPORT;
        }
    }
#else
    (void)chip_id;
    (void)event_info;
#endif
    return 0;
}

#ifndef EMU_ST
STATIC int32_t sched_submit_check_master_pid(u32 chip_id, u32 dest_pid)
{
    u32 device_master_pid, host_master_pid;
    enum devdrv_process_type type;
    u32 dev_id, vfid;
    int32_t ret;

    ret = devdrv_query_master_pid_by_device_slave(chip_id, dest_pid, &device_master_pid);
    if (ret != 0) {
        sched_err("Query master pid by device slave failed. (chip_id=%u; dest_pid=%u; ret=%d)\n", chip_id, dest_pid,
                  ret);
        return ret;
    }

    if (device_master_pid == ka_task_get_current()->tgid) {
        return 0;
    }

    ret = hal_kernel_devdrv_query_process_host_pid(ka_task_get_current()->tgid, &dev_id, &vfid, &host_master_pid,
                                                   &type);
    if (ret != 0) {
        sched_err("Query master tgid by host slave failed. (chip_id=%u; dest_pid=%u; device_master_pid=%u; ret=%d)\n",
                  chip_id, dest_pid, device_master_pid, ret);
        return ret;
    }

    if (device_master_pid != host_master_pid) {
        sched_err("Pid does not match. (chip_id=%u; dest_pid=%u; device_master_pid=%u; host_master_pid=%u)\n", chip_id,
                  dest_pid, device_master_pid, host_master_pid);
        return DRV_ERROR_INNER_ERR;
    }

    return 0;
}
#endif

int esched_adapt_submit_check_master_pid(u32 devid, struct sched_published_event_info *event_info)
{
#ifndef EMU_ST
    u32 target_devid;
    int32_t ret;

    if (event_info->dst_engine != ACPU_DEVICE) {
        return 0;
    }
    /*
     * devid may be the host virtual device, whose feature cache is never populated
     * (only real devices go through esched_feature_init_by_devid on online).
     * Use the real destination device for both the feature check and the pid check.
     */
    target_devid = (devid == uda_get_host_id()) ? event_info->dst_devid : devid;
    if (!esched_has_feature(target_devid, ESCHED_SOC_PLATFORM_CLOUD_V4)) {
        return 0;
    }
    ret = sched_submit_check_master_pid(target_devid, event_info->pid);
    if (ret != 0) {
        return DRV_ERROR_INNER_ERR;
    }
#else
    (void)devid;
    (void)event_info;
#endif
    return 0;
}

int esched_create_default_dev(void)
{
    struct sched_dev_ops ops;
    int ret = 0;

    esched_feature_system_init();
    esched_setup_dev_soft_ops(&ops);
#ifdef CFG_SOC_PLATFORM_CLOUD_V4
    ret = esched_create_dev(uda_get_host_id(), &ops);
    if (ret != 0) {
        sched_err("Create dev failed. (devid=%u)\n", uda_get_host_id());
        return ret;
    }
#endif

#ifndef CFG_FEATURE_EXTERNAL_CDEV
    ret = esched_create_dev(0, &ops);
    if (ret != 0) {
#ifdef CFG_SOC_PLATFORM_CLOUD_V4
        esched_destroy_dev(uda_get_host_id());
#endif
        sched_err("Create dev failed.\n");
        return ret;
    }
#endif

    return ret;
}

void esched_destroy_default_dev(void)
{
#ifdef CFG_SOC_PLATFORM_CLOUD_V4
    esched_destroy_dev(uda_get_host_id());
#endif

#ifndef CFG_FEATURE_EXTERNAL_CDEV
    esched_destroy_dev(0);
#endif
    esched_feature_uninit();
}

int esched_uda_online_register(void)
{
    return 0;
}

void esched_uda_online_unregister(void)
{}

void esched_vfio_mia_init(void)
{}

void esched_vfio_mia_uninit(void)
{}

bool esched_dst_engine_is_local(u32 dst_engine)
{
#ifndef CFG_FEATURE_REMOTE_SUBMIT
    if ((dst_engine == TS_CPU) || (dst_engine == DVPP_CPU)) {
        return false;
    }
    return true;
#else
    if ((dst_engine == ACPU_LOCAL) || (dst_engine == CCPU_LOCAL)) {
        return true;
    }

    if ((dst_engine == ACPU_HOST) || (dst_engine == CCPU_HOST)) {
        return true;
    }

    /* set local flag false when dst_engine is TS_CPU or DVPP_CPU */
    return false;
#endif
}

void query_host_pid(struct sched_proc_ctx *proc_ctx)
{
    (void)proc_ctx;
}

void esched_adapt_exit_check_sched_cpu(struct sched_numa_node *node, struct sched_proc_ctx *proc_ctx)
{
    if (esched_has_feature(node->node_id, ESCHED_FEATURE_HARDWARE_SCHED)) {
        if (node->hard_res.cpu_work_mode != STARS_WORK_MODE_MSGQ) {
            node->ops.task_exit_check_sched_cpu(node, proc_ctx);
        }
    } else {
        node->ops.task_exit_check_sched_cpu(node, proc_ctx);
    }
}

int esched_adapt_check_swapout_support(u32 chip_id, int timeout)
{
#if !defined CFG_FEATURE_THREAD_SWAPOUT && !defined CFG_FEATURE_HARD_SOFT_SCHED
    if (esched_has_feature(chip_id, ESCHED_FEATURE_HARDWARE_SCHED) && (timeout == SCHED_THREAD_SWAPOUT)) {
#ifndef EMU_ST
        sched_warn("Swapout thread_ctx from cpu not supported yet.\n");
#endif
        return DRV_ERROR_NOT_SUPPORT;
    }
#endif
    return 0;
}

int esched_adapt_check_hw_sched_msgq(u32 chip_id, struct sched_numa_node *node, struct sched_grp_ctx *grp_ctx,
                                     struct sched_proc_ctx *proc_ctx, int32_t pid, u32 gid)
{
    if (esched_has_feature(chip_id, ESCHED_FEATURE_HARDWARE_SCHED) && (grp_ctx->sched_mode == SCHED_MODE_SCHED_CPU) &&
        (node->hard_res.cpu_work_mode == STARS_WORK_MODE_MSGQ)) {
        sched_err("The work mode is msgq. (chip_id=%u, pid=%d, gid=%u)\n", chip_id, pid, gid);
        esched_proc_put(proc_ctx);
        return DRV_ERROR_NOT_SUPPORT;
    }
    return 0;
}

/*
 * Inherit the hook function of the wait event in the split operator scenario.
 */
void esched_adapt_fill_exact_event_priv(u32 chip_id, struct sched_event *event, struct sched_thread_ctx *thread_ctx)
{
    if (!esched_has_feature(chip_id, ESCHED_FEATURE_HARDWARE_SCHED)) {
        event->priv = thread_ctx->event->priv;
    }
}

void esched_adapt_init_sched_cpu_ctx(struct sched_numa_node *node, u32 cpuid)
{
    if (!esched_has_feature(node->node_id, ESCHED_FEATURE_HARDWARE_SCHED)) {
        struct sched_cpu_ctx *cpu_ctx = sched_get_cpu_ctx(node, cpuid);

        cpu_ctx->sample = (struct sched_cpu_sample *)sched_vzalloc(sizeof(struct sched_cpu_sample));
        if (cpu_ctx->sample == NULL) {
#ifndef EMU_ST
            sched_warn("Malloc sample warn. (size=0x%lx)\n", sizeof(struct sched_cpu_sample));
#endif
        }

        cpu_ctx->sched_trace = (struct sched_cpu_sched_trace *)sched_vzalloc(sizeof(struct sched_cpu_sched_trace));
        if (cpu_ctx->sched_trace == NULL) {
#ifndef EMU_ST
            sched_warn("Malloc sched_trace warn . (size=0x%lx)\n", sizeof(struct sched_cpu_sched_trace));
#endif
        }
    }
}

/*
 * On host, CLOUD_V4 with UB connect protocol skips the remaining wait-event checks.
 * Returns non-zero when the caller should return success immediately.
 */
int esched_adapt_wait_event_ub_precheck(u32 chip_id)
{
    if (esched_has_feature(chip_id, ESCHED_SOC_PLATFORM_CLOUD_V4) &&
        (devdrv_get_connect_protocol(chip_id) == CONNECT_PROTOCOL_UB)) {
        return 1;
    }
    return 0;
}

/*
 * On CLOUD_V4 with UB connect protocol, clear the swapout flag when the thread
 * enters a normal wait (not a swapout request).
 */
void esched_adapt_set_wait_swapout_flag(u32 chip_id, struct sched_thread_ctx *thread_ctx)
{
#ifdef CFG_FEATURE_THREAD_SWAPOUT
    if (esched_has_feature(chip_id, ESCHED_SOC_PLATFORM_CLOUD_V4) &&
        (devdrv_get_connect_protocol(chip_id) == CONNECT_PROTOCOL_UB)) {
        thread_ctx->swapout_flag = SCHED_INVALID;
    }
#else
    (void)chip_id;
    (void)thread_ctx;
#endif
}

void esched_adapt_clear_event_finish_func(u32 chip_id, struct sched_cpu_ctx *cpu_ctx,
                                          struct sched_thread_ctx *thread_ctx)
{
    (void)chip_id;
    (void)cpu_ctx;
    (void)thread_ctx;
}

bool esched_adapt_guard_cpu_skip(struct sched_numa_node *node, struct sched_cpu_ctx *cpu_ctx)
{
    if (esched_has_feature(node->node_id, ESCHED_FEATURE_HARDWARE_MIA) &&
        !esched_adapt_is_cpu_belongs_to_vf(node, cpu_ctx)) {
        return true;
    }
    return false;
}

int esched_adapt_publish_event_to_sched_grp(u32 chip_id, struct sched_event *event, struct sched_grp_ctx *grp_ctx)
{
    if (esched_has_feature(chip_id, ESCHED_FEATURE_SOFT_NON_SCHED_ONLY)) {
        return DRV_ERROR_NOT_SUPPORT;
    }
    return sched_publish_event_to_sched_grp(event, grp_ctx);
}

bool esched_adapt_has_sia_map_topic_table(u32 chip_id)
{
    return (esched_has_feature(chip_id, ESCHED_FEATURE_SIA_MAP_TOPIC_TABLE) != 0);
}

int esched_adapt_publish_event_to_nonlocal(u32 dev_id, u32 event_src, struct sched_published_event_info *event_info,
                                           struct sched_published_event_func *event_func)
{
    if (esched_has_feature(dev_id, ESCHED_FEATURE_REMOTE_PUB_HARD_SCHED)) {
        return esched_publish_event_to_topic(dev_id, event_src, event_info, event_func);
    }
#ifdef CFG_FEATURE_REMOTE_SUBMIT
    esched_submit_trace_update(dev_id, event_src, event_info);
    return sched_publish_event_to_remote(dev_id, event_src, event_info, event_func);
#else
    (void)event_src;
    (void)event_info;
    (void)event_func;
    return 0;
#endif
}

bool esched_adapt_is_soc_miniv3(u32 chip_id)
{
    return (esched_has_feature(chip_id, ESCHED_SOC_PLATFORM_MINIV3) != 0);
}

bool esched_adapt_has_hard_soft_sched(void)
{
    return false;
}

bool esched_adapt_has_hardware_mia(u32 chip_id)
{
    return (esched_has_feature(chip_id, ESCHED_FEATURE_HARDWARE_MIA) != 0);
}

bool esched_adapt_is_soc_cloud_v4(u32 chip_id)
{
    return (esched_has_feature(chip_id, ESCHED_SOC_PLATFORM_CLOUD_V4) != 0);
}

bool esched_adapt_need_clear_publish_tid(u32 chip_id, u32 event_id)
{
    return (!esched_adapt_is_soc_cloud_v4(chip_id)) || (event_id != EVENT_TS_CALLBACK_MSG);
}

bool esched_adapt_release_task_on_close(u32 devid)
{
    /* On CLOUD_V4 the task is released by the pre_release notifier instead. */
    return (esched_has_feature(devid, ESCHED_SOC_PLATFORM_CLOUD_V4) == 0);
}

void esched_adapt_setup_dev_hw_ops_host(struct sched_dev_ops *ops)
{
    ops->sched_cpu_get_next_thread = NULL;
    ops->sched_cpu_to_idle = NULL;
    ops->sched_cpu_get_event = NULL;
    ops->conf_sched_cpu = NULL;
}

void esched_adapt_setup_dev_hw_ops_device(struct sched_dev_ops *ops)
{
    (void)ops;
}

int32_t sched_fop_set_sched_cpu(u32 devid, unsigned long arg)
{
    (void)devid;
    (void)arg;
    return DRV_ERROR_INVALID_DEVICE;
}

static void esched_fill_feature_bitmap(unsigned char *bitmaps, unsigned int ori_feature_num, unsigned int feature_num)
{
    const struct esched_chip_features *feat;
    unsigned int i;
    unsigned int j;

    for (i = 0; i < ESCHED_FEATURE_DEV_NUM; i++) {
        feat = esched_get_dev_features(i);
        if (feat == NULL) {
            /* No chip config for this dev_id, bitmap stays 0 (all features disabled) */
            continue;
        }
        for (j = 0; j < feat->feature_count; j++) {
            if (feat->features[j] < feature_num) {
                bitmaps[i * ori_feature_num + feat->features[j]] = 1;
            }
        }
    }
}

int32_t sched_fop_feature_get(u32 devid, unsigned long arg)
{
    struct esched_feature_get_arg __ka_user *uarg = (struct esched_feature_get_arg __ka_user *)(uintptr_t)arg;
    unsigned char *bitmaps;
    unsigned int ori_feature_num;
    unsigned int feature_num;
    unsigned int bitmap_size;
    int ret;

    (void)devid;

    /* Read user-space's feature count */
    ret = copy_from_user_safe(&feature_num, &uarg->user_feature_num, sizeof(feature_num));
    if (ret != 0) {
        sched_err("sched_fop_feature_get: copy_from_user user_feature_num failed (ret=%d)\n", ret);
        return -EFAULT;
    }
    if ((feature_num == 0) || (feature_num > ESCHED_FEATURE_MAX_LIMIT)) {
        sched_err("invalid param, feature_num is %u, valid range (0, %u].\n", feature_num, ESCHED_FEATURE_MAX_LIMIT);
        return -EFAULT;
    }

    ori_feature_num = feature_num;
    if (feature_num > ESCHED_FEATURE_MAX) {
        feature_num = ESCHED_FEATURE_MAX;
    }

    /* Allocate flat bitmap: rows = ESCHED_FEATURE_DEV_NUM, columns = ori_feature_num (user-space size) */
    bitmap_size = ESCHED_FEATURE_DEV_NUM * ori_feature_num;
    bitmaps = ka_mm_kvzalloc(bitmap_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (bitmaps == NULL) {
        sched_err("sched_fop_feature_get: kvzalloc failed.\n");
        return -ENOMEM;
    }

    esched_fill_feature_bitmap(bitmaps, ori_feature_num, feature_num);

    ret = copy_to_user_safe(&uarg->user_feature_num, &feature_num, sizeof(feature_num));
    if (ret != 0) {
        sched_err("sched_fop_feature_get: copy_to_user user_feature_num failed (ret=%d)\n", ret);
        ka_mm_kvfree(bitmaps);
        return -EFAULT;
    }

    ret = copy_to_user_safe(uarg->feature_bitmaps, bitmaps, bitmap_size);
    ka_mm_kvfree(bitmaps);
    if (ret != 0) {
        sched_err("sched_fop_feature_get: copy_to_user feature_bitmaps failed (ret=%d)\n", ret);
        return -EFAULT;
    }

    return 0;
}

int sched_check_cp2_dest_pid(struct sched_published_event_info *event_info)
{
    return 0;
}

struct sched_thread_ctx *sched_drv_sched_cpu_get_next_thread(struct sched_cpu_ctx *cpu_ctx)
{
    (void)cpu_ctx;
    return NULL;
}

void esched_drv_sched_cpu_to_idle(struct sched_thread_ctx *thread_ctx, struct sched_cpu_ctx *cpu_ctx)
{
    (void)thread_ctx;
    (void)cpu_ctx;
}

struct topic_data_chan *esched_drv_get_thread_topic_chan(u32 dev_id, struct sched_thread_ctx *thread_ctx)
{
    (void)dev_id;
    (void)thread_ctx;
    return NULL;
}
