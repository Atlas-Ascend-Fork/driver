/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DEV_USER_CFG_PUBLIC_H
#define DEV_USER_CFG_PUBLIC_H

#include "config.h"

#define NETWORK_ITEM_NAME_NUM 7
#define CERT_ITEM_NAME_S0 "hccp_certs_eqpt_cb"
#define CERT_ITEM_NAME_S1 "hccp_pri_data_cb"
#define CERT_ITEM_NAME_S2 "hccp_certs_revoc_cb"
#define CERT_ITEM_NAME_S3 "hccp_certs_mng_cb"

#define DIGITAL_ITEM_NAME_S0 "hccn_envelope_mng"
#define DIGITAL_ITEM_NAME_S1 "hccn_envelope_pri_standby"
#define DIGITAL_ITEM_NAME_S2 "hccn_envelope_pri_active"

#define VERIFY_ITEM_NUM 1

#ifdef CFG_FEATURE_UC_CHIP_MAX_ONE
#define DEVDRV_UC_CHIP_MAX 1
#else
#ifdef CFG_SOC_PLATFORM_MINIV2
#define DEVDRV_UC_CHIP_MAX 2
#else
#define DEVDRV_UC_CHIP_MAX 4
#endif
#endif

/* p2p user config */
#define SZ_1M 0x100000
#define SIZE_IN_G 1024
#define SZ_2G 0x80000000

#define P2P_MEM_CONFIG_NEED_RUN 0x50325001
#define P2P_MEM_CONFIG_RUN_SUCCESS 0x50325002
#define P2P_MEM_CONFIG_RUN_FAIL 0x50325004
#define P2P_MEM_CONFIG_SYNC_SUCCESS 0x503250FF

#ifndef UC_CFG_NAME_LEN_MAX
#define UC_CFG_NAME_LEN_MAX 32
#endif

struct user_cfg_ioctl_para {
    unsigned int dev_id;
    int cmd;
    int cfg_index;
    char name[UC_CFG_NAME_LEN_MAX];
    unsigned int buf_size;
    void *buf;
};

typedef enum {
    CHIP_INFO_SOLO = 0,
    CHIP_INFO_DUAL,
} BOOT_CFG_CHIP_INFO;

/* cpu number configuration struct in flash, total 16 Bytes, part for reserved */
typedef struct uc_cpu_cfg_stru {
    unsigned char ctrl_cpu_num;
    unsigned char data_cpu_num;
    unsigned char ai_cpu_num;
    unsigned char com_cpu_num;
    unsigned int reserved_2[3]; /* 3 word 12 bytes, totally 16 bytes */
} uc_cpu_cfg_t;

/* Use 'C' as magic number */
#define USER_CFG_CTL_IOC_MAGIC 'C'
#define USER_CFG_FLASH_OP _IOR(USER_CFG_CTL_IOC_MAGIC, 1, int)
#define USER_CFG_GET_BOOT_CFG _IOR(USER_CFG_CTL_IOC_MAGIC, 2, int)
#define USER_CFG_GET_DTS_CFG _IOR(USER_CFG_CTL_IOC_MAGIC, 4, int)
#define USER_CFG_GET_SYS_INFO _IOR(USER_CFG_CTL_IOC_MAGIC, 6, int)

#endif /* _DEV_USER_CFG_PUBLIC_H_ */
