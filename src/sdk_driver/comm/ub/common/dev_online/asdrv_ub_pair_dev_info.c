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

#include "ka_hashtable_pub.h"
#include "udma_ctl.h"
#include "pbl/pbl_uda.h"
#include "asdrv_ub_link_chan.h"
#include "asdrv_ub_hotreset.h"
#ifndef CFG_FEATURE_SUPPORT_RUN_INSTALL
#include "asdrv_ub_socket.h"
#endif
#include "asdrv_ub_pair_dev_info.h"

#define PAIR_INFO_HASH_TABLE_BIT 10
#define PAIR_INFO_HASH_TABLE_MASK ((0x1 << PAIR_INFO_HASH_TABLE_BIT) - 1)
KA_TASK_DEFINE_MUTEX(pair_info_mutex);
STATIC KA_DEFINE_HASHTABLE(pair_info_table, PAIR_INFO_HASH_TABLE_BIT);
struct ubdrv_pair_info_work_mng g_pair_info_work = {0};
struct ubdrv_del_dev_work g_del_work = {0};
ka_atomic_t g_pair_msg_version = KA_BASE_ATOMIC_INIT(UBDRV_MSG_VERSION_INVALID);

void ubdrv_set_pair_msg_version(u32 version)
{
    int old;

    old = ka_base_atomic_cmpxchg(&g_pair_msg_version, UBDRV_MSG_VERSION_INVALID, (int)version);
    if ((old != UBDRV_MSG_VERSION_INVALID) && (old != (int)version)) {
        ubdrv_warn("Version has already been set, msg version will not change.(current=%d; new=%u)\n", old, version);
    }
}

u32 ubdrv_get_pair_msg_version(void)
{
    return (u32)ka_base_atomic_read(&g_pair_msg_version);
}

void ubdrv_clear_pair_msg_version(void)
{
    (void)ka_base_atomic_set(&g_pair_msg_version, UBDRV_MSG_VERSION_INVALID);
}

ka_mutex_t *get_global_pair_info_mutex(void)
{
    return &pair_info_mutex;
}

bool ubdrv_check_devid(u32 dev_id)
{
    if ((dev_id >= ASCEND_UB_DEV_MAX_NUM) ||
        ((dev_id >= ASCEND_UB_PF_DEV_MAX_NUM) && (dev_id < ASCEND_UB_VF_DEV_NUM_BASE))) {
        ubdrv_err("Check dev_id fail. (dev_id=%u)\n", dev_id);
        return false;
    }
    return true;
}

struct ubdrv_pair_info_node *ubdrv_pair_info_hash_find(u32 dev_id)
{
    struct ubdrv_pair_info_node *pair_node = NULL;
    u32 key = dev_id & PAIR_INFO_HASH_TABLE_MASK; /* Search the hash table using dev_id as the key value */

    ka_hash_for_each_possible(pair_info_table, pair_node, hnode, key) if (pair_node->dev_id == dev_id)
    {
        return pair_node;
    }

    return NULL;
}

STATIC void ubdrv_pair_info_hash_add(struct ubdrv_pair_info_node *pair_node)
{
    u32 key = pair_node->dev_id & PAIR_INFO_HASH_TABLE_MASK;

    ka_hash_add(pair_info_table, &pair_node->hnode, key);
    return;
}

STATIC void ubdrv_pair_info_hash_del(struct ubdrv_pair_info_node *pair_node)
{
    ka_hash_del(&pair_node->hnode);
    ubdrv_kfree(pair_node);
    return;
}

STATIC int ubdrv_pair_info_hash_count(void)
{
    struct ubdrv_pair_info_node *pair_node = NULL;
    int count = 0;
    u32 bkt = 0;

    ka_hash_for_each(pair_info_table, bkt, pair_node, hnode)
    {
        count++;
    }
    return count;
}

STATIC int ubdrv_pair_info_add(struct pair_dev_info *dev_info)
{
    struct ubdrv_pair_info_node *pair_node = NULL;
    u32 dev_id = dev_info->dev_id;

    if (ubdrv_check_devid(dev_id) == false) {
        return -EINVAL;
    }
    pair_node = ubdrv_pair_info_hash_find(dev_id);
    if (pair_node != NULL) {
        ubdrv_warn("Add pair info was unsuccessful, dev_id already exists. (dev_id=%u)\n", dev_id);
        return -EEXIST;
    }
    pair_node = ubdrv_kzalloc(sizeof(struct ubdrv_pair_info_node), KA_GFP_KERNEL | __KA_GFP_ACCOUNT);
    if (pair_node == NULL) {
        ubdrv_err("Add pair info fail, alloc node mem fail. (dev_id=%u)\n", dev_id);
        return -ENOMEM;
    }
    pair_node->dev_id = dev_id;
    (void)memcpy_s(&pair_node->dev_info, sizeof(struct pair_dev_info), dev_info, sizeof(struct pair_dev_info));
    ubdrv_pair_info_hash_add(pair_node);
    return 0;
}

STATIC void ubdrv_pair_info_clear(void)
{
    struct ubdrv_pair_info_node *pair_node = NULL;
    ka_hlist_node_t *local_hnode = NULL;
    u32 bkt = 0;

    ubdrv_clear_pair_msg_version();
    ka_hash_for_each_safe(pair_info_table, bkt, local_hnode, pair_node, hnode)
    {
        ubdrv_pair_info_hash_del(pair_node);
    }
    return;
}

