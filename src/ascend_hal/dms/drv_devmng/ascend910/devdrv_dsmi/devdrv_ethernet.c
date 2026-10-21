/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <net/route.h>
#include <pthread.h>
#include <dirent.h>

#include "securec.h"
#include "dms/dms_devdrv_info_comm.h"
#include "devdrv_ioctl.h"
#include "devmng_common.h"
#include "ascend_hal.h"
#include "devdrv_ethernet.h"

#define INVALID_PID (-1)

int drvDeviceGetEthIdByIndex(uint32_t dev_id, uint32_t port_id, uint32_t *eth_id)
{
    int fd;
    int ret;
    struct ioctl_arg user_arg;

    user_arg.dev_id = dev_id;
    user_arg.data1 = port_id;

    if (eth_id == NULL) {
        DEVDRV_DRV_ERR("eth_id is NULL. devid(%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    fd = devdrv_open_device_manager();
    if (fd < 0) {
        DEVDRV_DRV_ERR("open device manager failed, fd(%d). devid(%u)\n", fd, dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = ioctl(fd, DEVDRV_MANAGER_GET_ETH_ID, &user_arg);
    if (ret == -EAGAIN) {
        DEVDRV_DRV_WARN("ret(%d).\n", ret);
        return INVALID_PID;
    } else if (ret != 0) {
        DEVDRV_DRV_ERR("ioctl failed, ret(%d).\n", ret);
        return INVALID_PID;
    }

    *eth_id = user_arg.data1;

    return DRV_ERROR_NONE;
}

#define IPV6_IP_LOCAL_INDEX_ZERO (0)
#define IPV6_IP_LOCAL_INDEX_ONE (1)
#define IPV6_IP_LOCAL_INDEX_ZERO_VAL (0xFE)
#define IPV6_IP_LOCAL_INDEX_ONE_VAL (0x80)
bool devdrv_ipv6_ip_is_local(const ipaddr_t *ip_addr)
{
    if (ip_addr->addr_v6[IPV6_IP_LOCAL_INDEX_ZERO] == IPV6_IP_LOCAL_INDEX_ZERO_VAL &&
        ip_addr->addr_v6[IPV6_IP_LOCAL_INDEX_ONE] == IPV6_IP_LOCAL_INDEX_ONE_VAL) {
        return true;
    }
    return false;
}

STATIC int devdrv_ip_format_check(unsigned int ip_type, unsigned char *ip_addr, unsigned int ip_len)
{
    int part_of_ip[DEVDRV_PART_IP_NUM] = {0};
    int ret;
    int index;
    (void)(ip_len);

    if (ip_addr == NULL) {
        DEVDRV_DRV_ERR("ip_addr is NULL\n");
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (ip_type == DEVDRV_IPV4) {
        /* Save the 4 part ip address(0, 1, 2, 3) in array */
        ret = sscanf_s((char *)ip_addr, "%d.%d.%d.%d", &part_of_ip[0], &part_of_ip[1], &part_of_ip[2], &part_of_ip[3]);
        if (ret != DEVDRV_PART_IP_NUM) {
            DEVDRV_DRV_ERR("IP format error. ret = %d\n", ret);
            return ret;
        }

        for (index = 0; index < DEVDRV_PART_IP_NUM; index++) {
            if (part_of_ip[index] < DEVDRV_MIN_IP_VALUE || part_of_ip[index] > DEVDRV_MAX_IP_VALUE) {
                return DRV_ERROR_INVALID_VALUE;
            }
        }

        return DRV_ERROR_NONE;
    }

    return DRV_ERROR_INVALID_VALUE;
}

#define IPV6_EACH_LEN 2
STATIC int devdrv_ipv6_gw_format(const char *gw_in, unsigned char *gw_out, int len)
{
    int i;
    int ret;

    for (i = 0; i < len; i++) {
        ret = sscanf_s(gw_in + i * IPV6_EACH_LEN, "%02hhx", &gw_out[i]);
        if (ret <= 0) {
            DEVDRV_DRV_ERR("sscanf_s failed. (gw_in=\"%s\")\n", gw_in);
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }
    return 0;
}

#define IPV6_IP_U8_LEN 16
STATIC int devdrv_ipv6_ip_format(const ipaddr_t *ip_addr, char *ip_out, u32 len)
{
    int i = 0, ret = 0;
    const unsigned char *ip_u8_addr = (const unsigned char *)ip_addr;

    for (i = 0; i < IPV6_IP_U8_LEN; i++) {
        ret = sprintf_s(&ip_out[i * IPV6_EACH_LEN], len - i * IPV6_EACH_LEN, "%02x", ip_u8_addr[i]);
        if (ret < 0) {
            DEVDRV_DRV_ERR("sprintf_s failed. (ret=%d)\n", ret);
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    return 0;
}

STATIC int devdrv_set_ipaddr(const char *ethname, struct dmanager_ip_info config_data)
{
    unsigned char ip_addr[DEVDRV_MAX_IP_LEN];
    struct ifreq ifr;
    int ret;
    int fd = -1;

    (((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr).s_addr = config_data.ip_addr.addr_v4;
    ret = strcpy_s(ifr.ifr_name, sizeof(ifr.ifr_name), ethname);
    if (ret != 0) {
        DEVDRV_DRV_ERR("copy ethname to ifr.ifr_name failed, ret(%d)\n", ret);
        return ret;
    }

    ret = strcpy_s((char *)ip_addr, sizeof(ip_addr), inet_ntoa(((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr));
    if (ret != 0) {
        DEVDRV_DRV_ERR("transfer config_data to ip_addr failed, ret(%d)\n", ret);
        return ret;
    }

    ret = devdrv_ip_format_check(config_data.ip_type, ip_addr, DEVDRV_MAX_IP_LEN);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ip format error, ret(%d)\n", ret);
        return ret;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        DEVDRV_DRV_ERR("open socket failed, %s.\n", strerror(errno));
        return fd;
    }

    ((struct sockaddr_in *)&(ifr.ifr_addr))->sin_family = AF_INET;

    ret = ioctl(fd, SIOCSIFADDR, &ifr);
    if (ret != 0) {
        DEVDRV_DRV_ERR("set %s ip addr error, %s.\n", ethname, strerror(errno));
        (void)close(fd);
        return ret;
    }

    /* set activated flags */
    ret = ioctl(fd, SIOCGIFFLAGS, &ifr);
    if (ret != 0) {
        DEVDRV_DRV_ERR("get flags failed, %s.\n", strerror(errno));
        (void)close(fd);
        return ret;
    }

    ifr.ifr_flags = (unsigned short)ifr.ifr_flags | IFF_UP | IFF_RUNNING;
    ret = ioctl(fd, SIOCSIFFLAGS, &ifr);
    if (ret != 0) {
        DEVDRV_DRV_ERR("set flags failed, %s.\n", strerror(errno));
        (void)close(fd);
        return ret;
    }

    (void)close(fd);
    return DRV_ERROR_NONE;
}

STATIC int devdrv_set_maskaddr(const char *ethname, struct dmanager_ip_info config_data)
{
    unsigned char mask_addr[DEVDRV_MAX_IP_LEN];
    struct ifreq ifr;
    int ret;
    int fd = -1;

    (((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr).s_addr = config_data.mask_addr.addr_v4;
    ret = strcpy_s(ifr.ifr_name, sizeof(ifr.ifr_name), ethname);
    if (ret != 0) {
        DEVDRV_DRV_ERR("copy ethname to ifr.ifr_name failed, ret(%d)\n", ret);
        return ret;
    }

    ret = strcpy_s((char *)mask_addr, sizeof(mask_addr), inet_ntoa(((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr));
    if (ret != 0) {
        DEVDRV_DRV_ERR("transfer config_data to mask_addr failed, ret(%d)\n", ret);
        return ret;
    }

    ret = devdrv_ip_format_check(config_data.ip_type, mask_addr, DEVDRV_MAX_IP_LEN);
    if (ret != 0) {
        DEVDRV_DRV_ERR("mask address format error, ret(%d)\n", ret);
        return ret;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        DEVDRV_DRV_ERR("open socket failed, %s.\n", strerror(errno));
        return fd;
    }

    ((struct sockaddr_in *)&(ifr.ifr_addr))->sin_family = AF_INET;

    ret = ioctl(fd, SIOCSIFNETMASK, &ifr);
    if (ret != 0) {
        DEVDRV_DRV_ERR("set %s mask addr error, %s.\n", ethname, strerror(errno));
        (void)close(fd);
        return ret;
    }

    /* set activated flags */
    ret = ioctl(fd, SIOCGIFFLAGS, &ifr);
    if (ret != 0) {
        DEVDRV_DRV_ERR("get flags failed, %s.\n", strerror(errno));
        (void)close(fd);
        return ret;
    }

    ifr.ifr_flags = (unsigned short)ifr.ifr_flags | IFF_UP | IFF_RUNNING;
    ret = ioctl(fd, SIOCSIFFLAGS, &ifr);
    if (ret != 0) {
        DEVDRV_DRV_ERR("set flags failed, %s.\n", strerror(errno));
        (void)close(fd);
        return ret;
    }

    (void)close(fd);
    return DRV_ERROR_NONE;
}

/*
 * A introduction of /proc/net/if_inet6
 * each column: addr ifindex prefix scope flag ethname
 * eg: fe80000000000000121b54fffefc9900 03 40 20 80   end0v0
 * prefix: "40" = 0x40 = 64
 * IPV6_STANDARD_CHAR_LEN: addr_len = 32
 * IPV6_PREFIX_OFFSET: addr_len + 1 + ifindex_len + 1 = 32 + 1 + 2 + 1 = 36
 * IPV6_PREFIX_LEN: prefix_len = 2
 **/
#define IPV6_PREFIX_DETAIL_FILE "/proc/net/if_inet6"
#define IPV6_PREFIX_OFFSET (36)
#define IPV6_PREFIX_LEN (2)
#define IPV6_STANDARD_CHAR_LEN (32)
#define IPV6_DETAIL_LINE_LEN (256)
STATIC int devdrv_get_ipv6_prefix(const char *ethname, const ipaddr_t *ip_addr, u8 *prefix)
{
    int ret;
    u32 prefix_len = 0;
    char buf[IPV6_DETAIL_LINE_LEN];
    char ip_addr_str[IPV6_STANDARD_CHAR_LEN + 1];
    char prefix_str[IPV6_PREFIX_LEN + 1];
    FILE *fp = NULL;
    int errno_tmp;

    ret = devdrv_ipv6_ip_format(ip_addr, ip_addr_str, IPV6_STANDARD_CHAR_LEN + 1);
    if (ret != 0) {
        DEVDRV_DRV_ERR("Failed to invoke devdrv_ipv6_ip_format. (ethname=\"%s\")\n", ethname);
        return ret;
    }

    fp = fopen(IPV6_PREFIX_DETAIL_FILE, "r");
    if (fp == NULL) {
        errno_tmp = errno;
        DEVDRV_DRV_ERR("Failed to open the ipv6_route file, (errno=%d).\n", errno_tmp);
        return DRV_ERROR_INVALID_HANDLE;
    }

    ret = DRV_ERROR_INVALID_VALUE;
    while (memset_s(buf, IPV6_DETAIL_LINE_LEN, 0, IPV6_DETAIL_LINE_LEN) == 0 && fgets(buf, sizeof(buf), fp) != NULL) {
        /* If less than offset + prefix_len, it will skip. */
        if (strlen(buf) < IPV6_PREFIX_OFFSET + IPV6_PREFIX_LEN) {
            continue;
        }
        /* If ethname or ip not found, it will skip. */
        if (strstr(buf, ethname) == NULL || strncmp(ip_addr_str, buf, IPV6_STANDARD_CHAR_LEN) != 0) {
            continue;
        }
        /* To get prefix value, which is a string. */
        ret = memcpy_s(prefix_str, IPV6_PREFIX_LEN, buf + IPV6_PREFIX_OFFSET, IPV6_PREFIX_LEN);
        if (ret != 0) {
            ret = DRV_ERROR_OUT_OF_MEMORY;
            DEVDRV_DRV_ERR("memcpy_s failed.\n");
            goto OUT;
        }
        /* To make string to integer. */
        prefix_str[IPV6_PREFIX_LEN] = '\0';
        ret = sscanf_s(prefix_str, "%x", &prefix_len);
        if (ret <= 0) {
            ret = DRV_ERROR_OUT_OF_MEMORY;
            DEVDRV_DRV_ERR("sscanf_s failed.\n");
            goto OUT;
        }

        *prefix = prefix_len;
        ret = 0;
        break;
    }

OUT:
    (void)fclose(fp);
    return ret;
}

STATIC int devdrv_get_ip_by_ipvx(int ipvx, const char *ethname, ipaddr_t *ip_addr, ipaddr_t *mask_addr)
{
    u8 ipv6_prefix;
    int ret;
    struct ifaddrs *ifAddrStruct0 = NULL;
    struct ifaddrs *ifAddrStruct1 = NULL;

    ret = getifaddrs(&ifAddrStruct0);
    if (ret != OK || ifAddrStruct0 == NULL) {
        DEVDRV_DRV_ERR("Failed to get network-adapter info.\n");
        return DRV_ERROR_NO_DEVICE;
    }

    for (ifAddrStruct1 = ifAddrStruct0; ifAddrStruct1 != NULL; ifAddrStruct1 = ifAddrStruct1->ifa_next) {
        if (ifAddrStruct1->ifa_addr == NULL || ifAddrStruct1->ifa_name == NULL ||
            strncmp(ifAddrStruct1->ifa_name, ethname, DEVDRV_MAX_ETH_NAME_LEN) != 0) {
            continue;
        }
        /* ipv4 */
        if ((((*ifAddrStruct1).ifa_addr)->sa_family) == ipvx && ipvx == AF_INET) {
            ip_addr->addr_v4 = ((struct sockaddr_in *)ifAddrStruct1->ifa_addr)->sin_addr.s_addr;
            mask_addr->addr_v4 = ((struct sockaddr_in *)ifAddrStruct1->ifa_netmask)->sin_addr.s_addr;
            goto OUT;
            /* ipv6 */
        } else if ((((*ifAddrStruct1).ifa_addr)->sa_family) == ipvx && ipvx == AF_INET6) {
            ret = memcpy_s(ip_addr, sizeof(ipaddr_t), &((struct sockaddr_in6 *)ifAddrStruct1->ifa_addr)->sin6_addr,
                           sizeof(struct in6_addr));
            if (ret != 0) {
                ret = DRV_ERROR_MEMORY_OPT_FAIL;
                DEVDRV_DRV_ERR("memcpy_s failed. (ipvx=%d; ethx=\"%s\")\n", AF_INET6, ethname);
                goto OUT;
            }
            if (devdrv_ipv6_ip_is_local(ip_addr)) {
                continue;
            }
            /* To get ipv6 prefix. */
            ret = devdrv_get_ipv6_prefix(ethname, ip_addr, &ipv6_prefix);
            if (ret != 0) {
                DEVDRV_DRV_ERR("Failed to invoke devdrv_get_ipv6_prefix. (ethname=\"%s\")\n", ethname);
                goto OUT;
            }
            ((u8 *)mask_addr)[0] = ipv6_prefix;
            goto OUT;
        }
    }
    ret = DRV_ERROR_NO_DEVICE;

OUT:
    freeifaddrs(ifAddrStruct0);
    return ret;
}

#define IPV6_HEX_GW_OFFSET 72
#define IPV6_ROUTE_DETAIL_FILE "/proc/net/ipv6_route"
#define IPV6_GW_DEFAULT_ADDR "00000000000000000000000000000000"
#define IPV6_GW_NOT_SET_FLAG 0
#define IPV6_GW_ALL_ZERO_FLAG 1
#define IPV6_GW_WAS_SET_FLAG 2
STATIC int devdrv_get_ipv6_gw_by_ethx(const char *ethname, char *gateway)
{
    int ret;
    int ipv6_gw_set_flag = IPV6_GW_NOT_SET_FLAG;
    char buf[IPV6_DETAIL_LINE_LEN];
    FILE *fp = NULL;
    int errno_tmp;

    fp = fopen(IPV6_ROUTE_DETAIL_FILE, "r");
    if (fp == NULL) {
        errno_tmp = errno;
        DEVDRV_DRV_ERR("Failed to open the ipv6_route file, (errno=%d).\n", errno_tmp);
        return DRV_ERROR_INVALID_HANDLE;
    }

    while (memset_s(buf, IPV6_DETAIL_LINE_LEN, 0, IPV6_DETAIL_LINE_LEN) == 0 && fgets(buf, sizeof(buf), fp) != NULL) {
        if (strlen(buf) < IPV6_HEX_GW_OFFSET + IPV6_STANDARD_CHAR_LEN) {
            continue;
        }
        /* Check if it owns the key words or not. */
        if (strstr(buf, ethname) == NULL) {
            continue;
        }
        /* default gateway. */
        if (strncmp(buf, IPV6_GW_DEFAULT_ADDR, IPV6_STANDARD_CHAR_LEN) == 0 &&
            strncmp(buf + IPV6_HEX_GW_OFFSET, IPV6_GW_DEFAULT_ADDR, IPV6_STANDARD_CHAR_LEN) != 0) {
            ipv6_gw_set_flag = IPV6_GW_WAS_SET_FLAG;
            gateway[IPV6_STANDARD_CHAR_LEN] = '\0';

            ret = memcpy_s(gateway, IPV6_STANDARD_CHAR_LEN, buf + IPV6_HEX_GW_OFFSET, IPV6_STANDARD_CHAR_LEN);
            if (ret != 0) {
                ret = DRV_ERROR_MEMORY_OPT_FAIL;
                DEVDRV_DRV_ERR("memcpy_s failed.\n");
            }
            break;
        }
        ipv6_gw_set_flag = IPV6_GW_ALL_ZERO_FLAG;
    }

    (void)fclose(fp);
    if (ipv6_gw_set_flag == IPV6_GW_NOT_SET_FLAG) {
        return DRV_ERROR_NO_DEVICE;
    } else if (ipv6_gw_set_flag == IPV6_GW_ALL_ZERO_FLAG) {
        ret = strcpy_s(gateway, IPV6_STANDARD_CHAR_LEN + 1, IPV6_GW_DEFAULT_ADDR);
        if (ret != 0) {
            ret = DRV_ERROR_MEMORY_OPT_FAIL;
            DEVDRV_DRV_ERR("strcpy_s failed.\n");
        }
    }

    return ret;
}

STATIC int devdrv_get_ipv4_gw_by_ethx(const char *ethname, unsigned int *gateway)
{
    char buf[DEVDRV_MAX_LINE_LEN] = {0};
    unsigned int gtw_addr;
    char *tmp = NULL;
    FILE *fp = NULL;
    int ret;
    int errno_tmp;

    fp = fopen(DEVDRV_PATH_PROCNET_ROUTE, "r");
    if (fp == NULL) {
        errno_tmp = errno;
        DEVDRV_DRV_ERR("fopen failed, (errno=%d).\n", errno_tmp);
        return DRV_ERROR_INVALID_HANDLE;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        tmp = buf;
        while (tmp != NULL && (*tmp == ' ')) {
            ++tmp;
        }

        if (tmp != NULL && (strncmp(tmp, ethname, strlen(ethname)) == 0)) {
            break;
        }
    }

    ret = sscanf_s(buf, "%*s%*s%x", &gtw_addr);
    if (ret != 1) {
        DEVDRV_DRV_ERR("sscanf buf(%s) to gtw_addr failed. ret(%u)\n", buf, ret);
        (void)fclose(fp);
        return ret;
    }

    if (tmp != NULL && strncmp(tmp, ethname, strlen(ethname)) != 0) {
        (void)fclose(fp);
        return DRV_ERROR_NO_DEVICE;
    }

    *gateway = gtw_addr;
    (void)fclose(fp);
    return DRV_ERROR_NONE;
}

/* get ipv4 */
int devdrv_get_ip_address(const char *ethname, struct dmanager_ip_info *ack_msg)
{
    int ipvx;

    if (ethname == NULL) {
        DEVDRV_DRV_ERR("Ethname is NULL.\n");
        return DRV_ERROR_PARA_ERROR;
    }
    ipvx = ack_msg->ip_type == 0 ? AF_INET : AF_INET6;
    return devdrv_get_ip_by_ipvx(ipvx, ethname, &ack_msg->ip_addr, &ack_msg->mask_addr);
}

int devdrv_set_ip_address(const char *ethname, struct dmanager_ip_info config_msg)
{
    int ret = -1;

    if (ethname == NULL) {
        DEVDRV_DRV_ERR("ethname is NULL.\n");
        return ret;
    }
    ret = devdrv_set_ipaddr(ethname, config_msg);
    if (ret != 0) {
        DEVDRV_DRV_ERR("set ip address failed, ret(%d)\n", ret);
        return ret;
    }
    ret = devdrv_set_maskaddr(ethname, config_msg);
    if (ret != 0) {
        DEVDRV_DRV_ERR("set mask address failed, ret(%d)\n", ret);
        return ret;
    }

    return DRV_ERROR_NONE;
}

int devdrv_get_gateway_address(const char *ethname, struct dmanager_gtw_info *ack_msg)
{
    int ret;
    unsigned int ipv4_gtw_addr; /* To be compatible with the old-code-style for ipv4. */
    char ipv6_gtw_addr[IPV6_STANDARD_CHAR_LEN + 1] = {0};
    unsigned char convert_ipv6_gw[IPV6_IP_U8_LEN] = {0};

    if (ack_msg->ip_type != DEVDRV_IPV4 && ack_msg->ip_type != DEVDRV_IPV6) {
        return DRV_ERROR_NOT_SUPPORT;
    }
    /* ipv4 */
    if (ack_msg->ip_type == DEVDRV_IPV4) {
        ret = devdrv_get_ipv4_gw_by_ethx(ethname, &ipv4_gtw_addr);
        if (ret != 0) {
            if (ret != DRV_ERROR_NO_DEVICE) { /* Do not print error logs for setting gateway. */
                DEVDRV_DRV_ERR("Failed to invoke devdrv_get_ipv4_gw_by_ethx. (ethname=\"%s\")\n", ethname);
            }
            return ret;
        }
        ack_msg->gtw_addr.addr_v4 = ipv4_gtw_addr;
        /* ipv6 */
    } else {
        ret = devdrv_get_ipv6_gw_by_ethx(ethname, ipv6_gtw_addr);
        if (ret != 0) {
            DEVDRV_DRV_ERR("Failed to invoke devdrv_get_ipv6_gw_by_ethx. (ethname=\"%s\")\n", ethname);
            return ret;
        }
        ret = devdrv_ipv6_gw_format(ipv6_gtw_addr, convert_ipv6_gw, IPV6_IP_U8_LEN);
        if (ret != 0) {
            DEVDRV_DRV_ERR("Failed to invoke devdrv_ipv6_gw_format.\n");
            return ret;
        }
        ret = memcpy_s(&ack_msg->gtw_addr, sizeof(ipaddr_t), convert_ipv6_gw, IPV6_IP_U8_LEN);
        if (ret != 0) {
            DEVDRV_DRV_ERR("memcpy_s failed.\n");
            return DRV_ERROR_OUT_OF_MEMORY;
        }
    }

    return DRV_ERROR_NONE;
}

int devdrv_set_gateway_address(char *ethname, struct dmanager_gtw_info config_data, unsigned int ethname_len)
{
    (void)ethname_len;
    unsigned char gtw_addr[DEVDRV_MAX_IP_LEN];
    struct dmanager_gtw_info cur_data = {0};
    struct rtentry rte = {0};
    struct ifreq ifr;
    int ret;
    int fd = -1;

    ret = memset_s(&rte, sizeof(rte), 0, sizeof(rte));
    if (ret != 0) {
        DEVDRV_DRV_ERR("set rte to 0 fail, ret(%d).\n", ret);
        return ret;
    }

    (((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr).s_addr = config_data.gtw_addr.addr_v4;

    ret = strcpy_s((char *)gtw_addr, sizeof(gtw_addr), inet_ntoa(((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr));
    if (ret != 0) {
        DEVDRV_DRV_ERR("transfer config_data to ip_addr failed, ret(%d).\n", ret);
        return ret;
    }

    ret = devdrv_ip_format_check(config_data.ip_type, gtw_addr, DEVDRV_MAX_IP_LEN);
    if (ret != 0) {
        DEVDRV_DRV_ERR("ip format error, ret(%d).\n", ret);
        return ret;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        DEVDRV_DRV_ERR("open socket failed, %s.\n", strerror(errno));
        return fd;
    }

    rte.rt_dev = ethname;
    rte.rt_flags = RTF_UP | RTF_GATEWAY; /* use gateway */

    ((struct sockaddr_in *)&(rte.rt_dst))->sin_family = AF_INET;
    ((struct sockaddr_in *)&(rte.rt_dst))->sin_addr.s_addr = 0;

    ((struct sockaddr_in *)&(rte.rt_genmask))->sin_family = AF_INET;
    ((struct sockaddr_in *)&(rte.rt_genmask))->sin_addr.s_addr = 0;

    ((struct sockaddr_in *)&(rte.rt_gateway))->sin_family = AF_INET;

    ret = devdrv_get_gateway_address(ethname, &cur_data);
    if (ret == 0 && cur_data.gtw_addr.addr_v4 != 0) {
        ((struct sockaddr_in *)&(rte.rt_gateway))->sin_addr.s_addr = cur_data.gtw_addr.addr_v4;
        ret = ioctl(fd, SIOCDELRT, &rte);
        if (ret != 0) {
            DEVDRV_DRV_ERR("clear %s gateway failed, %s.\n", ethname, strerror(errno));
            (void)close(fd);
            return ret;
        }
    }

    ((struct sockaddr_in *)&(rte.rt_gateway))->sin_addr.s_addr = config_data.gtw_addr.addr_v4;
    ret = ioctl(fd, SIOCADDRT, &rte);
    if (ret != 0) {
        DEVDRV_DRV_ERR("set %s gateway failed, %s.\n", ethname, strerror(errno));
        (void)close(fd);
        return ret;
    }

    (void)close(fd);
    return DRV_ERROR_NONE;
}
