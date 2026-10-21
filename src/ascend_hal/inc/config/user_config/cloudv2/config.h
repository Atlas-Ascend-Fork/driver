/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef __CONFIG_H
#define __CONFIG_H

#ifndef NULL
#define NULL (0L)
#endif
/*
 * Static options for user config area
 */
#define CURRENT_USER_CONFIG_VERSION 1 /* User config version supported by current software */

#define UC_FLASH_MAIN_ADDR 0x3E00000 /* Flash main area offset address of config */
#define UC_FLASH_BK_ADDR 0x3E80000   /* Flash backup area offset address of config */
#define UC_FLASH_DEV_OFFSET 0x100000 /* Address difference per device in shared-flash config scenario */

#define UC_FLASH_PARTITION_NUM 16           /* Number of user config partitions */
#define UC_FLASH_PARTITION_MAIN_NUM 8       /* Number of main partitions in user config area */
#define UC_FLASH_PARTITION_BK_NUM 8         /* Number of backup partitions in user config area */
#define UC_FLASH_PARTITION_SIZE (64 * 1024) /* User config partition size (bytes) */

#define UC_VERION_FLASH_OFFSET 0                   /* Config version offset */
#define UC_CONFIG_FLASH_OFFSET 4                   /* Config content offset */
#define UC_CHECK_FLASH_OFFSET (64 * 1024 - 4 - 32) /* Config checksum offset */
#define UC_VALID_FLASH_OFFSET (64 * 1024 - 4)      /* Config partition valid flag offset */
#define UC_VALID_FLAG_VALUE (0x8a7b6c5dU)          /* Config partition valid flag value */

#define UC_ITEM_VALID_VALUE 1       /* Config item valid flag value */
#define UC_ITEM_MAX_NUM 5           /* Number of config items */
#define UC_ITEM_DATA_MAX_LEN 0x8000 /* Max length of config content, 32KB */

/* Config item data bytes (min 2, first byte is validity) */
#define UC_P2P_SIZE 2
#define UC_DDR_ECC_SIZE 2
#define UC_LOG_LEVEL_SIZE 2
#define UC_USER_SHADOW_SIZE (2048 + 1) /* 1 byte is validity flag */
#define UC_USER_SPOD_SIZE (16 + 1)     /* 1 byte is validity flag */
#define UC_MAC_INFO_1_SIZE (16 + 1)    /* 1 byte is validity flag */
#define UC_MAC_INFO_SIZE 16            /* 16 bytes MAC info storage */
#define UC_SIGN_AUTH_ENABLE_SIZE 2     /* 2 bytes store signature verification flag */

/* Offsets of each item in config content */
#define UC_P2P_OFFSET 0                                                 /* P2P user config offset */
#define UC_DDR_ECC_OFFSET (UC_P2P_OFFSET + UC_P2P_SIZE)                 /* DDR ECC enable user config offset */
#define UC_LOG_LEVEL_OFFSET (UC_DDR_ECC_OFFSET + UC_DDR_ECC_SIZE)       /* LOG level user config offset */
#define UC_USERSHADOW_OFFSET (UC_LOG_LEVEL_OFFSET + UC_LOG_LEVEL_SIZE)  /* usershadow_0 user config offset */
#define UC_USERSPOD_OFFSET (UC_USERSHADOW_OFFSET + UC_USER_SHADOW_SIZE) /* SPOD user config offset */
#define UC_MAC_INFO_1_OFFSET (UC_USERSPOD_OFFSET + UC_USER_SPOD_SIZE)   /* MAC INFO 1 user config offset */
#define UC_MAC_INFO_OFFSET 0                                            /* MAC INFO user config offset */
#define UC_SIGN_AUTH_ENABLE_OFFSET \
    (UC_MAC_INFO_1_OFFSET + UC_MAC_INFO_1_SIZE) /* Signature verification flag user config offset */

