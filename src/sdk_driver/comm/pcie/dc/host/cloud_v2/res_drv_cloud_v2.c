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

#include "ka_driver_pub.h"
#include "ka_barrier_pub.h"
#include "comm_kernel_interface.h"
#include "res_drv.h"
#include "devdrv_util.h"
#include "devdrv_ctrl.h"
#include "res_drv_cloud_v2.h"
#include "devdrv_feature.h"
#include "devdrv_mem_alloc.h"
#include "devdrv_smmu.h"

#ifndef DRV_UT

#define DEVDRV_PCIE_LINK_DOWN_STATE 0xFFFFFFFF
#define DEVDRV_PCIE_LINK_INFO_OFFSET 0x50
#define DEVDRV_CLOUD_V2_LINK_INFO_SPEED_SHIFT 16
#define DEVDRV_CLOUD_V2_LINK_INFO_SPEED_MASK 0xF
#define DEVDRV_CLOUD_V2_LINK_INFO_WIDTH_SHIFT 20
#define DEVDRV_CLOUD_V2_LINK_INFO_WIDTH_MASK 0x3F
#define DEVDRV_CLOUD_V2_LINK_STATUS_NORMAL 0x10

#define DEVDRV_HCCS_CACHELINE_SIZE 128
#define DEVDRV_HCCS_CACHELINE_MASK (DEVDRV_HCCS_CACHELINE_SIZE - 1)

#define DEVDRV_WAIT_MODE_SWITCH_RETRY_COUNT 300
#define DEVDRV_WAIT_MODE_SWITCH_TIME 100 /* 100ms */

#define DEVDRV_CLOUD_V2_P2P_SUPPORT_MAX_DEVICE 16

#define DEVDRV_CLOUD_V2_2DIE_DEVID_STRIDE_2 2
#define DEVDRV_CLOUD_V2_2DIE_2PCIE_MAINBOARD 0xE
#define DEVDRV_CLOUD_V2_HW_INFO_SRAM_OFFSET 0x5800
#define DEVDRV_CLOUD_V2_2DIE_BIT 0x80
#define DEVDRV_CLOUD_V2_BOARD_TYPE_MASK 0x7
#define DEVDRV_CLOUD_V2_BOARD_TYPE_OFFSET 4
#define DEVDRV_CLOUD_V2_1DIE_EVB 0x0
#define DEVDRV_CLOUD_V2_1DIE_TRAIN_PCIE_CARD 0x1
#define DEVDRV_CLOUD_V2_1DIE_INFER_PCIE_CARD 0x2
#define DEVDRV_CLOUD_V2_1DIE_SINGLE_MODULE 0x3
#define DEVDRV_CLOUD_V2_1DIE_DOUBLE_MODULE 0x5
#define DEVDRV_CLOUD_V2_2DIE_EVB 0x0
#define DEVDRV_CLOUD_V2_2DIE_OAM_MODULE 0x1
#define DEVDRV_CLOUD_V2_2DIE_HAM_MODULE_A 0x3
#define DEVDRV_CLOUD_V2_2DIE_HAM_MODULE_B 0x5

#define INVALID_CHIP_ID 0xFF
#define INVALID_MULTI_DIE 0xFF
#define DEVDRV_INIT_HWINFO_RETRY_TIME 60  /* wait 60 times */
#define DEVDRV_INIT_HWINFO_WAIT_TIME 1000 /* wait 1000ms */

/* bios set, host use */
typedef struct devdrv_hw_info {
    u8 chip_id;
    u8 multi_chip;
    u8 multi_die;
    u8 mainboard_id;
    u16 addr_mode;
    u16 board_id;

    u8 version;
    u8 connect_type;
    u16 hccs_hpcs_bitmap;

    u16 server_id;
    u16 scale_type;
    u32 super_pod_id;

    u32 reserved;
} devdrv_hw_info_t;
STATIC devdrv_hw_info_t g_hw_info = {0};

enum devdrv_board_type {
    BOARD_CLOUD_V2_MODE_1P = 0,
    BOARD_CLOUD_V2_MODE_EVB_2P,
    BOARD_CLOUD_V2_MODE_PCIE_4P,
    BOARD_CLOUD_V2_MODE_8P,
    BOARD_CLOUD_V2_MODE_DUO_8P,
    BOARD_CLOUD_V2_MODE_AK_2P,
    BOARD_CLOUD_V2_MODE_AK_4P,
    BOARD_CLOUD_V2_MODE_OAM_8P,
    BOARD_MAX_MODE_NUM
};

#define BOARD_CLOUD_V2_8P_MAX_DEV_NUM 16

#define PCI_BAR_RSV_MEM 0
#define PCI_BAR_IO 2
#define PCI_BAR_MEM 4

#define DEVDRV_IO_STARS_SQCQ_OFFSET 0x8008000
#define DEVDRV_IO_STARS_SQCQ_SIZE 0x8000000
#define DEVDRV_IO_TS_DB_OFFSET 0x8000
#define DEVDRV_IO_TS_DB_SIZE 0x4000000
#define DEVDRV_IO_TS_SRAM_OFFSET 0x18308000
#define DEVDRV_IO_TS_SRAM_SIZE 0x20000
#define DEVDRV_IO_LOAD_SRAM_OFFSET 0x18208000
#define DEVDRV_IO_LOAD_SRAM_SIZE 0x40000
#define DEVDRV_IO_STARS_INTR_OFFSET 0x10808000
#define DEVDRV_IO_STARS_INTR_SIZE 0x800000
#define DEVDRV_IO_STARS_SQCQ_INTR_OFFSET 0x10c08000
#define DEVDRV_IO_STARS_SQCQ_INTR_SIZE 0x200000
#define DEVDRV_IO_STARS_TOPIC_SCHED_OFFSET 0x4808000
#define DEVDRV_IO_STARS_TOPIC_SCHED_SIZE 0x800000
#define DEVDRV_IO_STARS_CDQM_OFFSET 0x5008000
#define DEVDRV_IO_STARS_CDQM_SIZE 0x1000000
#define DEVDRV_IO_STARS_CNT_NOTIFY_OFFSET 0x18508000
#define DEVDRV_IO_STARS_CNT_NOTIFY_SIZE 0x1000

#define DEVDRV_IO_HWTS_NOTIFY_OFFSET 0x4108000
#define DEVDRV_IO_HWTS_NOTIFY_SIZE 0x100000
#define DEVDRV_IO_HWTS_EVENT_OFFSET 0x4208000
#define DEVDRV_IO_HWTS_EVENT_SIZE 0x100000

#define DEVDRV_SOC_DOORBELL_OFFSET 0x18408000
#define DEVDRV_IEP_SDI0_SIZE 0x2000

#define DEVDRV_IEP_DMA_OFFSET 0
#define DEVDRV_IEP_DMA_SIZE 0x8000
#define DEVDRV_IEP_DMA_CHAN_OFFSET 0x2000
#define DEVDRV_VF_DMA_OFFSET 0

/* mem base */
#define DEVDRV_MDEV_RESERVE_MEM_SIZE 0x20000000
#define DEVDRV_PF_MDEV_VPC_OFFSET 0
#define DEVDRV_PF_MDEV_VPC_SIZE 0x4020000
#define DEVDRV_PF_MDEV_DVPP_OFFSET DEVDRV_PF_MDEV_VPC_SIZE
#define DEVDRV_PF_MDEV_DVPP_SIZE 0x1800000

#define DEVDRV_PEH_RESERVE_MEM_MSG_OFFSET 0x10000
#define DEVDRV_RESERVE_MEM_MSG_OFFSET 0
#define DEVDRV_RESERVE_MEM_MSG_SIZE (1 * 1024 * 1024)

STATIC u64 g_reserver_mem_msg_offset = DEVDRV_RESERVE_MEM_MSG_OFFSET;
#define DEVDRV_HBOOT_L3D_SRAM_OFFSET (g_reserver_mem_msg_offset + 0x1AC00000)
#define DEVDRV_HBOOT_L3D_SRAM_SIZE 0x200000

#define DEVDRV_RESERVE_MEM_TEST_OFFSET (g_reserver_mem_msg_offset + DEVDRV_RESERVE_MEM_MSG_SIZE - 4 * 1024)
#define DEVDRV_RESERVE_MEM_TEST_SIZE (4 * 1024)

#define DEVDRV_RESERVE_MEM_TOTAL_SIZE (24 * 1024 * 1024)

#define DEVDRV_RESERVE_MEM_TS_SQ_OFFSET 0
#define DEVDRV_RESERVE_MEM_TS_SQ_SIZE 0

#define DEVDRV_RESERVE_BBOX_OFFSET (g_reserver_mem_msg_offset + DEVDRV_RESERVE_MEM_TOTAL_SIZE)
#define DEVDRV_RESERVE_BBOX_SIZE 0x200000

#define DEVDRV_RESERVE_BBOX_ALL_SIZE 0xB00000
#define DEVDRV_RESERVE_BBOX_HDR_ADDR 0x800000

#define DEVDRV_BBOX_DDR_DUMP_OFFSET (DEVDRV_RESERVE_BBOX_OFFSET + DEVDRV_RESERVE_BBOX_SIZE)
#define DEVDRV_BBOX_DDR_DUMP_SIZE 0x900000

#define DEVDRV_TS_LOG_OFFSET (g_reserver_mem_msg_offset + 0x2500000)
#define DEVDRV_TS_LOG_SIZE 0x100000

#define DEVDRV_CHIP_DFX_FULL_OFFSET (g_reserver_mem_msg_offset + 0x2700000)
#define DEVDRV_CHIP_DFX_FULL_SIZE 0x800000

#define DEVDRV_RESERVE_HDR_OFFSET (DEVDRV_RESERVE_BBOX_OFFSET + DEVDRV_RESERVE_BBOX_SIZE + DEVDRV_RESERVE_BBOX_HDR_ADDR)
#define DEVDRV_RESERVE_HDR_SIZE 0x80000

#define DEVDRV_BBOX_KDUMP_OFFSET (DEVDRV_RESERVE_HDR_OFFSET + DEVDRV_RESERVE_HDR_SIZE)
#define DEVDRV_BBOX_KDUMP_SIZE 0x80000

#define DEVDRV_BBOX_VMCORE_OFFSET (DEVDRV_BBOX_KDUMP_OFFSET + sizeof(unsigned int))
#define DEVDRV_BBOX_VMCORE_SIZE 8

#define DEVDRV_RESERVE_RESERVE1_OFFSET (DEVDRV_RESERVE_BBOX_OFFSET + DEVDRV_RESERVE_BBOX_ALL_SIZE)
#define DEVDRV_RESERVE_RESERVE1_SIZE 0x0

#define DEVDRV_RESERVE_STARS_TOPIC_SCHED_OFFSET (0x1B000000 + g_reserver_mem_msg_offset)
#define DEVDRV_RESERVE_STARS_TOPIC_SCHED_SIZE 0xC0000

#define DEVDRV_RESERVE_DEVMNG_INFO_OFFSET 0x2400000
#define DEVDRV_RESERVE_DEVMNG_INFO_SIZE 0x100000

#define DEVDRV_RESERVE_HBM_TOTAL_SIZE 0x3100000 /* 49 * 1024 * 1024 */

#define DEVDRV_RESERVE_S2S_MSG_OFFSET (0x1C000000 + g_reserver_mem_msg_offset)
#define DEVDRV_RESERVE_S2S_MSG_SIZE 0x1000000 /* 16 * 1024 * 1024 */

#define DEVDRV_RESERVE_IMU_LOG_SIZE 0x0
#define DEVDRV_RESERVE_IMU_LOG_OFFSET (DEVDRV_RESERVE_HBM_TOTAL_SIZE - DEVDRV_RESERVE_IMU_LOG_SIZE)

#define DEVDRV_RESERVE_UEFI_SIZE 0x200000
#define DEVDRV_RESERVE_UEFI_OFFSET (DEVDRV_RESERVE_IMU_LOG_OFFSET - DEVDRV_RESERVE_UEFI_SIZE)

#define DEVDRV_RESERVE_RESERVE2_SIZE 0x0

#define DEVDRV_RESERVE_REG_SIZE 0x800000
#define DEVDRV_RESERVE_REG_OFFSET (DEVDRV_RESERVE_UEFI_OFFSET - DEVDRV_RESERVE_RESERVE2_SIZE - DEVDRV_RESERVE_REG_SIZE)

#define DEVDRV_IEP_SDI0_DB_OFFSET 0x1000
#define DEVDRV_DB_IOMAP_SIZE 0x1000

/* vf mem */
#define DEVDRV_VF_MDEV_RESERVE_MEM_SIZE 0x6000000
#define DEVDRV_VF_MDEV_VPC_OFFSET 0
#define DEVDRV_VF_MDEV_VPC_SIZE 0x4020000
#define DEVDRV_VF_MDEV_DVPP_OFFSET DEVDRV_PF_MDEV_VPC_SIZE
#define DEVDRV_VF_MDEV_DVPP_SIZE 0x1800000

#define DEVDRV_VF_MEM_MSG_OFFSET 0
#define DEVDRV_VF_MEM_MSG_SIZE (1 * 1024 * 1024)

#define DEVDRV_VF_RESERVE_MEM_TEST_OFFSET (DEVDRV_VF_MEM_MSG_OFFSET + DEVDRV_VF_MEM_MSG_SIZE - 4 * 1024)
#define DEVDRV_VF_RESERVE_MEM_TEST_SIZE (4 * 1024)

/* VF IO BASE */
#define DEVDRV_VF_IO_STARS_REG_OFFSET 0X8000
#define DEVDRV_VF_IO_STARS_REG_SIZE (32 * 1024 * 1024)

#define DEVDRV_VF_IO_TS_DB_OFFSET 0X2008000
#define DEVDRV_VF_IO_TS_DB_SIZE (4 * 1024 * 1024)

