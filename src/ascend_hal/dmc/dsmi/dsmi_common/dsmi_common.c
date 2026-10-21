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
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <pwd.h>
#include <linux/limits.h>

#include "dev_mon_log.h"
#include "dsmi_common_interface.h"
#include "dm_udp.h"
#include "dm_hdc.h"
#include "mmpa_api.h"
#include "ascend_hal.h"
#include "ascend_inpackage_hal.h"
#include "dsmi_product.h"
#include "hdc_user_interface.h"
#include "dms_user_interface.h"

#ifdef IAM_CONFIG
#include "dev_mon_iam.h"
#include "dm_iam.h"
#define INVALID_FD (-1)
#endif

#include "dsmi_common.h"
#include "drvdmp_adapt.h"

#include "udis_user.h"
#ifdef CFG_FEATURE_SUPPORT_UDIS
#include "device_monitor_type.h"
#endif

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define DSMI_NOT_INIT_FLAG 0
#define DSMI_INIT_FLAG 1
#define DSMI_MSG_WAIT_MAX 60000
#define DSMI_MSG_WAIT_TIME_MSEC 1
#define DSMI_MSG_SEM_PERMISSION_RW_GROUP 0660
#define FILE_READ_RETRY_TIMES 2

STATIC unsigned int g_init_flag = DSMI_NOT_INIT_FLAG;
STATIC DM_CB_S *g_dm_cb = NULL;
STATIC pthread_mutex_t g_init_lock = PTHREAD_MUTEX_INITIALIZER;
#define DSMI_TIME_SECOND_TO_US_SCALE 1000000

STATIC LIST_T *g_cmd_req_list = NULL; /* cmd register list */

/* global variables definition */
STATIC DM_INTF_S *g_dsmi_intf = NULL;
STATIC unsigned int g_dsmi_msg_send_cnt = 0;
STATIC unsigned int g_dsmi_msg_recv_cnt = 0;
STATIC unsigned int g_dsmi_msg_recv_success_cnt = 0;
STATIC struct timespec g_dsmi_send_time = {
    .tv_sec = 0,
    .tv_nsec = 0,
};
STATIC struct timespec g_dsmi_recv_time = {
    .tv_sec = 0,
    .tv_nsec = 0,
};
STATIC struct timespec g_dsmi_recv_ok_time = {
    .tv_sec = 0,
    .tv_nsec = 0,
};
STATIC struct timespec g_dsmi_timeout_time = {
    .tv_sec = 0,
    .tv_nsec = 0,
};

#define DSMI_FILE_NULL(pfile)                                 \
    do {                                                      \
        if ((pfile) == NULL) {                                \
            DEV_MON_ERR(" local copy file param is null \n"); \
            return DRV_ERROR_PARA_ERROR;                      \
        }                                                     \
    } while (0)

STATIC int dsmi_dmp_buff_cmp(const void *item1, const void *item2)
{
    if (item1 == item2) {
        return 0;
    }
    return -1;
}

STATIC void dsmi_free_dmp_buff(void *item)
{
    DSMI_DMP_COMMAND_ST *dmp = (DSMI_DMP_COMMAND_ST *)item;

    if (dmp != NULL) {
        if (dmp->send_msg.data != NULL) {
            free(dmp->send_msg.data);
            dmp->send_msg.data = NULL;
        }
        dmp->send_msg.data_len = 0;

        if (dmp->recv_msg.data != NULL) {
            free(dmp->recv_msg.data);
            dmp->recv_msg.data = NULL;
        }
        dmp->recv_msg.data_len = 0;
        free(dmp);
        dmp = NULL;
    }
}

void dsmi_cmd_req_free(const void *item)
{
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    int find = 0;

    list_iter_init(g_cmd_req_list, &iter);
    while ((node = list_iter_next(&iter)) != NULL) {
        if (list_to_item(node) == item) {
            find = 1;
            break;
        }
    }

    if (find != 0) {
        /* if no retry, just delete the node from pending_list */
        (void)list_remove_by_tag(g_cmd_req_list, item, dsmi_dmp_buff_cmp);
    }

    list_iter_destroy(&iter);

    return;
}

STATIC void dsmi_msg_recev(DM_INTF_S *intf, DM_RECV_ST *recv, void *user_data, int data_len)
{
    int ret = 0;
    int find = 0;
    LIST_NODE_T *node = NULL;
    LIST_ITER_T iter = {0};
    DSMI_DFT_RES_CMD *result_msg = NULL;
    DSMI_COMMAND_CTL_ST *user_send_data = NULL;
    DSMI_DMP_COMMAND_ST *dmp_cmd = NULL;

    DEV_MON_INFO("start dsmi_msg_recev\n");

    (void)clock_gettime(CLOCK_MONOTONIC, &g_dsmi_recv_time);
    g_dsmi_msg_recv_cnt++;

    if ((intf == NULL) || (recv == NULL)) {
        DEV_MON_ERR("dsmi_msg_recev parameter null.\n");
        return;
    }

    result_msg = (DSMI_DFT_RES_CMD *)recv->msg.data;
    if (result_msg == NULL) {
        DEV_MON_ERR("recv msg data null.\n");
        return;
    }
    if (data_len != (int)sizeof(DSMI_COMMAND_CTL_ST)) {
        DEV_MON_ERR("Invalid data length. (data_len=%d, expected=%d)\n", data_len, (int)sizeof(DSMI_COMMAND_CTL_ST));
        return;
    }
    user_send_data = (DSMI_COMMAND_CTL_ST *)user_data;
    if (user_send_data == NULL) {
        DEV_MON_ERR("user_data data null.\n");
        return;
    }

    dmp_cmd = user_send_data->dmp_cmd;

    list_iter_init(g_cmd_req_list, &iter);
    while ((node = list_iter_next(&iter)) != NULL) {
        if (list_to_item(node) == dmp_cmd) {
            find = 1;
            break;
        }
    }

    if (find != 0) {
        /* copy response data to buffer */
        if (dmp_cmd->recv_msg.data_len < recv->msg.data_len) {
            ret = memcpy_s((void *)dmp_cmd->recv_msg.data, dmp_cmd->recv_msg.data_len, (void *)result_msg,
                           dmp_cmd->recv_msg.data_len);
            if (ret != 0) {
                list_iter_destroy(&iter);
                DEV_MON_ERR("memcpy_s recv_msg.data fail:%d errno:%d.\n", ret, errno);
                return;
            }
        } else {
            ret = memcpy_s((void *)dmp_cmd->recv_msg.data, dmp_cmd->recv_msg.data_len, (void *)result_msg,
                           recv->msg.data_len);
            if (ret != 0) {
                list_iter_destroy(&iter);
                DEV_MON_ERR("memcpy_s recv_msg.data fail:%d errno:%d.\n", ret, errno);
                return;
            }
        }

        mb();
        /* already recv response data ,notify main process */
        dmp_cmd->recv_success = TRUE;
        (void)clock_gettime(CLOCK_MONOTONIC, &g_dsmi_recv_ok_time);
        g_dsmi_msg_recv_success_cnt++;
    }
    list_iter_destroy(&iter);
}

