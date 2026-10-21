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
#include "pbl/pbl_feature_loader.h"
#include "pbl/pbl_uda.h"
#include "pbl/pbl_urd.h"
#include "pbl/pbl_urd_main_cmd_def.h"
#include "pbl/pbl_urd_sub_cmd_def.h"
#include "dms/dms_cmd_def.h"

#include "asdrv_ub_hotreset.h"
#include "asdrv_ub_main.h"
#include "asdrv_ub_urd.h"

int ascend_ub_get_urma_name(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct ascend_ub_msg_dev *msg_dev;
    const char *urma_name;
    unsigned int dev_id; // logical id
    unsigned int udevid;
    int ret = 0;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        ubdrv_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    if (out == NULL) {
        ubdrv_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    dev_id = *(unsigned int *)in;
    ret = uda_devid_to_udevid(dev_id, &udevid);
    if (ret != 0) {
        ubdrv_err("Get udevid failed. (dev_id=%u;udevid=%u;ret=%d)\n", dev_id, udevid, ret);
        return -EINVAL;
    }

    msg_dev = ubdrv_get_msg_dev_by_devid(udevid);
    if ((msg_dev == NULL) || (msg_dev->ubc_dev == NULL)) {
        ubdrv_warn("Msg_dev uninit. (dev_id=%u)\n", udevid);
        return -ENODEV;
    }
    if (ubdrv_add_device_status_ref(udevid) != 0) {
        ubdrv_err("Add device ref failed. (dev_id=%u)\n", udevid);
        return -ENODEV;
    }
    urma_name = msg_dev->ubc_dev->dev_name;
    if (out_len < ka_base_strlen(urma_name) + 1) {
        ubdrv_err("out_len is wrong. (out_len=%u)\n", out_len);
        ret = -EINVAL;
        goto sub_ref;
    }
    ret = strncpy_s(out, out_len, urma_name, ka_base_strlen(urma_name));
    if (ret != 0) {
        ubdrv_err("Strcpy failed. (dev_id=%u;urma_name=%s)\n", udevid, urma_name);
        goto sub_ref;
    }

sub_ref:
    ubdrv_sub_device_status_ref(udevid);
    return ret;
}

// dms_urma_addr_info and ubcore_eid has the same length
STATIC void ubdrv_fill_eid_from_ubcore_to_dms(union ubcore_eid *src_eid, union dms_urma_addr_info *dst_eid)
{
    int i;

    // UBCORE_EID_SIZE = DMS_EID_INFO_LEN
    for (i = 0; i < UBCORE_EID_SIZE; i++) {
        dst_eid->eid[i] = src_eid->raw[i];
    }

    return;
}

int ascend_ub_get_eid_index(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct dev_eid_info *eid_info = NULL;
    struct dms_eid_query_info *query_info = NULL;
    u32 dev_id; // logical id
    u32 udevid, i;
    int ret;

    if ((in == NULL) || (in_len != sizeof(unsigned int))) {
        ubdrv_err("Input argument is null, or in_len is wrong. (in_len=%u)\n", in_len);
        return -EINVAL;
    }
    if ((out == NULL) || (out_len < sizeof(struct dms_eid_query_info))) {
        ubdrv_err("Output argument is null, or out_len is wrong. (out_len=%u)\n", out_len);
        return -EINVAL;
    }

    query_info = (struct dms_eid_query_info *)out;
    dev_id = *(unsigned int *)in;
    ret = uda_devid_to_udevid(dev_id, &udevid);
    if (ret != 0) {
        ubdrv_err("Get udevid failed. (dev_id=%u;udevid=%u;ret=%d)\n", dev_id, udevid, ret);
        return -EINVAL;
    }

    if (udevid >= ASCEND_UB_DEV_MAX_NUM) {
        ubdrv_err("Invalid dev_id, get msg_dev failed. (dev_id=%u)\n", udevid);
        return -EINVAL;
    }

    eid_info = ubdrv_get_eid_info_by_devid(udevid);
    for (i = 0; i < eid_info->num; i++) {
        // dms_urma_addr_info is same with ubcore_eid_info, so do urma_eid_t
        ubdrv_fill_eid_from_ubcore_to_dms(&eid_info->local_eid[i].eid, &query_info->local_eid[i]);
    }
    query_info->num = eid_info->num;
    query_info->min_idx = eid_info->min_idx;
    return 0;
}