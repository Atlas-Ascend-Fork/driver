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

#include "ka_task_pub.h"
#include "ka_list_pub.h"
#include "ka_compiler_pub.h"
#include "ka_errno_pub.h"
#include "ka_kvm_pub.h"
#include "ka_pci_pub.h"
#include "dvt.h"
#include "log.h"
#include "mmio.h"
#include "vfio_ops.h"
#include "vdavinci.h"

struct position {
    unsigned int index;
    uint64_t pos;
    size_t count;
};

STATIC int hw_vdavinci_rw_bar0(struct hw_vdavinci *vdavinci, struct position *p, char *buf, bool write)
{
    int ret;
    u64 bar_start;
    unsigned int ka_maybe_unused index = p->index;
    uint64_t pos = p->pos;
    size_t count = p->count;

    bar_start = (*(u64 *)(vdavinci->cfg_space.config + KA_PCI_BASE_ADDRESS_0)) & KA_PCI_BASE_ADDRESS_MEM_MASK;
#if IS_VDAVINCI_IODEV_SUPPORT
    if (ka_unlikely(vdavinci->iodev == NULL)) {
        ret = hw_vdavinci_register_iodev(vdavinci, bar_start, DOORBELL_MAX * DOORBELL_SIZE, index);
        if (ret != 0) {
            vascend_warn(vdavinci_to_dev(vdavinci), "vdavinci iodev is not supported, ret: %d\n", ret);
            vdavinci->iodev = KA_ERR_PTR(ret);
        }
    }
#endif
    if (ka_likely(write)) {
        ret = g_hw_vdavinci_ops.emulate_mmio_write(vdavinci, bar_start + pos, buf, count);
    } else {
        ret = g_hw_vdavinci_ops.emulate_mmio_read(vdavinci, bar_start + pos, buf, count);
    }

    return ret;
}

STATIC int _hw_vdavinci_rw(struct hw_vdavinci *vdavinci, struct position *p, char *buf, bool write)
{
    u64 bar_start;
    int ret;
    unsigned int index = p->index;
    uint64_t pos = p->pos;
    size_t count = p->count;

    if (ka_unlikely(vdavinci == NULL || !hw_dvt_is_vdavinci_active(vdavinci))) {
        return -EINVAL;
    }
    switch (index) {
        case VFIO_PCI_CONFIG_REGION_INDEX:
            if (write) {
                ret = g_hw_vdavinci_ops.emulate_cfg_write(vdavinci, pos, buf, count);
            } else {
                ret = g_hw_vdavinci_ops.emulate_cfg_read(vdavinci, pos, buf, count);
            }
            break;
        case VFIO_PCI_BAR0_REGION_INDEX:
            ret = hw_vdavinci_rw_bar0(vdavinci, p, buf, write);
            break;
        case VFIO_PCI_BAR2_REGION_INDEX:
            bar_start = (*(u64 *)(vdavinci->cfg_space.config + KA_PCI_BASE_ADDRESS_2)) & KA_PCI_BASE_ADDRESS_MEM_MASK;
            if (write) {
                ret = g_hw_vdavinci_ops.emulate_mmio_quirk_write(vdavinci, bar_start + pos, buf, count);
            } else {
                ret = g_hw_vdavinci_ops.emulate_mmio_quirk_read(vdavinci, bar_start + pos, buf, count);
            }
            break;
        default:
            vascend_err(vdavinci_to_dev(vdavinci), "unsupported region: %u, vid: %u\n", index, vdavinci->id);
            return -EINVAL;
    }

    return ret == 0 ? count : ret;
}

ssize_t hw_vdavinci_rw(struct hw_vdavinci *vdavinci, char *buf, size_t count, loff_t *ppos, bool write)
{
    unsigned int index;
    uint64_t pos;
    struct position p;

    if (vdavinci == NULL) {
        return -EINVAL;
    }
    if (*ppos < 0) {
        vascend_err(vdavinci_to_dev(vdavinci), "invalid pos: %lld, or vdavinci is null\n", *ppos);
        return -EINVAL;
    }

    pos = (*(u64 *)ppos) & VFIO_PCI_OFFSET_MASK;
    index = VFIO_PCI_OFFSET_TO_INDEX(*(u64 *)ppos);
    if (index >= VFIO_PCI_NUM_REGIONS) {
        vascend_err(vdavinci_to_dev(vdavinci), "invalid index: %u, vid: %u\n", index, vdavinci->id);
        return -EINVAL;
    }

    p.index = index;
    p.pos = pos;
    p.count = count;

    return _hw_vdavinci_rw(vdavinci, &p, buf, write);
}

