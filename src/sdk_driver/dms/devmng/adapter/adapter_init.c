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
#include "ka_pci_pub.h"
#include "ka_common_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"
#include "ka_base_pub.h"
#include "ka_dfx_pub.h"
#include "adapter_api.h"
#include "adpater_def.h"

#ifdef CFG_HOST_ENV
#define PCIE_KO_NAME "drv_pcie_host"
#else
#define PCIE_KO_NAME "drv_pcie"
#endif
#define UB_KO_NAME "asdrv_ub"

#define PCI_VENDOR_ID_HUAWEI 0x19e5
#define DEVDRV_DIVERSITY_PCIE_VENDOR_ID 0xFFFF

struct bus_adpater_stu g_pcie_adpat = {0};
struct bus_adpater_stu g_ubbus_adpat = {0};

struct symbol_list pcie_ops[] = {
    /* p2p ops */
    {"devdrv_enable_p2p", (ka_offsetof(struct bus_adpater_stu, p2p) + ka_offsetof(struct p2p_ops_stu, enable))},
    {"devdrv_disable_p2p", (ka_offsetof(struct bus_adpater_stu, p2p) + ka_offsetof(struct p2p_ops_stu, disable))},
    {"devdrv_is_p2p_enabled", (ka_offsetof(struct bus_adpater_stu, p2p) + ka_offsetof(struct p2p_ops_stu, is_enabled))},
    {"devdrv_flush_p2p", (ka_offsetof(struct bus_adpater_stu, p2p) + ka_offsetof(struct p2p_ops_stu, flush))},
    {"devdrv_get_p2p_capability",
     (ka_offsetof(struct bus_adpater_stu, p2p) + ka_offsetof(struct p2p_ops_stu, getcapability))},
    {"devdrv_get_p2p_access_status",
     (ka_offsetof(struct bus_adpater_stu, p2p) + ka_offsetof(struct p2p_ops_stu, get_access_status))},

    /* hccs ops */
    {"devdrv_get_hccs_link_status_and_group_id",
     (ka_offsetof(struct bus_adpater_stu, hccs) + ka_offsetof(struct hccs_ops_stu, get_hccs_link_status_and_group_id))},

    /* pcie ops */
    {"devdrv_get_pci_dev_info",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, get_pci_dev_info))},
    {"devdrv_get_pcie_id_info",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, get_pcie_id_info))},
    {"devdrv_pcie_read_proc",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, read_proc))},
    {"devdrv_get_addr_info",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, get_addr_info))},
    {"devdrv_get_bbox_reservd_mem",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, get_bbox_reservd_mem))},
    {"devdrv_register_black_callback",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, register_black_callback))},
    {"devdrv_unregister_black_callback",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, unregister_black_callback))},
    {"devdrv_dev_state_notifier_unregister",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, notifier_unregister))},
    {"drvdrv_dev_state_notifier_register",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, notifier_register))},
    {"devdrv_set_module_init_finish",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, set_module_init_finish))},
    {"drvdrv_dev_startup_register",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, startup_register))},
    {"devdrv_get_host_type",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, get_host_type))},
    {"devdrv_get_master_devid_in_the_same_os",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, get_master_devid_in_the_same_os))},
    {"devdrv_pcie_reinit", (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, reinit))},
    {"devdrv_get_pci_enabled_vf_num",
     (ka_offsetof(struct bus_adpater_stu, pcie) + ka_offsetof(struct pcie_ops_stu, get_pci_enabled_vf_num))},
};

struct symbol_list ubbus_ops[] = {
    /* dma ops */
    {"ubdrv_flush_p2p", (ka_offsetof(struct bus_adpater_stu, p2p) + ka_offsetof(struct p2p_ops_stu, flush))},
};

struct module_adapter_cb adap_cb[] = {
    {PCIE_KO_NAME, pcie_ops, (sizeof(pcie_ops) / sizeof(struct symbol_list)), &g_pcie_adpat},
    {UB_KO_NAME, ubbus_ops, (sizeof(ubbus_ops) / sizeof(struct symbol_list)), &g_ubbus_adpat},
};

static const ka_pci_device_id_t devdrv_driver_tbl[] = {
    {KA_PCI_VDEVICE(HUAWEI, 0xd806), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd807), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd808), 0},
    {DEVDRV_DIVERSITY_PCIE_VENDOR_ID, 0xd500, KA_PCI_ANY_ID, KA_PCI_ANY_ID, 0, 0, 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd809), 0},
    {KA_PCI_VDEVICE(HUAWEI, 0xd200), 0},
    {}};
