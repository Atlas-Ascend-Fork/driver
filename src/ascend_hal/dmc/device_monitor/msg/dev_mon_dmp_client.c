/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ascend_inpackage_hal.h"
#include "dev_mon_dmp_client.h"
#include "dm_smbus.h"
#include "dsmi_common_interface.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DEV_PROPERTY_TAG_LEN 256

STATIC hash_table *g_client_rsp_hashtable = NULL;

STATIC LIST_T *g_slice_msg_list = NULL; /* slice msg register list */

static void free_client_resp_node(void *resp)
{
    DEV_MON_CLIENT_RESP_TAG_VALUE_ST *value = NULL;
    DRV_CHECK_RET(resp != NULL);
    value = (DEV_MON_CLIENT_RESP_TAG_VALUE_ST *)resp;

    if (value->data != NULL) {
        free(value->data);
        value->data = NULL;
    }

    free(resp);
    resp = NULL;
}

#define SET_CTL_CB(ctl, ret, intf, addr, addr_len, rsp_hndl, user_data, data_len)                     \
    do {                                                                                              \
        (ctl)->intf = intf;                                                                           \
        (ctl)->addr = *(addr);                                                                        \
        (ctl)->addr_len = addr_len;                                                                   \
        (ctl)->rsp_hndl = rsp_hndl;                                                                   \
        (ctl)->user_data = NULL;                                                                      \
        (ctl)->data_len = 0;                                                                          \
        (ctl)->next = NULL;                                                                           \
        if ((user_data) && ((data_len) > 0)) {                                                        \
            (ctl)->user_data = malloc((size_t)(data_len));                                            \
            if (!(ctl)->user_data) {                                                                  \
                goto error_out;                                                                       \
            }                                                                                         \
            if (memset_s((void *)(ctl)->user_data, (size_t)(data_len), 0, (size_t)(data_len)) != 0) { \
                goto error_out;                                                                       \
            }                                                                                         \
            (ret) = memcpy_s((ctl)->user_data, (size_t)(data_len), user_data, (size_t)(data_len));    \
            (ctl)->data_len = data_len;                                                               \
        }                                                                                             \
    } while (0)

#define SET_BIT(x, y) ((x) |= (1U << (y)))
#define CLR_BIT(x, y) ((x) &= (unsigned char)(~((unsigned char)(1U << (unsigned char)(y)))))

STATIC void slice_msg_free(void *ctl_cb)
{
    SEND_CTL_CB *cb = (SEND_CTL_CB *)ctl_cb;
    if (cb != NULL) {
        if (cb->user_data != NULL) {
            free(cb->user_data);
            cb->user_data = NULL;
        }

        if (cb->msg != NULL) {
            if (cb->msg->data != NULL) {
                free(cb->msg->data);
                cb->msg->data = NULL;
            }
            free(cb->msg);
            cb->msg = NULL;
        }

        free(cb);
        cb = NULL;
    }
}

STATIC int slice_msg_cmp(const void *item1, const void *item2)
{
    if (item1 == item2) {
        return 0;
    }

    return -1;
}

STATIC void free_current_ctl_cb(const void *item)
{
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    bool find = FALSE;

    list_iter_init(g_slice_msg_list, &iter);
    while ((node = list_iter_next(&iter)) != NULL) {
        if (list_to_item(node) == item) {
            find = TRUE;
            break;
        }
    }
    if (find) {
        /* if no retry, just delete the node from pending_list */
        (void)list_remove_by_tag(g_slice_msg_list, item, slice_msg_cmp);
    }

    list_iter_destroy(&iter);

    return;
}

STATIC void put_node_ctl_cb(LIST_T *list, SEND_CTL_CB *cb_head)
{
    SEND_CTL_CB *p = cb_head;
    SEND_CTL_CB *tmp_p = NULL;

    while (p != NULL) {
        tmp_p = p->next;
        list_node_put(list, p->split_msg_node);
        p = tmp_p;
    }
}

