/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/file.h>
#include <unistd.h>
#include "securec.h"
#include "dsmi_common_interface.h"
#include "file_opt.h"
#include "hccn_comm.h"
#include "tcpip_cmd_data.h"
#include "ubase_cmd_data.h"
#include "shell_cmd_data.h"
#include "dev_info_data.h"
#include "hccn_log.h"
#include "hccn_err.h"
#include "tool_param.h"
#include "ds_net_interface.h"
#include "tool_comm.h"
#include "tls.h"
#include "tool_product.h"
#include "tool_tls.h"
#include "tool_cmd_err.h"
#include "cfg_cmd_data.h"
#include "tool_log.h"
#include "ds_data.h"
#include "tool_cmd_parse.h"
#include "tool_dev_info.h"
#include "tool_shell_ops.h"
#include "tool_ubase_ops.h"
#include "tool_tcpip_ops.h"
#include "tool_cfg_ops.h"

int tool_read_conf(int slot_id, const char *conf_name, char *conf_value, unsigned int len)
{
    return FileReadCfg(CFGFILE_NAME, slot_id, conf_name, conf_value, len);
}

#ifndef ENABLE_BUILD_PRODUCT
STATIC int tool_cfg_cmdline_recovery(struct command_context *ctx, const char *cmd_prefix,
                                     int (*cmdline_recovery)(struct command_context *, const char *))
#else
EXPORT_FUNCTION int tool_cfg_cmdline_recovery(struct command_context *ctx, const char *cmd_prefix,
                                              int (*cmdline_recovery)(struct command_context *, const char *))
#endif
{
    char line_buf[CONFLINE_LENGTH] = {0};
    FILE *fp = NULL;
    int t_ret = 0;
    int ret = 0;

#ifndef CONFIG_LLT
    fp = fopen(CFGFILE_NAME, "r");
    CHK_PRT_RETURN(fp == NULL, hccn_err("open configure file failed, errno:%d real_conf_path:%s", errno, CFGFILE_NAME),
                   errno);

    ret = flock(fileno(fp), LOCK_EX);
    if (ret != 0) {
        hccn_err("hccn.conf lock fd:%d failed! ret:%d errno:%d", fileno(fp), ret, errno);
        ret = errno;
        goto close_fd;
    }

    ret = fseek(fp, 0, SEEK_SET);
    if (ret != 0) {
        hccn_err("hccn.conf fseek fd:%d failed! ret:%d errno:%d", fileno(fp), ret, errno);
        ret = errno;
        goto out;
    }

    while (feof(fp) == 0 && fgets(line_buf, CONFLINE_LENGTH, fp) != NULL) {
        if (strstr(line_buf, cmd_prefix) != NULL) {
            ret = cmdline_recovery(ctx, line_buf);
            if (ret != 0) {
                hccn_err("recovery configure failed, ret:%d cmdline:%s", ret, line_buf);
                break;
            }
        }
    }

out:
    t_ret = flock(fileno(fp), LOCK_UN);
    if (t_ret != 0) {
        hccn_err("unlock hccn.conf failed, t_ret:%d errno:%d", t_ret, errno);
    }

close_fd:
    (void)fclose(fp);

#endif
    return ret != 0 ? ret : t_ret;
}