void ascend_ub_get_min_eid_from_list(u32 *idx, struct ubcore_eid_info *eid_list, u32 eid_list_num)
{
    struct ubcore_eid_info *tmp_eid = &eid_list[0];
    u32 i, idx_tmp = 0;

    for (i = 1; i < eid_list_num; i++) {
        if (ubdrv_cmp_eid(&eid_list[i].eid, &tmp_eid->eid) == UBDRV_CMP_SMALL) {
            tmp_eid = &eid_list[i];
            idx_tmp = i;
        }
    }

    *idx = idx_tmp;

    return;
}

STATIC void ubdrv_init_h2d_phy_flag(u32 dev_id, bool *phy_flag, struct pair_dev_info *dev_info,
                                    struct dev_eid_info *eid_info)
{
    int i;

    for (i = 0; i < dev_info->chan_num; i++) {
        if (ubdrv_cmp_eid(&eid_info->local_eid[eid_info->min_idx].eid, &dev_info->chan_info[i].local_eid) ==
            UBDRV_CMP_EQUAL) {
            if ((dev_info->chan_info[i].flag & UBDRV_VIRTUAL_FLAG) != 0) {
                ubdrv_info("Set phy_flag is false.(flag=%hu;dev_id=%u)\n", dev_info->chan_info[i].flag, dev_id);
                *phy_flag = false;
            } else {
                ubdrv_info("Set phy_flag is true.(flag=%hu;dev_id=%u)\n", dev_info->chan_info[i].flag, dev_id);
                *phy_flag = true;
            }
            return;
        }
    }
    return;
}

STATIC void ubdrv_uninit_h2d_phy_flag(u32 dev_id, bool *phy_flag)
{
    ubdrv_info("Set phy_flag is true.(dev_id=%u)\n", dev_id);
    *phy_flag = true;
    return;
}

STATIC void ubdrv_init_h2d_eid_info(struct pair_dev_info *dev_info)
{
    u32 dev_id = dev_info->dev_id;
    struct dev_eid_info *eid_info;
    struct ascend_dev *asd_dev;
    u32 i, num = 0;

    asd_dev = ubdrv_get_asd_dev_by_devid(dev_id);
    eid_info = &asd_dev->eid_info;
    for (i = 0; i < dev_info->chan_num; i++) {
        if ((dev_info->chan_info[i].flag & UBDRV_H2D_FE_FLAG) != 0) {
            (void)memcpy_s(&eid_info->local_eid[num].eid, sizeof(union ubcore_eid), &dev_info->chan_info[i].local_eid,
                           sizeof(union ubcore_eid));
            (void)memcpy_s(&eid_info->remote_eid[num].eid, sizeof(union ubcore_eid), &dev_info->chan_info[i].remote_eid,
                           sizeof(union ubcore_eid));
            num++;
        }
    }

    eid_info->num = num;
    ascend_ub_get_min_eid_from_list_proc(eid_info);
    ubdrv_init_h2d_phy_flag(dev_id, &asd_dev->phy_flag, dev_info, eid_info);
    return;
}

STATIC void ubdrv_uninit_h2d_eid_info(u32 dev_id)
{
    struct dev_eid_info *eid_info;
    struct ascend_dev *asd_dev;

    asd_dev = ubdrv_get_asd_dev_by_devid(dev_id);
    eid_info = &asd_dev->eid_info;
    ubdrv_uninit_h2d_phy_flag(dev_id, &asd_dev->phy_flag);
    eid_info->num = 0;
    return;
}

int ubdrv_init_h2d_eid_index(u32 dev_id)
{
    struct devdrv_ub_dev_info eid_query_info = {0};
    struct dev_eid_info *eid_info = NULL;
    struct ub_idev *idev = NULL;
    int ret, num;
    u32 min_idx;

    eid_info = ubdrv_get_eid_info_by_devid(dev_id);
    if ((eid_info->num == 0) || (eid_info->min_idx >= eid_info->num)) {
        ubdrv_err("Invalid eid num.(dev_id=%u;num=%u;min_idx=%u)\n", dev_id, eid_info->num, eid_info->min_idx);
        return -EINVAL;
    }
    min_idx = eid_info->min_idx;
    idev = ubdrv_get_idev_by_eid(&eid_info->local_eid[min_idx], UBDRV_INFO_LEVEL);
    if (idev == NULL) {
        ubdrv_warn("Find fe unsuccessful. (dev_id=%u;eid=" EID_FMT ")\n", dev_id,
                   EID_ARGS(eid_info->local_eid[min_idx].eid));
        return -EINVAL;
    }
    ret = ubdrv_get_ub_dev_info(dev_id, &eid_query_info, &num);
    if (ret != 0) {
        return ret;
    }
    ka_task_down_write(&idev->rw_sem);
    idev->eid_index = eid_query_info.eid_index[0];
    idev->valid = ASCEND_UB_VALID; // set valid after eid_index is set
    ka_task_up_write(&idev->rw_sem);
    return 0;
}

