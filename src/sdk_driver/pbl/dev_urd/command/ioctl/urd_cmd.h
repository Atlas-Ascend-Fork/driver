/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef URD_CMD_H
#define URD_CMD_H
#include "pbl_urd_common.h"

struct urd_ioctl_arg {
    unsigned int devid;
    struct urd_cmd cmd;
    struct urd_cmd_para cmd_para;
};

#define DMS_MAGIC 'V'
#define URD_IOCTL_CMD _IO(DMS_MAGIC, 1)
#define URD_QUERY_IOCTL_CMD _IO(DMS_MAGIC, 2)
#define URD_CONFIG_IOCTL_CMD _IO(DMS_MAGIC, 3)

#endif