STATIC int tool_inner_write_conf_byfd(FILE *fp, const char *conf_name, const char *conf_value, char *sum_buf,
                                      int sum_buf_len)
{
    char line_buf[CONFLINE_LENGTH] = {0};
    char new_conf[CONFLINE_LENGTH] = {0};
    int ret, ret_val;

    if (sum_buf == NULL) {
        return -ENOENT;
    }
    while (feof(fp) == 0 && fgets(line_buf, CONFLINE_LENGTH, fp) != NULL) {
        if ((strncmp(line_buf, conf_name, strlen(conf_name)) == 0) && (line_buf[strlen(conf_name)] == '=')) {
            continue;
        } else {
            ret = strcat_s(sum_buf, sum_buf_len, line_buf);
            CHK_PRT_RETURN(ret != 0, hccn_err("strcat failed, ret:%d", ret), ret);
        }
    }

    ret = sprintf_s(new_conf, CONFLINE_LENGTH, "%s=%s\n", conf_name, conf_value);
    CHK_PRT_RETURN(ret <= 0,
                   hccn_err("sprintf str op failed, ret:%d conf_name:%s conf_value:%s", ret, conf_name, conf_value),
                   -EIO);

    ret = strcat_s(sum_buf, sum_buf_len, new_conf);
    CHK_PRT_RETURN(ret < 0, hccn_err("sum_buf str op failed, ret:%d new_conf:%s", ret, new_conf), -ENOENT);

    hccn_info("---configure---->\n%s<----------", sum_buf);
    ret = ftruncate(fileno(fp), 0);
    if (ret < 0) {
        hccn_err("hccn.conf ftruncate failed! ret[%d] errno[%d]", ret, errno);
        return ret;
    }

    ret = fseek(fp, 0, SEEK_SET);
    if (ret) {
        hccn_err("hccn.conf fseek fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
        return ret;
    }

    ret_val = fputs(sum_buf, fp);
    if (ret_val < 0) {
        hccn_warn("fputs failed, ret_val:%d, fd:%d, errno:%d", ret_val, fileno(fp), errno);
    }
    return 0;
}

STATIC int tool_inner_write_conf(const char *conf_path, const char *conf_name, const char *conf_value)
{
    char real_conf_path[PATH_MAX + 1] = {0}; // lint !e813
    char *sum_buf = NULL;
    int ret, ret_val;
    FILE *fp = NULL;

    if ((strlen(conf_path) > PATH_MAX) || (realpath(conf_path, real_conf_path) == NULL)) {
        hccn_err("write path_len[%zu] > PATH_MAX or conf_path is invalid, err[%d]", strlen(conf_path), errno);
        return -EINVAL;
    }

#ifndef CONFIG_LLT
    fp = fopen(real_conf_path, "a+");
    if (fp == NULL) {
        hccn_err("Open read configure failed! errno[%d]", errno);
        return errno;
    }

    sum_buf = (char *)calloc(CONFILE_LENGTH, sizeof(char));
    if (sum_buf == NULL) {
        hccn_err("calloc failed.");
        ret = -ENOENT;
        goto close_fd;
    }

    ret = flock(fileno(fp), LOCK_EX);
    if (ret) {
        hccn_err("hccn.conf lock failed! ret:%d, fd:%d, errno:%d", ret, fileno(fp), errno);
        ret = errno;
        goto out;
    }

    ret = fseek(fp, 0, SEEK_SET);
    if (ret) {
        hccn_err("hccn.conf fseek fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
        ret = errno;
        goto out;
    }

    ret = tool_inner_write_conf_byfd(fp, conf_name, conf_value, sum_buf, CONFILE_LENGTH);
out:
    free(sum_buf);
    sum_buf = NULL;
close_fd:
    ret_val = fclose(fp);
    fp = NULL;
    CHK_PRT_RETURN(ret_val != 0, hccn_err("fclose failed, ret_val:%d, errno:%d", ret_val, errno), ret_val);
    return ret;
#else
    return 0;
#endif
}

int tool_write_conf(int slot_id, const char *conf_name, const char *conf_value)
{
    char conf[CONFLINE_LENGTH] = {0};
    int ret;

    if (conf_name == NULL || conf_value == NULL) {
        hccn_err("Input param is NULL.");
        return -EINVAL;
    }

    ret = sprintf_s(conf, CONFLINE_LENGTH, "%s_%d", conf_name, slot_id);
    if (ret <= 0) {
        hccn_err("conf str op failed! ret[%d] conf_name[%s] slot_id[%d]", ret, conf_name, slot_id);
        return -ENOENT;
    }

    ret = tool_inner_write_conf(CFGFILE_NAME, conf, conf_value);
    if (ret) {
        hccn_err("tool_inner_write_conf failed! ret[%d] conf[%s] conf_value[%s]", ret, conf, conf_value);
        return ret;
    }
    hccn_info("write conf[%s] realconf[%s] conf_value[%s]", conf, conf_name, conf_value);

    return 0;
}

bool tool_check_conf_udhcpc_enable(int slot_id)
{
    char udhcpc_flag[UDHCPC_QUIT_INBUFF_LEN] = {0};
    int ret;

    ret = tool_read_conf(slot_id, "udhcpc_enable", udhcpc_flag, sizeof(udhcpc_flag));
    CHK_PRT_RETURN(ret == 0, hccn_info("[Ipv4]hccn.conf has udhcpc_enable flag"), true);

    return false;
}

#ifndef CONFIG_LLT
STATIC int rule_list_match(char list[][CONFLINE_LENGTH], int list_cnt, const char *dst)
{
    int i;

    if (list == NULL || list_cnt <= 0) {
        return -EINVAL;
    }

    for (i = 0; i < list_cnt; i++) {
        if (strstr(dst, list[i]) == NULL) {
            return -EINVAL;
        }
    }

    return 0;
}

STATIC int handle_conf_to_file_proc_buf(FILE *fp, int op_type, char match_list[][CONFLINE_LENGTH], int list_cnt,
                                        char *file_buf)
{
    char line_buf[CONFLINE_LENGTH] = {0};
    bool found_flag = false;
    int ret;

    while (feof(fp) == 0 && fgets(line_buf, CONFLINE_LENGTH, fp) != NULL) {
        if ((op_type != FILE_OPT_TYPE_APPEND_WRITE) && !found_flag &&
            (rule_list_match(match_list, list_cnt, line_buf) == 0)) {
            found_flag = true;
            continue;
        }
        ret = strcat_s(file_buf, CONFILE_LENGTH, line_buf);
        CHK_PRT_RETURN(ret != 0, hccn_err("strcat hccn.conf failed, ret:%d, errno:%d", ret, errno), -EIO);
    }
    return 0;
}
#endif

int tool_update_conf(const char *cfg, int op_type, char match_list[][CONFLINE_LENGTH], int list_cnt)
{
    char *file_buf = (char *)calloc(1, CONFILE_LENGTH);
    FILE *fp = NULL;
    int ret = 0;

    CHK_PRT_RETURN(file_buf == NULL, hccn_err("no memory to update hccn.conf, malloc failed"), -ENOMEM);

#ifndef CONFIG_LLT
    fp = fopen(CFGFILE_NAME, "r+");
    if (fp == NULL) {
        ret = errno;
        hccn_err("open hccn.conf failed, errno:%d, file path=\"%s\"", ret, CFGFILE_NAME);
        goto free_buf;
    }

    if (flock(fileno(fp), LOCK_EX) != 0) {
        ret = errno;
        hccn_err("lock hccn.conf failed, errno:%d", ret);
        goto close_file;
    }

    ret = handle_conf_to_file_proc_buf(fp, op_type, match_list, list_cnt, file_buf);
    if (ret != 0) {
        goto out;
    }

    if ((op_type == FILE_OPT_TYPE_APPEND_WRITE) || (op_type == FILE_OPT_TYPE_MATCH_REPLACES)) {
        ret = strcat_s(file_buf, CONFILE_LENGTH, cfg);
        if (ret != 0) {
            hccn_err("strcat hccn.conf failed, ret:%d, errno:%d", ret, errno);
            ret = -EIO;
            goto out;
        }
    }

    ret = ftruncate(fileno(fp), 0);
    if (ret != 0) {
        ret = errno;
        hccn_err("truncate hccn.conf failed, ret:%d", ret);
        goto out;
    }
    (void)fseek(fp, 0, SEEK_SET);
    ret = fputs(file_buf, fp);
    if (ret < 0) {
        hccn_err("write hccn.conf failed, ret:%d", ret);
    } else {
        ret = 0;
    }
out:
    if (flock(fileno(fp), LOCK_UN) != 0) {
        hccn_warn("unlock hccn.conf failed, errno:%d", errno);
    }
close_file:
    (void)fclose(fp);
#endif
free_buf:
    FREE_BUF(file_buf);
    return ret;
}

#ifndef CONFIG_LLT
int hccn_inner_del_conf_byfd(FILE *fp, const char *conf_name)
{
    char line_buf[CONFLINE_LENGTH] = {0};
    long offset_write = 0;
    long offset_read = 0;
    long buff_len = 0;
    int ret = 0;
    int ret_val;

    while (feof(fp) == 0 && fgets(line_buf, CONFLINE_LENGTH, fp) != NULL) {
        buff_len = (long)strlen(line_buf);

        if (offset_read != offset_write) {
            ret = fseek(fp, offset_write, SEEK_SET);
            if (ret) {
                hccn_err("hccn.conf fseek fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
                return FILE_OPT_SYS_DELETE_FILE_ERR;
            }
            ret_val = fputs(line_buf, fp);
            if (ret_val < 0) {
                hccn_warn("fputs failed, ret_val:%d, fd:%d, errno:%d", ret_val, fileno(fp), errno);
            }
        }

        if (strstr(line_buf, conf_name)) {
            offset_read += buff_len;
        } else {
            offset_read += buff_len;
            offset_write += buff_len;
        }

        ret = fseek(fp, offset_read, SEEK_SET);
        if (ret) {
            hccn_err("fseek fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
            ret = FILE_OPT_SYS_DELETE_FILE_ERR;
            break;
        }
    }

    ret_val = ftruncate(fileno(fp), offset_write);
    if (ret_val < 0) {
        hccn_warn("ftruncate failed, ret_val:%d, fd:%d, errno:%d", ret_val, fileno(fp), errno);
    }
    return ret;
}

STATIC int hccn_inner_del_conf(const char *conf_path, const char *conf_name)
{
    char real_conf_path[PATH_MAX + 1] = {0};
    int ret, ret_val;
    FILE *fp = NULL;

    if ((strlen(conf_path) > PATH_MAX) || (realpath(conf_path, real_conf_path) == NULL)) {
        hccn_err("delete path_len[%zu] > PATH_MAX or conf_path is invalid, err[%d]", strlen(conf_path), errno);
        return -EINVAL;
    }

    fp = fopen(real_conf_path, "r+");
    if (fp == NULL) {
        hccn_err("Open configure file failed! errno[%d], real_conf_path[%s]", errno, real_conf_path);
        return FILE_OPT_SYS_DELETE_FILE_ERR;
    }

    ret = flock(fileno(fp), LOCK_EX);
    if (ret) {
        hccn_err("hccn.conf lock failed! ret[%d] errno[%d]", ret, errno);
        ret = FILE_OPT_SYS_DELETE_FILE_ERR;
        goto close_file;
    }

    ret = fseek(fp, 0, SEEK_SET);
    if (ret) {
        hccn_err("hccn.conf fseek fd[%d] failed! ret[%d] errno[%d]", fileno(fp), ret, errno);
        ret = FILE_OPT_SYS_DELETE_FILE_ERR;
        goto out;
    }

    ret = hccn_inner_del_conf_byfd(fp, conf_name);

out:
    if (flock(fileno(fp), LOCK_UN) != 0) {
        hccn_warn("unlock hccn.conf failed, errno:%d", errno);
    }
close_file:
    ret_val = fclose(fp);
    if (ret_val != 0) {
        hccn_warn("fclose failed, ret_val:%d, errno:%d", ret_val, errno);
    }
    fp = NULL;
    return ret;
}
#endif

int tool_del_conf(int slot_id, const char *conf_name)
{
    char conf[CONFLINE_LENGTH] = {0};
    int ret;

    CHK_PRT_RETURN(conf_name == NULL, hccn_err("conf_name is NULL"), -EINVAL);

    ret = sprintf_s(conf, CONFLINE_LENGTH, "%s_%d", conf_name, slot_id);
    CHK_PRT_RETURN(ret <= 0, hccn_err("conf str op failed! ret[%d] conf_name[%s] slot_id[%d]", ret, conf_name, slot_id),
                   -EIO);

#ifndef CONFIG_LLT
    ret = hccn_inner_del_conf(CFGFILE_NAME, conf);
    if (ret) {
        hccn_err("cfg_inner_del_conf failed! ret[%d] conf[%s]", ret, conf);
        return ret;
    }

    hccn_info("del conf[%s]", conf);
#endif
    return ret;
}

STATIC int tool_get_cfg_status(struct command_context *ctx, unsigned int *status)
{
    unsigned int cfg_status_size = sizeof(unsigned int);
    struct dsmi_info info = {0};
    int ret = 0;

    ctx->result = (char *)status;
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), NULL, 0, ctx->result, &cfg_status_size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_GET_CFG_STATUS, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("get cfg status failed, logic_id:%d ret:%d", ctx->npu_info.logic_id, ret), ret);

    return 0;
}

STATIC int tool_get_cfg_exec(struct command_context *ctx)
{
    unsigned int status = TOOL_NET_CFG_NONE;
    int ret = 0;

    ret = tool_get_cfg_status(ctx, &status);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_get_cfg_status failed, logic_id:%d, ret:%d", ctx->npu_info.logic_id, ret),
                   ret);

    switch (status) {
        case TOOL_NET_CFG_NONE:
            TOOL_PRINT_INFO("config status:NONE"); /* not config */
            break;
        case TOOL_NET_CFG_START:
            TOOL_PRINT_INFO("config status:START"); /* start to config and not finish */
            break;
        case TOOL_NET_CFG_FAIL:
            TOOL_PRINT_INFO("config status:FAIL"); /* config fail */
            break;
        case TOOL_NET_CFG_SUCCESS:
            TOOL_PRINT_INFO("config status:SUCCESS"); /* config success */
            break;
        default:
            TOOL_PRINT_INFO("config status:FAULT"); /* unexpected fault. Maybe software coding error */
            break;
    }

    return 0;
}

STATIC int tool_set_cfg_status(struct command_context *ctx, unsigned int status)
{
    unsigned int cfg_status_size = sizeof(unsigned int);
    struct dsmi_info info = {0};
    unsigned int size = 0;
    int ret = 0;

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)(&status), cfg_status_size, NULL, &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_CFG_STATUS, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("set cfg status failed, logic_id:%d ret:%d", ctx->npu_info.logic_id, ret), ret);

    return 0;
}

