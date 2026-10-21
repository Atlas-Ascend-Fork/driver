/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef __DRV_INTERNAL_H__
#define __DRV_INTERNAL_H__
#include "ascend_hal.h"

typedef struct tagDrvSpec {
    unsigned int aiCoreNum;
    unsigned int aiCpuNum;
    unsigned int aiCoreInuse;
    unsigned int aiCoreErrorMap;
    unsigned int aiCpuInuse;
    unsigned int aiCpuErrorMap;
    uint8_t aiCoreNumLevel;  /* 0 invalid */
    uint8_t aiCoreFreqLevel; /* 0 invalid */
} drvSpec_t;

typedef struct tagDrvCpuInfo {
    unsigned int ccpu_num;
    unsigned int ccpu_os_sched;
    unsigned int dcpu_num;
    unsigned int dcpu_os_sched;
    unsigned int aicpu_num;
    unsigned int aicpu_os_sched;
    unsigned int tscpu_num;
    unsigned int tscpu_os_sched;
    unsigned int comcpu_num;
    unsigned int comcpu_os_sched;
} drvCpuInfo_t;

typedef struct devdrv_core_utilization {
    unsigned int dev_id;
    unsigned int vfid;
    unsigned int core_type; /* 0: aicore  1: aivector 2:aicpu */
    unsigned int utilization;
} devdrv_core_utilization_t;

typedef enum devdrv_core_type {
    DEV_DRV_TYPE_AICORE = 0,
    DEV_DRV_TYPE_AIVECTOR,
    DEV_DRV_TYPE_AICPU,
    DEV_DRV_TYPE_MAX,
} devdrv_core_type_t;

#define DEVDRV_MAX_COMPUTING_POWER_TYPE 1
#define SOC_VERSION_LEN 32U
struct devdrv_device_info {
    uint8_t env_type; /**< 0, FPGA  1, EMU 2, ESL*/
    unsigned int ctrl_cpu_ip;
    unsigned int ctrl_cpu_id;
    unsigned int ctrl_cpu_core_num;
    unsigned long long ctrl_cpu_occupy_bitmap;
    unsigned int ctrl_cpu_endian_little;
    unsigned int ts_cpu_core_num;
    unsigned int ai_cpu_core_num;
    unsigned int ai_core_num;
    unsigned int ai_core_freq;
    unsigned int ai_cpu_core_id;
    unsigned int ai_core_id;
    unsigned long long aicpu_occupy_bitmap;
    unsigned int hardware_version;
    unsigned int ts_num;
    uint64_t cpu_system_count;
    uint64_t monotonic_raw_time_ns;
    unsigned int vector_core_num;
    unsigned int vector_core_freq;
    uint64_t computing_power[DEVDRV_MAX_COMPUTING_POWER_TYPE];
    unsigned int ffts_type;
    unsigned int chip_id;
    unsigned int die_id;
    unsigned int addr_mode;
    unsigned int mainboard_id;
    unsigned int host_device_connect_type;
    char soc_version[SOC_VERSION_LEN];
    unsigned long long aicore_bitmap[2];      /* support max 128 aicore */
    unsigned long long vector_core_bitmap[2]; /* support max 128 vector core */
    unsigned char product_type;
    unsigned char resv1[7];
    uint64_t real_time_ms;
    unsigned int l2_num;
    unsigned int hbm_num;
    unsigned int resv2[69];
};

typedef enum tagDrvWorkStatus {
    DRV_DEV_READY_INIT = 0x0,
    DRV_DEV_READY_EXIST,
    DRV_DEV_READY_WORK,
    DRV_DEV_READY_DMP_WORK,
    DRV_DEV_READY_RESERVED,
} drvWorkStatus_t;

typedef enum {
    SPI_FLASH = 0,
    UFS,
    PCIE,
    SSD,
    BOOTSTRAP_ID_MAX
} BOOTSTRAP_ID;

#define BITMAP_MAX_LEN 8 /* (Maximum core num:512) / sizeof(unsigned long long) */
typedef struct tagDrvHostAicpuInfo {
    unsigned int num;
    unsigned long long bitmap[BITMAP_MAX_LEN];
    unsigned int work_mode;
    unsigned int frequency;
} drvHostAicpuInfo_t;

/**
 * @ingroup driver
 * @brief get cpu information of device
 * @attention null
 * @param [in]  devId device id
 * @param [out]  drvspec cpu information
 * @return  0 for success, others for fail
 */
DLLEXPORT drvError_t drvGetDeviceSpec(uint32_t devId, drvSpec_t *drvspec);

