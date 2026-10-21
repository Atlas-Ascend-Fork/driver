/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <limits.h>
#include "encrypt.h"
#include "securec.h"
#include "ds_net_interface.h"
#include "ascend_hal.h"
#include "weak_passwd_dict.h"
#include "hccn_err.h"
#include "hccn_log.h"
#include "hccn_comm.h"
#include "tls_data.h"
#include "ds_data.h"
#include "orion_rdfx_tls.h"
#include "file_opt.h"
#include "tool_log.h"
#include "tool_comm.h"
#include "tool_cmd_err.h"
#include "tool_param.h"
#include "tool_cfg_ops.h"
#include "tool_tls.h"

#define ALARM_RANGE                                             \
    {                                                           \
        .int_range = { TLS_ALARM_MIN_DAYS, TLS_ALARM_MAX_DAYS } \
    }
#define BOOL_RANGE            \
    {                         \
        .int_range = { 0, 1 } \
    }
#define NAME_RANGE                \
    {                             \
        .str_range = { NAME_MAX } \
    }

STATIC struct tls_cmd_args tls_args_buf = {
    .enable = {"-enable", ARG_TYPE_INT, parse_int, MODE_SET, 0, true, .range = BOOL_RANGE, .ext_data = TLS_SET_ENABLE},
    .clear = {"-clear", ARG_TYPE_NONE, parse_none, MODE_SET, 0, .ext_data = TLS_SET_CLEAR},
    .host = {"-host", ARG_TYPE_NONE, parse_none, MODE_BOTH, 0, .ext_data = TLS_SET_ALL},
    .path = {"-path", ARG_TYPE_STR, parse_str, MODE_SET, 0, .ext_data = TLS_SET_CFG},
    .pri = {"-pri", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
    .pub = {"-pub", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
    .ca = {{"-ca1", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca2", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca3", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca4", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca5", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca6", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca7", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca8", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca9", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca10", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca11", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca12", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca13", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG},
           {"-ca14", ARG_TYPE_STR, parse_str, MODE_SET, 0, true, .range = NAME_RANGE, .ext_data = TLS_SET_CFG}},
    .crl = {"-crl", ARG_TYPE_OPTSTR, parse_optstr, MODE_SET, 0, true, .range = NAME_RANGE,
            .ext_data = TLS_SET_CFG | TLS_SET_CLEAR},
    .alarm = {"-alarm", ARG_TYPE_INT, parse_int, MODE_SET, 0, true, .range = ALARM_RANGE, .ext_data = TLS_SET_ALARM},
};

void get_tls_sub_opts(struct sub_option_entry **sub_opts, unsigned int *count)
{
    *sub_opts = tls_args_buf.options;
    *count = tls_args_buf.options_end - tls_args_buf.options;
}

void *tls_alloc_args(void)
{
    struct tls_cmd_args *tls_args = &tls_args_buf;
    struct sub_option_entry *opt;
    unsigned int i;

    tls_args->set_mode_mask = TLS_SET_ALL;
    tls_args->option_count = tls_args->options_end - tls_args->options;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-length-bounds"
    for (i = 0; i < tls_args->option_count; ++i) {
        opt = &tls_args->options[i];
        opt->is_set = false;
        (void)memset_s(&opt->val, sizeof(opt->val), 0, sizeof(opt->val));
    }
#pragma GCC diagnostic pop

    return tls_args;
}

void tls_free_args(struct cmd_entry *self, void *args)
{
    struct tls_cmd_args *tls_args = (struct tls_cmd_args *)args;
    struct sub_option_entry *opt;
    unsigned int i;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-length-bounds"
    for (i = 0; i < tls_args->option_count; ++i) {
        opt = &tls_args->options[i];
        opt->val.as_str = NULL;
        FREE_BUF(opt->val.mem);
    }
#pragma GCC diagnostic pop
    (void)self;
}

int tls_assign(void *args, struct sub_option_entry *opt, value_union *arg_val)
{
    struct tls_cmd_args *tls_args = (struct tls_cmd_args *)args;

    // 选择了一个模式后，其余模式位将被置零，从而判断选项是否冲突
    tls_args->set_mode_mask &= (unsigned int)opt->ext_data;
    if (tls_args->set_mode_mask == 0) {
        TOOL_PRINT_ERR("Option %s conflicts with previous options.", opt->name);
        return TOOL_PRIVATE_ARGS_CONFLICT_ERR;
    }

    opt->is_set = true;
    if (!arg_val->is_set) {
        return 0;
    }
    opt->val.is_set = true;
    switch (opt->type) {
        case ARG_TYPE_INT:
            opt->val.as_int = arg_val->int_val;
            break;
        case ARG_TYPE_OPTSTR:
        case ARG_TYPE_STR:
            opt->val.mem = strdup(arg_val->str_val);
            if (opt->val.mem == NULL) {
                hccn_err("strdup %lu bytes failed", strlen(arg_val->str_val));
                return -ENOMEM;
            }
            opt->val.as_str = opt->val.mem;
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

STATIC unsigned int tool_tls_cert_is_preconfig(struct tls_cert_show_info show_info[], unsigned int num)
{
    unsigned int preconfig = 0;
    unsigned int i;
    size_t size;

    for (i = 0; i < num; i++) {
        size = strlen(show_info[i].issuer) + 1;
        if (strncmp(show_info[i].issuer, "/C=CN/O=Huawei/CN=Huawei IT Product CA", size) == 0 ||
            strncmp(show_info[i].issuer, "/C=CN/O=Huawei/CN=Huawei Equipment CA", size) == 0 ||
            strncmp(show_info[i].issuer, "/C=CN/O=Huawei/CN=Huawei Computing RSA-PSS Equipment CA 1", size) == 0 ||
            strncmp(show_info[i].issuer, "/C=CN/O=Huawei/CN=Huawei Computing RSA-PSS Equipment CA 2", size) == 0 ||
            strncmp(show_info[i].issuer, "/C=CN/O=Huawei/CN=Huawei RSA-PSS Equipment Root CA 1", size) == 0 ||
            strncmp(show_info[i].issuer, "/C=CN/O=Huawei/CN=Huawei RSA Equipment Root CA 2", size) == 0) {
            preconfig = 1;
            break;
        }
    }
    return preconfig;
}

STATIC int tool_save_tls_enable_cfg(const struct command_context *ctx, unsigned int tls_enable, bool save_first_time)
{
    char enable_cfg[MAX_TLS_LEN] = {'\0'};
    int ret;

    ret = tool_read_conf(ctx->slot_id, "tls_enable", enable_cfg, sizeof(enable_cfg));
    // only save tls_enable for the first time
    if (save_first_time && ret != FILE_OPT_SYS_RD_FILE_NOT_FOUND) {
        return 0;
    }

    ret = sprintf_s(enable_cfg, MAX_TLS_LEN, "%u", tls_enable);
    if (ret <= 0) {
        hccn_warn("sprintf_s tls_enable info unsuccessful. ret:%d slot_id:%d logic_id:%d", ret, ctx->slot_id,
                  ctx->npu_info.logic_id);
        return -EIO;
    }

    ret = tool_write_conf(ctx->slot_id, "tls_enable", enable_cfg);
    CHK_PRT_RETURN(ret != 0,
                   hccn_warn("tool_write_conf tls_enable unsuccessful, ret:%d slot_id:%d logic_id:%d", ret,
                             ctx->slot_id, ctx->npu_info.logic_id),
                   ret);

    return 0;
}

STATIC int tool_get_cert_type(unsigned int index, char *type, unsigned int len)
{
    int ret;
    if (index == TLS_PUB_CERT) {
        ret = snprintf_s(type, len, len - 1, "pub cert");
        if (ret <= 0) {
            hccn_err("snprintf_s for pub cert failed, ret:%d, index:%u", ret, index);
            return -EIO;
        }
    } else if ((index >= TLS_CA1_CERT) && (index <= TLS_CA14_CERT)) {
        ret = snprintf_s(type, len, len - 1, "ca%u cert", index);
        if (ret <= 0) {
            hccn_err("snprintf_s for ca cert failed, ret:%d, index:%u", ret, index);
            return -EIO;
        }
    } else {
        ret = snprintf_s(type, len, len - 1, "crl");
        if (ret <= 0) {
            hccn_err("snprintf_s for crl failed, ret:%d, index:%u", ret, index);
            return -EIO;
        }
    }
    return 0;
}

STATIC int tool_check_cert_expired(int logic_id, struct tls_cert_show_info *show_info, unsigned int index)
{
    char time_now[TOOL_TIME_LEN] = {0};
    char type[TLS_TYPE_LEN] = {0};
    struct tm tm_now = {0};
    struct tm *tmp = NULL;
    time_t cert_end_time;
    struct tm tm = {0};
    char *p = NULL;
    time_t now = 0;
    int ret = 0;

    ret = tool_get_cert_type(index, type, TLS_TYPE_LEN);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_get_cert_type failed, ret:%d, index:%d", ret, index), ret);
    TOOL_PRINT_INFO("device %d, [%s] info:\n\t subject[%s]\n\t issuer[%s]\n\t start_time[%s GMT]\n\t end_time[%s GMT]",
                    logic_id, type, show_info[index].subject, show_info[index].issuer, show_info[index].start_time,
                    show_info[index].end_time);

    p = strptime(show_info[index].end_time, "%a %b%d%H:%M:%S%Y", &tm);
    if (p == NULL) {
        hccn_err("string to time failed, index:%u, end_time:%s", index, show_info[index].end_time);
        return -EINVAL;
    }
    cert_end_time = mktime(&tm);
    time(&now);
    tmp = gmtime_r(&now, &tm_now);
    if (tmp == NULL) {
        hccn_err("gmtime_r failed, now:%ld", now);
        return -EINVAL;
    }
    now = mktime(&tm_now);
    p = ctime_r(&now, time_now);
    if (p == NULL) {
        hccn_err("ctime_r failed, now:%ld", now);
        return -EINVAL;
    }
    if (cert_end_time - now <= 0) {
        TOOL_PRINT_ERR("\t tls expiration status[2](0:normal, 1:near expiration, 2:has expired)");
        return TLS_CERT_EXPIRED_ERR;
    }
    hccn_info("logic_id[%d] show_info[%d].tls_alarm:%u", logic_id, index, show_info[index].tls_alarm);
    if ((unsigned int)(cert_end_time - now) <= (show_info[index].tls_alarm * TLS_DAY_TO_S)) {
        TOOL_PRINT_INFO("\t tls expiration status[1](0:normal, 1:near expiration, 2:has expired)");
    } else {
        TOOL_PRINT_INFO("\t tls expiration status[0](0:normal, 1:near expiration, 2:has expired)");
    }
    return 0;
}

STATIC int tool_show_tls_cert_info(int logic_id, struct tls_cert_show_info show_info[], unsigned int num)
{
    unsigned int preconfig;
    unsigned int i;
    int ret = 0;

    CHK_PRT_RETURN(show_info == NULL, hccn_err("show_info is NULL"), -EINVAL);

    preconfig = tool_tls_cert_is_preconfig(show_info, num);

    TOOL_PRINT_INFO("device %d, tls switch[%u](0:disable, 1:enable), tls preconfigured[%u](0:non-preset, 1:preset),"
                    " tls alarm time threshold[%u]days",
                    logic_id, show_info[0].tls_enable, preconfig, show_info[0].tls_alarm);

    for (i = 0; i < num; i++) {
        if (strlen(show_info[i].issuer) > 0) {
            ret = tool_check_cert_expired(logic_id, show_info, i);
            CHK_PRT_RETURN(ret != 0 && ret != TLS_CERT_EXPIRED_ERR,
                           hccn_err("tool_check_cert_expired failed, ret:%d, i:%d, logic_id:%d", ret, i, logic_id),
                           ret);
        }
    }
    return 0;
}

STATIC void tool_tls_cfg_print(struct command_context *ctx, struct tls_cmd_args *tls_args, int logic_id)
{
    char ca_bit_map[TLS_CA14_CERT + 1] = {0};
    int i;

    for (i = TLS_CA1_CERT; i <= TLS_CA14_CERT; i++) {
        ca_bit_map[i - TLS_CA1_CERT] = tls_args->ca[i - TLS_CA1_CERT].is_set ? '1' : '0';
    }
    ca_bit_map[i - 1] = '\0';

    LOG_PROMPT(ctx, "set tls cfg[pub:%u, ca1-ca14:%s, pri:%u, crl:%u, host:%u] for device[%d]", tls_args->pub.is_set,
               ca_bit_map, tls_args->pri.is_set, tls_args->crl.is_set, tls_args->host.is_set, logic_id);
    return;
}

STATIC int tool_check_tls_cfg_intergrity(struct tls_cmd_args *tls_args)
{
    const struct sub_option_entry *opt_group[] = {
        &tls_args->pri,
        &tls_args->pub,
        &tls_args->ca[0],
    };
    bool set_any = false, set_all = true;
    int i;

    for (i = 0; i < ARRAY_SIZE_COMPUTE(opt_group); ++i) {
        set_any = set_any || opt_group[i]->is_set;
        set_all = set_all && opt_group[i]->is_set;
    }

    if (tls_args->crl.is_set && tls_args->host.is_set && !set_any) {
        hccn_err("CRL can only be configured for device-side");
        return TOOL_USER_INPUT_PARAM_ERR;
    }

    if (set_any && !set_all) {
        hccn_err("PUB_CERT, PRI_KEY, CA1_CERT must be set together or not set at all!");
        return TLS_INPUT_CERT_COUNT_ERR;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-length-bounds"
    if (!tls_args->pub.is_set) {
        for (i = TLS_CA1_CERT; i <= TLS_CA14_CERT; i++) {
            CHK_PRT_RETURN(tls_args->certs[i].is_set,
                           hccn_err("CA certificate must be updated after updating pub certificate!"),
                           TLS_CA_WITHOUT_PRI_ERR);
        }
        CHK_PRT_RETURN(!tls_args->crl.is_set, hccn_err("No certificate given!"), TLS_INPUT_CERT_COUNT_ERR);
        hccn_info("Update crl only.");
        return 0;
    }

    // CAs must start from -ca1 and continuous
    for (i = TLS_CA1_CERT; i <= TLS_CA14_CERT && tls_args->certs[i].is_set; i++) {
    }
    for (; i <= TLS_CA14_CERT; i++) {
        CHK_PRT_RETURN(tls_args->certs[i].is_set, hccn_err("certificate chain is not continuous."),
                       TLS_CERT_DISCONTINOUS_ERR);
    }
#pragma GCC diagnostic pop

    return 0;
}

STATIC int tool_read_file_check_size(int type, const int len, bool is_host)
{
    int cert_len = is_host ? OLD_CERT_MAX_SIZE : CERT_MAX_SIZE;

    if ((type == TLS_CRL) && (len >= CRL_MAX_SIZE)) {
        hccn_err("crl size[%d] is bigger than [%d], invalid", len, CRL_MAX_SIZE);
        return -EFBIG;
    }

    if ((type == TLS_PRI_KY) && (len >= KY_MAX_SIZE)) {
        hccn_err("key size[%d] is bigger than [%d], invalid", len, KY_MAX_SIZE);
        return -EFBIG;
    }

    if ((type >= TLS_PUB_CERT) && (type <= TLS_CA14_CERT) && (len > cert_len)) {
        hccn_err("cert type[%d] size[%d] is bigger than [%d], invalid", type, len, cert_len);
        return -EFBIG;
    }

    return 0;
}

STATIC int tool_check_path_valid(const char *name)
{
    char real_path[PATH_MAX + 1] = {0};

    if (strlen(name) == 0 || (realpath(name, real_path) == NULL)) {
        hccn_err("input path or filename %s is invalid, err:%d", name, errno);
        return -ENOENT;
    }
    return 0;
}

STATIC int tool_read_file(const char *path, const char *name, char *content, int *len, int type, bool is_host)
{
    char real_conf_path[PATH_MAX + 1] = {0};
    char full_path[PATH_MAX + 1] = {0};
    FILE *file = NULL;
    int ret, ret_val;
    long temp_len;

    ret = snprintf_s(full_path, PATH_MAX + 1, PATH_MAX, "%s/%s", path, name);
    CHK_PRT_RETURN(ret <= 0, hccn_err("snprintf_s failed, ret:%d", ret), -EIO);

    CHK_PRT_RETURN(realpath(full_path, real_conf_path) == NULL,
                   hccn_err("conf_path %s is invalid, err[%d]", full_path, errno), -ENOENT);

    file = fopen(real_conf_path, "r");
    CHK_PRT_RETURN(file == NULL, hccn_err("file is NULL, invalid"), -ENOENT);

    ret = fseek(file, 0, SEEK_END);
    if (ret < 0) {
        hccn_err("fseek failed with error:%d", errno);
        ret = errno;
        goto out;
    }

    temp_len = ftell(file);
    if (temp_len <= 0 || temp_len > INT_MAX) {
        hccn_err("ftell failed with error:%d", errno);
        ret = errno;
        goto out;
    }

    *len = (int)temp_len;

    ret = tool_read_file_check_size(type, *len, is_host);
    if (ret != 0) {
        hccn_err("check size err, ret:%d", ret);
        goto out;
    }

    rewind(file);
    ret = fread((void *)content, *len, 1, file); /* read a buf which size is *len */
    if (ret != 1) {
        hccn_err("fread failed ret:%d, error:%d, ferror(fp):%d", ret, errno, ferror(file));
        ret = errno;
        goto out;
    }
    ret = 0;
    content[*len] = '\0';
out:
    ret_val = fclose(file);
    if (ret_val != 0) {
        hccn_err("fclose failed, ret_val:%d, errno:%d", ret_val, errno);
    }
    file = NULL;
    return ret;
}

STATIC int tool_get_passwd(char *pwd, unsigned int pwd_len)
{
    struct termios old_ts = {0};
    struct termios ts = {0};
    char *str = NULL;
    int ret;

    ret = tcgetattr(STDIN_FILENO, &ts);
    if (ret != 0) {
        hccn_err("tcgetattr failed, ret:%d, errno:%d", ret, errno);
        return ret;
    }

    ret = memcpy_s(&old_ts, sizeof(struct termios), &ts, sizeof(struct termios));
    if (ret != 0) {
        hccn_err("memcpy_s for old_ts failed, ret:%d", ret);
        return -ENOMEM;
    }

    ts.c_lflag &= ~ECHO;
    ts.c_lflag |= ECHONL;
    ret = tcsetattr(STDIN_FILENO, TCSAFLUSH, &ts);
    CHK_PRT_RETURN(ret != 0, hccn_err("tcsetattr failed, ret:%d, errno:%d", ret, errno), ret);

    (void)fprintf(stdout, "Please enter pass phrase for pri key:");
    str = fgets(pwd, pwd_len, stdin);
    CHK_PRT_RETURN(str == NULL, hccn_err("fgets for pwd failed, pwd_len:%u", pwd_len), -ENOMEM);

    ret = tcsetattr(STDIN_FILENO, TCSANOW, &old_ts);
    if (ret != 0) {
        hccn_err("tcsetattr failed, ret:%d, err_no:%d", ret, errno);
        (void)memset_s(pwd, pwd_len, 0, pwd_len);
        return ret;
    }

    return 0;
}

STATIC void tool_filter_str(char *str, unsigned int *len, char filter)
{
    int idx;

    for (idx = *len - 1; idx >= 0; idx--) {
        if (str[idx] == filter) {
            str[idx] = '\0';
            break;
        }
    }

    *len = strlen(str);
}

STATIC bool is_special_char(char c)
{
    char special_chars[] = "`~!@#$%^&*()-_=+\\|[{}];:'"
                           ",<.>/? ";
    unsigned int chars_len = strlen(special_chars);
    unsigned int i;

    for (i = 0; i < chars_len; i++) {
        if (c == special_chars[i]) {
            return true;
        }
    }

    return false;
}

STATIC int tool_check_pwd_complexity(const char *pwd, unsigned int len)
{
    unsigned int type[PWD_TPYE_CNT] = {0};
    unsigned int type_cnt = 0;
    unsigned int i;

    for (i = 0; i < len; i++) {
        if (pwd[i] >= '0' && pwd[i] <= '9') {
            type[PWD_NUM_INDEX] = 1;
        } else if (pwd[i] >= 'a' && pwd[i] <= 'z') {
            type[PWD_LOW_LET_INDEX] = 1;
        } else if (pwd[i] >= 'A' && pwd[i] <= 'Z') {
            type[PWD_UP_LET_INDEX] = 1;
        } else if (is_special_char(pwd[i])) {
            type[PWD_SYMBOL_INDEX] = 1;
        } else {
            hccn_err("pwd contains invalid char at position %u", i + 1);
            return -EINVAL;
        }
    }

    for (i = 0; i < PWD_TPYE_CNT; i++) {
        type_cnt += type[i];
    }

    if (type_cnt < PWD_COMPLEXITY_THR) {
        hccn_err("type_cnt[%u] < [%u], the complexity of pwd is too low", type_cnt, PWD_COMPLEXITY_THR);
        return TLS_PWD_COMPLEXITY_ERR;
    }

    return 0;
}

STATIC void weak_pwd_check_ret_print(int ret)
{
    switch (ret) {
        case TLS_NO_WEAK_PWD_DICT:
            TOOL_PRINT_INFO("You have not configured a weak password dictionary, please config to ensure safer");
            break;
        case TLS_WEAK_PWD_DICT_TOO_MANY:
            TOOL_PRINT_ERR("You config the weak passwords exceed 10000 entries, invalid!");
            break;
        case TLS_WEAK_PWD_DICT_NULL:
            TOOL_PRINT_INFO("There is nothing in weak password dictionary, please config to ensure safer");
            break;
        case TLS_PWD_TOO_WEAK_ERR:
            TOOL_PRINT_ERR("The password you input is too weak, please choose another one!");
            break;
        default:
            break;
    }
}

STATIC int tool_check_pwd(char *pwd, unsigned int *len)
{
    int ret;

    tool_filter_str(pwd, len, '\n');
    if (*len < PWD_MIN_LEN || *len > PWD_MAX_LEN) {
        hccn_err("len[%u] is invalid, must be between [%d] and [%d]", *len, PWD_MIN_LEN, PWD_MAX_LEN);
        return TLS_PWD_LEN_INVALID_ERR;
    }

    ret = tool_check_pwd_complexity(pwd, *len);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_check_pwd_complexity failed, ret:0x%x", ret), ret);

    ret = crypto_check_weak_pwd(pwd, *len);
    weak_pwd_check_ret_print(ret);
    if (ret != 0 && ret != TLS_NO_WEAK_PWD_DICT && ret != TLS_WEAK_PWD_DICT_NULL) {
        hccn_err("crypto_check_weak_pwd failed, ret:0x%x", ret);
        return TLS_WEAK_PWD_DICT_CHECK_FAIL;
    }

    return 0;
}

STATIC int tool_digital_envelope_enc_data_with_symm_ky(unsigned char *in_buf, unsigned int in_len,
                                                       const unsigned char *symm_ky,
                                                       struct symmetric_enc_info *enc_info)
{
    crypt_info info = {0};
    int ret;

    ret = RAND_priv_bytes(enc_info->iv, IV_LEN);
    CHK_PRT_RETURN(ret != 1, hccn_err("gen iv err, ret:%d", ret), -EINVAL);

    info.key = symm_ky;
    info.iv = enc_info->iv;
    info.in = in_buf;
    info.in_len = in_len;
    info.tag = enc_info->tag;
    info.tag_len = TAG_LEN;
    info.en_dec = TLS_ENC_MODE;

    ret = crypto_encrypt_with_aes_gcm(&info, enc_info->out_buf, enc_info->out_len);
    CHK_PRT_RETURN(ret != 0, hccn_err("crypto_encrypt_with_aes_gcm failed:%d", ret), ret);

    return 0;
}

STATIC int tool_digital_envelope_enc_symm_ky(struct envelope_pub_info *pub_info, unsigned char *symm_ky,
                                             unsigned int symm_ky_len, unsigned char *symm_enc_ky,
                                             unsigned int *symm_enc_ky_len)
{
    EVP_PKEY *pub_ky = NULL;
    RSA *pub_rsa = NULL;
    int enc_len;
    int ret;

    pub_ky = tls_load_ky(pub_info->pub_ky_info, pub_info->pub_ky_len, TLS_PUB_PLAINTEXT, NULL);
    CHK_PRT_RETURN((pub_ky == NULL), hccn_err("tls_load_ky for pub_ky failed"), -EINVAL);

    pub_rsa = EVP_PKEY_get1_RSA(pub_ky);
    EVP_PKEY_free(pub_ky);
    pub_ky = NULL;
    CHK_PRT_RETURN((pub_rsa == NULL), hccn_err("pub_rsa is NULL, EVP_PKEY_get1_RSA failed"), -EINVAL);

    enc_len = RSA_public_encrypt(symm_ky_len, symm_ky, symm_enc_ky, pub_rsa, RSA_PKCS1_OAEP_PADDING);
    if (enc_len <= 0 || enc_len > (int)*symm_enc_ky_len) {
        hccn_err("RSA_public_encrypt failed, enc_len:%d, symm_enc_ky_len:%u", enc_len, *symm_enc_ky_len);
        ret = -EINVAL;
        goto free_rsa;
    }

    *symm_enc_ky_len = enc_len;
    ret = 0;
free_rsa:
    RSA_free(pub_rsa);
    pub_rsa = NULL;
    return ret;
}

STATIC int tool_enc_tls_pwd(unsigned char *pwd, unsigned int pwd_len, struct envelope_pub_info *pub_info,
                            struct tls_cert_ky_crl_info *tls_cfg)
{
    unsigned char symm_ky[ENVELOPE_SYMM_KY_LEN] = {0};
    unsigned char enc_pwd[PWD_MAX_ENC_LEN + 1] = {0};
    unsigned int enc_pwd_len = sizeof(enc_pwd);
    struct symmetric_enc_info enc_info = {0};
    int ret;

    ret = RAND_priv_bytes(symm_ky, ENVELOPE_SYMM_KY_LEN);
    CHK_PRT_RETURN(ret != 1, hccn_err("gen symm_ky err, ret:%d", ret), -EINVAL);

    enc_info.iv = tls_cfg->mng.envelope_iv;
    enc_info.tag = tls_cfg->mng.envelope_tag;
    enc_info.out_buf = enc_pwd;
    enc_info.out_len = &enc_pwd_len;
    ret = tool_digital_envelope_enc_data_with_symm_ky(pwd, pwd_len, symm_ky, &enc_info);
    if (ret != 0) {
        hccn_err("tool_envelope_enc_data_with_symm_ky failed, ret[%d]", ret);
        goto clear_symm_ky;
    }

    ret = memcpy_s(tls_cfg->pwd.pwd_info, sizeof(tls_cfg->pwd.pwd_info), enc_pwd, enc_pwd_len);
    if (ret != 0) {
        hccn_err("memcpy_s for pwd failed, ret:%d", ret);
        (void)memset_s(enc_pwd, sizeof(enc_pwd), 0, sizeof(enc_pwd));
        goto clear_symm_ky;
    }
    tls_cfg->mng.pwd_enc_len = enc_pwd_len;

    (void)memset_s(enc_pwd, sizeof(enc_pwd), 0, sizeof(enc_pwd));

    tls_cfg->symm_enc_ky_info.symm_enc_ky_len = sizeof(tls_cfg->symm_enc_ky_info.symm_enc_ky);
    ret = tool_digital_envelope_enc_symm_ky(pub_info, symm_ky, ENVELOPE_SYMM_KY_LEN,
                                            tls_cfg->symm_enc_ky_info.symm_enc_ky,
                                            &(tls_cfg->symm_enc_ky_info.symm_enc_ky_len));
    if (ret != 0) {
        hccn_err("tool_digital_envelope_enc_symm_ky failed, ret[%d]", ret);
    }

clear_symm_ky:
    (void)memset_s(symm_ky, ENVELOPE_SYMM_KY_LEN, 0, ENVELOPE_SYMM_KY_LEN);
    return ret;
}

STATIC int tool_handle_tls_pwd(int logic_id, unsigned char *pwd, unsigned int pwd_len,
                               struct tls_cert_ky_crl_info *tls_cfg)
{
    unsigned int size_out = (unsigned int)sizeof(struct envelope_pub_info);
    struct envelope_pub_info *pub_info = NULL;
    struct dsmi_info info = {0};
    int ret;

    pub_info = calloc(1, sizeof(struct envelope_pub_info));
    CHK_PRT_RETURN((pub_info == NULL), hccn_err("no memory for pub_info"), -ENOMEM);
    pub_info->pub_ky_len = (unsigned int)sizeof(pub_info->pub_ky_info);

    DSMI_FILL_INFO(info, NULL, 0, NULL, 0, (char *)pub_info, &size_out);
    ret = dsmi_exe_get_info(logic_id, DS_GET_ENVELOPE_PUB, &info);
    if (ret != 0) {
        hccn_err("get tls digital envelope pub_ky failed, ret:%d", ret);
        goto out;
    }

    tls_cfg->mng.pwd_len = pwd_len;
    ret = tool_enc_tls_pwd(pwd, pwd_len, pub_info, tls_cfg);
    if (ret != 0) {
        hccn_err("tool_enc_tls_pwd failed, ret:%d, logic_id:%d", ret, logic_id);
    }

out:
    free(pub_info);
    return ret;
}

STATIC int tool_get_and_check_tls_pwd(int logic_id, bool host_flag, struct tls_cert_ky_crl_info *tls_cfg)
{
    char pwd[FGETS_MAX_LEN] = {0};
    int ret, ret_val;
    unsigned int len;

    ret = tool_get_passwd(pwd, sizeof(pwd));
    CHK_PRT_RETURN(ret != 0, hccn_err("memcpy_s for pwd failed, ret:%d", ret), ret);

    len = strlen(pwd);
    ret = tool_check_pwd(pwd, &len);
    if (ret != 0 || len > PWD_MAX_LEN) {
        hccn_err("tool_check_pwd failed ret:%d, or len[%u] > PWD_MAX_LEN[%d]", ret, len, PWD_MAX_LEN);
        goto clear_pwd;
    }

    if (host_flag == false) {
        ret = tool_handle_tls_pwd(logic_id, (unsigned char *)pwd, len, tls_cfg);
        if (ret != 0) {
            hccn_err("tool_handle_tls_pwd failed, ret:%d", ret);
            goto clear_pwd;
        }
    } else {
        ret = memcpy_s(tls_cfg->pwd.pwd_info, sizeof(tls_cfg->pwd.pwd_info), pwd, len);
        if (ret != 0) {
            hccn_err("memcpy_s for pwd failed, ret:%d", ret);
            ret = -ENOMEM;
            goto clear_pwd;
        }
        tls_cfg->mng.pwd_len = len;
    }

clear_pwd:
    ret_val = memset_s(pwd, sizeof(pwd), 0, sizeof(pwd));
    if (ret_val != 0) {
        hccn_err("memset_s for pwd failed, ret:%d", ret_val);
        ret = -ENOMEM;
    }
    return ret;
}

STATIC void tool_print_read_error(const char *path, const char *filename, int ret)
{
    TOOL_PRINT_ERR("File %s/%s does not exist or read failed!", path, filename);
    hccn_err("tool_read_file for %s/%s failed, ret:%d", path, filename, ret);
}

STATIC int tool_get_tls_cfg_from_file(int logic_id, struct tls_cmd_args *tls_args, struct tls_cert_ky_crl_info *tls_cfg)
{
    const bool is_host = tls_args->host.is_set;
    const char *path = tls_args->path.val.as_str;
    const char *cert_name, *crl_name;
    int ret, len = 0;
    char *temp_cert;
    unsigned int i;

    if (tls_args->pri.is_set) {
        ret = tool_read_file(path, tls_args->pri.val.as_str, (char *)&(tls_cfg->ky.ky_info), &len, TLS_PRI_KY, is_host);
        CHK_PRT_RETURN(ret != 0, tool_print_read_error(path, tls_args->pri.val.as_str, ret), ret);
        tls_cfg->mng.ky_len = len;
        ret = tool_get_and_check_tls_pwd(logic_id, is_host, tls_cfg);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_get_and_check_tls_pwd failed, ret:%d", ret), ret);
    }

    // host侧证书不支持新证书格式
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-length-bounds"
    for (i = TLS_PUB_CERT; i <= TLS_CA14_CERT; i++) {
        if (tls_args->certs[i].is_set) {
            cert_name = tls_args->certs[i].val.as_str;
            temp_cert = is_host ? tls_cfg->certs[i].certInfo :
                                  tls_cfg->ncerts[i / TLS_CA8_CERT].certs[i % TLS_CA8_CERT].ncert_info;

            ret = tool_read_file(path, cert_name, temp_cert, &len, i, is_host);
            if (ret != 0) {
                TOOL_PRINT_ERR("File %s/%s does not exist or read failed!", path, cert_name);
                hccn_err("tool_read_file for %d:%s/%s failed, ret:%d, host:%d.", i, path, cert_name, ret, is_host);
                return ret;
            }
            tls_cfg->mng.cert_len[i] = len;
            tls_cfg->mng.total_cert_len += len;
            if (!is_host) {
                tls_cfg->ncerts[i / TLS_CA8_CERT].ncert_count++;
            }
        }
    }
#pragma GCC diagnostic pop

    if (tls_args->crl.is_set) {
        crl_name = tls_args->crl.val.as_str;
        ret = tool_read_file(path, crl_name, (char *)tls_cfg->crl.crl_info, &len, TLS_CRL, is_host);
        CHK_PRT_RETURN(ret != 0, tool_print_read_error(path, crl_name, ret), ret);
        tls_cfg->mng.crl_len = len;
    }

    return 0;
}

STATIC int tool_tls_handle_cfg(struct tls_cmd_args *tls_args, struct tls_cert_ky_crl_info *tls_cfg, int logic_id)
{
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret;

    CHK_PRT_RETURN(!tls_args->path.is_set,
                   hccn_err("Please specify the path of the certificate on the system by -path <%%s>"),
                   TLS_INPUT_CERT_COUNT_ERR);

    ret = tool_check_tls_cfg_intergrity(tls_args);
    CHK_PRT_RETURN(ret != 0, hccn_err("tls_cfg is incomplete, ret:%d", ret), ret);

    ret = tool_check_path_valid(tls_args->path.val.as_str);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_check_path_valid failed, ret:%d", ret), ret);

    ret = tool_get_tls_cfg_from_file(logic_id, tls_args, tls_cfg);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_get_tls_cfg_from_file failed, ret:%d", ret), ret);

    if (tls_args->host.is_set) {
        tls_cfg->mng.save_mode = TLS_SAVE_TO_FILE;
        ret = rdfx_set_tls_cfg(logic_id, (char *)tls_cfg, sizeof(struct tls_cert_ky_crl_info), NULL, 0);
        if (ret == -EBUSY) {
            return DEV_SYS_BUSY_ERR;
        } else if (ret != 0) {
            hccn_err("rdfx_set_tls_cfg failed, ret:%d", ret);
        }
    } else {
        tls_cfg->mng.save_mode = TLS_SAVE_TO_FlASH;
        DSMI_FILL_INFO(info, (char *)tls_cfg, sizeof(struct tls_cert_ky_crl_info), NULL, 0, NULL, &size);
        ret = dsmi_exe_get_info(logic_id, DS_SET_TLS_CFG, &info);
        if (ret != 0) {
            hccn_err("set tls cfg by dsmi failed, ret:%d", ret);
        }
    }

    if (ret == TLS_NO_CERT_ERR || ret == TLS_CERT_EXPIRED_ERR || ret == TLS_CERT_ILLEGAL_ERR) {
        return ret;
    }
    if (ret != 0) {
        return TLS_CERT_VERIFY_FAIL;
    }

    return 0;
}

STATIC void get_tls_cert_count(struct tls_cert_ky_crl_info *tls_cfg, struct tls_cmd_args *tls_args)
{
    int i;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-length-bounds"
    for (i = TLS_PUB_CERT; i <= TLS_CA14_CERT; i++) {
        tls_cfg->mng.cert_count += (unsigned int)tls_args->certs[i].is_set;
    }
#pragma GCC diagnostic pop
}

STATIC int tool_set_tls_cfg(struct command_context *ctx, struct tls_cmd_args *tls_args)
{
    struct tls_cert_ky_crl_info *tls_cfg = NULL;
    int ret;

    if (tls_args->crl.is_set && !tls_args->crl.val.is_set) {
        TOOL_PRINT_ERR("param option %s missing value", tls_args->crl.name);
        hccn_err("Setting CRL requires a value.");
        return TOOL_USER_INPUT_PARAM_ERR;
    }

    LOG_PROMPT(ctx, "start to set tls cfg of device[%d]", ctx->npu_info.logic_id);

    tls_cfg = calloc(1, sizeof(struct tls_cert_ky_crl_info));
    CHK_PRT_RETURN(tls_cfg == NULL, hccn_err("tls_cfg is NULL"), -ENOMEM);

    get_tls_cert_count(tls_cfg, tls_args);

    tool_tls_cfg_print(ctx, tls_args, ctx->npu_info.logic_id);
    ret = tool_tls_handle_cfg(tls_args, tls_cfg, ctx->npu_info.logic_id);
    if (ret != 0) {
        if (ret != DEV_SYS_BUSY_ERR) {
            hccn_err("handle tls cfg err, ret:%d", ret);
        }
    }

    (void)memset_s(tls_cfg, sizeof(struct tls_cert_ky_crl_info), 0, sizeof(struct tls_cert_ky_crl_info));
    free(tls_cfg);
    tls_cfg = NULL;
    return ret;
}

STATIC int tool_get_tls_cfg(struct command_context *ctx, struct tls_cmd_args *tls_args)
{
    unsigned int save_mode = (tls_args == NULL) ? TLS_SAVE_TO_FlASH : TLS_SAVE_TO_FILE;
    unsigned int size_out = sizeof(struct tls_cert_show_info) * MAX_SHOW_INFO_COUNT;
    struct tls_cert_show_info show_info[MAX_SHOW_INFO_COUNT];
    struct dsmi_info info = {0};
    int ret = 0;

    if (save_mode == TLS_SAVE_TO_FILE) {
        ret = rdfx_get_tls_cfg(ctx->npu_info.logic_id, (const char *)&save_mode, sizeof(save_mode), (char *)show_info,
                               &size_out);
    } else {
        DSMI_FILL_INFO(info, NULL, 0, NULL, 0, (char *)show_info, &size_out);
        ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_TLS_CFG, &info);
    }

    if (ret == -ENOENT) {
        hccn_warn("logic_id[%u] has no certs, please set certs firstly", ctx->npu_info.logic_id);
        return TLS_NO_CERT_ERR;
    }
    CHK_PRT_RETURN(ret != 0, hccn_err("get tls config failed, ret:%d", ret), ret);

    // save tls_enable to conf for the first time, ignore failure
    if (save_mode == TLS_SAVE_TO_FlASH) {
        ret = tool_save_tls_enable_cfg(ctx, show_info[0].tls_enable, true);
        if (ret != 0) {
            hccn_warn("logic_id:%d save tls_enable cfg unsuccessful, ret:%d", ctx->npu_info.logic_id, ret);
        }
    }

    ret = tool_show_tls_cert_info(ctx->npu_info.logic_id, show_info, MAX_SHOW_INFO_COUNT);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("tool_show_tls_cert_info failed! ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    return 0;
}

int set_tls_flash_enable(struct command_context *ctx, struct tls_enable_info *enable_info)
{
    struct dsmi_info info = {0};
    unsigned int size_out = 0;
    long int val;
    int ret;

    ret = halGetDeviceInfo(ctx->npu_info.logic_id, MODULE_TYPE_SYSTEM, INFO_TYPE_RUN_MACH, &val);
    CHK_PRT_RETURN(ret != 0, hccn_err("halGetDeviceInfo failed, ret:%d, logic_id:%d", ret, ctx->npu_info.logic_id),
                   ret);

    enable_info->machine_type = (int)val;
    enable_info->save_mode = TLS_SAVE_TO_FlASH;

    DSMI_FILL_INFO(info, (char *)enable_info, sizeof(struct tls_enable_info), NULL, 0, NULL, &size_out);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_TLS_ENABLE, &info);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("dsmi_set_tls_enable failed logic_id:%d ret:%d enable:%u", ctx->npu_info.logic_id, ret,
                            enable_info->enable),
                   ret);

    return 0;
}

STATIC int tool_set_tls_enable(struct command_context *ctx, struct tls_cmd_args *tls_args)
{
    struct tls_enable_info enable_info = {0};
    char enable_cfg[MAX_TLS_LEN] = {'\0'};
    unsigned int size_out = 0;
    int ret = 0;

    CHK_PRT_RETURN(!tls_args->enable.is_set, TOOL_PRINT_ERR("please input [-enable 0/1] to set tls enable"), -EINVAL);

    LOG_PROMPT(ctx, "start to set tls enable[%d] of device[%d]", tls_args->enable.val.as_int, ctx->npu_info.logic_id);

    enable_info.enable = (unsigned int)tls_args->enable.val.as_int;
    if (tls_args->host.is_set) {
        enable_info.save_mode = TLS_SAVE_TO_FILE;
        ret = rdfx_set_tls_enable(ctx->npu_info.logic_id, (const char *)&enable_info, sizeof(struct tls_enable_info),
                                  NULL, &size_out);
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("rdfx_set_tls_enable failed for logic_id:%d ret:%d enable:%d", ctx->npu_info.logic_id,
                                ret, tls_args->enable.val.as_int),
                       ret);
        return 0;
    }

    ret = set_tls_flash_enable(ctx, &enable_info);
    CHK_PRT_RETURN(ret != 0, hccn_err("Set tls flash enable failed, logic_id=%d", ctx->npu_info.logic_id), ret);

    ret = sprintf_s(enable_cfg, MAX_TLS_LEN, "%u", enable_info.enable);
    CHK_PRT_RETURN(ret <= 0, hccn_err("sprintf tls enable info failed!"), -EIO);

    ret = tool_write_conf(ctx->slot_id, "tls_enable", enable_cfg);
    CHK_PRT_RETURN(
        ret != 0, hccn_err("write cfg error. ret:%d slot_id:%d logic_id:%d", ret, ctx->slot_id, ctx->npu_info.logic_id),
        TOOL_WRITE_CONF_ERR);

    return 0;
}

STATIC int tool_set_tls_alarm(struct command_context *ctx, struct tls_cmd_args *tls_args)
{
    struct tls_alarm_info alarm_info = {0};
    struct dsmi_info info = {0};
    unsigned int size_out = 0;
    int ret;

    CHK_PRT_RETURN(!tls_args->alarm.is_set, TOOL_PRINT_ERR("please input [-alarm <day>] to set tls alarm time"),
                   -EINVAL);

    LOG_PROMPT(ctx, "start to set tls alarm[%d] of device[%d]", tls_args->alarm.val.as_int, ctx->npu_info.logic_id);

    alarm_info.alarm = (unsigned int)tls_args->alarm.val.as_int;
    if (tls_args->host.is_set) {
        alarm_info.save_mode = TLS_SAVE_TO_FILE;
        ret = rdfx_set_tls_alarm(ctx->npu_info.logic_id, (const char *)&alarm_info, sizeof(struct tls_alarm_info), NULL,
                                 &size_out);
        CHK_PRT_RETURN(ret != 0,
                       hccn_err("rdfx_set_tls_alarm failed for logic_id:%d ret:%d alarm:%d", ctx->npu_info.logic_id,
                                ret, tls_args->alarm.val.as_int),
                       ret);
        return 0;
    }

    alarm_info.save_mode = TLS_SAVE_TO_FlASH;
    DSMI_FILL_INFO(info, (char *)&alarm_info, sizeof(struct tls_alarm_info), NULL, 0, NULL, &size_out);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_TLS_ALARM, &info);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("dsmi_set_tls_alarm failed logic_id:%d ret:%d alarm:%u", ctx->npu_info.logic_id, ret,
                            alarm_info.alarm),
                   ret);

    return 0;
}

