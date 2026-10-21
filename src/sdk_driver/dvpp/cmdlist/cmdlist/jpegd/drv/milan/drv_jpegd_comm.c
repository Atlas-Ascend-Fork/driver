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

#include "drv_jpegd_comm.h"
#include "dvpp_cmdlist_log.h"
#include "cmdlst_reg_reg_offset.h"
#include "jpg_sub_ctrl_reg_offset.h"
#include "jpgd_cfg_reg_offset.h"
#include "jpgd_reg_union_define.h"
#include "dvpp_cmdlist_define.h"
#include "dvpp_cmdlist_num_define.h"
#include "jpg_cvdr_reg_nmanager_reg_offset.h"

#define LOW_BIT 4                   // 4:低4bit
#define HIGH_BIT 34                 // 34:高34bit
#define CMDLST_LEN_OFFSET 4         // cmdlst每个节点的长度在配置到寄存器时需要右移4字节
#define PAGE_BYTE 2048              // 2048:last page字节对齐
#define MASK_IRQ 0x9003F            // 默认走stars, mask中断, 错误中断merge
#define SMMU_VALUE 0x4075           // 0x4075:user域smmu相关的默认值
#define IRQ_CLEAR 0x3F              // 0x3F:清中断标志0~5bit
#define ENABLE 0x1                  // 1:使能
#define ACCURACY_SELECT 0           // 0:精度与libjpeg保持一致
#define HEIGHT_ALIGN 16             // 16:高度16对齐
#define JPEGD_CMD_NODE_HEAD_SIZE 32 // 32:cmd node预留一段长度用于中断配置和下一个node信息记录

void config_jpegd_cmdnode_reverse_space(struct CmdNode *node)
{
    uint32_t i;
    for (i = 0; i < JPEGD_CMD_NODE_HEAD_SIZE; i++) {
        dvpp_seq_set_cmdnode_val(node, CMDLST_PADDING_DATA);
    }
}

void config_jpegd_interrupt(struct CmdNode *node, uint32_t isLastNode)
{
    uint32_t irq = 0;
    dvpp_ext_set_cmdnode_val(node, (SOC_jpg_sub_ctrl_JPGDEC_CRG_CFG0_0_REG & CMDLIST_FIX_WRITE));
    dvpp_ext_set_cmdnode_val(node, ENABLE);

    dvpp_ext_set_cmdnode_val(node, (SOC_jpg_sub_ctrl_JPGDEC_IRQ_REG0_0_REG & CMDLIST_FIX_WRITE));
    dvpp_ext_set_cmdnode_val(node, IRQ_CLEAR);

    dvpp_ext_set_cmdnode_val(node, (SOC_jpg_sub_ctrl_JPGDEC_IRQ_REG1_0_REG & CMDLIST_FIX_WRITE));
    irq = (isLastNode == 1) ? 0xB0002 : MASK_IRQ;
    dvpp_ext_set_cmdnode_val(node, irq); // 除了1bit设置为1，其余bit位unmask中断
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("cmdlist jpegd irq set:%u\n", irq);

    dvpp_ext_set_cmdnode_val(node, (SOC_jpg_sub_ctrl_JPGDEC_SHIM_AWUSER_1_0_REG & CMDLIST_FIX_WRITE));
    dvpp_ext_set_cmdnode_val(node, SMMU_VALUE);

    dvpp_ext_set_cmdnode_val(node, (SOC_jpg_sub_ctrl_JPGDEC_SHIM_ARUSER_1_0_REG & CMDLIST_FIX_WRITE));
    dvpp_ext_set_cmdnode_val(node, SMMU_VALUE);

    dvpp_ext_set_cmdnode_val(node, (SOC_jpg_sub_ctrl_JPGCMDLST0_CLK_EN_REG & CMDLIST_FIX_WRITE));
    dvpp_ext_set_cmdnode_val(node, ENABLE);
}

void config_jpegd_accuracy(struct CmdNode *node)
{
    // accuracy_select: 0:the same as libjpeg,1:the same as Ascend310P
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_ACCURACY_SEL_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, ACCURACY_SELECT);
}

