/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <poll.h>
#include <errno.h>
#include "securec.h"

#include "dev_mon_log.h"
#include "dm_loop.h"
#include "mmpa_api.h"
#include "dsmi_common_interface.h"
#include "dsmi_common.h"
#include "dm_msg_intf.h"

#define DM_RECV_HANDLE_TIMEOUT 5000 /* 5s */
#define DM_RECV_HANDLE_TIMEOUT_CNT 5
#define DM_RECV_HANDLE_TIMEOUT_MAX 8

#define DATA_LEN_CHECK() \
    if (data_len <= 0) { \
        return -ENOMEM;  \
    }

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

STATIC void __dm_timeout_handle(int interval, int elapsed, void *user_data, int data_len)
{
    PENDING_REQ_T *req = *((PENDING_REQ_T **)user_data);
    DM_INTF_S *intf = NULL;
    DM_CB_S *cb = NULL;
    int ret;
    intptr_t tmpptr;
    (void)elapsed;
    (void)data_len;
    tmpptr = (intptr_t)req;

    if ((req == NULL) || (req->intf == NULL) || (req->intf->dm_cb == NULL)) {
        DEV_MON_ERR("__dm_timeout_handle input user_data null!\n");
        return;
    }

    intf = req->intf;
    cb = (DM_CB_S *)intf->dm_cb;

    /* judge if support retry */
    if (req->retries > 0) {
        /* resend the msg and change value of retries */
        req->retries--;
        ret = intf->send_msg(intf, REQUEST_MSG, &req->addr, req->addr_len, &req->msg, (signed long)tmpptr);
        if (ret == 0) {
            DEV_MON_INFO("process timeout, retry call intf->send_msg ret = %d\n", ret);
            /* read the timer
               if retries >= 0, auto-response 0xC3 when timeout
               if retries < 0, do nothing    */
            ret = poller_timer_add(cb->intf_poller, interval, __dm_timeout_handle, &req, sizeof(PENDING_REQ_T *),
                                   &req->timer_id);
        }

        /* if resend msg fail, just stop */
        if (ret != 0) {
            DEV_MON_INFO("finish call intf->send_msg");
            req->retries = 0;
        }
    }

    /* if no retry, send the requester the timeout msg */
    if (req->retries == 0) {
        /* through timeout interface to return C3(timeout) */
        DM_INTF_S *selfloop = NULL;
        DM_MSG_ST msg;
        /* fill in timeout code with through timeout_hndl */
        msg = req->msg;
        intf->timeout_hndl(&req->msg, &msg);
        DEV_MON_INFO("finish call intf->timeout_hndl\n");

        /* judge the interface of user is selfloop interface */
        if (strncmp(intf->name, SELFLOOP_INTF, sizeof(intf->name)) != 0) {
            selfloop = dm_get_intf(cb, SELFLOOP_INTF, sizeof(SELFLOOP_INTF));
        } else {
            selfloop = intf;
        }

        if (selfloop != NULL) {
            (void)dm_send_rsp(selfloop, (DM_ADDR_ST *)(&req->addr), req->addr_len, &msg, (signed long)tmpptr);
            DEV_MON_INFO("finish call dm_send_rsp\n");
        }
    }

    return;
}

STATIC int __dm_pending_req_cmp(const void *item1, const void *item2)
{
    if ((item1 == NULL) || (item2 == NULL)) {
        return -1;
    }

    if (item1 == item2) {
        return 0;
    } else if (item1 > item2) {
        return 1;
    } else {
        return -1;
    }
}

STATIC void __dm_pending_req_free(void *item)
{
    PENDING_REQ_T *req = (PENDING_REQ_T *)item;

    if (req != NULL) {
        if (req->user_data != NULL) {
            free(req->user_data);
            req->user_data = NULL;
        }

        free(req);
        req = NULL;
    }

    return;
}

