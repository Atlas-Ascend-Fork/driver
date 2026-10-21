/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <pthread.h>
#include "securec.h"
#include "devmng_common.h"
#include "devdrv_user_common.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devdrv_ioctl.h"
#include "devdrv_shm_comm.h"
#include "devdrv_ethernet.h"
#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "dms/dms_drv_internal.h"
#include "dsmi_common_interface.h"
#include "devmng_user_common.h"
#include "dms/dms_misc_interface.h"
#ifdef DRV_HOST
#include "devdrv_host_flush_cache.h"
#else
#include "devdrv_flush_cache.h"
#endif
#include "devdrv_pcie.h"
#include "ascend_dev_num.h"

#define INIT_DIVISOR_UNIT 1
#define IMU_DMP_MSG_RECV_REAL 32
#define IMU_DMP_MSG_RECV (IMU_DMP_MSG_RECV_REAL + 2)
#define LPM3_SMOKE_IPC_LEN 8
#define TSENSOR_RESULT_LEN 4
#define HUGE_PAGE_UNIT_SIZE (1024 * 2)
#define DDR_HBM_INDEX 2
#define FLASH_INDEX_MAX 1000

#define DEVDRV_IPC_LEN 34
#define DEVDRV_IPC_BUF_LEN 32
#define DEVDRV_LPM3_IPC_LEN 8

STATIC pthread_mutex_t g_dmanage_address_mutex = PTHREAD_MUTEX_INITIALIZER;