#if ((IS_VDAVINCI_KERNEL_VERSION_SUPPORT) || (defined(DRV_UT)))
STATIC int vdavinci_ioeventfd_handler(void *opaque, void *unused)
{
    struct vdavinci_ioeventfd *ioeventfd = opaque;
    struct position p;

    p.index = (unsigned int)ioeventfd->bar;
    p.pos = ioeventfd->pos;
    p.count = ioeventfd->count;

    _hw_vdavinci_rw(ioeventfd->vdavinci, &p, (char *)&ioeventfd->data, true);
    return 0;
}

STATIC struct vdavinci_ioeventfd *ioeventfd_find_exist(struct hw_vdavinci *vdavinci, loff_t pos, int bar, uint64_t data,
                                                       int count)
{
    struct vdavinci_ioeventfd *ioeventfd;

    ka_list_for_each_entry(ioeventfd, &vdavinci->ioeventfds_list, next)
    {
        if (ioeventfd->pos == pos && ioeventfd->bar == bar && ioeventfd->data == data && ioeventfd->count == count) {
            return ioeventfd;
        }
    }

    return NULL;
}

void hw_vdavinci_ioeventfd_deactive(struct hw_vdavinci *vdavinci, struct vdavinci_ioeventfd *ioeventfd)
{
    ka_vfio_virqfd_disable(&ioeventfd->virqfd);
    ka_list_del(&ioeventfd->next);
    ka_mm_kfree(ioeventfd);
    vdavinci->ioeventfds_nr--;
}

long hw_vdavinci_set_ioeventfd(struct hw_vdavinci *vdavinci, loff_t offset, uint64_t data, int count, int fd)
{
    int ret = 0;
    struct vdavinci_ioeventfd *ioeventfd;
    int bar = VFIO_PCI_OFFSET_TO_INDEX(offset);
    /* offset: offset from the device fd
     * pos: offset from the bar base
     */
    loff_t pos = offset & VFIO_PCI_OFFSET_MASK;

    if (bar != VFIO_PCI_BAR0_REGION_INDEX) {
        return -EINVAL;
    }

    if (pos + count > DOORBELL_MAX * DOORBELL_SIZE) {
        return -EINVAL;
    }

    ka_task_mutex_lock(&vdavinci->ioeventfds_lock);
    ioeventfd = ioeventfd_find_exist(vdavinci, pos, bar, data, count);
    if (ioeventfd) {
        if (fd == -1) {
            hw_vdavinci_ioeventfd_deactive(vdavinci, ioeventfd);
        } else {
            ret = -EEXIST;
            vascend_err(vdavinci_to_dev(vdavinci),
                        "ioeventfd exist, bar %d, pos %lld,"
                        " count %d, data %llu fd %d\n",
                        bar, pos, count, data, fd);
        }
        goto out_unlock;
    }

    if (fd < 0) {
        ret = -ENODEV;
        goto out_unlock;
    }

    if (vdavinci->ioeventfds_nr >= DOORBELL_MAX) {
        ret = -ENOSPC;
        goto out_unlock;
    }

    ioeventfd = ka_mm_kzalloc(sizeof(*ioeventfd), KA_GFP_KERNEL);
    if (!ioeventfd) {
        ret = -ENOMEM;
        goto out_unlock;
    }

    ioeventfd->vdavinci = vdavinci;
    ioeventfd->data = data;
    ioeventfd->pos = pos;
    ioeventfd->count = count;

    ret = ka_vfio_virqfd_enable(ioeventfd, vdavinci_ioeventfd_handler, NULL, NULL, &ioeventfd->virqfd, fd);
    if (ret) {
        ka_mm_kfree(ioeventfd);
        goto out_unlock;
    }

    ka_list_add(&ioeventfd->next, &vdavinci->ioeventfds_list);
    vdavinci->ioeventfds_nr++;

    vascend_info(vdavinci_to_dev(vdavinci),
                 "set ioeventfd success, bar %d, pos %lld, "
                 "count %d, fd %d\n",
                 bar, pos, count, fd);

out_unlock:
    ka_task_mutex_unlock(&vdavinci->ioeventfds_lock);
    return ret;
}
#endif

#if IS_VDAVINCI_IODEV_SUPPORT
STATIC unsigned long hw_kvm_mmio_read_buf(const void *buf, unsigned int len)
{
#define CHAR_LEN 1
#define HWORD_LEN 2
#define WORD_LEN 4
#define DWORD_LEN 8
    const size_t destMax = 8;
    unsigned long data = 0;
    union {
        u16 hword;
        u32 word;
        u64 dword;
    } tmp;

    if (ka_unlikely(buf == NULL)) {
        return 0;
    }
    switch (len) {
        case CHAR_LEN:
            data = *(u8 *)buf;
            break;
        case HWORD_LEN:
            (void)memcpy_s(&tmp.hword, destMax, buf, len);
            data = tmp.hword;
            break;
        case WORD_LEN:
            (void)memcpy_s(&tmp.word, destMax, buf, len);
            data = tmp.word;
            break;
        case DWORD_LEN:
            (void)memcpy_s(&tmp.dword, destMax, buf, len);
            data = tmp.dword;
            break;
    }

    return data;
}

