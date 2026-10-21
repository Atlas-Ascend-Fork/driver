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

#include "asdrv_ub_ubcore_adapt.h"
#include "adapt_ops.h"
#include "ubcore_adapt.h"
#include "asdrv_ub_common.h"

STATIC uba_ubcore_jfc *uba_ubdrv_create_jfc(uba_ubcore_device *dev, uba_ubcore_jfc_cfg *cfg,
                                            uba_ubcore_comp_callback_t jfce_handler,
                                            uba_ubcore_event_callback_t jfae_handler, uba_ubcore_udata *udata)
{
    return ubcore_create_jfc(dev, cfg, jfce_handler, jfae_handler, udata);
}

STATIC int uba_ubdrv_delete_jfc(uba_ubcore_jfc *jfc)
{
    return ubcore_delete_jfc(jfc);
}

STATIC uba_ubcore_jfr *uba_ubdrv_create_jfr(uba_ubcore_device *dev, uba_ubcore_jfr_cfg *cfg,
                                            uba_ubcore_event_callback_t jfae_handler, uba_ubcore_udata *udata)
{
    return ubcore_create_jfr(dev, cfg, jfae_handler, udata);
}

STATIC int uba_ubdrv_delete_jfr(uba_ubcore_jfr *jfr)
{
    return ubcore_delete_jfr(jfr);
}

STATIC int uba_ubdrv_post_jfr_wr(uba_ubcore_jfr *jfr, uba_ubcore_jfr_wr *wr, uba_ubcore_jfr_wr **bad_wr)
{
    return ubcore_post_jfr_wr(jfr, wr, bad_wr);
}

STATIC int uba_ubdrv_poll_jfc(uba_ubcore_jfc *jfc, int cr_cnt, uba_ubcore_cr *cr)
{
    return ubcore_poll_jfc(jfc, cr_cnt, cr);
}

STATIC int uba_ubdrv_rearm_jfc(uba_ubcore_jfc *jfc, bool solicited_only)
{
    return ubcore_rearm_jfc(jfc, solicited_only);
}

STATIC uba_ubcore_target_seg *uba_ubdrv_register_seg(uba_ubcore_device *dev, uba_ubcore_seg_cfg *cfg,
                                                     uba_ubcore_udata *udata)
{
    return ubcore_register_seg(dev, cfg, udata);
}

STATIC int uba_ubdrv_unregister_seg(uba_ubcore_target_seg *tseg)
{
    return ubcore_unregister_seg(tseg);
}

STATIC uba_ubcore_target_seg *uba_ubdrv_import_seg(uba_ubcore_device *dev, uba_ubcore_target_seg_cfg *cfg,
                                                   uba_ubcore_udata *udata)
{
    return ubcore_import_seg(dev, cfg, udata);
}

STATIC int uba_ubdrv_unimport_seg(uba_ubcore_target_seg *tseg)
{
    return ubcore_unimport_seg(tseg);
}

STATIC int uba_ubdrv_alloc_jfc(uba_ubcore_device *dev, uba_ubcore_jfc_cfg *cfg, uba_ubcore_comp_callback_t jfce_handler,
                               uba_ubcore_event_callback_t jfae_handler, uba_ubcore_jfc **jfc, uba_ubcore_udata *udata)
{
    return ubcore_alloc_jfc(dev, cfg, jfce_handler, jfae_handler, jfc, udata);
}

STATIC int uba_ubdrv_set_jfc_opt(uba_ubcore_jfc *jfc, uint64_t opt, void *buf, uint32_t len, uba_ubcore_udata *udata)
{
    return ubcore_set_jfc_opt(jfc, opt, buf, len, udata);
}

STATIC int uba_ubdrv_active_jfc(uba_ubcore_jfc *jfc, uba_ubcore_udata *udata)
{
    return ubcore_active_jfc(jfc, udata);
}

STATIC int uba_ubdrv_deactive_jfc(uba_ubcore_jfc *jfc, uba_ubcore_udata *udata)
{
    return ubcore_deactive_jfc(jfc, udata);
}

STATIC int uba_ubdrv_free_jfc(uba_ubcore_jfc *jfc, uba_ubcore_udata *udata)
{
    return ubcore_free_jfc(jfc, udata);
}