STATIC int dsmi_check_out_valid(DSMI_DFT_RES_CMD *response_msg, unsigned short opcode)
{
    DRV_CHECK_RETV((response_msg != NULL), DRV_ERROR_PARA_ERROR)
    if (response_msg->error_code != 0) {
        if (response_msg->error_code != DRV_ERROR_NOT_EXIST) {
            DEV_MON_EX_NOTSUPPORT_ERR(response_msg->error_code,
                                      "recv msg data error code %d, recv msg data opcode 0x%x, opcode 0x%x.\n",
                                      response_msg->error_code, response_msg->opcode, opcode);
        }
        return response_msg->error_code;
    }

    if (response_msg->opcode != opcode) {
        DEV_MON_ERR("recv msg data opcode not match 0x%x, opcode 0x%x.\n", response_msg->opcode, opcode);
        return DRV_ERROR_INNER_ERR;
    }

    return 0;
}
#ifdef CFG_FEATURE_DMP_UDP
STATIC int dsmi_init_udp_dest_addr(DM_ADDR_ST *dest_addr, int device_id)
{
#ifdef CFG_FEATURE_DMP_UDP_DOUBLE_SOCKET
    uid_t cur_uid;
    struct passwd *user = NULL;
#endif
    int ret;
    DM_UDP_ADDR_ST *udp_addr = NULL;
    const char *dmp_socket_path = DMP_SERVER_PATH_SERVICE;

    if (dest_addr == NULL || device_id < 0) {
        DEV_MON_ERR("devid %d dsmi_init_dest_addr parameter error!\n", device_id);
        return DRV_ERROR_PARA_ERROR;
    }

#ifdef CFG_FEATURE_DMP_UDP_DOUBLE_SOCKET
    user = getpwnam(DM_USER);
    cur_uid = user_prop_check();
    if ((cur_uid == ROOT_USER) || ((user != NULL) && (cur_uid == user->pw_uid))) {
        dmp_socket_path = DMP_SERVER_PATH_MANAGEMENT;
    }
#endif

    udp_addr = (DM_UDP_ADDR_ST *)dest_addr;
    udp_addr->addr_type = DM_UDP_ADDR_TYPE;
    udp_addr->channel = DM_UDP_CHANNEL;
    udp_addr->sock_addr.sun_family = AF_LOCAL;
    udp_addr->dev_id = device_id;

    ret = strcpy_s(udp_addr->sock_addr.sun_path, sizeof(udp_addr->sock_addr.sun_path), dmp_socket_path);
    if (ret != 0) {
        DEV_MON_ERR("strcpy_s error. (ret=%d).\n", ret);
        return ret;
    }

    return 0;
}
#elif defined(IAM_CONFIG)
STATIC int dsmi_init_iam_dest_addr(DM_ADDR_ST *dest_addr, int device_id, const DM_MSG_ST *msg)
{
    int ret;
    DSMI_CMD_CODE *send_msg = NULL;
    DM_IAM_ADDR_ST *iam_addr = (DM_IAM_ADDR_ST *)dest_addr;

    send_msg = (DSMI_CMD_CODE *)msg->data;
    ret = memset_s(iam_addr, sizeof(DM_IAM_ADDR_ST), 0, sizeof(DM_IAM_ADDR_ST));
    DRV_CHECK_RETV_DO_SOMETHING(ret == 0, DRV_ERROR_INNER_ERR, DEV_MON_ERR("DSMI call safe fun fail, ret = %d\n", ret));

    iam_addr->addr_type = DM_IAM_ADDR_TYPE;
    iam_addr->channel = DM_IAM_CHANNEL;
    iam_addr->src_devid = device_id;
    iam_addr->iam_type = IAM_CLIENT;
    iam_addr->file_type = send_msg->optype;

    return 0;
}
#else
STATIC int dsmi_init_hdc_dest_addr(DM_ADDR_ST *dest_addr, int device_id)
{
    int ret;
    DM_HDC_ADDR_ST *hdc_addr = (DM_HDC_ADDR_ST *)dest_addr;

    ret = memset_s(hdc_addr, sizeof(DM_HDC_ADDR_ST), 0, sizeof(DM_HDC_ADDR_ST));
    DRV_CHECK_RETV_DO_SOMETHING(ret == 0, DRV_ERROR_INNER_ERR,
                                DEV_MON_ERR("DSMI call safe fun fail. (ret = %d)\n", ret));

    hdc_addr->addr_type = DM_HDC_ADDR_TYPE;
    hdc_addr->channel = DM_HDC_CHANNEL;
    hdc_addr->hdc_type = 0;
    hdc_addr->peer_node = 0;
    hdc_addr->peer_devid = device_id;

    return 0;
}
#endif

#ifdef CFG_FEATURE_POWER_COMMAND

STATIC void *g_sysctr_so_handle = NULL;

int dsmi_invoke_os_cmd(DSMI_POWER_STATE cmd_type)
{
    int ret;
    SYS_CMD_INTERFACE sys_cmd_interface = NULL;

    g_sysctr_so_handle = dlopen(LIB_SYS_CTR_SO_PATH, RTLD_NOW | RTLD_GLOBAL);
    if (g_sysctr_so_handle == NULL) {
        DEV_MON_ERR("dlopen %s failed, %s.\n", LIB_SYS_CTR_SO_PATH, dlerror());
        return DRV_ERROR_INVALID_HANDLE;
    }

    if (cmd_type == POWER_STATE_POWEROFF) {
        sys_cmd_interface = (SYS_CMD_INTERFACE)dlsym(g_sysctr_so_handle, SYS_SHUTDOWN);
    } else if (cmd_type == POWER_STATE_RESET) {
        sys_cmd_interface = (SYS_CMD_INTERFACE)dlsym(g_sysctr_so_handle, SYS_REBOOT);
    } else {
        (void)dlclose(g_sysctr_so_handle);
        DEV_MON_ERR("cmd_type(%d) is invalid.\n", cmd_type);
        return DRV_ERROR_PARA_ERROR;
    }

    if (sys_cmd_interface == NULL) {
        (void)dlclose(g_sysctr_so_handle);
        DEV_MON_ERR("dlsym failed, errno(%d).\n", errno);
        return DRV_ERROR_INVALID_HANDLE;
    }

    sync();
    DEV_MON_EVENT("Use the SysReboot() to reset env.\n");
    DlogFlush();
    ret = sys_cmd_interface();
    DEV_MON_ERR("reboot failed, ret(%d), errno(%d).\n", ret, errno);
    (void)dlclose(g_sysctr_so_handle);
    return ret;
}

int dsmi_raise_script(const char *path, char **argv)
{
    int status;
    int err_buf;
    pid_t pid;
    pid_t wait_ppid;
    unsigned int status_child;

    if (path == NULL || argv == NULL) {
        DEV_MON_ERR("parameter is null. (path=%d; argv=%d)\n", path != NULL, argv != NULL);
        return DRV_ERROR_PARA_ERROR;
    }

#ifndef DEV_MON_UT
    pid = fork();
    if (pid < 0) {
        err_buf = errno;
        DEV_MON_ERR("dsmi fork fail.error:%d\n", err_buf);
        return err_buf;
    } else if (pid == 0) {
        if (execv(path, argv) != 0) {
            err_buf = errno;
            DEV_MON_ERR("dsmi execv. (strerror=%s; error=%d).\n", strerror(errno), err_buf);
            _exit(-1);
        }
    } else {
        do {
            wait_ppid = waitpid(pid, &status, 0);
        } while ((wait_ppid < 0) && (errno == EINTR));

        status_child = WIFEXITED(status);
        if (status_child == 0) {
            DEV_MON_ERR("dsmi raise script failed, execve failed, status_child(%u).\n", status_child);
            return -1;
        }

        status_child = WEXITSTATUS(status);
        if (status_child != 0) {
            DEV_MON_ERR("dsmi raise script failed, cmd run failed, status_child(%u).\n", status_child);
            return -1;
        }
    }
#endif

    return DRV_ERROR_NONE;
}
#endif

#if !((defined CFG_SOC_PLATFORM_MINI) && (defined UPGRADE_DEVICE))
STATIC int dsmi_check_device_exist(int device_id, const int *device_list, int device_count)
{
    int i;
    for (i = 0; i < device_count; i++) {
        if (device_id == device_list[i]) {
            return 0;
        }
    }

    return DRV_ERROR_INVALID_DEVICE;
}
#endif

int dsmi_is_null_check(const char *ptr)
{
    return ptr == NULL ? 1 : 0;
}

