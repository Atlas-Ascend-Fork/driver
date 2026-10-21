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
#include "ka_common_pub.h"
#include "ka_compiler_pub.h"
#include "ka_ioctl_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_task_pub.h"

#include "apm_slave.h"
#include "apm_slave_domain.h"
#include "apm_slab.h"
#include "apm_device_slave_proxy_domain.h"

#define APM_DOMAIN_NAME_LEN 64

static struct task_ctx_domain **device_slave_proxy_domain = NULL;
static ka_mutex_t proxy_domain_mutex;

int apm_query_master_tgid_by_device_slave(u32 udevid, int slave_tgid, int *master_tgid)
{
    struct apm_cmd_query_master_info para;
    int ret;

    if (device_slave_proxy_domain[udevid] == NULL) {
        apm_err("Invalid para. (slave_tgid=%d; udevid=%u)\n", slave_tgid, udevid);
        return -EINVAL;
    }

    ret = apm_slave_query_master(device_slave_proxy_domain[udevid], slave_tgid, &para);
    if (ret == 0) {
        *master_tgid = para.master_tgid;
    }

    return ret;
}

static int apm_device_slave_proxy_domain_create(u32 udevid)
{
    char *domain_name = NULL;

    domain_name = (char *)apm_vzalloc(APM_DOMAIN_NAME_LEN);
    if (domain_name == NULL) {
#ifndef EMU_ST
        apm_err("Vmalloc domain name failed. (size=%d)\n", APM_DOMAIN_NAME_LEN);
        return -ENOMEM;
#endif
    }

    (void)sprintf_s(domain_name, APM_DOMAIN_NAME_LEN, "apm_device_%u_slave_proxy", udevid);
    device_slave_proxy_domain[udevid] = task_ctx_domain_create(domain_name, 0);
    if (device_slave_proxy_domain[udevid] == NULL) {
        apm_vfree(domain_name);
        apm_err("Proxy domain create failed. (udevid=%u)\n", udevid);
        return -ENOMEM;
    }
    return 0;
}

static void apm_device_slave_proxy_domain_destory(u32 udevid)
{
    const char *domain_name = device_slave_proxy_domain[udevid]->name;
    task_ctx_domain_destroy(device_slave_proxy_domain[udevid]);
    device_slave_proxy_domain[udevid] = NULL;
    if (domain_name != NULL) {
        apm_vfree(domain_name);
    }
}

static struct task_ctx_domain *apm_get_device_slave_proxy_domain(u32 udevid)
{
    struct task_ctx_domain *domain = NULL;
    int ret;

    if (device_slave_proxy_domain[udevid] != NULL) {
        return device_slave_proxy_domain[udevid];
    }

    ka_task_mutex_lock(&proxy_domain_mutex);
    domain = device_slave_proxy_domain[udevid];
    if (domain == NULL) {
        ret = apm_device_slave_proxy_domain_create(udevid);
        if (ret == 0) {
            domain = device_slave_proxy_domain[udevid];
        }
    }
    ka_task_mutex_unlock(&proxy_domain_mutex);

    return domain;
}

int apm_device_slave_proxy_domain_bind(int slave_tgid, int master_tgid, struct apm_cmd_bind *para)
{
    struct task_ctx_domain *domain = NULL;
    int ret;

    domain = apm_get_device_slave_proxy_domain(para->devid);
    if (domain == NULL) {
        apm_err("Get domain failed. (slave_pid=%d)\n", para->slave_pid);
        return -EFAULT;
    }

    ret = apm_slave_ctx_create(domain, slave_tgid, para->slave_pid, NULL);
    if (ret != 0) {
        apm_err("Create slave ctx failed. (slave_pid=%d)\n", para->slave_pid);
        return ret;
    }

    ret = apm_slave_add_master(domain, slave_tgid, master_tgid, para);
    if (ret != 0) {
        apm_err("Add master failed. (slave_pid=%d; master_pid=%d)\n", para->slave_pid, para->master_pid);
        return ret;
    }

    return 0;
}

int apm_device_slave_proxy_domain_unbind(int slave_tgid, struct apm_cmd_bind *para)
{
    struct task_ctx_domain *domain = NULL;
    int ret;

    domain = apm_get_device_slave_proxy_domain(para->devid);
    if (domain == NULL) {
        apm_err("Get domain failed. (slave_pid=%d)\n", para->slave_pid);
        return -EFAULT;
    }

    ret = apm_slave_del_master(domain, slave_tgid, para);
    if (ret != 0) {
        apm_warn("Del master. (slave_pid=%d; master_pid=%d; ret=%d)\n", para->slave_pid, para->master_pid, ret);
        return ret;
    }

    return 0;
}

