/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/time.h>

#include "ascend_hal.h"
#include "comm_user_interface.h"
#include "hdc_cmn.h"
#include "hdc_ub_drv.h"
#include "hdc_core.h"

#define HDC_PERF_DEC_NOTATION 10

STATIC bool g_hdc_perf_log_not_limit = false;

bool hdc_ub_perf_log_not_limit(void)
{
    return g_hdc_perf_log_not_limit;
}

void hdc_ub_perf_log_env_init(void)
{
#ifdef CFG_BUILD_DEBUG
    g_hdc_perf_log_not_limit = (getenv(HDC_PERF_LOG_LIMIT_TIME_ENV) != NULL);
#endif
    return;
}

// basic interface
int hdc_get_lock_index(int dev_id, int session_id)
{
    return (dev_id * HDCDRV_UB_SINGLE_DEV_MAX_SESSION + session_id);
}

STATIC unsigned long long hdc_get_perf_stander(const char *env_str, unsigned long long default_val)
{
    char *env, *end_ptr;
    unsigned long long ret;

    env = getenv(env_str);
    if (env == NULL) {
        return default_val;
    }
    errno = 0;
    ret = strtoull(env, &end_ptr, HDC_PERF_DEC_NOTATION);
    if ((env == end_ptr) || (errno == ERANGE)) {
        return default_val;
    }

    return ret;
}

void hdc_ub_fill_jetty_info(hdcdrv_jetty_info_t *info, struct hdc_ub_session *session)
{
    info->session_jfc_recv_id = session->ctx->jfc_recv.jfc->jfc_id.id;
    info->session_jfc_send_id = session->ctx->jfc_send.jfc->jfc_id.id;
    info->session_jfs_id = session->ctx->jfs.jfs->jfs_id.id;
    info->session_jfr_id = session->ctx->jfr.jfr->jfr_id.id;

    return;
}

void hdc_ub_init_dfx_info(hdc_ub_dbg_stat_t *dbg_stat)
{
    dbg_stat->tx = 0;
    dbg_stat->tx_bytes = 0;
    dbg_stat->rx_epoll = 0;
    dbg_stat->rx_full = 0;
    dbg_stat->rx_add_list = 0;
    dbg_stat->rx = 0;
    dbg_stat->rx_bytes = 0;
    dbg_stat->tx_full = 0;
    dbg_stat->tx_fail_hdc = 0;
    dbg_stat->tx_fail_ub = 0;
    dbg_stat->remote_rx_full = 0;
    dbg_stat->remote_rx_fail = 0;
    dbg_stat->epoll_thread_create = 0;
    dbg_stat->epoll_thread_epfd = -1;
    dbg_stat->recv_event_abnormal = 0;

    return;
}

unsigned int hdc_get_jfc_id_by_type(hdcdrv_jetty_info_t *jetty_info, int tx_rx_flag)
{
    if (tx_rx_flag == HDC_UB_TX) {
        return jetty_info->session_jfc_send_id;
    }

    return jetty_info->session_jfc_recv_id;
}

// hdc fill dfx
void hdc_jfc_dbg_fill(int tx_rx_flag, struct hdc_ub_session *session, enum drv_hdc_ub_op op)
{
    if (tx_rx_flag == HDC_UB_TX) {
        switch (op) {
            case HDC_URMA_POLL_FAIL_BY_REM_ACESS_ABORT:
                session->dbg_stat.remote_rx_full++;
                break;
            case HDC_URMA_WAIT_FAIL:
            case HDC_URMA_POLL_FAIL:
            case HDC_URMA_REARM_FAIL:
                session->dbg_stat.tx_fail_ub++;
                break;
            default:
                break;
        }
    } else {
        session->dbg_stat.rx_fail_ub++;
    }
}

