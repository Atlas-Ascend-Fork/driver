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
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"

#include "securec.h"

#include "pbl_feature_loader.h"
#include "pbl_uda.h"
#include "pbl_ka_mem_query.h"
#include "dpa_kernel_interface.h"
#include "comm_kernel_interface.h"
#include "svm_kernel_interface.h"

#include "svm_ub_mem.h"
#include "svm_kern_log.h"
#include "svm_addr_desc.h"
#include "svm_dev_topology.h"
#include "svm_pgtable.h"
#include "svm_slab.h"
#include "smm_kernel.h"
#include "dbi_kern.h"
#include "kmc_msg.h"
#include "svm_smp.h"
#include "pmq_msg.h"
#include "pmq.h"
#include "pmq_client.h"

int svm_pmq_client_cp_pa_query(u32 udevid, u64 va, u64 size, svm_pa_seg_wraper_t pa_seg_wraper[], u64 *seg_num)
{
    struct svm_global_va src_info;
    struct svm_pa_seg *pa_seg = NULL;
    u64 pa_seg_wraper_num = *seg_num;
    u64 tmp_seg_num;
    u64 npage_size;
    u64 i;
    int ret;

    ret = svm_dbi_kern_query_npage_size(udevid, &npage_size);
    if (ret != 0) {
        svm_err("Get page size failed. (udevid=%u)\n", udevid);
        return ret;
    }

    svm_global_va_pack(udevid, 0, va, size, &src_info);
    ret = hal_kernel_apm_query_slave_tgid_by_master(ka_task_get_current_tgid(), udevid, PROCESS_CP1, &src_info.tgid);
    if (ret != 0) {
        svm_err("Query slave tgid failed. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    *seg_num = svm_get_align_up_num(va, size, npage_size);
    if (*seg_num == 0) {
        svm_err("Invalid size. (size=0x%llx)\n", size);
        return -EINVAL;
    }

    pa_seg = svm_kvzalloc(*seg_num * sizeof(struct svm_pa_seg), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pa_seg == NULL) {
        svm_err("Malloc pa seg failed. (seg_num=%llu)\n", *seg_num);
        return -ENOMEM;
    }

    tmp_seg_num = *seg_num;
    ret = svm_smp_query_mem_pa(udevid, ka_task_get_current_tgid(), va, size, pa_seg, &tmp_seg_num);
    if (ret != 0) {
        ret = svm_pmq_client_pa_query(uda_get_host_id(), &src_info, pa_seg, seg_num);
        if (ret != 0) {
            svm_kvfree(pa_seg);
            svm_err("Query failed. (udevid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
        tmp_seg_num = *seg_num;
    }

    *seg_num = svm_make_pa_continues(pa_seg, tmp_seg_num);
    *seg_num = (*seg_num <= pa_seg_wraper_num) ? *seg_num : pa_seg_wraper_num;
    for (i = 0; i < *seg_num; i++) {
        pa_seg_wraper[i].pa = pa_seg[i].pa;
        pa_seg_wraper[i].size = pa_seg[i].size;
    }

    svm_kvfree(pa_seg);

    return 0;
}
KA_EXPORT_SYMBOL_GPL(svm_pmq_client_cp_pa_query);

/* Needed by operator package development, stubbing is done first, and subsequent adaptation will be required. */
int hal_kernel_svm_dev_va_to_dma_addr(int pid, u32 udevid, u64 va, u64 *dma_addr)
{
    return -EOPNOTSUPP;
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_svm_dev_va_to_dma_addr);

static int pmq_h2d_init_pa_handle(u32 udevid)
{
    struct svm_kmc_msg_head msg = {0};
    u32 in_len, real_out_len;
    int ret;

    in_len = sizeof(struct svm_kmc_msg_head);
    real_out_len = in_len;
    msg.msg_id = SVM_KMC_MSG_INTI_PA_HANDLE;
    ret = pmq_msg_send(uda_get_host_id(), udevid, (void *)&msg, in_len, &real_out_len);
    if (ret != 0) {
        svm_err("Kmc h2d recv_handle_init_msg failed. (ret=%d; udevid=%u)\n", ret, udevid);
    }

    return ret;
}

static int pmq_local_pa_get(u32 udevid, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 *seg_num,
                            u64 flag)
{
    SVM_UNUSED(flag);
    return svm_pmq_pa_get(src_info->tgid, src_info->va, src_info->size, pa_seg, seg_num);
}

static int pmq_local_pa_put(u32 udevid, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 seg_num)
{
    svm_pmq_pa_put(pa_seg, seg_num);
    return 0;
}

static int pmq_trans_pa_d2h(u32 udevid, struct svm_pa_seg pa_seg[], u64 seg_num)
{
    struct uda_mia_dev_para mia_para;
    u32 phy_devid;
    u64 i;
    int ret;

    /* Todo: tmp for vf not config bar4 */
    if (uda_is_phy_dev(udevid)) {
        phy_devid = udevid;
    } else {
        ret = uda_udevid_to_mia_devid(udevid, &mia_para);
        if (ret != 0) {
            svm_err("uda_udevid_to_mia_devid failed. (ret=%d; udevid=%u)\n", ret, udevid);
            return ret;
        }
        phy_devid = mia_para.phy_devid;
    }

    for (i = 0; i < seg_num; i++) {
        ret = devdrv_devmem_addr_d2h(phy_devid, pa_seg[i].pa, &pa_seg[i].pa);
        if (ret != 0) {
            svm_err("devdrv_devmem_addr_d2h failed. (ret=%d; udevid=%u)\n", ret, phy_devid);
            return ret;
        }
    }

    return 0;
}

static int pmq_d2h_pci_pa_get(u32 udevid, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 *seg_num,
                              u64 flag)
{
    int ret;

    SVM_UNUSED(flag);
    ret = svm_pmq_client_pa_query(udevid, src_info, pa_seg, seg_num);
    if (ret != 0) {
        svm_err("Host local pa query failed. (ret=%d; udevid=%u; src_udevid=%u)\n", ret, udevid, src_info->udevid);
        return ret;
    }

    return pmq_trans_pa_d2h(src_info->udevid, pa_seg, *seg_num);
}

static int pmq_d2h_ub_pa_get(u32 udevid, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 *seg_num,
                             u64 flag)
{
    struct devdrv_addr_desc addr_desc = {0};
    u64 trans_addr;
    int ret;

    svm_ub_va_to_mem_id_and_offset(src_info->va, &addr_desc.peer_ub.mem_id, &addr_desc.peer_ub.offset);
    addr_desc.peer_ub.cross_server = svm_is_cross_server(udevid, src_info->server_id);
    ret = devdrv_addr_trans_local_to_peer(udevid, &addr_desc, &trans_addr);
    if (ret != 0) {
        svm_err("Check whether the LingQu package is correctly installed, and use gmsysview in the control node to "
                "check whether the UB Decoder is correctly configured. "
                "(ret=%d; udevid=%u; src_udevid=%u; mem_id=%d; offset=0x%llx)\n",
                ret, udevid, src_info->udevid, addr_desc.peer_ub.mem_id, addr_desc.peer_ub.offset);
        return -ENOSPC;
    }

    *seg_num = 1;
    pa_seg[0].pa = trans_addr;
    pa_seg[0].size = src_info->size;
    return 0;
}

static int pmq_d2h_pa_put(u32 udevid, struct svm_global_va *src_info, struct svm_pa_seg pa_seg[], u64 seg_num)
{
#ifndef EMU_ST /* Reliability check: this may not be a D2H put. */
    pmq_try_recycle_local_mem(src_info, pa_seg, seg_num);
#endif
    return 0;
}

static const struct smm_ops pmq_local_ops = {
    .pa_get = pmq_local_pa_get,
    .pa_put = pmq_local_pa_put,
    .pa_location = SMM_LOCAL_PA,
};

static const struct smm_ops pmq_d2h_pci_ops = {
    .pa_get = pmq_d2h_pci_pa_get,
    .pa_put = pmq_d2h_pa_put,
    .pa_location = SMM_PEER_DEVICE_PA,
};

static const struct smm_ops pmq_d2h_ub_ops = {
    .pa_get = pmq_d2h_ub_pa_get,
    .pa_put = pmq_d2h_pa_put,
    .pa_location = SMM_PEER_DEVICE_PA,
};

static const struct smm_ops pmq_cs_ops = {
    .pa_get = pmq_d2h_ub_pa_get,
    .pa_put = pmq_d2h_pa_put,
    .pa_location = SMM_PEER_DEVICE_PA,
};

static int pmq_register_smm_pa_ops(u32 udevid)
{
    const struct smm_ops *ops = (udevid == uda_get_host_id()) ?
                                    &pmq_local_ops :
                                    ((devdrv_get_connect_protocol(udevid) == CONNECT_PROTOCOL_PCIE) ? &pmq_d2h_pci_ops :
                                                                                                      &pmq_d2h_ub_ops);
    int ret;

    if (udevid == uda_get_host_id()) {
        ret = svm_smm_register_cs_ops(udevid, &pmq_cs_ops);
        if (ret != 0) {
            svm_err("Register smm pa cs ops failed. (ret=%d)\n", ret);
            return ret;
        }
    }
    return svm_smm_register_ops(uda_get_host_id(), udevid, ops);
}

int pmq_client_host_init_dev(u32 udevid)
{
    int ret;

    ret = pmq_register_smm_pa_ops(udevid);
    if (ret != 0) {
        svm_err("Register smm pa ops failed. (ret=%d; udevid=%u)\n", ret, udevid);
        return ret;
    }

    if (udevid != uda_get_host_id()) {
        return pmq_h2d_init_pa_handle(udevid);
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT_DEV(pmq_client_host_init_dev, FEATURE_LOADER_STAGE_3);

void pmq_client_host_uninit_dev(u32 udevid)
{}
DECLAER_FEATURE_AUTO_UNINIT_DEV(pmq_client_host_uninit_dev, FEATURE_LOADER_STAGE_3);

static inline int pmq_client_smp_pin_mem(u32 devid, int tgid, u64 va, u64 size, bool cp_only_flag)
{
    if (cp_only_flag) {
        return svm_smp_pin_dev_cp_only_mem(devid, tgid, va, size);
    }

    return svm_smp_pin_mem(devid, tgid, va, size, false);
}

static inline int pmq_client_smp_unpin_mem(u32 devid, int tgid, u64 va, u64 size, bool cp_only_flag)
{
    if (cp_only_flag) {
        return svm_smp_unpin_dev_cp_only_mem(devid, tgid, va, size);
    }

    return svm_smp_unpin_mem(devid, tgid, va, size, false);
}

static int pmq_client_get_mem_pa_list_proc(u32 devid, int tgid, struct ka_mem_attr *mem, u64 *pa_num,
                                           struct ka_pa_wraper *pa_list)
{
    int ret = 0;

    if ((mem == NULL) || (pa_num == NULL) || (pa_list == NULL) || (*pa_num == 0)) {
        return -EINVAL;
    }

    ret = pmq_client_smp_pin_mem(devid, tgid, mem->addr, mem->size, mem->cp_only_flag);
    if (ret != 0) {
        svm_err("Pin svm mem failed. (devid=%u; size=%ld; ret=%d)\n", devid, mem->size, ret);
        return ret;
    }

    /* only support phy continuous addr for now */
    if (devid == uda_get_host_id()) {
        ret = svm_pmq_pa_query(tgid, mem->addr, mem->size, (struct svm_pa_seg *)pa_list, pa_num);
    } else {
        ret = svm_pmq_client_cp_pa_query(devid, mem->addr, mem->size, (svm_pa_seg_wraper_t *)pa_list, pa_num);
    }
    if (ret != 0) {
        (void)pmq_client_smp_unpin_mem(devid, tgid, mem->addr, mem->size, mem->cp_only_flag);
        svm_err("Query svm pa failed. (devid=%u; size=%ld; ret=%d)\n", devid, mem->size, ret);
        return ret;
    }
    if (mem->size != svm_get_pa_size((struct svm_pa_seg *)pa_list, *pa_num)) {
        (void)pmq_client_smp_unpin_mem(devid, tgid, mem->addr, mem->size, mem->cp_only_flag);
        svm_err("Invalid mem size. (devid=%u; tgid=%d; size=%llu; pa_size=%llu)\n", devid, tgid, mem->size,
                svm_get_pa_size((struct svm_pa_seg *)pa_list, *pa_num));
        return -EINVAL;
    }

    if ((devid != uda_get_host_id()) && (!mem->raw_pa_flag)) {
        /* struct ka_pa_wraper is the same as struct svm_pa_seg */
        struct svm_pa_seg *pa_seg = (struct svm_pa_seg *)pa_list;
        ret = pmq_trans_pa_d2h(devid, pa_seg, *pa_num);
        if (ret != 0) {
            (void)pmq_client_smp_unpin_mem(devid, tgid, mem->addr, mem->size, mem->cp_only_flag);
            svm_err("Failed to get bar. (ret=%d; devid=%u)\n", ret, devid);
            return ret;
        }
    }

    return 0;
}

static int pmq_client_put_mem_pa_list_proc(u32 devid, int tgid, struct ka_mem_attr *mem, u64 pa_num,
                                           struct ka_pa_wraper *pa_list)
{
    int ret = 0;

    if (mem == NULL) {
        return -EINVAL;
    }

    ret = pmq_client_smp_unpin_mem(devid, tgid, mem->addr, mem->size, mem->cp_only_flag);
    if (ret != 0) {
        svm_err("Unpin svm mem failed. (devid=%u; size=%ld; ret=%d)\n", devid, mem->size, ret);
        return ret;
    }

    return 0;
}

static u32 pmq_client_get_mem_page_size_proc(u32 devid, int tgid, struct ka_mem_attr *mem)
{
    u64 page_size;
    int ret;
    (void)mem;
    (void)tgid;
    ret = svm_dbi_kern_query_npage_size(devid, &page_size);
    if (ret != 0) {
        return 0;
    }
    return (u32)page_size;
}

int pmq_client_host_init(void)
{
    struct svm_mem_query_ops ops = {
        .get_svm_mem_pa = pmq_client_get_mem_pa_list_proc,
        .put_svm_mem_pa = pmq_client_put_mem_pa_list_proc,
        .get_svm_mem_page_size = pmq_client_get_mem_page_size_proc,
    };

    return hal_kernel_register_mem_query_ops(&ops);
}
DECLAER_FEATURE_AUTO_INIT(pmq_client_host_init, FEATURE_LOADER_STAGE_9);

void pmq_client_host_uninit(void)
{
    hal_kernel_unregister_mem_query_ops();
}
DECLAER_FEATURE_AUTO_UNINIT(pmq_client_host_uninit, FEATURE_LOADER_STAGE_9);
