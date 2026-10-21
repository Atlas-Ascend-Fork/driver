/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bbox_dump_process.h"
#include "bbox_dump_interface.h"
#include "bbox_log_common.h"
#include "bbox_system_api.h"
#include "bbox_print.h"
#include "bbox_tool_fs.h"
#include "bbox_utils.h"
#include "bbox_rdr_pub.h"
#include "bbox_drv_adapter.h"
#include "bbox_feature.h"
#include "securec.h"

#define FLAG_ALL_DONE 5
#define BBOX_SET_TYPE_BIT(status, type) ((status) | (1ULL << (type)))
#define BBOX_TYPE_BIT_NOT_SET(status, type) (((status) & (1ULL << (type))) == 0)

u32 g_vmcore_file_size = 0;

#ifdef CFG_BUILD_DEBUG
/**
 * @brief       store given data as binary file to path
 * @param [in]  config:      data config struct
 * @param [in]  data:        data buffer
 * @param [in]  path:        path to store data
 * @return      NA
 */
STATIC void bbox_dump_bin(const dump_data_config_st *config, u8 *data, const char *path)
{
    s32 ret;
    char bin_path[DIR_MAXLEN] = {0};
    char filename[DIR_MAXLEN] = {0};

    BBOX_CHK_NULL_PTR(config, return);
    BBOX_CHK_NULL_PTR(data, return);
    BBOX_CHK_NULL_PTR(path, return);

    ret = bbox_age_add_folder(path, "bin", bin_path, DIR_MAXLEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return, "path add failed, %s", path);

    ret = sprintf_s(filename, DIR_MAXLEN, "%s.bin", config->name);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret == -1, return, "gen bin filename failed, %s", config->name);

    bbox_save_buf_to_fs(bin_path, filename, (char *)data, config->data_size, BBOX_FALSE);
    BBOX_DBG("write binary file %s/%s.", bin_path, filename);
}
#else
STATIC void bbox_dump_bin(const dump_data_config_st *config, u8 *data, const char *path)
{
    UNUSED(config);
    UNUSED(data);
    UNUSED(path);
    return;
}
#endif

/**
 * @brief       store given data as binary file to path specially for vmcore
 * @param [in]  config:      data config struct
 * @param [in]  data:        data buffer
 * @param [in]  path:        path to store data
 * @return      NA
 */
STATIC bbox_status bbox_dump_vmcore_bin(const dump_data_config_st *config, u8 *data, const char *path)
{
    bbox_status ret;
    char bin_path[DIR_MAXLEN] = {0};
    char filename[DIR_MAXLEN] = {0};
    u32 realsize = 0;

    BBOX_CHK_NULL_PTR(config, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(data, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);

    ret = bbox_age_add_folder(path, "bin", bin_path, DIR_MAXLEN);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return ret, "path add failed, %s", path);

    ret = sprintf_s(filename, DIR_MAXLEN, "%s", config->name);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret == -1, return ret, "gen bin filename failed, %s", config->name);

    realsize = (g_vmcore_file_size < 0xfff00000uL) ? g_vmcore_file_size : (0xfff00000uL);
    ret = bbox_save_buf_to_fs(bin_path, filename, (char *)data, realsize, BBOX_FALSE);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret < 0, return ret, "Save vmcore file failed.");

    BBOX_DBG("write binary file %s/%s.", bin_path, filename);
    return BBOX_SUCCESS;
}

/**
 * @brief       get status of data which is wait for dump
 * @param [in]  phy_id:       device phy id
 * @param [in]  data_config:  data array
 * @param [in]  config_size:  data array size
 * @return      == 0 failed, > 0 success
 */
STATIC u32 bbox_get_dump_data_status(u32 phy_id, int logic_id, const dump_data_config_st *data_config, u32 config_size)
{
    u32 i;
    u32 data_status = 0;
    char dev_buf[DEV_BUF_MAX_LEN];

    BBOX_CHK_NULL_PTR(data_config, return 0);
    BBOX_CHK_INVALID_PARAM(config_size == 0, return 0, "%u", config_size);
    for (i = 0; i < config_size; i++) {
        data_status = (u32)BBOX_SET_TYPE_BIT(data_status, (u32)data_config[i].type);
        if (data_config[i].bbox_data_check_ptr != NULL) {
            bbox_status ret = data_config[i].bbox_data_check_ptr(phy_id);
            if (ret != BBOX_SUCCESS) {
                BBOX_INF("[%s] will stop dump on [%s].", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)),
                         data_config[i].name);
                break;
            }
        }
    }
    return data_status;
}

