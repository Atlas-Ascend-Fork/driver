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

struct pcivnic_sq_desc *pcivnic_get_w_sq_desc(void *msg_chan, u32 *tail)
{
    void *sq_desc = NULL;

    sq_desc = devdrv_get_msg_chan_slave_sq_tail(msg_chan, tail);

    return (struct pcivnic_sq_desc *)sq_desc;
}

int pcivnic_copy_sq_desc_to_remote(struct pcivnic_pcidev *pcidev, const struct pcivnic_sq_desc *sq_desc)
{
    void *msg_chan = pcidev->msg_chan;

    (void)sq_desc;
    devdrv_move_msg_chan_slave_sq_tail(msg_chan);

    /* shared memory without copying */
    ka_wmb();

    /* trigger doorbell irq */
    devdrv_msg_ring_doorbell(msg_chan);
    return 0;
}

struct pcivnic_cq_desc *pcivnic_get_w_cq_desc(void *msg_chan)
{
    void *cq_desc = NULL;

    cq_desc = devdrv_get_msg_chan_slave_cq_tail(msg_chan);

    return (struct pcivnic_cq_desc *)cq_desc;
}

int pcivnic_copy_cq_desc_to_remote(struct pcivnic_pcidev *pcidev, const struct pcivnic_cq_desc *cq_desc)
{
    void *msg_chan = pcidev->msg_chan;

    (void)cq_desc;
    devdrv_move_msg_chan_slave_cq_tail(msg_chan);

    /* shared memory without copying */
    ka_wmb();

    /* trigger doorbell irq */
    devdrv_msg_ring_cq_doorbell(msg_chan);
    return 0;
}

int pcivnic_up_get_next_hop(const unsigned char *dmac)
{
    if (*(u32 *)pcivnic_get_vnic_mac() != *(u32 *)dmac) {
        return PCIVNIC_NEXT_HOP_BROADCAST;
    }

    if (dmac[PCIVNIC_MAC_5] == HOST_MAC_LAST_BYTE) {
        return PCIVNIC_NEXT_HOP_LOCAL_NETDEV;
    } else if (dmac[PCIVNIC_MAC_5] < NETDEV_PF_PCIDEV_NUM) {
        if ((uda_get_chip_type(dmac[PCIVNIC_MAC_5]) != HISI_CLOUD_V2) &&
            (pcivnic_get_vnic_mac()[PCIVNIC_MAC_4] != dmac[PCIVNIC_MAC_4])) {
            return PCIVNIC_NEXT_HOP_BROADCAST;
        }
        return dmac[PCIVNIC_MAC_5];
    } else {
        return PCIVNIC_NEXT_HOP_BROADCAST;
    }
}

STATIC int pcivnic_ctrl_msg_recv(u32 devid, void *data, u32 in_data_len, u32 out_data_len, u32 *real_out_len)
{
    *real_out_len = 0;

    return 0;
}

STATIC struct devdrv_common_msg_client g_pcivnic_host_comm_msg_client = {
    .type = DEVDRV_COMMON_MSG_PCIVNIC,
    .common_msg_recv = pcivnic_ctrl_msg_recv,
};

int pcivnic_register_common_client(void)
{
    return devdrv_register_common_msg_client(&g_pcivnic_host_comm_msg_client);
}
int pcivnic_unregister_common_client(u32 dev_id)
{
    return devdrv_unregister_common_msg_client(dev_id, &g_pcivnic_host_comm_msg_client);
}

bool pcivnic_get_sysfs_creat_group_capbility(ka_device_t *dev, int dev_id)
{
    ka_pci_dev_t *pdev = NULL;
    int devnum_by_pdev;

    pdev = ka_pci_to_pci_dev(dev);
    devnum_by_pdev = devdrv_get_davinci_dev_num_by_pdev(pdev);
    if (devnum_by_pdev <= 0) {
        return false;
    }

    if (dev_id % devnum_by_pdev == 0) {
        return true;
    }

    return false;
}

#define PCIVNIC_HOST_NOTIFIER "vnic_host"
STATIC int pcivnic_host_notifier_func(u32 udevid, enum uda_notified_action action)
{
    int ret = 0;

    if (udevid >= NETDEV_PCIDEV_NUM) {
        return 0;
    }

    if (action == UDA_INIT) {
        ret = pcivnic_init_instance(udevid, uda_get_device(udevid));
    } else if (action == UDA_UNINIT) {
        ret = pcivnic_uninit_instance(udevid);
    } else {
#ifndef DRV_UT
        return 0;
#endif
    }

    devdrv_info("notifier action. (udevid=%u;action=%d;ret=%d)\n", udevid, action, ret);

    return ret;
}

int pcivnic_register_client(void)
{
    struct uda_dev_type type;
    int ret;

    uda_davinci_near_real_entity_type_pack(&type);
    ret = uda_notifier_register(PCIVNIC_HOST_NOTIFIER, &type, UDA_PRI1, pcivnic_host_notifier_func);
    if (ret != 0) {
        devdrv_err("uda_notifier_register fail ret = %d\n", ret);
        return ret;
    }
    return 0;
}

int pcivnic_init_netdev(struct pcivnic_netdev *vnic_dev)
{
    u32 chip_type = HISI_CHIP_NUM;
    int cycle_times = 0;
    int retry_times = 0;
    int dev_num;
    int ret;
    u32 i;

    dev_num = devdrv_get_dev_num();

retry:
    for (i = 0; i < NETDEV_PCIDEV_NUM; i++) {
        if (cycle_times == dev_num) {
            break;
        }

        chip_type = devdrv_get_dev_chip_type_by_addid(i);
        if (chip_type == HISI_CHIP_UNKNOWN) {
            continue;
        }

        cycle_times++;
        if (pcivnic_is_register_netdev(i) == true) {
            ret = pcivnic_register_netdev(vnic_dev);
            if (ret != 0) {
                devdrv_err("register netdev fail ret = %d\n", ret);
            }
            return ret;
        }
    }
    if ((cycle_times != dev_num) && (retry_times <= PCIVNIC_INIT_NETDEV_RETRY_TIMES)) {
        cycle_times = 0;
        retry_times++;
        ka_system_msleep(1000); // 1000 milliseconds
        goto retry;
    }

    return 0;
}

int pcivnic_unregister_client(void)
{
    struct uda_dev_type type;
    uda_davinci_near_real_entity_type_pack(&type);
    return uda_notifier_unregister(PCIVNIC_HOST_NOTIFIER, &type);
}