#define DEVDRV_VF_IO_HWTS_NOTIFY_OFFSET 0X2408000
#define DEVDRV_VF_IO_HWTS_NOTIFY_SIZE (64 * 1024)

#define DEVDRV_VF_IEP_DMA_OFFSET 0X2508000
#define DEVDRV_VF_IEP_DMA_SIZE 0X100

#define DEVDRV_VF_IEP_SDI0_OFFSET 0X2608000
#define DEVDRV_VF_IEP_SDI0_SIZE 0x10000

#define DEVDRV_VF_IO_TS_SRAM_OFFSET 0X2708000
#define DEVDRV_VF_IO_TS_SRAM_SIZE (2 * 1024)

#define DEVDRV_VF_IO_SOC_DOORBELL_OFFSET 0X2808000
#define DEVDRV_VF_IO_SOC_DOORBELL_SIZE (4 * 1024)

#define DEVDRV_VF_IO_LOAD_SRAM_OFFSET 0X2908000
#define DEVDRV_VF_IO_LOAD_SRAM_SIZE (16 * 1024)

/* VF STARS REG */
#define DEVDRV_VF_IO_STARS_SQCQ_OFFSET 0X0
#define DEVDRV_VF_IO_STARS_SQCQ_SIZE (8 * 1024 * 1024)

#define DEVDRV_VF_IO_CDQ_INFO_OFFSET 0X800000
#define DEVDRV_VF_IO_CDQ_INFO_SIZE (2 * 1024 * 1024)

#define DEVDRV_VF_IO_RTSQINT_OFFSET 0XA00000
#define DEVDRV_VF_IO_RTSQINT_SIZE (2 * 1024 * 1024)

#define DEVDRV_VF_IO_ACSQINT_OFFSET 0XC00000
#define DEVDRV_VF_IO_ACSQINT_SIZE (2 * 1024 * 1024)

#define DEVDRV_VF_IO_CQINT_OFFSET 0XD00000
#define DEVDRV_VF_IO_CQINT_SIZE (2 * 1024 * 1024)

#define DEVDRV_VF_IO_TOPIC_INFO_OFFSET 0X1000000
#define DEVDRV_VF_IO_TOPIC_INFO_SIZE (2 * 1024 * 1024)

#define DEVDRV_VF_IO_RSV_OFFSET 0X1000000
#define DEVDRV_VF_IO_RSV_SIZE 0x10000

/* ************ interrupt defined for normal host ************* */
#define DEVDRV_MSI_X_MAX_VECTORS 256
#define DEVDRV_MSI_X_MIN_VECTORS 128

/* device os load notify use irq vector 0, later 0 also used to admin msg chan */
#define DEVDRV_LOAD_MSI_X_VECTOR_NUM 0

/* irq used to msg trans, a msg chan need two vector. one for tx finish, the other for rx msg.
   msg chan 0 is used to admin(chan 0) role */
#define DEVDRV_MSG_MSI_X_VECTOR_BASE 0
#define DEVDRV_MSG_MSI_X_VECTOR_NUM 48

/* irq used to dma, a dma chan need 22 vector. one for cq, the other for err.
  host support 11 dma chan witch is related to enum devdrv_dma_data_type */
#define DEVDRV_DMA_MSI_X_VECTOR_BASE 48
#define DEVDRV_DMA_MSI_X_VECTOR_NUM 32

/* irq used to tsdrv: mailbox, log, stars cq */
#define DEVDRV_TSDRV_MSI_X_VECTOR_BASE 80
#define DEVDRV_TSDRV_MSI_X_VECTOR_NUM 40

#define DEVDRV_TOPIC_SCHED_MSI_X_VECTOR_BASE 125
#define DEVDRV_TOPIC_SCHED_MSI_X_VECTOR_NUM 1

#define DEVDRV_CDQM_MSI_X_VECTOR_BASE 126
#define DEVDRV_CDQM_MSI_X_VECTOR_NUM 2

/* msg chan irq section2 */
#define DEVDRV_MSG_MSI_X_VECTOR_2_BASE 128

/* ************ interrupt defined for normal host ************* */
#define DEVDRV_MSI_X_MDEV_VF_MAX_VECTORS 256
#define DEVDRV_MSI_X_MDEV_VF_MIN_VECTORS 256
#define DEVDRV_MSI_X_VF_MAX_VECTORS 64
#define DEVDRV_MSI_X_VF_MIN_VECTORS 64

/* device os load notify use irq vector 0, later 0 also used to admin msg chan */
#define DEVDRV_LOAD_MSI_X_VF_VECTOR_NUM 0

/* irq used to msg trans, a msg chan need two vector. one for tx finish, the other for rx msg.
   msg chan 0 is used to admin(chan 0) role */
#define DEVDRV_MSG_MSI_X_VF_VECTOR_BASE 0
#define DEVDRV_MSG_MSI_X_VF_VECTOR_NUM 18

/* irq used to dma, a dma chan need 22 vector. one for cq, the other for err.
  host support 11 dma chan witch is related to enum devdrv_dma_data_type */
#define DEVDRV_DMA_MSI_X_VF_VECTOR_BASE 18
#define DEVDRV_DMA_MSI_X_VF_VECTOR_NUM 24

#define DEVDRV_TOPIC_SCHED_MSI_X_VF_VECTOR_BASE 42
#define DEVDRV_TOPIC_SCHED_MSI_X_VF_VECTOR_NUM 2

#define DEVDRV_CDQM_MSI_X_VF_VECTOR_BASE 44
#define DEVDRV_CDQM_MSI_X_VF_VECTOR_NUM 2

/* irq used to tsdrv: mailbox, log, stars cq */
#define DEVDRV_TSDRV_MSI_X_VF_VECTOR_BASE 46
#define DEVDRV_TSDRV_MSI_X_VF_VECTOR_NUM 18

/* msg chan irq section2 */
#define DEVDRV_MSG_MSI_X_VF_VECTOR_2_BASE 64

/* peh msi-x, peh vf only has 33 msi-x irq */
#define DEVDRV_MSI_X_PEH_VF_MAX_VECTORS 33
#define DEVDRV_MSI_X_PEH_VF_MIN_VECTORS 33

/* device os load notify use irq vector 0, later 0 also used to admin msg chan */
#define DEVDRV_LOAD_MSI_X_PEH_VF_VECTOR_NUM 0

/* irq used to msg trans, a msg chan need two vector. one for tx finish, the other for rx msg.
   msg chan 0 is used to admin(chan 0) role */
#define DEVDRV_MSG_MSI_X_PEH_VF_VECTOR_BASE 0
#define DEVDRV_MSG_MSI_X_PEH_VF_VECTOR_NUM 18

/* irq used to dma, a dma chan need 22 vector. one for cq, the other for err.
  host support 11 dma chan witch is related to enum devdrv_dma_data_type */
#define DEVDRV_DMA_MSI_X_PEH_VF_VECTOR_BASE 18
#define DEVDRV_DMA_MSI_X_PEH_VF_VECTOR_NUM 10

#define DEVDRV_TOPIC_SCHED_MSI_X_PEH_VF_VECTOR_BASE 28
#define DEVDRV_TOPIC_SCHED_MSI_X_PEH_VF_VECTOR_NUM 1

#define DEVDRV_CDQM_MSI_X_PEH_VF_VECTOR_BASE 29
#define DEVDRV_CDQM_MSI_X_PEH_VF_VECTOR_NUM 0

/* irq used to tsdrv: mailbox, log, stars cq */
#define DEVDRV_TSDRV_MSI_X_PEH_VF_VECTOR_BASE 29
#define DEVDRV_TSDRV_MSI_X_PEH_VF_VECTOR_NUM 4

/* msg chan irq section2 */
#define DEVDRV_MSG_MSI_X_PEH_VF_VECTOR_2_BASE 33

#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define DMA_CHAN_REMOTE_USED_NUM 2
#define DMA_CHAN_REMOTE_USED_START_INDEX 2
#define DMA_CHAN_BAR_SPACE_USED_START_INDEX 0
#else
#define DMA_CHAN_REMOTE_USED_NUM 13
#define DMA_CHAN_REMOTE_USED_START_INDEX 25
#define DMA_CHAN_BAR_SPACE_USED_START_INDEX 0
#endif

#define HOST_VF_DMA_MASK 0x3FFC000000ULL /* channal 26~37 for host VF use, 25 for PF only */

#define DEVDRV_MAX_DMA_CH_SQ_DEPTH 0x10000
#define DEVDRV_MAX_DMA_CH_CQ_DEPTH 0x10000
#define DEVDRV_DMA_CH_SQ_DESC_RSV 0x400

#define CLOUD_V2_BLOCKS_NUM 11
#define CLOUD_V2_MODULE_NUM 8
#define CLOUD_V2_VF_VM_MODULE_NUM 6

#define DEVDRV_MAX_MSG_PF_CHAN_CNT 46
#define DEVDRV_MAX_MSG_VF_CHAN_CNT 16
/* msg chan cnt for modules */
#define DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX 1                         /* non_trans:1 */
#define DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX 1                            /* non_trans:1 */
#define DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX 1                           /* non_trans:1 */
#define DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX 1                          /* non_trans:1 */
#define DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX 1                         /* non_trans:1 */
#define DEVDRV_TSDRV_MSG_CHAN_CNT_MAX 1                               /* non_trans:1 */
#define DEVDRV_DEV_HDC_PF_MSG_CHAN_CNT_MAX 25                         /* trans:24 non_trans:1 */
#define DEVDRV_QUEUE_MSG_CHAN_CNT_MAX 1                               /* non_trans:1 */
#define DEVDRV_S2S_MSG_CHAN_CNT_MAX DEVDRV_S2S_NON_TRANS_MSG_CHAN_NUM /* non_trans:8 */

#define DEVDRV_DEV_HDC_VF_MSG_CHAN_CNT_MAX 3 /* trans:2 non_trans:1 */

#define CLOUD_V2_NVME_LOW_LEVEL_DB_IRQ_NUM 2
#define CLOUD_V2_NVME_DB_IRQ_STRDE 8

STATIC unsigned int devdrv_pf_msg_chan_cnt_cloud_v2[devdrv_msg_client_max] = {
    DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX, /* used for pcivnic */
    DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX,    /* used for test */
    DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX,   /* used for svm */
    DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX,  /* used for common */
    DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX, /* used for device manager */
    DEVDRV_TSDRV_MSG_CHAN_CNT_MAX,       /* used for tsdrv */
    DEVDRV_DEV_HDC_PF_MSG_CHAN_CNT_MAX,  /* used for hdc */
    DEVDRV_QUEUE_MSG_CHAN_CNT_MAX,       /* used for queue */
    DEVDRV_S2S_MSG_CHAN_CNT_MAX,         /* used for s2s */
};

STATIC unsigned int devdrv_vf_msg_chan_cnt_cloud_v2[devdrv_msg_client_max] = {
    DEVDRV_PCIVNIC_DEV_MSG_CHAN_CNT_MAX, /* used for pcivnic */
    DEVDRV_SMMU_DEV_MSG_CHAN_CNT_MAX,    /* used for test */
    DEVDRV_DEVMM_DEV_MSG_CHAN_CNT_MAX,   /* used for svm */
    DEVDRV_COMMON_DEV_MSG_CHAN_CNT_MAX,  /* used for common */
    DEVDRV_DEV_MAMAGER_MSG_CHAN_CNT_MAX, /* used for device manager */
    DEVDRV_TSDRV_MSG_CHAN_CNT_MAX,       /* used for tsdrv */
    DEVDRV_DEV_HDC_VF_MSG_CHAN_CNT_MAX,  /* used for hdc */
    DEVDRV_QUEUE_MSG_CHAN_CNT_MAX,       /* used for queue */
};

