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

#ifndef KA_DRIVER_PUB_H
#define KA_DRIVER_PUB_H

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/acpi.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/dmi.h>
#include <linux/i2c.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#include <linux/gpio/consumer.h>
#endif
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/uuid.h>
#include <linux/notifier.h>
#include <linux/mdev.h>
#include <linux/vfio.h>
#include <linux/workqueue.h>
#include <linux/pci.h>
#include <linux/iova.h>
#include <linux/iommu.h>
#include <linux/irqbypass.h>

#include <conftest.h>
#include "ka_common_pub.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
#define KA_WQ_FLAG (WQ_MEM_RECLAIM | WQ_SYSFS)
#else
#define KA_WQ_FLAG (__WQ_LEGACY | WQ_MEM_RECLAIM | WQ_SYSFS)
#endif

typedef struct clk ka_clk_t;
typedef struct gendisk ka_gendisk_t;
typedef struct acpi_object_list ka_acpi_object_list_t;
typedef struct acpi_buffer ka_acpi_buffer_t;
typedef struct fwnode_handle ka_fwnode_handle_t;

typedef struct miscdevice ka_miscdevice_t;
#define KA_MISC_DYNAMIC_MINOR MISC_DYNAMIC_MINOR
#define ka_driver_init_md_minor(md_minor) .minor = md_minor,
#define ka_driver_init_md_name(md_name) .name = md_name,
#define ka_driver_init_md_fops(md_fops) .fops = md_fops,

#define ka_driver_init_drv_name(drv_name) .name = drv_name,
#define ka_driver_init_drv_id_table(drv_id_table) .id_table = drv_id_table,
#define ka_driver_init_drv_probe(drv_probe) .probe = drv_probe,
#define ka_driver_init_drv_remove(drv_remove) .remove = drv_remove,
#define ka_driver_init_drv_driver(drv_name, drv_pm) \
    .driver = {                                     \
        .name = drv_name,                           \
        .pm = drv_pm,                               \
    },
#define ka_driver_init_drv_err_handler(drv_err_handler) .err_handler = drv_err_handler,
#define ka_driver_init_drv_shutdown(drv_shutdown) .shutdown = drv_shutdown,
#define ka_driver_init_drv_virt_configure(drv_virt_configure) .virt_configure = drv_virt_configure,

typedef struct dmi_system_id ka_dmi_system_id_t;
typedef struct dmi_device ka_dmi_device_t;
typedef struct gpio_desc ka_gpio_desc_t;
typedef struct i2c_adapter ka_i2c_adapter_t;
typedef struct i2c_driver ka_i2c_driver_t;
typedef struct i2c_msg ka_i2c_msg_t;
typedef struct i2c_client ka_i2c_client_t;
typedef union i2c_smbus_data ka_i2c_smbus_data_t;
typedef struct iova ka_iova_t;
typedef struct iova_fq ka_iova_fq_t;
typedef struct property ka_property_t;
typedef struct of_phandle_args ka_of_phandle_args_t;
typedef struct of_device_id ka_of_device_id_t;
typedef struct vc_data ka_vc_data_t;
typedef struct mdev_device ka_mdev_device_t;
typedef struct mdev_parent_ops ka_mdev_parent_ops_t;
typedef struct vfio_info_cap ka_vfio_info_cap_t;
typedef struct vfio_info_cap_header ka_vfio_info_cap_header_t;
typedef struct vfio_irq_set ka_vfio_irq_set_t;
typedef struct device_attribute ka_device_attribute_t;
typedef struct iova_domain ka_iova_domain_t;
typedef enum gpiod_flags ka_gpiod_flags_t;
typedef char *(*ka_class_devnode)(ka_device_t *dev, umode_t *mode);
typedef struct vfio_device ka_vfio_device_t;
typedef struct mdev_driver ka_mdev_driver_t;
typedef struct iommu_domain ka_iommu_domain_t;
typedef struct kref ka_kref_t;
typedef struct mutex ka_mutex_t;
typedef struct eventfd_ctx ka_eventfd_ctx_t;
typedef struct vfio_device_info ka_vfio_device_info_t;
typedef struct vfio_device_ioeventfd ka_vfio_device_ioeventfd_t;
typedef struct vfio_irq_info ka_vfio_irq_info_t;
typedef struct vfio_region_info ka_vfio_region_info_t;
typedef struct vfio_region_info_cap_sparse_mmap ka_vfio_region_info_cap_sparse_mmap_t;
typedef struct iommu_iotlb_gather ka_iommu_iotlb_gather_t;

