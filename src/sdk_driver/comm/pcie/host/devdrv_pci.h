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

#ifndef _DEVDRV_PCI_H_
#define _DEVDRV_PCI_H_

#include "ka_base_pub.h"
#include "ka_pci_pub.h"
#include "ka_system_pub.h"
#include "ka_common_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "devdrv_device_load.h"
#include "devdrv_atu.h"
#include "devdrv_dma.h"
#include "securec.h"
#include "devdrv_feature.h"
#include "devdrv_adapt.h"
#include "devdrv_vpc.h"
#include "devdrv_pasid_rbtree.h"

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define PCI_VENDOR_ID_HX 0x2092

#define CLOUD_V1_DEVICE 0xd801
#define MINI_V2_DEVICE 0xd500
#define MINI_V2_2P_DEVICE 0xd501
#define CLOUD_V2_DEVICE 0xd802
#define CLOUD_V2_2P_DEVICE 0xd804
#define CLOUD_V2_HCCS_IEP_DEVICE 0xd803 /* HCCS connection protocol */
#define CLOUD_V2_VF_DEVICE 0xd805
#define MINI_V3_DEVICE 0xd105
#define CLOUD_V4_DEVICE 0xd806
#define CLOUD_V5_DEVICE 0xd807
#define MINI_V4_DEVICE 0xd808
#define MINI_V5_DEVICE 0xd200
#define CLOUD_V6_DEVICE 0xd809

#define DEVDRV_RC_MSI_ADDR 0x820
#define DEVDRV_RC_MSI_UPPER_ADDR 0x824
#define DEVDRV_RC_MSI_EN 0x828
#define DEVDRV_RC_MSI_MASK 0x82C
#define DEVDRV_RC_MSI_STATUS 0x830
#define DEVDRV_MINI_MSI_X_OFFSET 0x10000
#define DEVDRV_DMA_TEST_SIZE 0x100000

/* devdrv_remove is called from prereset or module_exit */
#define DEVDRV_REMOVE_CALLED_BY_PRERESET 0
#define DEVDRV_REMOVE_CALLED_BY_MODULE_EXIT 1

/* device is alive or dead */
#define DEVDRV_DEVICE_DEAD 1    /* heartbeat lost or npu offline */
#define DEVDRV_DEVICE_REMOVE 2  /* pcie remove, intercept msg and dma */
#define DEVDRV_DEVICE_ALIVE 3   /* pcie is normal */
#define DEVDRV_DEVICE_SUSPEND 4 /* pcie dev suspend */
#define DEVDRV_DEVICE_RESUME 5  /* pcie dev resume */
#define DEVDRV_DEVICE_UDA_RM 6  /* uda has remove davinci, intercept dma done irq before uda remove */

/* suspend status */
#define DEVDRV_NORMAL_STARTUP_STATUS 0
#define DEVDRV_SUSPEND_STATUS 1
#define DEVDRV_RESUME_STATUS 2
#define DEVDRV_SUSPEND_STARTUP_STATUS 3
#define DEVDRV_REMOVE_STATUS 4

#define DEVDRV_WAIT_STATE_STATUS_DELAY 10
#define DEVDRV_WAIT_STATE_STATUS_TIMEOUT 100

/* get ram info from device */
#define DEVDRV_DEVICE_RAM_INFO_MAX_NUM 256
#define DEVDRV_DEVICE_RAM_INFO_DATA_SIZE 48 /* all 50 left */
#define DEVDRV_DEVICE_RAM_INFO_NUM_ONE_TIME 4

/* bbox reserved dma mem,2MB per mini */
#define DEVDRV_BBOX_RESVED_MEM_ALLOC_PAGES_ORDER 9
#define DEVDRV_BBOX_RESVED_MEM_SIZE (2 * 1024 * 1024)

#define DEVDRV_BIOS_VERSION_ARR_LEN 4
#define DEVDRV_HOST_RECV_INTERRUPT_FLAG 1

