/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#include "drv_jpege_comm.h"
#include "dvpp_cmdlist_log.h"
#include "jpege_cmdlst_reg_reg_offset.h"
#include "jpg_cvdr_reg_nmanager_reg_offset.h"
#include "jpg_sub_ctrl_reg_offset.h"
#include "jpge_reg_union_define.h"
#include "jpgenc_reg_offset.h"
#include "dvpp_cmdlist_define.h"

const uint32_t DVPP_JPEGE_PREFETCH_DELAY = 1210; // 两个预取命令之间的最小周期数典型值为1210
const uint32_t MASK_IRQ = 0xB0023;               // 默认走stars, 错误中断merge
const uint32_t LAST_MASK_IRQ = 0xB0022;          // 默认走stars, mask正常中断, 错误中断merge
const uint32_t DVPP_JPEGE_INPUT_ADDR_OFFSET = 4;
const uint32_t DVPP_JPEGE_INPUT_HIGH_ADDR_OFFSET = 34;

#define QTABLE_SIZE 64
#define G_LUMA_DC_BITS_COUNT 16  // LUMA DC位表大小
#define G_LUMA_DC_VALUE_COUNT 12 // LUMA DC位值大小
#define G_LUMA_AC_BITS_COUNT 16
#define G_LUMA_AC_VALUE_COUNT 162
#define G_CHROMA_DC_BITS_COUNT 16
#define G_CHROMA_DC_VALUE_COUNT 12
#define G_CHROMA_AC_BITS_COUNT 16
#define G_CHROMA_AC_VALUE_COUNT 162

// huffman table
// DC 0
const uint8_t LUMA_DC_BITS[G_LUMA_DC_BITS_COUNT] = {
    0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const uint8_t LUMA_DC_VALUE[G_LUMA_DC_VALUE_COUNT] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
};

// AC 0
const uint8_t LUMA_AC_BITS[G_LUMA_AC_BITS_COUNT] = {
    0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D,
};
const uint8_t LUMA_AC_VALUE[G_LUMA_AC_VALUE_COUNT] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71,
    0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72,
    0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3,
    0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
    0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
};

// DC 1
const uint8_t CHROMA_DC_BITS[G_CHROMA_DC_BITS_COUNT] = {
    0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const uint8_t CHROMA_DC_VALUE[G_CHROMA_DC_VALUE_COUNT] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
};

// AC 1
const uint8_t CHROMA_AC_BITS[G_CHROMA_AC_BITS_COUNT] = {
    0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77,
};
const uint8_t CHROMA_AC_VALUE[G_CHROMA_AC_VALUE_COUNT] = {
    0,   1,   2,   3,   17,  4,   5,   33,  49,  6,   18,  65,  81,  7,   97,  113, 19,  34,  50,  129, 8,
    20,  66,  145, 161, 177, 193, 9,   35,  51,  82,  240, 21,  98,  114, 209, 10,  22,  36,  52,  225, 37,
    241, 23,  24,  25,  26,  38,  39,  40,  41,  42,  53,  54,  55,  56,  57,  58,  67,  68,  69,  70,  71,
    72,  73,  74,  83,  84,  85,  86,  87,  88,  89,  90,  99,  100, 101, 102, 103, 104, 105, 106, 115, 116,
    117, 118, 119, 120, 121, 122, 130, 131, 132, 133, 134, 135, 136, 137, 138, 146, 147, 148, 149, 150, 151,
    152, 153, 154, 162, 163, 164, 165, 166, 167, 168, 169, 170, 178, 179, 180, 181, 182, 183, 184, 185, 186,
    194, 195, 196, 197, 198, 199, 200, 201, 202, 210, 211, 212, 213, 214, 215, 216, 217, 218, 226, 227, 228,
    229, 230, 231, 232, 233, 234, 242, 243, 244, 245, 246, 247, 248, 249, 250,
};