void ubdrv_uninit_h2d_eid_index(u32 dev_id)
{
    struct ub_idev *idev = NULL;

    idev = ubdrv_find_idev_by_udevid(dev_id);
    if (idev == NULL) {
        ubdrv_err("Find idev fail. (dev_id=%u)\n", dev_id);
        return;
    }

    ka_task_down_write(&idev->rw_sem);
    idev->valid = ASCEND_UB_INVALID;
    ka_task_up_write(&idev->rw_sem);
    return;
}

bool ubdrv_init_pair_info_h2d_eid_index(void)
{
    struct ubdrv_pair_info_node *pair_node = NULL;
    ka_hlist_node_t *local_hnode = NULL;
    bool is_all_finish = true;
    u32 bkt = 0;
    u32 dev_id;
    int ret;

    ka_task_mutex_lock(&pair_info_mutex);
    ka_hash_for_each_safe(pair_info_table, bkt, local_hnode, pair_node, hnode)
    {
        dev_id = pair_node->dev_info.dev_id;
        if (dev_id < ASCEND_UB_PF_DEV_MAX_NUM) {
            ret = ubdrv_init_h2d_eid_index(dev_id);
            if (ret != 0) {
                is_all_finish = false;
            }
        }
    }
    ka_task_mutex_unlock(&pair_info_mutex);
    return is_all_finish;
}

STATIC void ubdrv_set_all_probe_dev_bitmap(void)
{
    struct ubdrv_pair_info_node *pair_node = NULL;
    ka_hlist_node_t *local_hnode = NULL;
    u32 dev_id;
    u32 bkt = 0;

    ka_hash_for_each_safe(pair_info_table, bkt, local_hnode, pair_node, hnode)
    {
        dev_id = pair_node->dev_id;
        devdrv_set_probe_dev_bitmap(dev_id);
    }
    return;
}

int ubdrv_set_detected_phy_dev_num(int dev_num)
{
    static int set_pdev_flag = 0;
    int ret;
    if (set_pdev_flag == 0) {
        ret = uda_set_detected_phy_dev_num(dev_num);
        if (ret != 0) {
            ubdrv_err("Set detected phy dev num failed. (ret=%d; pdev_count=%u)\n", ret, dev_num);
            return ret;
        } else {
            ubdrv_info("Set detected phy dev num success. (ret=%d; pdev_count=%u)\n", ret, dev_num);
        }
        set_pdev_flag = 1;
    }
    return 0;
}

#ifdef CFG_FEATURE_SUPPORT_RUN_INSTALL

STATIC void ubdrv_uninit_pf_h2d_eid_index(u32 dev_id)
{
    if (dev_id >= ASCEND_UB_PF_DEV_MAX_NUM) {
        return; // vfe does not need to init h2d fe info, when vfe is't init
    }
    ubdrv_uninit_h2d_eid_index(dev_id);
    return;
}

STATIC int ubdrv_get_pair_dev_num(u32 *dev_num)
{
    struct ubcore_user_ctl user_ctl = {0};
    struct ubcore_device *ubc_dev = NULL;
    struct pair_dev_num_info dev_num_info = {0};
    int ret;

    ubc_dev = ubdrv_get_default_user_ctrl_urma_dev();
    if (ubc_dev == NULL) {
        return -ETIMEDOUT; // No mami data, No udma dev, need retry
    }
    user_ctl.in.opcode = UDMA_USER_CTL_QUERY_PAIR_DEVNUM;
    user_ctl.out.addr = (uint64_t)(uintptr_t)(&dev_num_info);
    user_ctl.out.len = sizeof(struct pair_dev_num_info);
    user_ctl.uctx = NULL;
    ret = ubcore_user_control(ubc_dev, &user_ctl);
    if (ret != 0) {
        return ret;
    }

    if ((dev_num_info.pair_device_num == 0) || (dev_num_info.pair_device_num >= ASCEND_UB_DEV_MAX_NUM)) {
        ubdrv_err("Ubcore user ctrl get device num failed. (pair_dev_num=%u)\n", dev_num_info.pair_device_num);
        return -EINVAL;
    }
    ubdrv_info("Ubcore user ctrl get device num. (pair_dev_num=%u)\n", dev_num_info.pair_device_num);
    ubdrv_set_pair_msg_version(dev_num_info.version);
    *dev_num = dev_num_info.pair_device_num;
    return 0;
}

STATIC void ubdrv_little_to_big_endian_convert(union ubcore_eid *eid)
{
    int j = UBCORE_EID_SIZE - 1;
    int i = 0;
    u8 tmp;

    while (i < j) {
        tmp = eid->raw[i];
        eid->raw[i] = eid->raw[j];
        eid->raw[j] = tmp;
        i++;
        j--;
    }
    return;
}

STATIC void ubdrv_dev_info_eid_convert(struct pair_dev_info *dev_info)
{
    u32 chan_num = dev_info->chan_num;
    u32 i;

    ubdrv_little_to_big_endian_convert(&(dev_info->bus_instance_eid));
    ubdrv_little_to_big_endian_convert(&(dev_info->d2d_eid));
    ubdrv_little_to_big_endian_convert(&(dev_info->d2d_eid_crossboard));
    for (i = 0; i < chan_num; i++) {
        ubdrv_little_to_big_endian_convert(&(dev_info->chan_info[i].local_eid));
        ubdrv_little_to_big_endian_convert(&(dev_info->chan_info[i].remote_eid));
    }
    return;
}