STATIC int tool_cfg_bond_attr_recovery(struct command_context *ctx)
{
    char bond_attr_val[BOND_ATTR_VAL_STRING_LEN + 1] = {0};
    struct bond_attr_cmd_args bond_attr_args = {0};
    char *tmp_ptr = NULL;
    int ret = 0;

    ret = tool_read_conf(ctx->slot_id, "bond_attr", bond_attr_val, sizeof(bond_attr_val));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            hccn_warn("tool_read_conf bond_attr do not need recover cfg, ret:%d logic_id:%d slot_id:%d", ret,
                      ctx->npu_info.logic_id, ctx->slot_id);
            return 0;
        } else {
            hccn_err("tool_read_conf bond_attr set failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                     ctx->slot_id);
            return ret;
        }
    }

    bond_attr_args.lacp_rate = (int)strtoul(bond_attr_val, &tmp_ptr, STR_NUM_BASE_10);
    CHK_PRT_RETURN(tmp_ptr == bond_attr_val || *tmp_ptr != '\0',
                   hccn_err("lacp_rate must be int, lacp_rate:%d ptr_tail:%s)", bond_attr_args.lacp_rate, tmp_ptr),
                   -EINVAL);

    ctx->cmd_args = (void *)(&bond_attr_args);
    ret = tool_exec_with_link_status(ctx, tool_bond_attr_set, false);
    if (ret != 0) {
        hccn_err("tool_exec_with_link_status failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id);
    }

    ctx->cmd_args = NULL;
    return ret;
}

STATIC int tool_cfg_mtu_recovery(struct command_context *ctx)
{
    char mtu_val[MTU_VAL_STRING_LEN] = {0};
    struct mtu_cmd_args mtu_args = {0};
    char *tmp_ptr = NULL;
    int ret = 0;

    ret = tool_read_conf(ctx->slot_id, "mtu", mtu_val, sizeof(mtu_val));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            hccn_warn("tool_read_conf mtu do not need recover cfg, ret:%d logic_id:%d slot_id:%d", ret,
                      ctx->npu_info.logic_id, ctx->slot_id);
            return 0;
        } else {
            hccn_err("tool_read_conf mtu set failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                     ctx->slot_id);
            return ret;
        }
    }

    mtu_args.mtu_size = (int)strtoul(mtu_val, &tmp_ptr, STR_NUM_BASE_10);
    CHK_PRT_RETURN(tmp_ptr == mtu_val || *tmp_ptr != '\0',
                   hccn_err("mtu_size must be int, mtu_size:%d ptr_tail:%s)", mtu_args.mtu_size, tmp_ptr), -EINVAL);

    ctx->cmd_args = (void *)(&mtu_args);
    ret = tool_exec_with_link_status(ctx, tool_set_mtu_exec, false);
    if (ret != 0) {
        hccn_err("tool_exec_with_link_status failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id);
    }

    ctx->cmd_args = NULL;
    return ret;
}

