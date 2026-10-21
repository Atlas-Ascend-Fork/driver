/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef SVM_SUB_UK_MSG_TYPE_H
#define SVM_SUB_UK_MSG_TYPE_H

#include "ascend_hal_define.h"
#include "svm_pub.h"

#define SVM_SUB_EVNET_TYPE_BASE DRV_SUBEVENT_SVM_DEV_OPEN_MSG
#define SVM_SUB_EVNET_TYPE_NUM 32

#define SVM_SUB_EVNET_H2D_TYPE_BASE SVM_SUB_EVNET_TYPE_BASE
#define SVM_SUB_EVNET_H2D_TYPE_NUM 21

#define SVM_SUB_EVNET_D2H_TYPE_BASE (SVM_SUB_EVNET_H2D_TYPE_BASE + SVM_SUB_EVNET_H2D_TYPE_NUM)
#define SVM_SUB_EVNET_D2H_TYPE_NUM 6

#define SVM_SUB_EVNET_K2U_TYPE_BASE (SVM_SUB_EVNET_D2H_TYPE_BASE + SVM_SUB_EVNET_D2H_TYPE_NUM)
#define SVM_SUB_EVNET_K2U_TYPE_NUM 6

/* H2D */
#define SVM_MEMSET_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 0)
#define SVM_MEMCPY_LOCAL_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 1)
#define SVM_MPL_POPULATE_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 2)
#define SVM_MPL_DEPOPULATE_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 3)
#define SVM_MPL_POPULATE_NO_PIN_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 4)
#define SVM_MPL_DEPOPULATE_NO_UNPIN_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 5)
#define SVM_SMM_MMAP_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 6)   /* Esched log process for this ID, do not change it. */
#define SVM_SMM_MUNMAP_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 7) /* Esched log process for this ID, do not change it. */
#define SVM_URMA_CHAN_AGENT_INIT_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 8)
#define SVM_URMA_CHAN_AGENT_UNINIT_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 9)
#define SVM_URMA_SEG_REGISTER_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 10)
#define SVM_URMA_SEG_UNREGISTER_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 11)
#define SVM_GET_MEMINFO_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 12)
#define SVM_VA_RESERVE_EVENT                                                                \
    (SVM_SUB_EVNET_H2D_TYPE_BASE + 13) /* Esched log process for this ID, do not change it. \
                                        */
#define SVM_MADVISE_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 14)
#define SVM_NOTICE_GAP_VA_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 15)
#define SVM_SOMA_POOL_CREATE_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 16)
#define SVM_SOMA_POOL_DESTROY_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 17)
#define SVM_SOMA_POOL_SET_ATTR_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 18)
#define SVM_SOMA_POOL_GET_ATTR_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 19)
#define SVM_SOMA_POOL_SYNC_FREE_EVENT (SVM_SUB_EVNET_H2D_TYPE_BASE + 20)

/* D2H */
#define SVM_ADD_GRP_EVENT (SVM_SUB_EVNET_D2H_TYPE_BASE + 0)
#define SVM_MWL_OP_EVENT (SVM_SUB_EVNET_D2H_TYPE_BASE + 1)

/* K2U */
#define SVM_SMP_DEL_MEM_EVENT (SVM_SUB_EVNET_K2U_TYPE_BASE + 0) /* struct svm_smp_del_msg */
#define SVM_MEM_SHOW_EVENT (SVM_SUB_EVNET_K2U_TYPE_BASE + 1)    /* struct svm_mem_show_msg */
#define SVM_PAGEFAULT_EVENT (SVM_SUB_EVNET_K2U_TYPE_BASE + 2)   /* struct svm_pagefault_msg */

static inline bool svm_sub_event_is_h2d(u32 subevent_id)
{
    return (subevent_id >= SVM_SUB_EVNET_H2D_TYPE_BASE) &&
           (subevent_id < (SVM_SUB_EVNET_H2D_TYPE_BASE + SVM_SUB_EVNET_H2D_TYPE_NUM));
}

static inline bool svm_sub_event_is_d2h(u32 subevent_id)
{
    return (subevent_id >= SVM_SUB_EVNET_D2H_TYPE_BASE) &&
           (subevent_id < (SVM_SUB_EVNET_D2H_TYPE_BASE + SVM_SUB_EVNET_D2H_TYPE_NUM));
}

static inline bool svm_sub_event_is_k2u(u32 subevent_id)
{
    return (subevent_id >= SVM_SUB_EVNET_K2U_TYPE_BASE) &&
           (subevent_id < (SVM_SUB_EVNET_K2U_TYPE_BASE + SVM_SUB_EVNET_K2U_TYPE_NUM));
}

#endif