STATIC int tool_clear_tls_cfg_on_device(struct command_context *ctx, const struct tls_clear_info *clear_info)
{
    int logic_id = ctx->npu_info.logic_id;
    struct dsmi_info info = {0};
    unsigned int size_out = 0;
    int ret;

    DSMI_FILL_INFO(info, (char *)clear_info, sizeof(*clear_info), NULL, 0, NULL, &size_out);
    ret = dsmi_exe_get_info(logic_id, DS_CLEAR_TLS_CFG, &info);
    if (ret == -ENOENT) {
        hccn_err("No TLS certs found in flash, nothing to clear. ret:%d logic_id:%d", ret, logic_id);
        ret = TLS_NO_CERT_ERR;
    } else if (ret != 0) {
        hccn_err("clear tls cfg failed ret[%d] logic_id[%d]", ret, logic_id);
        return ret;
    }

    // cleared successfully or no cert found
    if (clear_info->clear_mode == TLS_CLEAR_ALL) {
        ret = tool_del_conf(ctx->slot_id, "tls_enable");
        CHK_PRT_RETURN(
            ret != 0,
            hccn_err("clear tls certs success, clear tls_enable in %s failed, ret:%d, logic_id:%d, slot_id:%d",
                     CFGFILE_NAME, ret, logic_id, ctx->slot_id),
            ret);
    }

    return ret;
}

