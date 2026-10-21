/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include "ka_kernel_def_pub.h"

#include "dpa/dpa_apm_kernel.h"

#include "apm_device_slave_proxy_domain.h"
#include "apm_master_domain.h"
#include "apm_msg.h"
#include "apm_host_proxy.h"

static int apm_host_proxy_query_from_device(u32 udevid, int slave_pid, int *master_pid)
{
    struct apm_msg_query_master msg;
    int ret;

    apm_msg_fill_header(&msg.header, APM_MSG_TYPE_QUERY_MASTER);
    msg.udevid = udevid;
    msg.slave_tgid = slave_pid;

    ret = apm_msg_send(udevid, &msg.header, sizeof(msg));
    if (ret == 0) {
        *master_pid = msg.master_tgid;
    }

    return ret;
}

int devdrv_query_master_pid_by_device_slave(u32 udevid, int slave_pid, u32 *master_pid)
{
    int ret;

    if (master_pid == NULL) {
        apm_err("Null ptr. (slave_pid=%d)\n", slave_pid);
        return -EINVAL;
    }

    if (udevid >= uda_get_udev_max_num()) {
        apm_err("Invalid para. (slave_pid=%d; udevid=%u)\n", slave_pid, udevid);
        return -EINVAL;
    }

    ret = apm_query_master_tgid_by_device_slave(udevid, slave_pid, (int *)master_pid);
    if ((ret != 0) && (ka_base_in_atomic() == false)) {
        ret = apm_host_proxy_query_from_device(udevid, slave_pid, (int *)master_pid);
    }

    return ret;
}
KA_EXPORT_SYMBOL_GPL(devdrv_query_master_pid_by_device_slave);

static int apm_host_proxy_msg_bind(int master_tgid, int slave_tgid, struct apm_cmd_bind *para)
{
    int ret;

    ret = apm_device_slave_proxy_domain_bind(slave_tgid, master_tgid, para);
    if (ret != 0) {
        apm_err("Slave bind failed. (ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n", ret,
                para->devid, para->proc_type, para->mode, para->slave_pid, para->master_pid);
        return ret;
    }

    ret = apm_master_domain_add_slave(para, master_tgid, slave_tgid);
    if (ret != 0) {
        (void)apm_device_slave_proxy_domain_unbind(slave_tgid, para);
        /* The master may have exited. To avoid residual ctx of the slave, destroy the slave ctx. */
        apm_device_slave_proxy_domain_task_exit(para->devid, slave_tgid);
        apm_err("Master bind failed. (ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n", ret,
                para->devid, para->proc_type, para->mode, para->slave_pid, para->master_pid);
        return ret;
    }

    return 0;
}

static int apm_host_proxy_msg_unbind(int master_tgid, int slave_tgid, struct apm_cmd_bind *para)
{
    int ret;

    ret = apm_master_domain_pre_unbind(master_tgid, para);
    if (ret != 0) {
        apm_warn("Master pre unbind warn. (ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n",
                 ret, para->devid, para->proc_type, para->mode, para->slave_pid, para->master_pid);
        return ret;
    }

    ret = apm_master_domain_del_slave(para, master_tgid, slave_tgid);
    if (ret != 0) { /* when fails, release other resources to avoid residual resources. */
        apm_warn("Master unbind warn. (ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n", ret,
                 para->devid, para->proc_type, para->mode, para->slave_pid, para->master_pid);
    }

    ret = apm_device_slave_proxy_domain_unbind(slave_tgid, para);
    if (ret != 0) {
        apm_warn("Slave unbind warn. (ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n", ret,
                 para->devid, para->proc_type, para->mode, para->slave_pid, para->master_pid);
    }

    return 0;
}

static int apm_host_proxy_msg_set_slave_status(u32 udevid, struct apm_msg_header *header)
{
    struct apm_msg_set_slave_status *msg = (struct apm_msg_set_slave_status *)(void *)header;
    int ret;

    ret = apm_master_domain_set_slave_status(msg->master_tgid, udevid, msg->slave_tgid, msg->type, msg->status, 0);
    if (ret != 0) {
        return ret;
    }

    (void)apm_device_slave_proxy_domain_set_slave_status(udevid, msg->slave_tgid, msg->type, msg->status);
    return 0;
}