/**
 * @brief       read data by given type
 * @param [in]  phy_id:       device phy id
 * @param [in]  config:      data config struct
 * @param [out] buf:         buffer to store read data
 * @return      == -1 failed, == 0 success
 */
STATIC bbox_status bbox_dump_data(u32 phy_id, const dump_data_config_st *config, u8 *buf)
{
    BBOX_CHK_NULL_PTR(config, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(buf, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(config->data_size == 0, return BBOX_FAILURE, "%u", config->data_size);

    if (config->bbox_data_dump_ptr == NULL) {
        return BBOX_SUCCESS;
    }
    if (strcmp(config->name, "vmcore") == 0) {
        return config->bbox_data_dump_ptr(
            phy_id, config->phy_offset,
            (u32)((g_vmcore_file_size + DMA_MEMDUMP_MAXLEN - 1) & (~DMA_MEMDUMP_MAXLEN + 1)), buf);
    } else {
        return config->bbox_data_dump_ptr(phy_id, config->phy_offset, config->data_size, buf);
    }
}

/**
 * @brief       call parser to parse data with given type
 * @param [in]  config:      data config struct
 * @param [in]  data:        data buffer
 * @param [in]  path:        path to store data
 * @return      == -1 failed, == 0 success
 */
STATIC bbox_status bbox_parse_data(const dump_data_config_st *config, u8 *data, const char *path)
{
    BBOX_CHK_NULL_PTR(config, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(data, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);

    if (config->bbox_data_parse_ptr == NULL) {
        return BBOX_SUCCESS;
    }
    return config->bbox_data_parse_ptr(config->parse_type, data, config->data_size, path);
}

/**
 * @brief       read & dump given type of data
 * @param [in]  phy_id:       device phy id
 * @param [in]  config:      data config struct
 * @param [in]  path:        path to dump data
 * @return      == -1 failed, == 0 success
 */
STATIC bbox_status bbox_proc_data(u32 phy_id, int logic_id, const dump_data_config_st *config, const char *path)
{
    char dev_buf[DEV_BUF_MAX_LEN];

    BBOX_CHK_NULL_PTR(config, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(config->data_size == 0, return BBOX_FAILURE, "%u", config->data_size);

    u8 *buffer = NULL;
    if (strcmp(config->name, "vmcore") == 0) {
        if (g_vmcore_file_size) {
            buffer = (u8 *)bbox_malloc((g_vmcore_file_size + DMA_MEMDUMP_MAXLEN - 1) & (~DMA_MEMDUMP_MAXLEN + 1));
        } else {
            BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "Get vmcore size failed[%s].", config->name);
            return BBOX_FAILURE;
        }
    } else {
        buffer = (u8 *)bbox_malloc(config->data_size);
    }
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, buffer == NULL, return BBOX_FAILURE, "malloc failed.");

    /* read data */
    bbox_status ret = bbox_dump_data(phy_id, config, buffer);
    if (ret != BBOX_SUCCESS) {
        bbox_free(buffer);
        if (ret == DRV_ERROR_NOT_SUPPORT) {
            return ret;
        }
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "read data[%s] failed.", config->name);
    }
    if (strcmp(config->name, "vmcore_stat") == 0) {
        if (*(u32 *)buffer == FLAG_ALL_DONE) {
            g_vmcore_file_size = *((u32 *)(buffer + sizeof(unsigned int)));
        } else {
            g_vmcore_file_size = 0;
            BBOX_ERR("[%s] status is %u.", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)),
                     *((u32 *)buffer));
        }
    }
    if (strcmp(config->name, "vmcore") == 0) {
        if (bbox_dump_vmcore_bin(config, buffer, path) != BBOX_SUCCESS) {
            bbox_free(buffer);
            return BBOX_FAILURE;
        }
    } else {
        bbox_dump_bin(config, buffer, path);
    }

    /* write data */
    ret = bbox_parse_data(config, buffer, path);
    if (ret != BBOX_SUCCESS) {
        bbox_free(buffer);
        BBOX_ERR_CTRL(BBOX_ERR, return BBOX_FAILURE, "parse data[%s] failed.", config->name);
    }

    bbox_free(buffer);
    return BBOX_SUCCESS;
}

