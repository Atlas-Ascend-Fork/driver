/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef PROF_IOCTL_H
#define PROF_IOCTL_H
#include "prof_command_base.h"
#include "pbl_prof_interface_cmd.h"

#define PROF_POLL_DEPTH 512U
/* sample period time / ms */
#define PROF_PERIOD_MIN 10U    /* 10ms */
#define PROF_PERIOD_MAX 10000U /* 10s */

#define PROF_HOST_SAMPLE_PERIOD_DEFAULT 7U /* 7ms */
#define PROF_HOST_SAMPLE_PERIOD_MIN 1U     /* 1ms */
#define PROF_HOST_SAMPLE_PERIOD_MAX 10000U /* 10s */

#ifndef PROF_UNIT_TEST
#define PROF_EVENT_REPLY_BUFFER_RET_OFFSET (sizeof(int))
#else
#define PROF_EVENT_REPLY_BUFFER_RET_OFFSET (sizeof(unsigned long long))
#endif
#define PROF_EVENT_REPLY_BUFFER_RET(ptr) (*((int *)ptr))
#define PROF_EVENT_REPLY_BUFFER_DATA_PTR(ptr) (((char *)ptr) + PROF_EVENT_REPLY_BUFFER_RET_OFFSET)

enum channel_poll_flag {
    POLL_INVALID,
    POLL_VALID
};

typedef struct prof_ioctl_para {
    uint32_t device_id;
    uint32_t vfid; /* vfid = 0, is physical machine; vfid = 1~16, is virtual machine */
    uint32_t channel_id;
    uint32_t cmd;
    uint32_t buf_len;
    uint32_t buf_idx;
    uint32_t sample_period; /**< Sampling period */
    int ret_val;
    int timeout;
    int poll_number; /**< channel number */
    uint32_t user_data_size;
    uint32_t use_mode;
    char user_data[PROF_USER_DATA_LEN];
    void *out_buf; /**< save return info */
} prof_ioctl_para_t;

enum prof_cmd_type {
    PROF_GET_PLATFORM = 201,
    PROF_GET_DEVNUM,
    PROF_GET_DEVIDS, // for reserve
    PROF_START,
    PROF_STOP,
    PROF_READ,
    PROF_POLL,
    PROF_GET_CHANNEL_LIST,
    PROF_DATA_FLUSH,
    PROF_CHAN_REGISTER,
    PROF_WRITE,
    PROF_CHAN_QUERY,
    PROF_CMD_MAX
};

#endif