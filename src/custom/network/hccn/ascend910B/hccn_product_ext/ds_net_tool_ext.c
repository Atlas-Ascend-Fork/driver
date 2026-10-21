/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include "securec.h"
#include "user_log.h"
#include "ascend_hal.h"
#include "ds_net.h"
#include "hccn_dfx_struct.h"
#include "ds_net_ext.h"
#include "tool.h"
#include "hccn_comm.h"
#include "dsmi_common_interface.h"
#include "ds_net_common.h"
#include "ds_net_tool_ext.h"

// 判断源die与目的die是否属于同一个server内
STATIC bool dsmi_ping_is_same_pod(uint32_t src_phy_id, uint32_t dstip, uint32_t dest_sdid)
{
    int ret;
    struct dsmi_spod_info src_sdid_info = {0};
    unsigned int len = sizeof(struct dsmi_spod_info);
    unsigned int dst_server_id;
    unsigned int src_logic_id;

    ret = dsmi_get_logicid_from_phyid(src_phy_id, &src_logic_id);
    if (ret != 0) {
        roce_err("Ping dsmi_ping_is_same_serverid get logicid from phyid fail. (ret=[%d]; phy_id=[%d])", ret,
                 src_phy_id);
        return ret;
    }

    // 获取源die的server信息
    ret = dsmi_get_device_info(src_logic_id, DSMI_MAIN_CMD_CHIP_INF, DSMI_CHIP_INF_SUB_CMD_SPOD_INFO,
                               (void *)&src_sdid_info, &len);
    if (ret != 0) {
        roce_err("PING dsmi_ping_is_same_serverid get device info failed. (ret=[%d])", ret);
        return false;
    }

    // 根据目的die的ip地址或sdid，计算目的die所属的server_id
    // ip地址格式：192. server_id . 2+device侧devid . 199-host侧devid
    // sdid格式：10位server_id | 4位chip_id | 2位die_id | 16位host侧devid
    if (dstip != PING_IP_INVALID_VALUE) {
        dst_server_id = (ntohl(dstip) >> VNIC_IP_SERVER_ID) & 0xFF;
    } else {
        dst_server_id = (dest_sdid >> SDID_SERVER_ID) & 0x3FF;
    }

    return src_sdid_info.server_id == dst_server_id;
}

// 判断源die与目的die是否属于同一块NPU
STATIC bool dsmi_ping_is_same_card(uint32_t src_phy_id, uint32_t dest_phy_id, char *is_same_card)
{
    if ((src_phy_id / MOD_NUM_TWO) == (dest_phy_id / MOD_NUM_TWO)) {
        *is_same_card = 1;
        return true;
    }

    return false;
}

int dsmi_ping_enable_p2p(uint32_t src_phy_id, uint32_t dest_phy_id, uint32_t dstip, uint32_t dest_sdid,
                         char *is_same_card)
{
    int ret;
    uint32_t status;
    uint32_t dev1;
    uint32_t dev2;
    uint32_t dev1_phyid = src_phy_id;
    uint32_t dev2_phyid = dest_phy_id;

    if (!dsmi_ping_is_same_pod(src_phy_id, dstip, dest_sdid) ||
        dsmi_ping_is_same_card(src_phy_id, dest_phy_id, is_same_card)) {
        roce_info("PING dsmi_ping_enable_p2p is not in same serverid, or src and dst are on the same card."
                  " (dev1_phyid=[%u]; dev2_phyid=[%u])",
                  dev1_phyid, dev2_phyid);
        return 0;
    }
    ret = dsmi_get_logicid_from_phyid(dev1_phyid, &dev1);
    if (ret != 0) {
        roce_err("Ping dsmi_ping_enable_p2p get logicid from phyid fail. (ret=[%d]; phy_id=[%d])", ret, dev1_phyid);
        return ret;
    }
    ret = dsmi_get_logicid_from_phyid(dev2_phyid, &dev2);
    if (ret != 0) {
        roce_err("Ping dsmi_ping_enable_p2p get logicid from phyid fail. (ret=[%d]; phy_id=[%d])", ret, dev2_phyid);
        return ret;
    }

    ret = halDeviceEnableP2P(dev1, dev2_phyid, 0);
    if (ret != 0) {
        roce_err("PING dsmi_ping_enable_p2p dev1 enable p2p failed. (ret=[%d]; dev1=[%u]; dev2=[%u])", ret, dev1, dev2);
        return ret;
    }

    ret = halDeviceEnableP2P(dev2, dev1_phyid, 0);
    if (ret != 0) {
        roce_err("PING dsmi_ping_enable_p2p dev2 enable p2p failed; (ret=[%d]; dev1=[%u]; dev2=[%u])", ret, dev1, dev2);
        return ret;
    }

    roce_info("PING dsmi_ping_enable_p2p ok. (dev1=[%u]; dev2=[%u])", dev1, dev2);
    return ret;
}