#define ka_guid_t guid_t

#define KA_IOVA_FQ_SIZE IOVA_FQ_SIZE

#define KA_MINORBITS MINORBITS
#define KA_MINORMASK MINORMASK

#define KA_DRIVER_MAJOR(dev) MAJOR(dev)
#define KA_DRIVER_MINOR(dev) MINOR(dev)
#define KA_DRIVER_MKDEV(ma, mi) MKDEV(ma, mi)

#define KA_DRIVER_ALIGN(x, a) ALIGN(x, a)
#define KA_DRIVER_IS_ALIGNED(x, a) IS_ALIGNED(x, a)

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 4, 0)
#define KA_DRIVER_ALIGN_DOWN(addr, size) ((addr) & (~((size) - 1)))
#else
#define KA_DRIVER_ALIGN_DOWN(addr, size) ALIGN_DOWN(addr, size)
#endif

#define KA_DRIVER_DEVICE_ATTR(_name, _mode, _show, _store) DEVICE_ATTR(_name, _mode, _show, _store)
#define KA_DEVICE_ATTR_RO(_name) DEVICE_ATTR_RO(_name)

#define ka_driver_clk_disable(clk) clk_disable(clk)
#define ka_driver_clk_enable(clk) clk_enable(clk)
#define ka_driver_clk_get_parent(clk) clk_get_parent(clk)
#define ka_driver_clk_get_rate(clk) clk_get_rate(clk)
int ka_driver_get_acpi_disabled(void);
#define ka_driver_acpi_evaluate_object(handle, pathname, external_params, return_buffer) \
    acpi_evaluate_object(handle, pathname, external_params, return_buffer)
#define ka_driver_acpi_evaluate_object_typed(handle, pathname, external_params, return_buffer, return_type) \
    acpi_evaluate_object_typed(handle, pathname, external_params, return_buffer, return_type)
#define ka_driver_acpi_get_devices(HID, user_function, context, return_value) \
    acpi_get_devices(HID, user_function, context, return_value)
#define ka_driver_is_acpi_device_node(fwnode) is_acpi_device_node(fwnode)
#define ka_driver_dev_driver_string(dev) dev_driver_string(dev)
#define ka_driver_misc_deregister(misc) misc_deregister(misc)
#define ka_driver_misc_register(misc) misc_register(misc)

#define ka_driver_get_random_bytes(buf, len) get_random_bytes(buf, len)
#define ka_driver_dmi_check_system(list) dmi_check_system(list)
#define ka_driver_dmi_find_device(type, name, from) dmi_find_device(type, name, from)
#define ka_driver_dmi_match(f, str) dmi_match(f, str)
#define ka_driver_devm_gpiod_get_optional(dev, con_id, flags) devm_gpiod_get_optional(dev, con_id, flags)
#define ka_driver_i2c_del_adapter(adap) i2c_del_adapter(adap)
#define ka_driver_i2c_del_driver(driver) i2c_del_driver(driver)
#define ka_driver_i2c_get_adapter(nr) i2c_get_adapter(nr)
#define ka_driver_i2c_put_adapter(adap) i2c_put_adapter(adap)
#define ka_driver_i2c_register_driver(owner, driver) i2c_register_driver(owner, driver)
#define ka_driver_i2c_transfer(adap, msgs, num) i2c_transfer(adap, msgs, num)
#define ka_driver_i2c_transfer_buffer_flags(client, buf, count, flags) \
    i2c_transfer_buffer_flags(client, buf, count, flags)
