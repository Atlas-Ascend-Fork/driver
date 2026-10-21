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

#include "uda_pub_def.h"
#include "pbl_spod_info.h"
#include "ka_barrier_pub.h"
#include "ka_kernel_def_pub.h"
#include "ka_base_pub.h"
#include "ka_task_pub.h"
#include "ka_memory_pub.h"
#include "pbl/pbl_uda.h"
#include "pbl_mem_alloc_interface.h"
#include "spod_info.h"

struct spod_info g_spod_info[UDA_MAX_PHY_DEV_NUM];
bool g_spod_info_been_set[UDA_MAX_PHY_DEV_NUM];

struct spod_node_status {
    u8 status[DBL_SPOD_MAX_NUM];
};

#ifdef DRV_HOST
static struct spod_node_status *g_spod_node_status[UDA_MAX_PHY_DEV_NUM] = {NULL};
static ka_rw_semaphore_t g_spod_node_status_lock[UDA_MAX_PHY_DEV_NUM];
#endif

int dbl_get_spod_info(unsigned int udevid, struct spod_info *s)
{
    unsigned int udevid_ex = udevid;

    if (udevid == uda_get_host_id()) {
        (void)uda_get_cur_ns_udevids(&udevid_ex, 1);
    }

    if (udevid_ex >= UDA_MAX_PHY_DEV_NUM || s == NULL) {
        return -EINVAL;
    }
    if (!g_spod_info_been_set[udevid_ex]) {
        return -EAGAIN;
    }
    *s = g_spod_info[udevid_ex];
    return 0;
}
KA_EXPORT_SYMBOL(dbl_get_spod_info);

int dbl_set_spod_info(unsigned int udevid, struct spod_info *s)
{
    if (udevid >= UDA_MAX_PHY_DEV_NUM || s == NULL) {
        return -EINVAL;
    }
    g_spod_info[udevid] = *s;
    ka_mb();
    g_spod_info_been_set[udevid] = true;
    return 0;
}
KA_EXPORT_SYMBOL(dbl_set_spod_info);

/* SDID total 32 bits, low to high: */
#define UDEVID_BIT_LEN 16
#define DIE_ID_BIT_LEN 2
#define CHIP_ID_BIT_LEN 4
#define SERVER_ID_BIT_LEN 10

static inline void parse_bit_shift(unsigned int *rcv, unsigned int *src, int bit_width)
{
    *rcv = (*src) & KA_GENMASK(bit_width - 1, 0);
    (*src) >>= bit_width;
}

int dbl_parse_sdid(unsigned int sdid, struct sdid_parse_info *s)
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
KA_EXPORT_SYMBOL(dbl_parse_sdid);

int dbl_make_sdid(struct sdid_parse_info *s, unsigned int *sdid)
{
    unsigned int tmp_sdid;

    if (s == NULL || sdid == NULL) {
        return -EINVAL;
    }

    if (s->udevid >= (1u << UDEVID_BIT_LEN) || s->die_id >= (1u << DIE_ID_BIT_LEN) ||
        s->chip_id >= (1u << CHIP_ID_BIT_LEN) || s->server_id >= (1u << SERVER_ID_BIT_LEN)) {
        return -EINVAL;
    }

    tmp_sdid = s->server_id;

    tmp_sdid <<= CHIP_ID_BIT_LEN;
    tmp_sdid |= s->chip_id;

    tmp_sdid <<= DIE_ID_BIT_LEN;
    tmp_sdid |= s->die_id;

    tmp_sdid <<= UDEVID_BIT_LEN;
    tmp_sdid |= s->udevid;

    *sdid = tmp_sdid;
    return 0;
}
KA_EXPORT_SYMBOL(dbl_make_sdid);

#ifdef DRV_HOST
static int dbl_get_spod_node_status(unsigned int local_udevid, unsigned int index, unsigned int *status)
{
    struct spod_node_status *nodes_status = NULL;

    if (index >= DBL_SPOD_MAX_NUM) {
        uda_err("Invalid index. (index=%u; valid range is [0, %u))\n", index, DBL_SPOD_MAX_NUM);
        return -EINVAL;
    }

    ka_task_down_read(&g_spod_node_status_lock[local_udevid]);

    nodes_status = g_spod_node_status[local_udevid];
    if (nodes_status == NULL) {
        ka_task_up_read(&g_spod_node_status_lock[local_udevid]);
        return -ENODATA;
    }

    *status = nodes_status->status[index];
    ka_task_up_read(&g_spod_node_status_lock[local_udevid]);
    return 0;
}

