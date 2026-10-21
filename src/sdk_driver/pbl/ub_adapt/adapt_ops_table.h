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

#ifndef __ADAPT_OPS_TABLE_H__
#define __ADAPT_OPS_TABLE_H__

#include <linux/ummu_core.h>
#include "ubcore_types.h"

struct uba_ubcore_adapt_ops {
    struct ubcore_jfc *(*create_jfc)(struct ubcore_device *dev, struct ubcore_jfc_cfg *cfg,
                                     ubcore_comp_callback_t jfce_handler, ubcore_event_callback_t jfae_handler,
                                     struct ubcore_udata *udata);
    int (*delete_jfc)(struct ubcore_jfc *jfc);
    struct ubcore_jfr *(*create_jfr)(struct ubcore_device *dev, struct ubcore_jfr_cfg *cfg,
                                     ubcore_event_callback_t jfae_handler, struct ubcore_udata *udata);
    int (*delete_jfr)(struct ubcore_jfr *jfr);
    int (*post_jfr_wr)(struct ubcore_jfr *jfr, struct ubcore_jfr_wr *wr, struct ubcore_jfr_wr **bad_wr);
    int (*poll_jfc)(struct ubcore_jfc *jfc, int cr_cnt, struct ubcore_cr *cr);
    int (*rearm_jfc)(struct ubcore_jfc *jfc, bool solicited_only);
    struct ubcore_target_seg *(*register_seg)(struct ubcore_device *dev, struct ubcore_seg_cfg *cfg,
                                              struct ubcore_udata *udata);
    int (*unregister_seg)(struct ubcore_target_seg *tseg);
    struct ubcore_target_seg *(*import_seg)(struct ubcore_device *dev, struct ubcore_target_seg_cfg *cfg,
                                            struct ubcore_udata *udata);
    int (*unimport_seg)(struct ubcore_target_seg *tseg);
    int (*alloc_jfc)(struct ubcore_device *dev, struct ubcore_jfc_cfg *cfg, ubcore_comp_callback_t jfce_handler,
                     ubcore_event_callback_t jfae_handler, struct ubcore_jfc **jfc, struct ubcore_udata *udata);
    int (*set_jfc_opt)(struct ubcore_jfc *jfc, uint64_t opt, void *buf, uint32_t len, struct ubcore_udata *udata);
    int (*active_jfc)(struct ubcore_jfc *jfc, struct ubcore_udata *udata);
    int (*deactive_jfc)(struct ubcore_jfc *jfc, struct ubcore_udata *udata);
    int (*free_jfc)(struct ubcore_jfc *jfc, struct ubcore_udata *udata);
    int (*alloc_jfs)(struct ubcore_device *dev, struct ubcore_jfs_cfg *cfg, ubcore_event_callback_t jfae_handler,
                     struct ubcore_jfs **jfs, struct ubcore_udata *udata);
    int (*set_jfs_opt)(struct ubcore_jfs *jfs, uint64_t opt, void *buf, uint32_t len, struct ubcore_udata *udata);
    int (*active_jfs)(struct ubcore_jfs *jfs, struct ubcore_udata *udata);
    int (*deactive_jfs)(struct ubcore_jfs *jfs, struct ubcore_udata *udata);
    int (*free_jfs)(struct ubcore_jfs *jfs, struct ubcore_udata *udata);
    struct ubcore_tjetty *(*import_jfr_ex)(struct ubcore_device *dev, struct ubcore_tjetty_cfg *cfg,
                                           struct ubcore_active_tp_cfg *active_tp_cfg, struct ubcore_udata *udata);
    int (*get_tp_list)(struct ubcore_device *dev, struct ubcore_get_tp_cfg *cfg, uint32_t *tp_cnt,
                       struct ubcore_tp_info *tp_list, struct ubcore_udata *udata);
    int (*user_control)(struct ubcore_device *dev, struct ubcore_user_ctl *k_user_ctl);
    struct ubcore_tjetty *(*import_jetty_ex)(struct ubcore_device *dev, struct ubcore_tjetty_cfg *cfg,
                                             struct ubcore_active_tp_cfg *active_tp_cfg, struct ubcore_udata *udata);
    struct ubcore_tjetty *(*import_jfr)(struct ubcore_device *dev, struct ubcore_tjetty_cfg *cfg,
                                        struct ubcore_udata *udata);
    int (*unimport_jfr)(struct ubcore_tjetty *tjfr);
    int (*unimport_jetty)(struct ubcore_tjetty *tjfr);
    int (*delete_jetty)(struct ubcore_jetty *jetty);
    int (*post_jetty_send_wr)(struct ubcore_jetty *jetty, struct ubcore_jfs_wr *wr, struct ubcore_jfs_wr **bad_wr);
    struct ubcore_jetty *(*create_jetty)(struct ubcore_device *dev, struct ubcore_jetty_cfg *cfg,
                                         ubcore_event_callback_t jfae_handler, struct ubcore_udata *udata);
    struct ubcore_tjetty *(*import_jetty)(struct ubcore_device *dev, struct ubcore_tjetty_cfg *cfg,
                                          struct ubcore_udata *udata);
    void (*put_device)(struct ubcore_device *dev);
    int (*get_jfc_opt_by_id)(struct ubcore_device *dev, uint32_t jfc_id, uint64_t opt, void *buf, uint32_t len);
    int (*get_jetty_opt_by_id)(struct ubcore_device *dev, uint32_t jetty_id, uint64_t opt, void *buf, uint32_t len);
    struct ubcore_device *(*get_device_by_eid)(union ubcore_eid *eid, enum ubcore_transport_type type);
};

struct uba_ummu_adapt_ops {
    struct device *(*alloc_tdev)(struct tdev_attr *attr, u32 *ptid);
    void (*free_tdev)(struct device *tdev);
};

#endif /* __ADAPT_OPS_TABLE_H__ */
