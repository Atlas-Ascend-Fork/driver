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
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_driver_pub.h"
#include "ka_hashtable_pub.h"
#include "ka_system_pub.h"
#include "ka_memory_pub.h"
#include "ka_ioctl_pub.h"

#include "ascend_hal_define.h"
#include "ascend_kernel_hal.h"
#include "pbl/pbl_spod_info.h"
#include "securec.h"
#include "comm_kernel_interface.h"

#include "trs_shr_id_fops.h"
#include "trs_shr_id.h"
#include "trs_shr_id_spod_msg.h"
#include "trs_shr_id_spod.h"
#include "trs_shr_id_auto_init.h"

STATIC KA_TASK_DEFINE_MUTEX(spod_mutex);

#define SHR_ID_PID_POD_MAX_NUM (SHR_ID_PID_MAX_NUM - 1)
#define USLEEP_MIN 5000
#define USLEEP_MAX 6000
enum trs_s2s_msg_status {
    TRS_S2S_MSG_STATUS_INIT = 0,
    TRS_S2S_MSG_STATUS_SEND,
    TRS_S2S_MSG_STATUS_RECV,
    TRS_S2S_MSG_STATUS_ABNORMAL,
};

struct shr_id_spod_wlist {
    u32 sdid;
    ka_pid_t pid;
};

struct shr_id_pod_create_info {
    u32 sdid;
    ka_pid_t pid;
    int server_abnormal;
    enum trs_s2s_msg_status status;
};

enum shadow_node_recycle_strategy {
    TRS_SHADOW_NODE_RETRY = 0,
    TRS_SHADOW_NODE_DESTORY,
    TRS_SHADOW_NODE_EXIT,
    TRS_SHADOW_NODE_MAX,
};

struct shr_id_spod_info {
    struct shr_id_spod_wlist wlist[SHR_ID_PID_POD_MAX_NUM];
    struct shr_id_pod_create_info create_info[SHR_ID_PID_SERVER_ID_MAX_NUM];
    u32 shadow_node_num;
    ka_delayed_work_t shrid_recycle_work; /* recycle shr id */
    void *node;
    struct shr_id_node_op_attr attr;
    ka_atomic_t in_recycling; /* recycling spod_info in workqueue */
};

#ifdef CFG_FEATURE_SDID_STUB
STATIC int hal_kernel_get_spod_node_status_stub(u32 dev_id, u32 sdid, u32 *status)
{
    (void)dev_id;
    (void)sdid;
    *status = DMS_SPOD_NODE_STATUS_NORMAL;
    return 0;
}
#endif

STATIC bool trs_spod_sdid_status_check(u32 devid, u32 sdid)
{
    u32 status;
    int ret;

#ifdef CFG_FEATURE_SDID_STUB
    ret = hal_kernel_get_spod_node_status_stub(devid, sdid, &status);
#else
    ret = hal_kernel_get_spod_node_status(devid, sdid, &status);
#endif

    if (ret != 0) {
#ifndef EMU_ST
        if (ret != -EOPNOTSUPP) {
            trs_debug("Can't get sdid status. (devid=%u; sdid=%u; ret=%d)\n", devid, sdid, ret);
        }
        return false;
#endif
    }

    if (status == DMS_SPOD_NODE_STATUS_NORMAL) {
        return false;
    }

    trs_debug("Sdid is abnormal. (devid=%u; sdid=%u; status=%u)\n", devid, sdid, status);
    return true;
}

#ifdef CFG_FEATURE_SUPPORT_XCOM
typedef int (*devdrv_register_p2p_msg_proc_ops)(enum devdrv_msg_client_type msg_type, devdrv_p2p_msg_recv func);
#define DEVDRV_REGISTER_P2P_MSG_PROC_OPS "xcom_register_p2p_msg_proc_func"
typedef int (*devdrv_unregister_p2p_msg_proc_ops)(enum devdrv_msg_client_type msg_type);
#define DEVDRV_UNREGISTER_P2P_MSG_PROC_OPS "xcom_unregister_p2p_msg_proc_func"
typedef int (*devdrv_p2p_msg_send_ops)(u32 local_devid, u32 sdid, enum devdrv_msg_client_type msg_type,
                                       struct data_input_info *data_info);
#define DEVDRV_P2P_MSG_SEND_OPS "xcom_p2p_msg_send"
static devdrv_p2p_msg_send_ops devdrv_p2p_msg_send_func = NULL;
#endif

STATIC int _trs_pod_msg_send(u32 devid, u32 sdid, void *msg, size_t size, int *cmd_result, u32 mode)
{
    struct trs_pod_msg_data *tmp_msg;
    struct data_input_info data;
    int ret;

    *cmd_result = 0;
    if ((msg == NULL) || (size != sizeof(struct trs_pod_msg_data))) {
        return -EINVAL;
    }

    data.data = msg;
    data.data_len = (u32)size;
    data.in_len = (u32)size;
    data.out_len = 0;
    data.msg_mode = mode;
    tmp_msg = (struct trs_pod_msg_data *)msg;
#ifdef CFG_FEATURE_SUPPORT_XCOM
    if (devdrv_p2p_msg_send_func) {
        ret = devdrv_p2p_msg_send_func(devid, sdid, devdrv_msg_client_tsdrv, &data);
    } else {
        trs_err("devdrv_p2p_msg_send_func is null.\n");
        return -EINVAL;
    }
#else
    ret = devdrv_s2s_msg_send(devid, sdid, DEVDRV_S2S_MSG_TRSDRV, 1, &data);
#endif
    *cmd_result = (int)tmp_msg->header.result;
    return ret;
}

STATIC int trs_pod_msg_send(u32 devid, u32 sdid, void *msg, size_t size)
{
    int ret, cmd_result;
    ret = _trs_pod_msg_send(devid, sdid, msg, size, &cmd_result, DEVDRV_S2S_SYNC_MODE);
    return (ret == 0) ? cmd_result : ret;
}

