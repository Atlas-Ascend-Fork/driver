/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>

#include "securec.h"
#include "hdc_cmn.h"
#include "ascend_inpackage_hal.h"
#include "hdc_file_common.h"

unsigned long long g_recv_bytes = 0;

void set_flag(struct filehdr *hdr, enum FILE_HDR_FLAGS flag)
{
    hdr->flags = flag;
}

struct fileopt *get_specific_option(char *rcvbuf, uint16_t option_type)
{
    struct filehdr *fh = (struct filehdr *)rcvbuf;
    size_t offset = sizeof(struct filehdr);
    struct fileopt *fopt = NULL;

    while (offset < ntohs(fh->hdrlen)) {
        fopt = (struct fileopt *)((char *)fh + offset);
        if (ntohs(fopt->kind) == option_type) {
            break;
        }

        offset += sizeof(struct fileopt);
        offset += ntohs(fopt->opt_len);
    }

    if (offset < ntohs(fh->hdrlen)) {
        return fopt;
    } else {
        return NULL;
    }
}

bool validate_recv_segment(char *p_rcvbuf, signed int buf_len)
{
    struct filehdr *fh = (struct filehdr *)p_rcvbuf;
    uint32_t offset = sizeof(struct filehdr);
    struct fileopt *fopt = NULL;

    if ((uint32_t)buf_len < offset || ntohl(fh->len) != (uint32_t)buf_len || ntohl(fh->len) < ntohs(fh->hdrlen)) {
        HDC_LOG_ERR("Input parameter is error. (buf_len=%d; file_len=%d; header_len=%d)\n", buf_len, ntohl(fh->len),
                    (signed int)ntohs(fh->hdrlen));
        return false;
    }

    while (offset < ntohs(fh->hdrlen)) {
        if ((offset + sizeof(struct fileopt)) > ntohs(fh->hdrlen)) {
            HDC_LOG_ERR("Input parameter is error. (offset=%d; fileopt_len=%d; header_len=%d)\n", offset,
                        (signed int)sizeof(struct fileopt), (signed int)ntohs(fh->hdrlen));
            return false;
        }

        fopt = (struct fileopt *)((char *)fh + offset);
        offset += (uint32_t)sizeof(struct fileopt);
        offset += ntohs(fopt->opt_len);
    }

    if (offset != ntohs(fh->hdrlen)) {
        HDC_LOG_ERR("Parameter offset is error. (offset=%d; header_len=%d)\n", offset, (signed int)ntohs(fh->hdrlen));
        return false;
    }

    return true;
}

hdcError_t recv_reply(HDC_SESSION session, uint16_t *res)
{
    struct drvHdcMsg *p_rcvmsg = NULL;
    signed int rcvbuf_count;
    char *p_rcvbuf = NULL;
    signed int buf_len = 0;
    struct filehdr *fh = NULL;
    hdcError_t ret;
    int retry_cnt = 0;

recv_flag_again:
    ret = drvHdcAllocMsg(session, &p_rcvmsg, 1);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcAllocMsg error. (hdcError_t=%d)\n", ret);
        return ret;
    }

    ret = halHdcRecv(session, p_rcvmsg, FILE_SEG_MAX_SIZE, 0, &rcvbuf_count, 0);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling halHdcRecv error. (hdcError_t=%d)\n", ret);
        (void)drvHdcFreeMsg(p_rcvmsg);
        return ret;
    }

    ret = drvHdcGetMsgBuffer(p_rcvmsg, 0, &p_rcvbuf, &buf_len);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcGetMsgBuffer error. (hdcError_t=%d)\n", ret);
        (void)drvHdcFreeMsg(p_rcvmsg);
        return ret;
    }

    g_recv_bytes += (unsigned long long)buf_len;
    if (!validate_recv_segment(p_rcvbuf, buf_len)) {
        HDC_LOG_ERR("Calling validate_recv_segment error. (buffer=\"%s\"; buffer_len=%d)\n", p_rcvbuf, buf_len);
        (void)drvHdcFreeMsg(p_rcvmsg);
        return DRV_ERROR_INVALID_VALUE;
    }

    fh = (struct filehdr *)p_rcvbuf;
    if (fh->flags != FILE_FLAGS_RPLY) {
        (void)drvHdcFreeMsg(p_rcvmsg);
        if ((fh->flags == FILE_FLAGS_ACK) && (retry_cnt == 0)) {
            retry_cnt++;
            HDC_LOG_WARN("Receive ack flag, reply may arrive in the next packet, retry recv.\n");
            goto recv_flag_again;
        }
        HDC_LOG_ERR("Receive packet isn't reply after retry. (flags=%d, retry_time=%d)\n", fh->flags, retry_cnt);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (get_specific_option(p_rcvbuf, FILE_OPT_RCVOK) != NULL) {
        *res = FILE_OPT_RCVOK;
    } else if (get_specific_option(p_rcvbuf, FILE_OPT_NOSPC) != NULL) {
        *res = FILE_OPT_NOSPC;
    } else if (get_specific_option(p_rcvbuf, FILE_OPT_WRPTH) != NULL) {
        *res = FILE_OPT_WRPTH;
    } else {
        *res = FILE_OPT_NOOP;
    }

    (void)drvHdcFreeMsg(p_rcvmsg);
    p_rcvmsg = NULL;
    return ret;
}

#ifndef DRV_UT
hdcError_t hdc_session_send(HDC_SESSION session, char *sndbuf, signed int bufsize)
{
    hdcError_t ret;
    struct drvHdcMsg *p_sndmsg = NULL;

    ret = drvHdcAllocMsg(session, &p_sndmsg, 1);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcAllocMsg error. (hdcError_t=%d)\n", ret);
        return ret;
    }

    ret = drvHdcAddMsgBuffer(p_sndmsg, sndbuf, bufsize);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcAddMsgBuffer error. (hdcError_t=%d)\n", ret);
        (void)drvHdcFreeMsg(p_sndmsg);
        return ret;
    }

    ret = halHdcSend(session, p_sndmsg, HDC_FLAG_WAIT_TIMEOUT, HDC_SEND_FILE_WAIT_TIME);
    if (ret != DRV_ERROR_NONE) {
        if (ret == DRV_ERROR_WAIT_TIMEOUT) {
            HDC_LOG_ERR("Calling halHdcSend timeout, peer oom or peer receive packet blocking.\n");
        } else {
            HDC_LOG_ERR("Calling halHdcSend error. (hdcError_t=%d)\n", ret);
        }
        (void)drvHdcFreeMsg(p_sndmsg);
        return ret;
    }

    (void)drvHdcFreeMsg(p_sndmsg);
    return DRV_ERROR_NONE;
}
#endif

