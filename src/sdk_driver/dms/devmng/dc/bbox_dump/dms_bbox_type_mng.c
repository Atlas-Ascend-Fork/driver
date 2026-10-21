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
#include "ka_errno_pub.h"
#include "dms_bbox_type_mng.h"
#include "pbl/pbl_feature_loader.h"
#include "dms_template.h"
#include "dms/dms_cmd_def.h"
#include "devdrv_user_common.h"
#include "urd_acc_ctrl.h"
#include "comm_kernel_interface.h"
#include "pbl/pbl_soc_res.h"
#include "devdrv_manager.h"
#include "pbl_uda.h"
#include "devdrv_pcie.h"
#include "comm_pcie.h"
#include "dms_bbox_dump.h"

#define BBOX_UB_DEFAULT_PIN 0U
#define BBOX_UB_DEFAULT_NON_PIN 1U

STATIC struct bbox_ub_dump_ctx ub_dump_ctx_mng[ASCEND_DEV_MAX_NUM][MEM_CTRL_TYPE_MAX] = {0};

struct bbox_ub_dump_ctx *bbox_get_ub_dump_ctx(u32 dev_id, u32 type)
{
    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (type >= MEM_CTRL_TYPE_MAX)) {
        dms_err("Invalid dev_id or type. (dev_id=%u; type=%u)\n", dev_id, type);
        return NULL;
    }

    return &ub_dump_ctx_mng[dev_id][type];
}

void bbox_ub_dump_ctx_init(void)
{
    u32 i, j;
    struct bbox_ub_dump_ctx *ctx = NULL;

    for (i = 0; i < ASCEND_DEV_MAX_NUM; i++) {
        for (j = 0; j < MEM_CTRL_TYPE_MAX; j++) {
            ctx = bbox_get_ub_dump_ctx(i, j);
            ka_task_init_rwsem(&ctx->sem);
        }
    }
}

struct ubcore_device *bbox_get_ubcore_dev(u32 dev_id)
{
    ka_device_t *uda_dev;

#ifdef DRV_HOST
    uda_dev = uda_get_device(dev_id);
#else
    uda_dev = uda_get_agent_device(dev_id);
#endif
    if (uda_dev == NULL) {
        dms_err("Failed to get ub dev. (devid=%u)\n", dev_id);
        return NULL;
    }

    return ka_container_of(uda_dev, struct ubcore_device, dev);
}

int bbox_init_ub_segment(struct ubcore_device *ubc_dev, u32 dev_id, u32 type, u64 va, u32 len, u32 token_val)
{
    struct bbox_ub_dump_ctx *ub_dump_ctx = bbox_get_ub_dump_ctx(dev_id, type);
    union ubcore_reg_seg_flag flag = {0};
    struct ubcore_seg_cfg seg_cfg = {0};

    flag.bs.token_policy = UBCORE_TOKEN_PLAIN_TEXT;
    flag.bs.access = UBCORE_ACCESS_READ;
#ifdef DRV_HOST
    flag.bs.non_pin = BBOX_UB_DEFAULT_PIN;
#else
    flag.bs.non_pin = BBOX_UB_DEFAULT_NON_PIN;
#endif
    seg_cfg.va = va;
    seg_cfg.len = len;
    seg_cfg.flag = flag;
    seg_cfg.token_value.token = token_val;

    ub_dump_ctx->target_seg = ubcore_register_seg(ubc_dev, &seg_cfg, NULL);
    if (KA_IS_ERR_OR_NULL(ub_dump_ctx->target_seg)) {
        dms_err("Failed to register segment. (devid=%u, type=%u, size=0x%x)\n", dev_id, type, len);
        return -EFAULT;
    }

    dms_info("Register local segment successfully. (devid=%u; type=%u; size=0x%x)\n", dev_id, type, len);

    return 0;
}

void bbox_uninit_ub_segment(struct bbox_ub_dump_ctx *ctx)
{
    if (!KA_IS_ERR_OR_NULL(ctx->target_seg)) {
        ubcore_unregister_seg(ctx->target_seg);
        ctx->target_seg = NULL;
    }
}

