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
#include "pbl_uda.h"
#include "pbl_feature_loader.h"

#include "svm_um_handle.h"
#include "svm_kern_log.h"
#include "framework_vma.h"
#include "smm_uk_msg.h"
#include "smm_flag.h"
#include "svm_smp.h"
#include "va_mng.h"
#include "casm_kernel.h"
#include "dma_map_kernel.h"
#include "ubmem_client.h"
#include "ksvmm.h"
#include "pma.h"
#ifndef CFG_FEATURE_NOT_SUPPORT_PMA_UB
#include "pma_ub.h"
#endif
#include "svm_dev_topology.h"
#include "k2umc.h"
#include "share_um_handle.h"

static bool svm_is_master_addr(struct svm_global_va *src_info, int master_tgid)
{
    return ((src_info->udevid == uda_get_host_id()) && (src_info->tgid == master_tgid));
}

static int um_smm_mmap_pre_handle(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len)
{
    struct svm_smm_map_msg *map_msg = (struct svm_smm_map_msg *)msg;
    struct svm_global_va *src_info = &map_msg->src_info;
    int va_type, ret;

    if ((msg_len != sizeof(*map_msg)) || (udevid == uda_get_host_id())) {
        svm_err("Invalid para. (udevid=%u; master_tgid=%d; slave_tgid=%d; msg_len=%u)\n", udevid, master_tgid,
                slave_tgid, msg_len);
        return -EINVAL;
    }

    if (svm_is_k2u_msg(&map_msg->head)) {
        /* K2UMC replay only needs user-mode map processing; kernel resources were prepared by repair. */
        return 0;
    }

    if (svm_smm_get_src_valid_flag(map_msg->flag) == 0) {
        /* cross app map */
        ret = svm_casm_get_src_info(udevid, map_msg->dst_va, map_msg->dst_size, src_info);
        if (ret != 0) {
            svm_err(
                "Casm get src info failed in um pre handle. (udevid=%u; slave_tgid=%d; dst_va=0x%llx; dst_size=%llu)\n",
                udevid, slave_tgid, map_msg->dst_va, map_msg->dst_size);
            return ret;
        }
    } else {
        /* in single app map */
        if (svm_is_master_addr(src_info, master_tgid)) {
            ret = svm_get_current_task_va_type(src_info->va, src_info->size, &va_type);
            if (ret != 0) {
                svm_err("Invalid addr. (udevid=%u; master_tgid=%d; va=0x%llx; size=%llu)\n", udevid, master_tgid,
                        src_info->va, src_info->size);
                return ret;
            }

            if (va_type == VA_TYPE_NON_SVM) {
                map_msg->flag |= SVM_SMM_FLAG_SRC_NON_SVM_VA;
            }
        }

        if ((map_msg->flag & SVM_SMM_FLAG_SRC_NON_SVM_VA) == 0) {
            if (map_msg->dst_task_type == PROCESS_CP1) {
                ret = svm_smp_pin_mem(src_info->udevid, master_tgid, src_info->va, src_info->size, false);
                if (ret != 0) {
                    svm_err("Smm map um pre handle failed. (udevid=%u; master_tgid=%d; va=0x%llx; size=%llu)\n", udevid,
                            master_tgid, src_info->va, src_info->size);
                    return ret;
                }
            } else { /* task grp map & smm */
                ret = svm_smp_check_mem_exists(src_info->udevid, master_tgid, src_info->va, src_info->size);
                if (ret != 0) {
                    svm_err("Smp does not exist; munmap is not allowed. (udevid=%u; master_tgid=%d; va=0x%llx; "
                            "size=%llu)\n",
                            udevid, master_tgid, src_info->va, src_info->size);
                    return ret;
                }
            }
        }

        if (svm_dev_is_ub_connect(udevid, src_info->server_id, src_info->udevid)) {
            u64 maped_va;
            ret = ubmem_map_client(src_info->udevid, src_info, &maped_va);
            if (ret != 0) {
                if ((map_msg->dst_task_type == PROCESS_CP1) && ((map_msg->flag & SVM_SMM_FLAG_SRC_NON_SVM_VA) == 0)) {
                    (void)svm_smp_unpin_mem(src_info->udevid, master_tgid, src_info->va, src_info->size, false);
                }
                svm_err("Ubmem map failed. (udevid=%u; tgid=%d; src_udevid=%u; va=0x%llx; size=%llu)\n", udevid,
                        master_tgid, src_info->udevid, src_info->va, src_info->size);
                return ret;
            }

            if (maped_va != 0) {
                src_info->va = maped_va;
            }
        }
    }

    return 0;
}

