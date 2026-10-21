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
#include "ka_system_pub.h"
#include "ka_driver_pub.h"
#include "ka_memory_pub.h"
#include "ascend_hal_define.h"
#include "pbl/pbl_soc_res.h"
#include "pbl/pbl_runenv_config.h"
#include "dpa/dpa_rmo_kernel.h"
#ifdef CFG_FEATURE_VM_ADAPT
#include "trs_sec_eh_auto_init.h"
#else
#include "trs_sia_adapt_auto_init.h"
#endif
#include "soc_adapt.h"
#include "trs_pub_def.h"
#include "trs_msg.h"
#include "trs_host_msg.h"
#include "trs_chan.h"
#include "trs_core.h"
#include "trs_abnormal_info.h"
#include "trs_ts_status.h"
#include "trs_id.h"
#include "comm_kernel_interface.h"
#include "trs_ioctl.h"
#include "trs_mailbox_def.h"
#include "trs_host_ts_cq.h"
#include "trs_host_comm.h"

static struct ka_vmap_pfn_handle *trs_host_vmap_pfn_from_pa_wraper(struct ka_pa_wraper *pa_wraper_list, u32 pa_num,
                                                                   pgprot_t prot)
{
    unsigned long *pfns;
    u32 total_pages = 0;
    u32 idx = 0;
    u32 i, j;
    struct ka_vmap_pfn_handle *handle;

    if (pa_wraper_list == NULL || pa_num == 0)
        return NULL;

    for (i = 0; i < pa_num; i++) {
        total_pages += pa_wraper_list[i].size >> PAGE_SHIFT;
    }

    pfns = (unsigned long *)trs_vzalloc(sizeof(unsigned long) * total_pages);
    if (pfns == NULL)
        return NULL;

    for (i = 0; i < pa_num; i++) {
        unsigned long base_pfn = __ka_mm_phys_to_pfn(pa_wraper_list[i].pa);
        u32 seg_pages = pa_wraper_list[i].size >> PAGE_SHIFT;
        for (j = 0; j < seg_pages; j++) {
            pfns[idx++] = base_pfn + j;
        }
    }

    handle = ka_mm_vmap_pfn_create(pfns, total_pages, prot);
    trs_vfree(pfns);
    return handle;
}

static pgprot_t trs_host_sq_mem_pgprot(void)
{
    pgprot_t prot = ka_mm_pgprot_noncached(KA_PAGE_KERNEL);

    /* Device SVM pages are shared with the host CPU in a confidential VM. */
    return run_in_confidential_virtual_mach() ? ka_mm_pgprot_decrypted(prot) : prot;
}

static int trs_host_mem_vmap(struct trs_id_inst *inst, struct ka_pa_wraper *pa_wraper_list, u32 pa_num,
                             struct trs_sq_mem_handle *handle)
{
    struct ka_vmap_pfn_handle *vmap_handle;

    vmap_handle = trs_host_vmap_pfn_from_pa_wraper(pa_wraper_list, pa_num, trs_host_sq_mem_pgprot());
    if (vmap_handle == NULL) {
        trs_err("vmap_pfn fail. (devid=%u; pa_num=%u)\n", inst->devid, pa_num);
        return -ENOMEM;
    }
    handle->vmap_pfn_handle = vmap_handle;
    return 0;
}

static void trs_host_mem_vunmap(struct trs_sq_mem_handle *handle)
{
    if (handle->vmap_pfn_handle != NULL) {
        ka_mm_vmap_pfn_destroy(handle->vmap_pfn_handle);
        handle->vmap_pfn_handle = NULL;
    }
}

