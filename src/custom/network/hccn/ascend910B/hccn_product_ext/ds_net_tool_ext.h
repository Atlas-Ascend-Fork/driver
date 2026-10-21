/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SRC_DRIVER_NETWORK_HCCN_DSMI_DS_NET_TOOL_EXT_H
#define SRC_DRIVER_NETWORK_HCCN_DSMI_DS_NET_TOOL_EXT_H
#include <linux/types.h>

#define MAX_ROCE_PORT_NUM 32
#define UDP_MODE_MULTI_QP 2
#define UDP_MODE_CODE 3
#define QP_NUM_MIN 2
#define QP_NUM_MAX 32
#define UDP_PORT_MIN 4097
#define UDP_PORT_MAX 65535
#define LISTEN_PORT_MIN 10000
#define LISTEN_PORT_MAX 57999
#define DEFAULT_LISTEN_PORT 16668

struct ds_roce_port_info {
    unsigned int port_mode;
    unsigned int port_cnt;
    unsigned int listen_port;
    unsigned int ports_list[MAX_ROCE_PORT_NUM];
};

struct udp_multi_qp_info {
    char *param;
    int (*param_parse_func)(struct ds_roce_port_info *info, char *val, int len);
};

#define MAX_IPV6_NUM 16
struct ds_gateway_v6_addr {
    unsigned char address[MAX_IPV6_NUM];
};

int dsmi_ping_enable_p2p(uint32_t src_phy_id, uint32_t dest_phy_id, uint32_t dstip, uint32_t dest_sdid,
                         char *is_same_card);
int dsmi_ping_disable_p2p(uint32_t src_phy_id, uint32_t dest_phy_id, uint32_t dstip, uint32_t dest_sdid,
                          char *is_same_card);
int dsmi_handle_multi_roce_ports(int logic_id, struct ds_roce_port_info *info, int cmd_mode);
int dsmi_set_ipv6_gateway_address(int logic_id, struct ds_gateway_v6_addr gw_v6_ip);
int dsmi_get_ipv6_gateway_address(int logic_id, struct ds_gateway_v6_addr *gw_v6_ip);

#endif