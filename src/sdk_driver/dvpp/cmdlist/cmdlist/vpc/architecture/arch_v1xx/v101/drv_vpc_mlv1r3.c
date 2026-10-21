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

#include "drv_arch_comm.h"
#include "dvpp_cmdlist_define.h"
#include "vpc_reg.h"
#include "vpc_yuvscale_milan_v1r3_reg_c_union_define.h"
#include "vpc_pipe_milan_v1r3_filter_reg_c_union_define.h"
#include "pipeline_module.h"
#include "dvpp_tlv_types.h"
#include "dvpp_cmdlist_log.h"

static void drv_vpc_config_cmdnode_dump_reg(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx)
{
    const uint32_t CMDLST_HW_AXI_ADDR_DUMP_H = VPC_CMDLST_OFFSET_ADDR + 0x028;
    const uint32_t CMDLST_HW_AXI_ADDR_DUMP_L = VPC_CMDLST_OFFSET_ADDR + 0x02c;
    const uint32_t VPC_FRM_CYCLE_CNT = VPC_TOP_OFFSET_ADDR + 0x0bc;

    struct DumpReg *dump = (struct DumpReg *)(dvpp_get_vpu_mod_val(decoder, node_idx, VPU_VPC_TYPE_PIPE_DUMPREG));
    if ((dump == NULL) || (dump->en == 0)) {
        DVPP_CMDLIST_VPC_LOG_DEBUG("dump reg disable.");
        return;
    }

    node->isNeedWrBack = 1;
    node->wrBackSize = dump->readSize;
    node->wrBackValAddr = dump->readAddr;
    node->len1 = 16U;
    node->wrBackRegAddr = VPC_FRM_CYCLE_CNT;

    dvpp_ext_set_cmdnode_val(node, CMDLST_HW_AXI_ADDR_DUMP_H);
    dvpp_ext_set_cmdnode_val(node, node->wrBackValAddr >> NUM_39);
    dvpp_ext_set_cmdnode_val(node, CMDLST_HW_AXI_ADDR_DUMP_L);
    dvpp_ext_set_cmdnode_val(node, (node->wrBackValAddr >> NUM_7) & 0xFFFFFFFF);
    DVPP_CMDLIST_VPC_LOG_DEBUG("dump reg size %u\n", dump->readSize);
}

static void drv_vpc_config_cmdnode_mosaic(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx)
{
    U_VPC_REGION_SIZE size;
    U_VPC_MOSAIC_BLOCK blockSize;
    U_VPC_MOSAIC_HPOS hPos;
    U_VPC_MOSAIC_VPOS vPos;
    uint32_t control = 0;
    uint32_t i = 0;

    struct Mosaic *mosaic = (struct Mosaic *)(dvpp_get_vpu_mod_val(decoder, node_idx, VPU_VPC_TYPE_PIPE_MOSAIC));
    if ((mosaic == NULL) || (mosaic_en(mosaic) == 0)) {
        DVPP_CMDLIST_VPC_LOG_DEBUG("mosaic disable.");
        dvpp_seq_set_cmdnode_val(node, REG_REGION_MOSAIC_CONCTRL);
        dvpp_seq_set_cmdnode_val(node, control);
        return;
    }

    size.bits.in_width = mosaic->inWidth - 1;
    size.bits.in_height = mosaic->inHeight - 1;
    dvpp_seq_set_cmdnode_val(node, REG_REGION_SIZE);
    dvpp_seq_set_cmdnode_val(node, size.u32);

    blockSize.bits.mosaic_block = mosaic->blockSize;
    dvpp_seq_set_cmdnode_val(node, REG_REGION_MOSAIC_BLOCK);
    dvpp_seq_set_cmdnode_val(node, blockSize.u32);

    for (i = 0; i < MAX_MOSAIC_NUM; i++) {
        if (mosaic->mosaicRegion[i].en != 0) {
            control |= (0x1U << i);
            hPos.bits.mosaic_hmin = mosaic->mosaicRegion[i].hmin;
            hPos.bits.mosaic_hmax = mosaic->mosaicRegion[i].hmax;
            vPos.bits.mosaic_vmin = mosaic->mosaicRegion[i].vmin;
            vPos.bits.mosaic_vmax = mosaic->mosaicRegion[i].vmax;
            // 配置马赛克区域大小，第1, 2 ,3个区域在第0个区域后偏移
            dvpp_seq_set_cmdnode_val(node, REG_REGION_MOSAIC_HPOS + (0x10 * i));
            dvpp_seq_set_cmdnode_val(node, hPos.u32);
            dvpp_seq_set_cmdnode_val(node, REG_REGION_MOSAIC_VPOS + (0x10 * i));
            dvpp_seq_set_cmdnode_val(node, vPos.u32);
        }
    }
    dvpp_seq_set_cmdnode_val(node, REG_REGION_MOSAIC_CONCTRL);
    dvpp_seq_set_cmdnode_val(node, control);
}