STATIC void free_send_ctl_cb(SEND_CTL_CB *cb_head)
{
    bool find = FALSE;
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    SEND_CTL_CB *p = NULL;
    SEND_CTL_CB *tmp_p = NULL;
    p = cb_head;

    list_iter_init(g_slice_msg_list, &iter);
    while ((node = list_iter_next(&iter)) != NULL) {
        if (list_to_item(node) == cb_head) {
            find = TRUE;
            break;
        }
    }

    if (find) {
        while (p != NULL) {
            tmp_p = p->next;
            free_current_ctl_cb(p);
            p = tmp_p;
        }
    }
    list_iter_destroy(&iter);

    return;
}

STATIC void clear_client_rsp_data(hash_table *hashtable, DM_INTF_S *intf, DM_RECV_ST *data)
{
    char property_tag[DEV_PROPERTY_TAG_LEN] = {0};
    char *p_property_tag = property_tag;
    DEV_MP_MSG_ST *tmp = NULL;
    int ret;
    DRV_CHECK_RET((hashtable != NULL));
    DRV_CHECK_RET((intf != NULL));
    DRV_CHECK_RET((data != NULL));
    tmp = (DEV_MP_MSG_ST *)data->msg.data;
    DRV_CHECK_RET((tmp != NULL));
    ret = sprintf_s(property_tag, sizeof(property_tag), "tag.%u.%u", tmp->op_fun, tmp->op_cmd);
    DRV_CHECK_RET_DO_SOMETHING((ret >= 0), DEV_MON_ERR("sprintf_s call failed! ret = %d\n", ret));
    halHashTableRm(hashtable, p_property_tag);
}

STATIC DEV_MON_CLIENT_RESP_TAG_VALUE_ST *client_resp_hash_insert(hash_table *hashtable, DM_INTF_S *intf,
                                                                 unsigned char op_fun, unsigned char op_cmd,
                                                                 unsigned short data_len, unsigned char *data)
{
    char property_tag[DEV_PROPERTY_TAG_LEN] = {0};
    char *p_property_tag = property_tag;
    char *tmp_buf = NULL;
    DM_REP_MSG *ob = NULL;
    unsigned int malloc_len = 0;
    int ret;
    DEV_MON_CLIENT_RESP_TAG_VALUE_ST *value = NULL;
    DRV_CHECK_RETV((hashtable != NULL), NULL);
    DRV_CHECK_RETV((intf != NULL), NULL);
    DRV_CHECK_RETV((data != NULL), NULL);
    DRV_CHECK_RETV((data_len > 0), NULL);
    DRV_CHECK_RETV((data_len <= P_MAX_LENGTH), NULL);

    ret = sprintf_s(property_tag, sizeof(property_tag), "tag.%u.%u", op_fun, op_cmd);
    DRV_CHECK_RETV_DO_SOMETHING((ret >= 0), NULL, DEV_MON_ERR("sprintf_s call failed! ret = %d\n", ret));
    value = halHashTableGet(hashtable, p_property_tag);
    if (value != NULL) {
        /* if there is already data, refresh it */
        /* re-apply for space */
        DRV_CHECK_RETV((data_len > DDMP_CMD_RESP_HEAD_LEN), NULL);
        DRV_CHECK_RETV((value->data_len > DDMP_CMD_RESP_HEAD_LEN), NULL);
        malloc_len = (data_len + value->data_len - DDMP_CMD_RESP_HEAD_LEN);
        DRV_CHECK_RETV((malloc_len > value->data_len), NULL);
        tmp_buf = (char *)malloc(malloc_len);
        DRV_CHECK_RETV((tmp_buf != NULL), NULL);
        ret = memset_s((void *)tmp_buf, malloc_len, 0, malloc_len);
        if (ret != 0) {
            DEV_MON_ERR("memset_s fail: %d\n", ret);
            free(tmp_buf);
            tmp_buf = NULL;
            return NULL;
        }

        ret = memcpy_s(tmp_buf, malloc_len, value->data, value->data_len);
        DRV_CHECK_RETV_DO_SOMETHING((ret == 0), NULL, free(tmp_buf); tmp_buf = NULL);
        ret = memcpy_s(tmp_buf + value->data_len, (size_t)(malloc_len - value->data_len), data + DDMP_CMD_RESP_HEAD_LEN,
                       (size_t)(data_len - DDMP_CMD_RESP_HEAD_LEN));
        DRV_CHECK_RETV_DO_SOMETHING((ret == 0), NULL, free(tmp_buf); tmp_buf = NULL);
        free(value->data);
        value->data = tmp_buf;
        value->data_len = malloc_len;
        /* refresh the message content */
        ob = (DM_REP_MSG *)value->data;
        ob->data_length = ((value->data_len - DDMP_CMD_RESP_HEAD_LEN) >= ob->total_length) ?
                              (ob->total_length) :
                              (value->data_len - DDMP_CMD_RESP_HEAD_LEN);
        return value;
    }

    value = (DEV_MON_CLIENT_RESP_TAG_VALUE_ST *)malloc(sizeof(DEV_MON_CLIENT_RESP_TAG_VALUE_ST));
    DRV_CHECK_RETV((value != NULL), NULL);
    ret = memset_s((void *)value, sizeof(DEV_MON_CLIENT_RESP_TAG_VALUE_ST), 0,
                   sizeof(DEV_MON_CLIENT_RESP_TAG_VALUE_ST));
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail: %d\n", ret);
        free(value);
        value = NULL;
        return NULL;
    }

    value->op_fun = op_fun;
    value->op_cmd = op_cmd;
    value->data_len = data_len;
    value->data = (char *)malloc(data_len);

    if (value->data == NULL) {
        DEV_MON_ERR("malloc failed\n");
        free(value);
        value = NULL;
        return NULL;
    }

    ret = memset_s((void *)value->data, data_len, 0, data_len);
    if (ret != 0) {
        DEV_MON_ERR("memset_s fail: %d\n", ret);
        free(value->data);
        value->data = NULL;
        free(value);
        value = NULL;
        return NULL;
    }

    ret = memcpy_s(value->data, data_len, data, data_len);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), NULL, free(value->data); value->data = NULL; free(value); value = NULL;
                                DEV_MON_ERR("memcpy_s call failed! ret:%d\n", ret));
    ret = halHashTablePut2(hashtable, p_property_tag, value, free_client_resp_node);
    if (ret != 0) {
        DEV_MON_ERR("hash_table_put2 fail: %d\n", ret);

        if (value->data != NULL) {
            free(value->data);
            value->data = NULL;
        }

        free(value);
        value = NULL;
        return NULL;
    }

    return value;
}

