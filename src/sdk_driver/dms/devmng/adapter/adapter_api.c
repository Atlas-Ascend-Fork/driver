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

#include "adapter_api.h"
#include "adpater_def.h"
#include "ka_kernel_def_pub.h"
#include "ka_memory_pub.h"
#include "ka_base_pub.h"
#include "dms_define.h"

void adap_dev_state_notifier_register(devdrv_dev_state_notify state_callback)
{
    static bool log_logged = false;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(0); /* use device 0 get connect type */
    if (adap->pcie.notifier_register != NULL) {
        adap->pcie.notifier_register(state_callback);
    } else if (log_logged == false) {
        log_logged = true;
        dms_err("Pcie.notifier_register is NULL.\n");
    }
    put_adapter(adap);
}
KA_EXPORT_SYMBOL(adap_dev_state_notifier_register);

void adap_dev_state_notifier_unregister(void)
{
    static bool log_logged = false;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(0); /* use device 0 get connect type */
    if (adap->pcie.notifier_unregister != NULL) {
        adap->pcie.notifier_unregister();
    } else if (log_logged == false) {
        log_logged = true;
        dms_err("Pcie.notifier_unregister is NULL.\n");
    }
    put_adapter(adap);
}
KA_EXPORT_SYMBOL(adap_dev_state_notifier_unregister);

/* p2p */
int adap_enable_p2p(int pid, u32 dev_id, u32 peer_dev_id)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(dev_id); /* use device 0 get connect type */
    if (adap->p2p.enable != NULL) {
        ret = adap->p2p.enable(pid, dev_id, peer_dev_id);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_enable_p2p);

int adap_disable_p2p(int pid, u32 dev_id, u32 peer_dev_id)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(dev_id); /* use device 0 get connect type */
    if (adap->p2p.disable != NULL) {
        ret = adap->p2p.disable(pid, dev_id, peer_dev_id);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_disable_p2p);

