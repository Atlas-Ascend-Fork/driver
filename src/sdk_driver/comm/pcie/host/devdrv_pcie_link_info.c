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

#include "ka_kernel_def_pub.h"
#include "ka_driver_pub.h"
#include "ka_pci_pub.h"
#include "devdrv_util.h"
#include "devdrv_pci.h"
#include "devdrv_ctrl.h"
#include "devdrv_common_msg.h"
#include "dms/dms_cmd_def.h"
#include "devdrv_pcie_link_info.h"
#include "pbl/pbl_uda.h"

static u32 g_pcie_channel_status = DEVDRV_PCIE_COMMON_CHANNEL_INIT;
static ka_atomic_t g_peer_pcie_status;

#define PCIE_DUMP_LTSSM_TRACER_FN_NAME "pcie_dump_ltssm_tracer"
typedef void (*pcie_dump_ltssm_tracer_fn)(void);
__attribute__((unused)) STATIC pcie_dump_ltssm_tracer_fn g_pcie_dump_ltssm_tracer_fn = NULL;
ka_mutex_t g_pcie_dump_ltssm_tracer_lock;

void devdrv_pcie_dump_ltssm_tracer_init_lock(void)
{
    ka_task_mutex_init(&g_pcie_dump_ltssm_tracer_lock);
}

void devdrv_get_pcie_dump_ltssm_tracer_symbol(void)
{
#ifdef CFG_FEATURE_PCIE_LINK_INFO
    g_pcie_dump_ltssm_tracer_fn = (pcie_dump_ltssm_tracer_fn)(uintptr_t)__ka_system_symbol_get(
        PCIE_DUMP_LTSSM_TRACER_FN_NAME);
    if (g_pcie_dump_ltssm_tracer_fn == NULL) {
        devdrv_warn("pcie_dump_ltssm_tracer symbol not find.\n");
    }
#endif
}

void devdrv_put_pcie_dump_ltssm_tracer_symbol(void)
{
#ifdef CFG_FEATURE_PCIE_LINK_INFO
    ka_task_mutex_lock(&g_pcie_dump_ltssm_tracer_lock);
    if (g_pcie_dump_ltssm_tracer_fn == NULL) {
        devdrv_warn("pcie_dump_ltssm_tracer symbol not find.\n");
        ka_task_mutex_unlock(&g_pcie_dump_ltssm_tracer_lock);
        return;
    }
    __ka_system_symbol_put(PCIE_DUMP_LTSSM_TRACER_FN_NAME);
    g_pcie_dump_ltssm_tracer_fn = NULL;
    ka_task_mutex_unlock(&g_pcie_dump_ltssm_tracer_lock);
#endif
}

STATIC void devdrv_pcie_dump_ltssm_tracer(void)
{
#ifdef CFG_FEATURE_PCIE_LINK_INFO
    ka_task_mutex_lock(&g_pcie_dump_ltssm_tracer_lock);
    if (g_pcie_dump_ltssm_tracer_fn == NULL) {
        devdrv_warn("pcie_dump_ltssm_tracer symbol not find.\n");
        ka_task_mutex_unlock(&g_pcie_dump_ltssm_tracer_lock);
        return;
    }
    g_pcie_dump_ltssm_tracer_fn();
    ka_task_mutex_unlock(&g_pcie_dump_ltssm_tracer_lock);
#endif
}

void devdrv_set_pcie_channel_status(u32 status)
{
#ifdef CFG_FEATURE_PCIE_LINK_INFO
    g_pcie_channel_status = status;
    devdrv_info("set pcie channel status. (status=%u)\n", status);
#endif
    return;
}
KA_EXPORT_SYMBOL(devdrv_set_pcie_channel_status);

u32 devdrv_get_pcie_channel_status(void)
{
    return g_pcie_channel_status;
}

void devdrv_peer_pcie_status_init(void)
{
    ka_base_atomic_set(&g_peer_pcie_status, DEVDRV_PEER_STATUS_NORMAL);
    return;
}

void devdrv_set_peer_pcie_status(u32 status)
{
#ifdef CFG_FEATURE_PCIE_LINK_INFO
    ka_base_atomic_set(&g_peer_pcie_status, status);
#endif
    return;
}

