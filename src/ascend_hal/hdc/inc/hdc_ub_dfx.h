/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __HDC_UB_DFX_H__
#define __HDC_UB_DFX_H__

#ifndef HDC_UT_TEST
#include "mmpa_api.h"
#endif

#ifdef CFG_FEATURE_SUPPORT_UB

struct hdc_time_record_for_single_send {
    struct timespec ub_send_start;
    struct timespec find_idle_block;
    struct timespec fill_jfs_wr;
    struct timespec post_jfs_wr_start;
    struct timespec wait_jfc_send_start;
    struct timespec wait_jfc_send_finish;
    struct timespec poll_jfc_send;
    struct timespec ub_send_end;
    int fail_times;
};

struct hdc_time_record_for_single_recv {
    bool recv_peek_flag;
    struct timespec ub_recv_peek_start;
    struct timespec wait_jfc_recv;
    struct timespec poll_jfc_recv;
    struct timespec ack_and_rearm_jfc;
    struct timespec ub_recv_peek_end;
    struct timespec ub_recv_start;
    struct timespec copy_buf_to_user;
    struct timespec ub_recv_end;
    int fail_times;
};

struct hdc_time_record_for_urma_func {
    struct timespec urma_init_start;
    struct timespec lock_wait;
    struct timespec ctx_calloc;
    bool need_init;
    struct timespec hdc_init_urma_start;
    struct timespec attr_calloc;
    struct timespec create_ctx;
    struct timespec alloc_token_id;
    struct timespec urma_init_end;
    struct timespec res_init_start;
    struct timespec own_reg_re_start;
    struct timespec own_reg_re_end;
    struct timespec register_own_seg;
    struct timespec reg_dmp_end;
    struct timespec alloc_tid_end;
    struct timespec own_reg_new_end;
    struct timespec create_jfce;
    struct timespec create_jfc;
    struct timespec rearm_jfc;
    struct timespec create_jfs;
    struct timespec create_jfr;
    struct timespec post_jfr_wr;
    struct timespec res_init_end;
    struct timespec get_tp_list;
    struct timespec import_jfr;
};

struct hdc_time_record_for_dms_func {
    struct timespec dms_start;
    struct timespec get_token_val;
    struct timespec get_dev_info;
    struct timespec dms_end;
};

struct hdc_time_record_for_urma_init {
    struct hdc_time_record_for_dms_func dms_record;
    struct hdc_time_record_for_urma_func urma_record;
    int fail_times;
};

struct hdc_time_record_for_accept {
    struct timespec accept_start;
    struct timespec conn_wait;
    struct timespec res_init;
    struct timespec alloc_session;
    struct timespec pre_init;
    struct timespec create_ub_ctx;
    struct timespec add_ctrl;
    struct timespec send_ctrl_msg;
    struct timespec accept_end;
    struct timespec pipe_write_start;
    struct hdc_time_record_for_urma_init urma_func_cost;
    int fail_times;
};

struct hdc_time_record_for_connect {
    struct timespec connect_start;
    struct timespec link_pre_init;
    struct timespec mem_res_init;
    struct timespec alloc_session;
    struct timespec pre_init;
    struct timespec create_ub_ctx;
    struct timespec get_res_info_and_add_ctrl;
    struct timespec send_ctrl_msg;
    struct timespec check_reply;
    struct timespec free_tid_and_fill_jetty_info;
    struct timespec connect_end;
    struct hdc_time_record_for_urma_init urma_func_cost;
    int fail_times;
};

struct hdc_time_record_for_remote_close {
    struct timespec close_proc;
    struct timespec del_jfs;
    struct timespec del_jfcs;
    int fail_times;
};

struct hdc_time_record_for_urma_uninit {
    struct timespec del_jfr;
    struct timespec wait_data_fin;
    struct timespec del_jfcr;
    struct timespec del_jfs_1;
    struct timespec del_jfcs_1;
    struct timespec own_unreg;
    bool need_uninit;
    struct timespec share_unreg;
    struct timespec free_token_id;
    struct timespec del_ctx;
    int fail_times;
};

struct hdc_time_record_for_close {
    int dev_id;
    int service_type;
    unsigned int local_id;
    int close_type;
    struct hdc_time_record_for_remote_close *remote_close;
    struct timespec start_close_handle;
    struct timespec del_recv_epoll;
    struct timespec sub_close_event;
    struct timespec del_data_epoll;
    struct timespec unimport_jetty;
    struct timespec del_ub_ctx;
    struct timespec close_kernel;
    struct timespec mem_uninit;
    struct timespec wake_recv;
    struct timespec close_notify;
    struct hdc_time_record_for_urma_uninit urma_uninit;
    struct timespec write_file;      // remote_close
    struct timespec session_free;    // user_close
    struct timespec del_close_epoll; // user_close
    int fail_times;
};

#endif

#endif /* __HDC_UB_DFX_H__ */
