/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BBOX_RDR_PUB_H
#define BBOX_RDR_PUB_H

#include <stdbool.h>
#include "bbox_proc_types.h"

/**********************************************************************/
/*                            exception id                            */
/*                                                                    */
/*  31  30 29  28 27     25 24       17 16       12 11             0  */
/*  +-----+------+---------+-----------+-----------+---------------+  */
/*  | H/D | type |  level  | module id | submod id | function code |  */
/*  +--2--+--2---+----3----+-----8-----+-----5-----+-------12------+  */
/*          H/D             type                level                 */
/*      0b01 host       0b01 error          0b100 emergency           */
/*      0b10 device     0b10 exception      0b011 critical            */
/*                                          0b010 warning             */
/*                                          0b001 notice              */
/*                                          0b000 no-level            */
/**********************************************************************/
// exceptions report from device hdc
#define ATF_PANIC 0xA8340000U

#define RDR_EXCEPID_COREID_BIT 17U
#define BBOX_EXCEPID_POSITION_BIT 30U
#define BBOX_EXCEPID_POSITION_MASK 0x3U
#define BBOX_EXCEPID_POSITION_DEVICE 0x2U
#define BBOX_EXCEPID_COREID_MASK 0xFFU

enum bbox_rdr_rb_reason {
    REBOOT_REASON_LABEL0 = 0x0, /* label0: bios restart in phase mode. */
    DEVICE_COLDBOOT = 0x0, /* Cold start, such as the first startup after power-off; first startup after power failure.
                            */
    BIOS_EXCEPTION = 0x1,  /* bios abnormal, BIOS exception occurred during the previous startup. */
    DEVICE_HOTBOOT = 0x2,  /* Hot reset, such as reset by pressing a button or a hard reset of the chip. */
    REBOOT_REASON_LABEL1 = 0x10,   /* label1: reset due to hardware reasons. */
    ABNORMAL_EXCEPTION = 0x10,     /* Undetected exception. */
    TSENSOR_EXCEPTION = 0x1F,      /* soc thermal protection reset. */
    PMU_EXCEPTION = 0x20,          /* Hardware reset caused by overcurrent, undervoltage, or PMU overtemperature. */
    DDR_CRITICAL_EXCEPTION = 0x22, /* ddr critical abnormal reset, such as DDR chip overtemperature reset. */
    REBOOT_REASON_LABEL2 = 0x24,   /* label2:OS software-induced reset. */
    OS_PANIC = 0x24,               /* os panic， for example, accessing an illegal addr. */
    OS_OOM = 0x2A,                 /* OOM Exception. */
    OS_COMM = 0x2B,                /* Communication abnormality. */
    REBOOT_REASON_LABEL3 = 0x2C,   /* label3: module reset. */
    STARTUP_EXCEPTION = 0x2C,      /* Module startup abnormality. */
    HEARTBEAT_EXCEPTION = 0x2D,    /* Module heartbeat abnormality. */
    RUN_EXCEPTION = 0x2E,          /* Module operation abnormality. */
    HDR_EXCEPTION = 0x2F,          /* Module snapshot abnormality. */
    LPM_EXCEPTION = 0x32,          /* LPM3 various abnormalities detected by the subsystem. */
    TS_EXCEPTION = 0x33,           /* TS various abnormalities detected by the subsystem. */
    MICROWATT_EXCEPTION = 0x34,    /* MICROWATT exception. */
    DVPP_EXCEPTION = 0x35,         /* DVPP exception. */
    DRIVER_EXCEPTION = 0x36,       /* DRIVER exception. */
    TEE_EXCEPTION = 0x38,          /* teeos exception. */
    LPFW_EXCEPTION = 0x39,         /* LPFW exception. */
    NETWORK_EXCEPTION = 0x3A,      /* NETWORK exception. */
    HSM_EXCEPTION = 0x3B,          /* HSM exception. */
    ATF_EXCEPTION = 0x3C,          /* ATF exception. */
    ISP_EXCEPTION = 0x3D,          /* ISP exception. */
    SAFETYISLAND_EXCEPTION = 0x3E, /* SAFETYISLAND exception. */
    TOOLCHAIN_EXCEPTION = 0x3F,    /* TOOLCHAIN exception. */
    DSS_EXCEPTION = 0x40,          /* DSS exception. */
    COMISOLATOR_EXCEPTION = 0x41,  /* COMISOLATOR exception. */
    SD_EXCEPTION = 0x42,           /* SD exception. */
    DP_EXCEPTION = 0x43,           /* DP exception. */
    REBOOT_REASON_LABEL4 = 0x60,   /* label4: */
    SUSPEND_FAIL = REBOOT_REASON_LABEL4, /* Sleep failure LP-initiated reset. */
    RESUME_FAIL = 0x61,                  /* Wake-up failure LP-initiated reset. */
    CPUCORE_EXCEPTION = 0x62,            /* Core hang-up abnormality. */
    REBOOT_REASON_LABEL8 = 0x89,         /* label5: host side abnormality. */
    DEVICE_LTO_EXCEPTION = 0x8A,         /* Device startup timeout. */
    DEVICE_HBL_EXCEPTION = 0x8B,         /* Device heartbeat lost. */
    DEVICE_HDC_EXCEPTION = 0x8E,         /* Device HDC abnormality. */
    BOOT_DOT_INFO = 0xFE, // Startup logging information; log files are exported unconditionally after startup.
    BBOX_EXCEPTION_REASON_INVALID = 0xFF,
};