STATIC int __dm_pending_req_add(DM_INTF_S *intf, const DM_ADDR_ST *addr, unsigned int addr_len, const DM_MSG_ST *msg,
                                DM_MSG_CMD_HNDL_T rsp_hndl, const void *user_data, int data_len,
                                PENDING_REQ_T **pending_req)
{
    DM_CB_S *cb = (DM_CB_S *)(intf->dm_cb);
    PENDING_REQ_T *req = NULL;
    int ret;
    DATA_LEN_CHECK();
    req = malloc(sizeof(*req));
    if (req == NULL) {
        DEV_MON_ERR("malloc fail, errno=%d.\n", errno);
        return -ENOMEM;
    }

    ret = memset_s((void *)req, sizeof(*req), 0, sizeof(*req));
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail: %d\n", ret);
        free(req);
        req = NULL;
        return ret;
    }

    *pending_req = req;
    req->intf = intf;
    req->addr = *addr;
    req->addr_len = addr_len;
    req->rsp_hndl = rsp_hndl;
    /* copy the message, for retries */
    req->msg = *msg;

    if (req->msg.data_len > sizeof(req->msg_data)) {
        req->msg.data_len = sizeof(req->msg_data);
    }

    req->msg.data = req->msg_data;
    ret = memmove_s(req->msg_data, DM_MSG_DATA_MAX, msg->data, req->msg.data_len);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(req); req = NULL; DEV_MON_ERR("memmove_s error\n"));

    /* copy the user_data */
    if ((user_data != NULL) && data_len != 0) {
        req->user_data = malloc((size_t)data_len);
        if (req->user_data == NULL) {
            DEV_MON_ERR("malloc fail, errno=%d.\n", errno);
            free(req);
            req = NULL;
            return -ENOMEM;
        }

        ret = memset_s((void *)req->user_data, (size_t)data_len, 0, (size_t)data_len);
        if (ret != 0) {
            DEV_MON_ERR("memset_s fail: %d\n", ret);
            free(req->user_data);
            req->user_data = NULL;
            free(req);
            req = NULL;
            return ret;
        }

        ret = memmove_s(req->user_data, (size_t)data_len, user_data, (size_t)data_len);
        DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(req->user_data); req->user_data = NULL; free(req); req = NULL;
                                    DEV_MON_ERR("memmove_s error\n"));
        req->data_len = data_len;
    }

    /* add timeout handler */
    req->retries = intf->retries;

    if (req->retries >= 0) {
        /* if retries >= 0, auto-response 0xC3 when timeout
           if retries < 0, do nothing */
        ret = poller_timer_add(cb->intf_poller, (int)intf->retry_time_ms, __dm_timeout_handle, &req,
                               sizeof(PENDING_REQ_T *), &req->timer_id);
        if (ret != 0) {
            free(req->user_data);
            req->user_data = NULL;
            free(req);
            req = NULL;
            return ret;
        }
    }

    /* add to pending list */
    ret = list_append(cb->pending_list, req);
    if (ret != 0) {
        if (req->retries >= 0) {
            (void)poller_timer_remove(cb->intf_poller, &req->timer_id);
        }

        free(req->user_data);
        req->user_data = NULL;
        free(req);
        req = NULL;
        return ret;
    }

    DEV_MON_INFO("finish __dm_pending_req_add\n");
    return 0;
}

STATIC int __dm_cmd_handler_cmp(const void *item1, const void *item2)
{
    if ((item1 == NULL) || (item2 == NULL)) {
        return -1;
    }

    return 0;
}

STATIC void __dm_cmd_handler_free(void *item)
{
    DM_CMD_REGISTER_T *cmd = (DM_CMD_REGISTER_T *)item;

    if (cmd != NULL) {
        if (cmd->user_data != NULL) {
            free(cmd->user_data);
            cmd->user_data = NULL;
        }

        free(cmd);
        cmd = NULL;
    }

    return;
}

STATIC void __dm_pending_req_del(DM_INTF_S *intf, signed long long msgid)
{
    DM_CB_S *cb = (DM_CB_S *)(intf->dm_cb);
    PENDING_REQ_T *req = NULL;
    PENDING_REQ_T *item = NULL;
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    bool find = FALSE;

    req = (PENDING_REQ_T *)(uintptr_t)msgid;

    list_iter_init(cb->pending_list, &iter);
    while ((node = list_iter_next(&iter)) != NULL) {
        if (list_to_item(node) == req) {
            find = TRUE;
            break;
        }
    }

    if (find) {
        item = (PENDING_REQ_T *)list_to_item(node);
        if (item->retries >= 0) {
            /* if has retry, should delete timer from timer_list
               which is added when send msg */
            (void)poller_timer_remove(cb->intf_poller, &item->timer_id);
        }
        /* if no retry, just delete the node from pending_list */
        (void)list_remove_by_tag(cb->pending_list, item, __dm_pending_req_cmp);
    }

    list_iter_destroy(&iter);

    return;
}