int dsmi_ping_disable_p2p(uint32_t src_phy_id, uint32_t dest_phy_id, uint32_t dstip, uint32_t dest_sdid,
                          char *is_same_card)
{
    int ret;
    uint32_t status;
    uint32_t dev1;
    uint32_t dev2;
    uint32_t dev1_phyid = src_phy_id;
    uint32_t dev2_phyid = dest_phy_id;

    if (!dsmi_ping_is_same_pod(src_phy_id, dstip, dest_sdid) ||
        dsmi_ping_is_same_card(src_phy_id, dest_phy_id, is_same_card)) {
        roce_info("PING dsmi_ping_disable_p2p is not in same serverid, or src and dst are on the same card."
                  " (dev1_phyid=[%u]; dev2_phyid=[%u])",
                  dev1_phyid, dev2_phyid);
        return 0;
    }
    ret = dsmi_get_logicid_from_phyid(dev1_phyid, &dev1);
    if (ret != 0) {
        roce_err("Ping dsmi_ping_disable_p2p get logicid from phyid fail. (ret=[%d]; phy_id=[%d])", ret, dev1_phyid);
        return ret;
    }
    ret = dsmi_get_logicid_from_phyid(dev2_phyid, &dev2);
    if (ret != 0) {
        roce_err("Ping dsmi_ping_disable_p2p get logicid from phyid fail. (ret=[%d]; phy_id=[%d])", ret, dev2_phyid);
        return ret;
    }

    ret = drvGetP2PStatus(dev1, dev2_phyid, &status);
    if (ret != 0) {
        roce_err("PING dsmi_ping_disable_p2p get p2p status failed. (ret=[%d]; dev1=[%u]; dev2=[%u])", ret, dev1, dev2);
        return ret;
    }

    // p2p开关与进程绑定。本进程没有退出时，应该为enable状态
    if (status == DRV_P2P_STATUS_DISABLE) {
        roce_err("PING dsmi_ping_disable_p2p already disable. (dev1=[%u]; dev2=[%u])", dev1, dev2);
        return ret;
    }

    ret = halDeviceDisableP2P(dev1, dev2_phyid, 0);
    if (ret != 0) {
        roce_err("PING dsmi_ping_disable_p2p dev1 disable p2p failed. (ret=[%d]; dev1=[%u]; dev2=[%u])", ret, dev1,
                 dev2);
        return ret;
    }

    ret = halDeviceDisableP2P(dev2, dev1_phyid, 0);
    if (ret != 0) {
        roce_err("PING dsmi_ping_disable_p2p dev2 disable p2p failed. (ret=[%d]; dev1=[%u]; dev2=[%u])", ret, dev1,
                 dev2);
        return ret;
    }

    roce_info("PING dsmi_ping_disable_p2p ok. (dev1=[%u]; dev2=[%u])", dev1, dev2);
    return ret;
}

int dsmi_inet_ntop_ipv6_address(struct ds_gateway_v6_addr *gw_v6_ip)
{
    const char *str = NULL;
    char net_addr[INET6_ADDRSTRLEN] = {0};

    str = inet_ntop(AF_INET6, gw_v6_ip->address, net_addr, sizeof(net_addr));
    if (str == NULL) {
        roce_err("the value of gateway ipv6 is a invalid!");
        return -EINVAL;
    }

    return 0;
}

int dsmi_set_ipv6_gateway_address(int logic_id, struct ds_gateway_v6_addr gw_v6_ip)
{
    int ret;
    int size_out = 0;
    struct ds_trans_data data_info = {0};

    /* before set new gateway, check the value of gateway is correct */
    ret = dsmi_inet_ntop_ipv6_address(&gw_v6_ip);
    if (ret) {
        roce_err("dsmi inet ntop ipv6 address fail. (ret:%d; logic_id:%d)", ret, logic_id);
        return ret;
    }

    DSMI_COMMON_SET_SEND_DATA(data_info, DS_SET_DEFAULT_GATEWAY_IPV6, (char *)&gw_v6_ip,
                              sizeof(struct ds_gateway_v6_addr), NULL, &size_out);

    ret = dsmi_network_common_send(logic_id, 0, &data_info);
    if (ret) {
        roce_err("dsmi set ipv6 default gateway address failed.(ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }
    return 0;
}

int dsmi_get_ipv6_gateway_address(int logic_id, struct ds_gateway_v6_addr *gw_v6_ip)
{
    int ret;
    int size_out = sizeof(struct ds_gateway_v6_addr);
    struct ds_trans_data data_info = {0};

    DSMI_COMMON_SET_SEND_DATA(data_info, DS_GET_DEFAULT_GATEWAY_IPV6, NULL, 0, (char *)gw_v6_ip, &size_out);

    ret = dsmi_network_common_send(logic_id, 0, &data_info);
    if (ret) {
        roce_err("dsmi get ipv6 default gateway address failed.(ret=%d; logic_id=%d)", ret, logic_id);
        return ret;
    }
    return 0;
}