STATIC int ubdrv_check_and_convert_pair_dev_info(u32 pair_index, struct pair_dev_info *dev_info)
{
    if ((dev_info->chan_num == 0) || (dev_info->chan_num > PAIR_CHAN_MAX_NUM)) {
        ubdrv_err("User ctrl get chan_num failed. (pair_index=%u;chan_num=%d)\n", pair_index, dev_info->chan_num);
        return -EINVAL;
    }

    dev_info->dev_id = dev_info->module_id;
    if (ubdrv_check_devid(dev_info->dev_id) == false) {
        return -EINVAL;
    }
    ubdrv_dev_info_eid_convert(dev_info);
    return 0;
}

int ubdrv_get_pair_dev_info(u32 pair_index, struct pair_dev_info *dev_info)
{
    struct ubcore_user_ctl user_ctl = {0};
    struct ubcore_device *ubc_dev = NULL;
    struct pair_get_dev_info_input input = {0};
    int ret;

    ubc_dev = ubdrv_get_default_user_ctrl_urma_dev();
    if (ubc_dev == NULL) {
        ubdrv_err("Find urma dev failed.\n");
        return -EINVAL;
    }

    input.index = (u16)pair_index;
    input.version = (u16)ubdrv_get_pair_msg_version();
    user_ctl.in.opcode = UDMA_USER_CTL_GET_DEV_RES_RATIO;
    user_ctl.in.addr = (uint64_t)(uintptr_t)(&input);
    user_ctl.in.len = sizeof(struct pair_get_dev_info_input);
    user_ctl.out.addr = (uint64_t)(uintptr_t)(dev_info);
    user_ctl.out.len = sizeof(struct pair_dev_info);
    user_ctl.uctx = NULL;
    ret = ubcore_user_control(ubc_dev, &user_ctl);
    if (ret != 0) {
        ubdrv_err("User ctrl call failed. (pair_index=%u;ret=%d;in_len=%u;out_len=%u)\n", pair_index, ret,
                  user_ctl.in.len, user_ctl.out.len);
        return ret;
    }

    if (ubdrv_check_and_convert_pair_dev_info(pair_index, dev_info) != 0) {
        return -EINVAL;
    }
    return 0;
}

STATIC bool ubdrv_is_vf_by_devid(u32 dev_id)
{
    if (dev_id >= ASCEND_UB_VF_DEV_NUM_BASE) {
        return true;
    }
    return false;
}

STATIC int ubdrv_pair_info_add_process(struct pair_dev_info *dev_info)
{
    u32 dev_id = dev_info->dev_id;
    int ret = 0;

    ka_task_mutex_lock(&pair_info_mutex);
    ret = ubdrv_pair_info_add(dev_info);
    if (ret != 0) {
        goto unlock_table_add;
    }

    if (ubdrv_is_vf_by_devid(dev_id) == true) {
        devdrv_set_probe_dev_bitmap(dev_id);
    } else {
        ubdrv_init_h2d_eid_info(dev_info);
        devdrv_set_probe_dev_bitmap(dev_id);
        ubdrv_link_work_sched();
    }
unlock_table_add:
    ka_task_mutex_unlock(&pair_info_mutex);
    return ret;
}

STATIC int ubdrv_uda_ctrl_stop(u32 dev_id)
{
    int ret;

    ret = uda_dev_ctrl(dev_id, UDA_CTRL_PRE_HOTRESET);
    if (ret != 0) {
        ubdrv_err("Uda ctrl stop failed. (dev_id=%u;ret=%d)\n", dev_id, ret);
        (void)uda_dev_ctrl(dev_id, UDA_CTRL_PRE_HOTRESET_CANCEL);
        return ret;
    }
    return 0;
}

STATIC int ubdrv_set_dynamic_del_device_status(u32 dev_id)
{
    struct ascend_ub_dev_status *status_mng;
    int status;

    status_mng = ubdrv_get_dev_status_mng(dev_id);
    ka_task_down_write(&status_mng->rw_sem);
    status = status_mng->device_status;
    if (status == UBDRV_DEVICE_ONLINE) {
        status_mng->device_status = UBDRV_DEVICE_BEGIN_OFFLINE;
        ka_task_up_write(&status_mng->rw_sem);
        return 0;
    } else if (status == UBDRV_DEVICE_UNINIT) {
        status_mng->device_status = UBDRV_DEVICE_DEAD;
        ka_task_up_write(&status_mng->rw_sem);
        return 0;
    }
    ka_task_up_write(&status_mng->rw_sem);
    ubdrv_warn("Device is processing link msg, can't be deleted. (dev_id=%u;status=%d)\n", dev_id, status);
    return -EAGAIN;
}

u64 g_del_device_bitmap[ASCEND_UB_DEV_MAX_NUM / BITS_PER_LONG_LONG + 1];

void ubdrv_clr_del_dev_bitmap(u32 devid)
{
    CLR_BIT_64(g_del_device_bitmap[devid / BITS_PER_LONG_LONG], devid % BITS_PER_LONG_LONG);
}

u64 ubdrv_check_del_dev_bitmap(u32 devid)
{
    return CHECK_BIT_64(g_del_device_bitmap[devid / BITS_PER_LONG_LONG], devid % BITS_PER_LONG_LONG);
}

