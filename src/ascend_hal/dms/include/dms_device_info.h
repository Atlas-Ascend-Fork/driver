/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __ABL_DMS_DEVICE_INFO_H
#define __ABL_DMS_DEVICE_INFO_H
#include "dsmi_common_interface.h"

drvError_t DmsSetDeviceInfo(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                            unsigned int size);
drvError_t DmsSetDeviceInfoEx(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                              unsigned int size);
drvError_t DmsSetTsInfo(unsigned int dev_id, unsigned int sub_cmd, const void *buf, unsigned int size);
drvError_t DmsGetTsInfoEx(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                          unsigned int *size);

drvError_t DmsGetDeviceInfo(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                            unsigned int *size);
drvError_t DmsGetDeviceInfoEx(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                              unsigned int *size);
drvError_t DmsGetDeviceInfoExV2(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                                unsigned int *size);
drvError_t DmsGetDevBootStatus(int phy_id, unsigned int *boot_status);
drvError_t DmsGetChipType(unsigned int phy_id, unsigned int *chip_type);
drvError_t DmsGetMasterDevInTheSameOs(unsigned int phy_id, unsigned int *master_dev_id);
drvError_t DmsGetDevProbeNum(unsigned int *num);
drvError_t DmsGetDevProbeList(unsigned int *devices, unsigned int len);
drvError_t DmsGetP2PStatus(unsigned int dev_id, unsigned int peer_dev_id, unsigned int *status);
drvError_t DmsGetP2PCapbility(unsigned int dev_id, unsigned long long *capbility);
drvError_t DmsEnableP2P(unsigned int dev_id, unsigned int peer_dev_id, unsigned int type);
drvError_t DmsDisableP2P(unsigned int dev_id, unsigned int peer_dev_id, unsigned int type);
drvError_t DmsCanAccessPeer(unsigned int dev_id, unsigned int peer_dev_id, int *can_access_peer);
drvError_t DmsHalGetDeviceInfoEx(unsigned int dev_id, int module_type, int info_type, void *buf, unsigned int *size);
drvError_t DmsHalSetDeviceInfoEx(unsigned int dev_id, int module_type, int info_type, const void *buf,
                                 unsigned int size);
drvError_t dms_set_detect_info(unsigned int dev_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                               unsigned int size);
drvError_t dms_get_detect_info(unsigned int dev_id, DSMI_DETECT_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                               unsigned int *size);
drvError_t DmsGetAiCoreDieNum(unsigned int dev_id, long long *value);
drvError_t dms_get_hcom_cpu_num(unsigned int dev_id, long long *value);
drvError_t DmsGetCpuWorkMode(unsigned int dev_id, long long *value);
drvError_t DmsGetTrsMode(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                         unsigned int *size);
drvError_t dms_get_process_resource(unsigned int dev_id, struct dsmi_resource_para *para, void *buf,
                                    unsigned int buf_len);
drvError_t dms_set_sign_flag_ioctl(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, const void *buf,
                                   unsigned int size);
drvError_t dms_get_sign_flag_ioctl(unsigned int dev_id, DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd, void *buf,
                                   unsigned int *size);
drvError_t dms_get_phy_dev_info(unsigned int phy_id, int info_type, unsigned int *val);
#endif
