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
#include "ka_system_pub.h"
#include "dms/dms_devdrv_manager_comm.h"
#include "comm_kernel_interface.h"
#include "vmng_kernel_interface.h"
#include "pbl/pbl_feature_loader.h"
#include "pbl_mem_alloc_interface.h"
#include "pbl/pbl_soc_res.h"
#include "dms_time.h"
#include "dms_define.h"
#include "urd_feature.h"
#include "dms_template.h"
#include "urd_acc_ctrl.h"
#include "dms/dms_cmd_def.h"
#include "dms/dms_notifier.h"
#include "kernel_version_adapt.h"
#include "devdrv_pcie.h"
#include "adapter_api.h"
#include "dms_urd_forward.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "ka_dfx_pub.h"
#include "dms_hotreset.h"

#ifdef ENABLE_BUILD_PRODUCT
#define DEVDRV_CLOUD_V2_BOARD_TYPE_MASK 0x7
#define DEVDRV_CLOUD_V2_BOARD_TYPE_OFFSET 4
#define DEVDRV_CLOUD_V2_1DIE_TRAIN_PCIE_CARD 0x1
#define DEVDRV_CLOUD_V2_1DIE_INFER_PCIE_CARD 0x2
#define DEVDRV_CLOUD_V2_BIT7_MASK 0x80 /* mask to extract bit7 */
#endif

STATIC struct hotreset_task_info *g_device_task_info[ASCEND_DEV_MAX_NUM] = {0};
STATIC bool g_hotreset_executing_flag[ASCEND_DEV_MAX_NUM] = {false};
static ka_task_spinlock_t g_hotreset_executing_spinlock;
static KA_TASK_DEFINE_MUTEX(g_device_task_info_mutex);

BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_BASIC_POWER_INFO)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_HOTRESET_SETFLAG, NULL, NULL,
                    DMS_ACC_ROOT_ONLY | DMS_ENV_PHYSICAL | DMS_ENV_ADMIN_DOCKER, dms_hotreset_atomic_setflag)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_HOTRESET_CLEARFLAG, NULL, NULL,
                    DMS_ACC_ROOT_ONLY | DMS_ENV_PHYSICAL | DMS_ENV_ADMIN_DOCKER, dms_hotreset_atomic_clearflag)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_PRERESET_ASSEMBLE, NULL, NULL,
                    DMS_ACC_ROOT_ONLY | DMS_ENV_PHYSICAL | DMS_ENV_ADMIN_DOCKER, dms_power_pcie_pre_reset)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_HOTRESET_ASSEMBLE, NULL, NULL,
#ifdef CFG_FEATURE_SRIOV
                    DMS_ENV_NOT_NORMAL_DOCKER | DMS_ACC_ROOT_ONLY | DMS_VDEV_VIRTUAL,
#else
                    DMS_ACC_ROOT_ONLY | DMS_ENV_PHYSICAL | DMS_ENV_ADMIN_DOCKER,
#endif
                    dms_hotreset_assmemble)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_PRERESET_ASSEMBLE1, NULL, NULL,
                    DMS_ACC_ROOT_ONLY | DMS_ENV_PHYSICAL | DMS_ENV_ADMIN_DOCKER, dms_power_pcie_pre_reset_v1)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_HOTRESET_RESCAN, NULL, NULL,
                    DMS_ACC_ROOT_ONLY | DMS_ENV_PHYSICAL | DMS_ENV_ADMIN_DOCKER, dms_hotreset_atomic_rescan)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_HOTRESET_UNBIND, NULL, NULL,
                    DMS_ACC_ROOT_ONLY | DMS_ENV_PHYSICAL | DMS_ENV_ADMIN_DOCKER, dms_hotreset_atomic_unbind)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_HOTRESET_RESET, NULL, NULL,
                    DMS_ACC_ROOT_ONLY | DMS_ENV_PHYSICAL | DMS_ENV_ADMIN_DOCKER, dms_hotreset_atomic_reset)
ADD_FEATURE_COMMAND(DMS_MODULE_BASIC_POWER_INFO, DMS_MAIN_CMD_HOTRESET, DMS_SUBCMD_HOTRESET_REMOVE, NULL, NULL,
                    DMS_ACC_ROOT_ONLY | DMS_ENV_PHYSICAL | DMS_ENV_ADMIN_DOCKER, dms_hotreset_atomic_remove)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

