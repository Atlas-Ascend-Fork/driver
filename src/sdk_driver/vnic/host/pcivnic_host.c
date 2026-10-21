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

#include "ka_barrier_pub.h"
#include "ka_net_pub.h"
#include "ka_driver_pub.h"
#include "ka_memory_pub.h"
#include "ka_system_pub.h"
#include "ka_pci_pub.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_soc_res.h"

#include "pcivnic_host.h"
#include "pcivnic_main.h"

struct pcivnic_netdev *g_vnic_dev = NULL;
unsigned char g_pcivnic_mac[KA_ETH_ALEN] = {0};
struct pcivnic_ctrl_msg_get_stat pcivnic_stat = {{0}};

struct pcivnic_netdev *pcivnic_get_vnic_dev(void)
{
    return g_vnic_dev;
}
unsigned char *pcivnic_get_vnic_mac(void)
{
    return g_pcivnic_mac;
}
struct pcivnic_pcidev *pcivnic_get_pcidev(void *msg_chan)
{
    return (struct pcivnic_pcidev *)devdrv_get_msg_chan_priv(msg_chan);
}

void pcivnic_set_w_sq_desc_head(void *msg_chan, u32 head)
{
    devdrv_set_msg_chan_slave_sq_head(msg_chan, head);
}

bool pcivnic_w_sq_full_check(void *msg_chan)
{
    return devdrv_msg_chan_slave_sq_full_check(msg_chan);
}

struct pcivnic_sq_desc *pcivnic_get_r_sq_desc(void *msg_chan, u32 *head)
{
    void *sq_desc = NULL;

    sq_desc = devdrv_get_msg_chan_host_sq_head(msg_chan, head);

    return (struct pcivnic_sq_desc *)sq_desc;
}

void pcivnic_move_r_sq_desc(void *msg_chan)
{
    devdrv_move_msg_chan_host_sq_head(msg_chan);

    return;
}

int pcivnic_dma_copy(const struct pcivnic_pcidev *pcidev, u64 src, u64 dst, u32 size,
                     struct devdrv_asyn_dma_para_info *para_info)
{
    int ret;
    ret = hal_kernel_devdrv_dma_async_copy(pcidev->dev_id, DEVDRV_DMA_DATA_COMMON, src, dst, size,
                                           DEVDRV_DMA_DEVICE_TO_HOST, para_info);
    if (ret != 0) {
        devdrv_err("dev %d dma copy failed size %x\n", pcidev->dev_id, size);
    }

    return ret;
}

struct pcivnic_cq_desc *pcivnic_get_r_cq_desc(void *msg_chan)
{
    void *cq_desc = NULL;

    cq_desc = devdrv_get_msg_chan_host_cq_head(msg_chan);

    return (struct pcivnic_cq_desc *)cq_desc;
}

void pcivnic_move_r_cq_desc(void *msg_chan)
{
    devdrv_move_msg_chan_host_cq_head(msg_chan);
}

struct pcivnic_pcidev *pcivnic_get_pciedev(const ka_device_t *dev)
{
    struct pcivnic_pcidev *pcidev = NULL;
    u32 dev_id;

    if (g_vnic_dev != NULL) {
        for (dev_id = 0; dev_id < NETDEV_PCIDEV_NUM; dev_id++) {
            pcidev = g_vnic_dev->pcidev[dev_id];
            if (pcidev == NULL) {
                continue;
            }
            if (pcidev->dev == dev) {
                break;
            }
            pcidev = NULL;
        }
    }

    return pcidev;
}

bool pcivnic_is_p2p_enabled(u32 dev_id, u32 peer_dev_id)
{
    return devdrv_is_p2p_enabled(dev_id, peer_dev_id);
}

