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
#define UC_FLASH_BK_ADDR 0x3F00000   /* Flash backup area offset address of config */
#define UC_FLASH_DEV_OFFSET 0        /* Address difference per device in shared-flash config scenario */

#define UC_FLASH_PARTITION_NUM 32           /* Number of user config partitions */
#define UC_FLASH_PARTITION_MAIN_NUM 16      /* Number of main partitions in user config area */
#define UC_FLASH_PARTITION_BK_NUM 16        /* Number of backup partitions in user config area */
#define UC_FLASH_PARTITION_SIZE (64 * 1024) /* User config partition size (bytes) */

#define UC_VERION_FLASH_OFFSET 0                   /* Config version offset */
#define UC_CONFIG_FLASH_OFFSET 4                   /* Config content offset */
#define UC_CHECK_FLASH_OFFSET (64 * 1024 - 4 - 32) /* Config checksum offset */
#define UC_VALID_FLASH_OFFSET (64 * 1024 - 4)      /* Config partition valid flag offset */
#define UC_VALID_FLAG_VALUE 0x8a7b6c5d             /* Config partition valid flag value */

#define UC_ITEM_VALID_VALUE 1       /* Config item valid flag value */
#define UC_ITEM_MAX_NUM 5           /* Number of config items */
#define UC_ITEM_DATA_MAX_LEN 0x8000 /* Max length of config content, 32KB */

/* Config item data bytes (min 2, first byte is validity) */
#define UC_CPU_CFG_SIZE (16 + 1)      /* CPU count config */
#define UC_MAC_INFO_1_SIZE (16 + 1)   /* 1 byte is validity flag */
#define UC_MAC_INFO_SIZE 16           /* 16 bytes MAC info storage */
#define UC_CPU_WORK_MODE_SIZE (1 + 1) /* CPU work mode config */
#define UC_SIGN_AUTH_ENABLE_SIZE 2    /* 2 bytes store signature verification flag */

/* Offsets of each item in config content. NOTE: Once determined, offsets cannot be modified;
   when deleting a config item, the offset value must not be removed here */
#define UC_CPU_CFG_OFFSET 0                                                 /* CPU count config offset */
#define UC_MAC_INFO_1_OFFSET (UC_CPU_CFG_OFFSET + UC_CPU_CFG_SIZE)          /* MAC INFO 1 user config offset */
#define UC_MAC_INFO_OFFSET 0                                                /* MAC INFO user config offset */
#define UC_CPU_WORK_MODE_OFFSET (UC_MAC_INFO_1_OFFSET + UC_MAC_INFO_1_SIZE) /* CPU work mode config offset */
#define UC_SIGN_AUTH_ENABLE_OFFSET \
    (UC_CPU_WORK_MODE_OFFSET + UC_CPU_WORK_MODE_SIZE) /* Signature verification flag user config offset */

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
#define CPU_NUM_CONFIG_NAME "cpu_num_cfg"
#define UBMEM_RETRY_FLAG_NAME "ubmem_retry_flag"

#define AUTH_CONFIG_ENABLE_NAME "sign_auth_enable"
#define AUTH_CONFIG_ENABLE 1
#define AUTH_CONFIG_DISENABLE 0

static int cpu_work_mode_check_para(unsigned int buf_size, unsigned char *buf);
static int cpu_sign_auth_check_para(unsigned int buf_size, unsigned char *buf);

/*
 * default_data field must start with "0x", every two chars represent one byte
 * e.g.: value 0,1 represented as 0x0001
 */
static const struct user_config_item user_cfg_version_1[UC_ITEM_MAX_NUM] = {
    {CPU_NUM_CONFIG_NAME, -1, UC_CPU_CFG_OFFSET, UC_CPU_CFG_SIZE, UC_AUTHORITY_USER_WR, "0x01000300", NULL},
    {"mac_info", -1, UC_MAC_INFO_OFFSET, UC_MAC_INFO_SIZE, UC_AUTHORITY_USER_WR, "0x00", NULL},
    {"mac_info_1", -1, UC_MAC_INFO_1_OFFSET, UC_MAC_INFO_1_SIZE, UC_AUTHORITY_USER_WR, "0x00", NULL},
    {"cpu_work_mode", -1, UC_CPU_WORK_MODE_OFFSET, UC_CPU_WORK_MODE_SIZE, UC_AUTHORITY_USER_WR, "0x00",
     cpu_work_mode_check_para},
    {"sign_auth_enable", -1, UC_SIGN_AUTH_ENABLE_OFFSET, UC_SIGN_AUTH_ENABLE_SIZE, UC_AUTHORITY_USER_WR, "0x00",
     cpu_sign_auth_check_para},
};

#define UC_ITEM_INDEX_MAC_INFO 1 /* INDEX of MAC_INFO in static table user_cfg_version_1 */

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

#define UC_CPU_WORK_MODE_MAX 2
static int cpu_work_mode_check_para(unsigned int buf_size, unsigned char *buf)
{
    if (buf == NULL) {
        return UC_USER_PARA_ERR;
    }

    if (buf_size + 1 != UC_CPU_WORK_MODE_SIZE) {
        return UC_USER_PARA_ERR;
    }

    if (*buf >= UC_CPU_WORK_MODE_MAX) {
        return UC_USER_PARA_ERR;
    }
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
