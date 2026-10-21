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

#include "securec.h"
#include "ka_base_pub.h"
#include "ka_driver_pub.h"
#include "ka_kvm_pub.h"
#include "ka_task_pub.h"

#ifndef VFIO_IOMMU_NOTIFY_DMA_UNMAP
#define VFIO_IOMMU_NOTIFY_DMA_UNMAP BIT(0)
#define VFIO_GROUP_NOTIFY_SET_KVM BIT(0)
enum vfio_notify_type {
    VFIO_IOMMU_NOTIFY = 0,
    VFIO_GROUP_NOTIFY = 1,
};
#endif

#define KA_CALL_MDEV_OPS(dev, func, ...)                                        \
    ({                                                                          \
        const ka_mdev_type_ops *ops = ka_mdev_get_ops(dev);                     \
        (ops != NULL && ops->func != NULL) ? ops->func(dev, ##__VA_ARGS__) : 0; \
    })

#if IS_ENABLED(CONFIG_HISI_ASCEND_MPAM)
#define KA_IS_ASCEND_HOST_KERNEL 0
#else
#define KA_IS_ASCEND_HOST_KERNEL 1
#endif

typedef struct {
    struct list_head list;
    ka_device_t *dev;
    const ka_mdev_type_ops *ops;
} ka_mdev_ops_node;

static LIST_HEAD(ka_mdev_ops_list);
static DEFINE_MUTEX(ka_mdev_ops_lock);
static DEFINE_MUTEX(ka_mdev_register_lock);

int ka_driver_get_acpi_disabled(void)
{
    return acpi_disabled;
}
EXPORT_SYMBOL_GPL(ka_driver_get_acpi_disabled);

#ifndef __cplusplus
ka_class_t *ka_driver_class_create(ka_module_t *owner, const char *name)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    return class_create(name);
#else
    return class_create(owner, name);
#endif
}
EXPORT_SYMBOL_GPL(ka_driver_class_create);

#ifndef EMU_ST
typedef char *(*ka_class_devnode_const)(const struct device *dev, umode_t *mode); // typedef function pointer
int ka_driver_class_set_devnode(ka_class_t *cls, ka_class_devnode devnode)
{
    if (cls == NULL || devnode == NULL) {
        return -EINVAL;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
    cls->devnode = (ka_class_devnode_const)(devnode);
#else
    cls->devnode = devnode;
#endif
    return 0;
}
EXPORT_SYMBOL_GPL(ka_driver_class_set_devnode);
#endif

#endif

int ka_driver_dmi_find_devid(ka_pci_dev_t *pdev, int DMI_DEV_TYPE_DEV_SLOT, int *dev_id)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
#ifdef CONFIG_DMI
    const struct dmi_device *dmi_dev = NULL;
    const struct dmi_device *from = NULL;
    const struct dmi_dev_onboard *dev_data = NULL;
    do {
        from = dmi_dev;
        dmi_dev = dmi_find_device(DMI_DEV_TYPE_DEV_SLOT, NULL, from);
        if (dmi_dev != NULL) {
            dev_data = (struct dmi_dev_onboard *)dmi_dev->device_data;
#ifdef CONFIG_PCI_DOMAINS_GENERIC
            if ((dev_data != NULL) && (dev_data->bus == pdev->bus->number) &&
                (PCI_SLOT(((unsigned int)(dev_data->devfn))) == PCI_SLOT(pdev->devfn)) &&
                (dev_data->segment == pdev->bus->domain_nr)) {
                *dev_id = dev_data->instance;
                break;
            }
#else
            if ((dev_data != NULL) && (dev_data->bus == pdev->bus->number) &&
                (PCI_SLOT(((unsigned int)(dev_data->devfn))) == PCI_SLOT(pdev->devfn))) {
                *dev_id = dev_data->instance;
                break;
            }
#endif
        }
    } while (dmi_dev != NULL);
    return 0;
#else
    return -EOPNOTSUPP;
#endif
#else
    return -EOPNOTSUPP;
#endif
}
EXPORT_SYMBOL_GPL(ka_driver_dmi_find_devid);

bool ka_is_dev_dma_coherent(ka_device_t *dev)
{
#ifndef __ASM_DEVICE_H
    return true;
#else
#if ((LINUX_VERSION_CODE == KERNEL_VERSION(4, 19, 36)) || (LINUX_VERSION_CODE == KERNEL_VERSION(4, 19, 90)))
    return dev->archdata.dma_coherent;
#else
#endif /* LINUX_VERSION_CODE */
    return true;
#endif /* __ASM_DEVICE_H */
}
EXPORT_SYMBOL_GPL(ka_is_dev_dma_coherent);

int ka_iommu_map(ka_iommu_domain_t *domain, unsigned long iova, phys_addr_t paddr, size_t size, int prot, ka_gfp_t gfp)
{
    int ret = -1;

#if IS_ENABLED(CONFIG_IOMMU_API)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0))
    ret = iommu_map(domain, iova, paddr, size, prot, gfp);
#else
    (void)gfp;
    ret = iommu_map(domain, iova, paddr, size, prot);
#endif /* KERNEL_VERSION(6,3,0) */
#endif /* CONFIG_IOMMU_API */

    return ret;
}
EXPORT_SYMBOL_GPL(ka_iommu_map);

void ka_vfio_unpin_pages(ka_vfio_device_t *vfio_device, ka_pin_info *pin_info)
{
#if IS_ENABLED(CONFIG_VFIO)
#if KA_IS_ASCEND_HOST_KERNEL
#ifndef FUNC_VFIO_UNPIN_PAGES_ARG2_IS_DMA_ADDR_T
    unsigned long *gfns = NULL;
    ka_mdev_device_t *__maybe_unused mdev = NULL;
    int i = 0;
#endif
    int ret = pin_info->npage;

#ifndef FUNC_VFIO_UNPIN_PAGES_ARG2_IS_DMA_ADDR_T
    gfns = kzalloc(sizeof(unsigned long) * pin_info->npage, GFP_KERNEL);
    if (IS_ERR_OR_NULL(gfns)) {
        return;
    }
    for (i = 0; i < pin_info->npage; i++) {
        gfns[i] = pin_info->gfn + i;
    }
#endif

#ifdef FUNC_VFIO_UNPIN_PAGES_ARG2_IS_DMA_ADDR_T
    vfio_unpin_pages(vfio_device, pin_info->gfn << PAGE_SHIFT, pin_info->npage);
#elif defined(FUNC_VFIO_UNPIN_PAGES_ARG1_IS_VFIO_DEVICE_PTR)
    ret = vfio_unpin_pages(vfio_device, gfns, pin_info->npage);
#elif defined(FUNC_VFIO_UNPIN_PAGES_ARG1_IS_DEVICE_PTR) && !defined(FUNC_VFIO_DEVICE_DATA_PRESENT) && \
    defined(TYPE_VFIO_DEVICE_HAS_MEMBER_DEV)
    ret = vfio_unpin_pages(vfio_device->dev, gfns, pin_info->npage);
#else
    mdev = vfio_device_data(vfio_device);
    ret = vfio_unpin_pages(mdev_dev(mdev), gfns, pin_info->npage);
#endif
    WARN_ON(ret != pin_info->npage);
#ifndef FUNC_VFIO_UNPIN_PAGES_ARG2_IS_DMA_ADDR_T
    kfree(gfns);
#endif
#endif /* KA_IS_ASCEND_HOST_KERNEL */
#endif /* CONFIG_VFIO */
}
EXPORT_SYMBOL_GPL(ka_vfio_unpin_pages);