int dsmi_check_device_id(int device_id)
{
#if ((defined CFG_SOC_PLATFORM_MINI) && (defined UPGRADE_DEVICE))
    /* when the DSMI module operates on the device side, there is no need to verify the validity of the device ID */
    (void)(device_id);
    return 0;
#else
    int ret;
    int device_count = 0;
    int *device_list = NULL;
    int i = 0;
    int device_id_temp;

    /* since the upper 16 bits of the device ID are used for other functions,
    only the lower 16 bits are checked when examining the device ID */
    device_id_temp = ((unsigned int)device_id) & 0xFFFF;

    ret = dsmi_get_device_count(&device_count);
    CHECK_DEVICE_BUSY(device_id, ret);
    if (ret != 0 || (device_count == 0)) {
        DEV_MON_ERR("devid %d dsmi_get_device_count,test_fail, ret  = %d, device_count = %d\n", device_id_temp, ret,
                    device_count);
        return DRV_ERROR_INVALID_DEVICE;
    }

    device_list = (int *)malloc((unsigned long)device_count * sizeof(int));
    if (device_list == NULL) {
        DEV_MON_ERR("devid %d malloc fail\n", device_id_temp);
        return DRV_ERROR_MALLOC_FAIL;
    }

    ret = memset_s(device_list, (unsigned long)device_count * sizeof(int), INVALID_DEVICE_ID,
                   (unsigned long)device_count * sizeof(int));
    if (ret != 0) {
        DEV_MON_ERR("devid %d memset fail, ret = %d\n", device_id_temp, ret);
        goto check_device_id_resource_free;
    }

    ret = dsmi_list_device(device_list, device_count);
    if (ret != 0) {
        DEV_MON_ERR("devid %d get device list error, ret  = %d\n", device_id_temp, ret);
        goto check_device_id_resource_free;
    }

    ret = dsmi_check_device_exist(device_id_temp, device_list, device_count);
    if (ret == 0) {
        goto check_device_id_resource_free;
    }

    DEV_MON_ERR("the count of device is 0x%x ,all validity device id is as follow\n", device_count);
    for (i = 0; i < device_count; i++) {
        DEV_MON_ERR("validity device id is 0x%x\n", device_list[i]);
    }

    DEV_MON_ERR("Device id does not exist. (device_id=%d)\n", device_id_temp);

check_device_id_resource_free:
    free(device_list);
    device_list = NULL;
    return ret;
#endif
}

DSMI_DMP_COMMAND_ST *dmp_command_init(unsigned int device_index, unsigned short opcode, unsigned char optype,
                                      unsigned short input_len, unsigned short output_len)
{
    DSMI_DMP_COMMAND_ST *dmp = NULL;
    int ret;
    DSMI_CMD_CODE *send_msg = NULL;

    DRV_CHECK_RETV((output_len < MAX_OUTPUT_LEN), NULL);
    DRV_CHECK_RETV((input_len <= (USHORT_MAX - sizeof(DSMI_CMD_CODE))), NULL);
    ret = dsmi_init();
    DRV_CHECK_RETV_DO_SOMETHING((ret == OK), NULL, DEV_MON_ERR("call dsmi_init failed and ret=%d.\n", ret));

    dmp = (DSMI_DMP_COMMAND_ST *)malloc(sizeof(DSMI_DMP_COMMAND_ST));
    DRV_CHECK_RETV(dmp, NULL);
    ret = memset_s(dmp, sizeof(DSMI_DMP_COMMAND_ST), 0, sizeof(DSMI_DMP_COMMAND_ST));
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), NULL, free(dmp); dmp = NULL;
                                DEV_MON_ERR("devid %d memset_s failed, ret = %d\r\n", device_index, ret));

    dmp->send_msg.data_len = (unsigned short)(sizeof(DSMI_CMD_CODE) + input_len);
    dmp->recv_msg.data_len = (unsigned short)(sizeof(DSMI_DFT_RES_CMD) + output_len);

    DRV_CHECK_RETV_DO_SOMETHING((dmp->send_msg.data_len < MAX_INPUT_LEN), NULL, free(dmp); dmp = NULL);
    dmp->send_msg.data = (unsigned char *)malloc(dmp->send_msg.data_len);
    DRV_CHECK_RETV_DO_SOMETHING(((dmp->send_msg.data) != NULL), NULL, free(dmp); dmp = NULL;
                                DEV_MON_ERR("devid %d malloc failed, ret = %d\r\n", device_index, ret));

    ret = memset_s(dmp->send_msg.data, dmp->send_msg.data_len, 0, dmp->send_msg.data_len);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), NULL, dsmi_free_dmp_buff(dmp); dmp = NULL;
                                DEV_MON_ERR("devid %d memset_s failed, ret = %d\r\n", device_index, ret));

    dmp->recv_msg.data = (unsigned char *)malloc(dmp->recv_msg.data_len);
    DRV_CHECK_RETV_DO_SOMETHING(((dmp->recv_msg.data) != NULL), NULL, dsmi_free_dmp_buff(dmp); dmp = NULL;
                                DEV_MON_ERR("devid %d malloc failed, ret = %d\r\n", device_index, ret));

    ret = memset_s(dmp->recv_msg.data, dmp->recv_msg.data_len, 0, dmp->recv_msg.data_len);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), NULL, dsmi_free_dmp_buff(dmp); dmp = NULL;
                                DEV_MON_ERR("devid %d memset_s failed, ret = %d\r\n", device_index, ret));

    ret = list_append(g_cmd_req_list, dmp);
    DRV_CHECK_RETV_DO_SOMETHING((ret == 0), NULL, dsmi_free_dmp_buff(dmp); dmp = NULL;
                                DEV_MON_ERR("devid %d list_append_failed, ret = %d\r\n", device_index, ret));

    dmp->device_index = (device_index & 0xFFFF);
    dmp->op_code = opcode;
    send_msg = (DSMI_CMD_CODE *)(dmp->send_msg.data);
    send_msg->lun = HEAD_LUN;
    send_msg->optype = optype;
    send_msg->opcode = opcode;
    send_msg->offset = 0;
    send_msg->length = input_len;
    return dmp;
}

STATIC int dsmi_wait_receive(struct dsmi_dmp_command_st *dmp)
{
    int send_msg_wait = 0;

    /* wait receive success, 60s */
    while (dmp->recv_success != TRUE) {
        (void)mmSleep(DSMI_MSG_WAIT_TIME_MSEC);
        send_msg_wait++;

        if (send_msg_wait > DSMI_MSG_WAIT_MAX) {
            (void)clock_gettime(CLOCK_MONOTONIC, &g_dsmi_timeout_time);
            DEV_MON_ERR("DSMI recv timeout."
                        " (dev_id=%u; recv_data_len=%u; recv_cnt=%u; recv_succ_cnt=%u; send_cnt=%u)\n",
                        dmp->device_index, dmp->recv_msg.data_len, g_dsmi_msg_recv_cnt, g_dsmi_msg_recv_success_cnt,
                        g_dsmi_msg_send_cnt);
            DEV_MON_ERR("Time Consumed."
                        " (send_time=%lds, %ldns; recv_time=%lds, %ldns; reok_time=%lds, %ldns; tout_time=%lds, %ldns)",
                        g_dsmi_send_time.tv_sec, g_dsmi_send_time.tv_nsec, g_dsmi_recv_time.tv_sec,
                        g_dsmi_recv_time.tv_nsec, g_dsmi_recv_ok_time.tv_sec, g_dsmi_recv_ok_time.tv_nsec,
                        g_dsmi_timeout_time.tv_sec, g_dsmi_timeout_time.tv_nsec);
            return DRV_ERROR_WAIT_TIMEOUT;
        }
    }

    return 0;
}