STATIC int tool_cfg_ip_udhcpc_recovery(struct command_context *ctx)
{
    char dhcp_res[OUTBUF_LEN_5120] = {0};
    int slot_id = ctx->slot_id;
    int ret = 0;

    if (!tool_check_conf_udhcpc_enable(slot_id)) {
        return 0;
    }

    ctx->result = dhcp_res;
    ctx->cmd_args = NULL;
    ret = tool_get_udhcpc_exec(ctx);
    if (ret != 0) {
        hccn_err("tool_cfg_ip_udhcpc_recovery failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id);
    }

    FREE_BUF(ctx->cmd_args);
    return ret;
}

STATIC int tool_cfg_mac_addr_recovery(struct command_context *ctx)
{
    char mac_addr_val[MAC_STR_LEN + 1] = {0};
    struct mac_val mac_args = {0};
    struct dsmi_info info = {0};
    value_union type_val = {0};
    unsigned int size = 0;
    int ret = 0;

    ret = tool_read_conf(ctx->slot_id, "mac_addr", mac_addr_val, sizeof(mac_addr_val));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            hccn_warn("tool_read_conf mac_addr do not need recover cfg, ret:%d logic_id:%d slot_id:%d", ret,
                      ctx->npu_info.logic_id, ctx->slot_id);
            return 0;
        } else {
            hccn_err("tool_read_conf mac_addr set failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                     ctx->slot_id);
            return ret;
        }
    }

    ret = parse_mac(NULL, mac_addr_val, &type_val);
    CHK_PRT_RETURN(ret != 0, hccn_err("parse_mac failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    (void)memcpy_s(&mac_args, sizeof(struct mac_val), &type_val.mac, sizeof(type_val.mac));
    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)&mac_args, sizeof(struct mac_val), NULL,
                   &size);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_MAC_ADDR, &info);
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("set mac failed, logic_id:%d dev_name:%s ret:%d", ctx->npu_info.logic_id, ctx->dev_name, ret), ret);

    return 0;
}

STATIC int tool_cfg_pfc_recovery(struct command_context *ctx)
{
    struct netdev_info ndev_info = {0};
    int t_ret = 0;
    int ret = 0;

    ctx->cmd_args = calloc(1, sizeof(struct pfc_cmd_args));
    CHK_PRT_RETURN(ctx->cmd_args == NULL, hccn_err("calloc pfc_cmd_args failed, logic_id:%d", ctx->npu_info.logic_id),
                   -ENOMEM);

    ret = tool_read_conf(ctx->slot_id, "bitmap", (char *)ctx->cmd_args, sizeof(struct pfc_cmd_args));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            hccn_warn("tool_read_conf pfc do not need recover cfg, ret:%d logic_id:%d slot_id:%d", ret,
                      ctx->npu_info.logic_id, ctx->slot_id);
            ret = 0;
        } else {
            hccn_err("tool_read_conf pfc set failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                     ctx->slot_id);
        }
        goto err_read_conf;
    }

    ret = tool_get_bond_slaves(ctx, &ndev_info);
    if (ret != 0) {
        hccn_err("get bond slaves failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id);
        goto err_read_conf;
    }

    ret = tool_set_pfc_exec(ctx, &ndev_info);
    if (ret != 0) {
        hccn_err("tool_set_pfc_exec failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id);
    }

    (void)memset_s(ctx->dev_name, MAX_DEV_NAME_LEN, 0, MAX_DEV_NAME_LEN);
    t_ret = sprintf_s(ctx->dev_name, MAX_DEV_NAME_LEN, "bond%d", ctx->slot_id);
    if (t_ret <= 0) {
        hccn_err("sprintf_s failed, logic_id:%d slot_id:%d ret:%d", ctx->npu_info.logic_id, ctx->slot_id, t_ret);
        t_ret = -EIO;
    } else {
        t_ret = 0;
    }
    ret = (ret != 0) ? ret : t_ret;

err_read_conf:
    free(ctx->cmd_args);
    ctx->cmd_args = NULL;

    return ret;
}

STATIC int tool_tc_cfg_recovery(struct command_context *ctx)
{
    struct tc_cfg_cmd_args tc_cfg_args = {0};
    char tc_cfg_str[CONFLINE_LENGTH] = {0};
    struct tc_cfg_info tc_cfg = {0};
    struct dsmi_info info = {0};
    char *tc_param_str = NULL;
    char *next_token = NULL;
    unsigned int size = 0;
    int tc_param_i = 0;
    int ret = 0;

    ret = tool_read_conf(ctx->slot_id, "tc_cfg", tc_cfg_str, sizeof(tc_cfg_str));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            hccn_warn("tool_read_conf tc_cfg do not need recover cfg, ret:%d logic_id:%d slot_id:%d", ret,
                      ctx->npu_info.logic_id, ctx->slot_id);
            return 0;
        } else {
            hccn_err("tool_read_conf tc_cfg set failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                     ctx->slot_id);
            return ret;
        }
    }

    tc_param_str = strtok_s(tc_cfg_str, " ", &next_token);
    for (; tc_param_i < ARGC_ID_4; tc_param_i++) {
        CHK_PRT_RETURN(tc_param_str == NULL,
                       hccn_err("tc cfg file is error, logic_id:%d tc_param_i:%d", ctx->npu_info.logic_id, tc_param_i),
                       -EINVAL);

        if (tc_param_i == TC_BUFF_INDEX) {
            (void)memcpy_s(&tc_cfg_args.tc_buff, INPUT_TC_CFG_LEN, tc_param_str, INPUT_TC_CFG_LEN);
        } else if (tc_param_i == TC_WATERLINE_INDEX) {
            (void)memcpy_s(&tc_cfg_args.waterline, INPUT_TC_CFG_LEN, tc_param_str, INPUT_TC_CFG_LEN);
        }

        tc_param_str = strtok_s(NULL, " ", &next_token);
    }

    ret = tool_tc_cfg_get_size(tc_cfg_args.tc_buff, &tc_cfg);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_tc_cfg_get_size failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                   ret);

    ret = tool_tc_cfg_get_waterline(tc_cfg_args.waterline, &tc_cfg);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("tool_tc_cfg_get_waterline failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)(&tc_cfg), sizeof(struct tc_cfg_info),
                   NULL, &size);
    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_TC_CFG_INFO, &info);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi set tc_cfg info failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                   ret);

    return 0;
}

