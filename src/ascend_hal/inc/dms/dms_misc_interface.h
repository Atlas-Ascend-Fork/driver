/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DMS_MISC_INTERFACE_H
#define DMS_MISC_INTERFACE_H

#include "ascend_hal.h"
#include "dms_soc_interface.h"
#include "dms_memory_interface.h"
#include "dms_lpm_interface.h"
#include "dsmi_common_interface.h"

#define DMS_FAULT_INJECT_RESERVED_LEN 32
typedef struct dms_fault_inject_stru {
    int node_type;
    int node_id;
    int sub_node_type;
    int sub_node_id;
    int fault_type;
    int sub_fault_type;
    int times;                                             /* inject times */
    unsigned char reserved[DMS_FAULT_INJECT_RESERVED_LEN]; /* reserved data must be set to 0 */
} DMS_FAULT_INJECT_STRU;

struct dms_osc_freq {
    unsigned int dev_id;
    unsigned int sub_cmd;
    unsigned long long value;
};

struct dms_dev_replace_stru {
    struct dsmi_device_attr src_dev_attr;
    struct dsmi_device_attr dst_dev_attr;
    unsigned int timeout;
    unsigned long long flag;
};

struct dms_eid_info_stru {
    unsigned int dev_id;
    struct dsmi_eid_pair_info eid;
};

#define DMS_MAX_EID_PAIR_NUM 8
struct dms_ub_dev_info {
    unsigned int eid_index[DMS_MAX_EID_PAIR_NUM];
    void *urma_dev[DMS_MAX_EID_PAIR_NUM]; // urma_device_t
};

int DmsGetTsInfo(unsigned int dev_id, unsigned int vfid, unsigned int core_id, void *result, unsigned int result_size);
int dms_get_average_util_from_ts(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, unsigned int *value);
int dms_get_single_util_from_ts(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf,
                                unsigned int *size);
int dms_get_ts_info(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *out_buf, unsigned int *size);
int dms_set_ts_info(unsigned int dev_id, unsigned int sub_cmd, void *buf, unsigned int size);

drvError_t dms_get_device_topology(unsigned int dev_id1, unsigned int dev_id2, int *topology_type);
drvError_t dms_get_phy_devices_topology(unsigned int dev_id1, unsigned int dev_id2, int *topology_type);
int dms_get_dvpp_info(unsigned int dev_id, unsigned int vfid, unsigned int subcmd, void *buf, unsigned int *size);
drvError_t dms_set_sriov_switch(unsigned int dev_id, unsigned int sub_cmd, const void *buf, unsigned int buf_size);
int DmsGetHccsInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size);
int DmsGetSioInfo(unsigned int dev_id, unsigned int vfid, unsigned int sub_cmd, void *buf, unsigned int *size);
int dms_get_last_boot_state(unsigned int dev_id, BOOT_TYPE boot_type, unsigned int *state);
int dms_fault_inject(unsigned int dev_id, unsigned int vfid, unsigned int main_cmd, unsigned int sub_cmd,
                     DMS_FAULT_INJECT_STRU *para);
drvError_t DmsSetHostAicpuInfo(unsigned int dev_id, unsigned int sub_cmd, const void *buf, unsigned int size);
drvError_t DmsGetHostAicpuInfo(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                               unsigned int *size);
#define DMS_URMA_NAME_MAX_LEN 64
int dms_get_urma_name_by_devid(unsigned int dev_id, char *name, unsigned int len);

drvError_t __attribute__((weak)) dms_get_spod_info(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd,
                                                   void *buf, unsigned int *size)
{
    (void)dev_id;
    (void)main_cmd;
    (void)sub_cmd;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}
drvError_t __attribute__((weak)) dms_get_spod_item(uint32_t devId, int32_t infoType, int64_t *value)
{
    (void)devId;
    (void)infoType;
    (void)value;
    return DRV_ERROR_NOT_SUPPORT;
}
drvError_t __attribute__((weak)) dms_parse_sdid(uint32_t sdid, struct halSDIDParseInfo *sdid_parse)
{
    (void)sdid;
    (void)sdid_parse;
    return DRV_ERROR_NOT_SUPPORT;
}
drvError_t __attribute__((weak)) dms_get_spod_ping_info(unsigned int dev_id, unsigned int main_cmd,
                                                        unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)dev_id;
    (void)main_cmd;
    (void)sub_cmd;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}

int dms_get_aicpu_utilization(unsigned int dev_id, unsigned int *utilization);

drvError_t DmsDeviceInitStatus(unsigned int dev_id, unsigned int *status);
drvError_t DmsTsHeartbeatStatus(unsigned int dev_id, unsigned int vf_id, unsigned int ts_id, unsigned int *status);

drvError_t dms_set_cc_info(unsigned int device_id, void *buf, unsigned int buf_size);
drvError_t dms_get_cc_info(unsigned int device_id, void *buf, unsigned int *buf_size);
drvError_t dms_get_connect_type(int64_t *type);

drvError_t __attribute__((weak)) dms_get_spod_node_status(unsigned int dev_id, unsigned int main_cmd,
                                                          unsigned int sub_cmd, void *buf, unsigned int *size)
{
    (void)dev_id;
    (void)main_cmd;
    (void)sub_cmd;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t __attribute__((weak)) dms_set_spod_node_status(unsigned int dev_id, unsigned int sub_cmd, const void *buf,
                                                          unsigned int size)
{
    (void)dev_id;
    (void)sub_cmd;
    (void)buf;
    (void)size;
    return DRV_ERROR_NOT_SUPPORT;
}

drvError_t dms_get_ub_dev_info(unsigned int dev_id, struct dms_ub_dev_info *eid_info, int *num);

enum devdrv_token_val_type {
    SHARED_TOKEN_VAL = 0, // Shared within the process scope
    UNIQUE_TOKEN_VAL,
    TOKEN_VAL_TYPE_MAX
};
drvError_t dms_get_token_val(unsigned int dev_id, unsigned int type, unsigned int *val);

#define D2D_EID_LENGTH 16
struct dms_ub_d2d_eid {
    unsigned char d2d_eid[D2D_EID_LENGTH];
};
drvError_t dms_get_ub_d2d_eid(unsigned int dev_id, struct dms_ub_d2d_eid *d2d_eid);
drvError_t dms_get_ub_d2d_eid_v2(unsigned int devid, unsigned int peer_udevid, struct dms_ub_d2d_eid *d2d_eid,
                                 uint32_t *eid_index);

enum devdrv_d2d_eid_type {
    D2D_EID_IN_BOARD = 0, // Shared within the process scope
    D2D_EID_CROSS_BOARD,
    D2D_EID_TYPE_MAX
};
drvError_t dms_get_ub_d2d_eid_by_type(unsigned int devid, unsigned int d2d_eid_type, struct dms_ub_d2d_eid *d2d_eid,
                                      uint32_t *eid_index);

#define BUS_INST_EID_LENGTH 16
struct dms_ub_bus_inst_eid {
    unsigned char bus_inst_eid[BUS_INST_EID_LENGTH];
};
drvError_t dms_get_ub_bus_inst_eid(unsigned int dev_id, struct dms_ub_bus_inst_eid *bus_inst_eid);

struct dms_ubdev_id_info {
    unsigned short device_id;
    unsigned short vendor_id;
    unsigned short module_vendor_id;
    unsigned short module_id;
    unsigned char reserved[32];
};
drvError_t dms_get_ub_dev_id_info(unsigned int dev_id, struct dms_ubdev_id_info *id_info);

#endif /* __DMS_MISC_INTERFACE_H */