STATIC struct host_exception_list g_host_exception_list[] = {
    {EVENT_OOM, OS_OOM, EXCEPID_AP_OOM, "AP", "OS_OOM", "OOM"},
    {EVENT_LOAD_TIMEOUT, DEVICE_LTO_EXCEPTION, EXCEPID_OSLOAD_TIMEOUT, "DRIVER", "DEVICE_LTO_EXCEPTION", "LTO"},
    {EVENT_HEARTBEAT_LOST, DEVICE_HBL_EXCEPTION, EXCEPID_HEARTBEAT_LOST, "DRIVER", "DEVICE_HBL_EXCEPTION", "HBL"},
    {EVENT_HDC_EXCEPTION, DEVICE_HDC_EXCEPTION, EXCEPID_HDC_EXCEPTION, "DRIVER", "DEVICE_HDC_EXCEPTION", "HDC"},
    {EVENT_DUMP_FORCE, TOOLCHAIN_EXCEPTION, EXCEPID_DUMP_FORCE, "TOOLCHAIN", "TOOLCHAIN_EXCEPTION", "DUMP FORCE"},
    {EVENT_DUMP_VMCORE, TOOLCHAIN_EXCEPTION, EXCEPID_DUMP_VMCORE, "TOOLCHAIN", "TOOLCHAIN_EXCEPTION", "DUMP VMCORE"},
};

STATIC const char *bbox_get_event_name(enum EXCEPTION_EVENT_TYPE event)
{
    u32 i;
    u32 num = sizeof(g_host_exception_list) / sizeof(struct host_exception_list);
    const char *name = "UNDEF";

    for (i = 0; i < num; i++) {
        if (g_host_exception_list[i].event == event) {
            return g_host_exception_list[i].event_name;
        }
    }
    return name;
}

/**
 * @brief       save history log to given path
 * @param [in]  path:       path to store data
 * @param [in]  tms_str:     dump timestamp
 * @param [in]  tms_dir:     dump timestamp
 * @param [in]  event:      exception event
 * @return      NA
 */
STATIC void bbox_save_hist_log(const char *path, const char *tms_str, const char *tms_dir,
                               enum EXCEPTION_EVENT_TYPE event)
{
    s32 i;
    char buf[LOG_MAXLEN] = {0};
    struct host_exception_list *node = NULL;

    for (i = 0; i < (int)(sizeof(g_host_exception_list) / sizeof(struct host_exception_list)); i++) {
        if (g_host_exception_list[i].event == event) {
            node = &g_host_exception_list[i];
            break;
        }
    }

    BBOX_CHK_EXPR_CTRL(BBOX_ERR, node == NULL, return, "does not support event: %d.", (int)event);

    s32 ret = sprintf_s(buf, LOG_MAXLEN,
                        "[%s] system exception code [0x%08X]: ModuleName [%s], "
                        "ExceptionReason [%s], TimeStamp [%s].\n",
                        tms_str, node->except_id, node->module, node->reason, tms_dir);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret == -1, return, "format history log information failed.");

    ret = bbox_save_buf_to_fs(path, HISTORYLOG, buf, (u32)strlen(buf), BBOX_TRUE);
    if (ret < 0) {
        BBOX_ERR("save history log information failed");
        return;
    }
}

/**
 * @brief        create exception dir
 * @param [in]   phy_id:        device id
 * @param [in]   event:        exception event
 * @param [in]   path:         path to store data
 * @param [in]   tms:          timestamp of device log
 * @param [out]  tms_path:      timestamp path
 * @return       == -1 failed, == 0 success
 */