bool adap_is_p2p_enabled(u32 dev_id, u32 peer_dev_id)
{
    bool ret = false;
    static bool log_logged = false;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(dev_id); /* use device 0 get connect type */
    if (adap->p2p.is_enabled != NULL) {
        ret = adap->p2p.is_enabled(dev_id, peer_dev_id);
    } else if (log_logged == false) {
        log_logged = true;
        dms_err("P2p.is_enabled is NULL.\n");
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_is_p2p_enabled);

void adap_flush_p2p(int pid)
{
    static bool log_logged = false;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(0); /* use device 0 get connect type */
    if (adap->p2p.flush != NULL) {
        adap->p2p.flush(pid);
    } else if (log_logged == false) {
        log_logged = true;
        dms_err("P2p.flush is NULL.\n");
    }
    put_adapter(adap);
}
KA_EXPORT_SYMBOL(adap_flush_p2p);

int adap_get_p2p_capability(u32 dev_id, u64 *capability)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(dev_id); /* use device 0 get connect type */
    if (adap->p2p.getcapability != NULL) {
        ret = adap->p2p.getcapability(dev_id, capability);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_get_p2p_capability);

int adap_get_p2p_access_status(u32 devid, u32 peer_devid, int *status)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(devid); /* use device 0 get connect type */
    if (adap->p2p.get_access_status != NULL) {
        ret = adap->p2p.get_access_status(devid, peer_devid, status);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_get_p2p_access_status);

/* pcie */
int adap_get_pci_dev_info(u32 devid, struct devdrv_pci_dev_info *dev_info)
{
    int ret = 0;
    static bool log_logged = false;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(devid);
    if (adap->pcie.get_pci_dev_info != NULL) {
        ret = adap->pcie.get_pci_dev_info(devid, dev_info);
    } else if (log_logged == false) {
        log_logged = true;
        dms_err("Pcie.get_pci_dev_info is NULL.\n");
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_get_pci_dev_info);

int adap_get_pcie_id_info(u32 devid, struct devdrv_pcie_id_info *pcie_id_info)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(devid);
    if (adap->pcie.get_pcie_id_info != NULL) {
        ret = adap->pcie.get_pcie_id_info(devid, pcie_id_info);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_get_pcie_id_info);

int adap_pcie_read_proc(u32 dev_id, enum devdrv_addr_type type, u32 offset, unsigned char *value, u32 len)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(dev_id);
    if (adap->pcie.read_proc != NULL) {
        ret = adap->pcie.read_proc(dev_id, type, offset, value, len);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_pcie_read_proc);

int adap_pcie_reinit(u32 dev_id)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(dev_id);
    if (adap->pcie.reinit != NULL) {
        ret = adap->pcie.reinit(dev_id);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_pcie_reinit);

int adap_get_addr_info(u32 devid, enum devdrv_addr_type type, u32 index, u64 *addr, size_t *size)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(devid);
    if (adap->pcie.get_addr_info != NULL) {
        ret = adap->pcie.get_addr_info(devid, type, index, addr, size);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_get_addr_info);

int adap_get_bbox_reservd_mem(unsigned int devid, unsigned long long *dma_addr, ka_page_t **dma_pages,
                              unsigned int *len)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(devid);
    if (adap->pcie.get_bbox_reservd_mem != NULL) {
        ret = adap->pcie.get_bbox_reservd_mem(devid, dma_addr, dma_pages, len);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_get_bbox_reservd_mem);

int adap_get_hccs_link_status_and_group_id(u32 devid, u32 *hccs_status, u32 hccs_group_id[], u32 group_id_num)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(devid);
    if (adap->hccs.get_hccs_link_status_and_group_id != NULL) {
        ret = adap->hccs.get_hccs_link_status_and_group_id(devid, hccs_status, hccs_group_id, group_id_num);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_get_hccs_link_status_and_group_id);

int adap_register_black_callback(struct devdrv_black_callback *black_callback)
{
    int ret = 0;
    static bool log_logged = false;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(0); /* use device 0 get connect type */
    if (adap->pcie.register_black_callback != NULL) {
        ret = adap->pcie.register_black_callback(black_callback);
    } else if (log_logged == false) {
        log_logged = true;
        dms_err("Pcie.register_black_callback is NULL.\n");
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_register_black_callback);

void adap_unregister_black_callback(const struct devdrv_black_callback *black_callback)
{
    static bool log_logged = false;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(0); /* use device 0 get connect type */
    if (adap->pcie.unregister_black_callback != NULL) {
        adap->pcie.unregister_black_callback(black_callback);
    } else if (log_logged == false) {
        log_logged = true;
        dms_err("Pcie.unregister_black_callback is NULL.\n");
    }
    put_adapter(adap);
}
KA_EXPORT_SYMBOL(adap_unregister_black_callback);

int adap_set_module_init_finish(int dev_id, int module)
{
    int ret = 0;
    static bool log_logged = false;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(dev_id); /* use device 0 get connect type */
    if (adap->pcie.set_module_init_finish != NULL) {
        ret = adap->pcie.set_module_init_finish(dev_id, module);
    } else if (log_logged == false) {
        log_logged = true;
        dms_err("Pcie.set_module_init_finish is NULL.\n");
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_set_module_init_finish);

void adap_dev_startup_register(devdrv_dev_startup_notify startup_callback)
{
    static bool log_logged = false;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(0); /* use device 0 get connect type */
    if (adap->pcie.startup_register != NULL) {
        adap->pcie.startup_register(startup_callback);
    } else if (log_logged == false) {
        log_logged = true;
        dms_err("Pcie.startup_register is NULL.\n");
    }
    put_adapter(adap);
}
KA_EXPORT_SYMBOL(adap_dev_startup_register);

unsigned int adap_get_host_type(void)
{
    unsigned int ret = 0;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(0); /* use device 0 get connect type */
    if (adap->pcie.get_host_type != NULL) {
        ret = adap->pcie.get_host_type();
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_get_host_type);

int adap_get_master_devid_in_the_same_os(u32 dev_id, u32 *master_dev_id)
{
    int ret = 0;
    static bool log_logged = false;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(dev_id);
    if (adap->pcie.get_master_devid_in_the_same_os != NULL) {
        ret = adap->pcie.get_master_devid_in_the_same_os(dev_id, master_dev_id);
    } else {
        if (log_logged == false) {
            log_logged = true;
            dms_err("Pcie.get_master_devid_in_the_same_os is NULL.\n");
        }
        if (master_dev_id != NULL) {
            *master_dev_id = dev_id;
        } else {
            ret = -EINVAL;
        }
    }

    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_get_master_devid_in_the_same_os);

int adap_get_pci_enabled_vf_num(u32 dev_id, int *vf_num)
{
    int ret = -EOPNOTSUPP;
    struct bus_adpater_stu *adap = get_adapter_by_dev_id(dev_id);
    if (adap->pcie.get_pci_enabled_vf_num != NULL) {
        ret = adap->pcie.get_pci_enabled_vf_num(dev_id, vf_num);
    }
    put_adapter(adap);
    return ret;
}
KA_EXPORT_SYMBOL(adap_get_pci_enabled_vf_num);