int apm_device_slave_proxy_domain_set_slave_status(u32 udevid, int slave_tgid, int status_type, int status)
{
    struct task_ctx_domain *domain = device_slave_proxy_domain[udevid];

    if (domain == NULL) {
        apm_err("No device domain. (udevid=%d)\n", udevid);
        return -EFAULT;
    }

    switch (status_type) {
        case SLAVE_STATUS_EXIT:
            (void)apm_slave_check_set_exit_status(domain, slave_tgid, NULL, APM_SLAVE_CHECK_EXIT_FROM_APM);
            break;
        case SLAVE_STATUS_OOM:
        default:
            break;
    }

    return 0;
}

static int apm_device_slave_proxy_domain_fops_query_master_pid(u32 cmd, unsigned long arg)
{
    struct apm_cmd_query_master_info *usr_arg = (struct apm_cmd_query_master_info __ka_user *)arg;
    struct apm_cmd_query_master_info para;
    struct task_ctx_domain *domain = NULL;
    int ret, tgid;
    u32 devid;

    ret = (int)ka_base_copy_from_user(&para, usr_arg, sizeof(para));
    if (ret != 0) {
        apm_err("Copy from user failed. (ret=%d)\n", ret);
        return ret;
    }

    devid = para.udevid;
    ret = uda_devid_to_udevid(devid, &para.udevid);
    if (ret != 0) {
        apm_err("Get udevid failed. (ret=%d; devid=%d)\n", ret, devid);
        return ret;
    }

    domain = device_slave_proxy_domain[para.udevid];
    if (domain == NULL) {
        apm_err("No device domain. (udevid=%d)\n", para.udevid);
        return -EFAULT;
    }

    tgid = para.slave_pid;
    ret = apm_slave_query_master_self_exit_check(domain, tgid, &para);
    if (ret != 0) {
        apm_info("Query completed. (ret=%d; slave_pid=%d)\n", ret, para.slave_pid);
        return ret;
    }

    return (int)ka_base_copy_to_user(usr_arg, &para, sizeof(para));
}

void apm_device_slave_proxy_domain_task_exit(u32 udevid, int tgid)
{
    if (device_slave_proxy_domain[udevid] != NULL) {
        apm_slave_ctx_destroy(device_slave_proxy_domain[udevid], tgid);
    }
}

void apm_device_slave_proxy_domain_master_exit(int master_tgid)
{
    u32 i;

    for (i = 0; i < uda_get_udev_max_num(); i++) {
        if (device_slave_proxy_domain[i] != NULL) {
            apm_info("Try destroy. (udevid=%u; master_tgid=%d)\n", i, master_tgid);
            apm_slave_ctx_destroy_by_master_tgid(device_slave_proxy_domain[i], master_tgid);
        }
    }
}

void apm_device_slave_proxy_domain_task_show(u32 udevid, int tgid, int feature_id, ka_seq_file_t *seq)
{
    u32 i;

    for (i = 0; i < uda_get_udev_max_num(); i++) {
        if (device_slave_proxy_domain[i] != NULL) {
            if (tgid != 0) {
                apm_slave_ctx_show(device_slave_proxy_domain[i], tgid, seq);
            } else {
                task_ctx_domain_show(device_slave_proxy_domain[i], seq);
            }
        }
    }
}
DECLAER_FEATURE_AUTO_SHOW_TASK(apm_device_slave_proxy_domain_task_show, FEATURE_LOADER_STAGE_3);

int apm_device_slave_proxy_domain_init(void)
{
    u32 max_dev_num = uda_get_udev_max_num();

    device_slave_proxy_domain = apm_vzalloc(sizeof(struct task_ctx_domain *) * max_dev_num);
    if (device_slave_proxy_domain == NULL) {
        apm_err("Malloc proxy domain failed. (max_dev_num=%u; size=%ld)\n", max_dev_num,
                sizeof(struct task_ctx_domain *) * max_dev_num);
        return -ENOMEM;
    }

    ka_task_mutex_init(&proxy_domain_mutex);

    apm_register_ioctl_cmd_func(_KA_IOC_NR(APM_QUERY_MASTER_INFO_BY_DEVICE_SLAVE),
                                apm_device_slave_proxy_domain_fops_query_master_pid);

    return 0;
}
DECLAER_FEATURE_AUTO_INIT(apm_device_slave_proxy_domain_init, FEATURE_LOADER_STAGE_3);

void apm_device_slave_proxy_domain_uninit(void)
{
    u32 i;

    for (i = 0; i < uda_get_udev_max_num(); i++) {
        if (device_slave_proxy_domain[i] != NULL) {
            apm_device_slave_proxy_domain_destory(i);
        }
    }

    apm_vfree(device_slave_proxy_domain);
    device_slave_proxy_domain = NULL;
}
DECLAER_FEATURE_AUTO_UNINIT(apm_device_slave_proxy_domain_uninit, FEATURE_LOADER_STAGE_3);