STATIC int _dsmi_send_msg_rec_res(struct dsmi_dmp_command_st *dmp)
{
    int ret;
    int ret_r;
    mmTimeval start = {0};
    mmTimeval end = {0};
    DM_ADDR_ST dest_addr = {0};
    DSMI_COMMAND_CTL_ST cmd_ctl = {NULL, 0};

    DRV_CHECK_RETV((dmp != NULL), DRV_ERROR_PARA_ERROR);
    DRV_CHECK_RETV((dmp->recv_msg.data != NULL), DRV_ERROR_PARA_ERROR);
    DRV_CHECK_RETV((dmp->send_msg.data != NULL), DRV_ERROR_PARA_ERROR);

    cmd_ctl.dmp_cmd = dmp;

    /* init dest address information */
#ifdef CFG_FEATURE_DMP_UDP
    ret = dsmi_init_udp_dest_addr(&dest_addr, dmp->device_index);
#elif defined(IAM_CONFIG)
    ret = dsmi_init_iam_dest_addr(&dest_addr, dmp->device_index, &(dmp->send_msg));
#else
    ret = dsmi_init_hdc_dest_addr(&dest_addr, (int)dmp->device_index);
#endif
    if (ret != 0) {
        DEV_MON_ERR("call dsmi_init_dest_addr error:%d.\n", ret);
        return ret;
    }

    ret_r = mmGetTimeOfDay(&start, NULL);
    DRV_CHECK_CHK(ret_r == 0);
    /* send msg to dest addr */
    ret = dev_mon_send_request(g_dsmi_intf, &dest_addr, sizeof(DM_ADDR_ST), &(dmp->send_msg), dsmi_msg_recev,
                               (void *)&cmd_ctl, sizeof(DSMI_COMMAND_CTL_ST));
    if (ret != 0) {
        if (ret == DRV_ERROR_REMOTE_NO_SESSION) {
            DEV_MON_WARNING("dsmi/dmp aisle is busy, not available hdc session, please try later.\n");
        } else {
            ret = DRV_ERROR_SEND_MESG;
            DEV_MON_ERR("call dev_mon_send_request error:%d.\n", ret);
        }
        return ret;
    }

    (void)clock_gettime(CLOCK_MONOTONIC, &g_dsmi_send_time);
    g_dsmi_msg_send_cnt++;
    DEV_MON_INFO("finish call dev_mon_send_request\n");

    ret = dsmi_wait_receive(dmp);
    if (ret != 0) {
        DEV_MON_ERR("dsmi wait receive fail, ret = %d.\n", ret);
        return ret;
    }

    ret_r = mmGetTimeOfDay(&end, NULL);
    DRV_CHECK_CHK(ret_r == 0);

    DEV_MON_DEBUG(
        "Time consumption option. (code=0x%X; time_cost=%uus)\n", dmp->op_code,
        (unsigned int)((end.tv_sec - start.tv_sec) * DSMI_TIME_SECOND_TO_US_SCALE + (end.tv_usec - start.tv_usec)));

    /* result handle */
    ret = dsmi_check_out_valid((DSMI_DFT_RES_CMD *)dmp->recv_msg.data, dmp->op_code);
    if (ret != 0 && ret != DRV_ERROR_NOT_EXIST) {
        DEV_MON_EX_NOTSUPPORT_ERR(ret, "call dsmi_check_out_valid error:%d.\n", ret);
    }
    return ret;
}

int dsmi_send_msg_rec_res(struct dsmi_dmp_command_st *dmp)
{
    int ret, retry = 0;

    while (1) {
        ret = _dsmi_send_msg_rec_res(dmp);
        if (ret == 0) {
            return 0;
        } else if ((ret == DRV_ERROR_SEND_MESG) || (ret == DRV_ERROR_REMOTE_NO_SESSION)) {
            if (retry++ >= 3) { /* retry 3 count */
                break;
            }

            (void)sleep(1);
        } else {
            break;
        }
    }

    return ret;
}

void clear_all_blank(char *str)
{
    unsigned int i = 0;
    unsigned int j = 0;
    DRV_CHECK_RET(str != NULL);
    unsigned int len = (unsigned int)strnlen(str, MAX_LINE_LEN);

    for (i = 0; ((i < len) && (j < len)); i++) {
        if (str[i] != ' ') {
            str[j++] = str[i];
        }
    }

    str[j] = '\0';
}

int split_by_char(const char *src, char *path, unsigned int path_len_max, char *value, unsigned int value_len_max,
                  char split_char)
{
    char *split_point = NULL;
    char *end_char = NULL;
    char str_tmp[MAX_LINE_LEN + 1] = {0};
    int ret;

    // # is  exegesis
    if ((src == NULL) || (*src == '#')) {
        DEV_MON_ERR(" fun split_by_char  param is null \n");
        return DRV_ERROR_PARA_ERROR;
    }

    if ((path_len_max != PATH_MAX) || (value_len_max != PATH_MAX)) {
        DEV_MON_ERR("len is wrong, path_len: %u, value_len: %u \n", path_len_max, value_len_max);
        return DRV_ERROR_FILE_OPS;
    }

    ret = memcpy_s(str_tmp, MAX_LINE_LEN, src, strlen(src));
    DRV_CHECK_RETV_DO_SOMETHING(ret == 0, DSMI_SAFE_FUN_FAIL,
                                DEV_MON_ERR("\rDSMI call safe fun fail, ret = %d\r\n", ret));

    str_tmp[MAX_LINE_LEN] = '\0';
    clear_all_blank((char *)str_tmp);
    if (strnlen(str_tmp, MAX_LINE_LEN + 1) >= MAX_LINE_LEN) {
        return DRV_ERROR_CONFIG_READ_FAIL;
    }
    split_point = strchr(str_tmp, split_char);
    end_char = strchr(str_tmp, '\0');
    if (split_point != NULL && end_char != NULL) {
        ret = strncpy_s(path, PATH_MAX, str_tmp, (size_t)(split_point - str_tmp));
        DRV_CHECK_RETV_DO_SOMETHING(ret == 0, DSMI_SAFE_FUN_FAIL,
                                    DEV_MON_ERR("\rDSMI call safe fun fail, ret = %d\r\n", ret));
        path[split_point - str_tmp] = '\0';

        ret = strncpy_s(value, PATH_MAX, split_point + 1, (size_t)(end_char - (split_point + 1)));
        DRV_CHECK_RETV_DO_SOMETHING(ret == 0, DSMI_SAFE_FUN_FAIL,
                                    DEV_MON_ERR("\rDSMI call safe fun fail, ret = %d\r\n", ret));
        value[end_char - split_point - 1] = '\0';
        return 0;
    }
    // there is no description of component names and types in this row; no symbols:
    DEV_MON_DEBUG(" fun split_by_char, src = %s, should contain : maybe have blank line in cfg file, is not problem\n",
                  str_tmp);
    return DRV_ERROR_CONFIG_READ_FAIL;
}

STATIC void get_file_uid_gid(uid_t *uid, gid_t *gid)
{
    unsigned int i;
    struct passwd *user = NULL;
    const char *necessary_users[] = {"HwDmUser", "HwSysUser", "HwBaseUser"};

    for (i = 0; i < (sizeof(necessary_users) / sizeof(necessary_users[0])); i++) {
        user = getpwnam(necessary_users[i]);
        if (user == NULL) {
            *uid = USER_ID;
            *gid = GROUP_ID;
            return;
        }

        user = NULL;
    }

    user = getpwnam(DM_USER);
    *uid = user->pw_uid;
    *gid = user->pw_gid;

    return;
}

