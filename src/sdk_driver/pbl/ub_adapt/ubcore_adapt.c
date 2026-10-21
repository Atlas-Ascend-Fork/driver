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

#include "ubcore_adapt.h"
#include "adapt_ops.h"
#include "ka_kernel_def_pub.h"

uba_ubcore_jfc *uba_ubcore_create_jfc(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_jfc_cfg *cfg,
                                      uba_ubcore_comp_callback_t jfce_handler, uba_ubcore_event_callback_t jfae_handler,
                                      uba_ubcore_udata *udata)
{
    uba_ubcore_jfc *jfc = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->create_jfc == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    jfc = dev_ops->ops.ubcore->create_jfc(dev, cfg, jfce_handler, jfae_handler, udata);
    UBA_ADD_MODULE_ON_SUCC(jfc != NULL, module_type, UBA_FUNC_CREATE_JFC);
    uba_sub_adapt_ops_ref(dev_ops);
    return jfc;
}
KA_EXPORT_SYMBOL(uba_ubcore_create_jfc);

int uba_ubcore_delete_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->delete_jfc == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->delete_jfc(jfc);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_DELETE_JFC);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_delete_jfc);

uba_ubcore_jfr *uba_ubcore_create_jfr(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_jfr_cfg *cfg,
                                      uba_ubcore_event_callback_t jfae_handler, uba_ubcore_udata *udata)
{
    uba_ubcore_jfr *jfr = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->create_jfr == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    jfr = dev_ops->ops.ubcore->create_jfr(dev, cfg, jfae_handler, udata);
    UBA_ADD_MODULE_ON_SUCC(jfr != NULL, module_type, UBA_FUNC_CREATE_JFR);
    uba_sub_adapt_ops_ref(dev_ops);
    return jfr;
}
KA_EXPORT_SYMBOL(uba_ubcore_create_jfr);

int uba_ubcore_delete_jfr(enum uba_module_type module_type, uba_ubcore_jfr *jfr)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->delete_jfr == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->delete_jfr(jfr);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_DELETE_JFR);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_delete_jfr);

int uba_ubcore_post_jfr_wr(enum uba_module_type module_type, uba_ubcore_jfr *jfr, uba_ubcore_jfr_wr *wr,
                           uba_ubcore_jfr_wr **bad_wr)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->post_jfr_wr == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->post_jfr_wr(jfr, wr, bad_wr);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_POST_JFR_WR);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_post_jfr_wr);

int uba_ubcore_poll_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc, int cr_cnt, uba_ubcore_cr *cr)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->poll_jfc == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->poll_jfc(jfc, cr_cnt, cr);
    UBA_ADD_MODULE_ON_SUCC(ret >= 0, module_type, UBA_FUNC_POLL_JFC);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_poll_jfc);

int uba_ubcore_rearm_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc, bool solicited_only)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->rearm_jfc == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->rearm_jfc(jfc, solicited_only);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_REARM_JFC);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_rearm_jfc);

uba_ubcore_target_seg *uba_ubcore_register_seg(enum uba_module_type module_type, uba_ubcore_device *dev,
                                               uba_ubcore_seg_cfg *cfg, uba_ubcore_udata *udata)
{
    uba_ubcore_target_seg *seg = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->register_seg == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    seg = dev_ops->ops.ubcore->register_seg(dev, cfg, udata);
    UBA_ADD_MODULE_ON_SUCC(seg != NULL, module_type, UBA_FUNC_REGISTER_SEG);
    uba_sub_adapt_ops_ref(dev_ops);
    return seg;
}
KA_EXPORT_SYMBOL(uba_ubcore_register_seg);

int uba_ubcore_unregister_seg(enum uba_module_type module_type, uba_ubcore_target_seg *tseg)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->unregister_seg == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->unregister_seg(tseg);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_UNREGISTER_SEG);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_unregister_seg);

