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

#ifndef DEVDRV_ADAPT_H
#define DEVDRV_ADAPT_H

#include "ka_common_pub.h"
#include "comm_kernel_interface.h"
#include "devdrv_feature.h"

#define HOST_PRODUCT_DC 0

/* reserve mem pool for hdc */
#define DEVDRV_RESERVE_MEM_PHY_ADDR (0x0ULL)
#define DEVDRV_RESERVE_MEM_SIZE (0x0ULL)

/* devdrv_dma.h */
#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA) /* for fpga */
#define DEVDRV_DMA_COPY_TIMEOUT (KA_HZ * 1000)
#define DEVDRV_DMA_COPY_MAX_TIMEOUT (KA_HZ * 1000)
#define DEVDRV_DMA_WAIT_CHAN_AVAIL_TIMEOUT 20000
#else
/* Due to the impact of the previously unfinished dma task, the time-out period cannot
  be given based on the amount of data moved by the dma. Consider the influence of
  the PCIE bus bandwidth and the multi-channel of the DMA, giving a larger waiting time  */
#define DEVDRV_DMA_COPY_TIMEOUT (KA_HZ * 50)      /* 50s */
#define DEVDRV_DMA_COPY_MAX_TIMEOUT (KA_HZ * 100) /* 100s */
/* wait for dma chan SQ queue when full */
#define DEVDRV_DMA_WAIT_CHAN_AVAIL_TIMEOUT 10000
#endif

/* 10s */
#define DEVDRV_DMA_QUERY_MAX_WAIT_TIME 10000000
/* 36s */
#define DEVDRV_DMA_QUERY_MAX_WAIT_LONG_TIME 36000000

/* devdrv_msg_def.h */
#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define DEVDRV_MSG_TIMEOUT 200000000          /* 200s for fpga  */
#define DEVDRV_ADMIN_MSG_TIMEOUT 200000000    /* 200s for fpga  */
#define DEVDRV_ADMIN_MSG_IRQ_TIMEOUT 50000000 /* 50s */
#define DEVDRV_MSG_WAIT_MIN_TIME 200          /* 200us */
#define DEVDRV_MSG_WAIT_MAX_TIME 400          /* 400us */
#define DEVDRV_ADMIN_MSG_WAIT_MIN_TIME 200    /* 200us */
#define DEVDRV_ADMIN_MSG_WAIT_MAX_TIME 400    /* 400us */
#else
#ifdef CFG_BUILD_ASAN
#define DEVDRV_MSG_TIMEOUT 60000000       /* 60s for asan */
#define DEVDRV_ADMIN_MSG_TIMEOUT 60000000 /* 60s for asan */
#else
#define DEVDRV_MSG_TIMEOUT 5000000        /* 5s */
#define DEVDRV_ADMIN_MSG_TIMEOUT 15000000 /* 15s for admin */
#endif
#define DEVDRV_ADMIN_MSG_IRQ_TIMEOUT 5000000 /* 5s for admin */
#define DEVDRV_MSG_WAIT_MIN_TIME 1           /* 1us */
#define DEVDRV_MSG_WAIT_MAX_TIME 2           /* 2us */
#define DEVDRV_ADMIN_MSG_WAIT_MIN_TIME 10    /* 10us */
#define DEVDRV_ADMIN_MSG_WAIT_MAX_TIME 11    /* 11us */
#endif

#define DEVDRV_ADMIN_MSG_TIMEOUT_LONG DEVDRV_ADMIN_MSG_TIMEOUT
#define DEVDRV_MSG_RETRY_LIMIT 5

extern int (*devdrv_res_init_func[HISI_CHIP_NUM])(struct devdrv_pci_ctrl *pci_ctrl);

/* devdrv_pcie_link_info.h */
void devdrv_peer_ctrl_init(void);

/* devdrv_pcie.c */
int devdrv_get_product(void);
extern const ka_pci_device_id_t g_devdrv_tbl[];
extern const ka_pci_error_handlers_t g_devdrv_err_handler;
int devdrv_get_device_id_tbl_num(void);
void devdrv_shutdown(ka_pci_dev_t *pdev);

/* devdrv_dma.c */
void devdrv_traffic_and_manage_dma_chan_config(struct devdrv_dma_dev *dma_dev);

/* devdrv_pm.c */
bool devdrv_is_sentry_work_mode(void);
void devdrv_load_half_resume(struct devdrv_pci_ctrl *pci_ctrl);
int drv_pcie_suspend(ka_device_t *dev);
int drv_pcie_resume_notify(ka_device_t *dev);

