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

#include "devmm_proc_mem_copy.h"
#include "devmm_common.h"
#include "svm_msg_client.h"
#include "svm_shmem_interprocess.h"
#include "svm_master_dev_capability.h"
#include "svm_res_idr.h"
#include "svm_vmma_mng.h"
#include "svm_heap_mng.h"
#include "svm_master_proc_mng.h"
#include "svm_master_addr_ref_ops.h"
#include "svm_master_memcpy.h"
#ifndef UVM_OPEN
#include "uvm_master_common.h"
#endif
#ifndef DEVMM_UT
#define DEVMM_COPY_765K_LEN 0xC0000ul /* 768K */
#define DEVMM_COPY_256K_PAGE_NUM 64u  /* page num of 256k for 4k page size */
#else                                 /* for ut to go dma async path */
#define DEVMM_COPY_765K_LEN 0x4000ul  /* 16k */
#define DEVMM_COPY_256K_PAGE_NUM 1u   /* page num of 4k for 4k page size */
#endif
#define DEVMM_COPY_1M25_LEN 0x140000ul /* 1M25 */
#define DEVMM_COPY_3M_LEN 0x300000ul   /* 3M */
#define DEVMM_COPY_512K_PAGE_NUM 128u  /* page num of 512k for 4k page size */
#define DEVMM_COPY_1M_PAGE_NUM 256u    /* page num of 1M for 4k page size */
#define DEVMM_COPY_2M_PAGE_NUM 512u    /* page num of 2M for 4k page size */

static ka_atomic_t devmm_task_id = KA_BASE_ATOMIC_INIT(0);
#ifndef UVM_OPEN
struct devmm_uvm_copy_ctx {
    u64 uvm_va;
    u64 other_va;
    ka_rw_semaphore_t *uvm_page_lock;
    struct devmm_memory_attributes src_attr;
    struct devmm_memory_attributes dst_attr;
    struct devmm_mem_copy_convrt_para *task_para;
};

struct devmm_uvm_copy_loop_param {
    u32 lock_idx;
    u64 offset;
    u64 copy_size;
    bool src_is_uvm;
    struct devmm_uvm_copy_page_info *copy_info;
};
#endif
void devmm_find_memcpy_dir(enum devmm_copy_direction *dir, struct devmm_memory_attributes *src_attr,
                           struct devmm_memory_attributes *dst_attr)
{
    /*
     * copy        directions     checking  list:
     * SRC\DST     NOT-SVM        NONPG-SVM HOST-SVM DEV-SVM
     * NOT-SVM     user-cpoy-done H2H       H2H      H2D
     * NONPG-SVM   err            err       err      err
     * HOST-SVM    H2H            H2H       H2H      H2D
     * DEV-SVM     D2H            D2H       D2H      D2D
     */
    if (devmm_is_device_agent(src_attr) && devmm_is_device_agent(dst_attr)) {
        *dir = DEVMM_COPY_DEVICE_TO_DEVICE;
    } else if (devmm_is_device_agent(dst_attr) && devmm_is_master(src_attr)) {
        *dir = DEVMM_COPY_HOST_TO_DEVICE;
    } else if (devmm_is_device_agent(src_attr) && devmm_is_master(dst_attr)) {
        *dir = DEVMM_COPY_DEVICE_TO_HOST;
    } else if (devmm_is_master(src_attr) && devmm_is_master(dst_attr)) {
        *dir = DEVMM_COPY_HOST_TO_HOST;
    } else {
        devmm_drv_err("Don't support this type, show config information. "
                      "(src_attr_is_svm_device=%d; dst_attr_is_svm_host=%d)\n",
                      src_attr->is_svm_device, dst_attr->is_svm_host);
        *dir = DEVMM_COPY_INVILED_DIRECTION;
    }
}

static int devmm_memcpy_range_check(struct devmm_svm_process *svm_proc, u64 src, u64 dst, u64 byte_count)
{
    struct devmm_svm_heap *heap = NULL;

    if (devmm_va_is_not_svm_process_addr(svm_proc, src) == false) {
        heap = devmm_svm_get_heap(svm_proc, (unsigned long)src);
        if ((heap == NULL) || (devmm_check_va_add_size_by_heap(heap, src, byte_count) != 0)) {
            devmm_drv_err("Can't find heap. (src_va=0x%llx; size=%lu)\n", src, byte_count);
            return -EINVAL;
        }
    }

    if (devmm_va_is_not_svm_process_addr(svm_proc, dst) == false) {
        heap = devmm_svm_get_heap(svm_proc, (unsigned long)dst);
        if ((heap == NULL) || (devmm_check_va_add_size_by_heap(heap, dst, byte_count) != 0)) {
            devmm_drv_err("Can't find heap. (dst_va=0x%llx; size=%lu)\n", dst, byte_count);
            return -EINVAL;
        }
    }
    return 0;
}

static int devmm_get_vmma_info(struct devmm_svm_process *svm_proc, u64 va, struct devmm_vmma_info *info)
{
    struct devmm_vmma_struct *vmma = NULL;
    struct devmm_svm_heap *heap = NULL;

    heap = devmm_svm_get_heap(svm_proc, va);
    if (heap == NULL) {
        return -EINVAL;
    }
    vmma = devmm_vmma_get(&heap->vmma_mng, va);
    if (vmma == NULL) {
        return -EINVAL;
    }
    *info = vmma->info;
    devmm_vmma_put(vmma);
    return 0;
}

static bool devmm_va_is_multi_dev_map(struct devmm_svm_process *svm_proc, struct devmm_memory_attributes *attr,
                                      u64 byte_count)
{
    u64 vmma_left_size, total_size, left_size = byte_count, add_size, tmp_va = attr->va;
    bool first_va_is_local = false, current_is_local = false;
    u32 first_va_devid, current_devid;
    struct devmm_vmma_info vmma_info = {0};
    int ret;

    if (attr->is_reserve_addr != true) {
        return false;
    }

    for (total_size = 0; total_size < byte_count;) {
        ret = devmm_get_vmma_info(svm_proc, tmp_va, &vmma_info);
        if (ret != 0) {
            return false;
        }
        current_is_local = (bool)vmma_info.local_handle_flag;
        current_devid = vmma_info.devid;
        vmma_left_size = vmma_info.size - (tmp_va - vmma_info.va);

        if (total_size == 0) {
            first_va_is_local = current_is_local;
            first_va_devid = current_devid;
        } else {
            if ((first_va_is_local != current_is_local) || (first_va_devid != current_devid)) {
                return true;
            }
        }
        add_size = ka_base_min_t(u64, vmma_left_size, left_size);
        total_size += add_size;
        tmp_va += add_size;
        left_size -= add_size;
    }
    return false;
}

STATIC int devmm_memcpy_para_check(struct devmm_svm_process *svm_proc, struct devmm_memory_attributes *src_attr,
                                   struct devmm_memory_attributes *dst_attr, u64 byte_count)
{
    if (src_attr->is_svm_non_page || (byte_count == 0)) {
        devmm_drv_err("Src_attr_va has non page or byte_count is 0. "
                      "(src_attr_va=0x%llx; dst_attr_va=0x%llx; byte_count=%llu)\n",
                      src_attr->va, dst_attr->va, byte_count);
        return -EINVAL;
    }

    if ((src_attr->is_svm_readonly && !src_attr->is_svm_dev_readonly) ||
        (dst_attr->is_svm_readonly && !dst_attr->is_svm_dev_readonly)) {
        devmm_drv_err("Va is readonly and is not dev_readonly, not allowed memcpy. "
                      "(src_attr_va=0x%llx; dst_attr_va=0x%llx; byte_count=%llu)\n",
                      src_attr->va, dst_attr->va, byte_count);
        return -EINVAL;
    }

    if (devmm_is_host_agent(src_attr->devid)) {
        if (!src_attr->is_svm_remote_maped) {
            devmm_drv_err("Src_attr_va host is agent address, but not mapped by master. (src_attr_va=0x%llx)\n",
                          src_attr->va);
            return -EINVAL;
        }
    }

    if (devmm_is_host_agent(dst_attr->devid)) {
        if (!dst_attr->is_svm_remote_maped) {
            devmm_drv_err("Dst_attr_va va host is agent address, but not mapped by master. (dst_attr_va=0x%llx)\n",
                          dst_attr->va);
            return -EINVAL;
        }
    }

    if (devmm_memcpy_range_check(svm_proc, src_attr->va, dst_attr->va, byte_count) != 0) {
        return -EINVAL;
    }

    if (devmm_va_is_multi_dev_map(svm_proc, src_attr, byte_count)) {
        devmm_drv_err("Can not use multi dev map va to memcpy. (src_va=0x%llx; byte_count=%llu\n", src_attr->va,
                      byte_count);
        return -EINVAL;
    }
    if (devmm_va_is_multi_dev_map(svm_proc, dst_attr, byte_count)) {
        devmm_drv_err("Can not use multi dev map va to memcpy. (dst_va=0x%llx; byte_count=%llu\n", dst_attr->va,
                      byte_count);
        return -EINVAL;
    }
    return 0;
}

STATIC int devmm_memcpy_pre_process(struct devmm_svm_process *svm_process, struct devmm_mem_copy_para *copy_para,
                                    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr,
                                    enum devmm_copy_direction *dir)
{
    u64 byte_count = copy_para->ByteCount;
    u64 src = copy_para->src;
    u64 dst = copy_para->dst;
    int ret = 0;

    if (dst_attr->is_svm_non_page) {
        ret = devmm_insert_host_page_range(svm_process, dst, byte_count, dst_attr);
        if (ret != 0) {
            devmm_drv_err("Insert host range failed. (dst=0x%llx; byte_count=%llu)\n", dst, byte_count);
            *dir = DEVMM_COPY_INVILED_DIRECTION;
            return ret;
        }

        dst_attr->is_svm_host = true;
        dst_attr->is_svm_non_page = false;
    }

    devmm_find_memcpy_dir(dir, src_attr, dst_attr);
    if (*dir >= DEVMM_COPY_INVILED_DIRECTION) {
        devmm_drv_err("Direction error. (dir=%d; dst=0x%llx; src=0x%llx; byte_count=%llu)\n", *dir, dst, src,
                      byte_count);
        return -EINVAL;
    }
    copy_para->direction = (copy_para->direction >= DEVMM_COPY_INVILED_DIRECTION) ? *dir : copy_para->direction;
    /* p2p copy, should trans dst addr to host bar, not support va copy */
    dst_attr->copy_use_va = (copy_para->direction == DEVMM_COPY_DEVICE_TO_DEVICE) ? false : dst_attr->copy_use_va;

    return ret;
}

STATIC u32 devmm_memcpy_get_per_page_num(u64 size)
{
    /*
     * size greater than DEVMM_COPY_MIN_LEN_MULTI_BLK(512k), start use multi block
     * according to the test result, the speed of copy in the following page num is higher
     * use 4K page size to get blk num, if page size is 64K or 2M, dma can get higher performance
     * here just computes first async memcpy size, second blk if left size less than 4M submit dma once
     */
    if (size <= DEVMM_COPY_765K_LEN) {
        return DEVMM_COPY_256K_PAGE_NUM; /* page num of 256k for 4k page size */
    } else if (size <= DEVMM_COPY_1M25_LEN) {
        return DEVMM_COPY_512K_PAGE_NUM; /*  page num of 512k for 4k page size */
    } else if (size <= DEVMM_COPY_3M_LEN) {
        return DEVMM_COPY_1M_PAGE_NUM; /*  page num of 1M for 4k page size */
    } else {
        return DEVMM_COPY_2M_PAGE_NUM; /*  page num of 2M for 4k page size */
    }
}

