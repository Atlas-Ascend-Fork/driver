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
#include "ka_kernel_def_pub.h"
#include "securec.h"
#include "ascend_kernel_hal.h"
#include "ascend_platform.h"
#include "ascend_dev_num.h"
#include "soc_resmng_log.h"
#include "pbl/pbl_soc_res.h"
#include "soc_interface_adapt.h"

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#ifdef CFG_FEATURE_PG
STATIC int soc_get_pg_res_max_num(unsigned int dev_id, HAL_PG_INFO_TYPE info_type, enum soc_mia_res_type *soc_res_type,
                                  unsigned int *max_num)
{
    int ret;
    u64 max_aic_num_per_die = 0;

    switch (info_type) {
        case PG_INFO_TYPE_AIC:
            ret = soc_resmng_dev_get_key_value(dev_id, "soc_max_aic_num_per_die", &max_aic_num_per_die);
            if (ret != 0) {
                soc_err("Get max aic num per die failed. (devid=%u; ret=%d)\n", dev_id, ret);
                return ret;
            }
            *max_num = (u32)max_aic_num_per_die;
            *soc_res_type = MIA_AC_AIC;
            return 0;
        case PG_INFO_TYPE_HBM:
            *max_num = SOC_MAX_HBM_NUM_PER_DIE;
            *soc_res_type = MIA_MEM_HBM;
            return 0;
        case PG_INFO_TYPE_MATA:
            *max_num = SOC_MAX_MATA_NUM_PER_DIE;
            *soc_res_type = MIA_SYS_MATA;
            return 0;
        default:
            return -EOPNOTSUPP;
    }
}

STATIC int soc_get_pg_res(unsigned int dev_id, HAL_PG_INFO_TYPE info_type, hal_pg_info_t *pg_info)
{
    unsigned int i, max_num = 0;
    struct soc_mia_res_info_ex res_info = {0};
    enum soc_mia_res_type soc_res_type = {0};
    u64 die_num;
    int ret;

    ret = soc_resmng_dev_get_key_value(dev_id, "soc_die_num", &die_num);
    if (ret != 0) {
        soc_err("Get soc_die_num failed. (dev_id=%u; ret=%d)\n", dev_id, ret);
        return ret;
    }

    ret = soc_get_pg_res_max_num(dev_id, info_type, &soc_res_type, &max_num);
    if (ret != 0) {
        soc_err("Get max num failed. (dev_id=%u; info_type=%u; ret=%d)\n", dev_id, info_type, ret);
        return ret;
    }

    for (i = 0; i < die_num; i++) {
        ret = soc_resmng_dev_die_get_res(dev_id, i, soc_res_type, &res_info);
        if (ret != 0) {
            soc_err("Get res failed. (dev_id=%u; soc_res_type=%u; ret=%d)\n", dev_id, soc_res_type, ret);
            return ret;
        }

        pg_info->bitmap |= res_info.bitmap << (i * max_num);
        pg_info->num += res_info.total_num;
        pg_info->freq = res_info.freq;
    }

    return 0;
}
#endif

int hal_kernel_get_pg_info_adapt(unsigned int dev_id, HAL_PG_INFO_TYPE info_type, char *data, unsigned int size,
                                 unsigned int *ret_size)
{
#ifdef CFG_FEATURE_PG
    hal_pg_info_t pg_info = {0};
    int ret;

    if ((dev_id >= ASCEND_DEV_MAX_NUM) || (data == NULL) || (ret_size == NULL) || (info_type >= PG_INFO_TYPE_MAX)) {
        soc_err("Input para is invalid. (dev_id=%u; data_is_null=%d; ret_size_is_null=%d; info_type=%d)\n", dev_id,
                (data == NULL), (ret_size == NULL), info_type);
        return -EINVAL;
    }

    ret = soc_get_pg_res(dev_id, info_type, &pg_info);
    if (ret != 0) {
        soc_err("Get pg info failed. (dev_id=%u; info_type=%u; ret=%d)\n", dev_id, info_type, ret);
        return ret;
    }
    *ret_size = sizeof(hal_pg_info_t);

    if (*ret_size > size) {
        soc_err("Return size is larger than input data size. (dev_id=%u; size=%u; ret_size=%u)\n", dev_id, size,
                *ret_size);
        return -EINVAL;
    }

    *(hal_pg_info_t *)data = pg_info;
    return 0;
#else
    return -EOPNOTSUPP;
#endif
}