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

#ifndef __UBCORE_ADAPT_H__
#define __UBCORE_ADAPT_H__

#include "ubcore_types.h"
#include "ubcore_uapi.h"
#include "module_type.h"

#define UBA_UBCORE_JFC_ID UBCORE_JFC_ID
#define UBA_UBCORE_JFC_CQE_BASE_ADDR UBCORE_JFC_CQE_BASE_ADDR
#define UBA_UBCORE_JFC_FULL_CTX UBCORE_JFC_FULL_CTX
#define UBA_UBCORE_JFS_SQE_BASE_ADDR UBCORE_JFS_SQE_BASE_ADDR
#define UBA_UBCORE_JFS_FULL_CTX UBCORE_JFS_FULL_CTX
#define UBA_UBCORE_JFR_FULL_CTX UBCORE_JFR_FULL_CTX

#define UBA_UBCORE_TP_RM UBCORE_TP_RM
#define UBA_UBCORE_TP_RC UBCORE_TP_RC
#define UBA_UBCORE_TP_UM UBCORE_TP_UM

#define UBA_UBCORE_JFR UBCORE_JFR
#define UBA_UBCORE_JETTY UBCORE_JETTY

#define UBA_UBCORE_CTP UBCORE_CTP
#define UBA_UBCORE_TOKEN_PLAIN_TEXT UBCORE_TOKEN_PLAIN_TEXT
#define UBA_UBCORE_TOKEN_NONE UBCORE_TOKEN_NONE

#define UBA_UBCORE_ACCESS_LOCAL_ONLY UBCORE_ACCESS_LOCAL_ONLY
#define UBA_UBCORE_ACCESS_READ UBCORE_ACCESS_READ
#define UBA_UBCORE_ACCESS_WRITE UBCORE_ACCESS_WRITE
#define UBA_UBCORE_ACCESS_ATOMIC UBCORE_ACCESS_ATOMIC

#define UBA_UBCORE_OPC_WRITE_IMM UBCORE_OPC_WRITE_IMM
#define UBA_UBCORE_CR_WR_FLUSH_ERR_DONE UBCORE_CR_WR_FLUSH_ERR_DONE
#define UBA_UBCORE_JFS_STATE UBCORE_JFS_STATE

#define UBA_UBCORE_EID_SIZE UBCORE_EID_SIZE
#define UBA_UBCORE_DEVID_SIZE UBCORE_DEVID_SIZE
#define UBA_UBCORE_MAX_DEV_NAME UBCORE_MAX_DEV_NAME

#define UBA_UBCORE_JFC_CI UBCORE_JFC_CI
#define UBA_UBCORE_JFS_CI UBCORE_JFS_CI

#define UBA_UBCORE_TRANSPORT_UB UBCORE_TRANSPORT_UB
typedef enum ubcore_transport_type uba_ubcore_transport_type;

typedef struct ubcore_eid_info uba_ubcore_eid_info;
typedef struct ubcore_cg_object uba_ubcore_cg_object;
typedef struct ubcore_token_id uba_ubcore_token_id;
typedef union ubcore_token_id_flag uba_ubcore_token_id_flag;
typedef struct ubcore_token uba_ubcore_token;
typedef struct ubcore_seg uba_ubcore_seg;
typedef struct ubcore_ubva uba_ubcore_ubva;
typedef struct ubcore_udrv_priv uba_ubcore_udrv_priv;
typedef struct ubcore_jetty_id uba_ubcore_jetty_id;
typedef struct ubcore_jfc_attr uba_ubcore_jfc_attr;
typedef struct ubcore_jfs_attr uba_ubcore_jfs_attr;
typedef struct ubcore_jfr_attr uba_ubcore_jfr_attr;
typedef struct ubcore_jetty_attr uba_ubcore_jetty_attr;
typedef struct ubcore_ht_param uba_ubcore_ht_param;
typedef struct ubcore_tp uba_ubcore_tp;
typedef struct ubcore_vtpn uba_ubcore_vtpn;
typedef struct ubcore_jetty_grp_cfg uba_ubcore_jetty_grp_cfg;
typedef struct ubcore_sg uba_ubcore_sg;
typedef struct ubcore_sge uba_ubcore_sge;
typedef struct ubcore_rw_wr uba_ubcore_rw_wr;
typedef struct ubcore_send_wr uba_ubcore_send_wr;
typedef struct ubcore_cas_wr uba_ubcore_cas_wr;
typedef struct ubcore_faa_wr uba_ubcore_faa_wr;
typedef struct ubcore_cr_token uba_ubcore_cr_token;
typedef struct ubcore_user_ctl_in uba_ubcore_user_ctl_in;
typedef struct ubcore_user_ctl_out uba_ubcore_user_ctl_out;
typedef struct ubcore_net_addr uba_ubcore_net_addr;
typedef struct ubcore_jetty_group uba_ubcore_jetty_group;
typedef struct ubcore_ops uba_ubcore_ops;
typedef struct ubcore_device_attr uba_ubcore_device_attr;
typedef struct ubcore_tp_ext uba_ubcore_tp_ext;
typedef struct ubcore_event uba_ubcore_event;
typedef struct ubcore_event_handler uba_ubcore_event_handler;
typedef struct ubcore_device_cfg uba_ubcore_device_cfg;
typedef struct ubcore_hash_table uba_ubcore_hash_table;
typedef struct ubcore_eid_table uba_ubcore_eid_table;
typedef struct ubcore_cg_device uba_ubcore_cg_device;
typedef struct ubcore_sip_table uba_ubcore_sip_table;
typedef struct ubcore_logic_device uba_ubcore_logic_device;