STATIC int tool_clear_tls_cfg(struct command_context *ctx, struct tls_cmd_args *tls_args)
{
    struct tls_clear_info clear_info = {.clear_mode = TLS_CLEAR_ALL, .save_mode = TLS_SAVE_TO_FlASH};
    unsigned int size_out = 0;
    int ret;

    if (tls_args->crl.is_set && tls_args->crl.val.is_set) {
        TOOL_PRINT_ERR("flag option %s got needless value", tls_args->crl.name);
        return TOOL_USER_INPUT_PARAM_ERR;
    }
    if (tls_args->crl.is_set && tls_args->host.is_set) {
        TOOL_PRINT_ERR("CRL clear does not support host side.");
        return TOOL_USER_INPUT_PARAM_ERR;
    }

    LOG_PROMPT(ctx, "start to clear tls cfg[crl:%u, host:%u] of device[%d]", tls_args->crl.is_set,
               tls_args->host.is_set, ctx->npu_info.logic_id);

    if (tls_args->host.is_set) {
        clear_info.save_mode = TLS_SAVE_TO_FILE;
        ret = rdfx_clear_tls_cfg(ctx->npu_info.logic_id, (const char *)&clear_info, sizeof(clear_info), NULL,
                                 &size_out);
        if (ret == -EBUSY) {
            return DEV_SYS_BUSY_ERR;
        }
        CHK_PRT_RETURN(ret != 0, hccn_err("clear tls cfg failed ret[%d]", ret), ret);
    } else {
        if (tls_args->crl.is_set) {
            clear_info.clear_mode = TLS_CLEAR_CRL;
        }
        ret = tool_clear_tls_cfg_on_device(ctx, &clear_info);
    }

    if (ret == 0) {
        hccn_info("TLS config cleared successfully. logic_id:%d", ctx->npu_info.logic_id);
    }

    return ret;
}