#define DEVDRV_ADDR_ADD 4
#define DEVDRV_ADDR_MOVE_32 32
#define DEVDRV_MSIX_TABLE_SPAN 0x10
#define DEVDRV_MSIX_TABLE_ADDRH 0x4
#define DEVDRV_MSIX_TABLE_NUM 0x8
#define DEVDRV_MSIX_TABLE_MASK 0xC
#define DEVDRV_MSIX_ADDR_BIT 0xFFFFFFFFU
#define DEVDRV_DMA_BIT_MASK_64 64
#define DEVDRV_DMA_BIT_MASK_48 48
#define DEVDRV_DMA_BIT_MASK_32 32
#define DEVDRV_FREE_PAGE_PARA 9

#define DEVDRV_REVISION_TYPE_DEFAULT 0x20
#define DEVDRV_REVISION_TYPE_MDEV_SRIOV_VF 0x71

#define DEVDRV_PCI_SUBSYS_DEV 0X0100UL
#define DEVDRV_1PF2P_PCI_SUBSYS_DEV 0X0110UL
#define DEVDRV_PCI_SUBSYS_VENDOR 0X0200UL
#define DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR 0X19E5UL
#define DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR_HX 0X2092UL
#define DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR_HK 0X20C6UL
#define DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR_KL 0X203FUL
#define DEVDRV_PCI_SUBSYS_PRIVATE_VENDOR_CJ 0X20E9UL
#define DEVDRV_PCI_SUBSYS_DEV_MASK_BIT 4

#define DEVDRV_DAVINCI_DEV_NUM_1PF1P 1
#define DEVDRV_DAVINCI_DEV_NUM_1PF2P 2

#define DEVDRV_SYSFS_ENABLE 1
#define DEVDRV_SYSFS_DISABLE 0

struct devdrv_pgtab_info;

#define MSI_X_MAX_VECTORS 2048
#define DEVDRV_MSI_MAX_VECTORS 32

#define DEVDRV_WAIT_BOOT_MODE_MAX 1000
#define DEVDRV_WAIT_BOOT_MODE_SLEEP_TIME 20

/* msg guard work */
#define DEVDRV_GUARD_WORK_DELAY_MIN 1
#define DEVDRV_GUARD_WORK_DELAY_MAX 1000
#define DEVDRV_GUARD_WORK_DELAY_MULTI 2
/* dma guard work */
#define DEVDRV_DMA_DONE_GUARD_WORK_DELAY 100

#define DEVDRV_CHECK_MODULE_TIMEOUT 1000 /* 1000 * 100ms = 100s */

#define CACHE_INVALID 1 /* Invalid cache before read */
#define CACHE_CLEAN 2   /* Clean and invalid cache after write */

#define DEVDRV_INVALID_PHY_ID 0xFFFFFFFFU

struct devdrv_msix_ctrl {
    u32 next_entry; /* for assign */
    ka_msix_entry_t entries[MSI_X_MAX_VECTORS];
};

struct devdrv_msi_info {
    void *data;
    ka_irqreturn_t (*callback_func)(int, void *);
};

/* command to get ram info from device */
struct devdrv_device_mem_info {
    u64 start_addr;
    u64 offset;
    u32 mem_type; /* enum devdrv_device_mem_type */
    u32 node_id;
};

struct devdrv_dma_test {
    u32 flag;
    ka_timer_list_t test_timer; /* device os load time out timer */
    int timer_remain;           /* timer_remain <= 0 means time out */
    int timer_expires;
};

#define DEVDRV_STARTUP_STATUS_INIT 0
#define DEVDRV_STARTUP_STATUS_TIMEOUT 1
#define DEVDRV_STARTUP_STATUS_FINISH 2

#define DEVDRV_MODULE_INIT_TIMEOUT 300000  /* 300s */
#define DEVDRV_MODULE_FINISH_TIMEOUT 60000 /* 60s */
#define DEVDRV_PCIE_HOT_RESET_ALL_DEVICE_MASK (~0x0ULL)