static void drv_vpc_config_cmdnode_cover(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx)
{
    U_VPC_REGION_SIZE size;
    U_VPC_COVER_VALUE coverValue;
    U_VPC_COVER_POS0 coverPos0;
    U_VPC_COVER_POS1 coverPos1;
    U_VPC_COVER_POS2 coverPos2;
    U_VPC_COVER_POS3 coverPos3;
    uint32_t control = 0;
    uint32_t i = 0;
    uint32_t value = 0;

    struct Cover *cover = (struct Cover *)(dvpp_get_vpu_mod_val(decoder, node_idx, VPU_VPC_TYPE_PIPE_COVER));
    if ((cover == NULL) || (cover_en(cover) == 0)) {
        DVPP_CMDLIST_VPC_LOG_DEBUG("cover disable.");
        dvpp_seq_set_cmdnode_val(node, REG_REGION_COVER_CONCTRL);
        dvpp_seq_set_cmdnode_val(node, control);
        return;
    }

    size.bits.in_width = cover->inWidth - 1;
    size.bits.in_height = cover->inHeight - 1;
    dvpp_seq_set_cmdnode_val(node, REG_REGION_SIZE);
    dvpp_seq_set_cmdnode_val(node, size.u32);

    for (i = 0; i < MAX_COVER_NUM; i++) {
        if (cover->coverRegion[i].en != 0) {
            control |= (0x1U << i);

            coverValue.bits.cover_value = cover->coverRegion[i].coverValue;
            // 配置cover区域颜色，第1, 2, 3, 4, 5, 6, 7个区域在第0个区域后偏移，偏移量0x40
            dvpp_seq_set_cmdnode_val(node, REG_REGION_COVER_VALUE0 + (0x40 * i));
            dvpp_seq_set_cmdnode_val(node, coverValue.u32);

            coverPos0.bits.cover_x0 = (uint32_t)(cover->coverRegion[i].points[NUM_0].x);
            coverPos0.bits.cover_y0 = (uint32_t)(cover->coverRegion[i].points[NUM_0].y);
            coverPos1.bits.cover_x1 = (uint32_t)(cover->coverRegion[i].points[NUM_1].x);
            coverPos1.bits.cover_y1 = (uint32_t)(cover->coverRegion[i].points[NUM_1].y);
            coverPos2.bits.cover_x2 = (uint32_t)(cover->coverRegion[i].points[NUM_2].x);
            coverPos2.bits.cover_y2 = (uint32_t)(cover->coverRegion[i].points[NUM_2].y);
            coverPos3.bits.cover_x3 = (uint32_t)(cover->coverRegion[i].points[NUM_3].x);
            coverPos3.bits.cover_y3 = (uint32_t)(cover->coverRegion[i].points[NUM_3].y);

            // cmdlist芯片协议，10个地址连续的寄存器，可以用一个偏移地址表示，偏移地址对应value高16置9
            value = (REG_REGION_COVER_POS0_0 + (0x40 * i)) | (0x9 << SET_MULTI_REG_OFFSET);
            dvpp_seq_set_cmdnode_val(node, value);
            dvpp_seq_set_cmdnode_val(node, coverPos0.u32);
            dvpp_seq_set_cmdnode_val(node, coverPos1.u32);
            dvpp_seq_set_cmdnode_val(node, coverPos2.u32);
            dvpp_seq_set_cmdnode_val(node, coverPos3.u32);
            dvpp_seq_set_cmdnode_val(node, (uint32_t)(cover->coverRegion[i].coverkk01));
            dvpp_seq_set_cmdnode_val(node, (uint32_t)(cover->coverRegion[i].coverkk02));
            dvpp_seq_set_cmdnode_val(node, (uint32_t)(cover->coverRegion[i].coverkk03));
            dvpp_seq_set_cmdnode_val(node, (uint32_t)(cover->coverRegion[i].coverkk12));
            dvpp_seq_set_cmdnode_val(node, (uint32_t)(cover->coverRegion[i].coverkk13));
            dvpp_seq_set_cmdnode_val(node, (uint32_t)(cover->coverRegion[i].coverkk23));
        }
    }
    dvpp_seq_set_cmdnode_val(node, REG_REGION_COVER_CONCTRL);
    dvpp_seq_set_cmdnode_val(node, control);
}

static void drv_vpc_config_cmdnode_osd_clut(struct Osd *osd, struct CmdNode *node)
{
    uint32_t i = 0;
    uint32_t value = 0;
    value = (REG_REGION_OSD_CLUT2_LAB_0 | (0x3 << SET_MULTI_REG_OFFSET));
    dvpp_seq_set_cmdnode_val(node, value);
    for (i = 0; i < NUM_4; i++) {
        dvpp_seq_set_cmdnode_val(node, osd->clut[i]);
    }

    value = (REG_REGION_OSD_CLUT4_LAB_0 | (0xf << SET_MULTI_REG_OFFSET));
    dvpp_seq_set_cmdnode_val(node, value);
    for (i = 0; i < NUM_16; i++) {
        dvpp_seq_set_cmdnode_val(node, osd->clut[i]);
    }
}

static void drv_vpc_config_cmdnode_osd_csc(struct Osd *osd, struct CmdNode *node, uint32_t idx)
{
    uint32_t i = 0;
    uint32_t value = (REG_REGION_OSD_CSC_0 | (0x17 << SET_MULTI_REG_OFFSET));
    dvpp_seq_set_cmdnode_val(node, value);
    for (i = 0; i < NUM_24; i++) {
        dvpp_seq_set_cmdnode_val(node, osd->osdRegion[idx].osdCsc.coeff[i]);
    }
}

static void drv_vpc_config_cmdnode_osd(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx)
{
    uint8_t cscEn = 0;
    U_VPC_REGION_SIZE size;
    U_VPC_OSD_SIZE osdSize;
    U_VPC_OSD_HPOS osdHpos;
    U_VPC_OSD_VPOS osdVpos;
    U_VPC_OSD_CTRL_MODE osdCtrlMode;
    U_VPC_OSD_REV_MODE osdRevMode;
    U_VPC_OSD_CSC_CTRL cscCtrl;
    uint32_t value;
    uint32_t control = 0;
    uint32_t i = 0;

    struct Osd *osd = (struct Osd *)(dvpp_get_vpu_mod_val(decoder, node_idx, VPU_VPC_TYPE_PIPE_OSD));
    if ((osd == NULL) || (osd_en(osd) == 0)) {
        DVPP_CMDLIST_VPC_LOG_DEBUG("osd disable.");
        dvpp_seq_set_cmdnode_val(node, REG_REGION_OSD_CONCTRL);
        dvpp_seq_set_cmdnode_val(node, control);
        return;
    }

    size.bits.in_width = osd->inWidth - 1;
    size.bits.in_height = osd->inHeight - 1;
    dvpp_seq_set_cmdnode_val(node, REG_REGION_SIZE);
    dvpp_seq_set_cmdnode_val(node, size.u32);

    for (i = 0; i < MAX_OSD_NUM; i++) {
        if (osd->osdRegion[i].en != 0) {
            control |= (0x1U << i);
            osdSize.bits.osd_img_height = osd->osdRegion[i].osdImgHeight - 1;
            osdSize.bits.osd_img_width = osd->osdRegion[i].osdImgWidth - 1;
            osdHpos.bits.osd_hmax = osd->osdRegion[i].osdHmax - 1;
            osdHpos.bits.osd_hmin = osd->osdRegion[i].osdHmin;
            osdVpos.bits.osd_vmax = osd->osdRegion[i].osdVmax - 1;
            osdVpos.bits.osd_vmin = osd->osdRegion[i].osdVmin;
            osdCtrlMode.bits.osd_global_alpha = NUM_255;
            osdCtrlMode.bits.osd_palpha_en = 1;    // alpha 默认使能
            osdCtrlMode.bits.osd_bit_ext_mode = 0; // 低位补0
            osdCtrlMode.bits.osd_alpha0 = osd->osdRegion[i].osdAlpha0;
            osdCtrlMode.bits.osd_alpha1 = osd->osdRegion[i].osdAlpha1;
            osdRevMode.bits.osd_alpha_rev = osd->osdRegion[i].osdAlphaRev;
            osdRevMode.bits.osd_rgb_rev = osd->osdRegion[i].osdRgbRev;
            // cmdlist芯片协议，9个地址连续的寄存器，可以用一个偏移地址表示，偏移地址对应value高16置8
            value = (REG_REGION_OSD_ADDR_L + (0x40 * i)) | (0x8 << SET_MULTI_REG_OFFSET);
            dvpp_seq_set_cmdnode_val(node, value);
            dvpp_seq_set_cmdnode_val(node, osd->osdRegion[i].osdImgAddrL);
            dvpp_seq_set_cmdnode_val(node, osd->osdRegion[i].osdImgAddrH);
            dvpp_seq_set_cmdnode_val(node, osd->osdRegion[i].osdImgStride);
            dvpp_seq_set_cmdnode_val(node, osdSize.u32);
            dvpp_seq_set_cmdnode_val(node, osdHpos.u32);
            dvpp_seq_set_cmdnode_val(node, osdVpos.u32);
            dvpp_seq_set_cmdnode_val(node, osd->osdRegion[i].osdFormat);
            dvpp_seq_set_cmdnode_val(node, osdCtrlMode.u32);
            dvpp_seq_set_cmdnode_val(node, osdRevMode.u32);

            if (osd->osdRegion[i].osdCsc.en != 0) {
                cscEn = 1;
                drv_vpc_config_cmdnode_osd_csc(osd, node, i);
            }
        }
    }

    cscCtrl.bits.osd_csc_en = cscEn;
    dvpp_seq_set_cmdnode_val(node, REG_REGION_OSD_CSC_CTRL);
    dvpp_seq_set_cmdnode_val(node, cscCtrl.u32);

    drv_vpc_config_cmdnode_osd_clut(osd, node);
    dvpp_seq_set_cmdnode_val(node, REG_REGION_OSD_CONCTRL);
    dvpp_seq_set_cmdnode_val(node, control);
    return;
}