uba_ubcore_target_seg *uba_ubcore_import_seg(enum uba_module_type module_type, uba_ubcore_device *dev,
                                             uba_ubcore_target_seg_cfg *cfg, uba_ubcore_udata *udata)
{
    uba_ubcore_target_seg *seg = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->import_seg == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    seg = dev_ops->ops.ubcore->import_seg(dev, cfg, udata);
    UBA_ADD_MODULE_ON_SUCC(seg != NULL, module_type, UBA_FUNC_IMPORT_SEG);
    uba_sub_adapt_ops_ref(dev_ops);
    return seg;
}
KA_EXPORT_SYMBOL(uba_ubcore_import_seg);

int uba_ubcore_unimport_seg(enum uba_module_type module_type, uba_ubcore_target_seg *tseg)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->unimport_seg == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->unimport_seg(tseg);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_UNIMPORT_SEG);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_unimport_seg);

int uba_ubcore_alloc_jfc(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_jfc_cfg *cfg,
                         uba_ubcore_comp_callback_t jfce_handler, uba_ubcore_event_callback_t jfae_handler,
                         uba_ubcore_jfc **jfc, uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->alloc_jfc == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->alloc_jfc(dev, cfg, jfce_handler, jfae_handler, jfc, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_ALLOC_JFC);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_alloc_jfc);

int uba_ubcore_set_jfc_opt(enum uba_module_type module_type, uba_ubcore_jfc *jfc, uint64_t opt, void *buf, uint32_t len,
                           uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->set_jfc_opt == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->set_jfc_opt(jfc, opt, buf, len, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_SET_JFC_OPT);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_set_jfc_opt);

int uba_ubcore_active_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc, uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->active_jfc == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->active_jfc(jfc, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_ACTIVE_JFC);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_active_jfc);

int uba_ubcore_deactive_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc, uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->deactive_jfc == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->deactive_jfc(jfc, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_DEACTIVE_JFC);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_deactive_jfc);

int uba_ubcore_free_jfc(enum uba_module_type module_type, uba_ubcore_jfc *jfc, uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->free_jfc == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->free_jfc(jfc, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_FREE_JFC);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_free_jfc);

int uba_ubcore_alloc_jfs(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_jfs_cfg *cfg,
                         uba_ubcore_event_callback_t jfae_handler, uba_ubcore_jfs **jfs, uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->alloc_jfs == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->alloc_jfs(dev, cfg, jfae_handler, jfs, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_ALLOC_JFS);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_alloc_jfs);

int uba_ubcore_set_jfs_opt(enum uba_module_type module_type, uba_ubcore_jfs *jfs, uint64_t opt, void *buf, uint32_t len,
                           uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->set_jfs_opt == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->set_jfs_opt(jfs, opt, buf, len, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_SET_JFS_OPT);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_set_jfs_opt);

int uba_ubcore_active_jfs(enum uba_module_type module_type, uba_ubcore_jfs *jfs, uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->active_jfs == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->active_jfs(jfs, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_ACTIVE_JFS);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_active_jfs);

int uba_ubcore_deactive_jfs(enum uba_module_type module_type, uba_ubcore_jfs *jfs, uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->deactive_jfs == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->deactive_jfs(jfs, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_DEACTIVE_JFS);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_deactive_jfs);

int uba_ubcore_free_jfs(enum uba_module_type module_type, uba_ubcore_jfs *jfs, uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->free_jfs == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->free_jfs(jfs, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_FREE_JFS);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_free_jfs);

uba_ubcore_tjetty *uba_ubcore_import_jfr_ex(enum uba_module_type module_type, uba_ubcore_device *dev,
                                            uba_ubcore_tjetty_cfg *cfg, uba_ubcore_active_tp_cfg *active_tp_cfg,
                                            uba_ubcore_udata *udata)
{
    uba_ubcore_tjetty *tjfr = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->import_jfr_ex == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    tjfr = dev_ops->ops.ubcore->import_jfr_ex(dev, cfg, active_tp_cfg, udata);
    UBA_ADD_MODULE_ON_SUCC(tjfr != NULL, module_type, UBA_FUNC_IMPORT_JFR_EX);
    uba_sub_adapt_ops_ref(dev_ops);
    return tjfr;
}
KA_EXPORT_SYMBOL(uba_ubcore_import_jfr_ex);

