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

#ifndef _DEVDRV_CTRL_H_
#define _DEVDRV_CTRL_H_

#include "ka_memory_pub.h"
#include "comm_kernel_interface.h"
#include "devdrv_msg.h"
#include "devdrv_pci.h"

#define DEVDRV_MAX_DELAY_COUNT 20
#define DEVDRV_HOT_RESET_DELAY 3
#define DEVDRV_HOT_RESET_LONG_DELAY 5 /* FOR HCCS over PCIE */
#define DEVDRV_HOT_RESET_AFTER_DELAY 10
#define DEVDRV_HOT_RESAN_AFTER_DELAY 50
#define MAX_OS_EACH_AISERVER 2

#define DEVDRV_EP_SUSPEND_READY 1
#define DEVDRV_SECOND_TO_USECOND 1000000U
#define DEVDRV_GET_EP_SUSPEND_DELAY 20000U
#define DEVDRV_GET_EP_SUSPEND_TIMEOUT 900
#define DEVDRV_GET_EP_SUSPEND_RANGE 10
#define DEVDRV_SUSPEND_ONE_SECOND 50
#ifdef CFG_FEATURE_SRIOV
#define MAX_DEV_CNT 1124
#else
#define MAX_DEV_CNT 64
#endif
#define MAX_PF_DEV_CNT 64
#define MAX_VF_CNT_OF_PF 16U
#define DEVDRV_SRIOV_VF_DEVID_START 100U
#define DEVDRV_MAX_SRIOV_INSTANCE 8U

#define DEVDRV_PCIE_HOT_RESET_FLAG 0x5b5bb5b5
#define DEVDRV_LOAD_FILE_DELAY 10000
#define DEVDRV_DEVFN_BIT 3
#define DEVDRV_DEVFN_DEV_VAL 0x1F
#define DEVDRV_DEVFN_FN_VAL 0x7
#define DEVDRV_RESERVE_NUM 2
#define DEVDRV_MOVE_BIT_32 32
#define DEVDRV_MAX_FUNC_NUM 2
#define DEVDRV_DMI_DEV_TYPE_DEV_SLOT (-4) /* same value as DMI_DEV_TYPE_DEV_SLOT defined in dmi.h */

#define DEVDRV_MAX_CAPABILITY_VALUE_NUM 2
#define DEVDRV_MAX_CAPABILITY_VALUE_OFFSET 4

#define DEVDRV_P2P_CAPABILITY_SHIFT_32 32
#define DEVDRV_P2P_ACCESS_DISABLE 0
#define DEVDRV_P2P_ACCESS_ENABLE 1
#define DEVDRV_P2P_ACCESS_UNKNOWN 2

#define DEVDRV_H2D_ATTR_INVALID 0
#define DEVDRV_H2D_ATTR_VALID 1

#define DEVDRV_P2P_CAPA_SIGN_BIT 24
#define DEVDRV_P2P_CAPA_SIGN_VAL 0xFFFFFF
#define DEVDRV_P2P_GROUP_SIGN_BIT 51
#define DEVDRV_P2P_GROUP_SIGN_VAL 0xF

#define REF_CNT_CHECK_PRINT_FREQUENCY 1000 /* 1s */
#define REF_CNT_CHECK_LIMIT 120000         /* 120s */
#define REF_CNT_CHECK_WAIT_L 1000          /* 1000us */
#define REF_CNT_CHECK_WAIT_H 1010          /* 1010us */

/* PCIe Speed value in regs */
enum devdrv_pcie_speed_reg_enum {
    DEVDRV_BANDW_PCIESPD_GEN1_REG = 1,
    DEVDRV_BANDW_PCIESPD_GEN2_REG = 2,
    DEVDRV_BANDW_PCIESPD_GEN3_REG = 3,
    DEVDRV_BANDW_PCIESPD_GEN4_REG = 4,
    DEVDRV_BANDW_PCIESPD_GEN5_REG = 5,
    DEVDRV_BANDW_PCIESPD_GEN6_REG = 6,
    DEVDRV_BANDW_PCIESPD_GEN_MAX
};

