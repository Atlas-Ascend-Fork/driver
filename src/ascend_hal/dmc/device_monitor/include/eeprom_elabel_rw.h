/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef EEPROM_ELABEL_RW_H
#define EEPROM_ELABEL_RW_H
#pragma pack(1)

#include <sys/ioctl.h>

#define ELABEL_HEADER_SIZE 4
#define EEPROM_IOC_MAGIC 'E'
#define SET_EEPROM_WRITE_CMD _IOR(EEPROM_IOC_MAGIC, 42, int)
#define SET_EEPROM_READ_CMD _IOR(EEPROM_IOC_MAGIC, 43, int)

#define EEPROM_NAME_NUM 2
#define EEPROM_MINI_NAME "/dev/eeprom_m24256-1"
#define EEPROM_DEVELOP_NAME "/dev/eeprom_m24256"
#define EEPROM_MAX_NUM 2
#define EEPROM_NAME_LEN 50
#define ELABLE_MAX_LEN 65540
#define ELABLE_MIN_LEN 4

#define EEPROM_PATH "/var/eeprom.cfg"

#define EEPROM_ELABEL_OFFSET 0x200

#define ELABELNULL 135 // Elabel data is null

#define EEPROM_CFG_MAX_LINE 50

/* elabel table */
typedef struct tag_index_info {
    unsigned short area_num;
    unsigned short field_num;
    unsigned long offset;
    unsigned long size;
} INDEX_INFO_T, *PINDEX_INFO_T;

typedef struct elabel_data {
    unsigned short crc;
    unsigned short len;
    char data[0];
} ELABEL_DATA;

typedef struct eeprom_info {
    char *buf;                 // data_buf
    unsigned int count;        // size
    unsigned int page_address; // offset
} EEPROM_INFO;

#define ELABEL_AREA_CHASSISINFO 1
#define ELABEL_AREA_BOARDINFO 2
#define ELABEL_AREA_PRODUCTINFO 3
#define ELABEL_AREA_EXTENDELABEL 5
#define ELABEL_AREA_SYSTEMINFO 6

/* chassis  field */
#define ELABEL_CHASSIS_TYPE 0
#define ELABEL_CHASSIS_PART_NUMBER 1
#define ELABEL_CHASSIS_SERIAL_NUMBER 2

/* board  field */
#define ELABEL_BOARD_MFGDATE 0
#define ELABEL_BOARD_MANUFACTURER 1
#define ELABEL_BOARD_PRODUCTNAME 2
#define ELABEL_BOARD_SERIALNUMBER 3
#define ELABEL_BOARD_PARTNUMBER 4
#define ELABEL_BOARD_FRUFILEID 5

/* product  field */
#define ELABEL_PRODUCT_MANUFACTURER 0
#define ELABEL_PRODUCT_NAME 1
#define ELABEL_PRODUCT_PARTNUMBER 2
#define ELABEL_PRODUCT_VERSION 3
#define ELABEL_PRODUCT_SERIALNUMBER 4
#define ELABEL_PRODUCT_ASSETTAG 5
#define ELABEL_PRODUCT_FRUFILEID 6

/* extend elabel */
#define ELABEL_EXTENDELABEL_FIELD 0

/* system  field */
#define ELABEL_SYSTEM_MANUFACTURE 0
#define ELABEL_SYSTEM_NAME 1
#define ELABEL_SYSTEM_VERSION 2
#define ELABEL_SYSTEM_SERIAL_NUMBER 3

#ifndef ERROR
#define ERROR (-1)
#endif

#ifndef OK
#define OK 0
#endif

int elabel_open(const char *pathname, int flags, mode_t mode);
int elabel_ioctl(int fd, unsigned long cmd, EEPROM_INFO *arg);
int elabel_close(int fd);
void *elabel_malloc(size_t size); // lint !e101 !e132
int elabel_strcmp(const char *s1, const char *s2);
int write_elabel_data(unsigned char test_item, const char *elabel_data, unsigned int len, char *eeprom_name);
int clear_elabel_data(unsigned char test_item, const char *eeprom_name, unsigned int len);
int read_elabel_data(unsigned char test_item, char *elabel_data, unsigned short *elabel_len, const char *eeprom_name);
PINDEX_INFO_T get_elabel_by_item(unsigned char test_item);

int get_eeprom_cfg(char eeprom_index, char *eeprom_name, unsigned int len);
int get_eeprom_name(char eeprom_index, char *eeprom_name, unsigned int len);

#pragma pack()
#endif
