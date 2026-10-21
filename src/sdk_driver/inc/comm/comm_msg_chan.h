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

#ifndef __COMM_MSG_CHAN_H__
#define __COMM_MSG_CHAN_H__

#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "comm_msg_chan_cmd.h"
#include "pbl/pbl_soc_res_attr.h"
#include "addr_trans.h"
#include "pair_dev_info.h"

/* ******************** non trans for host and device ******************** */
/* **************************** host ************************************* */

/* host msg client type */
enum devdrv_msg_client_type {
    devdrv_msg_client_pcivnic = 0,
    devdrv_msg_client_pasid,
    devdrv_msg_client_devmm,
    devdrv_msg_client_common,
    devdrv_msg_client_devmanager,
    devdrv_msg_client_tsdrv,
    devdrv_msg_client_hdc,
    devdrv_msg_client_queue,
    devdrv_msg_client_s2s,
    devdrv_msg_client_tablesync,
    devdrv_msg_client_esched,
    devdrv_msg_client_max
};

/* host non-trans chan info */
struct devdrv_non_trans_msg_chan_info {
    enum devdrv_msg_client_type msg_type;
    u32 flag; /* sync or async:bit0 */
    u32 level;
    u32 s_desc_size;
    u32 c_desc_size;
    int (*rx_msg_process)(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
};

/* alloc non-trans msg chan */
void *devdrv_pcimsg_alloc_non_trans_queue(u32 dev_id, struct devdrv_non_trans_msg_chan_info *chan_info);
void *devdrv_pcimsg_alloc_non_trans_queue_inner_msg(u32 index_id, struct devdrv_non_trans_msg_chan_info *chan_info);
/* free non-trans msg chan */
int devdrv_pcimsg_free_non_trans_queue(void *msg_chan);
/* non-trans msg sync send */
int devdrv_sync_msg_send(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);

/* **************************** host **************************** */
/* device msg client type */
enum agentdrv_msg_client_type {
    agentdrv_msg_client_pcivnic = 0,
    agentdrv_msg_client_pasid,
    agentdrv_msg_client_devmm,
    agentdrv_msg_client_common,
    agentdrv_msg_client_devmanager,
    agentdrv_msg_client_tsdrv,
    agentdrv_msg_client_hdc,
    agentdrv_msg_client_queue,
    agentdrv_msg_client_s2s,
    agentdrv_msg_client_tablesync,
    agentdrv_msg_client_esched,
    agentdrv_msg_client_max
};

/* device non-trans msg client */
struct agentdrv_non_trans_msg_client {
    enum agentdrv_msg_client_type type;
    u32 flag;
    int (*init_non_trans_msg_chan)(void *msg_chan);
    void (*uninit_non_trans_msg_chan)(void *msg_chan);
    int (*non_trans_msg_process)(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
    /* irq top half, can not sleep or time-consuming operation */
    int (*non_trans_msg_irq_notify)(void *msg_chan, void *data, u32 in_data_len);
};

/* register non-trans msg client */
int agentdrv_register_non_trans_msg_client(const struct agentdrv_non_trans_msg_client *msg_client);

/* unregister non-trans msg client */
int agentdrv_unregister_non_trans_msg_client(const struct agentdrv_non_trans_msg_client *msg_client);

/* non-trans msg sync send */
int agentdrv_sync_msg_send(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);

/* ********************* common for host and device ********************** */
/* *************************** device ************************************ */
struct agentdrv_common_msg_client {
    enum devdrv_common_msg_type type;
    int (*common_msg_recv)(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
    void (*init_notify)(u32 devid);
};

int agentdrv_register_common_msg_client(const struct agentdrv_common_msg_client *msg_client);
int agentdrv_unregister_common_msg_client(const struct agentdrv_common_msg_client *msg_client);
int agentdrv_common_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len,
                             enum devdrv_common_msg_type msg_type);

/* *************************** device ************************************ */
struct devdrv_common_msg_client {
    enum devdrv_common_msg_type type;
    int (*common_msg_recv)(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
    void (*init_notify)(u32 dev_id, int status);
};

int devdrv_register_common_msg_client(const struct devdrv_common_msg_client *msg_client);
int devdrv_unregister_common_msg_client(u32 devid, const struct devdrv_common_msg_client *msg_client);
int devdrv_common_msg_send(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len,
                           enum devdrv_common_msg_type msg_type);

/* ********************* dev_manager api ********************* */
struct devdrv_base_device_info {
    unsigned int venderid;
    unsigned int subvenderid;
    unsigned int deviceid;
    unsigned int subdeviceid;
    unsigned int bus;
    unsigned int device;
    unsigned int fn;
    int domain;
    unsigned int reserve[32];
};

enum devdrv_base_p2p_attr_op {
    DEVDRV_BASE_P2P_ADD = 0,
    DEVDRV_BASE_P2P_DEL,
    DEVDRV_BASE_P2P_QUERY,
    DEVDRV_BASE_P2P_ACCESS_STATUS_QUERY,
    DEVDRV_BASE_P2P_CAPABILITY_QUERY,
    DEVDRV_BASE_P2P_MAX
};
struct devdrv_base_comm_p2p_attr {
    u32 op;

    int pid;
    u32 devid;
    u32 peer_dev_id;
    u32 type;