int devmm_memcpy_d2d_process(struct devmm_svm_process *src_proc, struct devmm_memory_attributes *src_attr,
                             struct devmm_svm_process *dst_proc, struct devmm_memory_attributes *dst_attr,
                             struct devmm_mem_copy_para *para)
{
    struct devmm_chan_memcpy_d2d memcpy_msg = {{{0}}};
    int ret;

    if (devmm_get_vfid_from_dev_id(src_attr) != devmm_get_vfid_from_dev_id(dst_attr)) {
        devmm_drv_err("Don't support memcpy d2d in different vf. "
                      "(vfid_from_dev_id_src_attr=%d; vfid_from_dev_id_dst_attr=%d)\n",
                      devmm_get_vfid_from_dev_id(src_attr), devmm_get_vfid_from_dev_id(dst_attr));
        return -EPERM;
    }
    memcpy_msg.head.msg_id = DEVMM_CHAN_MEMCPY_D2D_ID;
    memcpy_msg.head.process_id.hostpid = src_proc->process_id.hostpid;
    memcpy_msg.head.dev_id = (u16)src_attr->devid;
    memcpy_msg.size = para->ByteCount;
    memcpy_msg.src = src_attr->va;
    memcpy_msg.dst = dst_attr->va;
    memcpy_msg.head.process_id.vfid = (u16)devmm_get_vfid_from_dev_id(src_attr);
    memcpy_msg.head.res = (u32)dst_proc->process_id.hostpid;
    ret = devmm_chan_msg_send(&memcpy_msg, sizeof(memcpy_msg), sizeof(struct devmm_chan_msg_head));
    if (ret != 0) {
        devmm_drv_err("Device memcpy message deal failed. (ret=%d; hostpid=%d)\n", ret, src_proc->process_id.hostpid);
        devmm_print_pre_alloced_va(src_proc, src_attr->va);
        devmm_print_pre_alloced_va(dst_proc, dst_attr->va);
    }
    return ret;
}

STATIC int devmm_ioctl_memcpy_process_frame(struct devmm_svm_process *svm_pro, struct devmm_mem_copy_para *copy_para,
                                            struct devmm_memory_attributes *src_attr,
                                            struct devmm_memory_attributes *dst_attr,
                                            struct devmm_mem_copy_convrt_para *para)
{
    u32 page_size = ka_base_min((u32)KA_MM_PAGE_SIZE, devmm_svm->device_page_size);
    size_t count = copy_para->ByteCount;
    u64 src = copy_para->src;
    u64 dst = copy_para->dst;
    size_t byte_offset, def_offset;
    u32 stamp = (u32)ka_jiffies;
    int ret = -EINVAL;

    if (copy_para->direction == DEVMM_COPY_DEVICE_TO_DEVICE) {
        if (src_attr->is_svm_huge && dst_attr->is_svm_huge) {
            page_size = devmm_svm->device_hpage_size;
        }
    }

    def_offset = (size_t)devmm_memcpy_get_per_page_num(count) * page_size;
    para->direction = (u32)copy_para->direction;
    para->blk_size = page_size;
    para->last_seq_flag = false;
    for (byte_offset = 0; byte_offset < count;) {
        if ((byte_offset + def_offset) >= count) {
            para->count = count - byte_offset;
            para->last_seq_flag = true;
        } else {
            para->count = def_offset;
        }
        para->src = src;
        para->dst = dst;
        ret = devmm_ioctl_memcpy_process_res(svm_pro, para, src_attr, dst_attr);
        if (ret != 0) {
            devmm_drv_err_if((ret != -EOPNOTSUPP),
                             "Memcpy error. (ret=%d; src=0x%llx; dst=0x%llx; count=%lu; direction=%u)\n", ret, src, dst,
                             para->count, para->direction);
            if (ret != -EOPNOTSUPP) {
                devmm_print_pre_alloced_va(svm_pro, src);
                devmm_print_pre_alloced_va(svm_pro, dst);
            }
            return ret;
        }
        src += para->count;
        dst += para->count;
        byte_offset += para->count;
        /* left count less than 4M, submit dma once */
        if ((count - byte_offset) < DEVMM_COPY_END_BLK_LEN) {
            def_offset = DEVMM_COPY_END_BLK_LEN;
        }
        para->seq++;
        devmm_try_cond_resched(&stamp);
    }

    return ret;
}

STATIC int devmm_ioctl_p2p_memcpy_process(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *para,
                                          struct devmm_memory_attributes *src_attr,
                                          struct devmm_memory_attributes *dst_attr,
                                          struct devmm_mem_copy_convrt_para *task_para)
{
    int ret;
    struct devmm_memory_attributes src_owner_attr;
    struct devmm_memory_attributes dst_owner_attr;
    struct devmm_svm_process *src_proc = svm_proc;
    struct devmm_svm_process *dst_proc = svm_proc;
    struct devmm_svm_heap *heap = NULL;

    if (src_attr->is_ipc_open) {
        ret = devmm_ipc_get_owner_proc_attr(svm_proc, src_attr, &src_proc, &heap, &src_owner_attr);
        if (ret != 0) {
            devmm_drv_err("Src va get ipc owner attr failed. (src=0x%llx)\n", para->src);
            return ret;
        }

        devmm_drv_debug("Src va use pid va. (src=0x%llx; hostpid=%d; va=0x%llx)\n", para->src,
                        src_proc->process_id.hostpid, src_owner_attr.va);
    } else {
        src_owner_attr = *src_attr;
    }

    if (dst_attr->is_ipc_open) {
        ret = devmm_ipc_get_owner_proc_attr(svm_proc, dst_attr, &dst_proc, &heap, &dst_owner_attr);
        if (ret != 0) {
#ifndef EMU_ST
            if (src_proc != svm_proc) {
                devmm_ipc_put_owner_proc_attr(src_proc, heap);
            }
#endif
            devmm_drv_err("Dst va get ipc owner attr failed. (dst=0x%llx)\n", para->dst);
            return ret;
        }

        devmm_drv_debug("Dst va use pid va. (dst=0X%llx; hostpid=%d; va=0x%llx)\n", para->dst,
                        dst_proc->process_id.hostpid, dst_owner_attr.va);
    } else {
        dst_owner_attr = *dst_attr;
    }

#ifndef EMU_ST
    if (((src_attr->is_mem_import) || (dst_attr->is_mem_import)) &&
        devmm_is_same_dev(src_owner_attr.devid, dst_owner_attr.devid)) {
        devmm_drv_run_info(
            "Vmm mem export does not support same dev d2d drvMemcpy. (src=0x%llx; dst=0x%llx; src_is_import=%d; "
            "dst_is_import=%d)\n",
            src_attr->va, dst_attr->va, src_attr->is_mem_import, dst_attr->is_mem_import);
        return -EOPNOTSUPP;
    }
#endif

    if (devmm_is_same_dev(src_owner_attr.devid, dst_owner_attr.devid)) {
        /* later we use pcie dma local copy */
        ret = devmm_memcpy_d2d_process(src_proc, &src_owner_attr, dst_proc, &dst_owner_attr, para);
#ifndef EMU_ST
        if (src_proc != svm_proc) {
            devmm_ipc_put_owner_proc_attr(src_proc, heap);
        }

        if (dst_proc != svm_proc) {
            devmm_ipc_put_owner_proc_attr(dst_proc, heap);
        }
#endif
    } else {
#ifndef EMU_ST
        if (src_proc != svm_proc) {
            devmm_ipc_put_owner_proc_attr(src_proc, heap);
        }

        if (dst_proc != svm_proc) {
            devmm_ipc_put_owner_proc_attr(dst_proc, heap);
        }
#endif
        /* use h2d or d2h copy two diff device */
        ret = devmm_ioctl_memcpy_process_frame(svm_proc, para, src_attr, dst_attr, task_para);
    }

    return ret;
}

int devmm_check_va_direction(enum devmm_copy_direction para_dir, enum devmm_copy_direction real_dir, u32 task_mode,
                             bool is_memcpy_batch, struct devmm_memory_attributes *dst_attr)
{
    if (para_dir != real_dir) {
        return -EINVAL;
    }
    if (((task_mode == DEVMM_CPY_ASYNC_API_MODE) || (is_memcpy_batch == true)) &&
        !((real_dir == DEVMM_COPY_HOST_TO_DEVICE) || (real_dir == DEVMM_COPY_DEVICE_TO_HOST))) {
        return -EINVAL;
    }

    if (dst_attr->is_svm_dev_readonly && (real_dir != DEVMM_COPY_HOST_TO_DEVICE)) {
        return -EINVAL;
    }
    return 0;
}

STATIC int devmm_normal_memcpy_proc(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                                    struct devmm_memory_attributes *src_attr, struct devmm_memory_attributes *dst_attr,
                                    struct devmm_mem_copy_convrt_para *task_para)
{
    if ((copy_para->direction == DEVMM_COPY_HOST_TO_DEVICE) || (copy_para->direction == DEVMM_COPY_DEVICE_TO_HOST)) {
        return devmm_ioctl_memcpy_process_frame(svm_proc, copy_para, src_attr, dst_attr, task_para);
    } else if (copy_para->direction == DEVMM_COPY_DEVICE_TO_DEVICE) {
        return devmm_ioctl_p2p_memcpy_process(svm_proc, copy_para, src_attr, dst_attr, task_para);
    } else if (copy_para->direction == DEVMM_COPY_HOST_TO_HOST) {
        devmm_svm_stat_copy_inc(DEVMM_COPY_HOST_TO_HOST, copy_para->ByteCount);
    } else {
        devmm_drv_err("Direction error. (direction=%d; src=0x%llx; dst=0x%llx)\n", copy_para->direction, copy_para->src,
                      copy_para->dst);
        return -EINVAL;
    }

    return 0;
}

struct devmm_memcpy_addr_info {
    u64 va;
    u64 size;
    u32 logical_devid;
    bool local_is_dev;
};

static void devmm_memcpy_addr_info_pack(u64 va, u64 size, u32 logical_devid, bool local_is_dev,
                                        struct devmm_memcpy_addr_info *info)
{
    info->va = va;
    info->size = size;
    info->logical_devid = logical_devid;
    info->local_is_dev = local_is_dev;
}

static int _devmm_get_memcpy_mem_attrs(struct devmm_svm_process *svm_proc, struct devmm_memcpy_addr_info *info,
                                       struct devmm_memory_attributes *attr)
{
    if (devmm_is_static_soma_reserve_addr(svm_proc, info->va)) {
        return devmm_get_soma_mem_attrs(svm_proc, info->va, attr);
    }
    if (devmm_va_is_not_svm_process_addr(svm_proc, info->va)) {
        if (info->local_is_dev) {
            return devmm_get_local_dev_mem_attrs(svm_proc, info->va, info->size, info->logical_devid, attr);
        } else {
            devmm_get_local_host_mem_attrs(svm_proc, info->va, attr);
            return 0;
        }
    } else {
        return devmm_get_svm_mem_attrs(svm_proc, info->va, attr);
    }
}

