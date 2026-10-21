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
#ifndef EMU_ST
#include "ka_task_pub.h"
#include "ka_common_pub.h"
#include "ka_memory_pub.h"
#include "trs_pub_def.h"
#include "trs_ub_init_common.h"

#include "comm_kernel_interface.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_soc_res.h"
#include "uba/ubcore_adapt.h"
#if defined(CFG_FEATURE_TRS_SIA_ADAPT) && defined(CFG_FEATURE_HOST_ENV)
#include "trs_sia_adapt_auto_init.h"
#elif defined(CFG_FEATURE_TRS_SIA_AGENT)
#include "trs_sia_agent_auto_init.h"
#endif

static KA_TASK_DEFINE_RWLOCK(ub_dev_lock);
struct trs_ub_dev *g_trs_ub_dev[TRS_DEV_MAX_NUM] = {NULL};

static int trs_ub_get_eid_info(struct trs_ub_dev *ub_dev, uba_ubcore_eid_info *eid_info)
{
    struct devdrv_ub_dev_info dev_info;
    uba_ubcore_device *dev = ub_dev->ubc_dev;
    int ret, num;
    u32 i;

    ret = devdrv_get_ub_dev_info(ub_dev->devid, &dev_info, &num);
    if ((ret != 0) || (num == 0)) {
        trs_err("Failed to get ub dev_info. (ret=%u; devid=%u; num=%u)\n", ret, ub_dev->devid, num);
        return (ret != 0) ? ret : -ENODEV;
    }

    ka_task_spin_lock(&dev->eid_table.lock);
    if ((dev->eid_table.eid_entries == NULL) || (dev_info.eid_index[0] >= dev->eid_table.eid_cnt)) {
        ka_task_spin_unlock(&dev->eid_table.lock);
        trs_err("Invalid eid_table or eid_index. (devid=%u; eid_cnt=%u; eid_index=%u)\n", ub_dev->devid,
                dev->eid_table.eid_cnt, dev_info.eid_index[0]);
        return -EINVAL;
    }

    for (i = 0; i < dev->eid_table.eid_cnt; i++) {
        if (dev_info.eid_index[0] == dev->eid_table.eid_entries[i].eid_index) {
            eid_info->eid_index = dev_info.eid_index[0];
            eid_info->eid = dev->eid_table.eid_entries[i].eid;
            ka_task_spin_unlock(&dev->eid_table.lock);
            trs_info("Get eid success. (devid=%u; eid_index=%u)\n", ub_dev->devid, eid_info->eid_index);
            return 0;
        }
    }
    ka_task_spin_unlock(&dev->eid_table.lock);
    return -ENODEV;
}

int trs_ub_create_jetty(struct trs_ub_dev *ub_dev, u32 vfid)
{
    int ret = 0;

    ret = trs_ub_sq_jetty_init(ub_dev, vfid);
    if (ret != 0) {
        trs_err("Failed to init sq head jetty. (devid=%u)\n", ub_dev->devid);
        return ret;
    }

    ret = trs_ub_cq_jetty_init(ub_dev, vfid);
    if (ret != 0) {
        trs_ub_sq_jetty_uninit(ub_dev, vfid);
        trs_err("Failed to init cq jetty. (devid=%u)\n", ub_dev->devid);
        return ret;
    }

    return ret;
}

void trs_ub_destroy_jetty(struct trs_ub_dev *ub_dev, u32 vfid)
{
    trs_ub_cq_jetty_uninit(ub_dev, vfid);
    trs_ub_sq_jetty_uninit(ub_dev, vfid);
}

static struct trs_ub_dev *trs_ub_create_dev(u32 devid, u32 vfid)
{
    uba_ubcore_eid_info eid_info;
    struct trs_ub_dev *ub_dev = NULL;
    u32 token_val;
    int ret;

    ret = devdrv_get_token_val(devid, &token_val);
    if (ret != 0) {
        trs_err("Failed to get token value. (ret=%d; devid=%u)\n", ret, devid);
        return NULL;
    }

    ub_dev = (struct trs_ub_dev *)ka_mm_vzalloc(sizeof(struct trs_ub_dev));
    if (ub_dev == NULL) {
        trs_err("Failed to vzalloc ub_dev. (devid=%u)\n", devid);
        return NULL;
    }

    ub_dev->ubc_dev = trs_ub_get_ubcore_dev(devid);
    if (ub_dev->ubc_dev == NULL) {
        ka_mm_vfree(ub_dev);
        trs_err("Failed to get ubc_dev. (devid=%u)\n", devid);
        return NULL;
    }

    ub_dev->devid = devid;
    ret = trs_ub_get_eid_info(ub_dev, &eid_info);
    if (ret != 0) {
        ka_mm_vfree(ub_dev);
        trs_err("Failed to get eid_info. (ret=%d)\n", ret);
        return NULL;
    }
    ub_dev->vfid = vfid;
    ub_dev->sec_vfid = TRS_VF_MAX_NUM;
    ub_dev->token_val = token_val;
    ub_dev->jetty_info[vfid].devid = devid;
    ub_dev->eid_info = eid_info;
#ifdef TRS_HOST
    ka_task_rwlock_init(&ub_dev->rw_lock);
#endif