static void config_jpege_prefetch(struct CmdNode *node)
{
    union Prefetch prefetch;
    union IDY0 idY0;
    union IDY1 idY1;
    union IDUV0 iduV0;
    union IDUVY iduVY;
    prefetch.u32 = 0;
    prefetch.bits.prefetchEn = 0;
    prefetch.bits.prefetchDelay = DVPP_JPEGE_PREFETCH_DELAY;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_PREFETCH_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, prefetch.u32);

    idY0.u32 = 0;
    idY0.bits.ID0Y = 49; // 49:初始值
    idY0.bits.ID1Y = 50; // 50:初始值
    idY0.bits.ID2Y = 51; // 51:初始值
    idY0.bits.ID3Y = 52; // 52:初始值
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_PREFETCH_IDY0_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, idY0.u32);

    idY1.u32 = 0;
    idY1.bits.ID4Y = 53; // 53:初始值
    idY1.bits.ID5Y = 54; // 54:初始值
    idY1.bits.ID6Y = 55; // 55:初始值
    idY1.bits.ID7Y = 56; // 56:初始值
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_PREFETCH_IDY1_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, idY1.u32);

    iduV0.u32 = 0;
    iduV0.bits.ID0UV = 57; // 57:初始值
    iduV0.bits.ID1UV = 58; // 58:初始值
    iduV0.bits.ID2UV = 59; // 59:初始值
    iduV0.bits.ID3UV = 60; // 60:初始值
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_PREFETCH_IDUV0_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, iduV0.u32);

    iduVY.u32 = 0;
    iduVY.bits.ID8Y = 61;  // 61:初始值
    iduVY.bits.ID4UV = 62; // 62:初始值
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_PREFETCH_IDUVY_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, iduVY.u32);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege prefetch reg set ready!");
}

static void config_jpege_rst(struct CmdNode *node)
{
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_RESTART_INTERVAL_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, 0);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege restart marker reg set ready!");
}

static void config_jpege_synchronization(struct CmdNode *node)
{
    union SYNCCFG syn;
    syn.u32 = 0;
    syn.bits.source = 1;
    syn.bits.sramNoopt = 1;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_SYNCCFG_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, syn.u32);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege source synchronization reg set ready!");
}

