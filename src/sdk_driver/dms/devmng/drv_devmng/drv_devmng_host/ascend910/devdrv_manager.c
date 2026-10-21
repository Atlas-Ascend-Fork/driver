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

#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif
#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif

#include "pbl/pbl_uda.h"
#include "pbl/pbl_runenv_config.h"
#include "pbl/pbl_soc_res.h"

#include "devdrv_manager_common.h"
#include "devdrv_pm.h"
#include "devdrv_driver_pm.h"
#include "devmng_dms_adapt.h"
#include "devdrv_manager_msg.h"
#include "devdrv_platform_resource.h"
#include "devdrv_black_box.h"
#include "devdrv_pcie.h"
#include "devdrv_device_online.h"
#include "devdrv_common.h"
#include "comm_kernel_interface.h"
#include "dms_urd_forward.h"
#include "dms_event_distribute.h"
#include "hvdevmng_init.h"
#include "tsdrv_status.h"
#include "dev_mnt_vdevice.h"
#include "vmng_kernel_interface.h"
#include "devdrv_manager_pid_map.h"
#include "pbl_mem_alloc_interface.h"
#include "davinci_interface.h"
#include "devdrv_user_common.h"
#include "pbl/pbl_davinci_api.h"
#include "devdrv_manager_container.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "svm_kernel_interface.h"
#include "dms_hotreset.h"
#include "ascend_kernel_hal.h"
#include "pbl/pbl_soc_res_attr.h"
#include "pbl/pbl_chip_config.h"

#ifdef CFG_FEATURE_DEVICE_SHARE
#include "devdrv_manager_dev_share.h"
#endif

#ifdef CFG_FEATURE_CHIP_DIE
#include "devdrv_chip_dev_map.h"
#endif

#ifdef CFG_FEATURE_TIMESYNC
#include "dms_time.h"
#endif
#include "adapter_api.h"

#include "pbl/pbl_feature_loader.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_memory_pub.h"
#include "ka_compiler_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_system_pub.h"
#include "ka_list_pub.h"
#include "ka_fs_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_dfx_pub.h"
#include "ka_barrier_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_common_pub.h"
#include "ka_pci_pub.h"
#include "ka_feature_pub.h"

#include "devdrv_manager.h"
#include "devdrv_manager_ioctl.h"
#include "devdrv_chan_msg_process.h"
#include "devdrv_black_box_dump.h"
#include "devdrv_device_status.h"
#include "devdrv_pci_info.h"
#include "devdrv_ipc_notify.h"
#include "devdrv_core_info.h"
#include "devdrv_resource_info.h"
#include "devdrv_shm_info.h"
#include "devdrv_vdev_info.h"

#define DEVMNG_DEV_BOOT_ARG_NUM 3
#define DEVMNG_DEV_ID_LEN 16
#define DEVMNG_DEV_BOOT_INIT_SH "/usr/bin/device_boot_init.sh"

struct devdrv_manager_info *dev_manager_info;
STATIC struct tsdrv_drv_ops devdrv_host_drv_ops;
STATIC ka_rw_semaphore_t devdrv_ops_sem;
STATIC struct devdrv_info *devdrv_info_array[ASCEND_DEV_MAX_NUM];
STATIC struct devdrv_board_info_cache *g_devdrv_board_info[ASCEND_DEV_MAX_NUM] = {NULL};
STATIC int module_init_finish = 0;

STATIC void devdrv_set_devdrv_info_array(u32 dev_id, struct devdrv_info *dev_info);
STATIC void devdrv_manager_uninit_one_device_info(unsigned int dev_id);

static KA_TASK_DEFINE_SPINLOCK(devdrv_spinlock);

#define U32_MAX_BIT_NUM 32

#define AICORE_NUM_MAX 70

#define SET_BIT_64(x, y) ((x) |= ((u64)0x01 << (y)))
#define CLR_BIT_64(x, y) ((x) &= (~((u64)0x01 << (y))))
#define CHECK_BIT_64(x, y) ((x) & ((u64)0x01 << (y)))
#define DEVDRV_INVALID_PHY_ID 0xFFFFFFFF

#ifdef CFG_FEATURE_OLD_DEVID_TRANS
STATIC void devdrv_manager_release_one_device(struct devdrv_info *dev_info);
STATIC int devdrv_manager_create_one_device(struct devdrv_info *dev_info);
#endif
#ifndef CFG_FEATURE_REFACTOR
u32 devdrv_manager_get_ts_num(struct devdrv_info *dev_info);
#endif
#ifdef CONFIG_SYSFS

static u32 sysfs_devid = 0;

#define DEVDRV_ATTR_RO(_name) static ka_base_kobj_attribute_t _name##_attr = __KA_FS_ATTR_RO(_name)

#define DEVDRV_ATTR(_name) \
    static ka_base_kobj_attribute_t _name##_attr = __KA_FS_ATTR(_name, 0600, _name##_show, _name##_store)

STATIC ssize_t devdrv_resources_store(ka_kobject_t *kobj, ka_base_kobj_attribute_t *attr, const char *buf, size_t count)
{
    u32 result;
    int ret;

    if (buf == NULL) {
        devdrv_drv_err("Input buffer is null.\n");
        return count;
    }

    ret = ka_base_kstrtouint(buf, 10, &result); /* base is 10, buf is converted to decimal number */
    if (ret) {
        devdrv_drv_err("unable to transform input string into devid, input string: %s, ret(%d)", buf, ret);
        return count;
    }

    devdrv_drv_info("input devid is: %u.\n", result);

    if (result >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_warn("input devid(%u) is too big, reset into 0, input string: %s", result, buf);
        result = 0;
    }

    sysfs_devid = result;

    return count;
}

STATIC ssize_t devdrv_resources_show(ka_kobject_t *kobj, ka_base_kobj_attribute_t *attr, char *buf)
{
    return 0;
}
DEVDRV_ATTR(devdrv_resources);

// clang-format off
static ka_attribute_t *devdrv_manager_attrs[] = {
    ka_fs_get_dev_attr(devdrv_resources_attr)
    NULL,
};

static ka_attribute_group_t devdrv_manager_attr_group = {
    ka_fs_init_ag_attrs(devdrv_manager_attrs)
    ka_fs_init_ag_name("devdrv_manager")
};
// clang-format on

#endif /* CONFIG_SYSFS */

int copy_from_user_safe(void *to, const void __ka_user *from, unsigned long n)
{
    if ((to == NULL) || (from == NULL) || (n == 0)) {
        devdrv_drv_err("User pointer is NULL.\n");
        return -EINVAL;
    }

    if (ka_base_copy_from_user(to, (void *)from, n)) {
        return -ENODEV;
    }

    return 0;
}
KA_EXPORT_SYMBOL(copy_from_user_safe);

int copy_to_user_safe(void __ka_user *to, const void *from, unsigned long n)
{
    if ((to == NULL) || (from == NULL) || (n == 0)) {
        devdrv_drv_err("User pointer is NULL.\n");
        return -EINVAL;
    }

    if (ka_base_copy_to_user(to, (void *)from, n)) {
        return -ENODEV;
    }

    return 0;
}
KA_EXPORT_SYMBOL(copy_to_user_safe);

STATIC void devdrv_set_devdrv_info_array(u32 dev_id, struct devdrv_info *dev_info)
{
    unsigned long flags;
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("wrong device id, dev_id = %d\n", dev_id);
        return;
    }
    ka_task_spin_lock_irqsave(&devdrv_spinlock, flags);
    devdrv_info_array[dev_id] = dev_info;
    ka_task_spin_unlock_irqrestore(&devdrv_spinlock, flags);
    return;
}

struct devdrv_info *devdrv_get_devdrv_info_array(u32 dev_id)
{
    struct devdrv_info *dev_info = NULL;
    unsigned long flags;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("invalid dev_id(%u)\n", dev_id);
        return NULL;
    }

    ka_task_spin_lock_irqsave(&devdrv_spinlock, flags);
    dev_info = devdrv_info_array[dev_id];
    ka_task_spin_unlock_irqrestore(&devdrv_spinlock, flags);

    return dev_info;
}

#define DEV_READY_WAIT_ONCE_TIME_MS 100
#define UNIT_100MS_PER_SECOND 10
int devdrv_wait_device_ready(u32 dev_id, u32 timeout_second)
{
    struct devdrv_info *dev_info = NULL;
    u32 cycle_time = timeout_second * UNIT_100MS_PER_SECOND;

    while (cycle_time > 0) {
        dev_info = devdrv_get_devdrv_info_array(dev_id);
        if (dev_info == NULL) {
            devdrv_drv_err("Device info is null. (dev_id=%u)\n", dev_id);
            return -ENODEV;
        }

        if (dev_info->dev_ready == DEVDRV_DEV_READY_WORK) {
            break;
        }

        cycle_time--;
        ka_system_msleep(DEV_READY_WAIT_ONCE_TIME_MS);
    }

    if (cycle_time == 0) {
        devdrv_drv_err("Wait device ready timeout. (dev_id=%u; timeout=%u s)\n", dev_id, timeout_second);
        return -ENODEV;
    }

    return 0;
}

STATIC int devdrv_manager_fresh_amp_smp_mode(void)
{
    u32 num_dev, dev_id, phy_id, master_id, vfid;
    int ret;

    num_dev = devdrv_manager_get_devnum();

    for (dev_id = 0; dev_id < num_dev; dev_id++) {
        ret = devdrv_manager_container_logical_id_to_physical_id(dev_id, &phy_id, &vfid);
        if (ret) {
            devdrv_drv_err("logical_id to phy_id fail. (dev_id=%u; dev_num=%u)\n", dev_id, num_dev);
            return ret;
        }

        ret = adap_get_master_devid_in_the_same_os(phy_id, &master_id);
        if (ret) {
            devdrv_drv_err("Get masterId failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
            return ret;
        }

        if (phy_id != master_id) {
            dev_manager_info->amp_or_smp = DEVMNG_SMP_MODE;
            devdrv_drv_info("This machine is smp mode.\n");
            return 0;
        }
    }

    dev_manager_info->amp_or_smp = DEVMNG_AMP_MODE;
    devdrv_drv_info("this machine is amp mode.\n");
    return 0;
}

int devdrv_manager_get_amp_smp_mode(u32 *amp_or_smp)
{
    if ((dev_manager_info == NULL) || (amp_or_smp == NULL)) {
        return -EINVAL;
    }

    (void)devdrv_manager_fresh_amp_smp_mode();
    *amp_or_smp = dev_manager_info->amp_or_smp;
    return 0;
}

struct devdrv_info *devdrv_manager_get_devdrv_info(u32 dev_id)
{
    struct devdrv_info *dev_info = NULL;
    unsigned long flags;

    if ((dev_manager_info == NULL) || (dev_id >= ASCEND_DEV_MAX_NUM)) {
        return NULL;
    }

    ka_task_spin_lock_irqsave(&dev_manager_info->spinlock, flags);
    dev_info = dev_manager_info->dev_info[dev_id];
    ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);

    return dev_info;
}
KA_EXPORT_SYMBOL(devdrv_manager_get_devdrv_info);

STATIC void devdrv_manager_dev_num_increase(unsigned int dev_id)
{
    dev_manager_info->num_dev++;

    if (devdrv_manager_is_pf_device(dev_id)) {
        dev_manager_info->pf_num++;
    } else {
        dev_manager_info->vf_num++;
    }
}

STATIC void devdrv_manager_dev_num_decrease(unsigned int dev_id)
{
    dev_manager_info->num_dev--;

    if (devdrv_manager_is_pf_device(dev_id)) {
        dev_manager_info->pf_num--;
    } else {
        dev_manager_info->vf_num--;
    }
}

STATIC void devdrv_manager_dev_num_reset(void)
{
    dev_manager_info->num_dev = 0;
    dev_manager_info->pf_num = 0;
    dev_manager_info->vf_num = 0;
}

STATIC int devdrv_manager_set_devinfo_inc_devnum(u32 dev_id, struct devdrv_info *dev_info)
{
    unsigned long flags;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (dev_manager_info == NULL) || (dev_info == NULL)) {
        devdrv_drv_err("invalid dev_id(%u), or dev_manager_info(%pK) or dev_info(%pK) is NULL\n", dev_id,
                       dev_manager_info, dev_info);
        return -EINVAL;
    }

    ka_task_spin_lock_irqsave(&dev_manager_info->spinlock, flags);
    if (dev_manager_info->dev_info[dev_id] != NULL) {
        ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);
        devdrv_drv_err("dev_info is not NULL, dev_id(%u)\n", dev_id);
        return -ENODEV;
    }

    if (dev_manager_info->num_dev >= ASCEND_DEV_MAX_NUM) {
        ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);
        devdrv_drv_err("wrong device num, num_dev(%u). dev_id(%u)\n", dev_manager_info->num_dev, dev_id);
        return -EFAULT;
    }

    devdrv_manager_dev_num_increase(dev_id);
    dev_manager_info->dev_info[dev_id] = dev_info;
    dev_manager_info->dev_id[dev_id] = dev_id;
    ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);

    return 0;
}

STATIC int devdrv_manager_reset_devinfo_dec_devnum(u32 dev_id)
{
    unsigned long flags;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (dev_manager_info == NULL)) {
        devdrv_drv_err("invalid dev_id(%u), or dev_manager_info is NULL\n", dev_id);
        return -EINVAL;
    }

    ka_task_spin_lock_irqsave(&dev_manager_info->spinlock, flags);
    if (dev_manager_info->dev_info[dev_id] == NULL) {
        ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);
        devdrv_drv_err("dev_info is not NULL, dev_id(%u)\n", dev_id);
        return -ENODEV;
    }

    if ((dev_manager_info->num_dev > ASCEND_DEV_MAX_NUM) || (dev_manager_info->num_dev == 0)) {
        ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);
        devdrv_drv_err("wrong device num, num_dev(%u). dev_id(%u)\n", dev_manager_info->num_dev, dev_id);
        return -EFAULT;
    }

    dev_manager_info->dev_id[dev_id] = 0;
    dev_manager_info->dev_info[dev_id] = NULL;
    devdrv_manager_dev_num_decrease(dev_id);
    dev_manager_info->device_status[dev_id] = DRV_STATUS_INITING;
    ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);

    return 0;
}