STATIC void comm_msg_handle(DM_INTF_S *intf, DM_RECV_ST *recv, const void *user_data, int data_len)
{
    int ret;
    int find = 0;
    DM_REP_MSG *ob = NULL;
    SEND_CTL_CB *p = NULL;
    SEND_CTL_CB *tmp_p = NULL;
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};

    (void)data_len;
    DRV_CHECK_RET((intf != NULL));
    DRV_CHECK_RET((recv != NULL));
    DRV_CHECK_RET((user_data != NULL));
    ret = memcpy_s(&p, sizeof(SEND_CTL_CB *), user_data, sizeof(SEND_CTL_CB *));
    DRV_CHECK_RET_DO_SOMETHING((ret == 0), DEV_MON_ERR("memcpy_s call failed! ret:%d\n", ret));
    DRV_CHECK_RET((p != NULL));

    list_iter_init(g_slice_msg_list, &iter);
    while ((node = list_iter_next(&iter)) != NULL) {
        if (list_to_item(node) == p) {
            find = 1;
            break;
        }
    }

    if (!find) {
        /* if no find */
        list_iter_destroy(&iter);
        return;
    }

    /* Check the return value; if it is not a success message, directly call the callback function to return,
       and release the command list and any existing responses */
    ob = (DM_REP_MSG *)recv->msg.data;
    DRV_CHECK_RET_DO_SOMETHING((ob != NULL), free_send_ctl_cb(p));
    DRV_CHECK_RET_DO_SOMETHING((recv->msg.data_len >= DDMP_CMD_RESP_HEAD_LEN), free_send_ctl_cb(p));

    if (ob->err_code != 0) {
        if (p->rsp_hndl != NULL) {
            p->rsp_hndl(intf, recv, p->user_data, p->data_len);
        }

        free_send_ctl_cb(p);
        p = NULL;
        clear_client_rsp_data(g_client_rsp_hashtable, intf, recv);
        goto iter_destory;
    }

    if (p->next == NULL) { /* the last frame, call the user's callback function */
        /* Confirm whether all messages have been received.
           If some messages are not fully received, resend the messages and update the offset */
        ret = dmp_msg_recv_resp(intf, recv, p);
        if (ret != 0) {
            DEV_MON_ERR("send request failed\n");
            free_send_ctl_cb(p);
            p = NULL;
        }
        goto iter_destory;
    } else {
        tmp_p = p->next;
        free_current_ctl_cb(p);
        ret = dm_send_req(tmp_p->intf, &tmp_p->addr, tmp_p->addr_len, tmp_p->msg, (DM_MSG_CMD_HNDL_T)comm_msg_handle,
                          &tmp_p, sizeof(SEND_CTL_CB *));
        if (ret != 0) {
            DEV_MON_ERR("send request failed\n");
            free_send_ctl_cb(tmp_p);
            tmp_p = NULL;
        }
    }