static void config_jpege_addr(struct CmdNode *node, const jpege_config *jpege_cfg, uint64_t buf_addr)
{
    // 配置输入地址
    uint32_t reg_val = (uint32_t)((jpege_cfg->y_phy_addr >> NUM_32) & 0xFFFFFFFF);
    dvpp_seq_set_cmdnode_val(node, SOC_jpg_sub_ctrl_JPGENC_SHIM_ARADDR_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    // 配置输出地址
    reg_val = (uint32_t)((buf_addr >> NUM_32) & 0xFFFFFFFF);
    dvpp_seq_set_cmdnode_val(node, SOC_jpg_sub_ctrl_JPGENC_SHIM_AWADDR_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege addr reg set ready!");
}

static void config_jpege_cvdr(struct CmdNode *node)
{
    // 芯片给出CVDR_NR_RD3 nrrd_allocated_du配置为0xe
    uint32_t reg_val = 0x800001C0;
    dvpp_seq_set_cmdnode_val(node, SOC_jpg_cvdr_reg_nmanager_AXI_JPEG_NR_RD_CFG_3_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    // 芯片给出CVDR_NR_RD4 nrrd_allocated_du配置为0x0
    reg_val = 0x80000000;
    dvpp_seq_set_cmdnode_val(node, SOC_jpg_cvdr_reg_nmanager_AXI_JPEG_NR_RD_CFG_4_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    // CVDR_NR_WR3、CVDR_NR_WR4将31bit:nrwr_enable置为1
    reg_val = 0x80000000;
    dvpp_seq_set_cmdnode_val(node, SOC_jpg_cvdr_reg_nmanager_AXI_JPEG_NR_WR_CFG_3_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);
    dvpp_seq_set_cmdnode_val(node, SOC_jpg_cvdr_reg_nmanager_AXI_JPEG_NR_WR_CFG_4_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    // AXI_JPEG_LIMITER_NR_WR3、AXI_JPEG_LIMITER_NR_RD3、AXI_JPEG_LIMITER_NR_WR4、AXI_JPEG_LIMITER_NR_RD4
    // 配置默认值0x0F00FFFF
    reg_val = 0x0F00FFFF;
    dvpp_seq_set_cmdnode_val(node, SOC_jpg_cvdr_reg_nmanager_AXI_JPEG_LIMITER_NR_RD_3_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);
    dvpp_seq_set_cmdnode_val(node, SOC_jpg_cvdr_reg_nmanager_AXI_JPEG_LIMITER_NR_RD_4_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);
    dvpp_seq_set_cmdnode_val(node, SOC_jpg_cvdr_reg_nmanager_AXI_JPEG_LIMITER_NR_WR_3_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);
    dvpp_seq_set_cmdnode_val(node, SOC_jpg_cvdr_reg_nmanager_AXI_JPEG_LIMITER_NR_WR_4_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege cvdr reg set ready!");
}

static void config_jpege_icr(struct CmdNode *node)
{
    union statusICR icr;
    icr.u32 = 0;
    icr.bits.encodeDoneIsr = 0x1;
    icr.bits.genHeaderDoneIsr = 0x1;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_STATUS_ICR_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, icr.u32);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege icr reg set ready!");
}

static void config_jpege_pic_format(struct CmdNode *node, const jpege_config *jpege_cfg)
{
    union inputSwap swap;

    // 输入格式寄存器:0(yuv422 UYVY)、1(yuv420 nv12)
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_PIC_FORMAT_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, jpege_cfg->is_yuv420);

    // 输入yuv420 nv21、yuv422 YUYV、YVYU、VYUY需要通过uv swap相应转换成yuv420 nv12或yuv422 UYVY
    swap.u32 = 0;
    swap.bits.swapinYUV = jpege_cfg->swap_yuv;
    swap.bits.swapinUV = jpege_cfg->swap_uv;

    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_INPUT_SWAP_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, swap.u32);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege format:%u, input swap:%u\n", jpege_cfg->is_yuv420, swap.u32);
}

static void config_jpege_pic_size(struct CmdNode *node, const jpege_config *jpege_cfg)
{
    uint32_t reg_val = 0;
    DVPP_CMDLIST_JPEGE_LOG_DEBUG("input frame width_minus1 %u, height_minus1 %u\n", jpege_cfg->width_minus1,
                                 jpege_cfg->height_minus1);

    // JPE_ENC_HRIGHT1寄存器配置图像宽(实际宽-1)
    reg_val = jpege_cfg->width_minus1;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_ENC_HRIGHT1_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    // JPE_ENC_VBOTTOM寄存器配置图像高(实际高-1)
    reg_val = jpege_cfg->height_minus1;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_ENC_VBOTTOM_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    // JPE_ENC_HRIGHT2:暂时未用到
    reg_val = 0;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_ENC_HRIGHT2_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);
}

static void config_jpege_quant(struct CmdNode *node, const jpege_config *jpege_cfg)
{
    // 前面准备语法头时，已经根据质量参数更新量化表，这里只需要写进去就行，YUV使用0,1,1
    // 当前芯片UV分量只支持使用同一张量化表

    // q-table 0
    uint32_t reg_val = 0;
    union tableData data;
    uint32_t i = 0;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_ID_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    for (i = 1; i < QTABLE_SIZE; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->y_quant_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->y_quant_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }

    // q-table 1
    reg_val = 1;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_ID_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);
    for (i = 1; i < QTABLE_SIZE; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->cb_quant_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->cb_quant_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }

    // q-table 2
    reg_val = NUM_2;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_ID_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);
    for (i = 1; i < QTABLE_SIZE; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->cr_quant_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->cr_quant_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }

    // TQ
    reg_val = 0;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TQ_Y_SELECT_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);
    reg_val = 1;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TQ_U_SELECT_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);
    reg_val = 1;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TQ_V_SELECT_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege quant reg set ready!");
}

static void config_jpege_huffman_dc0(struct CmdNode *node, const jpege_config *jpege_cfg)
{
    uint32_t length = G_LUMA_DC_BITS_COUNT + G_LUMA_DC_VALUE_COUNT;
    uint32_t reg_val = NUM_4; // VLC DC-table 0
    union tableData data;
    uint32_t i = 0;

    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TDC0_LEN_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, length);

    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_ID_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    for (i = 1; i < G_LUMA_DC_BITS_COUNT; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->dc0_bits_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->dc0_bits_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }

    for (i = 1; i < G_LUMA_DC_VALUE_COUNT; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->dc0_val_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->dc0_val_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }
}