#define DEVDRV_COMMON_OPS                                                                                     \
    void (*init_load_file_info)(struct devdrv_pci_ctrl * pci_ctrl);                                           \
    void (*init_depend_module_info)(struct devdrv_pci_ctrl * pci_ctrl);                                       \
    int (*alloc_devid)(struct devdrv_ctrl * ctrl_this);                                                       \
    int (*is_p2p_access_cap)(struct devdrv_pci_ctrl * pci_ctrl, struct devdrv_pci_ctrl * peer_pci_ctrl);      \
    enum devdrv_load_wait_mode (*get_load_wait_mode)(struct devdrv_pci_ctrl * pci_ctrl);                      \
    int (*get_pf_max_msg_chan_cnt)(void);                                                                     \
    int (*get_vf_max_msg_chan_cnt)(void);                                                                     \
    u32 (*get_p2p_support_max_devnum)(void);                                                                  \
    void (*set_dev_shr_info)(struct devdrv_pci_ctrl * pci_ctrl);                                              \
    u32 (*get_nvme_low_level_db_irq_num)(void);                                                               \
    u32 (*get_nvme_db_irq_strde)(void);                                                                       \
    void (*get_vf_dma_info)(struct devdrv_pci_ctrl * pci_ctrl);                                               \
    bool (*is_mdev_vm_full_spec)(struct devdrv_pci_ctrl * pci_ctrl);                                          \
    int (*get_p2p_addr)(struct devdrv_pci_ctrl * pci_ctrl, u32 remote_dev_id, enum devdrv_p2p_addr_type type, \
                        phys_addr_t * phy_addr, size_t * size);                                               \
    unsigned int (*get_server_id)(struct devdrv_pci_ctrl * pci_ctrl);                                         \
    unsigned int (*get_max_server_num)(struct devdrv_pci_ctrl * pci_ctrl);                                    \
    void (*init_virt_info)(struct devdrv_pci_ctrl * pci_ctrl);                                                \
    void (*init_link_info)(struct devdrv_pci_ctrl * pci_ctrl);                                                \
    int (*set_udevid_reorder_para)(struct devdrv_pci_ctrl * pci_ctrl);                                        \
    int (*devdrv_deal_suspend_handshake)(struct devdrv_pci_ctrl * pci_ctrl);                                  \
    int (*check_ep_suspend_status)(struct devdrv_pci_ctrl * pci_ctrl);                                        \
    int (*single_fault_init)(struct devdrv_pci_ctrl * pci_ctrl);                                              \
    int (*single_fault_uninit)(struct devdrv_pci_ctrl * pci_ctrl)

#define DEVDRV_PFVF_COMMON_OPS                                     \
    int (*init_bar_info)(struct devdrv_pci_ctrl * pci_ctrl);       \
    void (*init_bar_addr_info)(struct devdrv_pci_ctrl * pci_ctrl); \
    void (*init_msg_cnt)(struct devdrv_pci_ctrl * pci_ctrl);       \
    void (*boot_mode_rebuild)(struct devdrv_pci_ctrl * pci_ctrl);  \
    void (*init_dma_info)(struct devdrv_pci_ctrl * pci_ctrl);      \
    void (*init_setup_runtime_info)(struct devdrv_pci_ctrl * pci_ctrl)

#define DEVDRV_INIT_INTR_OPS void (*init_intr_info)(struct devdrv_pci_ctrl * pci_ctrl)

struct devdrv_dev_peh_ops {
    int (*msi_x_table_retrans)(struct devdrv_pci_ctrl *pci_ctrl);
    void (*flush_cache)(u64 base, size_t len, u32 mode);
    int (*get_peh_link_info)(ka_pci_dev_t *pdev, u32 *link_speed, u32 *link_width, u32 *link_status);
    void (*link_speed_slow_to_normal)(struct devdrv_pci_ctrl *pci_ctrl);
    void (*pdev_sid_init)(struct devdrv_pci_ctrl *pci_ctrl);
    void (*pdev_sid_uninit)(struct devdrv_pci_ctrl *pci_ctrl);
    int (*smmu_iova_to_phys_proc)(struct devdrv_pci_ctrl *pci_ctrl, ka_dma_addr_t *va, u32 va_cnt, phys_addr_t *pa);
};

#define DEVDRV_CONNECT_PROTO_HCCS_MASK (1ULL << (CONNECT_PROTOCOL_HCCS + DEVDRV_FEATURE_CONNECT_PROTO_BIT_OFFSET))

#define DEVDRV_ENV_BOOT_COMMON_OPS                                \
    bool is_mdev_vf_pm;                                           \
    bool is_mdev_vf_vm;                                           \
    bool is_p2p_access_cap_supported;                             \
    bool is_guard_work_supported;                                 \
    int (*mdev_pm_init)(struct devdrv_pci_ctrl * pci_ctrl);       \
    void (*mdev_pm_uninit)(struct devdrv_pci_ctrl * pci_ctrl);    \
    int (*mdev_vm_init)(struct devdrv_pci_ctrl * pci_ctrl);       \
    void (*mdev_vm_uninit)(struct devdrv_pci_ctrl * pci_ctrl);    \
    int (*add_davinci_dev)(struct devdrv_pci_ctrl * pci_ctrl);    \
    int (*remove_davinci_dev)(struct devdrv_pci_ctrl * pci_ctrl); \
    int (*init_interrupt)(struct devdrv_pci_ctrl * pci_ctrl);     \
    int (*uninit_interrupt)(struct devdrv_pci_ctrl * pci_ctrl);   \
    bool (*dma_pasid_valid_check)(u32 dev_id, u64 pasid)

struct devdrv_dev_ops {
    DEVDRV_COMMON_OPS;
    DEVDRV_PFVF_COMMON_OPS;
    DEVDRV_INIT_INTR_OPS;
    DEVDRV_ENV_BOOT_COMMON_OPS;
    struct devdrv_dev_peh_ops *peh;
};

struct res_config {
    const devdrv_feature_ops_bitmap_t mode;
    struct devdrv_dev_ops ops;
};

devdrv_feature_ops_bitmap_t devdrv_feature_get_rescfg_bitmap(struct devdrv_pci_ctrl *pci_ctrl);
struct res_config *devdrv_feature_get_res_cfg(struct res_config *res_cfg, size_t res_cfg_size,
                                              devdrv_feature_ops_bitmap_t mode);

#endif