STATIC hdcError_t send_fin(struct filesock *fs, char *sndbuf, signed int bufsize)
{
    struct filehdr *fh = (struct filehdr *)sndbuf;
    uint32_t len;
    uint16_t hdrlen;
    hdcError_t ret;
    (void)bufsize;

    set_flag(fh, FILE_FLAGS_FIN);
    hdrlen = sizeof(struct filehdr);
    len = hdrlen;
    fh->len = htonl(len);
    fh->seq = htonl(fs->seq);
    fh->hdrlen = htons(hdrlen);

    ret = hdc_session_send(fs->session, sndbuf, (int)len);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling hdc_session_send error. (hdcError_t=%d)\n", ret);
    }

    return ret;
}

STATIC void send_data_fill_fh(struct filehdr *fh, struct filesock *fs)
{
    uint16_t hdrlen;

    set_flag(fh, FILE_FLAGS_DATA);
    fh->seq = htonl(fs->seq++);
    hdrlen = sizeof(struct filehdr);
    fh->hdrlen = htons(hdrlen);
}

STATIC hdcError_t send_data_handle(struct filesock *fs, struct drvHdcMsg *p_sndmsg, char *sndbuf, signed int bufsize)
{
    signed int fd = -1;
    struct filehdr *fh = (struct filehdr *)sndbuf;
    signed long long len, rdlen;
    uint16_t hdrlen = sizeof(struct filehdr);
    hdcError_t ret = 0;

    send_data_fill_fh(fh, fs);
    len = hdrlen;

    fd = open(fs->file, O_RDONLY);
    if (fd < 0) {
        HDC_LOG_ERR("Open file error. (file=\"%s\"; strerror=\"%s\")\n", fs->file, strerror(errno));
        return DRV_ERROR_FILE_OPS;
    }

    while ((rdlen = read(fd, &(sndbuf[hdrlen]), (size_t)(bufsize - hdrlen))) != 0) {
        if (rdlen == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                HDC_LOG_ERR("Read file error. (file=\"%s\"; strerror=\"%s\")\n", fs->file, strerror(errno));
                ret = DRV_ERROR_FILE_OPS;
                goto other_error;
            }
        }
        len += rdlen;
        fh->len = htonl((uint32_t)len);

        if ((ret = drvHdcAddMsgBuffer(p_sndmsg, sndbuf, (int)len)) != DRV_ERROR_NONE) {
            HDC_LOG_ERR("Calling drvHdcAddMsgBuffer error. (hdcError_t=%d)\n", ret);
            goto other_error;
        }
        if ((ret = halHdcSend(fs->session, p_sndmsg, HDC_FLAG_WAIT_TIMEOUT, HDC_SEND_FILE_WAIT_TIME)) !=
            DRV_ERROR_NONE) {
            if (ret == DRV_ERROR_WAIT_TIMEOUT) {
                HDC_LOG_ERR("Calling halHdcSend timeout, peer oom or peer receive packet blocking.\n");
            } else {
                HDC_LOG_ERR("Calling halHdcSend error. (hdcError_t=%d)\n", ret);
            }
            goto other_error;
        }

        (void)drvHdcReuseMsg(p_sndmsg);
        send_data_fill_fh(fh, fs);
        len = hdrlen;

        if ((fs->hdc_errno != DRV_ERROR_NONE) || fs->exit) {
            HDC_LOG_WARN("Other thread something wrong, quit.\n");
            if (fs->hdc_errno != DRV_ERROR_NONE) {
                ret = DRV_ERROR_INVALID_VALUE;
            }
            goto other_error;
        }
    }

other_error:
    (void)close(fd);
    fd = -1;
    return ret;
}

STATIC hdcError_t send_data(struct filesock *fs, char *sndbuf, signed int bufsize)
{
    struct drvHdcMsg *p_sndmsg = NULL;
    uint16_t hdrlen = sizeof(struct filehdr);
    hdcError_t ret;
    struct drvHdcCapacity capacity = {0};

    if (bufsize <= hdrlen) {
        HDC_LOG_ERR("Input parameter is error. (bufsize=%d; hdrlen=%d)\n", bufsize, hdrlen);
        return DRV_ERROR_INVALID_VALUE;
    }
    ret = drvHdcGetCapacity(&capacity);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcGetCapacity error. (hdcError_t=%d)\n", ret);
        return ret;
    }
    HDC_LOG_INFO("Get capacity_maxSegment value. (maxSegment=%d; bufsize=%d)\n", capacity.maxSegment, bufsize);

    ret = drvHdcAllocMsg(fs->session, &p_sndmsg, 1);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcAllocMsg error. (hdcError_t=%d)\n", ret);
        return ret;
    }

    ret = (hdcError_t)clock_gettime(CLOCK_MONOTONIC, &fs->start_time);
    hdc_file_check("clock_gettime", ret, DRV_ERROR_NONE);

    if ((ret = send_data_handle(fs, p_sndmsg, sndbuf, bufsize)) != DRV_ERROR_NONE) {
        HDC_LOG_WARN("Send data not success. (ret=%d)\n", ret);
    }

    (void)drvHdcFreeMsg(p_sndmsg);
    p_sndmsg = NULL;
    return ret;
}

STATIC uint64_t get_send_rate(const struct filesock *fs)
{
    struct timespec now;
    uint64_t rate, time;
    signed int ret;

    ret = (hdcError_t)clock_gettime(CLOCK_MONOTONIC, &now);
    hdc_file_check("clock_gettime", (hdcError_t)ret, DRV_ERROR_NONE);
    time = (uint64_t)(((uint64_t)now.tv_sec * SECONDS_TO_NS + (uint64_t)now.tv_nsec) -
                      ((uint64_t)fs->start_time.tv_sec * SECONDS_TO_NS + (uint64_t)fs->start_time.tv_nsec)) /
           CONVERT_NS_TO_US;
    if (time != 0) {
        rate = (uint64_t)(fs->prog_info.send_bytes) * SECONDS_TO_MICORSECONDS / time;
    } else {
        rate = 0;
    }

    return rate;
}

void call_progress_notifier(struct filesock *fs, bool is_fin)
{
    if (fs->progress_notifier != NULL) {
        if (is_fin) {
            fs->prog_info.send_bytes = (long long int)fs->file_size;
            fs->prog_info.progress = HDC_SEND_FILE_PROGRESS;
            fs->prog_info.rate = (long long int)get_send_rate(fs);
            fs->prog_info.remain_time = 0;
            fs->progress_notifier(&fs->prog_info);
        } else {
            if (fs->file_size == 0) {
                fs->prog_info.progress = HDC_SEND_FILE_PROGRESS;
            } else {
                fs->prog_info.progress = (int)(((uint64_t)(fs->prog_info.send_bytes) * HDC_SEND_FILE_PROGRESS) /
                                               fs->file_size); // lint !e573
            }

            fs->prog_info.rate = (long long int)get_send_rate(fs);
            if (fs->prog_info.rate != 0) {
                fs->prog_info.remain_time = (int)((fs->file_size - (uint64_t)(fs->prog_info.send_bytes)) /
                                                  (uint64_t)fs->prog_info.rate);
            } else {
                fs->prog_info.remain_time = 0x7FFFFFFF;
            }

            /* avoid repeat called */
            if (fs->prog_info.progress != HDC_SEND_FILE_PROGRESS) {
                fs->progress_notifier(&fs->prog_info);
            }
        }
    }
}