#ifndef FUNC_VFIO_PIN_PAGES_ARG2_IS_DMA_ADDR_T
#if KA_IS_ASCEND_HOST_KERNEL
static int ka_init_gfns_pfns(unsigned long **gfns, unsigned long **pfns, unsigned long gfn, int npage)
{
    int i = 0;

    *gfns = kzalloc(sizeof(unsigned long) * npage, GFP_KERNEL);
    if (IS_ERR_OR_NULL(*gfns)) {
        return -ENOMEM;
    }
    for (i = 0; i < npage; i++) {
        (*gfns)[i] = gfn + i;
    }
    *pfns = kzalloc(sizeof(unsigned long) * npage, GFP_KERNEL);
    if (IS_ERR_OR_NULL(*pfns)) {
        kfree(*gfns);
        return -ENOMEM;
    }

    return 0;
}

static void ka_clean_gfns_pfns(unsigned long **gfns, unsigned long **pfns)
{
    kfree(*gfns);
    kfree(*pfns);
    *gfns = NULL;
    *pfns = NULL;
}
#endif /* KA_IS_ASCEND_HOST_KERNEL */
#endif /* !FUNC_VFIO_PIN_PAGES_ARG2_IS_DMA_ADDR_T */

int ka_vfio_pin_pages(ka_vfio_device_t *vfio_device, ka_pin_info *pin_info)
{
    int ret = -1;
#if IS_ENABLED(CONFIG_VFIO)
#if KA_IS_ASCEND_HOST_KERNEL
#ifndef FUNC_VFIO_PIN_PAGES_ARG2_IS_DMA_ADDR_T
    int i = 0;
    unsigned long *gfns = NULL, *pfns = NULL;
    ka_mdev_device_t *__maybe_unused mdev = NULL;
#endif

#ifndef FUNC_VFIO_PIN_PAGES_ARG2_IS_DMA_ADDR_T
    ret = ka_init_gfns_pfns(&gfns, &pfns, pin_info->gfn, pin_info->npage);
    if (ret != 0) {
        return ret;
    }
#endif
#ifdef FUNC_VFIO_PIN_PAGES_ARG2_IS_DMA_ADDR_T
    ret = vfio_pin_pages(vfio_device, pin_info->gfn << PAGE_SHIFT, pin_info->npage, IOMMU_READ | IOMMU_WRITE,
                         pin_info->pages);
#elif defined(FUNC_VFIO_PIN_PAGES_ARG1_IS_VFIO_DEVICE_PTR)
    ret = vfio_pin_pages(vfio_device, gfns, pin_info->npage, IOMMU_READ | IOMMU_WRITE, pfns);
#elif defined(FUNC_VFIO_PIN_PAGES_ARG1_IS_DEVICE_PTR) && !defined(FUNC_VFIO_DEVICE_DATA_PRESENT) && \
    defined(TYPE_VFIO_DEVICE_HAS_MEMBER_DEV)
    ret = vfio_pin_pages(vfio_device->dev, gfns, pin_info->npage, IOMMU_READ | IOMMU_WRITE, pfns);
#else
    mdev = vfio_device_data(vfio_device);
    ret = vfio_pin_pages(mdev_dev(mdev), gfns, pin_info->npage, IOMMU_READ | IOMMU_WRITE, pfns);
#endif
    if (ret != pin_info->npage) {
        ret = (ret < 0) ? ret : -EINVAL;
    }
#ifndef FUNC_VFIO_PIN_PAGES_ARG2_IS_DMA_ADDR_T
    for (i = 0; ret > 0 && i < pin_info->npage; i++) {
        pin_info->pages[i] = pfn_to_page(pfns[i]);
    }

    ka_clean_gfns_pfns(&gfns, &pfns);
#endif /* !FUNC_VFIO_PIN_PAGES_ARG2_IS_DMA_ADDR_T */
#endif /* KA_IS_ASCEND_HOST_KERNEL */
#endif /* CONFIG_VFIO */
    return ret;
}
EXPORT_SYMBOL_GPL(ka_vfio_pin_pages);

ka_device_t *ka_get_mdev_parent(ka_mdev_device_t *mdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    return mdev->type->parent->dev;
#else
    return mdev_parent_dev(mdev);
#endif
}
EXPORT_SYMBOL_GPL(ka_get_mdev_parent);

bool ka_device_is_mdev(ka_device_t *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0)
    if (dev == NULL) {
        return false;
    }
    return !strcmp(dev->bus->name, "mdev");
#else
    return dev->bus == &mdev_bus_type;
#endif
}
EXPORT_SYMBOL_GPL(ka_device_is_mdev);

ka_mdev_device_t *ka_get_mdev_device(ka_device_t *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    return to_mdev_device(dev);
#else
    return mdev_from_dev(dev);
#endif
}
EXPORT_SYMBOL_GPL(ka_get_mdev_device);

void *ka_get_mdev_drvdata(ka_device_t *dev)
{
    if (dev == NULL) {
        return NULL;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
    return dev_get_drvdata(dev);
#else
    return mdev_get_drvdata(mdev_from_dev(dev));
#endif
}
EXPORT_SYMBOL_GPL(ka_get_mdev_drvdata);

void ka_set_mdev_drvdata(ka_device_t *dev, void *data)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
    dev_set_drvdata(dev, data);
#else
    ka_mdev_device_t *mdev = mdev_from_dev(dev);

    mdev_set_drvdata(mdev, data);
#endif
}
EXPORT_SYMBOL_GPL(ka_set_mdev_drvdata);

void ka_mdev_unregister_driver(ka_mdev_driver_t *drv)
{
#if IS_ENABLED(CONFIG_VFIO_MDEV)
#if KA_IS_ASCEND_HOST_KERNEL
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
    mdev_unregister_driver(drv);
#endif /* KERNEL_VERSION(6,1,0) */
#endif /* KA_IS_ASCEND_HOST_KERNEL */
#endif /* CONFIG_VFIO_MDEV */
}
EXPORT_SYMBOL_GPL(ka_mdev_unregister_driver);