static int devmm_get_memcpy_mem_attrs(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                                      u32 logical_devid, struct devmm_memory_attributes *src_attr,
                                      struct devmm_memory_attributes *dst_attr)
{
    struct devmm_memcpy_addr_info addr_info;
    bool local_is_dev;
    int ret = 0;

    local_is_dev = (copy_para->is_support_dev_local_addr && (copy_para->direction == DEVMM_COPY_DEVICE_TO_HOST));
    devmm_memcpy_addr_info_pack(copy_para->src, copy_para->ByteCount, logical_devid, local_is_dev, &addr_info);
    ret = _devmm_get_memcpy_mem_attrs(svm_proc, &addr_info, src_attr);
    if (ret != 0) {
        devmm_drv_err("Get src attrs failed. (addr=0x%llx)\n", copy_para->src);
        return ret;
    }

    local_is_dev = (copy_para->is_support_dev_local_addr && (copy_para->direction == DEVMM_COPY_HOST_TO_DEVICE));
    devmm_memcpy_addr_info_pack(copy_para->dst, copy_para->ByteCount, logical_devid, local_is_dev, &addr_info);
    ret = _devmm_get_memcpy_mem_attrs(svm_proc, &addr_info, dst_attr);
    if (ret != 0) {
        devmm_drv_err("Get dst attrs failed. (addr=0x%llx)\n", copy_para->dst);
        return ret;
    }

    return 0;
}

static bool devmm_check_memcpy_is_allow(struct devmm_svm_process *svm_proc, enum devmm_copy_direction real_dir,
                                        u32 task_mode, struct devmm_memory_attributes *src_attr,
                                        struct devmm_memory_attributes *dst_attr)
{
    if (task_mode == DEVMM_CPY_ASYNC_API_MODE) {
        u32 phy_devid = (real_dir == DEVMM_COPY_HOST_TO_DEVICE) ? dst_attr->devid : src_attr->devid;

        return devmm_proc_dev_is_async_allow(svm_proc, phy_devid);
    }

    return true;
}

static int devmm_memcpy_batch_mem_attr_check(struct devmm_mem_copy_convrt_para *task_para,
                                             struct devmm_memory_attributes *src_attr,
                                             struct devmm_memory_attributes *dst_attr, enum devmm_copy_direction dir)
{
    if (task_para->is_memcpy_batch) {
        if (((dir == DEVMM_COPY_HOST_TO_DEVICE) && (dst_attr->is_mem_import || dst_attr->is_ipc_open)) ||
            ((dir == DEVMM_COPY_DEVICE_TO_HOST) && (src_attr->is_mem_import || src_attr->is_ipc_open))) {
            devmm_drv_run_info(
                "Memcpy batch,not support ipc_open or mem_import. (src=0x%llx; dst=0x%llx; dst_is_memory_import=%d; "
                "dst_is_ipc_open=%d; "
                " src_is_memory_import=%d; src_is_ipc_open=%d)\n",
                src_attr->va, dst_attr->va, dst_attr->is_mem_import, dst_attr->is_ipc_open, src_attr->is_mem_import,
                src_attr->is_ipc_open);
            return -EOPNOTSUPP;
        }
    }

    if (task_para->is_memcpy_batch && (!task_para->is_memcpy_batch_first_addr)) {
        if (((dir == DEVMM_COPY_HOST_TO_DEVICE) && (task_para->dev_id != dst_attr->devid)) ||
            ((dir == DEVMM_COPY_DEVICE_TO_HOST) && (task_para->dev_id != src_attr->devid))) {
            u32 cur_cpy_devid = (task_para->direction == DEVMM_COPY_HOST_TO_DEVICE) ? dst_attr->devid : src_attr->devid;
            devmm_drv_err(
                "Memcpy batch,addr devid diff from first cpy devid. (src=0x%llx; dst=0x%llx; cur_cpy_devid=%u; "
                "first_cpy_devid=%u)\n",
                dst_attr->va, src_attr->va, cur_cpy_devid, task_para->dev_id);
            return -EINVAL;
        }
    }

    return 0;
}

/* Copy Scenarios and Policies:
   1. pcie dma support sva, use process va to copy. if src and dst both use va, we directly commit a dma copy descriptor
      a. device addr use va.
      b. host locked addr, if mapped by device, use va.
      c. in same os, p2p copy, src and dst both use va.
      d. in different os, p2p copy, src and dst both use va, user can call prefetch create p2p page table to
         Improve performance. Otherwise, a page fault occurs.
      e. ipc open addr, not to replace with it`s associated ipc create addr.

   2. host agent addr, just support p2p copy; and it must be mapped by master, so we use master process page to
      replace it, copy between host agent and device agent, converted to H2D or D2H.

   3. the host and device are interconnected through HCCS, The rules of host addr are as follows:
      a. if device pcie dma support sva and host locked addr is mapped by device, directly use va.
      b. otherwise, if run in host machine, use host pa.
      c. otherwise, if run in virtual machine, send host ipa to device to search pa.

   4. ipc open addr, host not storing it`s dma addr, use it`s associated ipc create addr to search dma addr.

   5. pcie dma not support sva, host and device are interconnected through pcie
      a. same device p2p copy, send msg to device translate uva to kva, then copy.
      b. different device p2p copy(include same os or different os), one device addr use dma addr, the other use host
         bar dma addr(dma addr mapped by first device).
      c. h2d or d2h copy, both use it`s dma addr.
*/
STATIC int devmm_memcpy_proc(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                             u32 logical_devid, struct devmm_mem_copy_convrt_para *task_para)
{
    enum devmm_copy_direction dir = DEVMM_COPY_INVILED_DIRECTION;
    struct devmm_memory_attributes src_attr = {0};
    struct devmm_memory_attributes dst_attr = {0};
    int ret;

    ret = devmm_get_memcpy_mem_attrs(svm_proc, copy_para, logical_devid, &src_attr, &dst_attr);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_memcpy_para_check(svm_proc, &src_attr, &dst_attr, copy_para->ByteCount);
    if (ret != 0) {
        devmm_drv_err("Memcpy para check failed. (src_device_id=%d; dst_device_id=%d)\n", src_attr.devid,
                      dst_attr.devid);
        return -EINVAL;
    }

    ret = devmm_memcpy_pre_process(svm_proc, copy_para, &src_attr, &dst_attr, &dir);
    if (ret != 0) {
        devmm_drv_err("Memcpy pre process fail. (src=0x%llx; dst=0x%llx; count=0x%lx)\n", copy_para->src,
                      copy_para->dst, copy_para->ByteCount);
        return ret;
    }

    ret = devmm_check_va_direction(copy_para->direction, dir, task_para->task_mode, task_para->is_memcpy_batch,
                                   &dst_attr);
    if (ret != 0) {
        devmm_drv_err("Copy direction error. (expected_direction=%u; real_direction=%u; src=0x%llx; "
                      "dst=0x%llx; dst_is_dev_readonly=%d; is_memcpy_batch=%d)\n",
                      copy_para->direction, dir, copy_para->src, copy_para->dst, dst_attr.is_svm_dev_readonly,
                      task_para->is_memcpy_batch);
        return ret;
    }

    if (devmm_check_memcpy_is_allow(svm_proc, dir, task_para->task_mode, &src_attr, &dst_attr) == false) {
#ifndef EMU_ST
        devmm_drv_err("Device is reset, memcpy is not allowed. (dir=%u; task_mode=%u; src_dev=%u; dst_dev=%u)\n", dir,
                      task_para->task_mode, src_attr.devid, dst_attr.devid);
        return -ENONET;
#endif
    }
    ret = devmm_memcpy_batch_mem_attr_check(task_para, &src_attr, &dst_attr, dir);
    if (ret != 0) {
        devmm_drv_err("Memcpy batch mem attr check failed. (ret=%d)\n", ret);
        return ret;
    }
    return devmm_normal_memcpy_proc(svm_proc, copy_para, &src_attr, &dst_attr, task_para);
}
#ifndef UVM_OPEN
struct devmm_uvm_copy_page_info {
    enum devmm_copy_direction dir;
    u64 src;
    u64 dst;
    u64 size;
    struct devmm_memory_attributes src_attr;
    struct devmm_memory_attributes dst_attr;
};

static int devmm_uvm_memcpy_lock_src_page(struct devmm_svm_process *svm_proc,
                                          struct devmm_uvm_copy_page_info *copy_info, uint64_t *src_lock_end)
{
    ka_rw_semaphore_t *src_lock = NULL;
    src_lock = uvm_get_page_lock_array(svm_proc->uvm_heap, copy_info->src);
    if (src_lock == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Uvm get page lock array failed. (va=0x%llx)\n", copy_info->src);
        return -EFAULT;
#endif
    }

    if (*src_lock_end <= copy_info->src) {
        ka_task_down_read(src_lock);
        *src_lock_end = ka_base_round_down(copy_info->src, DEVMM_UVM_PAGE_SIZE) + DEVMM_UVM_PAGE_SIZE;
    }

    return 0;
}

static int devmm_uvm_memcpy_lock_dst_page(struct devmm_svm_process *svm_proc,
                                          struct devmm_uvm_copy_page_info *copy_info, uint64_t *dst_lock_end)
{
    ka_rw_semaphore_t *dst_lock = NULL;

    dst_lock = uvm_get_page_lock_array(svm_proc->uvm_heap, copy_info->dst);
    if (dst_lock == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Uvm get page lock array failed. (va=0x%llx)\n", copy_info->dst);
        return -EFAULT;
#endif
    }

    if (*dst_lock_end <= copy_info->dst) {
        ka_task_down_write(dst_lock);
        *dst_lock_end = ka_base_round_down(copy_info->dst, DEVMM_UVM_PAGE_SIZE) + DEVMM_UVM_PAGE_SIZE;
    }

    return 0;
}

static int devmm_uvm_memcpy_check_source(struct devmm_svm_process *svm_proc, struct devmm_uvm_copy_page_info *copy_info)
{
    int ret;

    ret = devmm_get_uvm_mem_attrs(svm_proc, copy_info->src, &copy_info->src_attr);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Get src attrs failed. (addr=0x%llx)\n", copy_info->src);
        return ret;
#endif
    }

    if (copy_info->src_attr.is_uvm_non_page) {
#ifndef EMU_ST
        devmm_drv_err("Src addr is no mapped. (va=0x%llx)\n", copy_info->src_attr.va);
        return -EFAULT;
#endif
    }

    return 0;
}

static int devmm_uvm_memcpy_check_destination(struct devmm_svm_process *svm_proc,
                                              struct devmm_uvm_copy_page_info *copy_info)
{
    ka_vm_area_struct_t *vma = NULL;
    struct uvm_page_info page_info = {0};
    int ret;

    ret = uvm_get_page_info_by_va(svm_proc->uvm_heap, copy_info->dst, &page_info);
    if (ret) {
        return ret;
    }

    if (!page_bitmap_get_alloced(page_info.page_bitmap)) {
#ifndef EMU_ST
        devmm_drv_err("Uvm dst page was not alloced. (dst=0x%llx)\n", copy_info->dst);
        return -EFAULT;
#endif
    }

    ret = devmm_get_uvm_mem_attrs(svm_proc, copy_info->dst, &copy_info->dst_attr);
    if (ret != 0) {
        return ret;
    }