static void drv_vpc_config_cmdnode_rotate(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx)
{
    U_VPC_ROTATE_SRC_SIZE rotateSize;
    U_VPC_ROTATE_MODE rotateMode;
    uint32_t value = 0;
    uint32_t i = 0;

    struct Rotate *rotate = (struct Rotate *)(dvpp_get_vpu_mod_val(decoder, node_idx, VPU_VPC_TYPE_PIPE_ROTATE));
    if ((rotate == NULL) || (rotate->en == 0)) {
        DVPP_CMDLIST_VPC_LOG_DEBUG("rotate disable.");
        dvpp_seq_set_cmdnode_val(node, REG_ROTATE_MODE);
        dvpp_seq_set_cmdnode_val(node, 0);
        return;
    }

    rotateSize.bits.src_width = rotate->inWidth;
    rotateSize.bits.src_height = rotate->inHeight;

    rotateMode.bits.rot_en = 1; // 旋转功能打开
    rotateMode.bits.rot_dir = rotate->rotDir;

    // cmdlist芯片协议，20个地址连续的寄存器，可以用一个偏移地址表示，偏移地址对应value高16置19
    value = REG_ROTATE_SRC_SIZE | (0x13 << SET_MULTI_REG_OFFSET);
    dvpp_seq_set_cmdnode_val(node, value);
    dvpp_seq_set_cmdnode_val(node, rotateSize.u32);
    dvpp_seq_set_cmdnode_val(node, rotateMode.u32);
    // planar0/1/2基地址的高（低）地址可以都配置为输入、输出图片的（高）低地址
    for (i = 0; i < NUM_3; i++) {
        dvpp_seq_set_cmdnode_val(node, rotate->inPicAddrH[i]);
        dvpp_seq_set_cmdnode_val(node, rotate->inPicAddrL[i]);
        dvpp_seq_set_cmdnode_val(node, rotate->inPicStride[i]);
    }
    for (i = 0; i < NUM_3; i++) {
        dvpp_seq_set_cmdnode_val(node, rotate->outPicAddrH[i]);
        dvpp_seq_set_cmdnode_val(node, rotate->outPicAddrL[i]);
        dvpp_seq_set_cmdnode_val(node, rotate->outPicStride[i]);
    }
    return;
}

static void drv_vpc_config_cmdnode_yuvscaler_4tap_coeff(struct CmdNode *node, struct YuvScale *yuvScale,
                                                        uint32_t regOffset)
{
    U_COEFF_V_Y0 verticalY0;
    U_COEFF_V_Y1 verticalY1;
    U_COEFF_V_UV0 verticalUv0;
    U_COEFF_V_UV1 verticalUv1;
    uint32_t i = 0;
    uint32_t startAddr = 0;
    uint32_t addrWithOffset = 0;
    for (i = 0; i < NUM_17; i++) { // 只需要配17个寄存器，后面的与前面对称
        startAddr = REG_NEW_SCALER_COEFF_V_Y0 + regOffset + i * 0x10;
        addrWithOffset = startAddr | (0x1UL << SET_MULTI_REG_OFFSET);
        dvpp_seq_set_cmdnode_val(node, addrWithOffset);
        verticalY0.u32 = 0;
        verticalY0.bits.vcoeff0y = 0;
        verticalY0.bits.vcoeff1y = (uint16_t)(yuvScale->coeff.scale[1].taps4[i * NUM_4]);
        verticalY0.bits.vcoeff2y = (uint16_t)(yuvScale->coeff.scale[1].taps4[(i * NUM_4) + NUM_1]);
        dvpp_seq_set_cmdnode_val(node, verticalY0.u32);
        verticalY1.u32 = 0;
        verticalY1.bits.vcoeff3y = (uint16_t)(yuvScale->coeff.scale[1].taps4[(i * NUM_4) + NUM_2]);
        verticalY1.bits.vcoeff4y = (uint16_t)(yuvScale->coeff.scale[1].taps4[(i * NUM_4) + NUM_3]);
        verticalY1.bits.vcoeff5y = 0;
        dvpp_seq_set_cmdnode_val(node, verticalY1.u32);
        // uv_v_coeff 4 tap
        startAddr = REG_NEW_SCALER_COEFF_V_UV0 + regOffset + i * 0x10;
        addrWithOffset = startAddr | (0x1UL << SET_MULTI_REG_OFFSET);
        dvpp_seq_set_cmdnode_val(node, addrWithOffset);
        verticalUv0.u32 = 0;
        verticalUv0.bits.vcoeff0uv = 0;
        verticalUv0.bits.vcoeff1uv = (uint16_t)(yuvScale->coeff.scale[1].taps4[i * NUM_4]);
        verticalUv0.bits.vcoeff2uv = (uint16_t)(yuvScale->coeff.scale[1].taps4[(i * NUM_4) + NUM_1]);
        dvpp_seq_set_cmdnode_val(node, verticalUv0.u32);
        verticalUv1.u32 = 0;
        verticalUv1.bits.vcoeff3uv = (uint16_t)(yuvScale->coeff.scale[1].taps4[(i * NUM_4) + NUM_2]);
        verticalUv1.bits.vcoeff4uv = (uint16_t)(yuvScale->coeff.scale[1].taps4[(i * NUM_4) + NUM_3]);
        verticalUv1.bits.vcoeff5uv = 0;
        dvpp_seq_set_cmdnode_val(node, verticalUv1.u32);
    }
}