#define DEVDRV_LOAD_INIT_STATUS 1
#define DEVDRV_LOAD_SUCCESS_STATUS 2
#define DEVDRV_LOAD_HALF_PROBE_STATUS 3
#define DEVDRV_LOAD_HALF_WAIT_COUNT 300

struct devdrv_startup_status {
    int status;
    u32 module_bit_map;
    unsigned long timestamp;
};

struct devdrv_bar_dma_info {
    u64 io_dma_addr;
    u64 io_phy_size;
    u64 io_txatu_base;
    u64 mem_dma_addr;
    u64 mem_phy_size;
    u64 mem_txatu_base;
    u64 db_dma_addr;
    u64 db_phy_size;
    u64 msg_mem_dma_addr;
    u64 msg_mem_phy_size;
    u64 rsv_mem_dma_addr;
    u64 rsv_mem_phy_size;
};

struct devdrv_bar_addr {
    u64 addr;
    size_t size;
};

struct devdrv_intr_info {
    int min_vector;
    int max_vector;
    int device_os_load_irq;
    /* irq used to msg trans, a msg chan need two vector. one for tx finish, the other for rx msg.
       msg chan 0 is used to admin(chan 0) role */
    int msg_irq_base;
    int msg_irq_num;
    /* irq used to dma, a dma chan need 2 vector. one for cq, the other for err.
       host support x dma chan witch is related to enum devdrv_dma_data_type */
    int dma_irq_base;
    int dma_irq_num;
    int tsdrv_irq_base;
    int tsdrv_irq_num;
    int topic_sched_irq_base;
    int topic_sched_irq_num;
    int cdqm_irq_base;
    int cdqm_irq_num;
    /* When the host can apply for enough interrupt vectors,
       allocate the second segment of interrupts to the msg and tsdrv. */
    int msg_irq_vector2_base;
    int msg_irq_vector2_num;
    int tsdrv_irq_vector2_base;
    int tsdrv_irq_vector2_num;
};

#define DEVDRV_IRQ_GEAR_INFO_INVALID 0
#define DEVDRV_IRQ_GEAR_INFO_VALID 1

struct devdrv_irq_gear_info {
    int flag;
    struct devdrv_intr_info intr;
};

#define DEVDRV_RES_GEAR_MAX_VAL 6
#define DEVDRV_RES_GEAR_VAL_CNT (DEVDRV_RES_GEAR_MAX_VAL + 1)

struct devdrv_load_file_info {
    struct devdrv_load_file_cfg *load_file_cfg;
    int load_file_num;
};

#define DEVDRV_MODULE_UNPROBED 0
#define DEVDRV_MODULE_ONLINE 1

struct devdrv_depend_module {
    char *module_name;
    u32 status;
};

struct devdrv_depend_info {
    struct devdrv_depend_module *module_list;
    u32 module_num;
};

struct devdrv_link_info {
    u64 bandwidth;
    u64 packspeed;
    u32 bandwidth_ratio;
    u32 packspeed_ratio;
};

struct devdrv_res_info {
    void __ka_mm_iomem *phy_match_flag_addr;
    void __ka_mm_iomem *nvme_db_base;
    void __ka_mm_iomem *nvme_pf_ctrl_base;
    void __ka_mm_iomem *load_sram_base;

    phys_addr_t rsv_phy_addr;

    struct devdrv_dma_res dma_res;

    struct devdrv_bar_addr msg_db;
    struct devdrv_bar_addr msg_mem;
    struct devdrv_bar_addr s2s_msg;