    if (copy_info->dst_attr.is_uvm_non_page) {
        ret = uvm_host_alloc_and_map_page(svm_proc, &page_info, page_info.va_align, -1, false);
        if (ret != 0) {
            devmm_drv_err("Uvm_prealloc_page_host failed. (ret=%d, va=0x%llx)\n", ret, page_info.va_align);
            return ret;
        }
        page_bitmap_set_host_mapped(page_info.page_bitmap);
        copy_info->dst_attr.is_uvm_non_page = false;
        copy_info->dst_attr.is_uvm_host = true;
    } else if (copy_info->dst_attr.is_uvm_read_mostly) {
        ret = uvm_free_device_page(svm_proc, &page_info, 0, false);
        if (ret) {
            devmm_drv_err("Failed to free device va. (va=0x%llx)\n", page_info.va_align);
            return ret;
        }

        vma = devmm_find_vma(svm_proc, page_info.va_align);
        if (vma == NULL) {
            devmm_drv_err("Find vma error. (va=0x%llx)\n", page_info.va_align);
            return -EADDRNOTAVAIL;
        }

        ret = uvm_reset_ptes_of_hugepage(svm_proc, vma, page_info.va_align, false);
        if (ret != 0) {
            devmm_drv_err("UVM reset ptes of hugepage failed. (vaddr=0x%llx)\n", page_info.va_align);
        }

        page_bitmap_unset_read_mostly(page_info.page_bitmap);
        copy_info->dst_attr.is_uvm_host = true;
        copy_info->dst_attr.is_uvm_device = false;
        copy_info->dst_attr.is_uvm_read_mostly = false;
    }

    return 0;
}

static bool devmm_uvm_compare_memcpy_info(struct devmm_uvm_copy_page_info *copy_info,
                                          struct devmm_uvm_copy_page_info *copy_info_pre)
{
    if (copy_info->dir == copy_info_pre->dir) {
        if (copy_info->dir == DEVMM_COPY_HOST_TO_DEVICE) {
            return copy_info->dst_attr.devid == copy_info_pre->dst_attr.devid;
        } else if (copy_info->dir == DEVMM_COPY_DEVICE_TO_HOST) {
            return copy_info->src_attr.devid == copy_info_pre->src_attr.devid;
        } else if (copy_info->dir == DEVMM_COPY_DEVICE_TO_DEVICE) {
            return (copy_info->src_attr.devid == copy_info_pre->src_attr.devid) &&
                   (copy_info->dst_attr.devid == copy_info_pre->dst_attr.devid);
        } else if (copy_info->dir == DEVMM_COPY_HOST_TO_HOST) {
            return true;
        }
    }

    return false;
}

static int devmm_uvm_memcpy_page_pre_process(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                                             struct devmm_uvm_copy_page_info *copy_info, uint64_t *src_lock_end,
                                             uint64_t *dst_lock_end)
{
    int ret = 0;

    if (copy_para->src < copy_para->dst) {
        ret = devmm_uvm_memcpy_lock_src_page(svm_proc, copy_info, src_lock_end);
        if (ret != 0) {
            return ret;
        }

        ret = devmm_uvm_memcpy_lock_dst_page(svm_proc, copy_info, dst_lock_end);
        if (ret != 0) {
            return ret;
        }
    } else {
        ret = devmm_uvm_memcpy_lock_dst_page(svm_proc, copy_info, dst_lock_end);
        if (ret != 0) {
            return ret;
        }

        ret = devmm_uvm_memcpy_lock_src_page(svm_proc, copy_info, src_lock_end);
        if (ret != 0) {
            return ret;
        }
    }

    ret = devmm_uvm_memcpy_check_source(svm_proc, copy_info);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_uvm_memcpy_check_destination(svm_proc, copy_info);
    if (ret != 0) {
        return ret;
    }

    devmm_find_memcpy_dir(&copy_info->dir, &copy_info->src_attr, &copy_info->dst_attr);

    return 0;
}

static int devmm_uvm_memcpy_post_process(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                                         uint64_t begin_offset, uint64_t *src_lock_end, uint64_t *dst_lock_end)
{
    ka_rw_semaphore_t *src_start = NULL;
    ka_rw_semaphore_t *src_end = NULL;
    ka_rw_semaphore_t *dst_start = NULL;
    ka_rw_semaphore_t *dst_end = NULL;
    u32 i = 0;

    src_start = uvm_get_page_lock_array(svm_proc->uvm_heap, copy_para->src + begin_offset);
    if (src_start == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Uvm get page lock array failed. (va=0x%llx)\n", copy_para->src + begin_offset);
        return -EFAULT;
#endif
    }

    src_end = uvm_get_page_lock_array(svm_proc->uvm_heap, *src_lock_end - 1);
    if (src_end == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Uvm get page lock array failed. (va=0x%llx)\n", *src_lock_end - 1);
        return -EFAULT;
#endif
    }

    i = 0;
    while (src_start <= src_end) {
        ka_task_up_read(src_start);
        src_start++;
        i++;
    }
    *src_lock_end = 0;

    dst_start = uvm_get_page_lock_array(svm_proc->uvm_heap, copy_para->dst + begin_offset);
    if (dst_start == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Uvm get page lock array failed. (va=0x%llx)\n", copy_para->dst + begin_offset);
        return -EFAULT;
#endif
    }

    dst_end = uvm_get_page_lock_array(svm_proc->uvm_heap, *dst_lock_end - 1);
    if (dst_end == NULL) {
#ifndef EMU_ST
        devmm_drv_err("Uvm get page lock array failed. (va=0x%llx)\n", *dst_lock_end - 1);
        return -EFAULT;
#endif
    }

    i = 0;
    while (dst_start <= dst_end) {
        ka_task_up_write(dst_start);
        dst_start++;
        i++;
    }
    *dst_lock_end = 0;

    return 0;
}

static int devmm_uvm_memcpy_h2h_process(struct devmm_svm_process *svm_proc, struct devmm_chan_uvm_memcpy *memcpy_msg,
                                        struct devmm_uvm_copy_page_info *copy_info)
{
    struct uvm_page_info page_info = {0};
    void *src_kva = NULL;
    void *dst_kva = NULL;
    ka_page_t *src_pg = NULL;
    ka_page_t *dst_pg = NULL;
    int ret, i;

    for (i = 0; i < memcpy_msg->page_num; ++i) {
        ret = uvm_get_page_info_by_va(svm_proc->uvm_heap, memcpy_msg->src_addr[i], &page_info);
        if (ret != 0) {
            devmm_drv_err("uvm_get_page_info_by_va failed in devmm_uvm_memcpy_h2h_process. (va=0x%llx;)\n",
                          memcpy_msg->src_addr[i]);
            return ret;
        }

        src_pg = devmm_pa_to_page(page_info.page_map->pa_addr);
        if (src_pg == NULL) {
            devmm_drv_err("devmm_pa_to_page failed in devmm_uvm_memcpy_h2h_process. (va=0x%llx, pa=0x%llx;)\n",
                          memcpy_msg->src_addr[i], page_info.page_map->pa_addr);
            return -EINVAL;
        }

#ifdef EMU_ST
        src_pg = (ka_page_t *)((uint64_t)src_pg >> KA_MM_PAGE_SHIFT);
#endif

        src_kva = ka_mm_page_address(src_pg) + memcpy_msg->src_addr[i] - page_info.va_align;
        ret = uvm_get_page_info_by_va(svm_proc->uvm_heap, memcpy_msg->dst_addr[i], &page_info);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("uvm_get_page_info_by_va failed. (va=0x%llx;)\n", memcpy_msg->dst_addr[i]);
            return ret;
#endif
        }

        dst_pg = devmm_pa_to_page(page_info.page_map->pa_addr);
        if (dst_pg == NULL) {
#ifndef EMU_ST
            devmm_drv_err("devmm_pa_to_page failed. (va=0x%llx, pa=0x%llx;)\n", memcpy_msg->dst_addr[i],
                          page_info.page_map->pa_addr);
            return -EINVAL;
#endif
        }
#ifdef EMU_ST
        dst_pg = (ka_page_t *)((uint64_t)dst_pg >> KA_MM_PAGE_SHIFT);
#endif
        dst_kva = ka_mm_page_address(dst_pg) + memcpy_msg->dst_addr[i] - page_info.va_align;

#ifndef EMU_ST
        ret = memcpy_s(dst_kva, memcpy_msg->copy_size[i], src_kva, memcpy_msg->copy_size[i]);
        if (ret != 0) {
            devmm_drv_err("memcpy_s failed. (va=0x%llx)\n", dst_kva);
            return ret;
        }
#endif
    }

    return 0;
}

static int devmm_uvm_memcpy_h2d_process(struct devmm_svm_process *svm_proc, struct devmm_chan_uvm_memcpy *memcpy_msg,
                                        struct devmm_uvm_copy_page_info *copy_info)
{
    struct devmm_addr_info *addr_info = NULL;
    struct uvm_page_info page_info = {0};
    ka_page_t *src_pg = NULL;
    int ret, i, j;
    u32 devid, offset;
    u64 src_begin;

    addr_info = (struct devmm_addr_info *)devmm_kvalloc(sizeof(struct devmm_addr_info) * UVM_MEMCPY_MSG_MAX_PAGE_NUM,
                                                        KA_GFP_KERNEL | __KA_GFP_ACCOUNT | __KA_GFP_ZERO);
    if (addr_info == NULL) {
        devmm_drv_err("Kzalloc for struct devmm_addr_info failed.\n");
        return -ENOMEM;
    }

    devid = copy_info->dst_attr.devid;
    src_begin = memcpy_msg->src_addr[0];

    memcpy_msg->head.dev_id = devid;
    memcpy_msg->head.msg_id = DEVMM_CHAN_UVM_MEMCPY_H2D_ID;
    memcpy_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    memcpy_msg->head.process_id.vfid = svm_proc->process_id.vfid;

    for (i = 0; i < memcpy_msg->page_num; ++i) {
        ret = uvm_get_page_info_by_va(svm_proc->uvm_heap, memcpy_msg->src_addr[i], &page_info);
        if (ret != 0) {
            devmm_drv_err("uvm_get_page_info_by_va failed in devmm_uvm_memcpy_h2d_process. (va=0x%llx;)\n",
                          memcpy_msg->src_addr[i]);
#ifndef EMU_ST
            goto unmap_dma;
#endif
        }

        src_pg = devmm_pa_to_page(page_info.page_map->pa_addr);
        if (src_pg == NULL) {
            devmm_drv_err("devmm_pa_to_page failed in devmm_uvm_memcpy_h2d_process. (va=0x%llx, pa=0x%llx;)\n",
                          memcpy_msg->src_addr[i], page_info.page_map->pa_addr);
#ifndef EMU_ST
            goto unmap_dma;
#endif
        }
        offset = memcpy_msg->src_addr[i] - ka_base_round_down(memcpy_msg->src_addr[i], DEVMM_UVM_PAGE_SIZE);
        ret = devmm_dma_map_page_partial(devid, src_pg, offset, memcpy_msg->copy_size[i], NULL, &addr_info[i]);
        if (ret != 0) {
            devmm_drv_err("Host dma map page failed. (dev_id=%u; va=0x%llx; offset=%u)\n", devid,
                          memcpy_msg->src_addr[i], offset);
            ret = -EIO;
            goto unmap_dma;
        }

        memcpy_msg->src_addr[i] = addr_info[i].addr;
    }

