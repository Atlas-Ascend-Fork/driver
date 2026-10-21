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
#include "ka_dfx_pub.h"

#include "pbl_feature_loader.h"
#include "pbl_uda.h"
#include "pbl_soc_res.h"

#include "svm_kern_log.h"
#include "kmc_h2d.h"
#include "dbi_kern.h"
#include "ubmem_msg.h"
#include "ubmm.h"
#include "svm_ub_mem.h"
#include "ubmem_client.h"

#ifdef CFG_FEATURE_UB
static int ubmem_query_host_mem_id(u32 udevid, u32 *mem_id)
{
    static u32 local_mem_id, flag = 0;
    u32 udevid_ex = 0;
    u64 value;
    int ret;

    if (flag != 0) {
        *mem_id = local_mem_id;
        return 0;
    }

    (void)uda_get_cur_ns_udevids(&udevid_ex, 1);
    udevid_ex = (udevid == uda_get_host_id()) ? udevid_ex : udevid; /* Not host_id real dev, change to first udevid. */
    ret = soc_resmng_dev_get_key_value(udevid_ex, "UB_MEM_ID", &value);
    if (ret != 0) {
        svm_err("Get mem_id failed. (udevid=%u)\n", udevid);
        return ret;
    }

    local_mem_id = (u32)value;
    flag = 1;
    *mem_id = local_mem_id;
    return 0;
}

static int ubmem_map_local_client(u32 udevid, struct svm_global_va *src_va, u64 *maped_va)
{
    u64 uba_base, uba;
    u32 mem_id;
    int ret;

    ret = ubmm_do_map(udevid, src_va->tgid, src_va->va, src_va->size, &uba);
    if (ret != 0) {
        svm_err_if((ret != -ENOTSUPP), "Ub mem map failed. (ret=%d; udevid=%u; tgid=%d; va=0x%llx; size=0x%llx)\n", ret,
                   udevid, src_va->tgid, src_va->va, src_va->size);
        return ret;
    }

    ret = ubmem_query_host_mem_id(udevid, &mem_id);
    if (ret != 0) {
        (void)ubmm_do_unmap(udevid, src_va->tgid, src_va->va, src_va->size);
        return ret;
    }

    ret = ubmm_query_uba_base(udevid, &uba_base);
    if (ret != 0) {
        (void)ubmm_do_unmap(udevid, src_va->tgid, src_va->va, src_va->size);
        return ret;
    }

    ret = svm_ub_mem_id_and_offset_to_va(mem_id, uba - uba_base, maped_va);
    if (ret != 0) {
        svm_err("Invalid uba. (udevid=%u; uba=0x%llx; uba_base=0x%llx)\n", udevid, uba, uba_base);
        (void)ubmm_do_unmap(udevid, src_va->tgid, src_va->va, src_va->size);
        return ret;
    }

    return 0;
}

static int ubmem_unmap_local_client(u32 udevid, struct svm_global_va *src_va)
{
    return ubmm_do_unmap(udevid, src_va->tgid, src_va->va, src_va->size);
}
#else
static int ubmem_map_local_client(u32 udevid, struct svm_global_va *src_va, u64 *maped_va)
{
    return -ENOTSUPP;
}

static int ubmem_unmap_local_client(u32 udevid, struct svm_global_va *src_va)
{
    return -ENOTSUPP;
}
#endif

static int ubmem_map_remote_client(u32 udevid, struct svm_global_va *src_va, u64 *maped_va)
{
    struct ubmem_map_msg msg;
    u32 reply_len;
    int ret;

    msg.head.msg_id = SVM_KMC_MSG_UBMEM_MAP;
    msg.head.extend_num = 0;
    msg.src_va = *src_va;

    ret = svm_kmc_h2d_send(udevid, &msg, (u32)sizeof(msg), (u32)sizeof(msg), &reply_len);
    if (ret != 0) {
        svm_err_if((ret != -ENOTSUPP), "Kmc h2d failed. (ret=%d; udevid=%u)\n", ret, udevid);
        return ret;
    }

    *maped_va = msg.maped_va;

    return 0;
}

static int ubmem_unmap_remote_client(u32 udevid, struct svm_global_va *src_va)
{
    struct ubmem_unmap_msg msg;
    u32 reply_len;
    int ret;

    msg.head.msg_id = SVM_KMC_MSG_UBMEM_UNMAP;
    msg.head.extend_num = 0;
    msg.src_va = *src_va;

    ret = svm_kmc_h2d_send(udevid, &msg, (u32)sizeof(msg), (u32)sizeof(msg), &reply_len);
    if (ret != 0) {
        svm_err("Kmc h2d failed. (ret=%d; udevid=%u)\n", ret, udevid);
        return ret;
    }

    return 0;
}

int ubmem_map_client(u32 udevid, struct svm_global_va *src_va, u64 *maped_va)
{
    int ret = (udevid == uda_get_host_id()) ? ubmem_map_local_client(udevid, src_va, maped_va) :
                                              ubmem_map_remote_client(udevid, src_va, maped_va);
    if (ret == -ENOTSUPP) {
        *maped_va = 0;
        return 0;
    }
    return ret;
}

int ubmem_unmap_client(u32 udevid, struct svm_global_va *src_va)
{
    if (!svm_dbi_kern_is_support_ubmem(udevid)) {
        return 0;
    }

    if (udevid == uda_get_host_id()) {
        return ubmem_unmap_local_client(udevid, src_va);
    } else {
        return ubmem_unmap_remote_client(udevid, src_va);
    }
}
