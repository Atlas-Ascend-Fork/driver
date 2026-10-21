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
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ascend_hal.h>

#include "securec.h"
#include "dsmi_common_interface.h"
#include "dsmi_common.h"
#include "dev_mon_api.h"
#include "dsmi_cmd_info_def.h"

/*****************************************************************************
 Prototype    : dsmi_cmd_dft_get_elabel
 Description  : get elabel command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_dft_get_elabel(int device_id, unsigned char item_type, unsigned int eeprom_index, char *elabel_data,
                            int *len)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_READ_LABEL_DATA, device_id, (sizeof(unsigned int) + sizeof(unsigned char)),
                     ELABLE_DATA_MAX_LENGTH)
    DM_COMMAND_ADD_REQ(&item_type, (sizeof(unsigned char)))
    DM_COMMAND_ADD_REQ(&eeprom_index, (sizeof(unsigned int)))
    DM_COMMAND_SEND()
    DM_COMMAND_GET_RSP_LEN(len)
    DM_COMMAND_PUSH_OUT(elabel_data, ELABLE_DATA_MAX_LENGTH)
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_device_health
 Description  : get device health command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_device_health(int device_id, unsigned char *phealth)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_HEALTH_STATE, device_id, 0, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(phealth, sizeof(unsigned char))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_device_temperature
 Description  : get device temperature command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_device_temperature(int device_id, signed short *ptemperature)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_CHIP_TEMP, device_id, 0, sizeof(signed short))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(ptemperature, sizeof(signed short))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_device_power_info
 Description  : get device power command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_device_power_info(int device_id, unsigned short *p_power)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_POWER, device_id, 0, sizeof(unsigned short))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(p_power, sizeof(unsigned short))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_deviceid
 Description  : get device id command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_deviceid(int device_id, unsigned short *pdevice_id)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_DID, device_id, 0, sizeof(unsigned short))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pdevice_id, sizeof(unsigned short))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_venderid
 Description  : get vendor id command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_venderid(int device_id, unsigned short *pvenderid)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_VID, device_id, 0, sizeof(unsigned short))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pvenderid, sizeof(unsigned short))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_sub_venderid
 Description  : get sub vendor id command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_sub_venderid(int device_id, unsigned short *pvenderid)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_SUBVID, device_id, 0, sizeof(unsigned short))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pvenderid, sizeof(unsigned short))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_sub_deviceid
 Description  : get sub device id command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_sub_deviceid(int device_id, unsigned short *pdeviceid)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_SUBDID, device_id, 0, sizeof(unsigned short))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pdeviceid, sizeof(unsigned short))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_device_voltage
 Description  : get device voltage command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_device_voltage(int device_id, unsigned short *pvoltage)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_CHIP_VOLT, device_id, 0, sizeof(unsigned short))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pvoltage, sizeof(unsigned short))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_davinchi_info
 Description  : get device info command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_davinchi_info(int device_id, unsigned char device_info, unsigned int *result_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_D_INFO, device_id, sizeof(unsigned char), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&device_info, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(result_data, sizeof(unsigned int))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_device_flash_count
 Description  : get flash count command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_device_flash_count(int device_id, unsigned int *pflash_count)
{
    unsigned char flash_id = 0xff;

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_FLASH_D_INFO, device_id, sizeof(unsigned char), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&flash_id, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pflash_count, sizeof(unsigned int))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_device_flash_info
 Description  : get device flash info command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_device_flash_info(int device_id, unsigned char flash_id, DSMI_DEVICE_FLASH_INFO *pflash_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_FLASH_D_INFO, device_id, sizeof(unsigned char), sizeof(DSMI_DEVICE_FLASH_INFO))
    DM_COMMAND_ADD_REQ(&flash_id, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pflash_info, sizeof(DSMI_DEVICE_FLASH_INFO))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_device_die
 Description  : get device die id command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_device_die(int device_id, int soc_type, struct dsmi_soc_die_stru *pdevice_die)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_DIE_ID, device_id, sizeof(int), sizeof(DSMI_DIE_ID))
    DM_COMMAND_ADD_REQ(&soc_type, sizeof(int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pdevice_die, sizeof(struct dsmi_soc_die_stru))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_ecc_info
 Description  : get ecc info command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_ecc_info(int device_id, unsigned char device_info, DSMI_ECC_STATICS_RESULT *pecc_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_QUERY_ECC_STAT, device_id, sizeof(unsigned char), sizeof(DSMI_ECC_STATICS_RESULT))
    DM_COMMAND_ADD_REQ(&device_info, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pecc_info, sizeof(DSMI_ECC_STATICS_RESULT))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_system_time
 Description  : get system time command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_system_time(int device_id, unsigned int *ntime_stamp)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_SYSTEM_TIME, device_id, 0, sizeof(unsigned int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(ntime_stamp, sizeof(unsigned int))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_config_enable
 Description  : device config enable command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_config_enable(int device_id, DSMI_CONFIG_PARA config_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_CONFIG_ENABLE, device_id, sizeof(DSMI_CONFIG_PARA), 0)
    DM_COMMAND_ADD_REQ(&config_info, sizeof(DSMI_CONFIG_PARA))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_enable
 Description  : get enable status command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_enable(int device_id, unsigned char device_type, unsigned char config_item, unsigned char *enable_flag)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_ENABLE, device_id, (sizeof(unsigned char) + sizeof(unsigned char)),
                     sizeof(unsigned char))
    DM_COMMAND_ADD_REQ(&device_type, sizeof(unsigned char))
    DM_COMMAND_ADD_REQ(&config_item, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(enable_flag, sizeof(unsigned char))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_set_mac_addr
 Description  : set mac addr command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_set_mac_addr(int device_id, DSMI_MAC_PARA mac_para, const char *pmac_addr)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_MAC_ADDR, device_id, (sizeof(DSMI_MAC_PARA) + MAC_ADDR_LEN), 0)
    DM_COMMAND_ADD_REQ(&mac_para, sizeof(DSMI_MAC_PARA))
    DM_COMMAND_ADD_REQ(pmac_addr, MAC_ADDR_LEN)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_mac_count
 Description  : get mac count command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_mac_count(int device_id, unsigned char *count)
{
    DSMI_MAC_PARA mac_para = {0};
    mac_para.mac_type = MAC_COUNT_TYPE;
    mac_para.mac_id = 0;

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_MAC_ADDR, device_id, sizeof(DSMI_MAC_PARA), sizeof(unsigned char))
    DM_COMMAND_ADD_REQ(&mac_para, sizeof(DSMI_MAC_PARA))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(count, sizeof(unsigned char))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_mac_addr
 Description  : get mac address command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_mac_addr(int device_id, int mac_id, char *pmac_addr)
{
    DSMI_MAC_PARA mac_para = {0};
    mac_para.mac_type = MAC_INFO_TYPE;
    mac_para.mac_id = mac_id;

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_MAC_ADDR, device_id, sizeof(DSMI_MAC_PARA), MAC_ADDR_LEN)
    DM_COMMAND_ADD_REQ(&mac_para, sizeof(DSMI_MAC_PARA))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pmac_addr, MAC_ADDR_LEN)
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_set_device_ip_address
 Description  : set device ip address command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_set_device_ip_address(int device_id, DSMI_PORT_PARA port_para, IPADDR_ST ip_addr, IPADDR_ST netmask)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_IP_ADDR, device_id, (sizeof(DSMI_PORT_PARA) + sizeof(DSMI_ADDR_PARA)), 0)
    DM_COMMAND_ADD_REQ(&port_para, sizeof(DSMI_PORT_PARA))
    DM_COMMAND_ADD_REQ(&ip_addr, sizeof(IPADDR_ST))
    DM_COMMAND_ADD_REQ(&netmask, sizeof(IPADDR_ST))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_device_ip_address
 Description  : get device ip address command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_device_ip_address(int device_id, DSMI_PORT_PARA port_para, IPADDR_ST *ip_addr, IPADDR_ST *netmask)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_IP_ADDR, device_id, (sizeof(DSMI_PORT_PARA)),
                     (sizeof(IPADDR_ST) + sizeof(IPADDR_ST)))
    DM_COMMAND_ADD_REQ(&port_para, sizeof(DSMI_PORT_PARA))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(ip_addr, sizeof(IPADDR_ST))
    DM_COMMAND_PUSH_OUT(netmask, sizeof(IPADDR_ST))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_set_device_gtw_address
 Description  : set gateway address command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_set_device_gtw_address(int device_id, DSMI_PORT_PARA port_para, IPADDR_ST gtw_addr)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_GTW_ADDR, device_id, (sizeof(DSMI_PORT_PARA) + sizeof(IPADDR_ST)), 0)
    DM_COMMAND_ADD_REQ(&port_para, sizeof(DSMI_PORT_PARA))
    DM_COMMAND_ADD_REQ(&gtw_addr, sizeof(IPADDR_ST))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_device_gtw_address
 Description  : get device gateway address command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_device_gtw_address(int device_id, DSMI_PORT_PARA port_para, IPADDR_ST *gtw_addr)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_GTW_ADDR, device_id, sizeof(DSMI_PORT_PARA), sizeof(IPADDR_ST))
    DM_COMMAND_ADD_REQ(&port_para, sizeof(DSMI_PORT_PARA))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(gtw_addr, sizeof(IPADDR_ST))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_search_network_device_info
Description  : get device gateway address command proc
 Return Value : 0 success

*****************************************************************************/
__attribute__((visibility("default"))) int dsmi_cmd_get_network_device_info(int device_id, const char *inbuf,
                                                                            unsigned int size_in, char *outbuf,
                                                                            unsigned int *size_out)
{
    unsigned int out_length = 0;

    if (inbuf == NULL || outbuf == NULL || size_out == NULL || size_in > USHORT_MAX || *size_out > MAX_OUTPUT_LEN) {
        DEV_MON_ERR("devid %d para error.\n", device_id);
        return -EINVAL;
    }

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_NET_DEV_INFO, device_id,
                     (unsigned short)(sizeof(unsigned int) + size_in + sizeof(unsigned int)),
                     (unsigned short)(sizeof(unsigned int) + (*size_out)))

    DM_COMMAND_ADD_REQ(&size_in, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(inbuf, size_in)
    DM_COMMAND_ADD_REQ(size_out, sizeof(unsigned int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(&out_length, sizeof(unsigned int))

    if (out_length <= *size_out) {
        *size_out = out_length;
    } else {
        DEV_MON_WARNING("devid %d outbuflen not enough, size_out %u, real_out %u\n", device_id, *size_out, out_length);
    }
    DM_COMMAND_PUSH_OUT(outbuf, (*size_out))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_fan_info
 Description  : get fan info command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_fan_info(int device_id, DSMI_FAN_INFO *fan_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_FAN_INFO, device_id, 0, sizeof(DSMI_FAN_INFO))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(fan_info, sizeof(DSMI_FAN_INFO))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_soc_sensor_info
 Description  : get sensor info command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_soc_sensor_info(int device_id, unsigned char sensor_id, TAG_SENSOR_INFO *tsensor_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_MINI_SENSOR_INFO, device_id, sizeof(unsigned char), sizeof(TAG_SENSOR_INFO))
    DM_COMMAND_ADD_REQ(&sensor_id, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(tsensor_info, sizeof(TAG_SENSOR_INFO))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_component_list
 Description  : get component list command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_component_list(int device_id, unsigned long long *component_table)
{
    unsigned char ctl_cmd = GET_SUPPORT_COMPONENT;

    DM_COMMAND_BIGIN(DMP_LSB_OP_CODE_UPGRADE_CTRL, device_id, sizeof(unsigned char), sizeof(unsigned long long))
    DM_COMMAND_ADD_REQ(&ctl_cmd, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(component_table, sizeof(unsigned long long))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_upgrade_ctl_cmd
 Description  : upgrade ctl  command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_upgrade_ctl_cmd(int device_id, unsigned char ctl_cmd)
{
    DM_COMMAND_BIGIN(DMP_LSB_OP_CODE_UPGRADE_CTRL, device_id, sizeof(unsigned char), 0)
    DM_COMMAND_ADD_REQ(&ctl_cmd, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_update_send_file_name
 Description  : upgrade send file name command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_update_send_file_name(int device_id, unsigned char component_type, const char *file_name,
                                   unsigned int data_len)
{
    unsigned char ctl = TRANSMIT_FILE;

    DM_COMMAND_BIGIN(DMP_LSB_OP_CODE_UPGRADE_CTRL, device_id,
                     (unsigned int)((sizeof(unsigned char) + sizeof(unsigned char)) + data_len), 0)
    DM_COMMAND_ADD_REQ(&ctl, sizeof(unsigned char))
    DM_COMMAND_ADD_REQ(&component_type, sizeof(unsigned char))
    DM_COMMAND_ADD_REQ(file_name, data_len)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_load_patch(int device_id, const char *file_name, unsigned int data_len)
{
    unsigned char ctl = UPGRADE_PATCH;

    DM_COMMAND_BIGIN(DEV_MON_CMD_HOT_PATCH_OPERATION, device_id, (unsigned int)(sizeof(unsigned char) + data_len), 0)
    DM_COMMAND_ADD_REQ(&ctl, sizeof(unsigned char))
    DM_COMMAND_ADD_REQ(file_name, data_len)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_unload_patch(int device_id, unsigned char ctl_cmd)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_HOT_PATCH_OPERATION, device_id, sizeof(unsigned char), 0)
    DM_COMMAND_ADD_REQ(&ctl_cmd, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_load_mami_patch(int device_id, unsigned char mami_patch_type, const char *file_name, unsigned int data_len)
{
    unsigned char ctl = UPGRADE_MAMI_PACH;

    DM_COMMAND_BIGIN(DEV_MON_CMD_ABL_MAMI_PATCH_OPERATION, device_id,
                     (unsigned int)(sizeof(unsigned char) + sizeof(unsigned char) + data_len), 0)
    DM_COMMAND_ADD_REQ(&ctl, sizeof(unsigned char))
    DM_COMMAND_ADD_REQ(&mami_patch_type, sizeof(unsigned char))
    DM_COMMAND_ADD_REQ(file_name, data_len)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}
/*****************************************************************************
 Prototype    : dsmi_cmd_upgrade_get_state
 Description  : get upgrade state command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_upgrade_get_state(int device_id, unsigned char com_type, unsigned char *schedule,
                               unsigned char *upgrade_status)
{
    DM_COMMAND_BIGIN(DMP_LSB_OP_CODE_UPGRADE_STATE, device_id, sizeof(unsigned char),
                     (sizeof(unsigned char) + sizeof(unsigned char)))
    DM_COMMAND_ADD_REQ(&com_type, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(upgrade_status, sizeof(unsigned char))
    DM_COMMAND_PUSH_OUT(schedule, sizeof(unsigned char))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_upgrade_get_version
 Description  : get version command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_upgrade_get_version(int device_id, unsigned char component_type, unsigned char *version_str,
                                 unsigned int *len)
{
    DM_COMMAND_BIGIN(DMP_LSB_OP_CODE_UPGRADE_VERSION, device_id, sizeof(unsigned char), FW_VERSION_MAX_LENGTH)
    DM_COMMAND_ADD_REQ(&component_type, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(version_str, FW_VERSION_MAX_LENGTH)
    DM_COMMAND_GET_RSP_LEN(len)
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_board_id
 Description  : get board id command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_board_id(int device_id, unsigned int *board_id)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_BOARD_ID, device_id, 0, sizeof(unsigned int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(board_id, sizeof(unsigned int))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_pcb_id
 Description  : get pcb id command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_pcb_id(int device_id, unsigned char *pcb_id)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_PCB_VERSION, device_id, 0, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pcb_id, sizeof(unsigned char))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_board_information
 Description  : get board information command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_board_information(int device_id, unsigned char info_type, unsigned int *id_value)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_BOARD_INFO, device_id, sizeof(unsigned char), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&info_type, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(id_value, sizeof(unsigned int))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_user_config
 Description  : get user config
 Return Value : 0 success
             other value is error
*****************************************************************************/
int dsmi_cmd_get_user_config(int device_id, unsigned char config_name_len, const char *config_name,
                             unsigned int *p_buf_size, unsigned char *buf)
{
    unsigned int config_name_len_tmp = (unsigned int)config_name_len;

    DM_COMMAND_BIGIN(DMP_MON_CMD_GET_USER_CONFIG, device_id,
                     (unsigned short)((unsigned int)config_name_len_tmp + sizeof(char) + sizeof(int)),
                     (unsigned short)(*p_buf_size))
    DM_COMMAND_ADD_REQ(p_buf_size, sizeof(int))
    DM_COMMAND_ADD_REQ(&config_name_len, sizeof(char))
    DM_COMMAND_ADD_REQ(config_name, config_name_len)
    DM_COMMAND_SEND()
    DM_COMMAND_GET_RSP_LEN(p_buf_size)
    DM_COMMAND_PUSH_OUT(buf, *p_buf_size)
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_set_user_config
 Description  : get user config
 Return Value : 0 success
             other value is error
*****************************************************************************/
int dsmi_cmd_set_user_config(int device_id, unsigned char config_name_len, const char *config_name,
                             unsigned int buf_size, const unsigned char *buf)
{
    DM_COMMAND_BIGIN(DMP_MON_CMD_SET_USER_CONFIG, device_id,
                     (unsigned short)(config_name_len + buf_size + sizeof(char) + sizeof(int)), 0)
    DM_COMMAND_ADD_REQ(&config_name_len, sizeof(char))
    DM_COMMAND_ADD_REQ(&buf_size, sizeof(int))
    DM_COMMAND_ADD_REQ(config_name, config_name_len)
    DM_COMMAND_ADD_REQ(buf, buf_size)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_clear_user_config
 Description  : get user config
 Return Value : 0 success
             other value is error
*****************************************************************************/
int dsmi_cmd_clear_user_config(int device_id, unsigned char config_name_len, const char *config_name)
{
    unsigned int config_name_len_tmp = (unsigned int)config_name_len;
    DM_COMMAND_BIGIN(DMP_MON_CMD_CLEAR_USER_CONFIG, device_id,
                     (unsigned short)((unsigned int)config_name_len_tmp + sizeof(char)), 0)
    DM_COMMAND_ADD_REQ(&config_name_len, sizeof(char))
    DM_COMMAND_ADD_REQ(config_name, config_name_len)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_network_health
 Description  : get net work status form net
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_network_health(int device_id, unsigned int *presult)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_NETWORK_HEALTH, device_id, 0, sizeof(unsigned int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(presult, sizeof(unsigned int))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_errorstring
 Description  : get device errorcode elabel command proc
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_errorstring(int device_id, unsigned int errcode, unsigned char *perrinfo, int buffsize)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_ERRSTR, device_id, (unsigned short)(sizeof(unsigned int) + sizeof(int)), buffsize)
    DM_COMMAND_ADD_REQ(&errcode, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&buffsize, sizeof(int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(perrinfo, buffsize)
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_get_llc_perf_para
 Description  : get LLC performance parameter
 Return Value : 0 success

*****************************************************************************/
int dsmi_cmd_get_llc_perf_para(int device_id, DSMI_LLC_RX_RESULT *perf_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_LLC_PERF_PARA, device_id, 0, sizeof(DSMI_LLC_RX_RESULT))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(perf_info, sizeof(DSMI_LLC_RX_RESULT))
    DM_COMMAND_END()
}

int dsmi_cmd_get_aicpu_info(int device_id, struct dsmi_aicpu_info_stru *pdevice_aicpu_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_AICPU_INFO, device_id, 0, sizeof(struct dsmi_aicpu_info_stru))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pdevice_aicpu_info, sizeof(struct dsmi_aicpu_info_stru))
    DM_COMMAND_END()
}

/*****************************************************************************
 Prototype    : dsmi_cmd_set_sec_revocation
 Description  : revocation operation
 Return Value : 0     : success
                not 0 : fail

*****************************************************************************/
int dsmi_cmd_set_sec_revocation(int device_id, DSMI_REVOCATION_TYPE revo_type, const unsigned char *file_data,
                                unsigned int file_size)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_REVOCATION, device_id,
                     (unsigned int)(sizeof(DSMI_REVOCATION_TYPE) + sizeof(unsigned int) + file_size), 0)
    DM_COMMAND_ADD_REQ(&revo_type, sizeof(DSMI_REVOCATION_TYPE))
    DM_COMMAND_ADD_REQ(&file_size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(file_data, file_size)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_set_power_state(int device_id, struct dsmi_power_state_info_stru *power_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_POWER_STATE, device_id, sizeof(struct dsmi_power_state_info_stru), 0)
    DM_COMMAND_ADD_REQ(power_info, sizeof(struct dsmi_power_state_info_stru))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_get_hiss_status(int device_id, struct dsmi_hiss_status_stru *hiss_status_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_HISSSTATUS, device_id, 0, sizeof(struct dsmi_hiss_status_stru))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(hiss_status_data, sizeof(struct dsmi_hiss_status_stru))
    DM_COMMAND_END()
}

int dsmi_cmd_get_lp_status(int device_id, struct dsmi_lp_status_stru *lp_status_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_LPSTATUS, device_id, 0, sizeof(struct dsmi_lp_status_stru))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(lp_status_data, sizeof(struct dsmi_lp_status_stru))
    DM_COMMAND_END()
}

int dsmi_cmd_get_can_status(int device_id, const char *name, unsigned int name_len,
                            struct dsmi_can_status_stru *canstatus_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_CAN_STATUS, device_id, (unsigned short)(sizeof(unsigned int) + name_len),
                     (unsigned short)(sizeof(struct dsmi_can_status_stru)))
    DM_COMMAND_ADD_REQ(&name_len, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(name, name_len)
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(canstatus_data, sizeof(struct dsmi_can_status_stru))
    DM_COMMAND_END()
}

int dsmi_cmd_get_ufs_status(int device_id, struct dsmi_ufs_status_stru *ufsstatus_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_UFS_STATUS, device_id, 0, sizeof(struct dsmi_ufs_status_stru))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(ufsstatus_data, sizeof(struct dsmi_ufs_status_stru))
    DM_COMMAND_END()
}

int dsmi_cmd_get_sensorhub_status(int device_id, struct dsmi_sensorhub_status_stru *sensorhubstatus_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_SENSORHUB_STATUS, device_id, 0, sizeof(struct dsmi_sensorhub_status_stru))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(sensorhubstatus_data, sizeof(struct dsmi_sensorhub_status_stru))
    DM_COMMAND_END()
}

int dsmi_cmd_get_can_config(int device_id, const char *name, unsigned int name_len,
                            struct dsmi_can_config_stru *canconfig_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_CAN_CONFIG, device_id, (unsigned short)(sizeof(unsigned int) + name_len),
                     (unsigned short)(sizeof(struct dsmi_can_config_stru)))
    DM_COMMAND_ADD_REQ(&name_len, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(name, name_len)
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(canconfig_data, sizeof(struct dsmi_can_config_stru))
    DM_COMMAND_END()
}

int dsmi_cmd_get_sensorhub_config(int device_id, struct dsmi_sensorhub_config_stru *sensorhubconfig_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_SENSORHUB_CONFIG, device_id, 0, sizeof(struct dsmi_sensorhub_config_stru))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(sensorhubconfig_data, sizeof(struct dsmi_sensorhub_config_stru))
    DM_COMMAND_END()
}

int dsmi_cmd_get_gpio_status(int device_id, unsigned int gpio_num, unsigned int *status)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_GPIO_STATUS, device_id, sizeof(unsigned int), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&gpio_num, sizeof(unsigned int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(status, sizeof(unsigned int))
    DM_COMMAND_END()
}

int dsmi_cmd_get_soc_hw_fault(int device_id, struct dsmi_emu_subsys_state_stru *emu_subsys_state_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_SOC_HW_FAULT, device_id, 0, sizeof(struct dsmi_emu_subsys_state_stru))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(emu_subsys_state_data, sizeof(struct dsmi_emu_subsys_state_stru))
    DM_COMMAND_END()
}

int dsmi_cmd_get_safetyisland_status(int device_id, struct dsmi_safetyisland_status_stru *emu_subsys_state_data)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_SAFETYISLAND_STATUS, device_id, 0, sizeof(struct dsmi_safetyisland_status_stru))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(emu_subsys_state_data, sizeof(struct dsmi_safetyisland_status_stru))
    DM_COMMAND_END()
}

int dsmi_cmd_get_device_cgroup_info(int device_id, struct tag_cgroup_info *cg_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_CGROUP_INFO, device_id, 0, sizeof(struct tag_cgroup_info))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(cg_info, sizeof(struct tag_cgroup_info))
    DM_COMMAND_END()
}

int dsmi_cmd_set_device_info(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                             unsigned int buf_size)
{
    if (buf == NULL || buf_size > USHORT_MAX) {
        DEV_MON_ERR("devid %d para error.\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_DEVICE_INFO, dev_id,
                     (unsigned short)(sizeof(DSMI_MAIN_CMD) + sizeof(unsigned int) + sizeof(unsigned int) + buf_size),
                     0)
    DM_COMMAND_ADD_REQ(&main_cmd, sizeof(DSMI_MAIN_CMD))
    DM_COMMAND_ADD_REQ(&sub_cmd, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&buf_size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, buf_size)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

drvError_t dsmi_cmd_set_device_info_ex(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd,
                                       const void *buf, unsigned int buf_size)
{
    if (buf == NULL || buf_size > USHORT_MAX) {
        DEV_MON_ERR("Parameter error. (devid=%u; buf_size=%u; buf_size_max=%u)\n", dev_id, buf_size, USHORT_MAX);
        return DRV_ERROR_PARA_ERROR;
    }

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_DEVICE_INFO_EX, dev_id,
                     (unsigned short)(sizeof(DSMI_MAIN_CMD) + sizeof(unsigned int) + sizeof(unsigned int) + buf_size),
                     0)
    DM_COMMAND_ADD_REQ(&main_cmd, sizeof(DSMI_MAIN_CMD))
    DM_COMMAND_ADD_REQ(&sub_cmd, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&buf_size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, buf_size)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_get_device_info_critical(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                                      unsigned int *size)
{
    unsigned int out_length = 0;

    if (buf == NULL || size == NULL || *size > USHORT_MAX) {
        DEV_MON_ERR("devid %d para error.\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_DEVICE_INFO_CRITICAL, (int)dev_id,
                     (unsigned short)(sizeof(DSMI_MAIN_CMD) + sizeof(unsigned int) + sizeof(unsigned int) + *size),
                     (unsigned short)(sizeof(unsigned int) + (*size)))
    DM_COMMAND_ADD_REQ(&main_cmd, sizeof(DSMI_MAIN_CMD))
    DM_COMMAND_ADD_REQ(&sub_cmd, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, *size)
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(&out_length, sizeof(unsigned int))

    if (out_length <= *size) {
        *size = out_length;
    } else {
        DEV_MON_WARNING("devid %d outbuflen not enough, size_out %u, real_out %u\n", dev_id, *size, out_length);
    }

    DM_COMMAND_PUSH_OUT(buf, (*size))
    DM_COMMAND_END()
}

drvError_t dsmi_cmd_set_device_info_critical(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd,
                                             const void *buf, unsigned int buf_size)
{
    if (buf == NULL || buf_size > USHORT_MAX) {
        DEV_MON_ERR("Para error (devid=%u, buf_size=%u).\n", dev_id, buf_size);
        return DRV_ERROR_PARA_ERROR;
    }

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_DEVICE_INFO_CRITICAL, (int)dev_id,
                     (unsigned short)(sizeof(DSMI_MAIN_CMD) + sizeof(unsigned int) + sizeof(unsigned int) + buf_size),
                     0)
    DM_COMMAND_ADD_REQ(&main_cmd, sizeof(DSMI_MAIN_CMD))
    DM_COMMAND_ADD_REQ(&sub_cmd, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&buf_size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, buf_size)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_get_device_info(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                             unsigned int *size)
{
    unsigned int out_length = 0;

    if (buf == NULL || size == NULL || *size > USHORT_MAX) {
        DEV_MON_ERR("devid %d para error.\n", dev_id);
        return DRV_ERROR_PARA_ERROR;
    }

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_DEVICE_INFO, dev_id,
                     (unsigned short)(sizeof(DSMI_MAIN_CMD) + sizeof(unsigned int) + sizeof(unsigned int) + *size),
                     (unsigned short)(sizeof(unsigned int) + (*size)))
    DM_COMMAND_ADD_REQ(&main_cmd, sizeof(DSMI_MAIN_CMD))
    DM_COMMAND_ADD_REQ(&sub_cmd, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, *size)
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(&out_length, sizeof(unsigned int))

    if (out_length <= *size) {
        *size = out_length;
    } else {
        DEV_MON_WARNING("devid %d outbuflen not enough, size_out %u, real_out %u\n", dev_id, *size, out_length);
    }

    DM_COMMAND_PUSH_OUT(buf, (*size))
    DM_COMMAND_END()
}

int dsmi_cmd_create_capability_group(int device_id, int ts_id, struct dsmi_capability_group_info *group_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_CREATE_CAPABILITY_GROUP, device_id,
                     sizeof(int) + sizeof(struct dsmi_capability_group_info), 0)
    DM_COMMAND_ADD_REQ(&ts_id, sizeof(int))
    DM_COMMAND_ADD_REQ(group_info, sizeof(struct dsmi_capability_group_info))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_delete_capability_group(int device_id, int ts_id, int group_id)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_DELETE_CAPABILITY_GROUP, device_id, sizeof(ts_id) + sizeof(group_id), 0)
    DM_COMMAND_ADD_REQ(&ts_id, sizeof(int))
    DM_COMMAND_ADD_REQ(&group_id, sizeof(int))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_get_capability_group_info(int device_id, int ts_id, int group_id,
                                       struct dsmi_capability_group_info *group_info, int group_count)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_CAPABILITY_GROUP_INFO, device_id,
                     (unsigned short)(sizeof(ts_id) + sizeof(group_id) + sizeof(group_count)),
                     (unsigned short)((unsigned long)group_count * sizeof(struct dsmi_capability_group_info)))
    DM_COMMAND_ADD_REQ(&ts_id, sizeof(int))
    DM_COMMAND_ADD_REQ(&group_id, sizeof(int))
    DM_COMMAND_ADD_REQ(&group_count, sizeof(int))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(group_info, (long unsigned int)group_count * sizeof(struct dsmi_capability_group_info))
    DM_COMMAND_END()
}

int dsmi_cmd_create_capability_multi_group(int device_id, struct dsmi_capability_multi_group_info *group_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_CREATE_CAPABILITY_MULTI_GROUP, device_id,
                     (unsigned short)sizeof(struct dsmi_capability_multi_group_info), 0)
    DM_COMMAND_ADD_REQ(group_info, sizeof(struct dsmi_capability_multi_group_info))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_delete_capability_multi_group(int device_id, struct dsmi_capability_multi_group_info *group_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_DELETE_CAPABILITY_MULTI_GROUP, device_id,
                     (unsigned short)sizeof(struct dsmi_capability_multi_group_info), 0)
    DM_COMMAND_ADD_REQ(group_info, sizeof(struct dsmi_capability_multi_group_info))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_get_capability_multi_group(int device_id, struct dsmi_capability_multi_group_info *group_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_CAPABILITY_MULTI_GROUP, device_id,
                     (unsigned short)sizeof(struct dsmi_capability_multi_group_info),
                     (unsigned short)sizeof(struct dsmi_capability_multi_group_info))
    DM_COMMAND_ADD_REQ(group_info, sizeof(struct dsmi_capability_multi_group_info))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(group_info, sizeof(struct dsmi_capability_multi_group_info))
    DM_COMMAND_END()
}

int dsmi_cmd_get_last_bootstate(int device_id, BOOT_TYPE boot_type, unsigned int *state)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_LAST_BOOTSTATE, device_id, sizeof(BOOT_TYPE), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&boot_type, sizeof(BOOT_TYPE))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(state, sizeof(unsigned int))
    DM_COMMAND_END()
}

