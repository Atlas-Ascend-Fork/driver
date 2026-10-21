/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <pthread.h>
#include <sys/mman.h>
#include "securec.h"
#include "dmc/dmc_log_user.h"
#include "drv_current.h"
#include "drv_log_user_kernel_api.h"

// this file is a common.c

int32_t errno_to_user_errno(int32_t kern_err_no)
{
    return errno_to_user_errno_inner(kern_err_no);
}

const char *halDrvLogGetModuleStr(enum devdrv_module_type module)
{
    return drv_log_get_module_str_inner(module);
}

int32_t drv_log_out_handle_register(struct log_out_handle *handle, size_t input_size, uint32_t flag)
{
    return drv_log_out_handle_register_inner(handle, input_size, flag);
}

/* compatibility */
int32_t halIsRunLog(void)
{
    return is_run_log_inner();
}

int32_t drv_log_out_handle_unregister(void)
{
    return drv_log_out_handle_unregister_inner();
}

uint32_t halGetConLogLevel(void)
{
    return get_con_log_level_inner();
}

const char *halGetLogGetLevelString(uint32_t level)
{
    return get_log_get_level_string_inner(level);
}

const char *halGetLogGetLevelStringDefault(uint32_t level)
{
    return drv_log_get_level_str_default(level);
}

const char *halGetLogGetPrintTime(void)
{
    return get_log_get_print_time_inner();
}

const char *halGetLogGetPrintTimeDefault(void)
{
    return drv_get_tm_default();
}

uint32_t halGetLogLevelShift(uint32_t level)
{
    return get_log_level_shift_inner(level);
}

void (*halGetLogPrint(void))(int32_t, int32_t, const char *, ...)
{
    return get_log_print_inner();
}

uint32_t get_con_log_level(void)
{
    return get_con_log_level_inner();
}

const char *get_log_get_level_string(uint32_t level)
{
    return get_log_get_level_string_inner(level);
}

const char *get_log_get_print_time(void)
{
    return get_log_get_print_time_inner();
}

uint32_t get_log_level_shift(uint32_t level)
{
    return get_log_level_shift_inner(level);
}

const char *drv_log_get_module_str(enum devdrv_module_type module)
{
    return drv_log_get_module_str_inner(module);
}

void (*get_log_Print(void))(int32_t, int32_t, const char *, ...)
{
    return get_log_print_inner();
}

int32_t drv_log_report_err_msg_handle_register(struct err_msg_report_handle *handle, size_t input_size)
{
    return drv_log_report_err_msg_handle_register_impl(handle, input_size);
}

int32_t drv_log_report_err_msg_handle_unregister(void)
{
    return drv_log_report_err_msg_handle_unregister_impl();
}

register_format_err_msg_func get_format_err_msg_register_func(void)
{
    return get_format_err_msg_register_func_impl();
}

report_predefined_err_msg_func get_predefined_err_msg_report_func(void)
{
    return get_predefined_err_msg_report_func_impl();
}

report_inner_err_msg_func get_inner_err_msg_report_func(void)
{
    return get_inner_err_msg_report_func_impl();
}

static void report_arg_out_of_range_inner(const char *func_name, const char *para_name, const char *para_value_str,
                                          const char *reason)
{
    unsigned long arg_num = 4;
    const char *keys[] = {"func_name", "para_value", "para_name", "reason"};
    const char *values[] = {func_name, para_value_str, para_name, reason};
    REPORT_PREDEFINED_ERR_MSG("EL0016", keys, values, arg_num);
}

void report_arg_out_of_range(const char *func_name, const char *para_name, unsigned int para_value, unsigned int left,
                             unsigned int right)
{
    int ret;
    char para_value_str[MAX_PARA_VALUE_STR_SIZE] = {0};
    char reason[MAX_REASON_STR_SIZE] = {0};
    ret = snprintf_s(para_value_str, sizeof(para_value_str), sizeof(para_value_str), "%u", para_value);
    if (ret < 0) {
        return;
    }
    ret = snprintf_s(reason, sizeof(reason), sizeof(reason),
                     "The parameter value is out of range. The valid range is [%u,%u]", left, (right - 1));
    if (ret < 0) {
        return;
    }
    report_arg_out_of_range_inner(func_name, para_name, para_value_str, reason);
}

void report_enum_arg_out_of_range(const char *func_name, const char *para_name, unsigned int para_value,
                                  const char *enum_name)
{
    int ret = 0;
    char para_value_str[MAX_PARA_VALUE_STR_SIZE] = {0};
    char reason[MAX_REASON_STR_SIZE] = {0};
    ret = snprintf_s(para_value_str, sizeof(para_value_str), sizeof(para_value_str), "UNKNOWN(%u)", para_value);
    if (ret < 0) {
        return;
    }
    ret = snprintf_s(reason, sizeof(reason), sizeof(reason),
                     "The input parameter is not within the defined scope of enumeration type %s", enum_name);
    if (ret < 0) {
        return;
    }
    report_arg_out_of_range_inner(func_name, para_name, para_value_str, reason);
}

void report_arg_equal_to_zero(const char *func_name, const char *para_name)
{
    unsigned long arg_num = 4;
    const char *keys[] = {"func_name", "para_value", "para_name", "reason"};
    const char *values[] = {func_name, "0", para_name, "The input parameter must be greater than 0"};
    REPORT_PREDEFINED_ERR_MSG("EL0016", keys, values, arg_num);
}
void report_arg_null_pointer(const char *func_name, const char *para_name)
{
    unsigned long arg_num = 2;
    const char *keys[] = {"func_name", "para_name"};
    const char *values[] = {func_name, para_name};
    REPORT_PREDEFINED_ERR_MSG("EL0017", keys, values, arg_num);
}
static void report_insufficient_memory_inner(const char *error_code, unsigned long long size, const char *module_name)
{
    const char *func_name = drv_current->hal_api_name;
    int ret = 0;
    unsigned long arg_num = 3;
    char size_str[MAX_MEM_SIZE_STR_SIZE] = {0};
    ret = snprintf_s(size_str, sizeof(size_str), sizeof(size_str), "%llu bytes", size);
    if (ret < 0) {
        return;
    }
    const char *keys[] = {"func_name", "size", "moduleName"};
    const char *values[] = {func_name, size_str, module_name};
    REPORT_PREDEFINED_ERR_MSG(error_code, keys, values, arg_num);
}
void report_insufficient_host_memory(unsigned long long size, const char *module_name)
{
    report_insufficient_memory_inner("EL0018", size, module_name);
}
void report_insufficient_device_memory(unsigned long long size, const char *module_name)
{
    report_insufficient_memory_inner("EL0019", size, module_name);
}