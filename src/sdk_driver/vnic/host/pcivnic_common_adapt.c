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

#ifdef CONFIG_GENERIC_BUG
#undef CONFIG_GENERIC_BUG
#endif
#ifdef CONFIG_BUG
#undef CONFIG_BUG
#endif
#ifdef CONFIG_DEBUG_BUGVERBOSE
#undef CONFIG_DEBUG_BUGVERBOSE
#endif

#include "ka_system_pub.h"
#include "ka_errno_pub.h"
#include "ka_driver_pub.h"
#include "ka_compiler_pub.h"
#include "ka_barrier_pub.h"
#include "ka_net_pub.h"
#include "ka_fs_pub.h"
#include "ka_memory_pub.h"
#include "pcivnic_mem_alloc.h"
#include "pbl/pbl_spod_info.h"
#include "pcivnic_main.h"

void pcivnic_tx_sq_task(unsigned long data)
{}
void pcivnic_tx_cq_task(unsigned long data)
{}
void pcivnic_forward_packet(ka_sk_buff_t *skb, struct pcivnic_netdev *vnic_dev, int forward_dev)
{
    ka_sk_buff_t *skb_cp = NULL;
    int next_hop;
    int begin = 0;

    next_hop = pcivnic_up_get_next_hop((unsigned char *)skb->data);
    if (next_hop == PCIVNIC_NEXT_HOP_LOCAL_NETDEV) {
        pcivnic_rx_packet(skb, vnic_dev, (u32)forward_dev);
    } else if (next_hop != PCIVNIC_NEXT_HOP_BROADCAST) {
        if ((forward_dev == next_hop) || (vnic_dev->pcidev[next_hop] == NULL)) {
            ka_net_dev_consume_skb_any(skb);
        } else {
            if (pcivnic_forward_flow_ctrl(vnic_dev, forward_dev, next_hop) == PCIVNIC_VALID) {
                ka_net_dev_consume_skb_any(skb);
                return;
            }

            (void)pcivnic_pciedev_send(skb, vnic_dev->pcidev[next_hop], NULL);
        }
    } else {
        while (1) {
            next_hop = pcivnic_get_next_valid_pcidev(vnic_dev, begin);
            if (next_hop < 0) {
                break;
            }
            begin = next_hop + 1;
            if (forward_dev == next_hop) {
                continue;
            }

            if (pcivnic_forward_flow_ctrl(vnic_dev, forward_dev, next_hop) == PCIVNIC_VALID) {
                continue;
            }

            skb_cp = ka_net_skb_copy(skb, KA_GFP_ATOMIC);
            if (skb_cp != NULL) {
                (void)pcivnic_pciedev_send(skb_cp, vnic_dev->pcidev[next_hop], NULL);
            }
        }

        pcivnic_rx_packet(skb, vnic_dev, (u32)forward_dev);
    }
}

STATIC ssize_t pcivnic_read_file(ka_file_t *file, loff_t *pos, char *addr, size_t count)
{
    return ka_fs_read_file(file, pos, addr, count);
}

STATIC char *pcivnic_skip_blank_space(const char *ptr)
{
    while ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n')) {
        ptr++;
    }

    return (char *)ptr;
}

STATIC char *pcivnic_skip_line(const char *ptr)
{
    while ((*ptr != '\n') && (*ptr != '\0')) {
        ptr++;
    }

    if (*ptr == '\n') {
        ptr++;
    }

    return pcivnic_skip_blank_space(ptr);
}

STATIC char *pcivnic_skip_notes(const char *ptr)
{
    /* skip blank space  */
    ptr = pcivnic_skip_blank_space(ptr);

    /* skip notes with # */
    while (*ptr == '#') {
        ptr = pcivnic_skip_line(ptr);
    }

    return (char *)ptr;
}

STATIC int pcivnic_is_valid_mac(const unsigned char *mac, int len)
{
    (void)len;

    return ka_net_is_valid_ether_addr(mac);
}