int pcivnic_down_get_next_hop(const unsigned char *dmac)
{
    if (*(u32 *)g_pcivnic_mac != *(u32 *)dmac) {
        return PCIVNIC_NEXT_HOP_BROADCAST;
    }

    if (dmac[PCIVNIC_MAC_5] < NETDEV_PF_PCIDEV_NUM) {
#ifndef CFG_FEATURE_OVER_XCOM
        if ((uda_get_chip_type(dmac[PCIVNIC_MAC_5]) != HISI_CLOUD_V2) &&
            (g_pcivnic_mac[PCIVNIC_MAC_4] != dmac[PCIVNIC_MAC_4])) {
            return PCIVNIC_NEXT_HOP_BROADCAST;
        }
#endif
        return dmac[PCIVNIC_MAC_5];
    }

    return PCIVNIC_NEXT_HOP_BROADCAST;
}

ssize_t pcivnic_get_dev_stat(ka_device_t *dev, ka_device_attribute_t *attr, char *buf)
{
    struct pcivnic_pcidev *pcidev = pcivnic_get_pciedev(dev);
    ssize_t offset = 0;
    u32 out_len, msg_len;
    int ret;
    (void)attr;

    if (pcidev == NULL) {
        devdrv_err("cannot find pcidev\n");
        return offset;
    }

    offset = pcivnic_get_dev_stat_inner(dev, buf);

    ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "\ndevice stat:\n");
    if (ret >= 0) {
        offset += ret;
    }

    pcivnic_stat.head.msg_type = PCIVNIC_CTRL_MSG_TYPE_GET_STAT;

    msg_len = sizeof(pcivnic_stat);
    if (msg_len > VNIC_STAT_MSG_MAX_LEN) {
        msg_len = VNIC_STAT_MSG_MAX_LEN;
    }

    ret = devdrv_common_msg_send(pcidev->dev_id, (void *)&pcivnic_stat, sizeof(pcivnic_stat.head), msg_len, &out_len,
                                 DEVDRV_COMMON_MSG_PCIVNIC);
    if ((ret == 0) && (pcivnic_stat.msg_len < KA_MM_PAGE_SIZE - offset)) {
        pcivnic_stat.msg[pcivnic_stat.msg_len] = '\0';
        ret = snprintf_s(buf + offset, KA_MM_PAGE_SIZE - offset, KA_MM_PAGE_SIZE - offset - 1, "%s\n",
                         pcivnic_stat.msg);
        if (ret >= 0) {
            offset += ret;
        }
    }

    return offset;
}

STATIC int pcivnic_exchange_net_info(u32 dev_id)
{
    struct pcivnic_ctrl_msg_net_info msg = {{0}};
    u32 out_len = 0;
    int ret, i;

    msg.head.msg_type = PCIVNIC_CTRL_MSG_TYPE_EXCHANGE_NET_INFO;
    msg.head.host_udevid = dev_id;
    for (i = 0; i < KA_ETH_ALEN; i++) {
        msg.mac[i] = g_pcivnic_mac[i];
    }
    ret = devdrv_common_msg_send(dev_id, (void *)&msg, sizeof(msg.head) + sizeof(msg.mac), sizeof(msg), &out_len,
                                 DEVDRV_COMMON_MSG_PCIVNIC);
    if (ret != 0) {
        devdrv_err("dev %d sync abnormal, wait for device startup.\n", dev_id);
        return ret;
    }

    ret = soc_resmng_dev_set_vnic_ip(dev_id, msg.ip);
    if (ret != 0) {
        devdrv_err("dev %d set vnic ip to soc_resmng fail.\n", dev_id);
        return ret;
    }

    return 0;
}

struct devdrv_trans_msg_chan_info g_msg_chan_info = {
    .msg_type = devdrv_msg_client_pcivnic,
    .queue_depth = PCIVNIC_DESC_QUEUE_DEPTH,
    .level = DEVDRV_MSG_CHAN_LEVEL_LOW,
    .cq_desc_size = sizeof(struct pcivnic_cq_desc),
    .sq_desc_size = sizeof(struct pcivnic_sq_desc),
    .rx_msg_notify = pcivnic_rx_msg_notify,
    .tx_finish_notify = pcivnic_tx_finish_notify,
};