#ifdef CFG_FEATURE_SDID_STUB
STATIC int dbl_get_spod_info_stub(unsigned int udevid, struct spod_info *s)
{
    u32 chip_id = 0U;
    int ret = 0;
    ka_device_node_t *node = NULL;

    node = ka_driver_of_find_compatible_node(NULL, NULL, "hisi,resource_manage");
    if (node == NULL) {
        trs_err("The hisi,resource_manage is not configured in dts.\n");
        return -EINVAL;
    }
    ret = ka_driver_of_property_read_u32(node, "chip_id", &chip_id);
    if (ret != 0) {
        ka_driver_of_node_put(node);
        trs_err("chip_id get failed. (ret=%d)\n", ret);
        return -EINVAL;
    }
    ka_driver_of_node_put(node);
    if (chip_id == 0U) {
        s->sdid = 0U;
        s->server_id = 0U;
    } else {
        s->sdid = 0x400000;
        s->server_id = 1U;
    }
    return 0;
}
#endif

STATIC int shr_id_create_shadow_node_msg_send(struct shr_id_node_op_attr *attr, u32 sdid, int pid)
{
    struct shr_id_pod_create_msg *create_msg = NULL;
    struct trs_pod_msg_data msg;
    struct spod_info spod = {0};
    int ret;
#ifdef CFG_FEATURE_SDID_STUB
    ret = dbl_get_spod_info_stub(attr->inst.devid, &spod);
#else
    ret = dbl_get_spod_info(attr->inst.devid, &spod);
#endif
    if (ret != 0) {
        return ret;
    }

    msg.header.valid = TRS_POD_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_POD_MSG_CREATE_SHADOW;
    msg.header.result = 0;

    create_msg = (struct shr_id_pod_create_msg *)msg.payload;
    create_msg->attr = *attr;
    create_msg->attr.inst.devid = spod.sdid;
    create_msg->attr.res_get = NULL;
    create_msg->attr.res_put = NULL;
    create_msg->pid = pid;
    trs_debug("Create shadow info. (devid=%u; local_sdid=0x%x; sdid=0x%x;)\n", attr->inst.devid, spod.sdid, sdid);

    ret = trs_pod_msg_send(attr->inst.devid, sdid, &msg, sizeof(struct trs_pod_msg_data));
    if (ret != 0) {
        trs_err("Send fail. (devid=%u; sdid=0x%x; ret=%d; result=%d)\n", attr->inst.devid, sdid, ret,
                msg.header.result);
        return ret;
    }

    return 0;
}

STATIC int shr_id_pod_set_pid_msg_send(struct shr_id_node_op_attr *attr, u32 sdid, int pid)
{
    struct shr_id_pod_set_pid_msg *set_pid_msg = NULL;
    struct trs_pod_msg_data msg;
    int ret, i;

    msg.header.valid = TRS_POD_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_POD_MSG_SET_PID;
    msg.header.result = 0;

    set_pid_msg = (struct shr_id_pod_set_pid_msg *)msg.payload;
    for (i = 0; i < SHR_ID_NSM_NAME_SIZE; i++) {
        set_pid_msg->name[i] = attr->name[i];
    }
    set_pid_msg->pid = pid;

    ret = trs_pod_msg_send(attr->inst.devid, sdid, &msg, sizeof(struct trs_pod_msg_data));
    if (ret != 0) {
        trs_err("Send fail. (devid=%u; sdid=%u; ret=%d; result=%d)\n", attr->inst.devid, sdid, ret, msg.header.result);
        return ret;
    }
    return 0;
}

STATIC int shr_id_destory_shadow_node_msg_send(struct shr_id_node_op_attr *attr, u32 sdid, int pid, int *cmd_result,
                                               u32 mode)
{
    struct shr_id_pod_destroy_msg *destory_msg = NULL;
    struct trs_pod_msg_data msg;
    int ret, i;

    if (trs_spod_sdid_status_check(attr->inst.devid, sdid)) {
        if (mode == DEVDRV_S2S_ASYNC_MODE) {
            return -EPIPE;
        }
        *cmd_result = 0;
        return 0;
    }

    msg.header.valid = TRS_POD_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_POD_MSG_DESTORY_SHADOW;
    msg.header.result = 0;

    destory_msg = (struct shr_id_pod_destroy_msg *)msg.payload;
    for (i = 0; i < SHR_ID_NSM_NAME_SIZE; i++) {
        destory_msg->name[i] = attr->name[i];
    }
    destory_msg->pid = pid;

    ret = _trs_pod_msg_send(attr->inst.devid, sdid, &msg, sizeof(struct trs_pod_msg_data), cmd_result, mode);
    if ((ret != 0) || ((*cmd_result) != 0)) {
        trs_debug("Should send again. (devid=%u; sdid=%u; ret=%d; cmd_result=%d)\n", attr->inst.devid, sdid, ret,
                  *cmd_result);
        return ret;
    }
    return 0;
}

#ifdef CFG_FEATURE_SUPPORT_XCOM
STATIC int devdrv_s2s_async_msg_recv_stub(u32 devid, u32 sdid, enum devdrv_s2s_msg_type msg_type,
                                          struct data_recv_info *data_info)
{
    (void)devid;
    (void)sdid;
    (void)msg_type;
    (void)data_info;
    return 0;
}
#endif

