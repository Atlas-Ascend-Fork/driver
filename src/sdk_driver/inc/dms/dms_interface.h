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
#ifndef __DMS_INTERFACE_H__
#define __DMS_INTERFACE_H__

#include "fms/fms_dtm.h"

/************************************* sensor interface ********************************************************/

/* generation threshold and recovery threshold types for statistical sensors */
#define DMS_SENSOR_THRES_TYPE_PERIOD 0    /* threshold type: periodic statistics type */
#define DMS_SENSOR_THRES_TYPE_CONTINUED 1 /* threshold type: continuous detection type */
#define DMS_SENSOR_OCCUR_THRES_TYPE_PERIOD \
    DMS_SENSOR_THRES_TYPE_PERIOD /* threshold generation type: periodic statistics */
#define DMS_SENSOR_OCCUR_THRES_TYPE_CONTINUED \
    DMS_SENSOR_THRES_TYPE_CONTINUED /* threshold generation type: continuous detection type */
#define DMS_SENSOR_RESUME_THRES_TYPE_PERIOD \
    DMS_SENSOR_THRES_TYPE_PERIOD /* recovery threshold type: periodic statistics type */
#define DMS_SENSOR_RESUME_THRES_TYPE_CONTINUED \
    DMS_SENSOR_THRES_TYPE_CONTINUED /* recovery threshold type: continuous detection type */

/* sensor classification definition */
#define DMS_STATICSTIC_SENSOR_CLASS 0x70        /* statistical sensor */
#define DMS_GENERAL_THRESHOLD_SENSOR_CLASS 0x01 /* general threshold sensors */
#define DMS_DISCRETE_SENSOR_CLASS 0x6f          /* discrete sensor */
#define DMS_PER_SENSOR_CLASS 0x71               /* performance statistics type sensor */

/* performance statistics type sensor enable flag */
#define DMS_SENSOR_ENABLE_FALG 1  /* enable */
#define DMS_SENSOR_DISABLE_FALG 0 /* disable */

#define DMS_SENSOR_CHECK_TIMER_LEN 100                            /* sensor detection timer duration, 100 ms */
#define DMS_SENSOR_CHECK_INTERVAL_TIME DMS_SENSOR_CHECK_TIMER_LEN /* sensor detection timer duration */

/* max length of additional data */
#define DMS_MAX_EVENT_NAME_LENGTH 256
#define DMS_MAX_EVENT_DATA_LENGTH 32

/* sensor status */
#define DMS_SENSOR_STATUS_GOOD 0U               /* good */
#define DMS_SENSOR_STATUS_FAULT 1U              /* fault */
#define DMS_SENSOR_STATUS_INVALID 2U            /* invalid */
#define DMS_SENSOR_STATUS_LOW_MINOR_FAULT 3U    /* low minor fault */
#define DMS_SENSOR_STATUS_LOW_MAJOR_FAULT 4U    /* low major fault */
#define DMS_SENSOR_STATUS_LOW_CRITICAL_FAULT 5U /* low critical fault */
#define DMS_SENSOR_STATUS_UP_MINOR_FAULT 6U     /* up minor fault */
#define DMS_SENSOR_STATUS_UP_MAJOR_FAULT 7U     /* up major fault */
#define DMS_SENSOR_STATUS_UP_CRITICAL_FAULT 8U  /* up critical fault */
#define DMS_SENSOR_STATUS_CFG_FAULT 0xf9U       /* configuration failed */

/* notification status */
#define DMS_ES_UNSPECIFIED (unsigned int)0x0000

/* Event status definition for Event Type = DMS_EC_THRESHOLD (Threshold-related) */
#define DMS_ES_LOWER_MINOR 0x0001U
#define DMS_ES_LOWER_MAJOR 0x0002U
#define DMS_ES_LOWER_CRIT 0x0004U
#define DMS_ES_UPPER_MINOR 0x0008U
#define DMS_ES_UPPER_MAJOR 0x0010U
#define DMS_ES_UPPER_CRIT 0x0020U

#define TOPOLOGY_HCCS 0
#define TOPOLOGY_PIX 1
#define TOPOLOGY_PIB 2
#define TOPOLOGY_PHB 3
#define TOPOLOGY_SYS 4
#define TOPOLOGY_SIO 5
#define TOPOLOGY_HCCS_SW 6
#define TOPOLOGY_UB 7
/***************************************************************************************************************/

int dms_get_devid_from_data(void *data);
int dms_get_dev_topology(unsigned int dev_id1, unsigned int dev_id2, int *topology_type);

#endif
