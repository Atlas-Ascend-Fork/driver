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
#ifndef _ASDRV_UB_MAIN_H_
#define _ASDRV_UB_MAIN_H_

#include "addr_trans.h"
#include "asdrv_ub_dev.h"
#include "pair_dev_info.h"
#include "ummu_core_adapt.h"
#include "adapt_ops.h"

#define ASCEND_UB_REAL_NOTIFIER "asdrv_ub"
#define ASCEND_UB_MIA_NOTIFIER "asdrv_mia_ub"
#define ASCEND_UB_DAVINCI_NOTIFIER "asdrv_davinci"

#define UBDEV_UBUS_DMA_BIT 64

#define UBDRV_UB_SEL 0   // 0-UB 1-PCIE
#define UBDRV_PCIE_SEL 1 // 0-UB 1-PCIE

#define UBDRV_UB_PCIE_SEL_REG_SIZE 4

#define UBDRV_PLATFORM_MASK 0xFFFF
#define UBDRV_PLATFORM_OFFSET 16
#define UBDRV_PLATFORM_FPGA 0
#define UBDRV_PLATFORM_EMU 1
#define UBDRV_PLATFORM_ESL 2

#define UBDRV_VERSION_MASK 0xFFF
#define UBDRV_VERSION_OFFSET 4

#define UBDRV_ENV_TYPE_MASK 0xF
#define UBDRV_ENV_TYPE_OFFSET 0

#define UBDRV_ESL_PCIE_TYPE 4
#define UBDRV_ESL_PCIE_UB_TYPE 5

#define UBDRV_TIME_FOR_ADD_DAVINCI_DEV_TOTAL 50000  // 50s
#define UBDRV_TIME_FOR_ADD_DAVINCI_DEV_EACH_TIME 20 // 20ms

#define UBDRV_DEVICE_OFFLINE_WAIT_CNT 20000000 // 20s
#define UBDRV_DEVICE_REMOVE_WAIT_CNT 1000000U  // 200s

#define UBDRV_P2P_MAX_PROC_NUM 32

#define UBDRV_P2P_CAPABILITY_SHIFT_32 32

#define P2PCAPABILITY_CAPABILITY_ID (0x9ULL)     // bit0-7  : Fixed at 9, vendor specific capability.
#define P2PCAPABILITY_NEXT_POINTER (0x0ULL << 8) // bit8-15 : Fixed at 0, P2P capability simulation
#define P2PCAPABILITY_CAPABILITY_LENGTH \
    (0x8ULL << 16) // bit16-23 : Fixed at 8,
                   // the number of bytes occupied by Virtual P2P Capability
#define P2PCAPABILITY_SIGNATURE_BITS \
    (0x503250ULL << 24)                       // bit24-47 : Fixed at 503250h,
                                              // physical and virtual vendor capability differentiation
#define P2PCAPABILITY_VERSION (0x0ULL << 48)  // bit48-50 : Fixed at 0, the Capability struct description
#define P2PCAPABILITY_GROUP_ID (0x1ULL << 51) // bit51-54 : Fixed at 1, equal to the p2pgroup number
#define P2PCAPABILITY_RESERVED (0x0ULL << 55) // bit55-63 : Fixed at 0, reserved

#define UBDRV_P2P_SUPPORT_MAX_DEVICE 64U
#define UBDRV_ENABLE 1U
#define UBDRV_DISABLE 0
#define UBDRV_P2P_ACCESS_ENABLE 1U
#define UBDRV_P2P_ACCESS_DISABLE 0

struct ubdrv_p2p_attr_info {
    int ref;
    int proc_ref[UBDRV_P2P_MAX_PROC_NUM];
    int pid[UBDRV_P2P_MAX_PROC_NUM];
};

/* EVB PCIE reserved 4 sub machine form */
enum ubdrv_evb_sub_machine_form {
    UBDRV_EVB_SUB_MACHINE_FORM_PCIE_NORMAL = 0,
    UBDRV_EVB_SUB_MACHINE_FORM_PCIE_RESV1 = 1,
    UBDRV_EVB_SUB_MACHINE_FORM_PCIE_RESV2 = 2,
    UBDRV_EVB_SUB_MACHINE_FORM_PCIE_RESV3 = 3,
};

enum ubdrv_equipment_sub_machine_form {
    UBDRV_EQUIPMENT_PCIE_MODULE_SCAN = 0,
    UBDRV_EQUIPMENT_PCIE_MODULE_BI_TSS = 1,
    UBDRV_EQUIPMENT_UB_32G = 2,
    UBDRV_EQUIPMENT_UB_16G = 3,
    /* 4-7: reserved */
    UBDRV_EQUIPMENT_PCIE_CARD_PCIE_BOOT = 8,
    UBDRV_EQUIPMENT_PCIE_CARD_UBOE_BOOT = 9,
};

enum ubdrv_eid_cmp_ret {
    UBDRV_CMP_EQUAL = 0,
    UBDRV_CMP_SMALL = 1,
    UBDRV_CMP_LARGE = 2,
};

struct ascend_ub_dev_status *ubdrv_get_dev_status_mng(u32 dev_id);
int ubdrv_get_device_status(u32 dev_id);
void ubdrv_set_device_status(u32 dev_id, u32 status);
int ubdrv_add_device_status_ref(u32 dev_id);
void ubdrv_sub_device_status_ref(u32 dev_id);
int ubdrv_davinci_bind_fe(struct ub_idev *idev, u32 dev_id);
void ubdrv_davinci_unbind_fe(struct ub_idev *idev, u32 dev_id);
struct dev_eid_info *ubdrv_get_eid_info_by_devid(u32 devid);
struct ubcore_device *ubdrv_get_default_user_ctrl_urma_dev(void);
struct ub_idev *ubdrv_get_idev_by_eid_and_feidx(struct ubcore_eid_info *eid, u32 ue_idx,
                                                enum ubdrv_log_level log_level);
