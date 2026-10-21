/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DM_MSG_INTF_H
#define DM_MSG_INTF_H

#include "dm_common.h"

DM_INTF_S *dm_get_intf(DM_CB_S *cb, const char *name, int name_len);
int dm_init(DM_CB_S **dm_cb);
void dm_destroy(DM_CB_S *dm_cb);
int dm_intf_register(DM_CB_S *cb, DM_INTF_S *intf);
int dm_intf_deregister(DM_INTF_S *intf);
int dm_send_req(DM_INTF_S *intf, DM_ADDR_ST *addr, unsigned int addr_len, const DM_MSG_ST *msg,
                DM_MSG_CMD_HNDL_T rsp_hndl, const void *user_data, int data_len);
int dm_send_rsp(DM_INTF_S *intf, DM_ADDR_ST *addr, unsigned int addr_len, const DM_MSG_ST *msg, signed long seq_no);

int dm_cmd_register(DM_CB_S *dm_cb, DM_MSG_CMD_HNDL_T hndl, DM_MSG_UNSUP_HNDL_T unsup_hndl, const void *user_data,
                    int data_len);
int dm_cmd_deregister(DM_CB_S *dm_cb);

#ifdef STATIC_SKIP
void __dm_timeout_handle(int interval, int elapsed, void *user_data, int data_len);
#endif

#ifdef STATIC_SKIP
int __dm_pending_req_cmp(const void *item1, const void *item2);
#endif

#ifdef STATIC_SKIP
void __dm_pending_req_free(void *item);
#endif

#ifdef STATIC_SKIP
int __dm_pending_req_add(DM_INTF_S *intf, const DM_ADDR_ST *addr, unsigned int addr_len, const DM_MSG_ST *msg,
                         DM_MSG_CMD_HNDL_T rsp_hndl, const void *user_data, int data_len, PENDING_REQ_T **pending_req);
#endif

#ifdef STATIC_SKIP
int __dm_cmd_handler_cmp(const void *item1, const void *item2);
#endif

#ifdef STATIC_SKIP
void __dm_cmd_handler_free(void *item);
#endif

#ifdef STATIC_SKIP
void __dm_pending_req_del(DM_INTF_S *intf, signed long long msgid);
#endif

#ifdef STATIC_SKIP
void __dm_rsp_handle(DM_INTF_S *intf, DM_RECV_ST *precv);
#endif

#ifdef STATIC_SKIP
void __dm_cmd_handle(DM_INTF_S *intf, DM_RECV_ST *precv);
#endif

#ifdef STATIC_SKIP
void __dm_msg_handle(DM_INTF_S *intf, DM_RECV_ST *precv);
#endif

#ifdef STATIC_SKIP
void __dm_recv(int fd, short revents, void *user_data, int data_len);
#endif

#ifdef STATIC_SKIP
int __dm_intf_cmp(const void *item1, const void *item2);
#endif

#ifdef STATIC_SKIP
void __dm_intf_free(void *item);
#endif

#endif