int ka_mdev_register_driver(ka_mdev_driver_t *drv)
{
    int ret = 0;
#if IS_ENABLED(CONFIG_VFIO_MDEV)
#if KA_IS_ASCEND_HOST_KERNEL
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
    ret = mdev_register_driver(drv);
    if (ret != 0) {
        return ret;
    }
#endif /* KERNEL_VERSION(6,1,0) */
#endif /* KA_IS_ASCEND_HOST_KERNEL */
#endif /* CONFIG_VFIO_MDEV */

    return ret;
}
EXPORT_SYMBOL_GPL(ka_mdev_register_driver);

void ka_copy_reserved_iova(ka_iova_domain_t *from, ka_iova_domain_t *to)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0))
#define IOVA_ANCHOR ~0UL
    unsigned long flags;
    struct rb_node *node;
    struct iova *iova;
    struct iova *new_iova;

    spin_lock_irqsave(&from->iova_rbtree_lock, flags);
    for (node = rb_first(&from->rbroot); node; node = rb_next(node)) {
        iova = rb_entry(node, struct iova, node);

        if (iova->pfn_lo == IOVA_ANCHOR) {
            continue;
        }

        new_iova = reserve_iova(to, iova->pfn_lo, iova->pfn_hi);
        if (IS_ERR_OR_NULL(new_iova)) {
            printk(KERN_ERR "Reserve iova range %lx@%lx failed\n", iova->pfn_lo, iova->pfn_hi);
        }
    }
    spin_unlock_irqrestore(&from->iova_rbtree_lock, flags);
#else
    copy_reserved_iova(from, to);
#endif
}
EXPORT_SYMBOL_GPL(ka_copy_reserved_iova);

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)))
int ka_vfio_rw_gpa(ka_vfio_gpa *info, unsigned long gpa, void *buf, unsigned long len, bool write)
{
    int ret;
    int (*vfio_dma_rw_fn)(struct vfio_device *device, dma_addr_t iova, void *data, size_t len, bool write);

    vfio_dma_rw_fn = symbol_get(vfio_dma_rw);
    if (!vfio_dma_rw_fn) {
        return -EINVAL;
    }
    ret = vfio_dma_rw_fn(info->device, gpa, buf, len, write);
    symbol_put(vfio_dma_rw);

    return ret;
}
EXPORT_SYMBOL_GPL(ka_vfio_rw_gpa);
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)))
int ka_vfio_rw_gpa(ka_vfio_gpa *info, unsigned long gpa, void *buf, unsigned long len, bool write)
{
    int ret;
    int (*vfio_dma_rw_fn)(struct vfio_group *group, dma_addr_t user_iova, void *data, size_t len, bool write);

    vfio_dma_rw_fn = symbol_get(vfio_dma_rw);
    if (!vfio_dma_rw_fn) {
        return -EINVAL;
    }
    ret = vfio_dma_rw_fn(info->group, gpa, buf, len, write);
    symbol_put(vfio_dma_rw);
    return ret;
}
EXPORT_SYMBOL_GPL(ka_vfio_rw_gpa);
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)))
int ka_vfio_rw_gpa(ka_vfio_gpa *info, unsigned long gpa, void *buf, unsigned long len, bool write)
{
    int ret = 0;
#if IS_ENABLED(CONFIG_KVM)
    struct kvm *kvm = NULL;
    int idx;
    bool kthread = (ka_task_get_current_mm() == NULL);
    mm_segment_t old_fs = get_fs();

    kvm = info->kvm;
    if (kthread) {
        if (!mmget_not_zero(kvm->mm)) {
            return -EFAULT;
        }
        ka_use_mm(kvm->mm);
    }

    idx = srcu_read_lock(&kvm->srcu);
    set_fs(USER_DS);
    if (write) {
        ret = kvm_write_guest(kvm, gpa, buf, len);
    } else {
        ret = kvm_read_guest(kvm, gpa, buf, len);
    }
    set_fs(old_fs);
    srcu_read_unlock(&kvm->srcu, idx);

    if (kthread) {
        ka_unuse_mm(kvm->mm);
        mmput(kvm->mm);
    }
#endif /* CONFIG_KVM */
    return ret;
}
EXPORT_SYMBOL_GPL(ka_vfio_rw_gpa);
#else
int ka_vfio_rw_gpa(ka_vfio_gpa *info, unsigned long gpa, void *buf, unsigned long len, bool write)
{
    return 0;
}
EXPORT_SYMBOL_GPL(ka_vfio_rw_gpa);
#endif /* KERNEL_VERSION(5,7,0) */

bool ka_refcount_mutex_lock(ka_kref_t *ref, ka_mutex_t *lock)
{
    bool ret = false;

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)))
    ret = refcount_dec_and_mutex_lock(&ref->refcount, lock);
#endif

    return ret;
}
EXPORT_SYMBOL_GPL(ka_refcount_mutex_lock);

bool ka_put_vfio_device(ka_vfio_device_t *vfio_device)
{
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0)))
    if (WARN_ON(vfio_device == NULL)) {
        return false;
    }
    vfio_device_put(vfio_device);

    return true;
#endif
    return false;
}
EXPORT_SYMBOL_GPL(ka_put_vfio_device);

ka_vfio_device_t *ka_get_vfio_device(ka_device_t *dev)
{
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0)))
    return vfio_device_get_from_dev(dev);
#endif

    return NULL;
}
EXPORT_SYMBOL_GPL(ka_get_vfio_device);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
static void guid_to_uuid(ka_device_t *dev, uuid_le *dst, const guid_t *src)
{
    if (UUID_SIZE != sizeof(guid_t) || UUID_SIZE != sizeof(uuid_le) || UUID_SIZE != sizeof(uuid_t)) {
        return;
    }
    uuid_copy((uuid_t *)dst, (uuid_t *)src);
}
#endif

uuid_le ka_get_uuid(ka_mdev_device_t *mdev)
{
    uuid_le uuid = {0};
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
    ka_device_t *pdev = ka_get_mdev_parent(mdev);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
    ka_device_t *pdev = ka_get_mdev_parent(mdev);
    const guid_t *m_uuid = mdev_uuid(mdev);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
    guid_to_uuid(pdev, &uuid, &mdev->uuid);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
    guid_to_uuid(pdev, &uuid, m_uuid);
#else
    uuid = mdev_uuid(mdev);
#endif
#endif /* KERNEL_VERSION(4,15,0) */

    return uuid;
}
EXPORT_SYMBOL_GPL(ka_get_uuid);