    u64 *capability;
    int *status;
};

/* device boot status */
enum dsmi_boot_status {
    DSMI_BOOT_STATUS_UNINIT = 0, /* device uninit */
    DSMI_BOOT_STATUS_BIOS,       /* device BIOS starting */
    DSMI_BOOT_STATUS_OS,         /* device OS starting */
    DSMI_BOOT_STATUS_FINISH      /* device boot finish */
};

int devdrv_get_device_boot_status(u32 devid, u32 *boot_status);
int devdrv_get_device_boot_status_inner(u32 index_id, u32 *boot_status);

#define COMM_STATUS_STRUCT_RSV 4
enum communication_status {
    COMM_STATUS_OK = 0,
    COMM_STATUS_LINK_ERR,
    COMM_STATUS_MSG_ERR,
};
struct devdrv_comm_status_info {
    u32 status;
    u32 lane_num;
    u32 rate_mode;
    u32 rsv[COMM_STATUS_STRUCT_RSV];
};
int devdrv_get_com_status_inner(u32 devid, struct devdrv_comm_status_info *status);

enum devdrv_dma_copy_support_mode {
    DEVDRV_DMA_COPY_SUPPORT_MODE_PHY = 0,
    DEVDRV_DMA_COPY_SUPPORT_MODE_DMA,
    DEVDRV_DMA_COPY_SUPPORT_MODE_SVA,
    DEVDRV_DMA_COPY_SUPPORT_MODE_MAX
};
int devdrv_get_dma_copy_support_mode(u32 udevid, u32 *support_mode);

#define CONNECT_PROTOCOL_PCIE 0
#define CONNECT_PROTOCOL_HCCS 1
#define CONNECT_PROTOCOL_UB 2
#define CONNECT_PROTOCOL_UNKNOWN 3
int devdrv_get_connect_protocol(u32 dev_id);
int devdrv_get_global_connect_protocol(void);
int agentdrv_get_msg_chan_devid(void *msg_chan);
int devdrv_get_msg_chan_devid(void *msg_chan);
int devdrv_get_msg_chan_devid_inner(void *msg_chan);
int agentdrv_set_msg_chan_priv(void *msg_chan, void *priv);
void *agentdrv_get_msg_chan_priv(void *msg_chan);
int devdrv_set_msg_chan_priv(void *msg_chan, void *priv);
void *devdrv_get_msg_chan_priv(void *msg_chan);

#define DEVDRV_VIRT_MACH_SIGN 0x503250
#define DEVDRV_VIRT_PASS_THROUGH_MACH_FLAG 0x0     /* virtual machine passthrough flag */
#define DEVDRV_HOST_PHY_MACH_FLAG 0x5a6b7c8d       /* host physical mathine flag */
#define DEVDRV_HOST_VM_MACH_FLAG 0x1a2b3c4d        /* vm mathine flag */
#define DEVDRV_HOST_CONTAINER_MACH_FLAG 0xa4b3c2d1 /* container mathine flag */
int devdrv_get_host_phy_mach_flag(u32 devid, u32 *host_flag);
int devdrv_get_env_boot_type(u32 dev_id);
int devdrv_get_env_boot_type_inner(u32 index_id);
int devdrv_get_device_info(u32 devid, struct devdrv_base_device_info *device_info);
int soc_get_dev_topology(unsigned int dev_id1, unsigned int dev_id2, int *topology_type);
int devdrv_p2p_attr_op(struct devdrv_base_comm_p2p_attr *attr);
int devdrv_get_pfvf_type_by_devid_inner(u32 index_id);
int devdrv_get_pfvf_type_by_devid(u32 dev_id);
int devdrv_get_pm_devid_by_vm_devid(u32 udev_id, u32 *pm_devid);
bool devdrv_is_mdev_vm_boot_mode(u32 dev_id);
bool devdrv_is_mdev_vm_boot_mode_inner(u32 index_id);
int devdrv_get_connect_protocol_inner(u32 index_id);
bool devdrv_is_sriov_support(u32 dev_id);

int devdrv_sriov_enable(u32 dev_id, u32 boot_mode);
int devdrv_sriov_disable(u32 dev_id, u32 boot_mode);
int devdrv_get_ub_urma_info_by_udevid(u32 udevid, struct ascend_urma_dev_info *urma_info);
int devdrv_get_dev_topology(u32 devid, u32 peer_devid, int *topo_type);
ka_device_t *devdrv_base_comm_get_device(u32 devid, u32 vfid, u32 udevid);

/* ********************* remote address operate(RAO) api ********************** */
enum devdrv_rao_client_type {
    DEVDRV_RAO_CLIENT_DEVMNG = 0,
    DEVDRV_RAO_CLIENT_BBOX_DDR,
    DEVDRV_RAO_CLIENT_BBOX_SRAM,
    DEVDRV_RAO_CLIENT_BBOX_KLOG,
    DEVDRV_RAO_CLIENT_BBOX_DEBUG_RUN_OS,
    DEVDRV_RAO_CLIENT_BBOX_SEC_OS,
    DEVDRV_RAO_CLIENT_BBOX_DEBUG_DEV,
    DEVDRV_RAO_CLIENT_TEST,
    DEVDRV_RAO_CLIENT_MAX
};

enum devdrv_rao_permission_type {
    DEVDRV_RAO_PERM_RMT_READ = 0,
    DEVDRV_RAO_PERM_RMT_WRITE,
    DEVDRV_RAO_PERM_RMT_RW,
    DEVDRV_RAO_PERM_MAX
};

int devdrv_register_rao_client(u32 dev_id, enum devdrv_rao_client_type type, u64 va, u64 len,
                               enum devdrv_rao_permission_type perm);
int devdrv_unregister_rao_client(u32 dev_id, enum devdrv_rao_client_type type);
int devdrv_rao_read(u32 dev_id, enum devdrv_rao_client_type type, u64 offset, u64 len);
int devdrv_rao_write(u32 dev_id, enum devdrv_rao_client_type type, u64 offset, u64 len);

/* ********************* register communication api ********************** */
enum devdrv_communication_type {
    DEVDRV_COMMNS_PCIE = 0,
    DEVDRV_COMMNS_UB,
    DEVDRV_COMMNS_XLINK,
    DEVDRV_COMMNS_TYPE_MAX
};

enum devdrv_ops_status {
    DEVDRV_COMM_OPS_TYPE_UNINIT,
    DEVDRV_COMM_OPS_TYPE_INIT,
    DEVDRV_COMM_OPS_TYPE_ENABLE,
    DEVDRV_COMM_OPS_TYPE_DISABLE,
    DEVDRV_COMM_OPS_TYPE_MAX
};

#define MAX_EID_NUM_PER_DEV 8
struct devdrv_ub_dev_info {
    u32 eid_index[MAX_EID_NUM_PER_DEV];
    void *udma_dev[MAX_EID_NUM_PER_DEV];
};

/* ********************* URMA_COPY API ********************** */
enum devdrv_urma_copy_dir {
    LOCAL_TO_PEER = 0,
    PEER_TO_LOCAL,
    DIR_MAX
};

enum devdrv_urma_chan_type {
    URMA_CHAN_TSDRV = 0,
    URMA_CHAN_COMMON,
    URMA_CHAN_BBOX,
    URMA_CHAN_MAX
};

struct devdrv_urma_copy {
    u64 len;
    u64 offset;
    void *seg; // struct ubcore_target_seg*
};

struct devdrv_seg_info {
    u32 token_value;
    u32 access;
    u32 mem_len;
    u64 va;
};
#define DEVDRV_ACCESS_LOCAL_ONLY 0x1
#define DEVDRV_ACCESS_READ (0x1 << 1)
#define DEVDRV_ACCESS_WRITE (0x1 << 2)
#define DEVDRV_ACCESS_ATOMIC (0x1 << 3)

/* UB urma copy */
int devdrv_urma_copy(u32 dev_id, enum devdrv_urma_chan_type type, enum devdrv_urma_copy_dir dir,
                     struct devdrv_urma_copy *local, struct devdrv_urma_copy *peer);

/* register seg */
int devdrv_register_seg(u32 dev_id, struct devdrv_seg_info *info, void **tseg, size_t *out_len);
/* unregister seg */
int devdrv_unregister_seg(u32 dev_id, void *tseg, size_t in_len);
/* import seg */
void *devdrv_import_seg(u32 dev_id, u32 peer_token, void *peer_seg, size_t in_len, size_t *out_len);
/* unimport seg */
int devdrv_unimport_seg(u32 dev_id, void *peer_tseg, size_t in_len);

int devdrv_send_vdm_msg(u32 dev_id);
/* **************************** DMA_COPY API ******************************* */
#define DEVDRV_DMA_DESC_FILL_CONTINUE 0
#define DEVDRV_DMA_DESC_FILL_FINISH 1

#define DEVDRV_DMA_PASSID_DEFAULT 0

#define DEVDRV_DMA_WAIT_INTR 1
#define DEVDRV_DMA_WAIT_QUREY 2

#define DEVDRV_REMOTE_IRQ_FLAG 0x1
#define DEVDRV_LOCAL_IRQ_FLAG 0x2
#define DEVDRV_LOCAL_REMOTE_IRQ_FLAG 0x3
#define DEVDRV_ATTR_FLAG 0x4
#define DEVDRV_WD_BARRIER_FLAG 0x8
#define DEVDRV_RD_BARRIER_FLAG 0x10

/*
 * asynchronous dma parameter structure
 * interrupt_and_attr_flag: bit0 remote interrupt flag
 *                          bit1 local interrupt flag
 *                          bit2 attr of sq BD flag
 *                          bit3 wd barrier flag
 *                          bit4 rd barrier flag
 * remote_msi_vector : remote msi interrupt num
 */
struct devdrv_asyn_dma_para_info {
    u32 interrupt_and_attr_flag;
    u32 remote_msi_vector;
    void *priv;
    u32 trans_id;
    void (*finish_notify)(void *, u32, u32);
};

/* the direction of dma */
enum devdrv_dma_direction {
    DEVDRV_DMA_HOST_TO_DEVICE = 0x0,
    DEVDRV_DMA_DEVICE_TO_HOST = 0x1,
    DEVDRV_DMA_LOCAL_TO_LOCAL = 0x2,
    DEVDRV_DMA_SYS_TO_SYS = DEVDRV_DMA_LOCAL_TO_LOCAL
};

/* DMA copy type, cloud has 30 chan pcie use 22, mini has 16 pcie use 12 */
enum devdrv_dma_data_type {
    DEVDRV_DMA_DATA_COMMON = 0, /* used for IDE,vnic,file transfer,hdc low level service, have 1 dma channel */
    DEVDRV_DMA_DATA_PCIE_MSG,   /* used for non trans msg, admin msg, p2p msg, have 1 dma channel */
    DEVDRV_DMA_DATA_TRAFFIC,    /* used for devmm(online), hdc(offline), have the remaining part of dma channel */
    DEVDRV_DMA_DATA_MANAGE,     /* used for scene hdc manage service type(64~95) */
    DEVDRV_DMA_DATA_TYPE_MAX
};

/* DMA link copy */
struct devdrv_dma_node {
    u64 src_addr;
    u64 dst_addr;
    u32 size;
    enum devdrv_dma_direction direction;
    u32 loc_passid;
};

/* sync DMA copy */
int hal_kernel_devdrv_dma_sync_copy(u32 udevid, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                                    enum devdrv_dma_direction direction);
/* async DMA copy */
int hal_kernel_devdrv_dma_async_copy(u32 udevid, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                                     enum devdrv_dma_direction direction, struct devdrv_asyn_dma_para_info *para_info);
/* sync DMA link copy */
int hal_kernel_devdrv_dma_sync_link_copy(u32 udevid, enum devdrv_dma_data_type type, int wait_type,
                                         struct devdrv_dma_node *dma_node, u32 node_cnt);
/* async DMA link copy */
int hal_kernel_devdrv_dma_async_link_copy(u32 udevid, enum devdrv_dma_data_type type, struct devdrv_dma_node *dma_node,
                                          u32 node_cnt, struct devdrv_asyn_dma_para_info *para_info);

/* sync DMA copy assign dma chan */
int hal_kernel_devdrv_dma_sync_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst,
                                         u32 size, enum devdrv_dma_direction direction);
/* async DMA copy assign dma chan */
int hal_kernel_devdrv_dma_async_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst,
                                          u32 size, enum devdrv_dma_direction direction,
                                          struct devdrv_asyn_dma_para_info *para_info);
/* sync DMA link copy assign dma chan */
int hal_kernel_devdrv_dma_sync_link_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int wait_type, int instance,
                                              struct devdrv_dma_node *dma_node, u32 node_cnt);
