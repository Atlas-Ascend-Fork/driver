/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "pbl/pbl_uda.h"
#include "apm_master_domain.h"
#include "apm_slab.h"
#include "apm_kern_log.h"
#include "apm_dev.h"

static bool *g_apm_dev_host_reset = NULL;

bool apm_dev_is_host_reset(u32 udevid)
{
    if ((g_apm_dev_host_reset == NULL) || (udevid >= uda_get_udev_max_num())) {
        return true;
    }

    return g_apm_dev_host_reset[udevid];
}

static int apm_uda_notifier(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= uda_get_udev_max_num()) {
        apm_err("Invalid para. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    if (action == UDA_INIT) {
        g_apm_dev_host_reset[udevid] = false;
    } else if ((action == UDA_PRE_HOTRESET) || (action == UDA_HOTRESET)) {
        ret = apm_master_domain_check_all_master_is_bind_slave(udevid);
        if (ret != 0) {
            apm_warn("Proc still running, stop host reset. (udevid=%u)\n", udevid);
            return ret;
        }

        g_apm_dev_host_reset[udevid] = true;
    } else if ((action == UDA_PRE_HOTRESET_CANCEL) || (action == UDA_HOTRESET_CANCEL)) {
        g_apm_dev_host_reset[udevid] = false;
    }

    apm_info("notifier action. (udevid=%u; action=%d; ret=%d)\n", udevid, action, ret);
    return ret;
}

int apm_dev_init(void)
{
    struct uda_dev_type type;
    int ret = 0;

    g_apm_dev_host_reset = apm_vzalloc(sizeof(bool) * uda_get_udev_max_num());
    if (g_apm_dev_host_reset == NULL) {
        apm_err("Vmalloc array fail. (size=%lu)\n", sizeof(bool) * uda_get_udev_max_num());
        return -ENOMEM;
    }

    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_real_virtual_notifier_register("apm_host", &type, UDA_PRI2, apm_uda_notifier);
    if (ret != 0) {
        apm_vfree(g_apm_dev_host_reset);
        g_apm_dev_host_reset = NULL;
        apm_err("Uda register host notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(apm_dev_init, FEATURE_LOADER_STAGE_2);

void apm_dev_uninit(void)
{
    struct uda_dev_type type;
    int ret = 0;

    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_real_virtual_notifier_unregister("apm_host", &type);
    if (ret != 0) {
        apm_warn("Uda unregister host notifier warn. (ret=%d)\n", ret);
        return;
    }

    apm_vfree(g_apm_dev_host_reset);
    g_apm_dev_host_reset = NULL;
}
DECLAER_FEATURE_AUTO_UNINIT(apm_dev_uninit, FEATURE_LOADER_STAGE_2);