STATIC void __dm_rsp_handle(DM_INTF_S *intf, DM_RECV_ST *precv)
{
    DM_CB_S *cb = (DM_CB_S *)(intf->dm_cb);
    PENDING_REQ_T *req = NULL;
    PENDING_REQ_T *item = NULL;
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    bool find = FALSE;
    /*
     * About the use of msgid:
     * send address of PENDING_REQ_T as msgid, when resp comes back,
     * we can find the PENDING_REQ_T based on the msgid right time.
     */
    req = (PENDING_REQ_T *)(uintptr_t)precv->msgid;
    list_iter_init(cb->pending_list, &iter);

    while ((node = list_iter_next(&iter)) != NULL) {
        if (list_to_item(node) == req) {
            find = TRUE;
            break;
        }
    }

    if (find && (unsigned long)(uintptr_t)req != 0) {
        item = (PENDING_REQ_T *)list_to_item(node);
        if (item->rsp_hndl != NULL) {
            /* call user's rsp_handle to handle data */
            item->rsp_hndl(intf, precv, req->user_data, req->data_len);
        }

        __dm_pending_req_del(intf, precv->msgid);
    }

    list_iter_destroy(&iter);

    return;
}

STATIC void __dm_cmd_handle(DM_INTF_S *intf, DM_RECV_ST *precv)
{
    bool find = FALSE;
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    DM_CB_S *ipmi_cb = (DM_CB_S *)(intf->dm_cb);
    DM_CMD_REGISTER_T *cmd = NULL;

    list_iter_init(ipmi_cb->cmd_reg_list, &iter);

    if ((node = list_iter_next(&iter)) != NULL) {
        cmd = list_to_item(node);
        if (cmd == NULL) {
            DEV_MON_ERR("__dm_cmd_handle: cmd is null\n");
            return;
        }
        find = TRUE;
    }

    /* if find no cmd, use ipmi_send_rsp() to send resp */
    if (!find) {
        DM_MSG_ST msg;
        /* fill in unsupported info with calling DM_MSG_UNSUP_HNDL_T */
        msg = precv->msg;
        ipmi_cb->unsup_hndl(&precv->msg, &msg);
        /* send response */
        (void)dm_send_rsp(intf, (DM_ADDR_ST *)precv->addr, precv->addr_len, &msg, precv->msgid);
    } else {
        if ((cmd != NULL) && (cmd->hndl != NULL)) {
            /* call user's cmd function to handle data */
            cmd->hndl(intf, precv, cmd->user_data, cmd->data_len);
        }
    }

    list_iter_destroy(&iter);

    return;
}

STATIC void __dm_msg_handle(DM_INTF_S *intf, DM_RECV_ST *precv)
{
    switch (precv->recv_type) {
        case RESPONSE_MSG:
            __dm_rsp_handle(intf, precv);
            break;

        case REQUEST_MSG:
            __dm_cmd_handle(intf, precv);
            break;

        default:
            break;
    }

    return;
}

STATIC void dm_recv_time_check(DM_INTF_S *intf, struct timespec start, struct timespec end)
{
    long recv_time;

    recv_time = (end.tv_sec * MS_PER_SECOND + end.tv_nsec / NS_PER_MSECOND) -
                (start.tv_sec * MS_PER_SECOND + start.tv_nsec / NS_PER_MSECOND);
    if (recv_time > DM_RECV_HANDLE_TIMEOUT) {
        intf->stats.msg_handle_timeout++;
        if ((intf->stats.msg_handle_timeout >= DM_RECV_HANDLE_TIMEOUT_CNT) &&
            (intf->stats.msg_handle_timeout <= DM_RECV_HANDLE_TIMEOUT_MAX)) {
            DEV_MON_ERR("Recv handle cost long time. (intf_name=%s; timeout=%u; recv_time=%llums)\n", intf->name,
                        intf->stats.msg_handle_timeout, recv_time);
        }
    } else {
        intf->stats.msg_handle_timeout = 0;
    }
}