iter_destory:
    list_iter_destroy(&iter);
    return;
}

int dmp_msg_recv_resp(DM_INTF_S *intf, DM_RECV_ST *recv, SEND_CTL_CB *ctl)
{
    DM_REP_MSG *ob = NULL;
    DEV_MP_MSG_ST *req = NULL;
    signed int ret = 0;
    unsigned char *tmp_recv = NULL;
    DM_SMBUS_ADDR_ST *addr = NULL;
    DEV_MON_CLIENT_RESP_TAG_VALUE_ST *value = NULL;
    DRV_CHECK_RETV((intf != NULL), FAILED);
    DRV_CHECK_RETV((recv != NULL), FAILED);
    DRV_CHECK_RETV((ctl != NULL), FAILED);
    addr = (DM_SMBUS_ADDR_ST *)recv->addr;
    DRV_CHECK_RETV((addr != NULL), FAILED);

    if ((addr->addr_type == DM_SMBUS_ADDR_TYPE) && (addr->channel == DM_SMBUS_CHANNEL)) {
        if (ctl->rsp_hndl != NULL) {
            ctl->rsp_hndl(intf, recv, ctl->user_data, ctl->data_len);
        }
        free_send_ctl_cb(ctl);
        return OK;
    }

    ob = (DM_REP_MSG *)recv->msg.data;
    DRV_CHECK_RETV((ob != NULL), FAILED);
    DRV_CHECK_RETV((recv->msg.data_len >= DDMP_CMD_RESP_HEAD_LEN), FAILED);
    value = client_resp_hash_insert(g_client_rsp_hashtable, intf, ob->op_fun, ob->op_cmd, recv->msg.data_len,
                                    recv->msg.data);
    if (value == NULL) {
        return FAILED;
    }

    if (ob->total_length > DM_MSG_DATA_MAX) {
        DEV_MON_ERR("ob total_length illegal!total_length:%u\n", ob->total_length);
        clear_client_rsp_data(g_client_rsp_hashtable, intf, recv);
        return FAILED;
    }

    if (value->data_len == (ob->total_length + DDMP_CMD_RESP_HEAD_LEN)) { /* the data has been collected */
        /* reorganization news */
        tmp_recv = recv->msg.data;
        recv->msg.data = (unsigned char *)malloc((size_t)value->data_len);

        if (recv->msg.data == NULL) {
            recv->msg.data = tmp_recv;
            DEV_MON_ERR("malloc recv msg data fail!\n");
            clear_client_rsp_data(g_client_rsp_hashtable, intf, recv);
            return FAILED;
        }

        ret = memset_s((void *)recv->msg.data, value->data_len, 0, value->data_len);
        if (ret != 0) {
            DEV_MON_ERR("memset_s fail: %d\n", ret);
            clear_client_rsp_data(g_client_rsp_hashtable, intf, recv);
            free(recv->msg.data);
            recv->msg.data = NULL;
            return ret;
        }

        ret = memcpy_s(recv->msg.data, value->data_len, value->data, value->data_len);
        DRV_CHECK_RETV_DO_SOMETHING((ret == 0), FAILED, free(recv->msg.data); recv->msg.data = tmp_recv;
                                    clear_client_rsp_data(g_client_rsp_hashtable, intf, recv));
        recv->msg.data_len = (unsigned short)value->data_len;
        ob = (DM_REP_MSG *)recv->msg.data;
        ob->data_length = (value->data_len - DDMP_CMD_RESP_HEAD_LEN);
        clear_client_rsp_data(g_client_rsp_hashtable, intf, recv);
        if (ctl->rsp_hndl != NULL) {
            ctl->rsp_hndl(intf, recv, ctl->user_data, ctl->data_len);
        }
        free(recv->msg.data);
        recv->msg.data = NULL;
        free_current_ctl_cb(ctl);
        recv->msg.data = tmp_recv;
        return OK;
    } else if (value->data_len > (ob->total_length + DDMP_CMD_RESP_HEAD_LEN)) {
        DEV_MON_ERR("value data_len is illegal! (data_len=%u; total_len=%u)\n", value->data_len, ob->total_length);
        clear_client_rsp_data(g_client_rsp_hashtable, intf, recv);
        return FAILED;
    } else {
        /* modify the request message offset and resend it */
        req = (DEV_MP_MSG_ST *)ctl->msg->data;
        DRV_CHECK_RETV_DO_SOMETHING((req != NULL), FAILED, clear_client_rsp_data(g_client_rsp_hashtable, intf, recv));
        DRV_CHECK_RETV_DO_SOMETHING((value->data_len > DDMP_CMD_RESP_HEAD_LEN), FAILED,
                                    clear_client_rsp_data(g_client_rsp_hashtable, intf, recv));
        DRV_CHECK_RETV_DO_SOMETHING((intf->max_trans_len > DDMP_CMD_RESP_HEAD_LEN), FAILED,
                                    clear_client_rsp_data(g_client_rsp_hashtable, intf, recv));
        req->offset = (value->data_len - DDMP_CMD_RESP_HEAD_LEN);
        req->length = (intf->max_trans_len - DDMP_CMD_RESP_HEAD_LEN);
        ret = dm_send_req(ctl->intf, &ctl->addr, ctl->addr_len, ctl->msg, (DM_MSG_CMD_HNDL_T)comm_msg_handle, &ctl,
                          sizeof(SEND_CTL_CB *));
        if (ret != 0) {
            DEV_MON_ERR("send request failed\n");
            clear_client_rsp_data(g_client_rsp_hashtable, intf, recv);
            return FAILED;
        }

        return OK;
    }
}

