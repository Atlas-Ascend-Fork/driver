/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <stdbool.h>

#include "securec.h"
#include "davinci_interface.h"

#include "uda_cmd.h"
#include "ascend_hal.h"
#include "uda_user_kernel_api.h"
#include "uda_user.h"
#include "pbl_uda_user.h"

#ifndef ASCEND_BACKUP_DEV_NUM
#define ASCEND_BACKUP_DEV_NUM 0
#endif

#define UDA_BUFF_LEN 32
#define MAX_REMOTE_UDEV_NUM (1124 + 16 * ASCEND_BACKUP_DEV_NUM)
#define UDA_HOST_ID 65

#define UDA_DEVICE_PATH "/dev/"
#define UDA_DEVICE_NAME "davinci"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#ifdef EMU_ST
int uda_file_access(const char *pathname, int flags);
int uda_file_open(const char *pathname, int flags);
int uda_file_close(int fd);
int uda_ioctl(int fd, unsigned long cmd, void *para);
#else
#define uda_file_access access
#define uda_file_open open
#define uda_file_close close
#define uda_ioctl(fd, cmd, ...) ioctl((fd), (cmd), ##__VA_ARGS__)
#endif

static bool uda_is_init = false;
static int uda_dev_fd = -1;
static pid_t uda_cur_pid = -1;
static unsigned int uda_dev_num_davinci = 0;
static unsigned int uda_dev_num_kunpeng = 0;
static struct uda_user_info user_info;
static struct uda_logic_dev *logic_dev = NULL;
static pthread_mutex_t uda_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool uda_is_admin(void)
{
    return (user_info.admin_flag == 1);
}

static bool uda_is_in_local(void)
{
    return (user_info.local_flag == 1);
}

static bool uda_is_support_udev_mng(void)
{
    return (user_info.support_udev_mng == 1);
}

static uint32_t uda_get_max_dev_num(void)
{
    return uda_is_admin() ? user_info.max_udev_num : user_info.max_dev_num;
}

STATIC void uda_report_device_occupied(uint32_t devId)
{
    int ret = 0;
    char dev_id_str[16] = {0};
    const char *keys[] = {"deviceid"};
    const char *values[] = {dev_id_str};

    ret = snprintf_s(dev_id_str, sizeof(dev_id_str), sizeof(dev_id_str) - 1, "%u", devId);
    if (ret < 0) {
        return;
    }
    REPORT_PREDEFINED_ERR_MSG("EL0020", keys, values, 1UL);
}

static int uda_dev_access(uint32_t *dev_num)
{
    uint32_t i;
    int ret;

    *dev_num = 0;
    for (i = 0; i < user_info.max_udev_num; i++) {
        char name[UDA_BUFF_LEN];
        int fd;
        if (sprintf_s(name, UDA_BUFF_LEN, "%s%s%u", UDA_DEVICE_PATH, UDA_DEVICE_NAME, i) <= 0) {
            uda_err("Invalid name.\n");
            return DRV_ERROR_INVALID_VALUE;
        }
        if (uda_file_access(name, O_RDONLY) != 0) {
            continue;
        }

        fd = uda_file_open(name, O_RDONLY); /* open device set namespace */
        if (fd >= 0) {
            uda_file_close(fd);
            (*dev_num)++;
        } else {
            if (errno == EBUSY) {
                uda_report_device_occupied(i);
                ret = DRV_ERROR_RESOURCE_OCCUPIED;
            } else if (errno == ENODEV) {
                ret = DRV_ERROR_NO_DEVICE;
            } else {
                ret = DRV_ERROR_INNER_ERR;
            }
            uda_err("Dev open failed. (dev=%s; errno=%d; ret=%d)\n", name, errno, ret);
            return ret;
        }
    }

    return 0;
}