STATIC void __dm_recv(int fd, short revents, void *user_data, int data_len)
{
    DM_INTF_S *intf = *((DM_INTF_S **)user_data);
    unsigned char *data = NULL;
    DM_ADDR_ST addr = {0};
    DM_RECV_ST st_recv = {0};
    int ret;
    struct timespec time_start = {0, 0};
    struct timespec time_end = {0, 0};

    (void)data_len;

    if (intf == NULL) {
        DEV_MON_ERR("__dm_recv: intf is null.\n");
        return;
    }

    if (intf->recv_msg == NULL) {
        DEV_MON_ERR("__dm_recv: intf->recv_msg is null.\n");
        return;
    }

    data = (unsigned char *)malloc(DM_MSG_DATA_MAX);
    DRV_CHECK_RET((data != NULL));
    ret = memset_s(data, DM_MSG_DATA_MAX, 0, DM_MSG_DATA_MAX);
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail. (ret=%d)\n", ret);
        free(data);
        data = NULL;
        return;
    }

    st_recv.msg.data = data;
    st_recv.msg.data_len = DM_MSG_DATA_MAX;
    st_recv.addr = (unsigned char *)&addr;
    st_recv.addr_len = sizeof(addr);
    st_recv.dev_id = 0;

    if (intf->recv_msg != NULL) {
        /* call intf->recv_msg to receive msg */
        (void)clock_gettime(CLOCK_MONOTONIC, &time_start);
        ret = intf->recv_msg(intf, fd, revents, &st_recv);
        if (ret == 0) {
            /* handle msg */
            __dm_msg_handle(intf, &st_recv);
        }
        (void)clock_gettime(CLOCK_MONOTONIC, &time_end);
        dm_recv_time_check(intf, time_start, time_end);
    }

    free(data);
    data = NULL;
    return;
}

STATIC int __dm_intf_cmp(const void *item1, const void *item2)
{
    const DM_INTF_S *intf1 = (const DM_INTF_S *)item1;
    const DM_INTF_S *intf2 = (const DM_INTF_S *)item2;

    if ((intf1 == NULL) || (intf2 == NULL)) {
        return -EINVAL;
    }

    return strncmp(intf1->name, intf2->name, sizeof(intf1->name));
}

STATIC void __dm_intf_free(void *item)
{
    DM_INTF_S *intf = (DM_INTF_S *)item;

    if (intf != NULL) {
        if (intf->close != NULL) {
            intf->close(intf);
        }

        free(intf);
        intf = NULL;
    }

    return;
}

int dm_cmd_register(DM_CB_S *dm_cb, DM_MSG_CMD_HNDL_T hndl, DM_MSG_UNSUP_HNDL_T unsup_hndl, const void *user_data,
                    int data_len)
{
    DM_CMD_REGISTER_T *req = NULL;
    int ret;

    if ((dm_cb == NULL) || (data_len <= 0)) {
        DEV_MON_ERR("dm_cmd_register input dm_cb null!\n");
        return -EINVAL;
    }

    req = calloc(1, sizeof(*req));
    if (req == NULL) {
        DEV_MON_ERR("dm_cmd_register alloc mem error, errno=%d.\n", errno);
        return -ENOMEM;
    }

    req->hndl = hndl;
    dm_cb->unsup_hndl = unsup_hndl;

    if ((user_data != NULL) && data_len != 0) {
        req->user_data = calloc(1, (size_t)data_len);
        if (req->user_data == NULL) {
            DEV_MON_ERR("calloc fail, errno=%d.\n", errno);
            free(req);
            req = NULL;
            return -ENOMEM;
        }

        ret = memcpy_s(req->user_data, (size_t)data_len, user_data, (size_t)data_len);
        DRV_CHECK_RETV_DO_SOMETHING((ret == 0), ret, free(req->user_data); req->user_data = NULL; free(req); req = NULL;
                                    DEV_MON_ERR("memcpy_s error\n"));
        req->data_len = data_len;
    }

    /* which has been register should be added to cmd_reg_list */
    ret = list_append(dm_cb->cmd_reg_list, (void *)req);
    if (ret != 0) {
        if (req->user_data != NULL) {
            free(req->user_data);
            req->user_data = NULL;
        }

        free(req);
        req = NULL;
    }

    return 0; // lint !e593 !e429
}