/* PCIe Speed value define */
#define DEVDRV_BANDW_INVALID_SPEED 0
#define DEVDRV_BANDW_X1_SPEED_GEN1 (256 * 1024 * 1024ULL)      // 256MB/s
#define DEVDRV_BANDW_X1_SPEED_GEN2 (512 * 1024 * 1024ULL)      // 512MB/s
#define DEVDRV_BANDW_X1_SPEED_GEN3 (1 * 1024 * 1024 * 1024ULL) // 1GB/s
#define DEVDRV_BANDW_X1_SPEED_GEN4 (2 * 1024 * 1024 * 1024ULL) // 2GB/s
#define DEVDRV_BANDW_X1_SPEED_GEN5 (4 * 1024 * 1024 * 1024ULL) // 4GB/s
#define DEVDRV_BANDW_X1_SPEED_GEN6 (8 * 1024 * 1024 * 1024ULL) // 8GB/s

struct devdrv_ops {
    /* trans msg */
    struct devdrv_msg_chan *(*alloc_trans_chan)(void *priv, struct devdrv_trans_msg_chan_info *chan_info);
    int (*realease_trans_chan)(struct devdrv_msg_chan *msg_chan);

    /* non-trans msg */
    struct devdrv_msg_chan *(*alloc_non_trans_chan)(void *priv, struct devdrv_non_trans_msg_chan_info *chan_info);
    int (*release_non_trans_chan)(struct devdrv_msg_chan *msg_chan);
};

/* record startup mini and report to dev manager */
#define DEVDRV_STATE_UNPROBE 0
#define DEVDRV_STATE_PROBE 1
struct devdrv_dev_state_ctrl {
    /* total mini nums */
    u32 dev_num;
    /* initial reporting label */
    u32 first_flag;
    u32 reset_devid;
    u32 devid[MAX_DEV_CNT];
    u32 state_flag[MAX_DEV_CNT];
    devdrv_dev_startup_notify startup_callback;
    devdrv_dev_state_notify state_notifier_callback;
};

struct devdrv_pci_state_ctrl {
    devdrv_suspend_pre_check suspend_pre_check;
    devdrv_peer_fault_notify peer_fault_notify;
};

struct devdrv_pci_ctrl_mng {
    ka_rwlock_t lock[MAX_DEV_CNT];
    struct devdrv_pci_ctrl **mng_table;
};

#define DEVDRV_P2P_MAX_PROC_NUM 32

struct devdrv_p2p_attr_info {
    int ref;
    int proc_ref[DEVDRV_P2P_MAX_PROC_NUM];
    int pid[DEVDRV_P2P_MAX_PROC_NUM];
};

#define DEVDRV_H2D_MAX_PROC_NUM 32
#define DEVDRV_H2D_MAX_ATU_NUM 6

struct devdrv_h2d_attr_info {
    int ref;
    int proc_ref[DEVDRV_H2D_MAX_PROC_NUM];
    int pid[DEVDRV_H2D_MAX_PROC_NUM];
    ka_dma_addr_t addr;
    u64 size;
    u32 valid;
};

struct devdrv_p2p_config_info {
    u32 cnt;
};

#define DEVDRV_RES_GEAR_RESV_NUM 5

struct devdrv_res_gear {
    u32 irq_res_gear;
    u32 reserve[DEVDRV_RES_GEAR_RESV_NUM];
};

#define DEVDRV_DMA_IOVA_RANGE_UNINIT 0
#define DEVDRV_DMA_IOVA_RANGE_INIT 1
struct devdrv_dma_iova_addr_range {
    int init_flag;
    u64 start_addr;
    u64 end_addr;
};

