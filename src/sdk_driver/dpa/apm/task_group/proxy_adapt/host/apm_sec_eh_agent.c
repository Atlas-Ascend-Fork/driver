/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 * Author: huawei
 * Create: 2025-10-25
 */

#include "vmng_kernel_interface.h"

#include "apm_task_group_def.h"
#include "apm_msg.h"

#define APM_SEC_EH_AGENT_DEV_MAX_NUM 1124U
#define APM_SEC_EH_AGENT_NOTIFIER "apm_sec_eh_agent"

static int apm_sec_eh_notice_device_vmid(u32 chip_id, struct uda_mia_dev_para *mia_para, int vm_id)
{
    struct apm_msg_notice_host_os_id msg;
    int ret;

    apm_msg_fill_header(&msg.header, APM_MSG_TYPE_NOTICE_HOST_OS_ID);
    msg.phy_devid = mia_para->phy_devid;
    msg.sub_devid = mia_para->sub_devid;
    msg.host_os_id = vm_id;

    return apm_msg_send(chip_id, &msg.header, sizeof(msg));
}

static int apm_sec_eh_dev_init(u32 udevid)
{
    struct uda_mia_dev_para mia_para = {0};
    int vm_id = -1;
    int ret = 0;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if (ret != 0) {
        apm_err("Failed to get mia para. (udevid=%u)\n", udevid);
        return ret;
    }

    vm_id = vmngh_ctrl_get_vm_id(mia_para.phy_devid, mia_para.sub_devid + 1);
    if (vm_id < 0) {
        apm_err("Failed to get vm id. (udevid=%u; vm_id=%d)\n", udevid, vm_id);
        return -ENOSYS;
    }

    ret = apm_sec_eh_notice_device_vmid(mia_para.phy_devid, &mia_para, vm_id);
    if (ret != 0) {
        apm_err("Failed to notice device sec eh dev online. (udevid=%u; vm_id=%d)\n", udevid, vm_id);
        return ret;
    }

    return 0;
}

static void apm_sec_eh_dev_uninit(u32 udevid)
{
    struct uda_mia_dev_para mia_para = {0};
    int ret;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    if (ret != 0) {
        apm_warn("Failed to get mia para. (udevid=%u)\n", udevid);
    }

    ret = apm_sec_eh_notice_device_vmid(mia_para.phy_devid, &mia_para, -1);
    if (ret != 0) {
        apm_warn("Failed to notice device sec eh dev offline. (udevid=%u)\n", udevid);
    }
}

static int apm_sec_eh_agent_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= APM_SEC_EH_AGENT_DEV_MAX_NUM) {
        return -ERANGE;
    }

    if (action == UDA_INIT) {
        ret = apm_sec_eh_dev_init(udevid);
    } else if (action == UDA_UNINIT) {
        apm_sec_eh_dev_uninit(udevid);
    } else {
        /* do nothing */
    }

    return ret;
}

int apm_sec_eh_agent_init(void)
{
    struct uda_dev_type type;
    int ret;

    uda_dev_type_pack(&type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_REAL_SEC_EH);
    ret = uda_notifier_register(APM_SEC_EH_AGENT_NOTIFIER, &type, UDA_PRI0, apm_sec_eh_agent_notifier_func);
    if (ret != 0) {
        apm_err("Register sec eh agent uda notifier failed. (ret=%d)\n", ret);
        return ret;
    }

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(apm_sec_eh_agent_init, FEATURE_LOADER_STAGE_0);

void apm_sec_eh_agent_uninit(void)
{
    struct uda_dev_type type;

    uda_dev_type_pack(&type, UDA_DAVINCI, UDA_ENTITY, UDA_NEAR, UDA_REAL_SEC_EH);
    (void)uda_notifier_unregister(APM_SEC_EH_AGENT_NOTIFIER, &type);
}
DECLAER_FEATURE_AUTO_UNINIT(apm_sec_eh_agent_uninit, FEATURE_LOADER_STAGE_0);