int dsmi_cmd_ctrl_device_node(int device_id, struct dsmi_dtm_node_s dtm_node, DSMI_DTM_OPCODE opcode, IN_OUT_BUF buf)
{
    DM_COMMAND_BIGIN(
        DEV_MON_CMD_D_CTRL_DEVICE_NODE, device_id,
        (unsigned short)(sizeof(struct dsmi_dtm_node_s) + sizeof(DSMI_DTM_OPCODE) + sizeof(unsigned int) + buf.in_size),
        (unsigned short)(sizeof(unsigned int) + buf.in_size))
    DM_COMMAND_ADD_REQ(&dtm_node, sizeof(struct dsmi_dtm_node_s))
    DM_COMMAND_ADD_REQ(&opcode, sizeof(DSMI_DTM_OPCODE))
    DM_COMMAND_ADD_REQ(&(buf.in_size), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf.in_buf, buf.in_size)
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(&(buf.in_size), sizeof(unsigned int))
    DM_COMMAND_PUSH_OUT(buf.in_buf, buf.in_size)
    DM_COMMAND_END()
}

int dsmi_cmd_get_all_device_node(int device_id, DEV_DTM_CAP capability, struct dsmi_dtm_node_s node_info[],
                                 unsigned int *size)
{
    unsigned int out_length = 0;

    if (size == NULL || *size > INPUT_SIZE_MAX) {
        DEV_MON_ERR("device_id %d para error.\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }
    DM_COMMAND_BIGIN(
        DEV_MON_CMD_D_GET_ALL_DEVICE_NODE, device_id,
        (unsigned short)(sizeof(unsigned int) + sizeof(unsigned int) + sizeof(struct dsmi_dtm_node_s) * (*size)),
        (unsigned short)(sizeof(unsigned int) + (*size) * sizeof(struct dsmi_dtm_node_s)))
    DM_COMMAND_ADD_REQ(&capability, sizeof(DEV_DTM_CAP))
    DM_COMMAND_ADD_REQ(size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(node_info, (*size) * sizeof(struct dsmi_dtm_node_s))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(&out_length, sizeof(unsigned int))
    if (out_length <= *size) {
        *size = out_length;
    } else {
        DEV_MON_WARNING("device_id %d outbuflen not enough, size_out %u, real_out %u\n", device_id, *size, out_length);
    }
    DM_COMMAND_PUSH_OUT(node_info, (*size) * sizeof(struct dsmi_dtm_node_s))
    DM_COMMAND_END()
}

int dsmi_cmd_get_reboot_reason(int device_id, struct dsmi_reboot_reason *reboot_reason)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_REBOOT_REASON, device_id, 0, sizeof(struct dsmi_reboot_reason))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(reboot_reason, sizeof(struct dsmi_reboot_reason))
    DM_COMMAND_END()
}

int dsmi_cmd_set_bist_info(int device_id, DSMI_BIST_CMD cmd, const void *buf, unsigned int buf_size)
{
    if (buf_size > USHORT_MAX) {
        DEV_MON_ERR("size %d is out of range.\n", buf_size);
        return DRV_ERROR_PARA_ERROR;
    }

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_BIST_INFO, device_id,
                     (unsigned short)(sizeof(DSMI_BIST_CMD) + sizeof(unsigned int) + buf_size), 0)
    DM_COMMAND_ADD_REQ(&cmd, sizeof(DSMI_BIST_CMD))
    DM_COMMAND_ADD_REQ(&buf_size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, buf_size)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_get_bist_info(int device_id, DSMI_BIST_CMD cmd, void *buf, unsigned int *size)
{
    unsigned int out_length = 0;

    if (*size > USHORT_MAX) {
        DEV_MON_ERR("size %d is out of range, max %u.\n", *size, USHORT_MAX);
        return DRV_ERROR_PARA_ERROR;
    }

    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_BIST_INFO, device_id,
                     (unsigned short)(sizeof(DSMI_BIST_CMD) + sizeof(unsigned int) + *size),
                     (unsigned short)(sizeof(unsigned int) + (*size)))
    DM_COMMAND_ADD_REQ(&cmd, sizeof(DSMI_BIST_CMD))
    DM_COMMAND_ADD_REQ(size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, *size)
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(&out_length, sizeof(unsigned int))

    if (out_length <= *size) {
        *size = out_length;
    } else {
        DEV_MON_WARNING("devid %d outbuflen not enough, size_out %u, real_out %u\n", device_id, *size, out_length);
    }

    DM_COMMAND_PUSH_OUT(buf, (*size))
    DM_COMMAND_END()
}