STATIC int devdrv_manager_set_devdrv_info(u32 dev_id, struct devdrv_info *dev_info)
{
    unsigned long flags;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (dev_manager_info == NULL)) {
        devdrv_drv_err("Invalid dev_id or dev_manager_info is NULL. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ka_task_spin_lock_irqsave(&dev_manager_info->spinlock, flags);
    dev_manager_info->dev_info[dev_id] = dev_info;
    ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);

    return 0;
}

int devdrv_get_platformInfo(u32 *info)
{
    if (info == NULL) {
        return -EINVAL;
    }

    *info = DEVDRV_MANAGER_HOST_ENV;
    return 0;
}
KA_EXPORT_SYMBOL(devdrv_get_platformInfo);

int devdrv_get_devinfo(unsigned int devid, struct devdrv_device_info *info)
{
#ifndef CFG_FEATURE_REFACTOR
    struct devdrv_platform_data *pdata = NULL;
#endif
    struct devdrv_info *dev_info = NULL;

    if (info == NULL) {
        devdrv_drv_err("invalid parameter, dev_id = %d\n", devid);
        return -EINVAL;
    }

    dev_info = devdrv_manager_get_devdrv_info(devid);
    if (dev_info == NULL) {
        devdrv_drv_err("device is not ready, devid = %d\n", devid);
        return -ENODEV;
    }

    /* check if received device ready message from device side */
    if (dev_info->dev_ready == 0) {
        devdrv_drv_err("device(%u) not ready!", dev_info->dev_id);
        return -ENODEV;
    }

    info->ctrl_cpu_ip = dev_info->ctrl_cpu_ip;
    info->ctrl_cpu_id = dev_info->ctrl_cpu_id;
    info->ctrl_cpu_core_num = dev_info->ctrl_cpu_core_num;
    info->ctrl_cpu_occupy_bitmap = dev_info->ctrl_cpu_occupy_bitmap;
    info->ctrl_cpu_endian_little = dev_info->ctrl_cpu_endian_little;
#ifndef CFG_FEATURE_REFACTOR
    pdata = dev_info->pdata;
    info->ts_cpu_core_num = pdata->ts_pdata[0].ts_cpu_core_num;
#else
    info->ts_cpu_core_num = 0;
#endif
    info->ai_cpu_core_num = dev_info->ai_cpu_core_num;
    info->ai_core_num = dev_info->ai_core_num;
    info->aicpu_occupy_bitmap = dev_info->aicpu_occupy_bitmap;
    info->env_type = dev_info->env_type;

    return 0;
}
KA_EXPORT_SYMBOL_GPL(devdrv_get_devinfo);

int devdrv_manager_devid_to_nid(u32 devid, u32 mem_type)
{
    return KA_NUMA_NO_NODE;
}
KA_EXPORT_SYMBOL(devdrv_manager_devid_to_nid);

STATIC inline void *devdrv_manager_vzalloc(size_t alloc_size)
{
    return dbl_vmalloc(alloc_size, KA_GFP_KERNEL | __KA_GFP_ZERO | __KA_GFP_ACCOUNT, KA_PAGE_KERNEL);
}

STATIC struct devdrv_manager_context *devdrv_manager_context_init(void)
{
    struct devdrv_manager_context *dev_manager_context = NULL;
    size_t ctx_size = sizeof(struct devdrv_manager_context);

    dev_manager_context = (struct devdrv_manager_context *)dbl_kzalloc(ctx_size, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (dev_manager_context == NULL) {
        devdrv_drv_err("vmalloc dev_manager_context is NULL.\n");
        return NULL;
    }

    dev_manager_context->pid = ka_task_get_current_pid();
    dev_manager_context->tgid = ka_task_get_current_tgid();
    dev_manager_context->task = ka_task_get_current();
    dev_manager_context->start_time = ka_task_get_current_starttime();
    dev_manager_context->real_start_time = get_start_time(ka_task_get_current());
    dev_manager_context->mnt_ns = ka_task_get_current_mnt_ns();
    dev_manager_context->pid_ns = ka_task_get_current_pid_ns();

    if (devdrv_manager_ipc_notify_init(dev_manager_context)) {
        devdrv_drv_err("manager ipc id init failed\n");
        dbl_kfree(dev_manager_context);
        dev_manager_context = NULL;
        return NULL;
    }

    return dev_manager_context;
}

void devdrv_manager_context_uninit(struct devdrv_manager_context *dev_manager_context)
{
    if (dev_manager_context == NULL) {
        return;
    }
    devdrv_manager_ipc_notify_uninit(dev_manager_context);
    dbl_kfree(dev_manager_context);
    dev_manager_context = NULL;
}

STATIC int devdrv_manager_open(ka_inode_t *inode, ka_file_t *filep)
{
    struct devdrv_manager_context *dev_manager_context = NULL;

    dev_manager_context = devdrv_manager_context_init();
    if (dev_manager_context == NULL) {
        devdrv_drv_err("context init failed\n");
        return -ENOMEM;
    }
    filep->private_data = dev_manager_context;

#ifndef CFG_FEATURE_APM_SUPP_PID
#if defined(CFG_HOST_ENV)
    if (ka_feature_is_enable()) {
        devdrv_check_pid_map_process_sign(dev_manager_context->tgid, dev_manager_context->start_time);
    }
#endif
#endif
    return 0;
}

STATIC int devdrv_manager_release(ka_inode_t *inode, ka_file_t *filep)
{
    struct devdrv_manager_context *dev_manager_context = NULL;

    if (filep == NULL) {
        devdrv_drv_err("filep is NULL.\n");
        return -EINVAL;
    }

    if (filep->private_data == NULL) {
        devdrv_drv_err("filep private_data is NULL.\n");
        return -EINVAL;
    }

    dev_manager_context = filep->private_data;
    devdrv_host_black_box_close_check(dev_manager_context->tgid);
    devdrv_manager_resource_recycle(dev_manager_context);
    filep->private_data = NULL;
    return 0;
}

struct devdrv_manager_info *devdrv_get_manager_info(void)
{
    return dev_manager_info;
}
KA_EXPORT_SYMBOL(devdrv_get_manager_info);

int devdrv_try_get_dev_info_occupy(struct devdrv_info *dev_info)
{
    if (dev_info == NULL) {
        devdrv_drv_err("The dev_info is NULL\n");
        return -EFAULT;
    }

    ka_task_mutex_lock(&dev_info->lock);
    ka_base_atomic_inc(&dev_info->occupy_ref);
    if (dev_info->status == DEVINFO_STATUS_REMOVED) {
        ka_base_atomic_dec(&dev_info->occupy_ref);
        ka_task_mutex_unlock(&dev_info->lock);
        devdrv_drv_err("The dev_info has been removed.\n");
        return -EFAULT;
    }
    ka_task_mutex_unlock(&dev_info->lock);

    return 0;
}
KA_EXPORT_SYMBOL(devdrv_try_get_dev_info_occupy);

void devdrv_put_dev_info_occupy(struct devdrv_info *dev_info)
{
    if (dev_info == NULL) {
        devdrv_drv_err("The dev_info is NULL\n");
        return;
    }

    ka_base_atomic_dec(&dev_info->occupy_ref);
}
KA_EXPORT_SYMBOL(devdrv_put_dev_info_occupy);

#ifdef CFG_FEATURE_CHIP_DIE
int devdrv_manager_get_random_from_dev_info(u32 devid, char *random_number, u32 random_len)
{
    struct devdrv_info *dev_info = NULL;
    u32 phys_id, vfid;
    int ret;

    if ((random_len < DEVMNG_SHM_INFO_RANDOM_SIZE) || (random_number == NULL)) {
        devdrv_drv_err("Invalid parameter. (random_len=%u; random_number=\"%s\")\n", random_len,
                       (random_number == NULL) ? "NULL" : "OK");
        return -EINVAL;
    }

    if (devdrv_manager_container_logical_id_to_physical_id(devid, &phys_id, &vfid)) {
        devdrv_drv_err("Logical id transform physical id failed. (devid=%u; phys_id=%u)\n", devid, phys_id);
        return -ENODEV;
    }

    dev_info = devdrv_get_devdrv_info_array(phys_id);
    if (dev_info == NULL) {
        devdrv_drv_err("Device info is NULL. (phys_id=%u)\n", phys_id);
        return -ENODEV;
    }

    ret = memcpy_s(random_number, random_len, dev_info->random_number, DEVMNG_SHM_INFO_RANDOM_SIZE);
    if (ret != 0) {
        devdrv_drv_err("Memcpy random from device info failed. (devid=%u; phys_id=%u)\n", devid, phys_id);
        return ret;
    }

    return 0;
}
#endif

STATIC int devdrv_host_notice_dev_process_exit(u32 phy_id, int host_pid)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}, {0}};
    struct devdrv_ioctl_para_query_pid *para_info_tmp = NULL;
    struct devdrv_info *info = NULL;
    int out_len = 0;
    int ret;

    info = devdrv_manager_get_devdrv_info(phy_id);
    if (info == NULL) {
        return 0;
    }
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_NOTICE_PROCESS_EXIT;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_VALID;
    /* give a random value for checking result later */
    dev_manager_msg_info.header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;
    /* inform corresponding devid to device side */
    dev_manager_msg_info.header.dev_id = info->dev_id;

    para_info_tmp = (struct devdrv_ioctl_para_query_pid *)dev_manager_msg_info.payload;
    para_info_tmp->host_pid = host_pid;

    ret = devdrv_manager_send_msg(info, &dev_manager_msg_info, &out_len);
    if ((ret != 0) ||
        (out_len != (sizeof(struct devdrv_ioctl_para_query_pid) + sizeof(struct devdrv_manager_msg_head)))) {
        /* Ignore sending failure */
        return 0;
    }

    return (dev_manager_msg_info.header.result == 0) ? 0 : -ETXTBSY;
#endif
}

#ifdef CFG_FEATURE_NOTIFY_REBOOT
STATIC int devdrv_host_notice_reboot(u32 phy_id)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}, {0}};
    struct devdrv_info *info = NULL;
    int out_len = 0;
    int ret;

    info = devdrv_manager_get_devdrv_info(phy_id);
    if (info == NULL) {
        devdrv_drv_err("Get devinfo is null. (phy_id=%u)\n", phy_id);
        return -EAGAIN;
    }
    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_NOTICE_REBOOT;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_VALID;
    dev_manager_msg_info.header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;
    dev_manager_msg_info.header.dev_id = info->dev_id;

    ret = devdrv_manager_send_msg(info, &dev_manager_msg_info, &out_len);
    if (ret != 0) {
        devdrv_drv_err("send msg fail. (ret=%d)\n", ret);
        return -EAGAIN;
    }
    if (out_len != sizeof(struct devdrv_manager_msg_head)) {
        devdrv_drv_err("send msg out_len invalid. (out_len=%d)\n", out_len);
        return -EAGAIN;
    }
    if (dev_manager_msg_info.header.result != 0) {
        devdrv_drv_err("send msg header result fail. (result=%u)\n", dev_manager_msg_info.header.result);
        return -EAGAIN;
    }
    return 0;
}
STATIC void devdrv_notify_all_dev_reboot(void)
{
    unsigned int i;
    int ret;

    if (run_in_virtual_mach()) {
        devdrv_drv_warn("In VM, dose not notice device set flag.\n");
        return;
    }

    for (i = 0; i < ASCEND_PDEV_MAX_NUM; ++i) {
        if (!uda_is_udevid_exist(i)) {
            continue;
        }

        ret = devdrv_host_notice_reboot(i);
        if (ret != 0) {
            return;
        }
    }
}
#endif

#define MAX_NOTICE_DEV_EXIT_TIMES 1000
STATIC void devdrv_host_release_notice_dev(int host_pid)
{
    u32 did, try_time;
    int ret;

    for (did = 0, try_time = 0; did < ASCEND_DEV_MAX_NUM; did++) {
        do {
            ret = devdrv_host_notice_dev_process_exit(did, host_pid);
            if (ret != 0) {
                ka_system_usleep_range(10, 20); /* 10-20 us */
                try_time++;
            }
        } while ((ret != 0) && (try_time < MAX_NOTICE_DEV_EXIT_TIMES));
    }
}

int devdrv_manager_get_docker_id(u32 *docker_id)
{
    if (devdrv_manager_container_is_in_container()) {
        return devdrv_manager_container_get_docker_id(docker_id);
    } else {
        *docker_id = MAX_DOCKER_NUM;
        return 0;
    }
}
KA_EXPORT_SYMBOL(devdrv_manager_get_docker_id);

STATIC ssize_t devdrv_manager_read(ka_file_t *filep, char __ka_user *buf, size_t count, loff_t *ppos)
{
    return 0;
}

// clang-format off
const ka_file_operations_t devdrv_manager_file_operations = {
    ka_fs_init_f_owner(KA_THIS_MODULE)
    ka_fs_init_f_read(devdrv_manager_read)
    ka_fs_init_f_open(devdrv_manager_open)
    ka_fs_init_f_release(devdrv_manager_release)
    ka_fs_init_f_unlocked_ioctl(devdrv_manager_ioctl)
    ka_fs_init_f_poll(devdrv_manager_poll)
};
// clang-format on

int devdrv_manager_send_msg(struct devdrv_info *dev_info, struct devdrv_manager_msg_info *dev_manager_msg_info,
                            int *out_len)
{
    int ret;

    ret = devdrv_common_msg_send(dev_info->pci_dev_id, dev_manager_msg_info, sizeof(struct devdrv_manager_msg_info),
                                 sizeof(struct devdrv_manager_msg_info), (u32 *)out_len,
                                 DEVDRV_COMMON_MSG_DEVDRV_MANAGER);

    return ret;
}

STATIC int devdrv_manager_send_devid(struct devdrv_info *dev_info)
{
    struct devdrv_manager_msg_info dev_manager_msg_info;
    u32 out_len;
    int ret;
    u32 i;

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_SEND_DEVID;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_VALID;

    /* give a random value for checking result later */
    dev_manager_msg_info.header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;

    /* inform corresponding devid to device side */
    dev_manager_msg_info.header.dev_id = dev_info->dev_id;

    for (i = 0; i < DEVDRV_MANAGER_INFO_PAYLOAD_LEN; i++) {
        dev_manager_msg_info.payload[i] = 0;
    }

    ret = devdrv_common_msg_send(dev_info->pci_dev_id, &dev_manager_msg_info, sizeof(dev_manager_msg_info),
                                 sizeof(dev_manager_msg_info), &out_len, DEVDRV_COMMON_MSG_DEVDRV_MANAGER);
    if (ret || (dev_manager_msg_info.header.result != 0)) {
        devdrv_drv_err("send dev_id(%u) to device(%u) failed, ret(%d)\n", dev_info->dev_id, dev_info->pci_dev_id, ret);
        return -EFAULT;
    }

    return 0;
}

