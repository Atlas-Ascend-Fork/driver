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
#ifndef _ASDRV_UB_LINK_CHAN_H_
#define _ASDRV_UB_LINK_CHAN_H_

#include "asdrv_ub_dev.h"
#include "asdrv_ub_main_adapt.h"

#define UBDRV_WAIT_LOCAL_DEV_READY_TIME 10      /* 10us */
#define UBDRV_WAIT_LOCAL_DEV_READY_CNT 60000000 /* 60000000 * 10us =  600s*/

struct ubdrv_link_work_mng {
    u32 work_magic;
    ka_delayed_work_t link_work;
};
#define UBDRV_LINK_WORK_DELAY 5000 // ms

int ubdrv_create_single_link_chan(struct ub_idev *idev);
void ubdrv_free_single_link_chan(struct ub_idev *idev);
int ubdrv_send_link_msg(struct ub_idev *idev);
int ubdrv_import_server(struct ub_idev *idev, struct ascend_ub_admin_chan *chan, struct ubcore_eid_info *eid,
                        u32 dev_id);
void ubdrv_unimport_server(struct ascend_ub_admin_chan *chan, u32 dev_id);
int ubdrv_link_chan_send_msg(u32 dev_id, struct ub_idev *idev, struct ascend_ub_user_data *data);
void ubdrv_link_work_sched(void);
void ubdrv_link_init_work(void);
void ubdrv_link_uninit_work(void);
void ubdrv_link_chan_recv_prepare_process(struct ascend_ub_jetty_ctrl *cfg, struct ascend_ub_msg_desc *desc,
                                          u32 seg_id);
void ubdrv_link_init_work_proc(struct ubdrv_link_work_mng *g_link_work);
int ubdrv_link_jetty_cfg_init_proc(struct ub_idev *idev, struct ascend_ub_sync_jetty *jetty, u32 link_token);
int ubdrv_link_jetty_cfg_init(struct ub_idev *idev, struct ascend_ub_sync_jetty *jetty);
void ubdrv_create_link_chan(void);
int ubdrv_dev_online_process(u32 dev_id, u32 remote_devid, struct ub_idev *idev, struct jetty_exchange_data *data);
void ubdrv_dev_offline_process(u32 dev_id);
int ubdrv_set_device_init_status(u32 dev_id);
void ubdrv_set_device_uninit_status(u32 dev_id);
void ubdrv_free_link_chan(void);
struct ubdrv_link_work_mng *get_global_link_work(void);
#endif