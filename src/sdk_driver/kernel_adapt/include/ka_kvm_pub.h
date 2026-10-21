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
#ifndef KA_KVM_PUB_H
#define KA_KVM_PUB_H

#include <linux/version.h>
#include <linux/uuid.h>
#include <linux/iova.h>
#include <linux/eventfd.h>
#include <linux/kvm_host.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
#include <kvm/iodev.h>
typedef struct kvm_io_device_ops ka_kvm_io_device_ops_t;
#else
struct ka_kvm_io_device_ops_t {
    int (*read)(struct kvm_vcpu *vcpu, struct kvm_io_device *this, gpa_t addr, int len, void *val);
    int (*write)(struct kvm_vcpu *vcpu, struct kvm_io_device *this, gpa_t addr, int len, const void *val);
    void (*destructor)(struct kvm_io_device *this);
};
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
#include <linux/overflow.h>
#endif
#include <linux/interval_tree.h>
#include <linux/interval_tree_generic.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
#include <linux/dma-iommu.h>
#endif
#include <linux/sizes.h>

typedef struct kvm_io_bus ka_kvm_io_bus_t;
typedef struct kvm ka_kvm_t;
typedef enum kvm_bus ka_kvm_bus_t;
typedef struct kvm_vcpu ka_kvm_vcpu_t;
typedef struct kvm_io_device ka_kvm_io_device_t;
typedef struct kvm_io_range ka_kvm_io_range_t;
typedef struct kvm_memory_slot ka_kvm_memory_slot_t;
typedef struct kvm_memslots ka_kvm_memslots_t;

#define ka_kvm_iodevice_init(dev, ops) kvm_iodevice_init(dev, ops)
#define ka_struct_size(p, member, count) struct_size(p, member, count)
#define ka_flex_array_size(p, member, count) flex_array_size(p, member, count)
#define ka_rcu_assign_pointer(p, v) rcu_assign_pointer(p, v)
#define ka_synchronize_srcu_expedited(ssp) synchronize_srcu_expedited(ssp)
#define ka_alloc_iova(iovad, size, limit_pfn, size_aligned) alloc_iova(iovad, size, limit_pfn, size_aligned)
#define ka_free_iova(iovad, pfn) free_iova(iovad, pfn)
#define ka_is_error_noslot_pfn(pfn) is_error_noslot_pfn(pfn)
#define ka_gfn_to_pfn(kvm, gfn) gfn_to_pfn(kvm, gfn)
#define ka_gfn_to_hva(kvm, hva) gfn_to_hva(kvm, hva)
#define ka_kvm_is_visible_gfn(kvm, gfn) kvm_is_visible_gfn(kvm, gfn)
#define ka_kvm_release_pfn_clean(pfn) kvm_release_pfn_clean(pfn)
#define ka_kvm_memslots(kvm) kvm_memslots(kvm)
#define ka_kvm_is_error_hva(addr) kvm_is_error_hva(addr)

#define KA_KVM_MEM_READONLY KVM_MEM_READONLY
#define KA_NR_IOBUS_DEVS NR_IOBUS_DEVS
#define KA_SZ_1G SZ_1G

ka_kvm_io_bus_t *ka_kvm_get_bus(ka_kvm_t *kvm, ka_kvm_bus_t idx);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0))
#define davinci_for_each_memslot(slot, slots, iter) kvm_for_each_memslot(slot, iter, slots)
#else
#define davinci_for_each_memslot(slot, slots, iter) \
    for ((iter) = 0; (iter) < (slots)->used_slots && ((slot) = &(slots)->memslots[(iter)]); (iter)++)
#endif

#endif /* KA_KVM_PUB_H */