int trs_pod_msg_recv_async(u32 devid, u32 sdid, void *msg, size_t size, int *cmd_result, u32 mode)
{
    struct trs_pod_msg_data *tmp_msg;
    struct data_recv_info data;
    int ret;

    *cmd_result = 0;
    if ((msg == NULL) || (size != sizeof(struct trs_pod_msg_data))) {
        return -EINVAL;
    }
    data.data = msg;
    data.data_len = (u32)size;
    data.out_len = 0;
    data.flag = mode;
    tmp_msg = (struct trs_pod_msg_data *)msg;

    if (trs_spod_sdid_status_check(devid, sdid)) {
#ifndef EMU_ST
        data.flag = DEVDRV_S2S_END_RECV;
#ifdef CFG_FEATURE_SUPPORT_XCOM
        ret = devdrv_s2s_async_msg_recv_stub(devid, sdid, DEVDRV_S2S_MSG_TRSDRV, &data);
#else
        ret = devdrv_s2s_async_msg_recv(devid, sdid, DEVDRV_S2S_MSG_TRSDRV, &data);
#endif
        return -EINVAL;
#endif
    }
#ifdef CFG_FEATURE_SUPPORT_XCOM
    ret = devdrv_s2s_async_msg_recv_stub(devid, sdid, DEVDRV_S2S_MSG_TRSDRV, &data);
#else
    ret = devdrv_s2s_async_msg_recv(devid, sdid, DEVDRV_S2S_MSG_TRSDRV, &data);

#endif
    *cmd_result = (int)tmp_msg->header.result;
    if (mode == DEVDRV_S2S_END_RECV) {
        trs_warn("Current node loop timeout, release s2s channel."
                 "(mode=%u; devid=%u; sdid=%u; ret=%d)\n",
                 mode, devid, sdid, ret);
    }
    return ret;
}

STATIC int shr_id_destory_shadow_node_msg_recv(struct shr_id_node_op_attr *attr, u32 sdid, int pid, int *cmd_result,
                                               u32 mode)
{
    struct shr_id_pod_destroy_msg *destory_msg = NULL;
    struct trs_pod_msg_data msg;
    int ret, i;

    msg.header.valid = TRS_POD_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_POD_MSG_DESTORY_SHADOW;
    msg.header.result = 0;

    destory_msg = (struct shr_id_pod_destroy_msg *)msg.payload;
    for (i = 0; i < SHR_ID_NSM_NAME_SIZE; i++) {
        destory_msg->name[i] = attr->name[i];
    }
    destory_msg->pid = pid;

    ret = trs_pod_msg_recv_async(attr->inst.devid, sdid, &msg, sizeof(struct trs_pod_msg_data), cmd_result, mode);
    return ret;
}

STATIC int shr_id_query_shadow_node_msg_send(struct shr_id_node_op_attr *attr, u32 sdid, int *cmd_result)
{
    struct shr_id_pod_query_msg *query_msg = NULL;
    struct trs_pod_msg_data msg;
    int ret, i;

    msg.header.valid = TRS_POD_MSG_SEND_MAGIC;
    msg.header.cmdtype = TRS_POD_MSG_QUERY_SHADOW;
    msg.header.result = 0;

    query_msg = (struct shr_id_pod_query_msg *)msg.payload;
    for (i = 0; i < SHR_ID_NSM_NAME_SIZE; i++) {
        query_msg->name[i] = attr->name[i];
    }

    ret = _trs_pod_msg_send(attr->inst.devid, sdid, &msg, sizeof(struct trs_pod_msg_data), cmd_result,
                            DEVDRV_S2S_SYNC_MODE);
    if ((ret != 0) || ((*cmd_result) != 0)) {
        trs_debug("Should send again. (devid=%u; sdid=%u; ret=%d; cmd_result=%d)\n", attr->inst.devid, sdid, ret,
                  *cmd_result);
        return ret;
    }
    return 0;
}

void shr_id_recycle_work(ka_work_struct_t *p_work);
STATIC struct shr_id_spod_info *shr_id_spod_info_create(void *node, struct shr_id_node_op_attr *attr)
{
    struct shr_id_spod_info *spod_info = trs_kvzalloc(sizeof(struct shr_id_spod_info),
                                                      KA_GFP_ATOMIC | __KA_GFP_ACCOUNT);
    if (spod_info == NULL) {
        return spod_info;
    }

    spod_info->node = node;
    spod_info->attr = *attr;
    KA_TASK_INIT_DELAYED_WORK(&spod_info->shrid_recycle_work, shr_id_recycle_work);
    ka_base_atomic_set(&spod_info->in_recycling, 0);
    shr_id_node_set_priv(node, (void *)spod_info);
    (void)shr_id_node_get(attr->name, attr->type); // for spod_info get

    return spod_info;
}

STATIC void shr_id_spod_info_destroy(struct shr_id_spod_info *spod_info)
{
    if (spod_info != NULL) {
        shr_id_node_set_priv(spod_info->node, NULL);
        /* shadow node ref is 0, so call node_put and free spod_info */
        shr_id_node_put(spod_info->node);
        trs_kvfree(spod_info);
        trs_debug("Spod info free.\n");
    }
}

STATIC int shr_id_get_recycle_strategy(struct shr_id_node_op_attr *attr, bool server_abnormal, bool shadow_node_exist)
{
    int ret = TRS_SHADOW_NODE_MAX;
    bool in_process;
    in_process = shr_id_is_destroyed_in_process(attr->pid, attr->start_time);
    if (in_process) {
        if (server_abnormal == true) { // if server abnormal, destroy it later in shr_id_file_release when process exit.
            ret = TRS_SHADOW_NODE_EXIT;
        } else if (shadow_node_exist) {
            ret = TRS_SHADOW_NODE_RETRY;
        } else {
            ret = TRS_SHADOW_NODE_DESTORY; // in process context, no error, destroy spod info
        }
    } else {
        if (shadow_node_exist) {
            ret = TRS_SHADOW_NODE_RETRY;
        } else {
            ret = TRS_SHADOW_NODE_DESTORY; // in release, no shadow node, force destroy spod info
        }
    }
    return ret;
}

