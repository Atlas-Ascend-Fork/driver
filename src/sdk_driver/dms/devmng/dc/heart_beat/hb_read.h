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

#ifndef __HB_READ_H
#define __HB_READ_H

#include "ka_task_pub.h"

struct hb_read_block {
    unsigned int dev_id;
    ka_work_struct_t hb_lost_work; /* work for deal with the tasks when heartbeat is lost */
    ka_workqueue_struct_t *hb_lost_wq;
    unsigned int hb_stutas;
    volatile u32 lost_count;
    volatile u64 old_count;
    volatile u64 total_lost_count;
    volatile u64 miss_read_count;
    u64 last_read_time;
    u32 urgent_task_id; /* ub emergency heartbeat registration timer ID for DMS, used to unregister the callback during
                           ko unloading */
    u32 timer_urgent_registered;
    ka_mutex_t mutex;
};

int hb_read_item_work_start(unsigned int dev_id, struct hb_read_block *hb_read_item);
void hb_read_item_work_stop(unsigned int dev_id, struct hb_read_block *hb_read_item);
struct hb_read_block *get_heart_beat_read_item(unsigned int dev_id);
#endif