    ret = devmm_chan_msg_send(memcpy_msg, sizeof(struct devmm_chan_uvm_memcpy), 0);
    if (ret != 0) {
        devmm_drv_err("Uvm H2D memcpy failed. (ret=%d; dev_id=%u; src=0x%llx; dst=0x%llx)\n", ret, devid, src_begin,
                      memcpy_msg->dst_addr[0]);
    }

unmap_dma:
    for (j = 0; j < i; ++j) {
        devmm_dma_unmap_page(NULL, &addr_info[j]);
    }
    devmm_kvfree(addr_info);

    return ret;
}

static int devmm_uvm_memcpy_d2h_process(struct devmm_svm_process *svm_proc, struct devmm_chan_uvm_memcpy *memcpy_msg,
                                        struct devmm_uvm_copy_page_info *copy_info)
{
    struct devmm_addr_info *addr_info = NULL;
    struct uvm_page_info page_info = {0};
    ka_page_t *dst_pg = NULL;
    int ret, i, j;
    u32 devid, offset;
    u64 dst_begin;

    addr_info = (struct devmm_addr_info *)devmm_kvalloc(sizeof(struct devmm_addr_info) * UVM_MEMCPY_MSG_MAX_PAGE_NUM,
                                                        KA_GFP_KERNEL | __KA_GFP_ACCOUNT | __KA_GFP_ZERO);
    if (addr_info == NULL) {
        devmm_drv_err("Kzalloc for struct devmm_addr_info failed.\n");
        return -ENOMEM;
    }

    devid = copy_info->src_attr.devid;
    dst_begin = memcpy_msg->dst_addr[0];

    memcpy_msg->head.dev_id = devid;
    memcpy_msg->head.msg_id = DEVMM_CHAN_UVM_MEMCPY_D2H_ID;
    memcpy_msg->head.process_id.hostpid = svm_proc->process_id.hostpid;
    memcpy_msg->head.process_id.vfid = svm_proc->process_id.vfid;

    for (i = 0; i < memcpy_msg->page_num; ++i) {
        ret = uvm_get_page_info_by_va(svm_proc->uvm_heap, memcpy_msg->dst_addr[i], &page_info);
        if (ret != 0) {
            devmm_drv_err("uvm_get_page_info_by_va failed. (va=0x%llx;)\n", memcpy_msg->dst_addr[i]);
            return ret;
        }

        dst_pg = devmm_pa_to_page(page_info.page_map->pa_addr);
        if (dst_pg == NULL) {
            devmm_drv_err("devmm_pa_to_page failed. (va=0x%llx, pa=0x%llx;)\n", memcpy_msg->dst_addr[i],
                          page_info.page_map->pa_addr);
            return -EINVAL;
        }

        offset = memcpy_msg->dst_addr[i] - ka_base_round_down(memcpy_msg->dst_addr[i], DEVMM_UVM_PAGE_SIZE);
        ret = devmm_dma_map_page_partial(devid, dst_pg, offset, memcpy_msg->copy_size[i], NULL, &addr_info[i]);
        if (ret != 0) {
            devmm_drv_err("Host dma map page failed. (dev_id=%u; va=0x%llx; offset=%u)\n", devid,
                          memcpy_msg->dst_addr[i], offset);
            ret = -EIO;
            goto unmap_dma;
        }

        memcpy_msg->dst_addr[i] = addr_info[i].addr;
    }

    ret = devmm_chan_msg_send(memcpy_msg, sizeof(struct devmm_chan_uvm_memcpy), 0);
    if (ret != 0) {
        devmm_drv_err("Uvm D2H memcpy failed. (ret=%d; dev_id=%u; src=0x%llx; dst=0x%llx)\n", ret, devid,
                      memcpy_msg->src_addr[0], dst_begin);
    }

unmap_dma:
    for (j = 0; j < i; ++j) {
        devmm_dma_unmap_page(NULL, &addr_info[j]);
    }
    devmm_kvfree(addr_info);

    return ret;
}

static int devmm_uvm_memcpy_process_frame(struct devmm_svm_process *svm_proc, struct devmm_chan_uvm_memcpy *memcpy_msg,
                                          struct devmm_uvm_copy_page_info *copy_info)
{
    if (copy_info->dir == DEVMM_COPY_HOST_TO_DEVICE) {
        return devmm_uvm_memcpy_h2d_process(svm_proc, memcpy_msg, copy_info);
    } else if (copy_info->dir == DEVMM_COPY_DEVICE_TO_HOST) {
        return devmm_uvm_memcpy_d2h_process(svm_proc, memcpy_msg, copy_info);
    } else if (copy_info->dir == DEVMM_COPY_HOST_TO_HOST) {
        return devmm_uvm_memcpy_h2h_process(svm_proc, memcpy_msg, copy_info);
    } else if (copy_info->dir == DEVMM_COPY_DEVICE_TO_DEVICE) {
        devmm_drv_err("Direction error. (direction=%d; src=0x%llx; dst=0x%llx)\n", copy_info->dir,
                      memcpy_msg->src_addr[0], memcpy_msg->dst_addr[0]);
        return 0;
    } else {
        devmm_drv_err("Direction error. (direction=%d; src=0x%llx; dst=0x%llx)\n", copy_info->dir,
                      memcpy_msg->src_addr[0], memcpy_msg->dst_addr[0]);
        return -EINVAL;
    }

    return 0;
}

STATIC int devmm_uvm_calc_page_copy(struct devmm_mem_copy_para *copy_para, u64 offset,
                                    struct devmm_uvm_copy_page_info *copy_info)
{
    u64 src_page_remain, dst_page_remain, copy_size;

    src_page_remain = ka_base_round_up(copy_para->src + offset, DEVMM_UVM_PAGE_SIZE) - copy_para->src - offset;
    if (src_page_remain == 0) {
        src_page_remain = DEVMM_UVM_PAGE_SIZE;
    }

    dst_page_remain = ka_base_round_up(copy_para->dst + offset, DEVMM_UVM_PAGE_SIZE) - copy_para->dst - offset;
    if (dst_page_remain == 0) {
        dst_page_remain = DEVMM_UVM_PAGE_SIZE;
    }

    copy_size = ka_base_min(ka_base_min(src_page_remain, dst_page_remain), copy_para->ByteCount - offset);
    copy_info->src = copy_para->src + offset;
    copy_info->dst = copy_para->dst + offset;
    copy_info->size = copy_size;

    return copy_size;
}

STATIC int devmm_uvm_page_copy(struct devmm_svm_process *svm_proc, struct devmm_chan_uvm_memcpy *memcpy_msg,
                               struct devmm_uvm_copy_page_info *copy_info, struct devmm_mem_copy_para *copy_para,
                               u64 offset)
{
    int ret = 0;

    ret = devmm_uvm_memcpy_check_source(svm_proc, copy_info);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_uvm_memcpy_check_destination(svm_proc, copy_info);
    if (ret != 0) {
        return ret;
    }

    devmm_find_memcpy_dir(&copy_info->dir, &copy_info->src_attr, &copy_info->dst_attr);

    memcpy_msg->src_addr[0] = copy_para->src + offset;
    memcpy_msg->dst_addr[0] = copy_para->dst + offset;
    memcpy_msg->copy_size[0] = copy_info->size;
    memcpy_msg->page_num = 1;

    ret = devmm_uvm_memcpy_process_frame(svm_proc, memcpy_msg, copy_info);

    return ret;
}

STATIC int devmm_uvm_overlap_memcpy_proc(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para)
{
    struct devmm_chan_uvm_memcpy *memcpy_msg = NULL;
    struct devmm_uvm_copy_page_info copy_info_cur = {0};
    u64 offset = 0, copy_size = 0;
    ka_rw_semaphore_t *src_lock = NULL, *dst_lock = NULL;
    int ret = 0;

    memcpy_msg = (struct devmm_chan_uvm_memcpy *)devmm_kvalloc(sizeof(struct devmm_chan_uvm_memcpy),
                                                               KA_GFP_KERNEL | __KA_GFP_ACCOUNT | __KA_GFP_ZERO);
    if (memcpy_msg == NULL) {
        devmm_drv_err("Kzalloc for struct devmm_chan_uvm_memcpy failed.\n");
        return -ENOMEM;
    }

    while (offset < copy_para->ByteCount) {
        copy_size = devmm_uvm_calc_page_copy(copy_para, offset, &copy_info_cur);
        dst_lock = uvm_get_page_lock_array(svm_proc->uvm_heap, copy_info_cur.dst);
        if (dst_lock == NULL) {
            devmm_drv_err("Uvm get page lock array failed. (va=0x%llx)\n", copy_info_cur.dst);
            ret = -EFAULT;
            goto free_memcpy_msg;
        }

        src_lock = uvm_get_page_lock_array(svm_proc->uvm_heap, copy_info_cur.src);
        if (src_lock == NULL) {
            devmm_drv_err("Uvm get page lock array failed. (va=0x%llx)\n", copy_info_cur.src);
            ret = -EFAULT;
            goto free_memcpy_msg;
        }

        ka_task_down_write(dst_lock);
        if (src_lock != dst_lock) {
            ka_task_down_read(src_lock);
        }

        ret = devmm_uvm_page_copy(svm_proc, memcpy_msg, &copy_info_cur, copy_para, offset);
        if (ret != 0) {
            goto unlock_page;
        }

        offset += copy_size;
        ka_task_up_write(dst_lock);
        if (src_lock != dst_lock) {
            ka_task_up_read(src_lock);
        }
    }

    goto free_memcpy_msg;

unlock_page:
    ka_task_up_write(dst_lock);
    if (src_lock != dst_lock) {
        ka_task_up_read(src_lock);
    }

free_memcpy_msg:
    devmm_kvfree(memcpy_msg);
    return ret;
}

STATIC int devmm_uvm_memcpy_normal_proc_inner(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                                              struct devmm_chan_uvm_memcpy *memcpy_msg)
{
    struct devmm_uvm_copy_page_info copy_info_pre = {0}, copy_info_cur = {0};
    u64 offset = 0, committed_size = 0, copy_size = 0;
    u64 src_lock_end = 0, dst_lock_end = 0;
    int ret = 0;
    while (committed_size < copy_para->ByteCount) {
        copy_size = devmm_uvm_calc_page_copy(copy_para, offset, &copy_info_cur);
        ret = devmm_uvm_memcpy_page_pre_process(svm_proc, copy_para, &copy_info_cur, &src_lock_end, &dst_lock_end);
        if (ret != 0) {
            devmm_uvm_memcpy_post_process(svm_proc, copy_para, committed_size, &src_lock_end, &dst_lock_end);
            return ret;
        }
        if (offset == 0) {
            copy_info_pre = copy_info_cur;
        }
        if (!devmm_uvm_compare_memcpy_info(&copy_info_cur, &copy_info_pre) ||
            (memcpy_msg->page_num >= UVM_MEMCPY_MSG_MAX_PAGE_NUM)) {
            ret = devmm_uvm_memcpy_process_frame(svm_proc, memcpy_msg, &copy_info_pre);
            devmm_uvm_memcpy_post_process(svm_proc, copy_para, committed_size, &src_lock_end, &dst_lock_end);
            if (ret != 0) {
                return ret;
            }
            ret = devmm_uvm_memcpy_page_pre_process(svm_proc, copy_para, &copy_info_cur, &src_lock_end, &dst_lock_end);
            if (ret != 0) {
                devmm_uvm_memcpy_post_process(svm_proc, copy_para, committed_size, &src_lock_end, &dst_lock_end);
                return ret;
            }
            memcpy_msg->page_num = 0;
            copy_info_pre = copy_info_cur;
            committed_size = offset;
        }
        memcpy_msg->src_addr[memcpy_msg->page_num] = copy_para->src + offset;
        memcpy_msg->dst_addr[memcpy_msg->page_num] = copy_para->dst + offset;
        memcpy_msg->copy_size[memcpy_msg->page_num] = copy_size;
        memcpy_msg->page_num++;
        offset += copy_size;
        if (offset >= copy_para->ByteCount) {
            ret = devmm_uvm_memcpy_process_frame(svm_proc, memcpy_msg, &copy_info_cur);
            devmm_uvm_memcpy_post_process(svm_proc, copy_para, committed_size, &src_lock_end, &dst_lock_end);
            if (ret != 0) {
                return ret;
            }
            memcpy_msg->page_num = 0;
            committed_size = offset;
        }
    }
    return 0;
}

