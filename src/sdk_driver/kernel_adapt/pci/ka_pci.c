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

#include <linux/pci.h>
#include "ka_pci_pub.h"
#include "ka_define.h"

ka_bus_type_t *ka_pci_get_bus_type(void)
{
    return &pci_bus_type;
}
EXPORT_SYMBOL_GPL(ka_pci_get_bus_type);

unsigned char ka_pci_get_bus_number(ka_pci_dev_t *pdev)
{
    return pdev->bus->number;
}
EXPORT_SYMBOL_GPL(ka_pci_get_bus_number);

unsigned short ka_pci_get_vendor_id(ka_pci_dev_t *pdev)
{
    return pdev->vendor;
}
EXPORT_SYMBOL_GPL(ka_pci_get_vendor_id);

unsigned short ka_pci_get_device_id(ka_pci_dev_t *pdev)
{
    return pdev->device;
}
EXPORT_SYMBOL_GPL(ka_pci_get_device_id);

unsigned short ka_pci_get_subsystem_vendor_id(ka_pci_dev_t *pdev)
{
    return pdev->subsystem_vendor;
}
EXPORT_SYMBOL_GPL(ka_pci_get_subsystem_vendor_id);

unsigned short ka_pci_get_subsystem_device_id(ka_pci_dev_t *pdev)
{
    return pdev->subsystem_device;
}
EXPORT_SYMBOL_GPL(ka_pci_get_subsystem_device_id);

void ka_pci_irq_vector(ka_pci_dev_t *pdev, u32 entry, u32 devid, u32 *irq,
                       int (*irq_vector_func)(u32, u32, unsigned int *))
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
    *irq = pci_irq_vector(pdev, entry);
#else
    (void)irq_vector_func(devid, entry, irq);
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_irq_vector);

void ka_pci_aer_clear_status(ka_pci_dev_t *pdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
    pci_aer_clear_nonfatal_status(pdev);
#else
    pci_cleanup_aer_uncorrect_error_status(pdev);
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_aer_clear_status);

int ka_pci_enable_msi_intr(ka_pci_dev_t *pdev, int min, int max, int flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
    /* request msi interrupt */
    return pci_alloc_irq_vectors(pdev, min, max, flags);
#else
    return pci_enable_msi_range(pdev, min, max);
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_enable_msi_intr);

void ka_pci_disable_msi_intr(ka_pci_dev_t *pdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
    pci_free_irq_vectors(pdev);
#else
    pci_disable_msi(pdev);
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_disable_msi_intr);

void ka_pci_stop_and_remove_bus_device_locked(ka_pci_dev_t *pdev, ka_mutex_t *remove_rescan_mutex)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    pci_stop_and_remove_bus_device_locked(pdev);
#else
    mutex_lock(remove_rescan_mutex);
    pci_stop_and_remove_bus_device(pdev);
    mutex_unlock(remove_rescan_mutex);
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_stop_and_remove_bus_device_locked);

unsigned int ka_pci_rescan_bus_locked(ka_pci_bus_t *bus, ka_mutex_t *remove_rescan_mutex)
{
    unsigned int ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
    pci_lock_rescan_remove();
    ret = pci_rescan_bus(bus);
    pci_unlock_rescan_remove();
#else
    mutex_lock(remove_rescan_mutex);
    ret = pci_rescan_bus(bus);
    mutex_unlock(remove_rescan_mutex);
#endif
    return ret;
}
EXPORT_SYMBOL_GPL(ka_pci_rescan_bus_locked);

unsigned int ka_pci_get_devfn(ka_pci_dev_t *pdev)
{
    return pdev->devfn;
}
EXPORT_SYMBOL_GPL(ka_pci_get_devfn);

ka_pci_bus_t *ka_pci_get_bus(ka_pci_dev_t *pdev)
{
    return pdev->bus;
}
EXPORT_SYMBOL_GPL(ka_pci_get_bus);

#ifdef CONFIG_PCI_DOMAINS_GENERIC
int ka_pci_get_bus_domain_nr(ka_pci_dev_t *pdev)
{
    return pdev->bus->domain_nr;
}
EXPORT_SYMBOL_GPL(ka_pci_get_bus_domain_nr);
#endif

ka_pci_dev_t *ka_pci_get_bus_self(ka_pci_dev_t *pdev)
{
    return pdev->bus->self;
}
EXPORT_SYMBOL_GPL(ka_pci_get_bus_self);

u8 ka_pci_get_revision(ka_pci_dev_t *pdev)
{
    return pdev->revision;
}
EXPORT_SYMBOL_GPL(ka_pci_get_revision);

unsigned int ka_pci_get_is_physfn(ka_pci_dev_t *pdev)
{
    return pdev->is_physfn;
}
EXPORT_SYMBOL_GPL(ka_pci_get_is_physfn);

unsigned int ka_pci_get_is_virtfn(ka_pci_dev_t *pdev)
{
    return pdev->is_virtfn;
}
EXPORT_SYMBOL_GPL(ka_pci_get_is_virtfn);

ka_device_t *ka_pci_get_dev(ka_pci_dev_t *pdev)
{
    return &pdev->dev;
}
EXPORT_SYMBOL_GPL(ka_pci_get_dev);