/* async DMA link copy assign dma chan */
int hal_kernel_devdrv_dma_async_link_copy_plus(u32 udevid, enum devdrv_dma_data_type type, int instance,
                                               struct devdrv_dma_node *dma_node, u32 node_cnt,
                                               struct devdrv_asyn_dma_para_info *para_info);

/* sync DMA link copy, pa copy */
int hal_kernel_devdrv_dma_sync_link_copy_extend(u32 udevid, enum devdrv_dma_data_type type, int wait_type,
                                                struct devdrv_dma_node *dma_node, u32 node_cnt);
/* sync DMA link copy assign dma chan, pa copy */
int hal_kernel_devdrv_dma_sync_link_copy_plus_extend(u32 udevid, enum devdrv_dma_data_type type, int wait_type,
                                                     int instance, struct devdrv_dma_node *dma_node, u32 node_cnt);

int devdrv_dma_done_schedule(u32 udevid, enum devdrv_dma_data_type type, int instance);

struct devdrv_dma_prepare {
    u32 devid;
    u64 sq_size;
    u64 cq_size;
    ka_dma_addr_t sq_dma_addr;
    ka_dma_addr_t cq_dma_addr;
    void *sq_base;
    void *cq_base;
};

struct devdrv_dma_desc_info {
    ka_dma_addr_t sq_dma_addr;
    u64 sq_size;
    ka_dma_addr_t cq_dma_addr;
    u64 cq_size;
};