static int apm_host_proxy_msg_query_tast_group_exit_stage(u32 udevid, struct apm_msg_header *header)
{
    struct apm_msg_query_task_group_exit_stage *msg = (struct apm_msg_query_task_group_exit_stage *)(void *)header;

    return apm_master_domain_get_tast_group_exit_stage(msg->master_tgid, msg->slave_tgid, udevid, msg->proc_type_bitmap,
                                                       &msg->exit_stage);
}

static int apm_host_proxy_msg_slave_destroy(u32 udevid, struct apm_msg_header *header)
{
    struct apm_msg_slave_destroy *msg = (struct apm_msg_slave_destroy *)(void *)header;

    apm_device_slave_proxy_domain_task_exit(udevid, msg->tgid);
    return 0;
}

static int apm_host_proxy_msg_handle(u32 udevid, struct apm_msg_header *header)
{
    struct apm_msg_bind_unbind *msg = (struct apm_msg_bind_unbind *)(void *)header;
    struct apm_task_group_cfg *cfg = &msg->cfg;
    struct apm_cmd_bind *para = &cfg->para;
    int ret;

    /* Refresh to local udevid */
    para->devid = udevid;

    /* host store device slave tgid */
    para->slave_pid = cfg->slave_tgid;

    /* master is in local, tgid trans to vpid */
    ret = apm_master_domain_tgid_to_pid(cfg->master_tgid, &para->master_pid);
    if (ret != 0) {
        apm_warn("Master tgid to pid warn. (ret=%d; devid=%u; proc_type=%d; mode=%d; slave_pid=%d; master_pid=%d)\n",
                 ret, para->devid, para->proc_type, para->mode, para->slave_pid, para->master_pid);
        return ret;
    }

    if (header->msg_type == APM_MSG_TYPE_BIND) {
        ret = apm_host_proxy_msg_bind(cfg->master_tgid, cfg->slave_tgid, para);
    } else {
        ret = apm_host_proxy_msg_unbind(cfg->master_tgid, cfg->slave_tgid, para);
    }

    return ret;
}

static int apm_host_proxy_msg_query_master(u32 udevid, struct apm_msg_header *header)
{
    struct apm_msg_query_master *msg = (struct apm_msg_query_master *)(void *)header;

    return apm_query_master_tgid_by_slave(msg->slave_tgid, &msg->master_tgid);
}

static int apm_host_proxy_notice_device_bind_unbind(enum apm_msg_type msg_type, struct apm_task_group_cfg *cfg)
{
    struct apm_msg_bind_unbind msg;

    apm_msg_fill_header(&msg.header, msg_type);
    msg.cfg = *cfg;

    return apm_msg_send(cfg->para.devid, &msg.header, sizeof(msg));
}

static int apm_master_proxy_bind_unbind(struct master_ctx *m_ctx, enum apm_msg_type msg_type, int master_tgid,
                                        int slave_tgid, struct apm_cmd_bind *para)
{
    struct apm_task_group_cfg cfg;
    int ret;

    apm_fill_task_group_cfg(&cfg, master_tgid, slave_tgid, para);

    if (para->devid == UDA_INVALID_UDEVID) {
        u32 i;
        for (i = 0; i < uda_get_udev_max_num(); i++) {
            if (m_ctx->dev_ctx[i].valid == 0) {
                continue;
            }

            cfg.para.devid = i;
            ret = apm_host_proxy_notice_device_bind_unbind(msg_type, &cfg);
            if (ret != 0) {
                apm_warn("Notice failed. (ret=%d; udevid=%u; msg=%d; slave_pid=%d; master_pid=%d)\n", ret, i, msg_type,
                         para->slave_pid, para->master_pid);
            }
        }
        return 0;
    } else {
        return apm_host_proxy_notice_device_bind_unbind(msg_type, &cfg);
    }
}

static int apm_master_proxy_bind(struct master_ctx *m_ctx, int master_tgid, int slave_tgid, struct apm_cmd_bind *para)
{
    return apm_master_proxy_bind_unbind(m_ctx, APM_MSG_TYPE_BIND, master_tgid, slave_tgid, para);
}