typedef union ubcore_eid uba_ubcore_eid;
typedef union ubcore_reg_seg_flag uba_ubcore_reg_seg_flag;
typedef union ubcore_seg_attr uba_ubcore_seg_attr;
typedef union ubcore_import_seg_flag uba_ubcore_import_seg_flag;
typedef union ubcore_jfc_flag uba_ubcore_jfc_flag;
typedef union ubcore_jfs_flag uba_ubcore_jfs_flag;
typedef union ubcore_jfr_flag uba_ubcore_jfr_flag;
typedef union ubcore_jetty_flag uba_ubcore_jetty_flag;
typedef union ubcore_import_jetty_flag uba_ubcore_import_jetty_flag;
typedef union ubcore_jfs_wr_flag uba_ubcore_jfs_wr_flag;
typedef union ubcore_cr_flag uba_ubcore_cr_flag;
typedef union ubcore_device_cfg_mask uba_ubcore_device_cfg_mask;
typedef union ubcore_device_feat uba_ubcore_device_feat;
typedef union ubcore_atomic_feat uba_ubcore_atomic_feat;
typedef union ubcore_utp_cfg_flag uba_ubcore_utp_cfg_flag;
typedef union ubcore_vtp_cfg_flag uba_ubcore_vtp_cfg_flag;
typedef union ubcore_tp_flag uba_ubcore_tp_flag;
typedef union ubcore_jetty_grp_flag uba_ubcore_jetty_grp_flag;
typedef union ubcore_tp_cfg_flag uba_ubcore_tp_cfg_flag;
typedef union ubcore_tp_mod_flag uba_ubcore_tp_mod_flag;
typedef union ubcore_tp_attr_mask uba_ubcore_tp_attr_mask;
typedef union ubcore_vtp_attr_mask uba_ubcore_vtp_attr_mask;
typedef union ubcore_net_addr_union uba_ubcore_net_addr_union;

typedef struct ubcore_ueid_cfg uba_ubcore_ueid_cfg;
typedef struct ubcore_device_status uba_ubcore_device_status;
typedef struct ubcore_port_status uba_ubcore_port_status;
typedef struct ubcore_res_key uba_ubcore_res_key;
typedef struct ubcore_res_val uba_ubcore_res_val;
typedef struct ubcore_cc_entry uba_ubcore_cc_entry;
typedef struct ubcore_stats_key uba_ubcore_stats_key;
typedef struct ubcore_stats_val uba_ubcore_stats_val;
typedef struct ubcore_tp_cfg uba_ubcore_tp_cfg;
typedef struct ubcore_tp_attr uba_ubcore_tp_attr;
typedef struct ubcore_req uba_ubcore_req;
typedef struct ubcore_resp uba_ubcore_resp;
typedef struct ubcore_resp_host uba_ubcore_resp_host;
typedef struct ubcore_devid uba_ubcore_devid;
typedef struct ubcore_vtp_attr uba_ubcore_vtp_attr;
typedef struct ubcore_tpg uba_ubcore_tpg;
typedef struct ubcore_vtp uba_ubcore_vtp;
typedef struct ubcore_guid uba_ubcore_guid;
typedef struct ubcore_device_cap uba_ubcore_device_cap;
typedef struct ubcore_port_attr uba_ubcore_port_attr;
typedef struct ubcore_rc_cfg uba_ubcore_rc_cfg;
typedef struct ubcore_tpg_cfg uba_ubcore_tpg_cfg;
typedef struct ubcore_tpg_ext uba_ubcore_tpg_ext;
typedef struct ubcore_vtp_cfg uba_ubcore_vtp_cfg;
typedef struct ubcore_utp uba_ubcore_utp;
typedef struct ubcore_utp_cfg uba_ubcore_utp_cfg;
typedef struct ubcore_ctp uba_ubcore_ctp;
typedef struct ubcore_ctp_cfg uba_ubcore_ctp_cfg;