STATIC SEND_CTL_CB *malloc_new_slice_msg(DM_INTF_S *intf)
{
    int ret;
    SEND_CTL_CB *cb_head = NULL;

    cb_head = (SEND_CTL_CB *)malloc(sizeof(SEND_CTL_CB));
    if (cb_head == NULL) {
        return NULL;
    }

    ret = memset_s((void *)cb_head, sizeof(SEND_CTL_CB), 0, sizeof(SEND_CTL_CB));
    DRV_CHECK_GOTO((ret == 0), free_cb_head);

    cb_head->msg = (DM_MSG_ST *)malloc(sizeof(DM_MSG_ST));
    DRV_CHECK_GOTO((cb_head->msg != NULL), free_cb_head);

    ret = memset_s((void *)cb_head->msg, sizeof(DM_MSG_ST), 0, sizeof(DM_MSG_ST));
    DRV_CHECK_GOTO((ret == 0), free_msg);

    cb_head->msg->data = (unsigned char *)malloc(intf->max_trans_len);
    DRV_CHECK_GOTO((cb_head->msg->data != NULL), free_msg);

    ret = memset_s((void *)cb_head->msg->data, intf->max_trans_len, 0, intf->max_trans_len);
    DRV_CHECK_GOTO((ret == 0), free_data);

    return cb_head;

free_data:
    free(cb_head->msg->data);
free_msg:
    free(cb_head->msg);
free_cb_head:
    free(cb_head);
    return NULL;
}