void ka_init_iova_domain(ka_iova_domain_t *iovad)
{
    if (iovad == NULL) {
        return;
    }
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
    init_iova_domain(iovad, PAGE_SIZE, 1);
#endif
}
EXPORT_SYMBOL_GPL(ka_init_iova_domain);

int ka_mdev_register_type_ops(ka_device_t *parent_dev, const ka_mdev_type_ops *ops)
{
    ka_mdev_ops_node *node = NULL;

    if (parent_dev == NULL || ops == NULL) {
        return -EINVAL;
    }
    mutex_lock(&ka_mdev_ops_lock);
    list_for_each_entry(node, &ka_mdev_ops_list, list)
    {
        if (node->dev == parent_dev) {
            mutex_unlock(&ka_mdev_ops_lock);
            return -EEXIST;
        }
    }
    node = kzalloc(sizeof(ka_mdev_ops_node), GFP_KERNEL);
    if (node == NULL) {
        mutex_unlock(&ka_mdev_ops_lock);
        return -ENOMEM;
    }
    node->dev = parent_dev;
    node->ops = ops;
    list_add(&node->list, &ka_mdev_ops_list);
    mutex_unlock(&ka_mdev_ops_lock);

    return 0;
}
EXPORT_SYMBOL_GPL(ka_mdev_register_type_ops);

void ka_mdev_unregister_type_ops(ka_device_t *parent_dev)
{
    ka_mdev_ops_node *node = NULL, *tmp = NULL;

    mutex_lock(&ka_mdev_ops_lock);
    list_for_each_entry_safe(node, tmp, &ka_mdev_ops_list, list)
    {
        if (node->dev == parent_dev) {
            list_del(&node->list);
            kfree(node);
            break;
        }
    }
    mutex_unlock(&ka_mdev_ops_lock);
}
EXPORT_SYMBOL_GPL(ka_mdev_unregister_type_ops);

static const ka_mdev_type_ops *ka_mdev_get_ops(ka_device_t *parent_dev)
{
    ka_mdev_ops_node *node = NULL;
    const ka_mdev_type_ops *ops = NULL;

    if (unlikely(parent_dev == NULL)) {
        return NULL;
    }
    mutex_lock(&ka_mdev_ops_lock);
    list_for_each_entry(node, &ka_mdev_ops_list, list)
    {
        if (node->dev == parent_dev) {
            ops = node->ops;
            break;
        }
    }
    mutex_unlock(&ka_mdev_ops_lock);

    return ops;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
unsigned int ka_available_instances_show(struct mdev_type *mtype)
{
    if (mtype == NULL || mtype->parent == NULL) {
        return 0;
    }

    return KA_CALL_MDEV_OPS(mtype->parent->dev, get_available, kobject_name(&mtype->kobj), NULL);
}
EXPORT_SYMBOL_GPL(ka_available_instances_show);

ssize_t ka_description_show(struct mdev_type *mtype, char *buf)
{
    if (mtype == NULL || mtype->parent == NULL) {
        return 0;
    }

    return KA_CALL_MDEV_OPS(mtype->parent->dev, show_description, kobject_name(&mtype->kobj), buf);
}
EXPORT_SYMBOL_GPL(ka_description_show);
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0))
static ssize_t device_api_show(struct mdev_type *mtype, struct mdev_type_attribute *attr, char *buf)
{
    if (mtype == NULL || buf == NULL) {
        return 0;
    }
#if IS_ENABLED(CONFIG_VFIO_MDEV)
    return KA_CALL_MDEV_OPS(mtype_get_parent_dev(mtype), device_api_ops, buf);
#else
    return 0;
#endif
}

static ssize_t available_instances_show(struct mdev_type *mtype, struct mdev_type_attribute *attr, char *buf)
{
    struct kobject *kobj = (struct kobject *)mtype;

    if (buf == NULL || mtype == NULL || kobj->name == NULL) {
        return 0;
    }
#if IS_ENABLED(CONFIG_VFIO_MDEV)
    return KA_CALL_MDEV_OPS(mtype_get_parent_dev(mtype), get_available, kobject_name(kobj), buf);
#else
    return 0;
#endif
}

static ssize_t description_show(struct mdev_type *mtype, struct mdev_type_attribute *attr, char *buf)
{
    struct kobject *kobj = (struct kobject *)mtype;

    if (buf == NULL || mtype == NULL || kobj->name == NULL) {
        return 0;
    }
#if IS_ENABLED(CONFIG_VFIO_MDEV)
    return KA_CALL_MDEV_OPS(mtype_get_parent_dev(mtype), show_description, kobject_name(kobj), buf);
#else
    return 0;
#endif
}

static ssize_t vfg_id_store(struct mdev_type *mtype, struct mdev_type_attribute *attr, const char *buf, size_t count)
{
    struct kobject *kobj = (struct kobject *)mtype;

    if (buf == NULL || mtype == NULL || kobj->name == NULL) {
        return 0;
    }
#if IS_ENABLED(CONFIG_VFIO_MDEV)
    return KA_CALL_MDEV_OPS(mtype_get_parent_dev(mtype), store_vfg_id, kobject_name(kobj), buf, count);
#else
    return 0;
#endif
}
#else
static ssize_t device_api_show(struct kobject *kobj, ka_device_t *dev, char *buf)
{
    if (dev == NULL || buf == NULL) {
        return 0;
    }

    return KA_CALL_MDEV_OPS(dev, device_api_ops, buf);
}

static ssize_t available_instances_show(struct kobject *kobj, ka_device_t *dev, char *buf)
{
    if (buf == NULL || kobj == NULL || dev == NULL) {
        return 0;
    }

    return KA_CALL_MDEV_OPS(dev, get_available, kobject_name(kobj), buf);
}

static ssize_t description_show(struct kobject *kobj, ka_device_t *dev, char *buf)
{
    if (kobj == NULL || dev == NULL || buf == NULL) {
        return 0;
    }

    return KA_CALL_MDEV_OPS(dev, show_description, kobject_name(kobj), buf);
}

static ssize_t vfg_id_store(struct kobject *kobj, ka_device_t *dev, const char *buf, size_t count)
{
    if (kobj == NULL || dev == NULL || buf == NULL) {
        return 0;
    }

    return KA_CALL_MDEV_OPS(dev, store_vfg_id, kobject_name(kobj), buf, count);
}
#endif /* KERNEL_VERSION(5,13,0) */
static MDEV_TYPE_ATTR_RO(device_api);
static MDEV_TYPE_ATTR_RO(available_instances);
static MDEV_TYPE_ATTR_RO(description);
static MDEV_TYPE_ATTR_WO(vfg_id);

