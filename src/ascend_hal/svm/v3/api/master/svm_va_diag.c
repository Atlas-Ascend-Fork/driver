/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#include "svm_log.h"
#include "va_allocator.h"
#include "malloc_mng.h"
#include "svm_va_diag.h"

#define SVM_DIAG_MAX_SEG_NUM 32
#define SVM_DIAG_MAX_CHECK_PAGES 4096
#define SVM_DIAG_PAGE_RESIDENT_MASK 0x01

static void svm_diag_dump_prop_item(const char *tag, const char *phase, const struct svm_prop *prop)
{
    svm_run_info("MemcpyVaDiag %s %s. (start=0x%llx; size=%llu; aligned_size=%llu; devid=%u; tgid=%d; flag=0x%llx)\n",
                 tag, phase, prop->start, prop->size, prop->aligned_size, prop->devid, prop->tgid, prop->flag);
}

static void svm_diag_dump_svm_range(u64 va, u64 size, const char *tag)
{
    struct svm_prop prop;
    u64 cur = va;
    u64 end = va + size;
    u32 seg_num = 0;
    int ret;

    ret = svm_get_prop(va, &prop);
    if (ret != DRV_ERROR_NONE) {
        struct svm_prop left_prop, right_prop;
        bool has_left, has_right;

        ret = svm_get_nearby_prop(va, &left_prop, &has_left, &right_prop, &has_right);
        if (ret != DRV_ERROR_NONE) {
            svm_run_info("MemcpyVaDiag %s nearby prop unavailable. (ret=%d; va=0x%llx; size=%llu)\n", tag, ret, va,
                         size);
            return;
        }

        svm_run_info("MemcpyVaDiag %s svm no-hit. (va=0x%llx; size=%llu; has_left=%d; has_right=%d)\n", tag, va, size,
                     has_left, has_right);
        if (has_left) {
            svm_diag_dump_prop_item(tag, "left-near", &left_prop);
        }
        if (has_right) {
            svm_diag_dump_prop_item(tag, "right-near", &right_prop);
        }
        return;
    }

    while ((cur < end) && (seg_num < SVM_DIAG_MAX_SEG_NUM)) {
        ret = svm_get_prop(cur, &prop);
        if (ret != DRV_ERROR_NONE) {
            svm_run_info("MemcpyVaDiag %s svm seg break. (ret=%d; cur=0x%llx; end=0x%llx; seg_num=%u)\n", tag, ret, cur,
                         end, seg_num);
            break;
        }

        if (prop.size == 0) {
            svm_run_info("MemcpyVaDiag %s svm zero-size seg. (cur=0x%llx; seg_num=%u)\n", tag, cur, seg_num);
            break;
        }

        svm_diag_dump_prop_item(tag, "seg", &prop);
        seg_num++;
        cur = prop.start + prop.size;
    }

    if (seg_num >= SVM_DIAG_MAX_SEG_NUM) {
        svm_run_info("MemcpyVaDiag %s svm seg reach limit. (limit=%u; va=0x%llx; size=%llu)\n", tag,
                     SVM_DIAG_MAX_SEG_NUM, va, size);
    }
}

static void svm_diag_dump_non_svm_range(u64 va, u64 size, const char *tag)
{
    long page_size = sysconf(_SC_PAGESIZE);
    u64 start;
    u64 end;
    u64 check_size;
    u64 page_num;
    unsigned char vec[1];
    u64 i;
    u64 step;

    if (page_size <= 0) {
        svm_run_info("MemcpyVaDiag %s non-svm skip. (va=0x%llx; size=%llu; page_size=%ld)\n", tag, va, size, page_size);
        return;
    }

    start = va & ~((u64)page_size - 1);
    end = ((va + size + (u64)page_size - 1) / (u64)page_size) * (u64)page_size;
    if (end <= start) {
        svm_run_info("MemcpyVaDiag %s non-svm overflow. (va=0x%llx; size=%llu; page_size=%ld)\n", tag, va, size,
                     page_size);
        return;
    }
    check_size = end - start;
    page_num = check_size / (u64)page_size;

    /* For large non-SVM ranges, sample pages to reduce syscall overhead.
     * SVM_DIAG_MAX_CHECK_PAGES limits the maximum number of mincore calls. */
    step = (page_num > SVM_DIAG_MAX_CHECK_PAGES) ?
               (page_num + SVM_DIAG_MAX_CHECK_PAGES - 1) / SVM_DIAG_MAX_CHECK_PAGES :
               1;

    for (i = 0; i < page_num; i += step) {
        void *check_va = (void *)(uintptr_t)(start + i * (u64)page_size);
        vec[0] = 0;
        if (mincore(check_va, (size_t)page_size, vec) != 0) {
            svm_run_info("MemcpyVaDiag %s non-svm invalid page. (va=0x%llx; page_idx=%llu; code=%d)\n", tag,
                         (u64)(uintptr_t)check_va, i, errno);
            return;
        }
        if ((vec[0] & SVM_DIAG_PAGE_RESIDENT_MASK) == 0) {
            svm_run_info("MemcpyVaDiag %s non-svm page not resident. (va=0x%llx; page_idx=%llu; vec[0]=0x%02x)\n", tag,
                         (u64)(uintptr_t)check_va, i, vec[0]);
            return;
        }
    }

    if (step > 1) {
        svm_run_info(
            "MemcpyVaDiag %s non-svm range sampled check. (va=0x%llx; size=%llu; total_page=%llu; check_cnt=%llu; "
            "step=%llu)\n",
            tag, va, size, page_num, (page_num + step - 1) / step, step);
    } else {
        svm_run_info("MemcpyVaDiag %s non-svm range mincore check passed. (va=0x%llx; size=%llu; page_num=%llu)\n", tag,
                     va, size, page_num);
    }
}

void svm_diag_dump_va_prop(u64 va, u64 size, const char *tag)
{
    int ret;
    int va_type;
    const char *diag_tag = (tag != NULL) ? tag : "none";

    if (va == 0) {
        svm_run_info("MemcpyVaDiag %s skip zero va. (size=%llu)\n", diag_tag, size);
        return;
    }
    if (size == 0) {
        svm_run_info("MemcpyVaDiag %s skip zero size. (va=0x%llx)\n", diag_tag, va);
        return;
    }

    ret = svm_get_va_type(va, size, &va_type);
    if (ret != DRV_ERROR_NONE) {
        svm_run_info("MemcpyVaDiag %s mixed-or-invalid range. (ret=%d; va=0x%llx; size=%llu)\n", diag_tag, ret, va,
                     size);
        return;
    }

    if (va_type == VA_TYPE_SVM) {
        svm_diag_dump_svm_range(va, size, diag_tag);
        return;
    }

    if (va_type == VA_TYPE_NON_SVM) {
        svm_diag_dump_non_svm_range(va, size, diag_tag);
        return;
    }

    svm_run_info("MemcpyVaDiag %s unknown va type. (va=0x%llx; size=%llu; va_type=%d)\n", diag_tag, va, size, va_type);
}