STATIC SEND_CTL_CB *slice_msg(DM_INTF_S *intf, const DM_ADDR_ST *addr, unsigned int addr_len, const DM_MSG_ST *msg,
                              DM_MSG_CMD_HNDL_T rsp_hndl, const void *user_data, int data_len)
{
    int ret;
    unsigned int i;
    unsigned int count = 0;
    SEND_CTL_CB *cb_head = NULL;
    SEND_CTL_CB *tmp_cb = NULL;
    SEND_CTL_CB *p = NULL;
    DEV_MP_MSG_ST *data = NULL;
    unsigned int new_data_len = 0;
    DRV_CHECK_RETV((intf != NULL), NULL);
    DRV_CHECK_RETV((addr != NULL), NULL);
    DRV_CHECK_RETV((msg != NULL), NULL);
    DRV_CHECK_RETV((intf->max_trans_len > DEV_MON_REQUEST_HEAD_LEN), NULL);

    if (msg->data_len <= intf->max_trans_len) { /* no subcontracting required; directly call the sending interface */
        new_data_len = *(unsigned int *)(msg->data + 8);
        count = 1;
    } else {
        new_data_len = (intf->max_trans_len - DEV_MON_REQUEST_HEAD_LEN);
        DRV_CHECK_RETV((new_data_len <= (unsigned int)USHRT_MAX), NULL);
        count = (msg->data_len - DEV_MON_REQUEST_HEAD_LEN - 1U) / (unsigned short)(new_data_len);
        count++;
        DEV_MON_INFO("slice msg. (msg->data_len=%d; count=%d)\n", msg->data_len, count);
    }

    cb_head = malloc_new_slice_msg(intf);
    DRV_CHECK_RETV((cb_head != NULL), NULL);

    ret = list_append_get_node(g_slice_msg_list, cb_head, &cb_head->split_msg_node);
    if (ret != 0) {
        slice_msg_free(cb_head);
        return NULL;
    }

    SET_CTL_CB(cb_head, ret, intf, addr, addr_len, rsp_hndl, user_data, data_len);
    DRV_CHECK_GOTO((ret == 0), error_out);

    ret = memcpy_s(cb_head->msg->data, intf->max_trans_len, msg->data,
                   ((msg->data_len <= intf->max_trans_len) ? msg->data_len : intf->max_trans_len));
    DRV_CHECK_GOTO((ret == 0), error_out);

    cb_head->next = NULL;
    cb_head->msg->data_len = (msg->data_len <= (unsigned short)intf->max_trans_len) ?
                                 msg->data_len :
                                 (unsigned short)intf->max_trans_len;
    data = (DEV_MP_MSG_ST *)cb_head->msg->data;

    /* modify request data content */
    if (msg->data_len <= intf->max_trans_len) {
        SET_BIT((data->lun), 7);
    } else {
        CLR_BIT((data->lun), 7);
        data->offset = 0;
        data->length = new_data_len;
    }
    p = cb_head;

    for (i = 1; i < count; i++) {
        tmp_cb = malloc_new_slice_msg(intf);
        DRV_CHECK_GOTO((tmp_cb != NULL), error_out);

        ret = list_append_get_node(g_slice_msg_list, tmp_cb, &tmp_cb->split_msg_node);
        if (ret != 0) {
            slice_msg_free(tmp_cb);
            goto error_out;
        }

        p->next = tmp_cb;
        SET_CTL_CB(tmp_cb, ret, intf, addr, addr_len, rsp_hndl, user_data, data_len);
        DRV_CHECK_GOTO((ret == 0), error_out);

        /* copy head */
        ret = memcpy_s(tmp_cb->msg->data, intf->max_trans_len, msg->data, DEV_MON_REQUEST_HEAD_LEN);
        DRV_CHECK_GOTO((ret == 0), error_out);

        if (i != (count - 1)) {
            ret = memcpy_s((unsigned char *)(tmp_cb->msg->data + DEV_MON_REQUEST_HEAD_LEN),
                           intf->max_trans_len - DEV_MON_REQUEST_HEAD_LEN,
                           (unsigned char *)(msg->data + DEV_MON_REQUEST_HEAD_LEN + (unsigned short)(i * new_data_len)),
                           new_data_len);
            DRV_CHECK_GOTO((ret == 0), error_out);
            tmp_cb->msg->data_len = (short unsigned int)intf->max_trans_len;
            data = (DEV_MP_MSG_ST *)tmp_cb->msg->data;
            /* modify request data content */
            CLR_BIT((data->lun), 7);
            data->offset = (i * new_data_len);
            data->length = new_data_len;
        } else {
            ret = memcpy_s(
                ((unsigned char *)(tmp_cb->msg->data + DEV_MON_REQUEST_HEAD_LEN)),
                intf->max_trans_len - DEV_MON_REQUEST_HEAD_LEN,
                ((unsigned char *)(msg->data + DEV_MON_REQUEST_HEAD_LEN + (unsigned short)(i * new_data_len))),
                (msg->data_len - DEV_MON_REQUEST_HEAD_LEN - (unsigned short)(i * new_data_len)));
            DRV_CHECK_GOTO((ret == 0), error_out);
            tmp_cb->msg->data_len = (unsigned short)(msg->data_len - (unsigned short)(i * new_data_len));
            data = (DEV_MP_MSG_ST *)tmp_cb->msg->data;
            /* modify request data content */
            SET_BIT((data->lun), 7);
            data->offset = (i * new_data_len);
            data->length = (tmp_cb->msg->data_len - DEV_MON_REQUEST_HEAD_LEN);
            break;
        }

        tmp_cb->next = NULL;
        tmp_cb->msg->data_len = (unsigned short)intf->max_trans_len;
        p = tmp_cb;
    }
    return cb_head;
error_out:
    put_node_ctl_cb(g_slice_msg_list, cb_head);
    free_send_ctl_cb(cb_head);
    cb_head = NULL;
    return NULL;
}