STATIC void ubdrv_dynamic_del_device(ka_work_struct_t *p_work)
{
    struct ubdrv_pair_info_node *pair_node = NULL;
    struct ub_idev *idev = NULL;
    u32 dev_id, i;

    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; i++) {
        if (ubdrv_check_del_dev_bitmap(i) == 0) {
            continue;
        }
        dev_id = i;
        ka_task_mutex_lock(&pair_info_mutex);
        pair_node = ubdrv_pair_info_hash_find(dev_id);
        if (pair_node == NULL) {
            ubdrv_err("Del pair info fail, dev pair info is null. (dev_id=%u)\n", dev_id);
            goto unlock_table_in_del_work;
        }
        if (ubdrv_is_vf_by_devid(dev_id) == true) {
            goto del_pair_info;
        }

        idev = ubdrv_find_idev_by_udevid(dev_id);
        if (idev == NULL) {
            goto unlock_table_in_del_work;
        }
        if (ubdrv_set_dynamic_del_device_status(dev_id) != 0) {
            goto unlock_table_in_del_work;
        }
        if (ubdrv_get_device_status(dev_id) == UBDRV_DEVICE_BEGIN_OFFLINE) {
            if (ubdrv_uda_ctrl_stop(dev_id) != 0) {
                ubdrv_set_device_status(dev_id, UBDRV_DEVICE_ONLINE);
                ubdrv_err("Uda ctrl stop failed. (dev_id=%u)\n", dev_id);
                goto unlock_table_in_del_work;
            }
            ubdrv_set_device_status(dev_id, UBDRV_DEVICE_DEAD);
            ubdrv_remove_davinci_dev(dev_id, UDA_REAL);
            ubdrv_del_msg_device(dev_id, UBDRV_DEVICE_DEAD);
        }
        ubdrv_free_single_link_chan(idev);
        ubdrv_set_device_status(dev_id, UBDRV_DEVICE_UNINIT);

    del_pair_info:
        devdrv_clr_probe_dev_bitmap(dev_id);
        ubdrv_uninit_pf_h2d_eid_index(dev_id);
        ubdrv_uninit_h2d_eid_info(dev_id);
        ubdrv_pair_info_hash_del(pair_node);
    unlock_table_in_del_work:
        ubdrv_clr_del_dev_bitmap(dev_id);
        ka_task_mutex_unlock(&pair_info_mutex);
    }
    return;
}

STATIC int ubdrv_pair_notice_dev_info_check(struct pair_notice_dev_info *data, uint16_t len)
{
    u16 expect_data_len;
    u32 version;

    if ((data == NULL) || (len < sizeof(u32))) {
        ubdrv_err("Data len is too short to read version or data is NULL. (len=%u)\n", len);
        return -EINVAL;
    }

    version = data->version;
    if (version < UBDRV_MAMI_SUPPORT_D2D_EID_EX_VERSION) {
        expect_data_len = (u16)PAIR_NOTICE_DEV_INFO_LEN_V0;
    } else {
        expect_data_len = (u16)sizeof(struct pair_notice_dev_info);
    }

    if (len < expect_data_len) {
        ubdrv_err("Invalid pair info len in notice change data. (len=%u;expect_data_len=%u)\n", len, expect_data_len);
        return -EINVAL;
    }

    return 0;
}

// Serial Callback
STATIC int ubdrv_pair_info_call_process(struct ubcore_device *dev, void *data, uint16_t len)
{
    struct pair_notice_dev_info *new_data = data;
    int ret = -EINVAL;
    u32 dev_id;
    (void)dev;

    ret = ubdrv_pair_notice_dev_info_check(data, len);
    if (ret != 0) {
        return ret;
    }

    dev_id = new_data->notice_dev_info.module_id;
    if (ubdrv_check_and_convert_pair_dev_info(dev_id, &new_data->notice_dev_info) != 0) {
        return -EINVAL;
    }

    ubdrv_set_pair_msg_version(new_data->version);
    (void)ubdrv_set_detected_phy_dev_num(new_data->total_num);

    switch (new_data->op) {
        case UBDRV_PAIR_OP_ADD:
            ret = ubdrv_pair_info_add_process(&new_data->notice_dev_info);
            break;
        case UBDRV_PAIR_OP_DEL:
            ret = ubdrv_pair_info_del_process(&new_data->notice_dev_info, &g_del_work, g_del_device_bitmap);
            break;
        default:
            ubdrv_err("Invalid opcode in process of pair info notice. (op=%u)\n", new_data->op);
            ret = -EINVAL;
            break;
    }
    ubdrv_info("Dev pair info notice info. (dev_id=%u;op=%u;ret=%d;uid=%u)\n", dev_id, new_data->op, ret,
               ka_task_get_current_cred_uid());
    return ret;
}

STATIC int ubdrv_register_pair_info_call(void)
{
    struct ubcore_user_ctl user_ctl = {0};
    struct ubcore_device *ubc_dev = NULL;
    int ret;

    if (g_pair_info_work.pair_call_valid == ASCEND_UB_VALID) {
        return 0;
    }
    ubc_dev = ubdrv_get_default_user_ctrl_urma_dev();
    if (ubc_dev == NULL) {
        ubdrv_err("Register pair info call fail, ubc_dev is null.\n");
        return -ENODEV;
    }
    user_ctl.in.opcode = UDMA_USER_CTL_NPU_REGISTER_INFO_CB;
    user_ctl.in.addr = (uint64_t)(uintptr_t)(&ubdrv_pair_info_call_process);
    user_ctl.in.len = sizeof(uint64_t);
    ret = ubcore_user_control(ubc_dev, &user_ctl);
    if (ret != 0) {
        ubdrv_err("Register pair info call fail. (ret=%d)\n", ret);
        return ret;
    }
    g_pair_info_work.pair_call_valid = ASCEND_UB_VALID;
    ubdrv_info("Register pair info call success.\n");
    return 0;
}

