/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef SENSOR_USER_AGENT_H
#define SENSOR_USER_AGENT_H

#include "ascend_hal_error.h"

#define CFG_NAME_MAX_LENGTH 20
struct dms_sensor_node_cfg {
    char name[CFG_NAME_MAX_LENGTH];
    unsigned short node_type; /* bit 15~8: 000-hardware 001-soft 111-product; bit 7~0: node_type */
    unsigned char sensor_type;
    unsigned char resv; /* used for byte alignment */
    unsigned int assert_event_mask;
    unsigned int deassert_event_mask;
    unsigned char reserve[32]; /* 32: unused */
};

/* dms sensor ioctl args */
struct dms_sensor_user {
    unsigned int dev_id;
    struct dms_sensor_node_cfg cfg;
    uint64_t handle;
    int value;
    int assertion;
};

#endif /* SENSOR_USER_AGENT_H */