/*
 * snapshot log for startup not exception
 */
#define BOOT_DOT_ID 0xB2600000

/* Module ID list. */
enum bbox_rdr_coreid {
    BBOX_DRIVER = 0x1,
    BBOX_OS = 0x2,
    BBOX_TS = 0x3,
    BBOX_AICPU = 0x5,
    BBOX_DVPP = 0xa,
    BBOX_AIPP = 0xb,
    BBOX_LPM = 0xc,
    BBOX_MICROWATT = 0xd,
    BBOX_TOOLCHAIN = 0xf,
    BBOX_BSBC = 0x13,
    BBOX_BIOS = 0x14,
    BBOX_TEEOS = 0x15,
    BBOX_LPFW = 0x17,
    BBOX_NETWORK = 0x18,
    BBOX_ATF = 0x1A,
    BBOX_HSM = 0x1B,
    BBOX_ISP = 0x1C,
    BBOX_SIL = 0x1D,
    BBOX_DSS = 0x1E,
    BBOX_COMISOLATOR = 0x1F,
    BBOX_AOS_SD = 0x20,
    BBOX_AOS_DP = 0x21,
    BBOX_AOS_LINUX = 0x22,
    BBOX_AOS_CORE = 0x23,
    BBOX_PEK = 0x24,
    BBOX_IMU = 0x25,
    BBOX_QOS = 0x26,
    BBOX_UB = 0x27,
    BBOX_COMMON = 0x30,
    BBOX_REGDUMP = 0x31,
    BBOX_CORE_MAX = 0x32
};

static inline u8 bbox_excep_id_get_core_id(u32 excepid)
{
    return (u8)((excepid >> RDR_EXCEPID_COREID_BIT) & BBOX_EXCEPID_COREID_MASK);
}

static inline bool bbox_core_id_valid(u8 coreid)
{
    switch (coreid) {
        case BBOX_DRIVER:
        case BBOX_OS:
        case BBOX_TS:
        case BBOX_AICPU:
        case BBOX_DVPP:
        case BBOX_AIPP:
        case BBOX_LPM:
        case BBOX_MICROWATT:
        case BBOX_TOOLCHAIN:
        case BBOX_BSBC:
        case BBOX_BIOS:
        case BBOX_TEEOS:
        case BBOX_LPFW:
        case BBOX_NETWORK:
        case BBOX_ATF:
        case BBOX_HSM:
        case BBOX_ISP:
        case BBOX_SIL:
        case BBOX_DSS:
        case BBOX_COMISOLATOR:
        case BBOX_AOS_SD:
        case BBOX_AOS_DP:
        case BBOX_AOS_LINUX:
        case BBOX_AOS_CORE:
        case BBOX_PEK:
        case BBOX_IMU:
        case BBOX_QOS:
        case BBOX_UB:
        case BBOX_COMMON:
        case BBOX_REGDUMP:
            return true;
        default:
            return false;
    }
}

static inline bool bbox_check_excep_id_postition(u32 excepid)
{
    return (((excepid >> BBOX_EXCEPID_POSITION_BIT) & BBOX_EXCEPID_POSITION_MASK) == BBOX_EXCEPID_POSITION_DEVICE);
}

static inline bool bbox_check_excep_id_core_id(u32 excepid)
{
    return bbox_core_id_valid(bbox_excep_id_get_core_id(excepid));
}

static inline bool bbox_check_excep_id(u32 excepid)
{
    return (bbox_check_excep_id_postition(excepid) && bbox_check_excep_id_core_id(excepid));
}

#endif /* BBOX_RDR_PUB_H */
