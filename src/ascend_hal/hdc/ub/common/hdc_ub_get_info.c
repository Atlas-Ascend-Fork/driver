/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <unistd.h>

#include "ascend_hal.h"
#include "hdc_cmn.h"
#include "hdc_epoll.h"
#include "hdc_ub_drv.h"

#define HDC_LOCAL_SIDE_PRINT 0
#define HDC_REMOTE_SIDE_PRINT 1

void hdc_ub_print_dfx_info(hdc_ub_dbg_stat_t *dbg_stat, hdcdrv_jetty_info_t *jetty_info,
                           hdc_ub_send_recv_info_t *sr_info, int flag, struct hdc_ub_session *session)
{
    if (flag == HDC_LOCAL_SIDE_PRINT) {
        HDC_RUN_LOG_INFO(
            "local side dfx info (l_id=%u; r_id=%u; type=\"%s\") jfc_r=%u; jfc_s=%u; "
            "jfr=%u; jfs=%u; tx=%llu; tx_byte=%llu; tx_full=%llu; tx_fail_hdc=%llu; tx_fail_ub=%llu; rx_epoll=%llu; "
            "rx_full=%llu; rx_add_list=%llu; rx=%llu; rx_byte=%llu; remote_rx_full=%llu; remote_rx_fail=%llu; epfd=%d; "
            "ep_create=%u; ep_ab=%llu\n",
            session->local_id, session->remote_id, hdc_get_sevice_str(session->service_type),
            jetty_info->session_jfc_recv_id, jetty_info->session_jfc_send_id, jetty_info->session_jfr_id,
            jetty_info->session_jfs_id, dbg_stat->tx, dbg_stat->tx_bytes, dbg_stat->tx_full, dbg_stat->tx_fail_hdc,
            dbg_stat->tx_fail_ub, dbg_stat->rx_epoll, dbg_stat->rx_full, dbg_stat->rx_add_list, dbg_stat->rx,
            dbg_stat->rx_bytes, dbg_stat->remote_rx_full, dbg_stat->remote_rx_fail, hdc_ub_get_epoll_thread_epfd(),
            (unsigned int)hdc_ub_is_epoll_thread_created(), hdc_ub_get_epoll_recv_abnormal_cnt());
        HDC_RUN_LOG_INFO(
            "local side recv_send_cost info (l_id=%u; r_id=%u; type=\"%s\") "
            "s_find_block:%llu; s_memcpy=%llu; s_post_jfs_wr=%llu; s_poll_jfc:%llu; s_ack_rearm_jfc=%llu; s_time=%llu; "
            "r_poll_jfc:%llu; r_ack_rearm_jfc=%llu; r_memcpy=%llu; r_post_jfr_wr=%llu; r_peek_time=%llu; r_time=%llu "
            "send_exceed_times=%llu; recv_exceed_times=%llu\n",
            session->local_id, session->remote_id, hdc_get_sevice_str(session->service_type), sr_info->timecost1,
            sr_info->timecost2, sr_info->timecost3, sr_info->timecost4, sr_info->timecost5, sr_info->timecost_send,
            sr_info->timecost6, sr_info->timecost7, sr_info->timecost8, sr_info->timecost9, sr_info->timecost_recv_peek,
            sr_info->timecost_recv, sr_info->timecost_exceed_cnt_send, sr_info->timecost_exceed_cnt_recv);
    } else {
        HDC_RUN_LOG_INFO(
            "remote side dfx info (l_id=%u; r_id=%u; type=\"%s\") jfc_r=%u; jfc_s=%u; "
            "jfr=%u; jfs=%u; tx=%llu; tx_byte=%llu; tx_full=%llu; tx_fail_hdc=%llu; tx_fail_ub=%llu; rx_epoll=%llu; "
            "rx_full=%llu; rx_add_list=%llu; rx=%llu; rx_byte=%llu; remote_rx_full=%llu; remote_rx_fail=%llu; epfd=%d; "
            "ep_create=%u; ep_ab=%llu\n",
            session->remote_id, session->local_id, hdc_get_sevice_str(session->service_type),
            jetty_info->session_jfc_recv_id, jetty_info->session_jfc_send_id, jetty_info->session_jfr_id,
            jetty_info->session_jfs_id, dbg_stat->tx, dbg_stat->tx_bytes, dbg_stat->tx_full, dbg_stat->tx_fail_hdc,
            dbg_stat->tx_fail_ub, dbg_stat->rx_epoll, dbg_stat->rx_full, dbg_stat->rx_add_list, dbg_stat->rx,
            dbg_stat->rx_bytes, dbg_stat->remote_rx_full, dbg_stat->remote_rx_fail, dbg_stat->epoll_thread_epfd,
            dbg_stat->epoll_thread_create, dbg_stat->recv_event_abnormal);
        // RPINT INFO
        HDC_RUN_LOG_INFO("remote side dfx info (l_id=%u; r_id=%u; type=\"%s\") "
                         "timecost (us) (s_find_block=%llu; s_memcpy=%llu; s_post_jfs_wr=%llu; s_poll_jfc=%llu; "
                         "s_ack_rearm_jfc=%llu; s_time=%llu; "
                         "r_poll_jfc=%llu; r_ack_rearm_jfc=%llu; r_memcpy=%llu; r_post_jfr_wr=%llu; "
                         "r_peek_time=%llu; r_time=%llu) "
                         "send_exceed_times=%llu; recv_exceed_times=%llu\n",
                         session->remote_id, session->local_id, hdc_get_sevice_str(session->service_type),
                         sr_info->timecost1, sr_info->timecost2, sr_info->timecost3, sr_info->timecost4,
                         sr_info->timecost5, sr_info->timecost_send, sr_info->timecost6, sr_info->timecost7,
                         sr_info->timecost8, sr_info->timecost9, sr_info->timecost_recv_peek, sr_info->timecost_recv,
                         sr_info->timecost_exceed_cnt_send, sr_info->timecost_exceed_cnt_recv);
    }

    return;
}