STATIC struct devdrv_load_file_cfg cloud_v2_910b_file[CLOUD_V2_BLOCKS_NUM] = {
    {
        .file_name = "/driver/device/ascend_910b_syscfg.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910b_hbm.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910b_imp.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910b_imu.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910b_device_boot.img",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910b_hsm.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910b_tee.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910b_device_sw.img",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910b_device_sw.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910b_device_config.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_cloud_v2.crl",
        .file_type = DEVDRV_NON_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
};

STATIC struct devdrv_load_file_cfg cloud_v2_910_93_file[CLOUD_V2_BLOCKS_NUM] = {
    {
        .file_name = "/driver/device/ascend_910_93_syscfg.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910_93_hbm.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910_93_imp.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910_93_imu.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910_93_device_boot.img",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910_93_hsm.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910_93_tee.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910_93_device_sw.img",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910_93_device_sw.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_910_93_device_config.bin",
        .file_type = DEVDRV_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
    {
        .file_name = "/driver/device/ascend_cloud_v2.crl",
        .file_type = DEVDRV_NON_CRITICAL_FILE,
        .fail_mode = DEVDRV_NON_NOTICE,
    },
};

STATIC struct devdrv_depend_module cloud_v2_module[CLOUD_V2_MODULE_NUM] = {
    {
        .module_name = "asdrv_fms",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "asdrv_buff",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "asdrv_queue",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "asdrv_dpa",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "asdrv_vnic",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "ascend_soc_platform",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "asdrv_trs",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "ts_agent",
        .status = DEVDRV_MODULE_UNPROBED,
    },
};

STATIC struct devdrv_depend_module cloud_v2_vf_vm_module[CLOUD_V2_VF_VM_MODULE_NUM] = {
    {
        .module_name = "asdrv_fms",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "asdrv_queue",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "asdrv_dpa",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "asdrv_vnic",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "ascend_soc_platform",
        .status = DEVDRV_MODULE_UNPROBED,
    },
    {
        .module_name = "ts_agent_vm",
        .status = DEVDRV_MODULE_UNPROBED,
    },
};

void devdrv_cloud_v2_init_pf_bar_addr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.msg_db.addr = pci_ctrl->io_phy_base + DEVDRV_SOC_DOORBELL_OFFSET + DEVDRV_IEP_SDI0_DB_OFFSET;
    pci_ctrl->res.msg_db.size = DEVDRV_DB_IOMAP_SIZE;

    pci_ctrl->res.msg_mem.addr = pci_ctrl->rsv_mem_phy_base + g_reserver_mem_msg_offset;
    pci_ctrl->res.msg_mem.size = DEVDRV_RESERVE_MEM_MSG_SIZE;

    if (pci_ctrl->addr_mode == DEVDRV_ADMODE_FULL_MATCH) {
        pci_ctrl->res.s2s_msg.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_S2S_MSG_OFFSET;
        pci_ctrl->res.s2s_msg.size = DEVDRV_RESERVE_S2S_MSG_SIZE;
    } else {
        pci_ctrl->res.s2s_msg.addr = 0;
        pci_ctrl->res.s2s_msg.size = 0;
    }

    pci_ctrl->res.stars_sqcq.addr = pci_ctrl->io_phy_base + DEVDRV_IO_STARS_SQCQ_OFFSET;
    pci_ctrl->res.stars_sqcq.size = DEVDRV_IO_STARS_SQCQ_SIZE;

    pci_ctrl->res.stars_sqcq_intr.addr = pci_ctrl->io_phy_base + DEVDRV_IO_STARS_SQCQ_INTR_OFFSET;
    pci_ctrl->res.stars_sqcq_intr.size = DEVDRV_IO_STARS_SQCQ_INTR_SIZE;

    pci_ctrl->res.stars_topic_sched.addr = pci_ctrl->io_phy_base + DEVDRV_IO_STARS_TOPIC_SCHED_OFFSET;
    pci_ctrl->res.stars_topic_sched.size = DEVDRV_IO_STARS_TOPIC_SCHED_SIZE;

    pci_ctrl->res.stars_topic_sched_cqe.addr = 0;
    pci_ctrl->res.stars_topic_sched_cqe.size = 0;

    pci_ctrl->res.stars_topic_sched_rsv_mem.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_STARS_TOPIC_SCHED_OFFSET;
    pci_ctrl->res.stars_topic_sched_rsv_mem.size = DEVDRV_RESERVE_STARS_TOPIC_SCHED_SIZE;

    pci_ctrl->res.stars_cdqm.addr = pci_ctrl->io_phy_base + DEVDRV_IO_STARS_CDQM_OFFSET;
    pci_ctrl->res.stars_cdqm.size = DEVDRV_IO_STARS_CDQM_SIZE;

    pci_ctrl->res.stars_intr.addr = pci_ctrl->io_phy_base + DEVDRV_IO_STARS_INTR_OFFSET;
    pci_ctrl->res.stars_intr.size = DEVDRV_IO_STARS_INTR_SIZE;

    pci_ctrl->res.ts_db.addr = pci_ctrl->io_phy_base + DEVDRV_IO_TS_DB_OFFSET;
    pci_ctrl->res.ts_db.size = DEVDRV_IO_TS_DB_SIZE;

    pci_ctrl->res.ts_sram.addr = pci_ctrl->io_phy_base + DEVDRV_IO_TS_SRAM_OFFSET;
    pci_ctrl->res.ts_sram.size = DEVDRV_IO_TS_SRAM_SIZE;

    pci_ctrl->res.ts_sq.addr = pci_ctrl->mem_phy_base + DEVDRV_RESERVE_MEM_TS_SQ_OFFSET;
    pci_ctrl->res.ts_sq.size = DEVDRV_RESERVE_MEM_TS_SQ_SIZE;

    pci_ctrl->res.test.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_MEM_TEST_OFFSET;
    pci_ctrl->res.test.size = DEVDRV_RESERVE_MEM_TEST_SIZE;

    pci_ctrl->res.load_sram.addr = pci_ctrl->io_phy_base + DEVDRV_IO_LOAD_SRAM_OFFSET;
    pci_ctrl->res.load_sram.size = DEVDRV_IO_LOAD_SRAM_SIZE;

    pci_ctrl->res.bbox.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_BBOX_OFFSET;
    pci_ctrl->res.bbox.size = DEVDRV_RESERVE_BBOX_SIZE;

    pci_ctrl->res.imu_log.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_IMU_LOG_OFFSET;
    pci_ctrl->res.imu_log.size = DEVDRV_RESERVE_IMU_LOG_SIZE;

    pci_ctrl->res.ts_notify.addr = pci_ctrl->io_phy_base + DEVDRV_IO_HWTS_NOTIFY_OFFSET;
    pci_ctrl->res.ts_notify.size = DEVDRV_IO_HWTS_NOTIFY_SIZE;
    pci_ctrl->res.ts_event.addr = pci_ctrl->io_phy_base + DEVDRV_IO_HWTS_EVENT_OFFSET;
    pci_ctrl->res.ts_event.size = DEVDRV_IO_HWTS_EVENT_SIZE;

    pci_ctrl->res.hdr.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_RESERVE_HDR_OFFSET;
    pci_ctrl->res.hdr.size = DEVDRV_RESERVE_HDR_SIZE;

    pci_ctrl->res.reg_sram.addr = pci_ctrl->io_phy_base + DEVDRV_IO_LOAD_SRAM_OFFSET;
    pci_ctrl->res.reg_sram.size = DEVDRV_IO_LOAD_SRAM_SIZE;

    pci_ctrl->res.l3d_sram.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_HBOOT_L3D_SRAM_OFFSET;
    pci_ctrl->res.l3d_sram.size = DEVDRV_HBOOT_L3D_SRAM_SIZE;

    pci_ctrl->res.kdump.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_BBOX_KDUMP_OFFSET;
    pci_ctrl->res.kdump.size = DEVDRV_BBOX_KDUMP_SIZE;

    pci_ctrl->res.vmcore.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_BBOX_VMCORE_OFFSET;
    pci_ctrl->res.vmcore.size = DEVDRV_BBOX_VMCORE_SIZE;

    pci_ctrl->res.bbox_ddr_dump.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_BBOX_DDR_DUMP_OFFSET;
    pci_ctrl->res.bbox_ddr_dump.size = DEVDRV_BBOX_DDR_DUMP_SIZE;

    pci_ctrl->res.ts_log.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_TS_LOG_OFFSET;
    pci_ctrl->res.ts_log.size = DEVDRV_TS_LOG_SIZE;

    pci_ctrl->res.chip_dfx.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_CHIP_DFX_FULL_OFFSET;
    pci_ctrl->res.chip_dfx.size = DEVDRV_CHIP_DFX_FULL_SIZE;

    /* not support */
    pci_ctrl->res.vpc.size = 0;
    pci_ctrl->res.dvpp.size = 0;
    pci_ctrl->res.hwts.size = 0;
    pci_ctrl->res.vf_bandwidth.addr = 0;
    pci_ctrl->res.vf_bandwidth.size = 0;
    pci_ctrl->res.ts_share_mem.addr = 0;
    pci_ctrl->res.ts_share_mem.size = 0;

    /* pcie reserve bar space for  other modules */
    pci_ctrl->res.tsdrv_resv.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_TSDRV_RESV_OFFSET;
    pci_ctrl->res.tsdrv_resv.size = DEVDRV_RESERVE_TSDRV_RESV_SIZE;
    pci_ctrl->res.devmng_resv.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_DEVMNG_RESV_OFFSET;
    pci_ctrl->res.devmng_resv.size = DEVDRV_RESERVE_DEVMNG_RESV_SIZE;
    pci_ctrl->res.devmng_info_mem.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_DEVMNG_INFO_OFFSET;
    pci_ctrl->res.devmng_info_mem.size = DEVDRV_RESERVE_DEVMNG_INFO_SIZE;
    pci_ctrl->res.hbm_ecc_mem.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_HBM_ECC_OFFSET;
    pci_ctrl->res.hbm_ecc_mem.size = DEVDRV_RESERVE_HBM_ECC_SIZE;
}

void devdrv_cloud_v2_init_vf_bar_addr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.msg_db.addr = pci_ctrl->io_phy_base + DEVDRV_VF_IO_SOC_DOORBELL_OFFSET;
    pci_ctrl->res.msg_db.size = DEVDRV_VF_IO_SOC_DOORBELL_SIZE;

    pci_ctrl->res.msg_mem.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_VF_MEM_MSG_OFFSET + g_reserver_mem_msg_offset;
    pci_ctrl->res.msg_mem.size = DEVDRV_VF_MEM_MSG_SIZE;

    pci_ctrl->res.stars_sqcq.addr = pci_ctrl->io_phy_base + DEVDRV_VF_IO_STARS_REG_OFFSET +
                                    DEVDRV_VF_IO_STARS_SQCQ_OFFSET;
    pci_ctrl->res.stars_sqcq.size = DEVDRV_VF_IO_STARS_SQCQ_SIZE;

    pci_ctrl->res.stars_sqcq_intr.addr = pci_ctrl->io_phy_base + DEVDRV_VF_IO_STARS_REG_OFFSET +
                                         DEVDRV_VF_IO_CQINT_OFFSET;
    pci_ctrl->res.stars_sqcq_intr.size = DEVDRV_VF_IO_CQINT_SIZE;

    pci_ctrl->res.stars_topic_sched.addr = pci_ctrl->io_phy_base + DEVDRV_VF_IO_STARS_REG_OFFSET +
                                           DEVDRV_VF_IO_TOPIC_INFO_OFFSET;
    pci_ctrl->res.stars_topic_sched.size = DEVDRV_VF_IO_TOPIC_INFO_SIZE;

    pci_ctrl->res.stars_topic_sched_cqe.addr = 0;
    pci_ctrl->res.stars_topic_sched_cqe.size = 0;

    pci_ctrl->res.stars_topic_sched_rsv_mem.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_VF_IO_RSV_OFFSET +
                                                   g_reserver_mem_msg_offset;
    pci_ctrl->res.stars_topic_sched_rsv_mem.size = DEVDRV_VF_IO_RSV_SIZE;

    pci_ctrl->res.stars_cdqm.addr = pci_ctrl->io_phy_base + DEVDRV_VF_IO_STARS_REG_OFFSET +
                                    DEVDRV_VF_IO_CDQ_INFO_OFFSET;
    pci_ctrl->res.stars_cdqm.size = DEVDRV_VF_IO_CDQ_INFO_SIZE;

    pci_ctrl->res.stars_intr.addr = pci_ctrl->io_phy_base + DEVDRV_VF_IO_STARS_REG_OFFSET + DEVDRV_VF_IO_RTSQINT_OFFSET;
    pci_ctrl->res.stars_intr.size = DEVDRV_VF_IO_RTSQINT_SIZE;

    pci_ctrl->res.ts_db.addr = pci_ctrl->io_phy_base + DEVDRV_VF_IO_TS_DB_OFFSET;
    pci_ctrl->res.ts_db.size = DEVDRV_VF_IO_TS_DB_SIZE;

    pci_ctrl->res.ts_sram.addr = pci_ctrl->io_phy_base + DEVDRV_VF_IO_TS_SRAM_OFFSET;
    pci_ctrl->res.ts_sram.size = DEVDRV_VF_IO_TS_SRAM_SIZE;

    pci_ctrl->res.test.addr = pci_ctrl->rsv_mem_phy_base + DEVDRV_VF_RESERVE_MEM_TEST_OFFSET +
                              g_reserver_mem_msg_offset;
    pci_ctrl->res.test.size = DEVDRV_VF_RESERVE_MEM_TEST_SIZE;

    pci_ctrl->res.load_sram.addr = pci_ctrl->io_phy_base + DEVDRV_VF_IO_LOAD_SRAM_OFFSET;
    pci_ctrl->res.load_sram.size = DEVDRV_VF_IO_LOAD_SRAM_SIZE;

    pci_ctrl->res.hwts.addr = pci_ctrl->io_phy_base + DEVDRV_VF_IO_HWTS_NOTIFY_OFFSET;
    pci_ctrl->res.hwts.size = DEVDRV_VF_IO_HWTS_NOTIFY_SIZE;

    if (pci_ctrl->ops.is_mdev_vf_vm) {
        pci_ctrl->res.vpc.addr = pci_ctrl->mdev_rsv_mem_phy_base + DEVDRV_VF_MDEV_VPC_OFFSET;
        pci_ctrl->res.vpc.size = DEVDRV_VF_MDEV_VPC_SIZE;

        pci_ctrl->res.dvpp.addr = pci_ctrl->mdev_rsv_mem_phy_base + DEVDRV_VF_MDEV_DVPP_OFFSET;
        pci_ctrl->res.dvpp.size = DEVDRV_VF_MDEV_DVPP_SIZE;
    } else {
        pci_ctrl->res.vpc.size = 0;
        pci_ctrl->res.dvpp.size = 0;
    }

    /* not support */
    pci_ctrl->res.ts_sq.addr = 0;
    pci_ctrl->res.ts_sq.size = 0;

    pci_ctrl->res.bbox.addr = 0;
    pci_ctrl->res.bbox.size = 0;

    pci_ctrl->res.imu_log.addr = 0;
    pci_ctrl->res.imu_log.size = 0;

    pci_ctrl->res.hdr.size = 0;
    pci_ctrl->res.reg_sram.size = 0;
    pci_ctrl->res.vf_bandwidth.addr = 0;
    pci_ctrl->res.vf_bandwidth.size = 0;

    pci_ctrl->res.bbox_ddr_dump.size = 0;
    pci_ctrl->res.ts_log.size = 0;
    pci_ctrl->res.chip_dfx.size = 0;

    pci_ctrl->res.l3d_sram.addr = 0;
    pci_ctrl->res.l3d_sram.size = 0;
    /* pcie reserve bar space for  other modules */
    pci_ctrl->res.tsdrv_resv.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_TSDRV_RESV_OFFSET;
    pci_ctrl->res.tsdrv_resv.size = DEVDRV_RESERVE_TSDRV_RESV_SIZE;
    pci_ctrl->res.devmng_resv.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_DEVMNG_RESV_OFFSET;
    pci_ctrl->res.devmng_resv.size = DEVDRV_RESERVE_DEVMNG_RESV_SIZE;
    pci_ctrl->res.hbm_ecc_mem.addr = pci_ctrl->res.msg_mem.addr + DEVDRV_RESERVE_HBM_ECC_OFFSET;
    pci_ctrl->res.hbm_ecc_mem.size = DEVDRV_RESERVE_HBM_ECC_SIZE;
    pci_ctrl->res.devmng_info_mem.size = 0;
}