void shr_id_recycle_work(ka_work_struct_t *p_work)
{
    struct shr_id_spod_info *spod_info = ka_container_of(p_work, struct shr_id_spod_info, shrid_recycle_work.work);
    struct shr_id_node_op_attr attr = spod_info->attr;
    int i, ret, cmd_result;
    void *shrid_node = NULL;
    bool shadow_node_exist = false;
    bool spod_server_abnormal = false;

    ka_base_atomic_set(&spod_info->in_recycling, 1);
    shrid_node = shr_id_node_get(spod_info->attr.name, spod_info->attr.type); // for spod_info get
    if (shrid_node == NULL) {
        return;
    }

    for (i = 0; i < SHR_ID_PID_SERVER_ID_MAX_NUM; i++) {
        if (spod_info->create_info[i].pid != 0) {
            if (spod_info->create_info[i].server_abnormal != 0) {
                spod_server_abnormal = true;
                continue;
            }
            ret = shr_id_query_shadow_node_msg_send(&attr, spod_info->create_info[i].sdid, &cmd_result);
            trs_debug("Query shadow. (devid=%u; serverid=%u; sdid=%u; name=%s; type=%d; id=%u; ret=%d; result=%d)\n",
                      attr.inst.devid, i, spod_info->create_info[i].sdid, attr.name, attr.type, attr.id, ret,
                      cmd_result);

            spod_info->create_info[i].server_abnormal = ret;
            if ((ret == 0) && (cmd_result == 0)) {
                spod_info->create_info[i].sdid = 0;
                spod_info->create_info[i].pid = 0;
            }
            if ((ret == 0) && (cmd_result == -EEXIST)) {
                shadow_node_exist = true;
            }
            if (ret != 0) {
                spod_server_abnormal = true;
            }
        }
    }

    ret = shr_id_get_recycle_strategy(&attr, spod_server_abnormal, shadow_node_exist);
    if (ret == TRS_SHADOW_NODE_RETRY) {
        (void)ka_task_schedule_delayed_work(&spod_info->shrid_recycle_work,
                                            ka_system_msecs_to_jiffies(300)); /* 300 ms */
    } else if (ret == TRS_SHADOW_NODE_DESTORY) {
        shr_id_node_priv_mutex_lock(shrid_node);
        ka_base_atomic_set(&spod_info->in_recycling, 0);
        shr_id_spod_info_destroy(spod_info);
        shr_id_node_priv_mutex_unlock(shrid_node);
    } else {
        ka_base_atomic_set(&spod_info->in_recycling, 0);
        trs_debug("NOT destroy shadow. (devid=%u; sdid=%u; name=%s; type=%d; id=%u; server_abnormal=%u;"
                  " shadow_exist=%u; ret=%d)\n",
                  attr.inst.devid, spod_info->wlist[0].sdid, attr.name, attr.type, attr.id, spod_server_abnormal,
                  shadow_node_exist, ret);
    }
    shr_id_node_put(shrid_node);
}

static bool shr_id_hash_find(struct shr_id_proc_ctx *proc_ctx, u32 sdid)
{
    struct shr_id_hash_node *node = NULL;
    u32 key;

    key = sdid;
    ka_task_read_lock(&proc_ctx->abnormal_ht.lock);
    ka_hash_for_each_possible(proc_ctx->abnormal_ht.htable, node, link, key)
    {
        if (node->key == sdid) {
            ka_task_read_unlock(&proc_ctx->abnormal_ht.lock);
            return true;
        }
    }
    ka_task_read_unlock(&proc_ctx->abnormal_ht.lock);
    return false;
}

static void shr_id_spod_set_sdid_pid_zero(struct shr_id_spod_info *spod_info, int server_id)
{
    spod_info->create_info[server_id].sdid = 0;
    spod_info->create_info[server_id].pid = 0;
}

static void shr_id_hash_add(struct shr_id_proc_ctx *proc_ctx, u32 sdid)
{
    struct shr_id_hash_node *node = NULL;

    if (shr_id_hash_find(proc_ctx, sdid)) {
        return;
    }
    node = trs_kmalloc(sizeof(struct shr_id_hash_node), KA_GFP_KERNEL);
    if (node == NULL) {
        trs_err("Malloc shrId abnormal node fail.\n");
        return;
    }
    node->key = sdid;
    ka_task_write_lock(&proc_ctx->abnormal_ht.lock);
    ka_hash_add(proc_ctx->abnormal_ht.htable, &node->link, node->key);
    ka_task_write_unlock(&proc_ctx->abnormal_ht.lock);
    trs_warn("Sdid is abnormal. Add to the abnormal table. (sdid=%u; pid=%d)\n", sdid, proc_ctx->pid);
}

static void shr_id_spod_server_abnormal(struct shr_id_spod_info *spod_info, int server_id, bool *spod_server_abnormal)
{
    *spod_server_abnormal = true;
    spod_info->create_info[server_id].status = TRS_S2S_MSG_STATUS_ABNORMAL;
    shr_id_spod_set_sdid_pid_zero(spod_info, server_id);
    spod_info->shadow_node_num--;
}

static void shr_id_spod_async_msg_send(struct shr_id_proc_ctx *proc_ctx, struct shr_id_spod_info *spod_info,
                                       bool *spod_server_abnormal, bool *shadow_node_exist, bool is_timeout)
{
    int i, ret, cmd_result;

    for (i = 0; i < SHR_ID_PID_SERVER_ID_MAX_NUM; i++) {
        if (spod_info->create_info[i].pid != 0 && spod_info->create_info[i].status == TRS_S2S_MSG_STATUS_INIT) {
            if (is_timeout) {
                shr_id_hash_add(proc_ctx, spod_info->create_info[i].sdid);
                shr_id_spod_server_abnormal(spod_info, i, spod_server_abnormal);
                continue;
            }
            if (shr_id_hash_find(proc_ctx, spod_info->create_info[i].sdid)) {
#ifndef EMU_ST
                shr_id_spod_server_abnormal(spod_info, i, spod_server_abnormal);
                continue;
#endif
            }
            ret = shr_id_destory_shadow_node_msg_send(&spod_info->attr, spod_info->create_info[i].sdid,
                                                      spod_info->create_info[i].pid, &cmd_result,
                                                      DEVDRV_S2S_ASYNC_MODE);
            if (ret == 0) {
#ifndef EMU_ST
                spod_info->create_info[i].status = TRS_S2S_MSG_STATUS_SEND;
                trs_debug("Async send success. (devid=%u; sdid=%u; name=%s; id=%u; status=%d; ret=%d;)\n",
                          spod_info->attr.inst.devid, spod_info->create_info[i].sdid, spod_info->attr.name,
                          spod_info->attr.id, spod_info->create_info[i].status, ret);
#endif
            } else if (ret == -EBUSY) {
                trs_debug("Async send busy. Need send again.(devid=%u; sdid=%u; name=%s; id=%u; status=%d; ret=%d;)\n",
                          spod_info->attr.inst.devid, spod_info->create_info[i].sdid, spod_info->attr.name,
                          spod_info->attr.id, spod_info->create_info[i].status, ret);
            } else {
#ifndef EMU_ST
                shr_id_hash_add(proc_ctx, spod_info->create_info[i].sdid);
                shr_id_spod_server_abnormal(spod_info, i, spod_server_abnormal);
                trs_warn("Send spod server is abnormal. (devid=%u; sdid=%u; name=%s; id=%u; status=%d; ret=%d;)\n",
                         spod_info->attr.inst.devid, spod_info->wlist[0].sdid, spod_info->attr.name, spod_info->attr.id,
                         spod_info->create_info[i].status, ret);
#endif
            }
        }
    }
}