int pcivnic_device_status_abnormal(const void *msg_chan)
{
    return devdrv_device_status_abnormal_check(msg_chan);
}

int pcivnic_init_msg_chan(struct pcivnic_pcidev *pcidev)
{
    u32 devid = pcidev->dev_id;
    void *msg_chan = NULL;

    msg_chan = devdrv_pcimsg_alloc_trans_queue(devid, &g_msg_chan_info);
    if (msg_chan == NULL) {
        devdrv_err("dev %d init host msg chan abnormal!\n", devid);
        return -EINVAL;
    }

    pcivnic_init_msgchan_cq_desc(msg_chan);
    pcidev->msg_chan = msg_chan;
    devdrv_set_msg_chan_priv(msg_chan, (void *)pcidev);

    return 0;
}

void pcivnic_uninit_msg_chan(struct pcivnic_pcidev *pcidev)
{
    if (pcidev->msg_chan == NULL) {
        return;
    }

    devdrv_pcimsg_realease_trans_queue(pcidev->msg_chan);
    pcidev->msg_chan = NULL;
}

int pcivnic_init_msg_instance(struct pcivnic_pcidev *pcidev)
{
    struct pcivnic_netdev *vnic_dev = (struct pcivnic_netdev *)pcidev->netdev;
    struct pcivnic_ctrl_msg_register_netdev register_msg = {{0}};
    u32 len = 0;
    int ret;

    ret = pcivnic_exchange_net_info(pcidev->dev_id);
    if (ret != 0) {
        devdrv_err("dev %d exchange net info failed.\n", pcidev->dev_id);
        return ret;
    }

    /* init msg channel from DMA */
    ret = pcivnic_init_msg_chan(pcidev);
    if (ret != 0) {
#ifndef DRV_UT
        devdrv_err("dev %d init msg chan for netdev failed!\n", pcidev->dev_id);
        return ret;
#endif
    }
    pcidev->status = BIT_STATUS_LINK;

    register_msg.head.msg_type = PCIVNIC_CTRL_MSG_TYPE_RIGISTER_NETDEV;
    ret = devdrv_common_msg_send(pcidev->dev_id, (void *)&register_msg, sizeof(register_msg), sizeof(register_msg),
                                 &len, DEVDRV_COMMON_MSG_PCIVNIC);
    if (ret != 0) {
        pcivnic_uninit_msg_chan(pcidev);
        devdrv_err("dev %d register device netdev fail.\n", pcidev->dev_id);
        return ret;
    }

    (void)devdrv_set_module_init_finish((int)pcidev->dev_id, DEVDRV_HOST_MODULE_PCIVNIC);

    /* !!!!! CI will judge this log to determine the device startup status,
     * do not change it. !!!!!
     */
    devdrv_info("dev id %d: %s, alloc new pcivnic device <%s> success (locked and finish).\n", pcidev->dev_id,
                ka_driver_dev_driver_string(pcidev->dev), ka_net_netdev_get_name(vnic_dev->ndev));

    return 0;
}