#define ka_driver_i2c_verify_client(dev) i2c_verify_client(dev)
#define ka_driver_i2c_smbus_read_word_data(client, command) i2c_smbus_read_word_data(client, command)
#define ka_driver_i2c_smbus_write_word_data(client, command, value) i2c_smbus_write_word_data(client, command, value)
#define ka_driver_i2c_smbus_xfer(adapter, addr, flags, read_write, command, protocol, data) \
    i2c_smbus_xfer(adapter, addr, flags, read_write, command, protocol, data)
#define ka_driver_of_get_address(dev, index, size, flags) of_get_address(dev, index, size, flags)
#define ka_driver_of_device_is_compatible(device, compat) of_device_is_compatible(device, compat)
#define ka_driver_of_find_compatible_node(from, type, compatible) of_find_compatible_node(from, type, compatible)
#define ka_driver_of_find_property(np, name, lenp) of_find_property(np, name, lenp)
#define ka_driver_of_get_next_available_child(node, prev) of_get_next_available_child(node, prev)
#define ka_driver_of_get_next_child(node, prev) of_get_next_child(node, prev)
#define ka_driver_for_each_child_of_node(parent, child) for_each_child_of_node(parent, child)
#define ka_driver_of_parse_phandle(np, phandle_name, index) of_parse_phandle(np, phandle_name, index)
#define ka_driver_of_parse_phandle_with_args(np, list_name, cells_name, index, out_args) \
    of_parse_phandle_with_args(np, list_name, cells_name, index, out_args)
#define ka_driver_of_parse_phandle_with_args_map(np, list_name, stem_name, index, out_args) \
    of_parse_phandle_with_args_map(np, list_name, stem_name, index, out_args)
#define ka_driver_of_parse_phandle_with_fixed_args(np, list_name, cell_count, index, out_args) \
    of_parse_phandle_with_fixed_args(np, list_name, cell_count, index, out_args)
#define ka_driver_of_match_device(matches, dev) of_match_device(matches, dev)
#define ka_driver_of_property_read_u32(np, propname, out_value) of_property_read_u32(np, propname, out_value)

#ifndef __cplusplus
ka_class_t *ka_driver_class_create(ka_module_t *owner, const char *name);

#ifndef EMU_ST
int ka_driver_class_set_devnode(ka_class_t *cls, ka_class_devnode devnode);
#endif

#define ka_driver_class_destroy(cls) class_destroy(cls)
#define ka_driver_device_create device_create
#define ka_driver_device_destroy(class, devt) device_destroy(class, devt)
#endif

#define ka_driver_of_node_put(node) of_node_put(node)
#define ka_driver_dev_get_drvdata(dev) dev_get_drvdata(dev)
#define ka_driver_mdev_dev(mdev) mdev_dev(mdev)
#define ka_driver_mdev_from_dev(dev) mdev_from_dev(dev)
#define ka_driver_mdev_get_drvdata(mdev) mdev_get_drvdata(mdev)
#define ka_driver_mdev_parent_dev(mdev) mdev_parent_dev(mdev)
#define ka_driver_mdev_register_device(dev, ops) mdev_register_device(dev, ops)
#define ka_driver_mdev_set_drvdata(mdev, data) mdev_set_drvdata(mdev, data)
#define ka_driver_mdev_unregister_device(dev) mdev_unregister_device(dev)

#define ka_driver_vfio_info_add_capability(caps, cap, size) vfio_info_add_capability(caps, cap, size)
#define ka_driver_vfio_info_cap_shift(caps, offset) vfio_info_cap_shift(caps, offset)
#define ka_driver_vfio_pin_pages(dev, user_pfn, npage, prot, phys_pfn) \
    vfio_pin_pages(dev, user_pfn, npage, prot, phys_pfn)
#define ka_driver_vfio_register_notifier(dev, type, events, nb) vfio_register_notifier(dev, type, events, nb)
#define ka_driver_vfio_set_irqs_validate_and_prepare(hdr, num_irqs, max_irq_type, data_size) \
    vfio_set_irqs_validate_and_prepare(hdr, num_irqs, max_irq_type, data_size)