int dbl_set_spod_node_status(unsigned int local_udevid, unsigned int index, unsigned int status)
{
    struct spod_node_status *nodes_status = NULL;

    if (local_udevid >= UDA_MAX_PHY_DEV_NUM) {
        uda_err("Invalid udevid. (udevid=%u; max=%u)\n", local_udevid, UDA_MAX_PHY_DEV_NUM);
        return -EINVAL;
    }

    if (index >= DBL_SPOD_MAX_NUM) {
        uda_err("Invalid index. (index=%u; valid range is [0, %u))\n", index, DBL_SPOD_MAX_NUM);
        return -EINVAL;
    }

    ka_task_down_write(&g_spod_node_status_lock[local_udevid]);
    nodes_status = g_spod_node_status[local_udevid];
    if (nodes_status == NULL) {
        ka_task_up_write(&g_spod_node_status_lock[local_udevid]);
        uda_err("Spod node status has not been inited. (local_udevid=%u)\n", local_udevid);
        return -ENODATA;
    }

    nodes_status->status[index] = status;
    ka_task_up_write(&g_spod_node_status_lock[local_udevid]);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(dbl_set_spod_node_status);

int dbl_spod_node_status_init(unsigned int udevid)
{
    struct spod_node_status *nodes_status = NULL;

    if (udevid >= UDA_MAX_PHY_DEV_NUM) {
        uda_err("Invalid udevid. (udevid=%u; max=%u)\n", udevid, UDA_MAX_PHY_DEV_NUM);
        return -EINVAL;
    }

    ka_task_down_write(&g_spod_node_status_lock[udevid]);

    if (g_spod_node_status[udevid] != NULL) {
        ka_task_up_write(&g_spod_node_status_lock[udevid]);
        uda_info("Spod node status has been inited. (udevid=%u)", udevid);
        return 0;
    }

    nodes_status = (struct spod_node_status *)dbl_kzalloc(sizeof(struct spod_node_status),
                                                          KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (nodes_status == NULL) {
        ka_task_up_write(&g_spod_node_status_lock[udevid]);
        uda_err("malloc spod node status failed. (udevid=%u)", udevid);
        return -ENOMEM;
    }

    g_spod_node_status[udevid] = nodes_status;
    ka_task_up_write(&g_spod_node_status_lock[udevid]);
    return 0;
}
KA_EXPORT_SYMBOL_GPL(dbl_spod_node_status_init);

void dbl_spod_node_status_uninit(unsigned int udevid)
{
    if (udevid >= UDA_MAX_PHY_DEV_NUM) {
        uda_err("Invalid udevid. (udevid=%u; max=%u)\n", udevid, UDA_MAX_PHY_DEV_NUM);
        return;
    }

    ka_task_down_write(&g_spod_node_status_lock[udevid]);

    if (g_spod_node_status[udevid] == NULL) {
        ka_task_up_write(&g_spod_node_status_lock[udevid]);
        return;
    }

    dbl_kfree(g_spod_node_status[udevid]);
    g_spod_node_status[udevid] = NULL;

    ka_task_up_write(&g_spod_node_status_lock[udevid]);
}
KA_EXPORT_SYMBOL_GPL(dbl_spod_node_status_uninit);

void spod_info_init(void)
{
    int i;
    for (i = 0; i < UDA_MAX_PHY_DEV_NUM; ++i) {
        ka_task_init_rwsem(&g_spod_node_status_lock[i]);
    }
}
#endif

int hal_kernel_get_spod_node_status(unsigned int local_udevid, unsigned int remote_sdid, unsigned int *status)
{
#ifdef DRV_HOST
    int ret = 0;
    struct sdid_parse_info sdid_info = {0};
    int index = 0;
    unsigned int soc_type = SOC_TYPE_MAX;

    if (!uda_is_phy_dev(local_udevid)) {
        return -EOPNOTSUPP;
    }

    if (local_udevid >= UDA_MAX_PHY_DEV_NUM) {
        return -EINVAL;
    }

    if (status == NULL) {
        return -EFAULT;
    }

    ret = dbl_parse_sdid(remote_sdid, &sdid_info);
    if (ret != 0) {
        return ret;
    }

    DBL_SPOD_CHECK_SDID(sdid_info, ret);
    if (ret != 0) {
        return -ERANGE;
    }

    ret = hal_kernel_get_soc_type(local_udevid, &soc_type);
    if (ret != 0) {
        return (ret == -EOPNOTSUPP ? ret : -ENODEV);
    }

    if (soc_type != SOC_TYPE_CLOUD_V3) {
        return -EOPNOTSUPP;
    }

    index = sdid_info.server_id * DBL_SPOD_MAX_UDEVID_NUM + sdid_info.udevid;
    return dbl_get_spod_node_status(local_udevid, index, status);
#else
    (void)local_udevid;
    (void)remote_sdid;
    (void)status;
    return -EOPNOTSUPP;
#endif
}
