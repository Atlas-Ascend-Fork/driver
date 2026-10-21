/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_UPGRADE_API_H
#define DEV_UPGRADE_API_H

#include "device_monitor_type.h"

typedef enum {
    UPGRADE_CTRL_CMD_PREPARE = 1,
    UPGRADE_CTRL_CMD_GET_LIST,
    UPGRADE_CTRL_CMD_TRANS_FILE,
    UPGRADE_CTRL_CMD_START,
    UPGRADE_CTRL_CMD_STOP,
    UPGRADE_CTRL_CMD_ACTIVE,
    UPGRADE_CTRL_CMD_SWITCH,
    UPGRADE_CTRL_CMD_ROLLBACK,
    UPGRADE_CTRL_CMD_START_AND_RESET,
    UPGRADE_CTRL_CMD_SYNC,
    UPGRADE_CTRL_CMD_SYNC_RECOVERY,
    UPGRADE_CTRL_CMD_SYNC_FIRMWARE,
    UPGRADE_CTRL_CMD_UPDATE_STATE,
    UPGRADE_CTRL_CMD_UPGRADE_PATCH,
    UPGRADE_CTRL_CMD_UNLOAD_PATCH,
    UPGRADE_CTRL_CMD_UPGRADE_MAMI_PATCH
} UPGRADE_CMD_TYPE;

/* direct response on the device side */
void dev_upgrade_api_process(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_upgrade_hot_patch_op(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_upgrade_api_get_state(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
int dev_upgrade_api_get_version(int dev_id, unsigned int component_type, unsigned char *version_str,
                                unsigned int max_len);
int dev_upgrade_api_init(void);
int dev_upgrade_api_uninit(void);
int dev_upgrade_api_soc_revocation(int dev_id, unsigned int type, const unsigned char *file_data,
                                   unsigned int file_size);

int dev_upgrade_soc_revocation(int dev_id, unsigned int type, const unsigned char *file_data, unsigned int file_size);
int dev_upgrade_api_get_dev_info(unsigned int dev_id, unsigned int vfid, unsigned int cmd, void *buf,
                                 unsigned int *size);
int dev_upgrade_api_set_dev_info(unsigned int dev_id, unsigned int cmd, void *buf, unsigned int size);
int dev_upgrade_get_dev_info(int dev_id, unsigned int cmd, void *buf, unsigned int *size);
int dev_upgrade_set_dev_info(int dev_id, unsigned int cmd, void *buf, unsigned int size);
int dev_upgrade_api_get_recovery_info(unsigned int dev_id, unsigned int vfid, unsigned int cmd, void *buf,
                                      unsigned int *size);
int dev_upgrade_api_set_recovery_info(unsigned int dev_id, unsigned int cmd, void *buf, unsigned int size);
int dev_upgrade_load_patch(int dev_id, const char *file_name);
int dev_upgrade_unload_patch(int dev_id);
void dev_upgrade_cmd_stop(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_upgrade_cmd_start_and_reset(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_upgrade_cmd_get_list(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_upgrade_cmd_prepare(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
int dev_upgrade_check_authority(int dev_id, unsigned char session_prop);
#ifndef CFG_SOC_PLATFORM_MINI
void dev_upgrade_sync_cmd_start(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
#endif
void dev_upgrade_cmd_start(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
void dev_upgrade_cmd_trans_file(SYSTEM_CB_T *cb, DM_INTF_S *intf, DM_RECV_ST *msg);
int dev_upgrade_load_mami_patch(int dev_id, unsigned char mami_pack_type, const char *file_name);
#endif /* _DEV_UPGRADE_API_H_ */