    struct devdrv_bar_addr ts_db;
    struct devdrv_bar_addr ts_sram;
    struct devdrv_bar_addr ts_sq;
    struct devdrv_bar_addr test;
    struct devdrv_bar_addr load_sram;
    struct devdrv_bar_addr hwts;
    struct devdrv_bar_addr imu_log;
    struct devdrv_bar_addr hdr;
    struct devdrv_bar_addr bbox;
    struct devdrv_bar_addr kdump;
    struct devdrv_bar_addr vmcore;
    struct devdrv_bar_addr bbox_ddr_dump;
    struct devdrv_bar_addr ts_log;
    struct devdrv_bar_addr chip_dfx;
    struct devdrv_bar_addr reg_sram;
    struct devdrv_bar_addr tsdrv_resv;
    struct devdrv_bar_addr devmng_resv;
    struct devdrv_bar_addr devmng_info_mem;
    struct devdrv_bar_addr hbm_ecc_mem;
    struct devdrv_bar_addr vf_bandwidth;
    struct devdrv_bar_addr ts_share_mem;
    struct devdrv_bar_addr ts_notify;
    struct devdrv_bar_addr ts_event;
    struct devdrv_bar_addr l3d_sram;
    struct devdrv_bar_addr stars_sqcq;
    struct devdrv_bar_addr stars_sqcq_intr;
    struct devdrv_bar_addr stars_topic_sched;
    struct devdrv_bar_addr stars_topic_sched_cqe;
    struct devdrv_bar_addr stars_topic_sched_rsv_mem;
    struct devdrv_bar_addr stars_cdqm;
    struct devdrv_bar_addr stars_intr;

    struct devdrv_bar_addr vpc;
    struct devdrv_bar_addr dvpp;

    struct devdrv_intr_info intr;
    struct devdrv_load_file_info load_file;
    struct devdrv_depend_info depend_info;
    /* pcie support msg chan cnt for module */
    int msg_chan_cnt[devdrv_msg_client_max];
    struct devdrv_link_info link_info;
};

#define DEVDRV_VDAVINCI_VIRTUAL_RESERVE_BYTES 40

struct devdrv_priv {
    ka_device_t *dev;
    void *dvt;
    void *ops;
    char reserve[DEVDRV_VDAVINCI_VIRTUAL_RESERVE_BYTES];
};

#define DEVDRV_MAX_MSG_CHAN_NUM 101 /* should be bigger than DEVDRV_MAX_MSG_PF_CHAN_CNT */
struct devdrv_pci_ctrl {
    struct devdrv_agent_load *agent_loader;
    struct devdrv_msg_dev *msg_dev;
    ka_workqueue_struct_t *work_queue[DEVDRV_MAX_MSG_CHAN_NUM];
    struct devdrv_dma_dev *dma_dev;
    struct devdrv_load_work load_work;
    struct devdrv_dma_test *dma_test;
    ka_pci_dev_t *pdev;
    struct devdrv_pgtab_info *pgtab_info;
    struct devdrv_shr_para __ka_mm_iomem *shr_para;
    struct devdrv_res_info res;
    struct devdrv_dev_ops ops;
    devdrv_feature_bitmap_t features;
    void __ka_mm_iomem *io_base;                /* pcie io bar base */
    void __ka_mm_iomem *msi_base;               /* pcie msg db base */
    void __ka_mm_iomem *mem_base;               /* pcie msg sqcq base */
    void __ka_mm_iomem *local_reserve_mem_base; /* local reserve mem base */
    void __ka_mm_iomem *rc_reg_base;
    phys_addr_t io_phy_base;
    u64 io_phy_size;
    phys_addr_t mem_phy_base;
    u64 mem_phy_size;
    phys_addr_t rsv_mem_phy_base;
    u64 rsv_mem_phy_size;
    phys_addr_t mdev_rsv_mem_phy_base;
    u64 mdev_rsv_mem_phy_size;
    u64 host_mem_dma_addr;
    u64 host_mem_phy_size;
    struct devdrv_bar_dma_info target_bar[DEVDRV_P2P_SUPPORT_MAX_DEVNUM];
    phys_addr_t mem_base_paddr;
    u32 dev_id; /* return from devdrv_ctrl layer */
    u32 remote_dev_id;
    u32 slot_id; /* slot (device) id of BDF */
    u32 func_id; /* function id of BDF */
    u32 os_load_flag;
    u32 chip_type;
    u32 virtfn_flag;
    int env_boot_mode;
    int connect_protocol;
    int addr_mode;
    int multi_die;
    u32 load_half_probe;
    u32 add_davinci_flag;
    int load_vector;
    ka_work_struct_t half_probe_work;
    /* device boot status */
    u32 device_boot_status;
    /* devdrv_remove is called by prereset or module_exit */
    u32 module_exit_flag;
    /* load status flag */
    u32 load_status_flag;
    /*
    device is alive or dead
    if dead, no need to send admin msg for save time
    */
    u32 device_status;

