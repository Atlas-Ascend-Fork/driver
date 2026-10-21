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

#ifndef VPC_CMDLIST_PIPELINE_KERNEL_H
#define VPC_CMDLIST_PIPELINE_KERNEL_H

#ifdef __KERNEL__
#include "ka_type.h"
#else
#include <stdint.h>
#endif

#include "pipeline_define.h"

typedef struct Crop {
    uint32_t hmin;
    uint32_t hmax;
    uint32_t vmin;
    uint32_t vmax;
} crop_t;

typedef struct Padding {
    uint8_t en;
    uint32_t mirrorUvSel;
    uint32_t dirPriority;
    uint32_t mirrorMode;
    uint32_t paddingSel;
    uint32_t bottomSize;
    uint32_t topSize;
    uint32_t rightSize;
    uint32_t leftSize;
    uint32_t inHeight;
    uint32_t inWidth;
    uint32_t regValueHor[3];
    uint32_t regValueVer[3];
} padding_t;

typedef struct Flip {
    uint8_t enX;
    uint8_t enY;
    uint8_t isPreX; // 选择使用prexlip还是postxlip, 不可同时使用
    uint8_t isPreY;
    uint32_t inHeight;
    uint32_t inWidth;
} flip_t;

typedef struct RoiPara {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} roi_para_t;

typedef struct GainPara {
    struct RoiPara roi;
    uint32_t gainVal;
} gain_para_t;

typedef struct BlendGainPara {
    struct GainPara in0Gain0;
    struct GainPara in0Gain1;
    struct GainPara in1Gain0;
    struct GainPara in1Gain1;
} blend_gain_para_t;

typedef struct Blend {
    uint8_t en;
    uint32_t blendMode;
    uint32_t lay0InSize;
    uint32_t lay1InSize;
    uint32_t leftPosition;
    uint32_t coefAlpha;
    uint32_t coefBeta;
    uint32_t coefGamma;
    uint32_t posterizeOp;
    uint32_t solarizeThldMin;
    uint32_t solarizeThldMax;
    uint32_t eraseTop;
    uint32_t eraseLeft;
    uint32_t eraseWidth;
    uint32_t eraseHeight;
    uint32_t eraseR;
    uint32_t eraseG;
    uint32_t eraseB;
    uint32_t out1en;
    struct BlendGainPara gain;
    uint32_t alphaMode;
    uint32_t offsetEn;
} blend_t;

typedef struct Pyramid {
    uint8_t en;
    uint32_t mode;
    uint32_t level;
    uint32_t uvSel;
    uint32_t mirrorMode;
    uint32_t paddingSel;
    uint32_t paddingVal;
} pyramid_t;

typedef struct Resize {
    uint8_t en;
    uint32_t inHeight;
    uint32_t inWidth;
    uint32_t outHeight;
    uint32_t outWidth;
    uint32_t ihincL;
    uint32_t ihincH;
    uint32_t ivincL;
    uint32_t ivincH;
    uint32_t biasHaL;
    uint32_t biasHaH;
    uint32_t biasHbL;
    uint32_t biasHbH;
    uint32_t biasVaL;
    uint32_t biasVaH;
    uint32_t biasVbL;
    uint32_t biasVbH;
    uint32_t biasHaUvL;
    uint32_t biasHaUvH;
    uint32_t biasHbUvL;
    uint32_t biasHbUvH;
    uint32_t cubicA;
    uint32_t rndCtrl0;
    uint8_t splitEn;
    uint32_t posType;
    uint32_t incType;
    uint32_t incSrc;
    uint32_t biasSrc;
    uint32_t alignMode;
    uint32_t funcType;
    uint32_t indexInLeft;
    uint32_t indexInRight;
    uint32_t indexInTop;
    uint32_t indexInBottom;
    uint32_t indexOutLeft;
    uint32_t indexOutRight;
    uint32_t indexOutTop;
    uint32_t indexOutBottom;
} resize_t;

typedef struct YuvScalerPara {
    int16_t taps4[SCALER_COFF_NB_4];
    int16_t taps6[SCALER_COFF_NB_6];
    int16_t taps8[SCALER_COFF_NB_8];
} yuvscaler_para_t;

typedef struct YuvWPara {
    struct YuvScalerPara scale[2]; // scale 0 - horizontal, scale 1 - vertical
} yuv_wpara_t;