int hdc_ub_get_session_dfx(unsigned int dev_id, struct hdc_ub_session *session)
{
    struct hdcdrv_event_msg dfx_event = {0};
    union hdcdrv_cmd send_cmd = {0};
    signed int ret;

    hdc_ub_print_dfx_info(&session->dbg_stat, &session->jetty_info, &session->send_recv_info, HDC_LOCAL_SIDE_PRINT,
                          session);

    hdc_fill_event_msg(&dfx_event, NULL, session, HDCDRV_NOTIFY_MSG_DFX);
    hdc_fill_send_ctrl_msg_cmd(&send_cmd, (int)dev_id, &dfx_event, sizeof(struct hdcdrv_event_msg));
    ret = hdc_send_ctrl_msg(session->bind_fd, &send_cmd);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("hdc_send_ctrl_msg failed, only print local side dfx.(ret=%d; dev_id=%u; session_id=%u; "
                    "service_type=\"%s\"; remote_pid=%llu)\n",
                    ret, dev_id, session->local_id, hdc_get_sevice_str(session->service_type),
                    session->peer_create_pid);
        return ret;
    }

    HDC_LOG_INFO("Get remote side dfx info.\n");
    hdc_ub_print_dfx_info(&dfx_event.dfx_msg_reply.dfx_info, &dfx_event.dfx_msg_reply.remote_jetty_info,
                          &dfx_event.dfx_msg_reply.send_recv_info, HDC_REMOTE_SIDE_PRINT, session);

    return ret;
}