STATIC int analysis_tls_ret(int ret)
{
    if (ret == TLS_CERT_VERIFY_ERR || ret == TLS_CERT_KYMATCH_ERR || ret == TLS_CERT_LOAD_ERR) {
        return TLS_CERT_VERIFY_FAIL;
    }
    return ret;
}

STATIC int tool_tls_exec(struct command_context *ctx)
{
    struct tls_cmd_args *tls_args = (struct tls_cmd_args *)ctx->cmd_args;
    int ret = 0;

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    if (ctx->mode == MODE_GET) {
        ret = tool_get_tls_cfg(ctx, tls_args);
        CHK_PRT_RETURN(ret != 0, hccn_err("tool_get_tls_cfg failed"), ret);
        goto out;
    }

    CHK_PRT_RETURN(tls_args == NULL, hccn_err("tls_args is NULL"), TOOL_PRIVATE_ARGS_NUM_ERR);

    switch (tls_args->set_mode_mask) {
        case TLS_SET_CFG:
            ret = tool_set_tls_cfg(ctx, tls_args);
            ret = analysis_tls_ret(ret);
            TOOL_PRINT_WARN("Please delete the private key file after updating for security considerations.");
            CHK_PRT_RETURN(ret != 0, hccn_err("tool_set_tls_cfg failed"), ret);
            break;
        case TLS_SET_ENABLE:
            ret = tool_set_tls_enable(ctx, tls_args);
            CHK_PRT_RETURN(ret != 0, hccn_err("tool_set_tls_enable failed"), ret);
            break;
        case TLS_SET_ALARM:
            ret = tool_set_tls_alarm(ctx, tls_args);
            CHK_PRT_RETURN(ret != 0, hccn_err("tool_set_tls_alarm failed"), ret);
            break;
        case TLS_SET_CLEAR:
            ret = tool_clear_tls_cfg(ctx, tls_args);
            CHK_PRT_RETURN(ret != 0, hccn_err("tool_clear_tls_cfg failed"), ret);
            break;
        default:
            hccn_err("input tls mode invalid");
            ret = -EINVAL;
            break;
    }

out:
    return ret;
}