void devdrv_cloud_v2_init_pf_intr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_intr_info *intr = &pci_ctrl->res.intr;

    intr->min_vector = DEVDRV_MSI_X_MIN_VECTORS;
    intr->max_vector = DEVDRV_MSI_X_MAX_VECTORS;
    intr->device_os_load_irq = DEVDRV_LOAD_MSI_X_VECTOR_NUM;
    intr->msg_irq_base = DEVDRV_MSG_MSI_X_VECTOR_BASE;
    intr->msg_irq_num = DEVDRV_MSG_MSI_X_VECTOR_NUM;
    intr->dma_irq_base = DEVDRV_DMA_MSI_X_VECTOR_BASE;
    intr->dma_irq_num = DEVDRV_DMA_MSI_X_VECTOR_NUM;
    intr->tsdrv_irq_base = DEVDRV_TSDRV_MSI_X_VECTOR_BASE;
    intr->tsdrv_irq_num = DEVDRV_TSDRV_MSI_X_VECTOR_NUM;
    intr->topic_sched_irq_base = DEVDRV_TOPIC_SCHED_MSI_X_VECTOR_BASE;
    intr->topic_sched_irq_num = DEVDRV_TOPIC_SCHED_MSI_X_VECTOR_NUM;
    intr->cdqm_irq_base = DEVDRV_CDQM_MSI_X_VECTOR_BASE;
    intr->cdqm_irq_num = DEVDRV_CDQM_MSI_X_VECTOR_NUM;
    intr->msg_irq_vector2_base = DEVDRV_MSG_MSI_X_VECTOR_2_BASE;
    intr->msg_irq_vector2_num = intr->max_vector - DEVDRV_MSG_MSI_X_VECTOR_2_BASE;
    intr->tsdrv_irq_vector2_base = 0;
    intr->tsdrv_irq_vector2_num = 0;
}

void devdrv_cloud_v2_init_vf_intr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_intr_info *intr = &pci_ctrl->res.intr;

    if (pci_ctrl->ops.is_mdev_vf_vm) {
        intr->min_vector = DEVDRV_MSI_X_MDEV_VF_MIN_VECTORS;
        intr->max_vector = DEVDRV_MSI_X_MDEV_VF_MAX_VECTORS;
    } else {
        intr->min_vector = DEVDRV_MSI_X_VF_MIN_VECTORS;
        intr->max_vector = DEVDRV_MSI_X_VF_MAX_VECTORS;
    }

    intr->device_os_load_irq = DEVDRV_LOAD_MSI_X_VF_VECTOR_NUM;
    intr->msg_irq_base = DEVDRV_MSG_MSI_X_VF_VECTOR_BASE;
    intr->msg_irq_num = DEVDRV_MSG_MSI_X_VF_VECTOR_NUM;
    intr->dma_irq_base = DEVDRV_DMA_MSI_X_VF_VECTOR_BASE;
    intr->dma_irq_num = DEVDRV_DMA_MSI_X_VF_VECTOR_NUM;
    intr->tsdrv_irq_base = DEVDRV_TSDRV_MSI_X_VF_VECTOR_BASE;
    intr->tsdrv_irq_num = DEVDRV_TSDRV_MSI_X_VF_VECTOR_NUM;
    intr->topic_sched_irq_base = DEVDRV_TOPIC_SCHED_MSI_X_VF_VECTOR_BASE;
    intr->topic_sched_irq_num = DEVDRV_TOPIC_SCHED_MSI_X_VF_VECTOR_NUM;
    intr->cdqm_irq_base = DEVDRV_CDQM_MSI_X_VF_VECTOR_BASE;
    intr->cdqm_irq_num = DEVDRV_CDQM_MSI_X_VF_VECTOR_NUM;
    intr->msg_irq_vector2_base = DEVDRV_MSG_MSI_X_VF_VECTOR_2_BASE;
    intr->msg_irq_vector2_num = 0;
    intr->tsdrv_irq_vector2_base = 0;
    intr->tsdrv_irq_vector2_num = 0;
}

void devdrv_cloud_v2_peh_init_vf_intr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_intr_info *intr = &pci_ctrl->res.intr;

    if (pci_ctrl->ops.is_mdev_vf_vm) {
        intr->min_vector = DEVDRV_MSI_X_MDEV_VF_MIN_VECTORS;
        intr->max_vector = DEVDRV_MSI_X_MDEV_VF_MAX_VECTORS;
    } else {
        intr->min_vector = DEVDRV_MSI_X_PEH_VF_MIN_VECTORS;
        intr->max_vector = DEVDRV_MSI_X_PEH_VF_MAX_VECTORS;
    }

    intr->device_os_load_irq = DEVDRV_LOAD_MSI_X_PEH_VF_VECTOR_NUM;
    intr->msg_irq_base = DEVDRV_MSG_MSI_X_PEH_VF_VECTOR_BASE;
    intr->msg_irq_num = DEVDRV_MSG_MSI_X_PEH_VF_VECTOR_NUM;
    intr->dma_irq_base = DEVDRV_DMA_MSI_X_PEH_VF_VECTOR_BASE;
    intr->dma_irq_num = DEVDRV_DMA_MSI_X_PEH_VF_VECTOR_NUM;
    intr->tsdrv_irq_base = DEVDRV_TSDRV_MSI_X_PEH_VF_VECTOR_BASE;
    intr->tsdrv_irq_num = DEVDRV_TSDRV_MSI_X_PEH_VF_VECTOR_NUM;
    intr->topic_sched_irq_base = DEVDRV_TOPIC_SCHED_MSI_X_PEH_VF_VECTOR_BASE;
    intr->topic_sched_irq_num = DEVDRV_TOPIC_SCHED_MSI_X_PEH_VF_VECTOR_NUM;
    intr->cdqm_irq_base = DEVDRV_CDQM_MSI_X_PEH_VF_VECTOR_BASE;
    intr->cdqm_irq_num = DEVDRV_CDQM_MSI_X_PEH_VF_VECTOR_NUM;
    intr->msg_irq_vector2_base = DEVDRV_MSG_MSI_X_PEH_VF_VECTOR_2_BASE;
    intr->msg_irq_vector2_num = 0;
    intr->tsdrv_irq_vector2_base = 0;
    intr->tsdrv_irq_vector2_num = 0;
}

void devdrv_cloud_v2_init_pf_dma_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    int total_func_num = pci_ctrl->shr_para->dev_num;
    u32 i;

    if (total_func_num <= 0) {
        devdrv_warn("Input parameter is invalid. (total_func_num=%d)\n", total_func_num);
        total_func_num = 1;
    }

    pci_ctrl->res.dma_res.pf_num = pci_ctrl->func_id;
    pci_ctrl->res.dma_res.dma_addr = pci_ctrl->io_base + DEVDRV_IEP_DMA_OFFSET;
    pci_ctrl->res.dma_res.dma_chan_addr = pci_ctrl->res.dma_res.dma_addr + DEVDRV_IEP_DMA_CHAN_OFFSET;
    pci_ctrl->res.dma_res.dma_chan_num = (u32)(DMA_CHAN_REMOTE_USED_NUM / total_func_num);
    for (i = 0; i < pci_ctrl->res.dma_res.dma_chan_num; i++) {
        pci_ctrl->res.dma_res.use_chan[i] = i + pci_ctrl->func_id * pci_ctrl->res.dma_res.dma_chan_num;
    }
    pci_ctrl->res.dma_res.dma_chan_start_id = DMA_CHAN_REMOTE_USED_START_INDEX;
    pci_ctrl->res.dma_res.chan_start_id = pci_ctrl->res.dma_res.dma_chan_start_id;
    pci_ctrl->res.dma_res.vf_id = 0;
    pci_ctrl->res.dma_res.sq_depth = DEVDRV_MAX_DMA_CH_SQ_DEPTH;
    pci_ctrl->res.dma_res.sq_rsv_num = DEVDRV_DMA_CH_SQ_DESC_RSV;
    pci_ctrl->res.dma_res.cq_depth = DEVDRV_MAX_DMA_CH_CQ_DEPTH;
}

void devdrv_cloud_v2_init_vf_dma_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    unsigned long host_bitmap = (unsigned long)pci_ctrl->shr_para->dma_bitmap & HOST_VF_DMA_MASK;
    u32 chan_num = (u32)ka_base_bitmap_weight(&host_bitmap, DEVDRV_DMA_MAX_CHAN_NUM);
    u32 max_dma_chan_num = (u32)pci_ctrl->res.intr.dma_irq_num / DEVDRV_EACH_DMA_IRQ_NUM;
    u32 idx, i;

    /* hccs peh's vf only has 10 msi-x irq for dma, support up to 5 dma channels at most */
    if ((pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) && (chan_num > max_dma_chan_num)) {
        chan_num = max_dma_chan_num;
    }
    pci_ctrl->res.dma_res.dma_chan_num = chan_num;
    idx = DMA_CHAN_REMOTE_USED_START_INDEX;
    for (i = 0; i < chan_num; i++) {
        idx = (u32)ka_base_find_next_bit(&host_bitmap, DEVDRV_DMA_MAX_CHAN_NUM, idx);
        if (idx >= DMA_CHAN_REMOTE_USED_START_INDEX) {
            pci_ctrl->res.dma_res.use_chan[i] = idx - DMA_CHAN_REMOTE_USED_START_INDEX;
        }
        idx++;
    }

    pci_ctrl->res.dma_res.dma_chan_start_id = DMA_CHAN_REMOTE_USED_START_INDEX;
    pci_ctrl->res.dma_res.chan_start_id = pci_ctrl->res.dma_res.dma_chan_start_id;
    pci_ctrl->res.dma_res.pf_num = 0;
    if (pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) {
        /* HCCS can't use vf 1 */
        pci_ctrl->res.dma_res.vf_id = pci_ctrl->shr_para->vf_id + 1;
    } else {
        pci_ctrl->res.dma_res.vf_id = pci_ctrl->shr_para->vf_id;
    }
    pci_ctrl->res.dma_res.dma_addr = pci_ctrl->io_base + DEVDRV_VF_DMA_OFFSET;
    pci_ctrl->res.dma_res.dma_chan_addr = pci_ctrl->res.dma_res.dma_addr + DEVDRV_IEP_DMA_CHAN_OFFSET;
    pci_ctrl->res.dma_res.sq_depth = DEVDRV_MAX_DMA_CH_SQ_DEPTH;
    pci_ctrl->res.dma_res.sq_rsv_num = DEVDRV_DMA_CH_SQ_DESC_RSV;
    pci_ctrl->res.dma_res.cq_depth = DEVDRV_MAX_DMA_CH_CQ_DEPTH;
}

void devdrv_cloud_v2_init_load_file_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->res.load_file.load_file_num = CLOUD_V2_BLOCKS_NUM;
    if (pci_ctrl->multi_die == DEVDRV_MULTI_DIE_ONE_CHIP) {
        pci_ctrl->res.load_file.load_file_cfg = cloud_v2_910_93_file;
    } else {
        pci_ctrl->res.load_file.load_file_cfg = cloud_v2_910b_file;
    }
}

void devdrv_cloud_v2_init_depend_module_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (pci_ctrl->ops.is_mdev_vf_vm) {
        pci_ctrl->res.depend_info.module_num = CLOUD_V2_VF_VM_MODULE_NUM;
        pci_ctrl->res.depend_info.module_list = cloud_v2_vf_vm_module;
    } else {
        pci_ctrl->res.depend_info.module_num = CLOUD_V2_MODULE_NUM;
        pci_ctrl->res.depend_info.module_list = cloud_v2_module;
    }
}

void devdrv_cloud_v2_link_speed_slow_to_normal(struct devdrv_pci_ctrl *pci_ctrl)
{
    u64 *sram_complet_addr = pci_ctrl->res.load_sram_base;
    int retry_count = DEVDRV_WAIT_MODE_SWITCH_RETRY_COUNT;
    u64 flag_r = 0;

    if (pci_ctrl->os_load_flag == 0) {
        ka_mm_writeq(DEVDRV_SLOW_TO_NORMAL_MODE, sram_complet_addr);
        return;
    }

    flag_r = ka_mm_readq(sram_complet_addr);
    if (flag_r == DEVDRV_SLOW_BOOT_MODE) {
        ka_mm_writeq(DEVDRV_SLOW_TO_NORMAL_MODE, sram_complet_addr);
        devdrv_info("Notify bios slow to normal.(devid=%u)\n", pci_ctrl->dev_id);
    } else if (flag_r == DEVDRV_NORMAL_BOOT_MODE) {
        devdrv_info("Normal mode, no need to notify.(devid=%u)\n", pci_ctrl->dev_id);
        return;
    } else if (flag_r == DEVDRV_ABNORMAL_BOOT_MODE) {
        devdrv_warn("Link may has problem.(devid=%u)\n", pci_ctrl->dev_id);
        return;
    } else {
        return;
    }

    do {
        flag_r = ka_mm_readq(sram_complet_addr);
        if (flag_r == DEVDRV_NORMAL_BOOT_MODE) {
            devdrv_info("Slow to normal success.(devid=%u)\n", pci_ctrl->dev_id);
            return;
        }
        ka_system_msleep(DEVDRV_WAIT_MODE_SWITCH_TIME);
        retry_count--;
    } while (retry_count != 0);

    return;
}

