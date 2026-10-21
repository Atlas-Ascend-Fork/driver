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
#include "dvpp_svm_mem_ctrl.h"

#include "comm_kernel_interface.h"
#include "svm_mem_query.h"

#include "dvpp_cmdlist_define.h"
#include "dvpp_cmdlist_log.h"

uint64_t dvpp_get_page_size(ka_pid_t pid, uint64_t addr, uint64_t size)
{
    uint64_t page_size;
    struct devmm_svm_process_id process_id = {0};
    process_id.hostpid = pid;

    if ((size == 0) || (addr == 0)) {
        DVPP_CMDLIST_LOG_ERROR("input arg is invalid. (pid=%d, size=%llu)\n", pid, size);
        return 0;
    }

    page_size = (uint64_t)devmm_get_mem_page_size(&process_id, addr, size);
    if (page_size == 0) {
        DVPP_CMDLIST_LOG_ERROR("query page_size is zero. (size=%llu)\n", size);
    }

    return page_size;
}

uint64_t dvpp_get_pa_num(uint64_t addr, uint64_t size, uint64_t page_size)
{
    uint64_t va_aligned_start, va_aligned_end, aligned_size, va_end;
    if ((size == 0) || (addr == 0) || (page_size == 0)) {
        DVPP_CMDLIST_LOG_ERROR("input arg is invalid. (size=%llu, page_size=%llu)\n", size, page_size);
        return 0;
    }
    va_end = addr + size;
    va_aligned_start = DVPP_ALIGN_DOWN(addr, page_size);
    va_aligned_end = DVPP_ALIGN_UP(va_end, page_size);
    aligned_size = va_aligned_end - va_aligned_start;

    return (aligned_size / page_size);
}

int32_t dvpp_get_pa_list_from_svm_addr(ka_pid_t pid, uint64_t addr, uint64_t size, dvpp_svm_pa_info *pa_info)
{
    uint64_t va_aligned_start, va_aligned_end, aligned_size, offset, va_end;
    int32_t ret;
    uint32_t i;

    struct devmm_svm_process_id process_id = {0};
    process_id.hostpid = pid;

    if ((addr == 0) || (size == 0) || (pa_info == NULL) || (pa_info->page_size == 0) || (pa_info->num == 0) ||
        (pa_info->pa_list == NULL) || (pa_info->size_list == NULL)) {
        DVPP_CMDLIST_LOG_ERROR("check input arg fail");
        return -1;
    }

    va_end = addr + size;
    va_aligned_start = DVPP_ALIGN_DOWN(addr, pa_info->page_size);
    va_aligned_end = DVPP_ALIGN_UP(va_end, pa_info->page_size);
    aligned_size = va_aligned_end - va_aligned_start;

    ret = devmm_get_mem_pa_list(&process_id, va_aligned_start, aligned_size, pa_info->pa_list,
                                (uint32_t)(pa_info->num));
    if (ret != 0) {
        DVPP_CMDLIST_LOG_ERROR("vir transfer to phy addr fail. (ret = %d; size = %llu; num = %llu)\n", ret, size,
                               pa_info->num);
        return ret;
    }
    for (i = 0; i < pa_info->num; i++) {
        pa_info->size_list[i] = pa_info->page_size;
    }
    /* adapt start offset */
    offset = addr - va_aligned_start;
    pa_info->pa_list[0] = pa_info->pa_list[0] + offset;
    pa_info->size_list[0] = pa_info->size_list[0] - offset;
    /* adapt end size offset */
    offset = va_aligned_end - va_end;
    pa_info->size_list[pa_info->num - 1] = pa_info->size_list[pa_info->num - 1] - offset;

    return 0;
}

void dvpp_put_pa_list(ka_pid_t pid, uint64_t addr, uint64_t size, dvpp_svm_pa_info *pa_info)
{
    uint64_t va_aligned_start, va_aligned_end, aligned_size, va_end;

    struct devmm_svm_process_id process_id = {0};
    process_id.hostpid = pid;

    if ((addr == 0) || (size == 0) || (pa_info == NULL) || (pa_info->page_size == 0) || (pa_info->num == 0) ||
        (pa_info->pa_list == NULL) || (pa_info->size_list == NULL)) {
        DVPP_CMDLIST_LOG_ERROR("put pa list fail, input arg illegal\n");
        return;
    }

    va_end = addr + size;
    va_aligned_start = DVPP_ALIGN_DOWN(addr, pa_info->page_size);
    va_aligned_end = DVPP_ALIGN_UP(va_end, pa_info->page_size);
    aligned_size = va_aligned_end - va_aligned_start;
    devmm_put_mem_pa_list(&process_id, va_aligned_start, aligned_size, pa_info->pa_list, (uint32_t)(pa_info->num));
}