ka_kobject_t *ka_pci_get_dev_kobj(ka_pci_dev_t *pdev)
{
    return &pdev->dev.kobj;
}
EXPORT_SYMBOL_GPL(ka_pci_get_dev_kobj);

unsigned short ka_pci_get_pdev_device(ka_pci_dev_t *pdev)
{
    return pdev->device;
}
EXPORT_SYMBOL_GPL(ka_pci_get_pdev_device);

#ifdef CONFIG_PCIEAER
u16 ka_pci_get_aer_cap(ka_pci_dev_t *pdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    return pdev->aer_cap;
#else
    return 0;
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_get_aer_cap);
#endif

u16 ka_pci_get_msix_entry(ka_msix_entry_t *msix_entry)
{
    return msix_entry->entry;
}
EXPORT_SYMBOL_GPL(ka_pci_get_msix_entry);

u32 ka_pci_get_msix_vector(ka_msix_entry_t *msix_entry)
{
    return msix_entry->vector;
}
EXPORT_SYMBOL_GPL(ka_pci_get_msix_vector);

void ka_pci_set_msix_entry(ka_msix_entry_t *msix_entry, u16 entry)
{
    msix_entry->entry = entry;
}
EXPORT_SYMBOL_GPL(ka_pci_set_msix_entry);

void ka_pci_set_msix_vector(ka_msix_entry_t *msix_entry, u32 vector)
{
    msix_entry->vector = vector;
}
EXPORT_SYMBOL_GPL(ka_pci_set_msix_vector);

int ka_pci_get_dev_iommu(ka_device_t *dev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
    return -EOPNOTSUPP;
#else
    if (dev->iommu == NULL) {
        return -EINVAL;
    }
    return 0;
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_get_dev_iommu);

int ka_pci_get_dev_iommu_fwspec(ka_device_t *dev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
    return -EOPNOTSUPP;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
    if (dev->iommu_fwspec == NULL) {
        return -EINVAL;
    }
    return 0;
#else
    if ((dev->iommu == NULL) || (dev->iommu->fwspec == NULL)) {
        return -EINVAL;
    }
    return 0;
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_get_dev_iommu_fwspec);

ka_iommu_domain_t *ka_pci_iommu_get_domain_for_dev(ka_device_t *dev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)
    return NULL;
#else
    return iommu_get_domain_for_dev(dev);
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_iommu_get_domain_for_dev);

u32 ka_pci_get_dev_iommu_fwspec_ids0(ka_device_t *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
    return dev->iommu->fwspec->ids[0];
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    return dev->iommu_fwspec->ids[0];
#else
    return 0;
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_get_dev_iommu_fwspec_ids0);

bridge_func ka_pci_get_bridge_reset_func()
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
    return symbol_get(pci_bridge_secondary_bus_reset);
#else
    return (bridge_func)(uintptr_t)__symbol_get("pci_reset_bridge_secondary_bus");
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_get_bridge_reset_func);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
MODULE_IMPORT_NS(CXL);
#define KA_PCI_EXP_AER_FLAGS (PCI_EXP_DEVCTL_CERE | PCI_EXP_DEVCTL_NFERE | PCI_EXP_DEVCTL_FERE | PCI_EXP_DEVCTL_URRE)
#endif

int ka_pci_enable_pcie_error_reporting(ka_pci_dev_t *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    int rc;

    if (pcie_aer_is_native(dev) == 0) {
        return -EIO;
    }
    rc = pcie_capability_set_word(dev, PCI_EXP_DEVCTL, KA_PCI_EXP_AER_FLAGS);
    return pcibios_err_to_errno(rc);
#else
    return pci_enable_pcie_error_reporting(dev);
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_enable_pcie_error_reporting);

int ka_pci_disable_pcie_error_reporting(ka_pci_dev_t *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    int rc;

    if (pcie_aer_is_native(dev) == 0) {
        return -EIO;
    }
    rc = pcie_capability_clear_word(dev, PCI_EXP_DEVCTL, KA_PCI_EXP_AER_FLAGS);
    return pcibios_err_to_errno(rc);
#else
    return pci_disable_pcie_error_reporting(dev);
#endif
}
EXPORT_SYMBOL_GPL(ka_pci_disable_pcie_error_reporting);

int ka_pci_configure_extended_capability(ka_pci_dev_t *dev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
    u32 cap;
    u16 ctl;
    int ret;

    ret = pcie_capability_read_dword(dev, PCI_EXP_DEVCAP, &cap);
    if (ret != 0) {
        return 0;
    }

    if (!(cap & PCI_EXP_DEVCAP_EXT_TAG)) {
        return 0;
    }

    ret = pcie_capability_read_word(dev, PCI_EXP_DEVCTL, &ctl);
    if (ret != 0) {
        return 0;
    }

    if (!(ctl & PCI_EXP_DEVCTL_EXT_TAG)) {
        pcie_capability_set_word(dev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_EXT_TAG);
    }
#endif

    return 0;
}
EXPORT_SYMBOL_GPL(ka_pci_configure_extended_capability);

int ka_pci_capability_read_word(ka_pci_dev_t *dev, int pos, u16 *val)
{
    return pcie_capability_read_word(dev, pos, val);
}
EXPORT_SYMBOL_GPL(ka_pci_capability_read_word);