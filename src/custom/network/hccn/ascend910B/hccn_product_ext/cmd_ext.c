/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include "cmd.h"
#include "tool.h"
#include "tool_recovery.h"
#include "tool_ext.h"
#include "tool_cdr.h"

static struct cmd_info g_cmd_list_ext[] = {
    {"-i", "devid", "-roce_test", tool_roce_test_cmd_execute, "roce test cmd",
#ifdef HCCN_TOOL_ADAPT_IPV6
     "   [-inet6]             : use IPv6, when not specified default is IPv4\n"
#endif
     "        ib_read_bw      : roce read bandwidth test\n"
     "        ib_write_bw     : roce write bandwidth test\n"
     "        ib_send_bw      : roce send bandwidth test\n"
     "        ib_read_lat     : roce read latency test\n"
     "        ib_write_lat    : roce write latency test\n"
     "        ib_send_lat     : roce send latency test\n"
     "        reset           : kill device's all perftest process, reset perftest\n"
     "   [-d %s]              : set ib device\n"
#ifdef CFG_SOC_PLATFORM_CLOUD
     "   [-s %d]              : set msg size [1 ~ 2147483647]B\n"
#else
     "   [-s %d]              : set msg size [1 ~ 874736000]B\n"
     "   [-a]                 : run sizes from 2 till 2^23\n"
     "   [-b]                 : measure bidirectional bandwidth (default unidirectional)\n"
     "   [-f %d]              : measure results within margins [0 ~ D/2-1]\n"
     "   [-D %d]              : run test for a customized period of seconds (default 5) [1 ~ 500]\n"
     "   [-l %d]              : post list of WQEs of <list size> size (instead of single post) [1 ~ 1024]\n"
     "   [-m %d]              : mtu size : [256 ~ 4096] (default port mtu 256,512,1024,2048,4096)\n"
     "   [-ib %d]             : --ib-port=<port>  Use port <port> of IB device, and the value must be 1 (default 1)"
     "\n"
     "   [-p %d]              : --port=<port> Listen on/connect to port <port> (default 18515) [0 ~ 65535]\n"
     "   [-q %d]              : --qp=<num of qp's>  Num of qp's(default 1) [1 ~ 100]\n"
     "   [-Q %d]              : --cq-mod  Generate Cqe only after <--cq-mod> completion [1 ~ 1024]\n"
     "   [-t %d]              : --tx-depth=<dep>  Size of tx queue (default 128) [1 ~ 15000]\n"
     "   [-u %d]              : --qp-timeout=<timeout> timeout value is 4 usec * 2 ^(timeout) (default 14) [0 ~ 255]"
     "\n"
     "   [-V]                 : --version  Display version number\n"
     "   [-x %d]              : --gid-index=<index> Test uses GID with GID index (ROCEV2 is recommended) [0 ~ 5]\n"
     "   [-tclass %d]         : set number of tos[0 ~ 255]\n"
     "   [-report_gbits]      : Report Max/Average BW of test in Gbit/sec (instead of MiB/sec) (SYMMETRIC)\n"
     "                         Note: MiB=2^20 byte, while Gb=10^9 bits. Use these formulas for conversion:\n"
     "                         Factor=10^9/(2^20*8)=119.2; MiB=Gb_result * factor; Gb=MiB_result/factor\n"
     "   [-S %d]              : set service level(default 0) [0 ~ 7]\n"
     "   [-bind_source_ip %s] : Source IP of the interface used for connection establishment.\n"
     "                         By default taken from routing table.\n"
#endif
     "   [-n %d]              : set number of exchanges [5 ~ 20000]\n"
#ifdef HCCN_TOOL_ADAPT_IPV6
     "   [address %s]         : set service IPv4 or IPv6 address, such as x.x.x.x or x:x:x:x:x:x:x:x\n"
#else
     "   [address %s]         : set service ip address, such as x.x.x.x\n"
#endif
#ifdef CFG_SOC_PLATFORM_CLOUD
     "   [-tcp]               : set tcp connect, when not specified default is CM\n"
#else
     "   [-tcp]               : set tcp connect\n"
#endif
    },
    {"-i", "devid", "-speed", tool_speed_cmd_execute, "speed related cmd",
     "   [-g]        : get the speed of eth ports\n"},
    {"-i", "devid", "-duplex", tool_duplex_cmd_execute, "duplex related cmd",
     "   [-g]        : get the duplex mode of eth ports\n"},
    {"-i", "devid", "-fec", tool_fec_cmd_execute, "fec related cmd",
#ifndef CFG_SOC_PLATFORM_CLOUD_V2
     "   [-g]        : get the fec mode of eth ports\n"
     "   [-s]        : set the fec mode of eth ports\n"
     "       encoding %s : mode type, choose one of the following, auto | off | rs\n"
#else
     "   This device does not support switching fec mode\n"
#endif
    },