static void config_jpege_huffman_ac0(struct CmdNode *node, const jpege_config *jpege_cfg)
{
    uint32_t length = G_LUMA_AC_BITS_COUNT + G_LUMA_AC_VALUE_COUNT;
    uint32_t reg_val = NUM_5; // VLC AC-table 0
    union tableData data;
    uint32_t i = 0;

    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TAC0_LEN_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, length);

    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_ID_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    for (i = 1; i < G_LUMA_AC_BITS_COUNT; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->ac0_bits_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->ac0_bits_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }

    for (i = 1; i < G_LUMA_AC_VALUE_COUNT; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->ac0_val_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->ac0_val_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }
}

static void config_jpege_huffman_dc1(struct CmdNode *node, const jpege_config *jpege_cfg)
{
    uint32_t length = G_CHROMA_DC_BITS_COUNT + G_CHROMA_DC_VALUE_COUNT;
    uint32_t reg_val = NUM_6; // VLC DC-table 1
    union tableData data;
    uint32_t i = 0;

    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TDC1_LEN_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, length);

    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_ID_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    for (i = 1; i < G_CHROMA_DC_BITS_COUNT; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->dc1_bits_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->dc1_bits_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }

    for (i = 1; i < G_CHROMA_DC_VALUE_COUNT; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->dc1_val_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->dc1_val_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }
}

static void config_jpege_huffman_ac1(struct CmdNode *node, const jpege_config *jpege_cfg)
{
    uint32_t length = G_CHROMA_AC_BITS_COUNT + G_CHROMA_AC_VALUE_COUNT;
    uint32_t reg_val = NUM_7; // VLC AC-table 1
    union tableData data;
    uint32_t i = 0;

    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TAC1_LEN_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, length);

    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_ID_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    for (i = 1; i < G_CHROMA_AC_BITS_COUNT; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->ac1_bits_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->ac1_bits_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }

    for (i = 1; i < G_CHROMA_AC_VALUE_COUNT; i += NUM_2) {
        data.u32 = 0;
        data.bits.tableWdataH = jpege_cfg->ac1_val_table[i - 1];
        data.bits.tableWdataL = jpege_cfg->ac1_val_table[i];
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_TABLE_DATA_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, data.u32);
    }
}

static void config_jpege_huffman(struct CmdNode *node, const jpege_config *jpege_cfg)
{
    // DC 0
    config_jpege_huffman_dc0(node, jpege_cfg);

    // AC 0
    config_jpege_huffman_ac0(node, jpege_cfg);

    // DC 1
    config_jpege_huffman_dc1(node, jpege_cfg);

    // AC 1
    config_jpege_huffman_ac1(node, jpege_cfg);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege huffman reg set ready!");
}

static void config_jpege_err_int(struct CmdNode *node)
{
    uint32_t reg_val = 0x690; // 0x690: 4/7/9/10 bit is set 1
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_ERROR_IMR_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege err int reg set ready!");
}