enum devdrv_load_wait_mode devdrv_cloud_v2_get_load_wait_mode(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (devdrv_feature_is_support(pci_ctrl->features, DEVDRV_FEATURE_PRE_CFG)) {
        devdrv_pre_cfg(pci_ctrl);
    }

    if (pci_ctrl->ops.peh != NULL) {
        pci_ctrl->ops.peh->link_speed_slow_to_normal(pci_ctrl);
    }
    return DEVDRV_LOAD_WAIT_INTERVAL;
}

int devdrv_cloud_v2_get_pf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_PF_CHAN_CNT;
}

int devdrv_cloud_v2_get_vf_msg_chan_cnt(void)
{
    return DEVDRV_MAX_MSG_VF_CHAN_CNT;
}

/* cloud v2 8P1DIE * 2, has 2 node(8p system), host side number 0-7 --- node 0, 8-15 --- node 1
   cloud v2 8P1DIE, has 1 node(8p system), host side number 0-7 --- node 0
   cloud v2 8P2DIE, has 1 node(8p system), host side number 0-15 --- node 0 */
STATIC int devdrv_get_cloud_v2_devid_by_slotid(struct devdrv_shr_para __ka_mm_iomem *para)
{
    int dev_id = -1;
    struct devdrv_ctrl *p_ctrls = get_devdrv_ctrl();

    if ((para->slot_id >= BOARD_CLOUD_V2_8P_MAX_DEV_NUM) || (para->slot_id < 0)) {
        devdrv_err("Input parameter is invalid. (slot_id=%d)\n", para->slot_id);
        return dev_id;
    }

    dev_id = para->slot_id;

    if (p_ctrls[dev_id].priv != NULL) {
        devdrv_err("This dev_id is already registered. (dev_id=%d)\n", dev_id);
        dev_id = -1;
    } else {
        p_ctrls[dev_id].startup_flg = DEVDRV_DEV_STARTUP_PROBED;
    }
    return dev_id;
}

STATIC int devdrv_get_cloud_v2_pcie_card_devid(struct devdrv_pci_ctrl *pci_ctrl)
{
    int dev_id = -1;
#if defined(CFG_FEATURE_DMI) && defined(CONFIG_DMI)
    int ret = 0;
    struct devdrv_ctrl *p_ctrls = get_devdrv_ctrl();

    if ((pci_ctrl == NULL) || (pci_ctrl->pdev == NULL) || (ka_pci_get_bus(pci_ctrl->pdev) == NULL)) {
        devdrv_err("Input parameter is invalid. (pci_ctrl_is_null=%d;pdev_is_null=%d;bus_is_null=%d;)\n",
                   (pci_ctrl == NULL), (pci_ctrl->pdev == NULL), (ka_pci_get_bus(pci_ctrl->pdev) == NULL));
        return dev_id;
    }

    ret = ka_driver_dmi_find_devid(pci_ctrl->pdev, DEVDRV_DMI_DEV_TYPE_DEV_SLOT, &dev_id);
    if (ret != 0) {
        return devdrv_alloc_devid_inturn(0, 1);
    }

    if ((dev_id >= MAX_PF_DEV_CNT) || (dev_id < 0)) {
        devdrv_info("Dmi not match dev_id, do logical way (dev_id=%d)\n", dev_id);
        dev_id = devdrv_alloc_devid_inturn(0, 1);
        return dev_id;
    }

    if (p_ctrls[dev_id].priv != NULL) {
        devdrv_err("This dev_id is already registered. (dev_id=%u)\n", dev_id);
        dev_id = -1;
    } else {
        p_ctrls[dev_id].startup_flg = DEVDRV_DEV_STARTUP_PROBED;
    }
#else
    dev_id = devdrv_alloc_devid_inturn(0, 1);
#endif

    return dev_id;
}

/* devid = pf_if * 16 + (vf_id - 1) + 100 */
STATIC int devdrv_sriov_cloud_v2_alloc_vf_devid(struct devdrv_pci_ctrl *pci_ctrl)
{
    int pf_dev_id, vf_dev_id;
    int dev_id;

    pf_dev_id = devdrv_sriov_get_pf_devid_by_vf_ctrl(pci_ctrl);
    if (pf_dev_id < 0) {
        return devdrv_alloc_devid_inturn(0, 1);
    }

    vf_dev_id = (int)pci_ctrl->func_id;
    if ((pf_dev_id < 0) || (pf_dev_id >= MAX_PF_DEV_CNT) || (vf_dev_id <= 0) || (vf_dev_id >= MAX_VF_CNT_OF_PF)) {
        devdrv_err("pf_dev_id[%d] or vf_dev_id[%d] is invalid\n", pf_dev_id, vf_dev_id);
        return -1;
    }
    dev_id = pf_dev_id * MAX_VF_CNT_OF_PF + (vf_dev_id - 1) + DEVDRV_SRIOV_VF_DEVID_START;

    devdrv_info("Get vf devid, (pf_dev_id=%d;vf_dev_id=%d;dev_id=%d)\n", pf_dev_id, vf_dev_id, dev_id);
    return dev_id;
}

STATIC int devdrv_get_cloud_v2_2die_2pcie_devid(struct devdrv_ctrl *ctrl_this)
{
    struct devdrv_pci_ctrl *pci_ctrl = (struct devdrv_pci_ctrl *)ctrl_this->priv;
    int dev_id;
    int die_id;
    int alloc_id;

    die_id = pci_ctrl->shr_para->load_flag == 1 ? 0 : 1;
    dev_id = g_hw_info.chip_id * DEVDRV_CLOUD_V2_2DIE_DEVID_STRIDE_2 + die_id;
    alloc_id = devdrv_alloc_devid_inturn((u32)dev_id, DEVDRV_CLOUD_V2_2DIE_DEVID_STRIDE_2);

    devdrv_info("Alloc dev_id. (dev_id=%d;chip_id=%d;die_id=%d;bdf=%02x:%02x.%d)\n", alloc_id, g_hw_info.chip_id,
                die_id, ka_pci_get_bus_number(pci_ctrl->pdev), KA_PCI_SLOT(ka_pci_get_devfn(pci_ctrl->pdev)),
                KA_PCI_FUNC(ka_pci_get_devfn(pci_ctrl->pdev)));

    return alloc_id;
}

STATIC int devdrv_get_cloud_v2_2die_1pcie_devid(struct devdrv_ctrl *ctrl_this)
{
    return devdrv_alloc_devid_stride_2(ctrl_this);
}

STATIC int devdrv_get_cloud_v2_2die_devid(struct devdrv_ctrl *ctrl_this)
{
    if ((g_hw_info.mainboard_id == DEVDRV_CLOUD_V2_2DIE_2PCIE_MAINBOARD) ||
        (g_hw_info.addr_mode == DEVDRV_ADMODE_FULL_MATCH)) {
        return devdrv_get_cloud_v2_2die_2pcie_devid(ctrl_this);
    } else {
        return devdrv_get_cloud_v2_2die_1pcie_devid(ctrl_this);
    }
}

/* [board_id] bit[7] 0:1DIE 1:2DIE; bit[6:4] board_type; bit[3:0] board_config(no judge) */
STATIC bool devdrv_cloud_v2_is_pcie_card(u32 board_id)
{
    u32 board_type;

    /* 2DIE no pcie card currently */
    if ((board_id & DEVDRV_CLOUD_V2_2DIE_BIT) != 0) {
        return false;
    }

    board_type = (board_id >> DEVDRV_CLOUD_V2_BOARD_TYPE_OFFSET) & DEVDRV_CLOUD_V2_BOARD_TYPE_MASK;
    if ((board_type == DEVDRV_CLOUD_V2_1DIE_TRAIN_PCIE_CARD) || (board_type == DEVDRV_CLOUD_V2_1DIE_INFER_PCIE_CARD)) {
        return true;
    }

    return false;
}

int devdrv_cloud_v2_alloc_devid(struct devdrv_ctrl *ctrl_this)
{
    struct devdrv_pci_ctrl *pci_ctrl = (struct devdrv_pci_ctrl *)ctrl_this->priv;
    int board_type;
    int dev_id;

    dev_id = devdrv_alloc_devid_check_ctrls(ctrl_this);
    if (dev_id != -1) {
        return dev_id;
    }

    if (pci_ctrl->ops.is_mdev_vf_vm) {
        return devdrv_alloc_devid_inturn(0, 1);
    }

    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        return devdrv_sriov_cloud_v2_alloc_vf_devid(pci_ctrl);
    }

    devdrv_info("Get para value. (board_type=%d;node_id=%d;chip_id=%d;slot_id=%d)\n", pci_ctrl->shr_para->board_type,
                pci_ctrl->shr_para->node_id, pci_ctrl->shr_para->chip_id, pci_ctrl->shr_para->slot_id);

    board_type = pci_ctrl->shr_para->board_type;
    if ((board_type == BOARD_CLOUD_V2_MODE_8P) || (board_type == BOARD_CLOUD_V2_MODE_DUO_8P) ||
        (board_type == BOARD_CLOUD_V2_MODE_OAM_8P)) {
        devdrv_info("Alloc devid by slotid. (board_type=%d)\n", board_type);
        return devdrv_get_cloud_v2_devid_by_slotid(pci_ctrl->shr_para);
    } else if (devdrv_cloud_v2_is_pcie_card(g_hw_info.board_id)) {
        devdrv_info("Alloc devid for pcie card. (board_id=%d)\n", g_hw_info.board_id);
        dev_id = devdrv_get_cloud_v2_pcie_card_devid(pci_ctrl);
        return dev_id;
    } else if (g_hw_info.multi_die != 0) {
        devdrv_info("Alloc devid for 2die. (multi_die=%d)\n", g_hw_info.multi_die);
        return devdrv_get_cloud_v2_2die_devid(ctrl_this);
    } else {
        devdrv_info("Alloc devid in turn.\n");
        return devdrv_alloc_devid_inturn(0, 1);
    }
}

u32 devdrv_cloud_v2_get_p2p_support_max_devnum(void)
{
    return DEVDRV_CLOUD_V2_P2P_SUPPORT_MAX_DEVICE;
}

int devdrv_cloud_v2_is_p2p_access_cap(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_pci_ctrl *peer_pci_ctrl)
{
    if ((pci_ctrl->ops.is_p2p_access_cap_supported == false) ||
        (peer_pci_ctrl->ops.is_p2p_access_cap_supported == false)) {
        devdrv_info("devid and peer_devid are vf, p2p is not supported. (dev_id=%u;peer_devid=%u)\n", pci_ctrl->dev_id,
                    peer_pci_ctrl->dev_id);
        return DEVDRV_P2P_ACCESS_DISABLE;
    }

    if ((KA_PCI_FUNC(ka_pci_get_devfn(pci_ctrl->pdev)) > 0) ||
        (KA_PCI_FUNC(ka_pci_get_devfn(peer_pci_ctrl->pdev)) > 0)) {
        devdrv_info("devid and peer_devid are die1 of 1pf2die, p2p is not supported. (dev_id=%u;peer_devid=%u)\n",
                    pci_ctrl->dev_id, peer_pci_ctrl->dev_id);
        return DEVDRV_P2P_ACCESS_DISABLE;
    }

    return DEVDRV_P2P_ACCESS_ENABLE;
}

bool devdrv_cloud_v2_is_mdev_vm_full_spec(struct devdrv_pci_ctrl *pci_ctrl)
{
    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT) && (pci_ctrl->shr_para->vm_full_spec_flag == 1)) {
        return true;
    } else {
        return false;
    }
}

void devdrv_cloud_v2_hccs_flush_cache(u64 base, size_t len, u32 mode)
{
#ifdef __aarch64__
    u64 addr_loop, addr_end;

    addr_loop = base & (~DEVDRV_HCCS_CACHELINE_MASK);
    addr_end = (base + len + DEVDRV_HCCS_CACHELINE_MASK) & (~DEVDRV_HCCS_CACHELINE_MASK);

    asm volatile("dsb st" : : : "memory");

    if (mode == CACHE_INVALID) {
        for (; addr_loop < addr_end;) {
            /* Invalid cache before read */
            asm volatile("DC IVAC ,%x0" ::"r"(addr_loop));
            ka_mb();
            addr_loop += DEVDRV_HCCS_CACHELINE_SIZE;
        }
    } else {
        for (; addr_loop < addr_end;) {
            /* Clean and invalid cache after write */
            asm volatile("DC CIVAC ,%x0" ::"r"(addr_loop));
            ka_mb();
            addr_loop += DEVDRV_HCCS_CACHELINE_SIZE;
        }
    }

    asm volatile("dsb st" : : : "memory");
#endif
}

int devdrv_cloud_v2_get_peh_link_info(ka_pci_dev_t *pdev, u32 *link_speed, u32 *link_width, u32 *link_status)
{
    u32 link_info = 0;
    int ret = 0;

    ret = ka_pci_read_config_dword(pdev, DEVDRV_PCIE_LINK_INFO_OFFSET, &link_info);
    if (ret != 0) {
        devdrv_warn("Get peh link info fail.(ret=%d)\n", ret);
        return ret;
    }

    if (link_info == DEVDRV_PCIE_LINK_DOWN_STATE) {
        *link_speed = 0;
        *link_width = 0;
        *link_status = 0;
        return 0;
    }

    *link_speed = (link_info >> DEVDRV_CLOUD_V2_LINK_INFO_SPEED_SHIFT) & DEVDRV_CLOUD_V2_LINK_INFO_SPEED_MASK;
    *link_width = (link_info >> DEVDRV_CLOUD_V2_LINK_INFO_WIDTH_SHIFT) & DEVDRV_CLOUD_V2_LINK_INFO_WIDTH_MASK;
    *link_status = DEVDRV_CLOUD_V2_LINK_STATUS_NORMAL;
    return 0;
}

