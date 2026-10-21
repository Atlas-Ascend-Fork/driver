/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ETHERNET_H
#define ETHERNET_H

#include "dms/dms_devdrv_info_comm.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DEVDRV_IPV4 (0)
#define DEVDRV_IPV6 (1)
#define DEVDRV_MAX_IP_LEN (16)
#define DEVDRV_MAX_LINE_LEN (256)
#define DEVDRV_MIN_IP_VALUE (0)
#define DEVDRV_MAX_IP_VALUE (255)

#define DEVDRV_PART_IP_NUM (4)
#define DEVDRV_INIT_BUF_SIZE (100)
#define DEVDRV_INCRE_BUF_SIZE (10)

#define IFI_HADDR (8) /* allow for 64-bit EUI-64 in future */

#define DEVDRV_PATH_PROCNET_DEV "/proc/net/dev"
#define DEVDRV_PATH_PROCNET_ROUTE "/proc/net/route"

int devdrv_get_ip_address(const char *ethname, struct dmanager_ip_info *ack_msg);
int devdrv_set_ip_address(const char *ethname, struct dmanager_ip_info config_msg);
int devdrv_get_gateway_address(const char *ethname, struct dmanager_gtw_info *ack_msg);
int devdrv_set_gateway_address(char *ethname, struct dmanager_gtw_info config_data, unsigned int ethname_len);
bool devdrv_ipv6_ip_is_local(const ipaddr_t *ip_addr);

#endif /* _ETHERNET_H */