static void drv_vpc_config_cmdnode_yuvscaler_6tap_coeff(struct CmdNode *node, struct YuvScale *yuvScale,
                                                        uint32_t regOffset)
{
    U_COEFF_V_UV0 verticalUv0;
    U_COEFF_V_UV1 verticalUv1;
    U_COEFF_V_Y0 verticalY0;
    U_COEFF_V_Y1 verticalY1;
    uint32_t i = 0;
    uint32_t startAddr = 0;
    uint32_t addrWithOffset = 0;
    for (i = 0; i < NUM_17; i++) { // 只需要配17个寄存器，后面的与前面对称
        startAddr = REG_NEW_SCALER_COEFF_V_Y0 + regOffset + i * 0x10;
        addrWithOffset = startAddr | (0x1UL << SET_MULTI_REG_OFFSET);
        dvpp_seq_set_cmdnode_val(node, addrWithOffset);
        verticalY0.u32 = 0;
        verticalY0.bits.vcoeff0y = (uint16_t)(yuvScale->coeff.scale[1].taps6[i * NUM_6]);
        verticalY0.bits.vcoeff1y = (uint16_t)(yuvScale->coeff.scale[1].taps6[(i * NUM_6) + NUM_1]);
        verticalY0.bits.vcoeff2y = (uint16_t)(yuvScale->coeff.scale[1].taps6[(i * NUM_6) + NUM_2]);
        dvpp_seq_set_cmdnode_val(node, verticalY0.u32);
        verticalY1.u32 = 0;
        verticalY1.bits.vcoeff3y = (uint16_t)(yuvScale->coeff.scale[1].taps6[(i * NUM_6) + NUM_3]);
        verticalY1.bits.vcoeff4y = (uint16_t)(yuvScale->coeff.scale[1].taps6[(i * NUM_6) + NUM_4]);
        verticalY1.bits.vcoeff5y = (uint16_t)(yuvScale->coeff.scale[1].taps6[(i * NUM_6) + NUM_5]);
        dvpp_seq_set_cmdnode_val(node, verticalY1.u32);

        // uv_v_coeff 6 tap
        startAddr = REG_NEW_SCALER_COEFF_V_UV0 + regOffset + i * 0x10;
        addrWithOffset = startAddr | (0x1UL << SET_MULTI_REG_OFFSET);
        dvpp_seq_set_cmdnode_val(node, addrWithOffset);
        verticalUv0.u32 = 0;
        verticalUv0.bits.vcoeff0uv = (uint16_t)(yuvScale->coeff.scale[1].taps6[i * NUM_6]);
        verticalUv0.bits.vcoeff1uv = (uint16_t)(yuvScale->coeff.scale[1].taps6[(i * NUM_6) + NUM_1]);
        verticalUv0.bits.vcoeff2uv = (uint16_t)(yuvScale->coeff.scale[1].taps6[(i * NUM_6) + NUM_2]);
        dvpp_seq_set_cmdnode_val(node, verticalUv0.u32);
        verticalUv1.u32 = 0;
        verticalUv1.bits.vcoeff3uv = (uint16_t)(yuvScale->coeff.scale[1].taps6[(i * NUM_6) + NUM_3]);
        verticalUv1.bits.vcoeff4uv = (uint16_t)(yuvScale->coeff.scale[1].taps6[(i * NUM_6) + NUM_4]);
        verticalUv1.bits.vcoeff5uv = (uint16_t)(yuvScale->coeff.scale[1].taps6[(i * NUM_6) + NUM_5]);
        dvpp_seq_set_cmdnode_val(node, verticalUv1.u32);
    }
}

