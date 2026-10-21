/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DSMI_DMP_COMMAND_H__
#define __DSMI_DMP_COMMAND_H__
#include "dev_mon_api.h"
#include "dsmi_common.h"

#define DSMI_IPV6_LENTH 0X10
#ifndef DSMI_BB_ERRSTR_LENGTH
#define DSMI_BB_ERRSTR_LENGTH 48
#endif
#ifndef DSMI_BB_EVENTSTR_LENGTH
#define DSMI_BB_EVENTSTR_LENGTH 256
#endif

int dsmi_cmd_dft_get_elabel(int device_id, unsigned char item_type, unsigned int eeprom_index, char *elabel_data,
                            int *len);
int dsmi_cmd_config_enable(int device_id, DSMI_CONFIG_PARA config_info);
int dsmi_cmd_get_board_id(int device_id, unsigned int *board_id);
int dsmi_cmd_get_board_information(int device_id, unsigned char info_type, unsigned int *id_value);
int dsmi_cmd_get_soc_sensor_info(int device_id, unsigned char sensor_id, TAG_SENSOR_INFO *tsensor_info);
int dsmi_cmd_get_component_list(int device_id, unsigned long long *component_table);
int dsmi_cmd_get_davinchi_info(int device_id, unsigned char device_info, unsigned int *result_data);
int dsmi_cmd_get_deviceid(int device_id, unsigned short *pdevice_id);
int dsmi_cmd_get_device_die(int device_id, int soc_type, struct dsmi_soc_die_stru *pdevice_die);
int dsmi_cmd_get_device_flash_count(int device_id, unsigned int *pflash_count);
int dsmi_cmd_get_device_flash_info(int device_id, unsigned char flash_id, DSMI_DEVICE_FLASH_INFO *pflash_info);
int dsmi_cmd_get_device_health(int device_id, unsigned char *phealth);
int dsmi_cmd_get_device_power_info(int device_id, unsigned short *p_power);
int dsmi_cmd_get_device_temperature(int device_id, signed short *ptemperature);
int dsmi_cmd_get_device_voltage(int device_id, unsigned short *pvoltage);
int dsmi_cmd_get_ecc_info(int device_id, unsigned char device_info, DSMI_ECC_STATICS_RESULT *pecc_info);
int dsmi_cmd_get_enable(int device_id, unsigned char device_type, unsigned char config_item,
                        unsigned char *enable_flag);
int dsmi_cmd_get_fan_info(int device_id, DSMI_FAN_INFO *fan_info);
int dsmi_cmd_get_mac_addr(int device_id, int mac_id, char *pmac_addr);
int dsmi_cmd_get_mac_count(int device_id, unsigned char *count);
int dsmi_cmd_set_device_ip_address(int device_id, DSMI_PORT_PARA port_para, IPADDR_ST ip_addr, IPADDR_ST netmask);
int dsmi_cmd_get_device_ip_address(int device_id, DSMI_PORT_PARA port_para, IPADDR_ST *ip_addr, IPADDR_ST *netmask);
int dsmi_cmd_set_device_gtw_address(int device_id, DSMI_PORT_PARA port_para, IPADDR_ST gtw_addr);
int dsmi_cmd_get_device_gtw_address(int device_id, DSMI_PORT_PARA port_para, IPADDR_ST *gtw_addr);

int dsmi_cmd_get_pcb_id(int device_id, unsigned char *pcb_id);
int dsmi_cmd_get_sub_deviceid(int device_id, unsigned short *pdeviceid);
int dsmi_cmd_get_sub_venderid(int device_id, unsigned short *pvenderid);
int dsmi_cmd_get_system_time(int device_id, unsigned int *ntime_stamp);
int dsmi_cmd_get_venderid(int device_id, unsigned short *pvenderid);
int dsmi_cmd_set_mac_addr(int device_id, DSMI_MAC_PARA mac_para, const char *pmac_addr);
int dsmi_cmd_upgrade_ctl_cmd(int device_id, unsigned char ctl_cmd);
int dsmi_cmd_update_send_file_name(int device_id, unsigned char component_type, const char *file_name,
                                   unsigned int data_len);
int dsmi_cmd_upgrade_get_state(int device_id, unsigned char com_type, unsigned char *schedule,
                               unsigned char *upgrade_status);
int dsmi_cmd_upgrade_get_version(int device_id, unsigned char component_type, unsigned char *version_str,
                                 unsigned int *len);
int dsmi_cmd_get_user_config(int device_id, unsigned char config_name_len, const char *config_name,
                             unsigned int *p_buf_size, unsigned char *buf);
int dsmi_cmd_set_user_config(int device_id, unsigned char config_name_len, const char *config_name,
                             unsigned int buf_size, const unsigned char *buf);
int dsmi_cmd_clear_user_config(int device_id, unsigned char config_name_len, const char *config_name);
int dsmi_cmd_get_cntpct(int device_id, struct dsmi_cntpct_stru *cntpct);
int dsmi_cmd_get_network_health(int device_id, unsigned int *presult);
int dsmi_cmd_get_errorstring(int device_id, unsigned int errcode, unsigned char *perrinfo, int buffsize);
int dsmi_cmd_get_llc_perf_para(int device_id, DSMI_LLC_RX_RESULT *perf_info);
int dsmi_cmd_get_aicpu_info(int device_id, struct dsmi_aicpu_info_stru *pdevice_aicpu_info);
int dsmi_cmd_set_sec_revocation(int device_id, DSMI_REVOCATION_TYPE revo_type, const unsigned char *file_data,
                                unsigned int file_size);
