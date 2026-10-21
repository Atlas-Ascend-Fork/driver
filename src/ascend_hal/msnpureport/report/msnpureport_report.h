/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MSNPUREPORT_REPORT_H
#define MSNPUREPORT_REPORT_H

#include "msnpureport_common.h"

typedef enum {
    ALL_LOG,
    SLOG_LOG, // contain message
    HISILOGS_LOG,
    STACKCORE_LOG,
    VMCORE_FILE,
    MODULE_LOG,
#ifdef UB_SUPPORT
    UB_LOG = 6,
#endif
#ifdef AO_SUPPORT
    AO_SELF_LOG = 7,
    AO_CNT_LOG = 8,
    AO_UART_LOG = 9,
#endif
#ifdef CCU_SUPPORT
    CCU_LOG = 10,
#endif
#ifdef NETWORK_SUPPORT
    NETWORK_LOG = 11,
#endif
    MAX_TYPE_NUM
} MsnLogType;

#ifdef UB_SUPPORT
#define MAX_UB_INFO_SIZE 4000
#define UB_INFO_WAIT_TIME 300U // 300 micro second
#define UB_INFO_HISI_LOGS_DIR "hisi_logs"
#define UB_INFO_DIR "ub"
#define UB_INFO_MAMI_DIR "mami"
#define UB_SUB_CMD_UB_DFX_INFO 1U
#define UB_INFO_UBNL_DFX_STATISTIC_PARAM_MINOR 34    // MAMI_NL_DFX_TXPA_GLB
#define UB_INFO_UBNL_DFX_SSU_SCHEDULE_PARAM_MINOR 28 // MAMI_NL_DFX_SSU_GLB
#define UB_INFO_UBNL_DFX_CONFIG_ITEM_PARAM_MINOR 38  // MAMI_NL_DFX_RXPA_ROUTE
#define UB_INFO_UBNL_DFX_PARAM_ID_TYPE 2
#define UB_INFO_UBNL_DFX_PARAM_PLANE_ID 0
#define UB_INFO_UBMEM_DAW_PARAM_MINOR 0
#define UB_INFO_UBTPL_ACL_PARAM_MINOR 0x01
#define UB_INFO_UBTPL_ACL_PARMA_MASK 5
#define UB_INFO_SL_TO_VL_PARAM_MINOR 1
#define UB_INFO_UBQOS_MAX_SL_NUM 16
#define UB_INFO_MAMI_EID_SIZE (128)
#define UB_INFO_MAMI_EID_DWORD_NUM (UB_INFO_MAMI_EID_SIZE / 32)

typedef enum {
    UB_INFO_PORT_STATUS = 0,           // 端口状态
    UB_INFO_PORT_PERF_TEST = 1,        // 端口峰值带宽
    UB_INFO_DLPHY_GET_CONFIG = 2,      // DLPHY层状态统计
    UB_INFO_UBNL_DFX_STATISTIC = 3,    // 网络管理维测信息, 网络层需要支持统计信息查询
    UB_INFO_UBNL_DFX_SSU_SCHEDULE = 4, // 网络管理维测信息, SSU调度队列统计和队列丢包统计
    UB_INFO_UBNL_DFX_CONFIG_ITEM = 5,  // 网络管理维测信息, 配置表项查询
    UB_INFO_UBMEM_DAW = 6,             // 查看DAW
    UB_INFO_UBTPL_ACL = 7,             // 查询ACL表
    UB_INFO_SL_TO_VL = 8,              // 查询SL与VL映射
    UB_INFO_MAX
} UB_INFO_TYPE;

struct DsmiUbDfxInput {
    unsigned int major;
    unsigned int minor;
    unsigned int buf_size;
    unsigned char buf[]; // 以buf_size大小填充
};

typedef enum {
    UB_MAMI_CMD_INVALID = 0,
    UB_MAMI_CMD_IPC,
    UB_MAMI_CMD_UBPORT,
    UB_MAMI_CMD_UBNL,
    UB_MAMI_CMD_UBTPL,
    UB_MAMI_CMD_UBMEM,
    UB_MAMI_CMD_DHCPRELAY,
    UB_MAMI_CMD_DEVREG,
    UB_MAMI_CMD_UBACL,
    UB_MAMI_CMD_UBQOS,
    UB_MAMI_CMD_UBLINKCOM,
    UB_MAMI_CMD_UBIRT,
    UB_MAMI_CMD_UBDEVM,
    UB_MAMI_CMD_MAX
} UB_MAMI_MAJOR_CMD;