STATIC int tool_cfg_prio_tc_recovery(struct command_context *ctx)
{
    struct netdev_info ndev_info = {0};
    int t_ret = 0;
    int ret = 0;

    ctx->cmd_args = calloc(1, sizeof(struct prio_tc_cmd_args));
    CHK_PRT_RETURN(ctx->cmd_args == NULL,
                   hccn_err("calloc prio_tc_cmd_args failed, logic_id:%d", ctx->npu_info.logic_id), -ENOMEM);

    ret = tool_read_conf(ctx->slot_id, "prio_tc", (char *)ctx->cmd_args, sizeof(struct prio_tc_cmd_args));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            hccn_warn("tool_read_conf prio_tc do not need recover cfg, ret:%d logic_id:%d slot_id:%d", ret,
                      ctx->npu_info.logic_id, ctx->slot_id);
            ret = 0;
        } else {
            hccn_err("tool_read_conf prio_tc set failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                     ctx->slot_id);
        }
        goto err_read_conf;
    }

    ret = tool_get_bond_slaves(ctx, &ndev_info);
    if (ret != 0) {
        hccn_err("get bond slaves failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id);
        goto err_read_conf;
    }

    ret = tool_set_prio_tc_exec(ctx, &ndev_info);
    if (ret != 0) {
        hccn_err("tool_set_prio_tc_exec failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id);
    }

    (void)memset_s(ctx->dev_name, MAX_DEV_NAME_LEN, 0, MAX_DEV_NAME_LEN);
    t_ret = sprintf_s(ctx->dev_name, MAX_DEV_NAME_LEN, "bond%d", ctx->slot_id);
    if (t_ret <= 0) {
        hccn_err("sprintf_s failed, logic_id:%d slot_id:%d ret:%d", ctx->npu_info.logic_id, ctx->slot_id, t_ret);
        t_ret = -EIO;
    } else {
        t_ret = 0;
    }
    ret = (ret != 0) ? ret : t_ret;

err_read_conf:
    free(ctx->cmd_args);
    ctx->cmd_args = NULL;

    return ret;
}

STATIC int tool_cfg_tls_enable_recovery(struct command_context *ctx)
{
    struct tls_enable_info enable_info = {0};
    char tls_enable[MAX_TLS_LEN] = {0};
    char *tmp_ptr = NULL;
    int ret;

    ret = tool_read_conf(ctx->slot_id, "tls_enable", tls_enable, sizeof(tls_enable));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            hccn_warn("tool_read_conf tls_enable do not need recover cfg, ret:%d logic_id:%d slot_id:%d", ret,
                      ctx->npu_info.logic_id, ctx->slot_id);
            return 0;
        } else {
            hccn_err("tool_read_conf tls_enable set failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                     ctx->slot_id);
            return ret;
        }
    }

    enable_info.enable = (unsigned int)strtoul(tls_enable, &tmp_ptr, STR_NUM_BASE_10);
    CHK_PRT_RETURN(tmp_ptr == tls_enable || *tmp_ptr != '\0',
                   hccn_err("enable status must be unsigned int, enable:%u; ptr_tail:%s)", enable_info.enable, tmp_ptr),
                   -EINVAL);

    ret = set_tls_flash_enable(ctx, &enable_info);
    CHK_PRT_RETURN(ret != 0, hccn_err("set tls flash enable failed, logic_id:%d", ctx->npu_info.logic_id), ret);
    return 0;
}

