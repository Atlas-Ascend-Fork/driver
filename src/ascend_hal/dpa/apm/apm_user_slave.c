/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "securec.h"
#include "apm_ioctl.h"

#define APM_BIND_RETRY_COUNT 2000
#define APM_BIND_RETRY_DELAY 100000

static void drv_fill_bind_para(struct apm_cmd_bind *para, struct drvBindHostpidInfo *info)
{
    para->devid = info->chip_id;
    para->proc_type = (int)info->cp_type;
    para->mode = info->mode;
    para->master_pid = info->host_pid;
    (void)memcpy_s(&para->slave_pid, sizeof(int), info->sign, sizeof(int));
    if (para->slave_pid == 0) {
        para->slave_pid = getpid();
    }
}

drvError_t drvBindHostPid(struct drvBindHostpidInfo info)
{
    struct apm_cmd_bind para;
    int ret, cnt = 0;
    bool retry = false;

    if ((info.len != PROCESS_SIGN_LENGTH) || (info.vfid != 0)) {
        apm_err("Invalid param. (sig_len=%u; proc_type=%d; vfid=%d)\n", info.len, (int)info.cp_type, info.vfid);
        return DRV_ERROR_PARA_ERROR;
    }

    drv_fill_bind_para(&para, &info);

    do {
        ret = apm_cmd_ioctl(APM_BIND, &para);
        cnt++;
        retry = ((ret != 0) && (errno == EAGAIN) && (cnt < APM_BIND_RETRY_COUNT));
        if (retry) {
            usleep(APM_BIND_RETRY_DELAY);
        }
    } while (retry);
    if ((ret != 0) && (ret != DRV_ERROR_NOT_SUPPORT)) {
        apm_err("Bind pid failed. (ret=%d; devid=%u; master_pid=%d; proc_type=%d; mode=%d)\n", ret, para.devid,
                para.master_pid, para.proc_type, para.mode);
    }

    return ret;
}

drvError_t drvUnbindHostPid(struct drvBindHostpidInfo info)
{
    struct apm_cmd_bind para;
    int ret;

    if ((info.len != PROCESS_SIGN_LENGTH) || (info.vfid != 0)) {
        apm_err("Invalid param. (sig_len=%u; proc_type=%d; vfid=%d)\n", info.len, (int)info.cp_type, info.vfid);
        return DRV_ERROR_PARA_ERROR;
    }

    drv_fill_bind_para(&para, &info);

    ret = apm_cmd_ioctl(APM_UNBIND, &para);
    if (ret != 0) {
        apm_err("Unbind pid failed. (ret=%d; devid=%u; master_pid=%d; proc_type=%d)\n", ret, para.devid,
                para.master_pid, para.proc_type);
        return ret;
    }

    return DRV_ERROR_NONE;
}

static inline int apm_trans_first_proc_type_from_bitmap(unsigned int proc_type_bitmap)
{
    int proc_type;

    for (proc_type = 0; proc_type < PROCESS_CPTYPE_MAX; proc_type++) {
        if ((proc_type_bitmap & (unsigned int)(0x1 << proc_type)) != 0) {
            break;
        }
    }

    return proc_type;
}

int apm_query_master_info(unsigned int cmd, int slave_pid, unsigned int *udevid, unsigned int *master_pid,
                          unsigned int *master_tgid, unsigned int *proc_type)
{
    struct apm_cmd_query_master_info para = {.slave_pid = (unsigned int)slave_pid};
    int ret;

    if (udevid != NULL && cmd == APM_QUERY_MASTER_INFO_BY_DEVICE_SLAVE) {
        para.udevid = *udevid;
    }

    ret = apm_cmd_ioctl(cmd, &para);
    if (ret != 0) {
        apm_warn("Query bindpid warn. (ret=%d; slave_pid=%d)\n", ret, slave_pid);
        return ret;
    }

    if (udevid != NULL) {
        *udevid = para.udevid;
    }

    if (master_pid != NULL) {
        *master_pid = (unsigned int)para.master_pid;
    }

    if (master_tgid != NULL) {
        *master_tgid = (unsigned int)para.master_tgid;
    }

    if (proc_type != NULL) {
        *proc_type = (unsigned int)apm_trans_first_proc_type_from_bitmap(para.proc_type_bitmap);
    }

    return DRV_ERROR_NONE;
}

drvError_t drvQueryProcessHostPid(int pid, unsigned int *chip_id, unsigned int *vfid, unsigned int *host_pid,
                                  unsigned int *cp_type)
{
    unsigned int master_tgid;

    if (vfid != NULL) {
        *vfid = 0;
    }

    return apm_query_master_info(APM_QUERY_MASTER_INFO, pid, chip_id, host_pid, &master_tgid, cp_type);
}

int halQueryMasterPidByDeviceSlave(unsigned int devid, int slave_pid, unsigned int *master_pid, unsigned int *proc_type)
{
    unsigned int master_tgid;
    return apm_query_master_info(APM_QUERY_MASTER_INFO_BY_DEVICE_SLAVE, slave_pid, &devid, master_pid, &master_tgid,
                                 proc_type);
}

int halQueryMasterPidByHostSlave(int slave_pid, unsigned int *udevid, unsigned int *master_pid, unsigned int *proc_type)
{
    unsigned int master_tgid;
    return apm_query_master_info(APM_QUERY_MASTER_INFO_BY_HOST_SLAVE, slave_pid, udevid, master_pid, &master_tgid,
                                 proc_type);
}

int halQueryMasterTgidByDeviceSlave(unsigned int devid, int slave_pid, unsigned int *master_tgid,
                                    unsigned int *proc_type)
{
    unsigned int master_pid;
    return apm_query_master_info(APM_QUERY_MASTER_INFO_BY_DEVICE_SLAVE, slave_pid, &devid, &master_pid, master_tgid,
                                 proc_type);
}