struct devdrv_dma_desc_rbtree_node {
    ka_rb_node_t node;
    u64 hash_va;
    struct devdrv_dma_prepare *dma_prepare;
};
void devdrv_dma_desc_node_uninit(struct devdrv_pci_ctrl *pci_ctrl);
typedef int (*pci_bridge_secondary_bus_reset_func)(ka_pci_dev_t *dev);
void devdrv_set_hccs_link_status(u32 dev_id, u32 val);
int devdrv_p2p_para_check(int pid, u32 dev_id, u32 peer_dev_id);
void devdrv_clear_p2p_resource(u32 devid);
void devdrv_clear_h2d_txatu_resource(u32 devid);
bool devdrv_is_dev_hot_reset(void);
struct devdrv_ctrl *get_devdrv_ctrl(void);
int devdrv_sriov_get_pf_devid_by_vf_ctrl(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_alloc_devid_check_ctrls(const struct devdrv_ctrl *ctrl_this);
int devdrv_alloc_devid_inturn(u32 beg, u32 stride);
int devdrv_alloc_devid_stride_2(struct devdrv_ctrl *ctrl_this);

void devdrv_slave_dev_delete(u32 dev_id);

u32 devdrv_get_devid_by_dev(const struct devdrv_msg_dev *msg_dev);

int devdrv_register_pci_devctrl(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_set_device_boot_status(struct devdrv_pci_ctrl *pci_ctrl, u32 status);

int devdrv_dev_online(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_dev_offline(struct devdrv_pci_ctrl *pci_ctrl);
int devdrv_ctrl_init(void);
void devdrv_ctrl_uninit(void);
void devdrv_set_ctrl_priv(u32 dev_id, void *priv);
int devdrv_init_pci_devctrl(struct devdrv_pci_ctrl *pci_ctrl);
struct devdrv_ctrl *devdrv_get_top_half_devctrl_by_id(u32 dev_id);
struct devdrv_ctrl *devdrv_get_bottom_half_devctrl_by_id(u32 dev_id);
struct devdrv_ctrl *devdrv_get_devctrl_by_id(u32 i);
struct devdrv_pci_ctrl *devdrv_get_pci_ctrl_by_id(u32 dev_id);
struct devdrv_pci_ctrl *devdrv_get_top_half_pci_ctrl_by_id(u32 dev_id);
struct devdrv_pci_ctrl *devdrv_get_bottom_half_pci_ctrl_by_id(u32 dev_id);
void devdrv_init_dev_startup_ctrl(void);
void drvdrv_dev_startup_record(u32 dev_id);
void drvdrv_dev_startup_report(u32 dev_id);
void drvdrv_dev_state_notifier(struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_set_devctrl_startup_flag(u32 dev_id, enum devdrv_dev_startup_flag_type flag);
void devdrv_register_half_devctrl(struct devdrv_pci_ctrl *pci_ctrl);
ka_pci_dev_t *devdrv_get_device_pf(ka_pci_dev_t *pdev, unsigned int pf_num);
void devdrv_set_startup_status(struct devdrv_pci_ctrl *pci_ctrl, int status);
void devdrv_probe_wait(int devid);
int devdrv_alloc_attr_info(void);
void devdrv_free_attr_info(void);
void devdrv_clients_instance_uninit(void);
int devdrv_get_connect_protocol_by_dev(ka_device_t *dev);

int devdrv_pci_ctrl_mng_init(void);
void devdrv_pci_ctrl_mng_uninit(void);
struct devdrv_pci_ctrl_mng *devdrv_get_pci_ctrl_mng(void);
void devdrv_pci_ctrl_add(u32 dev_id, struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_pci_ctrl_del_wait(u32 dev_id, struct devdrv_pci_ctrl *pci_ctrl);
void devdrv_pci_ctrl_del(u32 dev_id, struct devdrv_pci_ctrl *pci_ctrl);
struct devdrv_pci_ctrl *devdrv_pci_ctrl_get(u32 dev_id);
struct devdrv_pci_ctrl *devdrv_pci_ctrl_get_no_ref(u32 dev_id);
void devdrv_pci_ctrl_put(struct devdrv_pci_ctrl *pci_ctrl);

u32 devdrv_get_irq_res_gear(void);
void devdrv_parse_res_gear(void);
u64 devdrv_get_bandwidth_info(ka_pci_dev_t *pdev);

void *devdrv_pcimsg_alloc_non_trans_queue_inner(u32 index_id, struct devdrv_non_trans_msg_chan_info *chan_info);
int devdrv_pcimsg_free_non_trans_queue_inner(void *msg_chan);

int devdrv_get_sriov_and_mdev_mode(u32 dev_id, u32 *boot_mode);
ka_mutex_t *devdrv_get_ctrl_mutex(void);
int devdrv_hdc_suspend_precheck(int count);
void devdrv_peer_fault_notifier(u32 status);
int devdrv_get_pcie_id_info(u32 udevid, struct devdrv_base_device_info *dev_info);
int devdrv_get_runtime_runningplat(u32 udevid, u64 *running_plat);
int devdrv_set_runtime_runningplat(u32 udevid, u64 running_plat);
int devdrv_pcie_hotreset_assemble(u32 index_id);
int devdrv_pcie_prereset(u32 index_id);
int devdrv_pcie_unbind_atomic(u32 index_id);
int devdrv_pcie_reset_atomic(u32 index_id);
int devdrv_pcie_remove_atomic(u32 index_id);
int devdrv_hotreset_atomic_rescan(u32 dev_id);

#ifdef CFG_FEATURE_SRIOV
extern void *hw_dvt_hypervisor_dma_alloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp);
extern void hw_dvt_hypervisor_dma_free_coherent(ka_device_t *dev, size_t size, void *addr, ka_dma_addr_t dma_addr);
extern ka_dma_addr_t hw_dvt_hypervisor_dma_map_single(ka_device_t *dev, void *ptr, size_t size,
                                                      ka_dma_data_direction_t dir);
extern void hw_dvt_hypervisor_dma_unmap_single(ka_device_t *dev, ka_dma_addr_t addr, size_t size,
                                               ka_dma_data_direction_t dir);
extern ka_dma_addr_t hw_dvt_hypervisor_dma_map_page(ka_device_t *dev, ka_page_t *page, size_t offset, size_t size,
                                                    ka_dma_data_direction_t dir);
extern void hw_dvt_hypervisor_dma_unmap_page(ka_device_t *dev, ka_dma_addr_t addr, size_t size,
                                             ka_dma_data_direction_t dir);
#endif
void *devdrv_pci_msg_alloc_non_trans_queue(u32 index_id, struct devdrv_non_trans_msg_chan_info *chan_info);
int devdrv_pci_msg_free_non_trans_queue(void *msg_chan);
int devdrv_pci_get_device_boot_status(u32 index_id, u32 *boot_status);
int devdrv_pci_get_connect_protocol(u32 index_id);
int devdrv_pci_get_host_phy_mach_flag(u32 index_id, u32 *host_flag);
int devdrv_pci_get_env_boot_type(u32 index_id);
int devdrv_pci_get_pfvf_type_by_devid(u32 index_id);
bool devdrv_pci_is_mdev_vm_boot_mode(u32 index_id);
bool devdrv_pci_is_sriov_support(u32 index_id);
int devdrv_pcie_sriov_enable(u32 index_id, u32 boot_mode);
int devdrv_pcie_sriov_disable(u32 index_id, u32 boot_mode);
ka_device_t *hal_kernel_devdrv_get_pci_dev_by_devid(u32 udevid);
int devdrv_pcie_get_dev_topology(u32 index_id, u32 peer_index_id, int *topo_type);
int devdrv_get_pcie_id_info_inner(u32 index_id, struct devdrv_base_device_info *dev_info);

bool devdrv_is_p2p_enabled_inner(u32 index_id, u32 peer_index_id);
int devdrv_enable_p2p_inner(int pid, u32 index_id, u32 peer_index_id, u32 p2p_type);
int devdrv_disable_p2p_inner(int pid, u32 index_id, u32 peer_index_id, u32 p2p_type);
int devdrv_get_p2p_capability_inner(u32 index_id, u64 *capability);
int devdrv_get_p2p_access_status_inner(u32 index_id, u32 peer_index_id, int *status);
void devdrv_set_bar_wc_flag_inner(u32 index_id, u32 value);
int devdrv_get_bar_wc_flag_inner(u32 index_id, u32 *value);
void devdrv_set_host_phy_mach_flag_inner(u32 index_id, u32 host_flag);
int devdrv_get_master_devid_in_the_same_os_inner(u32 index_id, u32 *master_index_id);
int devdrv_dma_sync_copy_inner(u32 index_id, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                               enum devdrv_dma_direction direction);
int devdrv_dma_async_copy_inner(u32 index_id, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                                enum devdrv_dma_direction direction, struct devdrv_asyn_dma_para_info *para_info);
int devdrv_dma_sync_link_copy_inner(u32 index_id, enum devdrv_dma_data_type type, int wait_type,
                                    struct devdrv_dma_node *dma_node, u32 node_cnt);
int devdrv_dma_async_link_copy_inner(u32 index_id, enum devdrv_dma_data_type type, struct devdrv_dma_node *dma_node,
                                     u32 node_cnt, struct devdrv_asyn_dma_para_info *para_info);
int devdrv_dma_sync_copy_plus_inner(u32 index_id, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst,
                                    u32 size, enum devdrv_dma_direction direction);
int devdrv_dma_async_copy_plus_inner(u32 index_id, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst,
                                     u32 size, enum devdrv_dma_direction direction,
                                     struct devdrv_asyn_dma_para_info *para_info);
int devdrv_dma_sync_link_copy_plus_inner(u32 index_id, enum devdrv_dma_data_type type, int wait_type, int instance,
                                         struct devdrv_dma_node *dma_node, u32 node_cnt);
int devdrv_dma_async_link_copy_plus_inner(u32 index_id, enum devdrv_dma_data_type type, int instance,
                                          struct devdrv_dma_node *dma_node, u32 node_cnt,
                                          struct devdrv_asyn_dma_para_info *para_info);
int devdrv_dma_sync_link_copy_extend_inner(u32 index_id, enum devdrv_dma_data_type type, int wait_type,
                                           struct devdrv_dma_node *dma_node, u32 node_cnt);
int devdrv_dma_sync_link_copy_plus_extend_inner(u32 index_id, enum devdrv_dma_data_type type, int wait_type,
                                                int instance, struct devdrv_dma_node *dma_node, u32 node_cnt);
int devdrv_dma_done_schedule_inner(u32 index_id, enum devdrv_dma_data_type type, int instance);

int devdrv_get_pfvf_id_by_devid_inner(u32 index_id, u32 *pf_index_id, u32 *vf_index_id);
int devdrv_get_theoretical_capability_inner(u32 index_id, u64 *bandwidth, u64 *packspeed);
int devdrv_get_real_capability_ratio_inner(u32 index_id, u32 *bandwidth_ratio, u32 *packspeed_ratio);
int devdrv_force_linkdown_inner(u32 index_id);
void devdrv_hb_broken_stop_msg_send_inner(u32 index_id);
int devdrv_dma_fill_desc_of_sq_inner(u32 index_id, struct devdrv_dma_prepare *dma_prepare,
                                     struct devdrv_dma_node *dma_node, u32 node_cnt, u32 fill_status);
int devdrv_dma_fill_desc_of_sq_ext_inner(u32 index_id, void *sq_base, struct devdrv_dma_node *dma_node, u32 node_cnt,
                                         u32 fill_status);
int devdrv_dma_link_free_inner(struct devdrv_dma_prepare *dma_prepare);
int devdrv_dma_prepare_alloc_sq_addr_inner(u32 index_id, u32 node_cnt, struct devdrv_dma_prepare *dma_prepare);
void devdrv_dma_prepare_free_sq_addr_inner(u32 index_id, struct devdrv_dma_prepare *dma_prepare);
void *devdrv_pcimsg_alloc_trans_queue_inner(u32 index_id, struct devdrv_trans_msg_chan_info *chan_info);
int devdrv_get_support_msg_chan_cnt_inner(u32 index_id, enum devdrv_msg_client_type module_type);
struct devdrv_dma_prepare *devdrv_dma_link_prepare_inner(u32 index_id, enum devdrv_dma_data_type type,
                                                         struct devdrv_dma_node *dma_node, u32 node_cnt,
                                                         u32 fill_status);
u32 devdrv_get_dev_chip_type_inner(u32 index_id);
int devdrv_get_heartbeat_count_inner(u32 index_id, u64 *count);
int devdrv_set_heartbeat_count_inner(u32 index_id, u64 count);
int devdrv_get_device_index_inner(u32 host_dev_id);
int devdrv_devmem_addr_h2d_inner(u32 index_id, phys_addr_t host_bar_addr, phys_addr_t *device_phy_addr);
int devdrv_devmem_addr_d2h_inner(u32 index_id, phys_addr_t device_phy_addr, phys_addr_t *host_bar_addr);
ka_device_t *devdrv_get_pci_dev_by_devid_inner(u32 index_id);
int devdrv_register_irq_by_vector_index_inner(u32 index_id, int vector_index,
                                              ka_irqreturn_t (*callback_func)(int, void *), void *para,
                                              const char *name);
int devdrv_get_addr_info_inner(u32 index_id, enum devdrv_addr_type type, u32 index, u64 *addr, size_t *size);
int devdrv_pcie_reinit_inner(u32 index_id);
int devdrv_unregister_irq_by_vector_index_inner(u32 index_id, int vector_index, void *para);
int devdrv_get_bbox_reservd_mem_inner(unsigned int index_id, unsigned long long *dma_addr, ka_page_t **dma_pages,
                                      unsigned int *len);
bool devdrv_is_mdev_pm_boot_mode_inner(u32 index_id);
ka_pci_dev_t *devdrv_get_pci_pdev_by_devid_inner(u32 index_id);
int devdrv_get_devid_by_pfvf_id_inner(u32 pf_index_id, u32 vf_index_id, u32 *index_id);
bool devdrv_check_half_probe_finish_inner(u32 index_id);

/* alloc trans msg chan */
void *devdrv_pci_pcimsg_alloc_trans_queue(u32 udevid, struct devdrv_trans_msg_chan_info *chan_info);
int devdrv_pci_pcimsg_realease_trans_queue(void *msg_chan);
int devdrv_peh_register_irq_by_vector_index(struct devdrv_pci_ctrl *pci_ctrl);
#endif
