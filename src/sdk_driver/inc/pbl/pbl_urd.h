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

#ifndef PBL_URD_H
#define PBL_URD_H

#include "ka_type.h"
#include "ka_dfx_pub.h"
#include "pbl_urd_common.h"

struct urd_cmd_kernel_para {
    u32 udevid;
    u32 phyid;
};
typedef s32 (*feature_handler)(void *feature, char *in, u32 in_len, char *out, u32 out_len);
typedef int (*dev_feature_handler)(const struct urd_cmd *cmd, struct urd_cmd_kernel_para *kernel_para,
                                   struct urd_cmd_para *para);

#define URD_HANDLER 0
#define URD_DEV_HANDLER 1
typedef struct tag_dms_feature {
    const char *owner_name;
    u32 main_cmd;
    u32 sub_cmd;
    const char *filter;
    const char *proc_ctrl_str;
    u32 privilege;
    u32 handler_type;
    union {
        feature_handler handler;
        dev_feature_handler dev_handler;
        void *call_fun;
    };
} DMS_FEATURE_S;

/* To facilitate expansion, recommended use template */
int dms_feature_register(DMS_FEATURE_S *feature);
int dms_feature_unregister(DMS_FEATURE_S *feature);

ssize_t dms_feature_print_feature_list(char *buf);

#define URD_NOTIFIER_INIT (0x1)
#define URD_NOTIFIER_RELEASE (0x2)
int urd_register_notifier(ka_notifier_block_t *nb);
int urd_unregister_notifier(ka_notifier_block_t *nb);

int dms_cmd_process_from_kernel(u32 devid, struct urd_cmd *cmd, struct urd_cmd_para *cmd_para);

/* template */
#define BEGIN_DMS_MODULE_DECLARATION(module)                      \
    static DMS_FEATURE_S *_feature_table = NULL;                  \
    static u32 _feature_table_size = 0;                           \
    static void _init_feature_table(void);                        \
    static int _init_module_##module(void)                        \
    {                                                             \
        int ret = 0;                                              \
        unsigned int i;                                           \
        _init_feature_table();                                    \
        for (i = 0; i < _feature_table_size; i++) {               \
            ret = dms_feature_register(&_feature_table[i]);       \
            if (ret != 0) {                                       \
                break;                                            \
            }                                                     \
        }                                                         \
        if (ret != 0) {                                           \
            while (i > 0) {                                       \
                i--;                                              \
                (void)dms_feature_unregister(&_feature_table[i]); \
            }                                                     \
        }                                                         \
        return 0;                                                 \
    }                                                             \
    int init_module_##module(void);                               \
    int init_module_##module(void)                                \
    {                                                             \
        return _init_module_##module();                           \
    }                                                             \
    static int _exit_module_##module(void)                        \
    {                                                             \
        int ret = 0;                                              \
        int tmp_ret;                                              \
        unsigned int i;                                           \
        for (i = 0; i < _feature_table_size; i++) {               \
            tmp_ret = dms_feature_unregister(&_feature_table[i]); \
            if ((tmp_ret != 0) && (ret == 0)) {                   \
                ret = tmp_ret;                                    \
            }                                                     \
        }                                                         \
        return ret;                                               \
    }                                                             \
    int exit_module_##module(void);                               \
    int exit_module_##module(void)                                \
    {                                                             \
        return _exit_module_##module();                           \
    }

#define END_MODULE_DECLARATION()

#define BEGIN_FEATURE_COMMAND() static DMS_FEATURE_S _feature_def[] = {
#define ADD_FEATURE_COMMAND(o, m, s, f, l, p, handler) {o, m, s, f, l, p, URD_HANDLER, .call_fun = handler},
#define ADD_DEV_FEATURE_COMMAND(o, m, s, f, l, p, dev_handler) \
    {o, m, s, f, l, p, URD_DEV_HANDLER, .call_fun = dev_handler},
#define END_FEATURE_COMMAND()                                                 \
    }                                                                         \
    ;                                                                         \
    static void _init_feature_table(void)                                     \
    {                                                                         \
        _feature_table = _feature_def;                                        \
        _feature_table_size = sizeof(_feature_def) / sizeof(_feature_def[0]); \
    }

#define INIT_MODULE_FUNC(module) int init_module_##module(void)

#define EXIT_MODULE_FUNC(module) int exit_module_##module(void)

#define CALL_INIT_MODULE(module) (void)init_module_##module()

#define CALL_EXIT_MODULE(module) (void)exit_module_##module()

#endif