STATIC int devmm_uvm_memcpy_normal_proc(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para)
{
    struct devmm_chan_uvm_memcpy *memcpy_msg = NULL;
    int ret = 0;

    memcpy_msg = (struct devmm_chan_uvm_memcpy *)devmm_kvalloc(sizeof(struct devmm_chan_uvm_memcpy),
                                                               KA_GFP_KERNEL | __KA_GFP_ACCOUNT | __KA_GFP_ZERO);
    if (memcpy_msg == NULL) {
        devmm_drv_err("Kzalloc for struct devmm_chan_uvm_memcpy failed.\n");
        return -ENOMEM;
    }

    ret = devmm_uvm_memcpy_normal_proc_inner(svm_proc, copy_para, memcpy_msg);

    devmm_kvfree(memcpy_msg);
    return ret;
}

STATIC int devmm_uvm_memcpy_internal_proc(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para)
{
    u64 src_start = ka_base_round_down(copy_para->src, DEVMM_UVM_PAGE_SIZE);
    u64 dst_start = ka_base_round_down(copy_para->dst, DEVMM_UVM_PAGE_SIZE);
    u64 src_end = ka_base_round_up(copy_para->src + copy_para->ByteCount, DEVMM_UVM_PAGE_SIZE);
    u64 dst_end = ka_base_round_up(copy_para->dst + copy_para->ByteCount, DEVMM_UVM_PAGE_SIZE);
    if ((dst_start >= src_end) || (dst_end <= src_start)) {
        return devmm_uvm_memcpy_normal_proc(svm_proc, copy_para);
    } else {
        /* If the src and dst memory regions overlap, the correctness of the data copy cannot be guaranteed. */
        return devmm_uvm_overlap_memcpy_proc(svm_proc, copy_para);
    }

    return 0;
}

STATIC int devmm_uvm_check_non_uvm_addr(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                                        struct devmm_memory_attributes *src_attr,
                                        struct devmm_memory_attributes *dst_attr)
{
    int ret;

    ret = devmm_memcpy_para_check(svm_proc, src_attr, dst_attr, copy_para->ByteCount);
    if (ret != 0) {
#ifndef EMU_ST
        devmm_drv_err("Memcpy para check failed. (src_device_id=%d; dst_device_id=%d)\n", src_attr->devid,
                      dst_attr->devid);
        return -EINVAL;
#endif
    }

    if (dst_attr->is_svm_non_page) {
        ret = devmm_insert_host_page_range(svm_proc, copy_para->dst, copy_para->ByteCount, dst_attr);
        if (ret != 0) {
#ifndef EMU_ST
            devmm_drv_err("Insert host range failed. (dst=0x%llx; byte_count=%llu)\n", copy_para->dst,
                          copy_para->ByteCount);
            return ret;
#endif
        }

        dst_attr->is_svm_host = true;
        dst_attr->is_svm_non_page = false;
    }

    /* lhl TODOp2p copy, should trans dst addr to host bar, not support va copy */
    // dst_attr->copy_use_va = (copy_para->direction == DEVMM_COPY_DEVICE_TO_DEVICE) ? false : dst_attr->copy_use_va;
    return ret;
}

STATIC int devmm_uvm_copy_prepare(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                                  u32 logical_devid, struct devmm_uvm_copy_ctx *ctx, bool src_is_uvm)
{
    struct devmm_memcpy_addr_info addr_info;
    bool local_is_dev;
    int ret;
    local_is_dev = (copy_para->is_support_dev_local_addr && (copy_para->direction == DEVMM_COPY_HOST_TO_DEVICE));

    if (src_is_uvm) {
        ctx->uvm_va = copy_para->src;
        ctx->other_va = copy_para->dst;
        devmm_memcpy_addr_info_pack(copy_para->dst, copy_para->ByteCount, logical_devid, local_is_dev, &addr_info);
        ret = _devmm_get_memcpy_mem_attrs(svm_proc, &addr_info, &ctx->dst_attr);
        if (ret != 0) {
            devmm_drv_err("Get dst attrs failed. (addr=0x%llx)\n", copy_para->dst);
            return ret;
        }
    } else {
        ctx->uvm_va = copy_para->dst;
        ctx->other_va = copy_para->src;
        devmm_memcpy_addr_info_pack(copy_para->src, copy_para->ByteCount, logical_devid, local_is_dev, &addr_info);
        ret = _devmm_get_memcpy_mem_attrs(svm_proc, &addr_info, &ctx->src_attr);
        if (ret != 0) {
            devmm_drv_err("Get src attrs failed. (addr=0x%llx)\n", copy_para->src);
            return ret;
        }
    }

    ret = devmm_uvm_check_non_uvm_addr(svm_proc, copy_para, &ctx->src_attr, &ctx->dst_attr);
    if (ret != 0) {
        devmm_drv_err("Check non uvm mem failed. (src=0x%llx; dst=0x%llx, src_is_uvm=%d)\n", copy_para->src,
                      copy_para->dst, src_is_uvm);
        return ret;
    }

    ctx->uvm_page_lock = uvm_get_page_lock_array(svm_proc->uvm_heap, ctx->uvm_va);
    if (ctx->uvm_page_lock == NULL) {
        devmm_drv_err("Uvm get page lock array failed. (uvm va=0x%llx)\n", ctx->uvm_va);
        return -EFAULT;
    }

    return 0;
}

static int devmm_uvm_copy_loop_iter(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                                    struct devmm_uvm_copy_ctx *ctx, struct devmm_uvm_copy_loop_param *loop_param)
{
    int ret = 0;
    u32 lock_idx = loop_param->lock_idx;
    u64 offset = loop_param->offset;
    u64 copy_size = loop_param->copy_size;
    bool src_is_uvm = loop_param->src_is_uvm;
    struct devmm_uvm_copy_page_info *copy_info = loop_param->copy_info;

    if (src_is_uvm) {
        copy_info->src = ctx->uvm_va + offset;
        copy_info->dst = ctx->other_va + offset;
        ka_task_down_read(ctx->uvm_page_lock + lock_idx);
        ret = devmm_uvm_memcpy_check_source(svm_proc, copy_info);
    } else {
        copy_info->src = ctx->other_va + offset;
        copy_info->dst = ctx->uvm_va + offset;
        ka_task_down_write(ctx->uvm_page_lock + lock_idx);
        ret = devmm_uvm_memcpy_check_destination(svm_proc, copy_info);
    }

    if (ret != 0) {
        goto unlock_exit;
    }
    devmm_find_memcpy_dir(&copy_info->dir, &copy_info->src_attr, &copy_info->dst_attr);

    ctx->task_para->direction = (u32)copy_info->dir;
    ctx->task_para->src = copy_info->src;
    ctx->task_para->dst = copy_info->dst;
    ctx->task_para->count = copy_size;
    ctx->task_para->last_seq_flag = (offset + copy_size >= copy_para->ByteCount);

    ret = devmm_uvm_memcpy_mixed_process(svm_proc, ctx->task_para, src_is_uvm, &copy_info->src_attr,
                                         &copy_info->dst_attr);
    if (ret != 0) {
        devmm_drv_err_if((ret != -EOPNOTSUPP),
                         "Memcpy error. (ret=%d; src=0x%llx; dst=0x%llx; count=%lu; direction=%u)\n", ret,
                         ctx->task_para->src, ctx->task_para->dst, ctx->task_para->count, ctx->task_para->direction);
        if (ret != -EOPNOTSUPP) {
            devmm_print_pre_alloced_va(svm_proc, ctx->task_para->src);
            devmm_print_pre_alloced_va(svm_proc, ctx->task_para->dst);
        }
    }

unlock_exit:
    if (src_is_uvm) {
        ka_task_up_read(ctx->uvm_page_lock + lock_idx);
    } else {
        ka_task_up_write(ctx->uvm_page_lock + lock_idx);
    }

    return ret;
}

STATIC int devmm_uvm_copy_loop(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                               struct devmm_uvm_copy_ctx *ctx, bool src_is_uvm)
{
    u64 offset = 0;
    u64 copy_size = 0;
    int ret = 0;
    u32 i = 0;
    struct devmm_uvm_copy_page_info copy_info = {0};
    struct devmm_uvm_copy_loop_param loop_param = {.src_is_uvm = src_is_uvm, .copy_info = &copy_info};

    ctx->task_para->last_seq_flag = false;
    copy_info.src_attr = ctx->src_attr;
    copy_info.dst_attr = ctx->dst_attr;

    while (offset < copy_para->ByteCount) {
        u64 page_remain = ka_base_round_up(ctx->uvm_va + offset, DEVMM_UVM_PAGE_SIZE) - ctx->uvm_va - offset;
        if (page_remain == 0) {
            page_remain = DEVMM_UVM_PAGE_SIZE;
        }
        copy_size = ka_base_min(page_remain, copy_para->ByteCount - offset);

        loop_param.offset = offset;
        loop_param.lock_idx = i;
        loop_param.copy_size = copy_size;

        ret = devmm_uvm_copy_loop_iter(svm_proc, copy_para, ctx, &loop_param);
        if (ret != 0) {
            return ret;
        }

        i++;
        offset += copy_size;
    }

    return 0;
}

STATIC int devmm_uvm_memcpy_mixed_proc(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                                       bool src_is_uvm, u32 logical_devid, struct devmm_mem_copy_convrt_para *task_para)
{
    int ret = 0;
    struct devmm_uvm_copy_ctx ctx = {.task_para = task_para};

    ret = devmm_uvm_copy_prepare(svm_proc, copy_para, logical_devid, &ctx, src_is_uvm);
    if (ret != 0) {
        return ret;
    }

    return devmm_uvm_copy_loop(svm_proc, copy_para, &ctx, src_is_uvm);
}
#endif
void devmm_init_task_para(struct devmm_mem_copy_convrt_para *task_para, bool last_seq_flag, bool create_msg, bool is_2d,
                          u32 task_mode)
{
    task_para->task_id = (u32)ka_base_atomic_inc_return(&devmm_task_id);
    task_para->last_seq_flag = last_seq_flag;
    task_para->seq = 0;
    task_para->task_query_id = DEVMM_SVM_INVALID_INDEX;
    task_para->copy_task = NULL;
    task_para->create_msg = create_msg;
    task_para->task_mode = task_mode;
    task_para->is_2d = is_2d;
    task_para->need_write = false;
    task_para->is_memcpy_batch = false;
}