struct attribute *hw_vdavinci_type_attrs[] = {&mdev_type_attr_device_api.attr, &mdev_type_attr_available_instances.attr,
                                              &mdev_type_attr_description.attr, &mdev_type_attr_vfg_id.attr, NULL};

static struct attribute **ka_get_hw_vdev_type_attrs(void)
{
    return hw_vdavinci_type_attrs;
}
#endif /* KERNEL_VERSION(6,1,0) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0))
static int ka_hw_vdev_iommu_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
    ka_vdev *vdev = container_of(nb, ka_vdev, iommu_notifier);
    struct vfio_iommu_type1_dma_unmap *unmap = data;

    if (vdev == NULL || vdev->iommu_notify == NULL) {
        return NOTIFY_OK;
    }
    if (action != VFIO_IOMMU_NOTIFY_DMA_UNMAP) {
        return NOTIFY_OK;
    }
    if (unmap == NULL) {
        return NOTIFY_BAD;
    }

    return vdev->iommu_notify(vdev->data, unmap->iova, unmap->size);
}

static int ka_hw_vdev_group_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
    ka_vdev *vdev = container_of(nb, ka_vdev, group_notifier);

    if (vdev == NULL || vdev->group_notify == NULL) {
        return NOTIFY_OK;
    }
    if (action != VFIO_GROUP_NOTIFY_SET_KVM) {
        return NOTIFY_OK;
    }
    vdev->kvm = data;

    return vdev->group_notify(vdev->data, data);
}

static int ka_hw_get_vfio_group(ka_vdev *vdev)
{
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)))
    int ret = 0;
    struct vfio_group *vfio_group = NULL;

    if (vdev == NULL) {
        return -EINVAL;
    }
    vfio_group = vfio_group_get_external_user_from_dev(mdev_dev(vdev->mdev));
    if (IS_ERR_OR_NULL(vfio_group)) {
        ret = !vfio_group ? -EFAULT : (int)PTR_ERR(vfio_group);
        return ret;
    }
    vdev->vfio_group = vfio_group;
#endif
    return 0;
}

static void ka_hw_put_vfio_group(ka_vdev *vdev)
{
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)))
    if (vdev == NULL || vdev->vfio_group == NULL) {
        return;
    }
    vfio_group_put_external_user(vdev->vfio_group);
    vdev->vfio_group = NULL;
#endif
}
#endif /* KERNEL_VERSION(6,0,0) */

int ka_vdev_register_vfio_group(ka_vdev *vdev)
{
    int ret = 0;
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)))
    unsigned long events = VFIO_IOMMU_NOTIFY_DMA_UNMAP;

    vdev->iommu_notifier.notifier_call = ka_hw_vdev_iommu_notifier;
    vdev->group_notifier.notifier_call = ka_hw_vdev_group_notifier;
    ret = vfio_register_notifier(mdev_dev(vdev->mdev), VFIO_IOMMU_NOTIFY, &events, &vdev->iommu_notifier);
    if (ret != 0) {
        goto out;
    }
    events = VFIO_GROUP_NOTIFY_SET_KVM;
    ret = vfio_register_notifier(mdev_dev(vdev->mdev), VFIO_GROUP_NOTIFY, &events, &vdev->group_notifier);
    if (ret != 0) {
        goto unregister_iommu;
    }

    ret = ka_hw_get_vfio_group(vdev);
    if (ret != 0) {
        goto unregister_group;
    }

    return 0;

unregister_group:
    vfio_unregister_notifier(mdev_dev(vdev->mdev), VFIO_GROUP_NOTIFY, &vdev->group_notifier);
unregister_iommu:
    vfio_unregister_notifier(mdev_dev(vdev->mdev), VFIO_IOMMU_NOTIFY, &vdev->iommu_notifier);
    vdev->iommu_notifier.notifier_call = NULL;
    vdev->group_notifier.notifier_call = NULL;
out:
#endif
    return ret;
}
EXPORT_SYMBOL_GPL(ka_vdev_register_vfio_group);

void ka_vdev_unregister_vfio_group(ka_vdev *vdev)
{
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)))
    int ret = 0;

    ka_hw_put_vfio_group(vdev);
    ret = vfio_unregister_notifier(mdev_dev(vdev->mdev), VFIO_GROUP_NOTIFY, &vdev->group_notifier);
    if (ret != 0) {
        dev_err(mdev_dev(vdev->mdev), "Failed to unregister vfio group notifier, ret: %d\n", ret);
    }

    ret = vfio_unregister_notifier(mdev_dev(vdev->mdev), VFIO_IOMMU_NOTIFY, &vdev->iommu_notifier);
    if (ret != 0) {
        dev_err(mdev_dev(vdev->mdev), "Failed to unregister vfio iommu notifier, ret: %d\n", ret);
    }
    vdev->iommu_notifier.notifier_call = NULL;
    vdev->group_notifier.notifier_call = NULL;
#endif
}
EXPORT_SYMBOL_GPL(ka_vdev_unregister_vfio_group);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
static inline struct ka_hw_vfio_vdev *ka_vfio_dev_to_vfio_vnpu(struct vfio_device *vfio_dev)
{
    return container_of(vfio_dev, struct ka_hw_vfio_vdev, vfio_dev);
}

static inline ka_vdev *ka_vfio_dev_to_vnpu(struct vfio_device *vfio_dev)
{
    struct ka_hw_vfio_vdev *hw_vfio_vdev = NULL;

    if (vfio_dev == NULL) {
        return NULL;
    }
    hw_vfio_vdev = ka_vfio_dev_to_vfio_vnpu(vfio_dev);
    if (hw_vfio_vdev == NULL) {
        return NULL;
    }

    return hw_vfio_vdev->vdev;
}

static int ka_vdev_vfio_init(struct vfio_device *vfio_dev)
{
    ka_mdev_device_t *mdev = ka_get_mdev_device(vfio_dev->dev);
    struct ka_hw_vfio_vdev *vfio_vdev = ka_vfio_dev_to_vfio_vnpu(vfio_dev);
    ka_device_t *dev = ka_get_mdev_parent(mdev);
    const ka_mdev_type_ops *ops = ka_mdev_get_ops(dev);

    if (ops == NULL || ops->hw_vdev_create == NULL) {
        return -EINVAL;
    }
    vfio_vdev->vdev = ops->hw_vdev_create(&mdev->type->kobj, mdev);
    if (vfio_vdev->vdev == NULL) {
        return -EINVAL;
    }
    ka_set_mdev_drvdata(mdev_dev(mdev), vfio_vdev->vdev);

    return 0;
}

