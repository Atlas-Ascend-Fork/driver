/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEVICE_COMMON_API_H
#define DEVICE_COMMON_API_H

#include <stdint.h>
#include "drv_type.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef OPEN_FAILED
#define OPEN_FAILED (-1)
#endif

#ifndef COMPILE_UT
#define STATIC static
#define CONST const
#define INLINE inline
#else
#define STATIC
#define CONST
#define INLINE
#endif
#define MAX_MAP_LEN 0X200000
#define MIN_MAP_LEN 0X1000

#define DEVDRV_HOST_PHY_MACH_FLAG 0x5a6b7c8d       /* host physical mathine flag */
#define DEVDRV_HOST_VM_MACH_FLAG 0x1a2b3c4d        /* host virt mathine flag */
#define DEVDRV_HOST_DIRECT_VM_MACH_FLAG 0x0        /* host direct virt mathine flag */
#define DEVDRV_HOST_CONTAINER_MACH_FLAG 0xa4b3c2d1 /* container mathine flag */

int realpath_open(const char *pathname, int flags, mode_t mode);
int map_virtual_addr(void **base_viraddr, unsigned long base_phyaddr, int dev_fd, int length);
int drv_get_dev_phy_mach_flag(int dev_id);
int drv_check_is_vdev(unsigned int dev_id, unsigned int vfid);
int drv_check_dev_id_validity(unsigned int dev_id);

uint64_t board_get_pcbid(void);

uint64_t board_get_bomid(void);
int mac_validity_check(uint8_t *mac_addr);
int mac_read(uint8_t dev_id, uint8_t mac_id, uint8_t mac_type, uint8_t *mac_addr);
int dm_set_mac_info(uint8_t dev_id, uint8_t mac_id, uint8_t mac_type, uint8_t *mac_addr);
int dm_get_mac_count(uint8_t dev_id, uint8_t mac_id, uint8_t mac_type, uint8_t *mac_count);
int dm_get_mac_addr(uint8_t dev_id, uint8_t mac_id, uint8_t mac_type, uint8_t *mac_addr, unsigned int len);

int gpio_export(int pin);
int gpio_unexport(int pin);
int gpio_direction(int pin, int direct);
int gpio_read(int pin);
int gpio_write(int pin, int value);
#endif /* __DEVICE_COMMON_API_H__ */