int uba_ubcore_get_tp_list(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_get_tp_cfg *cfg,
                           uint32_t *tp_cnt, uba_ubcore_tp_info *tp_list, uba_ubcore_udata *udata)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->get_tp_list == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->get_tp_list(dev, cfg, tp_cnt, tp_list, udata);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_GET_TP_LIST);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_get_tp_list);

int uba_ubcore_user_control(enum uba_module_type module_type, uba_ubcore_device *dev, uba_ubcore_user_ctl *k_user_ctl)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->user_control == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->user_control(dev, k_user_ctl);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_USER_CONTROL);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_user_control);

uba_ubcore_tjetty *uba_ubcore_import_jetty_ex(enum uba_module_type module_type, uba_ubcore_device *dev,
                                              uba_ubcore_tjetty_cfg *cfg, uba_ubcore_active_tp_cfg *active_tp_cfg,
                                              uba_ubcore_udata *udata)
{
    uba_ubcore_tjetty *tjfr = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->import_jetty_ex == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    tjfr = dev_ops->ops.ubcore->import_jetty_ex(dev, cfg, active_tp_cfg, udata);
    UBA_ADD_MODULE_ON_SUCC(tjfr != NULL, module_type, UBA_FUNC_IMPORT_JETTY_EX);
    uba_sub_adapt_ops_ref(dev_ops);
    return tjfr;
}
KA_EXPORT_SYMBOL(uba_ubcore_import_jetty_ex);

uba_ubcore_tjetty *uba_ubcore_import_jfr(enum uba_module_type module_type, uba_ubcore_device *dev,
                                         uba_ubcore_tjetty_cfg *cfg, uba_ubcore_udata *udata)
{
    uba_ubcore_tjetty *tjfr = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->import_jfr == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    tjfr = dev_ops->ops.ubcore->import_jfr(dev, cfg, udata);
    UBA_ADD_MODULE_ON_SUCC(tjfr != NULL, module_type, UBA_FUNC_IMPORT_JFR);
    uba_sub_adapt_ops_ref(dev_ops);
    return tjfr;
}
KA_EXPORT_SYMBOL(uba_ubcore_import_jfr);

int uba_ubcore_unimport_jfr(enum uba_module_type module_type, uba_ubcore_tjetty *tjfr)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->unimport_jfr == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->unimport_jfr(tjfr);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_UNIMPORT_JFR);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_unimport_jfr);

int uba_ubcore_unimport_jetty(enum uba_module_type module_type, uba_ubcore_tjetty *tjfr)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->unimport_jetty == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->unimport_jetty(tjfr);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_UNIMPORT_JETTY);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_unimport_jetty);

int uba_ubcore_delete_jetty(enum uba_module_type module_type, uba_ubcore_jetty *jetty)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->delete_jetty == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->delete_jetty(jetty);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_DELETE_JETTY);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_delete_jetty);

int uba_ubcore_post_jetty_send_wr(enum uba_module_type module_type, uba_ubcore_jetty *jetty, uba_ubcore_jfs_wr *wr,
                                  uba_ubcore_jfs_wr **bad_wr)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->post_jetty_send_wr == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->post_jetty_send_wr(jetty, wr, bad_wr);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_POST_JETTY_SEND_WR);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_post_jetty_send_wr);