static void ka_hw_vdev_vfio_release(struct vfio_device *vfio_dev)
{
    ka_mdev_device_t *mdev = ka_get_mdev_device(vfio_dev->dev);
    ka_vdev *vdev = ka_vfio_dev_to_vnpu(vfio_dev);

    if (vdev == NULL || vdev->hw_vdev_remove == NULL) {
        return;
    }
    (void)vdev->hw_vdev_remove(mdev);
}

static int ka_hw_vdev_vfio_open(struct vfio_device *vfio_dev)
{
    ka_mdev_device_t *mdev = ka_get_mdev_device(vfio_dev->dev);
    ka_vdev *vdev = ka_vfio_dev_to_vnpu(vfio_dev);

    if (vdev == NULL || vfio_dev->kvm == NULL || vdev->hw_vdev_open == NULL) {
        return -EINVAL;
    }
    vdev->kvm = vfio_dev->kvm;

    return vdev->hw_vdev_open(mdev);
}

static void ka_hw_vdev_vfio_close(struct vfio_device *vfio_dev)
{
    ka_mdev_device_t *mdev = ka_get_mdev_device(vfio_dev->dev);
    ka_vdev *vdev = ka_vfio_dev_to_vnpu(vfio_dev);

    if (vdev == NULL || vdev->hw_vdev_release == NULL) {
        return;
    }
    vdev->hw_vdev_release(mdev);
    vdev->kvm = NULL;
    vdev->domain = NULL;
}

static ssize_t ka_hw_vdev_vfio_read(struct vfio_device *vfio_dev, char __user *buf, size_t count, loff_t *ppos)
{
    ka_mdev_device_t *mdev = ka_get_mdev_device(vfio_dev->dev);
    ka_vdev *vdev = ka_vfio_dev_to_vnpu(vfio_dev);

    if (vdev == NULL || vdev->hw_vdev_read == NULL) {
        return 0;
    }

    return vdev->hw_vdev_read(mdev, buf, count, ppos);
}

static ssize_t ka_hw_vdev_vfio_write(struct vfio_device *vfio_dev, const char __user *buf, size_t count, loff_t *ppos)
{
    ka_mdev_device_t *mdev = ka_get_mdev_device(vfio_dev->dev);
    ka_vdev *vdev = ka_vfio_dev_to_vnpu(vfio_dev);

    if (vdev == NULL || vdev->hw_vdev_write == NULL) {
        return 0;
    }

    return vdev->hw_vdev_write(mdev, buf, count, ppos);
}

static int ka_hw_vdev_vfio_mmap(struct vfio_device *vfio_dev, struct vm_area_struct *vma)
{
    ka_mdev_device_t *mdev = ka_get_mdev_device(vfio_dev->dev);
    ka_vdev *vdev = ka_vfio_dev_to_vnpu(vfio_dev);

    if (vdev == NULL || vdev->hw_vdev_mmap == NULL) {
        return -EINVAL;
    }

    return vdev->hw_vdev_mmap(mdev, vma);
}

static long ka_hw_vdev_vfio_ioctl(struct vfio_device *vfio_dev, unsigned int cmd, unsigned long arg)
{
    ka_mdev_device_t *mdev = ka_get_mdev_device(vfio_dev->dev);
    ka_vdev *vdev = ka_vfio_dev_to_vnpu(vfio_dev);

    if (vdev == NULL || vdev->hw_vdev_ioctl == NULL) {
        return -EINVAL;
        ;
    }

    return vdev->hw_vdev_ioctl(mdev, cmd, arg);
}

static void ka_hw_vdev_vfio_dma_unmap(struct vfio_device *vfio_dev, u64 iova, u64 length)
{
    ka_vdev *vdev = ka_vfio_dev_to_vnpu(vfio_dev);

    if (vdev == NULL || vdev->vfio_dma_unmap == NULL) {
        return;
    }
    vdev->vfio_dma_unmap(vdev->data, iova, length);
}

static const struct vfio_device_ops ka_hw_vnpu_vfio_dev_ops = {
    .init = ka_vdev_vfio_init,
    .release = ka_hw_vdev_vfio_release,
    .open_device = ka_hw_vdev_vfio_open,
    .close_device = ka_hw_vdev_vfio_close,
    .read = ka_hw_vdev_vfio_read,
    .write = ka_hw_vdev_vfio_write,
    .mmap = ka_hw_vdev_vfio_mmap,
    .dma_unmap = ka_hw_vdev_vfio_dma_unmap,
    .ioctl = ka_hw_vdev_vfio_ioctl,
#if IS_ENABLED(CONFIG_IOMMUFD)
#if KA_IS_ASCEND_HOST_KERNEL
    .bind_iommufd = vfio_iommufd_emulated_bind,
    .unbind_iommufd = vfio_iommufd_emulated_unbind,
    .attach_ioas = vfio_iommufd_emulated_attach_ioas,
    .detach_ioas = vfio_iommufd_emulated_detach_ioas,
#endif /* KA_IS_ASCEND_HOST_KERNEL */
#endif /* CONFIG_IOMMUFD */
};

static int ka_hw_vdev_vfio_probe(ka_mdev_device_t *mdev)
{
    int ret = 0;
#if IS_ENABLED(CONFIG_VFIO)
#if KA_IS_ASCEND_HOST_KERNEL
    struct ka_hw_vfio_vdev *vfio_vdev = NULL;

    vfio_vdev = vfio_alloc_device(ka_hw_vfio_vdev, vfio_dev, &mdev->dev, &ka_hw_vnpu_vfio_dev_ops);
    if (IS_ERR(vfio_vdev)) {
        return -EINVAL;
    }
    vfio_vdev->vdev->vfio_device = &vfio_vdev->vfio_dev;
    ret = vfio_register_emulated_iommu_dev(&vfio_vdev->vfio_dev);
    if (ret != 0) {
        goto out_put_vdev;
    }

    return 0;

out_put_vdev:
    vfio_put_device(&vfio_vdev->vfio_dev);
#endif /* KA_IS_ASCEND_HOST_KERNEL */
#endif /* CONFIG_VFIO */
    return ret;
}