STATIC void process_ack(char *p_rcvbuf, struct filesock *fs)
{
    struct fileopt *fopt = NULL;

    fopt = get_specific_option(p_rcvbuf, FILE_OPT_RCVSZ);
    if (fopt == NULL) {
        HDC_LOG_WARN("ACK packet doesn't have recv size, go on.\n");
        return;
    }

    if (ntohs(fopt->opt_len) >= sizeof(fs->prog_info.send_bytes)) {
        if (memcpy_s(&(fs->prog_info.send_bytes), sizeof(fs->prog_info.send_bytes), fopt->info,
                     sizeof(fs->prog_info.send_bytes)) != 0) {
            return;
        }

        call_progress_notifier(fs, 0);
    }
}

STATIC void process_reply(char *p_rcvbuf, struct filesock *fs)
{
    if (get_specific_option(p_rcvbuf, FILE_OPT_RCVOK) != NULL) {
        call_progress_notifier(fs, 1);
    } else if (get_specific_option(p_rcvbuf, FILE_OPT_NOSPC) != NULL) {
        HDC_LOG_ERR("There is no space on the destination side, exit.\n");
        fs->hdc_errno = DRV_ERROR_NO_FREE_SPACE;
    } else if (get_specific_option(p_rcvbuf, FILE_OPT_WRPTH) != NULL) {
        HDC_LOG_ERR("The destination path is illegal, exit. (dstpth=\"%s\")\n", fs->dstpth);
        fs->hdc_errno = DRV_ERROR_DST_PATH_ILLEGAL;
    } else if (get_specific_option(p_rcvbuf, FILE_OPT_PERDE) != NULL) {
        HDC_LOG_ERR("No permission for the destination path, exit. (dstpth=\"%s\")\n", fs->dstpth);
        fs->hdc_errno = DRV_ERROR_DST_PERMISSION_DENIED;
    } else if (get_specific_option(p_rcvbuf, FILE_OPT_WRING) != NULL) {
        HDC_LOG_WARN("The destination file is being written, exit. (dstpth=\"%s\")\n", fs->dstpth);
        fs->hdc_errno = DRV_ERROR_DST_FILE_IS_BEING_WRITTEN;
    } else if (get_specific_option(p_rcvbuf, FILE_OPT_SIZE) != NULL) {
        HDC_LOG_ERR("Remote whitelist verify failed, check the remote logs, exit.(dstpth=\"%s\")\n", fs->dstpth);
        fs->hdc_errno = DRV_ERROR_INVALID_VALUE;
    } else {
        HDC_LOG_ERR("Receive an unexpected reply packet, quit.\n");
        fs->exit = 1;
        fs->hdc_errno = DRV_ERROR_INVALID_VALUE;
    }
}

STATIC bool process_exit_check(const struct filesock *fs, signed int *cnt)
{
    if (fs->exit) {
        if ((*cnt)-- <= 0) {
            HDC_LOG_WARN("Exit for timeout without reply packet.\n");
            return true;
        }
    }
    return false;
}

STATIC void *process_recv_thread(void *arg)
{
    struct filesock *fs = (struct filesock *)arg;
    struct drvHdcMsg *p_rcvmsg = NULL;
    signed int rcvbuf_count;
    char *p_rcvbuf = NULL;
    signed int buf_len = 0;
    signed int timeout_recv_cnt = RECV_TIME_OUT_COUNT;
    signed int timeout_exit_cnt = RECV_TIME_OUT_COUNT;
    struct filehdr *fh = NULL;
    hdcError_t ret;
    struct timespec now_time;

    ret = (hdcError_t)clock_gettime(CLOCK_MONOTONIC, &fs->session_start_time);
    hdc_file_check("clock_gettime", ret, DRV_ERROR_NONE);

    ret = drvHdcAllocMsg(fs->session, &p_rcvmsg, 1);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Call drvHdcAllocMsg error. (hdcError_t=%d)\n", ret);
        return NULL;
    }

    fs->is_close = 0;

    while (1) {
        ret = (hdcError_t)clock_gettime(CLOCK_MONOTONIC, &now_time);
        if ((ret != DRV_ERROR_NONE) || (now_time.tv_sec - fs->session_start_time.tv_sec >= SESSION_TIME_OUT)) {
            (void)drvHdcSessionClose(fs->session);
            fs->is_close = 1;
            HDC_LOG_ERR("Session time out. (ret=%d)\n", ret);
            break;
        }

        (void)drvHdcReuseMsg(p_rcvmsg);
        ret = halHdcRecv(fs->session, p_rcvmsg, FILE_SEG_MAX_SIZE, HDC_FLAG_WAIT_TIMEOUT, &rcvbuf_count,
                         HDC_RECV_WAIT_TIME);
        if (ret == DRV_ERROR_WAIT_TIMEOUT) {
            if (fs->exit) {
                HDC_LOG_WARN("Calling halHdcRecv time out. (recv_cnt=%d; fs->exit=%d)\n", timeout_recv_cnt, fs->exit);
                break;
            }
            if (timeout_recv_cnt-- <= 0) {
                fs->hdc_errno = ret;
                HDC_LOG_ERR("halHdcRecv time out 5 times. (fs_exit=%d)\n", fs->exit);
                break;
            }
            continue;
        }
        if (ret != DRV_ERROR_NONE) {
            HDC_LOG_ERR("Calling halHdcRecv error. (hdcError_t=%d)\n", ret);
            fs->hdc_errno = ret;
            break;
        }

        if (process_exit_check(fs, &timeout_exit_cnt)) {
            HDC_LOG_WARN("Exit for timeout without reply packet.\n");
            break;
        }

        timeout_recv_cnt = RECV_TIME_OUT_COUNT;

        ret = drvHdcGetMsgBuffer(p_rcvmsg, 0, &p_rcvbuf, &buf_len);
        if (ret != DRV_ERROR_NONE) {
            HDC_LOG_ERR("Calling drvHdcGetMsgBuffer error. (hdcError_t=%d)\n", ret);
            fs->hdc_errno = ret;
            break;
        }

        g_recv_bytes += (unsigned long long)buf_len;
        if (!validate_recv_segment(p_rcvbuf, buf_len)) {
            HDC_LOG_WARN("Calling validate_recv_segment not success. (buffer=\"%s\"; buffer_len=%d)\n", p_rcvbuf,
                         buf_len);
            continue;
        }
        fh = (struct filehdr *)p_rcvbuf;
        if (fh->flags == FILE_FLAGS_RPLY) {
            process_reply(p_rcvbuf, fs);
            break;
        } else if (fh->flags == FILE_FLAGS_ACK) {
            process_ack(p_rcvbuf, fs);
        } else {
            HDC_LOG_WARN("Receive an unexpected packet, go on. (flags=%d)\n", fh->flags);
            continue;
        }
    }

    (void)drvHdcFreeMsg(p_rcvmsg);
    p_rcvmsg = NULL;
    return NULL;
}