STATIC int devdrv_cloud_v2_msi_x_table_retrans(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_host_dma_addr_to_pa_cmd cmd_data = {0};
    u32 data_len;

    data_len = sizeof(struct devdrv_host_dma_addr_to_pa_cmd);
    cmd_data.sub_cmd = DEVDRV_PEH_MSI_TABLE_REFRESH;
    cmd_data.host_devid = pci_ctrl->dev_id;

    return devdrv_admin_msg_chan_send(pci_ctrl->msg_dev, DEVDRV_HCCS_HOST_DMA_ADDR_MAP, &cmd_data, data_len, NULL,
                                      data_len);
}

STATIC int devdrv_cloud_v2_msi_x_table_retrans_pth(struct devdrv_pci_ctrl *pci_ctrl)
{
    if ((ka_pci_get_is_physfn(pci_ctrl->pdev) == 0) && (ka_pci_get_is_virtfn(pci_ctrl->pdev) == 0)) {
        return devdrv_cloud_v2_msi_x_table_retrans(pci_ctrl);
    }

    return 0;
}

STATIC int devdrv_cloud_v2_msi_x_table_retrans_sriov(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (pci_ctrl->shr_para->sid != 0) {
        return devdrv_cloud_v2_msi_x_table_retrans(pci_ctrl);
    }

    return 0;
}

STATIC int devdrv_cloud_v2_msi_x_table_retrans_pm(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_host_dma_addr_to_pa_cmd cmd_data = {0};
    struct devdrv_pci_ctrl *pci_ctrl_pf = NULL;
    u32 pf_dev_id, data_len;

    /* vf has no msg chan in pm, so must use pf's admin chan */
    pf_dev_id = devdrv_sriov_get_pf_devid_by_vf_ctrl(pci_ctrl);
    pci_ctrl_pf = devdrv_get_bottom_half_pci_ctrl_by_id(pf_dev_id);
    if ((pci_ctrl_pf == NULL) || (pci_ctrl_pf->msg_dev == NULL)) {
        devdrv_err("Get pci_ctrl_pf or msg_dev failed. (pf_dev_id=%u)\n", pf_dev_id);
        return -EINVAL;
    }

    data_len = sizeof(struct devdrv_host_dma_addr_to_pa_cmd);
    cmd_data.sub_cmd = DEVDRV_PEH_VF_MSI_TABLE_REFRESH;
    cmd_data.host_devid = pci_ctrl->dev_id;
    return devdrv_admin_msg_chan_send(pci_ctrl_pf->msg_dev, DEVDRV_HCCS_HOST_DMA_ADDR_MAP, &cmd_data, data_len, NULL,
                                      data_len);
}

STATIC int devdrv_cloud_v2_msi_x_table_no_retrans(struct devdrv_pci_ctrl *pci_ctrl)
{
    (void)pci_ctrl;
    return 0;
}

/*****************************************************************************************
 * msi_x_table_retrans                : hccs peh's Virtualization pass-through or smmu enabled, msi-x table will be va,
 *                                      need retranslation to pa by agent-smmu after request. In MDEV scenario, addr is
 *                                      mapped to PA by PM, so VM mode requires no mapping.
 * flush_cache                        : in hccs, hardware cannot actively flush cache to DDR; software must flush cache
 *                                      manually.
 * get_peh_link_info                  : get hccs link speed and bandwidth info.
 * link_speed_slow_to_normal          : link at low speed in hccs, then switch to high speed to load NPU firmware after
 *                                      link establishment.
 * pdev_sid_init                      : init sid for SMMU page table lookup. In VM the sid is arbitrarily configured,
 *                                      so no need to obtain it.
 * pdev_sid_uninit                    : uninit sid.
 *****************************************************************************************/

// clang-format off
// physical machine mode
#define DEVDRV_PEH_PHY_BOOT_OPS                                                       \
    .msi_x_table_retrans = devdrv_cloud_v2_msi_x_table_retrans_pth,                   \
    .flush_cache = devdrv_cloud_v2_hccs_flush_cache,                                  \
    .get_peh_link_info = devdrv_cloud_v2_get_peh_link_info,                           \
    .link_speed_slow_to_normal = devdrv_cloud_v2_link_speed_slow_to_normal,           \
    .pdev_sid_init = devdrv_pdev_sid_init,                                            \
    .pdev_sid_uninit = devdrv_pdev_sid_uninit,                                        \
    .smmu_iova_to_phys_proc = devdrv_smmu_iova_to_phys_proc,

// docker mode
#define DEVDRV_PEH_SRIOV_VF_BOOT_OPS                                                  \
    .msi_x_table_retrans = devdrv_cloud_v2_msi_x_table_retrans_sriov,                 \
    .flush_cache = devdrv_cloud_v2_hccs_flush_cache,                                  \
    .get_peh_link_info = devdrv_cloud_v2_get_peh_link_info,                           \
    .link_speed_slow_to_normal = devdrv_cloud_v2_link_speed_slow_to_normal,           \
    .pdev_sid_init = devdrv_pdev_sid_init,                                            \
    .pdev_sid_uninit = devdrv_pdev_sid_uninit,                                        \
    .smmu_iova_to_phys_proc = devdrv_smmu_iova_to_phys_proc,

// mdev computing group 1:1 physical machine mode
#define DEVDRV_PEH_MDEV_FULL_SPEC_VF_PM_BOOT_OPS                                      \
    .msi_x_table_retrans = devdrv_cloud_v2_msi_x_table_retrans_pm,                    \
    .flush_cache = devdrv_cloud_v2_hccs_flush_cache,                                  \
    .get_peh_link_info = devdrv_cloud_v2_get_peh_link_info,                           \
    .link_speed_slow_to_normal = devdrv_cloud_v2_link_speed_slow_to_normal,           \
    .pdev_sid_init = devdrv_pdev_sid_init,                                            \
    .pdev_sid_uninit = devdrv_pdev_sid_uninit,                                        \
    .smmu_iova_to_phys_proc = devdrv_smmu_iova_to_phys_proc,

// mdev computing group 1:1 virtual machine mode
#define DEVDRV_PEH_MDEV_FULL_SPEC_VF_VM_BOOT_OPS                                      \
    .msi_x_table_retrans = devdrv_cloud_v2_msi_x_table_no_retrans,                    \
    .flush_cache = devdrv_cloud_v2_hccs_flush_cache,                                  \
    .get_peh_link_info = devdrv_cloud_v2_get_peh_link_info,                           \
    .link_speed_slow_to_normal = devdrv_cloud_v2_link_speed_slow_to_normal,           \
    .pdev_sid_init = devdrv_pdev_sid_no_init,                                         \
    .pdev_sid_uninit = devdrv_pdev_sid_no_uninit,                                     \
    .smmu_iova_to_phys_proc = devdrv_smmu_iova_to_phys_proc,

// mdev computing group non-1:1 physical machine mode
#define DEVDRV_PEH_MDEV_VF_PM_BOOT_OPS                                                \
    .msi_x_table_retrans = devdrv_cloud_v2_msi_x_table_retrans_pm,                    \
    .flush_cache = devdrv_cloud_v2_hccs_flush_cache,                                  \
    .get_peh_link_info = devdrv_cloud_v2_get_peh_link_info,                           \
    .link_speed_slow_to_normal = devdrv_cloud_v2_link_speed_slow_to_normal,           \
    .pdev_sid_init = devdrv_pdev_sid_init,                                            \
    .pdev_sid_uninit = devdrv_pdev_sid_uninit,                                        \
    .smmu_iova_to_phys_proc = devdrv_smmu_iova_to_phys_proc,

// mdev computing group non-1:1 virtual machine mode
#define DEVDRV_PEH_MDEV_VF_VM_BOOT_OPS                                                \
    .msi_x_table_retrans = devdrv_cloud_v2_msi_x_table_no_retrans,                    \
    .flush_cache = devdrv_cloud_v2_hccs_flush_cache,                                  \
    .get_peh_link_info = devdrv_cloud_v2_get_peh_link_info,                           \
    .link_speed_slow_to_normal = devdrv_cloud_v2_link_speed_slow_to_normal,           \
    .pdev_sid_init = devdrv_pdev_sid_no_init,                                         \
    .pdev_sid_uninit = devdrv_pdev_sid_no_uninit,                                     \
    .smmu_iova_to_phys_proc = devdrv_smmu_iova_to_phys_proc,

static const struct devdrv_dev_peh_ops g_peh_ops_table[] = {
    [DEVDRV_PHY_BOOT] = {
        DEVDRV_PEH_PHY_BOOT_OPS
    },
    [DEVDRV_SRIOV_VF_BOOT] = {
        DEVDRV_PEH_SRIOV_VF_BOOT_OPS
    },
    [DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT] = {
        DEVDRV_PEH_MDEV_FULL_SPEC_VF_PM_BOOT_OPS
    },
    [DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT] = {
        DEVDRV_PEH_MDEV_FULL_SPEC_VF_VM_BOOT_OPS
    },
    [DEVDRV_MDEV_VF_PM_BOOT] = {
        DEVDRV_PEH_MDEV_VF_PM_BOOT_OPS
    },
    [DEVDRV_MDEV_VF_VM_BOOT] = {
        DEVDRV_PEH_MDEV_VF_VM_BOOT_OPS
    },
};
// clang-format on

STATIC int devdrv_cloud_v2_peh_ops_init(struct devdrv_pci_ctrl *pci_ctrl, struct devdrv_dev_peh_ops *peh_ops)
{
    if (pci_ctrl->env_boot_mode >= KA_BASE_ARRAY_SIZE(g_peh_ops_table)) {
        devdrv_err("Invalid env_boot_mode=%u\n", pci_ctrl->env_boot_mode);
        return -EINVAL;
    }

    *peh_ops = g_peh_ops_table[pci_ctrl->env_boot_mode];
    return 0;
}

void devdrv_cloud_v2_set_dev_shr_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    /* vf's vm not set host_dev_id, use host_dev_id on vf's pm */
    if (pci_ctrl->ops.is_mdev_vf_vm == false) {
        pci_ctrl->shr_para->host_dev_id = (int)pci_ctrl->dev_id;
    }
    pci_ctrl->shr_para->host_mem_bar_base = (u64)pci_ctrl->mem_phy_base;
    pci_ctrl->shr_para->host_io_bar_base = (u64)pci_ctrl->io_phy_base;
}

int devdrv_cloud_v2_get_p2p_addr(struct devdrv_pci_ctrl *pci_ctrl, u32 remote_dev_id, enum devdrv_p2p_addr_type type,
                                 phys_addr_t *phy_addr, size_t *size)
{
    u32 dev_id = pci_ctrl->dev_id;
    u64 io_txatu_base;

    io_txatu_base = pci_ctrl->target_bar[remote_dev_id].io_txatu_base;
    if (io_txatu_base == 0) {
        devdrv_err("Invalid io tx atu base. (dev_id=%u; remote_dev_id=%u; type=%u)\n", dev_id, remote_dev_id,
                   (u32)type);
        return -EINVAL;
    }

    switch (type) {
        case DEVDRV_P2P_IO_TS_DB:
            *phy_addr = (phys_addr_t)(io_txatu_base + DEVDRV_IO_TS_DB_OFFSET);
            *size = DEVDRV_IO_TS_DB_SIZE;
            break;
        case DEVDRV_P2P_IO_TS_SRAM:
            *phy_addr = (phys_addr_t)(io_txatu_base + DEVDRV_IO_TS_SRAM_OFFSET);
            *size = DEVDRV_IO_TS_SRAM_SIZE;
            break;
        case DEVDRV_P2P_IO_HWTS:
            *phy_addr = (phys_addr_t)(io_txatu_base + DEVDRV_IO_HWTS_NOTIFY_OFFSET);
            *size = DEVDRV_IO_HWTS_NOTIFY_SIZE;
            break;
        default:
            devdrv_err("P2P address type not support. (dev_id=%u; remote_dev_id=%u; type=%u)\n", dev_id, remote_dev_id,
                       (u32)type);
            return -EINVAL;
    }

    return 0;
}

unsigned devdrv_cloud_v2_get_server_id(struct devdrv_pci_ctrl *pci_ctrl)
{
    devdrv_hw_info_t *hw_data = NULL;
    void __ka_mm_iomem *hw_info_addr = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET +
                                       DEVDRV_CLOUD_V2_HW_INFO_SRAM_OFFSET;

    hw_data = (devdrv_hw_info_t *)hw_info_addr;
    g_hw_info.server_id = hw_data->server_id;

    return g_hw_info.server_id;
}

unsigned devdrv_cloud_v2_get_max_server_num(struct devdrv_pci_ctrl *pci_ctrl)
{
    devdrv_hw_info_t *hw_data = NULL;
    void __ka_mm_iomem *hw_info_addr = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET +
                                       DEVDRV_CLOUD_V2_HW_INFO_SRAM_OFFSET;

    hw_data = (devdrv_hw_info_t *)hw_info_addr;
    g_hw_info.scale_type = hw_data->scale_type;

    return (g_hw_info.scale_type / DEVDRV_S2S_MAX_CHIP_NUM);
}