void pcivnic_get_mac(unsigned char last_byte, unsigned char *mac)
{
    unsigned int tmpmac[KA_ETH_ALEN];
    ka_file_t *file = NULL;
    char *tmpbuf = NULL;
    char *pmac = NULL;
    loff_t offset = 0;
    int tmp_id = -1;
    ssize_t len;
    const int dev_id = 0;
    int ret = 0;

    file = ka_fs_filp_open(PCIVNIC_MAC_FILE, KA_O_RDONLY, 0);
    if (KA_IS_ERR(file)) {
        devdrv_warn("pcivnic mac config file(%s) does not exist\n", PCIVNIC_MAC_FILE);
        goto random_mac;
    }

    tmpbuf = (char *)pcivnic_kvzalloc(PCIVNIC_CONF_FILE_SIZE, KA_GFP_KERNEL);
    if (tmpbuf == NULL) {
        devdrv_err("pcivnic mac buffer malloc failed, size %d\n", PCIVNIC_CONF_FILE_SIZE);
        goto close_file;
    }

    len = pcivnic_read_file(file, &offset, tmpbuf, PCIVNIC_CONF_FILE_SIZE - 1);
    if (len <= 0) {
        devdrv_warn("read file(%s) unsuccessful\n", PCIVNIC_MAC_FILE);
        goto free_buf;
    }

    tmpbuf[len] = '\0';
    pmac = &tmpbuf[0];
    pmac = pcivnic_skip_notes(pmac);

    /* find the mac address by device id */
    while (*pmac != '\0') {
        tmp_id = -1;
        ret = sscanf_s(pmac, "%d %x:%x:%x:%x:%x:%x", &tmp_id, &tmpmac[PCIVNIC_MAC_0], &tmpmac[PCIVNIC_MAC_1],
                       &tmpmac[PCIVNIC_MAC_2], &tmpmac[PCIVNIC_MAC_3], &tmpmac[PCIVNIC_MAC_4], &tmpmac[PCIVNIC_MAC_5]);
        if (ret != (KA_ETH_ALEN + 1)) {
            devdrv_warn("sscanf_s unsuccessful! ret = %d\n", ret);
            goto free_buf;
        }

        if ((ret == PCIVNIC_CONF_SSCANF_OK) && (tmp_id == dev_id)) {
            break;
        }

        pmac = pcivnic_skip_line(pmac);
    }

    if (tmp_id != dev_id) {
        devdrv_warn("can't find device %d fixed mac address\n", dev_id);
        goto free_buf;
    }

    /* mac[4] and mac[5] will be changed in device */
    mac[PCIVNIC_MAC_0] = tmpmac[PCIVNIC_MAC_0] & 0xFF;
    mac[PCIVNIC_MAC_1] = tmpmac[PCIVNIC_MAC_1] & 0xFF;
    mac[PCIVNIC_MAC_2] = tmpmac[PCIVNIC_MAC_2] & 0xFF;
    mac[PCIVNIC_MAC_3] = last_byte;
    mac[PCIVNIC_MAC_4] = tmpmac[PCIVNIC_MAC_4] & 0xFF;
    mac[PCIVNIC_MAC_5] = last_byte;

    if (pcivnic_is_valid_mac(mac, KA_ETH_ALEN) == 0) {
        devdrv_warn("The MAC is invalid from file %s\n", PCIVNIC_MAC_FILE);
        goto free_buf;
    }

    pcivnic_kvfree(tmpbuf);
    tmpbuf = NULL;
    (void)ka_fs_filp_close(file, NULL);
    file = NULL;

    devdrv_info("pcivnic device %d, get mac success\n", dev_id);
    return;

free_buf:
    pcivnic_kvfree(tmpbuf);
    tmpbuf = NULL;
close_file:
    (void)ka_fs_filp_close(file, NULL);
    file = NULL;
random_mac:
    mac[PCIVNIC_MAC_0] = PCIVNIC_MAC_VAL_0;
    mac[PCIVNIC_MAC_1] = PCIVNIC_MAC_VAL_1;
    mac[PCIVNIC_MAC_2] = PCIVNIC_MAC_VAL_2;
    mac[PCIVNIC_MAC_3] = PCIVNIC_MAC_VAL_3;
    mac[PCIVNIC_MAC_4] = PCIVNIC_MAC_VAL_4;
    mac[PCIVNIC_MAC_5] = last_byte;
    devdrv_info("pcivnic using default MAC. (dev_id=%d)\n", dev_id);

    return;
}