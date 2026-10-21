/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef DBI_DEF_H
#define DBI_DEF_H

#include "pair_dev_info_comm.h"

#include "svm_pub.h"

#define BITS_PER_U64 64U

enum svm_dev_cap {
    SVM_DEV_CAP_BIT_SVA = 0,
    SVM_DEV_CAP_BIT_UBMEM,
    SVM_DEV_CAP_BIT_ASSIGN_GAP,
    SVM_DEV_CAP_BIT_HOST_ACCESS_DEV,
    SVM_DEV_CAP_BIT_MAX,
};

#define CAP_BITMAP_U64_NUM ((SVM_DEV_CAP_BIT_MAX + BITS_PER_U64) / BITS_PER_U64)

typedef u64 svm_dev_cap_t[CAP_BITMAP_U64_NUM];

static inline void svm_dev_cap_set(svm_dev_cap_t cap, u32 cap_bit)
{
    cap[cap_bit / BITS_PER_U64] |= (1ULL << (cap_bit % BITS_PER_U64));
}

static inline void svm_dev_cap_clear(svm_dev_cap_t cap, u32 cap_bit)
{
    cap[cap_bit / BITS_PER_U64] &= ~(1ULL << (cap_bit % BITS_PER_U64));
}

static inline bool svm_dev_cap_is_support(svm_dev_cap_t cap, u32 cap_bit)
{
    return (cap[cap_bit / BITS_PER_U64] & (1ULL << (cap_bit % BITS_PER_U64))) != 0;
}

typedef struct devdrv_pair_info_eid dbi_bus_inst_eid_t;

enum svm_smmu_mode {
    SVM_SMMU_MODE_SVA = 0U,
    SVM_SMMU_MODE_IOVA,
    SVM_SMMU_MODE_RAW_PA,
    SVM_SMMU_MODE_INVALID,
};

#define DBI_EID_FMT "%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x"
#define DBI_EID_RAW_ARGS(eid)                                                                                  \
    eid[0], eid[1], eid[2], eid[3], eid[4], eid[5], eid[6], eid[7], eid[8], eid[9], eid[10], eid[11], eid[12], \
        eid[13], eid[14], eid[15]
#define DBI_EID_ARGS(eid) DBI_EID_RAW_ARGS((eid).raw)

struct svm_device_basic_info {
    u64 npage_size;
    u64 hpage_size;
    u64 gpage_size;
    svm_dev_cap_t cap_flag;
    u32 bus_inst_eid_flag;
    dbi_bus_inst_eid_t bus_inst_eid;
    u64 d2h_acc_mask;
    enum svm_smmu_mode smmu_mode;
    u32 cc_vm; /* only valid for host */
};

#endif