STATIC int creat_local_file(int device_id, const char *dst_file_path, FILE **fp_w)
{
    int ret;
    int fd_w;
    uid_t uid;
    gid_t gid;

    ret = check_dst_file_path(device_id, dst_file_path);
    if (ret != 0) {
        DEV_MON_ERR("Call check_dst_file_path failed. (dev_id=%d; ret=%d)\n", device_id, ret);
        return ret;
    }

    dev_mon_fopen_creat(fp_w, dst_file_path, "wb");
    if (*fp_w == NULL) {
        DEV_MON_ERR("Open file failed. (dst_file_path=%s; errno=%d)\n", dst_file_path, errno);
        return DRV_ERROR_FILE_OPS;
    }

    fd_w = fileno(*fp_w);
    if (fd_w == -1) {
        DEV_MON_ERR("Call fileno failed. (errno=%d)\n", errno);
        (void)fclose(*fp_w);
        *fp_w = NULL;
        return DRV_ERROR_FILE_OPS;
    }

    get_file_uid_gid(&uid, &gid);

    ret = fchown(fd_w, uid, gid);
    if (ret != 0) {
        DEV_MON_ERR("Call fchown failed. (errno=%d)\n", errno);
        (void)fclose(*fp_w);
        *fp_w = NULL;
        return ret;
    }

    ret = fchmod(fd_w, S_IRGRP | S_IRUSR | S_IWUSR);
    if (ret != 0) {
        DEV_MON_ERR("Call fchmod failed. (errno=%d)\n", errno);
        (void)fclose(*fp_w);
        *fp_w = NULL;
        return ret;
    }

    return 0;
}

STATIC int read_copy_file(FILE *fp, long file_len, char *buffer)
{
    int ret, rw_len;
    int err;
    int retry_times;

    ret = fseek(fp, 0L, SEEK_SET);
    if (ret != 0) {
        DEV_MON_ERR("Call fseek failed. (errno=%d; ret=%d)\n", errno, ret);
        return DRV_ERROR_FILE_OPS;
    }

    for (retry_times = 1; retry_times <= FILE_READ_RETRY_TIMES; retry_times++) {
        rw_len = (int)fread(buffer, 1, (size_t)file_len, fp);
        err = ferror(fp);
        ret = feof(fp);
        if (file_len != rw_len || err != 0) {
            DEV_MON_ERR("Read file fail. (errno=%d; rw_len=%d, ret = %d, file_len = %d)\n", err, rw_len, ret, file_len);
            if (retry_times == FILE_READ_RETRY_TIMES) {
                return DRV_ERROR_FILE_OPS;
            }
        } else {
            buffer[file_len] = '\0';
            return 0;
        }
    }

    return 0;
}

STATIC int write_copy_file(int device_id, const char *dst_file, const char *buffer, long buff_len)
{
    int ret, rw_len;
    FILE *fp_w = NULL;

    ret = creat_local_file(device_id, (const char *)dst_file, &fp_w);
    if (ret != 0) {
        DEV_MON_ERR("Call creat_local_file failed. (ret=%d)\n", ret);
        return ret;
    }

    rw_len = (int)fwrite(buffer, 1, (size_t)buff_len, fp_w);
    if (buff_len != rw_len) {
        DEV_MON_ERR("Write file failed. (dst_file=%s; errno=%d; rw_len=%d; file_len=%ld)\n", dst_file, errno, rw_len,
                    buff_len);
        (void)fclose(fp_w);
        fp_w = NULL;
        return DRV_ERROR_FILE_OPS;
    }

    (void)fclose(fp_w);
    fp_w = NULL;

    ret = chmod(dst_file, S_IRUSR | S_IWUSR | S_IRGRP);
    if (ret != 0) {
        DEV_MON_ERR("Call chmod failed. (dst_file=%s; ret=%d; errno=%d; err_mesg=%s)\n", dst_file, ret, errno,
                    strerror(errno));
        return DRV_ERROR_FILE_OPS;
    }

    return 0;
}

int local_copy_file(int device_id, const char *src_file, const char *dst_file)
{
    FILE *fp_r = NULL;
    char *buffer = NULL;
    long flen;
    int ret;

    DSMI_FILE_NULL(src_file);
    DSMI_FILE_NULL(dst_file);

    if ((strlen(src_file) >= PATH_MAX) || (strlen(dst_file) >= PATH_MAX)) {
        DEV_MON_ERR("Parameter is invalid. (error=0x%x)\n", errno);
        return DRV_ERROR_PARA_ERROR;
    }

    dev_mon_fopen(&fp_r, src_file, "rb");
    DRV_CHECK_RETV_DO_SOMETHING((fp_r != NULL), DRV_ERROR_FILE_OPS, DEV_MON_ERR("DSMI dev_mon_fopen fail.\n"));

    ret = fseek(fp_r, 0, SEEK_END);
    if (ret != 0) {
        (void)fclose(fp_r);
        fp_r = NULL;
        DEV_MON_ERR("Call fseek failed. (file=%s; errno=%d; ret=%d)\n", src_file, errno, ret);
        return DRV_ERROR_FILE_OPS;
    }

    flen = ftell(fp_r);
    if ((flen <= 0) || (flen > LOCAL_COPY_FILE_SIZE_MAX)) {
        DEV_MON_ERR("Get file len failed. (errno=%d; flen=%ld)\n", errno, flen);
        (void)fclose(fp_r);
        fp_r = NULL;
        return DRV_ERROR_FILE_OPS;
    }

    buffer = (char *)calloc((unsigned long)(flen + 1), sizeof(char));
    DRV_CHECK_RETV_DO_SOMETHING(buffer != NULL, DRV_ERROR_MALLOC_FAIL, DEV_MON_ERR("Call calloc failed.\n");
                                (void)fclose(fp_r); fp_r = NULL);

    ret = read_copy_file(fp_r, flen, buffer);
    if (ret != 0) {
        (void)fclose(fp_r);
        fp_r = NULL;
        DSMI_FREE(buffer);
        DEV_MON_ERR("Read file failed. (file=%s, ret=%d)\n", src_file, ret);
        return DRV_ERROR_FILE_OPS;
    }
    (void)fclose(fp_r);
    fp_r = NULL;

    // maybe the  UPGRADE_DST_PATH already have the same file but is user need to upgrade
    // so copy file to buffer then overwrite the file in UPGRADE_DST_PATH
    // although user have put file to UPGRADE_DST_PATH
    ret = write_copy_file(device_id, dst_file, buffer, flen);
    if (ret != 0) {
        DSMI_FREE(buffer);
        DEV_MON_ERR("Write file failed. (device_id=%d; ret=%d)\n", device_id, ret);
        return DRV_ERROR_FILE_OPS;
    }
    DSMI_FREE(buffer);

    return 0;
}

int get_pcie_mode(void)
{
#ifdef CFG_EDGE_HOST
    return PCIE_EP_MODE;
#else
#ifdef UPGRADE_DEVICE
    return PCIE_RC_MODE;
#else
    return DRV_ERROR_INNER_ERR;
#endif
#endif
}

int dsmi_mutex_p(key_t sem_tag, int *sem_id, unsigned int timeout)
{
    int ret;
    int semid = 0;
    int semno = 0;
    int val;
    struct sembuf smbf[2] = {{0}}; // 2 smbf size

    DRV_CHECK_RETV(sem_id != NULL, -EINVAL);
#ifndef DEV_MON_UT
    ret = semget(sem_tag, 1, IPC_CREAT | IPC_EXCL | DSMI_MSG_SEM_PERMISSION_RW_GROUP);
    if (ret < 0) {
        if (errno == EEXIST) {
            semid = semget(sem_tag, 1, 0);
            if (semid < 0) {
                dev_upgrade_err("semget %d fail ret = %d\n", semid, ret);
                return -EINVAL;
            }
        } else {
            dev_upgrade_err("semget fail ret = %d\n", ret);
            return -EINVAL;
        }
    } else {
        /* initial creation of IPC semaphore */
        semid = ret;
        /* set semaphore value */
        val = 1;
        ret = semctl(semid, semno, SETVAL, val);
        if (ret < 0) {
            dev_upgrade_err("semctl fail, ret = 0x%x, errno=%d.\n", ret, errno);
            return -EINVAL;
        }
    }

    *sem_id = semid;

    smbf[0].sem_num = 0;
    smbf[0].sem_op = -1;
    smbf[0].sem_flg = (timeout == DSMI_MUTEX_WAIT_FOR_EVER) ? SEM_UNDO : (SEM_UNDO | IPC_NOWAIT);

    ret = semop(semid, smbf, 1);
    if (ret == 0) {
        return 0;
    } else if (errno == EAGAIN) {
        dev_upgrade_err("There are multiple same processes going on. (devid=0x%x)\n",
                        (sem_tag - DSMI_UPGRADE_LOCK_TAG));
    } else if (errno == EACCES) {
        dev_upgrade_err("Operation not permitted. (devid=0x%x)\n", (sem_tag - DSMI_UPGRADE_LOCK_TAG));
        return DRV_ERROR_OPER_NOT_PERMITTED;
    } else {
        dev_upgrade_err("semop fail ret = %d\n", ret);
    }
    return -EINVAL;
#endif
}