#define ka_driver_vfio_unregister_notifier(dev, type, nb) vfio_unregister_notifier(dev, type, nb)
#define ka_driver_dev_to_node(dev) dev_to_node(dev)
#define ka_iommu_unmap(domain, iova, size) iommu_unmap(domain, iova, size)
#define ka_iommu_iotlb_gather_init(gather) iommu_iotlb_gather_init(gather)
#define ka_iommu_iotlb_gather_add_range(gather, iova, size) iommu_iotlb_gather_add_range(gather, iova, size)
#define ka_iommu_iotlb_sync(domain, gather) iommu_iotlb_sync(domain, gather)

#define KA_DVT_MAX_TYPE_NAME 32

typedef struct {
    unsigned long gfn;
    int npage;
    ka_page_t **pages;
} ka_pin_info;

typedef struct {
    struct kvm *kvm;
    struct vfio_group *group;
    struct vfio_device *device;
} ka_vfio_gpa;

typedef struct {
    ka_eventfd_ctx_t *trigger;
    spinlock_t msix_lock;
    unsigned long flags;
    struct work_struct work;
    int irq;
    void *data;
    struct irq_bypass_producer producer;
} ka_vdev_pci_irq_ctx;

typedef struct {
    ka_mdev_device_t *mdev;
    u32 num_regions;
    ka_vdev_pci_irq_ctx *ctx;
    ka_mutex_t cache_lock;
    struct notifier_block iommu_notifier;
    struct notifier_block group_notifier;
    struct work_struct release_work;
    struct vfio_device *vfio_device;
    struct vfio_group *vfio_group;
    atomic_t released;
    struct kvm *kvm;
    void *domain;
    void *data;
    int (*iommu_notify)(void *data, unsigned long iova, unsigned long size);
    int (*group_notify)(void *data, void *group_data);
    int (*hw_vdev_remove)(ka_mdev_device_t *mdev);
    int (*hw_vdev_open)(ka_mdev_device_t *mdev);
    void (*hw_vdev_release)(ka_mdev_device_t *mdev);
    ssize_t (*hw_vdev_read)(ka_mdev_device_t *mdev, char __user *buf, size_t count, loff_t *ppos);
    size_t (*hw_vdev_write)(ka_mdev_device_t *mdev, const char __user *buf, size_t count, loff_t *ppos);
    int (*hw_vdev_mmap)(ka_mdev_device_t *mdev, struct vm_area_struct *vma);
    long (*hw_vdev_ioctl)(ka_mdev_device_t *mdev, unsigned int cmd, unsigned long arg);
    void (*vfio_dma_unmap)(void *data, u64 iova, u64 length);
    struct list_head dev_dma_info_list_head;
} ka_vdev;

typedef struct {
    ssize_t (*get_available)(ka_device_t *dev, const char *name, char *buf);
    ssize_t (*show_description)(ka_device_t *dev, const char *name, char *buf);
    ssize_t (*device_api_ops)(ka_device_t *dev, char *buf);
    ssize_t (*store_vfg_id)(ka_device_t *dev, const char *name, const char *buf, size_t count);
    ka_vdev *(*hw_vdev_create)(struct kobject *kobj, ka_mdev_device_t *mdev);
} ka_mdev_type_ops;

typedef struct {
    char name[KA_DVT_MAX_TYPE_NAME];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    struct mdev_type mtype;
#endif
} ka_vdev_type;

typedef struct {
    unsigned int dev_num;
    unsigned int vdev_type_num;
    ka_vdev_type *types;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    struct mdev_type **mdev_types;
#else
    struct attribute_group **groups;
#endif
    ka_mdev_driver_t *drv;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    struct mdev_parent parent;
#else
    struct mdev_parent_ops *vdev_mdev_ops;
#endif /* KERNEL_VERSION(6,1,0) */
    ka_mdev_type_ops *type_ops;
} ka_dvt_dev;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
struct ka_hw_vfio_vdev {
    struct vfio_device vfio_dev;
    ka_vdev *vdev;
};
#endif

extern ka_mdev_driver_t ka_hw_vdev_mdev_driver;

