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

#ifndef PBL_SOC_RES_ATTR_H
#define PBL_SOC_RES_ATTR_H

#include "ka_type.h"

/* board hardware info */
#define BOARD_HW_INFO "board_hw_info"
#define HW_INFO_RESERVED_LEN 2
typedef struct soc_res_board_hw_info {
    u8 chip_id;
    u8 multi_chip; /* multi-chip or single-chip */
    u8 multi_die;  /* multi-die or single-die */
    u8 rsv;        /* reserved for u16 alignment */
    u16 mainboard_id;
    u16 addr_mode; /* host and device addressing mode. 0: independent, 1: unified */
    u16 board_id;

    u8 version;            /* data version */
    u8 inter_connect_type; /* connect mode. 0: pcie, 1: hccs */
    u16 hccs_hpcs_bitmap;  /* hccs lane info */

    u16 server_id;    /* super pod server ID */
    u16 scale_type;   /* super pod scale type */
    u32 super_pod_id; /* super pod ID */

    u16 chassis_id;    /* supor global frame number */
    u8 super_pod_type; /* 1D/2D */
    u8 slot_id;
    u8 super_pod_intercon_type;
    u8 reserved2[HW_INFO_RESERVED_LEN];
} soc_res_board_hw_info_t;

#define BOARD_HW_INFO_EX "board_hw_info_ex"
#define HW_INFO_EX_RESERVED_LEN 252
typedef struct soc_res_board_hw_info_ex {
    u16 scale_up_group_id;
    u16 scale_out_group_id;
    u8 reserved[HW_INFO_EX_RESERVED_LEN];
} soc_res_board_hw_info_ex_t;

/* soc version */
#define SOC_VERSION "soc_ver"
#define SOC_VERSION_LEN 32

#define EID_SIZE 16
#define MAX_EID_NUM_PER_VNPU 8

struct fe_attr_list {
    u8 eid[EID_SIZE];
    u32 func_id;
    u32 die_id;
};

// communication resource
struct vnpu_commu_res {
    u32 fe_num;
    struct fe_attr_list fe_attr[MAX_EID_NUM_PER_VNPU];
};

struct ascend_urma_dev_info {
    u32 func_id;
    u32 die_id;
};
#endif /* PBL_SOC_RES_ATTR_H */