static int apm_master_proxy_unbind(struct master_ctx *m_ctx, int master_tgid, int slave_tgid, struct apm_cmd_bind *para)
{
    return apm_master_proxy_bind_unbind(m_ctx, APM_MSG_TYPE_UNBIND, master_tgid, slave_tgid, para);
}

static int apm_host_proxy_notice_device_destroy(int tgid, u32 udevid)
{
    struct apm_msg_master_destroy msg;

    apm_msg_fill_header(&msg.header, APM_MSG_TYPE_MASTER_DESTROY);
    msg.tgid = tgid;

    return apm_msg_send(udevid, &msg.header, sizeof(msg));
}

static void apm_master_proxy_destroy(struct master_ctx *m_ctx, int tgid)
{
    u32 i;

    apm_device_slave_proxy_domain_master_exit(tgid);

    for (i = 0; i < uda_get_udev_max_num(); i++) {
        int ret;

        if (m_ctx->dev_ctx[i].valid == 0) {
            continue;
        }

        ret = apm_host_proxy_notice_device_destroy(tgid, i);
        if (ret != 0) {
            apm_warn("Notice warn. (ret=%d; tgid=%d; udevid=%d)\n", ret, tgid, i);
        }
    }
}

static int apm_master_proxy_query_slave_meminfo(u32 udevid, int slave_tgid, processType_t process_type,
                                                processMemType_t type, u64 *size)
{
    struct apm_msg_query_slave_meminfo msg = {0};
    int ret;

    if ((type == PROC_MEM_TYPE_ALL) && (process_type != PROCESS_CP1)) {
        return -EOPNOTSUPP;
    }

    apm_msg_fill_header(&msg.header, APM_MSG_TYPE_QUERY_SLAVE_MEMINFO);
    msg.udevid = udevid;
    msg.slave_tgid = slave_tgid;
    msg.type = (u32)type;

    ret = apm_msg_send(udevid, &msg.header, sizeof(msg));
    if (ret == 0) {
        *size = msg.size;
    }

    return ret;
}

static struct apm_master_domain_ops proxy_master_ops = {
    .destroy = apm_master_proxy_destroy,
    .bind = apm_master_proxy_bind,
    .unbind = apm_master_proxy_unbind,
};

static struct apm_master_domain_cmd_ops master_domain_cmd_ops = {
    .query_meminfo = apm_master_proxy_query_slave_meminfo,
};

int apm_host_proxy_init(void)
{
    apm_register_msg_handle(APM_MSG_TYPE_SLAVE_DESTROY, (u32)sizeof(struct apm_msg_slave_destroy),
                            apm_host_proxy_msg_slave_destroy);
    apm_register_msg_handle(APM_MSG_TYPE_BIND, (u32)sizeof(struct apm_msg_bind_unbind), apm_host_proxy_msg_handle);
    apm_register_msg_handle(APM_MSG_TYPE_UNBIND, (u32)sizeof(struct apm_msg_bind_unbind), apm_host_proxy_msg_handle);
    apm_register_msg_handle(APM_MSG_TYPE_QUERY_MASTER, (u32)sizeof(struct apm_msg_query_master),
                            apm_host_proxy_msg_query_master);
    apm_register_msg_handle(APM_MSG_TYPE_SET_SLAVE_STATUS, (u32)sizeof(struct apm_msg_set_slave_status),
                            apm_host_proxy_msg_set_slave_status);
    apm_register_msg_handle(APM_MSG_TYPE_QUERY_TASK_GROUP_EXIT_STAGE,
                            (u32)sizeof(struct apm_msg_query_task_group_exit_stage),
                            apm_host_proxy_msg_query_tast_group_exit_stage);

    apm_master_domain_ops_register(&proxy_master_ops);
    apm_master_domain_cmd_ops_register(&master_domain_cmd_ops);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(apm_host_proxy_init, FEATURE_LOADER_STAGE_3);

void apm_host_proxy_uninit(void)
{}
DECLAER_FEATURE_AUTO_UNINIT(apm_host_proxy_uninit, FEATURE_LOADER_STAGE_3);