uba_ubcore_jetty *uba_ubcore_create_jetty(enum uba_module_type module_type, uba_ubcore_device *dev,
                                          uba_ubcore_jetty_cfg *cfg, uba_ubcore_event_callback_t jfae_handler,
                                          uba_ubcore_udata *udata)
{
    uba_ubcore_jetty *jetty = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->create_jetty == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    jetty = dev_ops->ops.ubcore->create_jetty(dev, cfg, jfae_handler, udata);
    UBA_ADD_MODULE_ON_SUCC(jetty != NULL, module_type, UBA_FUNC_CREATE_JETTY);
    uba_sub_adapt_ops_ref(dev_ops);
    return jetty;
}
KA_EXPORT_SYMBOL(uba_ubcore_create_jetty);

uba_ubcore_tjetty *uba_ubcore_import_jetty(enum uba_module_type module_type, uba_ubcore_device *dev,
                                           uba_ubcore_tjetty_cfg *cfg, uba_ubcore_udata *udata)
{
    uba_ubcore_tjetty *tjfr = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->import_jetty == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    tjfr = dev_ops->ops.ubcore->import_jetty(dev, cfg, udata);
    UBA_ADD_MODULE_ON_SUCC(tjfr != NULL, module_type, UBA_FUNC_IMPORT_JETTY);
    uba_sub_adapt_ops_ref(dev_ops);
    return tjfr;
}
KA_EXPORT_SYMBOL(uba_ubcore_import_jetty);

void uba_ubcore_put_device(enum uba_module_type module_type, uba_ubcore_device *dev)
{
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->put_device == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return;
    }

    dev_ops->ops.ubcore->put_device(dev);
    UBA_ADD_MODULE_ON_SUCC(true, module_type, UBA_FUNC_PUT_DEVICE);
    uba_sub_adapt_ops_ref(dev_ops);
}
KA_EXPORT_SYMBOL(uba_ubcore_put_device);

int uba_ubcore_get_jfc_opt_by_id(enum uba_module_type module_type, uba_ubcore_device *dev, uint32_t jfc_id,
                                 uint64_t opt, void *buf, uint32_t len)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->get_jfc_opt_by_id == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->get_jfc_opt_by_id(dev, jfc_id, opt, buf, len);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_GET_JFC_OPT_BY_ID);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_get_jfc_opt_by_id);

int uba_ubcore_get_jetty_opt_by_id(enum uba_module_type module_type, uba_ubcore_device *dev, uint32_t jetty_id,
                                   uint64_t opt, void *buf, uint32_t len)
{
    int ret = -EOPNOTSUPP;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return ret;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->get_jetty_opt_by_id == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return ret;
    }

    ret = dev_ops->ops.ubcore->get_jetty_opt_by_id(dev, jetty_id, opt, buf, len);
    UBA_ADD_MODULE_ON_SUCC(ret == 0, module_type, UBA_FUNC_GET_JETTY_OPT_BY_ID);
    uba_sub_adapt_ops_ref(dev_ops);
    return ret;
}
KA_EXPORT_SYMBOL(uba_ubcore_get_jetty_opt_by_id);

uba_ubcore_device *uba_ubcore_get_device_by_eid(enum uba_module_type module_type, uba_ubcore_eid *eid,
                                                uba_ubcore_transport_type type)
{
    uba_ubcore_device *dev = NULL;
    struct uba_dev_ops *dev_ops = uba_add_adapt_ops_ref(UBA_OPS_UBCORE);

    if (dev_ops == NULL) {
        return NULL;
    }
    if ((dev_ops->ops.ubcore == NULL) || (dev_ops->ops.ubcore->get_device_by_eid == NULL)) {
        uba_sub_adapt_ops_ref(dev_ops);
        return NULL;
    }

    dev = dev_ops->ops.ubcore->get_device_by_eid(eid, type);
    UBA_ADD_MODULE_ON_SUCC(dev != NULL, module_type, UBA_FUNC_GET_DEVICE_BY_EID);
    uba_sub_adapt_ops_ref(dev_ops);
    return dev;
}
KA_EXPORT_SYMBOL(uba_ubcore_get_device_by_eid);