STATIC int devdrv_get_board_info_from_dev(unsigned int dev_id, struct devdrv_board_info_cache *board_info)
{
    int ret;
    int vfid = 0;
    DMS_FEATURE_S feature_cfg = {0};
    struct urd_forward_msg urd_msg = {0};
    struct devdrv_info *dev_info = NULL;

    feature_cfg.main_cmd = DMS_MAIN_CMD_SOC;
    feature_cfg.sub_cmd = DMS_SUBCMD_GET_SOC_INFO;
    feature_cfg.filter = NULL;

    dev_info = devdrv_manager_get_devdrv_info(dev_id);
    if (dev_info == NULL) {
        devdrv_drv_err("Device is not initialized. (phy_id=%u)\n", dev_id);
        return -ENODEV;
    }

    ret = devdrv_try_get_dev_info_occupy(dev_info);
    if (ret != 0) {
        devdrv_drv_warn("Device has been reset. (phy_id=%u)\n", dev_id);
        return -EINVAL;
    }

    ret = dms_set_urd_msg(&feature_cfg, (void *)&dev_id, sizeof(u32), sizeof(struct devdrv_board_info_cache), &urd_msg);
    if (ret != 0) {
        devdrv_drv_err("dms_set_urd_msg failed. (phy_id=%u; ret=%d)\n", dev_id, ret);
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

    ret = dms_urd_forward_send_to_device(dev_id, vfid, &urd_msg, (void *)board_info,
                                         sizeof(struct devdrv_board_info_cache));
    if (ret != 0) {
        devdrv_drv_ex_notsupport_err(ret, "dms_urd_forward_send_to_device failed. (phy_id=%u; ret=%d)\n", dev_id, ret);
        goto OCCUPY_AND_TASK_CNT_OUT;
    }

OCCUPY_AND_TASK_CNT_OUT:
    ka_base_atomic_dec(&dev_info->occupy_ref);
    return ret;
}

STATIC int dms_get_slot_id_by_dmi(unsigned int dev_id, unsigned int *slot_id)
{
    unsigned long long product_type;
    ka_pci_dev_t *pci_pdev = NULL;
    int ret;

    ret = soc_resmng_dev_get_key_value(dev_id, "PRODUCT_TYPE", &product_type);
    if (ret != 0) {
        devdrv_drv_err("Get product type failed. (dev_id=%u, ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (product_type == 0x3) { /* 0x3: pcie card */
        pci_pdev = hal_kernel_devdrv_get_pci_pdev_by_devid(dev_id);
        if (pci_pdev == NULL) {
            devdrv_drv_err("Failed to obtain the pci_pdev. (dev_id=%u)\n", dev_id);
            return -ENODEV;
        }

        ret = ka_driver_dmi_find_devid(pci_pdev, DMI_DEV_TYPE_DEV_SLOT, slot_id);
        if (ret != 0) {
            devdrv_drv_ex_notsupport_err(ret, "Failed to find the device by DMI. (dev_id=%u; ret=%d)\n", dev_id, ret);
            return ret;
        }
    }

    return 0;
}

STATIC int dms_host_set_slot_id(u32 dev_id, u32 slot_id)
{
    struct devdrv_manager_msg_info dev_manager_msg_info = {{0}, {0}};
    int out_len = 0;
    int ret;

    dev_manager_msg_info.header.msg_id = DEVDRV_MANAGER_CHAN_H2D_HOST_SET_SLOT_ID;
    dev_manager_msg_info.header.valid = DEVDRV_MANAGER_MSG_VALID;
    dev_manager_msg_info.header.result = (u16)DEVDRV_MANAGER_MSG_INVALID_RESULT;
    dev_manager_msg_info.header.dev_id = dev_id;
    *(u32 *)dev_manager_msg_info.payload = slot_id;

    ret = devdrv_common_msg_send(dev_id, (void *)&dev_manager_msg_info, sizeof(struct devdrv_manager_msg_info),
                                 sizeof(struct devdrv_manager_msg_info), (u32 *)&out_len,
                                 DEVDRV_COMMON_MSG_DEVDRV_MANAGER);
    if (ret != 0) {
        devdrv_drv_err("Send msg failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return -EAGAIN;
    }
    if (out_len != sizeof(struct devdrv_manager_msg_head)) {
        devdrv_drv_err("Send msg out_len invalid. (dev_id=%u; out_len=%d)\n", dev_id, out_len);
        return -EAGAIN;
    }
    if (dev_manager_msg_info.header.result != 0) {
        devdrv_drv_err("Send msg header result failed. (dev_id=%u; result=%u)\n", dev_id,
                       dev_manager_msg_info.header.result);
        return -EAGAIN;
    }
    return 0;
}

STATIC int devdrv_save_board_info_in_host(unsigned int dev_id)
{
    int ret;
    u32 chip_type;
    struct devdrv_board_info_cache *board_info = NULL;

    if (g_devdrv_board_info[dev_id] == NULL) {
        board_info = (struct devdrv_board_info_cache *)dbl_kzalloc(sizeof(struct devdrv_board_info_cache),
                                                                   KA_GFP_KERNEL);
        if (board_info == NULL) {
            devdrv_drv_err("board info kzalloc failed. (dev_id=%u)\n", dev_id);
            return -ENOMEM;
        }

        ret = devdrv_get_board_info_from_dev(dev_id, board_info);
        if (ret != 0) {
            devdrv_drv_ex_notsupport_err(ret, "devdrv get board info from dev failed. (dev_id=%u; ret=%d)\n", dev_id,
                                         ret);
            goto release_board_info;
        }

        g_devdrv_board_info[dev_id] = board_info;

        chip_type = uda_get_chip_type(dev_id);
        if ((chip_type == HISI_CLOUD_V4) || (chip_type == HISI_CLOUD_V5) || (chip_type == HISI_MINI_V4)) {
            ret = dms_get_slot_id_by_dmi(dev_id, &g_devdrv_board_info[dev_id]->slot_id);
            if (ret != 0) {
                devdrv_drv_ex_notsupport_err(ret, "Failed to obtain the pci_pdev. (dev_id=%u; ret=%d)\n", dev_id, ret);
                goto release_board_info;
            }

            ret = dms_host_set_slot_id(dev_id, g_devdrv_board_info[dev_id]->slot_id);
            if (ret != 0) {
                devdrv_drv_ex_notsupport_err(ret, "Failed to set the slot id. (dev_id=%u; ret=%d)\n", dev_id, ret);
                goto release_board_info;
            }
        }
    }

    return 0;

release_board_info:
    if (g_devdrv_board_info[dev_id] == board_info) {
        g_devdrv_board_info[dev_id] = NULL;
    }
    dbl_kfree(board_info);
    board_info = NULL;
    return ret;
}

STATIC void devdrv_board_info_init(unsigned int dev_id)
{
    u32 task_cnt = 0;

    /* retry times: 10 */
    while (task_cnt < 10) {
        if (devdrv_save_board_info_in_host(dev_id) == 0) {
            devdrv_drv_info("succeed to save board info into host. (dev_id=%u; task_cnt=%u; board_id=%u; pcb_id=%u; "
                            "bom_id=%u; slot_id=%u)\n",
                            dev_id, task_cnt, g_devdrv_board_info[dev_id]->board_id,
                            g_devdrv_board_info[dev_id]->pcb_id, g_devdrv_board_info[dev_id]->bom_id,
                            g_devdrv_board_info[dev_id]->slot_id);
            return;
        }
        /* Check interval: 2 seconds */
        ka_system_ssleep(2);
        task_cnt++;
    }
}

int devdrv_manager_register(struct devdrv_info *dev_info)
{
    if (dev_info == NULL) {
        devdrv_drv_err("devdrv manager has not initialized\n");
        return -EINVAL;
    }

    if (devdrv_manager_get_devdrv_info(dev_info->dev_id) != NULL) {
        devdrv_drv_err("device(%u) has already registered\n", dev_info->dev_id);
        return -ENODEV;
    }

    if (devdrv_manager_get_dev_resource(dev_info)) {
        devdrv_drv_err("devdrv manager get device(%u) resource failed\n", dev_info->dev_id);
        return -EINVAL;
    }
    dev_info->drv_ops = &devdrv_host_drv_ops;

    if (devdrv_manager_set_devinfo_inc_devnum(dev_info->dev_id, dev_info)) {
        devdrv_drv_err("devdrv_manager_set_devinfo_inc_devnum failed, deviceid : %u\n", dev_info->dev_id);
        goto devinfo_iounmap;
    }

    if (devdrv_manager_send_devid(dev_info)) {
        devdrv_drv_err("send devid to device(%u) failed.\n", dev_info->dev_id);
        goto devinfo_unregister_client;
    }

    devdrv_board_info_init(dev_info->dev_id);

    if (dms_device_register(dev_info)) {
        devdrv_drv_err("Dms device register failed. (dev_id=%u)\n", dev_info->dev_id);
        goto devinfo_unregister_client;
    }

#ifndef CFG_FEATURE_UB
    if (adap_set_module_init_finish(dev_info->dev_id, DEVDRV_HOST_MODULE_DEVMNG)) {
        devdrv_drv_err("set module init finish failed, dev_id = %u\n", dev_info->dev_id);
        goto devinfo_unregister_client;
    }
#endif

    return 0;
devinfo_unregister_client:
    (void)devdrv_manager_reset_devinfo_dec_devnum(dev_info->dev_id);
devinfo_iounmap:
    return -EINVAL;
}
KA_EXPORT_SYMBOL(devdrv_manager_register);

void devdrv_manager_unregister(struct devdrv_info *dev_info)
{
    devdrv_drv_debug("devdrv_manager_unregister started.\n");

    if ((dev_info == NULL) || (dev_manager_info == NULL) || (dev_info->dev_id >= ASCEND_DEV_MAX_NUM)) {
        devdrv_drv_err("dev_info(%pK) or dev_manager_info(%pK) is NULL, dev_id(%u)\n", dev_info, dev_manager_info,
                       (dev_info == NULL) ? ASCEND_DEV_MAX_NUM : dev_info->dev_id);
        return;
    }

    if (dev_manager_info->dev_info[dev_info->dev_id] == NULL) {
        devdrv_drv_err("device(%u) is not initialized\n", dev_info->dev_id);
        return;
    }
    if (devdrv_manager_reset_devinfo_dec_devnum(dev_info->dev_id)) {
        devdrv_drv_err("devdrv_manager_unregister device(%u) fail !!!!!!!\n", dev_info->dev_id);
        return;
    }

    dms_device_unregister(dev_info);

    devdrv_drv_debug("devdrv_manager_unregister device(%u) finished, "
                     "dev_manager_info->num_dev = %d\n",
                     dev_info->dev_id, dev_manager_info->num_dev);
}
KA_EXPORT_SYMBOL(devdrv_manager_unregister);

void __attribute__((weak)) devdrv_host_generate_sdid(struct devdrv_info *dev_info)
{}
STATIC int devdrv_manager_device_ready_info(struct devdrv_info *dev_info, struct devdrv_device_info *drv_info)
{
    int ret;
#ifndef CFG_FEATURE_REFACTOR
    u32 tsid = 0;
    struct devdrv_platform_data *pdata = dev_info->pdata;
    ka_task_spin_lock_bh(&dev_info->spinlock);
    dev_info->ai_core_num = drv_info->ai_core_num;
    dev_info->aicore_freq = drv_info->aicore_freq;
    pdata->ai_core_num_level = drv_info->ai_core_num_level;
    pdata->ai_core_freq_level = drv_info->ai_core_freq_level;
    dev_info->ai_cpu_core_num = drv_info->ai_cpu_core_num;
    dev_info->ctrl_cpu_core_num = drv_info->ctrl_cpu_core_num;
    dev_info->ctrl_cpu_occupy_bitmap = drv_info->ctrl_cpu_occupy_bitmap;

    dev_info->ctrl_cpu_id = drv_info->ctrl_cpu_id;
    dev_info->ctrl_cpu_ip = drv_info->ctrl_cpu_ip;
    pdata->ts_pdata[tsid].ts_cpu_core_num = drv_info->ts_cpu_core_num;

    dev_info->ai_cpu_core_id = drv_info->ai_cpu_core_id;
    dev_info->aicpu_occupy_bitmap = drv_info->aicpu_occupy_bitmap;

    dev_info->inuse.ai_core_num = drv_info->ai_core_ready_num;
    dev_info->inuse.ai_core_error_bitmap = drv_info->ai_core_broken_map;
    dev_info->inuse.ai_cpu_num = drv_info->ai_cpu_ready_num;
    dev_info->inuse.ai_cpu_error_bitmap = drv_info->ai_cpu_broken_map;
    dev_info->ai_subsys_ip_broken_map = drv_info->ai_subsys_ip_map;

    dev_info->ffts_type = drv_info->ffts_type;
    dev_info->vector_core_num = drv_info->vector_core_num;
    dev_info->vector_core_freq = drv_info->vector_core_freq;
    dev_info->aicore_bitmap = drv_info->aicore_bitmap;
    dev_info->vector_core_bitmap = drv_info->vector_core_bitmap;

    dev_info->chip_id = drv_info->chip_id;
    dev_info->multi_chip = drv_info->multi_chip;
    dev_info->multi_die = drv_info->multi_die;
    dev_info->mainboard_id = drv_info->mainboard_id;
    dev_info->addr_mode = drv_info->addr_mode;
    dev_info->connect_type = drv_info->connect_type;
    dev_info->board_id = drv_info->board_id;
    dev_info->server_id = drv_info->server_id;
    dev_info->scale_type = drv_info->scale_type;
    dev_info->super_pod_id = drv_info->super_pod_id;

    dev_info->die_id = drv_info->die_id;

    ka_task_spin_unlock_bh(&dev_info->spinlock);

#ifdef CFG_FEATURE_PG
    ret = strcpy_s(dev_info->pg_info.spePgInfo.socVersion, MAX_CHIP_NAME, drv_info->soc_version);
    if (ret) {
        devdrv_drv_err("Call strcpy_s failed.\n");
        return 0;
    }
#endif

    devdrv_drv_info("device: %u"
                    "ai cpu num: %d, "
                    "ai cpu broken bitmap: 0x%x, "
                    "ai core num: %d, vector core num:%d, "
                    "ai core bitmap:0x%llx, vector core bitmap:0x%llx, "
                    "ai core broken bitmap: 0x%x, "
                    "ai subsys broken map: 0x%x.\n",
                    dev_info->dev_id, drv_info->ai_cpu_ready_num, drv_info->ai_cpu_broken_map,
                    drv_info->ai_core_ready_num, dev_info->vector_core_num, dev_info->aicore_bitmap,
                    dev_info->vector_core_bitmap, drv_info->ai_core_broken_map, drv_info->ai_subsys_ip_map);
#endif
    if ((dev_info->ai_cpu_core_num > U32_MAX_BIT_NUM) || (dev_info->ai_core_num > AICORE_NUM_MAX)) {
        devdrv_drv_err("Invalid core num. (aicpu=%u; aicore=%u; max_aicpu_bit=%u; max_aicore_bit=%u)\n",
                       dev_info->ai_cpu_core_num, dev_info->ai_core_num, U32_MAX_BIT_NUM, AICORE_NUM_MAX);
        return -ENODEV;
    }

    (void)hvdevmng_set_core_num(dev_info->dev_id, 0, 0);

    ret = soc_resmng_dev_set_mia_res(dev_info->dev_id, MIA_AC_AIC, dev_info->aicore_bitmap, 1);
    devdrv_drv_info("Set aicore bitmap. (devid=%u; aicore_num=%u; bitmap=0x%llx; ret=%d)\n", dev_info->dev_id,
                    dev_info->ai_core_num, dev_info->aicore_bitmap, ret);

    dev_info->ai_core_id = drv_info->ai_core_id;
    dev_info->ctrl_cpu_endian_little = drv_info->ctrl_cpu_endian_little;
    dev_info->env_type = drv_info->env_type;
    dev_info->hardware_version = drv_info->hardware_version;

    dev_info->dump_ddr_dma_addr = drv_info->dump_ddr_dma_addr;
    dev_info->dump_ddr_size = drv_info->dump_ddr_size;
    dev_info->capability = drv_info->capability;
    dev_info->reg_ddr_dma_addr = drv_info->reg_ddr_dma_addr;
    dev_info->reg_ddr_size = drv_info->reg_ddr_size;
#ifdef CFG_FEATURE_PCIE_BBOX_DUMP
    devdrv_devlog_init(dev_info, drv_info);
#endif

    dev_info->chip_name = drv_info->chip_name;
    dev_info->chip_version = drv_info->chip_version;
    dev_info->chip_info = drv_info->chip_info;

    dev_info->dev_nominal_osc_freq = drv_info->dev_nominal_osc_freq;

    devdrv_host_generate_sdid(dev_info);

#ifdef CFG_FEATURE_SRIOV
    ret = strcpy_s(dev_info->template_name, TEMPLATE_NAME_LEN, drv_info->template_name);
    if (ret != 0) {
        devdrv_drv_err("Copy name length failed.(devid=%u; ret=%d)\n", dev_info->dev_id, ret);
        return -EINVAL;
    }
#endif

    devdrv_drv_info("Initialize chip info. (chip_name=%u; chip_version=%u; nominal_osc_freq=%llu Hz)\n",
                    dev_info->chip_name, dev_info->chip_version, dev_info->dev_nominal_osc_freq);

    devdrv_drv_debug("received ready message from pcie device(%u)\n", dev_info->dev_id);
    devdrv_drv_debug(" ai_core_num = %d, ai_cpu_core_num = %d, "
                     "ctrl_cpu_core_num = %d, ctrl_cpu_endian_little = %d, "
                     "ctrl_cpu_id = %d, ctrl_cpu_ip = %d, "
                     "ai_core_id = %d, "
                     "ai_cpu_core_id = %d\n",
                     dev_info->ai_core_num, dev_info->ai_cpu_core_num, dev_info->ctrl_cpu_core_num,
                     dev_info->ctrl_cpu_endian_little, dev_info->ctrl_cpu_id, dev_info->ctrl_cpu_ip,
                     dev_info->ai_core_id, dev_info->ai_cpu_core_id);
#ifndef CFG_FEATURE_REFACTOR
    devdrv_drv_debug("ts_cpu_core_num = %d\n", pdata->ts_pdata[tsid].ts_cpu_core_num);
#endif
    devdrv_drv_debug("env_type = %d\n", dev_info->env_type);
#ifdef CFG_FEATURE_CHIP_DIE
    ret = memcpy_s(dev_info->random_number, DEVMNG_SHM_INFO_RANDOM_SIZE, drv_info->random_number,
                   DEVMNG_SHM_INFO_RANDOM_SIZE);
    if (ret != 0) {
        devdrv_drv_err("Memcpy random number failed. (dev_id=%u, ret=%d)\n", dev_info->dev_id, ret);
    }
#endif
    return 0;
}

STATIC void devdrv_manager_set_base_dev_info(u32 dev_id, struct devdrv_info *dev_info)
{
    struct dbl_dev_base_info dev_base_info = {0};

    dev_base_info.chip_id = dev_info->chip_id;
    dev_base_info.die_id = dev_info->die_id;
    dev_base_info.addr_mode = dev_info->addr_mode;
    dev_base_info.multi_die = dev_info->multi_die;
    dev_base_info.dev_ready = DEVDRV_DEV_READY_WORK;

    dbl_set_dev_base_info(dev_id, dev_base_info);
}

int devdrv_manager_device_ready(void *msg, u32 *ack_len)
{
    struct devdrv_manager_msg_info *dev_manager_msg_info = NULL;
    struct devdrv_device_info *drv_info = NULL;
#ifndef CFG_FEATURE_REFACTOR
    struct devdrv_platform_data *pdata = NULL;
#endif
    struct devdrv_info *dev_info = NULL;
    u32 dev_id;
    int ret;

    if (module_init_finish == 0) {
        devdrv_drv_warn("Init not finish.\n");
        return -EINVAL;
    }

    dev_manager_msg_info = (struct devdrv_manager_msg_info *)msg;
    dev_id = dev_manager_msg_info->header.dev_id;
    if (dev_manager_msg_info->header.valid != DEVDRV_MANAGER_MSG_VALID) {
        devdrv_drv_err("invalid message from host\n");
        return -EINVAL;
    }

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Invalid dev_id. (dev_id=%u; max=%d)\n", dev_id, ASCEND_DEV_MAX_NUM);
        return -ENODEV;
    }

    /* dev send message with pcie device id, get dev_info from devdrv_info_array */
    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if (dev_info == NULL) {
        devdrv_drv_warn("Device is not ready. (dev_id=%u)\n", dev_id);
        dev_manager_msg_info->header.result = EAGAIN;
        goto READY_EXIT;
    }

    if (dev_info->work.func == NULL) {
        devdrv_drv_warn("Work is not ready. (dev_id=%u)\n", dev_id);
        dev_manager_msg_info->header.result = EAGAIN;
        goto READY_EXIT;
    }

    if (dev_info->dev_ready != 0) {
        devdrv_drv_info("Device already informed. (dev_id=%u)\n", dev_id);
        dev_manager_msg_info->header.result = EINVAL;
        goto READY_EXIT;
    }

    dev_info->dev_ready = DEVDRV_DEV_READY_EXIST;

#ifndef CFG_FEATURE_REFACTOR
    pdata = dev_info->pdata;
    if (pdata == NULL) {
        devdrv_drv_err("data is NULL\n");
        return -ENOMEM;
    }
    pdata->dev_id = dev_info->dev_id;
    pdata->ts_num = devdrv_get_ts_num();

    dev_info->ts_num = pdata->ts_num;
#endif
    drv_info = (struct devdrv_device_info *)dev_manager_msg_info->payload;
    if (drv_info->ts_load_fail == 0) {
        devdrv_drv_info("Load tsfw succeed, set device ready. (dev_id=%u)\n", dev_id);
    }

    ret = devdrv_manager_device_ready_info(dev_info, drv_info);
    if (ret != 0) {
        devdrv_drv_err("Failed to generate device ready info. (dev_id=%u)\n", dev_id);
        return -EINVAL;
    }
    dev_manager_msg_info->header.result = 0;

    if (dev_manager_info != NULL) {
        ka_task_queue_work(dev_manager_info->dev_rdy_work, &dev_info->work);
    }

    devdrv_manager_set_base_dev_info(dev_id, dev_info);

    devdrv_drv_info("Receive device ready notify. (dev_id=%u; dev_manager_info%s)\n", dev_id,
                    dev_manager_info != NULL ? "!=NULL" : "==NULL");

READY_EXIT:
    *ack_len = sizeof(*dev_manager_msg_info);
    return 0;
}

#if !defined(CFG_FEATURE_NOT_BOOT_INIT)
STATIC int devmng_alloc_dev_boot_argv(char **argv)
{
    int ret = 0;

    argv[0] = (char *)dbl_kzalloc((ka_base_strlen(DEVMNG_DEV_BOOT_INIT_SH) + 1), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (argv[0] == NULL) {
        devdrv_drv_err("kzalloc argv[0] fail !\n");
        return -ENOMEM;
    }

    argv[1] = (char *)dbl_kzalloc(DEVMNG_DEV_ID_LEN, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (argv[1] == NULL) {
        devdrv_drv_err("kzalloc argv[1] fail !\n");
        ret = -ENOMEM;
        goto kzalloc_argv1_fail;
    }

    argv[DEVMNG_DEV_BOOT_ARG_NUM - 1] = (char *)dbl_kzalloc(DEVMNG_DEV_ID_LEN, KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (argv[DEVMNG_DEV_BOOT_ARG_NUM - 1] == NULL) {
        devdrv_drv_err("kzalloc last argv fail !\n");
        ret = -ENOMEM;
        goto kzalloc_argv2_fail;
    }

    return 0;

kzalloc_argv2_fail:
    dbl_kfree(argv[1]);
kzalloc_argv1_fail:
    dbl_kfree(argv[0]);
    return ret;
}

STATIC void devmng_free_dev_boot_argv(char **argv)
{
    int i;

    for (i = 0; i < DEVMNG_DEV_BOOT_ARG_NUM; i++) {
        if (argv[i] != NULL) {
            dbl_kfree(argv[i]);
            argv[i] = NULL;
        }
    }

    return;
}

STATIC int devmng_call_dev_boot_init(u32 phy_id)
{
    int ret;
    u32 logic_id = UDA_INVALID_UDEVID;
    char *argv[DEVMNG_DEV_BOOT_ARG_NUM + 1] = {NULL};
    char *envp[] = {"HOME=/", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};

    ret = devmng_alloc_dev_boot_argv(argv);
    if (ret != 0) {
        devdrv_drv_err("Allocate memory for argument failed. (phy_id=%u; ret=%d)\n", phy_id, ret);
        return ret;
    }

    /* argument 0, for script path */
    ret = snprintf_s(argv[0], (ka_base_strlen(DEVMNG_DEV_BOOT_INIT_SH) + 1), ka_base_strlen(DEVMNG_DEV_BOOT_INIT_SH),
                     "%s", DEVMNG_DEV_BOOT_INIT_SH);
    if (ret < 0) {
        devdrv_drv_err("snprintf_s argv[0] fail. (ret=%d)\n", ret);
        ret = -EINVAL;
        goto free_argv;
    }

    /* argument 1, for udevice id */
    ret = snprintf_s(argv[1], DEVMNG_DEV_ID_LEN, DEVMNG_DEV_ID_LEN - 1, "%u", phy_id);
    if (ret < 0) {
        devdrv_drv_err("snprintf_s argv[1] fail. (ret=%d)\n", ret);
        ret = -EINVAL;
        goto free_argv;
    }

    /* argument 2, for logical id */
    ret = uda_udevid_to_logic_id(phy_id, &logic_id);
    if (ret != 0) {
        devdrv_drv_err("Udevice id to logical id failed. (ret=%d)\n", ret);
        goto free_argv;
    }

    ret = snprintf_s(argv[DEVMNG_DEV_BOOT_ARG_NUM - 1], DEVMNG_DEV_ID_LEN, DEVMNG_DEV_ID_LEN - 1, "%u", logic_id);
    if (ret < 0) {
        devdrv_drv_err("snprintf_s last argument fail. (ret=%d)\n", ret);
        ret = -EINVAL;
        goto free_argv;
    }

    devdrv_drv_info("Call boot init script. (phy_id=%u; logic_id=%u)\n", phy_id, logic_id);
    ret = ka_system_call_usermodehelper(DEVMNG_DEV_BOOT_INIT_SH, argv, envp, UMH_WAIT_EXEC);
    if (ret != 0) {
        devdrv_drv_err("ka_system_call_usermodehelper fail. (ret=%d)\n", ret);
    }

free_argv:
    devmng_free_dev_boot_argv(argv);
    return ret;
}
#endif

STATIC void devdrv_manager_dev_ready_work(ka_work_struct_t *work)
{
    struct devdrv_info *dev_info = NULL;
    u32 tsid = 0;
    u32 dev_id;
    u32 chip_type;
    int ret;

    dev_info = ka_container_of(work, struct devdrv_info, work);
    dev_id = dev_info->pci_dev_id;
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("invalid dev_id(%u)\n", dev_id);
        return;
    }

    devdrv_drv_info("Device ready work start. (dev_id=%u)\n", dev_id);
    if (devdrv_manager_register(dev_info)) {
        devdrv_drv_err("devdrv_manager_register failed. dev_id(%u)\n", dev_id);
        return;
    }

    tsdrv_set_ts_status(dev_info->dev_id, tsid, TS_WORK);
    dev_info->dev_ready = DEVDRV_DEV_READY_WORK;

    chip_type = uda_get_chip_type(dev_id);

#ifdef CFG_FEATURE_TIMESYNC
    dms_time_sync_init(dev_id);
#if !defined(CFG_FEATURE_ASCEND950_STUB)
    devdrv_refresh_aicore_info_init(dev_id);
#endif
#endif
    ka_task_up(&dev_info->no_trans_chan_wait_sema);
    dev_info->status = DEVINFO_STATUS_WORKING;
#ifdef ENABLE_BUILD_PRODUCT
    ret = dms_hotreset_task_init(dev_id);
    if (ret != 0) {
        devdrv_drv_err("Dms hotreset task init fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }
#endif
    /* not need to return error when call device_boot_init.sh fail */
#if defined(CFG_FEATURE_NOT_BOOT_INIT) /* 960 will enable this function later */
    devdrv_drv_info("Do not run devmng_call_dev_boot_init\n");
#else
    ret = devmng_call_dev_boot_init(dev_id);
    if (ret) {
        devdrv_drv_err("dev_id[%u] devmng_call_dev_boot_init fail, ret[%d]\n", dev_id, ret);
    }
#endif
#ifndef ENABLE_BUILD_PRODUCT
    ret = dms_hotreset_task_init(dev_id);
    if (ret != 0) {
        devdrv_drv_err("Dms hotreset task init fail. (dev_id=%u; ret=%d)\n", dev_id, ret);
    }
#endif

    ret = module_feature_auto_init_dev(dev_id);
    if (ret != 0) {
        devdrv_drv_ex_notsupport_err(ret, "Module auto init dev fail. (udevid=%u; ret=%d)\n", dev_id, ret);
    }
}

struct tsdrv_drv_ops *devdrv_manager_get_drv_ops(void)
{
    return &devdrv_host_drv_ops;
}
KA_EXPORT_SYMBOL(devdrv_manager_get_drv_ops);

struct devdrv_check_start_s {
    u32 dev_id;
    ka_timer_list_t check_timer;
};

STATIC struct devdrv_check_start_s devdrv_check_start[ASCEND_DEV_MAX_NUM];

STATIC void devdrv_check_start_event(ka_timer_list_t *t)
{
    struct devdrv_info *dev_info = NULL;
    ka_timespec_t stamp;
    struct devdrv_check_start_s *devdrv_start_check = ka_system_from_timer(devdrv_start_check, t, check_timer);
    u32 dev_id;
    u32 tsid = 0;

    dev_id = devdrv_start_check->dev_id;

    devdrv_drv_debug("Checking whether device is started. (dev_id=%u)\n", dev_id);

    stamp = ka_system_current_kernel_time();

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if (dev_info == NULL) {
        devdrv_drv_err("Device info array is not ready. (dev_id=%u)\n", dev_id);
        return;
    }

    if (dev_info->dev_ready < DEVDRV_DEV_READY_EXIST) {
        devdrv_drv_err("Device is not ready. (dev_id=%u; dev_ready=%d; info_dev_id=%u)\n", dev_id, dev_info->dev_ready,
                       dev_info->dev_id);
        (void)devdrv_host_black_box_add_exception(dev_info->dev_id, DEVDRV_BB_DEVICE_LOAD_TIMEOUT, stamp, NULL);
        tsdrv_set_ts_status(dev_info->dev_id, tsid, TS_DOWN);
        return;
    }

    devdrv_drv_debug("Device is started and working. (dev_id=%u)\n", dev_id);
}

#ifndef CFG_FEATURE_UB
STATIC int devdrv_manager_dev_state_notify(u32 probe_num, u32 devid, u32 state)
{
    struct devdrv_black_box_state_info *bbox_state_info = NULL;
    ka_timespec_t tstemp = {0};
    struct devdrv_info *dev_info = NULL;
    unsigned long flags;
    int ret;

    if ((dev_manager_info == NULL) || ((enum devdrv_device_state)state >= STATE_TO_MAX) ||
        (devid >= ASCEND_DEV_MAX_NUM)) {
        devdrv_drv_err("state notify para is invalid,"
                       "with dev_manager_info(%pK),"
                       "state(%d), devid(%u).\n",
                       dev_manager_info, (u32)state, devid);
        return -ENODEV;
    }

    ka_task_spin_lock_irqsave(&dev_manager_info->spinlock, flags);
    dev_manager_info->prob_num = probe_num;
    ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);

    dev_info = devdrv_get_devdrv_info_array(devid);
    if (dev_info == NULL) {
        devdrv_drv_warn("device is not ready.\n");
        return -ENODEV;
    }

    CLR_BIT_64(dev_manager_info->prob_device_bitmap[devid / KA_BITS_PER_LONG_LONG], devid % KA_BITS_PER_LONG_LONG);

    bbox_state_info = dbl_kzalloc(sizeof(struct devdrv_black_box_state_info), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (bbox_state_info == NULL) {
        devdrv_drv_err("malloc state_info failed. dev_id(%u)\n", devid);
        return -ENOMEM;
    }

    bbox_state_info->devId = devid;
    bbox_state_info->state = state;

    devdrv_drv_info("dev state notified with devid(%u), state(%d)", devid, state);

    ret = devdrv_host_black_box_add_exception(0, DEVDRV_BB_DEVICE_STATE_INFORM, tstemp, (void *)bbox_state_info);
    if (ret) {
        dbl_kfree(bbox_state_info);
        bbox_state_info = NULL;
        devdrv_drv_err("devdrv_host_black_box_add_exception failed. dev_id(%u)\n", devid);
        ka_system_ssleep(1); // add 1s for bbox to dump when unbind
        return -ENODEV;
    }

    dbl_kfree(bbox_state_info);
    bbox_state_info = NULL;
    ka_system_ssleep(1); // add 1s for bbox to dump when unbind

    return 0;
}

STATIC int devdrv_manager_dev_startup_notify(u32 prob_num, const u32 devids[], u32 array_len, u32 devnum)
{
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    struct devdrv_black_box_devids *bbox_devids = NULL;
    ka_timespec_t tstemp = {0};
    unsigned long flags;
    int ret;
    u32 i;

    (void)array_len;
    if ((dev_manager_info == NULL) || (devnum > ASCEND_DEV_MAX_NUM) || (devids == NULL)) {
        devdrv_drv_err("dev manager info is not initialized\n");
        return -ENODEV;
    }

    ka_task_spin_lock_irqsave(&dev_manager_info->spinlock, flags);
    dev_manager_info->prob_num = prob_num;
    for (i = 0; i < devnum; i++) {
        if (devdrv_manager_is_pf_device(devids[i]) || devdrv_manager_is_mdev_vm_mode(devids[i])) {
            SET_BIT_64(dev_manager_info->prob_device_bitmap[devids[i] / KA_BITS_PER_LONG_LONG],
                       devids[i] % KA_BITS_PER_LONG_LONG);
        }
    }
    ka_task_spin_unlock_irqrestore(&dev_manager_info->spinlock, flags);

    bbox_devids = dbl_kzalloc(sizeof(struct devdrv_black_box_devids), KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (bbox_devids == NULL) {
        devdrv_drv_err("malloc devids failed\n");
        return -ENOMEM;
    }

    bbox_devids->dev_num = devnum;
    for (i = 0; i < devnum; i++) {
        bbox_devids->devids[i] = devids[i];
    }

    ret = devdrv_host_black_box_add_exception(0, DEVDRV_BB_DEVICE_ID_INFORM, tstemp, (void *)bbox_devids);
    if (ret) {
        dbl_kfree(bbox_devids);
        bbox_devids = NULL;
        devdrv_drv_err("devdrv_host_black_box_add_exception failed, ret(%d).\n", ret);
        return -ENODEV;
    }
    dbl_kfree(bbox_devids);
    bbox_devids = NULL;
#endif
    return 0;
}
#endif

STATIC void devmng_basic_info_uninit(void)
{
    int devid;

    for (devid = 0; devid < ASCEND_DEV_MAX_NUM; devid++) {
        if (g_devdrv_board_info[devid] != NULL) {
            dbl_kfree(g_devdrv_board_info[devid]);
            g_devdrv_board_info[devid] = NULL;
        }
    }
}

STATIC struct devdrv_info *devdrv_manager_dev_info_alloc(u32 dev_id)
{
    struct devdrv_info *dev_info = NULL;
    int ret;

    dev_info = dbl_kzalloc(sizeof(*dev_info), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (dev_info == NULL) {
        devdrv_drv_err("kzalloc dev_info failed. dev_id(%u)\n", dev_id);
        return NULL;
    }

    ka_task_mutex_init(&dev_info->lock);

    dev_info->dev_id = dev_id;
    dev_info->pci_dev_id = dev_id;
    dev_info->cce_ops.cce_dev = NULL;

    ret = devmng_shm_init(dev_info);
    if (ret) {
        dbl_kfree(dev_info);
        dev_info = NULL;
        devdrv_drv_err("dev_id[%u] devmng_shm_init fail, ret[%d]\n", dev_id, ret);
        return NULL;
    }

    return dev_info;
}

struct devdrv_board_info_cache *devdrv_get_board_info_host(unsigned int dev_id)
{
    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Invalid basic_info parameter. (phy_id=%u; dev_maxnum=%d)\n", dev_id, ASCEND_DEV_MAX_NUM);
        return NULL;
    }

    return g_devdrv_board_info[dev_id];
}

#ifdef CFG_FEATURE_REFACTOR
STATIC int devdrv_manager_init_board_hw_info(struct devdrv_info *dev_info)
{
    int ret = 0;
#ifdef CFG_FEATURE_BIOS_HW_INFO_BY_SOC_RES
    soc_res_board_hw_info_t hw_info = {0};
    soc_res_board_hw_info_ex_t hw_info_ex = {0};
    ret = soc_resmng_dev_get_attr(dev_info->dev_id, BOARD_HW_INFO, &hw_info, sizeof(hw_info));
    if (ret != 0) {
        devdrv_drv_err("Get board hardware info from soc res failed. (dev_id=%u; ret=%d)\n", dev_info->dev_id, ret);
        return ret;
    }
    dev_info->chip_id = hw_info.chip_id;
    dev_info->multi_chip = hw_info.multi_chip;
    dev_info->multi_die = hw_info.multi_die;
    dev_info->mainboard_id = hw_info.mainboard_id;
    dev_info->addr_mode = hw_info.addr_mode;
    dev_info->connect_type = hw_info.inter_connect_type;
    dev_info->board_id = hw_info.board_id;
    dev_info->server_id = hw_info.server_id;
    dev_info->scale_type = hw_info.scale_type;
    dev_info->super_pod_id = hw_info.super_pod_id;
    dev_info->chassis_id = hw_info.chassis_id;
    dev_info->super_pod_type = hw_info.super_pod_type;
    dev_info->super_pod_intercon_type = hw_info.super_pod_intercon_type;

    ret = soc_resmng_dev_get_attr(dev_info->dev_id, BOARD_HW_INFO_EX, &hw_info_ex, sizeof(hw_info_ex));
    if (ret != 0) {
        devdrv_drv_warn("Get board hardware info ex fail, groupid set invaild value. (dev_id=%u; ret=%d)\n",
                        dev_info->dev_id, ret);
        dev_info->scale_up_group_id = 0xffff;
        dev_info->scale_out_group_id = 0xffff;
    } else {
        dev_info->scale_up_group_id = hw_info_ex.scale_up_group_id;
        dev_info->scale_out_group_id = hw_info_ex.scale_out_group_id;
    }

    devdrv_drv_info(
        "Get hardware info success."
        "(chip_id=%u; multi_chip=%u; multi_die=%u; mainboard_id=0x%x; "
        "addr_mode=%u; inter_connect_type=%u; board_id=0x%x;"
        "server_id=%u; scale_type=%u; super_pod_id=%u; chassis_id=%u; super_pod_type=%u; super_pod_intercon_type=%u;"
        "scale_up_group_id=%u; scale_out_group_id=%u)\n",
        dev_info->chip_id, dev_info->multi_chip, dev_info->multi_die, dev_info->mainboard_id, dev_info->addr_mode,
        dev_info->connect_type, dev_info->board_id, dev_info->server_id, dev_info->scale_type, dev_info->super_pod_id,
        dev_info->chassis_id, dev_info->super_pod_type, dev_info->super_pod_intercon_type, dev_info->scale_up_group_id,
        dev_info->scale_out_group_id);
#endif
    return ret;
}

#ifdef CFG_FEATURE_PRODUCT_TYPE_BY_SOC_RES
STATIC int devdrv_manager_init_product_type_by_soc_res(struct devdrv_info *dev_info)
{
    u64 product_type = 0;
    int ret;

    ret = soc_resmng_dev_get_key_value(dev_info->dev_id, "PRODUCT_TYPE", &product_type);
    if (ret != 0) {
        devdrv_drv_err("Get board type failed. (dev_id=%u)\n", dev_info->dev_id);
        return ret;
    }
    dev_info->product_type = (u8)product_type;
    devdrv_drv_info("Get product type success. (dev_id=%u; product_type=0x%x)\n", dev_info->dev_id,
                    dev_info->product_type);
    return 0;
}
#endif

STATIC int devdrv_manager_init_cpu_info(u32 udevid, struct devdrv_info *dev_info)
{
    int ret;
    struct soc_mia_res_info_ex info = {0};

    ret = soc_resmng_dev_get_mia_res_ex(udevid, MIA_CPU_DEV_CCPU, &info);
    if (ret != 0) {
        devdrv_drv_err("Get ccpu info failed. (dev_id=%u; ret=%d)\n", udevid, ret);
        return ret;
    }
    dev_info->ctrl_cpu_core_num = info.total_num;
    dev_info->ctrl_cpu_id = __ka_base_ffs(info.bitmap);
    dev_info->ctrl_cpu_occupy_bitmap = info.bitmap;
#if defined(__KA_LITTLE_ENDIAN)
    dev_info->ctrl_cpu_endian_little = 1;
#elif defined(__KA_BIG_ENDIAN)
    dev_info->ctrl_cpu_endian_little = 0;
#endif

    ret = soc_resmng_dev_get_mia_res_ex(udevid, MIA_CPU_DEV_ACPU, &info);
    if (ret != 0) {
        devdrv_drv_err("Get acpu info failed. (dev_id=%u; ret=%d)\n", udevid, ret);
        return ret;
    }
    dev_info->ai_cpu_core_id = __ka_base_ffs(info.bitmap);
    dev_info->ai_cpu_core_num = info.total_num;
    dev_info->aicpu_occupy_bitmap = info.bitmap;

    devdrv_drv_info(
        "(devid=%u; ctrl_cpu_core_num=0x%x; ctrl_cpu_id=0x%x; ctrl_cpu_occupy_bitmap=0x%llx;"
        "ctrl_cpu_endian_little=0x%x; ai_cpu_core_id=0x%x; ai_cpu_core_num=0x%x; aicpu_occupy_bitmap=0x%llx;)\n",
        udevid, dev_info->ctrl_cpu_core_num, dev_info->ctrl_cpu_id, dev_info->ctrl_cpu_occupy_bitmap,
        dev_info->ctrl_cpu_endian_little, dev_info->ai_cpu_core_id, dev_info->ai_cpu_core_num,
        dev_info->aicpu_occupy_bitmap);

    return 0;
}

STATIC int devdrv_manager_init_l2_res_info(struct devdrv_info *dev_info, u64 die_num)
{
    u64 die_id;
    int ret = -EINVAL;
    u32 dev_id = dev_info->dev_id;
    struct soc_mia_res_info_ex info = {0};

    dev_info->l2_num = 0;
    for (die_id = 0; die_id < die_num; die_id++) {
        ret = soc_resmng_dev_die_get_res(dev_id, (u32)die_id, MIA_SYS_l2, &info);
        if (ret != 0) {
            devdrv_drv_err("Get l2 info failed. (devid=%u; die_id=0x%llx; ret=%d)\n", dev_id, die_id, ret);
            return ret;
        }
        dev_info->l2_num += info.total_num;
    }

    return 0;
}

STATIC int devdrv_manager_init_hbm_res_info(struct devdrv_info *dev_info, u64 die_num)
{
    u64 die_id;
    int ret = -EINVAL;
    u32 dev_id = dev_info->dev_id;
    struct soc_mia_res_info_ex info = {0};

    dev_info->hbm_num = 0;
    for (die_id = 0; die_id < die_num; die_id++) {
        ret = soc_resmng_dev_die_get_res(dev_id, (u32)die_id, MIA_MEM_HBM, &info);
        if (ret != 0) {
            devdrv_drv_err("Get hbm info failed. (devid=%u; die_id=0x%llx; ret=%d)\n", dev_id, die_id, ret);
            return ret;
        }
        dev_info->hbm_num += info.total_num;
    }

    return 0;
}

STATIC u64 devdrv_manager_repeat_bits(u64 bitmap, u32 unit)
{
    int i;
    u64 result = 0;
    u64 temp_bitmap = bitmap;
    u32 n = ka_base_fls64(bitmap);
    for (i = 0; i < unit; i++) {
        result |= temp_bitmap;
        if (i < unit - 1) {
            temp_bitmap <<= n;
        }
    }

    return result;
}

STATIC int devdrv_manager_init_aiv_res_info(struct devdrv_info *dev_info, u64 die_num)
{
    u64 die_id;
    u32 die1_offset;
    int ret = -EINVAL;
    u64 max_aiv_num_per_die = 0;
    u32 dev_id = dev_info->dev_id;
    struct soc_mia_res_info_ex info = {0};

    ret = soc_resmng_dev_get_key_value(dev_id, "soc_max_aiv_num_per_die", &max_aiv_num_per_die);
    if (ret != 0) {
        devdrv_drv_err("Get max aiv num per die failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (max_aiv_num_per_die > 64U) {
        devdrv_drv_err("Get max aiv num per die over 64. (devid=%u; max_aiv_num_per_die=0x%llx)\n", dev_id,
                       max_aiv_num_per_die);
        return -EINVAL;
    }

    die1_offset = 64U - max_aiv_num_per_die;
    dev_info->vector_core_num = 0;
    dev_info->vector_core_bitmap = 0;
    dev_info->vector_core_bitmap_h = 0;

    for (die_id = 0; die_id < die_num; die_id++) {
        ret = soc_resmng_dev_die_get_res(dev_id, (u32)die_id, MIA_AC_AIV, &info);
        if (ret != 0) {
            devdrv_drv_err("Get aiv info failed. (devid=%u; die_id=0x%llx; ret=%d)\n", dev_id, die_id, ret);
            return ret;
        }

        dev_info->vector_core_num += info.total_num;
        dev_info->vector_core_freq = info.freq;
        dev_info->vector_core_bitmap |= devdrv_manager_repeat_bits(info.bitmap, info.unit_per_bit)
                                        << (die_id * max_aiv_num_per_die);
        if (die_id != 0) {
            dev_info->vector_core_bitmap_h |= devdrv_manager_repeat_bits(info.bitmap, info.unit_per_bit) >>
                                              (die_id * die1_offset);
        }
    }

    return 0;
}

STATIC int devdrv_manager_init_aic_res_info(struct devdrv_info *dev_info, u64 die_num, u32 pf_flag)
{
    u64 die_id;
    int ret = -EINVAL;
    u64 max_aic_num_per_die = 0;
    u32 dev_id = dev_info->dev_id;
    struct soc_mia_res_info_ex info = {0};

    ret = soc_resmng_dev_get_key_value(dev_id, "soc_max_aic_num_per_die", &max_aic_num_per_die);
    if (ret != 0) {
        devdrv_drv_err("Get max aic num per die failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    dev_info->ai_core_num = 0;
    dev_info->aicore_bitmap = 0;
    for (die_id = 0; die_id < die_num; die_id++) {
        ret = soc_resmng_dev_die_get_res(dev_id, (u32)die_id, MIA_AC_AIC, &info);
        if (ret != 0) {
            devdrv_drv_err("Get aic info failed. (devid=%u; die_id=0x%llx; ret=%d)\n", dev_id, die_id, ret);
            return ret;
        }

        if ((die_id * max_aic_num_per_die) >= 64U) {
            devdrv_drv_err("Max aic num per die invalid. (devid=%u; die_id=0x%llx; max_aic_num_per_die=0x%llx)\n",
                           dev_id, die_id, max_aic_num_per_die);
            return -EINVAL;
        }

        dev_info->ai_core_num += info.total_num;
        dev_info->aicore_bitmap |= info.bitmap << (die_id * max_aic_num_per_die);
        if (pf_flag == DEVMNG_PF_DEV) {
            dev_info->ai_core_id = 0;
            dev_info->aicore_freq = info.freq;
        }
    }

    return 0;
}

STATIC int devdrv_manager_init_res_info(struct devdrv_info *dev_info)
{
    int ret;
    u64 die_num;
    u32 dev_id = dev_info->dev_id;

    ret = soc_resmng_dev_get_key_value(dev_id, "soc_die_num", &die_num);
    if (ret != 0) {
        devdrv_drv_err("Get soc_die_num failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = devdrv_manager_init_aic_res_info(dev_info, die_num, DEVMNG_PF_DEV);
    if (ret != 0) {
        devdrv_drv_err("Init pf aic info failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = devdrv_manager_init_aiv_res_info(dev_info, die_num);
    if (ret != 0) {
        devdrv_drv_err("Init pf aiv info failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = devdrv_manager_init_l2_res_info(dev_info, die_num);
    if (ret != 0) {
        devdrv_drv_err("Init pf l2 info failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = devdrv_manager_init_hbm_res_info(dev_info, die_num);
    if (ret != 0) {
        devdrv_drv_err("Init pf hbm info failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = soc_resmng_subsys_get_num(dev_id, TS_SUBSYS, &dev_info->ts_num);
    if (ret != 0) {
        devdrv_drv_err("Get ts num failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = soc_resmng_dev_get_attr(dev_id, SOC_VERSION, dev_info->soc_version, SOC_VERSION_LEN);
    if (ret != 0) {
        devdrv_drv_err("Get soc version failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = devdrv_manager_init_cpu_info(dev_id, dev_info);
    if (ret != 0) {
        return ret;
    }

    return ret;
}

STATIC int devdrv_manager_clear_cpu_cfg(u32 udevid)
{
    int ret;

    ret = soc_resmng_dev_set_mia_res(udevid, MIA_CPU_DEV_CCPU, 0, 1);
    if (ret != 0) {
        devdrv_drv_err("Set ctrl cpu num to 0 failed. (dev_id=%u; ret=%d)\n", udevid, ret);
    }

    ret = soc_resmng_dev_set_mia_res(udevid, MIA_CPU_DEV_DCPU, 0, 1);
    if (ret != 0) {
        devdrv_drv_err("Set data cpu num to 0 failed. (dev_id=%u; ret=%d)\n", udevid, ret);
    }

    ret = soc_resmng_dev_set_mia_res(udevid, MIA_CPU_DEV_ACPU, 0, 1);
    if (ret != 0) {
        devdrv_drv_err("Set aicpu num to 0 failed. (dev_id=%u; ret=%d)\n", udevid, ret);
    }

    ret = soc_resmng_dev_set_mia_res(udevid, MIA_CPU_DEV_COMCPU, 0, 1);
    if (ret != 0) {
        devdrv_drv_err("Set communication cpu num to 0 failed. (dev_id=%u; ret=%d)\n", udevid, ret);
    }

    return 0;
}

STATIC int devdrv_manager_init_devinfo(struct devdrv_info *dev_info)
{
    int ret;

    ret = devdrv_manager_init_res_info(dev_info);
    if (ret != 0) {
        return ret;
    }

    devdrv_drv_info(
        "(devid=%u; aicore_num=0x%x; ai_core_id=0x%x; aicore_bitmap=0x%llx; aicore_freq=0x%llx;"
        "vector_core_num=0x%x; vector_core_bitmap=0x%llx; vector_core_bitmap_h=0x%llx; vector_core_freq=0x%llx;"
        " ts_num=0x%x; soc_version=%s)\n",
        dev_info->dev_id, dev_info->ai_core_num, dev_info->ai_core_id, dev_info->aicore_bitmap, dev_info->aicore_freq,
        dev_info->vector_core_num, dev_info->vector_core_bitmap, dev_info->vector_core_bitmap_h,
        dev_info->vector_core_freq, dev_info->ts_num, dev_info->soc_version);

#ifdef CFG_FEATURE_BIOS_HW_INFO_BY_SOC_RES
    ret = devdrv_manager_init_board_hw_info(dev_info);
    if (ret != 0) {
        devdrv_drv_err("Get board hardware info fail. (ret=%d)\n", ret);
        return ret;
    }
#endif

#ifdef CFG_FEATURE_BIOS_HW_INFO_BY_SOC_RES
    ret = devdrv_manager_init_product_type_by_soc_res(dev_info);
    if (ret != 0) {
        devdrv_drv_err("Get board type fail. (ret=%d)\n", ret);
        return ret;
    }
#endif
    return 0;
}

STATIC int devdrv_manager_init_vf_splited_res(struct devdrv_info *dev_info_vf)
{
    u64 bitmap;
    u32 unit_per_bit;
    u64 die_num;
    u32 dev_id = dev_info_vf->dev_id;
    int ret;

    ret = soc_resmng_dev_get_key_value(dev_id, "soc_die_num", &die_num);
    if (ret != 0) {
        devdrv_drv_err("Get soc_die_num failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = devdrv_manager_init_aic_res_info(dev_info_vf, die_num, DEVMNG_VF_DEV);
    if (ret != 0) {
        devdrv_drv_err("Init vf aic info failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = devdrv_manager_init_aiv_res_info(dev_info_vf, die_num);
    if (ret != 0) {
        devdrv_drv_err("Init vf aiv info failed. (devid=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = soc_resmng_dev_get_mia_res(dev_id, MIA_CPU_DEV_ACPU, &bitmap, &unit_per_bit);
    if (ret != 0) {
        devdrv_drv_err("Get vf acpu info failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }
    dev_info_vf->ai_cpu_core_id = __ka_base_ffs(bitmap);
    dev_info_vf->ai_cpu_core_num = (ka_base_bitmap_weight((unsigned long *)&bitmap, KA_BITS_PER_LONG_LONG)) *
                                   unit_per_bit;
    dev_info_vf->aicpu_occupy_bitmap = bitmap;

    return 0;
}

STATIC int devdrv_manager_init_vf_res_info(struct devdrv_info *dev_info_pf, struct devdrv_info *dev_info_vf)
{
    int ret;

    ret = devdrv_manager_init_vf_splited_res(dev_info_vf);
    if (ret != 0) {
        devdrv_drv_err("Init vf split res failed. (dev_id=%u; ret=%d)\n", dev_info_vf->dev_id, ret);
        return ret;
    }

    dev_info_vf->ai_core_id = dev_info_pf->ai_core_id;
    dev_info_vf->aicore_freq = dev_info_pf->aicore_freq;
    dev_info_vf->vector_core_freq = dev_info_pf->vector_core_freq;

    dev_info_vf->ctrl_cpu_core_num = dev_info_pf->ctrl_cpu_core_num;
    dev_info_vf->ctrl_cpu_id = dev_info_pf->ctrl_cpu_id;
    dev_info_vf->ctrl_cpu_occupy_bitmap = dev_info_pf->ctrl_cpu_occupy_bitmap;
    dev_info_vf->ctrl_cpu_endian_little = dev_info_pf->ctrl_cpu_endian_little;

    dev_info_vf->ts_num = dev_info_pf->ts_num;

    ret = strncpy_s(dev_info_vf->soc_version, SOC_VERSION_LENGTH, dev_info_pf->soc_version, SOC_VERSION_LEN - 1);
    if (ret != 0) {
        devdrv_drv_err("Copy soc version failed. (dev_id=%u; ret=%d)\n", dev_info_vf->dev_id, ret);
        return -EINVAL;
    }

    devdrv_drv_info(
        "(devid=%u; aicore_num=0x%x; ai_core_id=0x%x; aicore_bitmap=0x%llx; aicore_freq=0x%llx;"
        "vector_core_num=0x%x; vector_core_freq=0x%llx; vector_core_bitmap=0x%llx; vector_core_bitmap_h=0x%llx;"
        "ts_num=0x%x; soc_version=%s; ctrl_cpu_core_num=0x%x; ctrl_cpu_id=0x%x; ctrl_cpu_occupy_bitmap=0x%llx;"
        "ctrl_cpu_endian_little=0x%x; ai_cpu_core_id=0x%x; ai_cpu_core_num=0x%x; aicpu_occupy_bitmap=0x%llx;)\n",
        dev_info_vf->dev_id, dev_info_vf->ai_core_num, dev_info_vf->ai_core_id, dev_info_vf->aicore_bitmap,
        dev_info_vf->aicore_freq, dev_info_vf->vector_core_num, dev_info_vf->vector_core_freq,
        dev_info_vf->vector_core_bitmap, dev_info_vf->vector_core_bitmap_h, dev_info_vf->ts_num,
        dev_info_vf->soc_version, dev_info_vf->ctrl_cpu_core_num, dev_info_vf->ctrl_cpu_id,
        dev_info_vf->ctrl_cpu_occupy_bitmap, dev_info_vf->ctrl_cpu_endian_little, dev_info_vf->ai_cpu_core_id,
        dev_info_vf->ai_cpu_core_num, dev_info_vf->aicpu_occupy_bitmap);
    return 0;
}

STATIC int devdrv_manager_init_vf_devinfo(struct devdrv_info *dev_info_vf)
{
    int ret;
    u32 vf_udevid, pf_id, vf_id;
    struct devdrv_info *dev_info_pf;

    vf_udevid = dev_info_vf->dev_id;
    ret = devdrv_manager_get_pf_vf_id(vf_udevid, &pf_id, &vf_id);
    if (ret != 0) {
        return ret;
    }
    devdrv_drv_info("Get pf id success. (vf_udevid=%u; pf_id=%u)\n", vf_udevid, pf_id);

    dev_info_pf = devdrv_manager_get_devdrv_info(pf_id);
    if (dev_info_pf == NULL) {
        devdrv_drv_err("Get pf dev_info failed. (dev_id=%u)\n", pf_id);
        return -EINVAL;
    }

    ret = devdrv_manager_init_vf_res_info(dev_info_pf, dev_info_vf);
    if (ret != 0) {
        devdrv_drv_err("Init vf split res failed. (dev_id=%u; ret=%d)\n", vf_udevid, ret);
        return ret;
    }

    ret = devdrv_manager_init_board_hw_info(dev_info_vf);
    if (ret != 0) {
        devdrv_drv_err("Get board hardware info failed. (ret=%d)\n", ret);
        return ret;
    }

#ifdef CFG_FEATURE_BIOS_HW_INFO_BY_SOC_RES
    ret = devdrv_manager_init_product_type_by_soc_res(dev_info_vf);
    if (ret != 0) {
        devdrv_drv_err("Get board type failed. (ret=%d)\n", ret);
        return ret;
    }
#endif

    return 0;
}
#endif

#ifndef DEVDRV_MANAGER_HOST_UT_TEST
STATIC int devdrv_manager_init_instance(u32 dev_id, ka_device_t *dev)
{
#ifndef CFG_FEATURE_REFACTOR
    struct devdrv_platform_data *pdata = NULL;
#endif
    struct devdrv_info *dev_info = NULL;
    u32 tsid = 0;
    int ret;
    u32 init_flag = 1;
#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
    dms_release_one_device_remote_event(dev_id);
#endif
    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if (dev_info != NULL) {
        init_flag = 0;
        devmng_shm_init(dev_info);
        devdrv_drv_info("dev_id(%u) repeat init instance.\n", dev_id);
    } else {
        dev_info = devdrv_manager_dev_info_alloc(dev_id);
        if (dev_info == NULL) {
            devdrv_drv_err("dev_id(%u) alloc info mem fail.\n", dev_id);
            return -ENOMEM;
        }
    }

    ka_task_sema_init(&dev_info->no_trans_chan_wait_sema, 0);
    dev_info->dev_ready = 0;
    dev_info->driver_flag = 0;
    dev_info->dev = dev;
    dev_info->capability = 0;
    dev_info->dmp_started = false;
#ifdef CFG_FEATURE_REFACTOR
    if (uda_is_pf_dev(dev_id) == true) {
        ret = devdrv_manager_init_devinfo(dev_info);
    } else {
        ret = devdrv_manager_init_vf_devinfo(dev_info);
    }
    if (ret != 0) {
        devdrv_drv_err("Init devinfo failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        goto kfree_info;
    }
#endif
#ifdef CFG_FEATURE_DEVICE_SHARE
    set_device_share_flag(dev_id, DEVICE_UNSHARE);
#endif
    tsdrv_set_ts_status(dev_info->dev_id, tsid, TS_INITING);
    devdrv_drv_debug("*** set status initing device id :%u***\n", dev_id);

#ifndef CFG_FEATURE_REFACTOR
    if (init_flag == 1) {
        pdata = dbl_kzalloc(sizeof(struct devdrv_platform_data), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
        if (pdata == NULL) {
            ret = -ENOMEM;
            goto kfree_info;
        }

        /* host has one ts in any scenes */
        pdata->ts_num = 1;
    } else {
        pdata = dev_info->pdata;
    }
#endif

    if (init_flag == 1) {
        devdrv_set_devdrv_info_array(dev_id, dev_info);

        dev_info->plat_type = (u8)DEVDRV_MANAGER_HOST_ENV;
#ifndef CFG_FEATURE_REFACTOR
        dev_info->pdata = pdata;
#endif
        ka_task_spin_lock_init(&dev_info->spinlock);
    }
    devdrv_manager_set_no_trans_chan(dev_id, NULL);
    ret = devdrv_manager_none_trans_init(dev_id);
    if (ret != 0) {
        goto release_one_device;
    }

    KA_TASK_INIT_WORK(&dev_info->work, devdrv_manager_dev_ready_work);
    ret = devdrv_manager_init_common_chan(dev_id);
    if (ret != 0) {
        devdrv_drv_err("common chan init failed. (dev_id=%u)\n", dev_id);
        goto uninit_non_trans_chan;
    }

    /* init a timer for check whether device manager is ready */
    devdrv_check_start[dev_id].dev_id = dev_id;
    ka_system_timer_setup(&devdrv_check_start[dev_id].check_timer, devdrv_check_start_event, 0);
    devdrv_check_start[dev_id].check_timer.expires = ka_jiffies + LOAD_DEVICE_TIME * KA_HZ;
    ka_system_add_timer(&devdrv_check_start[dev_id].check_timer);

    /* device online inform user */
    devdrv_manager_online_devid_update(dev_id);
    dbl_dev_base_info_init(dev_id);
    devdrv_drv_info("devdrv_manager_init_instance dev_id :%u OUT !\n", dev_id);
    return 0;

uninit_non_trans_chan:
    devdrv_manager_non_trans_uninit(dev_id);
release_one_device:
#ifndef CFG_FEATURE_REFACTOR
    dbl_kfree(pdata);
    pdata = NULL;
#endif
kfree_info:
    devmng_shm_uninit(dev_info);
    ka_task_mutex_destroy(&dev_info->lock);
    dbl_kfree(dev_info);
    dev_info = NULL;
    return ret;
}
#endif

STATIC int devdrv_manager_uninit_instance(u32 dev_id)
{
    struct devdrv_common_msg_client *devdrv_commn_chan = NULL;
    struct devdrv_info *dev_info = NULL;
    int ret;
    u32 retry_cnt = 0;
#ifdef CFG_FEATURE_GET_CURRENT_EVENTINFO
    dms_release_one_device_remote_event(dev_id);
#endif
    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (dev_info == NULL)) {
        devdrv_drv_err("get sema timeout,the ready of device is not ok, dev_id:%u. dev_info = %pK.\n", dev_id,
                       dev_info);
        return -EINVAL;
    }

    ka_task_mutex_lock(&dev_info->lock);
    dev_info->status = DEVINFO_STATUS_REMOVED;
    ka_task_mutex_unlock(&dev_info->lock);
    dev_info->dmp_started = false;
    while (retry_cnt < WAIT_PROCESS_EXIT_TIME) {
        if (ka_base_atomic_read(&dev_info->occupy_ref) == 0) {
            break;
        }
        retry_cnt++;
        ka_system_msleep(1);
    }
    devmng_shm_uninit(dev_info);
    devdrv_manager_device_status_init(dev_id);

    ret = ka_task_down_timeout(&dev_info->no_trans_chan_wait_sema, DEVDRV_INIT_INSTANCE_TIMEOUT);
    if (ret) {
        devdrv_drv_err("devid %d get sema from init instance timeout, ret:%d\n", dev_id, ret);
    }
    ka_task_cancel_work_sync(&dev_info->work);
    dev_info->work.func = NULL;
    devdrv_drv_info("dev_id(%u) wait ioctrl retry_cnt %d.\n", dev_id, retry_cnt);

    ka_system_del_timer_sync(&devdrv_check_start[dev_id].check_timer);

#ifdef CFG_FEATURE_TIMESYNC
    dms_time_sync_exit(dev_id);
#if !defined(CFG_FEATURE_ASCEND950_STUB)
    devdrv_refresh_aicore_info_exit(dev_id);
#endif
#endif

    /* uninit common channel */
    devdrv_commn_chan = devdrv_manager_get_common_chan(dev_id);
    devdrv_unregister_common_msg_client(dev_id, devdrv_commn_chan);

    /* uninit non transparent channel */
    devdrv_manager_non_trans_uninit(dev_id);
    devdrv_manager_unregister(dev_info);
    devdrv_manager_online_del_devids(dev_id);
    dbl_dev_base_info_init(dev_id);
#ifdef CFG_FEATURE_REFACTOR
    devdrv_manager_clear_cpu_cfg(dev_id);
#endif

    if (!devdrv_manager_is_pf_device(dev_id)) {
        /* VF device need to destroy all device info when destroy vdevice */
#ifdef CFG_FEATURE_SRIOV
        dms_hotreset_vf_task_exit(dev_id);
#endif
        dev_manager_info->device_status[dev_id] = DRV_STATUS_INITING;
    }
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    module_feature_auto_uninit_dev(dev_id);
#endif
    return 0;
}

void devdrv_manager_uninit_one_device_info(unsigned int dev_id)
{
    struct devdrv_info *dev_info = NULL;

    dev_info = devdrv_get_devdrv_info_array(dev_id);
    if (dev_info == NULL) {
        return;
    }

    devdrv_set_devdrv_info_array(dev_id, NULL);
#ifndef CFG_FEATURE_REFACTOR
    dbl_kfree(dev_info->pdata);
    dev_info->pdata = NULL;
#endif
    ka_task_mutex_destroy(&dev_info->lock);
    dbl_kfree(dev_info);
    dev_info = NULL;
}

void devdrv_manager_uninit_devinfo(void)
{
    u32 i;

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        devdrv_manager_uninit_one_device_info(i);
    }
}

#define DEVMNG_HOST_NOTIFIER "mng_host"
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
static int devdrv_manager_host_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= ASCEND_DEV_MAX_NUM) {
        devdrv_drv_err("Invalid para. (udevid=%u; max=%u)\n", udevid, ASCEND_DEV_MAX_NUM);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        ret = devdrv_manager_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = devdrv_manager_uninit_instance(udevid);
    } else if (action == UDA_HOTRESET) {
        ret = dms_notify_device_hotreset(udevid);
    } else if (action == UDA_HOTRESET_CANCEL) {
        dms_notify_single_device_cancel_hotreset(udevid);
    } else if (action == UDA_PRE_HOTRESET) {
        ret = dms_notify_pre_device_hotreset(udevid);
    } else if (action == UDA_PRE_HOTRESET_CANCEL) {
        dms_notify_single_device_cancel_hotreset(udevid);
    }

    devdrv_drv_debug("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}
#endif

void devdrv_manager_ops_sem_down_write(void)
{
    ka_task_down_write(&devdrv_ops_sem);
}
KA_EXPORT_SYMBOL(devdrv_manager_ops_sem_down_write);

void devdrv_manager_ops_sem_up_write(void)
{
    ka_task_up_write(&devdrv_ops_sem);
}
KA_EXPORT_SYMBOL(devdrv_manager_ops_sem_up_write);

void devdrv_manager_ops_sem_down_read(void)
{
    ka_task_down_read(&devdrv_ops_sem);
}
KA_EXPORT_SYMBOL(devdrv_manager_ops_sem_down_read);

void devdrv_manager_ops_sem_up_read(void)
{
    ka_task_up_read(&devdrv_ops_sem);
}
KA_EXPORT_SYMBOL(devdrv_manager_ops_sem_up_read);

STATIC int devdrv_manager_reboot_handle(ka_notifier_block_t *self, unsigned long event, void *data)
{
#ifdef CFG_FEATURE_TIMESYNC
    int count = 0;
#endif

    if (event != KA_SYS_RESTART && event != KA_SYS_HALT && event != KA_SYS_POWER_OFF) {
        return KA_NOTIFY_DONE;
    }
#ifdef CFG_FEATURE_TIMESYNC
    dms_time_sync_reboot_handle();
    ka_mb();
    while (dms_is_sync_timezone()) {
        if (++count > DMS_TIMEZONE_MAX_COUNT) {
            devdrv_drv_err("wait localtime sync over 6 seconds.\n");
            return KA_NOTIFY_BAD;
        }
        ka_system_msleep(DMS_TIMEZONE_SLEEP_MS);
    }
#endif

#ifdef CFG_FEATURE_NOTIFY_REBOOT
    devdrv_notify_all_dev_reboot();
#endif

    devdrv_drv_info("System reboot now.....\n");
    return KA_NOTIFY_OK;
}

#ifndef CFG_FEATURE_APM_SUPP_PID
#if (!defined(DEVMNG_UT)) && (!defined(DEVDRV_MANAGER_HOST_UT_TEST))
void devdrv_check_pid_map_process_sign(ka_pid_t tgid, u64 start_time)
{
    struct devdrv_manager_info *d_info = devdrv_get_manager_info();
    struct devdrv_process_sign *d_sign_hostpid = NULL, *d_sign_devpid = NULL;
    ka_hlist_node_t *local_sign = NULL;
    struct devdrv_process_sign *free_sign = NULL, *free_sign_tmp = NULL;
    u32 bkt;
    int release_flag = 0;
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    ka_list_head_t free_list_head;
    KA_INIT_LIST_HEAD(&free_list_head);

    if (d_info == NULL) {
        devdrv_drv_err("dev_manager_info is NULL. (devpid=%d)\n", tgid);
        return;
    }
    /* for host side */
    ka_task_mutex_lock(&d_info->devdrv_sign_list_lock);
    if (!ka_list_empty_careful(&d_info->hostpid_list_header)) {
        ka_list_for_each_safe(pos, n, &d_info->hostpid_list_header)
        {
            d_sign_hostpid = ka_list_entry(pos, struct devdrv_process_sign, list);
            if (d_sign_hostpid->hostpid == tgid && d_sign_hostpid->hostpid_start_time != start_time) {
                devdrv_drv_debug("Delete sign list node. (dsign_hostpid=%d; tgid=%d; dsign_time=%llu; cur_time=%llu)\n",
                                 d_sign_hostpid->hostpid, tgid, d_sign_hostpid->hostpid_start_time, start_time);
                ka_list_del(&d_sign_hostpid->list);
                d_info->devdrv_sign_count[d_sign_hostpid->docker_id]--;
                dbl_vfree(d_sign_hostpid);
                d_sign_hostpid = NULL;
                break;
            }
        }
    }
    ka_task_mutex_unlock(&d_info->devdrv_sign_list_lock);

    ka_task_spin_lock_bh(&d_info->proc_hash_table_lock);
    ka_hash_for_each_safe(d_info->proc_hash_table, bkt, local_sign, d_sign_devpid, link)
    {
        /* release devpid if match */
        devdrv_release_pid_with_start_time(d_sign_devpid, tgid, start_time, &free_list_head, &release_flag);
    }
    ka_task_spin_unlock_bh(&d_info->proc_hash_table_lock);

    if (release_flag == 1) {
        devdrv_release_try_to_sync_to_peer(tgid);
        devdrv_drv_debug("Sync to_peer and release slave pid. (devpid=%d; start_time=%llu)\n", tgid, start_time);
    }

    ka_list_for_each_entry_safe(free_sign, free_sign_tmp, &free_list_head, list)
    {
        ka_list_del(&free_sign->list);
        devdrv_drv_info("Destroy master pid ctx when proc exit. (hostpid=%d; devpid=%d)", free_sign->hostpid, tgid);
        dbl_vfree(free_sign);
        free_sign = NULL;
    }
    return;
}
#endif
#endif

STATIC int devdrv_manage_release_prepare(ka_file_t *file_op, unsigned long mode)
{
    struct devdrv_manager_context *dev_manager_context = NULL;

    if (mode != NOTIFY_MODE_RELEASE_PREPARE) {
        devdrv_drv_err("Invalid mode. (mode=%lu)\n", mode);
        return -EINVAL;
    }

    if (file_op == NULL) {
        devdrv_drv_err("filep is NULL.\n");
        return -EINVAL;
    }

    if (file_op->private_data == NULL) {
        devdrv_drv_err("filep private_data is NULL.\n");
        return -EINVAL;
    }

    dev_manager_context = file_op->private_data;
    devdrv_host_release_notice_dev(dev_manager_context->tgid);
    devdrv_manager_process_sign_release(dev_manager_context->tgid);
    devdrv_drv_debug("Dmanage end release prepare.\n");
    return 0;
}

STATIC int devdrv_manager_process_exit(ka_notifier_block_t *nb, unsigned long mode, void *data)
{
    ka_task_struct_t *task = (ka_task_struct_t *)data;
    (void)mode;
    (void)nb;

    if ((task != NULL) && (task->mm != NULL) && (task->tgid != 0) && (task->tgid == task->pid) &&
        ascend_intf_is_pid_init(task->tgid, DAVINCI_INTF_MODULE_DEVMNG)) {
        /* Only the process open davinci device is check. */
        devdrv_manager_process_sign_release(task->tgid);
    }
    return 0;
}

STATIC const struct notifier_operations g_drv_intf_notifier_ops = {
    .notifier_call = devdrv_manage_release_prepare,
};

STATIC ka_notifier_block_t g_process_sign_exit_nb = {
    .notifier_call = devdrv_manager_process_exit,
};

static ka_notifier_block_t devdrv_manager_reboot_notifier = {
    .notifier_call = devdrv_manager_reboot_handle,
};

STATIC int devdrv_manager_register_notifier(void)
{
    int ret;

    ret = drv_ascend_register_notify(DAVINCI_INTF_MODULE_DEVMNG, &g_drv_intf_notifier_ops);
    if (ret != 0) {
        devdrv_drv_err("Register sub module fail. (ret=%d)\n", ret);
        return -ENODEV;
    }

    ret = ka_dfx_profile_event_register(KA_PROFILE_TASK_EXIT, &g_process_sign_exit_nb);
    if (ret != 0) {
        devdrv_drv_err("Register ka_dfx_profile_event_register fail. (ret=%d).\n", ret);
        return -ENODEV;
    }

    ret = ka_dfx_register_reboot_notifier(&devdrv_manager_reboot_notifier);
    if (ret != 0) {
        devdrv_drv_err("ka_dfx_register_reboot_notifier failed.\n");
        (void)ka_dfx_unregister_reboot_notifier(&devdrv_manager_reboot_notifier);
        (void)ka_dfx_profile_event_unregister(KA_PROFILE_TASK_EXIT, &g_process_sign_exit_nb);
        return ret;
    }
    return 0;
}

STATIC void devdrv_manager_unregister_notifier(void)
{
    (void)ka_dfx_unregister_reboot_notifier(&devdrv_manager_reboot_notifier);
    (void)ka_dfx_profile_event_unregister(KA_PROFILE_TASK_EXIT, &g_process_sign_exit_nb);
}

STATIC int devdrv_manager_info_init(void)
{
    int i;

    dev_manager_info = dbl_kzalloc(sizeof(*dev_manager_info), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (dev_manager_info == NULL) {
        devdrv_drv_err("kzalloc return NULL, failed to alloc mem for manager struct.\n");
        return -ENOMEM;
    }

    ka_task_mutex_init(&dev_manager_info->pm_list_lock);
    KA_INIT_LIST_HEAD(&dev_manager_info->pm_list_header);

    ka_task_spin_lock_init(&dev_manager_info->proc_hash_table_lock);
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    ka_hash_init(dev_manager_info->proc_hash_table);
#endif
    KA_INIT_LIST_HEAD(&dev_manager_info->hostpid_list_header);
    ka_task_mutex_init(&dev_manager_info->devdrv_sign_list_lock);
    (void)memset_s(dev_manager_info->devdrv_sign_count, MAX_DOCKER_NUM + 1, 0, MAX_DOCKER_NUM + 1);

    dev_manager_info->prob_num = 0;

    for (i = 0; i < ASCEND_DEV_MAX_NUM / KA_BITS_PER_LONG_LONG + 1; i++) {
        dev_manager_info->prob_device_bitmap[i] = 0;
    }

    devdrv_manager_dev_num_reset();
    dev_manager_info->host_type = adap_get_host_type();
    dev_manager_info->drv_ops = &devdrv_host_drv_ops;

    ka_task_spin_lock_init(&dev_manager_info->spinlock);

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        devdrv_manager_set_devdrv_info(i, NULL);
        devdrv_manager_set_no_trans_chan(i, NULL);
        devdrv_set_devdrv_info_array(i, NULL);
        dev_manager_info->device_status[i] = DRV_STATUS_INITING;
    }
    dev_manager_info->plat_info = DEVDRV_MANAGER_HOST_ENV;

    return 0;
}

STATIC void devdrv_manager_info_free(void)
{
    dbl_kfree(dev_manager_info);
    dev_manager_info = NULL;
}

int devdrv_manager_init(void)
{
    struct uda_dev_type type;
    ka_device_t *i_device = NULL;
    int ret = 0;

    ret = drv_davinci_register_sub_module(DAVINCI_INTF_MODULE_DEVMNG, &devdrv_manager_file_operations);
    if (ret) {
        devdrv_drv_err("drv_davinci_register_sub_module failed! ret=%d\n", ret);
        goto register_sub_module_fail;
    }

    ret = devdrv_manager_info_init();
    if (ret) {
        devdrv_drv_err("Init dev_manager_info failed.\n");
        goto dev_manager_info_init_failed;
    }

    ret = devmng_devlog_addr_init();
    if (ret) {
        devdrv_drv_err("Ts log addr init failed. (ret=%d)\n", ret);
        goto tslog_addr_init_fail;
    }

    devdrv_host_black_box_init();

    ret = devdrv_manager_register_notifier();
    if (ret) {
        devdrv_drv_err("Failed to register dmanager notifier.\n");
        goto register_notifier_fail;
    }

    i_device = davinci_intf_get_owner_device();
    if (i_device == NULL) {
        devdrv_drv_err("failed to intf get owner device.\n");
        ret = -ENODEV;
        goto get_device_fail;
    }
    dev_manager_info->dev = i_device;

    tsdrv_status_init();

#ifdef CONFIG_SYSFS
    ret = ka_sysfs_create_group(&i_device->kobj, &devdrv_manager_attr_group);
    if (ret) {
        devdrv_drv_err("sysfs create failed, ret(%d)\n", ret);
        goto sysfs_create_group_failed;
    }
#endif /* CONFIG_SYSFS */

    dev_manager_info->dev_rdy_work = ka_task_create_singlethread_workqueue("dev_manager_workqueue");
    if (dev_manager_info->dev_rdy_work == NULL) {
        devdrv_drv_err("create workqueue failed\n");
        ret = -EINVAL;
        goto workqueue_create_failed;
    }

    ret = devdrv_manager_container_table_init(dev_manager_info);
    if (ret) {
        devdrv_drv_err("container table init failed, ret(%d)\n", ret);
        ret = -ENODEV;
        goto container_table_init_failed;
    }

    ret = devdrv_manager_online_kfifo_alloc();
    if (ret) {
        devdrv_drv_err("ka_base_kfifo_alloc failed, ret(%d)\n", ret);
        ret = -ENOMEM;
        goto online_kfifo_alloc_failed;
    }

    ret = dev_mnt_vdevice_init();
    if (ret != 0) {
        devdrv_drv_err("vdevice init failed, ret = %d\n", ret);
        goto vdevice_init_failed;
    }

    devdrv_manager_common_chan_init();
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(DEVMNG_HOST_NOTIFIER, &type, UDA_PRI1, devdrv_manager_host_notifier_func);
    if (ret) {
        devdrv_drv_err("uda_notifier_register failed, ret=%d\n", ret);
        goto uda_notifier_register_failed;
    }
#endif
    ret = hvdevmng_init();
    if (ret != 0) {
        devdrv_drv_err("vmngh_register vmnh client failed, ret = %d\n", ret);
        goto vmngh_register_failed;
    }

#ifndef CFG_FEATURE_UB
    adap_dev_startup_register(devdrv_manager_dev_startup_notify);
    adap_dev_state_notifier_register(devdrv_manager_dev_state_notify);
#endif

    ret = log_level_file_init();
    if (ret != 0) {
        devdrv_drv_err("log_level_file_init failed!!! ret(%d), default_log_level is ERROR.\n", ret);
    }
    ka_task_init_rwsem(&devdrv_ops_sem);

    devdrv_pid_map_init();

    module_init_finish = 1;
    return 0;

vmngh_register_failed:
#ifndef DEVDRV_MANAGER_HOST_UT_TEST
    (void)uda_notifier_unregister(DEVMNG_HOST_NOTIFIER, &type);
#endif
uda_notifier_register_failed:
    dev_mnt_vdevice_uninit();
vdevice_init_failed:
    devdrv_manager_online_kfifo_free();
online_kfifo_alloc_failed:
    devdrv_manager_container_table_exit(dev_manager_info);
container_table_init_failed:
    ka_task_destroy_workqueue(dev_manager_info->dev_rdy_work);
workqueue_create_failed:
#ifdef CONFIG_SYSFS
    ka_sysfs_remove_group(&i_device->kobj, &devdrv_manager_attr_group);
sysfs_create_group_failed:
#endif /* CONFIG_SYSFS */
get_device_fail:
    devdrv_manager_unregister_notifier();
register_notifier_fail:
    devmng_devlog_addr_uninit();
tslog_addr_init_fail:
    devdrv_manager_info_free();
dev_manager_info_init_failed:
    (void)drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_DEVMNG);
register_sub_module_fail:
    return ret;
}

void devdrv_manager_exit(void)
{
    ka_list_head_t *pos = NULL;
    ka_list_head_t *n = NULL;
    struct devdrv_pm *pm = NULL;
    struct uda_dev_type type;

    log_level_file_remove();

#ifndef CFG_FEATURE_UB
    adap_dev_state_notifier_unregister();
#endif

#ifdef CONFIG_SYSFS
    ka_sysfs_remove_group(&dev_manager_info->dev->kobj, &devdrv_manager_attr_group);
#endif /* CONFIG_SYSFS */

    hvdevmng_uninit();
    devdrv_host_black_box_exit();

    if (!ka_list_empty_careful(&dev_manager_info->pm_list_header)) {
        ka_list_for_each_safe(pos, n, &dev_manager_info->pm_list_header)
        {
            pm = ka_list_entry(pos, struct devdrv_pm, list);
            ka_list_del(&pm->list);
            dbl_kfree(pm);
            pm = NULL;
        }
    }
    devdrv_manager_free_hashtable();

    uda_davinci_near_real_entity_type_pack(&type);
    (void)uda_notifier_unregister(DEVMNG_HOST_NOTIFIER, &type);
    devdrv_manager_common_chan_uninit();

    devdrv_manager_uninit_devinfo();

    ka_task_destroy_workqueue(dev_manager_info->dev_rdy_work);
    devdrv_manager_container_table_exit(dev_manager_info);
    dbl_kfree(dev_manager_info);
    dev_manager_info = NULL;
    devdrv_manager_unregister_notifier();
    devdrv_manager_online_kfifo_free();
    dev_mnt_vdevice_uninit();
    dms_hotreset_task_exit();
    if (drv_ascend_unregister_sub_module(DAVINCI_INTF_MODULE_DEVMNG)) {
        devdrv_drv_err("drv_ascend_unregister_sub_module failed!\n");
    }

    devmng_devlog_addr_uninit();
    devmng_basic_info_uninit();
#ifndef CFG_FEATURE_NO_DP_PROC
    devdrv_pid_map_uninit();
#endif
}