void *hal_kernel_devdrv_dma_alloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp);
void *hal_kernel_devdrv_dma_zalloc_coherent(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp);
void hal_kernel_devdrv_dma_free_coherent(ka_device_t *dev, size_t size, void *addr, ka_dma_addr_t dma_addr);
ka_dma_addr_t hal_kernel_devdrv_dma_map_single(ka_device_t *dev, void *ptr, size_t size, ka_dma_data_direction_t dir);
void hal_kernel_devdrv_dma_unmap_single(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir);
ka_dma_addr_t hal_kernel_devdrv_dma_map_page(ka_device_t *dev, ka_page_t *page, size_t offset, size_t size,
                                             ka_dma_data_direction_t dir);
void hal_kernel_devdrv_dma_unmap_page(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir);
ka_dma_addr_t hal_kernel_devdrv_dma_map_page_attrs(ka_device_t *dev, ka_page_t *page, size_t offset, size_t size,
                                                   ka_dma_data_direction_t dir, u64 attrs);
void hal_kernel_devdrv_dma_unmap_page_attrs(ka_device_t *dev, ka_dma_addr_t addr, size_t size,
                                            ka_dma_data_direction_t dir, u64 attrs);
ka_dma_addr_t devdrv_dma_map_resource(ka_device_t *dev, phys_addr_t phys_addr, size_t size, ka_dma_data_direction_t dir,
                                      unsigned long attrs);
void devdrv_dma_unmap_resource(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir,
                               unsigned long attrs);

int devdrv_dma_get_sq_cq_desc_size(u32 devid, u32 *sq_desc_size, u32 *cq_desc_size);
int devdrv_dma_fill_desc_of_sq(u32 udevid, struct devdrv_dma_prepare *dma_prepare, struct devdrv_dma_node *dma_node,
                               u32 node_cnt, u32 fill_status);

int devdrv_dma_fill_desc_of_sq_ext(u32 udevid, void *sq_base, struct devdrv_dma_node *dma_node, u32 node_cnt,
                                   u32 fill_status);
/* async DAM link prepare */
struct devdrv_dma_prepare *devdrv_dma_link_prepare(u32 udevid, enum devdrv_dma_data_type type,
                                                   struct devdrv_dma_node *dma_node, u32 node_cnt, u32 fill_status);
/* async DAM link free */
int devdrv_dma_link_free(struct devdrv_dma_prepare *dma_prepare);
int devdrv_dma_sqcq_desc_check(u32 devid, struct devdrv_dma_desc_info *dma_desc_info);
int devdrv_dma_prepare_alloc_sq_addr(u32 udevid, u32 node_cnt, struct devdrv_dma_prepare *dma_prepare);
void devdrv_dma_prepare_free_sq_addr(u32 udevid, struct devdrv_dma_prepare *dma_prepare);
int hal_kernel_devdrv_dma_map_sg_cache(ka_scatterlist_t *sg, int nents, dma_addr_t *dma_handle,
                                       enum dma_data_direction dir);
void hal_kernel_devdrv_dma_unmap_sg_cache(ka_scatterlist_t *sg, int nents, dma_addr_t dma_handle,
                                          enum dma_data_direction dir);
int hal_kernel_devdrv_dma_map_sg_no_cache(ka_scatterlist_t *sg, int nents, dma_addr_t *dma_handle,
                                          enum dma_data_direction dir);
void hal_kernel_devdrv_dma_unmap_sg_no_cache(ka_scatterlist_t *sg, int nents, dma_addr_t dma_handle,
                                             enum dma_data_direction dir);
/* **************************** DMA_COPY API END ******************************* */