    struct devdrv_startup_status startup_status;

    /* defined for normal host */
    int msix_irq_num;
    struct devdrv_msix_ctrl msix_ctrl;

    /* defined for 3559 */
    void *msi_addr;
    ka_dma_addr_t msi_dma_addr;
    struct devdrv_msi_info msi_info[DEVDRV_MSI_MAX_VECTORS];
    u32 mem_bar_id;
    struct devdrv_iob_atu mem_rx_atu[DEVDRV_MAX_RX_ATU_NUM];

    /* reserved dma mem for bbox to ddr dump */
    u32 bbox_resv_size;
    u64 bbox_resv_dmaAddr;
    ka_page_t *bbox_resv_dmaPages;
    /* msg guard work */
    ka_delayed_work_t guard_work;
    u32 guard_work_delay;
    u32 dev_id_in_pdev;
    int vector_num;
    u32 msix_offset;
    phys_addr_t mem_bar_offset;
    phys_addr_t rsv_mem_bar_offset;
    phys_addr_t io_bar_offset;

    /* reference count for pci_ctrl */
    ka_atomic_t ref_cnt;
    struct devdrv_dma_iova_addr_range *iova_range;
    ka_rb_root_t dma_desc_rbtree;
    ka_task_spinlock_t dma_desc_rblock;
    u32 hccs_status;
    u32 hccs_group_id[HCCS_GROUP_SUPPORT_MAX_CHIPNUM];
    u32 bar_wc_flag;
    u32 ep_pf_index;
    u32 is_sriov_enabled;
};

#define PDEV_MAX_DEV_NUM 2
struct devdrv_pdev_ctrl {
    struct devdrv_priv vdavinci_priv; /* MUST at first one, shared with UVP */
    int dev_num;
    struct devdrv_pci_ctrl *pci_ctrl[PDEV_MAX_DEV_NUM];
    u32 main_dev_id;
    u32 sysfs_flag;
};

