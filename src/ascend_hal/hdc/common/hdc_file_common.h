/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HDC_FILE_TRANSFER_H_
#define _HDC_FILE_TRANSFER_H_

#include <stdint.h>
#include <stdbool.h>

#include "ascend_hal.h"

#ifndef HDC_UT_TEST
#include "mmpa_api.h"
#endif

#define HDC_NAME_MAX 256
#define PATH_MAX 4096
#define FILE_SEG_MAX_SIZE 4194304
#define SESSION_TIME_OUT 1800

#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define RECV_TIME_OUT_COUNT 30 /* retry 30 times for fpga */
#else
#define RECV_TIME_OUT_COUNT 5
#endif

#define HDC_SLAVE_BASE_PATH_NAME "/home/HwHiAiUser/hdcd/device"
#define HDC_SLAVE_DM_PATH_NAME "/home/HwDmUser/hdcd/device"
#define HDC_SLAVE_CANN_FOLDER_NAME "/aicpu"
#define HDC_HOST_BASE_PATH_NAME "/var/hdcd"
#define HDC_SEND_FILE_PROGRESS 100
#define HDCD_CANN_FILE_TRANS_MAX_SIZE 5368709120 /* 5G */
#define HDCD_CANN_FILE_TRANS_MAX_NUM 640
#define HDC_FILE_TRANS_MODE_UPGRADE 0xff
#define HDC_FILE_TRANS_MODE_CANN 0x5a

#if defined(CFG_PLATFORM_ESL) || defined(CFG_PLATFORM_FPGA)
#define HDC_SEND_WAIT_TIME 300000
#define HDC_RECV_WAIT_TIME 300000
#else
#define HDC_SEND_WAIT_TIME 3000
#define HDC_RECV_WAIT_TIME 3000
#endif

#define HDC_SEND_FILE_WAIT_TIME 300000
#define SECONDS_TO_MICORSECONDS 1000000
#define SECONDS_TO_NS 1000000000ULL
#define CONVERT_NS_TO_US 1000
#define hdc_file_check(func_name, ret, exp)                                                          \
    do {                                                                                             \
        if ((ret) != (exp))                                                                          \
            HDC_LOG_ERR("%s return value expect: %d, but actually return: %d", func_name, exp, ret); \
    } while (0)

#define DIR_SEND_MAX_DEPTH 20
enum FILE_HDR_FLAGS {
    FILE_FLAGS_DATA = 1,
    FILE_FLAGS_ACK = 2,
    FILE_FLAGS_REQ = 4,
    FILE_FLAGS_RPLY = 8,
    FILE_FLAGS_FIN = 16,
    FILE_FLAGS_CMD = 32,
    FILE_FLAGS_END = 64,
    FILE_FLAGS_PULL = 128
};

enum FILE_OPT_KINDS {
    FILE_OPT_NOOP = 0,
    FILE_OPT_NAME,
    FILE_OPT_DSTPTH,
    FILE_OPT_SIZE,
    FILE_OPT_MODE,
    FILE_OPT_RCVOK,
    FILE_OPT_NOSPC,
    FILE_OPT_WRPTH,
    FILE_OPT_RCVSZ,
    FILE_OPT_MKDIR,
    FILE_OPT_RECUR,
    FILE_OPT_PERDE,
    FILE_OPT_NOFILE,
    FILE_OPT_WRING,
    FILE_OPT_TMSTMP
};

struct filehdr {
    uint32_t len;
    uint32_t seq;
    uint16_t hdrlen;
    uint8_t user_mode;
    uint8_t flags;
    uint32_t reserved32;
} __attribute__((packed));

struct fileopt {
    uint16_t kind;
    uint16_t opt_len;
    uint32_t reserved;
    char info[0];
} __attribute__((packed));

struct filesock {
    HDC_SESSION session;
    uint32_t seq;
    char file[PATH_MAX];
    char dstpth[PATH_MAX];
    uint64_t file_size;
    uint32_t file_mode;

    bool is_sender;
    hdcError_t hdc_errno;
    bool exit;
    uint32_t is_close;
    int root_privilege;

    struct timespec start_time;
    struct timespec session_start_time;
    void (*progress_notifier)(struct drvHdcProgInfo *prog_info);
    struct drvHdcProgInfo prog_info;

    int white_list_idx;
    uint8_t user_mode;
};

void set_flag(struct filehdr *hdr, enum FILE_HDR_FLAGS flag);
uint16_t set_option_dstpth(char *sndbuf, signed int bufsize, uint32_t offset, const char *dstpth);
uint16_t set_option_mode(char *sndbuf, signed int bufsize, uint32_t offset, uint32_t mode);
#ifndef DRV_UT
hdcError_t hdc_session_send(HDC_SESSION session, char *sndbuf, signed int bufsize);
#endif
hdcError_t send_file_in_session(signed int user_mode, HDC_SESSION session, const char *file, const char *dst_path,
                                void (*progress_notifier)(struct drvHdcProgInfo *));
hdcError_t get_hdc_capacity(struct drvHdcCapacity *capacity);
hdcError_t recv_reply(HDC_SESSION session, uint16_t *res);
hdcError_t send_end(HDC_SESSION session, char *sndbuf, signed int bufsize);
void call_progress_notifier(struct filesock *fs, bool is_fin);
uint16_t set_option_name(char *sndbuf, signed int bufsize, uint32_t offset, const char *file);
bool validate_recv_segment(char *p_rcvbuf, signed int buf_len);
struct fileopt *get_specific_option(char *rcvbuf, uint16_t option_type);
bool is_dir(const char *name, size_t len);
hdcError_t drvHdcSendDir(signed int peer_node, signed int peer_devid, const char *plocal_dir, const char *pdst_path,
                         void (*progress_notifier)(struct drvHdcProgInfo *));
hdcError_t send_dir_in_session(HDC_SESSION session, const char *plocal_dir, const char *pdst_path, int count,
                               void (*progress_notifier)(struct drvHdcProgInfo *));
hdcError_t get_new_dir_name(char *dst_path, int dst_len, const char *src_path, const char *d_name);

#endif