STATIC bbox_status bbox_create_excep_dir(u32 phy_id, int logic_id, enum EXCEPTION_EVENT_TYPE event, const char *path,
                                         const char *tms, char *tms_path)
{
    s32 err;
    bbox_status ret;
    struct timeval tv = {0};
    char tms_dir[DATE_MAXLEN] = {0};
    char host_tms[HOST_DATE_MAXLEN] = {0};
    char dev_path[DIR_MAXLEN] = {0};
    const char *except_tms = tms; // use device timestamp by default

    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(tms_path, return BBOX_FAILURE);

    // create device-x dir
    ret = bbox_create_excep_dir_adapt(phy_id, logic_id, event, path, dev_path);
    if (ret != BBOX_SUCCESS) {
        return BBOX_FAILURE;
    }

    ret = bbox_get_time_of_day(&tv);
    if (ret != BBOX_SUCCESS) {
        BBOX_PERROR("gettimeofday", "");
        return BBOX_FAILURE;
    }
    // create timestmp dir
    if (tms == NULL) {
        struct bbox_time time = {0, 0};
        time.tv_sec = (u64)tv.tv_sec;
        time.tv_nsec = (u64)tv.tv_usec * NSEC_PER_USEC;
        ret = bbox_time_to_str(tms_dir, DATE_MAXLEN, &time);
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "generate timestamp failed.");
        except_tms = tms_dir;
    }
    err = bbox_format_path(tms_path, DIR_MAXLEN, dev_path, except_tms);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, err == -1, return BBOX_FAILURE, "format path[%s" OS_DIR_SLASH "%s] failed.", tms_path,
                       except_tms);

    ret = bbox_mkdir_recur(tms_path);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "create dir[%s] failed.", tms_path);

    // create history.log
    bbox_host_time_to_str(host_tms, HOST_DATE_MAXLEN, &tv);
    bbox_save_hist_log(dev_path, host_tms, except_tms, event);
    return BBOX_SUCCESS;
}

/**
 * @brief       read data for given device id and store to path
 * @param [in]  phy_id:      device id
 * @param [in]  path:       path to store data
 * @param [in]  event:      exception event
 * @param [in]  tms:        timestamp of device log
 * @return      == -1 failed, == 0 success
 */
bbox_status bbox_dump_excep_event(u32 phy_id, const char *path, enum EXCEPTION_EVENT_TYPE event, const char *tms,
                                  int logic_id)
{
    bool dump_done = true;
    char dev_buf[DEV_BUF_MAX_LEN];
    char tms_path[DIR_MAXLEN] = {0};

    BBOX_CHK_NULL_PTR(path, return BBOX_FAILURE);
    BBOX_CHK_INVALID_PARAM(phy_id >= MAX_PHY_DEV_NUM, return BBOX_FAILURE, "%u", phy_id);
    BBOX_INF("[%s] bbox exception event(%s), dump start.", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)),
             bbox_get_event_name(event));

    const struct dump_data_config *data_config = bbox_get_data_config(event);
    u32 config_size = bbox_get_data_config_size(event);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, (data_config == NULL || config_size == 0), return BBOX_FAILURE,
                       "[%s] event[%d] config is invalid.",
                       bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)), (int)event);

    u32 data_status = bbox_get_dump_data_status(phy_id, logic_id, data_config, config_size);
    bbox_status ret = bbox_create_excep_dir(phy_id, logic_id, event, path, tms, tms_path);
    BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "create dir[%s] failed.", tms_path);

    bbox_write_done_file(tms_path, STORE_STARTING);
    u32 idx;
    for (idx = 0; idx < config_size; idx++) {
        u32 type = (u32)data_config[idx].type;
        const char *name = data_config[idx].name;
        if (BBOX_TYPE_BIT_NOT_SET(data_status, type)) {
            continue;
        }
        ret = bbox_proc_data(phy_id, logic_id, &data_config[idx], tms_path);
        if (ret != BBOX_SUCCESS) {
            if (ret != DRV_ERROR_NOT_SUPPORT) {
                BBOX_ERR("[%s] dump type[%s] failed.", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)),
                         name);
                dump_done = false;
            }
            continue;
        }
        BBOX_INF("[%s] dump type[%s] succeed.", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)), name);
    }
    bbox_write_done_file(tms_path, dump_done ? STORE_DONE : STORE_FAILED);
    BBOX_INF("[%s] bbox exception event(%s), dump end.", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)),
             bbox_get_event_name(event));
    return dump_done ? BBOX_SUCCESS : BBOX_FAILURE;
}