/* Config item authority control */
#define UC_AUTHORITY_ROOT_WR (0) /* root user read/write, normal user no access */
#define UC_AUTHORITY_USER_WR (1) /* root user read/write, normal user read/write */
#define UC_AUTHORITY_USER_RO (2) /* root user read/write, normal user read-only */
#define UC_USER_PARA_ERR -1
#define UC_USER_PARA_OK 0

/* User config item info */
struct user_config_item {
    const char *name;         /* Config item name */
    int board_id;             /* Board ID this config belongs to; if board_id is -1, all board IDs use this config */
    int offset;               /* Config item data address offset */
    int len;                  /* Data byte length, min 2; first byte is config validity, 1 means valid */
    int authority_flag;       /* Config item authority, see UC_AUTHORITY_XXX */
    const char *default_data; /* Config item default value, data is string, e.g.: "0x12,0x34,0x78,0xaa" */
    int (*check_para)(unsigned int buf_size, unsigned char *buf);
};

/*
 * User config items
 */
#define DDR_ECC_CONFIG_NAME "ddr_ecc_enable"
#define SSH_CONFIG_NAME "ssh_status"
#define CPU_NUM_CONFIG_NAME "cpu_num_cfg"
#define P2P_MEM_CONFIG_NAME "p2p_mem_cfg"
#define CCPU_USR_CERT_CONFIG_NAME "ccpu_usr_cert_hash"
#define UBMEM_RETRY_FLAG_NAME "ubmem_retry_flag"

#define AUTH_CONFIG_ENABLE_NAME "sign_auth_enable"
#define AUTH_CONFIG_ENABLE 1
#define AUTH_CONFIG_DISENABLE 0

static int spod_info_check_para(unsigned int buf_size, unsigned char *buf);
static int cpu_sign_auth_check_para(unsigned int buf_size, unsigned char *buf);
/*
 * default_data field must start with "0x", every two chars represent one byte
 * e.g.: value 0,1 represented as 0x0001
 */
static const struct user_config_item user_cfg_version_1[UC_ITEM_MAX_NUM] = {
    {"usershadow", -1, UC_USERSHADOW_OFFSET, UC_USER_SHADOW_SIZE, UC_AUTHORITY_ROOT_WR, "0x00", NULL},
    {"mac_info", -1, UC_MAC_INFO_OFFSET, UC_MAC_INFO_SIZE, UC_AUTHORITY_USER_WR, "0x00", NULL},
    {"spod_info", -1, UC_USERSPOD_OFFSET, UC_USER_SPOD_SIZE, UC_AUTHORITY_USER_WR, "0x00", spod_info_check_para},
    {"mac_info_1", -1, UC_MAC_INFO_1_OFFSET, UC_MAC_INFO_1_SIZE, UC_AUTHORITY_USER_WR, "0x00", NULL},
    {"sign_auth_enable", -1, UC_SIGN_AUTH_ENABLE_OFFSET, UC_SIGN_AUTH_ENABLE_SIZE, UC_AUTHORITY_USER_WR, "0x00",
     cpu_sign_auth_check_para},
};

#define UC_ITEM_INDEX_USER_SHADOW 0
#define UC_ITEM_INDEX_MAC_INFO 1

/*
 * Dynamic options for user config area
 */
/* Dynamic config head address */
#define UC_DYNAMIC_CFG_HEAD_OFFSET 0x2000 // 8KB
#define UC_CFG_HEAD_LEN_OFFSET UC_DYNAMIC_CFG_HEAD_OFFSET
#define UC_CFG_HEAD_BYTES 4 // 4Bytes
#define UC_CFG_HEAD_ITEM_START (UC_DYNAMIC_CFG_HEAD_OFFSET + UC_CFG_HEAD_BYTES)
#define UC_CFG_NAME_LEN_MAX 32
#define UC_SHA256_DIGEST 32

typedef struct _uc_cfg_head {
    unsigned char item_name[UC_CFG_NAME_LEN_MAX];
    unsigned int blk_offset;
    unsigned int item_offset;
    unsigned int item_bytes;
    unsigned int authority_flg;
    unsigned int valid_flg;
} uc_cfg_head_t;

