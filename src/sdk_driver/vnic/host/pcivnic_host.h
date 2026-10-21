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

#ifndef _PCIVNIC_HOST_H_
#define _PCIVNIC_HOST_H_
#include "pbl/pbl_uda.h"
#include "pcivnic_main.h"

#define PCIVNIC_INIT_NETDEV_RETRY_TIMES 3

struct pcivnic_netdev *pcivnic_alloc_vnic_dev(void);
void pcivnic_free_vnic_dev(void);
int pcivnic_register_client(void);
int pcivnic_unregister_client(void);
int pcivnic_init_netdev(struct pcivnic_netdev *vnic_dev);
int pcivnic_init_msg_instance(struct pcivnic_pcidev *pcidev);
struct pcivnic_netdev *pcivnic_get_vnic_dev(void);
unsigned char *pcivnic_get_vnic_mac(void);
int pcivnic_init_instance(u32 dev_id, ka_device_t *dev);
int pcivnic_uninit_instance(u32 dev_id);
int pcivnic_init_msg_chan(struct pcivnic_pcidev *pcidev);
void pcivnic_uninit_msg_chan(struct pcivnic_pcidev *pcidev);
int pcivnic_register_common_client(void);
int pcivnic_unregister_common_client(u32 dev_id);
#endif