int dsmi_mutex_v(int sem_id)
{
    int ret;
    struct sembuf smbf;

    smbf.sem_num = 0;
    smbf.sem_op = 1;
    smbf.sem_flg = SEM_UNDO | IPC_NOWAIT;

#ifndef DEV_MON_UT

    ret = semop(sem_id, &smbf, 1);
    if (ret != 0) {
        dev_upgrade_err("semop fail! , ret = 0x%x\n", ret);
        return -EINVAL;
    }
    return ret;

#endif
}

DEV_INFO_MAIN_CMD_TYPE dsmi_get_dev_info_main_cmd_type(DSMI_MAIN_CMD main_cmd, unsigned int sub_cmd)
{
    if ((main_cmd >= DSMI_PRODUCT_MAIN_CMD_START) && (main_cmd <= DSMI_PRODUCT_MAIN_CMD_END)) {
        return MAIN_CMD_TYPE_PRODUCT;
    }

    if ((main_cmd == DSMI_MAIN_CMD_CHIP_INF) && (sub_cmd == DSMI_CHIP_INF_SUB_CMD_CUST_BOARD_INF)) {
        return MAIN_CMD_TYPE_HOST_DMP;
    }

#ifdef CFG_FEATURE_PCIE_LINK_INFO
    if ((main_cmd == DSMI_MAIN_CMD_PCIE) && (sub_cmd == DSMI_PCIE_SUB_CMD_PCIE_INFO)) {
        return MAIN_CMD_TYPE_HOST_DEVMNG;
    }
#endif

    if ((main_cmd == DSMI_MAIN_CMD_CHIP_INF) || (main_cmd == DSMI_MAIN_CMD_SVM) ||
        (main_cmd == DSMI_MAIN_CMD_VDEV_MNG) || (main_cmd == DSMI_MAIN_CMD_HOST_AICPU) ||
        ((main_cmd == DSMI_MAIN_CMD_TS) && (sub_cmd == (unsigned int)DSMI_TS_SUB_CMD_FFTS_TYPE)) ||
        ((main_cmd == DSMI_MAIN_CMD_TS) && (sub_cmd == (unsigned int)DSMI_TS_SUB_CMD_COMMON_MSG)) ||
        ((main_cmd == DSMI_MAIN_CMD_HCCS) && (sub_cmd == (unsigned int)DSMI_HCCS_CMD_GET_PING_INFO)) ||
        ((main_cmd == DSMI_MAIN_CMD_HCCS) && (sub_cmd == (unsigned int)DSMI_HCCS_CMD_GET_CREDIT_INFO)) ||
        ((main_cmd == DSMI_MAIN_CMD_TRS) && (sub_cmd == (unsigned int)DSMI_TRS_SUB_CMD_KERNEL_LAUNCH_MODE)) ||
        ((main_cmd == DSMI_MAIN_CMD_UB) && (sub_cmd == (unsigned int)DSMI_UB_INFO_SUB_CMD_PORT_STATUS)) ||
        ((main_cmd == DSMI_MAIN_CMD_UB) && (sub_cmd == (unsigned int)DSMI_UB_INFO_SUB_CMD_ID)) ||
        ((main_cmd == DSMI_MAIN_CMD_UB) && (sub_cmd == (unsigned int)DSMI_UB_INFO_SUB_CMD_URMA_DEV_NAME)) ||
        ((main_cmd == DSMI_MAIN_CMD_UPGRADE) && (sub_cmd == (unsigned int)DSMI_UPGRADE_SUB_TYPE_SWPLUGIN_POLICY))) {
        return MAIN_CMD_TYPE_HOST_DEVMNG;
    }

    return MAIN_CMD_TYPE_HOST_DMP;
}

STATIC void set_init_flag(unsigned int flag)
{
    g_init_flag = flag;
}
STATIC unsigned int get_init_flag(void)
{
    return g_init_flag;
}

#ifndef DEV_MON_UT
int user_prop_check(void)
{
    int uid;
    uid = (int)getuid();
    return uid;
}
#endif

STATIC void dm_command_time_out_print(const DM_MSG_ST *req, DM_MSG_ST *resp)
{
    (void)req;
    if ((resp != NULL) && (resp->data != NULL)) {
        DSMI_DFT_RES_CMD *recv_msg = (DSMI_DFT_RES_CMD *)(resp->data);
        recv_msg->error_code = DRV_ERROR_WAIT_TIMEOUT;
        DEV_MON_ERR("DSMI message receive timeout, recv_msg->opcode is 0x%x.\n", recv_msg->opcode);
    } else {
        DEV_MON_ERR("DSMI message receive timeout, recv_msg is NULL.\n");
    }
    return;
}

static int dsmi_init_channel(DM_ADDR_ST *my_addr)
{
    int ret;

#ifdef CFG_FEATURE_DMP_UDP
    const char *socket_name = DM_UDP_SERVICE_INTF;
    DM_UDP_ADDR_ST *udp_addr = (DM_UDP_ADDR_ST *)my_addr;
#endif
#ifdef IAM_CONFIG
    DM_IAM_ADDR_ST *iam_addr = (DM_IAM_ADDR_ST *)my_addr;
#endif
#ifdef CFG_FEATURE_DMP_HDC
    DM_HDC_ADDR_ST *hdc_addr = (DM_HDC_ADDR_ST *)my_addr;
#endif

    ret = memset_s(my_addr, sizeof(DM_ADDR_ST), 0, sizeof(DM_ADDR_ST));
    DRV_CHECK_RETV_DO_SOMETHING(ret == 0, DSMI_SAFE_FUN_FAIL,
                                DEV_MON_ERR("memcpy_s DM_ADDR_ST failed, ret = %d\n", ret));
#ifdef CFG_FEATURE_DMP_UDP

#ifdef CFG_FEATURE_DMP_UDP_DOUBLE_SOCKET
    uid_t cur_uid;
    struct passwd *user = NULL;

    user = getpwnam(DM_USER);
    cur_uid = user_prop_check();
    if ((cur_uid == ROOT_USER) || ((user != NULL) && (cur_uid == user->pw_uid))) {
        socket_name = DM_UDP_MANAGEMENT_INTF;
    }
#endif
    udp_addr->addr_type = DM_UDP_ADDR_TYPE;
    udp_addr->channel = DM_UDP_CHANNEL;
    udp_addr->service_type = DM_CLIENT;
    udp_addr->sock_addr.sun_family = AF_LOCAL;

    ret = dm_udp_init(&g_dsmi_intf, g_dm_cb, dm_command_time_out_print, my_addr, socket_name, (int)strlen(socket_name));
    if (ret) {
        DEV_MON_ERR("call dm_udp_init fail ret = %d.\n", ret);
        return ret;
    }
#endif

#ifdef IAM_CONFIG
    iam_addr = (DM_IAM_ADDR_ST *)my_addr;
    iam_addr->addr_type = DM_IAM_ADDR_TYPE;
    iam_addr->channel = DM_IAM_CHANNEL;
    iam_addr->iam_type = IAM_CLIENT;
    iam_addr->src_devid = 0;
    iam_addr->rpc_session_id = 0;
    iam_addr->rpc_req_id = 0;
    iam_addr->iam_res_fd = INVALID_FD;
    iam_addr->iam_queue = (void *)NULL;
    ret = dm_iam_init(&g_dsmi_intf, g_dm_cb, dm_command_time_out_print, my_addr, NULL, 0);
    if (ret != 0) {
        DEV_MON_ERR("call dm_iam_init fail ret = %d.\n", ret);
        return ret;
    }
#endif

#ifdef CFG_FEATURE_DMP_HDC
    hdc_addr = (DM_HDC_ADDR_ST *)my_addr;
    hdc_addr->addr_type = DM_HDC_ADDR_TYPE;
    hdc_addr->channel = DM_HDC_CHANNEL;
    hdc_addr->hdc_type = 0;
    hdc_addr->peer_node = 0;
    hdc_addr->peer_devid = 0;
    hdc_addr->session = (unsigned long long)NULL;
    ret = dm_hdc_init(&g_dsmi_intf, g_dm_cb, dm_command_time_out_print, my_addr, NULL, 0);
    if (ret) {
        DEV_MON_ERR("call dm_hdc_init fail ret = %d.\n", ret);
        return ret;
    }
#endif

    return 0;
}