/* device to device msg */
enum agentdrv_p2p_msg_type {
    AGENTDRV_P2P_MSG_DEVMM = 0,
    AGENTDRV_P2P_MSG_TYPE_MAX
};

/* devid: device devid, in_len <=28, out_len <=1k */
typedef int (*p2p_msg_recv)(u32 devid, void *data, u32 data_len, u32 in_len, u32 *out_len);
int agentdrv_register_p2p_msg_proc_func(enum agentdrv_p2p_msg_type msg_type, p2p_msg_recv func);

/* local_devid:device devid, devid: host devid */
int agentdrv_p2p_msg_send(u32 local_devid, u32 dst_host_udevid, enum agentdrv_p2p_msg_type msg_type, void *data,
                          u32 data_len, u32 in_len, u32 *out_len);

/* devid: device devid, online_devid: host online devid, status: DEVDRV_DEV_* */
typedef int (*devdrv_notify_func)(u32 devid, u32 notify_type, u32 online_devid, u32 status);
int agentdrv_register_dev_online_proc_func(devdrv_notify_func func);

struct data_recv_info {
    void *data;
    u32 data_len;
    u32 out_len;
    u32 flag;
};

enum devdrv_s2s_msg_type {
    DEVDRV_S2S_MSG_DEVMM = 0,
    DEVDRV_S2S_MSG_TRSDRV = 1,
    DEVDRV_S2S_MSG_TEST = 2,
    DEVDRV_S2S_MSG_TYPE_MAX
};

/* device to device msg */
enum agentdrv_s2s_msg_type {
    AGENTDRV_S2S_MSG_DEVMM = 0,
    AGENTDRV_S2S_MSG_TRSDRV = 1,
    AGENTDRV_S2S_MSG_USER_TEST = 2,
    AGENTDRV_S2S_MSG_PCIVNIC = 3,
    AGENTDRV_S2S_MSG_TYPE_MAX
};

struct data_input_info {
    void *data;
    u32 data_len;
    u32 in_len;
    u32 out_len;
    u32 msg_mode;
};

/* devid: device devid, in_len <=28, out_len <=1k */
typedef int (*s2s_msg_recv)(u32 local_devid, u32 sdid, struct data_input_info *data_info);
int agentdrv_register_s2s_msg_proc_func(enum agentdrv_s2s_msg_type msg_type, s2s_msg_recv func);
int agentdrv_unregister_s2s_msg_proc_func(enum agentdrv_s2s_msg_type msg_type);

/* local_devid:recv_side devid, sdid: send_side sdid */
int agentdrv_s2s_msg_send(u32 local_devid, u32 sdid, enum agentdrv_s2s_msg_type msg_type, u32 direction,
                          struct data_input_info *data_info);
int agentdrv_s2s_async_msg_recv(u32 local_devid, u32 sdid, enum agentdrv_s2s_msg_type msg_type,
                                struct data_recv_info *data_info);
int devdrv_register_s2s_msg_proc_func(enum devdrv_s2s_msg_type msg_type, s2s_msg_recv func);
int devdrv_unregister_s2s_msg_proc_func(enum devdrv_s2s_msg_type msg_type);
int devdrv_s2s_msg_send(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type, u32 direction,
                        struct data_input_info *data_info);
int devdrv_s2s_npu_link_check(u32 dev_id, u32 sdid);
int devdrv_s2s_async_msg_recv(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type, struct data_recv_info *data_info);

#define DEVDRV_ASYNC_P2P_PARA_RSV 4
struct devdrv_async_p2p_para_info {
    void *priv;
    void *out_buf;
    u32 buf_len;
    u32 out_len;
    u32 rsv[DEVDRV_ASYNC_P2P_PARA_RSV];
};
typedef int (*devdrv_p2p_msg_recv)(u32 local_devid, u32 sdid, struct data_input_info *data_info);
typedef int (*devdrv_p2p_async_cb)(u32 devid, struct devdrv_async_p2p_para_info *param, int status);
#define DEVDRV_P2P_MSG_SYNC 0U
#define DEVDRV_P2P_MSG_ASYNC 1U
int xcom_register_p2p_msg_proc_func(enum devdrv_msg_client_type msg_type, devdrv_p2p_msg_recv func);
int xcom_unregister_p2p_msg_proc_func(enum devdrv_msg_client_type msg_type);
int xcom_p2p_msg_send(u32 local_devid, u32 sdid, enum devdrv_msg_client_type msg_type,
                      struct data_input_info *data_info);
int devdrv_register_p2p_msg_proc_func(enum devdrv_msg_client_type msg_type, u32 msg_mode, devdrv_p2p_msg_recv func);
int devdrv_unregister_p2p_msg_proc_func(enum devdrv_msg_client_type msg_type, u32 msg_mode);
int devdrv_p2p_msg_send(u32 devid, enum devdrv_msg_client_type msg_type, struct data_input_info *data_info,
                        struct devdrv_async_p2p_para_info *param, devdrv_p2p_async_cb func);