int hdc_session_alive_check(int dev_id, int l_id, unsigned int unique_val)
{
    int idx;
    int ret = 0;

    if ((l_id < 0) || (l_id >= HDCDRV_UB_SINGLE_DEV_MAX_SESSION)) {
        HDC_LOG_ERR("session para invalid.(id=%d; dev_id=%d)\n", l_id, dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((dev_id < 0) || (dev_id >= hdc_get_max_device_num())) {
        HDC_LOG_ERR("session para invalid.(id=%d; dev_id=%d)\n", l_id, dev_id);
        return DRV_ERROR_INVALID_VALUE;
    }

    idx = hdc_get_lock_index(dev_id, l_id);
    if ((g_hdcConfig.info_list[idx].status == HDC_SESSION_STATUS_IDLE) ||
        (g_hdcConfig.info_list[idx].unique_val != unique_val)) {
        ret = -HDCDRV_SESSION_HAS_CLOSED;
    }

    return ret;
}

void hdc_get_time_record(struct timespec *time_val, int *fail_count)
{
    int ret;
    int count = *fail_count;

    ret = clock_gettime(CLOCK_MONOTONIC, time_val);
    if (ret != 0) {
        count++;
    }

    *fail_count = count;

    return;
}

unsigned long long hdc_get_time_cost(struct timespec *start_tval, struct timespec *end_tval)
{
    unsigned long long cost_s, cost_ns;

    cost_s = (unsigned long long)(end_tval->tv_sec - start_tval->tv_sec);
    cost_ns = (unsigned long long)((CONVERT_S_TO_NS + end_tval->tv_nsec - start_tval->tv_nsec) % CONVERT_S_TO_NS);

    if (end_tval->tv_nsec < start_tval->tv_nsec) {
        cost_s--;
    }

    return (cost_s * CONVERT_S_TO_US) + (cost_ns / CONVERT_US_TO_NS);
}

STATIC void hdc_get_time_record_for_single_urma_init(struct hdc_time_record_for_urma_init *record,
                                                     hdc_urma_init_info_t *urma_timecost)
{
    if (record->urma_record.need_init) {
        urma_timecost->attr_calloc = hdc_get_time_cost(&record->urma_record.hdc_init_urma_start,
                                                       &record->urma_record.attr_calloc);
        urma_timecost->get_dev_info = hdc_get_time_cost(&record->urma_record.attr_calloc,
                                                        &record->dms_record.get_dev_info);
        urma_timecost->create_ctx = hdc_get_time_cost(&record->dms_record.get_dev_info,
                                                      &record->urma_record.create_ctx);
        urma_timecost->alloc_token_id = hdc_get_time_cost(&record->urma_record.create_ctx,
                                                          &record->urma_record.alloc_token_id);
        urma_timecost->register_share_seg = hdc_get_time_cost(&record->urma_record.alloc_token_id,
                                                              &record->urma_record.urma_init_end);
    }

    return;
}

STATIC void hdc_get_time_record_for_urma_res_init(struct hdc_time_record_for_urma_init *record,
                                                  struct timespec *import_jetty_start,
                                                  hdc_urma_init_info_t *urma_timecost)
{
    urma_timecost->urma_init_end = hdc_get_time_cost(&record->urma_record.lock_wait,
                                                     &record->urma_record.urma_init_end);
    urma_timecost->lock_wait = hdc_get_time_cost(&record->urma_record.urma_init_start, &record->urma_record.lock_wait);
    urma_timecost->ctx_calloc = hdc_get_time_cost(&record->urma_record.ctx_calloc, &record->dms_record.get_token_val);
    urma_timecost->get_token_val = hdc_get_time_cost(&record->dms_record.get_token_val,
                                                     &record->urma_record.urma_init_start);
    if (record->urma_record.own_reg_re_end.tv_sec != 0) {
        urma_timecost->own_reg_re = hdc_get_time_cost(&record->urma_record.own_reg_re_start,
                                                      &record->urma_record.own_reg_re_end);
    }
    if (record->urma_record.own_reg_new_end.tv_sec != 0) {
        struct timespec *new_start = (record->urma_record.own_reg_re_end.tv_sec != 0) ?
                                         &record->urma_record.own_reg_re_end :
                                         &record->urma_record.res_init_start;
        urma_timecost->reg_alloc_tid = hdc_get_time_cost(new_start, &record->urma_record.alloc_tid_end);
        urma_timecost->own_reg_new = hdc_get_time_cost(&record->urma_record.alloc_tid_end,
                                                       &record->urma_record.own_reg_new_end);
    }
    urma_timecost->reg_dmp = hdc_get_time_cost(&record->urma_record.register_own_seg, &record->urma_record.reg_dmp_end);
    urma_timecost->create_jfc = hdc_get_time_cost(&record->urma_record.register_own_seg,
                                                  &record->urma_record.create_jfc);
    urma_timecost->create_jfs = hdc_get_time_cost(&record->urma_record.create_jfc, &record->urma_record.create_jfs);
    urma_timecost->create_jfr = hdc_get_time_cost(&record->urma_record.create_jfs, &record->urma_record.create_jfr);
    urma_timecost->post_jfr_wr = hdc_get_time_cost(&record->urma_record.create_jfr, &record->urma_record.post_jfr_wr);
    urma_timecost->res_init_end = hdc_get_time_cost(&record->urma_record.urma_init_end,
                                                    &record->urma_record.res_init_end);
    urma_timecost->get_tp_list = hdc_get_time_cost(import_jetty_start, &record->urma_record.get_tp_list);
    urma_timecost->import_jfr = hdc_get_time_cost(&record->urma_record.get_tp_list, &record->urma_record.import_jfr);

    return;
}

void hdc_get_urma_init_time_cost(struct hdc_time_record_for_urma_init *record, struct hdc_ub_session *session,
                                 struct timespec *import_jetty_start, hdc_urma_init_info_t *urma_timecost,
                                 int log_limit)
{
    if (record->fail_times > 0) {
        HDC_PERF_LOG_RUN_INFO_LIMIT("hdc perf data not get successfully.(dev_id=%d; l_id=%d; type=\'%s\'\n",
                                    session->dev_id, session->local_id, hdc_get_sevice_str(session->service_type));
    }

    hdc_get_time_record_for_single_urma_init(record, urma_timecost);
    hdc_get_time_record_for_urma_res_init(record, import_jetty_start, urma_timecost);

    if (record->urma_record.need_init) {
        HDC_PERF_LOG_RUN_INFO(
            log_limit,
            "hdc urma_cost info (us).(dev_id=%d; l_id=%u; unique_val=%u; type=%d; "
            "ctx_c=%llu; get_token=%llu; attr_c:%llu; dev_info=%llu; urma_ctx=%llu; alloc_id=%llu; share_seg=%llu; "
            "lock_wait=%llu; urma_init=%llu; own_reg_re=%llu; alloc_tid=%llu; own_reg(new)=%llu; reg_dmp=%llu; "
            "jfc=%llu; jfs=%llu; jfr:%llu; post_wr=%llu; res_init=%llu; get_tp=%llu; import=%llu)\n",
            session->dev_id, session->local_id, session->unique_val, session->service_type, urma_timecost->ctx_calloc,
            urma_timecost->get_token_val, urma_timecost->attr_calloc, urma_timecost->get_dev_info,
            urma_timecost->create_ctx, urma_timecost->alloc_token_id, urma_timecost->register_share_seg,
            urma_timecost->lock_wait, urma_timecost->urma_init_end, urma_timecost->own_reg_re,
            urma_timecost->reg_alloc_tid, urma_timecost->own_reg_new, urma_timecost->reg_dmp, urma_timecost->create_jfc,
            urma_timecost->create_jfs, urma_timecost->create_jfr, urma_timecost->post_jfr_wr,
            urma_timecost->res_init_end, urma_timecost->get_tp_list, urma_timecost->import_jfr);
    } else {
        HDC_PERF_LOG_RUN_INFO(
            log_limit,
            "hdc urma_cost info (us).(dev_id=%d; l_id=%u; unique_val=%u; type=%d; "
            "ctx_c=%llu; get_token=%llu; lock_wait=%llu; urma_init=%llu; own_reg_re=%llu; "
            "alloc_tid=%llu; own_reg(new)=%llu; reg_dmp=%llu; jfc=%llu; jfs=%llu; jfr:%llu; post_wr=%llu; "
            "res_init=%llu; get_tp=%llu; import=%llu\n",
            session->dev_id, session->local_id, session->unique_val, session->service_type, urma_timecost->ctx_calloc,
            urma_timecost->get_token_val, urma_timecost->lock_wait, urma_timecost->urma_init_end,
            urma_timecost->own_reg_re, urma_timecost->reg_alloc_tid, urma_timecost->own_reg_new, urma_timecost->reg_dmp,
            urma_timecost->create_jfc, urma_timecost->create_jfs, urma_timecost->create_jfr, urma_timecost->post_jfr_wr,
            urma_timecost->res_init_end, urma_timecost->get_tp_list, urma_timecost->import_jfr);
    }
}

STATIC unsigned long long hdc_calc_kernel_cost(unsigned long long start_ns, unsigned long long end_ns)
{
    if ((start_ns == 0) || (end_ns < start_ns)) {
        return 0;
    }

    return (end_ns - start_ns) / CONVERT_US_TO_NS;
}

/* cost from a kernel timestamp to a user timespec, same monotonic clock domain */
STATIC unsigned long long hdc_calc_k2u_cost(const struct timespec *user_tval, unsigned long long kernel_start_ns)
{
    unsigned long long end_ns;

    if (kernel_start_ns == 0) {
        return 0;
    }

    end_ns = (unsigned long long)user_tval->tv_sec * CONVERT_S_TO_NS + (unsigned long long)user_tval->tv_nsec;
    if (end_ns < kernel_start_ns) {
        return 0;
    }

    return (end_ns - kernel_start_ns) / CONVERT_US_TO_NS;
}

void hdc_get_connect_time_cost(struct hdc_time_record_for_connect *connect_record, struct hdc_ub_session *session,
                               const union hdcdrv_cmd *send_cmd)
{
    hdc_ub_connect_info_t timecost = {0};
    hdc_urma_init_info_t urma_timecost = {0};
    const struct hdcdrv_connect_perf *perf = &send_cmd->send_ctrl_msg.connect;

    if (connect_record->fail_times > 0) {
        HDC_PERF_LOG_RUN_INFO_LIMIT("hdc perf data not get successfully.(dev_id=%d; l_id=%d; type=\'%s\'\n",
                                    session->dev_id, session->local_id, hdc_get_sevice_str(session->service_type));
        return;
    }

    // cost of connect
    timecost.connect_end = hdc_get_time_cost(&connect_record->connect_start, &connect_record->connect_end);
    if (timecost.connect_end <= HDC_ACCEPT_PERF_STANDER) {
        return;
    }

    timecost.link_pre_init = hdc_get_time_cost(&connect_record->connect_start, &connect_record->link_pre_init);
    timecost.mem_res_init = hdc_get_time_cost(&connect_record->link_pre_init, &connect_record->mem_res_init);
    timecost.alloc_session = hdc_get_time_cost(&connect_record->mem_res_init, &connect_record->alloc_session);
    timecost.pre_init = hdc_get_time_cost(&connect_record->alloc_session, &connect_record->pre_init);
    timecost.create_ub_ctx = hdc_get_time_cost(&connect_record->pre_init, &connect_record->create_ub_ctx);
    timecost.get_res_info_and_add_ctrl = hdc_get_time_cost(&connect_record->create_ub_ctx,
                                                           &connect_record->get_res_info_and_add_ctrl);
    timecost.send_ctrl_msg = hdc_get_time_cost(&connect_record->get_res_info_and_add_ctrl,
                                               &connect_record->send_ctrl_msg);
    timecost.check_reply = hdc_get_time_cost(&connect_record->send_ctrl_msg, &connect_record->check_reply);
    timecost.free_tid_and_fill_jetty_info = hdc_get_time_cost(&connect_record->urma_func_cost.urma_record.import_jfr,
                                                              &connect_record->free_tid_and_fill_jetty_info);

    // kernel cost computed from raw timestamps returned by ioctl
    timecost.k_pre = hdc_calc_kernel_cost(perf->k_cnt_start, perf->k_cnt_send_ctrl_start);
    timecost.k_send = hdc_calc_kernel_cost(perf->k_cnt_send_ctrl_start, perf->k_cnt_send_ctrl_end);
    timecost.k_wait = hdc_calc_kernel_cost(perf->k_cnt_send_ctrl_end, perf->k_cnt_wait_fin);
    timecost.k_sched = hdc_calc_kernel_cost(perf->k_cnt_reply_wake, perf->k_cnt_wait_fin);
    timecost.k_copy = hdc_calc_kernel_cost(perf->k_cnt_wait_fin, perf->k_cnt_cpy_end);
    timecost.k_ret = hdc_calc_k2u_cost(&connect_record->send_ctrl_msg, perf->k_cnt_cpy_end);

    // get all urma res timecost
    hdc_get_urma_init_time_cost(&connect_record->urma_func_cost, session, &connect_record->send_ctrl_msg,
                                &urma_timecost, HDC_PERF_LOG_PRINT_LIMIT);
    HDC_PERF_LOG_RUN_INFO_LIMIT("hdc connect time_cost:%llu, exceed %lu us.(dev_id=%d; l_id=%u; unique_val=%u; "
                                "r_id=%u; type=%d; link_pre=%llu; res_init=%llu; alloc_session=%llu; pre_init=%llu; "
                                "ub_ctx=%llu; get_res_ctrl=%llu; send_wait=%llu; check=%llu; free_fill=%llu; "
                                "import_jetty=%llu)\n",
                                timecost.connect_end, (unsigned long)HDC_ACCEPT_PERF_STANDER, session->dev_id,
                                session->local_id, session->unique_val, session->remote_id, session->service_type,
                                timecost.link_pre_init, timecost.mem_res_init, timecost.alloc_session,
                                timecost.pre_init, timecost.create_ub_ctx, timecost.get_res_info_and_add_ctrl,
                                timecost.send_ctrl_msg, timecost.check_reply, timecost.free_tid_and_fill_jetty_info,
                                (urma_timecost.get_tp_list + urma_timecost.import_jfr));
    HDC_PERF_LOG_RUN_INFO_LIMIT("hdc connect kernel_cost.(dev_id=%d; l_id=%u; unique_val=%u; r_id=%u; type=%d; "
                                "k_pre=%llu; k_send=%llu; k_wait=%llu; k_copy=%llu; k_sched=%llu; k_ret=%llu)\n",
                                session->dev_id, session->local_id, session->unique_val, session->remote_id,
                                session->service_type, timecost.k_pre, timecost.k_send, timecost.k_wait,
                                timecost.k_copy, timecost.k_sched, timecost.k_ret);
}

void hdc_get_accept_time_cost(struct hdc_time_record_for_accept *accept_record, struct hdc_ub_session *session,
                              bool succ_flag, const union hdcdrv_cmd *send_cmd)
{
    hdc_ub_accept_info_t accept_time_cost = {0};
    hdc_urma_init_info_t urma_timecost = {0};
    const struct hdcdrv_accept_perf *perf = &send_cmd->send_ctrl_msg.accept;

    if (accept_record->fail_times > 0) {
        HDC_PERF_LOG_RUN_INFO_LIMIT("hdc perf data not get successfully.(dev_id=%d; l_id=%d; type=\'%s\'\n",
                                    session->dev_id, session->local_id, hdc_get_sevice_str(session->service_type));
        return;
    }

    if (succ_flag) {
        // cost of accept
        accept_time_cost.accept = hdc_get_time_cost(&accept_record->conn_wait, &accept_record->accept_end);
    } else {
        // cost of accept
        accept_time_cost.accept = hdc_get_time_cost(&accept_record->conn_wait, &accept_record->send_ctrl_msg);
    }

    if (accept_time_cost.accept <= HDC_ACCEPT_PERF_STANDER) {
        return;
    }

    accept_time_cost.conn_wait = hdc_get_time_cost(&accept_record->accept_start, &accept_record->conn_wait);
    accept_time_cost.res_init = hdc_get_time_cost(&accept_record->conn_wait, &accept_record->res_init);
    accept_time_cost.alloc_session = hdc_get_time_cost(&accept_record->res_init, &accept_record->alloc_session);
    accept_time_cost.pre_init = hdc_get_time_cost(&accept_record->alloc_session, &accept_record->pre_init);
    accept_time_cost.urma_init = hdc_get_time_cost(&accept_record->pre_init, &accept_record->create_ub_ctx);
    accept_time_cost.add_ctrl = hdc_get_time_cost(&accept_record->create_ub_ctx, &accept_record->add_ctrl);
    accept_time_cost.send_ctrl_msg = hdc_get_time_cost(&accept_record->urma_func_cost.urma_record.import_jfr,
                                                       &accept_record->send_ctrl_msg);

    // cost of pipe write start (esched thread) to read pipe return (accept thread)
    accept_time_cost.pipe_trans = hdc_get_time_cost(&accept_record->pipe_write_start, &accept_record->conn_wait);

    // time_cost exceed ACCEPT_PERF_STANDER, print log
    hdc_get_urma_init_time_cost(&accept_record->urma_func_cost, session, &accept_record->add_ctrl, &urma_timecost,
                                HDC_PERF_LOG_PRINT_NOT_LIMIT);
    HDC_PERF_LOG_RUN_INFO(HDC_PERF_LOG_PRINT_NOT_LIMIT,
                          "hdc accept time_cost:%llu, exceed %lu us.(dev_id=%d; l_id=%u; unique_val=%u; r_id=%u; "
                          "type=%d; conn_wait=%llu; pipe_trans=%llu; res_init=%llu; alloc_session=%llu; "
                          "pre_init=%llu; urma_init=%llu; add_ctrl=%llu; import_jetty=%llu; send_ctrl_msg=%llu)\n",
                          accept_time_cost.accept, (unsigned long)HDC_ACCEPT_PERF_STANDER, session->dev_id,
                          session->local_id, session->unique_val, session->remote_id, session->service_type,
                          accept_time_cost.conn_wait, accept_time_cost.pipe_trans, accept_time_cost.res_init,
                          accept_time_cost.alloc_session, accept_time_cost.pre_init, accept_time_cost.urma_init,
                          accept_time_cost.add_ctrl, (urma_timecost.get_tp_list + urma_timecost.import_jfr),
                          accept_time_cost.send_ctrl_msg);
    if (succ_flag) {
        // kernel cost computed from raw timestamps returned by ioctl
        accept_time_cost.notify_search = hdc_calc_kernel_cost(perf->k_acc_notify_entry, perf->k_acc_notify_search_end);
        // cost from kernel notify search end to user event thread pipe write start
        accept_time_cost.k2u_trans = hdc_calc_k2u_cost(&accept_record->pipe_write_start, perf->k_acc_notify_search_end);
        accept_time_cost.k_pre = hdc_calc_kernel_cost(perf->k_acc_start, perf->k_acc_send_ctrl_start);
        accept_time_cost.k_send = hdc_calc_kernel_cost(perf->k_acc_send_ctrl_start, perf->k_acc_send_ctrl_end);
        accept_time_cost.k_ret = hdc_calc_k2u_cost(&accept_record->send_ctrl_msg, perf->k_acc_send_ctrl_end);

        HDC_PERF_LOG_RUN_INFO(HDC_PERF_LOG_PRINT_NOT_LIMIT,
                              "hdc accept kernel_cost.(dev_id=%d; l_id=%u; unique_val=%u; r_id=%u; type=%d; "
                              "notify_search=%llu; k2u_trans=%llu; k_pre=%llu; k_send=%llu; k_ret=%llu)\n",
                              session->dev_id, session->local_id, session->unique_val, session->remote_id,
                              session->service_type, accept_time_cost.notify_search, accept_time_cost.k2u_trans,
                              accept_time_cost.k_pre, accept_time_cost.k_send, accept_time_cost.k_ret);
    }
}

void hdc_get_recv_time_cost(struct hdc_time_record_for_single_recv *recv_record, struct hdc_ub_session *session)
{
    unsigned long long recv_total, recv_record_now;
    hdc_ub_send_recv_info_t recv_time_cost = {0};

    if (!recv_record->recv_peek_flag) {
        return;
    }

    if (recv_record->fail_times > 0) {
        HDC_PERF_LOG_RUN_INFO_LIMIT("hdc perf data not get successfully.(dev_id=%d; l_id=%d; type=\'%s\'\n",
                                    session->dev_id, session->local_id, hdc_get_sevice_str(session->service_type));
    }

    // cost of wait_poll_jfc in recv
    recv_time_cost.timecost6 = hdc_get_time_cost(&recv_record->ub_recv_peek_start, &recv_record->poll_jfc_recv);
    // cost of ack + rearm in recv
    recv_time_cost.timecost7 = hdc_get_time_cost(&recv_record->poll_jfc_recv, &recv_record->ack_and_rearm_jfc);
    // cost of total recv_peek
    recv_time_cost.timecost_recv_peek = hdc_get_time_cost(&recv_record->ub_recv_peek_start,
                                                          &recv_record->ub_recv_peek_end);
    // cost of total recv
    recv_time_cost.timecost_recv = hdc_get_time_cost(&recv_record->ub_recv_start, &recv_record->ub_recv_end);

    // cost of memcpy in recv
    recv_time_cost.timecost8 = hdc_get_time_cost(&recv_record->ub_recv_start, &recv_record->copy_buf_to_user);
    // cost of urma_post_jfr_wr in recv
    recv_time_cost.timecost9 = hdc_get_time_cost(&recv_record->copy_buf_to_user, &recv_record->ub_recv_end);

    // cost of total recv
    recv_time_cost.timecost_recv = hdc_get_time_cost(&recv_record->ub_recv_start, &recv_record->ub_recv_end);

    // if time_cost exceed PERF_STANDER, print log and add count
    recv_total = recv_time_cost.timecost_recv_peek + recv_time_cost.timecost_recv;
    if (recv_total > HDC_PERF_STANDER) {
        session->send_recv_info.timecost_exceed_cnt_recv++;
        HDC_PERF_LOG_RUN_INFO_LIMIT(
            "hdc recv exceed %d us.(dev_id=%d; l_id=%d; unique_val=%u; type=\'%s\'; jfr_id=%u; jfc_r=%u;"
            " wait_poll_jfc:%llu; ack_rearm_jfc=%llu; memcpy=%llu; post_jfr_wr=%llu; recv_peek=%llu;"
            " recv=%llu; exceed_times=%llu\n",
            HDC_PERF_STANDER, session->dev_id, session->local_id, session->unique_val,
            hdc_get_sevice_str(session->service_type), session->jetty_info.session_jfr_id,
            session->jetty_info.session_jfc_recv_id, recv_time_cost.timecost6, recv_time_cost.timecost7,
            recv_time_cost.timecost8, recv_time_cost.timecost9, recv_time_cost.timecost_recv_peek,
            recv_time_cost.timecost_recv, session->send_recv_info.timecost_exceed_cnt_recv);
    }

    // if time_cost exceed current max val, update max val
    recv_record_now = session->send_recv_info.timecost_recv_peek + session->send_recv_info.timecost_recv;
    if (recv_total > recv_record_now) {
        session->send_recv_info.timecost6 = recv_time_cost.timecost6;
        session->send_recv_info.timecost7 = recv_time_cost.timecost7;
        session->send_recv_info.timecost8 = recv_time_cost.timecost8;
        session->send_recv_info.timecost9 = recv_time_cost.timecost9;
        session->send_recv_info.timecost_recv_peek = recv_time_cost.timecost_recv_peek;
        session->send_recv_info.timecost_recv = recv_time_cost.timecost_recv;
    }

    return;
}

void hdc_get_send_time_cost(struct hdc_time_record_for_single_send *send_record, struct hdc_ub_session *session)
{
    hdc_ub_send_recv_info_t send_time_cost = {0};
    unsigned long long timecost_wait_jfc;
    unsigned long long timecost_post_jfs_wr;

    if (send_record->fail_times > 0) {
        HDC_PERF_LOG_RUN_INFO_LIMIT("hdc perf data not get successfully.(dev_id=%d; l_id=%d; type=\'%s\'\n",
                                    session->dev_id, session->local_id, hdc_get_sevice_str(session->service_type));
    }

    // cost of find_idle_block in send
    send_time_cost.timecost1 = hdc_get_time_cost(&send_record->ub_send_start, &send_record->find_idle_block);
    // cost of memcpy in send
    send_time_cost.timecost2 = hdc_get_time_cost(&send_record->find_idle_block, &send_record->fill_jfs_wr);
    // cost of urma_post_jfs_wr in send
    send_time_cost.timecost3 = hdc_get_time_cost(&send_record->post_jfs_wr_start, &send_record->wait_jfc_send_finish);
    // cost of poll_jfc in send
    send_time_cost.timecost4 = hdc_get_time_cost(&send_record->wait_jfc_send_finish, &send_record->poll_jfc_send);
    // cost of ack + rearm in send
    send_time_cost.timecost5 = hdc_get_time_cost(&send_record->poll_jfc_send, &send_record->ub_send_end);
    // send may retry, so use sum of all time instead of send timecost
    send_time_cost.timecost_send = send_time_cost.timecost1 + send_time_cost.timecost2 + send_time_cost.timecost3 +
                                   send_time_cost.timecost4 + send_time_cost.timecost5;
    // cost of total send. If time_cost exceed PERF_STANDER, print log and add count
    if (send_time_cost.timecost_send > HDC_PERF_STANDER) {
        session->send_recv_info.timecost_exceed_cnt_send++;
        timecost_post_jfs_wr = hdc_get_time_cost(&send_record->post_jfs_wr_start, &send_record->wait_jfc_send_start);
        timecost_wait_jfc = hdc_get_time_cost(&send_record->wait_jfc_send_start, &send_record->wait_jfc_send_finish);
        HDC_PERF_LOG_RUN_INFO_LIMIT(
            "hdc send exceed %d us.(dev_id=%d; l_id=%u; unique_val=%u; type=\'%s\'; jfs_id=%u; jfc_s=%u; "
            "find_block:%llu; memcpy=%llu; post_jfs_wr=%llu; wait_jfc=%llu; poll_jfc:%llu; ack_rearm_jfc=%llu; "
            "send=%llu; exceed_times=%llu\n",
            HDC_PERF_STANDER, session->dev_id, session->local_id, session->unique_val,
            hdc_get_sevice_str(session->service_type), session->jetty_info.session_jfs_id,
            session->jetty_info.session_jfc_send_id, send_time_cost.timecost1, send_time_cost.timecost2,
            timecost_post_jfs_wr, timecost_wait_jfc, send_time_cost.timecost4, send_time_cost.timecost5,
            send_time_cost.timecost_send, session->send_recv_info.timecost_exceed_cnt_send);
    }

    // if time_cost exceed current max val, update max val
    if (send_time_cost.timecost_send > session->send_recv_info.timecost_send) {
        session->send_recv_info.timecost1 = send_time_cost.timecost1;
        session->send_recv_info.timecost2 = send_time_cost.timecost2;
        session->send_recv_info.timecost3 = send_time_cost.timecost3;
        session->send_recv_info.timecost4 = send_time_cost.timecost4;
        session->send_recv_info.timecost5 = send_time_cost.timecost5;
        session->send_recv_info.timecost_send = send_time_cost.timecost_send;
    }

    return;
}

STATIC void hdc_get_urma_uninit_time_cost(struct hdc_time_record_for_close *close_record,
                                          hdc_ub_close_info_t *time_cost)
{
    struct hdc_time_record_for_urma_uninit *uninit_record = &close_record->urma_uninit;
    time_cost->del_jfr = hdc_get_time_cost(&close_record->unimport_jetty, &uninit_record->del_jfr);
    time_cost->wait_data_fin = hdc_get_time_cost(&uninit_record->del_jfr, &uninit_record->wait_data_fin);
    time_cost->del_jfcr = hdc_get_time_cost(&uninit_record->wait_data_fin, &uninit_record->del_jfcr);
    time_cost->del_jfs_1 = hdc_get_time_cost(&uninit_record->del_jfcr, &uninit_record->del_jfs_1);
    time_cost->del_jfcs_1 = hdc_get_time_cost(&uninit_record->del_jfs_1, &uninit_record->del_jfcs_1);
    time_cost->own_unreg = hdc_get_time_cost(&uninit_record->del_jfcs_1, &uninit_record->own_unreg);
    if (uninit_record->need_uninit) {
        time_cost->share_unreg = hdc_get_time_cost(&uninit_record->own_unreg, &uninit_record->share_unreg);
        time_cost->free_token_id = hdc_get_time_cost(&uninit_record->share_unreg, &uninit_record->free_token_id);
        time_cost->del_ctx = hdc_get_time_cost(&uninit_record->free_token_id, &uninit_record->del_ctx);
    }
}

STATIC void hdc_print_close_time_cost(struct hdc_time_record_for_close *close_record, hdc_ub_close_info_t *time_cost,
                                      unsigned long long stander)
{
    if (close_record->close_type == HDCDRV_CLOSE_TYPE_USER) {
        HDC_PERF_LOG_RUN_INFO_LIMIT(
            "hdc urma_uninit_cost info (us). dev_id=%d; l_id=%u; type=\'%s\'; "
            "unimport_jetty=%llu; del_jfr:%llu; del_jfcr=%llu; del_jfs=%llu; del_jfcs=%llu; own_unreg=%llu; "
            "(urma_uninit_flag=%d; share_unreg=%llu; free_token_id=%llu; del_ctx=%llu)\n",
            close_record->dev_id, close_record->local_id, hdc_get_sevice_str(close_record->service_type),
            time_cost->unimport_jetty, time_cost->del_jfr, time_cost->del_jfcr, time_cost->del_jfs_1,
            time_cost->del_jfcs_1, time_cost->own_unreg, close_record->urma_uninit.need_uninit, time_cost->share_unreg,
            time_cost->free_token_id, time_cost->del_ctx);
        HDC_PERF_LOG_RUN_INFO_LIMIT(
            "hdc close time_cost:%llu, exceed %llu us.(dev_id=%d; l_id=%u; type=\'%s\'; "
            "del_recv_epoll:%llu; sub_event=%llu; del_data_epoll=%llu; wait_data_fin=%llu; del_urma=%llu; "
            "close_kernel=%llu; mem_uninit=%llu; wake_recv=%llu; close_notify=%llu; close_record_free=%llu; "
            "del_close_epoll=%llu\n",
            time_cost->timecost_close, stander, close_record->dev_id, close_record->local_id,
            hdc_get_sevice_str(close_record->service_type), time_cost->del_recv_epoll, time_cost->sub_close_event,
            time_cost->del_data_epoll, time_cost->wait_data_fin, time_cost->del_urma, time_cost->close_kernel,
            time_cost->mem_uninit, time_cost->wake_recv, time_cost->close_notify, time_cost->session_free,
            time_cost->del_close_epoll);
    } else if (close_record->close_type == HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST) {
        HDC_PERF_LOG_RUN_INFO_LIMIT(
            "hdc urma_uninit_cost info (us). dev_id=%d; l_id=%u; type=\'%s\'; del_jfs=%llu; "
            "del_jfcs=%llu; unimport_jetty=%llu; del_jfr:%llu; del_jfcr=%llu; del_jfs_1=%llu; del_jfcs_1=%llu; "
            "own_unreg=%llu; (urma_uninit_flag=%d; share_unreg=%llu; free_token_id=%llu; del_ctx=%llu)\n",
            close_record->dev_id, close_record->local_id, hdc_get_sevice_str(close_record->service_type),
            time_cost->del_jfs, time_cost->del_jfcs, time_cost->unimport_jetty, time_cost->del_jfr, time_cost->del_jfcr,
            time_cost->del_jfs_1, time_cost->del_jfcs_1, time_cost->own_unreg, close_record->urma_uninit.need_uninit,
            time_cost->share_unreg, time_cost->free_token_id, time_cost->del_ctx);
        HDC_PERF_LOG_RUN_INFO_LIMIT(
            "hdc close time_cost:%llu, exceed %llu us.(dev_id=%d; l_id=%u; type=\'%s\'; "
            "del_recv_epoll:%llu; sub_event=%llu; del_data_epoll=%llu; wait_data_fin=%llu; del_urma=%llu; "
            "close_kernel=%llu; mem_uninit=%llu; wake_recv=%llu; close_notify=%llu; write_file=%llu;\n",
            time_cost->timecost_close, stander, close_record->dev_id, close_record->local_id,
            hdc_get_sevice_str(close_record->service_type), time_cost->del_recv_epoll, time_cost->sub_close_event,
            time_cost->del_data_epoll, time_cost->wait_data_fin, time_cost->del_urma, time_cost->close_kernel,
            time_cost->mem_uninit, time_cost->wake_recv, time_cost->close_notify, time_cost->write_file);
    }
}

void hdc_get_close_time_cost(struct hdc_time_record_for_close *close_record)
{
    hdc_ub_close_info_t time_cost = {0};
    unsigned long long stander = hdc_get_perf_stander(HDC_CLOSE_PERF_STANDER_ENV, HDC_CLOSE_PERF_STANDER);
    struct hdc_time_record_for_remote_close *proc_record = close_record->remote_close;
    struct hdc_time_record_for_urma_uninit *urma_uninit_record = &close_record->urma_uninit;
    int fail_times;
    if (close_record->close_type == HDCDRV_CLOSE_TYPE_USER) {
        fail_times = close_record->fail_times + urma_uninit_record->fail_times;
    } else if (close_record->close_type == HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST) {
        fail_times = close_record->fail_times + proc_record->fail_times + urma_uninit_record->fail_times;
    } else {
        return; // session has closed
    }

    if (fail_times > 0) {
        HDC_PERF_LOG_RUN_INFO_LIMIT("hdc perf data not get successfully.(dev_id=%d; l_id=%d; type=\'%s\'\n",
                                    close_record->dev_id, close_record->local_id,
                                    hdc_get_sevice_str(close_record->service_type));
        return;
    }

    if (close_record->close_type == HDCDRV_CLOSE_TYPE_USER) {
        time_cost.timecost_close = hdc_get_time_cost(&close_record->start_close_handle, &close_record->del_close_epoll);
    } else if (close_record->close_type == HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST) {
        time_cost.timecost_close = hdc_get_time_cost(&proc_record->close_proc, &proc_record->del_jfcs) +
                                   hdc_get_time_cost(&close_record->start_close_handle, &close_record->write_file);
    }

    if (time_cost.timecost_close <= stander) {
        return;
    }
    if (close_record->close_type == HDCDRV_CLOSE_TYPE_USER) {
        time_cost.session_free = hdc_get_time_cost(&close_record->close_notify, &close_record->session_free);
        time_cost.del_close_epoll = hdc_get_time_cost(&close_record->session_free, &close_record->del_close_epoll);
    } else if (close_record->close_type == HDCDRV_CLOSE_TYPE_REMOTE_CLOSED_POST) {
        time_cost.del_jfs = hdc_get_time_cost(&proc_record->close_proc, &proc_record->del_jfs);
        time_cost.del_jfcs = hdc_get_time_cost(&proc_record->del_jfs, &proc_record->del_jfcs);
        time_cost.write_file = hdc_get_time_cost(&close_record->close_notify, &close_record->write_file);
    }
    time_cost.del_recv_epoll = hdc_get_time_cost(&close_record->start_close_handle, &close_record->del_recv_epoll);
    time_cost.sub_close_event = hdc_get_time_cost(&close_record->del_recv_epoll, &close_record->sub_close_event);
    time_cost.del_data_epoll = hdc_get_time_cost(&close_record->sub_close_event, &close_record->del_data_epoll);
    time_cost.unimport_jetty = hdc_get_time_cost(&close_record->del_data_epoll, &close_record->unimport_jetty);
    time_cost.del_urma = hdc_get_time_cost(&close_record->del_data_epoll, &close_record->del_ub_ctx);
    time_cost.close_kernel = hdc_get_time_cost(&close_record->del_ub_ctx, &close_record->close_kernel);
    time_cost.mem_uninit = hdc_get_time_cost(&close_record->close_kernel, &close_record->mem_uninit);
    time_cost.wake_recv = hdc_get_time_cost(&close_record->mem_uninit, &close_record->wake_recv);
    time_cost.close_notify = hdc_get_time_cost(&close_record->wake_recv, &close_record->close_notify);
    hdc_get_urma_uninit_time_cost(close_record, &time_cost);
    hdc_print_close_time_cost(close_record, &time_cost, stander);
}

STATIC void hdc_update_session_recv_peek_record(struct hdc_ub_rx_buf *buf,
                                                struct hdc_time_record_for_single_recv *recv_record)
{
    if (recv_record->fail_times > 0) {
        buf->recv_record.recv_peek_flag = false;
        return;
    }

    buf->recv_record.ub_recv_peek_start = recv_record->ub_recv_peek_start;
    buf->recv_record.wait_jfc_recv = recv_record->wait_jfc_recv;
    buf->recv_record.poll_jfc_recv = recv_record->poll_jfc_recv;
    buf->recv_record.ack_and_rearm_jfc = recv_record->ack_and_rearm_jfc;
    buf->recv_record.ub_recv_peek_end = recv_record->ub_recv_peek_end;

    return;
}

void hdc_update_session_recv_record(struct hdc_ub_rx_buf *buf, struct hdc_time_record_for_single_recv *recv_record)
{
    if (recv_record->fail_times > 0) {
        buf->recv_record.recv_peek_flag = false;
        return;
    }

    buf->recv_record.ub_recv_start = recv_record->ub_recv_start;
    buf->recv_record.copy_buf_to_user = recv_record->copy_buf_to_user;
    buf->recv_record.ub_recv_end = recv_record->ub_recv_end;

    return;
}

void hdc_get_ub_res_info(hdc_ub_context_t *ctx, hdc_ub_res_info_t *ub_res_info)
{
    ub_res_info->l_jfr_id = ctx->jfr.jfr->jfr_id.id;
    ub_res_info->l_jfs_id = ctx->jfs.jfs->jfs_id.id;
    ub_res_info->l_jfc_r_id = ctx->jfc_recv.jfc->jfc_id.id;
    ub_res_info->l_jfc_s_id = ctx->jfc_send.jfc->jfc_id.id;
    ub_res_info->l_jfce_r_fd = ctx->jfc_recv.jfce->fd;
}

STATIC int hdc_update_rx_list(struct hdc_ub_epoll_node *node, struct hdc_ub_session *session, urma_cr_t *cr,
                              urma_target_seg_t *tseg, struct hdc_time_record_for_single_recv *recv_record)
{
    if ((cr->completion_len > HDC_MEM_BLOCK_SIZE) || (cr->completion_len <= 0) ||
        (cr->user_ctx >= (HDC_URMA_MAX_JFR_DEPTH + HDC_URMA_MAX_JFS_DEPTH))) {
        HDC_LOG_ERR("Recv buf len overflow. (dev_id=%d; recv len=%u; l_id=%u; r_id=%u; user_ctx=%lu)\n",
                    session->dev_id, cr->completion_len, session->local_id, session->remote_id, cr->user_ctx);
        session->dbg_stat.rx_fail_hdc++;
        return DRV_ERROR_TRANS_LINK_ABNORMAL;
    }

    node->rx_list[node->tail].tseg = tseg;
    node->rx_list[node->tail].addr = session->user_va + cr->user_ctx * HDC_MEM_BLOCK_SIZE;
    node->rx_list[node->tail].len = cr->completion_len;
    node->rx_list[node->tail].idx = cr->user_ctx;

    hdc_get_time_record(&recv_record->ub_recv_peek_end, &recv_record->fail_times);
    hdc_update_session_recv_peek_record(&node->rx_list[node->tail], recv_record);
    node->tail = (node->tail + 1) % HDC_RX_LIST_LEN;

    return DRV_ERROR_NONE;
}

STATIC bool hdc_check_wait_recv_jfc_ret(hdc_ub_context_t *ctx, struct hdc_ub_session *session)
{
    int cnt;
    urma_jfc_t *ev_jfc;
    bool flag = true;

    cnt = ascend_urma_wait_jfc(ctx->jfc_recv.jfce, 1, 1, &ev_jfc);
    if ((cnt != 1) || (ctx->jfc_recv.jfc != ev_jfc)) {
        /* if cnt is 0, means no cqe come; if cnt is -1, means error. For some services that frequently time out
            (like log, bbox), no need to print. Otherwise, the log will be refreshed. */
        HDC_LOG_ERR("Wait jfc has problem. (cnt=%d; jfc_id=%u; l_id=%u; r_id=%u; dev_id=%d;"
                    "service_type=\"%s\")\n",
                    cnt, hdc_get_jfc_id_by_type(&session->jetty_info, HDC_UB_RX), session->local_id, session->remote_id,
                    session->dev_id, hdc_get_sevice_str(session->service_type));
        hdc_ub_get_urma_dfx(session, NULL, &ctx->jfr, &ctx->jfc_recv);
        hdc_jfc_dbg_fill(HDC_UB_RX, session, HDC_URMA_WAIT_FAIL);
        flag = false;
    }

    // If wait_jfc receives cqe, it must be paired with an ack; otherwise, the delete_jfc interface will be blocked
    if (cnt > 1) {
        urma_ack_jfc((urma_jfc_t **)&ev_jfc, (uint32_t *)&cnt, 1);
        (void)urma_rearm_jfc(ctx->jfc_recv.jfc, false);
    }

    return flag;
}

int hdc_recv_data_in_event_handle(struct hdc_ub_epoll_node *node)
{
    int service_type, dev_id;
    struct hdc_time_record_for_single_recv recv_record = {0};
    struct hdc_ub_session *session = (struct hdc_ub_session *)node->session;
    hdc_ub_context_t *ctx = NULL;
    urma_target_seg_t *tseg = NULL;
    urma_cr_t cr = {0};
    uint64_t inc = 1;
    mmSsize_t mm_ret;
    int ret = DRV_ERROR_NONE;

    pthread_mutex_lock(&node->mutex);
    // Add a session status check
    if ((session == NULL) || (session->status == HDC_SESSION_STATUS_IDLE)) {
        ret = HDCDRV_ERR;
        goto recv_abnormal_exit;
    }

    session->dbg_stat.rx_epoll++;

    if (hdc_rx_list_is_full(node)) {
        session->dbg_stat.rx_full++;
        goto recv_abnormal_exit;
    }

    hdc_get_time_record(&recv_record.ub_recv_peek_start, &recv_record.fail_times);
    node->pkt_num++;
    ctx = node->ctx;
    if (ctx == NULL) {
        goto recv_abnormal_exit;
    }
    tseg = ctx->tseg;
    dev_id = session->dev_id;
    service_type = session->service_type;

    if (!hdc_check_wait_recv_jfc_ret(ctx, session)) {
        goto recv_abnormal_exit;
    }
    wmb();
    hdc_get_time_record(&recv_record.wait_jfc_recv, &recv_record.fail_times);

    if (hdc_poll_jfc(&ctx->jfc_recv, &cr, session, HDC_UB_RX, session->service_type) != 0) {
        hdc_ack_jfc(&ctx->jfc_recv);
        (void)hdc_rearm_jfc(&ctx->jfc_recv, session, HDC_UB_RX);
        goto recv_abnormal_exit;
    }
    wmb();
    hdc_get_time_record(&recv_record.poll_jfc_recv, &recv_record.fail_times);

    hdc_ack_jfc(&ctx->jfc_recv);
    if (hdc_rearm_jfc(&ctx->jfc_recv, session, HDC_UB_RX) != 0) {
        goto recv_abnormal_exit;
    }
    wmb();
    hdc_get_time_record(&recv_record.ack_and_rearm_jfc, &recv_record.fail_times);

    if (hdc_update_rx_list(node, session, &cr, tseg, &recv_record) != DRV_ERROR_NONE) {
        goto recv_abnormal_exit;
    }
    session->dbg_stat.rx_add_list++;
    wmb();

    pthread_cond_signal(&node->cond);
    if (session->recv_eventfd != -1) {
        mm_ret = mm_write_file(session->recv_eventfd, &inc, sizeof(inc));
        if (mm_ret != (mmSsize_t)sizeof(inc)) {
            HDC_LOG_WARN("send close event not success.(ret=%ld; id=%d; dev=%d)\n", mm_ret, session->local_id,
                         session->dev_id);
        }
    }
    pthread_mutex_unlock(&node->mutex);
    // data_in notify may directly use halHdcRecv, so need to use outside mutex
    hdc_touch_data_in_notify(dev_id, service_type);
    return HDCDRV_OK;

recv_abnormal_exit:
    pthread_mutex_unlock(&node->mutex);
    return ret;
}

uint8_t hdc_ub_get_jfs_priority_by_type(int service_type)
{
    uint8_t priority_level;

    switch (service_type) {
        case HDC_SERVICE_TYPE_PROFILING:
        case HDC_SERVICE_TYPE_IDE1:
        case HDC_SERVICE_TYPE_IDE2:
        case HDC_SERVICE_TYPE_LOG:
        case HDC_SERVICE_TYPE_BBOX:
        case HDC_SERVICE_TYPE_IDE_FILE_TRANS:
        case HDC_SERVICE_TYPE_DUMP:
            priority_level = ASCEND_URMA_PRIORITY_LOW;
            break;
        default:
            priority_level = ASCEND_URMA_PRIORITY_HIGH;
            break;
    }

    return priority_level;
}

STATIC void hdc_fill_msg_connect(struct hdcdrv_event_connect *msg, struct hdc_ub_session *session,
                                 struct hdcdrv_cmd_connect *cmd)
{
    struct hdc_jfr_id_info *jfr_info = (struct hdc_jfr_id_info *)msg->jetty_info;

    msg->client_session = session->local_id;
    msg->service_type = cmd->service_type;
    msg->client_pid = session->create_pid;
    msg->peer_create_pid = session->peer_create_pid;

    // jetty msg
    hdc_pack_jetty_info(session->ctx, jfr_info);
}

STATIC void hdc_fill_msg_connect_reply(struct hdcdrv_event_connect_reply *msg, struct hdc_ub_session *session,
                                       struct hdcdrv_cmd_accept *cmd)
{
    struct hdc_jfr_id_info *jfr_info = (struct hdc_jfr_id_info *)msg->jetty_info;

    (void)cmd;

    msg->server_session = session->local_id;
    msg->client_session = session->remote_id;
    msg->peer_pid = session->create_pid;
    msg->server_pid = session->create_pid;
    msg->server_error_code = 0;

    // jetty reply
    hdc_pack_jetty_info(session->ctx, jfr_info);
}

STATIC void hdc_fill_msg_close(struct hdcdrv_event_msg *msg, struct hdc_ub_session *session,
                               const struct hdcdrv_cmd_close *cmd)
{
    // only local_id, type and close_state fill in user, other info will be filled in kernel
    msg->close_msg.local_session = session->local_id;
    msg->close_msg.session_close_state = cmd->local_close_state;
    msg->close_msg.remote_session = session->remote_id;
}

STATIC void hdc_fill_msg_dfx(struct hdcdrv_event_msg *msg, struct hdc_ub_session *session)
{
    msg->dfx_msg.l_session_id = session->local_id;
    msg->dfx_msg.r_session_id = session->remote_id;
    msg->dfx_msg.para_info.unique_val = session->unique_val;
}

STATIC void hdc_fill_msg_dfx_reply(struct hdcdrv_event_msg *msg, struct hdc_ub_session *session)
{
    msg->dfx_msg_reply.l_session_id = session->remote_id;
    msg->dfx_msg_reply.r_session_id = session->local_id;
    hdc_ub_fill_dfx_info(&msg->dfx_msg_reply.dfx_info, session);
    hdc_ub_fill_jetty_info(&msg->dfx_msg_reply.remote_jetty_info, session);
    hdc_ub_fill_send_recv_info(&msg->dfx_msg_reply.send_recv_info, session);
}

void hdc_fill_event_msg(struct hdcdrv_event_msg *msg, union hdcdrv_cmd *hdc_cmd, struct hdc_ub_session *session,
                        enum hdcdrv_notify_type notify_type)
{
    switch (notify_type) {
        case HDCDRV_NOTIFY_MSG_CONNECT:
            hdc_fill_msg_connect(&msg->connect_msg, session, &hdc_cmd->connect);
            break;
        case HDCDRV_NOTIFY_MSG_CONNECT_REPLY:
            hdc_fill_msg_connect_reply(&msg->connect_msg_reply, session, &hdc_cmd->accept);
            break;
        case HDCDRV_NOTIFY_MSG_CLOSE:
            hdc_fill_msg_close(msg, session, &hdc_cmd->close);
            break;
        case HDCDRV_NOTIFY_MSG_DFX:
            hdc_fill_msg_dfx(msg, session);
            break;
        case HDCDRV_NOTIFY_MSG_DFX_REPLY:
            hdc_fill_msg_dfx_reply(msg, session);
            break;
        default:
            break;
    }

    msg->type = notify_type;
    msg->error_code = 0;
}

#define UB_CTX_DFX_JFC_LEN 128
#define UB_CTX_DFX_JFR_LEN 64
#define UB_CTX_DFX_JFS_LEN 256

char g_hdc_jfc_ctx_buf[UB_CTX_DFX_JFC_LEN];
char g_hdc_jfr_ctx_buf[UB_CTX_DFX_JFR_LEN];
char g_hdc_jfs_ctx_buf[UB_CTX_DFX_JFS_LEN];

STATIC void hdc_check_ret_and_print_urma_dfx(int ret, char *buf, size_t len, unsigned int opcode,
                                             struct hdc_ub_session *session)
{
    if (ret != 0) {
        HDC_LOG_ERR("urma_get_jetty_opt failed.(l_id=%u; dev_id=%d; ret=%d; opcode=%u)\n", session->local_id,
                    session->dev_id, ret, opcode);
        return;
    }

    HDC_ERR_HEX_DUMP(buf, len, "l_id=%u; dev_id=%d; opcode=%u; context:", session->local_id, session->dev_id, opcode);
    (void)memset_s(buf, len, 0, len);
}

void hdc_ub_get_urma_dfx(struct hdc_ub_session *session, hdc_urma_jfs_t *hdc_jfs, hdc_urma_jfr_t *hdc_jfr,
                         hdc_urma_jfc_t *hdc_jfc)
{
    int ret;

    if (hdc_jfs != NULL) {
        ret = urma_get_jfs_opt(hdc_jfs->jfs, URMA_JFS_FULL_CTX, g_hdc_jfs_ctx_buf, UB_CTX_DFX_JFS_LEN);
        hdc_check_ret_and_print_urma_dfx(ret, g_hdc_jfs_ctx_buf, UB_CTX_DFX_JFS_LEN, URMA_JFS_FULL_CTX, session);
    }

    if (hdc_jfr != NULL) {
        ret = urma_get_jfr_opt(hdc_jfr->jfr, URMA_JFR_FULL_CTX, g_hdc_jfr_ctx_buf, UB_CTX_DFX_JFR_LEN);
        hdc_check_ret_and_print_urma_dfx(ret, g_hdc_jfr_ctx_buf, UB_CTX_DFX_JFR_LEN, URMA_JFR_FULL_CTX, session);
    }

    if (hdc_jfc != NULL) {
        ret = urma_get_jfc_opt(hdc_jfc->jfc, URMA_JFC_FULL_CTX, g_hdc_jfc_ctx_buf, UB_CTX_DFX_JFC_LEN);
        hdc_check_ret_and_print_urma_dfx(ret, g_hdc_jfc_ctx_buf, UB_CTX_DFX_JFC_LEN, URMA_JFC_FULL_CTX, session);
    }
}

void hdc_ub_get_urma_dfx_by_flag(struct hdc_ub_session *session, hdc_ub_context_t *ctx, int hdc_rx_tx_flag)
{
    if (hdc_rx_tx_flag == HDC_UB_TX) {
        hdc_ub_get_urma_dfx(session, &ctx->jfs, NULL, &ctx->jfc_send);
    } else {
        hdc_ub_get_urma_dfx(session, NULL, &ctx->jfr, &ctx->jfc_recv);
    }
}