void config_jpegd_pixel_size(struct CmdNode *node, JpegdConfig *jpegdConfig)
{
    // 1. config input pix width and height
    union FrameSize frameSize;
    union CropHorizontal cropHorizontal;
    union CropVertical cropVertical;
    union FrameLastpageY frameStrideY;
    union FrameLastpageC frameStrideC;
    union FrameStride frameStride;

    frameSize.bits.pixWidth = jpegdConfig->width - 1;
    frameSize.bits.pixHeight = jpegdConfig->height - 1;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_FRAME_SIZE_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, frameSize.u32);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("cmdlist width:%u, height:%u\n", frameSize.bits.pixWidth, frameSize.bits.pixHeight);

    // 2. config crop info h and v
    // 2.1 h
    cropHorizontal.u32 = 0;
    cropHorizontal.bits.pixStartHor = jpegdConfig->horizontalStartPixel;
    cropHorizontal.bits.pixEndHor = jpegdConfig->horizontalEndPixel;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_CROP_HORIZONTAL_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, cropHorizontal.u32);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("cmdlist cropHorizontal start:%u, end:%u\n", cropHorizontal.bits.pixStartHor,
                                 cropHorizontal.bits.pixEndHor);

    // 2.2 v
    cropVertical.u32 = 0;
    cropVertical.bits.pixStartVer = jpegdConfig->verticalStartPixel;
    cropVertical.bits.pixEndVer = jpegdConfig->verticalEndPixel;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_CROP_VERTICAL_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, cropVertical.u32);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("cmdlist cropVertical start:%u, end:%u\n", cropVertical.bits.pixStartVer,
                                 cropVertical.bits.pixEndVer);

    // 3. config frame stride
    // 3.1 y
    frameStrideY.u32 = 0;
    frameStrideY.bits.lastPageY = jpegdConfig->cPhyAddr / PAGE_BYTE;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_LAST_PAGE_Y_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, frameStrideY.u32);

    // 3.2 c
    frameStrideC.u32 = 0;
    frameStrideC.bits.lastPageC = (jpegdConfig->cPhyAddr +
                                   DVPP_ALIGN_UP(jpegdConfig->height, HEIGHT_ALIGN) * jpegdConfig->cStride) /
                                  PAGE_BYTE;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_LAST_PAGE_C_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, frameStrideC.u32);

    // 4. config frame stride y&c
    frameStride.u32 = 0;
    frameStride.bits.strideY = jpegdConfig->yStride >> LOW_BIT;
    frameStride.bits.strideC = jpegdConfig->cStride >> LOW_BIT;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_FRAME_STRIDE_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, frameStride.u32);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("frame ystride:%u, cstride:%u, stride:%u\n", frameStride.bits.strideY,
                                 frameStride.bits.strideC, frameStride.u32);
}

void config_jpegd_addr(struct CmdNode *node, JpegdConfig *jpegdConfig)
{
    uint32_t frameStartYLow = 0;
    uint32_t frameStartYHigh = 0;
    uint32_t frameStartCLow = 0;
    uint32_t frameStartCHigh = 0;
    // 1. config input start stream
    // 1.1 start low
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_BITSTREAMS_START_L_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, GET_LOW_ADDR(jpegdConfig->phyStrStart));

    // 1.2 start high
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_BITSTREAMS_START_H_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, GET_HIGH_ADDR(jpegdConfig->phyStrStart));

    // 2. config end stream
    // 2.1 end low
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_BITSTREAMS_END_L_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, GET_LOW_ADDR(jpegdConfig->phyStrEnd));

    // 2.2 end high
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_BITSTREAMS_END_H_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, GET_HIGH_ADDR(jpegdConfig->phyStrEnd));

    // 3. config frame start y
    // 3.1 Low
    frameStartYLow = (jpegdConfig->yPhyAddr & 0x3FFFFFFF0) >> LOW_BIT;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_FRAME_START_Y_L_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, frameStartYLow);

    // 3.2 High
    frameStartYHigh = (jpegdConfig->yPhyAddr & 0xFFFFFFFC00000000) >> HIGH_BIT;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_FRAME_START_Y_H_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, frameStartYHigh);

    // 4. config frame start c
    // 4.1 Low
    frameStartCLow = (jpegdConfig->cPhyAddr & 0x3FFFFFFF0) >> LOW_BIT;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_FRAME_START_C_L_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, frameStartCLow);

    // 4.2 High
    frameStartCHigh = (jpegdConfig->cPhyAddr & 0xFFFFFFFC00000000) >> HIGH_BIT;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_FRAME_START_C_H_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, frameStartCHigh);
}

