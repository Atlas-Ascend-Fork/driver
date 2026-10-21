/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "securec.h"
#include "mmpa_api.h"
#ifdef __linux
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#endif
#include "dms/dms_devdrv_info_comm.h"
#include "dms/dms_drv_internal.h"
#include "devmng_common.h"
#include "devdrv_ioctl.h"
#include "devdrv_user_common.h"
#include "dms_user_common.h"
#ifdef CFG_FEATURE_SUPPORT_DEVMNG_BBOX
#include "devmng_user.h"
#endif
#include "davinci_interface.h"
#include "devmng_user_common.h"
#include "dms_device_info.h"
#include "devdrv_pcie.h"
#include "ascend_dev_num.h"
#include "drv_devmng_adapt.h"
#include "dms/dms_soc_interface.h"

typedef enum {
    DRV_P2P_TYPE_ALL = 0,
    DRV_P2P_TYPE_MEM,
    DRV_P2P_TYPE_NOTIFY,
    DRV_P2P_TYPE_MAX
} drv_p2p_type;

#define I2C_24CXX_ADDR 0x20
#define WRITE_VALUE 0xCF
#define CMD_READ_PORT0 0x00
#define CMD_OUTPUT_PORT0 0x02
#define CMD_CFG_OUTPUT_PORT0 0x06
#define dev_name_smbus "SMBus I801 adapter"
#define DEV_NAME_LEN 64
#define i2c_dev_sys_dir "/sys/bus/i2c/devices/"
#ifdef __linux
#define fd_is_invalid(fd) ((fd) < 0)
#else
#define fd_is_invalid(fd) ((fd) == (mmProcess)DEVDRV_INVALID_FD_OR_INDEX)
#endif
#define PCIE_HOTREST_WITH_TASK_CNT 0
#define PCIE_HOTREST 1
#define KDUMP_TIMEOUT_TIMES 600
#define FLAG_ALL_DONE 5
#define PCIE_NUM (3)

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#ifdef __linux
STATIC int drv_reset_device_by_i2c(const char *I2C_DEV, int array_len)
{
    struct i2c_smbus_ioctl_data ioctl_data = {0};
    const unsigned long addr = I2C_24CXX_ADDR;
    union i2c_smbus_data data;
    unsigned char p01_value;
    mmIoctlBuf buf = {0};

    int fd = -1;
    int rc;

    if (I2C_DEV == NULL) {
        DEVDRV_DRV_ERR("I2C_DEV is NULL.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    fd = mmOpen2(I2C_DEV, M_RDWR, M_IRUSR);
    if (fd < 0) {
        DEVDRV_DRV_ERR("failed to open %s, fd(%d)\n", I2C_DEV, fd);
        return fd;
    }

    (void)array_len;
    buf.inbuf = (void *)((uintptr_t)addr);
    buf.inbufLen = sizeof(unsigned long);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;

    rc = dmanage_mmIoctl(fd, I2C_SLAVE, &buf);
    if (rc < 0) {
        DEVDRV_DRV_ERR("ioctl, I2C_SLAVE failed, rc(%d).\n", rc);
        (void)close(fd);
        fd = -1;
        return rc;
    }

    /* set port0 read */
    data.byte = 0xFF;
    ioctl_data.read_write = I2C_SMBUS_WRITE;
    ioctl_data.command = CMD_CFG_OUTPUT_PORT0;
    ioctl_data.size = I2C_SMBUS_BYTE_DATA;
    ioctl_data.data = &data;
    buf.inbuf = (void *)&ioctl_data;
    buf.inbufLen = sizeof(struct i2c_smbus_ioctl_data);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;
    rc = dmanage_mmIoctl(fd, I2C_SMBUS, &buf);
    if (rc < 0) {
        DEVDRV_DRV_ERR("write, I2C_SMBUS failed, rc(%d).\n", rc);
        (void)close(fd);
        fd = -1;
        return rc;
    }

    /* read port0 value */
    data.byte = 0x00;
    ioctl_data.read_write = I2C_SMBUS_READ;
    ioctl_data.command = CMD_READ_PORT0;
    ioctl_data.size = I2C_SMBUS_BYTE_DATA;
    ioctl_data.data = &data;

    buf.inbuf = (void *)&ioctl_data;
    buf.inbufLen = sizeof(struct i2c_smbus_ioctl_data);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;

    rc = dmanage_mmIoctl(fd, I2C_SMBUS, &buf);
    if (rc < 0) {
        DEVDRV_DRV_ERR("write, I2C_SMBUS failed, rc(%d).\n", rc);
        (void)close(fd);
        fd = -1;
        return rc;
    }
    p01_value = data.byte;

    p01_value &= 0xfd;

    data.byte = 0x00;
    ioctl_data.read_write = I2C_SMBUS_WRITE;
    ioctl_data.command = CMD_CFG_OUTPUT_PORT0;
    ioctl_data.size = I2C_SMBUS_BYTE_DATA;
    ioctl_data.data = &data;

    buf.inbuf = (void *)&ioctl_data;
    buf.inbufLen = sizeof(struct i2c_smbus_ioctl_data);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;

    rc = dmanage_mmIoctl(fd, I2C_SMBUS, &buf);
    if (rc < 0) {
        DEVDRV_DRV_ERR("write, I2C_SMBUS failed, rc(%d).\n", rc);
        (void)close(fd);
        fd = -1;
        return rc;
    }
    data.byte = p01_value;
    ioctl_data.read_write = I2C_SMBUS_WRITE;
    ioctl_data.command = CMD_OUTPUT_PORT0;
    ioctl_data.size = I2C_SMBUS_BYTE_DATA;
    ioctl_data.data = &data;

    buf.inbuf = (void *)&ioctl_data;
    buf.inbufLen = sizeof(struct i2c_smbus_ioctl_data);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;
    rc = dmanage_mmIoctl(fd, I2C_SMBUS, &buf);
    if (rc < 0) {
        DEVDRV_DRV_ERR("write, I2C_SMBUS failed, rc(%d).\n", rc);
        (void)close(fd);
        fd = -1;
        return rc;
    }

    (void)sleep(1);

    p01_value |= 0x02;
    data.byte = p01_value;
    ioctl_data.read_write = I2C_SMBUS_WRITE;
    ioctl_data.command = CMD_OUTPUT_PORT0;
    ioctl_data.size = I2C_SMBUS_BYTE_DATA;
    ioctl_data.data = &data;

    buf.inbuf = (void *)&ioctl_data;
    buf.inbufLen = sizeof(struct i2c_smbus_ioctl_data);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;
    rc = dmanage_mmIoctl(fd, I2C_SMBUS, &buf);
    if (rc < 0) {
        DEVDRV_DRV_ERR("write, I2C_SMBUS failed, rc(%d).\n", rc);
        (void)close(fd);
        fd = -1;
        return rc;
    }

    (void)close(fd);
    fd = -1;

    return DRV_ERROR_NONE;
}

STATIC int drv_get_i2c_dev_count(const char *root)
{
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    int i2c_index_max = 0;
    int i2c_index_tmp = 0;
    char dev_name[DEV_NAME_LEN] = {0};
    char filename[DEV_NAME_LEN] = {0};
    int ret = 0;
    int fd = -1;
    int errno_tmp;

    dir = opendir(root);
    if (dir == NULL) {
        DEVDRV_DRV_ERR("fail to open dir");
        return DRV_ERROR_INVALID_HANDLE;
    }

    ptr = readdir(dir);
    while (ptr != NULL) {
        /* skip . and .. dir */
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) {
            ptr = readdir(dir);
            continue;
        }
        DEVDRV_DRV_DEBUG("%s%s\n", root, ptr->d_name);
        ret = sscanf_s(ptr->d_name, "i2c-%d", &i2c_index_tmp);
        if (ret < 0) {
            DEVDRV_DRV_ERR("sscanf_s failed, ret(%d).\n", ret);
            (void)closedir(dir);
            dir = NULL;
            return ret;
        } else if (ret == 0) {
            ptr = readdir(dir);
            continue;
        }

        i2c_index_max = (i2c_index_max > i2c_index_tmp) ? i2c_index_max : i2c_index_tmp;
        ptr = readdir(dir);
    }
    (void)closedir(dir);
    dir = NULL;

    /* read name */
    ret = sprintf_s(filename, DEV_NAME_LEN, "/sys/bus/i2c/devices/i2c-%d/name", i2c_index_max);
    if (ret < 0) {
        DEVDRV_DRV_ERR("sprintf_s failed, ret(%d).\n", ret);
        return ret;
    }

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        errno_tmp = errno;
        DEVDRV_DRV_ERR("open i2c dev error:%s, (errno=%d).\n", filename, errno_tmp);
        return fd;
    }

    do {
        ret = read(fd, dev_name, strlen(dev_name_smbus));
        if ((ret == -1) && (errno != EINTR)) {
            DEVDRV_DRV_ERR("read i2c dev name failed, ret(%d).\n", ret);
            (void)close(fd);
            fd = -1;
            return ret;
        }
    } while ((ret == -1) && (errno == EINTR));
    (void)close(fd);
    fd = -1;
    dev_name[DEV_NAME_LEN - 1] = 0;
    /* check name */
    ret = strncmp(dev_name, dev_name_smbus, strlen(dev_name_smbus));
    if (ret != 0) {
        DEVDRV_DRV_ERR("read [%s], check i2c dev fail:%d,got:%s.\n", filename, ret, dev_name);
        return ret;
    }
    DEVDRV_DRV_DEBUG("read [%s], find i2c-%d name dev_name:%s\n", filename, i2c_index_max, dev_name);

    return i2c_index_max;
}
#endif
/*
HP PC + EVB:SMBus I2C cmd->pca6416, Port:0,GPIO:1
server + EVB:BMC I2C cmd->pca6416
HP PC + pcie card:SMBus I2C cmd->mcu
server + pcie card:BMC I2C cmd->mcu
hi35xx + mini:GPIO
*/
drvError_t drvResetDevice(uint32_t devId)
{
#ifdef __linux
    int i2c_index_max;
    char dev_name[DEV_NAME_LEN] = {0};
    int ret;
    (void)(devId);

    i2c_index_max = drv_get_i2c_dev_count(i2c_dev_sys_dir);
    if (i2c_index_max == -1) {
        DEVDRV_DRV_ERR("find i2c dev error\n");
        return DRV_ERROR_INVALID_VALUE;
    } else {
        DEVDRV_DRV_DEBUG("/sys/bus/i2c/devices/ max i2c dev: i2c-%d\n", i2c_index_max);
    }

    ret = sprintf_s(dev_name, DEV_NAME_LEN, "/dev/i2c-%d", i2c_index_max);
    if (ret < 0) {
        DEVDRV_DRV_ERR("sprintf_s return error: %d.\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    DEVDRV_DRV_DEBUG("send I2C cmd reset:0,i2c dev:%s\n", dev_name);

    /* HP PC + EVB */
    if (drv_reset_device_by_i2c(dev_name, DEV_NAME_LEN) != 0) {
        DEVDRV_DRV_ERR("Error to reset device by i2c.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

#else
    mmIoctlBuf buf = {0};
    ULONG data = 0;
    int ret;
    mmProcess fd = -1;

    if (devId >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", devId);
        return DRV_ERROR_INVALID_DEVICE;
    }
    fd = devdrv_open_device_manager();
    if (fd_is_invalid(fd)) {
        DEVDRV_DRV_ERR("open device manager failed, fd(%d). devid(%u)\n", fd, devId);
        return DRV_ERROR_INVALID_DEVICE;
    }

    buf.inbuf = (void *)&data;
    buf.inbufLen = sizeof(data);
    buf.outbuf = (void *)&data;
    buf.outbufLen = sizeof(data);
    buf.oa = NULL;

    ret = dmanage_mmIoctl(fd, DEVDRV_MANAGER_RESET_DEVICE, &buf);
    if (ret) {
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", devId, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
#endif

    return DRV_ERROR_NONE;
}

#ifdef CFG_FEATURE_SUPPORT_DEVMNG_BBOX
STATIC drvError_t drvPcieRead(uint32_t devId, uint32_t offset, uint8_t *value, uint32_t len,
                              enum devdrv_pcie_read_type type)
{
    struct devdrv_pcie_read_para pcie_read_para = {0};
    uint32_t i;
    int ret;
    mmProcess fd = -1;
    mmIoctlBuf buf = {0};
    if (devId >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u). type %d.\n", devId, type);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (value == NULL) {
        DEVDRV_DRV_ERR("value is NULL. (dev_id=%u; type=%d)\n", devId, type);
        return DRV_ERROR_INVALID_HANDLE;
    }
    if ((len == 0) || (len > sizeof(pcie_read_para.value))) {
        DEVDRV_DRV_ERR("invalid len(%u). devid(%u). type %d.\n", len, devId, type);
        return DRV_ERROR_INVALID_VALUE;
    }

    fd = devdrv_open_device_manager();
    if (fd_is_invalid(fd)) {
        DEVDRV_DRV_ERR("open device manager failed, fd(%d). devid(%u). type %d.\n", fd, devId, type);
        return DRV_ERROR_INVALID_HANDLE;
    }
    pcie_read_para.devId = devId;
    pcie_read_para.offset = offset;
    pcie_read_para.len = len;
    pcie_read_para.type = type;
    buf.inbuf = (void *)&pcie_read_para;
    buf.inbufLen = sizeof(struct devdrv_pcie_read_para);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;

    ret = dmanage_mmIoctl(fd, DEVDRV_MANAGER_PCIE_READ, &buf);
    if (ret != 0) {
        if (errno == EOPNOTSUPP) {
            return DRV_ERROR_NOT_SUPPORT;
        }
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), type %d, ret(%d).\n", devId, type, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    for (i = 0; i < len; i++) {
        value[i] = pcie_read_para.value[i];
    }

    return DRV_ERROR_NONE;
}

STATIC drvError_t drv_pcie_read_log_dump(uint32_t devId, uint32_t offset, uint8_t *value, uint32_t len,
                                         enum devdrv_pcie_read_type type)
{
#ifdef CFG_FEATURE_LOG_DUMP_FROM_PCIE
    int ret;
    struct urd_ioctl_arg ioarg = {0};
    struct devdrv_bbox_pcie_logdump bbox_pcie_log_dump = {0};

    if (value == NULL) {
        DEVDRV_DRV_ERR("Buffer is NULL. (devid=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    if (devId >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("Invalid device id. (devid=%u, max_devid=%d).\n", devId, ASCEND_DEV_MAX_NUM);
        return DRV_ERROR_PARA_ERROR;
    }

    bbox_pcie_log_dump.devid = devId;
    bbox_pcie_log_dump.len = len;
    bbox_pcie_log_dump.offset = offset;
    bbox_pcie_log_dump.buff = value;
    bbox_pcie_log_dump.type = type;

    ioarg.devid = devId;
    ioarg.cmd.main_cmd = DMS_MAIN_CMD_BBOX;
    ioarg.cmd.sub_cmd = DMS_SUBCMD_GET_PCIE_LOG_DUMP_INFO;
    ioarg.cmd.filter = NULL;
    ioarg.cmd.filter_len = 0;
    ioarg.cmd_para.input = (void *)&bbox_pcie_log_dump;
    ioarg.cmd_para.input_len = sizeof(struct devdrv_bbox_pcie_logdump);
    ioarg.cmd_para.output = NULL;
    ioarg.cmd_para.output_len = 0;

    ret = DmsIoctlConvertErrno(DMS_IOCTL_CMD, &ioarg);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "drv_dev_log_dump failed. (devid=%u, ret=%d)", devId, ret);
    }

    return ret;
#else
    (void)devId;
    (void)offset;
    (void)value;
    (void)len;
    (void)type;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t drv_pcie_sram_read(uint32_t devId, uint32_t offset, uint8_t *value, uint32_t len)
{
    return drvPcieRead(devId, offset, value, len, DEVDRV_PCIE_READ_TYPE_SRAM);
}

drvError_t drv_pcie_ddr_read(uint32_t devId, uint32_t offset, uint8_t *value, uint32_t len)
{
    return drvPcieRead(devId, offset, value, len, DEVDRV_PCIE_READ_TYPE_DDR);
}

drvError_t drv_pcie_bbox_hdr_read(uint32_t devId, uint32_t offset, uint8_t *value, uint32_t len)
{
    return drvPcieRead(devId, offset, value, len, DEVDRV_PCIE_READ_TYPE_HDR);
}

drvError_t drv_reg_sram_read(uint32_t devId, uint32_t offset, uint8_t *value, uint32_t len)
{
    return drvPcieRead(devId, offset, value, len, DEVDRV_PCIE_READ_TYPE_REG_SRAM);
}

STATIC drvError_t drv_hboot_sram_read(uint32_t devId, uint32_t offset, uint8_t *value, uint32_t len)
{
    return drvPcieRead(devId, offset, value, len, DEVDRV_PCIE_READ_TYPE_HBOOT_SRAM);
}

STATIC drvError_t drv_vmcore_stat_read(uint32_t devId, uint32_t offset, uint8_t *value, uint32_t len)
{
#ifdef CFG_FEATURE_BBOX_KDUMP
    drvError_t ret;
    unsigned int read_times = 0;

    if (value == NULL) {
        DEVDRV_DRV_ERR("Value is NULL. (devid=%u)\n", devId);
        return DRV_ERROR_PARA_ERROR;
    }

    while ((*value) != FLAG_ALL_DONE) {
        if (read_times >= KDUMP_TIMEOUT_TIMES) {
            ret = drvPcieRead(devId, offset, value, len, DEVDRV_PCIE_READ_TYPE_VMCORE_STAT);
            if (ret != DRV_ERROR_NONE) {
                return ret;
            }
            return DRV_ERROR_NONE;
        }

        ret = drvPcieRead(devId, offset, value, len, DEVDRV_PCIE_READ_TYPE_VMCORE_STAT);
        if (ret != DRV_ERROR_NONE) {
            return ret;
        }

        read_times++;
        (void)sleep(1);
    }
    return DRV_ERROR_NONE;
#else
    (void)devId;
    (void)offset;
    (void)value;
    (void)len;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}
#endif

drvError_t drvMemRead(uint32_t devId, MEM_CTRL_TYPE mem_type, uint32_t offset, uint8_t *value, uint32_t len)
{
#ifdef CFG_FEATURE_SUPPORT_DEVMNG_BBOX
    switch (mem_type) {
        case MEM_TYPE_PCIE_SRAM:
            return drv_pcie_sram_read(devId, offset, value, len);
        case MEM_TYPE_PCIE_DDR:
            return drv_pcie_ddr_read(devId, offset, value, len);
        case MEM_TYPE_IMU_DDR:
#ifdef CFG_FEATURE_IMU_DDR_READ
            return drvPcieIMUDDRRead(devId, offset, value, len);
#else
            return DRV_ERROR_NOT_SUPPORT;
#endif
        case MEM_TYPE_BBOX_DDR:
            return drv_device_memory_dump(devId, offset, len, value);
        case MEM_TYPE_BBOX_HDR:
            return drv_pcie_bbox_hdr_read(devId, offset, value, len);
        case MEM_TYPE_REG_SRAM:
            return drv_reg_sram_read(devId, offset, value, len);
        case MEM_TYPE_REG_DDR:
            return drv_reg_ddr_read(devId, offset, len, value);
        case MEM_TYPE_TS_LOG:
            return drv_ts_log_dump(devId, offset, len, value);
        case MEM_TYPE_HBOOT_SRAM:
            return drv_hboot_sram_read(devId, offset, value, len);
        case MEM_TYPE_RUN_OS_LOG:
            return drv_dev_log_dump(devId, offset, len, value, DEVDRV_DEV_LOG_DUMP_RUN_OS);
        case MEM_TYPE_DEBUG_OS_LOG:
            return drv_dev_log_dump(devId, offset, len, value, DEVDRV_DEV_LOG_DUMP_DEBUG_OS);
        case MEM_TYPE_DEBUG_DEV_LOG:
            return drv_dev_log_dump(devId, offset, len, value, DEVDRV_DEV_LOG_DUMP_DEBUG_DEV);
        case MEM_TYPE_RUN_EVENT_LOG:
            return drv_dev_log_dump(devId, offset, len, value, DEVDRV_DEV_LOG_DUMP_RUN_EVENT);
        case MEM_TYPE_SEC_LOG:
            return drv_dev_log_dump(devId, offset, len, value, DEVDRV_DEV_LOG_DUMP_SEC);
        case MEM_TYPE_VMCORE_FILE:
            return drv_vmcore_dump(devId, offset, len, value);
        case MEM_TYPE_VMCORE_STAT:
            return drv_vmcore_stat_read(devId, offset, value, len);
        case MEM_TYPE_CHIP_LOG_PCIE_BAR:
            return drv_pcie_read_log_dump(devId, offset, value, len, DEVDRV_PCIE_READ_TYPE_CHIP_DFX_LOG);
        case MEM_TYPE_TS_LOG_PCIE_BAR:
            return drv_pcie_read_log_dump(devId, offset, value, len, DEVDRV_PCIE_READ_TYPE_TS_LOG);
        case MEM_TYPE_BBOX_PCIE_BAR:
            return drv_pcie_read_log_dump(devId, offset, value, len, DEVDRV_PCIE_READ_TYPE_BBOX_DDR_LOG);
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }
#else
    (void)devId;
    (void)mem_type;
    (void)offset;
    (void)value;
    (void)len;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

#ifdef CFG_FEATURE_BBOX_KDUMP
STATIC drvError_t drvPcieWrite(uint32_t devId, uint32_t offset, uint8_t *value, uint32_t len,
                               enum devdrv_pcie_write_type type)
{
    struct devdrv_pcie_write_para pcie_write_para = {0};
    uint32_t i;
    int ret;
    int err_buf;
    mmProcess fd = -1;
    mmIoctlBuf buf = {0};

    if (devId >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("Invalid device id. (dev_id=%u; max_dev_num=%u; type=%d)\n", devId, ASCEND_DEV_MAX_NUM, type);
        return DRV_ERROR_PARA_ERROR;
    }

    if (value == NULL) {
        DEVDRV_DRV_ERR("value is NULL. (dev_id=%u; type=%d)\n", devId, type);
        return DRV_ERROR_PARA_ERROR;
    }

    if ((len == 0) || (len > DEVDRV_VALUE_SIZE)) {
        DEVDRV_DRV_ERR("Invalid length. (len=%u; max_len=%u; dev_id=%u; type=%d)\n", len, DEVDRV_VALUE_SIZE, devId,
                       type);
        return DRV_ERROR_PARA_ERROR;
    }

    fd = devdrv_open_device_manager();
    if (fd_is_invalid(fd)) {
        DEVDRV_DRV_ERR("Open device manager failed. (fd=%d; dev_id=%u; type=%d)\n", fd, devId, type);
        return DRV_ERROR_INVALID_HANDLE;
    }
    pcie_write_para.devId = devId;
    pcie_write_para.offset = offset;
    pcie_write_para.len = len;
    pcie_write_para.type = type;

    for (i = 0; i < len; i++) {
        pcie_write_para.value[i] = value[i];
    }

    buf.inbuf = (void *)&pcie_write_para;
    buf.inbufLen = sizeof(struct devdrv_pcie_write_para);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;

    ret = dmanage_mmIoctl(fd, DEVDRV_MANAGER_PCIE_WRITE, &buf);
    if (ret != 0) {
        err_buf = errno;
        ret = errno_to_user_errno(err_buf);
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Ioctl failed. (devid=%u; type=%u; ret=%d; errno=%d)\n", devId, type, ret,
                                     err_buf);
        return ret;
    }

    return DRV_ERROR_NONE;
}
#endif

#ifdef CFG_FEATURE_BBOX_KDUMP
STATIC drvError_t drv_pcie_hbm_write(uint32_t devId, uint32_t offset, uint8_t *value, uint32_t len)
{
    return drvPcieWrite(devId, offset, value, len, DEVDRV_PCIE_WRITE_TYPE_KDUMP);
}
#endif

drvError_t drvMemWrite(uint32_t devId, MEM_CTRL_TYPE mem_type, uint32_t offset, uint8_t *value, uint32_t len)
{
#ifdef CFG_FEATURE_BBOX_KDUMP
    switch (mem_type) {
        case MEM_TYPE_KDUMP_MAGIC:
            return drv_pcie_hbm_write(devId, offset, value, len);
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }
#else
    (void)devId;
    (void)mem_type;
    (void)offset;
    (void)value;
    (void)len;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t drvDmaMmap(uint32_t devId, uint64_t vir_addr, uint32_t *size)
{
    (void)devId;
    (void)vir_addr;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}

#ifdef CFG_FEATURE_DEVMNG_IOCTL
STATIC drvError_t drv_get_device_boot_status_ex(int phy_id, uint32_t *boot_status)
{
    int ret;
    mmProcess fd = -1;
    mmIoctlBuf buf = {0};
    struct devdrv_get_device_boot_status_para get_device_boot_status_para = {0};

    if ((uint32_t)phy_id >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%d).\n", phy_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (boot_status == NULL) {
        DEVDRV_DRV_ERR("boot_status is NULL. devid(%d).\n", phy_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    get_device_boot_status_para.devId = (uint32_t)phy_id;
    fd = devdrv_open_device_manager();
    if (fd_is_invalid(fd)) {
        DEVDRV_DRV_ERR("open device manager failed, fd(%d). devid(%d)\n", fd, phy_id);
        return DRV_ERROR_INVALID_HANDLE;
    }
    buf.inbuf = (void *)&get_device_boot_status_para;
    buf.inbufLen = sizeof(struct devdrv_get_device_boot_status_para);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;
    ret = dmanage_mmIoctl(fd, DEVDRV_MANAGER_GET_DEVICE_BOOT_STATUS, &buf);
    if (ret != 0) {
        ret = mm_get_error_code();
        if (ret == ENXIO) {
            DEVDRV_DRV_WARN("no such device, devid(%d), ret(%d).\n", phy_id, ret);
        } else {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "ioctl failed, devid(%d), ret(%d).\n", phy_id, ret);
            return errno_to_user_errno(-ret);
        }
    }

    *boot_status = get_device_boot_status_para.boot_status;

    return DRV_ERROR_NONE;
}
#endif

/* get device boot status */
drvError_t drvGetDeviceBootStatus(int phy_id, uint32_t *boot_status)
{
#ifdef CFG_FEATURE_DEVMNG_IOCTL
    return drv_get_device_boot_status_ex(phy_id, boot_status);
#elif defined CFG_FEATURE_NOT_SUPPORT_DEVICE_BOOT_STATUS
    return DRV_ERROR_NOT_SUPPORT;
#else
    return DmsGetDevBootStatus(phy_id, boot_status);
#endif
}

#ifdef __linux
/* get flag of virtual mathine or physical mathine in host */
drvError_t drvGetHostPhyMachFlag(unsigned int device_id, unsigned int *host_flag)
{
    struct devdrv_get_host_phy_mach_flag_para flag_para = {0};
    int ret;
    int fd = -1;

    if (device_id >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid device id %d.\n", device_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (host_flag == NULL) {
        DEVDRV_DRV_ERR("host_flag is NULL. devid(%u).\n", device_id);
        return DRV_ERROR_INVALID_HANDLE;
    }
    flag_para.devId = device_id;
    fd = devdrv_open_device_manager();
    if (fd < 0) {
        DEVDRV_DRV_ERR("open device manager failed, fd(%d). devid(%u)\n", fd, device_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = ioctl(fd, DEVDRV_MANAGER_GET_HOST_PHY_MACH_FLAG, &flag_para);
    if (ret != 0) {
        dmanage_share_log_read();
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", device_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    *host_flag = flag_para.host_flag;

    return DRV_ERROR_NONE;
}
#endif

drvError_t drvGetP2PStatus(uint32_t dev, uint32_t peer_dev, uint32_t *status)
{
    return DmsGetP2PStatus(dev, peer_dev, status);
}

#ifdef PCIE_HOST
drvError_t halDeviceEnableP2P(uint32_t dev, uint32_t peer_dev, uint32_t flag)
{
    (void)(flag);
    return DmsEnableP2P(dev, peer_dev, DRV_P2P_TYPE_MEM);
}

drvError_t halDeviceDisableP2P(uint32_t dev, uint32_t peer_dev, uint32_t flag)
{
    (void)(flag);
    return DmsDisableP2P(dev, peer_dev, DRV_P2P_TYPE_MEM);
}

#ifdef CFG_FEATURE_P2P_NOTIFY
drvError_t halDeviceEnableP2PNotify(uint32_t phy_dev, uint32_t peer_phy_dev, uint32_t flag)
{
    drvError_t ret;
    uint32_t logical_id;

    (void)(flag);
    ret = drvDeviceGetIndexByPhyId(phy_dev, &logical_id);
    if (ret != 0) {
        DEVDRV_DRV_WARN("invalid phy_dev %u.\n", phy_dev);
        return DRV_ERROR_INVALID_DEVICE;
    }
    return DmsEnableP2P(logical_id, peer_phy_dev, DRV_P2P_TYPE_NOTIFY);
}

drvError_t halDeviceDisableP2PNotify(uint32_t phy_dev, uint32_t peer_phy_dev, uint32_t flag)
{
    drvError_t ret;
    uint32_t logical_id;

    (void)(flag);
    ret = drvDeviceGetIndexByPhyId(phy_dev, &logical_id);
    if (ret != 0) {
        DEVDRV_DRV_WARN("invalid phy_dev %u.\n", phy_dev);
        return DRV_ERROR_INVALID_DEVICE;
    }
    return DmsDisableP2P(logical_id, peer_phy_dev, DRV_P2P_TYPE_NOTIFY);
}
#endif // CFG_FEATURE_P2P_NOTIFY

drvError_t halDeviceCanAccessPeer(int *canAccessPeer, uint32_t dev, uint32_t peer_dev)
{
    return DmsCanAccessPeer(dev, peer_dev, canAccessPeer);
}
#endif // PCIE_HOST

int drvGetDeviceDevIDByHostDevID(uint32_t host_dev_id, uint32_t *local_dev_id)
{
    struct devdrv_get_local_devid_para para = {0};
    int ret;
    int fd = -1;
    mmIoctlBuf buf = {0};

    if (host_dev_id >= ASCEND_HOST_PDEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid host device id %u.\n", host_dev_id);
        return DRV_ERROR_INVALID_DEVICE;
    }
    if (local_dev_id == NULL) {
        DEVDRV_DRV_ERR("local_dev_id is NULL. host devid(%u).\n", host_dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    para.host_dev_id = host_dev_id;
    fd = devdrv_open_device_manager();
    if (fd < 0) {
        DEVDRV_DRV_ERR("open failed, fd(%d). host devid(%u)\n", fd, host_dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    buf.inbuf = (void *)&para;
    buf.inbufLen = sizeof(struct devdrv_get_local_devid_para);
    buf.outbuf = buf.inbuf;
    buf.outbufLen = buf.inbufLen;
    buf.oa = NULL;
    ret = dmanage_mmIoctl(fd, DEVDRV_MANAGER_GET_LOCAL_DEV_ID_BY_HOST_DEV_ID, &buf);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, host devid(%u), ret(%d).\n", host_dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    *local_dev_id = para.local_dev_id;

    return DRV_ERROR_NONE;
}

drvError_t drvDeviceGetPcieInfo(uint32_t devId, int32_t *bus, int32_t *dev, int32_t *func)
{
    struct devdrv_pci_info devdrv_pci_info;
    mmIoctlBuf dev_info_buf = {0};
    mmIoctlBuf para_buf = {0};
    uint32_t para;
    int ret;

    if (devId >= ASCEND_DEV_MAX_NUM || bus == NULL || dev == NULL || func == NULL) {
        DEVDRV_DRV_ERR("Parameter is invalid. (devid=%u; bus_is_null=%d; dev_is_null=%d; func_is_null=%d)\n", devId,
                       (bus == NULL), (dev == NULL), (func == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    drv_ioctl_param_init(&para_buf, (void *)&para, sizeof(uint32_t));
    ret = drv_common_ioctl(&para_buf, DEVDRV_MANAGER_GET_PLATINFO);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed devId = %u, ret = %d.\n", devId, ret);
        return ret;
    }

    if (para == DEVDRV_MANAGER_DEVICE_ENV) {
        DEVDRV_DRV_ERR("offline env, no pcie info. devid = %u\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }

    devdrv_pci_info.dev_id = devId;
    drv_ioctl_param_init(&dev_info_buf, (void *)&devdrv_pci_info, sizeof(struct devdrv_pci_info));
    ret = drv_common_ioctl(&dev_info_buf, DEVDRV_MANAGER_GET_PCIINFO);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "ioctl failed devId = %u, ret = %d.\n", devId, ret);
        return ret != DRV_ERROR_BUSY ? ret : DRV_ERROR_RESOURCE_OCCUPIED;
    }

    *bus = devdrv_pci_info.bus_number;
    *dev = devdrv_pci_info.dev_number;
    *func = devdrv_pci_info.function_number;

    return DRV_ERROR_NONE;
}

drvError_t drvDeviceGetPcieIdInfo(uint32_t devId, struct tag_pcie_idinfo *pcie_idinfo)
{
#ifdef CFG_FEATURE_GET_PCIE_ID_INFO
    dms_pcie_id_info_t id_info = {0};
    int ret;

    if ((devId >= ASCEND_DEV_MAX_NUM) || (pcie_idinfo == NULL)) {
        DEVDRV_DRV_ERR("invalid devid(%u) or pcie_idinfo is NULL.\n", devId);
        return DRV_ERROR_INVALID_VALUE;
    }
    ret = DmsGetPcieIdInfo(devId, &id_info);
    if (ret != 0) {
        DMS_EX_NOTSUPPORT_ERR(ret, "dms get pcie id info failed. (devId=%u; ret=%d)", devId, ret);
        return ret;
    }

    pcie_idinfo->venderid = id_info.venderid;
    pcie_idinfo->subvenderid = id_info.subvenderid;
    pcie_idinfo->subdeviceid = id_info.subdeviceid;
    pcie_idinfo->deviceid = id_info.deviceid;
    pcie_idinfo->bdf_busid = id_info.bus;
    pcie_idinfo->bdf_deviceid = id_info.device;
    pcie_idinfo->bdf_funcid = id_info.fn;

    return DRV_ERROR_NONE;
#else
    (void)devId;
    (void)pcie_idinfo;
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

drvError_t halDrvDeviceGetPcieIdInfo(unsigned int devId, void *pcie_idinfo)
{
    return drvDeviceGetPcieIdInfo((uint32_t)devId, (struct tag_pcie_idinfo *)pcie_idinfo);
}

drvError_t drv_get_pcieinfo(uint32_t devId, int32_t info_type, int64_t *value)
{
    int ret;
    int32_t bus, dev, func;
    int64_t val;

    if (info_type == INFO_TYPE_ID) {
        ret = drvDeviceGetPcieInfo(devId, &bus, &dev, &func);
        if (ret != 0) {
            DEVDRV_DRV_ERR("drv_device_get_pcie_info failed ret = %d.\n", ret);
            return ret;
        }
        /* obtain the lower eight bits of bus */
        *value = (((((uint32_t)dev) & 0x1f) << PCIE_NUM) | (((uint32_t)func) & 0x07) |
                  ((((uint32_t)bus) & 0xff) << 8u));
    } else if (info_type == INFO_TYPE_P2P_CAPABILITY) {
        ret = DmsGetP2PCapbility(devId, (unsigned long long *)&val);
        if (ret != 0) {
            DEVDRV_DRV_ERR("drvDeviceGetP2PCapbility failed ret = %d.\n", ret);
            return ret;
        }
        *value = val;
    } else {
        DEVDRV_DRV_INFO("This version does not support this type. (Type=%d)\n", info_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}