int bbox_import_ub_segment(struct ubcore_device *ubc_dev, struct bbox_ub_dump_ctx *ctx, struct ubcore_seg *seg,
                           u32 rmt_token_value)
{
    struct ubcore_target_seg_cfg seg_cfg = {0};
    seg_cfg.seg.attr.bs.token_policy = UBCORE_TOKEN_PLAIN_TEXT;
    seg_cfg.token_value.token = rmt_token_value;
    seg_cfg.seg = *seg;
    ctx->remote_seg = ubcore_import_seg(ubc_dev, &seg_cfg, NULL);
    if (KA_IS_ERR_OR_NULL(ctx->remote_seg)) {
        dms_err("Failed to import segment. \n");
        return -EFAULT;
    }

    return 0;
}
void bbox_unimport_ub_segment(struct bbox_ub_dump_ctx *ctx)
{
    if (!KA_IS_ERR_OR_NULL(ctx->remote_seg)) {
        (void)ubcore_unimport_seg(ctx->remote_seg);
        ctx->remote_seg = NULL;
    }
}

STATIC int bbox_ub_get_dump_ctx_para_check(struct bbox_ub_dump_ctx *ctx, u32 len)
{
    if ((ctx->va == NULL) || (KA_IS_ERR_OR_NULL(ctx->target_seg)) || (KA_IS_ERR_OR_NULL(ctx->remote_seg))) {
        dms_err("Unmapped addr or seg. (target_seg_is_null=%d; remote_seg_is_null=%d)\n",
                (int)(KA_IS_ERR_OR_NULL(ctx->target_seg)), (int)(KA_IS_ERR_OR_NULL(ctx->remote_seg)));
        return -EINVAL;
    }

    if (len > ctx->size) {
        dms_err("Unmapped region or invalid para. (len=0x%x; ctx_size=0x%x)\n", len, ctx->size);
        return -EINVAL;
    }
    return 0;
}

STATIC void bbox_pack_copy_info(struct bbox_ub_dump_ctx *ctx, u32 offset, u32 len, struct devdrv_urma_copy *local,
                                struct devdrv_urma_copy *peer)
{
    local->seg = ctx->target_seg;
    local->len = (u64)len;
    local->offset = 0;
    peer->seg = ctx->remote_seg;
    peer->len = (u64)len;
    peer->offset = offset;
}

int bbox_ub_get_data(u32 dev_id, u32 type, u32 offset, u32 len, void *buf)
{
    struct bbox_ub_dump_ctx *ub_dump_ctx = bbox_get_ub_dump_ctx(dev_id, type);
    struct devdrv_urma_copy local, peer;
    int ret;

    ka_task_down_read(&ub_dump_ctx->sem);
    ret = bbox_ub_get_dump_ctx_para_check(ub_dump_ctx, len);
    if (ret != 0) {
        ka_task_up_read(&ub_dump_ctx->sem);
        return ret;
    }

    bbox_pack_copy_info(ub_dump_ctx, offset, len, &local, &peer);
    ret = devdrv_urma_copy(dev_id, URMA_CHAN_BBOX, PEER_TO_LOCAL, &local, &peer);
    if (ret != 0) {
        ka_task_up_read(&ub_dump_ctx->sem);
        dms_err("Failed to copy with urma. (ret=%d; dev_id=%u; len=0x%x; offset=0x%x)\n", ret, dev_id, len, offset);
        return ret;
    }

    if (copy_to_user_safe(buf, (u8 *)ub_dump_ctx->va, len) != 0) {
        ka_task_up_read(&ub_dump_ctx->sem);
        dms_err("Copy to user failed. (dev_id=%u; len=0x%x; offset=0x%x)\n", dev_id, len, offset);
        return -EFAULT;
    }
    ka_task_up_read(&ub_dump_ctx->sem);
    return 0;
}