KA_MODULE_DEVICE_TABLE(pci, devdrv_driver_tbl);

static int get_adapter_connect_protocol(unsigned int dev_id)
{
#ifdef CFG_HOST_ENV
    (void)dev_id;
    /* Avoiding the issue of using dev_id=0 to obtain the connection protocol on the host side */
#ifdef CFG_FEATURE_UB
    return CONNECT_PROTOCOL_UB;
#else
    return CONNECT_PROTOCOL_PCIE;
#endif
#else
    return devdrv_get_connect_protocol(dev_id);
#endif
}

struct bus_adpater_stu *get_adapter_by_dev_id(unsigned int dev_id)
{
    struct bus_adpater_stu *adap[] = {&g_pcie_adpat, &g_ubbus_adpat};
    int connect_type = CONNECT_PROTOCOL_UNKNOWN;

    connect_type = get_adapter_connect_protocol(dev_id);
    if ((connect_type == CONNECT_PROTOCOL_PCIE) || (connect_type == CONNECT_PROTOCOL_HCCS)) {
        ka_task_down_read(&adap[ADAPTER_PCIE_BUS]->rw_lock);
        return adap[ADAPTER_PCIE_BUS];
    } else {
        ka_task_down_read(&adap[ADAPTER_UB_BUS]->rw_lock);
        return adap[ADAPTER_UB_BUS];
    }
}

struct bus_adpater_stu *get_adapter_by_module(unsigned int index)
{
    struct bus_adpater_stu *adap[] = {&g_pcie_adpat, &g_ubbus_adpat};
    if (index > ADAPTER_UB_BUS) {
        ka_task_down_read(&adap[0]->rw_lock);
        return adap[0];
    }
    ka_task_down_read(&adap[index]->rw_lock);
    return adap[index];
}

void put_adapter(struct bus_adpater_stu *adap)
{
    ka_task_up_read(&adap->rw_lock);
}

static void init_module_function(ka_module_t *mod)
{
#ifdef CFG_HOST_ENV
    int i;
    for (i = 0; i < (sizeof(adap_cb) / sizeof(struct module_adapter_cb)); i++) {
        if (ka_base_strcmp(mod->name, adap_cb[i].mod_name) != 0) {
            continue;
        }
        init_module_func(mod, adap_cb[i].sym_list, adap_cb[i].sym_count, adap_cb[i].adap);
    }
#else
    (void)mod;
#endif
}

static void uninit_module_function(ka_module_t *mod)
{
#ifdef CFG_HOST_ENV
    int i;
    for (i = 0; i < (sizeof(adap_cb) / sizeof(struct module_adapter_cb)); i++) {
        if (ka_base_strcmp(mod->name, adap_cb[i].mod_name) != 0) {
            continue;
        }
        uninit_module_func(adap_cb[i].sym_list, adap_cb[i].sym_count, adap_cb[i].adap);
    }
#else
    (void)mod;
#endif
}

static int adapter_module_callback(ka_notifier_block_t *nb, unsigned long val, void *data)
{
    ka_module_t *mod = data;
    if (mod == NULL) {
        return KA_NOTIFY_DONE;
    }
    switch (val) {
        case MODULE_STATE_GOING:
            uninit_module_function(mod);
            break;
        case MODULE_STATE_LIVE:
            init_module_function(mod);
            break;
        default:
            break;
    }
    return KA_NOTIFY_DONE;
}
static ka_notifier_block_t g_adapter_module_nb = {.notifier_call = adapter_module_callback, .priority = 0};

int KA_MODULE_INIT ascend_adapter_init(void)
{
    int err, i;
    for (i = 0; i < (sizeof(adap_cb) / sizeof(struct module_adapter_cb)); i++) {
        ka_task_init_rwsem(&adap_cb[i].adap->rw_lock);
    }
    err = ka_dfx_register_module_notifier(&g_adapter_module_nb);
    return err;
}

void KA_MODULE_EXIT ascend_adapter_exit(void)
{
    ka_dfx_unregister_module_notifier(&g_adapter_module_nb);
    return;
}

ka_module_init(ascend_adapter_init);
ka_module_exit(ascend_adapter_exit);

KA_MODULE_LICENSE("GPL");
KA_MODULE_AUTHOR("Huawei Tech. Co., Ltd.");
KA_MODULE_DESCRIPTION("DAVINCI DMS Manager driver");
KA_MODULE_SOFTDEP("pre: asdrv_pbl post: drv_pcie_host");
