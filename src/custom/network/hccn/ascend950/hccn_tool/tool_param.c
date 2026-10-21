/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "errno.h"
#include "securec.h"
#include "hccn_err.h"
#include "hccn_log.h"
#include "hccn_comm.h"
#include "tcpip_cmd_data.h"
#include "shell_cmd_data.h"
#include "tool_param.h"

int parse_int(struct sub_option_entry *self, const char *val, value_union *type_val)
{
    char *endptr = NULL;
    long int result;

    if (val == NULL || type_val == NULL) {
        hccn_err("input val or type_val is null");
        return -EINVAL;
    }

    result = strtol(val, &endptr, 0);
    if (endptr == val || *endptr != '\0') {
        hccn_err("invalid int value: %s", val);
        return -EINVAL;
    }

    type_val->int_val = (int)result;
    type_val->is_set = true;
    return 0;
}

int parse_ip(struct sub_option_entry *self, const char *val, value_union *type_val)
{
    union addr_t in = {0};
    int ret;

    if (val == NULL || type_val == NULL) {
        hccn_err("input val or type_val is null");
        return -EINVAL;
    }

    if (inet_pton(AF_INET, val, &in.addr) == 1) {
        type_val->ip.ip_addr = in.addr.s_addr;
        goto out;
    }

    if (inet_pton(AF_INET6, val, &in.addr6) == 1) {
        ret = memcpy_s(type_val->ip.ip6_addr, MAX_IPV6_NUM, in.addr6.s6_addr, sizeof(in.addr6.s6_addr));
        if (ret != 0) {
            hccn_err("ipv6 addr memcpy failed. ret:%d, ip6_addr_len:%zu", ret, sizeof(in.addr6.s6_addr));
            return ret;
        }
        goto out;
    }

    return -EINVAL;
out:
    ret = strncpy_s(type_val->ip.str_ip, MAX_IP_VAL_LEN, val, strlen(val));
    if (ret != 0) {
        hccn_err("ip addr strncpy failed! ret:%d, str_ip_len:%zu", ret, strlen(val));
        return ret;
    }

    type_val->is_set = true;
    return 0;
}

int parse_str(struct sub_option_entry *self, const char *val, value_union *type_val)
{
    int ret = 0;

    if (val == NULL || type_val == NULL) {
        hccn_err("input val or type_val is null");
        return -EINVAL;
    }

    ret = strncpy_s(type_val->str_val, MAX_STR_VAL_LEN, val, strlen(val) + 1);
    CHK_PRT_RETURN(ret != 0, hccn_err("strncpy_s %s failed, ret:%d", val, ret), ret);

    type_val->is_set = true;
    return 0;
}

STATIC int mac_hex_to_dec(char curr_chr)
{
    if (curr_chr >= '0' && curr_chr <= '9') {
        return (curr_chr - '0');
    }
    if (curr_chr >= 'a' && curr_chr <= 'f') {
        return (curr_chr - 'a' + MAC_TEN_OFFSET_10);
    }
    if (curr_chr >= 'A' && curr_chr <= 'F') {
        return (curr_chr - 'A' + MAC_TEN_OFFSET_10);
    }
    return -EINVAL;
}

int parse_mac(struct sub_option_entry *self, const char *val, value_union *type_val)
{
    int index, mac_pieces_idx, c2i;
    unsigned char piece_value = 0;
    char curr_chr;

    CHK_PRT_RETURN(val == NULL || type_val == NULL, hccn_err("input val or type_val is null"), -EINVAL);
    CHK_PRT_RETURN(strlen(val) != MAC_STR_LEN, hccn_err("input mac addr len:%zu != %d, ", strlen(val), MAC_STR_LEN),
                   -EINVAL);

    for (index = 0; index < MAC_STR_LEN; index++) {
        curr_chr = val[index];
        if ((index % MAC_MOD_3) == MAC_REMAINDER_2) {
            CHK_PRT_RETURN(curr_chr != ':',
                           hccn_err("mac addr string contains invalid char, index[%d], mac_char[%c]", index, curr_chr),
                           -EINVAL);
            piece_value = 0;
            continue;
        }

        c2i = mac_hex_to_dec(curr_chr);
        CHK_PRT_RETURN(c2i == -EINVAL,
                       hccn_err("mac addr string contains invalid char, index[%d], mac_char[%c]", index, curr_chr),
                       -EINVAL);

        if (index % MAC_MOD_3 == MAC_REMAINDER_0) {
            piece_value += c2i * MAC_HEX_BASE_16;
        }

        if (index % MAC_MOD_3 == MAC_REMAINDER_1) {
            piece_value += c2i;
            mac_pieces_idx = (int)(index / MAC_MOD_3);
            type_val->mac.mac_pieces[mac_pieces_idx] = piece_value;
        }
    }

    type_val->is_set = true;
    return 0;
}