STATIC bool inline hw_iodev_in_range(struct hw_vdavinci_kvm_iodev *iodev, gpa_t addr)
{
    return (addr >= iodev->addr) && (addr < iodev->addr + (gpa_t)iodev->len);
}

STATIC int hw_vdavinci_mmio_read(ka_kvm_vcpu_t *vcpu, ka_kvm_io_device_t *dev, gpa_t addr, int len, void *val)
{
    return -EOPNOTSUPP;
}

STATIC int hw_vdavinci_mmio_write(ka_kvm_vcpu_t *vcpu, ka_kvm_io_device_t *dev, gpa_t addr, int len, const void *val)
{
    struct hw_vdavinci_kvm_iodev *iodev = NULL;
    struct position p = {0};
    unsigned long value = hw_kvm_mmio_read_buf(val, len);

    if (ka_unlikely(vcpu == NULL || dev == NULL || val == NULL)) {
        return -EOPNOTSUPP;
    }
    iodev = ka_container_of(dev, struct hw_vdavinci_kvm_iodev, dev);
    if (ka_unlikely(iodev == NULL || iodev->vdavinci == NULL)) {
        return -EOPNOTSUPP;
    }
    if (ka_unlikely(!hw_iodev_in_range(iodev, addr))) {
        return -EOPNOTSUPP;
    }

    p.index = iodev->index;
    p.pos = addr - iodev->addr;
    p.count = (size_t)len;

    (void)_hw_vdavinci_rw(iodev->vdavinci, &p, (char *)&value, true);

    return 0;
}

STATIC void hw_vdavinci_iodev_destructor(ka_kvm_io_device_t *dev)
{
    struct hw_vdavinci_kvm_iodev *iodev = NULL;

    iodev = ka_container_of(dev, struct hw_vdavinci_kvm_iodev, dev);
    ka_mm_kfree(iodev);
}

STATIC const ka_kvm_io_device_ops_t g_hw_vdavinci_iodev_ops = {
    .read = hw_vdavinci_mmio_read, .write = hw_vdavinci_mmio_write, .destructor = hw_vdavinci_iodev_destructor};

/* r1 < r2 => -1, r1 > r2 => 1, else 0 */
STATIC inline int hw_kvm_io_bus_cmp(const ka_kvm_io_range_t *r1, const ka_kvm_io_range_t *r2)
{
    if (r1->addr < r2->addr) {
        return -1;
    }

    if (r1->addr + r1->len > r2->addr + r2->len) {
        return 1;
    }

    return 0;
}

STATIC int hw_kvm_io_bus_register_dev(ka_kvm_t *kvm, ka_kvm_bus_t bus_idx, gpa_t addr, int len, ka_kvm_io_device_t *dev)
{
    int i;
    ka_kvm_io_bus_t *new_bus = NULL, *bus = NULL;
    ka_kvm_io_range_t range = {0};

    bus = ka_kvm_get_bus(kvm, bus_idx);
    if (bus == NULL) {
        return -ENOMEM;
    }

    if (bus->dev_count - bus->ioeventfd_count > KA_NR_IOBUS_DEVS - 1) {
        return -ENOSPC;
    }

    new_bus = ka_mm_kzalloc(ka_struct_size(bus, range, bus->dev_count + 1), KA_GFP_KERNEL_ACCOUNT);
    if (new_bus == NULL) {
        return -ENOMEM;
    }

    range.addr = addr;
    range.len = len;
    range.dev = dev;

    for (i = 0; i < bus->dev_count; i++) {
        if (hw_kvm_io_bus_cmp(&bus->range[i], &range) > 0) {
            break;
        }
    }

    (void)memcpy_s(new_bus, sizeof(*bus) + i * sizeof(ka_kvm_io_range_t), bus,
                   sizeof(*bus) + i * sizeof(ka_kvm_io_range_t));
    new_bus->dev_count++;
    new_bus->range[i] = range;
    (void)memcpy_s(new_bus->range + i + 1, (bus->dev_count - i) * sizeof(ka_kvm_io_range_t), bus->range + i,
                   (bus->dev_count - i) * sizeof(ka_kvm_io_range_t));
    ka_rcu_assign_pointer(kvm->buses[bus_idx], new_bus);
    ka_synchronize_srcu_expedited(&kvm->srcu);
    ka_mm_kfree(bus);

    return 0;
}