static void shr_id_spod_async_msg_recv(struct shr_id_proc_ctx *proc_ctx, struct shr_id_spod_info *spod_info,
                                       bool *spod_server_abnormal, bool *shadow_node_exist, u32 recv_mode)
{
    int i, ret, cmd_result;

    for (i = 0; i < SHR_ID_PID_SERVER_ID_MAX_NUM; i++) {
        if (spod_info->create_info[i].pid != 0 && spod_info->create_info[i].status == TRS_S2S_MSG_STATUS_SEND) {
            ret = shr_id_destory_shadow_node_msg_recv(&spod_info->attr, spod_info->create_info[i].sdid,
                                                      spod_info->create_info[i].pid, &cmd_result, recv_mode);
            spod_info->create_info[i].server_abnormal = ret;
            if ((ret == 0) && (cmd_result == 0)) {
                spod_info->shadow_node_num--;
                spod_info->create_info[i].status = TRS_S2S_MSG_STATUS_RECV;
                shr_id_spod_set_sdid_pid_zero(spod_info, i);
                trs_debug("Async recv success. (devid=%u; sdid=%u; name=%s; id=%u; status=%d; ret=%d; result=%d)\n",
                          spod_info->attr.inst.devid, spod_info->create_info[i].sdid, spod_info->attr.name,
                          spod_info->attr.id, spod_info->create_info[i].status, ret, cmd_result);
            } else if ((ret == 0) && (cmd_result == -EEXIST)) {
                spod_info->shadow_node_num--;
                spod_info->create_info[i].status = TRS_S2S_MSG_STATUS_RECV;
                *shadow_node_exist = true;
#ifndef EMU_ST
                trs_debug(
                    "Async recv exist, delay work.(devid=%u; sdid=%u; name=%s; id=%u; status=%d; ret=%d; result=%d)\n",
                    spod_info->attr.inst.devid, spod_info->create_info[i].sdid, spod_info->attr.name,
                    spod_info->attr.id, spod_info->create_info[i].status, ret, cmd_result);
#endif
            } else if (ret == -EAGAIN) {
#ifndef EMU_ST
                trs_debug("Async recv busy. (devid=%u; sdid=%u; name=%s; id=%u; status=%d; ret=%d;)\n",
                          spod_info->attr.inst.devid, spod_info->create_info[i].sdid, spod_info->attr.name,
                          spod_info->attr.id, spod_info->create_info[i].status, ret);
#endif
            } else {
                shr_id_hash_add(proc_ctx, spod_info->create_info[i].sdid);
                shr_id_spod_server_abnormal(spod_info, i, spod_server_abnormal);
                trs_warn("Recv spod server is abnormal. (devid=%u; sdid=%u; name=%s; type=%d; id=%u; ret=%d; "
                         "in_recycle=%d)\n",
                         spod_info->attr.inst.devid, spod_info->wlist[0].sdid, spod_info->attr.name,
                         spod_info->attr.type, spod_info->attr.id, ret, ka_base_atomic_read(&spod_info->in_recycling));
            }
        }
    }
}

static int shr_id_async_send_and_recv(struct shr_id_spod_info *spod_info, bool *spod_server_abnormal,
                                      bool *shadow_node_exist, bool is_timeout)
{
    struct shr_id_proc_ctx *proc_ctx;
    u32 recv_mode;

    recv_mode = is_timeout ? DEVDRV_S2S_END_RECV : DEVDRV_S2S_KEEP_RECV;
    proc_ctx = shr_id_proc_ctx_find(spod_info->attr.pid);
    if (proc_ctx == NULL) {
#ifndef EMU_ST
        trs_err("Async recycle proc not exist.\n");
        return 0;
#endif
    }
    shr_id_spod_async_msg_send(proc_ctx, spod_info, spod_server_abnormal, shadow_node_exist, is_timeout);
    shr_id_spod_async_msg_recv(proc_ctx, spod_info, spod_server_abnormal, shadow_node_exist, recv_mode);
    return (spod_info->shadow_node_num == 0) ? 0 : -1;
}

static void shadow_node_loop_for_async_send_recv(struct shr_id_spod_info *spod_info, bool *spod_server_abnormal,
                                                 bool *shadow_node_exist)
{
    u64 start_time, current_time, time_cost;
    bool is_timeout = false;
    int ret;

    start_time = ka_system_ktime_get_ns();
    while (!is_timeout) {
        ret = shr_id_async_send_and_recv(spod_info, spod_server_abnormal, shadow_node_exist, false);
        if (ret == 0) {
            break;
        }
        ka_system_usleep_range(USLEEP_MIN, USLEEP_MAX);
        current_time = ka_system_ktime_get_ns();
        time_cost = current_time - start_time;
        if (time_cost >= MAX_LOOP_TIME) {
            trs_debug("The async collection loop has timed out.(devid=%u, pid=%u, name=%s, time_cost=%llu ns)\n",
                      spod_info->attr.inst.devid, spod_info->attr.pid, spod_info->attr.name, time_cost);
            is_timeout = true;
        }
    }
    if (is_timeout) {
        (void)shr_id_async_send_and_recv(spod_info, spod_server_abnormal, shadow_node_exist, true);
    }
}