int dsmi_init(void)
{
    int ret;
    DM_ADDR_ST my_addr;

    (void)pthread_mutex_lock(&g_init_lock);
    if (get_init_flag() == DSMI_INIT_FLAG) {
        (void)pthread_mutex_unlock(&g_init_lock);
        return 0;
    }

    DEV_MON_INFO("step in, init work start.\n");
    ret = dm_init((DM_CB_S **)&g_dm_cb);
    if (ret != 0) {
        (void)pthread_mutex_unlock(&g_init_lock);
        DEV_MON_ERR("call dm_init error ret = %d.\n", ret);
        return ret;
    }

    ret = list_create(&g_cmd_req_list, dsmi_dmp_buff_cmp, dsmi_free_dmp_buff);
    if (ret != 0) {
        dm_destroy((DM_CB_S *)g_dm_cb);
        g_dm_cb = NULL;
        (void)pthread_mutex_unlock(&g_init_lock);
        DEV_MON_ERR("create g_cmd_req_list fail,ret:%d\r\n", ret);
        return ret;
    }

    ret = slice_msg_list_init();
    if (ret != 0) {
        list_destroy(g_cmd_req_list);
        g_cmd_req_list = NULL;
        dm_destroy((DM_CB_S *)g_dm_cb);
        g_dm_cb = NULL;
        (void)pthread_mutex_unlock(&g_init_lock);
        DEV_MON_ERR("create slice_msg_list_init fail,ret:%d\r\n", ret);
        return ret;
    }

    ret = client_rsp_hashtable_init();
    if (ret != 0) {
        list_destroy(g_cmd_req_list);
        g_cmd_req_list = NULL;
        slice_msg_list_uninit();
        dm_destroy((DM_CB_S *)g_dm_cb);
        g_dm_cb = NULL;
        (void)pthread_mutex_unlock(&g_init_lock);
        DEV_MON_ERR("create client_rsp_hashtable fail,ret:%d\r\n", ret);
        return ret;
    }

    ret = dsmi_init_channel(&my_addr);
    if (ret != 0) {
        list_destroy(g_cmd_req_list);
        g_cmd_req_list = NULL;
        slice_msg_list_uninit();
        client_rsp_hashtable_uninit();
        dm_destroy((DM_CB_S *)g_dm_cb);
        g_dm_cb = NULL;
        (void)pthread_mutex_unlock(&g_init_lock);
        DEV_MON_ERR("call dsmi_init_channel error ret = %d.\n", ret);
        return ret;
    }

    set_init_flag(DSMI_INIT_FLAG);
    (void)pthread_mutex_unlock(&g_init_lock);
    DEV_MON_INFO("step out, init work has been done.\n");
    return 0;
}

#ifdef DEV_MON_ST
void dsmi_exit(void)
#else
static __attribute__((destructor)) void dsmi_exit(void)
#endif
{
    if (get_init_flag() == DSMI_NOT_INIT_FLAG) {
        return;
    }

    (void)usleep(DM_COMMON_INIT_EXIT_DELAY);

    if (g_dm_cb != NULL) {
        dm_destroy((DM_CB_S *)g_dm_cb);
        g_dm_cb = NULL;
    }

    if (g_cmd_req_list != NULL) {
        list_destroy(g_cmd_req_list);
        g_cmd_req_list = NULL;
    }
    slice_msg_list_uninit();
    client_rsp_hashtable_uninit();

    /* stop then fault event thread */
    dsmi_stop_fault_event_thread();

    set_init_flag(DSMI_NOT_INIT_FLAG);
    return;
}

int dsmi_udis_get_hbm_isolated_pages_info(int dev_id, unsigned char module_type,
                                          struct dsmi_ecc_pages_stru *pdevice_ecc_pages_statistics)
{
    int ret;
    struct udis_dev_info get_info = {0};