u32 devdrv_get_peer_pcie_status(void)
{
    return ka_base_atomic_read(&g_peer_pcie_status);
}
KA_EXPORT_SYMBOL(devdrv_get_peer_pcie_status);
STATIC int devdrv_get_pcie_mac_link_info(struct devdrv_pcie_link_info_para *pcie_link_info)
{
    u32 mac_reg_link_value;
    u32 ltssm_st;
    void __ka_mm_iomem *apb_base = NULL;

    if (pcie_link_info == NULL) {
        devdrv_err("pcie_link_info is NULL.\n");
        return -EINVAL;
    }
    apb_base = ka_mm_ioremap(PCIE_BASE_ADDR, PCIE_REG_SIZE);
    if (apb_base == NULL) {
        devdrv_err("ka_mm_ioremap fail, apb_base is NULL.\n");
        return -ENOMEM;
    }

    mac_reg_link_value = ka_mm_readl(apb_base + PCIE_MAC_REG_BASE + PCIE_MAC_REG_LINK_ADDR);

    // link_status
    pcie_link_info->link_status = DEVDRV_PCIE_LINK_STATUS_DOWN;
    ltssm_st = (mac_reg_link_value >> PCIE_MAC_REG_LINK_LTSSM_ST_OFFSET) & 0x3F;
    if (ltssm_st == PCIE_MAC_REG_LINK_LTSSM_L0) {
        pcie_link_info->link_status = DEVDRV_PCIE_LINK_STATUS_OK;
    } else {
        ka_mm_iounmap(apb_base);
        apb_base = NULL;
        devdrv_limit_exclusive(warn, DEVDRV_LIMIT_LOG_0x00, "read mac reg link. (reg_value=0x%x)\n",
                               mac_reg_link_value);
        return 0;
    }

    // rate_mode
    pcie_link_info->rate_mode = (mac_reg_link_value >> PCIE_MAC_REG_LINK_SPEED_OFFSET) & 0xF;
    // lane_num
    pcie_link_info->lane_num = mac_reg_link_value & 0x3F;

    ka_mm_iounmap(apb_base);
    apb_base = NULL;
    return 0;
}

int devdrv_get_pcie_link_info(u32 udevid, struct devdrv_pcie_link_info_para *pcie_link_info)
{
    int ret;

    if (pcie_link_info == NULL) {
        devdrv_err("pcie_link_info is NULL.\n");
        return -EINVAL;
    }
    ret = devdrv_get_pcie_mac_link_info(pcie_link_info);
    if (ret != 0) {
        devdrv_err("get mac link info err. (ret=%d, udevid=%u)\n", ret, udevid);
        return ret;
    }

    // if pcie link up status is down, no need to check channel status
    if (pcie_link_info->link_status == DEVDRV_PCIE_LINK_STATUS_DOWN) {
        devdrv_pcie_dump_ltssm_tracer();
        return 0;
    }

    if (g_pcie_channel_status != DEVDRV_PCIE_COMMON_CHANNEL_OK) {
        pcie_link_info->link_status = DEVDRV_PCIE_LINK_STATUS_CHANNEL_ERR;
    }

    return 0;
}
KA_EXPORT_SYMBOL(devdrv_get_pcie_link_info);

int devdrv_set_err_out_gpio(void)
{
    int ret;

    ret = gpio_request(PCIE_ERR_OUT_GPIO5_00, NULL);
    if (ret != 0) {
        devdrv_err("gpio request fail. (ret=%d)\n", ret);
        return ret;
    }

    ret = gpio_direction_output(PCIE_ERR_OUT_GPIO5_00, 1);
    if (ret != 0) {
        devdrv_err("gpio set direction fail. (ret=%d)\n", ret);
        gpio_free(PCIE_ERR_OUT_GPIO5_00);
        return ret;
    }

    gpio_free(PCIE_ERR_OUT_GPIO5_00);
    return 0;
}

STATIC int devdrv_set_pcie_linkdown(void)
{
    void __ka_mm_iomem *apb_base = NULL;
    u32 tmp;

    apb_base = ka_mm_ioremap(PCIE_BASE_ADDR, PCIE_REG_SIZE);
    if (apb_base == NULL) {
        devdrv_err("ka_mm_ioremap fail, apb_base is NULL.\n");
        return -ENOMEM;
    }

    // mask linkdown interrupt
    tmp = ka_mm_readl(apb_base + PCIE_MAC_REG_BASE + PCIE_MAC_REG_MAC_INT_MASK_OFFSET);
    tmp |= 0x2U;
    ka_mm_writel(tmp, apb_base + PCIE_MAC_REG_BASE + PCIE_MAC_REG_MAC_INT_MASK_OFFSET);
    // disable port
    ka_mm_writel(0U, apb_base + PCIE_CORE_GLOBAL_REG_BASE + PCIE_PORT_EN_OFFSET);

    ka_mm_iounmap(apb_base);
    return 0;
}