typedef struct YuvScale {
    uint32_t bypass;
    uint32_t ihleftInt;
    uint32_t ihleftDec;
    uint32_t ihrightInt;
    uint32_t ihrightDec;
    uint32_t ivtopInt;
    uint32_t ivtopDec;
    uint32_t ivbotInt;
    uint32_t ivbotDec;
    uint64_t ihinc;
    uint64_t ivinc;
    uint32_t inHeight;
    uint32_t inWidth;
    uint32_t outHeight;
    uint32_t outWidth;
    struct YuvWPara coeff;
} yuvscaler_t;

typedef struct MosaicRegion {
    uint8_t en;
    uint32_t hmin;
    uint32_t hmax;
    uint32_t vmin;
    uint32_t vmax;
} mosaic_region_t;

typedef struct Mosaic {
    uint32_t inWidth;
    uint32_t inHeight;
    uint32_t blockSize;
    struct MosaicRegion mosaicRegion[MOSAIC_REGION_NUM];
} mosaic_t;

typedef struct Rotate {
    uint8_t en;
    uint32_t inHeight;
    uint32_t inWidth;
    uint32_t rotDir;
    uint32_t inPicAddrH[3];
    uint32_t inPicAddrL[3];
    uint32_t inPicStride[3];
    uint32_t outPicAddrH[3];
    uint32_t outPicAddrL[3];
    uint32_t outPicStride[3];
} rotate_t;

typedef struct Point {
    int32_t x;
    int32_t y;
} point_t;

typedef struct CoverRegion {
    uint8_t en;
    uint32_t coverValue;
    int32_t coverkk01;
    int32_t coverkk02;
    int32_t coverkk03;
    int32_t coverkk12;
    int32_t coverkk13;
    int32_t coverkk23;
    struct Point points[4];
} cover_region_t;

typedef struct Cover {
    uint32_t inWidth;
    uint32_t inHeight;
    uint32_t alignMode;
    struct CoverRegion coverRegion[COVER_REGION_NUM];
} cover_t;

typedef struct OsdCsc {
    uint8_t en;
    uint32_t coeff[24]; // 24: csc系数个数
} osd_csc_t;

typedef struct OsdRegion {
    uint8_t en;
    uint32_t osdImgAddrL;
    uint32_t osdImgAddrH;
    uint32_t osdImgStride;
    uint32_t osdImgHeight;
    uint32_t osdImgWidth;
    uint32_t osdHmin;
    uint32_t osdHmax;
    uint32_t osdVmin;
    uint32_t osdVmax;
    uint32_t osdFormat;
    uint32_t osdGlobalAlpha;
    uint32_t osdBitExtMode;
    uint8_t osdAlphaEn;
    uint32_t osdAlpha0;
    uint32_t osdAlpha1;
    uint32_t osdAlphaRev; // OSD 图像 alpha值反色开关
    uint32_t osdRgbRev;   // OSD 图像 rgb像素值反色开关
    struct OsdCsc osdCsc;
} osd_region_t;

typedef struct Osd {
    uint32_t inWidth;
    uint32_t inHeight;
    uint32_t clut[16];
    struct OsdRegion osdRegion[OSD_REGION_NUM];
} osd_t;

typedef struct Histogram {
    uint8_t en;
    uint8_t isYuvSum;
    uint32_t mode;
    uint32_t readSize;
    uint64_t readAddr;
    uint32_t maxminEn;
} histogram_t;

typedef struct DumpReg {
    uint8_t en;
    uint32_t readSize;
    uint64_t readAddr;
} dump_reg_t;

typedef struct Lut {
    uint8_t en; // 使能LUT模块中的LUT功能，与后面posterizeEn，cutoutEn同级别
    uint32_t width;
    uint32_t height;
    uint8_t mapValueYorR[256];
    uint8_t mapValueUorG[256];
    uint8_t mapValueVorB[256];
    uint8_t posterizeEn;
    uint32_t posterizeValue;
    uint32_t cutoutEn;        // 11:9, 8:6, 5:3, 2:0, 分别控制窗口3,2,1,0的使能
    uint32_t cutoutValue[12]; // 12：4个窗口 * 3个通道。win0:R,G,B; ... win3:R,G,B
    uint32_t cutoutWin[16];   // 16: 4个窗口 * 4个坐标。win0:H,V; ... win3:H,V
    uint8_t lutSecondaryPointerEn;
    uint32_t mapYorRAddrHigh;
    uint32_t mapYorRAddrLow;
    uint32_t mapUorGAddrHigh;
    uint32_t mapUorGAddrLow;
    uint32_t mapVorBAddrHigh;
    uint32_t mapVorBAddrLow;
} lut_t;