static void drv_vpc_config_cmdnode_yuvscaler_coeff(struct CmdNode *node, struct YuvScale *yuvScale, uint32_t regOffset)
{
    U_COEFF_H_Y0 horizonY0;
    U_COEFF_H_Y1 horizonY1;
    U_COEFF_H_Y2 horizonY2;
    U_COEFF_H_UV0 horizonUv0;
    U_COEFF_H_UV1 horizonUv1;
    uint32_t i = 0;
    uint32_t startAddr = 0;
    uint32_t addrWithOffset = 0;
    uint8_t isHalfLineMode = 0;
    for (i = 0; i < NUM_17; i++) { // 只需要配17个寄存器，后面的与前面对称
        startAddr = REG_NEW_SCALER_COEFF_H_Y0 + regOffset + i * 0x10;
        addrWithOffset = startAddr | (0x2UL << SET_MULTI_REG_OFFSET);
        // y_h_coeff 8 tap
        dvpp_seq_set_cmdnode_val(node, addrWithOffset);
        horizonY0.u32 = 0;
        horizonY0.bits.hcoeff0y = (uint16_t)(yuvScale->coeff.scale[0].taps8[i * NUM_8]);
        horizonY0.bits.hcoeff1y = (uint16_t)(yuvScale->coeff.scale[0].taps8[(i * NUM_8) + NUM_1]);
        horizonY0.bits.hcoeff2y = (uint16_t)(yuvScale->coeff.scale[0].taps8[(i * NUM_8) + NUM_2]);
        dvpp_seq_set_cmdnode_val(node, horizonY0.u32);
        horizonY1.u32 = 0;
        horizonY1.bits.hcoeff3y = (uint16_t)(yuvScale->coeff.scale[0].taps8[(i * NUM_8) + NUM_3]);
        horizonY1.bits.hcoeff4y = (uint16_t)(yuvScale->coeff.scale[0].taps8[(i * NUM_8) + NUM_4]);
        horizonY1.bits.hcoeff5y = (uint16_t)(yuvScale->coeff.scale[0].taps8[(i * NUM_8) + NUM_5]);
        dvpp_seq_set_cmdnode_val(node, horizonY1.u32);
        horizonY2.u32 = 0;
        horizonY2.bits.hcoeff6y = (uint16_t)(yuvScale->coeff.scale[0].taps8[(i * NUM_8) + NUM_6]);
        horizonY2.bits.hcoeff7y = (uint16_t)(yuvScale->coeff.scale[0].taps8[(i * NUM_8) + NUM_7]);
        dvpp_seq_set_cmdnode_val(node, horizonY2.u32);

        // uv_h_coeff 4 tap
        startAddr = REG_NEW_SCALER_COEFF_H_UV0 + regOffset + i * 0x10;
        addrWithOffset = startAddr | (0x1UL << SET_MULTI_REG_OFFSET);
        dvpp_seq_set_cmdnode_val(node, addrWithOffset);
        horizonUv0.u32 = 0;
        horizonUv0.bits.hcoeff0uv = 0;
        horizonUv0.bits.hcoeff1uv = (uint16_t)(yuvScale->coeff.scale[0].taps4[i * NUM_4]);
        horizonUv0.bits.hcoeff2uv = (uint16_t)(yuvScale->coeff.scale[0].taps4[(i * NUM_4) + NUM_1]);
        dvpp_seq_set_cmdnode_val(node, horizonUv0.u32);
        horizonUv1.u32 = 0;
        horizonUv1.bits.hcoeff3uv = (uint16_t)(yuvScale->coeff.scale[0].taps4[(i * NUM_4) + NUM_2]);
        horizonUv1.bits.hcoeff4uv = (uint16_t)(yuvScale->coeff.scale[0].taps4[(i * NUM_4) + NUM_3]);
        horizonUv1.bits.hcoeff5uv = 0;
        dvpp_seq_set_cmdnode_val(node, horizonUv1.u32);
    }

    isHalfLineMode = drv_vpc_get_yuvscaler_half_line_mode(yuvScale->ivinc >> SCALER_BIT_MOVE_TO_OLD_INC,
                                                          yuvScale->outWidth);
    if (isHalfLineMode) { // 6 tap
        drv_vpc_config_cmdnode_yuvscaler_6tap_coeff(node, yuvScale, regOffset);
    } else { // 4tap
        drv_vpc_config_cmdnode_yuvscaler_4tap_coeff(node, yuvScale, regOffset);
    }
}

static void drv_vpc_config_cmdnode_yuvscaler(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx,
                                             uint32_t chn)
{
    U_BYPASS regBypass;
    U_FORMAT uFormat;
    uint32_t regOffset = 0;
    uint32_t startAddr = 0;
    uint32_t addrWithOffset = 0;
    struct YuvScale *yuvscale = NULL;
    regBypass.u32 = 0;
    uFormat.u32 = 0;
    if (chn == MAIN_CHN) {
        yuvscale = (struct YuvScale *)(dvpp_get_vpu_mod_val(decoder, node_idx, VPU_VPC_TYPE_PIPE_YUVSCALER_CHN0));
    } else if (chn == SUB_CHN_1) {
        yuvscale = (struct YuvScale *)(dvpp_get_vpu_mod_val(decoder, node_idx, VPU_VPC_TYPE_PIPE_YUVSCALER_CHN1));
        regOffset = 0x1000; // yuvscale2与1的地址偏移
    }
    if ((yuvscale == NULL) || (yuvscaler_en(yuvscale) == 0)) {
        DVPP_CMDLIST_VPC_LOG_DEBUG("chn%u yuvscaler disable.", chn);
        regBypass.u32 = 0;
        regBypass.bits.bypass = NUM_3;
        dvpp_seq_set_cmdnode_val(node, REG_NEW_SCALER_BYPASS + regOffset);
        dvpp_seq_set_cmdnode_val(node, regBypass.u32);
        return;
    }

    startAddr = REG_NEW_SCALER_IHLEFT_INT + regOffset;
    addrWithOffset = startAddr | (0x7UL << SET_MULTI_REG_OFFSET);
    dvpp_seq_set_cmdnode_val(node, addrWithOffset);
    dvpp_seq_set_cmdnode_val(node, yuvscale->ihleftInt);
    dvpp_seq_set_cmdnode_val(node, yuvscale->ihleftDec);
    dvpp_seq_set_cmdnode_val(node, yuvscale->ihrightInt);
    dvpp_seq_set_cmdnode_val(node, yuvscale->ihrightDec);
    dvpp_seq_set_cmdnode_val(node, yuvscale->ivtopInt);
    dvpp_seq_set_cmdnode_val(node, yuvscale->ivtopDec);
    dvpp_seq_set_cmdnode_val(node, yuvscale->ivbotInt);
    dvpp_seq_set_cmdnode_val(node, yuvscale->ivbotDec);

    startAddr = REG_NEW_SCALER_IHINC + regOffset;
    addrWithOffset = startAddr | (0x2UL << SET_MULTI_REG_OFFSET);
    dvpp_seq_set_cmdnode_val(node, addrWithOffset);
    dvpp_seq_set_cmdnode_val(node, (uint32_t)(yuvscale->ihinc));
    dvpp_seq_set_cmdnode_val(node, (uint32_t)(yuvscale->ivinc));
    regBypass.u32 = 0;
    regBypass.bits.bypass = yuvscale->bypass;
    dvpp_seq_set_cmdnode_val(node, regBypass.u32);

    startAddr = REG_NEW_SCALER_IWIDTH + regOffset;
    addrWithOffset = startAddr | (0x3UL << SET_MULTI_REG_OFFSET);
    dvpp_seq_set_cmdnode_val(node, addrWithOffset);
    dvpp_seq_set_cmdnode_val(node, yuvscale->inWidth);
    dvpp_seq_set_cmdnode_val(node, yuvscale->inHeight);
    dvpp_seq_set_cmdnode_val(node, yuvscale->outWidth);
    dvpp_seq_set_cmdnode_val(node, yuvscale->outHeight);

    uFormat.bits.phase_mode = 0;
    dvpp_seq_set_cmdnode_val(node, REG_NEW_SCALER_FORMAT + regOffset);
    dvpp_seq_set_cmdnode_val(node, uFormat.u32);

    drv_vpc_config_cmdnode_yuvscaler_coeff(node, yuvscale, regOffset);
}