void devdrv_cloud_v2_init_virt_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_virt_para __ka_mm_iomem *virt_para;
    int numa_node;
    unsigned int host_flag;
    int ret;

    ret = devdrv_pci_get_host_phy_mach_flag(pci_ctrl->dev_id, &host_flag);
    if (ret != 0) {
        devdrv_err("Get phy mach flag failed. (dev_id=%d; ret=%d)\n", pci_ctrl->dev_id, ret);
        return;
    }
    if (host_flag == DEVDRV_HOST_PHY_MACH_FLAG) {
        return;
    }
    /* now only support PF */
    if (pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_VF) {
        return;
    }
    virt_para = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET + DEVDRV_VIRT_PARA_ADDR_OFFSET;
    /* compatible with old version */
    ka_mm_writel(DEVDRV_VIRT_NUMA_MAGIC, &virt_para->numa_node);
    numa_node = (int)ka_mm_readl(&virt_para->numa_node);
    if (numa_node != DEVDRV_VIRT_NUMA_MAGIC) {
        if (numa_node >= 0 && numa_node < KA_MAX_NUMNODES) {
            ka_base_set_dev_node(ka_pci_get_dev(pci_ctrl->pdev), numa_node);
        } else {
            devdrv_warn("invalid numa node: %d\n", numa_node);
        }
    }
}

u32 devdrv_cloud_v2_get_nvme_low_level_db_irq_num(void)
{
    return CLOUD_V2_NVME_LOW_LEVEL_DB_IRQ_NUM;
}

u32 devdrv_cloud_v2_get_nvme_db_irq_strde(void)
{
    return CLOUD_V2_NVME_DB_IRQ_STRDE;
}

void devdrv_cloud_v2_init_pf_msg_cnt(struct devdrv_pci_ctrl *pci_ctrl)
{
    int i;
    for (i = 0; i < devdrv_msg_client_max; i++) {
        pci_ctrl->res.msg_chan_cnt[i] = (int)devdrv_pf_msg_chan_cnt_cloud_v2[i];
    }
}

void devdrv_cloud_v2_init_vf_msg_cnt(struct devdrv_pci_ctrl *pci_ctrl)
{
    int i;
    for (i = 0; i < devdrv_msg_client_max; i++) {
        pci_ctrl->res.msg_chan_cnt[i] = (int)devdrv_vf_msg_chan_cnt_cloud_v2[i];
    }
}

STATIC int devdrv_init_hw_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    void __ka_mm_iomem *hw_info_addr = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET +
                                       DEVDRV_CLOUD_V2_HW_INFO_SRAM_OFFSET;
    devdrv_hw_info_t *hw_data = NULL;
    int retry_times = 0;

retry:
    hw_data = (devdrv_hw_info_t __ka_mm_iomem *)hw_info_addr;
    g_hw_info.chip_id = hw_data->chip_id;
    g_hw_info.multi_chip = hw_data->multi_chip;
    g_hw_info.multi_die = hw_data->multi_die;
    g_hw_info.mainboard_id = hw_data->mainboard_id;
    g_hw_info.addr_mode = hw_data->addr_mode;
    g_hw_info.board_id = hw_data->board_id;
    g_hw_info.version = hw_data->version;
    g_hw_info.connect_type = hw_data->connect_type;
    g_hw_info.hccs_hpcs_bitmap = hw_data->hccs_hpcs_bitmap;
    g_hw_info.server_id = hw_data->server_id;
    g_hw_info.scale_type = hw_data->scale_type;
    g_hw_info.super_pod_id = hw_data->super_pod_id;

    /* hccs peh's Virtualization pass-through, need wait bios recovery die1's peh config */
    if ((retry_times < DEVDRV_INIT_HWINFO_RETRY_TIME) && ((u32)g_hw_info.chip_id == INVALID_CHIP_ID) &&
        ((u32)g_hw_info.multi_die == INVALID_MULTI_DIE)) {
        ka_system_msleep(DEVDRV_INIT_HWINFO_WAIT_TIME);
        retry_times++;
        goto retry;
    }

    pci_ctrl->connect_protocol = g_hw_info.connect_type;
    pci_ctrl->addr_mode = g_hw_info.addr_mode;
    pci_ctrl->multi_die = g_hw_info.multi_die;
    devdrv_info("hw info:chip_id=%u;multi_die=%u;addr_mode=%u;connect_type=%u;retry_times=%d\n", g_hw_info.chip_id,
                g_hw_info.multi_die, g_hw_info.addr_mode, g_hw_info.connect_type, retry_times);

    if (retry_times == DEVDRV_INIT_HWINFO_RETRY_TIME) {
        return -ENODEV;
    }
    return 0;
}

int devdrv_cloud_v2_init_pf_bar_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    resource_size_t offset;
    unsigned long size;
    int ret;

    pci_ctrl->io_phy_base = (phys_addr_t)ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_IO);
    pci_ctrl->io_phy_size = (u64)ka_pci_resource_len(pci_ctrl->pdev, PCI_BAR_IO);

    pci_ctrl->io_base = ka_mm_ioremap(pci_ctrl->io_phy_base, pci_ctrl->io_phy_size);
    if (pci_ctrl->io_base == NULL) {
        devdrv_err("Ioremap io_base failed. (size=0x%llx)\n", pci_ctrl->io_phy_size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    ret = devdrv_init_hw_info(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Init hw info failed.");
        devdrv_res_uninit(pci_ctrl);
        return ret;
    }

    if (pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) {
        g_reserver_mem_msg_offset = DEVDRV_PEH_RESERVE_MEM_MSG_OFFSET;
    } else {
        g_reserver_mem_msg_offset = DEVDRV_RESERVE_MEM_MSG_OFFSET;
    }

    pci_ctrl->mem_bar_id = PCI_BAR_MEM;
    pci_ctrl->mem_phy_base = (phys_addr_t)ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_MEM);
    pci_ctrl->mem_phy_size = (u64)ka_pci_resource_len(pci_ctrl->pdev, PCI_BAR_MEM);

    offset = ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM) + (resource_size_t)g_reserver_mem_msg_offset;
    size = DEVDRV_RESERVE_MEM_MSG_SIZE;

    pci_ctrl->mem_base = ka_mm_ioremap(offset, size);
    if (pci_ctrl->mem_base == NULL) {
        devdrv_err("Ioremap mem_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->rsv_mem_phy_base = (phys_addr_t)ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM);
    pci_ctrl->rsv_mem_phy_size = (u64)ka_pci_resource_len(pci_ctrl->pdev, PCI_BAR_RSV_MEM);

    offset = ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_IO) + DEVDRV_SOC_DOORBELL_OFFSET + DEVDRV_IEP_SDI0_DB_OFFSET;
    size = DEVDRV_DB_IOMAP_SIZE;
    pci_ctrl->msi_base = ka_mm_ioremap(offset, size);
    if (pci_ctrl->msi_base == NULL) {
        devdrv_err("Ioremap msi_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->shr_para = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET + DEVDRV_SHR_PARA_ADDR_OFFSET;
    /* bios will use phy_match_flag to safety check when upgrade firmware */
    pci_ctrl->shr_para->phy_match_flag = DEVDRV_HOST_PHY_MACH_FLAG;
    pci_ctrl->res.phy_match_flag_addr = (u8 *)pci_ctrl->shr_para + PHY_MATCH_FLAG_OFFSET_IN_SHR_MEM;
    pci_ctrl->res.nvme_db_base = pci_ctrl->msi_base;
    pci_ctrl->res.nvme_pf_ctrl_base = NULL;
    pci_ctrl->res.load_sram_base = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET;

    return 0;
}

void devdrv_cloud_v2_vf_boot_mode_rebuild(struct devdrv_pci_ctrl *pci_ctrl)
{
    if (ka_pci_get_revision(pci_ctrl->pdev) == DEVDRV_REVISION_TYPE_MDEV_SRIOV_VF) {
        if (pci_ctrl->shr_para->vm_full_spec_flag == 1) {
            /* vf full spec mdev's vm */
            pci_ctrl->env_boot_mode = DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT;
        } else {
            /* vf not full spec mdev's vm */
            pci_ctrl->env_boot_mode = DEVDRV_MDEV_VF_VM_BOOT;
        }
    } else {
        if ((pci_ctrl->shr_para->vm_full_spec_flag == 1) && (pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_PM_BOOT)) {
            /* vf full spec mdev's phy */
            pci_ctrl->env_boot_mode = DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT;
        }
    }
}

STATIC void devdrv_init_vf_hw_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    struct devdrv_pci_ctrl *pci_ctrl_pf = NULL;
    int pf_dev_id;

    pf_dev_id = devdrv_sriov_get_pf_devid_by_vf_ctrl(pci_ctrl);
    if (pf_dev_id < 0) {
        /* in mdev vm, no pf pci_ctrl, get from shr_para */
        pci_ctrl->connect_protocol = pci_ctrl->shr_para->connect_protocol;
        pci_ctrl->addr_mode = pci_ctrl->shr_para->addr_mode;
        pci_ctrl->multi_die = 0;

        if (pci_ctrl->connect_protocol == CONNECT_PROTOCOL_HCCS) {
            g_reserver_mem_msg_offset = DEVDRV_PEH_RESERVE_MEM_MSG_OFFSET;
        } else {
            g_reserver_mem_msg_offset = DEVDRV_RESERVE_MEM_MSG_OFFSET;
        }

        devdrv_info("In mdev vm, vf hw info:multi_die=%u;addr_mode=%u;connect_type=%u\n", pci_ctrl->multi_die,
                    pci_ctrl->addr_mode, pci_ctrl->connect_protocol);
        return;
    }

    pci_ctrl_pf = devdrv_get_bottom_half_pci_ctrl_by_id((u32)pf_dev_id);
    if (pci_ctrl_pf == NULL) {
        devdrv_err("Get vf's pf pci_ctrl failed. (pf_dev_id=%d)\n", pf_dev_id);
        pci_ctrl->connect_protocol = CONNECT_PROTOCOL_HCCS;
        pci_ctrl->addr_mode = DEVDRV_ADMODE_FULL_MATCH;
        pci_ctrl->multi_die = 0;
        return;
    }

    pci_ctrl->connect_protocol = pci_ctrl_pf->connect_protocol;
    pci_ctrl->addr_mode = pci_ctrl_pf->addr_mode;
    pci_ctrl->multi_die = 0;

    pci_ctrl->shr_para->connect_protocol = pci_ctrl->connect_protocol;
    pci_ctrl->shr_para->addr_mode = pci_ctrl->addr_mode;

    devdrv_info("In mdev pm, vf hw info:multi_die=%u;addr_mode=%u;connect_type=%u\n", pci_ctrl->multi_die,
                pci_ctrl->addr_mode, pci_ctrl->connect_protocol);
    return;
}

int devdrv_cloud_v2_init_vf_bar_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    resource_size_t offset;
    unsigned long size;
    unsigned long rsv_vpc_mem_offset = 0;

    pci_ctrl->mem_bar_id = PCI_BAR_MEM;

    pci_ctrl->mem_phy_base = (phys_addr_t)ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_MEM);
    pci_ctrl->mem_phy_size = (u64)ka_pci_resource_len(pci_ctrl->pdev, PCI_BAR_MEM);

    offset = ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_IO) + DEVDRV_VF_IO_SOC_DOORBELL_OFFSET;
    size = DEVDRV_VF_IO_SOC_DOORBELL_SIZE;
    pci_ctrl->msi_base = ka_mm_ioremap(offset, size);
    if (pci_ctrl->msi_base == NULL) {
        devdrv_err("Ioremap msi_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->io_phy_base = (phys_addr_t)ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_IO);
    pci_ctrl->io_phy_size = (u64)ka_pci_resource_len(pci_ctrl->pdev, PCI_BAR_IO);

    pci_ctrl->io_base = ka_mm_ioremap(pci_ctrl->io_phy_base, pci_ctrl->io_phy_size);
    if (pci_ctrl->io_base == NULL) {
        devdrv_err("Ioremap io_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }
    pci_ctrl->shr_para = pci_ctrl->io_base + DEVDRV_VF_IO_LOAD_SRAM_OFFSET + DEVDRV_SHR_PARA_ADDR_OFFSET;
    devdrv_init_vf_hw_info(pci_ctrl);

    if ((pci_ctrl->env_boot_mode == DEVDRV_MDEV_VF_VM_BOOT) ||
        (pci_ctrl->env_boot_mode == DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT)) {
        rsv_vpc_mem_offset = DEVDRV_VF_MDEV_RESERVE_MEM_SIZE;
        pci_ctrl->mdev_rsv_mem_phy_base = (phys_addr_t)ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM);
        pci_ctrl->mdev_rsv_mem_phy_size = DEVDRV_VF_MDEV_RESERVE_MEM_SIZE;
    }

    offset = ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM) + DEVDRV_VF_MEM_MSG_OFFSET + rsv_vpc_mem_offset +
             g_reserver_mem_msg_offset;
    size = DEVDRV_VF_MEM_MSG_SIZE;
    pci_ctrl->mem_base = ka_mm_ioremap(offset, size);
    if (pci_ctrl->mem_base == NULL) {
        devdrv_err("Ioremap mem_base failed. (size=%lu)\n", size);
        devdrv_res_uninit(pci_ctrl);
        return -ENOMEM;
    }

    pci_ctrl->rsv_mem_phy_base = (phys_addr_t)ka_pci_resource_start(pci_ctrl->pdev, PCI_BAR_RSV_MEM) +
                                 rsv_vpc_mem_offset;
    pci_ctrl->rsv_mem_phy_size = (u64)ka_pci_resource_len(pci_ctrl->pdev, PCI_BAR_RSV_MEM) - rsv_vpc_mem_offset;

    pci_ctrl->res.phy_match_flag_addr = (u8 *)pci_ctrl->shr_para + PHY_MATCH_FLAG_OFFSET_IN_SHR_MEM;
    pci_ctrl->res.nvme_db_base = pci_ctrl->msi_base;
    pci_ctrl->res.nvme_pf_ctrl_base = NULL;
    pci_ctrl->res.load_sram_base = pci_ctrl->io_base + DEVDRV_IO_LOAD_SRAM_OFFSET;
    pci_ctrl->func_id = pci_ctrl->shr_para->vf_id;

    return 0;
}

void devdrv_cloud_v2_init_pf_setup_runtime_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->os_load_flag = (u32)pci_ctrl->shr_para->load_flag;
    pci_ctrl->remote_dev_id = pci_ctrl->os_load_flag == 1 ? 0 : 1;
    pci_ctrl->local_reserve_mem_base = NULL;
}