struct devdrv_trans_msg_chan_info;
struct devdrv_comm_ops {
    ka_atomic_t ref_cnt;
    enum devdrv_communication_type comm_type;
#ifdef CFG_ENV_HOST
    void *(*alloc_non_trans)(u32 dev_id, struct devdrv_non_trans_msg_chan_info *chan_info);
    int (*free_non_trans)(void *msg_chan);
    int (*register_common_msg_client)(const struct devdrv_common_msg_client *msg_client);
    int (*unregister_common_msg_client)(u32 devid, const struct devdrv_common_msg_client *msg_client);
    int (*common_msg_send)(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len,
                           enum devdrv_common_msg_type msg_type);
    int (*get_boot_status)(u32 dev_id, u32 *boot_status);
    int (*get_com_status)(u32 devid, struct devdrv_comm_status_info *status);
    int (*get_host_phy_mach_flag)(u32 devid, u32 *host_flag);
    int (*get_env_boot_type)(u32 dev_id);
    ka_device_t *(*get_device)(u32 dev_id, u32 vfid, u32 udevid);
    int (*get_dev_topology)(u32 devid, u32 peer_devid, int *topo_type);
    int (*p2p_attr_op)(struct devdrv_base_comm_p2p_attr *attr);
    int (*get_device_info)(u32 devid, struct devdrv_base_device_info *device_info);
    int (*hotreset_assemble)(u32 dev_id);
    int (*prereset_assemble)(u32 dev_id);
    int (*rescan_atomic)(u32 dev_id);
    int (*unbind_atomic)(u32 dev_id);
    int (*reset_atomic)(u32 dev_id);
    int (*remove_atomic)(u32 dev_id);
    int (*rao_read)(u32 dev_id, enum devdrv_rao_client_type type, u64 offset, u64 len);
    int (*rao_write)(u32 dev_id, enum devdrv_rao_client_type type, u64 offset, u64 len);
    int (*register_s2s_msg)(enum devdrv_s2s_msg_type msg_type, s2s_msg_recv func);
    int (*unregister_s2s_msg)(enum devdrv_s2s_msg_type msg_type);
    int (*send_s2s_msg)(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type, u32 direction,
                        struct data_input_info *data_info);
    int (*recv_s2s_async_msg)(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type, struct data_recv_info *data_info);
    void *(*alloc_trans_queue)(u32 udevid, struct devdrv_trans_msg_chan_info *chan_info);
    int (*realease_trans_queue)(void *msg_chan);
    void (*ring_doorbell)(void *msg_chan);
    void (*ring_cq_doorbell)(void *msg_chan);
#else
    int (*register_non_trans_client)(const struct agentdrv_non_trans_msg_client *msg_client);
    int (*unregister_non_trans_client)(const struct agentdrv_non_trans_msg_client *msg_client);
    int (*register_common_msg_client)(const struct agentdrv_common_msg_client *msg_client);
    int (*unregister_common_msg_client)(const struct agentdrv_common_msg_client *msg_client);
    int (*common_msg_send)(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len,
                           enum devdrv_common_msg_type msg_type);
    int (*get_remote_rx_msg_notify_irq)(void *msg_chan);
    int (*get_remote_tx_finish_notify_irq)(void *msg_chan);
    ka_dma_addr_t (*get_msg_chan_local_sq_tail_dma_addr)(void *msg_chan);
    ka_dma_addr_t (*get_msg_chan_host_sq_tail_dma_addr)(void *msg_chan);
    ka_dma_addr_t (*get_msg_chan_local_cq_tail_dma_addr)(void *msg_chan);
    ka_dma_addr_t (*get_msg_chan_host_cq_tail_dma_addr)(void *msg_chan);
    int (*send_p2p_msg)(u32 local_devid, u32 dst_host_udevid, enum agentdrv_p2p_msg_type type, void *data, u32 data_len,
                        u32 in_len, u32 *out_len);
    int (*register_p2p_msg)(enum agentdrv_p2p_msg_type msg_type, p2p_msg_recv func);
    int (*register_dev_online)(devdrv_notify_func func);
    int (*register_s2s_msg)(enum agentdrv_s2s_msg_type msg_type, s2s_msg_recv func);
    int (*unregister_s2s_msg)(enum agentdrv_s2s_msg_type msg_type);
    int (*send_s2s_msg)(u32 local_devid, u32 sdid, enum agentdrv_s2s_msg_type msg_type, u32 direction,
                        struct data_input_info *data_info);
    int (*recv_s2s_async_msg)(u32 local_devid, u32 sdid, enum agentdrv_s2s_msg_type msg_type,
                              struct data_recv_info *data_info);
#endif
    int (*send_vdm_msg)(u32 dev_id);
    int (*sync_msg_send)(void *msg_chan, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len);
    int (*register_devdrv_p2p_msg)(enum devdrv_msg_client_type msg_type, u32 msg_mode, devdrv_p2p_msg_recv func);
    int (*unregister_devdrv_p2p_msg)(enum devdrv_msg_client_type msg_type, u32 msg_mode);
    int (*devdrv_p2p_msg_send)(u32 devid, enum devdrv_msg_client_type msg_type, struct data_input_info *data_input,
                               struct devdrv_async_p2p_para_info *param, devdrv_p2p_async_cb func);
    int (*set_msg_chan_priv)(void *msg_chan, void *priv);
    void *(*get_msg_chan_priv)(void *msg_chan);
    int (*get_msg_chan_devid)(void *msg_chan);
    int (*get_connect_type)(u32 dev_id);
    int (*get_pfvf_type_by_devid)(u32 dev_id);
    int (*get_pm_devid_by_vm_devid)(u32 index_id, u32 *pm_index_id);
    bool (*mdev_vm_boot_mode)(u32 dev_id);
    bool (*sriov_support)(u32 dev_id);
    int (*sriov_enable)(u32 dev_id, u32 boot_mode);  /* not must */
    int (*sriov_disable)(u32 dev_id, u32 boot_mode); /* not must */
    int (*register_rao_client)(u32 dev_id, enum devdrv_rao_client_type type, u64 va, u64 len,
                               enum devdrv_rao_permission_type perm);
    int (*unregister_rao_client)(u32 dev_id, enum devdrv_rao_client_type type);
    int (*get_all_device_count)(u32 *count);
    int (*get_device_probe_list)(u32 *devids, u32 *count);
    int (*get_urma_info_by_eid)(u32 udevid, struct ascend_urma_dev_info *urma_info);
    int (*get_ub_dev_info)(u32 dev_id, struct devdrv_ub_dev_info *eid_info, int *num);
    int (*addr_trans_p2p)(u32 udevid, u32 peer_udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr);
    int (*get_token_val)(u32 dev_id, u32 *token_val);
    int (*add_pasid)(u32 dev_id, u64 pasid);
    int (*del_pasid)(u32 dev_id, u64 pasid);
    int (*get_d2d_eid)(u32 local_udevid, u32 d2d_eid_type, struct devdrv_pair_info_eid *eid, u32 *eid_index);
    int (*get_bus_instance_eid)(u32 udevid, struct devdrv_pair_info_eid *eid);
    int (*get_ub_dev_id_info)(u32 dev_id, struct devdrv_dev_id_info *id_info);
    int (*addr_trans_cs_p2p)(u32 udevid, u32 peer_udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr);
    int (*addr_trans_p2l)(u32 udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr);
    int (*addr_trans_l2p)(u32 udevid, struct devdrv_addr_desc *addr_desc, u64 *trans_addr);
    bool (*query_addr_trans_support)(enum devdrv_addr_trans_cap_type cap_type);
    int (*urma_copy)(u32 dev_id, enum devdrv_urma_chan_type type, enum devdrv_urma_copy_dir dir,
                     struct devdrv_urma_copy *local, struct devdrv_urma_copy *peer);
    int (*register_seg)(u32 dev_id, struct devdrv_seg_info *info, void **tseg, size_t *out_len);
    int (*unregister_seg)(u32 dev_id, void *tseg, size_t in_len);
    void *(*import_seg)(u32 dev_id, u32 peer_token, void *peer_seg, size_t in_len, size_t *out_len);
    int (*unimport_seg)(u32 dev_id, void *peer_tseg, size_t in_len);
    int (*dma_sync_copy)(u32 udevid, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                         enum devdrv_dma_direction direction);
    int (*dma_async_copy)(u32 udevid, enum devdrv_dma_data_type type, u64 src, u64 dst, u32 size,
                          enum devdrv_dma_direction direction, struct devdrv_asyn_dma_para_info *para_info);
    int (*dma_sync_link_copy)(u32 udevid, enum devdrv_dma_data_type type, int wait_type,
                              struct devdrv_dma_node *dma_node, u32 node_cnt);
    int (*dma_async_link_copy)(u32 udevid, enum devdrv_dma_data_type type, struct devdrv_dma_node *dma_node,
                               u32 node_cnt, struct devdrv_asyn_dma_para_info *para_info);
    int (*dma_sync_copy_plus)(u32 udevid, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst, u32 size,
                              enum devdrv_dma_direction direction);
    int (*dma_async_copy_plus)(u32 udevid, enum devdrv_dma_data_type type, int instance, u64 src, u64 dst, u32 size,
                               enum devdrv_dma_direction direction, struct devdrv_asyn_dma_para_info *para_info);
    int (*dma_sync_link_copy_plus)(u32 udevid, enum devdrv_dma_data_type type, int wait_type, int instance,
                                   struct devdrv_dma_node *dma_node, u32 node_cnt);
    int (*dma_async_link_copy_plus)(u32 udevid, enum devdrv_dma_data_type type, int instance,
                                    struct devdrv_dma_node *dma_node, u32 node_cnt,
                                    struct devdrv_asyn_dma_para_info *para_info);
    int (*dma_sync_link_copy_extend)(u32 udevid, enum devdrv_dma_data_type type, int wait_type,
                                     struct devdrv_dma_node *dma_node, u32 node_cnt);
    int (*dma_sync_link_copy_plus_extend)(u32 udevid, enum devdrv_dma_data_type type, int wait_type, int instance,
                                          struct devdrv_dma_node *dma_node, u32 node_cnt);
    int (*dma_done_schedule)(u32 udevid, enum devdrv_dma_data_type type, int instance);
    int (*dma_get_sq_cq_desc_size)(u32 devid, u32 *sq_desc_size, u32 *cq_desc_size);
    int (*dma_fill_desc_of_sq)(u32 udevid, struct devdrv_dma_prepare *dma_prepare, struct devdrv_dma_node *dma_node,
                               u32 node_cnt, u32 fill_status);
    int (*dma_fill_desc_of_sq_ext)(u32 udevid, void *sq_base, struct devdrv_dma_node *dma_node, u32 node_cnt,
                                   u32 fill_status);
    struct devdrv_dma_prepare *(*dma_link_prepare)(u32 udevid, enum devdrv_dma_data_type type,
                                                   struct devdrv_dma_node *dma_node, u32 node_cnt, u32 fill_status);
    int (*dma_link_free)(struct devdrv_dma_prepare *dma_prepare);
    int (*dma_sqcq_desc_check)(u32 devid, struct devdrv_dma_desc_info *dma_desc_info);
    int (*dma_prepare_alloc_sq_addr)(u32 udevid, u32 node_cnt, struct devdrv_dma_prepare *dma_prepare);
    void (*dma_prepare_free_sq_addr)(u32 udevid, struct devdrv_dma_prepare *dma_prepare);
    void *(*devdrv_dma_alloc_coherent)(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp);
    void *(*devdrv_dma_zalloc_coherent)(ka_device_t *dev, size_t size, ka_dma_addr_t *dma_addr, ka_gfp_t gfp);
    void (*devdrv_dma_free_coherent)(ka_device_t *dev, size_t size, void *addr, ka_dma_addr_t dma_addr);
    ka_dma_addr_t (*devdrv_dma_map_single)(ka_device_t *dev, void *ptr, size_t size, ka_dma_data_direction_t dir);
    void (*devdrv_dma_unmap_single)(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir);
    ka_dma_addr_t (*devdrv_dma_map_page)(ka_device_t *dev, ka_page_t *page, size_t offset, size_t size,
                                         ka_dma_data_direction_t dir);
    void (*devdrv_dma_unmap_page)(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir);
    ka_dma_addr_t (*devdrv_dma_map_page_attrs)(ka_device_t *dev, ka_page_t *page, size_t offset, size_t size,
                                               ka_dma_data_direction_t dir, u64 attrs);
    void (*devdrv_dma_unmap_page_attrs)(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir,
                                        u64 attrs);
    ka_dma_addr_t (*devdrv_dma_map_resource)(ka_device_t *dev, phys_addr_t phys_addr, size_t size,
                                             ka_dma_data_direction_t dir, unsigned long attrs);
    void (*devdrv_dma_unmap_resource)(ka_device_t *dev, ka_dma_addr_t addr, size_t size, ka_dma_data_direction_t dir,
                                      unsigned long attrs);
    int (*devdrv_dma_map_sg_cache)(ka_scatterlist_t *sg, int nents, ka_dma_addr_t *dma_handle,
                                   ka_dma_data_direction_t dir);
    void (*devdrv_dma_unmap_sg_cache)(ka_scatterlist_t *sg, int nents, ka_dma_addr_t dma_handle,
                                      ka_dma_data_direction_t dir);
    int (*devdrv_dma_map_sg_no_cache)(ka_scatterlist_t *sg, int nents, ka_dma_addr_t *dma_handle,
                                      ka_dma_data_direction_t dir);
    void (*devdrv_dma_unmap_sg_no_cache)(ka_scatterlist_t *sg, int nents, ka_dma_addr_t dma_handle,
                                         ka_dma_data_direction_t dir);
    int (*get_dma_copy_support_mode)(u32 udevid, u32 *support_mode);
    void (*set_sq_head)(void *msg_chan, u32 head);
    void *(*get_sq_head)(void *msg_chan, u32 *head);
    void (*move_sq_head)(void *msg_chan);
    void *(*get_sq_tail)(void *msg_chan, u32 *tail);
    void (*move_sq_tail)(void *msg_chan);
    bool (*sq_full_check)(void *msg_chan);
    void *(*get_cq_head)(void *msg_chan);
    void (*move_cq_head)(void *msg_chan);
    void *(*get_cq_tail)(void *msg_chan);
    void (*move_cq_tail)(void *msg_chan);
    int (*force_linkdown)(u32 udevid);
    int (*get_non_trans_chan_capacity)(void *msg_chan, u32 *len);
};