static void ka_hw_vdev_vfio_remove(ka_mdev_device_t *mdev)
{
#if IS_ENABLED(CONFIG_VFIO)
#if KA_IS_ASCEND_HOST_KERNEL
    ka_vdev *vdev = ka_get_mdev_drvdata(mdev_dev(mdev));
    struct ka_hw_vfio_vdev *vfio_vdev = NULL;

    if (vdev == NULL) {
        return;
    }
    vfio_vdev = container_of(vdev->vfio_device, struct ka_hw_vfio_vdev, vfio_dev);
    if (vfio_vdev == NULL) {
        return;
    }

    vfio_unregister_group_dev(&vfio_vdev->vfio_dev);
    vfio_put_device(&vfio_vdev->vfio_dev);
#endif /* KA_IS_ASCEND_HOST_KERNEL */
#endif /* CONFIG_VFIO */
}
#else
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(5, 12, 0))
static int ka_hw_vdev_create_ops(struct kobject *kobj, ka_mdev_device_t *mdev)
{
    ka_vdev *vdev = NULL;
    const ka_mdev_type_ops *ops = ka_mdev_get_ops(ka_get_mdev_parent(mdev));

    if (ops == NULL || ops->hw_vdev_create == NULL) {
        return -EINVAL;
    }
    vdev = ops->hw_vdev_create(kobj, mdev);
    if (vdev == NULL) {
        return -EINVAL;
    }

    return 0;
}
#else
static int ka_hw_vdev_create_ops(ka_mdev_device_t *mdev)
{
    ka_vdev *vdev = NULL;
    const ka_mdev_type_ops *ops = ka_mdev_get_ops(ka_get_mdev_parent(mdev));
    struct kobject *kobj = NULL;

    if (mdev == NULL || mdev->type == NULL) {
        return -EINVAL;
    }
    kobj = (struct kobject *)mdev->type;
    if (kobj == NULL || kobj->name == NULL) {
        return -EINVAL;
    }
    if (ops == NULL || ops->hw_vdev_create == NULL) {
        return -EINVAL;
    }
    vdev = ops->hw_vdev_create(kobj, mdev);
    if (vdev == NULL) {
        return -EINVAL;
    }

    return 0;
}
#endif /* KERNEL_VERSION(5,12,0) */

static int ka_hw_vdev_remove_ops(ka_mdev_device_t *mdev)
{
    ka_vdev *vdev = ka_get_mdev_drvdata(mdev_dev(mdev));

    if (vdev == NULL || vdev->hw_vdev_remove == NULL) {
        return -EINVAL;
    }

    return vdev->hw_vdev_remove(mdev);
}

static int ka_hw_vdev_open_ops(ka_mdev_device_t *mdev)
{
    ka_vdev *vdev = ka_get_mdev_drvdata(mdev_dev(mdev));

    if (vdev == NULL || vdev->hw_vdev_open == NULL) {
        return -EINVAL;
    }

    return vdev->hw_vdev_open(mdev);
}

static void ka_hw_vdev_release_ops(ka_mdev_device_t *mdev)
{
    ka_vdev *vdev = ka_get_mdev_drvdata(mdev_dev(mdev));

    if (vdev == NULL || vdev->hw_vdev_release == NULL) {
        return;
    }

    vdev->hw_vdev_release(mdev);
}

static ssize_t ka_hw_vdev_read_ops(ka_mdev_device_t *mdev, char __user *buf, size_t count, loff_t *ppos)
{
    ka_vdev *vdev = ka_get_mdev_drvdata(mdev_dev(mdev));

    if (vdev == NULL || vdev->hw_vdev_read == NULL) {
        return 0;
    }

    return vdev->hw_vdev_read(mdev, buf, count, ppos);
}

static ssize_t ka_hw_vdev_write_ops(ka_mdev_device_t *mdev, const char __user *buf, size_t count, loff_t *ppos)
{
    ka_vdev *vdev = ka_get_mdev_drvdata(mdev_dev(mdev));

    if (vdev == NULL || vdev->hw_vdev_write == NULL) {
        return 0;
    }

    return vdev->hw_vdev_write(mdev, buf, count, ppos);
}

static int ka_hw_vdev_mmap_ops(ka_mdev_device_t *mdev, struct vm_area_struct *vma)
{
    ka_vdev *vdev = ka_get_mdev_drvdata(mdev_dev(mdev));

    if (vdev == NULL || vdev->hw_vdev_mmap == NULL) {
        return 0;
    }

    return vdev->hw_vdev_mmap(mdev, vma);
}

static long ka_hw_vdev_ioctl_ops(ka_mdev_device_t *mdev, unsigned int cmd, unsigned long arg)
{
    ka_vdev *vdev = ka_get_mdev_drvdata(mdev_dev(mdev));

    if (vdev == NULL || vdev->hw_vdev_ioctl == NULL) {
        return 0;
    }

    return vdev->hw_vdev_ioctl(mdev, cmd, arg);
}
#endif /* KERNEL_VERSION(6,1,0) */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
ka_mdev_driver_t ka_hw_vdev_mdev_driver = {.device_api = VFIO_DEVICE_API_PCI_STRING,
                                           .driver =
                                               {
                                                   .name = "vnpu_mdev",
                                                   .owner = THIS_MODULE,
                                               },
                                           .probe = ka_hw_vdev_vfio_probe,
                                           .remove = ka_hw_vdev_vfio_remove,
                                           .get_available = ka_available_instances_show,
                                           .show_description = ka_description_show};
EXPORT_SYMBOL_GPL(ka_hw_vdev_mdev_driver);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0))
ka_mdev_driver_t ka_hw_vdev_mdev_driver = {
    .driver =
        {
            .name = "vnpu_mdev",
            .owner = THIS_MODULE,
        },
    .probe = ka_hw_vdev_vfio_probe,
    .remove = ka_hw_vdev_vfio_remove,
};
EXPORT_SYMBOL_GPL(ka_hw_vdev_mdev_driver);
#else
ka_mdev_driver_t ka_hw_vdev_mdev_driver = {};
EXPORT_SYMBOL_GPL(ka_hw_vdev_mdev_driver);
#endif /* KERNEL_VERSION(6,1,0) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
int ka_vdev_init_type_groups(ka_dvt_dev *dvt)
{
    unsigned int i;
    ka_vdev_type *type = NULL;

    dvt->mdev_types = kcalloc(dvt->vdev_type_num * dvt->dev_num, sizeof(struct mdev_type *), GFP_KERNEL);
    if (dvt->mdev_types == NULL) {
        return -ENOMEM;
    }

    for (i = 0; i < dvt->vdev_type_num * dvt->dev_num; i++) {
        type = &dvt->types[i];

        dvt->mdev_types[i] = &type->mtype;
        dvt->mdev_types[i]->sysfs_name = type->name;
    }

    return 0;
}
EXPORT_SYMBOL_GPL(ka_vdev_init_type_groups);