void config_jpegd_out_type(struct CmdNode *node, const JpegdConfig *jpegdConfig)
{
    union OutputType outputType;

    // config otput type
    outputType.u32 = 0;
    outputType.bits.outputType = jpegdConfig->outputType;
    outputType.bits.uvSwap = jpegdConfig->uvSwap; // 芯片默认是v在前
    outputType.bits.rgbAlpha = 0;                 // 暂不支持rgb输出
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_OUTPUT_TYPE_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, outputType.u32);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("frame outputType:%u, uvSwap:%u, outputType u32:%u\n", outputType.bits.outputType,
                                 outputType.bits.uvSwap, outputType.u32);
}

void config_jpegd_freq_scale(struct CmdNode *node)
{
    union FreqScale freqScale;

    // config freq_scale
    freqScale.u32 = 0;
    freqScale.bits.freqScale = 0;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_FREQ_SCALE_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, freqScale.u32);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("frame freqScale:0x%x\n", freqScale.u32);
}

void config_jpegd_cvdr_du_nr_rd(struct CmdNode *node)
{
    const uint32_t du_nr_rd = 0x800001c0U; // 芯片推荐的配置值，含义可见寄存器表单的DU配置页
    dvpp_seq_set_cmdnode_val(node, (SOC_jpg_cvdr_reg_nmanager_AXI_JPEG_NR_RD_CFG_0_REG));
    dvpp_seq_set_cmdnode_val(node, du_nr_rd);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("cvdr du:0x%x\n", du_nr_rd);
}

void config_jpegd_sample_factor(struct CmdNode *node, const JpegdConfig *jpegdConfig)
{
    union SamplingFactor sampleFactor;

    // config sample factor
    sampleFactor.u32 = 0;
    sampleFactor.bits.yFac = jpegdConfig->yFac;
    sampleFactor.bits.uFac = jpegdConfig->uFac;
    sampleFactor.bits.vFac = jpegdConfig->vFac;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_SAMPLING_FACTOR_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, sampleFactor.u32);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("sampleFactor:0x%x, yFac:0x%x, uFac:0x%x, vFac:0x%x\n", sampleFactor.u32,
                                 sampleFactor.bits.yFac, sampleFactor.bits.uFac, sampleFactor.bits.vFac);
}

void config_jpegd_dri(struct CmdNode *node, const JpegdConfig *jpegdConfig)
{
    union Dri dri;

    // config dri
    dri.u32 = 0;
    dri.bits.dri = jpegdConfig->dri;
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_DRI_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, dri.u32);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("dri:0x%x\n", dri.u32);
}

void config_jpegd_table(struct CmdNode *node, JpegdConfig *jpegdConfig)
{
    uint32_t i = 0;

    // config tables
    for (i = 0; i < QUANT_TABLE_SIZE; i++) {
        dvpp_seq_set_cmdnode_val(node, ((SOC_JPGD_CFG_QUANT_TABLE_0_REG + i * 0x4) & CMDLIST_FIX_WRITE));
        dvpp_seq_set_cmdnode_val(node, jpegdConfig->quantTable[i]);
    }

    for (i = 0; i < HDC_TABLE_SIZE; i++) {
        dvpp_seq_set_cmdnode_val(node, ((SOC_JPGD_CFG_HDC_TABLE_0_REG + i * 0x4) & CMDLIST_FIX_WRITE));
        dvpp_seq_set_cmdnode_val(node, jpegdConfig->huffmanTable[i]);
    }

    for (i = 0; i < HAC_MIN_TABLE_SIZE; i++) {
        dvpp_seq_set_cmdnode_val(node, ((SOC_JPGD_CFG_HAC_MIN_TABLE_0_REG + i * 0x4) & CMDLIST_FIX_WRITE));
        dvpp_seq_set_cmdnode_val(node, jpegdConfig->huffmanMinTable[i]);
    }

    for (i = 0; i < HAC_BASE_TABLE_SIZE; i++) {
        dvpp_seq_set_cmdnode_val(node, ((SOC_JPGD_CFG_HAC_BASE_TABLE_0_REG + i * 0x4) & CMDLIST_FIX_WRITE));
        dvpp_seq_set_cmdnode_val(node, jpegdConfig->huffmanBaseTable[i]);
    }

    for (i = 0; i < HAC_SYMBOL_TABLE_SIZE; i++) {
        dvpp_seq_set_cmdnode_val(node, ((SOC_JPGD_CFG_HAC_SYMBOL_TABLE_0_REG + i * 0x4) & CMDLIST_FIX_WRITE));
        dvpp_seq_set_cmdnode_val(node, jpegdConfig->huffmanSymbolTable[i]);
    }
}