typedef struct ubcore_device uba_ubcore_device;
typedef struct ubcore_jfc_cfg uba_ubcore_jfc_cfg;
typedef struct ubcore_jfr_cfg uba_ubcore_jfr_cfg;
typedef struct ubcore_jfs_cfg uba_ubcore_jfs_cfg;
typedef struct ubcore_udata uba_ubcore_udata;
typedef struct ubcore_jfc uba_ubcore_jfc;
typedef struct ubcore_jfr uba_ubcore_jfr;
typedef struct ubcore_jfs uba_ubcore_jfs;
typedef struct ubcore_ucontext uba_ubcore_ucontext;
typedef ubcore_comp_callback_t uba_ubcore_comp_callback_t;
typedef ubcore_event_callback_t uba_ubcore_event_callback_t;
typedef struct ubcore_jfr_wr uba_ubcore_jfr_wr;
typedef struct ubcore_cr uba_ubcore_cr;
typedef struct ubcore_target_seg uba_ubcore_target_seg;
typedef struct ubcore_seg_cfg uba_ubcore_seg_cfg;
typedef struct ubcore_target_seg_cfg uba_ubcore_target_seg_cfg;
typedef struct ubcore_tjetty_cfg uba_ubcore_tjetty_cfg;
typedef struct ubcore_tjetty uba_ubcore_tjetty;
typedef struct ubcore_jfs_wr uba_ubcore_jfs_wr;
typedef struct ubcore_jetty uba_ubcore_jetty;
typedef struct ubcore_active_tp_cfg uba_ubcore_active_tp_cfg;
typedef struct ubcore_get_tp_cfg uba_ubcore_get_tp_cfg;
typedef struct ubcore_tp_info uba_ubcore_tp_info;
typedef struct ubcore_user_ctl uba_ubcore_user_ctl;
typedef struct ubcore_jetty_cfg uba_ubcore_jetty_cfg;

uba_ubcore_jfc *uba_ubcore_create_jfc(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_jfc_cfg *cfg,
                                      uba_ubcore_comp_callback_t jfce_handler, uba_ubcore_event_callback_t jfae_handler,
                                      uba_ubcore_udata *udata);
int uba_ubcore_delete_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc);
uba_ubcore_jfr *uba_ubcore_create_jfr(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_jfr_cfg *cfg,
                                      uba_ubcore_event_callback_t jfae_handler, uba_ubcore_udata *udata);
int uba_ubcore_delete_jfr(enum uba_module_type module_type, uba_ubcore_jfr *jfr);
int uba_ubcore_post_jfr_wr(enum uba_module_type module_type, uba_ubcore_jfr *jfr, uba_ubcore_jfr_wr *wr,
                           uba_ubcore_jfr_wr **bad_wr);
int uba_ubcore_poll_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc, int cr_cnt, uba_ubcore_cr *cr);
int uba_ubcore_rearm_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc, bool solicited_only);
uba_ubcore_target_seg *uba_ubcore_register_seg(enum uba_module_type module_type, uba_ubcore_device *dev,
                                               uba_ubcore_seg_cfg *cfg, uba_ubcore_udata *udata);
int uba_ubcore_unregister_seg(enum uba_module_type module_type, uba_ubcore_target_seg *tseg);
uba_ubcore_target_seg *uba_ubcore_import_seg(enum uba_module_type module_type, uba_ubcore_device *dev,
                                             uba_ubcore_target_seg_cfg *cfg, uba_ubcore_udata *udata);
int uba_ubcore_unimport_seg(enum uba_module_type module_type, uba_ubcore_target_seg *tseg);