static void shadow_node_loop_for_sync_send(struct shr_id_spod_info *spod_info, bool *spod_server_abnormal,
                                           bool *shadow_node_exist)
{
    int i, ret, cmd_result;

    for (i = 0; i < SHR_ID_PID_SERVER_ID_MAX_NUM; i++) {
        if (spod_info->create_info[i].pid != 0) {
            ret = shr_id_destory_shadow_node_msg_send(&spod_info->attr, spod_info->create_info[i].sdid,
                                                      spod_info->create_info[i].pid, &cmd_result, DEVDRV_S2S_SYNC_MODE);
            if ((ret == 0) && (cmd_result == 0)) {
                spod_info->create_info[i].sdid = 0;
                spod_info->create_info[i].pid = 0;
            }
            spod_info->create_info[i].server_abnormal = ret;
            if ((ret == 0) && (cmd_result == -EEXIST)) {
                *shadow_node_exist = true;
            }
            if (ret != 0) {
                *spod_server_abnormal = true;
            }
        }
    }
}

STATIC int shr_id_destroy_shadow_node_async(void *node, u32 mode)
{
    struct shr_id_spod_info *spod_info = NULL;
    struct shr_id_node_op_attr attr;
    bool spod_server_abnormal = false;
    bool shadow_node_exist = false;
    void *shrid_node = NULL;
    int ret;

    /* the spod_info of remote_node is NULL, not need mutex_lock */
    spod_info = (struct shr_id_spod_info *)shr_id_node_get_priv(node);
    if (spod_info == NULL) {
        return 0;
    }
    shrid_node = shr_id_node_get(spod_info->attr.name, spod_info->attr.type); // for spod_info get
    if (shrid_node == NULL) {
        return 0;
    }
    shr_id_node_priv_mutex_lock(shrid_node);
    spod_info = (struct shr_id_spod_info *)shr_id_node_get_priv(node);
    if ((spod_info == NULL)) {
        shr_id_node_priv_mutex_unlock(shrid_node);
        shr_id_node_put(shrid_node);
        return 0;
    }

    attr = spod_info->attr;
    if (mode == DEVDRV_S2S_ASYNC_MODE) {
        shadow_node_loop_for_async_send_recv(spod_info, &spod_server_abnormal, &shadow_node_exist);
    } else {
        shadow_node_loop_for_sync_send(spod_info, &spod_server_abnormal, &shadow_node_exist);
    }
    ret = shr_id_get_recycle_strategy(&attr, spod_server_abnormal, shadow_node_exist);
    if (ret == TRS_SHADOW_NODE_RETRY) {
        (void)ka_task_schedule_delayed_work(&spod_info->shrid_recycle_work, 0);
        ret = 0;
    } else if ((ret == TRS_SHADOW_NODE_DESTORY) && (ka_base_atomic_read(&spod_info->in_recycling) == 0)) {
        trs_debug("Destroy shadow. (devid=%u; sdid=%u; name=%s; type=%d; id=%u;)\n", attr.inst.devid,
                  spod_info->wlist[0].sdid, attr.name, attr.type, attr.id);
        shr_id_spod_info_destroy(spod_info);
        ret = 0;
    } else {
        trs_warn("NOT destroy shadow. (devid=%u; sdid=%u; name=%s; type=%d; id=%u; server_abnormal=%u;"
                 " shadow_exist=%u; ret=%d; in_recycle=%d)\n",
                 attr.inst.devid, spod_info->wlist[0].sdid, attr.name, attr.type, attr.id, spod_server_abnormal,
                 shadow_node_exist, ret, ka_base_atomic_read(&spod_info->in_recycling));
        ret = -EPIPE;
    }

    shr_id_node_priv_mutex_unlock(shrid_node);
    shr_id_node_put(shrid_node);
    return ret;
}

/* for pod feature */
STATIC int _shr_id_pod_find_wlist_index(struct shr_id_spod_info *spod_info, u32 sdid, ka_pid_t pid)
{
    int i;

    for (i = 0; i < SHR_ID_PID_POD_MAX_NUM; i++) {
        if ((spod_info->wlist[i].pid == pid) && (spod_info->wlist[i].sdid == sdid)) {
            return i;
        }
    }

    return SHR_ID_PID_POD_MAX_NUM;
}

STATIC int _shr_id_pod_get_idle_wlist_index(struct shr_id_spod_info *spod_info)
{
    int i;

    for (i = 0; i < SHR_ID_PID_POD_MAX_NUM; i++) {
        if (spod_info->wlist[i].pid == 0) {
            return i;
        }
    }
    return SHR_ID_PID_POD_MAX_NUM;
}

STATIC int _shr_id_pod_set_pid(struct shr_id_spod_info *spod_info, u32 sdid, ka_pid_t pid)
{
    int idx = _shr_id_pod_find_wlist_index(spod_info, sdid, pid);
    if (idx != SHR_ID_PID_POD_MAX_NUM) {
        return 0;
    }

    idx = _shr_id_pod_get_idle_wlist_index(spod_info);
    if (idx == SHR_ID_PID_POD_MAX_NUM) {
        return -EBUSY;
    }

    spod_info->wlist[idx].pid = pid;
    spod_info->wlist[idx].sdid = sdid;

    return 0;
}

STATIC void _shr_id_pod_clear_pid(struct shr_id_spod_info *spod_info, u32 sdid, ka_pid_t pid)
{
    int idx = _shr_id_pod_find_wlist_index(spod_info, sdid, pid);
    if (idx != SHR_ID_PID_POD_MAX_NUM) {
        spod_info->wlist[idx].pid = 0;
        spod_info->wlist[idx].sdid = 0;
    }
}