void ka_vdev_cleanup_type_groups(ka_dvt_dev *dvt)
{
    kfree(dvt->mdev_types);
    dvt->mdev_types = NULL;
}
EXPORT_SYMBOL_GPL(ka_vdev_cleanup_type_groups);
#else
int ka_vdev_init_type_groups(ka_dvt_dev *dvt)
{
    unsigned int i, j;
    ka_vdev_type *type = NULL;
    struct attribute_group *group = NULL;

    /* we need put a NULL pointer at the end of supported_type_groups
     * array, vfio-mdev module use the NULL pointer as the arrary end.
     */
    dvt->groups = kcalloc(dvt->vdev_type_num * dvt->dev_num + 1, sizeof(struct attribute_group *), GFP_KERNEL);
    if (dvt->groups == NULL) {
        return -ENOMEM;
    }

    for (i = 0; i < dvt->vdev_type_num * dvt->dev_num; i++) {
        type = &dvt->types[i];

        group = kzalloc(sizeof(struct attribute_group), GFP_KERNEL);
        if (WARN_ON(!group)) {
            goto unwind;
        }

        group->name = type->name;
        group->attrs = ka_get_hw_vdev_type_attrs();
        dvt->groups[i] = group;
    }
    return 0;

unwind:
    for (j = 0; j < i; j++) {
        kfree(dvt->groups[j]);
        dvt->groups[j] = NULL;
    }

    kfree(dvt->groups);
    dvt->groups = NULL;
    return -ENOMEM;
}
EXPORT_SYMBOL_GPL(ka_vdev_init_type_groups);

void ka_vdev_cleanup_type_groups(ka_dvt_dev *dvt)
{
    int i;

    for (i = 0; i < dvt->vdev_type_num * dvt->dev_num; i++) {
        kfree(dvt->groups[i]);
        dvt->groups[i] = NULL;
    }

    kfree(dvt->groups);
    dvt->groups = NULL;
}
EXPORT_SYMBOL_GPL(ka_vdev_cleanup_type_groups);
#endif

int ka_vdev_set_device_ops(ka_dvt_dev *dvt, const struct attribute_group **group)
{
#if IS_ENABLED(CONFIG_VFIO_MDEV)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
    dvt->drv->driver.dev_groups = group;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0))
    dvt->drv->driver.dev_groups = group;
    dvt->drv->supported_type_groups = dvt->groups;
#else
    struct mdev_parent_ops *vdev_mdev_ops = NULL;

    vdev_mdev_ops = kzalloc(sizeof(struct mdev_parent_ops), GFP_KERNEL);
    if (vdev_mdev_ops == NULL) {
        return -ENOMEM;
    }
    vdev_mdev_ops->create = ka_hw_vdev_create_ops;
    vdev_mdev_ops->remove = ka_hw_vdev_remove_ops;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
    vdev_mdev_ops->open = ka_hw_vdev_open_ops;
    vdev_mdev_ops->release = ka_hw_vdev_release_ops;
#else
    vdev_mdev_ops->open_device = ka_hw_vdev_open_ops;
    vdev_mdev_ops->close_device = ka_hw_vdev_release_ops;
#endif /* KERNEL_VERSION(5,15,0) */
    vdev_mdev_ops->read = ka_hw_vdev_read_ops;
    vdev_mdev_ops->write = ka_hw_vdev_write_ops;
    vdev_mdev_ops->mmap = ka_hw_vdev_mmap_ops;
    vdev_mdev_ops->ioctl = ka_hw_vdev_ioctl_ops;
    vdev_mdev_ops->supported_type_groups = dvt->groups;
    vdev_mdev_ops->mdev_attr_groups = group;
    dvt->vdev_mdev_ops = vdev_mdev_ops;
#endif /* KERNEL_VERSION(6,1,0) */
#endif /* CONFIG_VFIO_MDEV */

    return 0;
}
EXPORT_SYMBOL_GPL(ka_vdev_set_device_ops);

void ka_vdev_clean_device_ops(ka_dvt_dev *dvt)
{
#if IS_ENABLED(CONFIG_VFIO_MDEV)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
    if (IS_ERR_OR_NULL(dvt) || IS_ERR_OR_NULL(dvt->vdev_mdev_ops)) {
        return;
    }
    kfree(dvt->vdev_mdev_ops);
    dvt->vdev_mdev_ops = NULL;
#endif /* KERNEL_VERSION(5,19,0) */
#endif /* CONFIG_VFIO_MDEV */
}
EXPORT_SYMBOL_GPL(ka_vdev_clean_device_ops);

int ka_vdev_register_mdev_device(ka_device_t *dev, ka_dvt_dev *dvt, const char *name)
{
    int ret = 0;
#if IS_ENABLED(CONFIG_VFIO_MDEV)
#if KA_IS_ASCEND_HOST_KERNEL
    const char *saved_name;

    mutex_lock(&ka_mdev_register_lock);
    saved_name = dev->driver->name;
    dev->driver->name = name;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
    ret = mdev_register_parent(&dvt->parent, dev, dvt->drv, dvt->mdev_types, dvt->vdev_type_num * dvt->dev_num);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0))
    ret = mdev_register_device(dev, dvt->drv);
#else
    ret = mdev_register_device(dev, dvt->vdev_mdev_ops);
#endif /* KERNEL_VERSION(6,1,0) */
    dev->driver->name = saved_name;
    mutex_unlock(&ka_mdev_register_lock);
#endif /* KA_IS_ASCEND_HOST_KERNEL */
#endif /* CONFIG_VFIO_MDEV */

    return ret;
}
EXPORT_SYMBOL_GPL(ka_vdev_register_mdev_device);

void ka_vdev_unregister_mdev_device(ka_device_t *dev, ka_dvt_dev *dvt)
{
#if IS_ENABLED(CONFIG_VFIO_MDEV)
#if KA_IS_ASCEND_HOST_KERNEL
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
    mdev_unregister_parent(&dvt->parent);
#else
    mdev_unregister_device(dev);
#endif /* KERNEL_VERSION(6,1,0) */
#endif /* KA_IS_ASCEND_HOST_KERNEL */
#endif /* CONFIG_VFIO_MDEV */
}
EXPORT_SYMBOL_GPL(ka_vdev_unregister_mdev_device);

__u64 ka_eventfd_signal(ka_eventfd_ctx_t *ctx, __u64 n)
{
    __u64 ret = 1;

#if IS_ENABLED(CONFIG_EVENTFD)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0))
    eventfd_signal(ctx);
#else
    ret = eventfd_signal(ctx, n);
#endif /* KERNEL_VERSION(6,8,0) */
#endif /* CONFIG_EVENTFD */
    return ret;
}
EXPORT_SYMBOL_GPL(ka_eventfd_signal);

ka_kvm_io_bus_t *ka_kvm_get_bus(ka_kvm_t *kvm, ka_kvm_bus_t idx)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
    return srcu_dereference_check(kvm->buses[idx], &kvm->srcu,
                                  lockdep_is_held(&kvm->slots_lock) || !refcount_read(&kvm->users_count));
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0))
    return kvm_get_bus(kvm, idx);
#else
    return NULL;
#endif
}
EXPORT_SYMBOL_GPL(ka_kvm_get_bus);