/**
 * @ingroup driver
 * @brief Obtains the PCIe bus information of device
 * @attention null
 * @param [in] devId device id
 * @param [out] *bus  Returns the PCIe bus number
 * @param [out] *dev  Returns the PCIe device ID
 * @param [out] *func Returns the PCIe function ID
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t drvDeviceGetPcieInfo(uint32_t devId, int32_t *bus, int32_t *dev, int32_t *func);

/**
 * @ingroup driver
 * @brief Get device information, number and bit order of ctrl_cpu and ai_cpu, etc
 * @attention null
 * @param [in] devId  Device ID
 * @param [out] *info  See struct devdrv_device_info
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t drvGetDevInfo(uint32_t devId, struct devdrv_device_info *info);

/**
 * @ingroup driver
 * @brief Get CPU information by devId
 * @attention null
 * @param [in] devId  Device ID
 * @param [out] *cpuInfo  See drvCpuInfo_t
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t drvGetCpuInfo(uint32_t devId, drvCpuInfo_t *cpuInfo);

drvError_t drv_get_container_dev_ids(uint32_t *devices, uint32_t len, uint32_t *num);
DLLEXPORT drvError_t drvAllocateTflops(uint32_t devid, const char *vnpu_str, unsigned char *uuid,
                                       unsigned int uuid_size);
DLLEXPORT drvError_t drvStartContainerTflopsServe(uint32_t devid);

/**
 * @ingroup driver
 * @brief Start host and mini time zone synchronization thread
 * @attention Can only be called once by the daemon on the host, not multiple times
 * @return   0 for success, others for fail
 */
drvError_t drvStartTimeSyncServeHost(void);

/**
 * @ingroup driver
 * @brief get online device list interface
 * @attention when device boot,device list only get once,otherwise will get no device.
 * @param [in] *devBuf buffer for storing physical device id
 * @param [in] bufCnt buffer count,max count for store device id
 * @param [out] devCnt online device count.
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t halGetOnlineDevList(unsigned int *devBuf, unsigned int bufCnt, unsigned int *devCnt);

/**
 * @ingroup driver
 * @brief poll wait online device interface
 * @attention null.
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t halDevOnlinePollWait(void);

/**
 * @ingroup driver
 * @brief get bootstrap
 * @attention null.
 * @param [out]  *bootstrap : Stores bootstrap information, which is used to indicate whether UFS is started or SSD
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t drvGetBootstrap(unsigned int *bootstrap);

/* *
 * @ingroup driver
 * @brief get board info form host mem
 * @param [in]  dev_id : Logical device id
 * @param [out]  board_id : board id
 * @param [out]  pcb_id : pcb id
 * @param [out]  bom_id : bom id
 * @param [out]  slot_id : slot id
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t drvDeviceGetBoardFromHostMem(uint32_t devId, uint32_t *board_id, uint32_t *pcb_id,
                                                  uint32_t *bom_id, uint32_t *slot_id);

DLLEXPORT drvError_t drvGetDeviceStatus(unsigned int devId, unsigned int *device_status);

DLLEXPORT drvError_t drvUpdateDeviceStatus(unsigned int dev_id, unsigned int device_status);

DLLEXPORT drvError_t drvDeviceHealthStatus(uint32_t devId, unsigned int *healthStatus);

/**
 * @ingroup driver
 * @brief get device information
 * @attention null.
 * @param [in] device_id device id
 * @param [in] sub_cmd sub command type for chip device information
 * @param [out] buf output buffer
 * @param [in out] size input buffer size and output data size
 * @return   0 for success, others for fail
 */
DLLEXPORT drvError_t drvQueryDeviceInfo(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, void *buf,
                                        unsigned int *size);

DLLEXPORT drvError_t drvSetDeviceInfo(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd, const void *buf,
                                      unsigned int size);

DLLEXPORT drvError_t drvSetDeviceInfoToDmsHal(unsigned int dev_id, unsigned int main_cmd, unsigned int sub_cmd,
                                              const void *buf, unsigned int size);

DLLEXPORT int dms_get_ts_info_from_udis(unsigned int dev_id, const char *name, void *buf, unsigned int *size);

DLLEXPORT int dms_get_util_from_udis(unsigned int dev_id, const char *name, unsigned int *result_data);

DLLEXPORT int dms_get_npu_multi_util_from_udis(unsigned int dev_id, const char *name, void *buf, unsigned int *size);

DLLEXPORT int dms_get_cpu_rate_from_udis(unsigned int dev_id, const char *name, unsigned int *result_data);

DLLEXPORT drvError_t drv_get_cpu_topo(unsigned int dev_id, void *buf, unsigned int *size);

#endif