static void drv_vpc_config_cmdnode_writeback_rear(struct CmdNode *node)
{
    uint32_t offset_addr = 0;
    uint32_t burstSize = 0;
    const uint32_t burstMaxSize = sizeof(uint32_t) * 256;
    uint32_t i = 0;
    uint32_t j = 0;
    if (node->isNeedWrBack) {
        // 配置读操作的寄存器地址，配置完后硬件就会启动
        for (i = 0; i < node->wrBackSize / burstMaxSize; ++i) {
            dvpp_seq_set_cmdnode_val(node, (node->wrBackRegAddr + offset_addr) | 0x1U | (0xffU << NUM_24));
            offset_addr += 0x400;
            node->len1 += sizeof(uint32_t);
        }

        burstSize = node->wrBackSize % burstMaxSize;
        if (burstSize != 0) {
            ++i;
            burstSize = burstSize / sizeof(uint32_t) - 1;
            dvpp_seq_set_cmdnode_val(node, (node->wrBackRegAddr + offset_addr) | 0x1U | (burstSize << NUM_24));
            node->len1 += sizeof(uint32_t);
        }

        node->len1 = DVPP_ALIGN_UP(node->len1, ADDR_ALIGN_SIZE);
        for (j = i; j < node->len1 / sizeof(uint32_t); ++j) {
            dvpp_seq_set_cmdnode_val(node, CMDLST_PADDING_DATA);
        }
    }
}

void drv_vpc_config_cmdnode_filter_control(struct CmdNode *node, struct Filter *filter)
{
    U_FILTER_CTRL0 ctr0;
    ctr0.bits.filter_en = filter->en;
    ctr0.bits.filter_format = 0; // 此处对标Ascend910_73，统一设置为0
    ctr0.bits.filter_mode_sel = filter->mode;

    dvpp_seq_set_cmdnode_val(node, REG_FILTER_CONTROL0);
    dvpp_seq_set_cmdnode_val(node, ctr0.u32);

    dvpp_seq_set_cmdnode_val(node, REG_FILTER_SORT_CONTROL);
    dvpp_seq_set_cmdnode_val(node, filter->sortMode);

    dvpp_seq_set_cmdnode_val(node, REG_FILTER_BLUR_CTRL);
    dvpp_seq_set_cmdnode_val(node, filter->blurMode);
}

void drv_vpc_cmdnode_clear_interrupt(struct CmdNode *node)
{
    dvpp_ext_set_cmdnode_val(node, REG_VPC_NORM_CVDR_INT_CLR1);
    dvpp_ext_set_cmdnode_val(node, 0xffffffff);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_NORM_CVDR_INT_CLR2);
    dvpp_ext_set_cmdnode_val(node, 0xffffffff);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_NORM_CVDR_INT_CLR3);
    dvpp_ext_set_cmdnode_val(node, 0xffffffff);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_NORM_CVDR_INT_CLR4);
    dvpp_ext_set_cmdnode_val(node, 0xffffffff);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_ERR_INT_CLR);
    dvpp_ext_set_cmdnode_val(node, 0xffffffff);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_NORM_INT_CLR2);
    dvpp_ext_set_cmdnode_val(node, 0xffffffff);
}

void drv_vpc_cmdnode_config_interrupt(struct CmdNode *node)
{
    dvpp_ext_set_cmdnode_val(node, REG_VPC_ERR_INT_MASK);
    dvpp_ext_set_cmdnode_val(node, node->headInfo.errMask);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_NORM_INT_MASK2);
    dvpp_ext_set_cmdnode_val(node, 0xFFFFFFFF);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_CMDLIST_IN_INT_CTRL);
    dvpp_ext_set_cmdnode_val(node, 0x2);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_ERR_OUT_DMUXEN);
    dvpp_ext_set_cmdnode_val(node, 0xC);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_EOF_INT1_MERGE_ENABLE_RDMA_CVDR);
    dvpp_ext_set_cmdnode_val(node, node->headInfo.cvdrIntMerge);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_EOF_INT1_MERGE_ENABLE_PIPE);
    dvpp_ext_set_cmdnode_val(node, node->headInfo.pipeIntMerge);
}

static void drv_vpc_config_cmdnode_rdma(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx)
{
    struct Rdma *rdma = NULL;
    rdma = (struct Rdma *)(dvpp_get_vpu_mod_val(decoder, node_idx, VPU_VPC_TYPE_CHNL_RDMA));
    if (rdma == NULL) {
        DVPP_CMDLIST_VPC_LOG_DEBUG("rdma disable.");
        return;
    }

    dvpp_seq_set_cmdnode_val(node, REG_RDMA_LUMA_HEAD_ADDR_L);
    dvpp_seq_set_cmdnode_val(node, rdma->srcyhAddrL);
    dvpp_seq_set_cmdnode_val(node, REG_RDMA_CHROMA_HEAD_ADDR_L);
    dvpp_seq_set_cmdnode_val(node, rdma->srcchAddrL);
    dvpp_seq_set_cmdnode_val(node, REG_RDMA_LUMA_HEAD_STRIDE);
    dvpp_seq_set_cmdnode_val(node, rdma->srcyhStride);
    dvpp_seq_set_cmdnode_val(node, REG_RDMA_LUMA_PAYLOAD_ADDR_L);
    dvpp_seq_set_cmdnode_val(node, rdma->srcyAddrL);
    dvpp_seq_set_cmdnode_val(node, REG_RDMA_CHROMA_PAYLOAD_ADDR_L);
    dvpp_seq_set_cmdnode_val(node, rdma->srccAddrL);
    dvpp_seq_set_cmdnode_val(node, REG_RDMA_LUMA_PAYLOAD_STRIDE);
    dvpp_seq_set_cmdnode_val(node, rdma->srcyStride);
    dvpp_seq_set_cmdnode_val(node, REG_RDMA_CHROMA_HEAD_STRIDE);
    dvpp_seq_set_cmdnode_val(node, rdma->srcchStride);
    dvpp_seq_set_cmdnode_val(node, REG_RDMA_CHROMA_PAYLOAD_STRIDE);
    dvpp_seq_set_cmdnode_val(node, rdma->srccStride);

    dvpp_seq_set_cmdnode_val(node, REG_RDMA_LUMA_HEAD_ADDR_H);
    dvpp_seq_set_cmdnode_val(node, rdma->srcyhAddrH);
    dvpp_seq_set_cmdnode_val(node, REG_RDMA_CHROMA_HEAD_ADDR_H);
    dvpp_seq_set_cmdnode_val(node, rdma->srcchAddrH);
    dvpp_seq_set_cmdnode_val(node, REG_RDMA_LUMA_PAYLOAD_ADDR_H);
    dvpp_seq_set_cmdnode_val(node, rdma->srcyAddrH);
    dvpp_seq_set_cmdnode_val(node, REG_RDMA_CHROMA_PAYLOAD_ADDR_H);
    dvpp_seq_set_cmdnode_val(node, rdma->srccAddrH);
}