typedef struct _uc_blk_used_info {
    unsigned char index;
    unsigned char used;
    unsigned short used_len;
} uc_blk_used_info_t;

#define UC_BLK_INFO_OFFSET (UC_CHECK_FLASH_OFFSET - sizeof(uc_blk_used_info_t) * UC_FLASH_PARTITION_MAIN_NUM)
#define UC_BLK_INFO_BYTES (sizeof(uc_blk_used_info_t) * UC_FLASH_PARTITION_MAIN_NUM)
#define UC_ITEM_CONTENT_BLK_START 2
#define UC_BLK_INDEX_MAC_INFO 1
#define UC_ITEM_CONTENT_BLK_OFFSET 0 // first block offset
#define UC_MAC_CNT 2

typedef struct {
    unsigned short crc;
    unsigned char version;
    unsigned char reserve1;
    unsigned short server_id;
    unsigned short scale_type;
    unsigned int super_pod_id;
    unsigned int reserved[1];
} uc_spod_cfg_t;

#define CRC_POLYNOMIAL 0x1021
#define CRC_16_INIT_VAL 0xFFFF
#define BIT15_MASK 0x8000
#define BITS_PER_BYTE 8
static unsigned short calc_crc16(unsigned char *data, unsigned int len)
{
    unsigned int val = CRC_16_INIT_VAL;
    unsigned char tmp_data;
    unsigned int i;

    while (len--) {
        tmp_data = *(data++);
        val ^= ((unsigned int)tmp_data << BITS_PER_BYTE);
        for (i = 0; i < BITS_PER_BYTE; i++) {
            val = (val & BIT15_MASK) ? ((val << 1) ^ CRC_POLYNOMIAL) : (val << 1);
        }
    }
    return (unsigned short)val;
}

static int spod_info_check_para(unsigned int buf_size, unsigned char *buf)
{
    uc_spod_cfg_t default_data = {0};
    uc_spod_cfg_t param_copy;
    uc_spod_cfg_t *p_param_stru = NULL;
    int ret;

    if (buf == NULL) {
        return UC_USER_PARA_ERR;
    }
    if (buf_size + 1 != UC_USER_SPOD_SIZE) {
        return UC_USER_PARA_ERR;
    }
    if (sizeof(uc_spod_cfg_t) + 1 != UC_USER_SPOD_SIZE) {
        return UC_USER_PARA_ERR;
    }
    ret = memcpy_s(&param_copy, sizeof(param_copy), buf, buf_size);
    if (ret != 0) {
        return UC_USER_PARA_ERR;
    }
    if (param_copy.server_id > 0x3ff) { /* server id use 10 bit */
        return UC_USER_PARA_ERR;
    }
    param_copy.server_id = 0;
    param_copy.scale_type = 0;
    param_copy.super_pod_id = 0;
    if (memcmp(&param_copy, &default_data, sizeof(uc_spod_cfg_t)) != 0) {
        return UC_USER_PARA_ERR;
    }
    p_param_stru = (uc_spod_cfg_t *)buf;
    p_param_stru->crc = calc_crc16(&p_param_stru->version, sizeof(uc_spod_cfg_t) - sizeof(p_param_stru->crc));
    return UC_USER_PARA_OK;
}

static int cpu_sign_auth_check_para(unsigned int buf_size, unsigned char *buf)
{
    if (buf == NULL) {
        return UC_USER_PARA_ERR;
    }

    if (buf_size >= UC_SIGN_AUTH_ENABLE_SIZE) {
        return UC_USER_PARA_ERR;
    }

    if ((buf[0] != AUTH_CONFIG_ENABLE) && (buf[0] != AUTH_CONFIG_DISENABLE)) {
        return UC_USER_PARA_ERR;
    }

    return UC_USER_PARA_OK;
}

#endif
