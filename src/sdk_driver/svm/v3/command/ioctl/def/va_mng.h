/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef VA_MNG_H
#define VA_MNG_H

#include "svm_pub.h"

/*
   total 256T virsual address space:
   1. 4-8T 4T pcieth
   2. 16-224T 208T svm
   3. 224-240 16T share pool
   4. 249-249.5 0.5T ub
*/

#define PCIE_TH_VA_START (4ULL * SVM_BYTES_PER_TB)
#define PCIE_TH_VA_SIZE (4ULL * SVM_BYTES_PER_TB)
#define PCIE_TH_VA_RANGE_COUNT 1
#define SVM_VA_RESERVE_START (16 * SVM_BYTES_PER_TB)
#define SVM_VA_RESERVE_SIZE (208 * SVM_BYTES_PER_TB)
#define SP_VA_START (224ULL * SVM_BYTES_PER_TB)
#define SP_VA_SIZE (16ULL * SVM_BYTES_PER_TB)

#define SVM_VA_STATIC_RESERVE_START SVM_VA_RESERVE_START
#define SVM_VA_STATIC_RESERVE_END (168 * SVM_BYTES_PER_TB)

#define SVM_VA_RESERVE_ALIGN (1 * SVM_BYTES_PER_GB)

#define VA_TYPE_SVM 1
#define VA_TYPE_NON_SVM 2

/* Interface */
static inline void svm_get_pcie_th_va_range(u64 *va, u64 *size)
{
    *va = PCIE_TH_VA_START;
    *size = PCIE_TH_VA_SIZE;
}

/* Interface */
static inline void svm_get_va_range(u64 *va, u64 *size)
{
    *va = SVM_VA_RESERVE_START;
    *size = SVM_VA_RESERVE_SIZE;
}

/* Interface */
static inline bool va_is_in_pcie_th_range(u64 va, u64 size)
{
    u64 start, total_size;
    svm_get_pcie_th_va_range(&start, &total_size);
    return (svm_check_va_range(va, size, start, start + total_size) == 0);
}

/* Interface */
static inline bool va_is_in_sp_range(u64 va, u64 size)
{
    return (svm_check_va_range(va, size, SP_VA_START, SP_VA_START + SP_VA_SIZE) == 0);
}

#endif