u64 pcivnic_dma_map_single(struct pcivnic_pcidev *pcidev, ka_sk_buff_t *skb, u32 buff_type, u32 index, u32 data_len)
{
    ka_dma_data_direction_t dma_dir;
    u64 dma_addr = (~(ka_dma_addr_t)0);
    size_t len;

    dma_dir = (buff_type == PCIVNIC_DESC_QUEUE_TX) ? KA_DMA_TO_DEVICE : KA_DMA_FROM_DEVICE;
    len = (buff_type == PCIVNIC_DESC_QUEUE_TX) ? skb->len : data_len;

#ifdef CFG_FEATURE_AGENT_SMMU
    if ((devdrv_get_connect_protocol(pcidev->dev_id) == CONNECT_PROTOCOL_HCCS) && (pcidev->host_phy_mach_flag == 0)) {
        /* hccs peh's virtualization pass-through, need use mem pool to improve performance */
        if (buff_type == PCIVNIC_DESC_QUEUE_TX) {
            if (memcpy_s(pcidev->tx_buff[index].addr, len, skb->data, len) != 0) {
                devdrv_err_spinlock("device %d memcpy_s fail.\n", pcidev->dev_id);
                return (~(ka_dma_addr_t)0);
            }
            dma_addr = pcidev->tx_buff[index].dma_addr;
        }

        if (buff_type == PCIVNIC_DESC_QUEUE_RX) {
            dma_addr = pcidev->rx_buff[index].dma_addr;
        }
        return dma_addr;
    }
#endif

    dma_addr = hal_kernel_devdrv_dma_map_single(pcidev->dev, skb->data, len, dma_dir);
    if (ka_mm_dma_mapping_error(pcidev->dev, dma_addr) != 0) {
        devdrv_err_spinlock("device %d dma map is error\n", pcidev->dev_id);
        return (~(ka_dma_addr_t)0);
    }

    return dma_addr;
}

void pcivnic_dma_unmap_single(struct pcivnic_pcidev *pcidev, ka_sk_buff_t *skb, u32 buff_type, u32 index, u32 data_len)
{
#ifdef CFG_FEATURE_AGENT_SMMU
    if ((devdrv_get_connect_protocol(pcidev->dev_id) == CONNECT_PROTOCOL_HCCS) && (pcidev->host_phy_mach_flag == 0)) {
        /* hccs peh's virtualization pass-through, need use mem pool to improve performance */
        if (buff_type == PCIVNIC_DESC_QUEUE_RX) {
            if (memcpy_s(skb->data, data_len, pcidev->rx_buff[index].addr, data_len) != 0) {
                devdrv_err_spinlock("device %d memcpy_s fail.\n", pcidev->dev_id);
            }
        }
        return;
    }
#endif

    if (buff_type == PCIVNIC_DESC_QUEUE_TX) {
        hal_kernel_devdrv_dma_unmap_single(pcidev->dev, pcidev->tx[index].addr, skb->len, KA_DMA_TO_DEVICE);
        pcidev->tx[index].addr = (~(ka_dma_addr_t)0);
    }
    if (buff_type == PCIVNIC_DESC_QUEUE_RX) {
        hal_kernel_devdrv_dma_unmap_single(pcidev->dev, pcidev->rx[index].addr, data_len, KA_DMA_FROM_DEVICE);
        pcidev->rx[index].addr = (~(ka_dma_addr_t)0);
    }
}

/* hccs peh's virtualization pass-through, need use mem pool to improve performance */
void pcivnic_skb_data_buff_uninit(struct pcivnic_pcidev *pcidev)
{
#ifdef CFG_FEATURE_AGENT_SMMU
    u32 host_flag;
    int ret;
    int i;

    ret = devdrv_get_host_phy_mach_flag(pcidev->dev_id, &host_flag);
    if (ret != 0) {
        return;
    }

    if ((devdrv_get_connect_protocol(pcidev->dev_id) != CONNECT_PROTOCOL_HCCS) || (host_flag != 0)) {
        return;
    }

    for (i = 0; i < PCIVNIC_DESC_QUEUE_DEPTH; i++) {
        if (pcidev->tx_buff[i].addr != NULL) {
            hal_kernel_devdrv_dma_free_coherent(pcidev->dev, PCIVNIC_MAX_SKB_BUFF_SIZE, pcidev->tx_buff[i].addr,
                                                pcidev->tx_buff[i].dma_addr);
            pcidev->tx_buff[i].addr = NULL;
        }
        pcidev->tx_buff[i].dma_addr = (~(ka_dma_addr_t)0);

        if (pcidev->rx_buff[i].addr != NULL) {
            hal_kernel_devdrv_dma_free_coherent(pcidev->dev, PCIVNIC_MAX_SKB_BUFF_SIZE, pcidev->rx_buff[i].addr,
                                                pcidev->rx_buff[i].dma_addr);
            pcidev->rx_buff[i].addr = NULL;
        }
        pcidev->rx_buff[i].dma_addr = (~(ka_dma_addr_t)0);
    }
#endif
}