static int trs_host_mem_virt_split(struct trs_id_inst *inst, size_t size, struct trs_sq_mem_handle *handle,
                                   u32 page_size)
{
    u32 split_num = 4; /* 4 segments addr for simulation/diagnosis */
    u32 split_size = KA_DRIVER_ALIGN_DOWN(size / split_num, page_size);
    u32 last_size = size - ((split_num - 1) * split_size);
    struct ka_pa_wraper *new_wraper_list = NULL;
    u32 i;

    if (handle->pa_num != 1 || size <= split_num * 4096) { /* 4096: 4K */
        return 0;
    }

    new_wraper_list = (struct ka_pa_wraper *)trs_vzalloc(sizeof(struct ka_pa_wraper) * split_num);
    if (new_wraper_list == NULL) {
        trs_err("Vzalloc new_pa_wrapper_list fail.\n");
        return -ENOMEM;
    }

    for (i = 0; i < split_num; i++) {
        new_wraper_list[i].pa = handle->pa_wraper_list[0].pa + i * split_size;
        new_wraper_list[i].size = (i < split_num - 1) ? split_size : last_size;
        trs_debug("Virt split segment. (devid=%u; size=%llu)\n", inst->devid, new_wraper_list[i].size);
    }

    /* Destroy old vmap_pfn_handle (mapped as single segment), recreate for split segments */
    ka_mm_vmap_pfn_destroy(handle->vmap_pfn_handle);
    handle->vmap_pfn_handle = NULL;

    trs_vfree(handle->pa_wraper_list);
    handle->pa_wraper_list = new_wraper_list;
    handle->pa_num = split_num;

    handle->vmap_pfn_handle = trs_host_vmap_pfn_from_pa_wraper(new_wraper_list, split_num, trs_host_sq_mem_pgprot());
    if (handle->vmap_pfn_handle == NULL) {
        trs_err("Recreate vmap_pfn after virt_split fail. (devid=%u)\n", inst->devid);
        return -ENOMEM;
    }

    return 0;
}

static int _trs_get_sq_mem_handle(u32 devid, int pid, struct trs_sq_mem_handle *handle, u32 *out_page_size)
{
    struct ka_pa_wraper *pa_wraper_list = NULL;
    u64 pa_num, base_va;
    u32 align_len, page_size;
    int ret;

    page_size = hal_kernel_get_mem_page_size(devid, pid, &handle->mem_attr);
    if (page_size == 0) {
        trs_err("Get page size failed. (devid=%u; pid=%d)\n", devid, pid);
        return -EINVAL;
    }

    align_len = ALIGN_UP(handle->mem_attr.size, page_size);
    base_va = KA_DRIVER_ALIGN_DOWN(handle->mem_attr.addr, page_size);

    pa_num = align_len / page_size;
    pa_wraper_list = trs_vzalloc(sizeof(struct ka_pa_wraper) * pa_num);
    if (pa_wraper_list == NULL) {
        trs_err("Vzalloc pa_wrapper_list fail.\n");
        return -ENOMEM;
    }

    handle->mem_attr.addr = base_va;
    handle->mem_attr.size = align_len;
    ret = hal_kernel_get_mem_pa_list(devid, pid, &handle->mem_attr, &pa_num, pa_wraper_list);
    if (ret != 0) {
        trs_err("Failed to get pa list. (ret=%d; devid=%u; pid=%d; align_len=%u)\n", ret, devid, pid, align_len);
        trs_vfree(pa_wraper_list);
        return ret;
    }

    handle->pa_wraper_list = pa_wraper_list;
    handle->pa_num = (u32)pa_num;
    if (out_page_size != NULL) {
        *out_page_size = page_size;
    }
    return 0;
}

static void _trs_put_sq_mem_handle(u32 devid, int pid, struct trs_sq_mem_handle *handle)
{
    int ret;

    if (handle->pa_num > 0 && handle->pa_wraper_list != NULL) {
        ret = hal_kernel_put_mem_pa_list(devid, pid, &handle->mem_attr, handle->pa_num, handle->pa_wraper_list);
        if (ret != 0) {
            trs_warn("Put pa list failed. (devid=%u; pid=%d; ret=%d)\n", devid, pid, ret);
        }
        trs_vfree(handle->pa_wraper_list);
        handle->pa_wraper_list = NULL;
    }

    handle->pa_num = 0;
}