typedef struct Filter {
    uint8_t en;
    uint8_t isGaussFun;
    uint8_t needTransFixPoint;
    uint8_t isKernelFp32;
    uint32_t mode;
    uint32_t blurMode;
    uint32_t sortMode;
    uint32_t rndMode;
    uint32_t ksizeX;
    uint32_t ksizeY;
    uint32_t inHeight;
    uint32_t inWidth;
    uint32_t outHeight;
    uint32_t outWidth;
    uint32_t paddingSel;
    uint32_t regValueHor[3];
    uint32_t regValueVer[3];
    uint32_t coeff[25];
    uint32_t scale;
    uint32_t delta;
    uint32_t edgeBypass;
} filter_t;

typedef struct Hsv {
    uint8_t hueFst;
    uint8_t hueEn;
    uint8_t satEn;
    uint8_t hsv2rgbEn;
    uint8_t rgb2hsvEn;
    uint32_t scaleSat;
    uint32_t deltaHue;
} hsv_t;

typedef struct Convert {
    uint8_t en;
    uint8_t scaleEn;
    uint32_t rnd;
    uint32_t scale;
} convert_t;

typedef struct Enhance {
    uint8_t en;
    uint32_t adder;
    uint32_t multipler;
} enhance_t;

typedef struct Contrast {
    uint8_t en;
    uint32_t para0;
    uint32_t para1;
    uint32_t para2;
    uint32_t para3;
} contrast_t;

typedef struct ContrastV2 {
    uint8_t secondaryPointerEn;
    uint32_t factor;
    uint32_t rHighAddr;
    uint32_t rLowAddr;
    uint32_t gHighAddr;
    uint32_t gLowAddr;
    uint32_t bHighAddr;
    uint32_t bLowAddr;
    uint32_t dataLen;
} contrast_v2_t;

typedef struct Transform {
    uint8_t en;
    uint32_t para[3][3];
} transform_t;

typedef struct Normalize {
    uint8_t en;
    uint32_t mode;
    uint32_t scale[3];
    uint32_t delta[3];
} normalize_t;

typedef struct VpcClip {
    uint8_t en;
    uint8_t above;
    uint32_t max[3];
    uint32_t min[3];
    uint32_t cval[3];
    uint32_t mode;
} vpc_clip_t;

typedef struct PixAug {
    struct Enhance enhance;
    struct Contrast contrast;
    struct Transform transform;
    struct Normalize normalize;
    struct VpcClip clip;
    struct ContrastV2 contrastV2;
} pixaug_t;

typedef struct MaskData {
    uint8_t en;
    uint32_t maskAddrL;
    uint32_t maskAddrH;
    uint32_t maskStride;
} mask_t;

typedef struct Affine {
    uint8_t en;
    uint8_t en3d;
    uint32_t coorSel;
    uint32_t rndMode;
    uint32_t dataFmt;
    uint32_t calcMode;
    uint32_t inHeight;
    uint32_t inWidth;
    uint32_t outHeight;
    uint32_t outWidth;
    uint32_t lutAddrL;
    uint32_t lutAddrH;
    uint32_t lutStride;
    uint32_t wrAddrL;
    uint32_t wrAddrH;
    uint32_t wrStride;
    uint32_t blkHeight;
    uint32_t blkWidth;
    uint32_t coeff[12];
    uint32_t inDepth3d;
    uint32_t outDepth3d;
    uint32_t paddingSel;
    uint32_t regValueHor[3];
    uint32_t regValueVer[3];
    uint32_t outMode;
    uint32_t viewFmt;
    uint32_t lut1AddrL;
    uint32_t lut1AddrH;
    uint32_t enChn1;
    struct MaskData mask0;
    struct MaskData mask1;
} affine_t;

typedef struct DataTransform {
    uint8_t en;
    uint32_t mode;
} data_transform_t;

#endif // VPC_CMDLIST_PIPELINE_KERNEL_H