STATIC hdcError_t send_data_and_fin(struct filesock *fs, char *sndbuf, signed int bufsize)
{
    hdcError_t ret;

    // send packet
    ret = send_data(fs, sndbuf, bufsize);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_WARN("Send data has problem.\n");
        return ret;
    }

    // send fin
    ret = send_fin(fs, sndbuf, bufsize);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Send fin error.\n");
        return ret;
    }

    return DRV_ERROR_NONE;
}

hdcError_t send_end(HDC_SESSION session, char *sndbuf, signed int bufsize)
{
    struct filehdr *fh = (struct filehdr *)sndbuf;
    uint32_t len;
    uint16_t hdrlen;
    hdcError_t ret;
    (void)bufsize;

    set_flag(fh, FILE_FLAGS_END);
    hdrlen = sizeof(struct filehdr);
    len = hdrlen;
    fh->len = htonl(len);
    fh->seq = htonl(0);
    fh->hdrlen = htons(hdrlen);

    ret = hdc_session_send(session, sndbuf, (int)len);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling hdc_session_send error. (hdcError_t=%d)\n", ret);
    }

    return ret;
}

STATIC hdcError_t is_recv_side_ready(struct filesock *fs)
{
    hdcError_t ret;
    uint16_t is_rcvok;

    ret = recv_reply(fs->session, &is_rcvok);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling recv_reply error.\n");
        return ret;
    }

    if (is_rcvok == FILE_OPT_NOSPC) {
        HDC_LOG_ERR("There is no space on the destination side, exit.\n");
        ret = DRV_ERROR_NO_FREE_SPACE;
    } else if (is_rcvok == FILE_OPT_WRPTH) {
        HDC_LOG_ERR("The destination path is illegal, exit. (dstpth=\"%s\")\n", fs->dstpth);
        ret = DRV_ERROR_DST_PATH_ILLEGAL;
    } else if (is_rcvok != FILE_OPT_RCVOK) {
        HDC_LOG_WARN("Receive an unexpected reply packet. (kinds=%d)\n", is_rcvok);
    }

    return ret;
}

uint16_t set_option_mode(char *sndbuf, signed int bufsize, uint32_t offset, uint32_t mode)
{
    struct fileopt *fopt = NULL;
    uint16_t len = sizeof(struct fileopt);
    uint16_t mode_len = sizeof(mode);
    uint32_t resspace;
    char *pinfo = NULL;

    if ((sndbuf == NULL) || (bufsize < 0) || ((uint32_t)bufsize < offset) || ((uint32_t)bufsize - offset < len) ||
        (uint32_t)bufsize - offset - len < sizeof(mode)) {
        return 0;
    }

    fopt = (struct fileopt *)(sndbuf + offset);
    fopt->kind = htons(FILE_OPT_MODE);
    fopt->opt_len = htons(mode_len);
    pinfo = fopt->info;
    resspace = (uint32_t)bufsize - offset - len;
    if (memcpy_s(pinfo, resspace, &mode, sizeof(mode)) != 0) {
        return 0;
    }

    len = (uint16_t)(len + mode_len);
    return len;
}

STATIC uint16_t set_option_size(char *sndbuf, signed int bufsize, uint32_t offset, uint64_t size)
{
    struct fileopt *fopt = NULL;
    uint16_t len = sizeof(struct fileopt);
    uint16_t size_len = sizeof(size);
    uint32_t resspace;
    char *pinfo = NULL;

    if ((sndbuf == NULL) || (bufsize < 0) || ((uint32_t)bufsize < offset) || ((uint32_t)bufsize - offset < len) ||
        (uint32_t)bufsize - offset - len < sizeof(size)) {
        return 0;
    }

    fopt = (struct fileopt *)(sndbuf + offset);
    fopt->kind = htons(FILE_OPT_SIZE);
    fopt->opt_len = htons(size_len);
    pinfo = fopt->info;
    resspace = (uint32_t)bufsize - offset - len;
    if (memcpy_s(pinfo, resspace, &size, sizeof(size)) != 0) {
        return 0;
    }

    len = (uint16_t)(len + size_len);
    return len;
}

uint16_t set_option_dstpth(char *sndbuf, signed int bufsize, uint32_t offset, const char *dstpth)
{
    struct fileopt *fopt = NULL;
    uint16_t len = sizeof(struct fileopt);
    uint16_t dstpth_len = (uint16_t)(strlen(dstpth) + 1);
    uint32_t resspace;
    char *pinfo = NULL;

    if ((sndbuf == NULL) || (bufsize < 0) || ((uint32_t)bufsize < offset) || ((uint32_t)bufsize - offset < len) ||
        ((uint32_t)bufsize - offset - len < dstpth_len)) {
        return 0;
    }

    fopt = (struct fileopt *)(sndbuf + offset);
    fopt->kind = htons(FILE_OPT_DSTPTH);
    fopt->opt_len = htons(dstpth_len);
    pinfo = fopt->info;
    resspace = (uint32_t)bufsize - offset - len;
    if (strcpy_s(pinfo, resspace, dstpth) != 0) {
        return 0;
    }

    len = (uint16_t)(len + dstpth_len);
    return len;
}

uint16_t set_option_name(char *sndbuf, signed int bufsize, uint32_t offset, const char *file)
{
    struct fileopt *fopt = NULL;
    uint16_t len = sizeof(struct fileopt);
    uint16_t name_len = (uint16_t)(strlen(file) + 1);
    uint32_t resspace;
    char *pinfo = NULL;

    if ((sndbuf == NULL) || (bufsize < 0) || ((uint32_t)bufsize < offset) || ((uint32_t)bufsize - offset < len) ||
        ((uint32_t)bufsize - offset - len < name_len)) {
        return 0;
    }

    fopt = (struct fileopt *)(sndbuf + offset);
    fopt->kind = htons(FILE_OPT_NAME);
    fopt->opt_len = htons(name_len);
    pinfo = fopt->info;
    resspace = (uint32_t)bufsize - offset - len;
    if (strcpy_s(pinfo, resspace, file) != 0) {
        return 0;
    }

    len = (uint16_t)(len + name_len);
    return len;
}