int hw_vdavinci_register_iodev(struct hw_vdavinci *vdavinci, gpa_t addr, int len, int index)
{
    int ret = 0;
    struct hw_vdavinci_kvm_iodev *iodev = NULL;

    if (vdavinci == NULL || vdavinci->vdev.kvm == NULL) {
        return -EINVAL;
    }
    iodev = ka_mm_kzalloc(sizeof(struct hw_vdavinci_kvm_iodev), KA_GFP_KERNEL);
    if (iodev == NULL) {
        return -ENOMEM;
    }
    iodev->len = len;
    iodev->addr = addr;
    iodev->index = index;
    iodev->vdavinci = vdavinci;
    vdavinci->iodev = iodev;

    ka_task_mutex_lock(&vdavinci->vdev.kvm->slots_lock);
    ka_kvm_iodevice_init(&iodev->dev, &g_hw_vdavinci_iodev_ops);

    ret = hw_kvm_io_bus_register_dev(vdavinci->vdev.kvm, KVM_MMIO_BUS, iodev->addr, iodev->len, &iodev->dev);

    ka_task_mutex_unlock(&vdavinci->vdev.kvm->slots_lock);

    if (ret != 0) {
        ka_mm_kfree(iodev);
        vdavinci->iodev = NULL;
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci register iodev failed, ret: %d\n", ret);
        return ret;
    }

    vascend_info(vdavinci_to_dev(vdavinci), "vdavinci register iodev success\n");
    return 0;
}

/* Caller must hold slots_lock. */
STATIC int hw_kvm_io_bus_unregister_dev(ka_kvm_t *kvm, ka_kvm_bus_t bus_idx, ka_kvm_io_device_t *dev)
{
    int i;
    ka_kvm_io_bus_t *new_bus, *bus;

    bus = ka_kvm_get_bus(kvm, bus_idx);
    if (bus == NULL) {
        return 0;
    }

    for (i = 0; i < bus->dev_count; i++) {
        if (bus->range[i].dev == dev) {
            break;
        }
    }

    if (i == bus->dev_count) {
        return 0;
    }

    new_bus = ka_mm_kzalloc(ka_struct_size(bus, range, bus->dev_count - 1), KA_GFP_KERNEL_ACCOUNT);
    if (new_bus == NULL) {
        return -ENOMEM;
    }
    (void)memcpy_s(new_bus, ka_struct_size(bus, range, i), bus, ka_struct_size(bus, range, i));
    new_bus->dev_count--;
    (void)memcpy_s(new_bus->range + i, ka_flex_array_size(new_bus, range, new_bus->dev_count - i), bus->range + i + 1,
                   ka_flex_array_size(new_bus, range, new_bus->dev_count - i));

    ka_rcu_assign_pointer(kvm->buses[bus_idx], new_bus);
    ka_synchronize_srcu_expedited(&kvm->srcu);
    ka_mm_kfree(bus);

    return 0;
}

void hw_vdavinci_unregister_iodev(struct hw_vdavinci *vdavinci)
{
    int ret;
    struct hw_vdavinci_kvm_iodev *iodev = NULL;

    if (vdavinci == NULL || vdavinci->vdev.kvm == NULL) {
        return;
    }
    iodev = vdavinci->iodev;
    if (KA_IS_ERR_OR_NULL(iodev)) {
        return;
    }

    ka_task_mutex_lock(&vdavinci->vdev.kvm->slots_lock);
    ret = hw_kvm_io_bus_unregister_dev(vdavinci->vdev.kvm, KVM_MMIO_BUS, &iodev->dev);
    ka_task_mutex_unlock(&vdavinci->vdev.kvm->slots_lock);
    iodev->vdavinci = NULL;
    if (ret != 0) {
        vdavinci->iodev = NULL;
        vascend_err(vdavinci_to_dev(vdavinci), "vdavinci unregister iodev error, ret: %d\n", ret);
        return;
    }
    ka_mm_kfree(iodev);
    vdavinci->iodev = NULL;
    vascend_info(vdavinci_to_dev(vdavinci), "vdavinci unregister iodev success\n");
}
#else
int hw_vdavinci_register_iodev(struct hw_vdavinci *vdavinci, gpa_t addr, int len, int index)
{
    return -ENOTSUPP;
}

void hw_vdavinci_unregister_iodev(struct hw_vdavinci *vdavinci)
{}
#endif /* IS_VDAVINCI_IODEV_SUPPORT */