/**
 * @brief        get timestamp from device log info
 * @param [in]   info:      log info
 * @param [out]  tms:       timestamp string
 * @param [in]   len:       timestamp string length
 * @return       == -1 failed, == 0 success
 */
bbox_status bbox_get_device_log_tms(const struct rdr_log_info *info, char *tms, s32 len)
{
    BBOX_CHK_NULL_PTR(info, return BBOX_FAILURE);
    BBOX_CHK_NULL_PTR(tms, return BBOX_FAILURE);
    u32 i;
    u32 next = (info->log_num < RDR_LOG_BUFFER_NUM) ? 0UL : ((u32)info->next_valid_index % RDR_LOG_BUFFER_NUM);

    for (i = 0; i < RDR_MIN(info->log_num, RDR_LOG_BUFFER_NUM); i++) {
        if (info->log_buffer[next].record_info.e_type == OS_OOM) {
            s32 err = bbox_time_to_str(tms, (u32)len, &info->log_buffer[next].record_info.tm);
            BBOX_CHK_EXPR_CTRL(BBOX_ERR, err == -1, return BBOX_FAILURE, "bbox_time_to_str failed.");
        }
        next++;
        next %= RDR_LOG_BUFFER_NUM;
    }
    return BBOX_SUCCESS;
}

STATIC bbox_status bbox_dump_dev_event(u32 phy_id, const char *path, int logic_id)
{
    u16 event = 0;
    char tms[DATE_MAXLEN] = {0};
    char dev_buf[DEV_BUF_MAX_LEN];

    bbox_status ret = bbox_check_dev_event(phy_id, logic_id, &event, tms, DATE_MAXLEN);
    if (ret != BBOX_SUCCESS || event >= DEVICE_EVENT_MAX) {
        BBOX_ERR("[%s] check event failed with %d", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)), ret);
        return BBOX_FAILURE;
    }

    if ((event & DEVICE_EVENT_OOM_TRIGGER) != 0) {
        const char *except_tms = (strlen(tms) == 0) ? NULL : tms;
        ret = bbox_dump_excep_event(phy_id, path, EVENT_OOM, except_tms, logic_id);
        BBOX_CHK_EXPR_CTRL(BBOX_ERR, ret != BBOX_SUCCESS, return BBOX_FAILURE, "[%s] dump oom event data failed.",
                           bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)));
    }
    return BBOX_SUCCESS;
}

bbox_status bbox_dump_runtime(u32 phy_id, const char *path, enum BBOX_DUMP_MODE mode, int logic_id)
{
    char dev_buf[DEV_BUF_MAX_LEN];
    u32 master_id = 0;
    bool err = false;

    drvError_t drv_ret = bbox_drv_get_master_dev_id(phy_id, &master_id);
    if (drv_ret != DRV_ERROR_NONE) {
        BBOX_ERR("[%s] get masterid failed with %d", bbox_get_dev_str(phy_id, logic_id, dev_buf, sizeof(dev_buf)),
                 (int)drv_ret);
        return BBOX_FAILURE;
    }

    if (phy_id != master_id) {
        return BBOX_SUCCESS;
    }

    // dump device files
    bbox_status ret = bbox_dump_dev_files(phy_id, logic_id, path);
    if (ret != BBOX_SUCCESS) {
        // dump HDC exception
        ret = bbox_dump_excep_event(phy_id, path, EVENT_HDC_EXCEPTION, NULL, logic_id);
        err = ((ret == BBOX_SUCCESS) ? err : true);
    }

    // dump device event, like oom
    if (mode == BBOX_DUMP_MODE_ALL || mode == BBOX_DUMP_MODE_FORCE) {
        ret = bbox_dump_dev_event(phy_id, path, logic_id);
        err = ((ret == BBOX_SUCCESS) ? err : true);
    }

    return (err == false) ? BBOX_SUCCESS : BBOX_FAILURE;
}