STATIC int shr_id_pod_set_pid(struct shr_id_node_op_attr *attr, u32 serverid, u32 sdid, ka_pid_t pid)
{
    struct shr_id_spod_info *spod_info = NULL;
    void *shrid_node = NULL;
    int ret;

    shrid_node = shr_id_node_get(attr->name, attr->type);
    if (shrid_node == NULL) {
        return -EINVAL;
    }

    shr_id_node_priv_mutex_lock(shrid_node);
    shr_id_node_spin_lock(shrid_node);
    if (!shr_id_node_need_wlist(shrid_node)) {
        shr_id_node_spin_unlock(shrid_node);
        shr_id_node_priv_mutex_unlock(shrid_node);
        shr_id_node_put(shrid_node);
        trs_err("Shr node can't set spod pid.(create_pid=%d; current_pid=%d)\n", attr->pid, pid);
        return -EPERM;
    }
    spod_info = (struct shr_id_spod_info *)shr_id_node_get_priv(shrid_node);
    if (spod_info == NULL) {
        spod_info = shr_id_spod_info_create(shrid_node, attr);
        if (spod_info == NULL) {
#ifndef EMU_ST
            shr_id_node_spin_unlock(shrid_node);
            shr_id_node_priv_mutex_unlock(shrid_node);
            shr_id_node_put(shrid_node);
#endif
            return -ENOMEM;
        }
    }
    shr_id_node_spin_unlock(shrid_node);

    ret = _shr_id_pod_set_pid(spod_info, sdid, pid);
    if (ret != 0) {
#ifndef EMU_ST
        shr_id_node_priv_mutex_unlock(shrid_node);
        shr_id_node_put(shrid_node);
#endif
        return ret;
    }

    trs_debug("Set pod pid. (devid=%u; serverid=%u; sdid=%u; name=%s; type=%d; id=%u; pid=%d)\n", attr->inst.devid,
              serverid, sdid, attr->name, attr->type, attr->id, pid);

    if (spod_info->create_info[serverid].pid == 0) {
        ret = shr_id_create_shadow_node_msg_send(attr, sdid, pid);
        if (ret == 0) {
            spod_info->create_info[serverid].pid = pid;
            spod_info->create_info[serverid].sdid = sdid;
            spod_info->create_info[serverid].server_abnormal = 0;
            spod_info->shadow_node_num++;
        }
    } else {
        ret = shr_id_pod_set_pid_msg_send(attr, sdid, pid);
    }

    if (ret != 0) {
        _shr_id_pod_clear_pid(spod_info, sdid, pid);
    }

    shr_id_node_priv_mutex_unlock(shrid_node);
    shr_id_node_put(shrid_node);
    return ret;
}

STATIC int _shr_id_set_pod_pid(const char *name, ka_pid_t create_pid, u32 serverid, u32 sdid, ka_pid_t pid)
{
    struct shr_id_node_op_attr attr;
    int ret;

    ret = shr_id_node_get_attr(name, &attr);
    if (ret != 0) {
        return ret;
    }

    if (attr.pid != create_pid) {
        /* Only creator process have permission to add pid to wlist */
        return -EACCES;
    }
    if (pid == 0) {
        trs_err("Pid is invalid. (sdid=%u; pid=%u)\n", sdid, pid);
        return -EINVAL;
    }

    return shr_id_pod_set_pid(&attr, serverid, sdid, pid);
}

STATIC bool is_set_local_pod_pid(u32 devid, u32 serverid)
{
    struct spod_info spod = {0};
    int ret;
#ifdef CFG_FEATURE_SDID_STUB
    ret = dbl_get_spod_info_stub(devid, &spod);
#else
    ret = dbl_get_spod_info(devid, &spod);
#endif
    if (ret != 0) {
        return false;
    }

    if (spod.server_id == serverid) {
        trs_debug("Set local pod pid. (local server=%u; serverid=%u)\n", spod.server_id, serverid);
        return true;
    }
    return false;
}
#ifdef CFG_FEATURE_SDID_STUB
/* SDID total 32 bits, low to high: */
#define UDEVID_BIT_LEN 16
#define DIE_ID_BIT_LEN 2
#define CHIP_ID_BIT_LEN 4
#define SERVER_ID_BIT_LEN 10

STATIC inline void parse_bit_shift(unsigned int *rcv, unsigned int *src, int bit_width)
{
    *rcv = (*src) & GENMASK(bit_width - 1, 0);
    *src = (*src) >> bit_width;
}

STATIC int dbl_parse_sdid_stub(unsigned int sdid, struct sdid_parse_info *s)
{
    if (s == NULL) {
        return -EINVAL;
    }

    parse_bit_shift(&s->udevid, &sdid, UDEVID_BIT_LEN);
    parse_bit_shift(&s->die_id, &sdid, DIE_ID_BIT_LEN);
    parse_bit_shift(&s->chip_id, &sdid, CHIP_ID_BIT_LEN);
    parse_bit_shift(&s->server_id, &sdid, SERVER_ID_BIT_LEN);
    return 0;
}
#endif

STATIC int shr_id_set_pod_pid(struct shr_id_proc_ctx *proc_ctx, unsigned long arg)
{
    struct shr_id_pod_pid_ioctl_info ioctl_info;
    struct shr_id_node_op_attr attr;
    struct sdid_parse_info parse = {0};
    int ret;

    if (ka_base_copy_from_user(&ioctl_info, (void *)(uintptr_t)arg, sizeof(struct shr_id_pod_pid_ioctl_info)) != 0) {
        trs_err("Copy from user fail. \n");
        return -EFAULT;
    }

    ret = shr_id_node_get_attr(ioctl_info.name, &attr);
    if (ret != 0) {
        return ret;
    }
#ifdef CFG_FEATURE_SDID_STUB
    ret = dbl_parse_sdid_stub(ioctl_info.sdid, &parse);
#else
    ret = dbl_parse_sdid(ioctl_info.sdid, &parse);
#endif
    if ((ret != 0) || (parse.server_id >= SHR_ID_PID_SERVER_ID_MAX_NUM)) {
        trs_err("Parse fail. (ret=%d; server_id=%u; server id range is [0, %u].)\n", ret, parse.server_id,
                (SHR_ID_PID_SERVER_ID_MAX_NUM - 1));
        return -EINVAL;
    }

    if (is_set_local_pod_pid(attr.inst.devid, parse.server_id) == true) {
        return shr_id_node_set_pids(attr.name, attr.type, proc_ctx->pid, &ioctl_info.pid, 1);
    }

    return _shr_id_set_pod_pid(ioctl_info.name, proc_ctx->pid, parse.server_id, ioctl_info.sdid, ioctl_info.pid);
}