STATIC int uba_ubdrv_alloc_jfs(uba_ubcore_device *dev, uba_ubcore_jfs_cfg *cfg,
                               uba_ubcore_event_callback_t jfae_handler, uba_ubcore_jfs **jfs, uba_ubcore_udata *udata)
{
    return ubcore_alloc_jfs(dev, cfg, jfae_handler, jfs, udata);
}

STATIC int uba_ubdrv_set_jfs_opt(uba_ubcore_jfs *jfs, uint64_t opt, void *buf, uint32_t len, uba_ubcore_udata *udata)
{
    return ubcore_set_jfs_opt(jfs, opt, buf, len, udata);
}

STATIC int uba_ubdrv_active_jfs(uba_ubcore_jfs *jfs, uba_ubcore_udata *udata)
{
    return ubcore_active_jfs(jfs, udata);
}

STATIC int uba_ubdrv_deactive_jfs(uba_ubcore_jfs *jfs, uba_ubcore_udata *udata)
{
    return ubcore_deactive_jfs(jfs, udata);
}

STATIC int uba_ubdrv_free_jfs(uba_ubcore_jfs *jfs, uba_ubcore_udata *udata)
{
    return ubcore_free_jfs(jfs, udata);
}

STATIC uba_ubcore_tjetty *uba_ubdrv_import_jfr_ex(uba_ubcore_device *dev, uba_ubcore_tjetty_cfg *cfg,
                                                  uba_ubcore_active_tp_cfg *active_tp_cfg, uba_ubcore_udata *udata)
{
    return ubcore_import_jfr_ex(dev, cfg, active_tp_cfg, udata);
}

STATIC int uba_ubdrv_get_tp_list(uba_ubcore_device *dev, uba_ubcore_get_tp_cfg *cfg, uint32_t *tp_cnt,
                                 uba_ubcore_tp_info *tp_list, uba_ubcore_udata *udata)
{
    return ubcore_get_tp_list(dev, cfg, tp_cnt, tp_list, udata);
}

STATIC int uba_ubdrv_user_control(uba_ubcore_device *dev, uba_ubcore_user_ctl *k_user_ctl)
{
    return ubcore_user_control(dev, k_user_ctl);
}

STATIC uba_ubcore_tjetty *uba_ubdrv_import_jetty_ex(uba_ubcore_device *dev, uba_ubcore_tjetty_cfg *cfg,
                                                    uba_ubcore_active_tp_cfg *active_tp_cfg, uba_ubcore_udata *udata)
{
    return ubcore_import_jetty_ex(dev, cfg, active_tp_cfg, udata);
}

STATIC uba_ubcore_tjetty *uba_ubdrv_import_jfr(uba_ubcore_device *dev, uba_ubcore_tjetty_cfg *cfg,
                                               uba_ubcore_udata *udata)
{
    return ubcore_import_jfr(dev, cfg, udata);
}

STATIC int uba_ubdrv_unimport_jfr(uba_ubcore_tjetty *tjfr)
{
    return ubcore_unimport_jfr(tjfr);
}

STATIC int uba_ubdrv_unimport_jetty(uba_ubcore_tjetty *tjfr)
{
    return ubcore_unimport_jetty(tjfr);
}

STATIC int uba_ubdrv_delete_jetty(uba_ubcore_jetty *jetty)
{
    return ubcore_delete_jetty(jetty);
}

STATIC int uba_ubdrv_post_jetty_send_wr(uba_ubcore_jetty *jetty, uba_ubcore_jfs_wr *wr, uba_ubcore_jfs_wr **bad_wr)
{
    return ubcore_post_jetty_send_wr(jetty, wr, bad_wr);
}

STATIC uba_ubcore_jetty *uba_ubdrv_create_jetty(uba_ubcore_device *dev, uba_ubcore_jetty_cfg *cfg,
                                                uba_ubcore_event_callback_t jfae_handler, uba_ubcore_udata *udata)
{
    return ubcore_create_jetty(dev, cfg, jfae_handler, udata);
}

STATIC uba_ubcore_tjetty *uba_ubdrv_import_jetty(uba_ubcore_device *dev, uba_ubcore_tjetty_cfg *cfg,
                                                 uba_ubcore_udata *udata)
{
    return ubcore_import_jetty(dev, cfg, udata);
}