static void config_jpege_input_addr(struct CmdNode *node, const jpege_config *jpege_cfg)
{
    uint32_t reg_val = (jpege_cfg->y_phy_addr & 0x3FFFFFFF0) >> DVPP_JPEGE_INPUT_ADDR_OFFSET;
    union frameStride strideValue;
    // y low addr
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_ADDRESS_Y_L_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    // y high addr
    reg_val = (jpege_cfg->y_phy_addr & 0xFFFFFFFC00000000) >> DVPP_JPEGE_INPUT_HIGH_ADDR_OFFSET;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_ADDRESS_Y_H_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    if (jpege_cfg->is_yuv420 == 1) {
        // c low addr
        reg_val = (jpege_cfg->u_phy_addr & 0x3FFFFFFF0) >> DVPP_JPEGE_INPUT_ADDR_OFFSET;
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_ADDRESS_UV_L_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, reg_val);

        // c high addr
        reg_val = (jpege_cfg->u_phy_addr & 0xFFFFFFFC00000000) >> DVPP_JPEGE_INPUT_HIGH_ADDR_OFFSET;
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_ADDRESS_UV_H_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, reg_val);

        // 每次读取MCU的个数
        reg_val = NUM_4;
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_PREREAD_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, reg_val);
    } else {
        reg_val = 0;
        dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_PREREAD_REG & CMDLIST_FIX_WRITE);
        dvpp_seq_set_cmdnode_val(node, reg_val);
    }

    // stride

    strideValue.u32 = 0;
    strideValue.bits.stride = jpege_cfg->y_stride >> NUM_4;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_STRIDE_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, strideValue.u32);
    DVPP_CMDLIST_JPEGE_LOG_DEBUG("y_stride:%u, reg stride:%u\n", jpege_cfg->y_stride, strideValue.u32);
}

static void config_jpege_output_addr(struct CmdNode *node, uint64_t buf_addr)
{
    // output low addr
    uint32_t reg_val = (buf_addr & 0x3FFFFFFF0) >> DVPP_JPEGE_INPUT_ADDR_OFFSET;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_OUT_ADDR_L_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    // output high addr
    reg_val = (buf_addr & 0xFFFFFFFC00000000) >> DVPP_JPEGE_INPUT_HIGH_ADDR_OFFSET;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_OUT_ADDR_H_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);
}

static void config_jpege_pic_info(struct CmdNode *node, jpege_config *jpege_cfg, uint64_t buf_addr)
{
    // 1. JPE_STATUS_ICR寄存器配置
    config_jpege_icr(node);

    // 2. 配置输入图像格式
    config_jpege_pic_format(node, jpege_cfg);

    // 3. 配置输入图像大小
    config_jpege_pic_size(node, jpege_cfg);

    // 4. 配置量化表
    config_jpege_quant(node, jpege_cfg);

    // 5. 配置哈夫曼表
    config_jpege_huffman(node, jpege_cfg);

    // 6. 配置错误中断mask
    config_jpege_err_int(node);

    // 7. 配置输入buffer
    config_jpege_input_addr(node, jpege_cfg);

    // 8. 配置输出buffer
    config_jpege_output_addr(node, buf_addr);
}

static void config_jpege_start(struct CmdNode *node)
{
    uint32_t reg_val = 0x1;
    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_ENCODE_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    dvpp_seq_set_cmdnode_val(node, SOC_JPGENC_JPE_INIT_REG & CMDLIST_FIX_WRITE);
    dvpp_seq_set_cmdnode_val(node, reg_val);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege start reg set ready!");
}

static void config_jpege_read(struct CmdNode *node)
{
    // 配置要回读的寄存器的偏移地址, 0x1代表读操作, 左移24位要配置的是读操作寄存器的个数0~255代表个数1~256
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGENC_JPG_BYTE_CNT_REG & CMDLIST_FIX_WRITE) | 0x1 | (0 << 24));
}

static void config_cmdnode_length(struct CmdNode *node)
{
    // 1.配置cmdlst第一个节点的长度
    node->len0 = node->dataSize;
    node->len1 = sizeof(uint32_t);                           // 回读1个寄存器，所以长度是1
    node->len1 = DVPP_ALIGN_UP(node->len1, ADDR_ALIGN_SIZE); // 128对齐
    DVPP_CMDLIST_JPEGE_LOG_DEBUG("1. cmdlist current:%u, len0:%u, len1:%u", node->dataSize, node->len0, node->len1);
}

void config_jpege_dump_addr(struct CmdNode *node, jpege_config *jpege_cfg)
{
    uint32_t high_addr = 0;
    uint32_t low_addr = 0;
    // 计算高位地址 CMDLST_HW_AXI_ADDR_DUMP_H
    dvpp_seq_set_cmdnode_val(node, SOC_CMDLST_REG_CMDLST_HW_AXI_ADDR_DUMP_H_REG & CMDLIST_FIX_WRITE);
    high_addr = (jpege_cfg->read_user_addr >> 39); // 39~63bit
    dvpp_seq_set_cmdnode_val(node, high_addr);

    // 计算低位地址 CMDLST_HW_AXI_ADDR_DUMP_L
    dvpp_seq_set_cmdnode_val(node, SOC_CMDLST_REG_CMDLST_HW_AXI_ADDR_DUMP_L_REG & CMDLIST_FIX_WRITE);
    low_addr = (jpege_cfg->read_user_addr >> NUM_7) & 0xFFFFFFFF; // 7~38bit
    dvpp_seq_set_cmdnode_val(node, low_addr);
}

