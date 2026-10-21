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

#include "ka_kernel_def_pub.h"
#include "securec.h"

#include "pbl_kernel_interface.h"
#include "pbl_uda.h"
#include "pbl_feature_loader.h"

#include "framework_cmd.h"
#include "svm_kern_log.h"
#include "dbi_msg.h"
#include "dbi_ioctl.h"
#include "dbi_core.h"

static int svm_ioctl_dbi_cap_query(void *feature, char *in, u32 in_len, char *out, u32 out_len)
{
    struct svm_dbi_cap_query_para *in_para = (struct svm_dbi_cap_query_para *)in;
    struct svm_dbi_cap_query_para *out_para = (struct svm_dbi_cap_query_para *)out;
    struct svm_device_basic_info dbi = {0};
    u32 udevid;
    u32 copy_size;
    int ret;

    if ((in == NULL) || (in_len < sizeof(*in_para)) || (out == NULL) || (out_len < sizeof(*out_para))) {
        svm_debug("Input param not valid. (in_is_null=%d, in_len=%u, expected=%u, out_is_null=%d, out_len=%u)\n",
                  (in == NULL), in_len, (u32)sizeof(*in_para), (out == NULL), out_len);
        return -EINVAL;
    }
    if (in_para->bitmap_size == 0) {
        svm_debug("Bitmap size not valid. (size=%u)\n", in_para->bitmap_size);
        return -EINVAL;
    }

    ret = uda_devid_to_udevid_ex(in_para->devid, &udevid);
    if (ret != 0) {
        svm_debug("Device id convert not succeed. (devid=%u, ret=%d)\n", in_para->devid, ret);
        return ret;
    }

    ret = dbi_query(udevid, &dbi);
    if (ret != 0) {
        svm_debug("Dbi query not succeed. (udevid=%u, ret=%d)\n", udevid, ret);
        return ret;
    }

    copy_size = (in_para->bitmap_size < (u32)sizeof(svm_dev_cap_t)) ? in_para->bitmap_size : (u32)sizeof(svm_dev_cap_t);
    ret = memcpy_s(out_para->cap_bitmap, copy_size, dbi.cap_flag, copy_size);
    if (ret != 0) {
        return -EINVAL;
    }
    return 0;
}

#define DMS_MODULE_SVM "svm"
BEGIN_DMS_MODULE_DECLARATION(DMS_MODULE_SVM)
BEGIN_FEATURE_COMMAND()
ADD_FEATURE_COMMAND(DMS_MODULE_SVM, DMS_MAIN_CMD_SVM, DMS_SUBCMD_QUERY_SVM_FEATURE, NULL, NULL, DMS_SUPPORT_ALL_USER,
                    svm_ioctl_dbi_cap_query)
END_FEATURE_COMMAND()
END_MODULE_DECLARATION()

static int svm_urd_init(void)
{
    CALL_INIT_MODULE(DMS_MODULE_SVM);
    return 0;
}
DECLAER_FEATURE_AUTO_INIT(svm_urd_init, FEATURE_LOADER_STAGE_1);

static void svm_urd_uninit(void)
{
    CALL_EXIT_MODULE(DMS_MODULE_SVM);
}
DECLAER_FEATURE_AUTO_UNINIT(svm_urd_uninit, FEATURE_LOADER_STAGE_1);