STATIC hdcError_t get_file_name(const struct filesock *fs, char *name, signed int len)
{
    uint32_t i = 0;
    uint32_t sppos = 0;

    (void)len;
    while (*(fs->file + i) != '\0') {
        if (*(fs->file + i) == '/') {
            sppos = i + 1;
        }
        i++;
    }

    if (strcpy_s(name, HDC_NAME_MAX, fs->file + sppos) != 0) {
        HDC_LOG_ERR("Calling strcpy_s error. (strerror=\"%s\")\n", strerror(errno));
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

STATIC hdcError_t send_request(struct filesock *fs, char *sndbuf, signed int bufsize)
{
    struct filehdr *fh = (struct filehdr *)sndbuf;
    uint32_t len;
    uint16_t hdrlen;
    uint16_t offset;
    hdcError_t ret;
    char name[HDC_NAME_MAX];

    set_flag(fh, FILE_FLAGS_REQ);
    hdrlen = sizeof(struct filehdr);

    if ((ret = get_file_name(fs, name, HDC_NAME_MAX)) != DRV_ERROR_NONE) {
        return DRV_ERROR_INVALID_VALUE;
    }

    offset = set_option_name(sndbuf, bufsize, hdrlen, name);
    if (offset == 0) {
        HDC_LOG_ERR("Calling set_option_name error. (file_name=\"%s\")\n", name);
        return DRV_ERROR_INVALID_VALUE;
    }

    hdrlen = (uint16_t)(hdrlen + offset);
    offset = set_option_dstpth(sndbuf, bufsize, hdrlen, fs->dstpth);
    if (offset == 0) {
        HDC_LOG_ERR("Calling set_option_dstpth error. (destination_path=\"%s\")\n", fs->dstpth);
        return DRV_ERROR_INVALID_VALUE;
    }

    hdrlen = (uint16_t)(hdrlen + offset);
    offset = set_option_size(sndbuf, bufsize, hdrlen, fs->file_size);
    if (offset == 0) {
        HDC_LOG_ERR("Calling set_option_size error. (file_size=%lu)\n", fs->file_size);
        return DRV_ERROR_INVALID_VALUE;
    }

    hdrlen = (uint16_t)(hdrlen + offset);
    offset = set_option_mode(sndbuf, bufsize, hdrlen, fs->file_mode);
    if (offset == 0) {
        HDC_LOG_ERR("Calling set_option_mode error. (file_mode=%d)\n", fs->file_mode);
        return DRV_ERROR_INVALID_VALUE;
    }

    hdrlen = (uint16_t)(hdrlen + offset);
    len = hdrlen;
    fh->len = htonl(len);
    fh->seq = htonl(fs->seq);
    fh->hdrlen = htons(hdrlen);
    fh->user_mode = fs->user_mode;

    ret = hdc_session_send(fs->session, sndbuf, (int)len);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling hdc_session_send error. (hdcError_t=%d)\n", ret);
        return ret;
    }

    return ret;
}

STATIC hdcError_t validate_file(struct filesock *fs)
{
    struct stat s_buf;
    signed int fd = -1;
    uint32_t mask = 0xfff;
    uint32_t sppos = 0;
    uint32_t i = 0;

    fd = open(fs->file, O_RDONLY);
    if (fd < 0) {
        HDC_LOG_ERR("Open file error. (file=\"%s\"; strerror=\"%s\")\n", fs->file, strerror(errno));
        return DRV_ERROR_OPEN_FAILED;
    }

    if (fstat(fd, &s_buf) < 0) {
        HDC_LOG_ERR("Calling fstat error. (file=\"%s\"; strerror=\"%s\")\n", fs->file, strerror(errno));
        (void)close(fd);
        fd = -1;
        return DRV_ERROR_LOCAL_ABNORMAL_FILE;
    }

    if (!S_ISREG(s_buf.st_mode)) {
        HDC_LOG_ERR("Calling S_ISREG error, please check input. (file=\"%s\")\n", fs->file);
        (void)close(fd);
        fd = -1;
        return DRV_ERROR_LOCAL_ABNORMAL_FILE;
    }

    (void)close(fd);
    fd = -1;
    fs->file_size = (uint64_t)s_buf.st_size;
    fs->file_mode = s_buf.st_mode & mask;

    while (*(fs->file + i) != '\0') {
        if (*(fs->file + i) == '/') {
            sppos = i + 1;
        }
        i++;
    }

    if (strcpy_s(fs->prog_info.name, HDC_NAME_MAX, fs->file + sppos) != 0) {
        HDC_LOG_ERR("Calling strcpy_s error. (strerror=\"%s\")\n", strerror(errno));
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

hdcError_t get_hdc_capacity(struct drvHdcCapacity *capacity)
{
    hdcError_t ret;

    ret = drvHdcGetCapacity(capacity);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcGetCapacity error. (hdcError_t=%d)\n", ret);
        return ret;
    }

    if (capacity->maxSegment <= 0) {
        HDC_LOG_ERR("Parameter capacity_maxSegment is invalid. (maxSegment=%d)\n", capacity->maxSegment);
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

hdcError_t send_file_in_session(signed int user_mode, HDC_SESSION session, const char *file, const char *dst_path,
                                void (*progress_notifier)(struct drvHdcProgInfo *))
{
    struct filesock *fs = NULL;
    hdcError_t ret;
    pthread_t recv_thread = 0;
    struct drvHdcCapacity capacity = {0};
    char *p_sndbuf = NULL;
    pthread_attr_t attr;
    char *file_path = NULL;

    (void)pthread_attr_init(&attr);

    if ((ret = get_hdc_capacity(&capacity)) != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling get_hdc_capacity error. (hdcError_t=%d)\n", ret);
        goto error0;
    }

    p_sndbuf = (char *)malloc((uint32_t)capacity.maxSegment);
    if (p_sndbuf == NULL) {
        HDC_LOG_ERR("Calling malloc p_sndbuf failed.\n");
        ret = DRV_ERROR_MALLOC_FAIL;
        goto error0;
    }
    fs = (struct filesock *)malloc(sizeof(struct filesock));
    if (fs == NULL) {
        HDC_LOG_ERR("Calling malloc fs failed.\n");
        ret = DRV_ERROR_MALLOC_FAIL;
        goto error1;
    }

    file_path = realpath((const char *)file, NULL);
    if (file_path == NULL) {
        HDC_LOG_ERR("Got realpath failed. (file_path=\"%s\")\n", file);
        ret = DRV_ERROR_INVALID_VALUE;
        goto error2;
    }

    if ((memset_s(fs, sizeof(struct filesock), 0, sizeof(struct filesock)) != 0) ||
        (strcpy_s(fs->file, PATH_MAX, file_path) != 0) || (strcpy_s(fs->dstpth, PATH_MAX, dst_path) != 0)) {
        HDC_LOG_ERR("Calling memset_s or strcpy_s error. (strerror=\"%s\")\n", strerror(errno));
        ret = DRV_ERROR_INVALID_VALUE;
        goto error3;
    }

    fs->session = session;
    fs->progress_notifier = progress_notifier;
    fs->user_mode = (uint8_t)user_mode;
    ret = validate_file(fs);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling validate_file error.\n");
        goto error3;
    }

    // send request
    ret = send_request(fs, p_sndbuf, (int)capacity.maxSegment);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling send_request error.\n");
        goto error3;
    }

    // recv reply
    ret = is_recv_side_ready(fs);
    if (ret != DRV_ERROR_NONE) {
        goto error3;
    }

    // create recv thread
    if (pthread_create(&recv_thread, &attr, process_recv_thread, (void *)fs) != 0) {
        HDC_LOG_ERR("Create recv_thread error. (strerror=\"%s\")\n", strerror(errno));
        ret = DRV_ERROR_INVALID_VALUE;
        goto error3;
    }

    ret = send_data_and_fin(fs, p_sndbuf, (int)capacity.maxSegment);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_WARN("Calling send_data_and_fin not success.\n");
    }

    fs->exit = 1;
    if (pthread_join(recv_thread, NULL) != 0) {
        HDC_LOG_ERR("Calling pthread_join recv_thread error. (strerror=\"%s\")\n", strerror(errno));
        ret = DRV_ERROR_INVALID_VALUE;
    }
    if (fs->hdc_errno != DRV_ERROR_NONE) {
        ret = fs->hdc_errno;
    }

error3:
    free(file_path);
    file_path = NULL;
error2:
    free(fs);
    fs = NULL;
error1:
    free(p_sndbuf);
    p_sndbuf = NULL;
error0:
    (void)pthread_attr_destroy(&attr);
    return ret;
}

hdcError_t drvHdcGetTrustedBasePathEx(signed int user_mode, signed int peer_node, signed int peer_devid,
                                      char *base_path, unsigned int path_len)
{
    hdcError_t ret;
    signed int s_ret;
    signed int id = 0;
    char path[HDC_NAME_MAX] = {0};

    if ((peer_node != 0) && (peer_node != 0xffff)) {
        HDC_LOG_ERR("Input parameter peer_node is error. (peer_node=%d)\n", peer_node);
        return DRV_ERROR_INVALID_VALUE;
    }

    if (base_path == NULL) {
        HDC_LOG_ERR("Input parameter base_path is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    /* When the peer node value is 0xffff, obtains the local security directory */
    if (peer_node == 0xffff) {
        s_ret = get_local_trusted_base_path(user_mode, path, peer_devid);
    } else {
        ret = drv_hdc_get_peer_dev_id(peer_devid, &id);
        if (ret != DRV_ERROR_NONE) {
            HDC_LOG_ERR("Calling drv_hdc_get_peer_dev_id failed. (dev_id=%d; ret=%d)\n", peer_devid, ret);
            return ret;
        }
        s_ret = get_peer_trusted_base_path(user_mode, path, id);
    }

    if (s_ret < 0) {
        HDC_LOG_ERR("Calling sprintf_s failed.\n");
        return DRV_ERROR_INNER_ERR;
    }

    if (path_len <= (unsigned int)strlen(path)) {
        HDC_LOG_ERR("Parameter path_len is invalid. (id=%d; path_len=%d; len=%u)\n", id, path_len,
                    (unsigned int)strlen(path));
        return DRV_ERROR_INVALID_VALUE;
    }

    if (strcpy_s(base_path, path_len, path) != 0) {
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

STATIC hdcError_t drv_hdc_send_file_para_check(signed int user_mode, signed int peer_node, signed int peer_devid,
                                               const char *file, const char *dst_path)
{
#ifndef CFG_SOC_PLATFORM_RC
    hdcError_t ret;
#endif
    char base_path[HDC_NAME_MAX] = {0};
    int file_len, dstpth_len;

    /* progress_notifier is designed as a hook-function,could be null for real-use */
    if ((file == NULL) || (dst_path == NULL)) {
        HDC_LOG_ERR("Input parameter is error. (file or dst_path)\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    file_len = (int)strnlen(file, PATH_MAX);
    dstpth_len = (int)strnlen(dst_path, PATH_MAX);
    if ((file_len == 0) || (file_len >= PATH_MAX) || (dstpth_len >= PATH_MAX) || (dstpth_len == 0)) {
        HDC_LOG_ERR("Input parameter is invalid. (file_len=%d;dstpth_len=%d;)\n", file_len, dstpth_len);
        return DRV_ERROR_INVALID_VALUE;
    }

#ifndef CFG_SOC_PLATFORM_RC
    ret = drvHdcGetTrustedBasePathEx(user_mode, peer_node, peer_devid, base_path, HDC_NAME_MAX);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcGetTrustedBasePath error. (hdcError_t=%d)\n", ret);
        return ret;
    }
#endif

    if (strstr(dst_path, (const char *)base_path) != dst_path) {
        HDC_LOG_ERR("Calling strstr failed. (dev_id=%d; file=\"%s\"; dst_path=\"%s\"; base_path=\"%s\")\n", peer_devid,
                    file, dst_path, base_path);
        return DRV_ERROR_DST_PATH_ILLEGAL;
    }

    return DRV_ERROR_NONE;
}

hdcError_t drvHdcSendFileEx(signed int user_mode, signed int peer_node, signed int peer_devid, const char *file,
                            const char *dst_path, void (*progress_notifier)(struct drvHdcProgInfo *))
{
    HDC_CLIENT client = NULL;
    HDC_SESSION session = NULL;
    hdcError_t ret;
    struct drvHdcCapacity capacity = {0};
    char *p_sndbuf = NULL;
    signed int type;

    if ((ret = drv_hdc_send_file_para_check(user_mode, peer_node, peer_devid, file, dst_path)) != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drv_hdc_send_file_para_check error. (hdcError_t=%d)\n", ret);
        return ret;
    }

#ifdef HDC_UT_TEST
    type = HDC_SERVICE_TYPE_FILE_TRANS;
#else
    if ((user_mode == 0) || (user_mode == HDC_FILE_TRANS_MODE_CANN)) {
        type = HDC_SERVICE_TYPE_FILE_TRANS;
    } else {
        type = HDC_SERVICE_TYPE_UPGRADE;
    }
#endif

    if ((ret = drvHdcClientCreate(&client, 1, type, 0)) != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcClientCreate error. (hdcError_t=%d)\n", ret);
        return ret;
    }

    ret = drvHdcSessionConnect(peer_node, peer_devid, client, &session);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_WARN("Calling drvHdcSessionConnect not success. (hdcError_t=%d)\n", ret);
        (void)drvHdcClientDestroy(client);
        return ret;
    }

    (void)drvHdcSetSessionReference(session);
    ret = send_file_in_session(user_mode, session, file, dst_path, progress_notifier);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_WARN("Calling send_file_in_session not success. (ret=%d)\n", ret);
        goto out;
    }

    if ((ret = get_hdc_capacity(&capacity)) != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling get_hdc_capacity error. (hdcError_t=%d)\n", ret);
        goto out;
    }

    p_sndbuf = malloc((uint32_t)capacity.maxSegment);
    if (p_sndbuf == NULL) {
        HDC_LOG_ERR("Calling malloc error. (strerror=\"%s\")\n", strerror(errno));
        ret = DRV_ERROR_MALLOC_FAIL;
        goto out;
    }

    ret = send_end(session, p_sndbuf, (int)capacity.maxSegment);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling send_end error.\n");
    }

    free(p_sndbuf);
    p_sndbuf = NULL;

out:
    (void)drvHdcSessionClose(session);
    (void)drvHdcClientDestroy(client);
    return ret;
}

bool is_dir(const char *name, size_t len)
{
    struct stat s_buf;
    (void)len;

    if (name == NULL) {
        HDC_LOG_ERR("Input parameter is error.\n");
        return false;
    }

    if (access(name, F_OK) != 0) {
        return false;
    }

    if (stat(name, &s_buf) < 0) {
        return false;
    }

    if (S_ISDIR(s_buf.st_mode)) {
        return true;
    } else {
        return false;
    }
}

STATIC uint16_t set_option_mkdir(char *sndbuf, signed int bufsize, uint32_t offset, const char *dir)
{
    struct fileopt *fopt = NULL;
    uint16_t len = sizeof(struct fileopt);
    char name[HDC_NAME_MAX];
    uint16_t name_len;
    uint32_t sppos = 0;
    uint32_t i = 0;
    uint32_t resspace;
    char *pinfo = NULL;

    while (*(dir + i) != '\0') {
        if (*(dir + i) == '/') {
            sppos = i + 1;
        }
        i++;
    }

    if (strcpy_s(name, HDC_NAME_MAX, dir + sppos) != 0) {
        return 0;
    }

    name_len = (uint16_t)(strlen(name) + 1);
    if ((sndbuf == NULL) || (bufsize < 0) || ((uint32_t)bufsize < offset) || ((uint32_t)bufsize - offset < len) ||
        ((uint32_t)bufsize - offset - len < name_len)) {
        return 0;
    }

    fopt = (struct fileopt *)(sndbuf + offset);
    fopt->kind = htons(FILE_OPT_MKDIR);
    fopt->opt_len = htons(name_len);
    pinfo = fopt->info;
    resspace = (uint32_t)bufsize - offset - len;
    if (strcpy_s(pinfo, resspace, name) != 0) {
        return 0;
    }

    len = (uint16_t)(len + name_len);
    return len;
}

STATIC hdcError_t send_cmd(HDC_SESSION session, char *sndbuf, signed int bufsize, const char *local_dir,
                           const char *dst_path)
{
    struct filehdr *fh = (struct filehdr *)sndbuf;
    uint32_t len;
    uint16_t hdrlen;
    uint16_t offset;
    hdcError_t ret;
    struct stat s_buf;

    if (stat(local_dir, &s_buf) < 0) {
        HDC_LOG_ERR("Calling stat error. (local_dir=\"%s\"; strerror=\"%s\")\n", local_dir, strerror(errno));
        return DRV_ERROR_INVALID_VALUE;
    }

    set_flag(fh, FILE_FLAGS_CMD);
    hdrlen = sizeof(struct filehdr);
    offset = set_option_mkdir(sndbuf, bufsize, hdrlen, local_dir);
    if (offset == 0) {
        HDC_LOG_ERR("Calling set_option_mkdir error. (local_dir=\"%s\")\n", local_dir);
        return DRV_ERROR_INVALID_VALUE;
    }

    hdrlen = (uint16_t)(hdrlen + offset);
    offset = set_option_dstpth(sndbuf, bufsize, hdrlen, dst_path);
    if (offset == 0) {
        HDC_LOG_ERR("Calling set_option_dstpth error. (destination_path=\"%s\")\n", dst_path);
        return DRV_ERROR_INVALID_VALUE;
    }

    hdrlen = (uint16_t)(hdrlen + offset);
    offset = set_option_mode(sndbuf, bufsize, hdrlen, s_buf.st_mode & 0xfff);
    if (offset == 0) {
        HDC_LOG_ERR("Calling set_option_mode error. (dir_mode=%d)\n", s_buf.st_mode & 0xfff);
        return DRV_ERROR_INVALID_VALUE;
    }

    hdrlen = (uint16_t)(hdrlen + offset);
    len = hdrlen;
    fh->len = htonl(len);
    fh->seq = htonl(0);
    fh->hdrlen = htons(hdrlen);

    ret = hdc_session_send(session, sndbuf, (int)len);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling hdc_session_send error. (hdcError_t=%d)\n", ret);
    }

    return ret;
}

hdcError_t get_new_dir_name(char *dst_path, int dst_len, const char *src_path, const char *d_name)
{
    if ((strcpy_s(dst_path, (size_t)dst_len, src_path) != EOK) ||
        (*(dst_path + strlen(dst_path) - 1) != '/' && (strcat_s(dst_path, (size_t)dst_len, "/") != EOK)) ||
        (strcat_s(dst_path, (size_t)dst_len, d_name) != EOK)) {
        return DRV_ERROR_INVALID_VALUE;
    }

    return DRV_ERROR_NONE;
}

STATIC hdcError_t __send_current_dir(HDC_SESSION session, const char *local_dir, size_t local_dir_len,
                                     char *new_local_dir, size_t new_local_len, char *new_dst_path, size_t new_dst_len,
                                     signed int count, void (*progress_notifier)(struct drvHdcProgInfo *))
{
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    hdcError_t ret = DRV_ERROR_NONE;
    (void)local_dir_len;
    (void)new_dst_len;

    if ((dir = opendir(local_dir)) == NULL) {
        HDC_LOG_ERR("Calling opendir error. (strerror=\"%s\")\n", strerror(errno));
        ret = DRV_ERROR_FILE_OPS;
        return ret;
    }

    while ((ptr = readdir(dir)) != NULL) {
        if ((strcmp(ptr->d_name, ".") == 0) || (strcmp(ptr->d_name, "..") == 0)) {
            continue;
        }

        ret = get_new_dir_name(new_local_dir, (int)new_local_len, local_dir, ptr->d_name);
        if (ret != DRV_ERROR_NONE) {
            HDC_LOG_ERR("Calling strcpy_s or strcat_s error. (strerror=\"%s\")\n", strerror(errno));
            break;
        }

        if (is_dir(new_local_dir, new_local_len)) {
            ret = send_dir_in_session(session, new_local_dir, new_dst_path, count, progress_notifier);
            if (ret != DRV_ERROR_NONE) {
                HDC_LOG_ERR("Calling send_dir_in_session error. (local_dir=\"%s\"; dst_path=\"%s\")\n", new_local_dir,
                            new_dst_path);
                break;
            }
        } else {
            ret = send_file_in_session(0, session, new_local_dir, new_dst_path, progress_notifier);
            if (ret != DRV_ERROR_NONE) {
                HDC_LOG_ERR("Calling send_file_in_session error. (file_name=\"%s\"; dst_path=\"%s\")\n", new_local_dir,
                            new_dst_path);
                break;
            }
        }
    }

    (void)closedir(dir);
    dir = NULL;
    ptr = NULL;
    return ret;
}

STATIC hdcError_t send_current_dir(HDC_SESSION session, const char *local_dir, size_t local_len, const char *dst_path,
                                   size_t dst_len, signed int count, void (*progress_notifier)(struct drvHdcProgInfo *))
{
    char *new_dst_path = NULL;
    char *new_local_dir = NULL;
    uint32_t sppos = 0;
    uint32_t i = 0;
    hdcError_t ret;
    (void)local_len;
    (void)dst_len;

    new_dst_path = (char *)malloc(PATH_MAX);
    if (new_dst_path == NULL) {
        HDC_LOG_ERR("Calling malloc new_dst_path error.\n");
        ret = DRV_ERROR_MALLOC_FAIL;
        goto error2;
    }

    new_local_dir = (char *)malloc(PATH_MAX);
    if (new_local_dir == NULL) {
        HDC_LOG_ERR("Calling malloc new_local_dir error.\n");
        ret = DRV_ERROR_MALLOC_FAIL;
        goto error1;
    }

    while (*(local_dir + i) != '\0') {
        if (*(local_dir + i) == '/') {
            sppos = i + 1;
        }
        i++;
    }

    if ((strcpy_s(new_dst_path, PATH_MAX, dst_path) != 0) ||
        (*(new_dst_path + strlen(new_dst_path) - 1) != '/' && (strcat_s(new_dst_path, PATH_MAX, "/") != 0)) ||
        (strcpy_s(new_dst_path + strlen(new_dst_path), PATH_MAX - strlen(new_dst_path), local_dir + sppos) != 0)) {
        HDC_LOG_ERR("Calling strcpy_s or strcat_s error. (strerror=\"%s\")\n", strerror(errno));
        ret = DRV_ERROR_INVALID_VALUE;
        goto error0;
    }

    ret = __send_current_dir(session, local_dir, PATH_MAX, new_local_dir, PATH_MAX, new_dst_path, PATH_MAX, count,
                             progress_notifier);

error0:

    free(new_local_dir);
    new_local_dir = NULL;
error1:
    free(new_dst_path);
    new_dst_path = NULL;
error2:
    return ret;
}

hdcError_t send_dir_in_session(HDC_SESSION session, const char *plocal_dir, const char *pdst_path, signed int count,
                               void (*progress_notifier)(struct drvHdcProgInfo *))
{
    hdcError_t ret;
    struct drvHdcCapacity capacity = {0};
    char *p_sndbuf = NULL;
    uint16_t is_rcvok;
    char *local_dir = NULL;
    signed int is_null;
    signed int is_count = count;

    if (is_count-- <= 0) {
        HDC_LOG_ERR("Only support directory maxdepth. (maxdepth=%d)\n", DIR_SEND_MAX_DEPTH);
        return DRV_ERROR_INVALID_VALUE;
    }

    is_null = (plocal_dir == NULL) || (pdst_path == NULL);
    if (is_null != 0) {
        HDC_LOG_ERR("Input parameter plocal_dir or pdst_path is NULL.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if (!is_dir(plocal_dir, PATH_MAX)) {
        HDC_LOG_ERR("Input path isn't dir, please check. (path=\"%s\")\n", plocal_dir);
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((ret = get_hdc_capacity(&capacity)) != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling get_hdc_capacity error. (hdcError_t=%d)\n", ret);
        goto error0;
    }

    p_sndbuf = malloc((uint32_t)capacity.maxSegment);
    if (p_sndbuf == NULL) {
        HDC_LOG_ERR("Calling malloc p_sndbuf failed.\n");
        ret = DRV_ERROR_MALLOC_FAIL;
        goto error0;
    }
    local_dir = (char *)malloc(PATH_MAX);
    if (local_dir == NULL) {
        HDC_LOG_ERR("Calling malloc local_dir failed.\n");
        ret = DRV_ERROR_MALLOC_FAIL;
        goto error1;
    }

    if (strcpy_s(local_dir, PATH_MAX, plocal_dir) != 0) {
        HDC_LOG_ERR("Calling strcpy_s error. (strerror=\"%s\")\n", strerror(errno));
        ret = DRV_ERROR_INVALID_VALUE;
        goto error2;
    }

    if (*(local_dir + strlen(local_dir) - 1) == '/') {
        *(local_dir + strlen(local_dir) - 1) = '\0';
    }

    ret = send_cmd(session, p_sndbuf, (int)capacity.maxSegment, local_dir, pdst_path);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling send_cmd error. (local_dir=\"%s\"; dst_path=\"%s\")\n", local_dir, pdst_path);
        goto error2;
    }

    ret = recv_reply(session, &is_rcvok);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling recv_reply error.\n");
        goto error2;
    }

    if (is_rcvok == FILE_OPT_WRPTH) {
        HDC_LOG_ERR("Create destination path error. (local_dir=\"%s\"; dst_path=\"%s\")\n", local_dir, pdst_path);
        ret = DRV_ERROR_INVALID_VALUE;
        goto error2;
    }

    ret = send_current_dir(session, local_dir, PATH_MAX, pdst_path, PATH_MAX, is_count, progress_notifier);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling send_current_dir error. (local_dir=\"%s\"; dst_path=\"%s\")\n", local_dir, pdst_path);
    }

error2:
    free(local_dir);
    local_dir = NULL;
error1:
    free(p_sndbuf);
    p_sndbuf = NULL;

error0:
    return ret;
}

hdcError_t drvHdcSendDir(signed int peer_node, signed int peer_devid, const char *plocal_dir, const char *pdst_path,
                         void (*progress_notifier)(struct drvHdcProgInfo *))
{
    HDC_CLIENT client = NULL;
    HDC_SESSION session = NULL;
    hdcError_t ret;
    struct drvHdcCapacity capacity = {0};
    char *p_sndbuf = NULL;
    signed int is_null;

    /* progress_notifier is designed as a hook-function,could be null for real-use */
    is_null = (plocal_dir == NULL) || (pdst_path == NULL);
    if (is_null != 0) {
        HDC_LOG_ERR("Input parameter is error.\n");
        return DRV_ERROR_INVALID_VALUE;
    }

    if ((ret = drvHdcClientCreate(&client, 1, HDC_SERVICE_TYPE_FILE_TRANS, 0)) != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcClientCreate error. (hdcError_t=%d)\n", ret);
        return ret;
    }

    if ((ret = drvHdcSessionConnect(peer_node, peer_devid, client, &session)) != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling drvHdcSessionConnect error. (hdcError_t=%d)\n", ret);
        (void)drvHdcClientDestroy(client);
        return ret;
    }

    (void)drvHdcSetSessionReference(session);
    ret = send_dir_in_session(session, plocal_dir, pdst_path, DIR_SEND_MAX_DEPTH, progress_notifier);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling send_dir_in_session error.\n");
        goto out;
    }

    if ((ret = get_hdc_capacity(&capacity)) != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling get_hdc_capacity error. (hdcError_t=%d)\n", ret);
        goto out;
    }

    p_sndbuf = malloc((uint32_t)capacity.maxSegment);
    if (p_sndbuf == NULL) {
        HDC_LOG_ERR("Calling malloc error. (strerror=\"%s\")\n", strerror(errno));
        ret = DRV_ERROR_MALLOC_FAIL;
        goto out;
    }

    ret = send_end(session, p_sndbuf, (int)capacity.maxSegment);
    if (ret != DRV_ERROR_NONE) {
        HDC_LOG_ERR("Calling send_end error.\n");
    }

    free(p_sndbuf);
    p_sndbuf = NULL;

out:
    (void)drvHdcSessionClose(session);
    (void)drvHdcClientDestroy(client);

    return ret;
}