// The following functions are used for dfx processing, which has been registered to the public drv thread
void hdc_ub_fill_dfx_info(hdc_ub_dbg_stat_t *dbg_stat, struct hdc_ub_session *session)
{
    dbg_stat->tx = session->dbg_stat.tx;
    dbg_stat->tx_bytes = session->dbg_stat.tx_bytes;
    dbg_stat->rx_epoll = session->dbg_stat.rx_epoll;
    dbg_stat->rx_full = session->dbg_stat.rx_full;
    dbg_stat->rx_add_list = session->dbg_stat.rx_add_list;
    dbg_stat->rx = session->dbg_stat.rx;
    dbg_stat->rx_bytes = session->dbg_stat.rx_bytes;
    dbg_stat->tx_full = session->dbg_stat.tx_full;
    dbg_stat->tx_fail_hdc = session->dbg_stat.tx_fail_hdc;
    dbg_stat->tx_fail_ub = session->dbg_stat.tx_fail_ub;
    dbg_stat->remote_rx_full = session->dbg_stat.remote_rx_full;
    dbg_stat->remote_rx_fail = session->dbg_stat.remote_rx_fail;
    // epoll thread dfx is process global, fill from getters on the replying side
    dbg_stat->epoll_thread_create = (unsigned int)hdc_ub_is_epoll_thread_created();
    dbg_stat->epoll_thread_epfd = hdc_ub_get_epoll_thread_epfd();
    dbg_stat->recv_event_abnormal = hdc_ub_get_epoll_recv_abnormal_cnt();

    return;
}

// The following functions are used for dfx processing, which has been registered to the public drv thread
void hdc_ub_fill_send_recv_info(hdc_ub_send_recv_info_t *send_recv_info, struct hdc_ub_session *session)
{
    send_recv_info->timecost1 = session->send_recv_info.timecost1;
    send_recv_info->timecost2 = session->send_recv_info.timecost2;
    send_recv_info->timecost3 = session->send_recv_info.timecost3;
    send_recv_info->timecost4 = session->send_recv_info.timecost4;
    send_recv_info->timecost5 = session->send_recv_info.timecost5;
    send_recv_info->timecost6 = session->send_recv_info.timecost6;
    send_recv_info->timecost7 = session->send_recv_info.timecost7;
    send_recv_info->timecost8 = session->send_recv_info.timecost8;
    send_recv_info->timecost9 = session->send_recv_info.timecost9;
    send_recv_info->timecost_exceed_cnt_send = session->send_recv_info.timecost_exceed_cnt_send;
    send_recv_info->timecost_exceed_cnt_recv = session->send_recv_info.timecost_exceed_cnt_recv;
    send_recv_info->timecost_send = session->send_recv_info.timecost_send;
    send_recv_info->timecost_recv = session->send_recv_info.timecost_recv;
    send_recv_info->timecost_recv_peek = session->send_recv_info.timecost_recv_peek;

    return;
}

int hdc_dfx_query_handle(int dev_id, struct hdc_ub_session *session, struct hdcdrv_event_dfx *dfx_msg)
{
    struct hdcdrv_event_msg msg = {0};
    union hdcdrv_cmd send_cmd = {0};
    int ret;

    (void)dfx_msg;

    hdc_fill_event_msg(&msg, NULL, session, HDCDRV_NOTIFY_MSG_DFX_REPLY);

    hdc_fill_send_ctrl_msg_cmd(&send_cmd, dev_id, (void *)&msg, sizeof(struct hdcdrv_event_msg));
    ret = hdc_send_ctrl_msg(session->bind_fd, &send_cmd);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("hdc_send_ctrl_msg failed.(ret=%d)\n", ret);
    }

    return ret;
}

signed int hdc_ub_get_peer_devId(mmProcess handle, signed int dev_id, signed int *peer_dev_id)
{
    signed int ret;
    union hdcdrv_cmd hdc_cmd;

    hdc_cmd.get_peer_dev_id.dev_id = dev_id;

    ret = hdc_ub_ioctl(handle, HDCDRV_GET_PEER_DEV_ID, &hdc_cmd);

    *peer_dev_id = hdc_cmd.get_peer_dev_id.peer_dev_id;

    return ret;
}