void devdrv_cloud_v2_init_vf_setup_runtime_info(struct devdrv_pci_ctrl *pci_ctrl)
{
    pci_ctrl->remote_dev_id = (u32)pci_ctrl->shr_para->vf_id;
    pci_ctrl->os_load_flag = 0;
    pci_ctrl->local_reserve_mem_base = NULL;
}

#endif

// clang-format off
static const devdrv_feature_bitmap_t cloud_v2_features =
    (1ULL << DEVDRV_FEATURE_S2S) |
    (1ULL << DEVDRV_FEATURE_DMA_COPY_SVA) |
    (1ULL << DEVDRV_FEATURE_HCCS_LINK_INFO) |
    (1ULL << DEVDRV_FEATURE_PRE_CFG);

#define DEVDRV_CLOUD_V2_OPS                                                                       \
    .init_load_file_info = devdrv_cloud_v2_init_load_file_info,                                   \
    .init_depend_module_info = devdrv_cloud_v2_init_depend_module_info,                           \
    .alloc_devid = devdrv_cloud_v2_alloc_devid,                                                   \
    .is_p2p_access_cap = devdrv_cloud_v2_is_p2p_access_cap,                                       \
    .get_load_wait_mode = devdrv_cloud_v2_get_load_wait_mode,                                     \
    .get_pf_max_msg_chan_cnt = devdrv_cloud_v2_get_pf_msg_chan_cnt,                               \
    .get_vf_max_msg_chan_cnt = devdrv_cloud_v2_get_vf_msg_chan_cnt,                               \
    .get_p2p_support_max_devnum = devdrv_cloud_v2_get_p2p_support_max_devnum,                     \
    .set_dev_shr_info = devdrv_cloud_v2_set_dev_shr_info,                                         \
    .get_nvme_low_level_db_irq_num = devdrv_cloud_v2_get_nvme_low_level_db_irq_num,               \
    .get_nvme_db_irq_strde = devdrv_cloud_v2_get_nvme_db_irq_strde,                               \
    .get_vf_dma_info = devdrv_cloud_v2_init_vf_dma_info,                                          \
    .is_mdev_vm_full_spec = devdrv_cloud_v2_is_mdev_vm_full_spec,                                 \
    .get_p2p_addr = devdrv_cloud_v2_get_p2p_addr,                                                 \
    .get_server_id = devdrv_cloud_v2_get_server_id,                                               \
    .get_max_server_num = devdrv_cloud_v2_get_max_server_num,                                     \
    .init_virt_info = devdrv_cloud_v2_init_virt_info,                                             \
    .init_link_info = NULL,                                                                       \
    .set_udevid_reorder_para = NULL,                                                              \
    .devdrv_deal_suspend_handshake = NULL,                                                        \
    .check_ep_suspend_status = NULL,                                                              \
    .single_fault_init = NULL,                                                                    \
    .single_fault_uninit = NULL,

#define DEVDRV_CLOUD_V2_PF_OPS                                                                    \
    .init_bar_info = devdrv_cloud_v2_init_pf_bar_info,                                            \
    .init_bar_addr_info = devdrv_cloud_v2_init_pf_bar_addr_info,                                  \
    .init_msg_cnt = devdrv_cloud_v2_init_pf_msg_cnt,                                              \
    .boot_mode_rebuild = NULL,                                                                    \
    .init_dma_info = devdrv_cloud_v2_init_pf_dma_info,                                            \
    .init_setup_runtime_info = devdrv_cloud_v2_init_pf_setup_runtime_info,

#define DEVDRV_CLOUD_V2_VF_OPS                                                                    \
    .init_bar_info = devdrv_cloud_v2_init_vf_bar_info,                                            \
    .init_bar_addr_info = devdrv_cloud_v2_init_vf_bar_addr_info,                                  \
    .init_msg_cnt = devdrv_cloud_v2_init_vf_msg_cnt,                                              \
    .boot_mode_rebuild = devdrv_cloud_v2_vf_boot_mode_rebuild,                                    \
    .init_dma_info = NULL,                                                                        \
    .init_setup_runtime_info = devdrv_cloud_v2_init_vf_setup_runtime_info,

#define DEVDRV_CLOUD_V2_PF_INIT_INTR_OPS                                                          \
    .init_intr_info = devdrv_cloud_v2_init_pf_intr_info,

#define DEVDRV_CLOUD_V2_VF_PCIE_INIT_INTR_OPS                                                     \
    .init_intr_info = devdrv_cloud_v2_init_vf_intr_info,

#define DEVDRV_CLOUD_V2_VF_HCCS_INIT_INTR_OPS                                                     \
    .init_intr_info = devdrv_cloud_v2_peh_init_vf_intr_info,

#define DEVDRV_CLOUD_V2_PF_PCIE_CFG(boot, boot_ops)                                               \
    {                                                                                             \
        .mode = (1U << (DEVDRV_SRIOV_TYPE_PF + DEVDRV_FEATURE_PFVF_BIT_OFFSET)) |                 \
                (1U << (CONNECT_PROTOCOL_PCIE + DEVDRV_FEATURE_CONNECT_PROTO_BIT_OFFSET)) |       \
                (1U << ((boot) + DEVDRV_FEATURE_ENV_BOOT_BIT_OFFSET)),                            \
        .ops = {                                                                                  \
            DEVDRV_CLOUD_V2_OPS                                                                   \
            DEVDRV_CLOUD_V2_PF_OPS                                                                \
            DEVDRV_CLOUD_V2_PF_INIT_INTR_OPS                                                      \
            boot_ops                                                                              \
        },                                                                                        \
    }

#define DEVDRV_CLOUD_V2_PF_HCCS_CFG(boot, boot_ops)                                               \
    {                                                                                             \
        .mode = (1U << (DEVDRV_SRIOV_TYPE_PF + DEVDRV_FEATURE_PFVF_BIT_OFFSET)) |                 \
                (1U << (CONNECT_PROTOCOL_HCCS + DEVDRV_FEATURE_CONNECT_PROTO_BIT_OFFSET)) |       \
                (1U << ((boot) + DEVDRV_FEATURE_ENV_BOOT_BIT_OFFSET)),                            \
        .ops = {                                                                                  \
            DEVDRV_CLOUD_V2_OPS                                                                   \
            DEVDRV_CLOUD_V2_PF_OPS                                                                \
            DEVDRV_CLOUD_V2_PF_INIT_INTR_OPS                                                      \
            boot_ops                                                                              \
        },                                                                                        \
    }

#define DEVDRV_CLOUD_V2_VF_PCIE_CFG(boot, boot_ops)                                               \
    {                                                                                             \
        .mode = (1U << (DEVDRV_SRIOV_TYPE_VF + DEVDRV_FEATURE_PFVF_BIT_OFFSET)) |                 \
                (1U << (CONNECT_PROTOCOL_PCIE + DEVDRV_FEATURE_CONNECT_PROTO_BIT_OFFSET)) |       \
                (1U << ((boot) + DEVDRV_FEATURE_ENV_BOOT_BIT_OFFSET)),                            \
        .ops = {                                                                                  \
            DEVDRV_CLOUD_V2_OPS                                                                   \
            DEVDRV_CLOUD_V2_VF_OPS                                                                \
            DEVDRV_CLOUD_V2_VF_PCIE_INIT_INTR_OPS                                                 \
            boot_ops                                                                              \
        },                                                                                        \
    }

#define DEVDRV_CLOUD_V2_VF_HCCS_CFG(boot, boot_ops)                                               \
    {                                                                                             \
        .mode = (1U << (DEVDRV_SRIOV_TYPE_VF + DEVDRV_FEATURE_PFVF_BIT_OFFSET)) |                 \
                (1U << (CONNECT_PROTOCOL_HCCS + DEVDRV_FEATURE_CONNECT_PROTO_BIT_OFFSET)) |       \
                (1U << ((boot) + DEVDRV_FEATURE_ENV_BOOT_BIT_OFFSET)),                            \
        .ops = {                                                                                  \
            DEVDRV_CLOUD_V2_OPS                                                                   \
            DEVDRV_CLOUD_V2_VF_OPS                                                                \
            DEVDRV_CLOUD_V2_VF_HCCS_INIT_INTR_OPS                                                 \
            boot_ops                                                                              \
        },                                                                                        \
    }

struct res_config cloud_v2_res_cfg[] = {
    DEVDRV_CLOUD_V2_PF_PCIE_CFG(DEVDRV_PHY_BOOT, DEVDRV_PHY_BOOT_OPS),
    DEVDRV_CLOUD_V2_PF_HCCS_CFG(DEVDRV_PHY_BOOT, DEVDRV_PHY_BOOT_OPS),

    DEVDRV_CLOUD_V2_VF_PCIE_CFG(DEVDRV_SRIOV_VF_BOOT, DEVDRV_SRIOV_VF_BOOT_OPS),
    DEVDRV_CLOUD_V2_VF_HCCS_CFG(DEVDRV_SRIOV_VF_BOOT, DEVDRV_SRIOV_VF_BOOT_OPS),

    DEVDRV_CLOUD_V2_VF_PCIE_CFG(DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT, DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT_OPS),
    DEVDRV_CLOUD_V2_VF_HCCS_CFG(DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT, DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT_OPS),

    DEVDRV_CLOUD_V2_VF_PCIE_CFG(DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT, DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT_OPS),
    DEVDRV_CLOUD_V2_VF_HCCS_CFG(DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT, DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT_OPS),

    DEVDRV_CLOUD_V2_VF_PCIE_CFG(DEVDRV_MDEV_VF_PM_BOOT, DEVDRV_MDEV_VF_PM_BOOT_OPS),
    DEVDRV_CLOUD_V2_VF_HCCS_CFG(DEVDRV_MDEV_VF_PM_BOOT, DEVDRV_MDEV_VF_PM_BOOT_OPS),

    DEVDRV_CLOUD_V2_VF_PCIE_CFG(DEVDRV_MDEV_VF_VM_BOOT, DEVDRV_MDEV_VF_VM_BOOT_OPS),
    DEVDRV_CLOUD_V2_VF_HCCS_CFG(DEVDRV_MDEV_VF_VM_BOOT, DEVDRV_MDEV_VF_VM_BOOT_OPS),
};
// clang-format on

int devdrv_cloud_v2_res_init(struct devdrv_pci_ctrl *pci_ctrl)
{
#ifndef DRV_UT
    devdrv_feature_ops_bitmap_t mode = devdrv_feature_get_rescfg_bitmap(pci_ctrl);
    struct res_config *res_cfg = NULL;
    size_t res_cfg_size;
    int ret;

    pci_ctrl->features = cloud_v2_features;

    res_cfg_size = KA_BASE_ARRAY_SIZE(cloud_v2_res_cfg);
    res_cfg = devdrv_feature_get_res_cfg(cloud_v2_res_cfg, res_cfg_size, mode);
    if (res_cfg == NULL) {
        return -EINVAL;
    }

    ret = res_cfg->ops.init_bar_info(pci_ctrl);
    if (ret != 0) {
        devdrv_err("Init %s bar info failed.\n", pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_PF ? "pf" : "vf");
        return ret;
    }

    if (res_cfg->ops.boot_mode_rebuild != NULL) {
        res_cfg->ops.boot_mode_rebuild(pci_ctrl);
        mode = devdrv_feature_get_rescfg_bitmap(pci_ctrl);
        res_cfg = devdrv_feature_get_res_cfg(cloud_v2_res_cfg, res_cfg_size, mode);
        if (res_cfg == NULL) {
            return -EINVAL;
        }
    }

    if ((mode & DEVDRV_CONNECT_PROTO_HCCS_MASK) != 0) {
        res_cfg->ops.peh = devdrv_kzalloc(sizeof(struct devdrv_dev_peh_ops), KA_GFP_KERNEL);
        if (res_cfg->ops.peh == NULL) {
            return -ENOMEM;
        }
        ret = devdrv_cloud_v2_peh_ops_init(pci_ctrl, res_cfg->ops.peh);
        if (ret != 0) {
            devdrv_err("Init %s peh ops failed.\n", pci_ctrl->virtfn_flag == DEVDRV_SRIOV_TYPE_PF ? "pf" : "vf");
            devdrv_kfree(res_cfg->ops.peh);
            res_cfg->ops.peh = NULL;
            return ret;
        }
    } else {
        res_cfg->ops.peh = NULL;
    }

    (void)memcpy_s(&pci_ctrl->ops, sizeof(struct devdrv_dev_ops), &res_cfg->ops, sizeof(struct devdrv_dev_ops));
    res_cfg->ops.init_bar_addr_info(pci_ctrl);
    res_cfg->ops.init_intr_info(pci_ctrl);
    res_cfg->ops.init_msg_cnt(pci_ctrl);
    res_cfg->ops.init_setup_runtime_info(pci_ctrl);

    if (res_cfg->ops.init_dma_info != NULL) {
        res_cfg->ops.init_dma_info(pci_ctrl);
    }

    res_cfg->ops.init_load_file_info(pci_ctrl);
    res_cfg->ops.init_depend_module_info(pci_ctrl);

#endif

    return 0;
}