    ret = sprintf_s(get_info.name, UDIS_MAX_NAME_LEN, "ecc_his_cnt");
    if (ret < 0) {
        DEV_MON_WARNING("sprintf to udis get_info.name not success. (dev_id=%u;)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    get_info.module_type = UDIS_MODULE_MEMORY;
    ret = halUdisGetDeviceInfo((unsigned int)dev_id, &get_info);
    if (ret != 0) {
        return ret;
    }

    if (get_info.data_len != sizeof(struct dsmi_ecc_pages_stru)) {
        DEV_MON_WARNING("Expected data_len != actual data_len for isolated_pages_info. "
                        "(dev_id=%u; module_type=%u; data_len=%u; data_len_expected=%u)\n",
                        dev_id, module_type, get_info.data_len, (unsigned int)sizeof(struct dsmi_ecc_pages_stru));
        return DRV_ERROR_INNER_ERR;
    }

    ret = memcpy_s(pdevice_ecc_pages_statistics, sizeof(struct dsmi_ecc_pages_stru), get_info.data, get_info.data_len);
    if (ret != 0) {
        DEV_MON_WARNING("Memcpy to ecc_statistics_all not success. (dev_id=%u;)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return 0;
}

int dsmi_udis_get_single_ecc_info(int dev_id, struct ecc_common_data *dsmi_ecc_common_data_s)
{
    int ret;
    struct udis_dev_info get_info = {0};
    struct ecc_single_data_info single_ecc_data[HBM_ECC_RECORD_MAX_NUMS] = {0};
    int idx = 0;

    for (idx = 0; idx < SINGLE_ECC_INFO_BLOCK_NUMS; idx++) {
        ret = sprintf_s(get_info.name, UDIS_MAX_NAME_LEN, "%s%d", "single_ecc", idx);
        if (ret < 0) {
            DEV_MON_WARNING("sprintf to udis get_info.name not success. (dev_id=%u;)\n", dev_id);
            return DRV_ERROR_INVALID_HANDLE;
        }

        get_info.module_type = UDIS_MODULE_MEMORY;
        ret = halUdisGetDeviceInfo((unsigned int)dev_id, &get_info);
        if (ret != 0) {
            DEV_MON_WARNING("udis get single ecc info fail:(ret:%d name:%s)\n", ret, get_info.name);
            return ret;
        }

        if (get_info.data_len != (unsigned int)(sizeof(struct ecc_single_data_info) * SINGLE_ECC_INFO_BLOCK_NUMS)) {
            DEV_MON_WARNING("Expected data_len != actual data_len for single_ecc_info. "
                            "(dev_id=%u; data_len=%u; data_len_expected=%u)\n",
                            dev_id, get_info.data_len,
                            (unsigned int)(sizeof(struct ecc_single_data_info) * SINGLE_ECC_INFO_BLOCK_NUMS));
            return DRV_ERROR_INNER_ERR;
        }

        ret = memcpy_s(&single_ecc_data[idx * SINGLE_ECC_INFO_BLOCK_NUMS],
                       (unsigned int)(sizeof(struct ecc_single_data_info) * SINGLE_ECC_INFO_BLOCK_SIZE), get_info.data,
                       get_info.data_len);
        if (ret != 0) {
            DEV_MON_WARNING("Memcpy to ecc_common_data not success. (dev_id=%u;)\n", dev_id);
            return DRV_ERROR_INVALID_HANDLE;
        }
    }

    for (idx = 0; idx < HBM_ECC_RECORD_MAX_NUMS; idx++) {
        dsmi_ecc_common_data_s[idx].physical_addr = 0;
        dsmi_ecc_common_data_s[idx].stack_pc_id = single_ecc_data[idx].hbmc_id;
        dsmi_ecc_common_data_s[idx].reg_addr_h = single_ecc_data[idx].single_bit_high_addr;
        dsmi_ecc_common_data_s[idx].reg_addr_l = single_ecc_data[idx].single_bit_low_addr;
        dsmi_ecc_common_data_s[idx].ecc_count = single_ecc_data[idx].single_bit_count;
        dsmi_ecc_common_data_s[idx].timestamp = (int)single_ecc_data[idx].last_appear_time_stamp;
    }
    return 0;
}

int dsmi_udis_get_multi_ecc_info(int dev_id, struct ecc_common_data *dsmi_ecc_common_data_s)
{
    int ret;
    struct udis_dev_info get_info = {0};
    struct ecc_double_data_info double_ecc_data[HBM_ECC_RECORD_MAX_NUMS] = {0};
    int idx = 0;

    for (; idx < DOUBLE_ECC_INFO_BLOCK_NUMS; idx++) {
        ret = sprintf_s(get_info.name, UDIS_MAX_NAME_LEN, "%s%d", "multi_ecc", idx);
        if (ret < 0) {
            DEV_MON_WARNING("sprintf to udis get_info.name not success. (dev_id=%u;)\n", dev_id);
            return DRV_ERROR_INVALID_HANDLE;
        }

        get_info.module_type = UDIS_MODULE_MEMORY;
        ret = halUdisGetDeviceInfo((unsigned int)dev_id, &get_info);
        if (ret != 0) {
            DEV_MON_WARNING("udis get device info failed: (dev_id=%u)\n", dev_id);
            return ret;
        }

        if (get_info.data_len != (unsigned int)(sizeof(struct ecc_double_data_info) * DOUBLE_ECC_INFO_BLOCK_SIZE)) {
            DEV_MON_WARNING("Expected data_len != actual data_len for isolated_pages_info. "
                            "(dev_id=%u; data_len=%u; data_len_expected=%u)\n",
                            dev_id, get_info.data_len,
                            (unsigned int)(sizeof(struct ecc_double_data_info) * DOUBLE_ECC_INFO_BLOCK_SIZE));
            return DRV_ERROR_INNER_ERR;
        }

        ret = memcpy_s(&double_ecc_data[idx * DOUBLE_ECC_INFO_BLOCK_SIZE], get_info.data_len, get_info.data,
                       get_info.data_len);
        if (ret != 0) {
            DEV_MON_WARNING("Memcpy to ecc_common_data not success. (dev_id=%u;)\n", dev_id);
            return DRV_ERROR_INVALID_HANDLE;
        }
    }

    for (idx = 0; idx < HBM_ECC_RECORD_MAX_NUMS; idx++) {
        dsmi_ecc_common_data_s[idx].stack_pc_id = 0;
        dsmi_ecc_common_data_s[idx].ecc_count = 0;
        dsmi_ecc_common_data_s[idx].physical_addr = double_ecc_data[idx].physical_addr;
        dsmi_ecc_common_data_s[idx].timestamp = double_ecc_data[idx].timer_stamp;
        dsmi_ecc_common_data_s[idx].reg_addr_h = ((uint32_t)double_ecc_data[idx].row << HIGH_ADDR_COLUMN_BITS_COUNT) |
                                                 double_ecc_data[idx].column;
        dsmi_ecc_common_data_s[idx].reg_addr_l = ((uint32_t)double_ecc_data[idx].rank << LOW_ADDR_RANK_BITS_OFFSET) |
                                                 double_ecc_data[idx].bank;
    }

    return 0;
}

STATIC int dsmi_get_multi_ecc_time_block(int dev_id, int idx, struct udis_multi_ecc_time_data *multi_ecc_time_data)
{
    struct udis_dev_info get_info = {0};
    int ret;

    ret = sprintf_s(get_info.name, UDIS_MAX_NAME_LEN, "%s%d", "multi_time", idx);
    if (ret < 0) {
        DEV_MON_WARNING("sprintf to udis get_info.name failed. (dev_id=%u)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    get_info.module_type = UDIS_MODULE_MEMORY;
    ret = halUdisGetDeviceInfo((unsigned int)dev_id, &get_info);
    if (ret != 0) {
        return ret;
    }

    if (get_info.data_len != sizeof(unsigned int) * DOUBLE_ECC_TIME_INFO_BLOCK_SIZE) {
        DEV_MON_WARNING("Expected data_len != actual data_len for isolated_pages_info. "
                        "(dev_id=%u; data_len=%u; data_len_expected=%u)\n",
                        dev_id, get_info.data_len,
                        (unsigned int)(sizeof(unsigned int) * DOUBLE_ECC_TIME_INFO_BLOCK_SIZE));
        return DRV_ERROR_INNER_ERR;
    }

    ret = memcpy_s(&multi_ecc_time_data->multi_ecc_times[idx * DOUBLE_ECC_TIME_INFO_BLOCK_SIZE],
                   sizeof(unsigned int) * DOUBLE_ECC_TIME_INFO_BLOCK_SIZE, get_info.data, get_info.data_len);
    if (ret != 0) {
        DEV_MON_WARNING("Memcpy to ecc_common_data not success. (dev_id=%u;)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    return 0;
}

int dsmi_udis_multi_ecc_time_info(int dev_id, struct udis_multi_ecc_time_data *multi_ecc_time_data)
{
    int ret;
    struct udis_dev_info get_info = {0};
    int idx = 0;

    for (idx = 0; idx < DOUBLE_ECC_TIME_INFO_BLOCK_NUMS; idx++) {
        ret = dsmi_get_multi_ecc_time_block(dev_id, idx, multi_ecc_time_data);
        if (ret != 0) {
            return ret;
        }
    }
    ret = sprintf_s(get_info.name, UDIS_MAX_NAME_LEN, "mul_ecc_cnt");
    if (ret < 0) {
        DEV_MON_WARNING("sprintf to udis get_info.name not success. (dev_id=%u;)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }

    get_info.module_type = UDIS_MODULE_MEMORY;
    ret = halUdisGetDeviceInfo((unsigned int)dev_id, &get_info);
    if (ret != 0) {
        return ret;
    }

    if (get_info.data_len != sizeof(unsigned int)) {
        DEV_MON_WARNING("Expected data_len != actual data_len for isolated_pages_info. "
                        "(dev_id=%u; data_len=%u; data_len_expected=%u)\n",
                        dev_id, get_info.data_len, (unsigned int)sizeof(unsigned int));
        return DRV_ERROR_INNER_ERR;
    }

    ret = memcpy_s(&multi_ecc_time_data->multi_record_count, sizeof(unsigned int), get_info.data, get_info.data_len);
    if (ret != 0) {
        DEV_MON_WARNING("Memcpy to ecc_common_data not success. (dev_id=%u;)\n", dev_id);
        return DRV_ERROR_INVALID_HANDLE;
    }
    return 0;
}