#define TRS_64K_PAGE_SIZE (64 * 1024)
int trs_host_get_sq_mem_handle(struct trs_id_inst *inst, int pid, struct trs_sq_mem_handle *handle)
{
    u32 page_size;
    int ret;

    ret = _trs_get_sq_mem_handle(inst->devid, pid, handle, &page_size);
    if (ret != 0) {
        return ret;
    }

    if (!trs_soc_is_support_virt(inst) && (handle->pa_num > 1)) {
        trs_err("No mem. (devid=%u; pa_num=%u)\n", inst->devid, handle->pa_num);
        ret = -ENOMEM;
        goto put_mem;
    }

    if ((handle->pa_num > 1) && (KA_MM_PAGE_SIZE == TRS_64K_PAGE_SIZE)) {
        ret = -EOPNOTSUPP;
        goto put_mem;
    }

    if (!handle->mem_attr.raw_pa_flag) {
        ret = trs_host_mem_vmap(inst, handle->pa_wraper_list, handle->pa_num, handle);
        if (ret != 0) {
            goto put_mem;
        }
    }

    if (trs_chan_get_virt_flag_test(inst)) {
        /* split address for simulation with virtual address */
        ret = trs_host_mem_virt_split(inst, handle->mem_attr.size, handle, page_size);
        if (ret != 0) {
            goto vunmap_mem;
        }
    }

    return 0;

vunmap_mem:
    if (!handle->mem_attr.raw_pa_flag) {
        trs_host_mem_vunmap(handle);
    }
put_mem:
    trs_host_put_sq_mem_handle(inst, pid, handle);
    return ret;
}

void trs_host_put_sq_mem_handle(struct trs_id_inst *inst, int pid, struct trs_sq_mem_handle *handle)
{
    trs_host_mem_vunmap(handle);
    _trs_put_sq_mem_handle(inst->devid, pid, handle);
}

#ifdef CFG_FEATURE_SUPPORT_RMO
static int trs_mem_dispatch_to_ts(u32 devid, struct rmo_mem_raw_addr *addr, u64 len)
{
    struct trs_mem_dispatch_msg mbox_data = {0};
    struct trs_id_inst inst;
    int ret;

    trs_mbox_init_header(&mbox_data.header, TRS_MBOX_MEM_DISPATCH);
    mbox_data.size = len;
    mbox_data.hostpid = ka_task_get_current_tgid();

    /* NULL means mem disable, others enable */
    if (addr != NULL) {
        mbox_data.flag = 1;
        mbox_data.is_addr_ext = (addr->raw_addr_len <= sizeof(u64)) ? 0 : 1;
        mbox_data.addr = (mbox_data.is_addr_ext) ? addr->raw_addr : (void *)(uintptr_t)(*((u64 *)(addr->raw_addr)));
        mbox_data.addr_len = addr->raw_addr_len;
    } else {
        mbox_data.flag = 0;
    }

    trs_id_inst_pack(&inst, devid, 0);
    ret = trs_mbox_send(&inst, 0, &mbox_data, sizeof(struct trs_mem_dispatch_msg), 3000); // timeout: 3000 ms
    if ((ret != 0) || (mbox_data.header.result) != 0) {
        trs_err("Send mbox fail. (devid=%u; result=%u; ret=%d)\n", devid, mbox_data.header.result, ret);
        return -EFAULT;
    }

    trs_debug("Dispatch success. (devid=%u; hostpid=%d; len=%llu)\n", devid, mbox_data.hostpid, len);
    return 0;
}
#endif

