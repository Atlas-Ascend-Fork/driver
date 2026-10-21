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

#ifndef _PCIVNIC_CONFIG_H_
#define _PCIVNIC_CONFIG_H_

#ifdef CFG_FEATURE_OVER_XCOM
#define NETDEV_PCIDEV_NUM 8U
#else
#define NETDEV_PCIDEV_NUM 64
#endif

#ifdef CFG_FEATURE_OVER_XCOM
#define NETDEV_PF_PCIDEV_NUM 8U
#else
#define NETDEV_PF_PCIDEV_NUM 64
#endif
#define NETDEV_VF_PCIDEV_START 100

#define HOST_MAC_LAST_BYTE 0xd3

#endif