struct devdrv_faultmgr_ops {
    ka_atomic_t ref_cnt;
    int (*devdrv_ubmem_fault_submit)(u32 dev_id, u32 event_type);
};

struct devdrv_serdes_dump_ops {
    ka_atomic_t ref_cnt;
    int (*devdrv_dump_serdes_dfx_reg)(u32 die_id, u32 micro_id, u32 lane_id);
};

int devdrv_register_communication_ops(struct devdrv_comm_ops *ops);
void devdrv_unregister_communication_ops(struct devdrv_comm_ops *ops);
void devdrv_set_communication_ops_status(u32 type, u32 status, u32 dev_id);
void devdrv_set_communication_ops_status_inner(u32 type, u32 status, u32 index_id);
void devdrv_set_probe_dev_bitmap(u32 devid);
void devdrv_clr_probe_dev_bitmap(u32 devid);
u64 devdrv_check_probe_dev_bitmap(u32 devid);
int devdrv_hot_reset_device(u32 dev_id);
int devdrv_hot_pre_reset(u32 dev_id);
int devdrv_hotreset_atomic_reset(u32 dev_id);
int devdrv_hotreset_atomic_unbind(u32 dev_id);
int devdrv_hotreset_atomic_remove(u32 dev_id);
int devdrv_hotreset_atomic_rescan(u32 dev_id);
int devdrv_get_ub_dev_info(u32 dev_id, struct devdrv_ub_dev_info *eid_info, int *num);
int devdrv_get_token_val(u32 dev_id, u32 *token_val);