STATIC void ubdrv_unregister_pair_info_call(void)
{
    struct ubcore_user_ctl user_ctl = {0};
    struct ubcore_device *ubc_dev = NULL;
    int ret;

    if (g_pair_info_work.pair_call_valid == ASCEND_UB_INVALID) {
        return;
    }
    ubc_dev = ubdrv_get_default_user_ctrl_urma_dev();
    if (ubc_dev == NULL) {
        ubdrv_err("Register pair info call fail, ubc_dev is null.\n");
        return;
    }
    user_ctl.in.opcode = UDMA_USER_CTL_NPU_UNREGISTER_INFO_CB;
    ret = ubcore_user_control(ubc_dev, &user_ctl);
    if (ret != 0) {
        ubdrv_err("Unregister pair info call fail, user ctrl returned an error code. (ret=%d)\n", ret);
    }
    ubdrv_info("Unregister pair info call success.\n");
    return;
}

int ubdrv_procfs_pair_info_show(ka_seq_file_t *seq, void *offset)
{
    struct ubdrv_pair_info_node *pair_info = NULL;
    struct pair_dev_info *dev_info = NULL;
    struct pair_chan_info *chan_info = NULL;
    u32 dev_id = *((u32 *)seq->private);
    u32 chan_num;
    int ret = 0;
    u32 i;

    ka_task_mutex_lock(&pair_info_mutex);
    pair_info = ubdrv_pair_info_hash_find(dev_id);
    if (pair_info == NULL) {
        ka_fs_seq_printf(seq, "Dev pair info is null.(dev_id=%u)\n", dev_id);
        ret = -ENODEV;
        goto unlock_pair_table;
    }
    dev_info = &pair_info->dev_info;
    chan_num = dev_info->chan_num;
    ka_fs_seq_printf(seq, "-------------------show dev pair info-------------------\n");
    ka_fs_seq_printf(seq, "Dev id info. (dev_id=%u;slot_id=%u;module_id=%u;chan_num=%u)\n", dev_info->dev_id,
                     dev_info->slot_id, dev_info->module_id, dev_info->chan_num);
    ka_fs_seq_printf(seq,
                     "Eid info: \nbus instance eid=" EID_FMT "\nd2d eid=" EID_FMT "\n"
                     "d2d eid cross board=" EID_FMT "\n",
                     EID_ARGS(dev_info->bus_instance_eid), EID_ARGS(dev_info->d2d_eid),
                     EID_ARGS(dev_info->d2d_eid_crossboard));
    for (i = 0; i < chan_num; i++) {
        chan_info = &dev_info->chan_info[i];
        ka_fs_seq_printf(seq, "chan_id=%u;flag=%u;hop=%hu;rsv=%hu\n", i, chan_info->flag, (u16)chan_info->hop,
                         (u16)chan_info->rsv);
        ka_fs_seq_printf(seq, "local_eid: " EID_FMT "\n", EID_ARGS(chan_info->local_eid));
        ka_fs_seq_printf(seq, "remote_eid: " EID_FMT "\n", EID_ARGS(chan_info->remote_eid));
    }
    ka_fs_seq_printf(seq, "---------------------------end--------------------------\n");
unlock_pair_table:
    ka_task_mutex_unlock(&pair_info_mutex);
    return ret;
}
#endif // CFG_FEATURE_SUPPORT_RUN_INSTALL

STATIC void ubdrv_h2d_eid_info_clear(u32 *dev_list, u32 dev_num)
{
    u32 i;

    for (i = 0; i < dev_num; i++) {
        ubdrv_uninit_h2d_eid_info(dev_list[i]);
    }
    return;
}

int ubdrv_get_all_pair_dev_info(void)
{
    struct pair_dev_info dev_info = {0};
    u32 *dev_list;
    u32 dev_num;
    u32 dev_id;
    u32 pf_num = 0;
    int ret;
    u32 i;

    ret = ubdrv_get_pair_dev_num(&dev_num);
    if (ret != 0) {
#ifndef CFG_PLATFORM_FPGA
        ubdrv_warn("User ctrl get dev_num unsuccessful. (ret=%d)\n", ret);
#endif
        return ret;
    }
    dev_list = ubdrv_kzalloc((sizeof(u32) * dev_num), KA_GFP_KERNEL);
    if (dev_list == NULL) {
        ubdrv_err("Alloc dev list mem fail.\n");
        return -ENOMEM;
    }
    ka_task_mutex_lock(&pair_info_mutex);
    for (i = 0; i < dev_num; i++) {
        ret = ubdrv_get_pair_dev_info(i, &dev_info);
        if (ret != 0) {
            ubdrv_err("Get pair dev info fail. (ret=%d;pair_index=%u)\n", ret, i);
            goto get_pair_fail;
        }
        dev_id = dev_info.dev_id;
        ubdrv_init_h2d_eid_info(&dev_info);
        ret = ubdrv_pair_info_add(&dev_info);
        if (ret != 0) {
            ubdrv_uninit_h2d_eid_info(dev_id);
            goto get_pair_fail;
        }
        dev_list[i] = dev_id;
        if (dev_id < ASCEND_UB_PF_DEV_MAX_NUM) {
            pf_num++;
        }
    }
    ubdrv_set_all_probe_dev_bitmap();
    (void)ubdrv_set_detected_phy_dev_num(pf_num);

    ka_task_mutex_unlock(&pair_info_mutex);
    ubdrv_kfree(dev_list);

    return 0;

get_pair_fail:
    ubdrv_h2d_eid_info_clear(dev_list, i);
    ubdrv_pair_info_clear();
    ka_task_mutex_unlock(&pair_info_mutex);
    ubdrv_kfree(dev_list);
    return ret;
}