static void um_smm_mmap_pre_cancel_handle(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len)
{
    struct svm_smm_map_msg *map_msg = (struct svm_smm_map_msg *)msg;
    struct svm_global_va *src_info = &map_msg->src_info;

    src_info->va = map_msg->src_va;
    if ((msg_len != sizeof(*map_msg)) || (udevid == uda_get_host_id())) {
        svm_err("Invalid para. (udevid=%u; master_tgid=%d; slave_tgid=%d; msg_len=%u)\n", udevid, master_tgid,
                slave_tgid, msg_len);
        return;
    }

    if (svm_is_k2u_msg(&map_msg->head)) {
        /* K2UMC replay did not acquire resources in the normal pre-handle path. */
        return;
    }

    if (svm_smm_get_src_valid_flag(map_msg->flag) != 0) {
        if (svm_dev_is_ub_connect(udevid, src_info->server_id, src_info->udevid)) {
            (void)ubmem_unmap_client(src_info->udevid, src_info);
        }

        if ((map_msg->dst_task_type == PROCESS_CP1) && (svm_smm_get_src_svm_va_flag(map_msg->flag) == 1)) {
            (void)svm_smp_unpin_mem(src_info->udevid, master_tgid, src_info->va, src_info->size, false);
        }
    }
}

static int um_smm_mmap_post_handle(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len)
{
    struct svm_smm_map_msg *map_msg = (struct svm_smm_map_msg *)msg;
    struct svm_global_va *src_info = &map_msg->src_info;
    int ret;

    if ((msg_len != sizeof(*map_msg)) || (udevid == uda_get_host_id())) {
        svm_err("Invalid para. (udevid=%u; master_tgid=%d; slave_tgid=%d; msg_len=%u)\n", udevid, master_tgid,
                slave_tgid, msg_len);
        return -EINVAL;
    }

    if (svm_is_k2u_msg(&map_msg->head)) {
        /* Complete the synchronous K2UMC replay and skip the normal map post-processing. */
        svm_k2umc_finish(map_msg->head.k2u_sync_handle);
        return -EINPROGRESS;
    }

    if ((svm_smm_get_src_svm_va_flag(map_msg->flag) == 1) && (map_msg->dst_task_type == PROCESS_CP1)) {
        ret = ksvmm_add_seg(udevid, master_tgid, map_msg->dst_va, src_info);
        if (ret != 0) {
            svm_err("Ksvmm add seg failed in smm um post handle. (udevid=%u; master_tgid=%d; va=0x%llx; size=%llu)\n",
                    udevid, master_tgid, map_msg->dst_va, map_msg->dst_size);
            return ret;
        }
    }

    return 0;
}