STATIC int shr_id_pod_name_update(u32 devid, char *name)
{
    char tmp_name[SHR_ID_NSM_NAME_SIZE];
    struct spod_info spod = {0};
    int ret, offset;
#ifdef CFG_FEATURE_SDID_STUB
    ret = dbl_get_spod_info_stub(devid, &spod);
#else
    ret = dbl_get_spod_info(devid, &spod);
#endif
    if (ret != 0) {
        return ret;
    }

    offset = snprintf_s(tmp_name, SHR_ID_NSM_NAME_SIZE, SHR_ID_NSM_NAME_SIZE - 1, "%08x", spod.sdid);
    if (offset < 0) {
        trs_err("Snprintf failed. (offset=%d)\n", offset);
        return -EINVAL;
    }
    tmp_name[offset] = '\0';

    ret = strcat_s(name, SHR_ID_NSM_NAME_SIZE, tmp_name);
    if (ret != EOK) {
        trs_err("Strcat failed. (ret=%d)\n", ret);
        return -EINVAL;
    }
    trs_debug("Name update. (devid=%u; name=%s; tmp_name=%s)\n", devid, name, tmp_name);
    return 0;
}

bool hal_kernel_trs_is_belong_to_pod_proc(unsigned int sdid, unsigned int tsid, int pid, int res_type,
                                          unsigned int res_id)
{
    struct trs_id_inst inst = {.devid = sdid, .tsid = tsid};
    return shr_id_is_belong_to_proc(&inst, pid, res_type, res_id);
}
KA_EXPORT_SYMBOL_GPL(hal_kernel_trs_is_belong_to_pod_proc);

STATIC int shr_id_spod_init(void)
{
    struct shr_id_node_ops ops;
    int ret;
#ifdef CFG_FEATURE_SUPPORT_XCOM
    devdrv_register_p2p_msg_proc_ops devdrv_register_p2p_msg_proc_ops_func = NULL;
#endif

    shr_id_register_ioctl_cmd_func(_KA_IOC_NR(SHR_ID_SET_POD_PID), shr_id_set_pod_pid);
#ifdef CFG_FEATURE_SUPPORT_XCOM
    devdrv_p2p_msg_send_func = (devdrv_p2p_msg_send_ops)(uintptr_t)__ka_system_symbol_get(DEVDRV_P2P_MSG_SEND_OPS);
    if (devdrv_p2p_msg_send_func == NULL) {
        trs_warn("Lookup symbol: %s not found.\n", DEVDRV_P2P_MSG_SEND_OPS);
        return DRV_ERROR_NONE;
    }
    devdrv_register_p2p_msg_proc_ops_func = (devdrv_register_p2p_msg_proc_ops)(uintptr_t)__ka_system_symbol_get(
        DEVDRV_REGISTER_P2P_MSG_PROC_OPS);
    if (devdrv_register_p2p_msg_proc_ops_func == NULL) {
        trs_warn("Lookup symbol: %s not found.\n", DEVDRV_REGISTER_P2P_MSG_PROC_OPS);
        return DRV_ERROR_NONE;
    }
    ret = devdrv_register_p2p_msg_proc_ops_func(devdrv_msg_client_tsdrv, trs_s2s_msg_recv);
    __ka_system_symbol_put(DEVDRV_REGISTER_P2P_MSG_PROC_OPS);
#else
    ret = devdrv_register_s2s_msg_proc_func(DEVDRV_S2S_MSG_TRSDRV, trs_s2s_msg_recv);
#endif
    if (ret != 0) {
        return ret;
    }

    ops.destory_node = shr_id_destroy_shadow_node_async;
    ops.name_update = shr_id_pod_name_update;
    shr_id_register_node_ops(&ops);

    trs_info("Spod init success.\n");
    return DRV_ERROR_NONE;
}
DECLAER_FEATURE_AUTO_INIT(shr_id_spod_init, FEATURE_LOADER_STAGE_5);

STATIC void shr_id_spod_uninit(void)
{
#ifdef CFG_FEATURE_SUPPORT_XCOM
    devdrv_unregister_p2p_msg_proc_ops devdrv_unregister_p2p_msg_proc_ops_func = NULL;

    devdrv_unregister_p2p_msg_proc_ops_func = (devdrv_unregister_p2p_msg_proc_ops)(uintptr_t)__ka_system_symbol_get(
        DEVDRV_UNREGISTER_P2P_MSG_PROC_OPS);
    if (devdrv_unregister_p2p_msg_proc_ops_func != NULL) {
        (void)devdrv_unregister_p2p_msg_proc_ops_func(devdrv_msg_client_tsdrv);
        __ka_system_symbol_put(DEVDRV_UNREGISTER_P2P_MSG_PROC_OPS);
    }

    if (devdrv_p2p_msg_send_func != NULL) {
        __ka_system_symbol_put(DEVDRV_P2P_MSG_SEND_OPS);
        devdrv_p2p_msg_send_func = NULL;
    }
#else
    (void)devdrv_unregister_s2s_msg_proc_func(DEVDRV_S2S_MSG_TRSDRV);
#endif

    trs_info("Spod uninit success.\n");
}
DECLAER_FEATURE_AUTO_UNINIT(shr_id_spod_uninit, FEATURE_LOADER_STAGE_5);