int dmanage_get_pcie_id_info(unsigned int dev_id, struct dmanage_pcie_id_info *pcie_id_info)
{
    struct dmanage_pcie_id_info id_info = {0};
    int ret;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (pcie_id_info == NULL) {
        DEVDRV_DRV_ERR("pcie_id_info is NULL. devid(%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = memset_s((void *)pcie_id_info, sizeof(struct dmanage_pcie_id_info), 0, sizeof(struct dmanage_pcie_id_info));
    if (ret != EOK) {
        DEVDRV_DRV_ERR("memset_s fail, ret(%d). devid(%u)\n", ret, dev_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    id_info.davinci_id = dev_id;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_PCIE_ID_INFO, (void *)&id_info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", dev_id, ret);
        return ret != DRV_ERROR_BUSY ? ret : DRV_ERROR_RESOURCE_OCCUPIED;
    }

    pcie_id_info->venderid = id_info.venderid;
    pcie_id_info->subvenderid = id_info.subvenderid;
    pcie_id_info->subdeviceid = id_info.subdeviceid;
    pcie_id_info->deviceid = id_info.deviceid;
    pcie_id_info->bus = id_info.bus;
    pcie_id_info->device = id_info.device;
    pcie_id_info->fn = id_info.fn;

    return DRV_ERROR_NONE;
}

#ifdef CFG_FEATURE_SCAN_XSFP
int dmanage_get_optical_module_temp(unsigned int dev_id, int *value)
{
    return dmanage_get_optical_module_temp_adapt(dev_id, value);
}
#endif

int devdrv_get_board_id(unsigned int dev_id, unsigned int *board_id)
{
    return DmsGetBoardId(dev_id, board_id);
}

/*
 * Obtain the highest severity level from the Fault Management module
 */
int dmanage_get_device_health(unsigned int dev_id, unsigned int *phealth)
{
    int ret;
    struct ioctl_arg arg;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (phealth == NULL) {
        DEVDRV_DRV_ERR("phealth is NULL. devid(%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    arg.dev_id = dev_id;
    arg.data1 = 0;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_HEALTH_CODE, (void *)&arg);
    if (ret != 0) {
        *phealth = 0;
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    if (arg.data2 == -EAGAIN) {
        return -EAGAIN;
    }

    *phealth = arg.data1;
    return DRV_ERROR_NONE;
}

/*
 * Failed to obtain the query environment from the Fault Management module
 * If there are more than one, give them all at a time
 */
int dmanage_get_device_errorcode(unsigned int dev_id, int *p_error_code_count, unsigned int *p_error_code,
                                 int p_error_code_len)
{
    struct devdrv_error_code_para error_code_para = {0, {0}, 0, {0}};
    int ret;
    int i;

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((p_error_code_count == NULL) || (p_error_code == NULL)) {
        DEVDRV_DRV_ERR("Parameter invalid. (p_error_code_count_is_null=%d; p_error_code_is_null=%d; devid=%u)\n",
                       (p_error_code_count == NULL), (p_error_code == NULL), dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    error_code_para.dev_id = dev_id;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_ERROR_CODE, (void *)&error_code_para);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    if (error_code_para.error_code_count > DMANAGE_ERROR_ARRAY_NUM ||
        error_code_para.error_code_count > p_error_code_len) {
        DEVDRV_DRV_WARN("bbox errcode_length exceeds input_length. (dev_id=%u; errcode_length=%d; input_length=%d).\n",
                        dev_id, error_code_para.error_code_count, p_error_code_len);
        *p_error_code_count = (p_error_code_len < DMANAGE_ERROR_ARRAY_NUM ? p_error_code_len : DMANAGE_ERROR_ARRAY_NUM);
    } else {
        *p_error_code_count = error_code_para.error_code_count;
    }

    for (i = 0; i < *p_error_code_count; i++) {
        p_error_code[i] = error_code_para.error_code[i];
    }
    return DRV_ERROR_NONE;
}

int devdrv_lpm3_smoke_ipc(unsigned char *send, unsigned char *ack, unsigned int len)
{
    unsigned char ipc[LPM3_SMOKE_IPC_LEN] = {0};
    int ret;
    u32 i;

    if (send == NULL || ack == NULL || len != LPM3_SMOKE_IPC_LEN) {
        DEVDRV_DRV_ERR("Parameter is invalid. (send_is_null=%d; ack_is_null=%d; len=%u)\n", (send == NULL),
                       (ack == NULL), len);
        return DRV_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < len; i++) {
        ipc[i] = send[i];
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_LPM3_SMOKE, (void *)ipc);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret(%d).\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    for (i = 0; i < len; i++) {
        ack[i] = ipc[i];
    }

    return DRV_ERROR_NONE;
}

int devdrv_get_host_phy_mach_flag(unsigned int dev_id, unsigned int *host_flag)
{
    struct devdrv_get_host_phy_mach_flag_para para;
    int ret;

    if (host_flag == NULL) {
        DEVDRV_DRV_ERR("host_flag is NULL.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }
    para.devId = (unsigned int)dev_id;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_HOST_PHY_MACH_FLAG, (void *)&para);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret(%d)\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    *host_flag = para.host_flag;

    return DRV_ERROR_NONE;
}

int dmanage_get_container_flag(unsigned int *flag)
{
    unsigned int container_flag;
    int ret;

    if (flag == NULL) {
        DEVDRV_DRV_ERR("flag is NULL.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_CONTAINER_FLAG, (void *)&container_flag);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret(%d).\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    /* 1 is a normal container, 0 is physical machine or privilege container */
    *flag = container_flag;

    return DRV_ERROR_NONE;
}

int dmanage_get_emmc_voltage(int *emmc_vcc, int *emmc_vccq)
{
    int ret;
    struct devdrv_emmc_voltage_para devdrv_emmc_voltage = {0, 0};
    if ((emmc_vcc == NULL) || (emmc_vccq == NULL)) {
        DEVDRV_DRV_ERR("Parameter is invalid. (emmc_vcc_is_null=%d; emmc_vccq_is_null=%d)\n", (emmc_vcc == NULL),
                       (emmc_vccq == NULL));
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_EMMC_VOLTAGE, (void *)&devdrv_emmc_voltage);
    if (ret != 0) {
        *emmc_vcc = 0;
        *emmc_vccq = 0;
        DEVDRV_DRV_ERR("ioctl failed, ret(%d).\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    *emmc_vcc = devdrv_emmc_voltage.emmc_vcc;
    *emmc_vccq = devdrv_emmc_voltage.emmc_vccq;
    return DRV_ERROR_NONE;
}

int dmanage_enable_efuse_ldo2(void)
{
    int ret;
    int para = 0;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_ENABLE_EFUSE_LDO, (void *)&para);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret(%d).\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    DEVDRV_DRV_INFO("enable efuse ldo2.\n");
    return DRV_ERROR_NONE;
}

int dmanage_disable_efuse_ldo2(void)
{
    int ret;
    int para = 0;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_DISABLE_EFUSE_LDO, (void *)&para);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret(%d).\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    DEVDRV_DRV_INFO("disable efuse ldo2.\n");
    return DRV_ERROR_NONE;
}

int devdrv_imu_smoke_ipc(unsigned int dev_id, const unsigned char *send, unsigned int send_len, unsigned char *ack,
                         unsigned int *ack_len)
{
    int ret;
    unsigned char ipc[DEVDRV_IMU_CMD_LEN + 1] = {0};

    if (send == NULL || ack == NULL || dev_id >= ASCEND_DEV_MAX_NUM || send_len > DEVDRV_IMU_CMD_LEN ||
        ack_len == NULL) {
        DEVDRV_DRV_ERR("Parameter invalid. devid=%u; send_len=%u, send_is_null=%d; ack_is_null=%d; ack_len_is_null=%d",
                       dev_id, send_len, (send == NULL), (ack == NULL), (ack_len == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }
    if (*ack_len < DEVDRV_IMU_CMD_LEN) {
        DEVDRV_DRV_ERR("ack size too short, ack_len(%u). devid(%u)\n", *ack_len, dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    ipc[0] = (unsigned char)dev_id;

    ret = memcpy_s((void *)&ipc[1], DEVDRV_IMU_CMD_LEN, (void *)send, send_len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy failed, ret(%d). devid(%u)\n", ret, dev_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_IMU_SMOKE, (void *)ipc);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    ret = memcpy_s((void *)&ack[0], DEVDRV_IMU_CMD_LEN, (void *)&ipc[1], DEVDRV_IMU_CMD_LEN);
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy failed, ret(%d). devid(%u)\n", ret, dev_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    *ack_len = DEVDRV_IMU_CMD_LEN;

    return DRV_ERROR_NONE;
}

int dmanage_get_imu_info(unsigned int dev_id, unsigned char *send, unsigned int send_len, unsigned char *ack,
                         unsigned int *ack_len)
{
    unsigned char ipc[DEVDRV_IMU_CMD_LEN + 1] = {0};
    unsigned int bak_len;
    int ret;

    if (send == NULL || ack == NULL || dev_id >= ASCEND_DEV_MAX_NUM || send_len > DEVDRV_IMU_CMD_LEN ||
        ack_len == NULL) {
        DEVDRV_DRV_ERR("Parameter is invalid. "
                       "(devid=%u; send_len=%u; send_is_null=%d; ack_is_null=%d; ack_len_is_null=%d)\n",
                       dev_id, send_len, (send == NULL), (ack == NULL), (ack_len == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    ipc[0] = (unsigned char)dev_id;
    ret = memcpy_s((void *)&ipc[1], DEVDRV_IMU_CMD_LEN, (void *)send, send_len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy_s failed. (dev_id=%u; ret=%d).\n", dev_id, ret);
        return DRV_ERROR_OUT_OF_MEMORY;
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_IMU_INFO, (void *)ipc);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    bak_len = ((unsigned int)ipc[0] > DEVDRV_IMU_CMD_LEN) ? DEVDRV_IMU_CMD_LEN : (unsigned int)ipc[0];
    ret = memcpy_s((void *)ack, *ack_len, (void *)&ipc[1], bak_len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy_s failed. (dev_id=%u; ret=%d).\n", dev_id, ret);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    *ack_len = bak_len;

    return DRV_ERROR_NONE;
}

STATIC int dmanage_get_eth_name(unsigned int dev_id, struct dmanager_card_info card_info, char *eth_name_buf,
                                unsigned int buf_size)
{
    unsigned int hostDevid = 0;
    int ret = -1;
    int ethid = card_info.card_id;

    if (buf_size != DEVDRV_MAX_ETH_NAME_LEN) {
        DEVDRV_DRV_ERR("eth name buf size[%u] err, devid(%u).\n", buf_size, dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }
/* 910/910B/910_A3 need to trans eth_id for AI Server, in 950 eth_id/port_id = card_id (from user) */
#ifdef CFG_FEATURE_TRANS_ETH_ID
    if (drvDeviceGetEthIdByIndex(dev_id, card_info.card_id, (uint32_t *)&ethid)) {
        DEVDRV_DRV_ERR("devid %d card_id %d transfer ethid(%d) fail.\n", dev_id, card_info.card_id, ethid);
        return DRV_ERROR_INVALID_VALUE;
    }
#endif
    if (card_info.card_type == DEVDRV_VNIC) {
        ret = drvGetDevIDByLocalDevID(dev_id, &hostDevid);
        if (ret != 0) {
            DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "get hostDevid by dev_id failed. dev_id(%d), ret(%d)\n", dev_id, ret);
            return -1;
        }
        ret = sprintf_s(eth_name_buf, buf_size, "end%uv%u", hostDevid, dev_id);
        if (ret < 0) {
            DEVDRV_DRV_ERR("create VNIC eth_name failed, devid(%u), ret(%d).\n", dev_id, ret);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else if (card_info.card_type == DEVDRV_ROCE) {
        ret = sprintf_s(eth_name_buf, buf_size, "eth%d", ethid);
        if (ret < 0) {
            DEVDRV_DRV_ERR("create ROCE eth_name failed, devid(%u), ret(%d).\n", dev_id, ret);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else if (card_info.card_type == DEVDRV_BOND) {
        ret = sprintf_s(eth_name_buf, buf_size, "bond%d", ethid);
        if (ret < 0) {
            DEVDRV_DRV_ERR("Failed to invoke sprintf_s for BOND eth_name. (devid=%u; ret=%d)\n", dev_id, ret);
            return DRV_ERROR_INVALID_VALUE;
        }
    } else {
        DEVDRV_DRV_ERR("do not support card_type:%d\n", card_info.card_type);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

STATIC int dmanage_nic_type_check(unsigned int dev_id, struct dmanager_card_info card_info)
{
    if (card_info.card_type == DEVDRV_VNIC) {
        /* All platform are support DEVDRV_VNIC */
        return DRV_ERROR_NONE;
    } else if (card_info.card_type == DEVDRV_ROCE) {
#ifdef CFG_FEATURE_NETWORK_ROCE
        return DRV_ERROR_NONE;
#else
        return DRV_ERROR_NOT_SUPPORT;
#endif
    } else if (card_info.card_type == DEVDRV_BOND) {
#ifdef CFG_FEATURE_BOND_PORT_CONFIG
        return DRV_ERROR_NONE;
#else
        return DRV_ERROR_NOT_SUPPORT;
#endif
    } else {
        DEVDRV_DRV_ERR("Parameter invalid. (devid=%u; card_type=%d)\n", dev_id, card_info.card_type);
        return DRV_ERROR_PARA_ERROR;
    }
}

int dmanage_get_ip_address(unsigned int dev_id, struct dmanager_card_info card_info, struct dmanager_ip_info *ack_info)
{
    int ret;
    char eth_name[DEVDRV_MAX_ETH_NAME_LEN] = {0};

    if (ack_info == NULL) {
        DEVDRV_DRV_ERR("ack_info is NULL. devid(%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = dmanage_nic_type_check(dev_id, card_info);
    if (ret != 0) {
        return ret;
    }

    ret = dmanage_get_eth_name(dev_id, card_info, eth_name, sizeof(eth_name));
    if (ret != 0) {
        DEVDRV_DRV_ERR("dmanage_get_eth_name failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_mutex_lock(&g_dmanage_address_mutex);
    ret = devdrv_get_ip_address(eth_name, ack_info);
    (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
    if (ret != 0) {
        if (ret != DRV_ERROR_INVALID_VALUE && ret != DRV_ERROR_NO_DEVICE) {
            DEVDRV_DRV_ERR("get ip address failed, ethname(%s), ret(%d).\n", eth_name, ret);
        }
        return ret;
    }
    return DRV_ERROR_NONE;
}

STATIC int dmanage_execute_set_ip_cmd(char **clean, char **set, char **up, char **lldptool, const char *ip_addr)
{
    int ret;

    if (clean != NULL) {
        ret = dmanage_run_proc(clean);
        if (ret != 0) {
            DEVDRV_DRV_ERR("Failed to clean old ip.\n");
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    ret = dmanage_run_proc(set);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to set new ip.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    ret = dmanage_run_proc(up);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to make ethx up.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

#ifdef CFG_FEATURE_LLDP_TOOL
    ret = dmanage_run_proc(lldptool);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke lldptool.\n");
        return DRV_ERROR_INVALID_VALUE;
    }
#else
    (void)lldptool;
#endif

    ret = dmanage_restart_ssh(ip_addr);
    if (ret != 0) {
        DEVDRV_DRV_ERR("dmanage_restart_ssh failed, ret: %d.\n", ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

STATIC int dmanage_set_ip_info_check(unsigned int dev_id, struct dmanager_card_info card_info,
                                     struct dmanager_ip_info config_info, char *eth_name, int len)
{
    int ret;
    if (config_info.ip_type != IPADDR_TYPE_V4 && config_info.ip_type != IPADDR_TYPE_V6) {
        return DRV_ERROR_NOT_SUPPORT;
    }

    if (dev_id >= MAX_DAVINCI_NUM_OF_ONE_CHIP) {
        DEVDRV_DRV_ERR("The dev_id is out of range. (devid=%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = dmanage_nic_type_check(dev_id, card_info);
    if (ret != 0) {
        return ret;
    }

    ret = dmanage_get_eth_name(dev_id, card_info, eth_name, len);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke dmanage_get_eth_name to get eth_name. (devid=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

#define LLDPTOOL_IPV4_STR "ipv4="
#define LLDPTOOL_IPV6_STR "ipv6="
#define LLDPTOOL_STR_LEN (5)
STATIC int dmanage_set_ipv4_info(unsigned int dev_id, struct dmanager_ip_info config_info, const char *eth_name)
{
    int ret;
    char ip_addr[INET6_ADDRSTRLEN] = {0};
    char mask_addr[INET6_ADDRSTRLEN] = {0};
    char lldptool_mng_addr[INET6_ADDRSTRLEN + LLDPTOOL_STR_LEN] = {0};
    const char *ipv4_cmd_set_ip[] = {
        "sudo", "/var/dmp_sudo_config.sh", "/sbin/ifconfig", eth_name, ip_addr, "netmask", mask_addr, NULL};
    const char *ipvx_cmd_eth_up[] = {"sudo", "/var/dmp_sudo_config.sh", "/sbin/ifconfig", eth_name, "up", NULL};
    const char *ipvx_cmd_lldptool[] = {
        "sudo", "/var/dmp_sudo_config.sh", "/sbin/lldptool", "-T", "-i", eth_name, "-V", "mngAddr", lldptool_mng_addr,
        NULL};

    (void)inet_ntop(AF_INET, &config_info.ip_addr, ip_addr, INET_ADDRSTRLEN);
    (void)inet_ntop(AF_INET, &config_info.mask_addr, mask_addr, INET_ADDRSTRLEN);
    ret = sprintf_s(lldptool_mng_addr, INET6_ADDRSTRLEN + LLDPTOOL_STR_LEN, "%s%s", LLDPTOOL_IPV4_STR, ip_addr);
    if (ret <= 0) {
        DEVDRV_DRV_ERR("sprintf failed. (devid=%u; eth_name=\"%s\"; ret=%d)\n", dev_id, eth_name, ret);
        return ret;
    }
    (void)pthread_mutex_lock(&g_dmanage_address_mutex);
    ret = dmanage_execute_set_ip_cmd(NULL, (char **)ipv4_cmd_set_ip, (char **)ipvx_cmd_eth_up,
                                     (char **)ipvx_cmd_lldptool, ip_addr);
    (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke dmanage_execute_set_ip_cmd. (devid=%u; eth_name=\"%s\"; ret=%d)\n", dev_id,
                       eth_name, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC int dmanage_ipv6_cmd_format(struct dmanager_ip_info config_info, char *cmd_ip_set, int set_len,
                                   char *cmd_lldptool, int lld_len)
{
    int ret;
    char ip_addr[INET6_ADDRSTRLEN] = {0};

    (void)inet_ntop(AF_INET6, &config_info.ip_addr, ip_addr, INET6_ADDRSTRLEN);
    ret = sprintf_s(cmd_ip_set, set_len, "%s/%u", ip_addr, config_info.mask_addr.addr_v6[0]);
    if (ret <= 0) {
        DEVDRV_DRV_ERR("sprintf failed. ret=%d\n", ret);
        return ret;
    }

    ret = sprintf_s(cmd_lldptool, lld_len, "%s%s", LLDPTOOL_IPV6_STR, ip_addr);
    if (ret <= 0) {
        DEVDRV_DRV_ERR("sprintf failed. ret=%d\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

STATIC int dmanage_set_ipv6_info(unsigned int dev_id, struct dmanager_ip_info config_info, char *eth_name)
{
    int ret;
    int ret_tmp;
    char ip_addr[INET6_ADDRSTRLEN] = {0};
    char ip_and_netmask[INET6_ADDRSTRLEN] = {0};
    char old_ip_and_netmask[INET6_ADDRSTRLEN] = {0};
    char lldptool_mng_addr[INET6_ADDRSTRLEN + LLDPTOOL_STR_LEN] = {0};
    const char *ipv6_cmd_clean[] = {
        "sudo", "/var/dmp_sudo_config.sh", "/sbin/ip", "-6", "addr", "flush", "dev", eth_name, "scope", "global", NULL};
    const char *ipv6_cmd_set_ip[] = {
        "sudo", "/var/dmp_sudo_config.sh", "/sbin/ifconfig", eth_name, "add", ip_and_netmask, NULL};
    const char *ipv6_cmd_rollback_ip[] = {
        "sudo", "/var/dmp_sudo_config.sh", "/sbin/ifconfig", eth_name, "add", old_ip_and_netmask, NULL};
    const char *ipvx_cmd_eth_up[] = {"sudo", "/var/dmp_sudo_config.sh", "/sbin/ifconfig", eth_name, "up", NULL};
    const char *ipvx_cmd_lldptool[] = {
        "sudo", "/var/dmp_sudo_config.sh", "/sbin/lldptool", "-T", "-i", eth_name, "-V", "mngAddr", lldptool_mng_addr,
        NULL};
    struct dmanager_ip_info old_ip_info = {0};

    old_ip_info.ip_type = DEVDRV_IPV6;
    ret_tmp = dmanage_ipv6_cmd_format(config_info, ip_and_netmask, INET6_ADDRSTRLEN, lldptool_mng_addr,
                                      INET6_ADDRSTRLEN + LLDPTOOL_STR_LEN);
    if (ret_tmp != 0) {
        DEVDRV_DRV_ERR("Failed format ipv6 cmd. (devid=%u; eth_name=\"%s\"; ret=%d)\n", dev_id, eth_name, ret_tmp);
        return DRV_ERROR_MEMORY_OPT_FAIL;
    }
    (void)pthread_mutex_lock(&g_dmanage_address_mutex);
    ret_tmp = devdrv_get_ip_address(eth_name, &old_ip_info);
    if (ret_tmp != 0 && ret_tmp != DRV_ERROR_NO_DEVICE) {
        (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
        DEVDRV_DRV_ERR("Failed to get old ip info. (devid=%u; eth_name=\"%s\"; ret_tmp=%d)\n", dev_id, eth_name,
                       ret_tmp);
        return ret_tmp;
    }

    ret = dmanage_execute_set_ip_cmd((char **)ipv6_cmd_clean, (char **)ipv6_cmd_set_ip, (char **)ipvx_cmd_eth_up,
                                     (char **)ipvx_cmd_lldptool, ip_addr);
    if (ret != 0 && ret_tmp != DRV_ERROR_NO_DEVICE) {
        DEVDRV_DRV_ERR("dmanage_execute_set_ip_cmd failed, (ip_and_netmask=\"%s\"; ret=%d, ret_tmp=%d)\n",
                       ip_and_netmask, ret, ret_tmp);
        ret_tmp = dmanage_ipv6_cmd_format(old_ip_info, old_ip_and_netmask, INET6_ADDRSTRLEN, lldptool_mng_addr,
                                          INET6_ADDRSTRLEN + LLDPTOOL_STR_LEN);
        if (ret_tmp != 0) {
            (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
            DEVDRV_DRV_ERR("Failed format ipv6 cmd. (eth_name=\"%s\"; ret=%d)\n", eth_name, ret_tmp);
            return DRV_ERROR_MEMORY_OPT_FAIL;
        }

        ret_tmp = dmanage_execute_set_ip_cmd((char **)ipv6_cmd_clean, (char **)ipv6_cmd_rollback_ip,
                                             (char **)ipvx_cmd_eth_up, (char **)ipvx_cmd_lldptool, ip_addr);
        if (ret_tmp != 0) {
            DEVDRV_DRV_ERR("rollback failed. (eth_name=\"%s\"; ret_tmp=%d, old_ip_and_netmask=\"%s\")\n", eth_name,
                           ret_tmp, old_ip_and_netmask);
        } else {
            DEVDRV_DRV_INFO("rollback success.\n");
        }
    }
    (void)pthread_mutex_unlock(&g_dmanage_address_mutex);

    return ret;
}

int dmanage_set_ip_address(unsigned int dev_id, struct dmanager_card_info card_info,
                           struct dmanager_ip_info config_info)
{
    int ret;
    char eth_name[DEVDRV_MAX_ETH_NAME_LEN] = {0};

    ret = dmanage_set_ip_info_check(dev_id, card_info, config_info, eth_name, DEVDRV_MAX_ETH_NAME_LEN);
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "dmanage_set_ip_info_check failed. devid=%u; eth_name=\"%s\"; ret=%d\n",
                                     dev_id, eth_name, ret);
        return ret;
    }

    if (config_info.ip_type == IPADDR_TYPE_V4) {
        ret = dmanage_set_ipv4_info(dev_id, config_info, eth_name);
    } else {
        if (devdrv_ipv6_ip_is_local(&config_info.ip_addr)) {
            return DRV_ERROR_NOT_SUPPORT;
        }

        ret = dmanage_set_ipv6_info(dev_id, config_info, eth_name);
    }
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke dmanage_execute_set_ip_cmd. (devid=%u; eth_name=\"%s\"; ret=%d)\n", dev_id,
                       eth_name, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int dmanage_get_gateway_address(unsigned int dev_id, struct dmanager_card_info card_info,
                                struct dmanager_gtw_info *ack_info)
{
    char eth_name[DEVDRV_MAX_ETH_NAME_LEN] = {0};
    int ret;

    if (ack_info == NULL) {
        DEVDRV_DRV_ERR("ack_info is NULL. devid(%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = dmanage_nic_type_check(dev_id, card_info);
    if (ret != 0) {
        return ret;
    }

    ret = dmanage_get_eth_name(dev_id, card_info, eth_name, sizeof(eth_name));
    if (ret != 0) {
        DEVDRV_DRV_ERR("dmanage_get_eth_name failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    (void)pthread_mutex_lock(&g_dmanage_address_mutex);
    ret = devdrv_get_gateway_address(eth_name, ack_info);
    if (ret != 0) {
        if (ret != DRV_ERROR_NO_DEVICE) {
            DEVDRV_DRV_ERR("get gateway address failed, ethname(%s), ret(%d).\n", eth_name, ret);
        }
        (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
        return ret;
    }
    (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
    return DRV_ERROR_NONE;
}

#ifndef DEVDRV_UT
STATIC int dms_set_gateway_address(unsigned int dev_id, struct dmanager_gtw_info config_info, const char *eth_name)
{
    struct ifreq ifr_add, ifr_del;
    char gtw_addr_add[DEVDRV_MAX_IP_LEN] = {0}, gtw_addr_del[DEVDRV_MAX_IP_LEN] = {0};
    const char *argv_add[] = {
        "sudo", "/var/dmp_sudo_config.sh", "/sbin/route", "add", "default", "gw", gtw_addr_add, eth_name, NULL};
    const char *argv_del[] = {
        "sudo", "/var/dmp_sudo_config.sh", "/sbin/route", "del", "default", "gw", gtw_addr_del, eth_name, NULL};
    struct dmanager_gtw_info cur_data = {0};
    int ret;

    (void)pthread_mutex_lock(&g_dmanage_address_mutex);
    ret = devdrv_get_gateway_address(eth_name, &cur_data);
    if (ret == 0 && cur_data.gtw_addr.addr_v4 != 0) {
        (((struct sockaddr_in *)&(ifr_del.ifr_addr))->sin_addr).s_addr = cur_data.gtw_addr.addr_v4;
        ret = strcpy_s((char *)gtw_addr_del, sizeof(gtw_addr_del),
                       inet_ntoa(((struct sockaddr_in *)&(ifr_del.ifr_addr))->sin_addr));
        if (ret != 0) {
            (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
            DEVDRV_DRV_ERR("transfer config_data to ip_addr failed.\n");
            return ret;
        }

        ret = dmanage_run_proc((char **)argv_del);
        if (ret != 0) {
            (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
            DEVDRV_DRV_ERR("run cmd failed, devid(%u), ret(%d).\n", dev_id, ret);
            return DRV_ERROR_INVALID_VALUE;
        }
    }

    (((struct sockaddr_in *)&(ifr_add.ifr_addr))->sin_addr).s_addr = config_info.gtw_addr.addr_v4;
    ret = strcpy_s((char *)gtw_addr_add, sizeof(gtw_addr_add),
                   inet_ntoa(((struct sockaddr_in *)&(ifr_add.ifr_addr))->sin_addr));
    if (ret != 0) {
        (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
        DEVDRV_DRV_ERR("strcpy_s failed, devid(%u), ret(%d).\n", dev_id, ret);
        return ret;
    }

    ret = dmanage_run_proc((char **)argv_add);
    if (ret != 0) {
        (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
        DEVDRV_DRV_ERR("run cmd failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }
    (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
    return DRV_ERROR_NONE;
}
#endif

int dmanage_set_gateway_address(unsigned int dev_id, struct dmanager_card_info card_info,
                                struct dmanager_gtw_info config_info)
{
    char eth_name[DEVDRV_MAX_ETH_NAME_LEN] = {0};
    int ret;

    if (dev_id >= MAX_DAVINCI_NUM_OF_ONE_CHIP) {
        DEVDRV_DRV_ERR("dev_id is out range. dev_id = %u\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = dmanage_nic_type_check(dev_id, card_info);
    if (ret != 0) {
        return ret;
    }

    ret = dmanage_get_eth_name(dev_id, card_info, eth_name, sizeof(eth_name));
    if (ret != 0) {
        DEVDRV_DRV_ERR("dmanage_get_eth_name failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }
#ifndef DEVDRV_UT
    ret = dms_set_gateway_address(dev_id, config_info, eth_name);
    if (ret != 0) {
        return ret;
    }
#endif
    return DRV_ERROR_NONE;
}

int dmanage_set_gateway_address6(unsigned int dev_id, struct dmanager_card_info card_info,
                                 struct dmanager_gtw_info config_info)
{
    int ret;
    int roll_back_flag;
    char ipv6_old_gw_addr[INET6_ADDRSTRLEN] = {0};
    char ipv6_new_gw_addr[INET6_ADDRSTRLEN] = {0};
    char ipv6_zero_gw_addr[INET6_ADDRSTRLEN] = {0};
    char ethname[DEVDRV_MAX_ETH_NAME_LEN] = {0};
    const char *ipv6_gw_del[] = {"sudo", "/var/dmp_sudo_config.sh", "/sbin/route", "-A",    "inet6", "del", "default",
                                 "gw",   ipv6_old_gw_addr,          "dev",         ethname, NULL};
    const char *ipv6_gw_set[] = {"sudo", "/var/dmp_sudo_config.sh", "/sbin/route", "-A",    "inet6", "add", "default",
                                 "gw",   ipv6_new_gw_addr,          "dev",         ethname, NULL};
    const char *ipv6_gw_rollback[] = {
        "sudo", "/var/dmp_sudo_config.sh", "/sbin/route", "-A",    "inet6", "add", "default",
        "gw",   ipv6_old_gw_addr,          "dev",         ethname, NULL};
    struct dmanager_ip_info ipv6_ip_info = {0};
    struct dmanager_gtw_info ipv6_old_gw_info = {0};
    ipaddr_t ip_addr_zero = {0};

    if (dev_id >= MAX_DAVINCI_NUM_OF_ONE_CHIP) {
        DEVDRV_DRV_ERR("The dev_id is out range. (dev_id=%u; max=%u)\n", dev_id, MAX_DAVINCI_NUM_OF_ONE_CHIP);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = dmanage_nic_type_check(dev_id, card_info);
    if (ret != 0) {
        return ret;
    }

    ret = dmanage_get_eth_name(dev_id, card_info, ethname, sizeof(ethname));
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke dmanage_get_eth_name. (devid=%u; ret=%d)\n", dev_id, ret);
        return DRV_ERROR_INVALID_VALUE;
    }

    ipv6_ip_info.ip_type = DEVDRV_IPV6;
    (void)pthread_mutex_lock(&g_dmanage_address_mutex);
    ret = devdrv_get_ip_address(ethname, &ipv6_ip_info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke devdrv_get_ip_address. (ethname=\"%s\")\n", ethname);
        goto OUT;
    }

    /* if gateway value is all zero, it will be formatted as '::'. */
    (void)inet_ntop(AF_INET6, &ip_addr_zero, ipv6_zero_gw_addr, INET6_ADDRSTRLEN);

    ipv6_old_gw_info.ip_type = DEVDRV_IPV6;
    ret = devdrv_get_gateway_address(ethname, &ipv6_old_gw_info);
    if (ret != 0 && ret != DRV_ERROR_NO_DEVICE) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to get old gw info. (devid=%u; ret=%d)\n", dev_id, ret);
        goto OUT;
    }

    (void)inet_ntop(AF_INET6, &config_info.gtw_addr, ipv6_new_gw_addr, INET6_ADDRSTRLEN);
    (void)inet_ntop(AF_INET6, &ipv6_old_gw_info.gtw_addr, ipv6_old_gw_addr, INET6_ADDRSTRLEN);
    if (strcmp(ipv6_old_gw_addr, ipv6_zero_gw_addr) == 0) { /* if default gateway is NULL. */
        ret = dmanage_run_proc((char **)ipv6_gw_set);
        if (ret != 0) {
            DEVDRV_DRV_ERR("Failed to set the gateway, (ipv6_new_gw_addr=\"%s\"; ethname=\"%s\")\n", ipv6_new_gw_addr,
                           ethname);
            goto OUT;
        }
    } else {
        ret = dmanage_run_proc((char **)ipv6_gw_del);
        if (ret != 0) {
            DEVDRV_DRV_ERR("Failed to delete the gateway. (ipv6_old_gw_addr=\"%s\"; ethname=\"%s\")\n",
                           ipv6_old_gw_addr, ethname);
            goto OUT;
        }

        ret = dmanage_run_proc((char **)ipv6_gw_set);
        if (ret != 0) {
            DEVDRV_DRV_ERR("Failed to set the gateway. (ipv6_new_gw_addr=\"%s\"; ethname=\"%s\", ret=%d)\n",
                           ipv6_new_gw_addr, ethname, ret);

            roll_back_flag = dmanage_run_proc((char **)ipv6_gw_rollback);
            if (roll_back_flag != 0) {
                DEVDRV_DRV_ERR("gateway rollback failed. (ipv6_old_gw_addr=\"%s\"; ethname=\"%s\")\n", ipv6_old_gw_addr,
                               ethname);
            }
            goto OUT;
        }
    }

OUT:
    (void)pthread_mutex_unlock(&g_dmanage_address_mutex);
    return ret;
}

int dmanage_get_pmu_voltage(struct dmanager_pmu_voltage_stru *pmu_voltage)
{
    int ret;

    if (pmu_voltage == NULL) {
        DEVDRV_DRV_ERR("pmu_voltage is NULL.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_PMU_VOLTAGE, (void *)pmu_voltage);
    if (ret != 0) {
        pmu_voltage->get_value = 0;
        pmu_voltage->return_value = 0;
        DEVDRV_DRV_ERR("ioctl failed, ret(%d).\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

int dmanage_inquire_imu_info(unsigned int dev_id, unsigned int *status)
{
    struct ioctl_arg arg;
    int ret;

    if (status == NULL) {
        DEVDRV_DRV_ERR("status is NULL. devid(%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.dev_id = dev_id;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_DEBUG_INFORM, (void *)&arg);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    *status = arg.data1;

    return DRV_ERROR_NONE;
}

int dmanage_get_computing_power(unsigned int dev_id, struct tag_computing_power_msg *data)
{
    struct computing_power_arg arg;
    int ret;

    if (data == NULL) {
        DEVDRV_DRV_ERR("data is NULL. devid(%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    arg.dev_id = dev_id;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_COMPUTE_POWER, (void *)&arg);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    ret = memcpy_s((void *)data, sizeof(struct tag_computing_power_msg), (void *)&arg.compute_power_msg,
                   sizeof(struct tag_computing_power_msg));
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy_s failed, ret(%d). devid(%u)\n", ret, dev_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    return DRV_ERROR_NONE;
}

int dmanage_get_bb_errstr(unsigned int dev_id, unsigned int errcode, unsigned char *errstr, int buf_len)
{
    struct bb_err_string err_str = {0};
    int ret;
    int buf_len_temp = buf_len;

    if (errstr == NULL) {
        DEVDRV_DRV_ERR("errstr is NULL. devid(%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (buf_len_temp > BBOX_ERRSTR_LEN) {
        buf_len_temp = BBOX_ERRSTR_LEN;
    } else if (buf_len_temp < BBOX_ERRSTR_LEN) {
        DEVDRV_DRV_ERR("buf_len(%d) shorter than %d. devid(%u)\n", buf_len_temp, BBOX_ERRSTR_LEN, dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    err_str.dev_id = dev_id;
    err_str.errcode = errcode;
    err_str.buf_len = buf_len_temp;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_BBOX_ERRSTR, (void *)&err_str);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }
    ret = memcpy_s((void *)errstr, BBOX_ERRSTR_LEN, (void *)err_str.errstr, BBOX_ERRSTR_LEN);
    if (ret != 0) {
        DEVDRV_DRV_ERR("memcpy_s failed, ret(%d). devid(%u)\n", ret, dev_id);
        return DRV_ERROR_OUT_OF_MEMORY;
    }
    return DRV_ERROR_NONE;
}

int dmanage_get_llc_perf_para(unsigned int dev_id, struct dmanager_llc_perf_info *perf_info)
{
    int ret;

    if (perf_info == NULL) {
        DEVDRV_DRV_ERR("perf_info is NULL. devid(%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (dev_id >= ASCEND_DEV_MAX_NUM) {
        DEVDRV_DRV_ERR("invalid devid(%u).\n", dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    perf_info->dev_id = dev_id;
    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_LLC_PERF_PARA, (void *)perf_info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, devid(%u), ret(%d).\n", dev_id, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

/*
 * reset i2c controller
 */
int dmanage_reset_i2c_controller(void)
{
    struct ioctl_arg arg;
    int ret;

    arg.dev_id = 0;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_RST_I2C_CTROLLER, (void *)&arg);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret(%d).\n", ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    return DRV_ERROR_NONE;
}

/*
 * gpio retry read
 */
int dmanage_i2c_gpio_read(unsigned int gpio_num, unsigned int *gpio_value)
{
    int ret;
    struct ioctl_arg arg;

    if (gpio_value == NULL) {
        DEVDRV_DRV_ERR("gpio_value is NULL.\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    arg.dev_id = 0;
    arg.data1 = gpio_num;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_GPIO_STATE, (void *)&arg);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, gpio_num(%u), ret(%d).\n", gpio_num, ret);
        return DRV_ERROR_IOCRL_FAIL;
    }

    *gpio_value = arg.data1;
    return DRV_ERROR_NONE;
}

/*
 * if wanting to do some init operations when dmp_daemon process up, add it here
 */
int dmanage_dmp_init(void)
{
    return DRV_ERROR_NONE;
}

#ifdef CFG_FEATURE_PSS_SIGN
int dmanage_set_device_sign(unsigned int dev_id, unsigned int subcmd, void *buf, unsigned int buf_size)
{
#if defined(CFG_FEATURE_PSS_SIGN) && defined(CFG_FEATURE_PKCS_SIGN)
    int ret;
    unsigned char val;
    struct devdrv_sign_verification_info sign_info = {0};

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (subcmd != DSMI_SEC_SUB_CMD_PSS) || (buf == NULL)) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; subcmd=%u; buf_is_null=%d)\n", dev_id, subcmd, (buf == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (buf_size != sizeof(unsigned char)) {
        DEVDRV_DRV_ERR("The value of buf_size is invalid. (buf_size=%u; size=%u)\n", buf_size, sizeof(unsigned char));
        return DRV_ERROR_PARA_ERROR;
    }

    val = *(unsigned char *)buf;
    if (val != PKCS_SIGN_TYPE_ON && val != PKCS_SIGN_TYPE_OFF) {
        DEVDRV_DRV_ERR("The value of buf is invalid. (buf=%u)\n", val);
        return DRV_ERROR_PARA_ERROR;
    }

    sign_info.dev_id = dev_id;
    sign_info.sign_buf = val;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_SET_SIGN, (void *)&sign_info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke dmanage_set_device_sign. (devid=%u; ret=%d).\n", dev_id, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
#else
    (void)(dev_id);
    (void)(subcmd);
    (void)(buf);
    (void)(buf_size);
    return DRV_ERROR_NOT_SUPPORT;
#endif
}

#define PKCS_SIGN_TYPE_OFF 1
#define PKCS_SIGN_TYPE_ON 0
int dmanage_get_device_sign(unsigned int dev_id, unsigned int vfid, unsigned int subcmd, void *buf,
                            unsigned int *buf_size)
{
    (void)vfid;
#if defined(CFG_FEATURE_PSS_SIGN) && defined(CFG_FEATURE_PKCS_SIGN)
    int ret;
    unsigned char *val = NULL;
    struct devdrv_sign_verification_info sign_info = {0};
#endif

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (subcmd != DSMI_SEC_SUB_CMD_PSS) || (buf == NULL) || (buf_size == NULL)) {
        DEVDRV_DRV_ERR("Invalid parameter. (dev_id=%u; subcmd=%u; buf_is_null=%d)\n", dev_id, subcmd, (buf == NULL));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (*buf_size != sizeof(unsigned char)) {
        DEVDRV_DRV_ERR("The value of buf_size is invalid. (buf_size=%u; size=%u)\n", *buf_size, sizeof(unsigned char));
        return DRV_ERROR_PARA_ERROR;
    }

#if defined(CFG_FEATURE_PSS_SIGN) && defined(CFG_FEATURE_PKCS_SIGN)
    val = (unsigned char *)buf;
    sign_info.dev_id = dev_id;
    sign_info.sign_buf = *val;

    ret = dmanage_common_ioctl(DEVDRV_MANAGER_GET_SIGN, (void *)&sign_info);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke dmanage_get_device_sign. (devid=%u; ret=%d).\n", dev_id, ret);
        return ret;
    }
    *val = sign_info.sign_buf;
#elif defined(CFG_FEATURE_PSS_SIGN)
    *(unsigned char *)buf = PKCS_SIGN_TYPE_OFF;
#else
    *(unsigned char *)buf = PKCS_SIGN_TYPE_ON;
#endif

    return DRV_ERROR_NONE;
}
#endif

int dmanage_set_device_sec_info(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int buf_size)
{
    int ret = 0;

    switch (sub_cmd) {
#ifdef CFG_FEATURE_PSS_SIGN
        case DSMI_SEC_SUB_CMD_PSS:
            ret = dmanage_set_device_sign(dev_id, sub_cmd, buf, buf_size);
            break;
#endif
        case DSMI_SEC_SUB_CMD_CC:
            ret = dms_set_cc_info(dev_id, buf, buf_size);
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to set sec information. (dev_id=%u; sub_cmd=%u; ret=%d)\n", dev_id,
                                     sub_cmd, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int dmanage_get_device_sec_info(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf,
                                unsigned int *buf_size)
{
    int ret = 0;

    switch (sub_cmd) {
#ifdef CFG_FEATURE_PSS_SIGN
        case DSMI_SEC_SUB_CMD_PSS:
            ret = dmanage_get_device_sign(dev_id, vfid, sub_cmd, buf, buf_size);
            break;
#endif
        case DSMI_SEC_SUB_CMD_CC:
            ret = dms_get_cc_info(dev_id, buf, buf_size);
            break;
        default:
            return DRV_ERROR_NOT_SUPPORT;
    }
    if (ret != 0) {
        DEVDRV_DRV_EX_NOTSUPPORT_ERR(ret, "Failed to set sec information. (dev_id=%u; sub_cmd=%u; ret=%d)\n", dev_id,
                                     sub_cmd, ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}