static int um_smm_munmap_pre_handle(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len)
{
    struct svm_smm_unmap_msg *unmap_msg = (struct svm_smm_unmap_msg *)msg;
    struct svm_global_va *src_info = &unmap_msg->src_info;
    int ret;

    if ((msg_len != sizeof(*unmap_msg)) || (udevid == uda_get_host_id())) {
        svm_err("Invalid para. (udevid=%u; master_tgid=%d; slave_tgid=%d; msg_len=%u)\n", udevid, master_tgid,
                slave_tgid, msg_len);
        return -EINVAL;
    }

    if (svm_is_k2u_msg(&unmap_msg->head)) {
        /* K2UMC replay only needs user-mode unmap processing; repair owns the kernel-side flow. */
        return 0;
    }

    if (svm_smm_get_src_valid_flag(unmap_msg->flag) == 0) {
        /* cross app map */
        ret = svm_casm_get_src_info(udevid, unmap_msg->dst_va, unmap_msg->dst_size, src_info);
        if (ret != 0) {
            svm_err(
                "Casm get src info failed in um pre handle. (udevid=%u; slave_tgid=%d; dst_va=0x%llx; dst_size=%llu)\n",
                udevid, slave_tgid, unmap_msg->dst_va, unmap_msg->dst_size);
            return ret;
        }
    } else {
        if (svm_is_master_addr(src_info, master_tgid)) {
            int va_type;
            ret = svm_get_current_task_va_type(src_info->va, src_info->size, &va_type);
            if (ret != 0) {
                svm_err("Invalid addr. (udevid=%u; master_tgid=%d; va=0x%llx; size=%llu)\n", udevid, master_tgid,
                        src_info->va, src_info->size);
                return ret;
            }

            if (va_type == VA_TYPE_NON_SVM) {
                unmap_msg->flag |= SVM_SMM_FLAG_SRC_NON_SVM_VA;
                return 0;
            }
        }

        /* master or agent svm address */
        ret = svm_smp_check_mem_exists(src_info->udevid, master_tgid, src_info->va, src_info->size);
        if ((ret != 0) && (ret != -EOWNERDEAD)) {
            svm_err("Smp does not exist; munmap is not allowed. (udevid=%u; master_tgid=%d; va=0x%llx; size=%llu)\n",
                    udevid, master_tgid, src_info->va, src_info->size);
            return ret;
        }
    }

    if (unmap_msg->dst_task_type == PROCESS_CP1) {
        ret = ksvmm_del_seg(udevid, master_tgid, unmap_msg->dst_va);
        if (ret != 0) {
            svm_err("Ksvmm del seg failed in smm munmap um pre handle. (udevid=%u; master_tgid=%d; va=0x%llx)\n",
                    udevid, master_tgid, unmap_msg->dst_va);
            return ret;
        }

        /* smm src udevid and tgid is same, this is vmm unmap. */
        if ((src_info->udevid == udevid) && (src_info->tgid == slave_tgid)) {
#ifndef CFG_FEATURE_NOT_SUPPORT_PMA_UB
            pma_ub_mem_recycle_notify(udevid, master_tgid, unmap_msg->dst_va, unmap_msg->dst_size);
#endif
            pma_mem_recycle_notify(udevid, master_tgid, unmap_msg->dst_va, unmap_msg->dst_size);
        }
    }

    return 0;
}

static int um_smm_munmap_post_handle(u32 udevid, int master_tgid, int slave_tgid, void *msg, u32 msg_len)
{
    struct svm_smm_unmap_msg *unmap_msg = (struct svm_smm_unmap_msg *)msg;
    struct svm_global_va *src_info = &unmap_msg->src_info;

    if ((msg_len != sizeof(*unmap_msg)) || (udevid == uda_get_host_id())) {
        svm_err("Invalid para. (udevid=%u; master_tgid=%d; slave_tgid=%d; msg_len=%u)\n", udevid, master_tgid,
                slave_tgid, msg_len);
        return -EINVAL;
    }

    if (svm_is_k2u_msg(&unmap_msg->head)) {
        /* Complete the synchronous K2UMC replay and skip the normal unmap post-processing. */
        svm_k2umc_finish(unmap_msg->head.k2u_sync_handle);
        return -EINPROGRESS;
    }

    if (svm_smm_get_src_valid_flag(unmap_msg->flag) != 0) {
        int ret;
        if (svm_dev_is_ub_connect(udevid, src_info->server_id, src_info->udevid)) {
            ret = ubmem_unmap_client(src_info->udevid, src_info);
            if (ret != 0) {
                svm_warn("Ubmem unmap failed. (udevid=%u; va=0x%llx; size=%llu)\n", src_info->udevid, src_info->va,
                         src_info->size);
            }
        }

        if ((unmap_msg->dst_task_type == PROCESS_CP1) && (svm_smm_get_src_svm_va_flag(unmap_msg->flag) == 1)) {
            ret = svm_smp_unpin_mem(src_info->udevid, master_tgid, src_info->va, src_info->size, false);
            if (ret != 0) {
                svm_warn("Unpin failed. (udevid=%u; va=0x%llx; size=%llu)\n", src_info->udevid, src_info->va,
                         src_info->size);
            }
        }
    }
    return 0;
}

int share_um_handle_init(void)
{
    svm_um_register_handle(SVM_SMM_MMAP_EVENT, um_smm_mmap_pre_handle, um_smm_mmap_pre_cancel_handle,
                           um_smm_mmap_post_handle);
    svm_um_register_handle(SVM_SMM_MUNMAP_EVENT, um_smm_munmap_pre_handle, NULL, um_smm_munmap_post_handle);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(share_um_handle_init, FEATURE_LOADER_STAGE_4);