int slice_msg_list_init(void)
{
    int ret;

    if (g_slice_msg_list != NULL) {
        DEV_MON_WARNING("g_slice_msg_list already init.");
        return 0;
    }

    ret = list_create(&g_slice_msg_list, slice_msg_cmp, slice_msg_free);
    if (ret != 0) {
        DEV_MON_ERR("create g_slice_msg_list fail,ret:%d\r\n", ret);
        return ret;
    }

    return 0;
}

void slice_msg_list_uninit(void)
{
    if (g_slice_msg_list != NULL) {
        list_destroy(g_slice_msg_list);
    }
    g_slice_msg_list = NULL;
}

int client_rsp_hashtable_init(void)
{
    if (g_client_rsp_hashtable != NULL) {
        DEV_MON_WARNING("g_client_rsp_hashtable already init.");
        return 0;
    }

    g_client_rsp_hashtable = halHashTableNew();
    if (g_client_rsp_hashtable == NULL) {
        DEV_MON_ERR("create g_client_rsp_hashtable fail.");
        return FAILED;
    }
    return 0;
}

void client_rsp_hashtable_uninit(void)
{
    if (g_client_rsp_hashtable != NULL) {
        halHashTableDelete(g_client_rsp_hashtable);
        g_client_rsp_hashtable = NULL;
    }
}

int dev_mon_send_request(DM_INTF_S *intf, DM_ADDR_ST *addr, unsigned int addr_len, const DM_MSG_ST *msg,
                         DM_MSG_CMD_HNDL_T rsp_hndl, const void *user_data, int data_len)
{
    int ret;
    SEND_CTL_CB *cb_head = NULL;
    DRV_CHECK_RETV((intf != NULL), DRV_ERROR_PARA_ERROR);
    DRV_CHECK_RETV((addr != NULL), DRV_ERROR_PARA_ERROR);
    DRV_CHECK_RETV((msg != NULL), DRV_ERROR_PARA_ERROR);
    DRV_CHECK_RETV((addr_len <= sizeof(DM_ADDR_ST)), DRV_ERROR_PARA_ERROR);
    cb_head = slice_msg(intf, addr, addr_len, msg, rsp_hndl, user_data, data_len);
    if (cb_head == NULL) {
        DEV_MON_ERR("slice_msg return NULL pointer\n");
        return DRV_ERROR_PARA_ERROR;
    }

    ret = dm_send_req(cb_head->intf, addr, cb_head->addr_len, cb_head->msg, (DM_MSG_CMD_HNDL_T)comm_msg_handle,
                      &cb_head, sizeof(SEND_CTL_CB *));
    put_node_ctl_cb(g_slice_msg_list, cb_head);
    if (ret != 0) {
        free_send_ctl_cb(cb_head);
        return ret;
    }
    return ret;
}
