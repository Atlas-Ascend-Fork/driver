/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __DMS_SOC_INTERFACE_H
#define __DMS_SOC_INTERFACE_H

#include "ascend_hal_error.h"
#include "dms_drv_internal.h"

/* Die ID */
#define DMS_PMU_DIEID_DATA_SIZE 8
#define DMS_DIEID_DATA_V2_SIZE 8
typedef struct dms_soc_dieid_stru {
    unsigned int dev_id;
    unsigned int soc_type;
    unsigned int pmu_dieid_type;
    unsigned int pmu_dev_id;
    unsigned char size;
    unsigned int data[DMS_PMU_DIEID_DATA_SIZE];
} dms_soc_die_id_t;

typedef struct dms_soc_dieid_v2_stru {
    unsigned int dev_id;
    unsigned int part_id;
    unsigned int data[DMS_DIEID_DATA_V2_SIZE];
} dms_soc_die_id_v2_t;

/* PCIE ID */
typedef struct dms_pcie_id_info {
    unsigned int venderid;    /* Vendor id */
    unsigned int subvenderid; /* Sub vendor id */
    unsigned int deviceid;    /* Device id */
    unsigned int subdeviceid; /* Sub device id */
    unsigned int bus;         /* Bus id */
    unsigned int device;      /* Device id */
    unsigned int fn;          /* Function id */
    unsigned int davinci_id;  /* device logical id */
    unsigned int domain;      /* domain id */
} dms_pcie_id_info_t;

#define MAX_CHIP_NAME 32
typedef struct dms_chip_info {
    unsigned char type[MAX_CHIP_NAME];
    unsigned char name[MAX_CHIP_NAME];
    unsigned char version[MAX_CHIP_NAME];
} dms_chip_info_t;

typedef struct dms_query_chip_info {
    unsigned int dev_id;
    unsigned int reg_val;
    dms_chip_info_t info;
    unsigned char resv[8];
} dms_query_chip_info_t;

drvError_t DmsGetBoardId(unsigned int dev_id, unsigned int *board_id);
drvError_t DmsGetPcbId(unsigned int dev_id, unsigned int *pcb_id);
drvError_t DmsGetBomId(unsigned int dev_id, unsigned int *bom_id);
drvError_t DmsGetSlotId(unsigned int dev_id, unsigned int *slot_id);
drvError_t DmsGetPcieIdInfo(unsigned int dev_id, dms_pcie_id_info_t *pcie_idinfo);
drvError_t DmsGetCpuInfo(unsigned int dev_id, drvCpuInfo_t *cpu_info);
drvError_t DmsGetSocDieId(dms_soc_die_id_t *soc_die_id);
drvError_t DmsGetSocDieIdV2(dms_soc_die_id_v2_t *soc_die_id);
drvError_t DmsGetChipInfo(dms_query_chip_info_t *chip_info);
drvError_t dms_user_get_reboot_reason(unsigned int dev_id, void *reboot_reason, unsigned int len);
drvError_t DmsSetBistInfo(unsigned int dev_id, unsigned int cmd, unsigned char *in_buf, unsigned int buf_len);
drvError_t DmsGetChipVersion(unsigned int devid, unsigned char *chip_version);
drvError_t DmsSetBistInfoMultiCmd(unsigned int dev_id, unsigned int cmd, unsigned char *in_buf, unsigned int buf_len);
#endif /* __DMS_SOC_INTERFACE_H */