#if defined CFG_FEATURE_ECC_HBM_INFO || defined CFG_FEATURE_ECC_DDR_INFO
int dsmi_cmd_get_total_ecc_isolated_pages_info(int device_id, unsigned char module_type,
                                               struct dsmi_ecc_pages_stru *pdevice_ecc_pages_statistics)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_GET_ISOLATED_PAGES_INFO, device_id, sizeof(unsigned char),
                     sizeof(struct dsmi_ecc_pages_stru))
    DM_COMMAND_ADD_REQ(&module_type, sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(pdevice_ecc_pages_statistics, sizeof(struct dsmi_ecc_pages_stru))
    DM_COMMAND_END()
}

int dsmi_cmd_clear_ecc_isolated_info(int device_id)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_CLEAR_ISOLATED_INFO, device_id, 0, 0)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}
#endif

int dsmi_cmd_fault_inject(DSMI_FAULT_INJECT_INFO info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_DMS_FAULT_INJECT, info.device_id, sizeof(DSMI_FAULT_INJECT_INFO), 0)
    DM_COMMAND_ADD_REQ(&info, sizeof(DSMI_FAULT_INJECT_INFO))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_get_flash_content(int device_id, DSMI_FLASH_CONTENT *content_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_FLASH_CONTENT, device_id,
                     (unsigned short)(content_info->size * sizeof(unsigned char) + sizeof(unsigned int) +
                                      sizeof(unsigned int) + sizeof(unsigned int)),
                     (unsigned short)(content_info->size * sizeof(unsigned char)))
    DM_COMMAND_ADD_REQ(&(content_info->type), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&(content_info->size), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&(content_info->offset), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(content_info->buf, content_info->size * sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(content_info->buf, content_info->size * sizeof(unsigned char))
    DM_COMMAND_END()
}

int dsmi_cmd_set_flash_content(int device_id, DSMI_FLASH_CONTENT *content_info)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_SET_FLASH_CONTENT, device_id,
                     (unsigned short)(content_info->size * sizeof(unsigned char) + sizeof(unsigned int) +
                                      sizeof(unsigned int) + sizeof(unsigned int)),
                     0)
    DM_COMMAND_ADD_REQ(&(content_info->type), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&(content_info->size), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&(content_info->offset), sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(content_info->buf, content_info->size * sizeof(unsigned char))
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_get_device_state(int device_id, void *in_buf, unsigned long in_size, unsigned long *out_size)
{
    DM_COMMAND_BIGIN(DEV_MON_CMD_D_GET_DEVICE_STATE, device_id, (unsigned short)(sizeof(unsigned long) + in_size),
                     (unsigned short)(sizeof(unsigned long) + in_size))
    DM_COMMAND_ADD_REQ(&in_size, sizeof(unsigned long))
    DM_COMMAND_ADD_REQ(in_buf, in_size)
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(out_size, sizeof(unsigned long))
    if (*out_size != 0) {
        DM_COMMAND_PUSH_OUT(in_buf, *out_size);
    }
    DM_COMMAND_END()
}

int dsmi_cmd_set_detect_info(unsigned int dev_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                             unsigned int buf_size)
{
    if ((buf == NULL) || (buf_size > USHORT_MAX)) {
        DEV_MON_ERR("para error, (devid=%u, buf_size=%u).\n", dev_id, buf_size);
        return DRV_ERROR_PARA_ERROR;
    }

    DM_COMMAND_BIGIN(
        DEV_MON_CMD_D_SET_DETECT_INFO, dev_id,
        (unsigned short)(sizeof(DSMI_DETECT_MAIN_CMD) + sizeof(unsigned int) + sizeof(unsigned int) + buf_size), 0)
    DM_COMMAND_ADD_REQ(&main_cmd, sizeof(DSMI_DETECT_MAIN_CMD))
    DM_COMMAND_ADD_REQ(&sub_cmd, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(&buf_size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, buf_size)
    DM_COMMAND_SEND()
    DM_COMMAND_END()
}

int dsmi_cmd_get_detect_info(unsigned int dev_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                             unsigned int *size)
{
    unsigned int out_length = 0;

    if ((buf == NULL) || (size == NULL) || (*size > USHORT_MAX)) {
        DEV_MON_ERR("para error, (devid=%u, buf_null=%d, size_null=%d, size=%u, max=%u).\n", dev_id, (buf == NULL),
                    (size == NULL), (size != NULL) ? *size : 0, USHORT_MAX);
        return DRV_ERROR_PARA_ERROR;
    }

    DM_COMMAND_BIGIN(
        DEV_MON_CMD_D_GET_DETECT_INFO, dev_id,
        (unsigned short)(sizeof(DSMI_DETECT_MAIN_CMD) + sizeof(unsigned int) + sizeof(unsigned int) + *size),
        (unsigned short)(sizeof(unsigned int) + (*size)))
    DM_COMMAND_ADD_REQ(&main_cmd, sizeof(DSMI_DETECT_MAIN_CMD))
    DM_COMMAND_ADD_REQ(&sub_cmd, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(size, sizeof(unsigned int))
    DM_COMMAND_ADD_REQ(buf, *size)
    DM_COMMAND_SEND()
    DM_COMMAND_PUSH_OUT(&out_length, sizeof(unsigned int))

    if (out_length <= *size) {
        *size = out_length;
    } else {
        DEV_MON_WARNING("outbuflen not enough, (devid=%u, size_out=%u, real_out=%u)\n", dev_id, *size, out_length);
    }

    DM_COMMAND_PUSH_OUT(buf, (*size))
    DM_COMMAND_END()
}