int ka_iommu_map(ka_iommu_domain_t *domain, unsigned long iova, phys_addr_t paddr, size_t size, int prot, gfp_t gfp);
int ka_driver_dmi_find_devid(ka_pci_dev_t *pdev, int DMI_DEV_TYPE_DEV_SLOT, int *dev_id);
bool ka_is_dev_dma_coherent(ka_device_t *dev);
void ka_vfio_unpin_pages(ka_vfio_device_t *vfio_device, ka_pin_info *pin_info);
int ka_vfio_pin_pages(ka_vfio_device_t *vfio_device, ka_pin_info *pin_info);
ka_device_t *ka_get_mdev_parent(ka_mdev_device_t *mdev);
bool ka_device_is_mdev(ka_device_t *dev);
ka_mdev_device_t *ka_get_mdev_device(ka_device_t *dev);
void ka_set_mdev_drvdata(ka_device_t *dev, void *data);
void *ka_get_mdev_drvdata(ka_device_t *dev);
void ka_mdev_unregister_driver(ka_mdev_driver_t *drv);
int ka_mdev_register_driver(ka_mdev_driver_t *drv);
void ka_copy_reserved_iova(ka_iova_domain_t *from, ka_iova_domain_t *to);
int ka_vfio_rw_gpa(ka_vfio_gpa *info, unsigned long gpa, void *buf, unsigned long len, bool write);
bool ka_refcount_mutex_lock(ka_kref_t *ref, ka_mutex_t *lock);
bool ka_put_vfio_device(ka_vfio_device_t *vfio_device);
ka_vfio_device_t *ka_get_vfio_device(ka_device_t *dev);
uuid_le ka_get_uuid(ka_mdev_device_t *mdev);
void ka_init_iova_domain(ka_iova_domain_t *iovad);
int ka_mdev_register_type_ops(ka_device_t *parent_dev, const ka_mdev_type_ops *ops);
void ka_mdev_unregister_type_ops(ka_device_t *parent_dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
unsigned int ka_available_instances_show(struct mdev_type *mtype);
ssize_t ka_description_show(struct mdev_type *mtype, char *buf);
#endif
int ka_vdev_register_vfio_group(ka_vdev *vdev);
void ka_vdev_unregister_vfio_group(ka_vdev *vdev);
int ka_vdev_init_type_groups(ka_dvt_dev *dvt);
void ka_vdev_cleanup_type_groups(ka_dvt_dev *dvt);
int ka_vdev_set_device_ops(ka_dvt_dev *dvt, const struct attribute_group **group);
void ka_vdev_clean_device_ops(ka_dvt_dev *dvt);
int ka_vdev_register_mdev_device(ka_device_t *dev, ka_dvt_dev *dvt, const char *name);
void ka_vdev_unregister_mdev_device(ka_device_t *dev, ka_dvt_dev *dvt);
__u64 ka_eventfd_signal(ka_eventfd_ctx_t *ctx, __u64 n);

#define ka_driver_gpio_is_valid(num) gpio_is_valid(num)
#define ka_driver_gpio_request(num, name) gpio_request(num, name)
#define ka_driver_gpio_get_value(num) gpio_get_value(num)
#define ka_driver_gpio_free(num) gpio_free(num)
#define ka_device_for_each_child(parent, data, fn) device_for_each_child(parent, data, fn)
#define ka_device_lock(dev) device_lock(dev)
#define ka_device_unlock(dev) device_unlock(dev)
#define ka_iova_shift(iovad) iova_shift(iovad)
#define ka_valid_dma_direction(dir) valid_dma_direction(dir)
#define ka_reserve_iova(iovad, pfn_lo, pfn_hi) reserve_iova(iovad, pfn_lo, pfn_hi)
#define ka_strstarts(str, prefix) strstarts(str, prefix)
#define ka_vfio_virqfd_disable(pvirqfd) vfio_virqfd_disable(pvirqfd)
#define ka_vfio_virqfd_enable(opaque, handler, thread, data, pvirqfd, fd) \
    vfio_virqfd_enable(opaque, handler, thread, data, pvirqfd, fd)

#define KA_VFIO_DEVICE_API_PCI_STRING VFIO_DEVICE_API_PCI_STRING
#define KA_VFIO_DEVICE_FLAGS_PCI VFIO_DEVICE_FLAGS_PCI
#define KA_VFIO_DEVICE_FLAGS_RESET VFIO_DEVICE_FLAGS_RESET
#define KA_VFIO_DEVICE_GET_INFO VFIO_DEVICE_GET_INFO
#define KA_VFIO_DEVICE_GET_IRQ_INFO VFIO_DEVICE_GET_IRQ_INFO
#define KA_VFIO_DEVICE_GET_REGION_INFO VFIO_DEVICE_GET_REGION_INFO
#define KA_VFIO_DEVICE_IOEVENTFD VFIO_DEVICE_IOEVENTFD
#define KA_VFIO_DEVICE_IOEVENTFD_SIZE_MASK VFIO_DEVICE_IOEVENTFD_SIZE_MASK
#define KA_VFIO_DEVICE_RESET VFIO_DEVICE_RESET
#define KA_VFIO_DEVICE_SET_IRQS VFIO_DEVICE_SET_IRQS
#define KA_VFIO_IRQ_INFO_AUTOMASKED VFIO_IRQ_INFO_AUTOMASKED
#define KA_VFIO_IRQ_INFO_EVENTFD VFIO_IRQ_INFO_EVENTFD
#define KA_VFIO_IRQ_INFO_NORESIZE VFIO_IRQ_INFO_NORESIZE
#define KA_VFIO_IRQ_SET_ACTION_MASK VFIO_IRQ_SET_ACTION_MASK
#define KA_VFIO_IRQ_SET_ACTION_TRIGGER VFIO_IRQ_SET_ACTION_TRIGGER
#define KA_VFIO_IRQ_SET_ACTION_TYPE_MASK VFIO_IRQ_SET_ACTION_TYPE_MASK
#define KA_VFIO_IRQ_SET_ACTION_UNMASK VFIO_IRQ_SET_ACTION_UNMASK
#define KA_VFIO_IRQ_SET_DATA_EVENTFD VFIO_IRQ_SET_DATA_EVENTFD
#define KA_VFIO_IRQ_SET_DATA_NONE VFIO_IRQ_SET_DATA_NONE
#define KA_VFIO_PIN_PAGES_MAX_ENTRIES VFIO_PIN_PAGES_MAX_ENTRIES
#define KA_VFIO_REGION_INFO_CAP_SPARSE_MMAP VFIO_REGION_INFO_CAP_SPARSE_MMAP
#define KA_VFIO_REGION_INFO_FLAG_CAPS VFIO_REGION_INFO_FLAG_CAPS
#define KA_VFIO_REGION_INFO_FLAG_MMAP VFIO_REGION_INFO_FLAG_MMAP
#define KA_VFIO_REGION_INFO_FLAG_READ VFIO_REGION_INFO_FLAG_READ
#define KA_VFIO_REGION_INFO_FLAG_WRITE VFIO_REGION_INFO_FLAG_WRITE

#ifdef FUNC_IRQ_BYPASS_REGISTER_PRODUCER_ARG3_IS_INT
#define ka_irq_bypass_register_producer(producer) irq_bypass_register_producer(producer, NULL, 0)
#else
#define ka_irq_bypass_register_producer(producer) irq_bypass_register_producer(producer)
#endif
#define ka_irq_bypass_unregister_producer(producer) irq_bypass_unregister_producer(producer)

#ifdef TYPE_IRQ_BYPASS_PRODUCER_HAS_MEMBER_TOKEN
static inline bool ka_irq_bypass_producer_token_is_err(struct irq_bypass_producer *producer)
{
    return IS_ERR(producer->token);
}

static inline void ka_irq_bypass_producer_set_token(struct irq_bypass_producer *producer, void *token)
{
    producer->token = token;
}
#else
static inline bool ka_irq_bypass_producer_token_is_err(struct irq_bypass_producer *producer)
{
    (void)producer;
    return false;
}

static inline void ka_irq_bypass_producer_set_token(struct irq_bypass_producer *producer, void *token)
{
    (void)producer;
    (void)token;
}
#endif

#define ka_test_and_clear_bit(nr, p) test_and_clear_bit(nr, p)

#endif