/* hccs peh's virtualization pass-through, need use mem pool to improve performance */
int pcivnic_skb_data_buff_init(struct pcivnic_pcidev *pcidev)
{
#ifdef CFG_FEATURE_AGENT_SMMU
    u32 host_phy_mach_flag;
    int ret;
    int i;

    ret = devdrv_get_host_phy_mach_flag(pcidev->dev_id, &host_phy_mach_flag);
    if (ret != 0) {
        pcidev->host_phy_mach_flag = DEVDRV_HOST_PHY_MACH_FLAG;
        return 0;
    }
    pcidev->host_phy_mach_flag = host_phy_mach_flag;

    if ((devdrv_get_connect_protocol(pcidev->dev_id) != CONNECT_PROTOCOL_HCCS) || (host_phy_mach_flag != 0)) {
        return 0;
    }

    for (i = 0; i < PCIVNIC_DESC_QUEUE_DEPTH; i++) {
        pcidev->tx_buff[i].addr = hal_kernel_devdrv_dma_zalloc_coherent(pcidev->dev, PCIVNIC_MAX_SKB_BUFF_SIZE,
                                                                        &pcidev->tx_buff[i].dma_addr, KA_GFP_KERNEL);
        if (pcidev->tx_buff[i].addr == NULL) {
            pcivnic_skb_data_buff_uninit(pcidev);
            devdrv_err("Call skb_data tx buff failed. (dev_id=%d)\n", pcidev->dev_id);
            return -ENOMEM;
        }

        pcidev->rx_buff[i].addr = hal_kernel_devdrv_dma_zalloc_coherent(pcidev->dev, PCIVNIC_MAX_SKB_BUFF_SIZE,
                                                                        &pcidev->rx_buff[i].dma_addr, KA_GFP_KERNEL);
        if (pcidev->rx_buff[i].addr == NULL) {
            pcivnic_skb_data_buff_uninit(pcidev);
            devdrv_err("Call skb_data rx buff failed. (dev_id=%d)\n", pcidev->dev_id);
            return -ENOMEM;
        }
    }
#endif
    return 0;
}

STATIC bool pcivnic_init_instance_check(u32 dev_id)
{
    if (devdrv_get_connect_protocol(dev_id) == CONNECT_PROTOCOL_UB) {
        devdrv_info("Connect protocol is ub, don't need to init and uninit.(dev_id=%d)\n", dev_id);
        return false;
    }

    if ((devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) &&
        (devdrv_is_mdev_vm_full_spec(dev_id) != true)) {
        return false;
    }

    return true;
}

STATIC int pcivnic_init_npu_instance(u32 dev_id)
{
    struct pcivnic_ctrl_msg_dev_instance instance_msg = {{0}};
    u32 len = 0;
    int ret;

    instance_msg.head.msg_type = PCIVNIC_CTRL_MSG_TYPE_INSTANCE;
    instance_msg.head.host_udevid = dev_id;
    ret = devdrv_common_msg_send(dev_id, (void *)&instance_msg, sizeof(instance_msg), sizeof(instance_msg), &len,
                                 DEVDRV_COMMON_MSG_PCIVNIC);
    if (ret != 0) {
        devdrv_err("dev %d instance device netdev fail.\n", dev_id);
#ifdef DRV_UT
        ret = 0;
#endif
        return ret;
    }

    return 0;
}