STATIC struct hotreset_task_info *dms_get_device_task_info(unsigned int dev_id)
{
    struct hotreset_task_info *device_task_info = NULL;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        dms_err("Wrong device id. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    device_task_info = g_device_task_info[dev_id];

    return device_task_info;
}

STATIC void dms_set_device_task_info(unsigned int dev_id, struct hotreset_task_info *device_task_info)
{
    g_device_task_info[dev_id] = device_task_info;
}

STATIC struct hotreset_task_info *dms_task_info_alloc(unsigned int dev_id)
{
    struct hotreset_task_info *device_task_info = NULL;
    device_task_info = dbl_kzalloc(sizeof(struct hotreset_task_info), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (device_task_info == NULL) {
        dms_err("Kzalloc failed. (dev_id=%u)\n", dev_id);
        return NULL;
    }
    ka_task_init_rwsem(&device_task_info->task_rw_sema);

    return device_task_info;
}

STATIC unsigned long get_dms_task_info_cnt(unsigned int dev_id)
{
    return g_device_task_info[dev_id]->task_ref_cnt;
}

STATIC void set_hotreset_task_flag(unsigned int dev_id)
{
    ka_base_set_bit(HOTRESET_TASK_FLAG_BIT, &g_device_task_info[dev_id]->task_flag);
}

STATIC void clear_hotreset_task_flag(unsigned int dev_id)
{
    ka_base_clear_bit(HOTRESET_TASK_FLAG_BIT, &g_device_task_info[dev_id]->task_flag);
}

STATIC int get_hotreset_task_flag(unsigned int dev_id)
{
    return ka_base_test_bit(HOTRESET_TASK_FLAG_BIT, &g_device_task_info[dev_id]->task_flag);
}

STATIC int dms_power_hotreset_notifier_handle(ka_notifier_block_t *self, unsigned long event, void *data)
{
    struct devdrv_info *dev_info = NULL;

    dev_info = (struct devdrv_info *)data;
    if ((data == NULL) || (event <= DMS_DEVICE_NOTIFIER_MIN) || (event >= DMS_DEVICE_NOTIFIER_MAX)) {
        dms_err("Invalid parameter. (event=0x%lx; data=\"%s\")\n", event, data == NULL ? "NULL" : "OK");
        return KA_NOTIFY_BAD;
    }

    dms_debug("Notifier hotreset handle success. (event=0x%lx; dev_id=%u)\n", event, dev_info->dev_id);
    return KA_NOTIFY_DONE;
}

static ka_notifier_block_t dms_power_hotreset_notifier = {
    .notifier_call = dms_power_hotreset_notifier_handle,
};

#ifdef ENABLE_BUILD_PRODUCT
static bool dms_is_910B_pcie_card(int dev_id, bool *isCard)
{
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    int board_id = 0;
    u32 board_type;

    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        dms_err("Get device ctrl block failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    if (dev_cb->dev_info == NULL) {
        dms_err("Device ctrl dev_info is null. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    dev_info = (struct devdrv_info *)dev_cb->dev_info;
    board_id = dev_info->board_id;

    // non-910B
    if ((board_id & DEVDRV_CLOUD_V2_BIT7_MASK) != 0) {
        *isCard = false;
        return 0;
    }

    board_type = (board_id >> DEVDRV_CLOUD_V2_BOARD_TYPE_OFFSET) & DEVDRV_CLOUD_V2_BOARD_TYPE_MASK;
    if ((board_type == DEVDRV_CLOUD_V2_1DIE_TRAIN_PCIE_CARD) || (board_type == DEVDRV_CLOUD_V2_1DIE_INFER_PCIE_CARD)) {
        *isCard = true;
    } else {
        *isCard = false;
    }

    return 0;
}

static int dms_is_910_A3(int dev_id, int *is_910_A3)
{
    struct devdrv_info *dev_info = NULL;
    struct dms_dev_ctrl_block *dev_cb = NULL;
    int board_id = 0;

    dev_cb = dms_get_dev_cb(dev_id);
    if (dev_cb == NULL) {
        dms_err("Get device ctrl block failed. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    if (dev_cb->dev_info == NULL) {
        dms_err("Device ctrl dev_info is null. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    dev_info = (struct devdrv_info *)dev_cb->dev_info;
    board_id = dev_info->board_id;

    // non-910A3
    if (((board_id & DEVDRV_CLOUD_V2_BIT7_MASK) >> DEVDRV_CLOUD_V2_BOARD_TYPE_MASK) != 1) {
        *is_910_A3 = 0;
        return 0;
    }
    *is_910_A3 = 1;
    return 0;
}
#endif

int dms_power_check_phy_mach(unsigned int dev_id)
{
    unsigned int host_flag;
    int ret;
#ifdef ENABLE_BUILD_PRODUCT
    int is_910_A3 = 0;
    bool isCard;
#endif

    ret = devdrv_get_host_phy_mach_flag(dev_id, &host_flag);
    if (ret != 0) {
        dms_err("Devdrv_get_host_phy_mach_flag return value error. (devid=%u; ret=0x%x)\n", dev_id, ret);
        return ret;
    }
#ifdef ENABLE_BUILD_PRODUCT
    if (host_flag != DEVDRV_HOST_PHY_MACH_FLAG && host_flag != 0) {
#else
    if (host_flag != DEVDRV_HOST_PHY_MACH_FLAG) {
#endif
        dms_err("Device not phy mach. (devid=%u; host_flag=0x%x)\n", dev_id, host_flag);
        return -EPERM;
    }

#ifdef ENABLE_BUILD_PRODUCT
    // 910B standard card does not support reset in container and VM scenarios
    ret = dms_is_910B_pcie_card(dev_id, &isCard);
    if (ret != 0) {
        dms_err("get board id failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    // upgrade_tool intercepts non-physical-machine hot reset for 910B standard card
    if (isCard == true && host_flag != DEVDRV_HOST_PHY_MACH_FLAG) {
        dms_err("910_A2 Pcie card not phy mach not support hot_reset. (devid=%u; host_flag=0x%x)\n", dev_id, host_flag);
        return -EPERM;
    }

    ret = dms_is_910_A3(dev_id, &is_910_A3);
    if (ret != 0) {
        dms_err("call dms_is_910_A3 failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    if (is_910_A3 == 1 && host_flag != DEVDRV_HOST_PHY_MACH_FLAG) {
        dms_err("910_A3 is not on physical machine, does not support hot_reset. (devid=%u; host_flag=0x%x)\n", dev_id,
                host_flag);
        return -EPERM;
    }
#endif

#if defined(CFG_HOST_ENV) && defined(CFG_FEATURE_VFIO)
    if (vmng_get_device_split_mode(dev_id) != VMNG_NORMAL_NONE_SPLIT_MODE) {
        dms_err("The device has been split. Hot reset is not allowed. (dev_id=%u)\n", dev_id);
        return -EPERM;
    }
#endif

    return 0;
}

STATIC bool devdrv_check_hotreset_flag(unsigned int dev_id)
{
    int flag;
    ka_task_down_write(&g_device_task_info[dev_id]->task_rw_sema);
    flag = get_hotreset_task_flag(dev_id);
    if (flag != 0) {
        ka_task_up_write(&g_device_task_info[dev_id]->task_rw_sema);
        dms_info("Pre-hotreset has been triggered. (dev_id=%u)\n", dev_id);
        return false;
    }
    ka_task_up_write(&g_device_task_info[dev_id]->task_rw_sema);

    return true;
}

STATIC int dms_power_check_all_phy_mach(void)
{
    unsigned int dev_num;
    unsigned int phys_id = 0;
    unsigned int vfid = 0;
    int i, ret;

    dev_num = devdrv_manager_get_devnum();
    if (dev_num > ASCEND_DEV_MAX_NUM) {
        dms_err("Get invalid device num. (dev_num=%u; max=%u)\n", dev_num, ASCEND_DEV_MAX_NUM);
        return -EINVAL;
    }
    for (i = 0; i < dev_num; ++i) {
        ret = devdrv_manager_container_logical_id_to_physical_id(i, &phys_id, &vfid);
        if (ret) {
            dms_err("Can't transform virt id. (virt_id=%d; ret=%d)\n", i, ret);
            return ret;
        }
        ret = dms_power_check_phy_mach(phys_id);
        if (ret) {
            return ret;
        }
    }

    return 0;
}

static void devdrv_uda_all_dev_ctrl_hotreset_cancel(u32 max_dev)
{
    u32 i;

    for (i = 0; i < max_dev; i++) {
        if (uda_is_udevid_exist(i)) {
            (void)uda_dev_ctrl(i, UDA_CTRL_HOTRESET_CANCEL);
        }
    }
}

STATIC void devdrv_uda_all_pre_dev_ctrl_hotreset_cancel(u32 max_dev)
{
    u32 i;

    for (i = 0; i < max_dev; i++) {
        if (uda_is_udevid_exist(i)) {
            (void)uda_dev_ctrl(i, UDA_CTRL_PRE_HOTRESET_CANCEL);
        }
    }
}

STATIC int devdrv_uda_all_dev_ctrl_hotreset(u32 max_dev)
{
    int ret;
    u32 i;

    ka_task_mutex_lock(&g_device_task_info_mutex);
    for (i = 0; i < max_dev; i++) {
        if (!uda_is_udevid_exist(i) || (dms_get_device_task_info(i) == NULL)) {
            continue;
        }

        if (devdrv_check_hotreset_flag(i) == true) {
            ka_task_mutex_unlock(&g_device_task_info_mutex);
            ret = uda_dev_ctrl(i, UDA_CTRL_HOTRESET);
            ka_task_mutex_lock(&g_device_task_info_mutex);
            if (ret != 0) {
                ka_task_mutex_unlock(&g_device_task_info_mutex);
                devdrv_uda_all_dev_ctrl_hotreset_cancel(i);
                devdrv_drv_err("Uda ctrl hotreset failed. (devid=%u; ret=%d)\n", i, ret);
                return ret;
            }
        }
    }
    ka_task_mutex_unlock(&g_device_task_info_mutex);
    return 0;
}

STATIC int devdrv_get_brother_udevid(u32 udevid, u32 *bro_udevid)
{
    int ret;
    u32 i;
    u32 udevid_master_id, tmp_master_id;

    ret = adap_get_master_devid_in_the_same_os(udevid, &udevid_master_id);
    if (ret != 0) {
        dms_warn("udevid master id cannot be obtained. (udevid=%u; ret=%d)\n", udevid, ret);
        return ret;
    }

    for (i = 0; i < ASCEND_PDEV_MAX_NUM; i++) {
        if (i == udevid) {
            continue;
        }
        if (!uda_is_udevid_exist(i)) {
            continue;
        }

        ret = adap_get_master_devid_in_the_same_os(i, &tmp_master_id);
        if (ret != 0) {
            dms_warn("Enumerated udevid master id cannot be obtained. (udevid=%u; ret=%d)\n", i, ret);
            continue;
        }

        if (udevid_master_id == tmp_master_id) {
            devdrv_drv_info("get smp brother devid. (ori_devid=%u; bro_devid=%u; ori_masterid=%u; tmp_masterid=%u)\n",
                            udevid, i, udevid_master_id, tmp_master_id);
            *bro_udevid = i;
            return 0;
        }
    }
    return -ESRCH;
}

int devdrv_uda_one_dev_ctrl_hotreset(u32 udevid)
{
    int ret;
    u32 bro_udevid;

    ka_task_mutex_lock(&g_device_task_info_mutex);
    if (dms_get_device_task_info(udevid) == NULL) {
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        return 0;
    }

    if (devdrv_check_hotreset_flag(udevid) == true) {
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        ret = uda_dev_ctrl(udevid, UDA_CTRL_HOTRESET);
        ka_task_mutex_lock(&g_device_task_info_mutex);
        if (ret != 0) {
            ka_task_mutex_unlock(&g_device_task_info_mutex);
            devdrv_drv_err("Uda ctrl hotreset failed. (devid=%u; ret=%d)\n", udevid, ret);
            return ret;
        }
    }

    ret = devdrv_get_brother_udevid(udevid, &bro_udevid);
    if (ret != 0) {
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        return 0;
    }

    if (dms_get_device_task_info(bro_udevid) == NULL) {
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        return 0;
    }

    if (devdrv_check_hotreset_flag(bro_udevid) == true) {
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        ret = uda_dev_ctrl(bro_udevid, UDA_CTRL_HOTRESET);
        ka_task_mutex_lock(&g_device_task_info_mutex);
        if (ret != 0) {
            ka_task_mutex_unlock(&g_device_task_info_mutex);
            (void)uda_dev_ctrl(udevid, UDA_CTRL_HOTRESET_CANCEL);
            devdrv_drv_err("Uda ctrl hotreset failed. (bro_udevid=%u; ret=%d)\n", bro_udevid, ret);
            return ret;
        }
    }
    ka_task_mutex_unlock(&g_device_task_info_mutex);

    return 0;
}
void devdrv_uda_one_dev_ctrl_hotreset_cancel(u32 udevid)
{
    int ret;
    u32 bro_udevid;

    (void)uda_dev_ctrl(udevid, UDA_CTRL_HOTRESET_CANCEL);

    ret = devdrv_get_brother_udevid(udevid, &bro_udevid);
    if (ret != 0) {
        return;
    }
    (void)uda_dev_ctrl(bro_udevid, UDA_CTRL_HOTRESET_CANCEL);
}

STATIC int dms_all_device_hotreset_excuting_check_set(void)
{
    u32 i;

    ka_task_spin_lock(&g_hotreset_executing_spinlock);
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        if (g_hotreset_executing_flag[i]) {
            ka_task_spin_unlock(&g_hotreset_executing_spinlock);
            dms_err("Hot reset is being performed on some devices. (dev_id=%u)\n", i);
            return -EBUSY;
        }
    }
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        g_hotreset_executing_flag[i] = true;
    }
    ka_task_spin_unlock(&g_hotreset_executing_spinlock);
    return 0;
}

STATIC void dms_set_all_device_hotreset_excuting_flag(bool flag)
{
    u32 i;
    ka_task_spin_lock(&g_hotreset_executing_spinlock);
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        g_hotreset_executing_flag[i] = flag;
    }
    ka_task_spin_unlock(&g_hotreset_executing_spinlock);
}

STATIC int dms_power_set_all_hot_reset(void *feature)
{
    int ret = 0;

#ifdef CFG_FEATURE_TIMESYNC
    int i;
#endif
    ret = dms_power_check_all_phy_mach();
    if (ret) {
        dms_err("Permission deny or devid is invalid; (ret=%d)\n", ret);
        return ret;
    }

#ifdef CFG_FEATURE_TIMESYNC
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        set_time_need_update(i);
    }
#endif

    ret = dms_all_device_hotreset_excuting_check_set();
    if (ret != 0) {
        return ret;
    }

    ret = devdrv_uda_all_dev_ctrl_hotreset(ASCEND_DEV_MAX_NUM);
    if (ret != 0) {
        dms_err("Notify all uda device hotreset failed. (ret=%d)\n", ret);
        goto ALL_DEV_HORESET_END;
    }

#ifdef CFG_FEATURE_SRIOV
    ret = devdrv_manager_check_and_disable_sriov(DEVDRV_RESET_ALL_DEVICE_ID);
    if (ret != 0) {
        dms_err("Failed to check and disable for all devices. (ret=%d)\n", ret);
        devdrv_uda_all_dev_ctrl_hotreset_cancel(ASCEND_DEV_MAX_NUM);
        goto ALL_DEV_HORESET_END;
    }
#endif

    ret = devdrv_hot_reset_device(0xffff); // all device hot reset flag: 0xffff
    if (ret != 0) {
        dms_err("Hotreset all devices failed. (ret=%d)\n", ret);
        devdrv_uda_all_dev_ctrl_hotreset_cancel(ASCEND_DEV_MAX_NUM);
    }

ALL_DEV_HORESET_END:
    dms_set_all_device_hotreset_excuting_flag(false);
    return ret;
}

STATIC int dms_single_device_hotreset_excuting_check_set(unsigned int phy_id, unsigned int bro_phy_id)
{
    if ((phy_id >= ASCEND_DEV_MAX_NUM) || (bro_phy_id >= ASCEND_DEV_MAX_NUM)) {
        dms_err("Invalid physical id. (phy_id=%u; bro_phy_id=%u)\n", phy_id, bro_phy_id);
        return -EINVAL;
    }

    ka_task_spin_lock(&g_hotreset_executing_spinlock);
    if (g_hotreset_executing_flag[phy_id] || g_hotreset_executing_flag[bro_phy_id]) {
        ka_task_spin_unlock(&g_hotreset_executing_spinlock);
        dms_err("Hotreset is executing. (dev_phy_id=%u; bro_phy_id=%u)\n", phy_id, bro_phy_id);
        return -EBUSY;
    }

    g_hotreset_executing_flag[phy_id] = true;
    g_hotreset_executing_flag[bro_phy_id] = true;

    ka_task_spin_unlock(&g_hotreset_executing_spinlock);
    return 0;
}

STATIC void dms_set_single_device_hotreset_excuting_flag(unsigned int phy_id, unsigned int bro_phy_id, bool flag)
{
    if ((phy_id >= ASCEND_DEV_MAX_NUM) || (bro_phy_id >= ASCEND_DEV_MAX_NUM)) {
        dms_err("Invalid physical id. (phy_id=%u; bro_phy_id=%u)\n", phy_id, bro_phy_id);
        return;
    }

    ka_task_spin_lock(&g_hotreset_executing_spinlock);

    g_hotreset_executing_flag[phy_id] = flag;
    g_hotreset_executing_flag[bro_phy_id] = flag;

    ka_task_spin_unlock(&g_hotreset_executing_spinlock);
}

STATIC int dms_power_set_single_hot_reset(void *feature, unsigned int virt_id)
{
    int ret = 0;
    unsigned int phy_id;
    unsigned int vfid;
    unsigned int bro_phy_id;
    /* the single device hot reset */
    ret = devdrv_manager_container_logical_id_to_physical_id(virt_id, &phy_id, &vfid);
    if (ret) {
        dms_err("Trans logical_id to physical_id failed. (devid=%u)\n", virt_id);
        return ret;
    }

    if (phy_id >= ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid physical id. (phy_id=%u)\n", phy_id);
        return -EINVAL;
    }

    if (devdrv_manager_is_pf_device(phy_id)) {
        ret = dms_power_check_phy_mach(phy_id);
        if (ret != 0) {
            dms_err("Permission deny or devid is invalid. (dev_id=%u; ret=%d)\n", virt_id, ret);
            return ret;
        }

#ifdef CFG_FEATURE_TIMESYNC
        set_time_need_update(phy_id);
#endif
    }

    if (devdrv_get_brother_udevid(phy_id, &bro_phy_id) != 0) {
        bro_phy_id = phy_id;
    } else if (bro_phy_id >= ASCEND_DEV_MAX_NUM) {
        dms_err("Invalid brother physical id. (bro_phy_id=%u)\n", bro_phy_id);
        bro_phy_id = phy_id;
    }

    ret = dms_single_device_hotreset_excuting_check_set(phy_id, bro_phy_id);
    if (ret != 0) {
        return ret;
    }

    ret = devdrv_uda_one_dev_ctrl_hotreset(phy_id);
    if (ret != 0) {
        dms_err("Uda ctrl hotreset failed. (devid=%u; ret=%d)\n", phy_id, ret);
        goto SINGLE_DEV_HOTRESET_END;
    }

    if (devdrv_manager_is_pf_device(phy_id)) {
#ifdef CFG_FEATURE_SRIOV
        ret = devdrv_manager_check_and_disable_sriov(phy_id);
        if (ret != 0) {
            dms_err("Check disable sriov failed. (dev_id=%u; phy_id=%u; ret=%d)\n", virt_id, phy_id, ret);
            devdrv_uda_one_dev_ctrl_hotreset_cancel(phy_id);
            goto SINGLE_DEV_HOTRESET_END;
        }
#endif

        ret = devdrv_hot_reset_device(phy_id);
    } else if (devdrv_manager_is_mdev_vm_mode(phy_id)) {
        ret = devdrv_hot_reset_device(phy_id);
#ifdef CFG_FEATURE_SRIOV
    } else {
        ret = vmngh_sriov_reset_vdev(phy_id, vfid);
#endif
    }

    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Hotreset single device failed. (dev_id=%u; ret=%d)\n", phy_id, ret);
        devdrv_uda_one_dev_ctrl_hotreset_cancel(phy_id);
    }

SINGLE_DEV_HOTRESET_END:
    dms_set_single_device_hotreset_excuting_flag(phy_id, bro_phy_id, false);
    return ret;
}

int dms_power_pcie_pre_reset(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    unsigned int virt_id;
    int ret;
    unsigned int phys_id = 0;
    unsigned int vfid = 0;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input arg is NULL or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    virt_id = *(unsigned int *)(uintptr_t)in;

    ret = devdrv_manager_container_logical_id_to_physical_id(virt_id, &phys_id, &vfid);
    if (ret != 0) {
        dms_err("Failed to transfer dev_id. (dev_id=%u)\n", virt_id);
        return -EFAULT;
    }

    if ((vfid != 0) || (!devdrv_manager_is_pf_device(phys_id))) {
        return -EOPNOTSUPP;
    }

    ret = dms_power_check_phy_mach(phys_id);
    if (ret) {
        dms_err("Permission deny or devid is invalid. (dev_id=%u; ret=%d)\n", phys_id, ret);
        return -EINVAL;
    }

    ret = devdrv_uda_one_dev_ctrl_hotreset(phys_id);
    if (ret != 0) {
        dms_err("Call uda_dev_ctrl failed, (phys_id=%u; ret=%d).\n", phys_id, ret);
        return ret;
    }

    ret = devdrv_hot_pre_reset(phys_id);
    if (ret != 0) {
        dms_err("Hotreset failed. (dev_id=%u; ret=%d)\n", phys_id, ret);
        devdrv_uda_one_dev_ctrl_hotreset_cancel(phys_id);
    }

    return ret;
}

int dms_hotreset_assmemble(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    unsigned int virt_id;
    int ret = 0;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input arg is NULL or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    virt_id = *(unsigned int *)(uintptr_t)in;

    /* hot reset all devices */
    if (virt_id == DEVDRV_RESET_ALL_DEVICE_ID) {
        ret = dms_power_set_all_hot_reset(feature);
        if (ret) {
            dms_err("Set all device hotreset failed. (dev_id=%d; ret=%d)\n", virt_id, ret);
        }
        return ret;
    }

    /* the single device hot reset */
    ret = dms_power_set_single_hot_reset(feature, virt_id);
    if (ret != 0) {
        dms_ex_notsupport_err(ret, "Set single device hotreset failed. (dev_id=%u; ret=%d)\n", virt_id, ret);
    }

    return ret;
}

void dms_notify_single_device_cancel_hotreset(unsigned int dev_id)
{
    ka_task_mutex_lock(&g_device_task_info_mutex);
    if (dms_get_device_task_info(dev_id) != NULL) {
        clear_hotreset_task_flag(dev_id);
    }
    ka_task_mutex_unlock(&g_device_task_info_mutex);
    return;
}

int dms_hotreset_task_cnt_increase(unsigned int dev_id)
{
    int flag;
    ka_task_mutex_lock(&g_device_task_info_mutex);
    if (dms_get_device_task_info(dev_id) == NULL) {
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        dms_err("The device does not exist. (dev_id=%u)\n", dev_id);
        return -ENODEV;
    }

    ka_task_down_write(&g_device_task_info[dev_id]->task_rw_sema);
    flag = get_hotreset_task_flag(dev_id);
    if (flag != 0) {
        ka_task_up_write(&g_device_task_info[dev_id]->task_rw_sema);
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        dms_err("Hotreset is running. (dev_id=%u; flag=%d)\n", dev_id, flag);
        return -EBUSY;
    }
    g_device_task_info[dev_id]->task_ref_cnt++;
    ka_task_up_write(&g_device_task_info[dev_id]->task_rw_sema);
    ka_task_mutex_unlock(&g_device_task_info_mutex);
    return 0;
}

void dms_hotreset_task_cnt_decrease(unsigned int dev_id)
{
    ka_task_mutex_lock(&g_device_task_info_mutex);
    if (dms_get_device_task_info(dev_id) == NULL) {
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        dms_err("The device does not exist. (dev_id=%u)\n", dev_id);
        return;
    }

    ka_task_down_write(&g_device_task_info[dev_id]->task_rw_sema);
    g_device_task_info[dev_id]->task_ref_cnt--;
    ka_task_up_write(&g_device_task_info[dev_id]->task_rw_sema);
    ka_task_mutex_unlock(&g_device_task_info_mutex);
}

int dms_hotreset_atomic_setflag(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret = 0;
    unsigned int virt_id;
    unsigned int phy_id;
    unsigned int vfid;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input arg is NULL or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    virt_id = *(unsigned int *)(uintptr_t)in;
    if ((virt_id == DEVDRV_RESET_ALL_DEVICE_ID) || (virt_id == ALL_DEVICE_HOTRESET_FLAG)) {
        ret = dms_notify_all_device_pre_hotreset();
        if (ret != 0) {
            dms_err("Upgradetool set all device hotreset flag failed. (ret=%d)\n", ret);
        }
        return ret;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(virt_id, &phy_id, &vfid);
    if (ret) {
        dms_err("Trans logical_id to physical_id failed. (devid=%u)\n", virt_id);
        return ret;
    }

    ret = dms_notify_pre_device_hotreset(phy_id);
    if (ret != 0) {
        dms_err("dms_notify_pre_device_hotreset failed. (phy_id =%d, ret=%d)\n", phy_id, ret);
        return ret;
    }

    ret = dms_single_device_hotreset_excuting_check_set(phy_id, phy_id);
    return ret;
}

int dms_notify_all_device_pre_hotreset(void)
{
    unsigned int i = 0;
    int ret = 0;

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        if (!uda_is_udevid_exist(i)) {
            continue;
        }

        ret = uda_dev_ctrl(i, UDA_CTRL_PRE_HOTRESET);
        if (ret != 0) {
            devdrv_uda_all_pre_dev_ctrl_hotreset_cancel(i);
            dms_err("Uda ctrl hotreset failed. (devid=%u; ret=%d)\n", i, ret);
            return ret;
        }
    }

    dms_event("Pre-hotreset set all devices flag success.\n");
    return ret;
}

int dms_notify_pre_device_hotreset(unsigned int dev_id)
{
    int flag;
    unsigned long task_cnt;

    ka_task_mutex_lock(&g_device_task_info_mutex);
    if (dms_get_device_task_info(dev_id) == NULL) {
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        dms_err("The device does not exist. (dev_id=%u).\n", dev_id);
        return -EINVAL;
    }

    ka_task_down_write(&g_device_task_info[dev_id]->task_rw_sema);
    flag = get_hotreset_task_flag(dev_id);
    if (flag != 0) {
        ka_task_up_write(&g_device_task_info[dev_id]->task_rw_sema);
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        dms_info("Pre-hotreset has been triggered. (dev_id=%u)\n", dev_id);
        return 0;
    }
    set_hotreset_task_flag(dev_id);

    task_cnt = get_dms_task_info_cnt(dev_id);
    if (task_cnt != 0) {
        clear_hotreset_task_flag(dev_id);
        ka_task_up_write(&g_device_task_info[dev_id]->task_rw_sema);
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        dms_err("Dms task_cnt not zero. (task_cnt=%lu; dev_id=%u)\n", task_cnt, dev_id);
        return -EINVAL;
    }
    ka_task_up_write(&g_device_task_info[dev_id]->task_rw_sema);
    ka_task_mutex_unlock(&g_device_task_info_mutex);
    return 0;
}

int dms_notify_device_hotreset(unsigned int dev_id)
{
    unsigned long task_cnt;
    ka_task_mutex_lock(&g_device_task_info_mutex);
    if (dms_get_device_task_info(dev_id) == NULL) {
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        dms_err("The device does not exist. (dev_id=%u).\n", dev_id);
        return -EINVAL;
    }

    ka_task_down_write(&g_device_task_info[dev_id]->task_rw_sema);

    set_hotreset_task_flag(dev_id);

    task_cnt = get_dms_task_info_cnt(dev_id);
    if (task_cnt != 0) {
        clear_hotreset_task_flag(dev_id);
        ka_task_up_write(&g_device_task_info[dev_id]->task_rw_sema);
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        dms_err("Dms task_cnt not zero. (task_cnt=%lu; dev_id=%u)\n", task_cnt, dev_id);
        return -EINVAL;
    }
    ka_task_up_write(&g_device_task_info[dev_id]->task_rw_sema);
    ka_task_mutex_unlock(&g_device_task_info_mutex);
    return 0;
}

int dms_power_pcie_pre_reset_v1(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret;
    unsigned int dev_id = 0;
    unsigned int udevid = 0;
    unsigned int chip_type;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input arg is NULL or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    dev_id = *(unsigned int *)in;
    ret = uda_devid_to_udevid(dev_id, &udevid);
    if (ret != 0) {
        devdrv_drv_err("Failed to transfer dev_id to udevid. (dev_id=%u)\n", dev_id);
        return -EFAULT;
    }

    chip_type = uda_get_chip_type(udevid);
    if ((chip_type == HISI_CLOUD_V1) || (chip_type == HISI_CLOUD_V2)) {
        return 0;
    } else if ((chip_type == HISI_MINI_V2) || (chip_type == HISI_MINI_V3) || (chip_type == HISI_CLOUD_V4) ||
               (chip_type == HISI_CLOUD_V5) || (chip_type == HISI_MINI_V4) || (chip_type == HISI_MINI_V5)) {
        ret = devdrv_uda_one_dev_ctrl_hotreset(udevid);
        if (ret != 0) {
            devdrv_drv_err("Call uda_dev_ctrl failed, (udevid=%u; ret=%d).\n", udevid, ret);
            return ret;
        }
        ret = devdrv_hot_pre_reset(udevid);
        if (ret != 0) {
            devdrv_uda_one_dev_ctrl_hotreset_cancel(udevid);
        }
        return ret;
    } else {
        return -EOPNOTSUPP;
    }
}

int dms_power_hotreset_init(void)
{
    int ret;

    ret = dms_register_notifier(&dms_power_hotreset_notifier);
    if (ret) {
        dms_err("Dms event register notifier failed. (ret=%d)\n", ret);
        return ret;
    }
    ka_task_spin_lock_init(&g_hotreset_executing_spinlock);
    CALL_INIT_MODULE(DMS_MODULE_BASIC_POWER_INFO);
    dms_debug("Dms power hotreset init success.\n");
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(dms_power_hotreset_init, FEATURE_LOADER_STAGE_5);

void dms_power_hotreset_uninit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_BASIC_POWER_INFO);
    (void)dms_unregister_notifier(&dms_power_hotreset_notifier);
    dms_debug("Dms power hotreset exit success.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(dms_power_hotreset_uninit, FEATURE_LOADER_STAGE_5);

int dms_hotreset_task_init(unsigned int dev_id)
{
    struct hotreset_task_info *device_task_info = NULL;

    ka_task_mutex_lock(&g_device_task_info_mutex);
    device_task_info = dms_get_device_task_info(dev_id);
    if (device_task_info != NULL) {
        dms_info("Repeat init stop flag instance. (dev_id=%u)\n", dev_id);
    } else {
        device_task_info = dms_task_info_alloc(dev_id);
        if (device_task_info == NULL) {
            ka_task_mutex_unlock(&g_device_task_info_mutex);
            dms_err("Alloc stop flag info mem fail. (dev_id=%u)\n", dev_id);
            return -ENOMEM;
        }
    }

    dms_set_device_task_info(dev_id, device_task_info);
    clear_hotreset_task_flag(dev_id);
    dms_set_single_device_hotreset_excuting_flag(dev_id, dev_id, false);
    g_device_task_info[dev_id]->task_ref_cnt = 0;
    ka_task_mutex_unlock(&g_device_task_info_mutex);
    dms_debug("Dms hotreset task init success. (dev_id=%u)\n", dev_id);
    return 0;
}

void dms_hotreset_task_exit(void)
{
    struct hotreset_task_info *device_task_info = NULL;
    unsigned int i;

    ka_task_mutex_lock(&g_device_task_info_mutex);
    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        device_task_info = dms_get_device_task_info(i);
        if (device_task_info == NULL) {
            continue;
        }

        dms_set_device_task_info(i, NULL);
        dbl_kfree(device_task_info);
        device_task_info = NULL;
    }
    ka_task_mutex_unlock(&g_device_task_info_mutex);

    dms_debug("Dms hotreset task exit success.\n");
}
#ifdef CFG_FEATURE_SRIOV
void dms_hotreset_vf_task_exit(unsigned int dev_id)
{
    struct hotreset_task_info *device_task_info = NULL;

    ka_task_mutex_lock(&g_device_task_info_mutex);
    device_task_info = dms_get_device_task_info(dev_id);
    if (device_task_info == NULL) {
        ka_task_mutex_unlock(&g_device_task_info_mutex);
        return;
    }

    dms_set_device_task_info(dev_id, NULL);
    dbl_kfree(device_task_info);
    device_task_info = NULL;
    ka_task_mutex_unlock(&g_device_task_info_mutex);

    dms_debug("Dms hotreset task exit success.\n");
}

#endif

static int dms_hotreset_atomic_precheck(void *feature, char *in, unsigned int in_len, unsigned int *phy_id)
{
    int ret = 0;
    unsigned int virt_id;
    unsigned int vfid;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        dms_err("Input arg is NULL or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    virt_id = *(unsigned int *)(uintptr_t)in;
    if (virt_id == DEVDRV_RESET_ALL_DEVICE_ID) {
        return -EOPNOTSUPP;
    }

    ret = devdrv_manager_container_logical_id_to_physical_id(virt_id, phy_id, &vfid);
    if (ret) {
        dms_err("Trans logical_id to physical_id failed. (devid=%u)\n", virt_id);
        return ret;
    }

    if ((vfid != 0) || (!devdrv_manager_is_pf_device(*phy_id))) {
        return -EOPNOTSUPP;
    }

    return ret;
}

int dms_hotreset_atomic_clearflag(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret = 0;
    unsigned int phy_id;

    ret = dms_hotreset_atomic_precheck(feature, in, in_len, &phy_id);
    if (ret) {
        dms_err("Call dms_hotreset_atomic_precheck failed. (ret=%d)\n", ret);
        return ret;
    }
    dms_notify_single_device_cancel_hotreset(phy_id);
    dms_set_single_device_hotreset_excuting_flag(phy_id, phy_id, false);
    return 0;
}

int dms_hotreset_atomic_unbind(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret = 0;
    unsigned int phy_id;

    ret = dms_hotreset_atomic_precheck(feature, in, in_len, &phy_id);
    if (ret) {
        dms_err("Call dms_hotreset_atomic_precheck failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = devdrv_hotreset_atomic_unbind(phy_id);
    return ret;
}

int dms_hotreset_atomic_reset(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret = 0;
    unsigned int phy_id;

    ret = dms_hotreset_atomic_precheck(feature, in, in_len, &phy_id);
    if (ret) {
        dms_err("Call dms_hotreset_atomic_precheck failed. (ret=%d)\n", ret);
        return ret;
    }

#ifdef CFG_FEATURE_TIMESYNC
    set_time_need_update(phy_id);
#endif
    ret = devdrv_hotreset_atomic_reset(phy_id);
    return ret;
}

int dms_hotreset_atomic_remove(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret = 0;
    unsigned int phy_id;

    ret = dms_hotreset_atomic_precheck(feature, in, in_len, &phy_id);
    if (ret) {
        dms_err("Call dms_hotreset_atomic_precheck failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = devdrv_hotreset_atomic_remove(phy_id);
    return ret;
}

int dms_hotreset_atomic_rescan(void *feature, char *in, unsigned int in_len, char *out, unsigned int out_len)
{
    int ret = 0;
    unsigned int phy_id;

    ret = dms_hotreset_atomic_precheck(feature, in, in_len, &phy_id);
    if (ret) {
        dms_err("Call dms_hotreset_atomic_precheck failed. (ret=%d)\n", ret);
        return ret;
    }

    ret = devdrv_hotreset_atomic_rescan(phy_id);
    return ret;
}