static int uda_char_dev_open(void)
{
    struct davinci_intf_open_arg arg;
    int ret;

    arg.device_id = 0;
    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, UDA_CHAR_DEV_NAME);
    if (ret != 0) {
        uda_err("Strcpy_s failed. (ret=%d)\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    uda_dev_fd = uda_file_open(davinci_intf_get_dev_path(), O_RDWR | O_CLOEXEC);
    if (uda_dev_fd < 0) {
        uda_err("Open failed. (fd=%d; errno=%d)\n", uda_dev_fd, errno);
        return DRV_ERROR_INNER_ERR;
    }

    ret = uda_ioctl(uda_dev_fd, DAVINCI_INTF_IOCTL_OPEN, &arg);
    if (ret != 0) {
        uda_err("Ioctrl failed. (fd=%d; errno=%d)\n", uda_dev_fd, errno);
        uda_file_close(uda_dev_fd);
        uda_dev_fd = -1;
        return DRV_ERROR_IOCRL_FAIL;
    }
    uda_cur_pid = getpid();

    uda_info("The fd and pid info: (fd=%d; pid=%d)\n", uda_dev_fd, uda_cur_pid);
    return DRV_ERROR_NONE;
}

static void uda_char_dev_close(void)
{
    struct davinci_intf_close_arg arg;
    int ret;

    arg.device_id = 0;
    ret = strcpy_s(arg.module_name, DAVINIC_MODULE_NAME_MAX, UDA_CHAR_DEV_NAME);
    if (ret != 0) {
        uda_err("Strcpy_s failed. (ret=%d)\n", ret);
        return;
    }

    ret = uda_ioctl(uda_dev_fd, DAVINCI_INTF_IOCTL_CLOSE, &arg);
    if (ret != 0) {
        uda_err("Ioctl failed. (fd=%d; errno=%d)\n", uda_dev_fd, errno);
    }

    uda_file_close(uda_dev_fd);
    uda_dev_fd = -1;

    return;
}

static int uda_cmd_ioctl(unsigned long cmd, void *para)
{
    int ret;

    (void)pthread_mutex_lock(&uda_fd_mutex);
    if (uda_dev_fd < 0 || uda_cur_pid != getpid()) {
        ret = uda_char_dev_open();
        if (ret != 0) {
            (void)pthread_mutex_unlock(&uda_fd_mutex);
            return ret;
        }
    }
    (void)pthread_mutex_unlock(&uda_fd_mutex);

    do {
        ret = uda_ioctl(uda_dev_fd, cmd, para);
    } while ((ret == -1) && (errno == EINTR));

    if (ret < 0) {
        if (errno == EBUSY) {
            ret = DRV_ERROR_RESOURCE_OCCUPIED;
        } else if (errno == ENODEV) {
            ret = DRV_ERROR_NO_DEVICE;
        } else if (errno == ENOMEM) {
            ret = DRV_ERROR_OUT_OF_MEMORY;
        } else if (errno == EOPNOTSUPP) {
            ret = DRV_ERROR_NOT_SUPPORT;
        } else {
            ret = DRV_ERROR_IOCRL_FAIL;
        }
        share_log_read_err(HAL_MODULE_TYPE_DEV_MANAGER);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static int uda_get_user_info(void)
{
    return uda_cmd_ioctl(UDA_GET_USER_INFO, &user_info);
}

static int uda_init_dev_table(uint32_t dev_num)
{
    struct uda_setup_table info;

    info.dev_num = dev_num;
    return uda_cmd_ioctl(UDA_SETUP_DEV_TABLE, &info);
}

static int uda_get_dev_list(uint32_t start_devid, uint32_t end_devid)
{
    struct uda_dev_list info;

    info.start_devid = start_devid;
    info.end_devid = end_devid;
    info.logic_dev = logic_dev;
    return uda_cmd_ioctl(UDA_GET_DEV_LIST, &info);
}

static int uda_trans_devid(unsigned long cmd, uint32_t raw_devid, uint32_t *trans_devid)
{
    struct uda_devid_trans info;
    int ret;

    info.raw_devid = raw_devid;

    ret = uda_cmd_ioctl(cmd, &info);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    *trans_devid = info.trans_devid;

    return DRV_ERROR_NONE;
}

static uint32_t uda_get_dev_num_from_dev_list(struct uda_logic_dev *dev, uint32_t max_dev_num, uint32_t hw_type)
{
    uint32_t dev_num = 0, i;

    for (i = 0; i < max_dev_num; i++) {
        if ((dev[i].valid == 1) && (dev[i].hw_type == hw_type)) {
            dev_num++;
        }
    }

    return dev_num;
}

STATIC int _uda_init(void)
{
    uint32_t access_dev_num, dev_num, i;
    int ret;

    ret = uda_get_user_info();
    if (ret != 0) {
        uda_err("Get user info failed. (ret=%d)\n", ret);
        return ret;
    }

    if (!uda_is_admin()) {
        ret = uda_dev_access(&access_dev_num);
        if (ret != 0) {
            return ret;
        }
    } else {
        access_dev_num = user_info.max_dev_num;
    }

    dev_num = uda_get_max_dev_num();
    if (logic_dev == NULL) {
        logic_dev = malloc(dev_num * sizeof(struct uda_logic_dev));
        if (logic_dev == NULL) {
            uda_err("Alloc logic devctx failed. (dev_num=%u; errno=%d).\n", dev_num, errno);
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    for (i = 0; i < dev_num; i++) {
        logic_dev[i].valid = 0;
    }

    ret = uda_init_dev_table(access_dev_num);
    if (ret != 0) {
        uda_err("Init dev table failed. (ret=%d; access_dev_num=%u)\n", ret, access_dev_num);
        return ret;
    }

    ret = uda_get_dev_list(0, dev_num);
    if (ret != 0) {
        uda_err("Get dev list failed. (ret=%d)\n", ret);
        return ret;
    }

    uda_dev_num_davinci = uda_get_dev_num_from_dev_list(logic_dev, user_info.max_dev_num, UDA_HW_DAVINCI);
    uda_dev_num_kunpeng = uda_get_dev_num_from_dev_list(logic_dev, user_info.max_dev_num, UDA_HW_KUNPENG);

    uda_info("User info. (admin_flag=%u; local_flag=%u; max_dev_num=%u; max_udev_num=%u; "
             "uda_dev_num_davinci=%u; uda_dev_num_kunpeng=%u)\n",
             user_info.admin_flag, user_info.local_flag, user_info.max_dev_num, user_info.max_udev_num,
             uda_dev_num_davinci, uda_dev_num_kunpeng);

    return 0;
}

static int uda_init(void)
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret = 0;

    if (uda_is_init) {
        return 0;
    }

    (void)pthread_mutex_lock(&init_mutex);
    if (!uda_is_init) {
        ret = _uda_init();
        if (ret == 0) {
            uda_is_init = true;
        }
    }
    (void)pthread_mutex_unlock(&init_mutex);

    return ret;
}

void uda_uninit(void) /* It cannot be defined as static because of emulation tests. */
{
    (void)pthread_mutex_lock(&uda_fd_mutex);
    if (uda_dev_fd >= 0) {
        if (uda_cur_pid == getpid()) {
            uda_char_dev_close();
        }
        uda_dev_fd = -1;
    }
    (void)pthread_mutex_unlock(&uda_fd_mutex);

    uda_is_init = false;
}

static void __attribute__((constructor(101))) uda_user_init(void)
{
#ifdef CFG_FEATURE_UDA_CONSTRUCT_INIT
    (void)uda_init();
#endif
}

static void __attribute__((destructor)) uda_user_uninit(void)
{
    uda_uninit();
}

static void uda_get_dev_IDs(struct uda_logic_dev *dev, uint32_t max_dev_num, uint32_t hw_type, uint32_t *id,
                            uint32_t idLen)
{
    uint32_t i, num = 0;

    for (i = 0; ((i < max_dev_num) && (num < idLen)); i++) { /* [start, end) */
        if ((dev[i].valid == 1) && (dev[i].hw_type == hw_type)) {
            id[num] = dev[i].devid;
            num++;
        }
    }
}

int uda_user_get_dev_num(uint32_t *devNum)
{
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if (devNum == NULL) {
        uda_err("Null ptr\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    *devNum = uda_dev_num_davinci;

    return DRV_ERROR_NONE;
}

int uda_user_get_dev_ids(uint32_t *devices, uint32_t len)
{
    int ret;

    if (devices == NULL) {
        uda_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    uda_get_dev_IDs(logic_dev, user_info.max_dev_num, UDA_HW_DAVINCI, devices, len);

    return DRV_ERROR_NONE;
}

int uda_user_get_dev_num_ex(uint32_t hw_type, uint32_t *devNum)
{
    int ret;

    if (devNum == NULL) {
        uda_err("Null ptr\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if (hw_type == UDA_HW_DAVINCI) {
        *devNum = uda_dev_num_davinci;
    } else if (hw_type == UDA_HW_KUNPENG) {
        *devNum = uda_dev_num_kunpeng;
    } else {
        return DRV_ERROR_NOT_SUPPORT;
    }

    return DRV_ERROR_NONE;
}

int uda_user_get_dev_ids_ex(uint32_t hw_type, uint32_t *devices, uint32_t len)
{
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if ((devices == NULL) || (len == 0)) {
        uda_err("Null ptr or len is zero. (len=%u)\n", len);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (hw_type > UDA_HW_KUNPENG) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    uda_get_dev_IDs(logic_dev, user_info.max_dev_num, hw_type, devices, len);

    return DRV_ERROR_NONE;
}

int uda_user_get_vdev_num(uint32_t *devNum)
{
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if (devNum == NULL) {
        uda_err("Null ptr\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!uda_is_admin()) {
        *devNum = 0;
        return DRV_ERROR_NONE;
    }

    ret = uda_get_dev_list(user_info.max_dev_num, user_info.max_udev_num);
    if (ret != 0) {
        uda_err("Get dev list failed. (ret=%d)\n", ret);
        return ret;
    }

    *devNum = uda_get_dev_num_from_dev_list(&logic_dev[user_info.max_dev_num],
                                            user_info.max_udev_num - user_info.max_dev_num, UDA_HW_DAVINCI);

    return DRV_ERROR_NONE;
}

int uda_user_get_vdev_ids(uint32_t *devices, uint32_t len)
{
    int ret;

    if (devices == NULL) {
        uda_err("Null ptr.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if (!uda_is_admin()) {
        return DRV_ERROR_NONE;
    }

    ret = uda_get_dev_list(user_info.max_dev_num, user_info.max_udev_num);
    if (ret != 0) {
        uda_err("Get dev list failed. (ret=%d)\n", ret);
        return ret;
    }

    uda_get_dev_IDs(&logic_dev[user_info.max_dev_num], user_info.max_udev_num - user_info.max_dev_num, UDA_HW_DAVINCI,
                    devices, len);

    return DRV_ERROR_NONE;
}

STATIC int uda_get_phy_devid_by_devid(uint32_t devid, uint32_t *phy_devid)
{
    if (logic_dev[devid].valid == 0) {
        uda_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    *phy_devid = logic_dev[devid].phy_devid;
    return DRV_ERROR_NONE;
}

STATIC int uda_get_devid_by_phy_devid(uint32_t phy_devid, uint32_t *devid)
{
    uint32_t i, max_dev_num;

    max_dev_num = uda_get_max_dev_num();
    for (i = 0; i < max_dev_num; i++) {
        if ((logic_dev[i].valid == 1) && (logic_dev[i].phy_devid == phy_devid)) {
            *devid = logic_dev[i].devid;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_INVALID_VALUE;
}

int uda_get_udevid_by_devid(uint32_t devid, uint32_t *udevid)
{
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if ((udevid == NULL) || (devid >= uda_get_max_dev_num())) {
        uda_err("Null ptr or invalid devid. (devid=%u; max_dev_num=%u)\n", devid, uda_get_max_dev_num());
        return DRV_ERROR_INVALID_VALUE;
    }

    if (logic_dev[devid].valid == 0) {
        uda_err("Invalid devid. (devid=%u)\n", devid);
        return DRV_ERROR_INVALID_VALUE;
    }

    *udevid = logic_dev[devid].udevid;
    return DRV_ERROR_NONE;
}

int uda_get_devid_by_udevid(uint32_t udevid, uint32_t *devid)
{
    uint32_t i, max_dev_num;
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if (devid == NULL) {
        uda_err("Null ptr\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    max_dev_num = uda_get_max_dev_num();
    for (i = 0; i < max_dev_num; i++) {
        if ((logic_dev[i].valid == 1) && (logic_dev[i].udevid == udevid)) {
            *devid = logic_dev[i].devid;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_INVALID_VALUE;
}

int uda_get_devid_by_mia_dev(uint32_t phy_devid, uint32_t sub_devid, uint32_t *devid)
{
    uint32_t i, max_dev_num;
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if (devid == NULL) {
        uda_err("Null ptr\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    max_dev_num = uda_get_max_dev_num();
    for (i = user_info.max_dev_num; i < max_dev_num; i++) {
        if ((logic_dev[i].valid == 1) && (logic_dev[i].phy_devid == phy_devid) &&
            (logic_dev[i].sub_devid == sub_devid)) {
            *devid = logic_dev[i].devid;
            return DRV_ERROR_NONE;
        }
    }

    return DRV_ERROR_INVALID_VALUE;
}

int uda_user_get_phy_id_by_index(uint32_t devid, uint32_t *phyId)
{
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if ((phyId == NULL) || (devid >= uda_get_max_dev_num())) {
        uda_err("Null ptr or invalid devid. (devid=%u; max_dev_num=%u)\n", devid, uda_get_max_dev_num());
        return DRV_ERROR_INVALID_VALUE;
    }

    /* milan return udevid, obp return phy devid */
    if (uda_is_support_udev_mng()) {
        if (devid < user_info.max_dev_num) {
            return (drvError_t)uda_get_udevid_by_devid(devid, phyId);
        } else {
            /* The management process queries the information about the mia device from the physical machine.
            The mia device may be added or deleted. Therefore, the management process needs to query the information
            from the kernel in real time. */
            return uda_trans_devid(UDA_DEVID_TO_UDEVID, devid, phyId);
        }
    } else {
        return uda_get_phy_devid_by_devid(devid, phyId);
    }
}

#ifdef CFG_FEATURE_ASCEND950_STUB
static int uda_get_index_by_phyid(unsigned long cmd, uint32_t phyId, uint32_t *devid)
{
    uint32_t flag;
    int ret;
    ret = uda_cmd_ioctl(UDA_GET_RAW_PROC_IS_CONTAIN_FLAG, &flag);
    if ((ret == DRV_ERROR_NONE) && (flag == 1)) {
        return uda_get_devid_by_udevid(phyId, devid);
    }
    return uda_trans_devid(cmd, phyId, devid);
}

int uda_set_raw_proc_is_contain(uint32_t flag)
{
    int ret;

    ret = uda_cmd_ioctl(UDA_SET_RAW_PROC_IS_CONTAIN_FLAG, &flag);
    if (ret != DRV_ERROR_NONE) {
        uda_err("ioctl failed. (flag=%u, ret=%d)\n", flag, ret);
    }
    return ret;
}

int uda_get_raw_proc_is_contain(uint32_t *flag)
{
    int ret;

    if (flag == NULL) {
        uda_err("flag is NULL\n");
        return -EINVAL;
    }

    ret = uda_cmd_ioctl(UDA_GET_RAW_PROC_IS_CONTAIN_FLAG, flag);
    if (ret != DRV_ERROR_NONE) {
        uda_err("ioctl failed. (ret=%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
#endif

int uda_user_get_index_by_phy_id(uint32_t phyId, uint32_t *devid)
{
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if (devid == NULL) {
        uda_err("Null ptr\n");
        return DRV_ERROR_INVALID_VALUE;
    }
    if (phyId >= user_info.max_udev_num) {
        uda_err("phy id invalid.(phyid=%u;max_udev_num=%u)\n", phyId, user_info.max_udev_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    /* milan return udevid, obp return phy devid */
    if (uda_is_support_udev_mng()) {
        if ((!uda_is_admin()) || (phyId < user_info.max_dev_num)) {
            return uda_get_devid_by_udevid(phyId, devid);
        } else {
            /* The management process queries the information about the mia device from the physical machine.
            The mia device may be added or deleted. Therefore, the management process needs to query the information
            from the kernel in real time. */
#ifndef CFG_FEATURE_ASCEND950_STUB
            return uda_trans_devid(UDA_UDEVID_TO_DEVID, phyId, devid);
#else
            return uda_get_index_by_phyid(UDA_UDEVID_TO_DEVID, phyId, devid);
#endif
        }
    } else {
        return uda_get_devid_by_phy_devid(phyId, devid);
    }
}

int uda_user_get_device_local_ids(uint32_t *devices, uint32_t len)
{
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if (devices == NULL) {
        uda_err("Parameter devices is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!uda_is_in_local()) {
        return DRV_ERROR_PARA_ERROR;
    }

    return uda_user_get_dev_ids(devices, len);
}

int uda_user_get_devid_by_local_devid(uint32_t local_devid, uint32_t *remote_udevid)
{
    uint32_t local_udevid;
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if (remote_udevid == NULL || local_devid >= user_info.max_udev_num) {
        uda_err("Param invalid.(remote_udevid=%d; local_devid=%u; max=%u)\n", remote_udevid != NULL, local_devid,
                user_info.max_udev_num);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!uda_is_in_local()) {
        return DRV_ERROR_PARA_ERROR;
    }

    ret = uda_user_get_phy_id_by_index(local_devid, &local_udevid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return uda_trans_devid(UDA_LUDEVID_TO_RUDEVID, local_udevid, remote_udevid);
}

int uda_user_get_local_devid_by_host_devid(uint32_t remote_udevid, uint32_t *local_devid)
{
    uint32_t local_udevid;
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if (local_devid == NULL) {
        uda_err("Null ptr\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (remote_udevid >= MAX_REMOTE_UDEV_NUM) {
        uda_err("remote udevid invalid.(udevid=%u;max=%u)\n", remote_udevid, user_info.max_udev_num);
        return DRV_ERROR_INVALID_DEVICE;
    }

    if (!uda_is_in_local()) {
        return DRV_ERROR_PARA_ERROR;
    }

    ret = uda_trans_devid(UDA_RUDEVID_TO_LUDEVID, remote_udevid, &local_udevid);
    if (ret != DRV_ERROR_NONE) {
        return ret;
    }

    return uda_user_get_index_by_phy_id(local_udevid, local_devid);
}

int uda_user_get_phy_devid_by_logic_devid(unsigned int dev_id, unsigned int *phy_dev_id)
{
    int ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    if ((phy_dev_id == NULL) || (dev_id >= uda_get_max_dev_num())) {
        uda_err("Null ptr or invalid devid. (dev_id=%u; max_dev_num=%u)\n", dev_id, uda_get_max_dev_num());
        return DRV_ERROR_PARA_ERROR;
    }

    return uda_get_phy_devid_by_devid(dev_id, phy_dev_id);
}

int uda_dev_is_exist(unsigned int udevid, bool *is_exist)
{
    int ret;
    unsigned int max_num;
    unsigned int i;

    ret = uda_init();
    if (ret != 0) {
        return ret;
    }

    max_num = uda_get_max_dev_num();
    for (i = 0; i < max_num; i++) {
        if ((logic_dev[i].valid == 1) && (udevid == logic_dev[i].udevid)) {
            *is_exist = true;
            return 0;
        }
    }

    *is_exist = false;
    return 0;
}

int uda_user_get_host_id(uint32_t *host_id)
{
    if (host_id == NULL) {
        uda_err("Null ptr.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    *host_id = UDA_HOST_ID;
    return DRV_ERROR_NONE;
}

int uda_get_udevid_by_devid_ex(uint32_t devid, uint32_t *udevid)
{
    if (udevid == NULL) {
        uda_err("Null ptr.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (devid != UDA_HOST_ID) {
        return uda_get_udevid_by_devid(devid, udevid);
    } else {
        *udevid = devid;
    }

    return DRV_ERROR_NONE;
}

int uda_get_devid_by_udevid_ex(uint32_t udevid, uint32_t *devid)
{
    if (devid == NULL) {
        uda_err("Null ptr.\n");
        return DRV_ERROR_PARA_ERROR;
    }

    if (udevid != UDA_HOST_ID) {
        return uda_get_devid_by_udevid(udevid, devid);
    } else {
        *devid = udevid;
    }

    return DRV_ERROR_NONE;
}

STATIC int uda_init_restore(void)
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret = 0;

    if (uda_is_init) {
        return 0;
    }

    (void)pthread_mutex_lock(&init_mutex);
    ret = _uda_init();
    if (ret == 0) {
        uda_is_init = true;
    }
    (void)pthread_mutex_unlock(&init_mutex);

    return ret;
}

drvError_t udaCloseRestoreHandler(uint32_t devid, halDevCloseIn *in)
{
    (void)devid;
    (void)in;

    uda_uninit();
    (void)uda_init_restore();

    return 0;
}

int uda_user_get_udevid_by_devid(uint32_t devid, uint32_t *udevid)
{
    return uda_get_udevid_by_devid(devid, udevid);
}
int uda_user_get_devid_by_udevid(uint32_t udevid, uint32_t *devid)
{
    return uda_get_devid_by_udevid(udevid, devid);
}
int uda_user_get_devid_by_mia_dev(uint32_t phy_devid, uint32_t sub_devid, uint32_t *devid)
{
    return uda_get_devid_by_mia_dev(phy_devid, sub_devid, devid);
}
int uda_user_get_udevid_by_devid_ex(uint32_t devid, uint32_t *udevid)
{
    return uda_get_udevid_by_devid_ex(devid, udevid);
}
int uda_user_get_devid_by_udevid_ex(uint32_t udevid, uint32_t *devid)
{
    return uda_get_devid_by_udevid_ex(udevid, devid);
}