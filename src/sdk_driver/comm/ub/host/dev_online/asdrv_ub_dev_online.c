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
#include "asdrv_ub_link_chan.h"
#include "asdrv_ub_pair_dev_info.h"

STATIC void ubdrv_link_server_work(ka_work_struct_t *p_work)
{
    struct ubdrv_link_work_mng *work_mng;
    bool is_all_finish;
    (void)p_work;

    ubdrv_info("Enter server work.\n");
    is_all_finish = ubdrv_init_pair_info_h2d_eid_index();
    ubdrv_create_link_chan();
    work_mng = get_global_link_work();
    if ((is_all_finish == false) && (work_mng->work_magic == UBDRV_WORK_MAGIC)) {
        ka_task_schedule_delayed_work(&work_mng->link_work, ka_system_msecs_to_jiffies(UBDRV_LINK_WORK_DELAY));
    }
    return;
}

void ubdrv_link_init_work_proc(struct ubdrv_link_work_mng *work_mng)
{
    KA_TASK_INIT_DELAYED_WORK(&work_mng->link_work, ubdrv_link_server_work);
}

#ifdef CFG_FEATURE_SUPPORT_RUN_INSTALL
STATIC void ubdrv_print_pair_info(struct pair_dev_info *dev_info)
{
    struct pair_chan_info *chan_info = NULL;
    u32 chan_num = dev_info->chan_num;
    u32 i;

    ubdrv_info("Get pair info. (dev_id=%u;slot_id=%u;module_id=%u;chan_num=%u)\n", dev_info->dev_id, dev_info->slot_id,
               dev_info->module_id, dev_info->chan_num);
    for (i = 0; i < chan_num; i++) {
        chan_info = &dev_info->chan_info[i];
        ubdrv_info("chan_id=%u;flag=%hu;hop=%hu;rsv=%hu\n", i, chan_info->flag, (u16)chan_info->hop,
                   (u16)chan_info->rsv);
        ubdrv_info("local_eid: " EID_FMT "\n", EID_ARGS(chan_info->local_eid));
        ubdrv_info("remote_eid: " EID_FMT "\n", EID_ARGS(chan_info->remote_eid));
    }
    return;
}

STATIC void ubdrv_set_del_dev_bitmap(u32 devid, u64 *del_device_bitmap)
{
    SET_BIT_64(del_device_bitmap[devid / BITS_PER_LONG_LONG], devid % BITS_PER_LONG_LONG);
}

int ubdrv_pair_info_del_process(struct pair_dev_info *dev_info, struct ubdrv_del_dev_work *del_work,
                                u64 *del_device_bitmap)
{
    struct ubdrv_pair_info_node *pair_node = NULL;
    ka_mutex_t *pair_info_mutex = NULL;
    u32 dev_id = dev_info->dev_id;
    int ret = -ENODEV;

    pair_info_mutex = get_global_pair_info_mutex();
    ka_task_mutex_lock(pair_info_mutex);
    pair_node = ubdrv_pair_info_hash_find(dev_id);
    if (pair_node == NULL) {
        ubdrv_err("Del pair info fail, dev pair info is null. (dev_id=%u)\n", dev_id);
        goto unlock_table_in_del;
    }
    if (ka_base_memcmp(dev_info, &pair_node->dev_info, sizeof(struct pair_dev_info)) != 0) {
        ubdrv_err("Del pair info fail, dev pair info not match. (dev_id=%u)\n", dev_id);
        ubdrv_print_pair_info(dev_info);
        goto unlock_table_in_del;
    }

    ubdrv_set_del_dev_bitmap(dev_id, del_device_bitmap);
    ka_task_mutex_unlock(pair_info_mutex);
    ka_task_schedule_work(&del_work->work);
    return 0;

unlock_table_in_del:
    ka_task_mutex_unlock(pair_info_mutex);
    return ret;
}
#endif

void ascend_ub_get_min_eid_from_list_proc(struct dev_eid_info *eid_info)
{
    ascend_ub_get_min_eid_from_list(&eid_info->min_idx, eid_info->remote_eid, eid_info->num);
}

int ubdrv_link_jetty_cfg_init(struct ub_idev *idev, struct ascend_ub_sync_jetty *jetty)
{
    u32 link_token = DEFAULT_TOKEN_VALUE;
    return ubdrv_link_jetty_cfg_init_proc(idev, jetty, link_token);
}