int dm_cmd_deregister(DM_CB_S *dm_cb)
{
    int ret = 0;

    if (dm_cb == NULL) {
        return -EINVAL;
    }

    return ret;
}

void dm_destroy(DM_CB_S *dm_cb)
{
    int ret;

    if (dm_cb == NULL) {
        return;
    }

    if (dm_cb->intf_poller != NULL) {
        ret = poller_destory(dm_cb->intf_poller);
        if (ret != 0) {
            DEV_MON_ERR("Poller destroy fail. (ret=%d; state=%d)\n", ret, (dm_cb->intf_poller)->state);
            return;
        }
        dm_cb->intf_poller = NULL;
    }

    if (dm_cb->pending_list != NULL) {
        list_destroy(dm_cb->pending_list);
        dm_cb->pending_list = NULL;
    }

    if (dm_cb->intf_list != NULL) {
        list_destroy(dm_cb->intf_list);
        dm_cb->intf_list = NULL;
    }

    if (dm_cb->cmd_reg_list != NULL) {
        list_destroy(dm_cb->cmd_reg_list);
        dm_cb->cmd_reg_list = NULL;
    }

    if (dm_cb != NULL) {
        free(dm_cb);
        dm_cb = NULL;
    }

    return;
}

DM_INTF_S *dm_get_intf(DM_CB_S *cb, const char *name, int name_len)
{
    DM_INTF_S *intf = NULL;
    DM_INTF_S *tmp = NULL;
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    int ret = 0;

    if ((cb == NULL) || (name == NULL) || name_len > DM_INTF_NAME_LEN) {
        DEV_MON_ERR("dm_get_intf input cb or name null ,name len %d larger than %d\n", name_len, DM_INTF_NAME_LEN);
        return NULL;
    }

    list_iter_init(cb->intf_list, &iter);

    while ((node = list_iter_next(&iter)) != NULL) {
        tmp = (DM_INTF_S *)(list_to_item(node));
        if (tmp == NULL) {
            DEV_MON_ERR("dm_get_intf:tmp is null!\n");
            break;
        }

        ret = strncmp(tmp->name, name, sizeof(tmp->name));
        if (ret == 0) {
            intf = tmp;
            break;
        }
    }

    list_iter_destroy(&iter);
    return intf;
}

int dm_init(DM_CB_S **dm_cb)
{
    DM_CB_S *cb = NULL;
    int ret;

    if (dm_cb == NULL) {
        DEV_MON_ERR("dm_init input dm_cb null.");
        return -EINVAL;
    }

    cb = malloc(sizeof(DM_CB_S));
    if (cb == NULL) {
        DEV_MON_ERR("dm_init malloc cb error, errno=%d.", errno);
        return -ENOMEM;
    }

    ret = memset_s((void *)cb, sizeof(DM_CB_S), 0, sizeof(DM_CB_S));
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail: %d\n", ret);
        free(cb);
        cb = NULL;
        return ret;
    }

    cb->cmd_reg_list = NULL;
    cb->intf_list = NULL;
    cb->intf_poller = NULL;
    cb->pending_list = NULL;
    *dm_cb = cb;
    ret = poller_create(&(cb->intf_poller));
    if (ret != 0) {
        DEV_MON_ERR("call poller_create fail,ret:%d\r\n", ret);
        goto out;
    }

    ret = poller_run(cb->intf_poller);
    if (ret != 0) {
        DEV_MON_ERR("call poller_run fail,ret:%d\r\n", ret);
        goto out;
    }

    ret = list_create(&(cb->intf_list), __dm_intf_cmp, __dm_intf_free);
    if (ret != 0) {
        DEV_MON_ERR("create intf list fail,ret:%d\r\n", ret);
        goto out;
    }

    ret = list_create(&(cb->pending_list), __dm_pending_req_cmp, __dm_pending_req_free);
    if (ret != 0) {
        DEV_MON_ERR("create pending list fail,ret:%d\r\n", ret);
        goto out;
    }

    ret = list_create(&(cb->cmd_reg_list), __dm_cmd_handler_cmp, __dm_cmd_handler_free);
    if (ret != 0) {
        DEV_MON_ERR("create cmd_reg_list fail,ret:%d\r\n", ret);
        goto out;
    }

    ret = selfloop_init(NULL, cb, NULL, NULL, 0);
    if (ret != 0) {
        DEV_MON_ERR("selfloop init fail,ret:%d\r\n", ret);
        goto out;
    }