static void drv_vpc_config_cmdnode_subchn_top(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx)
{
    U_VPC_OUTPUT_PIC_SIZE out2PicSize;
    U_VPC_OUTPUT_FORMAT out2Format;
    const uint32_t height_offset = 16;
    struct Top *top = (struct Top *)(dvpp_get_vpu_mod_val(decoder, node_idx, VPU_VPC_TYPE_TOP));

    out2PicSize.u32 = (top->out1Width) | (top->out1Height << height_offset);
    dvpp_seq_set_cmdnode_val(node, REG_VPC_OUTPUT2_PIC_SIZE);
    dvpp_seq_set_cmdnode_val(node, out2PicSize.u32);

    out2Format.u32 = top->out1Format;
    dvpp_seq_set_cmdnode_val(node, REG_VPC_OUTPUT_FORMAT);
    dvpp_seq_set_cmdnode_val(node, out2Format.u32);
}

static void drv_vpc_config_cmdnode_subchn(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx)
{
    struct Top *top = (struct Top *)(dvpp_get_vpu_mod_val(decoder, node_idx, VPU_VPC_TYPE_TOP));
    if (top->out1To3En == 0x3) {
        drv_vpc_config_cmdnode_subchn_top(decoder, node, node_idx);
        drv_vpc_config_cmdnode_out_swap(decoder, node, node_idx, SUB_CHN_1);
        drv_vpc_config_cmdnode_out_uvdown(decoder, node, node_idx, SUB_CHN_1);
        drv_vpc_config_cmdnode_postpadding(decoder, node, node_idx, SUB_CHN_1);
    }
}

void drv_vpc_build_cmdnode(dvpp_decoder *decoder, struct CmdNode *node, uint32_t node_idx)
{
    drv_vpc_config_cmdnode_reverse_space(node);
    drv_vpc_config_cmdnode_top(decoder, node, node_idx);
    drv_vpc_config_cmdnode_rdma(decoder, node, node_idx);
    drv_vpc_config_cmdnode_in_swap(decoder, node, node_idx);
    drv_vpc_config_cmdnode_out_swap(decoder, node, node_idx, MAIN_CHN);
    drv_vpc_config_cmdnode_uvup(decoder, node, node_idx);
    drv_vpc_config_cmdnode_csc(decoder, node, node_idx);
    drv_vpc_config_cmdnode_in0_uvdown(decoder, node, node_idx);
    drv_vpc_config_cmdnode_out_uvdown(decoder, node, node_idx, MAIN_CHN);
    drv_vpc_config_cmdnode_xflip(decoder, node, node_idx);
    drv_vpc_config_cmdnode_precrop(decoder, node, node_idx);
    drv_vpc_config_cmdnode_postcrop(decoder, node, node_idx);
    drv_vpc_config_cmdnode_resize(decoder, node, node_idx, MAIN_CHN);
    drv_vpc_config_cmdnode_resize(decoder, node, node_idx, SUB_CHN_1);
    drv_vpc_config_cmdnode_yuvscaler(decoder, node, node_idx, MAIN_CHN);
    drv_vpc_config_cmdnode_yuvscaler(decoder, node, node_idx, SUB_CHN_1);
    drv_vpc_config_cmdnode_prepadding(decoder, node, node_idx);
    drv_vpc_config_cmdnode_postpadding(decoder, node, node_idx, MAIN_CHN);
    drv_vpc_config_cmdnode_dump_reg(decoder, node, node_idx);
    drv_vpc_config_cmdnode_histogram(decoder, node, node_idx);
    drv_vpc_config_cmdnode_mosaic(decoder, node, node_idx);
    drv_vpc_config_cmdnode_cover(decoder, node, node_idx);
    drv_vpc_config_cmdnode_osd(decoder, node, node_idx);
    drv_vpc_config_cmdnode_rotate(decoder, node, node_idx);
    drv_vpc_config_cmdnode_affine(decoder, node, node_idx);
    drv_vpc_config_cmdnode_lut(decoder, node, node_idx);
    drv_vpc_config_cmdnode_filter(decoder, node, node_idx);
    drv_vpc_config_cmdnode_blending(decoder, node, node_idx);
    drv_vpc_config_cmdnode_vlc(decoder, node, node_idx);
    drv_vpc_config_cmdnode_subchn(decoder, node, node_idx);
    drv_vpc_config_cmdnode_frame_start(decoder, node, node_idx);
    drv_vpc_config_cmdnode_head(decoder, node, node_idx);
    drv_vpc_config_cmdnode_writeback_rear(node);
}

void drv_vpc_cmdnode_set_next_node(struct CmdBuf *cmd_buf, struct CmdNode *node)
{
    const uint32_t REG_CONNECT_HEAD_1 = VPC_CMDLST_OFFSET_ADDR + 0x008;
    const uint32_t REG_CONNECT_HEAD_2 = VPC_CMDLST_OFFSET_ADDR + 0x010;
    const uint32_t REG_CONNECT_HEAD_3 = VPC_CMDLST_OFFSET_ADDR + 0x014;
    uint64_t baseAddr = (uint64_t)(uintptr_t)(node->next->startAddr) - (uint64_t)(uintptr_t)(cmd_buf->startAddr) +
                        cmd_buf->ioStartAddr;
    uint32_t buf_len0 = node->next->dataSize - node->next->len1;

    dvpp_ext_set_cmdnode_val(node, REG_CONNECT_HEAD_2);
    dvpp_ext_set_cmdnode_val(node, GET_LOW_ADDR((baseAddr >> NUM_7)));
    dvpp_ext_set_cmdnode_val(node, REG_CONNECT_HEAD_1);
    dvpp_ext_set_cmdnode_val(node, (uint32_t)(baseAddr >> NUM_39));
    dvpp_ext_set_cmdnode_val(node, REG_CONNECT_HEAD_3);
    dvpp_ext_set_cmdnode_val(node, ((buf_len0 >> NUM_4) | ((node->next->len1 >> NUM_4) << NUM_16)));
}