STATIC void tool_tls_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_tls_print_help(ctx, MODE_GET);
            tool_tls_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-tls", "get tls cfg for NPU or Host");
            tool_print_npu_info(DEV_NPU_ATTR);
            TOOL_PRINT_OPT("[-host]", "get tls cfg message for Host");
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-tls", "set tls cfg for NPU or Host");
            tool_print_npu_info(DEV_NPU_ATTR);
            TOOL_PRINT_OPT("-path <file path>", "set the path where the tls certificate file is located");
            TOOL_PRINT_OPT("-pri <filename>", "set pri key filename, only support pem file format");
            TOOL_PRINT_OPT("-pub <filename>", "set pub certificate filename, only support pem file format");
            TOOL_PRINT_OPT("-caN <filename>",
                           "set ca certificate filename, range of N [1, 14], only support pem file format");
            TOOL_PRINT_OPT("[-crl <filename>]", "set the filename for revocation list, only support crl file format");
            TOOL_PRINT_OPT("[-host]", "set tls certificate for Host");

            TOOL_PRINT_CMD("-s", "-tls", "set tls cfg for NPU or Host");
            tool_print_npu_info(DEV_NPU_ATTR);
            TOOL_PRINT_OPT("-clear", "clear tls cfg");
            TOOL_PRINT_OPT("[-crl]", "clear crl only, conflicts with -host");
            TOOL_PRINT_OPT("[-host]", "clear tls cfg for Host");

            TOOL_PRINT_CMD("-s", "-tls", "set tls cfg for NPU or Host");
            tool_print_npu_info(DEV_NPU_ATTR);
            TOOL_PRINT_OPT("-enable <0/1>", "set tls enable, 0: disable, 1: enable");
            TOOL_PRINT_OPT("[-host]", "set tls enable for Host");

            TOOL_PRINT_CMD("-s", "-tls", "set tls cfg for NPU or Host");
            tool_print_npu_info(DEV_NPU_ATTR);
            TOOL_PRINT_OPT("-alarm <alarm day>", "set tls expired alarm(days), range [7, 180]");
            TOOL_PRINT_OPT("[-host]", "set tls expired alarm for Host");
            break;
        default:
            break;
    }
}

STATIC struct cmd_entry g_tls_cmds[] = {
    {"-tls",
     get_tls_sub_opts,
     MODE_BOTH,
     tls_alloc_args,
     tls_assign,
     tool_tls_exec,
     {DEV_NPU_ATTR},
     tool_tls_print_help,
     NULL,
     0,
     .free_args = tls_free_args},
};

void tool_get_tls_cmds(struct cmd_entry **cmds, unsigned int *count)
{
    *cmds = g_tls_cmds;
    *count = ARRAY_SIZE_COMPUTE(g_tls_cmds);
}