out:

    if (ret != 0) {
        dm_destroy(cb);
        *dm_cb = NULL;
    }

    return ret;
}

int dm_intf_register(DM_CB_S *cb, DM_INTF_S *intf)
{
    int ret;

    if ((cb == NULL) || (intf == NULL)) {
        DEV_MON_ERR("dm_intf_register input cb or intf null.\n");
        return -EINVAL;
    }

    ret = list_append(cb->intf_list, intf);
    if (ret != 0) {
        return ret;
    }

    /* add openipmi fd into fd_list of poller */
    ret = poller_fd_add(cb->intf_poller, intf->rfd, POLLIN, __dm_recv, &intf, sizeof(DM_INTF_S *));
    if (ret != 0) {
        (void)list_del_by_tag(cb->intf_list, intf, __dm_intf_cmp);
        return ret;
    }

    return 0;
}

int dm_intf_deregister(DM_INTF_S *intf)
{
    int ret;
    DM_CB_S *cb = NULL;

    if (intf == NULL) {
        DEV_MON_ERR("dm_intf_deregister input intf null.\n");
        return -EINVAL;
    }

    cb = (DM_CB_S *)intf->dm_cb;
    ret = poller_fd_remove(cb->intf_poller, intf->rfd, POLLIN);
    if (ret == 0) {
        ret = list_remove_by_tag(cb->intf_list, intf, __dm_intf_cmp);
    }

    return ret;
}

int dm_send_req(DM_INTF_S *intf, DM_ADDR_ST *addr, unsigned int addr_len, const DM_MSG_ST *msg,
                DM_MSG_CMD_HNDL_T rsp_hndl, const void *user_data, int data_len)
{
    PENDING_REQ_T *p_req = NULL;
    int ret;
    intptr_t tmpptr;
    DSMI_DFT_RES_CMD *send_msg = NULL;
    unsigned short opcode = INVALID_OPCODE;

    if ((intf == NULL) || (addr == NULL) || (addr_len == 0) || (msg == NULL)) {
        return DRV_ERROR_PARA_ERROR;
    }

    /* print opcode */
    if (msg->data != NULL) {
        send_msg = (DSMI_DFT_RES_CMD *)(msg->data);
        opcode = send_msg->opcode;
    } else {
        DEV_MON_ERR("MSG data is NULL.\n");
    }

    /* add msg into pending list */
    ret = __dm_pending_req_add(intf, addr, addr_len, msg, rsp_hndl, user_data, data_len, &p_req);
    if (ret != 0) {
        return DRV_ERROR_INNER_ERR;
    }

    tmpptr = (intptr_t)p_req;
    /* call intf->send msg function to send msg */
    if (intf->send_msg != NULL) {
        ret = intf->send_msg(intf, REQUEST_MSG, addr, addr_len, msg, (long)tmpptr);
        DEV_MON_DEBUG("finish call intf->send_msg, ret = %d\n", ret);
        /* if send fail , do not handle */
        if (ret != 0) {
            DEV_MON_ERR("failed call intf->send_msg, ret = %d, send_msg->opcode = 0x%x.\n", ret, opcode);
            ret = DRV_ERROR_SEND_MESG;
        }
        DEV_MON_DEBUG("finish call dm_send_req\n");
    }

    return ret;
}

int dm_send_rsp(DM_INTF_S *intf, DM_ADDR_ST *addr, unsigned int addr_len, const DM_MSG_ST *msg, signed long seq_no)
{
    int ret;
    if ((intf == NULL) || (intf->send_msg == NULL)) {
        DEV_MON_ERR("Invalid parameter. (intf_is_null=%d or intf->send_msg=null)\n", (intf != NULL));
        return DRV_ERROR_PARA_ERROR;
    }
    /* send rep message */
    ret = intf->send_msg(intf, RESPONSE_MSG, addr, addr_len, msg, (long)seq_no);
    return ret;
}