int uba_ubcore_alloc_jfc(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_jfc_cfg *cfg,
                         uba_ubcore_comp_callback_t jfce_handler, uba_ubcore_event_callback_t jfae_handler,
                         uba_ubcore_jfc **jfc, uba_ubcore_udata *udata);
int uba_ubcore_set_jfc_opt(enum uba_module_type module_type, uba_ubcore_jfc *jfc, uint64_t opt, void *buf, uint32_t len,
                           uba_ubcore_udata *udata);
int uba_ubcore_active_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc, uba_ubcore_udata *udata);
int uba_ubcore_deactive_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc, uba_ubcore_udata *udata);
int uba_ubcore_free_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc, uba_ubcore_udata *udata);
int uba_ubcore_alloc_jfs(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_jfs_cfg *cfg,
                         uba_ubcore_event_callback_t jfae_handler, uba_ubcore_jfs **jfs, uba_ubcore_udata *udata);
int uba_ubcore_set_jfs_opt(enum uba_module_type module_type, uba_ubcore_jfs *jfs, uint64_t opt, void *buf, uint32_t len,
                           uba_ubcore_udata *udata);
int uba_ubcore_active_jfs(enum uba_module_type module_type, uba_ubcore_jfs *jfs, uba_ubcore_udata *udata);
int uba_ubcore_deactive_jfs(enum uba_module_type module_type, uba_ubcore_jfs *jfs, uba_ubcore_udata *udata);
int uba_ubcore_free_jfs(enum uba_module_type module_type, uba_ubcore_jfs *jfs, uba_ubcore_udata *udata);
uba_ubcore_tjetty *uba_ubcore_import_jfr_ex(enum uba_module_type module_type, uba_ubcore_device *dev,
                                            uba_ubcore_tjetty_cfg *cfg, uba_ubcore_active_tp_cfg *active_tp_cfg,
                                            uba_ubcore_udata *udata);
int uba_ubcore_get_tp_list(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_get_tp_cfg *cfg,
                           uint32_t *tp_cnt, uba_ubcore_tp_info *tp_list, uba_ubcore_udata *udata);
int uba_ubcore_user_control(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_user_ctl *k_user_ctl);
uba_ubcore_tjetty *uba_ubcore_import_jetty_ex(enum uba_module_type module_type, uba_ubcore_device *dev,
                                              uba_ubcore_tjetty_cfg *cfg, uba_ubcore_active_tp_cfg *active_tp_cfg,
                                              uba_ubcore_udata *udata);
uba_ubcore_tjetty *uba_ubcore_import_jfr(enum uba_module_type module_type, uba_ubcore_device *dev,
                                         uba_ubcore_tjetty_cfg *cfg, uba_ubcore_udata *udata);
int uba_ubcore_unimport_jfr(enum uba_module_type module_type, uba_ubcore_tjetty *tjfr);
int uba_ubcore_unimport_jetty(enum uba_module_type module_type, uba_ubcore_tjetty *tjfr);
int uba_ubcore_delete_jetty(enum uba_module_type module_type, uba_ubcore_jetty *jetty);
int uba_ubcore_post_jetty_send_wr(enum uba_module_type module_type, uba_ubcore_jetty *jetty, uba_ubcore_jfs_wr *wr,
                                  uba_ubcore_jfs_wr **bad_wr);
uba_ubcore_jetty *uba_ubcore_create_jetty(enum uba_module_type module_type, uba_ubcore_device *dev,
                                          uba_ubcore_jetty_cfg *cfg, uba_ubcore_event_callback_t jfae_handler,
                                          uba_ubcore_udata *udata);
uba_ubcore_tjetty *uba_ubcore_import_jetty(enum uba_module_type module_type, uba_ubcore_device *dev,
                                           uba_ubcore_tjetty_cfg *cfg, uba_ubcore_udata *udata);
void uba_ubcore_put_device(enum uba_module_type module_type, uba_ubcore_device *dev);
int uba_ubcore_get_jfc_opt_by_id(enum uba_module_type module_type, uba_ubcore_device *dev, uint32_t jfc_id,
                                 uint64_t opt, void *buf, uint32_t len);
int uba_ubcore_get_jetty_opt_by_id(enum uba_module_type module_type, uba_ubcore_device *dev, uint32_t jetty_id,
                                   uint64_t opt, void *buf, uint32_t len);
uba_ubcore_device *uba_ubcore_get_device_by_eid(enum uba_module_type module_type, uba_ubcore_eid *eid,
                                                uba_ubcore_transport_type type);

#endif