typedef enum {
    UB_MAMI_PORT_LINK_STATUS = 0,
    UB_MAMI_PORT_PHY_LINK_STATE,
    UB_MAMI_PORT_SPEED,
    UB_MAMI_PORT_CNA = 5,
    UB_MAMI_BONDING_GROUP,
    UB_MAMI_BONDING_GROUP_CNA,
    UB_MAMI_PORT_GROUP_CNA,
    UB_MAMI_PORT_RESET,
    UB_MAMI_PORT_ENABLE,
    UB_MAMI_PORT_DISABLE,
    UB_MAMI_PORT_TOPO,
    UB_MAMI_PORT_FEC_MODE,
    UB_MAMI_PORT_FEC_STATS,
    UB_MAMI_PORT_PERF_TEST,
    UB_MAMI_PORT_DLPHY_PMU_STATS,
    UB_MAMI_PORT_DLPHY_PFA_STATS,
    UB_MAMI_PORT_TP_STATS,
    UB_MAMI_TA_DFX_MRD_STATS,
    UB_MAMI_TA_DFX_EIP_STATS,
    UB_MAMI_TA_DFX_USI_STATS,
    UB_MAMI_PORT_SU_BER_STATS,
    UB_MAMI_PORT_CLEAR,
    UB_MAMI_PORT_AUTO_LANE,
    UB_MAMI_DLPHY_INIT_CONFIG,
    UB_MAMI_DLPHY_MODIFY_CONFIG,
    UB_MAMI_DLPHY_GET_CONFIG,
    // MISC DFX 先行,暂时放在ubport中， SUBCTRL超窗口接口先去除
    UB_MAMI_MISC_IMP_INIT_STATS,
    UB_MAMI_MISC_IMP_STATS,
    UB_MAMI_MISC_CFG_BUS_STATS,
    UB_MAMI_MISC_ERMG_STATS,
    UB_MAMI_MISC_INTC_NBAR_STATS,
    UB_MAMI_MISC_INTC_LOCAL_RAS_STATS,
    UB_MAMI_MISC_INTC_AER_RAS_STATS,
    UB_MAMI_MISC_INTC_MISC_RAS_STATS,
    UB_MAMI_MISC_INTC_BAR_STATS,
    UB_MAMI_PORT_LANE_NUM,
    UB_MAMI_DLPHY_DFX_MUXPCS_GLB,
    UB_MAMI_DLPHY_DFX_MAC_MUX_PORT,
    UB_MAMI_DLPHY_DFX_PCS_COM_STATS,
    UB_MAMI_DLPHY_DFX_PCS_LANE,
    UB_MAMI_DLPHY_DFX_CFGSPACE_PHY,
    UB_MAMI_DLPHY_DFX_CFGSPACE_DL,
    UB_MAMI_DLPHY_DFX_DL_PORT_STATS,
    UB_MAMI_BA_DFX_RX_DMA,
    UB_MAMI_BA_DFX_TX_DMA_PORT,
    UB_MAMI_BA_DFX_TX_DMA_GLOBAL,
    UB_MAMI_BA_DFX_BA_MASTER,
    UB_MAMI_TP_DFX_PPP_GLB_STATS,
    UB_MAMI_TP_DFX_RX_DMA_HEAD_STATS,
    UB_MAMI_TP_DFX_TPP_STATS,
    UB_MAMI_TP_DFX_TPRXP_STATS,
    UB_MAMI_TP_DFX_TAI_TQS_STATS,
    UB_MAMI_PORT_LANES_DISABLE,
    UB_MAMI_PORT_CTP_RC_TAACK_MODE,
    UB_MAMI_PORT_MAX
} UB_MAMI_PORT_MINOR_CMD;

struct UbMamiStatsKey {
    uint32_t idType; // 支持portSet粒度
    union {
        uint32_t planeId;
        uint32_t portSetId;
        uint32_t lgPortId;
    };
    uint32_t rsv[8]; // 预留字段，写0
};

struct UbMamiEid {
    uint8_t compressedFlag; /* 0: 128位非压缩格式的eid; 1: 压缩格式的eid */
    uint8_t rsv[3];
    union {
        uint32_t eid[UB_INFO_MAMI_EID_DWORD_NUM];
        uint32_t compressedEid; /* 20bit */
    };
};

struct UbMamiTplAclQry {
    uint32_t planeId;
    uint16_t ueIdx;
    uint8_t routeType : 2; /* 0-IPv4; 1-IPv6; 2-CNA; 在枚举mamiTplRouteType中取值 */
    uint32_t rsv0 : 14;
    struct UbMamiEid seid;
    struct UbMamiEid deid;
    uint32_t transType;
    uint32_t aclGrpId;
    uint8_t sip[16];
    uint8_t dip[16];
    uint32_t startIdx; /* 分页查询时的起始编号，仅mamiGetAll有效 */
    uint8_t rsv[36];
    uint32_t fieldMask; /* 在枚举mamiTplAclQryFieldMask中取值 */
};

struct UbMamiSl2Vl {
    uint16_t sl; // 0-15
    uint16_t vl; // 0-15
};

struct UbQosSl2VlCmd {
    unsigned int plane_id;
    unsigned int num;
    struct UbMamiSl2Vl cfg[UB_INFO_UBQOS_MAX_SL_NUM];
};

#endif

#ifdef __cplusplus
extern "C" {
#endif

int32_t MsnReport(ArgInfo *argInfo);

#ifdef __cplusplus
}
#endif
#endif