void ubdrv_put_all_pair_dev_info(void)
{
    ka_task_mutex_lock(&pair_info_mutex);
    ubdrv_pair_info_clear();
    ka_task_mutex_unlock(&pair_info_mutex);
    return;
}

STATIC void ubdrv_pair_info_work_sched(ka_work_struct_t *p_work)
{
    int ret;

#ifdef CFG_FEATURE_SUPPORT_RUN_INSTALL
    ret = ubdrv_register_pair_info_call();
    if ((ret != 0) && (g_pair_info_work.work_magic == UBDRV_WORK_MAGIC)) {
        ka_task_schedule_delayed_work(&g_pair_info_work.pair_info_work,
                                      ka_system_msecs_to_jiffies(UBDRV_PAIR_INFO_WORK_DELAY));
        return;
    }
#endif
    ret = ubdrv_get_all_pair_dev_info();
    if ((ret == -ETIMEDOUT) && (g_pair_info_work.work_magic == UBDRV_WORK_MAGIC)) {
        ka_task_schedule_delayed_work(&g_pair_info_work.pair_info_work,
                                      ka_system_msecs_to_jiffies(UBDRV_PAIR_INFO_WORK_DELAY));
        return;
    }
    g_pair_info_work.work_magic = 0;
    if (ret != 0) {
        ubdrv_warn("Get pair info unsuccessful, wait call notice. (ret=%d)\n", ret);
        return;
    }
    ubdrv_info("Get pair info success.\n");
    ubdrv_link_work_sched();
    return;
}

void ubdrv_pair_info_init_work(struct ub_idev *idev)
{
    if (idev->idev_id != USER_CTRL_DEFAULT_DEV_ID) {
        return;
    }
#ifdef CFG_FEATURE_SUPPORT_RUN_INSTALL
    KA_TASK_INIT_WORK(&g_del_work.work, ubdrv_dynamic_del_device);
    g_pair_info_work.pair_call_valid = ASCEND_UB_INVALID;
#endif
    KA_TASK_INIT_DELAYED_WORK(&g_pair_info_work.pair_info_work, ubdrv_pair_info_work_sched);
    g_pair_info_work.work_magic = UBDRV_WORK_MAGIC;
    ka_task_schedule_delayed_work(&g_pair_info_work.pair_info_work,
                                  ka_system_msecs_to_jiffies(UBDRV_PAIR_INFO_QUERY_DELAY));
    ubdrv_info("Init pair info work success.\n");
}

void ubdrv_pair_info_uninit_work(struct ub_idev *idev)
{
    if (idev->idev_id != USER_CTRL_DEFAULT_DEV_ID) {
        return;
    }
    g_pair_info_work.work_magic = 0;
    (void)ka_task_cancel_delayed_work_sync(&g_pair_info_work.pair_info_work);
#ifdef CFG_FEATURE_SUPPORT_RUN_INSTALL
    ubdrv_unregister_pair_info_call();
    g_pair_info_work.pair_call_valid = ASCEND_UB_INVALID;
    (void)ka_task_cancel_work_sync(&g_del_work.work);
#endif
    ubdrv_info("Uninit pair info work success.\n");
}

int ubdrv_query_h2d_pair_chan_info(u32 dev_id, struct ubcore_eid_info *local_eid, struct ubcore_eid_info *remote_eid)
{
    struct ubdrv_pair_info_node *pair_info_node = NULL;
    size_t len = sizeof(union ubcore_eid);
    struct pair_dev_info *dev_info;
    int ret = -ENODEV;
    u32 chan_num;
    u32 j;

    ka_task_mutex_lock(&pair_info_mutex);
    pair_info_node = ubdrv_pair_info_hash_find(dev_id);
    if (pair_info_node == NULL) {
        ubdrv_err("Can't find pair info by dev_id. (dev_id=%u)\n", dev_id);
        goto unlock_table_in_query;
    }

    dev_info = &pair_info_node->dev_info;
    chan_num = dev_info->chan_num;
    for (j = 0; j < chan_num; j++) {
        if ((dev_info->chan_info[j].flag & UBDRV_H2D_FE_FLAG) != 0) {
            (void)memcpy_s(&local_eid->eid, len, &dev_info->chan_info[j].local_eid, len);
            (void)memcpy_s(&remote_eid->eid, len, &dev_info->chan_info[j].remote_eid, len);
            ret = 0;
            goto unlock_table_in_query;
        }
    }

unlock_table_in_query:
    ka_task_mutex_unlock(&pair_info_mutex);
    return ret;
}