int dsmi_cmd_set_power_state(int device_id, struct dsmi_power_state_info_stru *power_info);
int dsmi_cmd_get_hiss_status(int device_id, struct dsmi_hiss_status_stru *hiss_status_data);
int dsmi_cmd_get_lp_status(int device_id, struct dsmi_lp_status_stru *lp_status_data);
int dsmi_cmd_get_can_status(int device_id, const char *name, unsigned int name_len,
                            struct dsmi_can_status_stru *canstatus_data);
int dsmi_cmd_get_ufs_status(int device_id, struct dsmi_ufs_status_stru *ufsstatus_data);
int dsmi_cmd_get_sensorhub_status(int device_id, struct dsmi_sensorhub_status_stru *sensorhubstatus_data);
int dsmi_cmd_get_can_config(int device_id, const char *name, unsigned int name_len,
                            struct dsmi_can_config_stru *canconfig_data);

int dsmi_cmd_get_sensorhub_config(int device_id, struct dsmi_sensorhub_config_stru *sensorhubconfig_data);
int dsmi_cmd_get_gpio_status(int device_id, unsigned int gpio_num, unsigned int *status);
int dsmi_cmd_get_soc_hw_fault(int device_id, struct dsmi_emu_subsys_state_stru *emu_subsys_state_data);
int dsmi_cmd_get_safetyisland_status(int device_id, struct dsmi_safetyisland_status_stru *emu_subsys_state_data);
int dsmi_cmd_get_device_cgroup_info(int device_id, struct tag_cgroup_info *cg_info);
int dsmi_cmd_set_device_info(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                             unsigned int buf_size);
int dsmi_cmd_set_detect_info(unsigned int dev_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                             unsigned int buf_size);
drvError_t dsmi_cmd_set_device_info_ex(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd,
                                       const void *buf, unsigned int buf_size);
int dsmi_cmd_get_device_info(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                             unsigned int *size);
int dsmi_cmd_get_detect_info(unsigned int dev_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                             unsigned int *size);
int dsmi_cmd_get_device_info_critical(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                                      unsigned int *size);
drvError_t dsmi_cmd_set_device_info_critical(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd,
                                             const void *buf, unsigned int buf_size);
int dsmi_cmd_create_capability_group(int device_id, int ts_id, struct dsmi_capability_group_info *group_info);
int dsmi_cmd_delete_capability_group(int device_id, int ts_id, int group_id);
int dsmi_cmd_get_capability_group_info(int device_id, int ts_id, int group_id,
                                       struct dsmi_capability_group_info *group_info, int group_count);
int dsmi_cmd_create_capability_multi_group(int device_id, struct dsmi_capability_multi_group_info *group_info);
int dsmi_cmd_delete_capability_multi_group(int device_id, struct dsmi_capability_multi_group_info *group_info);
int dsmi_cmd_get_capability_multi_group(int device_id, struct dsmi_capability_multi_group_info *group_info);
int dsmi_cmd_get_reboot_reason(int device_id, struct dsmi_reboot_reason *reboot_reason);
int dsmi_cmd_get_last_bootstate(int device_id, BOOT_TYPE key, unsigned int *state);
int dsmi_cmd_ctrl_device_node(int device_id, struct dsmi_dtm_node_s dtm_node, DSMI_DTM_OPCODE opcode, IN_OUT_BUF buf);
int dsmi_cmd_get_all_device_node(int device_id, DEV_DTM_CAP capability, struct dsmi_dtm_node_s node_info[],
                                 unsigned int *size);
int dsmi_cmd_get_bist_info(int device_id, DSMI_BIST_CMD cmd, void *buf, unsigned int *size);
int dsmi_cmd_set_bist_info(int device_id, DSMI_BIST_CMD cmd, const void *buf, unsigned int buf_size);
int dsmi_cmd_fault_inject(DSMI_FAULT_INJECT_INFO info);
int dsmi_cmd_set_flash_content(int device_id, DSMI_FLASH_CONTENT *content_info);
int dsmi_cmd_get_flash_content(int device_id, DSMI_FLASH_CONTENT *content_info);
int dsmi_cmd_get_device_state(int device_id, void *in_buf, unsigned long in_size, unsigned long *out_size);
#if defined CFG_FEATURE_ECC_HBM_INFO || defined CFG_FEATURE_ECC_DDR_INFO
int dsmi_cmd_get_total_ecc_isolated_pages_info(int device_id, unsigned char module_type,
                                               struct dsmi_ecc_pages_stru *pdevice_ecc_pages_statistics);
int dsmi_cmd_clear_ecc_isolated_info(int device_id);
#endif
int dsmi_cmd_load_patch(int device_id, const char *file_name, unsigned int data_len);
int dsmi_cmd_unload_patch(int device_id, unsigned char ctl_cmd);
int dsmi_cmd_load_mami_patch(int device_id, unsigned char mami_patch_type, const char *file_name,
                             unsigned int data_len);
#endif /* __DSMI_DMP_COMMAND_H__ */