void devdrv_set_pcie_info_to_dbl(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_set_resmng_hccs_link_status_and_group_id(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_set_resmng_pdev_by_devid(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_set_resmng_host_phy_match_flag(struct devdrv_pci_ctrl *pci_ctrl);
bool devdrv_is_tsdrv_irq(const struct devdrv_intr_info *intr, int irq);
void devdrv_bind_irq(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_unbind_irq(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_load_half_probe(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_load_half_free(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_init_dev_num(void);
int devdrv_get_dev_num(void);
void devdrv_notify_dev_init_status(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_guard_work_sched_immediate(struct devdrv_pci_ctrl *pci_ctrl);

extern int devdrv_sysfs_init(ka_pci_dev_t *pdev);
extern void devdrv_sysfs_exit(ka_pci_dev_t *pdev);
bool devdrv_is_pdev_main_davinci_dev(struct devdrv_pci_ctrl *pci_ctrl);
struct devdrv_pci_ctrl *devdrv_get_dev_by_index_in_pdev(ka_pci_dev_t *pdev, int dev_index);
struct devdrv_pci_ctrl *devdrv_get_pdev_main_davinci_dev(ka_pci_dev_t *pdev);
int devdrv_get_davinci_dev_num_by_pdev(ka_pci_dev_t *pdev);
u32 devdrv_get_main_davinci_devid_by_pdev(ka_pci_dev_t *pdev);

int devdrv_init_interrupt_normal(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_uninit_interrupt_normal(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_uninit_interrupt(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_vf_half_probe(u32 index_id);
int devdrv_vf_half_free(u32 index_id);
void devdrv_init_shr_info_after_half_probe(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_pcictrl_shr_para_init(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_get_product(void);
extern ka_pci_driver_t g_devdrv_driver_ver;
int devdrv_get_pdev_davinci_dev_num(u32 device, u16 subsystem_device);
void devdrv_guard_work_uninit(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_guard_work_init(struct devdrv_pci_ctrl *pci_ctrl);
ka_irqreturn_t devdrv_half_probe_irq(int irq, void *data);
int devdrv_cfg_pdev(ka_pci_dev_t *pdev);
void devdrv_uncfg_pdev(ka_pci_dev_t *pdev);
void drv_pcie_remove(ka_pci_dev_t *pdev);
void devdrv_init_hccs_link_info(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_shr_para_rebuild(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_pre_cfg(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_add_davinci_dev(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_remove_davinci_dev(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_init_interrupt(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_notify_uda_remove(struct devdrv_pci_ctrl *pci_ctrl);

/*****************************************************************************************
 * is_mdev_vf_pm                      : computing group virtual machine PM mode, include mdev full spec vf pm.
 * is_mdev_vf_vm                      : computing group virtual machine VM mode, include mdev full spec vf vm.
 * is_p2p_access_cap_supported        : P2P access capability is supported.
 * is_guard_work_supported            : the DMA copy fallback mechanism is supported to prevent interrupt loss.
 * mdev_pm_init                       : computing group virtual machine PM mode initialization.
 * mdev_pm_uninit                     : computing group virtual machine PM mode deinitialization.
 * mdev_vm_init                       : computing group virtual machine VM mode initialization.
 * mdev_vm_uninit                     : computing group virtual machine VM mode deinitialization.
 * add_davinci_dev                    : add davinci dev. In the computing group virtual machine, PM mode only performs
 *                                      DMA operations and does not require add davinci dev. In container scenario,
 *                                      add davinci dev will be handled by the vmng module.
 * remove_davinci_dev                 : remove davinci dev.
 * init_interrupt                     : init interrupt. In the computing group virtual machine, PM mode only performs
 *                                      DMA operations and does not require init interrupt.
 * uninit_interrupt                   : uninit interrupt.
 * dma_pasid_valid_check              : pasid valid check, since pasid is insecure under VM, the check is unnecessary.

 *****************************************************************************************/

// clang-format off

// physical machine mode
#define DEVDRV_PHY_BOOT_OPS                                                                    \
    .is_mdev_vf_pm = false,                                                                    \
    .is_mdev_vf_vm = false,                                                                    \
    .is_p2p_access_cap_supported = true,                                                       \
    .is_guard_work_supported = true,                                                           \
    .mdev_pm_init = NULL,                                                                      \
    .mdev_pm_uninit = NULL,                                                                    \
    .mdev_vm_init = NULL,                                                                      \
    .mdev_vm_uninit = NULL,                                                                    \
    .add_davinci_dev = devdrv_add_davinci_dev,                                                 \
    .remove_davinci_dev = devdrv_remove_davinci_dev,                                           \
    .init_interrupt = devdrv_init_interrupt,                                                   \
    .uninit_interrupt = devdrv_uninit_interrupt,                                               \
    .dma_pasid_valid_check = devdrv_dma_pasid_valid_check,

// docker mode
#define DEVDRV_SRIOV_VF_BOOT_OPS                                                               \
    .is_mdev_vf_pm = false,                                                                    \
    .is_mdev_vf_vm = false,                                                                    \
    .is_p2p_access_cap_supported = true,                                                       \
    .is_guard_work_supported = true,                                                           \
    .mdev_pm_init = NULL,                                                                      \
    .mdev_pm_uninit = NULL,                                                                    \
    .mdev_vm_init = NULL,                                                                      \
    .mdev_vm_uninit = NULL,                                                                    \
    .add_davinci_dev = NULL,                                                                   \
    .remove_davinci_dev = devdrv_notify_uda_remove,                                            \
    .init_interrupt = devdrv_init_interrupt,                                                   \
    .uninit_interrupt = devdrv_uninit_interrupt,                                               \
    .dma_pasid_valid_check = devdrv_dma_pasid_valid_check,

// mdev computing group 1:1 physical machine mode
#define DEVDRV_MDEV_FULL_SPEC_VF_PM_BOOT_OPS                                                   \
    .is_mdev_vf_pm = true,                                                                     \
    .is_mdev_vf_vm = false,                                                                    \
    .is_p2p_access_cap_supported = true,                                                       \
    .is_guard_work_supported = false,                                                          \
    .mdev_pm_init = devdrv_mdev_pm_init,                                                       \
    .mdev_pm_uninit = devdrv_mdev_pm_uninit,                                                   \
    .mdev_vm_init = NULL,                                                                      \
    .mdev_vm_uninit = NULL,                                                                    \
    .add_davinci_dev = NULL,                                                                   \
    .remove_davinci_dev = devdrv_notify_uda_remove,                                            \
    .init_interrupt = NULL,                                                                    \
    .uninit_interrupt = NULL,                                                                  \
    .dma_pasid_valid_check = devdrv_dma_pasid_valid_check,

// mdev computing group 1:1 virtual machine mode
#define DEVDRV_MDEV_FULL_SPEC_VF_VM_BOOT_OPS                                                   \
    .is_mdev_vf_pm = false,                                                                    \
    .is_mdev_vf_vm = true,                                                                     \
    .is_p2p_access_cap_supported = true,                                                       \
    .is_guard_work_supported = true,                                                           \
    .mdev_pm_init = NULL,                                                                      \
    .mdev_pm_uninit = NULL,                                                                    \
    .mdev_vm_init = devdrv_mdev_vm_init,                                                       \
    .mdev_vm_uninit = devdrv_mdev_vm_uninit,                                                   \
    .add_davinci_dev = devdrv_add_davinci_dev,                                                 \
    .remove_davinci_dev = devdrv_remove_davinci_dev,                                           \
    .init_interrupt = devdrv_init_interrupt,                                                   \
    .uninit_interrupt = devdrv_uninit_interrupt,                                               \
    .dma_pasid_valid_check = NULL,

// mdev computing group non-1:1 physical machine mode
#define DEVDRV_MDEV_VF_PM_BOOT_OPS                                                             \
    .is_mdev_vf_pm = true,                                                                     \
    .is_mdev_vf_vm = false,                                                                    \
    .is_p2p_access_cap_supported = true,                                                       \
    .is_guard_work_supported = false,                                                          \
    .mdev_pm_init = devdrv_mdev_pm_init,                                                       \
    .mdev_pm_uninit = devdrv_mdev_pm_uninit,                                                   \
    .mdev_vm_init = NULL,                                                                      \
    .mdev_vm_uninit = NULL,                                                                    \
    .add_davinci_dev = NULL,                                                                   \
    .remove_davinci_dev = devdrv_notify_uda_remove,                                            \
    .init_interrupt = NULL,                                                                    \
    .uninit_interrupt = NULL,                                                                  \
    .dma_pasid_valid_check = devdrv_dma_pasid_valid_check,

// mdev computing group non-1:1 virtual machine mode
#define DEVDRV_MDEV_VF_VM_BOOT_OPS                                                             \
    .is_mdev_vf_pm = false,                                                                    \
    .is_mdev_vf_vm = true,                                                                     \
    .is_p2p_access_cap_supported = false,                                                      \
    .is_guard_work_supported = true,                                                           \
    .mdev_pm_init = NULL,                                                                      \
    .mdev_pm_uninit = NULL,                                                                    \
    .mdev_vm_init = devdrv_mdev_vm_init,                                                       \
    .mdev_vm_uninit = devdrv_mdev_vm_uninit,                                                   \
    .add_davinci_dev = devdrv_add_davinci_dev,                                                 \
    .remove_davinci_dev = devdrv_remove_davinci_dev,                                           \
    .init_interrupt = devdrv_init_interrupt,                                                   \
    .uninit_interrupt = devdrv_uninit_interrupt,                                               \
    .dma_pasid_valid_check = NULL,

// clang-format on

#endif