int devdrv_force_linkdown_inner(u32 index_id)
{
    struct devdrv_pci_ctrl *pci_ctrl;
    int ret;

    if (index_id >= MAX_DEV_CNT) {
        devdrv_err("index_id invalid\n");
        return -EINVAL;
    }
    ret = devdrv_set_pcie_linkdown();
    if (ret != 0) {
        return ret;
    }
    (void)devdrv_set_err_out_gpio();
    devdrv_peer_fault_notifier(DEVDRV_PEER_STATUS_LINKDOWN);
    devdrv_set_peer_pcie_status(DEVDRV_PEER_STATUS_LINKDOWN);
    devdrv_set_pcie_channel_status(DEVDRV_PCIE_COMMON_CHANNEL_LINKDOWN);

    pci_ctrl = devdrv_pci_ctrl_get(index_id);
    if (pci_ctrl == NULL) {
        devdrv_warn("pcie not ready\n");
        return 0;
    }
    devdrv_set_device_status(pci_ctrl, DEVDRV_DEVICE_DEAD);
    if (pci_ctrl->dma_dev != NULL) {
        devdrv_set_dma_status(pci_ctrl->dma_dev, DEVDRV_DMA_DEAD);
        devdrv_dma_stop_business((unsigned long)(uintptr_t)pci_ctrl->dma_dev);
    }

    devdrv_pci_ctrl_put(pci_ctrl);
    return 0;
}

int devdrv_pcie_host_force_linkdown(u32 udevid)
{
    u32 index_id;

    (void)uda_udevid_to_add_id(udevid, &index_id);
    return devdrv_force_linkdown_inner(index_id);
}

STATIC int devdrv_check_get_bandwidth_para(struct dms_get_device_info_in *input, u32 in_len,
                                           struct dms_get_device_info_out *output, u32 out_len)
{
    if ((input == NULL) || (in_len != sizeof(struct dms_get_device_info_in))) {
        devdrv_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }

    if ((input->buff == NULL) || (input->buff_size < (sizeof(struct devdrv_pcie_link_info_para)))) {
        devdrv_err("Input argument buff is null, or buff_size is wrong. (buff_size=%u)\n", input->buff_size);
        return -EINVAL;
    }

    if ((output == NULL) || (out_len != sizeof(struct dms_get_device_info_out))) {
        devdrv_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }
    return 0;
}

STATIC int devdrv_logic_id_to_index_id(u32 logic_id, u32 *index_id)
{
    u32 udevid;
    int ret;

    ret = uda_devid_to_udevid(logic_id, &udevid);
    if (ret != 0) {
        devdrv_err("Failed to convert dev_id to udevid. (logic_id=%u;ret=%d)\n", logic_id, ret);
        return ret;
    }
    (void)uda_udevid_to_add_id(udevid, index_id);
    if (*index_id >= MAX_DEV_CNT) {
        devdrv_err("Invalid dev_id. (logic_id=%u;dev_id=%u)\n", logic_id, *index_id);
        return -EINVAL;
    }
    return 0;
}

int devdrv_get_pcie_link_bandwidth(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct devdrv_pcie_link_info_para pcie_link_info = {0};
    struct dms_get_device_info_in *input = NULL;
    struct dms_get_device_info_out *output = NULL;
    struct devdrv_pci_ctrl *pci_ctrl = NULL;
    u32 index_id;
    u16 lnksta;
    int ret;

    input = (struct dms_get_device_info_in *)in;
    output = (struct dms_get_device_info_out *)out;
    ret = devdrv_check_get_bandwidth_para(input, in_len, output, out_len);
    if (ret != 0) {
        return ret;
    }
    if (devdrv_get_global_connect_protocol() != DEVDRV_COMMNS_PCIE) {
        return -EOPNOTSUPP;
    }
    ret = devdrv_logic_id_to_index_id(input->dev_id, &index_id);
    if (ret != 0) {
        devdrv_err("Failed to convert dev_id to udevid. (dev_id=%u;ret=%d)\n", input->dev_id, ret);
        return ret;
    }

    pci_ctrl = devdrv_pci_ctrl_get(index_id);
    if (pci_ctrl == NULL) {
        devdrv_err("Can not get dev_ctrl. (dev_id=%u)\n", index_id);
        return -EINVAL;
    }

    ret = ka_pci_capability_read_word(pci_ctrl->pdev, KA_PCI_EXP_LNKSTA, &lnksta);
    if (ret != 0) {
        devdrv_err("Failed to read link status. (dev_id=%u;ret=%d)\n", index_id, ret);
        devdrv_pci_ctrl_put(pci_ctrl);
        return ret;
    }
    /* link rate_mode: bit 3:0, 1=Gen1, 2=Gen2, etc. */
    pcie_link_info.rate_mode = (lnksta & KA_PCI_EXP_LNKSTA_CLS);
    /* link lane_num: bit 9:4, 1=x1, 2=x2, 4=x4, 8=x8, 16=x16, 32=x32 */
    pcie_link_info.lane_num = (lnksta & KA_PCI_EXP_LNKSTA_NLW) >> KA_PCI_EXP_LNKSTA_NLW_SHIFT;
    devdrv_pci_ctrl_put(pci_ctrl);
    ret = ka_base_copy_to_user(input->buff, &pcie_link_info, sizeof(struct devdrv_pcie_link_info_para));
    if (ret != 0) {
        devdrv_err("Failed to copy to user. (dev_id=%u;ret=%d)\n", index_id, ret);
        return ret;
    }
    output->out_size = sizeof(struct devdrv_pcie_link_info_para);
    return 0;
}