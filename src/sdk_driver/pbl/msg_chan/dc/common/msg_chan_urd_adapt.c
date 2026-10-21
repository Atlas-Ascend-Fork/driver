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

#include "ka_memory_pub.h"
#include "ka_kernel_def_pub.h"
#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_runenv_config.h"
#include "pbl/pbl_urd_sub_cmd_def.h"

#include "ascend_dev_num.h"
#include "msg_chan_main.h"

int devdrv_get_connect_type_urd(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    int connect_type;

    if ((out == NULL) || (out_len != sizeof(int64_t))) {
        devdrv_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    connect_type = devdrv_get_global_connect_protocol();
    if (connect_type == DEVDRV_COMMNS_TYPE_MAX) {
        devdrv_err("devdrv_get_connect_type failed. Please check device probe status.\n");
        return -EAGAIN;
    }

    if (connect_type == DEVDRV_COMMNS_UB) {
        *(int64_t *)out = CONNECT_PROTOCOL_UB;
    } else {
        *(int64_t *)out = CONNECT_PROTOCOL_PCIE;
    }

    return 0;
}

STATIC int devdrv_get_all_device_count_inner(u32 *count)
{
    int ret = -EOPNOTSUPP;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_warn("Can not get dev_ops.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.get_all_device_count != NULL) {
        ret = dev_ops->ops.get_all_device_count(count);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}

int devdrv_get_device_probe_num_urd(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    bool is_normal_docker;
    u32 count = 0;
    int ret;

    if ((out == NULL) || (out_len != sizeof(u32))) {
        devdrv_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    is_normal_docker = run_in_normal_docker();
    if (is_normal_docker) {
        count = uda_get_cur_ns_dev_num();
        goto docker_out;
    }

    ret = devdrv_get_all_device_count_inner(&count);
    if (ret != 0) {
        devdrv_warn("Can not get all device count. (ret=0x%d)\n", ret);
        return ret;
    }

docker_out:
    *(u32 *)out = count;
    devdrv_debug("Get all device count. (num=%u; is_normal_docker=%d)\n", count, is_normal_docker);

    return 0;
}

STATIC int devdrv_get_device_probe_list_inner(u32 *devids, u32 *count)
{
    int ret = -EOPNOTSUPP;
    struct devdrv_comm_dev_ops *dev_ops = devdrv_add_ops_ref();
    if (dev_ops == NULL) {
        devdrv_err("Get dev_ops fail.\n");
        return -ENODEV;
    }

    if (dev_ops->ops.get_device_probe_list != NULL) {
        ret = dev_ops->ops.get_device_probe_list(devids, count);
    }
    devdrv_sub_ops_ref(dev_ops);
    return ret;
}

int devdrv_get_device_probe_list_urd(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct urd_probe_dev_info *probe_devinfo;
    bool is_normal_docker;
    int ret;

    if (out == NULL || out_len != sizeof(struct urd_probe_dev_info)) {
        devdrv_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    probe_devinfo = (struct urd_probe_dev_info *)ka_mm_kmalloc(sizeof(struct urd_probe_dev_info),
                                                               KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (probe_devinfo == NULL) {
        devdrv_err("No mem. (size=%lu)\n", sizeof(struct urd_probe_dev_info));
        return -EINVAL;
    }

    is_normal_docker = run_in_normal_docker();
    if (is_normal_docker) {
        probe_devinfo->num_dev = uda_get_cur_ns_dev_num();
        ret = uda_get_cur_ns_udevids(probe_devinfo->devids, ASCEND_DEV_MAX_NUM);
        if (ret != 0) {
            devdrv_err("Docker get device probe list failed. (ret=%d)\n", ret);
        } else {
            (void)memcpy_s((void *)out, sizeof(struct urd_probe_dev_info), probe_devinfo,
                           sizeof(struct urd_probe_dev_info));
        }
        goto ret_out;
    }

    ret = devdrv_get_device_probe_list_inner(probe_devinfo->devids, &(probe_devinfo->num_dev));
    if (ret != 0) {
        devdrv_err("Get device probe list inner failed. (ret=%d)\n", ret);
        goto ret_out;
    }

    ret = memcpy_s((void *)out, sizeof(struct urd_probe_dev_info), probe_devinfo, sizeof(struct urd_probe_dev_info));
    if (ret != 0) {
        devdrv_err("copy to user failed. (ret=%d)\n", ret);
        goto ret_out;
    }

ret_out:
    devdrv_debug("Get probe list. (num=%u; ret=%d)\n", probe_devinfo->num_dev, ret);
    ka_mm_kfree(probe_devinfo);
    return ret;
}

int devdrv_get_token_val_urd(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    unsigned int tmp_token_val;
    unsigned int *token_val;
    u32 dev_id; // logical id
    u32 udevid;
    int ret;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        devdrv_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    if ((out == NULL) || (out_len != sizeof(unsigned int))) {
        devdrv_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    token_val = (unsigned int *)out;
    dev_id = *(unsigned int *)in;
    ret = uda_devid_to_udevid(dev_id, &udevid);
    if (ret != 0) {
        devdrv_err("Get udevid failed. (dev_id=%u;udevid=%u;ret=%d)\n", dev_id, udevid, ret);
        return -EINVAL;
    }

    ret = devdrv_get_token_val(udevid, &tmp_token_val);
    if (ret != 0) {
        devdrv_err("Get token_val failed.(ret=%d; dev_id=%u; udevid=%u)\n", ret, dev_id, udevid);
        return ret;
    }

    *token_val = tmp_token_val;
    return 0;
}