void drv_vpc_config_sqelist(struct CmdBuf *cmd_buf, uint32_t sqe_idx, uint32_t blkdim)
{
    uint32_t *cur_sqe_addr = (uint32_t *)(uintptr_t)(cmd_buf->bufAddr + sqe_idx * SQE_LEN);
    uint64_t offset = (uint64_t)(uintptr_t)(cmd_buf->cmdHeadNode->startAddr) -
                      (uint64_t)(uintptr_t)(cmd_buf->startAddr);
    uint64_t startAddr = cmd_buf->ioStartAddr + offset;
    uint32_t buf_len0 = cmd_buf->cmdHeadNode->dataSize - cmd_buf->cmdHeadNode->len1;
    uint32_t buf_len1 = cmd_buf->cmdHeadNode->len1;

    cur_sqe_addr[0] = DVPP_SQE_VPC; // Type=12 VPC任务
    cur_sqe_addr[0] |= (1 << SQE_WRCQE_OFFSET);
    cur_sqe_addr[0] |= (blkdim << SQE_BLKDIM_OFFSET); // 第0个4字节的第16-31位表示blockdim的数量
    cur_sqe_addr[NUM_3] &= ~(0xff << SQE_BLKDIM_OFFSET);
    cur_sqe_addr[NUM_3] |= (KERNEL_CREDIT << SQE_TIMEOUT_OFFSET);
    cur_sqe_addr[NUM_3] |= (0x1 << SQE_PTRMODE_OFFSET);
    cur_sqe_addr[NUM_4] = GET_LOW_ADDR((startAddr >> NUM_7));
    cur_sqe_addr[NUM_5] = (uint32_t)(startAddr >> NUM_39);
    // Cmdlist Length in 16byte unit
    cur_sqe_addr[NUM_6] = (buf_len0 >> NUM_4) | ((buf_len1 >> NUM_4) << NUM_16);
}

void drv_vpc_config_sqe(struct CmdBuf *cmd_buf, enum dvpp_sqe_ptr_mode mode, uint32_t blkdim, struct dvpp_sqe *sqe)
{
    if (mode == DVPP_SQE_FIRST_PTR_MODE) {
        uint64_t offset = (uint64_t)(uintptr_t)(cmd_buf->cmdHeadNode->startAddr) -
                          (uint64_t)(uintptr_t)(cmd_buf->startAddr);
        uint64_t startAddr = cmd_buf->ioStartAddr + offset;
        uint32_t buf_len0 = cmd_buf->cmdHeadNode->dataSize - cmd_buf->cmdHeadNode->len1;
        uint32_t buf_len1 = cmd_buf->cmdHeadNode->len1;
        uint32_t timeout = ((sqe->filed[3] >> SQE_TIMEOUT_OFFSET) & 0xff);

        sqe->filed[0] |= DVPP_SQE_VPC;
        sqe->filed[0] |= (1 << SQE_WRCQE_OFFSET);
        if ((timeout == 0x0U) || (timeout == 0xffU)) { // 未配置或配置非法值0, 则使用默认值
            sqe->filed[NUM_3] &= ~(0xff << SQE_TIMEOUT_OFFSET);
            sqe->filed[NUM_3] |= (KERNEL_CREDIT << SQE_TIMEOUT_OFFSET);
        }
        sqe->filed[NUM_3] &= ~(0x1 << SQE_PTRMODE_OFFSET);
        // Start address [38:7] of the frame (in 128 bytes boundary) for configure buffer.
        sqe->filed[NUM_4] = GET_LOW_ADDR((startAddr >> NUM_7));
        // Start address [63:39] of the frame (in 128 bytes boundary) for configure buffer.
        sqe->filed[NUM_5] = (uint32_t)(startAddr >> NUM_39);
        sqe->filed[NUM_6] = ((buf_len0 >> NUM_4) | ((buf_len1 >> NUM_4) << NUM_16));
    } else {
        // TD 二级链表模式(多block dim)
        sqe->filed[0] = DVPP_SQE_VPC; // Type=12 VPC 任务
        sqe->filed[0] |= (1 << SQE_WRCQE_OFFSET);
        sqe->filed[0] |= (blkdim << SQE_BLKDIM_OFFSET); // 第0个4字节的第16-31位表示blockdim的数量
        sqe->filed[NUM_3] &= ~(0xff << SQE_BLKDIM_OFFSET);
        sqe->filed[NUM_3] |= (KERNEL_CREDIT << SQE_TIMEOUT_OFFSET); // 第3个4字节的第16-23位全1表示该任务不记录超时
        sqe->filed[NUM_3] |= (0x1U << SQE_PTRMODE_OFFSET); // 第3个4字节的第24位表示链表模式，1表示二级
        sqe->filed[NUM_4] = GET_LOW_ADDR(cmd_buf->ioAddr); // 第4个4字节存放二级链表的低32位地址
        sqe->filed[NUM_5] = GET_HIGH_ADDR(cmd_buf->ioAddr); // 第5个4字节的低16位存放二级链表的高16位地址
        sqe->filed[NUM_5] &= 0x1FFFF;                       // 第5个4字节的17~30bit是保留字段
        sqe->filed[NUM_5] |= (0x1U << SQE_ADDRTYPE_OFFSET); // 第5个4字节的最高位表示地址类型 1:虚拟地址 0:物理地址
    }
}

void drv_vpc_cmdnode_connect_last_node(struct CmdBuf *cmd_buf)
{
    struct CmdNode *node = cmd_buf->cmdTailNode;

    drv_vpc_cmdnode_clear_interrupt(node);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_ERR_INT_MASK);
    dvpp_ext_set_cmdnode_val(node, node->headInfo.errMask);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_NORM_INT_MASK2);
    dvpp_ext_set_cmdnode_val(node, 0xFFFDFFFF);

    dvpp_ext_set_cmdnode_val(node, REG_VPC_EOF_INT1_MERGE_ENABLE_RDMA_CVDR);
    dvpp_ext_set_cmdnode_val(node, node->headInfo.cvdrIntMerge);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_EOF_INT1_MERGE_ENABLE_PIPE);
    dvpp_ext_set_cmdnode_val(node, node->headInfo.pipeIntMerge);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_CMDLIST_IN_INT_CTRL);
    dvpp_ext_set_cmdnode_val(node, 0x2);
    dvpp_ext_set_cmdnode_val(node, REG_VPC_ERR_OUT_DMUXEN);
    dvpp_ext_set_cmdnode_val(node, 0xC);
}

void drv_vpc_cmdnode_config_writeback_front(struct CmdNode *pos_node, struct CmdNode *pre_node)
{
    UNUSED(pos_node);
    UNUSED(pre_node);
    return;
}
