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

#ifndef FMS_FPDC_H
#define FMS_FPDC_H
#include "ka_driver_pub.h"

#include "dms_device_node_type.h"

#define CPER_SEC_HISI_OEM_2 GUID_INIT(0x45534EA6, 0xCE23, 0x4115, 0x85, 0x35, 0xE0, 0x7A, 0xB3, 0xAE, 0xF9, 0x1D)
#define CPER_SEC_HISI_PCIE_LOCAL GUID_INIT(0xb2889fc9, 0xe7d7, 0x4f9d, 0xa8, 0x67, 0xaf, 0x42, 0xe9, 0x8b, 0xe7, 0x72)
#define CPER_SEC_HISI_COMMON GUID_INIT(0xC8B328A8, 0x9917, 0x4AF6, 0x9A, 0x13, 0x2E, 0x08, 0xAB, 0x2E, 0x75, 0x86)
#define CPER_SEC_PCIE_AER_ERROR GUID_INIT(0xD995E954, 0xBBC1, 0x430F, 0xAD, 0x91, 0xB4, 0x4D, 0xCB, 0x3C, 0x6F, 0x35)
#define CPER_SEC_UB_AER_ERROR GUID_INIT(0x74D255B0, 0x73E8, 0x488A, 0xB8, 0x67, 0x90, 0x65, 0x4A, 0x35, 0x86, 0x5E)
#define CPER_SEC_PCIE_LOCAL_RESV_LEN 2

/* OEM RAS */
struct sec_oem_error {
    unsigned int valid_fields;
    unsigned char version;
    unsigned char soc_id;
    unsigned char socket_id;
    unsigned char nimbus_id;
    unsigned char module_id;
    unsigned char submodule_id;
    unsigned char error_severity;
    unsigned char reserve;
    unsigned int err_fr_l;
    unsigned int err_fr_h;
    unsigned int err_ctrl_l;
    unsigned int err_ctrl_h;
    unsigned int err_status_l;
    unsigned int err_status_h;
    unsigned int err_addr_l;
    unsigned int err_addr_h;
    unsigned int err_misc0_l;
    unsigned int err_misc0_h;
    unsigned int err_misc1_l;
    unsigned int err_misc1_h;
};

struct cper_sec_pcie_local {
    unsigned long long valid_fields;
    unsigned char version;
    unsigned char soc_id;
    unsigned char socket_id;
    unsigned char nimbus_id;
    unsigned char submodule_id;
    unsigned char core_id;
    unsigned char port_id;
    unsigned char err_severity;
    unsigned short err_type;
    unsigned char reserve[CPER_SEC_PCIE_LOCAL_RESV_LEN];
    unsigned int err_misc_0;
    unsigned int err_misc_1;
    unsigned int err_misc_2;
    unsigned int err_misc_3;
    unsigned int err_misc_4;
    unsigned int err_misc_5;
    unsigned int err_misc_6;
    unsigned int err_misc_7;
    unsigned int err_misc_8;
    unsigned int err_misc_9;
    unsigned int err_misc_10;
    unsigned int err_misc_11;
    unsigned int err_misc_12;
    unsigned int err_misc_13;
    unsigned int err_misc_14;
    unsigned int err_misc_15;
    unsigned int err_misc_16;
    unsigned int err_misc_17;
    unsigned int err_misc_18;
    unsigned int err_misc_19;
    unsigned int err_misc_20;
    unsigned int err_misc_21;
    unsigned int err_misc_22;
    unsigned int err_misc_23;
    unsigned int err_misc_24;
    unsigned int err_misc_25;
    unsigned int err_misc_26;
    unsigned int err_misc_27;
    unsigned int err_misc_28;
    unsigned int err_misc_29;
    unsigned int err_misc_30;
    unsigned int err_misc_31;
    unsigned int err_misc_32;
};

#define UB_ERROR_NUM 32
struct cper_sec_ub_error {
    unsigned long long validation_bit;
    unsigned int eid;
    unsigned int can;
    unsigned int port;
    unsigned int oui;
    unsigned short device;
    unsigned short type;
    unsigned short class_code;
    unsigned short reserved;
    unsigned int overflow_flag;
    unsigned int err_ctrl_reg;
    unsigned long long uncor_status;
    unsigned long long uncor_mask;
    unsigned long long uncor_severity;
    unsigned long long cor_status;
    unsigned long long cor_mask;
    unsigned long long co_severity;
    unsigned int err_info_ctrl_reg;
    unsigned int spec_def_err_info[UB_ERROR_NUM];
    unsigned long long vendor_def_err_info;
};

typedef enum {
    FPDC_SRC_RAS,
    FPDC_SRC_SAFETY
} FPDC_SRC_TYPE;

enum ras_sec_type {
    RAS_SEC_GENERIC = 0x00,
    RAS_SEC_IA = 0x01,
    RAS_SEC_IPF = 0x02,
    RAS_SEC_ARM = 0x03,
    RAS_SEC_MEM = 0x04,
    RAS_SEC_PCIE = 0x05,
    RAS_SEC_OEM = 0x06,
    RAS_SEC_EMMC = 0x07,
    RAS_SEC_OTHER
};

#ifdef DEFINE_HNS_LLT
typedef struct {
    __u8 b[16];
} guid_t;
#endif

struct notify_data {
    FPDC_SRC_TYPE src_type;
    const ka_guid_t *section_type;
    DMS_DEVICE_NODE_TYPE node_type;
    unsigned int chip_id;      /* physical device id in device side */
    unsigned long long sub_id; /* optional */
    const void *origin_data;
    unsigned int data_len;
    unsigned int arm_error_idx;
};

#pragma pack(1)
struct vendor_specific_error_info {
    unsigned int err_status;
    unsigned long long err_addr;
    unsigned char oem_valid_flag;
    unsigned char oem_socket_id;
    unsigned char oem_die_id;
    unsigned char oem_sub_module;
};
#pragma pack()

/* callback function define */
typedef void (*FAULT_NOTIFY_FUNC)(const struct notify_data *pdata);

/* register callback function when node_type fault occurs */
int fpdc_register_fault_notifier(DMS_DEVICE_NODE_TYPE node_type, FAULT_NOTIFY_FUNC notify_func);
int fpdc_unregister_fault_notifier(DMS_DEVICE_NODE_TYPE node_type);

#endif