int parse_none(struct sub_option_entry *self, const char *val, value_union *type_val)
{
    type_val->is_set = false;
    if (val != NULL) {
        TOOL_PRINT_ERR("Option %s does not require a value.", self->name);
        return TOOL_PRIVATE_ARGS_VALUE_ERR;
    }

    return 0;
}

int parse_optstr(struct sub_option_entry *self, const char *val, value_union *type_val)
{
    type_val->is_set = false;
    return val == NULL ? 0 : parse_str(self, val, type_val);
}

bool is_valid_integer(const char *str)
{
    char *endptr = NULL;

    (void)strtol(str, &endptr, 0);

    return (endptr != NULL && *endptr == '\0');
}

bool is_valid_ip(const char *str_ip, int expected_family)
{
    union addr_t in = {0};
    void *dst = NULL;

    dst = (expected_family == AF_INET6) ? (void *)&in.addr6 : (void *)&in.addr;

    return inet_pton(expected_family, str_ip, dst) == 1;
}

bool is_value_in_range(struct sub_option_entry *opt, value_union *val)
{
    int int_val, int_min, int_max;
    int str_len, max_len;
    bool flag;
    int i = 0;

    switch (opt->type) {
        case ARG_TYPE_INT:
            int_min = opt->range.int_range.min;
            int_max = opt->range.int_range.max;
            int_val = val->int_val;
            flag = (int_min <= int_val) && (int_val <= int_max);
            CHK_PRT_RETURN(
                !flag, TOOL_PRINT_ERR("Option [%s %d] is out of range [%d, %d].", opt->name, int_val, int_min, int_max),
                false);
            return true;
        case ARG_TYPE_OPTSTR:
            if (!val->is_set) {
                return true;
            }
            // Continue check string length
        case ARG_TYPE_STR:
            max_len = opt->range.str_range.max_len;
            str_len = (int)strlen(val->str_val);
            flag = str_len <= max_len;
            CHK_PRT_RETURN(!flag,
                           TOOL_PRINT_ERR("%s length %d is invalid, exceeds the length threshold %d", val->str_val,
                                          str_len, max_len),
                           false);
            return true;
        case ARG_TYPE_IP:
            return true;
        case ARG_TYPE_FLAG:
            for (; i < ARRAY_SIZE_COMPUTE(opt->range.flag_range.valid_flags); i++) {
                if (val->flag_val == opt->range.flag_range.valid_flags[i]) {
                    return true;
                }
            }
            TOOL_PRINT_ERR("flag %d is invalid", val->flag_val);
            return false;
        case ARG_TYPE_MAC:
            return true;
        default:
            return true;
    }
    return true;
}

int tool_ipv4_int_to_str(const unsigned int ip, char *str, int len)
{
    int ret;

    TOOL_CHECK_PTR_VALID_RETURN_VAL(str, -EINVAL);

    unsigned int gw_fst = SPLIT_UIP(ip, IP_SPLIT_BIT_0);
    unsigned int gw_sed = SPLIT_UIP(ip, IP_SPLIT_BIT_8);
    unsigned int gw_thd = SPLIT_UIP(ip, IP_SPLIT_BIT_16);
    unsigned int gw_fth = SPLIT_UIP(ip, IP_SPLIT_BIT_24);

    ret = sprintf_s(str, (size_t)len, "%u.%u.%u.%u", gw_fst, gw_sed, gw_thd, gw_fth);
    CHK_PRT_RETURN(ret <= 0, hccn_err("sprintf failed %u.%u.%u.%u!, ret:%d", gw_fst, gw_sed, gw_thd, gw_fth, ret),
                   -EIO);

    return 0;
}