STATIC int tool_cfg_ip_recovery(struct command_context *ctx)
{
    struct ip_cmd_args cfg_ip_args = {0};
    char mask_val[MAX_IP_VAL_LEN] = {0};
    char ip_val[MAX_IP_VAL_LEN] = {0};
    unsigned int missing_args_num = 0;
    struct dsmi_info info = {0};
    value_union type_val = {0};
    unsigned int size = 0;
    int ret = 0;

    if (tool_check_conf_udhcpc_enable(ctx->slot_id)) {
        hccn_info("udhcpc enable, do not need recover ip cfg, logic_id:%d", ctx->npu_info.logic_id);
        return 0;
    }

    ret = tool_read_conf(ctx->slot_id, "address", ip_val, sizeof(ip_val));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            missing_args_num++;
        } else {
            hccn_err("tool_read_conf ip address set failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                     ctx->slot_id);
            return ret;
        }
    }

    ret = tool_read_conf(ctx->slot_id, "netmask", mask_val, sizeof(mask_val));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            CHK_PRT_RETURN(
                missing_args_num != 0,
                hccn_warn("Both ip and netmask are missing, do not need recover ip cfg, logic_id:%d slot_id:%d",
                          ctx->npu_info.logic_id, ctx->slot_id),
                0);
            missing_args_num++;
        } else {
            hccn_err("tool_read_conf ip netmask set failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                     ctx->slot_id);
            return ret;
        }
    }

    CHK_PRT_RETURN(missing_args_num != 0,
                   hccn_err("Invalid missing_args_num:%u, need both ip and netmask for recovery logic_id:%d slot_id:%d",
                            missing_args_num, ctx->npu_info.logic_id, ctx->slot_id),
                   -EINVAL);

    ret = parse_ip(NULL, ip_val, &type_val);
    CHK_PRT_RETURN(ret != 0, hccn_err("parse_ip ip_val failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id), ret);
    (void)memcpy_s(&cfg_ip_args.ip, sizeof(struct ip_val), &type_val.ip, sizeof(struct ip_val));

    (void)memset_s(&type_val, sizeof(value_union), 0, sizeof(value_union));
    ret = parse_ip(NULL, mask_val, &type_val);
    CHK_PRT_RETURN(ret != 0, hccn_err("parse_ip mask_val failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                   ret);
    (void)memcpy_s(&cfg_ip_args.netmask, sizeof(struct ip_val), &type_val.ip, sizeof(struct ip_val));

    DSMI_FILL_INFO(info, (char *)ctx, sizeof(struct command_context), (char *)&cfg_ip_args, sizeof(struct ip_cmd_args),
                   NULL, &size);

    ctx->cmd_args = (void *)(&cfg_ip_args);
    CHK_PRT_RETURN(is_ipaddr_conflict(ctx), hccn_err("ipaddr conflict, addr:%s netmask:%s", ip_val, mask_val),
                   DEV_EXE_CONFLICT_IP_ADDR_ERR);

    ret = dsmi_exe_get_info(ctx->npu_info.logic_id, DS_SET_IP, &info);
    if (ret != 0) {
        hccn_err("set ip failed, logic_id:%d dev_name:%s ret:%d", ctx->npu_info.logic_id, ctx->dev_name, ret);
    }

    ctx->cmd_args = NULL;
    return ret;
}

STATIC int tool_cfg_netdetect_recovery(struct command_context *ctx)
{
    struct netdetect_cmd_args netdetect_args = {0};
    char ip_val[MAX_IP_VAL_LEN] = {0};
    value_union type_val = {0};
    int ret = 0;

    ret = tool_read_conf(ctx->slot_id, "netdetect", ip_val, sizeof(ip_val));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            hccn_warn("tool_read_conf netdetect ip address do not need recover cfg, ret:%d logic_id:%d slot_id:%d", ret,
                      ctx->npu_info.logic_id, ctx->slot_id);
            return 0;
        } else {
            hccn_err("tool_read_conf netdetect ip address set failed! ret:%d logic_id:%d slot_id:%d", ret,
                     ctx->npu_info.logic_id, ctx->slot_id);
            return ret;
        }
    }

    ret = parse_ip(NULL, ip_val, &type_val);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("parse_ip netdetect ip address failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                   ret);

    (void)memcpy_s(&netdetect_args.address, sizeof(struct ip_val), &type_val.ip, sizeof(struct ip_val));
    ctx->cmd_args = (void *)(&netdetect_args);

    ret = tool_set_netdetect(ctx);
    if (ret != 0) {
        hccn_err("tool set netdetect failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id);
    }

    ctx->cmd_args = NULL;
    return ret;
}

int tool_cfg_gateway_recovery(struct command_context *ctx)
{
    struct gateway_cmd_args *gw_args = NULL;
    char gateway_val[MAX_IP_VAL_LEN] = {0};
    value_union type_val = {0};
    int ret = 0;

    if (tool_check_conf_udhcpc_enable(ctx->slot_id)) {
        hccn_info("udhcpc enable, do not need recover gateway cfg, logic_id:%d", ctx->npu_info.logic_id);
        return 0;
    }

    ctx->cmd_args = calloc(1, sizeof(struct gateway_cmd_args));
    CHK_PRT_RETURN(ctx->cmd_args == NULL,
                   hccn_err("calloc gateway_cmd_args failed, logic_id:%d", ctx->npu_info.logic_id), -ENOMEM);

    ret = tool_read_conf(ctx->slot_id, "gateway", gateway_val, sizeof(gateway_val));
    if (ret != 0) {
        if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
            hccn_warn("tool_read_conf gateway do not need recover cfg, ret:%d logic_id:%d slot_id:%d", ret,
                      ctx->npu_info.logic_id, ctx->slot_id);
            ret = 0;
        } else {
            hccn_err("tool_read_conf gateway set failed! ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                     ctx->slot_id);
        }
        goto err_read_conf;
    }

    ret = parse_ip(NULL, gateway_val, &type_val);
    if (ret != 0) {
        hccn_err("parse_ip gateway_val failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id);
        goto err_read_conf;
    }

    gw_args = (struct gateway_cmd_args *)ctx->cmd_args;
    (void)memcpy_s(&gw_args->gw_addr, sizeof(struct ip_val), &type_val.ip, sizeof(struct ip_val));

    ret = tool_set_gateway_exec(ctx);
    if (ret != 0) {
        hccn_err("tool_set_gateway_exec failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id);
    }

err_read_conf:
    free(ctx->cmd_args);
    ctx->cmd_args = NULL;

    return ret;
}

STATIC int tool_cfg_arp_cmd(struct command_context *ctx, const char *cmd)
{
    char mac_addr_val[MAC_STR_LEN + 1] = {0};
    struct arp_cmd_args *arp_args = NULL;
    char ip_val[MAX_IP_VAL_LEN] = {0};
    value_union mac_type_val = {0};
    value_union ip_type_val = {0};
    int ret = 0;

    ret = sscanf_s(cmd, "-s %s %s", ip_val, MAX_IP_VAL_LEN, mac_addr_val, MAC_STR_LEN + 1);
    CHK_PRT_RETURN(ret != ARGC_ID_2, hccn_err("tool arp recovery get param failed, ret:%d", ret), ret);

    ret = parse_ip(NULL, ip_val, &ip_type_val);
    CHK_PRT_RETURN(ret != 0, hccn_err("parse_ip failed, ret:%d", ret), ret);
    ret = parse_mac(NULL, mac_addr_val, &mac_type_val);
    CHK_PRT_RETURN(ret != 0, hccn_err("parse_mac failed, ret:%d", ret), ret);

    ctx->cmd_args = calloc(1, sizeof(struct arp_cmd_args));
    CHK_PRT_RETURN(ctx->cmd_args == NULL, hccn_err("calloc arp_cmd_args failed, logic_id:%d", ctx->npu_info.logic_id),
                   -ENOMEM);

    arp_args = (struct arp_cmd_args *)ctx->cmd_args;
    (void)memcpy_s(&arp_args->ip, sizeof(arp_args->ip), &ip_type_val.ip, sizeof(ip_type_val.ip));
    (void)memcpy_s(&arp_args->mac, sizeof(arp_args->mac), &mac_type_val.mac, sizeof(mac_type_val.mac));
    arp_args->mode = ARP_SET_MODE_ADD;
    ctx->sub_opt_mask = SUB_OPT_MAC_MASK;

    ret = tool_arp_set(ctx);
    if (ret != 0) {
        hccn_err("tool_arp_set failed, ret:%d", ret);
    }

    ctx->sub_opt_mask = 0;
    free(ctx->cmd_args);
    ctx->cmd_args = NULL;
    return ret;
}

STATIC int tool_cfg_arp_cmd_recovery(struct command_context *ctx, const char *cfg_cmd)
{
    char line_buf[CONFLINE_LENGTH] = {0};
    char *next_token = NULL;
    char *cmd = NULL;
    int ret = 0;

    ret = strncpy_s(line_buf, CONFLINE_LENGTH, cfg_cmd, strlen(cfg_cmd));
    if (ret != 0) {
        hccn_err("tool arp recovery strncpy_s cfg failed, ret:%d", ret);
        return ret;
    }

    cmd = strtok_s(line_buf, "=", &next_token);
    if (cmd == NULL) {
        hccn_err("strtok_s failed, cfg_cmd:%s", line_buf);
        return -EINVAL;
    }

    cmd = strtok_s(NULL, "=", &next_token);
    if (cmd == NULL) {
        hccn_err("strtok_s failed, cfg_cmd:%s", line_buf);
        return -EINVAL;
    }

    /* del the end \n symbol */
    cmd[strlen(cmd) - 1] = '\0';

    return tool_cfg_arp_cmd(ctx, cmd);
}

int tool_cfg_arp_recovery(struct command_context *ctx)
{
    char arp_prefix[CONFLINE_LENGTH] = {0};
    int ret = 0;

    if (tool_check_conf_udhcpc_enable(ctx->slot_id)) {
        hccn_info("udhcpc enable, do not need recover arp cfg, logic_id:%d", ctx->npu_info.logic_id);
        return 0;
    }

    ret = sprintf_s(arp_prefix, CONFLINE_LENGTH, "%s_%d", "arp", ctx->slot_id);
    CHK_PRT_RETURN(ret <= 0,
                   hccn_err("sprintf_s arp_prefix failed, ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id,
                            ctx->slot_id),
                   -EIO);

    return tool_cfg_cmdline_recovery(ctx, arp_prefix, tool_cfg_arp_cmd_recovery);
}

STATIC int tool_cfg_qos_dscp_recovery(struct command_context *ctx)
{
    struct qos_map_info qos_cfg[HCCL_QOS_LEVEL_CNT] = {0};
    char qos_dscp_cfg_string[QOS_DSCP_CFG_LEN] = {0};
    struct qos_dscp_args *qos_args = NULL;
    int i, ret = 0;

    ctx->cmd_args = calloc(1, sizeof(struct qos_dscp_args));
    CHK_PRT_RETURN(ctx->cmd_args == NULL, hccn_err("calloc qos_dscp_args failed, logic_id:%d", ctx->npu_info.logic_id),
                   -ENOMEM);

    qos_args = (struct qos_dscp_args *)ctx->cmd_args;

    ret = tool_read_conf(ctx->slot_id, "qos_dscp", qos_dscp_cfg_string, QOS_DSCP_CFG_LEN);
    if (ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND || ret == FILE_OPT_INNER_PARAM_ERR) {
        hccn_warn("not save qos_map before, ret:%d logic_id:%d slot_id:%d", ret, ctx->npu_info.logic_id, ctx->slot_id);
        ret = 0;
        goto out;
    }
    CHK_PRT_GOTO(ret != 0,
                 hccn_err("read %s for qos_dscp_cfg failed! ret:%d logic_id:%d slot_id:%d", CFGFILE_NAME, ret,
                          ctx->npu_info.logic_id, ctx->slot_id),
                 out);

    ret = parse_qos_cfg(qos_cfg, HCCL_QOS_LEVEL_CNT, qos_dscp_cfg_string);
    CHK_PRT_GOTO(ret != 0, hccn_err("parse_qos_cfg failed! ret:%d slot_id:%d", ret, ctx->slot_id), out);

    qos_args->op = OP_MAP;
    for (i = 0; i < HCCL_QOS_LEVEL_CNT; i++) {
        if (qos_cfg[i].save_cfg_flag) {
            qos_args->dscp_idx = qos_cfg[i].dscp_idx;
            qos_args->qos_idx = i;
            ctx->sub_opt_mask |= QOS_OPT_BIT;
            ctx->sub_opt_mask |= DSCP_OPT_BIT;
            ret = tool_set_qos_dscp_exec(ctx);
            CHK_PRT_GOTO(ret != 0, hccn_err("tool_set_qos_dscp_exec for qos_%d failed", i), out);
        }
    }
out:
    FREE_BUF(ctx->cmd_args);
    return ret;
}

STATIC recovery_cmd_t g_recovery_independent_cmd_list[] = {
    {"bond_attr", tool_cfg_bond_attr_recovery},
    {"mtu", tool_cfg_mtu_recovery},
    {"ipv4 udhcpc", tool_cfg_ip_udhcpc_recovery},
    {"mac address", tool_cfg_mac_addr_recovery},
    {"pfc", tool_cfg_pfc_recovery},
    {"tc", tool_tc_cfg_recovery},
    {"prio_tc", tool_cfg_prio_tc_recovery},
    {"tls_enable", tool_cfg_tls_enable_recovery},
    {"qos_dscp", tool_cfg_qos_dscp_recovery},
};

STATIC recovery_cmd_t g_recovery_dependent_cmd_list[] = {
    {"ipv4", tool_cfg_ip_recovery},
    {"netdetect", tool_cfg_netdetect_recovery},
    {"ipv4 gateway", tool_cfg_gateway_recovery},
    {"arp", tool_cfg_arp_recovery},
};

STATIC recovery_cmd_t *tool_get_recovery_independent_cmd_list(unsigned int *size)
{
    *size = ARRAY_SIZE_COMPUTE(g_recovery_independent_cmd_list);
    return tool_get_full_independent_cmd_list(g_recovery_independent_cmd_list, size);
}

STATIC recovery_cmd_t *tool_get_recovery_dependent_cmd_list(unsigned int *size)
{
    *size = ARRAY_SIZE_COMPUTE(g_recovery_dependent_cmd_list);
    return tool_get_full_dependent_cmd_list(g_recovery_dependent_cmd_list, size);
}

STATIC int tool_run_recovery_cmd(int (*recovery_cmd_execute)(struct command_context *), struct command_context *ctx)
{
    int retry_times = 0;
    int ret = 0;

    ret = recovery_cmd_execute(ctx);
    while (ret != 0 && retry_times < RECOVER_RETRY_TIMES) {
        retry_times++;
        sleep(1);
        ret = recovery_cmd_execute(ctx);
    }
    return ret;
}

STATIC unsigned int tool_recover_cfg_func(struct command_context *ctx, recovery_cmd_t *cmd_list, unsigned int cmd_size,
                                          const char *cmd_list_desc, bool is_independent)
{
    unsigned int error_flag = 0;
    unsigned int op_index = 0;
    int ret = 0;

    for (; op_index < cmd_size; op_index++) {
        ret = tool_run_recovery_cmd(cmd_list[op_index].recovery_cmd_execute, ctx);
        if (ret != 0) {
            hccn_err("[%s]the recovery of the %s configuration file has failed. ret:%d logic_id:%d op_index:%u)",
                     cmd_list_desc, cmd_list[op_index].recovery_cmd_name, ret, ctx->npu_info.logic_id, op_index);
            LOG_PROMPT(ctx, "recover %s failed, logic_id:%d", cmd_list[op_index].recovery_cmd_name,
                       ctx->npu_info.logic_id);
            error_flag += 1;

            if (is_independent == false && op_index == 0) {
                break;
            }
        }
    }
    return error_flag;
}

STATIC unsigned int tool_recover_independent_cfg(struct command_context *ctx)
{
    recovery_cmd_t *cmd_list = NULL;
    unsigned int cmd_size = 0;

    cmd_list = tool_get_recovery_independent_cmd_list(&cmd_size);
    if (cmd_list == NULL) {
        hccn_err("get recover independent cmd list failed, cmd_list is NULL");
        return 1;
    }

    return tool_recover_cfg_func(ctx, cmd_list, cmd_size, "independent", true);
}

STATIC unsigned int tool_recover_dependent_cfg(struct command_context *ctx)
{
    recovery_cmd_t *cmd_list = NULL;
    unsigned int cmd_size = 0;

    cmd_list = tool_get_recovery_dependent_cmd_list(&cmd_size);
    if (cmd_list == NULL) {
        hccn_err("get recover dependent cmd list failed, cmd_list is NULL");
        return 1;
    }

    return tool_recover_cfg_func(ctx, cmd_list, cmd_size, "dependent", false);
}

STATIC int tool_cfg_cmd_recovery(struct command_context *ctx)
{
    enum dsmi_boot_status boot_status = DSMI_BOOT_STATUS_UNINIT;
    char *dev_name = ctx->dev_name;
    unsigned int error_flag = 0;
    int ret = 0;
    size_t i;

    for (i = 0; i < MAX_BOOT_STATUS_RETRY; i++) {
        (void)dsmi_get_device_boot_status(ctx->npu_info.logic_id, &boot_status);
        if (boot_status == DSMI_SYSTEM_START_FINISH) {
            break;
        }
        sleep(1);
    }

    CHK_PRT_RETURN(boot_status != DSMI_SYSTEM_START_FINISH,
                   hccn_err("dsmi_get_device_boot_status timeout after %d attempts, boot_status:%d",
                            MAX_BOOT_STATUS_RETRY, boot_status),
                   -ETIMEDOUT);

    ret = sprintf_s(dev_name, MAX_DEV_NAME_LEN, "bond%d", ctx->slot_id);
    CHK_PRT_RETURN(
        ret <= 0,
        hccn_err("sprintf_s failed, logic_id:%d slot_id:%d ret:%d", ctx->npu_info.logic_id, ctx->slot_id, ret), -EIO);

    (void)tool_set_cfg_status(ctx, TOOL_NET_CFG_START);
    LOG_PROMPT(ctx, "start to recover the config of device %d", ctx->npu_info.logic_id);

    error_flag += tool_recover_independent_cfg(ctx);
    error_flag += tool_recover_dependent_cfg(ctx);
    if (error_flag > 0) {
        hccn_err("start to recover failed, error_flag:%u logic_id:%d slot_id:%d", error_flag, ctx->npu_info.logic_id,
                 ctx->slot_id);
        (void)tool_set_cfg_status(ctx, TOOL_NET_CFG_FAIL);
        return -1;
    }

    (void)tool_set_cfg_status(ctx, TOOL_NET_CFG_SUCCESS);
    return 0;
}

STATIC int tool_check_dev_health(int logic_id)
{
    unsigned int health = 0;
    unsigned int cnt = 0;
    int ret = 0;

    do {
        cnt += 1;
        ret = dsmi_get_device_health(logic_id, &health);
        if (ret == 0) {
            break;
        } else if (ret != 0 && cnt > MAX_HEALTH_CHECK_CNT) {
            hccn_warn("dsmi get device health timeout, ret:%d", ret);
            return ret;
        }
        hccn_warn("device not ready, ret:%d logic_id:%d cnt:%u", ret, logic_id, cnt);
        sleep(1);
    }
#ifndef CONFIG_LLT
    while (1);
#else
    while (0);
#endif

    if (health == DEV_NO_EXIST) {
        hccn_err("device does not exist, logic_id:%d health:%u", logic_id, health);
        return -1;
    }

    return 0;
}

STATIC int tool_cfg_cmd_recovery_get_info(struct command_context *ctx)
{
    int ret = 0;

    ret = tool_check_dev_health(ctx->npu_info.logic_id);
    CHK_PRT_RETURN(ret != 0, hccn_err("tool_check_dev_health failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
                   ret);

    ret = get_other_id_from_logic_id(ctx);
    CHK_PRT_RETURN(ret != 0,
                   hccn_err("get_other_id_from_logic_id failed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id), ret);

    return 0;
}

STATIC int tool_cfg_cmd_recovery_all(struct command_context *ctx)
{
    int dev_list[DEV_LIST_MAX] = {0};
    int err_count = 0;
    int dev_num = 0;
    int ret = 0;
    int i;

    ret = dsmi_get_device_count(&dev_num);
    CHK_PRT_RETURN(ret != 0 || dev_num <= 0 || dev_num > DEV_LIST_MAX,
                   hccn_err("dsmi_get_device_count failed, ret:%d dev_num:%d", ret, dev_num), ret);

    ret = dsmi_list_device(dev_list, dev_num);
    CHK_PRT_RETURN(ret != 0, hccn_err("dsmi_list_device failed, ret:%d dev_num:%d", ret, dev_num), ret);

    for (i = 0; i < dev_num; i++) {
        ctx->npu_info.logic_id = dev_list[i];
        ret = tool_cfg_cmd_recovery_get_info(ctx);
        if (ret != 0) {
            err_count++;
            hccn_err("tool cfg cmd recovery get info failed, i:%d ret:%d err_count:%d", i, ret, err_count);
            continue;
        }

        ret = tool_cfg_cmd_recovery(ctx);
        if (ret != 0) {
            err_count++;
            hccn_err("tool cfg cmd recovery failed, i:%d ret:%d err_count:%d", i, ret, err_count);
            TOOL_PRINT_ERR("device %d recovery failed", ctx->npu_info.logic_id);
            continue;
        }

        TOOL_PRINT_INFO("device %d recovery success", ctx->npu_info.logic_id);
    }

    if (err_count != 0) {
        hccn_err("tool cfg cmd recovery all failed, err_count:%d", err_count);
        return -1;
    }

    return 0;
}

STATIC int tool_set_cfg_exec(struct command_context *ctx)
{
    int ret = 0;

    ret = hccn_check_usr_identify();
    CHK_PRT_RETURN(
        ret != 0,
        hccn_err("permission denied: only root user is allowed, ret:%d logic_id:%d", ret, ctx->npu_info.logic_id),
        TOOL_PUBLIC_ROOT_UNSUPPORT);

    CHK_PRT_RETURN(access(CFGFILE_NAME, F_OK) != 0,
                   hccn_err("the config file(%s) does not exist, failed to recover the configuration. errno:%d",
                            CFGFILE_NAME, errno),
                   -EINVAL);

    if (ctx->global_flag) {
        return tool_cfg_cmd_recovery_all(ctx);
    } else {
        return tool_cfg_cmd_recovery(ctx);
    }
}

STATIC int tool_cfg_exec(struct command_context *ctx)
{
    if (ctx->mode == MODE_GET) {
        return tool_get_cfg_exec(ctx);
    } else { // MODE_SET
        return tool_set_cfg_exec(ctx);
    }
}

STATIC void tool_cfg_print_help(struct command_context *ctx, unsigned int mode)
{
    switch (mode) {
        case MODE_ALL:
            tool_cfg_print_help(ctx, MODE_GET);
            tool_cfg_print_help(ctx, MODE_SET);
            break;
        case MODE_GET:
            TOOL_PRINT_CMD("-g", "-cfg_recovery", "get config status");
            tool_print_npu_info(DEV_NPU_ATTR);
            break;
        case MODE_SET:
            TOOL_PRINT_CMD("-s", "-cfg_recovery", "recover the configuration of a single device");
            tool_print_npu_info(DEV_NPU_ATTR);

            TOOL_PRINT_CMD("-s", "-cfg_recovery", "recover the configuration of all devices");
            tool_print_npu_info(DEV_GLOBAL_ATTR);
            break;
        default:
            break;
    }
}

STATIC struct cmd_entry g_cfg_cmds[] = {
    {
        "-cfg_recovery",
        NULL,
        MODE_BOTH,
        NULL,
        NULL,
        tool_cfg_exec,
        {DEV_NPU_ATTR, DEV_GLOBAL_ATTR},
        tool_cfg_print_help,
        NULL,
        0,
    },
};

void tool_get_cfg_cmds(struct cmd_entry **cmds, unsigned int *count)
{
    *cmds = g_cfg_cmds;
    *count = ARRAY_SIZE_COMPUTE(g_cfg_cmds);
}