STATIC int ubdrv_get_urma_info_by_eid(struct ubcore_eid_info *local_eid, u32 udevid,
                                      struct ascend_urma_dev_info *urma_info)
{
    struct uda_mia_dev_para mia_para = {0};
    struct udma_ue_info_ex info = {0};
    struct ubcore_user_ctl user_ctl = {0};
    struct ub_idev *idev = NULL;
    int ret = 0;
    u32 vnpu_id, phy_id;

    ret = uda_udevid_to_mia_devid(udevid, &mia_para);
    phy_id = mia_para.phy_devid;
    vnpu_id = mia_para.sub_devid + 1;
    if ((ret != 0) || (vnpu_id >= ASCEND_UDMA_MAX_FE_NUM)) {
        ubdrv_err("Get mia devid from uda failed. (udevid=%u; ret=%d; vnpu_id=%u)\n", udevid, ret, vnpu_id);
        return -EINVAL;
    }

    idev = ubdrv_get_idev_by_eid(local_eid, UBDRV_INFO_LEVEL);
    if (idev == NULL) {
        ubdrv_err("Get idev by eid fail. (dev_id=%u;vnpu_id=%u;ret=%d)\n", phy_id, vnpu_id, ret);
        return -EINVAL;
    }
    user_ctl.in.opcode = UDMA_USER_CTL_QUERY_UE_INFO;
    user_ctl.out.addr = (uint64_t)(uintptr_t)(&info);
    user_ctl.out.len = sizeof(struct udma_ue_info_ex);
    user_ctl.uctx = NULL;
    ret = ubcore_user_control(idev->ubc_dev, &user_ctl);
    if (ret != 0) {
        ubdrv_err("Ubcore get user ctl failed. (idev_id=%u;ret=%d)\n", idev->idev_id, ret);
        return ret;
    }
    urma_info->func_id = info.ue_id;
    urma_info->die_id = info.die_id;
    ubdrv_info("Get urma info success. (udevid=%u; func_id=%u;die_id=%u)\n", udevid, info.ue_id, info.die_id);
    return 0;
}

int ubdrv_get_urma_info(u32 udevid, struct ascend_urma_dev_info *urma_info)
{
    int ret;
    struct ubcore_eid_info local_eid = {0};
    struct ubcore_eid_info remote_eid = {0};

    if (urma_info == NULL) {
        ubdrv_err("Urma info is null. (udevid=%u)\n", udevid);
        return -EINVAL;
    }

    ret = ubdrv_query_h2d_pair_chan_info(udevid, &local_eid, &remote_eid);
    if (ret != 0) {
        return ret;
    }
    ret = ubdrv_get_urma_info_by_eid(&local_eid, udevid, urma_info);
    if (ret != 0) {
        ubdrv_err("Get urma info by eid fail. (ret=%d;udevid=%u)\n", ret, udevid);
        return ret;
    }
    return 0;
}

int ubdrv_query_pair_devinfo_by_remote_eid(u32 *dev_id, struct ubcore_eid_info *local_eid,
                                           struct ubcore_eid_info *remote_eid)
{
    size_t len = sizeof(union ubcore_eid);
    struct dev_eid_info *eid_info;
    u32 chan_num;
    u32 min_idx;
    u32 i;

    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; i++) {
        eid_info = ubdrv_get_eid_info_by_devid(i);
        chan_num = eid_info->num;
        if (chan_num == 0) {
            continue;
        }
        min_idx = eid_info->min_idx;
        if (ubdrv_cmp_eid(&remote_eid->eid, &eid_info->remote_eid[min_idx].eid) == UBDRV_CMP_EQUAL) {
            *dev_id = i;
            (void)memcpy_s(&local_eid->eid, len, &eid_info->local_eid[min_idx].eid, len);
            return 0;
        }
    }
    ubdrv_err("Match eid fail by remote eid info. (remote_eid: " EID_FMT ")(dev_id=%u)\n", EID_ARGS(remote_eid->eid),
              *dev_id);
    return -ENODEV;
}

bool ubdrv_is_valid_devid(u32 dev_id)
{
    bool ret = true;
    struct ubdrv_pair_info_node *pair_node = NULL;

    if (ubdrv_check_devid(dev_id) == false) {
        return false;
    }

    ka_task_mutex_lock(&pair_info_mutex);
    pair_node = ubdrv_pair_info_hash_find(dev_id);
    if (pair_node == NULL) {
        ret = false;
    }
    ka_task_mutex_unlock(&pair_info_mutex);
    return ret;
}

int ubdrv_get_all_device_count(u32 *count)
{
    ka_task_mutex_lock(&pair_info_mutex);
    *count = ubdrv_pair_info_hash_count();
    ka_task_mutex_unlock(&pair_info_mutex);
    return 0;
}

int ubdrv_get_device_probe_list(u32 *devids, u32 *count)
{
    u32 index = 0;
    u32 i;

    for (i = 0; i < ASCEND_UB_DEV_MAX_NUM; i++) {
        devids[i] = UBDRV_INVALID_PHY_ID;
        if (devdrv_check_probe_dev_bitmap(i)) {
            devids[index] = i;
            index++;
        }
    }
    *count = index;
    return 0;
}
