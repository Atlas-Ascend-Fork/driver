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
#ifndef DEVDRV_HARDWARE_VERSION_H
#define DEVDRV_HARDWARE_VERSION_H

enum devdrv_arch_type {
    ARCH_BEGIN = 0,
    ARCH_V100 = ARCH_BEGIN, /* Ascend310，Ascend910，Ascend910B Ascend910_93 used value */
    ARCH_V200,              /* Ascend310P */
    ARCH_V300,              /* Ascend310B */
    ARCH_C100 = 3,          /* Ascend910 real value */
    ARCH_C220 = 4,          /* Ascend910B & Ascend910_93 real value */
    ARCH_M100 = 5,          /* Ascend310 real value */
    ARCH_M200 = 6,
    ARCH_M201 = 7,
    ARCH_T300 = 8, /* Tiny */
    ARCH_N350 = 9, /* Nano */
    ARCH_M300 = 10,
    ARCH_M310 = 11,
    ARCH_S200 = 12, /* Hi3796CV300ES & TsnsE */
    ARCH_S202 = 13, /* Hi3796CV300CS & OPTG & SD3403 &TsnsC */
    ARCH_M5102 = 14,
    ARCH_END
};

enum devdrv_chip_type {
    CHIP_BEGIN = 0,
    CHIP_MINI = CHIP_BEGIN, /* Ascend310 */
    CHIP_CLOUD,             /* Ascend910 */
    CHIP_RSVD_1,            /* Reserved */
    CHIP_LHISI,             /* Hi3796CV300ES & TsnsE, Hi3796CV300CS & OPTG & SD3403 &TsnsC */
    CHIP_DC,                /* Ascend310P */
    CHIP_CLOUD_V2 = 5,      /* Ascend910B & Ascend910_93 */
    CHIP_RSVD_2 = 6,        /* Reserved */
    CHIP_MINI_V3 = 7,       /* Ascend310B */
    CHIP_TINY_V1 = 8,       /* Tiny */
    CHIP_NANO_V1 = 9,       /* Nano */
    CHIP_KUNPENG_V1 = 10,   /* KUNPENG */
    CHIP_RSVD_3 = 11,       /* Reserved */
    CHIP_RSVD_4 = 12,       /* Reserved */
    CHIP_CLOUD_V3 = 13,     /* Ascend910_93 */
    CHIP_RSVD_5 = 14,       /* Reserved */
    CHIP_CLOUD_V4 = 15,     /* Ascend950 */
    CHIP_CLOUD_V5 = 16,     /* Ascend960 */
    CHIP_RSVD_6 = 17,       /* Reserved */
    CHIP_RSVD_7 = 18,       /* Reserved */
    CHIP_MINI_V4 = 19,      /* Ascend350 */
    CHIP_XPU = 20,          /* Keep value aligned, not use in driver */
    CHIP_5162A = 21,        /* Keep value aligned, not use in driver */
    CHIP_CLOUD_V6 = 22,     /* Ascend960PR(David V122) */
    CHIP_CLOUD_V7 = 23,     /* Ascend970 */
    CHIP_MINI_V5_RC = 24,   /* Ascend310N_RC */
    CHIP_MINI_V5 = 25,      /* Ascend310N */
    CHIP_END
};

enum devdrv_chip_name {
    CHIP_NAME_CLOUD_V4 = 6514, /* 6514 = 0x1972, 950 */
    CHIP_NAME_END
};

enum devdrv_version {
    VER_BEGIN = 0,
    VER_NA = VER_BEGIN,
    VER_ES,
    VER_CS,
    VER_END
};

enum devdrv_pg_version {
    VER_BIN1 = 1,
    VER_BIN2,
    VER_BIN3,
    VER_BIN4,
    VER_BIN5,
    VER_BIN6,
    VER_BIN7,
    VER_BIN8,
    VER_BIN9,
    VER_BIN10,
    VER_BIN11,
};

#define PLAT_COMBINE(arch, chip, ver) (((arch) << 16) | ((chip) << 8) | (ver))
#define PLAT_GET_ARCH(type) ((type >> 16) & 0xffff)
#define PLAT_GET_CHIP(type) ((type >> 8) & 0xff)
#define PLAT_GET_VER(type) (type & 0xff)

enum devdrv_hardware_version {
    DEVDRV_PLATFORM_MINI_V1 = PLAT_COMBINE(ARCH_V100, CHIP_MINI, VER_NA),
    DEVDRV_PLATFORM_CLOUD_V1 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD, VER_NA),
    DEVDRV_PLATFORM_LHISI_ES = PLAT_COMBINE(ARCH_S200, CHIP_LHISI, VER_ES),
    DEVDRV_PLATFORM_LHISI_CS = PLAT_COMBINE(ARCH_S202, CHIP_LHISI, VER_CS),
    DEVDRV_PLATFORM_ASCEND310P = PLAT_COMBINE(ARCH_V200, CHIP_DC, VER_NA),
    DEVDRV_PLATFORM_CLOUD_V2 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_NA),
    DEVDRV_PLATFORM_CLOUD_V2_B1 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_BIN1),
    DEVDRV_PLATFORM_CLOUD_V2_B2 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_BIN2),
    DEVDRV_PLATFORM_CLOUD_V2_B3 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_BIN3),
    DEVDRV_PLATFORM_CLOUD_V2_B4 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_NA),
    DEVDRV_PLATFORM_CLOUD_V2_B4_1 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_BIN10),
    DEVDRV_PLATFORM_CLOUD_V2_B2C = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_BIN8),
    DEVDRV_PLATFORM_CLOUD_V3 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V2, VER_NA),
    DEVDRV_PLATFORM_ASCEND310B = PLAT_COMBINE(ARCH_V300, CHIP_MINI_V3, VER_NA),
    DEVDRV_PLATFORM_ASCEND310B_B1 = PLAT_COMBINE(ARCH_V300, CHIP_MINI_V3, VER_BIN1),
    DEVDRV_PLATFORM_ASCEND310B_B2 = PLAT_COMBINE(ARCH_V300, CHIP_MINI_V3, VER_BIN2),
    DEVDRV_PLATFORM_ASCEND310B_B3 = PLAT_COMBINE(ARCH_V300, CHIP_MINI_V3, VER_BIN3),
    DEVDRV_PLATFORM_ASCEND310B_B4 = PLAT_COMBINE(ARCH_V300, CHIP_MINI_V3, VER_BIN4),
    DEVDRV_PLATFORM_CLOUD_V4 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V4, VER_NA),
    DEVDRV_PLATFORM_CLOUD_V5 = PLAT_COMBINE(ARCH_V100, CHIP_CLOUD_V5, VER_NA),
    DEVDRV_PLATFORM_MINI_V4 = PLAT_COMBINE(ARCH_V100, CHIP_MINI_V4, VER_NA),
    DEVDRV_PLATFORM_END
};

#endif