static void config_jpege_reg(struct CmdNode *node, jpege_config *jpege_cfg)
{
    uint64_t buf_addr = 0;
    // 4. 配置回读操作地址
    config_jpege_dump_addr(node, jpege_cfg);

    // 5. 初始化阶段寄存器配置
    config_jpege_prefetch(node);
    config_jpege_rst(node);
    config_jpege_synchronization(node);

    // 6. 获取输入输出地址并配置相关寄存器,输出地址已往后偏移了协议大小
    buf_addr = jpege_cfg->stream_addr;
    config_jpege_addr(node, jpege_cfg, buf_addr);

    // 7. 配置CVDR寄存器,主要与性能相关
    config_jpege_cvdr(node);

    // 9. jpege具体参数配置
    config_jpege_pic_info(node, jpege_cfg, buf_addr);

    // 10.配置jpege启动寄存器
    config_jpege_start(node);

    // 12. buffer0填充padding数据保证在ddr中的长度128byte对齐
    add_padding_data(node);

    // 13. 配置节点寄存器配置长度， len0为cmdbuffer0, len1为cmdbuffer1，len1目前只有1个回读寄存器
    config_cmdnode_length(node);

    // 14. 回读操作
    config_jpege_read(node);

    // 15.buffer1填充padding数据保证在ddr中的长度128byte对齐
    add_padding_data(node);
}

void config_jpege_cmdnode(struct CmdNode *node, jpege_config *jpege_cfg)
{
    config_jpege_cmdnode_reverse_space(node);

    config_jpege_reg(node, jpege_cfg);
}

void config_jepge_next_node(struct CmdBuf *cmd_buf, struct CmdNode *node)
{
    uint64_t baseAddr = 0;
    // 1. 填写下一个节点在ddr中的长度
    // 配置CMDLST_HW_BUF_LEN, 回读寄存器占用的cmdlist buffer长度, 目前仅回读一个寄存器JPG_BYTE_CNT
    DVPP_CMDLIST_JPEGE_LOG_DEBUG("2. cmdlist current:%u, len0:%u, len1:%u", node->next->dataSize, node->next->len0,
                                 node->next->len1);
    dvpp_ext_set_cmdnode_val(node, SOC_CMDLST_REG_CMDLST_HW_BUF_LEN_REG & CMDLIST_FIX_WRITE);
    dvpp_ext_set_cmdnode_val(node, (node->next->len0 >> DVPP_JPEGE_INPUT_ADDR_OFFSET) |
                                       ((node->next->len1 >> DVPP_JPEGE_INPUT_ADDR_OFFSET) << NUM_16));

    // 2. 预测计算下一个节点的地址
    baseAddr = (uint64_t)(uintptr_t)(node->next->startAddr) - (uint64_t)(uintptr_t)(cmd_buf->startAddr) +
               cmd_buf->ioStartAddr;

    // 3.1计算高位地址
    dvpp_ext_set_cmdnode_val(node, SOC_CMDLST_REG_CMDLST_HW_AXI_ADDR_H_REG & CMDLIST_FIX_WRITE);
    dvpp_ext_set_cmdnode_val(node, baseAddr >> 39); // 39~63bit

    // 3.2计算低位地址
    dvpp_ext_set_cmdnode_val(node, SOC_CMDLST_REG_CMDLST_HW_AXI_ADDR_L_REG & CMDLIST_FIX_WRITE);
    dvpp_ext_set_cmdnode_val(node, (baseAddr >> NUM_7) & 0xFFFFFFFF); // 7~38bit
}