void config_jpegd_dec_start(struct CmdNode *node)
{
    union JpegdStart decStart;

    // config decode start
    decStart.u32 = 0;
    decStart.bits.decodeStart = ENABLE;
    decStart.bits.ckGtEn = 0; // 0为开启自动时钟门控，1为关闭自动时钟门控
    dvpp_seq_set_cmdnode_val(node, (SOC_JPGD_CFG_JPEG_DEC_START_REG & CMDLIST_FIX_WRITE));
    dvpp_seq_set_cmdnode_val(node, decStart.u32);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("decode start:0x%x, decodeStart bit:0x%x, clock enable bit:0x%x \n", decStart.u32,
                                 decStart.bits.decodeStart, decStart.bits.ckGtEn);
}

void config_jpegd_reg(struct CmdNode *node, JpegdConfig *jpegdConfig)
{
    config_jpegd_accuracy(node);

    config_jpegd_pixel_size(node, jpegdConfig);

    config_jpegd_addr(node, jpegdConfig);

    config_jpegd_out_type(node, jpegdConfig);

    config_jpegd_freq_scale(node);

    config_jpegd_cvdr_du_nr_rd(node);

    config_jpegd_sample_factor(node, jpegdConfig);

    config_jpegd_dri(node, jpegdConfig);

    config_jpegd_table(node, jpegdConfig);

    config_jpegd_dec_start(node);
}

void config_jepgd_next_node(struct CmdBuf *cmd_buf, struct CmdNode *node)
{
    uint64_t baseAddr = 0;
    // 配置len, 128byte对齐为了计算出来的buffer1的起始地址是128byte对齐
    uint32_t len = DVPP_ALIGN_UP(node->next->dataSize, ADDR_ALIGN_SIZE);
    DVPP_CMDLIST_JPEGD_LOG_DEBUG("cmdlist current:%u, len:%u", node->next->dataSize, len);
    dvpp_ext_set_cmdnode_val(node, SOC_CMDLST_REG_CMDLST_HW_BUF_LEN_REG & CMDLIST_FIX_WRITE);
    dvpp_ext_set_cmdnode_val(node, len >> LOW_BIT);

    // 2.计算下一个节点的地址
    baseAddr = (uint64_t)(uintptr_t)(node->next->startAddr) - (uint64_t)(uintptr_t)(cmd_buf->startAddr) +
               cmd_buf->ioStartAddr;

    // 3.1计算高位地址
    dvpp_ext_set_cmdnode_val(node, SOC_CMDLST_REG_CMDLST_HW_AXI_ADDR_H_REG & CMDLIST_FIX_WRITE);
    dvpp_ext_set_cmdnode_val(node, baseAddr >> 39); // 39~63bit

    // 3.2计算低位地址
    dvpp_ext_set_cmdnode_val(node, SOC_CMDLST_REG_CMDLST_HW_AXI_ADDR_L_REG & CMDLIST_FIX_WRITE);
    dvpp_ext_set_cmdnode_val(node, (baseAddr >> NUM_7) & 0xFFFFFFFF); // 7~38bit
}

void config_jpegd_cmdnode(struct CmdNode *node, JpegdConfig *jpegdConfig)
{
    config_jpegd_cmdnode_reverse_space(node);

    config_jpegd_reg(node, jpegdConfig);

    // cmdbuf填充padding数据保证在ddr中的长度128byte对齐
    add_padding_data(node);
}