int devdrv_process_pasid_add(u32 dev_id, u64 pasid);
int devdrv_process_pasid_del(u32 dev_id, u64 pasid);

void *devdrv_pcimsg_alloc_trans_queue(u32 udevid, struct devdrv_trans_msg_chan_info *chan_info);
int devdrv_pcimsg_realease_trans_queue(void *msg_chan);
void devdrv_msg_ring_doorbell(void *msg_chan);
void devdrv_msg_ring_cq_doorbell(void *msg_chan);
void *devdrv_get_msg_chan_host_sq_head(void *msg_chan, u32 *head);
void devdrv_move_msg_chan_host_sq_head(void *msg_chan);
void *devdrv_get_msg_chan_host_cq_head(void *msg_chan);
/* msg chan operate */
void devdrv_move_msg_chan_host_cq_head(void *msg_chan);
void devdrv_set_msg_chan_slave_sq_head(void *msg_chan, u32 head);
void *devdrv_get_msg_chan_slave_sq_tail(void *msg_chan, u32 *tail);
void devdrv_move_msg_chan_slave_sq_tail(void *msg_chan);
bool devdrv_msg_chan_slave_sq_full_check(void *msg_chan);
void *devdrv_get_msg_chan_slave_cq_tail(void *msg_chan);
void devdrv_move_msg_chan_slave_cq_tail(void *msg_chan);
int devdrv_force_linkdown(u32 udevid);

int devdrv_register_faultmgr_ops(struct devdrv_faultmgr_ops *ops);
void devdrv_unregister_faultmgr_ops(void);
int devdrv_register_serdes_dump_ops(struct devdrv_serdes_dump_ops *ops);
void devdrv_unregister_serdes_dump_ops(void);
int devdrv_soc_misc_submit_ubmem_event(u32 dev_id, u32 event_type);
int devdrv_soc_misc_dump_serdes_dfx_reg(u32 die_id, u32 micro_id, u32 lane_id);
int devdrv_get_non_trans_chan_capacity(void *msg_chan, u32 *len);
#endif /* __COMM_MSG_CHAN_H__ */