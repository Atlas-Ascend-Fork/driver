/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tool_recovery.h"
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <limits.h>
#include <errno.h>
#include "ds_net.h"
#include "dsmi_common_interface.h"
#include "ossl_user_linux.h"

static int tool_parse_exist_ip_rule_lines(struct ip_rule_exsit_struct *ip_rule_exsit)
{
    int ret = 0;
    int len = strlen(ip_rule_exsit->ip_rule_out_buf);
    int i;
    int count = 0;

    for (i = 0; i < len; i++) {
        if (strchr("\n", ip_rule_exsit->ip_rule_out_buf[i])) {
            (ip_rule_exsit->list_count)++;
        }
    }
    ip_rule_exsit->rule_list = (int *)calloc(ip_rule_exsit->list_count, sizeof(int));
    if (ip_rule_exsit->rule_list == NULL) {
        roce_err("Tool parse rule list calloc rule list failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }
    ip_rule_exsit->same_list = (int *)calloc(ip_rule_exsit->list_count, sizeof(int));
    if (ip_rule_exsit->same_list == NULL) {
        roce_err("Tool parse rule list calloc same rule failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    for (i = 0; i < len; i++) {
        if (strchr("\n", ip_rule_exsit->ip_rule_out_buf[i])) {
            ip_rule_exsit->rule_list[count] = i;
            count++;
        }
    }

    return ret;
}

static int tool_get_exist_ip_rule_check(const char *prefix, const char *cfg, struct ip_rule_exsit_struct *ip_rule_exsit)
{
    int ret, i;
    char dir[MAX_PARAM_LEN] = {0};
    char ip_str[MAX_PARAM_LEN] = {0};
    char table_str[MAX_PARAM_LEN] = {0};
    unsigned int table_id;
    char *p = NULL;
    char cmdline[INFO_PAYLOAD_LEN] = {0};
    char slice_str[INFO_PAYLOAD_LEN];
    int slice_start = 0;
    int slice_length = 0;

    ret = strncpy_s(cmdline, INFO_PAYLOAD_LEN, cfg, strlen(cfg));
    p = &cmdline[strlen(prefix)];
    ret = sscanf_s(p, "add %s %s table %u", dir, sizeof(dir), ip_str, sizeof(ip_str), &table_id);
    if (ret <= 0) {
        roce_err("Tool exsit ip rule recovery get param failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    ret = sprintf_s(table_str, MAX_PARAM_LEN, "lookup %u", table_id);
    if (ret <= 0) {
        roce_err("Sprintf ip rule table id failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }
    ret = 0;
    for (i = 0; i < ip_rule_exsit->list_count; i++) {
        if (ip_rule_exsit->same_list[i] != IP_RULE_EXSIT_FLAG) {
            slice_length = ip_rule_exsit->rule_list[i] - slice_start;
            ret = strncpy_s(slice_str, INFO_PAYLOAD_LEN, ip_rule_exsit->ip_rule_out_buf + slice_start, slice_length);
            if (ret != 0) {
                roce_err("get exsit ip rule check strcpy slice_str failed. (ret=%d)", ret);
                return ret;
            }
            slice_str[slice_length] = '\0';
            if (strstr(slice_str, dir) != NULL && strstr(slice_str, ip_str) != NULL &&
                strstr(slice_str, table_str) != NULL) {
                ip_rule_exsit->same_list[i] = IP_RULE_EXSIT_FLAG;
                ret = IP_RULE_EXSIT_FLAG;
                break;
            }
        }
        slice_start = ip_rule_exsit->rule_list[i];
    }

    return ret;
}

static int tool_get_ip_rule_for_recovery_str(int logic_id, struct ip_rule_exsit_struct *ip_rule_exsit)
{
    int ret;
    struct send_data_info data_info;
    struct ip_rule_cmd_info cmd_info = {0};

    cmd_info.cmd_type = 'g';
    TOOL_SET_SEND_DATA(data_info, DS_IP_RULE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);
    data_info.out_buf = (char *)calloc(RDFX_EXT_REPLY_MAX_LEN, sizeof(char));
    if (data_info.out_buf == NULL) {
        roce_err("Tool get rule calloc out buf failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }
    data_info.size_out = RDFX_EXT_REPLY_MAX_LEN;

    ret = dsmi_send_data(logic_id, 0, &data_info);
    if (ret) {
        free(data_info.out_buf);
        roce_err("Dsmi send data failed. (ret=%d; logic_id=%d)", ret, logic_id);
        return dsmi_analysis_dsmi_ret_to_uda(ret);
    }
    data_info.out_buf[RDFX_EXT_REPLY_MAX_LEN - 1] = '\0';
    ip_rule_exsit->ip_rule_out_buf = (char *)calloc(ip_rule_exsit->ip_rule_out_buf_size, sizeof(char));
    if (ip_rule_exsit->ip_rule_out_buf == NULL) {
        roce_err("Tool get ip rule recovery str calloc ip rule out buf failed.");
        free(data_info.out_buf);
        return UDA_TOOL_NO_MEM_ERR;
    }
    ret = strncpy_s(ip_rule_exsit->ip_rule_out_buf, ip_rule_exsit->ip_rule_out_buf_size, data_info.out_buf,
                    data_info.size_out);
    if (ret != 0) {
        roce_err("get ip rule recovery strcpy cfg failed. (ret=%d)", ret);
        free(data_info.out_buf);
        return ret;
    }

    free(data_info.out_buf);

    return ret;
}

static void free_ip_rule_exsit_param_struct(struct ip_rule_exsit_struct *ip_rule_exsit_param)
{
    if (ip_rule_exsit_param->rule_list != NULL) {
        free(ip_rule_exsit_param->rule_list);
        ip_rule_exsit_param->rule_list = NULL;
    }
    if (ip_rule_exsit_param->same_list != NULL) {
        free(ip_rule_exsit_param->same_list);
        ip_rule_exsit_param->same_list = NULL;
    }
    if (ip_rule_exsit_param->ip_rule_out_buf != NULL) {
        free(ip_rule_exsit_param->ip_rule_out_buf);
        ip_rule_exsit_param->ip_rule_out_buf = NULL;
    }

    return 0;
}

static int tool_open_cfg_file(FILE **fp)
{
    int ret;
    *fp = fopen(CFGFILE_NAME, "r");
    if (*fp == NULL) {
        roce_err("Open configure file failed. (file=\"%s\")", CFGFILE_NAME);
        return UDA_TOOL_SYS_FOPEN_ERR;
    }
    ret = flock(fileno(*fp), LOCK_EX);
    if (ret != 0) {
        roce_err("Lock configure file failed. (ret=%d; errno=%d)", ret, errno);
        fclose(*fp);
        return UDA_TOOL_SYS_FOPEN_ERR;
    }
    ret = fseek(*fp, 0, SEEK_SET);
    if (ret != 0) {
        roce_err("Fseek configure file failed. (ret=%d; errno=%d)", ret, errno);
        fclose(*fp);
        return UDA_TOOL_SYS_READ_FILE_ERR;
    }
    return ret;
}

static int tool_cfg_cmdline_recovery_check(const char *prefix, int max_len, struct tool_param *param,
                                           int (*recovery)(const char *, struct tool_param *param))
{
    FILE *fp = NULL;
    char line_buf[CONLINE_LENGTH] = {0};
    unsigned int tmp;
    int ret;
    struct ip_rule_exsit_struct ip_rule_exsit_param = {NULL, RDFX_EXT_REPLY_MAX_LEN, NULL, NULL, 0};

    ret = tool_open_cfg_file(&fp);
    if (ret) {
        return ret;
    }

    ret = tool_get_ip_rule_for_recovery_str(param->logic_id, &ip_rule_exsit_param);
    if (ret) {
        roce_err("get ip rule for recovery check failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        goto err_free;
    }
    ret = tool_parse_exist_ip_rule_lines(&ip_rule_exsit_param);
    if (ret) {
        roce_err("tool parse exist ip rule lines failed. (ret=%d; logic_id=%d)", ret, param->logic_id);
        goto err_free;
    }
    while (!feof(fp) && fgets(line_buf, CONLINE_LENGTH, fp) != NULL) {
        if (strstr(line_buf, prefix) != NULL) {
            tmp = (unsigned int)tool_get_exist_ip_rule_check(prefix, line_buf, &ip_rule_exsit_param);
            if (tmp == IP_RULE_EXSIT_FLAG) {
                continue;
            } else if (tmp) {
                roce_err("exist ip rule check failed. (ret=%d, cmdline=\"%s\")", ret, line_buf);
                ret = (int)tmp;
                break;
            }
            ret = recovery(line_buf, param);
            if (ret != 0) {
                roce_err("Recovery configure failed. (ret=%d, cmdline=\"%s\")", ret, line_buf);
                break;
            }
        }
    }

err_free:
    tmp = (unsigned int)ret;
    tmp |= (unsigned int)flock(fileno(fp), LOCK_UN);
    ret = (int)tmp;
    if (ret != 0) {
        roce_err("Unlock configure file failed. (ret=%d; errno=%d)", ret, errno);
    }
    free_ip_rule_exsit_param_struct(&ip_rule_exsit_param);
    (void)fclose(fp);
    return ret;
}

static int tool_send_recovery_ip_rule_cmd(int logic_id, const char *cmdline)
{
    int ret;
    struct send_data_info data_info;
    struct ip_rule_cmd_info cmd_info = {0};
    char dir[MAX_PARAM_LEN] = {0};
    unsigned char ip_fst, ip_sed, ip_thd, ip_fth;

    ret = sscanf_s(cmdline, "add %s %hhu.%hhu.%hhu.%hhu table %u", dir, sizeof(dir), &ip_fst, &ip_sed, &ip_thd, &ip_fth,
                   &cmd_info.table_id);
    if (ret <= 0) {
        roce_err("Tool ip rule recovery get param failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    if (strncmp(dir, "from", strlen("from") + 1) == 0) {
        cmd_info.dir = IP_RULE_DIR_FROM;
    } else if (strncmp(dir, "to", strlen("to") + 1) == 0) {
        cmd_info.dir = IP_RULE_DIR_TO;
    } else {
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    cmd_info.cmd_type = 's';
    cmd_info.ip_address = ip_fst + (ip_sed << 0x8) + (ip_thd << 0x10) + (ip_fth << 0x18);

    if (cmd_info.table_id > MAX_IP_ROUTE_TAB_IDX) {
        roce_err("Invalid table index in ip rule cmd. (table_index=%d)", cmd_info.table_id);
        return UDA_PARAM_INVALID_ERR;
    }

    TOOL_SET_SEND_DATA(data_info, DS_IP_RULE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);

    ret = dsmi_send_data(logic_id, 0, &data_info);
    if (ret != 0) {
        roce_err("Tool ip rule recovery dsmi send data fail! ret[%d] logic_id[%d]", ret, logic_id);
        ret = dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return ret;
}

static int tool_send_recovery_ip_route_cmd(int logic_id, const char *cmdline)
{
    int ret;
    struct send_data_info data_info;
    struct ip_route_cmd_info cmd_info = {0};
    struct ip_route_cmd_param cmd_param;
    unsigned char ip_fst, ip_sed, ip_thd, ip_fth;
    unsigned char via_fst, via_sed, via_thd, via_fth;

    ret = memset_s(&cmd_param, sizeof(cmd_param), 0, sizeof(cmd_param));
    if (ret != 0) {
        roce_err("Tool ip route recovery memset param failed.");
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    ret = sscanf_s(cmdline, "add %hhu.%hhu.%hhu.%hhu/%u via %hhu.%hhu.%hhu.%hhu dev eth%u table %u", &ip_fst, &ip_sed,
                   &ip_thd, &ip_fth, &cmd_info.ip_mask, &via_fst, &via_sed, &via_thd, &via_fth, &cmd_info.eth_id,
                   &cmd_info.table_id);
    if (ret <= 0) {
        roce_err("Tool ip route recovery get param failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    cmd_info.ip_address = ip_fst + (ip_sed << 0x8) + (ip_thd << 0x10) + (ip_fth << 0x18);
    cmd_info.via_address = via_fst + (via_sed << 0x8) + (via_thd << 0x10) + (via_fth << 0x18);
    cmd_info.cmd_type = 's';

    if (cmd_info.table_id > MAX_IP_ROUTE_TAB_IDX) {
        roce_err("Invalid table index in ip route cmd. (table_index=%d)", cmd_info.table_id);
        return UDA_PARAM_INVALID_ERR;
    }

    TOOL_SET_SEND_DATA(data_info, DS_IP_ROUTE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);

    ret = dsmi_send_data(logic_id, 0, &data_info);
    if (ret != 0 && ret != EEXIST) {
        roce_err("Tool ip route recovery dsmi send data failed. (ret[%d]; logic_id[%d])", ret, logic_id);
        ret = dsmi_analysis_dsmi_ret_to_uda(ret);
    }

    return ret;
}

STATIC int tool_send_ip_rule_recovery_cmd(const char *cfg, struct tool_param *param)
{
    char *p = NULL;
    int ret;
    int i;

    char cmdline[INFO_PAYLOAD_LEN] = {0};

    ret = strncpy_s(cmdline, INFO_PAYLOAD_LEN, cfg, strlen(cfg));
    if (ret != 0) {
        roce_err("Tool ip rule recovery strcpy cfg failed. (ret=%d)", ret);
        return ret;
    }

    for (i = 0; i < INFO_PAYLOAD_LEN; i++) {
        if (cmdline[i] == '=') {
            p = &cmdline[i];
            break;
        }
    }

    if (p == NULL) {
        roce_err("Tool ip rule recovery cfg error.");
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    p++;
    ret = tool_send_recovery_ip_rule_cmd(param->logic_id, p);

    return ret;
}

STATIC int tool_send_ip_route_recovery_cmd(const char *cfg, struct tool_param *param)
{
    char *p = NULL;
    int ret;
    int i;

    char cmdline[INFO_PAYLOAD_LEN] = {0};

    ret = strncpy_s(cmdline, INFO_PAYLOAD_LEN, cfg, strlen(cfg));
    if (ret != 0) {
        roce_err("Tool ip route recovery strcpy cfg failed. (ret=%d)", ret);
        return ret;
    }

    for (i = 0; i < INFO_PAYLOAD_LEN; i++) {
        if (cmdline[i] == '=') {
            p = &cmdline[i];
            break;
        }
    }

    if (p == NULL) {
        roce_err("Tool ip route recovery cfg error.");
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    p++;

    ret = tool_send_recovery_ip_route_cmd(param->logic_id, p);
    // 如果ip_route已经存在就不再重复配置
    ret = (ret == EEXIST) ? 0 : ret;
    if (ret != 0) {
        roce_err("Tool ip route recovery failed. (ret=%d; err_str=\"%s\")", ret, get_cmd_err_str(ret));
    }
    return ret;
}

int tool_cfg_ip_rule_recovery(struct tool_param *param)
{
    char ip_rule_prefix[CONLINE_LENGTH] = {0};
    int ret;

    ret = sprintf_s(ip_rule_prefix, CONLINE_LENGTH, "ip_rule_%d=", param->phy_id);
    if (ret <= 0) {
        roce_err("Set ip rule prefix failed. (ret=%d)", ret);
        return ret;
    }

    ret = tool_cfg_cmdline_recovery_check(ip_rule_prefix, CONLINE_LENGTH, param, tool_send_ip_rule_recovery_cmd);

    return ret;
}

int tool_cfg_ip_route_recovery(struct tool_param *param)
{
    char ip_route_prefix[CONLINE_LENGTH] = {0};
    int ret;

    ret = sprintf_s(ip_route_prefix, CONLINE_LENGTH, "ip_route_%d=", param->phy_id);
    if (ret <= 0) {
        roce_err("Set ip route prefix failed. (ret=%d)", ret);
        return ret;
    }

    ret = tool_cfg_cmdline_recovery(ip_route_prefix, CONLINE_LENGTH, param, tool_send_ip_route_recovery_cmd);

    return ret;
}

int tool_add_ip_rule_conf(int phy_id, const char *cmd)
{
    int ret;
    char cmd_buf[CONLINE_LENGTH] = {0};

    ret = sprintf_s(cmd_buf, CONLINE_LENGTH, "ip_rule_%d=%s\n", phy_id, cmd);
    if (ret <= 0) {
        roce_err("Add ip rule sprintf cmd failed. (ret=%d)", ret);
        return ret;
    }
    ret = handle_conf_to_file(cmd_buf, FILE_OPT_TYPE_APPEND_WRITE, NULL, 0);

    return ret;
}

int tool_del_ip_rule_conf(int phy_id, const char *dir, const char *ip)
{
#define IP_RULE_DEL_MATCH_CNT 3
    int ret;
    char match_list[IP_RULE_DEL_MATCH_CNT][CONLINE_LENGTH];
    unsigned int tmp;

    ret = memset_s(match_list, sizeof(match_list), 0, sizeof(match_list));
    if (ret != 0) {
        roce_err("Del ip rule memset match list failed. (ret=%d)", ret);
        return ret;
    }
    ret = sprintf_s(match_list[0], CONLINE_LENGTH, "ip_rule_%d=", phy_id);
    if (ret <= 0) {
        roce_err("Del ip rule sprintf match list failed. (ret=%d)", ret);
        return ret;
    }
    tmp = (unsigned int)strcpy_s(match_list[ARGC_ID_1], CONLINE_LENGTH, dir);
    tmp |= (unsigned int)strcpy_s(match_list[ARGC_ID_2], CONLINE_LENGTH, ip);
    ret = (int)tmp;
    if (ret != 0) {
        roce_err("Del ip rule strcpy match list failed. (ret=%d)", ret);
        return ret;
    }
    ret = handle_conf_to_file(NULL, FILE_OPT_TYPE_MATCH_DEL, match_list, IP_RULE_DEL_MATCH_CNT);
    return ret;
}

int tool_add_ip_route_conf(int phy_id, const char *cmd)
{
    int ret;
    char cmd_buf[CONLINE_LENGTH] = {0};

    ret = sprintf_s(cmd_buf, CONLINE_LENGTH, "ip_route_%d=%s\n", phy_id, cmd);
    if (ret <= 0) {
        roce_err("Add ip route sprintf cmd failed. (ret=%d)", ret);
        return ret;
    }
    ret = handle_conf_to_file(cmd_buf, FILE_OPT_TYPE_APPEND_WRITE, NULL, 0);

    return ret;
}

int tool_del_ip_route_conf(int phy_id, const char *ip, const char *ip_mask, const char *table)
{
#define IP_ROUTE_DEL_MATCH_CNT 3
    int ret;
    char match_list[IP_ROUTE_DEL_MATCH_CNT][CONLINE_LENGTH];
    unsigned int tmp;

    ret = memset_s(match_list, sizeof(match_list), 0, sizeof(match_list));
    if (ret != 0) {
        roce_err("Del ip route memset match list failed. (ret=%d)", ret);
        return ret;
    }
    tmp = (unsigned int)sprintf_s(match_list[0], CONLINE_LENGTH, "ip_route_%d=", phy_id);
    tmp |= (unsigned int)sprintf_s(match_list[ARGC_ID_1], CONLINE_LENGTH, "%s/%s", ip, ip_mask);
    tmp |= (unsigned int)sprintf_s(match_list[ARGC_ID_2], CONLINE_LENGTH, "table %s", table);
    ret = (int)tmp;
    if (ret <= 0) {
        roce_err("Del ip route sprintf match list failed. (ret=%d)", ret);
        return ret;
    }
    ret = handle_conf_to_file(NULL, FILE_OPT_TYPE_MATCH_DEL, match_list, IP_ROUTE_DEL_MATCH_CNT);

    return ret;
}

char *get_cmd_err_str(int err_code)
{
    int cnt;
    int i;
    const struct err_code g_err_code_map[] = {
        {"No such file or directory", ENOENT},          /* 2 */
        {"Permission denied", EACCES},                  /* 13 */
        {"Invalid argument", EFAULT},                   /* 14 */
        {"File exists", EEXIST},                        /* 17 */
        {"Network is unreachable", ENETUNREACH},        /* 114 */
        {"Nexthop has invalid gateway", EADDRNOTAVAIL}, /* 125 */
    };

    cnt = sizeof(g_err_code_map) / sizeof(g_err_code_map[0]);
    for (i = 0; i < cnt; i++) {
        if (err_code == g_err_code_map[i].err_val) {
            return g_err_code_map[i].err_name;
        }
    }

    return NULL;
}

// 将IPV6地址中省略的0补上
int change_ip_form(char *ip, int ip_len)
{
    char change_ip[MAX_IP6_LEN] = {0};
    // loc冒号或者最后一个\0的坐标 loc_tmp每断四个16进制的起始坐标
    int add_len = 0, total_add_len = 0, part_len = 0, ret = 0, loc = 0, loc_tmp = 0;
    char *find = NULL;
    if (ip == NULL) {
        return;
    }

    while (1) {
        find = strstr(ip + loc, ":");
        if (find != NULL) {
            loc_tmp = strstr(ip + loc, ":") - ip;
        } else {
            loc_tmp = strlen(ip);
        }
        part_len = loc_tmp - loc;
        if (part_len == 4 || part_len == 0) { // ipv6一段完整的是4个16进制申数字
            ret = memcpy_s(change_ip + loc + total_add_len, part_len + 1, ip + loc, part_len + 1);
            if (ret != 0) {
                roce_err("Memcpy no change ip failed. (ret=%d)", ret);
                return ret;
            }
            loc = ++loc_tmp;
        } else if (part_len < 4) {    // 小于4需要补0
            add_len = 4 - (part_len); // ipv6一段完整的是4个16进制申数字，不够的需要补齐0
            ret = memset_s(change_ip + loc + total_add_len, add_len, '0', add_len);
            if (ret != 0) {
                roce_err("Memset changed ip add 0 failed. (ret=%d)", ret);
                return ret;
            }
            ret = memcpy_s(change_ip + loc + add_len + total_add_len, part_len + 1, ip + loc, part_len + 1);
            if (ret != 0) {
                roce_err("Memcpy changed ip failed. (ret=%d)", ret);
                return ret;
            }
            total_add_len += add_len;
            loc = ++loc_tmp;
        } else {
            return;
        }
        if (find == NULL) {
            break;
        }
    }
    ret = memcpy_s(ip, ip_len, change_ip, MAX_IP6_LEN);
    if (ret != 0) {
        roce_err("Memcpy total failed. (ret=%d)", ret);
        return ret;
    }
    return 0;
}

STATIC int rule_list_match(char list[][CONLINE_LENGTH], int list_cnt, const char *dst)
{
    int i, len, ret;
    char *ipv6_str = NULL;
    char ip_tmp[MAX_IP6_LEN] = {0};
    char ipv6_conf[CONLINE_LENGTH] = {0};
    int copylen, copylen1, ip_len_before, ip_len_after;
    if (list == NULL || list_cnt <= 0) {
        return -EINVAL;
    }
    if (strstr(dst, "IPv6neigh") != NULL) {
        copylen = (int)(strstr(dst, "-s ") - dst + strlen("-s "));
        ret = memcpy_s(ipv6_conf, CONLINE_LENGTH, dst, copylen);
        if (ret != 0) {
            roce_err("Memcpy before ipv6 string failed. (ret=%d)", ret);
            return ret;
        }
        ipv6_str = strstr(dst, "-s ") + strlen("-s ");
        len = strstr(ipv6_str, " ") - ipv6_str;
        ret = memcpy_s(ip_tmp, MAX_IP6_LEN, ipv6_str, len);
        if (ret != 0) {
            roce_err("Memcpy ipv6 string failed. (ret=%d)", ret);
            return ret;
        }
        ip_len_before = strlen(ip_tmp);
        ret = change_ip_form(ip_tmp, MAX_IP6_LEN);
        if (ret != 0) {
            roce_err("Change ip failed. (ret=%d)", ret);
            return ret;
        }
        ip_len_after = strlen(ip_tmp);
        ret = memcpy_s(ipv6_conf + copylen, ip_len_after, ip_tmp, ip_len_after);
        if (ret != 0) {
            roce_err("Memcpy ipv6 string failed. (ret=%d)", ret);
            return ret;
        }
        copylen1 = strlen(dst) - copylen - ip_len_before;
        ret = memcpy_s(ipv6_conf + copylen + ip_len_after, copylen1, dst + copylen + ip_len_before, copylen1);
        if (ret != 0) {
            roce_err("Memcpy after ipv6 string failed. (ret=%d)", ret);
            return ret;
        }
        dst = ipv6_conf;
    }
    for (i = 0; i < list_cnt; i++) {
        if (list[i] != NULL && strstr(dst, list[i]) == NULL) {
            return -1;
        }
    }

    return 0;
}

STATIC int handle_conf_to_file_proc_buf(FILE *fp, int op_type, char match_list[][CONLINE_LENGTH], int list_cnt,
                                        char *file_buf)
{
    int ret;
    int find = false;
    char line_buf[CONLINE_LENGTH] = {0};

    while (!feof(fp) && fgets(line_buf, CONLINE_LENGTH, fp) != NULL) {
        if ((op_type != FILE_OPT_TYPE_APPEND_WRITE) && (find == false) &&
            (rule_list_match(match_list, list_cnt, line_buf) == 0)) {
            find = true;
            continue;
        } else {
            ret = strcat_s(file_buf, CONFILE_LENGTH, line_buf);
            if (ret != 0) {
                roce_err("Strcat configure file failed. (ret=%d; errno=%d)", ret, errno);
                return -EINVAL;
            }
        }
    }
    return 0;
}

int handle_conf_to_file(const char *cfg, int op_type, char match_list[][CONLINE_LENGTH], int list_cnt)
{
    int ret;
    FILE *fp = NULL;
    char *file_buf = (char *)calloc(1, CONFILE_LENGTH);

    if (file_buf == NULL) {
        roce_err("No memory, malloc failed.");
        return UDA_TOOL_NO_MEM_ERR;
    }

    fp = fopen(CFGFILE_NAME, "r+");
    if (fp == NULL) {
        roce_err("Open configure file failed. (file=\"%s\")", CFGFILE_NAME);
        ret = UDA_TOOL_SYS_FOPEN_ERR;
        goto free_buf;
    }

    if (flock(fileno(fp), LOCK_EX) != 0) {
        roce_err("Lock configure file failed. (errno=%d)", errno);
        ret = UDA_TOOL_SYS_FOPEN_ERR;
        goto close_file;
    }

    ret = handle_conf_to_file_proc_buf(fp, op_type, match_list, list_cnt, file_buf);
    if (ret != 0) {
        goto out;
    }

    if ((op_type == FILE_OPT_TYPE_APPEND_WRITE) || (op_type == FILE_OPT_TYPE_MATCH_REPLACES)) {
        ret = strcat_s(file_buf, CONFILE_LENGTH, cfg);
        if (ret != 0) {
            ret = UDA_TOOL_INSUFFICIENT_FILE_SIZE_ERR;
            roce_err("Strcat configure file failed. (ret=%d; errno=%d)", ret, errno);
            goto out;
        }
    }

    ftruncate(fileno(fp), 0);
    (void)fseek(fp, 0, SEEK_SET);
    ret = fputs(file_buf, fp);
    if (ret < 0) {
        roce_err("Write configure file failed. (ret=%d)\n", ret);
    } else {
        ret = 0;
    }
out:
    if (flock(fileno(fp), LOCK_UN) != 0) {
        roce_warn("Unlock configure file failed. (errno=%d)", errno);
    }
close_file:
    (void)fclose(fp);
free_buf:
    free(file_buf);
    return ret;
}

int tool_cfg_cmdline_recovery(const char *prefix, int max_len, struct tool_param *param,
                              int (*recovery)(const char *, struct tool_param *param))
{
    FILE *fp = NULL;
    int ret;
    char line_buf[CONLINE_LENGTH] = {0};
    unsigned int tmp;

    fp = fopen(CFGFILE_NAME, "r");
    if (fp == NULL) {
        roce_err("Open configure file failed. (file=\"%s\")", CFGFILE_NAME);
        return UDA_TOOL_SYS_FOPEN_ERR;
    }
    ret = flock(fileno(fp), LOCK_EX);
    if (ret != 0) {
        roce_err("Lock configure file failed. (ret=%d; errno=%d)", ret, errno);
        fclose(fp);
        return UDA_TOOL_SYS_FOPEN_ERR;
    }
    ret = fseek(fp, 0, SEEK_SET);
    if (ret != 0) {
        roce_err("Fseek configure file failed. (ret=%d; errno=%d)", ret, errno);
        fclose(fp);
        return UDA_TOOL_SYS_READ_FILE_ERR;
    }
    while (!feof(fp) && fgets(line_buf, CONLINE_LENGTH, fp) != NULL) {
        if (strstr(line_buf, prefix) != NULL) {
            ret = recovery(line_buf, param);
            if (ret != 0) {
                roce_err("Recovery configure failed. (ret=%d, cmdline=\"%s\")", ret, line_buf);
                break;
            }
        }
    }

    tmp = (unsigned int)ret;
    tmp |= (unsigned int)flock(fileno(fp), LOCK_UN);
    ret = (int)tmp;
    if (ret != 0) {
        roce_err("Unlock configure file failed. (ret=%d; errno=%d)", ret, errno);
    }

    (void)fclose(fp);
    return ret;
}

static int tool_send_recovery_neigh_cmd(int logic_id, char *cmdline)
{
    int ret;
    struct send_data_info data_info;
    struct ds_neigh_table_value cmd_info = {0};
    char ipv6_address[IP6_ADDR_LEN] = {0};
    unsigned int len_cmdline;

    len_cmdline = strlen(cmdline) - 1;
    if ((len_cmdline < INFO_PAYLOAD_LEN) && (cmdline[len_cmdline] == '\n')) {
        cmdline[len_cmdline] = '\0';
    }

    ret = sscanf_s(cmdline, "-i eth%u -s %s %hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &cmd_info.eth_id, ipv6_address,
                   sizeof(ipv6_address), &cmd_info.mac_address[INDEX_ZERO], &cmd_info.mac_address[INDEX_ONE],
                   &cmd_info.mac_address[INDEX_TWO], &cmd_info.mac_address[INDEX_THREE],
                   &cmd_info.mac_address[INDEX_FOUR], &cmd_info.mac_address[INDEX_FIVE]);
    if (ret <= 0) {
        roce_err("Tool neigh recovery get param failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = inet_pton(AF_INET6, ipv6_address, &cmd_info.ip6_address);
    if (ret <= 0) {
        roce_err("inet_pton address fail! ret[%d]", ret);
        return UDA_PARAM_IP_INVALID_ERR;
    }

    cmd_info.cmd_type = 'd';

    TOOL_SET_SEND_DATA(data_info, DS_SET_NEIGH_TABLE_CFG, (char *)(uintptr_t)&cmd_info, sizeof(cmd_info), NULL, 0);

    (void)dsmi_send_data(logic_id, 0, &data_info);
    // 对一个ip不能重复配置neigh，若需要更新，必须先删除旧配置，再配新neigh
    cmd_info.cmd_type = 's';
    ret = dsmi_send_data(logic_id, 0, &data_info);
    if (ret != 0) {
        roce_err("Tool neigh recovery dsmi send data fail! ret[%d] logic_id[%d]", ret, logic_id);
    }

    return ret;
}

int tool_send_neigh_recovery_cmd(const char *cfg, struct tool_param *param)
{
    char *p = NULL;
    int ret;
    char cmdline[INFO_PAYLOAD_LEN] = {0};

    ret = strncpy_s(cmdline, INFO_PAYLOAD_LEN, cfg, strlen(cfg));
    if (ret) {
        roce_err("Tool neigh recovery strncpy cfg failed. (ret=%d)", ret);
        return ret;
    }

    p = strtok(cmdline, "=");
    if (p == NULL) {
        roce_err("Strtok get p failed.");
        return UDA_TOOL_INNER_PARAM_ERR;
    }

    p = strtok(NULL, "=");
    /* del the end \n symbol */
    if (p == NULL) {
        roce_err("Strtok get p failed.");
        return UDA_TOOL_INNER_PARAM_ERR;
    }
    p[strlen(p) - 1] = '\0';

    ret = tool_send_recovery_neigh_cmd(param->logic_id, p);

    return ret;
}

int tool_cfg_neigh_recovery(struct tool_param *param)
{
#ifdef HCCN_TOOL_ADAPT_IPV6
    char neigh_prefix[CONLINE_LENGTH] = {0};
    int ret;

    ret = sprintf_s(neigh_prefix, CONLINE_LENGTH, "IPv6neigh_%d=", param->phy_id);
    if (ret <= 0) {
        roce_err("Set neigh prefix failed. (ret=%d)", ret);
        return UDA_TOOL_NO_MEM_ERR;
    }

    ret = tool_cfg_cmdline_recovery(neigh_prefix, CONLINE_LENGTH, param, tool_send_neigh_recovery_cmd);

    return ret;
#else
    return 0;
#endif
}