int devmm_ioctl_memcpy_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_copy_para *copy_para = &arg->data.copy_para;
    struct devmm_mem_copy_convrt_para task_para = {0};
    u32 logical_devid = arg->head.devid; // not convert id

    devmm_drv_debug("Enter devmm_ioctl_memcpy. (dst=0x%llx; src=0x%llx; ByteCount=0x%lx; dir=%d; logical_devid=%u)\n",
                    arg->data.copy_para.dst, arg->data.copy_para.src, arg->data.copy_para.ByteCount,
                    arg->data.copy_para.direction, logical_devid);

    if (copy_para->is_support_dev_local_addr) {
        if (copy_para->direction != DEVMM_COPY_DEVICE_TO_HOST) {
#ifndef EMU_ST
            devmm_drv_run_info("Dev local addr copy only support d2h. (dir=%u)\n", copy_para->direction);
#endif
            return -EOPNOTSUPP;
        }
    }

    devmm_init_task_para(&task_para, false, false, false, DEVMM_CPY_SYNC_MODE);
    return devmm_memcpy_proc(svm_proc, copy_para, logical_devid, &task_para);
}
#ifndef UVM_OPEN
int devmm_ioctl_uvm_memcpy_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_copy_para *copy_para = &arg->data.copy_para;
    struct devmm_mem_copy_convrt_para task_para = {0};
    u32 logical_devid = arg->head.devid; // not convert id
    bool src_is_uvm, dst_is_uvm;

    devmm_drv_debug("Enter devmm_ioctl_uvm_memcpy. (dst=0x%llx; src=0x%llx; ByteCount=0x%lx; dir=%d)\n",
                    arg->data.copy_para.dst, arg->data.copy_para.src, arg->data.copy_para.ByteCount,
                    arg->data.copy_para.direction);

    src_is_uvm = devmm_va_is_in_uvm_range(copy_para->src);
    if (src_is_uvm && !devmm_vaddr_and_size_is_in_uvm_range(copy_para->src, copy_para->ByteCount)) {
        devmm_drv_err("Src addr is not in uvm range, src=0x%llx, size=0x%llx\n", copy_para->src, copy_para->ByteCount);
        return -EFAULT;
    }

    dst_is_uvm = devmm_va_is_in_uvm_range(copy_para->dst);
    if (dst_is_uvm && !devmm_vaddr_and_size_is_in_uvm_range(copy_para->dst, copy_para->ByteCount)) {
        devmm_drv_err("Dst addr is not in uvm range, dst=0x%llx, size=0x%llx\n", copy_para->dst, copy_para->ByteCount);
        return -EFAULT;
    }

    if (src_is_uvm && dst_is_uvm) {
        return devmm_uvm_memcpy_internal_proc(svm_proc, copy_para);
    } else {
        devmm_init_task_para(&task_para, false, false, false, DEVMM_CPY_SYNC_MODE);
        return devmm_uvm_memcpy_mixed_proc(svm_proc, copy_para, src_is_uvm, logical_devid, &task_para);
    }
}
#endif
static void devmm_copy_para_init(struct devmm_mem_copy_para *single_copy_para, u64 dst, u64 src, size_t size, int index)
{
    single_copy_para->dst = dst;
    single_copy_para->src = src;
    single_copy_para->ByteCount = size;
    if (index == 0) {
        /* memcpy batch, the other direction must keep the same as the first, so only init the dir when index=0. */
        single_copy_para->direction = DEVMM_COPY_INVILED_DIRECTION;
    }
}

static void devmm_copy_task_handle(struct devmm_mem_copy_convrt_para *task_para, u32 addr_arr_len, u32 index)
{
    if (index == 0) {
        task_para->is_memcpy_batch_first_addr = true;
        task_para->is_memcpy_batch = true;
    } else {
        task_para->is_memcpy_batch_first_addr = false;
    }
    if (index == addr_arr_len - 1) {
        task_para->task_mode = DEVMM_CPY_SYNC_MODE;
    }
}

static int devmm_memcpy_batch_addr_info_init(struct devmm_mem_copy_batch_para *batch_para, u64 **dst_arr, u64 **src_arr,
                                             size_t **size_arr)
{
    u64 arg_size;

    arg_size = batch_para->addr_count * sizeof(u64);
    *dst_arr = devmm_kvalloc(arg_size, 0);
    if (*dst_arr == NULL) {
        devmm_drv_err("Kvzalloc for dst arg failed. (size=%llu)\n", arg_size);
        return -ENOMEM;
    }
    if (ka_base_copy_from_user(*dst_arr, (void __ka_user *)(uintptr_t)(batch_para->dst), arg_size) != 0) {
        devmm_drv_err("Copy_from_user dst args fail. (copy_size=%llu; dst=0x%llx)\n", arg_size,
                      (u64)(uintptr_t)batch_para->dst);
        goto free_dst_args;
    }

    *src_arr = devmm_kvalloc(arg_size, 0);
    if (*src_arr == NULL) {
        devmm_drv_err("Kvzalloc for dst arg failed. (size=%llu)\n", arg_size);
        goto free_dst_args;
    }
    if (ka_base_copy_from_user(*src_arr, (void __ka_user *)(uintptr_t)(batch_para->src), arg_size) != 0) {
        devmm_drv_err("Copy_from_user dst args fail. (copy_size=%llu; src=0x%llx)\n", arg_size,
                      (u64)(uintptr_t)batch_para->src);
        goto free_src_args;
    }

    arg_size = batch_para->addr_count * sizeof(size_t);
    *size_arr = devmm_kvalloc(arg_size, 0);
    if (*size_arr == NULL) {
        devmm_drv_err("Kvzalloc for size arg failed. (size=%llu)\n", arg_size);
        goto free_src_args;
    }
    if (ka_base_copy_from_user(*size_arr, (void __ka_user *)(uintptr_t)(batch_para->size), arg_size) != 0) {
        devmm_drv_err("Copy_from_user size args fail. (copy_size=%llu; size_addr=0x%llx)\n", arg_size,
                      (u64)(uintptr_t)batch_para->size);
        goto free_size_args;
    }
    return 0;

free_size_args:
    devmm_kvfree(*size_arr);
    *size_arr = NULL;
free_src_args:
    devmm_kvfree(*src_arr);
    *src_arr = NULL;
free_dst_args:
    devmm_kvfree(*dst_arr);
    *dst_arr = NULL;
    return -EINVAL;
}

static void devmm_memcpy_batch_addr_info_uninit(u64 **dst_arr, u64 **src_arr, size_t **size_arr)
{
    devmm_kvfree(*size_arr);
    *size_arr = NULL;
    devmm_kvfree(*src_arr);
    *src_arr = NULL;
    devmm_kvfree(*dst_arr);
    *dst_arr = NULL;
}

static int devmm_memcpy_batch_para_check(struct devmm_mem_copy_batch_para *copy_batch_para)
{
    if ((copy_batch_para->dst == NULL) || (copy_batch_para->src == NULL) || (copy_batch_para->size == NULL) ||
        (copy_batch_para->addr_count == 0)) {
        devmm_drv_err("Memcpy batch para check failed. (dst_is_null=%d; src_is_null=%d; size_is_null=%d; count=%llu)\n",
                      (copy_batch_para->dst == NULL), (copy_batch_para->src == NULL), (copy_batch_para->size == NULL),
                      (u64)copy_batch_para->addr_count);
        return -EINVAL;
    }
    if (copy_batch_para->addr_count > DEVMM_MEMCPY_BATCH_MAX_COUNT) {
        return -EOPNOTSUPP;
    }
    return 0;
}

static int devmm_memcpy_addr_info_get(struct devmm_svm_process *svm_proc, struct devmm_ioctl_addr_info *info, u64 dst,
                                      u64 src, size_t size)
{
    u32 cmd_flag = DEVMM_HAS_MUTIL_ADDR | DEVMM_ADD_SUB_REF;
    int ret;

    (void)memset_s(info, sizeof(struct devmm_ioctl_addr_info), 0, sizeof(struct devmm_ioctl_addr_info));
    info->num = 2; /* 2 addr */
    info->va[0] = dst;
    info->size[0] = size;
    info->va[1] = src;
    info->size[1] = size;
    info->cmd_id = DEVMM_SVM_MEMCPY_BATCH;

    ret = devmm_set_page_ref_before_ioctl(svm_proc, cmd_flag, info);
    if (ret != 0) {
        devmm_drv_err("Memcpy addr info get failed. (dst=0x%llx; src=0x%llx; size=%llu; ret=%d)\n", dst, src, (u64)size,
                      ret);
        return ret;
    }
    return 0;
}

static void devmm_memcpy_addr_info_put(struct devmm_svm_process *svm_proc, struct devmm_ioctl_addr_info *info, int ret)
{
    devmm_clear_page_ref_after_ioctl(svm_proc, DEVMM_HAS_MUTIL_ADDR | DEVMM_ADD_SUB_REF, ret, info);
}

static void devmm_memcpy_task_wait_and_del(struct devmm_svm_process *svm_proc,
                                           struct devmm_mem_copy_convrt_para *task_para)
{
    bool finished = false;

    finished = devmm_wait_task_finish(task_para->dev_id, &task_para->copy_task->finish_num, (int)task_para->seq);
    if (!finished) {
        /* Timeout: hang copy_task on svm_proc, do not free. */
        devmm_timeout_task_add(svm_proc, task_para->copy_task);
    } else {
        devmm_task_del(task_para->copy_task);
    }
    task_para->copy_task = NULL;
}

static int devmm_memcpy_batch_proc(struct devmm_svm_process *svm_proc, u64 *dst_arr, u64 *src_arr, size_t *size_arr,
                                   uint32_t addr_count)
{
    struct devmm_mem_copy_para single_copy_para = {0};
    struct devmm_ioctl_addr_info addr_info = {0};
    struct devmm_mem_copy_convrt_para task_para = {0};
    u32 stamp = (u32)ka_jiffies;
    int ret = 0;
    u32 i;

    devmm_init_task_para(&task_para, false, false, false, DEVMM_CPY_ASYNC_MODE);

    for (i = 0; i < addr_count; i++) {
        ret = devmm_memcpy_addr_info_get(svm_proc, &addr_info, dst_arr[i], src_arr[i], size_arr[i]);
        if (ret != 0) {
            devmm_drv_err("Memcpy batch get addr failed. (src=0x%llx; dst=0x%llx; ret=%d)\n", src_arr[i], dst_arr[i],
                          ret);
            if (task_para.copy_task != NULL) {
                devmm_memcpy_task_wait_and_del(svm_proc, &task_para);
            }
            return ret;
        }
        devmm_copy_task_handle(&task_para, addr_count, i);
        devmm_copy_para_init(&single_copy_para, dst_arr[i], src_arr[i], size_arr[i], i);

        ret = devmm_memcpy_proc(svm_proc, &single_copy_para, 0, &task_para);
        if (ret != 0) {
            devmm_drv_no_err_if((ret == -EOPNOTSUPP),
                                "Single memcpy task. (ret=%d; src=0x%llx; dst=0x%llx; index=%u)\n", ret, src_arr[i],
                                dst_arr[i], i);
            if (task_para.copy_task != NULL) {
                devmm_memcpy_task_wait_and_del(svm_proc, &task_para);
            }
        }
        devmm_memcpy_addr_info_put(svm_proc, &addr_info, ret);
        if (ret != 0) {
            break;
        }
        devmm_try_cond_resched(&stamp);
    }

    return ret;
}