int pcivnic_init_instance(u32 dev_id, ka_device_t *dev)
{
    struct pcivnic_netdev *vnic_dev = g_vnic_dev;
    struct pcivnic_pcidev *pcidev = NULL;
    int ret;

    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        return 0;
    }

    if (dev_id >= NETDEV_PCIDEV_NUM) {
        devdrv_err("dev_id is invalid, dev_id=%d\n", dev_id);
        return -EINVAL;
    }

    if (pcivnic_init_instance_check(dev_id) == false) {
        return 0;
    }

    ret = pcivnic_init_npu_instance(dev_id);
    if (ret != 0) {
        devdrv_err("dev %d instance device netdev fail.\n", dev_id);
        return ret;
    }

    pcidev = pcivnic_add_dev(vnic_dev, dev, PCIVNIC_DESC_QUEUE_DEPTH, dev_id);
    if (pcidev == NULL) {
        devdrv_err("dev_id %u add pcidev failed!\n", dev_id);
        return -ENOMEM;
    }

    ret = pcivnic_register_common_client();
    if (ret != 0) {
        pcivnic_del_dev(vnic_dev, (int)dev_id);
        devdrv_err("dev_id %u register_common_msg failed ret %d", dev_id, ret);
        return ret;
    }

    ret = pcivnic_init_msg_instance(pcidev);
    if (ret != 0) {
#ifndef DRV_UT
        (void)pcivnic_unregister_common_client(dev_id);
        pcivnic_del_dev(vnic_dev, (int)dev_id);
        return ret;
#endif
    }

    pcivnic_guard_work_init(pcidev);

    devdrv_info("Pcivnic_init_instance finish.(dev_id=%d)\n", pcidev->dev_id);

    return 0;
}

int pcivnic_uninit_instance(u32 dev_id)
{
    struct pcivnic_pcidev *pcidev = NULL;

    if (devdrv_get_pfvf_type_by_devid(dev_id) == DEVDRV_SRIOV_TYPE_VF) {
        return 0;
    }

    if (dev_id >= NETDEV_PCIDEV_NUM) {
        devdrv_err("dev_id is invalid, dev_id=%d\n", dev_id);
        return -EINVAL;
    }

    if (pcivnic_init_instance_check(dev_id) == false) {
        return 0;
    }

    if (g_vnic_dev == NULL) {
        devdrv_err("g_vnic_dev is null. (dev_id=%u)\n", dev_id);
        return 0;
    }

    pcidev = g_vnic_dev->pcidev[dev_id];
    if (pcidev == NULL) {
        return 0;
    }
    pcivnic_guard_work_uninit(pcidev);
    (void)pcivnic_unregister_common_client(pcidev->dev_id);
    devdrv_set_msg_chan_priv(pcidev->msg_chan, NULL);

    pcivnic_del_dev(g_vnic_dev, (int)pcidev->dev_id);
    pcivnic_uninit_msg_chan(pcidev);
    devdrv_info("Pcivnic_uninit_instance finish.(dev_id=%d)\n", pcidev->dev_id);
    return 0;
}

struct pcivnic_netdev *pcivnic_alloc_vnic_dev(void)
{
    struct pcivnic_netdev *vnic_dev = NULL;
    char *ndev_name = "endvnic";

    devdrv_info("pcivnic_alloc_vnic_dev start.\n");
    vnic_dev = pcivnic_alloc_netdev(ndev_name, PCIVNIC_NAME_SIZE);
    if (vnic_dev == NULL) {
        devdrv_err("alloc netdev failed!\n");
        return NULL;
    }

    pcivnic_get_mac(HOST_MAC_LAST_BYTE, g_pcivnic_mac);
    pcivnic_set_netdev_mac(vnic_dev, g_pcivnic_mac);

    g_vnic_dev = vnic_dev;

    return vnic_dev;
}

void pcivnic_free_vnic_dev(void)
{
    struct pcivnic_netdev *vnic_dev = g_vnic_dev;

    if (vnic_dev != NULL) {
        devdrv_info("destroy pcivnic device <%s>.\n", ka_net_netdev_get_name(vnic_dev->ndev));
        pcivnic_free_netdev(vnic_dev);
        g_vnic_dev = NULL;
    }
    return;
}