/* ubcore_put_device is available since openEuler-24.03-LTS-SP4, and currently only device-side trs module uses it.
 * Exclude on host to avoid driver dkms install failure on old OS that lacks this symbol.
 */
#ifndef DRV_HOST
STATIC void uba_ubdrv_put_device(uba_ubcore_device *dev)
{
    ubcore_put_device(dev);
}
#endif

STATIC int uba_ubdrv_get_jfc_opt_by_id(uba_ubcore_device *dev, uint32_t jfc_id, uint64_t opt, void *buf, uint32_t len)
{
    return ubcore_get_jfc_opt_by_id(dev, jfc_id, opt, buf, len);
}

STATIC int uba_ubdrv_get_jetty_opt_by_id(uba_ubcore_device *dev, uint32_t jetty_id, uint64_t opt, void *buf,
                                         uint32_t len)
{
    return ubcore_get_jetty_opt_by_id(dev, jetty_id, opt, buf, len);
}

STATIC uba_ubcore_device *uba_ubdrv_get_device_by_eid(uba_ubcore_eid *eid, uba_ubcore_transport_type type)
{
    return ubcore_get_device_by_eid(eid, type);
}

STATIC struct uba_ubcore_adapt_ops g_ubcore_ops = {
    .create_jfc = uba_ubdrv_create_jfc,
    .delete_jfc = uba_ubdrv_delete_jfc,
    .create_jfr = uba_ubdrv_create_jfr,
    .delete_jfr = uba_ubdrv_delete_jfr,
    .post_jfr_wr = uba_ubdrv_post_jfr_wr,
    .poll_jfc = uba_ubdrv_poll_jfc,
    .rearm_jfc = uba_ubdrv_rearm_jfc,
    .register_seg = uba_ubdrv_register_seg,
    .unregister_seg = uba_ubdrv_unregister_seg,
    .import_seg = uba_ubdrv_import_seg,
    .unimport_seg = uba_ubdrv_unimport_seg,
    .alloc_jfc = uba_ubdrv_alloc_jfc,
    .set_jfc_opt = uba_ubdrv_set_jfc_opt,
    .active_jfc = uba_ubdrv_active_jfc,
    .deactive_jfc = uba_ubdrv_deactive_jfc,
    .free_jfc = uba_ubdrv_free_jfc,
    .alloc_jfs = uba_ubdrv_alloc_jfs,
    .set_jfs_opt = uba_ubdrv_set_jfs_opt,
    .active_jfs = uba_ubdrv_active_jfs,
    .deactive_jfs = uba_ubdrv_deactive_jfs,
    .free_jfs = uba_ubdrv_free_jfs,
    .import_jfr = uba_ubdrv_import_jfr,
    .import_jfr_ex = uba_ubdrv_import_jfr_ex,
    .import_jetty = uba_ubdrv_import_jetty,
    .import_jetty_ex = uba_ubdrv_import_jetty_ex,
    .create_jetty = uba_ubdrv_create_jetty,
    .delete_jetty = uba_ubdrv_delete_jetty,
    .post_jetty_send_wr = uba_ubdrv_post_jetty_send_wr,
    .get_tp_list = uba_ubdrv_get_tp_list,
    .user_control = uba_ubdrv_user_control,
    .unimport_jfr = uba_ubdrv_unimport_jfr,
    .unimport_jetty = uba_ubdrv_unimport_jetty,
/* ubcore_put_device is available since openEuler-24.03-LTS-SP4, and currently only device-side trs module uses it.
 * Exclude on host to avoid driver dkms install failure on old OS that lacks this symbol.
 */
#ifndef DRV_HOST
    .put_device = uba_ubdrv_put_device,
#endif
    .get_jfc_opt_by_id = uba_ubdrv_get_jfc_opt_by_id,
    .get_jetty_opt_by_id = uba_ubdrv_get_jetty_opt_by_id,
    .get_device_by_eid = uba_ubdrv_get_device_by_eid,
};

STATIC struct uba_dev_ops g_ubcore_dev_ops = {
    .type = UBA_OPS_UBCORE,
    .ops.ubcore = &g_ubcore_ops,
};

struct uba_dev_ops *ubdrv_get_ubcore_dev_ops(void)
{
    return &g_ubcore_dev_ops;
}