struct ub_idev *ubdrv_get_idev_by_eid(struct ubcore_eid_info *eid, enum ubdrv_log_level log_level);
struct ub_idev *ubdrv_find_idev_by_udevid(u32 dev_id);
int ubdrv_get_ub_dev_info(u32 dev_id, struct devdrv_ub_dev_info *eid_query_info, int *num);
int ubdrv_get_token_val(u32 dev_id, u32 *val);
int ubdrv_add_udma_device(struct ubcore_device *ubc_dev);
void ubdrv_remove_udma_device(struct ubcore_device *ubc_dev, void *client_ctx);
int ubdrv_add_msg_device(u32 dev_id, u32 remote_id, u32 idev_id, u32 ue_idx,
                         struct jetty_exchange_data *data); // ue_idx 0:pf 1-8:vf
void ubdrv_del_msg_device(u32 dev_id, enum ubdrv_dev_status final_state);
struct ascend_ub_msg_dev *ubdrv_get_msg_dev_by_devid(u32 dev_id);
void devdrv_ub_set_device_boot_status(u32 dev_id, u32 status);
int devdrv_ub_get_env_boot_type(u32 dev_id);
int devdrv_ub_get_pfvf_type_by_devid(u32 dev_id);
bool devdrv_ub_is_mdev_vm_boot_mode(u32 dev_id);
bool devdrv_ub_is_sriov_support(u32 dev_id);
struct ascend_dev *ubdrv_get_asd_dev_by_devid(u32 dev_id);
int ubdrv_add_davinci_dev(u32 dev_id, u32 dev_type);
int ubdrv_wait_add_davinci_dev(void);
void ubdrv_remove_davinci_dev(u32 dev_id, u32 dev_type);
enum ubdrv_dev_startup_flag_type ubdrv_get_startup_flag(u32 dev_id);
void ubdrv_set_startup_flag(u32 dev_id, enum ubdrv_dev_startup_flag_type startup_flag);
void ubdrv_set_msg_retry_cnt(u32 dev_id, u32 cnt);
u32 ubdrv_get_msg_retry_cnt(u32 dev_id);
void ubdrv_mutex_lock_polling(u32 dev_id, ka_mutex_t *mutex);
struct ascend_ub_link_res *ubdrv_get_link_res_by_devid(u32 dev_id);
enum ubdrv_eid_cmp_ret ubdrv_cmp_eid(union ubcore_eid *src_eid, union ubcore_eid *dst_eid);
struct devdrv_comm_ops *get_global_ubdrv_ops(void);
void ubdrv_remove_davinci_dev_proc(u32 dev_id);
int ub_check_pack_master_id_to_uda(struct ascend_ub_msg_dev *msg_dev, struct uda_dev_para *uda_para, u32 dev_id);
void ubdrv_exit_release_msg_chan_proc(u32 dev_id);
void uda_dev_type_pack_proc(struct uda_dev_type *uda_type, u32 dev_type);
int ubdrv_register_uda_notifier(void);
void ubdrv_unregister_uda_notifier(void);
int ubdrv_module_init_for_pcie(void);
void devdrv_set_communication_ops_status_proc(u32 type, u32 status, u32 dev_id);
void ubdrv_module_uninit_for_pcie(void);
void ubdrv_module_exit_proc(void);
u32 get_global_add_davinci_flag(void);
int devdrv_ub_get_connect_protocol(u32 dev_id);
int ubdrv_process_add_pasid(u32 dev_id, u64 pasid);
int ubdrv_process_del_pasid(u32 dev_id, u64 pasid);
int ubdrv_mia_dev_notifier_func(u32 udevid, enum uda_notified_action action);
struct ascend_ub_ctrl *get_global_ub_ctrl(void);
struct ubcore_client *get_global_ascend_client(void);
int ubdrv_ub_enable_funcs(u32 devid, u32 boot_mode);
int ubdrv_ub_disable_funcs(u32 devid, u32 boot_mode);
int devdrv_ub_get_device_boot_status(u32 devid, u32 *boot_status);
int ubdrv_add_msg_device_proc(struct ascend_ub_msg_dev *msg_dev, u32 dev_id);
int ubdrv_module_init_proc(void);
int ubdrv_fe_init_instance(u32 udevid);
int ubdrv_fe_uninit_instance(u32 udevid);
void ubdrv_admin_msg_chan_uninit(u32 dev_id, struct ascend_ub_msg_dev *msg_dev);
int ubdrv_init_ub_ctrl(void);
void ubdrv_uninit_ub_ctrl(void);
int ubdrv_alloc_attr_info(void);
void ubdrv_free_attr_info(void);
int ubdrv_get_ub_pcie_sel(void);

int ubdrv_get_local_token(u32 dev_id, u32 *token_value);
void ubdrv_set_local_token(u32 dev_id, u32 token_value, u32 token_valid);
int ubdrv_get_dev_id_info(u32 dev_id, struct devdrv_dev_id_info *id_info);
void ubdrv_set_dev_id_info(u32 dev_id, struct ubdrv_id_info *id_info, u32 info_valid);

int ubdrv_find_eid_index_by_eid(struct ub_idev *idev, struct ubcore_eid_info *eid, u32 *eid_idx);
int ubdrv_set_chip_type_from_image_load(u32 device_id);
void ubdrv_set_chip_type_from_exchange_data(u32 chip_type);
u32 ubdrv_get_chip_type(void);
int ubdrv_parse_ub_pcie_by_value(u32 platform_type, u32 env_type);
#endif