int trs_host_mem_sharing_register(void)
{
#ifdef CFG_FEATURE_SUPPORT_RMO
    rmo_mem_sharing_register(trs_mem_dispatch_to_ts, TS_ACCESSOR);
    trs_info("Register success.\n");
#endif
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(trs_host_mem_sharing_register, FEATURE_LOADER_STAGE_6);

void trs_host_mem_sharing_unregister(void)
{
#ifdef CFG_FEATURE_SUPPORT_RMO
    rmo_mem_sharing_unregister(TS_ACCESSOR);
#endif
}
DECLAER_FEATURE_AUTO_UNINIT(trs_host_mem_sharing_unregister, FEATURE_LOADER_STAGE_6);

int trs_host_get_ssid(struct trs_id_inst *inst, int *user_visible_flag, int *ssid)
{
    struct trs_msg_sync_ssid *ssid_msg = NULL;
    struct trs_msg_data msg = {0};
    int ret;

    msg.header.tsid = inst->tsid;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_MSG_GET_SSID;
    msg.header.result = 0;

    ssid_msg = (struct trs_msg_sync_ssid *)msg.payload;
    ssid_msg->vfid = 0;
    ssid_msg->hpid = ka_task_get_current_tgid();

    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if ((ret != 0) || (msg.header.result != 0)) {
        trs_err("Msg send fail. (devid=%u; tsid=%u; ret=%d; result=%d)\n", inst->devid, inst->tsid, ret,
                msg.header.result);
        return -ENODEV;
    }
    *user_visible_flag = 0;
    *ssid = ssid_msg->ssid;
    return 0;
}

int trs_host_ts_adapt_abnormal_proc(u32 devid, struct trs_msg_data *msg)
{
    struct stars_abnormal_info *abnormal_info = (struct stars_abnormal_info *)msg->payload;
    struct trs_id_inst inst;

    if (abnormal_info == NULL) {
        trs_err("Data is NULL. (devid=%u; tsid=%u)\n", devid, msg->header.tsid);
        return -EFAULT;
    }

    trs_id_inst_pack(&inst, devid, msg->header.tsid);
    return trs_chan_abnormal_proc(&inst, abnormal_info);
}

int trs_host_set_ts_status(u32 devid, struct trs_msg_data *data)
{
    u32 status = data->payload[0];
    struct trs_id_inst inst;

    if ((status != TRS_INST_STATUS_NORMAL) && (status != TRS_INST_STATUS_ABNORMAL)) {
        trs_err("Invalid para. (devid=%u; tsid=%u; status=%u)\n", devid, data->header.tsid, status);
        return -EINVAL;
    }

    trs_id_inst_pack(&inst, devid, data->header.tsid);
    return trs_set_ts_status(&inst, status);
}

int trs_host_flush_id(u32 devid, struct trs_msg_data *data)
{
    struct trs_msg_id_sync_head *msg_info = (struct trs_msg_id_sync_head *)data->payload;
    struct trs_id_inst inst;

    trs_id_inst_pack(&inst, devid, data->header.tsid);
    return trs_id_free_batch_by_type(&inst, msg_info->type);
}

int trs_host_res_is_check_msg_proc(u32 devid, struct trs_msg_data *data)
{
    struct trs_msg_res_id_check *id_msg = (struct trs_msg_res_id_check *)data->payload;
    struct trs_id_inst inst;

    if ((id_msg->id_type < 0) || (id_msg->id_type >= TRS_MAX_ID_TYPE)) {
        trs_err("Invalid para. (devid=%u; res_type=%d)\n", devid, id_msg->id_type);
        return -EINVAL;
    }

    trs_id_inst_pack(&inst, devid, data->header.tsid);
    if (trs_res_is_belong_to_proc(&inst, id_msg->hpid, id_msg->id_type, id_msg->res_id)) {
        trs_debug("Res id belong to master process. (master_tgid=%u; type=%s; id=%d)\n", id_msg->hpid,
                  trs_id_type_to_name(id_msg->id_type), id_msg->res_id);
        return 0;
    }
    trs_err("Res id not belong to master process. (master_tgid=%u; type=%s; id=%d)\n", id_msg->hpid,
            trs_id_type_to_name(id_msg->id_type), id_msg->res_id);
    return -1;
}

int trs_host_res_id_check(struct trs_id_inst *inst, int id_type, u32 res_id)
{
#ifndef EMU_ST
    struct trs_msg_res_id_check *id_msg = NULL;
    struct trs_msg_data msg = {0};
    int ret;

    msg.header.tsid = inst->tsid;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_MSG_RES_ID_CHECK;
    msg.header.result = 0;

    id_msg = (struct trs_msg_res_id_check *)msg.payload;
    id_msg->hpid = (u32)ka_task_get_current_tgid();
    id_msg->id_type = id_type;
    id_msg->res_id = res_id;

    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if ((ret != 0) || (msg.header.result != 0)) {
        trs_err("Msg send fail. (devid=%u; tsid=%u; ret=%d; result=%d)\n", inst->devid, inst->tsid, ret,
                msg.header.result);
        return -ENODEV;
    }
#endif
    return 0;
}

static inline void trs_res_id_get_para_pack(struct trs_id_inst *inst, u32 cmdtype, int id_type, u32 res_id, u32 hpid,
                                            u32 seq, struct trs_msg_data *msg)
{
    struct trs_msg_res_id_get *id_msg = NULL;

    msg->header.tsid = inst->tsid;
    msg->header.valid = TRS_MSG_SEND_MAGIC;
    msg->header.cmdtype = cmdtype;
    msg->header.result = 0;

    id_msg = (struct trs_msg_res_id_get *)msg->payload;
    id_msg->hpid = hpid;
    id_msg->id_type = id_type;
    id_msg->res_id = res_id;
    id_msg->seq = seq;
}

#define TRS_HOST_RES_ID_COMPENSATE_RETRY_MAX 3
/*
 * Retry put with same (hpid, seq). Idempotent due to trs_res_put hpid/seq check:
 * - device already put: ref_hpid/ref_seq cleared -> -EKEYREJECTED -> treat as success
 * - device never put: hpid/seq match -> ref-- -> correct, no leak
 */
static int trs_host_res_id_retry_put(struct trs_id_inst *inst, int id_type, u32 res_id, u32 hpid, u32 seq)
{
    struct trs_msg_data msg = {0};
    int i;

    trs_res_id_get_para_pack(inst, TRS_MSG_RES_ID_PUT, id_type, res_id, hpid, seq, &msg);
    for (i = 0; i < TRS_HOST_RES_ID_COMPENSATE_RETRY_MAX; i++) {
        int ret = trs_host_msg_send_pure_ret(inst->devid, &msg, sizeof(struct trs_msg_data));
        if (ret == 0) {
            /* -EKEYREJECTED means device already put (hpid/seq cleared), treat as success */
            return (msg.header.result == -EKEYREJECTED) ? 0 : msg.header.result;
        }

        trs_err("Retry put fail. (devid=%u; tsid=%u; seq=%u; retry=%d)\n", inst->devid, inst->tsid, seq, i);
    }
    return -EIO;
}

int trs_host_res_id_put(struct trs_id_inst *inst, int id_type, u32 res_id, u32 hpid, u32 seq)
{
    struct trs_msg_data msg = {0};
    int ret;

    trs_res_id_get_para_pack(inst, TRS_MSG_RES_ID_PUT, id_type, res_id, hpid, seq, &msg);
    ret = trs_host_msg_send_pure_ret(inst->devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        /* send fail: device may not have put, retry to avoid resource leak */
        trs_err("Msg send fail, retrying put. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        return trs_host_res_id_retry_put(inst, id_type, res_id, hpid, seq);
    }

    if (msg.header.result != 0) {
        trs_err("Msg fail. (devid=%u; tsid=%u; result=%d)\n", inst->devid, inst->tsid, msg.header.result);
    }

    return msg.header.result;
}

int trs_host_res_id_get(struct trs_id_inst *inst, int id_type, u32 res_id, u32 hpid, u32 seq)
{
    struct trs_msg_data msg = {0};
    int ret;

    trs_res_id_get_para_pack(inst, TRS_MSG_RES_ID_GET, id_type, res_id, hpid, seq, &msg);
    ret = trs_host_msg_send_pure_ret(inst->devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        /* msg send fail: device may have done ref++, send retry put to compensate */
        trs_err("Msg send fail, retrying put. (devid=%u; tsid=%u; ret=%d)\n", inst->devid, inst->tsid, ret);
        (void)trs_host_res_id_retry_put(inst, id_type, res_id, hpid, seq);
        return -EIO;
    }

    if (msg.header.result != 0) {
        trs_err("Msg fail. (devid=%u; tsid=%u; result=%d)\n", inst->devid, inst->tsid, msg.header.result);
    }

    return msg.header.result;
}

int trs_host_ras_report(struct trs_id_inst *inst)
{
    struct trs_msg_data msg = {0};
    int ret;

    msg.header.tsid = inst->tsid;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_MSG_RAS_REPORT;
    msg.header.result = 0;

    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if ((ret != 0) || (msg.header.result != 0)) {
        trs_err("Msg send fail. (devid=%u; tsid=%u; ret=%d; result=%d)\n", inst->devid, inst->tsid, ret,
                msg.header.result);
        return ret;
    }
    return 0;
}

int trs_host_res_num_query(struct trs_id_inst *inst, struct trs_res_query_para *para)
{
    struct trs_res_query_para *query_para = NULL;
    struct trs_msg_data msg = {0};
    int ret;

    msg.header.tsid = inst->tsid;
    msg.header.valid = TRS_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_MSG_RES_NUM_QUERY;
    msg.header.result = 0;
    query_para = (struct trs_res_query_para *)msg.payload;
    query_para->type = para->type;

    ret = trs_host_msg_send(inst->devid, &msg, sizeof(struct trs_msg_data));
    if (ret != 0) {
        if (ret != -EOPNOTSUPP) {
            trs_err("Msg send fail. (devid=%u; tsid=%u; ret=%d; result=%d)\n", inst->devid, inst->tsid, ret,
                    msg.header.result);
        }
        return ret;
    }

    para->alloc_num = query_para->alloc_num;
    return 0;
}

int trs_host_ts_cq_process(u32 devid, struct trs_msg_data *data)
{
    struct trs_msg_ts_cq_process *msg = (struct trs_msg_ts_cq_process *)data->payload;
    struct trs_ts_cq_info *cq_info = NULL;
    struct trs_id_inst inst;
    int ret;

    trs_id_inst_pack(&inst, devid, data->header.tsid);

    cq_info = trs_get_ts_cq_info(devid);
    if ((cq_info == NULL) || (cq_info->host_ts_cq_process_func == NULL) || (cq_info->para == NULL)) {
        trs_err("Invalid ts cq info. (devid=%u)\n", devid);
        return -EINVAL;
    }

    ret = trs_ts_cq_cpy(&inst, msg->cqid, msg->cq_type, msg->cqe);
    if (ret != 0) {
        trs_err("Failed to copy ts cq. (ret=%d; devid=%u; cq_type=%u; cqid=%u)\n", ret, devid, msg->cq_type, msg->cqid);
        return ret;
    }
    ret = cq_info->host_ts_cq_process_func(0, 0, cq_info->para, &msg->cqid, 1);
    return (ret == -EBUSY) ? 0 : ret; /* -EBUSY means just schedule in work */
}

int trs_host_get_connect_protocol(struct trs_id_inst *inst)
{
    int type = devdrv_get_connect_protocol(inst->devid);

    switch (type) {
        case CONNECT_PROTOCOL_PCIE:
            return TRS_CONNECT_PROTOCOL_PCIE;
        case CONNECT_PROTOCOL_HCCS:
            return TRS_CONNECT_PROTOCOL_HCCS;
        case CONNECT_PROTOCOL_UB:
            return TRS_CONNECT_PROTOCOL_UB;
        default:
            return TRS_CONNECT_PROTOCOL_UNKNOWN;
    }
}

int trs_get_host_mach_flag(struct trs_id_inst *inst, u32 *host_flag)
{
    int ret;

    ret = devdrv_get_host_phy_mach_flag(inst->devid, host_flag);
    if (ret != 0) {
        trs_err("Failed to get host phy mach flag. (ret=%d; devid=%u)\n", ret, inst->devid);
        return ret;
    }
    trs_debug("Get success. (devid=%u; host_flag=0x%x)\n", inst->devid, *host_flag);
    return 0;
}

int trs_host_request_irq(struct trs_id_inst *inst, struct trs_adapt_irq_attr *attr, void *para, const char *name,
                         ka_irqreturn_t (*handler)(int irq, void *para))
{
    int ret;
    struct res_inst_info res_inst;
    u32 hwirq;
    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);
    ret = soc_resmng_get_hwirq(&res_inst, attr->irq_type, attr->irq, &hwirq);
    if (ret != 0) {
        trs_err("Get hw irq failed. (devid=%u; irqtype=%u; irq=%u; ret=%d)\n", inst->devid, attr->irq_type, attr->irq,
                ret);
        return ret;
    }

#ifndef EMU_ST
    ret = devdrv_register_irq_by_vector_index(inst->devid, hwirq, handler, para, name);
#else
    ret = ka_system_request_irq(attr->irq, handler, 0, name, para);
#endif
    if (ret != 0) {
        trs_err("Request irq failed. (devid=%u; irq_type=%u; irq=%u; hwirq=%u; name=%s; ret=%d)\n", inst->devid,
                attr->irq_type, attr->irq, hwirq, name, ret);
    }
    return ret;
}

void trs_host_free_irq(struct trs_id_inst *inst, struct trs_adapt_irq_attr *attr, void *para)
{
    struct res_inst_info res_inst;
    u32 hwirq;
    int ret;
    soc_resmng_inst_pack(&res_inst, inst->devid, TS_SUBSYS, inst->tsid);
    ret = soc_resmng_get_hwirq(&res_inst, attr->irq_type, attr->irq, &hwirq);
    if (ret != 0) {
        trs_warn("Get hw irq warn. (devid=%u; irqtype=%u; irq=%u; ret=%d)\n", inst->devid, attr->irq_type, attr->irq,
                 ret);
        return;
    }
#ifndef EMU_ST
    ret = devdrv_unregister_irq_by_vector_index(inst->devid, hwirq, para);
#else
    ret = ka_system_free_irq(attr->irq, para);
#endif
    if (ret != 0) {
        trs_warn("Free irq warn. (devid=%u; irqtype=%u; irq=%u; hwirq=%u; ret=%d)\n", inst->devid, attr->irq_type,
                 attr->irq, hwirq, ret);
        return;
    }
}

int trs_adapt_ops_request_irq(struct trs_id_inst *inst, u32 irq_type, u32 irq, void *para,
                              ka_irqreturn_t (*handler)(int irq, void *para))
{
    struct trs_adapt_irq_attr attr;
    const char *name = NULL;

    attr.irq_type = irq_type;
    attr.irq = irq;
    name = soc_resmng_get_ts_irq_name_by_type(irq_type);
    if (name == NULL) {
        trs_err("Get irq name failed.(devid=%u; irq_type=%d)\n", inst->devid, irq_type);
        return -EINVAL;
    }

    return trs_host_request_irq(inst, &attr, para, name, handler);
}

void trs_adapt_ops_free_irq(struct trs_id_inst *inst, u32 irq_type, u32 irq, void *para)
{
    struct trs_adapt_irq_attr attr;
    attr.irq_type = irq_type;
    attr.irq = irq;
    (void)trs_host_free_irq(inst, &attr, para);
}