int devmm_ioctl_memcpy_batch(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_copy_batch_para *copy_batch_para = &arg->data.copy_batch_para;
    u64 *dst_arr = NULL, *src_arr = NULL;
    size_t *size_arr = NULL;
    int ret;

    ret = devmm_memcpy_batch_para_check(copy_batch_para);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_memcpy_batch_addr_info_init(copy_batch_para, &dst_arr, &src_arr, &size_arr);
    if (ret != 0) {
        devmm_drv_err("Memcpy batch info init failed. (ret=%d; addr_num=%u)\n", ret,
                      arg->data.copy_batch_para.addr_count);
        return ret;
    }

    ret = devmm_memcpy_batch_proc(svm_proc, dst_arr, src_arr, size_arr, copy_batch_para->addr_count);
    if (ret != 0) {
        devmm_drv_no_err_if((ret == -EOPNOTSUPP), "Memcpy batch. (ret=%d; addr_num=%u)\n", ret,
                            arg->data.copy_batch_para.addr_count);
    }

    devmm_memcpy_batch_addr_info_uninit(&dst_arr, &src_arr, &size_arr);
    return ret;
}

int devmm_ioctl_async_memcpy_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    struct devmm_mem_copy_convrt_para task_para = {0};
    struct devmm_mem_copy_para copy_para;
    int ret;

    devmm_init_task_para(&task_para, false, false, false, DEVMM_CPY_ASYNC_API_MODE);
    task_para.task_id = DEVMM_DMA_ASYNC_MODE_CHANNEL; /* async use same dma channel */

    copy_para.ByteCount = arg->data.async_copy_para.byte_count;
    copy_para.dst = arg->data.async_copy_para.dst;
    copy_para.src = arg->data.async_copy_para.src;
    copy_para.direction = DEVMM_COPY_INVILED_DIRECTION;
    copy_para.is_support_dev_local_addr = false;

    ret = devmm_memcpy_proc(svm_proc, &copy_para, arg->head.logical_devid, &task_para);
    if (ret != 0) {
        devmm_drv_no_err_if((ret == -EOPNOTSUPP), "Async_memcpy convert. (dst=0x%llx; src=0x%llx; count=%lu)\n",
                            arg->data.async_copy_para.dst, arg->data.async_copy_para.src,
                            arg->data.async_copy_para.byte_count);
        return ret;
    }
    arg->data.async_copy_para.task_id = task_para.task_query_id;
    devmm_async_cpy_inc_addr_ref(svm_proc, copy_para.src, copy_para.dst, copy_para.ByteCount);

    devmm_drv_debug("Asynchrony memcpy. (dst=0x%llx; src=0x%llx; count=%lu; did=%u; task_id=%d)\n",
                    arg->data.async_copy_para.dst, arg->data.async_copy_para.src, arg->data.async_copy_para.byte_count,
                    task_para.dev_id, arg->data.async_copy_para.task_id);

    return 0;
}

int devmm_ioctl_cpy_result_refresh(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    return devmm_cpy_result_refresh(svm_proc, &arg->data.async_copy_para);
}

int devmm_ioctl_sumbit_convert_dma(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    return devmm_sumbit_convert_dma_proc(svm_proc, &arg->data.convert_copy_para.dmaAddr,
                                         arg->data.convert_copy_para.sync_flag);
}

int devmm_ioctl_wait_convert_dma_result(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    return devmm_wait_convert_dma_result(svm_proc, &arg->data.convert_copy_para.dmaAddr);
}

int devmm_check_memcpy2d_input(enum devmm_copy_direction dir, u64 spitch, u64 dpitch, u64 width, u64 height)
{
    if ((width > dpitch) || (width > spitch)) {
        devmm_drv_err("Dpitch and spitch should both be larger than width. (dpitch=%llu; spitch=%llu; "
                      "width=%llu)\n",
                      dpitch, spitch, width);
        return -EINVAL;
    }
    if ((width == 0) || (height == 0)) {
        devmm_drv_err("Width and height should both be larger than 0. (width=%llu; height=%llu)\n", width, height);
        return -EINVAL;
    }
    if ((dir <= DEVMM_COPY_HOST_TO_HOST) || (dir > DEVMM_COPY_INVILED_DIRECTION)) {
#ifndef EMU_ST
        devmm_drv_run_info("Don't support h2h&invalid copy. (direction=%u)\n", dir);
#endif
        return -EOPNOTSUPP;
    }
    return 0;
}

static int _devmm_vmmas_occupy_inc_before_memcpy_proc(struct devmm_svm_process *svm_proc, u64 va, u64 size,
                                                      struct devmm_svm_heap **heap)
{
    struct devmm_svm_heap *tmp_heap = NULL;
    int ret;

    *heap = NULL;
    if (devmm_va_is_in_svm_range(va) == false) {
        return 0;
    }

    tmp_heap = devmm_svm_get_heap(svm_proc, va);
    if (tmp_heap == NULL) {
        devmm_drv_err("Invalid addr. (va=0x%llx)\n", va);
        return -EINVAL;
    }

    if (tmp_heap->heap_sub_type == SUB_RESERVE_TYPE) {
        ret = devmm_vmmas_occupy_inc(&tmp_heap->vmma_mng, va, size);
        if (ret != 0) {
            devmm_drv_err("Invalid addr. (va=0x%llx; size=%llu)\n", va, size);
            return -EINVAL;
        }
        *heap = tmp_heap;
    }
    return 0;
}

static void _devmm_vmmas_occupy_dec_after_memcpy_proc(struct devmm_svm_heap *heap, u64 va, u64 size)
{
    if (heap != NULL) {
        devmm_vmmas_occupy_dec(&heap->vmma_mng, va, size);
    }
}

static int devmm_vmmas_occupy_inc_before_memcpy_proc(struct devmm_svm_process *svm_proc,
                                                     struct devmm_mem_copy_para *copy_para,
                                                     struct devmm_svm_heap **src_heap, struct devmm_svm_heap **dst_heap)
{
    int ret;

    ret = _devmm_vmmas_occupy_inc_before_memcpy_proc(svm_proc, copy_para->src, copy_para->ByteCount, src_heap);
    if (ret != 0) {
        return ret;
    }
    ret = _devmm_vmmas_occupy_inc_before_memcpy_proc(svm_proc, copy_para->dst, copy_para->ByteCount, dst_heap);
    if (ret != 0) {
        _devmm_vmmas_occupy_dec_after_memcpy_proc(*src_heap, copy_para->src, copy_para->ByteCount);
        return ret;
    }

    return 0;
}

static void devmm_vmmas_occupy_dec_after_memcpy_proc(struct devmm_mem_copy_para *copy_para,
                                                     struct devmm_svm_heap *src_heap, struct devmm_svm_heap *dst_heap)
{
    _devmm_vmmas_occupy_dec_after_memcpy_proc(src_heap, copy_para->src, copy_para->ByteCount);
    _devmm_vmmas_occupy_dec_after_memcpy_proc(dst_heap, copy_para->dst, copy_para->ByteCount);
}

static int devmm_memcpy2d_proc(struct devmm_svm_process *svm_proc, struct devmm_mem_copy_para *copy_para,
                               u32 logical_devid, struct devmm_mem_copy_convrt_para *task_para)
{
    struct devmm_svm_heap *src_heap = NULL;
    struct devmm_svm_heap *dst_heap = NULL;
    int ret;

    ret = devmm_vmmas_occupy_inc_before_memcpy_proc(svm_proc, copy_para, &src_heap, &dst_heap);
    if (ret != 0) {
        return ret;
    }

    ret = devmm_memcpy_proc(svm_proc, copy_para, logical_devid, task_para);
    devmm_vmmas_occupy_dec_after_memcpy_proc(copy_para, src_heap, dst_heap);
    return ret;
}

int devmm_ioctl_memcpy2d_proc(struct devmm_svm_process *svm_proc, struct devmm_ioctl_arg *arg)
{
    enum devmm_copy_direction dir = arg->data.copy2d_para.direction;
    struct devmm_mem_copy_convrt_para task_para = {0};
    struct devmm_mem_copy_para copy_para = {0};
    u64 spitch = arg->data.copy2d_para.spitch;
    u64 dpitch = arg->data.copy2d_para.dpitch;
    u64 width = arg->data.copy2d_para.width;
    u64 height = arg->data.copy2d_para.height;
    u64 src = arg->data.copy2d_para.src;
    u64 dst = arg->data.copy2d_para.dst;
    u32 stamp = (u32)ka_jiffies;
    u64 addr_cnt;
    int ret;

    devmm_drv_debug("Enter devmm_ioctl_memcpy2d. (dst=0x%llx; src=0x%llx; dpitch=%llu; spitch=%llu; "
                    "width=%llu; height=%llu; direction=%u)\n",
                    dst, src, dpitch, spitch, width, height, dir);

    ret = devmm_check_memcpy2d_input(dir, spitch, dpitch, width, height);
    if (ret != 0) {
        return ret;
    }

    if ((height != 1) && (((src + spitch) < src) || ((dst + dpitch) < dst))) {
        devmm_drv_err("Pitch is invalid. (src=0x%llx; dst=0x%llx; spitch=%llu; dpitch=%llu)\n", src, dst, spitch,
                      dpitch);
        return -EINVAL;
    }

    if (dir >= DEVMM_COPY_DEVICE_TO_DEVICE) {
#ifndef EMU_ST
        devmm_drv_run_info("Don't support d2d copy. (direction=%u)\n", dir);
#endif
        return -EOPNOTSUPP;
    }

    devmm_init_task_para(&task_para, false, false, true, DEVMM_CPY_ASYNC_MODE);
    copy_para.ByteCount = width;
    copy_para.direction = dir;
    copy_para.is_support_dev_local_addr = false;
    for (addr_cnt = 0; addr_cnt < height; addr_cnt++) {
        copy_para.src = src;
        copy_para.dst = dst;
        if (addr_cnt == height - 1) {
            task_para.task_mode = DEVMM_CPY_SYNC_MODE;
        }

        ret = devmm_memcpy2d_proc(svm_proc, &copy_para, arg->head.logical_devid, &task_para);
        if (ret != 0) {
            devmm_drv_no_err_if((ret == -EOPNOTSUPP),
                                "Memcpy2d proc. (dst=0x%llx; src=0x%llx; dpitch=%llu; spitch=%llu; "
                                "width=%llu; height=%llu; direction=%u; current_dst=0x%llx; current_src=0x%llx; "
                                "current_height=%llu)\n",
                                arg->data.copy2d_para.dst, arg->data.copy2d_para.src, dpitch, spitch, width, height,
                                dir, dst, src, addr_cnt);
            if (task_para.copy_task != NULL) {
                bool finished = false;
                finished = devmm_wait_task_finish(task_para.dev_id, &task_para.copy_task->finish_num,
                                                  (int)task_para.seq);
                if (!finished) {
                    devmm_timeout_task_add(svm_proc, task_para.copy_task);
                } else {
                    devmm_task_del(task_para.copy_task);
                }
                task_para.copy_task = NULL;
            }
            break;
        }
        src += spitch;
        dst += dpitch;
        devmm_try_cond_resched(&stamp);
    }
    return ret;
}

bool devmm_check_va_is_async_cpying(struct devmm_svm_process *svm_proc, u64 va)
{
    struct devmm_svm_proc_master *master_data = (struct devmm_svm_proc_master *)svm_proc->priv_data;

    return !devmm_idr_is_empty(&master_data->async_copy_record.task_idr);
}