    return ub_dev;
}

static void trs_ub_destroy_dev(struct trs_ub_dev *ub_dev)
{
    ka_mm_vfree(ub_dev);
}

static void trs_ub_dev_release(struct kref_safe *ref)
{
    struct trs_ub_dev *ub_dev = ka_container_of(ref, struct trs_ub_dev, ref);
    trs_ub_destroy_dev(ub_dev);
}

void trs_put_ub_dev(struct trs_ub_dev *ub_dev)
{
    kref_safe_put(&ub_dev->ref, trs_ub_dev_release);
}

struct trs_ub_dev *trs_get_ub_dev(u32 devid)
{
    struct trs_ub_dev *ub_dev = NULL;
    if (devid >= TRS_DEV_MAX_NUM) {
        return NULL;
    }

    ka_task_read_lock_bh(&ub_dev_lock);
    ub_dev = g_trs_ub_dev[devid];
    if (ub_dev != NULL) {
        if (kref_safe_get_unless_zero(&ub_dev->ref) == 0) {
            ka_task_read_unlock_bh(&ub_dev_lock);
            return NULL;
        }
    }
    ka_task_read_unlock_bh(&ub_dev_lock);

    return ub_dev;
}

static int _trs_ub_dev_init(u32 devid, u32 vfid)
{
    struct trs_ub_dev *ub_dev = NULL;
    int ret;

    if (devdrv_get_connect_protocol(devid) != CONNECT_PROTOCOL_UB) {
        trs_info("Not ub-connect-protocol, break trs-ub-dev init process. (devid=%u)\n", devid);
        return 0;
    }

    if (vfid >= TRS_VF_MAX_NUM) {
        trs_err("Invalid vfid. (vfid=%u)\n", vfid);
        return -EINVAL;
    }

    if (g_trs_ub_dev[devid] != NULL) {
        trs_err("Repeat init ub dev. (devid=%u)\n", devid);
        return -EFAULT;
    }

    ub_dev = trs_ub_create_dev(devid, vfid);
    if (ub_dev == NULL) {
        trs_err("Create ub dev failed. (devid=%u)\n", devid);
        return -EFAULT;
    }

    ret = trs_ub_dev_adapt_init(ub_dev, vfid);
    if (ret != 0) {
        trs_ub_destroy_dev(ub_dev);
        return ret;
    }

    kref_safe_init(&ub_dev->ref);
    g_trs_ub_dev[devid] = ub_dev;

    trs_info("Trs ub dev online success. (devid=%u; vfid=%u)\n", devid, vfid);
    return ret;
}

static void _trs_ub_dev_uninit(u32 devid, u32 vfid)
{
    struct trs_ub_dev *ub_dev = g_trs_ub_dev[devid];

    ka_task_write_lock_bh(&ub_dev_lock);
    if (ub_dev == NULL) {
        ka_task_write_unlock_bh(&ub_dev_lock);
        return;
    }

    g_trs_ub_dev[devid] = NULL;
    ka_task_write_unlock_bh(&ub_dev_lock);
    trs_ub_dev_adapt_uninit(ub_dev, vfid);
    trs_put_ub_dev(ub_dev);

    trs_info("Trs ub dev offline success. (devid=%u)\n", devid);
}

int trs_ub_dev_init(u32 ts_inst_id)
{
    struct trs_id_inst inst;
    u32 vfgid, vfid = 0;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (inst.tsid > 0) {
        return 0;
    }

    if (uda_is_phy_dev(inst.devid) == false) {
        int ret = soc_resmng_dev_get_mia_base_info(inst.devid, &vfgid, &vfid);
        if (ret != 0) {
            trs_err("Failed to get vfid. (udevid=%u; ret=%d)\n", inst.devid, ret);
            return ret;
        }
    }

    return _trs_ub_dev_init(inst.devid, vfid);
}
DECLAER_FEATURE_AUTO_INIT_DEV(trs_ub_dev_init, FEATURE_LOADER_STAGE_3);

void trs_ub_dev_uninit(u32 ts_inst_id)
{
    struct trs_id_inst inst;
    u32 vfgid, vfid = 0;

    trs_ts_inst_to_id_inst(ts_inst_id, &inst);
    if (inst.tsid > 0) {
        return;
    }

    if (uda_is_phy_dev(inst.devid) == false) {
        int ret = soc_resmng_dev_get_mia_base_info(inst.devid, &vfgid, &vfid);
        if (ret != 0) {
            trs_err("Failed to get vfid. (udevid=%u; ret=%d)\n", inst.devid, ret);
            return;
        }
    }

    _trs_ub_dev_uninit(inst.devid, vfid);
}
DECLAER_FEATURE_AUTO_UNINIT_DEV(trs_ub_dev_uninit, FEATURE_LOADER_STAGE_3);
#else
void trs_ub_comm_stub(void)
{}
#endif