void config_jpegd_sqelist(struct CmdBuf *cmd_buf, uint32_t sqe_idx, uint32_t blkdim)
{
    uint32_t *cur_sqe_addr = (uint32_t *)(uintptr_t)(cmd_buf->bufAddr + sqe_idx * SQE_LEN);
    uint64_t offset = (uint64_t)(uintptr_t)(cmd_buf->cmdHeadNode->startAddr) -
                      (uint64_t)(uintptr_t)(cmd_buf->startAddr);
    uint64_t startAddr = cmd_buf->ioStartAddr + offset;

    cur_sqe_addr[NUM_0] = DVPP_SQE_JPEGD; // Type=14 JPEGD任务
    cur_sqe_addr[NUM_0] |= (1 << SQE_WRCQE_OFFSET);
    cur_sqe_addr[NUM_0] |= (blkdim << SQE_BLKDIM_OFFSET); // 第0个4字节的第16-31位表示blockdim的数量
    cur_sqe_addr[NUM_3] &= ~(0xff << SQE_BLKDIM_OFFSET);
    cur_sqe_addr[NUM_3] |= (KERNEL_CREDIT << SQE_TIMEOUT_OFFSET);
    cur_sqe_addr[NUM_3] |= (0x1 << SQE_PTRMODE_OFFSET);
    cur_sqe_addr[NUM_4] = GET_LOW_ADDR((startAddr >> NUM_7));
    cur_sqe_addr[NUM_5] = startAddr >> NUM_39;
    cur_sqe_addr[NUM_6] = cmd_buf->cmdHeadNode->dataSize >> NUM_4;
}

void config_jpegd_sqe(struct CmdBuf *cmd_buf, enum dvpp_sqe_ptr_mode mode, uint32_t blkdim, struct dvpp_sqe *sqe)
{
    if (mode == DVPP_SQE_FIRST_PTR_MODE) {
        uint64_t offset = (uint64_t)(uintptr_t)(cmd_buf->cmdHeadNode->startAddr) -
                          (uint64_t)(uintptr_t)(cmd_buf->startAddr);
        uint64_t startAddr = cmd_buf->ioStartAddr + offset;

        sqe->filed[NUM_0] |= DVPP_SQE_JPEGD;
        sqe->filed[NUM_0] |= (1 << SQE_WRCQE_OFFSET);
        sqe->filed[NUM_3] &= ~(0xff << SQE_BLKDIM_OFFSET);
        sqe->filed[NUM_3] |= (KERNEL_CREDIT << SQE_BLKDIM_OFFSET);
        sqe->filed[NUM_3] &= ~(0x1 << SQE_PTRMODE_OFFSET);
        // Start address [38:7] of the frame (in 128 bytes boundary) for configure buffer.
        sqe->filed[NUM_4] = GET_LOW_ADDR((startAddr >> NUM_7));
        // Start address [63:39] of the frame (in 128 bytes boundary) for configure buffer.
        sqe->filed[NUM_5] = startAddr >> NUM_39;
        sqe->filed[NUM_6] = cmd_buf->cmdHeadNode->dataSize >> NUM_4;
    } else {
        // TD 二级链表模式(多block dim)
        sqe->filed[NUM_0] = DVPP_SQE_JPEGD; // Type=14 JPEGD任务
        sqe->filed[NUM_0] |= (1 << SQE_WRCQE_OFFSET);
        sqe->filed[NUM_0] |= (blkdim << SQE_BLKDIM_OFFSET); // 第0个4字节的第16-31位表示blockdim的数量
        sqe->filed[NUM_3] &= ~(0xff << SQE_BLKDIM_OFFSET);
        sqe->filed[NUM_3] |= (KERNEL_CREDIT << SQE_TIMEOUT_OFFSET); // 第3个4字节的第16-23位表示超时时间
        sqe->filed[NUM_3] |= (0x1 << SQE_PTRMODE_OFFSET); // 第3个4字节的第24位表示链表模式，1表示二级
        sqe->filed[NUM_4] = GET_LOW_ADDR(cmd_buf->ioAddr); // 第4个4字节存放二级链表的低32位地址
        sqe->filed[NUM_5] = GET_HIGH_ADDR(cmd_buf->ioAddr); // 第5个4字节的低16位存放二级链表的高16位地址
        sqe->filed[NUM_5] &= 0x1FFFF;                       // 第5个4字节的17~30bit是保留字段
        sqe->filed[NUM_5] |= (0x1 << SQE_ADDRTYPE_OFFSET); // 第5个4字节的最高位表示地址类型 1:虚拟地址 0:物理地址
    }
}