    {"-i", "devid", "-pfc", tool_pfc_cmd_execute, "pfc enable cfg cmd",
     "   [-s]        : set pfc cfg\n"
     "       bitmap %s  : set pfc cfg, each bit corresponds to one priority, such as 0,0,0,0,1,0,0,0\n"
     "   [-g]        : get pfc cfg\n"},
    {"-i", "devid", "-tc_cfg", tool_tc_cfg_cmd_execute, "traffic class cfg cmd",
     "   [-s]        : set tc cfg\n"
     "       size %s : set the buffer size of tc [0 ~ 7], size shall not be less than 6272 when it is non-zero, "
     "and total size must be smaller than 1797120, such as 816640,81920,81920,816640,0,0,0,0\n"
     "  waterline %s : set the waterline of tc [0 ~ 7], waterline must not be larger than size, "
     "such as 780800,46080,46080,780800,0,0,0,0\n"
     "   [-g]        : get tc cfg\n"},
    {"-i", "devid", "-cur_tc_buf", tool_tc_buf_cmd_execute, "maximum historical tc usage cmd",
     "   [-g]        : get the maximum tc Bufer currently and then clear the value\n"
     "       tc %d   : set the tc id [0 ~ 7]\n"},
    {"-i", "devid", "-prio_tc", tool_prio_tc_cmd_execute, "priority map TC cmd",
     "   [-s]        : set the mapping between each priority and TC, TC range [0 ~ 3]\n"
     "       map %s  : set the mapping between each priority and TC, "
     "each number corresponds to one TC, such as 0,0,0,0,0,0,0,0\n"
     "   [-g]        : get the mapping between priority and TC\n"},
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    {"-i", "devid", "-tc_qos", tool_tc_qos_cmd_execute, "TC QoS configuration cmd",
     "   [-s]              : set TC QoS parameters\n"
     "       tsa <params>  : set the scheduling algorithm for each Traffic Class,\n"
     "                       TC range: [0 ~ 3], TC count [1 ~ 4], valid types: 'ets' or 'strict'\n"
     "                       Example: 0:ets,1:strict,2:ets,3:strict\n"
     "       OR\n"
     "       tcbw <params> : set bandwidth allocation for ETS-enabled TCs (in percent)\n"
     "                       Sum of 'ets' tcbw must be = 100%, The 'strict' tcbw must be 0.\n"
     "   [-g]              : get current TC QoS settings\n"},
#endif
    {"-i", "devid", "-ping", tool_ping_cmd_execute, "ping cmd",
#ifdef HCCN_TOOL_ADAPT_IPV6
     "   [-inet6]            : use IPv6, when not specified default is IPv4\n"
#endif
     "   [-g]                : get ping result\n"
     "       address %s      : set net detect IPv4 ip address, such as x.x.x.x\n"
#ifdef HCCN_TOOL_ADAPT_IPV6
     "       ipv6_address %s : set net detect IPv6 ip address, such as x:x:x:x:x:x:x:x\n"
#endif
     "      pkt %d           : ping cfg, the packet data length, [0 ~ 65507]\n"},
    {"-i", "devid", "-send_arp", tool_send_gratuitous_arp_cmd_execute, "send gratuitous arp related cmd",
     "   [-s]        : set the setting of gratuitous arp\n"
     "       enable %s  : set the ability of the gratuitous arp, 0: disable, 1: enable,\n"
     "       or\n"
     "       period %s  : set the period of the gratuitous arp, [1 ~ 3600] (s)\n"
     "   [-g]        : get the setting of gratuitous arp\n"},
    {"-i", "devid", "-ip_rule", tool_ip_rule_cmd_execute, "ip rule cmd",
     "   [-g]        : get rule table\n"
     "   [-a]        : add rule\n"
     "        dir %s : from or to\n"
     "         ip %s : destination ip, such as x.x.x.x\n"
     "      table %s : table index, such as xx, [0 ~ 255]\n"
     "   [-d]        : del rule\n"
     "        dir %s : from or to\n"
     "         ip %s : destination ip, such as x.x.x.x\n"},
    {"-i", "devid", "-ip_route", tool_ip_route_cmd_execute, "ip route cmd",
     "    [-g]       : get route table\n"
     "      [table %s] : table index, such as xx, [0 ~ 255]\n"
     "    [-a]       : add rule\n"
     "         ip %s : ip addr, such as x.x.x.x\n"
     "    ip_mask %s : ip mask bit, such as x, [0 ~ 32]\n"
     "        via %s : forward ip, such as x.x.x.x\n"
     "        dev %s : ethernet device, such as ethx\n"
     "      table %s : table index, such as xx, [0 ~ 255]\n"
     "    [-d]       : del rule\n"
     "         ip %s : ip region, such as x.x.x.x\n"
     "    ip_mask %s : ip mask bit, such as x, [0 ~ 32]\n"
     "      table %s : table index, such as xx, [0 ~ 255]\n"},
    {"-i", "devid", "-serdes", tool_serdes_cmd_execute, "serdes related cmd",
     "   [-g]        : get the serdes info(snr) of eth ports\n"},
#ifdef HCCN_TOOL_SHOW_LIST
    {"-i", "devid", "-pcs", tool_pcs_cmd_execute, "pcs related cmd",
     "   [-g]        : get the pcs info of eth ports\n"},
    {"-i", "devid", "-backpressure", tool_bp_cmd_execute, "backpressure cmd",
     "   [-g]        : get backpressure status\n"},
    {"-i", "devid", "-adapt_status", tool_adapt_status_cmd_execute, "mac adapt status related cmd",
     "   [-g]        : get the mac adapt status info of eth ports\n"
     "   [port %s]   : set the port id [0 ~ 7]\n"},
    {"-i", "devid", "-cdr", tool_cdr_cmd_execute, "cdr related cmd",
     "     [-t/--type [0 ~ 3]]  0: trig to update cdr reg info\n"
     "                        1: get cdr reg info\n"
     "                        2: trig get ext info to store in cdr log\n"
     "                        3: get cdr log info\n"},
#endif
#ifdef HCCN_TOOL_ADAPT_IPV6
    {"-i", "devid", "-neigh", tool_neigh_table_cmd_execute, "neigh table cmd",
     "   [-inet6]             : use IPv6\n"
     "   [-g]                 : get neigh table\n"
     "   [-a]                 : add neigh \n"
     "        dev %s          : eth device, such as eth0\n"
     "        ipv6_address %s : IPv6 ip address, such as x:x:x:x:x:x:x:x\n"
     "        mac %s          : mac address, such as xx:xx:xx:xx:xx:xx\n"
     "   [-d]                 : del neigh \n"
     "        dev %s          : eth device, such as eth0\n"
     "        ipv6_address %s : IPv6 ip address, such as x:x:x:x:x:x:x:x\n"},
    {"-i", "devid", "-scdr", tool_sd_cdr_cmd_execute, "cdr related cmd",
     "   [-g]                : get cdr info\n"
     "   [-t/--type [0 ~ 8]]  0: stop eth adapt config\n"
     "                        1: start eth adapt config\n"
     "                        2: stop eth cdr loopback\n"
     "                        3: start eth cdr loopback\n"
     "                        4: get cdr reg info\n"
     "                        5: get cdr snr and history snr\n"
     "                        6: enable cdr reset\n"
     "                        7: disable cdr reset\n"
     "                        8: get huyang FMEA info\n"
     "   [-reg_read]            : read reg for scdr\n"
     "                address %s: scdr reg, needs to be a multiples of 4, such as 0x08000000\n"
     "                           the range is:[0x02000000, 0x02079FFF],\n"
     "                                        [0x04000000, 0x0400FFFF],\n"
     "                                        [0x08000000, 0x080FFFFF]\n"
     "                nums    %d: read nums, [1 ~ 62]\n"},
    {"-i", "devid", "-down_data", tool_down_data_cmd_execute, "eth down data cmd",
     "    [-g]       : get eth link down data\n"},
    {"-i", "devid", "-traceroute", tool_traceroute_cmd_execute, "traceroute cmd",
     "   [-inet6]            : use IPv6, when not specified default is IPv4\n"
     "   [-g]                : get the traceroute info\n"
     "       -m      %d      : max_llt, set the max number of hops (max TTL to be reached)\n"
     "                         Default is 30 [1 ~ 255]\n"
     "       -t      %d      : tos, set the TOS (IPv4 type of service: [0 ~ 63]) or\n"
     "                         TC (IPv6 traffic class : [0 ~ 255]) value for outgoing packets\n"
     "       -w      %d      : waittime, set the number of seconds to wait for\n"
     "                         response to a probe (default is 3) [1 ~ 60] s\n"
     "       -sport  %d      : set source port [0 ~ 65535]\n"
     "       -dport  %d      : set dest port [0 ~ 65535]\n"
     "       -d      %s      : trace the route to dest ip\n"
     "   reset               : kill device's all traceroute process, reset traceroute status\n"},
    {"-i", "devid", "-xsfp_reset", tool_xsfp_reset_cmd_execute, "xsfp reset cmd",
     "   [-g]                : get configuration of xsfp reset \n"
     "   [-s]                : set configuration of xsfp reset \n"
     "        max_times  %d  : set the max time of xsfp reset [0 ~ 30]\n"
     "        wait_times %d  : set the wait time of xsfp reset [0 ~ 40]*1.5s\n"},
    {"-i", "devid", "-stat_extra", tool_get_extra_statistics_cmd_execute, "packet extra statistics cmd",
     "   [-g]                : get packet extra statistics\n"
     "   [-c]                : clear packet extra statistics\n"},
    {"-i", "devid", "-fec_err", tool_get_fec_error_cmd_execute, "collect FEC error histogram statistics cmd",
     "   [-g]                : get FEC error histogram statistics\n"},
    {"-i", "devid", "-auto_downgrade", tool_downgrade_lane_cmd_execute, "downgrade lane cmd",
     "   [-g]                : get the downgrade lane status\n"
     "   [-s]                : set the configuration about downgrade lane\n"
     "        enable %d      : set the downgrade lane switch, 0:disable, 1:enable\n"
     "        filter_time %d : set the filter time (default 7s) of downgrade lane, [3 ~ 8] s\n"},
    {"-i", "devid", "-pfc_stat", tool_pfc_stat_cmd_execute, "pfc_stat cmd",
     "   [-g]        : get the pfc all priorities pause duration \n"
     "   [-c]        : clear the pfc all priorities pause duration\n"},
    {"-i", "devid", "-tc_stat", tool_tc_stat_cmd_execute, "packet statistics cmd",
     "   [-g]                : get packet statistics\n"
     "   [-c]                : clear packet statistics\n"},
    {"-i", "devid", "-hilink_info", tool_hilink_info_cmd_execute, "hilink info cmd",
     "     [-t/--type [0 ~ 1]]   0: get npu hilink stable register info\n"
     "                           1: get npu link down hilink register info\n"},
    {"-i", "devid", "-qp_info", tool_get_qp_info_cmd_execute, "qp info cmd",
     "   [-g]        : get info of all active qp\n"},
    {"-i", "devid", "-bad_thd", tool_roce_cw_bad_cnt_thd_cmd_execute, "roce cw_bad_cnt_thd cmd",
     "   [-g]                : get ROCE cw_bad_cnt_thd \n"
     "   [-s]                : set configuration of ROCE cw_bad_cnt_thd \n"
     "        high_limit %d  : set the max value of cw_bad_cnt_thd,  [3 ~ 14] (default 3)\n"
     "                   note: setting high_limit > 3 will increase bad codewords\n"},
    {"-i", "devid", "-pfc_storm", tool_pfc_storm_cmd_execute, "pfc storm watchdog cmd",
     "   [-g]                  : get the configuration parameters of the pfc storm watchdog\n"
     "   [-s]                  : set the configuration parameters of the pfc storm watchdog\n"
     "        mode           %d : the mode of the pfc storm watchdog (default 0)\n"
     "                            mode: 0 [Disable PFC storm detection]\n"
     "                            mode: 1 [Only warn and err counts arecounted, "
     "and no closing action is performed]\n"
     "                            mode: 2 [Enable PFC storm detection]\n"
     "        close_time     %d : duration of pfc storm watchdog automatic shutdown PFC [0 ~ 100](100ms)"
     " (default 1)\n"
     "        detect_period  %d : pfc storm watchdog detection period [10 ~ 100](100ms) (default 20)\n"
     "        warn_thrd      %d : threshold value for PFC storm watchdog alarm [40 ~ 80](%) (default 50)\n"
     "        err_thrd       %d : threshold value of PFC storm watchdog reporting error [60 ~ 100](%) (default 90)\n"
     "        close_time <= detect_period; warn_thrd < err_thrd\n"},
#endif
#ifdef CFG_SOC_PLATFORM_CLOUD_V2
    {"-i", "devid", "-hccs_ping", tool_ping_cmd_execute_ext, "supernodes ping cmd",
     "   [-g]                : get the supernodes ping result\n"
     "       address   %s    : set net detect IPv4 ip address, such as x.x.x.x\n"
     "       or\n"
     "       sdid      %s    : set net detect device sdid value. "
     "this value conflicts with address, one of the two is used\n"
     "       pkt       %d    : ping cfg, the packet data length, [1792 ~ 3000] (byte) (default 1792)\n"
     "       cnt       %d    : ping cfg, the total count of packet send, [1 ~ 1000] (default 3)\n"
     "       interval  %d    : ping cfg, the interval between receiving a reply packet and sending the next packet, "
     "[0 ~ 10] (ms) (default 0)\n"
     "       timeout   %d    : ping cfg, the timeout waiting time for each packet, [1 ~ 20000] (ms)\n"},
    {"-i", "devid", "-scdr_reset", tool_scdr_reset_cmd_execute, "scdr reset parameter query and config",
     "   [-g]                    : get cdr reset info.\n"
     "   [-s]                    : set cdr reset info.\n"
     "       delay_time %d       : cdr reset delay time, [0 ~ 60] (s) (default 0)\n"},
    {"-i", "devid", "-esd", tool_scdr_esd_cmd_execute, "cdr related cmd", "   [-g]        : get the scdr esd result\n"},
    {"-i", "devid", "-diagnostic", tool_diagnostic_cmd_execute, "diagnostic mode query and config",
     "   [-g]                        : query current diagnostic status.\n"
     "   [-s]                        : configure diagnostic mode and I2C operations.\n"
     "       enable on/off           : Control diagnostic mode.\n"
     "                                   - on: Enable diagnostic mode (default duration: 60 minutes).\n"
     "                                   - off: Disable diagnostic mode.\n"
     "       duration <time>         : Duration of active diagnostic in minutes, "
     "optional, [5 ~ 720] (default 60).\n"
     "       OR\n"
     "       i2c_mode read/write     : Perform direct I2C access to target device.\n"
     "                                   - read: Read from slave device\n"
     "                                   - write: Write to slave device, reg value must be provided.\n"
     "       object <name>           : target device, only xsfp/cpld is supported now\n"
     "           xsfp                : optical module\n"
     "               page <index>    : Page index of the target register space, "
     "such as 'page 10', 'page 0xa', [0x0 ~ 0xFF]\n"
     "               offset <index>  : Register offset within the specified page. "
     "[page0:0 ~ 255, other page:128 ~ 255]\n"
     "               size <index>    : Size of data, [read:1 ~ 128, write:1 ~ 4]\n"
     "               [value]         : Data value to write.\n"
     "                               : Maximum 4 bytes (32-bit) allowed.\n"
     "                               : Only applicable for 'write' operation.\n"
     "           OR\n"
     "           cpld                : CPLD (read only, will print all info at once)\n"},
    {"-i", "devid", "-optical_snr", tool_optical_snr_query_cmd_execute, "optical snr query info cmd",
     "   [-g]                        : get optical snr query info.\n"
     "   [-s]                        : set optical snr query info.\n"
     "       status enable/disable   : optical snr query switch status "
     "(only influence the optical not on the allowlist).\n"
     "                                 - enable: enable optical snr query switch.\n"
     "                                 - disable: disable optical snr query switch.\n"},
    {"-i", "devid", "-mem_reserve", tool_mem_reserve_cmd_execute, "memory reserve cmd",
     "   [-s]              : set memory reserve\n"
     "       type <type>   : process type, only support 'pdccl'\n"
     "       size <value>  : reserve size in MB, [0,2,4,8,16,32,64,128,256,512]\n"
     "   [-g]              : get memory reserve\n"},
    {"-i", "devid", "-dump", tool_dump_execute, "dump dfx info cmd",
     "  [-t]                         : dump target\n"
     "      adapt_info               : link state machine adapt info\n"},
#endif
};

int insert_ext_cmd(void)
{
    int cmd_num = sizeof(g_cmd_list_ext) / sizeof(g_cmd_list_ext[0]);

    return insert_cmd_list(g_cmd_list_ext, cmd_num);
}