void config_jpege_interrupt(struct CmdNode *node, uint32_t isLastNode)
{
    // 清中断寄存器,默认值为0,0~4bit设置为1
    uint32_t regVal = 0x1F;
    dvpp_ext_set_cmdnode_val(node, SOC_jpg_sub_ctrl_JPGENC_IRQ_REG0_REG & CMDLIST_FIX_WRITE);
    dvpp_ext_set_cmdnode_val(node, regVal);

    // 中断unmake, 多节点只有最后一个节点unmask中断
    if (isLastNode == 1) {
        // 0和2~4bit设置为1 2~4bit:jpgenc_err_irq cvdr_axi_rd_resp_err cvdr_axi_wr_resp_err 和0bit:jpgenc_nrwr_eof中断
        regVal = LAST_MASK_IRQ;
    } else {
        // 2~4bit设置为1 2~4bit:jpgenc_err_irq cvdr_axi_rd_resp_err cvdr_axi_wr_resp_err
        regVal = MASK_IRQ;
    }
    dvpp_ext_set_cmdnode_val(node, SOC_jpg_sub_ctrl_JPGENC_IRQ_REG1_REG & CMDLIST_FIX_WRITE);
    dvpp_ext_set_cmdnode_val(node, regVal);

    DVPP_CMDLIST_JPEGE_LOG_DEBUG("jpege irq reg set ready!");
}

void drv_jpege_config_sqe(struct CmdBuf *cmd_buf, enum dvpp_sqe_ptr_mode mode, uint32_t blkdim, struct dvpp_sqe *sqe)
{
    if (mode == DVPP_SQE_FIRST_PTR_MODE) {
        uint64_t offset = (uint64_t)(uintptr_t)(cmd_buf->cmdHeadNode->startAddr) -
                          (uint64_t)(uintptr_t)(cmd_buf->startAddr);
        uint64_t startAddr = cmd_buf->ioStartAddr + offset;

        sqe->filed[NUM_0] |= DVPP_SQE_JPEGE;
        sqe->filed[NUM_3] = (KERNEL_CREDIT << SQE_BLKDIM_OFFSET);
        sqe->filed[NUM_3] &= ~(0x1 << SQE_PTRMODE_OFFSET);
        // Start address [38:7] of the frame (in 128 bytes boundary) for configure buffer.
        sqe->filed[NUM_4] = GET_LOW_ADDR((startAddr >> NUM_7));
        // Start address [63:39] of the frame (in 128 bytes boundary) for configure buffer.
        sqe->filed[NUM_5] = startAddr >> NUM_39;
        sqe->filed[NUM_6] = (cmd_buf->cmdHeadNode->len0 >> ALIGN_LEN) |
                            ((cmd_buf->cmdHeadNode->len1 >> ALIGN_LEN) << NUM_16);
    } else {
        // TD 二级链表模式(多block dim)
        sqe->filed[NUM_0] |= DVPP_SQE_JPEGE;                // Type=13 JPEGE任务
        sqe->filed[NUM_0] |= (blkdim << SQE_BLKDIM_OFFSET); // 第0个4字节的第16-31位表示blockdim的数量
        sqe->filed[NUM_3] = (KERNEL_CREDIT << SQE_TIMEOUT_OFFSET); // 第3个4字节的第16-23位表示kernel_credit(timeout):
                                                                   // 全1表示该任务不记录超时
        sqe->filed[NUM_3] |= (0x1 << SQE_PTRMODE_OFFSET); // 第3个4字节的第24位表示链表模式，1表示二级
        sqe->filed[NUM_4] = GET_LOW_ADDR(cmd_buf->ioAddr); // 第4个4字节存放二级链表的低32位地址
        sqe->filed[NUM_5] = GET_HIGH_ADDR(cmd_buf->ioAddr); // 第5个4字节的低16位存放二级链表的高16位地址
        sqe->filed[NUM_5] &= 0x1FFFF;                       // 第5个4字节的17~30bit是保留字段
        sqe->filed[NUM_5] |= (0x1 << SQE_ADDRTYPE_OFFSET); // 第5个4字节的最高位表示地址类型 1:虚拟地址 0:物理地址
    }
}
